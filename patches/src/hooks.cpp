// Hook management and reimplementation registry.
// Low-level hooking + build number patch + reimplementation table.

#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstring>

// ============================================================
// Low-level hook infrastructure (unchanged)
// ============================================================

bool InstallHook(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12; // mov rax, imm64 (10) + jmp rax (2)

    // prologueSize must cover at least the jump and fit in our save buffer
    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Allocate executable trampoline: original bytes + jump back
    info->trampoline = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    // Copy original prologue bytes to trampoline
    memcpy(info->trampoline, info->original, prologueSize);

    // Append jump back to (target + prologueSize)
    uint8_t* jmpBack = (uint8_t*)info->trampoline + prologueSize;
    jmpBack[0] = 0x48; jmpBack[1] = 0xB8; // mov rax, imm64
    *(uintptr_t*)(jmpBack + 2) = targetAddr + prologueSize;
    jmpBack[10] = 0xFF; jmpBack[11] = 0xE0; // jmp rax

    // Overwrite target with jump to detour (always 12 bytes)
    DWORD oldProtect;
    VirtualProtect((void*)targetAddr, prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* patch = (uint8_t*)targetAddr;
    patch[0] = 0x48; patch[1] = 0xB8; // mov rax, imm64
    *(uintptr_t*)(patch + 2) = (uintptr_t)detourFunc;
    patch[10] = 0xFF; patch[11] = 0xE0; // jmp rax

    // NOP any remaining bytes between the jump and the prologue boundary
    for (size_t i = JUMP_SIZE; i < prologueSize; i++)
        patch[i] = 0x90;

    VirtualProtect((void*)targetAddr, prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)targetAddr, prologueSize);

    return true;
}

void RemoveHook(HookInfo* info) {
    if (!info->target) return;

    DWORD oldProtect;
    VirtualProtect(info->target, info->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(info->target, info->original, info->patchSize);
    VirtualProtect(info->target, info->patchSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), info->target, info->patchSize);

    if (info->trampoline) {
        VirtualFree(info->trampoline, 0, MEM_RELEASE);
        info->trampoline = nullptr;
    }
    info->target = nullptr;
}

// ============================================================
// Build number patch (data patch, not a hook)
// ============================================================

