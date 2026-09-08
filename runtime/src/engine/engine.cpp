// ============================================================================
// runtime/src/engine/engine.cpp — Wave 8 lane B — Engine::Engine + Engine::MainLoop
// ============================================================================
//
// Lane B of PLAN.md §4.2 wave 8. Eleven symbols were in scope; THREE are
// ported and EIGHT are held back with written reasons. The layouts and the
// evidence for every offset are in runtime/types/engine/Engine.h and
// runtime/types/engine/MainLoop.h; the full write-up, the oracle argument per
// function and the reasoning behind each holdback are in
// docs/runtime_port/WAVE8_LANE_B.md.
//
//   | Export                              | Retail      | Ord  | Ported | Oracle        |
//   |-------------------------------------|-------------|------|--------|---------------|
//   | ??0Engine@0@QEAA@PEAX@Z             | 0x1800dbf00 |  174 | NO     | — allocator   |
//   | ??1Engine@0@QEAA@XZ                 | 0x1800dbec0 |  586 | NO     | — allocator   |
//   | ?Startup@Engine@1@QEAAIPEBDII_N@Z   | 0x1800db8a0 | 4094 | NO     | — allocator   |
//   | ?Shutdown@Engine@1@QEAAXXZ          | 0x1800db4f0 | 4047 | NO     | — allocator   |
//   | ??0MainLoop@Engine@@QEAA@PEAV11@@Z  | 0x1800dd6b0 |  263 | yes    | shadow        |
//   | ??1MainLoop@Engine@@QEAA@XZ         | 0x1800dd640 |  627 | yes    | shadow        |
//   | ?SetNextLoop@MainLoop@Engine@@...   | 0x1800dd7a0 | 3929 | yes    | replay        |
//   | ?Run@MainLoop@Engine@@...           | 0x1800dd3c0 | 3601 | NO     | — no oracle   |
//   | ?OnVideoError@MainLoop@Engine@@...  | 0x1800dc5e0 | 3291 | NO     | — needs detour|
//   | ?OnVideoRestart@MainLoop@Engine@@...| 0x1800dc3b0 | 3295 | NO     | — needs detour|
//   | ?_singleton@MainLoop@Engine@@1PE... | 0x1801fe750 | 4813 | n/a    | DATA symbol   |
//
// All eleven mangled names were checked against the export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was
// written; the addresses and ordinals above are that query's output, not
// Ghidra's guess. All eleven are present — this lane contributes none of the
// three phantom names WAVE8.md §4 lists (those are lanes C and E).
//
// ---------------------------------------------------------------------------
// NEITHER CLASS IS POLYMORPHIC — SO THERE IS NO VTABLE-OWNERSHIP PROBLEM
// ---------------------------------------------------------------------------
// `??_7Engine@Engine@@6B@` and `??_7MainLoop@Engine@@6B@` are both absent from
// the shipped export table, and WAVE8.md §2 is right that absence is not
// proof. It is shown here from the ctors' first stores instead:
//
//   MainLoop 0x1800dd6c6:  XOR EAX,EAX
//            0x1800dd6c8:  MOV qword ptr [RCX],RAX        <- a ZERO, not a vtable
//   Engine   0x1800dbf1e:  CALL 0x1800c79a0               <- MemoryAllocator ctor
//            0x1800dbf24:  XOR EAX,EAX
//            0x1800dbf26:  MOV qword ptr [RDI+0x670],RAX  <- a ZERO, not a vtable
//
// and confirmed against the profile build (MainLoop ctor 0x1800d5420 opens
// with the identical zero store). No method of either class is virtual.
// PLAN §4.3 risk 3 therefore does not apply to lane B at all.
//
// ---------------------------------------------------------------------------
// NO DETOUR IS INSTALLED — AND FOR ONCE THAT IS NOT LUCK
// ---------------------------------------------------------------------------
// Waves 6 and 7 each expected to be the first user of standalone/runtime_detour.h
// and each was not. Lane B checked rather than assumed: `get_xrefs_to` over
// the retail DLL for all four ported/considered entry points —
// ??0MainLoop (0x1800dd6b0), ??1MainLoop (0x1800dd640), ?Run (0x1800dd3c0),
// ?SetNextLoop (0x1800dd7a0) — and for ??0Engine / ?Startup returns ONLY
// DATA references: the export address table (0x18020eb..-0x18020ec..), the
// .pdata/unwind tables (0x1801b....) and the SEH scope tables (0x18019....).
// **Zero DLL-internal CALL sites.** These are exe-entry-point functions: the
// game's own game_main.cpp is the only caller of every one of them, and
// registering the export flips that call site. A detour would add nothing.
//
// The two functions that DO have a DLL-internal caller are the two video
// callbacks, and that is the first genuine detour requirement this project has
// found in 4,632 functions:
//
//   ?OnVideoError@   <- 0x1800dcd27 in MainLoop::OnVideoError_static
//   ?OnVideoRestart@ <- 0x1800dc5c7 in MainLoop::OnVideoRestart_static
//
// Those two static thunks are what the MainLoop CTOR installs into
// `Video::Device::_singleton` at +0x50/+0x58 (see RtEngine_MainLoop_Ctor
// below), and the device's own lost/restore path is the ONLY thing that ever
// calls them. So a port of either member that registered the export and
// stopped there would be dead code on the only path that reaches it, while
// the diff oracle reported `calls=0 divergences=0` — exactly the failure
// reference_uncalled_port_is_not_verified.md describes. Both are held back;
// whoever ports them must install the detour, and lane B's ported ctor keeps
// installing the DLL's OWN thunk addresses so that path stays entirely inside
// the DLL until then.
//
// ---------------------------------------------------------------------------
// WHY THE FOUR `Engine::Engine` MEMBERS ARE HELD BACK
// ---------------------------------------------------------------------------
// `Engine::Engine` embeds `System::MemoryAllocator` at offset 0 — proven in
// runtime/types/engine/Engine.h, not taken on faith from PLAN §4.3 risk 2 —
// and the allocator port carries a NO-GO verdict (WAVE2_ALLOCATOR.md: its
// shadow proof has never executed, and an unmeasured port is not a port).
//
// WAVE8.md §3 anticipates that the ctor and dtor fall to this. What the
// disassembly adds is that `Startup` and `Shutdown` fall to it too, which was
// NOT anticipated:
//
//   Startup  0x1800db8d9: this->_allocator.minRangeSize = (u64)arg2 << 20;
//            0x1800db8e4: this->_allocator.maxTotalSize = (u64)arg3 << 20;
//            0x1800db8f9: this->_allocator.pageSize     = GetSystemInfo().dwPageSize;
//            0x1800db905: CALL 0x1800c7690     <- the allocator's own init
//   Shutdown 0x1800db87a: CALL 0x1800c75b0     <- the allocator's own shutdown
//
// Both bodies therefore sit *inside* the object the NO-GO covers, on the one
// path where getting it wrong destroys the heap the whole process allocates
// from. And neither has an oracle: replay is invalid by construction (running
// both would start the engine twice / free every subsystem twice), and a
// shadow would have to replicate the allocator state the NO-GO says we may
// not replicate.
//
// One consequence worth stating because CLAUDE.md's "Launcher Dialog" section
// and standalone/main.cpp's HarnessGuardUnattendedBoot depend on it: the
// registry arming bit is written from `Engine+0x6f0` by the first four
// instructions of `Engine::Shutdown`, not by `Engine::Startup`. Holding both
// back leaves that behaviour bit-for-bit as retail, so no unattended run in
// this project changes. See runtime/types/engine/Engine.h for the four stores.
//
// ---------------------------------------------------------------------------
// WHY `MainLoop::Run` IS HELD BACK
// ---------------------------------------------------------------------------
// `Run` (0x1800dd3c0) is fully understood — the body is 45 instructions and
// every one of them is accounted for in WAVE8_LANE_B.md — and it is still a
// holdback, for three reasons that compound:
//
//   1. **There is no oracle of any kind.** It is called once per process and
//      does not return until the game quits. Replay would run the entire game
//      twice; a shadow has nothing to compare, because everything `Run`
//      produces it produces by calling something else.
//   2. Two of the four things it calls are DLL-INTERNAL and unexported:
//      `MainLoop::Update` (0x1800dd070) and `MainLoop::Frame` (0x1800dd100).
//      A port would reach them by address, which works, but it means the
//      ported `Run` is a shell around un-ported, un-diffed logic — the port
//      would move no verified ground.
//   3. A defect here ends the process before any oracle can report, taking
//      the whole integration run with it. That is the one place in this wave
//      where an unverified port costs the other four lanes their measurement.
//
// This is the Wave 2 allocator verdict applied to a second function: a port
// you cannot verify is a holdback with a reason, not a port. `Run` becomes
// portable when `Update` and `Frame` are ported alongside it and the three of
// them can be checked against a recorded frame trace.
//
// ---------------------------------------------------------------------------
// HOW THE DEPENDENCIES ARE RESOLVED, AND WHY THE TWO KINDS DIFFER
// ---------------------------------------------------------------------------
// CODE dependencies go through `ResolveAddr(Module::Runtime, …)`, which is
// registry-aware: if a later wave ports `Video::Context`'s dtor or
// `CreateVideoContext`, this lane picks up that port automatically. That is
// goal.cpp's stated position and it is the right one for code.
//
// DATA dependencies go through `ResolveExportRaw` (GetProcAddress), which is
// deliberately NOT registry-aware. `ResolveAddr(Module::Runtime, addr)` checks
// the port registry FIRST (reference_resolveaddr_consults_port_registry_first.md),
// and for a data symbol that is the wrong answer: what these bodies need is
// the address of the global the DLL'S OWN CODE reads. If the registry ever
// held our own storage for `MainLoop::_singleton`, a ctor that wrote there
// would leave the DLL's internal readers looking at a null singleton, and
// nothing in this file would notice. Every data global below is resolved by
// its exported name, once, and the name is spelled as a literal.
//
// (The two static video thunks and the three `Collection::Array` destructors
// are the only dependencies with no export name at all; they go by address.)
// ============================================================================

