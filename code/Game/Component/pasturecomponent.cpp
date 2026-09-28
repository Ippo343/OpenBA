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
#include "Component/AnimalDescription.h"
#include "Component/LivestockComponent.h"
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

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

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

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

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
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (t == AnimalDescription::TypeId()) {
            void* cand = *(void**)(list + (int64_t)idx * 8);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(cand, AnimalDescription::TypeId())) found = cand;
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
    void* animalTmpl = (s_fn_IsDerivedFrom_dispatch &&
                        s_fn_IsDerivedFrom_dispatch(tmpl, AnimalDescription::TypeId())) ? tmpl : nullptr;
    if (s_fn_AnimalSetBounds) {
        s_fn_AnimalSetBounds(p, (short*)(p + 0x38), (short*)(p + 0x3a),
                             (short*)(p + 0x3c), (short*)(p + 0x3e),
                             *(int*)((uintptr_t)animalTmpl + 0x20));
    }

    uint8_t t[8] = {0};
    if (s_fn_Time_CtorF) s_fn_Time_CtorF(t, 0.0f);
    // Immediate Update tick (vtable slot +0x48).
    reinterpret_cast<Component*>(p)->Update(t);   // B.3.8 bridge (slot 9, +0x48)
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

// 0x140127f40 door: registered by address (standalone/reg_animal_serialize.cpp).
void PastureComponent_Shutdown(PastureComponent* self, void* stream) {
    self->PastureComponent::Persist(stream);
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


// ============================================================
// Init: resolve function pointers
// ============================================================
void PastureComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[PastureComponent] Funcs resolved: ComputeCapacity=%p "
           "AddToList=%p PastureHelper=%p ReassignEntityRef=%p\n",
           (void*)s_fn_ComputePastureCapacity,
           (void*)s_fn_EraseByValue,
           (void*)s_fn_PastureHelper5ed0, (void*)s_fn_ReassignEntityRef);
#endif
}

// ===== ENDGAME B.3.8 MI-B — PastureComponent's own slot bodies =====
// Slots 13/14/15 live in pasturecomponent_states.cpp (registered by address from
// standalone/reg_pasture_live.cpp); the class (PastureComponent.h) reaches them and
// the slot-12 body below through its members, at the bottom of this file.
#include "Object/factory_registration.h"



// ============================================================================
// ENDGAME B.3.8 MI-B — PastureComponent as a REAL class (PastureComponent.h).
//
// Before: a hand-rolled InstallLive over a 48-slot table (slots 1/3/4/6/7/12-21
// patched per create, Inert_ScalarDtor in slot 0, Dispatcher<PastureCtrlTag>
// stamped into raw bytes at +0x48 with its back-pointer at +0x60). Slots 18-21 held
// ControllerLive_ScalarDeletingDtor, the class's type info / id and `return 2`: that
// is PastureDescription's table 0x140163ff0 read past this 18-slot table's end
// (B38 D3.5) — gone; nothing in the tree dispatches a component past slot 17.
// After: the compiler's 18-slot vtable, a real ctor/dtor chain, the controller a
// member object at retail's +0x48 (B38_RULES D1, D2, D6).
// ============================================================================

// 0x1401280e0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28 = +0x30 = +0x38 = 0; +0x44 = 0; the member at +0x48 with its back-pointer
// (+0x60); then `_flags |= 0x8b`. Retail leaves +0x40 unwritten; the tree's factory
// zeroed it and that is kept.
PastureComponent::PastureComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr), _pad40{}, field44(0),
      _controller(this) {
    OrFlagsInCtor(0x8b);
}

// [0] 0x140036910: ~PastureComponent (0x140128070), MemFree on &1.
Object::Base* PastureComponent::DeletingDtor(unsigned flags) {
    this->~PastureComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140128070: MemFree(+0x30), +0x30 = +0x38 = 0, then the restamps and ~Internal
// (the compiler chain).
PastureComponent::~PastureComponent() {
    Object::detail::InternalMemFree(field30);
    field30 = nullptr;
    field38 = nullptr;
}

// create 0x140036730: MemAlloc(0x68, 8), then the ctor.
void* PastureComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PastureComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* PastureComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PastureComponent());
}

// ============================================================
// PastureComponent::Persist (vtable slot [3], 0x140127f40)
// ============================================================
// Component_Serialize + empty serializer(v0) + main serializer(v0) with:
//   - IBP array serialization at +0x28
//   - Entity count at +0x38, entity array at +0x30
//   - IsDerivedFrom validation on each entity (type 0x1401c7970)
//   - Int fields at +0x40 and +0x44

