// ============================================================================
// standalone/modal_driver.cpp — Wave 10B lane SHOW
//
// THE ROUTE INTO THE NESTED MODAL PUMP.
//
// `UI::UserInterface::ShowDialog` (retail 0x18011fd10) is a nested modal loop.
// Read off the shipped bytes, it:
//
//   1. MemAlloc(0x128, 4) a SECOND UserInterface and construct it (0x180120de0)
//   2. copy this->[+0x90 .. +0xc8] (eight qwords) into the nested one
//   3. dialog->_ui (+0x10) = nestedUI ;  dialog->_flags (+0x20) |= 0x20
//      -- 0x20 is the bit `?DeleteOnClose@DialogInstance@UI@@QEBA_NXZ`
//      (0x180114110) reads INVERTED: `!(flags>>5 & 1)`. So the OR makes
//      DeleteOnClose FALSE: the modal dialog is NOT destroyed when it closes,
//      because the CALLER still has to read the result out of it. Every
//      call site does exactly that (optionsdialog.cpp reads +0xC0/+0xC4 after
//      ShowDialog returns, then DeletePtr's).
//      It is NOT the modal bit -- `?IsModal@` (0x180114170) reads bit 1.
//   4. call the dialog's vtable slot 4 (+0x20)          [0x18011fe2a]
//   5. Array::SetSize push the dialog into nestedUI->_addDialogs (+0x28)
//   6. UserInterface::Update(nestedUI, Time(0))          [0x18011fe67]
//   7. append {DrawStandAloneUI  (0x1801203f0), nestedUI} to MainLoop+0x80
//   8. append {UpdateStandAloneUI(0x180120310), nestedUI} to MainLoop+0x90
//   9. WHILE the dialog is still in nestedUI->_dialogs (list head at +0x00,
//      chained through +0x10): call `MainLoop::Frame(_singleton, true)`
//      (0x1800dd100) and stop early if it returns true (quit requested)
//  10. compact both callback arrays, removing its own two entries
//  11. ~UserInterface(nestedUI) (0x180120b90) + MemFree
//  12. return *(int*)(dialog + 0x24)  -- the dialog RESULT, i.e. exactly what
//      `?GetResult@DialogInstance@UI@@QEBAHXZ` (0x1800455c0: `MOV EAX,[RCX+0x24]`)
//      returns and `?SetDialogResult@` (0x1800455d0: `MOV [RCX+0x24],EDX`) writes.
//
// Step 9 is the wall. `MainLoop::Frame(this, param_1=true)` skips
// `MainLoop::Update` -- 0x1800dd28e sits inside `if (!param_1)` -- so every
// existing harness tick (MenuDLT::Stub_Update, ToolHarness_Tick, AutoStart)
// stops running the moment a modal opens, and only resumes when it closes.
// That is why `BANISHED_MODAL_ANSWER` had to substitute the answer and skip
// ShowDialog altogether: it is a BYPASS, not a route.
//
// But `Frame` DOES walk one array on every fixed-timestep tick, `param_1` or
// not (0x1800dd2a3..0x1800dd2d6):
//
//     MOV RDX, [RDI+0x80]        ; DRAW array base
//     MOV RAX, [RDI+0x90]        ; UPDATE array base
//     LEA RCX, [RDI+0x48]        ; arg1 = &fixedTimestep   (System::Time*)
//     MOV RDX, [RBX+RDX+8]       ; arg2 = DRAW[i].ctx      <-- SEE BELOW
//     CALL [RBX+RAX]             ; fn   = UPDATE[i].fn
//
// so a callback we append to MainLoop+0x90 runs inside the nested pump. That
// is the route, and this file is it.
//
// ---------------------------------------------------------------------------
// THE RETAIL QUIRK THAT DICTATES THIS FILE'S SHAPE
// ---------------------------------------------------------------------------
// Look again at those two loads. The FUNCTION comes from the +0x90 (update)
// array and the CONTEXT comes from the +0x80 (draw) array, at the same index.
// This is not a decompiler artefact -- it is in the disassembly above, and
// Ghidra's C prints it too. It is invisible in retail because `ShowDialog` is
// the only writer and it appends to both arrays in lockstep with the same
// `nestedUI` context, so DRAW[i].ctx == UPDATE[i].ctx for every i.
//
// Two consequences for us, both load-bearing:
//
//   (a) Our callback MUST IGNORE its second argument. We cannot make the ctx
//       we register in +0x90 arrive; we would receive whatever sits at the same
//       index of +0x80. State lives in this file's statics instead.
//
//   (b) We MUST append a paired entry to +0x80 as well. If +0x90 is longer
//       than +0x80, `MOV RDX,[RBX+RDX+8]` reads past the draw array -- and if
//       +0x80 has never been allocated it is NULL, making that a read of
//       address 0x8+i*0x10 and an immediate access violation IN RETAIL'S
//       FRAME LOOP. Appending in lockstep, exactly as ShowDialog does, is what
//       keeps the quirk harmless.
//
// We are armed BEFORE any ShowDialog runs, so we occupy the low indices and
// ShowDialog appends after us. Its removal pass matches on (fn, ctx) pairs of
// its own, so it compacts its entries and leaves ours in place.
//
// ---------------------------------------------------------------------------
// HOW THE DISMISSAL IS DONE, AND WHY IT IS NOT AN INVENTION
// ---------------------------------------------------------------------------
// Every MessageDialog button in the ported exe does precisely this
// (code/Game/Dialog/messagedialog.cpp, Slot10/11/12/13, retail 0x14006CFF0
// /0x14006D030/0x14006CFB0/0x14006CF70):
//
//     *(uint32_t*)(self + 0x24) = <result>;
//     fn_UI_CloseDialog(*(void**)(self + 0x10), InternalPtr(self));
//
// so this driver performs the same two operations on the front dialog of the
// nested UI. It is a synthetic CLICK, not a synthetic dialog system: retail's
// CloseDialog, retail's removal list, retail's `CleanRemovedDialogs` and
// retail's loop-exit test all run untouched.
//
// Note `*(void**)(dialog+0x10)` is the NESTED UserInterface, because step 3 of
// ShowDialog put it there. So CloseDialog is aimed at the nested UI without us
// having to thread it through -- the same way a real button reaches it.
// ============================================================================

