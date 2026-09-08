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
//   | ?AddDialog@                 | 0x180120ee0 | 1546 | W9-Z   | none      |
//   | ?BringDialogToFront@ (V)    | 0x180120440 | 1672 | NO     | —         |
//   | ?CloseDialog@ (V)           | 0x180120f20 | 1725 | NO     | —         |
//   | ?Draw@                      | 0x180121890 | 2272 | NO     | —         |
//   | ?EnableCursor@              | 0x180122580 | 2293 | W9-Z   | none      |
//   | ?EndSpriteCapture@          | 0x18011f4a0 | 2316 | yes    | replay    |
//   | ?GetAddDialogCount@         | 0x180031160 | 2441 | yes    | replay    |
//   | ?GetFirstContainedDialog@   | 0x18011fb00 | 2669 | yes    | replay    |
//   | ?GetFirstDialog@            | 0x180031150 | 2670 | yes    | replay    |
//   | ?PausesGame@                | 0x18011fa10 | 3317 | yes    | replay    |
//   | ?Play@                      | 0x1801223d0 | 3418 | W9-Z   | none      |
//   | ?SaveAllSettings@           | 0x1801204b0 | 3608 | NO     | —         |
//   | ?SetCursor@                 | 0x1801225f0 | 3865 | W9-Z   | none      |
//
// `W9-Z` = ported by Wave 9 lane Z as a NO-ORACLE port, for the cutover. See
// the lane-Z block further down for why an oracle is not merely absent but
// would be actively misleading on all four.
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

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — the same reasoning rt_port.h gives for declaring RegisterRuntimeExport
// locally: resolve.h drags in <windows.h>, and PLAN.md §4.1 requires
// runtime/src/ to stay platform-neutral. These declarations must stay
// byte-identical to resolve.h's; the linker checks them against it.
//
// Added by Wave 9 lane Z for `AddDialog`, whose array push is a Runtime
// INTERNAL with no exported name — 0x18007a0d0, ported by Wave 9 lane Q. Note
// this is the PORT-AWARE door: ResolveAddr consults the runtime address
// registry before the DLL (reference_resolveaddr_consults_port_registry_first
// .md), which is exactly what is wanted for a dependency and is also what makes
// the call survive cutover.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using UI::UserInterface;

constexpr size_t kUiSize = sizeof(UserInterface);
static_assert(kUiSize == 0x128, "layout header and this file must agree");

