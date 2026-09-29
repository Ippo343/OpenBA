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
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Configuration::_singleton
#include "engine/MainLoop.h"   // ENDGAME B.3.9 SS2: Engine::MainLoop::_singleton
// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::{Camera,Scene,Geometry,
// MeshInstance,GraphicsMesh,SunLight,ParticleSystemInstance} and Video::Context
// are real classes. These go AHEAD of "resolve.h" — resolve.h pulls
// <windows.h>, whose <wingdi.h> declares a global `Rectangle` FUNCTION that
// hides `Volume::Rectangle`'s class name inside Volume.h's own static_asserts
// (Camera.h and Geometry.h include Volume.h).
#include "graphics/Camera.h"
#include "graphics/Scene.h"
#include "graphics/Mesh.h"                // MeshInstance
#include "graphics/GraphicsMesh.h"
#include "graphics/SunLight.h"
#include "graphics/ParticleInstance.h"    // ParticleSystemInstance
#include "Game/MenuDialogLoopT.h"         // _loadBit (LoadSceneA1)
#include "video/Context.h"
#include "options.h"
#include <new>                            // B.3.9 GFX: placement new
#include "MenuScene.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "system/Filename.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "audio/Audio.h"       // ENDGAME B.3.7 callers (K4)

// Wave 9 lane S probe (runtime/src/audio/audio.cpp).

// ============================================================
// MenuDLT fields (Engine::DialogLoopT subclass)
// ============================================================
// Caller batch CB6: the helpers below take the loop as `void* self` (their
// callers pass `this`); its fields are the named members of the real class
// code/Game/Game/MenuDialogLoopT.h (+0xb30 the embedded Audio::Scene, +0xb68
// .. +0xbf8 the menu fields, the three {data, count|cap} arrays at +0xbd0/
// +0xbe0/+0xbf0). The old MenuDLTOff offset table went with its last user.
static inline MenuDialogLoopT* Loop(void* self) {
    return static_cast<MenuDialogLoopT*>(self);   // B.3.8 bridge
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
    // ??0Transform@Math@@QEAA@XZ  retail 0x180015080
    *reinterpret_cast<Math::Transform*>(transformObj) = Math::Transform();

    // Zero scalars at +0x1C (qword) and +0x24 (dword) per original.
    Field<uint64_t>(transformObj, 0x1C) = 0;
    Field<uint32_t>(transformObj, 0x24) = 0;

    // ?Identity@Transform3@Math@@SA?AV12@XZ  retail 0x1800347f0.
    // Returns by value onto a caller-owned object; the hidden-return-slot shape
    // (and the stale-RCX wild write it once caused) went with the C seam.
    Math::Transform3 ident = Math::Transform3::Identity();

    // Frustum ctor takes 4 Scalar* args in order (near, far, fovX, fovY) — the
    // first two are stored as raw plane distances, the last two are interpreted
    // as half-FOV angles (in degrees) for sin/cos. Confirmed by decompile of
    // FUN_140098800 + Volume::Frustum::Frustum (Runtime 0x1800fa1e0).
    float fovX = MenuSceneConst::kHelper8800_FovX;
    float fovY = MenuSceneConst::kHelper8800_FovY;
    float nearZ = MenuSceneConst::kHelper8800_NearZ;
    float farZ  = MenuSceneConst::kHelper8800_FarZ;

    uint8_t frustumBuf[0x80] = {};
    RtVolume_Frustum_Ctor_Perspective(reinterpret_cast<::Frustum*>(frustumBuf), reinterpret_cast<const Math::Scalar*>(&nearZ), reinterpret_cast<const Math::Scalar*>(&farZ), reinterpret_cast<const Math::Scalar*>(&fovX), reinterpret_cast<const Math::Scalar*>(&fovY));

    // Camera ctor: self = transform+0x34, frustum, transform3, isPerspective=true
    void* cameraObj = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transformObj) + 0x34);
    RtGraphics_Camera_Ctor(reinterpret_cast<RtGraphics::Camera*>(cameraObj), reinterpret_cast<const ::Frustum*>(frustumBuf), reinterpret_cast<const Math::Transform3*>(&ident), true);
    return transformObj;
}

