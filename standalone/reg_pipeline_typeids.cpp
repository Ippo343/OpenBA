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
#include <cstdint>

// Backing storage for the pipeline type-id .rdata globals (filled after
// Engine::Startup — see FillPipelineTypeIds).
static uint32_t g_tid_CreatePlacedDescription = 0;  // 0x1401c7468
static uint32_t g_tid_ModelComponent          = 0;  // 0x1401c7a50
static uint32_t g_tid_ModelDescription        = 0;  // 0x1401c7a88
// 0x1401c7580 is DecalComponent — the RegisterType at 0x14014edd0 passes
// rdx="Decal" / r8="Component", the same name+suffix pair ModelComponent uses at
// 0x14014f3d0. Read by the shared FindComponent<DecalComponent> (0x14000b630),
// which the DestroyComponent state-6 rubble step uses to re-stamp the scorch mark.
static uint32_t g_tid_DecalComponent          = 0;  // 0x1401c7580
// FencedTool (pasture/crop-field) Startup constants — engine_funcs.cpp reads all
// three via ResolveAddr; unregistered they stayed null, FencedTool_Startup found
// no CreateFencedDescription, and the first fenced activation crashed on the
// null-desc +0x28 read (Startup_Impl+0xc4, 2026-07-30). NOTE the fencedtool.cpp /
// FencedTool.h labels "SizeDescription"(0x1401c7468) / "FenceDescription"
// (0x1401c7a88) are MISNOMERS for CreatePlacedDescription / ModelDescription —
// both already seeded above.
static uint32_t g_tid_CreateFencedDescription = 0;  // 0x1401c4100
// 0x1401c7a18 is MapDescription (factory_registrations.csv:277,
// MapDescription_Register @ 0x14014f390). It was seeded by the name
// "AreaDescription" until 2026-07-31 — a type that exists NOWHERE in the game — so
// GetTypeIdByName returned 0, that zero became the .rdata slot's backing storage,
// and PlacementTool_Startup's findComp searched for type-id 0 and matched nothing.
// Consequences: tool+0x30 (the template's MapDescription) stayed null, and BOTH
// PlacementTool slot-7 Serialize (retail 0x1400fdee0) and fd650 deref it unguarded
// — so EVERY player building placement crashed on the first preview frame — while
// MapComponent went silently dead (GetMapDescription returned null for every
// building, so Draw/OnCreate always returned early).
static uint32_t g_tid_MapDescription          = 0;  // 0x1401c7a18
static uint32_t g_tid_RotationSetting         = 0;  // 0x1401c63c8
// SelectResourceUI combo populate (selectresourceui.cpp IterateResources)
// derefs *g_TypeId_SelectDescription — null until seeded (PastureUI promotion
// crash 2026-07-30, IterateResources+0x107).
static uint32_t g_tid_SelectDescription       = 0;  // 0x1401c48a8

// Fill from the real-CRC type tree — call AFTER Engine::Startup.
extern "C" void FillPipelineTypeIds() {
    // No-op unless the storage was registered (live mode); harmless otherwise.
    g_tid_CreatePlacedDescription = ::Object::GetTypeIdByName("CreatePlacedDescription");
    g_tid_ModelComponent          = ::Object::GetTypeIdByName("ModelComponent");
    g_tid_ModelDescription        = ::Object::GetTypeIdByName("ModelDescription");
    g_tid_DecalComponent          = ::Object::GetTypeIdByName("DecalComponent");
    g_tid_CreateFencedDescription = ::Object::GetTypeIdByName("CreateFencedDescription");
    g_tid_MapDescription          = ::Object::GetTypeIdByName("MapDescription");
    g_tid_RotationSetting         = ::Object::GetTypeIdByName("RotationSetting");
    g_tid_SelectDescription       = ::Object::GetTypeIdByName("SelectDescription");
}

void RegisterPipelineTypeIds() {
    // Seeding ModelComponent's type-id un-dormants the shared
    // FindComponent(ModelComponent) finder (0x140003f60) for EVERY caller, not just
    // the decal. Unconditional since 2026-09-13 (the decal gate graduated).
    RegisterExeAddr(0x1401c7468, reinterpret_cast<uintptr_t>(&g_tid_CreatePlacedDescription));
    RegisterExeAddr(0x1401c7a50, reinterpret_cast<uintptr_t>(&g_tid_ModelComponent));
    RegisterExeAddr(0x1401c7a88, reinterpret_cast<uintptr_t>(&g_tid_ModelDescription));
    RegisterExeAddr(0x1401c7580, reinterpret_cast<uintptr_t>(&g_tid_DecalComponent));
    RegisterExeAddr(0x1401c4100, reinterpret_cast<uintptr_t>(&g_tid_CreateFencedDescription));
    RegisterExeAddr(0x1401c7a18, reinterpret_cast<uintptr_t>(&g_tid_MapDescription));
    RegisterExeAddr(0x1401c63c8, reinterpret_cast<uintptr_t>(&g_tid_RotationSetting));
    RegisterExeAddr(0x1401c48a8, reinterpret_cast<uintptr_t>(&g_tid_SelectDescription));
    // 0x1401c79e0 (MapComponent) intentionally omitted — already registered by
    // RegisterEconomyTypeIds() (reg_economy_typeids.cpp).
}
