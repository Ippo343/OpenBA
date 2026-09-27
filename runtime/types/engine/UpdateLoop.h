#pragma once
// ============================================================================
// runtime/types/engine/UpdateLoop.h — the Engine loop family's binary layouts
// ============================================================================
//
// Wave 8, lane A. Every offset below is pinned by a named function in the
// SHIPPED Runtime-x64.dll (image base 0x180000000), read from the DISASSEMBLY
// rather than only from Ghidra's C (reference_ghidra_c_output_vs_disassembly.md).
// Where an exported one-line accessor pins a field, that accessor is cited —
// it gives the offset AND the engine's own name for the field.
//
// The family is one single-inheritance chain plus one Thread subclass:
//
//     Engine::UpdateLoop            0x28   ??_7UpdateLoop@Engine@@6B@        0x180155ba0
//       Engine::LoaderLoop          0x48   ??_7LoaderLoop@Engine@@6B@        0x180157940
//         Engine::DialogLoaderLoopT 0x188  ??_7DialogLoaderLoopT@Engine@@6B@ 0x1801578d0
//       Engine::DialogLoopT         0x168  ??_7DialogLoopT@Engine@@6B@       0x1801579a0
//
//     System::Thread                0x30
//       Engine::Loader              0x38   ??_7Loader@Engine@@6B@            0x180157a08
//
// ALL FIVE VTABLES ARE EXPORTED DATA SYMBOLS, and each address above is the
// address the class's own constructor stamps (`LEA RAX,[…] / MOV [RCX],RAX`),
// verified instruction by instruction — not inferred from the export existing.
// That is Wave 6's `??_7Geometry` lever: a ported constructor RESOLVES the
// exported symbol and stamps it, and never mints a table of its own, so an
// object our ctor built and one the DLL's ctor built dispatch identically.
//
// The slot counts below came out of the binary, by reading the vtable region
// 0x1801578d0..0x180157a18 as qwords — not from a base class's count
// (reference_vtable_slot_count_from_the_name_string.md: a table sized to the
// base mis-dispatches every derived virtual, and in the tutorial wave that made
// slot 8 call the constructor and auto-pass 61 goals).
//
//   UpdateLoop        12 slots (0x00..0x58)
//   LoaderLoop        12 slots, no padding before DialogLoopT's table
//   DialogLoopT       13 slots — slot 12 (+0x60) CreateDialog, `_purecall`
//   DialogLoaderLoopT 13 slots — slot 12 (+0x60) CreateDialog, `_purecall`,
//                     then one qword of zero padding before LoaderLoop's table
//   Loader             2 slots — slot 0 Run (0x1800dbfc0), slot 1 0x18000c590
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1). Engine::Loader's two
// event HANDLEs are inherited from System::Thread, which already spells them
// `void*` for exactly this reason.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../system/Thread.h"
#include "../system/String.h"
#include "../ui/UserInterface.h"

// DELIBERATELY NOT INCLUDED: ../system/Value.h. Nothing here needs it — `Time`
// appears in this file only inside comments, and `LoaderLoop::_delayTicks` is
// declared as the int64 it is. Pulling it in would drag a GLOBAL `struct Time`
// (Value.h:65) into every consumer, which collides outright with the exe tree's
// own global `struct Time` (engine/types/common.h:12): a translation unit that
// included both this header and anything from engine/types/ would fail with
// `redefinition of 'struct Time'`. That duplicate is pre-existing and belongs to
// whoever owns Value.h; this header simply declines to be the thing that carries
// it into the exe tree. Files that genuinely need `Time` — such as
// runtime/src/engine/loops.cpp — include Value.h themselves.

