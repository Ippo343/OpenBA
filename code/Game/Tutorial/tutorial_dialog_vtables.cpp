// code/Game/Tutorial/tutorial_dialog_vtables.cpp
//
// Standalone vtables + factories for the four Tutorial dialog/controller types
// that were still generic `UI_DIALOG_STUB(...)` entries: TutorialUI,
// TutorialGoalDialog, TutorialSetupDialog, TutorialHighlightDialog.
//
// A UI_DIALOG_STUB allocates + zeroes the object and installs 48 StubNop slots,
// so ~1900 lines of already-decompiled Tutorial dialog bodies were unreachable
// — the [[reference-ported-registered-but-unwired]] shape exactly.
//
// ############################################################################
// # INTEGRATION REQUIRED — this file DOUBLE-REGISTERS until you delete four   #
// # lines from code/Game/Dialog/ui_dialog_stub_factories.cpp:                 #
// #                                                                          #
// #   UI_DIALOG_STUB(TutorialUI,          "UI::DialogController", 0x60);      #
// #   UI_DIALOG_STUB(TutorialHighlightDialog, "UI::DialogInstance", 0xF0);    #
// #   UI_DIALOG_STUB(TutorialGoalDialog,  "TutorialGoal",         0x40);      #
// #   UI_DIALOG_STUB(TutorialSetupDialog, "TutorialSetup",        0x78);      #
// #                                                                          #
// # Until those four lines are gone, two FactoryRegistration nodes exist per  #
// # type name and which one Object::Factory::Startup keeps is undefined.      #
// ############################################################################
//
// Vtable layouts were read BYTE-WISE out of Application-x64.exe .rdata (the
// type-name string that follows each table is what fixes its end — see
// [[project-menu-dialog-vtables-unregistered]]), not guessed from Ghidra:
//
//  TutorialUI              @0x140160F30, 11 slots
//    0 0x140029470 dtor | 1 0x1400293F0 TypeName | 2 0x1400293E0 GetTypeId
//    3 0x1400E3AA0 (NOT PORTED) | 4 0x1400E37A0 Setup  -> TutorialUI_Serialize
//    5/6 0x14003A960/966 base thunks | 7 0x1400E3700 -> TutorialUI_Startup
//    8 0x14003A96C base thunk | 9 0x1400E36E0 -> TutorialUI_OnRemove
//    10 0x14003A978 base thunk
//
//  TutorialGoalDialog      @0x140160870, 10 slots
//    0 0x140027590 -> _ScalarDtor | 3 0x1400DF080 -> _Slot3 | 4 micro-ret
//    5 0x1400DEFD0 -> _Shutdown (Serialize) | 6 0x140027520 -> _OnEvent
//    7 0x140001640 -> _Slot7 | 8 0x1400DEEE0 -> _OnInit | 9 0x1400274B0 -> _OnDestroy
//
//  TutorialSetupDialog     @0x140160D20, 12 slots
//    0 0x140028620 -> _ScalarDtor | 3 0x1400E2BE0 -> _Slot3 | 4 micro-ret
//    5 0x1400E2A60 -> _Shutdown (Serialize) | 6 0x140028600 -> _OnEvent
//    7 0x140001640 -> _Slot7 | 8 0x1400E25F0 -> _Slot8 | 9 0x1400E24C0 -> _Slot9
//    10 0x1400E2460 -> _Slot10 | 11 0x1400E25F0.. -> _Slot11 (ctor wrapper)
//
//  TutorialHighlightDialog @0x1401609E0, 10 slots — see the block at the end.
//
// A SECOND defect this file closes, and the one that actually crashes:
// several of those already-ported bodies END by storing their own retail
// vtable back into the object (TutorialGoalDialog_ScalarDtor / _OnEvent /
// _OnDestroy, TutorialSetupDialog_ScalarDtor) via
// ResolveAddr(Module::Exe, 0x140160870 / 0x1401607A0 / 0x140160D20 /
// 0x140160C30). Those four .rdata addresses are registered NOWHERE, so each
// store writes 0 over slot 0 of a live object. Registering them here (see
// standalone/reg_tutorial_wave.cpp) makes the existing bodies correct with no
// edit to them at all.

