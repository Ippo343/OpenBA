// code/Game/Component/wellcomponent.cpp
// Clean reimplementation of WellComponent lifecycle methods from Application-x64.exe.
//
// WellComponent (0x48 bytes) extends ControllerComponent. No additional fields.
// Vtable at 0x14015ae90. Two non-trivial overrides:
//   Shutdown  (0x14005cfa0) — Two empty serializer blocks
//   OnCreate  (0x14005cf60) — Registers entity with population manager
//

#include "Component/wellcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// FUN_140091ba0 — on entity (un)register, scan the manager's entity array for the
// matching IBP and, if found, clear its natural-resource grid mark + null the slot.
// (Despite the legacy "ArrayPushEntity" name, it is a remove-by-IBP, not a push.)
typedef void (*fn_ArrayPushEntity_t)(void* manager, void* internalBasePtr);
static fn_ArrayPushEntity_t s_fn_ArrayPushEntity = nullptr;

// Game world/population manager singleton
static void** s_pWorldManager = nullptr;

static bool s_initialized = false;

// ============================================================
// WellComponent::Shutdown (vtable slot [3], 0x14005cfa0)
// ============================================================
// Component_Serialize + two empty serializer blocks (controller compat).

void WellComponent_Shutdown(WellComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (component section)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer1);

    // Second empty serializer block (controller section)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// WellComponent::OnCreate (vtable slot [7], 0x14005cf60)
// ============================================================
// Registers the owning entity with the world manager's water source list.

void WellComponent_OnCreate(WellComponent* self) {
    // ⚠ FIXED 2026-08-22 (fire lane): retail 0x14005cf60 is
    //     InternalBasePtr::InternalBasePtr(&local, param_1)   // param_1 == THIS
    //     FUN_140091ba0(*(gw + 0xf0), &local)
    // — it wraps the COMPONENT, not the owner entity. This passed `self->owner`,
    // so the IBP handed to 0x140091ba0 identified the wrong object and its
    // scan-for-matching-IBP could never hit. Latent while WellComponent was inert
    // (slot 7 was never installed); it matters now that the FireWork spiral search
    // depends on wells being findable in the gw+0xf0 entity index.
    uint8_t basePtr[24];
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(basePtr, self);

    // Push to world manager's water source array at +0xF0
    if (s_pWorldManager && *s_pWorldManager && s_fn_ArrayPushEntity) {
        void* manager = *s_pWorldManager;
        void* waterArray = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(manager) + 0xF0);
        s_fn_ArrayPushEntity(waterArray, basePtr);
    }
}

// FUN_140004680 — MapComponent/resData finder used by the remove scan (registered).
typedef void* (*fn_FindMapResData_t)(int64_t resData, void* outIBP);
static fn_FindMapResData_t s_fn_FindMapResData = nullptr;

// ============================================================
// Reimpl of FUN_1400889e0 (Component reg wave B — WELL cluster).
// Pure leaf (no exe callees). Compacts the 4-byte-stride int array of one terrain
// grid cell, deleting every entry equal to `value`. The cell is located from the
// world map width (*(int*)(*(int**)(world+0xf8)) >> 3 = cells-per-row) and the
// 8-px-quantised (x,y): cellIndex = (width>>3 ... already shifted)*(y>>3)+(x>>3);
// cell record stride 0x10 lives at manager+0x140; record = {int* data@+0, int count@+8}.
// Faithful 1:1 port of the Ghidra decompile (same quirky shift-and-overwrite compaction).
// ============================================================
void WellComp_RemoveCellEntry_889e0(int64_t manager, int x, int y, int value) {
    void* world = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!world) return;
    int width = *reinterpret_cast<int*>(*reinterpret_cast<int**>(
                    reinterpret_cast<uint8_t*>(world) + 0xf8)) >> 3;
    int64_t* cell = reinterpret_cast<int64_t*>(
        (int64_t)((width * (y >> 3)) + (x >> 3)) * 0x10 +
        *reinterpret_cast<int64_t*>(manager + 0x140));
    int count = (int)cell[1];
    if (count <= 0) return;

    int i = 0;
    int64_t byteOff = 0;
    do {
        if (*reinterpret_cast<int*>(byteOff + cell[0]) == value) {
            int64_t shiftOff = byteOff;
            int j = i;
            if (i < count) {
                do {
                    j++;
                    if (j < (int)cell[1]) {
                        int* p = reinterpret_cast<int*>(cell[0] + shiftOff);
                        if (p) *p = p[1];
                    }
                    shiftOff += 4;
                } while (j < (int)cell[1]);
            }
            *reinterpret_cast<int*>(&cell[1]) = (int)cell[1] - 1;
            i--;
            byteOff -= 4;
        }
        count = (int)cell[1];
        i++;
        byteOff += 4;
    } while (i < count);
}

// ============================================================
// Reimpl of FUN_140091ba0 (Component reg wave B — WELL cluster).
// On (un)register, scan the manager's IBP array {ptr@+0xe0, count@+0xe8} for the entry
// whose resolved value equals *param2 (the owner IBP). When found: resolve the entry's
// MapComponent/resData (FUN_140004680, registered) for its tile (short x@+0x28, y@+0x2a),
// clear the natural-resource grid mark via WellComp_RemoveCellEntry_889e0(manager, x, y,
// -1-index), then null the slot with a fresh InternalBasePtr. Closure-safe: FUN_140004680
// (reg) + FUN_1400889e0 (reg below) + CheckPtr/IBP-ctor (engine). 1:1 port.
// ============================================================
void WellComp_UnregisterEntity_91ba0(int64_t manager, void* param2) {
    if (*reinterpret_cast<int*>(manager + 0xe8) <= 0) return;
    int64_t off = 0;
    int idx = 0;
    do {
        int64_t arr = *reinterpret_cast<int64_t*>(manager + 0xe0);
        fn_InternalBasePtr_CheckPtr(param2);
        if (*reinterpret_cast<int64_t*>(arr + off) == *reinterpret_cast<int64_t*>(param2)) {
            if (idx < 0) return;
            int64_t* slot = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(manager + 0xe0) + (int64_t)idx * 8);
            fn_InternalBasePtr_CheckPtr(slot);
            int64_t entry = *reinterpret_cast<int64_t*>(*slot + 0x10);
            fn_InternalBasePtr_CheckPtr(&entry);
            int64_t resData = 0;
            if (s_fn_FindMapResData) s_fn_FindMapResData(entry, &resData);
            fn_InternalBasePtr_CheckPtr(&resData);
            int rx = (int)*reinterpret_cast<int16_t*>(resData + 0x28);
            int ry = (int)*reinterpret_cast<int16_t*>(resData + 0x2a);
            WellComp_RemoveCellEntry_889e0(manager, rx, ry, -1 - idx);
            int64_t cleared = 0;
            fn_InternalBasePtr_Ctor(&cleared, nullptr);
            *reinterpret_cast<int64_t*>((int64_t)idx * 8 + *reinterpret_cast<int64_t*>(manager + 0xe0)) = cleared;
            return;
        }
        idx++;
        off += 8;
    } while (idx < *reinterpret_cast<int*>(manager + 0xe8));
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WellComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_fn_ArrayPushEntity = reinterpret_cast<fn_ArrayPushEntity_t>(
        ResolveAddr(Module::Exe, 0x140091ba0));
    s_fn_FindMapResData = reinterpret_cast<fn_FindMapResData_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(WellComponent, 0x48, 0x82)
REGISTER_TYPE(WellComponent, "ControllerComponent",
              &WellComponent_Factory, &WellComponent_CreateInPlace);
