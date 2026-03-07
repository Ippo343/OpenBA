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
// Reimplementations
// ============================================================

// FrustumOutlineDescription::Dtor (0x140012500)
// Sets vtable, destroys String at +0x70, calls ElementDescription dtor, optionally frees.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "FrustumOutlineDescription::Dtor";
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015c300);
    fn_String_Dtor((void*)((uintptr_t)self + 0x70));
    fn_ElementDescription_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// GetSerializeVersion: calls ElementDescription::GetFileVersion() and ORs with 0x100
// to indicate a custom serialize format version bit.
// Original at 0x14006c500: SUB RSP,28h; CALL [GetFileVersion]; BTS EAX,8; ADD RSP,28h; RET
static uint32_t Reimpl_Slot3(void* self) {
    g_lastHook = "FrustumOutlineDescription::Slot3";
    uint32_t ver = fn_ElementDescription_GetFileVersion(self);
    return ver | 0x100u;
}

// FrustumOutlineDescription::Serialize (0x14006c460)
// Calls base Serialize, then serializes ExternalPtr at +0x68, String at +0x70,
// and two ints at +0x78 and +0x7c.
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "FrustumOutlineDescription::Serialize";
    uint8_t serBuf[24];
    fn_ElementDescription_Serialize(self, stream);
    g_fn_Serializer_Ctor(serBuf, stream, 0);
    fn_ExternalPtr_Serialize((void*)((uintptr_t)self + 0x68), serBuf);
    g_fn_String_Serialize((void*)((uintptr_t)self + 0x70), serBuf);
    g_fn_Serialize_Int(serBuf, (void*)((uintptr_t)self + 0x78), nullptr);
    g_fn_Serialize_Int(serBuf, (void*)((uintptr_t)self + 0x7c), nullptr);
    g_fn_Serializer_Dtor(serBuf);
}

// FrustumOutlineDescription::Cleanup (0x140012390)
// Virtual dtor (no-free), re-constructs as default FrustumOutlineDescription.
// Extra: *(int64*)(self+0x78) = -1 (vs CaptureBox which zeroes +0x68 and +0x70 only)
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "FrustumOutlineDescription::Cleanup";
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    fn_ElementDescription_Ctor(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015c300);
    *(int64_t*)((uintptr_t)self + 0x68) = 0;
    *(int64_t*)((uintptr_t)self + 0x70) = 0;
    *(int64_t*)((uintptr_t)self + 0x78) = (int64_t)-1;
}

// FrustumOutlineDescription::CreateElement (0x14006c340)
// Allocates FrustumOutlineElement (0xd8 bytes), constructs, sets +0xd0 = -1,
// wraps in InternalBasePtr, calls CreateInstance, copies Sprite from description,
// then copies two ints from description (+0x78, +0x7c) into element (+0xd0, +0xd4).
static void Reimpl_CreateElement(void* self, void* outPtr) {
    g_lastHook = "FrustumOutlineDescription::CreateElement";
    void* elem = fn_MemAlloc(g_pMemAllocSingleton, 0xd8, 8);
    if (elem) {
        fn_Element_Ctor(elem);
        *(void**)elem = (void*)ResolveAddr(Module::Exe, 0x14015c2a0);
        fn_Sprite_DefaultCtor2((void*)((uintptr_t)elem + 0xc0));
        *(int64_t*)((uintptr_t)elem + 0xd0) = (int64_t)-1;
    }
    void* localPtr[2] = {nullptr, nullptr};
    fn_InternalBasePtr_Ctor(localPtr, elem);
    fn_ElementDescription_CreateInstance(self, localPtr);
    fn_InternalBasePtr_CheckPtr(localPtr);
    void* pElem = localPtr[0];
    uint64_t spriteSheet = *(uint64_t*)((uintptr_t)self + 0x68);
    void* spriteName = (void*)((uintptr_t)self + 0x70);
    uint64_t spriteRetval[2] = {0, 0};
    fn_Sprite_Sprite(spriteRetval, spriteSheet, spriteName);
    *(uint64_t*)((uintptr_t)pElem + 0xc0) = spriteRetval[0];
    *(uint64_t*)((uintptr_t)pElem + 0xc8) = spriteRetval[1];
    fn_InternalBasePtr_CheckPtr(localPtr);
    *(int32_t*)((uintptr_t)localPtr[0] + 0xd0) = *(int32_t*)((uintptr_t)self + 0x78);
    fn_InternalBasePtr_CheckPtr(localPtr);
    *(int32_t*)((uintptr_t)localPtr[0] + 0xd4) = *(int32_t*)((uintptr_t)self + 0x7c);
    *(void**)outPtr = localPtr[0];
}

// FrustumOutlineDescription::CreateInPlace (0x140012330)
// Default-constructs a FrustumOutlineDescription in place.
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "FrustumOutlineDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_ElementDescription_Ctor(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015c300);
    *(int64_t*)((uintptr_t)self + 0x68) = 0;
    *(int64_t*)((uintptr_t)self + 0x70) = 0;
    *(int64_t*)((uintptr_t)self + 0x78) = (int64_t)-1;
    return self;
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
