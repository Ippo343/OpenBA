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

#include "../../funcs/rt_port.h"
#include "io/Stream.h"          // WAVE 10 LANE UIF — SaveAllSettings' MemoryStream
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
// WAVE 10 LANE UIF. Oracle lookup only; the registration repeats the literal.
const char* const kSaveAllSettings = "?SaveAllSettings@UserInterface@UI@@QEAAXXZ";

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

// WAVE 10 LANE UIF. Defined at the bottom of this file; forward-declared here
// because it is TRIGGERED FROM THE CONSTRUCTOR and not from registration.
//
// ⚠ THIS PLACEMENT IS LOAD-BEARING AND IT WAS MEASURED, NOT GUESSED. The first
// version of this lane called it from `RtUiManagerRegister()` and the process
// died there, every time:
//
//     Exception 0xC0000005  Fault: WRITE of 0x0000000000000648
//     RCX=0000000000000640   [REDZONE-DUMP] no allocator singleton
//
// `RuntimePortInit()` runs long before `Engine::Startup` creates the allocator,
// so `System::MemoryAllocator::_singleton` is still NULL and retail's
// `EnterCriticalSection(_singleton + 0x640)` writes to address 0x648. Any
// oracle that needs the HEAP must therefore run from a live call site, not from
// a registration function. The constructor is the earliest one this class has,
// and every `UI::UserInterface` in the process goes through it.
void VerifyUserInterfaceDtorTwin();
void VerifySaveAllSettingsTwin();   // same trigger, same reason

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

        // WAVE 10 LANE UIF: the destructor's twin-synthetic oracle, run ONCE
        // from the first constructed UserInterface. See the forward
        // declaration above for why it cannot run at registration time.
        static bool twinDone = false;
        if (!twinDone) {
            twinDone = true;
            VerifyUserInterfaceDtorTwin();
            VerifySaveAllSettingsTwin();
        }
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
    RT_DIFF_BEGIN("UI::UserInterface::BringDialogToFront");

    // The list header is UserInterface+0x00; treat it as the three raw fields
    // rather than pulling Collection.h's template in for one use.
    uint8_t* list = reinterpret_cast<uint8_t*>(self);
    void**   tail = reinterpret_cast<void**>(list + 0x08);

    void* dlg = *dlgPtr;
    if (**reinterpret_cast<void***>(tail) == dlg) {   // unguarded, see above
        RT_DIFF_WORK(false);
        return;
    }

    using RemoveFn  = void  (*)(void*, void* const*);
    using NewNodeFn = void* (*)(void*, void* const*);
    static RemoveFn  remove  = nullptr;
    static NewNodeFn newNode = nullptr;
    if (!remove)  remove  = reinterpret_cast<RemoveFn>(
        ResolveAddr(Module::Runtime, 0x18007a010));
    if (!newNode) newNode = reinterpret_cast<NewNodeFn>(
        ResolveAddr(Module::Runtime, 0x180032750));

    remove(list, dlgPtr);
    uint8_t* node = static_cast<uint8_t*>(newNode(nullptr, dlgPtr));

    // `tail` is RE-READ here (0x180120469): Remove may have just emptied the
    // list. Reading the cached value instead is the one mistake this body
    // invites.
    uint8_t* t = *reinterpret_cast<uint8_t**>(list + 0x08);
    if (t) {
        *reinterpret_cast<void**>(t + 0x10) = node;                    // tail->next
        *reinterpret_cast<void**>(node + 0x08) =
            *reinterpret_cast<void**>(list + 0x08);                    // node->prev
        ++*reinterpret_cast<int32_t*>(list + 0x10);
        *reinterpret_cast<void**>(list + 0x08) = node;
    } else {
        ++*reinterpret_cast<int32_t*>(list + 0x10);
        *reinterpret_cast<void**>(list + 0x00) = node;
        *reinterpret_cast<void**>(list + 0x08) = node;
    }

    RT_DIFF_WORK(true);
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
//     (docs/runtime_port/WAVE10_LANE_ALLOC.md): `BANISHED_RT_ALLOC=1` now arms
//     with Runtime-x64.dll PRESENT, 27 exports registered and the atomic swap
//     taken, so there is ONE heap whether or not the flag is set. Note what
//     this port actually needs from that: it does NOT need the flag. It frees
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

// The negative-control knob itself, at file scope so both this file and
// runtime/src/ui/dialog.cpp read ONE value. `std::getenv` is cached
// process-wide (standalone/env_cache.h), and the result is cached again here.
extern "C" int RtUiUifNegCtrl() {
    static int v = -1;
    if (v < 0) {
        const char* e = std::getenv("BANISHED_UIF_NEGCTRL");
        v = (e && e[0]) ? std::atoi(e) : 0;
    }
    return v;
}

