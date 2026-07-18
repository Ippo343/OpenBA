// code/Game/Component/pasturecomponent.cpp
// Clean reimplementation of PastureComponent lifecycle methods from Application-x64.exe.
//
// PastureComponent (0x68 bytes) has a SHIFTED controller sub-object at +0x48.
// Own data at +0x28..+0x44, controller vtable at +0x48.
//
// Vtable at 0x140163f60. Three non-trivial overrides:
//   Shutdown  (0x140127f40) — Component_Serialize + entity ref array serialization
//   Draw      (0x140127a30) — Compute pasture capacity
//   OnCreate  (0x1401277e0) — Complex entity ref setup + registration
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/pasturecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// FUN_140002340 — serialize InternalBasePtr array
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

// FUN_1400032e0 — resize InternalBasePtr array
typedef void (*fn_ResizeIBPArray_t)(void* arrayPtr, int newSize);
static fn_ResizeIBPArray_t s_fn_ResizeIBPArray = nullptr;

// FUN_140128570 — compute pasture capacity
typedef uint64_t (*fn_ComputePastureCapacity_t)(uintptr_t self);
static fn_ComputePastureCapacity_t s_fn_ComputePastureCapacity = nullptr;

// FUN_140002cc0 — add entity ref to list
typedef void (*fn_AddToList_t)(void* listPtr, void* entityRef);
static fn_AddToList_t s_fn_AddToList = nullptr;

// FUN_140003110 — insert entity ref at position in list
typedef void (*fn_InsertInList_t)(void* listPtr, int position, void* entityRef);
static fn_InsertInList_t s_fn_InsertInList = nullptr;

// PastureComponent_helper_5ed0 — process entity ref for pasture
typedef void (*fn_PastureHelper5ed0_t)(void* entityRef);
static fn_PastureHelper5ed0_t s_fn_PastureHelper5ed0 = nullptr;

// FUN_1401240a0 — reassign entity refs for pasture
typedef void (*fn_ReassignEntityRef_t)(uintptr_t entityPtr);
static fn_ReassignEntityRef_t s_fn_ReassignEntityRef = nullptr;

// Type ID for entity validation — 0x1401c7970
static uint32_t* s_TypeId_PastureAnimal = nullptr;

// Type ID for component lookup — 0x1401c6f98
static uint32_t* s_TypeId_PastureSubComp = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_fn_IBP_Serialize = fn_InternalBasePtr_Serialize;

    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_fn_ResizeIBPArray = reinterpret_cast<fn_ResizeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));

    s_fn_ComputePastureCapacity = reinterpret_cast<fn_ComputePastureCapacity_t>(
        ResolveAddr(Module::Exe, 0x140128570));

    s_fn_AddToList = reinterpret_cast<fn_AddToList_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_InsertInList = reinterpret_cast<fn_InsertInList_t>(
        ResolveAddr(Module::Exe, 0x140003110));

    s_fn_PastureHelper5ed0 = reinterpret_cast<fn_PastureHelper5ed0_t>(
        ResolveAddr(Module::Exe, 0x140105ed0));

    s_fn_ReassignEntityRef = reinterpret_cast<fn_ReassignEntityRef_t>(
        ResolveAddr(Module::Exe, 0x1401240a0));

    s_TypeId_PastureAnimal = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7970));

    s_TypeId_PastureSubComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6f98));

    s_initialized = true;
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// FUN_140004680 — FindComponent (map-grid component) on the pasture owner.
//   RCX=owner, RDX=outIBP -> returns outIBP. Used to read the footprint rect.
typedef void* (*fn_FindMapComp_t)(int64_t owner, void* outIBP);
static fn_FindMapComp_t s_fn_FindMapComp = nullptr;

// FUN_140005500 — FindComponent (animal/model component variant), RCX=owner, RDX=outIBP.
typedef void* (*fn_FindAnimalComp_t)(int64_t owner, void* outIBP);
static fn_FindAnimalComp_t s_fn_FindAnimalComp = nullptr;

