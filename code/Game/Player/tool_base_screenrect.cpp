// code/Game/Player/tool_base_screenrect.cpp
// Player registration wave, round 2d — batch "screenrect" (edittool.cpp deep tier).
//
// Lands 0x140100380 (Entity_impl_0380) — computes an entity's screen-space bounding
// rectangle by transforming its world bounding-box corners and projecting them through
// the camera. Resolved via ResolveAddr(Module::Exe, 0x140100380) from
// code/Game/Player/edittool.cpp (EditTool_Startup positions the tool dialog near the
// selected entity's on-screen rectangle). It resolves to 0 (silently skipped) unless
// registered.
//
// This file provides faithful 1:1 reimplementations of the four-function closure and a
// single registration point (RegisterPlayerScreenRectBatch in
// standalone/reg_player_screenrect.cpp).
//
// Closure (registered bottom-up):
//   0x1400fec70 CharacterComponent_impl_ec70  -> CharacterComponent_impl_f3e0 (0x14009f3e0, REG)
//   0x140098210 cameraWorldToScreen           -> Math dispatch slots (registered locally)
//   0x1400fe960 boundsAccumulate              -> 0x140098210 + Vector2 Min/Max/*= slots
//   0x140100380 Entity_impl_0380              -> 0x1400fec70, 0x1400fe960,
//                                                Entity_ClearPathQueue (0x140003f60, REG),
//                                                FUN_140004680 (0x140004680, REG),
//                                                + Volume/Vector3/Transform/Box slots
//
// The Math operations are invoked through exe IAT dispatch slots (0x14015xxxx). Each was
// decoded from the exe's name table (slot RVA + 2-byte hint -> mangled string) and is a
// pure Runtime export. The bodies resolve them via ResolveExport so they work whether or
// not the slot is filled; reg_player_screenrect.cpp ALSO registers the slots (a local
// slot-storage registrar) for any other standalone caller that reads the .rdata slot.
//
// All value-returning Math members use the MS x64 member struct-return ABI:
//   RCX=this, RDX=&retbuf, R8/R9.. = args. The retbuf pointer is returned in RAX.
// Verified instruction-by-instruction from disassemble_function (this path is DORMANT on
// the autostart A/B test, so it is ported from disasm, not the muddled decompile).

#include "engine_funcs.h"
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "math/Math.h"
#include "volume/Volume.h"
#include "Game/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Component/exe_bodies.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Device::_singleton
#include "Player/exe_bodies.h"
// One-shot screen-rect projection diagnostics (BANISHED_TOOL_DIAG). A/B-safe: only
// emits when the placement pick chain is active + the env is set. Bounded line count.

// ============================================================
// Locally-resolved Math/Volume dispatch-slot exports (value-return, member ABI).
// ============================================================
// Each is a Runtime export decoded from the exe dispatch-slot name table:
//   0x140151f00 ??0Vector4@Math@@QEAA@AEBVVector3@1@AEBVScalar@1@@Z   Vector4(Vector3&,Scalar&)
//   0x140151868 ??0Transform3@Math@@QEAA@AEBV01@@Z                    Transform3(const Transform3&)
//   0x1401522d0 ?Inverse@Transform3@Math@@QEBA?AV12@XZ                Transform3::Inverse()
//   0x1401520a0 ?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ         Transform3::AsMatrix()
//   0x1401522c8 ??DMatrix@Math@@QEBA?AV01@AEBV01@@Z                   Matrix::operator*(Matrix)
//   0x1401522c0 ??DMatrix@Math@@QEBA?AVVector4@1@AEBV21@@Z            Matrix::operator*(Vector4)
//   0x1401528a8 ??0Rectangle@Volume@@QEAA@XZ                          Rectangle::Rectangle()
//   0x140152a78 ??0Vector3@Math@@QEAA@XZ                              Vector3::Vector3()
//   0x140152950 ?GetCorners@Box@Volume@@QEBAXPEAVVector3@Math@@@Z     Box::GetCorners(Vector3*)
//   0x140152900 ?Min@Vector3@Math@@QEBA?AV12@AEBV12@@Z                Vector3::Min(Vector3)
//   0x140151470 ??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z         Transform::operator*(Vector3)
//   0x140152780 ?Max@Vector2@Math@@QEBA?AV12@AEBV12@@Z                Vector2::Max(Vector2)
//   0x140152788 ?Min@Vector2@Math@@QEBA?AV12@AEBV12@@Z                Vector2::Min(Vector2)
//   0x140151e70 ??XVector2@Math@@QEAAXAEBV01@@Z                       Vector2::operator*=(Vector2)