namespace {

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` 0x1800c7ad0 — retail's four
// inlined lock+Free sequences, out of line. Reached through the PORT-AWARE door
// so that lane E's port of it is what runs when it is registered.
using UiVoidPtrFn = void (*)(void*);
inline void UiMemFree(void* p) {
    RT_EXPORT_ONCE(UiVoidPtrFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// 0x1800325f0 — `Collection::LinkedList<T=8>::Clear`. UNEXPORTED, so the
// address door is the only one; port-aware, so `BANISHED_RT_LINKEDLIST=0` still
// routes it to the DLL exactly as runtime/src/ui/dialog.cpp's copy does.
inline void UiListClear(void* list) {
    static UiVoidPtrFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<UiVoidPtrFn>(
        ResolveAddr(Module::Runtime, 0x1800325f0));
    if (fn) fn(list);
}

// Slot 0 of any Object::Base-derived vtable is the scalar deleting destructor;
// retail calls it with EDX = 1 ("destruct AND free").
using UiDeletingDtorFn = void (*)(void* self, int flags);
inline UiDeletingDtorFn UiDeletingDtorOf(void* obj) {
    return reinterpret_cast<UiDeletingDtorFn>((*reinterpret_cast<void***>(obj))[0]);
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
inline int UifNegCtrl() { return RtUiUifNegCtrl(); }

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
                UiDeletingDtorOf(dlg)(dlg, 1);
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
            UiDeletingDtorOf(dlg)(dlg, 1);
            *reinterpret_cast<void**>(node) = nullptr;
        }
    }
    if (UifNegCtrl() == 1) {
        // NEGATIVE CONTROL 1 — destroy the bit-5 dialogs phase 1 deliberately
        // skipped. The twin's ordered log must change.
        for (int i = 0; i < *reinterpret_cast<int32_t*>(p + 0x20); ++i) {
            uint8_t** rd = *reinterpret_cast<uint8_t***>(p + 0x18);
            if (rd && rd[i]) UiDeletingDtorOf(rd[i])(rd[i], 1);
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
    UiListClear(p);
}

} // namespace

extern "C" void RtUi_UserInterface_Dtor(UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::~UserInterface [liveness]");
    // `worked` is the only thing a LIVE call can report: there is no comparison
    // to make against a destructor that runs once, so `calls=` stays 0 here on
    // purpose and the CORRECTNESS evidence is the twin oracle below, which runs
    // from RtUiManagerRegister. reference_calls_zero_can_mean_always_took_the_
    // uncovered_branch.md: this is the "no comparison exists" cause of calls=0,
    // and it is named here rather than left for a reader to guess.
    const bool didWork = (*reinterpret_cast<int32_t*>(
                              reinterpret_cast<uint8_t*>(self) + 0x20) > 0) ||
                         (*reinterpret_cast<void**>(self) != nullptr);
    UiDtorBody(self);
    RT_DIFF_WORK(didWork);
}

// ============================================================================
// THE TWIN-SYNTHETIC ORACLE for ??1UserInterface
// ============================================================================
// reference_synthetic_subject_beats_a_fake.md. A destructor cannot be replayed,
// but it CAN be run twice on two subjects that are identical by construction —
// and unlike the ctor's twin oracle above, this one needs a whole object GRAPH,
// not just 0x128 poisoned bytes. So the harness builds one:
//
//   * 5 synthetic dialogs, 0x28 bytes each, out of the REAL allocator
//     (`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`), so the frees this body
//     performs are frees the live allocator accepts. Each carries a one-entry
//     fake vtable whose slot 0 is a RECORDER, not a destructor: it appends
//     `d<idx>(<edx>)` to the current side's log and returns. That is what makes
//     the dispatch sequence comparable at all — a real deleting destructor
//     would free the subject and leave nothing to compare.
//   * a 4-node `_dialogs` list over d0..d3, and a `_removedDialogs` array of
//     [d1, d0, d4, d3].
//
// The population is chosen to hit every branch in the body ONCE, which is the
// effectiveness control this instrument needs (reference_probe_needs_an_
// effectiveness_control.md — a green comparison over a population that never
// entered the interesting branch is not a verification):
//
//   d1  a MIDDLE node          -> both `prev` and `next` relink arms
//   d0  the HEAD node          -> the `node == head` arm
//   d4  NOT IN THE LIST        -> the search falls off the end, AND its flag
//                                 bit 5 is SET, so its destructor must NOT run
//   d3  the TAIL node          -> the `node == tail` arm
//   d2  left in the list       -> phase 2 runs, and `*node = 0` is compared
//
// so the expected log is exactly `d1(1) d0(1) d3(1) d2(1) ` and a port that
// destroyed d4, or that visited the list in the wrong order, or that took the
// head/tail arms wrongly, produces a different string. That expected string is
// asserted as well as compared, so "both sides did nothing" cannot read green.
//
// WHAT THE COMPARISON COVERS: all 0x128 bytes of the object (both sides start
// from the SAME 0x5A poison, so the ~200 bytes neither implementation touches
// are equal by construction rather than by luck —
// reference_oracle_must_not_compare_unwritten_bytes.md), plus the ordered
// destructor-dispatch log.
//
// WHAT IT DOES NOT COVER, stated rather than left implicit: the IDENTITY of the
// blocks handed to `MemFree`. Each side frees its OWN nodes and arrays, at
// different addresses, so the freed pointers are not comparable between sides.
// A wrong pointer passed to `MemFree` is caught by a DIFFERENT instrument —
// `BANISHED_REDZONE=watch` over a stress town — and docs/runtime_port/
// WAVE10_LANE_UIF.md records that run rather than letting this site imply it.
namespace {

struct UiTwinSide {
    UserInterface* ui;
    void*          dlg[5];
    char           log[160];
    int            logLen;
};

UiTwinSide* g_uiTwinCur = nullptr;

// The fake vtable's slot 0. Signature is retail's deleting-destructor ABI.
void UiTwinRecordDtor(void* self, int flags) {
    UiTwinSide* s = g_uiTwinCur;
    if (!s) return;
    int idx = -1;
    for (int i = 0; i < 5; ++i) if (s->dlg[i] == self) idx = i;
    if (s->logLen < static_cast<int>(sizeof(s->log)) - 16) {
        s->logLen += std::snprintf(s->log + s->logLen,
                                   sizeof(s->log) - s->logLen,
                                   "d%d(%d) ", idx, flags);
    }
}

void* g_uiTwinVtbl[1] = { reinterpret_cast<void*>(&UiTwinRecordDtor) };

inline void* UiMemAlloc(size_t n, int align) {
    using Fn = void* (*)(size_t, int);
    return RT_EXPORT_ONCE(Fn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(n, align);
}

bool UiTwinBuild(UiTwinSide& s) {
    s.logLen = 0;
    s.log[0] = '\0';
    s.ui = static_cast<UserInterface*>(UiMemAlloc(kUiSize, 16));
    if (!s.ui) return false;
    // The SAME poison on both sides: every byte this body never writes must
    // compare equal by construction.
    std::memset(s.ui, 0x5A, kUiSize);
    uint8_t* p = reinterpret_cast<uint8_t*>(s.ui);

    for (int i = 0; i < 5; ++i) {
        uint8_t* d = static_cast<uint8_t*>(UiMemAlloc(0x28, 8));
        if (!d) return false;
        std::memset(d, 0, 0x28);
        *reinterpret_cast<void**>(d) = g_uiTwinVtbl;
        // d4 is the one whose bit 5 is SET — "a nested ShowDialog UI owns it",
        // so its destructor must NOT run.
        *reinterpret_cast<uint32_t*>(d + 0x20) = (i == 4) ? (1u << 5) : 0u;
        s.dlg[i] = d;
    }

    uint8_t* n[4];
    for (int i = 0; i < 4; ++i) {
        n[i] = static_cast<uint8_t*>(UiMemAlloc(0x18, 8));
        if (!n[i]) return false;
        std::memset(n[i], 0, 0x18);
        *reinterpret_cast<void**>(n[i]) = s.dlg[i];
    }
    for (int i = 0; i < 4; ++i) {
        *reinterpret_cast<void**>(n[i] + 0x08) = (i > 0) ? n[i - 1] : nullptr;
        *reinterpret_cast<void**>(n[i] + 0x10) = (i < 3) ? n[i + 1] : nullptr;
    }
    *reinterpret_cast<void**>(p + 0x00)    = n[0];   // _dialogs._head
    *reinterpret_cast<void**>(p + 0x08)    = n[3];   // _dialogs._tail
    *reinterpret_cast<int32_t*>(p + 0x10)  = 4;      // _dialogs._count
    *reinterpret_cast<uint32_t*>(p + 0x14) = 0;      // the pad, identical both sides

    void** rem = static_cast<void**>(UiMemAlloc(4 * 8, 8));
    if (!rem) return false;
    rem[0] = s.dlg[1];  rem[1] = s.dlg[0];  rem[2] = s.dlg[4];  rem[3] = s.dlg[3];
    *reinterpret_cast<void**>(p + 0x18)   = rem;
    *reinterpret_cast<int32_t*>(p + 0x20) = 4;
    *reinterpret_cast<int32_t*>(p + 0x24) = 4;

    void** add = static_cast<void**>(UiMemAlloc(8, 8));
    if (!add) return false;
    add[0] = nullptr;
    *reinterpret_cast<void**>(p + 0x28)   = add;
    *reinterpret_cast<int32_t*>(p + 0x30) = 0;
    *reinterpret_cast<int32_t*>(p + 0x34) = 1;

    void* ev = UiMemAlloc(0x30, 16);
    if (!ev) return false;
    std::memset(ev, 0, 0x30);
    *reinterpret_cast<void**>(p + 0x48)   = ev;
    *reinterpret_cast<int32_t*>(p + 0x50) = 0;
    *reinterpret_cast<int32_t*>(p + 0x54) = 1;

    // Everything from +0x38 up (bar the +0x48 array just written) is left as
    // poison: this body never reads or writes it, and both sides hold the same
    // bytes, so it is part of the COMPARED extent rather than excluded from it.
    return true;
}

void VerifyUserInterfaceDtorTwin() {
    if (!RtDiffEnabled()) return;
    static RtDiffSite site = { "UI::UserInterface::~UserInterface [twin synthetic]" };
    RtDiffRegister(&site);

    UiTwinSide a{}, b{};
    if (!UiTwinBuild(a) || !UiTwinBuild(b)) {
        RtDiffReport(&site, false, "twin subjects built",
                     "allocator refused a synthetic block");
        return;
    }

    // RT_RAW_OK: the ORACLE half must reach the SHIPPED body, never our own
    // registration — WAVE9_LANE_FNHOOK.md's thirteen self-comparing detours.
    using Fn = void (*)(UserInterface*);
    Fn theirs = reinterpret_cast<Fn>(ResolveExportRaw("??1UserInterface@UI@@QEAA@XZ"));
    if (!theirs) {
        RtDiffReport(&site, false, "twin subjects built",
                     "shipped ??1UserInterface not resolvable");
        return;
    }

    g_uiTwinCur = &a;  UiDtorBody(a.ui);
    g_uiTwinCur = &b;  theirs(b.ui);
    g_uiTwinCur = nullptr;

    const bool logEq      = (std::strcmp(a.log, b.log) == 0);
    const bool ranBranches = (std::strcmp(a.log, "d1(1) d0(1) d3(1) d2(1) ") == 0);

    char ours[256], them[256];
    std::snprintf(ours, sizeof(ours),
                  "log=[%s] expected=[d1(1) d0(1) d3(1) d2(1) ]", a.log);
    std::snprintf(them, sizeof(them), "log=[%s]", b.log);
    RtDiffReport(&site, logEq && ranBranches, ours, them);

    RtDiffReportBytes(&site, a.ui, b.ui, kUiSize);

    // The subjects' own storage: the nodes and the three arrays were freed by
    // the bodies under test; the dialogs were not (the recorder is not a
    // destructor) and neither were the two UserInterface objects.
    for (int i = 0; i < 5; ++i) { UiMemFree(a.dlg[i]); UiMemFree(b.dlg[i]); }
    UiMemFree(a.ui);
    UiMemFree(b.ui);
}

} // namespace

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

// The two EXPORTED IO vtables, through the never-caches-a-null data door.
inline const void* VtblIoStream() {
    return RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");          // 0x1801549a0
}
inline const void* VtblIoMemoryStream() {
    return RT_DATA_SLOT(const void*, "??_7MemoryStream@IO@@6B@");    // 0x1801555b0
}
// The shared IO::Encoding vtable. UNEXPORTED — the address door is the only
// one, and it is PORT-AWARE, so lane C2's mint satisfies it at cutover.
inline void* VtblIoEncoding() {
    static void* v = nullptr;
    if (!v) v = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x1801554a8ull));
    return v;
}

inline void* UiMemAllocA(size_t n, int align) {
    using Fn = void* (*)(size_t, int);
    return RT_EXPORT_ONCE(Fn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(n, align);
}

// `IO::Serializer` — {Stream*, ReaderWriter*}, 0x10 bytes
// (runtime/types/io/Serializer.h). Built on the stack twice per controller.
struct UiSerializer { void* stream; void* rw; };

using UiCreateRwFn   = void* (*)(void* enc, UiSerializer* out, const char* name);
using UiScalarFn     = void  (*)(void* rw, void* value, const char* name);
using UiStreamVFn    = void  (*)(void* self, void* stream);
using UiGetIdFn      = int32_t (*)(void* self);
using UiDelDtorFn    = void  (*)(void* self, int flags);

void UiSaveAllSettingsBody(UserInterface* self) {
    uint8_t* node = *reinterpret_cast<uint8_t**>(self);   // _dialogs._head
    if (!node) return;

    const void* vtStream = VtblIoStream();
    const void* vtMem    = VtblIoMemoryStream();
    void*       vtEnc    = VtblIoEncoding();

    do {
        uint8_t* dlg = *reinterpret_cast<uint8_t**>(node);
        if (*reinterpret_cast<void**>(dlg + 0xa8) != nullptr) {
            // ---- 1. the inlined IO::MemoryStream ---------------------------
            RtIo::MemoryStream st;
            st.base.vptr     = vtStream;
            st.base.direction = 0;
            st.base.encoding = nullptr;
            st.base.vptr     = vtMem;
            st.data          = nullptr;
            st.ownsData      = 0;
            st.cursor        = 0;      // the one qword at +0x24 covers both
            st.size          = 0;

            // ---- 2. the 8-byte IO::Encoding -------------------------------
            void* enc = UiMemAllocA(8, 4);
            if (enc) *reinterpret_cast<void**>(enc) = vtEnc;
            // Retail frees the PREVIOUS encoding before installing this one.
            // It is always null at this point; the store order is retail's.
            if (void* oldEnc = st.base.encoding) UiMemFree(oldEnc);
            st.base.encoding = enc;

            // ---- 3. point the stream at the dialog's settings buffer ------
            const int32_t size = *reinterpret_cast<int32_t*>(dlg + 0xb0);
            void*         buf  = *reinterpret_cast<void**>(dlg + 0xa8);
            st.data      = buf;
            st.size      = static_cast<uint32_t>(size);
            st.cursor    = 0;
            st.base.direction = 1;
            if (size >= 4) {
                *reinterpret_cast<uint32_t*>(buf) = 0xdeadbeefu;
                // NEGATIVE CONTROL 3 — skip the cursor advance, so everything
                // the writer emits lands 4 bytes early, over the magic.
                if (UifNegCtrl() != 3) st.cursor += 4;
            }

            // ---- 4. the top-level ReaderWriter ----------------------------
            UiSerializer ser{ &st, nullptr };
            void* rw = nullptr;
            if (enc) {
                void** encVt = *reinterpret_cast<void***>(enc);
                UiCreateRwFn make = reinterpret_cast<UiCreateRwFn>(
                    st.base.direction != 0 ? encVt[4]    // +0x20 CreateWriter
                                           : encVt[3]);  // +0x18 CreateReader
                rw = make(enc, &ser, nullptr);
            }
            ser.rw = rw;

            // ---- 5. the dialog's own OnSerializeSettings ------------------
            reinterpret_cast<UiStreamVFn>(
                (*reinterpret_cast<void***>(dlg))[9])(dlg, &st);   // +0x48

            // ---- 6. the controller loop -----------------------------------
            int32_t count = *reinterpret_cast<int32_t*>(dlg + 0x80);
            if (rw) {
                reinterpret_cast<UiScalarFn>(
                    (*reinterpret_cast<void***>(rw))[20])(rw, &count, nullptr); // +0xa0
            }
            for (int32_t i = 0; i < count; ++i) {
                uint8_t* arr = *reinterpret_cast<uint8_t**>(dlg + 0x78);
                void*    ctl = *reinterpret_cast<void**>(arr + i * 8);
                int32_t  id  = reinterpret_cast<UiGetIdFn>(
                                   (*reinterpret_cast<void***>(ctl))[2])(ctl);  // +0x10
                if (rw) {
                    reinterpret_cast<UiScalarFn>(
                        (*reinterpret_cast<void***>(rw))[19])(rw, &id, nullptr); // +0x98
                }

                UiSerializer ser2{ &st, nullptr };
                void* rw2 = nullptr;
                if (enc) {
                    void** encVt = *reinterpret_cast<void***>(enc);
                    UiCreateRwFn make2 = reinterpret_cast<UiCreateRwFn>(
                        st.base.direction != 0 ? encVt[4] : encVt[3]);
                    rw2 = make2(enc, &ser2, nullptr);
                }
                ser2.rw = rw2;

                // RE-READ at 0x1801206a1, not cached across the CreateWriter.
                arr = *reinterpret_cast<uint8_t**>(dlg + 0x78);
                ctl = *reinterpret_cast<void**>(arr + i * 8);
                reinterpret_cast<UiStreamVFn>(
                    (*reinterpret_cast<void***>(ctl))[10])(ctl, &st);   // +0x50

                if (rw2) {
                    reinterpret_cast<UiDelDtorFn>(
                        (*reinterpret_cast<void***>(rw2))[0])(rw2, 1);
                }
            }

            // ---- 7. teardown ----------------------------------------------
            if (rw) {
                reinterpret_cast<UiDelDtorFn>(
                    (*reinterpret_cast<void***>(rw))[0])(rw, 1);
            }
            st.base.vptr = vtMem;
            if (st.ownsData != 0 && st.data) UiMemFree(st.data);   // dead: never owns
            st.base.vptr = vtStream;
            if (st.base.encoding) UiMemFree(st.base.encoding);
        }
        node = *reinterpret_cast<uint8_t**>(node + 0x10);
    } while (node);
}

// ---- the buffer-replay oracle's storage ------------------------------------
// Three parallel copies of every dialog settings buffer this call touches:
// the ORIGINAL (to restore), OURS (the result of our body) and, implicitly,
// the live buffers after the DLL's body runs. 64 KB is generous — a dialog's
// settings blob is tens of bytes — and a call whose total exceeds it is NOT
// DIFFED rather than reported as a divergence, because "the oracle's buffer was
// too small" is not a fact about the port (WAVE7_LANE_B.md §4 says so, so that
// a low `calls=` is not read as a stall).
constexpr size_t kSasBufBytes = 64 * 1024;
constexpr int    kSasMaxBufs  = 64;
uint8_t  g_sasOrig[kSasBufBytes];
uint8_t  g_sasOurs[kSasBufBytes];
uint8_t* g_sasPtr[kSasMaxBufs];
uint32_t g_sasLen[kSasMaxBufs];
uint32_t g_sasOff[kSasMaxBufs];

} // namespace

// ORACLE: REPLAY ON THE SIDE EFFECT, which for this body is the only thing
// there is to compare — it returns void and writes nothing to `this`.
//
//   snapshot every dialog's settings buffer -> run OURS -> copy the result out
//   -> RESTORE the snapshot -> run the SHIPPED body -> compare our copy against
//   what the shipped body left.
//
// This is sound for the reason reference_write_path_serialize_is_replayable.md
// records: on the WRITE path a Serialize consumes state and produces bytes, so
// running it twice from the SAME starting bytes must produce the SAME bytes.
// Both runs also allocate and free their own Encoding and ReaderWriters, which
// balances.
//
// THE EFFECTIVENESS CONTROL IS REPORTED, NOT ASSUMED. The diff message carries
// `dialogs=N bytes=B`, so a green result over a UI where every dialog had a
// null +0xa8 — which is the common case at the main menu — reads as
// `dialogs=0 bytes=0` and is visibly NOT a verification
// (reference_probe_needs_an_effectiveness_control.md, and
// reference_uncalled_port_is_not_verified.md one level in: `calls=` counts this
// function, `bytes=` counts whether it had anything to say).
extern "C" void RtUi_UserInterface_SaveAllSettings(UserInterface* self) {
    RT_DIFF_BEGIN("UI::UserInterface::SaveAllSettings");

    int      nBufs = 0;
    uint32_t total = 0;
    bool     fits  = true;

    if (rt_diff_on__) {
        for (uint8_t* node = *reinterpret_cast<uint8_t**>(self); node;
             node = *reinterpret_cast<uint8_t**>(node + 0x10)) {
            uint8_t* dlg = *reinterpret_cast<uint8_t**>(node);
            uint8_t* buf = *reinterpret_cast<uint8_t**>(dlg + 0xa8);
            if (!buf) continue;
            const int32_t len = *reinterpret_cast<int32_t*>(dlg + 0xb0);
            if (len <= 0) continue;
            if (nBufs >= kSasMaxBufs ||
                total + static_cast<uint32_t>(len) > kSasBufBytes) { fits = false; break; }
            g_sasPtr[nBufs] = buf;
            g_sasLen[nBufs] = static_cast<uint32_t>(len);
            g_sasOff[nBufs] = total;
            std::memcpy(g_sasOrig + total, buf, static_cast<size_t>(len));
            total += static_cast<uint32_t>(len);
            ++nBufs;
        }
    }

    const bool replay = rt_diff_on__ && fits && nBufs > 0;

    UiSaveAllSettingsBody(self);

    if (replay) {
        for (int i = 0; i < nBufs; ++i)
            std::memcpy(g_sasOurs + g_sasOff[i], g_sasPtr[i], g_sasLen[i]);
        for (int i = 0; i < nBufs; ++i)
            std::memcpy(g_sasPtr[i], g_sasOrig + g_sasOff[i], g_sasLen[i]);

        // RT_RAW_OK: the ORACLE half must reach the SHIPPED body and never our
        // own registration.
        using Fn = void (*)(UserInterface*);
        Fn theirs = RT_ORIGINAL_ONCE(Fn, kSaveAllSettings);
        if (theirs) {
            theirs(self);
            char ours[128], them[128];
            std::snprintf(ours, sizeof(ours), "dialogs=%d bytes=%u", nBufs, total);
            std::snprintf(them, sizeof(them), "dialogs=%d bytes=%u", nBufs, total);
            RtDiffReport(&rt_diff_site__, true, ours, them);
            for (int i = 0; i < nBufs; ++i) {
                RtDiffReportBytes(&rt_diff_site__, g_sasOurs + g_sasOff[i],
                                  g_sasPtr[i], g_sasLen[i]);
            }
        }
    } else if (rt_diff_on__) {
        char ours[128], them[128];
        std::snprintf(ours, sizeof(ours), "dialogs=%d bytes=%u", nBufs, total);
        std::snprintf(them, sizeof(them), "%s", fits ? "NOT DIFFED (no settings buffer)"
                                                     : "NOT DIFFED (oracle buffer too small)");
        RtDiffNoteWork(&rt_diff_site__, nBufs > 0);
        (void)ours; (void)them;
    }
    RT_DIFF_WORK(nBufs > 0);
}

// ============================================================================
// THE TWIN-SYNTHETIC ORACLE for ?SaveAllSettings
// ============================================================================
// THE REPLAY ORACLE ABOVE IS LIVE BUT HAS NO POPULATION, and this is a measured
// fact, not a guess. A `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1
// BANISHED_SAVE_TEST=900` run that really saved (`[SaveTest] frame 900:
// invoking retail autosave handler` … `frame 961: operation 4 -> 0`) reports
//
//     UI::UserInterface::SaveAllSettings   calls=0   worked=0   divergences=0
//
// The site IS registered, so the body ran — `worked=0` says every call found
// `dialogs=0`: not one dialog in the live list had a non-null settings buffer
// at +0xa8, so the whole loop was skipped. That is precisely the state
// reference_calls_zero_can_mean_always_took_the_uncovered_branch.md describes,
// and it is why `worked=` is reported here at all.
//
// So the interesting half is built instead. Two identical synthetic
// `UserInterface`s, each with two synthetic dialogs that DO have settings
// buffers and two controllers apiece; ours runs on one, the SHIPPED body on the
// other, and the comparison is the SETTINGS BUFFERS plus an ordered call log.
//
// WHAT THAT ACTUALLY EXERCISES — all of it, and none of it is stubbed out:
//   * the inlined `IO::MemoryStream` construction and both vtable stores
//   * the 8-byte `IO::Encoding` allocation and its unexported vtable
//   * the `0xdeadbeef` magic and the cursor advance to 4
//   * `CreateWriter` through encoding slot +0x20, once at the top and once per
//     controller, and the throw-away ReaderWriter deletes
//   * `ScalarInt` (+0xa0) for the controller count and `ScalarUInt` (+0x98)
//     for each controller id — REAL `IO::BinaryWriter` calls writing REAL bytes
//     into the buffer, which is what the compare then reads
//   * the inlined `~MemoryStream` and the encoding free
// Only the DIALOG's own `+0x48` and the CONTROLLERS' `+0x50` are synthetic, and
// they are recorders rather than stubs: they append to the log, so their ORDER
// and their ARGUMENTS are compared too.
namespace {

struct SasTwinSide {
    uint8_t* ui;
    uint8_t* dlg[2];
    uint8_t* buf[2];
    uint8_t* ctl[4];
    char     log[192];
    int      logLen;
};

SasTwinSide* g_sasTwinCur = nullptr;

void SasTwinNote(const char* what, int idx) {
    SasTwinSide* s = g_sasTwinCur;
    if (!s || s->logLen >= static_cast<int>(sizeof(s->log)) - 12) return;
    s->logLen += std::snprintf(s->log + s->logLen, sizeof(s->log) - s->logLen,
                               "%s%d ", what, idx);
}

// Dialog vtable slot 9 (+0x48) — OnSerializeSettings(Stream&).
void SasTwinDialogSerialize(void* self, void* /*stream*/) {
    SasTwinSide* s = g_sasTwinCur;
    int idx = -1;
    if (s) for (int i = 0; i < 2; ++i) if (s->dlg[i] == self) idx = i;
    SasTwinNote("D", idx);
}
// Controller vtable slot 2 (+0x10) — the id getter.
int32_t SasTwinCtlGetId(void* self) {
    SasTwinSide* s = g_sasTwinCur;
    int idx = -1;
    if (s) for (int i = 0; i < 4; ++i) if (s->ctl[i] == self) idx = i;
    return 0x1000 + idx;
}
// Controller vtable slot 10 (+0x50) — OnSerializeSettings(Stream&).
void SasTwinCtlSerialize(void* self, void* /*stream*/) {
    SasTwinSide* s = g_sasTwinCur;
    int idx = -1;
    if (s) for (int i = 0; i < 4; ++i) if (s->ctl[i] == self) idx = i;
    SasTwinNote("C", idx);
}

void* g_sasTwinVtbl[11] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&SasTwinCtlGetId),        // [2]  +0x10
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(&SasTwinDialogSerialize), // [9]  +0x48
    reinterpret_cast<void*>(&SasTwinCtlSerialize),    // [10] +0x50
};

