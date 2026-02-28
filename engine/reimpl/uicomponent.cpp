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
// Strategy: Passthrough wrappers. Serialize interacts with building data lookup.
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
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Serialize(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookSerialize.trampoline)(self);
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
