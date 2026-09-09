// ===========================================================================
// standalone/ui_hover_detour.cpp — Wave 10D lane MOUSE
// ===========================================================================
// The second half of the hover route. `standalone/mouse_inject.cpp` makes the
// game genuinely hover; this file makes the game's OWN hover reach the three
// ported bodies.
//
// WHY IT IS NEEDED, MEASURED RATHER THAN ASSUMED
// ----------------------------------------------
// `EndDragDrop`, `EndHover` and `BeginToolTip` have exactly one caller each,
// and it is `UI::Element::Update` (0x180116270). That body IS ported
// (`runtime/src/ui/element_vtable.cpp`, `RtUi_Element_Update`) and IS
// registered under both doors -- but it is reached through the ELEMENT'S
// VTABLE, from the DLL's own `DialogInstance::Update`, and a vtable dispatch
// inside the DLL never goes through the export table. So registration
// redirects our call sites and nothing else, and the port simply does not run:
//
//     [Element] ATTENDANCE: neither Update nor Persist was entered in this run.
//
// printed by element_vtable.cpp's own attendance counter in EVERY in-game cell
// this lane ran, including the one where the injector had demonstrably opened a
// real tooltip (`S5 tipShown=89`). That line is the evidence; it is not an
// inference from entries=0.
//
// WHY A DETOUR IS THE RIGHT ANSWER **NOW**, WHEN IT WAS NOT BEFORE
// ---------------------------------------------------------------
// WAVE10C_LANE_UIF6.md §8 considered a detour on these three and rejected it:
//
//     "it would change *who* reaches the body, not whether the game hovers, so
//      it adds DLL-prologue-patching risk and buys no coverage."
//
// THAT REASONING WAS CORRECT AND IS NOW STALE, and the thing that made it stale
// is the injector, not an argument. With no mouse, a detour really would have
// bought nothing: the DLL's `Element::Update` never took its tooltip or drop
// arm either, so the detoured bodies would have sat at entries=0 exactly as the
// registered ones did. With a mouse, the DLL's `Element::Update` takes those
// arms for real -- `_hovering` set after a genuine 0.25 s dwell, tooltip bit 20
// set, a real drag ended -- and the detour is then not a substitute for
// coverage, it is the delivery mechanism for coverage that now exists.
// (reference_holdback_blockers_are_stale_check_them_first.md: the blocker was
// re-checked rather than inherited, and half of it turned out to be true.)
//
// FULL REPLACEMENT, NOT A WRAPPER
// -------------------------------
// Each detour points straight at the ported body -- there is no wrapper and the
// trampoline is never called. Two consequences worth stating:
//   * The MS x64 argument registers arrive untouched, and each ported body's
//     signature already matches the export's (both InternalPtr<> parameters of
//     BeginToolTip are by hidden reference, which is how element_vtable.cpp:747
//     and userinterface_hover.cpp already spell them).
//   * `BeginToolTip`'s prologue opens with `MOV RAX,RSP`, an RSP capture. That
//     is the Bug Class 3 hazard `InstallDetour` deliberately does NOT adjust --
//     and here it cannot matter at all, because nothing ever enters the
//     trampoline.
//   * None of the three calls `RT_ORIGINAL_ONCE` on ITSELF, so none can recurse
//     into the address we just patched
//     (reference_detour_machinery_exists_wave7_zero_is_stale.md). Checked, not
//     assumed: `userinterface_hover.cpp` reaches only `MemFree`, `CloseDialog`,
//     `GetToolTipString`, `SetText`, the `ToolTipDialog` operator new / vtable
//     and `Array<Event>::SetSize` that way.
//
// OPT-IN, NEVER DEFAULT-ON
// ------------------------
// `BANISHED_RT_UIHOVER_DETOUR=1`. Like `BANISHED_RT_UIFRAME_DETOUR`, this is an
// opt-in and NOT a kill switch: `RtWaveEnabled` is default-ON and that is the
// wrong default for a code patch into the shipped DLL. Unset, this file writes
// nothing, patches nothing and prints nothing.
//
// It is also deliberately INDEPENDENT of the injector's script: a later lane
// that reaches these bodies some other way can set this alone.
// ===========================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "runtime_detour.h"
#include "ui_hover_detour.h"

// The ported bodies, exactly as runtime/src/ui/userinterface_hover.cpp defines
// them. Declared here rather than pulled through a header so this file has no
// build-order dependency on the runtime tree.
extern "C" void RtUi_UserInterface_EndDragDrop(void* self);
extern "C" void RtUi_DialogInstance_EndHover(void* self, void* ui);
extern "C" void RtUi_UserInterface_BeginToolTip(void* self, void** dlgPtr,
                                                void** elemPtr, int32_t x, int32_t y);

