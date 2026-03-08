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

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Static inline implementations (replaces fn_* exe pointer calls)
// ============================================================

// Game_GetMonthFromTime (FUN_1400932A0)
// Converts weather manager time to month index 0-11.
// param_1 = weatherMgr ptr; time stored at +8.
static int DebugUI_GetMonthFromTime_Impl(void* weatherMgr) {
    float seconds = fn_Time_AsSeconds((void*)((uintptr_t)weatherMgr + 8));
    int month = (int)((seconds / 25200.0f) * 12.0f);  // 25200s/yr, 12 months
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
    // weatherTable = **(gameWorld+0x130)
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    uintptr_t weatherTable = *(uintptr_t*)weatherListPtr;
    uintptr_t entry = weatherTable + (uintptr_t)month * 0x20;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x80));
    *(float*)(*(uintptr_t*)(s + 0x80) + 0x128) = (float)*(uint8_t*)(entry + 0x3e); // sunR
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x88));
    *(float*)(*(uintptr_t*)(s + 0x88) + 0x128) = (float)*(uint8_t*)(entry + 0x3d); // sunG
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x90));
    *(float*)(*(uintptr_t*)(s + 0x90) + 0x128) = (float)*(uint8_t*)(entry + 0x3c); // sunB
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xe0));
    *(uint32_t*)(*(uintptr_t*)(s + 0xe0) + 0x128) = *(uint32_t*)(entry + 0x50);    // sunH (float)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xe8));
    *(uint32_t*)(*(uintptr_t*)(s + 0xe8) + 0x128) = *(uint32_t*)(entry + 0x54);    // sunP (float)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x98));
    *(float*)(*(uintptr_t*)(s + 0x98) + 0x128) = (float)*(uint8_t*)(entry + 0x3a); // ambR
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xa0));
    *(float*)(*(uintptr_t*)(s + 0xa0) + 0x128) = (float)*(uint8_t*)(entry + 0x39); // ambG
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xa8));
    *(float*)(*(uintptr_t*)(s + 0xa8) + 0x128) = (float)*(uint8_t*)(entry + 0x38); // ambB
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xb0));
    *(float*)(*(uintptr_t*)(s + 0xb0) + 0x128) = (float)*(uint8_t*)(entry + 0x42); // fogR
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xb8));
    *(float*)(*(uintptr_t*)(s + 0xb8) + 0x128) = (float)*(uint8_t*)(entry + 0x41); // fogG
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xc0));
    *(float*)(*(uintptr_t*)(s + 0xc0) + 0x128) = (float)*(uint8_t*)(entry + 0x40); // fogB
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xc8));
    *(uint32_t*)(*(uintptr_t*)(s + 0xc8) + 0x128) = *(uint32_t*)(entry + 0x44);    // fogN (float)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xd0));
    *(uint32_t*)(*(uintptr_t*)(s + 0xd0) + 0x128) = *(uint32_t*)(entry + 0x48);    // fogF (float)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xd8));
    *(float*)(*(uintptr_t*)(s + 0xd8) + 0x128) =
        *(float*)(entry + 0x4c) * 255.0f;  // fogM (normalize)
}

// DebugUI_WriteLightingColors (FUN_140061530)
// Reads slider values from UI elements, writes packed ARGB colors to weather table.
// Inverse of SetDayLighting: slider → weather table.
static void DebugUI_WriteLightingColors_Impl(DebugUI* self, int month) {
    uintptr_t s = (uintptr_t)self;
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    uintptr_t weatherTable = *(uintptr_t*)weatherListPtr;
    uintptr_t entry = weatherTable + (uintptr_t)month * 0x20;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x80));
    float sunR = *(float*)(*(uintptr_t*)(s + 0x80) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x88));
    float sunG = *(float*)(*(uintptr_t*)(s + 0x88) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x90));
    // Pack sun ARGB at entry+0x3c
    *(uint32_t*)(entry + 0x3c) = ((int)sunR << 8 | (int)sunG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0x90) + 0x128) | 0xFF000000;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x98));
    float ambR = *(float*)(*(uintptr_t*)(s + 0x98) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xa0));
    float ambG = *(float*)(*(uintptr_t*)(s + 0xa0) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xa8));
    // Pack amb ARGB at entry+0x38
    *(uint32_t*)(entry + 0x38) = ((int)ambR << 8 | (int)ambG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0xa8) + 0x128) | 0xFF000000;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xb0));
    float fogR = *(float*)(*(uintptr_t*)(s + 0xb0) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xb8));
    float fogG = *(float*)(*(uintptr_t*)(s + 0xb8) + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xc0));
    // Pack fog ARGB at entry+0x40
    *(uint32_t*)(entry + 0x40) = ((int)fogR << 8 | (int)fogG) << 8
        | (int)*(float*)(*(uintptr_t*)(s + 0xc0) + 0x128) | 0xFF000000;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xe0));
    *(uint32_t*)(entry + 0x50) = *(uint32_t*)(*(uintptr_t*)(s + 0xe0) + 0x128); // sunH
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xe8));
    *(uint32_t*)(entry + 0x54) = *(uint32_t*)(*(uintptr_t*)(s + 0xe8) + 0x128); // sunP
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xc8));
    *(uint32_t*)(entry + 0x44) = *(uint32_t*)(*(uintptr_t*)(s + 0xc8) + 0x128); // fogN
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xd0));
    *(uint32_t*)(entry + 0x48) = *(uint32_t*)(*(uintptr_t*)(s + 0xd0) + 0x128); // fogF
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xd8));
    *(float*)(entry + 0x4c) = *(float*)(*(uintptr_t*)(s + 0xd8) + 0x128) / 255.0f; // fogM
}

