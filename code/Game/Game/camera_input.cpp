// code/Game/Game/camera_input.cpp
// In-game camera controller — keyboard/mouse driven view movement.
//
// Clean reimplementation of two helpers that live inside EntityUI::Update
// (the in-game player/tool UI controller, GameUpdateLoop+0x120):
//   FUN_1400feec0  EntityUI::UpdateCamera   — WASD pan, Q/E rotate, wheel zoom, pitch
//   FUN_1400ff6e0  EntityUI::UpdateHotkeys   — TogglePause + camera presets (0..4)
//
// EntityUI::Update (0x1401026a0) is not registered, so the camera would never
// move on its own. This file is invoked directly from GameUpdateLoop::Update
// (Step 10) so the KeyMap drives the camera exactly like the original.
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

#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "options.h"
#include "Game/exe_bodies.h"   // Reimpl_MapGetTerrainPos
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Device::_singleton

extern "C" bool StartTown_GetSpawnTile(int* x, int* y);   // starttown.cpp (BANISHED_CAMERA_PIN=town)

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
    if (Input::Configuration::_singleton == nullptr) return nullptr;
    void* l2 = Input::Configuration::_singleton;
    if (!l2) return nullptr;
    return *(void**)l2;
}

// f3e0 = 0x14009f3e0 grid->world: Reimpl_MapGetTerrainPos (Game/exe_bodies.h).

// Video::Device (screen dims): retail reads **exref 0x140151a80; the class static
// is that cell (ENDGAME B.3.9 SS2). +4=W, +8=H.
inline bool ScreenSize(int& w, int& h) {
    w = h = 0;
    void* dev = ::Video::Device::_singleton;
    if (!dev) return false;
    w = *(int*)((uintptr_t)dev + 4);
    h = *(int*)((uintptr_t)dev + 8);
    return true;
}

// Game-settings byte at a given offset (DAT_1401c3de0 -> settings base).
inline char SettingByte(int byteOffset) {
    if (g_gameSettings == nullptr) return 0;
    return *((char*)g_gameSettings + byteOffset);
}

} // namespace