// ---- the four exported DATA slots Wave 9 lane Z's ports read ----------------
// Through RT_DATA_SLOT, which never caches a null: at cutover these are supplied
// by lane C2's mint, and a slot resolved before RtDataMintInit() must be able to
// retry rather than freeze the DLL's answer for the process
// (rt_port.h, "Resolve-and-cache that NEVER caches a null").
//
// Every one of the four is already a minted row today — see the DEPENDENCY
// AUDIT in the lane-Z block below for the row numbers — so none of these is a
// new cutover obligation.
inline const double* TimerSecondsPerTick() {
    return RT_DATA_SLOT(const double*, "?_secondsPerTick@Timer@System@@0NA");
}
inline void** VideoDeviceSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
}
inline void** ApplicationSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Application@1@0PEAV11@EA");
}
inline void** InputConfigSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA");
}

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
// WAVE 9 LANE Z — the four UserInterface members the CUTOVER still needs
// ============================================================================
// Wave 7 lane C held `AddDialog`, `Play`, `SetCursor` and `EnableCursor` back.
// Its reasons are in the HELD BACK block above and none of them was wrong: all
// four mutate state that lives OUTSIDE the 0x128-byte object, so the replay
// oracle this file is built around cannot see the half that can break.
//
// What changed is not the argument, it is the cost of declining. Wave 9 removes
// `Runtime-x64.dll`. `ResolveExport` on an unported name then returns null in
// silence (standalone/resolve_standalone.cpp:692), and the four `fn_UI_*`
// pointers in engine/funcs/engine_funcs.cpp become null calls. All four are in
// the DLL-absent boot's `[RtUnresolved]` list — that list IS the work list —
// and "no oracle is available" stops being a reason to hold back once the
// alternative is an EXEC-of-0.
//
// So they are ported here as **NO-ORACLE PORTS**, the disposition Wave 9 lane M
// established for `EditElement::SetText`, `NumberElement::SetValueInt` /
// `SetValueFloat` and `NumberEditElement::SetMinMax`: a real body, registered,
// with `RT_DIFF_BEGIN` + `RT_DIFF_WORK` so a harness run still reports `calls=`
// AND how many of those calls did work — and with NO call into the shipped body
// at all, because for each of the four a second execution is not a comparison,
// it is a second side effect. Per function:
//
//   AddDialog     calls the dialog's OWN vtable slot 4 (`OnOpened`) and then
//                 pushes into `_addDialogs`. A replay would open the dialog
//                 twice and push twice — and because the scratch copy shares
//                 the array BUFFER with the live object, the second push would
//                 write through into the caller's real array.
//   Play          claims a free channel in `Audio::Scene`'s 256-entry table and
//                 stamps a handle built from the scene's monotonically
//                 increasing counter. A replay claims a SECOND channel and
//                 bumps that counter again, so the two results cannot be equal
//                 even when both are right — and the sound plays twice.
//   SetCursor     ends in `Video::Device::_singleton -> +0x100 -> vslot 8`.
//                 Lane C's note stands: setting the same cursor twice is very
//                 likely idempotent, but "very likely" is not an oracle and a
//                 graphics-backend call is not where to find out.
//   EnableCursor  is the case where a replay is WORSE than useless, and lane C
//                 spelled out why: our body has already written
//                 `Application::_singleton[0x10]`, so the DLL's replay takes
//                 the equality early-out at 0x1801225a3 and never walks the
//                 handler tree — the object compare would pass while the only
//                 risky half went untested.
//
// `RT_DIFF_WORK` is not decoration here. A site can report a large `calls=` and
// still have executed nothing (Wave 9 lane M found one with 12,896 calls of
// which TEN did work), and three of these four have an early-out that dominates
// in normal play: `Play` returns immediately for any UI sound id with no sound
// bound, and `EnableCursor` returns immediately whenever the requested state
// already matches. The counter reports how many calls got PAST that guard, so a
// reader can tell "exercised" from "entered".
//
// DEPENDENCY AUDIT — why these four and not the other 27 UI names in the
// residue. A port that calls a DLL internal does not close a cutover row, it
// MOVES it (WAVE9.md, "porting a function can GROW the cutover surface"). Every
// non-vtable callee of these four is already satisfied on OUR side:
//
//   0x18007a0d0  the `_addDialogs` push  -> PORTED, Wave 9 lane Q, registered
//                as `RtColl_ArraySetSizeFill_18007a0d0`
//                (runtime/src/collection/array.cpp:812, whose own comment
//                already names it "the dialog pending-list push").
//   0x1801fe728  `?_secondsPerTick@Timer@System@@0NA`           -> MINTED
//   0x1801fe7a8  `?_singleton@Device@Video@@0PEAV12@EA`         -> MINTED
//   0x1801fc678  `?_singleton@Application@1@0PEAV11@EA`         -> MINTED
//   0x1801fe768  `?_singleton@Configuration@Input@@0PEAV12@EA`  -> MINTED
//
// (all four rows in standalone/rt_data_mint_tables.inc, which lane C2 owns and
// this lane did not touch). The remaining indirect calls go through vtables of
// objects the CALLER owns, which is the caller's cutover problem and not a new
// dependency of ours.
// ============================================================================

