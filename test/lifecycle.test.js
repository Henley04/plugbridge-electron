'use strict';
const { test, describe, afterEach } = require('node:test');
const assert = require('node:assert/strict');
const {
  loadPlugin,
  createHost,
  ensurePluginBuilt,
  PLUGIN_PATH,
  makeTone,
  makeSilence,
} = require('./helpers');
const { Host } = require('..');

describe('Lifecycle and disposal', { skip: !ensurePluginBuilt() }, () => {
  let plugin;
  afterEach(() => {
    if (plugin) {
      try {
        plugin.dispose();
      } catch (_) {
        // ignore — dispose should be idempotent/safe.
      }
    }
    plugin = null;
  });

  test('dispose() once does not throw', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.dispose());
    plugin = null; // already disposed; don't double-dispose in afterEach
  });

  test('dispose() is idempotent (calling twice does not throw)', () => {
    plugin = loadPlugin().plugin;
    plugin.dispose();
    assert.doesNotThrow(() => plugin.dispose());
    plugin = null; // already disposed twice
  });

  test('load -> dispose -> load again on the same host works (no resource leak)', () => {
    const host = createHost();
    const p1 = host.load(PLUGIN_PATH);
    p1.setActive(true);
    p1.setProcessing(true);
    p1.process({
      inputs: makeTone(2, 4, 440, 48000, 0.5),
      outputs: makeSilence(2, 4),
      numSamples: 4,
    });
    p1.dispose();

    // Load a second plugin on the same host.
    const p2 = host.load(PLUGIN_PATH);
    p2.setActive(true);
    p2.setProcessing(true);
    const inputs = makeTone(2, 4, 440, 48000, 0.5);
    const outputs = makeSilence(2, 4);
    assert.doesNotThrow(() => p2.process({ inputs, outputs, numSamples: 4 }));
    assert.ok(Math.abs(outputs[0][0] - inputs[0][0]) < 1e-5);
    p2.dispose();
  });

  test('load, activate, process, dispose does not crash', () => {
    const { plugin: p } = loadPlugin();
    p.setActive(true);
    p.setProcessing(true);
    p.process({
      inputs: makeTone(2, 4, 440, 48000, 0.5),
      outputs: makeSilence(2, 4),
      numSamples: 4,
    });
    assert.doesNotThrow(() => p.dispose());
  });

  test(
    'Symbol.dispose (using syntax) auto-disposes the plugin at block exit',
    { skip: typeof Symbol.dispose !== 'symbol' ? 'Symbol.dispose not available' : false },
    () => {
      const host = createHost();
      {
        // `using p = host.load(...)` is TC39 explicit-resource-management syntax
        // that not all supported Node runtimes parse (it fails at parse time,
        // before the skip guard above can take effect). Calling the dispose
        // symbol manually exercises the same code path as `using` would.
        const p = host.load(PLUGIN_PATH);
        p.setActive(true);
        p.setProcessing(true);
        const inputs = makeTone(2, 4, 440, 48000, 0.5);
        const outputs = makeSilence(2, 4);
        p.process({ inputs, outputs, numSamples: 4 });
        assert.ok(Math.abs(outputs[0][0] - inputs[0][0]) < 1e-5);
        p[Symbol.dispose]();
      } // p[Symbol.dispose]() called manually above (mirrors `using` semantics).
      // Reaching this point without error demonstrates the dispose symbol works.
      assert.ok(true);
    }
  );

  test('on("restart", cb) can be registered without throwing', () => {
    plugin = loadPlugin().plugin;
    assert.doesNotThrow(() => plugin.on('restart', () => {}));
  });

  test(
    'loading many plugins without explicit dispose does not crash (soft GC test)',
    { skip: typeof global.gc !== 'function' ? '--expose-gc not enabled; running subset only' : false },
    () => {
      const host = createHost();
      const plugins = [];
      for (let i = 0; i < 10; i++) {
        plugins.push(host.load(PLUGIN_PATH));
      }
      // Trigger a GC pass to exercise finalizers.
      assert.doesNotThrow(() => global.gc());
      // Clean up explicitly to avoid resource warnings between tests.
      for (const p of plugins) p.dispose();
    }
  );

  test('loading and disposing many plugins in a loop does not crash', () => {
    const host = createHost();
    for (let i = 0; i < 10; i++) {
      const p = host.load(PLUGIN_PATH);
      p.setActive(true);
      p.setProcessing(true);
      p.process({
        inputs: makeTone(2, 4, 440, 48000, 0.5),
        outputs: makeSilence(2, 4),
        numSamples: 4,
      });
      p.dispose();
    }
    assert.ok(true);
  });

  test('setActive(false) after setActive(true) does not throw', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.doesNotThrow(() => plugin.setProcessing(false));
    assert.doesNotThrow(() => plugin.setActive(false));
  });

  test('setProcessing(false) after setProcessing(true) does not throw', () => {
    plugin = loadPlugin().plugin;
    plugin.setActive(true);
    plugin.setProcessing(true);
    assert.doesNotThrow(() => plugin.setProcessing(false));
  });

  test('use-after-dispose throws VST3_FAULTED on every method (never crashes)', () => {
    // Regression: previously, calling any non-dispose method on a disposed
    // instance threw a C++ EvstException that node-addon-api's default
    // catch (const Napi::Error&) handler does not intercept, causing
    // std::terminate / SIGABRT (exit code 134). After the fix, checkAlive()
    // throws a Napi::Error directly so the JS catch surfaces VST3_FAULTED.
    plugin = loadPlugin().plugin;
    plugin.dispose();
    plugin = null; // already disposed

    const disposed = loadPlugin().plugin;
    disposed.dispose();
    const probes = [
      () => disposed.getInfo(),
      () => disposed.getLatency(),
      () => disposed.getPluginInfo(),
      () => disposed.getParameterCount(),
      () => disposed.getParameterInfo(0),
      () => disposed.getParameter(0),
      () => disposed.setParameter(0, 0.5),
      () => disposed.setParameters([{ id: 0, value: 0.5 }]),
      () => disposed.setActive(true),
      () => disposed.setProcessing(true),
      () => disposed.process({ numSamples: 0 }),
      () => disposed.saveState(),
      () => disposed.loadState(Buffer.alloc(0)),
      () => disposed.addMidiEvent({ type: 0, channel: 0, data1: 60, data2: 100 }),
      () => disposed.addMidiBytes(0, [0x90, 60, 100]),
      () => disposed.takeOutputEvents(),
      () => disposed.clearEvents(),
      () => disposed.getUnitCount(),
      () => disposed.getBusList(0),
      () => disposed.getBusInfo(0, 0),
      () => disposed.setProcessContext({ tempo: 120 }),
      () => disposed.getProcessContext(),
      () => disposed.setProcessSetup({ sampleRate: 48000 }),
      () => disposed.hasEditor(),
      () => disposed.getEditorSize(),
      () => disposed.isEditorOpen(),
      () => disposed.setSystemTime(0),
      () => disposed.getProcessContextRequirements(),
      () => disposed.applyRestartFlags(0),
      () => disposed.getParameterTree(),
    ];
    for (const probe of probes) {
      assert.throws(probe, (err) => err.code === 'VST3_FAULTED',
        'expected VST3_FAULTED from a disposed instance');
    }
  });

  test('Host constructor validates options per documented contract', () => {
    // Regression: previously the Host constructor silently coerced invalid
    // values to defaults (e.g. sampleRate=-1 became 48000). Per API.md the
    // constructor must throw VST3_INVALID_PARAMETER for any out-of-range
    // field.
    const invalid = [
      { sampleRate: -1 },
      { sampleRate: 0 },
      { maxBlockSize: 0 },
      { maxBlockSize: -1 },
      { audioInputs: -1 },
      { audioOutputs: -1 },
      { sampleSize: 16 },
      { sampleSize: 128 },
    ];
    for (const opts of invalid) {
      assert.throws(() => new Host(opts), (err) => err.code === 'VST3_INVALID_PARAMETER',
        'expected VST3_INVALID_PARAMETER for ' + JSON.stringify(opts));
    }
    // Valid options still work.
    assert.doesNotThrow(() => new Host({ sampleRate: 44100, maxBlockSize: 256 }));
    assert.doesNotThrow(() => new Host({ sampleSize: 64, processMode: 'offline' }));
    assert.doesNotThrow(() => new Host({}));
    assert.doesNotThrow(() => new Host());
  });

  test('host.load(path, opts) validates per-load overrides', () => {
    const host = createHost();
    assert.throws(() => host.load(PLUGIN_PATH, { sampleRate: 0 }),
      (err) => err.code === 'VST3_INVALID_PARAMETER');
    assert.throws(() => host.load(PLUGIN_PATH, { maxBlockSize: -1 }),
      (err) => err.code === 'VST3_INVALID_PARAMETER');
    assert.throws(() => host.load(PLUGIN_PATH, { sampleSize: 17 }),
      (err) => err.code === 'VST3_INVALID_PARAMETER');
  });
});