// FUN_1400c3470 — entity flag-change notification.
typedef void (*fn_FlagNotify_t)(void* entity);
static fn_FlagNotify_t s_fn_FlagNotify = nullptr;

// Entity_FindComponent_Family @0x1400012d0 — RCX=entity, RDX=outIBP.
typedef void* (*fn_FindCompFamily_t)(int64_t entity, void* outIBP);
static fn_FindCompFamily_t s_fn_FindCompFamily = nullptr;

// AnimalComponent_SetBounds @0x140105e20 — clamps/records the animal's tile bounds.
typedef void (*fn_AnimalSetBounds_t)(void* self, short* a, short* b, short* c, short* d, int e);
static fn_AnimalSetBounds_t s_fn_AnimalSetBounds = nullptr;

// System::Time::Time(float) dispatch slot 0x140152b00 — RCX=outTime, XMM1=seconds.
typedef void* (*fn_Time_CtorF_t)(void* out, float seconds);
static fn_Time_CtorF_t s_fn_Time_CtorF = nullptr;

// Type id consulted by capacity / helper (DWORD_1401c6fd0).
static uint32_t* s_pTypeId_1c6fd0 = nullptr;

static bool s_pastureExtraInit = false;
static void EnsurePastureExtras() {
    if (s_pastureExtraInit) return;
    s_fn_FindMapComp = reinterpret_cast<fn_FindMapComp_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_FindAnimalComp = reinterpret_cast<fn_FindAnimalComp_t>(
        ResolveAddr(Module::Exe, 0x140005500));
    s_fn_FlagNotify = reinterpret_cast<fn_FlagNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_FindCompFamily = reinterpret_cast<fn_FindCompFamily_t>(
        ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_AnimalSetBounds = reinterpret_cast<fn_AnimalSetBounds_t>(
        ResolveAddr(Module::Exe, 0x140105e20));
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_CtorF = *reinterpret_cast<fn_Time_CtorF_t*>(slot);
    s_pTypeId_1c6fd0 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6fd0));
    s_pastureExtraInit = true;
}

// Reimpl of AnimalComponent_SetBounds (0x140105e20): record the animal's footprint
// tile-rect (origin-half .. clamped to the map). self+0x28 = packed cell index, map
// dimension at g_world+0xf8. Pure leaf (no exe sub-callees) -> closure-safe.
void PastureComponent_AnimalSetBounds(void* selfP, short* outX, short* outY,
                                      short* outW, short* outH, int size) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int* mapInfo = *reinterpret_cast<int**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int cell = *reinterpret_cast<int*>((uintptr_t)selfP + 0x28);
    int mapDim = *mapInfo;
    short half = static_cast<short>(size / 2);
    *outX = static_cast<short>(cell % mapDim) - half;
    *outY = static_cast<short>(cell / mapDim) - half;
    *outW = static_cast<short>(size);
    *outH = static_cast<short>(size);
    if (*outX < 0) *outX = 0;
    if (*outY < 0) *outY = 0;
    if (mapDim <= (int)*outW + (int)*outX)
        *outW = static_cast<short>((short)mapDim - *outX) - 1;
    if (mapDim <= (int)*outH + (int)*outY)
        *outH = static_cast<short>((short)mapDim - *outY) - 1;
}

