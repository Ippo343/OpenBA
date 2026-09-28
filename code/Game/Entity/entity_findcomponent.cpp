// code/Game/Entity/entity_findcomponent.cpp
// Entity::FindComponent<X> -- the exe's per-class monomorphisations of the component
// finder (each FUN_ below compares a component's Type() against X's type-id cell, then
// confirms with IsDerivedFrom). The shared walk is Entity_FindComponentByExeTypeId
// (engine/funcs/engine_funcs.cpp).
//
// ENDGAME B.3.9 TR: these were inline bodies in engine_funcs.h reading the
// TypeId_*_FC cell pointers. They read X::TypeId() of the class AT THE CELL now
// (INVENTORY_C Table 1; several finder NAMES are historical misnomers, kept because the
// callers use them), which needs the class headers -- so the bodies moved here and
// engine_funcs.h keeps only the declarations.
#include "engine_funcs.h"
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
#include "Entity/exe_bodies.h"

namespace {
inline void FindByTypeId(void* entity, void* retval, uint32_t typeId) {
    Entity_FindComponentByExeTypeId(entity, retval, &typeId);
}
}  // namespace

// fn_FindComponent_Build -- MapComponent (cell 0x1401c79e0), retail FUN_140004680. MISNOMER: the cell is MapComponent, not BuildComponent.
void fn_FindComponent_Build(void* entity, void* retval) {
    FindByTypeId(entity, retval, MapComponent::TypeId());
}

// fn_FindComponent_BuildState -- StateMachineComponent (cell 0x1401c7d98), retail FUN_140003ea0.
void fn_FindComponent_BuildState(void* entity, void* retval) {
    FindByTypeId(entity, retval, StateMachineComponent::TypeId());
}

// fn_FindComponent_ConsumeProduce -- ConsumeProduceComponent (cell 0x1401c7350), retail FUN_14000b6f0.
void fn_FindComponent_ConsumeProduce(void* entity, void* retval) {
    FindByTypeId(entity, retval, ConsumeProduceComponent::TypeId());
}

// fn_FindComponent_Position -- BuildComponent (cell 0x1401c7008), retail FUN_14000d6c0.
void fn_FindComponent_Position(void* entity, void* retval) {
    FindByTypeId(entity, retval, BuildComponent::TypeId());
}

// fn_FindComponent_Destroy -- DestroyComponent (cell 0x1401c75f0), retail FUN_140005780.
void fn_FindComponent_Destroy(void* entity, void* retval) {
    FindByTypeId(entity, retval, DestroyComponent::TypeId());
}

// fn_FindComponent_Building -- TradeComponent (cell 0x1401c4b80), retail FUN_140015140. MISNOMER: a TRADING-POST finder, not a "building" finder.
void fn_FindComponent_Building(void* entity, void* retval) {
    FindByTypeId(entity, retval, TradeComponent::TypeId());
}

// fn_FindComponent_Work -- WorkComponent (cell 0x1401c8070), retail FUN_140003050. WorkComponent, not WorkPlaceComponent (see fn_FindComponent_WorkPlace).
void fn_FindComponent_Work(void* entity, void* retval) {
    FindByTypeId(entity, retval, WorkComponent::TypeId());
}

// fn_FindComponent_Attendance -- AttendanceComponent (cell 0x1401c3f40), retail FUN_1400105e0.
void fn_FindComponent_Attendance(void* entity, void* retval) {
    FindByTypeId(entity, retval, AttendanceComponent::TypeId());
}

// fn_FindComponent_Pasture -- PastureComponent (cell 0x1401c7ba0), retail FUN_140004fe0.
void fn_FindComponent_Pasture(void* entity, void* retval) {
    FindByTypeId(entity, retval, PastureComponent::TypeId());
}

// fn_FindComponent_Orchard -- OrchardComponent (cell 0x1401c46b0), retail FUN_140005440.
void fn_FindComponent_Orchard(void* entity, void* retval) {
    FindByTypeId(entity, retval, OrchardComponent::TypeId());
}

// fn_FindComponent_WorkPlace -- WorkPlaceComponent (cell 0x1401c80e0), retail FUN_140002e10.
void fn_FindComponent_WorkPlace(void* entity, void* retval) {
    FindByTypeId(entity, retval, WorkPlaceComponent::TypeId());
}

// fn_FindComponent_SM -- ClearAreaComponent (cell 0x1401c71c8), retail FUN_14000a170.
void fn_FindComponent_SM(void* entity, void* retval) {
    FindByTypeId(entity, retval, ClearAreaComponent::TypeId());
}

// fn_FindComponent_Citizen -- CitizenComponent (cell 0x1401c7158), retail FUN_140009790.
void fn_FindComponent_Citizen(void* entity, void* retval) {
    FindByTypeId(entity, retval, CitizenComponent::TypeId());
}

// fn_FindComponent_Storage -- StorageComponent (cell 0x1401c7e78), retail FUN_140006270.
void fn_FindComponent_Storage(void* entity, void* retval) {
    FindByTypeId(entity, retval, StorageComponent::TypeId());
}

// fn_FindComponent_Residence -- StorageComponent (cell 0x1401c7e78), retail FUN_140006270. same function as fn_FindComponent_Storage.
void fn_FindComponent_Residence(void* entity, void* retval) {
    FindByTypeId(entity, retval, StorageComponent::TypeId());
}

// fn_FindComponent_Cemetery -- CemeteryComponent (cell 0x1401c4090), retail FUN_140010a00.
void fn_FindComponent_Cemetery(void* entity, void* retval) {
    FindByTypeId(entity, retval, CemeteryComponent::TypeId());
}

// fn_FindComponent_Immigration -- ImmigrationComponent (cell 0x1401c4480), retail FUN_140012020.
void fn_FindComponent_Immigration(void* entity, void* retval) {
    FindByTypeId(entity, retval, ImmigrationComponent::TypeId());
}

// fn_FindComponent_Tracker -- TrackerComponent (cell 0x1401c4b10), retail FUN_140013320.
void fn_FindComponent_Tracker(void* entity, void* retval) {
    FindByTypeId(entity, retval, TrackerComponent::TypeId());
}

// fn_FindComponent_MeshInstance -- StatusIconComponent (cell 0x1401c7e08), retail FUN_140002f90. MISNOMER: finds StatusIconComponent.
void fn_FindComponent_MeshInstance(void* entity, void* retval) {
    FindByTypeId(entity, retval, StatusIconComponent::TypeId());
}

// fn_FindComponent_Character -- ModelComponent (cell 0x1401c7a50), retail FUN_140003f60. MISNOMER: finds ModelComponent.
void fn_FindComponent_Character(void* entity, void* retval) {
    FindByTypeId(entity, retval, ModelComponent::TypeId());
}

// fn_FindComponent_MultiObject -- MultiObjectComponent (cell 0x1401c7ac0), retail FUN_140004740.
void fn_FindComponent_MultiObject(void* entity, void* retval) {
    FindByTypeId(entity, retval, MultiObjectComponent::TypeId());
}

// fn_FindComponent_Fire -- FireComponent (cell 0x1401c41e0).
void fn_FindComponent_Fire(void* entity, void* retval) {
    FindByTypeId(entity, retval, FireComponent::TypeId());
}

// fn_FindComponent_Unknown7ee8 -- StorageLocationComponent (cell 0x1401c7ee8). MISNOMER: the cell is StorageLocationComponent.
void fn_FindComponent_Unknown7ee8(void* entity, void* retval) {
    FindByTypeId(entity, retval, StorageLocationComponent::TypeId());
}
