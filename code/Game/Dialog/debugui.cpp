// code/Game/Dialog/debugui.cpp
// Clean reimplementation of DebugUI panel methods.
//
// DebugUI (0x130 bytes) -- developer debug/cheat panel controller.
// Extends DialogController (0x30 base).
// Contains resource spawn buttons, disease/weather controls, rendering
// debug toggles, lighting color sliders, and performance counters.
//
// Decompiled from Application-x64.exe, with element names recovered
// from Application-x64-profile.exe debug strings.
//
// Slot4 (setup), Slot7, Slot8, and Slot9 are cleanly reimplemented here.

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ComboElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/NumberElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "graphics/Scene.h"
#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time / System::Random
#include "audio/Audio.h"       // ENDGAME B.3.7 callers (K4)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/CitizenComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/ComponentDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/RawMaterialDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/InteractComponent.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/InteractDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/CitizenDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Game/Disease.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/PopulationDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "engine/MainLoop.h"   // ENDGAME B.3.9 SS2: Engine::MainLoop::_singleton
#include "Component/exe_bodies.h"             // B3R: the bodies called by name
#include "Action/exe_bodies.h"                // B3R: Pf_RecursiveChildSum_123070, Pf_EntityResolvePos_2e20
#include "Entity/componententity_funcs.h"     // B3R: ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Component/firecomponent_funcs.h"    // B3R: FireComponent_Ignite_610
#include "Component/diseasecomponent_funcs.h" // B3R: DiseaseComp_Infect
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// Static inline implementations (replaces fn_* exe pointer calls)
// ============================================================

// Game_GetMonthFromTime (FUN_1400932A0)
// Converts weather manager time to month index 0-11.
// param_1 = weatherMgr ptr; time stored at +8.
static int DebugUI_GetMonthFromTime_Impl(void* weatherMgr) {
    float seconds = reinterpret_cast<const System::Time*>((void*)((uintptr_t)weatherMgr + 8))->AsSeconds();   // B.3.4 bridge
    int month = (int)((seconds / 3600.0f) * 12.0f);  // 3600s/yr (DAT_14017225c), 12 months
    if (month > 11) month = 0;
    return month;
}

// DebugUI_SetDayLighting (FUN_140061780)
// Reads per-month lighting data from weather table, writes to slider elements.
// Weather table = **(gameWorld+0x130); entry stride = 0x20; month=param_2.
// Entry layout (+month*0x20): [+0x38]=R [+0x3d]=G [+0x3c]=B sun byte channels
// Sun: [0x3e]=R [0x3d]=G [0x3c]=B; Amb: [0x3a]=R [0x39]=G [0x38]=B;
// Fog: [0x42]=R [0x41]=G [0x40]=B; sun H/P at [0x50/0x54]; fog N/F/M at [0x44/0x48/0x4c]
static void DebugUI_SetDayLighting_Impl(DebugUI* self, int month) {
    uintptr_t s = (uintptr_t)self;
    // weatherTable = **(gameWorld+0x130) == envRes (null if Environment.rsc
    // failed to load; guard so the reads below don't hit a wild low address).
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    uintptr_t weatherTable = weatherListPtr ? *(uintptr_t*)weatherListPtr : 0;
    if (!weatherTable) return;
    uintptr_t entry = weatherTable + (uintptr_t)month * 0x20;

    *(float*)(*(uintptr_t*)(s + 0x80) + 0x128) = (float)*(uint8_t*)(entry + 0x3e); // sunR
    *(float*)(*(uintptr_t*)(s + 0x88) + 0x128) = (float)*(uint8_t*)(entry + 0x3d); // sunG
    *(float*)(*(uintptr_t*)(s + 0x90) + 0x128) = (float)*(uint8_t*)(entry + 0x3c); // sunB
    *(uint32_t*)(*(uintptr_t*)(s + 0xe0) + 0x128) = *(uint32_t*)(entry + 0x50);    // sunH (float)
    *(uint32_t*)(*(uintptr_t*)(s + 0xe8) + 0x128) = *(uint32_t*)(entry + 0x54);    // sunP (float)
    *(float*)(*(uintptr_t*)(s + 0x98) + 0x128) = (float)*(uint8_t*)(entry + 0x3a); // ambR
    *(float*)(*(uintptr_t*)(s + 0xa0) + 0x128) = (float)*(uint8_t*)(entry + 0x39); // ambG
    *(float*)(*(uintptr_t*)(s + 0xa8) + 0x128) = (float)*(uint8_t*)(entry + 0x38); // ambB
    *(float*)(*(uintptr_t*)(s + 0xb0) + 0x128) = (float)*(uint8_t*)(entry + 0x42); // fogR
    *(float*)(*(uintptr_t*)(s + 0xb8) + 0x128) = (float)*(uint8_t*)(entry + 0x41); // fogG
    *(float*)(*(uintptr_t*)(s + 0xc0) + 0x128) = (float)*(uint8_t*)(entry + 0x40); // fogB
    *(uint32_t*)(*(uintptr_t*)(s + 0xc8) + 0x128) = *(uint32_t*)(entry + 0x44);    // fogN (float)
    *(uint32_t*)(*(uintptr_t*)(s + 0xd0) + 0x128) = *(uint32_t*)(entry + 0x48);    // fogF (float)
    *(float*)(*(uintptr_t*)(s + 0xd8) + 0x128) =
        *(float*)(entry + 0x4c) * 255.0f;  // fogM (normalize)
}

// DebugUI_WriteLightingColors (FUN_140061530)
// Reads slider values from UI elements, writes packed ARGB colors to weather table.
// Inverse of SetDayLighting: slider → weather table.
static void DebugUI_WriteLightingColors_Impl(DebugUI* self, int month) {
    uintptr_t s = (uintptr_t)self;
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    // weatherTable == *(weatherSim+0) == envRes (Environment.rsc keyframe table).
    // Null if Environment.rsc failed to load; without this guard the per-month
    // writes below would target (0 + month*0x20) — a wild low-address store.
    uintptr_t weatherTable = weatherListPtr ? *(uintptr_t*)weatherListPtr : 0;
    if (!weatherTable) return;
    uintptr_t entry = weatherTable + (uintptr_t)month * 0x20;

    float sunR = *(float*)(*(uintptr_t*)(s + 0x80) + 0x128);
    float sunG = *(float*)(*(uintptr_t*)(s + 0x88) + 0x128);
    // Pack sun ARGB at entry+0x3c
    *(uint32_t*)(entry + 0x3c) = ((int)sunR << 8 | (int)sunG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0x90) + 0x128) | 0xFF000000;

    float ambR = *(float*)(*(uintptr_t*)(s + 0x98) + 0x128);
    float ambG = *(float*)(*(uintptr_t*)(s + 0xa0) + 0x128);
    // Pack amb ARGB at entry+0x38
    *(uint32_t*)(entry + 0x38) = ((int)ambR << 8 | (int)ambG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0xa8) + 0x128) | 0xFF000000;

    float fogR = *(float*)(*(uintptr_t*)(s + 0xb0) + 0x128);
    float fogG = *(float*)(*(uintptr_t*)(s + 0xb8) + 0x128);
    // Pack fog ARGB at entry+0x40
    *(uint32_t*)(entry + 0x40) = ((int)fogR << 8 | (int)fogG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0xc0) + 0x128) | 0xFF000000;

    *(uint32_t*)(entry + 0x50) = *(uint32_t*)(*(uintptr_t*)(s + 0xe0) + 0x128); // sunH
    *(uint32_t*)(entry + 0x54) = *(uint32_t*)(*(uintptr_t*)(s + 0xe8) + 0x128); // sunP
    *(uint32_t*)(entry + 0x44) = *(uint32_t*)(*(uintptr_t*)(s + 0xc8) + 0x128); // fogN
    *(uint32_t*)(entry + 0x48) = *(uint32_t*)(*(uintptr_t*)(s + 0xd0) + 0x128); // fogF
    *(float*)(entry + 0x4c) = *(float*)(*(uintptr_t*)(s + 0xd8) + 0x128) / 255.0f; // fogM
}

// DebugUI_OnDaySliderChanged (FUN_1400607E0)
// Reads slider value (0-359), converts to game time, resets weather, sets lighting.
// DAT_140172238 = 360.0f (slider range), DAT_14017225c = 3600.0f (seconds/year)
// DAT_1401721cc = 30.0f (days/month for month index calc)
static void DebugUI_OnDaySliderChanged_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
    float sliderVal = *(float*)(*(uintptr_t*)(s + 0x78) + 0x128);

    // Convert slider (0-359) to game time in ticks
    int64_t timeTicks;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeTicks), (sliderVal / 360.0f) * 3600.0f);

    // Set game world time
    *(int64_t*)(*(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x128) + 8) = timeTicks;

    // Reset weather time to 0
    int64_t zeroTick;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&zeroTick), 0.0f);
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    Weather_ResetTime_Reimpl(weatherListPtr, &zeroTick);   // 0x1400944e0

    // Update lighting sliders for the new month
    int monthIdx = (int)(sliderVal / 30.0f);  // 30 days/month
    DebugUI_SetDayLighting_Impl(self, monthIdx);
}

