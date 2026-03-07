// engine/reimpl/professionui.cpp
// Reimplementation of ProfessionUI vtable methods from Application-x64.exe.
//
// ProfessionUI (0x78 bytes), vtable at 0x14015d0f0.
// Extends DialogController (0x30 base). Has Ctor at 0x1400775a0.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140013450 — Dtor (scalar deleting destructor)
//   +0x28: 0x140076CA0 — Slot5 (handler)
//   +0x40: 0x1400765A0 — Slot8 (handler)
//   +0x48: 0x140076C10 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x1400133F0 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x1400133E0 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14003A95A — 6 bytes, Slot3 (thunk to Runtime)
//   +0x20: 0x14003A984 — 6 bytes, Slot4 (thunk to Runtime)
//
// Stubs (base class no-ops):
//   +0x30: Slot6, +0x38: Slot7, +0x50: Slot10

#include "ui/Panels.h"
#include "Dialog/ui_panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// BST recursive free helper (children at +0x18/+0x20)
// ============================================================
static void ProfUI_FreeBST(void* node) {
    if (!node) return;
    ProfUI_FreeBST(*(void**)((uintptr_t)node + 0x18));
    ProfUI_FreeBST(*(void**)((uintptr_t)node + 0x20));
    fn_MemFreeStatic(node);
}

// ============================================================
// Reimplemented wrappers
// ============================================================

// Dtor: REIMPLEMENTED (0x140013450 -> inner dtor 0x1400774e0)
// Inner dtor frees: BST at +0x60, buffers at +0x50/+0x40/+0x30,
// then calls DialogController::~Dtor.
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ProfessionUI::Dtor";
    uintptr_t s = (uintptr_t)self;

    // Set vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x14015d0f0);
    *(void**)self = s_vtable;

    // BST at +0x60 (children at +0x18/+0x20)
    void* bstRoot = *(void**)(s + 0x60);
    if (bstRoot) {
        ProfUI_FreeBST(*(void**)((uintptr_t)bstRoot + 0x18));
        ProfUI_FreeBST(*(void**)((uintptr_t)bstRoot + 0x20));
        fn_MemFreeStatic(bstRoot);
    }
    *(uint64_t*)(s + 0x60) = 0;
    *(uint32_t*)(s + 0x68) = 0;

    // Buffer at +0x50
    fn_MemFreeStatic(*(void**)(s + 0x50));
    *(uint64_t*)(s + 0x50) = 0;
    *(uint64_t*)(s + 0x58) = 0;

    // Buffer at +0x40
    fn_MemFreeStatic(*(void**)(s + 0x40));
    *(uint64_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x48) = 0;

    // Buffer at +0x30
    fn_MemFreeStatic(*(void**)(s + 0x30));
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;

    // Call base class destructor
    fn_DialogController_Dtor(self);

    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Slot5_t)(void* self);
static void Reimpl_Slot5(void* self) {
    g_lastHook = "ProfessionUI::Slot5";
    ProfessionUI_Slot5((ProfessionUI*)self);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "ProfessionUI::Slot8";
    ProfessionUI_Slot8((ProfessionUI*)self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "ProfessionUI::Slot9";
    ProfessionUI_Slot9((ProfessionUI*)self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallProfessionUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013450);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[ProfessionUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionUIDtor() { RemoveHook(&g_hookDtor); }

void InstallProfessionUISlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140076CA0);
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 17);
#ifdef _DEBUG
    printf("[ProfessionUI] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionUISlot5() { RemoveHook(&g_hookSlot5); }

void InstallProfessionUISlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400765A0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[ProfessionUI] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionUISlot8() { RemoveHook(&g_hookSlot8); }

void InstallProfessionUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140076C10);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[ProfessionUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionUISlot9() { RemoveHook(&g_hookSlot9); }