// DebugUI_OnDaySliderChanged (FUN_1400607E0)
// Reads slider value (0-359), converts to game time, resets weather, sets lighting.
// DAT_140172238 = 360.0f (slider range), DAT_14017225c = 25200.0f (seconds/year)
// DAT_1401721cc = 30.0f (days/month for month index calc)
static void DebugUI_OnDaySliderChanged_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x78));
    float sliderVal = *(float*)(*(uintptr_t*)(s + 0x78) + 0x128);

    // Convert slider (0-359) to game time in ticks
    int64_t timeTicks;
    fn_Time_Ctor_Float(&timeTicks, (sliderVal / 360.0f) * 25200.0f);

    // Set game world time
    *(int64_t*)(*(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x128) + 8) = timeTicks;

    // Reset weather time to 0
    int64_t zeroTick;
    fn_Time_Ctor_Float(&zeroTick, 0.0f);
    void** weatherListPtr = *(void***)((uintptr_t)(*g_pGameWorldSingleton) + 0x130);
    if (fn_Weather_ResetTime) fn_Weather_ResetTime(weatherListPtr, &zeroTick);

    // Update lighting sliders for the new month
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x78));
    int monthIdx = (int)(sliderVal / 30.0f);  // 30 days/month
    DebugUI_SetDayLighting_Impl(self, monthIdx);
}

