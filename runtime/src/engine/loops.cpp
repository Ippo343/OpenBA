// ============================================================================
// runtime/src/engine/loops.cpp — Wave 8 lane A: the Engine loop family
// ============================================================================
// Original source file(s): code\Runtime\Engine\{DialogLoop,DialogLoaderLoop}.cpp
// plus the UpdateLoop / LoaderLoop / Loader bodies that live alongside them
// (docs/runtime_port/original_source_tree.txt).
//
// Ported here (12 of the lane's 14 names; retail addresses are
// Runtime-x64.dll, image base 0x180000000):
//
//   ??0UpdateLoop@Engine@@QEAA@XZ                          0x1800ddd30  ord 468
//   ??1UpdateLoop@Engine@@UEAA@XZ                          0x1800dddb0  ord 704
//   ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ                 0x1800dd7b0  ord 3160
//   ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime…   0x1800dc1e0  ord 261
//   ?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z          0x1800dc050  ord 3204
//   ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z      0x1800db3f0  ord 153
//   ?Load@DialogLoopT@Engine@@UEAAXXZ                      0x1800db140  ord 3201
//   ?Startup@DialogLoopT@Engine@@UEAAXXZ                   0x1800db2f0  ord 4093
//   ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z  0x1800db270  ord 4485
//   ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System…   0x1800db0d0  ord 151
//   ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ                0x1800dad50  ord 3200
//   ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ             0x1800db000  ord 4092
//
// CORRECTION 2026-09-11 (WAVE 25 lane VTIO): the count above is Wave 8's and is
// left as written.  SIX more names are ported at the bottom of this file — the
// shared UpdateLoop base slots 0x180031740..0x180031790 — bringing the file to
// 18.  See "WAVE 25 lane VTIO" below for what that does and does not buy.
//
// NOT ported, both deliberately — the reasons are in docs/runtime_port/
// WAVE8_LANE_A.md §3 and are summarised at the bottom of this file:
//
//   ?LoadResources@UpdateLoop@Engine@@QEAAXXZ              0x1800dd7d0  ord 3210
//   ?_loader@LoaderLoop@Engine@@1VLoader@2@A               0x1801fe7b8  ord 4794
//
// ---------------------------------------------------------------------------
// THE VTABLE QUESTION (WAVE8.md §1), answered per class from the DISASSEMBLY.
//
// All five of this lane's classes export their vtable, and — the part that
// actually matters — each class's constructor stamps EXACTLY the address the
// export directory gives for its `??_7` symbol:
//
//   UpdateLoop        LEA RAX,[0x180155ba0] @0x1800ddd44   == ??_7UpdateLoop@Engine@@6B@
//   LoaderLoop        LEA RAX,[0x180157940] @0x1800dc20c   == ??_7LoaderLoop@Engine@@6B@
//   DialogLoopT       LEA RAX,[0x1801579a0] @0x1800db414   == ??_7DialogLoopT@Engine@@6B@
//   DialogLoaderLoopT LEA RAX,[0x1801578d0] @0x1800db0fd   == ??_7DialogLoaderLoopT@Engine@@6B@
//   Loader            LEA RAX,[0x180157a08] @0x1800dc111   == ??_7Loader@Engine@@6B@
//
// So this lane needs NO detour, for Wave 6's reason rather than Wave 7's luck:
// every ported ctor RESOLVES THE EXPORTED SYMBOL BY NAME and stamps that, so an
// object our ctor built and one the DLL's ctor built carry the identical table
// and a virtual call from either side lands on the same body. Nothing here
// mints a table. The detour count for the whole port therefore stays at 0
// after this lane, and WAVE8.md §1's "state which reason applies" is answered:
// the vtables are exported and are stamped by name.
//
// ---------------------------------------------------------------------------
// IS THE PORT ACTUALLY REACHED? (WAVE8.md, and
// reference_uncalled_port_is_not_verified.md — a port with zero callers reports
// `calls=0 divergences=0` forever, which reads as a pass.)
//
// Three of this lane's functions are `UEAA` — virtuals. Registering an export
// flips OUR call sites; it does NOT rewrite the DLL's own vtables, so a call
// the DLL makes through ??_7DialogLoopT@Engine@@6B@ still reaches the DLL. That
// would normally leave a ported virtual dead. It does not here, and the reason
// is specific and checked:
//
//   * Every DialogLoopT / DialogLoaderLoopT object in our process is built by
//     our tree (fn_DialogLoopT_Ctor …) and then has its vtable OVERWRITTEN with
//     an exe-subclass table we own: g_menuDialogLoopVtable
//     (standalone/menudialogloopt_stubs.cpp, 13 slots), g_splashDialogLoopVtable
//     (standalone/splash_loop_stubs.cpp), g_outerLoaderLoopVtable
//     (standalone/loader_loop_stubs.cpp).
//   * Those stubs call the base implementation through fn_DialogLoopT_Startup /
//     _Load / _Update, which engine_funcs.cpp resolves with ResolveExport.
//   * EngineFuncsInit runs AFTER RuntimePortInit (standalone/main.cpp:1614 then
//     1631), so those pointers pick up this file's registrations.
//
// `?IsRunning@` reaches us the same way, through a dispatch slot:
// standalone/reg_unreg_lane3.cpp registers exe slot 0x140152850 with
// ResolveExport("?IsRunning@UpdateLoop@Engine@@QEBA_NXZ"), and
// AddressRegistryInit also runs after RuntimePortInit.
//
// The four ctors and Loader::Load are plain `QEAA` and are reached directly.
//
// ---------------------------------------------------------------------------
// PORTABILITY: no <windows.h> in this file (PLAN.md §4.1 decision 1).
// Loader::Load ends in an inlined System::Thread::Start — two Win32 event
// calls — and reaches them through the already-ported
// RtSystem_Thread_Start (runtime/src/system/thread_win.cpp), which is the
// platform seam for exactly this.
// ============================================================================

#include <atomic>       // ENDGAME B.3.7: RULE B6-5 namespace-scope door caches
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>          // ENDGAME B.3.1 step 2: placement new into a POD member

#include "../../funcs/rt_port.h"
#include "mainloop_internal.h"             // ENDGAME B.3.7: rtengine::AtomicExport
#include "../system/pod_bridge.h"          // the one transitional POD<->class cast
#include "../../types/engine/UpdateLoop.h"
#include "../../types/engine/MainLoop.h"   // lane B; for MainLoop::_currentLoop
#include "../../types/engine/Engine.h"     // ENDGAME B.3.7: Engine::Engine (Loader::Run)
#include "../../types/application/Application.h"  // ENDGAME B.3.7: SleepThread
#include "../../types/ui/DialogInstance.h" // ENDGAME B.3.7: slot 4 OnOpened
#include "../../types/object/External.h"
#include "../../types/system/Value.h"
// WAVE 27 lane LOOPS: the three Draw slots and MainLoop::Draw reach into the
// render context (Clear, the shader-time constants, the viewport, _frameIndex),
// so the layout has to be the asserted one rather than raw byte offsets.
#include "../../types/video/Context.h"
#include "../../types/video/CompanionData.h"   // ENDGAME B.3.5: Video::ContextData
#include "../collection/array_internal.h"  // ENDGAME B.3.2 step 2: kSpec_Fill8Z bridge

// ----------------------------------------------------------------------------
// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally: resolve.h drags in <windows.h>, and PLAN §4.1
// requires runtime/src/ to stay platform-neutral. These declarations must stay
// byte-identical to resolve.h's; the linker checks them against it.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

// ENDGAME B.3.7: the `RtSystem_Thread_Start` forward declaration that stood
// here is gone. `Engine::Loader` derives from the real `System::Thread`
// (runtime/types/system/Thread.h), so Loader::Load calls the base's `Start`
// member and the three waits call `WaitForFinish` directly.

using RtEngine::UpdateLoop;
using RtEngine::LoaderLoop;
using RtEngine::DialogLoopT;
using RtEngine::DialogLoaderLoopT;
using RtEngine::Loader;