#include <cstdint>
#include "../system/timer.h"
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/engine/Engine.h"
#include "../../types/engine/MainLoop.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring the seam locally: resolve.h
// drags in <windows.h>, and PLAN §4.1 requires runtime/src/ to stay
// platform-neutral. This declaration must stay byte-identical to resolve.h's.
// It has to sit at FILE scope, not inside the anonymous namespace below, or it
// would declare a different, internal-linkage function that never links.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The engine layouts live in `namespace RtEngine` (see the header banners): the
// exe tree's engine/types/{MainLoop,UpdateLoop}.h define GLOBAL structs of the
// same names and `engine/types` precedes `runtime/types` on the include path.
using RtEngine::Engine;
using RtEngine::MainLoop;
using RtEngine::UpdateLoop;
namespace MainLoopAddr = RtEngine::MainLoopAddr;

namespace {

// ---------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------
using VoidThisFn   = void  (*)(void*);
using CtorFn       = void* (*)(void*, void*);
using SetNextFn    = void  (*)(void*, void*);
using SecToTicksFn = int64_t (*)(float);

// Data globals — resolved by exported name, never through the registry.
// See "HOW THE DEPENDENCIES ARE RESOLVED" above.
MainLoop** MainLoopSingletonSlot() {
    static MainLoop** p = reinterpret_cast<MainLoop**>(
        ResolveExportRaw("?_singleton@MainLoop@Engine@@1PEAV12@EA"));
    return p;
}

// Video::Device::_singleton. The MainLoop ctor stores its two error callbacks
// into the pointed-at Device at +0x50/+0x58 — the profile build calls that
// setter `Video::Device::SetVideoErrorFuncs` (0x1800fd860); retail inlines it.
void** VideoDeviceSingletonSlot() {
    static void** p = reinterpret_cast<void**>(
        ResolveExportRaw("?_singleton@Device@Video@@0PEAV12@EA"));
    return p;
}

// System::Timer::_secondsPerTick — the divisor in the inlined SecondsToTicks.
// Was a RAW-DOOR data read of the DLL's slot (WAVE9_LANE_V.md §4 names this
// line). Wave 9 lane B3's System::Timer port owns the storage, so it comes
// from us; the value is the compile-time constant 1e-6 either way, and ours is
// never the zero the DLL's slot holds before Engine::Startup.
const double* SecondsPerTick() {
    static const double p = RtSystem::TimerSecondsPerTick();
    return &p;
}

// The two clock constants, as the exact doubles retail loads. These are
// `(double)(1.0f/60.0f)` and `(double)(1.0f/15.0f)` — a float widened by the
// compiler, NOT `1.0/60.0`. Retail loads them from 0x1801616a8 / 0x1801616b0;
// the profile build (0x1800d5493) shows the source wrote the float and called
// `System::Timer::SecondsToTicks(float)`. Writing the decimal literals here
// would give a different double and a different tick count. See
// runtime/types/engine/MainLoop.h.
constexpr float kFixedStepSeconds  = 1.0f / 60.0f;   // -> 0x3f91111120000000
constexpr float kMaxAccumSeconds   = 1.0f / 15.0f;   // -> 0x3fb1111120000000

// Retail's inlined `SecondsToTicks`: widen the float to double, divide by
// _secondsPerTick, truncate toward zero (CVTTSD2SI).
int64_t SecondsToTicksInline(float seconds) {
    return static_cast<int64_t>(static_cast<double>(seconds) / *SecondsPerTick());
}

}  // namespace

