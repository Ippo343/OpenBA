// engine/reimpl/saveloaddialog.cpp
// Reimplementation of SaveLoadDialog vtable methods from Application-x64.exe.
//
// SaveLoadDialog (0x160 bytes), vtable at 0x14015f900.
// Extends DialogInstance (0xB8 base).
// Contains UI::Sprite sub-object at +0xD0.
// Has Ctor at 0x1400b81c0.
//
// Vtable layout (10 slots):
//   +0x00: 0x1400211b0 — ScalarDtor (hookable, 15b prologue)
//   +0x08: 0x140021150 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140021140 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400b7d00 — Startup: build save/load UI, populate file list (hookable, 14b)
//   +0x28: 0x14003a94e — Runtime thunk (6b, unhookable)
//   +0x30: 0x1400b6880 — Update: refresh file list, update preview (hookable, 15b)
//   +0x38: 0x1400b79b0 — Load: load save file metadata, timestamps (hookable, 14b)
//   +0x40: 0x1400b67e0 — OnEvent: branch-based handler (rel branch in prologue, unhookable)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)

#include "ui/Dialogs.h"
#include "Dialog/ui_dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookStartup;
static HookInfo g_hookUpdate;
static HookInfo g_hookLoad;
static HookInfo g_hookOnEvent;

// ============================================================
// Wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags) — REIMPLEMENTED
// Ghidra: FUN_1400211b0 -> FUN_1400b80f0
// 1. Set vtable
// 2. Iterate array at +0x110 (count at +0x118), 0x40-byte entries.
//    For each entry: destroy vtable-object at +0x30 via ScalarDtor(1),
//    then destroy vtable-object at +0x38 via ScalarDtor(1).
// 3. MemFree +0x110 array, zero +0x110 and +0x118
// 4. Call DialogInstance::~DialogInstance
// 5. Conditional free
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "SaveLoadDialog::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, SaveLoadDialogAddr::Vtable);
    *(void**)self = s_vtable;

    uintptr_t s = (uintptr_t)self;

    // Destroy save file entry array at +0x110 (FUN_1400b80f0 logic)
    int count = *(int*)(s + 0x118);
    if (count > 0) {
        uintptr_t arrBase = *(uintptr_t*)(s + 0x110);
        uintptr_t offset = 0;
        for (int i = 0; i < count; i++) {
            // Destroy vtable-object at +0x30 in entry via ScalarDtor(obj, 1)
            void** obj1 = *(void***)(arrBase + 0x30 + offset);
            if (obj1 != nullptr) {
                // Call through vtable[0] = ScalarDtor
                typedef void* (*VDtor_t)(void*, uint32_t);
                ((VDtor_t)(*(void***)obj1)[0])(obj1, 1);
            }
            // Destroy vtable-object at +0x38 in entry (only if +0x30 was non-null)
            if (*(uintptr_t*)(arrBase + 0x30 + offset) != 0) {
                void** obj2 = *(void***)(arrBase + 0x38 + offset);
                if (obj2 != nullptr) {
                    typedef void* (*VDtor_t)(void*, uint32_t);
                    ((VDtor_t)(*(void***)obj2)[0])(obj2, 1);
                }
            }
            offset += 0x40;
        }
    }
    fn_MemFreeStatic(*(void**)(s + 0x110));
    *(uint64_t*)(s + 0x110) = 0;
    *(uint64_t*)(s + 0x118) = 0;

    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot 4: Startup(this) — REIMPLEMENTED
static void Reimpl_Startup(void* self) {
    g_lastHook = "SaveLoadDialog::Startup";
    SaveLoadDialog_Startup((SaveLoadDialog*)self);
}

// Slot 6: Update(this) — REIMPLEMENTED
static void Reimpl_Update(void* self) {
    g_lastHook = "SaveLoadDialog::Update";
    SaveLoadDialog_Update((SaveLoadDialog*)self);
}

// Slot 8: OnEvent(this, element, eventType, eventData) — REIMPLEMENTED
// No trampoline call — rel branch at byte 13.
static void Reimpl_OnEvent(void* self, void* element, int eventType, int eventData) {
    g_lastHook = "SaveLoadDialog::OnEvent";
    SaveLoadDialog_OnEvent((SaveLoadDialog*)self, element, eventType, eventData);
}

// Slot 7: Load(this, time, loaderLoop, param4) — REIMPLEMENTED
// Accumulates spinner rotation, processes deferred flags, constructs game loader loop chain
// on load completion. All constructor calls use resolved Runtime DLL exports.
typedef void (*fn_Load_t)(void* self, void* time, void** loaderLoop, uint64_t param4);
static void Reimpl_Load(void* self, void* time, void** loaderLoop, uint64_t param4) {
    g_lastHook = "SaveLoadDialog::Load";
    SaveLoadDialog_Load((SaveLoadDialog*)self, time, loaderLoop, param4);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallSaveLoadDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400211b0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallSaveLoadDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b7d00);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallSaveLoadDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b6880);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogUpdate() { RemoveHook(&g_hookUpdate); }

void InstallSaveLoadDialogLoad() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b79b0);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 14);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Load hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogLoad() { RemoveHook(&g_hookLoad); }

// OnEvent (0x1400b67e0) — short JZ at byte 13, full reimpl only.
// Prologue: PUSH RBX(2)+SUB RSP,0x30(4)+MOV RBX,RCX(3)+CMP R8d,2(4) = 13 bytes
void InstallSaveLoadDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b67e0);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 13);
#ifdef _DEBUG
    printf("[SaveLoadDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogOnEvent() { RemoveHook(&g_hookOnEvent); }