// Rectangle::Rectangle() and Box::GetCorners are called by name:
// RtVolume_Rectangle_Ctor / RtVolume_Box_GetCorners (runtime/types/volume/Volume.h).

static inline uintptr_t GameWorld() {
    return reinterpret_cast<uintptr_t>(g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
}

// Display object = Video::Device; +4=width, +8=height.
// The original exe reads it via **exref 0x140151a80 (slot -> cell -> Device); the
// cell is the class static Video::Device::_singleton (ENDGAME B.3.9 SS2).
static inline void* Display() {
    return ::Video::Device::_singleton;
}

// CharacterComponent_impl_f3e0 (0x14009f3e0) = grid->world conversion:
// Reimpl_MapGetTerrainPos (Game/exe_bodies.h), called by name.

// ============================================================
// FUN_1400fec70 — CharacterComponent_impl_ec70 (build 4 footprint corners)
// ============================================================
// Given a tile rectangle (startCol=p2, startRow=p3, endCol=p4, endRow=p5) writes 4
// world-space corner Vector3s into param_6[0..11] (12 floats). Each corner is the
// grid->world position of a (col,row) cell: the input grid coord's z is 0, but the
// f3e0 grid->world conversion fills a real z (terrain height) which is kept verbatim.
//   grid = *(g_pGameWorldSingleton + 0xf8); grid[0] = width.
//   DAT_140172114 = 0.5f (cell-centre offset).
// Caller signature matches the disasm: __fastcall
//   (int startRow_packed_as_p1, int startCol p2, int startRow p3, int endCol p4,
//    int endRow p5, float* outCorners p6)  -> returns 4 (corner count).
// NOTE: the 0380 caller passes the first integer arg in RCX as the "endRow" combined
// value, but ec70 only ever reads param_2..param_6; param_1 is unused. We keep the
// 1:1 prototype (param_1 ignored, exactly like the original which never reads it).
extern "C" uint32_t PlayerScreenRect_BuildCorners_ec70(uint64_t /*param_1*/, int param_2,
                                                       int param_3, int param_4,
                                                       int param_5, float* param_6) {
    const float halfCell = 0.5f;  // DAT_140172114
    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));

    auto cornerToWorld = [&](int col_for_idx, int row_for_idx, float* out) {
        int w = grid[0];
        int idx = w * row_for_idx + col_for_idx;
        float centerOff = static_cast<float>(w >> 1) - halfCell;
        float in[3];
        in[0] = static_cast<float>(idx % w) - centerOff;
        in[1] = static_cast<float>(idx / w) - centerOff;
        in[2] = 0.0f;
        float world[3] = {0.0f, 0.0f, 0.0f};
        Reimpl_MapGetTerrainPos(grid, world, in, '\0', nullptr);
        out[0] = world[0];
        out[1] = world[1];
        out[2] = world[2];   // f3e0 fills a real z (terrain height); kept verbatim
    };

    // Corner 0: (col=p2, row=p3)
    cornerToWorld(param_2, param_3, &param_6[0]);
    // Corner 1: (col=p2, row=p5)
    cornerToWorld(param_2, param_5, &param_6[3]);
    // Corner 2: (col=p4, row=p3)
    cornerToWorld(param_4, param_3, &param_6[6]);
    // Corner 3: (col=p4, row=p5)
    cornerToWorld(param_4, param_5, &param_6[9]);
    return 4;
}