// ============================================================================
// ?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z   retail 0x1800dd7a0
// ============================================================================
// The whole function:
//     1800dd7a0  MOV qword ptr [RCX + 0x28],RDX
//     1800dd7a4  RET
//
// ORACLE: replay, and this is the ONE function in lane B where replay is
// genuinely valid rather than a convenient fiction. WAVE8.md §5 rules replay
// out for anything that mutates shared state, because running both mutates
// twice — but this store is IDEMPOTENT: theirs writes the identical pointer to
// the identical field, so the second write is a no-op on the live object. The
// comparison is therefore not tautological, and it checks the one thing that
// could actually be wrong: that our store lands at +0x28 and not at some other
// offset. (A tautological `self->_nextLoop == loop` after our own assignment
// would prove nothing; the static_assert in MainLoop.h already covers the
// compile-time half.)
extern "C" void RtEngine_MainLoop_SetNextLoop(MainLoop* self, UpdateLoop* loop) {
    RT_DIFF_BEGIN("Engine::MainLoop::SetNextLoop");

    self->_nextLoop = loop;

    if (rt_diff_on__) {
        UpdateLoop* ours = self->_nextLoop;
        RtOriginal<SetNextFn>("?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z")(
            self, loop);
        UpdateLoop* theirs = self->_nextLoop;
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), static_cast<const void*>(ours));
        RtDiffFormat(b, sizeof(b), static_cast<const void*>(theirs));
        RtDiffReport(&rt_diff_site__, ours == theirs, a, b);
    }
}

