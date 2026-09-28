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
// the very top of `Engine::Shutdown`, and `Video::Configuration::Save`
// (0x1800f6d10, called later in the same function) persists it. Neither the
// -- CORRECTED by Wave 23 lane ENGINE: this comment previously named
// 0x1800f6d10 `Video::Configuration::Shutdown`. pefile against the shipped
// export directory says it is `?Save@Configuration@Video@@QEAAXXZ`, and
// `Engine::PlatformConfigure` calls the same address twice (0x1800a1798,
// 0x1800a17ee) on the persist path. The wrong name mattered because it made
// the arming bit look like it was written by a teardown rather than by an
// ordinary save that also runs mid-boot. --
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
#include <type_traits>
#include "../common.h"
#include "../system/MemoryAllocator.h"

// (The `RtEngine::EngineAddr` address constants -- the allocator singleton
// 0x1801fc718, the Engine ctor / dtor / Startup / Shutdown / Get / accessor /
// Platform* exports and the allocator's ctor / AddNewMemoryRange -- had no
// reader and went at B3R W0-SM M1; each address is on its declaration below.
// The Engine ctor calls the allocator's ctor 0x1800c79a0 on `this`, and
// Engine::Startup calls AddNewMemoryRange 0x1800c7690 after seeding
// +0x10/+0x18/+0x668. `RtEngine` stays the namespace of the aliases at the
// bottom: engine/types defines GLOBAL `struct MainLoop` / `struct UpdateLoop`
// and is on the include path ahead of runtime/types.)

// ############################################################################
// Engine::Engine -- THE REAL CLASS (ENDGAME B.3 step 7, block B1)
// ############################################################################
//
// NON-POLYMORPHIC (proved above: the ctor's first store is a zero, and +0x000
// is the embedded MemoryAllocator). The Wave 8 POD was field-identical and is
// held only through pointers inside the runtime tree, so the POD BECOMES the
// class (B7-6) and `RtEngine::Engine` is an alias of it.
//
// SIZE 0x6f8 IS NOW PINNED, NOT INFERRED (closes B37_RULES sec. 13 item 3).
// `??4Engine@0@QEAAAEAV00@AEBV00@@Z` (0x18000c7d0) is a flat copy of the whole
// object: its aligned path moves 13 x 0x80 + 0x78 = 0x6f8 bytes with MOVAPS and
// its unaligned path is `MOV R8D,0x6f8; CALL memcpy` (0x18000c898). Independent
// bound: the exe's GameMain (0x140041ef0) holds the Engine BY VALUE at
// [RSP+0x60] in a 0x760-byte frame whose next locals are its home slots, so the
// object cannot exceed 0x700.
//
// HELD BY VALUE, BUILT THROUGH THE DOOR (B5-4): the exe keeps it on GameMain's
// stack and calls the imported `??0Engine@0@QEAA@PEAX@Z`. The tree's GameMain
// (code/Game/Game/game_main.cpp) holds raw storage and calls the same door, so
// the class is CONCRETE with a PROTECTED defaulted constructor (not retail's:
// that stays the registered DOOR `RtEngine_Engine_Ctor`, engine_lifecycle.cpp).
//
// `?_singleton@Engine@1@0PEAV11@EA` (0x1801fe748) is the class static below; its
// minted row is RETIRED (ENDGAME B.3.9 ENG W2). The subsystem fields keep the POD's `void*`
// spelling; the sixteen exported accessors below give each its retail type.
// ############################################################################

namespace System      { class Configuration; class Time; }
namespace IO          { class FileSystem; class Persistence; class Modification; }
namespace Object      { class Factory; class InternalFactory; class ExternalFactory; }
namespace Video       { class Configuration; class Device; }
namespace Audio       { class Configuration; class Device; }
namespace Input       { class Configuration; class Device; }
namespace Achievement { class AchievementManager; }
namespace Application { class Application; }

namespace Engine {

class Engine {
public:
    // ---- lifetime: DOORS (engine_lifecycle.cpp), declared, undefined -------
    explicit Engine(void* platformHandle);                        // 0x1800dbf00  door RtEngine_Engine_Ctor
    ~Engine();                                                    // 0x1800dbec0  door RtEngine_Engine_Dtor

    // ---- PORTED --------------------------------------------------------------
    uint32_t Startup(const char* commandLine, uint32_t minRangeMB,
                     uint32_t maxTotalMB, bool noConfigure);      // 0x1800db8a0  engine_lifecycle.cpp
    void Shutdown();                                              // 0x1800db4f0  engine_lifecycle.cpp
    bool PlatformConfigure(bool configure, void* platformHandle); // 0x1800a1670  engine_platform.cpp
    bool PlatformStartup();                                       // 0x1800a2100  application/platform_startup_win.cpp