// ============================================================
// FUN_140098210 — camera world->screen projection
// ============================================================
// Projects a single world-space point (param_2) through the camera (param_1 = camera
// holder at *(g_pGameWorldSingleton + 0xe0)) into screen-space pixel coordinates,
// written to param_3[0]=x, param_3[1]=y. Returns true when the point is in front of the
// camera (w > 0).
//   v4      = Vector4(param_2, g_fUVScale)                 (homogeneous, w = 1.0)
//   projMat = 64-byte Matrix copied from (param_1 + 0xc8)  (camera projection matrix)
//   xform   = camera Transform3 copied from (param_1 + 0x98)  (48 bytes)
//   mat     = xform.Inverse().AsMatrix() * projMat         (view-inverse * projection)
//   clip    = mat * v4                                     (Vector4)
//   if clip.w (local_fc) > 0:
//       ndc = (clip.x / w, clip.y / w)
//       screen.x = ndc.x * (W*0.5) + (W*0.5)
//       screen.y = H - (ndc.y * (H*0.5) + (H*0.5))
//   display = **g_pSingletonExref_151a80 ; +4=W, +8=H ; DAT_140172114 = 0.5f.
extern "C" uint8_t PlayerScreenRect_WorldToScreen_098210(uintptr_t param_1, float* param_2,
                                                         float* param_3) {
    const float halfScale = 0.5f;  // DAT_140172114

    float uvScale = kRdata_14017214c;  // DAT_14017214c = 1.0f (the Scalar)

    // local_f8 = Vector4(param_2, uvScale)  ([RSP+0x70] in the original)
    Math::Vector4 v4(*reinterpret_cast<const Math::Vector3*>(param_2), uvScale);

    // Copy the 64-byte projection Matrix from (param_1 + 0xc8) ([RSP+0x20] in the original).
    alignas(16) uint8_t projMat[64];
    for (int i = 0; i < 16; i++)
        reinterpret_cast<uint32_t*>(projMat)[i] =
            *reinterpret_cast<uint32_t*>(param_1 + 0xc8 + i * 4);

    // local_e8 = Transform3(*(param_1 + 0x98))  (copy ctor of the camera transform)
    Math::Transform3 xform = *reinterpret_cast<const Math::Transform3*>(param_1 + 0x98);

    // inv = xform.Inverse() ; asMat = inv.AsMatrix() ; mat = asMat * projMat
    Math::Transform3 inv    = xform.Inverse();
    Math::Matrix     asMat  = inv.AsMatrix();
    Math::Matrix     mat    = asMat * *reinterpret_cast<const Math::Matrix*>(projMat);

    // clip = mat * v4
    Math::Vector4 clipVec = mat * v4;
    const float* clip = &clipVec.x;

    float w = clip[3];   // local_fc
    if (w <= 0.0f)
        return 0;

    void* disp = Display();
    if (!disp) return 0;
    int screenW = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(disp) + 4);
    int screenH = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(disp) + 8);

    float ndcX = clip[0] / w;
    float ndcY = clip[1] / w;
    float halfW = static_cast<float>(screenW) * halfScale;
    float halfH = static_cast<float>(screenH) * halfScale;
    param_3[0] = ndcX * halfW + halfW;
    param_3[1] = static_cast<float>(screenH) - (ndcY * halfH + halfH);
    return 1;
}

