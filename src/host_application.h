//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// HostApplication — directly implements Steinberg::Vst::IHostApplication so
// that we own our curated EvstPlugInterfaceSupport from construction (the SDK
// helper class HostApplication creates its PlugInterfaceSupport internally
// and keeps the member private, which leaves no way for a subclass to swap
// in its own list of advertised interfaces). Implementing IHostApplication
// directly also gives us proper refcounting — the SDK base returns 1 from
// addRef/release (singleton-style), which is wrong for a host that is
// queried by every loaded plugin instance.
//-----------------------------------------------------------------------------
#pragma once

#include "public.sdk/source/vst/hosting/pluginterfacesupport.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

#include <atomic>
#include <functional>
#include <memory>

namespace evst3 {

// Forward decl
class ComponentHandler;

// EvstPlugInterfaceSupport subclasses the SDK's PlugInterfaceSupport helper
// and advertises exactly the host interfaces evst3 implements. This is the
// list plugins see when they query IHostApplication::queryInterface for
// IPlugInterfaceSupport and call isPlugInterfaceSupported on the result.
//
// evst3 advertises the GUI / editor interfaces (IPlugFrame, IPlugView,
// IPlugViewContentScaleSupport, IContextMenu) since the host implements
// editor embedding via IPlugFrame and forwards context-menu requests from
// IComponentHandler3::createContextMenu to the JS side.
class EvstPlugInterfaceSupport : public Steinberg::Vst::PlugInterfaceSupport {
public:
    EvstPlugInterfaceSupport();
    ~EvstPlugInterfaceSupport() noexcept override;
};

// EvstHostApplication directly implements Steinberg::Vst::IHostApplication.
// It owns its own EvstPlugInterfaceSupport instance, which is installed from
// construction (not swapped in after the fact, which is impossible because
// the SDK HostApplication helper keeps mPlugInterfaceSupport private).
//
// queryInterface exposes IHostApplication, FUnknown, and forwards to the
// EvstPlugInterfaceSupport so plugins can obtain IPlugInterfaceSupport
// directly from the host context. Refcounting is real (using the SDK
// FUnknownPrivate::atomicAdd helper via IMPLEMENT_REFCOUNT) so a plugin
// holding a reference to the host does not see it disappear early.
class EvstHostApplication : public Steinberg::Vst::IHostApplication {
public:
    EvstHostApplication();
    ~EvstHostApplication() noexcept;

    // IHostApplication
    Steinberg::tresult PLUGIN_API getName(Steinberg::Vst::String128 name) override;
    Steinberg::tresult PLUGIN_API createInstance(Steinberg::TUID cid,
                                                 Steinberg::TUID _iid,
                                                 void** obj) override;

    // FUnknown — implemented via DECLARE_FUNKNOWN_METHODS below.
    // Exposes IHostApplication + FUnknown and forwards IPlugInterfaceSupport
    // queries to evstPlugInterfaceSupport_.
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // Accessor used internally by Host / PluginInstance when wiring up
    // connection proxies etc. Returns the curated support instance.
    EvstPlugInterfaceSupport* plugInterfaceSupport() const {
        return evstPlugInterfaceSupport_;
    }

    // Set the component handler that the host uses to route beginEdit / restart
    // calls. Stored as a raw pointer (the handler is owned by PluginInstance).
    void setComponentHandler(ComponentHandler* handler) { handler_ = handler; }

    ComponentHandler* componentHandler() const { return handler_; }

private:
    // FUnknown refcount. We mirror the SDK HostApplication helper's
    // singleton-style behavior (addRef/release always return 1) because
    // EvstHostApplication's lifetime is owned by the Host JS wrapper, not
    // by COM refcounting. The member is kept so the SDK's atomicAdd helper
    // compiles and so plugins querying queryInterface repeatedly don't trip
    // an assert under SMTG_FUNKNOWN_DTOR_ASSERT.
    std::atomic<int32_t> __funknownRefCount{1};
    ComponentHandler* handler_ = nullptr;
    // The curated IPlugInterfaceSupport advertised through queryInterface.
    // Held as an IPtr so it is released when EvstHostApplication is destroyed.
    Steinberg::IPtr<EvstPlugInterfaceSupport> evstPlugInterfaceSupport_;
};

} // namespace evst3
