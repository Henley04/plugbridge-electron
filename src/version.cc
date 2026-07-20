//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// Version helpers
//-----------------------------------------------------------------------------
#include "version.h"

#include "pluginterfaces/vst/ivsthostapplication.h"

namespace evst3 {

std::string vst3SdkVersion() {
    // The VST3 SDK exposes its version via kVstVersionString if available;
    // otherwise we compile a static string. Steinberg updates this in
    // pluginterfaces/vst/ivstaudioprocessor.h (kVstVersionString).
    return kVstVersionString;
}

std::string evst3Version() {
    return "0.4.2";
}

} // namespace evst3
