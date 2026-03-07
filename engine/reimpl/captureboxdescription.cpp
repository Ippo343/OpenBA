// engine/reimpl/captureboxdescription.cpp
// Reimplementation of CaptureBoxDescription vtable methods from Application-x64.exe.
//
// CaptureBoxDescription (0x78 bytes), vtable at 0x14015d4a0.
// Extends ElementDescription (Object::External base, 0x20 bytes).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140014010 — Slot0 Dtor(self, flags)
//   +0x28: 0x1400791a0 — Slot5 Serialize(self, stream)
//   +0x30: 0x140013e40 — Slot6 Cleanup(self)
//   +0x40: 0x1400790a0 — Slot8 CreateElement(self, stream)
//   +0x48: 0x140013df0 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x140013d80 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140013d70 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140079210 — Slot3 GetSerializeVersion (special pattern)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;
static HookInfo g_hookSlot3;
static HookInfo g_hookCreateElement;
static HookInfo g_hookCreateInPlace;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CaptureBoxDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// GetSerializeVersion: calls ElementDescription::GetFileVersion() and ORs with 0x100
// to indicate that this description class has a custom serialize format (version bit 8).
// Original at 0x140079210: SUB RSP,28h; CALL [GetFileVersion]; BTS EAX,8; ADD RSP,28h; RET
// Disabled because the prologue contains a RIP-relative CALL at offset 4 (hookable with clean reimpl).
static uint32_t Reimpl_Slot3(void* self) {
    g_lastHook = "CaptureBoxDescription::Slot3";
    uint32_t ver = fn_ElementDescription_GetFileVersion(self);
    return ver | 0x100u;
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CaptureBoxDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CaptureBoxDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

typedef void (*fn_CreateElement_t)(void* self, void* stream);
static void Reimpl_CreateElement(void* self, void* stream) {
    g_lastHook = "CaptureBoxDescription::CreateElement";
    ((fn_CreateElement_t)g_hookCreateElement.trampoline)(self, stream);
}

typedef void* (*fn_CreateInPlace_t)(void* self);
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CaptureBoxDescription::CreateInPlace";
    return ((fn_CreateInPlace_t)g_hookCreateInPlace.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCaptureBoxDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014010);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCaptureBoxDescriptionSlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140079210);
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 14);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionSlot3() { RemoveHook(&g_hookSlot3); }

void InstallCaptureBoxDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400791a0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCaptureBoxDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013e40);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallCaptureBoxDescriptionCreateElement() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400790a0);
    InstallHook(target, (void*)Reimpl_CreateElement, &g_hookCreateElement, 15);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] CreateElement hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionCreateElement() { RemoveHook(&g_hookCreateElement); }

void InstallCaptureBoxDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013df0);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[CaptureBoxDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