// ============================================================================
// ??0MainLoop@Engine@@QEAA@PEAV11@@Z                       retail 0x1800dd6b0
// ============================================================================
// Retail, instruction for instruction:
//
//   zero +0x00,+0x08,+0x10, the WORD at +0x18, and +0x20..+0x70
//   this->_engine = engine                                        (+0x78)
//   zero +0x80..+0x98
//   this->_deltaTime      = (i64)(1/60 / _secondsPerTick)         (+0x48)
//   this->_maxAccumulator = (i64)(1/15 / _secondsPerTick)         (+0x50)
//   MainLoop::_singleton = this
//   Video::Device::_singleton->[+0x50] = &OnVideoError_static
//   Video::Device::_singleton->[+0x58] = &OnVideoRestart_static
//   this->CreateVideoContext()
//   return this
//
// TWO FAITHFULNESS POINTS the assembly makes and a memset would destroy:
//
//   * The ctor writes a WORD at +0x18 (both flag bytes at once) and NEVER
//     touches +0x1a..+0x1f. Those six bytes keep whatever the allocator left
//     there. A `memset(self, 0, 0xa0)` would be wrong, and — worse — it would
//     be wrong in a way no oracle in this project could see, because nothing
//     ever reads them (reference_oracle_must_not_compare_unwritten_bytes.md,
//     reference_never_written_needs_by_whom.md). The field-by-field writes
//     below are deliberate.
//   * The two callbacks installed into the Video::Device are the DLL's OWN
//     static thunks, resolved by address. They are kept as the DLL's on
//     purpose: they are the only DLL-internal callers of OnVideoError /
//     OnVideoRestart, both of which are held back (see the detour section).
//
// ORACLE: shadow. Replay is invalid — running theirs afterwards would create a
// SECOND 0x2168-byte Video::Context, leak the first, and re-stamp the
// singleton. The shadow checks four things our body did NOT compute directly:
//
//   1. `_deltaTime` against the DLL's exported `System::Timer::SecondsToTicks`
//      applied to the same float. This is the real check — it is the only
//      arithmetic in the function, it is the thing the float-vs-double trap
//      would break, and the comparison value comes from the DLL, not from us.
//   2. `_maxAccumulator`, likewise.
//   3. `MainLoop::_singleton == this`, read back through the exported data
//      symbol (so a ctor that stamped the wrong global fails here).
//   4. `_context != nullptr` — CreateVideoContext's post-condition.
//
// It is weaker than a byte diff and this file says so plainly. It is not
// nothing: every failure mode the disassembly suggests is covered by one of
// the four terms, and per
// reference_shadow_predicate_must_name_its_failing_term.md the message below
// names all four so a divergence says which one failed.
extern "C" MainLoop* RtEngine_MainLoop_Ctor(MainLoop* self, Engine* engine) {
    RT_DIFF_BEGIN("Engine::MainLoop::MainLoop");

    self->_videoResources._data     = nullptr;
    self->_videoResources._size     = 0;
    self->_videoResources._capacity = 0;
    self->_context         = nullptr;
    self->_contextDirty    = 0;
    self->_quit            = 0;
    // +0x1a..+0x1f: retail does not write these. Neither do we.
    self->_currentLoop     = nullptr;
    self->_nextLoop        = nullptr;
    self->_startTicks      = 0;
    self->_accumulator     = 0;
    self->_totalTime       = 0;
    self->_deltaTime       = 0;
    self->_maxAccumulator  = 0;
    self->_frameTime       = 0;
    self->_frameStartTicks = 0;
    self->_frameUpdateTime = 0;
    self->_frameEndTicks   = 0;
    self->_engine          = engine;
    self->_postDraw._data       = nullptr;
    self->_postDraw._size       = 0;
    self->_postDraw._capacity   = 0;
    self->_postUpdate._data     = nullptr;
    self->_postUpdate._size     = 0;
    self->_postUpdate._capacity = 0;

    self->_deltaTime      = SecondsToTicksInline(kFixedStepSeconds);
    self->_maxAccumulator = SecondsToTicksInline(kMaxAccumSeconds);

    *MainLoopSingletonSlot() = self;

    // Video::Device::SetVideoErrorFuncs(device, &OnVideoError_static,
    //                                   &OnVideoRestart_static), inlined by
    // retail into two stores.
    uint8_t* device = static_cast<uint8_t*>(*VideoDeviceSingletonSlot());
    *reinterpret_cast<void**>(device + MainLoopAddr::DeviceErrorFuncSlot) =
        reinterpret_cast<void*>(ResolveAddr(Module::Runtime,
                                            MainLoopAddr::OnVideoError_static));
    *reinterpret_cast<void**>(device + MainLoopAddr::DeviceRestartFuncSlot) =
        reinterpret_cast<void*>(ResolveAddr(Module::Runtime,
                                            MainLoopAddr::OnVideoRestart_static));

    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, MainLoopAddr::CreateVideoContext))(self);

    if (rt_diff_on__) {
        SecToTicksFn secToTicks =
            RtOriginal<SecToTicksFn>("?SecondsToTicks@Timer@System@@SA_JM@Z");
        const bool deltaOk   = self->_deltaTime      == secToTicks(kFixedStepSeconds);
        const bool maxAccOk  = self->_maxAccumulator == secToTicks(kMaxAccumSeconds);
        const bool singleOk  = *MainLoopSingletonSlot() == self;
        const bool contextOk = self->_context != nullptr;
        char ours[160], theirs[160];
        std::snprintf(ours, sizeof(ours),
                      "delta=%lld maxAcc=%lld singleton=%p context=%p",
                      static_cast<long long>(self->_deltaTime),
                      static_cast<long long>(self->_maxAccumulator),
                      static_cast<void*>(*MainLoopSingletonSlot()),
                      self->_context);
        std::snprintf(theirs, sizeof(theirs),
                      "delta=%lld maxAcc=%lld singleton=%p context!=null "
                      "[deltaOk=%d maxAccOk=%d singletonOk=%d contextOk=%d]",
                      static_cast<long long>(secToTicks(kFixedStepSeconds)),
                      static_cast<long long>(secToTicks(kMaxAccumSeconds)),
                      static_cast<void*>(self),
                      static_cast<int>(deltaOk), static_cast<int>(maxAccOk),
                      static_cast<int>(singleOk), static_cast<int>(contextOk));
        RT_DIFF_SHADOW(deltaOk && maxAccOk && singleOk && contextOk, ours, theirs);
    }

    return self;
}