// DebugUI_OnDiseaseComboChanged (FUN_14005FC60)
// Handles disease combo selection — updates internal pointer.
// self+0x38 = diseaseCombo (InternalBasePtr); combo item data is the disease entity.
static void DebugUI_OnDiseaseComboChanged_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
    int selIdx = *(int*)(*(uintptr_t*)(s + 0x38) + 0x118);  // selected item index
    if (selIdx > 3) {
        // Only take action if index > 3 (first 3 are non-disease entries)
        void* comboElem = *(void**)(s + 0x38);
        void* itemData = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemData(selIdx);  // B.3.6 bridge
        // Construct InternalBasePtr from item data — used internally by disease system
        uint8_t basePtrBuf[8];
        new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(itemData));   // B.3.3 bridge
        // (result discarded — original function only constructs local_res8 on stack)
    }
}

// ============================================================
// DebugUI_OnDrawComboChanged (FUN_1400608A0)
// Handles draw combo selection — shows/hides scene debug geometry.
//
// When a draw combo item is selected, this function iterates through
// all draw entries in the draw manager (world+0x130). For the selected
// entry, it creates a DrawGeometry + AudioObjectGroup if they don't
// exist, adds them to the graphics/audio scenes. For all other entries,
// it hides the geometry by setting alpha to -1.0f.
//
// Draw manager struct layout (pointed to by world+0x130):
//   R14 = *(world+0x130) — draw manager ptr
//   *R14 = inner data ptr:
//     +0x28 = ptr to array of draw descriptors (0x10 stride per entry)
//     +0x30 = count of draw entries
//   R14+0x10 = state field (int, set to 0 on show, 3 on hide)
//   R14+0x50 = ptr to array of Geometry* (one per draw entry)
//   R14+0x60 = ptr to array of audio object ptrs (one per draw entry)
//
// Draw descriptor entry (at inner[+0x28] + i*0x10):
//   +0x00 = geometry descriptor ptr (passed to DrawGeometry_Init)
//   +0x08 = audio descriptor ptr (null if no audio; +0x20 = sound type)
//
// Audio component (found via Entity_FindAudioComponent):
//   +0x28 = position (Vector3, 12 bytes)
//   +0x38 = emitter array ptr (each entry 0x10: [emitterId(4), ?(4), ...])
//   +0x40 = emitter count
//
// Original: 0x1400608A0
// ============================================================
static void DebugUI_OnDrawComboChanged_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;

    // The DrawGeometry ctor / init are 0x1400d3f10 WeatherParticle_GeoCtor and
    // 0x1400d3f80 WeatherParticle_Config, called by name (B3R; the old
    // "not reimplemented" bail-out never ran: both are registered).

    // Get selected combo index
    int selectedIdx = *(int*)(*(uintptr_t*)(s + 0x108) + 0x118);

    // Get item data (cast to int index into draw list)
    void* itemData = reinterpret_cast<const UI::ComboElement*>(*(void**)(s + 0x108))->GetItemData(selectedIdx);  // B.3.6 bridge
    int selectedDrawIdx = (int)(intptr_t)itemData;

    // Get draw manager from game world
    uintptr_t world = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t* drawMgr = *(uintptr_t**)(world + 0x130);  // R14 = plVar1

    uintptr_t innerData = *(uintptr_t*)drawMgr;  // *plVar1
    int drawCount = *(int*)(innerData + 0x30);
    uintptr_t descArray = *(uintptr_t*)(innerData + 0x28);  // array of 0x10-stride descriptors

    void** geometryArray = (void**)(drawMgr[10]);  // R14+0x50 = plVar1[10] (offset 0x50)
    // R14+0x60 = plVar1[0xC] (offset 0x60) is the audio object array; only the
    // PORT GAP audio branch below reads it in retail.

    for (int i = 0; i < drawCount; i++) {
        if (selectedDrawIdx == i) {
            // Selected entry — create and show geometry if not already created
            if (geometryArray[i] == nullptr) {
                // Allocate and construct DrawGeometry (0xf0 bytes)
                void* geomMem = System::MemoryAllocator::MemAlloc(0xf0, 8);
                void* geometry = nullptr;
                if (geomMem) {
                    geometry = WeatherParticle_GeoCtor(geomMem);   // 0x1400d3f10
                }
                geometryArray[i] = geometry;

                // Initialize with descriptor data
                uintptr_t descPtr = *(uintptr_t*)(descArray + (uintptr_t)i * 0x10);
                WeatherParticle_Config(geometryArray[i], (void*)descPtr);   // 0x1400d3f80

                // Add to graphics scene
                void* graphicsScene = *(void**)(world + 0x60);
                reinterpret_cast<Graphics::Scene*>(graphicsScene)
                    ->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(
                        geometryArray[i]));   // B.3.5 bridge

                // Reset state to 0 (showing)
                *(int*)((uintptr_t)drawMgr + 0x10) = 0;

                // Check if this entry has an audio descriptor
                uintptr_t audioDescPtr = *(uintptr_t*)(descArray + (uintptr_t)i * 0x10 + 8);
                // PORT GAP 0x140002280 Entity::FindAudioComponent: not ported; retail
                // calls it here. Retail's audio branch for an entry with an audio
                // descriptor (0x48-byte group built from the descriptor, registered
                // with the manager at world+0xE8 under the descriptor's +0x20 sound type,
                // then FindAudioComponent(group) -> move every emitter to the origin and
                // mute the first) waited on the finder, so today an entry carrying an
                // audio template shows its geometry only. This is that null arm.
                if (audioDescPtr != 0) {
                    static bool warnedAudio = false;
                    if (!warnedAudio) {
                        warnedAudio = true;
                        fprintf(stderr, "[DebugUI::OnDrawComboChanged] audio helpers "
                            "not reimplemented; showing geometry without ambient audio\n");
                    }
                }
            }
        } else {
            // Non-selected entry — hide geometry if it exists
            void* geometry = geometryArray[i];
            if (geometry != nullptr) {
                // Set alpha to -1.0f (0xbf800000) to hide
                *(uint32_t*)((uintptr_t)geometry + 0xC4) = 0xbf800000;
                // Set state to 3 (hiding)
                *(int*)((uintptr_t)drawMgr + 0x10) = 3;
            }
        }
    }
}

// SetEngineNextLoop — now inline in engine_funcs.h as SetEngineNextLoop()
// (was static SetEngineNextLoop_Impl here, promoted to shared inline)

// ============================================================
// Game_DestroyAllRawMaterials (FUN_140060000)
// Iterates ExternalFactory for all entities with ComponentDescription (DAT_1401c7318).
// For each entity, searches components for RawMaterialDescription (DAT_1401c7cb8).
// If found, destroys the entity via Game_DestroyEntity.
// ============================================================
static void Game_DestroyAllRawMaterials_Impl() {
    void* factory = ::Object::ExternalFactory::_singleton;
    const ::Object::ExternalFactory* efactoryRaw =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    // Iterate all entities with ComponentDescription type
    void* entity = const_cast<::Object::External*>(
        efactoryRaw->FindNext(ComponentDescription::TypeId(), nullptr));  // B.3.3 bridge
    while (entity) {
        // IsDerivedFrom check against ComponentDescription
        bool isBuild = ::Object::IsDerivedFromNullSafe(entity, ComponentDescription::TypeId());
        void* buildEntity = isBuild ? entity : nullptr;

        // Search entity's components for RawMaterialDescription
        uintptr_t e = (uintptr_t)buildEntity;
        int compCount = *(int*)(e + 0x30);
        void** compArray = *(void***)(e + 0x28);

        for (int i = 0; i < compCount; i++) {
            void* comp = compArray[i];
            // Get component type via vtable[2] (+0x10)
            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
            if (typeId == RawMaterialDescription::TypeId()) {
                // Re-read component pointer from array and confirm with IsDerivedFrom
                void* rawComp = compArray[i];
                bool isRawMat = ::Object::IsDerivedFromNullSafe(rawComp, RawMaterialDescription::TypeId());
                if (isRawMat && rawComp != nullptr) {
                    // Destroy the entity (re-read entityList from global each time)
                    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
                    void* entityList = *(void**)(game + 0xF0);
                    // The door was named fn_Game_DestroyEntity; it is bound to
                    // 0x140091e60 RawMaterialComponent_RegisterRawMaterial (B3R: the
                    // binding decides, INVENTORY_E §5.8).
                    RawMaterialComponent_RegisterRawMaterial(entityList, buildEntity);
                }
                break;
            }
        }

        entity = const_cast<::Object::External*>(
            efactoryRaw->FindNext(ComponentDescription::TypeId(),
                                  reinterpret_cast<const ::Object::External*>(entity)));  // B.3.3 bridge
    }
}

// ============================================================
// Game_SpawnItemsInStorage (FUN_140060290)
// Iterates the storage list at gameWorld+0xF0, sub-list at +0x90/+0x98.
// For each storage, spawns up to 5 items if current count < capacity.
// ============================================================
static void Game_SpawnItemsInStorage_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);

    int storageCount = *(int*)(entityListBase + 0x98);
    if (storageCount <= 0) return;

    uintptr_t arrayBase = *(uintptr_t*)(entityListBase + 0x90);
    for (int i = 0; i < storageCount; i++) {
        void* storage = *(void**)(arrayBase + (uintptr_t)i * 8);

        // Check if storage has items to spawn (component+0x28 != 0)
        if (*(uintptr_t*)((uintptr_t)storage + 0x28) == 0) continue;

        for (int j = 0; j < 5; j++) {
            int currentCount = *(int*)((uintptr_t)storage + 0x38);
            // 0x140128570 PastureComponent_ComputeCapacity (door misnamed
            // fn_StorageComponent_GetCapacity; the int the door read is the low dword).
            int capacity = static_cast<int>(PastureComponent_ComputeCapacity(reinterpret_cast<uintptr_t>(storage)));
            if (capacity <= currentCount) break;

            // Spawn one item
            uint8_t retBuf[8] = {0};
            new (retBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            Pasture_SpawnLivestock(storage, retBuf);   // 0x140128670 (door misnamed fn_StorageComponent_SpawnItem)
        }
    }
}

