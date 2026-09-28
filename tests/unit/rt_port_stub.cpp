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
// These two definitions are the entire dependency, and they are deliberately
// inert: registering is a no-op. B3R RT-Core: `ResolveExport`, `ResolveAddr`
// and the `g_unitResolveAddr` hook are gone -- no source linked into
// `make unit` reads the registry any more (array_test.cpp's kSpec_PkgEntry
// case, the hook's last user, now runs the real element bodies).
//
// SHARED BETWEEN B.3.1 CORE BATCHES. Every batch that links a runtime/src file
// needs these same two symbols, so this file is deliberately family-neutral
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
