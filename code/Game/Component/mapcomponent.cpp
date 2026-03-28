// code/Game/Component/mapcomponent.cpp
// Clean reimplementation of MapComponent lifecycle methods from Application-x64.exe.
//
// MapComponent (0x38 bytes) manages tile map coordinates for entities.
// Fields: +0x28..+0x2E = four uint16 tile coords (initialized to 0xFFFF = invalid)
//         +0x30 = int32 extra field (serialized)
//
// Vtable at 0x140163aa0. Three non-trivial overrides:
//   Shutdown  (0x1401243e0) — Serializes tile coords as shorts + extra int
//   Draw      (0x140124370) — Checks building flag, calls map update helper
//   OnCreate  (0x1401242f0) — Reads building data, calls terrain clear + map init
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/mapcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// IO::Serializer::Serialize(short*) — dispatch slot at 0x140152468
typedef void (*fn_Serialize_Short_t)(void* serializer, int16_t* field, const char* name);
static fn_Serialize_Short_t s_fn_Serialize_Short = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_dispatch_t)(void* serializer, int32_t* field, const char* name);
static fn_Serialize_Int_dispatch_t s_fn_Serialize_Int_dispatch = nullptr;

// IO::Serializer::Serializer(stream, version) — dispatch slot at 0x140152ad8
typedef void (*fn_Serializer_Ctor_dispatch_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_dispatch_t s_fn_Serializer_Ctor_dispatch = nullptr;

// IO::Serializer::~Serializer() — dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_dispatch_t)(void* self);
static fn_Serializer_Dtor_dispatch_t s_fn_Serializer_Dtor_dispatch = nullptr;

// Component_Serialize (base class) — at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// MapComponent_sub_41b0 — map overlay update helper (0x1401241b0)
typedef void (*fn_MapUpdateOverlay_t)(void* self);
static fn_MapUpdateOverlay_t s_fn_MapUpdateOverlay = nullptr;

// CreateNaturalResource_impl_4750 — map initialization (0x140124750)
typedef void (*fn_MapInit_t)(void* self);
static fn_MapInit_t s_fn_MapInit = nullptr;

// MapComponent_helper_4d30 — terrain clear helper (0x1400a4d30)
typedef void (*fn_TerrainClear_t)(void* terrainPtr, int param2, int x0, int y0, int x1, int y1);
static fn_TerrainClear_t s_fn_TerrainClear = nullptr;

// TypeId for MapDescription — at 0x1401c7a18
static uint32_t* s_TypeId_MapDescription = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots — read function pointer from .rdata slot at runtime
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152468);
    if (slot) s_fn_Serialize_Short = *reinterpret_cast<fn_Serialize_Short_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int_dispatch = *reinterpret_cast<fn_Serialize_Int_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor_dispatch = *reinterpret_cast<fn_Serializer_Ctor_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor_dispatch = *reinterpret_cast<fn_Serializer_Dtor_dispatch_t*>(slot);

    // Direct exe addresses
    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_MapUpdateOverlay = reinterpret_cast<fn_MapUpdateOverlay_t>(
        ResolveAddr(Module::Exe, 0x1401241b0));

    s_fn_MapInit = reinterpret_cast<fn_MapInit_t>(
        ResolveAddr(Module::Exe, 0x140124750));

    s_fn_TerrainClear = reinterpret_cast<fn_TerrainClear_t>(
        ResolveAddr(Module::Exe, 0x1400a4d30));

    // Type ID pointer
    s_TypeId_MapDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a18));

    s_helpersResolved = true;
}

// ============================================================
// Helper: Get MapDescription sibling component
// ============================================================
// Pattern: IsDerivedFrom(self->_unk18, MapDescription typeId)
// Returns the component pointer if type matches, else nullptr.
static void* GetMapDescription(MapComponent* self) {
    EnsureHelpers();
    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_MapDescription) return nullptr;

    void* obj = self->_unk18;
    if (!obj) return nullptr;

    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_MapDescription);
    return derived ? obj : nullptr;
}