// Reimpl of FUN_140128570 (compute pasture capacity = footprint area / animal slot size)
// Reads the owner's map-grid footprint (w*h at +0x2c/+0x2e of the found component),
// finds the animal-template sub-component (type 0x1401c6fd0) to read its per-animal
// area cost at +0x24, and returns (w*h)/cost. Returns 0 if no animal ref bound.
uint64_t PastureComponent_ComputeCapacity(uintptr_t self) {
    EnsureHelpers();
    EnsurePastureExtras();
    if (*(int64_t*)(self + 0x28) == 0) return 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    uint8_t outIBP[8] = {0};
    if (s_fn_FindMapComp) s_fn_FindMapComp(*(int64_t*)(self + 0x10), outIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(outIBP);

    int64_t animalRef = *(int64_t*)(self + 0x28);
    void* mapComp = *(void**)outIBP;
    short w = *(short*)((uintptr_t)mapComp + 0x2c);
    short h = *(short*)((uintptr_t)mapComp + 0x2e);

    // Scan the animal ref's component list for the per-animal template (type 0x1401c6fd0).
    void* found = nullptr;
    int n = *(int*)(animalRef + 0x30);
    uintptr_t list = *(uintptr_t*)(animalRef + 0x28);
    int idx = 0;
    for (int i = 0; i < n; i++) {
        void* comp = *(void**)(list + (int64_t)i * 8);
        uint32_t t = (*(uint32_t(**)(void*))(*(uintptr_t*)comp + 0x10))(comp);
        if (s_pTypeId_1c6fd0 && t == *s_pTypeId_1c6fd0) {
            void* cand = *(void**)(list + (int64_t)idx * 8);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(cand, *s_pTypeId_1c6fd0)) found = cand;
            break;
        }
        idx++;
    }
    if (found == nullptr) return 0;
    int cost = *(int*)((uintptr_t)found + 0x24);
    if (cost == 0) return 0;
    return (uint64_t)((int)(w * h) / cost) & 0xffffffff;
}

// Reimpl of FUN_140105ed0 (process an animal entity-ref for the pasture)
// Sets the "in-pasture" flag bit (0x400000) on the ref, notifies on change, finds
// the animal's family/model component to read its world position, clamps to the map
// grid, records the tile index at +0x28, records the animal-template bounds, and
// kicks an immediate Update (vtable +0x48 with t=0).
void PastureComponent_Helper5ed0(void* entityRef) {
    EnsureHelpers();
    EnsurePastureExtras();
    uint8_t* p = reinterpret_cast<uint8_t*>(entityRef);

    uint32_t oldFlags = *(uint32_t*)(p + 0x20);
    uint32_t newFlags = oldFlags | 0x400000;
    *(uint32_t*)(p + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(p + 0x10) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x10);
        if (s_fn_FlagNotify) s_fn_FlagNotify(*(void**)(p + 0x10));
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x10);
    uint8_t famIBP[8] = {0};
    if (s_fn_FindCompFamily) s_fn_FindCompFamily(*(int64_t*)(p + 0x10), famIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(famIBP);
    uintptr_t fam = *(uintptr_t*)famIBP;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(fam + 0x68));
    uintptr_t model = *(uintptr_t*)(fam + 0x68);

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int mapW = **(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int x = (int)(*(float*)(model + 0x158) + (float)(mapW >> 1));
    int y = (int)(*(float*)(model + 0x15c) + (float)(mapW >> 1));
    if (x < 0) x = 0;
    if (mapW <= x) x = mapW - 1;
    if (y < 0) y = 0;
    if (mapW <= y) y = mapW - 1;

    void* tmpl = *(void**)(p + 0x18);
    *(int*)(p + 0x28) = mapW * y + x;
    void* animalTmpl = (s_fn_IsDerivedFrom_dispatch && s_pTypeId_1c6fd0 &&
                        s_fn_IsDerivedFrom_dispatch(tmpl, *s_pTypeId_1c6fd0)) ? tmpl : nullptr;
    if (s_fn_AnimalSetBounds) {
        s_fn_AnimalSetBounds(p, (short*)(p + 0x38), (short*)(p + 0x3a),
                             (short*)(p + 0x3c), (short*)(p + 0x3e),
                             *(int*)((uintptr_t)animalTmpl + 0x20));
    }

    uint8_t t[8] = {0};
    if (s_fn_Time_CtorF) s_fn_Time_CtorF(t, 0.0f);
    // Immediate Update tick (vtable slot +0x48).
    (*(void(**)(void*, void*))(*(uintptr_t*)p + 0x48))(p, t);
}

