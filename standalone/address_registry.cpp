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

// M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
// (decompiled bodies live in code/Game/Game/serialize_struct60b.cpp)
extern "C" {
    void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount);
    void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src);
    void Reimpl_Struct60B_DtorRange(void* unused, void* first, int count);
}

// M3.5 Path B session 3 — typed-ExternalPtr serialize helper at 0x140007140.
// (decompiled body lives in code/Game/Game/serialize_externals_typed.cpp)
extern "C" {
    void Reimpl_ExternalPtr_Serialize_Typed_7140(void* refField, void* serializer);
}

// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
// (decompiled bodies live in code/Game/Game/serialize_resize_helpers.cpp)
extern "C" {
    void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_ArrayOfArrays(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);
}

// M3.5 Path B session 4 — per-element 0x60B + typed-ExternalPtr-array serialize
// (decompiled bodies live in code/Game/Game/serialize_struct60b_serialize.cpp)
extern "C" {
    void Reimpl_Struct60B_Serialize(void* serializer, int count, void* arrayBase);
    void Reimpl_Array_Serialize_External_MaterialInstance(
        void* serializer, int count, void* arrayBase);
}

// M3.5 Path B session 6 — heightmap mesh-build path helpers
// (decompiled bodies live in code/Game/Game/serialize_heightmap_mesh.cpp)
extern "C" {
    void  Reimpl_TradeWork_sub_c1e0(void* self);
    void  Reimpl_FUN_1400ac880(void* self, int colStart, int rowStart,
                               int cellSize_x, int cellSize_y, int16_t* vertOut);
    void* Reimpl_TradeWork_helper_e270(void* self, void* retbuf, int col, int row);
}

// M3.5 Path B session 7 — image-buffer heightmap upload helper
// (decompiled body lives in code/Game/Game/serialize_image_upload.cpp)
extern "C" {
    void Reimpl_TradeWork_sub_c780(void* self);
}

