# electron-vst3-bridge

> Production-grade VST3 audio plugin bridge for Electron — load, automate, process, and embed VST3 plugin editors with zero-copy audio buffers, full GUI/editor support, and the complete VST3 spec surface.

[![CI](https://github.com/Henley04/electron-vst3-bridge/workflows/CI/badge.svg)](https://github.com/Henley04/electron-vst3-bridge/actions)
[![npm version](https://img.shields.io/npm/v/electron-vst3-bridge.svg)](https://www.npmjs.com/package/electron-vst3-bridge)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Node.js](https://img.shields.io/badge/node-%3E%3D16.17-brightgreen)](https://nodejs.org/)
[![Electron](https://img.shields.io/badge/electron-%3E%3D20-blueviolet)](https://www.electronjs.org/)

`electron-vst3-bridge` brings the VST3 plugin ecosystem to Electron applications. Load any VST3 plugin, push audio through it with **zero-copy `Float32Array` buffers**, automate parameters, send MIDI events, save and restore plugin state, **embed the plugin's native GUI editor inside a `BrowserWindow`**, react to plugin-initiated restart notifications, and forward context-menu / open-editor requests — all from JavaScript, with no DAW and no compiler toolchain required at install time.

It is built on the **official Steinberg VST3 SDK (v3.8.0, MIT-licensed)** and ships **prebuilt native binaries** for Windows, macOS (Apple Silicon), and Linux. The C++ addon is N-API v8 (ABI-stable) and links against AppKit/Cocoa on macOS, gdi32/comctl32 on Windows, and GTK/X11 on Linux so the plugin's `IPlugView` can be re-parented into a native Electron window.

## Features

- **Designed for Electron** — every GUI method takes a native parent window handle obtained from `BrowserWindow.getNativeWindowHandle()`. The host advertises `IPlugFrame`, `IPlugView`, `IPlugViewContentScaleSupport`, and `IContextMenu` so plugins query-recognize the host as a GUI-capable environment.
- **Editor / GUI embedding** — `hasEditor()`, `openEditor(parentHandle)`, `closeEditor()`, `getEditorSize()`, `setEditorScale(factor)` (HiDPI / retina), `isEditorOpen()`. Implemented via `IPlugView::attached` + `IPlugView::setFrame` (the host implements `IPlugFrame` so the plugin can request resizes, dispatched as the `'editorResize'` event).
- **Plugin→host GUI events** — `on('editorResize', cb)` (synchronous accept/reject), `on('requestOpenEditor', cb)` (forwarded from `IComponentHandler3::requestOpenEditor`), `on('contextMenu', cb)` (forwarded from `IComponentHandler3::createContextMenu`).
- **Discovery** — scan platform-default VST3 locations, recursively scan an arbitrary directory, or inspect a single `.vst3` module without instantiating the DSP.
- **Lifecycle** — `load()`, `setActive(bool)`, `setProcessing(bool)`, `dispose()` (idempotent), and `[Symbol.dispose]` enabling the `using` keyword for scope-based cleanup.
- **Audio processing** — zero-copy `Float32Array` / `Float64Array` channel buffers; configurable sample size (32 or 64 bit); configurable process mode (`realtime` / `offline` / `prefetch`); parameter-flush blocks via `process({ numSamples: 0 })`; silence-flag propagation on input and output buses; tail-samples query (returns `Number.POSITIVE_INFINITY` for `kInfiniteTail`); latency query.
- **Parameters** — `getParameter` / `setParameter` / `setParameters` (atomic batch), `getParameterInfo`, `formatParameter`, `parseParameter` (string → normalized), `plainToNormalized` / `normalizedToPlain`; `ParameterFlags.IsProgramChange` honored by plugins that expose program-change parameters.
- **MIDI / events** — structured `MidiEvent`s (NoteOn / NoteOff / PolyPressure / Controller / ProgramChange / ChannelPressure / PitchBend / SysEx) plus raw `addMidiBytes`; optional `noteId` on NoteOn/NoteOff/PolyPressure for note-expression targeting; output event retrieval via `takeOutputEvents()`.
- **State persistence** — `saveState()` writes a versioned `NST3` envelope (4-byte magic, 1-byte version, length-prefixed component + controller blobs); `loadState()` auto-detects the envelope and falls back to legacy single-blob loading for backward compatibility with 0.1.0 state files. `BufferStream` implements `IStreamAttributes` so plugins can read the `.vstpreset` file path and state-type attribute during `setState`.
- **Units & programs** — `IUnitInfo` enumeration (`getUnitCount`, `getUnitInfo`, `getProgramListCount`, `getProgramListInfo`, `getProgramName`, `selectProgram`, `getCurrentUnit`, `getUnitByBusInfo`); per-program and per-unit bulk data via `IProgramListData` / `IUnitData` (`getProgramData`, `setProgramData`, `getUnitData`, `setUnitData`).
- **Note expression** — `INoteExpressionController` enumeration (`getNoteExpressionCount`, `getNoteExpressionInfo`) and `addNoteExpressionEvent({ noteId, typeId, value, sampleOffset? })` queuing.
- **Keyswitches** — `IKeyswitchController` enumeration (`getKeyswitchCount`, `getKeyswitchInfo`).
- **Bus management** — runtime `getBusList`, `getBusInfo`, `activateBus` (toggle individual buses while inactive); speaker-arrangement negotiation via `setBusArrangement` / `getBusArrangement` with the `SpeakerArrangement` enum; routing info via `getRoutingInfo`.
- **Process context** — configurable tempo / time signature / transport (`playing`, `cycleActive`, `recording`) / `systemTime` / `continuousTimeSamples` via `setProcessContext` / `getProcessContext`; `IProcessContextRequirements` gating so the host skips recomputation of unneeded fields each block.
- **Information interfaces** — `IAudioPresentationLatency` (`setAudioPresentationLatency`); `IInfoListener` (`setChannelContextInfo`); `IPrefetchableSupport` (`isPrefetchable`); `IEditController2` (`setKnobMode`).
- **Restart auto-react** — when the plugin calls `restartComponent`, the host automatically re-queries the affected SDK state (latency, bus info, etc.) BEFORE the JS `restart` event fires; `applyRestartFlags(flags)` is exposed for manual re-query.
- **Mutable ProcessSetup** — `setProcessSetup({ sampleRate?, maxBlockSize?, processMode?, sampleSize? })` re-negotiates the setup for the next `setActive(true)` (requires `setActive(false)` first).
- **Complete PlugInterfaceSupport** — the host advertises the 17 interfaces it implements via a custom `IPlugInterfaceSupport`, including the GUI/editor interfaces (`IPlugFrame`, `IPlugView`, `IPlugViewContentScaleSupport`, `IContextMenu`).
- **Plugin→host events** — `on('restart')`, `on('dirty')`, `on('beginGesture')`, `on('endGesture')`, `on('startGroup')`, `on('finishGroup')`, `on('editorResize')`, `on('requestOpenEditor')`, `on('contextMenu')`, all delivered safely across threads via a `Napi::ThreadSafeFunction` (the resize callback is synchronous because it must return a boolean accept/reject).
- **Error handling** — typed `EvstError` with `code`, `cause`, `runtimeTriple`, and `supportedTriples` fields; 14 stable error codes covering load, activation, processing, state, MIDI, platform, and unexpected-fault conditions.
- **Cross-platform prebuilt binaries** — `npm install electron-vst3-bridge` ships native `.node` files via `prebuildify` for `win32-x64`, `darwin-arm64`, `linux-x64`, and `linux-arm64`. No toolchain needed for end users.
- **MIT-licensed end-to-end** — both `electron-vst3-bridge` and the bundled VST3 SDK are MIT-licensed (since SDK v3.7.7), so there are no licensing concerns for commercial or closed-source use.
- **Strong TypeScript types** — a hand-written `index.d.ts` mirrors the native surface 1:1, including all 14 enums, the editor / GUI method group, and full JSDoc, for editor IntelliSense.

## Installation

```bash
npm install electron-vst3-bridge
```

No compiler toolchain required — prebuilt binaries are shipped for:

| Platform            | Triple          | Build Method                          |
|---------------------|-----------------|---------------------------------------|
| Windows x64         | `win32-x64`     | GitHub Actions (`windows-latest`)     |
| macOS Apple Silicon | `darwin-arm64`  | GitHub Actions (`macos-14`)           |
| Linux x64           | `linux-x64`     | GitHub Actions (`ubuntu-latest`)      |
| Linux ARM64         | `linux-arm64`   | GitHub Actions (`ubuntu-24.04-arm`)   |

> **Note on Intel Macs and Windows x86**: GitHub Actions no longer provides
> `darwin-x64` (Intel macOS) runners as of late 2025 — those runners were
> deprecated and removed. `win32-ia32` (32-bit Windows) was never supported
> in CI and modern VST3 plugins are universally 64-bit. Users on Intel
> Macs can build from source via `npm install` (which falls back to
> `node-gyp rebuild`) — see [Building from Source](#building-from-source).

If no prebuilt binary matches your runtime, `node-gyp-build` automatically falls back to a source build (`node-gyp rebuild`) — this requires Node.js headers and a C++17 compiler.

### Requirements

- Node.js `>= 16.17` (or Electron `>= 20` — the addon is N-API v8 ABI-stable)
- One of the supported platform triples above
- (For source builds only) a C++17 compiler and Python 3 — see [Building from Source](#building-from-source)

## Quick Start

### Headless audio processing (Node.js or Electron main process)

```js
const { Host } = require('electron-vst3-bridge');

// 1. Create a host with the audio format you want to process at.
const host = new Host({
  sampleRate: 48000,
  maxBlockSize: 512,
  audioInputs: 2,   // stereo in
  audioOutputs: 2,  // stereo out
});

// 2. Load a VST3 plugin (path to the .vst3 bundle/module).
const plugin = host.load('/path/to/SomePlugin.vst3');
console.log(plugin.getInfo());

// 3. Activate and start processing.
plugin.setActive(true);
plugin.setProcessing(true);

// 4. Tweak a parameter by ID (normalized 0..1).
const paramInfo = plugin.getParameterInfo(0);
plugin.setParameter(paramInfo.id, 0.75);

// 5. Process a block of audio — buffers are passed zero-copy.
const numSamples = 512;
const inputs = [
  new Float32Array(numSamples),  // left  (silence in)
  new Float32Array(numSamples),  // right
];
const outputs = [
  new Float32Array(numSamples),  // left  (filled by plugin)
  new Float32Array(numSamples),  // right
];
plugin.process({ inputs, outputs, numSamples });

// 6. Dispose when done (idempotent — safe to call twice).
plugin.dispose();
```

### Embedding the plugin GUI inside an Electron BrowserWindow

```js
// main.js (Electron main process)
const { app, BrowserWindow } = require('electron');
const { Host } = require('electron-vst3-bridge');

let win;
let plugin;

app.whenReady().then(async () => {
  win = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: { nodeIntegration: true, contextIsolation: false },
  });

  const host = new Host({ sampleRate: 48000, maxBlockSize: 512, audioInputs: 2, audioOutputs: 2 });
  plugin = host.load('/path/to/PluginWithGUI.vst3');
  plugin.setActive(true);
  plugin.setProcessing(true);

  if (plugin.hasEditor()) {
    // Electron exposes the NSView* (macOS) / HWND (Windows) / X11 Window id (Linux)
    // as a Buffer of raw pointer bytes via getNativeWindowHandle().
    const parentHandle = win.getNativeWindowHandle();

    // Resize listener — MUST return true to accept, false to reject.
    plugin.on('editorResize', (rect) => {
      // Adjust the BrowserWindow / container to the plugin's requested size.
      win.setContentSize(rect.width, rect.height);
      return true;  // host accepts the new size; the addon calls IPlugView::onSize.
    });

    // The plugin asked the host to open its editor (e.g. on double-click).
    plugin.on('requestOpenEditor', (editorName) => {
      // editorName: 0 = default "editor", 1 = any other name.
      if (!plugin.isEditorOpen()) plugin.openEditor(parentHandle);
    });

    // The plugin asked the host to display a context menu.
    plugin.on('contextMenu', (paramId) => {
      // paramId: -1 for a generic menu, otherwise the parameter the menu was opened for.
      // Build and show a native Electron Menu here.
    });

    // Attach the plugin's IPlugView to the BrowserWindow's native handle.
    plugin.openEditor(parentHandle);

    // Push the current display scale (e.g. 2.0 on a retina screen).
    plugin.setEditorScale(win.webContents.getDevicePixelRatio());

    // Read the preferred editor size after attach.
    const size = plugin.getEditorSize();
    win.setContentSize(size.width, size.height);
  }
});

app.on('window-all-closed', () => {
  if (plugin) {
    plugin.closeEditor();   // detach IPlugView before disposing
    plugin.dispose();
  }
  app.quit();
});
```

### Scoped cleanup with `using` (Node.js ≥ 20)

If you are on Node.js ≥ 20 with explicit resource management enabled, you can use the `using` keyword for automatic cleanup:

```js
{
  using plugin = host.load('/path/to/SomePlugin.vst3');
  // ... use plugin ...
  // plugin[Symbol.dispose]() is called automatically at end of scope.
}
```

## API

The full surface is documented in [`docs/API.md`](docs/API.md). The two main classes are:

### `Host`

```js
const { Host } = require('electron-vst3-bridge');
```

- `new Host(opts?)` — construct a host with `sampleRate`, `maxBlockSize`, `audioInputs`, `audioOutputs` (all optional, with sensible defaults).
- `host.load(path, opts?)` → `PluginInstance` — load and instantiate a VST3 plugin.
- `host.getOptions()` → `Required<HostOptions>` — snapshot of the host's audio format.
- `Host.scanDefaultLocations()` → `PluginInfo[]` — scan platform-default VST3 directories.
- `Host.scanDirectory(path)` → `PluginInfo[]` — recursively scan an arbitrary directory.
- `Host.inspectPlugin(path)` → `PluginInfo | PluginInfo[]` — read metadata from a single `.vst3` without instantiating it.

### `PluginInstance`

Obtained from `host.load(...)`. Never call `new PluginInstance(...)` directly.

- **Lifecycle**: `dispose()`, `[Symbol.dispose]()`, `on('restart', cb)`
- **Metadata**: `getInfo()`, `getPluginInfo()` (debug snapshot with full state + buses + interfaces + editor state), `getParameterTree()` (all parameters grouped by unit with current values), `getLatency()`
- **Processing**: `setActive(bool)`, `setProcessing(bool)`, `process({ inputs, outputs, numSamples })`
- **Parameters**: `getParameterCount()`, `getParameterInfo(index)`, `getParameter(id)`, `setParameter(id, value)`, `setParameters(changes)`, `formatParameter(id, value)`
- **MIDI**: `addMidiEvent(event)`, `addMidiBytes(sampleOffset, bytes)`, `takeOutputEvents()`, `clearEvents()`
- **State**: `saveState()` → `Buffer`, `loadState(buffer)`
- **Editor / GUI**: `hasEditor()`, `openEditor(parentHandle)`, `closeEditor()`, `getEditorSize()`, `setEditorScale(factor)`, `isEditorOpen()`
- **Events**: `on('restart' | 'dirty' | 'beginGesture' | 'endGesture' | 'startGroup' | 'finishGroup' | 'editorResize' | 'requestOpenEditor' | 'contextMenu', cb)`

## Editor / GUI lifecycle

The editor methods implement the host side of the VST3 `IPlugView` / `IPlugFrame` contract. They are designed to be driven from an Electron renderer process via `BrowserWindow.getNativeWindowHandle()`.

```
┌─────────────────────────────────────────────────────────────────────┐
│ Electron BrowserWindow                                              │
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Plugin's IPlugView (re-parented native HWND/NSView/X11 Win) │  │
│  │                                                               │  │
│  │  plugin.openEditor(parentHandle)                              │  │
│  │    └─► IEditController::createView("editor")                  │  │
│  │    └─► IPlugView::isPlatformTypeSupported (HWND/NSView/X11)   │  │
│  │    └─► IPlugView::setFrame(this EditorView as IPlugFrame)     │  │
│  │    └─► IPlugView::attached(parentHandle, platformType)        │  │
│  │                                                               │  │
│  │  plugin.setEditorScale(2.0)                                   │  │
│  │    └─► IPlugViewContentScaleSupport::setContentScaleFactor    │  │
│  │                                                               │  │
│  │  plugin.on('editorResize', rect => true|false)                │  │
│  │    └─► IPlugFrame::resizeView(view, newRect) [from plugin]    │  │
│  │    └─► if accepted: IPlugView::onSize(newRect)                │  │
│  │                                                               │  │
│  │  plugin.closeEditor()                                         │  │
│  │    └─► IPlugView::removed()                                   │  │
│  │    └─► release IPlugView                                      │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

The parent handle is interpreted per-platform:

| Platform | Handle type  | Electron source                            |
|----------|--------------|--------------------------------------------|
| Windows  | `HWND`       | `BrowserWindow.getNativeWindowHandle()`    |
| macOS    | `NSView*`    | `BrowserWindow.getNativeWindowHandle()`    |
| Linux    | X11 Window id | `BrowserWindow.getNativeWindowHandle()`   |

The addon accepts the handle as a `Buffer` (what Electron returns), a `bigint` (e.g. `getNativeWindowHandle().readBigInt64LE()`), a `number`, or a `Uint8Array`/`ArrayBuffer`.

## MIDI Events

`addMidiEvent` accepts a discriminated union tagged by `type`. Use the `MidiEventType` enum to construct events:

```js
const { Host, MidiEventType } = require('electron-vst3-bridge');

const host = new Host({ sampleRate: 48000, maxBlockSize: 512 });
const synth = host.load('/path/to/Synth.vst3');

synth.setActive(true);
synth.setProcessing(true);

// Schedule a C4 note on at sample offset 0, note off at offset 256.
synth.addMidiEvent({
  type: MidiEventType.NoteOn,
  channel: 0,
  note: 60,        // C4
  velocity: 0.8,
  sampleOffset: 0,
});
synth.addMidiEvent({
  type: MidiEventType.NoteOff,
  channel: 0,
  note: 60,
  velocity: 0.0,
  sampleOffset: 256,
});

const outputs = [new Float32Array(512), new Float32Array(512)];
synth.process({ inputs: [], outputs, numSamples: 512 });

// Capture any output events (note-offs from the synth, SysEx, etc.).
const outEvents = synth.takeOutputEvents();
```

You can also push raw MIDI bytes with `addMidiBytes`:

```js
// Status 0x90 (note on, channel 0), note 60, velocity 100
synth.addMidiBytes(0, Uint8Array.from([0x90, 60, 100]));
```

## Error Handling

All errors thrown by `electron-vst3-bridge` carry a `code` property with one of the `VST3_*` error codes documented in [`docs/API.md`](docs/API.md#error-codes).

```js
const { Host } = require('electron-vst3-bridge');
const host = new Host();

try {
  const plugin = host.load('/nonexistent/path.vst3');
} catch (err) {
  if (err.code === 'VST3_LOAD_FAILED') {
    console.error('Plugin could not be loaded:', err.message);
  } else if (err.code === 'VST3_FACTORY_MISSING') {
    console.error('Module loaded but no VST3 factory was found.');
  } else {
    throw err;  // rethrow unknown errors
  }
}
```

After any `process()` failure, the plugin enters a **faulted** state and all subsequent method calls (other than `dispose()`) reject with `VST3_FAULTED` until you `dispose()` the instance and create a new one via `host.load()`.

## State Save / Load

```js
const fs = require('fs');
const { Host } = require('electron-vst3-bridge');

const host = new Host();
const plugin = host.load('/path/to/SomePlugin.vst3');

plugin.setActive(true);

// Mutate some parameters.
plugin.setParameter(0, 0.42);
plugin.setParameter(1, 0.97);

// Serialize current state to a Buffer.
const state = plugin.saveState();
fs.writeFileSync('plugin-state.bin', state);

// ... later, restore it ...
plugin.loadState(state);
console.log(plugin.getParameter(0));  // 0.42
```

State round-trips through `IComponent::getState` / `setState` and (if present) `IEditController::setComponentState`, so all parameter values are preserved. The `BufferStream` passed to the plugin implements `IStreamAttributes`, so plugins that query `getFileName` / `getAttributes` during `setState` (e.g. to read the `.vstpreset` file path or the `StateType` attribute) receive populated values.

## Examples

The `examples/` directory contains runnable sample scripts:

- [`examples/scan-plugins.js`](examples/scan-plugins.js) — list installed VST3 plugins with metadata.
- [`examples/process-file.js`](examples/process-file.js) — read a WAV, process it through a plugin, write a WAV.
- [`examples/midi-synth.js`](examples/midi-synth.js) — schedule MIDI notes through an instrument, write output WAV.
- [`examples/parameter-sweep.js`](examples/parameter-sweep.js) — automate a parameter across blocks, write output WAV.
- [`examples/electron-editor.js`](examples/electron-editor.js) — embed a VST3 plugin's GUI editor inside an Electron `BrowserWindow`.

## Building from Source

End users should never need this — prebuilt binaries cover all supported platforms. For contributors and unsupported platforms:

```bash
# 1. Clone with the VST3 SDK submodule.
git clone --recursive https://github.com/Henley04/electron-vst3-bridge.git
cd electron-vst3-bridge

# 2. If you cloned without --recursive:
git submodule update --init --recursive

# 3. Install dev dependencies.
npm ci

# 4. Build the native addon (uses node-gyp under the hood).
npm run build

# 5. Verify it loads.
node -e "console.log(require('./').version())"
# { native: '0.4.0', vst3sdk: 'VST 3.8.0', napi: 8 }
```

Prerequisites for source builds:

- **Node.js 20+** (for development; the runtime supports 16.17+)
- **Electron ≥ 20** (for GUI/editor embedding — main-process native modules)
- **Python 3** (required by `node-gyp`)
- **C++17 compiler**:
  - macOS: Xcode Command Line Tools (`xcode-select --install`)
  - Linux: `g++` ≥ 11 or `clang++` ≥ 13, plus `libasound2-dev`, `libgtk-3-dev`, and `libstdc++-12-dev` (or equivalent)
  - Windows: Visual Studio 2022 with the "Desktop development with C++" workload
- **CMake ≥ 3.22** (only required to build the test plugin; see [CONTRIBUTING.md](CONTRIBUTING.md))

To create prebuilds for the current platform:

```bash
npm run prebuild
```

This invokes `prebuildify --napi-version 8 --tag-armv -t 20.0.0` and writes `.node` files into `prebuilds/<triple>/`. See [CONTRIBUTING.md](CONTRIBUTING.md) for the full CI-driven cross-platform build flow.

## Performance

`electron-vst3-bridge` is designed for real-time, block-based audio processing:

- **Zero-copy buffers** — `Float32Array` channel data is handed directly to the plugin via `AudioBusBuffers` channel pointers. There is no copy on input or output.
- **No allocations on the hot path** — `ProcessData`, `AudioBusBuffers`, `ParameterChangesContainer`, and `EventListContainer` are reused across `process()` calls; `setParameter` queues changes into pre-allocated queues. Steady-state `process()` does not allocate.
- **No locks during `IAudioProcessor::process`** — restart notifications are posted through a `Napi::ThreadSafeFunction` driven by an atomic flag, so the audio thread never blocks on JavaScript-side state.
- **Block-based processing** — `numSamples` per call is bounded by `maxBlockSize` (default 512). Process the file in chunks and you will get DAW-class throughput on commodity hardware.

### Limitations

- **64-bit audio is opt-in** — `kSample32` is the default; 64-bit double precision (`kSample64`) is opt-in via `sampleSize: 64` in `HostOptions`/`LoadOptions`. The host silently falls back to 32 if the plugin refuses 64.
- **Single-process context** — each `PluginInstance` owns its own component/controller pair; there is no built-in signal graph or routing layer. Compose plugins in JavaScript by chaining `process()` calls.
- **Process mode is configurable** — `realtime` is the default; `offline` and `prefetch` modes are opt-in via `processMode` in `HostOptions`/`LoadOptions`.
- **macOS target** — binaries are built with `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- **Linux target** — prebuilt binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).
- **Electron main-process only** — like all Node native addons, evst3 must run in the Electron main process (or a Node-style worker); it cannot be loaded directly from a renderer process with `contextIsolation: true`. Use IPC to bridge renderer → main for editor lifecycle calls.

## License

MIT — see [`LICENSE`](LICENSE).

The bundled VST3 SDK in `third_party/vst3sdk/` is also MIT-licensed (since SDK v3.7.7), so there are no licensing concerns for commercial or closed-source use. See `third_party/vst3sdk/LICENSE.txt` for the SDK's license.