namespace {

// The five mangled-name constants that stood here (kUpdateLoopCtor, kIsRunning,
// kLoaderLoopCtor, kDialogLoopTCtor, kDialogLoaderCtor) lost their last reader
// with the diff guards; B.3.7 close removed them. Every RegisterRuntimeExport
// call at the bottom of this file spells its literal in full (tools/
// runtime_census.py greps the literal at the registration site).

// ============================================================================
// Dependencies — each resolved ONCE and cached.
// ============================================================================
// Which door each one uses is a decision, not a habit:
//
//  * ResolveExport(name) — "whatever is registered". Used for everything a
//    ported body calls, so one live object is never split between our port and
//    the DLL's body for the same class (the Wave 5 lane D failure mode).
//  * ResolveAddr(Module::Runtime, addr) — "the DLL", for the one helper this
//    lane needs that the DLL does not export. Safe only because this lane never
//    calls RegisterRuntimeAddr on that address; ResolveAddr consults the port
//    registry FIRST (standalone/resolve_standalone.cpp), so reading bytes or
//    taking a pointer at an address you also register hands back your own
//    prologue (reference_resolveaddr_consults_port_registry_first.md).

// ENDGAME B.3.1 step 2: `MemFreeFn` and `AdjustDelimFn` are gone with the two
// String idioms they served — the allocator call lived inside the release
// sequence (now ~String, 0x180006c80) and the delimiter pass lived inside the
// Adopt-then-normalise pair (now the Filename ctor 0x18000c780).
using FindOrLoadFn     = void* (*)(void*, const void*);         // (factory, const Filename&)
using IsDerivedFromFn  = bool (*)(const void*, uint32_t);
using ExtFactoryCleanFn = void (*)(void*);
using ListCleanupFn    = void (*)(void*);
using RefreshListFn    = void (*)(void*);
using SetupFileSystemFn = void (*)(void*);
using ModArrayCopyFn   = void (*)(void*, const void*);
using UiCtorFn         = void* (*)(void*);
using ArraySetSizeFillFn = void (*)(void*, int, const void*);
using SecondsToTicksFn = int64_t (*)(float);
using TicksToSecondsFn = float (*)(int64_t);

// ENDGAME B.3.7 (RULE B7-7.1 / B6-5). These used to be function-local
// `static Fn fn = ResolveExport(...)` caches -- a guarded lazy initialiser that
// the loader thread reaches first (`FindOrLoad`/`IsDerivedFrom` from the two
// slot-6 `Load` bodies Loader::Run dispatches; the UI ctor/dtor from the loop
// ctor/dtor doors GameUpdateLoop::Load runs there). Each is now a
// NAMESPACE-SCOPE atomic slot (`rtengine::AtomicExport`, mainloop_internal.h).
std::atomic<void*> g_findOrLoad{nullptr};
std::atomic<void*> g_isDerivedFrom{nullptr};
std::atomic<void*> g_externalFactoryClean{nullptr};
std::atomic<void*> g_modificationRefreshList{nullptr};
std::atomic<void*> g_modificationSetupFileSystem{nullptr};
std::atomic<void*> g_userInterfaceCtor{nullptr};
std::atomic<void*> g_userInterfaceDtor{nullptr};
std::atomic<void*> g_listCleanup{nullptr};
std::atomic<void*> g_modArrayCopy{nullptr};
std::atomic<void*> g_vtUpdateLoop{nullptr};
std::atomic<void*> g_vtLoaderLoop{nullptr};
std::atomic<void*> g_vtDialogLoopT{nullptr};
std::atomic<void*> g_vtDialogLoaderLoopT{nullptr};
std::atomic<void*> g_loaderLoopLoader{nullptr};
std::atomic<void*> g_applicationSingleton{nullptr};

FindOrLoadFn FindOrLoad() {
    return reinterpret_cast<FindOrLoadFn>(rtengine::AtomicExport(g_findOrLoad,
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z"));
}
IsDerivedFromFn IsDerivedFrom() {
    return reinterpret_cast<IsDerivedFromFn>(rtengine::AtomicExport(g_isDerivedFrom,
        "?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
}
ExtFactoryCleanFn ExternalFactoryClean() {
    return reinterpret_cast<ExtFactoryCleanFn>(rtengine::AtomicExport(g_externalFactoryClean,
        "?Clean@ExternalFactory@Object@@QEAAXXZ"));
}
RefreshListFn ModificationRefreshList() {
    return reinterpret_cast<RefreshListFn>(rtengine::AtomicExport(g_modificationRefreshList,
        "?RefreshList@Modification@IO@@QEAAXXZ"));
}
SetupFileSystemFn ModificationSetupFileSystem() {
    return reinterpret_cast<SetupFileSystemFn>(rtengine::AtomicExport(g_modificationSetupFileSystem,
        "?SetupFileSystem@Modification@IO@@QEAAXXZ"));
}
UiCtorFn UserInterfaceCtor() {
    return reinterpret_cast<UiCtorFn>(rtengine::AtomicExport(g_userInterfaceCtor,
        "??0UserInterface@UI@@QEAA@XZ"));
}
// BATCH 8: the three loop dtors below each embed a UI::UserInterface and
// destroy it by calling this same exported dtor (0x180120b90, already ported
// and registered — runtime/src/ui/userinterface.cpp's RtUi_UserInterface_Dtor
// — so this reaches OUR body, not the DLL's).
using UiDtorFn = void (*)(void*);
UiDtorFn UserInterfaceDtor() {
    return reinterpret_cast<UiDtorFn>(rtengine::AtomicExport(g_userInterfaceDtor,
        "??1UserInterface@UI@@QEAA@XZ"));
}

// ---- the two the DLL does not export ---------------------------------------
// 0x180010ab0 — the tail call of ~UpdateLoop, on (this + 0x10). Already known
// to our tree as fn_ListCleanup (engine/funcs/engine_funcs.h:277), resolved
// there from this same address.
// NOT CONVERTED (ENDGAME B.3.2 step 2): 0x180010ab0 is a J2 entry
// (array_destroy.cpp), `ReleaseStringsCounted(..., kD_Str_08)` then
// `ArrayDtorImpl` — a per-element String RELEASE, not a trivial destroy.
// `UpdateLoop::_internalListA` (runtime/types/engine/UpdateLoop.h) is typed
// only as `void* _internalListA` — there is no `Collection::Array<T>` member
// here to call `.~Array()` on, and inventing the element type is struct work
// this "callers" step does not cover.
ListCleanupFn ListCleanup() {
    return reinterpret_cast<ListCleanupFn>(rtengine::AtomicResolve(g_listCleanup, [] {
        return reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180010ab0));
    }));
}
// 0x180010110 — Array<IO::Mod>::operator=, the deep copy Loader::Load runs when
// mod changes are switched OFF. Our tree already resolves this exact address as
// fn_ModArrayCopy (engine_funcs.cpp:1823).
ModArrayCopyFn ModArrayCopy() {
    return reinterpret_cast<ModArrayCopyFn>(rtengine::AtomicResolve(g_modArrayCopy, [] {
        return reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180010110));
    }));
}
// 0x18007a0d0 — Collection::Array<InternalPtr<UI::DialogInstance>>::SetSize
// (newSize, fillValue): the 8-byte-element, fill-the-new-tail instantiation.
// Both Startup bodies use it to append one dialog to UserInterface::_addDialogs.
// A DIFFERENT instantiation of the same template lives at 0x180076900 and is
// used by runtime/src/ui/element.cpp — the two addresses are not
// interchangeable, and the Wave 2 collection header explains why (the node/copy
// code is emitted per element type).
// WAVE 9 LANE Q: both addresses are now registered ports
// (runtime/src/collection/array.cpp), so this reaches our SetSize. They stay
// separate registrations for exactly the reason above.
//
// ENDGAME B.3.2 step 2 — the forwarder above is GONE at both call sites below:
// `UserInterface::_addDialogs` is already `Collection::Array<DialogInstancePtr>`
// (runtime/types/ui/UserInterface.h). 0x18007a0d0 is `kSpec_Fill8Z`
// (`Assign8Z`, a zero-then-copy double write — see achievement_manager.cpp's
// banner on the same assign body for why that keeps a row from being
// trait-equivalent even for a trivial pointer element), so it stays a
// hand-derived `rtcoll::AsArray<&kSpec_Fill8Z>` bridge. The row stays
// registered in array.cpp for runtime/src/ui/element.cpp's own instantiation.

// ---- exported DATA symbols --------------------------------------------------
template <typename T>
T* DataExport(const char* mangled) {
    return reinterpret_cast<T*>(ResolveExport(mangled));
}

// Wave 15 lane ORDER converted the three singleton accessors below from
// `static T** p = DataExport<T*>(...)` — a frozen static, which keeps the FIRST
// answer including a non-null one — to the caching door that now refuses to
// freeze a pre-init answer (rt_resolve_cache.cpp §6). All three names are Wave
// 15 rows. `DataExport` itself is left in place: `VTableUpdateLoop` and friends
// below resolve a `??_7` symbol whose value is an ADDRESS rather than a live
// mutable pointer, and freezing that is not the same hazard.
using ExternalFactorySlot = RtObject::ExternalFactory**;
using MainLoopSlot        = Engine::MainLoop**;
RtObject::ExternalFactory* Factory() {
    ExternalFactorySlot p = RT_DATA_SLOT(ExternalFactorySlot,
        "?_singleton@ExternalFactory@Object@@0PEAV12@EA");
    return p ? *p : nullptr;
}
// IO::Modification::_singleton. Note the `2PEAV12@EA` spelling — a PUBLIC
// static, where every neighbouring singleton is `0PEAV12@EA`; the other
// spelling is a phantom that resolves NULL in silence
// (reference_resolveexport_phantom_names.md).
uint8_t* ModificationSingleton() {
    uint8_t** p = RT_DATA_SLOT(uint8_t**, "?_singleton@Modification@IO@@2PEAV12@EA");
    return p ? *p : nullptr;
}
// Engine::MainLoop::_singleton — the DLL data symbol holding a MainLoop*.
//     IsRunning:          MOV RAX,[0x1801fe750]; CMP RCX,[RAX+0x20]
//     LoaderLoop::Exit:   MOV [RAX+0x28], this->_nextLoop
// so +0x20 is the CURRENT loop and +0x28 the PENDING one.
//
// Typed through lane B's runtime/types/engine/MainLoop.h rather than read by
// displacement. This lane derived +0x20/+0x28 from the disassembly above and
// lane B derived them from MainLoop::Run and SetNextLoop; the two agree, and
// going through the header means the compiler now checks that agreement instead
// of a hand-written `+ 0x20` in this file. `_currentLoop` is an
// `RtEngine::UpdateLoop*`, the same type this lane's ports take, so the
// comparison below is type-checked end to end.
Engine::MainLoop* MainLoopSingleton() {
    MainLoopSlot p = RT_DATA_SLOT(MainLoopSlot,
        "?_singleton@MainLoop@Engine@@1PEAV12@EA");
    return p ? *p : nullptr;
}
// UI::Dialog's BaseTypeInfo. Both Load bodies read its FIRST DWORD as the type
// id (`MOV EDX,[0x1801faa88]`), exactly as element.cpp reads its two.
uint32_t DialogTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}

// ENDGAME B.3.7: the four `??_7` reads are atomic slots too -- the ctor doors
// run on the loader thread (GameUpdateLoop::Load builds loops there).
void* VTableUpdateLoop() {
    return rtengine::AtomicExport(g_vtUpdateLoop, "??_7UpdateLoop@Engine@@6B@");               // 0x180155ba0
}
void* VTableLoaderLoop() {
    return rtengine::AtomicExport(g_vtLoaderLoop, "??_7LoaderLoop@Engine@@6B@");               // 0x180157940
}
void* VTableDialogLoopT() {
    return rtengine::AtomicExport(g_vtDialogLoopT, "??_7DialogLoopT@Engine@@6B@");             // 0x1801579a0
}
void* VTableDialogLoaderLoopT() {
    return rtengine::AtomicExport(g_vtDialogLoaderLoopT, "??_7DialogLoaderLoopT@Engine@@6B@"); // 0x1801578d0
}

// ============================================================================
// The two inlined idioms this family repeats
// ============================================================================

// ENDGAME B.3.1 step 2: the two hand-written idioms that used to live here are
// members of the real class now, and every one of their call sites below says
// so directly.
//
//   `StringRelease` was `~String` (0x180006c80), asymmetry and all — the
//   freeing branch leaves the object holding `data - 8`. That behaviour did not
//   move, it is inside the class (runtime/types/system/String.h).
//
//   `StringAcquire` (`dst = 0; if (src) { dst = src; ++src[-4]; }`) was the
//   copy constructor, 0x180006cb0.
//
// `_dialogFilename` is a member of `Engine::DialogLoopT` /
// `Engine::DialogLoaderLoopT`, neither of which is converted, so it keeps its
// POD type (B31_RULES, "Members of not-yet-converted structs") and its
// lifetime stays explicit at the two ends retail spells it: a placement
// copy-construct in the constructor and an explicit `~String` in the
// destructor. Both go through the one bridge, `RtSystem::Sys`.

// ============================================================================
// The two Time conversions this family inlines
// ============================================================================
// Both ctors and DialogLoopT::Update inline Timer's two static conversions
// rather than calling them, but the instruction sequences are IDENTICAL to the
// exported bodies (verified side by side):
//
//   ?SecondsToTicks@Timer@System@@SA_JM@Z  0x1800ca800
//       CVTSS2SD xmm1,xmm0 ; DIVSD xmm1,[Timer::_secondsPerTick] ; CVTTSD2SI
//   ?TicksToSeconds@Timer@System@@SAM_J@Z  0x1800ca820
//       CVTSI2SD xmm0,rcx ; MULSD xmm0,[Timer::_secondsPerTick] ; CVTPD2PS
//
// so calling them is bit-exact, and Timer::_secondsPerTick (a QPC-derived
// runtime global) never has to be duplicated on our side. This is the decision
// Wave 2 already took for System::Time's own ctor and AsSeconds
// (runtime/src/system/value_types.cpp) — same reasoning, same two exports.
//
// RT_EXPORT_ONCE since 2026-09-07 (Wave 9 lane R), and the reason is a good
// advertisement for running the checker after every landing: both names were
// UNPORTED when the paragraph above was written, so raw and non-raw reached the
// same DLL body and the choice was purely about GetProcAddress cost. Wave 9
// lane B3 has since PORTED both (runtime/src/system/timer.cpp) -- and at that
// instant these two lines would have become port bypasses, with nothing in this
// file changing. RT_EXPORT_ONCE keeps the caching the paragraph above argues
// for and reaches whichever implementation is armed.
int64_t SecondsToTicks(float seconds) {
    return RT_EXPORT_ONCE(SecondsToTicksFn, "?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
}
float TicksToSeconds(int64_t ticks) {
    return RT_EXPORT_ONCE(TicksToSecondsFn, "?TicksToSeconds@Timer@System@@SAM_J@Z")(ticks);
}

// ============================================================================
// The constructor bodies, factored out of their diffed entry points
// ============================================================================
// Retail's derived ctors CALL Engine::UpdateLoop::UpdateLoop. Ours call this
// helper instead, for the reason Wave 7 lane A gives for FindElement's private
// recursion: going through the diffed entry point would make every DialogLoopT
// construction run a nested UpdateLoop build-oracle as well, doubling the work
// and the scratch traffic for no extra coverage — the derived ctor's own oracle
// already compares all 0x28 base bytes.

// ??0UpdateLoop@Engine@@QEAA@XZ  0x1800ddd30
//
// The bit allocation is the whole of this ctor. ExternalFactory keeps a 32-bit
// mask at +0x04; the loop scans indices 0..31 for the lowest CLEAR bit with
// `BT`, sets it, and stores the INDEX at this->_loadBit — or stores -1 when all
// 32 are taken. Note what is NOT written: +0x20 is written last, from EAX, and
// the ctor never touches +0x24 after zeroing it.
void UpdateLoopCtorBody(UpdateLoop* self) {
    self->_vtable        = VTableUpdateLoop();
    // ENDGAME B.3.8 block RB (RULE B8-1): the rest is ONE body with the real
    // constructor: `Engine::UpdateLoop::InitFields`, below the ports banner.
    Engine::UpdateLoop::DoorBody::Init(reinterpret_cast<Engine::UpdateLoop*>(self));   // B.3.8 bridge
}

// ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z  0x1800dc1e0
void LoaderLoopCtorBody(LoaderLoop* self, void* nextLoop, const ::Time* delay) {
    UpdateLoopCtorBody(&self->_base);
    self->_base._vtable = VTableLoaderLoop();
    // ENDGAME B.3.8 block RB (RULE B8-1): ONE body with the real constructor.
    Engine::LoaderLoop::DoorBody::Init(reinterpret_cast<Engine::LoaderLoop*>(self),   // B.3.8 bridge
                                       static_cast<Engine::UpdateLoop*>(nextLoop), delay->ticks);
}

// ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z  0x1800db3f0
void DialogLoopTCtorBody(DialogLoopT* self, const RtSystem::String* filename) {
    UpdateLoopCtorBody(&self->_base);
    self->_base._vtable   = VTableDialogLoopT();
    new (&self->_dialogFilename) System::String(RtSystem::Sys(*filename));
    // ENDGAME B.3.8 block RB (RULE B8-1): ONE body with the real constructor
    // (whose member initialiser is the String copy above).
    Engine::DialogLoopT::DoorBody::Init(reinterpret_cast<Engine::DialogLoopT*>(self));   // B.3.8 bridge
}

// ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
// 0x1800db0d0
void DialogLoaderLoopTCtorBody(DialogLoaderLoopT* self, const RtSystem::String* filename,
                               void* nextLoop, const ::Time* delay) {
    LoaderLoopCtorBody(&self->_base, nextLoop, delay);
    self->_base._base._vtable = VTableDialogLoaderLoopT();
    new (&self->_dialogFilename) System::String(RtSystem::Sys(*filename));
    Engine::DialogLoaderLoopT::DoorBody::Init(reinterpret_cast<Engine::DialogLoaderLoopT*>(self));   // B.3.8 bridge
}

// ============================================================================
// The build oracle
// ============================================================================
// A constructor cannot be replayed onto the object it just built, and it cannot
// be replayed onto a scratch copy either without leaking: every one of these
// ctors takes a slot out of ExternalFactory's 32-bit allocation mask, and the
// two dialog ctors also take a reference on the caller's filename String.
//
// So this is the Wave 7 "poison twin", with one addition. Two scratch buffers
// are filled with the SAME poison byte; ours runs on one and the DLL's on the
// other; all N bytes are compared. The poison makes every byte NEITHER writes
// compare equal by construction, so no hand-written mask of unwritten ranges is
// needed at all — which is the trap that read as a catastrophic port defect in
// Wave 6 and cost a day.
//
// The addition: the allocation-mask slot each run takes is UNDONE BETWEEN THE
// TWO RUNS, so both runs see an identical factory mask and allocate the SAME
// bit index. Without that, +0x20 would differ for a purely bookkeeping reason
// and would have to be excluded; with it, `_loadBit` is compared like every
// other field. The undo is `mask &= ~(1 << bit)` — the last step of
// ~UpdateLoop, and deliberately NOT the whole destructor: ~UpdateLoop also
// calls ExternalFactory::Clean(), a real global sweep of unreferenced
// resources, and a scratch object that never loaded anything has nothing for
// Clean to collect. Running it would be a behaviour change the oracle imposed
// on the boot path, not an undo.
//
// The string references the two dialog ctors take are released after the
// comparison, one per run, with the same idiom the destructor uses.

constexpr uint8_t kPoison = 0xCD;
constexpr size_t  kScratchMax = sizeof(DialogLoaderLoopT);   // 0x188, the largest

alignas(16) uint8_t g_scratchOurs[kScratchMax];
alignas(16) uint8_t g_scratchTheirs[kScratchMax];

// The lane's ctors nest (DialogLoaderLoopT -> LoaderLoop -> UpdateLoop) but
// only through the private *CtorBody helpers above, which never touch the
// scratch. This guard exists so that a future edit that DOES route a nested
// construction through a diffed entry point degrades to "no oracle for the
// inner one" rather than to two frames stamping on one buffer.
int g_scratchDepth = 0;

struct ScratchGuard {
    bool held;
    ScratchGuard() : held(g_scratchDepth == 0) { ++g_scratchDepth; }
    ~ScratchGuard() { --g_scratchDepth; }
};

void ReleaseBitFor(const UpdateLoop* built) {
    if (built->_loadBit < 0) return;
    RtObject::ExternalFactory* factory = Factory();
    if (!factory) return;
    factory->_cleanBitMask &= ~(1u << built->_loadBit);
}

} // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body below is byte-faithful to the retail DISASSEMBLY, not only to
// Ghidra's C (reference_ghidra_c_output_vs_disassembly.md: the decompiler drops
// dereferences and renders MOVSXD as a zero extension; three Wave 6 defects,
// including the crash, came from trusting it). No null guard retail does not
// have — CLAUDE.md's rule holds here even where one would look free, and these
// loops are the boot path, where retail's unguarded derefs are load bearing.

