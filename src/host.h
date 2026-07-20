//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// Host — napi ObjectWrap that owns the host context and loads plugins.
//-----------------------------------------------------------------------------
#pragma once

#include <napi.h>

#include <memory>
#include <string>

#include "host_application.h"
#include "component_handler.h"

namespace evst3 {

// HostOptions mirrors the JS HostOptions type.
struct HostOptions {
    double sampleRate = 48000.0;
    int32_t maxBlockSize = 512;
    int32_t audioInputs = 2;
    int32_t audioOutputs = 2;
    // Sample size: 32 (kSample32, default) or 64 (kSample64). If the user
    // requests 64 but the plugin refuses via canProcessSampleSize, the host
    // silently falls back to 32.
    int32_t sampleSize = 32;
    // Process mode: 0 = realtime (default), 1 = offline, 2 = prefetch.
    // Matches Steinberg::Vst::ProcessMode.
    int32_t processMode = 0;
};

// Host is the top-level JS class. It owns the EvstHostApplication and provides
// plugin discovery + loading methods.
class Host : public Napi::ObjectWrap<Host> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    Host(const Napi::CallbackInfo& info);
    ~Host() override;

    // Static discovery methods (do not require an instance).
    static Napi::Value ScanDefaultLocations(const Napi::CallbackInfo& info);
    static Napi::Value ScanDirectory(const Napi::CallbackInfo& info);
    static Napi::Value InspectPlugin(const Napi::CallbackInfo& info);

    // Instance methods
    Napi::Value Load(const Napi::CallbackInfo& info);
    Napi::Value GetOptions(const Napi::CallbackInfo& info);

    // Accessors for PluginInstance to use during load.
    EvstHostApplication* hostApplication() const { return hostApp_.get(); }
    const HostOptions& options() const { return options_; }

private:
    HostOptions options_;
    std::unique_ptr<EvstHostApplication> hostApp_;
};

// Validate `opts` in-place against the documented contract
// (sampleRate > 0, maxBlockSize > 0, audioInputs >= 0, audioOutputs >= 0,
// sampleSize ∈ {32, 64}, processMode ∈ {0, 1, 2}). Throws Napi::Error with
// code VST3_INVALID_PARAMETER on the first out-of-range field. Used by both
// `new Host(opts)` and `host.load(path, opts)`.
void validateHostOptions(Napi::Env env, HostOptions& opts);

// Helper: convert a PluginClassInfo C++ struct to a Napi::Object.
Napi::Object pluginInfoToObject(Napi::Env env, const struct PluginClassInfo& info);

} // namespace evst3
