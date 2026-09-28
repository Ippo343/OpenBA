// code/Game/Player/tool_base_place.cpp
// Player wave (batch "place") — shared PlacementTool sub-helpers resolved via
// ResolveAddr(Module::Exe, 0xADDR) from code/Game/Player/placementtool.cpp.
//
// These resolve to 0 (silently skipped) unless registered. This file provides
// faithful 1:1 reimplementations and a single registration point
// (RegisterPlayerPlaceBatch in standalone/reg_player_place.cpp) so the call
// sites fire real logic.
//
// Closure-safety: only functions whose ENTIRE exe sub-callee closure is also
// satisfied (registered exe code / registered dispatch slot / Runtime export)
// are reimplemented + registered here. The rest are documented as DEFER in
// reg_player_place.cpp with the exact unregistered sub-callee.

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "Player/exe_bodies.h"

// ============================================================
// PlacementTool_sub_d960 — mirror/flip + advance mesh variant (0x1400fd960)
// ============================================================
// Increments the mesh-variant index (self+0x64), wrapping to 0 when it reaches
// the variant count (*(createComponent+0x28), createComponent at self+0x38).
// If a scene object is currently displayed (self+0x60 >= 0), it removes it,
// re-selects the mesh for the new variant (last entry of the variant's sub-array),
// updates the mesh hierarchy, and re-adds it to the scene (storing the new id at
// self+0x60).
//
// Decompiled + asm-verified from 0x1400fd960. The original reaches the four
// graphics ops through IAT dispatch slots:
//   [0x140151360] Scene::RemoveObject       -> engine_funcs fn_SceneRemoveObject
//   [0x140151498] MeshInstance::SetMesh      -> engine_funcs fn_Graphics_MeshInstance_SetMesh
//   [0x140152888] MeshInstance::UpdateHierarchy -> engine_funcs fn_MeshInstance_UpdateHierarchy
//   [0x140151370] Scene::AddObject           -> engine_funcs fn_SceneAddObject
// All four are available as Runtime-export-backed engine_funcs pointers (the
// 0x140152888 dispatch slot is registered by RegisterPlayerPlaceBatch so the
// engine_funcs init reads a non-null target). Variant-index math + mesh-array
// indexing has no exe sub-callees.
//
// Mesh selection (mirrors the disasm at 0x1400fd99b..0x1400fd9b9):
//   meshArray = *(createComponent + 0x20)
//   entry     = meshArray + variant*0x18
//   subCount  = *(int*)(entry + 0x10)
//   subArray  = *(void**)(entry + 0x08)
//   mesh      = subArray[subCount - 1]   // last entry
void PlacementToolPlace_FlipVariant_d960(void* selfp) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfp);

    // Advance + wrap the variant index against the create-component's count.
    int variant = *reinterpret_cast<int*>(self + 0x64) + 1;
    uintptr_t createComp = *reinterpret_cast<uintptr_t*>(self + 0x38);
    int count = *reinterpret_cast<int*>(createComp + 0x28);
    if (variant >= count) variant = 0;
    *reinterpret_cast<int*>(self + 0x64) = variant;

    // Only update the live scene object if one is currently shown.
    int sceneObj = *reinterpret_cast<int*>(self + 0x60);
    if (sceneObj < 0) return;

    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    void* scene = *reinterpret_cast<void**>(gameWorld + 0x60);

    if (scene) {
        reinterpret_cast<Graphics::Scene*>(scene)
            ->RemoveObject(static_cast<uint32_t>(sceneObj));   // B.3.5 bridge
    }

    // Re-select mesh for the new variant (last entry of its sub-array).
    uintptr_t meshArray = *reinterpret_cast<uintptr_t*>(createComp + 0x20);
    uintptr_t entry = meshArray + static_cast<int64_t>(variant) * 0x18;
    int subCount = *reinterpret_cast<int*>(entry + 0x10);
    uintptr_t subArray = *reinterpret_cast<uintptr_t*>(entry + 0x8);
    void* mesh = *reinterpret_cast<void**>(subArray + static_cast<int64_t>(subCount - 1) * 8);

    void* meshInstance = *reinterpret_cast<void**>(self + 0x68);
    if (meshInstance) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->SetMesh(Graphics::ExternalPtrHandle{mesh});   // B.3.5 bridge
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->UpdateHierarchy();   // B.3.5 bridge
    }
    if (scene && meshInstance) {
        uint32_t newId = reinterpret_cast<Graphics::Scene*>(scene)
            ->AddObject(reinterpret_cast<Graphics::Geometry*>(meshInstance));   // B.3.5 bridge
        *reinterpret_cast<uint32_t*>(self + 0x60) = newId;
    }
}