namespace RtEngine {

// ============================================================================
// Engine::UpdateLoop — 0x28 bytes
// ============================================================================
// Pinned by ??0UpdateLoop@Engine@@QEAA@XZ (0x1800ddd30), which writes every one
// of the 0x28 bytes, and by the two exported one-line accessors:
//
//   ?GetLoadBit@UpdateLoop@Engine@@QEBAHXZ      0x1800317c0  MOV EAX,[RCX+0x20]
//   ?GetLoadProgress@UpdateLoop@Engine@@QEBAHXZ 0x1800317b0  MOV EAX,[RCX+0x24]
//
// `_loadBit` is NOT a flag word: the ctor allocates the LOWEST CLEAR BIT of
// Object::ExternalFactory's 32-bit allocation mask (the dword at factory +0x04)
// and stores that bit's INDEX here, or -1 when all 32 are taken. ~UpdateLoop
// hands the index back by writing it into ExternalFactory::_cleanBitIndex
// (+0x00), calling ExternalFactory::Clean(), and clearing the bit again. So an
// UpdateLoop owns one of 32 resource-lifetime slots for as long as it lives.
struct UpdateLoop {
    void*    _vtable;         // +0x00  ??_7UpdateLoop@Engine@@6B@ (0x180155ba0)
    void*    _resourceList;   // +0x08  Object::External* — the ExternalList
                              //        LoadResources (0x1800dd7d0) stores here
    void*    _internalListA;  // +0x10  \ the pair ~UpdateLoop tail-calls
    void*    _internalListB;  // +0x18  / 0x180010ab0 on (this + 0x10)
    int32_t  _loadBit;        // +0x20  ?GetLoadBit@ 0x1800317c0
    int32_t  _loadProgress;   // +0x24  ?GetLoadProgress@ 0x1800317b0
};
static_assert(sizeof(UpdateLoop) == 0x28, "Engine::UpdateLoop is 0x28 bytes");
static_assert(offsetof(UpdateLoop, _vtable)        == 0x00, "UpdateLoop::_vtable @ +0x00");
static_assert(offsetof(UpdateLoop, _resourceList)  == 0x08, "UpdateLoop::_resourceList @ +0x08");
static_assert(offsetof(UpdateLoop, _internalListA) == 0x10, "UpdateLoop::_internalListA @ +0x10");
static_assert(offsetof(UpdateLoop, _internalListB) == 0x18, "UpdateLoop::_internalListB @ +0x18");
static_assert(offsetof(UpdateLoop, _loadBit)       == 0x20, "UpdateLoop::_loadBit @ +0x20");
static_assert(offsetof(UpdateLoop, _loadProgress)  == 0x24, "UpdateLoop::_loadProgress @ +0x24");

// ---- the 12 base virtual slots ---------------------------------------------
// Read out of ??_7UpdateLoop@Engine@@6B@ (0x180155ba0) and confirmed against
// the three derived tables, which override the same slots in the same order.
// Slot indices, not just offsets, because the ports call two of them by index:
// Startup dispatches slot 12 (CreateDialog, derived-only) and Loader::Load
// dispatches slot 11 (AllowsModChanges).
enum : int {
    kSlot_Destroy             = 0,   // +0x00  scalar deleting dtor, fn(this, flags)
    kSlot_Startup             = 1,   // +0x08
    kSlot_Shutdown            = 2,   // +0x10
    kSlot_Update              = 3,   // +0x18  bool(this, const Time&)
    kSlot_Draw                = 4,   // +0x20  void(this, float)
    kSlot_DrawFailed          = 5,   // +0x28  base 0x180031740
    kSlot_Load                = 6,   // +0x30
    kSlot_SaveRestore         = 7,   // +0x38  base 0x180031750
    kSlot_OnVideoError        = 8,   // +0x40  base 0x180031760
    kSlot_OnVideoRestart      = 9,   // +0x48  base 0x180031770
    kSlot_GetResourceFilename = 10,  // +0x50  base 0x180031780
    kSlot_AllowsModChanges    = 11,  // +0x58  base 0x180031790
    kSlot_CreateDialog        = 12,  // +0x60  DialogLoopT / DialogLoaderLoopT only
};

// ============================================================================
// Engine::Loader — 0x38 bytes, a System::Thread subclass
// ============================================================================
// Pinned by ??0Loader@Engine@@QEAA@XZ (0x1800dc0f0):
//     System::Thread::Thread(this, 0x40000)   // 0x1800ca4e0 — 256 KB stack
//     this->_vtable = ??_7Loader@Engine@@6B@  // 0x180157a08
//     this->_loop   = nullptr                 // +0x30
//
// The only instance is the static `?_loader@LoaderLoop@Engine@@1VLoader@2@A`
// at 0x1801fe7b8 — a DATA export, not a function. See loops.cpp for why this
// port deliberately does NOT register it.
//
// ?Run@Loader@Engine@@UEAAXXZ (0x1800dbfc0) is the worker body: it runs
// UpdateLoop::LoadResources(_loop) and then _loop->vtable[6] (Load) with
// ExternalFactory::_loadEnabled held true across both.
// ENDGAME B.3.4b (rule B4b-1 / B4b-3): the member is the POD `RtSystem::Thread`,
// not the real `System::Thread` class. `RtEngine::Loader` is itself a POD mirror
// of a retail class, and B34_BRIEFS §A.3/§A.4's "a class held BY VALUE must be
// concrete" is about a REAL CLASS holding a REAL CLASS — a POD mirror holding
// the POD is not that case. `System::Thread` is ABSTRACT (retail's slot 0 is
// `_purecall`, and no base body exists to delegate to), so spelling this member
// as the class would be ill-formed. `ConstructLoader`
// (standalone/rt_data_mint.cpp:434-461) still builds this object through the
// registered `??0Thread@System@@QEAA@I@Z` door and stamps
// `??_7Loader@Engine@@6B@` over +0x00 itself; nothing about that moves.
struct Loader {
    RtSystem::Thread _thread; // +0x00 .. +0x2f  (vtable, runFlags, 4 HANDLEs)
    void*            _loop;   // +0x30  UpdateLoop* — the loop being loaded
};
static_assert(sizeof(Loader) == 0x38, "Engine::Loader is 0x38 bytes");
static_assert(offsetof(Loader, _loop) == 0x30, "Loader::_loop @ +0x30");

// ============================================================================
// Engine::LoaderLoop — 0x48 bytes
// ============================================================================
// Pinned by ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z
// (0x1800dc1e0) and by the two readers:
//
//   ?IsFinished@LoaderLoop@Engine@@QEBA_NXZ (0x1800dc150)
//       return !(Loader::_loader._thread.runFlags & 1)   // 0x1801fe7c0
//              && this->_elapsedTicks >= this->_delayTicks;
//   ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z (0x1800dc2a0)
//       Application::SleepThread(14);
//       if (this->_autoAdvance && !(runFlags & 1) && _elapsed >= _delay)
//           MainLoop::_singleton->_nextLoop = this->_nextLoop;
//       this->_elapsedTicks += time.ticks;
//       return true;
//
// NOTE THE CORRECTION at +0x40. engine/types/LoaderLoop.h called this
// `m_finished` and described it as "set to 1 by the loading thread on
// completion". It is not: the CONSTRUCTOR sets it to 1 and nothing in the
// family writes it again; Update only READS it, and the thread-completion
// signal Update actually tests is bit 0 of Loader::_loader's Thread::runFlags.
// So this byte gates whether the loop auto-advances at all, and a caller that
// wanted "has the load finished?" wants IsFinished, not this field.
struct LoaderLoop {
    UpdateLoop _base;           // +0x00 .. +0x27
    void*      _nextLoop;       // +0x28  UpdateLoop* handed to MainLoop when done
    int64_t    _elapsedTicks;   // +0x30  ctor: Timer::SecondsToTicks(0.0f)
    int64_t    _delayTicks;     // +0x38  ctor: the `Time const&` argument's ticks
    uint8_t    _autoAdvance;    // +0x40  ctor: 1
    uint8_t    _pad41[7];       // +0x41  NEVER WRITTEN by the ctor
};
static_assert(sizeof(LoaderLoop) == 0x48, "Engine::LoaderLoop is 0x48 bytes");
static_assert(offsetof(LoaderLoop, _nextLoop)     == 0x28, "LoaderLoop::_nextLoop @ +0x28");
static_assert(offsetof(LoaderLoop, _elapsedTicks) == 0x30, "LoaderLoop::_elapsedTicks @ +0x30");
static_assert(offsetof(LoaderLoop, _delayTicks)   == 0x38, "LoaderLoop::_delayTicks @ +0x38");
static_assert(offsetof(LoaderLoop, _autoAdvance)  == 0x40, "LoaderLoop::_autoAdvance @ +0x40");

// ============================================================================
// Engine::DialogLoopT — 0x168 bytes
// ============================================================================
// Pinned by ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z (0x1800db3f0),
// by ?Load@DialogLoopT@Engine@@UEAAXXZ (0x1800db140, which reads +0x28 and
// writes +0x30), by ?Startup@ (0x1800db2f0, which writes +0x38), and — for the
// embedded UI — by the exported one-liner
//
//   ?GetUserInterface@DialogLoopT@Engine@@QEAAAEAVUserInterface@UI@@XZ
//       0x18004d7c0:  LEA RAX,[RCX+0x40] ; RET
//
// which is as strong as evidence gets: the UI is AT +0x40, by name.
struct DialogLoopT {
    UpdateLoop         _base;             // +0x000 .. +0x027
    RtSystem::String   _dialogFilename;   // +0x028  ctor arg, refcount acquired
    void*              _dialogExternal;   // +0x030  Object::External* (Load)
    void*              _dialogInstance;   // +0x038  UI::DialogInstance* (Startup)
    UI::UserInterface  _ui;               // +0x040, 0x128 bytes
};
static_assert(sizeof(DialogLoopT) == 0x168, "Engine::DialogLoopT is 0x168 bytes");
static_assert(offsetof(DialogLoopT, _dialogFilename) == 0x28, "DialogLoopT::_dialogFilename @ +0x28");
static_assert(offsetof(DialogLoopT, _dialogExternal) == 0x30, "DialogLoopT::_dialogExternal @ +0x30");
static_assert(offsetof(DialogLoopT, _dialogInstance) == 0x38, "DialogLoopT::_dialogInstance @ +0x38");
static_assert(offsetof(DialogLoopT, _ui)             == 0x40, "DialogLoopT::_ui @ +0x40 (GetUserInterface 0x18004d7c0)");

// ============================================================================
// Engine::DialogLoaderLoopT — 0x188 bytes
// ============================================================================
// The same three dialog fields as DialogLoopT plus the LoaderLoop base, so
// every offset is 0x20 higher. Pinned by
// ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
// (0x1800db0d0), by ?Load@ (0x1800dad50: reads +0x48, writes +0x50) and by
// ?Startup@ (0x1800db000: writes +0x58, then pushes onto the UI's _addDialogs
// array at this+0x88 — which is +0x60 + 0x28, the UI's own _addDialogs offset,
// an independent confirmation that the UI sits at +0x60).
struct DialogLoaderLoopT {
    LoaderLoop         _base;             // +0x000 .. +0x047
    RtSystem::String   _dialogFilename;   // +0x048
    void*              _dialogExternal;   // +0x050
    void*              _dialogInstance;   // +0x058
    UI::UserInterface  _ui;               // +0x060, 0x128 bytes
};
static_assert(sizeof(DialogLoaderLoopT) == 0x188, "Engine::DialogLoaderLoopT is 0x188 bytes");
static_assert(offsetof(DialogLoaderLoopT, _dialogFilename) == 0x48, "DialogLoaderLoopT::_dialogFilename @ +0x48");
static_assert(offsetof(DialogLoaderLoopT, _dialogExternal) == 0x50, "DialogLoaderLoopT::_dialogExternal @ +0x50");
static_assert(offsetof(DialogLoaderLoopT, _dialogInstance) == 0x58, "DialogLoaderLoopT::_dialogInstance @ +0x58");
static_assert(offsetof(DialogLoaderLoopT, _ui)             == 0x60, "DialogLoaderLoopT::_ui @ +0x60");

}  // namespace RtEngine