// DebugUI_OnDiseaseComboChanged (FUN_14005FC60)
// Handles disease combo selection — updates internal pointer.
// self+0x38 = diseaseCombo (InternalBasePtr); combo item data is the disease entity.
static void DebugUI_OnDiseaseComboChanged_Impl(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
    int selIdx = *(int*)(*(uintptr_t*)(s + 0x38) + 0x118);  // selected item index
    if (selIdx > 3) {
        // Only take action if index > 3 (first 3 are non-disease entries)
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
        void* comboElem = *(void**)(s + 0x38);
        void* itemData = fn_ComboElement_GetItemData(comboElem, selIdx);
        // Construct InternalBasePtr from item data — used internally by disease system
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, itemData);
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

    // Get selected combo index
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
    int selectedIdx = *(int*)(*(uintptr_t*)(s + 0x108) + 0x118);

    // Get item data (cast to int index into draw list)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
    void* itemData = fn_ComboElement_GetItemData(*(void**)(s + 0x108), selectedIdx);
    int selectedDrawIdx = (int)(intptr_t)itemData;

    // Get draw manager from game world
    uintptr_t world = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t* drawMgr = *(uintptr_t**)(world + 0x130);  // R14 = plVar1

    uintptr_t innerData = *(uintptr_t*)drawMgr;  // *plVar1
    int drawCount = *(int*)(innerData + 0x30);
    uintptr_t descArray = *(uintptr_t*)(innerData + 0x28);  // array of 0x10-stride descriptors

    void** geometryArray = (void**)(drawMgr[10]);  // R14+0x50 = plVar1[10] (offset 0x50)
    void** audioArray = (void**)(drawMgr[0xC]);    // R14+0x60 = plVar1[0xC] (offset 0x60)

    for (int i = 0; i < drawCount; i++) {
        if (selectedDrawIdx == i) {
            // Selected entry — create and show geometry if not already created
            if (geometryArray[i] == nullptr) {
                // Allocate and construct DrawGeometry (0xf0 bytes)
                void* geomMem = fn_MemAlloc(g_pMemAllocSingleton, 0xf0, 8);
                void* geometry = nullptr;
                if (geomMem) {
                    geometry = fn_DrawGeometry_Ctor(geomMem);
                }
                geometryArray[i] = geometry;

                // Initialize with descriptor data
                uintptr_t descPtr = *(uintptr_t*)(descArray + (uintptr_t)i * 0x10);
                fn_DrawGeometry_Init(geometryArray[i], (void*)descPtr);

                // Add to graphics scene
                void* graphicsScene = *(void**)(world + 0x60);
                fn_Graphics_Scene_AddGlobalObject(graphicsScene, geometryArray[i]);

                // Reset state to 0 (showing)
                *(int*)((uintptr_t)drawMgr + 0x10) = 0;

                // Check if this entry has an audio descriptor
                uintptr_t audioDescPtr = *(uintptr_t*)(descArray + (uintptr_t)i * 0x10 + 8);
                if (audioDescPtr != 0) {
                    // Allocate and construct AudioObjectGroup (0x48 bytes)
                    void* audioMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
                    void* audioObj = nullptr;
                    if (audioMem) {
                        audioObj = fn_AudioObjectGroup_Ctor(audioMem, (void*)audioDescPtr);
                    }

                    // Store via InternalBasePtr
                    uint8_t audioBasePtrBuf[8];
                    fn_InternalBasePtr_Ctor(audioBasePtrBuf, audioObj);
                    audioArray[i] = *(void**)audioBasePtrBuf;

                    // Register with audio manager
                    // Sound type from audio descriptor +0x20
                    int soundType = *(int*)(audioDescPtr + 0x20);
                    uint8_t audioRef[8];
                    *(void**)audioRef = audioArray[i];
                    void* audioMgr = *(void**)(world + 0xE8);
                    fn_AudioScene_RegisterObject(audioMgr, audioRef, soundType);

                    // Find audio component on the audio object
                    fn_InternalBasePtr_CheckPtr((void*)&audioArray[i]);
                    uint8_t audioCompBuf[8];
                    fn_Entity_FindAudioComponent((void*)audioArray[i], audioCompBuf);

                    // Check if position differs from zero — if so, move all emitters
                    float zeroPos[3] = {0.0f, 0.0f, 0.0f};
                    fn_InternalBasePtr_CheckPtr(audioCompBuf);
                    uintptr_t audioComp = *(uintptr_t*)audioCompBuf;

                    bool posEqual = fn_Vector3_Equals(zeroPos, (void*)(audioComp + 0x28));
                    if (!posEqual) {
                        int emitterCount = *(int*)(audioComp + 0x40);
                        uintptr_t emitterArray = *(uintptr_t*)(audioComp + 0x38);

                        for (int e = 0; e < emitterCount; e++) {
                            uint32_t emitterId = *(uint32_t*)(emitterArray + (uintptr_t)e * 0x10);
                            if (emitterId != 0) {
                                void* audioScene = *(void**)(world + 0x78);
                                fn_Audio_Scene_MoveEmitter(
                                    audioScene, emitterId,
                                    (void*)(audioComp + 0x28),  // old position
                                    zeroPos);                    // new position (zero)
                            }
                        }

                        // Update stored position to zero
                        *(float*)(audioComp + 0x28) = 0.0f;
                        *(float*)(audioComp + 0x2C) = 0.0f;
                        *(float*)(audioComp + 0x30) = 0.0f;
                    }

                    // Handle first emitter volume — mute it if needed
                    fn_InternalBasePtr_CheckPtr(audioCompBuf);
                    audioComp = *(uintptr_t*)audioCompBuf;
                    uint32_t* firstEmitter = *(uint32_t**)(audioComp + 0x38);
                    if (firstEmitter[0] == 0) {
                        // No emitter ID — just zero the volume field
                        firstEmitter[1] = 0;
                    } else {
                        // Check if volume is non-zero
                        float currentVolume = *(float*)&firstEmitter[1];
                        if (currentVolume != 0.0f) {
                            firstEmitter[1] = 0;
                            void* audioScene = *(void**)(world + 0x78);
                            fn_Audio_Scene_SetEmitterVolume(
                                audioScene, firstEmitter[0],
                                (void*)(audioComp + 0x28),
                                0.0f);
                        }
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
// Game_KillAllCitizens (FUN_140060000)
// Iterates ExternalFactory for all entities with BuildComponent (DAT_1401c7318).
// For each entity, searches components for CitizenComponent (DAT_1401c7cb8).
// If found, destroys the entity via Game_DestroyEntity.
// ============================================================
static void Game_KillAllCitizens_Impl() {
    void* factory = *g_pExternalFactorySingleton;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    void* entityList = *(void**)(game + 0xF0);

    // Iterate all entities with BuildComponent type
    void* entity = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, nullptr);
    while (entity) {
        // Verify this entity is actually a BuildComponent entity
        bool isBuild = fn_IsDerivedFrom(entity, *g_TypeId_BuildComponent);
        void* buildEntity = isBuild ? entity : nullptr;

        if (buildEntity) {
            // Search entity's components for a CitizenComponent
            uintptr_t e = (uintptr_t)buildEntity;
            int compCount = *(int*)(e + 0x30);
            void** compArray = *(void***)(e + 0x28);

            for (int i = 0; i < compCount; i++) {
                void* comp = compArray[i];
                // Get component type via vtable[2] (+0x10)
                typedef int (*fn_GetTypeId_t)(void*);
                int typeId = ((fn_GetTypeId_t)(*(void***)(comp))[2])(comp);
                if (typeId == (int)*g_TypeId_CitizenComponent) {
                    // Confirm with IsDerivedFrom
                    bool isCitizen = fn_IsDerivedFrom(comp, *g_TypeId_CitizenComponent);
                    if (isCitizen && comp != nullptr) {
                        fn_Game_DestroyEntity(entityList, buildEntity);
                    }
                    break;
                }
            }
        }

        entity = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, entity);
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
    void** storageArray = *(void***)(entityListBase + 0x90);

    for (int i = 0; i < storageCount; i++) {
        void* storage = storageArray[i];
        fn_InternalBasePtr_CheckPtr(&storage);

        // Check if storage has items to spawn (component+0x28 != 0)
        if (*(uintptr_t*)((uintptr_t)storage + 0x28) == 0) continue;

        for (int j = 0; j < 5; j++) {
            fn_InternalBasePtr_CheckPtr(&storage);
            int currentCount = *(int*)((uintptr_t)storage + 0x38);
            int capacity = fn_StorageComponent_GetCapacity(storage);
            if (capacity <= currentCount) break;

            // Spawn one item
            uint8_t retBuf[8] = {0};
            fn_InternalBasePtr_Ctor(retBuf, nullptr);
            fn_InternalBasePtr_CheckPtr(&storage);
            fn_StorageComponent_SpawnItem(storage, retBuf);
        }
    }
}

// ============================================================
// Game_SpawnCitizens (FUN_140060380)
// Spawns up to 4 new citizens at random buildings that have residence
// components with available capacity. Loads Template/Citizen.rsc,
// creates entity, finds CitizenAI component, sets up citizen.
// ============================================================
static void Game_SpawnCitizens_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    void* randomPtr = *(void**)(game + 0xD8);
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);
    int buildingCount = (int)*(int64_t*)(entityListBase + 8);

    if (buildingCount == 0) return;

    int spawnedTotal = 0;
    for (int attempt = 0; attempt < buildingCount && spawnedTotal < 4; attempt++) {
        // Get building from list
        void* building = *(void**)(*(uintptr_t*)entityListBase + (uintptr_t)attempt * 8);
        if (!building) continue;

        fn_InternalBasePtr_CheckPtr(&building);

        // Find ResidenceComponent on this building
        void* residenceComp = nullptr;
        {
            uintptr_t b = (uintptr_t)building;
            int compCount = *(int*)(b + 0x38);
            void** compArr = *(void***)(b + 0x30);
            for (int c = 0; c < compCount; c++) {
                void* comp = compArr[c];
                fn_InternalBasePtr_CheckPtr(&comp);
                typedef int (*fn_GetTypeId_t)(void*);
                int typeId = ((fn_GetTypeId_t)(*(void***)(comp))[2])(comp);
                if (typeId == (int)*g_TypeId_ResidenceComponent_FC) {
                    fn_InternalBasePtr_CheckPtr(&comp);
                    bool isRes = fn_IsDerivedFrom(comp, *g_TypeId_ResidenceComponent_FC);
                    residenceComp = isRes ? comp : nullptr;
                    break;
                }
            }
        }

        if (!residenceComp) continue;

        // Get max capacity from description
        void* desc = *(void**)((uintptr_t)residenceComp + 0x18);
        bool isResDesc = fn_IsDerivedFrom(desc, *g_TypeId_ResidenceDescription);
        void* resDesc = isResDesc ? desc : nullptr;
        if (!resDesc) continue;
        int maxCapacity = *(int*)((uintptr_t)resDesc + 0x30);

        // Sum current occupants across all sub-slots
        int currentOccupants = 0;
        int subCount = *(int*)((uintptr_t)residenceComp + 0x30);
        void** subArray = *(void***)((uintptr_t)residenceComp + 0x28);
        for (int s = 0; s < subCount; s++) {
            void* sub = subArray[s];
            fn_InternalBasePtr_CheckPtr(&sub);
            int occ = fn_ResidenceComponent_GetOccupantCount(sub, 0);
            currentOccupants += occ;
        }

        if (currentOccupants >= maxCapacity) continue;

        // Spawn citizen at this building
        // Get spawn point
        uint8_t spawnPos[16] = {0};
        fn_ResidenceComponent_GetSpawnPoint(residenceComp, spawnPos, 0, currentOccupants, nullptr);

        if (spawnedTotal >= 4) break;
        spawnedTotal++;

        // Random gender
        uint32_t gender = fn_Random_Rand(randomPtr, 2);

        // Load citizen template
        uint8_t filenameBuf[16];
        fn_Filename_CtorCStr(filenameBuf, "Template/Citizen.rsc");
        void* factory = *g_pExternalFactorySingleton;
        void* extTemplate = fn_ExternalFactory_FindOrLoad(factory, filenameBuf);
        fn_Filename_Dtor(filenameBuf);

        void* entityDesc = nullptr;
        if (extTemplate) {
            bool isEntityDesc = fn_IsDerivedFrom(extTemplate, *g_TypeId_BuildComponent);
            entityDesc = isEntityDesc ? extTemplate : nullptr;
        }
        if (!entityDesc) entityDesc = extTemplate;

        // Allocate and create entity (0x48 bytes)
        void* memBlock = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
        void* newEntity = nullptr;
        if (memBlock) {
            newEntity = fn_EntityFactory_CreateEntity(memBlock, entityDesc);
        }

        // Find CitizenAI component on the new entity
        uint8_t aiCompBuf[8] = {0};
        fn_InternalBasePtr_Ctor(aiCompBuf, nullptr);
        if (newEntity) {
            uintptr_t ne = (uintptr_t)newEntity;
            int compCount = *(int*)(ne + 0x38);
            void** compArr = *(void***)(ne + 0x30);
            for (int c = 0; c < compCount; c++) {
                void* comp = compArr[c];
                fn_InternalBasePtr_CheckPtr(&comp);
                typedef int (*fn_GetTypeId_t)(void*);
                int typeId = ((fn_GetTypeId_t)(*(void***)(comp))[2])(comp);
                if (typeId == (int)*g_TypeId_CitizenAIComponent_FC) {
                    fn_InternalBasePtr_CheckPtr(&comp);
                    bool isAI = fn_IsDerivedFrom(comp, *g_TypeId_CitizenAIComponent_FC);
                    void* aiComp = isAI ? comp : nullptr;
                    fn_InternalBasePtr_Ctor(aiCompBuf, aiComp);
                    break;
                }
            }
        }

        void* citizenAI = *(void**)aiCompBuf;
        if (citizenAI) {
            // Get citizen description
            void* citizenDesc = *(void**)((uintptr_t)citizenAI + 0x18);
            bool isCitDesc = fn_IsDerivedFrom(citizenDesc, *g_TypeId_CitizenDescription);
            void* citDesc = isCitDesc ? citizenDesc : nullptr;

            // Get age range from description
            float minAge = *(float*)((uintptr_t)citDesc + 0x50);
            float maxAge = *(float*)((uintptr_t)citDesc + 0x54);
            float randAge = fn_Random_RandFloat(randomPtr);
            float age = randAge * (maxAge - minAge) + minAge;

            // Get profession list from game
            void* profList = *(void**)(*(uintptr_t*)(game + 0xB0) + 0x30);

            // Setup citizen
            uint8_t parentBuf[8] = {0};
            fn_InternalBasePtr_Ctor(parentBuf, nullptr);
            fn_CitizenComponent_Setup(citizenAI, spawnPos, (gender != 0) ? 1 : 0, '\0',
                                      age, '\x01', parentBuf, profList);
        }

        // Register entity
        void* entityMgr = *(void**)(game + 0xE8);
        void* entityRef = newEntity;
        if (entityDesc) {
            fn_EntityList_RegisterEntity(entityMgr, &entityRef, *(int*)((uintptr_t)entityDesc + 0x20));
        }
    }
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
    int buildingCount = *(int*)(entityListBase + 0xA8);
    void* randomPtr = *(void**)(game + 0xD8);
    uint32_t randIdx = fn_Random_Rand(randomPtr, buildingCount);

    // Get disease description from combo selection
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    int diseaseIdx = *(int*)(*(uintptr_t*)(s + 0x30) + 0x118);
    void* descList = *(void**)(*(uintptr_t*)(game + 0xA8) + 0x20);
    void* diseaseEntry = *(void**)((uintptr_t)descList + (uintptr_t)diseaseIdx * 8);
    bool isDisease = fn_IsDerivedFrom(diseaseEntry, *g_TypeId_DiseaseDescription);
    void* diseaseDesc = isDisease ? diseaseEntry : nullptr;

    // Get building entity
    void* building = *(void**)(*(uintptr_t*)(entityListBase + 0xA0) + (uintptr_t)randIdx * 8);
    fn_InternalBasePtr_CheckPtr(&building);

    // Apply disease to building
    fn_DisasterComponent_Infect(building, diseaseDesc);
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
    uint32_t randIdx = fn_Random_Rand(randomPtr, buildingCount);

    // Get random building entity
    void* building = *(void**)(*(uintptr_t*)(entityListBase + 0x20) + (uintptr_t)randIdx * 8);
    fn_InternalBasePtr_CheckPtr(&building);

    // Find FireComponent on the building
    uint8_t fireCompBuf[8] = {0};
    fn_FindComponent_Fire(building, fireCompBuf);

    // Ignite
    void* fireComp = *(void**)fireCompBuf;
    fn_InternalBasePtr_CheckPtr(&fireComp);
    fn_FireComponent_Ignite(fireComp);
}

// ============================================================
// Game_SpawnTornado (FUN_14005FD20)
// Loads Template/Tornado.rsc, creates entity, registers it.
// ============================================================
static void Game_SpawnTornado_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // Load tornado template
    uint8_t filenameBuf[16];
    fn_Filename_CtorCStr(filenameBuf, "Template/Tornado.rsc");
    void* factory = *g_pExternalFactorySingleton;
    void* extTemplate = fn_ExternalFactory_FindOrLoad(factory, filenameBuf);
    fn_Filename_Dtor(filenameBuf);

    // Cast to entity description
    void* entityDesc = nullptr;
    if (extTemplate) {
        bool isEntityDesc = fn_IsDerivedFrom(extTemplate, *g_TypeId_BuildComponent);
        entityDesc = isEntityDesc ? extTemplate : nullptr;
    }
    if (!entityDesc) entityDesc = extTemplate;

    // Allocate and create entity (0x48 bytes)
    void* memBlock = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (memBlock) {
        newEntity = fn_EntityFactory_CreateEntity(memBlock, entityDesc);
    }

    // Register entity with entity manager
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, newEntity);
    void* entityRef = *(void**)entityBuf;
    void* entityMgr = *(void**)(game + 0xE8);
    fn_EntityList_RegisterEntity(entityMgr, &entityRef,
                                  *(int*)((uintptr_t)entityDesc + 0x20));
}

// ============================================================
// Game_InfectRandomCitizen (FUN_14005FE10)
// Picks a random citizen from the citizen list, infects them.
// ============================================================
static void Game_InfectRandomCitizen_Impl() {
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityListBase = *(uintptr_t*)(game + 0xF0);

    void* randomPtr = *(void**)(game + 0xD8);
    uint32_t citizenCount = *(uint32_t*)(entityListBase + 0xF8);
    uint32_t randIdx = fn_Random_Rand(randomPtr, citizenCount);

    // Get random citizen
    void* citizen = *(void**)(*(uintptr_t*)(entityListBase + 0xF0) + (uintptr_t)randIdx * 8);
    fn_InternalBasePtr_CheckPtr(&citizen);

    // Infect citizen's disease component
    fn_DiseaseComponent_Infect(citizen);

    // Original has a spurious MemFree(nullptr) — skip it
}

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
void DebugUI_Slot8(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Access engine singleton via exref (double-indirect pointer).
    // g_pEngineSingleton -> singleton ptr -> engine state.
    uintptr_t singleton = *(uintptr_t*)(*g_pEngineSingleton);
    uintptr_t engineState = *(uintptr_t*)(singleton + 0x10);

    // Read and display 4 debug counters from engine state
    int counterA = *(int*)(engineState + 0x2158);
    fn_NumberElement_SetValueInt(*(void**)(s + 0x58), counterA);

    int counterB = *(int*)(engineState + 0x215C);
    fn_NumberElement_SetValueInt(*(void**)(s + 0x40), counterB);

    int counterC = *(int*)(engineState + 0x2150);
    fn_NumberElement_SetValueInt(*(void**)(s + 0x48), counterC);

    int counterD = *(int*)(engineState + 0x2154);
    fn_NumberElement_SetValueInt(*(void**)(s + 0x50), counterD);

    // Read frame time and total time as Time objects (8-byte tick counts)
    int64_t frameTimeTicks = *(int64_t*)(singleton + 0x68);
    int64_t totalTimeTicks = *(int64_t*)(singleton + 0x58);
    float frameSeconds = fn_Time_AsSeconds(&frameTimeTicks);
    float totalSeconds = fn_Time_AsSeconds(&totalTimeTicks);

    // Exponential moving average smoothing constants
    const float alpha = 0.95f;   // DAT_140172138
    const float beta  = 0.05f;   // DAT_1401720d8

    float* smoothedFPS     = (float*)(s + 0x120);
    float* smoothedFrameMS = (float*)(s + 0x124);
    float* smoothedTotalMS = (float*)(s + 0x128);

    // Smooth: new = old * alpha + sample * beta
    // FPS is computed as 1/totalSeconds (instantaneous), smoothed separately
    float instantFPS = 1.0f / totalSeconds;  // DAT_14017214c (1.0f) / totalSeconds
    *smoothedTotalMS = (*smoothedTotalMS) * alpha + totalSeconds * beta;
    *smoothedFPS     = instantFPS * beta + (*smoothedFPS) * alpha;
    *smoothedFrameMS = (*smoothedFrameMS) * alpha + frameSeconds * beta;

    // Display smoothed FPS as integer
    fn_NumberElement_SetValueInt(*(void**)(s + 0x60), (int)*smoothedFPS);

    // Display smoothed frame time and total time in milliseconds
    fn_NumberElement_SetValueFloat(*(void**)(s + 0x68), *smoothedFrameMS * 1000.0f);
    fn_NumberElement_SetValueFloat(*(void**)(s + 0x70), *smoothedTotalMS * 1000.0f);
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
void DebugUI_Slot7(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Day slider text update ---
    // Get first child of sliderDay (a TextElement)
    void* sliderDay = *(void**)(s + 0x78);
    uint8_t childBuf[16];
    fn_Element_Child(sliderDay, childBuf, 0);
    void* textChild = *(void**)childBuf;

    // Read slider value from sliderDay element at +0x128 (current float value)
    float dayValue = *(float*)((uintptr_t)sliderDay + 0x128);

    // Build UI::String with format string from textChild+0xD8 and int param dayValue/30
    // UI::String::String(buf, *(textChild+0xD8), dayValue/30) is the engine call.
    // The format string is a UI::String reference. We construct a 16-byte UI::String
    // on the stack: [0..7] = string data, [8..15] = additional data
    uint8_t uiStrBuf[16];
    uint64_t fmtRef = *(uint64_t*)((uintptr_t)textChild + 0xD8);
    *(uint64_t*)&uiStrBuf[8] = *(uint64_t*)((uintptr_t)textChild + 0xE0);
    fn_UIString_Ctor_Index(uiStrBuf, fmtRef, (int)dayValue / 30);

    // Set the formatted text on the TextElement child
    fn_TextElement_SetText(textChild, uiStrBuf);

    // --- Sun color swatch ---
    void* imageColorSun = *(void**)(s + 0xF0);
    float sunR = *(float*)(*(uintptr_t*)(s + 0x80) + 0x128);
    float sunG = *(float*)(*(uintptr_t*)(s + 0x88) + 0x128);
    float sunB = *(float*)(*(uintptr_t*)(s + 0x90) + 0x128);
    // Pack ARGB: 0xFF000000 | (R << 16) | (G << 8) | B
    uint32_t sunColor = 0xFF000000
        | (((int)sunR << 8) | (int)sunG) << 8
        | (int)sunB;
    // Call vtable[10] (offset 0x50): SetColor(this, argb)
    void** sunVt = *(void***)imageColorSun;
    typedef void (*fn_SetColor_t)(void*, uint32_t);
    ((fn_SetColor_t)sunVt[10])(imageColorSun, sunColor);

    // --- Ambient color swatch ---
    void* imageColorAmb = *(void**)(s + 0xF8);
    float ambR = *(float*)(*(uintptr_t*)(s + 0x98) + 0x128);
    float ambG = *(float*)(*(uintptr_t*)(s + 0xA0) + 0x128);
    float ambB = *(float*)(*(uintptr_t*)(s + 0xA8) + 0x128);
    uint32_t ambColor = 0xFF000000
        | (((int)ambR << 8) | (int)ambG) << 8
        | (int)ambB;
    void** ambVt = *(void***)imageColorAmb;
    ((fn_SetColor_t)ambVt[10])(imageColorAmb, ambColor);

    // --- Fog color swatch ---
    void* imageColorFog = *(void**)(s + 0x100);
    float fogR = *(float*)(*(uintptr_t*)(s + 0xB0) + 0x128);
    float fogG = *(float*)(*(uintptr_t*)(s + 0xB8) + 0x128);
    float fogB = *(float*)(*(uintptr_t*)(s + 0xC0) + 0x128);
    uint32_t fogColor = 0xFF000000
        | (((int)fogR << 8) | (int)fogG) << 8
        | (int)fogB;
    void** fogVt = *(void***)imageColorFog;
    ((fn_SetColor_t)fogVt[10])(imageColorFog, fogColor);
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
    case 324:  Game_KillAllCitizens_Impl();        return;  // Kill all citizens
    case 325:  Game_SpawnItemsInStorage_Impl();    return;  // Spawn items in all storage
    case 326:  Game_SpawnCitizens_Impl();          return;  // Spawn new citizens
    case 327:  DebugUI_TriggerDisasterOnBuilding_Impl(self); return;  // Disaster on random building
    case 328:  Game_StartRandomFire_Impl();        return;  // Start random fire
    case 329:  Game_SpawnTornado_Impl();           return;  // Spawn tornado
    case 330:  Game_InfectRandomCitizen_Impl();    return;  // Infect random citizen

    // --- Debug render flag toggles (checkboxes) ---
    // param2 points to the checkbox element; flag is OR'd/AND'd into game+0xD0
    case 331: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x1);   return; }
    case 332: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x2);   return; }
    case 333: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x4);   return; }
    case 334: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x8);   return; }
    case 335: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x10);  return; }
    case 336: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x20);  return; }
    case 337: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x40);  return; }
    case 338: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x80);  return; }
    case 339: { void* cb = *(void**)&param2; DebugUI_ToggleDebugFlag(cb, 0x200); return; }

    default: return;
    }
}