// ============================================================
// FUN_1400fe960 — accumulate projected corners into a screen rectangle
// ============================================================
// Projects each of param_2 (count) world corners through WorldToScreen_098210, tracks
// the per-axis min/max in screen space (seeded with +/-1e6), clamps the result to the
// display [0..W]x[0..H], scales by (gameSettingsValue / 100.0), and writes the resulting
// rectangle (min.xy, max.xy) to param_4[0..1]. Returns true when at least one corner
// projected in front of the camera and the box overlaps the display.
//   running min seed = ( 1000000.0,  1000000.0)   (0x49742400)
//   running max seed = (-1000000.0, -1000000.0)   (0xc9742400)
//   display = **g_pSingletonExref_151a80 ; +4=W(R14), +8=H(R15).
//   g_pGameSettingsPtr deref +0 = int UI scale ; DAT_140172208 = 100.0f.
// Caller signature: __fastcall (uint64 p1(ignored), uint count p2, Vector3* corners p3,
//                               void* outRect p4) -> bool. The corners stride is 0xc.
extern "C" uint8_t PlayerScreenRect_AccumulateRect_fe960(uint64_t /*param_1*/, uint32_t param_2,
                                                        float* param_3, void* param_4) {
    const float bigPos =  1000000.0f;  // 0x49742400
    const float bigNeg = -1000000.0f;  // 0xc9742400
    const float scaleDiv = 100.0f;     // DAT_140172208

    void* disp = Display();
    if (!disp) { return 0; }
    int W = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(disp) + 4);  // R14
    int H = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(disp) + 8);  // R15

    float runMin[2] = { bigPos, bigPos };   // [RBP+0x30]
    float runMax[2] = { bigNeg, bigNeg };   // [RBP-0x50]

    if (static_cast<int>(param_2) <= 0)
        { return 0; }

    uintptr_t camHolder = *reinterpret_cast<uintptr_t*>(GameWorld() + 0xe0);

    bool any = false;
    float* corner = param_3;
    for (uint32_t i = 0; i < param_2; i++) {
        float scr[2] = {0.0f, 0.0f};   // [RBP-0x48]
        uint8_t ok = PlayerScreenRect_WorldToScreen_098210(camHolder, corner, scr);
        if (ok != 0) {
            // runMin = Vector2::Min(runMin, scr)
            {
                Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMin)->Min(
                    *reinterpret_cast<const Math::Vector2*>(scr));
                runMin[0] = r.x; runMin[1] = r.y;
            }
            // runMax = Vector2::Max(runMax, scr)
            {
                Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMax)->Max(
                    *reinterpret_cast<const Math::Vector2*>(scr));
                runMax[0] = r.x; runMax[1] = r.y;
            }
            any = true;
        }
        corner += 3;  // stride 0xc bytes
    }

    if (!any)
        { return 0; }

    // Reject if the box is entirely off the display.
    //   if (runMin.x > W) fail ; if (0 > runMax.x) fail ;
    //   if (runMin.y > H) fail ; if (0 > runMax.y) fail.
    float fW = static_cast<float>(W);
    float fH = static_cast<float>(H);
    if (runMin[0] > fW) { return 0; }
    if (0.0f > runMax[0]) { return 0; }
    if (runMin[1] > fH) { return 0; }
    if (0.0f > runMax[1]) { return 0; }

    // Clamp runMin to [0, (W,H)] : runMin = Min( Max(runMin, 0), (W,H) ).
    {
        Math::Vector2 zero(0.0f, 0.0f);
        Math::Vector2 wh(fW, fH);
        {   Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMin)->Max(zero);
            runMin[0]=r.x; runMin[1]=r.y; }
        {   Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMin)->Min(wh);
            runMin[0]=r.x; runMin[1]=r.y; }
    }
    // Clamp runMax similarly.
    {
        Math::Vector2 zero(0.0f, 0.0f);
        Math::Vector2 wh(fW, fH);
        {   Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMax)->Max(zero);
            runMax[0]=r.x; runMax[1]=r.y; }
        {   Math::Vector2 r = reinterpret_cast<const Math::Vector2*>(runMax)->Min(wh);
            runMax[0]=r.x; runMax[1]=r.y; }
    }

    // Scale by (gameSettings UI value / 100.0) on both axes (in place operator*=).
    float uiScale = 1.0f;
    if (g_pGameSettingsPtr && *g_pGameSettingsPtr)
        uiScale = static_cast<float>((*g_pGameSettingsPtr)[0]) / scaleDiv;
    {
        Math::Vector2 sv(uiScale, uiScale);
        reinterpret_cast<Math::Vector2*>(runMin)->operator*=(sv);
    }
    {
        Math::Vector2 sv(uiScale, uiScale);
        reinterpret_cast<Math::Vector2*>(runMax)->operator*=(sv);
    }

    // Write rect: param_4[0..1] = runMin.xy ; param_4[2..3] = runMax.xy (two 8-byte stores).
    float* out = reinterpret_cast<float*>(param_4);
    out[0] = runMin[0]; out[1] = runMin[1];
    out[2] = runMax[0]; out[3] = runMax[1];
    return 1;
}

