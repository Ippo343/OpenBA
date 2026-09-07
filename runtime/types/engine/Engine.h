#pragma once
// ============================================================================
// runtime/types/engine/Engine.h — Engine::Engine, binary-compatible layout
// ============================================================================
//
// Wave 8 lane B. `Engine::Engine` is the process-wide root object: it owns
// every subsystem singleton, and it IS the memory allocator.
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR IS EMBEDDED AT OFFSET 0 — PROVEN, NOT INFERRED
// ---------------------------------------------------------------------------
// PLAN.md §4.3 risk 2 says so; here is the evidence, read out of the shipped
// Runtime-x64.dll rather than repeated from the plan.
//
//   1. `?GetMemoryAllocator@Engine@1@QEAAPEAVMemoryAllocator@System@@XZ`
//      (retail 0x18000c8b0, ordinal 2782) is four bytes:
//          48 8b c1   MOV RAX,RCX
//          c3         RET
//      i.e. a bare `return (MemoryAllocator*)this`. The allocator subobject
//      starts at offset 0 and is not a pointer member.
//
//   2. `??0Engine@0@QEAA@PEAX@Z` (retail 0x1800dbf00, ordinal 174) opens with
//      `CALL 0x1800c79a0` on an unmodified RCX — the MemoryAllocator ctor on
//      `this` — and only then starts zeroing at +0x670. sizeof(MemoryAllocator)
//      is 0x670 (runtime/types/system/MemoryAllocator.h), so the two abut
//      exactly with no gap.
//
//   3. `??1Engine@0@QEAA@XZ` (retail 0x1800dbec0, ordinal 586) tail-jumps to
//      the DeleteCriticalSection import with `RCX = this + 0x640`, which is
//      `MemoryAllocator::lock`. The Engine dtor destroys the allocator inline.
//
//   4. `?Startup@Engine@1@QEAAIPEBDII_N@Z` (retail 0x1800db8a0) writes THREE
//      allocator fields before anything else exists:
//          this->_allocator.minRangeSize = (uint64)arg2 << 20;   // +0x010
//          this->_allocator.maxTotalSize = (uint64)arg3 << 20;   // +0x018
//          GetSystemInfo(&si);
//          this->_allocator.pageSize     = si.dwPageSize;        // +0x668
//      and then calls the allocator's own init at 0x1800c7690.
//
// (4) is the fact that decides lane B's scope: `Engine::Startup` is not
// separable from the allocator either, so the NO-GO verdict on the allocator
// port (docs/runtime_port/WAVE2_ALLOCATOR.md) reaches all four of
// ctor/dtor/Startup/Shutdown. See docs/runtime_port/WAVE8_LANE_B.md.
//
// ---------------------------------------------------------------------------
// THERE IS NO VTABLE
// ---------------------------------------------------------------------------
// `??_7Engine@Engine@@6B@` is absent from the shipped export directory, but
// WAVE8.md §2 is right that absence is not proof (Video::Context and
// UI::UserInterface were both non-polymorphic with no exported vtable, and
// that had to be shown, not assumed). Here it is shown: the ctor's first
// store into `this` is `MOV qword ptr [RDI + 0x670], RAX` with RAX == 0 —
// a zeroed subsystem pointer, not a vtable stamp — and the bytes at +0x000
// belong to the MemoryAllocator subobject, which the ctor at 0x1800c79a0
// initialises with `ranges = nullptr` as ITS first store. Nothing writes a
// code address into offset 0 anywhere in the ctor, so `Engine::Engine` is
// NOT polymorphic and PLAN §4.3 risk 3 does not apply to it.
//
// ---------------------------------------------------------------------------
// EVERY SUBSYSTEM FIELD IS NAMED BY AN EXPORTED ONE-LINE ACCESSOR
// ---------------------------------------------------------------------------
// The 16 `Engine::Get*` exports at 0x18000c8b0..0x18000c9a0 are each a single
// `MOV RAX,[RCX+off]; RET`. That is the strongest evidence there is: the
// offset AND the field's type come straight out of the shipped export table.
// The bytes were read with read_memory over 0x18000c8b0..0x18000c9b0.
//
//   off     field                   accessor export                    retail
//   ------  ----------------------  ---------------------------------  -----------
//   0x000   _allocator (embedded)   ?GetMemoryAllocator@  (return this) 0x18000c8b0
//   0x670   _fileSystem             ?GetFileSystem@                     0x18000c8c0
//   0x678   _persistence            ?GetPersistence@                    0x18000c8d0
//   0x680   _modification           ?GetModification@                   0x18000c8e0
//   0x688   _configuration          ?GetConfiguration@                  0x18000c8f0
//   0x690   _application            ?GetApplication@                    0x18000c900
//   0x698   _platformHandle         ctor arg (RDX)                      0x1800dbf00
//   0x6a0   _factory                ?GetFactory@                        0x18000c920
//   0x6a8   _internalFactory        ?GetInternalFactory@                0x18000c910
//   0x6b0   _externalFactory        ?GetExternalFactory@                0x18000c930
//   0x6b8   _videoConfiguration     ?GetVideoConfiguration@             0x18000c940
//   0x6c0   _videoDevice            ?GetVideoDevice@                    0x18000c950
//   0x6c8   _audioConfiguration     ?GetAudioConfiguration@             0x18000c960
//   0x6d0   _audioDevice            ?GetAudioDevice@                    0x18000c970
//   0x6d8   _inputConfiguration     ?GetInputConfiguration@             0x18000c980
//   0x6e0   _inputDevice            ?GetInputDevice@                    0x18000c990
//   0x6e8   _achievementManager     ?GetAchievementManager@             0x18000c9a0
//   0x6f0   _startupComplete        Startup/Shutdown, see below         0x1800db8a0
//   0x6f1   _noConfigure            Startup 5th arg (the `bool`)        0x1800db8a0
//
// Note `_factory` (+0x6a0) and `_internalFactory` (+0x6a8) are in the OPPOSITE
// order to the accessor addresses; the table above follows the bytes.
//
// ---------------------------------------------------------------------------
// _startupComplete (+0x6f0) IS THE LAUNCHER-DIALOG ARMING BIT
// ---------------------------------------------------------------------------
// CLAUDE.md ("Launcher Dialog") describes a persisted registry flag that says
// "the previous run never finished Engine::Startup", which is what makes
// `Engine::PlatformConfigure` open the Options/Video/Play dialog, and which
// `HarnessGuardUnattendedBoot` (standalone/main.cpp:184) clears for unattended
// runs. This field is the in-memory half of it:
//
//   Startup  0x1800db8cf:  this->_startupComplete = 0        // at entry
//            0x1800dbe96:  this->_startupComplete = 1        // only on full success
//   Shutdown 0x1800db50f:  cfg = this->_videoConfiguration;
//            0x1800db51b:  if (cfg) *(bool*)cfg = (this->_startupComplete == 0);
//
// So the byte at offset 0 of `Video::Configuration` is set from this flag at
// the very top of `Engine::Shutdown`, and `Video::Configuration::Shutdown`
// (0x1800f6d10, called later in the same function) persists it. Neither the
// bit's meaning nor its timing lives in `Engine::Startup`'s own body beyond
// those two stores — which is why holding Startup and Shutdown back leaves
// the harness guard's assumptions exactly as they are today.
//
// ---------------------------------------------------------------------------
// The `Engine` singleton is NOT `Engine::_singleton` under that name.
// The ctor's last act is `MOV qword ptr [0x1801fe748], RDI` and the dtor's
// first is to null it; `?Get@Engine@1@SAPEAV11@XZ` (0x18000c7c0) reads it.
// The MemoryAllocator's own singleton is a SECOND global, 0x1801fc718, which
// the Engine dtor also nulls — every allocation site in the DLL reads
// 0x1801fc718 and adds 0x640 to reach the lock, which is only coherent
// because the two point at the same object.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../common.h"
#include "../system/MemoryAllocator.h"

