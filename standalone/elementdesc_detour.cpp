// ============================================================================
// standalone/elementdesc_detour.cpp — Wave 10 lane EXTPTR
//   the detour that makes `?Serialize@ElementDescription@UI@@` REACHABLE.
// ============================================================================
//
// WHY. `RegisterRuntimeExport` / `RegisterRuntimeAddr` redirect OUR call sites
// and nothing else; the DLL's internal graph is direct rel32 CALLs that never
// touch the export table. For this function that is the difference between a
// port and a dead port, and the numbers say so:
//
//   * 21 rel32 CALLs to 0x180116b00 inside Runtime-x64.dll, plus the
//     `??_7ElementDescription@UI@@6B@` slot at 0x1801561e8 (+0x28), which is
//     how every DLL-built description reaches it.
//   * exactly TWO call sites in our whole tree
//     (`code/Game/Component/captureboxdescription.cpp` and
//     `frustumoutlinedescription.cpp`, both through the exe import slot
//     0x1401518c8), and NEITHER fires on an autostart boot — measured, not
//     assumed: with the port registered and BANISHED_RT_SAVE_TRACE=1 the
//     emission oracle printed no line for this subject at all, i.e. calls=0
//     ([[reference_uncalled_port_is_not_verified]]).
//
// So without this file the port is registered, correct-looking, and never
// executed. With it, every `.rsc` UIDescription the engine parses runs the
// port — which is the whole blast radius, exercised, on one boot.
//
// PROLOGUE, read out of the shipped DLL rather than off a mnemonic listing
// (this compiler emits redundant REX prefixes, so `PUSH RBX` is `40 53`):
//
//   180116b00  40 53                       PUSH RBX                    (2)
//   180116b02  48 83 EC 40                 SUB RSP,0x40                (4)
//   180116b06  48 C7 44 24 20 FE FF FF FF  MOV qword [RSP+0x20],-2     (9)
//   180116b0f  48 8B D9                    MOV RBX,RCX   <- resumes here
//
// Fifteen bytes, an exact instruction boundary, no PC-relative operand and no
// RSP capture — so the plain relocating installer is enough and nothing has to
// be re-encoded.
//
// OPT-IN, as `runtime_detour.h` requires of every detour ("Nothing goes
// default-on unproven"): `BANISHED_RT_ELEMDESCSER_DETOUR=1`. With it off the
// port keeps its two exe callers and the DLL keeps its own body — which is the
// state every previous wave shipped.
//
// THE ORACLE INTERACTION, which bites hard. `ResolveExportRaw` hands back the
// RAW DLL address — the address this file patches. Once the detour is in, an
// oracle that fetches "theirs" that way jumps straight back into OUR function
// and recurses until the stack dies. The port must call the TRAMPOLINE, and
// `RtUiElementDescSerializeTrampoline()` is that door: null means no detour is
// installed and the raw export IS retail's body, non-null means the raw export
// now points at us.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uintptr_t kElemDescSerializeAddr     = 0x180116b00;
constexpr size_t    kElemDescSerializePrologue = 15;

// The safety belt. `InstallRuntimeDetourChecked` refuses the patch unless these
// bytes are actually there — which catches a different Runtime build, a wrong
// ASLR delta, and, the one that matters most here, another detour already
// sitting on this address.
const uint8_t kElemDescSerializeBytes[kElemDescSerializePrologue] = {
    0x40, 0x53,                                     // PUSH RBX
    0x48, 0x83, 0xEC, 0x40,                         // SUB RSP,0x40
    0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF  // MOV [RSP+0x20],-2
};

RtDetour g_detour = {};
bool     g_installed = false;

}  // namespace

extern "C" bool RtUiElementDescSerializeInstallDetour(void* ourFn) {
    if (g_installed) return true;
    if (!ourFn) return false;

    const char* e = std::getenv("BANISHED_RT_ELEMDESCSER_DETOUR");
    if (!e || std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0)
        return false;   // opt-in; silence is the normal state

    if (!InstallRuntimeDetourChecked("UI::ElementDescription::Serialize[port]",
                                     kElemDescSerializeAddr, ourFn, &g_detour,
                                     kElemDescSerializePrologue,
                                     kElemDescSerializeBytes,
                                     kElemDescSerializePrologue)) {
        std::fprintf(stderr,
            "[ElemDescDetour] detour FAILED -- the DLL is UNTOUCHED and the port keeps "
            "only its two exe callers. A safe failure, not a silent one: nothing was "
            "patched.\n");
        std::fflush(stderr);
        return false;
    }

    g_installed = true;
    std::fprintf(stderr,
        "[ElemDescDetour] DETOUR INSTALLED at 0x180116b00 -- all 21 internal callers "
        "and the ElementDescription vtable slot now run the port, i.e. every .rsc "
        "UIDescription the engine parses.\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed. A caller that ignores
// this and uses the raw export anyway does not get a wrong answer — it gets a
// stack overflow.
extern "C" void* RtUiElementDescSerializeTrampoline() {
    return g_installed ? g_detour.trampoline : nullptr;
}
