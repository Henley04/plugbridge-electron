//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// Host implementation
//-----------------------------------------------------------------------------
#include "host.h"

#include "discovery.h"
#include "errors.h"
#include "string_convert.h"
#include "version.h"

#include "plugin_instance.h"

#include "public.sdk/source/vst/hosting/module.h"

namespace evst3 {

Napi::FunctionReference Host::constructor;

Napi::Object Host::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Host", {
        InstanceMethod("load", &Host::Load),
        InstanceMethod("getOptions", &Host::GetOptions),
        StaticMethod("scanDefaultLocations", &Host::ScanDefaultLocations),
        StaticMethod("scanDirectory", &Host::ScanDirectory),
        StaticMethod("inspectPlugin", &Host::InspectPlugin),
    });
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("Host", func);
    return exports;
}

Host::Host(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Host>(info) {
    Napi::Env env = info.Env();
    HostOptions opts;
    if (info.Length() >= 1 && info[0].IsObject()) {
        Napi::Object o = info[0].As<Napi::Object>();
        if (o.Has("sampleRate") && o.Get("sampleRate").IsNumber()) {
            opts.sampleRate = o.Get("sampleRate").As<Napi::Number>().DoubleValue();
        }
        if (o.Has("maxBlockSize") && o.Get("maxBlockSize").IsNumber()) {
            opts.maxBlockSize = o.Get("maxBlockSize").As<Napi::Number>().Int32Value();
        }
        if (o.Has("audioInputs") && o.Get("audioInputs").IsNumber()) {
            opts.audioInputs = o.Get("audioInputs").As<Napi::Number>().Int32Value();
        }
        if (o.Has("audioOutputs") && o.Get("audioOutputs").IsNumber()) {
            opts.audioOutputs = o.Get("audioOutputs").As<Napi::Number>().Int32Value();
        }
        if (o.Has("sampleSize") && o.Get("sampleSize").IsNumber()) {
            opts.sampleSize = o.Get("sampleSize").As<Napi::Number>().Int32Value();
        }
        if (o.Has("processMode") && o.Get("processMode").IsString()) {
            std::string pm = o.Get("processMode").As<Napi::String>().Utf8Value();
            if (pm == "offline") opts.processMode = 1;
            else if (pm == "prefetch") opts.processMode = 2;
            else opts.processMode = 0; // "realtime" or anything else
        }
    }
    // Validate per documented contract — throws Napi::Error (VST3_INVALID_PARAMETER)
    // if any field is out of range. Previously we silently coerced invalid
    // values to defaults, which masked user bugs and contradicted API.md.
    validateHostOptions(env, opts);
    options_ = opts;

    hostApp_ = std::make_unique<EvstHostApplication>();
}

// Validates a HostOptions struct against the documented contract.
// Throws a Napi::Error (with code VST3_INVALID_PARAMETER) on the first
// out-of-range field. Field ranges mirror API.md "new Host(opts?)" and
// "host.load(path, opts?)":
//   sampleRate   > 0
//   maxBlockSize > 0
//   audioInputs  >= 0
//   audioOutputs >= 0
//   sampleSize   ∈ {32, 64}
//   processMode  ∈ {0, 1, 2}
void validateHostOptions(Napi::Env env, HostOptions& opts) {
    if (opts.sampleRate <= 0.0) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.sampleRate must be > 0 (got " +
            std::to_string(opts.sampleRate) + ")");
    }
    if (opts.maxBlockSize <= 0) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.maxBlockSize must be > 0 (got " +
            std::to_string(opts.maxBlockSize) + ")");
    }
    if (opts.audioInputs < 0) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.audioInputs must be >= 0 (got " +
            std::to_string(opts.audioInputs) + ")");
    }
    if (opts.audioOutputs < 0) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.audioOutputs must be >= 0 (got " +
            std::to_string(opts.audioOutputs) + ")");
    }
    if (opts.sampleSize != 32 && opts.sampleSize != 64) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.sampleSize must be 32 or 64 (got " +
            std::to_string(opts.sampleSize) + ")");
    }
    if (opts.processMode < 0 || opts.processMode > 2) {
        throwNapiError(env, ErrorCode::InvalidParameter,
            "HostOptions.processMode must be 0 (realtime), 1 (offline), or 2 (prefetch); got " +
            std::to_string(opts.processMode));
    }
}

Host::~Host() {
    // HostApplication releases its PlugInterfaceSupport via the SDK base dtor.
    hostApp_.reset();
}

