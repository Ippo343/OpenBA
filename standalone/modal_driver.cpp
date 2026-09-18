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
#include "options.h"
#include "ui/Dialogs.h"      // MessageDialogAddr::Vtable, for the self-test subject
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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
    // Deliberately NOT the usual `=0 means off` rule: 0 is a legitimate dialog
    // result (MessageDialog Cancel). Presence of the variable arms the driver.
    if (!Opt().modalDriver) return;
    s_enabled = true;
    s_answer  = Opt().modalDriverAnswer;
    if (Opt().modalDriverTicks > 0) s_ticks = Opt().modalDriverTicks;   // "<answer>,<ticks>"
    std::fprintf(stderr,
        "[ModalDriver] ARMED (BANISHED_MODAL_DRIVER=\"%s\") - will answer any modal "
        "with result %d after %d nested tick(s)\n", Opt().modalDriverRaw, s_answer, s_ticks);
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

    // MATCH EITHER POINTER, and note WHY there are two.
    //
    // This used to be a single `ResolveAddr(Module::Runtime, ...)` lookup, and
    // the comment above -- "works identically whether the ShowDialog that
    // registered it was the shipped DLL's or ours" -- held only while NOTHING
    // was registered at 0x180120310. ResolveAddr consults the PORT REGISTRY
    // FIRST (reference_resolveaddr_consults_port_registry_first.md), so the
    // moment Wave 10C lane UIF6 registered its UpdateStandAloneUI port at that
    // address, `wanted` became OUR function pointer while the DLL's own
    // ShowDialog kept installing the DLL's. Nothing matched, FindModalDialog
    // returned null forever, and the driver never dismissed the modal: the
    // BANISHED_MODAL_DRIVER_TEST run HUNG to its timeout with no error line --
    // an instrument confidently reporting nothing, about the wrong thing.
    //
    // The registry address is what OUR ShowDialog port installs; a null
    // (nothing registered) simply never matches.
    void* wantedPort = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, kUpdateStandAloneUI));

    for (int i = 0; i < count; ++i) {
        void* const fn = arr[i].fn;
        if (!wantedPort || fn != wantedPort) continue;
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
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dialog));   // B.3.3 bridge
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

}  // namespace ModalDriver
