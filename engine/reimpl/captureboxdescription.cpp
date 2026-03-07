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
// Reimplementations
// ============================================================

// CaptureBoxDescription::Dtor (0x140014010)
// Sets vtable, destroys String at +0x70, calls ElementDescription dtor, optionally frees.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CaptureBoxDescription::Dtor";
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015d4a0);
    fn_String_Dtor((void*)((uintptr_t)self + 0x70));
    fn_ElementDescription_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// GetSerializeVersion: calls ElementDescription::GetFileVersion() and ORs with 0x100
// to indicate that this description class has a custom serialize format (version bit 8).
// Original at 0x140079210: SUB RSP,28h; CALL [GetFileVersion]; BTS EAX,8; ADD RSP,28h; RET
static uint32_t Reimpl_Slot3(void* self) {
    g_lastHook = "CaptureBoxDescription::Slot3";
    uint32_t ver = fn_ElementDescription_GetFileVersion(self);
    return ver | 0x100u;
}

// CaptureBoxDescription::Serialize (0x1400791a0)
// Calls base Serialize, then serializes ExternalPtr at +0x68 and String at +0x70.
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CaptureBoxDescription::Serialize";
    uint8_t serBuf[24];
    fn_ElementDescription_Serialize(self, stream);
    g_fn_Serializer_Ctor(serBuf, stream, 0);
    fn_ExternalPtr_Serialize((void*)((uintptr_t)self + 0x68), serBuf);
    g_fn_String_Serialize((void*)((uintptr_t)self + 0x70), serBuf);
    g_fn_Serializer_Dtor(serBuf);
}

// CaptureBoxDescription::Cleanup (0x140013e40)
// Virtual dtor (no-free), then re-constructs as default CaptureBoxDescription.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CaptureBoxDescription::Cleanup";
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    fn_ElementDescription_Ctor(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015d4a0);
    *(int64_t*)((uintptr_t)self + 0x68) = 0;
    *(int64_t*)((uintptr_t)self + 0x70) = 0;
}

// CaptureBoxDescription::CreateElement (0x1400790a0)
// Allocates CaptureBoxElement (0xe0 bytes), constructs it, sets extra fields to 0,
// wraps in InternalBasePtr, calls CreateInstance, copies Sprite from description.
static void Reimpl_CreateElement(void* self, void* outPtr) {
    g_lastHook = "CaptureBoxDescription::CreateElement";
    void* elem = fn_MemAlloc(g_pMemAllocSingleton, 0xe0, 8);
    if (elem) {
        fn_Element_Ctor(elem);
        *(void**)elem = (void*)ResolveAddr(Module::Exe, 0x14015d440);
        fn_Sprite_DefaultCtor2((void*)((uintptr_t)elem + 0xc0));
        *(int64_t*)((uintptr_t)elem + 0xd0) = 0;
        *(int64_t*)((uintptr_t)elem + 0xd8) = 0;
    }
    // InternalBasePtr is a 16-byte struct; first field is the element pointer.
    // After Ctor: localPtr[0] = elem (wrapped).
    // CreateInstance may update localPtr[0] to the fully initialized element.
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
    *(void**)outPtr = localPtr[0];
}

// CaptureBoxDescription::CreateInPlace (0x140013df0)
// Default-constructs a CaptureBoxDescription in place (no alloc).
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CaptureBoxDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_ElementDescription_Ctor(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x14015d4a0);
    *(int64_t*)((uintptr_t)self + 0x68) = 0;
    *(int64_t*)((uintptr_t)self + 0x70) = 0;
    return self;
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