// Retail addresses in Runtime-x64.dll (Ghidra space, image base 0x180000000).
// Ordinals are from the shipped export directory, read with pefile.
// Everything below lives in `RtEngine`, matching runtime/types/engine/UpdateLoop.h
// (lane A) and the RtVideo / RtObject convention the earlier waves set. This is
// NOT cosmetic: engine/types/MainLoop.h and engine/types/UpdateLoop.h define
// GLOBAL `struct MainLoop` / `struct UpdateLoop`, `engine/types` is on the
// include path AHEAD of `runtime/types`, and a global `struct Engine` here would
// be a hard redefinition in any TU that included both trees.
namespace RtEngine {

namespace EngineAddr {
    constexpr uintptr_t Singleton          = 0x1801fe748;  // Engine* (see note above)
    constexpr uintptr_t AllocatorSingleton = 0x1801fc718;  // same object, +0 view

    constexpr uintptr_t Ctor               = 0x1800dbf00;  // ord 174
    constexpr uintptr_t Dtor               = 0x1800dbec0;  // ord 586
    constexpr uintptr_t Startup            = 0x1800db8a0;  // ord 4094
    constexpr uintptr_t Shutdown           = 0x1800db4f0;  // ord 4047
    constexpr uintptr_t Get                = 0x18000c7c0;  // ord 2413
    constexpr uintptr_t GetMemoryAllocator = 0x18000c8b0;  // ord 2782, `return this`
    constexpr uintptr_t PlatformConfigure  = 0x1800a1670;  // ord 3329
    constexpr uintptr_t PlatformStartup    = 0x1800a2100;  // ord 3406
    constexpr uintptr_t PlatformShutdown   = 0x1800a20f0;  // ord 3397
    constexpr uintptr_t PlatformUpdate     = 0x1800a20e0;  // ord 3412