// ############################################################################
// THE REAL CLASSES (ENDGAME B.3 step 7, block B1)
// ############################################################################
//
// `Engine::UpdateLoop`, `Engine::LoaderLoop`, `Engine::DialogLoopT`,
// `Engine::DialogLoaderLoopT` and `Engine::Loader` under retail's names. The
// `RtEngine::` PODs above STAY, field-identical and cross-asserted below
// (B37_RULES B7-6): `UI::UserInterface` names the two dialog PODs as friends,
// `tests/unit/ui_userinterface_abi_test.cpp` pins them, and loops.cpp's remaining
// name doors take them as `self` (the ctor / dtor doors that stamped vtables
// through them are gone since ENDGAME B.3.9 ENG W2). An abstract class
// cannot be the alias target of a POD that is embedded or stamped (B36_RULES
// section 10 correction). ENDGAME B.3.8 (block LOOP, B38_RULES D8): the exe-side
// mirrors are gone — engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT}.h are
// forwarders to these classes and the five exe loops are real subclasses
// (code/Game/Game/*Loop*.h) — but the PODs themselves STAY on the runtime side
// for the three readers above; they retire at the END of B.3.
//
// THE FOUR LOOPS ARE THEIR OWN ROOT (RULE B7-1). No `Object::Base`: slot 1 is
// `Startup`, not `TypeName`; there is no `_typeInfo`, no type-name literal and
// no `DEFINE_TYPE`. Slot tables, read from retail `.rdata` (B37_SLOTS.md sec. 1):
//
//   slot  UpdateLoop 0x180155ba0      LoaderLoop 0x180157940  DialogLoopT 0x1801579a0  DialogLoaderLoopT 0x1801578d0
//   [ 0]  0x180031900 deleting        0x18004d4f0 deleting    0x18004d8b0 deleting     0x18004d670 deleting
//   [ 1]  _purecall Startup           0x1800dc260             0x1800db2f0              0x1800db000
//   [ 2]  _purecall Shutdown          0x1800dc280             0x1800db2c0              0x1800dafc0
//   [ 3]  _purecall Update            0x1800dc2a0             0x1800db270              0x1800daf20
//   [ 4]  _purecall Draw              0x1800dc310             0x1800db210              0x1800dae20
//   [ 5]  0x180031740 DrawFailed      (UL)                    (UL)                     (UL)
//   [ 6]  _purecall Load              0x1800dc350             0x1800db140              0x1800dad50
//   [ 7]  0x180031750 SaveRestore     (UL)                    (UL)                     (UL)
//   [ 8]  0x180031760 OnVideoError    0x1800dc360             (UL)                     (LL) 0x1800dc360
//   [ 9]  0x180031770 OnVideoRestart  (UL)                    (UL)                     (UL)
//   [10]  0x180031780 GetResourceFilename (UL)                (UL)                     (UL)
//   [11]  0x180031790 AllowsModChanges (UL)                   (UL)                     (UL)
//   [12]  --                          --                      _purecall (new)          _purecall (new)
//
// ENDGAME B.3.9 ENG W2 (B39_RULES D1): the four minted tables, their
// `RT_DELDTOR` rows and every ctor / dtor / slot DOOR are RETIRED. The exe's
// five loops (GameUpdateLoop, MenuDialogLoopT, the splash loop, two LoaderLoop
// subclasses) are real subclasses built by their own real ctors (EXE-1), so
// every live loop wears a COMPILER table. Slot 0 is a concrete `DeletingDtor`
// with the retired row's VECTOR arms on all four; `LoaderLoop` is CONCRETE as
// in retail; `UpdateLoop` (pure 1,2,3,4,6) and the two Dialog loops (pure 12)
// stay abstract, as in retail. A call from an exe slot body to a member that
// is VIRTUAL on the base is QUALIFIED (`->Engine::DialogLoopT::Load()`), or it
// would dispatch back into itself.
//
// NO VIRTUAL DESTRUCTOR (ENDGAME B.2): GCC spends two slots on one, which would
// push `Startup` off slot 1. The destructors here are protected and
// non-virtual; every destroyable subclass declares its own `DeletingDtor`
// (B9-4).
//
// THREADS (RULE B7-7): `Loader::Run` runs on the LOADER thread and dispatches
// the loop's slot 6 `Load` there, so every `Load` body and everything it calls
// keeps no function-local first-call cache (B6-5).
// ############################################################################