// ---- ?AddDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
//      0x180120ee0  ord 1546  ------------------------------------------------
//
//     180120eed  MOV RCX,[RDX]            ; the DialogInstance* inside the ptr
//     180120ef3  MOV RAX,[RCX]            ; its vtable
//     180120ef6  CALL [RAX+0x20]          ; slot 4
//     180120ef9  MOV EDX,[RBX+0x30]       ; _addDialogs._size
//     180120efc  LEA RCX,[RBX+0x28]       ; &_addDialogs
//     180120f00  INC EDX
//     180120f02  MOV R8,RDI               ; the InternalPtr, BY ADDRESS
//     180120f0f  JMP 0x18007a0d0          ; SetSize(size+1), filling the new tail
//
// SLOT +0x20 IS `OnOpened`, and that is read out of a table rather than guessed
// from the name. `standalone/rt_data_mint_tables.inc` carries the whole of
// `??_7DialogInstance@UI@@6B@` slot by slot, minted from the DLL's own vtable:
// index 4 (byte offset 0x20) is `?OnOpened@DialogInstance@UI@@UEAAXXZ` at
// 0x180114fb0. That "add a dialog" opens it is the corroboration, not the
// evidence.
//
// The argument is `Object::InternalPtr<DialogInstance>` BY VALUE, which for a
// class with a non-trivial copy constructor means BY ADDRESS: RDX is a pointer
// to the caller's 8-byte InternalPtr. Retail dereferences it once for the
// vtable call and then hands the SAME pointer to the array push as the fill
// source, which is why it is spelled `void**` and not `void*`
// (reference_hidden_retval_and_int_pointer_args.md).
//
// NOTE what retail does NOT do: no null check on `*dlgPtr`. It relies on the
// caller never passing an empty pointer, and CLAUDE.md is explicit that a guard
// here would not restore the behaviour, it would skip it.
extern "C" void RtUi_UserInterface_AddDialog(UserInterface* self, void** dlgPtr) {
    RT_DIFF_BEGIN("UI::UserInterface::AddDialog");

    void*  dlg  = *dlgPtr;
    void** vtbl = *reinterpret_cast<void***>(dlg);
    reinterpret_cast<void (*)(void*)>(vtbl[4])(dlg);          // OnOpened

    using PushFn = void (*)(void*, int, const void*);
    static PushFn push = reinterpret_cast<PushFn>(
        ResolveAddr(Module::Runtime, 0x18007a0d0));
    push(&self->_addDialogs, self->_addDialogs._size + 1, dlgPtr);

    RT_DIFF_WORK(true);   // no early-out: every call does the whole body
}

// ---- ?Play@UserInterface@UI@@QEAAXH@Z  0x1801223d0  ord 3418 ---------------
//
// Queues UI sound `id` on the audio scene. Retail, read at 0x1801223d0:
//
//     MOVSXD RAX,EDX / MOV RAX,[RCX+RAX*8+0x98]   ; _sounds[id]
//     TEST RAX,RAX / JZ ret                        ; nothing bound -> nothing
//     MOV R10,[RCX+0x90]                           ; _audioScene
//     TEST R10,R10 / JZ ret
//     MOV R11,[R10+0x10]                           ; the channel table
//     XORPS XMM0,XMM0 / DIVSD XMM0,[0x1801fe728] / CVTTSD2SI RCX,XMM0
//
// THE DIVISION IS REPRODUCED, NOT FOLDED TO ZERO. It is `Timer::SecondsToTicks`
// inlined on the constant 0.0 — `0.0 / _secondsPerTick` — and it is only zero
// while `_secondsPerTick` is non-zero. Were that slot ever to read 0.0, retail
// computes 0.0/0.0 = NaN and CVTTSD2SI yields the x86 "integer indefinite"
// INT64_MIN, which is a very different value to write into a channel. A literal
// `0` here would hide that difference; the divide keeps our body wrong in
// exactly the same way retail is wrong, which is the whole contract.
//
// The scan walks 0x100 slots of stride 0x70 for one whose handle word at +0x68
// is zero. The slot base is `table + i*0x70`: the loop cursor starts at
// `table+0x68` and steps 0x70, and the found-branch RECOMPUTES the base from
// the index (`IMUL RDX,RDX,0x70 / ADD RDX,R11`) — which is what pins +0x68 as a
// FIELD of the slot rather than as the slot's start. A full table falls out of
// the loop into a bare RET, so retail silently drops the sound; reproduced.
//
// The stamped handle is `(index * 2) | (++scene->counter << 9)`.
extern "C" void RtUi_UserInterface_Play(UserInterface* self, int id) {
    RT_DIFF_BEGIN("UI::UserInterface::Play");

    void* sound = self->_sounds[id];
    if (!sound) { RT_DIFF_WORK(false); return; }

    uint8_t* scene = reinterpret_cast<uint8_t*>(self->_audioScene);
    if (!scene) { RT_DIFF_WORK(false); return; }

    uint8_t* table = *reinterpret_cast<uint8_t**>(scene + 0x10);

    const double secondsPerTick = *TimerSecondsPerTick();
    const int64_t ticks = static_cast<int64_t>(0.0 / secondsPerTick);

    int index = -1;
    for (int i = 0; i < 0x100; ++i) {
        if (*reinterpret_cast<const int32_t*>(table + i * 0x70 + 0x68) == 0) {
            index = i;
            break;
        }
    }
    if (index < 0) { RT_DIFF_WORK(false); return; }   // table full: retail drops it

    uint8_t* slot = table + static_cast<int64_t>(index) * 0x70;
    *reinterpret_cast<void**>(slot)          = sound;
    *reinterpret_cast<int64_t*>(slot + 0x10) = ticks;
    *reinterpret_cast<int32_t*>(slot + 0x08) = 1;

    const int32_t counter = ++*reinterpret_cast<int32_t*>(scene + 0x08);
    *reinterpret_cast<int32_t*>(slot + 0x68) = (index * 2) | (counter << 9);

    RT_DIFF_WORK(true);
}

