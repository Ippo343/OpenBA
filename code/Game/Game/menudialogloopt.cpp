// code/Game/Game/menudialogloopt.cpp
// Phase A reimplementation of the MenuDLT (Engine::DialogLoopT subclass) 3D
// scene background — Load + Draw paths. Decompiled from:
//   - MenuDLT::Load (exe 0x14009A930)  — scene allocation + AddGlobalObject
//   - MenuDLT::Draw (exe 0x14009B440)  — camera/light setup + Renderer::Draw
//
// Phase A scope: render the menu scene mesh with sun lighting + fog as the
// background to the main menu UI. Skips:
//   - Audio music load (Audio/Music/Menu.rsc)
//   - TradeWork DLC ship (gameSettings[0x16] branch)
//   - Static-mesh array iteration (Phase B)
//   - Particle-system iteration  (Phase C)
//   - Exe-side vtable overrides (PTR_FUN_14015e290, _vslot10_14015e240)
//
// Skipping the vtable overrides means we use the default Runtime MeshInstance
// + SunLight vtables. This may render slightly differently from the original
// but is correct for an MVP scene background.

#include "Game/menudialogloopt_funcs.h"
#include "MenuScene.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

// ============================================================
// MenuDLT struct field offsets (Engine::DialogLoopT subclass)
// ============================================================
// DialogLoopT is 0x168 bytes (vtable + UpdateLoop + dialog fields + UI). MenuDLT
// extends it with menu-specific fields starting around +0xb70:
//   +0xb7c: float copy of transform[+0x28] (preserved across reload)
//   +0xb80: float copy of transform[+0x2C] (preserved across reload)
//   +0xb88: External* — Audio/Music/Menu.rsc (Phase A skips)
//   +0xb90: External* — Game/MenuScene/SceneN.rsc (the MenuScene)
//   +0xb98: Graphics::Renderer*
//   +0xba0: Graphics::Scene*  (NB: Graphics, NOT Audio)
//   +0xba8: Graphics::MeshInstance* — main scene mesh
//   +0xbb0: Graphics::SunLight*
//   +0xbb8: Math::Transform*  (size 0x10C, Camera embedded at +0x34)
//   +0xbc0: Geometry* — the scene's first Geometry (MenuScene+0x28)
//   +0xbc8: TradeWork* — DLC ship (Phase A skips)
//   +0xbd0..+0xbfc: dynamic arrays for static meshes/particles (Phase B/C)
namespace MenuDLTOff {
    constexpr uintptr_t audioScene_b30  = 0xb30;  // embedded Audio::Scene (0x38 bytes)
    constexpr uintptr_t scrollX_b7c     = 0xb7c;
    constexpr uintptr_t scrollY_b80     = 0xb80;
    constexpr uintptr_t musicExt_b88    = 0xb88;  // External* — Audio/Music/Menu.rsc
    constexpr uintptr_t menuSceneExt_b90 = 0xb90;
    constexpr uintptr_t renderer_b98    = 0xb98;
    constexpr uintptr_t graphicsScene_ba0 = 0xba0;
    constexpr uintptr_t mainMesh_ba8    = 0xba8;
    constexpr uintptr_t sunLight_bb0    = 0xbb0;
    constexpr uintptr_t transform_bb8   = 0xbb8;
    constexpr uintptr_t sceneGeom_bc0   = 0xbc0;
    // Phase B/C: dynamic arrays for static-mesh + particle instances.
    // Layout per array: data ptr / count int / capacity int (12 bytes total
    // but aligned to 16 in struct so each array spans .._00..0x10 bytes).
    constexpr uintptr_t partInstArr_bd0 = 0xbd0;  // data: ParticleSystemInstance*
    constexpr uintptr_t partInstCnt_bd8 = 0xbd8;
    constexpr uintptr_t partInstCap_bdc = 0xbdc;
    constexpr uintptr_t allInstArr_be0  = 0xbe0;  // data: Geometry* (mesh + particle, shared)
    constexpr uintptr_t allInstCnt_be8  = 0xbe8;
    constexpr uintptr_t allInstCap_bec  = 0xbec;
    constexpr uintptr_t sceneIdArr_bf0  = 0xbf0;  // data: uint32 ids
    constexpr uintptr_t sceneIdCnt_bf8  = 0xbf8;
    constexpr uintptr_t sceneIdCap_bfc  = 0xbfc;
}

// Helper: read/write fields by offset (matches the project's prevailing style).
template<typename T>
static inline T& Field(void* base, uintptr_t off) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + off);
}

// ============================================================
// Constants from the original exe (.rdata)
// ============================================================
// FUN_140098800 (Transform setup helper) defaults:
//   fovY  = 90.0f   (0x42b40000)   stored as fovX *and* fovY pair
//   far   = 100.0f  (0x42c80000)   from DAT_140172208
//   near  = 1.0f    (0x3f800000)   from DAT_14017214c
// FUN_140098660 (per-frame camera update):
//   fovX  = DAT_1401721dc = 40.0f  (0x42200000)  passed as XMM1 = scalar param
//   refAspect = DAT_140172170 = 1.7777778f       (16:9 reference)
//   near  = DAT_140172174 = 2.0f   (used only by helper-660; helper-8800 uses 1.0f)
//   far   = DAT_140172240 = 512.0f (used only by helper-660; helper-8800 uses 100.0f)
//   colorDiv = DAT_14017222c = 255.0f
// We hard-code these floats so we don't depend on registering the exe DAT_ slots.
namespace MenuSceneConst {
    static constexpr float kHelper8800_FovX        = 90.0f;   // 0x42b40000
    static constexpr float kHelper8800_FovY        = 90.0f;   // 0x42b40000 (same as fovX)
    static constexpr float kHelper8800_NearZ       = 1.0f;    // 0x3f800000
    static constexpr float kHelper8800_FarZ        = 100.0f;  // 0x42c80000

    static constexpr float kHelper8660_FovX        = 40.0f;   // DAT_1401721dc
    static constexpr float kHelper8660_NearZ       = 2.0f;    // DAT_140172174
    static constexpr float kHelper8660_FarZ        = 512.0f;  // DAT_140172240

    static constexpr float kColorScale255          = 255.0f;  // DAT_14017222c
}

// ============================================================
// Local helpers — reimplementations of small exe-internal helpers we depend on
// ============================================================

// Mirror of FUN_140098800 (exe 0x140098800) — Transform default ctor + Camera setup.
// Original:
//   Math::Transform::Transform(self)
//   self+0x1C = 0; self+0x24 = 0   (zero some scalars)
//   ident = Math::Transform3::Identity()
//   frustum = Volume::Frustum(near=1, far=100, fovY=90, fovX=90)
//   Graphics::Camera::Camera(self+0x34, frustum, ident, true /* isPerspective */)
static void* MenuScene_Helper_8800(void* transformObj) {
    if (!fn_Math_Transform_Ctor || !fn_Math_Transform3_Identity ||
        !fn_Volume_Frustum_Ctor || !fn_Graphics_Camera_Ctor) {
        printf("[MenuDLT::Helper8800] missing fn ptrs (TCtor=%p T3Id=%p FrustC=%p CamC=%p)\n",
               (void*)fn_Math_Transform_Ctor, (void*)fn_Math_Transform3_Identity,
               (void*)fn_Volume_Frustum_Ctor, (void*)fn_Graphics_Camera_Ctor);
        return transformObj;
    }
    fn_Math_Transform_Ctor(transformObj);

    // Zero scalars at +0x1C (qword) and +0x24 (dword) per original.
    Field<uint64_t>(transformObj, 0x1C) = 0;
    Field<uint32_t>(transformObj, 0x24) = 0;

    // Transform3::Identity returns BY VALUE: the hidden return slot is an
    // argument, not something the callee allocates. Passing nothing left it
    // writing 0x30 bytes through a stale RCX (see engine_funcs.h).
    uint8_t identBuf[0x40] = {};
    void* ident = fn_Math_Transform3_Identity(identBuf);

    // Frustum ctor takes 4 Scalar* args in order (near, far, fovX, fovY) — the
    // first two are stored as raw plane distances, the last two are interpreted
    // as half-FOV angles (in degrees) for sin/cos. Confirmed by decompile of
    // FUN_140098800 + Volume::Frustum::Frustum (Runtime 0x1800fa1e0).
    float fovX = MenuSceneConst::kHelper8800_FovX;
    float fovY = MenuSceneConst::kHelper8800_FovY;
    float nearZ = MenuSceneConst::kHelper8800_NearZ;
    float farZ  = MenuSceneConst::kHelper8800_FarZ;

    uint8_t frustumBuf[0x80] = {};
    fn_Volume_Frustum_Ctor(frustumBuf, &nearZ, &farZ, &fovX, &fovY);

    // Camera ctor: self = transform+0x34, frustum, transform3, isPerspective=true
    void* cameraObj = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transformObj) + 0x34);
    fn_Graphics_Camera_Ctor(cameraObj, frustumBuf, ident, true);
    return transformObj;
}

