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
//   | ??1UserInterface (dtor)     | 0x180120b90 |  705 | W10-UIF| twin      |
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
//   | ?SaveAllSettings@           | 0x1801204b0 | 3608 | W10-UIF| replay+twin|
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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <new>

#include "../../funcs/rt_port.h"
#include "io/Stream.h"          // WAVE 10 LANE UIF — SaveAllSettings' MemoryStream
#include "ui/UserInterface.h"
// ENDGAME B.3.6 block E: the real classes the member bodies call, and
// element_common.h's RULE B6-5 atomic door caches. element_common.h also
// carries the file-scope `Module` / `ResolveAddr` declaration described below.
#include "ui/DialogInstance.h"
#include "ui/DialogController.h"
#include "ui/Element.h"
#include "element_common.h"
#include "../../types/video/Device.h"        // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "../../types/application/Application.h"  // ENDGAME B.3.9 SS1: Application::_singleton
#include "../../types/input/Input.h"         // ENDGAME B.3.9 SS1: Input::Configuration::_singleton
#include "../../types/system/Timer.h"        // ENDGAME B.3.9 SS1: Timer::_secondsPerTick

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
// (The declaration now comes from element_common.h, included above.)

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
//
// ENDGAME B.3.6 (RULE B6-5): the member bodies below run on objects the LOADER
// thread constructs, so each slot is a namespace-scope atomic resolved through
// `rtuielem::AtomicExport` (the same never-cache-a-null contract) instead of
// RT_DATA_SLOT's function-local static.
std::atomic<void*> g_memFreeSlot{nullptr};
// ENDGAME B.3.9 SS1 (B39_RULES D7): all four are class statics now (redirect
// in their namespace's *PortInit) -- read fresh, no lazy AtomicExport cache
// (B9-10; this body runs on the LOADER THREAD, per the banner above).
inline const double* TimerSecondsPerTick() {
    return &::System::Timer::_secondsPerTick;
}
inline void** VideoDeviceSingleton() {
    return reinterpret_cast<void**>(&::Video::Device::_singleton);
}
inline void** ApplicationSingleton() {
    return reinterpret_cast<void**>(&::Application::Application::_singleton);
}
inline void** InputConfigSingleton() {
    return reinterpret_cast<void**>(&::Input::Configuration::_singleton);
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
// WAVE 10 LANE UIF. Oracle lookup only; the registration repeats the literal.
const char* const kSaveAllSettings = "?SaveAllSettings@UserInterface@UI@@QEAAXXZ";

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

    CtorBody(self);

    return self;
}

// ---- ?GetAddDialogCount@…@QEBAHXZ  0x180031160 ord 2441 --------------------
//   MOV EAX,[RCX+0x30] / RET
int32_t UI::UserInterface::GetAddDialogCount() const {
    return _addDialogs._size;
}

extern "C" int RtUi_UserInterface_GetAddDialogCount(const UserInterface* self) {
    return self->UI::UserInterface::GetAddDialogCount();
}

// ---- ?GetFirstDialog@…  0x180031150 ord 2670 -------------------------------
//   MOV RAX,[RCX] / MOV [RDX],RAX / MOV RAX,RDX / RET
// Returns a `LinkedList<…>::Iterator` by value. The iterator is EIGHT BYTES
// and is just the node pointer (Collection.h), so the hidden return slot in
// RDX gets one qword and the function hands the slot back in RAX —
// reference_hidden_retval_and_int_pointer_args.md: forget the retval slot and
// this becomes a wild write that still "works".
UI::UserInterface::Iterator UI::UserInterface::GetFirstDialog() {
    return _dialogs.GetFirst();          // the head node, one qword
}

