// code/Game/Player/tool_base_ray.cpp
// Player tool terrain-raycast / mouse-ray / screen->cell / placement-position chain.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from the
// per-tool Player .cpp files (placementtool.cpp etc.). They resolve to 0 (silently
// skipped) unless registered. This file provides faithful 1:1 reimplementations and a
// single registration point (RegisterPlayerRayBatch in standalone/reg_player_ray.cpp) so
// the shared call sites fire real logic in the standalone exe.
//
// Wave 5 (Player) round 2 — ray/screen-to-cell sub-batch. Each function is a 1:1 port
// of the decompiled exe helper; do not "improve" behavior.

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Camera.h"
#include <new>
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ============================================================
// Locally-resolved dispatch-slot helpers (Runtime exports behind exe .rdata slots)
// ============================================================
// 1005a0 (GetMouseRay) calls four Math/Volume member functions through exe dispatch
// slots that are NOT registered project-wide. They are pure Runtime exports, so we
// resolve them locally by their mangled names (read from the exe's slot name table)
// and register them as dispatch slots in reg_player_ray.cpp. The bodies here resolve
// them via ResolveExport so they work whether or not the slot is filled.
//
// All four are member functions returning Math::Vector3 by value (12 bytes > 8 -> the
// MS x64 ABI uses a hidden return buffer: RCX=this, RDX=&retbuf, R8/XMM2.. = args).
//   ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z   Vector3::Vector3(const Scalar&)   slot 0x140151320
//   ??DTransform3@Math@@QEBA?AVVector3@1@AEBV21@@Z  Transform3::operator*(Vector3)  slot 0x140152908
//   ?GetScreenDirection@Frustum@Volume@@QEBA?AVVector3@Math@@MM@Z  GetScreenDirection(float,float) slot 0x140152918
//   ?Rotate@Transform3@Math@@QEBA?AVVector3@2@AEBV32@@Z  Transform3::Rotate(Vector3)  slot 0x140151860
typedef float* (*fn_FrustumScreenDir_t)(const void* frustum, float* retbuf, float u, float v);

static fn_FrustumScreenDir_t s_fn_FrustumScreenDir = nullptr;
static bool s_rayExportsResolved = false;

static void EnsureRayExports() {
    if (s_rayExportsResolved) return;
    s_fn_FrustumScreenDir = reinterpret_cast<fn_FrustumScreenDir_t>(
        ResolveExport("?GetScreenDirection@Frustum@Volume@@QEBA?AVVector3@Math@@MM@Z"));
    s_rayExportsResolved = true;
}

