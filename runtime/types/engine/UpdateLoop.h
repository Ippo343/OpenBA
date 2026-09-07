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
struct Loader {
    System::Thread _thread;   // +0x00 .. +0x2f  (vtable, runFlags, 4 HANDLEs)
    void*          _loop;     // +0x30  UpdateLoop* — the loop being loaded
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
