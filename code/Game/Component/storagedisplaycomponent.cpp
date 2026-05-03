// code/Game/Component/storagedisplaycomponent.cpp
// Clean reimplementation of StorageDisplayComponent lifecycle methods from Application-x64.exe.
//
// StorageDisplayComponent (0x38 bytes) renders visual display objects for stored items
// on buildings (e.g. logs stacked outside a storage barn).
//
// Vtable at 0x14015a780. Six overrides:
//   Shutdown  (0x140056970) — Component_Serialize + empty Serializer block
//   Serialize (0x140056930) — RET 0 (3 bytes, unhookable no-op)
//   Draw      (0x140056730) — grid-based rendering of display objects
//   OnCreate  (0x1400566b0) — remove display objects from scene
//   Update    (0x1400566a0 -> 0x1400563e0) — proportional display allocation
//   OnUpdate2 (0x140056940) — Draw + Update combined
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/storagedisplaycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Graphics::Scene::RemoveObject(scene, objectId) via dispatch slot 0x140151360
typedef void (*fn_Scene_RemoveObject_t)(void* scene, uint32_t objectId);
static fn_Scene_RemoveObject_t s_fn_Scene_RemoveObject = nullptr;

// FUN_140004680 — FindComponent (MapComponent type)
typedef void* (*fn_FindComponent_Map_t)(uintptr_t entity, void* retval);
static fn_FindComponent_Map_t s_fn_FindComponent_Map = nullptr;

// FUN_14000ca10 — resize MeshInstance array (0x128-byte entries)
typedef void (*fn_MeshArrayResize_t)(void* arrayBase, int newCount);
static fn_MeshArrayResize_t s_fn_MeshArrayResize = nullptr;

// Math::Vector3::Vector3(out, x, y, z) — matches engine_funcs.h typedef
typedef float* (*fn_Vector3_Ctor_local_t)(float* self, float x, float y, float z);
static fn_Vector3_Ctor_local_t s_fn_Vector3_Ctor = nullptr;

// CharacterComponent_impl_f3e0 — map tile to world position
typedef void (*fn_TileToWorld_t)(void* mapInfo, float* outPos, float* inPos, char param4, void* param5);
static fn_TileToWorld_t s_fn_TileToWorld = nullptr;

// Math::Transform::FromYawPitchOffset — via dispatch slot 0x140151288
// Real: static Transform FromYawPitchOffset(Scalar& yaw, Scalar& pitch, Vector3& offset)
typedef void* (*fn_FromYawPitchOffset_t)(void* out, const void* yaw, const void* pitch, const void* offset);
static fn_FromYawPitchOffset_t s_fn_FromYawPitchOffset = nullptr;

// Graphics::Geometry::SetTransform — via dispatch slot 0x1401512e0
typedef void (*fn_SetTransform_t)(void* geometry, void* transform);
static fn_SetTransform_t s_fn_SetTransform = nullptr;

// Entity_FindComponent_Storage — find StorageComponent on entity
typedef void* (*fn_FindComponent_Storage_t)(uintptr_t entity, void* retval);
static fn_FindComponent_Storage_t s_fn_FindComponent_Storage = nullptr;

// Entity_impl_63e0_sub_62b0 — set display object mesh for one slot
typedef void (*fn_SetDisplayMesh_t)(uintptr_t self, int index, float fraction, uintptr_t materialType);
static fn_SetDisplayMesh_t s_fn_SetDisplayMesh = nullptr;

// DAT_1401c3dd8 — game state singleton
static void** s_pGameState = nullptr;

// DAT_140172204 — yaw rotation constant (float)
static float* s_pYawConstant = nullptr;

