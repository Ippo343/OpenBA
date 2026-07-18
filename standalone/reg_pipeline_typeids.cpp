// standalone/reg_pipeline_typeids.cpp
//
// Central type-id seeding for the StartTown building-assembly pipeline
// (ComponentEntity_impl_1fc0, 0x1400a1fc0) and its downstream FindComponent
// helpers. See notes/SCOPE_starttown_1fc0_pipeline.md ("central type-id seeding").
//
// The exe's shared FindComponent helpers (0x140003f60 = FindComponent(ModelComponent),
// 0x140004680 = FindComponent(MapComponent), etc.) match a sibling component by a CRC
// type-id read from a raw .rdata constant. In retail those constants are .data slots
// filled by each class's RegisterType at Object::Factory::Startup. In standalone they
// stay 0 (0xFFFFFFFF sentinel until we fill them) unless explicitly seeded, so the
// `if (typeId)` guard in the finder fails and it returns null. The confirmed symptom is
// the decal bug: FindComponent(ModelComponent) reads DWORD_1401c7a50, unseeded -> null
// -> DecalComponent read variant 0 (worked around locally in decalcomponent.cpp).
//
// Seed pattern mirrors reg_component_spawn.cpp / reg_economy_typeids.cpp:
//   (a) static backing-storage uint32_t per constant,
//   (b) RegisterPipelineTypeIds() maps each .rdata addr -> &storage via RegisterExeAddr,
//       gated on a live opt-in so a default (all-env-off) run stays byte-identical,
//   (c) FillPipelineTypeIds() fills each storage from Object::GetTypeIdByName AFTER
//       Engine::Startup (the CRC ids exist only then).
//
// Type names VERIFIED against Ghidra RegisterType writers (Application-x64.exe):
//   0x1401c7468  "CreatePlacedDescription"  (CreatePlacedDescription_RegisterType @14014ec10)
//   0x1401c7a50  "ModelComponent"           (ModelComponent_RegisterType         @14014f3d0)
//   0x1401c7a88  "ModelDescription"         (ModelDescription_Register           @14014f410)
//
// NOTE: the pipeline's fourth constant, 0x1401c79e0 = "MapComponent", is ALREADY
// registered+filled by RegisterEconomyTypeIds() / FillEconomyTypeIdGlobals()
// (reg_economy_typeids.cpp). It is intentionally NOT re-registered here to avoid a
// double RegisterExeAddr on the same slot.
//
// The integrator wires RegisterPipelineTypeIds() into address_registry.cpp and
// FillPipelineTypeIds() into game_main.cpp (after the Engine::Startup CRC probe).
#include "resolve.h"
#include "Object/factory_registration.h"     // Object::GetTypeIdByName
#include "Component/component_factory_base.h" // ComponentFactory::LiveEnabled
#include <cstdint>

// Backing storage for the pipeline type-id .rdata globals (filled after
// Engine::Startup — see FillPipelineTypeIds).
static uint32_t g_tid_CreatePlacedDescription = 0;  // 0x1401c7468
static uint32_t g_tid_ModelComponent          = 0;  // 0x1401c7a50
static uint32_t g_tid_ModelDescription        = 0;  // 0x1401c7a88

// Fill from the real-CRC type tree — call AFTER Engine::Startup.
extern "C" void FillPipelineTypeIds() {
    // No-op unless the storage was registered (live mode); harmless otherwise.
    g_tid_CreatePlacedDescription = ::Object::GetTypeIdByName("CreatePlacedDescription");
    g_tid_ModelComponent          = ::Object::GetTypeIdByName("ModelComponent");
    g_tid_ModelDescription        = ::Object::GetTypeIdByName("ModelDescription");
}

void RegisterPipelineTypeIds() {
    // GATED ON LIVE OPT-IN so a default (env-off) run is byte-for-byte unchanged:
    // seeding ModelComponent's type-id un-dormants the shared FindComponent(ModelComponent)
    // finder (0x140003f60) for EVERY caller, not just the decal. LiveEnabled returns true
    // when BANISHED_LIVE_DECAL (the primary consumer) OR the global BANISHED_LIVE_COMPONENTS
    // is set; with all envs off ResolveAddr stays null exactly as before.
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_DECAL")) return;
    RegisterExeAddr(0x1401c7468, reinterpret_cast<uintptr_t>(&g_tid_CreatePlacedDescription));
    RegisterExeAddr(0x1401c7a50, reinterpret_cast<uintptr_t>(&g_tid_ModelComponent));
    RegisterExeAddr(0x1401c7a88, reinterpret_cast<uintptr_t>(&g_tid_ModelDescription));
    // 0x1401c79e0 (MapComponent) intentionally omitted — already registered by
    // RegisterEconomyTypeIds() (reg_economy_typeids.cpp).
}
