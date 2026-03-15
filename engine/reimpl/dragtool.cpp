// engine/reimpl/dragtool.cpp
// Reimplementation of DragTool vtable methods from Application-x64.exe.
//
// DragTool (ABSTRACT, 0x38 bytes), vtable at 0x140161c10.
// Extends Tool (0x28 bytes, vtable at 0x140161f10).
//
// Vtable layout (15 slots):
//   +0x00: 0x14002da20 — Slot0 Dtor (hooked)
//   +0x08: 0x14002da10 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002da00 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x14003addc — Slot4 pure virtual stub
//   +0x28: 0x14003addc — Slot5 pure virtual stub
//   +0x30: 0x1400fc6b0 — Slot6 OnKey (hooked)
//   +0x38: 0x14003addc — Slot7 pure virtual stub
//   +0x40: 0x14003addc — Slot8 pure virtual stub
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unsafe for trampoline)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x14003addc — Slot13 pure virtual stub
//   +0x70: 0x14003addc — Slot14 pure virtual stub

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Player/dragtool.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
// Kept as trampoline: needs Object::Internal::~Internal which isn't resolved as a fn ptr yet.
// When fn_Internal_Dtor is added, switch to calling DragTool_Dtor() in code/Game/Player.
static HookInfo g_hookDragToolDtor;
typedef void* (*fn_DragToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_DragToolDtor(void* self, uint32_t flags) {
    g_lastHook = "DragTool::DragToolDtor";
    return ((fn_DragToolDtor_t)g_hookDragToolDtor.trampoline)(self, flags);
}

// --- Slot6: OnKey ---
// DEEPLY REIMPLEMENTED: delegates to DragTool_OnKey() in code/Game/Player/dragtool.cpp
static HookInfo g_hookDragToolOnKey;
static uint64_t Reimpl_DragToolOnKey(void* self, char key) {
    g_lastHook = "DragTool::DragToolOnKey";
    return DragTool_OnKey((DragTool*)self, key);
}

void InstallDragToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002da20);
    InstallHook(target, (void*)Reimpl_DragToolDtor, &g_hookDragToolDtor, 19);
#ifdef _DEBUG
    printf("[DragTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveDragToolDtor() { RemoveHook(&g_hookDragToolDtor); }

void InstallDragToolOnKey() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fc6b0);
    InstallHook(target, (void*)Reimpl_DragToolOnKey, &g_hookDragToolOnKey, 13);
#ifdef _DEBUG
    printf("[DragTool] OnKey hooked at %p\n", (void*)target);
#endif
}
void RemoveDragToolOnKey() { RemoveHook(&g_hookDragToolOnKey); }