namespace System { class Time; class String; }
namespace Save   { class SavedState; }
namespace UI     { class DialogInstance; }
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0), the free every
// loop `DeletingDtor` arm below makes (the retired RT_DELDTOR rows' MemFree).
// Declared, not included: runtime/types/object/Internal.h drags Value.h's
// global `struct Time` (see the banner above) into the exe tree.
namespace Object { namespace detail { void InternalMemFree(void* p); } }

namespace Engine {

// ============================================================================
// Engine::UpdateLoop -- 0x28, 12 slots, ABSTRACT (pure at 1, 2, 3, 4, 6)
// ============================================================================
class UpdateLoop {
public:
    // [0] 0x180031900 (unexported) -- MSVC's VECTOR deleting destructor, both
    // arms exactly as the retired RT_DELDTOR row ran them (DeletingDtorCore,
    // element 0x28): `flags & 2` reads the SIGNED int32 count at `this - 8`,
    // destroys the elements DOWNWARD, frees `this - 8` on `flags & 1` and
    // returns it; otherwise ~UpdateLoop, free on `flags & 1`. Every exe loop
    // declares its own slot 0 (B9-4), so this runs only for a bare base.
    virtual UpdateLoop* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<UpdateLoop*>(self + static_cast<int64_t>(i) * 0x28)->~UpdateLoop();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<UpdateLoop*>(self - 8);
        }
        this->~UpdateLoop();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual void Startup() = 0;                                    // [1] _purecall
    virtual void Shutdown() = 0;                                   // [2] _purecall
    virtual bool Update(const System::Time& time) = 0;             // [3] _purecall
    virtual void Draw(float interpolation) = 0;                    // [4] _purecall
    virtual void DrawFailed();                                     // [5] 0x180031740  RET
    virtual void Load() = 0;                                       // [6] _purecall
    virtual void SaveRestore(bool saving, Save::SavedState* state);// [7] 0x180031750  RET
    virtual void OnVideoError();                                   // [8] 0x180031760  RET
    virtual void OnVideoRestart();                                 // [9] 0x180031770  RET
    virtual const char* GetResourceFilename();                     // [10] 0x180031780 -> nullptr
    virtual bool AllowsModChanges();                               // [11] 0x180031790 -> false

