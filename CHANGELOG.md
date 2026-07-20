# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.2] - 2026-07-20

### Fixed — native robustness & API/d.ts alignment

Three issues surfaced by an external headless-host integration report.

- **Use-after-dispose no longer crashes the process** (was SIGABRT 134).
  Previously, calling any non-dispose method on a `PluginInstance` after
  `dispose()` threw a C++ `EvstException`. node-addon-api's default
  `catch (const Napi::Error&)` handler does not intercept
  `std::runtime_error` subclasses, so the exception escaped to
  `std::terminate` and aborted the Node process. `checkAlive()` now throws
  a `Napi::Error` directly (with `code: 'VST3_FAULTED'`), which
  node-addon-api translates to a JS exception as documented. All 60+
  method entry points were updated to pass `env` to `checkAlive()`.

- **`getParameterTree()` now returns `ParameterTreeNode[]`** as declared in
  `index.d.ts`. The previous implementation returned
  `{ units, parameterCount, unitCount, hasUnitInfo }` — an object wrapper
  with differently-named fields (`id`/`name`/`parentId` instead of
  `unitId`/`unitName`/`parentUnitId`, plus a richer `programLists` shape).
  The native now returns the array directly with field names matching the
  d.ts. The `orphanParameters` extra (debug-only, attached to the root
  unit when present) is retained but not declared in d.ts.

- **`new Host(opts)` and `host.load(path, opts)` now validate options**
  per `docs/API.md`. Previously invalid values (e.g. `sampleRate: -1`)
  were silently coerced to defaults, masking user bugs. The new shared
  `validateHostOptions(env, opts)` throws `VST3_INVALID_PARAMETER` with a
  descriptive message for each out-of-range field:
  - `sampleRate > 0`
  - `maxBlockSize > 0`
  - `audioInputs >= 0`
  - `audioOutputs >= 0`
  - `sampleSize ∈ {32, 64}`
  - `processMode ∈ {0, 1, 2}`

### Tests — 6 new regression tests

- `Lifecycle > use-after-dispose throws VST3_FAULTED on every method (never crashes)`
  probes 30 PluginInstance methods after `dispose()`.
- `Lifecycle > Host constructor validates options per documented contract`
  covers 8 invalid-option cases + 4 valid ones.
- `Lifecycle > host.load(path, opts) validates per-load overrides`.
- `Parameters > getParameterTree() shape (matches index.d.ts)` — 3 new
  tests covering the array shape, per-node fields, and parameter lookup.

## [0.4.1] - 2026-07-20

### Project rename: `electron-vst3-bridge` → `plugbridge-electron`

This release renames the npm package from `electron-vst3-bridge` to
`plugbridge-electron` to align with the GitHub repository name
(`Henley04/plugbridge-electron`) and to reflect the project's roadmap:
`plugbridge-electron` is now positioned as a **multi-format audio plugin
bridge for Electron**, with VST3 shipping today and AU, LV2, and LADSPA
backends planned. The previous package name is deprecated; users should
switch to `npm install plugbridge-electron`.

This is an identifier-only release. No native code, public JS API surface,
error codes, state-envelope format, or native module filename changed.

### Changed — identifiers (npm package name only)

- **`package.json` `name`**: `electron-vst3-bridge` → `plugbridge-electron`.
- **`package.json` `homepage` / `repository.url` / `bugs.url`**: updated to
  `Henley04/plugbridge-electron`.
- **Loader error prefixes** in `index.js`: `electron-vst3-bridge:` →
  `plugbridge-electron:` (covers both `VST3_PLATFORM_UNSUPPORTED` and
  `VST3_LOAD_FAILED` messages).
- **Loader header comment** in `index.js` / `index.mjs`: updated to
  `plugbridge-electron` and now describes the multi-format bridge roadmap
  (VST3 backend today, AU / LV2 / LADSPA planned).
- **`index.d.ts` header comment**: updated to describe the VST3 backend as
  the first of multiple planned backends.
- **`README.md`**: rewritten as a multi-format bridge overview. Added a
  **Roadmap** section (VST3 shipping; AU / LV2 / LADSPA planned) and split
  the Features list into "Cross-format (current and future)" and
  "VST3-specific (current backend)" groups. All `require` / `import`
  examples now use `plugbridge-electron`.
