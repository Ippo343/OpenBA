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

#include "Component/storagedisplaycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)

// ============================================================
// Pile-rendering bring-up (BANISHED_LIVE_STORAGEDISPLAY, default OFF)
// ============================================================
// The StorageYard/StorageCart carry a StorageDisplayComponent that renders visible
// stacks of the stored raw materials. In standalone this component is spawned inert,
// so nothing draws. The factory at the bottom of this file graduates the three draw
// vtable slots (Draw/Update/OnUpdate2) when BANISHED_LIVE_STORAGEDISPLAY is set; the
// helpers below make the two raw-.rdata type-ids (ModelDescription, RawMaterial-
// Description) resolvable in SA and provide a throttled diagnostic trace.


// ModelDescription type-id (DAT_1401c7a88) — used by cbd0 to find a material entity's
// display model. The raw .rdata slot resolves NULL in the standalone exe, so fall
// back to the runtime type registry (valid after
// Object::Factory::Startup). Never writes the shared .rdata global.
static uint32_t* s_pModelDescTid_rdata = nullptr;
static bool      s_modelDescRdataResolved = false;
static uint32_t SDD_ModelDescTypeId() {
    if (!s_modelDescRdataResolved) {
        s_pModelDescTid_rdata = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a88));
        s_modelDescRdataResolved = true;
    }
    if (s_pModelDescTid_rdata && *s_pModelDescTid_rdata != 0 && *s_pModelDescTid_rdata != 0xFFFFFFFF)
        return *s_pModelDescTid_rdata;
    static uint32_t cached = 0;
    if (cached == 0 || cached == 0xFFFFFFFF) cached = ::Object::GetTypeIdByName("ModelDescription");
    if (cached != 0xFFFFFFFF) return cached;
    return 0;
}

// RawMaterialDescription type-id (DAT_1401c7cb8, exposed as g_TypeId_RawMaterialDescription)
// — used by Update to size each material's pile. Same fallback as above.
static uint32_t SDD_RawMatDescTypeId() {
    if (g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription != 0
        && *g_TypeId_RawMaterialDescription != 0xFFFFFFFF)
        return *g_TypeId_RawMaterialDescription;
    static uint32_t cached = 0;
    if (cached == 0 || cached == 0xFFFFFFFF) cached = ::Object::GetTypeIdByName("RawMaterialDescription");
    if (cached != 0xFFFFFFFF) return cached;
    return 0;
}

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