constexpr uint32_t kSasTwinBufBytes = 256;

bool SasTwinBuild(SasTwinSide& s) {
    s.logLen = 0; s.log[0] = '\0';
    s.ui = static_cast<uint8_t*>(UiMemAlloc(kUiSize, 16));
    if (!s.ui) return false;
    std::memset(s.ui, 0, kUiSize);

    uint8_t* prev = nullptr;
    for (int i = 0; i < 2; ++i) {
        uint8_t* d = static_cast<uint8_t*>(UiMemAlloc(0xb8, 8));
        uint8_t* b = static_cast<uint8_t*>(UiMemAlloc(kSasTwinBufBytes, 8));
        void**   arr = static_cast<void**>(UiMemAlloc(2 * 8, 8));
        if (!d || !b || !arr) return false;
        std::memset(d, 0, 0xb8);
        // The buffer starts from a FIXED pattern on both sides, so any byte the
        // serializer does not overwrite compares equal by construction
        // (reference_oracle_must_not_compare_unwritten_bytes.md).
        std::memset(b, 0xC3, kSasTwinBufBytes);
        *reinterpret_cast<void**>(d) = g_sasTwinVtbl;
        for (int k = 0; k < 2; ++k) {
            uint8_t* c = static_cast<uint8_t*>(UiMemAlloc(0x20, 8));
            if (!c) return false;
            std::memset(c, 0, 0x20);
            *reinterpret_cast<void**>(c) = g_sasTwinVtbl;
            s.ctl[i * 2 + k] = c;
            arr[k] = c;
        }
        *reinterpret_cast<void**>(d + 0x78)   = arr;     // _controllers._data
        *reinterpret_cast<int32_t*>(d + 0x80) = 2;       // _controllers._size
        *reinterpret_cast<void**>(d + 0xa8)   = b;       // the settings buffer
        *reinterpret_cast<int32_t*>(d + 0xb0) = static_cast<int32_t>(kSasTwinBufBytes);
        s.dlg[i] = d;
        s.buf[i] = b;

        uint8_t* node = static_cast<uint8_t*>(UiMemAlloc(0x18, 8));
        if (!node) return false;
        std::memset(node, 0, 0x18);
        *reinterpret_cast<void**>(node) = d;
        if (prev) *reinterpret_cast<void**>(prev + 0x10) = node;
        else      *reinterpret_cast<void**>(s.ui)        = node;
        *reinterpret_cast<void**>(node + 0x08) = prev;
        prev = node;
    }
    *reinterpret_cast<void**>(s.ui + 0x08)   = prev;
    *reinterpret_cast<int32_t*>(s.ui + 0x10) = 2;
    return true;
}