// Mirror of FUN_140098660 (exe 0x140098660) — per-frame camera frustum update.
// Original handles widescreen FOV adjustment via sinf/atanf math; for Phase A
// we use the simple fovX = aspect * fovY frustum and skip the ultrawide clamp.
// This matches the original's behavior for aspect <= 16/9.
static void MenuScene_Helper_8660(void* transformObj, float fovY_deg, float nearZ, float farZ) {
    // B.3.5: the `!fn_Graphics_Camera_SetFrustum` readiness term went with the
    // door; B3R: the Frustum ctor door's too. The singleton object test stays.
    if (!::Video::Configuration::_singleton) {
        return;
    }
    void* videoConfig = ::Video::Configuration::_singleton;
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
    RtVolume_Frustum_Ctor_Perspective(reinterpret_cast<::Frustum*>(frustumBuf), reinterpret_cast<const Math::Scalar*>(&nearZ), reinterpret_cast<const Math::Scalar*>(&farZ), reinterpret_cast<const Math::Scalar*>(&fovX), reinterpret_cast<const Math::Scalar*>(&fovY_deg));

    // Camera lives at transform+0x34.
    void* cameraObj = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transformObj) + 0x34);
    reinterpret_cast<Graphics::Camera*>(cameraObj)->SetFrustum(
        *reinterpret_cast<const ::Frustum*>(frustumBuf));   // B.3.5 bridge
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
//
// ENDGAME B.3.2 step 2: the body WAS a hand-written copy of `rtcoll::SetSizeCore`
// specialised to a single append. Retail MenuDLT::Load (0x14009a930, re-decompiled
// 2026-09-13) inlines the THREE-argument form six times — once per array per
// loop — with the new element as the fill: `n = count+1; if (n != count) {...}`
// (the exact-equal early-out, always taken), `_size = n`, `if (cap < n) { cap =
// n; if (count != 0) cap = n*3; MemAlloc(cap << 3 | << 2, 8 | 4) }`, copy the
// prefix on a move else start at count, `dst[i] = i < count ? old[i] : elem`,
// skip a null computed address, `if (old != data) MemFree(old)`. The capacity
// spelling `(count != 0) ? n*3 : n` equals the reference `n == old+1 && old != 0
// ? n*3 : n` BECAUSE n is count+1 by construction at every one of the six call
// sites — there is no other caller. So each site is `Array<T>::SetSize(n, elem)`
// on a trait-derived row (void* / uint32_t: byteCopy, no init, no destroy,
// align 8 / 4 — the two alignments retail passes). The previous port's
// "return the slot, caller stores it" split and its `oldData && newData` memcpy
// guard produced the same bytes; the fill form is retail's actual shape.
template <typename T>
static void AppendToDynArray(void* self, uintptr_t arrOff, const T& value) {
    // B.3.2 bridge: MenuDLT+0xbd0/+0xbe0/+0xbf0 are raw headers in the DialogLoopT
    // subclass, which is still untyped bytes.
    auto* arr = reinterpret_cast<Collection::Array<T>*>(
        reinterpret_cast<uint8_t*>(self) + arrOff);
    arr->SetSize(arr->GetSize() + 1, value);
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
    void* inst = System::MemoryAllocator::MemAlloc(0x120, 8);
    if (!inst) return 0;
    ::new (inst) Graphics::MeshInstance();   // B.3.9 EXA: the real class (exe table 0x14015e240 is its own)

    // 2. Append to MenuDLT+0xbe0 (shared instance list).
    AppendToDynArray<void*>(self, offsetof(MenuDialogLoopT, _instanceArray), inst);

    // 3. Per-element setup — mirror MenuScene_helper_3b60.
    int16_t meshIdx = *reinterpret_cast<const int16_t*>(elem + 0x1c);
    int16_t matIdx  = *reinterpret_cast<const int16_t*>(elem + 0x1e);

    // Defensive bound check on meshIdx — keeps id arrays in sync if a future
    // serializer regression ever produces an out-of-range index.
    if (meshIdx < 0 || meshIdx >= meshArrCount) {
        uint32_t id0 = reinterpret_cast<Graphics::Scene*>(scene)->AddObject(
            reinterpret_cast<Graphics::Geometry*>(inst));   // B.3.5 bridge
        AppendToDynArray<uint32_t>(self, offsetof(MenuDialogLoopT, _sceneIdArray), id0);
        *outSkippedBadIdx = true;
        return 0;
    }

    // SetMesh(meshArray[meshIdx]) — value passed is the 8-byte ExternalPtr.
    void* meshExtPtr = meshArr[meshIdx];
    reinterpret_cast<Graphics::MeshInstance*>(inst)->SetMesh(
        Graphics::ExternalPtrHandle{meshExtPtr});   // B.3.5 bridge

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
    Math::Transform transformBuf = Math::Transform::FromYawPitchOffset(
        yaw, pitch, *reinterpret_cast<const Math::Vector3*>(offset));
    reinterpret_cast<Graphics::Geometry*>(inst)->SetTransform(transformBuf);   // B.3.5 bridge

    // Copy 3 dwords + scale into MeshInstance fields. Last value is g_fUVScale
    // (exe DAT_14017214c, hardcoded 1.0f to skip an extra ResolveAddr).
    Field<uint32_t>(inst, 0x10c) = *reinterpret_cast<const uint32_t*>(elem + 0x0c);
    Field<uint32_t>(inst, 0x110) = *reinterpret_cast<const uint32_t*>(elem + 0x10);
    Field<uint32_t>(inst, 0x114) = *reinterpret_cast<const uint32_t*>(elem + 0x14);
    Field<float>   (inst, 0x118) = 1.0f;

    // Iterate GetMaterialList(matIdx) and SetMaterialInstance per slot.
    int32_t matSlots = 0;
    // B.3.5: the two door-readiness terms went with their doors; `meshExtPtr` is
    // an object test and stays.
    if (meshExtPtr) {
        // B.3.5 bridge — `?GetMaterialList@GraphicsMesh@Graphics@@…H@Z` @0x1800126c0,
        // the ONE-ARG overload (the arg-less 0x1800126b0 twin returns element 0).
        // The door took the 8-byte ExternalPtr VALUE as `this`, so the receiver is
        // that same value reinterpreted; the member returns a REFERENCE where the
        // door returned its address.
        const void* matList = &reinterpret_cast<const Graphics::GraphicsMesh*>(meshExtPtr)
                                   ->GetMaterialList(matIdx);   // B.3.5 bridge
        if (matList) {
            // ENDGAME B.3.2 step 2: `?GetMaterialList@GraphicsMesh@Graphics@@`
            // returns `const Collection::Array<Object::ExternalPtr<
            // Graphics::MaterialInstance>>&` (the mangled name in
            // engine_funcs.cpp spells it out), and this walk read it by raw
            // offset. `ExternalPtr<T>` is 8 bytes and trivially copyable
            // (COLLECTION_INVENTORY.md §5.1 requires it to stay so), which
            // makes `Array<void*>` layout-equivalent. This is a READ-ONLY walk
            // — no SetSize, no destructor — so the spec policy is never
            // consulted and the trait-derived row is correct.
            // B.3.2 bridge: the cast goes when the returned type is spelled out.
            const Collection::Array<void*>& mats =
                *reinterpret_cast<const Collection::Array<void*>*>(matList);
            // The `mats.GetData()` term is the old `&& matData`, kept because
            // nothing here proves GetSize() is 0 whenever _data is null.
            for (int32_t m = 0; m < mats.GetSize() && mats.GetData(); ++m) {
                reinterpret_cast<Graphics::MeshInstance*>(inst)->SetMaterialInstance(
                    m, Graphics::ExternalPtrHandle{mats[m]});   // B.3.5 bridge
            }
            matSlots = mats.GetSize();
        }
    }

    // Vtable+0x08 (Update) with Time(0).
    // B.3.5 bridge — slot 1 of `Graphics::Geometry` IS `Update(const Time&)`
    // (Geometry.h's measured slot list). The receiver is a PROVED MeshInstance:
    // 0x120 bytes (sizeof) from MemAlloc, run through `??0MeshInstance@Graphics@@`
    // above, so the virtual call reads the same vptr the hand-inlined slot read.
    uint8_t timeBuf[8] = {};
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 0.0f);
    reinterpret_cast<Graphics::Geometry*>(inst)->Update(
        *reinterpret_cast<const System::Time*>(timeBuf));   // B.3.5 bridge

    // 4. Scene::AddObject + append id.
    uint32_t id = reinterpret_cast<Graphics::Scene*>(scene)->AddObject(
        reinterpret_cast<Graphics::Geometry*>(inst));   // B.3.5 bridge
    AppendToDynArray<uint32_t>(self, offsetof(MenuDialogLoopT, _sceneIdArray), id);

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
    // B.3.5: SetMesh / Scene::AddObject / Geometry::SetTransform are members now,
    // so their readiness terms and their `%p`s are gone; B.3.9 EXA took the
    // MeshInstance ctor door's (the class is constructed). MemoryAllocator stays;
    // the Time ctor is called by name (B3R).
    if (!System::MemoryAllocator::_singleton) {
        printf("[MenuDLT::PhaseB] missing required fn ptrs — skipping\n");
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
    // B.3.5: Scene::AddObject and Geometry::SetTransform are members now; the
    // Time ctor door stays. ENDGAME B.3.9 GFX (B9-13): the
    // ParticleSystemInstance CTOR door is gone -- the class's real ctor below.
    if (!System::MemoryAllocator::_singleton) {
        printf("[MenuDLT::PhaseC] missing required fn ptrs — skipping\n");
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
        void* inst = System::MemoryAllocator::MemAlloc(0xe0, 8);
        if (!inst) continue;
        ::new (inst) Graphics::ParticleSystemInstance();   // B.3.9 GFX: the real ctor

        // 2. Append to all-instance array (+0xbe0).
        AppendToDynArray<void*>(self, offsetof(MenuDialogLoopT, _instanceArray), inst);

        // 3. helper_3ca0 inline:
        //    Element layout: elem+0x00 = Vector3 offset, elem+0x0c = yaw,
        //                    elem+0x10 = resource index.
        const uint8_t* elem = elemArr + static_cast<size_t>(i) * 0x14;
        int32_t resIdx = *reinterpret_cast<const int32_t*>(elem + 0x10);
        void* particleResource = resArr[resIdx];

        // B.3.5 bridge — the hand-inlined `vtable[9]` IS
        // `?SetParticle@ParticleInstance@Graphics@@` @0x180104a00 (slot 9,
        // ParticleInstance.h's measured slot list). The receiver is a PROVED
        // ParticleSystemInstance: 0xe0 bytes (sizeof) from MemAlloc, run through
        // `??0ParticleSystemInstance@Graphics@@` just above.
        reinterpret_cast<Graphics::ParticleSystemInstance*>(inst)->SetParticle(
            Graphics::ExternalPtrHandle{particleResource});   // B.3.5 bridge

        // SetTransform(FromYawPitchOffset(yaw, pitch=0, offset=elem[0..0xb])).
        float yaw   = *reinterpret_cast<const float*>(elem + 0x0c);
        float pitch = 0.f;
        float offset[3] = {
            *reinterpret_cast<const float*>(elem + 0x00),
            *reinterpret_cast<const float*>(elem + 0x04),
            *reinterpret_cast<const float*>(elem + 0x08),
        };
        Math::Transform transformBuf = Math::Transform::FromYawPitchOffset(
            yaw, pitch, *reinterpret_cast<const Math::Vector3*>(offset));
        reinterpret_cast<Graphics::Geometry*>(inst)->SetTransform(transformBuf);   // B.3.5 bridge

        // Initial Update(Time(0)) — slot 1 of Graphics::Geometry.
        uint8_t timeBuf[8] = {};
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 0.0f);
        reinterpret_cast<Graphics::Geometry*>(inst)->Update(
            *reinterpret_cast<const System::Time*>(timeBuf));   // B.3.5 bridge

        // 4. Append to particle-only array (+0xbd0) — drives per-frame Update.
        AppendToDynArray<void*>(self, offsetof(MenuDialogLoopT, _particleArray), inst);

        // 5. Scene::AddObject + append id.
        uint32_t id = reinterpret_cast<Graphics::Scene*>(scene)->AddObject(
            reinterpret_cast<Graphics::Geometry*>(inst));   // B.3.5 bridge
        AppendToDynArray<uint32_t>(self, offsetof(MenuDialogLoopT, _sceneIdArray), id);
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

    void** arr   = *reinterpret_cast<void***>(Loop(self)->_particleArray);        // {data,
    int32_t count = *reinterpret_cast<int32_t*>(Loop(self)->_particleArray + 8);  //  count|cap}

    if (!arr || count <= 0) return;
    for (int32_t i = 0; i < count; ++i) {
        void* inst = arr[i];
        if (!inst) continue;
        // B.3.5 bridge — slot 1 of Graphics::Geometry is `Update(const Time&)`.
        // Retail reads the +0xbd0 array AS `Geometry**` (the banner above quotes
        // exe 0x14009b6e0 doing exactly that), so the receiver is proved.
        reinterpret_cast<Graphics::Geometry*>(inst)->Update(
            *reinterpret_cast<const System::Time*>(time));   // B.3.5 bridge
    }

    // Match retail MenuDLT::Update ordering (exe 0x14009b6e0):
    //   per-instance Update loop  →  _frameIndex++  →  Scene::Clean
    // Order matters: incrementing _frameIndex BEFORE the Update loop would
    // make every shared-emit-state instance early-return on its first update;
    // and skipping Scene::Clean leaves Geometry dirty bits set, causing the
    // Renderer to read stale bounds/transforms each frame (visible flicker).
    // B.3.5 bridge — a CLASS STATIC, so there is no receiver at all.
    Graphics::ParticleInstance::IncrementInstanceIndex();   // B.3.5 bridge
    void* scene = Loop(self)->_scene;
    if (scene) {
        reinterpret_cast<Graphics::Scene*>(scene)->Clean();   // B.3.5 bridge
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
    // B.3.5: SetMesh and Scene::AddGlobalObject are members now; B3R: the
    // Renderer / Scene / Time ctors are called by name.
    if (!System::MemoryAllocator::_singleton) {
        printf("[MenuDLT::LoadScene] missing required fn pointers — bailing out\n");
        return false;
    }

    // Required factory
    if (!::Object::ExternalFactory::_singleton) {
        printf("[MenuDLT::LoadScene] missing factory/filename helpers — bailing out\n");
        return false;
    }

    void* factory = ::Object::ExternalFactory::_singleton;
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guard
    // above still gates the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    // Engine::Random lives at self+0x168 (UpdateLoop base + DialogLoopT field area).
    void* sceneRng = &Loop(self)->_random;
    uint32_t variant = reinterpret_cast<System::Random*>(sceneRng)->Rand(10);   // B.3.4b bridge

    // Diagnostic lever: BANISHED_MENU_SCENE pins the menu scene index (0..9) so
    // the otherwise-random MenuScene/SceneN.rsc choice becomes deterministic.
    // Lets us reproduce the intermittent menu heap-overflow per-scene instead
    // of waiting for the RNG to land on the bad one. Unset = normal random pick.
    if (Opt().menuScene >= 0) {
        unsigned forced = static_cast<unsigned>(Opt().menuScene) % 10u;
        printf("[MenuDLT::LoadScene] BANISHED_MENU_SCENE pinned scene %u "
               "(rng would have picked %u)\n", forced, variant);
        variant = forced;
    }

    // Format the scene path (Game/MenuScene/Scene{0..9}.rsc).
    System::String stringBuf =
        System::String::Format("Game/MenuScene/Scene%i.rsc", variant);

    // Filename(const String&) is what retail builds here: Format gave us a String.
    System::Filename fnBuf(stringBuf);

    // THE MENU'S RESOURCES CARRY THE MENU'S BIT. Retail loads the scene inside
    // MenuDialogLoopT::Load on the loader thread, where Loader::Run has just
    // published THIS loop's _loadBit as the factory's _cleanBitIndex, so every
    // External the scene pulls in (the scene, its meshes, particles, geometry
    // materials) is marked with the menu's usage bit. The standalone runs this
    // load from the splash (ChunkedSceneLoader, main thread) while the index
    // still holds the Loading splash's bit — so the scene belonged to the splash,
    // and the splash's retirement (~UpdateLoop -> ExternalFactory::Clean) evicted
    // it under the live menu: MenuScene freed, its WorldGeometry freed (D9 #12),
    // the menu's Graphics::Scene still holding it. Publish the menu's bit for
    // the load, as Loader::Run would, and hand the index back afterwards (the
    // loader thread is idle here: the splash waits for EngineLoader::IsBusy()).
    struct PublishLoadBit {
        ::Object::ExternalFactory* f;
        int32_t saved;
        PublishLoadBit(::Object::ExternalFactory* factory, int32_t bit)
            : f(factory), saved(factory->_cleanBitIndex) { f->_cleanBitIndex = bit; }
        ~PublishLoadBit() { f->_cleanBitIndex = saved; }
    } publish(efactory, static_cast<MenuDialogLoopT*>(self)->_loadBit);   // bridge: self is the menu loop
    void* obj = const_cast<::Object::External*>(efactory->FindOrLoad(fnBuf));  // B.3.3 bridge

    // Original validates via IsDerivedFrom against TypeInfo_MenuScene at exe 0x1401c59f0.
    // In standalone the Runtime factory tree doesn't have every base type registered,
    // so IsDerivedFrom NULL-derefs on the lookup. Trust FindOrLoad's return — if it
    // gave us a non-NULL External and the path resolved, it's the right type.
    void* menuScene = obj;

    Loop(self)->_menuScene = menuScene;

    if (!menuScene) {
        printf("[MenuDLT::LoadScene] failed to load Game/MenuScene/Scene%u.rsc\n", variant);
        return false;
    }
    printf("[MenuDLT::LoadScene] loaded MenuScene variant %u at %p\n", variant, menuScene);

    // --- Allocate Renderer (0x50 bytes, alignment 4) ---
    void* renderer = System::MemoryAllocator::MemAlloc(0x50, 4);
    if (renderer) RtGraphics_Renderer_Ctor(reinterpret_cast<RtGraphics::Renderer*>(renderer));
    Loop(self)->_renderer = renderer;

    // --- Allocate Graphics::Scene (0x78 bytes, alignment 8) ---
    void* gScene = System::MemoryAllocator::MemAlloc(0x78, 8);
    if (gScene) RtGraphics_Scene_Ctor(reinterpret_cast<RtGraphics::Scene*>(gScene));
    Loop(self)->_scene = gScene;

    // --- Allocate SunLight (0x108 bytes, alignment 8) ---
    // Retail: the imported ??0SunLight, then the exe's own copy of the table
    // (0x14015e290, slot 0 = the exe's scalar deleting dtor 0x140019e40) --
    // i.e. `new Graphics::SunLight()` compiled in the exe. ENDGAME B.3.9 pilot:
    // the real class's constructor does both (SunLight.h, B38 D1 / B39 D1); the
    // ??0SunLight door and its engine_funcs global are gone.
    void* sunLight = System::MemoryAllocator::MemAlloc(0x108, 8);
    if (sunLight) ::new (sunLight) Graphics::SunLight();
    Loop(self)->_sunLight = sunLight;

    // --- Allocate Transform (0x10C bytes, alignment 4) ---
    void* transform = System::MemoryAllocator::MemAlloc(0x10C, 4);
    if (transform) MenuScene_Helper_8800(transform);
    Loop(self)->_transform = transform;

    // --- Allocate MeshInstance (0x120 bytes, alignment 8) ---
    void* mainMesh = System::MemoryAllocator::MemAlloc(0x120, 8);
    if (mainMesh) {
        // Retail: `??0MeshInstance` then the exe's copy 0x14015e240 of the class's
        // OWN table (Ghidra: slot 0 the exe scalar deleting dtor 0x140019e80, 1-8
        // thunks onto MeshInstance / Geometry members) = `new MeshInstance()`.
        ::new (mainMesh) Graphics::MeshInstance();   // B.3.9 EXA: the real class (exe table 0x14015e240 is its own)
    }
    Loop(self)->_mainMesh = mainMesh;

    // --- SetMesh from MenuScene+0x70 (the GraphicsMesh ExternalPtr) ---
    void* meshExt = Field<void*>(menuScene, 0x70);
    if (mainMesh && meshExt) {
        reinterpret_cast<Graphics::MeshInstance*>(mainMesh)->SetMesh(
            Graphics::ExternalPtrHandle{meshExt});   // B.3.5 bridge
    }

    // --- Time(0.0f) construction + Update vtable[+0x08] dispatch on the MeshInstance ---
    if (mainMesh) {
        uint8_t timeBuf[8] = {};
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 0.0f);
        // B.3.5 bridge — slot 1 of Graphics::Geometry is `Update(const Time&)`.
        // (Original code uses the overridden vtable but the base Runtime vtable
        // still has a valid Update slot here; the virtual call reads whichever
        // vptr the object carries, exactly as the hand-inlined slot did.)
        reinterpret_cast<Graphics::Geometry*>(mainMesh)->Update(
            *reinterpret_cast<const System::Time*>(timeBuf));   // B.3.5 bridge
    }

    // --- Capture scene Geometry pointer (MenuScene+0x28 = Geometry*) ---
    Loop(self)->_sceneGeometry = Field<void*>(menuScene, 0x28);

    // --- Set the +0x28 type/category field on each Geometry. Original:
    //   mainMesh->[+0x28] = 0   (default category)
    //   sunLight->[+0x28] = 0
    //   sceneGeom->[+0x28] = 1  (terrain category)
    if (mainMesh)  Field<uint32_t>(mainMesh, 0x28) = 0;
    if (sunLight)  Field<uint32_t>(sunLight, 0x28) = 0;
    void* sceneGeom = Loop(self)->_sceneGeometry;
    if (sceneGeom) Field<uint32_t>(sceneGeom, 0x28) = 1;

    // --- Add to global scene list (terrain Geometry, SunLight, MeshInstance) ---
    if (gScene) {
        Graphics::Scene* gs = reinterpret_cast<Graphics::Scene*>(gScene);   // B.3.5 bridge
        if (sceneGeom) gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(sceneGeom));  // B.3.5 bridge
        if (sunLight)  gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(sunLight));   // B.3.5 bridge
        if (mainMesh)  gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(mainMesh));   // B.3.5 bridge
    }

    // --- Copy transform from MenuScene+0x88 into our Transform at self+0xbb8 ---
    if (transform) {
        const void* srcTransform = reinterpret_cast<const uint8_t*>(menuScene) + 0x88;
        MenuScene_CopyTransform(transform, srcTransform);
        // Cache transform[+0x28] / [+0x2C] (yaw/pitch scrolls) at MenuDLT+0xb7c/+0xb80.
        Loop(self)->_scrollX = Field<float>(transform, 0x28);
        Loop(self)->_scrollY = Field<float>(transform, 0x2C);
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
    void* menuScene = Loop(self)->_menuScene;
    void* gScene    = Loop(self)->_scene;
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
    void* menuScene = Loop(self)->_menuScene;
    void* gScene    = Loop(self)->_scene;
    MenuDialogLoopT_LoadStaticMeshes_PhaseB(self, menuScene, gScene);
    MenuDialogLoopT_LoadSceneA3(self);
    return true;
}

