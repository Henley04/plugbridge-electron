'use strict';
//
// test/editor.test.js
//
// Headless exercise of the 0.4.0 editor / GUI API surface added by
// plugbridge-electron. The bundled Gain test plugin does NOT implement
// IPlugView, so the assertions check the no-editor branches:
//   - hasEditor() returns false
//   - openEditor(0) returns false without throwing
//   - openEditor(<invalid>) throws VST3_INVALID_PARAMETER
//   - closeEditor() is a no-op when no editor is open
//   - getEditorSize() returns all zeros
//   - setEditorScale() returns false (no IPlugViewContentScaleSupport)
//   - isEditorOpen() returns false
//   - getPluginInfo() exposes hasEditor / editorOpen / editorSize fields
//   - listener registration for the 3 new events does not throw
//
// To exercise the GUI path end-to-end (openEditor against a real native
// window), run examples/electron-editor.js against a VST3 plugin that
// ships an editor.
//
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const {
  loadPlugin,
  ensurePluginBuilt,
} = require('./helpers');

describe('Editor / GUI API (headless — Gain plugin has no IPlugView)', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) {
      try {
        plugin.dispose();
      } catch (_) {
        // ignore
      }
    }
    plugin = null;
  });

  test('hasEditor() returns false for the Gain test plugin', () => {
    plugin = loadPlugin().plugin;
    assert.equal(plugin.hasEditor(), false);
  });

  test('isEditorOpen() returns false before any openEditor call', () => {
    plugin = loadPlugin().plugin;
    assert.equal(plugin.isEditorOpen(), false);
  });

  test('getEditorSize() returns all zeros when no editor is open', () => {
    plugin = loadPlugin().plugin;
    const size = plugin.getEditorSize();
    assert.equal(size.left, 0);
    assert.equal(size.top, 0);
    assert.equal(size.right, 0);
    assert.equal(size.bottom, 0);
    assert.equal(size.width, 0);
    assert.equal(size.height, 0);
  });

  test('openEditor(0) returns false without throwing (no IPlugView available)', () => {
    plugin = loadPlugin().plugin;
    // A zero handle is rejected before createView is ever called; a non-zero
    // handle reaches createView("editor") which returns nullptr for the Gain
    // plugin, so openEditor returns false either way.
    assert.equal(plugin.openEditor(0), false);
    assert.equal(plugin.isEditorOpen(), false);
    // Non-zero fake handle — still false because the plugin has no editor.
    assert.equal(plugin.openEditor(0xdeadbeef), false);
    assert.equal(plugin.isEditorOpen(), false);
  });

  test('openEditor(<invalid type>) throws VST3_INVALID_PARAMETER', () => {
    plugin = loadPlugin().plugin;
    assert.throws(
      () => plugin.openEditor('not-a-handle'),
      (err) => err.code === 'VST3_INVALID_PARAMETER',
    );
    assert.throws(
      () => plugin.openEditor(null),
      (err) => err.code === 'VST3_INVALID_PARAMETER',
    );
    assert.throws(
      () => plugin.openEditor(undefined),
      (err) => err.code === 'VST3_INVALID_PARAMETER',
    );
  });

  test('setEditorScale() returns false when there is no IPlugViewContentScaleSupport', () => {
    plugin = loadPlugin().plugin;
    assert.equal(plugin.setEditorScale(1.0), false);
    assert.equal(plugin.setEditorScale(2.0), false);
  });

  test('closeEditor() is idempotent when no editor is open', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.closeEditor());
    assert.doesNotThrow(() => plugin.closeEditor());
    assert.equal(plugin.isEditorOpen(), false);
  });

  test('closeEditor() is idempotent after a failed openEditor', () => {
    plugin = loadPlugin().plugin;
    assert.equal(plugin.openEditor(0), false);
    assert.doesNotThrow(() => plugin.closeEditor());
    assert.equal(plugin.isEditorOpen(), false);
  });

  test('getPluginInfo() exposes hasEditor / editorOpen / editorSize fields', () => {
    plugin = loadPlugin().plugin;
    const info = plugin.getPluginInfo();
    assert.equal(info.hasEditor, false);
    assert.equal(info.editorOpen, false);
    // editorSize is only present when an editor is open — must be undefined here.
    assert.equal(info.editorSize, undefined);
  });

  test('listener registration for the 3 new editor events does not throw', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.on('editorResize', () => true));
    assert.doesNotThrow(() => plugin.on('requestOpenEditor', () => {}));
    assert.doesNotThrow(() => plugin.on('contextMenu', () => {}));
  });

  test('editor methods are safe to call after dispose', () => {
    plugin = loadPlugin().plugin;
    plugin.dispose();
    // After dispose, every method should throw VST3_FAULTED or VST3_UNKNOWN
    // (the host checkAlive guard fires) — never crash.
    assert.throws(() => plugin.hasEditor(), (err) => typeof err.code === 'string');
    assert.throws(() => plugin.isEditorOpen(), (err) => typeof err.code === 'string');
    assert.throws(() => plugin.getEditorSize(), (err) => typeof err.code === 'string');
    // closeEditor remains idempotent (no-op) — it must not throw.
    assert.doesNotThrow(() => plugin.closeEditor());
    plugin = null;
  });
});