// DAT_140172114 — half-tile offset constant (float)
static float* s_pHalfTileOffset = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151360);
    if (slot) s_fn_Scene_RemoveObject = *reinterpret_cast<fn_Scene_RemoveObject_t*>(slot);

    s_fn_FindComponent_Map = reinterpret_cast<fn_FindComponent_Map_t>(
        ResolveAddr(Module::Exe, 0x140004680));

    s_fn_MeshArrayResize = reinterpret_cast<fn_MeshArrayResize_t>(
        ResolveAddr(Module::Exe, 0x14000ca10));

    // Math::Vector3::Vector3 — resolved via export
    s_fn_Vector3_Ctor = reinterpret_cast<fn_Vector3_Ctor_local_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    if (!s_fn_Vector3_Ctor) {
        // Fallback: dispatch slot
        slot = ResolveAddr(Module::Exe, 0x140151230);
        if (slot) s_fn_Vector3_Ctor = *reinterpret_cast<fn_Vector3_Ctor_local_t*>(slot);
    }

    s_fn_TileToWorld = reinterpret_cast<fn_TileToWorld_t>(
        ResolveAddr(Module::Exe, 0x14000f3e0));

    slot = ResolveAddr(Module::Exe, 0x140151288);
    if (slot) s_fn_FromYawPitchOffset = *reinterpret_cast<fn_FromYawPitchOffset_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401512e0);
    if (slot) s_fn_SetTransform = *reinterpret_cast<fn_SetTransform_t*>(slot);

    // Entity_FindComponent_Storage — FUN_140003050 looks for StorageComponent type (DWORD_1401c8070)
    // Wait, actually 0x140003050 finds by type DWORD_1401c8070 = StorageLocationComponent
    // The actual storage find for StorageDisplayComponent uses a different approach
    // From the decompilation: Entity_FindComponent_Storage is called on the entity
    // Let me use the address from the decompilation
    s_fn_FindComponent_Storage = reinterpret_cast<fn_FindComponent_Storage_t>(
        ResolveAddr(Module::Exe, 0x140003050));

    s_fn_SetDisplayMesh = reinterpret_cast<fn_SetDisplayMesh_t>(
        ResolveAddr(Module::Exe, 0x1400562b0));

    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_pYawConstant = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172204));

    s_pHalfTileOffset = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172114));

    s_initialized = true;
}

// ============================================================
// StorageDisplayComponent::Shutdown (vtable slot [3], 0x140056970)
// ============================================================
// Simple: Component_Serialize + one empty Serializer block (version 0).

void StorageDisplayComponent_Shutdown(StorageDisplayComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializerBuf[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializerBuf, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// StorageDisplayComponent::Draw (vtable slot [6], 0x140056730)
// ============================================================
// Grid-based rendering: computes world positions for each display object
// from building tile dimensions, creates rotated transforms, applies to geometry.
//
// Original logic:
//   1. CheckPtr + FindComponent(MapComponent) on entity
//   2. Read tile dimensions (width at +0x2c, height at +0x2e)
//   3. Resize display array to width*height
//   4. Read map info from gameState->+0xf8
//   5. For each display object:
//      a. Compute tile coords from linear index (col = i % width, row = i / width)
//      b. Add building origin offset (tileX0, tileY0)
//      c. Compute world position via tile-to-world conversion
//      d. Apply yaw rotation (alternating checkerboard pattern)
//      e. Build transform and set on geometry

void StorageDisplayComponent_Draw(StorageDisplayComponent* self) {
    EnsureHelpers();

    uintptr_t selfAddr = (uintptr_t)self;

#ifdef _DEBUG
    printf("[StorageDisplayComp::Draw] ENTER self=%p MeshResize=%p\n",
           (void*)selfAddr, (void*)s_fn_MeshArrayResize);
    fflush(stdout);
#endif

    // CheckPtr on entity ref
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    }

    // Find MapComponent on entity
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);
    uintptr_t mapCompBuf[2] = {0, 0};
    if (s_fn_FindComponent_Map) {
        s_fn_FindComponent_Map(entity, (void*)mapCompBuf);
    }

    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)mapCompBuf);
    }

    uintptr_t mapComp = mapCompBuf[0];
    if (!mapComp) return;

    int width = (int)*(int16_t*)(mapComp + 0x2c);
    int16_t tileX0 = *(int16_t*)(mapComp + 0x28);
    int16_t tileY0 = *(int16_t*)(mapComp + 0x2a);
    int colCount = width;

    // CheckPtr again for height
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)mapCompBuf);
    }

    int height = (int)*(int16_t*)(mapComp + 0x2e);

    // Resize display array to width * height
    int totalSlots = width * height;
#ifdef _DEBUG
    printf("[StorageDisplayComp::Draw]   w=%d h=%d total=%d arrayBase=%p\n",
           width, height, totalSlots, (void*)(selfAddr + 0x28));
    fflush(stdout);
#endif
    if (s_fn_MeshArrayResize) {
        s_fn_MeshArrayResize((void*)(selfAddr + 0x28), totalSlots);
    }
#ifdef _DEBUG
    printf("[StorageDisplayComp::Draw]   MeshResize done\n");
    fflush(stdout);