// RETSLOT door (B5-12): `(this, ret)`, the member's prvalue placed in the slot.
extern "C" void* RtUi_UserInterface_GetFirstDialog(UserInterface* self, void* retval) {
    new (retval) UserInterface::Iterator(self->UI::UserInterface::GetFirstDialog());
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
bool UI::UserInterface::PausesGame() const {
    for (auto* node = _dialogs._head; node; node = node->next) {
        const auto* dlg = static_cast<const UI::DialogInstance*>(node->element);
        if (dlg->_flags & 1u) return true;          // DialogInstance +0x20 bit 0
    }
    return false;
}

extern "C" bool RtUi_UserInterface_PausesGame(const UserInterface* self) {
    return self->UI::UserInterface::PausesGame();
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
Object::InternalPtr<UI::DialogInstance>
UI::UserInterface::GetFirstContainedDialog(int32_t x, int32_t y) const {
    for (auto* node = _dialogs._tail; node; node = node->prev) {
        auto* dlg = static_cast<UI::DialogInstance*>(node->element);
        const uint32_t flags = dlg->_flags;                            // +0x20
        if (flags & 0x4u) continue;

        bool hit;
        if (flags & 0x8u) {
            // Element vtable slot 9 (+0x48), `Contains(x, y, ui)` — a VIRTUAL
            // call dispatches through the element's own (minted) table.
            const auto* element = static_cast<const UI::Element*>(dlg->_root);   // +0x50
            hit = element->Contains(x, y, static_cast<UserInterface*>(dlg->_ui)); // +0x10
        } else {
            const auto* e = static_cast<const UI::Element*>(dlg->_root);
            hit = (x >= e->_layoutLeft) && (x <= e->_layoutRight) &&
                  (y >= e->_layoutTop)  && (y <= e->_layoutBottom);
        }
        if (hit) return Object::InternalPtr<UI::DialogInstance>(dlg);
    }
    return Object::InternalPtr<UI::DialogInstance>();
}

// RETSLOT door (B5-12): `(this, ret, x, y)`.
extern "C" void* RtUi_UserInterface_GetFirstContainedDialog(
        const UserInterface* self, void* retval, int x, int y) {
    new (retval) Object::InternalPtr<UI::DialogInstance>(
        self->UI::UserInterface::GetFirstContainedDialog(x, y));
    return retval;
}

// ---- ?EndSpriteCapture@…@QEAAXXZ  0x18011f4a0 ord 2316 ---------------------
//   MOV qword ptr [RCX+0x110],0x0 / RET
// It does NOT touch the capture viewport at +0x118, and it does NOT tell the
// SpriteCapture object anything — Draw's `if (capture) EndAddingSprites` is
// what closes the capture out. Faithful: one store, nothing else.
void UI::UserInterface::EndSpriteCapture() {
    _spriteCapture = nullptr;
}

extern "C" void RtUi_UserInterface_EndSpriteCapture(UserInterface* self) {
    self->UI::UserInterface::EndSpriteCapture();
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
void UI::UserInterface::SetFullscreenFade(bool fadeIn, uint32_t color, bool immediate) {
    if (immediate) {
        _fadeColor  = color;
        _fadeAmount = fadeIn ? 1.0f : 0.0f;
    } else {
        _fadeVelocity = fadeIn ? 1.0f : -1.0f;
        _fadeColor    = color;
    }
}

extern "C" void RtUi_UserInterface_SetFullscreenFade(
        UserInterface* self, bool fadeIn, uint32_t color, bool immediate) {
    self->UI::UserInterface::SetFullscreenFade(fadeIn, color, immediate);
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
void UI::UserInterface::SetFullscreenFadeSprite(const Sprite* sprite) {
    std::memcpy(&_fadeSpriteSheet, sprite, 0x10);
}

extern "C" void RtUi_UserInterface_SetFullscreenFadeSprite(
        UserInterface* self, const void* sprite) {
    self->UI::UserInterface::SetFullscreenFadeSprite(static_cast<const UI::Sprite*>(sprite));
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
// with NO call into the shipped body at all, because for each of the four a
// second execution is not a comparison, it is a second side effect.
// Per function:
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
// Three of these four have an early-out that dominates in normal play: `Play`
// returns immediately for any UI sound id with no sound bound, and
// `EnableCursor` returns immediately whenever the requested state already
// matches.
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
void UI::UserInterface::AddDialog(Object::InternalPtr<DialogInstance>* dialog) {
    // DialogInstance vtable slot 4 (+0x20) `OnOpened` — a VIRTUAL call, so it
    // dispatches through the dialog's own (minted or exe hand-built) table.
    dialog->Get()->OnOpened();

    // retail 0x18007a0d0 — kSpec_Fill8Z, trait-equivalent for
    // `DialogInstancePtr` (a pointer): `_addDialogs` is that trait-derived
    // `Array<DialogInstancePtr>` instantiation already. The fill source is the
    // caller's InternalPtr itself (retail hands the same RDX to the push).
    _addDialogs.SetSizeFill(_addDialogs._size + 1,
                            reinterpret_cast<const DialogInstancePtr*>(dialog));
    // no early-out: every call does the whole body
}

extern "C" void RtUi_UserInterface_AddDialog(UserInterface* self, void** dlgPtr) {
    self->UI::UserInterface::AddDialog(
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr));
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
    self->UI::UserInterface::Play(id);
}

void UI::UserInterface::Play(int32_t id) {

    void* sound = _sounds[id];
    if (!sound) {  return; }

    uint8_t* scene = reinterpret_cast<uint8_t*>(_audioScene);
    if (!scene) {  return; }

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
    if (index < 0) {  return; }   // table full: retail drops it

    uint8_t* slot = table + static_cast<int64_t>(index) * 0x70;
    *reinterpret_cast<void**>(slot)          = sound;
    *reinterpret_cast<int64_t*>(slot + 0x10) = ticks;
    *reinterpret_cast<int32_t*>(slot + 0x08) = 1;

    const int32_t counter = ++*reinterpret_cast<int32_t*>(scene + 0x08);
    *reinterpret_cast<int32_t*>(slot + 0x68) = (index * 2) | (counter << 9);

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
    self->UI::UserInterface::SetCursor(
        Object::ExternalPtr<UI::Cursor>(static_cast<UI::Cursor*>(cursor)));
}

void UI::UserInterface::SetCursor(Object::ExternalPtr<Cursor> cursorPtr) {
    // `ExternalPtr` by value is the bare pointer in RDX (banner, B6-11).
    void* const cursor = cursorPtr.Get();

    _cursor = cursor;
    if (!cursor) {  return; }

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
// the lane banner.
extern "C" void RtUi_UserInterface_EnableCursor(UserInterface* self, bool enable) {
    self->UI::UserInterface::EnableCursor(enable);
}

void UI::UserInterface::EnableCursor(bool enable) {

    _cursorEnabled = static_cast<uint8_t>(enable);

    uint8_t* config = *reinterpret_cast<uint8_t**>(InputConfigSingleton());
    uint8_t* app    = *reinterpret_cast<uint8_t**>(ApplicationSingleton());

    uint8_t wanted = static_cast<uint8_t>(enable);
    if (*reinterpret_cast<const uint8_t*>(config + 0x0a) != 0) wanted = 0;

    if (*reinterpret_cast<const uint8_t*>(app + 0x10) == wanted) {
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
    if (!node) {  return; }

    void* window  = *reinterpret_cast<void**>(app);
    void* handler = *reinterpret_cast<void**>(node + 0x08);
    reinterpret_cast<void (*)(void*, uint32_t, uint64_t, uint64_t)>(handler)(
        window, 0x20u, 0, 0);

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
// ---- ?BringDialogToFront@UserInterface@UI@@QEAAXV?$InternalPtr@
//      VDialogInstance@UI@@@Object@@@Z   0x180120440  ord 1672 --------------
//                                                        WAVE 9 LANE UIC
//
// Move one dialog to the FRONT of `_dialogs`, which per
// runtime/types/ui/UserInterface.h means the TAIL: `_head` is the backmost
// dialog and `_tail` the frontmost. Retail, all of it:
//
//     18012044a  MOV R8,[RCX+0x8]      ; tail
//     18012044e  MOV RAX,[RDX]         ; the DialogInstance* inside the ptr
//     180120457  CMP [R8],RAX / JZ ret ; already frontmost -> nothing
//     18012045c  CALL 0x18007a010      ; LinkedList::Remove(this, ptr)
//     180120464  MOV RDX,RDI / CALL 0x180032750   ; node = NewNode(ptr)
//     180120469  MOV RCX,[RBX+0x8]     ; tail, RE-READ after the Remove
//     180120472  MOV [RCX+0x10],RAX    ; tail->next = node
//     18012047a  MOV [RAX+0x8],RCX     ; node->prev = tail
//     18012047e  INC [RBX+0x10] / MOV [RBX+0x8],RAX
//   (tail was null:)
//     180120490  INC [RBX+0x10] / MOV [RBX],RAX / MOV [RBX+0x8],RAX
//
// THE FIRST DEREFERENCE IS UNGUARDED AND RETAIL MEANS IT. `CMP [R8],RAX` reads
// through the tail with no null test, so calling this on an EMPTY dialog list
// faults. That is load-bearing (it asserts the caller already added the
// dialog), and CLAUDE.md's rule applies: a guard here would skip the behaviour
// rather than restore it. Reproduced. Note the irony that the code four
// instructions later DOES test the tail — because `Remove` may have emptied the
// list in between.
//
// THE ARGUMENT IS A POINTER TO AN InternalPtr, NOT THE DIALOG. `MOV RAX,[RDX]`
// dereferences it once, and both callees take the same `&value`.
// engine/funcs/engine_funcs.h:2460 already declares the two-argument shape, and
// counting arguments against the mangled name agrees (one by-value class
// argument, no hidden return slot).
//
// `CALL 0x180032750` IS MADE WITHOUT SETTING RCX. That is not a transcription
// gap: the callee never reads RCX (`XOR ECX,ECX` destroys it before any use),
// which is settled by reading the callee, and is written up at the port in
// runtime/src/collection/linkedlist.cpp.
//
// Both internals are ported by this lane and registered by address, so this
// body introduces no unsatisfied callee: 0x18007a010 and 0x180032750 (and
// 0x180009c30 underneath them) are all in linkedlist.cpp.
//
// ORACLE: NONE, and none is manufactured. The body is a list mutation whose own
// first test is "already frontmost" — so a replay finds the dialog already at
// the tail and does NOTHING, comparing an untouched list against itself and
// reporting green for any port at all. That is the same false-pass shape
// WAVE9.md records for `Audio::Scene::Pause`'s early return. Ships as a
// no-oracle port with NO call into the shipped body. `worked=` reports whether
// the call actually relinked, so `worked=0` everywhere means the population
// never brought a background dialog forward and the port is UNEXERCISED, not
// verified (reference_uncalled_port_is_not_verified.md).
extern "C" void RtUi_UserInterface_BringDialogToFront(UserInterface* self,
                                                      void** dlgPtr) {
    self->UI::UserInterface::BringDialogToFront(
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr));
}

void UI::UserInterface::BringDialogToFront(Object::InternalPtr<DialogInstance>* dialog) {

    // ENDGAME B.3.2 step 2 — `_dialogs` is the real
    // `Collection::LinkedList<DialogInstancePtr>` member (UserInterface.h,
    // +0x000), the ONE retail instantiation of the class. The unguarded tail
    // read below is retail's own (CLAUDE.md: a faithful crash on an empty
    // list is not ours to fix) — `GetLast()` returns a null-node `Iterator`
    // exactly like the raw `_tail` this used to read, so dereferencing it
    // still faults the same way on an empty list.
    void* dlg = dialog->Get();
    if (*_dialogs.GetLast() == dlg) {
        return;
    }

    // retail 0x18007a010 (Remove) then 0x180032750 (NewNode) + the inlined
    // tail-append — that whole sequence, re-reading `_tail` after Remove, IS
    // `LinkedList<T>::Add`'s own body (Collection.h), so the conversion is
    // Remove-then-Add rather than a re-derivation of the append.
    _dialogs.Remove(dlg);
    _dialogs.Add(dlg);
}

// ============================================================================
// WAVE 10 LANE UIF — ??1UserInterface@UI@@QEAA@XZ   0x180120b90  ord 705
// ============================================================================
//
// WHY THIS ROW IS OPEN NOW AND WAS NOT BEFORE. Wave 7 lane C held it back with
// three reasons, quoted at the top of this file: "Win32 in a non-_win file, the
// Wave 2 allocator (NO-GO), and no oracle: a destructor runs once." All three
// are answered rather than waived:
//
//   * WIN32. Retail INLINES `EnterCriticalSection` / `MemoryAllocator::Free`
//     (0x1800c6c00) / `LeaveCriticalSection` at all four of this body's frees.
//     The port calls the exported STATIC `?MemFree@MemoryAllocator@System@@
//     SAXPEAX@Z` (0x1800c7ad0), which is the out-of-line spelling of exactly
//     that inline — the same substitution `??1DialogInstance@` already makes in
//     runtime/src/ui/dialog.cpp, and it keeps <windows.h> out of this file
//     (PLAN §4.1 decision 1) while taking the SAME critical section.
//   * THE ALLOCATOR NO-GO. Opened by Wave 10 lane ALLOC
//     (docs/runtime_port/WAVE10_LANE_ALLOC.md): 27 exports registered, so
//     there is ONE heap. Note what this port actually needs from that. It frees
//     through whichever `MemFree` is live, and the blocks it frees were
//     allocated through the same one. The flag is a variable this lane's runs
//     sweep, not a dependency this body has.
//   * "A DESTRUCTOR RUNS ONCE" — true, and it rules out a REPLAY. It does not
//     rule out an oracle. See the twin-synthetic oracle below, which builds two
//     byte-identical synthetic subjects out of the real allocator, runs OUR
//     body on one and the SHIPPED body on the other, and compares both the
//     0x128-byte object and the ORDERED LOG of destructor dispatches.
//
// LANE ALLOC'S ONE STILL-STANDING CAVEAT DOES NOT REACH THIS ROW. Its §6 says
// "do NOT arm `Shutdown`/`~MemoryAllocator` into lane N's open teardown crash".
// That is the ALLOCATOR'S OWN teardown pair. `~UserInterface` is a client of the
// allocator, not part of it: it takes the allocator's lock and returns blocks,
// which the allocator has been doing for 900k operations per 30 s run under the
// armed configuration (lane ALLOC §5b). Nothing here arms `Shutdown`.
//
// ---------------------------------------------------------------------------
// THE BODY, from the disassembly at 0x180120b90 (0x243 bytes, tail-JMP at
// 0x180120dd2). Four phases:
//
//   1. For each of the `_removedDialogs[i]` (i < _removedDialogs._size):
//        find the `_dialogs` node whose element is that dialog; unlink it from
//        head/tail/prev/next, null the node's own two links, `--_count`, and
//        FREE THE NODE. Then re-read `_removedDialogs._data` (retail re-loads
//        it at 0x180120c74, AFTER the free) and, unless the dialog's flag bit 5
//        is set, run its deleting destructor and null the array slot.
//   2. Walk whatever is LEFT of `_dialogs` and run each remaining dialog's
//        deleting destructor, nulling the node's element. THE NODES ARE NOT
//        FREED HERE — phase 4 does that.
//   3. Free `_events._data` (+0x48), then `_addDialogs._data` (+0x28), then
//        `_removedDialogs._data` (+0x18), zeroing each pointer AND the qword
//        that follows it (size+capacity together) in that order.
//   4. TAIL-JMP to `Collection::LinkedList<8>::Clear` (0x1800325f0), which
//        frees the surviving nodes and zeroes head/tail/count.
//
// TWO TRANSCRIPTION TRAPS, both taken from the DISASSEMBLY and not from the
// decompiled C (reference_ghidra_c_output_vs_disassembly.md):
//
//   (a) THE FLAG WORD IS READ BEFORE THE NULL TEST, in both loops:
//           180120c78  MOV RAX,[RBX+R14]        ; the dialog
//           180120c7c  MOV ECX,[RAX+0x20]       ; <-- deref
//           180120c8c  TEST RAX,RAX             ; <-- null test, AFTER
//       This is an unguarded deref that retail relies on. It is reproduced in
//       that order. CLAUDE.md's rule applies: a null there is an upstream bug,
//       and a guard would not restore the behaviour, it would skip it.
//   (b) `_removedDialogs._data` IS RE-READ after the node free (0x180120c74),
//       so a body that caches it across the free is wrong in exactly the way a
//       reallocating allocator would expose. The port re-reads.
//
// EVERY CALLEE IS SATISFIED, so this row CLOSES rather than moving one frame
// deeper:
//   * 0x1800c7ad0 `MemFree`                — ported, Wave 9 lane Q
//   * 0x1800325f0 `LinkedList<8>::Clear`   — ported, runtime/src/collection/
//                                            linkedlist.cpp:334
//   * vtable slot 0 of each dialog         — the deleting destructor, which is
//                                            the DLL's (or the mint's) and is
//                                            not ours to satisfy either way.
// ---------------------------------------------------------------------------

namespace {

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` 0x1800c7ad0 — retail's four
// inlined lock+Free sequences, out of line. Reached through the PORT-AWARE door
// so that lane E's port of it is what runs when it is registered.
// ENDGAME B.3.6: a namespace-scope atomic slot (RULE B6-5), not RT_EXPORT_ONCE's
// function-local static — the member bodies below reach it.
using UiVoidPtrFn = void (*)(void*);
inline void UiMemFree(void* p) {
    reinterpret_cast<UiVoidPtrFn>(rtuielem::AtomicExport(
        g_memFreeSlot, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(p);
}

// ENDGAME B.3.2 step 2 — the 0x1800325f0 forwarder (`UiListClear`) that used
// to sit here is GONE; its one caller now calls `self->_dialogs.Clear()`
// directly (see `UiDtorBody`'s phase 4).

// Slot 0 of any Object::Base-derived vtable is the scalar deleting destructor;
// retail calls it with EDX = 1 ("destruct AND free"). ENDGAME B.3.6: that is
// `Object::Base::DeletingDtor(unsigned)`, slot 0 of the real `UI::DialogInstance`
// — a VIRTUAL call, dispatched through the dialog's own (minted or exe) table.
inline void UiDeleteDialog(void* dlg) {
    reinterpret_cast<UI::DialogInstance*>(dlg)->DeletingDtor(1);   // B.3.6 bridge
}

// ---- the NEGATIVE CONTROL knob ---------------------------------------------
// `BANISHED_UIF_NEGCTRL=<n>` deliberately breaks ONE thing in one of this
// lane's three ports, so that the three twin oracles can be shown to CATCH a
// defect rather than merely to report zero. A green oracle that has never
// failed is an untested instrument, and this project has paid for that twice
// (reference_probe_needs_an_effectiveness_control.md, and
// reference_control_must_pick_a_compared_subject.md on choosing a subject the
// comparison actually looks at — each of the three below is chosen inside the
// compared extent, not beside it).
//
//   1  ~UserInterface        ignore the dialog's flag bit 5, so the dialog a
//                            nested ShowDialog UI owns is destroyed anyway.
//                            Must show up in the twin's ORDERED LOG.
//   2  CreateInstance        (runtime/src/ui/dialog.cpp) drop the _id/_tabOrder
//                            crossover. Must show up in the BYTE compare.
//   3  SaveAllSettings       skip the cursor advance past the 0xdeadbeef magic.
//                            Must show up in the SETTINGS BUFFER compare.
//
// Unset (the normal case) every one of these is a compile-time-visible no-op
// branch on a value read once.
// Defined below at file scope so runtime/src/ui/dialog.cpp's CreateInstance can
// use the same knob for control 2.
// The body, factored out so the twin oracle can run OUR implementation on a
// synthetic subject without re-entering the diff wrapper.
void UiDtorBody(UserInterface* self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);

    // ---- phase 1 ----------------------------------------------------------
    if (*reinterpret_cast<int32_t*>(p + 0x20) > 0) {
        int32_t  i   = 0;
        uint64_t off = 0;
        do {
            uint8_t* removed = *reinterpret_cast<uint8_t**>(p + 0x18);
            uint8_t* head    = *reinterpret_cast<uint8_t**>(p + 0x00);
            uint8_t* node    = head;
            if (node) {
                void* want = *reinterpret_cast<void**>(removed + off);
                for (;;) {
                    if (*reinterpret_cast<void**>(node) == want) {
                        if (node == head)
                            *reinterpret_cast<void**>(p + 0x00) =
                                *reinterpret_cast<void**>(node + 0x10);
                        if (node == *reinterpret_cast<uint8_t**>(p + 0x08))
                            *reinterpret_cast<void**>(p + 0x08) =
                                *reinterpret_cast<void**>(node + 0x08);
                        if (uint8_t* prev = *reinterpret_cast<uint8_t**>(node + 0x08))
                            *reinterpret_cast<void**>(prev + 0x10) =
                                *reinterpret_cast<void**>(node + 0x10);
                        if (uint8_t* next = *reinterpret_cast<uint8_t**>(node + 0x10))
                            *reinterpret_cast<void**>(next + 0x08) =
                                *reinterpret_cast<void**>(node + 0x08);
                        *reinterpret_cast<void**>(node + 0x08) = nullptr;
                        *reinterpret_cast<void**>(node + 0x10) = nullptr;
                        --*reinterpret_cast<int32_t*>(p + 0x10);
                        UiMemFree(node);
                        break;
                    }
                    node = *reinterpret_cast<uint8_t**>(node + 0x10);
                    if (!node) break;
                }
            }

            // RE-READ, at 0x180120c74, after the free. See trap (b).
            uint8_t* rd  = *reinterpret_cast<uint8_t**>(p + 0x18);
            uint8_t* dlg = *reinterpret_cast<uint8_t**>(rd + off);
            // Trap (a): the flag word is read BEFORE the null test, as retail
            // does. Kept in that order deliberately.
            const uint32_t flags = *reinterpret_cast<uint32_t*>(dlg + 0x20);
            if (((~static_cast<uint8_t>(flags >> 5)) & 1) != 0 && dlg != nullptr) {
                UiDeleteDialog(dlg);
                *reinterpret_cast<void**>(rd + off) = nullptr;
            }

            ++i;
            off += 8;
        } while (i < *reinterpret_cast<int32_t*>(p + 0x20));
    }

    // ---- phase 2 ----------------------------------------------------------
    for (uint8_t* node = *reinterpret_cast<uint8_t**>(p + 0x00); node;
         node = *reinterpret_cast<uint8_t**>(node + 0x10)) {
        uint8_t* dlg = *reinterpret_cast<uint8_t**>(node);
        const uint32_t flags = *reinterpret_cast<uint32_t*>(dlg + 0x20);
        if (((~static_cast<uint8_t>(flags >> 5)) & 1) != 0 && dlg != nullptr) {
            UiDeleteDialog(dlg);
            *reinterpret_cast<void**>(node) = nullptr;
        }
    }
    // ---- phase 3 ----------------------------------------------------------
    // Retail's order is events, addDialogs, removedDialogs, and each pair of
    // stores is two QWORDs — the second covers `_size` AND `_capacity`.
    if (void* ev = *reinterpret_cast<void**>(p + 0x48)) UiMemFree(ev);
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;

    if (void* ad = *reinterpret_cast<void**>(p + 0x28)) UiMemFree(ad);
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;

    if (void* rm = *reinterpret_cast<void**>(p + 0x18)) UiMemFree(rm);
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;

    // ---- phase 4: the tail JMP -------------------------------------------
    // retail 0x1800325f0 — `self->_dialogs` is the real
    // `Collection::LinkedList<DialogInstancePtr>` member; `.Clear()` IS that
    // body (Collection.h's banner: it frees every node, then zeroes
    // head/tail/count).
    self->_dialogs.Clear();
}

// ============================================================================
// UI leftovers batch (docs/runtime_port/closure/WORKLIST.md) — the three
// remaining UserInterface frame bodies.
// ============================================================================

// Drop every pending `_events` entry (stride 0x30) whose dialog pointer at
// +0x00 matches `dlg`, shifting later entries down in place. Retail's own
// loop (0x18011f88a..0x18011f947) is a field-by-field copy of event[i+1] onto
// event[i] repeated in place rather than a single memmove, but the net effect
// over the whole run is identical to erasing every match and closing the gap,
// so it is written as one for clarity — the field layout it copies is exactly
// AssignEvent30's (array.cpp), which is the byte-level cross-check.
void RemoveEventsForDialog(uint8_t* p, void* dlg) {
    uint8_t* data = *reinterpret_cast<uint8_t**>(p + 0x48);
    int32_t* size = reinterpret_cast<int32_t*>(p + 0x50);
    for (int32_t i = 0; i < *size; ) {
        if (*reinterpret_cast<void**>(data + static_cast<size_t>(i) * 0x30) == dlg) {
            std::memmove(data + static_cast<size_t>(i) * 0x30,
                         data + static_cast<size_t>(i + 1) * 0x30,
                         static_cast<size_t>(*size - i - 1) * 0x30);
            --*size;
            // do not advance i -- re-check the slot the shift just filled
        } else {
            ++i;
        }
    }
}

// ?CleanRemovedDialogs@UserInterface@UI@@AEAAXXZ — retail 0x18011f790, ord 1704.
// For each dialog in `_removedDialogs` (+0x18, size +0x20): unlink it from
// `_dialogs` (same walk-and-splice UiDtorBody's phase 1 above already does,
// which is this same body's inlined shape one caller over), drop any pending
// `_events` entry that names it, then release it through its own vtable slot
// 0 UNLESS flag bit 5 says a nested ShowDialog UI owns it (the same bit
// UiDtorBody's phase 1 tests, same "read the flags before the null test"
// order retail uses). Finishes by clearing `_removedDialogs` to empty --
// `FUN_180079c90` (array.cpp's registration note), which is PROVEN to reduce
// to `_size = 0` and nothing else: its fill loop's bound folds to `< 0` for a
// compile-time-constant new size of 0, which is never true for a
// non-negative old size, so the loop, the realloc arm and the free arm are
// all unreachable.
} // namespace

void UI::UserInterface::CleanRemovedDialogs() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    const int32_t removedCount = *reinterpret_cast<int32_t*>(p + 0x20);
    for (int32_t i = 0; i < removedCount; ++i) {
        uint8_t* removed = *reinterpret_cast<uint8_t**>(p + 0x18);
        void* want = *reinterpret_cast<void**>(removed + static_cast<size_t>(i) * 8);

        for (uint8_t* node = *reinterpret_cast<uint8_t**>(p + 0x00); node;
             node = *reinterpret_cast<uint8_t**>(node + 0x10)) {
            if (*reinterpret_cast<void**>(node) != want) continue;
            if (node == *reinterpret_cast<uint8_t**>(p + 0x00))
                *reinterpret_cast<void**>(p + 0x00) = *reinterpret_cast<void**>(node + 0x10);
            if (node == *reinterpret_cast<uint8_t**>(p + 0x08))
                *reinterpret_cast<void**>(p + 0x08) = *reinterpret_cast<void**>(node + 0x08);
            if (uint8_t* prev = *reinterpret_cast<uint8_t**>(node + 0x08))
                *reinterpret_cast<void**>(prev + 0x10) = *reinterpret_cast<void**>(node + 0x10);
            if (uint8_t* next = *reinterpret_cast<uint8_t**>(node + 0x10))
                *reinterpret_cast<void**>(next + 0x08) = *reinterpret_cast<void**>(node + 0x08);
            *reinterpret_cast<void**>(node + 0x08) = nullptr;
            *reinterpret_cast<void**>(node + 0x10) = nullptr;
            --*reinterpret_cast<int32_t*>(p + 0x10);
            UiMemFree(node);
            break;
        }

        RemoveEventsForDialog(p, want);

        // RE-READ, exactly as UiDtorBody's own phase 1 does, in case any of
        // the above moved the array (it does not, but the re-read is free and
        // keeps this body honest against retail's own instruction order).
        uint8_t* dlg = *reinterpret_cast<uint8_t**>(
            *reinterpret_cast<uint8_t**>(p + 0x18) ? *reinterpret_cast<uint8_t**>(p + 0x18) + static_cast<size_t>(i) * 8 : nullptr);
        const uint32_t flags = *reinterpret_cast<uint32_t*>(dlg + 0x20);   // trap (a): read before the null test
        if (((~static_cast<uint8_t>(flags >> 5)) & 1) != 0 && dlg != nullptr) {
            UiDeleteDialog(dlg);
            *reinterpret_cast<void**>(*reinterpret_cast<uint8_t**>(p + 0x18) + static_cast<size_t>(i) * 8) = nullptr;
        }
    }

    *reinterpret_cast<int32_t*>(p + 0x20) = 0;   // FUN_180079c90 -- see banner
}

// ?UpdateEvents@UserInterface@UI@@QEAAXXZ — retail 0x18011fc60, ord 4515.
// Re-entrancy guarded by +0x88 (set for the duration of the drain, exactly
// like retail). Dispatches each queued event through its own dialog's
// OnElementEvent (vtable slot 8/+0x40 -- runtime/src/ui/dialog.cpp's
// RtUiDialogInstance_OnElementEvent is that slot's body), handing it a FRESH
// stack copy of the event's +0x08 field by address on every iteration (the
// same "controller must not see the caller's own storage" contract
// RtUiDialogInstance_OnElementEvent's own banner states for its element
// argument). Finishes by clearing `_events` -- `FUN_180079df0`, proven the
// same way `FUN_180079c90` is above: the dead-branch reasoning is identical,
// just at stride 0x30 instead of 8.
void UI::UserInterface::UpdateEvents() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    if (p[0x88] != 0) return;
    p[0x88] = 1;

    const int32_t count = *reinterpret_cast<int32_t*>(p + 0x50);
    uint8_t* data = *reinterpret_cast<uint8_t**>(p + 0x48);
    for (int32_t i = 0; i < count; ++i) {
        uint8_t* ev = data + static_cast<size_t>(i) * 0x30;
        void* dlg = *reinterpret_cast<void**>(ev);
        // The fresh stack copy of the event's element, passed BY ADDRESS
        // (RULE B6-11: slot 8's InternalPtr<Element> argument is a hidden
        // pointer to the caller's temporary).
        Object::InternalPtr<UI::Element> elem(*reinterpret_cast<UI::Element**>(ev + 0x08));
        // DialogInstance vtable slot 8 (+0x40) `OnElementEvent` — VIRTUAL, so
        // an exe dialog's hand-built slot 8 is what runs.
        reinterpret_cast<UI::DialogInstance*>(dlg)->OnElementEvent(&elem,   // B.3.6 bridge
            *reinterpret_cast<int32_t*>(ev + 0x10), *reinterpret_cast<int32_t*>(ev + 0x14),
            *reinterpret_cast<int32_t*>(ev + 0x18), *reinterpret_cast<int32_t*>(ev + 0x1c),
            *reinterpret_cast<int32_t*>(ev + 0x20), *reinterpret_cast<void**>(ev + 0x28));
    }

    *reinterpret_cast<int32_t*>(p + 0x50) = 0;   // FUN_180079df0 -- see banner
    p[0x88] = 0;
}

// ?GetMouseCursorIndex@UserInterface@UI@@QEBAHHH@Z — retail 0x18011fa40, ord
// 2800. Walks `_dialogs` back-to-front (from _tail, +0x08, via each node's
// `prev` at +0x08 -- the same front-to-back convention UserInterface.h
// documents for BringDialogToFront/GetFirstContainedDialog), returns 0 if
// none contains (x,y), otherwise defers to that dialog's OWN
// GetMouseCursorIndex starting from its root Element.
int32_t UI::UserInterface::GetMouseCursorIndex(int32_t x, int32_t y) const {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(this);
    for (const uint8_t* node = *reinterpret_cast<uint8_t* const*>(p + 0x08); node;
         node = *reinterpret_cast<uint8_t* const*>(node + 0x08)) {
        void* dlg = *reinterpret_cast<void* const*>(node);
        const uint8_t* d = static_cast<const uint8_t*>(dlg);
        const uint32_t flags = *reinterpret_cast<const uint32_t*>(d + 0x20);
        if ((flags >> 2) & 1) continue;
        const uint8_t* root = *reinterpret_cast<uint8_t* const*>(d + 0x50);
        bool hit;
        if ((flags >> 3) & 1) {
            // Element vtable slot 9 (+0x48) `Contains` — VIRTUAL.
            hit = reinterpret_cast<const UI::Element*>(root)->Contains(      // B.3.6 bridge
                x, y, *reinterpret_cast<UserInterface* const*>(d + 0x10));
        } else {
            const int32_t left   = *reinterpret_cast<const int32_t*>(root + 0x68);
            const int32_t right  = *reinterpret_cast<const int32_t*>(root + 0x70);
            const int32_t top    = *reinterpret_cast<const int32_t*>(root + 0x6c);
            const int32_t bottom = *reinterpret_cast<const int32_t*>(root + 0x74);
            hit = left <= x && x <= right && top <= y && y <= bottom;
        }
        if (hit) {
            if (!dlg) return 0;
            // The dialog's own `GetMouseCursorIndex(InternalPtr<Element>, x, y)`
            // (0x180114010), a non-virtual member of the real class now.
            Object::InternalPtr<UI::Element> element(
                *reinterpret_cast<UI::Element* const*>(d + 0x50));
            return reinterpret_cast<const UI::DialogInstance*>(d)                // B.3.6 bridge
                ->GetMouseCursorIndex(element, x, y);
        }
    }
    return 0;
}

extern "C" void RtUi_UserInterface_CleanRemovedDialogs(UserInterface* self) {
    self->UI::UserInterface::CleanRemovedDialogs();
}

extern "C" void RtUi_UserInterface_UpdateEvents(UserInterface* self) {
    self->UI::UserInterface::UpdateEvents();
}

extern "C" int32_t RtUi_UserInterface_GetMouseCursorIndex(const UserInterface* self,
                                                          int32_t x, int32_t y) {
    return self->UI::UserInterface::GetMouseCursorIndex(x, y);
}

extern "C" void RtUi_UserInterface_Dtor(UserInterface* self) {
    // `worked` is the only thing a LIVE call can report: there is no comparison
    // to make against a destructor that runs once, so `calls=` stays 0 here on
    // purpose and the CORRECTNESS evidence is the twin oracle below, which runs
    // from RtUiManagerRegister. reference_calls_zero_can_mean_always_took_the_
    // uncovered_branch.md: this is the "no comparison exists" cause of calls=0,
    // and it is named here rather than left for a reader to guess.
    const bool didWork = (*reinterpret_cast<int32_t*>(
                              reinterpret_cast<uint8_t*>(self) + 0x20) > 0) ||
                         (*reinterpret_cast<void**>(self) != nullptr);
    (void)didWork;   // the reporting that read it is gone; kept as the banner's statement
    UiDtorBody(self);
}

// ============================================================================
// WAVE 10 LANE UIF — ?SaveAllSettings@UserInterface@UI@@QEAAXXZ
//                                                 0x1801204b0  ord 3608
// ============================================================================
//
// "Serialize every open dialog's settings into that dialog's OWN settings
// buffer." It is a PURE SIDE EFFECT: it returns void, it touches no field of
// the UserInterface, and everything it does lands in memory somebody else owns.
// reference_port_must_reproduce_side_effects.md is the whole risk here, and it
// is why the oracle below diffs the BUFFERS and not the object.
//
// WHY IT WAS BLOCKED AND WHY IT IS NOT NOW. Wave 7 lane C's one-line reason was
// "Builds an IO::MemoryStream and serialises every dialog's settings.
// Allocates, frees, and writes persisted state." Two of those three are the
// allocator NO-GO, retired by Wave 10 lane ALLOC. The third — "writes persisted
// state" — turned out to be the thing that MAKES it verifiable rather than the
// thing that blocks it: nothing here reaches a file. `SaveAllSettings` writes
// into `DialogInstance::_userData` (+0xa8), a caller-owned byte buffer whose
// length is `+0xb0`. A buffer can be snapshotted, restored and re-written; a
// file handle cannot. reference_write_path_serialize_is_replayable.md, for the
// fifth time in this project.
//
// ---------------------------------------------------------------------------
// THE TWO FIELDS AT +0xa8/+0xb0 ARE NOT WHAT DialogInstance.h CALLS THEM
// ---------------------------------------------------------------------------
// runtime/types/ui/DialogInstance.h names them `_userData` (void*) and
// `_userFlags` (int) — correctly, from the CONSTRUCTOR, where they are
// parameters 5 and 6. This body reads them as `(settingsBuffer, byteCount)`:
//
//     1801204f8  CMP qword ptr [R14 + 0xa8],0x0   ; skip the dialog if null
//     1801205ae  MOV EAX,dword ptr [R14 + 0xb0]   ; -> MemoryStream::size
//     1801205b5  MOV RDX,qword ptr [R14 + 0xa8]   ; -> MemoryStream::data
//     1801205d4  MOV dword ptr [RDX],0xdeadbeef   ; a MAGIC written INTO it
//
// Both readings are of the same two words. This port does not rename lane B's
// fields — it reads them at their offsets and records here what this caller
// means by them, which is the honest form when two callers disagree.
//
// ---------------------------------------------------------------------------
// THE BODY, from the disassembly at 0x1801204b0 (0x2fb bytes). Per dialog in
// `_dialogs`, skipping any whose +0xa8 is null:
//
//   1. Construct an `IO::MemoryStream` ON THE STACK, inlined. Its layout is
//      already recovered and static_assert'ed in runtime/types/io/Stream.h and
//      every offset this body uses agrees with it field for field:
//        vptr = ??_7Stream@IO@@6B@ (0x1801549a0); direction = 0; encoding = 0;
//        vptr = ??_7MemoryStream@IO@@6B@ (0x1801555b0); data = 0; ownsData = 0;
//        cursor:size = 0 (one qword covering +0x24 and +0x28).
//   2. Allocate an 8-byte `IO::Encoding` (`Allocate(_singleton, 8, 4)` under the
//      allocator's critical section) and stamp the SHARED, UNEXPORTED encoding
//      vtable 0x1801554a8 into it. If the allocation fails retail carries on
//      with a NULL encoding — reproduced. The old `stream.encoding` is freed
//      first (always null here; the store order is retail's).
//   3. `data = dialog->+0xa8; size = dialog->+0xb0; cursor = 0; direction = 1`.
//      Then, IF `size >= 4`, write the literal `0xdeadbeef` at buffer offset 0
//      and advance `cursor` to 4. The magic is the settings blob's header.
//   4. Build a stack `IO::Serializer` {&stream, nullptr} and ask the encoding
//      for a ReaderWriter: slot +0x20 (`CreateWriter`) when `direction != 0`,
//      slot +0x18 (`CreateReader`) otherwise. `direction` is 1 by step 3, so
//      the reader arm is DEAD in this function — transcribed anyway, because
//      the branch is in the shipped bytes (0x1801205f4).
//   5. `dialog->vtbl[+0x48](dialog, &stream)` — the dialog's own
//      OnSerializeSettings, taking the raw Stream.
//   6. `count = dialog->+0x80` (the `_controllers` array size), serialized
//      through `rw->vtbl[+0xa0]` (`ScalarInt`, runtime/types/io/Serializer.h's
//      `rw_vtbl` table). Then for each controller in `dialog->+0x78`:
//        id = ctl->vtbl[+0x10](ctl);        rw->vtbl[+0x98](rw, &id, nullptr);
//        a SECOND, throw-away ReaderWriter over the SAME stream;
//        ctl->vtbl[+0x50](ctl, &stream);    then delete that ReaderWriter.
//      The controller pointer is RE-READ from +0x78 after the ReaderWriter is
//      created (0x1801206a1), not cached across it — transcribed.
//   7. Delete the top-level ReaderWriter, then run the inlined
//      `~MemoryStream`: `if (ownsData && data) MemFree(data)` — DEAD here,
//      `ownsData` is never set — and `if (encoding) MemFree(encoding)`.
//
// EVERY CALLEE IS SATISFIED — the admission test:
//   * `MemAlloc`/`MemFree` (0x1800c7a50 / 0x1800c7ad0)  — ported, lane Q, and
//     they are the out-of-line spelling of retail's four inlined
//     lock+Allocate/Free sequences (the same substitution `??1DialogInstance@`
//     makes in runtime/src/ui/dialog.cpp).
//   * `??_7Stream@IO@@6B@` / `??_7MemoryStream@IO@@6B@` — both EXPORTED data
//     rows, taken out of the DLL's own table rather than minted.
//   * the shared encoding vtable 0x1801554a8 — UNEXPORTED, so the address door
//     is the only one, exactly as runtime/src/io/compressedstream.cpp:800
//     already reaches it; lane C2 mints that row by address.
//   * encoding slots +0x18/+0x20 (`CreateReader`/`CreateWriter`, 0x1800a8120 /
//     0x1800a8080) — both PORTED in runtime/src/io/encoding.cpp.
//   * the dialog's +0x48 and each controller's +0x10/+0x50/+0x00 — virtuals on
//     objects we do not own, unchanged by this port either way.
// ---------------------------------------------------------------------------

namespace {

// `IO::Serializer` — {Stream*, ReaderWriter*}, 0x10 bytes
// (runtime/types/io/Serializer.h). Built on the stack twice per controller.
struct UiSerializer { void* stream; void* rw; };

using UiCreateRwFn   = void* (*)(void* enc, UiSerializer* out, const char* name);

} // namespace

void UI::UserInterface::SaveAllSettings() {
    uint8_t* node = *reinterpret_cast<uint8_t**>(this);   // _dialogs._head
    if (!node) return;

    do {
        uint8_t* dlg = *reinterpret_cast<uint8_t**>(node);
        if (*reinterpret_cast<void**>(dlg + 0xa8) != nullptr) {
            // ---- 1+2. the inlined IO::MemoryStream and its 8-byte encoding -
            // ENDGAME B.3.9 IO (foreign site, IO's class): retail's inlined
            // ??0MemoryStream is the class's real constructor -- the Stream
            // base, the field stores, MemAlloc(8,4) + the Binary encoding (null
            // on a failed allocation), the dead free of the previous one.
            alignas(IO::MemoryStream) unsigned char stMem[sizeof(IO::MemoryStream)];
            IO::MemoryStream* const st = ::new (static_cast<void*>(stMem)) IO::MemoryStream();
            void* enc = st->_encoding;

            // ---- 3. point the stream at the dialog's settings buffer ------
            const int32_t size = *reinterpret_cast<int32_t*>(dlg + 0xb0);
            void*         buf  = *reinterpret_cast<void**>(dlg + 0xa8);
            st->_data      = buf;
            st->_size      = static_cast<uint32_t>(size);
            st->_cursor    = 0;
            st->_direction = 1;
            if (size >= 4) {
                *reinterpret_cast<uint32_t*>(buf) = 0xdeadbeefu;
                st->_cursor += 4;
            }

            // ---- 4. the top-level ReaderWriter ----------------------------
            UiSerializer ser{ st, nullptr };
            void* rw = nullptr;
            if (enc) {
                void** encVt = *reinterpret_cast<void***>(enc);
                UiCreateRwFn make = reinterpret_cast<UiCreateRwFn>(
                    st->_direction != 0 ? encVt[4]    // +0x20 CreateWriter
                                        : encVt[3]);  // +0x18 CreateReader
                rw = make(enc, &ser, nullptr);
            }
            ser.rw = rw;

            // ---- 5. the dialog's own OnSerializeSettings ------------------
            // DialogInstance vtable slot 9 (+0x48) — VIRTUAL.
            reinterpret_cast<UI::DialogInstance*>(dlg)->OnSerializeSettings(st);   // B.3.6 bridge

            // ---- 6. the controller loop -----------------------------------
            int32_t count = *reinterpret_cast<int32_t*>(dlg + 0x80);
            if (rw) {
                reinterpret_cast<IO::ReaderWriter*>(rw)->SerializeInt(        // B.3.x bridge
                    count, nullptr);                                           // +0xa0
            }
            for (int32_t i = 0; i < count; ++i) {
                uint8_t* arr = *reinterpret_cast<uint8_t**>(dlg + 0x78);
                void*    ctl = *reinterpret_cast<void**>(arr + i * 8);
                // DialogController vtable slot 2 (+0x10) `Type()` — VIRTUAL.
                int32_t  id  = static_cast<int32_t>(
                    reinterpret_cast<UI::DialogController*>(ctl)->Type());      // B.3.6 bridge
                if (rw) {
                    reinterpret_cast<IO::ReaderWriter*>(rw)->SerializeUInt(   // B.3.x bridge
                        reinterpret_cast<uint32_t&>(id), nullptr);             // +0x98
                }

                UiSerializer ser2{ st, nullptr };
                void* rw2 = nullptr;
                if (enc) {
                    void** encVt = *reinterpret_cast<void***>(enc);
                    UiCreateRwFn make2 = reinterpret_cast<UiCreateRwFn>(
                        st->_direction != 0 ? encVt[4] : encVt[3]);
                    rw2 = make2(enc, &ser2, nullptr);
                }
                ser2.rw = rw2;

                // RE-READ at 0x1801206a1, not cached across the CreateWriter.
                arr = *reinterpret_cast<uint8_t**>(dlg + 0x78);
                ctl = *reinterpret_cast<void**>(arr + i * 8);
                // DialogController vtable slot 10 (+0x50) `OnSerializeSettings`
                // (B6-1.2: one slot LATER than DialogInstance's) — VIRTUAL.
                reinterpret_cast<UI::DialogController*>(ctl)->OnSerializeSettings(st); // B.3.6 bridge

                if (rw2) {
                    reinterpret_cast<IO::ReaderWriter*>(rw2)->DeletingDtor(1); // B.3.x bridge
                }
            }

            // ---- 7. teardown ----------------------------------------------
            if (rw) {
                reinterpret_cast<IO::ReaderWriter*>(rw)->DeletingDtor(1);      // B.3.x bridge
            }
            // ~MemoryStream (dead buffer free: never owns) then ~Stream (the
            // encoding): the class's destructor chain, no free of the storage.
            st->IO::MemoryStream::DeletingDtor(0);
        }
        node = *reinterpret_cast<uint8_t**>(node + 0x10);
    } while (node);
}

extern "C" void RtUi_UserInterface_SaveAllSettings(UserInterface* self) {
    self->UI::UserInterface::SaveAllSettings();
}

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

    // ---- Wave 9 lane UIC ---------------------------------------------------
    // The BY-VALUE `V?$InternalPtr…` spelling. The by-reference `AEAV…` name
    // this project once asked for is NOT in the export table and resolved NULL
    // in silence for a whole wave (engine/funcs/engine_funcs.cpp:2934's comment
    // records that; reference_resolveexport_phantom_names.md). Checked against
    // the shipped export directory again before this line was written.
    RegisterRuntimeExport("?BringDialogToFront@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z",
                          (void*)&RtUi_UserInterface_BringDialogToFront); ++count;
    RegisterRuntimeAddr(0x180120440, (void*)&RtUi_UserInterface_BringDialogToFront);

    // ---- Wave 10 lane UIF --------------------------------------------------
    // `QEAA` is the NON-VIRTUAL spelling and it is the right one here: this
    // class has NO vtable (see runtime/types/ui/UserInterface.h) and the export
    // directory carries `??1UserInterface@UI@@QEAA@XZ` at 0x120b90 / ord 705,
    // checked with pefile before this line was written. `UEAA` would resolve
    // NULL in silence (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    RegisterRuntimeExport("??1UserInterface@UI@@QEAA@XZ",
                          (void*)&RtUi_UserInterface_Dtor); ++count;
    RegisterRuntimeAddr(0x180120b90, (void*)&RtUi_UserInterface_Dtor);

    RegisterRuntimeExport("?SaveAllSettings@UserInterface@UI@@QEAAXXZ",
                          (void*)&RtUi_UserInterface_SaveAllSettings); ++count;
    RegisterRuntimeAddr(0x1801204b0, (void*)&RtUi_UserInterface_SaveAllSettings);

    // ---- UI leftovers batch (docs/runtime_port/closure/WORKLIST.md) --------
    RegisterRuntimeExport("?CleanRemovedDialogs@UserInterface@UI@@AEAAXXZ",
                          (void*)&RtUi_UserInterface_CleanRemovedDialogs); ++count;
    RegisterRuntimeAddr(0x18011f790, (void*)&RtUi_UserInterface_CleanRemovedDialogs);

    RegisterRuntimeExport("?UpdateEvents@UserInterface@UI@@QEAAXXZ",
                          (void*)&RtUi_UserInterface_UpdateEvents); ++count;
    RegisterRuntimeAddr(0x18011fc60, (void*)&RtUi_UserInterface_UpdateEvents);

    RegisterRuntimeExport("?GetMouseCursorIndex@UserInterface@UI@@QEBAHHH@Z",
                          (void*)&RtUi_UserInterface_GetMouseCursorIndex); ++count;
    RegisterRuntimeAddr(0x18011fa40, (void*)&RtUi_UserInterface_GetMouseCursorIndex);

    // The twin-synthetic oracle is NOT called here — it needs the allocator,
    // which does not exist yet at registration time. It is triggered from the
    // first `RtUi_UserInterface_Ctor`; see the note at its forward declaration.

    return count;
}