// ============================================================================
// ??1MainLoop@Engine@@QEAA@XZ                              retail 0x1800dd640
// ============================================================================
// Retail:
//
//   if (this->_context) {
//       Video::Context::~Context(this->_context);      // 0x1800f16e0, ord 562
//       Video::Context::operator delete(this->_context);// 0x180018500, ord 945
//   }
//   this->_context = nullptr;                           // UNCONDITIONAL
//   ~Array(&this->_postUpdate);                         // 0x180033a40, +0x90
//   ~Array(&this->_postDraw);                           // 0x180033960, +0x80
//   ~Array(&this->_videoResources);                     // 0x180033880, +0x00 (tail JMP)
//
// Note the null store at 0x1800dd674 is OUTSIDE the `if` — retail writes it
// even when the context was already null. Kept.
//
// The three Array destructors are DLL-internal (no export name; checked with
// pefile) and are reached by address. They are three DIFFERENT functions
// because the element types differ, which is the per-instantiation trap
// PLAN §4.3 risk 1 warns about: one Array body cannot serve two element
// widths, and here it cannot serve two element TYPES either — 0x180033880
// destroys a `System::String` inside each 0x18-byte entry, the other two
// destroy a plain 0x10-byte POD pair.
//
// ORACLE: shadow, on the post-conditions the three Array destructors and the
// context release guarantee. Replay is invalid: running theirs afterwards
// would double-free the context and the three buffers. The four terms are
// read back out of the object AFTER our body, and none of them is written by
// our own code — they are written by the DLL helpers we called, which is
// exactly what makes the check non-tautological. 0x180033880 ends with
// `_data = 0` and a single qword zero covering `_size` and `_capacity`, so
// all three counts are asserted, not just the pointers.
extern "C" void RtEngine_MainLoop_Dtor(MainLoop* self) {
    RT_DIFF_BEGIN("Engine::MainLoop::~MainLoop");

    void* context = self->_context;
    if (context) {
        reinterpret_cast<VoidThisFn>(
            ResolveAddr(Module::Runtime, 0x1800f16e0))(context);   // ~Context
        reinterpret_cast<VoidThisFn>(
            ResolveAddr(Module::Runtime, 0x180018500))(context);   // operator delete
    }
    self->_context = nullptr;

    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, 0x180033a40))(&self->_postUpdate);
    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, 0x180033960))(&self->_postDraw);
    reinterpret_cast<VoidThisFn>(
        ResolveAddr(Module::Runtime, 0x180033880))(&self->_videoResources);

    if (rt_diff_on__) {
        const bool contextOk = self->_context == nullptr;
        const bool resOk     = self->_videoResources._data == nullptr &&
                               self->_videoResources._size == 0 &&
                               self->_videoResources._capacity == 0;
        const bool drawOk    = self->_postDraw._data == nullptr &&
                               self->_postDraw._size == 0 &&
                               self->_postDraw._capacity == 0;
        const bool updateOk  = self->_postUpdate._data == nullptr &&
                               self->_postUpdate._size == 0 &&
                               self->_postUpdate._capacity == 0;
        char ours[192], theirs[192];
        std::snprintf(ours, sizeof(ours),
                      "context=%p res={%p,%d,%d} draw={%p,%d,%d} update={%p,%d,%d}",
                      self->_context,
                      static_cast<void*>(self->_videoResources._data),
                      self->_videoResources._size, self->_videoResources._capacity,
                      static_cast<void*>(self->_postDraw._data),
                      self->_postDraw._size, self->_postDraw._capacity,
                      static_cast<void*>(self->_postUpdate._data),
                      self->_postUpdate._size, self->_postUpdate._capacity);
        std::snprintf(theirs, sizeof(theirs),
                      "all null/0 [contextOk=%d resOk=%d drawOk=%d updateOk=%d]",
                      static_cast<int>(contextOk), static_cast<int>(resOk),
                      static_cast<int>(drawOk), static_cast<int>(updateOk));
        RT_DIFF_SHADOW(contextOk && resOk && drawOk && updateOk, ours, theirs);
    }
}