// Mirror of FUN_140098660 (exe 0x140098660) — per-frame camera frustum update.
// Original handles widescreen FOV adjustment via sinf/atanf math; for Phase A
// we use the simple fovX = aspect * fovY frustum and skip the ultrawide clamp.
// This matches the original's behavior for aspect <= 16/9.
static void MenuScene_Helper_8660(void* transformObj, float fovY_deg, float nearZ, float farZ) {
    if (!fn_Volume_Frustum_Ctor || !fn_Graphics_Camera_SetFrustum ||
        !g_pVideoConfigSingleton || !*g_pVideoConfigSingleton) {
        return;
    }
    void* videoConfig = *g_pVideoConfigSingleton;
    int width  = Field<int>(videoConfig, 0x4);
    int height = Field<int>(videoConfig, 0x8);
    // VideoConfig is filled lazily by the runtime in standalone Phase A; until
    // it has real dimensions, fall back to 16:9 reference aspect (matches the
    // original's DAT_140172170 = 1.7778).
    if (height <= 0) {
        width = 16;
        height = 9;
    }

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fovX = aspect * fovY_deg;
    // (Original ultrawide clamp at aspect > 16/9 omitted — Phase A MVP.)

    uint8_t frustumBuf[0x80] = {};
    fn_Volume_Frustum_Ctor(frustumBuf, &nearZ, &farZ, &fovX, &fovY_deg);

    // Camera lives at transform+0x34.
    void* cameraObj = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transformObj) + 0x34);
    fn_Graphics_Camera_SetFrustum(cameraObj, frustumBuf);
}

// Mirror of MenuScene_helper_a3a0 (exe 0x14001a3a0) — copy 13 dwords + Camera::operator=
// from MenuScene's embedded transform block (MenuScene+0x88, 0x10C bytes total) into
// the live Transform at MenuDLT+0xbb8. The first 13 dwords are pure value copies; the
// Camera operator= at offset 0x34 we approximate with memcpy (Camera is value-typed
// here — original uses a Runtime helper that does the same write under the hood).
static void MenuScene_CopyTransform(void* destTransform, const void* srcTransformBlock) {
    // Copy the leading 13 dwords (52 bytes — ends at offset 0x34 where Camera starts).
    std::memcpy(destTransform, srcTransformBlock, 13 * sizeof(uint32_t));
    // Copy the Camera block (offset 0x34 onward — 0xD8 bytes).
    std::memcpy(reinterpret_cast<uint8_t*>(destTransform) + 0x34,
                reinterpret_cast<const uint8_t*>(srcTransformBlock) + 0x34,
                0x10C - 0x34);
}

// ============================================================
// Phase B: append-only dynamic array helpers
// ============================================================
// Mirrors the inline grow-and-append pattern the original uses for the
// MenuDLT+0xbd0/+0xbe0/+0xbf0 arrays. Layout per array:
//   data ptr  : <off+0x0> = void*
//   count int : <off+0x8> = int32
//   cap   int : <off+0xc> = int32
// Growth rule: needed = count+1; if cap < needed, set cap = needed*3 (or
// needed*1 when count was 0); MemAlloc(cap*stride, alignFromStride); copy
// old elements; MemFree old buffer if reallocated.
// stride must be 4 (id array) or 8 (ptr array).
static void* AppendToDynArray(void* self, uintptr_t arrOff, size_t stride) {
    void*    oldData  = Field<void*>(self, arrOff + 0x0);
    int32_t  oldCount = Field<int32_t>(self, arrOff + 0x8);
    int32_t  oldCap   = Field<int32_t>(self, arrOff + 0xC);
    int32_t  newCount = oldCount + 1;

    Field<int32_t>(self, arrOff + 0x8) = newCount;

    if (oldCap < newCount) {
        int32_t newCap = newCount;
        if (oldCount != 0) newCap = newCount * 3;
        Field<int32_t>(self, arrOff + 0xC) = newCap;
        size_t bytes = static_cast<size_t>(newCap) * stride;
        size_t align = stride;  // 4 or 8
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, bytes, align);
        if (oldData && newData) {
            std::memcpy(newData, oldData, static_cast<size_t>(oldCount) * stride);
        }
        Field<void*>(self, arrOff + 0x0) = newData;
        if (oldData && newData != oldData && fn_MemFreeStatic) {
            fn_MemFreeStatic(oldData);
        }
    }
    // Return pointer to the new (uninitialized) slot.
    void* data = Field<void*>(self, arrOff + 0x0);
    return reinterpret_cast<uint8_t*>(data) + static_cast<size_t>(oldCount) * stride;
}

// ============================================================
// Phase B: static-mesh iteration (props/buildings/foliage)
// ============================================================
// Mirrors the original MenuDLT::Load static-mesh loop (exe 0x14009A930). For
// each element of MenuScene+0x30 (count=MenuScene+0x38, stride=0x20):
//   1. Alloc 0x120-byte MeshInstance, default-construct.
//      (Original overrides vtable[0] with PTR_EnvironmentDescription_vslot10_14015e240
//       — we keep the default Runtime vtable; the override only changes some
//       per-frame envmap behavior, not basic visibility.)
//   2. Append the new instance ptr to MenuDLT+0xbe0 (shared with particles).
//   3. Per-element setup (mirrors MenuScene_helper_3b60 at exe 0x1400a3b60):
//      - Read elem+0x0c..0x1e (3 dwords + yaw + meshIdx + matIdx).
//      - SetMesh(meshArray[meshIdx]) where meshArray = *(void**)(MenuScene+0x40).
//      - Geometry::SetTransform(FromYawPitchOffset(yaw, 0, {0,0,0})).
//      - Write inst+0x10c/0x110/0x114 from elem+0x0c/0x10/0x14.
//      - Write inst+0x118 = g_fUVScale (1.0f from exe DAT_14017214c).
//      - Iterate GraphicsMesh::GetMaterialList(matIdx) and call
//        MeshInstance::SetMaterialInstance for each entry.
//      - Vtable+0x08 (Update) called with Time(0).
//   4. Scene::AddObject(scene, instance) → uint32 id, append to MenuDLT+0xbf0.
//
// Implementation is split into two pieces so the loop can be driven
// in chunks across the Loading splash's Update ticks (eliminating the
// ~542ms synchronous freeze that would otherwise happen during MenuDLT::
// Startup). A synchronous all-in-one wrapper is provided below.

