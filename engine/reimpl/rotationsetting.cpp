// engine/reimpl/rotationsetting.cpp
// Reimplementation of RotationSetting vtable methods from Application-x64.exe.
//
// RotationSetting (0x18 bytes), vtable at 0x140160710.
// Extends ToolSetting (abstract, no vtable).
// Factory at 0x140026d20, Ctor is inline.
//
// Vtable layout (4 slots):
//   +0x00: 0x140026dd0 — Slot0 Dtor (hooked)
//   +0x08: 0x140026d10 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x140026d00 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookRotationSettingDtor;
typedef void* (*fn_RotationSettingDtor_t)(void* self, uint32_t flags);
static void* Reimpl_RotationSettingDtor(void* self, uint32_t flags) {
    g_lastHook = "RotationSetting::RotationSettingDtor";
    return ((fn_RotationSettingDtor_t)g_hookRotationSettingDtor.trampoline)(self, flags);
}

void InstallRotationSettingDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026dd0);
    InstallHook(target, (void*)Reimpl_RotationSettingDtor, &g_hookRotationSettingDtor, 19);
#ifdef _DEBUG
    printf("[RotationSetting] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveRotationSettingDtor() { RemoveHook(&g_hookRotationSettingDtor); }
