// engine/reimpl/trackercomponent.cpp
// Reimplementation of TrackerComponent lifecycle methods from Application-x64.exe.
//
// TrackerComponent (0x50 bytes) manages tracking data for entities (probably for
// path/resource tracking). Fields: +0x28=array1 ptr, +0x30=array1 count(?),
// +0x38=array2 ptr, +0x40=array2 count(?), +0x48=trackerId (init -1).
//
// Vtable at 0x14015ab80. Non-trivial overrides:
//   Shutdown (0x140059130) — Only non-trivial method. Serializes two arrays of
//     paired (InternalBasePtr, int) entries at +0x28/+0x30 and +0x38/+0x40,
//     plus the trackerId at +0x48.
//
// Note: All other vtable slots (Serialize, Draw, OnCreate, OnRemove, Update, etc.)
// are base no-ops.
//
// Shutdown decompilation (0x140059130, 81 instructions):
//   Component_Serialize(self, stream)
//   Serializer(stream, version=0)
//   local_count = *(uint*)(self+0x30)
//   Serialize(local_count)
//   If loading: resize array at self+0x28
//   Loop array1: Serialize InternalBasePtr + int pairs
//   local_count = *(uint*)(self+0x40)
//   Serialize(local_count)
//   If loading: resize array at self+0x38
//   Loop array2: Serialize InternalBasePtr + int pairs
//   Serialize(self+0x48)   // trackerId
//   ~Serializer()
//
// Strategy: Passthrough wrapper. The array serialization is complex with
// paired InternalBasePtr + int entries.
//
// Prologue size:
//   Shutdown: 15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)

#include "TrackerComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TrackerComponent::Shutdown";
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTrackerComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140059130);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TrackerComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTrackerComponentShutdown() { RemoveHook(&g_hookShutdown); }