// Per-element processing — body of the original loop's `for (i = 0; i < meshCount; ++i)`.
// Returns the number of material slots wired (for stat reporting). Sets
// *outSkippedBadIdx=true when the meshIdx was out of range (the slot still got
// an empty inst added so id arrays stay in sync).
static int32_t MenuDialogLoopT_PhaseB_ProcessElement(
    void* self, void* scene,
    const uint8_t* elem, void** meshArr, int32_t meshArrCount,
    bool* outSkippedBadIdx) {
    *outSkippedBadIdx = false;

    // 1. Allocate + construct MeshInstance.
    void* inst = fn_MemAlloc(g_pMemAllocSingleton, 0x120, 8);
    if (!inst) return 0;
    fn_Graphics_MeshInstance_Ctor(inst);

    // 2. Append to MenuDLT+0xbe0 (shared instance list).
    void** slotPtr = reinterpret_cast<void**>(
        AppendToDynArray(self, MenuDLTOff::allInstArr_be0, sizeof(void*)));
    if (slotPtr) *slotPtr = inst;

    // 3. Per-element setup — mirror MenuScene_helper_3b60.
    int16_t meshIdx = *reinterpret_cast<const int16_t*>(elem + 0x1c);
    int16_t matIdx  = *reinterpret_cast<const int16_t*>(elem + 0x1e);

    // Defensive bound check on meshIdx — keeps id arrays in sync if a future
    // serializer regression ever produces an out-of-range index.
    if (meshIdx < 0 || meshIdx >= meshArrCount) {
        unsigned int id0 = fn_Graphics_Scene_AddObject(scene, inst);
        uint32_t* idSlot0 = reinterpret_cast<uint32_t*>(
            AppendToDynArray(self, MenuDLTOff::sceneIdArr_bf0, sizeof(uint32_t)));
        if (idSlot0) *idSlot0 = id0;
        *outSkippedBadIdx = true;
        return 0;
    }

    // SetMesh(meshArray[meshIdx]) — value passed is the 8-byte ExternalPtr.
    void* meshExtPtr = meshArr[meshIdx];
    fn_Graphics_MeshInstance_SetMesh(inst, meshExtPtr);

    // Geometry::SetTransform(FromYawPitchOffset(yaw, pitch=0, offset=elem.pos)).
    // FromYawPitchOffset is `static Transform FromYawPitchOffset(Scalar& yaw,
    // Scalar& pitch, Vector3& offset)` — hidden retval ptr (RCX), RDX=&yaw,
    // R8=&pitch, R9=&offset. Returns ~28-byte Transform.
    // Per-element layout (verified against helper_3b60 disasm @ 0x1400a3b60):
    //   elem+0x00..0x0b = Vector3 position (X,Y,Z)
    //   elem+0x0c..0x17 = scale floats → MeshInstance+0x10c..0x114
    //   elem+0x18      = yaw
    //   elem+0x1c      = meshIdx (int16)
    //   elem+0x1e      = matIdx (int16)
    float yaw   = *reinterpret_cast<const float*>(elem + 0x18);
    float pitch = 0.f;
    float offset[3] = {
        *reinterpret_cast<const float*>(elem + 0x00),
        *reinterpret_cast<const float*>(elem + 0x04),
        *reinterpret_cast<const float*>(elem + 0x08),
    };
    uint8_t transformBuf[32] = {};
    fn_Transform_FromYawPitchOffset(transformBuf, &yaw, &pitch, offset);
    fn_Geometry_SetTransform(inst, transformBuf);

    // Copy 3 dwords + scale into MeshInstance fields. Last value is g_fUVScale
    // (exe DAT_14017214c, hardcoded 1.0f to skip an extra ResolveAddr).
    Field<uint32_t>(inst, 0x10c) = *reinterpret_cast<const uint32_t*>(elem + 0x0c);
    Field<uint32_t>(inst, 0x110) = *reinterpret_cast<const uint32_t*>(elem + 0x10);
    Field<uint32_t>(inst, 0x114) = *reinterpret_cast<const uint32_t*>(elem + 0x14);
    Field<float>   (inst, 0x118) = 1.0f;

    // Iterate GetMaterialList(matIdx) and SetMaterialInstance per slot.
    int32_t matSlots = 0;
    if (fn_Graphics_GraphicsMesh_GetMaterialList && fn_Graphics_MeshInstance_SetMaterialInstance &&
        meshExtPtr) {
        void* matList = fn_Graphics_GraphicsMesh_GetMaterialList(meshExtPtr, matIdx);
        if (matList) {
            int32_t matCount = Field<int32_t>(matList, 0x08);
            void**  matData  = Field<void**>(matList, 0x00);
            for (int32_t m = 0; m < matCount && matData; ++m) {
                fn_Graphics_MeshInstance_SetMaterialInstance(inst, m, matData[m]);
            }
            matSlots = matCount;
        }
    }

    // Vtable+0x08 (Update) with Time(0).
    uint8_t timeBuf[8] = {};
    fn_Time_Ctor_Float(timeBuf, 0.0f);
    void** vtable = *reinterpret_cast<void***>(inst);
    if (vtable && vtable[1]) {
        using fn_t = void (*)(void* self, void* time);
        reinterpret_cast<fn_t>(vtable[1])(inst, timeBuf);
    }

    // 4. Scene::AddObject + append id.
    unsigned int id = fn_Graphics_Scene_AddObject(scene, inst);
    uint32_t* idSlot = reinterpret_cast<uint32_t*>(
        AppendToDynArray(self, MenuDLTOff::sceneIdArr_bf0, sizeof(uint32_t)));
    if (idSlot) *idSlot = id;

    return matSlots;
}

// Initialize a chunked-loop state for PhaseB. Captures the inputs and
// validates fn pointers + array bounds. Sets state->totalCount=0 (so
// PhaseB_TickChunk early-exits) on any failure.
void MenuDialogLoopT_PhaseB_InitState(MenuDialogLoopT_PhaseBState* state,
                                      void* self, void* menuScene, void* scene) {
    state->self           = self;
    state->menuScene      = menuScene;
    state->scene          = scene;
    state->elemArr        = nullptr;
    state->meshArr        = nullptr;
    state->meshArrCount   = 0;
    state->totalCount     = 0;
    state->index          = 0;
    state->added          = 0;
    state->skippedBadIdx  = 0;
    state->matSlotsTotal  = 0;

    if (!self || !menuScene || !scene) return;
    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_Graphics_MeshInstance_Ctor ||
        !fn_Graphics_MeshInstance_SetMesh || !fn_Graphics_Scene_AddObject ||
        !fn_Geometry_SetTransform || !fn_Transform_FromYawPitchOffset ||
        !fn_Time_Ctor_Float) {
        printf("[MenuDLT::PhaseB] missing required fn ptrs "
               "(MICtor=%p MISetM=%p AddObj=%p GeomSetT=%p T_FYPO=%p TimeC=%p) — skipping\n",
               (void*)fn_Graphics_MeshInstance_Ctor,
               (void*)fn_Graphics_MeshInstance_SetMesh,
               (void*)fn_Graphics_Scene_AddObject,
               (void*)fn_Geometry_SetTransform,
               (void*)fn_Transform_FromYawPitchOffset,
               (void*)fn_Time_Ctor_Float);
        return;
    }

    // Count is read from MenuScene+0x38 just like the original (FUN_14009a930
    // line: `if (0 < *(int *)(*(longlong *)(param_1 + 0xb90) + 0x38))`).
    // Menu scenes have ~12-15K props (foliage, rocks, decals); the variation
    // between runs is because MenuDLT::Load picks Scene[0..9].rsc randomly.
    int32_t meshCount    = Field<int32_t>(menuScene, 0x38);
    int32_t meshArrCount = Field<int32_t>(menuScene, 0x48);  // arr40 count (bound for meshIdx)
    printf("[MenuDLT::PhaseB] MenuScene+0x38=%d (arr30 elems) +0x48=%d "
           "(arr40 GraphicsMesh count)\n", meshCount, meshArrCount);
    if (meshCount <= 0) {
        printf("[MenuDLT::PhaseB] MenuScene+0x38 count=%d — no static meshes\n", meshCount);
        return;
    }

    // Element array ptr at MenuScene+0x30 (Collection::Array data ptr).
    uint8_t* elemArr = Field<uint8_t*>(menuScene, 0x30);
    // GraphicsMesh ExternalPtr array data at *(MenuScene+0x40).
    void**   meshArr  = Field<void**>(menuScene, 0x40);
    if (!elemArr || !meshArr) {
        printf("[MenuDLT::PhaseB] elemArr=%p meshArr=%p (count=%d) — skipping\n",
               (void*)elemArr, (void*)meshArr, meshCount);
        return;
    }

    state->elemArr      = elemArr;
    state->meshArr      = meshArr;
    state->meshArrCount = meshArrCount;
    state->totalCount   = meshCount;
}

// Process up to maxElements elements of the PhaseB iteration. Returns the
// number of elements processed this call. Returns 0 when the loop has fully
// completed (state->index == state->totalCount). Caller drives this across
// multiple Update ticks.
int32_t MenuDialogLoopT_PhaseB_TickChunk(MenuDialogLoopT_PhaseBState* state, int32_t maxElements) {
    if (!state || state->totalCount <= 0) return 0;
    if (state->index >= state->totalCount) return 0;
    if (maxElements <= 0) maxElements = 1;

    int32_t end = state->index + maxElements;
    if (end > state->totalCount) end = state->totalCount;

    int32_t processed = 0;
    for (int32_t i = state->index; i < end; ++i) {
        const uint8_t* elem = state->elemArr + static_cast<size_t>(i) * 0x20;
        bool skippedBadIdx = false;
        int32_t matSlots = MenuDialogLoopT_PhaseB_ProcessElement(
            state->self, state->scene, elem,
            state->meshArr, state->meshArrCount, &skippedBadIdx);
        if (skippedBadIdx) ++state->skippedBadIdx;
        else               ++state->added;
        state->matSlotsTotal += matSlots;
        ++processed;
    }
    state->index = end;

    if (state->index >= state->totalCount) {
        printf("[MenuDLT::PhaseB] static meshes: added=%d/%d wired=%d skipped(badIdx)=%d "
               "(%d material slots)\n",
               state->added + state->skippedBadIdx, state->totalCount,
               state->added, state->skippedBadIdx, state->matSlotsTotal);
        fflush(stdout);
    }
    return processed;
}

// Synchronous wrapper — preserves the original DLL-mode signature. Loops
// PhaseB_TickChunk until done. Standalone uses the chunked state directly
// from ChunkedSceneLoader to avoid the freeze.
void MenuDialogLoopT_LoadStaticMeshes_PhaseB(void* self, void* menuScene, void* scene) {
    MenuDialogLoopT_PhaseBState state;
    MenuDialogLoopT_PhaseB_InitState(&state, self, menuScene, scene);
    if (state.totalCount <= 0) return;
    // Pass totalCount as the chunk size — entire iteration runs in one call.
    MenuDialogLoopT_PhaseB_TickChunk(&state, state.totalCount);
}