void PastureComponent::Persist(void* stream) {
    EnsureHelpers();

    // Base Component serialization
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Empty serializer block (version 0, immediately destroyed)
    alignas(8) uint8_t emptySerializer[24];
    ::new (static_cast<void*>(emptySerializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(emptySerializer)->~Serializer();                                       // B.3.4 bridge

    // Main serializer block (version 0)
    alignas(8) uint8_t serializer[16];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr array at +0x28
    if (s_fn_SerializeIBPArray) {
        s_fn_SerializeIBPArray((void*)&field28, serializer);
    }

    // Serialize entity count from +0x38
    int entityCount = *(int*)(reinterpret_cast<uintptr_t>(this) + 0x38);
    int countBuf = entityCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(countBuf, nullptr);   // B.3.4 bridge

    // On load, resize entity array. Retail reads the direction flag from the
    // STREAM (+8) through the serializer's first field — `*(int*)(ser[0] + 8)`
    // — not from serializer+8 (that's the version int, 0 here, which made the
    // old check pass on save too; benign only because resize-to-same-count is
    // a no-op).
    // B.3.2 bridge: FUN_1400032e0 is Collection::Array<void*>::SetSize(n)
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::Ptr8>((void*)&field30).SetSize(countBuf);
    }

    // Serialize each entity InternalBasePtr and validate type
    void* entityArray = *(void**)(reinterpret_cast<uintptr_t>(this) + 0x30);
    int64_t count = (int64_t)countBuf;
    if (count > 0 && entityArray) {
        uintptr_t ptr = (uintptr_t)entityArray;
        for (int64_t i = 0; i < count; i++) {
            reinterpret_cast<Object::InternalBasePtr*>((void*)ptr)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            void* obj = *(void**)ptr;
            if (obj != nullptr && s_fn_IsDerivedFrom_dispatch) {
                bool valid = s_fn_IsDerivedFrom_dispatch(obj, LivestockComponent::TypeId());
                if (!valid) {
                    *(uint64_t*)ptr = 0;
                }
            }
            ptr += 8;
        }
    }

    // Serialize int fields at +0x40 and +0x44
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)(reinterpret_cast<uintptr_t>(this) + 0x40), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*(int*)&field44, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}
// [4] 0x140127a50: `MOV qword [RCX+0x28],0` — one store.
void PastureComponent::Startup() { field28 = nullptr; }
// ============================================================
// PastureComponent::OnAdded (vtable slot [6], 0x140127a30; Ghidra "Draw")
// ============================================================
// Compute pasture capacity via FUN_140128570, store at +0x40.

void PastureComponent::OnAdded() {
    EnsureHelpers();

    if (s_fn_ComputePastureCapacity) {
        uint64_t capacity = s_fn_ComputePastureCapacity(reinterpret_cast<uintptr_t>(this));
        *(int*)(reinterpret_cast<uintptr_t>(this) + 0x40) = (int)capacity;
    }
}
void PastureComponent::OnRemove() {
    EnsureHelpers();
    Pasture_ReleaseAllAnimals(this);
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
// The slot-12 helpers (were function-local statics of the body; B6-5).
static void (*s_findPos)(int64_t, void*)            = nullptr;  // 0x140002ed0
static int  (*s_childCount)(int64_t, int)           = nullptr;  // 0x140123070
static void (*s_spotPos)(int64_t, float*, int, int, float*) = nullptr; // 0x140122e20
static void (*s_mapNotifyA)(int64_t, int, int)      = nullptr;  // 0x1400a8c90
static void (*s_mapNotifyB)(void*, int, int, uint64_t) = nullptr; // 0x1400a6fe0
static bool s_slot12Resolved = false;
static void PastureSlot12_Resolve() {
    if (s_slot12Resolved) return;
    s_findPos    = reinterpret_cast<void(*)(int64_t, void*)>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_childCount = reinterpret_cast<int(*)(int64_t, int)>(ResolveAddr(Module::Exe, 0x140123070));
    s_spotPos    = reinterpret_cast<void(*)(int64_t, float*, int, int, float*)>(
        ResolveAddr(Module::Exe, 0x140122e20));
    s_mapNotifyA = reinterpret_cast<void(*)(int64_t, int, int)>(ResolveAddr(Module::Exe, 0x1400a8c90));
    s_mapNotifyB = reinterpret_cast<void(*)(void*, int, int, uint64_t)>(
        ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_slot12Resolved = true;
}

void PastureComponent::OnEvent(int msgId) {
    if (msgId != 4) return;
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    PastureSlot12_Resolve();
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

// [13] 0x140127dd0 CommandMask / [14] 0x140127a60 Command / [15] 0x140127720 OnMessage
// live in pasturecomponent_states.cpp next to their helpers.

DEFINE_EXE_TYPE(PastureComponent, "ControllerComponent")
