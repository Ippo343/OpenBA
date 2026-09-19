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

#include "Component/pasturecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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

// FUN_140128570 — compute pasture capacity
typedef uint64_t (*fn_ComputePastureCapacity_t)(uintptr_t self);
static fn_ComputePastureCapacity_t s_fn_ComputePastureCapacity = nullptr;

// FUN_140002cc0 — DynArray8_EraseByValue(arrayField, valueIBP). NOT an "add".
typedef void (*fn_EraseByValue_t)(void* listPtr, void* entityRef);
static fn_EraseByValue_t s_fn_EraseByValue = nullptr;

// FUN_140003110 — DynArray8_ResizeFill(arrayField, newCount, valueIBP); called with
// count+1 it appends. NOT an "insert at position". ENDGAME B.3.2 step 2: resolved
// but never actually called in this file (only ever printed for debug) — no call
// site to convert.

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

    s_fn_ComputePastureCapacity = reinterpret_cast<fn_ComputePastureCapacity_t>(
        ResolveAddr(Module::Exe, 0x140128570));

    s_fn_EraseByValue = reinterpret_cast<fn_EraseByValue_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

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

    uint8_t outIBP[8] = {0};
    if (s_fn_FindMapComp) s_fn_FindMapComp(*(int64_t*)(self + 0x10), outIBP);

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
        if (s_fn_FlagNotify) s_fn_FlagNotify(*(void**)(p + 0x10));
    }

    uint8_t famIBP[8] = {0};
    if (s_fn_FindCompFamily) s_fn_FindCompFamily(*(int64_t*)(p + 0x10), famIBP);
    uintptr_t fam = *(uintptr_t*)famIBP;
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

    int64_t link = *(int64_t*)(*(int64_t*)(entityPtr + 0x28) + 0x10);
    uint8_t mapIBP[8] = {0};
    if (s_fn_FindMapComp) s_fn_FindMapComp(link, mapIBP);
    uintptr_t mapComp = *(uintptr_t*)mapIBP;
    uint16_t r0 = *(uint16_t*)(mapComp + 0x28);
    uint16_t r1 = *(uint16_t*)(mapComp + 0x2a);
    uint16_t r2 = *(uint16_t*)(mapComp + 0x2c);
    uint16_t r3 = *(uint16_t*)(mapComp + 0x2e);

    uint8_t modelIBP[8] = {0};
    if (s_fn_FindAnimalComp) s_fn_FindAnimalComp(*(int64_t*)(entityPtr + 0x10), modelIBP);
    uintptr_t model = *(uintptr_t*)modelIBP;

    uint32_t oldFlags = *(uint32_t*)(model + 0x20);
    uint32_t newFlags = oldFlags & 0xffbfffff;
    *(uint32_t*)(model + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(model + 0x10) != 0) {
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
    alignas(8) uint8_t emptySerializer[24];
    if (s_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(emptySerializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    }
    if (s_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(emptySerializer)->~Serializer();                                       // B.3.4 bridge
    }

    // Main serializer block (version 0)
    alignas(8) uint8_t serializer[16];
    if (s_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    }

    // Serialize InternalBasePtr array at +0x28
    if (s_fn_SerializeIBPArray) {
        s_fn_SerializeIBPArray((void*)&self->field28, serializer);
    }

    // Serialize entity count from +0x38
    int entityCount = *(int*)((uintptr_t)self + 0x38);
    int countBuf = entityCount;
    if (s_fn_Serializer_SerializeInt) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(countBuf, nullptr);   // B.3.4 bridge
    }

    // On load, resize entity array. Retail reads the direction flag from the
    // STREAM (+8) through the serializer's first field — `*(int*)(ser[0] + 8)`
    // — not from serializer+8 (that's the version int, 0 here, which made the
    // old check pass on save too; benign only because resize-to-same-count is
    // a no-op).
    // B.3.2 bridge: FUN_1400032e0 is Collection::Array<void*>::SetSize(n)
    if (*(int*)(*(uintptr_t*)serializer + 8) == 0) {
        GameColl::AsArray<GameColl::Ptr8>((void*)&self->field30).SetSize(countBuf);
    }

    // Serialize each entity InternalBasePtr and validate type
    void* entityArray = *(void**)((uintptr_t)self + 0x30);
    int64_t count = (int64_t)countBuf;
    if (count > 0 && entityArray) {
        uintptr_t ptr = (uintptr_t)entityArray;
        for (int64_t i = 0; i < count; i++) {
            if (s_fn_IBP_Serialize) {
                reinterpret_cast<Object::InternalBasePtr*>((void*)ptr)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
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
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)((uintptr_t)self + 0x40), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)&self->field44, nullptr);   // B.3.4 bridge
    }

    if (s_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
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
// PastureComponent vtable slot [7] (0x1401277e0) — RELEASE, not "OnCreate"
// ============================================================
// Ghidra's "OnCreate" label is the usual off-by-one (slot 6 is OnAdded, slot 7 is
// the remove side). The body is byte-identical to PastureDescription_helper_6e40,
// the controller's state-3 (teardown) ENTER: unassign every animal back to the
// world's free-roaming pool and erase the pasture from the world pasture registry
// at gameState+0x90.
//
// The previous port of this function spun forever. Two labelling mistakes caused it:
// FUN_140002cc0 is DynArray8_EraseByValue and FUN_140003110 is DynArray8_ResizeFill,
// not "AddToList"/"InsertInList" — and it is the ERASE from pasture+0x30 that shrinks
// pasture+0x38 and ends the `while (count >= 1)` loop. The single shared body now
// lives in pasturecomponent_states.cpp (Pasture_ReleaseAllAnimals), so the vtable
// slot and the state-3 handler cannot drift apart again.

extern "C" void Pasture_ReleaseAllAnimals(void* pasture);

void PastureComponent_OnCreate(PastureComponent* self) {
    EnsureHelpers();
    Pasture_ReleaseAllAnimals(self);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void PastureComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[PastureComponent] Funcs resolved: Component_Serialize=%p ComputeCapacity=%p "
           "AddToList=%p PastureHelper=%p ReassignEntityRef=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ComputePastureCapacity,
           (void*)s_fn_EraseByValue,
           (void*)s_fn_PastureHelper5ed0, (void*)s_fn_ReassignEntityRef);
#endif
}

