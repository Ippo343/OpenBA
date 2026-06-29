// code/Game/Game/camera_input.cpp
// In-game camera controller — keyboard/mouse driven view movement.
//
// Clean reimplementation of two helpers that live inside EntityUI::Update
// (the in-game player/tool UI controller, GameUpdateLoop+0x120):
//   FUN_1400feec0  EntityUI::UpdateCamera   — WASD pan, Q/E rotate, wheel zoom, pitch
//   FUN_1400ff6e0  EntityUI::UpdateHotkeys   — TogglePause + camera presets (0..4)
//
// In the DLL build the original exe owns EntityUI::Update (0x1401026a0) and our
// hooks chain into it, so the camera already responds to input. In the standalone
// build EntityUI::Update is not registered, so the camera never moved. This file is
// invoked directly from GameUpdateLoop::Update (Step 10, standalone only) so the
// KeyMap drives the camera exactly like the original.
//
// ── Camera state lives on the PlayerController at GameWorld+0xE0 ──
//   +0x1c,+0x20,+0x24  focus/target position (x,y,z) — what panning moves
//   +0x28              yaw    (degrees)               — Q/E rotate
//   +0x2c              pitch  (degrees, auto/smoothed)
//   +0x30              distance (smoothed)            — scroll-wheel zoom
//   +0x98..+0xa0       camera basis (right vector)    — NOT populated in standalone,
//                                                        so we derive it from yaw.
// ── Controller (EntityUI, GameUpdateLoop+0x120) smoothing state ──
//   +0x30 player(spacebar) pause flag   +0x38 wheel-allow   +0x3c zoom target
//   +0x40 pitch target   +0x44 manual-pitch 0..1   +0x48 timer
//   +0x70 camera-preset table (5 × 0x1c)   +0x80 tool-active
//   +0xa4 follow flag   +0xa8/+0xac follow target cells
//
// KeyMap input IDs (see inputbindings.cpp): 0/1 mouseXY, 2 mouseDeltaX, 4 wheel,
//   7 mid-mouse, 9/10 rotate L/R, 0xb/0xc forward/back, 0xd/0xe left/right,
//   0xf/0x10 zoom in/out, 0x11/0x12 pitch up/down, 0x17 toggle-pause, 0x2d..0x31
//   camera presets 0..4.
//
// All magic constants are taken verbatim from the exe .rdata (addresses noted).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Game {

namespace {

// ── .rdata constants (Application-x64.exe), decoded from raw bytes ──
constexpr float kDeg2Rad      = 0.01745329f; // 0x1401720cc
constexpr float kRotSpeed     = 90.0f;       // 0x140172204  deg/sec keyboard rotate
constexpr float kMidDragScale = 0.2f;        // 0x1401720fc  mouse-middle drag rotate
constexpr float kPitchStep    = 0.125f;      // 0x1401720ec  manual pitch increment
constexpr float kZoomStep     = 5.0f;        // 0x140172198  distance per zoom action
constexpr float kZoomMin      = 20.0f;       // 0x1401721c4
constexpr float kZoomMax      = 130.0f;      // 0x140172218
constexpr float kZoomSpan     = 110.0f;      // 0x14017220c  (130-20)
constexpr float kPanSpeedFar  = 70.0f;       // 0x1401721f8  pan speed when zoomed out
constexpr float kPanSpeedNear = 12.0f;       // 0x1401721b8  pan speed when zoomed in
constexpr float kPitchFarMan  = -65.0f;      // 0x140172298
constexpr float kPitchFarFlat = 85.0f;       // 0x140172200
constexpr float kPitchNearMan = -25.0f;      // 0x140172290
constexpr float kPitchNearFlt = 60.0f;       // 0x1401721f0
constexpr float kSmoothNew    = 0.1f;        // 0x1401720e8  exp-smoothing alpha
constexpr float kSmoothOld    = 0.9f;        // 0x140172134
constexpr float kClampBase    = 100.0f;      // 0x140172208
constexpr float kClampSpan    = 150.0f;      // 0x14017221c
constexpr float kHalf         = 0.5f;        // 0x140172114
constexpr float kFollowSnap   = 0.1f;        // 0x1401720e4
constexpr float kFollowLerp   = 0.2f;        // 0x1401720f8  (1 - kSmoothOld_follow)
constexpr float kFollowKeep   = 0.8f;        // 0x140172128

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// KeyMap accessors (the triple-deref exref pattern, same as inputbindings.cpp).
inline void* CamKeyMap() {
    if (!g_pKeyMapObjectExref || !*g_pKeyMapObjectExref) return nullptr;
    void* l2 = *(void**)*g_pKeyMapObjectExref;
    if (!l2) return nullptr;
    return *(void**)l2;
}

// f3e0 = CharacterComponent_impl_f3e0 (0x14009f3e0) grid->world (registered).
typedef float* (*fn_GridToWorld_t)(int* grid, float* out, float* in, char flag, void* extra);
fn_GridToWorld_t GridToWorld() {
    static fn_GridToWorld_t fn = nullptr;
    static bool tried = false;
    if (!tried) { tried = true; fn = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0)); }
    return fn;
}