    // ---- PORTED non-virtual members ----------------------------------------
    bool IsRunning() const;                                        // 0x1800dd7b0
    void LoadResources();                                          // 0x1800dd7d0  LOADER THREAD

    // ---- exported one-line accessors: faithful inline bodies, NOT REGISTERED
    // (nothing resolves them by name or address).
    void SetLoadProgress(int32_t progress) { _loadProgress = progress; }  // 0x1800317a0  MOV [RCX+0x24],EDX
    int32_t GetLoadProgress() const { return _loadProgress; }            // 0x1800317b0  MOV EAX,[RCX+0x24]
    int32_t GetLoadBit() const { return _loadBit; }                      // 0x1800317c0  MOV EAX,[RCX+0x20]

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    UpdateLoop(const UpdateLoop& other);                           // 0x1800317d0
    UpdateLoop& operator=(const UpdateLoop& other);                // 0x180031840

protected:
    // `??0UpdateLoop@Engine@@QEAA@XZ` 0x1800ddd30 — the REAL constructor
    // (ENDGAME B.3.8 block RB, RULE B8-1): the ExternalFactory clean-bit
    // allocation. The `??0` door is gone (ENDGAME B.3.9 ENG W2): every loop is
    // built by a subclass's real ctor.
    UpdateLoop() { InitFields(); }
    // `??1UpdateLoop@Engine@@UEAA@XZ` 0x1800dddb0. NON-VIRTUAL; the compiler
    // restamps on entry. Bit hand-back, ExternalFactory::Clean, the list
    // cleanup tail. Its `??1` door died with the RT_DELDTOR row (B.3.9 ENG W2).
    ~UpdateLoop() { ReleaseFields(); }

private:
    // The bodies live in loops.cpp; tests/unit/rb_engine_abi_test.cpp links
    // recording markers in their place (the unit binary links no loops.cpp).
    void InitFields();      // 0x1800ddd30 minus the vptr store (loops.cpp)
    void ReleaseFields();   // 0x1800dddb0 minus the restamp (loops.cpp)

public:
    // ---- the 0x20 bytes after the vptr, evidence in the POD above ----------
    void*   _resourceList;   // +0x08  Object::ExternalList* (LoadResources)
    void*   _internalListA;  // +0x10  \ the pair ~UpdateLoop releases through
    void*   _internalListB;  // +0x18  / 0x180010ab0
    int32_t _loadBit;        // +0x20  ?GetLoadBit@
    int32_t _loadProgress;   // +0x24  ?GetLoadProgress@
};