// ============================================================
// Game_SpawnCitizens (FUN_140060380)
// Spawns up to 4 new citizens at buildings that have residence
// components with available capacity. Iterates all buildings,
// spawning multiple citizens per building until full or 4 total.
// ============================================================
static void Game_SpawnCitizens_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    void* randomPtr = *(void**)(game + 0xD8);
    uintptr_t* entityListPtr = *(uintptr_t**)(game + 0xF0);
    int buildingCount = (int)entityListPtr[1]; // building count at entityList+0x08

    if (buildingCount == 0) return;

    int spawnedTotal = 0; // R13D — total citizens spawned across all buildings
    do {
        // Inner loop: scan all buildings
        int buildIdx = 0; // local_res10
        if (buildingCount > 0) {
            uintptr_t buildOffset = 0; // local_b0 — byte offset into building array
            do {
                // Get building from array: *(entityListPtr[0] + buildOffset)
                uintptr_t buildArrayBase = entityListPtr[0];
                void* building = *(void**)(buildOffset + buildArrayBase);

                if (building != nullptr) {

                    // Find ResidenceComponent (DAT_1401c7900) on this building
                    void* residenceComp = nullptr;
                    {
                        uintptr_t b = (uintptr_t)building;
                        int compCount = *(int*)(b + 0x38);
                        uintptr_t compArrayBase = *(uintptr_t*)(b + 0x30);
                        int ci = 0;
                        int64_t cOffset = 0;
                        while (ci < compCount) {
                            void** pComp = (void**)(compArrayBase + cOffset * 8);
                            void* comp = *pComp;
                            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
                            if (typeId == InteractComponent::TypeId()) {
                                // Re-read and CheckPtr on the array slot
                                pComp = (void**)(compArrayBase + cOffset * 8);
                                comp = *pComp;
                                bool isRes = ::Object::IsDerivedFromNullSafe(comp, InteractComponent::TypeId());
                                residenceComp = isRes ? comp : nullptr;
                                goto found_residence;
                            }
                            ci++;
                            cOffset++;
                        }
                        residenceComp = nullptr;
                    }
found_residence:
                    // Store in InternalBasePtr
                    void* resCompIBP = nullptr;
                    new (&resCompIBP) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(residenceComp));   // B.3.3 bridge

                    if (resCompIBP != nullptr) {
                        // Inner while(true) loop: spawn citizens at this building until full
                        int spawnedHere = 0; // R12D — per-building spawn counter
                        while (true) {
                            // Re-check ResidenceComponent each iteration
                            uintptr_t resComp = (uintptr_t)resCompIBP;

                            // Get ResidenceDescription from component+0x18
                            void* desc = *(void**)(resComp + 0x18);
                            bool isResDesc = ::Object::IsDerivedFromNullSafe(desc, InteractDescription::TypeId());
                            void* resDesc = isResDesc ? desc : nullptr;

                            // Read capacity value from desc+0x30 (maxOccupants)
                            // GetOccupantCount returns values that add to this
                            int capacityAccum = *(int*)((uintptr_t)resDesc + 0x30);

                            // Sum occupant counts from children at resComp+0x28/+0x30
                            int childCount = *(int*)(resComp + 0x30);
                            if (childCount > 0) {
                                uintptr_t childArrayBase = *(uintptr_t*)(resComp + 0x28);
                                int si = 0;
                                uintptr_t sOffset = 0;
                                do {
                                    void** pChild = (void**)(childArrayBase + sOffset);
                                    // 0x140123070 Pf_RecursiveChildSum_123070 (door misnamed
                                    // fn_ResidenceComponent_GetOccupantCount)
                                    int occResult = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(*pChild), 0);
                                    capacityAccum += occResult;
                                    si++;
                                    sOffset += 8;
                                } while (si < childCount);
                            }

                            // Break if no more room: spawnedHere >= capacityAccum
                            if (spawnedHere >= capacityAccum) break;

                            // Get spawn point for this slot
                            uint8_t spawnPos[16] = {0};
                            // 0x140122e20 Pf_EntityResolvePos_2e20 (door misnamed
                            // fn_ResidenceComponent_GetSpawnPoint)
                            Pf_EntityResolvePos_2e20(resCompIBP, reinterpret_cast<float*>(spawnPos), 0, spawnedHere, nullptr);

                            // Check if we've reached the global limit of 4
                            if (spawnedTotal >= 4) {
                                // Original doesn't exit; it just skips spawn and
                                // increments spawnedHere until capacity is hit
                                spawnedHere++;
                                continue;
                            }

                            spawnedTotal++;
                            // Save spawnedTotal (R13D saved to stack, reused for gender)
                            int savedSpawnedTotal = spawnedTotal;

                            // Random gender
                            uint32_t gender = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(2));   // B.3.4 bridge
                            uint32_t genderFlag = (gender != 0) ? 1 : 0;

                            // Load citizen template
                            System::Filename filenameBuf("Template/Citizen.rsc");
                            void* factory = ::Object::ExternalFactory::_singleton;
                            void* extTemplate = const_cast<::Object::External*>(
                                reinterpret_cast<::Object::ExternalFactory*>(factory)   // B.3.3 bridge
                                    ->FindOrLoad(filenameBuf));

                            // Determine entity description (RDI in disasm)
                            // If extTemplate is null, use null.
                            // If non-null, IsDerivedFrom check; use result (may be null).
                            void* entityDesc = nullptr;
                            if (extTemplate == nullptr) {
                                entityDesc = nullptr;
                            } else {
                                bool isBuildDesc = ::Object::IsDerivedFromNullSafe(extTemplate, ComponentDescription::TypeId());
                                entityDesc = isBuildDesc ? extTemplate : nullptr;
                            }

                            // Allocate and create entity (0x48 bytes)
                            void* memBlock = System::MemoryAllocator::MemAlloc(0x48, 8);
                            void* newEntity = nullptr;
                            if (memBlock != nullptr) {
                                newEntity = ComponentEntity_CtorFromDescription(
                                    reinterpret_cast<ComponentEntity*>(memBlock), entityDesc);   // 0x1400c3720
                            }

                            // Wrap new entity in InternalBasePtr
                            void* newEntityIBP = nullptr;
                            new (&newEntityIBP) ::Object::InternalBasePtr(
                                reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

                            // Find CitizenAIComponent (DAT_1401c7158) on the new entity
                            void* aiComp = nullptr;
                            {
                                uintptr_t ne = (uintptr_t)newEntityIBP;
                                int compCount = *(int*)(ne + 0x38);
                                uintptr_t compArrayBase = *(uintptr_t*)(ne + 0x30);
                                int ci = 0;
                                int64_t cOffset = 0;
                                while (ci < compCount) {
                                    void** pComp = (void**)(compArrayBase + cOffset * 8);
                                    void* comp = *pComp;
                                    uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
                                    if (typeId == CitizenComponent::TypeId()) {
                                        pComp = (void**)(compArrayBase + cOffset * 8);
                                        comp = *pComp;
                                        bool isAI = ::Object::IsDerivedFromNullSafe(comp, CitizenComponent::TypeId());
                                        aiComp = isAI ? comp : nullptr;
                                        goto found_ai;
                                    }
                                    ci++;
                                    cOffset++;
                                }
                                aiComp = nullptr;
                            }
found_ai:
                            // Wrap CitizenAI in InternalBasePtr
                            void* aiCompIBP = nullptr;
                            new (&aiCompIBP) ::Object::InternalBasePtr(
                                reinterpret_cast<RtObject::Internal*>(aiComp));   // B.3.3 bridge

                            // Setup citizen
                            void* citizenAI = aiCompIBP;

                            // Get CitizenDescription (DAT_1401c7190) from AI component
                            void* citDescRaw = *(void**)((uintptr_t)citizenAI + 0x18);
                            bool isCitDesc = ::Object::IsDerivedFromNullSafe(citDescRaw, CitizenDescription::TypeId());
                            void* citDesc = isCitDesc ? citDescRaw : nullptr;

                            // Get profession list from game (re-read global)
                            game = (uintptr_t)*g_pGameWorldSingleton;
                            void* profList = *(void**)(*(uintptr_t*)(game + 0xB0) + 0x30);

                            // Create null parent InternalBasePtr
                            void* parentIBP = nullptr;
                            new (&parentIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

                            // Read age range from CitizenDescription
                            citizenAI = aiCompIBP;
                            float minAge = *(float*)((uintptr_t)citDesc + 0x50);
                            float randAge = reinterpret_cast<System::Random*>(randomPtr)->RandFloat();   // B.3.4 bridge
                            float maxAge = *(float*)((uintptr_t)citDesc + 0x54);
                            float age = randAge * (maxAge - minAge) + minAge;

                            CitizenComp_DeferredInit_117080(reinterpret_cast<int64_t>(citizenAI), spawnPos,
                                                      genderFlag, '\0',
                                                      age, '\x01', reinterpret_cast<int64_t*>(&parentIBP),
                                                      reinterpret_cast<int64_t>(profList));   // 0x140117080

                            // Register entity with entity manager
                            // Use newEntityIBP (the InternalBasePtr value) as the entity ref
                            void* entityRef = newEntityIBP;
                            game = (uintptr_t)*g_pGameWorldSingleton;
                            void* entityMgr = *(void**)(game + 0xE8);
                            EntityList_RegisterEntity_Impl(entityMgr, &entityRef,
                                                         *(int*)((uintptr_t)entityDesc + 0x20));   // 0x1400c48a0

                            // Restore spawnedTotal
                            spawnedTotal = savedSpawnedTotal;

                            spawnedHere++;
                        } // while(true) — per-building spawn loop
                    }
                }

                // Advance to next building
                buildIdx++;
                buildOffset += 8;
                // Re-read building count (may change if entities are added)
                buildingCount = (int)entityListPtr[1];
            } while (buildIdx < buildingCount);
        }
    } while (spawnedTotal < 4);
}