// ============================================================
// MapComponent::Shutdown (vtable slot [3], 0x1401243e0)
// ============================================================
// Serializes tile coordinates and extra int field.
//
// Original disassembly:
//   CALL Component_Serialize (0x1401189b0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serialize_Short(&tileX0, nullptr) x4
//   CALL Serialize_Int(&extraField, nullptr)
//   CALL Serializer_Dtor(local)

void MapComponent_Shutdown(MapComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (24 bytes on stack)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor_dispatch) {
        s_fn_Serializer_Ctor_dispatch(serializer, stream, 0);
    }

    // Serialize 4 uint16 tile coords as shorts
    if (s_fn_Serialize_Short) {
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileX0, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileY0, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileX1, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileY1, nullptr);
    }

    // Serialize extra int at +0x30
    if (s_fn_Serialize_Int_dispatch) {
        s_fn_Serialize_Int_dispatch(serializer, &self->extraField, nullptr);
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor_dispatch) {
        s_fn_Serializer_Dtor_dispatch(serializer);
    }
}

// ============================================================
// MapComponent::Draw (vtable slot [6], 0x140124370)
// ============================================================
// Checks building data flag at +0x30 (byte), calls map update overlay if nonzero.
//
// Original disassembly:
//   MOV RBX,[RCX+0x18]              ; get sibling component
//   MOV EDX,[0x1401c7a18]           ; MapDescription type ID
//   CALL [0x140152a98]              ; IsDerivedFrom
//   CMOVNZ RDX,RBX                 ; building = derived ? obj : nullptr
//   CMP byte [RDX+0x30],0          ; check building flag
//   JZ skip
//   CALL MapComponent_sub_41b0     ; update map overlay

void MapComponent_Draw(MapComponent* self) {
    EnsureHelpers();

    void* building = GetMapDescription(self);
    if (!building) return;

    // Check building[0x30] != 0 (byte flag: hasMapOverlay)
    uint8_t flag = *(uint8_t*)((uintptr_t)building + 0x30);
    if (flag != 0) {
        if (s_fn_MapUpdateOverlay) {
            s_fn_MapUpdateOverlay(self);
        }
    }
}

// ============================================================
// MapComponent::OnCreate (vtable slot [7], 0x1401242f0)
// ============================================================
// Sets up map tiles. If building[0x32] != 0, clears terrain for the tile area.
// Always calls map initialization at the end.
//
// Original disassembly:
//   Get building via IsDerivedFrom
//   CMP byte [RDX+0x32],0
//   JZ skip_clear
//   MOVSX sign-extend tile coords from int16 to int32
//   MOV RCX,[g_pGameWorldSingleton]
//   MOV RCX,[RCX+0x148]            ; terrain subsystem pointer
//   XOR EDX,EDX                    ; param2 = 0
//   CALL 0x1400a4d30               ; terrain clear helper
// skip_clear:
//   JMP CreateNaturalResource_impl_4750  ; tail call to map init

void MapComponent_OnCreate(MapComponent* self) {
    EnsureHelpers();

    void* building = GetMapDescription(self);
    if (building) {
        // Check building[0x32] != 0 (byte flag: needsTerrainClear)
        uint8_t clearFlag = *(uint8_t*)((uintptr_t)building + 0x32);
        if (clearFlag != 0) {
            // Get terrain subsystem from game world singleton
            if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_TerrainClear) {
                uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
                void* terrainPtr = *(void**)(gameWorld + 0x148);
                if (terrainPtr) {
                    s_fn_TerrainClear(terrainPtr, 0,
                        (int)(int16_t)self->tileX0,
                        (int)(int16_t)self->tileY0,
                        (int)(int16_t)self->tileX1,
                        (int)(int16_t)self->tileY1);
                }
            }
        }
    }

    // Always call map initialization (tail call in original)
    if (s_fn_MapInit) {
        s_fn_MapInit(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MapComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MapComponent] Funcs resolved: IsDerivedFrom=%p Serialize_Short=%p "
           "MapUpdateOverlay=%p MapInit=%p TerrainClear=%p\n",
           (void*)s_fn_IsDerivedFrom_dispatch, (void*)s_fn_Serialize_Short,
           (void*)s_fn_MapUpdateOverlay, (void*)s_fn_MapInit,
           (void*)s_fn_TerrainClear);
#endif
}