static inline uintptr_t GameWorld() {
    return reinterpret_cast<uintptr_t>(g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
}

// CharacterComponent_impl_f3e0 (0x14009f3e0, registered) = grid->world conversion.
typedef float* (*fn_GridToWorld_t)(int* grid, float* out, float* in, char flag, void* extra);
static fn_GridToWorld_t s_GridToWorld = nullptr;
static bool s_g2wResolved = false;
static fn_GridToWorld_t s_fn_GridToWorld() {
    if (!s_g2wResolved) {
        s_GridToWorld = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
        s_g2wResolved = true;
    }
    return s_GridToWorld;
}

// ============================================================
// FUN_1400aa530 — QuarryComponent_impl_a530 (terrain DDA grid ray-march)
// ============================================================
// Marches a ray (origin param_2.xy, direction param_3.xy) across the terrain cell
// grid, testing each visited cell via QuarryComponent_helper_aa20 (a quad-vs-ray
// intersection against the terrain height field). Returns true (in the low byte) on
// the first cell whose helper reports a hit; on a hit, helper_aa20 fills param_5/param_6
// (and the position outputs). Returns false when the march leaves the grid or exceeds
// the max parametric distance (*param_4, normally 400.0).
//
// 1:1 port of the decompiled DDA. The original aliases stack slots through
// (slot - param_2) base-relative arithmetic; this port reasons about the logical
// "lane" (j=0 for X, j=1 for Y) each access targets and uses plain indexed locals.
//
// Stack lanes (RBP-relative in the original, here as named pairs):
//   normDir[2]  = Vector2::Normalize(param_3.xy)   (the ray direction; divisor)
//   center      = (width>>1) as float
//   tNear[2]    = parametric distance to the near grid boundary per axis
//   tFar[2]     = parametric distance to the far grid boundary per axis
//   cell[2]     = current integer cell (col,row), seeded at the origin cell
//   step[2]     = DDA step direction per axis (+1 / -1 / 0)
//   tMax[2]     = parametric distance to the next cell crossing per axis
//   tDelta[2]   = parametric distance between cell crossings per axis
// Caller signature: bool fn(longlong gameWorldMap, float* origin, Vector3* dir,
//                           float* maxDist, float* outA, float* outB)
//   (param_1 = *(g_pGameWorldSingleton+0x110) in the 101270 caller; +0xc0 = grid width.)
extern "C" uint8_t PlayerRay_TerrainMarch_aa530(uintptr_t param_1, float* param_2,
                                                float* param_3, float* param_4,
                                                float* param_5, float* param_6) {
    const uint32_t signMask = 0x80000000u;  // DAT_1401722c0
    const float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;  // DAT_14017214c (=1.0)

    int width = *reinterpret_cast<int*>(param_1 + 0xc0);

    // normDir = normalize(param_3.x, param_3.y) — the 2D ray direction (DDA divisor).
    // 🔑 Math::Vector2 is 8 bytes (2 floats); MSVC x64 returns it in RAX, NOT via a
    // hidden return buffer (only structs >8 bytes use one). The dispatch-slot call
    // (retbuf,self) therefore mis-passed the args and normalized (0,0)->(0,0), leaving a
    // ZERO direction — the DDA then can't step (step=0), tests only the seed cell, and
    // never hits the terrain (the "no ghost / CreateBar stuck top-left" root cause).
    // Compute the unit direction inline: behaviour-identical, no ABI dependency.
    float srcDir[2] = { param_3[0], param_3[1] };
    float normDir[2] = { srcDir[0], srcDir[1] };
    {
        float len = std::sqrt(srcDir[0] * srcDir[0] + srcDir[1] * srcDir[1]);
        if (len > 1e-6f) { normDir[0] = srcDir[0] / len; normDir[1] = srcDir[1] / len; }
    }

    float center = static_cast<float>(width >> 1);
    float centerPos[2] = { center, center };      // local_res8[0],[1]
    float centerNeg[2];                           // local_res8[2],[3] = center ^ 0x80000000 (-center)
    {
        uint32_t bits = reinterpret_cast<uint32_t&>(center) ^ signMask;
        centerNeg[0] = reinterpret_cast<float&>(bits);
        centerNeg[1] = centerNeg[0];
    }

    // First slab pass: parametric distances to the +/- grid boundaries per axis.
    float tNear[2] = {0.0f, 0.0f};   // local_68[j]
    float tFarA[2] = {0.0f, 0.0f};   // local_res8[4+j]
    for (int j = 0; j < 2; j++) {
        float d = normDir[j];
        float origin = param_2[j];
        if (d > 0.0f) {
            tNear[j] = (centerPos[j] - origin) * (uvScale / d);
            tFarA[j] = (centerNeg[j] - origin) * (uvScale / d);
        } else if (d < 0.0f) {
            tNear[j] = (centerNeg[j] - origin) * (uvScale / d);
            tFarA[j] = (centerPos[j] - origin) * (uvScale / d);
        } else {
            tFarA[j] = 0.0f;
            tNear[j] = 0.0f;
        }
    }

    // Entry/exit slab test: tEnter = max(tFarA[0], tFarA[1]); tExit = min(tNear[0], tNear[1]).
    float tEnter = (tFarA[0] > tFarA[1]) ? tFarA[0] : tFarA[1];
    float tExit  = (tNear[1] > tNear[0]) ? tNear[0] : tNear[1];
    if (tEnter > tExit)
        return 0;

    // Seed the DDA at the origin cell.
    int cell[2];
    cell[0] = static_cast<int>(center + param_2[0]);
    cell[1] = static_cast<int>(center + param_2[1]);

    int   step[2];     // auStack_60[j]
    float tMax[2];     // local_res8[2+j]
    float tDelta[2];   // local_58[2+j]
    for (int j = 0; j < 2; j++) {
        float tFar = tFarA[j];
        float dlt = (tNear[j] - tFar) / static_cast<float>(width);
        tDelta[j] = dlt;
        float dir = normDir[j];
        if (dir > 0.0f) {
            step[j] = 1;
            tMax[j] = static_cast<float>(cell[j] + 1) * dlt + tFar;
        } else if (dir < 0.0f) {
            step[j] = -1;
            tMax[j] = static_cast<float>(width - cell[j]) * dlt + tFar;
        } else {
            // 0x49742400 = large "infinity" sentinel so this axis never crosses.
            uint32_t inf = 0x49742400u;
            tMax[j] = reinterpret_cast<float&>(inf);
            step[j] = 0;
        }
    }

    // Main DDA march.
    for (;;) {
        int axis = (tMax[1] <= tMax[0]) ? 1 : 0;   // SETBE: S[3] <= S[2]
        if (cell[0] >= 0 && cell[0] < width && cell[1] >= 0 && cell[1] < width) {
            bool hit = false;
            uintptr_t fn = ResolveAddr(Module::Exe, 0x1400aaa20);  // QuarryComponent_helper_aa20 (registered)
            if (fn) {
                typedef bool (*fn_aa20_t)(uintptr_t, int, int, float*, float*, float*,
                                         float*, float*, void*);
                hit = reinterpret_cast<fn_aa20_t>(fn)(
                    param_1, cell[0], cell[1], param_3, param_2, param_4,
                    param_5, param_6, nullptr);
            }
            if (hit) return 1;
        }
        // Advance along the chosen axis.
        float oldTMax = tMax[axis];
        cell[axis] += step[axis];
        tMax[axis] = oldTMax + tDelta[axis];
        if (oldTMax >= *param_4) {  // exceeded max parametric distance
            return 0;
        }
    }
}

// ============================================================
// FUN_1401005a0 — GetMouseRay (camera mouse ray: origin + direction)
// ============================================================
// Reads the mouse position from the Input::KeyMap, validates it against the display
// size, then builds the camera ray:
//   origin    (param_2) = cameraTransform * Vector3(0)        (camera world position)
//   direction (param_3) = cameraTransform.Rotate( Frustum.GetScreenDirection(u, v) )
// where u = mouseX / screenWidth, v = mouseY / screenHeight.
// Returns true when the mouse is inside the display, false otherwise.
//   KeyMap   = ***g_pKeyMapObjectExref (triple-deref)
//   display  = *g_pVideoDeviceSingleton ; +4 = width, +8 = height
//   camera   = *(g_pGameWorldSingleton + 0xe0) ; +0x34 = Graphics::Camera,
//              +0x98..+0xc4 = 48-byte Transform3 (camera world matrix)
// Caller signature: bool fn(void* /*param_1, ignored*/, float* outOrigin, float* outDir)
extern "C" uint8_t PlayerRay_GetMouseRay_1005a0(void* /*param_1*/, float* outOrigin, float* outDir) {
    EnsureRayExports();

    if (!g_pKeyMapObjectExref || !g_pVideoDeviceSingleton) return 0;

    // KeyMap = triple-deref of the exref (matches the working dialog pattern
    // ***(void****)g_pKeyMapObjectExref: slot -> slot value -> exref -> object).
    void* keyMap = nullptr;
    {
        void**** p = reinterpret_cast<void****>(g_pKeyMapObjectExref);
        if (p && *p && **p && ***p) keyMap = ***p;
    }
    void* display = *g_pVideoDeviceSingleton;        // Video::Device instance
    uintptr_t camHolder = reinterpret_cast<uintptr_t>(
        g_pGameWorldSingleton ? *reinterpret_cast<void**>(GameWorld() + 0xe0) : nullptr);
    if (!keyMap || !display || !camHolder) return 0;

    short mouseX = fn_KeyMap_GetValue ? fn_KeyMap_GetValue(keyMap, 0, 0) : 0;
    short mouseY = fn_KeyMap_GetValue ? fn_KeyMap_GetValue(keyMap, 0, 1) : 0;

    int screenW = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(display) + 4);
    int screenH = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(display) + 8);

    // Safety: garbage/zero screen dims would let the bounds check pass with junk and then
    // crash in the camera-ray math (writing a by-value result through a bad retbuf). Reject
    // implausible display sizes so sub_da00 returns invalid instead of hard-faulting.
    if (screenW <= 0 || screenH <= 0 || screenW > 16384 || screenH > 16384)
        return 0;

    if (mouseX < 0 || mouseX >= screenW || mouseY < 0 || mouseY >= screenH)
        return 0;

    // Copy the 48-byte camera Transform3 (camHolder+0x98 .. +0xc4) onto the stack.
    float xform[12];
    for (int i = 0; i < 12; i++)
        xform[i] = *reinterpret_cast<float*>(camHolder + 0x98 + i * 4);

    // Camera = camHolder + 0x34. GetFrustum returns a Frustum (112 bytes) by value.
    void* camera = reinterpret_cast<void*>(camHolder + 0x34);
    // Camera::GetFrustum is a hidden-retslot door in retail; the door body moves
    // to the call site as exactly ONE construction into the same caller storage.
    alignas(16) uint8_t frustum[112];
    ::new (static_cast<void*>(frustum)) ::Frustum(
        reinterpret_cast<const Graphics::Camera*>(camera)->GetFrustum());   // B.3.5 bridge

    // origin = xform * Vector3(0,0,0).  (Vector3(Scalar=0) broadcasts 0 to all lanes.)
    Math::Vector3 zeroVec((Math::Scalar(0.0f)));
    {
        Math::Vector3 r =
            reinterpret_cast<const Math::Transform3*>(xform)->operator*(zeroVec);
        outOrigin[0] = r.x; outOrigin[1] = r.y; outOrigin[2] = r.z;
    }

    // screenDir = Frustum.GetScreenDirection(mouseX/width, mouseY/height).
    float u = static_cast<float>(mouseX) / static_cast<float>(screenW);
    float v = static_cast<float>(mouseY) / static_cast<float>(screenH);
    float screenDir[3] = {0.0f, 0.0f, 0.0f};
    if (s_fn_FrustumScreenDir) {
        float* r = s_fn_FrustumScreenDir(frustum, screenDir, u, v);
        screenDir[0] = r[0]; screenDir[1] = r[1]; screenDir[2] = r[2];
    }

    // direction = xform.Rotate(screenDir).
    {
        Math::Vector3 r = reinterpret_cast<const Math::Transform3*>(xform)->Rotate(
            *reinterpret_cast<const Math::Vector3*>(screenDir));
        outDir[0] = r.x; outDir[1] = r.y; outDir[2] = r.z;
    }
    return 1;
}