// ============================================================
// DebugUI_TriggerDisasterOnBuilding (FUN_14005FF30)
// Picks a random building from the building list, reads disease
// combo selection, and applies disease disaster to the building.
// ============================================================
static void DebugUI_TriggerDisasterOnBuilding_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);

    // Pick random building from the building list
    uint32_t buildingCount = *(uint32_t*)(entityListBase + 0xA8);
    if (buildingCount == 0) return;
    void* randomPtr = *(void**)(game + 0xD8);
    uint32_t randIdx = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(buildingCount));   // B.3.4 bridge

    // Get disease description from combo selection
    void* combo = *(void**)(s + 0x30);
    if (!combo) return;
    int diseaseIdx = *(int*)((uintptr_t)combo + 0x118);
    void* externalList = *(void**)((uintptr_t)game + 0xA8);
    if (!externalList) return;
    void* diseaseEntry = *(void**)(*(uintptr_t*)((uintptr_t)externalList + 0x20) +
                                   (int64_t)diseaseIdx * 8);
    bool isDisease = ::Object::IsDerivedFromNullSafe(diseaseEntry, Disease::TypeId());
    void* diseaseDesc = isDisease ? diseaseEntry : nullptr;
    if (!diseaseDesc) return;

    // Get building entity from building array
    void* building = *(void**)(*(uintptr_t*)(entityListBase + 0xA0) + (int64_t)(int)randIdx * 8);
    if (!building) return;

    // Apply disease to building
    CitizenComp_Infect(building, diseaseDesc);   // 0x140116b90 (door named fn_DisasterComponent_Infect)
}

// ============================================================
// Game_StartRandomFire (FUN_14005FEB0)
// Picks a random building, finds its FireComponent, ignites it.
// ============================================================
static void Game_StartRandomFire_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);
    uint32_t buildingCount = *(uint32_t*)(entityListBase + 0x28);

    if (buildingCount == 0) return;

    void* randomPtr = *(void**)(game + 0xD8);
    uint32_t randIdx = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(buildingCount));   // B.3.4 bridge

    // Get random building entity
    void* building = *(void**)(*(uintptr_t*)(entityListBase + 0x20) + (uintptr_t)randIdx * 8);

    // Find FireComponent on the building
    uint8_t fireCompBuf[8] = {0};
    fn_FindComponent_Fire(building, fireCompBuf);

    // Ignite
    void* fireComp = *(void**)fireCompBuf;
    FireComponent_Ignite_610(fireComp);   // 0x140049610
}

// ============================================================
// Game_SpawnTornado (FUN_14005FD20)
// Loads Template/Tornado.rsc, creates entity, registers it.
// ============================================================
static void Game_SpawnTornado_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // Load tornado template
    System::Filename filenameBuf("Template/Tornado.rsc");
    void* factory = ::Object::ExternalFactory::_singleton;
    void* extTemplate = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(factory)   // B.3.3 bridge
            ->FindOrLoad(filenameBuf));

    // Cast to entity description (must be derived from ComponentDescription type)
    void* entityDesc = nullptr;
    if (extTemplate) {
        bool isEntityDesc = ::Object::IsDerivedFromNullSafe(extTemplate, ComponentDescription::TypeId());
        if (isEntityDesc) {
            entityDesc = extTemplate;
        }
    }

    // filenameBuf releases via its destructor (System::Filename) at scope exit.

    // Allocate and create entity (0x48 bytes)
    void* newEntity = nullptr;
    void* memBlock = System::MemoryAllocator::MemAlloc(0x48, 8);
    if (memBlock) {
        newEntity = ComponentEntity_CtorFromDescription(
            reinterpret_cast<ComponentEntity*>(memBlock), entityDesc);   // 0x1400c3720
    }

    // Register entity with entity manager
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge
    void* entityRef = *(void**)entityBuf;
    void* entityMgr = *(void**)(game + 0xE8);
    EntityList_RegisterEntity_Impl(entityMgr, &entityRef,
                                  *(int*)((uintptr_t)entityDesc + 0x20));   // 0x1400c48a0
}

// ============================================================
// Game_InfectRandomCitizen (FUN_14005FE10)
// Picks a random citizen from the citizen list, infects them.
// ============================================================
static void Game_InfectRandomCitizen_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);

    uint32_t citizenCount = *(uint32_t*)(entityListBase + 0xF8);
    if (citizenCount == 0) return;

    void* randomPtr = *(void**)(game + 0xD8);
    uint32_t randIdx = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(citizenCount));   // B.3.4 bridge

    // Get random citizen
    uintptr_t citizenArray = *(uintptr_t*)(entityListBase + 0xF0);
    if (!citizenArray) return;
    void* citizen = *(void**)(citizenArray + (uintptr_t)randIdx * 8);
    if (!citizen) return;

    // Infect citizen's disease component
    DiseaseComp_Infect(reinterpret_cast<DiseaseComponent*>(citizen));   // 0x1400483a0
}

// ENDGAME B.3.9: every global the old debugui_sa::EnsureGlobals() healed now has a
// ported TypeId(): ComponentDescription (cell 0x1401c7318), RawMaterialDescription
// (0x1401c7cb8), InteractComponent (0x1401c7900), InteractDescription (0x1401c7938),
// CitizenDescription (0x1401c7190), Disease (was TypeId_DiseaseDescription, cell
// 0x1401c5948 -- Table 1's authority, not the name), PopulationDescription (was
// TypeId_DrawableType, cell 0x1401c47c8 -- Table 1's authority), CitizenComponent
// (0x1401c7158), and UI::Element (was TypeId_Element, healed here under the WRONG
// by-name guess "Internal" -- the cell is exref 0x140151660, UI::Element's own
// _typeInfo per Table 1 / standalone/reg_element_typeids.cpp, matching this file's
// OWN correct comment on the FindElemBase guard further down). The lazy-cache
// namespace is dead.

// ============================================================
// DebugUI::Slot8 (vtable +0x40) -- Update performance counters
// ============================================================
// Reads 4 debug counters from engine state, updates NumberElements.
// Smooths FPS/frame timing with exponential moving average (alpha=0.95).
// Displays smoothed FPS, frame ms, and total ms (* 1000).
//
// Engine state: singleton exref -> +0x10 sub-object holds counters:
//   +0x2150, +0x2154, +0x2158, +0x215C (int debug stats)
// Timing: singleton -> +0x58 (total time ticks), +0x68 (frame time ticks)
//
// Struct fields used:
//   +0x40 = numberDrawCalls NumberElement (counter from +0x215C)
//   +0x48 = numberTriangles NumberElement (counter from +0x2150)
//   +0x50 = numberVertices NumberElement (counter from +0x2154)
//   +0x58 = numberBatches NumberElement (counter from +0x2158)
//   +0x60 = numberFPS display (NumberElement, int, smoothed)
//   +0x68 = numberFrameMS display (NumberElement, float, smoothed)
//   +0x70 = numberTotalMS display (NumberElement, float, smoothed)
//   +0x120 = smoothed FPS accumulator (float)
//   +0x124 = smoothed frame time accumulator (float)
//   +0x128 = smoothed total time accumulator (float)
//
// Original: 0x1400610C0
// [8] 0x1400610c0
void DebugUI::OnUpdate(const System::Time& /*time*/) {
    // perf counters (self-guarded), then keep swatches/day live (the tree's
    // per-frame drive of slot 7, a standalone deviation).
    [this] {
        uintptr_t s = (uintptr_t)this;

        // MainLoop singleton access: slot → *slot → *(result + 0x10)
        // g_pMainLoopSingleton = &Engine::MainLoop::_singleton (retail: *exref 0x140151440)
        // So: *g_pMainLoopSingleton = the MainLoop, then +0x10 = its video context
        // Disasm: MOV RAX,[slot] / MOV RDX,[RAX] / MOV RSI,[RDX+0x10]
        uintptr_t engineObj = (uintptr_t)::Engine::MainLoop::_singleton;
        if (!engineObj) return;
        uintptr_t engineState = *(uintptr_t*)(engineObj + 0x10);
        if (!engineState) return;

        // Read and display 4 debug counters from engine state
        void* numBatches = *(void**)(s + 0x58);
        if (numBatches) reinterpret_cast<UI::NumberElement*>(numBatches)->SetValue(static_cast<int32_t>(*(int*)(engineState + 0x2158)));  // B.3.6 bridge

        void* numDrawCalls = *(void**)(s + 0x40);
        if (numDrawCalls) reinterpret_cast<UI::NumberElement*>(numDrawCalls)->SetValue(static_cast<int32_t>(*(int*)(engineState + 0x215C)));  // B.3.6 bridge

        void* numTriangles = *(void**)(s + 0x48);
        if (numTriangles) reinterpret_cast<UI::NumberElement*>(numTriangles)->SetValue(static_cast<int32_t>(*(int*)(engineState + 0x2150)));  // B.3.6 bridge

        void* numVertices = *(void**)(s + 0x50);
        if (numVertices) reinterpret_cast<UI::NumberElement*>(numVertices)->SetValue(static_cast<int32_t>(*(int*)(engineState + 0x2154)));  // B.3.6 bridge

        // Read frame time and total time from engine object (NOT engineState)
        // Original: *(*(exref 0x140151440) + 0x68) and + 0x58
        int64_t frameTimeTicks = *(int64_t*)(engineObj + 0x68);
        int64_t totalTimeTicks = *(int64_t*)(engineObj + 0x58);
        float frameSeconds = reinterpret_cast<const System::Time*>(&frameTimeTicks)->AsSeconds();   // B.3.4 bridge
        float totalSeconds = reinterpret_cast<const System::Time*>(&totalTimeTicks)->AsSeconds();   // B.3.4 bridge

        // Exponential moving average smoothing constants
        const float alpha = 0.95f;
        const float beta  = 0.05f;

        float* smoothedFPS     = (float*)(s + 0x120);
        float* smoothedFrameMS = (float*)(s + 0x124);
        float* smoothedTotalMS = (float*)(s + 0x128);

        float instantFPS = (totalSeconds > 0.0f) ? (1.0f / totalSeconds) : 0.0f;
        *smoothedTotalMS = (*smoothedTotalMS) * alpha + totalSeconds * beta;
        *smoothedFPS     = instantFPS * beta + (*smoothedFPS) * alpha;
        *smoothedFrameMS = (*smoothedFrameMS) * alpha + frameSeconds * beta;

        // Display smoothed values
        void* numFPS = *(void**)(s + 0x60);
        if (numFPS) reinterpret_cast<UI::NumberElement*>(numFPS)->SetValue(static_cast<int32_t>((int)*smoothedFPS));  // B.3.6 bridge

        void* numFrameMS = *(void**)(s + 0x68);
        if (numFrameMS) reinterpret_cast<UI::NumberElement*>(numFrameMS)->SetValue(static_cast<float>(*smoothedFrameMS * 1000.0f));  // B.3.6 bridge

        void* numTotalMS = *(void**)(s + 0x70);
        if (numTotalMS) reinterpret_cast<UI::NumberElement*>(numTotalMS)->SetValue(static_cast<float>(*smoothedTotalMS * 1000.0f));  // B.3.6 bridge
    }();
    DebugUI::OnUpdateLayout();
}