#include "Dialog/ui_dialogs.h"
#include "Dialog/ui_panels.h"
#include "Tutorial/tutorialgoaldialog_funcs.h"
#include "Object/factory_registration.h"
#include "Object/internal_factory_audit.h"   // ObjAudit::IsRegistered (see TutDlg_UnregisterInternal)
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "Dialog/dialog_controller_vtable.h"

// Bodies that live in their own translation units.
void TutorialSetupDialog_Slot8(void* self);
void TutorialSetupDialog_Slot9(void* self);
void TutorialSetupDialog_Slot10(void* self);
int  TutorialSetupDialog_Slot3(void* self);
void TutorialSetupDialog_Slot7(void* self);
void* TutorialSetupDialog_Slot11(void* self);
void* TutorialSetupDialog_ScalarDtor(void* self, uint32_t flags);
uint64_t TutorialSetupDialog_Shutdown(void* self, void* stream);
void TutorialSetupDialog_OnEvent(void* self);

namespace {

// ============================================================
// TutDlg_UnregisterInternal � the missing ~Internal on the dialog Dtors below
// ============================================================
// Every dialog here is a UI::DialogInstance, and DialogInstance derives from
// Object::Internal: its ctor runs Object::Internal::Internal (Runtime 0x1800bfd10),
// which ENTERS the object into the engine-wide InternalFactory handle table
// (obj+0x08 = index, obj+0x0c = generation, table[count++] = obj).
//
// All four Dtors below used to MemFree without ever running the matching
// Object::Internal::~Internal, so every destroyed dialog left a dangling entry in
// that table. That is failure mode (A) in Object/internal_factory_audit.h, and it
// is a live heap-corruption source, not a tidiness issue:
//
//   ~Internal (0x1800bfdb0) unregisters by SWAP-REMOVE �
//       last = count-1;
//       if (this->idx != last) { table[this->idx] = table[last];
//                                *(int*)(table[this->idx] + 8) = this->idx; }
//   � so the next unrelated ~Internal that moves a stale entry executes a 4-BYTE
//   store of a table index at freedPtr+0x08, into memory the allocator has long
//   since handed to something else.
//
// That is exactly the corruption this project has been chasing: a fully live
// object with only the LOW 32 BITS of a pointer field at +0x08 replaced by a small
// integer, high dword still the live heap segment (0x00000171_00006d86,
// ..._000045F6, ..._00006d95 �). Caught red-handed 2026-09-04 with a hardware
// write watchpoint (BANISHED_TREE_CATCH=1):
//   [HWWATCH] HIT DR0 addr=...30d0 value now=0x6d95 writer RIP=Runtime-x64.dll+0xBFE0D
//   #0 ~Internal  #1 Inert_ScalarDtor  #3 ComponentEntity_Dtor  #5 EntityList_RemoveEntity
// and the victims were whatever reused the block � EntityTreeNode::parent (the
// long-standing [TreeGuard] hits) and a UI::Element child vtable (the
// ToolbarDialog_Update -> UI::Element::Enable crash).
// ObjAudit named this type: table entries going stale with the ORIGINAL vtable
// 0x1403E1A60 == tut_highlight::g_vt. TutorialHighlightDialog churns hardest
// (TutorialSetupDialog's highlight is torn down and rebuilt as steps advance), so
// it dominates, but all four dialogs leak the same way.
//
// Guarded with ObjAudit::IsRegistered exactly as Inert_ScalarDtor is: only objects
// genuinely present in the table are unregistered, so an instance built without the
// real Internal ctor (obj+0x08 = heap garbage) can never swap-remove through a
// garbage index � that is failure mode (B), and it is worse than the leak.
static void TutDlg_UnregisterInternal(void* self) {
    if (!self) return;
    if (fn_ObjectInternal_Dtor && ::ObjAudit::IsRegistered(self)) {
        fn_ObjectInternal_Dtor(self);
    }
}


// Every table is over-sized to 48 entries so an out-of-range dispatch from the
// framework lands on a nop rather than off the end. Retail's real slot count is
// documented per type in the header comment above.
constexpr int kSlots = 48;

void Nop(void*, ...) {}

void FillNops(void** vt) {
    for (int i = 0; i < kSlots; ++i) vt[i] = reinterpret_cast<void*>(&Nop);
}


// ===========================================================================
// TutorialUI  (UI::DialogController, 0x60)
// ===========================================================================
namespace tut_ui {

void* g_vt[kSlots];

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialUI");
    return c;
}
const char* TypeName(void*) { return "TutorialUI"; }

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    TutDlg_UnregisterInternal(self);   // see the note above � MUST run before the free
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (+0x20, retail 0x1400E37A0): find preText/postText/imageTable/
// nextButton/quitButton by name, assign the two button ids, hide buttonClose.
// self+0x18 = owning DialogInstance IBP, self+0x20 = root Element IBP; the
// framework fills both before dispatching setup, so a null there means we are
// being ticked before attach — skip rather than fault.
void Slot4(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0x18) == 0) return;
    if (*reinterpret_cast<uint64_t*>(s + 0x20) == 0) return;
    if (!fn_FindElement || !fn_Element_SetElementId) return;
    TutorialUI_Serialize(reinterpret_cast<TutorialUI*>(self));
}