    // ---- the sixteen exported accessors 0x18000c8b0..0x18000c9a0: each is
    // `MOV RAX,[RCX+off]; RET` (GetMemoryAllocator is `MOV RAX,RCX`).
    // Faithful inline bodies, NOT REGISTERED (nothing resolves them).
    rt::system::MemoryAllocator* GetMemoryAllocator() { return &_allocator; }  // 0x18000c8b0
    IO::FileSystem*   GetFileSystem()   { return static_cast<IO::FileSystem*>(_fileSystem); }    // 0x18000c8c0
    IO::Persistence*  GetPersistence()  { return static_cast<IO::Persistence*>(_persistence); }  // 0x18000c8d0
    IO::Modification* GetModification() { return static_cast<IO::Modification*>(_modification); } // 0x18000c8e0
    System::Configuration* GetConfiguration() { return static_cast<System::Configuration*>(_configuration); } // 0x18000c8f0
    ::Application::Application* GetApplication() { return static_cast< ::Application::Application*>(_application); } // 0x18000c900
    Object::InternalFactory* GetInternalFactory() { return static_cast<Object::InternalFactory*>(_internalFactory); } // 0x18000c910
    Object::Factory*  GetFactory()      { return static_cast<Object::Factory*>(_factory); }        // 0x18000c920
    Object::ExternalFactory* GetExternalFactory() { return static_cast<Object::ExternalFactory*>(_externalFactory); } // 0x18000c930
    Video::Configuration* GetVideoConfiguration() { return static_cast<Video::Configuration*>(_videoConfiguration); } // 0x18000c940
    Video::Device*    GetVideoDevice()  { return static_cast<Video::Device*>(_videoDevice); }      // 0x18000c950
    Audio::Configuration* GetAudioConfiguration() { return static_cast<Audio::Configuration*>(_audioConfiguration); } // 0x18000c960
    Audio::Device*    GetAudioDevice()  { return static_cast<Audio::Device*>(_audioDevice); }      // 0x18000c970
    Input::Configuration* GetInputConfiguration() { return static_cast<Input::Configuration*>(_inputConfiguration); } // 0x18000c980
    Input::Device*    GetInputDevice()  { return static_cast<Input::Device*>(_inputDevice); }      // 0x18000c990
    Achievement::AchievementManager* GetAchievementManager() { return static_cast<Achievement::AchievementManager*>(_achievementManager); } // 0x18000c9a0

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    static Engine* Get();                                         // 0x18000c7c0
    // ?_singleton@Engine@1@0PEAV11@EA (0x1801fe748) — the class static (ENDGAME B.3.9 ENG, B39_RULES D7).
    // Its minted storage row is RETIRED (ENDGAME B.3.9 ENG W2: no reader was
    // left but the redirect). PUBLIC: foreign files read it directly.
    static Engine* _singleton;
    bool Update(const System::Time& time);                        // 0x1800db4a0
    void NotifyBeginLoad();                                       // 0x1800db480
    void NotifyEndLoad();                                         // 0x1800db460
    void PlatformUpdate();                                        // 0x1800a20e0
    void PlatformShutdown();                                      // 0x1800a20f0
    Engine& operator=(const Engine& other);                       // 0x18000c7d0  (the 0x6f8 copy)

protected:
    // RULE B5-4: NOT retail's ctor (that is the door). The exe's GameMain holds
    // the object by value in raw storage and constructs it through the door.
    Engine() = default;

public:
    // ---- the 0x6f8 bytes, evidence per field in the table at the top -------
    // The allocator subobject. NOT a pointer: `GetMemoryAllocator` returns
    // `this`, so `&engine->_allocator == (void*)engine`.
    rt::system::MemoryAllocator _allocator;  // +0x000 .. +0x66f

    void*   _fileSystem;                     // +0x670  IO::FileSystem*
    void*   _persistence;                    // +0x678  IO::Persistence*
    void*   _modification;                   // +0x680  IO::Modification*
    void*   _configuration;                  // +0x688  System::Configuration*
    void*   _application;                    // +0x690  Application::Application*
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
    // +0x6f2: never written by any retail Engine function; inside the 0x6f8
    // that operator= copies.
    uint8_t _pad6f2[6];                      // +0x6f2
};

static_assert(sizeof(Engine) == 0x6f8, "Engine is 0x6f8 bytes (??4Engine@0@ 0x18000c7d0 copies 0x6f8)");
static_assert(alignof(Engine) == 8, "qword-aligned");
static_assert(!std::is_polymorphic<Engine>::value,
              "Engine::Engine has no vtable: +0x000 is the MemoryAllocator");
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

}  // namespace Engine

// B7-6: one definition per layout.
namespace RtEngine { using Engine = ::Engine::Engine; }