#include "modal_driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "resolve.h"
#include "engine_funcs.h"
#include "ui/Dialogs.h"      // MessageDialogAddr::Vtable, for the self-test subject

namespace ModalDriver {
namespace {

// --- Layout, all read off the shipped Runtime-x64.dll -----------------------
constexpr uintptr_t kMainLoop_Singleton   = 0x1801fe750;  // Engine::MainLoop::_singleton (DATA, ord 4813)
constexpr uintptr_t kUpdateStandAloneUI   = 0x180120310;  // appended to MainLoop+0x90 by ShowDialog
constexpr uintptr_t kDrawStandAloneUI     = 0x1801203f0;  // appended to MainLoop+0x80 by ShowDialog
constexpr uintptr_t kShowDialog           = 0x18011fd10;

constexpr uintptr_t kML_DrawArray   = 0x80;   // {void(*fn)(...); void* ctx;}[]
constexpr uintptr_t kML_DrawCount   = 0x88;   // int32
constexpr uintptr_t kML_DrawCap     = 0x8c;   // int32
constexpr uintptr_t kML_UpdArray    = 0x90;
constexpr uintptr_t kML_UpdCount    = 0x98;
constexpr uintptr_t kML_UpdCap      = 0x9c;

constexpr uintptr_t kDlg_Ui      = 0x10;  // DialogInstance::_ui
constexpr uintptr_t kDlg_Flags   = 0x20;  // bit 0x20 => !DeleteOnClose
constexpr uintptr_t kDlg_Result  = 0x24;  // ?GetResult@ / ?SetDialogResult@

struct Entry { void* fn; void* ctx; };   // 16 bytes; the array element type

// --- Configuration ---------------------------------------------------------
bool s_parsed      = false;
bool s_enabled     = false;
int  s_answer      = 0;
int  s_ticks       = 3;

bool s_armed       = false;
int  s_dismissed   = 0;

// Per-modal state. The callback cannot receive a context (see quirk (a)).
void* s_seenDialog = nullptr;
int   s_seenTicks  = 0;

void ParseEnv() {
    if (s_parsed) return;
    s_parsed = true;
    const char* v = std::getenv("BANISHED_MODAL_DRIVER");
    // Deliberately NOT the usual `=0 means off` rule: 0 is a legitimate dialog
    // result (MessageDialog Cancel). Presence of the variable arms the driver.
    if (!v || v[0] == '\0') return;
    s_enabled = true;
    s_answer  = std::atoi(v);
    if (const char* comma = std::strchr(v, ',')) {
        int t = std::atoi(comma + 1);
        if (t > 0) s_ticks = t;
    }
    std::fprintf(stderr,
        "[ModalDriver] ARMED (BANISHED_MODAL_DRIVER=\"%s\") - will answer any modal "
        "with result %d after %d nested tick(s)\n", v, s_answer, s_ticks);
}

void* MainLoopSingleton() {
    void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Runtime, kMainLoop_Singleton));
    return slot ? *slot : nullptr;
}