// ============================================================================
// ENDGAME B.3.8 block RB (RULE B8-1): the loop bases' shared ctor/dtor bodies.
// Each is its door's body MINUS the vptr stores, moved verbatim; the real
// constructors/destructors (runtime/types/engine/UpdateLoop.h) and the doors
// below both run them.
// ============================================================================
// ??0UpdateLoop@Engine@@QEAA@XZ 0x1800ddd30: the clean-bit allocation.
void Engine::UpdateLoop::InitFields() {
    _resourceList  = nullptr;
    _internalListA = nullptr;
    _internalListB = nullptr;
    _loadProgress  = 0;

    RtObject::ExternalFactory* factory = Factory();
    // The allocation mask is the dword at factory + 0x04. Wave 3 declared that
    // word `_pad04`; this port is what named it, because the only two functions
    // that touch it are this ctor and ~UpdateLoop — see the correction note in
    // runtime/types/object/External.h.
    uint32_t& mask = factory->_cleanBitMask;

    int32_t bit = 0;
    for (;;) {
        if ((mask & (1u << bit)) == 0) {          // BT / JNC
            mask |= (1u << bit);
            break;
        }
        ++bit;
        if (bit >= 32) { bit = -1; break; }        // OR EAX,-1
    }
    _loadBit = bit;
}

// ??1UpdateLoop@Engine@@UEAA@XZ 0x1800dddb0 after the restamp.
void Engine::UpdateLoop::ReleaseFields() {
    const int32_t bit = _loadBit;
    if (bit >= 0) {
        RtObject::ExternalFactory* factory = Factory();
        // The dtor hands the slot back in three steps, in this order: publish
        // the index as the factory's _cleanBitIndex, sweep, THEN clear the bit.
        // Clean (0x1800be310) reads _cleanBitIndex and evicts every resource
        // whose usage mask reaches 0 once this bit is cleared from it, so the
        // order is load bearing — clearing the mask first would sweep for the
        // wrong bit.
        factory->_cleanBitIndex = bit;
        ExternalFactoryClean()(factory);
        // Retail re-reads this->_loadBit here rather than reusing the register
        // it tested; reproduced, because Clean could in principle reach code
        // that writes it.
        factory->_cleanBitMask &= ~(1u << _loadBit);
    }


    // Tail call in retail: `LEA RCX,[RBX+0x10]; JMP 0x180010ab0`.
    ListCleanup()(&_internalListA);
}

// ??0LoaderLoop 0x1800dc1e0 after UpdateLoop() and the stamp.
void Engine::LoaderLoop::InitFields(UpdateLoop* nextLoop, int64_t delayTicks) {
    _nextLoop     = nextLoop;
    // XORPS xmm0 / DIVSD / CVTTSD2SI — i.e. System::Time(0.0f). Written out as
    // the conversion rather than folded to the literal 0 because that is what
    // the instructions say; it IS 0 for every non-zero divisor.
    _elapsedTicks = SecondsToTicks(0.0f);
    _delayTicks   = delayTicks;
    _autoAdvance  = 1;
    // +0x41..+0x47 are NOT written (reference_oracle_must_not_compare_unwritten_bytes.md).
}

// ??0DialogLoopT 0x1800db3f0 / ??0DialogLoaderLoopT 0x1800db0d0 after the
// String copy: the two nulls and the UserInterface ctor door on the embedded UI.
void Engine::DialogLoopT::InitFields() {
    _dialogExternal = nullptr;
    _dialogInstance = nullptr;
    UserInterfaceCtor()(&_ui);
}
void Engine::DialogLoaderLoopT::InitFields() {
    _dialogExternal = nullptr;
    _dialogInstance = nullptr;
    UserInterfaceCtor()(&_ui);
}
// ??1DialogLoopT 0x1800db340 / ??1DialogLoaderLoopT 0x1800db060: the
// UserInterface teardown. The ~String and the base teardown after it are the
// compiler's member/base chain in the real destructor, the doors' own lines.
void Engine::DialogLoopT::ReleaseFields()       { UserInterfaceDtor()(&_ui); }
void Engine::DialogLoaderLoopT::ReleaseFields() { UserInterfaceDtor()(&_ui); }

// ---- ??0UpdateLoop@Engine@@QEAA@XZ  0x1800ddd30 ord 468 ---------------------
extern "C" UpdateLoop* RtEngine_UpdateLoop_Ctor(UpdateLoop* self) {
    UpdateLoopCtorBody(self);

    return self;
}

// ---- ??1UpdateLoop@Engine@@UEAA@XZ  0x1800dddb0 ord 704 ---------------------
// NO REPLAY IS POSSIBLE and none is attempted: running the DLL's destructor
// beside ours would hand the same bit index back twice and run
// ExternalFactory::Clean an extra time, which evicts real resources. The oracle
// is therefore a SHADOW predicate over what the body is supposed to have
// achieved, and its verdict names every term it tests, so a failure says which
// one failed rather than "something differed"
// (reference_shadow_predicate_must_name_its_failing_term.md).
extern "C" void RtEngine_UpdateLoop_Dtor(UpdateLoop* self) {

    // The standard MSVC base-destructor step: re-stamp the base vtable, so a
    // virtual dispatched during the rest of the teardown reaches the BASE
    // implementation and not the already-destroyed derived one.
    self->_vtable = VTableUpdateLoop();

    // ENDGAME B.3.8 block RB (RULE B8-1): the teardown is ONE body with the
    // real destructor: `Engine::UpdateLoop::ReleaseFields`.
    Engine::UpdateLoop::DoorBody::Release(reinterpret_cast<Engine::UpdateLoop*>(self));   // B.3.8 bridge
}

// ============================================================================
// BATCH 8: the three remaining family dtors — ~LoaderLoop, ~DialogLoopT,
// ~DialogLoaderLoopT. All three end in exactly the base cleanup above (the
// vtable restamp to UpdateLoop's own table, the ExternalFactory bit release,
// the ListCleanup tail call); factored out so each dtor states only what it
// adds over that base, matching how retail's decompile actually reads (see
// the three functions below for the decompiled shape each one open-codes).
// ============================================================================
void UpdateLoopBaseTeardown(UpdateLoop* self) {
    self->_vtable = VTableUpdateLoop();
    Engine::UpdateLoop::DoorBody::Release(reinterpret_cast<Engine::UpdateLoop*>(self));   // B.3.8 bridge
}

// ---- ??1LoaderLoop@Engine@@UEAA@XZ  0x1800dc170 ord 626 ---------------------
// Retail's decompile shows LoaderLoop's OWN vtable stamped and then
// immediately overwritten with UpdateLoop's, with nothing observable between
// the two stores: LoaderLoop adds no destructible members over UpdateLoop
// (a raw pointer and three ints), so MSVC folded ~LoaderLoop's body straight
// into ~UpdateLoop's rather than emitting a real call down to it. The net
// effect is UpdateLoopBaseTeardown once LoaderLoop's own vtable has been
// written, which is reproduced here as two statements rather than one,
// exactly because retail's own listing has two stores.
extern "C" void RtEngine_LoaderLoop_Dtor(LoaderLoop* self) {
    self->_base._vtable = VTableLoaderLoop();
    UpdateLoopBaseTeardown(&self->_base);
}

// ---- ??1DialogLoopT@Engine@@UEAA@XZ  0x1800db340 ord 576 --------------------
// Adds, over the base: destroy the embedded UI::UserInterface at +0x40 (its
// own exported dtor, already ported — runtime/src/ui/userinterface.cpp), then
// release `_dialogFilename` (+0x28) with `~String` (0x180006c80) BEFORE the
// base teardown restamps the vtable to UpdateLoop's and runs the
// ExternalFactory/ListCleanup pair.
extern "C" void RtEngine_DialogLoopT_Dtor(DialogLoopT* self) {
    self->_base._vtable = VTableDialogLoopT();
    Engine::DialogLoopT::DoorBody::Release(reinterpret_cast<Engine::DialogLoopT*>(self));   // B.3.8 bridge
    RtSystem::Sys(self->_dialogFilename).~String();
    UpdateLoopBaseTeardown(&self->_base);
}

