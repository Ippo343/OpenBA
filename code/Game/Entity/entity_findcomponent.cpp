// code/Game/Entity/entity_findcomponent.cpp
// Entity::FindComponent<X> -- the exe's per-class monomorphisations of the component
// finder (each FUN_ below compares a component's Type() against X's type-id cell, then
// confirms with IsDerivedFrom). The shared walk is Entity_FindComponentByExeTypeId
// (below; it moved here from engine/funcs/engine_funcs.cpp at B3R W2-EZ).
//
// ENDGAME B.3.9 TR: these were inline bodies in engine_funcs.h reading the
// TypeId_*_FC cell pointers. They read X::TypeId() of the class AT THE CELL now
// (INVENTORY_C Table 1; several finder NAMES are historical misnomers, kept because the
// callers use them), which needs the class headers -- so the bodies moved here and
// engine_funcs.h keeps only the declarations.
#include "object/TypeInfo.h"
#include <cstdint>
#include "Component/MapComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/ConsumeProduceComponent.h"
#include "Component/BuildComponent.h"
#include "Component/DestroyComponent.h"
#include "Component/TradeComponent.h"
#include "Component/WorkComponent.h"
#include "Component/AttendanceComponent.h"
#include "Component/PastureComponent.h"
#include "Component/OrchardComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/ClearAreaComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/StorageComponent.h"
#include "Component/CemeteryComponent.h"
#include "Component/ImmigrationComponent.h"
#include "Component/TrackerComponent.h"
#include "Component/StatusIconComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MultiObjectComponent.h"
#include "Component/FireComponent.h"
#include "Component/StorageLocationComponent.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Entity/exe_bodies.h"
#include "object/Internal.h"   // ::Object::InternalBasePtr (Entity_FindComponentByExeTypeId)
#include <new>

// Entity_FindComponentByExeTypeId -- searches entity component array for a component
// matching the given type ID. Stores found component (or null) via InternalBasePtr.
//
// Entity layout (from decompilation of all FindComponent variants):
//   entity+0x30 = component pointer array (void**)
//   entity+0x38 = component count (int32)
//
// Each component's Object::Base::Type() (retail slot 2, +0x10) returns its type ID.
// IsDerivedFrom confirms the match before storing.
//
// Decompiled from FUN_140004680 (fn_FindComponent_Build) and confirmed identical
// across all 20+ FindComponent_X variants.
//
// B3R W2-EZ: moved here from engine/funcs/engine_funcs.cpp. Its `fn_IsDerivedFrom`
// read (the null-safe trampoline EngineFuncsInit installed) is
// Object::IsDerivedFromNullSafe, the same trampoline by name (B3R_RULES §3).
Component* Entity_FindComponentByExeTypeId(ComponentEntity* e, const uint32_t* typeIdPtr) {
    // The caller's out slot (an InternalPtr<T>, held as T*) is written by the typed
    // wrapper (Entity/exe_bodies.h) with what this returns: null on every miss.
    if (!e || !typeIdPtr) return nullptr;

    uint32_t targetTypeId = *typeIdPtr;

    // Component array ComponentEntity::_components (+0x30), count _components._size (+0x38)
    Object::InternalPtr<Component>* compArray = e->_components._data;
    int compCount = e->_components._size;

    Component* found = nullptr;
    for (int i = 0; i < compCount; i++) {
        Component* comp = compArray[i].Get();
        if (!comp) continue;

        // Object::Base::Type() (retail slot 2, +0x10) returns the component's type ID
        int compTypeId = static_cast<int>(comp->Type());

        if ((uint32_t)compTypeId == targetTypeId) {
            // Confirm with IsDerivedFrom
            if (::Object::IsDerivedFromNullSafe(comp, targetTypeId)) {
                found = comp;
            }
            break;
        }
    }

    // The wrapper stores it (InternalBasePtr::InternalBasePtr(retval, found): one qword store)
    return found;
}

namespace {
template <class T>
inline void FindByTypeId(ComponentEntity* entity, T** retval) {
    uint32_t typeId = T::TypeId();
    Entity_FindComponentByExeTypeId(entity, retval, &typeId);
}
}  // namespace

// B4 FORWARD: Dialog/residenceui.cpp still calls this (its local holds the
// StorageComponent as uintptr_t: known_port_issues 14a row 452 reads it as another class).
void fn_FindComponent_Storage(ComponentEntity* entity, uintptr_t* retval) {
    fn_FindComponent_Storage(entity, (StorageComponent**)retval);   // LEFT: SIG fn_FindComponent_Storage(uintptr_t*): residenceui.cpp:460 keeps the component in an integer because it reads +0x50 / +0x58 past StorageComponent (known_port_issues 14a row 452); typing the local changes that arithmetic
}