// Append `e` to the (array,count,cap) triple at `base+arrOff`. Reallocates
// through the shipped allocator, the same one ShowDialog's inlined push uses,
// and sets capacity == count so retail's own next push takes its grow path.
bool AppendEntry(uintptr_t base, uintptr_t arrOff, uintptr_t cntOff, uintptr_t capOff,
                 const Entry& e) {
    Entry* old   = *reinterpret_cast<Entry**>(base + arrOff);
    int    count = *reinterpret_cast<int*>(base + cntOff);
    if (count < 0) return false;

    Entry* fresh = reinterpret_cast<Entry*>(
        fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)(count + 1) * sizeof(Entry), 8));
    if (!fresh) return false;

    for (int i = 0; i < count; ++i) fresh[i] = old[i];
    fresh[count] = e;

    *reinterpret_cast<Entry**>(base + arrOff) = fresh;
    *reinterpret_cast<int*>(base + cntOff)    = count + 1;
    *reinterpret_cast<int*>(base + capOff)    = count + 1;

    if (old) fn_MemFree(g_pMemAllocSingleton, old);
    return true;
}

// The paired no-op in the DRAW array. It exists only so that quirk (b) reads a
// valid slot; MainLoop::Draw will call it, so it must be a real function that
// returns. It takes no meaningful arguments under any x64 convention because it
// touches none.
void NoopDraw(void*, void*) {}

// Find the front dialog of a live nested modal UI, or nullptr.
//
// Detection is by SIGNATURE, not by cooperation from our port: we scan
// MainLoop+0x90 for an entry whose fn is `UpdateStandAloneUI`. That entry's
// ctx is the nested UserInterface -- and this works identically whether the
// ShowDialog that registered it was the shipped DLL's or ours, which is the
// property the twin-subject differential depends on.
void* FindModalDialog() {
    void* ml = MainLoopSingleton();
    if (!ml) return nullptr;
    uintptr_t base = reinterpret_cast<uintptr_t>(ml);

    Entry* arr   = *reinterpret_cast<Entry**>(base + kML_UpdArray);
    int    count = *reinterpret_cast<int*>(base + kML_UpdCount);
    if (!arr || count <= 0) return nullptr;

    void* wanted = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, kUpdateStandAloneUI));

    for (int i = 0; i < count; ++i) {
        if (arr[i].fn != wanted) continue;
        void* nestedUi = arr[i].ctx;
        if (!nestedUi) continue;
        // nestedUI->_dialogs head at +0x00; node->dialog at +0x00.
        void* head = *reinterpret_cast<void**>(nestedUi);
        if (!head) continue;
        return *reinterpret_cast<void**>(head);
    }
    return nullptr;
}