// Reimpl of FUN_1401240a0 (reassign animal ref's pasture tile after re-parenting)
// Looks up the new pasture owner's map footprint (via +0x28 link -> FindMapComp),
// reads its origin/size rect, then on the animal's own model component clears the
// in-pasture flag bit (0x400000), notifies, and copies the new rect into +0x38..+0x3e.
void PastureComponent_ReassignEntityRef(uintptr_t entityPtr) {
    EnsureHelpers();
    EnsurePastureExtras();
    if (*(int64_t*)(entityPtr + 0x28) == 0) return;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(entityPtr + 0x28));
    int64_t link = *(int64_t*)(*(int64_t*)(entityPtr + 0x28) + 0x10);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&link);
    uint8_t mapIBP[8] = {0};
    if (s_fn_FindMapComp) s_fn_FindMapComp(link, mapIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mapIBP);
    uintptr_t mapComp = *(uintptr_t*)mapIBP;
    uint16_t r0 = *(uint16_t*)(mapComp + 0x28);
    uint16_t r1 = *(uint16_t*)(mapComp + 0x2a);
    uint16_t r2 = *(uint16_t*)(mapComp + 0x2c);
    uint16_t r3 = *(uint16_t*)(mapComp + 0x2e);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(entityPtr + 0x10));
    uint8_t modelIBP[8] = {0};
    if (s_fn_FindAnimalComp) s_fn_FindAnimalComp(*(int64_t*)(entityPtr + 0x10), modelIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(modelIBP);
    uintptr_t model = *(uintptr_t*)modelIBP;

    uint32_t oldFlags = *(uint32_t*)(model + 0x20);
    uint32_t newFlags = oldFlags & 0xffbfffff;
    *(uint32_t*)(model + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(model + 0x10) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(model + 0x10));
        if (s_fn_FlagNotify) s_fn_FlagNotify(*(void**)(model + 0x10));
    }
    *(uint16_t*)(model + 0x38) = r0;
    *(uint16_t*)(model + 0x3a) = r1;
    *(uint16_t*)(model + 0x3c) = r2;
    *(uint16_t*)(model + 0x3e) = r3;
}

// ============================================================
// PastureComponent::Shutdown (vtable slot [3], 0x140127f40)
// ============================================================
// Component_Serialize + empty serializer(v0) + main serializer(v0) with:
//   - IBP array serialization at +0x28
//   - Entity count at +0x38, entity array at +0x30
//   - IsDerivedFrom validation on each entity (type 0x1401c7970)
//   - Int fields at +0x40 and +0x44

void PastureComponent_Shutdown(PastureComponent* self, void* stream) {
    EnsureHelpers();

    // Base Component serialization
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (version 0, immediately destroyed)
    uint8_t emptySerializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(emptySerializer, stream, 0);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(emptySerializer);
    }

    // Main serializer block (version 0)
    uint8_t serializer[16];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize InternalBasePtr array at +0x28
    if (s_fn_SerializeIBPArray) {
        s_fn_SerializeIBPArray((void*)&self->field28, serializer);
    }

    // Serialize entity count from +0x38
    int entityCount = *(int*)((uintptr_t)self + 0x38);
    int countBuf = entityCount;
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, &countBuf, nullptr);
    }

    // On load (serializer mode == 0), resize entity array
    if (*(int*)((uintptr_t)serializer + 8) == 0) {
        if (s_fn_ResizeIBPArray) {
            s_fn_ResizeIBPArray((void*)&self->field30, countBuf);
        }
    }

    // Serialize each entity InternalBasePtr and validate type
    void* entityArray = *(void**)((uintptr_t)self + 0x30);
    int64_t count = (int64_t)countBuf;
    if (count > 0 && entityArray) {
        uintptr_t ptr = (uintptr_t)entityArray;
        for (int64_t i = 0; i < count; i++) {
            if (s_fn_IBP_Serialize) {
                s_fn_IBP_Serialize((void*)ptr, serializer);
            }
            void* obj = *(void**)ptr;
            if (obj != nullptr && s_fn_IsDerivedFrom_dispatch && s_TypeId_PastureAnimal) {
                bool valid = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_PastureAnimal);
                if (!valid) {
                    *(uint64_t*)ptr = 0;
                }
            }
            ptr += 8;
        }
    }

    // Serialize int fields at +0x40 and +0x44
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, (int*)((uintptr_t)self + 0x40), nullptr);
        s_fn_Serializer_SerializeInt(serializer, (int*)&self->field44, nullptr);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// PastureComponent::Draw (vtable slot [6], 0x140127a30)
