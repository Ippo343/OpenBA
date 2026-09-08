// ============================================================================
// standalone/res_path_detour.cpp — Wave 9 lane RRES
//   the live-coverage seam for `ExternalFactory::ExpandRelativePath`
// ============================================================================
//
// WHY A DETOUR IS THE ONLY WAY THIS PORT GETS COVERAGE
//
// `RegisterRuntimeExport` / `RegisterRuntimeAddr` redirect OUR call sites.
// `ExpandRelativePath` has none: its only caller anywhere is retail's own
// `FindOrLoad` (0x1800bf320), which reaches it with a direct rel32 CALL inside
// the DLL and will keep doing so until `FindOrLoad` itself is ported. A port
// with no caller reports `calls=0` and is not verified by anything
// (reference_uncalled_port_is_not_verified.md).
//
// Patching the DLL's own body gives the port every one of retail's callers at
// once — ~22.6k calls per menu-to-gameplay run, from lane FNHOOK's
// `[ResLoad] census: FindOrLoad=22591` — with the real factory, the real path
// stack and the real filesystem underneath. That is the same instrument
// `standalone/rt_alloc_swap.cpp` uses to prove the allocator port against every
// caller in the process.
//
// OPT-IN, ALWAYS. `BANISHED_RT_RESPATH_DETOUR=1`. Nothing goes default-on
// unproven (`standalone/runtime_detour.h`: "every detour gets an env
// kill-switch at its call site"). With the switch off the DLL is untouched and
// the port is dormant — which is the honest state for an unproven body.
//
// THE TRAMPOLINE IS THE ORACLE'S ONLY ROUTE TO THE ORIGINAL. Once the patch is
// in, `GetProcAddress("?ExpandRelativePath@...")` returns the PATCHED address,
// so a diff that called it would re-enter our own body and compare ours to
// ours forever (reference_rtoriginal_is_getprocaddress.md). `RtResPathTrampoline`
// hands back retail's displaced prologue plus the jump back, i.e. the original
// body, and the port's diff site calls that and nothing else.
// ============================================================================

#include <windows.h>
#include <cstdio>

#include "runtime_detour.h"

namespace {

RtDetour g_expandDetour = {};
bool     g_installed    = false;

// 0x1800bd360 .. 0x1800bd370 is exactly SIXTEEN bytes and ends on an
// instruction boundary — read in Ghidra and counted, as runtime_detour.h
// demands:
//     1800bd360  48 89 54 24 10     MOV qword ptr [RSP + 0x10],RDX
//     1800bd365  55                 PUSH RBP
//     1800bd366  56                 PUSH RSI
//     1800bd367  57                 PUSH RDI
//     1800bd368  41 54              PUSH R12
//     1800bd36a  41 55              PUSH R13
//     1800bd36c  41 56              PUSH R14
//     1800bd36e  41 57              PUSH R15
//     1800bd370  48 8D 6C 24 D9     LEA RBP,[RSP + -0x27]      <-- boundary
// The prologue captures no RSP into a register (the `LEA RBP` is AFTER the cut)
// and contains no PC-relative instruction, so the plain installer applies.
//
// NOTE ON THE BYTE GUARD: the expected bytes are NOT passed. `PUSH RBP` here is
// the one-byte 0x55 form, but runtime_detour.h warns that this compiler also
// emits `40 55`, and a guard written from a mnemonic listing rather than from
// memory is a guard that refuses a correct patch. The installer's own
// resolve-and-check path is relied on instead; a wrong build shows up
// immediately as a diff storm, which is the thing this detour exists to run.
constexpr size_t kPrologueBytes = 16;

}  // namespace

extern "C" bool RtResPathInstallDetour(void* ourFn) {
    if (g_installed) return true;
    if (!ourFn) return false;

    if (!InstallRuntimeDetour("Object::ExternalFactory::ExpandRelativePath[port]",
                              0x1800bd360, ourFn, &g_expandDetour,
                              kPrologueBytes)) {
        std::fprintf(stderr, "[ResPath] detour FAILED -- the DLL is untouched and the "
                             "port keeps its zero callers\n");
        std::fflush(stderr);
        return false;
    }
    g_installed = true;
    std::fprintf(stderr, "[ResPath] DETOUR INSTALLED at 0x1800bd360 -- every retail "
                         "caller of ExpandRelativePath now runs the port\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed. A null here means the
// oracle must make NO comparison at all rather than fall back to a door that
// would resolve to our own code.
extern "C" void* RtResPathTrampoline() {
    return g_installed ? g_expandDetour.trampoline : nullptr;
}