- **`docs/API.md`**: all active `require('electron-vst3-bridge')` examples
  and module references updated to `plugbridge-electron`. Added a new
  `0.4.1 — Rename to plugbridge-electron (multi-format bridge)` section
  with the full identifier-migration table.
- **`CONTRIBUTING.md`**: title, intro, and clone instructions updated to
  `plugbridge-electron`; added a one-paragraph note about the multi-format
  bridge roadmap and the VST3 backend being the only currently-implemented
  backend.
- **`examples/*.js`** (5 files): all `electron-vst3-bridge` references in
  comments and version-print strings updated to `plugbridge-electron`.
  (`require('../')` paths were already correct — they never hard-coded the
  package name.)
- **`test/editor.test.js`** and **`test/plugin/{moduleinfo.json,source/version.h}`**:
  vendor / URL / comment references updated to `plugbridge-electron`.

### Unchanged in 0.4.1 (intentional)

- **`evst3.node`** native module filename — still `evst3` (the VST3 backend
  identifier; future backends will add `eau.node` / `elv2.node` /
  `eladspa.node`).
- **`binding.gyp` target `evst3`** and **`binary.module_name: "evst3"`** —
  internal VST3-backend identifiers, preserved so existing native code is
  unaffected.
- **C++ namespace `evst3`**, **`EvstError` / `EvstErrorCode`** — internal
  VST3-backend symbols.
- **`VST3_*` error code strings** — spec-stable public API exposed via
  `Error.prototype.code`.
- **`'NST3'` state-envelope magic bytes** — public on-disk format (kept
  verbatim since 0.2.0).
- **All public `Host` / `PluginInstance` JS methods, signatures, enums, and
  TypeScript types** — unchanged.

### Roadmap

`plugbridge-electron` is built to host multiple plugin formats under one
Electron-friendly API. The VST3 backend is the reference implementation;
the same host abstractions (`Host`, `PluginInstance`, audio processing,
MIDI, state, editor embedding) will be re-used by future backends.

| Format  | Status       | Native module  | SDK / binding                            |
|---------|--------------|----------------|------------------------------------------|
| VST3    | **Shipping** | `evst3.node`   | Steinberg VST3 SDK v3.8.0 (MIT)          |
| AU      | Planned      | `eau.node`     | Apple Audio Unit SDK (bundled with Xcode)|
| LV2     | Planned      | `elv2.node`    | Lilv / LV2 (MIT-style)                   |
| LADSPA  | Planned      | `eladspa.node` | LADSPA SDK (LGPL, dynamically loaded)    |

When a new backend lands, it will be exposed as a sibling entry point
(e.g. `require('plugbridge-electron/au')`) and will share the same `Host` /
`PluginInstance` JavaScript shape so application code can stay
format-agnostic.

[0.4.1]: https://github.com/Henley04/plugbridge-electron/releases/tag/v0.4.1

## [0.4.0] - 2026-07-20

### Project rename: `nvst3-host` → `electron-vst3-bridge`

This release rebrands the project from `nvst3-host` (a headless VST3 host for
Node.js) to `electron-vst3-bridge` (an audio plugin bridge designed for
Electron). The rename reflects the new headline capability: full GUI/editor
embedding via the VST3 `IPlugView` / `IPlugFrame` contract, driven from
Electron's `BrowserWindow.getNativeWindowHandle()`. The previous package
name is deprecated; users should switch to
`npm install electron-vst3-bridge`.

The native module filename also changes from `nst3.node` to `evst3.node`
(binding.gyp `target_name` updated), and the npm `binary.module_name`
changes from `nst3` to `evst3`. CI no longer publishes a tarball named
`nst3-prebuilds-*.tar.gz` — the new artifact is `evst3-prebuilds-*.tar.gz`.

The `'NST3'` state-envelope magic bytes are intentionally preserved for
backward compatibility with 0.2.0+ state files — they are a public on-disk
format, not a brand identifier.

### Added — GUI / editor surface (the "all VST functionality" gap)

- **`IPlugView` / `IPlugFrame` host-side implementation** — new
  `src/editor_view.{h,cc}` files implement `Steinberg::IPlugFrame` and own
  the lifecycle of the plugin's `IPlugView` (create → attach → resize →
  detach → destroy). The host side negotiates the platform window type
  (`kHWND` on Windows, `kNSView` on macOS, `kX11EmbedWindowID` on Linux)
  via `IPlugView::isPlatformTypeSupported` before calling `attached()`.