// ============================================================
// DebugUI_ToggleDebugFlag -- helper for debug render checkboxes
// ============================================================
// Reads checkbox checked state (+0xD8), sets or clears a bit in
// the game world's debug flags at game+0xD0.
//
// Original: 0x14005FCC0
void DebugUI_ToggleDebugFlag(void* checkboxElement, uint32_t flag) {
    void* game = *g_pGameWorldSingleton;
    uint32_t* debugFlags = (uint32_t*)((uintptr_t)game + 0xD0);

    // Read checkbox state: element+0xD8 is the checked bool
    bool checked = *(uint8_t*)((uintptr_t)checkboxElement + 0xD8) != 0;

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

// Helper: Find element by name, return raw pointer.
static void* DebugUI_FindElem(void* parentElem, const char* name) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);
    uint64_t result = 0;
    fn_FindElement(parentElem, &result, nameBuf);
    fn_String_Dtor(nameBuf);
    return (void*)result;
}

// Helper: Find element by name, wrap in InternalBasePtr via IsDerivedFrom.
static void* DebugUI_FindElemBase(void* parentElem, const char* name) {
    void* rawElem = DebugUI_FindElem(parentElem, name);
    if (!rawElem || !fn_IsDerivedFrom(rawElem, *g_TypeId_UIElementBase))
        rawElem = nullptr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(void**)basePtrBuf;
}

