// code/Game/Component/cropfieldcomponent.cpp
// Clean reimplementation of CropFieldComponent lifecycle methods from Application-x64.exe.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with crop flags
// and entity reference arrays for field tiles.
//
// Vtable at 0x140162ec0. One non-trivial hookable override:
//   Shutdown (0x14011ae70) — Component_Serialize + empty serializer + versioned block
//                            with entity ref validation, crop flags, IBP array, entity list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/cropfieldcomponent_funcs.h"
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "Entity/ComponentEntity.h"                 // ENDGAME B.3.9 C1: 0x1401c5d70 cell class
#include "Component/NaturalResourceComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7b30 cell class
#include "Component/CropFieldDescription.h"         // ENDGAME B.3.9 C1: 0x1401c7548 cell class
#include "Component/exe_bodies.h"
#include "system/MemoryAllocator.h"
#include "Game/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Player/exe_bodies.h"
#include "Game/exe_constants.h"
#include "Component/GrowthComponent.h"
#include "Component/GrowthDescription.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/StateMachineComponent.h"

// ============================================================
// Helpers, called directly (B.3R)
// ============================================================
//   FUN_14011aa20 CropFieldComponent_ValidateEntityRefs — validate entity references
//   FUN_140002340 Reimpl_ExternalPtr_Serialize_StringTable — serialize the IBP at +0x50
//   0x1400335b0   CropFieldComponent_ResizeEntityList (CropFieldComponent_helper_35b0)
//   0x1400336c0   CropFieldComponent_SerializeEntityList (CropFieldComponent_helper_36c0)

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// Extra sub-helpers needed by the reimpl bodies below.

// FUN_1400052c0 — FindComponent (BuildStateComponent variant) on a ComponentEntity:
//   WorkBase_FindComponent_52c0, called directly.
// Object::Base::IsA — the import slot 0x140151410 — is the member (Object::Base::IsA).

// Retail CropFieldComponent_helper_36c0 (0x1400336c0) disassembles to the
// dispatch slots ac8, ac8, aa0, a98, aa0, a98, a58 — i.e. two Serialize(int&),
// two InternalBasePtr::Serialize with an IsDerivedFrom check after each, and one
// Serialize(bool&). All of those are real C++ members now.
// ENDGAME B.3.9 C1 (B39_RULES.md D5): the three cells below are registered
// type-id cells (INVENTORY_C Table 1), read BY CELL. 0x1401c7b30 is
// NaturalResourceComponent (not "BuildStateComp" as the old local name said).


// Reimpl of FUN_14011aa20 (validate field-tile entity refs: drop stale/dead refs)
// Iterates the +0x58 array (stride 0x20, count at +0x60). For each "assigned"
// entry (state==1 at +0x4) it looks up the world entity by id; if it is gone, not
// a ComponentEntity, or its build-state component link no longer matches, the
// entry is reset to state 2 with null IBPs.
void CropFieldComponent::ValidateEntityRefs() {
    if (GameUpdateLoop::_singleton == nullptr) return;
    // GameUpdateLoop::m_game (+0xf8) is still void* (GAME-a) and the Map's cells are
    // untyped: the cell's entity read is LEFT.
    uintptr_t map = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton->m_game);

    int count = slots.count;
    CropSlot* arr = slots.data;
    for (int i = 0; i < count; i++) {
        CropSlot* entry = &arr[i];
        if (entry->state != 1) continue;

        int id = entry->cell;
        ComponentEntity* obj = *reinterpret_cast<ComponentEntity**>(
            *reinterpret_cast<uintptr_t*>(map + 0x80) + 8 + (int64_t)id * 0x30);
        bool keep = false;
        if (obj != nullptr) {
            if (obj->IsA(ComponentEntity::TypeId())) {
                if (obj->FindComponent<NaturalResourceComponent>() != nullptr) {   // 0x1400052c0
                    if (obj == entry->entity.Get()) keep = true;
                }
            }
        }
        if (keep) continue;

        // Stale: reset entry to state 2 and null the two IBP slots.
        entry->state = 2;
        entry->entity = ::Object::InternalPtr<ComponentEntity>(nullptr);
        entry->resource = ::Object::InternalPtr<NaturalResourceComponent>(nullptr);
    }
}