// Slot7 (+0x38, retail 0x1400E3700): per-frame button visibility toggle. It
// dereferences self+0x50 / self+0x58 unguarded — legitimate in retail, where
// Slot4 always ran first. Ours is dispatched by the same framework, but the
// framework also ticks a controller before setup, so gate on Slot4's output.
void Slot7(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0x50) == 0) return;
    if (*reinterpret_cast<uint64_t*>(s + 0x58) == 0) return;
    if (!fn_Element_SetFlagsOnOff) return;
    TutorialUI_Startup(reinterpret_cast<TutorialUI*>(self));
}

// Slot9 (+0x48, retail 0x1400E36E0): element event. Only quitButton (id 0x579,
// eventType 0x12) does anything — it tears the tutorial down and queues the
// main-menu loader chain.
void Slot9(void* self, void* p2, int eventType, int eventId) {
    if (!self) return;
    TutorialUI_OnRemove(reinterpret_cast<TutorialUI*>(self), p2, eventType, eventId);
}

struct Init {
    Init() {
        FillNops(g_vt);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&TypeName);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&Slot4);
        g_vt[7] = reinterpret_cast<void*>(&Slot7);
        g_vt[9] = reinterpret_cast<void*>(&Slot9);
    }
} g_init;

// Wave 10D lane VTFID: retail's TutorialUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
// (this file's no-op is spelled `Nop`, not `StubNop` — the stub identity is
//  what stops Apply() from clobbering a deliberate override, so it has to be
//  THIS file's actual filler, not the name the other 37 sites happen to use.)
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "TutorialUI", &Nop);

void Zero(void* p) { for (size_t o = 0; o < 0x60; o += 8) *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace tut_ui

// ===========================================================================
// TutorialGoalDialog  (parent "TutorialGoal", 0x40)
// ===========================================================================
namespace tut_goaldlg {

void* g_vt[kSlots];

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialGoalDialog");
    return c;
}
const char* TypeName(void*) { return "TutorialGoalDialog"; }

// NOTE slot 0 is deliberately NOT TutorialGoalDialog_ScalarDtor. That body
// stores the retail vtable, destroys both inline Strings, stores the
// TutorialGoal base vtable and then calls Object::External::~External. With
// 0x140160870 / 0x1401607A0 now registered the vtable stores are correct, but
// the External base was never constructed by our factory (we alloc+zero, we do
// not run External::External), so ~External would unwind fields that were never
// initialised. Keep the safe teardown; the strings are the only leak and they
// are empty on a zero-initialised object.
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    TutDlg_UnregisterInternal(self);   // see the note above � MUST run before the free
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

int  Slot3(void* self) { return TutorialGoalDialog_Slot3(self); }

// Slot5 — the .rsc deserializer. This is the slot that matters most: with the
// stub in place it consumed ZERO bytes where the stream holds a String, a
// Filename and an int, so every TutorialGoalDialog in Tutorial.rsc loaded blank
// AND desynced the stream cursor for whatever followed it.
uint64_t Slot5(void* self, void* stream) {
    if (!self || !stream) return 0;
    return TutorialGoalDialog_Shutdown(self, stream);
}
void* Slot6(void* self, uint64_t a, uint64_t b, uint64_t c) {
    if (!self) return self;
    return TutorialGoalDialog_OnEvent(self, a, b, c);
}
void Slot7(void* self) { if (self) TutorialGoalDialog_Slot7(self); }
uint64_t Slot8(void* self) {
    if (!self) return 0;
    // _OnInit derefs *g_TypeId_ComponentDescription_FC / *g_TypeId_ComponentEntity
    // unguarded. Those two exe .rdata globals are unregistered in standalone
    // (null pointer, not a zero value), so this is our-standalone-differs, not
    // a faithful crash — skip rather than fault. See CLAUDE.md.
    if (!g_TypeId_ComponentDescription_FC || !g_TypeId_ComponentEntity) return 0;
    return TutorialGoalDialog_OnInit(self);
}
void* Slot9(void* self) { return TutorialGoalDialog_OnDestroy(self); }

struct Init {
    Init() {
        FillNops(g_vt);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&TypeName);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[3] = reinterpret_cast<void*>(&Slot3);
        g_vt[5] = reinterpret_cast<void*>(&Slot5);
        g_vt[6] = reinterpret_cast<void*>(&Slot6);
        g_vt[7] = reinterpret_cast<void*>(&Slot7);
        g_vt[8] = reinterpret_cast<void*>(&Slot8);
        g_vt[9] = reinterpret_cast<void*>(&Slot9);
    }
} g_init;