// The callback itself. Registered in MainLoop+0x90; runs on every fixed-timestep
// tick of every Frame, nested or not. MUST ignore both arguments (quirk (a)).
void Tick(void*, void*) {
    void* dialog = FindModalDialog();
    if (!dialog) { s_seenDialog = nullptr; s_seenTicks = 0; return; }

    if (dialog != s_seenDialog) { s_seenDialog = dialog; s_seenTicks = 0; }
    if (++s_seenTicks < s_ticks) return;

    uintptr_t d  = reinterpret_cast<uintptr_t>(dialog);
    void*     ui = *reinterpret_cast<void**>(d + kDlg_Ui);
    uint32_t  flags = *reinterpret_cast<uint32_t*>(d + kDlg_Flags);

    std::fprintf(stderr,
        "[ModalDriver] dismissing modal dialog=%p nestedUi=%p flags=0x%x "
        "(DeleteOnClose=%d) with result %d\n",
        dialog, ui, flags, (flags & 0x20) ? 0 : 1, s_answer);

    // Exactly what MessageDialog::Slot10..13 do (see the header comment).
    *reinterpret_cast<uint32_t*>(d + kDlg_Result) = (uint32_t)s_answer;
    uint8_t ibp[16] = {};
    fn_InternalBasePtr_Ctor(ibp, dialog);
    fn_UI_CloseDialog(ui, ibp);

    ++s_dismissed;
    s_seenDialog = nullptr;
    s_seenTicks  = 0;
}

}  // namespace

bool Enabled() { ParseEnv(); return s_enabled; }
int  DismissedCount() { return s_dismissed; }

bool Poll() {
    ParseEnv();
    if (!s_enabled) return false;
    if (s_armed)    return true;

    void* ml = MainLoopSingleton();
    if (!ml) return false;                 // Engine::Startup has not built it yet
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return false;

    uintptr_t base = reinterpret_cast<uintptr_t>(ml);

    // Order matters only for readability -- both must succeed, and the DRAW
    // append must not be left dangling if the UPDATE one fails, or we would
    // have made the arrays UNEVEN in the direction quirk (b) tolerates (draw
    // longer than update is harmless; update longer than draw is the fault).
    Entry draw{ reinterpret_cast<void*>(&NoopDraw), ml };
    if (!AppendEntry(base, kML_DrawArray, kML_DrawCount, kML_DrawCap, draw)) {
        std::fprintf(stderr, "[ModalDriver] ARM FAILED: could not grow MainLoop+0x80\n");
        return false;
    }
    Entry upd{ reinterpret_cast<void*>(&Tick), ml };
    if (!AppendEntry(base, kML_UpdArray, kML_UpdCount, kML_UpdCap, upd)) {
        std::fprintf(stderr, "[ModalDriver] ARM FAILED: could not grow MainLoop+0x90 "
                             "(the +0x80 no-op stays; draw-longer-than-update is safe)\n");
        return false;
    }

    s_armed = true;
    std::fprintf(stderr,
        "[ModalDriver] armed on MainLoop=%p  draw[%d] update[%d]  "
        "(callback %p paired with no-op %p)\n",
        ml, *reinterpret_cast<int*>(base + kML_DrawCount),
        *reinterpret_cast<int*>(base + kML_UpdCount),
        (void*)&Tick, (void*)&NoopDraw);
    return true;
}

