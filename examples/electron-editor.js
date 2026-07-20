'use strict';
//
// examples/electron-editor.js
//
// Embeds a VST3 plugin's GUI editor inside an Electron BrowserWindow.
// Demonstrates the 0.4.0 editor / GUI surface added by
// plugbridge-electron:
//
//   1. BrowserWindow.getNativeWindowHandle() → Buffer of raw pointer bytes
//   2. plugin.hasEditor()                     → probe IEditController::createView
//   3. plugin.openEditor(parentHandle)        → IPlugView::attached + setFrame
//   4. plugin.on('editorResize', cb)          → IPlugFrame::resizeView dispatch
//   5. plugin.setEditorScale(dpr)             → IPlugViewContentScaleSupport
//   6. plugin.on('requestOpenEditor', cb)     → IComponentHandler3 forwarding
//   7. plugin.on('contextMenu', cb)           → IComponentHandler3 forwarding
//   8. plugin.closeEditor()                   → IPlugView::removed + release
//
// Requirements:
//   - Electron installed: `npm install --save-dev electron`
//   - A VST3 plugin that ships an IPlugView (the bundled Gain test plugin
//     has none — point this at a real instrument or effect).
//
// Usage:
//   npx electron examples/electron-editor.js [plugin.vst3]
//
// Examples:
//   npx electron examples/electron-editor.js /path/to/Synth.vst3
//   npx electron examples/electron-editor.js   # uses Gain.vst3 if built (no editor)
//

const { app, BrowserWindow, Menu, ipcMain } = require('electron');
const path = require('path');
const { Host, version } = require('../');

const PLUGIN_PATH = process.argv[2] ||
    path.join(__dirname, '..', 'test', 'plugin', 'build', 'Gain.vst3');

let win = null;
let host = null;
let plugin = null;

function log(msg) {
    console.log(`[evst3] ${msg}`);
}

