// engine/reimpl/frustumoutlinedescription.cpp
// Reimplementation of FrustumOutlineDescription vtable methods from Application-x64.exe.
//
// FrustumOutlineDescription (0x80 bytes), vtable at 0x14015c300.
// Extends ElementDescription (Object::External base, 0x20 bytes).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140012500 — Slot0 Dtor(self, flags)
//   +0x28: 0x14006c460 — Slot5 Serialize(self, stream)
//   +0x30: 0x140012390 — Slot6 Cleanup(self)
//   +0x40: 0x14006c340 — Slot8 CreateElement(self, stream)
//   +0x48: 0x140012330 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x1400122b0 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x1400122a0 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14006c500 — Slot3 GetSerializeVersion (special pattern)
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
    g_lastHook = "FrustumOutlineDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef int (*fn_Slot3_t)(void* self);
static int Reimpl_Slot3(void* self) {
    g_lastHook = "FrustumOutlineDescription::Slot3";
    return ((fn_Slot3_t)g_hookSlot3.trampoline)(self);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "FrustumOutlineDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "FrustumOutlineDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

typedef void (*fn_CreateElement_t)(void* self, void* stream);
static void Reimpl_CreateElement(void* self, void* stream) {
    g_lastHook = "FrustumOutlineDescription::CreateElement";
    ((fn_CreateElement_t)g_hookCreateElement.trampoline)(self, stream);
}

typedef void* (*fn_CreateInPlace_t)(void* self);
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "FrustumOutlineDescription::CreateInPlace";
    return ((fn_CreateInPlace_t)g_hookCreateInPlace.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFrustumOutlineDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012500);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallFrustumOutlineDescriptionSlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006c500);
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 14);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionSlot3() { RemoveHook(&g_hookSlot3); }

void InstallFrustumOutlineDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006c460);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallFrustumOutlineDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012390);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallFrustumOutlineDescriptionCreateElement() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006c340);
    InstallHook(target, (void*)Reimpl_CreateElement, &g_hookCreateElement, 15);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] CreateElement hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionCreateElement() { RemoveHook(&g_hookCreateElement); }

void InstallFrustumOutlineDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012330);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[FrustumOutlineDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