// ============================================================
// DebugUI::Slot7 (vtable +0x38) -- Update day text and color swatches
// ============================================================
// 1. Gets first child of sliderDay (+0x78), formats day text
// 2. Reads RGB slider values for sun/amb/fog, packs ARGB, sets color
//    on preview ImageElements via vtable[10] (SetColor at +0x50)
//
// Struct fields used:
//   +0x78 = sliderDay element (slider for day-of-year)
//   +0x80/+0x88/+0x90 = valueSunR/G/B slider elements
//   +0x98/+0xA0/+0xA8 = valueAmbR/G/B slider elements
//   +0xB0/+0xB8/+0xC0 = valueFogR/G/B slider elements
//   +0xF0 = imageColorSun preview element
//   +0xF8 = imageColorAmb preview element
//   +0x100 = imageColorFog preview element
//
// Original: 0x140061280
// [7] 0x140061280
void DebugUI::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    // --- Day slider text update ---
    // Original: CheckPtr(+0x78), get child, CheckPtr child, IsDerivedFrom, IBP ctor,
    //           CheckPtr(+0x78) again for value, CheckPtr child for format, SetText
    void* sliderDay = *(void**)(s + 0x78);
    if (sliderDay) {
        uint8_t childBuf[16];
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(sliderDay)->Child(0));  // B.3.6 bridge
        void* childRaw = *(void**)childBuf;
        // IsDerivedFrom check + IBP wrap (matches original)
        bool isText = childRaw && ::Object::IsDerivedFromNullSafe(childRaw, UI::Element::TypeId());
        void* textChild = isText ? childRaw : nullptr;
        uint8_t textIBP[8];
        new (textIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(textChild));   // B.3.3 bridge

        float dayValue = *(float*)(*(uintptr_t*)(s + 0x78) + 0x128);

        void* textElem = *(void**)textIBP;
        if (textElem) {
            uint8_t uiStrBuf[16];
            uint64_t fmtRef = *(uint64_t*)((uintptr_t)textElem + 0xD8);
            *(uint64_t*)&uiStrBuf[8] = *(uint64_t*)((uintptr_t)textElem + 0xE0);
            ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(fmtRef)), (int)dayValue / 30);  // B.3.6 bridge
            reinterpret_cast<UI::TextElement*>(*(void**)textIBP)->SetText(reinterpret_cast<const UI::String*>(uiStrBuf));  // B.3.6 bridge
        }
    }

    // Helper: read slider value with CheckPtr (matches original pattern)
    auto readSlider = [](uintptr_t s, uintptr_t offset) -> float {
        return *(float*)(*(uintptr_t*)(s + offset) + 0x128);
    };

    // --- Sun color swatch ---
    void* imageColorSun = *(void**)(s + 0xF0);
    if (imageColorSun) {
        float sunB = readSlider(s, 0x90);
        float sunG = readSlider(s, 0x88);
        uint32_t sunColor = ((int)*(float*)(*(uintptr_t*)(s + 0x80) + 0x128) << 8
            | (int)sunG) << 8 | (int)sunB | 0xFF000000;
        reinterpret_cast<UI::Element*>(imageColorSun)->SetColor(sunColor);  // B.3.6 bridge
    }

    // --- Ambient color swatch ---
    void* imageColorAmb = *(void**)(s + 0xF8);
    if (imageColorAmb) {
        float ambB = readSlider(s, 0xA8);
        float ambG = readSlider(s, 0xA0);
        uint32_t ambColor = ((int)*(float*)(*(uintptr_t*)(s + 0x98) + 0x128) << 8
            | (int)ambG) << 8 | (int)ambB | 0xFF000000;
        reinterpret_cast<UI::Element*>(imageColorAmb)->SetColor(ambColor);  // B.3.6 bridge
    }

    // --- Fog color swatch ---
    void* imageColorFog = *(void**)(s + 0x100);
    if (imageColorFog) {
        float fogB = readSlider(s, 0xC0);
        float fogG = readSlider(s, 0xB8);
        uint32_t fogColor = ((int)*(float*)(*(uintptr_t*)(s + 0xB0) + 0x128) << 8
            | (int)fogG) << 8 | (int)fogB | 0xFF000000;
        reinterpret_cast<UI::Element*>(imageColorFog)->SetColor(fogColor);  // B.3.6 bridge
    }
}

