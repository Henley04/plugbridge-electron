//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// ComponentHandler implementation
//-----------------------------------------------------------------------------
#include "component_handler.h"

#include "pluginterfaces/vst/ivsthostapplication.h" // for kLatencyChanged etc.

namespace evst3 {

ComponentHandler::ComponentHandler() = default;
ComponentHandler::~ComponentHandler() noexcept {
    instance_ = nullptr;
}

Steinberg::tresult PLUGIN_API ComponentHandler::beginEdit(Steinberg::Vst::ParamID id) {
    // beginEdit marks the start of a user-driven automation gesture. We track
    // the active gesture in activeGestures_ so future automation recording can
    // attribute subsequent performEdit points to the right gesture, and emit a
    // 'beginGesture' JS event so the host can react (e.g. start recording).
    {
        std::lock_guard<std::mutex> lk(gesturesMutex_);
        activeGestures_.insert(id);
    }
    if (hostEventCb_) {
        hostEventCb_({ "beginGesture", static_cast<double>(id), /*isBool*/ false,
                       /*hasPayload*/ true });
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::performEdit(
    Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) {
    // performEdit is called when the controller wants the host to record a
    // parameter automation point. We forward it to the PluginInstance so it
    // can update the controller value and add a point to the input parameter
    // change queue for the next process call (per VST3 spec). No additional
    // JS event is emitted for performEdit — the inputParams_ queue is the
    // source of truth and emitting per-point events would be too noisy.
    if (performEditSink_) {
        performEditSink_->onPerformEdit(id, valueNormalized);
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::endEdit(Steinberg::Vst::ParamID id) {
    // endEdit marks the end of an automation gesture. Remove from the active
    // set and emit an 'endGesture' JS event.
    {
        std::lock_guard<std::mutex> lk(gesturesMutex_);
        activeGestures_.erase(id);
    }
    if (hostEventCb_) {
        hostEventCb_({ "endGesture", static_cast<double>(id), /*isBool*/ false,
                       /*hasPayload*/ true });
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::restartComponent(Steinberg::int32 flags) {
    lastRestartFlags_.store(flags, std::memory_order_release);
    // Per the VST3 spec, restartComponent signals that the host should
    // re-query affected SDK state before delivering the notification to the
    // user. We invoke the apply-restart callback synchronously first so the
    // host's cached state (latency, bus info, etc.) is refreshed before the
    // JS 'restart' event fires; users handling the event themselves see the
    // up-to-date state without needing to call applyRestartFlags() manually.
    if (applyRestartCb_) {
        applyRestartCb_(flags);
    }
    if (restartCb_) {
        restartCb_(flags);
    }
    return Steinberg::kResultTrue;
}

// IComponentHandler2 — emit JS events for plugin→host calls.
Steinberg::tresult PLUGIN_API ComponentHandler::setDirty(Steinberg::TBool state) {
    // The plugin asks the host to mark the project dirty (e.g. a preset was
    // modified in-place). We emit a 'dirty' JS event carrying the boolean.
    if (hostEventCb_) {
        hostEventCb_({ "dirty", state ? 1.0 : 0.0, /*isBool*/ true,
                       /*hasPayload*/ true });
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::requestOpenEditor(Steinberg::FIDString name) {
    // The plugin is asking the host to open its editor. Forward the editor
    // name (e.g. "editor" for the default editor, or a plugin-defined name)
    // to JS as a 'requestOpenEditor' event so the host can decide whether
    // to call plugin.openEditor(parentHandle) with a suitable parent
    // window. Return kResultTrue to acknowledge the request (the host will
    // open the editor asynchronously from JS); kResultFalse would cause
    // the plugin to believe no editor is available.
    if (hostEventCb_) {
        // We can only carry a string payload through HostEvent by stashing
        // it; the existing TSFN contract is numeric. To keep the binary
        // format stable, we encode the editor name as a small integer:
        //   0 = default "editor" name
        //   1 = any non-default name (the JS side can interrogate via
        //       plugin.getPluginInfo() if it needs the actual string).
        // This is sufficient for the common case (plugins only ever
        // request the default editor).
        double value = (name && std::string(name) != "editor") ? 1.0 : 0.0;
        hostEventCb_({ "requestOpenEditor", value, /*isBool*/ false,
                       /*hasPayload*/ true });
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::startGroupEdit() {
    // The plugin is bracketing a group of parameter edits. Emit a 'startGroup'
    // JS event so the host can batch subsequent notifications atomically.
    // (Note: there is no host→plugin group API in VST3; this is purely a
    // plugin→host notification. The host-side atomic batch primitive is
    // PluginInstance::setParameters, which queues multiple parameter changes
    // for the next process() call as one batch.)
    if (hostEventCb_) {
        hostEventCb_({ "startGroup", 0.0, /*isBool*/ false, /*hasPayload*/ false });
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::finishGroupEdit() {
    if (hostEventCb_) {
        hostEventCb_({ "finishGroup", 0.0, /*isBool*/ false, /*hasPayload*/ false });
    }
    return Steinberg::kResultTrue;
}

Steinberg::Vst::IContextMenu* PLUGIN_API
ComponentHandler::createContextMenu(Steinberg::IPlugView* plugView,
                                    const Steinberg::Vst::ParamID* paramID) {
    // The plugin is asking the host to populate a context menu for a
    // parameter (or for the editor as a whole when paramID is null).
    // evst3 does not render native menus itself — the host's renderer
    // (Electron BrowserWindow) provides the menu chrome. We forward the
    // request to JS as a 'contextMenu' event carrying the paramId (or
    // -1 when paramID is null), then return nullptr to indicate the
    // host will display its own menu asynchronously.
    //
    // Plugins that absolutely require a non-null IContextMenu return
    // value can fall back to their internal menu implementation when
    // they receive nullptr here (this is the documented VST3 behavior).
    if (hostEventCb_) {
        double value = paramID ? static_cast<double>(*paramID) : -1.0;
        hostEventCb_({ "contextMenu", value, /*isBool*/ false,
                       /*hasPayload*/ true });
    }
    (void)plugView;
    return nullptr;
}

} // namespace evst3