Napi::Value Host::Load(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "load(path, opts?) requires a string path");
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();

    HostOptions loadOpts = options_;
    if (info.Length() >= 2 && info[1].IsObject()) {
        Napi::Object o = info[1].As<Napi::Object>();
        if (o.Has("sampleRate") && o.Get("sampleRate").IsNumber()) {
            loadOpts.sampleRate = o.Get("sampleRate").As<Napi::Number>().DoubleValue();
        }
        if (o.Has("maxBlockSize") && o.Get("maxBlockSize").IsNumber()) {
            loadOpts.maxBlockSize = o.Get("maxBlockSize").As<Napi::Number>().Int32Value();
        }
        if (o.Has("audioInputs") && o.Get("audioInputs").IsNumber()) {
            loadOpts.audioInputs = o.Get("audioInputs").As<Napi::Number>().Int32Value();
        }
        if (o.Has("audioOutputs") && o.Get("audioOutputs").IsNumber()) {
            loadOpts.audioOutputs = o.Get("audioOutputs").As<Napi::Number>().Int32Value();
        }
        if (o.Has("sampleSize") && o.Get("sampleSize").IsNumber()) {
            loadOpts.sampleSize = o.Get("sampleSize").As<Napi::Number>().Int32Value();
        }
        if (o.Has("processMode") && o.Get("processMode").IsString()) {
            std::string pm = o.Get("processMode").As<Napi::String>().Utf8Value();
            if (pm == "offline") loadOpts.processMode = 1;
            else if (pm == "prefetch") loadOpts.processMode = 2;
            else loadOpts.processMode = 0;
        }
    }
    // Validate the merged options (Host defaults + per-load overrides).
    // Throws VST3_INVALID_PARAMETER on the first out-of-range field.
    validateHostOptions(env, loadOpts);

    return translateExceptions(env, [&]() -> Napi::Value {
        auto instance = PluginInstance::Create(env, path, loadOpts, hostApp_.get());
        return instance;
    });
}

Napi::Value Host::GetOptions(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object o = Napi::Object::New(env);
    o.Set("sampleRate", Napi::Number::New(env, options_.sampleRate));
    o.Set("maxBlockSize", Napi::Number::New(env, options_.maxBlockSize));
    o.Set("audioInputs", Napi::Number::New(env, options_.audioInputs));
    o.Set("audioOutputs", Napi::Number::New(env, options_.audioOutputs));
    o.Set("sampleSize", Napi::Number::New(env, options_.sampleSize));
    o.Set("processMode", Napi::Number::New(env, options_.processMode));
    return o;
}

Napi::Value Host::ScanDefaultLocations(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return translateExceptions(env, [&]() -> Napi::Value {
        Napi::Array result = Napi::Array::New(env);
        uint32_t idx = 0;
        for (const auto& path : defaultPluginPaths()) {
            auto infos = scanDirectory(path);
            for (const auto& info : infos) {
                result[idx++] = pluginInfoToObject(env, info);
            }
        }
        return result;
    });
}

Napi::Value Host::ScanDirectory(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "scanDirectory(path) requires a string path");
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        auto infos = scanDirectory(path);
        Napi::Array result = Napi::Array::New(env, infos.size());
        for (uint32_t i = 0; i < infos.size(); ++i) {
            result[i] = pluginInfoToObject(env, infos[i]);
        }
        return result;
    });
}

Napi::Value Host::InspectPlugin(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        throwNapiError(env, ErrorCode::InvalidParameter, "inspectPlugin(path) requires a string path");
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();
    return translateExceptions(env, [&]() -> Napi::Value {
        auto infos = inspectPlugin(path);
        if (infos.size() == 1) {
            return pluginInfoToObject(env, infos[0]);
        }
        Napi::Array result = Napi::Array::New(env, infos.size());
        for (uint32_t i = 0; i < infos.size(); ++i) {
            result[i] = pluginInfoToObject(env, infos[i]);
        }
        return result;
    });
}

Napi::Object pluginInfoToObject(Napi::Env env, const PluginClassInfo& info) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("path", Napi::String::New(env, info.path));
    o.Set("name", Napi::String::New(env, info.name));
    o.Set("vendor", Napi::String::New(env, info.vendor));
    o.Set("version", Napi::String::New(env, info.version));
    o.Set("category", Napi::String::New(env, info.category));
    o.Set("subCategories", Napi::String::New(env, info.subCategories));
    o.Set("sdkVersion", Napi::String::New(env, info.sdkVersion));
    o.Set("classId", Napi::String::New(env, info.classId));
    o.Set("cardinality", Napi::Number::New(env, info.cardinality));
    Napi::Object factoryInfo = Napi::Object::New(env);
    factoryInfo.Set("vendor", Napi::String::New(env, info.factoryVendor));
    factoryInfo.Set("url", Napi::String::New(env, info.factoryUrl));
    factoryInfo.Set("email", Napi::String::New(env, info.factoryEmail));
    o.Set("factoryInfo", factoryInfo);
    return o;
}

} // namespace evst3
