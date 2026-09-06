#pragma once
// ============================================================================
// runtime/funcs/rt_port.h — shared scaffolding for Runtime port wave files
// ============================================================================
//
// A ported Runtime function lands in three places:
//
//   1. The implementation, a plain free function with the retail raw ABI
//      (see the ABI note in runtime/types/math/Math.h).
//   2. A RegisterRuntimeExport(<mangled name>, &ours) call in the wave's
//      *_PortInit(), which flips every one of our ~1,180 call sites.
//   3. Optionally a detour (standalone/runtime_detour.h) so the DLL's OWN
//      internal callers come to us too.
//
// This header holds only what every wave file needs: the per-wave kill switch
// and the "original DLL function" lookup the diff harness compares against.
//
// Wave init functions are declared at the bottom and called from
// RuntimePortInit() in standalone/resolve_standalone.cpp.
// ============================================================================

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "rt_diff.h"

// The seam's four entry points, DECLARED HERE rather than pulled in from
// standalone/resolve.h. That header includes <windows.h>, and rt_port.h is
// included by every file under runtime/src/ — which PLAN.md §4.1 requires to
// stay platform-neutral so a _posix/_sdl twin can be written later. Including
// resolve.h here leaked <windows.h> into the whole runtime tree, where it
// promptly collided with the DLL's own type names: <wingdi.h> declares a
// function called `Rectangle` (so Volume::Rectangle would not name a type) and
// <windef.h> still #defines the Win16 `near` and `far` keywords away (so a
// frustum's near/far plane distances would not parse).
//
// These declarations must stay byte-identical to the ones in
// standalone/resolve.h; the two are checked against each other by the linker
// and by standalone/resolve_standalone.cpp, which includes both.
void  RegisterRuntimeExport(const char* mangled_name, void* our_fn);
void  RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn);
void  RegisterRuntimeOrdinal(uint16_t ordinal, void* our_fn);
void* ResolveExportRaw(const char* mangled_name);

// Per-wave kill switch: BANISHED_RT_<NAME>=0 leaves the wave unregistered, so
// every call goes to the DLL exactly as before. Checked once, at init.
inline bool RtWaveEnabled(const char* envName) {
    const char* e = std::getenv(envName);
    return !(e && (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0));
}

// The shipped DLL's implementation of an export, for the diff harness and for
// detour trampolines. NEVER use ResolveExport here: once the port registers,
// that would hand back our own function and the oracle would compare ours to
// ours and always agree.
template <typename Fn>
inline Fn RtOriginal(const char* mangled) {
    return reinterpret_cast<Fn>(ResolveExportRaw(mangled));
}

// ---- Wave init entry points ------------------------------------------------
void RtMathPortInit();     // runtime/src/math/
void RtVolumePortInit();   // runtime/src/volume/
void RtSystemPortInit();   // runtime/src/system/ — Wave 2
void RtObjectPortInit();   // runtime/src/object/ — Wave 3