// ---- ?SetCursor@UserInterface@UI@@QEAAXV?$ExternalPtr@VCursor@UI@@@Object@@@Z
//      0x1801225f0  ord 3865  ------------------------------------------------
//
//     MOV [RCX+0xd0],RDX          ; _cursor = cursor   (UNCONDITIONALLY)
//     TEST RDX,RDX / JZ ret
//     MOV RAX,[0x1801fe7a8]       ; Video::Device::_singleton
//     MOV R9D,[RDX+0x24]          ; cursor hotspot Y
//     MOV RCX,[RAX+0x100]         ; the device's backend object
//     MOV RAX,[RDX+0x28]          ; the cursor's SpriteSheet
//     MOV R8,[RAX+0x30]
//     MOV R10,[RCX]               ; backend vtable
//     MOV RAX,[R8+0x50]
//     MOV R8D,[RDX+0x20]          ; cursor hotspot X
//     MOV RDX,[RAX+0x8]           ; the image
//     CALL [R10+0x40]             ; slot 8
//
// THE ARGUMENT IS THE CURSOR ITSELF, not a pointer to an ExternalPtr. RDX is
// stored whole into `_cursor` and then dereferenced at +0x20/+0x24/+0x28 in the
// same basic block; an 8-byte ExternalPtr has no +0x24. Counting arguments
// against the mangled name agrees: one by-value pointer-sized argument and no
// hidden return slot, so RCX/RDX is the whole signature.
//
// The store to `_cursor` happens BEFORE the null test and is therefore NOT
// inside the guard — a null cursor still clears the field. Reproduced.
extern "C" void RtUi_UserInterface_SetCursor(UserInterface* self, void* cursor) {
    RT_DIFF_BEGIN("UI::UserInterface::SetCursor");

    self->_cursor = cursor;
    if (!cursor) { RT_DIFF_WORK(false); return; }

    uint8_t* cur     = reinterpret_cast<uint8_t*>(cursor);
    uint8_t* device  = reinterpret_cast<uint8_t*>(*VideoDeviceSingleton());
    uint8_t* backend = *reinterpret_cast<uint8_t**>(device + 0x100);
    void**   vtbl    = *reinterpret_cast<void***>(backend);

    uint8_t* sheet  = *reinterpret_cast<uint8_t**>(cur + 0x28);
    uint8_t* sheetA = *reinterpret_cast<uint8_t**>(sheet + 0x30);
    uint8_t* entry  = *reinterpret_cast<uint8_t**>(sheetA + 0x50);
    void*    image  = *reinterpret_cast<void**>(entry + 0x08);

    const int32_t hotX = *reinterpret_cast<const int32_t*>(cur + 0x20);
    const int32_t hotY = *reinterpret_cast<const int32_t*>(cur + 0x24);

    reinterpret_cast<void (*)(void*, void*, int32_t, int32_t)>(vtbl[8])(
        backend, image, hotX, hotY);

    RT_DIFF_WORK(true);
}