static bool PatchBuildNumber() {
    HMODULE hRuntime = GetModuleHandleA("Runtime-x64.dll");
    if (!hRuntime) return false;

    BYTE* base = (BYTE*)hRuntime;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;

    const char* dateTarget = "Sep 10 2017";
    const size_t dateLen = 11;
    BYTE* dateStr = nullptr;

    for (DWORD i = 0; i < imageSize - dateLen; i++) {
        if (memcmp(base + i, dateTarget, dateLen) == 0) {
            dateStr = base + i;
            break;
        }
    }

    if (!dateStr) return false;

    DWORD oldProtect;

    // Patch date string: "Sep 10 2017" -> "Dec 56 2012"
    VirtualProtect(dateStr, 12, PAGE_READWRITE, &oldProtect);
    memcpy(dateStr, "Dec 56 2012", 11);
    VirtualProtect(dateStr, 12, oldProtect, &oldProtect);

    // Patch month number for Dec: "12" -> "34"
    BYTE* decMonthNum = dateStr + 0x70;
    if (decMonthNum[0] == '1' && decMonthNum[1] == '2') {
        VirtualProtect(decMonthNum, 4, PAGE_READWRITE, &oldProtect);
        decMonthNum[0] = '3';
        decMonthNum[1] = '4';
        VirtualProtect(decMonthNum, 4, oldProtect, &oldProtect);
    }

    // Fallback: direct buffer overwrite
    BYTE* buildBuf = dateStr + 12;
    if (memcmp(buildBuf, "170910", 6) == 0) {
        VirtualProtect(buildBuf, 6, PAGE_READWRITE, &oldProtect);
        memcpy(buildBuf, "123456", 6);
        VirtualProtect(buildBuf, 6, oldProtect, &oldProtect);
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Build number patch applied -> 123456\n");
#endif
    return true;
}

static void InstallBuildPatch() { PatchBuildNumber(); }
static void RemoveBuildPatch()  { /* data patch, no undo */ }

// ============================================================
// Reimplementation registry
// ============================================================
// Add entries here as functions are reimplemented.
// Each entry in engine/reimpl/*.cpp exposes Install/Remove functions.

// --- MainLoop reimplementations (engine/reimpl/mainloop.cpp) ---
extern void InstallMainLoopFrame();
extern void RemoveMainLoopFrame();
extern void InstallMainLoopUpdate();
extern void RemoveMainLoopUpdate();
extern void InstallMainLoopDraw();
extern void RemoveMainLoopDraw();
extern void InstallMainLoopSetNextLoop();
extern void RemoveMainLoopSetNextLoop();
extern void InstallMainLoopRun();
extern void RemoveMainLoopRun();
extern void InstallMainLoopIsRunningML();
extern void RemoveMainLoopIsRunningML();
extern void InstallMainLoopMarkFrameUpdateTime();
extern void RemoveMainLoopMarkFrameUpdateTime();
extern void InstallMainLoopAddPostUpdate();
extern void RemoveMainLoopAddPostUpdate();
extern void InstallMainLoopAddPostDraw();
extern void RemoveMainLoopAddPostDraw();
extern void InstallMainLoopRemovePostUpdate();
extern void RemoveMainLoopRemovePostUpdate();
extern void InstallMainLoopRemovePostDraw();
extern void RemoveMainLoopRemovePostDraw();
extern void InstallMainLoopOnVideoErrorStatic();
extern void RemoveMainLoopOnVideoErrorStatic();
extern void InstallMainLoopOnVideoRestartStatic();
extern void RemoveMainLoopOnVideoRestartStatic();
extern void InstallMainLoopCreateVideoCtx();
extern void RemoveMainLoopCreateVideoCtx();
extern void InstallMainLoopReleaseVideoCtx();
extern void RemoveMainLoopReleaseVideoCtx();
extern void InstallMainLoopCtor();
extern void RemoveMainLoopCtor();
extern void InstallMainLoopDtor();
extern void RemoveMainLoopDtor();

// --- UpdateLoop reimplementations (engine/reimpl/updateloop.cpp) ---
extern void InstallUpdateLoopIsRunning();
extern void RemoveUpdateLoopIsRunning();
extern void InstallUpdateLoopCtor();
extern void RemoveUpdateLoopCtor();
extern void InstallUpdateLoopDtor();
extern void RemoveUpdateLoopDtor();

// --- DialogLoaderLoopT reimplementations (engine/reimpl/dialogloaderloop.cpp) ---
extern void InstallDialogLoaderLoopStartup();
extern void RemoveDialogLoaderLoopStartup();
extern void InstallDialogLoaderLoopShutdown();
extern void RemoveDialogLoaderLoopShutdown();
extern void InstallDialogLoaderLoopUpdate();
extern void RemoveDialogLoaderLoopUpdate();
extern void InstallDialogLoaderLoopDraw();
extern void RemoveDialogLoaderLoopDraw();
extern void InstallDialogLoaderLoopLoad();
extern void RemoveDialogLoaderLoopLoad();

// --- DialogLoopT reimplementations (engine/reimpl/dialogloop.cpp) ---
extern void InstallDialogLoopStartup();
extern void RemoveDialogLoopStartup();
extern void InstallDialogLoopShutdown();
extern void RemoveDialogLoopShutdown();
extern void InstallDialogLoopUpdate();
extern void RemoveDialogLoopUpdate();
extern void InstallDialogLoopDraw();
extern void RemoveDialogLoopDraw();
extern void InstallDialogLoopLoad();
extern void RemoveDialogLoopLoad();

// --- PlatformConfigure (engine/reimpl/platformconfigure.cpp) ---
extern void InstallPlatformConfigure();
extern void RemovePlatformConfigure();

// --- Configuration reimplementations (engine/reimpl/configuration.cpp) ---
extern void InstallConfigCurrentBuildId();
extern void RemoveConfigCurrentBuildId();
extern void InstallConfigCurrentVersion();
extern void RemoveConfigCurrentVersion();
extern void InstallConfigGetVersionString();
extern void RemoveConfigGetVersionString();
extern void InstallConfigCtor();
extern void RemoveConfigCtor();
extern void InstallConfigDtor();
extern void RemoveConfigDtor();
extern void InstallConfigGetCommandLineString();
extern void RemoveConfigGetCommandLineString();
extern void InstallConfigSetCommandLine();
extern void RemoveConfigSetCommandLine();
extern void InstallConfigGetPlatformName();
extern void RemoveConfigGetPlatformName();
extern void InstallConfigGetLanguageName();
extern void RemoveConfigGetLanguageName();
extern void InstallConfigGetBuildName();
extern void RemoveConfigGetBuildName();

// --- Modification reimplementations (engine/reimpl/modification.cpp) ---
extern void InstallModificationFind();
extern void RemoveModificationFind();
extern void InstallModificationEnable();
extern void RemoveModificationEnable();
extern void InstallModificationDisableAll();
extern void RemoveModificationDisableAll();
extern void InstallModificationSwap();
extern void RemoveModificationSwap();
extern void InstallModificationEnableSavingSettings();
extern void RemoveModificationEnableSavingSettings();
extern void InstallModificationSaveList();
extern void RemoveModificationSaveList();
extern void InstallModificationRefreshList();
extern void RemoveModificationRefreshList();
extern void InstallModificationSetupFileSystem();
extern void RemoveModificationSetupFileSystem();

// --- LoaderLoop reimplementations (engine/reimpl/loaderloop.cpp) ---
extern void InstallLoaderLoopStartup();
extern void RemoveLoaderLoopStartup();
extern void InstallLoaderLoopShutdown();
extern void RemoveLoaderLoopShutdown();
extern void InstallLoaderLoopUpdate();
extern void RemoveLoaderLoopUpdate();
extern void InstallLoaderLoopDraw();
extern void RemoveLoaderLoopDraw();
extern void InstallLoaderLoopExit();
extern void RemoveLoaderLoopExit();
extern void InstallLoaderLoopIsFinished();
extern void RemoveLoaderLoopIsFinished();
extern void InstallLoaderLoopOnVideoError();
extern void RemoveLoaderLoopOnVideoError();

// --- LivestockComponent reimplementations (engine/reimpl/livestockcomponent.cpp) ---
extern void InstallLivestockComponentUpdate();
extern void RemoveLivestockComponentUpdate();

// --- StateMachineComponent reimplementations (engine/reimpl/statemachinecomponent.cpp) ---
extern void InstallStateMachineComponentUpdate();
extern void RemoveStateMachineComponentUpdate();

// --- HuntComponent reimplementations (engine/reimpl/huntcomponent.cpp) ---
extern void InstallHuntComponentUpdate();
extern void RemoveHuntComponentUpdate();

// --- DiseaseComponent reimplementations (engine/reimpl/diseasecomponent.cpp) ---
extern void InstallDiseaseComponentUpdate();
extern void RemoveDiseaseComponentUpdate();

// --- AttendanceComponent reimplementations (engine/reimpl/attendancecomponent.cpp) ---
extern void InstallAttendanceComponentUpdate();
extern void RemoveAttendanceComponentUpdate();

// --- AmbientEnvironmentComponent reimplementations (engine/reimpl/ambientenvironmentcomponent.cpp) ---
extern void InstallAmbientEnvironmentComponentUpdate();
extern void RemoveAmbientEnvironmentComponentUpdate();

// --- CharacterComponent reimplementations (engine/reimpl/charactercomponent.cpp) ---
extern void InstallCharacterComponentUpdate();
extern void RemoveCharacterComponentUpdate();

// --- PopulationComponent reimplementations (engine/reimpl/populationcomponent.cpp) ---
extern void InstallPopulationComponentUpdate();
extern void RemovePopulationComponentUpdate();

// --- FruitComponent reimplementations (engine/reimpl/fruitcomponent.cpp) ---
extern void InstallFruitComponentUpdate();
extern void RemoveFruitComponentUpdate();

// --- SpawnComponent reimplementations (engine/reimpl/spawncomponent.cpp) ---
extern void InstallSpawnComponentUpdate();
extern void RemoveSpawnComponentUpdate();

// --- FireComponent reimplementations (engine/reimpl/firecomponent.cpp) ---
extern void InstallFireComponentUpdate();
extern void RemoveFireComponentUpdate();

// --- GrowthComponent reimplementations (engine/reimpl/growth.cpp) ---
extern void InstallGrowthComponentUpdate();
extern void RemoveGrowthComponentUpdate();

// --- HerdComponent reimplementations (engine/reimpl/herdcomponent.cpp) ---
extern void InstallHerdComponentUpdate();
extern void RemoveHerdComponentUpdate();

// --- AnimalComponent reimplementations (engine/reimpl/animal.cpp) ---
extern void InstallAnimalComponentUpdate();
extern void RemoveAnimalComponentUpdate();

// --- CitizenAIComponent reimplementations (engine/reimpl/citizenaicomponent.cpp) ---
extern void InstallCitizenAIComponentUpdate();
extern void RemoveCitizenAIComponentUpdate();

// --- CitizenComponent reimplementations (engine/reimpl/citizencomponent.cpp) ---
extern void InstallCitizenComponentUpdate();
extern void RemoveCitizenComponentUpdate();

// ============================================================
// Phase 2: Passive component lifecycle methods
// ============================================================

// --- AbsorbResourceComponent (engine/reimpl/absorbresourcecomponent.cpp) ---
extern void InstallAbsorbResourceComponentShutdown();
extern void RemoveAbsorbResourceComponentShutdown();

// --- AmbientEmitterComponent (engine/reimpl/ambientemittercomponent.cpp) ---
extern void InstallAmbientEmitterComponentShutdown();
extern void RemoveAmbientEmitterComponentShutdown();
extern void InstallAmbientEmitterComponentDraw();
extern void RemoveAmbientEmitterComponentDraw();

// --- AutoPickupComponent (engine/reimpl/autopickupcomponent.cpp) ---
extern void InstallAutoPickupComponentShutdown();
extern void RemoveAutoPickupComponentShutdown();
extern void InstallAutoPickupComponentSerialize();
extern void RemoveAutoPickupComponentSerialize();

// --- BoatComponent (engine/reimpl/boatcomponent.cpp) ---
extern void InstallBoatComponentShutdown();
extern void RemoveBoatComponentShutdown();

// --- BuildComponent (engine/reimpl/buildcomponent.cpp) ---
extern void InstallBuildComponentShutdown();
extern void RemoveBuildComponentShutdown();
extern void InstallBuildComponentDraw();
extern void RemoveBuildComponentDraw();
extern void InstallBuildComponentOnCreate();
extern void RemoveBuildComponentOnCreate();

// --- CemeteryComponent (engine/reimpl/cemeterycomponent.cpp) ---
extern void InstallCemeteryComponentShutdown();
extern void RemoveCemeteryComponentShutdown();
extern void InstallCemeteryComponentDraw();
extern void RemoveCemeteryComponentDraw();
extern void InstallCemeteryComponentOnCreate();
extern void RemoveCemeteryComponentOnCreate();

// --- ClearAreaComponent (engine/reimpl/clearareacomponent.cpp) ---
extern void InstallClearAreaComponentShutdown();
extern void RemoveClearAreaComponentShutdown();

// --- ClearResourceComponent (engine/reimpl/clearresourcecomponent.cpp) ---
extern void InstallClearResourceComponentShutdown();
extern void RemoveClearResourceComponentShutdown();

// --- ConsumeProduceComponent (engine/reimpl/consumeproducecomponent.cpp) ---
extern void InstallConsumeProduceComponentShutdown();
extern void RemoveConsumeProduceComponentShutdown();
extern void InstallConsumeProduceComponentSerialize();
extern void RemoveConsumeProduceComponentSerialize();

// --- CropFieldComponent (engine/reimpl/cropfieldcomponent.cpp) ---
extern void InstallCropFieldComponentShutdown();
extern void RemoveCropFieldComponentShutdown();

// --- DecalComponent (engine/reimpl/decalcomponent.cpp) ---
extern void InstallDecalComponentShutdown();
extern void RemoveDecalComponentShutdown();
extern void InstallDecalComponentDraw();
extern void RemoveDecalComponentDraw();
extern void InstallDecalComponentOnCreate();
extern void RemoveDecalComponentOnCreate();
extern void InstallDecalComponentSerialize();
extern void RemoveDecalComponentSerialize();

// --- DestroyComponent (engine/reimpl/destroycomponent.cpp) ---
extern void InstallDestroyComponentShutdown();
extern void RemoveDestroyComponentShutdown();
extern void InstallDestroyComponentSerialize();
extern void RemoveDestroyComponentSerialize();

// --- FishingComponent (engine/reimpl/fishingcomponent.cpp) ---
extern void InstallFishingComponentShutdown();
extern void RemoveFishingComponentShutdown();

// --- FloorComponent (engine/reimpl/floorcomponent.cpp) ---
extern void InstallFloorComponentShutdown();
extern void RemoveFloorComponentShutdown();
extern void InstallFloorComponentDraw();
extern void RemoveFloorComponentDraw();
extern void InstallFloorComponentOnCreate();
extern void RemoveFloorComponentOnCreate();

// --- ForesterComponent (engine/reimpl/forestercomponent.cpp) ---
extern void InstallForesterComponentShutdown();
extern void RemoveForesterComponentShutdown();

// --- HappinessComponent (engine/reimpl/happinesscomponent.cpp) ---
extern void InstallHappinessComponentShutdown();
extern void RemoveHappinessComponentShutdown();
extern void InstallHappinessComponentOnCreate();
extern void RemoveHappinessComponentOnCreate();

// --- HealerComponent (engine/reimpl/healercomponent.cpp) ---
extern void InstallHealerComponentShutdown();
extern void RemoveHealerComponentShutdown();
extern void InstallHealerComponentOnCreate();
extern void RemoveHealerComponentOnCreate();

// --- HerbalistComponent (engine/reimpl/herbalistcomponent.cpp) ---
extern void InstallHerbalistComponentShutdown();
extern void RemoveHerbalistComponentShutdown();

// --- HighlightComponent (engine/reimpl/highlightcomponent.cpp) ---
extern void InstallHighlightComponentShutdown();
extern void RemoveHighlightComponentShutdown();

// --- ImmigrationComponent (engine/reimpl/immigrationcomponent.cpp) ---
extern void InstallImmigrationComponentShutdown();
extern void RemoveImmigrationComponentShutdown();

// --- InteractComponent (engine/reimpl/interactcomponent.cpp) ---
extern void InstallInteractComponentShutdown();
extern void RemoveInteractComponentShutdown();
extern void InstallInteractComponentDraw();
extern void RemoveInteractComponentDraw();

// --- MapComponent (engine/reimpl/mapcomponent.cpp) ---
extern void InstallMapComponentShutdown();
extern void RemoveMapComponentShutdown();
extern void InstallMapComponentDraw();
extern void RemoveMapComponentDraw();
extern void InstallMapComponentOnCreate();
extern void RemoveMapComponentOnCreate();

// --- MarketComponent (engine/reimpl/marketcomponent.cpp) ---
extern void InstallMarketComponentShutdown();
extern void RemoveMarketComponentShutdown();

// --- MarketLocationComponent (engine/reimpl/marketlocationcomponent.cpp) ---
extern void InstallMarketLocationComponentShutdown();
extern void RemoveMarketLocationComponentShutdown();
extern void InstallMarketLocationComponentOnCreate();
extern void RemoveMarketLocationComponentOnCreate();

// --- MineComponent (engine/reimpl/minecomponent.cpp) ---
extern void InstallMineComponentShutdown();
extern void RemoveMineComponentShutdown();

// --- ModelComponent (engine/reimpl/modelcomponent.cpp) ---
extern void InstallModelComponentShutdown();
extern void RemoveModelComponentShutdown();
extern void InstallModelComponentDraw();
extern void RemoveModelComponentDraw();
extern void InstallModelComponentOnCreate();
extern void RemoveModelComponentOnCreate();
extern void InstallModelComponentOnRemove();
extern void RemoveModelComponentOnRemove();
extern void InstallModelComponentSerialize();
extern void RemoveModelComponentSerialize();

// --- MultiObjectComponent (engine/reimpl/multiobjectcomponent.cpp) ---
extern void InstallMultiObjectComponentShutdown();
extern void RemoveMultiObjectComponentShutdown();
extern void InstallMultiObjectComponentOnCreate();
extern void RemoveMultiObjectComponentOnCreate();

// --- NaturalResourceComponent (engine/reimpl/naturalresourcecomponent.cpp) ---
extern void InstallNaturalResourceComponentShutdown();
extern void RemoveNaturalResourceComponentShutdown();
extern void InstallNaturalResourceComponentOnRemove();
extern void RemoveNaturalResourceComponentOnRemove();

// --- NomadComponent (engine/reimpl/nomadcomponent.cpp) ---
extern void InstallNomadComponentShutdown();
extern void RemoveNomadComponentShutdown();
extern void InstallNomadComponentOnCreate();
extern void RemoveNomadComponentOnCreate();

// --- OrchardComponent (engine/reimpl/orchardcomponent.cpp) ---
extern void InstallOrchardComponentShutdown();
extern void RemoveOrchardComponentShutdown();

// --- ParticleComponent (engine/reimpl/particlecomponent.cpp) ---
extern void InstallParticleComponentShutdown();
extern void RemoveParticleComponentShutdown();
extern void InstallParticleComponentDraw();
extern void RemoveParticleComponentDraw();
extern void InstallParticleComponentOnCreate();
extern void RemoveParticleComponentOnCreate();
extern void InstallParticleComponentOnRemove();
extern void RemoveParticleComponentOnRemove();
extern void InstallParticleComponentSerialize();
extern void RemoveParticleComponentSerialize();
extern void InstallParticleComponentOnUpdate2();
extern void RemoveParticleComponentOnUpdate2();

// --- PastureComponent (engine/reimpl/pasturecomponent.cpp) ---
extern void InstallPastureComponentShutdown();
extern void RemovePastureComponentShutdown();
extern void InstallPastureComponentDraw();
extern void RemovePastureComponentDraw();
extern void InstallPastureComponentOnCreate();
extern void RemovePastureComponentOnCreate();

// --- PickingComponent (engine/reimpl/pickingcomponent.cpp) ---
extern void InstallPickingComponentShutdown();
extern void RemovePickingComponentShutdown();
extern void InstallPickingComponentOnCreate();
extern void RemovePickingComponentOnCreate();

// --- QuarryComponent (engine/reimpl/quarrycomponent.cpp) ---
extern void InstallQuarryComponentShutdown();
extern void RemoveQuarryComponentShutdown();

// --- RawMaterialComponent (engine/reimpl/rawmaterialcomponent.cpp) ---
extern void InstallRawMaterialComponentShutdown();
extern void RemoveRawMaterialComponentShutdown();
extern void InstallRawMaterialComponentDraw();
extern void RemoveRawMaterialComponentDraw();
extern void InstallRawMaterialComponentSerialize();
extern void RemoveRawMaterialComponentSerialize();
extern void InstallRawMaterialComponentUpdate();
extern void RemoveRawMaterialComponentUpdate();

// --- ResidenceComponent (engine/reimpl/residencecomponent.cpp) ---
extern void InstallResidenceComponentShutdown();
extern void RemoveResidenceComponentShutdown();
extern void InstallResidenceComponentSerialize();
extern void RemoveResidenceComponentSerialize();
extern void InstallResidenceComponentOnCreate();
extern void RemoveResidenceComponentOnCreate();

// --- StatusIconComponent (engine/reimpl/statusiconcomponent.cpp) ---
extern void InstallStatusIconComponentShutdown();
extern void RemoveStatusIconComponentShutdown();
extern void InstallStatusIconComponentSerialize();
extern void RemoveStatusIconComponentSerialize();
extern void InstallStatusIconComponentOnRemove();
extern void RemoveStatusIconComponentOnRemove();
extern void InstallStatusIconComponentOnUpdate2();
extern void RemoveStatusIconComponentOnUpdate2();

// --- StorageComponent (engine/reimpl/storagecomponent.cpp) ---
extern void InstallStorageComponentShutdown();
extern void RemoveStorageComponentShutdown();
extern void InstallStorageComponentDraw();
extern void RemoveStorageComponentDraw();

// --- StorageDisplayComponent (engine/reimpl/storagedisplaycomponent.cpp) ---
extern void InstallStorageDisplayComponentShutdown();
extern void RemoveStorageDisplayComponentShutdown();
extern void InstallStorageDisplayComponentDraw();
extern void RemoveStorageDisplayComponentDraw();
extern void InstallStorageDisplayComponentOnCreate();
extern void RemoveStorageDisplayComponentOnCreate();
extern void InstallStorageDisplayComponentUpdate();
extern void RemoveStorageDisplayComponentUpdate();
extern void InstallStorageDisplayComponentOnUpdate2();
extern void RemoveStorageDisplayComponentOnUpdate2();

// --- StorageLocationComponent (engine/reimpl/storagelocationcomponent.cpp) ---
extern void InstallStorageLocationComponentShutdown();
extern void RemoveStorageLocationComponentShutdown();
extern void InstallStorageLocationComponentOnCreate();
extern void RemoveStorageLocationComponentOnCreate();

// --- TerraformComponent (engine/reimpl/terraformcomponent.cpp) ---
extern void InstallTerraformComponentShutdown();
extern void RemoveTerraformComponentShutdown();
extern void InstallTerraformComponentOnCreate();
extern void RemoveTerraformComponentOnCreate();

// --- TombstoneComponent (engine/reimpl/tombstonecomponent.cpp) ---
extern void InstallTombstoneComponentShutdown();
extern void RemoveTombstoneComponentShutdown();
extern void InstallTombstoneComponentSerialize();
extern void RemoveTombstoneComponentSerialize();
extern void InstallTombstoneComponentUpdate();
extern void RemoveTombstoneComponentUpdate();

// --- TornadoComponent (engine/reimpl/tornadocomponent.cpp) ---
extern void InstallTornadoComponentShutdown();
extern void RemoveTornadoComponentShutdown();
extern void InstallTornadoComponentSerialize();
extern void RemoveTornadoComponentSerialize();
extern void InstallTornadoComponentOnRemove();
extern void RemoveTornadoComponentOnRemove();

// --- TrackerComponent (engine/reimpl/trackercomponent.cpp) ---
extern void InstallTrackerComponentShutdown();
extern void RemoveTrackerComponentShutdown();

// --- TradeComponent (engine/reimpl/tradecomponent.cpp) ---
extern void InstallTradeComponentShutdown();
extern void RemoveTradeComponentShutdown();
extern void InstallTradeComponentDraw();
extern void RemoveTradeComponentDraw();
extern void InstallTradeComponentOnCreate();
extern void RemoveTradeComponentOnCreate();

// --- UIComponent (engine/reimpl/uicomponent.cpp) ---
extern void InstallUIComponentShutdown();
extern void RemoveUIComponentShutdown();
extern void InstallUIComponentSerialize();
extern void RemoveUIComponentSerialize();

// --- WellComponent (engine/reimpl/wellcomponent.cpp) ---
extern void InstallWellComponentShutdown();
extern void RemoveWellComponentShutdown();
extern void InstallWellComponentOnCreate();
extern void RemoveWellComponentOnCreate();

// --- WildAnimalComponent (engine/reimpl/wildanimalcomponent.cpp) ---
extern void InstallWildAnimalComponentShutdown();
extern void RemoveWildAnimalComponentShutdown();
extern void InstallWildAnimalComponentOnCreate();
extern void RemoveWildAnimalComponentOnCreate();

// --- WorkComponent (engine/reimpl/workcomponent.cpp) ---
extern void InstallWorkComponentShutdown();
extern void RemoveWorkComponentShutdown();
extern void InstallWorkComponentDraw();
extern void RemoveWorkComponentDraw();
extern void InstallWorkComponentOnCreate();
extern void RemoveWorkComponentOnCreate();

// --- WorkPlaceComponent (engine/reimpl/workplacecomponent.cpp) ---
extern void InstallWorkPlaceComponentShutdown();
extern void RemoveWorkPlaceComponentShutdown();
extern void InstallWorkPlaceComponentOnCreate();
extern void RemoveWorkPlaceComponentOnCreate();

// --- ZoneComponent (engine/reimpl/zonecomponent.cpp) ---
extern void InstallZoneComponentShutdown();
extern void RemoveZoneComponentShutdown();
extern void InstallZoneComponentDraw();
extern void RemoveZoneComponentDraw();
extern void InstallZoneComponentOnCreate();
extern void RemoveZoneComponentOnCreate();

static ReimplEntry g_reimplementations[] = {
    // --- Data patches ---
    { "BuildNumber",           InstallBuildPatch,          RemoveBuildPatch,          true  },

    // --- Launcher skip ---
    { "PlatformConfigure",     InstallPlatformConfigure,   RemovePlatformConfigure,   true  },

    // --- MainLoop reimplementations ---
    { "MainLoop::Frame",       InstallMainLoopFrame,       RemoveMainLoopFrame,       true  },
    { "MainLoop::Update",      InstallMainLoopUpdate,      RemoveMainLoopUpdate,      true  },
    { "MainLoop::Draw",        InstallMainLoopDraw,        RemoveMainLoopDraw,        true  },
    { "MainLoop::SetNextLoop", InstallMainLoopSetNextLoop, RemoveMainLoopSetNextLoop, true  },
    { "MainLoop::Run",         InstallMainLoopRun,         RemoveMainLoopRun,         true  },
    { "MainLoop::IsRunning",   InstallMainLoopIsRunningML, RemoveMainLoopIsRunningML, true  },
    { "MainLoop::MarkFrameUpdateTime", InstallMainLoopMarkFrameUpdateTime, RemoveMainLoopMarkFrameUpdateTime, true },
    { "MainLoop::AddPostUpdate",    InstallMainLoopAddPostUpdate,    RemoveMainLoopAddPostUpdate,    true  },
    { "MainLoop::AddPostDraw",      InstallMainLoopAddPostDraw,      RemoveMainLoopAddPostDraw,      true  },
    { "MainLoop::RemovePostUpdate", InstallMainLoopRemovePostUpdate, RemoveMainLoopRemovePostUpdate, true  },
    { "MainLoop::RemovePostDraw",   InstallMainLoopRemovePostDraw,   RemoveMainLoopRemovePostDraw,   true  },
    { "MainLoop::OnVideoError_s",   InstallMainLoopOnVideoErrorStatic,   RemoveMainLoopOnVideoErrorStatic,   true },
    { "MainLoop::OnVideoRestart_s", InstallMainLoopOnVideoRestartStatic, RemoveMainLoopOnVideoRestartStatic, true },
    { "MainLoop::CreateVideoCtx",   InstallMainLoopCreateVideoCtx,   RemoveMainLoopCreateVideoCtx,   true  },
    { "MainLoop::ReleaseVideoCtx",  InstallMainLoopReleaseVideoCtx,  RemoveMainLoopReleaseVideoCtx,  true  },
    { "MainLoop::Ctor",             InstallMainLoopCtor,             RemoveMainLoopCtor,             true  },
    { "MainLoop::Dtor",             InstallMainLoopDtor,             RemoveMainLoopDtor,             true  },

    // --- UpdateLoop reimplementations ---
    { "UpdateLoop::IsRunning", InstallUpdateLoopIsRunning, RemoveUpdateLoopIsRunning, true },
    { "UpdateLoop::Ctor",      InstallUpdateLoopCtor,      RemoveUpdateLoopCtor,      true },
    { "UpdateLoop::Dtor",      InstallUpdateLoopDtor,      RemoveUpdateLoopDtor,      true },

    // --- DialogLoaderLoopT reimplementations ---
    { "DialogLoaderLoopT::Startup",  InstallDialogLoaderLoopStartup,  RemoveDialogLoaderLoopStartup,  true  },
    { "DialogLoaderLoopT::Shutdown", InstallDialogLoaderLoopShutdown, RemoveDialogLoaderLoopShutdown, true  },
    { "DialogLoaderLoopT::Update",   InstallDialogLoaderLoopUpdate,   RemoveDialogLoaderLoopUpdate,   true  },
    { "DialogLoaderLoopT::Draw",     InstallDialogLoaderLoopDraw,     RemoveDialogLoaderLoopDraw,     true  },
    { "DialogLoaderLoopT::Load",     InstallDialogLoaderLoopLoad,     RemoveDialogLoaderLoopLoad,     true  },

    // --- DialogLoopT reimplementations ---
    { "DialogLoopT::Startup",  InstallDialogLoopStartup,  RemoveDialogLoopStartup,  true  },
    { "DialogLoopT::Shutdown", InstallDialogLoopShutdown, RemoveDialogLoopShutdown, true  },
    { "DialogLoopT::Update",   InstallDialogLoopUpdate,   RemoveDialogLoopUpdate,   true  },
    { "DialogLoopT::Draw",     InstallDialogLoopDraw,     RemoveDialogLoopDraw,     true  },
    { "DialogLoopT::Load",     InstallDialogLoopLoad,     RemoveDialogLoopLoad,     true  },

    // --- LoaderLoop reimplementations ---
    { "LoaderLoop::Startup",      InstallLoaderLoopStartup,      RemoveLoaderLoopStartup,      true  },
    { "LoaderLoop::Shutdown",     InstallLoaderLoopShutdown,      RemoveLoaderLoopShutdown,      true  },
    { "LoaderLoop::Update",       InstallLoaderLoopUpdate,        RemoveLoaderLoopUpdate,        true  },
    { "LoaderLoop::Draw",         InstallLoaderLoopDraw,          RemoveLoaderLoopDraw,          true  },
    { "LoaderLoop::Exit",         InstallLoaderLoopExit,          RemoveLoaderLoopExit,          true  },
    { "LoaderLoop::IsFinished",   InstallLoaderLoopIsFinished,    RemoveLoaderLoopIsFinished,    true  },
    { "LoaderLoop::OnVideoError", InstallLoaderLoopOnVideoError,  RemoveLoaderLoopOnVideoError,  true  },

    // --- Configuration reimplementations ---
    { "Configuration::CurrentBuildId",   InstallConfigCurrentBuildId,   RemoveConfigCurrentBuildId,   true  },
    { "Configuration::CurrentVersion",   InstallConfigCurrentVersion,   RemoveConfigCurrentVersion,   true  },
    { "Configuration::GetVersionString", InstallConfigGetVersionString, RemoveConfigGetVersionString, true  },
    { "Configuration::Ctor",             InstallConfigCtor,             RemoveConfigCtor,             true  },
    { "Configuration::Dtor",             InstallConfigDtor,             RemoveConfigDtor,             true  },
    { "Configuration::GetCommandLineString", InstallConfigGetCommandLineString, RemoveConfigGetCommandLineString, true },
    { "Configuration::SetCommandLine",   InstallConfigSetCommandLine,   RemoveConfigSetCommandLine,   true  },
    { "Configuration::GetPlatformName",  InstallConfigGetPlatformName,  RemoveConfigGetPlatformName,  true  },
    { "Configuration::GetLanguageName",  InstallConfigGetLanguageName,  RemoveConfigGetLanguageName,  true  },
    { "Configuration::GetBuildName",     InstallConfigGetBuildName,     RemoveConfigGetBuildName,     true  },

    // --- Modification reimplementations ---
    { "Modification::Find",       InstallModificationFind,       RemoveModificationFind,       true  },
    { "Modification::Enable",     InstallModificationEnable,     RemoveModificationEnable,     true  },
    { "Modification::DisableAll", InstallModificationDisableAll, RemoveModificationDisableAll, true  },
    { "Modification::Swap",       InstallModificationSwap,       RemoveModificationSwap,       true  },
    { "Modification::EnableSavingSettings", InstallModificationEnableSavingSettings, RemoveModificationEnableSavingSettings, true },
    { "Modification::SaveList",        InstallModificationSaveList,        RemoveModificationSaveList,        true },
    { "Modification::RefreshList",     InstallModificationRefreshList,     RemoveModificationRefreshList,     true },
    { "Modification::SetupFileSystem", InstallModificationSetupFileSystem, RemoveModificationSetupFileSystem, true },

    // --- LivestockComponent reimplementations ---
    { "LivestockComponent::Update", InstallLivestockComponentUpdate, RemoveLivestockComponentUpdate, true },

    // --- StateMachineComponent reimplementations ---
    { "StateMachineComponent::Update", InstallStateMachineComponentUpdate, RemoveStateMachineComponentUpdate, true },

    // --- HuntComponent reimplementations ---
    { "HuntComponent::Update", InstallHuntComponentUpdate, RemoveHuntComponentUpdate, true },

    // --- DiseaseComponent reimplementations ---
    { "DiseaseComponent::Update", InstallDiseaseComponentUpdate, RemoveDiseaseComponentUpdate, true },

    // --- AttendanceComponent reimplementations ---
    { "AttendanceComponent::Update", InstallAttendanceComponentUpdate, RemoveAttendanceComponentUpdate, true },

    // --- AmbientEnvironmentComponent reimplementations ---
    { "AmbientEnvironmentComponent::Update", InstallAmbientEnvironmentComponentUpdate, RemoveAmbientEnvironmentComponentUpdate, true },

    // --- CharacterComponent reimplementations ---
    { "CharacterComponent::Update", InstallCharacterComponentUpdate, RemoveCharacterComponentUpdate, true },

    // --- PopulationComponent reimplementations ---
    { "PopulationComponent::Update", InstallPopulationComponentUpdate, RemovePopulationComponentUpdate, true },

    // --- FruitComponent reimplementations ---
    { "FruitComponent::Update", InstallFruitComponentUpdate, RemoveFruitComponentUpdate, true },

    // --- SpawnComponent reimplementations ---
    { "SpawnComponent::Update", InstallSpawnComponentUpdate, RemoveSpawnComponentUpdate, true },

    // --- FireComponent reimplementations ---
    { "FireComponent::Update", InstallFireComponentUpdate, RemoveFireComponentUpdate, true },

    // --- GrowthComponent reimplementations ---
    { "GrowthComponent::Update", InstallGrowthComponentUpdate, RemoveGrowthComponentUpdate, true },

    // --- HerdComponent reimplementations ---
    { "HerdComponent::Update", InstallHerdComponentUpdate, RemoveHerdComponentUpdate, true },

    // --- AnimalComponent reimplementations ---
    { "AnimalComponent::Update", InstallAnimalComponentUpdate, RemoveAnimalComponentUpdate, true },

    // --- CitizenAIComponent reimplementations ---
    { "CitizenAIComponent::Update", InstallCitizenAIComponentUpdate, RemoveCitizenAIComponentUpdate, true },

    // --- CitizenComponent reimplementations ---
    { "CitizenComponent::Update", InstallCitizenComponentUpdate, RemoveCitizenComponentUpdate, true },

    // ============================================================
    // Phase 2: Passive component lifecycle methods
    // ============================================================

    // --- AbsorbResourceComponent ---
    { "AbsorbResourceComponent::Shutdown", InstallAbsorbResourceComponentShutdown, RemoveAbsorbResourceComponentShutdown, true },

    // --- AmbientEmitterComponent ---
    { "AmbientEmitterComponent::Shutdown", InstallAmbientEmitterComponentShutdown, RemoveAmbientEmitterComponentShutdown, true },
    { "AmbientEmitterComponent::Draw", InstallAmbientEmitterComponentDraw, RemoveAmbientEmitterComponentDraw, true },

    // --- AutoPickupComponent ---
    { "AutoPickupComponent::Shutdown", InstallAutoPickupComponentShutdown, RemoveAutoPickupComponentShutdown, true },
    { "AutoPickupComponent::Serialize", InstallAutoPickupComponentSerialize, RemoveAutoPickupComponentSerialize, true },

    // --- BoatComponent ---
    { "BoatComponent::Shutdown", InstallBoatComponentShutdown, RemoveBoatComponentShutdown, true },

    // --- BuildComponent ---
    { "BuildComponent::Shutdown", InstallBuildComponentShutdown, RemoveBuildComponentShutdown, true },
    { "BuildComponent::Draw", InstallBuildComponentDraw, RemoveBuildComponentDraw, true },
    { "BuildComponent::OnCreate", InstallBuildComponentOnCreate, RemoveBuildComponentOnCreate, true },

    // --- CemeteryComponent ---
    { "CemeteryComponent::Shutdown", InstallCemeteryComponentShutdown, RemoveCemeteryComponentShutdown, true },
    { "CemeteryComponent::Draw", InstallCemeteryComponentDraw, RemoveCemeteryComponentDraw, true },
    { "CemeteryComponent::OnCreate", InstallCemeteryComponentOnCreate, RemoveCemeteryComponentOnCreate, true },

    // --- ClearAreaComponent ---
    { "ClearAreaComponent::Shutdown", InstallClearAreaComponentShutdown, RemoveClearAreaComponentShutdown, true },

    // --- ClearResourceComponent ---
    { "ClearResourceComponent::Shutdown", InstallClearResourceComponentShutdown, RemoveClearResourceComponentShutdown, true },

    // --- ConsumeProduceComponent ---
    { "ConsumeProduceComponent::Shutdown", InstallConsumeProduceComponentShutdown, RemoveConsumeProduceComponentShutdown, true },
    { "ConsumeProduceComponent::Serialize", InstallConsumeProduceComponentSerialize, RemoveConsumeProduceComponentSerialize, true },

    // --- CropFieldComponent ---
    { "CropFieldComponent::Shutdown", InstallCropFieldComponentShutdown, RemoveCropFieldComponentShutdown, true },

    // --- DecalComponent ---
    { "DecalComponent::Shutdown", InstallDecalComponentShutdown, RemoveDecalComponentShutdown, true },
    { "DecalComponent::Draw", InstallDecalComponentDraw, RemoveDecalComponentDraw, true },
    { "DecalComponent::OnCreate", InstallDecalComponentOnCreate, RemoveDecalComponentOnCreate, true },
    { "DecalComponent::Serialize", InstallDecalComponentSerialize, RemoveDecalComponentSerialize, true },

    // --- DestroyComponent ---
    { "DestroyComponent::Shutdown", InstallDestroyComponentShutdown, RemoveDestroyComponentShutdown, true },
    { "DestroyComponent::Serialize", InstallDestroyComponentSerialize, RemoveDestroyComponentSerialize, true },

    // --- FishingComponent ---
    { "FishingComponent::Shutdown", InstallFishingComponentShutdown, RemoveFishingComponentShutdown, true },

    // --- FloorComponent ---
    { "FloorComponent::Shutdown", InstallFloorComponentShutdown, RemoveFloorComponentShutdown, true },
    { "FloorComponent::Draw", InstallFloorComponentDraw, RemoveFloorComponentDraw, true },
    { "FloorComponent::OnCreate", InstallFloorComponentOnCreate, RemoveFloorComponentOnCreate, true },

    // --- ForesterComponent ---
    { "ForesterComponent::Shutdown", InstallForesterComponentShutdown, RemoveForesterComponentShutdown, true },

    // --- HappinessComponent ---
    { "HappinessComponent::Shutdown", InstallHappinessComponentShutdown, RemoveHappinessComponentShutdown, true },
    { "HappinessComponent::OnCreate", InstallHappinessComponentOnCreate, RemoveHappinessComponentOnCreate, true },

    // --- HealerComponent ---
    { "HealerComponent::Shutdown", InstallHealerComponentShutdown, RemoveHealerComponentShutdown, true },
    { "HealerComponent::OnCreate", InstallHealerComponentOnCreate, RemoveHealerComponentOnCreate, true },

    // --- HerbalistComponent ---
    { "HerbalistComponent::Shutdown", InstallHerbalistComponentShutdown, RemoveHerbalistComponentShutdown, true },

    // --- HighlightComponent ---
    { "HighlightComponent::Shutdown", InstallHighlightComponentShutdown, RemoveHighlightComponentShutdown, true },

    // --- ImmigrationComponent ---
    { "ImmigrationComponent::Shutdown", InstallImmigrationComponentShutdown, RemoveImmigrationComponentShutdown, true },

    // --- InteractComponent ---
    { "InteractComponent::Shutdown", InstallInteractComponentShutdown, RemoveInteractComponentShutdown, true },
    { "InteractComponent::Draw", InstallInteractComponentDraw, RemoveInteractComponentDraw, true },

    // --- MapComponent ---
    { "MapComponent::Shutdown", InstallMapComponentShutdown, RemoveMapComponentShutdown, true },
    { "MapComponent::Draw", InstallMapComponentDraw, RemoveMapComponentDraw, true },
    { "MapComponent::OnCreate", InstallMapComponentOnCreate, RemoveMapComponentOnCreate, true },

    // --- MarketComponent ---
    { "MarketComponent::Shutdown", InstallMarketComponentShutdown, RemoveMarketComponentShutdown, true },

    // --- MarketLocationComponent ---
    { "MarketLocationComponent::Shutdown", InstallMarketLocationComponentShutdown, RemoveMarketLocationComponentShutdown, true },
    { "MarketLocationComponent::OnCreate", InstallMarketLocationComponentOnCreate, RemoveMarketLocationComponentOnCreate, true },

    // --- MineComponent ---
    { "MineComponent::Shutdown", InstallMineComponentShutdown, RemoveMineComponentShutdown, true },

    // --- ModelComponent ---
    { "ModelComponent::Shutdown", InstallModelComponentShutdown, RemoveModelComponentShutdown, true },
    { "ModelComponent::Draw", InstallModelComponentDraw, RemoveModelComponentDraw, true },
    { "ModelComponent::OnCreate", InstallModelComponentOnCreate, RemoveModelComponentOnCreate, true },
    { "ModelComponent::OnRemove", InstallModelComponentOnRemove, RemoveModelComponentOnRemove, true },
    { "ModelComponent::Serialize", InstallModelComponentSerialize, RemoveModelComponentSerialize, true },

    // --- MultiObjectComponent ---
    { "MultiObjectComponent::Shutdown", InstallMultiObjectComponentShutdown, RemoveMultiObjectComponentShutdown, true },
    { "MultiObjectComponent::OnCreate", InstallMultiObjectComponentOnCreate, RemoveMultiObjectComponentOnCreate, true },

    // --- NaturalResourceComponent ---
    { "NaturalResourceComponent::Shutdown", InstallNaturalResourceComponentShutdown, RemoveNaturalResourceComponentShutdown, true },
    { "NaturalResourceComponent::OnRemove", InstallNaturalResourceComponentOnRemove, RemoveNaturalResourceComponentOnRemove, true },

    // --- NomadComponent ---
    { "NomadComponent::Shutdown", InstallNomadComponentShutdown, RemoveNomadComponentShutdown, true },
    { "NomadComponent::OnCreate", InstallNomadComponentOnCreate, RemoveNomadComponentOnCreate, true },

    // --- OrchardComponent ---
    { "OrchardComponent::Shutdown", InstallOrchardComponentShutdown, RemoveOrchardComponentShutdown, true },

    // --- ParticleComponent ---
    { "ParticleComponent::Shutdown", InstallParticleComponentShutdown, RemoveParticleComponentShutdown, true },
    { "ParticleComponent::Draw", InstallParticleComponentDraw, RemoveParticleComponentDraw, true },
    { "ParticleComponent::OnCreate", InstallParticleComponentOnCreate, RemoveParticleComponentOnCreate, true },
    { "ParticleComponent::OnRemove", InstallParticleComponentOnRemove, RemoveParticleComponentOnRemove, true },
    { "ParticleComponent::Serialize", InstallParticleComponentSerialize, RemoveParticleComponentSerialize, true },
    { "ParticleComponent::OnUpdate2", InstallParticleComponentOnUpdate2, RemoveParticleComponentOnUpdate2, true },

    // --- PastureComponent ---
    { "PastureComponent::Shutdown", InstallPastureComponentShutdown, RemovePastureComponentShutdown, true },
    { "PastureComponent::Draw", InstallPastureComponentDraw, RemovePastureComponentDraw, true },
    { "PastureComponent::OnCreate", InstallPastureComponentOnCreate, RemovePastureComponentOnCreate, true },

    // --- PickingComponent ---
    { "PickingComponent::Shutdown", InstallPickingComponentShutdown, RemovePickingComponentShutdown, true },
    { "PickingComponent::OnCreate", InstallPickingComponentOnCreate, RemovePickingComponentOnCreate, true },

    // --- QuarryComponent ---
    { "QuarryComponent::Shutdown", InstallQuarryComponentShutdown, RemoveQuarryComponentShutdown, true },

    // --- RawMaterialComponent ---
    { "RawMaterialComponent::Shutdown", InstallRawMaterialComponentShutdown, RemoveRawMaterialComponentShutdown, true },
    { "RawMaterialComponent::Draw", InstallRawMaterialComponentDraw, RemoveRawMaterialComponentDraw, true },
    { "RawMaterialComponent::Serialize", InstallRawMaterialComponentSerialize, RemoveRawMaterialComponentSerialize, true },
    { "RawMaterialComponent::Update", InstallRawMaterialComponentUpdate, RemoveRawMaterialComponentUpdate, true },

    // --- ResidenceComponent ---
    { "ResidenceComponent::Shutdown", InstallResidenceComponentShutdown, RemoveResidenceComponentShutdown, true },
    { "ResidenceComponent::Serialize", InstallResidenceComponentSerialize, RemoveResidenceComponentSerialize, true },
    { "ResidenceComponent::OnCreate", InstallResidenceComponentOnCreate, RemoveResidenceComponentOnCreate, true },

    // --- StatusIconComponent ---
    { "StatusIconComponent::Shutdown", InstallStatusIconComponentShutdown, RemoveStatusIconComponentShutdown, true },
    { "StatusIconComponent::Serialize", InstallStatusIconComponentSerialize, RemoveStatusIconComponentSerialize, true },
    { "StatusIconComponent::OnRemove", InstallStatusIconComponentOnRemove, RemoveStatusIconComponentOnRemove, true },
    { "StatusIconComponent::OnUpdate2", InstallStatusIconComponentOnUpdate2, RemoveStatusIconComponentOnUpdate2, true },

    // --- StorageComponent ---
    { "StorageComponent::Shutdown", InstallStorageComponentShutdown, RemoveStorageComponentShutdown, true },
    { "StorageComponent::Draw", InstallStorageComponentDraw, RemoveStorageComponentDraw, true },

    // --- StorageDisplayComponent ---
    { "StorageDisplayComponent::Shutdown", InstallStorageDisplayComponentShutdown, RemoveStorageDisplayComponentShutdown, true },
    { "StorageDisplayComponent::Draw", InstallStorageDisplayComponentDraw, RemoveStorageDisplayComponentDraw, true },
    { "StorageDisplayComponent::OnCreate", InstallStorageDisplayComponentOnCreate, RemoveStorageDisplayComponentOnCreate, true },
    { "StorageDisplayComponent::Update", InstallStorageDisplayComponentUpdate, RemoveStorageDisplayComponentUpdate, true },
    { "StorageDisplayComponent::OnUpdate2", InstallStorageDisplayComponentOnUpdate2, RemoveStorageDisplayComponentOnUpdate2, true },

    // --- StorageLocationComponent ---
    { "StorageLocationComponent::Shutdown", InstallStorageLocationComponentShutdown, RemoveStorageLocationComponentShutdown, true },
    { "StorageLocationComponent::OnCreate", InstallStorageLocationComponentOnCreate, RemoveStorageLocationComponentOnCreate, true },

    // --- TerraformComponent ---
    { "TerraformComponent::Shutdown", InstallTerraformComponentShutdown, RemoveTerraformComponentShutdown, true },
    { "TerraformComponent::OnCreate", InstallTerraformComponentOnCreate, RemoveTerraformComponentOnCreate, true },

    // --- TombstoneComponent ---
    { "TombstoneComponent::Shutdown", InstallTombstoneComponentShutdown, RemoveTombstoneComponentShutdown, true },
    { "TombstoneComponent::Serialize", InstallTombstoneComponentSerialize, RemoveTombstoneComponentSerialize, true },
    { "TombstoneComponent::Update", InstallTombstoneComponentUpdate, RemoveTombstoneComponentUpdate, true },

    // --- TornadoComponent ---
    { "TornadoComponent::Shutdown", InstallTornadoComponentShutdown, RemoveTornadoComponentShutdown, true },
    { "TornadoComponent::Serialize", InstallTornadoComponentSerialize, RemoveTornadoComponentSerialize, true },
    { "TornadoComponent::OnRemove", InstallTornadoComponentOnRemove, RemoveTornadoComponentOnRemove, true },

    // --- TrackerComponent ---
    { "TrackerComponent::Shutdown", InstallTrackerComponentShutdown, RemoveTrackerComponentShutdown, true },

    // --- TradeComponent ---
    { "TradeComponent::Shutdown", InstallTradeComponentShutdown, RemoveTradeComponentShutdown, true },
    { "TradeComponent::Draw", InstallTradeComponentDraw, RemoveTradeComponentDraw, true },
    { "TradeComponent::OnCreate", InstallTradeComponentOnCreate, RemoveTradeComponentOnCreate, true },

    // --- UIComponent ---
    { "UIComponent::Shutdown", InstallUIComponentShutdown, RemoveUIComponentShutdown, true },
    { "UIComponent::Serialize", InstallUIComponentSerialize, RemoveUIComponentSerialize, true },

    // --- WellComponent ---
    { "WellComponent::Shutdown", InstallWellComponentShutdown, RemoveWellComponentShutdown, true },
    { "WellComponent::OnCreate", InstallWellComponentOnCreate, RemoveWellComponentOnCreate, true },

    // --- WildAnimalComponent ---
    { "WildAnimalComponent::Shutdown", InstallWildAnimalComponentShutdown, RemoveWildAnimalComponentShutdown, true },
    { "WildAnimalComponent::OnCreate", InstallWildAnimalComponentOnCreate, RemoveWildAnimalComponentOnCreate, true },

    // --- WorkComponent ---
    { "WorkComponent::Shutdown", InstallWorkComponentShutdown, RemoveWorkComponentShutdown, true },
    { "WorkComponent::Draw", InstallWorkComponentDraw, RemoveWorkComponentDraw, true },
    { "WorkComponent::OnCreate", InstallWorkComponentOnCreate, RemoveWorkComponentOnCreate, true },

    // --- WorkPlaceComponent ---
    { "WorkPlaceComponent::Shutdown", InstallWorkPlaceComponentShutdown, RemoveWorkPlaceComponentShutdown, true },
    { "WorkPlaceComponent::OnCreate", InstallWorkPlaceComponentOnCreate, RemoveWorkPlaceComponentOnCreate, true },

    // --- ZoneComponent ---
    { "ZoneComponent::Shutdown", InstallZoneComponentShutdown, RemoveZoneComponentShutdown, true },
    { "ZoneComponent::Draw", InstallZoneComponentDraw, RemoveZoneComponentDraw, true },
    { "ZoneComponent::OnCreate", InstallZoneComponentOnCreate, RemoveZoneComponentOnCreate, true },
};

static constexpr int g_reimplCount = sizeof(g_reimplementations) / sizeof(g_reimplementations[0]);

void InstallHooks() {
#ifdef _DEBUG
    printf("[BanishedPatch] Installing %d hooks/patches...\n", g_reimplCount);
#endif

    for (int i = 0; i < g_reimplCount; i++) {
        auto& r = g_reimplementations[i];
        if (r.enabled) {
            r.install();
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: installed\n", i, r.name);
#endif
        } else {
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: DISABLED\n", i, r.name);
#endif
        }
    }

#ifdef _DEBUG
    printf("[BanishedPatch] All hooks installed.\n");
#endif
}

void RemoveHooks() {
#ifdef _DEBUG
    printf("[BanishedPatch] Removing hooks...\n");
#endif

    for (int i = g_reimplCount - 1; i >= 0; i--) {
        g_reimplementations[i].remove();
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Hooks removed.\n");
#endif
}
