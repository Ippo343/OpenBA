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

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/RawMaterialDescription.h"
#include "Component/ModelDescription.h"
#include "graphics/Mesh.h"
#include "Component/storagedisplaycomponent_funcs.h"
#include "object/External.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "resolve.h"
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"   // WorkBase_FindComponent_6270
#include "Game/exe_constants.h"

// ============================================================
// Pile-rendering bring-up (BANISHED_LIVE_STORAGEDISPLAY, default OFF)
// ============================================================
// The StorageYard/StorageCart carry a StorageDisplayComponent that renders visible
// stacks of the stored raw materials. In standalone this component is spawned inert,
// so nothing draws. The factory at the bottom of this file graduates the three draw
// vtable slots (Draw/Update/OnUpdate2) when BANISHED_LIVE_STORAGEDISPLAY is set; the
// helpers below make the two raw-.rdata type-ids (ModelDescription, RawMaterial-
// Description) resolvable in SA and provide a throttled diagnostic trace.



// RawMaterialDescription type-id (cell 0x1401c7cb8) — used by Update to size each
// material's pile. TypeId() carries the ctor sentinel 0xFFFFFFFF until
// Factory::Startup runs (D6); treat that the same as unresolved.
static uint32_t SDD_RawMatDescTypeId() {
    const uint32_t id = RawMaterialDescription::TypeId();
    if (id != 0xFFFFFFFF) return id;
    return 0;
}