// ============================================================
// FUN_140100380 — Entity_impl_0380 (entity screen bounding-rectangle)
// ============================================================
// Writes an entity's screen-space bounding rectangle (Volume::Rectangle, 16 bytes) to
// param_3. If the entity has a valid placed description (offset 0x166 >= 0) it uses the
// entity's stored Transform (offsets 0x148..0x160) and Box (offsets 0x5c..0x70):
// transforms the box's 8 corners and projects them. Otherwise it falls back to the 4
// footprint corners built from the entity's tile rect (FUN_1400fec70).
//
//   param_2 = InternalBasePtr to the entity
//   param_3 = out Rectangle (16 bytes)
//   self    = param_1 (held in [RSP+8]; passed to ClearPathQueue/FUN_140004680 via *param_2)
//
//   Transform (Math::Transform, 7 floats at +0x148..+0x160) -> local "transform" (28 B)
//   Box (Volume::Box, 6 floats at +0x5c..+0x70)            -> local "box" (24 B)
//   const clamp vec (R8 in the Min loop) = (1e6, 1e6, 2.5)  (0x49742400,0x49742400,0x40200000)
//
// Return value: the original 0380 falls straight into the fe960 tail call and returns
// whatever fe960 left in AL (its "rect is valid" bool) — EditTool::Startup reads it as
// `char cVar5` and deselects when 0. We return that bool explicitly so the caller's
// gotRect is deterministic instead of relying on leftover-EAX codegen.
extern "C" uint8_t PlayerScreenRect_Entity_0380(uintptr_t /*param_1*/, void* param_2,
                                                uint64_t* param_3) {

    // local_a8 = Rectangle()  (default ctor); copy its 16 bytes to param_3 as the seed.
    alignas(16) uint8_t rect[16];
    void* rectPtr = RtVolume_Rectangle_Ctor(reinterpret_cast<struct Rectangle*>(rect));
    {
        uint64_t* r = reinterpret_cast<uint64_t*>(rectPtr ? rectPtr : rect);
        param_3[0] = r[0];
        param_3[1] = r[1];
    }

    // local_88 = Vector3[8] (96 bytes), each default-constructed.
    alignas(16) float corners[24];  // 8 * 3
    for (int i = 0; i < 8; i++)
        *reinterpret_cast<Math::Vector3*>(&corners[i * 3]) = Math::Vector3();

    // CheckPtr(param_2) ; ClearPathQueue(*param_2, &entityIBP) -> local_res8 (entity ptr)
    uintptr_t entity = 0;   // local_res8
    {
        // Entity_ClearPathQueue (0x140003f60) : member-return ABI RCX=this, RDX=&retIBP.
        ActCNR_FindComponent_3f60(*reinterpret_cast<uintptr_t*>(param_2),
                                  reinterpret_cast<void*>(&entity));
    }

    uint32_t cornerCount = 0;  // R15D — count fed to fe960

    bool useBox;
    if (entity == 0) {
        useBox = false;
    } else {
        // *(short*)(entity + 0x166) < 0  -> no placed description -> fallback
        useBox = (*reinterpret_cast<int16_t*>(entity + 0x166) >= 0);
    }

    if (useBox) {
        // CheckPtr(&entity) ; copy Transform (7 floats @ +0x148..+0x160).
        float transform[7];  // local_c4..local_ac (Math::Transform)
        for (int i = 0; i < 7; i++)
            transform[i] = *reinterpret_cast<float*>(entity + 0x148 + i * 4);

        // CheckPtr(&entity) ; copy Box (6 floats @ +0x5c..+0x70).
        float box[6];   // local_dc..local_c8 (Volume::Box: min.xyz, max.xyz)
        for (int i = 0; i < 6; i++)
            box[i] = *reinterpret_cast<float*>(entity + 0x5c + i * 4);

        // box.GetCorners(corners)  (8 corners)
        RtVolume_Box_GetCorners(reinterpret_cast<const struct Box*>(box),
                                reinterpret_cast<Math::Vector3*>(corners));

        // For each of 8 corners: corner = transform * corner.Min(clampVec).
        // clampVec (R8) = (1e6, 1e6, 2.5).
        float clampVec[3];
        { uint32_t b0 = 0x49742400u, b1 = 0x49742400u, b2 = 0x40200000u;
          clampVec[0] = reinterpret_cast<float&>(b0);
          clampVec[1] = reinterpret_cast<float&>(b1);
          clampVec[2] = reinterpret_cast<float&>(b2); }
        for (int i = 0; i < 8; i++) {
            float* c = &corners[i * 3];
            // tmp = c.Min(clampVec) -> write back to c (retbuf = local_98 scratch)
            {
                Math::Vector3 r = reinterpret_cast<const Math::Vector3*>(c)->Min(
                    *reinterpret_cast<const Math::Vector3*>(clampVec));
                c[0] = r.x; c[1] = r.y; c[2] = r.z;
            }
            // c = transform * c  (retbuf = local_a8 rect scratch in the original)
            {
                Math::Vector3 r = reinterpret_cast<const Math::Transform*>(transform)->operator*(
                    *reinterpret_cast<const Math::Vector3*>(c));
                c[0] = r.x; c[1] = r.y; c[2] = r.z;
            }
        }
        cornerCount = 8;
    } else {
        // Fallback: CheckPtr(param_2) ; self2 = FUN_140004680(*param_2, &local_res10)
        uint8_t local_res10[8] = {0};
        void* self2 = nullptr;   // returned IBP
        {
            self2 = FloorComponent_FindResData(*reinterpret_cast<void**>(param_2),
                                               reinterpret_cast<void*>(local_res10));
        }
        uintptr_t obj = self2 ? *reinterpret_cast<uintptr_t*>(self2) : 0;
        if (obj) {
            int16_t f28 = *reinterpret_cast<int16_t*>(obj + 0x28);  // startCol
            int16_t f2a = *reinterpret_cast<int16_t*>(obj + 0x2a);  // startRow
            int16_t f2c = *reinterpret_cast<int16_t*>(obj + 0x2c);  // width
            int16_t f2e = *reinterpret_cast<int16_t*>(obj + 0x2e);  // height
            // p4 = (f2c-1) + f28 ; p5 = (f2e-1) + f2a  (end col/row)
            int endCol = (static_cast<int>(f2c) - 1) + static_cast<int>(f28);
            int endRow = (static_cast<int>(f2e) - 1) + static_cast<int>(f2a);
            // ec70(endRow_combined, startCol=f28, startRow=f2a, endCol, endRow, corners)
            cornerCount = PlayerScreenRect_BuildCorners_ec70(
                static_cast<uint64_t>(static_cast<uint32_t>(endRow)),
                static_cast<int>(f28), static_cast<int>(f2a),
                endCol, endRow, corners);
        }
    }

    // FUN_1400fe960(?, cornerCount, corners, param_3)  -> fills the screen rectangle.
    // Its bool return (at least one corner in front of the camera + on-screen) is 0380's
    // return value (see header note); the caller uses it to decide whether to reposition.
    return PlayerScreenRect_AccumulateRect_fe960(0, cornerCount, corners, param_3);
}
