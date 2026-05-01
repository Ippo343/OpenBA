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
// KeyMap exref (slot 0x140151738) — fake External wrapper for standalone.
//
// Original exe layout: the slot at 0x140151738 holds an Object::ExternalPtr
// wrapper that, when triple-dereffed, yields the live Input::KeyMap pointer.
// The Runtime DLL fills this slot during type-registration plumbing in the
// real game. In standalone we never go through that machinery, so the slot
// stays zero and ButtonElement::Update faults inside Input::KeyMap::Pressed.
//
// We fake it with a 2-link chain matching the disasm
//   MOV RAX,[0x140151738] ; MOV R8,[RAX] ; MOV R14,[R8]
// where R14 = KeyMap*. Three derefs total from the slot address.
//
// After Engine::Startup (which constructs Input::Configuration with its
// embedded KeyMap), RunGameMain calls SetStandaloneKeyMap(km) to fill
// g_StandaloneKeyMapPtr with the real pointer. Game::RegisterDefaultInputBindings
// then walks the same triple-deref chain the original code uses.
// ============================================================
extern "C" {
    // Level 3 — actual KeyMap*. Filled at runtime after Engine::Startup.
    void* g_StandaloneKeyMapPtr = nullptr;
    // Level 2 — points to level-3 storage (constant).
    void* g_StandaloneKeyMapMid = &g_StandaloneKeyMapPtr;
}

extern "C" void SetStandaloneKeyMap(void* keyMap) {
    g_StandaloneKeyMapPtr = keyMap;
}

// Per-class vtable registration entry points. Each lives next to its class's
// reimplementation in code/Game/<Module>/<Class>.cpp.
extern "C" {
    void RegisterStatGoalVtable();
    void RegisterTimeGoalVtable();
    void RegisterBuildingGoalVtable();
    void RegisterPopulationGoalVtable();
    void RegisterTimeGoalInstanceVtable();
    void RegisterFinishTutorialGoalVtable();
    void RegisterFinishTutorialInstanceVtable();
    void RegisterMainMenuDialogVtable();
    void RegisterLoaderDialogVtable();
    void RegisterOptionsDialogVtable();
    void RegisterOptionInputBindDialogVtable();
    void RegisterMessageDialogVtable();
    void RegisterMenuSceneVtable();
}

// ============================================================
// Section 1: CODE FUNCTIONS
// ============================================================
// As you reimplement exe functions in code/Game/, add entries here:
//   RegisterExeAddr(0x140XXXXXX, (uintptr_t)&YourFunction);
//
// Example:
//   #include "Game/weather_funcs.h"
//   RegisterExeAddr(0x140025020, (uintptr_t)&Weather_Dtor);

// M3.5 — MenuScene::Serialize generic helpers
// (decompiled bodies live in code/Game/Game/serialize_arrays.cpp +
//  code/Game/Game/serialize_externals.cpp)
extern "C" {
    void Reimpl_Geometry_Init(void* self);
    void Reimpl_Transform_SerializeWrapper(void* transform, void* stream);
    void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount);
    void Reimpl_Array_Serialize_Stride20(void* serializer, uint32_t count, void* arrayBase);
    void Reimpl_Array_Serialize_External_GraphicsMesh(void* serializer, int count, void* arrayBase);
    void Reimpl_Array_Serialize_External_Particle(void* serializer, int count, void* arrayBase);
    void Reimpl_ExternalPtr_Serialize_GraphicsMesh(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_Weather(void* refField, void* serializer);
}

