// ============================================================================
// runtime/src/ui/userinterface.cpp — Wave 7 lane C — UI::UserInterface
// ============================================================================
//
// Ports 8 of the 22 `UI::UserInterface` entries our tree calls (PLAN.md §4.2
// wave 7, §6). The recovered layout and the evidence for every offset are in
// runtime/types/ui/UserInterface.h; the per-function write-up, the oracle
// argument for each of the 8, the reason for each of the 14 hold-backs, and
// the three phantom-name resolutions are in docs/runtime_port/WAVE7_LANE_C.md.
//
//   | Export (short form)         | Retail      | Ord  | Ported | Oracle    |
//   |-----------------------------|-------------|------|--------|-----------|
//   | ??0UserInterface (ctor)     | 0x180120de0 |  471 | yes    | twin      |
//   | ??1UserInterface (dtor)     | 0x180120b90 |  705 | NO     | —         |
//   | ?AddDialog@                 | 0x180120ee0 | 1546 | NO     | —         |
//   | ?BringDialogToFront@ (V)    | 0x180120440 | 1672 | NO     | —         |
//   | ?CloseDialog@ (V)           | 0x180120f20 | 1725 | NO     | —         |
//   | ?Draw@                      | 0x180121890 | 2272 | NO     | —         |
//   | ?EnableCursor@              | 0x180122580 | 2293 | NO     | —         |
//   | ?EndSpriteCapture@          | 0x18011f4a0 | 2316 | yes    | replay    |
//   | ?GetAddDialogCount@         | 0x180031160 | 2441 | yes    | replay    |
//   | ?GetFirstContainedDialog@   | 0x18011fb00 | 2669 | yes    | replay    |
//   | ?GetFirstDialog@            | 0x180031150 | 2670 | yes    | replay    |
//   | ?PausesGame@                | 0x18011fa10 | 3317 | yes    | replay    |
//   | ?Play@                      | 0x1801223d0 | 3418 | NO     | —         |
//   | ?SaveAllSettings@           | 0x1801204b0 | 3608 | NO     | —         |
//   | ?SetCursor@                 | 0x1801225f0 | 3865 | NO     | —         |
//   | ?SetFullscreenFade@         | 0x18011f500 | 3887 | yes    | replay    |
//   | ?SetFullscreenFadeSprite@   | 0x18011f560 | 3888 | yes    | replay    |
//   | ?ShowDialog@                | 0x18011fd10 | 4034 | NO     | —         |
//   | ?Update@                    | 0x1801213c0 | 4511 | NO     | —         |
//
// (The lane's scope list is 22 rows, but three of them are PHANTOM SPELLINGS
// of rows already in this table — see "THE THREE PHANTOM NAMES" below — so
// there are 19 distinct functions and the achievable denominator is 19.)
//
// Every mangled name above was checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with a pefile query BEFORE any body was
// written (reference_resolveexport_phantom_names.md: a name the DLL does not
// contain resolves to NULL in silence, the port still registers, and the census
// still counts it). The addresses and ordinals above are that query's output.
//
// ---------------------------------------------------------------------------
// WHY REGISTERING THIS PORT IS SAFE, EVEN THOUGH THE DLL KEEPS CALLING ITSELF
// ---------------------------------------------------------------------------
// `UI::UserInterface` is a stateful object the DLL drives internally:
// `Engine::DialogLoaderLoopT` and `DialogLoopT` construct one, `ShowDialog`
// constructs a nested one, and `Engine::MainLoop` pumps `Update`/`Draw` on it
// every frame. Registering a port redirects OUR callers only — the DLL reaches
// its own methods through direct near-CALLs the registry cannot see — so for
// anything ported here there really are two implementations operating on one
// live object. That is the hazard PLAN §4.2's closing note names, and WAVE7.md
// flagged this lane as the most likely first user of the detour mechanism.
//
// It is safe here, and NO DETOUR IS INSTALLED, because of four facts, each
// CHECKED rather than assumed:
//
//   1. The class is NOT polymorphic. The ctor's first store is
//      `_dialogs._head = 0`, not a vtable pointer (disassembly quoted in
//      runtime/types/ui/UserInterface.h), and none of the 52 exported members
//      is `UEAA`. So there is no vtable-ownership split: PLAN §4.3 risk 3 does
//      not apply, exactly as it did not for `Video::Context` in Wave 6 lane A.
//   2. Every ported body is a leaf. Five are pure reads; three write only
//      scalar fields of `this`. NONE of the eight allocates, frees, links,
//      unlinks, calls another ported body, or reaches any global. The layout
//      they read is `static_assert`ed offset by offset.
//   3. Everything that DOES reshape the object — the dtor, AddDialog,
//      CloseDialog, BringDialogToFront, Update, Draw, ShowDialog — is held
//      back and stays in the DLL. So the DLL remains the sole owner of the
//      object's structure, and our eight only observe it or set flags in it.
//   4. The replay oracle below proves, on every diffed call, that our body
//      leaves the WHOLE 0x128-byte object byte-identical to what the DLL's
//      body would have left, and returns what the DLL's body would have
//      returned.
//
// The honest form of the detour verdict, since this lane was asked for it
// explicitly: a detour is NOT needed *for what this lane ported*. It is not a
// finding that `UserInterface` in general needs no detour. The day someone
// ports `Update` or `AddDialog`, the DLL's `MainLoop` will still be calling
// the DLL's `Update` on the same object our `AddDialog` mutated, and that is
// the question the detour exists to answer. This lane deliberately did not
// walk up to that line.
//
// ---------------------------------------------------------------------------
// THE THREE PHANTOM NAMES — resolved, and they are NOT dead code
// ---------------------------------------------------------------------------
// WAVE7.md lists three lane-C mangled names that do not exist in the shipped
// export table, and expected the Wave 6 `Geometry::Attach`/`Detach` shape: a
// silent NULL behind an `if (ptr)` guard, i.e. behaviour that has never
// happened. **That is not what these are.** All three call sites have an
// explicit `ResolveAddr(Module::Runtime, …)` fallback, and all three fall back
// onto the RIGHT function:
//
//   engine_funcs.cpp:1603  CloseDialog         AEAV… -> NULL -> 0x180120F20 = the V twin
//   engine_funcs.cpp:3252  BringDialogToFront  AEAV… -> NULL -> 0x180120440 = the V twin
//   engine_funcs.cpp:2905  GetFirstContained…  QEAA… -> NULL -> 0x18011FB00 = the QEBA twin
//
// and the ABI is identical in every case, so the calls have always worked:
//   * `AEAV?$InternalPtr@…` (by reference) versus `V?$InternalPtr@…` (by
//     value) are the SAME machine calling sequence. `InternalPtr<T>` is an
//     8-byte class with a non-trivial copy constructor, so MSVC x64 passes it
//     by address either way — which the decompilations confirm directly: all
//     three callees dereference RDX (`AddDialog` does `*(void**)param_2`,
//     `CloseDialog` does `lVar2 = *param_2`). The only real difference is
//     whose copy the callee gets, and none of these three callees writes
//     through the slot, so passing the live pointer is behaviourally
//     identical to passing retail's temporary.
//   * `QEAA` versus `QEBA` is const-ness, which has no ABI at all.
//
// So this is a naming defect, not a functional gap, and the fix is at the
// source: engine_funcs.cpp now resolves the names the DLL actually exports,
// keeping the address fallback. Two things follow that DO matter:
//
//   (a) It makes the export-name audit honest again. Three of Wave 7's six
//       phantoms were lane C's, and all three were benign; a phantom masked by
//       an address fallback is exactly the case
//       reference_resolveexport_phantom_names.md warns is invisible.
//   (b) It changes what a PORT of one of these has to do. Had someone ported
//       `CloseDialog` and registered it under the DLL's real name only, the
//       phantom-name resolve would still have returned NULL, the address
//       fallback would still have found the DLL, and the port would have been
//       DEAD at its only call site while reporting `calls=0 divergences=0`
//       (reference_uncalled_port_is_not_verified.md). Registering under BOTH
//       the name and the retail address — which this file does for all eight
//       ports — is what closes that hole.
//
// ---------------------------------------------------------------------------
// WHAT STAYS IN THE DLL, AND WHY (short form; the long form is the lane doc)
// ---------------------------------------------------------------------------
//   ??1UserInterface (dtor)   Unlinks and frees every list node under the
//                             allocator's CRITICAL_SECTION and runs each
//                             dialog's deleting destructor. Win32 in a
//                             non-_win file (PLAN §4.1 decision 1), the Wave 2
//                             allocator (NO-GO), and no oracle: a destructor
//                             runs once.
//   ?AddDialog@               Grows _addDialogs. A replay would push the SAME
//                             dialog twice through the SAME buffer pointer,
//                             or realloc and free the buffer the live object
//                             still points at. No oracle exists.
//   ?CloseDialog@             Recursive, allocates, frees, unlinks nodes from
//                             a DialogInstance's OWN child list, and calls
//                             UpdateEvents. Not replayable for the same
//                             reason, several times over.
//   ?BringDialogToFront@      Removes and re-appends a list node. A replay
//                             moves it twice and allocates a second node.
//   ?Draw@                    Builds a Camera and a SpriteContext, submits
//                             every dialog's geometry and frees four buffers.
//                             A replay draws the frame twice.
//   ?Update@                  The class's whole tick: drains _addDialogs into
//                             the list, allocates a node per dialog, frees the
//                             array, advances the fade, calls
//                             DialogInstance::Update on every dialog and
//                             consumes input. Not replayable; not even
//                             idempotent.
//   ?ShowDialog@              A NESTED `Engine::MainLoop::Frame` LOOP. See the
//                             dedicated note below.
//   ?Play@                    Queues a sound on the Audio::Scene. A replay
//                             plays it twice, audibly.
//   ?EnableCursor@            Mutates the Application singleton and dispatches
//                             through a handler tree. See the note below.
//   ?SetCursor@               Calls the Video::Device backend. See the note.
//   ?SaveAllSettings@         Builds an IO::MemoryStream and serialises every
//                             dialog's settings. Allocates, frees, and writes
//                             persisted state.
//
// ?ShowDialog@ — READ THIS BEFORE PORTING IT
// ------------------------------------------
// `ShowDialog` (0x18011fd10) allocates a SECOND UserInterface (this is where
// the literal `Allocate(_singleton, 0x128, 4)` that pins our sizeof comes
// from), copies the audio scene and the seven sound slots into it, moves the
// dialog onto it, registers `DrawStandAloneUI`/`UpdateStandAloneUI` callbacks
// on `Engine::MainLoop`, and then
//
//     do { ... } while (!Engine::MainLoop::Frame(_singleton, true));
//
// pumps whole frames until the dialog leaves the nested UI's list. It then
// unregisters both callbacks, destroys the nested UI and frees it, and returns
// `*(int*)(dialog + 0x24)` — the dialog's result code.
//
// This single fact was the whole `Save9.sav` autoload stall and blocked the
// plan for two waves (docs/runtime_port/WAVE4_LANE_H.md §1): no unattended
// driver can reach a modal, because the driver runs inside the nested loop
// with its own state machine already finished. It is why `BANISHED_MODAL_ANSWER`
// exists. Porting it would mean owning `MainLoop::Frame` re-entrancy, two
// callback arrays' growth policy, and the lifetime of a second UserInterface —
// with no oracle of any kind, because running it twice pumps two frame loops.
// HELD BACK. This is the good outcome.
//
// One correction to WAVE4_LANE_H.md while we are here, since a port has now
// read the body: that document says ShowDialog "takes its argument by value
// and writes through the slot it is given". The by-value half is right and the
// caller-side fix built on it is right. The "writes through the slot" half is
// not: the decompiled body only READS `*param_2` (four times) and never stores
// to it. What retail's caller at exe 0x1400b61e0 is really doing with its
// scratch copy is honouring MSVC's by-value convention, where the CALLER owns
// and destroys the copy. Nothing about the fix changes.
//
// ?EnableCursor@ and ?SetCursor@ — held back for the same shape of reason
// -----------------------------------------------------------------------
// Both are small and fully reversed, and both would register a body whose
// risky half the oracle cannot see.
//   EnableCursor writes `_cursorEnabled`, then — unless
//   `Input::Configuration::_singleton[10]` forces it off — writes
//   `Application::_singleton[0x10]` and walks a keyed handler tree to fire one
//   indirect callback with id 0x20. A replay is WORSE than useless here: our
//   body would have already set the Application flag, so the DLL's replay
//   takes the early-out and never exercises the tree walk at all. The object
//   compare would pass while the only thing that can go wrong went untested.
//   SetCursor writes `_cursor` and then calls
//   `Video::Device::_singleton` -> `+0x100` -> vtable `+0x40` with the
//   cursor's sheet image and hotspot. A replay issues that backend call a
//   second time per diffed call. Setting the same cursor twice is very likely
//   idempotent, but "very likely" is not an oracle, and a graphics-backend
//   call is not the place to find out.
// Both are one afternoon's work for whoever wants them, with a shadow oracle
// that snapshots the Application singleton / the device state instead.
//
// ---------------------------------------------------------------------------
// PORTABILITY: no <windows.h> anywhere in this file, per PLAN §4.1 decision 1.
// Every held-back function above is also, not coincidentally, one that takes a
// CRITICAL_SECTION or talks to a device.
// ============================================================================

