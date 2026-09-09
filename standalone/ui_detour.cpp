// ============================================================================
// standalone/ui_detour.cpp — Wave 10: detours into UI::* inside Runtime-x64.dll
// ============================================================================
//
// WHY A UI DETOUR AT ALL. `RegisterRuntimeExport` redirects OUR call sites and
// nothing else: the DLL's internal call graph is direct rel32 CALLs that never
// touch the export table. For a leaf accessor that is fine. For a MUTATOR it is
// a split brain — two implementations of one function writing one object — and
// Wave 7 lane C stopped the whole `UserInterface` frame family at exactly that
// line: "The day someone ports `Update` or `AddDialog`, the DLL's `MainLoop`
// will still be calling the DLL's `Update` on an object our mutator reshaped,
// and that is exactly the question the detour exists to answer."
//
// A CORRECTION TO THAT DOCUMENT'S CLOSING CLAIM, since it is load-bearing for
// anyone reading it as a plan: WAVE7.md says "the detour was again not needed
// and the count is still 0 across all 4,632 functions". **That is stale.** The
// mechanism exists (standalone/runtime_detour.{h,cpp}, a relocating installer
// with an HDE64 prologue decoder) and the census counts three live detours
// today — System 2, Audio 1. Nobody has to build one; this file installs one.
//
// FIRST UI SUBJECT: ?UpdateContainers@TabElement@UI@@AEAAXXZ (0x1800d9710),
// ported in this same wave against the predictive shadow oracle
// (runtime/funcs/rt_predict.h). It is the right first subject for three
// reasons, in ascending order of importance:
//
//   1. Its prologue is ideal. Twelve bytes land exactly on an instruction
//      boundary with no PC-relative operand and no RSP capture:
//        1800d9710  48 89 5C 24 10   MOV [RSP+0x10],RBX   (5)
//        1800d9715  57               PUSH RDI             (1)
//        1800d9716  48 83 EC 30      SUB RSP,0x30         (4)
//        1800d971a  33 FF            XOR EDI,EDI          (2)   = 12
//        1800d971c  48 8B D9         MOV RBX,RCX          <- resumes here
//      Bytes read out of the shipped DLL, not off a mnemonic listing (this
//      compiler emits redundant REX prefixes, so a listing is not the bytes).
//
//   2. It closes a split brain THIS WAVE CREATED. Porting UpdateContainers gave
//      the process two copies of a function that writes Element flag words and
//      walks parent chains. Ours ran for our `SetActiveTab`; the DLL's ran for
//      its own callers. Leaving that is precisely what lane C refused to do.
//
//   3. It is the only way a real tab CLICK reaches the port. Tab input is
//      handled inside the DLL, so without a detour the port is reachable only
//      from the three exe call sites — which is why the port's oracle route had
//      to be driven by BANISHED_OPTIONS_TAB instead of by clicking anything.
//
// OPT-IN, like every other detour in this tree. `runtime_detour.h`: "Every
// detour gets an env kill-switch at its call site. Nothing goes default-on
// unproven." With it off, the port keeps its three exe callers and the DLL
// keeps its own.
//
// THE ORACLE INTERACTION, which is not obvious and bites hard.
// `RT_ORIGINAL_ONCE` resolves through `ResolveExportRaw`, i.e. the RAW DLL
// address — which is the address this file patches. Once the detour is in, an
// oracle that calls the original that way JUMPS STRAIGHT BACK INTO OUR OWN
// FUNCTION and recurses until the stack dies. So the port must call the
// TRAMPOLINE instead, and `RtUiUpdateContainersTrampoline()` below is that
// door. It returns null when no detour is installed, and a null there means the
// caller must fall back to the raw export — never the other way round.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdlib>

namespace {

// See the prologue table in the banner. Twelve is both the installer's minimum
// (the size of the absolute jump) and an exact instruction boundary here, which
// is the coincidence that makes this a clean first subject rather than a
// relocation exercise.
constexpr size_t   kUpdateContainersPrologue = 12;
constexpr uintptr_t kUpdateContainersAddr    = 0x1800d9710;

// The safety belt. `InstallRuntimeDetourChecked` refuses the patch unless these
// bytes are actually there, which catches a different Runtime build, a wrong
// ASLR delta, and — the one that matters most — another detour already sitting
// on this address.
const uint8_t kUpdateContainersBytes[kUpdateContainersPrologue] = {
    0x48, 0x89, 0x5C, 0x24, 0x10,   // MOV [RSP+0x10],RBX
    0x57,                           // PUSH RDI
    0x48, 0x83, 0xEC, 0x30,         // SUB RSP,0x30
    0x33, 0xFF                      // XOR EDI,EDI
};

RtDetour g_updContainersDetour = {};
bool     g_updContainersInstalled = false;

} // namespace

extern "C" bool RtUiUpdateContainersInstallDetour(void* ourFn) {
    if (g_updContainersInstalled) return true;
    if (!ourFn) return false;

    if (!InstallRuntimeDetourChecked("UI::TabElement::UpdateContainers[port]",
                                     kUpdateContainersAddr, ourFn,
                                     &g_updContainersDetour,
                                     kUpdateContainersPrologue,
                                     kUpdateContainersBytes,
                                     kUpdateContainersPrologue)) {
        std::fprintf(stderr,
            "[UiDetour] UpdateContainers detour FAILED -- the DLL is UNTOUCHED and "
            "the port keeps only its three exe callers. This is a safe failure, not "
            "a silent one: nothing was patched.\n");
        std::fflush(stderr);
        return false;
    }

    g_updContainersInstalled = true;
    std::fprintf(stderr,
        "[UiDetour] DETOUR INSTALLED at 0x1800d9710 -- every retail caller of "
        "TabElement::UpdateContainers (including a real tab CLICK, which is handled "
        "inside the DLL) now runs the port. Split brain closed.\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed.
//
// A null here means "no detour, so the raw export IS retail's body and is safe
// to call". A NON-null means the raw export now jumps to us, and calling it
// would recurse -- so a caller that ignores this and uses RT_ORIGINAL_ONCE
// anyway does not get a wrong answer, it gets a stack overflow.
extern "C" void* RtUiUpdateContainersTrampoline() {
    return g_updContainersInstalled ? g_updContainersDetour.trampoline : nullptr;
}