// ============================================================
// DebugUI::Slot9 (vtable +0x48) -- Event dispatch
// ============================================================
// Handles button clicks, combo changes, and slider changes for
// the debug panel. Dispatches to sub-functions for resource spawning,
// disasters, citizen spawning, debug render toggles, weather/lighting.
//
// Event types (param3):
//   0x12 = button click → IDs 300-339 (resource buttons, disaster buttons, debug toggles)
//   0x14 = combo change → ID 0x163 (drawCombo), ID 0x166 (diseaseCombo)
//   0x17 = slider change → ID 0x154 (sliderDay), IDs 0x155-0x162 (color sliders)
//
// Struct fields read by sub-functions:
//   +0x30 = diseaseCombo (used by OnDiseaseComboChanged)
//   +0x38 = drawCombo (unused here, handled by OnDrawComboChanged)
//   +0x78 = sliderDay (used by OnDaySliderChanged)
//   +0x80..+0xE8 = color sliders (used by WriteLightingColors)
//   +0x108 = drawCombo (used by OnDrawComboChanged, offset in combo handler)
//
// Original: 0x140060BC0
void DebugUI_Slot9(DebugUI* self, uint64_t param2, int param3, int param4) {
    void* game = *g_pGameWorldSingleton;

    // ---- Combo change (0x14) ----
    if (param3 == 0x14) {
        if (param4 == 0x166) {
            // Disease combo changed
            DebugUI_OnDiseaseComboChanged_Impl(self);
        } else if (param4 == 0x163) {
            // Draw combo changed (select visible scene/mesh)
            DebugUI_OnDrawComboChanged_Impl(self);
        }
        return;
    }

    // ---- Slider/number change (0x17) ----
    if (param3 == 0x17) {
        if (param4 == 0x154) {
            // Day slider changed → update game time and lighting
            DebugUI_OnDaySliderChanged_Impl(self);
        } else if ((unsigned)(param4 - 0x155) <= 0xD) {
            // Color sliders (0x155-0x162) changed → write lighting colors
            void* weatherMgr = *(void**)((uintptr_t)game + 0x128);
            int month = DebugUI_GetMonthFromTime_Impl(weatherMgr);
            DebugUI_WriteLightingColors_Impl(self, month);
        }
        return;
    }

    // ---- Button click (0x12) ----
    if (param3 != 0x12)
        return;

    // Range check: element IDs 300 (0x12C) through 339 (0x153)
    if ((unsigned)(param4 - 300) > 0x39)
        return;

    // Get citizen list from game world (+0xF0)
    void** citizenList = *(void***)((uintptr_t)game + 0xF0);

    switch (param4) {
    // --- Resource spawn buttons ---
    // Each calls Game_SpawnResources(citizenList, resourceFlag, count)
    case 300:  Game_SpawnResources_Impl(citizenList, 0x400,     50);   return;  // Logs
    case 301:  Game_SpawnResources_Impl(citizenList, 0x800,     50);   return;  // Food
    case 302:  Game_SpawnResources_Impl(citizenList, 0x1000,    50);   return;  // Herbs
    case 303:  Game_SpawnResources_Impl(citizenList, 0x80,      50);   return;  // Stone
    case 304:  Game_SpawnResources_Impl(citizenList, 0x100,     50);   return;  // Iron
    case 305:  Game_SpawnResources_Impl(citizenList, 0x200,     50);   return;  // Firewood
    case 306:  Game_SpawnResources_Impl(citizenList, 0x20,      1000); return;  // Tool
    case 307:  Game_SpawnResources_Impl(citizenList, 0x2000,    25);   return;  // Coat
    case 308:  Game_SpawnResources_Impl(citizenList, 0x4000,    25);   return;  // Leather coat
    case 309:  Game_SpawnResources_Impl(citizenList, 0x10000,   50);   return;  // Herbs2
    case 310:  Game_SpawnResources_Impl(citizenList, 0x20000,   100);  return;  // Ale
    case 311:  Game_SpawnResources_Impl(citizenList, 0x40000,   100);  return;  // Resource type 18
    case 312:  Game_SpawnResources_Impl(citizenList, 0x80000,   100);  return;  // Resource type 19
    case 313:  Game_SpawnResources_Impl(citizenList, 0x100000,  100);  return;  // Resource type 20
    case 314:  Game_SpawnResources_Impl(citizenList, 0x200000,  100);  return;  // Resource type 21
    case 315:  Game_SpawnResources_Impl(citizenList, 0x400000,  100);  return;  // Resource type 22
    case 316:  Game_SpawnResources_Impl(citizenList, 0x800000,  100);  return;  // Resource type 23
    case 317:  Game_SpawnResources_Impl(citizenList, 0x1000000, 100);  return;  // Resource type 24
    case 318:  Game_SpawnResources_Impl(citizenList, 0x2000000, 100);  return;  // Resource type 25
    case 319:  Game_SpawnResources_Impl(citizenList, 0x4000000, 100);  return;  // Resource type 26
    case 320:  Game_SpawnResources_Impl(citizenList, 0x8000000, 100);  return;  // Resource type 27

    // --- Disaster / entity manipulation buttons ---
    case 321:  Game_TriggerDisasterByType_Impl(0); return;  // Trigger disaster type 0
    case 322:  Game_TriggerDisasterByType_Impl(1); return;  // Trigger disaster type 1
    case 323:  Game_TriggerDisasterByType_Impl(2); return;  // Trigger disaster type 2
    case 324:  Game_DestroyAllRawMaterials_Impl();  return;  // Destroy all loose raw material entities
    case 325:  Game_SpawnItemsInStorage_Impl();    return;  // Spawn items in all storage
    case 326:  Game_SpawnCitizens_Impl();          return;  // Spawn new citizens
    case 327:  DebugUI_TriggerDisasterOnBuilding_Impl(self); return;  // Disaster on random building
    case 328:  Game_StartRandomFire_Impl();        return;  // Start random fire
    case 329:  Game_SpawnTornado_Impl();           return;  // Spawn tornado
    case 330:  Game_InfectRandomCitizen_Impl();    return;  // Infect random citizen

    // --- Debug render flag toggles (checkboxes) ---
    // param2 is a pointer to an InternalBasePtr (the checkbox element IBP).
    // Original copies *param2 (deref) to a local IBP, passes &local to ToggleDebugFlag.
    // ToggleDebugFlag calls CheckPtr on the IBP, then dereferences to get element.
    case 331: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x1);   return; }
    case 332: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x2);   return; }
    case 333: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x4);   return; }
    case 334: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x8);   return; }
    case 335: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x10);  return; }
    case 336: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x20);  return; }
    case 337: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x40);  return; }
    case 338: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x80);  return; }
    case 339: { uint64_t ibp = *(uint64_t*)(uintptr_t)param2; DebugUI_ToggleDebugFlag(&ibp, 0x200); return; }

    default: return;
    }
}

// ============================================================
// DebugUI_ToggleDebugFlag -- helper for debug render checkboxes
// ============================================================
// param_2 is an InternalBasePtr* pointing to the checkbox element.
// Must CheckPtr then deref through IBP to reach actual element.
// Reads checkbox checked state (element+0xD8), sets or clears a bit in
// the game world's debug flags at game+0xD0.
//
// Original: 0x14005FCC0
void DebugUI_ToggleDebugFlag(void* ibpPtr, uint32_t flag) {
    void* game = *g_pGameWorldSingleton;
    uint32_t* debugFlags = (uint32_t*)((uintptr_t)game + 0xD0);

    // CheckPtr resolves the InternalBasePtr (lazy/weak pointer resolution)

    // Deref through IBP to get actual checkbox element, then read +0xD8
    void* element = *(void**)ibpPtr;
    bool checked = *(uint8_t*)((uintptr_t)element + 0xD8) != 0;

    if (checked) {
        *debugFlags |= flag;
    } else {
        *debugFlags &= ~flag;
    }
}

// ============================================================
// DebugUI::Slot4 (vtable +0x20) -- Setup / element binding
// ============================================================
// Finds 50+ named UI elements in the dialog, assigns element IDs for
// event dispatch, stores persistent elements in DebugUI struct fields,
// populates disease/draw/weather combo boxes, sets slider min/max
// ranges, loads initial day-of-year from game time, and finds all
// performance counter NumberElements.
//
// Original: 0x1400619E0

// UI::StringTable::GetText(table, retStr, nameStr) — the exe .rdata dispatch slot
// 0x140151718 retail's diseaseCombo populate calls (export fallback for the case
// where the slot is unregistered in standalone). Non-trivial return: RDX is the
// hidden System::String out-param, R8 the name to look up.
// ENDGAME B.3.6 (K2): the slot/export cache went with this, its last caller; the
// body is the door's, moved here -- one construction into the caller's storage.
static void DebugUI_StringTableGetText(void* table, void* retStr, const void* nameStr) {
    ::new (retStr) System::String(reinterpret_cast<const UI::StringTable*>(table)  // B.3.6 bridge
        ->GetText(*static_cast<const System::String*>(nameStr)));
}

// Helper: Find element by name, return raw pointer.
static void* DebugUI_FindElem(void* parentElem, const char* name) {
    System::String nameBuf(name);
    uint8_t retBuf[16] = {};  // InternalPtr is 16 bytes (hidden return)
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameBuf));  // B.3.6 bridge
    return *(void**)retBuf;
}

// Helper: Find element by name, wrap in InternalBasePtr via IsDerivedFrom.
static void* DebugUI_FindElemBase(void* parentElem, const char* name) {
    void* rawElem = DebugUI_FindElem(parentElem, name);
#ifdef _DEBUG
    if (!rawElem) printf("[DebugUI::Slot4] FindElemBase('%s') = NULL\n", name);
    else printf("[DebugUI::Slot4] FindElemBase('%s') = %p\n", name, rawElem);
#endif
    // Original uses UI::Element's own id (exe slot 0x140151660), NOT
    // TypeId_ButtonElement which excludes sliders/images/numbers.
    if (!rawElem || !::Object::IsDerivedFromNullSafe(rawElem, UI::Element::TypeId())) {
        rawElem = nullptr;
    }
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    return *(void**)basePtrBuf;
}

// Helper: Find element, set element ID, return as InternalBasePtr.
// Returns nullptr if element not found (e.g., Debug Menu mod missing elements).
static void* DebugUI_FindAndSetId(void* parentElem, const char* name, int id) {
    void* elem = DebugUI_FindElemBase(parentElem, name);
    if (elem) reinterpret_cast<UI::Element*>(elem)->SetElementId(id);  // B.3.6 bridge
    return elem;
}