#include <cstring>

#include "../../funcs/rt_port.h"
#include "ui/UserInterface.h"

namespace {

using UI::UserInterface;

constexpr size_t kUiSize = sizeof(UserInterface);
static_assert(kUiSize == 0x128, "layout header and this file must agree");

// ---- the mangled names, once ------------------------------------------------
const char* const kCtor            = "??0UserInterface@UI@@QEAA@XZ";
const char* const kEndSpriteCapture= "?EndSpriteCapture@UserInterface@UI@@QEAAXXZ";
const char* const kGetAddCount     = "?GetAddDialogCount@UserInterface@UI@@QEBAHXZ";
const char* const kGetFirstDialog  =
    "?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@@Collection@@XZ";
const char* const kGetFirstCont    =
    "?GetFirstContainedDialog@UserInterface@UI@@QEBA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z";
const char* const kPausesGame      = "?PausesGame@UserInterface@UI@@QEBA_NXZ";
const char* const kSetFade         = "?SetFullscreenFade@UserInterface@UI@@QEAAX_NI0@Z";
const char* const kSetFadeSprite   = "?SetFullscreenFadeSprite@UserInterface@UI@@QEAAXVSprite@2@@Z";

// ============================================================================
// The replay oracle
// ============================================================================
// Snapshot the live 0x128-byte object into a private scratch copy BEFORE our
// body runs, run our body on the real object, run the DLL's body on the copy,
// then memcmp all 0x128 bytes and compare the return values. `RtDiffReportBytes`
// names the first differing offset, and the table in
// runtime/types/ui/UserInterface.h turns that offset straight into a field.
//
// This is a FULL oracle for the three setters, not a spot check: it catches a
// wrong offset, a branch taken that retail would not have taken, a write into
// padding, and any stray write into the ~200 bytes of the object this lane
// never reads. Unwritten padding is not a hazard here (contrast the ctor
// below) because both objects start from the same live snapshot, so bytes
// neither implementation touches are equal by construction —
// reference_oracle_must_not_compare_unwritten_bytes.md.
//
// For the five READ-ONLY functions the same snapshot serves a second purpose:
// after both have run, the copy proves the DLL's body did not write either.
//
// One shared file-scope buffer rather than a 296-byte stack frame per call.
// Safe because the UI is driven from one thread (Engine::MainLoop's) and no
// ported body here calls another ported body — all eight are leaves. If either
// premise ever changes this must become a per-call buffer.
alignas(16) uint8_t g_replayScratch[kUiSize];

inline UserInterface* ReplaySnapshot(bool on, const UserInterface* live) {
    if (!on) return nullptr;
    std::memcpy(g_replayScratch, live, kUiSize);
    return reinterpret_cast<UserInterface*>(g_replayScratch);
}

// ============================================================================
// The twin oracle — for the constructor only
// ============================================================================
// A constructor cannot be replayed against a snapshot of its own object: there
// is no "before" state to copy, and running the DLL's ctor over the live
// object after ours would simply overwrite our work with an identical result
// and prove nothing about the 25 bytes neither one writes.
//
// So: pre-fill TWO scratch buffers with the SAME poison pattern, run OUR ctor
// on one and the DLL's on the other, and compare all 0x128 bytes. The poison
// makes the six unwritten ranges (listed in the layout header) compare equal
// by construction, and makes any byte one implementation writes and the other
// does not fail loudly instead of silently matching a zero.
//
// The live object is constructed by our body exactly once, as normal; the twin
// comparison happens beside it and touches nothing the caller owns.
alignas(16) uint8_t g_ctorOurs[kUiSize];
alignas(16) uint8_t g_ctorTheirs[kUiSize];

} // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body is byte-faithful to the retail disassembly (read, not only the
// decompiled C — reference_ghidra_c_output_vs_disassembly.md). No null guard
// retail does not have; the CLAUDE.md rule holds even where a guard looks
// free.