// CharacterComponent_impl_f3e0 — map tile to world position
typedef void (*fn_TileToWorld_t)(void* mapInfo, float* outPos, float* inPos, char param4, void* param5);
static fn_TileToWorld_t s_fn_TileToWorld = nullptr;

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

    s_fn_TileToWorld = reinterpret_cast<fn_TileToWorld_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));  // CharacterComponent_impl_f3e0 tile->world (CALL @140056869 in StorageDisplayComponent::Draw)


    slot = ResolveAddr(Module::Exe, 0x1401512e0);
    if (slot) s_fn_SetTransform = *reinterpret_cast<fn_SetTransform_t*>(slot);

    // Entity_FindComponent_Storage (0x140006270) — finds the entity's StorageComponent
    // (type DWORD_1401c7e78). Verified in the real Update disassembly at 0x1400563e0:
    // the CALL at 0x140056432 targets 0x140006270, NOT 0x140003050. (0x140003050 finds
    // type 0x1401c8070 = WorkComponent — which the yard ALSO carries, so the old wiring
    // silently found the WorkComponent and read StorageComponent field offsets off it,
    // an out-of-bounds read. Registered in SA as WorkBase_FindComponent_6270.)
    s_fn_FindComponent_Storage = reinterpret_cast<fn_FindComponent_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));

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

    // Find MapComponent on entity
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);
    uintptr_t mapCompBuf[2] = {0, 0};
    if (s_fn_FindComponent_Map) {
        s_fn_FindComponent_Map(entity, (void*)mapCompBuf);
    }


    uintptr_t mapComp = mapCompBuf[0];
    if (!mapComp) return;

    int width = (int)*(int16_t*)(mapComp + 0x2c);
    int16_t tileX0 = *(int16_t*)(mapComp + 0x28);
    int16_t tileY0 = *(int16_t*)(mapComp + 0x2a);
    int colCount = width;

    // CheckPtr again for height

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

    // Real-value fallbacks: these raw .rdata floats resolve NULL in standalone
    // (ResolveAddr on a .rdata constant returns null), so use the true retail values
    // instead of 0 — else piles shift by a half-tile and lose the checkerboard yaw.
    //   DAT_140172204 = 0x42b40000 = 90.0f (per-cell checkerboard yaw, degrees)
    //   DAT_140172114 = 0x3f000000 = 0.5f  (half-tile center bias)
    float yawAngle = (s_pYawConstant   && *s_pYawConstant   != 0.0f) ? *s_pYawConstant   : 90.0f;
    float halfTile = (s_pHalfTileOffset && *s_pHalfTileOffset != 0.0f) ? *s_pHalfTileOffset : 0.5f;

    int displayCount = *(int*)(selfAddr + 0x30);
    if (displayCount <= 0) return;

    int mapSize = *mapInfo;
    float halfMap = (float)(mapSize >> 1) - halfTile;

    // Anchor trace: the yard's OWN world position (tile->world of its origin cell), so
    // per-pile world positions below can be compared against where the yard sits.

    for (int i = 0; i < displayCount; i++) {
        int row = i / colCount + (int)tileY0;
        int col = i % colCount + (int)tileX0;
        int tileIndex = mapSize * row + col;

        // Build position vector from tile
        Math::Vector3 vec3Buf((float)(tileIndex % mapSize) - halfMap,
                              (float)(tileIndex / mapSize) - halfMap,
                              0.0f);

        // Convert tile to world position (f3e0). worldPos[0]/[1] are the world X/Y
        // on the yard footprint; the original drops worldPos[2] and uses z=0.
        float worldPos[4] = {0.f, 0.f, 0.f, 0.f};
        if (s_fn_TileToWorld) {
            s_fn_TileToWorld(mapInfo, worldPos, &vec3Buf.x, '\0', nullptr);
        }

        // Build transform with yaw rotation
        // Alternating checkerboard: if (row + col) is odd, use zero yaw
        uint32_t yawBits = 0;
        uint32_t pitchBits = *(uint32_t*)&yawAngle;
        if ((row + col) & 1) {
            pitchBits = 0;
        }

        uintptr_t displayArray = *(uintptr_t*)(selfAddr + 0x28);
        // FromYawPitchOffset(retbuf, yaw=&pitchBits, pitch=&yawBits[=0], offset).
        // (Names are swapped to match the original's arg order.) The OFFSET is the
        // per-cell world position {worldPos.x, worldPos.y, 0} — verified in the real
        // Draw disassembly (0x1400568c9/cf write worldPos.x/.y into the offset vec,
        // 0x1400568ba forces offset.z=0). The prior port passed a ZERO offset here,
        // which placed every pile mesh at the world origin (invisible on the yard).
        Math::Vector3 offset(worldPos[0], worldPos[1], 0.0f);
        Math::Transform transform = Math::Transform::FromYawPitchOffset(
            *reinterpret_cast<const Math::Scalar*>(&pitchBits),
            *reinterpret_cast<const Math::Scalar*>(&yawBits), offset);   // B.3.1 bridge

        // Apply transform to geometry
        if (s_fn_SetTransform) {
            s_fn_SetTransform((void*)(displayArray + (uintptr_t)i * 0x128), &transform);
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

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // FindComponent(MapComponent) — needed for entity context
    uintptr_t mapCompBuf[2] = {0, 0};
    if (s_fn_FindComponent_Map) {
        s_fn_FindComponent_Map(entity, (void*)mapCompBuf);
    }

    // CheckPtr on entity again

    // FindComponent(StorageComponent) via FUN_140003050
    uintptr_t storageBuf[2] = {0, 0};
    if (s_fn_FindComponent_Storage) {
        s_fn_FindComponent_Storage(entity, (void*)storageBuf);
    }

    int displayIndex = 0;
    int slotCount = 0;
    uintptr_t storageComp = 0;

    // CheckPtr on storage ref

    storageComp = storageBuf[0];
    if (!storageComp) goto fill_remaining;

    slotCount = *(int*)(storageComp + 0x30);


    {
        float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
        int totalWeight = *(int*)(storageComp + 0x38);
        int displayCount = *(int*)(selfAddr + 0x30);
        const uint32_t rawMatTid = SDD_RawMatDescTypeId();

        if (slotCount > 0 && totalWeight > 0) {
            for (int slotIdx = 0; slotIdx < slotCount; slotIdx++) {

                uintptr_t slotArray = *(uintptr_t*)(storageComp + 0x28);
                uintptr_t slotEntry = slotArray + (uintptr_t)slotIdx * 0x10;
                int itemCount = *(int*)(slotEntry + 0x08);

                if (itemCount == 0) continue;

                // Get material type entity from slot
                uintptr_t materialType = *(uintptr_t*)slotEntry;

                // Find RawMaterialDescription on material type
                // The decompilation does a manual component search with typeId check.
                // (SA: rawMatTid comes from the runtime registry fallback — see helper.)
                void* rawMatDesc = nullptr;
                if (materialType && fn_IsDerivedFrom && rawMatTid) {
                    int compCount = *(int*)(materialType + 0x30);
                    uintptr_t compArray = *(uintptr_t*)(materialType + 0x28);
                    for (int c = 0; c < compCount; c++) {
                        void* comp = *(void**)(compArray + (uintptr_t)c * 8);
                        if (!comp) continue;   // guard partially-materialized SA entity
                        typedef uint32_t (*GetTypeId_t)(void*);
                        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                        if (typeId == rawMatTid) {
                            if (fn_IsDerivedFrom(comp, rawMatTid)) {
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
// Reimpl of FUN_14000ca10 (StorageDisplayComponent mesh-array resize:
//   grow/shrink an array of 0x128-byte Graphics::MeshInstance entries)
// ============================================================
// Dispatch slots (exe .rdata): copy-ctor 0x1401514a0, default-ctor 0x140151490,
// dtor 0x1401514a8. New tail entries get default-constructed with sceneId(+0x120)=-1.
typedef void (*fn_MI_CopyCtor_t)(void* dst, void* src);
typedef void (*fn_MI_DefCtor_t)(void* dst);
typedef void (*fn_MI_Dtor_t)(void* self);
static fn_MI_CopyCtor_t s_fn_MI_CopyCtor = nullptr;
static fn_MI_DefCtor_t  s_fn_MI_DefCtor  = nullptr;
static fn_MI_Dtor_t     s_fn_MI_Dtor     = nullptr;
static bool s_meshArrayHelpersResolved = false;
static void EnsureMeshArrayHelpers() {
    if (s_meshArrayHelpersResolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401514a0);
    if (slot) s_fn_MI_CopyCtor = *reinterpret_cast<fn_MI_CopyCtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151490);
    if (slot) s_fn_MI_DefCtor = *reinterpret_cast<fn_MI_DefCtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x1401514a8);
    if (slot) s_fn_MI_Dtor = *reinterpret_cast<fn_MI_Dtor_t*>(slot);
    s_meshArrayHelpersResolved = true;
}

void StorageDisplayComponent_MeshArrayResize(void* arrayBase, int newCount) {
    EnsureMeshArrayHelpers();
    const int64_t STRIDE = 0x128;
    int64_t* field = (int64_t*)arrayBase;     // [0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int64_t* oldBuf = (int64_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) * STRIDE), 8);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int64_t*)field[0]) ? oldCount : 0;
    if (start < newCount) {
        int64_t off = (int64_t)start * STRIDE;
        for (int i = start; i < newCount; i++) {
            void* dst = (void*)(field[0] + off);
            if (dst) {
                if (i < oldCount) {
                    if (s_fn_MI_CopyCtor) s_fn_MI_CopyCtor(dst, (void*)((uintptr_t)oldBuf + off));
                    *(uint32_t*)((uintptr_t)dst + 0x120) = *(uint32_t*)((uintptr_t)oldBuf + off + 0x120);
                } else {
                    if (s_fn_MI_DefCtor) s_fn_MI_DefCtor(dst);
                    *(uint32_t*)((uintptr_t)dst + 0x120) = 0xffffffff;
                }
            }
            off += STRIDE;
        }
    }

    if (oldBuf == (int64_t*)field[0]) {
        if (newCount < oldCount) {
            uintptr_t p = (uintptr_t)oldBuf + (uintptr_t)newCount * STRIDE;
            for (int i = 0; i < oldCount - newCount; i++) {
                if (s_fn_MI_Dtor) s_fn_MI_Dtor((void*)p);
                p += STRIDE;
            }
        }
    } else {
        uintptr_t p = (uintptr_t)oldBuf;
        for (int i = 0; i < oldCount; i++) {
            if (s_fn_MI_Dtor) s_fn_MI_Dtor((void*)p);
            p += STRIDE;
        }
        if (oldBuf) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_1400562b0 (Entity_impl_63e0_sub_62b0:
//   set/clear the mesh + scene-object for one display slot)
// ============================================================
// self+0x28 = display array (0x128 stride), self+0x30 = count. Scene id at entry+0x120,
// current mesh at entry+0xc0. param_4==0 => remove the object; otherwise pick the LOD
// mesh nearest the requested fraction and (re)add it to the scene.
typedef void (*fn_SDC_SceneRemove_t)(void* scene, uint32_t objId);
typedef void* (*fn_SDC_EntityHelperCbd0_t)(void* param4, void* retbuf);
typedef void (*fn_SDC_SetMesh_t)(void* meshInstance, int64_t meshPtr);
typedef uint32_t (*fn_SDC_SceneAdd_t)(void* scene, void* geometry);
static fn_SDC_SceneRemove_t      s_fn_SDC_SceneRemove = nullptr;
static fn_SDC_EntityHelperCbd0_t s_fn_SDC_Cbd0 = nullptr;
static fn_SDC_SetMesh_t          s_fn_SDC_SetMesh = nullptr;
static fn_SDC_SceneAdd_t         s_fn_SDC_SceneAdd = nullptr;
static bool s_sdcSlotHelpersResolved = false;
static void EnsureSlotHelpers() {
    if (s_sdcSlotHelpersResolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140151360);
    if (slot) s_fn_SDC_SceneRemove = *reinterpret_cast<fn_SDC_SceneRemove_t*>(slot);
    s_fn_SDC_Cbd0 = reinterpret_cast<fn_SDC_EntityHelperCbd0_t>(
        ResolveAddr(Module::Exe, 0x14000cbd0));
    slot = ResolveAddr(Module::Exe, 0x140151498);
    if (slot) s_fn_SDC_SetMesh = *reinterpret_cast<fn_SDC_SetMesh_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151370);
    if (slot) s_fn_SDC_SceneAdd = *reinterpret_cast<fn_SDC_SceneAdd_t*>(slot);
    s_sdcSlotHelpersResolved = true;
}

// ============================================================
// Reimpl of Entity_helper_cbd0 (0x14000cbd0) — Component reg wave B.
// Scans a host entity's component list (count@+0x30, IBP array@+0x28) for the first
// component whose runtime type id (vtable+0x10) equals DAT_1401c7a88, then resolves
// that IBP and, if it IsDerivedFrom that type, writes it to *retbuf (else null).
// Returns retbuf. Closure-safe: only IsDerivedFrom (engine). The type-id is a .rdata
// global resolved at call time. Faithful 1:1 port of the Ghidra decompile.
// ============================================================
void* StorageDisplay_EntityFindByType_cbd0(void* hostP, void* retbuf) {
    // DAT_1401c7a88 = ModelDescription. Resolved via the runtime registry
    // fallback (SDD_ModelDescTypeId).
    const uint32_t modelTid = SDD_ModelDescTypeId();
    uint8_t* host = reinterpret_cast<uint8_t*>(hostP);
    void* found = nullptr;
    if (host && modelTid) {
        int count = *reinterpret_cast<int*>(host + 0x30);
        int64_t arr = *reinterpret_cast<int64_t*>(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *reinterpret_cast<void**>(arr + (int64_t)i * 8);
            if (!comp) continue;   // guard partially-materialized SA entity
            int64_t vt = *reinterpret_cast<int64_t*>(comp);
            typedef uint32_t (*getType_t)(void*);
            uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(comp);
            if (tid == modelTid) {
                if (fn_IsDerivedFrom(comp, modelTid)) found = comp;
                break;
            }
        }
    }
    *reinterpret_cast<void**>(retbuf) = found;
    return retbuf;
}

void StorageDisplayComponent_SetDisplayMesh(uintptr_t self, int index, float fraction,
                                            uintptr_t materialType) {
    EnsureSlotHelpers();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;

    if (index >= *(int*)(self + 0x30)) return;
    void* scene = *(void**)(gw + 0x60);
    int64_t off = (int64_t)index * 0x128;
    uintptr_t arrBase = *(uintptr_t*)(self + 0x28);

    if (materialType == 0) {
        uint32_t objId = *(uint32_t*)(arrBase + 0x120 + off);
        if ((int32_t)objId >= 0) {
            if (s_fn_SDC_SceneRemove) s_fn_SDC_SceneRemove(scene, objId);
            *(uint32_t*)(arrBase + 0x120 + off) = 0xffffffff;
        }
        return;
    }

    // materialType != 0: select the LOD mesh nearest the requested fraction.
    // Every deref below is null-guarded so a partially-materialized SA material
    // entity (no ModelDescription / empty MeshGroup) can't fault SetMesh — it just
    // renders no pile for that slot instead of crashing the shared Graphics::Scene.
    int64_t cbd0Ret = 0;
    if (s_fn_SDC_Cbd0) s_fn_SDC_Cbd0((void*)materialType, &cbd0Ret);
    if (!cbd0Ret) return;                                   // material has no ModelDescription
    uintptr_t lodTable = *(uintptr_t*)(cbd0Ret + 0x20);
    if (!lodTable) return;                                  // ModelDescription has no MeshGroup
    int lodCount = *(int*)(lodTable + 0x28);
    if (lodCount <= 0) return;                              // empty LOD table
    int lodIdx = lodCount - 1;
    int scaled = (int)((float)lodCount * fraction);
    if (scaled < lodIdx) lodIdx = scaled;
    if (lodIdx < 0) lodIdx = 0;                             // clamp (fraction may be ~0)
    uintptr_t meshArr = *(uintptr_t*)(lodTable + 0x20);
    if (!meshArr) return;
    int64_t meshPtr = *(int64_t*)(meshArr + (int64_t)lodIdx * 8);
    if (!meshPtr) return;                                   // no mesh at this LOD

    arrBase = *(uintptr_t*)(self + 0x28);
    uint32_t objId = *(uint32_t*)(arrBase + 0x120 + off);
    if ((int32_t)objId >= 0) {
        if (*(int64_t*)(arrBase + 0xc0 + off) == meshPtr) {
            return;   // already showing this mesh (steady-state early-out)
        }
        if (s_fn_SDC_SceneRemove) s_fn_SDC_SceneRemove(scene, objId);
    }
    if (s_fn_SDC_SetMesh) s_fn_SDC_SetMesh((void*)(arrBase + off), meshPtr);
    void* geom = (void*)(arrBase + off);
    uint32_t newId = s_fn_SDC_SceneAdd ? s_fn_SDC_SceneAdd(scene, geom) : 0xffffffff;
    *(uint32_t*)((uintptr_t)geom + 0x120) = newId;

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

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// StorageYard/StorageCart resource-pile rendering (BANISHED_LIVE_STORAGEDISPLAY,
// default OFF unless SA_LIVE_ENV enables it).
//
// DRIVER: faithful — the pile refresh is the component's own per-frame Update (+0x48),
// dispatched natively by EntityList::Update's 5-bucket round-robin (see entitylist.cpp
// Step 4). That tick was previously believed dead in SA and replaced by an explicit
// per-frame drain; it is in fact alive (processCount is set, buckets are populated). The
// one thing that blocked THIS component was its owner StorageYard being wrongly stamped
// with entity flag bit 0x400 ("removed from world") by StartTown_RunOnCreatePass — fixed
// there. With that clear, the bucket tick calls this component's vtable +0x48 (slot 9 =
// TickVSlot below = Draw+Update) every cadence, exactly like retail. No explicit driver.
//
// The live vtable wires the real bodies into slots 6/9/11 (Draw / Update / OnUpdate2).
// Slot 9 is the active native driver; 6 and 11 back any incidental vtable dispatch.
static uint32_t StorageDisplayComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageDisplayComponent");
    return cached;
}
static void* StorageDisplayComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// index 6 — Draw(self): grid position layout.
static void StorageDisplayComponent_DrawVSlot(void* s) {
    StorageDisplayComponent_Draw(reinterpret_cast<StorageDisplayComponent*>(s));
}
// index 9 — Update tick (self, Time*): the NATIVE +0x48 driver dispatched by the
// EntityList 5-bucket round-robin. Draw lays out the w*h pile grid; Update allocates the
// pile meshes from the (seeded/live) StorageComponent contents. Both early-out cheaply
// when unchanged, so steady-state cost is a handful of yards * a few slots per cadence.
static void StorageDisplayComponent_TickVSlot(void* s, void* /*time*/) {
    StorageDisplayComponent* self = reinterpret_cast<StorageDisplayComponent*>(s);
    StorageDisplayComponent_Draw(self);
    StorageDisplayComponent_Update(self);
}
// index 11 — OnUpdate2(self): retail Draw+Update refresh (event/broadcast path).
static void StorageDisplayComponent_OnUpdate2VSlot(void* s) {
    StorageDisplayComponent_OnUpdate2(reinterpret_cast<StorageDisplayComponent*>(s));
}
// index 7 (+0x38) — removed-from-world: Scene::RemoveObject every pile-display
// entry (ids at entry+0x120). Entity teardown dispatches this via MarkForRemoval
// (0x1400c2ea0: for each component with capability bit 1 — our flags 0xCB have
// it — call vtable+0x38, THEN set the 0x400 bit). Leaving this slot inert
// crashed live 2026-07-20: demolishing a building freed the component while its
// pile meshes stayed registered in the renderer's draw list, and the next
// GameUpdateLoop::Draw died in Renderer::DrawItemList -> EdgeMeshInstance::Draw
// -> MeshInstance::GetNodeToWorld -> Transform::Lerp on the freed entries.
// Wired UNCONDITIONALLY (not only when the display is live): the handler
// no-ops on an empty/never-populated display array, and teardown safety must
// not depend on the display feature flag.
static void StorageDisplayComponent_OnRemovedVSlot(void* s) {
    StorageDisplayComponent_OnCreate(reinterpret_cast<StorageDisplayComponent*>(s));
}
static void StorageDisplayComponent_SerSlot3(void* s, void* stream) {
    StorageDisplayComponent_Shutdown(reinterpret_cast<StorageDisplayComponent*>(s),
                                     stream);
}
static void StorageDisplayComponent_FinishLiveVtable() {
    // Slot 3 (+0x18), UNCONDITIONAL — serializer wiring is not a feature flag.
    // Retail 0x140056970 is Component::Serialize plus one empty Serializer scope,
    // and an empty scope is still 16 real bytes in the save encoding. Without this
    // the ported body below was dead code and every record was 16 bytes short,
    // desyncing the stream for every object deserialized after it.
    StorageDisplayComponent_LiveVtable[3]  =
        reinterpret_cast<void*>(&StorageDisplayComponent_SerSlot3);
    StorageDisplayComponent_LiveVtable[7]  =
        reinterpret_cast<void*>(&StorageDisplayComponent_OnRemovedVSlot);
    StorageDisplayComponent_LiveVtable[6]  =
        reinterpret_cast<void*>(&StorageDisplayComponent_DrawVSlot);
    StorageDisplayComponent_LiveVtable[9]  =
        reinterpret_cast<void*>(&StorageDisplayComponent_TickVSlot);
    StorageDisplayComponent_LiveVtable[11] =
        reinterpret_cast<void*>(&StorageDisplayComponent_OnUpdate2VSlot);
}
extern "C" void* StorageDisplayComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0xCB,
        StorageDisplayComponent_LiveVtable,
        reinterpret_cast<void*>(&StorageDisplayComponent_LiveGetTypeId));
    StorageDisplayComponent_FinishLiveVtable();
    return p;
}
static void* StorageDisplayComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0xCB,
        StorageDisplayComponent_LiveVtable,
        reinterpret_cast<void*>(&StorageDisplayComponent_LiveGetTypeId));
    StorageDisplayComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(StorageDisplayComponent, "Component",
              &StorageDisplayComponent_Factory, &StorageDisplayComponent_CreateInPlace);