// ===== Standalone factory =====
// Inert by default; BANISHED_LIVE_PASTURE graduates the component to a live
// ControllerComponent. Size 0x68 and flags 0x8b come from PastureComponent_Ctor
// (0x1401280e0): Component_Ctor's +0x20=0x80 OR'd with the leaf's 0x8b.
//
// Live slots (indices read off the retail primary vtable at 0x140163f60):
//   [6]  OnAdded  = PastureComponent_Draw   (0x140127a30) — compute +0x40 capacity
//   [7]  OnRemove = release-all             (0x1401277e0)
//   [16] GetDefaultState -> 2   (retail 0x1400026c0 is literally `MOV EAX,2; RET`)
//   [17] CreateState     -> self+0x48       (retail 0x140036770 `LEA RAX,[RCX+0x48]`)
// plus the embedded node at +0x48: its vtable is the pasture state-table dispatcher
// and node+0x18 (= comp+0x60) is the back-pointer the ctor writes.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

extern "C" void** PastureCtrl_NodeVtable();
extern "C" void   PastureCtrl_PopulateTable();
extern "C" void   Pasture_MsgReceive(void* pasture, int msg);  // slot 15 breed tag
extern "C" uint64_t Pasture_CanCommand(void* pasture, int cmd); // slot 13 capability query
extern "C" void   Pasture_Command(void* pasture, int cmd);      // slot 14 execute command

