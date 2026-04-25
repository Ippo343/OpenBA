// Address registry for standalone exe build.
// Maps all known Ghidra exe addresses to our reimplemented functions/data.
//
// This is the core mapping that makes standalone mode work. As functions get
// reimplemented in code/Game/, add their addresses here. Unregistered addresses
// return 0 (NULL) from ResolveAddr and log a NOT IMPLEMENTED warning.
//
// Three sections:
//   1. CODE FUNCTIONS — direct exe function reimplementations
//   2. IAT DISPATCH SLOTS — .rdata slots that hold Runtime DLL function pointers
//   3. VTABLES & TYPE IDS — static vtable arrays and type info globals

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "address_registry.h"
#include "engine_funcs.h"
#include "Game/game.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Section 1: CODE FUNCTIONS
// ============================================================
// As you reimplement exe functions in code/Game/, add entries here:
//   RegisterExeAddr(0x140XXXXXX, (uintptr_t)&YourFunction);
//
// Example:
//   #include "Game/weather_funcs.h"
//   RegisterExeAddr(0x140025020, (uintptr_t)&Weather_Dtor);

static void RegisterCodeFunctions() {
    // Each entry maps a Ghidra address to our reimplemented function.
    // 349 unique code addresses to fill (0x140001xxx - 0x14014Fxxx range).
    // These are exe-internal functions called by code/Game/ via function pointers.

    // Game/KeyMap container ctor — FUN_140001050 (0xe0 bytes, sets g_pKeyMapSingleton)
    RegisterExeAddr(0x140001050, reinterpret_cast<uintptr_t>(&Game_Ctor));
}

// ============================================================
// Section 2: IAT DISPATCH SLOTS
// ============================================================
// The original exe has .rdata slots at 0x140151xxx-0x140152xxx that hold
// pointers to Runtime DLL functions (filled by the exe's custom loader).
// In standalone mode, we allocate our own static slots and fill them with
// the Runtime DLL function pointers resolved via ResolveExport/ResolveOrdinal.
//
// Each slot is a uintptr_t that code/Game/ reads via:
//   slot = ResolveAddr(Module::Exe, 0x14015XXXX);
//   fn = *reinterpret_cast<fn_t*>(slot);
//
// So we need to provide a real memory address containing the function pointer.

// Storage for IAT dispatch slot values.
// Each slot is a function pointer that code/Game/ dereferences.
#include <unordered_map>

static std::unordered_map<uintptr_t, uintptr_t>& GetSlotStorage() {
    static std::unordered_map<uintptr_t, uintptr_t> slots;
    return slots;
}

