// engine/reimpl/multiobjectcomponent.cpp
// Reimplementation of MultiObjectComponent lifecycle methods from Application-x64.exe.
//
// MultiObjectComponent — ControllerComponent subclass (0x60 bytes).
// Fields: field48 (array ptr), field50 (array count), multiCount (uint32, init 1).
// Manages multi-part objects (e.g. multi-tile buildings that span several entities).
//
// Vtable at 0x140163cf0:
//   [3] Shutdown        = 0x140125880  <-- PASSTHROUGH (complex serialize with entity validation)
//   [7] OnCreate        = 0x1401257C0  <-- HOOKED (mark child entities for removal)
//   Other slots: NO-OP (Draw=base, OnRemove, Update, Slot10, OnUpdate2)

#include "MultiObjectComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

// InternalBasePtr::CheckPtr — dispatched through [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// FUN_1400c2ea0 — mark entity for removal
typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t fn_MarkForRemoval = nullptr;

// FUN_140003110 — dynamic array push (add entity to removal queue)
typedef void (*fn_ArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
static fn_ArrayPush_t fn_ArrayPush = nullptr;

static void** g_pGameState = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x140125880) — PASSTHROUGH
// Complex serialization: serializes InternalBasePtr array with
// entity validation (IsDerivedFrom checks for ComponentEntity),
// removes invalid entities from array on load, serializes multiCount.
// Too many dependencies for clean reimpl.
// ============================================================
static void Passthrough_Shutdown(MultiObjectComponent* self, void* stream) {
    g_lastHook = "MultiObjectComponent::Shutdown";
    typedef void (*orig_t)(MultiObjectComponent*, void*);
    ((orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// OnCreate (0x1401257C0)
// Iterates child entity array at +0x48/+0x50. For each valid
// child entity, checks bit 10 of entity flags. If not already
// marked, calls MarkForRemoval and adds to removal queue.
// ============================================================
static void Reimpl_OnCreate(MultiObjectComponent* self) {
    g_lastHook = "MultiObjectComponent::OnCreate";
    int count = *(int*)((uintptr_t)self + 0x50);
    void** array = *(void***)((uintptr_t)self + 0x48);

    for (int i = 0; i < count; i++) {
        void* entityPtr = array[i];
        void* gameWorld = *(void**)((uintptr_t)*g_pGameState + 0xE8);

        if (entityPtr != nullptr) {
            fn_CheckPtr(&array[i]);

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                fn_CheckPtr(&array[i]);
                fn_MarkForRemoval(entityPtr);

                int removalCount = *(int*)((uintptr_t)gameWorld + 0xD8);
                fn_ArrayPush(
                    (void*)((uintptr_t)gameWorld + 0xD0),
                    removalCount + 1,
                    &array[i]
                );
            }
        }
    }
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    fn_ArrayPush = reinterpret_cast<fn_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ab0)));

    return fn_CheckPtr && fn_MarkForRemoval && fn_ArrayPush;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMultiObjectComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140125880);
    InstallHook(target, (void*)Passthrough_Shutdown, &g_hookShutdown, 18);
#ifdef _DEBUG
    printf("[MultiObjectComponent] Shutdown hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveMultiObjectComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMultiObjectComponentOnCreate() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[MultiObjectComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401257C0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[MultiObjectComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveMultiObjectComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
