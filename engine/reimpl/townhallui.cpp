// engine/reimpl/townhallui.cpp
// Reimplementation of TownHallUI vtable methods from Application-x64.exe.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0. Second-largest panel.
// Extends DialogController (0x30 base). Has Ctor at 0x140081280.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400146b0 — ScalarDtor(self, flags) [15b prologue]
//   +0x20: 0x140081000 — Serialize(self) [15b prologue]
//   +0x38: 0x14007d550 — Startup(self) [13b prologue]
//   +0x40: 0x14007e550 — OnCreate(self) [13b prologue]
//   +0x48: 0x14007c180 — OnRemove(self, ?, eventType, ...) [13b prologue]
//   +0x50: 0x140080db0 — Update(self, stream) [15b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140014650 — GetDescription, 8 bytes
//   +0x10: 0x140014640 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)

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
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookStartup;
static HookInfo g_hookOnCreate;
static HookInfo g_hookOnRemove;
static HookInfo g_hookUpdate;

// ============================================================
// BST recursive free helpers (iterative post-order to avoid stack overflow)
// ============================================================

// Free BST nodes with children at +0x10/+0x18 (used by container at +0x1D8)
static void FreeBST_10_18(void* node) {
    if (!node) return;
    FreeBST_10_18(*(void**)((uintptr_t)node + 0x10));
    FreeBST_10_18(*(void**)((uintptr_t)node + 0x18));
    fn_MemFreeStatic(node);
}

// Free BST nodes with children at +0x20/+0x28 (used by container at +0x138)
static void FreeBST_20_28(void* node) {
    if (!node) return;
    FreeBST_20_28(*(void**)((uintptr_t)node + 0x20));
    FreeBST_20_28(*(void**)((uintptr_t)node + 0x28));
    fn_MemFreeStatic(node);
}

// Free BST nodes with children at +0x18/+0x20 (used by container at +0x50)
static void FreeBST_18_20(void* node) {
    if (!node) return;
    FreeBST_18_20(*(void**)((uintptr_t)node + 0x18));
    FreeBST_18_20(*(void**)((uintptr_t)node + 0x20));
    fn_MemFreeStatic(node);
}

// ============================================================
// Reimplemented wrappers
// ============================================================