// Reimpl of CropFieldComponent_helper_35b0 @0x1400335b0
// (grow/shrink the +0x58 entity-ref array; element stride 0x20)
// Matches the standard grow-and-copy: count at [+8], capacity at [+0xc]. On grow,
// allocates cap*0x20, copies old [int,int,ptr,ptr,bool] entries, zero-inits new
// ones, frees old. On shrink, just updates count (no realloc).
// Collection::Array<CropSlot>::SetSize — a template instance with no Array<T> in the
// tree that writes this element's partial zero tail, kept as a free function on the
// typed header (LEFT, B4).
void CropFieldComponent_ResizeEntityList(CropSlotArray* arr, int newSize) {
    int oldCount = arr->count;
    if (newSize == oldCount) return;

    CropSlot* oldData = arr->data;
    arr->count = newSize;
    if (arr->capacity < newSize) {
        arr->capacity = newSize;
        if (newSize == oldCount + 1 && oldCount != 0) {
            arr->capacity = newSize * 3;
        }
        arr->data = static_cast<CropSlot*>(
            ::System::MemoryAllocator::MemAlloc((uint64_t)(arr->capacity) * 0x20, 8));
    }

    int startIdx = (oldData == arr->data) ? oldCount : 0;
    for (int64_t idx = startIdx; idx < newSize; idx++) {
        CropSlot* dst = &arr->data[idx];
        if (idx < oldCount) {
            const CropSlot* src = &oldData[idx];
            dst->cell = src->cell;
            dst->state = src->state;
            dst->entity = ::Object::InternalPtr<ComponentEntity>(nullptr);
            dst->entity = src->entity;
            dst->resource = ::Object::InternalPtr<NaturalResourceComponent>(nullptr);
            dst->resource = src->resource;
            dst->plantHere = src->plantHere;
        } else {
            dst->entity = ::Object::InternalPtr<ComponentEntity>(nullptr);
            dst->resource = ::Object::InternalPtr<NaturalResourceComponent>(nullptr);
        }
    }

    if (oldData != arr->data) {
        ::System::MemoryAllocator::MemFree(oldData);
    }
}

// Reimpl of CropFieldComponent_helper_36c0 @0x1400336c0
// (serialize the entity-ref array entries; element stride 0x20)
// Per entry: int id, int state, IBP(ComponentEntity), IBP(BuildStateComponent),
// bool. After each IBP is read it is type-validated and nulled if the type is wrong.
void CropFieldComponent_SerializeEntityList(IO::Serializer& serializer, int count, CropSlot* entry) {
    if (count <= 0) return;
    for (int i = 0; i < count; i++) {
        serializer.Serialize(entry->cell, nullptr);
        serializer.Serialize(entry->state, nullptr);
        // IBP #1 -> ComponentEntity
        entry->entity.InternalBasePtr::Serialize(serializer);
        if (entry->entity.Get() != nullptr &&
            !::Object::IsDerivedFromNullSafe(entry->entity.Get(), ComponentEntity::TypeId())) {
            entry->entity = ::Object::InternalPtr<ComponentEntity>(nullptr);
        }
        // IBP #2 -> NaturalResourceComponent (0x1401c7b30; the old local name here
        // said "BuildStateComp", but INVENTORY_C Table 1 says NaturalResourceComponent)
        entry->resource.InternalBasePtr::Serialize(serializer);
        if (entry->resource.Get() != nullptr &&
            !::Object::IsDerivedFromNullSafe(entry->resource.Get(), NaturalResourceComponent::TypeId())) {
            entry->resource = ::Object::InternalPtr<NaturalResourceComponent>(nullptr);
        }
        serializer.Serialize(entry->plantHere, nullptr);
        ++entry;
    }
}