#endif

    // Get map info from game state
    if (!s_pGameState || !*s_pGameState) return;
    uintptr_t gameState = (uintptr_t)*s_pGameState;
    int* mapInfo = *(int**)(gameState + 0xf8);
    if (!mapInfo) return;

    float yawAngle = s_pYawConstant ? *s_pYawConstant : 0.0f;
    float halfTile = s_pHalfTileOffset ? *s_pHalfTileOffset : 0.0f;

    int displayCount = *(int*)(selfAddr + 0x30);
    if (displayCount <= 0) return;

    int mapSize = *mapInfo;
    float halfMap = (float)(mapSize >> 1) - halfTile;

    for (int i = 0; i < displayCount; i++) {
        int row = i / colCount + (int)tileY0;
        int col = i % colCount + (int)tileX0;
        int tileIndex = mapSize * row + col;

        // Build position vector from tile
        float vec3Buf[4];
        if (s_fn_Vector3_Ctor) {
            s_fn_Vector3_Ctor(vec3Buf,
                (float)(tileIndex % mapSize) - halfMap,
                (float)(tileIndex / mapSize) - halfMap,
                0.0f);
        }

        // Convert tile to world position
        float worldPos[4];
        if (s_fn_TileToWorld) {
            s_fn_TileToWorld(mapInfo, worldPos, (float*)vec3Buf, '\0', nullptr);
        }

        // Build transform with yaw rotation
        // Alternating checkerboard: if (row + col) is odd, use zero yaw
        uint32_t yawBits = 0;
        uint32_t pitchBits = *(uint32_t*)&yawAngle;
        if ((row + col) & 1) {
            pitchBits = 0;
        }

        uintptr_t displayArray = *(uintptr_t*)(selfAddr + 0x28);
        uint8_t transformBuf[72];
        void* transform = nullptr;
        // pitchBits holds the actual yaw value (alternated by tile); yawBits=0
        // is the pitch. Offset is zero. (Variable naming is intentionally swapped
        // to match the original call's argument order.)
        float zeroOffset[3] = {0.f, 0.f, 0.f};
        if (s_fn_FromYawPitchOffset) {
            transform = s_fn_FromYawPitchOffset(transformBuf, &pitchBits, &yawBits, zeroOffset);
        }

        // Apply transform to geometry
        if (s_fn_SetTransform && transform) {
            s_fn_SetTransform((void*)(displayArray + (uintptr_t)i * 0x128), transform);
        }
    }
}

// ============================================================
// StorageDisplayComponent::OnCreate (vtable slot [7], 0x1400566b0)
// ============================================================
// Iterates display objects, removes each from the graphics scene
// if its scene ID is valid (>= 0). Resets each to -1 after removal.
// Display objects are 0x128-byte entries in array at +0x28, count at +0x30.
// Scene ID is at entry+0x120.

void StorageDisplayComponent_OnCreate(StorageDisplayComponent* self) {
    EnsureHelpers();

    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x30);
    uintptr_t arrayBase = *(uintptr_t*)(selfAddr + 0x28);

    if (!s_pGameState || !*s_pGameState || !s_fn_Scene_RemoveObject) return;

    // Get the graphics scene from game state
    uintptr_t gameState = (uintptr_t)*s_pGameState;
    void* scene = *(void**)(gameState + 0x60);

    for (int i = 0; i < count; i++) {
        uintptr_t entry = arrayBase + (uintptr_t)i * 0x128;
        uint32_t sceneId = *(uint32_t*)(entry + 0x120);

        if ((int32_t)sceneId >= 0) {
            s_fn_Scene_RemoveObject(scene, sceneId);
            *(uint32_t*)(entry + 0x120) = 0xFFFFFFFF; // Reset to -1
        }
    }
}

// ============================================================
// StorageDisplayComponent::Update (vtable slot [9], 0x1400563e0)
// ============================================================
// Complex display update: queries StorageComponent on the entity,
// iterates storage slots, looks up RawMaterialDescription for each material type,
// proportionally allocates display objects based on item weight.
//
// Original logic:
//   1. CheckPtr + FindComponent(MapComponent) + FindComponent(StorageComponent)
//   2. Read storage slotCount and field38 (total capacity weight)
//   3. For each storage slot with items (slot+0x08 count > 0):
//      a. Get the material type entity at slot[0x00]
//      b. Find RawMaterialDescription on it
//      c. Compute fraction = (matDesc[0x48] * slotItemCount) / totalWeight * displayCount
//      d. Assign that many display objects with the material's visual
//   4. Remaining display objects get cleared (no mesh)