// ============================================================================
// Registration
// ============================================================================
// Both the exported NAME and the retail ADDRESS are registered for every port.
// Registering only the name would leave `ResolveAddr(Module::Runtime, …)`
// callers on the DLL — the gap WAVE3_LANE_A.md found the hard way, and the one
// game_main.cpp would fall into: it resolves the MainLoop ctor/dtor by name but
// several sites in this tree reach Runtime functions by address after a failed
// name resolve.
//
// Every mangled name below is ONE string literal on ONE line. A name split
// across lines or hidden behind a named constant is invisible to
// tools/runtime_census.py and tools/export_name_audit.py, and a port the census
// cannot see is a port nobody will ever check
// (reference_census_cannot_see_nonliteral_registrations.md — it has already
// produced two false progress reports in this project).
//
// The integrator wires `RtEngineCoreRegister()` into
// runtime/src/engine/rt_engine_port.cpp alongside lane A's register function;
// this lane does NOT create or edit that file.
int RtEngineCoreRegister() {
    int n = 0;

    RegisterRuntimeExport("??0MainLoop@Engine@@QEAA@PEAV11@@Z", (void*)&RtEngine_MainLoop_Ctor); ++n;
    RegisterRuntimeAddr(0x1800dd6b0, (void*)&RtEngine_MainLoop_Ctor);

    RegisterRuntimeExport("??1MainLoop@Engine@@QEAA@XZ", (void*)&RtEngine_MainLoop_Dtor); ++n;
    RegisterRuntimeAddr(0x1800dd640, (void*)&RtEngine_MainLoop_Dtor);

    RegisterRuntimeExport("?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z", (void*)&RtEngine_MainLoop_SetNextLoop); ++n;
    RegisterRuntimeAddr(0x1800dd7a0, (void*)&RtEngine_MainLoop_SetNextLoop);

    // DELIBERATELY ABSENT — see the holdback sections in the banner:
    //   ??0Engine@0@QEAA@PEAX@Z              0x1800dbf00  allocator, NO-GO
    //   ??1Engine@0@QEAA@XZ                  0x1800dbec0  allocator, NO-GO
    //   ?Startup@Engine@1@QEAAIPEBDII_N@Z    0x1800db8a0  allocator, NO-GO
    //   ?Shutdown@Engine@1@QEAAXXZ           0x1800db4f0  allocator, NO-GO
    //   ?Run@MainLoop@Engine@@…              0x1800dd3c0  no oracle, kills the run
    //   ?OnVideoError@MainLoop@Engine@@…     0x1800dc5e0  needs a detour
    //   ?OnVideoRestart@MainLoop@Engine@@…   0x1800dc3b0  needs a detour
    //   ?_singleton@MainLoop@Engine@@1PE…    0x1801fe750  DATA symbol, nothing to port
    return n;
}