// M3.5 Path B session 8 — Geometry::Serialize body + version-gated extra
// (decompiled bodies live in code/Game/Game/geometry_serialize.cpp)
extern "C" {
    void Reimpl_TradeWork_impl_04d0(void* self, void* stream);
    void Reimpl_TradeWork_impl_f1a0(void* self, void* stream);
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

    // ============================================================
    // M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
    // ============================================================
    RegisterExeAddr(0x14001f250, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_Resize));
    RegisterExeAddr(0x14001f5e0, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_CopyCtor));
    RegisterExeAddr(0x14001fb80, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_DtorRange));

    // ============================================================
    // M3.5 Path B session 3 — typed-ExternalPtr serialize helper (0x140007140).
    // Body byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh) except
    // the typeInfo source slot — reads BaseTypeInfo* from .rdata slot
    // 0x140151350 (Graphics::MaterialInstance).
    // ============================================================
    RegisterExeAddr(0x140007140, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_Typed_7140));

    // ============================================================
    // M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
    // (uint16, int4, array-of-arrays, mesh-vertex 16B, int)
    // ============================================================
    RegisterExeAddr(0x14001c890, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride02));
    RegisterExeAddr(0x14001efd0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride16A));
    RegisterExeAddr(0x14001f0a0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_ArrayOfArrays));
    RegisterExeAddr(0x14001ef10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride16B));
    RegisterExeAddr(0x1400021d0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride04));

    // ============================================================
    // M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
    // array serialize (Geometry::Serialize cascade points 10 + 13).
    //   FUN_14001ffa0 — per-element 0x60B serialize (calls FUN_140007140 inside)
    //   FUN_140020150 — typed-ExternalPtr array serialize (Graphics::MaterialInstance,
    //                   typeInfo slot 0x140151350)
    // ============================================================
    RegisterExeAddr(0x14001ffa0, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_Serialize));
    RegisterExeAddr(0x140020150, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_MaterialInstance));

    // ============================================================
    // M3.5 Path B session 6 — heightmap mesh-build path helpers
    //   sub_c1e0     — top-level mesh builder (allocates Vertex/IndexBuffers)
    //   FUN_ac880    — per-cell vertex compute
    //   helper_e270  — vertex normal compute via heightmap deltas
    // ============================================================
    RegisterExeAddr(0x1400ac1e0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_sub_c1e0));
    RegisterExeAddr(0x1400ac880, reinterpret_cast<uintptr_t>(&Reimpl_FUN_1400ac880));
    RegisterExeAddr(0x1400ae270, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_helper_e270));

    // ============================================================
    // M3.5 Path B session 7 — image-buffer heightmap upload helper
    //   sub_c780     — alloc ImageBuffer, set NoiseUI vtable, Create + upload
    //                  (heightmap raw bytes -> Video texture)
    // ============================================================
    RegisterExeAddr(0x1400ac780, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_sub_c780));

    // ============================================================
    // M3.5 Path B session 8 — Geometry::Serialize body + version-gated extra
    //   impl_04d0    — main 15-step Geometry::Serialize body (master schema, v6)
    //   impl_f1a0    — version-gated extra serialize (parallel schema, v4)
    //
    // Both walk the helper graph established in sessions 2-7 and call into
    // the IO::Serializer dispatch slots (lazy-resolved within the reimpl).
    // ============================================================
    RegisterExeAddr(0x1400b04d0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_impl_04d0));
    RegisterExeAddr(0x1400af1a0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_impl_f1a0));
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

    // ----------------------------------------------------------------
    // M3.5 cleanup — IO::Serializer dispatch slots (Geometry::Serialize)
    // ----------------------------------------------------------------
    // Slots verified against fn_TradeWork_impl_04d0 (0x1400b04d0) call sites:
    //   ctor  takes (this, Stream&, version)        — single-arg, RDX/R8 set by caller
    //   dtor  takes (this)                          — single-arg
    //   Int/Bool/Byte/Float take (this, T*, name)
    //   UShort/Short/Byte arrays take (this, count, T*, name)
    //   GetVersion takes (this) -> int
    // Without these, terrain heightmap soft-fails and the menu renders empty.
    RegisterDispatchSlot(0x140152ad8,
        (uintptr_t)ResolveExport("??0Serializer@IO@@QEAA@AEAVStream@1@H@Z"));
    RegisterDispatchSlot(0x140152ad0,
        (uintptr_t)ResolveExport("??1Serializer@IO@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152ac8,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z"));
    RegisterDispatchSlot(0x140152410,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z"));
    RegisterDispatchSlot(0x140152a58,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z"));
    RegisterDispatchSlot(0x140152a60,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z"));
    RegisterDispatchSlot(0x140152a48,
        (uintptr_t)ResolveExport("?GetVersion@Serializer@IO@@QEBAHXZ"));
    RegisterDispatchSlot(0x140152488,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAGPEBD@Z"));
    RegisterDispatchSlot(0x1401523f8,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z"));
    RegisterDispatchSlot(0x140152618,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z"));
    // Bulk int array: Serialize(int count, int* data, name).
    // Used in many places (TradeWork, MoveTo, AutoPickup, etc.) and required
    // for Geometry::Serialize step 11 (+0x220 vertex-int array, count=901
    // observed; without this the cursor was advancing wrong → step 13 read
    // garbage count).
    RegisterDispatchSlot(0x140151278,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAHPEBD@Z"));
    RegisterDispatchSlot(0x1401526b8,
        (uintptr_t)ResolveExport("?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x140152668,
        (uintptr_t)ResolveExport("?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    RegisterDispatchSlot(0x140152660,
        (uintptr_t)ResolveExport("?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z"));

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

// ============================================================
// M3.5 Path B session 7 — NoiseUI ImageBuffer vtable scaffolding
// ============================================================
// The original exe stores a 10-slot vtable at 0x14015c8c0 that gets installed
// onto Video::ImageBuffer instances created by NoiseUI / ModDialog / and
// (per session 7) the Geometry::Serialize heightmap upload helper. Each slot
// in the original is a thin JMP qword [0x14015XXXX] thunk that the loader
// fills with Runtime DLL function pointers.
//
// In standalone mode we own the storage: WE allocate a 10-slot array, fill it
// with Runtime exports resolved at boot, and register its address. Code that
// reads `g_NoiseUI_ImageBufferVtable` (which equals `ResolveAddr(Module::Exe,
// 0x14015c8c0)`) gets our backing storage and dispatches normally.
//
// Slot layout (verified via Ghidra disassembly @ 0x14015c8c0):
//   [0] +0x00  Dtor             — original 0x1400129D0 (ModDialog_Slot10)
//   [1] +0x08  TypeName-ish     — JMP [0x140151b20]
//   [2] +0x10  ?                — JMP [0x140151b28]
//   [3] +0x18  ?                — JMP [0x140151ae0]
//   [4] +0x20  ?                — JMP [0x140151240]
//   [5] +0x28  ?                — JMP [0x140151ae8]
//   [6] +0x30  ?                — JMP [0x140151b00]
//   [7] +0x38  ?                — JMP [0x140151b08]
//   [8] +0x40  ?                — JMP [0x140151b10]
//   [9] +0x48  Create           — JMP [0x140151a88]  (?Create@ImageBuffer@Video@@)
//
// Only slots [0] (dtor) and [9] (Create) are dispatched by code we currently
// have running in standalone. Other slots are left null — if the engine ever
// dispatches one and finds it null, the stub will need to be filled with the
// matching Runtime export. Cascade-risk surface documented in the session 7
// report. The Runtime exports for the unknown slots can be discovered via
// Ghidra's `0x14015XXXX` slot pre-loaded values when the Runtime DLL fills
// them at startup; for now the safe-no-op approach mirrors the
// "MenuScene_VtblStub_ReturnNull" pattern used elsewhere.
static void* g_NoiseUI_ImageBufferVtable_Standalone[10] = {0};

// Forward-decl ImageBuffer dtor type — Runtime exports
//   ??1ImageBuffer@Video@@UEAA@XZ
typedef void  (*fn_ImgBuf_Dtor_StandaloneVt_t)(void* self);
typedef bool  (*fn_ImgBuf_Create_StandaloneVt_t)(
    void* self, int imageType, int usage, int format,
    unsigned int mipLevels, int width, int height,
    int filterMode, int addrU, int addrV, int addrW,
    void* mipData);

// Safe no-op stub for slots [1]..[8] until they're individually resolved.
static void* NoiseUIImgBuf_VtblStub_ReturnNull(void* /*self*/, ...) {
    return nullptr;
}

extern "C" void RegisterNoiseUIImageBufferVtable() {
    // Resolve slot [0] = ImageBuffer destructor.
    fn_ImgBuf_Dtor_StandaloneVt_t fnDtor =
        reinterpret_cast<fn_ImgBuf_Dtor_StandaloneVt_t>(
            ResolveExport("??1ImageBuffer@Video@@UEAA@XZ"));
    if (!fnDtor) {
        fnDtor = reinterpret_cast<fn_ImgBuf_Dtor_StandaloneVt_t>(
            ResolveAddr(Module::Runtime, 0x1800f2610));
    }

    // Resolve slot [9] = ImageBuffer::Create — vtable +0x48 in the layout.
    // Mangled name verified via Ghidra:
    //   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@
    //     W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@
    //     V?$Array@E@Collection@@@Collection@@@Z
    fn_ImgBuf_Create_StandaloneVt_t fnCreate =
        reinterpret_cast<fn_ImgBuf_Create_StandaloneVt_t>(
            ResolveExport("?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z"));

    g_NoiseUI_ImageBufferVtable_Standalone[0] = reinterpret_cast<void*>(fnDtor);
    g_NoiseUI_ImageBufferVtable_Standalone[1] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[2] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[3] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[4] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[5] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[6] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[7] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[8] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[9] = reinterpret_cast<void*>(fnCreate);

    RegisterExeAddr(0x14015c8c0,
                    reinterpret_cast<uintptr_t>(g_NoiseUI_ImageBufferVtable_Standalone));
}

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

    // M3.5 Path B session 7 — NoiseUI ImageBuffer vtable scaffolding
    // (used by NoiseUI / ModDialog / Geometry::Serialize image upload helper).
    RegisterNoiseUIImageBufferVtable();

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