// ============================================================
// Phase A: Draw — render menu scene with sun lighting + fog
// ============================================================
void MenuDialogLoopT_DrawScene_PhaseA(void* self, float alpha, void* context) {
    void* menuScene = Loop(self)->_menuScene;
    if (!menuScene || !context) return;

    // B.3.5: SunLight::Set, Context::SetFog and — since batch K1 —
    // `Graphics::Renderer::Draw` are all members. The readiness guard that tested
    // nothing but the Draw door pointer went with it.
    void* transform = Loop(self)->_transform;
    void* renderer  = Loop(self)->_renderer;
    void* sunLight  = Loop(self)->_sunLight;
    void* gScene    = Loop(self)->_scene;
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

    // ?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z  retail 0x1800220b0
    Math::Vector3 dirVec = Math::Vector3::FromYawPitch(sunYaw, sunPitch);

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
    reinterpret_cast<Graphics::SunLight*>(sunLight)->Set(
        dirVec,
        *reinterpret_cast<const Math::Vector4*>(sunColorVec),
        *reinterpret_cast<const Math::Vector4*>(ambientVec));   // B.3.5 bridge

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
    reinterpret_cast<Video::Context*>(context)->SetFog(
        fogStart, fogEnd, *reinterpret_cast<const Math::Vector4*>(fogColor));   // B.3.5 bridge

    // Mirror the small write the original does into Renderer+0x3C from
    // VideoConfig+0x30. This is a per-frame copy of some video setting.
    if (::Video::Configuration::_singleton) {
        Field<uint32_t>(renderer, 0x3C) =
            Field<uint32_t>(::Video::Configuration::_singleton, 0x30);
    }

    // Camera lives inside the Transform at +0x34.
    void* camera = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(transform) + 0x34);

    reinterpret_cast<Graphics::Renderer*>(renderer)->Draw(
        *reinterpret_cast<Video::Context*>(context), alpha,
        *reinterpret_cast<const Graphics::Scene*>(gScene),
        *reinterpret_cast<const Graphics::Camera*>(camera));   // B.3.5 bridge
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
    void* menuScene = Loop(self)->_menuScene;
    if (!menuScene) return;

    void* transform = Loop(self)->_transform;
    if (!transform) return;

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
    float animYaw = Loop(self)->_animYaw + deltaSeconds * kYawSpinRate;
    Loop(self)->_animYaw = animYaw;

    float cosAnim = std::cos(animYaw * kDegToRad);
    float cachedYaw = Loop(self)->_scrollX;

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

    // ?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z  retail 0x1800220b0
    Math::Vector3 fwdVec = Math::Vector3::FromYawPitch(yawScalar, pitchScalar);

    Math::Vector3 eyeVec(targetPtr[0] - fwdVec.x * distance,
                         targetPtr[1] - fwdVec.y * distance,
                         targetPtr[2] - fwdVec.z * distance);

    // ?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z  retail 0x18007dbb0.
    // Writes transform[+0x00..+0x1B] (pos+quat); the lookAtTarget at +0x1C and
    // yaw/pitch/distance at +0x28+ are preserved.
    *reinterpret_cast<Math::Transform*>(transform) = Math::Transform::FromLookAt(
        eyeVec, *reinterpret_cast<const Math::Vector3*>(targetPtr));

    // 3. Convert to Transform3 (48B = 3x4 matrix) and apply to Camera at +0x34.
    Math::Transform3 t3Buf =
        reinterpret_cast<const Math::Transform*>(transform)->AsTransform3();
    // B.3.5 bridge -- ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z.
    // The receiver is proved by this file's own writer: MenuScene setup at :169-170
    // runs `??0Camera@Graphics@@` on `transformObj + 0x34`, and `transform` here is
    // the same MenuDLT+0xbb8 field. The whole readiness `if` and its
    // "missing fn ptrs (SetCTW=%p)" printf went with the door pointer -- the `if`
    // tested nothing else, and `!menuScene` / `!transform` above are object tests
    // and stay.
    reinterpret_cast<Graphics::Camera*>(
        reinterpret_cast<uintptr_t>(transform) + 0x34)->SetCameraToWorld(t3Buf);   // B.3.5 bridge

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
               eyeVec.x, eyeVec.y, eyeVec.z,
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
    if (!time) return;
    if (!::Engine::MainLoop::_singleton) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            printf("[MenuDLT::TickAudio] WARN: MainLoop singleton not populated\n");
        }
        return;
    }
    void* mainLoop = ::Engine::MainLoop::_singleton;
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

    reinterpret_cast<Audio::Device*>(audioDevice)->Update(
        *reinterpret_cast<const System::Time*>(time));   // B.3.7 bridge
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
    if (!::Object::ExternalFactory::_singleton) {
        printf("[MenuDLT::LoadMusic] missing factory/filename helpers — skipping\n");
        return false;
    }

    void* factory = ::Object::ExternalFactory::_singleton;
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    System::Filename fnBuf("Audio/Music/Menu.rsc");  // System::Filename
    void* music = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(factory)
            ->FindOrLoad(fnBuf));                                            // B.3.3 bridge

    Loop(self)->_music = music;
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
    void* audioScene = Loop(self)->_audioScene;   // the embedded Audio::Scene (+0xb30)
    Loop(self)->_ui._audioScene = audioScene;   // +0xd0 = UI (+0x40) + 0x90

    // 2. Play the looping menu music.
    void* music = Loop(self)->_music;
    if (!music) {
        printf("[MenuDLT::StartMusic] no music External at +0xb88 — silent menu\n");
        return;
    }
    int64_t timeZero = 0;  // System::Time(0) — 8-byte zero ticks, passed by const ref.
    uint32_t handle = reinterpret_cast<Audio::Scene*>(audioScene)->PlayEffect(
        reinterpret_cast<Audio::SoundEffect*>(music), static_cast<Audio::Scene::SoundType>(2),
        *reinterpret_cast<const System::Time*>(&timeZero));   // B.3.7 bridge
    printf("[MenuDLT::StartMusic] PlayEffect(scene=%p music=%p chan=2) -> handle=%u\n",
           audioScene, music, handle);
    fflush(stdout);
}