app.whenReady().then(() => {
    const v = version();
    log(`plugbridge-electron ${v.native} (VST3 SDK ${v.vst3sdk}, N-API v${v.napi})`);
    log(`Loading plugin: ${PLUGIN_PATH}`);

    host = new Host({
        sampleRate: 48000,
        maxBlockSize: 512,
        audioInputs: 2,
        audioOutputs: 2,
    });

    plugin = host.load(PLUGIN_PATH);
    log(`Loaded: ${plugin.getInfo().name} (${plugin.getInfo().vendor})`);

    plugin.setActive(true);
    plugin.setProcessing(true);

    win = new BrowserWindow({
        width: 900,
        height: 700,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false,
        },
    });

    win.loadURL('data:text/html,' + encodeURIComponent(`<!doctype html>
<html><head><meta charset="utf-8"><title>evst3 editor</title>
<style>
  body { margin: 0; background: #1e1e1e; color: #ddd; font-family: sans-serif; }
  #bar { padding: 8px 12px; background: #2d2d2d; border-bottom: 1px solid #444; }
  #bar button { background: #0e639c; color: #fff; border: none; padding: 6px 12px;
                border-radius: 3px; cursor: pointer; font-size: 12px; }
  #bar button:disabled { background: #555; cursor: default; }
  #status { margin-left: 12px; font-size: 12px; color: #aaa; }
  #editor-host { position: relative; }
</style></head>
<body>
  <div id="bar">
    <button id="open">Open editor</button>
    <button id="close">Close editor</button>
    <span id="status">No editor open</span>
  </div>
  <div id="editor-host"></div>
  <script>
    const { ipcRenderer } = require('electron');
    document.getElementById('open').onclick = () => ipcRenderer.send('open');
    document.getElementById('close').onclick = () => ipcRenderer.send('close');
    ipcRenderer.on('status', (e, msg) => { document.getElementById('status').textContent = msg; });
    ipcRenderer.on('editor-enabled', (e, enabled) => {
      document.getElementById('open').disabled = !enabled;
    });
  </script>
</body></html>`));

    // --- Editor lifecycle --------------------------------------------------
    const hasEditor = plugin.hasEditor();
    log(`hasEditor() = ${hasEditor}`);

    // The plugin asked the host to display a context menu (right-click on a
    // parameter, etc.). Build a native Electron Menu here.
    plugin.on('contextMenu', (paramId) => {
        log(`contextMenu event: paramId=${paramId}`);
        const menu = Menu.buildFromTemplate([
            { label: paramId === -1 ? 'Generic menu' : `Parameter ${paramId}`, enabled: false },
            { type: 'separator' },
            { label: 'Reset to default', click: () => log('reset clicked') },
            { label: 'Copy value', click: () => log('copy clicked') },
        ]);
        menu.popup(win);
    });

    // The plugin asked the host to open its editor (e.g. on double-click).
    plugin.on('requestOpenEditor', (editorName) => {
        log(`requestOpenEditor event: editorName=${editorName === 0 ? '"editor"' : 'other'}`);
        if (!plugin.isEditorOpen()) openEditor();
    });

    ipcMain.on('open', openEditor);
    ipcMain.on('close', () => closeEditor());

    function openEditor() {
        if (!hasEditor) {
            log('This plugin has no IPlugView — cannot open editor.');
            win.webContents.send('status', 'Plugin has no editor');
            return;
        }
        if (plugin.isEditorOpen()) {
            log('Editor already open.');
            return;
        }
        // BrowserWindow.getNativeWindowHandle() returns a Buffer of raw
        // pointer bytes (NSView* on macOS, HWND on Windows, X11 Window id
        // on Linux). The addon accepts Buffer / bigint / number / TypedArray.
        const parentHandle = win.getNativeWindowHandle();
        log(`openEditor(parentHandle=${parentHandle.toString('hex')})`);

        // Resize listener — MUST return true to accept, false to reject.
        plugin.on('editorResize', (rect) => {
            log(`editorResize event: ${rect.width}x${rect.height}`);
            // Adjust the BrowserWindow content size to the plugin's preferred size.
            // Keep a 32px margin for the toolbar.
            win.setContentSize(rect.width, rect.height + 32);
            return true;  // accept — the addon calls IPlugView::onSize.
        });

        const ok = plugin.openEditor(parentHandle);
        if (!ok) {
            log('openEditor returned false — plugin refused or platform unsupported.');
            win.webContents.send('status', 'Editor open failed');
            return;
        }
        const size = plugin.getEditorSize();
        log(`Editor size: ${size.width}x${size.height}`);
        win.setContentSize(size.width, size.height + 32);

        // Push the current display scale (e.g. 2.0 on a retina screen).
        const dpr = win.webContents.getDevicePixelRatio();
        log(`setEditorScale(${dpr}) = ${plugin.setEditorScale(dpr)}`);

        win.webContents.send('status', `Editor open (${size.width}x${size.height})`);
    }

    function closeEditor() {
        if (!plugin.isEditorOpen()) {
            log('No editor open.');
            return;
        }
        plugin.closeEditor();
        log('Editor closed.');
        win.webContents.send('status', 'No editor open');
    }

    win.on('closed', () => {
        closeEditor();
        win = null;
    });

    win.webContents.once('did-finish-load', () => {
        win.webContents.send('editor-enabled', hasEditor);
        win.webContents.send('status', hasEditor ? 'Ready' : 'Plugin has no editor');
    });
});

app.on('window-all-closed', () => {
    if (plugin) {
        try {
            if (plugin.isEditorOpen()) plugin.closeEditor();
            plugin.dispose();
        } catch (err) {
            log(`Error during teardown: ${err.message}`);
        }
        plugin = null;
    }
    if (host) {
        try { host.dispose && host.dispose(); } catch (_) {}
        host = null;
    }
    app.quit();
});

app.on('before-quit', () => {
    if (plugin) {
        try {
            if (plugin.isEditorOpen()) plugin.closeEditor();
            plugin.dispose();
        } catch (_) {}
        plugin = null;
    }
});