// ---- ??0UserInterface@UI@@QEAA@XZ  0x180120de0 ord 471 ---------------------
// 42 stores of a constant, in address order, and RAX = this on return. The two
// non-zero constants are `_uiScale = 1.0f` (0x3f800000 at +0xdc) and the pair
// `_fadeSpriteIndex = -1` (+0xe8) / `_fadeColor = 0xffffffff` (+0xf8).
//
// FAITHFULNESS DETAIL: retail writes these as raw-width stores, not as typed
// field assignments, and two of them straddle field boundaries — the WORD at
// +0xd8 clears `_cursorEnabled` and `_mouseCaptured` together and stops (so
// +0xda..+0xdb stay poison), and the QWORDs at +0xfc and +0x104 cover the four
// ints of `_clipRegion` in two pairs and stop (so +0x10c..+0x10f stay poison).
// The port reproduces the WIDTHS, not just the values, because the twin oracle
// compares the bytes in between.
//
// The body is factored into CtorBody() so the twin oracle can run OUR
// implementation a second time on a scratch buffer without recursing into the
// diff wrapper.
static void CtorBody(UserInterface* self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    auto q64 = [p](size_t off, uint64_t v) { std::memcpy(p + off, &v, 8); };
    auto q32 = [p](size_t off, uint32_t v) { std::memcpy(p + off, &v, 4); };
    auto q16 = [p](size_t off, uint16_t v) { std::memcpy(p + off, &v, 2); };
    auto q8  = [p](size_t off, uint8_t  v) { std::memcpy(p + off, &v, 1); };

    q64(0x00, 0);  q64(0x08, 0);  q32(0x10, 0);
    q64(0x18, 0);  q64(0x20, 0);  q64(0x28, 0);  q64(0x30, 0);
    q64(0x38, 0);  q64(0x40, 0);  q64(0x48, 0);  q64(0x50, 0);
    q64(0x58, 0);  q64(0x60, 0);  q64(0x68, 0);  q32(0x70, 0);
    q64(0x78, 0);  q64(0x80, 0);  q8 (0x88, 0);
    q64(0x90, 0);  q64(0x98, 0);  q64(0xa0, 0);  q64(0xa8, 0);
    q64(0xb0, 0);  q64(0xb8, 0);  q64(0xc0, 0);  q64(0xc8, 0);
    q64(0xd0, 0);
    q16(0xd8, 0);                       // _cursorEnabled + _mouseCaptured
    q32(0xdc, 0x3f800000u);             // _uiScale = 1.0f
    q64(0xe0, 0);                       // _fadeSpriteSheet
    q32(0xe8, 0xffffffffu);             // _fadeSpriteIndex = -1 (empty sprite)
    q64(0xf0, 0);                       // _fadeVelocity + _fadeAmount
    q32(0xf8, 0xffffffffu);             // _fadeColor
    q64(0xfc, 0);                       // _clipRegion.left + .top
    q64(0x104, 0);                      // _clipRegion.right + .bottom
    q64(0x110, 0);                      // _spriteCapture
    q64(0x118, 0);  q64(0x120, 0);      // _captureViewport
}