// ============================================================
// Phase C: particle iteration (chimney smoke, fountains, etc)
// ============================================================
// Mirror of the particle loop in MenuDLT::Load (exe 0x14009A930). Steps for
// each element 0..MenuScene+0x58:
//   1. Allocate ParticleSystemInstance (0xe0 bytes, align 8) + ctor.
//      (Original sets vtable to PTR_FUN_14015ecb0 — skipped, default Runtime
//      vtable handles the menu case.)
//   2. Append to MenuDLT+0xbe0 (all-instance array, shared with meshes).
//   3. Configure via MenuScene_helper_3ca0 (exe 0x1400a3ca0):
//      - particle.vtable[+0x48](particle, particleResource)
//        where particleResource = MenuScene+0x60 array indexed by elem[+0x10]
//      - SetTransform(FromYawPitchOffset(elem[+0xc] yaw, 0 pitch, elem[+0x00] offset))
//      - vtable[+0x08](particle, Time(0)) — initial Update tick.
//   4. Append to MenuDLT+0xbd0 (particle-only array — drives per-frame Update).
//   5. Scene::AddObject(scene, particle) → uint32 id, append to MenuDLT+0xbf0.
//
// Per-element layout in MenuScene+0x50 (stride 0x14 = 5*4):
//   elem+0x00 = Vector3 offset (12B)
//   elem+0x0c = yaw (float)
//   elem+0x10 = particle resource index (int32 into MenuScene+0x60 array)
void MenuDialogLoopT_LoadParticles_PhaseC(void* self, void* menuScene, void* scene) {
    if (!menuScene || !scene) return;
    if (!fn_MemAlloc || !g_pMemAllocSingleton ||
        !fn_Graphics_ParticleSystemInstance_Ctor ||
        !fn_Graphics_Scene_AddObject ||
        !fn_Geometry_SetTransform || !fn_Transform_FromYawPitchOffset ||
        !fn_Time_Ctor_Float) {
        printf("[MenuDLT::PhaseC] missing required fn ptrs "
               "(PSICtor=%p AddObj=%p GeomSetT=%p T_FYPO=%p TimeC=%p) — skipping\n",
               (void*)fn_Graphics_ParticleSystemInstance_Ctor,
               (void*)fn_Graphics_Scene_AddObject,
               (void*)fn_Geometry_SetTransform,
               (void*)fn_Transform_FromYawPitchOffset,
               (void*)fn_Time_Ctor_Float);
        return;
    }

    int32_t particleCount = Field<int32_t>(menuScene, 0x58);
    if (particleCount <= 0) {
        printf("[MenuDLT::PhaseC] MenuScene+0x58 count=%d — no particles\n", particleCount);
        return;
    }

    uint8_t* elemArr = Field<uint8_t*>(menuScene, 0x50);
    void**   resArr  = Field<void**>(menuScene, 0x60);
    if (!elemArr || !resArr) {
        printf("[MenuDLT::PhaseC] elemArr=%p resArr=%p (count=%d) — skipping\n",
               (void*)elemArr, (void*)resArr, particleCount);
        return;
    }

    int32_t added = 0;
    for (int32_t i = 0; i < particleCount; ++i) {
        // 1. Allocate + construct ParticleSystemInstance.
        void* inst = fn_MemAlloc(g_pMemAllocSingleton, 0xe0, 8);
        if (!inst) continue;
        fn_Graphics_ParticleSystemInstance_Ctor(inst);

        // 2. Append to all-instance array (+0xbe0).
        void** allSlot = reinterpret_cast<void**>(
            AppendToDynArray(self, MenuDLTOff::allInstArr_be0, sizeof(void*)));
        if (allSlot) *allSlot = inst;

        // 3. helper_3ca0 inline:
        //    Element layout: elem+0x00 = Vector3 offset, elem+0x0c = yaw,
        //                    elem+0x10 = resource index.
        const uint8_t* elem = elemArr + static_cast<size_t>(i) * 0x14;
        int32_t resIdx = *reinterpret_cast<const int32_t*>(elem + 0x10);
        void* particleResource = resArr[resIdx];

        // particle.vtable[+0x48](particle, particleResource) — SetParticleSystem
        // (or whatever slot 9 is on the runtime ParticleSystemInstance vtable).
        void** vtable = *reinterpret_cast<void***>(inst);

        if (vtable && vtable[9]) {
            using fn_set_t = void (*)(void* self, void* res);
            reinterpret_cast<fn_set_t>(vtable[9])(inst, particleResource);
        }

        // SetTransform(FromYawPitchOffset(yaw, pitch=0, offset=elem[0..0xb])).
        float yaw   = *reinterpret_cast<const float*>(elem + 0x0c);
        float pitch = 0.f;
        float offset[3] = {
            *reinterpret_cast<const float*>(elem + 0x00),
            *reinterpret_cast<const float*>(elem + 0x04),
            *reinterpret_cast<const float*>(elem + 0x08),
        };
        uint8_t transformBuf[32] = {};
        fn_Transform_FromYawPitchOffset(transformBuf, &yaw, &pitch, offset);
        fn_Geometry_SetTransform(inst, transformBuf);

        // Initial Update(Time(0)).
        uint8_t timeBuf[8] = {};
        fn_Time_Ctor_Float(timeBuf, 0.0f);
        if (vtable && vtable[1]) {
            using fn_upd_t = void (*)(void* self, void* time);
            reinterpret_cast<fn_upd_t>(vtable[1])(inst, timeBuf);
        }

        // 4. Append to particle-only array (+0xbd0) — drives per-frame Update.
        void** partSlot = reinterpret_cast<void**>(
            AppendToDynArray(self, MenuDLTOff::partInstArr_bd0, sizeof(void*)));
        if (partSlot) *partSlot = inst;

        // 5. Scene::AddObject + append id.
        unsigned int id = fn_Graphics_Scene_AddObject(scene, inst);
        uint32_t* idSlot = reinterpret_cast<uint32_t*>(
            AppendToDynArray(self, MenuDLTOff::sceneIdArr_bf0, sizeof(uint32_t)));
        if (idSlot) *idSlot = id;
        ++added;
    }

    printf("[MenuDLT::PhaseC] particles: added=%d/%d\n", added, particleCount);
    fflush(stdout);
}

// ============================================================
// Per-frame particle Update — iterate MenuDLT+0xbd0 and dispatch vtable+0x08.
// ============================================================
// Mirrors the inner loop of MenuDLT::Update (exe 0x14009b6e0):
//   for (i = 0; i < self+0xbd8; ++i) {
//     particle = ((Geometry**)self+0xbd0)[i];
//     particle->vtable[+0x08](particle, time);
//   }
// Without this, particles are added to the scene but never simulate — emitters
// don't spawn sprites, so chimneys don't smoke.
void MenuDialogLoopT_TickParticles(void* self, void* time) {
    if (!self || !time) return;

    void** arr   = Field<void**>(self,  MenuDLTOff::partInstArr_bd0);
    int32_t count = Field<int32_t>(self, MenuDLTOff::partInstCnt_bd8);

    if (!arr || count <= 0) return;
    for (int32_t i = 0; i < count; ++i) {
        void* inst = arr[i];
        if (!inst) continue;
        void** vtable = *reinterpret_cast<void***>(inst);
        if (vtable && vtable[1]) {
            using fn_t = void (*)(void* self, void* time);
            reinterpret_cast<fn_t>(vtable[1])(inst, time);
        }
    }

    // Match retail MenuDLT::Update ordering (exe 0x14009b6e0):
    //   per-instance Update loop  →  _frameIndex++  →  Scene::Clean
    // Order matters: incrementing _frameIndex BEFORE the Update loop would
    // make every shared-emit-state instance early-return on its first update;
    // and skipping Scene::Clean leaves Geometry dirty bits set, causing the
    // Renderer to read stale bounds/transforms each frame (visible flicker).
    if (fn_Graphics_ParticleInstance_IncrementInstanceIndex) {
        fn_Graphics_ParticleInstance_IncrementInstanceIndex();
    }
    void* scene = Field<void*>(self, 0xba0);
    if (scene && fn_Graphics_Scene_Clean) {
        fn_Graphics_Scene_Clean(scene);
    }
}