// [4] 0x1400619e0
void DebugUI::OnCreated() {
    // Slot4 unconditionally dereferences ObjectInternal (every FindElemBase),
    // DiseaseDescription (diseaseCombo populate) and DrawableType (drawCombo
    // populate). Skip the whole setup rather than fault if any failed to
    // resolve; all three reliably resolve post-Engine::Startup.
    if (!UI::Element::TypeId() || !Disease::TypeId() || !PopulationDescription::TypeId()) {
        return;
    }
    uintptr_t s = (uintptr_t)this;
    void* parentElem = *(void**)(s + 0x20);

    // ==========================================================
    // 1. Resource spawn buttons (IDs 300..0x136)
    // ==========================================================
    DebugUI_FindAndSetId(parentElem, "buttonWood",     300);   // 0x12C
    DebugUI_FindAndSetId(parentElem, "buttonStone",    0x12D);
    DebugUI_FindAndSetId(parentElem, "buttonIron",     0x12E);
    DebugUI_FindAndSetId(parentElem, "buttonFuel",     0x12F);
    DebugUI_FindAndSetId(parentElem, "buttonCoal",     0x130);
    DebugUI_FindAndSetId(parentElem, "buttonTool",     0x131);
    DebugUI_FindAndSetId(parentElem, "buttonFood",     0x132);
    DebugUI_FindAndSetId(parentElem, "buttonHealth",   0x133);
    DebugUI_FindAndSetId(parentElem, "buttonClothes",  0x134);
    DebugUI_FindAndSetId(parentElem, "buttonTextile",  0x135);
    DebugUI_FindAndSetId(parentElem, "buttonAlcohol",  0x136);

    // buttonCustom0..buttonCustom9 (IDs 0x26E..0x277)
    {
        int id = 0x26E;
        for (int i = 0; i < 10; i++, id++) {
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), "buttonCustom%i", i);
            DebugUI_FindAndSetId(parentElem, nameBuf, id);
        }
    }

    // ==========================================================
    // 2. Spawn action buttons (IDs 0x141..0x146)
    // ==========================================================
    DebugUI_FindAndSetId(parentElem, "buttonCrop",      0x141);
    DebugUI_FindAndSetId(parentElem, "buttonOrchard",   0x142);
    DebugUI_FindAndSetId(parentElem, "buttonAnimals",   0x143);
    DebugUI_FindAndSetId(parentElem, "buttonVisible",   0x144);
    DebugUI_FindAndSetId(parentElem, "buttonLivestock", 0x145);
    DebugUI_FindAndSetId(parentElem, "buttonCitizen",   0x146);

    // ==========================================================
    // 3. Disease combo + disease button (ID 0x147)
    // ==========================================================
    {
        void* combo = DebugUI_FindElemBase(parentElem, "diseaseCombo");
        this->diseaseCombo = combo;

        if (combo) {
            // Populate from ExternalList (resource descriptions at gameWorld+0xA8).
            // STANDALONE: this list can be null/unpopulated, and
            // GetResourceCount(null) faults reading null+0x28. Guard so a missing
            // list yields an empty combo instead of crashing.
            void* gameWorld_dc = *g_pGameWorldSingleton;
            void* externalList = gameWorld_dc ? *(void**)((uintptr_t)gameWorld_dc + 0xa8) : nullptr;
            int resourceCount = externalList
                                    ? reinterpret_cast<const ::Object::ExternalList*>(
                                          externalList)->GetResourceCount()  // B.3.3 bridge
                                    : 0;
            for (int i = 0; i < resourceCount; i++) {
                void* resource = *(void**)((uintptr_t)i * 8 +
                    *(uintptr_t*)((uintptr_t)externalList + 0x20));
                // Retail's checked cast. Every entry of the Diseases.rsc ExternalList
                // IS a Disease, so this never narrows to null in practice — retail
                // relies on that and derefs the result unguarded two lines later
                // (0x140062560..0x140062578). Kept faithful: one combo row per list
                // entry, so the row index stays a direct index into the list, which is
                // exactly how DebugUI::TriggerDisasterOnBuilding (0x14005ff30) reads
                // the selection back.
                bool isDisease = ::Object::IsDerivedFromNullSafe(resource, Disease::TypeId());
                void* disease = isDisease ? resource : nullptr;
                if (!disease) continue;

                // Row text = StringTable::GetText(disease->stringTable(+0x20),
                //                                 &out, disease->stringName(+0x28))
                // — retail's 0x14006257c call, whose third argument Ghidra hides.
                System::String textBuf;
                DebugUI_StringTableGetText(*(void**)((uintptr_t)disease + 0x20),
                                           &textBuf,
                                           (void*)((uintptr_t)disease + 0x28));
                uint8_t spriteBuf[16];
                ::new (spriteBuf) UI::Sprite();
                reinterpret_cast<UI::ComboElement*>(this->diseaseCombo)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), textBuf, nullptr);  // B.3.6 bridge
            }
            reinterpret_cast<UI::ComboElement*>(this->diseaseCombo)->SetCurrentItem(0);  // B.3.6 bridge
        }
    }
    DebugUI_FindAndSetId(parentElem, "diseaseButton", 0x147);

    // ==========================================================
    // 4. Disaster buttons (IDs 0x148..0x14A)
    // ==========================================================
    DebugUI_FindAndSetId(parentElem, "fireButton",        0x148);
    DebugUI_FindAndSetId(parentElem, "tornadoButton",     0x149);
    DebugUI_FindAndSetId(parentElem, "cropDiseaseButton", 0x14A);

    // ==========================================================
    // 5. Cheat checkboxes (IDs 0x14B..0x153) with initial state
    // ==========================================================
    void* gameWorld = *g_pGameWorldSingleton;
    uint32_t cheatFlags = *(uint32_t*)((uintptr_t)gameWorld + 0xD0);

    void* elem;
    elem = DebugUI_FindAndSetId(parentElem, "zeroCostCheck",      0x14B);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = cheatFlags & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noHungerCheck",      0x14C);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 1) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noColdCheck",        0x14D);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 2) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noHealthCheck",      0x14E);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 3) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noSickCheck",        0x14F);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 4) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noCropDiseaseCheck", 0x150);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 5) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noFireCheck",        0x151);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 6) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noTornadoCheck",     0x152);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 7) & 1;

    // educatedCheck -- bit 9 (bit 8 is skipped)
    elem = DebugUI_FindAndSetId(parentElem, "educatedCheck",      0x153);
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 9) & 1;

    // ==========================================================
    // 6. Draw combo (ID 0x166) with drawable type entries
    // ==========================================================
    {
        void* combo = DebugUI_FindElemBase(parentElem, "drawCombo");
        this->drawCombo = combo;

        if (combo) {
            reinterpret_cast<UI::Element*>(this->drawCombo)->SetElementId(0x166);  // B.3.6 bridge

            // Populate from drawable type list at gameWorld+0xF0.
            // STANDALONE: guard a null list (empty combo instead of a fault).
            uintptr_t drawableList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
            int drawCount = drawableList ? *(int*)(drawableList + 200) : 0; // +0xC8
            int selectedIndex = 0;
            for (int i = 0; i < drawCount; i++) {
                void* entry = *(void**)((uintptr_t)i * 8 +
                    *(uintptr_t*)(drawableList + 0xC0));
                void* inner = *(void**)((uintptr_t)entry + 0x18);
                bool isDrawable = ::Object::IsDerivedFromNullSafe(inner, PopulationDescription::TypeId());
                void* drawableDesc = isDrawable ? inner : nullptr;

                uint8_t spriteBuf[16];
                ::new (spriteBuf) UI::Sprite();
                reinterpret_cast<UI::ComboElement*>(this->drawCombo)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), *reinterpret_cast<const System::String*>((void*)((uintptr_t)drawableDesc + 0x20)), entry);  // B.3.6 bridge

                // Check if current entry matches the null/default internal ptr
                uint8_t nullBuf[8];
                new (nullBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                if (entry == *(void**)nullBuf) {
                    selectedIndex = i + 4;
                }
            }
            reinterpret_cast<UI::ComboElement*>(this->drawCombo)->SetCurrentItem(selectedIndex);  // B.3.6 bridge
        }
    }

    // ==========================================================
    // 7. Lighting sliders -- find, store, set IDs
    // ==========================================================
    // Original finds all slider elements first, stores IBPs, sets IDs.
    // Min/max are set later (after image elements) matching original order.

    #define FIND_SLIDER(field, name, id) do { \
        this->field = DebugUI_FindElemBase(parentElem, name); \
        reinterpret_cast<UI::Element*>(*(void**)(s + offsetof(DebugUI, field)))->SetElementId(id);  /* B.3.6 bridge */ \
    } while(0)

    FIND_SLIDER(sliderDay, "sliderDay", 0x154);
    FIND_SLIDER(valueSunR, "valueSunR", 0x155);
    FIND_SLIDER(valueSunG, "valueSunG", 0x156);
    FIND_SLIDER(valueSunB, "valueSunB", 0x157);
    FIND_SLIDER(valueSunH, "valueSunH", 0x158);
    FIND_SLIDER(valueSunP, "valueSunP", 0x159);
    FIND_SLIDER(valueAmbR, "valueAmbR", 0x15A);
    FIND_SLIDER(valueAmbG, "valueAmbG", 0x15B);
    FIND_SLIDER(valueAmbB, "valueAmbB", 0x15C);
    FIND_SLIDER(valueFogR, "valueFogR", 0x15D);
    FIND_SLIDER(valueFogG, "valueFogG", 0x15E);
    FIND_SLIDER(valueFogB, "valueFogB", 0x15F);
    FIND_SLIDER(valueFogN, "valueFogN", 0x160);
    FIND_SLIDER(valueFogF, "valueFogF", 0x161);
    FIND_SLIDER(valueFogM, "valueFogM", 0x162);

    #undef FIND_SLIDER

    // ==========================================================
    // 8. Color preview images (no IDs, just stored)
    // ==========================================================
    this->imageColorSun = DebugUI_FindElemBase(parentElem, "imageColorSun");
    this->imageColorAmb = DebugUI_FindElemBase(parentElem, "imageColorAmb");
    this->imageColorFog = DebugUI_FindElemBase(parentElem, "imageColorFog");

    // ==========================================================
    // 8b. Slider min/max — set AFTER image elements (matches original order)
    // ==========================================================
    // Original uses CheckPtr + deref for every write. +0x118 = min, +0x11C = max.
    // Helper: CheckPtr on IBP field, then write float to resolved element.
    #define SET_MINMAX(field, minVal, maxVal) do { \
        *(float*)(*(uintptr_t*)(s + offsetof(DebugUI, field)) + 0x118) = minVal; \
        *(float*)(*(uintptr_t*)(s + offsetof(DebugUI, field)) + 0x11C) = maxVal; \
    } while(0)

    SET_MINMAX(sliderDay, 0.0f, 359.0f);
    SET_MINMAX(valueSunR, 0.0f, 255.0f);
    SET_MINMAX(valueSunG, 0.0f, 255.0f);
    SET_MINMAX(valueSunB, 0.0f, 255.0f);
    SET_MINMAX(valueSunH, -180.0f, 180.0f);
    SET_MINMAX(valueSunP, 0.0f, 90.0f);
    SET_MINMAX(valueAmbR, 0.0f, 255.0f);
    SET_MINMAX(valueAmbG, 0.0f, 255.0f);
    SET_MINMAX(valueAmbB, 0.0f, 255.0f);
    SET_MINMAX(valueFogR, 0.0f, 255.0f);
    SET_MINMAX(valueFogG, 0.0f, 255.0f);
    SET_MINMAX(valueFogB, 0.0f, 255.0f);
    SET_MINMAX(valueFogN, -50.0f, 500.0f);
    SET_MINMAX(valueFogF, -50.0f, 500.0f);
    SET_MINMAX(valueFogM, 0.0f, 255.0f);

    #undef SET_MINMAX

    // ==========================================================
    // 9. Weather combo (ID 0x163) with weather preset entries
    // ==========================================================
    {
        this->comboWeather = DebugUI_FindElemBase(parentElem, "comboWeather");

        if (this->comboWeather) {
            // Add "None" entry with userData = -1
            System::String noneBuf("None");
            uint8_t spriteBuf[16];
            ::new (spriteBuf) UI::Sprite();
            reinterpret_cast<UI::ComboElement*>(this->comboWeather)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), noneBuf, (void*)(intptr_t)-1);  // B.3.6 bridge

            // Populate from weather list at gameWorld+0x130.
            // STANDALONE: guard a null weather manager / list.
            void** weatherListPtr = *(void***)((uintptr_t)gameWorld + 0x130);
            uintptr_t weatherList = weatherListPtr ? *(uintptr_t*)weatherListPtr : 0;
            int weatherCount = weatherList ? *(int*)(weatherList + 0x30) : 0;
            int selectedIndex = 0;
            for (int i = 0; i < weatherCount; i++) {
                void* entry = *(void**)((uintptr_t)i * 0x10 +
                    *(uintptr_t*)(weatherList + 0x28));
                // `GetPath` returns by value through a hidden return slot and the callee
                // CONSTRUCTS into it; copy-initialising the local is the same store.
                System::Filename pathBuf =
                    reinterpret_cast<const ::Object::External*>(entry)->GetPath();  // B.3.3 bridge
                System::Filename filenameBuf = pathBuf.GetPathFilename(false);

                uint8_t spr[16];
                ::new (spr) UI::Sprite();
                reinterpret_cast<UI::ComboElement*>(this->comboWeather)->AddItem(*reinterpret_cast<const UI::Sprite*>(spr), *reinterpret_cast<const System::String*>(&filenameBuf), (void*)(intptr_t)i);  // B.3.6 bridge

                // Check if this weather is currently active
                uintptr_t activeCheck = *(uintptr_t*)((uintptr_t)i * 8 +
                    (uintptr_t)weatherListPtr[10]);
                if (activeCheck != 0 && *(float*)(activeCheck + 0xC4) > 0.0f) {
                    selectedIndex = i + 1;
                }
            }
            reinterpret_cast<UI::ComboElement*>(this->comboWeather)->SetCurrentItem(selectedIndex);  // B.3.6 bridge
            reinterpret_cast<UI::Element*>(this->comboWeather)->SetElementId(0x163);  // B.3.6 bridge
        }
    }

    // ==========================================================
    // 10. Action buttons (IDs 0x164, 0x165)
    // ==========================================================
    this->buttonSave = DebugUI_FindElemBase(parentElem, "buttonSave");
    if (this->buttonSave) reinterpret_cast<UI::Element*>(this->buttonSave)->SetElementId(0x164);  // B.3.6 bridge

    this->buttonMenuScene = DebugUI_FindElemBase(parentElem, "buttonMenuScene");
    if (this->buttonMenuScene) reinterpret_cast<UI::Element*>(this->buttonMenuScene)->SetElementId(0x165);  // B.3.6 bridge

    // ==========================================================
    // Initialize day slider from current game time
    // ==========================================================
    {
        // STANDALONE: gw+0x128 (weather time) can be null; Time_AsSeconds(null+8)
        // would fault. SetDayLighting derefs **(gw+0x130) (the weather table), also
        // null-able — only call it when that chain is live.
        uintptr_t weatherTime = *(uintptr_t*)((uintptr_t)gameWorld + 0x128);
        float timeSeconds = weatherTime ? reinterpret_cast<const System::Time*>((void*)(weatherTime + 8))->AsSeconds() : 0.0f;   // B.3.4 bridge
        // Convert time to month index: (seconds / secondsPerYear) * monthsPerYear
        // DAT_14017225c = 3600.0f, DAT_1401721b8 = 12.0f
        int monthIndex = (int)(timeSeconds / 3600.0f * 12.0f);
        if (monthIndex > 11) monthIndex = 0;

        void** weatherLive = *(void***)((uintptr_t)gameWorld + 0x130);
        if (this->sliderDay) {
            *(float*)((uintptr_t)this->sliderDay + 0x128) = (float)(monthIndex * 30);
            if (weatherLive && *weatherLive) DebugUI_SetDayLighting_Impl(this, monthIndex);
        }
    }

    // ==========================================================
    // 11. Performance counter elements (stored in struct fields)
    // ==========================================================
    this->numberFrameMS   = DebugUI_FindElemBase(parentElem, "numberFrameMS");
    this->numberTotalMS   = DebugUI_FindElemBase(parentElem, "numberTotalMS");
    this->numberFPS       = DebugUI_FindElemBase(parentElem, "numberFPS");
    this->numberDrawCalls = DebugUI_FindElemBase(parentElem, "numberDrawCalls");
    this->numberTriangles = DebugUI_FindElemBase(parentElem, "numberTriangles");
    this->numberVertices  = DebugUI_FindElemBase(parentElem, "numberVertices");
    this->numberBatches   = DebugUI_FindElemBase(parentElem, "numberBatches");

    // Zero the smoothing accumulators
    this->smoothedFPS = 0.0f;
    this->smoothedFrameMS = 0.0f;
    this->smoothedTotalMS = 0.0f;
}