void Zero(void* p) { for (size_t o = 0; o < 0x40; o += 8) *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    // Retail ctor seeds +0x30 with -1 (see TutorialGoalDialog_OnDestroy). A zero
    // there is a legal index, so the sentinel is load-bearing.
    *reinterpret_cast<uint32_t*>(static_cast<char*>(p) + 0x30) = 0xFFFFFFFFu;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    *reinterpret_cast<uint32_t*>(static_cast<char*>(m) + 0x30) = 0xFFFFFFFFu;
    return m;
}

}  // namespace tut_goaldlg

// ===========================================================================
// TutorialSetupDialog  (parent "TutorialSetup", 0x78)
// ===========================================================================
namespace tut_setupdlg {

void* g_vt[kSlots];

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialSetupDialog");
    return c;
}
const char* TypeName(void*) { return "TutorialSetupDialog"; }

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    TutDlg_UnregisterInternal(self);   // see the note above � MUST run before the free
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

int  Slot3(void* self) { return TutorialSetupDialog_Slot3(self); }

// Slot5 — the .rsc deserializer (two String vectors, two inline Strings, an
// ExternalPtr and two ints). Same stream-desync argument as TutorialGoalDialog.
uint64_t Slot5(void* self, void* stream) {
    if (!self || !stream) return 0;
    return TutorialSetupDialog_Shutdown(self, stream);
}
void Slot6(void* self) { if (self) TutorialSetupDialog_OnEvent(self); }
void Slot7(void* self) { if (self) TutorialSetupDialog_Slot7(self); }
void Slot8(void* self) {
    // _Slot8 derefs *g_TypeId_ComponentDescription_FC / *g_TypeId_ComponentEntity.
    if (!self || !g_TypeId_ComponentDescription_FC || !g_TypeId_ComponentEntity) return;
    if (!fn_TSD_Slot9Helper || !fn_TSD_OpenDialog) return;
    TutorialSetupDialog_Slot8(self);
}
void Slot9(void* self) {
    if (!self || !fn_TSD_Slot9Helper || !fn_TSD_OpenDialog) return;
    TutorialSetupDialog_Slot9(self);
}
void Slot10(void* self) {
    if (!self || !fn_TSD_SetDialogFlags || !fn_TSD_FindInstance) return;
    TutorialSetupDialog_Slot10(self);
}
void* Slot11(void* self) { return TutorialSetupDialog_Slot11(self); }

struct Init {
    Init() {
        FillNops(g_vt);
        g_vt[0]  = reinterpret_cast<void*>(&Dtor);
        g_vt[1]  = reinterpret_cast<void*>(&TypeName);
        g_vt[2]  = reinterpret_cast<void*>(&GetTypeId);
        g_vt[3]  = reinterpret_cast<void*>(&Slot3);
        g_vt[5]  = reinterpret_cast<void*>(&Slot5);
        g_vt[6]  = reinterpret_cast<void*>(&Slot6);
        g_vt[7]  = reinterpret_cast<void*>(&Slot7);
        g_vt[8]  = reinterpret_cast<void*>(&Slot8);
        g_vt[9]  = reinterpret_cast<void*>(&Slot9);
        g_vt[10] = reinterpret_cast<void*>(&Slot10);
        g_vt[11] = reinterpret_cast<void*>(&Slot11);
    }
} g_init;