// ============================================================
// CropFieldComponent::Shutdown (vtable slot [3], 0x14011ae70)
// ============================================================
// Complex entity reference serialization:
//   1. Component_Serialize (base)
//   2. Empty serializer (v0, immediately destroyed)
//   3. Main serializer (v0):
//      a. FUN_14011aa20 — validate entity refs
//      b. Serialize cropFlags as 2 bools (+0x48, +0x49)
//      c. FUN_140002340 — serialize IBP array at +0x50
//      d. Serialize entity count from +0x60
//      e. On load: resize entity list at +0x58
//      f. CropFieldComponent_helper_36c0 — serialize entity list entries

// ===== ENDGAME B.3.8 MI-B — CropFieldComponent's own slot bodies =====
// The class (CropFieldComponent.h) reaches these three through its members
// (OnMessage / CommandMask / Command, below the bodies).

// ============================================================================
// ENDGAME B.3.8 MI-B — CropFieldComponent as a REAL class (CropFieldComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/4/13/14/15/18-21 written from InstallLive, Inert_ScalarDtor in slot 0,
// Dispatcher<CropFieldCtrlTag> stamped into raw bytes at +0x28). Slots 18-21 held
// ControllerLive_ScalarDeletingDtor, the class's type info / id and `return 3`: that
// is CropFieldDescription's table 0x140162f50 read past this 18-slot table's end
// (B38 D3.5) — gone; nothing in the tree dispatches a component past slot 17.
// After: the compiler's 18-slot vtable, a real ctor/dtor chain, the controller a
// member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// 0x14011afd0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 (word) = 0x100; +0x50 =
// +0x58 = +0x60 = 0; `_flags |= 0x80` (already set by Component()).
// `MOV word [RBX+0x48],0x100`: +0x48 (work enabled) = 0, +0x49 ("a crop type is
// set") = 1; the tree's factory zero-filled both.
CropFieldComponent::CropFieldComponent()
    : _controller(this), workEnabled(false), hasCrop(true), _pad4A{}, cropDescription(nullptr),
      slots{nullptr, 0, 0} {
    OrFlagsInCtor(0x80);
}

// 0x14011af60: MemFree(+0x58), +0x58 = +0x60 = 0, then the restamps and ~Internal
// (the compiler chain).
CropFieldComponent::~CropFieldComponent() {
    Object::detail::InternalMemFree(slots.data);
    slots.data = nullptr;
    slots.count = 0;
    slots.capacity = 0;
}

// create 0x140032ed0: MemAlloc(0x68, 8), then the ctor.
void* CropFieldComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CropFieldComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CropFieldComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* CropFieldComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CropFieldComponent());
}

