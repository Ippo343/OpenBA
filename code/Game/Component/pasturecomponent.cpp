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
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // ActHunt_FindComponent_5500, ActMv_FindComponentFamily_12d0,
                                 // ActHunt_NotifyEntityChanged_c3470, the Pf_* map helpers
#include "Work/exe_bodies.h"     // WorkBase_FindComponent_2ed0

// The exe helpers this file calls, by retail address:
//   0x140004680 FloorComponent_FindResData      FindComponent (map-grid component), (owner, outIBP)
//   0x140005500 ActHunt_FindComponent_5500      FindComponent (animal/model variant), (owner, outIBP)
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470  entity flag-change notification
//   0x1400012d0 ActMv_FindComponentFamily_12d0  Entity_FindComponent_Family, (entity, outIBP)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable  serialize the InternalBasePtr at +0x28
//   0x140152a98 ?IsDerivedFrom@Base@Object@@    import slot
//   0x140152b00 ??0Time@System@@QEAA@M@Z        import slot (RtSystem_Time_Ctor)


// Reimpl of AnimalComponent_SetBounds (0x140105e20): record the animal's footprint
// tile-rect (origin-half .. clamped to the map). self+0x28 = packed cell index, map
// dimension at g_world+0xf8. Pure leaf (no exe sub-callees) -> closure-safe.
void PastureComponent_AnimalSetBounds(void* selfP, short* outX, short* outY,
                                      short* outW, short* outH, int size) {
    if (GameUpdateLoop::_singleton == nullptr) return;
    int* mapInfo = *reinterpret_cast<int**>((uintptr_t)GameUpdateLoop::_singleton + 0xf8);
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
    if (*(int64_t*)(self + 0x28) == 0) return 0;

    uint8_t outIBP[8] = {0};
    FloorComponent_FindResData(*(void**)(self + 0x10), outIBP);   // 0x140004680

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
            if (Object::IsDerivedFromNullSafe(cand, AnimalDescription::TypeId())) found = cand;
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
    uint8_t* p = reinterpret_cast<uint8_t*>(entityRef);

    uint32_t oldFlags = *(uint32_t*)(p + 0x20);
    uint32_t newFlags = oldFlags | 0x400000;
    *(uint32_t*)(p + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(p + 0x10) != 0) {
        ActHunt_NotifyEntityChanged_c3470(*(void**)(p + 0x10));   // 0x1400c3470
    }

    uint8_t famIBP[8] = {0};
    ActMv_FindComponentFamily_12d0(*(uintptr_t*)(p + 0x10), famIBP);   // 0x1400012d0
    uintptr_t fam = *(uintptr_t*)famIBP;
    uintptr_t model = *(uintptr_t*)(fam + 0x68);

    if (GameUpdateLoop::_singleton == nullptr) return;
    int mapW = **(int**)((uintptr_t)GameUpdateLoop::_singleton + 0xf8);
    int x = (int)(*(float*)(model + 0x158) + (float)(mapW >> 1));
    int y = (int)(*(float*)(model + 0x15c) + (float)(mapW >> 1));
    if (x < 0) x = 0;
    if (mapW <= x) x = mapW - 1;
    if (y < 0) y = 0;
    if (mapW <= y) y = mapW - 1;

    void* tmpl = *(void**)(p + 0x18);
    *(int*)(p + 0x28) = mapW * y + x;
    void* animalTmpl = reinterpret_cast<const Object::Base*>(tmpl)
                           ->IsDerivedFrom(AnimalDescription::TypeId()) ? tmpl : nullptr;   // B.3.x bridge
    PastureComponent_AnimalSetBounds(p, (short*)(p + 0x38), (short*)(p + 0x3a),
                                     (short*)(p + 0x3c), (short*)(p + 0x3e),
                                     *(int*)((uintptr_t)animalTmpl + 0x20));   // 0x140105e20

    uint8_t t[8] = {0};
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(t), 0.0f);
    // Immediate Update tick (vtable slot +0x48).
    reinterpret_cast<Component*>(p)->Update(t);   // B.3.8 bridge (slot 9, +0x48)
}