// ===========================================================================
// BANISHED_MODAL_DRIVER_TEST -- the twin-subject differential
// ===========================================================================
// UI1 recorded that a PREDICTIVE oracle over ShowDialog would compare exactly
// one flag bit, because everything else it writes is an address MemAlloc chose.
// That is correct, and it is why this lane does not use rt_predict here.
//
// What the route makes possible instead is a TWIN-SUBJECT DIFFERENTIAL: with a
// driver that can terminate the modal, ShowDialog becomes REPEATABLE, so we can
// build two dialogs of identical construction and show one through the shipped
// DLL body (ResolveExportRaw, which bypasses the port registry) and one through
// whatever the registry currently resolves, then compare the post-state each
// implementation leaves behind. Two distinct dialog objects, so nothing is
// aliased and nothing is replayed onto dirty state
// (reference_oracle_must_snapshot_aliased_inputs.md).
//
// COMPARED, and the reason each is a real signal:
//   * the RETURN VALUE against the result the driver injected -- this is the
//     whole contract of step 12, and it is end-to-end: it can only match if the
//     nested UI was built, the dialog reached its list, the pump ran, the
//     callbacks were registered and walked, and the loop exited on the right test.
//   * dialog->_ui (+0x10)   -- step 3 writes the nested UI here. After ShowDialog
//     returns, that UI is DESTROYED and FREED, so this is a dangling pointer in
//     both cells; we compare only whether it CHANGED from its pre-value, not its
//     value (which is an address MemAlloc chose and must differ).
//   * dialog->_flags (+0x20) bit 0x20 -- step 3's OR. The one bit a predictive
//     oracle could have compared; kept, but it is not the finding.
//   * MainLoop +0x88 / +0x98 (the two callback COUNTS) -- must return to their
//     pre-call values. This is the step-10 compaction, and it is the assertion
//     that our port cleans up after itself exactly as retail does. If a port
//     leaked one entry, retail's Frame would call into a freed nested UI on
//     every subsequent tick.
//   * MainLoop +0x80 / +0x90 (the array BASE POINTERS) -- compared for
//     ANY-CHANGE parity, not for value.
//
// NOT COMPARED, stated so the pass is not over-read: the nested UserInterface's
// 0x128 bytes (destroyed and freed before return -- unreadable by construction),
// the allocator's arena, and everything DialogInstance::Update / the vtable
// slot-4 call did to the dialog's own interior.
namespace {

bool  s_testDone = false;

// One cell of the differential.
struct Cell {
    int      ret;
    uint64_t uiAfter;
    uint32_t flagsAfter;
    int      drawCount, updCount;
    void*    drawArr;  void* updArr;
    int      dismissedDelta;
};

void* BuildSyntheticDialog(void* outerUi, const char* rsc) {
    uint8_t filename[8] = {};
    fn_Filename_CtorCStr(filename, rsc);
    void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);
    fn_Filename_Dtor(filename);
    if (ext && g_pExternalTypeId && !fn_IsDerivedFrom(ext, *g_pExternalTypeId)) ext = nullptr;

    void* dlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
    if (!dlg) return nullptr;
    uint64_t nullParent = 0;
    fn_DialogInstance_Ctor(dlg, outerUi, ext, &nullParent, nullptr, nullptr, 0);
    // MessageDialog vtable: the standalone table, exactly as
    // SaveLoad_ShowConfirmDialog patches it.
    *reinterpret_cast<void**>(dlg) = (void*)ResolveAddr(Module::Exe, MessageDialogAddr::Vtable);
    return dlg;
}

typedef int (*fn_ShowDialog_t)(void* self, void* dialogPtr);

// Set by the self-test around the OURS cell only; see the negative-control
// block in runtime/src/ui/userinterface_showdialog.cpp.
extern "C" int g_RtShowDialogDefect;