static void RegisterCodeFunctions() {
    // Each entry maps a Ghidra address to our reimplemented function.
    // 349 unique code addresses to fill (0x140001xxx - 0x14014Fxxx range).
    // These are exe-internal functions called by code/Game/ via function pointers.

    // Game/KeyMap container ctor — FUN_140001050 (0xe0 bytes, sets g_pKeyMapSingleton)
    RegisterExeAddr(0x140001050, reinterpret_cast<uintptr_t>(&Game_Ctor));

    // ============================================================
    // M3.5 — MenuScene::Serialize generic helpers
    // ============================================================
    RegisterExeAddr(0x1400b00e0, reinterpret_cast<uintptr_t>(&Reimpl_Geometry_Init));
    RegisterExeAddr(0x1400988c0, reinterpret_cast<uintptr_t>(&Reimpl_Transform_SerializeWrapper));
    RegisterExeAddr(0x14001baf0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride20));
    RegisterExeAddr(0x140001bb0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride08));
    RegisterExeAddr(0x14001bf10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride14));
    RegisterExeAddr(0x14001bbf0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_Stride20));
    RegisterExeAddr(0x14001bcc0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_GraphicsMesh));
    RegisterExeAddr(0x14001bff0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_Particle));
    RegisterExeAddr(0x14001b8e0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_GraphicsMesh));
    RegisterExeAddr(0x1400196a0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_Weather));
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

    // Application::Application::_singleton — Runtime data variable holding the
    // Application*. The exe normally has a .rdata exref at 0x140152368 that
    // points to this Runtime variable. engine_funcs.cpp reads:
    //   exref       = ResolveAddr(Exe, 0x140152368)         -> our slot storage
    //   *exref      = ResolveAddr(Runtime, 0x1801fc678)     -> &Application::_singleton
    //   g_pAppSingleton = *exref                             -> &Application::_singleton
    //   *g_pAppSingleton                                    -> Application*
    // Without this registration MainMenuDialog::OnCommand can't reach App::Quit.
    RegisterDispatchSlot(0x140152368,
        ResolveAddr(Module::Runtime, 0x1801fc678));

    // Object::Factory::_singleton — same pattern.
    // engine_funcs.cpp Phase H reads slot 0x140152760, expecting **slot to
    // be the live Factory*. Without this, fn_Factory_GetTypeIdFromName +
    // singleton are unusable, blocking the type-ID fallback in
    // LoadDialogResource (and many other Factory consumers).
    RegisterDispatchSlot(0x140152760,
        ResolveAddr(Module::Runtime, 0x1801fc6e8));

    // UI::Dialog::_typeInfo external — exe slot 0x140151728 in the original
    // is patched by the loader with the **type ID itself** (4-byte uint),
    // NOT a pointer to BaseTypeInfo. Original code reads it as
    // `*(uint32_t*)0x140151728` (single deref), so the slot must contain
    // the typeId directly.
    //
    // Note: Ghidra labels this _typeInfo@DialogInstance based on a heuristic
    // but the actual type checked here is UI::Dialog (Runtime 0x1801faa88) —
    // FUN_140064720 loads Dialog/Follow.rsc and runs IsDerivedFrom against
    // this slot, and the .rsc loads as a UI::Dialog External.
    //
    // Type ID isn't known until Factory::Startup runs; LoadDialogResource has
    // a fallback path that uses fn_Factory_GetTypeIdFromName instead, so we
    // leave this slot zero-filled until something else needs it.
    RegisterDispatchSlot(0x140151728, 0);

    // Object::External::External — dispatch slot for the External base ctor.
    // MenuScene_Init reads:
    //   slot = ResolveAddr(Module::Exe, 0x140151e98);
    //   fn_External_Ctor = *(fn_t*)slot;
    // So the slot must hold a pointer to the Runtime DLL's External::External.
    // Mangled: ??0External@Object@@QEAA@XZ (default ctor, public non-virtual).
    RegisterDispatchSlot(0x140151e98,
        (uintptr_t)ResolveExport("??0External@Object@@QEAA@XZ"));

    // Object::External::~External — paired dtor for the same dispatch pattern.
    // MenuScene_Dtor reads slot 0x140151ea0 the same way.
    RegisterDispatchSlot(0x140151ea0,
        (uintptr_t)ResolveExport("??1External@Object@@UEAA@XZ"));

    // Input::KeyMap exref. Slot stores a level-1 pointer to a "level-2 cell"
    // whose contents are the real KeyMap*. See the comment near
    // g_StandaloneKeyMapPtr above for the reasoning.
    //
    // Order: ResolveAddr(0x140151738) -> &slot_storage (uintptr_t)
    //        *(void**)slot_storage = (void*)&g_StandaloneKeyMapMid  -- level 1
    //        *(void**)&g_StandaloneKeyMapMid = g_StandaloneKeyMapMid
    //                                        = &g_StandaloneKeyMapPtr -- level 2
    //        *(void**)&g_StandaloneKeyMapPtr = g_StandaloneKeyMapPtr
    //                                        = KeyMap* -- level 3
    // Triple deref from the slot address yields the KeyMap, matching the
    // original exe's disasm.
    RegisterDispatchSlot(0x140151738,
        reinterpret_cast<uintptr_t>(&g_StandaloneKeyMapMid));
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

// Defined in standalone/splash_loop_stubs.cpp.
extern void RegisterSplashDialogLoopVtable();

// Defined in standalone/loader_loop_stubs.cpp.
extern void RegisterLoaderLoopVtables();

static void RegisterVtablesAndTypeIds() {
    // --- Game singleton pointer storage (initialized to null, written at boot) ---
    RegisterExeAddr(0x1401c3de8, reinterpret_cast<uintptr_t>(&g_storage_KeyMapSingleton));
    RegisterExeAddr(0x1401c3dd8, reinterpret_cast<uintptr_t>(&g_storage_GameWorldSingleton));
    RegisterExeAddr(0x1401c3de0, reinterpret_cast<uintptr_t>(&g_storage_GameSettingsPtr));

    // --- MenuDialogLoopT vtable (0x14015ed00) — stubs for now ---
    RegisterMenuDialogLoopVtable();

    // --- Splash DialogLoaderLoopT vtable (0x140158800) — stubs for now ---
    // Used by both Dialog\Loading.rsc and Dialog\ShiningRock.rsc instances.
    RegisterSplashDialogLoopVtable();

    // --- LoaderLoop vtables (0x140158870 inner + 0x1401588d0 outer) — stubs ---
    RegisterLoaderLoopVtables();

    // Per-class vtables defined alongside their reimplementation in
    // code/Game/<Module>/<Class>.cpp — declared at file scope above.
    RegisterStatGoalVtable();
    RegisterTimeGoalVtable();
    RegisterBuildingGoalVtable();
    RegisterPopulationGoalVtable();
    RegisterTimeGoalInstanceVtable();
    RegisterFinishTutorialGoalVtable();
    RegisterFinishTutorialInstanceVtable();
    RegisterMainMenuDialogVtable();
    RegisterLoaderDialogVtable();
    RegisterOptionsDialogVtable();
    RegisterOptionInputBindDialogVtable();
    RegisterMessageDialogVtable();
    RegisterMenuSceneVtable();

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