// ============================================================
// FUN_140101270 — ScreenToCell (mouse ray -> terrain cell index)
// ============================================================
// Builds the mouse ray (GetMouseRay), marches it across the terrain (aa530), and
// converts the hit world position to a clamped cell index. Returns -1 when the mouse
// is off-screen or the ray hits nothing.
//   maxDist constant = DAT_14017223c = 400.0f.
//   grid = *(g_pGameWorldSingleton + 0xf8); grid[0] = width.
//   map  = *(g_pGameWorldSingleton + 0x110) (passed as aa530's param_1).
// Caller signature: int fn(void* self)  (param_1 forwarded to GetMouseRay; ignored there.)
extern "C" int PlayerRay_ScreenToCell_101270(uintptr_t param_1) {
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float dir[3]    = {0.0f, 0.0f, 0.0f};

    uint8_t ok = PlayerRay_GetMouseRay_1005a0(reinterpret_cast<void*>(param_1), origin, dir);
    if (ok == 0)
        return -1;

    uint32_t maxBits = 0x43c80000u;     // DAT_14017223c = 400.0f
    float maxDist = reinterpret_cast<float&>(maxBits);

    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));
    uintptr_t map = *reinterpret_cast<uintptr_t*>(GameWorld() + 0x110);

    // local_18[4] in the original — aa530's helper aa20 writes 3 floats (world pos)
    // through this buffer (param_5 -> aa20 param_7), so it must hold at least 3.
    float hitPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // local_18 (out)
    float scratch[6] = {0,0,0,0,0,0};             // local_res10 (out, unused by caller)

    uint8_t hit = PlayerRay_TerrainMarch_aa530(
        map, origin, dir, &maxDist, hitPos, scratch);


    if (hit == 0)
        return -1;

    int width = grid[0];
    int col = static_cast<int>(hitPos[0] + static_cast<float>(width >> 1));
    int row = static_cast<int>(hitPos[1] + static_cast<float>(width >> 1));
    if (col < 0) col = 0;
    if (col >= width) col = width - 1;
    if (row < 0) row = 0;
    if (row >= width) row = width - 1;
    return width * row + col;
}