- **`IPlugViewContentScaleSupport`** — probed lazily after `attached()` and
  forwarded via the new `plugin.setEditorScale(factor)` JS method. Returns
  `true` when the plugin implements the interface and accepted the value.
- **`IComponentHandler3::requestOpenEditor`** — forwarded to JS as the new
  `'requestOpenEditor'` event. The native handler returns `kResultTrue` to
  acknowledge; the host decides asynchronously whether to call
  `plugin.openEditor(...)`. The listener receives `0` for the default
  `"editor"` view or `1` for any other name.
- **`IComponentHandler3::createContextMenu`** — forwarded to JS as the new
  `'contextMenu'` event. The native handler returns `nullptr` so the SDK
  does not draw anything; the host is responsible for displaying its own
  native menu asynchronously. The listener receives the parameter ID the
  menu was opened for, or `-1` for a generic (non-parameter) menu.
- **6 new `PluginInstance` methods** — `hasEditor()`, `openEditor(handle)`,
  `closeEditor()`, `getEditorSize()`, `setEditorScale(factor)`,
  `isEditorOpen()`. The parent handle is polymorphic: `bigint`,
  `number`, `Buffer`, `Uint8Array`, or `ArrayBuffer` — covering every form
  Electron's `BrowserWindow.getNativeWindowHandle()` can produce.
- **`'editorResize'` event** — synchronous accept/reject callback invoked
  when the plugin's `IPlugView` calls `IPlugFrame::resizeView`. The JS
  listener receives an `EditorViewRect` and MUST return `true` to accept
  (the addon then calls `IPlugView::onSize`) or `false` to reject. This
  is the only event listener in the API that returns a value — the SDK
  contract requires an inline accept/reject.
- **`getPluginInfo()` now reports editor state** — `editorOpen` (boolean),
  `hasEditor` (boolean), and `editorSize` (object with left/top/right/
  bottom/width/height; present only when an editor is open).
- **`PlugInterfaceSupport` advertises the GUI interfaces** — previously the
  host deliberately did NOT advertise `IPlugFrame`/`IPlugView`/
  `IPlugViewContentScaleSupport`/`IContextMenu` (because there was no
  editor support). Now all four are advertised, so plugins query-recognize
  the host as a GUI-capable environment and may behave differently
  (e.g. enable double-click-to-open-editor on parameter controls).
- **`binding.gyp` GUI linkage** — Windows: `gdi32.lib` + `comctl32.lib`
  added to `AdditionalDependencies`. macOS: `-framework AppKit` and
  `-framework Cocoa` added to `OTHER_LDFLAGS`. Linux: requires `libgtk-3-dev`
  at source-build time (already present in the runner image). The
  `EVST3_GUI=1` preprocessor define guards the editor code path.
- **Editor lifecycle teardown in `PluginInstance::teardown()`** —
  `closeEditor()` is called before releasing the component/controller, so
  the plugin's `IPlugView::removed()` runs while it still has access to
  the parent handle.
- **TypeScript surface** — `index.d.ts` adds `EditorViewRect`,
  `EditorSize`, `NativeWindowHandle` types; 6 new `PluginInstance` methods;
  3 new event-name types (`EditorResizeEventName`,
  `RequestOpenEditorEventName`, `ContextMenuEventName`); 3 new listener
  types; the `PluginEventName` / `PluginEventListener` unions are widened;
  `PluginInfoSnapshot` is extended with `hasEditor`, `editorOpen`,
  `editorSize?`.
- **`examples/electron-editor.js`** — runnable Electron example showing
  the editor embedding lifecycle (BrowserWindow → getNativeWindowHandle →
  openEditor → editorResize → closeEditor → dispose).
- **`test/editor.test.js`** — headless test of the editor API surface
  (`hasEditor()` returns false for the no-GUI Gain test plugin,
  `openEditor(0)` returns false without throwing, `closeEditor()` is
  idempotent, `getEditorSize()` returns zeros when no editor is open,
  `setEditorScale()` returns false when there is no scale-support
  interface, `isEditorOpen()` tracks state correctly).

### Changed — Identity / branding

- **Package name**: `nvst3-host` → `electron-vst3-bridge`.
- **Native module filename**: `nst3.node` → `evst3.node` (binding.gyp
  `target_name` changed from `nst3` to `evst3`).