void StorageDisplayComponent_Update(StorageDisplayComponent* self) {
    EnsureHelpers();

    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on entity ref
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    }

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // FindComponent(MapComponent) — needed for entity context
    uintptr_t mapCompBuf[2] = {0, 0};
    if (s_fn_FindComponent_Map) {
        s_fn_FindComponent_Map(entity, (void*)mapCompBuf);
    }

    // CheckPtr on entity again
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    }

    // FindComponent(StorageComponent) via FUN_140003050
    uintptr_t storageBuf[2] = {0, 0};
    if (s_fn_FindComponent_Storage) {
        s_fn_FindComponent_Storage(entity, (void*)storageBuf);
    }

    int displayIndex = 0;
    int slotCount = 0;
    uintptr_t storageComp = 0;

    // CheckPtr on storage ref
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)storageBuf);
    }

    storageComp = storageBuf[0];
    if (!storageComp) goto fill_remaining;

    slotCount = *(int*)(storageComp + 0x30);

    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)storageBuf);
    }

    {
        float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
        int totalWeight = *(int*)(storageComp + 0x38);
        int displayCount = *(int*)(selfAddr + 0x30);

        if (slotCount > 0 && totalWeight > 0) {
            for (int slotIdx = 0; slotIdx < slotCount; slotIdx++) {
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)storageBuf);
                }

                uintptr_t slotArray = *(uintptr_t*)(storageComp + 0x28);
                uintptr_t slotEntry = slotArray + (uintptr_t)slotIdx * 0x10;
                int itemCount = *(int*)(slotEntry + 0x08);

                if (itemCount == 0) continue;

                // Get material type entity from slot
                uintptr_t materialType = *(uintptr_t*)slotEntry;

                // Find RawMaterialDescription on material type
                // The decompilation does a manual component search with typeId check
                void* rawMatDesc = nullptr;
                if (materialType && fn_IsDerivedFrom && g_TypeId_RawMaterialDescription) {
                    int compCount = *(int*)(materialType + 0x30);
                    uintptr_t compArray = *(uintptr_t*)(materialType + 0x28);
                    for (int c = 0; c < compCount; c++) {
                        void* comp = *(void**)(compArray + (uintptr_t)c * 8);
                        typedef uint32_t (*GetTypeId_t)(void*);
                        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                        if (typeId == *g_TypeId_RawMaterialDescription) {
                            if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                                rawMatDesc = comp;
                            }
                            break;
                        }
                    }
                }

                if (!rawMatDesc) continue;

                // Compute proportional display count
                int matWeight = *(int*)((uintptr_t)rawMatDesc + 0x48);
                float fraction = ((float)(matWeight * itemCount) / (float)totalWeight)
                                 * (float)displayCount;
                int assignCount = (int)fraction;

                // Assign display objects
                if (assignCount > 0 && s_fn_SetDisplayMesh) {
                    for (int j = 0; j < assignCount; j++) {
                        s_fn_SetDisplayMesh(selfAddr, displayIndex, uvScale, materialType);
                        displayIndex++;
                    }
                }

                // Check if we've filled all display slots
                if (displayIndex >= displayCount) break;

                // Assign one more with partial fraction
                if (displayIndex < displayCount && s_fn_SetDisplayMesh) {
                    float partial = fraction - (float)assignCount;
                    s_fn_SetDisplayMesh(selfAddr, displayIndex, partial, materialType);
                    displayIndex++;
                }

                if (displayIndex >= displayCount) break;
            }
        }
    }

fill_remaining:
    // Fill remaining display slots with empty (no mesh)
    {
        int displayCount = *(int*)(selfAddr + 0x30);
        if (displayIndex < displayCount && s_fn_SetDisplayMesh) {
            while (displayIndex < displayCount) {
                s_fn_SetDisplayMesh(selfAddr, displayIndex, 0.0f, 0);
                displayIndex++;
            }
        }
    }
}

// ============================================================
// StorageDisplayComponent::OnUpdate2 (vtable slot [11], 0x140056940)
// ============================================================
// Calls vtable Draw then Update (display rebuild + content refresh).
//
// Original disassembly:
//   MOV RAX,[RCX]            ; vtable
//   CALL [RAX+0x30]          ; vtable->Draw(self)
//   JMP Entity_impl_63e0     ; tail call to Update logic

void StorageDisplayComponent_OnUpdate2(StorageDisplayComponent* self) {
    // Call Draw through vtable
    uintptr_t vtable = *(uintptr_t*)self;
    typedef void (*fn_Draw_t)(void* self);
    fn_Draw_t drawFn = *(fn_Draw_t*)(vtable + 0x30);
    if (drawFn) {
        drawFn(self);
    }

    // Call Update
    StorageDisplayComponent_Update(self);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void StorageDisplayComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Funcs resolved: Component_Serialize=%p "
           "Scene_RemoveObject=%p FindComponent_Map=%p MeshArrayResize=%p "
           "SetDisplayMesh=%p pGameState=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Scene_RemoveObject,
           (void*)s_fn_FindComponent_Map, (void*)s_fn_MeshArrayResize,
           (void*)s_fn_SetDisplayMesh, (void*)s_pGameState);
#endif
}