void Zero(void* p) { for (size_t o = 0; o < 0x78; o += 8) *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0; }
void SeedCtorScalars(void* p) {
    // Retail TutorialSetupDialog_Ctor (0x1400E2CC0) seeds +0x60 with -1.
    // [[reference-ctor-scalars-in-startup-are-unset-after-load]]: a zero there
    // is a legal value, so the sentinel has to come from construction.
    *reinterpret_cast<int32_t*>(static_cast<char*>(p) + 0x60) = -1;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; SeedCtorScalars(p);
    return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; SeedCtorScalars(m); return m; }

}  // namespace tut_setupdlg

// ===========================================================================
// TutorialHighlightDialog  (UI::DialogInstance, 0xF0)
//
// This one is NOT built by the External factory. TutorialSetupDialog's
// open-dialog helper (retail 0x1400E2700, ported as TutWave_OpenHighlightDialog)
// builds it BY HAND: MemAlloc(0xF0) -> DialogInstance::DialogInstance -> a
// single store of the .rdata vtable at 0x1401609E0. That is the
// [[project-menu-dialog-vtables-unregistered]] shape — promoting a factory
// changes nothing; what it needs is a real table registered at 0x1401609E0.
//
// Slot 6 (Draw, retail 0x1400DFFE0) and both of its positioning helpers
// (0x1400DFDA0 world-entity / 0x1400DFAC0 UI-element) are now ported in
// tutorialhighlightdialog.cpp, so the glow ring and the arrow track their
// target and bob with sinf(t*pi)*8 the way retail does.
// ===========================================================================
namespace tut_highlight {

void* g_vt[kSlots];

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialHighlightDialog");
    return c;
}
const char* TypeName(void*) { return "TutorialHighlightDialog"; }

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    TutDlg_UnregisterInternal(self);   // see the note above � MUST run before the free
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void Slot4_Startup(void* self) {
    if (!self) return;
    if (!fn_DialogInstance_FindElement) return;
    TutorialHighlightDialog_Startup(reinterpret_cast<TutorialHighlightDialog*>(self));
}

void Slot6_Draw(void* self) {
    if (!self) return;
    TutorialHighlightDialog_Draw(reinterpret_cast<TutorialHighlightDialog*>(self));
}

void Slot7_Load(void* self, void* param2) {
    if (!self || !param2) return;
    TutorialHighlightDialog_Load(reinterpret_cast<TutorialHighlightDialog*>(self), param2);
}

// Retail slot 8 is one of the shared 0x14003Axxx micro-thunks into the Runtime
// DialogInstance base. fn_DialogInstance_OnElementEvent takes 8 arguments and
// this slot's real ABI was never confirmed, so forwarding would be a guess —
// leave it a nop (the fill below already does that) rather than mis-call it.

struct Init {
    Init() {
        FillNops(g_vt);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&TypeName);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&Slot4_Startup);
        g_vt[6] = reinterpret_cast<void*>(&Slot6_Draw);
        g_vt[7] = reinterpret_cast<void*>(&Slot7_Load);
    }
} g_init;

