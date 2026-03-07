// engine/reimpl/uicomponent.cpp
// Reimplementation of UIComponent lifecycle methods from Application-x64.exe.
//
// UIComponent (0x38 bytes) manages UI overlay data for entities.
// Fields: +0x28 = array ptr (zeroed), +0x30 = array data (zeroed).
//
// Vtable at 0x1401649f0. Non-trivial overrides:
//   Shutdown  (0x14012dd00) — Serializes array count + bulk data via Serializer
//   Serialize (0x14012dca0) — Reads building data via IsDerivedFrom, resizes array
//
// Note: Draw, OnCreate, OnRemove, Update, Slot10, OnUpdate2 are all no-ops.
//
// Shutdown decompilation (0x14012dd00):
//   Component_Serialize(self, stream)
//   Create Serializer(stream, version=0)
//   local = *(int*)(self+0x30)   // count
//   Serialize(local)
//   If loading: FUN_14000a590(self+0x28, local)  // resize array
//   Serialize(local, *(uchar**)(self+0x28))      // bulk serialize array data
//   ~Serializer()
//
// Serialize decompilation (0x14012dca0):
//   Get building data via IsDerivedFrom(self+0x18, DAT_1401c7f90)
//   FUN_14000a590(self+0x28, *(int*)(building+0x20))  // resize to building's count
//   FUN_14003f010(*(ptr*)(self+0x28), 0, count)       // zero-fill
//
// Strategy: Clean reimplementations. Both functions fully converted.
//
// Prologue sizes:
//   Shutdown:  15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 15 (MOV [RSP+8],RBX + MOV [RSP+0x10],RSI + PUSH RDI + SUB RSP,0x20)

#include "UIComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;

// ============================================================
// Reimplementations
// ============================================================

// UIComponent::Shutdown (0x14012dd00)
// Serializes the UI array: count (int) + bulk data.
// On load, resizes the array before reading. On save, just serializes in-place.
// "Is loading" check: *(int*)(serializer_vtable + 8) == 0
static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "UIComponent::Shutdown";
    uint8_t serBuf[24];
    fn_ComponentBase_Serialize(self, stream);
    g_fn_Serializer_Ctor(serBuf, stream, 0);
    int count = *(int32_t*)((uintptr_t)self + 0x30);
    g_fn_Serialize_Int(serBuf, &count, nullptr);
    // Loading check: first qword of serializer is vtable; *(int*)(vtable+8)==0 means loading
    int64_t vtable = *(int64_t*)serBuf;
    if (*(int32_t*)(vtable + 8) == 0) {
        fn_DynArray_Resize((void*)((uintptr_t)self + 0x28), count);
    }
    void* arrayPtr = *(void**)((uintptr_t)self + 0x28);
    fn_Serializer_SerializeBulk(serBuf, count, arrayPtr, nullptr);
    g_fn_Serializer_Dtor(serBuf);
}

// UIComponent::Serialize (0x14012dca0)
// Looks up building data via IsDerivedFrom, then resizes and zeroes the array.
static void Reimpl_Serialize(void* self) {
    g_lastHook = "UIComponent::Serialize";
    void* base = *(void**)((uintptr_t)self + 0x18);
    bool ok = fn_IsDerivedFrom(base, *g_TypeId_UIDescription);
    void* pBVar2 = ok ? base : nullptr;
    int count = *(int32_t*)((uintptr_t)(uintptr_t)pBVar2 + 0x20);
    fn_DynArray_Resize((void*)((uintptr_t)self + 0x28), count);
    void* arrayPtr = *(void**)((uintptr_t)self + 0x28);
    fn_MemZeroFill(arrayPtr, 0, count);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallUIComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012dd00);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[UIComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveUIComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallUIComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012dca0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[UIComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveUIComponentSerialize() { RemoveHook(&g_hookSerialize); }