    // The allocator's own ctor, called by Engine's ctor on `this`.
    constexpr uintptr_t MemoryAllocatorCtor = 0x1800c79a0;
    // The allocator's init, called by Engine::Startup after seeding +0x10/+0x18/+0x668.
    constexpr uintptr_t MemoryAllocatorInit = 0x1800c7690;
}

struct Engine {
    // The allocator subobject. NOT a pointer: `GetMemoryAllocator` returns
    // `this`, so `&engine->_allocator == (void*)engine`.
    rt::system::MemoryAllocator _allocator;  // +0x000 .. +0x66f

    void*   _fileSystem;                     // +0x670  IO::FileSystem*
    void*   _persistence;                    // +0x678  IO::Persistence*
    void*   _modification;                   // +0x680  IO::Modification*
    void*   _configuration;                  // +0x688  System::Configuration*
    void*   _application;                    // +0x690  Engine::Application*
    void*   _platformHandle;                 // +0x698  ctor arg; HINSTANCE on Win32
    void*   _factory;                        // +0x6a0  Object::Factory*
    void*   _internalFactory;                // +0x6a8  Object::InternalFactory*
    void*   _externalFactory;                // +0x6b0  Object::ExternalFactory*
    void*   _videoConfiguration;             // +0x6b8  Video::Configuration*
    void*   _videoDevice;                    // +0x6c0  Video::Device*
    void*   _audioConfiguration;             // +0x6c8  Audio::Configuration*
    void*   _audioDevice;                    // +0x6d0  Audio::Device*
    void*   _inputConfiguration;             // +0x6d8  Input::Configuration*
    void*   _inputDevice;                    // +0x6e0  Input::Device*
    void*   _achievementManager;             // +0x6e8  Achievement::AchievementManager*

    // Startup clears this at entry and sets it at the very end; Shutdown
    // copies `(_startupComplete == 0)` into Video::Configuration byte 0, which
    // is the persisted launcher-dialog arming bit. See the block comment.
    uint8_t _startupComplete;                // +0x6f0
    // Startup's 5th argument (`bool`). Non-zero suppresses the
    // PlatformConfigure launcher dialog AND makes Shutdown skip the
    // Persistence shutdown at +0x678.
    uint8_t _noConfigure;                    // +0x6f1
    // +0x6f2: never written by any retail Engine function. The class size is
    // the ONE thing here that is inferred rather than read: no retail site
    // allocates an Engine (the exe's game_main.cpp does), so 0x6f8 is just
    // 0x6f2 rounded up to the struct's 8-byte alignment. Treat it as a lower
    // bound if a later wave finds a field past +0x6f1.
    uint8_t _pad6f2[6];                      // +0x6f2
};

static_assert(sizeof(Engine) == 0x6f8, "Engine is 0x6f8 bytes (inferred, see note)");
static_assert(offsetof(Engine, _allocator)          == 0x000, "Engine::_allocator");
static_assert(offsetof(Engine, _fileSystem)         == 0x670, "Engine::_fileSystem");
static_assert(offsetof(Engine, _persistence)        == 0x678, "Engine::_persistence");
static_assert(offsetof(Engine, _modification)       == 0x680, "Engine::_modification");
static_assert(offsetof(Engine, _configuration)      == 0x688, "Engine::_configuration");
static_assert(offsetof(Engine, _application)        == 0x690, "Engine::_application");
static_assert(offsetof(Engine, _platformHandle)     == 0x698, "Engine::_platformHandle");
static_assert(offsetof(Engine, _factory)            == 0x6a0, "Engine::_factory");
static_assert(offsetof(Engine, _internalFactory)    == 0x6a8, "Engine::_internalFactory");
static_assert(offsetof(Engine, _externalFactory)    == 0x6b0, "Engine::_externalFactory");
static_assert(offsetof(Engine, _videoConfiguration) == 0x6b8, "Engine::_videoConfiguration");
static_assert(offsetof(Engine, _videoDevice)        == 0x6c0, "Engine::_videoDevice");
static_assert(offsetof(Engine, _audioConfiguration) == 0x6c8, "Engine::_audioConfiguration");
static_assert(offsetof(Engine, _audioDevice)        == 0x6d0, "Engine::_audioDevice");
static_assert(offsetof(Engine, _inputConfiguration) == 0x6d8, "Engine::_inputConfiguration");
static_assert(offsetof(Engine, _inputDevice)        == 0x6e0, "Engine::_inputDevice");
static_assert(offsetof(Engine, _achievementManager) == 0x6e8, "Engine::_achievementManager");
static_assert(offsetof(Engine, _startupComplete)    == 0x6f0, "Engine::_startupComplete");
static_assert(offsetof(Engine, _noConfigure)        == 0x6f1, "Engine::_noConfigure");

}  // namespace RtEngine