- **npm `binary.module_name`**: `nst3` → `evst3`.
- **CI tarball name**: `nst3-prebuilds-*.tar.gz` → `evst3-prebuilds-*.tar.gz`.
- **C++ namespace**: `nst3` → `evst3` (all `src/*.{h,cc}` files).
- **C++ class renames**: `NstHostApplication` → `EvstHostApplication`,
  `NstPlugInterfaceSupport` → `EvstPlugInterfaceSupport`,
  `NstException` → `EvstException`, `throwNst()` → `throwEvst()`,
  `nst3Version()` → `evst3Version()`.
- **Host name string** reported to plugins via
  `IHostApplication::getName`: `"Node.js VST3 Host"` → `"Electron VST3 Bridge"`.
- **TS error type**: `NstError` → `EvstError`, `NstErrorCode` →
  `EvstErrorCode`. Deprecated aliases (`NstError`, `NstErrorCode`) are
  exported from `index.d.ts` so existing user code continues to type-check,
  but the runtime does not export an `NstError` symbol — it never did.
- **Loader messages** in `index.js`: all `"nvst3-host: …"` prefixes
  changed to `"electron-vst3-bridge: …"`.
- **Version**: `0.3.1` → `0.4.0`.

### Preserved for backward compatibility

- **`'NST3'` state-envelope magic bytes** — kept verbatim so 0.2.0+ state
  files continue to load. This is a public on-disk format, not a brand
  identifier.
- **`VST3_*` error code strings** — kept verbatim. These are spec-stable
  public API exposed via `Error.prototype.code`.
- **`NstError` / `NstErrorCode` TypeScript aliases** — exported as
  `@deprecated` aliases so existing user code keeps type-checking.

[0.4.0]: https://github.com/Henley04/electron-vst3-bridge/releases/tag/v0.4.0

## [0.3.1] - 2026-07-20

### Fixed

- **ESM named imports broken on Node 22+** — `import { Host, version } from 'nvst3-host'` failed with `SyntaxError: The requested module 'nvst3-host' does not provide an export named 'Host'`. Root cause: the native addon exposes its surface as N-API properties on the `native` object at load time, which is opaque to Node's `cjs-module-lexer` (the static analyzer that makes `export * from './index.js'` in `index.mjs` re-export named bindings). Fix: add no-op self-references (`exports.X = exports.X`) at the end of `index.js` for every public top-level symbol. This pattern is recognized by `cjs-module-lexer` and re-exported as named ESM bindings, without changing the runtime API.

### Changed

- `files` in `package.json` no longer includes `binding.gyp`, `src/`, or `third_party/`. The previous `third_party/` entry was ineffective (npm publish does not include git submodule contents) and the source-build fallback was therefore broken anyway — users on platforms without a prebuilt binary (e.g. `darwin-x64` / Intel Macs) should clone the GitHub repo with submodules rather than rely on `npm install` to compile from source. Removing the dead fallback files shrinks the published package and makes the failure mode clearer.

### Out of scope (unchanged from 0.3.0)

- UMP / MIDI 2.0, async worker thread, standalone `.vst3` bundle publishing, DAW-style integration test suite, code signing — see [0.3.0] notes for rationale.

[0.3.1]: https://github.com/Henley04/nvst3-host/releases/tag/v0.3.1

## [0.3.0] - 2026-07-20

### Audit-driven fixes — Headless-host hardening

This release closes out the host-side audit findings (10 critical + 7 partial +
8 not-implemented items). All changes are additive or correctness-only; no
breaking API changes. CI matrices and prebuilt-binary platforms have been
updated to reflect GitHub Actions runner availability.

### Added