// ============================================================
// Phase A1: pre-PhaseB allocation + MenuScene load
// ============================================================
// Mirror of the pre-PhaseB block in MenuDLT::Load (exe 0x14009A930):
// loads Game/MenuScene/SceneN.rsc, allocates the 5 scene-graph objects
// (Renderer/Scene/SunLight/Transform/MeshInstance), wires the main mesh,
// adds the 3 global objects (sceneGeom/sunLight/mainMesh), and copies
// the deserialized transform into the live Transform.
//
// Cheap (~10ms): doesn't touch the 12K-15K element static-mesh array.
// Standalone calls this from the Loading splash's Stub_Startup so the
// MenuScene External + scene-graph fields are populated before the heavy
// PhaseB chunked iteration starts.
bool MenuDialogLoopT_LoadSceneA1(void* self) {
    // Required Runtime functions
    if (!fn_MemAlloc || !g_pMemAllocSingleton ||
        !fn_Graphics_Renderer_Ctor || !fn_Graphics_Scene_Ctor ||
        !fn_Graphics_SunLight_Ctor || !fn_Graphics_MeshInstance_Ctor ||
        !fn_Graphics_MeshInstance_SetMesh || !fn_Graphics_Scene_AddGlobalObject ||
        !fn_Time_Ctor_Float) {
        printf("[MenuDLT::LoadScene] missing required fn pointers — bailing out\n");
        return false;
    }

    // Required factory
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_ExternalFactory_FindOrLoad || !fn_Filename_CtorCStr || !fn_Filename_Dtor ||
        !fn_String_Format || !fn_String_Dtor || !fn_Random_Rand || !fn_IsDerivedFrom) {
        printf("[MenuDLT::LoadScene] missing factory/filename helpers — bailing out\n");
        return false;
    }

    void* factory = *g_pExternalFactorySingleton;

    // Engine::Random lives at self+0x168 (UpdateLoop base + DialogLoopT field area).
    void* sceneRng = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x168);
    uint32_t variant = fn_Random_Rand(sceneRng, 10);

    // Diagnostic lever: BANISHED_MENU_SCENE pins the menu scene index (0..9) so
    // the otherwise-random MenuScene/SceneN.rsc choice becomes deterministic.
    // Lets us reproduce the intermittent menu heap-overflow per-scene instead
    // of waiting for the RNG to land on the bad one. Unset = normal random pick.
    if (const char* pin = std::getenv("BANISHED_MENU_SCENE")) {
        unsigned forced = static_cast<unsigned>(std::atoi(pin)) % 10u;
        printf("[MenuDLT::LoadScene] BANISHED_MENU_SCENE pinned scene %u "
               "(rng would have picked %u)\n", forced, variant);
        variant = forced;
    }

    // Format the scene path (Game/MenuScene/Scene{0..9}.rsc).
    uint8_t stringBuf[16] = {};
    fn_String_Format(stringBuf, "Game/MenuScene/Scene%i.rsc", variant);

    uint8_t fnBuf[16] = {};
    // fn_Filename_CtorString takes a String (not a cstr). The original uses a String
    // produced by Format. Both fn_Filename_CtorString and fn_Filename_CtorCStr exist
    // in engine_funcs; we go through the String path because Format gave us a String.
    if (fn_Filename_CtorString) {
        fn_Filename_CtorString(fnBuf, stringBuf);
    } else {
        // Fallback: synthesize a cstring (the format output is a refcounted String;
        // peek inside to get the char data — String layout: ptr to data at +0x00).
        char path[64];
        std::snprintf(path, sizeof(path), "Game/MenuScene/Scene%u.rsc", variant);
        fn_Filename_CtorCStr(fnBuf, path);
    }

    void* obj = fn_ExternalFactory_FindOrLoad(factory, fnBuf);

    // Original validates via IsDerivedFrom against TypeInfo_MenuScene at exe 0x1401c59f0.
    // In standalone the Runtime factory tree doesn't have every base type registered,
    // so IsDerivedFrom NULL-derefs on the lookup. Trust FindOrLoad's return — if it
    // gave us a non-NULL External and the path resolved, it's the right type.
    void* menuScene = obj;

    Field<void*>(self, MenuDLTOff::menuSceneExt_b90) = menuScene;

    fn_Filename_Dtor(fnBuf);
    fn_String_Dtor(stringBuf);

    if (!menuScene) {
        printf("[MenuDLT::LoadScene] failed to load Game/MenuScene/Scene%u.rsc\n", variant);
        return false;
    }
    printf("[MenuDLT::LoadScene] loaded MenuScene variant %u at %p\n", variant, menuScene);

    // --- Allocate Renderer (0x50 bytes, alignment 4) ---
    void* renderer = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 4);
    if (renderer) fn_Graphics_Renderer_Ctor(renderer);
    Field<void*>(self, MenuDLTOff::renderer_b98) = renderer;

    // --- Allocate Graphics::Scene (0x78 bytes, alignment 8) ---
    void* gScene = fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8);
    if (gScene) fn_Graphics_Scene_Ctor(gScene);
    Field<void*>(self, MenuDLTOff::graphicsScene_ba0) = gScene;

    // --- Allocate SunLight (0x108 bytes, alignment 8) ---
    void* sunLight = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 8);
    if (sunLight) {
        fn_Graphics_SunLight_Ctor(sunLight);
        // Original writes vtable override at +0x00:
        //   *(SunLight*)sunLight = PTR_FUN_14015e290
        // We skip this — using the Runtime default vtable is fine for Phase A.
    }
    Field<void*>(self, MenuDLTOff::sunLight_bb0) = sunLight;

    // --- Allocate Transform (0x10C bytes, alignment 4) ---
    void* transform = fn_MemAlloc(g_pMemAllocSingleton, 0x10C, 4);
    if (transform) MenuScene_Helper_8800(transform);
    Field<void*>(self, MenuDLTOff::transform_bb8) = transform;

    // --- Allocate MeshInstance (0x120 bytes, alignment 8) ---
    void* mainMesh = fn_MemAlloc(g_pMemAllocSingleton, 0x120, 8);
    if (mainMesh) {
        fn_Graphics_MeshInstance_Ctor(mainMesh);
        // Original writes vtable override at +0x00:
        //   *(MeshInstance*)mainMesh = PTR_EnvironmentDescription_vslot10_14015e240
        // Skipped for Phase A.
    }
    Field<void*>(self, MenuDLTOff::mainMesh_ba8) = mainMesh;

    // --- SetMesh from MenuScene+0x70 (the GraphicsMesh ExternalPtr) ---
    void* meshExt = Field<void*>(menuScene, 0x70);
    if (mainMesh && meshExt) {
        fn_Graphics_MeshInstance_SetMesh(mainMesh, meshExt);
    }

    // --- Time(0.0f) construction + Update vtable[+0x08] dispatch on the MeshInstance ---
    if (mainMesh) {
        uint8_t timeBuf[8] = {};
        fn_Time_Ctor_Float(timeBuf, 0.0f);
        // Call vtable[+0x08] — typically Update(Time*) for the default Runtime
        // MeshInstance vtable. (Original code uses the overridden vtable but the
        // base Runtime vtable still has a valid Update slot here.)
        void** vtable = *reinterpret_cast<void***>(mainMesh);
        if (vtable && vtable[1]) {
            using fn_t = void (*)(void* self, void* time);
            reinterpret_cast<fn_t>(vtable[1])(mainMesh, timeBuf);
        }
    }

    // --- Capture scene Geometry pointer (MenuScene+0x28 = Geometry*) ---
    Field<void*>(self, MenuDLTOff::sceneGeom_bc0) = Field<void*>(menuScene, 0x28);

    // --- Set the +0x28 type/category field on each Geometry. Original:
    //   mainMesh->[+0x28] = 0   (default category)
    //   sunLight->[+0x28] = 0
    //   sceneGeom->[+0x28] = 1  (terrain category)
    if (mainMesh)  Field<uint32_t>(mainMesh, 0x28) = 0;
    if (sunLight)  Field<uint32_t>(sunLight, 0x28) = 0;
    void* sceneGeom = Field<void*>(self, MenuDLTOff::sceneGeom_bc0);
    if (sceneGeom) Field<uint32_t>(sceneGeom, 0x28) = 1;

    // --- Add to global scene list (terrain Geometry, SunLight, MeshInstance) ---
    if (gScene) {
        if (sceneGeom) fn_Graphics_Scene_AddGlobalObject(gScene, sceneGeom);
        if (sunLight)  fn_Graphics_Scene_AddGlobalObject(gScene, sunLight);
        if (mainMesh)  fn_Graphics_Scene_AddGlobalObject(gScene, mainMesh);
    }

    // --- Copy transform from MenuScene+0x88 into our Transform at self+0xbb8 ---
    if (transform) {
        const void* srcTransform = reinterpret_cast<const uint8_t*>(menuScene) + 0x88;
        MenuScene_CopyTransform(transform, srcTransform);
        // Cache transform[+0x28] / [+0x2C] (yaw/pitch scrolls) at MenuDLT+0xb7c/+0xb80.
        Field<float>(self, MenuDLTOff::scrollX_b7c) = Field<float>(transform, 0x28);
        Field<float>(self, MenuDLTOff::scrollY_b80) = Field<float>(transform, 0x2C);
    }

    // Skip TradeWork (gameSettings[0x16]) — DLC ship, out of scope.
    // Static-mesh PhaseB + particle PhaseC happen in their own public funcs;
    // the synchronous all-in-one wrapper below calls them here.
    printf("[MenuDLT::LoadSceneA1] OK: renderer=%p gScene=%p sunLight=%p transform=%p "
           "mainMesh=%p sceneGeom=%p staticMeshCount=%d\n",
           renderer, gScene, sunLight, transform, mainMesh, sceneGeom,
           Field<int32_t>(menuScene, 0x38));
    return true;
}