// ============================================================
// UpdateCamera — port of FUN_1400feec0
// ============================================================
//   entityUI  = GameUpdateLoop::m_toolManager (+0x120 controller)
//   inputFlag = "no UI input focus" (1 = gameplay has input)
//   timeTicks = frame delta as System::Time ticks (int64)
void Camera_UpdateMovement(void* entityUI, int inputFlag, int64_t timeTicks) {
    if (!entityUI) return;
    if (GameUpdateLoop::_singleton == nullptr) return;

    void* keyMap = CamKeyMap();
    if (!keyMap) return;

    uint8_t* ui = (uint8_t*)entityUI;
    uint8_t* pc = (uint8_t*)GameUpdateLoop::_singleton->m_playerController; // PlayerController
    if (!pc) return;

    int64_t timeObj = timeTicks;
    const float dt = reinterpret_cast<const System::Time*>(&timeObj)->AsSeconds();   // B.3.4b bridge

    // ── yaw: keyboard rotate (ids 9/10) + middle-mouse drag (id 7 + mouseDeltaX id 2) ──
    float yaw = *(float*)(pc + 0x28);
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 9))  yaw -= dt * kRotSpeed;   // RotateLeft  // B.3.7 bridge
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 10)) yaw += dt * kRotSpeed;   // RotateRight  // B.3.7 bridge
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 7)) {                          // MouseMiddle drag  // B.3.7 bridge
        short ddx = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 2);  // B.3.7 bridge
        yaw -= (float)ddx * kMidDragScale;
    }

    // ── pan input axes: edge-scroll (optional) then keyboard overrides ──
    float panX = 0.0f; // left/right axis
    float panY = 0.0f; // forward/back axis
    if (SettingByte(0x14)) { // edge-scroll enabled in game settings
        int sw, sh; ScreenSize(sw, sh);
        unsigned mx = (unsigned)(uint16_t)reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0);  // B.3.7 bridge
        unsigned my = (unsigned)(uint16_t)reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 1);  // B.3.7 bridge
        if (mx < 4)                               panX = -1.0f;
        if ((int)mx > sw - 5 && (int)mx < sw)     panX =  1.0f;
        if (my < 4)                               panY = -1.0f;
        if ((int)my > sh - 5 && (int)my < sh)     panY =  1.0f;
    }
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0xd)) panX = -1.0f; // Left  // B.3.7 bridge
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0xe)) panX =  1.0f; // Right  // B.3.7 bridge
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0xb)) panY = -1.0f; // Forward  // B.3.7 bridge
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0xc)) panY =  1.0f; // Back  // B.3.7 bridge

    // ── zoom target init + wheel/key input ──
    if (*(float*)(ui + 0x3c) == 0.0f)
        *(float*)(ui + 0x3c) = *(float*)(pc + 0x30); // seed from current distance

    short wheel = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 4);  // B.3.7 bridge
    if (inputFlag == 0 && *(char*)(ui + 0x38) == 0) wheel = 0; // no wheel zoom if UI focused
    int zoomDir;
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0xf)) {          // ZoomIn  // B.3.7 bridge
        zoomDir = 1;
    } else if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x10)) {  // ZoomOut  // B.3.7 bridge
        zoomDir = -1;
    } else {
        zoomDir = wheel;
    }

    // ── manual pitch accumulator (0..1) from PitchUp/PitchDown (ids 0x11/0x12) ──
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x11))  // B.3.7 bridge
        *(float*)(ui + 0x44) += kPitchStep;
    else if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x12))  // B.3.7 bridge
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
        int* grid = static_cast<int*>(GameUpdateLoop::_singleton->m_game);
        if (grid) {
            float factor = clampf(1.0f - (*(float*)(pc + 0x30) - kClampBase) / kClampSpan, 0.0f, 1.0f);
            float halfExtent = (float)grid[0] * kHalf * factor;
            *(float*)(pc + 0x1c) = clampf(*(float*)(pc + 0x1c), -halfExtent, halfExtent);
            *(float*)(pc + 0x20) = clampf(*(float*)(pc + 0x20), -halfExtent, halfExtent);
        }
    }

    // ── follow/center-on-entity smooth move (only when +0xa4 flag set) ──
    if (*(char*)(ui + 0xa4) != 0) {
        int* grid = static_cast<int*>(GameUpdateLoop::_singleton->m_game);
        if (grid) {
            const int gw = grid[0];
            const float halfCell = (float)(gw >> 1) - kHalf;
            float worldA[3] = {0,0,0}, worldB[3] = {0,0,0};
            int cellA = *(int*)(ui + 0xac);
            int cellB = *(int*)(ui + 0xa8);
            float inA[3] = { (float)(cellA % gw) - halfCell, (float)(cellA / gw) - halfCell, 0.0f };
            float inB[3] = { (float)(cellB % gw) - halfCell, (float)(cellB / gw) - halfCell, 0.0f };
            Reimpl_MapGetTerrainPos(grid, worldA, inA, '\0', nullptr);   // 0x14009f3e0
            Reimpl_MapGetTerrainPos(grid, worldB, inB, '\0', nullptr);
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

    // Harness (env BANISHED_CAMERA_PIN=x,y[,dist]): pin the camera focus to a world
    // position every frame, overriding pan/follow, so a verification run can look at a
    // known spot (e.g. the demo quarry) without keyboard input. World coords are
    // cell - (mapDim/2 - 0.5); dist defaults to 40.
    static int s_pin = -1;
    static float s_pinX = 0, s_pinY = 0, s_pinDist = 40.0f;
    if (s_pin < 0) {
        const char* e = Opt().cameraPin;
        s_pin = 0;
        if (e && e[0]) {
            float px, py, pd;
            int got = std::sscanf(e, "%f,%f,%f", &px, &py, &pd);
            if (got >= 2) { s_pinX = px; s_pinY = py; if (got >= 3) s_pinDist = pd; s_pin = 1; }
            // "town[,dist]": pin on the start-town spawn tile, whatever seed/map size
            // produced it (the stress harness uses this — the spawn tile differs per map).
            // Resolved lazily: the tile only exists once StartTown_SpawnEntities has run,
            // which is before the first camera tick of the world.
            else if (std::strncmp(e, "town", 4) == 0) {
                int tx = 0, ty = 0;
                if (StartTown_GetSpawnTile(&tx, &ty) && GameUpdateLoop::_singleton != nullptr) {
                    void* game = GameUpdateLoop::_singleton->m_game;
                    const int dim = game ? *(int32_t*)game : 0;
                    if (dim > 0) {
                        s_pinX = (float)tx - ((float)(dim >> 1) - 0.5f);
                        s_pinY = (float)ty - ((float)(dim >> 1) - 0.5f);
                        if (std::sscanf(e + 4, ",%f", &pd) == 1) s_pinDist = pd;
                        s_pin = 1;
                        std::fprintf(stderr, "[CameraPin] town tile=(%d,%d) dim=%d -> world (%.1f,%.1f) dist=%.0f\n",
                                     tx, ty, dim, s_pinX, s_pinY, s_pinDist);
                    }
                }
                if (!s_pin) s_pin = -1;   // not ready yet — retry next tick
            }
        }
    }
    if (s_pin) {
        *(float*)(pc + 0x1c) = s_pinX;
        *(float*)(pc + 0x20) = s_pinY;
        *(float*)(pc + 0x24) = 0.0f;
        *(float*)(pc + 0x30) = s_pinDist;
        *(float*)(ui + 0x3c) = s_pinDist;   // zoom target, so smoothing doesn't fight it
    }


    // ── decrement the controller's input timer (+0x48) ──
    int64_t* tmr = (int64_t*)(ui + 0x48);
    *tmr -= timeTicks;
    if (*tmr < 0) *tmr = 0;

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

    uint8_t* pc = (uint8_t*)GameUpdateLoop::_singleton->m_playerController;
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
    if (!entityUI || GameUpdateLoop::_singleton == nullptr) return;
    void* keyMap = CamKeyMap();
    if (!keyMap) return;
    uint8_t* ui = (uint8_t*)entityUI;

    // TogglePause (id 0x17) — unless the pause is locked (GameWorld+0xd0 bit 10).
    if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x17)) {  // B.3.7 bridge
        uint32_t worldFlags = GameUpdateLoop::_singleton->m_gameFlags;
        if (((worldFlags >> 10) & 1) == 0)
            *(char*)(ui + 0x30) = (*(char*)(ui + 0x30) == 0) ? 1 : 0;
    }

    // Camera presets 0..4 (ids 0x2d..0x31). The original gates these on the active
    // tool allowing camera control; in standalone no tool is wired, so allow them.
    for (int i = 0; i < 5; ++i) {
        if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, i + 0x2d))  // B.3.7 bridge
            Camera_ApplyPreset(entityUI, i, timeTicks);
    }
}

} // namespace Game