static uint32_t PastureComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PastureComponent");
    return cached;
}
static void* PastureComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void  PastureComponent_OnAddedVSlot(void* s) {
    PastureComponent_Draw(reinterpret_cast<PastureComponent*>(s));
}
static void  PastureComponent_OnRemoveVSlot(void* s) {
    PastureComponent_OnCreate(reinterpret_cast<PastureComponent*>(s));
}
// Slot 15 (+0x78 OnMessage, retail 0x140127720): the herder's work-complete
// message tags one random pastured animal breed-eligible (flag bit 23) — the
// only setter of that bit in the exe; without it livestock never reproduce.
// Body in pasturecomponent_states.cpp; ABI = (self, msgCode, rest unused).
static void  PastureComponent_MsgVSlot(void* s, int msg, void*, uint32_t, uint32_t) {
    Pasture_MsgReceive(s, msg);
}
// Slots 13/14 (retail 0x140127dd0 / 0x140127a60): the pasture command pair —
// slot 13 answers "may command N run?", slot 14 executes it. Cmd 2 is the herder
// toggle SetLivestockType fires on the first type assignment (posts PastureWork).
static uint64_t PastureComponent_CanCommandVSlot(void* s, int cmd) {
    return Pasture_CanCommand(s, cmd);
}
static void  PastureComponent_CommandVSlot(void* s, int cmd) {
    Pasture_Command(s, cmd);
}
// ── slot 12 — retail 0x140127540 — claim the pasture's gate cells ─────────────────
// Only acts on message id 4. For each kind-2 child attach point of the pasture's
// position component it converts the point to a map cell, forces that cell's terrain
// byte (+0x29) to 2, clears the cell's entity slot (+0x08) to a null IBP, and notifies
// the two map subsystems:
//     if (1 << terrain & 0x1e0) map+0xa2 |= 1, and FUN_1400a8c90(map+0x70, x, y)
//     FUN_1400a6fe0(map+0x68, cell, 2, index)
// The 0x1e0 mask is the "was this a resource/rough tile" test the orchard and
// ResourceProduce paths use too; the |= records that at least one such tile changed so
// the caller can refresh.
//
// This is the pasture's gate/entrance claim — the cells a herder walks through. With
// the slot stubbed the terrain there was never converted, which is the same class of
// omission as the MapComponent occupancy gap already recorded for this wave.
extern "C" void PastureComponent_Slot12VSlot(void* selfV, int msgId) {
    if (msgId != 4) return;
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    static void (*s_findPos)(int64_t, void*)            = nullptr;  // 0x140002ed0
    static int  (*s_childCount)(int64_t, int)           = nullptr;  // 0x140123070
    static void (*s_spotPos)(int64_t, float*, int, int, float*) = nullptr; // 0x140122e20
    static void (*s_mapNotifyA)(int64_t, int, int)      = nullptr;  // 0x1400a8c90
    static void (*s_mapNotifyB)(void*, int, int, uint64_t) = nullptr; // 0x1400a6fe0
    static bool s_r = false;
    if (!s_r) {
        s_findPos    = reinterpret_cast<void(*)(int64_t, void*)>(ResolveAddr(Module::Exe, 0x140002ed0));
        s_childCount = reinterpret_cast<int(*)(int64_t, int)>(ResolveAddr(Module::Exe, 0x140123070));
        s_spotPos    = reinterpret_cast<void(*)(int64_t, float*, int, int, float*)>(
            ResolveAddr(Module::Exe, 0x140122e20));
        s_mapNotifyA = reinterpret_cast<void(*)(int64_t, int, int)>(ResolveAddr(Module::Exe, 0x1400a8c90));
        s_mapNotifyB = reinterpret_cast<void(*)(void*, int, int, uint64_t)>(
            ResolveAddr(Module::Exe, 0x1400a6fe0));
        s_r = true;
    }
    if (!s_findPos || !s_childCount || !s_spotPos) return;

    int64_t posIbp = 0;
    s_findPos(*reinterpret_cast<int64_t*>(self + 0x10), &posIbp);
    if (!posIbp) return;

    int* map = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!map) return;
    const int width = *map;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(map) + 0x80);
    if (!grid || width <= 0) return;

    for (uint32_t i = 0; (int)i < s_childCount(posIbp, 2); ++i) {
        float p[3] = {0, 0, 0};
        s_spotPos(posIbp, p, 2, (int)i, nullptr);
        int x = (int)(p[0] + (float)(width >> 1));
        int y = (int)(p[1] + (float)(width >> 1));
        if (x < 0) x = 0; if (x >= width) x = width - 1;
        if (y < 0) y = 0; if (y >= width) y = width - 1;
        const int cell = width * y + x;

        uintptr_t nullIbp = 0;
        if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        uint8_t* terrain = reinterpret_cast<uint8_t*>(grid + 0x29 + (uintptr_t)cell * 0x30);
        const uint8_t was = *terrain;
        const bool wasRough = ((1u << (was & 0x1f)) & 0x1e0u) != 0;
        uint8_t* flag = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(map) + 0xa2);
        *flag = static_cast<uint8_t>(*flag | (wasRough ? 1 : 0));
        *terrain = 2;
        if (wasRough && s_mapNotifyA)
            s_mapNotifyA(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(map) + 0x70),
                         cell % width, cell / width);
        if (s_mapNotifyB)
            s_mapNotifyB(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(map) + 0x68),
                         cell, 2, i);
        *reinterpret_cast<uintptr_t*>(grid + 8 + (uintptr_t)cell * 0x30) = nullIbp;
    }
}