// ============================================================
// Phase A3: post-PhaseB particle iteration (small, single-call)
// ============================================================
// Thin wrapper around PhaseC for symmetry with A1. Standalone calls this
// from ChunkedSceneLoader after the chunked PhaseB completes; DLL-mode
// calls it from MenuDialogLoopT_LoadScene_PhaseA below.
void MenuDialogLoopT_LoadSceneA3(void* self) {
    void* menuScene = Field<void*>(self, MenuDLTOff::menuSceneExt_b90);
    void* gScene    = Field<void*>(self, MenuDLTOff::graphicsScene_ba0);
    if (!menuScene || !gScene) {
        printf("[MenuDLT::LoadSceneA3] WARN: menuScene=%p gScene=%p — skipping\n",
               menuScene, gScene);
        return;
    }
    MenuDialogLoopT_LoadParticles_PhaseC(self, menuScene, gScene);
}

// ============================================================
// Phase A: full synchronous Load — A1 + PhaseB + A3 (DLL-mode signature)
// ============================================================
// Preserves the original public function so DLL-mode hook wrappers and any
// synchronous caller (e.g. menudialogloopt_stubs.cpp's fallback path) keep
// working unchanged. Standalone splits the work via LoadSceneA1 +
// PhaseB_TickChunk + LoadSceneA3 driven from the Loading splash so the 542ms
// PhaseB freeze becomes invisible.
bool MenuDialogLoopT_LoadScene_PhaseA(void* self) {
    if (!MenuDialogLoopT_LoadSceneA1(self)) return false;
    void* menuScene = Field<void*>(self, MenuDLTOff::menuSceneExt_b90);
    void* gScene    = Field<void*>(self, MenuDLTOff::graphicsScene_ba0);
    MenuDialogLoopT_LoadStaticMeshes_PhaseB(self, menuScene, gScene);
    MenuDialogLoopT_LoadSceneA3(self);
    return true;
}

// ============================================================
// Phase A: Draw — render menu scene with sun lighting + fog
// ============================================================
void MenuDialogLoopT_DrawScene_PhaseA(void* self, float alpha, void* context) {
    void* menuScene = Field<void*>(self, MenuDLTOff::menuSceneExt_b90);
    if (!menuScene || !context) return;

    if (!fn_Math_Vector3_FromYawPitch || !fn_Graphics_SunLight_Set ||
        !fn_Video_Context_SetFog || !fn_Graphics_Renderer_Draw) {
        printf("[MenuDLT::DrawScene] missing fn pointers — skipping\n");
        return;
    }

    void* transform = Field<void*>(self, MenuDLTOff::transform_bb8);
    void* renderer  = Field<void*>(self, MenuDLTOff::renderer_b98);
    void* sunLight  = Field<void*>(self, MenuDLTOff::sunLight_bb0);
    void* gScene    = Field<void*>(self, MenuDLTOff::graphicsScene_ba0);
    if (!transform || !renderer || !sunLight || !gScene) return;

    // Per-frame camera update: aspect-aware fovX, near/far from helper-8660 set.
    MenuScene_Helper_8660(transform,
                          MenuSceneConst::kHelper8660_FovX,
                          MenuSceneConst::kHelper8660_NearZ,
                          MenuSceneConst::kHelper8660_FarZ);

    // SunLight::Set(self, dir*, sunVec4*, ambientVec4*).
    // Color uint32s are stored as ARGB-packed (A=high, R-G-B descending), which
    // means the LE byte layout of e.g. ambientColor (+0x194) is:
    //   ms[0x194] = B, ms[0x195] = G, ms[0x196] = R, ms[0x197] = A
    // The runtime expects each Vector4 in (R, G, B, 1.0) order, so we read the
    // bytes in REVERSE: (byte+2, byte+1, byte+0, 1.0). This mirrors the original
    // disasm (exe 0x14009b440) which feeds e.g. (ms[0x196], ms[0x195], ms[0x194])
    // for ambient and (ms[0x19A], ms[0x199], ms[0x198]) for sun.
    uint8_t* ms = reinterpret_cast<uint8_t*>(menuScene);

    // SunLight direction is yaw + pitch derived from MenuScene+0x1AC..+0x1B3.
    // Verified via disassembly of MenuDLT::Draw (exe 0x14009b440):
    //   yaw   = *(float*)(menuScene + 0x1AC)
    //   pitch = -*(float*)(menuScene + 0x1B0)   (XOR'd with sign-flip mask 0x80000000)
    // The struct labels these as sunIntensity/ambientIntensity but the disasm
    // reads them as a sun-direction yaw/pitch pair; defaults of 120°/65° fit
    // a directional-light orientation, not an intensity scalar.
    // FromYawPitch is `static Vector3 FromYawPitch(const Scalar& yaw, const Scalar& pitch)`
    // — three machine args (RCX=retval, RDX=yaw, R8=pitch). Earlier code passed
    // only 2 args, which left R8 holding garbage and produced a junk direction.
    float sunYaw   = *reinterpret_cast<float*>(ms + 0x1AC);
    float sunPitch = -*reinterpret_cast<float*>(ms + 0x1B0);

    uint8_t dirVec[16] = {};  // Vector3 = 12 bytes, padded to 16 for safety
    fn_Math_Vector3_FromYawPitch(dirVec, &sunYaw, &sunPitch);

    // Sun color Vector4 — read sun field at +0x198..+0x19A in BGR memory order,
    // pack as RGB (byte+2 → x, byte+1 → y, byte+0 → z, 1.0 → w).
    float sunColorVec[4];
    sunColorVec[0] = static_cast<float>(ms[0x19A]) / MenuSceneConst::kColorScale255;
    sunColorVec[1] = static_cast<float>(ms[0x199]) / MenuSceneConst::kColorScale255;
    sunColorVec[2] = static_cast<float>(ms[0x198]) / MenuSceneConst::kColorScale255;
    sunColorVec[3] = 1.0f;

    // Ambient color Vector4 — same byte-reverse pattern over the ambient field
    // at +0x194..+0x196.
    float ambientVec[4];
    ambientVec[0] = static_cast<float>(ms[0x196]) / MenuSceneConst::kColorScale255;
    ambientVec[1] = static_cast<float>(ms[0x195]) / MenuSceneConst::kColorScale255;
    ambientVec[2] = static_cast<float>(ms[0x194]) / MenuSceneConst::kColorScale255;
    ambientVec[3] = 1.0f;

    // Original arg order: SunLight::Set(self, dir, sunVec4, ambientVec4) — R8 is
    // sun, R9 is ambient.
    fn_Graphics_SunLight_Set(sunLight, dirVec, sunColorVec, ambientVec);

    // Push sky color into Video::Context+0x3b8/+0x288 and OR fog-enable bits.
    // Original disasm:
    //   ctx[+0x3B8..+0x3C4] = MenuScene[+0x1B4..+0x1C0]  (4 dwords = sky color RGBA)
    //   ctx[+0x288..+0x294] = same 4 dwords             (mirrored)
    //   ctx[+0x10] |= 0x10800                            (fog/sky enable bits)
    // Smoke test confirmed this is fog/sky tint, not clear color — without
    // scene geometry to render fog against, this is invisible.
    uint8_t* ctxBytes = reinterpret_cast<uint8_t*>(context);
    std::memcpy(ctxBytes + 0x3B8, ms + 0x1B4, 16);
    std::memcpy(ctxBytes + 0x288, ms + 0x1B4, 16);
    *reinterpret_cast<uint32_t*>(ctxBytes + 0x10) |= 0x10800;

    // Fog color Vector4 — same BGR-in-memory → RGB-Vector4 byte reverse over the
    // skyColor field at +0x19C..+0x19E. The 4th component is fog density (not 1.0).
    float fogColor[4];
    fogColor[0] = static_cast<float>(ms[0x19E]) / MenuSceneConst::kColorScale255;
    fogColor[1] = static_cast<float>(ms[0x19D]) / MenuSceneConst::kColorScale255;
    fogColor[2] = static_cast<float>(ms[0x19C]) / MenuSceneConst::kColorScale255;
    fogColor[3] = *reinterpret_cast<float*>(ms + 0x1A8);  // density

    float fogStart = *reinterpret_cast<float*>(ms + 0x1A0);
    float fogEnd   = *reinterpret_cast<float*>(ms + 0x1A4);
    fn_Video_Context_SetFog(context, fogStart, fogEnd, fogColor);

    // Mirror the small write the original does into Renderer+0x3C from
    // VideoConfig+0x30. This is a per-frame copy of some video setting.
    if (g_pVideoConfigSingleton && *g_pVideoConfigSingleton) {
        Field<uint32_t>(renderer, 0x3C) =
            Field<uint32_t>(*g_pVideoConfigSingleton, 0x30);
    }

    // Camera lives inside the Transform at +0x34.
    void* camera = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transform) + 0x34);

    fn_Graphics_Renderer_Draw(renderer, context, alpha, gScene, camera);
}