extern "C" void* RtUi_UserInterface_Ctor(UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::UserInterface");

    CtorBody(self);

    if (rt_diff_on__) {
        using Fn = void* (*)(UserInterface*);
        std::memset(g_ctorOurs,   0xA5, kUiSize);
        std::memset(g_ctorTheirs, 0xA5, kUiSize);
        CtorBody(reinterpret_cast<UserInterface*>(g_ctorOurs));
        void* rb = RT_ORIGINAL_ONCE(Fn, kCtor)(reinterpret_cast<UserInterface*>(g_ctorTheirs));
        // The DLL's ctor returns `this` in RAX. Our port must too — a member
        // that forgets RAX is invisible to a bytes-only compare, and
        // code/Game/Game/gameupdateloop_load.cpp:91 types the pointer as
        // returning void*.
        if (rb != static_cast<void*>(g_ctorTheirs))
            RtDiffReport(&rt_diff_site__, false,
                         "ours returns this", "theirs did NOT return this");
        RT_DIFF_BYTES(g_ctorOurs, g_ctorTheirs, kUiSize);
    }
    return self;
}

// ---- ?GetAddDialogCount@…@QEBAHXZ  0x180031160 ord 2441 --------------------
//   MOV EAX,[RCX+0x30] / RET
extern "C" int RtUi_UserInterface_GetAddDialogCount(const UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::GetAddDialogCount");
    const UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    int ours = self->_addDialogs._size;

    if (rt_diff_on__) {
        using Fn = int (*)(const UserInterface*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kGetAddCount)(replay));
    }
    return ours;
}