// ---- ??1DialogLoaderLoopT@Engine@@UEAA@XZ  0x1800db060 ord 575 --------------
// Same shape as ~DialogLoopT, over the LoaderLoop base rather than UpdateLoop
// directly: own vtable, destroy the UI at +0x60, release `_dialogFilename`
// (+0x48), then tail into ~LoaderLoop(this) — reproduced here as the same
// LoaderLoop-vtable restamp plus UpdateLoopBaseTeardown, per the note above
// ~LoaderLoop on why those two collapse into one sequence.
extern "C" void RtEngine_DialogLoaderLoopT_Dtor(DialogLoaderLoopT* self) {
    self->_base._base._vtable = VTableDialogLoaderLoopT();
    Engine::DialogLoaderLoopT::DoorBody::Release(reinterpret_cast<Engine::DialogLoaderLoopT*>(self));   // B.3.8 bridge
    RtSystem::Sys(self->_dialogFilename).~String();
    self->_base._base._vtable = VTableLoaderLoop();
    UpdateLoopBaseTeardown(&self->_base._base);
}

// ---- ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ  0x1800dd7b0 ord 3160 -----------
// The one genuinely const, side-effect-free function in the lane, so it gets a
// full replay oracle. Note it is a member that tests its own `this` for null:
// retail really does `TEST RCX,RCX / JZ`, so the guard is FAITHFUL, not one we
// added (CLAUDE.md's rule cuts the other way here — removing it would diverge).
// ENDGAME B.3.7: the body is the member; the door keeps retail's null-`this`
// test (`TEST RCX,RCX / JZ`), which a member cannot express.
bool Engine::UpdateLoop::IsRunning() const {
    // _currentLoop (+0x20) is the loop MainLoop::Run is driving right now.
    // Retail dereferences the singleton unguarded; so do we — CLAUDE.md's
    // rule holds, and by the time any UpdateLoop exists MainLoop does too.
    return MainLoopSingleton()->_currentLoop == this;
}

extern "C" bool RtEngine_UpdateLoop_IsRunning(const UpdateLoop* self) {
    if (!self) return false;
    return reinterpret_cast<const Engine::UpdateLoop*>(self)
        ->Engine::UpdateLoop::IsRunning();                           // B.3.7 bridge
}

// ---- ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z --------
//      0x1800dc1e0 ord 261
extern "C" LoaderLoop* RtEngine_LoaderLoop_Ctor(LoaderLoop* self, void* nextLoop,
                                                const ::Time* delay) {
    LoaderLoopCtorBody(self, nextLoop, delay);

    return self;
}

// ---- ?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z  0x1800dc050 ord 3204 ----
// NO ORACLE, and the marker says so rather than pretending to one. This
// function switches the mod filesystem over and then WAKES THE LOADING THREAD:
// replaying it would remount the packages and start a second background load of
// the same UpdateLoop. Everything it does is an exported call whose own port
// (or whose DLL body) is verified elsewhere; what this port can get wrong is
// the ORDER and the two byte flags, and those are stated field by field below
// so a reviewer can check them against the disassembly without re-deriving it.
void Engine::Loader::Load(UpdateLoop* loop) {

    _loop = loop;

    // slot 11 (+0x58) = UpdateLoop::AllowsModChanges. Base returns false
    // (0x180031790); a menu loop overrides it to true. ENDGAME B.3.7: an
    // ordinary virtual call -- it dispatches through the loop's own (exe or
    // minted) table exactly as the hand-inlined `vtable[11]` did.
    const bool allowsModChanges = loop->AllowsModChanges();

    uint8_t* mod = ModificationSingleton();
    RtObject::ExternalFactory* factory = Factory();

    // IO::Modification +0x00 = _dirty, +0x01 = _savingEnabled
    // (engine/types/Modification.h). Retail compares the stored byte against
    // the bool it just got and only acts when they differ.
    if (mod[1] != static_cast<uint8_t>(allowsModChanges)) {
        if (allowsModChanges) {
            mod[1] = static_cast<uint8_t>(allowsModChanges);
            ModificationRefreshList()(mod);
            mod[0] = 1;
        } else {
            mod[1] = 0;
            // Array<Mod>::operator=(&_savedMods, &_mods) — dst +0x18, src +0x08.
            ModArrayCopy()(mod + 0x18, mod + 0x08);
        }
    }

    // ExternalFactory::_loadEnabled held across the remount, so the
    // FindOrLoad cache-hit path re-registers each resource against the current
    // clean bit (runtime/types/object/External.h documents +0x41).
    factory->_loadEnabled = true;
    ModificationSetupFileSystem()(mod);
    factory->_loadEnabled = false;

    // Retail inlines System::Thread::Start here:
    //     ResetEvent(this->hFinished)   [+0x28]
    //     SetEvent  (this->hStart)      [+0x18]
    // which is instruction-for-instruction ?Start@Thread@System@@QEAAXXZ
    // (0x1800ca610). Calling the ported Start keeps the two Win32 calls behind
    // the platform seam instead of dragging <windows.h> into this file.
    // ENDGAME B.3.7: `Loader` IS a `System::Thread` now; `Start` is a
    // non-virtual member of the base (thread_win.cpp), called directly.
    Start();
}

extern "C" void RtEngine_Loader_Load(Loader* self, void* loop) {
    reinterpret_cast<Engine::Loader*>(self)->Engine::Loader::Load(   // B.3.7 bridge
        static_cast<Engine::UpdateLoop*>(loop));
}

// ---- ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z  0x1800db3f0 ord 153 -
extern "C" DialogLoopT* RtEngine_DialogLoopT_Ctor(DialogLoopT* self,
                                                  const RtSystem::String* filename) {
    DialogLoopTCtorBody(self, filename);

    return self;
}

// ---- ?Load@DialogLoopT@Engine@@UEAAXXZ  0x1800db140 ord 3201 ----------------
// SHADOW, not replay: FindOrLoad either returns a cached External or LOADS the
// dialog resource off disk and inserts it into the global resource table.
// Running it twice is a second cache lookup with a real side effect on the
// clean-bit usage masks, and on a cold cache a second file read.
void Engine::DialogLoopT::Load() {
    // LOADER THREAD (dispatched by Loader::Run's slot-6 call).
    //
    // The local is a System::Filename, layout-identical to String; retail
    // copies the String field into it and normalises the delimiters in place —
    // which is the whole body of `??0Filename@System@@QEAA@AEBVString@1@@Z`
    // (0x18000c780), so the Adopt/AdjustDelimiters pair IS this construction,
    // and RAII is the release retail does at the bottom.
    System::Filename tmp(_dialogFilename);

    void* external = FindOrLoad()(Factory(), &tmp);
    void* result = nullptr;
    if (external) {
        // `? external : nullptr` is a CMOVNZ on the type test, not a branch
        // that skips the store: the field is written either way.
        if (IsDerivedFrom()(external, DialogTypeId())) result = external;
    }
    _dialogExternal = result;
}

extern "C" void RtEngine_DialogLoopT_Load(DialogLoopT* self) {
    reinterpret_cast<Engine::DialogLoopT*>(self)->Engine::DialogLoopT::Load();  // B.3.7 bridge
}

// ---- ?Startup@DialogLoopT@Engine@@UEAAXXZ  0x1800db2f0 ord 4093 -------------
// SHADOW: slot 12 CREATES a dialog instance (a live Object::Internal with a
// handle-table entry) and the body then queues it for display. A replay builds
// a second dialog and shows two.
void Engine::DialogLoopT::Startup() {

    // Slot 12, CreateDialog, returns InternalPtr<UI::DialogInstance> BY VALUE —
    // an 8-byte class type, so MSVC still gives it a HIDDEN RETURN SLOT in RDX
    // and returns that pointer in RAX (an `int`-typed or missing return slot in
    // an fn_* typedef is the wild-write family this project has paid for twice:
    // reference_hidden_retslot_wild_write.md). ENDGAME B.3.7: the slot is
    // declared retslot-first on the class, so this is an ordinary virtual call.
    void* ret = nullptr;
    Object::InternalPtr<UI::DialogInstance>* retSlot = CreateLoopDialog(
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&ret));

    void* dialog = *reinterpret_cast<void**>(retSlot);
    _dialogInstance = dialog;
    ret = dialog;   // retail re-stores it into the same slot before passing it on

    // dialog->OnOpened() -- slot 4 of UI::DialogInstance (B.3.6), the virtual
    // retail calls before the instance is queued.
    reinterpret_cast<UI::DialogInstance*>(dialog)->OnOpened();       // B.3.7 bridge

    // _ui._addDialogs.SetSize(size + 1, ret) — the append. Update drains that
    // array back-to-front into the live dialog list next frame
    // (runtime/types/ui/UserInterface.h).
    // retail 0x18007a0d0 — kSpec_Fill8Z; see the banner above.
    rtcoll::AsArray<&rtcoll::kSpec_Fill8Z>(&_ui._addDialogs)
        ->SetSizeFill(_ui._addDialogs.GetSize() + 1, &ret);
}

extern "C" void RtEngine_DialogLoopT_Startup(DialogLoopT* self) {
    reinterpret_cast<Engine::DialogLoopT*>(self)->Engine::DialogLoopT::Startup();  // B.3.7 bridge
}

// ---- ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z  0x1800db270 -----
//      ord 4485
// Partly oracled, and the split is the point. The body has two halves: a PURE
// Time round-trip through float, and a call into UI::UserInterface::Update
// which advances the whole front end. The round trip is replayed against the
// DLL's own two Timer conversions on every call — that is a full oracle for the
// only arithmetic in the function. The UI call is not replayed, for the obvious
// reason.
bool Engine::DialogLoopT::Update(const System::Time& time) {

    // CVTSI2SD / MULSD / CVTPD2PS / CVTPS2PD / DIVSD / CVTTSD2SI — i.e. the
    // tick count narrowed to a float number of seconds and converted straight
    // back. The narrowing is LOSSY and is the whole reason this line exists;
    // folding it away would diverge on any tick count over 2^24.
    System::Time rounded;
    rounded._ticks = SecondsToTicks(TicksToSeconds(time._ticks));

    _ui.Update(rounded);
    return true;   // MOV AL,1 — unconditional
}

extern "C" bool RtEngine_DialogLoopT_Update(DialogLoopT* self, const ::Time* time) {
    return reinterpret_cast<Engine::DialogLoopT*>(self)->Engine::DialogLoopT::Update(  // B.3.7 bridge
        *reinterpret_cast<const System::Time*>(time));
}

// ---- ??0DialogLoaderLoopT@Engine@@…  0x1800db0d0 ord 151 --------------------
extern "C" DialogLoaderLoopT* RtEngine_DialogLoaderLoopT_Ctor(
        DialogLoaderLoopT* self, const RtSystem::String* filename,
        void* nextLoop, const ::Time* delay) {
    DialogLoaderLoopTCtorBody(self, filename, nextLoop, delay);

    return self;
}

// ---- ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ  0x1800dad50 ord 3200 ----------
// Byte-for-byte the same body as DialogLoopT::Load with the LoaderLoop base's
// +0x20 offsets. Written out rather than shared, because retail emitted two
// bodies and a shared helper would hide a divergence between them.
void Engine::DialogLoaderLoopT::Load() {
    // LOADER THREAD (dispatched by Loader::Run's slot-6 call).
    //
    // As DialogLoopT::Load above: the Adopt/AdjustDelimiters pair is the
    // Filename ctor 0x18000c780 and the release at the bottom is ~Filename.
    System::Filename tmp(_dialogFilename);

    void* external = FindOrLoad()(Factory(), &tmp);
    void* result = nullptr;
    if (external) {
        if (IsDerivedFrom()(external, DialogTypeId())) result = external;
    }
    _dialogExternal = result;
}

extern "C" void RtEngine_DialogLoaderLoopT_Load(DialogLoaderLoopT* self) {
    reinterpret_cast<Engine::DialogLoaderLoopT*>(self)                // B.3.7 bridge
        ->Engine::DialogLoaderLoopT::Load();
}

// ---- ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ  0x1800db000 ord 4092 -------
// The DialogLoopT::Startup body plus a TAIL CALL into Loader::Load, which is
// what actually starts the background load behind a loading screen:
//     MOV RDX,[RBX+0x28]           ; this->_base._nextLoop
//     LEA RCX,[0x1801fe7b8]        ; &LoaderLoop::_loader
//     JMP 0x1800dc050              ; Engine::Loader::Load
// Note RCX is a LEA, not a load: `_loader` IS the object, not a pointer to one.
// LoaderLoop::_loader, defined further down (the one spelling of the slot).
namespace { Engine::Loader* LoaderLoopLoader(); }