- ESM dual-format entry — `index.mjs` re-exports the CJS surface so `import { Host } from 'nvst3-host'` works in ESM-only projects. The `package.json` `exports` field routes `.import`, `.require`, and `.types` correctly.
- Instance lifecycle tracking — `Host` constructor wrapper now tracks live `PluginInstance` objects and registers `process.on('beforeExit')` / `process.on('exit')` hooks that best-effort dispose any leaked instances before the native module unloads. Prevents use-after-free crashes when users forget to call `dispose()`.
- `setSystemTime(nanos)` — JS API to feed the host's monotonic system-time cache (nanoseconds since epoch). The real-time `process()` path now reads `systemTime` from a `std::atomic<int64_t>` instead of calling `std::chrono::system_clock::now()`, removing the only syscall on the audio thread.
- `getPluginInfo()` — comprehensive diagnostic snapshot combining static info, live state (active / processing / faulted / disposed), latency / tail / sample-size capability, all four bus categories with per-bus active state and speaker arrangement, and a boolean map of the 11 optional VST3 interfaces the plugin exposes (midiMapping, unitInfo, programListData, unitData, noteExpression, keyswitchController, processContextRequirements, audioPresentationLatency, prefetchableSupport, channelContextInfoListener, editController2).
- `getParameterTree()` — groups every parameter by `unitId` (root unit for orphans) and includes each parameter's current normalized value. Returns an array of unit nodes mirroring the `IUnitInfo` tree plus per-unit parameter lists.
- `IStreamAttributes` on `BufferStream` — `getFileName(String128)` and `getAttributes()` are now implemented (previously returned empty / nullptr). Host-side mutators `setFileName(utf8)`, `setStateType(utf8)`, and `setFilePath(utf8)` populate the stream's meta-info before handing it to the plugin for `setState`/`getState`. The lazy `getAttributes()` reuses the SDK `HostAttributeList` implementation.
- `linux-arm64` prebuilt binary via the new `ubuntu-24.04-arm` GitHub Actions runner. The prebuilt count is now 4 (was 3).
- `test/plugin/moduleinfo.json` — VST3 3.6.10+ module descriptor copied into the test plugin bundle on macOS and Linux by CMake.
- VST3 Validator step in CI (Linux x64 only). Runs `validator` if available on PATH, otherwise skips silently — keeps the matrix green on runners without the validator binary.
- Real-time safety smoke test (`test/rt-safety.test.js`). Three assertions cover the steady-state `process()` loop: (1) `heapUsed` does not grow beyond a small bound across 1000 blocks (catches runaway allocations); (2) the p99 per-block wall-clock time stays below a generous regression ceiling (catches accidental syscalls or heavy work on the audio thread); (3) output is bit-identical across many blocks of identical input (catches stateful drift). The test runs with `--expose-gc` so the heap-growth assertion can force GC snapshots; on runners without `--expose-gc` it falls back to a less-precise delta.

### Changed

- `version()` now reports `0.3.0`. `package.json` and `src/version.cc` kept in sync.
- `NstHostApplication` rewritten to directly implement `IHostApplication` instead of inheriting from `Steinberg::Vst::HostApplication`. The base class's `mPlugInterfaceSupport` was private and could not be reassigned, leaving the curated `NstPlugInterfaceSupport` list as dead code. The new implementation constructs `nstPlugInterfaceSupport_` in its own constructor, forwards `queryInterface` to it, and reuses the SDK's `HostMessage` / `HostAttributeList` for `createInstance`. `addRef` / `release` keep the singleton-style refcount (always returns 1) since lifetime is owned by the `Host` JS wrapper.
- `ComponentHandler::beginEdit` / `endEdit` now guard `activeGestures_` with `std::mutex` — previously the set was accessed from both the JS thread (user gesture begin/end) and the audio thread (restart propagation), with no synchronization.
- `binding.gyp` adds `SMTG_CPP_17=1` to the defines list, enabling the SDK's `std::u16string_view` variants in `vstbus.h` for C++17 hosts.
- `README` platform table now reflects runner reality: prebuilts ship for `win32-x64`, `darwin-arm64`, `linux-x64`, `linux-arm64`. `darwin-x64` (Intel Macs) and other x86 targets may be unavailable on GitHub Actions — source-build fallback is the supported path for those triples.
- `SUPPORTED_TRIPLES` in `index.js` retains all five entries for source-build discovery, but the loader's prebuilt path will only find binaries for the four ARM/x64 combinations actually shipped.

### Fixed

- `process()` no longer calls `std::chrono::system_clock::now()` on the audio thread. Callers that need `ProcessContext::systemTime` populated must either call `setSystemTime(Date.now() * 1e6)` periodically (e.g. from a `setInterval` on the JS thread) or set it once at activation; otherwise `systemTime` is `0` and the `kSystemTimeValid` bit stays clear.
- `NstPlugInterfaceSupport` is now actually installed on the host context — previously the curated FUID list was constructed but never reachable via `queryInterface`, so plugins probing for `IComponentHandler{,2,3}`, `IHostApplication`, `IPlugInterfaceSupport`, etc. would see `kNoInterface` and fall back to degraded behavior.