void SasTwinFree(SasTwinSide& s) {
    for (int i = 0; i < 2; ++i) {
        if (!s.dlg[i]) continue;
        UiMemFree(*reinterpret_cast<void**>(s.dlg[i] + 0x78));
        UiMemFree(s.buf[i]);
        UiMemFree(s.dlg[i]);
    }
    for (int i = 0; i < 4; ++i) if (s.ctl[i]) UiMemFree(s.ctl[i]);
    if (s.ui) {
        for (uint8_t* n = *reinterpret_cast<uint8_t**>(s.ui); n; ) {
            uint8_t* next = *reinterpret_cast<uint8_t**>(n + 0x10);
            UiMemFree(n);
            n = next;
        }
        UiMemFree(s.ui);
    }
}

void VerifySaveAllSettingsTwin() {
    if (!RtDiffEnabled()) return;
    static RtDiffSite site = { "UI::UserInterface::SaveAllSettings [twin synthetic]" };
    RtDiffRegister(&site);

    SasTwinSide a{}, b{};
    if (!SasTwinBuild(a) || !SasTwinBuild(b)) {
        RtDiffReport(&site, false, "twin subjects built",
                     "allocator refused a synthetic block");
        SasTwinFree(a); SasTwinFree(b);
        return;
    }

    // RT_RAW_OK: the ORACLE half must reach the SHIPPED body.
    using Fn = void (*)(UserInterface*);
    Fn theirs = reinterpret_cast<Fn>(
        ResolveExportRaw("?SaveAllSettings@UserInterface@UI@@QEAAXXZ"));
    if (!theirs) {
        RtDiffReport(&site, false, "twin subjects built",
                     "shipped ?SaveAllSettings not resolvable");
        SasTwinFree(a); SasTwinFree(b);
        return;
    }

    g_sasTwinCur = &a;  UiSaveAllSettingsBody(reinterpret_cast<UserInterface*>(a.ui));
    g_sasTwinCur = &b;  theirs(reinterpret_cast<UserInterface*>(b.ui));
    g_sasTwinCur = nullptr;

    // (1) the ordered call log, with the EXPECTED string asserted so that "both
    // sides did nothing" cannot read green.
    const bool logEq  = (std::strcmp(a.log, b.log) == 0);
    const bool ranAll = (std::strcmp(a.log, "D0 C0 C1 D1 C2 C3 ") == 0);
    char ours[256], them[256];
    std::snprintf(ours, sizeof(ours), "log=[%s] expected=[D0 C0 C1 D1 C2 C3 ]", a.log);
    std::snprintf(them, sizeof(them), "log=[%s]", b.log);
    RtDiffReport(&site, logEq && ranAll, ours, them);

    // (2) the SIDE EFFECT — the settings buffers, which is the whole point of
    // this function (reference_port_must_reproduce_side_effects.md).
    for (int i = 0; i < 2; ++i)
        RtDiffReportBytes(&site, a.buf[i], b.buf[i], kSasTwinBufBytes);

    // (3) the EFFECTIVENESS CONTROL: the magic proves the writer actually wrote
    // into the buffer rather than both sides leaving the 0xC3 fill alone.
    const uint32_t magicA = *reinterpret_cast<uint32_t*>(a.buf[0]);
    char cOurs[96], cThem[96];
    std::snprintf(cOurs, sizeof(cOurs), "buffer[0] magic=0xdeadbeef");
    std::snprintf(cThem, sizeof(cThem), "buffer[0] magic=0x%08x", magicA);
    RtDiffReport(&site, magicA == 0xdeadbeefu, cOurs, cThem);

    SasTwinFree(a);
    SasTwinFree(b);
}

} // namespace

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

    // The twin-synthetic oracle is NOT called here — it needs the allocator,
    // which does not exist yet at registration time. It is triggered from the
    // first `RtUi_UserInterface_Ctor`; see the note at its forward declaration.

    return count;
}
