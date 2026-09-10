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

#include <cstdint>
#include <cstring>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/engine/UpdateLoop.h"
#include "../../types/engine/MainLoop.h"   // lane B; for MainLoop::_currentLoop
#include "../../types/object/External.h"
#include "../../types/system/Value.h"

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

// System::Thread::Start, already ported by Wave 2 (thread_win.cpp).
namespace System { struct Thread; }
void RtSystem_Thread_Start(System::Thread* self);

using RtEngine::UpdateLoop;
using RtEngine::LoaderLoop;
using RtEngine::DialogLoopT;
using RtEngine::DialogLoaderLoopT;
using RtEngine::Loader;

namespace {

// ============================================================================
// The mangled names, spelled once as STRING LITERALS.
// ============================================================================
// tools/runtime_census.py greps for the literal inside RegisterRuntimeExport,
// so a named constant used at the registration site is invisible to it and has
// twice produced a false progress report
// (reference_census_cannot_see_nonliteral_registrations.md). These constants
// therefore exist ONLY for RT_ORIGINAL_ONCE lookups inside diff guards; every
// RegisterRuntimeExport call at the bottom of this file repeats the literal.
const char* const kUpdateLoopCtor    = "??0UpdateLoop@Engine@@QEAA@XZ";
const char* const kIsRunning         = "?IsRunning@UpdateLoop@Engine@@QEBA_NXZ";
const char* const kLoaderLoopCtor    = "??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z";
const char* const kDialogLoopTCtor   = "??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z";
const char* const kDialogLoaderCtor  = "??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z";

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
//  * RtOriginal / RT_ORIGINAL_ONCE — "the DLL, always". Only inside a diff
//    guard, or for a shipped body the oracle must compare against.

using MemFreeFn        = void (*)(void*);
using AdjustDelimFn    = void (*)(void*);                       // Filename&
using FindOrLoadFn     = void* (*)(void*, const void*);         // (factory, const Filename&)
using IsDerivedFromFn  = bool (*)(const void*, uint32_t);
using ExtFactoryCleanFn = void (*)(void*);
using ListCleanupFn    = void (*)(void*);
using RefreshListFn    = void (*)(void*);
using SetupFileSystemFn = void (*)(void*);
using ModArrayCopyFn   = void (*)(void*, const void*);
using UiCtorFn         = void* (*)(void*);
using UiUpdateFn       = int (*)(void*, const void*);
using ArraySetSizeFillFn = void (*)(void*, int, const void*);
using SecondsToTicksFn = int64_t (*)(float);
using TicksToSecondsFn = float (*)(int64_t);

MemFreeFn MemFree() {
    static MemFreeFn fn = reinterpret_cast<MemFreeFn>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    return fn;
}
AdjustDelimFn AdjustDelimiters() {
    // PRIVATE (AEAA, not QEAA) — the QEAA spelling is not an export at all
    // (runtime/types/system/Filename.h records that trap).
    static AdjustDelimFn fn = reinterpret_cast<AdjustDelimFn>(
        ResolveExport("?AdjustDelimiters@Filename@System@@AEAAXXZ"));
    return fn;
}
FindOrLoadFn FindOrLoad() {
    static FindOrLoadFn fn = reinterpret_cast<FindOrLoadFn>(ResolveExport(
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z"));
    return fn;
}
IsDerivedFromFn IsDerivedFrom() {
    static IsDerivedFromFn fn = reinterpret_cast<IsDerivedFromFn>(
        ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn;
}
ExtFactoryCleanFn ExternalFactoryClean() {
    static ExtFactoryCleanFn fn = reinterpret_cast<ExtFactoryCleanFn>(
        ResolveExport("?Clean@ExternalFactory@Object@@QEAAXXZ"));
    return fn;
}
RefreshListFn ModificationRefreshList() {
    static RefreshListFn fn = reinterpret_cast<RefreshListFn>(
        ResolveExport("?RefreshList@Modification@IO@@QEAAXXZ"));
    return fn;
}
SetupFileSystemFn ModificationSetupFileSystem() {
    static SetupFileSystemFn fn = reinterpret_cast<SetupFileSystemFn>(
        ResolveExport("?SetupFileSystem@Modification@IO@@QEAAXXZ"));
    return fn;
}
UiCtorFn UserInterfaceCtor() {
    static UiCtorFn fn = reinterpret_cast<UiCtorFn>(
        ResolveExport("??0UserInterface@UI@@QEAA@XZ"));
    return fn;
}
UiUpdateFn UserInterfaceUpdate() {
    static UiUpdateFn fn = reinterpret_cast<UiUpdateFn>(
        ResolveExport("?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z"));
    return fn;
}

// ---- the two the DLL does not export ---------------------------------------
// 0x180010ab0 — the tail call of ~UpdateLoop, on (this + 0x10). Already known
// to our tree as fn_ListCleanup (engine/funcs/engine_funcs.h:277), resolved
// there from this same address.
ListCleanupFn ListCleanup() {
    static ListCleanupFn fn = reinterpret_cast<ListCleanupFn>(
        ResolveAddr(Module::Runtime, 0x180010ab0));
    return fn;
}
// 0x180010110 — Array<IO::Mod>::operator=, the deep copy Loader::Load runs when
// mod changes are switched OFF. Our tree already resolves this exact address as
// fn_ModArrayCopy (engine_funcs.cpp:1823).
ModArrayCopyFn ModArrayCopy() {
    static ModArrayCopyFn fn = reinterpret_cast<ModArrayCopyFn>(
        ResolveAddr(Module::Runtime, 0x180010110));
    return fn;
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
ArraySetSizeFillFn ArraySetSizeFill() {
    static ArraySetSizeFillFn fn = reinterpret_cast<ArraySetSizeFillFn>(
        ResolveAddr(Module::Runtime, 0x18007a0d0));
    return fn;
}

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
using MainLoopSlot        = RtEngine::MainLoop**;
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
RtEngine::MainLoop* MainLoopSingleton() {
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

void* VTableUpdateLoop() {
    static void* v = ResolveExport("??_7UpdateLoop@Engine@@6B@");        // 0x180155ba0
    return v;
}
void* VTableLoaderLoop() {
    static void* v = ResolveExport("??_7LoaderLoop@Engine@@6B@");        // 0x180157940
    return v;
}
void* VTableDialogLoopT() {
    static void* v = ResolveExport("??_7DialogLoopT@Engine@@6B@");       // 0x1801579a0
    return v;
}
void* VTableDialogLoaderLoopT() {
    static void* v = ResolveExport("??_7DialogLoaderLoopT@Engine@@6B@"); // 0x1801578d0
    return v;
}

// ============================================================================
// The two inlined idioms this family repeats
// ============================================================================

// System::String's release sequence, inlined at ~200 sites in the DLL and
// reproduced here EXACTLY, asymmetry included: on the freeing branch the object
// is left holding `data - 8`, a dangling pointer, not null
// (runtime/types/system/String.h — "reproduce it; do not tidy it").
void StringRelease(RtSystem::String* s) {
    char* data = s->data;
    if (!data) return;
    if (--RtSystem::StringRefCount(data) == 0) {
        s->data = data - 8;
        MemFree()(s->data);
    } else {
        s->data = nullptr;
    }
}

// The copy-with-acquire the three ctors and both Load bodies open with:
//     dst = 0; if (src.data) { dst = src.data; ++src.data[-4]; }
void StringAcquire(RtSystem::String* dst, const RtSystem::String* src) {
    dst->data = nullptr;
    char* data = src->data;
    if (data) {
        dst->data = data;
        ++RtSystem::StringRefCount(data);
    }
}

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
// RT_ORIGINAL_ONCE rather than RtOriginal: DialogLoopT::Update runs once per
// frame for the whole of the front end, and a bare RtOriginal is a
// GetProcAddress under the loader lock on every call — 5.6 points of sim time
// when Wave 2 did it in System::Time (reference_rtoriginal_is_getprocaddress.md).
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
    self->_resourceList  = nullptr;
    self->_internalListA = nullptr;
    self->_internalListB = nullptr;
    self->_loadProgress  = 0;

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
    self->_loadBit = bit;
}

// ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z  0x1800dc1e0
void LoaderLoopCtorBody(LoaderLoop* self, void* nextLoop, const ::Time* delay) {
    UpdateLoopCtorBody(&self->_base);
    self->_base._vtable = VTableLoaderLoop();
    self->_nextLoop     = nextLoop;
    // XORPS xmm0 / DIVSD / CVTTSD2SI — i.e. System::Time(0.0f). Written out as
    // the conversion rather than folded to the literal 0 because that is what
    // the instructions say; it IS 0 for every non-zero divisor.
    self->_elapsedTicks = SecondsToTicks(0.0f);
    self->_delayTicks   = delay->ticks;
    self->_autoAdvance  = 1;
    // +0x41..+0x47 are NOT written. Left alone deliberately: they are the
    // padding the build oracle below must not compare
    // (reference_oracle_must_not_compare_unwritten_bytes.md).
}

// ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z  0x1800db3f0
void DialogLoopTCtorBody(DialogLoopT* self, const RtSystem::String* filename) {
    UpdateLoopCtorBody(&self->_base);
    self->_base._vtable   = VTableDialogLoopT();
    StringAcquire(&self->_dialogFilename, filename);
    self->_dialogExternal = nullptr;
    self->_dialogInstance = nullptr;
    UserInterfaceCtor()(&self->_ui);
}

// ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
// 0x1800db0d0
void DialogLoaderLoopTCtorBody(DialogLoaderLoopT* self, const RtSystem::String* filename,
                               void* nextLoop, const ::Time* delay) {
    LoaderLoopCtorBody(&self->_base, nextLoop, delay);
    self->_base._base._vtable = VTableDialogLoaderLoopT();
    StringAcquire(&self->_dialogFilename, filename);
    self->_dialogExternal = nullptr;
    self->_dialogInstance = nullptr;
    UserInterfaceCtor()(&self->_ui);
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

// ---- ??0UpdateLoop@Engine@@QEAA@XZ  0x1800ddd30 ord 468 ---------------------
extern "C" UpdateLoop* RtEngine_UpdateLoop_Ctor(UpdateLoop* self) {
    RT_DIFF_BEGIN("Engine::UpdateLoop::ctor");
    UpdateLoopCtorBody(self);

    if (rt_diff_on__) {
        ScratchGuard g;
        if (g.held) {
            using CtorFn = UpdateLoop* (*)(void*);
            constexpr size_t n = sizeof(UpdateLoop);
            std::memset(g_scratchOurs, kPoison, n);
            std::memset(g_scratchTheirs, kPoison, n);

            UpdateLoopCtorBody(reinterpret_cast<UpdateLoop*>(g_scratchOurs));
            ReleaseBitFor(reinterpret_cast<UpdateLoop*>(g_scratchOurs));

            RT_ORIGINAL_ONCE(CtorFn, kUpdateLoopCtor)(g_scratchTheirs);
            ReleaseBitFor(reinterpret_cast<UpdateLoop*>(g_scratchTheirs));

            RT_DIFF_BYTES(g_scratchOurs, g_scratchTheirs, n);
        }
    }
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
    RT_DIFF_BEGIN("Engine::UpdateLoop::dtor");

    // The standard MSVC base-destructor step: re-stamp the base vtable, so a
    // virtual dispatched during the rest of the teardown reaches the BASE
    // implementation and not the already-destroyed derived one.
    self->_vtable = VTableUpdateLoop();

    const int32_t bit = self->_loadBit;
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
        factory->_cleanBitMask &= ~(1u << self->_loadBit);
    }

    if (rt_diff_on__) {
        const bool vtableRestamped = (self->_vtable == VTableUpdateLoop());
        RtObject::ExternalFactory* factory = Factory();
        const uint32_t mask = factory ? factory->_cleanBitMask : 0u;
        const bool bitReleased = (bit < 0) || ((mask & (1u << bit)) == 0);
        const bool cleanBitPublished = (bit < 0) || (factory && factory->_cleanBitIndex == bit);
        char ours[128], theirs[128];
        std::snprintf(ours, sizeof(ours),
                      "vtable=%d bitReleased=%d cleanBitPublished=%d (bit=%d)",
                      (int)vtableRestamped, (int)bitReleased, (int)cleanBitPublished, (int)bit);
        std::snprintf(theirs, sizeof(theirs), "vtable=1 bitReleased=1 cleanBitPublished=1");
        RT_DIFF_SHADOW(vtableRestamped && bitReleased && cleanBitPublished, ours, theirs);
    }

    // Tail call in retail: `LEA RCX,[RBX+0x10]; JMP 0x180010ab0`.
    ListCleanup()(&self->_internalListA);
}

// ---- ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ  0x1800dd7b0 ord 3160 -----------
// The one genuinely const, side-effect-free function in the lane, so it gets a
// full replay oracle. Note it is a member that tests its own `this` for null:
// retail really does `TEST RCX,RCX / JZ`, so the guard is FAITHFUL, not one we
// added (CLAUDE.md's rule cuts the other way here — removing it would diverge).
extern "C" bool RtEngine_UpdateLoop_IsRunning(const UpdateLoop* self) {
    RT_DIFF_BEGIN("Engine::UpdateLoop::IsRunning");
    bool ours = false;
    if (self) {
        // _currentLoop (+0x20) is the loop MainLoop::Run is driving right now.
        // Retail dereferences the singleton unguarded; so do we — CLAUDE.md's
        // rule holds, and by the time any UpdateLoop exists MainLoop does too.
        ours = (MainLoopSingleton()->_currentLoop == self);
    }
    using Fn = bool (*)(const UpdateLoop*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kIsRunning)(self));
    return ours;
}

// ---- ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z --------
//      0x1800dc1e0 ord 261
extern "C" LoaderLoop* RtEngine_LoaderLoop_Ctor(LoaderLoop* self, void* nextLoop,
                                                const ::Time* delay) {
    RT_DIFF_BEGIN("Engine::LoaderLoop::ctor");
    LoaderLoopCtorBody(self, nextLoop, delay);

    if (rt_diff_on__) {
        ScratchGuard g;
        if (g.held) {
            using CtorFn = void* (*)(void*, void*, const void*);
            constexpr size_t n = sizeof(LoaderLoop);
            std::memset(g_scratchOurs, kPoison, n);
            std::memset(g_scratchTheirs, kPoison, n);

            LoaderLoopCtorBody(reinterpret_cast<LoaderLoop*>(g_scratchOurs), nextLoop, delay);
            ReleaseBitFor(&reinterpret_cast<LoaderLoop*>(g_scratchOurs)->_base);

            RT_ORIGINAL_ONCE(CtorFn, kLoaderLoopCtor)(g_scratchTheirs, nextLoop, delay);
            ReleaseBitFor(&reinterpret_cast<LoaderLoop*>(g_scratchTheirs)->_base);

            RT_DIFF_BYTES(g_scratchOurs, g_scratchTheirs, n);
        }
    }
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
extern "C" void RtEngine_Loader_Load(Loader* self, void* loop) {
    RT_DIFF_BEGIN("Engine::Loader::Load");

    self->_loop = loop;

    // slot 11 (+0x58) = UpdateLoop::AllowsModChanges. Base returns false
    // (0x180031790); a menu loop overrides it to true.
    using AllowsFn = bool (*)(void*);
    void** vtable = *reinterpret_cast<void***>(loop);
    const bool allowsModChanges =
        reinterpret_cast<AllowsFn>(vtable[RtEngine::kSlot_AllowsModChanges])(loop);

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
    RtSystem_Thread_Start(reinterpret_cast<System::Thread*>(&self->_thread));

    RT_DIFF_SHADOW(true,
                   "no oracle: mounts the mod filesystem and wakes the loader thread",
                   "not called");
}

// ---- ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z  0x1800db3f0 ord 153 -
extern "C" DialogLoopT* RtEngine_DialogLoopT_Ctor(DialogLoopT* self,
                                                  const RtSystem::String* filename) {
    RT_DIFF_BEGIN("Engine::DialogLoopT::ctor");
    DialogLoopTCtorBody(self, filename);

    if (rt_diff_on__) {
        ScratchGuard g;
        if (g.held) {
            using CtorFn = void* (*)(void*, const void*);
            constexpr size_t n = sizeof(DialogLoopT);
            std::memset(g_scratchOurs, kPoison, n);
            std::memset(g_scratchTheirs, kPoison, n);

            DialogLoopT* ours   = reinterpret_cast<DialogLoopT*>(g_scratchOurs);
            DialogLoopT* theirs = reinterpret_cast<DialogLoopT*>(g_scratchTheirs);

            DialogLoopTCtorBody(ours, filename);
            ReleaseBitFor(&ours->_base);

            RT_ORIGINAL_ONCE(CtorFn, kDialogLoopTCtor)(theirs, filename);
            ReleaseBitFor(&theirs->_base);

            RT_DIFF_BYTES(g_scratchOurs, g_scratchTheirs, n);

            // Each scratch run took one reference on the caller's filename.
            // Give both back before the buffers are reused. This is the whole
            // of the undo: neither run allocated anything else — the embedded
            // UserInterface ctor (0x180120de0) is a pure field-zeroing body
            // with no allocation and no global registration.
            StringRelease(&ours->_dialogFilename);
            StringRelease(&theirs->_dialogFilename);
        }
    }
    return self;
}

// ---- ?Load@DialogLoopT@Engine@@UEAAXXZ  0x1800db140 ord 3201 ----------------
// SHADOW, not replay: FindOrLoad either returns a cached External or LOADS the
// dialog resource off disk and inserts it into the global resource table.
// Running it twice is a second cache lookup with a real side effect on the
// clean-bit usage masks, and on a cold cache a second file read.
extern "C" void RtEngine_DialogLoopT_Load(DialogLoopT* self) {
    RT_DIFF_BEGIN("Engine::DialogLoopT::Load");

    // The local is a System::Filename, layout-identical to String; retail
    // copies the String field into it and normalises the delimiters in place.
    RtSystem::String tmp;
    StringAcquire(&tmp, &self->_dialogFilename);
    AdjustDelimiters()(&tmp);

    void* external = FindOrLoad()(Factory(), &tmp);
    void* result = nullptr;
    if (external) {
        // `? external : nullptr` is a CMOVNZ on the type test, not a branch
        // that skips the store: the field is written either way.
        if (IsDerivedFrom()(external, DialogTypeId())) result = external;
    }
    self->_dialogExternal = result;

    StringRelease(&tmp);

    RT_DIFF_SHADOW(true,
                   "no oracle: ExternalFactory::FindOrLoad has a load side effect",
                   "not called");
}

// ---- ?Startup@DialogLoopT@Engine@@UEAAXXZ  0x1800db2f0 ord 4093 -------------
// SHADOW: slot 12 CREATES a dialog instance (a live Object::Internal with a
// handle-table entry) and the body then queues it for display. A replay builds
// a second dialog and shows two.
extern "C" void RtEngine_DialogLoopT_Startup(DialogLoopT* self) {
    RT_DIFF_BEGIN("Engine::DialogLoopT::Startup");

    // Slot 12, CreateDialog, returns InternalPtr<UI::DialogInstance> BY VALUE —
    // an 8-byte class type, so MSVC still gives it a HIDDEN RETURN SLOT in RDX
    // and returns that pointer in RAX (an `int`-typed or missing return slot in
    // an fn_* typedef is the wild-write family this project has paid for twice:
    // reference_hidden_retslot_wild_write.md).
    using CreateDialogFn = void* (*)(void*, void*);
    void*  ret = nullptr;
    void** vtable = *reinterpret_cast<void***>(self);
    void*  retSlot =
        reinterpret_cast<CreateDialogFn>(vtable[RtEngine::kSlot_CreateDialog])(self, &ret);

    void* dialog = *reinterpret_cast<void**>(retSlot);
    self->_dialogInstance = dialog;
    ret = dialog;   // retail re-stores it into the same slot before passing it on

    // dialog->vtable[4] — the DialogInstance virtual retail calls before the
    // instance is queued.
    using Slot4Fn = void (*)(void*);
    void** dialogVtable = *reinterpret_cast<void***>(dialog);
    reinterpret_cast<Slot4Fn>(dialogVtable[4])(dialog);

    // _ui._addDialogs.SetSize(size + 1, ret) — the append. Update drains that
    // array back-to-front into the live dialog list next frame
    // (runtime/types/ui/UserInterface.h).
    ArraySetSizeFill()(&self->_ui._addDialogs, self->_ui._addDialogs._size + 1, &ret);

    RT_DIFF_SHADOW(self->_dialogInstance == dialog,
                   "no oracle: slot 12 constructs a live DialogInstance",
                   "not called");
}

// ---- ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z  0x1800db270 -----
//      ord 4485
// Partly oracled, and the split is the point. The body has two halves: a PURE
// Time round-trip through float, and a call into UI::UserInterface::Update
// which advances the whole front end. The round trip is replayed against the
// DLL's own two Timer conversions on every call — that is a full oracle for the
// only arithmetic in the function. The UI call is not replayed, for the obvious
// reason.
extern "C" bool RtEngine_DialogLoopT_Update(DialogLoopT* self, const ::Time* time) {
    RT_DIFF_BEGIN("Engine::DialogLoopT::Update[time]");

    // CVTSI2SD / MULSD / CVTPD2PS / CVTPS2PD / DIVSD / CVTTSD2SI — i.e. the
    // tick count narrowed to a float number of seconds and converted straight
    // back. The narrowing is LOSSY and is the whole reason this line exists;
    // folding it away would diverge on any tick count over 2^24.
    ::Time rounded;
    rounded.ticks = SecondsToTicks(TicksToSeconds(time->ticks));

    if (rt_diff_on__) {
        // What this checks is that OUR composition is the one retail
        // inlined: same two conversions, same order, float in between. A port
        // that dropped the float narrowing, or applied the conversions the
        // other way round, fails here on the first tick count that does not
        // round-trip.
        //
        // Until Wave 9 lane B3 ported System::Timer, both sides of this
        // comparison called the DLL and only the COMPOSITION was under test.
        // Now the left side (`rounded`, via the RT_EXPORT_ONCE helpers above)
        // runs our Timer and the right side is still the shipped body, so this
        // site tests the composition AND the two conversions at once.
        const int64_t theirs = RT_ORIGINAL_ONCE(SecondsToTicksFn,
                                    "?SecondsToTicks@Timer@System@@SA_JM@Z")(
                               RT_ORIGINAL_ONCE(TicksToSecondsFn,
                                    "?TicksToSeconds@Timer@System@@SAM_J@Z")(time->ticks));
        RT_DIFF_SCALAR(rounded.ticks, theirs);
    }

    UserInterfaceUpdate()(&self->_ui, &rounded);
    return true;   // MOV AL,1 — unconditional
}

// ---- ??0DialogLoaderLoopT@Engine@@…  0x1800db0d0 ord 151 --------------------
extern "C" DialogLoaderLoopT* RtEngine_DialogLoaderLoopT_Ctor(
        DialogLoaderLoopT* self, const RtSystem::String* filename,
        void* nextLoop, const ::Time* delay) {
    RT_DIFF_BEGIN("Engine::DialogLoaderLoopT::ctor");
    DialogLoaderLoopTCtorBody(self, filename, nextLoop, delay);

    if (rt_diff_on__) {
        ScratchGuard g;
        if (g.held) {
            using CtorFn = void* (*)(void*, const void*, void*, const void*);
            constexpr size_t n = sizeof(DialogLoaderLoopT);
            std::memset(g_scratchOurs, kPoison, n);
            std::memset(g_scratchTheirs, kPoison, n);

            DialogLoaderLoopT* ours   = reinterpret_cast<DialogLoaderLoopT*>(g_scratchOurs);
            DialogLoaderLoopT* theirs = reinterpret_cast<DialogLoaderLoopT*>(g_scratchTheirs);

            DialogLoaderLoopTCtorBody(ours, filename, nextLoop, delay);
            ReleaseBitFor(&ours->_base._base);

            RT_ORIGINAL_ONCE(CtorFn, kDialogLoaderCtor)(theirs, filename, nextLoop, delay);
            ReleaseBitFor(&theirs->_base._base);

            RT_DIFF_BYTES(g_scratchOurs, g_scratchTheirs, n);

            StringRelease(&ours->_dialogFilename);
            StringRelease(&theirs->_dialogFilename);
        }
    }
    return self;
}

// ---- ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ  0x1800dad50 ord 3200 ----------
// Byte-for-byte the same body as DialogLoopT::Load with the LoaderLoop base's
// +0x20 offsets. Written out rather than shared, because retail emitted two
// bodies and a shared helper would hide a divergence between them.
extern "C" void RtEngine_DialogLoaderLoopT_Load(DialogLoaderLoopT* self) {
    RT_DIFF_BEGIN("Engine::DialogLoaderLoopT::Load");

    RtSystem::String tmp;
    StringAcquire(&tmp, &self->_dialogFilename);
    AdjustDelimiters()(&tmp);

    void* external = FindOrLoad()(Factory(), &tmp);
    void* result = nullptr;
    if (external) {
        if (IsDerivedFrom()(external, DialogTypeId())) result = external;
    }
    self->_dialogExternal = result;

    StringRelease(&tmp);

    RT_DIFF_SHADOW(true,
                   "no oracle: ExternalFactory::FindOrLoad has a load side effect",
                   "not called");
}

// ---- ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ  0x1800db000 ord 4092 -------
// The DialogLoopT::Startup body plus a TAIL CALL into Loader::Load, which is
// what actually starts the background load behind a loading screen:
//     MOV RDX,[RBX+0x28]           ; this->_base._nextLoop
//     LEA RCX,[0x1801fe7b8]        ; &LoaderLoop::_loader
//     JMP 0x1800dc050              ; Engine::Loader::Load
// Note RCX is a LEA, not a load: `_loader` IS the object, not a pointer to one.
extern "C" void RtEngine_DialogLoaderLoopT_Startup(DialogLoaderLoopT* self) {
    RT_DIFF_BEGIN("Engine::DialogLoaderLoopT::Startup");

    using CreateDialogFn = void* (*)(void*, void*);
    void*  ret = nullptr;
    void** vtable = *reinterpret_cast<void***>(self);
    void*  retSlot =
        reinterpret_cast<CreateDialogFn>(vtable[RtEngine::kSlot_CreateDialog])(self, &ret);

    void* dialog = *reinterpret_cast<void**>(retSlot);
    self->_dialogInstance = dialog;
    ret = dialog;

    using Slot4Fn = void (*)(void*);
    void** dialogVtable = *reinterpret_cast<void***>(dialog);
    reinterpret_cast<Slot4Fn>(dialogVtable[4])(dialog);

    ArraySetSizeFill()(&self->_ui._addDialogs, self->_ui._addDialogs._size + 1, &ret);

    RT_DIFF_SHADOW(self->_dialogInstance == dialog,
                   "no oracle: slot 12 constructs a live DialogInstance, "
                   "then the loader thread is started",
                   "not called");

    // The DLL's own static Loader instance, by NAME. Resolved, never minted:
    // the DLL's loader thread was created on THAT object and waits on ITS
    // events, so a Loader of ours would be a second, unwatched object.
    static Loader* loader = reinterpret_cast<Loader*>(
        ResolveExport("?_loader@LoaderLoop@Engine@@1VLoader@2@A"));
    // Reached through the registered export so a single implementation serves
    // the object, exactly as retail's near-CALL keeps both halves in the DLL.
    using LoaderLoadFn = void (*)(void*, void*);
    static LoaderLoadFn load = reinterpret_cast<LoaderLoadFn>(
        ResolveExport("?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z"));
    load(loader, self->_base._nextLoop);
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

    return n;   // 12
}