void Engine::DialogLoaderLoopT::Startup() {

    // The same slot-12 call as DialogLoopT::Startup (see there).
    void* ret = nullptr;
    Object::InternalPtr<UI::DialogInstance>* retSlot = CreateLoopDialog(
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&ret));

    void* dialog = *reinterpret_cast<void**>(retSlot);
    _dialogInstance = dialog;
    ret = dialog;

    reinterpret_cast<UI::DialogInstance*>(dialog)->OnOpened();       // B.3.7 bridge

    // retail 0x18007a0d0 — kSpec_Fill8Z; see the banner above.
    rtcoll::AsArray<&rtcoll::kSpec_Fill8Z>(&_ui._addDialogs)
        ->SetSizeFill(_ui._addDialogs.GetSize() + 1, &ret);

    // The tail call `LEA RCX,[LoaderLoop::_loader]; JMP Loader::Load`.
    // `_loader` is the DLL-data slot (carry-over 5), resolved through the
    // port-aware door and never minted by this file; ENDGAME B.3.7: `Load` is
    // a direct member call now (it was a function-local `static` export cache
    // -- a B6-5 site -- before).
    LoaderLoopLoader()->Load(_nextLoop);
}

extern "C" void RtEngine_DialogLoaderLoopT_Startup(DialogLoaderLoopT* self) {
    reinterpret_cast<Engine::DialogLoaderLoopT*>(self)                // B.3.7 bridge
        ->Engine::DialogLoaderLoopT::Startup();
}

// ============================================================================
// WAVE 25 lane VTIO — the SIX SHARED UpdateLoop BASE SLOTS
// ============================================================================
// WHY THESE SIX, AND WHY TOGETHER.  The four Engine loop vtables
// (`??_7UpdateLoop@`, `??_7LoaderLoop@`, `??_7DialogLoopT@`,
// `??_7DialogLoaderLoopT@`) are FOUR census gap rows but they are NOT four
// independent bodies of work.  Their 43 concrete slot-instances resolve to only
// 26 DISTINCT function addresses, and SIX of those addresses appear in all four
// tables (five of them) or two (OnVideoError).  Measured, not assumed — the
// union was taken from the shipped `.rdata` by reading each table's qwords:
//
//   0x180031740  ?DrawFailed@UpdateLoop@           slot 5   in all 4 tables
//   0x180031750  ?SaveRestore@UpdateLoop@          slot 7   in all 4 tables
//   0x180031760  ?OnVideoError@UpdateLoop@         slot 8   in DialogLoopT + UpdateLoop
//   0x180031770  ?OnVideoRestart@UpdateLoop@       slot 9   in all 4 tables
//   0x180031780  ?GetResourceFilename@UpdateLoop@  slot 10  in all 4 tables
//   0x180031790  ?AllowsModChanges@UpdateLoop@     slot 11  in all 4 tables
//
// These are the `UpdateLoop` base class's own default implementations, which
// every derived loop inherits unchanged.  All six sit at 16-byte spacing, and
// every one of them is a LEAF OF AT MOST THREE BYTES.  Read out of retail
// (`objdump -d --start-address=0x180031740 --stop-address=0x1800317d0`):
//
//   180031740:  c2 00 00        ret  $0x0       <- returns, writes nothing
//   180031750:  c2 00 00        ret  $0x0
//   180031760:  c2 00 00        ret  $0x0
//   180031770:  c2 00 00        ret  $0x0
//   180031780:  33 c0 c3        xor %eax,%eax ; ret    -> (const char*)nullptr
//   180031790:  32 c0 c3        xor %al,%al   ; ret    -> false
//
// `c2 00 00` is `RET 0` — MSVC's retn-with-zero-immediate, semantically a plain
// return on x64.  RCX (`this`) is never read by any of the six, so none of them
// touches the object at all.
//
// WHAT PORTING THEM DOES, AND WHAT IT DOES NOT.
//   IT DOES: take `??_7UpdateLoop@Engine@@6B@` from ours 1/7 to **7/7
//     COMPLETE**, and move LoaderLoop 1/12 -> 6/12, DialogLoopT 4/12 -> 10/12,
//     DialogLoaderLoopT 3/12 -> 8/12.  At CUTOVER these six bodies are
//     REQUIRED: retail's are gone and the minted tables are the only owner.
//   IT DOES NOT: promote anything, or change any dispatch today.  COMPLETE is
//     the STATIC half of readiness only (WAVE20_GUIDE §2), and all four tables
//     still have retail stampers.  It also moves NO census column: an unpromoted
//     vtable's slots move nothing, exactly as Wave 21 lane ENC recorded.  The
//     value claimed here is cutover readiness, not a number.
//
// FAITHFULNESS.  Each body is written as retail's, unguarded.  The two
// value-returning ones matter more than they look: `?GetResourceFilename@`
// returns `PEBD` (const char*) and retail zeroes EAX, so the answer is a null
// POINTER, not an empty string; `?AllowsModChanges@` returns `_N` and retail
// zeroes only AL, leaving the upper bits of EAX undefined — the Win64 ABI
// defines only AL for a bool return, so `return false` is correct and a caller
// that read EAX would be reading retail's garbage too.
// ----------------------------------------------------------------------------
// `self` is deliberately unnamed-but-typed `void*`: retail never reads RCX, and
// giving these a real `UpdateLoop*` would imply a field access that does not
// exist.  The RT_DIFF sites are labelled with their CLASS in their own name
// (WAVE25_KICKOFF §1b): there is nothing to compare inside a body that reads
// nothing and returns nothing, so the four void slots carry an honest
// [ATTENDANCE] site rather than a `divergences=0` site that cannot fail.  The
// real check on all six is the [ORACLE] probe below, which consults the DLL.

// ENDGAME B.3.7: the six bodies are members of Engine::UpdateLoop; each door
// delegates with a QUALIFIED call (the doors sit in minted and exe tables, so
// an unqualified call would dispatch through those tables).
void Engine::UpdateLoop::DrawFailed() {
    // retail 0x180031740: `ret`. Nothing else.
}

void Engine::UpdateLoop::SaveRestore(bool /*saving*/, Save::SavedState* /*state*/) {
    // retail 0x180031750: `ret`. Both arguments are ignored by the base class.
}

void Engine::UpdateLoop::OnVideoError() {
    // retail 0x180031760: `ret`.  NOTE this is UpdateLoop's OnVideoError;
    // LoaderLoop overrides it with its OWN body at 0x1800dc360.
}

void Engine::UpdateLoop::OnVideoRestart() {
    // retail 0x180031770: `ret`.
}

const char* Engine::UpdateLoop::GetResourceFilename() {
    return nullptr;   // retail 0x180031780: xor eax,eax ; ret
}

bool Engine::UpdateLoop::AllowsModChanges() {
    return false;     // retail 0x180031790: xor al,al ; ret
}

extern "C" void RtEngine_UpdateLoop_DrawFailed(void* self) {
    reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::DrawFailed();  // B.3.7 bridge
}

extern "C" void RtEngine_UpdateLoop_SaveRestore(void* self, bool saving,
                                                void* savedState) {
    reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::SaveRestore(  // B.3.7 bridge
        saving, static_cast<Save::SavedState*>(savedState));
}

extern "C" void RtEngine_UpdateLoop_OnVideoError(void* self) {
    reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::OnVideoError();  // B.3.7 bridge
}

extern "C" void RtEngine_UpdateLoop_OnVideoRestart(void* self) {
    reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::OnVideoRestart();  // B.3.7 bridge
}

extern "C" const char* RtEngine_UpdateLoop_GetResourceFilename(void* self) {
    return reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::GetResourceFilename();  // B.3.7 bridge
}

extern "C" bool RtEngine_UpdateLoop_AllowsModChanges(void* self) {
    return reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::AllowsModChanges();  // B.3.7 bridge
}


// ============================================================================
// WAVE 27 lane LOOPS — the twelve remaining slot bodies of the four tables
// ============================================================================
// Lane scope was `??_7LoaderLoop@` 6/12, `??_7DialogLoaderLoopT@` 8/12,
// `??_7DialogLoopT@` 10/12 and `??_7Loader@` 1/2. Those four counts name TWELVE
// DISTINCT unported addresses, not 6+4+2+1 — `?OnVideoError@LoaderLoop@`
// (0x1800dc360) is slot 8 of BOTH LoaderLoop and DialogLoaderLoopT, so one body
// closes two slot-instances. The per-table slot maps were read out of the
// shipped `.rdata` and every address resolved through the PE export directory;
// see docs/runtime_port/WAVE27_LANE_LOOPS.md §2 for the full table.
//
// *** READ THIS BEFORE USING GHIDRA MCP ON THIS REGION ***
// Ghidra's retail database has WRONG FUNCTION BOUNDARIES here: it believes
// `SoundEnded` spans 0x1800dc0c0..0x1800dc2b0, which swallows
// `?Startup@LoaderLoop@` (0x1800dc260) and `?Shutdown@LoaderLoop@`
// (0x1800dc280), so `disassemble_function` on either address silently returns
// SoundEnded's body instead. Every body below was therefore disassembled with
// `objdump -d --start-address=… --stop-address=… Runtime-x64.dll`, which has no
// notion of function boundaries and cannot make that substitution.
//
// ----------------------------------------------------------------------------
// PORTABILITY (PLAN.md §4.1 decision 1)
// ----------------------------------------------------------------------------
// Three of these bodies end in `WaitForSingleObject(<HANDLE at 0x1801fe7e0>,
// INFINITE)`. That address is `_loader._thread.hFinished` (0x1801fe7b8 + 0x28,
// and Thread::hFinished is at +0x28 by runtime/types/system/Thread.h) — i.e.
// retail inlined `System::Thread::WaitForFinish`. So the port calls the
// REGISTERED `?WaitForFinish@Thread@System@@QEAAXXZ`, whose own body
// (runtime/src/system/thread_win.cpp:274) is that one Win32 call. No
// <windows.h> enters this file.
// ============================================================================

namespace {

// ---- ?_loader@LoaderLoop@Engine@@1VLoader@2@A  0x1801fe7b8  ord 4794 --------
// ADDRESS DOOR NOTE (kickoff §4 #10 shape) — this is a NAME door, not an
// address door, and that is deliberate:
//   SYMBOL      ?_loader@LoaderLoop@Engine@@1VLoader@2@A (DATA, ord 4794)
//   CONDITION   the row is currently RT_UNPROMOTABLE in reg_mint_promoted.cpp,
//               so ResolveExport falls through to the DLL's own static object.
//   CONSEQUENCE that is REQUIRED, not tolerated: the DLL's loader thread was
//               created on THAT object and waits on ITS events. If the row is
//               ever promoted while the DLL is present, every body below would
//               silently start operating on a different, unwatched Loader and
//               the loading screen would hang forever rather than crash.
// The existing `?Startup@DialogLoaderLoopT@` port above resolves the same
// symbol the same way; this helper just gives the four new users one spelling.
// ENDGAME B.3.7 (RULE B7-7.1): this was `static Loader* p = ResolveExport(...)`,
// a function-local lazy initialiser read from Update/Draw on the main thread
// AND from Startup chains the loader thread can reach; now a namespace-scope
// atomic slot. The object is the real `Engine::Loader` class (a
// `System::Thread`), still the DLL-data slot, never a C++ static member
// (carry-over 5).
Engine::Loader* LoaderLoopLoader() {
    return static_cast<Engine::Loader*>(rtengine::AtomicExport(g_loaderLoopLoader,
        "?_loader@LoaderLoop@Engine@@1VLoader@2@A"));
}

using CurrentTimeTicksFn  = int64_t (*)();

// The Video::Context backend object hangs off Context::_contextData (+0x2160).
// Retail does NOT route these through `?Clear@Context@Video@@` etc.; it inlines
// the three-instruction thunk and dispatches the backend vtable directly. The
// ports below reproduce that dispatch rather than calling the wrapper, because
// the wrapper is exactly those three instructions and nothing else
// (runtime/types/video/Context.h documents all nine).
// ENDGAME B.3.5: the four hand-inlined slot calls below now go through the real
// abstract `Video::ContextData` (runtime/types/video/CompanionData.h), whose 20
// pure slots are declared in retail order — slot 1 `BeginCommands`, 2
// `EndCommands`, 3 `Finish`, 4 `Clear`. The receiver is PROVED: it is
// `Video::Context::_contextData` (+0x2160), which the header types as
// `ContextData*`. The CONCRETE `*Data` subclass and the dx11 seam stay as they
// are (B35_RULES section 11); only the dispatch converts.
inline Video::ContextData* CtxDataOf(RtVideo::Context* ctx) {
    return reinterpret_cast<Video::ContextData*>(ctx->_contextData);   // B.3.5 bridge
}

// MainLoop::_singleton: NOT redefined here. `MainLoopSingleton()` already exists
// at the top of this file (line ~303) and goes through RT_DATA_SLOT, which caches
// and is the idiom the rest of the tree uses; the bodies below call THAT one. The
// row is tagged RT_UNPROMOTABLE (HARMFUL while the DLL is present) in
// reg_mint_promoted.cpp, so it necessarily reads the DLL's own slot — which is
// what every body below needs, because retail's MainLoop ctor wrote it.

// `?_singleton@Application@1@0PEAV11@EA` -- the DATA slot (carry-over 5),
// through a namespace-scope atomic cache.
Application::Application* ApplicationSingleton() {
    void** slot = static_cast<void**>(rtengine::AtomicExport(g_applicationSingleton,
        "?_singleton@Application@1@0PEAV11@EA"));
    return slot ? static_cast<Application::Application*>(*slot) : nullptr;
}

// `mov 0x1801fc678,%rcx ; mov $0xe,%edx ; call Application::SleepThread` — the
// identical three instructions open BOTH Update bodies below, unconditionally
// and before anything is read off `this`. ENDGAME B.3.7: a direct member call
// (it was a `static` ResolveExport cache of the export).
void SleepFourteen() {
    if (Application::Application* app = ApplicationSingleton()) app->SleepThread(14);
}

// The tail half of LoaderLoop::Update, lifted out because
// DialogLoaderLoopT::Update contains a byte-identical copy of it. Retail emitted
// it twice (the derived Update is not a call to the base); the port shares one
// function, which is a SOURCE-level difference with no observable behavioural
// one — same reads, same comparisons, same single store.
void LoaderLoopAdvance(Engine::LoaderLoop* self, const System::Time& time) {
    // `cmpb $0,0x40(%rbx)` — _autoAdvance, set to 1 by the ctor and never
    // written again (runtime/types/engine/UpdateLoop.h records that correction).
    if (self->_autoAdvance) {
        const Engine::Loader* loader = LoaderLoopLoader();
        // `testb $1,0x1801fe7c0` — _loader._thread.runFlags bit 0, the
        // still-running flag. NOT a "finished" field on the loop itself.
        const bool stillRunning =
            loader && (loader->_runFlags & 1u) != 0;
        if (!stillRunning && self->_elapsedTicks >= self->_delayTicks) {
            if (Engine::MainLoop* ml = MainLoopSingleton())
                ml->_nextLoop = self->_nextLoop;
        }
    }
    self->_elapsedTicks += time._ticks;   // `add %rax,0x30(%rbx)`
}

}  // namespace