// Reimpl of FUN_1401240a0 (reassign animal ref's pasture tile after re-parenting)
// Looks up the new pasture owner's map footprint (via +0x28 link -> FindMapComp),
// reads its origin/size rect, then on the animal's own model component clears the
// in-pasture flag bit (0x400000), notifies, and copies the new rect into +0x38..+0x3e.
void PastureComponent_ReassignEntityRef(uintptr_t entityPtr) {
    if (*(int64_t*)(entityPtr + 0x28) == 0) return;

    int64_t link = *(int64_t*)(*(int64_t*)(entityPtr + 0x28) + 0x10);
    uint8_t mapIBP[8] = {0};
    FloorComponent_FindResData((void*)link, mapIBP);   // 0x140004680
    uintptr_t mapComp = *(uintptr_t*)mapIBP;
    uint16_t r0 = *(uint16_t*)(mapComp + 0x28);
    uint16_t r1 = *(uint16_t*)(mapComp + 0x2a);
    uint16_t r2 = *(uint16_t*)(mapComp + 0x2c);
    uint16_t r3 = *(uint16_t*)(mapComp + 0x2e);

    uint8_t modelIBP[8] = {0};
    ActHunt_FindComponent_5500(*(uintptr_t*)(entityPtr + 0x10), modelIBP);   // 0x140005500
    uintptr_t model = *(uintptr_t*)modelIBP;

    uint32_t oldFlags = *(uint32_t*)(model + 0x20);
    uint32_t newFlags = oldFlags & 0xffbfffff;
    *(uint32_t*)(model + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(model + 0x10) != 0) {
        ActHunt_NotifyEntityChanged_c3470(*(void**)(model + 0x10));   // 0x1400c3470
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
    Reimpl_ExternalPtr_Serialize_StringTable((void*)&field28, serializer);   // 0x140002340

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
            if (obj != nullptr) {
                bool valid = reinterpret_cast<const Object::Base*>(obj)
                                 ->IsDerivedFrom(LivestockComponent::TypeId());   // B.3.x bridge
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
    uint64_t capacity = PastureComponent_ComputeCapacity(reinterpret_cast<uintptr_t>(this));   // 0x140128570
    *(int*)(reinterpret_cast<uintptr_t>(this) + 0x40) = (int)capacity;
}
void PastureComponent::OnRemove() {
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
// The slot-12 helpers: 0x140002ed0 WorkBase_FindComponent_2ed0, 0x140123070
// Pf_RecursiveChildSum_123070, 0x140122e20 Pf_EntityResolvePos_2e20, 0x1400a8c90
// Pf_FineCellDirty_a8c90, 0x1400a6fe0 Pf_EnqueueCmd2_a6fe0.

void PastureComponent::OnEvent(int msgId) {
    if (msgId != 4) return;
    const uintptr_t self = reinterpret_cast<uintptr_t>(this);
    if (GameUpdateLoop::_singleton == nullptr) return;

    int64_t posIbp = 0;
    WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(self + 0x10), &posIbp);
    if (!posIbp) return;

    int* map = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    if (!map) return;
    const int width = *map;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(map) + 0x80);
    if (!grid || width <= 0) return;

    for (uint32_t i = 0; (int)i < Pf_RecursiveChildSum_123070(posIbp, 2); ++i) {
        float p[3] = {0, 0, 0};
        Pf_EntityResolvePos_2e20((void*)posIbp, p, 2, (int)i, nullptr);
        int x = (int)(p[0] + (float)(width >> 1));
        int y = (int)(p[1] + (float)(width >> 1));
        if (x < 0) x = 0; if (x >= width) x = width - 1;
        if (y < 0) y = 0; if (y >= width) y = width - 1;
        const int cell = width * y + x;

        uintptr_t nullIbp = 0;
        new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        uint8_t* terrain = reinterpret_cast<uint8_t*>(grid + 0x29 + (uintptr_t)cell * 0x30);
        const uint8_t was = *terrain;
        const bool wasRough = ((1u << (was & 0x1f)) & 0x1e0u) != 0;
        uint8_t* flag = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(map) + 0xa2);
        *flag = static_cast<uint8_t>(*flag | (wasRough ? 1 : 0));
        *terrain = 2;
        if (wasRough)
            Pf_FineCellDirty_a8c90(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(map) + 0x70),
                                   cell % width, cell / width);
        // The door typed args 3/4 as (int, uint64_t); the body declares them void*: the
        // same register values, cast at the call.
        Pf_EnqueueCmd2_a6fe0(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(map) + 0x68),
                             (uint32_t)cell, (void*)(uintptr_t)2, (void*)(uintptr_t)i);
        *reinterpret_cast<uintptr_t*>(grid + 8 + (uintptr_t)cell * 0x30) = nullIbp;
    }
}

// [13] 0x140127dd0 CommandMask / [14] 0x140127a60 Command / [15] 0x140127720 OnMessage
// live in pasturecomponent_states.cpp next to their helpers.

DEFINE_EXE_TYPE(PastureComponent, "ControllerComponent")