// ── Family-A tail slots 1 / 4 / 19 / 20 / 21 ──────────────────────────────────────
// PastureComponent does not use the CONTROLLER_LIVE_FACTORY_STATES macro (it has this
// hand-rolled InstallLive), so none of the shared slot fills reached it and the
// coverage harness reported it as the weakest of the six at 14/22. All five of these
// are tiny in retail:
//   slot 1  0x140036720  MOV RAX,[rip+global]; RET   — the class BaseTypeInfo
//   slot 4  0x140127a50  *(uint64*)(self+0x28) = 0   — one store
//   slot 19 0x140036790  MOV RAX,[rip+global]; RET
//   slot 20 0x140036780  MOV EAX,[rip+global]; RET
//   slot 21 0x140128280  MOV EAX,2; RET              — 2 here, 3 on Orchard/CropField
//
// ⚠ As on Orchard/CropField, 19/20 read globals ADJACENT to the ones 1/2 use — the
// controller-interface subobject's, for which we hold no separate storage. Returning
// the class's own type info/id is a stand-in, not byte-exact; still strictly closer
// than the inert stub, and flagged so it is not mistaken for finished.
static void* PastureComponent_GetTypeInfoVSlot(void*) {
    static void* c = nullptr;
    if (!c) c = ::Object::GetTypeInfoByName("PastureComponent");
    return c;
}
static uint32_t PastureComponent_GetTypeIdVSlot(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PastureComponent");
    return c;
}
static void PastureComponent_Slot4Clear(void* self) {
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x28) = 0;
}
static int PastureComponent_Slot21Const(void*) { return 2; }

extern "C" void* ControllerLive_ScalarDeletingDtor(void*, unsigned int);  // slot 18
static int   PastureComponent_GetDefaultState(void*) { return 2; }
static void* PastureComponent_CreateState(void* self) {
    return reinterpret_cast<uint8_t*>(self) + 0x48;
}

// Re-run for every instance: InitInertTyped re-stubs the shared vtable each time.
static void PastureComponent_InstallLive(void* mem) {
    PastureComponent_LiveVtable[12] = reinterpret_cast<void*>(&PastureComponent_Slot12VSlot);
    PastureComponent_LiveVtable[1]  = reinterpret_cast<void*>(&PastureComponent_GetTypeInfoVSlot);
    // Slot 3 = Shutdown/Serialize. The body has been ported in this file since
    // the original wave and nothing ever wrote it into the live vtable, so the
    // coverage harness measured the slot as still holding the inert stub. Third
    // instance of "ported but unwired" in this component family — worth wiring
    // from InstallLive, the one place FillTypedVtable cannot erase.
    PastureComponent_LiveVtable[3]  = reinterpret_cast<void*>(
        static_cast<void(*)(PastureComponent*, void*)>(&PastureComponent_Shutdown));
    PastureComponent_LiveVtable[18] = reinterpret_cast<void*>(&ControllerLive_ScalarDeletingDtor);
    PastureComponent_LiveVtable[4]  = reinterpret_cast<void*>(&PastureComponent_Slot4Clear);
    PastureComponent_LiveVtable[19] = reinterpret_cast<void*>(&PastureComponent_GetTypeInfoVSlot);
    PastureComponent_LiveVtable[20] = reinterpret_cast<void*>(&PastureComponent_GetTypeIdVSlot);
    PastureComponent_LiveVtable[21] = reinterpret_cast<void*>(&PastureComponent_Slot21Const);
    PastureComponent_LiveVtable[6]  = reinterpret_cast<void*>(&PastureComponent_OnAddedVSlot);
    PastureComponent_LiveVtable[7]  = reinterpret_cast<void*>(&PastureComponent_OnRemoveVSlot);
    PastureComponent_LiveVtable[13] = reinterpret_cast<void*>(&PastureComponent_CanCommandVSlot);
    PastureComponent_LiveVtable[14] = reinterpret_cast<void*>(&PastureComponent_CommandVSlot);
    PastureComponent_LiveVtable[15] = reinterpret_cast<void*>(&PastureComponent_MsgVSlot);
    PastureComponent_LiveVtable[16] = reinterpret_cast<void*>(&PastureComponent_GetDefaultState);
    PastureComponent_LiveVtable[17] = reinterpret_cast<void*>(&PastureComponent_CreateState);
    PastureCtrl_PopulateTable();
    if (mem) {
        uint8_t* node = reinterpret_cast<uint8_t*>(mem) + 0x48;
        *reinterpret_cast<void**>(node) = reinterpret_cast<void*>(PastureCtrl_NodeVtable());
        *reinterpret_cast<void**>(node + 0x18) = mem;   // = comp+0x60, as the ctor does
    }
}

extern "C" void* PastureComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x68, 0x8b,
        PastureComponent_LiveVtable,
        reinterpret_cast<void*>(&PastureComponent_LiveGetTypeId));
    PastureComponent_InstallLive(p);
    return p;
}
static void* PastureComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x68, 0x8b,
        PastureComponent_LiveVtable,
        reinterpret_cast<void*>(&PastureComponent_LiveGetTypeId));
    PastureComponent_InstallLive(mem);
    return mem;
}

REGISTER_TYPE(PastureComponent, "ControllerComponent",
              &PastureComponent_Factory, &PastureComponent_CreateInPlace);