// ---- ?Startup@LoaderLoop@Engine@@UEAAXXZ  0x1800dc260  ord 4100 -------------
// THREE INSTRUCTIONS, and it is the whole reason this lane exists:
//     mov  0x28(%rcx),%rdx          ; this->_nextLoop
//     lea  0x1801fe7b8(%rip),%rcx   ; &LoaderLoop::_loader
//     jmp  0x1800dc050              ; tail-call Loader::Load
// The LEA-then-work-through-the-register shape is exactly what the
// `?_loader@LoaderLoop@` UNPROMOTABLE tag names as the reason a RIP-relative
// read/write census reports R=0 for that row.
void Engine::LoaderLoop::Startup() {
    LoaderLoopLoader()->Load(_nextLoop);
}

extern "C" void RtEngine_LoaderLoop_Startup(LoaderLoop* self) {
    reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::Startup();  // B.3.7 bridge
}

// ---- ?Shutdown@LoaderLoop@Engine@@UEAAXXZ  0x1800dc280  ord 4052 ------------
//     mov  0x1801fe7e0(%rip),%rcx   ; _loader._thread.hFinished
//     or   $0xffffffff,%edx         ; INFINITE
//     jmp  *WaitForSingleObject
// `self` is not read at all — the wait is on the one static loader.
void Engine::LoaderLoop::Shutdown() {
    // ENDGAME B.3.7: `Loader` IS a `System::Thread`; `WaitForFinish` is the
    // base's non-virtual member (thread_win.cpp), called directly.
    LoaderLoopLoader()->WaitForFinish();
}

extern "C" void RtEngine_LoaderLoop_Shutdown(LoaderLoop* self) {
    reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::Shutdown();  // B.3.7 bridge
}

// ---- ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ  0x1800dc360  ord 3290 --------
// BYTE-IDENTICAL to Shutdown above (same global, same -1, same import thunk) —
// two separate 17-byte copies in the shipped .text, not a COMDAT fold. Kept as
// two ports rather than one registered twice, because they are two exports and
// two slot-instances and a future divergence in either would be invisible if
// they shared a body. This is slot 8 of BOTH ??_7LoaderLoop@ and
// ??_7DialogLoaderLoopT@; DialogLoopT's slot 8 is the BASE
// `?OnVideoError@UpdateLoop@` (0x180031760, a bare `ret`, ported in Wave 25).
void Engine::LoaderLoop::OnVideoError() {
    LoaderLoopLoader()->WaitForFinish();
}

extern "C" void RtEngine_LoaderLoop_OnVideoError(LoaderLoop* self) {
    reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::OnVideoError();  // B.3.7 bridge
}

// ---- ?Load@LoaderLoop@Engine@@UEAAXXZ  0x1800dc350  ord 3205 ----------------
// `ret $0x0` and nothing else. The derived classes override it; LoaderLoop
// itself loads nothing, because the WORK is done on the loader thread by
// Loader::Run, which Startup kicks off.
void Engine::LoaderLoop::Load() {
}

extern "C" void RtEngine_LoaderLoop_Load(LoaderLoop* self) {
    reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::Load();  // B.3.7 bridge
}

// ---- ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z 0x1800dc2a0 -------
//      ord 4495.  Matches the pseudocode runtime/types/engine/UpdateLoop.h
//      recorded in Wave 8 instruction for instruction.
bool Engine::LoaderLoop::Update(const System::Time& time) {
    SleepFourteen();
    LoaderLoopAdvance(this, time);
    return true;   // `mov $0x1,%al`
}

extern "C" bool RtEngine_LoaderLoop_Update(LoaderLoop* self, const ::Time* time) {
    return reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::Update(  // B.3.7 bridge
        *reinterpret_cast<const System::Time*>(time));
}

// ---- ?Draw@LoaderLoop@Engine@@UEAAXM@Z  0x1800dc310  ord 2250 ---------------
// Clears the whole target to BLACK and draws nothing else — this is the frame
// you see behind a loading screen that has no dialog. THE FLOAT ARGUMENT IS
// NEVER READ (xmm1 is untouched by the whole body); that is retail's, not an
// omission here.
//     Clear(flags=7, colour=0x00000000, depth=1.0f, stencil=0)
// 7 is COLOUR|DEPTH|STENCIL; 1.0f is the literal at 0x18016169c, read out of
// .rdata rather than assumed.
void Engine::LoaderLoop::Draw(float /*interpolation*/) {
    MainLoop* ml = MainLoopSingleton();
    if (!ml) return;
    RtVideo::Context* ctx = reinterpret_cast<RtVideo::Context*>(ml->_context);
    Video::ContextData* cd = CtxDataOf(ctx);
    cd->Clear(7u, 0u, 1.0f, 0);
}

extern "C" void RtEngine_LoaderLoop_Draw(LoaderLoop* self, float dt) {
    reinterpret_cast<Engine::LoaderLoop*>(self)->Engine::LoaderLoop::Draw(dt);  // B.3.7 bridge
}

// ---- ?Shutdown@DialogLoaderLoopT@Engine@@UEAAXXZ  0x1800dafc0  ord 4045 -----
// Closes the loading dialog, THEN waits for the loader thread. The order is
// retail's and matters: CloseDialog only queues the dialog for removal, so
// doing it first lets the close overlap the wait.
//     mov 0x58(%rcx),%rax ; lea 0x30(%rsp),%rdx ; add $0x60,%rcx
//     call CloseDialog    ; then the Shutdown@LoaderLoop tail, inlined
// `_dialogInstance` is passed by ADDRESS: `InternalPtr<DialogInstance>` is a
// class type, so MSVC hands it to CloseDialog as a hidden reference to a stack
// copy. Passing the field's own address instead would let the callee mutate the
// member — hence the local.
void Engine::DialogLoaderLoopT::Shutdown() {
    void* byValue = _dialogInstance;
    _ui.CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&byValue));  // B.3.6 bridge

    // The Shutdown@LoaderLoop tail, INLINED by retail (not a call): the wait on
    // the one static loader.
    LoaderLoopLoader()->WaitForFinish();
}

extern "C" void RtEngine_DialogLoaderLoopT_Shutdown(DialogLoaderLoopT* self) {
    reinterpret_cast<Engine::DialogLoaderLoopT*>(self)                // B.3.7 bridge
        ->Engine::DialogLoaderLoopT::Shutdown();
}

// ---- ?Update@DialogLoaderLoopT@Engine@@UEAA_N…  0x1800daf20  ord 4484 -------
// The UI half of DialogLoopT::Update followed by the whole of
// LoaderLoop::Update, both inlined. The Time round-trip through FLOAT is the
// same lossy narrowing DialogLoopT::Update above documents at length, and is
// reproduced for the same reason: it is observable on any tick count over 2^24.
bool Engine::DialogLoaderLoopT::Update(const System::Time& time) {

    System::Time rounded;
    rounded._ticks = SecondsToTicks(TicksToSeconds(time._ticks));
    _ui.Update(rounded);

    SleepFourteen();

    // The LoaderLoop base at +0x00 — the inlined copy reads 0x40/0x38/0x30/0x28
    // off `this` directly, which are exactly those. NOT a call to
    // LoaderLoop::Update: retail inlined a second copy (no second Sleep).
    LoaderLoopAdvance(this, time);
    return true;
}

extern "C" bool RtEngine_DialogLoaderLoopT_Update(DialogLoaderLoopT* self,
                                                  const ::Time* time) {
    return reinterpret_cast<Engine::DialogLoaderLoopT*>(self)         // B.3.7 bridge
        ->Engine::DialogLoaderLoopT::Update(*reinterpret_cast<const System::Time*>(time));
}

// ---- ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z  0x1800dae20  ord 2238 --------
// Clear to BLACK, publish the elapsed load time into THREE shader constants,
// then draw the UI. Like every Draw in this family the float argument is
// ignored; the time it publishes is `_elapsedTicks`, i.e. how long this loading
// screen has been up, which is what drives the animated loading spinner.
//
// The three destinations and the two dirty bits, read off the stores:
//     ctx->_time          (+0x3c8) = (secs,0,0,0) ; _dirty |= 0x00800
//     ctx->_unknown03b8   (+0x3b8) = (secs,0,0,0)
//     ctx->_pixelConstantStaging[0x88] = (secs,0,0,0) ; _dirty |= 0x10800
// The second OR is 0x10800, not 0x10000 — it re-sets 0x800 that the first OR
// already set. Faithful, and harmless.
//
// NOTE the conversion is NOT the float round-trip the Update bodies do: it is
// `(float)((double)_elapsedTicks * Timer::_secondsPerTick)`, which is exactly
// `?TicksToSeconds@Timer@System@@SAM_J@Z` (0x1800ca820 — verified to be that
// same multiply-and-narrow). Retail computes it TWICE from the same input, once
// per constant group; one computation here, because a double-to-float narrowing
// of the same value is deterministic and the two cannot differ.
void Engine::DialogLoaderLoopT::Draw(float /*interpolation*/) {
    MainLoop* ml = MainLoopSingleton();
    if (!ml) return;
    RtVideo::Context* ctx = reinterpret_cast<RtVideo::Context*>(ml->_context);

    Video::ContextData* cd = CtxDataOf(ctx);
    cd->Clear(7u, 0u, 1.0f, 0);

    const float secs = TicksToSeconds(_elapsedTicks);

    ctx->_time.x = secs;   ctx->_time.y = 0.0f;
    ctx->_time.z = 0.0f;   ctx->_time.w = 0.0f;
    ctx->_dirty |= 0x800u;

    ctx->_unknown03b8.x = secs;  ctx->_unknown03b8.y = 0.0f;
    ctx->_unknown03b8.z = 0.0f;  ctx->_unknown03b8.w = 0.0f;

    float* staging = reinterpret_cast<float*>(&ctx->_pixelConstantStaging[0x88]);
    staging[0] = secs; staging[1] = 0.0f; staging[2] = 0.0f; staging[3] = 0.0f;
    ctx->_dirty |= 0x10800u;

    _ui.Draw(*ctx);   // B.3.6: `_ui` IS a UI::UserInterface; RtVideo::Context aliases Video::Context
}

extern "C" void RtEngine_DialogLoaderLoopT_Draw(DialogLoaderLoopT* self, float dt) {
    reinterpret_cast<Engine::DialogLoaderLoopT*>(self)                // B.3.7 bridge
        ->Engine::DialogLoaderLoopT::Draw(dt);
}