### CI

- New matrix entry `ubuntu-24.04-arm` (`linux-arm64` triple).
- `darwin-x64` runner is no longer available on GitHub Actions (Intel macOS runners were retired). Source builds remain supported via the `macos-latest` (arm64) cross-compilation target when an Intel Mac is unavailable.
- Release job's prebuild count assertion raised from 3 to 4.

### Out of scope (deferred)

- **UMP / MIDI 2.0** — VST3.7+ Event List UMP variants are not yet wired through the JS MIDI API. Full UMP support requires extending the `Event` struct, mapping UMP 1.x/2.x message bytes to VST3 `LegacyMIDICCOutEvent` / `kNoteExpressionValue` events, and adding a new `addUmpMessage(group, status, bytes)` JS surface. This is a major API addition, not a bug fix, and is intentionally deferred to a future minor-version spec.
- **Async / batch processing worker thread** — `process()` remains synchronous. VST3 plugins are NOT thread-safe by design: the SDK contract requires `IAudioProcessor::process` to be called from the host's single audio thread. Wrapping `process()` in `napi_async_work` would run it on the libuv worker pool, violating the SDK contract for any plugin that assumes single-threaded access (most do — they use thread-local state, non-atomic caches, etc.). The correct async pattern for VST3 hosts is to use a real-time audio thread on the C++ side with a lock-free ring buffer to the JS thread; that's a substantial architecture addition, deferred to a future spec.
- Standalone `.vst3` bundle publishing for the test plugin (only the build artifact is exercised by tests).
- DAW-style integration test suite (Validator smoke test is the current substitute).
- Code signing / notarization for prebuilt macOS binaries (unsigned for now; downstream users can re-sign).

