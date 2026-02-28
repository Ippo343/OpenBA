// engine/reimpl/buildcomponent.cpp
// Reimplementation of BuildComponent lifecycle methods from Application-x64.exe.
//
// BuildComponent — ControllerComponent subclass (0x78 bytes, largest in this batch).
// Fields: buildState (uint32 +0x48), buildProgress (uint32 +0x4C, init 1),
//         buildFlag (byte +0x50), field58/60/68/70 (entity arrays).
// Manages building construction: resource tracking, worker assignments, progress.
//
// Vtable at 0x140162260:
//   [3] Shutdown        = 0x140108D80  <-- PASSTHROUGH (complex serialize with entity arrays)
//   [4] Serialize       = 0x140108D60  <-- 3-byte RET 0 (unhookable no-op)
//   [6] Draw            = 0x140108D40  <-- PASSTHROUGH (complex: checks game flags, iterates
//                                         multi-object children, finds BuildComponents and
//                                         StorageComponents, manages worker assignments)
//   [7] OnCreate        = 0x140108CB0  <-- HOOKED (remove from removal queue)
//  [11] OnUpdate2       = 0x140108D70  <-- 3-byte RET 0 (unhookable no-op)
//   Other slots: NO-OP

#include "BuildComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

// InternalBasePtr::InternalBasePtr(this, Internal*) — dispatch [0x140152a90]
typedef void (*fn_InternalBasePtr_Ctor_t)(void* self, void* internal);
static fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;

// FUN_140017550 — find index in removal queue (returns -1 if not found)
typedef int (*fn_FindInQueue_t)(void* arrayBase, void* entityPtr);
static fn_FindInQueue_t fn_FindInQueue = nullptr;

static void** g_pGameState = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x140108D80) — PASSTHROUGH
// Complex serialization: Component_Serialize, empty controller block,
// then versioned block (v1) with buildState, buildProgress, buildFlag,
// two InternalBasePtr arrays (field58/60 and field68/70) each with
// IsDerivedFrom validation. Too many entity array management deps.
// ============================================================
static void Passthrough_Shutdown(BuildComponent* self, void* stream) {
    typedef void (*orig_t)(BuildComponent*, void*);
    ((orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Draw (0x140108D40) — PASSTHROUGH
// Complex construction management: checks game flags bit 0 at
// GameState+0xD0, iterates multi-object hierarchy, finds worker
// BuildComponents and StorageComponents on child entities,
// manages draw calls and worker assignments. Very complex (body=22
// bytes for the gate function, calls into 0x140108720 which is huge).
// ============================================================
static void Passthrough_Draw(BuildComponent* self) {
    typedef void (*orig_t)(BuildComponent*);
    ((orig_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// OnCreate (0x140108CB0)
// Removes entity from the removal queue. Creates InternalBasePtr
// from owner, searches the removal queue (GameState+0xF0+0xD0)
// for this entity. If found, shifts remaining entries down and
// decrements count.
// ============================================================
static void Reimpl_OnCreate(BuildComponent* self) {
    // Create InternalBasePtr on stack
    uint8_t ptrBuf[8];
    fn_InternalBasePtr_Ctor(ptrBuf, self->owner);

    void* gameState = *g_pGameState;
    void* worldData = *(void**)((uintptr_t)gameState + 0xF0);

    // Find entity index in removal queue
    int idx = fn_FindInQueue((void*)((uintptr_t)worldData + 0xD0), ptrBuf);

    if (idx >= 0) {
        int count = *(int*)((uintptr_t)worldData + 0xD8);

        // Shift remaining elements down (each element is 8 bytes / InternalBasePtr)
        if (idx < count) {
            int64_t offset = (int64_t)idx * 8;
            do {
                int nextIdx = idx + 1;
                if (nextIdx < count) {
                    uint64_t* pSlot = (uint64_t*)(*(uintptr_t*)((uintptr_t)worldData + 0xD0) + offset);
                    if (pSlot != nullptr) {
                        *pSlot = 0;
                        *pSlot = pSlot[1];
                    }
                }
                idx = nextIdx;
                offset += 8;
            } while (idx < count);
        }

        // Decrement count
        *(int*)((uintptr_t)worldData + 0xD8) = count - 1;
    }
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_FindInQueue = reinterpret_cast<fn_FindInQueue_t>(
        ResolveAddr(Module::Exe, 0x140017550));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a90)));

    return fn_InternalBasePtr_Ctor && fn_FindInQueue;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140108D80);
    InstallHook(target, (void*)Passthrough_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[BuildComponent] Shutdown hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveBuildComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallBuildComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140108D40);
    InstallHook(target, (void*)Passthrough_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[BuildComponent] Draw hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveBuildComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallBuildComponentOnCreate() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[BuildComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140108CB0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[BuildComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
