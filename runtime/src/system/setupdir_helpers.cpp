// ============================================================================
// runtime/src/system/setupdir_helpers.cpp — Wave 9 lane FS
// The three System-namespace leaves of IO::FileSystem::SetupDirectories
// ============================================================================
//
//   | Export                                                    | Retail      | Ord  |
//   |-----------------------------------------------------------|-------------|------|
//   | ?GetPathDirectory@Filename@System@@QEBA?AV12@XZ           | 0x1800c6880 | 2836 |
//   | ?GetPlatformName@Configuration@System@@QEAA?AVString@2@W4Platform@12@@Z | 0x1800c5210 | 2850 |
//   | ?GetBuildName@Configuration@System@@QEAA?AVString@2@W4Build@12@@Z       | 0x1800c5280 | 2470 |
//
// All three verified against Runtime-x64.dll's export directory with pefile
// before a body was written. All three RETURN A String BY VALUE, so the
// hidden return slot sits between `this` and the declared arguments — RCX =
// this, RDX = &ret, R8 = the first real parameter
// (reference_hidden_retval_and_int_pointer_args.md). Getting that wrong here
// would have made the enum argument land in the return slot.
//
// WHY THIS FILE EXISTS INSTEAD OF EDITS TO filename.cpp / configuration.cpp
// -------------------------------------------------------------------------
// Wave 2 lane A owns filename.cpp and Wave 9 lane ENG owns configuration.cpp,
// and both may still be in flight. The wave convention (rt_system_port.cpp's
// header) is one file per lane so no two lanes ever edit the same source; the
// registration door is the mangled name and the Ghidra address, so where the
// body lives is immaterial to correctness.
//
// NOTHING HERE TOUCHES WIN32, so it stays out of a `_win` file per PLAN §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "string.h"
#include "pod_bridge.h"
#include "setupdir_helpers.h"

namespace RtSystem {

namespace {

bool SameText(const char* a, const char* b) {
    if ((a == nullptr) != (b == nullptr)) return false;
    if (a == nullptr) return true;
    return std::strcmp(a, b) == 0;
}

}  // namespace

// ============================================================================
// ?GetPathDirectory@Filename@System@@QEBA?AV12@XZ          retail 0x1800c6880
// ============================================================================
// ENDGAME B.3.1 step 1: THE BODY MOVED to `System::Filename::GetPathDirectory`
// (runtime/src/system/filename.cpp), where the rest of the class lives. The
// reason it was written here in the first place — "Wave 2 lane A owns
// filename.cpp and two lanes must not edit one source" — went with the waves.
// This is now a one-line shim keeping the name, the signature and the two
// registry rows below alive; it dies in step 2 with its last caller.
Filename* RtSystem_Filename_GetPathDirectory(const Filename* self, Filename* ret) {
    // ENDGAME B.3.1 step 2: the POD<->class casts now go through the one
    // transitional bridge (pod_bridge.h). `ret` is the hidden return slot —
    // raw bytes — so the result is placement-constructed into it, which is
    // what retail's CALL at the 0x1800c6880 return path does.
    new (Sys(ret)) System::Filename(Sys(self)->GetPathDirectory());
    return ret;
}

// ============================================================================
// ?GetPlatformName@Configuration@System@@QEAA?AVString@2@W4Platform@12@@Z
//                                                          retail 0x1800c5210
// ============================================================================
// Platform 0 -> "Win" (literal at 0x180155cc4, read with read_memory).
// Anything else -> the NULL String, not an empty one: retail writes
// `*ret = 0` and returns without ever calling Set.
//
// `this` (RCX) is moved nowhere and NEVER DEREFERENCED — checked in the
// disassembly, not inferred from the decompile. That is not a curiosity: it is
// what makes this call safe for SetupDirectories to make while
// `?_singleton@Configuration@System@@0PEAV12@EA` is still an unminted slot.
String* RtSystem_Configuration_GetPlatformName(void* self, String* ret, int platform) {
    (void)self;   // retail never reads it

    // ENDGAME B.3.1 step 2: retail's `*ret = 0` IS the default ctor
    // (??0String@System@@QEAA@XZ, 0x180006760, one qword store), and `ret` is
    // the hidden return slot, so it is placement-constructed rather than
    // assigned. The conditional Set is retail's own 0x1800c95d0 call.
    System::String* out = new (Sys(ret)) System::String();
    if (platform == 0)
        out->Set("Win");

    return ret;
}

// ============================================================================
// ?GetBuildName@Configuration@System@@QEAA?AVString@2@W4Build@12@@Z
//                                                          retail 0x1800c5280
// ============================================================================
// Build 0 -> "Data" (0x180155cd0), build 1 -> "Eval" (0x180155cd8), anything
// else -> the null String. Retail spells the second test as `DEC R8D ; JZ`,
// not as a second compare; same thing, transcribed as an if-chain.
//
// `this` is unread here too. Together with GetPlatformName this is what makes
// the boot's data directory: `Format("%s%s%s%s\\", root, "", "Win", "Data")`
// = "<install>\WinData\", which is exactly the directory the shipped game
// ships (verified on disk).
String* RtSystem_Configuration_GetBuildName(void* self, String* ret, int build) {
    (void)self;   // retail never reads it

    // ENDGAME B.3.1 step 2, exactly as GetPlatformName above.
    System::String* out = new (Sys(ret)) System::String();
    if (build == 0)      out->Set("Data");
    else if (build == 1) out->Set("Eval");

    return ret;
}

}  // namespace RtSystem

// ============================================================================
// Registration — gated by the lane switch in rt_system_port.cpp
// ============================================================================
int RtSystemSetupDirHelpersRegister() {
    using namespace RtSystem;
    int n = 0;

    printf("[RtPort]   Wave9 lane FS helpers: %d exports registered\n", n);
    return n;
}