// ============================================================================
// Engine::Loader -- 0x38, a System::Thread subclass, 2 slots, NO deleting dtor
// ============================================================================
// `??_7Loader@Engine@@6B@` 0x180157a08: [0] 0x1800dbfc0 Run, [1] 0x18000c590
// `System::Thread::OnCompleted` (inherited). RULE B7-1.4: slot 0 is `Run`,
// there is no deleting-dtor slot, and `??1Loader@Engine@@QEAA@XZ` (0x18004d330)
// is a non-virtual `QEAA` export. The only instance is the static
// `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (0x1801fe7b8), built by
// `ConstructLoader` (standalone/rt_data_mint.cpp) through the Thread ctor door,
// never as a C++ object (B4b-2), and read through its RT_DATA_SLOT
// (carry-over 5).
class Loader : public System::Thread {
public:
    void Run() override;                                           // [0] 0x1800dbfc0  LOADER THREAD
    void Load(UpdateLoop* loop);                                   // 0x1800dc050      main thread

protected:
    // `??0Loader@Engine@@QEAA@XZ` 0x1800dc0f0 / `??1Loader@Engine@@QEAA@XZ`
    // 0x18004d330: neither is ported; the one object is built by ConstructLoader.
    Loader() = default;
    ~Loader() = default;

public:
    UpdateLoop* _loop;   // +0x30  the loop being loaded (Load's store, Run's read)
};

// ============================================================================
// Engine::LoaderLoop -- 0x48, 12 slots; CONCRETE, as in retail (B.3.9 ENG W2)
// ============================================================================
class LoaderLoop : public UpdateLoop {
public:
    // `??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z`
    // 0x1800dc1e0 — the REAL constructor (ENDGAME B.3.8 block RB, B8-1):
    // UpdateLoop() then the stamp are the compiler's. PUBLIC since the class is
    // concrete (B39_RULES D1.1); the door is gone. `Time` is by reference in
    // retail too. `System::Time` is incomplete here on purpose (the banner's
    // Value.h note); it is ONE int64 tick count at +0x00 (Value.h asserts it).
    LoaderLoop(UpdateLoop* nextLoop, const System::Time& delay) {
        InitFields(nextLoop, *reinterpret_cast<const int64_t*>(&delay));
    }

    // [0] 0x18004d4f0 -- the VECTOR deleting destructor (element 0x48), both
    // arms as the retired RT_DELDTOR row ran them; see UpdateLoop's.
    UpdateLoop* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<LoaderLoop*>(self + static_cast<int64_t>(i) * 0x48)->~LoaderLoop();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<UpdateLoop*>(self - 8);
        }
        this->~LoaderLoop();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    void Startup() override;                                       // [1] 0x1800dc260
    void Shutdown() override;                                      // [2] 0x1800dc280
    bool Update(const System::Time& time) override;                // [3] 0x1800dc2a0
    void Draw(float interpolation) override;                       // [4] 0x1800dc310
    void Load() override;                                          // [6] 0x1800dc350  RET
    void OnVideoError() override;                                  // [8] 0x1800dc360

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    void Exit();                                                   // 0x1800dc130
    bool IsFinished() const;                                       // 0x1800dc150
    LoaderLoop(const LoaderLoop& other);                           // 0x18004d3a0
    LoaderLoop& operator=(const LoaderLoop& other);                // 0x18004d430

    // `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (0x1801fe7b8, a protected
    // static in retail) STAYS a data slot until the end of B.3 (carry-over 5):
    // loops.cpp reads it through `RT_DATA_SLOT`, never as a C++ static member.

protected:
    // `??1LoaderLoop@Engine@@UEAA@XZ` 0x1800dc170: its own restamp, then
    // ~UpdateLoop folded in — both the compiler's chain. Nothing of its own.
    ~LoaderLoop() {}

private:
    void InitFields(UpdateLoop* nextLoop, int64_t delayTicks);   // loops.cpp

public:
    UpdateLoop* _nextLoop;      // +0x28  handed to MainLoop when the load is done
    int64_t     _elapsedTicks;  // +0x30  System::Time ticks
    int64_t     _delayTicks;    // +0x38  the ctor's `Time const&` argument
    uint8_t     _autoAdvance;   // +0x40  ctor: 1; never written again
    uint8_t     _pad41[7];      // +0x41  NEVER written by the ctor
};