// ============================================================
// Per-frame camera derivation (from MainMenuDialog_impl_b6e0 + helper_83f0)
// ============================================================
// Mirror of the camera-animation block in MenuDLT::Update (exe 0x14009b6e0):
//
//   1. Animate scrollX (yaw) at MenuDLT+0xb78:
//        animYaw[+0xb78] += dt * 11.25f  (DAT_1401721b4 = spin scale)
//        sin(animYaw), cos(animYaw)
//        transform[+0x28] = cos(animYaw) * 2.0f + cachedYaw[+0xb7c]
//        (DAT_140172174 = 2.0f orbit radius — the same const used as helper-8660 nearZ)
//
//   2. Build a camera-eye position from the outer Transform fields and look-at:
//        fwd = Vector3::FromYawPitch(transform[+0x28], transform[+0x2C])
//        eye = lookAtTarget[+0x1C..+0x24] - fwd * distance[+0x30]
//        new = Math::Transform::FromLookAt(eye, lookAtTarget)
//        transform[+0x00..+0x18] = new   (overwrites pos+quat — first 28B)
//
//   3. Convert to Transform3 and apply to embedded Camera:
//        t3 = Math::Transform::AsTransform3(transform)
//        Graphics::Camera::SetCameraToWorld(transform+0x34, t3)
//
// The deserialized values from .rsc are read but the leading 28B (Math::Transform
// pos+quat) are overwritten each frame by FromLookAt. Only the lookAtTarget at
// +0x1C..+0x27, the (animated) yaw at +0x28, the pitch at +0x2C, and the
// distance at +0x30 actually matter as inputs.
void MenuDialogLoopT_UpdateCamera_PhaseA(void* self, float deltaSeconds) {
    void* menuScene = Field<void*>(self, MenuDLTOff::menuSceneExt_b90);
    if (!menuScene) return;

    void* transform = Field<void*>(self, MenuDLTOff::transform_bb8);
    if (!transform) return;

    if (!fn_Math_Vector3_FromYawPitch || !fn_Math_Transform_FromLookAt ||
        !fn_Transform_AsTransform3 || !fn_Camera_SetCameraToWorld) {
        static int warned = 0;
        if (warned++ < 3) {
            printf("[MenuDLT::UpdateCamera] missing fn ptrs (FromYP=%p FromLA=%p "
                   "AsT3=%p SetCTW=%p) — skipping\n",
                   (void*)fn_Math_Vector3_FromYawPitch,
                   (void*)fn_Math_Transform_FromLookAt,
                   (void*)fn_Transform_AsTransform3,
                   (void*)fn_Camera_SetCameraToWorld);
        }
        return;
    }

    // 1. Animate yaw at MenuDLT+0xb78 (live) using cached scrollX at +0xb7c.
    //    Constants from the original exe (verified via Ghidra read_memory):
    //      DAT_1401721b4 = 11.25  (yaw spin rate, *degrees* per AsSeconds unit)
    //      DAT_140172174 = 2.0    (orbit radius — also doubles as helper-8660 nearZ)
    //
    // Banished's Math::Scalar uses **degrees**, so animYaw accumulates in
    // degrees and the cosine must be evaluated as cos(deg2rad(animYaw)).
    // The original calls Math::Scalar::SinCos which takes degrees; using
    // std::cos directly without conversion treats animYaw as radians and
    // produces a ~60x-faster wiggle (period ~0.56s vs the intended ~32s).
    static constexpr float kYawSpinRate    = 11.25f;     // deg/sec
    static constexpr float kYawOrbitRadius = 2.0f;       // deg
    static constexpr float kDegToRad       = 3.14159265358979323846f / 180.0f;
    constexpr uintptr_t animYawOff = 0xb78;

    float animYaw = Field<float>(self, animYawOff) + deltaSeconds * kYawSpinRate;
    Field<float>(self, animYawOff) = animYaw;

    float cosAnim = std::cos(animYaw * kDegToRad);
    float cachedYaw = Field<float>(self, MenuDLTOff::scrollX_b7c);

    // Update transform[+0x28] (yaw input to FromYawPitch) with the orbit offset.
    Field<float>(transform, 0x28) = cosAnim * kYawOrbitRadius + cachedYaw;

    // 2. Build the eye position using FromYawPitch direction + lookAtTarget - dist.
    //    Inputs read from outer Transform:
    //      yaw      = transform[+0x28]  (just updated)
    //      pitch    = transform[+0x2C]  (deserialized from .rsc)
    //      target   = transform[+0x1C..+0x24]  (Vector3 lookAtTarget)
    //      distance = transform[+0x30]
    float yawScalar   = Field<float>(transform, 0x28);
    float pitchScalar = Field<float>(transform, 0x2C);
    float distance    = Field<float>(transform, 0x30);
    float* targetPtr  = reinterpret_cast<float*>(
        reinterpret_cast<uintptr_t>(transform) + 0x1C);

    float fwdVec[4] = {};  // Vector3 + pad
    fn_Math_Vector3_FromYawPitch(fwdVec, &yawScalar, &pitchScalar);

    float eyeVec[3];
    eyeVec[0] = targetPtr[0] - fwdVec[0] * distance;
    eyeVec[1] = targetPtr[1] - fwdVec[1] * distance;
    eyeVec[2] = targetPtr[2] - fwdVec[2] * distance;

    // FromLookAt returns a Math::Transform (28B = pos vec3 + quat).
    uint8_t newTransformBuf[32] = {};
    fn_Math_Transform_FromLookAt(newTransformBuf, eyeVec, targetPtr);

    // Copy the 28 bytes back into transform[+0x00..+0x1B] (overwrites pos+quat —
    // the lookAtTarget at +0x1C and yaw/pitch/distance at +0x28+ are preserved).
    std::memcpy(transform, newTransformBuf, 28);

    // 3. Convert to Transform3 (48B = 3x4 matrix) and apply to Camera at +0x34.
    uint8_t t3Buf[64] = {};  // Transform3 = 48B, padded
    fn_Transform_AsTransform3(transform, t3Buf);
    void* camera = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transform) + 0x34);
    fn_Camera_SetCameraToWorld(camera, t3Buf);

    // Diagnostic: log the first few frames so we can verify the camera transform
    // is non-identity (the underlying bug we're fixing).
    static int logCount = 0;
    if (logCount++ < 5) {
        float* pos = reinterpret_cast<float*>(transform);
        printf("[MenuDLT::UpdateCamera] frame=%d animYaw=%.3f yaw=%.3f pitch=%.3f "
               "dist=%.3f target=(%.2f,%.2f,%.2f) eye=(%.2f,%.2f,%.2f) "
               "camPos=(%.2f,%.2f,%.2f)\n",
               logCount, animYaw, yawScalar, pitchScalar, distance,
               targetPtr[0], targetPtr[1], targetPtr[2],
               eyeVec[0], eyeVec[1], eyeVec[2],
               pos[0], pos[1], pos[2]);
    }
}

// ============================================================
// MenuDialogLoopT_TickAudio — per-frame Audio::Device::Update
// ============================================================
// Pull Audio::Device live from MainLoop->m_engine+0x6d0 (offset confirmed
// by Engine::Engine::GetAudioDevice at Runtime 0x18000c970:
// `return *(Device**)(this + 0x6d0);`).
//
// Note: CLAUDE.md/MEMORY.md sometimes lists Audio::Device at engine+0x6e0,
// but Ghidra disassembly + engine/reimpl/mainloop.cpp's GetAudioDevice
// helper both confirm +0x6d0. (+0x6e0 is the Input::Device offset.)
void MenuDialogLoopT_TickAudio(void* time) {
    if (!fn_AudioDevice_Update) {
        // First-call diagnostic so we notice if the resolve ever silently
        // returns NULL on a future build. Logged once.
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudio] WARN: fn_AudioDevice_Update is NULL\n");
        }
        return;
    }
    if (!time) return;
    if (!g_pMainLoopSingleton || !*g_pMainLoopSingleton) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudio] WARN: MainLoop singleton not populated\n");
        }
        return;
    }
    void* mainLoop = *g_pMainLoopSingleton;
    void* engine   = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(mainLoop) + 0x78);  // MainLoop::m_engine
    if (!engine) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudio] WARN: MainLoop->m_engine is NULL\n");
        }
        return;
    }
    void* audioDevice = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(engine) + 0x6d0);  // Engine::m_audioDevice
    if (!audioDevice) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudio] WARN: Engine[+0x6d0] (Audio::Device) is NULL\n");
        }
        return;
    }

    // Trace first few ticks so we can confirm the call is firing.
    static int tickCount = 0;
    if (tickCount < 3) {
        printf("[MenuDLT::TickAudio] tick=%d audioDevice=%p time=%p -> Audio::Device::Update\n",
               tickCount, audioDevice, time);
        ++tickCount;
    }

    fn_AudioDevice_Update(audioDevice, time);
}

