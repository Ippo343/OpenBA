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
// ============================================================================

#include <cstddef>
#include "../../types/system/String.h"
#include "../../types/system/WideString.h"

namespace RtSystem {
// (B.4 Z MIRROR2: the Set / AsUTF8 doors are gone; callers hold a real
// System::WideString and call its members.)


}  // namespace RtSystem