// ---- ?GetFirstDialog@…  0x180031150 ord 2670 -------------------------------
//   MOV RAX,[RCX] / MOV [RDX],RAX / MOV RAX,RDX / RET
// Returns a `LinkedList<…>::Iterator` by value. The iterator is EIGHT BYTES
// and is just the node pointer (Collection.h), so the hidden return slot in
// RDX gets one qword and the function hands the slot back in RAX —
// reference_hidden_retval_and_int_pointer_args.md: forget the retval slot and
// this becomes a wild write that still "works".
extern "C" void* RtUi_UserInterface_GetFirstDialog(UserInterface* self, void* retval) {
    RT_DIFF_BEGIN("UI::UserInterface::GetFirstDialog");
    const UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    *reinterpret_cast<void**>(retval) = self->_dialogs._head;

    if (rt_diff_on__) {
        using Fn = void* (*)(const UserInterface*, void*);
        void* theirsSlot = nullptr;
        void* rr = RT_ORIGINAL_ONCE(Fn, kGetFirstDialog)(replay, &theirsSlot);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), *reinterpret_cast<void**>(retval));
        RtDiffFormat(b, sizeof(b), theirsSlot);
        RtDiffReport(&rt_diff_site__,
                     *reinterpret_cast<void**>(retval) == theirsSlot && rr == &theirsSlot,
                     a, b);
    }
    return retval;
}