// [3] 0x14011ae70 (slot 3 is the first call into this class on a load-from-save).
void CropFieldComponent::Persist(IO::Stream& stream) {
    // Base Component serialization
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    IO::Stream& s = stream;

    // Empty serializer block (version 0, immediately destroyed)
    { IO::Serializer emptySerializer(s, 0); }

    // Main serializer block (version 0)
    IO::Serializer serializer(s, 0);

    // Validate entity references
    ValidateEntityRefs();   // 0x14011aa20

    // The two bools (+0x48 and +0x49)
    serializer.Serialize(workEnabled, nullptr);
    serializer.Serialize(hasCrop, nullptr);

    // ExternalPtr<ComponentDescription> at +0x50
    Reimpl_ExternalPtr_Serialize_StringTable(&cropDescription, &serializer);   // 0x140002340

    // Serialize entity count from +0x60
    int entityCount = slots.count;
    int countBuf = entityCount;
    serializer.Serialize(countBuf, nullptr);

    // On load, resize the entity list to the count just read.
    // ⚠ IO::Serializer +0x08 is m_encoding (a POINTER, never 0) — testing it as
    // the loading flag leaves this load-only branch dead on BOTH legs, so the
    // array is never grown and the loop below silently skips reading the IBPs
    // the save wrote -> stream desync. The flag is on the STREAM (+0x08).
    // Same defect as WorkPlaceComponent (21b3b33); see
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    // (The tree also tested the serializer's first qword, its stream pointer: it is
    // the stream `s`, never null, so the test is dropped -- value-identical.)
    if (static_cast<int>(serializer.GetDirection()) == 0) {   // (IO::Serializer::GetDirection, retail 0x180006210)
        CropFieldComponent_ResizeEntityList(&slots, countBuf);   // 0x1400335b0
    }

    // Serialize entity list entries
    CropFieldComponent_SerializeEntityList(serializer, countBuf, slots.data);   // 0x1400336c0
}
// [4] 0x14011ae60: `MOV qword [RCX+0x50],0` — one store, nothing else.
void CropFieldComponent::Startup() { cropDescription = nullptr; }
// [13] 0x14011ada0 / [14] 0x14011acc0 / [15] 0x14011ab50 — the bodies above. The
// OnMessage body takes the slot index from the base's third register (R9 = arg0).
// ── slot 13 — CanCommand — retail 0x14011ada0 ─────────────────────────────────────
// Decides which of the crop-field panel's three buttons are enabled. Unlike the
// ResourceProduce family's slot 13 (which ignores its argument and returns a mask),
// this one IS a per-command query:
//   cmd 0 (plant)   enabled when the SM sits in state 2  AND the field has slots
//   cmd 1 (harvest) enabled when the SM sits in state 10 AND the field has slots
//   cmd 2 (toggle)  returns a two-bit value: bit 1 = currently on (+0x48), bit 0 =
//                   selectable at all (has slots). Retail computes it as
//                   `-(+0x48 != 0) & 2 | hasSlots`, i.e. a MASK, not a boolean — the
//                   panel reads bit 1 to draw the toggle's state and bit 0 to enable
//                   it. Returning a plain bool here would collapse "on" and
//                   "available" into one and mis-draw the button.
// "Has slots" is self+0x50 != 0 — the slot array state 9 builds. An unplanted field
// therefore greys everything, which is retail behaviour, not a defect.
uint32_t CropFieldComponent::CommandMask(int command) {
    // The StateMachine bytes at StateMachineComponent +0x50 / +0x52 are raw in
    // COMP-e's header: LEFT.
    uintptr_t smc = reinterpret_cast<uintptr_t>(
        _entity->FindComponent<StateMachineComponent>());   // 0x140003ea0
    if (!smc) return 0;

    int8_t cur = *reinterpret_cast<int8_t*>(smc + 0x52);
    if (cur < 0) cur = *reinterpret_cast<int8_t*>(smc + 0x50);
    const bool hasSlots = (cropDescription != nullptr);

    switch (command) {
        case 0:  return (cur == 2  && hasSlots) ? 1 : 0;
        case 1:  return (cur == 10 && hasSlots) ? 1 : 0;
        case 2:  return static_cast<uint8_t>(
                     (workEnabled ? 2 : 0) |
                     (hasSlots ? 1 : 0));
        default: return 0;
    }
}
// ── slot 14 — Command — retail 0x14011acc0 ────────────────────────────────────────
//   cmd 0 -> post node event 3 (PLANT)    — the event state 2 registers
//   cmd 1 -> post node event 4 (HARVEST)  — the event state 10 registers
//   cmd 2 -> flip +0x48 and push it straight into the workplace accepting flag
// The two event ids line up exactly with the state table in
// cropfieldcomponent_states.cpp, so the panel drives the same paths the seasonal
// triggers do.
void CropFieldComponent::Command(int command) {
    // 0x140003ea0 FindComponent<StateMachineComponent>, 0x140002e10
    // FindComponent<WorkPlaceComponent>, 0x14012fc00 WorkPlaceComponent_SetAccepting.
    if (command != 0 && command != 1 && command != 2) return;

    if (command == 2) {
        workEnabled = !workEnabled;
        WorkPlaceComponent* wpc = _entity->FindComponent<WorkPlaceComponent>();
        if (!wpc) return;
        WorkPlaceComponent_SetAccepting(reinterpret_cast<int64_t>(wpc), workEnabled);
        return;
    }

    // The StateMachine sub-object (+0x48..+0x53) is raw in COMP-e's header: LEFT.
    uintptr_t smc = reinterpret_cast<uintptr_t>(
        _entity->FindComponent<StateMachineComponent>());
    if (!smc) return;

    // The current node is a ControllerComponent::Controller; its slot 2 is PostEvent
    // (retail `CALL [RAX+0x10]`). Every node in the tree is a compiler-tabled
    // Controller, so the old null-vtable / null-slot tests could never fire.
    const uintptr_t sm = smc + 0x48;
    ControllerComponent::Controller* node = *reinterpret_cast<ControllerComponent::Controller**>(sm);
    if (!node) return;
    const int curIdx  = *reinterpret_cast<int8_t*>(smc + 0x51);
    const int eventId = (command == 0) ? 3 : 4;
    node->PostEvent(reinterpret_cast<void*>(sm), curIdx, eventId);
}
// ── slot 15 — OnMessage — retail 0x14011ab50 ──────────────────────────────────────
// OnMessage(self, msgId, sender, slotIndex). Two messages, both addressed at ONE slot
// of the field's grid (self+0x58, stride 0x20, indexed by the 4th argument):
//
//   msg 0 — PLANT BOUND. The PlantArea work has just created the crop entity on this
//           slot's cell, so bind it: state (+0x04) = 1 "growing", stash the entity at
//           +0x08 and its NaturalResource component at +0x10, then copy the slot's
//           plant-mark byte (+0x18) into that component's +0x2d. This is what moves a
//           slot from "empty" to "growing", which is in turn what the state-9
//           per-frame handler counts to decide the field is planted and advance to
//           state 10. Without it a field plants and never progresses.
//
//   msg 1 — GROW TICK. Find the slot entity's growth component, then advance it by
//           Time(growthComp+0x28 * 300.0f * CropFieldDescription+0x24) through that
//           component's own vtable slot 9 (+0x48). 300.0f is 0x140172234.
//
// Retail's profile build carries two assertion strings for this function — "message
// send to planter harvester is wrong" and "map componment isn't initalized correctly!"
// [sic] — i.e. it treats a bad slot index or an uninitialised map as programmer error
// and derefs regardless. Kept unguarded to match, except where our standalone can
// legitimately have no entity on the cell (msg 1 already tests that in retail too).
void CropFieldComponent::OnMessage(int message, void* /*data*/, uint32_t arg0, uint32_t /*arg1*/) {
    if (GameUpdateLoop::_singleton == nullptr) return;

    // GameUpdateLoop::m_game (+0xf8) is still void* (GAME-a) and the Map's cells are
    // untyped: the cell's entity read is LEFT.
    const uintptr_t map = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton->m_game);
    if (!map) return;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(map + 0x80);
    if (!grid || !slots.data) return;

    CropSlot* slot = &slots.data[arg0];
    const int cell = slot->cell;
    ComponentEntity* const entity =
        *reinterpret_cast<ComponentEntity**>(grid + 8 + (uintptr_t)cell * 0x30);

    if (message == 0) {
        NaturalResourceComponent* nr = entity->FindComponent<NaturalResourceComponent>();   // 0x1400052c0
        slot->state    = 1;          // growing
        slot->entity   = entity;
        slot->resource = nr;
        // +0x2d is the high byte of NaturalResourceComponent::resourceFlags (COMP-d): LEFT.
        if (nr)
            *reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(nr) + 0x2d) =
                static_cast<char>(slot->plantHere);
        return;
    }

    if (message == 1) {
        if (!entity) return;
        GrowthComponent* growth = entity->FindComponent<GrowthComponent>();   // 0x14000c460
        if (!growth) return;

        Description* desc = _description;
        const uint32_t tidCropDesc = CropFieldDescription::TypeId();
        if (!(desc && ::Object::IsDerivedFromNullSafe(desc, tidCropDesc)))
            return;

        const float rate = growth->config->_f28;
        const float mult = static_cast<CropFieldDescription*>(desc)->_f24;
        ::System::Time t = {0};
        RtSystem_Time_Ctor(&t, rate * kRdata_140172234 * mult);   // 300.0f

        growth->Update(&t);   // slot 9
    }
}

DEFINE_EXE_TYPE(CropFieldComponent, "ControllerComponent")