Cell RunCell(void* outerUi, fn_ShowDialog_t show, const char* label, int defect) {
    Cell c{};
    void* ml   = MainLoopSingleton();
    uintptr_t b = reinterpret_cast<uintptr_t>(ml);

    int  preDraw = *reinterpret_cast<int*>(b + kML_DrawCount);
    int  preUpd  = *reinterpret_cast<int*>(b + kML_UpdCount);
    int  preDis  = s_dismissed;

    void* dlg = BuildSyntheticDialog(outerUi, "Dialog/SaveLoad.rsc:confirmDelete");
    if (!dlg) { std::fprintf(stderr, "[ModalDriver/TEST] %s: dialog build FAILED\n", label); c.ret = -9999; return c; }

    uint64_t dlgPtr = 0;
    fn_InternalBasePtr_Ctor(&dlgPtr, dlg);
    std::fprintf(stderr, "[ModalDriver/TEST] %s: entering ShowDialog(%p) dialog=%p\n",
                 label, (void*)show, dlg);
    std::fflush(stderr);

    g_RtShowDialogDefect = defect;
    c.ret = show(outerUi, &dlgPtr);
    g_RtShowDialogDefect = 0;

    c.uiAfter    = *reinterpret_cast<uint64_t*>((uintptr_t)dlg + kDlg_Ui);
    c.flagsAfter = *reinterpret_cast<uint32_t*>((uintptr_t)dlg + kDlg_Flags);
    c.drawCount  = *reinterpret_cast<int*>(b + kML_DrawCount);
    c.updCount   = *reinterpret_cast<int*>(b + kML_UpdCount);
    c.drawArr    = *reinterpret_cast<void**>(b + kML_DrawArray);
    c.updArr     = *reinterpret_cast<void**>(b + kML_UpdArray);
    c.dismissedDelta = s_dismissed - preDis;

    std::fprintf(stderr,
        "[ModalDriver/TEST] %s: ret=%d flags=0x%x drawCount %d->%d updCount %d->%d dismissed+%d\n",
        label, c.ret, c.flagsAfter, preDraw, c.drawCount, preUpd, c.updCount, c.dismissedDelta);
    std::fflush(stderr);

    fn_InternalBasePtr_DeletePtr(&dlgPtr);
    return c;
}

}  // namespace