// ---- ?PausesGame@…@QEBA_NXZ  0x18011fa10 ord 3317 --------------------------
// Walks the dialog list FORWARD from _head through `next` (+0x10) and returns
// true at the first dialog whose flag word at +0x20 has bit 0 set.
//
//   18011fa20  MOV RCX,[RAX]                ; node->element (the dialog)
//   18011fa23  TEST byte ptr [RCX+0x20],0x1 ; DialogInstance::_flags bit 0
//   18011fa29  MOV RAX,[RAX+0x10]           ; node = node->next
//
// The +0x20 flag word belongs to lane B's DialogInstance; it is read here as a
// raw offset with the disassembly above as its evidence, rather than by
// including a header another lane owns.
extern "C" bool RtUi_UserInterface_PausesGame(const UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::PausesGame");
    const UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    bool ours = false;
    for (auto* node = self->_dialogs._head; node; node = node->next) {
        const uint8_t* dlg = reinterpret_cast<const uint8_t*>(node->element);
        if (*reinterpret_cast<const uint32_t*>(dlg + 0x20) & 1u) { ours = true; break; }
    }

    if (rt_diff_on__) {
        using Fn = bool (*)(const UserInterface*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kPausesGame)(replay));
    }
    return ours;
}

// ---- ?GetFirstContainedDialog@…@QEBA…HH@Z  0x18011fb00 ord 2669 ------------
// Walks the dialog list BACKWARD from _tail through `prev` (+0x08) — i.e.
// front-most first, so the topmost dialog wins the hit test — and returns the
// first dialog that contains (x, y), or a null InternalPtr.
//
// Per dialog, from the disassembly at 0x18011fb30:
//   flags = *(uint32*)(dlg + 0x20)
//   if (flags & 0x4)  -> skip this dialog entirely
//   if (flags & 0x8)  -> custom hit test:
//         element = *(void**)(dlg + 0x50);          root element
//         ui      = *(void**)(dlg + 0x10);          owning UserInterface
//         hit     = element->vtable[0x48](element, x, y, ui)
//   else              -> rectangle test against the root element's bounds:
//         e = *(void**)(dlg + 0x50)
//         hit = x >= e[+0x68] && x <= e[+0x70] && y >= e[+0x6c] && y <= e[+0x74]
//
// NOTE THE COMPARISON DIRECTION: the JL/JG/JL/JLE chain makes the rectangle
// INCLUSIVE on all four sides, and it pairs +0x68 with +0x70 for x and +0x6c
// with +0x74 for y — left/right are NOT adjacent to top/bottom. Those four
// offsets are lane A's Element bounds, read raw here for the same reason as
// the flag word above.
//
// ORACLE CAVEAT, stated because it is the one thing here that is not free:
// under a diff the custom-hit-test virtual is invoked a SECOND time, on the
// same element with the same arguments. The method is `const` in every widget
// that overrides it (it answers "do you contain this point"), so the second
// call is a pure re-evaluation — but it is a virtual into another lane's code,
// and if some widget ever makes it stateful this site is where that surfaces.
extern "C" void* RtUi_UserInterface_GetFirstContainedDialog(
        const UserInterface* self, void* retval, int x, int y) {
    RT_DIFF_BEGIN("UI::UserInterface::GetFirstContainedDialog");
    const UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    void* ours = nullptr;
    for (auto* node = self->_dialogs._tail; node; node = node->prev) {
        const uint8_t* dlg = reinterpret_cast<const uint8_t*>(node->element);
        const uint32_t flags = *reinterpret_cast<const uint32_t*>(dlg + 0x20);
        if (flags & 0x4u) continue;

        bool hit;
        if (flags & 0x8u) {
            void* element = *reinterpret_cast<void* const*>(dlg + 0x50);
            void* ui      = *reinterpret_cast<void* const*>(dlg + 0x10);
            using ContainsFn = bool (*)(void*, int, int, void*);
            void** vtable = *reinterpret_cast<void***>(element);
            hit = reinterpret_cast<ContainsFn>(vtable[0x48 / 8])(element, x, y, ui);
        } else {
            const uint8_t* e = *reinterpret_cast<const uint8_t* const*>(dlg + 0x50);
            const int32_t left   = *reinterpret_cast<const int32_t*>(e + 0x68);
            const int32_t top    = *reinterpret_cast<const int32_t*>(e + 0x6c);
            const int32_t right  = *reinterpret_cast<const int32_t*>(e + 0x70);
            const int32_t bottom = *reinterpret_cast<const int32_t*>(e + 0x74);
            hit = (x >= left) && (x <= right) && (y >= top) && (y <= bottom);
        }
        if (hit) { ours = node->element; break; }
    }
    *reinterpret_cast<void**>(retval) = ours;

    if (rt_diff_on__) {
        using Fn = void* (*)(const UserInterface*, void*, int, int);
        void* theirsSlot = nullptr;
        void* rr = RT_ORIGINAL_ONCE(Fn, kGetFirstCont)(replay, &theirsSlot, x, y);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), ours);
        RtDiffFormat(b, sizeof(b), theirsSlot);
        RtDiffReport(&rt_diff_site__, ours == theirsSlot && rr == &theirsSlot, a, b);
    }
    return retval;
}

