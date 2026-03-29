// code/Game/Player/fencedtool.cpp
// Clean reimplementation of FencedTool vtable methods from Application-x64.exe.
//
// FencedTool (0x70 bytes) extends DragTool (0x38 bytes).
// Handles fenced area placement (pasture, crop fields).
//
// Vtable at 0x1401605d0 (15 slots). Four are reimplemented here:
//   Slot0  Dtor      (0x140026930) — scalar deleting destructor
//   Slot5  Shutdown2  (0x1400dccc0) — cleanup scene object and mesh
//   Slot6  OnKey      (0x1400dcbe0) — wraps DragTool::OnKey + rotation handling
//   Slot9  Shutdown   (0x1400dcd10) — remove scene object + call base shutdown
//
// Remaining slots (Startup, Serialize, Draw, CalcPath, CalcCost) are too complex
// (many unresolved helpers: MeshInstance, Transform, entity creation, fence segment
// calculation) and stay as trampolines.
//
// Decompiled from Application-x64.exe.

#include "Player/fencedtool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Vtable addresses (for vtable chain reset in Dtor)
// ============================================================
static void* s_vtable_FencedTool = nullptr;
static void* s_vtable_DragTool   = nullptr;
static void* s_vtable_Tool       = nullptr;

static void EnsureVtables() {
    if (!s_vtable_FencedTool) {
        s_vtable_FencedTool = (void*)ResolveAddr(Module::Exe, FencedToolAddr::Vtable);
        s_vtable_DragTool   = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool       = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// Object::Internal::~Internal — resolved via dispatch slot at 0x140152770
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// Graphics::Scene::RemoveObject — resolved locally via exe IAT dispatch slot
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
static fn_SceneRemoveObject_t s_fn_SceneRemoveObject = nullptr;

static bool s_sceneResolved = false;

static void EnsureSceneFuncs() {
    if (s_sceneResolved) return;
    // Resolve via IAT at [0x140151360] — same pattern as storagedisplaycomponent.cpp
    uintptr_t iatSlot = ResolveAddr(Module::Exe, 0x140151360);
    if (iatSlot) s_fn_SceneRemoveObject = *reinterpret_cast<fn_SceneRemoveObject_t*>(iatSlot);
    s_sceneResolved = true;
}

// ============================================================
// FencedTool::Dtor (vtable +0x00, Slot0)
// ============================================================
// Resets vtable chain: FencedTool -> DragTool -> Tool,
// calls Object::Internal::~Internal, conditionally frees memory.
// Original: 0x140026930
void* FencedTool_Dtor(FencedTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance
    self->vtable = s_vtable_FencedTool;
    self->vtable = s_vtable_DragTool;
    self->vtable = s_vtable_Tool;

    // Call Object::Internal::~Internal
    if (s_fn_Internal_Dtor) {
        s_fn_Internal_Dtor(self);
    }

    // Conditionally free memory
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// FencedTool::Shutdown2 (vtable +0x28, Slot5)
// ============================================================
// Removes the fence preview from the scene and destroys the mesh instance.
// Original: 0x1400dccc0
void FencedTool_Shutdown2(FencedTool* self) {
    EnsureSceneFuncs();

    // Remove scene object if added
    if ((int)self->sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, self->sceneObjectId);
        }
    }

    // Destroy mesh instance via vtable dtor if allocated
    void* mesh = self->meshInstance;
    if (mesh) {
        // Call scalar deleting destructor: vtable[0](mesh, 1)
        typedef void* (*fn_MeshDtor_t)(void* self, uint32_t flags);
        void** vtable = *(void***)mesh;
        fn_MeshDtor_t dtor = (fn_MeshDtor_t)vtable[0];
        dtor(mesh, 1);  // flags=1: free memory
    }
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline void* GetKeyMap() {
    return ***(void****)g_pKeyMapObjectExref;
}

// ============================================================
// FencedTool::OnKey (vtable +0x30, Slot6)
// ============================================================
// Wraps DragTool::OnKey with rotation handling for fence orientation.
//
// After the base OnKey call:
//   - KeyMap::Pressed(0, 0x13) = rotate counter-clockwise: decrement rotation,
//     wrap from 0 to 3
//   - KeyMap::Pressed(0, 0x14) = rotate clockwise: increment rotation,
//     wrap from 4 to 0
//
// Rotation value stored at rotationSetting(+0x50)->+0x10.
// Always returns 1 (event consumed).
// Original: 0x1400dcbe0
uint64_t FencedTool_OnKey(FencedTool* self, char key) {
    // Call base DragTool::OnKey
    if (fn_DragTool_OnKey) {
        fn_DragTool_OnKey(self, key);
    }

    void* keyMap = GetKeyMap();

    // Check rotate counter-clockwise (key 0x13)
    bool rotLeft = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 0x13) : false;
    if (rotLeft) {
        // Access rotationSetting IBP at self+0x50
        void* rotSettingIBP = (void*)((uintptr_t)self + 0x50);
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(rotSettingIBP);
        }
        uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
        *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) - 1;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(rotSettingIBP);
        }
        rotSetting = *(uintptr_t*)rotSettingIBP;
        if (*(int*)(rotSetting + 0x10) < 0) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            rotSetting = *(uintptr_t*)rotSettingIBP;
            *(int*)(rotSetting + 0x10) = 3;
        }
    } else {
        // Check rotate clockwise (key 0x14)
        bool rotRight = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 0x14) : false;
        if (rotRight) {
            void* rotSettingIBP = (void*)((uintptr_t)self + 0x50);
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
            *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) + 1;

            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            rotSetting = *(uintptr_t*)rotSettingIBP;
            if (*(int*)(rotSetting + 0x10) > 3) {
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr(rotSettingIBP);
                }
                rotSetting = *(uintptr_t*)rotSettingIBP;
                *(int*)(rotSetting + 0x10) = 0;
            }
        }
    }

    return 1;
}

// ============================================================
// FencedTool::Shutdown (vtable +0x48, Slot9)
// ============================================================
// Removes scene object if added, resets ID to -1, calls base DragTool::Shutdown.
// Original: 0x1400dcd10
void FencedTool_Shutdown(FencedTool* self) {
    EnsureSceneFuncs();

    // Remove scene object if added
    if ((int)self->sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, self->sceneObjectId);
        }
        self->sceneObjectId = (uint32_t)-1;
    }

    // Call base DragTool::Shutdown (0x1400fc640)
    if (fn_DragTool_Shutdown) {
        fn_DragTool_Shutdown(self);
    }
}