// ============================================================================
// Engine::DialogLoopT -- 0x168, 13 slots, ABSTRACT (pure slot 12)
// ============================================================================
class DialogLoopT : public UpdateLoop {
public:
    // [0] 0x18004d8b0 -- the VECTOR deleting destructor (element 0x168), both
    // arms as the retired RT_DELDTOR row ran them; see UpdateLoop's.
    UpdateLoop* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<DialogLoopT*>(self + static_cast<int64_t>(i) * 0x168)->~DialogLoopT();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<UpdateLoop*>(self - 8);
        }
        this->~DialogLoopT();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    void Startup() override;                                       // [1] 0x1800db2f0
    void Shutdown() override;                                      // [2] 0x1800db2c0
    bool Update(const System::Time& time) override;                // [3] 0x1800db270
    void Draw(float interpolation) override;                       // [4] 0x1800db210
    void Load() override;                                          // [6] 0x1800db140
    // [12] _purecall, NEW here. No attested name (B7-1.2); the exe fills it
    // with a dialog-creating body. It returns `Object::InternalPtr<
    // UI::DialogInstance>` BY VALUE through a VIRTUAL slot, so it is declared
    // retslot-first (B6-2): `this` in RCX, the hidden return slot in RDX, the
    // slot's address back in RAX. NAMED `CreateLoopDialog`, not the tree's old
    // descriptive `kSlot_CreateDialog`: `CreateDialog` is a <windows.h>
    // function-like macro (CreateDialogA), and the exe-side stub files include
    // both headers.
    virtual Object::InternalPtr<UI::DialogInstance>* CreateLoopDialog(
        Object::InternalPtr<UI::DialogInstance>* ret) = 0;

    // ?GetUserInterface@DialogLoopT@Engine@@QEAAAEAVUserInterface@UI@@XZ
    // 0x18004d7c0: `LEA RAX,[RCX+0x40]; RET`. Faithful inline body, NOT
    // REGISTERED (nothing resolves it).
    UI::UserInterface& GetUserInterface() { return _ui; }

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    DialogLoopT(const DialogLoopT& other);                         // 0x18004d7d0

protected:
    // `??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z` 0x1800db3f0 — the
    // REAL constructor (ENDGAME B.3.8 block RB, B8-1). Retail: UpdateLoop(),
    // stamp, String copy ctor (0x180006cb0) into +0x28, two nulls, the
    // UserInterface ctor on +0x40. The String copy is the member initialiser.
    // Protected: slot 12 is `_purecall`, so the class stays abstract.
    explicit DialogLoopT(const System::String& filename) : _dialogFilename(filename) {
        InitFields();
    }
    // `??1DialogLoopT@Engine@@UEAA@XZ` 0x1800db340: restamp, ~UserInterface
    // (the body, shared with the door), then ~String on +0x28 and ~UpdateLoop
    // — which the compiler's member/base chain runs in exactly that order.
    // `_ui`'s own implicit destructor after the body is a no-op: the
    // UserInterface door zeroes every array it frees.
    ~DialogLoopT() { ReleaseFields(); }

private:
    void InitFields();      // loops.cpp
    void ReleaseFields();   // loops.cpp

public:
    System::String    _dialogFilename;  // +0x028  the ctor's argument, refcounted
    void*             _dialogExternal;  // +0x030  Object::External* (Load)
    void*             _dialogInstance;  // +0x038  InternalPtr<UI::DialogInstance> (Startup)
    UI::UserInterface _ui;              // +0x040  0x128 bytes, GetUserInterface
};

// ============================================================================
// Engine::DialogLoaderLoopT -- 0x188, 13 slots, ABSTRACT (pure slot 12)
// ============================================================================
// Inherits `LoaderLoop::OnVideoError` (slot 8, 0x1800dc360), B7-1.3.
class DialogLoaderLoopT : public LoaderLoop {
public:
    // [0] 0x18004d670 -- the VECTOR deleting destructor (element 0x188), both
    // arms as the retired RT_DELDTOR row ran them; see UpdateLoop's.
    UpdateLoop* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<DialogLoaderLoopT*>(self + static_cast<int64_t>(i) * 0x188)->~DialogLoaderLoopT();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<UpdateLoop*>(self - 8);
        }
        this->~DialogLoaderLoopT();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    void Startup() override;                                       // [1] 0x1800db000
    void Shutdown() override;                                      // [2] 0x1800dafc0
    bool Update(const System::Time& time) override;                // [3] 0x1800daf20
    void Draw(float interpolation) override;                       // [4] 0x1800dae20
    void Load() override;                                          // [6] 0x1800dad50
    // [12] _purecall, NEW here: same shape as DialogLoopT's.
    virtual Object::InternalPtr<UI::DialogInstance>* CreateLoopDialog(
        Object::InternalPtr<UI::DialogInstance>* ret) = 0;

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    DialogLoaderLoopT(const DialogLoaderLoopT& other);             // 0x18004d570

protected:
    // `??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z`
    // 0x1800db0d0 — the REAL constructor (ENDGAME B.3.8 block RB, B8-1):
    // LoaderLoop(next, delay), stamp, String copy, then the two nulls and the
    // UserInterface ctor. Protected: slot 12 is `_purecall` (abstract).
    DialogLoaderLoopT(const System::String& filename, UpdateLoop* nextLoop,
                      const System::Time& delay)
        : LoaderLoop(nextLoop, delay), _dialogFilename(filename) {
        InitFields();
    }
    // `??1DialogLoaderLoopT@Engine@@UEAA@XZ` 0x1800db060: restamp,
    // ~UserInterface (shared body), ~String, ~LoaderLoop — the compiler's
    // chain runs the last two in that order.
    ~DialogLoaderLoopT() { ReleaseFields(); }