// ---- ?Shutdown@DialogLoopT@Engine@@UEAAXXZ  0x1800db2c0  ord 4046 -----------
// DialogLoaderLoopT::Shutdown without the thread wait, and at DialogLoopT's own
// offsets (_dialogInstance +0x38, _ui +0x40 — thirty-two bytes lower than the
// DialogLoaderLoopT pair, because there is no LoaderLoop subobject).
void Engine::DialogLoopT::Shutdown() {
    void* byValue = _dialogInstance;
    _ui.CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&byValue));  // B.3.6 bridge
}

extern "C" void RtEngine_DialogLoopT_Shutdown(DialogLoopT* self) {
    reinterpret_cast<Engine::DialogLoopT*>(self)->Engine::DialogLoopT::Shutdown();  // B.3.7 bridge
}

// ---- ?Draw@DialogLoopT@Engine@@UEAAXM@Z  0x1800db210  ord 2239 --------------
// THE CLEAR COLOUR IS NOT BLACK HERE. `mov $0xff808080,%r8d` — opaque mid-grey,
// against LoaderLoop::Draw's and DialogLoaderLoopT::Draw's 0. That is the menu
// backdrop, and it is the one thing that distinguishes this body from the other
// two Draws. No shader constants are published either: a menu dialog is not
// animated off a load timer.
void Engine::DialogLoopT::Draw(float /*interpolation*/) {
    MainLoop* ml = MainLoopSingleton();
    if (!ml) return;
    RtVideo::Context* ctx = reinterpret_cast<RtVideo::Context*>(ml->_context);

    Video::ContextData* cd = CtxDataOf(ctx);
    cd->Clear(7u, 0xff808080u, 1.0f, 0);

    _ui.Draw(*ctx);   // B.3.6: `_ui` IS a UI::UserInterface; RtVideo::Context aliases Video::Context
}

extern "C" void RtEngine_DialogLoopT_Draw(DialogLoopT* self, float dt) {
    reinterpret_cast<Engine::DialogLoopT*>(self)->Engine::DialogLoopT::Draw(dt);  // B.3.7 bridge
}

// ---- ?Run@Loader@Engine@@UEAAXXZ  0x1800dbfc0  ord 3600 ---------------------
// THE LOADER THREAD BODY — slot 0 of ??_7Loader@Engine@@6B@, and the last
// unported slot of that table. It brackets the actual load between two calls
// that flip some Engine-owned object into and out of "loading" mode:
//
//     rsi = Engine::_singleton
//     obj = *(*(rsi + 0x6c0) + 0x100)
//     obj->vtable[4](obj, true)                  ; enter loading mode
//     ExternalFactory::_singleton->+0x41 = 1     ; _loadEnabled
//     ExternalFactory::_singleton->+0x00 = _loop->_loadBit
//     UpdateLoop::LoadResources(_loop)
//     _loop->vtable[6]()                         ; UpdateLoop::Load
//     ExternalFactory::_singleton->+0x41 = 0
//     obj->vtable[4](obj, false)                 ; leave loading mode  (tail)
//
// `+0x41` and `+0x00` on ExternalFactory are the two fields
// runtime/types/engine/UpdateLoop.h already names from the UpdateLoop ctor and
// dtor side: the allocation-mask clean index at +0x00 and the load-enabled flag
// at +0x41. Nothing here invents an offset.
//
// NOTE retail RELOADS the `+0x6c0 / +0x100` chain for the closing call rather
// than keeping the pointer live across the load (0x1800dc01a repeats
// 0x1800dbfe0). Reproduced, because the load it brackets can legitimately
// replace that object and the reload is the reason retail survives that.
//
// THE ONE HELD-BACK CALLEE. `?LoadResources@UpdateLoop@Engine@@QEAAXXZ`
// (0x1800dd7d0) is deliberately NOT ported — see the "NOT REGISTERED" block
// below and WAVE8_LANE_A.md §3. This body reaches it through ResolveExport, so
// it gets the DLL's implementation today and will pick up a port the moment one
// is registered, with no edit here.
namespace {
std::atomic<void*> g_engineSingletonSlot{nullptr};
std::atomic<void*> g_factorySingletonSlot{nullptr};
}  // namespace

void Engine::Loader::Run() {
    // LOADER THREAD. ENDGAME B.3.7 (RULE B7-7.1): the two singleton reads were
    // uncached ResolveExport calls and `LoadResources` a function-local `static`
    // export cache; the slots are namespace-scope atomics now and LoadResources
    // is a direct member call. The ORDER of the calls below is unchanged
    // (B7-7.4: the device-lost / loader race is the user's decision, and this
    // conversion neither moves nor adds a call in the bracket).
    void** engineSlot = static_cast<void**>(rtengine::AtomicExport(
        g_engineSingletonSlot, "?_singleton@Engine@1@0PEAV11@EA"));
    void** factorySlot = static_cast<void**>(rtengine::AtomicExport(
        g_factorySingletonSlot, "?_singleton@ExternalFactory@Object@@0PEAV12@EA"));
    if (!engineSlot || !factorySlot) return;

    ::Engine::Engine* engine = static_cast< ::Engine::Engine*>(*engineSlot);
    uint8_t* factory = reinterpret_cast<uint8_t*>(*factorySlot);

    // `mov 0x6c0(%rsi),%rax ; mov 0x100(%rax),%rcx` — two plain loads, no null
    // test in retail. Left unguarded: retail relies on both being live for the
    // whole lifetime of the loader thread, and a guard here would SKIP the
    // bracket rather than restore it (CLAUDE.md, "Don't 'fix' a faithful crash").
    //
    // KEPT A RAW SLOT CALL, deliberately: the receiver is `Video::DeviceData`
    // (B.3.5), whose slot 4 is declared `void Slot4()` with NO argument
    // (runtime/types/video/CompanionData.h), while retail passes the bool in
    // DL (1 before, 0 after). A member call would drop that argument.
    using SetModeFn = void (*)(void*, bool);
    void* owner = engine->_videoDevice;
    void* mode  = *reinterpret_cast<void**>(
                      reinterpret_cast<uint8_t*>(owner) + 0x100);
    reinterpret_cast<SetModeFn>((*reinterpret_cast<void***>(mode))[4])(mode, true);

    factory[0x41] = 1;
    UpdateLoop* loop = _loop;
    *reinterpret_cast<int32_t*>(factory) = loop->_loadBit;

    // `CALL ?LoadResources@UpdateLoop@Engine@@QEAAXXZ` -- a near CALL in
    // retail, so no qualification question: it is non-virtual.
    loop->LoadResources();

    // slot 6 Load, dispatched off the loop's OWN table, so a LoaderLoop gets
    // the empty body and a DialogLoaderLoopT or the exe's GameUpdateLoop its
    // own. An ordinary virtual call.
    loop->Load();

    factory[0x41] = 0;

    void* owner2 = engine->_videoDevice;
    void* mode2  = *reinterpret_cast<void**>(
                       reinterpret_cast<uint8_t*>(owner2) + 0x100);
    reinterpret_cast<SetModeFn>((*reinterpret_cast<void***>(mode2))[4])(mode2, false);
}

extern "C" void RtEngine_Loader_Run(Loader* self) {
    // QUALIFIED: the live object wears the minted `??_7Loader` table whose
    // slot 0 is THIS door (B34_BRIEFS A.1).
    reinterpret_cast<Engine::Loader*>(self)->Engine::Loader::Run();  // B.3.7 bridge
}

// ---- ?Draw@MainLoop@Engine@@QEAAXM@Z  0x1800dcd40  ord 2251 -----------------
// The census BODY row, and the per-frame draw half of the main loop.
//
// CORRECTION TO runtime/types/engine/MainLoop.h: that header lists `Draw` under
// a heading "Internal, NOT exported". IT IS EXPORTED — ordinal 2251, read out
// of the shipped export directory with pefile. This is the AEAA trap in its
// other form (reference_aeaa_is_private_not_unexported.md): the name is public
// (QEAA) and the "not exported" was never checked. Corrected here AND at the
// header's own line, because a reader reaches the header first.
//
// TWO THINGS IN THIS BODY ARE EASY TO GET WRONG AND ARE DELIBERATE:
//
//  1. The timing block at the end reads the GLOBAL `MainLoop::_singleton`, not
//     `this` (`mov 0x1801fe750(%rip),%rbx` at 0x1800dce46, then +0x60/+0x68 off
//     RBX while RDI still holds the context). In every shipped configuration
//     they are the same object, so this looks like a distinction without a
//     difference — but reproducing it costs nothing and asserting they are the
//     same would be an unchecked claim.
//  2. The viewport is only re-published when it DIFFERS from the default render
//     buffer's full extent, and the zeroing of x and y is a single 8-byte store
//     (`mov %rsi,0x1d0(%rcx)`), not two 4-byte ones. Written as two stores here
//     because Viewport::x and ::y are adjacent int32s and no observer can tell.
void Engine::MainLoop::Draw(float dt) {

    RtVideo::Context* ctx = reinterpret_cast<RtVideo::Context*>(_context);
    Video::ContextData* cd = CtxDataOf(ctx);

    if (!cd->BeginCommands()) {
        // The device is gone. Tell the current loop, drop the dirty flag, and
        // hard-reset the context — then RETURN: nothing else in the frame runs.
        // ENDGAME B.3.7: slot 5 as an ordinary virtual call.
        _currentLoop->DrawFailed();

        RtVideo::Context* c = reinterpret_cast<RtVideo::Context*>(_context);
        _contextDirty = 0;
        reinterpret_cast<Video::Context*>(c)->Reset();        // B.3.5 bridge
        return;
    }

    if (_contextDirty) {
        RtVideo::Context* c = reinterpret_cast<RtVideo::Context*>(_context);
        _contextDirty = 0;
        reinterpret_cast<Video::Context*>(c)->ClearResources();  // B.3.5 bridge
    }

    {
        RtVideo::Context* c = reinterpret_cast<RtVideo::Context*>(_context);
        // RenderBuffer width/height at +0x10/+0x14. Read off this body's own
        // loads; no RenderBuffer type is pulled in for two ints.
        const uint8_t* rb =
            reinterpret_cast<const uint8_t*>(c->_defaultRenderBuffer);
        const int32_t w = *reinterpret_cast<const int32_t*>(rb + 0x10);
        const int32_t h = *reinterpret_cast<const int32_t*>(rb + 0x14);
        if (c->_viewport.x != 0 || c->_viewport.y != 0 ||
            c->_viewport.width != w || c->_viewport.height != h) {
            c->_dirty |= 0x200u;
            c->_viewport.x = 0;
            c->_viewport.y = 0;
            c->_viewport.width  = w;
            c->_viewport.height = h;
        }
    }

    // slot 4 Draw as an ordinary virtual call (ENDGAME B.3.7).
    _currentLoop->Draw(dt);

    // `_postDraw` walked by BYTE offset in retail (rbx += 0x10 per entry); the
    // size is re-read from the array on every iteration (`cmp 0x88(%rdi),%esi`),
    // so a callback that appends is seen by this same loop. Reproduced.
    for (int32_t i = 0; i < _postDraw._size; ++i) {
        RtEngine::MainLoopCallback e = _postDraw._data[i];
        using PostDrawFn = void (*)(void*, void*);
        reinterpret_cast<PostDrawFn>(e._func)(_context, e._context);
    }

    {
        RtVideo::Context* c = reinterpret_cast<RtVideo::Context*>(_context);
        Video::ContextData* cd2 = CtxDataOf(c);
        cd2->EndCommands();
    }

    RtVideo::Context* last = reinterpret_cast<RtVideo::Context*>(_context);

    // See note 1 above: the GLOBAL singleton, deliberately. ENDGAME B.3.7:
    // RT_EXPORT_ONCE (B7-7.2's accepted shape) replaces a function-local
    // `static` ResolveExport cache.
    if (MainLoop* g = MainLoopSingleton())
        g->_frameUpdateTime = RT_EXPORT_ONCE(CurrentTimeTicksFn,
            "?CurrentTimeTicks@Timer@System@@SA_JXZ")() - g->_frameStartTicks;

    Video::ContextData* cd3 = CtxDataOf(last);
    ++last->_frameIndex;
    cd3->Finish();
}

extern "C" void RtEngine_MainLoop_Draw(RtEngine::MainLoop* self, float dt) {
    self->Engine::MainLoop::Draw(dt);
}