// Register a dispatch slot: the registry maps ghidra_addr -> &slot_storage[ghidra_addr],
// and slot_storage[ghidra_addr] holds the actual function pointer.
static void RegisterDispatchSlot(uintptr_t ghidra_addr, uintptr_t fn_ptr) {
    auto& slots = GetSlotStorage();
    slots[ghidra_addr] = fn_ptr;
    // Register the ADDRESS of our slot storage (not the fn ptr itself),
    // because code reads: fn = *(fn_t*)ResolveAddr(Module::Exe, addr)
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

static void RegisterDispatchSlots() {
    // Each entry maps an exe .rdata dispatch slot to a Runtime DLL function.
    // EngineFuncsInit and Description reimpls read these via:
    //   fn = *(fn_t*)ResolveAddr(Module::Exe, 0x14015XXXX);
    RegisterDispatchSlot(0x1401518c0,
        (uintptr_t)ResolveExport("?GetFileVersion@ElementDescription@UI@@UEBAHXZ"));
    RegisterDispatchSlot(0x1401518f8,
        (uintptr_t)ResolveExport("?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z"));
    RegisterDispatchSlot(0x140151908,
        (uintptr_t)ResolveExport("?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z"));
    RegisterDispatchSlot(0x140151900,
        (uintptr_t)ResolveExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z"));
    RegisterDispatchSlot(0x140151910,
        (uintptr_t)ResolveExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z"));
    RegisterDispatchSlot(0x140151238,
        (uintptr_t)ResolveExport("?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x1401518c8,
        (uintptr_t)ResolveExport("?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z"));
    RegisterDispatchSlot(0x1401518b8,
        (uintptr_t)ResolveExport("??1ElementDescription@UI@@UEAA@XZ"));
    RegisterDispatchSlot(0x1401518b0,
        (uintptr_t)ResolveExport("??0ElementDescription@UI@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401518d0,
        (uintptr_t)ResolveExport("?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z"));
    RegisterDispatchSlot(0x1401518d8,
        (uintptr_t)ResolveExport("??0Element@UI@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401518e0,
        (uintptr_t)ResolveExport("??1Element@UI@@UEAA@XZ"));
    RegisterDispatchSlot(0x1401518e8,
        (uintptr_t)ResolveExport("?ComputeLayout@Element@UI@@UEAAXHHHH@Z"));
    RegisterDispatchSlot(0x1401518f0,
        (uintptr_t)ResolveExport("?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z"));
    RegisterDispatchSlot(0x140152400,
        (uintptr_t)ResolveExport("?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z"));
    RegisterDispatchSlot(0x140152738,
        (uintptr_t)ResolveExport("?CheckMod@SaveLoad@Save@@QEBAHHH@Z"));
    RegisterDispatchSlot(0x140151650,
        (uintptr_t)ResolveExport("?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z"));
    RegisterDispatchSlot(0x1401514c8,
        (uintptr_t)ResolveExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z"));
    RegisterDispatchSlot(0x140152b48,
        (uintptr_t)ResolveExport("??0Sprite@UI@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152770,
        (uintptr_t)ResolveExport("??1Internal@Object@@UEAA@XZ"));
    RegisterDispatchSlot(0x140152a30,
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
}

// ============================================================
// Section 3: VTABLES & TYPE IDS
// ============================================================
// Static vtable arrays for game classes. In standalone mode, WE define
// the vtable contents (pointing to our reimplemented virtual functions).
// Type IDs are simple uint32_t globals used by FindComponent.

// Singleton-pointer storage in .data/.bss. The original exe has 8-byte slots
// for global object pointers (e.g. KeyMap, GameWorld, GameSettings) that get
// written during Game_Ctor / engine boot. We reserve our own static storage
// here and register its address; engine_funcs.cpp dereferences these slots.
static void* g_storage_KeyMapSingleton = nullptr;       // 0x1401c3de8
static void* g_storage_GameWorldSingleton = nullptr;    // 0x1401c3dd8
static void* g_storage_GameSettingsPtr = nullptr;       // 0x1401c3de0

// Defined in standalone/menudialogloopt_stubs.cpp.
extern void RegisterMenuDialogLoopVtable();

static void RegisterVtablesAndTypeIds() {
    // --- Game singleton pointer storage (initialized to null, written at boot) ---
    RegisterExeAddr(0x1401c3de8, reinterpret_cast<uintptr_t>(&g_storage_KeyMapSingleton));
    RegisterExeAddr(0x1401c3dd8, reinterpret_cast<uintptr_t>(&g_storage_GameWorldSingleton));
    RegisterExeAddr(0x1401c3de0, reinterpret_cast<uintptr_t>(&g_storage_GameSettingsPtr));

    // --- MenuDialogLoopT vtable (0x14015ed00) — stubs for now ---
    RegisterMenuDialogLoopVtable();

    // TODO: Define static vtable arrays for each game class.
    // Each vtable is an array of function pointers matching the original layout.
    //
    // Pattern:
    //   static void* vtable_Weather[] = {
    //       (void*)&Weather_Dtor,     // +0x00
    //       (void*)&Weather_Slot1,    // +0x08
    //       ...
    //   };
    //   RegisterExeAddr(0x140160290, (uintptr_t)vtable_Weather);
    //
    // Type IDs are simpler — just static uint32_t storage:
    //   static uint32_t typeId_WeatherComponent = 0; // filled by RegisterType
    //   RegisterExeAddr(0x1401c7XXX, (uintptr_t)&typeId_WeatherComponent);
}

// ============================================================
// Init — call all registration sections
// ============================================================
void AddressRegistryInit() {
    RegisterCodeFunctions();
    RegisterDispatchSlots();
    RegisterVtablesAndTypeIds();

#ifdef _DEBUG
    AddressRegistryPrintStats();
#endif
}

void AddressRegistryPrintStats() {
    RegistryStats stats = GetRegistryStats();
    printf("[AddressRegistry] %zu addresses registered\n", stats.registered);
}

#endif // STANDALONE_EXE
