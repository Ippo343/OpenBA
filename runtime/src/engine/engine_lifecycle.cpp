// ============================================================================
// runtime/src/engine/engine_lifecycle.cpp — Wave 9 lane ENG
// Engine::Engine ctor / dtor / Startup / Shutdown  (+ 4 helpers they need)
// ============================================================================
//
// These four are the wall cutover boot attempts 04, 05 and 06 all died on:
// with Runtime-x64.dll absent, `code/Game/Game/game_main.cpp` resolves all four
// to null and returns 1 or 2 out of `RunGameMain` before the engine ever
// starts. Nothing else in Wave 9 can be measured past that point.
//
//   | Export                              | Retail      | Ord  | This file |
//   |-------------------------------------|-------------|------|-----------|
//   | ??0Engine@0@QEAA@PEAX@Z             | 0x1800dbf00 |  174 | ported    |
//   | ??1Engine@0@QEAA@XZ                 | 0x1800dbec0 |  586 | ported    |
//   | ?Startup@Engine@1@QEAAIPEBDII_N@Z   | 0x1800db8a0 | 4094 | ported    |
//   | ?Shutdown@Engine@1@QEAAXXZ          | 0x1800db4f0 | 4047 | ported    |
//   | ??0Configuration@System@@QEAA@XZ    | 0x1800c51b0 |  113 | ported    |
//   | ??0AchievementManager@Achievement@@… | 0x18009eb00 |   28 | ported    |
//   | ??2Device@Video@@SAPEAX_K@Z         | 0x18000c9b0 |  765 | ported    |
//   | ??3Device@Video@@SAXPEAX@Z          | 0x18000ca30 |  947 | ported    |
//
// Every mangled name in this file — the eight above and all 40-odd
// dependencies — was checked against the export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE the body was
// written. The addresses and ordinals above are that query's output. There are
// no phantom names here.
//
// Kill switch: RT_ENGINELIFE=0.
//
// ---------------------------------------------------------------------------
// 1. THE "ALLOCATOR, NO-GO" HOLDBACK WAS REAL WHEN IT WAS WRITTEN AND IS NOW
//    STALE — AND AT CUTOVER IT HAS INVERTED
// ---------------------------------------------------------------------------
// Wave 8 lane B held all four back with one reason: `System::MemoryAllocator`
// is embedded at Engine offset 0, `Startup` seeds three of its fields and calls
// its init, `Shutdown` calls its shutdown, and WAVE2_ALLOCATOR.md carried a
// NO-GO on the allocator port. Every one of those facts is still true. What has
// changed is the NO-GO, and it changed twice:
//
//   * Wave 9 lanes E/E2/E3 built the shadow oracle the NO-GO demanded and ran
//     it. The allocator port is complete — Allocate, Free, Startup, Shutdown,
//     Construct, Destruct, AddNewMemoryRange, ExpandMemoryRangeTable and the
//     six Platform* members all have bodies in runtime/src/system/allocator*.
//   * Lane E4 wired the CUTOVER arming path, and cutover attempt 06 logged
//         [RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- 27 exports
//         registered, no detour installed ...
//     So with the DLL gone, OUR allocator is the only allocator in the process.
//     "We must not take over the allocator" has become "we already have".
//
// ---------------------------------------------------------------------------
// 2. THE ONE DECISION THIS LANE HAD TO MAKE: DOES THE PORT CREATE THE ARENA?
// ---------------------------------------------------------------------------
// YES. IT MUST. Nothing else does, and the "ARMED" line above does not mean
// what it looks like it means.
//
// `RtSystemAllocatorRegister()`'s cutover branch (allocator.cpp) calls
// `RtAllocSwapArmCutover`, which stores two function pointers and returns, and
// then `RegisterAllocatorExports()`, which is 27 `RegisterRuntimeExport` calls.
// **Neither constructs a MemoryAllocator, and neither reserves one byte of
// arena.** "ARMED" means the export names now point at our bodies. It does not
// mean an arena exists.
//
// The arena is created in exactly one place in the whole process. Lane E2
// proved it two independent ways and printed the proof:
//
//     [RtAllocShadow] Engine::Startup ENTRY (min=512MB max=0MB):
//                     real arena live=0 bytes=0 ranges=0, skipped so far=0
//
//   * a full rel32 scan of Runtime-x64.dll's .text finds ZERO calls to
//     `?Startup@MemoryAllocator@System@@QEAA_N_K0@Z` (0x1800c7620): MSVC
//     inlined it into `Engine::Startup` and nothing else ever calls it;
//   * `rtalloc::Construct` leaves `pageSize` at 0, and `AddNewMemoryRange`
//     divides by `pageSize`, so no allocation CAN precede `Engine::Startup`.
//
// So there is no double-creation hazard to avoid, and the correct behaviour is
// the byte-faithful one: reproduce retail's writes and let them create the
// arena once. Concretely, retail's first four steps
//
//     0x1800db8d9  this->_allocator.minRangeSize = (u64)minMB << 20
//     0x1800db8e4  this->_allocator.maxTotalSize = (u64)maxMB << 20
//     0x1800db8f9  this->_allocator.pageSize     = GetSystemInfo().dwPageSize
//     0x1800db905  CALL 0x1800c7690              ; AddNewMemoryRange(this, 0)
//     0x1800db917  CALL 0x1800c5fa0              ; Error::SetText("Memory: The
//                                                ; application failed to start
//                                                ; the memory allocator!", 0)
//
// ARE `MemoryAllocator::Startup`, verbatim — the same four operations in the
// same order, ending in the same error string (read at 0x180155e48 and
// byte-compared with the literal in `rtalloc::Startup`). This body therefore
// calls `?Startup@MemoryAllocator@System@@QEAA_N_K0@Z` and does not re-derive
// them. That is not a shortcut: it is what keeps the DLL-PRESENT path honest
// too, because with `RT_ALLOC` unset that name resolves to the DLL's
// own 0x1800c7620 and the arena is created by retail's code exactly as before.
//
// The alternative — "arm from Startup and stop arming early" — was considered
// and rejected: `RtSystemAllocatorRegister()` must run inside `RuntimePortInit`
// because ~1,180 call sites resolve MemAlloc/MemFree before GameMain, and a
// registration that happened during `Engine::Startup` would leave every
// early-resolved site frozen on a null (WAVE9_LANE_H.md §5).
//
// ---------------------------------------------------------------------------
// 3. THE .data GLOBALS ARE THE REAL CUTOVER HAZARD, NOT THE ALLOCATOR
// ---------------------------------------------------------------------------
// Retail's four bodies write eleven absolute .data addresses (0x1801fc670,
// 0x1801fc678, 0x1801fc6a8, 0x1801fc6b0, 0x1801fc6c8, 0x1801fc6d0, 0x1801fc6e8,
// 0x1801fc6f8, 0x1801fc709, 0x1801fe720/728, 0x1801fe748, 0x1801fe758,
// 0x1801fe768, 0x1801fe7a0). With the DLL absent every one of those addresses
// is unmapped. A "faithful" port that stored to them would be a wild write to
// nothing on the very first boot.
//
// Ten of them are `?_singleton@…` DATA exports, and MSVC inlined the
// constructor that owns each one straight into `Engine::Startup`. Wave 9 lane X
// ported those constructors as standalone bodies and registered them, and each
// publishes its slot through `RT_DATA_SLOT` — the port-aware door — so the
// store follows lane C2's mint at cutover. This body therefore CALLS the
// constructor by its exported name instead of replaying the inlined stores.
// That is byte-equivalent, checked one at a time: e.g. `??0Configuration@Video@@`
// (0x1800f6f10) is instruction-for-instruction the block at 0x1800dbc2f-c87,
// singleton store included, and the same holds for FileSystem, Modification,
// Factory, ExternalFactory, Application, Audio::Configuration and
// Input::Configuration.
//
// TWO of the inlined constructors are NOT in lane X's fourteen, and this lane
// found that gap: `System::Configuration` (0x1800c51b0) and
// `Achievement::AchievementManager` (0x18009eb00). Both publish a singleton;
// `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` has two LIVE readers
// in our own tree (code/Game/Game/gameupdateloop_serialize.cpp:189 and
// eventlog_achievement.cpp), so at cutover the mint would have handed out
// storage that nothing ever wrote and both readers would see null forever. They
// are ported below, as #15 and #16 of lane X's list.
//
// The remaining stores are handled as follows:
//   0x1801fe720 / 0x1801fe728  Timer's two constants + PlatformStartup, which
//                              together ARE `?Startup@Timer@System@@SA_NXZ`
//                              (0x1800ca7c0 writes the same two and tail-jumps
//                              to 0x1800ca860). Called by name; Wave 9 lane B3
//                              owns the storage, so it is ours already.
//   0x1801fe748                `?_singleton@Engine@1@0PEAV11@EA`, ord 4808 —
//                              an exported DATA symbol, reached with
//                              RT_DATA_SLOT like the other ten.
//   0x1801fc709                `?_hideDisplay@Error@System@@0_NA`, ord 4791 —
//                              likewise.
//   0x1801fc6d0                NOT exported, and the ONE address in these four
//                              bodies with no name. It is a plugin HMODULE that
//                              Shutdown FreeLibrary's; see the note at that
//                              call site for the only guard in this file.
//
// ---------------------------------------------------------------------------
// 4. THE ORACLE, HONESTLY
// ---------------------------------------------------------------------------
// There is NO differential oracle for any of the four, and this file will not
// manufacture one. Each runs exactly once per process and each mutates global
// state that a replay would mutate twice — running `Startup` again would create
// a second FileSystem, a second Video::Device and a second 512 MB arena;
// running `Shutdown` again would double-free every subsystem. `the raw DLL door`
// against the shipped body is not a comparison here, it is a second side
// effect.
//
// What they get instead is a SHADOW on post-conditions the body did not compute
// directly, plus a did-work counter, per
// reference_probe_needs_an_effectiveness_control.md. The load-bearing terms are
// the ones written by somebody ELSE's code:
//
//   ctor      `*?_singleton@Engine@1@ == this` (through the exported slot, so a
//             ctor that stamped the wrong global fails), and the allocator
//             sub-object's ctor post-conditions minBin==0xbf / pageSize==0 /
//             ranges==null — all three written by rtalloc::Construct, not here.
//   Startup   `_startupComplete == 1`, `rangeCount >= 1` and
//             `totalReserved >= minRangeSize` (written by AddNewMemoryRange),
//             `minRangeSize == (u64)minMB << 20` (the one shift that could be
//             wrong), and all sixteen subsystem pointers non-null (written by
//             the sixteen constructors we called, not by us).
//   dtor      both singleton slots null.
//   Shutdown  every subsystem pointer that was non-null on entry has had its
//             teardown run, counted; and `ranges == nullptr` is NOT asserted
//             because `MemoryAllocator::Shutdown` releases the ranges without
//             nulling the pointer (allocator.cpp) — asserting it would be the
//             reference_oracle_must_not_compare_unwritten_bytes.md mistake.
//
// A shadow is weaker than a byte diff and this file says so plainly. Two
// further limits, stated rather than buried:
//
//   * `Engine::Shutdown`'s `calls=` is ZERO and will stay zero. game_main.cpp
//     resolves `?Shutdown@Engine@1@QEAAXXZ` only to satisfy its own null guard
//     (line 426) and then calls its own `ShutdownEngineStaged()` instead (line
//     924). Nothing in this project calls the real one. It is ported because
//     the guard demands a non-null pointer and because leaving it null is what
//     was blocking the boot — NOT because it is verified. It is not.
//   * Arming this lane DISABLES the allocator shadow driver. `RtAllocShadowInstall`
//     detours the DLL's `Engine::Startup` at 0x1800db8a0 to arm the replica; our
//     port never executes those bytes, so the detour never fires and the driver
//     reports `skipped_pre_startup=N`, i.e. INVALID. Any run of
//     `RT_ALLOC_SHADOW=1` must also set `RT_ENGINELIFE=0`.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/engine/Engine.h"
#include "../../types/application/Application.h"   // ENDGAME B.3.7: PlatformStartup / PlatformShutdown
#include "../../types/achievement/AchievementManager.h"   // ENDGAME B.3.7 fragment: the ctor door's named fields
#include "../../types/input/Input.h"    // ENDGAME B.3.7 callers (K5): Input::Configuration
#include "../../types/input/Device.h"   // ENDGAME B.3.7 callers (K5): Input::Device
#include "../../types/video/Device.h"      // ENDGAME B.3.5: Video::Device / Configuration
#include "../../types/audio/Audio.h"       // ENDGAME B.3.7 callers (K4): Audio::Configuration / Device
#include "../../types/audio/Stream.h"      // ENDGAME B.3.9 ENG W2: ~Stream in Shutdown
#include "../../types/system/MemoryAllocator.h"
#include "../../types/system/String.h"
#include "../../types/system/Error.h"      // ENDGAME B.3.9 SYS: Error::_hideDisplay
#include "../../types/system/Configuration.h"   // ENDGAME B.3.9 SYS: Configuration::_singleton

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning engine.cpp gives (resolve.h drags in <windows.h>, and
// PLAN §4.1 requires runtime/src/ to stay platform-neutral). Must stay
// byte-identical to resolve.h's declaration, and must sit at FILE scope.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// engine_lifecycle_win.cpp — the three OS calls these bodies make that are not
// already behind somebody else's seam.
long RtEngineLifecyclePlatformCoInit();
void RtEngineLifecyclePlatformCoUninit();
void RtEngineLifecyclePlatformFreeLibrary(void* module);

// singleton_ctors_win.cpp (Wave 9 lane X). `??0MemoryAllocator@System@@` routes
// to `rtalloc::Construct`, whose first act is `g_plat.lockInit(self->lock)`;
// `g_plat` is filled by `RtSystemAllocatorInstallPlatform()` and on a run with
// `RT_ALLOC` unset NOBODY calls it, so that would be an EXEC of 0.
// This lane checks the seam at REGISTRATION time, not in the body.
bool RtLaneXEnsureAllocatorPlatform();

// ENDGAME B.3.7: `Engine::Engine` is the real class; a global
// `using RtEngine::Engine;` is ill-formed next to `namespace Engine`.
using EngineT = ::Engine::Engine;
// NOT `using rt::system::MemoryAllocator;` -- runtime/types/common.h
// forward-declares a GLOBAL `struct MemoryAllocator`, and the using-declaration
// collides with it. Spelled out at each use instead.
typedef rt::system::MemoryAllocator RtMemoryAllocator;

// The unexported Input::KeyMap scalar deleting destructor 0x180053440, ported
// and registered by address in runtime/src/input/keymap_save.cpp (block B5).
void* RtInput_KeyMap_DeletingDtor(void* selfPtr);

namespace {

// ---------------------------------------------------------------------------
// Dependencies. Every one goes through RT_EXPORT_ONCE — the PORT-AWARE door
// with the never-cache-a-null contract — so a later wave's port of any of them
// is picked up automatically, and a name that is missing at cutover is NAMED
// once by RecordResolveMiss ([RtUnresolved]) instead of dying anonymously.
//
// Nothing here uses the raw DLL door/the raw DLL door. There is no oracle in these
// four bodies to need one (§4), and a raw door on this path would be a live
// bypass at cutover, not an oracle.
// ---------------------------------------------------------------------------
typedef void*    (*CtorFn)(void*);                 // T::T()               -> this
typedef void*    (*CtorArgFn)(void*, void*);       // T::T(void*)          -> this
typedef void*    (*CtorEnumFn)(void*, int);        // T::T(DeviceType)     -> this
typedef void     (*VoidThisFn)(void*);
typedef bool     (*BoolThisFn)(void*);
typedef bool     (*BoolVoidFn)();
typedef void*    (*MemAllocFn)(uint64_t, int);
typedef void     (*MemFreeFn)(void*);
typedef bool     (*AllocStartupFn)(void*, uint64_t, uint64_t);
// ENDGAME B.3.1 step 2: `StringSetFn` is gone — `Set` (0x1800c95d0) and the
// zeroing before it are the `const char*` constructor, 0x18000b630.
typedef void     (*SetCommandLineFn)(void*, const System::String*);
typedef void     (*ErrorSetTextFn)(const char*, bool);
typedef bool     (*PlatformConfigureFn)(void*, bool, void*);

// The two allocator statics every `new`/`delete` in these bodies goes through.
// Retail's unexported per-class `operator new` at 0x180011640 is
// `MemAlloc(size, 4)` and nothing else — 20 instructions, read in full; the
// exported `??2Device@Video@@SAPEAX_K@Z` (0x18000c9b0) is the identical body
// with `R8D = 8` instead of `4`, and `??3Device@Video@@` (0x18000ca30) and the
// unexported delete at 0x180006a10 are both `MemFree` verbatim.
void* MemAlloc(uint64_t size, int align) {
    return RT_EXPORT_ONCE(MemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(size, align);
}
void MemFree(void* p) {
    RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// ---- the exported DATA slots, all through the port-aware door ---------------
// See engine.cpp's "HOW THE DEPENDENCIES ARE RESOLVED" banner for why data is
// special. It concluded that data must go through the raw DLL door so a ported
// body writes the storage the DLL's own code reads. That was right FOR WAVE 8,
// and it is wrong at cutover, where there is no DLL code and the storage is
// lane C2's mint. RT_DATA_SLOT is the door that gets both: registry first
// (the mint, at cutover), the DLL's .data second (today).
EngineT** EngineSingletonSlot() {
    return reinterpret_cast<EngineT**>(&::Engine::Engine::_singleton);
}

// Effectiveness counters. Read by RtEngineLifecycleSummary(); the four bodies
// run once each, so "did it run at all" is the only coverage question there is.
unsigned g_ctorRuns     = 0;
unsigned g_startupRuns  = 0;
unsigned g_dtorRuns     = 0;
unsigned g_shutdownRuns = 0;
unsigned g_shutdownSteps = 0;   // subsystems actually torn down
unsigned g_startupFailAt = 0;   // 0 == reached the end

}  // namespace

// ============================================================================
// ??0Configuration@System@@QEAA@XZ                         retail 0x1800c51b0
// ============================================================================
// Lane X's #15. Its whole body, and it is also the block MSVC inlined at
// 0x1800db938-0x1800db954:
//
//   XOR EDX,EDX
//   MOV [RCX],RDX          MOV [RCX+0x08],RDX
//   LEA RAX,[RCX+0x10]
//   MOV [RAX],RDX          MOV dword [RAX+0x08],EDX     ; an Array<T> {ptr,int}
//   MOV [RCX+0x20],RDX     MOV dword [RCX+0x28],EDX     ; a second one
//   MOV [0x1801fc6f8],RCX  ; ?_singleton@Configuration@System@@
//   MOV RAX,RCX
//
// The two `Array` sub-objects are written as {pointer, int} and the four bytes
// at +0x1c and +0x2c are NOT touched — retail leaves whatever the allocator
// left there. A `memset(self, 0, 0x30)` would be wrong and no oracle in this
// project could see it (reference_oracle_must_not_compare_unwritten_bytes.md).
extern "C" void* RtSystem_Configuration_Ctor(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    *reinterpret_cast<void**>(p + 0x00)    = nullptr;
    *reinterpret_cast<void**>(p + 0x08)    = nullptr;
    *reinterpret_cast<void**>(p + 0x10)    = nullptr;
    *reinterpret_cast<int32_t*>(p + 0x18)  = 0;
    *reinterpret_cast<void**>(p + 0x20)    = nullptr;
    *reinterpret_cast<int32_t*>(p + 0x28)  = 0;
    System::Configuration::_singleton = static_cast<System::Configuration*>(self);   // class static (B.3.9 SYS)
    return self;
}

// ============================================================================
// ??0AchievementManager@Achievement@@QEAA@XZ               retail 0x18009eb00
// ============================================================================
// Lane X's #16, and the one that matters most of the two: two files under
// code/Game/ read `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` at
// runtime, so at cutover an unwritten mint row is a null in live game code.
//
// Eight qwords and a dword, then the singleton. Identical to the inlined block
// at 0x1800dbe55-0x1800dbe83.
//
// ENDGAME B.3.7 fragment: the stores name the class's fields (same bytes:
// +0x00..+0x3f and the dword at +0x40; `_pad44` stays untouched, as retail
// leaves it). The door stays a door (B36_RULES 7 / B7-13).
extern "C" void* RtAchievement_AchievementManager_Ctor(void* self) {
    auto* m = reinterpret_cast<Achievement::AchievementManager*>(self);  // B.3.7 bridge
    m->_achievements          = nullptr;   // +0x00
    m->_completed._data       = nullptr;   // +0x08
    m->_completed._size       = 0;         // +0x10
    m->_completed._capacity   = 0;         // +0x14
    m->_instances._data       = nullptr;   // +0x18
    m->_instances._size       = 0;         // +0x20
    m->_instances._capacity   = 0;         // +0x24
    m->_eventHandler          = nullptr;   // +0x28
    m->_updateTimes._data     = nullptr;   // +0x30
    m->_updateTimes._size     = 0;         // +0x38
    m->_updateTimes._capacity = 0;         // +0x3c
    m->_lastAchievementUpdate = 0;         // +0x40
    Achievement::AchievementManager::_singleton = m;   // class static (B.3.9 ENG)
    return self;
}

// ============================================================================
// ??2Device@Video@@SAPEAX_K@Z / ??3Device@Video@@SAXPEAX@Z
//                                        retail 0x18000c9b0 / 0x18000ca30
// ============================================================================
// Video::Device's class `operator new` / `operator delete`. Both were read in
// full: each takes `MemoryAllocator::_singleton`'s critical section at +0x640,
// calls `Allocate(size, 8)` / `Free(p)`, and releases it — which is exactly
// `MemAlloc(size, 8)` / `MemFree(p)`. `operator delete` keeps retail's leading
// `TEST RCX,RCX; JZ ret`, which `MemFree` also has.
extern "C" void* RtVideo_Device_OperatorNew(uint64_t size) {
    return MemAlloc(size, 8);
}
extern "C" void RtVideo_Device_OperatorDelete(void* p) {
    MemFree(p);
}

// ============================================================================
// ??0Engine@0@QEAA@PEAX@Z                                  retail 0x1800dbf00
// ============================================================================
// Retail, instruction for instruction:
//
//   CALL 0x1800c79a0                    ; MemoryAllocator::MemoryAllocator(this)
//   XOR EAX,EAX
//   [RDI+0x670 .. +0x6e8] = 0           ; fifteen qwords, skipping +0x698
//   [RDI+0x698] = RBX                   ; = the platform handle argument
//   MOV word ptr [RDI+0x6f0],AX         ; _startupComplete AND _noConfigure, as
//                                       ; ONE 16-bit store
//   MOV [0x1801fe748],RDI               ; Engine::_singleton = this
//   MOV RAX,RDI                         ; return this
//
// Note the allocator ctor runs FIRST and on the unmodified `this` — that is the
// evidence in Engine.h that the allocator sub-object is at offset 0, and it is
// why the ctor may not be reordered: `rtalloc::Construct` initialises the
// critical section every later allocation takes.
//
// `_platformHandle` is stored between the zeroed fields, from RBX, which was
// loaded from RDX before the call. Writing it in field order below matches.
extern "C" EngineT* RtEngine_Engine_Ctor(EngineT* self, void* platformHandle) {

    RT_EXPORT_ONCE(CtorFn, "??0MemoryAllocator@System@@QEAA@XZ")(&self->_allocator);

    self->_fileSystem         = nullptr;
    self->_persistence        = nullptr;
    self->_modification       = nullptr;
    self->_configuration      = nullptr;
    self->_application        = nullptr;
    self->_platformHandle     = platformHandle;
    self->_factory            = nullptr;
    self->_internalFactory    = nullptr;
    self->_externalFactory    = nullptr;
    self->_videoConfiguration = nullptr;
    self->_videoDevice        = nullptr;
    self->_audioConfiguration = nullptr;
    self->_audioDevice        = nullptr;
    self->_inputConfiguration = nullptr;
    self->_inputDevice        = nullptr;
    self->_achievementManager = nullptr;
    self->_startupComplete    = 0;
    self->_noConfigure        = 0;
    // +0x6f2..+0x6f7: retail's 16-bit store at +0x6f0 does not reach them and
    // neither do we.

    if (EngineT** slot = EngineSingletonSlot()) *slot = self;

    ++g_ctorRuns;


    return self;
}

// ============================================================================
// ??1Engine@0@QEAA@XZ                                      retail 0x1800dbec0
// ============================================================================
// The whole function:
//
//   XOR EAX,EAX
//   MOV [0x1801fe748],RAX     ; Engine::_singleton         = null
//   MOV [0x1801fc718],RAX     ; MemoryAllocator::_singleton = null
//   ADD RCX,0x640
//   JMP DeleteCriticalSection ; the allocator's lock, in place
//
// The last two lines are `MemoryAllocator::~MemoryAllocator` (0x1800c7960)
// inlined: that body is `MOV [0x1801fc718],0` + the same tail jump, and
// `rtalloc::Destruct` is its port. Calling the exported dtor rather than
// replaying the two stores is what routes the singleton null through the
// port-aware slot; the ORDER is preserved (Engine's global first, then the
// allocator's), which matters only in that nothing observes between them.
//
// The Engine object itself is NOT freed here: game_main.cpp owns the storage
// (a 0x700-byte stack object), exactly as retail's caller did.
extern "C" void RtEngine_Engine_Dtor(EngineT* self) {

    if (EngineT** slot = EngineSingletonSlot()) *slot = nullptr;

    RT_EXPORT_ONCE(VoidThisFn, "??1MemoryAllocator@System@@QEAA@XZ")(&self->_allocator);

    ++g_dtorRuns;

}

// ============================================================================
// ?Startup@Engine@1@QEAAIPEBDII_N@Z                        retail 0x1800db8a0
// ============================================================================
// 367 instructions, and the shape is one pattern repeated sixteen times:
//
//     void* p = operator new(SIZE);        // == MemAlloc(SIZE, 4)
//     if (p) <constructor, INLINED>;       // else p stays null
//     this-><field> = p;                   // the null is STORED, not rejected
//     if (!<Subsystem>::Startup(p)) return 0;
//
// Two faithfulness points the assembly makes that a tidy rewrite would lose:
//
//   * The allocation failure branch stores NULL into the Engine field and then
//     calls the subsystem's Startup ON THE NULL (0x1800dba7e/0x1800dba85 for
//     Persistence is the clearest). Retail does not check. Neither do we —
//     CLAUDE.md's "don't fix a faithful crash with a null check" applies
//     exactly here, and in practice MemAlloc on a fresh 512 MB arena does not
//     fail.
//   * Every failure after the allocator's own jumps to 0x1800db91c, which is
//     `XOR EAX,EAX` and NOTHING ELSE — no Error::SetText, no cleanup, no
//     Shutdown. Only the allocator failure sets the error text. So a failed
//     Startup leaves a half-built Engine that the caller must still destroy,
//     and `_startupComplete` stays 0, which is what arms the launcher dialog
//     on the next run (Engine.h, CLAUDE.md "Launcher Dialog").
//
// The `unsigned` return is 1 on success and 0 on every failure.
uint32_t Engine::Engine::Startup(const char* cmdLine, uint32_t minRangeMB,
                                uint32_t maxTotalMB, bool noConfigure) {

    ++g_startupRuns;
    g_startupFailAt = 0;

    // 0x1800db8c5-0x1800db8cf. The bool argument arrives on the stack (5th).
    _noConfigure     = noConfigure ? 1 : 0;
    _startupComplete = 0;

    // --- 0x1800db8d6 .. 0x1800db91e: MemoryAllocator::Startup, inlined -------
    // See §2 of the banner. This is the only place in the process an arena is
    // created, and this call is retail's four operations plus retail's error
    // string, not a substitute for them.
    if (!RT_EXPORT_ONCE(AllocStartupFn, "?Startup@MemoryAllocator@System@@QEAA_N_K0@Z")(
            &_allocator, static_cast<uint64_t>(minRangeMB),
            static_cast<uint64_t>(maxTotalMB))) {
        g_startupFailAt = 1;
        return 0;
    }

    // --- 0x1800db923: System::Configuration ---------------------------------
    void* configuration = MemAlloc(0x30, 4);
    if (configuration) RtSystem_Configuration_Ctor(configuration);
    _configuration = configuration;

    // --- 0x1800db967: Configuration::SetCommandLine(String(cmdLine)) --------
    // A temporary System::String, which is one pointer. Retail zeroes it,
    // calls String::Set, passes it by reference and then inlines ~String
    // (0x180006c80). ENDGAME B.3.1 step 2: the zero-then-Set pair IS the
    // `const char*` constructor (0x18000b630) and the inlined release IS the
    // destructor, so the whole block is one declaration and its scope.
    {
        System::String tmp(cmdLine);
        RT_EXPORT_ONCE(SetCommandLineFn,
            "?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z")(
                _configuration, &tmp);
    }

    // --- 0x1800db9aa .. 0x1800db9d9: System::Timer::Startup, inlined --------
    // Retail writes `_ticksPerSecond = 1000000` and
    // `_secondsPerTick = *0x1801616d0 / (double)1000000` and then CALLs
    // Timer::PlatformStartup. `?Startup@Timer@System@@SA_NXZ` (0x1800ca7c0) is
    // those same two stores plus a tail jump to the same PlatformStartup, so
    // the call below is the inlined block, not an approximation. Wave 9 lane B3
    // owns the two constants' storage, which is also why ours is never the zero
    // the DLL's slot holds before this point.
    if (!RT_EXPORT_ONCE(BoolVoidFn, "?Startup@Timer@System@@SA_NXZ")()) {
        g_startupFailAt = 2;
        return 0;
    }

    // --- 0x1800db9df: IO::FileSystem ----------------------------------------
    void* fileSystem = MemAlloc(0x50, 4);
    if (fileSystem)
        RT_EXPORT_ONCE(CtorFn, "??0FileSystem@IO@@QEAA@XZ")(fileSystem);
    _fileSystem = fileSystem;
    if (!RT_EXPORT_ONCE(BoolThisFn, "?Startup@FileSystem@IO@@QEAA_NXZ")(fileSystem)) {
        g_startupFailAt = 3;
        return 0;
    }

    // --- 0x1800dba5e: IO::Persistence ---------------------------------------
    void* persistence = MemAlloc(0x98, 4);
    if (persistence)
        RT_EXPORT_ONCE(CtorFn, "??0Persistence@IO@@QEAA@XZ")(persistence);
    _persistence = persistence;
    if (!RT_EXPORT_ONCE(BoolThisFn, "?Startup@Persistence@IO@@QEAA_NXZ")(persistence)) {
        g_startupFailAt = 4;
        return 0;
    }

    // --- 0x1800dba95: IO::Modification --------------------------------------
    // RefreshList returns void and its result is not tested; a failure here
    // cannot stop Startup, by retail's design.
    void* modification = MemAlloc(0x28, 4);
    if (modification)
        RT_EXPORT_ONCE(CtorFn, "??0Modification@IO@@QEAA@XZ")(modification);
    _modification = modification;
    RT_EXPORT_ONCE(VoidThisFn, "?RefreshList@Modification@IO@@QEAAXXZ")(modification);

    // --- 0x1800dbae0: Object::Factory ---------------------------------------
    void* factory = MemAlloc(0x10, 4);
    if (factory)
        RT_EXPORT_ONCE(CtorFn, "??0Factory@Object@@QEAA@XZ")(factory);
    _factory = factory;
    if (!RT_EXPORT_ONCE(BoolThisFn, "?Startup@Factory@Object@@QEAA_NXZ")(factory)) {
        g_startupFailAt = 5;
        return 0;
    }

    // --- 0x1800dbb20: Object::InternalFactory (no Startup) -------------------
    void* internalFactory = MemAlloc(0x9e8, 4);
    if (internalFactory)
        RT_EXPORT_ONCE(CtorFn, "??0InternalFactory@Object@@QEAA@XZ")(internalFactory);
    _internalFactory = internalFactory;

    // --- 0x1800dbb47: Object::ExternalFactory (no Startup) -------------------
    void* externalFactory = MemAlloc(0x48, 4);
    if (externalFactory)
        RT_EXPORT_ONCE(CtorFn, "??0ExternalFactory@Object@@QEAA@XZ")(externalFactory);
    _externalFactory = externalFactory;

    // --- 0x1800dbbb3: Engine::Application ------------------------------------
    // The ctor takes the platform handle, read back out of the Engine field at
    // 0x1800dbbc9 rather than from the register — so an Engine whose +0x698 was
    // changed since the ctor would see the new value. Kept.
    void* application = MemAlloc(0x30, 4);
    if (application)
        RT_EXPORT_ONCE(CtorArgFn, "??0Application@0@QEAA@PEAX@Z")(
            application, _platformHandle);
    _application = application;
    // ENDGAME B.3.7: `?PlatformStartup@Application@1@AEAA_NXZ` was a name
    // door; a member call on the real class now (the ctor above stays the
    // `Ctor_Application` door, B7-10).
    if (!static_cast< ::Application::Application*>(application)->PlatformStartup()) {
        g_startupFailAt = 6;
        return 0;
    }

    // --- 0x1800dbc12: CoInitializeEx(nullptr, COINIT_MULTITHREADED) ---------
    // Result discarded: retail neither tests nor stores it.
    (void)RtEngineLifecyclePlatformCoInit();

    // --- 0x1800dbc1c: Video::Configuration ----------------------------------
    void* videoConfiguration = MemAlloc(0x44, 4);
    if (videoConfiguration)
        RT_EXPORT_ONCE(CtorFn, "??0Configuration@Video@@QEAA@XZ")(videoConfiguration);
    _videoConfiguration = videoConfiguration;
    if (!reinterpret_cast<Video::Configuration*>(videoConfiguration)
             ->Startup()) {                                  // B.3.5 bridge
        g_startupFailAt = 7;
        return 0;
    }

    // --- 0x1800dbcaa: the two _noConfigure stores into Video::Configuration --
    // Both re-READ `this->_videoConfiguration` (0x1800dbcb3, 0x1800dbcc5) and
    // dereference it unguarded. If the allocation above failed this faults, in
    // retail exactly as here.
    if (_noConfigure)
        *(static_cast<uint8_t*>(_videoConfiguration) + 0x40) = 0;
    *(static_cast<uint8_t*>(_videoConfiguration) + 0x42) = _noConfigure;

    // --- 0x1800dbccf: Video::Device -----------------------------------------
    // Allocated through Video::Device's own class operator new (align 8, not 4)
    // and constructed with DeviceType 0. Lane X's negative control proved the
    // ctor's CMOVE publishes `?_singleton@Device@Video@@` only for type 0, so
    // this argument is load-bearing.
    // EnumerateVideoModes returns bool and retail does NOT test it.
    void* videoDevice = RtVideo_Device_OperatorNew(0x108);
    if (videoDevice)
        RT_EXPORT_ONCE(CtorEnumFn, "??0Device@Video@@QEAA@W4DeviceType@01@@Z")(
            videoDevice, 0);
    _videoDevice = videoDevice;
    (void)reinterpret_cast<Video::Device*>(videoDevice)
        ->EnumerateVideoModes();                             // B.3.5 bridge

    // --- 0x1800dbd00: Audio::Configuration ----------------------------------
    void* audioConfiguration = MemAlloc(0x10, 4);
    if (audioConfiguration)
        RT_EXPORT_ONCE(CtorFn, "??0Configuration@Audio@@QEAA@XZ")(audioConfiguration);
    _audioConfiguration = audioConfiguration;
    if (!reinterpret_cast<Audio::Configuration*>(audioConfiguration)
             ->Startup()) {                                  // B.3.7 bridge
        g_startupFailAt = 8;
        return 0;
    }

    // --- 0x1800dbd51: Input::Configuration ----------------------------------
    void* inputConfiguration = MemAlloc(0x10, 4);
    if (inputConfiguration)
        RT_EXPORT_ONCE(CtorFn, "??0Configuration@Input@@QEAA@XZ")(inputConfiguration);
    _inputConfiguration = inputConfiguration;
    if (!reinterpret_cast< ::Input::Configuration*>(inputConfiguration)
             ->Startup()) {                                  // B.3.7 bridge
        g_startupFailAt = 9;
        return 0;
    }

    // --- 0x1800dbd94: the launcher dialog ------------------------------------
    // `PlatformConfigure(this, true, this->_platformHandle)`. On refusal retail
    // sets `?_hideDisplay@Error@System@@0_NA` (the player pressed Quit, so no
    // error box) and returns 0. CLAUDE.md's "Launcher Dialog" section and
    // standalone/main.cpp's HarnessGuardUnattendedBoot both depend on this
    // branch being reached exactly when `_noConfigure` is false; it is.
    if (!_noConfigure) {
        // ENDGAME B.3.7: a member call (was the name door).
        if (!PlatformConfigure(true, _platformHandle)) {
            System::Error::_hideDisplay = true;   // class static (B.3.9 SYS)
            g_startupFailAt = 10;
            return 0;
        }
    }

    // --- 0x1800dbdc0: Video::Device::PlatformStartup ------------------------
    if (!reinterpret_cast<Video::Device*>(_videoDevice)
             ->PlatformStartup()) {                          // B.3.5 bridge
        g_startupFailAt = 11;
        return 0;
    }

    // --- 0x1800dbdd4: Audio::Device -----------------------------------------
    void* audioDevice = MemAlloc(0x1298, 4);
    if (audioDevice)
        RT_EXPORT_ONCE(CtorFn, "??0Device@Audio@@QEAA@XZ")(audioDevice);
    _audioDevice = audioDevice;
    if (!reinterpret_cast<Audio::Device*>(audioDevice)
             ->PlatformStartup()) {                          // B.3.7 bridge
        g_startupFailAt = 12;
        return 0;
    }

    // --- 0x1800dbe0b: Input::Device -----------------------------------------
    void* inputDevice = MemAlloc(0x1200, 4);
    if (inputDevice)
        RT_EXPORT_ONCE(CtorFn, "??0Device@Input@@QEAA@XZ")(inputDevice);
    _inputDevice = inputDevice;
    if (!reinterpret_cast< ::Input::Device*>(inputDevice)
             ->PlatformStartup()) {                          // B.3.7 bridge
        g_startupFailAt = 13;
        return 0;
    }

    // --- 0x1800dbe42: Achievement::AchievementManager (no Startup) ----------
    void* achievementManager = MemAlloc(0x48, 4);
    if (achievementManager)
        RtAchievement_AchievementManager_Ctor(achievementManager);
    _achievementManager = achievementManager;

    // --- 0x1800dbe96 -------------------------------------------------------
    _startupComplete = 1;


    return 1;
}

extern "C" unsigned RtEngine_Engine_Startup(EngineT* self, const char* cmdLine,
                                            unsigned minRangeMB, unsigned maxTotalMB,
                                            bool noConfigure) {
    return self->Engine::Engine::Startup(
        cmdLine, minRangeMB, maxTotalMB, noConfigure);
}

// ============================================================================
// ?Shutdown@Engine@1@QEAAXXZ                               retail 0x1800db4f0
// ============================================================================
// *** THIS BODY HAS calls=0 AND IS NOT VERIFIED. READ §4 OF THE BANNER. ***
// game_main.cpp resolves the name only to satisfy its own null guard and then
// runs its own `ShutdownEngineStaged()` instead. Nothing in this project calls
// the real one, so no run this wave can exercise a single line below. It is
// ported because a null here is what returned 2 out of `RunGameMain` on cutover
// attempts 04-06, and because a written body makes the remaining gap legible.
//
// The order is retail's and it is not arbitrary: EVERY subsystem is asked to
// SAVE its state (phase 1) before ANY of them is destroyed (phase 2). Interleave
// them and Video::Configuration::Save would run after the Video::Device that
// owns the numbers it persists is gone.
//
// Ten of retail's calls here are unexported inlined destructors. Each was
// matched to the exported destructor that consists of exactly those calls —
// e.g. `??1Factory@Object@@` (0x1800071a0) is `JMP 0x180008110` and nothing
// else, `??1AchievementManager@Achievement@@` (0x18009eab0) is the null store
// at 0x1801fc670 plus the same three calls in the same order. Using the
// exported name is byte-equivalent AND port-aware; using the raw address would
// be dead memory at cutover. The one place that was NOT reducible to an export
// is flagged inline.
void Engine::Engine::Shutdown() {

    ++g_shutdownRuns;
    unsigned steps = 0;

    // ---- phase 1: persist ---------------------------------------------------

    // 0x1800db50f. The launcher-dialog arming bit: byte 0 of Video::Configuration
    // becomes `_startupComplete == 0`, and Video::Configuration::Save (below)
    // writes it to the registry. Engine.h documents the whole chain.
    if (void* videoCfg = _videoConfiguration)
        *static_cast<uint8_t*>(videoCfg) = (_startupComplete == 0) ? 1 : 0;

    if (void* p = _achievementManager) {
        reinterpret_cast< ::Achievement::AchievementManager*>(p)->RemoveAll(); ++steps;  // B.3.7 bridge
    }
    if (void* p = _inputDevice) {
        reinterpret_cast< ::Input::Device*>(p)->PlatformShutdown(); ++steps;  // B.3.7 bridge
    }
    if (void* p = _inputConfiguration) {
        reinterpret_cast< ::Input::Configuration*>(p)->Save();  // B.3.7 bridge
        // 0x1800db55d: destroy the object hanging off the config's first field.
        // 0x180053440 is an unnamed scalar deleting destructor (a two-node BST
        // teardown plus MemFree) with NO export in either build, so it can only
        // be reached by address. ENDGAME B.3.7 callers (K5): that address door
        // resolved (registry-first) to `RtInput_KeyMap_DeletingDtor`, ported in
        // runtime/src/input/keymap_save.cpp and registered there BY ADDRESS, so
        // it is a direct call now — the same conversion block B5 made in
        // Input::Configuration::Startup's re-entry arm.
        if (void* inner = *static_cast<void**>(p)) {
            RtInput_KeyMap_DeletingDtor(inner);
        }
        ++steps;
    }
    if (void* p = _audioDevice) {
        // 0x1800db576: two virtual calls on objects the device owns, each
        // followed by an unconditional null store. Slot 0x90 on the first,
        // slot 0x10 on the second.
        uint8_t* dev = static_cast<uint8_t*>(p);
        if (void* a = *reinterpret_cast<void**>(dev + 0xa90)) {
            void** vt = *reinterpret_cast<void***>(a);
            reinterpret_cast<VoidThisFn>(vt[0x90 / 8])(a);
        }
        *reinterpret_cast<void**>(dev + 0xa90) = nullptr;
        if (void* b = *reinterpret_cast<void**>(dev + 0xa88)) {
            void** vt = *reinterpret_cast<void***>(b);
            reinterpret_cast<VoidThisFn>(vt[0x10 / 8])(b);
        }
        *reinterpret_cast<void**>(dev + 0xa88) = nullptr;
        ++steps;
    }
    if (void* p = _audioConfiguration) {
        reinterpret_cast<Audio::Configuration*>(p)->Save(); ++steps;   // B.3.7 bridge
    }
    if (void* p = _videoDevice) {
        reinterpret_cast<Video::Device*>(p)->PlatformShutdown();  // B.3.5 bridge
        // 0x1800db5cd: three 0x18-byte plugin slots at device+0xb0; FreeLibrary
        // each non-null module and clear all three qwords of every slot,
        // whether or not it held one.
        uint8_t* slot = static_cast<uint8_t*>(p) + 0xb0;
        for (int i = 0; i < 3; ++i, slot += 0x18) {
            if (void* mod = *reinterpret_cast<void**>(slot))
                RtEngineLifecyclePlatformFreeLibrary(mod);
            *reinterpret_cast<void**>(slot + 0x00) = nullptr;
            *reinterpret_cast<void**>(slot + 0x08) = nullptr;
            *reinterpret_cast<void**>(slot + 0x10) = nullptr;
        }
        ++steps;
    }
    if (void* p = _videoConfiguration) {
        reinterpret_cast<Video::Configuration*>(p)->Save(); ++steps;  // B.3.5 bridge
    }
    if (void* p = _application) {
        static_cast< ::Application::Application*>(p)->PlatformShutdown(); ++steps;  // ENDGAME B.3.7: member call
    }
    if (_factory != nullptr) {
        // 0x1800db62d: FreeLibrary on a module held in an UNEXPORTED global at
        // 0x1801fc6d0 — the only address in these four bodies with no name in
        // either build. At cutover ResolveAddr hands back 0 for it (the module
        // is not mapped) and this step is skipped. That is a genuine
        // standalone-vs-retail difference, not a defensive guard: retail always
        // has its own .data and we, with no DLL, have no such variable at all.
        if (uintptr_t g = ResolveAddr(Module::Runtime, 0x1801fc6d0)) {
            void** slot = reinterpret_cast<void**>(g);
            if (*slot) RtEngineLifecyclePlatformFreeLibrary(*slot);
            *slot = nullptr;
        }
        ++steps;
    }
    if (void* p = _modification) {
        RT_EXPORT_ONCE(VoidThisFn, "?SaveList@Modification@IO@@QEAAXXZ")(p); ++steps;
    }
    if (_persistence != nullptr && _noConfigure == 0) {
        RT_EXPORT_ONCE(VoidThisFn,
            "?WriteSettings@Persistence@IO@@QEBAXXZ")(_persistence); ++steps;
    }
    if (void* p = _fileSystem) {
        RT_EXPORT_ONCE(VoidThisFn, "?ClosePackages@FileSystem@IO@@QEAAXXZ")(p); ++steps;
    }

    // ---- phase 2: destroy and free -----------------------------------------
    // Reverse-ish of construction, and NOT the exact reverse — retail frees
    // Achievement, Input::Device, Input::Configuration, Audio::Device,
    // Audio::Configuration, Video::Device, Video::Configuration, Application,
    // System::Configuration, Factory, InternalFactory, ExternalFactory,
    // Modification, Persistence, FileSystem, in that order. Kept as written.

    if (void* p = _achievementManager) {
        // ~AchievementManager is a REAL non-virtual member (the null store at
        // 0x1801fc670, then the three Array dtors); the door was its export.
        reinterpret_cast< ::Achievement::AchievementManager*>(p)
            ->Achievement::AchievementManager::~AchievementManager();  // B.3.7 bridge
        MemFree(p);
    }
    if (void* p = _inputDevice) {
        // 0x1800db6c4: retail calls NO destructor here — just the free. Input::
        // Device's dtor exists (??1Device@Input@@, 0x1800e1480) and Shutdown
        // does not call it. Faithful, and worth flagging as a retail leak
        // rather than a transcription slip.
        MemFree(p);
    }
    if (void* p = _inputConfiguration) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Configuration@Input@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _audioDevice) {
        // 0x1800db6fe: only the Audio::Stream sub-object at +0xa00 is destroyed;
        // ??1Device@Audio@@ (0x1800de4f0) is NOT called. Faithful.
        // ENDGAME B.3.9 ENG W2: the class's real (public, QEAA) ~Stream.
        reinterpret_cast< ::Audio::Stream*>(static_cast<uint8_t*>(p) + 0xa00)
            ->~Stream();                                         // B.3.9 bridge
        MemFree(p);
    }
    if (void* p = _audioConfiguration) {
        // ~Configuration is a REAL non-virtual member (audio.cpp: the null store
        // to _singleton); ??1Configuration@Audio@@ is its door. B.3.7 close.
        reinterpret_cast< ::Audio::Configuration*>(p)
            ->Audio::Configuration::~Configuration();  // B.3.7 bridge
        MemFree(p);
    }
    if (void* p = _videoDevice) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Device@Video@@QEAA@XZ")(p);
        RtVideo_Device_OperatorDelete(p);     // ??3Device@Video@@, == MemFree
    }
    if (void* p = _videoConfiguration) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Configuration@Video@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _application) {
        // 0x18004da00 is ~Application immediately followed by MemFree(this) —
        // a scalar deleting destructor. Split into its two halves here. The
        // exported ??1Application@0@ writes the two adjacent zeroes in the
        // opposite order to the inlined copy; both are zero stores to fields
        // nothing reads between them.
        RT_EXPORT_ONCE(VoidThisFn, "??1Application@0@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _configuration) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Configuration@System@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _factory) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Factory@Object@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _internalFactory) {
        RT_EXPORT_ONCE(VoidThisFn, "??1InternalFactory@Object@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _externalFactory) {
        RT_EXPORT_ONCE(VoidThisFn, "??1ExternalFactory@Object@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _modification) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Modification@IO@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _persistence) {
        RT_EXPORT_ONCE(VoidThisFn, "??1Persistence@IO@@QEAA@XZ")(p);
        MemFree(p);
    }
    if (void* p = _fileSystem) {
        RT_EXPORT_ONCE(VoidThisFn, "??1FileSystem@IO@@QEAA@XZ")(p);
        MemFree(p);
    }

    // 0x1800db877: release every arena range. NOTE it does NOT null
    // `_allocator.ranges` — allocator.cpp's Shutdown releases the table and
    // leaves the pointer dangling, exactly as retail does, so the shadow below
    // must not assert on it.
    RT_EXPORT_ONCE(VoidThisFn, "?Shutdown@MemoryAllocator@System@@QEAAXXZ")(
        &_allocator);

    // 0x1800db88b: the tail jump.
    RtEngineLifecyclePlatformCoUninit();

    g_shutdownSteps = steps;

}

extern "C" void RtEngine_Engine_Shutdown(EngineT* self) {
    self->Engine::Engine::Shutdown();
}

// ============================================================================
// Summary — the effectiveness control
// ============================================================================
// Each of the four runs at most once per process, so `divergences=0` from the
// diff harness says almost nothing on its own: what matters is whether the body
// executed at all. Printed beside .
extern "C" void RtEngineLifecycleSummary() {
    if (g_ctorRuns == 0 && g_startupRuns == 0 &&
        g_dtorRuns == 0 && g_shutdownRuns == 0) return;
    printf("[RtEngineLife] ctor=%u startup=%u (failAt=%u) dtor=%u "
                "shutdown=%u (phase1 steps=%u)\n",
                g_ctorRuns, g_startupRuns, g_startupFailAt, g_dtorRuns,
                g_shutdownRuns, g_shutdownSteps);
    if (g_startupRuns && g_startupFailAt)
        printf("[RtEngineLife] Engine::Startup FAILED at step %u -- the engine "
                    "did not start; see the [RtUnresolved] lines above for the "
                    "symbol, if any, that was missing.\n", g_startupFailAt);
    if (g_shutdownRuns == 0)
        printf("[RtEngineLife] Engine::Shutdown ran 0 times -- STRUCTURAL, not a "
                    "regression: game_main.cpp calls ShutdownEngineStaged() instead. "
                    "That port is UNVERIFIED.\n");
    std::fflush(stdout);
}

// ============================================================================
// Registration
// ============================================================================
// Both the exported NAME and the retail ADDRESS, for the same reason lane B
// gives: registering only the name leaves ResolveAddr(Module::Runtime, ...)
// callers on the DLL.
//
// Every mangled name below is ONE string literal on ONE line, so
// tools/runtime_census.py and tools/export_name_audit.py can see it
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtEngineLifecycleRegister() {
    // The allocator platform seam must exist BEFORE Engine::Engine runs:
    // `??0MemoryAllocator@System@@` routes to rtalloc::Construct, whose first
    // call is g_plat.lockInit. On a run with RT_ALLOC unset nobody
    // else fills that table, and a null there is an EXEC of 0 on the very first
    // line of the boot. Refuse loudly rather than register a landmine — the
    // named-refusal shape lane E4 and lane H both settled on.
    if (!RtLaneXEnsureAllocatorPlatform()) {
        printf("[RtPort] Engine lifecycle: REFUSING to register -- "
                    "RtSystemAllocatorInstallPlatform() left g_plat.lockInit null, so "
                    "the MemoryAllocator constructor Engine::Engine calls cannot run. "
                    "See runtime/src/system/allocator_win.cpp\n");
        return 0;
    }

    int n = 0;

    RegisterRuntimeExport("??0Engine@0@QEAA@PEAX@Z", (void*)&RtEngine_Engine_Ctor); ++n;
    RegisterRuntimeAddr(0x1800dbf00, (void*)&RtEngine_Engine_Ctor);

    RegisterRuntimeExport("??1Engine@0@QEAA@XZ", (void*)&RtEngine_Engine_Dtor); ++n;
    RegisterRuntimeAddr(0x1800dbec0, (void*)&RtEngine_Engine_Dtor);

    RegisterRuntimeExport("?Startup@Engine@1@QEAAIPEBDII_N@Z", (void*)&RtEngine_Engine_Startup); ++n;
    RegisterRuntimeAddr(0x1800db8a0, (void*)&RtEngine_Engine_Startup);

    RegisterRuntimeExport("?Shutdown@Engine@1@QEAAXXZ", (void*)&RtEngine_Engine_Shutdown); ++n;
    RegisterRuntimeAddr(0x1800db4f0, (void*)&RtEngine_Engine_Shutdown);

    // The four helpers. The first two are lane X's #15 and #16 — inlined at
    // their only call site, which is why lane X's fourteen missed them.
    RegisterRuntimeExport("??0Configuration@System@@QEAA@XZ", (void*)&RtSystem_Configuration_Ctor); ++n;
    RegisterRuntimeAddr(0x1800c51b0, (void*)&RtSystem_Configuration_Ctor);

    RegisterRuntimeExport("??0AchievementManager@Achievement@@QEAA@XZ", (void*)&RtAchievement_AchievementManager_Ctor); ++n;
    RegisterRuntimeAddr(0x18009eb00, (void*)&RtAchievement_AchievementManager_Ctor);

    RegisterRuntimeExport("??2Device@Video@@SAPEAX_K@Z", (void*)&RtVideo_Device_OperatorNew); ++n;
    RegisterRuntimeAddr(0x18000c9b0, (void*)&RtVideo_Device_OperatorNew);

    RegisterRuntimeExport("??3Device@Video@@SAXPEAX@Z", (void*)&RtVideo_Device_OperatorDelete); ++n;
    RegisterRuntimeAddr(0x18000ca30, (void*)&RtVideo_Device_OperatorDelete);

    // NOT registered, deliberately: the two unexported per-class operator
    // new/delete bodies at 0x180011640 and 0x180006a10. They are MemAlloc(n,4)
    // and MemFree verbatim, and this lane calls those directly instead. They
    // have dozens of DLL-internal callers and no name, so registering the
    // addresses would change what ~20 other call sites in this tree resolve --
    // that is lane Q's kind of decision, not this lane's.
    return n;
}

// ?_singleton@Engine@1@0PEAV11@EA (0x1801fe748) -- the class static (ENDGAME B.3.9 ENG, B39_RULES D7);
// RtEnginePortInit redirects the minted storage row onto it.
::Engine::Engine* ::Engine::Engine::_singleton = nullptr;