// Exe helpers, by retail address:
//   0x140004680 FloorComponent_FindResData (MapComponent)
//   0x14000ca10 StorageDisplayComponent_MeshArrayResize (below; 0x128-byte MeshInstances)
//   0x14009f3e0 Reimpl_MapGetTerrainPos (CharacterComponent_impl_f3e0, map tile -> world;
//               CALL @140056869 in StorageDisplayComponent::Draw)
//   0x140006270 WorkBase_FindComponent_6270 — Entity_FindComponent_Storage, the entity's
//               StorageComponent (type DWORD_1401c7e78). Verified in the real Update
//               disassembly at 0x1400563e0: the CALL at 0x140056432 targets 0x140006270,
//               NOT 0x140003050. (0x140003050 finds type 0x1401c8070 = WorkComponent —
//               which the yard ALSO carries, so the old wiring silently found the
//               WorkComponent and read StorageComponent field offsets off it, an
//               out-of-bounds read.)
//   0x1400562b0 StorageDisplayComponent_SetDisplayMesh (below)
//   0x14000cbd0 StorageDisplay_EntityFindByType_cbd0 (below)
//   the GameWorld cell 0x1401c3dd8 is g_pGameWorldSingleton; .rdata 0x140172204 = 90.0f
//   (checkerboard yaw), 0x140172114 = 0.5f (half-tile centre bias), 0x14017214c = 1.0f.

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

    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on entity ref

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // FindComponent(MapComponent) — needed for entity context
    uintptr_t mapCompBuf[2] = {0, 0};
    FloorComponent_FindResData((void*)entity, (void*)mapCompBuf);   // 0x140004680

    // CheckPtr on entity again

    // FindComponent(StorageComponent) via FUN_140006270
    uintptr_t storageBuf[2] = {0, 0};
    WorkBase_FindComponent_6270(entity, (void*)storageBuf);

    int displayIndex = 0;
    int slotCount = 0;
    uintptr_t storageComp = 0;

    // CheckPtr on storage ref

    storageComp = storageBuf[0];
    if (!storageComp) goto fill_remaining;

    slotCount = *(int*)(storageComp + 0x30);


    {
        float uvScale = kRdata_14017214c;   // g_fUVScale, .rdata 0x14017214c
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
                if (materialType && rawMatTid) {
                    int compCount = *(int*)(materialType + 0x30);
                    uintptr_t compArray = *(uintptr_t*)(materialType + 0x28);
                    for (int c = 0; c < compCount; c++) {
                        void* comp = *(void**)(compArray + (uintptr_t)c * 8);
                        if (!comp) continue;   // guard partially-materialized SA entity
                        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
                        if (typeId == rawMatTid) {
                            if (Object::IsDerivedFromNullSafe(comp, rawMatTid)) {
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
                if (assignCount > 0) {
                    for (int j = 0; j < assignCount; j++) {
                        StorageDisplayComponent_SetDisplayMesh(selfAddr, displayIndex, uvScale, materialType);
                        displayIndex++;
                    }
                }

                // Check if we've filled all display slots
                if (displayIndex >= displayCount) break;

                // Assign one more with partial fraction
                if (displayIndex < displayCount) {
                    float partial = fraction - (float)assignCount;
                    StorageDisplayComponent_SetDisplayMesh(selfAddr, displayIndex, partial, materialType);
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
        if (displayIndex < displayCount) {
            while (displayIndex < displayCount) {
                StorageDisplayComponent_SetDisplayMesh(selfAddr, displayIndex, 0.0f, 0);
                displayIndex++;
            }
        }
    }
}


// ============================================================
// Reimpl of FUN_14000ca10 (StorageDisplayComponent mesh-array resize:
//   grow/shrink an array of 0x128-byte Graphics::MeshInstance entries)
// ============================================================
// Retail calls the three MeshInstance members through its import slots
// (copy-ctor 0x1401514a0, default-ctor 0x140151490, dtor 0x1401514a8).
// ENDGAME B.3.9 GFX (B9-13): those are the real class's copy ctor, ctor and
// public dtor, spelled as C++ below; the slot resolutions are gone. New tail
// entries get default-constructed with sceneId(+0x120)=-1.
void StorageDisplayComponent_MeshArrayResize(void* arrayBase, int newCount) {
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
        void* nb = System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) * STRIDE), 8);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int64_t*)field[0]) ? oldCount : 0;
    if (start < newCount) {
        int64_t off = (int64_t)start * STRIDE;
        for (int i = start; i < newCount; i++) {
            void* dst = (void*)(field[0] + off);
            if (dst) {
                if (i < oldCount) {
                    ::new (dst) Graphics::MeshInstance(*reinterpret_cast<const Graphics::MeshInstance*>(
                        (uintptr_t)oldBuf + off));
                    *(uint32_t*)((uintptr_t)dst + 0x120) = *(uint32_t*)((uintptr_t)oldBuf + off + 0x120);
                } else {
                    ::new (dst) Graphics::MeshInstance();
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
                reinterpret_cast<Graphics::MeshInstance*>(p)->Graphics::MeshInstance::~MeshInstance();
                p += STRIDE;
            }
        }
    } else {
        uintptr_t p = (uintptr_t)oldBuf;
        for (int i = 0; i < oldCount; i++) {
            reinterpret_cast<Graphics::MeshInstance*>(p)->Graphics::MeshInstance::~MeshInstance();
            p += STRIDE;
        }
        if (oldBuf) System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_1400562b0 (Entity_impl_63e0_sub_62b0:
//   set/clear the mesh + scene-object for one display slot)
// ============================================================
// self+0x28 = display array (0x128 stride), self+0x30 = count. Scene id at entry+0x120,
// current mesh at entry+0xc0. param_4==0 => remove the object; otherwise pick the LOD
// mesh nearest the requested fraction and (re)add it to the scene.
// ============================================================
// Reimpl of Entity_helper_cbd0 (0x14000cbd0) — Component reg wave B.
// Scans a host entity's component list (count@+0x30, IBP array@+0x28) for the first
// component whose runtime type id (vtable+0x10) equals DAT_1401c7a88, then resolves
// that IBP and, if it IsDerivedFrom that type, writes it to *retbuf (else null).
// Returns retbuf. Closure-safe: only IsDerivedFrom (engine). The type-id is a .rdata
// global resolved at call time. Faithful 1:1 port of the Ghidra decompile.
// ============================================================
void* StorageDisplay_EntityFindByType_cbd0(void* hostP, void* retbuf) {
    // DAT_1401c7a88 = ModelDescription.
    const uint32_t modelTid = ModelDescription::TypeId();
    uint8_t* host = reinterpret_cast<uint8_t*>(hostP);
    void* found = nullptr;
    if (host && modelTid) {
        int count = *reinterpret_cast<int*>(host + 0x30);
        int64_t arr = *reinterpret_cast<int64_t*>(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *reinterpret_cast<void**>(arr + (int64_t)i * 8);
            if (!comp) continue;   // guard partially-materialized SA entity
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (tid == modelTid) {
                if (Object::IsDerivedFromNullSafe(comp, modelTid)) found = comp;
                break;
            }
        }
    }
    *reinterpret_cast<void**>(retbuf) = found;
    return retbuf;
}

void StorageDisplayComponent_SetDisplayMesh(uintptr_t self, int index, float fraction,
                                            uintptr_t materialType) {
    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gw = (uintptr_t)GameUpdateLoop::_singleton;

    if (index >= *(int*)(self + 0x30)) return;
    void* scene = *(void**)(gw + 0x60);
    int64_t off = (int64_t)index * 0x128;
    uintptr_t arrBase = *(uintptr_t*)(self + 0x28);

    if (materialType == 0) {
        uint32_t objId = *(uint32_t*)(arrBase + 0x120 + off);
        if ((int32_t)objId >= 0) {
            reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject(objId);   // B.3.5 bridge
            *(uint32_t*)(arrBase + 0x120 + off) = 0xffffffff;
        }
        return;
    }

    // materialType != 0: select the LOD mesh nearest the requested fraction.
    // Every deref below is null-guarded so a partially-materialized SA material
    // entity (no ModelDescription / empty MeshGroup) can't fault SetMesh — it just
    // renders no pile for that slot instead of crashing the shared Graphics::Scene.
    int64_t cbd0Ret = 0;
    StorageDisplay_EntityFindByType_cbd0((void*)materialType, &cbd0Ret);   // 0x14000cbd0
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
        reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject(objId);   // B.3.5 bridge
    }
    reinterpret_cast<Graphics::MeshInstance*>(arrBase + off)
        ->SetMesh(Graphics::ExternalPtrHandle{reinterpret_cast<void*>(meshPtr)});   // B.3.5 bridge
    void* geom = (void*)(arrBase + off);
    uint32_t newId = reinterpret_cast<Graphics::Scene*>(scene)
        ->AddObject(reinterpret_cast<Graphics::Geometry*>(geom));   // B.3.5 bridge
    *(uint32_t*)((uintptr_t)geom + 0x120) = newId;

}

// ============================================================================
// ENDGAME B.3.8 VIS — StorageDisplayComponent as a REAL class (StorageDisplayComponent.h).
//
// Before: a 48-slot StorageDisplayComponent_LiveVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/StorageDisplayComponent.h"

// DRIVER: faithful — the pile refresh is the component's own per-frame Update
// (+0x48), dispatched natively by EntityList::Update's 5-bucket round-robin
// (entitylist.cpp Step 4) every cadence, exactly like retail.

// 0x140056a00: Component() is the CALL 0x1401188b0; the 0x14015a780 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0xcb.
StorageDisplayComponent::StorageDisplayComponent()
    : displayData0(nullptr), displayData1(nullptr) {
    OrFlagsInCtor(0xcb);
}

// ============================================================
// StorageDisplayComponent::Persist (vtable slot [3], 0x140056970)
// ============================================================
// Simple: Component_Serialize + one empty Serializer block (version 0).

void StorageDisplayComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}
// [4] 0x140056930 — the class's own `RET` body.
void StorageDisplayComponent::Startup() {}
// ============================================================
// StorageDisplayComponent::OnAdded (vtable slot [6], 0x140056730)
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

void StorageDisplayComponent::OnAdded() {
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

#ifdef _DEBUG
    printf("[StorageDisplayComp::Draw] ENTER self=%p MeshResize=%p\n",
           (void*)selfAddr, (void*)&StorageDisplayComponent_MeshArrayResize);
    fflush(stdout);
#endif

    // CheckPtr on entity ref

    // Find MapComponent on entity
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);
    uintptr_t mapCompBuf[2] = {0, 0};
    FloorComponent_FindResData((void*)entity, (void*)mapCompBuf);   // 0x140004680


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
    StorageDisplayComponent_MeshArrayResize((void*)(selfAddr + 0x28), totalSlots);   // 0x14000ca10
#ifdef _DEBUG
    printf("[StorageDisplayComp::Draw]   MeshResize done\n");
    fflush(stdout);
#endif

    // Get map info from game state
    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gameState = (uintptr_t)GameUpdateLoop::_singleton;
    int* mapInfo = *(int**)(gameState + 0xf8);
    if (!mapInfo) return;

    //   DAT_140172204 = 0x42b40000 = 90.0f (per-cell checkerboard yaw, degrees)
    //   DAT_140172114 = 0x3f000000 = 0.5f  (half-tile center bias)
    float yawAngle = kRdata_140172204;
    float halfTile = kRdata_140172114;

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
        Reimpl_MapGetTerrainPos(mapInfo, worldPos, &vec3Buf.x, '\0', nullptr);   // 0x14009f3e0

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
        reinterpret_cast<Graphics::Geometry*>(displayArray + (uintptr_t)i * 0x128)
            ->SetTransform(transform);   // B.3.5 bridge

    }
}