// Video::Device (screen dims) via the 0x140151a80 exref: **slot, then +4=W, +8=H.
inline bool ScreenSize(int& w, int& h) {
    w = h = 0;
    if (!g_pSingletonExref_151a80) return false;
    void* p = *g_pSingletonExref_151a80;
    if (!p) return false;
    void* dev = *(void**)p;
    if (!dev) return false;
    w = *(int*)((uintptr_t)dev + 4);
    h = *(int*)((uintptr_t)dev + 8);
    return true;
}

// Game-settings byte at a given offset (DAT_1401c3de0 -> settings base).
inline char SettingByte(int byteOffset) {
    if (!g_pGameSettingsPtr || !*g_pGameSettingsPtr) return 0;
    return *((char*)(*g_pGameSettingsPtr) + byteOffset);
}

} // namespace

// ============================================================
// UpdateCamera — port of FUN_1400feec0
// ============================================================
//   entityUI  = GameUpdateLoop::m_entityUI (+0x120 controller)
//   inputFlag = "no UI input focus" (1 = gameplay has input)
//   timeTicks = frame delta as System::Time ticks (int64)
void Camera_UpdateMovement(void* entityUI, int inputFlag, int64_t timeTicks) {
    if (!entityUI) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* keyMap = CamKeyMap();
    if (!keyMap || !fn_KeyMap_GetValue || !fn_KeyMap_Pressed || !fn_Time_AsSeconds) return;

    uint8_t* ui = (uint8_t*)entityUI;
    uint8_t* pc = (uint8_t*)*(void**)((uintptr_t)*g_pGameWorldSingleton + 0xe0); // PlayerController
    if (!pc) return;

    int64_t timeObj = timeTicks;
    const float dt = fn_Time_AsSeconds(&timeObj);

    // ── yaw: keyboard rotate (ids 9/10) + middle-mouse drag (id 7 + mouseDeltaX id 2) ──
    float yaw = *(float*)(pc + 0x28);
    if (fn_KeyMap_GetValue(keyMap, 0, 9))  yaw -= dt * kRotSpeed;   // RotateLeft
    if (fn_KeyMap_GetValue(keyMap, 0, 10)) yaw += dt * kRotSpeed;   // RotateRight
    if (fn_KeyMap_GetValue(keyMap, 0, 7)) {                          // MouseMiddle drag
        short ddx = fn_KeyMap_GetValue(keyMap, 0, 2);
        yaw -= (float)ddx * kMidDragScale;
    }

    // ── pan input axes: edge-scroll (optional) then keyboard overrides ──
    float panX = 0.0f; // left/right axis
    float panY = 0.0f; // forward/back axis
    if (SettingByte(0x14)) { // edge-scroll enabled in game settings
        int sw, sh; ScreenSize(sw, sh);
        unsigned mx = (unsigned)(uint16_t)fn_KeyMap_GetValue(keyMap, 0, 0);
        unsigned my = (unsigned)(uint16_t)fn_KeyMap_GetValue(keyMap, 0, 1);
        if (mx < 4)                               panX = -1.0f;
        if ((int)mx > sw - 5 && (int)mx < sw)     panX =  1.0f;
        if (my < 4)                               panY = -1.0f;
        if ((int)my > sh - 5 && (int)my < sh)     panY =  1.0f;
    }
    if (fn_KeyMap_GetValue(keyMap, 0, 0xd)) panX = -1.0f; // Left
    if (fn_KeyMap_GetValue(keyMap, 0, 0xe)) panX =  1.0f; // Right
    if (fn_KeyMap_GetValue(keyMap, 0, 0xb)) panY = -1.0f; // Forward
    if (fn_KeyMap_GetValue(keyMap, 0, 0xc)) panY =  1.0f; // Back

    // Diagnostic self-test (env BANISHED_CAMERA_SELFTEST): inject synthetic input so
    // we can confirm the input->camera path end-to-end without a keyboard. Forces a
    // forward pan + slow rotate and logs the PlayerController state periodically.
    static int s_selftest = -1;
    if (s_selftest < 0) { const char* e = std::getenv("BANISHED_CAMERA_SELFTEST"); s_selftest = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (s_selftest) { panY = -1.0f; *(float*)(pc + 0x28) += dt * 30.0f; yaw = *(float*)(pc + 0x28); }

    // ── zoom target init + wheel/key input ──
    if (*(float*)(ui + 0x3c) == 0.0f)
        *(float*)(ui + 0x3c) = *(float*)(pc + 0x30); // seed from current distance

    short wheel = fn_KeyMap_GetValue(keyMap, 0, 4);
    if (inputFlag == 0 && *(char*)(ui + 0x38) == 0) wheel = 0; // no wheel zoom if UI focused
    int zoomDir;
    if (fn_KeyMap_Pressed(keyMap, 0, 0xf)) {          // ZoomIn
        zoomDir = 1;
    } else if (fn_KeyMap_Pressed(keyMap, 0, 0x10)) {  // ZoomOut
        zoomDir = -1;
    } else {
        zoomDir = wheel;
    }

    // ── manual pitch accumulator (0..1) from PitchUp/PitchDown (ids 0x11/0x12) ──
    if (fn_KeyMap_Pressed(keyMap, 0, 0x11))
        *(float*)(ui + 0x44) += kPitchStep;
    else if (fn_KeyMap_Pressed(keyMap, 0, 0x12))
        *(float*)(ui + 0x44) -= kPitchStep;
    *(float*)(ui + 0x44) = clampf(*(float*)(ui + 0x44), 0.0f, 1.0f);

    // ── apply zoom step to the zoom target, clamp 20..130 ──
    if (zoomDir != 0) {
        float z = *(float*)(ui + 0x3c) + (zoomDir < 1 ? kZoomStep : -kZoomStep);
        *(float*)(ui + 0x3c) = clampf(z, kZoomMin, kZoomMax);
    }

    // ── pitch/distance coupling ──
    const float manual  = *(float*)(ui + 0x44);                       // 0..1
    const float normZ   = (*(float*)(ui + 0x3c) - kZoomMin) / kZoomSpan; // 0..1
    const float pitchTarget =
        (manual * kPitchFarMan  - (1.0f - manual) * kPitchFarFlat) * normZ +
        (manual * kPitchNearMan - (1.0f - manual) * kPitchNearFlt) * (1.0f - normZ);
    *(float*)(ui + 0x40) = pitchTarget;

    // pan speed scales with zoom (near=12, far=70)
    const float panSpeed = normZ * kPanSpeedFar + (1.0f - normZ) * kPanSpeedNear;

    // commit yaw; smooth pitch and distance toward their targets
    *(float*)(pc + 0x28) = yaw;
    *(float*)(pc + 0x2c) = pitchTarget * kSmoothNew + *(float*)(pc + 0x2c) * kSmoothOld;
    *(float*)(pc + 0x30) = *(float*)(pc + 0x30) * kSmoothOld + *(float*)(ui + 0x3c) * kSmoothNew;

    // ── pan: move focus along the camera's ground-plane basis ──
    // Original reads the camera right-vector from PlayerController+0x98; that field is
    // not populated in standalone, so derive it from yaw (matching Vector3::FromYawPitch:
    // view dir = (-cos·sin yaw, cos·cos yaw, sin), giving right = (cos yaw, sin yaw, 0)).
    float nlen = std::sqrt(panX * panX + panY * panY);
    if (nlen > 0.0f) {
        const float nx = panX / nlen;
        const float ny = panY / nlen;
        const float yawR = yaw * kDeg2Rad;
        const float cy = std::cos(yawR), sy = std::sin(yawR);
        const float rightX =  cy, rightY =  sy;   // camera right basis (PlayerController+0x98)
        const float fwdX   =  sy, fwdY   = -cy;   // cross(right, up=(0,0,1))
        const float aR = panSpeed * nx; // along right
        const float aF = panSpeed * ny; // along forward
        *(float*)(pc + 0x1c) += (rightX * aR + fwdX * aF) * dt;
        *(float*)(pc + 0x20) += (rightY * aR + fwdY * aF) * dt;
        // z basis is 0 for both vectors, so focus.z is unaffected by panning.
    }

    // ── clamp focus to the (zoom-scaled) map bounds ──
    {
        int* grid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
        if (grid) {
            float factor = clampf(1.0f - (*(float*)(pc + 0x30) - kClampBase) / kClampSpan, 0.0f, 1.0f);
            float halfExtent = (float)grid[0] * kHalf * factor;
            *(float*)(pc + 0x1c) = clampf(*(float*)(pc + 0x1c), -halfExtent, halfExtent);
            *(float*)(pc + 0x20) = clampf(*(float*)(pc + 0x20), -halfExtent, halfExtent);
        }
    }

    // ── follow/center-on-entity smooth move (only when +0xa4 flag set) ──
    if (*(char*)(ui + 0xa4) != 0) {
        fn_GridToWorld_t g2w = GridToWorld();
        int* grid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
        if (g2w && grid) {
            const int gw = grid[0];
            const float halfCell = (float)(gw >> 1) - kHalf;
            float worldA[3] = {0,0,0}, worldB[3] = {0,0,0};
            int cellA = *(int*)(ui + 0xac);
            int cellB = *(int*)(ui + 0xa8);
            float inA[3] = { (float)(cellA % gw) - halfCell, (float)(cellA / gw) - halfCell, 0.0f };
            float inB[3] = { (float)(cellB % gw) - halfCell, (float)(cellB / gw) - halfCell, 0.0f };
            g2w(grid, worldA, inA, '\0', nullptr);
            g2w(grid, worldB, inB, '\0', nullptr);
            float midX = (worldA[0] + worldB[0]) * kHalf;
            float midY = (worldA[1] + worldB[1]) * kHalf;
            float* fx = (float*)(pc + 0x1c);
            float* fy = (float*)(pc + 0x20);
            float* fz = (float*)(pc + 0x24);
            *fx = midX * kFollowLerp + *fx * kFollowKeep;
            *fy = midY * kFollowLerp + *fy * kFollowKeep;
            *fz = 0.0f * kFollowLerp + *fz * kFollowKeep;
            float dx = *fx - midX, dy = *fy - midY, dz = *fz;
            if (std::sqrt(dx*dx + dy*dy + dz*dz) < kFollowSnap)
                *(char*)(ui + 0xa4) = 0;
        }
    }

    // ── decrement the controller's input timer (+0x48) ──
    int64_t* tmr = (int64_t*)(ui + 0x48);
    *tmr -= timeTicks;
    if (*tmr < 0) *tmr = 0;

    if (s_selftest) {
        static int n = 0;
        if ((n++ % 120) == 0) {
            fprintf(stderr, "[CamSelfTest] focus=(%.2f, %.2f, %.2f) yaw=%.1f pitch=%.1f dist=%.2f dt=%.4f\n",
                    *(float*)(pc + 0x1c), *(float*)(pc + 0x20), *(float*)(pc + 0x24),
                    *(float*)(pc + 0x28), *(float*)(pc + 0x2c), *(float*)(pc + 0x30), dt);
            fflush(stderr);
        }
    }
}

// ============================================================
// Apply one camera preset (port of FUN_1400fe8d0)
// ============================================================
static void Camera_ApplyPreset(void* entityUI, int index, int64_t /*unused*/) {
    uint8_t* ui = (uint8_t*)entityUI;
    void* table = *(void**)(ui + 0x70);
    if (!table) return; // no preset table in standalone yet
    float* p = (float*)((uintptr_t)table + (uintptr_t)index * 0x1c);
    if (*(char*)(p + 6) == 0) return; // preset slot not set

    uint8_t* pc = (uint8_t*)*(void**)((uintptr_t)*g_pGameWorldSingleton + 0xe0);
    if (!pc) return;
    *(float*)(pc + 0x30) = p[5]; // distance
    *(float*)(pc + 0x28) = p[3]; // yaw
    *(float*)(pc + 0x1c) = p[0]; // focus x
    *(float*)(pc + 0x20) = p[1]; // focus y
    *(float*)(pc + 0x24) = p[2]; // focus z
    *(float*)(pc + 0x2c) = p[4]; // pitch
    *(float*)(ui + 0x3c) = p[5]; // zoom target
    *(float*)(ui + 0x40) = p[4]; // pitch target
    // Re-run movement with dt=0 to settle smoothing onto the preset immediately.
    Camera_UpdateMovement(entityUI, 0, 0);
}

// ============================================================
// UpdateHotkeys — port of FUN_1400ff6e0 (pause toggle + camera presets)
// ============================================================
void Camera_UpdateHotkeys(void* entityUI, int /*inputFlag*/, int64_t timeTicks) {
    if (!entityUI || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* keyMap = CamKeyMap();
    if (!keyMap || !fn_KeyMap_Pressed) return;
    uint8_t* ui = (uint8_t*)entityUI;

    // TogglePause (id 0x17) — unless the pause is locked (GameWorld+0xd0 bit 10).
    if (fn_KeyMap_Pressed(keyMap, 0, 0x17)) {
        uint32_t worldFlags = *(uint32_t*)((uintptr_t)*g_pGameWorldSingleton + 0xd0);
        if (((worldFlags >> 10) & 1) == 0)
            *(char*)(ui + 0x30) = (*(char*)(ui + 0x30) == 0) ? 1 : 0;
    }

    // Camera presets 0..4 (ids 0x2d..0x31). The original gates these on the active
    // tool allowing camera control; in standalone no tool is wired, so allow them.
    for (int i = 0; i < 5; ++i) {
        if (fn_KeyMap_Pressed(keyMap, 0, i + 0x2d))
            Camera_ApplyPreset(entityUI, i, timeTicks);
    }
}

} // namespace Game
