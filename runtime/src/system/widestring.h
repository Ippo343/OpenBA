#pragma once
// ============================================================================
// runtime/src/system/widestring.h — the registered `System::WideString` doors
// ============================================================================
// ENDGAME B.3.4b SYSTEM (4a): the LAYOUT and the class moved to
// `runtime/types/system/WideString.h`, which is where a type header belongs and
// where `tests/unit/system_abi_test.cpp` can reach it. This file keeps exactly
// what it always was for its three includers (platform_startup_win.cpp,
// application_win.cpp, updateloop_loadresources.cpp, value_types.cpp): the POD
// spelling and the free-function doors, whose names, signatures and
// registration rows are unchanged. The doors now DELEGATE to the class members;
// see the bodies in widestring.cpp.
//
// ⚠ `WideString` here is `RtSystem::WideString`, the POD. Inside a
// `namespace System { ... }` block the same bare name binds the real CLASS —
// B34_RULES §7.2's grep hazard, armed for this name.
// ============================================================================

#include <cstddef>
#include "../../types/system/String.h"
#include "../../types/system/WideString.h"

namespace RtSystem {
// ?Set@WideString@System@@QEAAXPEB_W@Z            0x1800caf40
void        RtSystem_WideString_Set(WideString* self, const wchar_t* src);
// ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ   0x1800cacd0
String*     RtSystem_WideString_AsUTF8(const WideString* self, String* ret);
// ??0WideString@System@@QEAA@AEBVString@1@@Z      0x1800cab50
WideString* RtSystem_WideString_CtorFromString(WideString* self, const String* src);
// ??0WideString@System@@QEAA@PEBD@Z               0x1800cb110  (Wave 9 lane PLAT)
WideString* RtSystem_WideString_CtorFromChars(WideString* self, const char* src);
// ??1WideString@System@@QEAA@XZ                   0x18000a730
void        RtSystem_WideString_Dtor(WideString* self);

}  // namespace RtSystem