// fn_FindComponent_Build -- MapComponent (cell 0x1401c79e0), retail FUN_140004680. MISNOMER: the cell is MapComponent, not BuildComponent.
void fn_FindComponent_Build(ComponentEntity* entity, MapComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_BuildState -- StateMachineComponent (cell 0x1401c7d98), retail FUN_140003ea0.
void fn_FindComponent_BuildState(ComponentEntity* entity, StateMachineComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_ConsumeProduce -- ConsumeProduceComponent (cell 0x1401c7350), retail FUN_14000b6f0.
void fn_FindComponent_ConsumeProduce(ComponentEntity* entity, ConsumeProduceComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Position -- BuildComponent (cell 0x1401c7008), retail FUN_14000d6c0.
void fn_FindComponent_Position(ComponentEntity* entity, BuildComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Destroy -- DestroyComponent (cell 0x1401c75f0), retail FUN_140005780.
void fn_FindComponent_Destroy(ComponentEntity* entity, DestroyComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Building -- TradeComponent (cell 0x1401c4b80), retail FUN_140015140. MISNOMER: a TRADING-POST finder, not a "building" finder.
void fn_FindComponent_Building(ComponentEntity* entity, TradeComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Work -- WorkComponent (cell 0x1401c8070), retail FUN_140003050. WorkComponent, not WorkPlaceComponent (see fn_FindComponent_WorkPlace).
void fn_FindComponent_Work(ComponentEntity* entity, WorkComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Attendance -- AttendanceComponent (cell 0x1401c3f40), retail FUN_1400105e0.
void fn_FindComponent_Attendance(ComponentEntity* entity, AttendanceComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Pasture -- PastureComponent (cell 0x1401c7ba0), retail FUN_140004fe0.
void fn_FindComponent_Pasture(ComponentEntity* entity, PastureComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Orchard -- OrchardComponent (cell 0x1401c46b0), retail FUN_140005440.
void fn_FindComponent_Orchard(ComponentEntity* entity, OrchardComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_WorkPlace -- WorkPlaceComponent (cell 0x1401c80e0), retail FUN_140002e10.
void fn_FindComponent_WorkPlace(ComponentEntity* entity, WorkPlaceComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_SM -- ClearAreaComponent (cell 0x1401c71c8), retail FUN_14000a170.
void fn_FindComponent_SM(ComponentEntity* entity, ClearAreaComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Citizen -- CitizenComponent (cell 0x1401c7158), retail FUN_140009790.
void fn_FindComponent_Citizen(ComponentEntity* entity, CitizenComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Storage -- StorageComponent (cell 0x1401c7e78), retail FUN_140006270.
void fn_FindComponent_Storage(ComponentEntity* entity, StorageComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Cemetery -- CemeteryComponent (cell 0x1401c4090), retail FUN_140010a00.
void fn_FindComponent_Cemetery(ComponentEntity* entity, CemeteryComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Immigration -- ImmigrationComponent (cell 0x1401c4480), retail FUN_140012020.
void fn_FindComponent_Immigration(ComponentEntity* entity, ImmigrationComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Tracker -- TrackerComponent (cell 0x1401c4b10), retail FUN_140013320.
void fn_FindComponent_Tracker(ComponentEntity* entity, TrackerComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_MeshInstance -- StatusIconComponent (cell 0x1401c7e08), retail FUN_140002f90. MISNOMER: finds StatusIconComponent.
void fn_FindComponent_MeshInstance(ComponentEntity* entity, StatusIconComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Character -- ModelComponent (cell 0x1401c7a50), retail FUN_140003f60. MISNOMER: finds ModelComponent.
void fn_FindComponent_Character(ComponentEntity* entity, ModelComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_MultiObject -- MultiObjectComponent (cell 0x1401c7ac0), retail FUN_140004740.
void fn_FindComponent_MultiObject(ComponentEntity* entity, MultiObjectComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Fire -- FireComponent (cell 0x1401c41e0).
void fn_FindComponent_Fire(ComponentEntity* entity, FireComponent** retval) {
    FindByTypeId(entity, retval);
}

// fn_FindComponent_Unknown7ee8 -- StorageLocationComponent (cell 0x1401c7ee8). MISNOMER: the cell is StorageLocationComponent.
void fn_FindComponent_Unknown7ee8(ComponentEntity* entity, StorageLocationComponent** retval) {
    FindByTypeId(entity, retval);
}

// ============================================================
// FUN_14000b6f0 — FindComponent<ConsumeProduceComponent>: retail compares against the
// cell 0x1401c7350 = ConsumeProduceComponent (`MOV EDI,[0x1401c7350]` at 0x14000b733,
// again at 0x14000b78c for the IsDerivedFrom). The tree's body compared
// StateMachineComponent's id (cell 0x1401c7d98) -- corrected here. Retail callers:
// QuarryComponent slot 15 (0x140054bc7), FUN_1400bef70 (0x1400bf413), the
// ConsumeProduceUI slots and ConsumeProduceAction::Serialize.
// ============================================================