// ============================================================
// StorageDisplayComponent::OnRemove (vtable slot [7], 0x1400566b0)
// ============================================================
// Iterates display objects, removes each from the graphics scene
// if its scene ID is valid (>= 0). Resets each to -1 after removal.
// Display objects are 0x128-byte entries in array at +0x28, count at +0x30.
// Scene ID is at entry+0x120.

void StorageDisplayComponent::OnRemove() {
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);
    int count = *(int*)(selfAddr + 0x30);
    uintptr_t arrayBase = *(uintptr_t*)(selfAddr + 0x28);

    if (GameUpdateLoop::_singleton == nullptr) return;

    // Get the graphics scene from game state
    uintptr_t gameState = (uintptr_t)GameUpdateLoop::_singleton;
    void* scene = *(void**)(gameState + 0x60);

    for (int i = 0; i < count; i++) {
        uintptr_t entry = arrayBase + (uintptr_t)i * 0x128;
        uint32_t sceneId = *(uint32_t*)(entry + 0x120);

        if ((int32_t)sceneId >= 0) {
            reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject(sceneId);   // B.3.5 bridge
            *(uint32_t*)(entry + 0x120) = 0xFFFFFFFF; // Reset to -1
        }
    }
}

// [9] 0x1400566a0 (`JMP 0x1400563e0`) — the NATIVE +0x48 driver. The tree's
// body lays out the w*h pile grid (Draw) and then allocates the pile meshes
// from the StorageComponent contents (Update); both early-out cheaply when
// unchanged.
void StorageDisplayComponent::Update(void* /*time*/) {
    StorageDisplayComponent::OnAdded();   // the slot-6 body, called directly
    StorageDisplayComponent_Update(this);
}

// ============================================================
// StorageDisplayComponent::Slot11 (vtable slot [11], 0x140056940)
// ============================================================
// Calls vtable Draw then Update (display rebuild + content refresh).
//
// Original disassembly:
//   MOV RAX,[RCX]            ; vtable
//   CALL [RAX+0x30]          ; vtable->Draw(self)
//   JMP Entity_impl_63e0     ; tail call to Update logic

void StorageDisplayComponent::Slot11() {
    // Call Draw through vtable
    OnAdded();   // vtable +0x30 (slot 6), a virtual dispatch in retail

    // Call Update
    StorageDisplayComponent_Update(this);
}

// create 0x14000c770: MemAlloc(0x38, 8), then the ctor.
void* StorageDisplayComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageDisplayComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageDisplayComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* StorageDisplayComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageDisplayComponent());
}

DEFINE_EXE_TYPE(StorageDisplayComponent, "Component")