[0.3.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.3.0

## [0.2.0] - 2026-07-19

### VST3 Spec Coverage — Complete

This release completes the project's VST3 host implementation to cover the
full set of host-side behaviors defined by the VST3 SDK specification. All
changes are additive; existing callers are unaffected unless explicitly noted
below.

### Added

- 64-bit audio processing (`kSample64`) — `sampleSize: 64` in `HostOptions`/`LoadOptions`; `getSampleSize()`, `canProcessSampleSize(size)`.
- Configurable process mode (`realtime` / `offline` / `prefetch`) — `processMode` in `HostOptions`/`LoadOptions`; `Event::kIsLive` cleared for non-realtime modes.
- Tail-samples query — `getTailSamples()` (returns `Number.POSITIVE_INFINITY` for `kInfiniteTail`).
- Parameter-flush blocks — `process({ numSamples: 0 })` flushes pending parameter changes without audio.
- Silence-flag propagation — `ProcessBlock.inputSilenceFlags` (per input bus) and `ProcessResult.outputSilenceFlags` (per output bus).
- Parameter string parsing — `parseParameter(id, str)`.
- Plain/normalized conversion — `plainToNormalized(id, plain)`, `normalizedToPlain(id, normalized)`.
- `Event::noteId` propagation — NoteOn / NoteOff / PolyPressure `MidiEvent` variants accept `noteId?: number`.
- `IUnitInfo` — `getUnitCount`, `getUnitInfo`, `getProgramListCount`, `getProgramListInfo`, `getProgramName`, `selectProgram`, `getCurrentUnit`, `getUnitByBusInfo`.
- `IProgramListData` / `IUnitData` — `getProgramData`, `setProgramData`, `getUnitData`, `setUnitData`.
- `INoteExpressionController` — `getNoteExpressionCount`, `getNoteExpressionInfo`, `addNoteExpressionEvent`.
- `IKeyswitchController` — `getKeyswitchCount`, `getKeyswitchInfo`.
- Runtime bus management — `getBusList`, `getBusInfo`, `activateBus`.
- Speaker-arrangement API — `setBusArrangement`, `getBusArrangement`, `SpeakerArrangement` enum.
- Routing info — `getRoutingInfo(srcBus, dstBus)`.
- Configurable `ProcessContext` — `setProcessContext(opts)`, `getProcessContext()`, `ProcessContextOptions` type.
- `IProcessContextRequirements` — `getProcessContextRequirements()`, `ProcessContextRequirementFlags` enum; the host gates recompute of unneeded `ProcessContext` fields each block.
- `IAudioPresentationLatency` — `setAudioPresentationLatency(busIndex, latencySamples)`.
- `IInfoListener` — `setChannelContextInfo(info)`, `ChannelContextInfo` type, `ChannelContextInfoFlags` enum.
- `IPrefetchableSupport` — `isPrefetchable()`.
- `IEditController2` — `setKnobMode(mode)`, `KnobMode` enum.
- Restart auto-react — `applyRestartFlags(flags)`; `restartComponent` re-queries affected SDK state BEFORE emitting the JS event.
- Mutable `ProcessSetup` — `setProcessSetup({ sampleRate?, maxBlockSize?, processMode?, sampleSize? })`.
- Custom `IPlugInterfaceSupport` — host advertises exactly the 13 implemented interfaces (no GUI-only interfaces such as `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport`).
- Plugin→host events — `on('dirty')`, `on('beginGesture')`, `on('endGesture')`, `on('startGroup')`, `on('finishGroup')` (in addition to the existing `on('restart')`).
- New enums: `SampleSize`, `ProcessMode`, `BusDirection`, `KnobMode`, `NoteExpressionTypeIds`, `SpeakerArrangement`, `ProcessContextRequirementFlags`, `ChannelContextInfoFlags`.
- New types: `ProcessSetupOptions`, `ProcessResult`, `UnitInfo`, `ProgramListInfo`, `BusRef`, `NoteExpressionInfo`, `NoteExpressionEvent`, `KeyswitchInfo`, `BusInfo`, `RoutingInfo`, `ProcessContextOptions`, `ProcessContextSnapshot`, `ChannelContextInfo`.

### Modified

- `process()` return type widened from `void` to `ProcessResult | void` (existing callers ignoring the return value are unaffected).
- `process()` accepts `numSamples: 0` as a parameter-flush block (previously rejected).
- `saveState()` writes a versioned `NST3` envelope (4-byte magic, 1-byte version, length-prefixed component + controller blobs); `loadState()` auto-detects the envelope and falls back to legacy single-blob loading for backward compatibility with 0.1.0 state files.
- `ComponentHandler::restartComponent` now invokes `applyRestartFlags` before emitting the JS `restart` event.
- `ComponentHandler::setDirty` emits a `dirty` JS event (previously a no-op).
- `ComponentHandler::beginEdit` / `endEdit` track active gestures and emit `beginGesture` / `endGesture` JS events.
- `HostOptions` / `LoadOptions` accept `sampleSize?: 32 | 64` and `processMode?: 'realtime' | 'offline' | 'prefetch'`.
- Steady-state `process()` path uses `IProcessContextRequirements` to skip recomputation of unneeded `ProcessContext` fields; zero allocations maintained.

### Test fixtures

- `GainProcessor` test plugin extended: `canProcessSampleSize` returns `kResultTrue` for both 32 and 64; `IUnitInfo` (Root unit + Presets list with Init/Bright programs); `INoteExpressionController` (Volume expression); `kProgramId` parameter tagged `kIsProgramChange`.

### Documentation

- Comprehensive `docs/API.md` `## 0.2.0 — VST3 Spec Coverage` section covering all new methods, types, and enums.
- README "Features" section updated to reflect the now-complete VST3 host capabilities.

### Out of scope (deferred to a future GUI-support spec)

- `IPlugView` / `IPlugFrame` / `IPlugViewContentScaleSupport` (window-handle embedding).
- `IComponentHandler3::createContextMenu` (only meaningful with a visible editor).
- `IComponentHandler2::requestOpenEditor` / `requestZoomFactor` / `notifyZoom` (GUI lifecycle).
- `IStreamAttributes` extension on `BufferStream` (`.vstpreset` file loading).

### Known Limitations

- No GUI/editor support — `nvst3-host` is a headless host. Plugins that ship only an editor still process audio correctly; their `IPlugView` is never opened.
- No built-in signal graph or routing layer — each `PluginInstance` is a single plugin; chaining is the caller's responsibility.
- Prebuilt Linux binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).
- macOS binaries target `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- No native async/batch API — all calls are synchronous from JavaScript's perspective (audio thread work happens inside `process()`).

[0.2.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.2.0

## [0.1.0] - 2026-07-18

### Added

- Initial release.
- VST3 plugin loading via the official Steinberg VST3 SDK v3.8.0 (MIT-licensed since v3.7.7).
- Cross-platform support: Windows x64, macOS x64, macOS arm64 (Apple Silicon), Linux x64.
- Prebuilt native binaries shipped via `prebuildify` — no compiler toolchain required for `npm install`.
- `Host` class with `load(path, opts)`, `getOptions()`, `scanDefaultLocations()`, `scanDirectory(path)`, `inspectPlugin(path)`.
- `PluginInstance` class covering:
  - **Lifecycle**: `dispose()` (idempotent), `[Symbol.dispose]()` for `using` syntax, `on('restart', cb)`.
  - **Metadata**: `getInfo()`, `getLatency()`.
  - **Processing**: `setActive(bool)`, `setProcessing(bool)`, `process({ inputs, outputs, numSamples })`.
  - **Parameters**: `getParameterCount()`, `getParameterInfo(index)`, `getParameter(id)`, `setParameter(id, value)`, `setParameters(changes[])`, `formatParameter(id, value)`.
  - **MIDI**: `addMidiEvent(event)`, `addMidiBytes(sampleOffset, bytes)`, `takeOutputEvents()`, `clearEvents()`.
  - **State**: `saveState()` → `Buffer`, `loadState(Buffer)`.
- Zero-copy audio processing — `Float32Array` channel buffers passed directly to the plugin via `AudioBusBuffers` channel pointers.
- Thread-safe restart notifications via `Napi::ThreadSafeFunction` (no locks held on the audio thread).
- Full MIDI event support: Note On/Off, Poly Pressure, Controller, Program Change, Channel Pressure, Pitch Bend, and SysEx (input and output).
- Plugin discovery across platform-default VST3 locations plus arbitrary directories.
- State persistence round-trip via `IComponent::getState`/`setState` and `IEditController::setComponentState`.
- Structured error codes: `VST3_LOAD_FAILED`, `VST3_FACTORY_MISSING`, `VST3_COMPONENT_CREATION_FAILED`, `VST3_CONTROLLER_MISSING`, `VST3_NOT_ACTIVE`, `VST3_NOT_PROCESSING`, `VST3_FAULTED`, `VST3_PLATFORM_UNSUPPORTED`, `VST3_INVALID_PARAMETER`, `VST3_INVALID_BUFFER`, `VST3_PROCESSING_ERROR`, `VST3_STATE_ERROR`, `VST3_MIDI_ERROR`, `VST3_UNKNOWN`.
- Faulted-state isolation — after a `process()` failure, subsequent calls reject with `VST3_FAULTED` until `dispose()` is called.
- Hand-written TypeScript definitions (`index.d.ts`) mirroring the native surface 1:1 for editor IntelliSense.
- Enums: `ParameterFlags`, `RestartFlags`, `BusType`, `MediaType`, `MidiEventType`, `PluginCategory`.
- `version()` returning `{ native, vst3sdk, napi }` for diagnostic introspection.
- `SUPPORTED_TRIPLES` constant listing the four supported platform triples.
- `NAPI_VERSION` constant exposing the Node-API version the binary was compiled against.
- Loader (`index.js`) with structured `VST3_PLATFORM_UNSUPPORTED` errors when no prebuilt matches and source-build fallback fails.
- C++17 source build via `node-gyp` with `binding.gyp` configuring VST3 SDK include paths and platform-specific defines.
- macOS binaries built with `MACOSX_DEPLOYMENT_TARGET=10.13` for backwards compatibility.
- Linux binaries linked against `libdl`/`libpthread` only (no plugin-runtime dependencies).

### Known Limitations

- No GUI/editor support — `nst3` is a headless host. Plugins that ship only an editor still process audio correctly; their `IPlugView` is never opened.
- 32-bit float audio only (`kSample32`); 64-bit double precision (`kSample64`) is not exposed.
- `processMode` is always `kRealtime`; no explicit offline rendering mode is requested from plugins.
- No built-in signal graph or routing layer — each `PluginInstance` is a single plugin; chaining is the caller's responsibility.
- Prebuilt Linux binaries require `glibc ≥ 2.28` (Ubuntu 18.04+ / Debian 10+).
- macOS binaries target `MACOSX_DEPLOYMENT_TARGET=10.13` (High Sierra and later).
- No native async/batch API — all calls are synchronous from JavaScript's perspective (audio thread work happens inside `process()`).

[0.1.0]: https://github.com/Henley04/nvst3-host/releases/tag/v0.1.0
