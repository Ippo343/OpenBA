#pragma once
// ============================================================================
// runtime/src/system/setupdir_helpers.h — Wave 9 lane FS
// ============================================================================
// The three System-namespace leaves of IO::FileSystem::SetupDirectories,
// declared here so runtime/src/io/filesystem_setupdirs_win.cpp can call them
// DIRECTLY rather than through a resolve door. They are its own lane's ports;
// routing them through RT_EXPORT_ONCE would mean a name lookup per call and,
// worse, would make the pair's behaviour depend on registration order.
//
// All three return a String BY VALUE: the hidden return slot is the argument
// after `this`.
// ============================================================================

#include "../../types/system/String.h"
#include "../../types/system/Filename.h"

namespace RtSystem {

// ?GetPathDirectory@Filename@System@@QEBA?AV12@XZ                  0x1800c6880
Filename* RtSystem_Filename_GetPathDirectory(const Filename* self, Filename* ret);

// ?GetPlatformName@Configuration@System@@QEAA?AVString@2@W4Platform@12@@Z
//                                                                  0x1800c5210
String* RtSystem_Configuration_GetPlatformName(void* self, String* ret, int platform);

// ?GetBuildName@Configuration@System@@QEAA?AVString@2@W4Build@12@@Z 0x1800c5280
String* RtSystem_Configuration_GetBuildName(void* self, String* ret, int build);

}  // namespace RtSystem