namespace {

// Prologue guards. Every byte was read out of the shipped image with objdump,
// not from a mnemonic listing -- this compiler emits redundant REX prefixes
// (`PUSH RBX` is `40 53`, not `53`), and a listing would have produced a guard
// that never matches.
//
// Each length ends on an instruction boundary at or past 12 bytes; the counts
// are spelled out so the arithmetic can be re-checked without re-disassembling.

// 0x180120af0  ?EndDragDrop@UserInterface@UI@@QEAAXXZ
//   48 89 5c 24 08     mov [rsp+8],rbx     (5)
//   57                 push rdi            (1)  -> 6
//   48 83 ec 50        sub rsp,0x50        (4)  -> 10
//   48 8b 41 60        mov rax,[rcx+0x60]  (4)  -> 14
const uint8_t kEndDragDropPro[] = {
    0x48,0x89,0x5c,0x24,0x08, 0x57, 0x48,0x83,0xec,0x50, 0x48,0x8b,0x41,0x60 };

// 0x180113d10  ?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z
//   40 53              rex push rbx            (2)
//   48 83 ec 50        sub rsp,0x50            (4)  -> 6
//   80 b9 98 ...  00   cmp byte [rcx+0x98],0   (7)  -> 13
const uint8_t kEndHoverPro[] = {
    0x40,0x53, 0x48,0x83,0xec,0x50, 0x80,0xb9,0x98,0x00,0x00,0x00,0x00 };

// 0x180120860  ?BeginToolTip@UserInterface@UI@@...
//   48 8b c4           mov rax,rsp   (3)   <- the RSP capture; harmless here
//   55 / 57            push rbp/rdi  (2)   -> 5
//   41 54 / 41 56 / 41 57  push r12/r14/r15 (6) -> 11
//   48 8b ec           mov rbp,rsp   (3)   -> 14
//   48 83 ec 60        sub rsp,0x60  (4)   -> 18
// Stopping at 18 leaves the `mov [rbp-0x20],-2` SEH scope-index store in place.
const uint8_t kBeginToolTipPro[] = {
    0x48,0x8b,0xc4, 0x55, 0x57, 0x41,0x54, 0x41,0x56, 0x41,0x57,
    0x48,0x8b,0xec, 0x48,0x83,0xec,0x60 };

RtDetour g_endDrag, g_endHover, g_beginTip;
bool     g_done = false;

bool OptedIn() {
    const char* e = std::getenv("BANISHED_RT_UIHOVER_DETOUR");
    return e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

}  // namespace

extern "C" void RtUiHoverInstallDetours() {
    if (g_done) return;
    g_done = true;
    if (!OptedIn()) return;   // silent, and nothing is patched

    // Note the UN-resolved GHIDRA addresses. InstallRuntimeDetourChecked
    // resolves them Raw on its own side of the boundary, which is the whole
    // point of that signature -- a caller that resolved them with
    // ResolveAddr(Module::Runtime, ...) would aim the patch at OUR OWN prologue,
    // because these three addresses ARE registered in the port registry
    // (reference_a_rule_on_the_callers_side_gets_broken.md,
    //  reference_resolveaddr_consults_port_registry_first.md).
    const bool a = InstallRuntimeDetourChecked(
        "UI::UserInterface::EndDragDrop", 0x180120af0,
        (void*)&RtUi_UserInterface_EndDragDrop, &g_endDrag,
        sizeof(kEndDragDropPro), kEndDragDropPro, sizeof(kEndDragDropPro));
    const bool b = InstallRuntimeDetourChecked(
        "UI::DialogInstance::EndHover", 0x180113d10,
        (void*)&RtUi_DialogInstance_EndHover, &g_endHover,
        sizeof(kEndHoverPro), kEndHoverPro, sizeof(kEndHoverPro));
    const bool c = InstallRuntimeDetourChecked(
        "UI::UserInterface::BeginToolTip", 0x180120860,
        (void*)&RtUi_UserInterface_BeginToolTip, &g_beginTip,
        sizeof(kBeginToolTipPro), kBeginToolTipPro, sizeof(kBeginToolTipPro));

    // Report each SEPARATELY. A combined "3 detours installed" line cannot say
    // which one refused, and a refused install leaves the DLL untouched and the
    // corresponding entry counter at 0 -- which reads exactly like "the port is
    // dead" rather than "the patch never went in".
    printf("[UIHoverDetour] EndDragDrop=%d EndHover=%d BeginToolTip=%d "
           "(opt-in BANISHED_RT_UIHOVER_DETOUR)\n", (int)a, (int)b, (int)c);
    fflush(stdout);
}