// ============================================================
// MenuDialogLoopT_LoadMusic — load Audio/Music/Menu.rsc into self+0xb88
// ============================================================
// Mirror of the first block of MenuDLT::Load (exe 0x14009a930), which runs
// right after Engine::DialogLoopT::Load:
//   Filename f("Audio/Music/Menu.rsc");
//   External* e = ExternalFactory::FindOrLoad(_singleton, f);
//   if (e && IsDerivedFrom(e, SoundEffect typeinfo)) keep; else null;
//   *(External**)(self+0xb88) = e;
// Standalone trusts FindOrLoad's return and skips the IsDerivedFrom check (the
// Runtime factory tree is incomplete → IsDerivedFrom NULL-derefs; same
// convention as LoadSceneA1's MenuScene load). Playback is started later in
// StartMusic (the original splits Load from the Startup-time PlayEffect).
bool MenuDialogLoopT_LoadMusic(void* self) {
    if (!self) return false;
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_ExternalFactory_FindOrLoad || !fn_Filename_CtorCStr || !fn_Filename_Dtor) {
        printf("[MenuDLT::LoadMusic] missing factory/filename helpers — skipping\n");
        return false;
    }

    void* factory = *g_pExternalFactorySingleton;
    uint8_t fnBuf[16] = {};  // System::Filename
    fn_Filename_CtorCStr(fnBuf, "Audio/Music/Menu.rsc");
    void* music = fn_ExternalFactory_FindOrLoad(factory, fnBuf);
    fn_Filename_Dtor(fnBuf);

    Field<void*>(self, MenuDLTOff::musicExt_b88) = music;
    printf("[MenuDLT::LoadMusic] Audio/Music/Menu.rsc -> %p (self+0xb88)\n", music);
    return music != nullptr;
}

// ============================================================
// MenuDialogLoopT_StartMusic — register audio scene + play menu music
// ============================================================
// Mirror of the audio portion of MenuDLT::Startup (exe 0x14009b930), run AFTER
// Engine::DialogLoopT::Startup:
//   *(Scene**)(self+0xd0) = (Scene*)(self+0xb30);   // dialog base audio scene
//   Audio::Scene::PlayEffect(self+0xb30, *(self+0xb88), 2, &Time(0));
// soundType/channel 2 is the music channel (same channel Pause toggles in
// MenuDLT::Update). PlayEffect's 1st arg is an ExternalPtr<SoundEffect> (the
// 8-byte External* value stored at +0xb88); the 4th is a const Time& (pointer).
void MenuDialogLoopT_StartMusic(void* self) {
    if (!self) return;

    // 1. Register the loop's embedded Audio::Scene (+0xb30) with the
    //    DialogInstance base at +0xd0 (where UI sound effects route). The
    //    original writes this even before PlayEffect.
    void* audioScene = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self) + MenuDLTOff::audioScene_b30);
    Field<void*>(self, 0xd0) = audioScene;

    // 2. Play the looping menu music.
    void* music = Field<void*>(self, MenuDLTOff::musicExt_b88);
    if (!music) {
        printf("[MenuDLT::StartMusic] no music External at +0xb88 — silent menu\n");
        return;
    }
    if (!fn_Audio_Scene_PlayEffect) {
        printf("[MenuDLT::StartMusic] fn_Audio_Scene_PlayEffect unresolved — skipping\n");
        return;
    }

    int64_t timeZero = 0;  // System::Time(0) — 8-byte zero ticks, passed by const ref.
    uint32_t handle = fn_Audio_Scene_PlayEffect(
        audioScene, reinterpret_cast<uint64_t>(music), 2, &timeZero);
    printf("[MenuDLT::StartMusic] PlayEffect(scene=%p music=%p chan=2) -> handle=%u\n",
           audioScene, music, handle);
    fflush(stdout);
}

// ============================================================
// MenuDialogLoopT_BindUISounds — bind UI hover/press/click sound effects
// ============================================================
// Mirror of the sound-binding block of OptionsDialog_helper_9260 (exe
// 0x140099260) — the per-UI setup MenuDLT::Startup invokes right after
// Engine::DialogLoopT::Startup. It copies five SoundEffect Externals from the
// KeyMap/PlacementTool container (g_pKeyMapSingleton, populated by
// PlacementTool_Ctor at 0x140099db0) into the UserInterface's sound table at
// UI+0x98+id*8.
//
// UserInterface::Play(ui, id) (Runtime 0x1801223d0) reads UI+0x98+id*8 and, if
// non-null, plays it through the audio scene at UI+0x90. The element framework
// fires Play automatically — confirmed callers: ButtonElement::Update plays
// id 4 on press and id 5 on click; Element::Update plays id 3 on drag-drop;
// Slider/Scroll::Update play their own. All run inside the
// Engine::DialogLoopT::Update we already invoke each frame, so NO explicit
// per-element call is needed — only the sound table + audio scene must be set.
//
//   soundId 2 (UI+0xa8) <- KeyMap+0x70  Audio/UI/BeginDragEffect
//   soundId 3 (UI+0xb0) <- KeyMap+0x78  Audio/UI/EndDragEffect    (drag drop)
//   soundId 4 (UI+0xb8) <- KeyMap+0x60  Audio/UI/ButtonDownEffect (press)
//   soundId 5 (UI+0xc0) <- KeyMap+0x68  Audio/UI/ButtonUpEffect   (click)
//   soundId 6 (UI+0xc8) <- KeyMap+0x58  Audio/UI/AlertEffect
//
// UI is embedded at MenuDLT+0x40 (DialogLoopT layout), so UI+0x90 == self+0xd0
// — exactly the field StartMusic sets to the embedded Audio::Scene (+0xb30), so
// these sounds route through the same scene we tick each frame (TickAudioScene).
//
// The five SoundEffect Externals must already be loaded into the KeyMap
// container: EnsureUISoundsLoaded() loads them on demand before this runs.
// Copying a null slot is harmless — Play no-ops on a null entry. Each
// slot is an 8-byte Object::ExternalPtr<SoundEffect> (raw External*), copied
// by value exactly as helper_9260 does (no SetSound call).
void MenuDialogLoopT_BindUISounds(void* self) {
    if (!self) return;
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) {
        printf("[MenuDLT::BindUISounds] KeyMap container not populated — UI silent\n");
        return;
    }
    void* keyMap = *g_pKeyMapSingleton;
    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x40);

    Field<void*>(ui, 0xa8) = Field<void*>(keyMap, 0x70);  // BeginDrag  (id 2)
    Field<void*>(ui, 0xb0) = Field<void*>(keyMap, 0x78);  // EndDrag    (id 3)
    Field<void*>(ui, 0xb8) = Field<void*>(keyMap, 0x60);  // ButtonDown (id 4)
    Field<void*>(ui, 0xc0) = Field<void*>(keyMap, 0x68);  // ButtonUp   (id 5)
    Field<void*>(ui, 0xc8) = Field<void*>(keyMap, 0x58);  // Alert      (id 6)

    printf("[MenuDLT::BindUISounds] ui=%p beginDrag=%p endDrag=%p btnDown=%p "
           "btnUp=%p alert=%p\n",
           ui, Field<void*>(ui, 0xa8), Field<void*>(ui, 0xb0),
           Field<void*>(ui, 0xb8), Field<void*>(ui, 0xc0), Field<void*>(ui, 0xc8));
    fflush(stdout);
}

// ============================================================
// MenuDialogLoopT_TickAudioScene — per-frame Audio::Scene::Update
// ============================================================
// Mirror of the Audio::Scene::Update call in MenuDLT::Update (exe 0x14009b6e0):
//   if (!Application::IsPaused()) {
//       Audio::Scene::Update(self+0xb30, *(Time*)time, Time(0),
//                            &Random(self+0x168), &listenerVec3{0,0,0}, 0.0f);
//   }
// The standalone main menu is never paused, so we always tick (and skip the
// Application::IsPaused / Audio::Scene::Pause(channel 2) transition logic). This
// is what advances the music stream each frame — without it PlayEffect starts
// the music but it never progresses.
void MenuDialogLoopT_TickAudioScene(void* self, void* time) {
    if (!self || !time) return;
    if (!fn_Audio_Scene_Update) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudioScene] WARN: fn_Audio_Scene_Update is NULL — "
                   "menu music will not advance\n");
        }
        return;
    }

    void* audioScene = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self) + MenuDLTOff::audioScene_b30);
    void* rng = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x168);

    int64_t elapsed  = *reinterpret_cast<int64_t*>(time);  // *(u64*)time — System::Time ticks
    int64_t timeZero = 0;                                  // Time(0), passed by value
    float listenerVec3[3] = { 0.0f, 0.0f, 0.0f };          // origin listener (2D menu music)

    fn_Audio_Scene_Update(audioScene, elapsed, timeZero, rng, listenerVec3, 0.0f);

    static int n = 0;
    if (n < 3) {
        ++n;
        printf("[MenuDLT::TickAudioScene] Audio::Scene::Update(scene=%p) tick=%d\n",
               audioScene, n);
    }
}
