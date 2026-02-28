// engine/reimpl/ambientemittercomponent.cpp
// Reimplementation of AmbientEmitterComponent lifecycle methods from Application-x64.exe.
//
// AmbientEmitterComponent (0x48 bytes) manages ambient sound emitters for entities
// (e.g., river sounds, fire crackle). Fields: +0x28=emitter data ptr, +0x30=count,
// +0x38=emitter array, +0x40=audio data.
//
// Vtable at 0x140158a60. Non-trivial overrides:
//   Shutdown  (0x140042f30) — Complex (72 instructions): Serializes Vector3 position (+0x28),
//     emitter array with version-gated loading, paired (InternalBasePtr, uint, float) entries,
//     version check for array resize from building data
//   Draw      (0x140042ba0) — Reads building data, resizes emitter list, calls audio setup
//   OnCreate  (0x140042b90) — Not defined as function in Ghidra (inline thunk), skip
//
// Note: Serialize, OnRemove, Update, Slot10, OnUpdate2 are all base no-ops.
//
// Shutdown decompilation (0x140042f30, 72 instructions):
//   Component_Serialize(self, stream)
//   Serializer(stream, version=1)
//   Vector3::Serialize(self+0x28)
//   local_count = *(uint*)(self+0x40)
//   Serialize(local_count)
//   If loading: resize array at self+0x38
//   Loop: Serialize InternalBasePtr + uint + float per entry
//   If version==0: resize from building data (+0x30)
//   ~Serializer()
//
// Draw decompilation (0x140042ba0, 24 instructions):
//   Get building data via IsDerivedFrom(self+0x18, DAT_1401c3e98)
//   Resize emitter list: FUN_140001770(self+0x38, building[0x30])
//   If building[0x20] != 0: call audio setup FUN_140042e10(self)
//
// Strategy: Passthrough wrappers. Both methods interact with audio subsystem and
// building data lookup.
//
// Prologue sizes:
//   Shutdown: 15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Draw:     15 (MOV [RSP+8],RBX + MOV [RSP+0x10],RSI + PUSH RDI + SUB RSP,0x20)

#include "AmbientEmitterComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;

// ============================================================
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Draw(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAmbientEmitterComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042f30);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[AmbientEmitterComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveAmbientEmitterComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallAmbientEmitterComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042ba0);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 15);
#ifdef _DEBUG
    printf("[AmbientEmitterComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveAmbientEmitterComponentDraw() { RemoveHook(&g_hookDraw); }
