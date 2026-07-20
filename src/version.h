//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// Version helpers
//-----------------------------------------------------------------------------
#pragma once

#include <string>

namespace evst3 {

// VST3 SDK version string (e.g. "VST 3.8.0 Build 66")
std::string vst3SdkVersion();

// evst3 native addon version
std::string evst3Version();

} // namespace evst3