// ScalarDtor: REIMPLEMENTED (0x1400146b0 -> inner dtor 0x1400810d0)
// Inner dtor frees: BST at +0x1D8, buffer +0x1C8, BST at +0x138,
// buffer +0x118, buffers +0xF0/+0xE0/+0xD0/+0xC0/+0xB0,
// BST at +0x50, container at +0x40, then calls DialogController::~Dtor.
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TownHallUI::ScalarDtor";
    uintptr_t s = (uintptr_t)self;

    // Set vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x14015dce0);
    *(void**)self = s_vtable;

    // BST at +0x1D8 (children at +0x10/+0x18)
    void* bstRoot1D8 = *(void**)(s + 0x1D8);
    if (bstRoot1D8) {
        FreeBST_10_18(*(void**)((uintptr_t)bstRoot1D8 + 0x10));
        FreeBST_10_18(*(void**)((uintptr_t)bstRoot1D8 + 0x18));
        fn_MemFreeStatic(bstRoot1D8);
    }
    *(uint64_t*)(s + 0x1D8) = 0;
    *(uint32_t*)(s + 0x1E0) = 0;

    // Buffer at +0x1C8
    fn_MemFreeStatic(*(void**)(s + 0x1C8));
    *(uint64_t*)(s + 0x1C8) = 0;
    *(uint64_t*)(s + 0x1D0) = 0;

    // BST at +0x138 (children at +0x20/+0x28)
    void* bstRoot138 = *(void**)(s + 0x138);
    if (bstRoot138) {
        FreeBST_20_28(*(void**)((uintptr_t)bstRoot138 + 0x20));
        FreeBST_20_28(*(void**)((uintptr_t)bstRoot138 + 0x28));
        fn_MemFreeStatic(bstRoot138);
    }
    *(uint64_t*)(s + 0x138) = 0;
    *(uint32_t*)(s + 0x140) = 0;

    // Buffer at +0x118
    fn_MemFreeStatic(*(void**)(s + 0x118));
    *(uint64_t*)(s + 0x118) = 0;
    *(uint64_t*)(s + 0x120) = 0;

    // Buffer at +0xF0
    fn_MemFreeStatic(*(void**)(s + 0xF0));
    *(uint64_t*)(s + 0xF0) = 0;
    *(uint64_t*)(s + 0xF8) = 0;

    // Buffer at +0xE0
    fn_MemFreeStatic(*(void**)(s + 0xE0));
    *(uint64_t*)(s + 0xE0) = 0;
    *(uint64_t*)(s + 0xE8) = 0;

    // Buffer at +0xD0
    fn_MemFreeStatic(*(void**)(s + 0xD0));
    *(uint64_t*)(s + 0xD0) = 0;
    *(uint64_t*)(s + 0xD8) = 0;

    // Buffer at +0xC0
    fn_MemFreeStatic(*(void**)(s + 0xC0));
    *(uint64_t*)(s + 0xC0) = 0;
    *(uint64_t*)(s + 0xC8) = 0;

    // Buffer at +0xB0
    fn_MemFreeStatic(*(void**)(s + 0xB0));
    *(uint64_t*)(s + 0xB0) = 0;
    *(uint64_t*)(s + 0xB8) = 0;

    // BST at +0x50 (children at +0x18/+0x20)
    void* bstRoot50 = *(void**)(s + 0x50);
    if (bstRoot50) {
        FreeBST_18_20(*(void**)((uintptr_t)bstRoot50 + 0x18));
        FreeBST_18_20(*(void**)((uintptr_t)bstRoot50 + 0x20));
        fn_MemFreeStatic(bstRoot50);
    }
    *(uint64_t*)(s + 0x50) = 0;
    *(uint32_t*)(s + 0x58) = 0;

    // Container at +0x40: FUN_140015930
    // Iterates count=*(int*)(+0x48), for each element at base[i*0x40+0x30]
    // frees that pointer and zeros it+next, then frees array at +0x40.
    int containerCount = *(int*)(s + 0x48);
    if (containerCount > 0) {
        uintptr_t arrayBase = *(uintptr_t*)(s + 0x40);
        for (int i = 0; i < containerCount; i++) {
            uintptr_t elemPtr = arrayBase + (uintptr_t)i * 0x40 + 0x30;
            fn_MemFreeStatic(*(void**)elemPtr);
            *(uint64_t*)elemPtr = 0;
            *(uint64_t*)(elemPtr + 8) = 0;
        }
    }
    fn_MemFreeStatic(*(void**)(s + 0x40));
    *(uint64_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x48) = 0;

    // Call base class destructor
    fn_DialogController_Dtor(self);

    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "TownHallUI::Serialize";
    TownHallUI_Serialize((TownHallUI*)self);
}

// Startup: REIMPLEMENTED
static void Reimpl_Startup(void* self) {
    g_lastHook = "TownHallUI::Startup";
    TownHallUI_Startup((TownHallUI*)self);
}

// OnCreate: REIMPLEMENTED
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "TownHallUI::OnCreate";
    TownHallUI_OnCreate((TownHallUI*)self);
}

// OnRemove: REIMPLEMENTED
typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    g_lastHook = "TownHallUI::OnRemove";
    TownHallUI_OnRemove((TownHallUI*)self, param2, eventType, eventId);
}

typedef void (*fn_Update_t)(void* self, void* stream);
static void Reimpl_Update(void* self, void* stream) {
    g_lastHook = "TownHallUI::Update";
    TownHallUI_Update((TownHallUI*)self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTownHallUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400146b0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[TownHallUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTownHallUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140081000);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TownHallUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallTownHallUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007d550);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[TownHallUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIStartup() { RemoveHook(&g_hookStartup); }

void InstallTownHallUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007e550);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[TownHallUI] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallTownHallUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007c180);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[TownHallUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIOnRemove() { RemoveHook(&g_hookOnRemove); }

void InstallTownHallUIUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140080db0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[TownHallUI] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIUpdate() { RemoveHook(&g_hookUpdate); }
