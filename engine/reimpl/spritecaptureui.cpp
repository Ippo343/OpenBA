// engine/reimpl/spritecaptureui.cpp
// Reimplementation of SpriteCaptureUI vtable methods from Application-x64.exe.
//
// SpriteCaptureUI (0x70 bytes), vtable at 0x14015d4f0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140013fc0 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x140079640 — Serialize(self) [large fn, 117 instrs]
//   +0x40: 0x1400793a0 — OnCreate(self) [large fn, 156 instrs]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140013d00 — GetDescription, 8 bytes (MOV RAX,[RIP+x]; RET)
//   +0x10: 0x140013cf0 — GetTypeId, 7 bytes (MOV EAX,[RIP+x]; RET)
//   +0x18: 0x140079850 — Shutdown, 3 bytes (RET 0)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x38: 0x14003a97e — thunk (6b JMP)
//   +0x48: 0x140079620 — mid-function code, not a valid entry
//   +0x50: 0x14003a978 — thunk (6b JMP)

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
static HookInfo g_hookOnCreate;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "SpriteCaptureUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, SpriteCaptureUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "SpriteCaptureUI::Serialize";
    SpriteCaptureUI_Serialize((SpriteCaptureUI*)self);
}

typedef void (*fn_OnCreate_t)(void* self);
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "SpriteCaptureUI::OnCreate";
    SpriteCaptureUI_OnCreate((SpriteCaptureUI*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallSpriteCaptureUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013fc0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[SpriteCaptureUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveSpriteCaptureUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallSpriteCaptureUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140079640);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[SpriteCaptureUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveSpriteCaptureUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallSpriteCaptureUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400793a0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 17);
#ifdef _DEBUG
    printf("[SpriteCaptureUI] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveSpriteCaptureUIOnCreate() { RemoveHook(&g_hookOnCreate); }