// ============================================================
// DebugUI::Slot6 (vtable +0x30) — No-op
// ============================================================

// ============================================================
// STANDALONE promotion — real DebugUI controller vtable
// ============================================================
// The developer debug/cheat panel is opened by the game's own data-driven
// dialog system (a .rsc that names DebugUI as its controller, opened via the
// dev keybind). In standalone this type was registered as a passive
// UI_DIALOG_STUB (all slots StubNop) in ui_dialog_stub_factories.cpp, so the
// panel window rendered from its .rsc but Slot4 (element binding / combo
// populate / element-id assignment / perf-counter binding) never ran — hence
// "it launches but none of the game data is linked to it". Slot7 (swatches),
// Slot8 (perf counters) and Slot9 (button/combo/slider events) were likewise
// dead. This block promotes DebugUI to a real per-class vtable wiring the
// reimplemented slots above, following the CitizenUI/BuildRequirementUI
// promotion pattern. The stub REGISTER_TYPE in ui_dialog_stub_factories.cpp is
// REMOVED to avoid a duplicate registration.
//
// Verified vtable slot indices (exe DebugUI vtable @ 0x14015b870, read live):
//   +0x20 (idx4) Slot4 = Setup (framework-dispatched once after construction)
//   +0x30 (idx6)       = Slot6 no-op (OnClosed)
//   +0x38 (idx7) Slot7 = swatches/day text (framework OnUpdateLayout drives it
//                        on show/resize; Runtime dispatch is live in standalone)
//   +0x40 (idx8)       = per-frame OnUpdate. The real +0x40 override IS Slot8
//                        (perf counters), so we call Slot8 here; we ALSO append
//                        Slot7 so the color swatches / day text stay synced to
//                        the sliders every frame regardless of layout re-fires.
//   +0x48 (idx9) Slot9 = event dispatch (OnElementEvent drives it)
//
// Every exe-.rdata type-id DebugUI dereferences is NULL in standalone (raw
// .rdata addr -> ResolveAddr returns 0). EnsureGlobals lazy-fills them from the
// live Runtime factory tree (post-Engine::Startup) by each id's REAL registered
// name (the engine_funcs labels now match, verified against the Ghidra
// registration writers):
//   TypeId_ComponentDescription_FC(0x1401c7318)        -> "ComponentDescription"
//   TypeId_RawMaterialDescription(0x1401c7cb8)-> "RawMaterialDescription"
//   TypeId_InteractComponent_FC(0x1401c7900) -> "InteractComponent"
//   TypeId_InteractDescription_FC(0x1401c7938)  -> "InteractDescription"
//   TypeId_CitizenDescription(0x1401c7190)    -> "CitizenDescription"
//   TypeId_DiseaseDescription(0x1401c5948)    -> "Disease"
//   TypeId_DrawableType(0x1401c47c8)          -> "PopulationDescription"
//   TypeId_Element                     -> "Internal"
//   TypeId_CitizenComponent_FC(0x1401c7158) -> "CitizenComponent" (name now correct)
//     (via GetTypeIdByName, matching the proven CitizenUI heal)
// Slot8 reads the MainLoop through g_pMainLoopSingleton (the exe's exref
// 0x140151440 names the same symbol) and opens with a null check on it.

// [9] 0x140060bc0
void DebugUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                             int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    // Resource-spawn buttons use no type-ids; the entity-manipulation buttons
    // (raw-material purge, spawn citizens, disasters, tornado) do, and those ids
    // reliably resolve above. The body stays the free DebugUI_Slot9: it is reached
    // BY NAME with a null self from standalone/disaster_harness.cpp.
    DebugUI_Slot9(this, reinterpret_cast<uint64_t>(element), a, b);
}

// create 0x140010ec0: MemAlloc(0x130, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* DebugUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DebugUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DebugUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* DebugUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DebugUI());
}

DEFINE_EXE_TYPE(DebugUI, "DialogController")