// ---- ?EnableCursor@UserInterface@UI@@QEAAX_N@Z  0x180122580  ord 2293 ------
//
//     MOV [RCX+0xd8],DL           ; _cursorEnabled = enable   (a BYTE store)
//     MOV RAX,[0x1801fe768]       ; Input::Configuration::_singleton
//     MOV R10,[0x1801fc678]       ; Application::_singleton
//     MOVZX ECX,DL / XOR EDX,EDX
//     CMP [RAX+0xa],DL / CMOVNZ ECX,EDX   ; config+0xa forces the cursor OFF
//     CMP [R10+0x10],CL / JZ ret          ; already in that state -> nothing
//     MOV RAX,[R10+0x18]          ; the keyed handler tree
//     MOV [R10+0x10],CL           ; publish the new state
//     ... descent keyed on the int at node+0x00 against 0x20 ...
//     JMP [RAX+0x8]  (window, 0x20, 0, 0)
//
// READ THE COMPARE CHAIN CAREFULLY — `JA` then `JNC` is not the obvious pair.
// `CMP ECX,0x20 / JA` takes the key>0x20 arm to node+0x18. The fallthrough
// `JNC` fires only when CF is clear AND the JA did not take, i.e. key == 0x20,
// and it EXITS the loop; only key < 0x20 reaches `MOV RAX,[RAX+0x20]`. Reading
// that `JNC` as "not less" and merging it with the `JA` arm descends the wrong
// child and returns a wrong handler — which would fail as a wrong CALL target,
// not as a visible UI bug.
//
// `CMOVNZ ECX,EDX` reads a BYTE at config+0xa: any non-zero value there pins the
// PUBLISHED state to 0 regardless of the caller's argument, while
// `_cursorEnabled` still records what the caller asked for. That asymmetry is
// retail's and is preserved.
//
// NO ORACLE, and this is the one where a replay is actively misleading — see
// the lane banner. `RT_DIFF_WORK` records how many calls got past the
// state-equality guard at 0x1801225a3, which is the only honest number a run
// can produce about this function without a second execution.
extern "C" void RtUi_UserInterface_EnableCursor(UserInterface* self, bool enable) {
    RT_DIFF_BEGIN("UI::UserInterface::EnableCursor");

    self->_cursorEnabled = static_cast<uint8_t>(enable);

    uint8_t* config = *reinterpret_cast<uint8_t**>(InputConfigSingleton());
    uint8_t* app    = *reinterpret_cast<uint8_t**>(ApplicationSingleton());

    uint8_t wanted = static_cast<uint8_t>(enable);
    if (*reinterpret_cast<const uint8_t*>(config + 0x0a) != 0) wanted = 0;

    if (*reinterpret_cast<const uint8_t*>(app + 0x10) == wanted) {
        RT_DIFF_WORK(false);
        return;
    }

    uint8_t* node = *reinterpret_cast<uint8_t**>(app + 0x18);
    *reinterpret_cast<uint8_t*>(app + 0x10) = wanted;

    while (node) {
        const uint32_t key = *reinterpret_cast<const uint32_t*>(node);
        if (key > 0x20u)      node = *reinterpret_cast<uint8_t**>(node + 0x18);
        else if (key < 0x20u) node = *reinterpret_cast<uint8_t**>(node + 0x20);
        else                  break;
    }
    if (!node) { RT_DIFF_WORK(false); return; }

    void* window  = *reinterpret_cast<void**>(app);
    void* handler = *reinterpret_cast<void**>(node + 0x08);
    reinterpret_cast<void (*)(void*, uint32_t, uint64_t, uint64_t)>(handler)(
        window, 0x20u, 0, 0);

    RT_DIFF_WORK(true);
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

    // ---- Wave 9 lane Z: the four cutover ports ----------------------------
    // Same double registration, for the same reason, and the literal-string
    // rule above applies to these four as well.
    RegisterRuntimeExport("?AddDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z",
                          (void*)&RtUi_UserInterface_AddDialog); ++count;
    RegisterRuntimeAddr(0x180120ee0, (void*)&RtUi_UserInterface_AddDialog);

    RegisterRuntimeExport("?Play@UserInterface@UI@@QEAAXH@Z",
                          (void*)&RtUi_UserInterface_Play); ++count;
    RegisterRuntimeAddr(0x1801223d0, (void*)&RtUi_UserInterface_Play);

    RegisterRuntimeExport("?SetCursor@UserInterface@UI@@QEAAXV?$ExternalPtr@VCursor@UI@@@Object@@@Z",
                          (void*)&RtUi_UserInterface_SetCursor); ++count;
    RegisterRuntimeAddr(0x1801225f0, (void*)&RtUi_UserInterface_SetCursor);

    RegisterRuntimeExport("?EnableCursor@UserInterface@UI@@QEAAX_N@Z",
                          (void*)&RtUi_UserInterface_EnableCursor); ++count;
    RegisterRuntimeAddr(0x180122580, (void*)&RtUi_UserInterface_EnableCursor);

    return count;
}