// ============================================================
// MenuDialogLoopT_StopMusic — the audio tail of MenuDLT::Shutdown
// ============================================================
// Mirror of the audio statement at the tail of MenuDLT::Shutdown (exe
// 0x14009b280):
//   Engine::DialogLoopT::Shutdown(self);
//   Audio::Scene::StopAll(self+0xb30);          // 0x14009b413/1a  <- this
//   UI::UserInterface::EnableCursor(self+0x40, false);
//
// LOAD-BEARING, not cosmetic. The menu music is a STREAMED sound: its
// Audio::Stream thread (Runtime 0x1800a0f60) keeps a packed IO::FileInfo open
// and calls FileSystem::ReadFile (0x1800ace00) once per buffer slot, and
// ReadFile indexes `_packfiles[handle>>24]` with NO lock (0x1800ace31/39). The
// next thing the main thread does after this Shutdown is LoaderLoop::Startup ->
// Engine::Loader::Load (0x1800dc050) -> Modification::SetupFileSystem
// (0x1800aebb0), which on a load-a-save is dirty (SaveLoad::UpdateLoad
// 0x1800c2690 marks the mod list dirty unconditionally) and so runs
// ClosePackages (frees `_packfiles` and nulls it) + LoadPackages. Retail takes
// no lock anywhere on that path either. What keeps the stream thread out of
// ReadFile during the remount is THIS call: StopAll -> Device::StopPlatform
// (0x1800dea20) raises the stream's stop flag and signals its wake event, so
// the thread abandons its pass and closes the file before the remount starts.
// Without it the stream kept reading through the remount and faulted on the
// freed/null array (STATUS open defect (c), ~1 in 60 resave runs).
void MenuDialogLoopT_StopMusic(void* self) {
    if (!self) return;
    void* audioScene = Loop(self)->_audioScene;   // the embedded Audio::Scene (+0xb30)
    reinterpret_cast<Audio::Scene*>(audioScene)->StopAll();   // B.3.7 bridge
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
    if (g_gameContainer == nullptr) {
        printf("[MenuDLT::BindUISounds] KeyMap container not populated — UI silent\n");
        return;
    }
    void* keyMap = g_gameContainer;
    void* ui = &Loop(self)->_ui;

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

    void* audioScene = Loop(self)->_audioScene;   // the embedded Audio::Scene (+0xb30)
    void* rng = &Loop(self)->_random;

    System::Time elapsed;  elapsed._ticks  = *reinterpret_cast<int64_t*>(time);  // *(u64*)time — System::Time ticks
    System::Time timeZero; timeZero._ticks = 0;                                  // Time(0), passed by value
    float listenerVec3[3] = { 0.0f, 0.0f, 0.0f };          // origin listener (2D menu music)

    reinterpret_cast<Audio::Scene*>(audioScene)->Update(
        elapsed, timeZero, *reinterpret_cast<System::Random*>(rng),
        *reinterpret_cast<const Vector3*>(listenerVec3), 0.0f);   // B.3.7 bridge

    static int n = 0;
    if (n < 3) {
        ++n;
        printf("[MenuDLT::TickAudioScene] Audio::Scene::Update(scene=%p) tick=%d\n",
               audioScene, n);
    }
}