// ---- ?EndSpriteCapture@…@QEAAXXZ  0x18011f4a0 ord 2316 ---------------------
//   MOV qword ptr [RCX+0x110],0x0 / RET
// It does NOT touch the capture viewport at +0x118, and it does NOT tell the
// SpriteCapture object anything — Draw's `if (capture) EndAddingSprites` is
// what closes the capture out. Faithful: one store, nothing else.
extern "C" void RtUi_UserInterface_EndSpriteCapture(UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::EndSpriteCapture");
    UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    self->_spriteCapture = nullptr;

    if (rt_diff_on__) {
        using Fn = void (*)(UserInterface*);
        RT_ORIGINAL_ONCE(Fn, kEndSpriteCapture)(replay);
        RT_DIFF_BYTES(self, replay, kUiSize);
    }
}

// ---- ?SetFullscreenFade@…@QEAAX_NI0@Z  0x18011f500 ord 3887 ----------------
// Three arguments and two completely different behaviours, which the parameter
// names hide: `immediate` (the third arg, R9B) does not mean "reverse".
//
//   immediate == true : set the fade AMOUNT directly, to 1.0f (fadeIn) or 0.0f,
//                       and do NOT start an animation. _fadeVelocity is left
//                       alone, so IsFading() stays false.
//   immediate == false: set the fade VELOCITY to +1.0f (fadeIn) or -1.0f and
//                       let Update integrate _fadeAmount toward the limit.
//
// FAITHFULNESS DETAIL: the colour is written in BOTH branches but at different
// points — first in the immediate branch, last in the animated one — and the
// animated branch writes it TWICE in the disassembly, once per sub-branch
// (0x18011f534 and 0x18011f546). Same value either way; reproduced as one
// store per path, which is what the byte compare sees.
extern "C" void RtUi_UserInterface_SetFullscreenFade(
        UserInterface* self, bool fadeIn, uint32_t color, bool immediate) {
    RT_DIFF_BEGIN("UI::UserInterface::SetFullscreenFade");
    UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    if (immediate) {
        self->_fadeColor  = color;
        self->_fadeAmount = fadeIn ? 1.0f : 0.0f;
    } else {
        self->_fadeVelocity = fadeIn ? 1.0f : -1.0f;
        self->_fadeColor    = color;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(UserInterface*, bool, uint32_t, bool);
        RT_ORIGINAL_ONCE(Fn, kSetFade)(replay, fadeIn, color, immediate);
        RT_DIFF_BYTES(self, replay, kUiSize);
    }
}

// ---- ?SetFullscreenFadeSprite@…@QEAAXVSprite@2@@Z  0x18011f560 ord 3888 ----
//   MOV RAX,[RDX] / MOV [RCX+0xe0],RAX / MOV RAX,[RDX+0x8] / MOV [RCX+0xe8],RAX
// A `UI::Sprite` is 0x10 bytes and is copied WHOLE, including the four padding
// bytes at its +0x0c — so this writes UserInterface +0xe0..+0xef, and the port
// must copy 16 bytes rather than assign the two named fields, or the byte
// compare will trip on padding the DLL propagated and we did not.
//
// `Sprite` is passed by value, which for a class with a non-trivial copy
// constructor means BY ADDRESS: RDX is a pointer to the caller's copy. That is
// exactly how our tree already calls it — see the 16-byte `uint8_t sprite[16]`
// that code/Game/Dialog/optionsdialog.cpp's ApplyKeyMapToUI_Impl builds.
extern "C" void RtUi_UserInterface_SetFullscreenFadeSprite(
        UserInterface* self, const void* sprite) {
    RT_DIFF_BEGIN("UI::UserInterface::SetFullscreenFadeSprite");
    UserInterface* replay = ReplaySnapshot(rt_diff_on__, self);

    std::memcpy(&self->_fadeSpriteSheet, sprite, 0x10);

    if (rt_diff_on__) {
        using Fn = void (*)(UserInterface*, const void*);
        RT_ORIGINAL_ONCE(Fn, kSetFadeSprite)(replay, sprite);
        RT_DIFF_BYTES(self, replay, kUiSize);
    }
}