// Helper: Find element, set element ID, return as InternalBasePtr.
static void* DebugUI_FindAndSetId(void* parentElem, const char* name, int id) {
    void* elem = DebugUI_FindElemBase(parentElem, name);
    fn_Element_SetElementId(elem, id);
    return elem;
}

void DebugUI_Slot4(DebugUI* self) {
    uintptr_t s = (uintptr_t)self;
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
        self->diseaseCombo = combo;

        // Populate from ExternalList (resource descriptions at gameWorld+0xA8)
        void* gameWorld = *g_pGameWorldSingleton;
        void* externalList = *(void**)((uintptr_t)gameWorld + 0xa8);
        int resourceCount = fn_ExternalList_GetResourceCount(externalList);
        for (int i = 0; i < resourceCount; i++) {
            void* resource = *(void**)((uintptr_t)i * 8 +
                *(uintptr_t*)((uintptr_t)externalList + 0x20));
            bool isDisease = fn_IsDerivedFrom(resource, *g_TypeId_DiseaseDescription);
            if (isDisease) {
                uint8_t textBuf[8];
                fn_StringTable_GetText(*(void**)((uintptr_t)resource + 0x20), textBuf, 0);
                uint8_t spriteBuf[16];
                fn_Sprite_Default_Ctor(spriteBuf);
                fn_ComboElement_AddItem(self->diseaseCombo, spriteBuf, textBuf, nullptr);
                fn_String_Dtor(textBuf);
            }
        }
        fn_ComboElement_SetCurrentItem(self->diseaseCombo, 0);
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
    *(uint8_t*)((uintptr_t)elem + 0xD8) = cheatFlags & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noHungerCheck",      0x14C);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 1) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noColdCheck",        0x14D);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 2) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noHealthCheck",      0x14E);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 3) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noSickCheck",        0x14F);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 4) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noCropDiseaseCheck", 0x150);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 5) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noFireCheck",        0x151);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 6) & 1;

    elem = DebugUI_FindAndSetId(parentElem, "noTornadoCheck",     0x152);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 7) & 1;

    // educatedCheck -- bit 9 (bit 8 is skipped)
    elem = DebugUI_FindAndSetId(parentElem, "educatedCheck",      0x153);
    *(uint8_t*)((uintptr_t)elem + 0xD8) = (cheatFlags >> 9) & 1;

    // ==========================================================
    // 6. Draw combo (ID 0x166) with drawable type entries
    // ==========================================================
    {
        void* combo = DebugUI_FindElemBase(parentElem, "drawCombo");
        self->drawCombo = combo;
        fn_Element_SetElementId(self->drawCombo, 0x166);

        // Populate from drawable type list at gameWorld+0xF0
        uintptr_t drawableList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
        int drawCount = *(int*)(drawableList + 200); // +0xC8
        int selectedIndex = 0;
        for (int i = 0; i < drawCount; i++) {
            void* entry = *(void**)((uintptr_t)i * 8 +
                *(uintptr_t*)(drawableList + 0xC0));
            void* inner = *(void**)((uintptr_t)entry + 0x18);
            bool isDrawable = fn_IsDerivedFrom(inner, *g_TypeId_DrawableType);
            void* drawableDesc = isDrawable ? inner : nullptr;

            uint8_t spriteBuf[16];
            fn_Sprite_Default_Ctor(spriteBuf);
            fn_ComboElement_AddItem(self->drawCombo, spriteBuf,
                (void*)((uintptr_t)drawableDesc + 0x20), entry);

            // Check if current entry matches the null/default internal ptr
            uint8_t nullBuf[8];
            fn_InternalBasePtr_Ctor(nullBuf, nullptr);
            if (entry == *(void**)nullBuf) {
                selectedIndex = i + 4;
            }
        }
        fn_ComboElement_SetCurrentItem(self->drawCombo, selectedIndex);
    }

    // ==========================================================
    // 7. Lighting sliders -- find, store, set IDs and min/max
    // ==========================================================
    // Slider element: +0x118 = min value, +0x11C = max value

    #define SETUP_SLIDER(field, name, id, minVal, maxVal) do { \
        self->field = DebugUI_FindElemBase(parentElem, name); \
        fn_Element_SetElementId(self->field, id); \
        *(float*)((uintptr_t)self->field + 0x118) = minVal; \
        *(float*)((uintptr_t)self->field + 0x11C) = maxVal; \
    } while(0)

    SETUP_SLIDER(sliderDay, "sliderDay", 0x154, 0.0f, 359.0f);
    SETUP_SLIDER(valueSunR, "valueSunR", 0x155, 0.0f, 255.0f);
    SETUP_SLIDER(valueSunG, "valueSunG", 0x156, 0.0f, 255.0f);
    SETUP_SLIDER(valueSunB, "valueSunB", 0x157, 0.0f, 255.0f);
    SETUP_SLIDER(valueSunH, "valueSunH", 0x158, -180.0f, 180.0f);
    SETUP_SLIDER(valueSunP, "valueSunP", 0x159, 0.0f, 90.0f);
    SETUP_SLIDER(valueAmbR, "valueAmbR", 0x15A, 0.0f, 255.0f);
    SETUP_SLIDER(valueAmbG, "valueAmbG", 0x15B, 0.0f, 255.0f);
    SETUP_SLIDER(valueAmbB, "valueAmbB", 0x15C, 0.0f, 255.0f);
    SETUP_SLIDER(valueFogR, "valueFogR", 0x15D, 0.0f, 255.0f);
    SETUP_SLIDER(valueFogG, "valueFogG", 0x15E, 0.0f, 255.0f);
    SETUP_SLIDER(valueFogB, "valueFogB", 0x15F, 0.0f, 255.0f);
    SETUP_SLIDER(valueFogN, "valueFogN", 0x160, -50.0f, 500.0f);
    SETUP_SLIDER(valueFogF, "valueFogF", 0x161, -50.0f, 500.0f);
    SETUP_SLIDER(valueFogM, "valueFogM", 0x162, 0.0f, 255.0f);

    #undef SETUP_SLIDER

    // ==========================================================
    // 8. Color preview images (no IDs, just stored)
    // ==========================================================
    self->imageColorSun = DebugUI_FindElemBase(parentElem, "imageColorSun");
    self->imageColorAmb = DebugUI_FindElemBase(parentElem, "imageColorAmb");
    self->imageColorFog = DebugUI_FindElemBase(parentElem, "imageColorFog");

    // ==========================================================
    // 9. Weather combo (ID 0x163) with weather preset entries
    // ==========================================================
    {
        self->comboWeather = DebugUI_FindElemBase(parentElem, "comboWeather");

        // Add "None" entry with userData = -1
        uint8_t noneBuf[8];
        fn_String_Ctor_CStr(noneBuf, "None");
        uint8_t spriteBuf[16];
        fn_Sprite_Default_Ctor(spriteBuf);
        fn_ComboElement_AddItem(self->comboWeather, spriteBuf, noneBuf, (void*)(intptr_t)-1);
        fn_String_Dtor(noneBuf);

        // Populate from weather list at gameWorld+0x130
        void** weatherListPtr = *(void***)((uintptr_t)gameWorld + 0x130);
        uintptr_t weatherList = *(uintptr_t*)weatherListPtr;
        int weatherCount = *(int*)(weatherList + 0x30);
        int selectedIndex = 0;
        for (int i = 0; i < weatherCount; i++) {
            void* entry = *(void**)((uintptr_t)i * 0x10 +
                *(uintptr_t*)(weatherList + 0x28));
            void* path = fn_External_GetPath(entry);
            uint8_t filenameBuf[16];
            fn_Filename_GetPathFilename(path, filenameBuf, false);

            uint8_t spr[16];
            fn_Sprite_Default_Ctor(spr);
            fn_ComboElement_AddItem(self->comboWeather, spr, filenameBuf,
                (void*)(intptr_t)i);

            fn_String_Dtor(filenameBuf);
            fn_String_Dtor((uint8_t*)filenameBuf + 8);

            // Check if this weather is currently active
            uintptr_t activeCheck = *(uintptr_t*)((uintptr_t)i * 8 +
                (uintptr_t)weatherListPtr[10]);
            if (activeCheck != 0 && *(float*)(activeCheck + 0xC4) > 0.0f) {
                selectedIndex = i + 1;
            }
        }
        fn_ComboElement_SetCurrentItem(self->comboWeather, selectedIndex);
        fn_Element_SetElementId(self->comboWeather, 0x163);
    }

    // ==========================================================
    // 10. Action buttons (IDs 0x164, 0x165)
    // ==========================================================
    self->buttonSave = DebugUI_FindElemBase(parentElem, "buttonSave");
    fn_Element_SetElementId(self->buttonSave, 0x164);

    self->buttonMenuScene = DebugUI_FindElemBase(parentElem, "buttonMenuScene");
    fn_Element_SetElementId(self->buttonMenuScene, 0x165);

    // ==========================================================
    // Initialize day slider from current game time
    // ==========================================================
    {
        uintptr_t weatherTime = *(uintptr_t*)((uintptr_t)gameWorld + 0x128);
        float timeSeconds = fn_Time_AsSeconds((void*)(weatherTime + 8));
        // Convert time to month index: (seconds / secondsPerYear) * monthsPerYear
        // DAT_14017225c = 25200.0f, DAT_1401721b8 = 12.0f
        int monthIndex = (int)(timeSeconds / 25200.0f * 12.0f);
        if (monthIndex > 11) monthIndex = 0;

        *(float*)((uintptr_t)self->sliderDay + 0x128) = (float)(monthIndex * 30);
        DebugUI_SetDayLighting_Impl(self, monthIndex);
    }

    // ==========================================================
    // 11. Performance counter elements (stored in struct fields)
    // ==========================================================
    self->numberFrameMS   = DebugUI_FindElemBase(parentElem, "numberFrameMS");
    self->numberTotalMS   = DebugUI_FindElemBase(parentElem, "numberTotalMS");
    self->numberFPS       = DebugUI_FindElemBase(parentElem, "numberFPS");
    self->numberDrawCalls = DebugUI_FindElemBase(parentElem, "numberDrawCalls");
    self->numberTriangles = DebugUI_FindElemBase(parentElem, "numberTriangles");
    self->numberVertices  = DebugUI_FindElemBase(parentElem, "numberVertices");
    self->numberBatches   = DebugUI_FindElemBase(parentElem, "numberBatches");

    // Zero the smoothing accumulators
    self->smoothedFPS = 0.0f;
    self->smoothedFrameMS = 0.0f;
    self->smoothedTotalMS = 0.0f;
}
