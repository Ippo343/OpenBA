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
    // TODO: As functions are confirmed working in DLL mode, register them here.
    // Each entry maps a Ghidra address to our reimplemented function.
    //
    // Pattern:
    //   RegisterExeAddr(GHIDRA_ADDR, (uintptr_t)&OurFunction);
    //
    // 349 unique code addresses to fill (0x140001xxx - 0x14014Fxxx range).
    // These are exe-internal functions called by code/Game/ via function pointers.
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
__attribute__((unused))
static void RegisterDispatchSlot(uintptr_t ghidra_addr, uintptr_t fn_ptr) {
    auto& slots = GetSlotStorage();
    slots[ghidra_addr] = fn_ptr;
    // Register the ADDRESS of our slot storage (not the fn ptr itself),
    // because code reads: fn = *(fn_t*)ResolveAddr(Module::Exe, addr)
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

static void RegisterDispatchSlots() {
    // TODO: Map each .rdata dispatch slot to its Runtime DLL function.
    // These are resolved via ResolveExport() or ResolveOrdinal() after Runtime DLL loads.
    //
    // Pattern:
    //   void* fn = ResolveExport("?MangledName@@...");
    //   if (fn) RegisterDispatchSlot(0x14015XXXX, (uintptr_t)fn);
    //
    // 233 unique .rdata addresses to fill (0x14015xxxx - 0x1401Cxxxx range).
    // Many of these are already resolved in engine_funcs.cpp — just need the
    // Ghidra slot address -> mangled name mapping.
    //
    // Priority: the most-used slots (Time::AsSeconds, Random::RandFloat,
    // CheckPtr, FindComponent, etc.) should be filled first.
}

// ============================================================
// Section 3: VTABLES & TYPE IDS
// ============================================================
// Static vtable arrays for game classes. In standalone mode, WE define
// the vtable contents (pointing to our reimplemented virtual functions).
// Type IDs are simple uint32_t globals used by FindComponent.

static void RegisterVtablesAndTypeIds() {
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