// ============================================================
// FUN_1400fda00 — PlacementTool_sub_da00 (placement footprint position calc)
// ============================================================
// Picks the terrain cell under the mouse (ScreenToCell), centres the building
// footprint (width/height from the tool's description) over it, validates the
// footprint is fully on the map, computes the world centre position (averaging the
// two diagonal corners via CharacterComponent_impl_f3e0 grid->world), and a yaw
// factor. Returns true when the placement is valid.
//   self+0x28 -> description IBP; desc+0x44 = footprint width, desc+0x48 = height.
//   self+0x48 -> rotation IBP; *(rot+0x10) = orientation (1 or 3 swaps W/H).
//   grid = *(g_pGameWorldSingleton + 0xf8); grid[0] = width.
//   DAT_140172204 = 90.0f (yaw scalar), DAT_140172114 = 0.5f (cell centre offset).
// Caller signature: bool fn(longlong self, int* outStartCol, int* outStartRow,
//                           int* outEndCol, int* outEndRow, float* outPos, float* outYaw)
extern "C" uint8_t PlayerRay_PlacementPosCalc_fda00(uintptr_t param_1, int* param_2,
                                                    int* param_3, int* param_4,
                                                    int* param_5, float* param_6,
                                                    float* param_7) {
    const float yawScalar  = 90.0f;  // DAT_140172204 = 0x42b40000
    const float halfCell   = 0.5f;   // DAT_140172114 = 0x3f000000

    uintptr_t descIbpSlot = param_1 + 0x48;   // rotation/footprint IBP (self+0x48)
    uintptr_t descPtr     = *reinterpret_cast<uintptr_t*>(param_1 + 0x28);

    uint32_t footW = *reinterpret_cast<uint32_t*>(descPtr + 0x44);
    uint32_t footH = *reinterpret_cast<uint32_t*>(descPtr + 0x48);

    // Orientation: rot+0x10 == 1 or 3 swaps width/height.
    uint32_t dimX = footW, dimY = footH;
    uintptr_t rot = *reinterpret_cast<uintptr_t*>(descIbpSlot);
    int orient = *reinterpret_cast<int*>(rot + 0x10);
    if (orient == 1 || orient == 3) {
        dimX = footH;
        dimY = footW;
    }

    int* grid = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(GameWorld() + 0xf8));
    int cellIdx = PlayerRay_ScreenToCell_101270(descIbpSlot);
    if (cellIdx < 0)
        return 0;

    int width = grid[0];
    int row = cellIdx / width;
    int col = cellIdx % width;

    // Centre the footprint over (col,row).
    if ((dimX & 1) == 0) {
        *param_2 = (col - (static_cast<int>(dimX) >> 1)) + 1;
        *param_4 = (static_cast<int>(dimX) >> 1) + col;
    } else {
        int h = static_cast<int>(dimX - 1) >> 1;
        *param_2 = col - h;
        *param_4 = h + col;
    }
    int dimYHalf;
    if ((dimY & 1) == 0) {
        dimYHalf = static_cast<int>(dimY) >> 1;
        *param_3 = (row - dimYHalf) + 1;
    } else {
        dimYHalf = static_cast<int>(dimY - 1) >> 1;
        *param_3 = row - dimYHalf;
    }
    *param_5 = dimYHalf + row;

    // Validate the footprint is fully on the map (and start col > 0).
    int startCol = *param_2;
    int w = grid[0];
    uint8_t valid;
    if (startCol < 0 || w <= startCol || *param_3 < 0 || w <= *param_3 ||
        startCol < 1 || w <= *param_4 || *param_5 < 0 || w <= *param_5) {
        valid = 0;
    } else {
        valid = 1;
    }

    // outYaw = orientation * 90.0f.
    uintptr_t rot2 = *reinterpret_cast<uintptr_t*>(descIbpSlot);
    *param_7 = static_cast<float>(*reinterpret_cast<int*>(rot2 + 0x10)) * yawScalar;

    if (valid != 0) {
        // Average the two diagonal corner world positions for the footprint centre.
        int gw = grid[0];
        int idxA = *param_3 * gw + *param_2;
        float centerOff = static_cast<float>(gw >> 1) - halfCell;
        float inGridA[3];
        inGridA[0] = static_cast<float>(idxA % gw) - centerOff;
        inGridA[1] = static_cast<float>(idxA / gw) - centerOff;
        inGridA[2] = 0.0f;
        float worldA[3] = {0.0f, 0.0f, 0.0f};
        if (s_fn_GridToWorld()) s_fn_GridToWorld()(grid, worldA, inGridA, '\0', nullptr);

        gw = grid[0];
        int idxB = *param_5 * gw + *param_4;
        centerOff = static_cast<float>(gw >> 1) - halfCell;
        float inGridB[3];
        inGridB[0] = static_cast<float>(idxB % gw) - centerOff;
        inGridB[1] = static_cast<float>(idxB / gw) - centerOff;
        inGridB[2] = 0.0f;
        float worldB[3] = {0.0f, 0.0f, 0.0f};
        if (s_fn_GridToWorld()) s_fn_GridToWorld()(grid, worldB, inGridB, '\0', nullptr);

        param_6[0] = (worldB[0] + worldA[0]) * halfCell;
        param_6[1] = (worldB[1] + worldA[1]) * halfCell;
        param_6[2] = 0.0f;
    }
    return valid;
}