// ============================================================================
// Registration
// ============================================================================
// Every port is registered TWICE, under its mangled name and under its retail
// address. The address form is not defensive housekeeping here — it is load
// bearing. Three of this class's call sites resolve a mangled name the DLL does
// not export and reach the function through `ResolveAddr(Module::Runtime, …)`
// instead (see THE THREE PHANTOM NAMES above), and `ResolveAddr` consults the
// runtime address registry FIRST (standalone/resolve_standalone.cpp). A
// name-only registration would leave such a site on the DLL while the census
// counted the port as landed — the exact both-doors failure Wave 5 lane D hit.
//
// Ordering is already right: RuntimePortInit() runs before AddressRegistryInit()
// and EngineFuncsInit() (standalone/main.cpp:1614 / :1626 / :1631), both of
// which cache these pointers, so a port registered here is what they cache.
// EVERY NAME BELOW IS SPELLED AS A STRING LITERAL, on purpose and at the cost
// of repeating the k* constants above. tools/runtime_census.py matches
// `RegisterRuntimeExport("<literal>"` and CANNOT see through a named constant:
// that blind spot hid six live Wave 3 ports for a whole wave and made the
// Object census read 21/41 when it was 27/41
// (reference_census_cannot_see_nonliteral_registrations.md). The census is
// this project's authority over any agent's "N ported" claim, including this
// lane's, so it has to be able to read this function.
int RtUiManagerRegister() {
    int count = 0;

    RegisterRuntimeExport("??0UserInterface@UI@@QEAA@XZ",
                          (void*)&RtUi_UserInterface_Ctor); ++count;
    RegisterRuntimeAddr(0x180120de0, (void*)&RtUi_UserInterface_Ctor);

    RegisterRuntimeExport("?GetAddDialogCount@UserInterface@UI@@QEBAHXZ",
                          (void*)&RtUi_UserInterface_GetAddDialogCount); ++count;
    RegisterRuntimeAddr(0x180031160, (void*)&RtUi_UserInterface_GetAddDialogCount);

    RegisterRuntimeExport("?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@@Collection@@XZ",
                          (void*)&RtUi_UserInterface_GetFirstDialog); ++count;
    RegisterRuntimeAddr(0x180031150, (void*)&RtUi_UserInterface_GetFirstDialog);

    RegisterRuntimeExport("?PausesGame@UserInterface@UI@@QEBA_NXZ",
                          (void*)&RtUi_UserInterface_PausesGame); ++count;
    RegisterRuntimeAddr(0x18011fa10, (void*)&RtUi_UserInterface_PausesGame);

    RegisterRuntimeExport("?GetFirstContainedDialog@UserInterface@UI@@QEBA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z",
                          (void*)&RtUi_UserInterface_GetFirstContainedDialog); ++count;
    RegisterRuntimeAddr(0x18011fb00, (void*)&RtUi_UserInterface_GetFirstContainedDialog);

    RegisterRuntimeExport("?EndSpriteCapture@UserInterface@UI@@QEAAXXZ",
                          (void*)&RtUi_UserInterface_EndSpriteCapture); ++count;
    RegisterRuntimeAddr(0x18011f4a0, (void*)&RtUi_UserInterface_EndSpriteCapture);

    RegisterRuntimeExport("?SetFullscreenFade@UserInterface@UI@@QEAAX_NI0@Z",
                          (void*)&RtUi_UserInterface_SetFullscreenFade); ++count;
    RegisterRuntimeAddr(0x18011f500, (void*)&RtUi_UserInterface_SetFullscreenFade);

    RegisterRuntimeExport("?SetFullscreenFadeSprite@UserInterface@UI@@QEAAXVSprite@2@@Z",
                          (void*)&RtUi_UserInterface_SetFullscreenFadeSprite); ++count;
    RegisterRuntimeAddr(0x18011f560, (void*)&RtUi_UserInterface_SetFullscreenFadeSprite);

    return count;
}
