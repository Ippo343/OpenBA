#pragma once
// ============================================================================
// src/engine/system/setupdir_helpers.h — Wave 9 lane FS
// ============================================================================
// The three System-namespace leaves of IO::FileSystem::SetupDirectories,
// declared here so src/engine/io/filesystem_setupdirs.cpp can call them
// DIRECTLY rather than through a resolve door. They are its own lane's ports;
// routing them through RT_EXPORT_ONCE would mean a name lookup per call and,
// worse, would make the pair's behaviour depend on registration order.
//
// Both return a String BY VALUE (B.4 Z LAST IO: declared so; the hidden return
// slot is the compiler's).
// ============================================================================

#include "system/String.h"
#include "system/Filename.h"

namespace RtSystem {
// ?GetPlatformName@Configuration@System@@QEAA?AVString@2@W4Platform@12@@Z
//                                                                  0x1800c5210
System::String RtSystem_Configuration_GetPlatformName(void* self, int platform);

// ?GetBuildName@Configuration@System@@QEAA?AVString@2@W4Build@12@@Z 0x1800c5280
System::String RtSystem_Configuration_GetBuildName(void* self, int build);

}  // namespace RtSystem