// ============================================================================
// Registration
// ============================================================================
// Every port is registered TWICE, under its mangled name and under its retail
// address. The name form is what every live call site in our tree uses today
// (engine_funcs.cpp resolves all twelve by ResolveExport); the address form
// costs nothing and closes the Wave 5 lane D failure mode where a later call
// site reaches the other door and one live object ends up split between two
// implementations. Nine of these names ALSO have an
// `if (!fn) fn = ResolveAddr(Module::Runtime, <addr>)` fallback in
// engine_funcs.cpp, so the address registration keeps that path consistent too.
//
// The literal string is repeated at each call rather than using the constants
// above: tools/runtime_census.py cannot see a registration whose name is a
// named constant, and that blind spot has produced a false progress report
// twice (reference_census_cannot_see_nonliteral_registrations.md).
//
// NOT REGISTERED, deliberately:
//
//   ?_loader@LoaderLoop@Engine@@1VLoader@2@A  (0x1801fe7b8, ord 4794)
//     This is DATA — the DLL's one static Engine::Loader instance, 0x38 bytes,
//     constructed during the DLL's own startup. Registering it would mean
//     handing every caller a DIFFERENT object from the one the DLL's loader
//     thread was created on and waits on: the thread would sleep on one pair of
//     events forever while Loader::Load signalled another. There is nothing to
//     "port" about a static object; what a port can own is the CODE that
//     touches it, and Loader::Load above does. This file therefore RESOLVES the
//     symbol (in DialogLoaderLoopT::Startup) and never registers it. That is
//     the finished state, not an omission.
//
//   ?LoadResources@UpdateLoop@Engine@@QEAAXXZ  (0x1800dd7d0, ord 3210)
//     Held back. See docs/runtime_port/WAVE8_LANE_A.md §3.
// ============================================================================
int RtEngineLoopsRegister() {
    int n = 0;

    RegisterRuntimeExport("??0UpdateLoop@Engine@@QEAA@XZ",
                          (void*)&RtEngine_UpdateLoop_Ctor); ++n;
    RegisterRuntimeAddr(0x1800ddd30, (void*)&RtEngine_UpdateLoop_Ctor);

    RegisterRuntimeExport("??1UpdateLoop@Engine@@UEAA@XZ",
                          (void*)&RtEngine_UpdateLoop_Dtor); ++n;
    RegisterRuntimeAddr(0x1800dddb0, (void*)&RtEngine_UpdateLoop_Dtor);

    RegisterRuntimeExport("?IsRunning@UpdateLoop@Engine@@QEBA_NXZ",
                          (void*)&RtEngine_UpdateLoop_IsRunning); ++n;
    RegisterRuntimeAddr(0x1800dd7b0, (void*)&RtEngine_UpdateLoop_IsRunning);

    RegisterRuntimeExport("??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z",
                          (void*)&RtEngine_LoaderLoop_Ctor); ++n;
    RegisterRuntimeAddr(0x1800dc1e0, (void*)&RtEngine_LoaderLoop_Ctor);

    RegisterRuntimeExport("?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z",
                          (void*)&RtEngine_Loader_Load); ++n;
    RegisterRuntimeAddr(0x1800dc050, (void*)&RtEngine_Loader_Load);

    RegisterRuntimeExport("??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z",
                          (void*)&RtEngine_DialogLoopT_Ctor); ++n;
    RegisterRuntimeAddr(0x1800db3f0, (void*)&RtEngine_DialogLoopT_Ctor);

    RegisterRuntimeExport("?Load@DialogLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoopT_Load); ++n;
    RegisterRuntimeAddr(0x1800db140, (void*)&RtEngine_DialogLoopT_Load);

    RegisterRuntimeExport("?Startup@DialogLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoopT_Startup); ++n;
    RegisterRuntimeAddr(0x1800db2f0, (void*)&RtEngine_DialogLoopT_Startup);

    RegisterRuntimeExport("?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z",
                          (void*)&RtEngine_DialogLoopT_Update); ++n;
    RegisterRuntimeAddr(0x1800db270, (void*)&RtEngine_DialogLoopT_Update);

    RegisterRuntimeExport(
        "??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z",
        (void*)&RtEngine_DialogLoaderLoopT_Ctor); ++n;
    RegisterRuntimeAddr(0x1800db0d0, (void*)&RtEngine_DialogLoaderLoopT_Ctor);

    RegisterRuntimeExport("?Load@DialogLoaderLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoaderLoopT_Load); ++n;
    RegisterRuntimeAddr(0x1800dad50, (void*)&RtEngine_DialogLoaderLoopT_Load);

    RegisterRuntimeExport("?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoaderLoopT_Startup); ++n;
    RegisterRuntimeAddr(0x1800db000, (void*)&RtEngine_DialogLoaderLoopT_Startup);

    // ---- BATCH 8: the three remaining family dtors --------------------------
    RegisterRuntimeExport("??1LoaderLoop@Engine@@UEAA@XZ",
                          (void*)&RtEngine_LoaderLoop_Dtor); ++n;
    RegisterRuntimeAddr(0x1800dc170, (void*)&RtEngine_LoaderLoop_Dtor);

    RegisterRuntimeExport("??1DialogLoopT@Engine@@UEAA@XZ",
                          (void*)&RtEngine_DialogLoopT_Dtor); ++n;
    RegisterRuntimeAddr(0x1800db340, (void*)&RtEngine_DialogLoopT_Dtor);

    RegisterRuntimeExport("??1DialogLoaderLoopT@Engine@@UEAA@XZ",
                          (void*)&RtEngine_DialogLoaderLoopT_Dtor); ++n;
    RegisterRuntimeAddr(0x1800db060, (void*)&RtEngine_DialogLoaderLoopT_Dtor);

    // ---- WAVE 25 lane VTIO: the six shared UpdateLoop base slots ------------
    // Registered by NAME and by ADDRESS, like every other row in this file.
    // DECLARED SURFACE GROWTH (WAVE25_KICKOFF §1f), MEASURED by a row-set diff
    // of `tools/runtime_census.py --csv` across a stash of this change, NOT
    // reasoned about.  My first draft of this comment claimed **+0** and was
    // WRONG; the flattering number was the wrong one, exactly as §1f warns.
    //
    //   called 1323 -> 1325 (+2)   ported 1171 -> 1173 (+2)   net gap +0
    //
    // AND NONE OF THAT IS THESE SIX REGISTRATIONS.  A registration alone does
    // not create a census row — the population is built from RESOLVE DOORS —
    // so four of the six (DrawFailed, SaveRestore, OnVideoError,
    // OnVideoRestart) move NO column at all.  The two rows that appeared are
    // `?GetResourceFilename@` and `?AllowsModChanges@`, and they appeared
    // because the ORACLE above opens a real `the raw DLL door` door on each.
    // They enter `called` and `ported` in the same breath, so the gap is
    // unchanged; the cost is honest and it belongs to the instrument, not to
    // the ports.
    //
    // What these six DO move is the PROMOTION BACKLOG, which is where an
    // unpromoted vtable's slot work is supposed to show (Wave 21 lane ENC):
    //   ??_7UpdateLoop@Engine@@6B@         1/7  -> 7/7  COMPLETE
    //   ??_7LoaderLoop@Engine@@6B@         1/12 -> 6/12
    //   ??_7DialogLoopT@Engine@@6B@        4/12 -> 10/12
    //   ??_7DialogLoaderLoopT@Engine@@6B@  3/12 -> 8/12
    RegisterRuntimeExport("?DrawFailed@UpdateLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_UpdateLoop_DrawFailed); ++n;
    RegisterRuntimeAddr(0x180031740, (void*)&RtEngine_UpdateLoop_DrawFailed);

    RegisterRuntimeExport("?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z",
                          (void*)&RtEngine_UpdateLoop_SaveRestore); ++n;
    RegisterRuntimeAddr(0x180031750, (void*)&RtEngine_UpdateLoop_SaveRestore);

    RegisterRuntimeExport("?OnVideoError@UpdateLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_UpdateLoop_OnVideoError); ++n;
    RegisterRuntimeAddr(0x180031760, (void*)&RtEngine_UpdateLoop_OnVideoError);

    RegisterRuntimeExport("?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_UpdateLoop_OnVideoRestart); ++n;
    RegisterRuntimeAddr(0x180031770, (void*)&RtEngine_UpdateLoop_OnVideoRestart);

    RegisterRuntimeExport("?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ",
                          (void*)&RtEngine_UpdateLoop_GetResourceFilename); ++n;
    RegisterRuntimeAddr(0x180031780, (void*)&RtEngine_UpdateLoop_GetResourceFilename);

    RegisterRuntimeExport("?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ",
                          (void*)&RtEngine_UpdateLoop_AllowsModChanges); ++n;
    RegisterRuntimeAddr(0x180031790, (void*)&RtEngine_UpdateLoop_AllowsModChanges);

    // ---- WAVE 27 lane LOOPS: the twelve remaining slot bodies + MainLoop::Draw
    // Same two-door rule as everything above, and the literals are repeated at
    // each site because runtime_census.py cannot see a named constant here.
    RegisterRuntimeExport("?Startup@LoaderLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_LoaderLoop_Startup); ++n;
    RegisterRuntimeAddr(0x1800dc260, (void*)&RtEngine_LoaderLoop_Startup);

    RegisterRuntimeExport("?Shutdown@LoaderLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_LoaderLoop_Shutdown); ++n;
    RegisterRuntimeAddr(0x1800dc280, (void*)&RtEngine_LoaderLoop_Shutdown);

    RegisterRuntimeExport("?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z",
                          (void*)&RtEngine_LoaderLoop_Update); ++n;
    RegisterRuntimeAddr(0x1800dc2a0, (void*)&RtEngine_LoaderLoop_Update);

    RegisterRuntimeExport("?Draw@LoaderLoop@Engine@@UEAAXM@Z",
                          (void*)&RtEngine_LoaderLoop_Draw); ++n;
    RegisterRuntimeAddr(0x1800dc310, (void*)&RtEngine_LoaderLoop_Draw);

    RegisterRuntimeExport("?Load@LoaderLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_LoaderLoop_Load); ++n;
    RegisterRuntimeAddr(0x1800dc350, (void*)&RtEngine_LoaderLoop_Load);

    RegisterRuntimeExport("?OnVideoError@LoaderLoop@Engine@@UEAAXXZ",
                          (void*)&RtEngine_LoaderLoop_OnVideoError); ++n;
    RegisterRuntimeAddr(0x1800dc360, (void*)&RtEngine_LoaderLoop_OnVideoError);

    RegisterRuntimeExport("?Shutdown@DialogLoaderLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoaderLoopT_Shutdown); ++n;
    RegisterRuntimeAddr(0x1800dafc0, (void*)&RtEngine_DialogLoaderLoopT_Shutdown);

    RegisterRuntimeExport("?Update@DialogLoaderLoopT@Engine@@UEAA_NAEBVTime@System@@@Z",
                          (void*)&RtEngine_DialogLoaderLoopT_Update); ++n;
    RegisterRuntimeAddr(0x1800daf20, (void*)&RtEngine_DialogLoaderLoopT_Update);

    RegisterRuntimeExport("?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z",
                          (void*)&RtEngine_DialogLoaderLoopT_Draw); ++n;
    RegisterRuntimeAddr(0x1800dae20, (void*)&RtEngine_DialogLoaderLoopT_Draw);

    RegisterRuntimeExport("?Shutdown@DialogLoopT@Engine@@UEAAXXZ",
                          (void*)&RtEngine_DialogLoopT_Shutdown); ++n;
    RegisterRuntimeAddr(0x1800db2c0, (void*)&RtEngine_DialogLoopT_Shutdown);

    RegisterRuntimeExport("?Draw@DialogLoopT@Engine@@UEAAXM@Z",
                          (void*)&RtEngine_DialogLoopT_Draw); ++n;
    RegisterRuntimeAddr(0x1800db210, (void*)&RtEngine_DialogLoopT_Draw);

    RegisterRuntimeExport("?Run@Loader@Engine@@UEAAXXZ",
                          (void*)&RtEngine_Loader_Run); ++n;
    RegisterRuntimeAddr(0x1800dbfc0, (void*)&RtEngine_Loader_Run);

    // NOT a vtable slot — the census BODY row. Exported (ord 2251) despite
    // MainLoop.h having filed it under "Internal, NOT exported"; see the body.
    RegisterRuntimeExport("?Draw@MainLoop@Engine@@QEAAXM@Z",
                          (void*)&RtEngine_MainLoop_Draw); ++n;
    RegisterRuntimeAddr(0x1800dcd40, (void*)&RtEngine_MainLoop_Draw);

    return n;   // 18 (Wave 8+25) + 13 (Wave 27 lane LOOPS) = 31
}