private:
    void InitFields();      // loops.cpp
    void ReleaseFields();   // loops.cpp

public:
    System::String    _dialogFilename;  // +0x048
    void*             _dialogExternal;  // +0x050
    void*             _dialogInstance;  // +0x058
    UI::UserInterface _ui;              // +0x060
};

// ---- sizes, alignment, shape ------------------------------------------------
static_assert(sizeof(UpdateLoop) == 0x28, "Engine::UpdateLoop is 0x28 (slot 0 MOV EDX,0x28)");
static_assert(sizeof(Loader) == 0x38, "Engine::Loader is 0x38 (ctor's last store +0x30)");
static_assert(sizeof(LoaderLoop) == 0x48, "Engine::LoaderLoop is 0x48 (slot 0 MOV EDX,0x48)");
static_assert(sizeof(DialogLoopT) == 0x168, "Engine::DialogLoopT is 0x168 (slot 0 MOV EDX,0x168)");
static_assert(sizeof(DialogLoaderLoopT) == 0x188, "Engine::DialogLoaderLoopT is 0x188 (slot 0 MOV EDX,0x188)");
static_assert(alignof(UpdateLoop) == 8 && alignof(Loader) == 8 && alignof(LoaderLoop) == 8 &&
              alignof(DialogLoopT) == 8 && alignof(DialogLoaderLoopT) == 8,
              "qword-aligned (the vptr)");
static_assert(std::is_polymorphic<UpdateLoop>::value && std::is_abstract<UpdateLoop>::value,
              "UpdateLoop is abstract: retail slots 1,2,3,4,6 are _purecall");
static_assert(!std::is_abstract<LoaderLoop>::value, "LoaderLoop is concrete, as in retail (B.3.9 ENG W2)");
static_assert(std::is_abstract<DialogLoopT>::value && std::is_abstract<DialogLoaderLoopT>::value,
              "slot 12 is _purecall in retail");
static_assert(!std::is_abstract<Loader>::value && std::is_polymorphic<Loader>::value,
              "Loader overrides Thread's one pure slot (Run)");

// ---- the class/POD cross-asserts (B7-6). The vptr (+0x00) has none: the POD
// spells it as a field, the class lets the compiler own it.
#define B37_LOOP_PIN(C, f, off)                                                        \
    static_assert(offsetof(C, f) == off, "Engine::" #C "::" #f " @ " #off);           \
    static_assert(offsetof(C, f) == offsetof(RtEngine::C, f), "alias " #C "::" #f);
static_assert(sizeof(UpdateLoop) == sizeof(RtEngine::UpdateLoop), "UpdateLoop class == POD");
B37_LOOP_PIN(UpdateLoop, _resourceList,  0x08)
B37_LOOP_PIN(UpdateLoop, _internalListA, 0x10)
B37_LOOP_PIN(UpdateLoop, _internalListB, 0x18)
B37_LOOP_PIN(UpdateLoop, _loadBit,       0x20)
B37_LOOP_PIN(UpdateLoop, _loadProgress,  0x24)
static_assert(sizeof(Loader) == sizeof(RtEngine::Loader), "Loader class == POD");
B37_LOOP_PIN(Loader, _loop, 0x30)
static_assert(offsetof(Loader, _runFlags) == offsetof(RtSystem::Thread, runFlags),
              "Loader's Thread base is the POD's _thread member");
static_assert(sizeof(LoaderLoop) == sizeof(RtEngine::LoaderLoop), "LoaderLoop class == POD");
B37_LOOP_PIN(LoaderLoop, _nextLoop,     0x28)
B37_LOOP_PIN(LoaderLoop, _elapsedTicks, 0x30)
B37_LOOP_PIN(LoaderLoop, _delayTicks,   0x38)
B37_LOOP_PIN(LoaderLoop, _autoAdvance,  0x40)
static_assert(sizeof(DialogLoopT) == sizeof(RtEngine::DialogLoopT), "DialogLoopT class == POD");
B37_LOOP_PIN(DialogLoopT, _dialogFilename, 0x28)
B37_LOOP_PIN(DialogLoopT, _dialogExternal, 0x30)
B37_LOOP_PIN(DialogLoopT, _dialogInstance, 0x38)
B37_LOOP_PIN(DialogLoopT, _ui,             0x40)
static_assert(sizeof(DialogLoaderLoopT) == sizeof(RtEngine::DialogLoaderLoopT), "DialogLoaderLoopT class == POD");
B37_LOOP_PIN(DialogLoaderLoopT, _dialogFilename, 0x48)
B37_LOOP_PIN(DialogLoaderLoopT, _dialogExternal, 0x50)
B37_LOOP_PIN(DialogLoaderLoopT, _dialogInstance, 0x58)
B37_LOOP_PIN(DialogLoaderLoopT, _ui,             0x60)
#undef B37_LOOP_PIN
static_assert(sizeof(System::String) == sizeof(RtSystem::String),
              "_dialogFilename is the real String; the POD spells the same 8 bytes");

}  // namespace Engine