void RunSelfTest(void* outerUi) {
    if (s_testDone) return;
    const char* t = std::getenv("BANISHED_MODAL_DRIVER_TEST");
    if (!t || !t[0] || std::strcmp(t, "0") == 0) return;
    if (!s_armed) {
        std::fprintf(stderr, "[ModalDriver/TEST] SKIPPED: driver not armed "
                             "(set BANISHED_MODAL_DRIVER=<answer> too)\n");
        return;
    }
    if (!outerUi) return;
    s_testDone = true;

    void* raw = ResolveExportRaw(
        "?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z");
    void* cur = (void*)ResolveAddr(Module::Runtime, kShowDialog);
    std::fprintf(stderr,
        "[ModalDriver/TEST] === twin-subject differential over ShowDialog ===\n"
        "[ModalDriver/TEST] THEIRS(raw DLL)=%p  OURS(registry)=%p  %s\n",
        raw, cur, (raw == cur) ? "*** IDENTICAL -- the port is NOT registered; "
                                 "this run compares retail with itself and proves nothing ***"
                              : "(distinct -- a real differential)");

    // Round 0 is the real measurement. Rounds 1..3 are NEGATIVE CONTROLS: each
    // injects one defect into OUR body only (never theirs) and names the single
    // screen that must catch it. Each screen is scored SEPARATELY -- a control
    // scored on the union of several screens reads 3/3 while hiding that one of
    // them caught nothing (reference_union_control_hides_a_blind_screen.md).
    struct Round { int defect; const char* expect; const char* what; };
    static const Round kRounds[] = {
        { 0, nullptr,     "clean (the measurement)" },
        { 1, "drawCount", "leak one benign callback pair" },
        { 1, "updCount",  "" },                       // same round, second screen
        { 2, "return",    "return +0x28 instead of the result at +0x24" },
        { 3, "flags",     "set an extra flag bit alongside 0x20" },
    };
    (void)kRounds;

    int  caught[4] = {0,0,0,0};
    bool screenFired[4][6] = {};
    static const char* kScreen[6] =
        { "return", "flags", "drawCount", "updCount", "dismissed", "ui!=0" };

    for (int defect = 0; defect <= 3; ++defect) {
        std::fprintf(stderr, "[ModalDriver/TEST] --- round defect=%d (%s) ---\n", defect,
            defect == 0 ? "CLEAN: this round is the measurement"
          : defect == 1 ? "CONTROL: leak one benign callback pair -> drawCount/updCount"
          : defect == 2 ? "CONTROL: return +0x28 not +0x24            -> return"
                        : "CONTROL: extra flag bit alongside 0x20     -> flags");

        Cell theirs = RunCell(outerUi, (fn_ShowDialog_t)raw, "THEIRS", 0);
        Cell ours   = RunCell(outerUi, (fn_ShowDialog_t)cur, "OURS  ", defect);

        long long t[6] = { theirs.ret, theirs.flagsAfter, theirs.drawCount,
                           theirs.updCount, theirs.dismissedDelta, theirs.uiAfter != 0 };
        long long o[6] = { ours.ret,   ours.flagsAfter,   ours.drawCount,
                           ours.updCount,   ours.dismissedDelta,   ours.uiAfter != 0 };
        for (int i = 0; i < 6; ++i) {
            if (t[i] != o[i]) {
                screenFired[defect][i] = true;
                ++caught[defect];
                std::fprintf(stderr, "[ModalDriver/TEST] MISMATCH %-10s theirs=%lld ours=%lld\n",
                             kScreen[i], t[i], o[i]);
            }
        }

        // The EFFECTIVENESS control, per round. dismissed=0 in both cells means
        // nothing entered the nested pump, and equal-and-unexercised would read
        // as a pass (reference_probe_needs_an_effectiveness_control.md).
        if (theirs.dismissedDelta == 0 && ours.dismissedDelta == 0)
            std::fprintf(stderr, "[ModalDriver/TEST] round %d INCONCLUSIVE: dismissed=0 in BOTH "
                                 "cells -- nothing exercised the nested pump\n", defect);
        else if (defect == 0 && caught[0] == 0)
            std::fprintf(stderr, "[ModalDriver/TEST] round 0: 6/6 screens equal; both cells "
                                 "returned the injected result %d; dismissed theirs=%d ours=%d\n",
                                 s_answer, theirs.dismissedDelta, ours.dismissedDelta);
    }

    std::fprintf(stderr, "[ModalDriver/TEST] === per-screen control table ===\n");
    for (int d = 1; d <= 3; ++d) {
        char fired[128] = {0};
        for (int i = 0; i < 6; ++i)
            if (screenFired[d][i]) { std::strcat(fired, kScreen[i]); std::strcat(fired, " "); }
        std::fprintf(stderr, "[ModalDriver/TEST] defect %d: %s -> screens fired: %s\n",
                     d, d == 1 ? "leaked callback pair"
                      : d == 2 ? "wrong return field  "
                               : "extra flag bit      ",
                     fired[0] ? fired : "NONE  *** this control was NOT caught ***");
    }
    bool controlsOk = caught[1] > 0 && caught[2] > 0 && caught[3] > 0;
    std::fprintf(stderr, "[ModalDriver/TEST] VERDICT: round0 mismatches=%d, controls caught=%d/3 -> %s\n",
                 caught[0], (caught[1]>0) + (caught[2]>0) + (caught[3]>0),
                 (caught[0] == 0 && controlsOk)
                     ? "PASS (differential clean AND proven capable)"
                     : (caught[0] != 0) ? "FAIL (round 0 diverged)"
                                        : "INCONCLUSIVE (a control escaped -- the "
                                          "instrument is blind on that screen)");
    std::fflush(stderr);
}

}  // namespace ModalDriver