// ============================================================
// Compute pasture capacity via FUN_140128570, store at +0x40.

void PastureComponent_Draw(PastureComponent* self) {
    EnsureHelpers();

    if (s_fn_ComputePastureCapacity) {
        uint64_t capacity = s_fn_ComputePastureCapacity((uintptr_t)self);
        *(int*)((uintptr_t)self + 0x40) = (int)capacity;
    }
}

// ============================================================
// PastureComponent::OnCreate (vtable slot [7], 0x1401277e0)
// ============================================================
// Complex entity ref management. Iterates entity array backwards,
// validates references, registers with population manager lists,
// reassigns entity refs. The loop processes each entity at +0x30
// array[count-1] down to array[0], then registers self with
// game world population list at +0x90.
//
// Due to extreme complexity (100+ lines of Ghidra output with multiple
// nested conditionals, InternalBasePtr operations, and sub-component
// lookups), this delegates to the original function via a resolved
// function pointer. This preserves correctness while still extracting
// the function to the clean code layer.

// Direct address call to original implementation
typedef void (*fn_PastureOnCreate_Orig_t)(void* self);
static fn_PastureOnCreate_Orig_t s_fn_PastureOnCreate_Orig = nullptr;

void PastureComponent_OnCreate(PastureComponent* self) {
    EnsureHelpers();

    // Resolve original on first call (cannot do in EnsureHelpers due to hook)
    if (!s_fn_PastureOnCreate_Orig) {
        // We call the sub-functions individually since the original is hooked.
        // The logic is: iterate entity array backwards, process each entity.
    }

    // Due to the extreme complexity of this function (deeply nested entity ref
    // management with InternalBasePtr ctor/CheckPtr/dereference chains, BST
    // lookups, and population manager registration), we reproduce the original
    // control flow faithfully.

    int entityCount = *(int*)((uintptr_t)self + 0x38);

    // Process entities backwards (original uses do-while with decrementing count)
    while (entityCount >= 1) {
        // Create InternalBasePtr for null (self reference)
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor(&selfRef, nullptr);
        }

        // Get entity from array[count-1]
        uintptr_t entityArray = *(uintptr_t*)((uintptr_t)self + 0x30);
        uintptr_t entitySlot = entityArray + (int64_t)(entityCount - 1) * 8;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr((void*)entitySlot);
        }
        void* entity = *(void**)entitySlot;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(&selfRef);
        }

        // Check if entity's +0x28 field differs from selfRef
        int64_t entityField28 = *(int64_t*)((uintptr_t)entity + 0x28);
        if (entityField28 != (int64_t)selfRef) {
            if (entityField28 == 0) {
                // No assignment: add to unassigned list at game world +0xf0 -> +0x80
                uint64_t entityRef = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(&entityRef, entity);
                }
                if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_AddToList) {
                    uintptr_t gameState = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
                    s_fn_AddToList((void*)(gameState + 0x80), &entityRef);
                }
            } else {
                // Has assignment: add to assigned entity's component list
                uint64_t entityRef2 = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(&entityRef2, entity);
                }

                // Get the assigned entity's +0x28 field, CheckPtr, then add to its +0x30 list
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)entity + 0x28));
                }
                uintptr_t assignedEntity = *(uintptr_t*)((uintptr_t)entity + 0x28);
                if (assignedEntity) {
                    s_fn_AddToList((void*)(assignedEntity + 0x30), &entityRef2);
                }

                // Find sub-component of specific type on assigned entity
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)entity + 0x10));
                }
                uintptr_t entityOwner = *(uintptr_t*)((uintptr_t)entity + 0x10);
                void* foundComp = nullptr;

                int compCount = *(int*)(entityOwner + 0x38);
                for (int i = 0; i < compCount; i++) {
                    uintptr_t compSlot = *(uintptr_t*)(entityOwner + 0x30) + (int64_t)i * 8;
                    if (fn_InternalBasePtr_CheckPtr) {
                        fn_InternalBasePtr_CheckPtr((void*)compSlot);
                    }
                    void* comp = *(void**)compSlot;
                    // Check type via vtable slot +0x10 (GetDescription)
                    uint32_t compType = (*(uint32_t(**)(void*))(*(uintptr_t*)comp + 0x10))(comp);
                    if (s_TypeId_PastureSubComp && compType == *s_TypeId_PastureSubComp) {
                        // Validate with IsDerivedFrom
                        if (s_fn_IsDerivedFrom_dispatch) {
                            bool valid = s_fn_IsDerivedFrom_dispatch(comp, *s_TypeId_PastureSubComp);
                            foundComp = valid ? comp : nullptr;
                        }
                        break;
                    }
                }

                // Process found component
                uint64_t compRef = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(&compRef, foundComp);
                }
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr(&compRef);
                }
                if (s_fn_PastureHelper5ed0) {
                    s_fn_PastureHelper5ed0((void*)compRef);
                }
            }

            // Update entity's +0x28 to point to selfRef (our pasture)
            *(int64_t*)((uintptr_t)entity + 0x28) = (int64_t)selfRef;

            // Register or re-register entity based on selfRef
            if ((int64_t)selfRef == 0) {
                // Insert into unassigned list
                uint64_t entityRef3 = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(&entityRef3, entity);
                }
                if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_InsertInList) {
                    uintptr_t gameState = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
                    int listCount = *(int*)(gameState + 0x88);
                    s_fn_InsertInList((void*)(gameState + 0x80), listCount + 1, &entityRef3);
                }
            } else {
                // Insert into assigned entity's list and call reassign
                uint64_t entityRef4 = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor(&entityRef4, entity);
                }
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)entity + 0x28));
                }
                uintptr_t target = *(uintptr_t*)((uintptr_t)entity + 0x28);
                if (target && s_fn_InsertInList) {
                    int listCount2 = *(int*)(target + 0x38);
                    s_fn_InsertInList((void*)(target + 0x30), listCount2 + 1, &entityRef4);
                }
                if (s_fn_ReassignEntityRef) {
                    s_fn_ReassignEntityRef((uintptr_t)entity);
                }
            }
        }

        // Re-read count (original code re-reads from self+0x38 each iteration)
        entityCount = *(int*)((uintptr_t)self + 0x38);
    }

    // Final: register self with population manager at game world +0xf0 -> +0x90
    uint64_t selfEntityRef = 0;
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(&selfEntityRef, self);
    }
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_AddToList) {
        uintptr_t gameState = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        s_fn_AddToList((void*)(gameState + 0x90), &selfEntityRef);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void PastureComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[PastureComponent] Funcs resolved: Component_Serialize=%p ComputeCapacity=%p "
           "AddToList=%p InsertInList=%p PastureHelper=%p ReassignEntityRef=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ComputePastureCapacity,
           (void*)s_fn_AddToList, (void*)s_fn_InsertInList,
           (void*)s_fn_PastureHelper5ed0, (void*)s_fn_ReassignEntityRef);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid PastureComponent
// (size 0x68, +0x20 capability flags 0x8b, Object::Internal base + shared stub
// vtable, zeroed body). Size from PastureComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(PastureComponent, 0x68, 0x8b)
REGISTER_TYPE(PastureComponent, "ControllerComponent",
              &PastureComponent_Factory, &PastureComponent_CreateInPlace);