void Zero(void* p) { for (size_t o = 0; o < 0xF0; o += 8) *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xF0, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace tut_highlight

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Vtable accessors + the hand-rolled TutorialHighlightDialog ctor, consumed by
// tutorial_wave.cpp and registered in standalone/reg_tutorial_wave.cpp.
// ---------------------------------------------------------------------------
extern "C" void* GetTutorialUIVtable()              { return tut_ui::g_vt; }
extern "C" void* GetTutorialGoalDialogVtable()      { return tut_goaldlg::g_vt; }
extern "C" void* GetTutorialSetupDialogVtable()     { return tut_setupdlg::g_vt; }
extern "C" void* GetTutorialHighlightDialogVtable() { return tut_highlight::g_vt; }

// 0x1400E01D0 — TutorialHighlightDialog::TutorialHighlightDialog.
//
// Ghidra shows the trailing arguments as `in_stack_*` because the frame is
// shared with the base ctor; the real layout, from the disassembly at
// 0x1400E01D0-0x1400E027D:
//   RCX      self
//   RDX      UI::UserInterface*
//   R8       UI::Dialog External*          (TutorialSetupDialog+0x50)
//   R9       InternalPtr* parent — *R9 is copied into the arg-4 home slot and a
//            pointer to THAT copy is what reaches DialogInstance::DialogInstance
//   [rsp+28] InternalPtr*  -> self+0xE0 = *arg
//   [rsp+30] int32         -> self+0xD8
//   [rsp+38] InternalPtr*  -> self+0xE8 = *arg
// The base call passes dataSource=0, extraPtr=0, flags=0 (all three stack slots
// are zeroed at 0x1400E01F4/F8/FC before the CALL).
extern "C" void* TutWave_HighlightDialogCtor(void* self, void* ui, void* dialogExternal,
                                             void* parentIBP, void* ibpE0,
                                             int32_t valD8, void* ibpE8) {
    if (!self) return nullptr;

    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;
    if (fn_DialogInstance_Ctor && dialogExternal) {
        fn_DialogInstance_Ctor(self, ui, dialogExternal, &parentCopy, nullptr, nullptr, 0);
    }

    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    *reinterpret_cast<void**>(self) = tut_highlight::g_vt;
    *reinterpret_cast<uint64_t*>(s + 0xB8) = 0;
    *reinterpret_cast<uint64_t*>(s + 0xC0) = 0;
    *reinterpret_cast<uint64_t*>(s + 0xC8) = 0;
    *reinterpret_cast<uint64_t*>(s + 0xD0) = 0;
    *reinterpret_cast<int32_t*>(s  + 0xD8) = valD8;
    *reinterpret_cast<uint64_t*>(s + 0xE0) = ibpE0 ? *reinterpret_cast<uint64_t*>(ibpE0) : 0;
    *reinterpret_cast<uint64_t*>(s + 0xE8) = ibpE8 ? *reinterpret_cast<uint64_t*>(ibpE8) : 0;
    return self;
}

// 0x1400E2CC0 — TutorialSetupDialog::TutorialSetupDialog. Retail:
//   Object::External::External(this); vtable = TutorialSetup; vtable =
//   TutorialSetupDialog; zero +0x20..+0x58; +0x60 = -1; +0x68 = 0; +0x70 = 0.
// Reached from TutorialSetupDialog_Slot11 and _OnEvent, both of which resolve
// this address and would EXEC 0 without it.
extern "C" void* TutWave_TutorialSetupDialogCtor(void* self) {
    if (!self) return nullptr;
    if (fn_External_External) fn_External_External(self);
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    *reinterpret_cast<void**>(self) = tut_setupdlg::g_vt;
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x40) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x48) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;
    *reinterpret_cast<int32_t*>(s  + 0x60) = -1;
    *reinterpret_cast<uint64_t*>(s + 0x68) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x70) = 0;
    return self;
}

extern "C" void* TutorialUI_SAFactory(void)                    { return tut_ui::Factory(); }
extern "C" void* TutorialUI_SAInPlace(void* m)                 { return tut_ui::InPlace(m); }
extern "C" void* TutorialGoalDialog_SAFactory(void)            { return tut_goaldlg::Factory(); }
extern "C" void* TutorialGoalDialog_SAInPlace(void* m)         { return tut_goaldlg::InPlace(m); }
extern "C" void* TutorialSetupDialog_SAFactory(void)           { return tut_setupdlg::Factory(); }
extern "C" void* TutorialSetupDialog_SAInPlace(void* m)        { return tut_setupdlg::InPlace(m); }
extern "C" void* TutorialHighlightDialog_SAFactory(void)       { return tut_highlight::Factory(); }
extern "C" void* TutorialHighlightDialog_SAInPlace(void* m)    { return tut_highlight::InPlace(m); }

// These four REPLACE the UI_DIALOG_STUB(...) entries listed at the top of this
// file. Delete those four lines in the same integration commit.
REGISTER_TYPE(TutorialUI, "UI::DialogController",
              &TutorialUI_SAFactory, &TutorialUI_SAInPlace);
REGISTER_TYPE(TutorialGoalDialog, "TutorialGoal",
              &TutorialGoalDialog_SAFactory, &TutorialGoalDialog_SAInPlace);
REGISTER_TYPE(TutorialSetupDialog, "TutorialSetup",
              &TutorialSetupDialog_SAFactory, &TutorialSetupDialog_SAInPlace);
REGISTER_TYPE(TutorialHighlightDialog, "UI::DialogInstance",
              &TutorialHighlightDialog_SAFactory, &TutorialHighlightDialog_SAInPlace);
