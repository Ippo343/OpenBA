// ============================================================================
// tests/unit/rt_port_stub.cpp — the runtime port registry, stubbed for the host
// ============================================================================
//
// The ported Runtime sources under test still carry their `*Register()`
// functions, which call into the port registry (standalone/resolve_standalone.cpp).
// That file drags in <windows.h>, the address registry, the data mint and the
// whole boot path — none of which a host unit test should need to link
// (tests/unit/README.md, B31_RULES rule 6).
//
// These three definitions are the entire dependency. They are deliberately
// inert: registering is a no-op, and `ResolveExport` answers null so that every
// resolve-or-fall-back site in runtime/src/ takes its DIRECT-CALL fallback —
// which is the port body, i.e. exactly the code under test.
//
// SHARED BETWEEN B.3.1 CORE BATCHES. Every batch that links a runtime/src file
// needs these same three symbols, so this file is deliberately family-neutral
// and lives under the name any batch would reach for. If two batches both
// create it, git reports one add/add conflict and the resolution is "keep one
// copy" — not two files, which would collide at link time. (`weak` is not used:
// GCC's weak definitions are not dependable in PE-COFF, and the linker here did
// not resolve against them at all.)
// ============================================================================

// Signatures must stay byte-identical to runtime/funcs/rt_port.h (C++ linkage,
// not extern "C" — that header declares them unadorned).
#include <cstdint>

void RegisterRuntimeExport(const char* /*mangled_name*/, void* /*our_fn*/) {}
void RegisterRuntimeAddr(uintptr_t /*ghidra_addr*/, void* /*our_fn*/) {}
void* ResolveExport(const char* /*mangled_name*/) { return nullptr; }

// ---------------------------------------------------------------------------
// ResolveAddr — the by-address door, stubbed the same way (ENDGAME B.3.2)
// ---------------------------------------------------------------------------
// Runtime sources that reach a sibling body BY ADDRESS (runtime/src/collection
// and the Object/Video files around it) declare this locally rather than
// including standalone/resolve.h, for the reason that header's users all give:
// it drags in <windows.h>. The declaration below must stay byte-identical to
// theirs; the linker checks the two.
//
// It answers NULL by default, so every resolve-or-fall-back site takes its
// direct-call fallback. A test that needs one specific address answered
// installs `g_unitResolveAddr` for the duration — tests/unit/array_test.cpp
// does that for `kSpec_PkgEntry`'s two element operations, which are the only
// way to reach 0x1800298f0's shrink arm at all.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };

uintptr_t (*g_unitResolveAddr)(int module, uintptr_t ghidraAddr) = nullptr;

uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr) {
    if (g_unitResolveAddr)
        return g_unitResolveAddr(static_cast<int>(mod), ghidra_addr);
    return 0;
}
