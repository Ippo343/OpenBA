// code/Game/Game/gameupdateloop.cpp
// Clean reimplementation of GameUpdateLoop vtable methods from Application-x64.exe.
//
// GameUpdateLoop (0x160 bytes), vtable at 0x14015e2f0.
// Extends UpdateLoop (0x28 bytes base).
//
// This is the main gameplay loop. It owns all game subsystems and orchestrates
// the simulation tick (Update), render frame (Draw), and lifecycle (Startup/Shutdown).
//
// Methods reimplemented:
//   - GameUpdateLoop::Update            (0x140096380) -- main simulation tick
//   - GameUpdateLoop::Draw              (0x140096010) -- render interpolated frame
//   - GameUpdateLoop::Startup           (0x140097C00) -- init UI dialogs, cursor, game systems
//   - GameUpdateLoop::Shutdown          (0x1400977C0) -- teardown all subsystems
//   - GameUpdateLoop::OnScenarioComplete(0x140096830) -- scenario win/lose notification
//
// HEAVILY INSTRUMENTED with printf() for tracing a hang during first Update tick
// after loading a tutorial scenario.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::Scene and Video::Context
// are real classes. FIRST in the block on purpose — "resolve.h" (and several of
// the standalone headers below) pull <windows.h>, whose <wingdi.h> declares a
// global `Rectangle` FUNCTION that hides `Volume::Rectangle`'s class name inside
// Volume.h's own static_asserts (Scene.h -> Geometry.h -> Volume.h).
#include "graphics/Scene.h"
#include "graphics/Camera.h"          // ENDGAME B.3.5 batch K1 — Graphics::{Camera,Renderer}
#include "graphics/AnimationManager.h"
#include "video/Context.h"
#include "video/Device.h"
#include "video/ImageBuffer.h"
#include "graphics/SunLight.h"      // B.3.x W0: Shutdown deletes m_sunLight by name
#include "graphics/Mesh.h"          // B.3.x W0: Shutdown deletes m_skyMesh by name
#include "sampler.h"
#include "options.h"
#include "Game/GameUpdateLoop.h"   // ENDGAME B.3.8 LOOP: the real class
#include "Game/WorldGeometry.h"    // caller batch CB6: m_worldGeometry's class
#include "Player/Tool.h"            // caller batch CB6: the active tool's Draw
#include "Object/exe_type.h"        // DEFINE_EXE_VTABLE
#include "Entity/entitylist_funcs.h"
#include "Object/internal_factory_audit.h"
#include <windows.h>
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "system/String.h"
#include "system/Filename.h"
#include "stress_harness.h"   // BANISHED_STRESS Update/Draw brackets (standalone/)
#include "perf_experimental.h" // PerfX non-retail perf knobs (standalone/)
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <atomic>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K4)
#include "ui/Dialog.h"         // ENDGAME B.3.9 CB: UI::Dialog::TypeId()
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K2)
#include "save/SaveLoad.h"                    // ENDGAME B.3.7 callers (K2)
#include "engine/MainLoop.h"                  // ENDGAME B.3.7 callers (K2)
#include "audio/Audio.h"                      // ENDGAME B.3.7 callers (K4)
#include "input/Input.h"                      // ENDGAME B.3.7 callers (K3)
#include "graphics/Mesh.h"               // ENDGAME B.3.9 SS2: MeshInstance::_rootOnlyUpdates
#include "graphics/ParticleInstance.h"   // ENDGAME B.3.9 SS2: ParticleInstance::_frameIndex
#include "io/FileSystem.h"               // ENDGAME B.3.9 SS2: IO::FileSystem::_singleton

// EventNotify ribbon standalone ctor (code/Game/Dialog/eventnotifydialog.cpp) —
// builds the ribbon dialog (DialogInstance base + our SA vtable + field init).
extern "C" void* EventNotifyDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);
extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);
// Event-log writer (code/Game/Game/eventlog.cpp) — appends a real entry to the
// gameWorld+0x138 event log (formatted from the events/seasons StringTables).
extern "C" void EventLog_AddSimple(int eventType, int severity);
extern "C" void BuildingGoal_Probe(const char* templateName);  // code/Game/Goal/buildinggoal.cpp
extern "C" void EventLog_AddEventWithName(void* eventLog, int eventType, int severity,
                                          void* entityIBP, void* nameStr);  // 0x140095750
// NOTE: EventLog_CheckResourceShortages (code/Game/Game/eventlog.cpp, port of
// FUN_14008c100) is intentionally NOT called from here anymore — see the retirement
// note at its former call site below. The scan now runs once per tick from
// CitizenBehavior_CheckThresholds via the registered fn_NotificationEvent path.

// GUI_PLAN Keystone F Phase 3 — player tool activation + per-frame tick
// (code/Game/Player/tool_activation_standalone.cpp). Gated behind BANISHED_LIVE_TOOLS
// so the default build stays byte-identical: build the tool-mode tree at EntityUI
// startup, tick the active tool each frame (OnKey/Serialize/cursor), draw its footprint.
extern "C" void EntityUI_BuildToolModeTree_SA(void* entityUI);
extern "C" void EntityUI_OpenRegisteredDialogs_SA(void* entityUI);  // EntityUI::Startup 2nd half
extern "C" int  EntityUI_ToolTick_SA(void* entityUI, int activeFlag, int64_t time);
extern "C" void EntityUI_ToolDraw_SA(void* entityUI, void* context, int activeFlag);
// Minimap terrain painter (code/Game/Game/minimap_paint.cpp) — standalone fallback for
// the exe FUN_1400a5140 (ImageCapture::DrawCheck), wired into fn_ImageCapture_DrawCheck.
extern "C" void Minimap_PaintTerrain(void* minimap, void* context, void* worldGeom, void* sunLight);
// In-app RenderDoc capture driver — ticked once per in-game rendered frame so a
// delayed capture armed by OptionsDialog::ApplyVideoSettings (after an MSAA/res
// apply) actually fires. No-op unless the exe was launched via RenderDoc's UI.
#include "renderdoc_integration.h"
#include "scene_watch.h"
#include "citizen_vitals.h"
#include "mouse_inject.h"                 // BANISHED_MOUSE_INJECT hover/drag route (standalone/)
#include "object/External.h"   // ENDGAME B.3.3 step 6A — must precede the
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"          // kRdata_* (B3R)
#include "Action/exe_bodies.h"           // Pf_RegionPairCacheTick_5d10
#include "Component/exe_bodies.h"        // CitizenComp_AssignHousing
#include "Dialog/exe_bodies.h"           // ApplyKeyMapToUI_Impl, Toolbar_ShowGroup_Impl
#include "Dialog/ToolbarDialog.h"        // ToolbarDialog_Ctor_Impl
#include "Entity/citizenbehavior_funcs.h" // CitizenBehavior_Update
#include "graphics/Animation.h"          // RtGraphics_AnimationManager_Dtor
#include "picking/Scene.h"
                                // `namespace Object { ... }` forward decl below
                                // (NAMESPACE HAZARD: spell ::Object::X everywhere
                                // in this file, even at global scope)


// ============================================================
// Exe globals (resolved at init time from .rdata/.data addresses)
// ============================================================

// Graphics::MeshInstance::_rootOnlyUpdates (retail: import slot 0x140151f30) and
// Graphics::ParticleInstance::_frameIndex (import slot 0x140151f38) are read as
// their class statics (ENDGAME B.3.9 SS2).

// ============================================================
// Exe function pointers (not yet in engine_funcs.h)
// ============================================================

// RegionPairCache::Tick (0x1400a5d10) - per-sim-tick staleness sweep of the
// pathfinder REGION-PAIR DISTANCE CACHE at Game+0x78. Named "UpdateTimedEvents"
// here historically; there is no timed-event queue. Ported + registered
// 2026-08-30 (reg_pf_region.cpp) - see code/Game/Action/region_paircache.cpp.
// RCX=regionPairCache (game+0x78), RDX=time

// Graphics::AnimationManager::Update(this, Time const&, bool rootOnly)
// Runtime DLL export
// ENDGAME B.3.5 batch K1: the `fn_AnimationManager_Update` cache (read out of exe
// dispatch slot 0x140152050) went with its one caller, which calls
// `Graphics::AnimationManager::Update` directly. The member is DEFINED — the door in
// runtime/src/graphics/wave23_lane_gfxbody.cpp:521 delegates to it.

// EntityList::Update (FUN_1400c4090) — tick all entity components
// RCX=entityList, RDX=time

// CitizenBehavior::Update / Map::Update (FUN_14008c2b0) — population tick
// RCX=citizenList, RDX=time

// EntityList::PostUpdate (FUN_1400c43d0) — post-update cleanup pass
// RCX=entityList

// AmbientSound::Update (FUN_1400a40e0) — ambient sound state update
// RCX=ambientSoundData

// EntityUI::Update / PlayerToolUpdate (FUN_1401026a0)
// RCX=entityUI, RDX=time(int64), R8=inputFlag(bool)
// Returns int (cursor index)

// PlayerController::CalcTransform (FUN_1400983f0)
// RCX=playerController, RDX=outTransform -> returns Transform*

// Graphics::Scene::Clean(this) — moved to engine_funcs.{h,cpp}; reuse the
// project-wide extern so MenuDLT can also call it.

// ENDGAME B.3.5: the `?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z`
// cache (typedef + static + its resolver) is GONE — all five callers in this file
// now call `Graphics::Scene::RemoveGlobalObject` directly.

// Graphics::Renderer::Draw(this, Context*, float alpha, Scene*, Camera*)
// (NB: a global with the same name is declared extern in engine_funcs.h. The two
// resolve to the same Runtime export when both succeed; we keep the local static
// here for backward-compat with the existing in-flight reimpl. Local static name
// renamed to avoid the extern/static clash.)
// ENDGAME B.3.5 batch K1: the `fn_Graphics_Renderer_Draw_local` alias (binding spelling
// 5 — a plain `s_fn_X = fn_Y` with a corrected-mangled-name fallback) went with its one
// caller, which calls `Graphics::Renderer::Draw` directly. The member is DEFINED — the
// door in runtime/src/graphics/wave27_lane_gfxbig.cpp:567 delegates to it. The [FIX-M5]
// history the fallback recorded is kept at the resolver site.

// Graphics::Renderer::~Renderer(this)

// ENDGAME B.3.5: the `?SetTime@Context@Video@@QEAAXM@Z` and
// `?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z` caches (typedefs,
// statics, resolvers and CHECK_FN lines) are GONE — both are members of block
// D1's real `Video::Context` now.

// Math::Transform3::AsMatrix(this) -> Matrix (non-trivial return: 64 bytes)
// MSVC x64 member function ABI: RCX=this, RDX=hidden retval ptr, returns RAX=retval ptr

// Math::Vector3::Normalize(this)
// REMOVED 2026-07-31: `fn_Vector3_Normalize` lived here with a ONE-ARG typedef
// `void(*)(void* self)` bound to `?Normalize@Vector3@Math@@QEBA?AV12@XZ`. That export
// returns Vector3 BY VALUE, so the ABI passes a hidden return-buffer pointer in RDX and
// the callee WRITES 8+ bytes through it — a one-arg call leaves RDX stale and the store
// lands wherever the last call left it. It was resolved and CHECK_FN'd but never called
// from this file, so it corrupted nothing; it was a loaded gun for the next caller.
// See reference_byvalue_return_hidden_rdx — this same export already caused one
// deterministic crash (the ±0.7071 garbage signature).
//
// Deleted rather than corrected: the three real callers (action_base_move.cpp:58,
// moveto.cpp:101, and the action_child_ctors/animalcomponent/weather_particles group)
// each declare their own correct 2-arg `(void* self, void* retbuf)` static. An unused,
// wrongly-typed resolver is pure liability.
//
// NOTE the mirror-image trap: `AEAV` is a REFERENCE return, NOT by-value.
// `?Normalize@Vector2@Math@@QEAAAEAV12@XZ` returns Vector2& in RAX and a one-arg typedef
// is CORRECT for it (worldgen_f390_coastline.cpp:94 looks wrong and is right). Same
// method name, different ABI between Vector2 and Vector3.

// Engine::MainLoop::SetNextLoop — declared in engine_funcs.h, resolved by EngineFuncsInit()

// fn_ImageBuffer_CopyToTexture — available from engine_funcs.h

// FUN_140095da0 — QuickSave handler (complex, just call via pointer)

// Startup sub-functions
// FUN_140099260 — SetupUIReferences(keyMapManager, UI)

// FUN_1400a90a0 — ProfessionList::Startup(professionList)

// fn_ToolbarDialog_Ctor — available from engine_funcs.h (last param is void* toolbarData)

// FUN_140068ab0 — EventNotifyDialog::Ctor(mem)

// FUN_14005e230 — AutoSaveDialog::Ctor(mem, ui, dialog, parentPtr)

// FUN_140101ee0 — EntityUI::Startup(entityUI)

// Shutdown sub-functions
// FUN_140102b60 — EntityUI::SetTool(entityUI, tool)

// CitizenComponent::AssignHousing — now shared from engine_funcs.h

// EntityList::Shutdown (FUN_1400c3d30)

// EntityList::Dtor (FUN_1400c3e60)

// CitizenBehavior::Dtor (FUN_1400907b0)

// Game::Dtor (FUN_14009ed60)

// Tutorial::Dtor (FUN_1400f8ff0)

// EntityUI::Dtor (FUN_1401024d0)

// Weather::Dtor (FUN_140093eb0)

// EventNotifier string cleanup (FUN_14000a8a0)

// CharacterGrid::Dtor (FUN_140018220) - frees the five arrays at +0x10/+0x20/
// +0x30/+0x40/+0x50 of the 0x60-byte Morton quadtree at gameWorld+0x88.
// NOT a music dtor: the MusicPlayer at +0x80 is a POD retail simply MemFrees.

// ImageCapture::Dtor (FUN_1400a4fc0)

// Runtime destructors (resolved by mangled name or ordinal)




// Draw sub-functions
// FUN_1400a5140 — ImageCapture::DrawCheck (image capture terrain update)

// FUN_140098660 — PlayerController::UpdateCamera(playerCtrl, float, float, float)

// FUN_1400938d0 — Weather::DrawUpdate(weather, ctx)

// Native time-of-day sun/lighting driver (code/Game/Game/weather_sun.cpp).
extern "C" void Weather_SunUpdate(void* self, void* ctx);

// In-game camera controller input (code/Game/Game/camera_input.cpp). Drives the
// PlayerController camera from the KeyMap (WASD pan, Q/E rotate, wheel zoom, presets).
// Only called in standalone, where EntityUI::Update (0x1401026a0) isn't registered.
namespace Game {
    void Camera_UpdateMovement(void* entityUI, int inputFlag, int64_t timeTicks);
    void Camera_UpdateHotkeys(void* entityUI, int inputFlag, int64_t timeTicks);
}

// OnScenarioComplete sub-functions
// FUN_140095580 — EventNotifier::FireNotification(notifier, eventType, priority, entityPtr)

// FUN_140095750 — EventNotifier::FireNotificationWithName(notifier, type, priority, entityPtr, nameStr)

// fn_UIString_Ctor — available from engine_funcs.h (retval, stringTable, stringName)

// fn_UIString_ToSystemString — available from engine_funcs.h (self, retval)

// FUN_1401027c0 — CalcWindFactor (for audio scene update)
// FUN_1401027c0 — despite the name, a pure DISTANCE: |cameraPos - playerCtrl anchor|.
// Returns a FLOAT in XMM0 (it tail-JMPs into the sqrtf at 0x140132010); this typedef
// used to say uint32_t, which would have read garbage out of EAX. Its Time* param is
// dead in retail — spilled at entry and never read.

// System::Time::Time(this, float seconds) -> Time* (constructor from seconds)

// Object::ExternalList::Find(this, retval, Filename const&, Object::Type) (Runtime 0x1800bf700)
// `retval` is the HIDDEN RETURN SLOT for the ExternalPtr<External> this returns
// by value; ExternalPtr is ONE RAW POINTER and holds NO reference.
//
// *** THE 4th ARGUMENT IS NEVER READ. *** Reversed 2026-09-06 (runtime port
// Wave 3 lane C): R9 is dead through the whole body — the list is searched by
// PATH only and the first path match is returned whatever its type. Do not
// expect it to filter. (It is `Object::Type`, not a `typeInfo*`; the old
// parameter name here implied both a pointer and an effect, and neither holds.)
// The match is a linear scan and is case-SENSITIVE, unlike ExternalFactory::Find
// over the same kind of path, which is case-insensitive. A path with no
// `:identifier` suffix is searched as `path + ":resource"`.

// System::Filename::Filename(this, const char*) (Runtime)

// fn_Filename_Dtor — available from engine_funcs.h

// fn_InternalBasePtr_Ctor — available from engine_funcs.h

// ENDGAME B.3.9 CB: the toolbar lookup's type id is UI::Dialog's. Retail reads it
// through the import slot ?_typeInfo@Dialog@UI (0x140151728) at 0x140097c83 /
// 0x140097cae (FUN_140097c00). The tree used to resolve exe cell 0x1401c4138 instead --
// that is CreatePathDescription's record (CreatePathDescription_RegisterType
// 0x14014ab25; its only reader is EntityUI_impl_1ee0), unregistered, so the read
// always failed and a by-name "Dialog" fallback supplied the value.

// ============================================================
// Float constants from exe .rdata
// ============================================================
// DAT_14017214c = 1.0f (UV scale / general purpose)
// DAT_14017219c = some audio time scale factor
// DAT_1401721dc, DAT_140172174, DAT_140172240 = camera update constants
// B3R: read as kRdata_14017214c / _14017219c / _1401721dc / _140172174 / _140172240.



// ============================================================
// Initialization — must be called from EngineFuncsInit or similar
// ============================================================
static int s_frameNum = 0;

// Per-render-frame token (incremented once per GameUpdateLoop::Update). Exposed so the
// tree-driven ParticleComponent tick can un-stamp each pooled emit-state at most once
// per frame (matching retail's once-per-render-frame _frameIndex advance in
// Renderer::Render), independent of how many houses share the emit or how many sim
// sub-iterations the speed multiplier runs.
extern "C" int GameUpdateLoop_CurrentFrame() { return s_frameNum; }
// Debug trace — disabled for performance, enable when debugging
#define TRACE(...) do { } while(0)

// B3R: EnsureInit and every cache it filled are gone. Each callee is called by
// name (Game/, Entity/, Action/, Dialog/ exe_bodies.h and the Runtime headers);
// the .rdata floats are kRdata_* (Game/exe_constants.h); the unread
// g_pDebugSingleton cell (0x1401c3de0 is g_pGameSettingsPtr's cell) went with it.
// Three callees have no body in the tree and are PORT GAP null arms at their call
// sites: 0x1401026a0 EntityUI::Update, 0x140101ee0 EntityUI::Startup,
// 0x1400938d0 Weather::DrawUpdate. The 0x140152160 dispatch-slot fallback for
// ExternalList::Find went with its cache (the member is defined).


// ============================================================
// Forward declarations
// ============================================================

// In-game tool-injection harness (standalone/tool_harness.cpp), env-gated by
// BANISHED_TOOL_HARNESS. No-op unless that env var is set.
extern "C" void ToolHarness_Tick(void* entityUI, int frameNum);
extern "C" void PauseMenuHarness_Tick(void* entityUI, int frameNum);  // standalone/pausemenu_harness.cpp
extern "C" void FoodPanel_Tick(void* entityUI, int frameNum);  // standalone/food_panel_harness.cpp
extern "C" void FoodPlace_Tick(int frameNum);                  // standalone/food_placement_harness.cpp
extern "C" void StartTown_DeferredSpawn_Tick(int frameNum);    // code/Game/Game/starttown.cpp
extern "C" void StartTown_ApplyPendingOccupiedVisuals(int frameNum);   // code/Game/Game/starttown.cpp
extern "C" void FoodStockCensus_Tick(int frame);                 // standalone/food_stock_census.cpp
extern "C" void CitizenDeathHarness_Tick(int frameNum);   // standalone/death_harness.cpp
extern "C" void SaveLoadHarness_Tick(void* gul, int frameNum); // standalone/saveload_harness.cpp
extern "C" void DisasterHarness_Tick(int frameNum);       // standalone/disaster_harness.cpp
extern "C" void PastureFillHarness_Tick(int frameNum);    // standalone/pasture_harness.cpp
extern "C" void MarketOverstock_Tick(int frameNum);   // standalone/market_overstock_harness.cpp
extern "C" void PanelStaffingDriver_Tick(void* entityUI, int frameNum);
extern "C" void PanelHoldHarness_Tick(void* entityUI, int frameNum);  // standalone/panel_staffing_driver.cpp
extern "C" void BuilderSeat_Tick(int frameNum);        // standalone/builder_seat_harness.cpp
extern "C" float AchClock_TimeScale();                 // standalone/ach_clock.cpp
extern "C" void  AchClock_Tick(int frameNum);          // standalone/ach_clock.cpp
extern "C" void TradeSellHarness_Tick(void* entityUI, int frameNum);  // standalone/trade_buy_harness.cpp
extern "C" void FinishBuildsHarness_Tick(int frameNum);   // standalone/finish_builds_harness.cpp
extern "C" void OrchardDiag_Tick(int frameNum);           // standalone/orchard_diag.cpp
extern "C" void FoodDiag_Tick(int frameNum);              // standalone/food_production_diag.cpp
extern "C" void CropFieldForceStart_Tick(int frameNum);   // standalone/cropfield_force_start.cpp
extern "C" void OrchardForceStart_Tick(void* entityUI, int frameNum);  // standalone/orchard_force_start.cpp
extern "C" void OrchardWorkersHarness_Tick(int frameNum);  // standalone/orchard_workers_harness.cpp
extern "C" void FoodWorkers_Tick(int frameNum);            // standalone/food_workers_harness.cpp
extern "C" void OrchardGrowthHarness_Tick(int frameNum);   // standalone/orchard_growth_harness.cpp
extern "C" void OrchardCutHarness_Tick(int frameNum);      // standalone/orchard_cut_harness.cpp
// TownHallUI gate diagnostic (code/Game/Dialog/townhallui.cpp) — one-shot report
// of the fn-pointer / type-id / town-data gates behind the town-hall panel.
extern "C" void TownHallSA_Diag(int frameNum);

// fn_MemFreeStatic is already available from engine_funcs.h

// ============================================================
// GameUpdateLoop::Update (0x140096380) — Main simulation tick
// ============================================================
// This is the core game loop tick. Called once per frame by MainLoop::Frame.
// Runs the simulation N times (speed multiplier), then updates UI, audio, etc.
//
// HEAVILY INSTRUMENTED for debugging tutorial hang.

extern "C" unsigned long long __stdcall GetTickCount64(void);   // kernel32 (fps trace)

// ============================================================
// [SIMPROF] Per-phase frame profiler (env BANISHED_SIMPROF)
// ============================================================
// Diagnostic only: wall-clock cost of each sim phase, so we can tell whether a
// speed-multiplier slowdown is a linear "N sim iterations" cost or something
// superlinear. Reports every BANISHED_SIMPROF frames (default 120) and resets.
// Zero cost when off (one predictable branch per probe).
// QueryPerformanceCounter/Frequency come from <windows.h> (LARGE_INTEGER), which
// this TU already pulls in; the wrappers below keep the call sites on plain int64.

namespace {

enum SimPhase {
    kPhPairCache = 0, kPhAnimation, kPhEntityList, kPhWeather,
    kPhCitizenList, kPhPostSim, kPhWholeUpdate, kPhCount
};
const char* const kPhaseName[kPhCount] = {
    "PairCache", "Animation", "EntityList", "Weather",
    "CitizenList", "PostSim(UI/cam/audio)", "TOTAL Update"
};

int      g_simProfOn   = -1;      // -1 = unresolved, 0 = off, else report interval
long long g_qpcFreq    = 0;
double   g_phSec[kPhCount] = {0};
int      g_phCalls[kPhCount] = {0};
// Second accumulator with its OWN reset, drained by the BANISHED_STRESS harness
// (GameUpdateLoop_SimProfTake) so its CSV rows are not tied to the stderr report interval.
double   g_phSecStress[kPhCount] = {0};
int      g_profFrames  = 0;
long long g_profSimIters = 0;

inline bool SimProfOn() {
    if (g_simProfOn < 0) {
        const char* e = Opt().simprof;
        int n = (e && e[0] && e[0] != '0') ? std::atoi(e) : 0;
        if (e && e[0] && e[0] != '0' && n <= 1) n = 120;   // "1" => default interval
        g_simProfOn = n;
        if (n) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = f.QuadPart; }
    }
    return g_simProfOn > 0;
}
inline long long QpcNow() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }

// Milliseconds between two QpcNow() samples. Self-contained: g_qpcFreq is only
// filled when the sim profiler is armed, so query the frequency here instead.
inline double QpcMs(long long ticks) {
    static long long freq = 0;
    if (!freq) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = f.QuadPart; }
    return freq ? 1000.0 * (double)ticks / (double)freq : 0.0;
}

struct PhaseTimer {
    int ph; long long t0; bool on;
    explicit PhaseTimer(int p) : ph(p), t0(0), on(SimProfOn()) {
        g_samplerTag = 2 + p;   // [Sampler] phase tag (standalone/sampler.h)
        if (on) t0 = QpcNow();
    }
    ~PhaseTimer() {
        g_samplerTag = 1;
        if (!on || !g_qpcFreq) return;
        const double s = (double)(QpcNow() - t0) / (double)g_qpcFreq;
        g_phSec[ph] += s;
        g_phSecStress[ph] += s;
        g_phCalls[ph]++;
    }
};

// Emit the accumulated report and reset the window.
void SimProfReport(int speedMultiplier, long entCount) {
    if (++g_profFrames < g_simProfOn) return;
    double total = g_phSec[kPhWholeUpdate];
    double msPerFrame = total * 1000.0 / g_profFrames;
    std::fprintf(stderr,
        "[SIMPROF] speed=%d frames=%d  %.2f ms/frame (%.1f fps equivalent)  "
        "simIters/frame=%.2f entities~=%ld\n",
        speedMultiplier, g_profFrames, msPerFrame,
        msPerFrame > 0 ? 1000.0 / msPerFrame : 0.0,
        (double)g_profSimIters / (double)g_profFrames, entCount);
    for (int i = 0; i < kPhCount; i++) {
        if (!g_phCalls[i]) continue;
        double ms = g_phSec[i] * 1000.0 / g_profFrames;
        std::fprintf(stderr,
            "[SIMPROF]   %-22s %8.3f ms/frame  %6.2f%%  calls/frame=%.2f  %.4f ms/call\n",
            kPhaseName[i], ms, total > 0 ? g_phSec[i] * 100.0 / total : 0.0,
            (double)g_phCalls[i] / (double)g_profFrames,
            g_phSec[i] * 1000.0 / (double)g_phCalls[i]);
    }
    std::fflush(stderr);
    for (int i = 0; i < kPhCount; i++) { g_phSec[i] = 0; g_phCalls[i] = 0; }
    g_profFrames = 0;
    g_profSimIters = 0;
}

// [FORCE SPEED] env BANISHED_FORCE_SPEED=N — pin the EntityUI speed multiplier
// (+0x34) and clear the paused flag (+0x30) every frame, so an A/B of 1x vs 2x
// vs 3x is deterministic under autostart with no clicking. Diagnostic only.
int g_forceSpeed = -1;
inline void ApplyForceSpeed(void* entityUIv) {
    if (g_forceSpeed < 0) {
        const char* e = Opt().forceSpeed;
        g_forceSpeed = (e && e[0]) ? std::atoi(e) : 0;
    }
    if (g_forceSpeed > 0 && entityUIv) {
        uint8_t* ui = (uint8_t*)entityUIv;
        *(char*)(ui + 0x30) = 0;                       // not paused
        *(int32_t*)(ui + 0x34) = g_forceSpeed;         // speed multiplier
    }
}

}  // namespace

// [STRESS] drain the harness copy of the phase accumulators: outMs[i] = total ms of
// phase i since the previous Take (i in SimPhase order; kPhWholeUpdate is the last).
extern "C" int GameUpdateLoop_SimProfTake(double* outMs, int maxN) {
    const int n = maxN < kPhCount ? maxN : kPhCount;
    for (int i = 0; i < n; i++) { outMs[i] = g_phSecStress[i] * 1000.0; }
    for (int i = 0; i < kPhCount; i++) g_phSecStress[i] = 0;
    return n;
}
extern "C" const char* GameUpdateLoop_SimProfName(int phase) {
    return (phase >= 0 && phase < kPhCount) ? kPhaseName[phase] : "?";
}

bool GameUpdateLoop::Update(const System::Time& timeRef) {
    // The body reads the Time through a raw pointer (unchanged from the
    // hand-built table's slot); retail's slot takes it by const&.
    void* time = const_cast<System::Time*>(&timeRef);   // B.3.8 bridge
    // Stage C entry guard (TEMPORARY): GameUpdateLoop::Load (0x140096b00, the
    // in-game world build) is not yet implemented, so on the new-game path
    // m_game (+0xF8) is still null when the loader chain transitions in. Every
    // step below dereferences the unbuilt world, AND EnsureInit() below resolves
    // dispatch slots (e.g. 0x140152058 = AchievementInstance::Update) that are
    // unregistered in standalone and null-deref. So this guard MUST run before
    // EnsureInit. Hold with a no-op tick until Load runs — remove once Load
    // constructs the world (project_newgame_path_roadmap Stage C). The STAGE C
    // marker has already fired by this point.
    if (m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Update] world not built "
                    "(m_game==null) — Stage C cliff, holding (no-op tick)\n");
            fflush(stderr);
        }
        return true;
    }


    s_frameNum++;

    // [SIMPROF] whole-Update stamp + deterministic speed override (both diagnostic-only)
    const bool simProf = SimProfOn();
    const long long frameT0 = simProf ? QpcNow() : 0;
    g_samplerTag = 1;   // [Sampler] inside Update
    struct SamplerTagReset { ~SamplerTagReset() { g_samplerTag = 0; } } samplerTagReset;
    ApplyForceSpeed(m_toolManager);
    StressHarness_UpdateBegin(s_frameNum);   // [STRESS] no-op unless BANISHED_STRESS
    PerfX::BeginFrame(m_renderer);      // [PerfX] per-frame frustum + pause flush (no-op unless enabled)

    // [EVENTS] RETIRED: the BANISHED_LIVE_EVENTS resource-shortage scanner used to
    // run here (a second port of FUN_14008c100 via EventLog_CheckResourceShortages).
    // It is superseded by the registered tick path: CitizenBehavior_CheckThresholds
    // (citizenbehavior.cpp) scans the SAME townMgr+0x170/+0x180 shortage tables every
    // tick and, now that 0x140095580 (EventLog_AddEvent) is registered as
    // fn_NotificationEvent, fires the same 0x11-0x18 sev-3 events. Keeping this block
    // would double-decrement those shared cooldown timers and double-log each
    // shortage, so it is removed — following the "retire stale duplicate" precedent.
    // EventLog_CheckResourceShortages stays defined (faithful port) but uncalled.
    // The scenario/achievement popup (0x1a-0x1d) source op_6830 (0x140096830) and the
    // ribbon / EventPopupDialog paths are unaffected and stay live for any real event.


    // Drive a Player tool (env-gated, one-shot) so otherwise-dormant tool code
    // paths fire for debugger validation. No-op unless BANISHED_TOOL_HARNESS set.
    ToolHarness_Tick(m_toolManager, s_frameNum);

    // Open the IN-GAME pause menu (the toolbar menu button's action, retail
    // ToolbarDialog_OnRemove case 3) and click one of its buttons -- by default
    // Quit Game. The front-end Quit is already drivable via BANISHED_AUTOQUIT;
    // this is the only unattended way to reach the in-game one, which needs a
    // live world underneath it. No-op unless BANISHED_PAUSEMENU is set
    // (standalone/pausemenu_harness.cpp).
    PauseMenuHarness_Tick(m_toolManager, s_frameNum);

    // Dialog harness for the six food-production panels: opens each building's info
    // panel and reports the controllers on it plus the component's CanCommand verdicts
    // (what greys or enables each button). No-op unless BANISHED_FOOD_PANEL is set
    // (standalone/food_panel_harness.cpp).
    FoodPanel_Tick(m_toolManager, s_frameNum);

    // Placement + SLOT-COVERAGE harness: where each food building landed, and which of
    // its 22 component vtable slots are actually live versus still holding the shared
    // inert stub. No-op unless BANISHED_FOOD_PLACE is set
    // (standalone/food_placement_harness.cpp).
    FoodPlace_Tick(s_frameNum);

    // Deferred demo spawns that CANNOT be sited at world-build time because their
    // siting input does not exist yet. Currently the hunter lodge, which is scored by
    // live animal density out of the character spatial-hash manager at gw+0x88 — an
    // index that is empty during StartTown because characters register during their own
    // Startup. Must run BEFORE FoodWorkers_Tick's first pass (frame 400) so the lodge is
    // staffable on that pass; it re-checks every 120 frames regardless. No-op unless
    // BANISHED_BUILD_DEMO_HUNTERLODGE is set (code/Game/Game/starttown.cpp).
    StartTown_DeferredSpawn_Tick(s_frameNum);

    // Kill N citizens on a chosen frame so the removal teardown (HandleLifeEvent ->
    // MarkForRemoval -> the +0x38 broadcast -> CitizenComponent::OnRemove) can be
    // exercised without waiting decades of game time for a natural death. No-op
    // unless BANISHED_KILL_CITIZENS is set.
    CitizenDeathHarness_Tick(s_frameNum);

    // Trigger the retail autosave path on demand and log the operation's
    // progress + the resulting .sav. No-op unless BANISHED_SAVE_TEST is set
    // (standalone/saveload_harness.cpp).
    SaveLoadHarness_Tick(this, s_frameNum);

    // Fire / citizen-disease / crop-infestation / tornado on demand. Every disaster
    // rolls its own dice inside its own component tick, and those chances are
    // denominated in YEARS (Common.rsc _fireChance = 20-40 years per 100 population),
    // so the organic arm is unreachable in a session-length run. Half the building
    // templates carry _years=0 and can never ignite at all, which is why the fire arm
    // targets a HOUSE at the centre of the residential cluster rather than rolling a
    // random structure. No-op unless one of BANISHED_FORCE_{FIRE,TORNADO,SICK,INFEST}
    // is set (standalone/disaster_harness.cpp).
    DisasterHarness_Tick(s_frameNum);

    // One-shot pasture stocking for the animal-wave test rig. No-op unless
    // BANISHED_PASTURE_FILL is set (standalone/pasture_harness.cpp).
    PastureFillHarness_Tick(s_frameNum);

    // Force-feed the market one resource past 2x its target share so MarketWork
    // slot 4 stage 3 can take its SURPLUS SHED arm (0x1400f09eb) — the only path in
    // that function haulers never reach on their own, because they stock the market
    // TO its target and stop. No-op unless BANISHED_MARKET_OVERSTOCK is set
    // (standalone/market_overstock_harness.cpp).
    MarketOverstock_Tick(s_frameNum);

    // THE VIRTUAL PLAYER: clicks workerCountUp (the retail first-seat path) on every
    // under-capacity workplace. Automatic for unattended runs only; replaces the old
    // BANISHED_PASTURE_HERDERS / BANISHED_BUILDER_AUTOPOOL cascade-bypass stand-ins
    // (standalone/panel_staffing_driver.cpp).
    PanelStaffingDriver_Tick(m_toolManager, s_frameNum);
    // Hold one building panel open so its layout can be captured (BANISHED_PANEL_HOLD).
    PanelHoldHarness_Tick(m_toolManager, s_frameNum);

    // Player stand-in for the town-hall Builder spinner. No-op unless BANISHED_BUILDERS.
    BuilderSeat_Tick(s_frameNum);
    // Non-retail clock knobs for the time-gated achievements: advance the year
    // counter stat 26 reads. No-op unless BANISHED_ACH_YEAR_BOOST is set
    // (standalone/ach_clock.cpp).
    AchClock_Tick(s_frameNum);

    // THE VIRTUAL MERCHANT CUSTOMER: with a merchant docked, fill the sell rows and
    // press Trade through the ported checkout. No-op unless BANISHED_TRADE_SELL is
    // set (standalone/trade_buy_harness.cpp).
    TradeSellHarness_Tick(m_toolManager, s_frameNum);

    // Finish every structure still under construction, the way the stress placer
    // finishes its own. No-op unless BANISHED_FINISH_BUILDS is set
    // (standalone/finish_builds_harness.cpp).
    FinishBuildsHarness_Tick(s_frameNum);

    // Periodic OrchardComponent state dump + PlantOrchard/HarvestOrchard work
    // counters. No-op unless BANISHED_ORCHARD_DIAG is set (standalone/orchard_diag.cpp).
    OrchardDiag_Tick(s_frameNum);

    // Periodic census of ALL SIX food-production classes (state, staffing, fishing
    // rate, hunt progress) plus town-storage totals by resource type — the number
    // that actually answers "does this building feed the town". No-op unless
    // BANISHED_FOOD_DIAG is set (standalone/food_production_diag.cpp).
    FoodDiag_Tick(s_frameNum);
    FoodStockCensus_Tick(s_frameNum);

    // Switch a demo crop field on (and optionally fire its plant command) so states
    // 9/10/11 are reachable without the drag tool. No-op unless
    // BANISHED_CROPFIELD_FORCE_START is set (standalone/cropfield_force_start.cpp).
    CropFieldForceStart_Tick(s_frameNum);

    // One-shot: force an untyped demo orchard's master switch on + assign a real
    // tree type via the live combo-select path, so the state-2->state-9 transition
    // has something to prove. No-op unless BANISHED_ORCHARD_FORCE_START is set
    // (standalone/orchard_force_start.cpp).
    OrchardForceStart_Tick(m_toolManager, s_frameNum);

    // Seat the first worker at each orchard — the one player click the employment
    // allocator structurally cannot make (it gates on roster != 0). No-op unless
    // BANISHED_ORCHARD_WORKERS is set (standalone/orchard_workers_harness.cpp).
    OrchardWorkersHarness_Tick(s_frameNum);

    // Seat the first worker at each Fishing Dock / Hunter Lodge / Gatherer's Hut /
    // Crop Field — the one player click those buildings have no tool for yet. The
    // employment allocator gates on a NON-ZERO roster, so it can rebalance but never
    // seat worker #1. No-op unless BANISHED_FOOD_WORKERS is set
    // (standalone/food_workers_harness.cpp).
    FoodWorkers_Tick(s_frameNum);

    // Fast-forward tree maturity, then fruit ripeness, so the harvest half of the orchard
    // state machine is reachable in one run rather than a multi-hour soak. No-op unless
    // BANISHED_ORCHARD_GROW is set (standalone/orchard_growth_harness.cpp). It advances two
    // simulation values only — every gate downstream of them still has to pass.
    OrchardGrowthHarness_Tick(s_frameNum);

    // Click the panel's CUT button (command id 3) on the player's behalf, so state 20 can
    // be exercised without a human. No-op unless BANISHED_ORCHARD_CUT is set
    // (standalone/orchard_cut_harness.cpp). Destructive to the orchard it fires on.
    OrchardCutHarness_Tick(s_frameNum);

    // One-shot at frame 120. Despite the name this is NOT purely a diagnostic: it
    // also publishes nine shared element type-id globals that other dialogs read
    // (toolbardialog / optionsdialog / saveloaddialog / spritecaptureui), so it must
    // stay ungated — see the note above TownHallSA_Diag in townhallui.cpp. The
    // reporting half is gated on BANISHED_TOWNHALL_DIAG.
    TownHallSA_Diag(s_frameNum);


    // Apply the deferred founding-house "occupied" visuals now that the scene is live
    // (the Scene mesh-visual can't run during the build-time founding pass — it
    // deadlocks; see g_wpcRosterOnly). One-shot, drains its queue on an early frame.
    StartTown_ApplyPendingOccupiedVisuals(s_frameNum);

    // (StatusIcon "no workers" warnings are now raised by the genuine event
    // WorkPlaceComponent_SetAccepting at workplace activation and cleared by
    // WorkplaceComponent_Attach — the per-frame occupancy-poll drain was removed.
    // See code/Game/Component/statusiconcomponent.cpp.)

    // (StorageDisplayComponent pile rendering is now driven natively by EntityList::Update's
    // 5-bucket +0x48 tick — the explicit per-frame drain was removed once the spurious
    // bit-0x400 cull on start-town buildings was fixed. See storagedisplaycomponent.cpp.)

    TRACE("[GameUpdateLoop::Update] ENTER frame=%d (self=%p, time=%p, gameTime=%lld)\n",
          s_frameNum, (void*)this, time, (long long)m_gameTime);

    // Step 1: Check if UI pauses game
    TRACE("[GameUpdateLoop::Update] checking PausesGame...\n");
    bool paused = m_userInterface->PausesGame();  // B.3.6 bridge
    TRACE("[GameUpdateLoop::Update] PausesGame = %d\n", (int)paused);

    if (paused) {
        TRACE("[GameUpdateLoop::Update] game paused by UI, skipping simulation loop\n");
        goto post_simulation;
    }

    {
        int speedMultiplier = 0;
        int iteration = 0;

        // Step 2: Read speed multiplier from EntityUI
        // EntityUI+0x30 = paused flag, EntityUI+0x34 = speed multiplier
        uint8_t* entityUI = (uint8_t*)m_toolManager;
        bool entityUIPaused = (*(char*)(entityUI + 0x30)) != 0;
        if (!entityUIPaused) {
            speedMultiplier = *(int32_t*)(entityUI + 0x34);
        }
        TRACE("[GameUpdateLoop::Update] entityUIPaused=%d, speedMultiplier=%d\n",
               (int)entityUIPaused, speedMultiplier);

        if (speedMultiplier > 0) {
            // Accumulate game time
            m_gameTime += *(int64_t*)time;
            TRACE("[GameUpdateLoop::Update] gameTime updated to %lld\n", (long long)m_gameTime);
        }

        // Step 3: Inner simulation loop
        if (speedMultiplier > 0) {
            do {
                TRACE("[GameUpdateLoop::Update] === sim iteration %d/%d ===\n", iteration + 1, speedMultiplier);

                // 3a. Region-pair distance cache staleness sweep
                void* game = m_game;
                void* regionPairCache = *(void**)((uint8_t*)game + 0x78);
                TRACE("[GameUpdateLoop::Update] calling RegionPairCache::Tick (cache=%p)...\n", regionPairCache);
                { PhaseTimer pt(kPhPairCache);
                Pf_RegionPairCacheTick_5d10(static_cast<int64_t*>(regionPairCache),
                            reinterpret_cast<const int64_t*>(time));   // 0x1400a5d10
                }
                TRACE("[GameUpdateLoop::Update] RegionPairCache::Tick done\n");

                // 3b. AnimationManager::Update
                bool rootOnly = (iteration < speedMultiplier - 1);
                PerfX::SetIteration(iteration, speedMultiplier);   // [PerfX] ANIM_BATCH needs the iteration index
                ::Graphics::MeshInstance::_rootOnlyUpdates = rootOnly;
                TRACE("[GameUpdateLoop::Update] calling AnimationManager::Update (rootOnly=%d)...\n", (int)rootOnly);
                { PhaseTimer pt(kPhAnimation);
                // B.3.5 bridge — the exe dispatch slot 0x140152050 read and its
                // [GUARD-M5] readiness guard went with the door. `time` is a
                // `const System::Time*` here, so it is dereferenced at the call.
                reinterpret_cast<Graphics::AnimationManager*>(m_animationManager)
                    ->Update(*reinterpret_cast<const System::Time*>(time),
                             rootOnly);   // B.3.5 bridge
                }
                TRACE("[GameUpdateLoop::Update] AnimationManager::Update done\n");

                // 3c. EntityList::Update (component ticks)
                // Uses custom reimpl — original fn_EntityList_Update (0x1400c4090)
                // hangs due to a spinlock issue in the hooked environment.
                TRACE("[GameUpdateLoop::Update] calling EntityList::Update (entityList=%p)...\n", m_entityList);
                { PhaseTimer pt(kPhEntityList);
                EntityList_Update(reinterpret_cast<EntityList*>(m_entityList), time);
                }
                TRACE("[GameUpdateLoop::Update] EntityList::Update done\n");

                // 3d. Day counter increment
                {
                    int32_t* dayCounter = (int32_t*)m_dayCounter;
                    int64_t* accumulated = (int64_t*)(dayCounter + 2); // +0x08
                    int64_t* dayLength = (int64_t*)(dayCounter + 4);   // +0x10
                    *accumulated += *(int64_t*)time;
                    if (*accumulated >= *dayLength) {
                        *dayCounter = *dayCounter + 1;
                        *accumulated -= *dayLength;
                        TRACE("[GameUpdateLoop::Update] day counter incremented to %d\n", *dayCounter);
                    }
                }

                // 3e. Weather::ResetTime
                TRACE("[GameUpdateLoop::Update] calling Weather_ResetTime...\n");
                { PhaseTimer pt(kPhWeather);
                Weather_ResetTime_Reimpl(m_weather, (void*)time);   // 0x1400944e0
                }
                TRACE("[GameUpdateLoop::Update] Weather_ResetTime done\n");

                // 3f. CitizenBehavior::Update / Map::Update
                TRACE("[GameUpdateLoop::Update] calling CitizenList::Update (citizenList=%p)...\n", m_citizenList);
                { PhaseTimer pt(kPhCitizenList);
                CitizenBehavior_Update(static_cast<CitizenBehaviorRaw*>(m_citizenList),
                                       reinterpret_cast<int64_t*>(time));   // 0x14008c2b0
                }
                TRACE("[GameUpdateLoop::Update] CitizenList::Update done\n");

                // 3g. Tutorial frame/time increment
                {
                    uint8_t* tutorial = (uint8_t*)m_tutorial;
                    int32_t* frameCount = (int32_t*)(tutorial + 0x08);
                    int64_t* tutorialTime = (int64_t*)(tutorial + 0x130);
                    *frameCount = *frameCount + 1;
                    *tutorialTime = *tutorialTime + *(int64_t*)time;
                    TRACE("[GameUpdateLoop::Update] tutorial frame=%d, tutorialTime=%lld\n",
                           *frameCount, (long long)*tutorialTime);
                }

                // 3h. Global frame index increment
                ::Graphics::ParticleInstance::_frameIndex =
                    ::Graphics::ParticleInstance::_frameIndex + 1;

                iteration++;
                if (simProf) g_profSimIters++;   // [SIMPROF]
            } while (iteration < speedMultiplier);
        }

        // Step 4: Achievement check
        TRACE("[GameUpdateLoop::Update] checking achievements (scenarioInstance=%p, completed=%d)...\n",
               m_scenarioInstance, (int)m_scenarioCompleted);
        if (m_scenarioInstance != nullptr && m_scenarioCompleted == 0) {
            TRACE("[GameUpdateLoop::Update] calling AchievementInstance::Update...\n");
            m_scenarioInstance->Update(   // B.3.7 bridge
                *reinterpret_cast<const System::Time*>(time));
            TRACE("[GameUpdateLoop::Update] AchievementInstance::Update done\n");

            bool isMet = m_scenarioInstance->IsMet();   // B.3.7 bridge
            TRACE("[GameUpdateLoop::Update] AchievementInstance::IsMet = %d\n", (int)isMet);

            if (!isMet) {
                bool hasFailed = m_scenarioInstance->HasFailed();   // B.3.7 bridge
                TRACE("[GameUpdateLoop::Update] AchievementInstance::HasFailed = %d\n", (int)hasFailed);
                if (hasFailed) {
                    TRACE("[GameUpdateLoop::Update] calling OnScenarioComplete (failed)...\n");
                    OnScenarioComplete();
                    TRACE("[GameUpdateLoop::Update] OnScenarioComplete done\n");
                }
            } else {
                TRACE("[GameUpdateLoop::Update] calling OnScenarioComplete (met)...\n");
                OnScenarioComplete();
                TRACE("[GameUpdateLoop::Update] OnScenarioComplete done\n");
            }
        }

        // Step 4b: AchievementManager::Update with scaled time
        TRACE("[GameUpdateLoop::Update] calling AchievementManager::Update...\n");
        {
            float seconds = reinterpret_cast<const System::Time*>(time)->AsSeconds();   // B.3.4b bridge
            int64_t scaledTime[1];
            // AchClock_TimeScale() is 1.0 unless BANISHED_ACH_TIME_SCALE is set, so
            // this is retail's `seconds * speedMultiplier` unchanged by default. This
            // one Time is what every TimeGoalInstance accumulates, so scaling it here
            // advances all of them at once (standalone/ach_clock.cpp).
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(scaledTime),
                               seconds * (float)speedMultiplier * AchClock_TimeScale());
            // Achievement::AchievementManager::_singleton (retail: **import slot
            // 0x1401514f0). The null check on the manager is the M5 first-slice guard.
            if (::Achievement::AchievementManager::_singleton) {
                void* achievementMgr = ::Achievement::AchievementManager::_singleton;

                // BANISHED_ACH_IGNORE_MODS=1 -- NON-RETAIL TEST FIXTURE.
                //
                // AchievementManager::Update's first act (Runtime 0x18009e6c6) is
                //     if (*(int*)(IO::FileSystem::_singleton + 0x38) > 0) return;
                // i.e. AreUserPackFilesEnabled(). Retail disables achievements outright
                // whenever any .pkm is mounted, silently. That gate lives inside the
                // Runtime DLL, so it cannot be flagged off at its source -- instead we
                // zero the mod count for the duration of THIS ONE CALL and put it back
                // immediately, so no other reader ever observes the lie.
                //
                // What this buys: the goal wiring can be exercised in a modded session
                // (e.g. with DebugMenu.pkm mounted). What it does NOT buy: evidence that
                // a retail player could earn the achievement -- retail would have
                // refused to evaluate at all. An award obtained under this flag is a
                // test result, not a legitimate unlock, and it still writes a real
                // HKCU/.../Ach<N> value. Belongs with the other declared deviations in
                // docs/waves/achievements_wave.md section 7.
                int32_t* modGate = nullptr;
                int32_t  savedGate = 0;
                static int s_ignoreMods = -1;
                if (s_ignoreMods < 0) {
                    const char* e = Opt().achIgnoreMods;
                    s_ignoreMods = (e && e[0] && e[0] != '0') ? 1 : 0;
                }
                if (s_ignoreMods == 1) {
                    if (::IO::FileSystem::_singleton) {
                        modGate = reinterpret_cast<int32_t*>(
                            reinterpret_cast<uint8_t*>(::IO::FileSystem::_singleton) + 0x38);
                        savedGate = *modGate;
                        if (savedGate > 0) {
                            *modGate = 0;
                            static bool s_said = false;
                            if (!s_said) {
                                s_said = true;
                                fprintf(stderr, "[Ach] BANISHED_ACH_IGNORE_MODS: masking the "
                                        "user-pack gate (FileSystem+0x38 was %d) for the "
                                        "AchievementManager::Update call ONLY. Retail would "
                                        "evaluate NOTHING here -- any award this run is a test "
                                        "result, not a legitimate unlock.\n", savedGate);
                                fflush(stderr);
                            }
                        }
                    }
                }

                // With the gate masked, Update runs on to its instance list at mgr+0x18
                // and derefs it unguarded -- faithful, because retail always has one by
                // then (Reset() builds it at new-game, Persist restores it on load).
                // Measured 2026-09-05: a LOADED town could reach here with mgr+0x18
                // null, and Update then faulted READ of 0 at Runtime+0x9E6F4.
                // ROOT-CAUSED 2026-09-06 (Wave 4 lane H) and FIXED AT THE CAUSE: the
                // Load path never called GameContainer_EnsureLists, so
                // AchievementManager::SetAchievements never ran for a loaded game.
                // Retail does it once at app init (GameContainer_Init 0x140099db0);
                // standalone replaced that with per-dialog calls in New Game, Scenario
                // and Achievements and MISSED Load. Fixed in MainMenuDialog_OpenLoad.
                // The old diagnosis on this line -- "Reset()/Persist has not populated
                // it, a load-path defect" -- named the wrong MECHANISM: it was a
                // missing init, not a Persist defect.
                // The report below is now a FIXTURE THAT SHOULD NEVER FIRE. If it does,
                // some OTHER path into a game is missing that same call.
                void* instList = *reinterpret_cast<void**>(
                    static_cast<uint8_t*>(achievementMgr) + 0x18);
                if (modGate && !instList) {
                    static bool s_warned = false;
                    if (!s_warned) {
                        s_warned = true;
                        fprintf(stderr, "[Ach] BANISHED_ACH_IGNORE_MODS: manager has NO "
                                "INSTANCE LIST (mgr+0x18 null) -- nothing can be evaluated "
                                "or awarded this run. The Load path's missing "
                                "GameContainer_EnsureLists was fixed 2026-09-06, so this "
                                "should be unreachable -- some OTHER path into a game "
                                "is missing that call. Skipping the Update call.\n");
                        fflush(stderr);
                    }
                } else {
                    reinterpret_cast<Achievement::AchievementManager*>(achievementMgr)->Update(   // B.3.7 bridge
                        *reinterpret_cast<const System::Time*>(scaledTime));
                }

                if (modGate) *modGate = savedGate;   // restore before anything else reads it
            }
        }
        TRACE("[GameUpdateLoop::Update] AchievementManager::Update done\n");

        // Step 5: EntityList post-update cleanup
        TRACE("[GameUpdateLoop::Update] calling EntityList::PostUpdate...\n");
        EntityList_PostUpdate(reinterpret_cast<EntityList*>(m_entityList));
        TRACE("[GameUpdateLoop::Update] EntityList::PostUpdate done\n");

        // Step 6: Camera transform update
        TRACE("[GameUpdateLoop::Update] updating camera transform...\n");
        {
            // Math::Transform3 is 0x30 = 48 bytes (static_assert in
            // runtime/types/math/Math.h:118), and retail's AsTransform3
            // (0x18001fcd0) writes twelve MOVSS stores covering ret+0x00..0x2f.
            // This buffer was declared [44] until Wave 9, so every frame wrote
            // FOUR BYTES PAST THE END of a stack buffer. Line 2281 of this same
            // file already had it right at [48]; this was a plain typo, not a
            // layout disagreement. Found by lane P while diagnosing an
            // AsTransform3 divergence that turned out to be an oracle defect --
            // the overflow is older and unrelated to that port.
            uint8_t transform3[48]; // Transform3 scratch space (0x30 bytes)
            void* playerCtrl = m_playerController;
            void* result = Reimpl_FUN_1400983f0(playerCtrl, transform3);   // 0x1400983f0
            // AsTransform3 on the result, then SetCameraToWorld
            Math::Transform3 t3 =
                reinterpret_cast<const Math::Transform*>(result)->AsTransform3();
            // B.3.5 bridge -- ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z.
            // `playerCtrl` is `m_playerController` (+0xE0, GameUpdateLoop.h:115) and
            // its ctor Reimpl_FUN_1400985b0 (gameupdateloop_load.cpp:591) constructs
            // a `Graphics::Camera` at `p + 0x34` -- the receiver is proved by the
            // writer, not by the variable's name. There was no readiness guard here.
            reinterpret_cast<Graphics::Camera*>((uintptr_t)playerCtrl + 0x34)
                ->SetCameraToWorld(t3);   // B.3.5 bridge
        }
        TRACE("[GameUpdateLoop::Update] camera transform done\n");
    }

post_simulation:
    // [SIMPROF] everything from here to EXIT is the once-per-render-frame work
    // (ambient/terrain/UI/tools/camera/audio) — NOT multiplied by game speed.
    { PhaseTimer ptPost(kPhPostSim);

    // Step 7: MusicPlayer::Update - exe 0x1400a40e0, not yet ported. [GUARD-M5]
    // NOT an ambient-sound update despite the fn_ name: it picks the next music
    // track by month + town mood, so while this stays null background music
    // never plays. See engine/types/GameUpdateLoop.h +0x80.
    TRACE("[GameUpdateLoop::Update] calling MusicPlayer::Update...\n");
    MusicPlayer_Update(m_musicPlayer);   // 0x1400a40e0
    TRACE("[GameUpdateLoop::Update] MusicPlayer::Update done\n");

    // Step 8: World geometry vtable call (terrain update)
    TRACE("[GameUpdateLoop::Update] calling WorldGeometry::Update...\n");
    {
        // Caller batch CB6: slot 1 (vtable +0x08) of the real class. The receiver is
        // PROVED: Load's step 23 builds it with WorldGeometry's ctor (and Load is
        // m_worldGeometry's only writer). A virtual call, as retail's is.
        m_worldGeometry->Update(
            *reinterpret_cast<const System::Time*>(time));   // B.3.8 bridge
    }
    TRACE("[GameUpdateLoop::Update] WorldGeometry::Update done\n");

    // Step 9: UI::UserInterface::Update
    // fn_UI_Update returns the cursor id; engine_funcs.h now declares that directly
    // (the export is ...QEAAH...). The local cast below is now redundant.
    TRACE("[GameUpdateLoop::Update] calling UI::UserInterface::Update...\n");
    // Wave 10D lane MOUSE: the ONLY seam at which a synthetic cursor can be
    // placed -- after this frame's input poll, before the UI decodes it. Both
    // calls are a cached-getenv no-op unless BANISHED_MOUSE_INJECT is set.
    MouseInject::PreUpdate(m_userInterface);
    int uiCursor = m_userInterface->Update(  // B.3.6 bridge
        *reinterpret_cast<const System::Time*>(time));
    MouseInject::PostUpdate(m_userInterface);
    TRACE("[GameUpdateLoop::Update] UI::UserInterface::Update done (cursor=%d)\n", uiCursor);

    // Step 10: EntityUI / player tool update
    int cursorIndex = 1;
    {
        bool pausedNow = m_userInterface->PausesGame();  // B.3.6 bridge
        if (!pausedNow) {
            // Check if UI has input focus: userInterface+0xd9 == 0 means no dialog has input
            bool noInputFocus = (m_userInterface->_mouseCaptured == 0);   // B.3.8 bridge
            // Standalone: EntityUI::Update (the camera/tool controller) is not
            // registered, so fn_EntityUI_Update is null and the camera never moved.
            // Drive the camera directly from the KeyMap here — hotkeys (pause/presets)
            // first, then per-frame movement, matching the original helper order.
            Game::Camera_UpdateHotkeys(m_toolManager, noInputFocus ? 1 : 0, *(int64_t*)time);
            Game::Camera_UpdateMovement(m_toolManager, noInputFocus ? 1 : 0, *(int64_t*)time);
            // Phase 3: tick the active player tool (the tool half of EntityUI::Update
            // that camera_input.cpp omitted) — OnKey (dialog + left-click confirm) +
            // Serialize (ghost preview) + cursor. -1 = no active tool -> keep default.
            {
                int toolCursor = EntityUI_ToolTick_SA(m_toolManager, noInputFocus ? 1 : 0, *(int64_t*)time);
                if (toolCursor >= 0) cursorIndex = toolCursor;
            }
            TRACE("[GameUpdateLoop::Update] calling EntityUI::Update (inputFlag=%d)...\n", (int)noInputFocus);
            // PORT GAP 0x1401026a0 EntityUI::Update: not ported; retail calls it here
            // (RCX=entityUI, RDX=time, R8B=inputFlag) and takes its cursor index.
            int64_t result = 1;
            cursorIndex = (int)result;
            TRACE("[GameUpdateLoop::Update] EntityUI::Update done (cursorIndex=%d)\n", cursorIndex);
        }

        // Override cursor if UI has input or game is paused
        bool hasInput = (m_userInterface->_mouseCaptured != 0);   // B.3.8 bridge
        if (hasInput || m_userInterface->PausesGame()) {  // B.3.6 bridge
            cursorIndex = uiCursor;
        }
    }

    // Step 11: Set cursor
    TRACE("[GameUpdateLoop::Update] setting cursor (index=%d)...\n", cursorIndex);
    {
        void* keyMapMgr = g_gameContainer;  // [GUARD-M5]
        if (keyMapMgr) {
            uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x28 + (int64_t)cursorIndex * 8);
            m_userInterface->SetCursor(  // B.3.6 bridge
                Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(cursorVal)));
        }
    }
    TRACE("[GameUpdateLoop::Update] cursor set\n");

    // Step 12: Audio::Scene::Update
    TRACE("[GameUpdateLoop::Update] calling Audio::Scene::Update...\n");
    {
        void* playerCtrl = m_playerController;
        // Build listener position from PlayerController camera data
        float listenerPos[3];
        // Transform3 at playerCtrl+0x98, camera offset at +0x1c,+0x20,+0x24
        // Copy transform3 for audio listener
        float tx = *(float*)((uintptr_t)playerCtrl + 0x98);
        float ty = *(float*)((uintptr_t)playerCtrl + 0x9c);
        float tz = *(float*)((uintptr_t)playerCtrl + 0xa0);
        listenerPos[0] = tx + 0.0f;
        listenerPos[1] = ty + 0.0f;
        listenerPos[2] = tz + 0.0f;

        // Get speed multiplier for audio time scaling
        int speedMul = 0;
        uint8_t* entityUI = (uint8_t*)m_toolManager;
        if (*(char*)(entityUI + 0x30) == 0) {
            speedMul = *(int32_t*)(entityUI + 0x34);
        }

        {
            float seconds = reinterpret_cast<const System::Time*>(time)->AsSeconds();   // B.3.4b bridge
            int64_t scaledTime[1];
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(scaledTime),
                               seconds * (float)speedMul * kRdata_14017219c);
            float windFactor = CalcWindFactor_Reg(scaledTime);   // 0x1401027c0
            // Sanity-check the camera-position sub-offset (playerCtrl+0x34+0x88) once:
            // it was derived from stack arithmetic in the retail disasm, not from a
            // struct definition. CLAUDE.md documents the default camera dist as 40, so
            // a plausible reading is tens of units — not 0, not 1e38.
            {
                static int s_once = 0;
                if (s_once < 3) {
                    s_once++;
                    std::fprintf(stderr, "[AudioScene] windFactor(camera dist)=%.3f\n", windFactor);
                    std::fflush(stderr);
                }
            }
            // windFactor is the FLOAT the mangled name's trailing `M` says; an
            // earlier uint32_t cache typedef converted it (40.5f -> 40).
            System::Time audioNow;    audioNow._ticks    = *(int64_t*)time;
            System::Time audioScaled; audioScaled._ticks = scaledTime[0];
            reinterpret_cast<Audio::Scene*>(m_audioScene)->Update(
                audioNow, audioScaled, *m_random,
                *reinterpret_cast<const Vector3*>(listenerPos), windFactor);   // B.3.7 bridge
        }
    }
    TRACE("[GameUpdateLoop::Update] Audio::Scene::Update done\n");

    // Step 13: Quicksave/quickload key checks
    // Original asm at 0x1400966c1: triple deref from slot, dispatch call at 0x140151758
    // Match proven working pattern from dragtool/pathtool — no null guards (original has none).
    {
        void* keyMap = Input::Configuration::_singleton->_keyMap;

        if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x21)) {  // B.3.7 bridge
            m_quickSavePressed = 1;
        }

        // HARNESS, not retail: BANISHED_SCREENSHOT_AT=<frame> raises the same
        // request bit the 0x21 keypress raises, once, at that frame. Retail's
        // GameUpdateLoop::TakeScreenshot (0x140095da0, ported in
        // code/Game/Game/w2_takescreenshot.cpp) then runs from Draw and writes a
        // .bmp, exactly as a real keypress would.
        //
        // WHY THIS EXISTS: the diff oracle, [LodCensus] and [DrawCensus] between
        // them cover the scene walk and the draw submission, and NONE of them can
        // answer "is it actually on the screen" -- they all stop short of the GPU.
        // Wave 6 needed that answer for a report of missing terrain resources and
        // had no way to get it unattended. Combined with BANISHED_SEED and
        // BANISHED_CAMERA_PIN (which make framing reproducible run to run) this
        // turns a visual regression into a file two runs can diff.
        {
            static const char* const ssEnv = Opt().screenshotAt;
            static const long ssAt = ssEnv ? std::atol(ssEnv) : -1;
            static long ssFrame = 0;
            static bool ssFired = false;
            ++ssFrame;
            if (ssAt >= 0 && !ssFired && ssFrame >= ssAt) {
                ssFired = true;
                m_quickSavePressed = 1;
                std::fprintf(stderr, "[Screenshot] BANISHED_SCREENSHOT_AT=%ld reached at frame %ld"
                                     " -- raising the TakeScreenshot request bit\n", ssAt, ssFrame);
            }
        }

        if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x22)) {  // B.3.7 bridge
            m_quickLoadPressed = 1;
        }
        TRACE("[GameUpdateLoop::Update] quicksave/quickload checked (save=%d, load=%d)\n",
              m_quickSavePressed, m_quickLoadPressed);
    }

    // Step 14: SaveLoad::Update if active
    if (m_saveLoad != nullptr) {  // [GUARD-M5]
        TRACE("[GameUpdateLoop::Update] calling SaveLoad::Update...\n");
        m_saveLoad->Update(*reinterpret_cast<const System::Time*>(time));   // B.3.7 bridge
        TRACE("[GameUpdateLoop::Update] SaveLoad::Update done\n");
    }

    // Step 14b: Calculate sun direction (normalized)
    // Original at 0x140096717-0x14009676f computes direction from PlayerController
    // offsets (+0x98..+0xac), normalizes via Math::Vector3::Normalize, but the
    // normalized result goes to a stack temporary and is never stored to SunLight
    // or passed to Scene::Clean. Verified in disassembly: no MOV between the
    // Normalize CALL and the Scene::Clean CALL. This appears to be dead code in
    // the original binary — possibly a removed/vestigial lighting update.
    {
        void* playerCtrl = m_playerController;
        uintptr_t pc = (uintptr_t)playerCtrl;
        float dirX = *(float*)(pc + 0xa4) + *(float*)(pc + 0x98) + 0.0f;
        float dirY = *(float*)(pc + 0xa8) + *(float*)(pc + 0x9c) + 0.0f;
        float dirZ = (*(float*)(pc + 0xac) + *(float*)(pc + 0xa0)) - kRdata_14017214c;
        float dir[3] = { dirX, dirY, dirZ };
        float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len > 0.0f) { dir[0] /= len; dir[1] /= len; dir[2] /= len; }
        // Result intentionally unused — matches original binary behavior.
        (void)dir;
    }

    // [diag] BANISHED_SCENE_WATCH — validate the node array BEFORE Clean walks it,
    // so a recycled node is reported the frame it appears rather than whenever
    // Clean's vtable+0x40 dispatch happens to reach it ("EXEC of <heap address>").
    SceneWatch_Check(m_graphicsScene);
    // [diag] BANISHED_SCENE_CENSUS — membership + position churn (the "models keep
    // popping in and out" report). Same place as the integrity check: before Clean,
    // after every component has had its per-frame say.
    SceneWatch_Census(m_graphicsScene);
    if (GameUpdateLoop::_singleton) CitizenVitals_Tick(GameUpdateLoop::_singleton);

    // Step 15: Graphics::Scene::Clean
    TRACE("[GameUpdateLoop::Update] calling Graphics::Scene::Clean...\n");
    // B.3.5 bridge — the `if (fn_Graphics_Scene_Clean)` readiness guard (and its
    // SKIP trace) went with the door call.
    reinterpret_cast<Graphics::Scene*>(m_graphicsScene)->Clean();   // B.3.5 bridge
    TRACE("[GameUpdateLoop::Update] Graphics::Scene::Clean done\n");

    // Step 16: Image capture check
    {
        uint8_t* imageCapture = (uint8_t*)m_imageCapture;
        if (*(int64_t*)(imageCapture + 8) != 0 && *imageCapture != 0) {
            TRACE("[GameUpdateLoop::Update] processing image capture...\n");
            ::Video::Device::_singleton
                ->Lock();   // B.3.5 bridge
            reinterpret_cast<Video::ImageBuffer*>(*(void**)(imageCapture + 0x10))
                ->CopyToTexture(*(void**)(imageCapture + 0x28),
                                *(int32_t*)(imageCapture + 4),
                                *(int32_t*)(imageCapture + 4));   // B.3.5 bridge
            ::Video::Device::_singleton
                ->Unlock();   // B.3.5 bridge
            *imageCapture = 0;
            TRACE("[GameUpdateLoop::Update] image capture done\n");
        }
    }

    // Step 17: Next loop transition check
    if (m_nextLoop != nullptr) {
        // Check if fullscreen fade is done (UI+0xf0 == 0.0f)
        float fadeProgress = m_userInterface->_fadeVelocity;   // B.3.8 bridge
        if (fadeProgress == 0.0f) {
            TRACE("[GameUpdateLoop::Update] transitioning to next loop %p\n", m_nextLoop);
            ::Engine::MainLoop::_singleton->SetNextLoop(   // B.3.7 bridge
                m_nextLoop);
        }
    }

    }  // [SIMPROF] end kPhPostSim

    // [SIMPROF] close the whole-frame timer before reporting so TOTAL includes
    // this frame, then emit/reset on the reporting interval.
    if (simProf && g_qpcFreq) {
        g_phSec[kPhWholeUpdate] += (double)(QpcNow() - frameT0) / (double)g_qpcFreq;
        g_phCalls[kPhWholeUpdate]++;
        long entCount = -1;
        if (m_entityList)
            entCount = *reinterpret_cast<int*>((uint8_t*)m_entityList + 0x30);
        int spd = 0;
        if (m_toolManager && *(char*)((uint8_t*)m_toolManager + 0x30) == 0)
            spd = *(int32_t*)((uint8_t*)m_toolManager + 0x34);
        SimProfReport(spd, entCount);
    }
    // [STRESS] close the Update bracket; the harness's own periodic work (CSV row,
    // staffing pass) runs inside this call, AFTER its timer has stopped.
    StressHarness_UpdateEnd(this, s_frameNum);

    TRACE("[GameUpdateLoop::Update] EXIT\n");
    return true;
}


// ============================================================
// GameUpdateLoop::Draw (0x140096010) — Render one interpolated frame
// ============================================================

void GameUpdateLoop::Draw(float alpha) {
    // Stage C entry guard (TEMPORARY): see GameUpdateLoop_Update — must run
    // before EnsureInit (which null-derefs an unregistered dispatch slot in
    // standalone). m_game is null until GameUpdateLoop::Load builds the world;
    // the draw path dereferences the unbuilt world (UI, renderer, scenes).
    if (m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Draw] world not built "
                    "(m_game==null) — Stage C cliff, skipping draw\n");
            fflush(stderr);
        }
        return;
    }

    StressHarness_DrawBegin();   // [STRESS] render-frame period + Draw bracket

    // Tick the RenderDoc capture state machine once per rendered frame. The
    // engine's Present happens after this Draw returns (MainLoop::Draw EndFrame),
    // so a StartFrameCapture here brackets a full frame of GPU work.
    RenderDocIntegration::OnFrame();
    // One-shot in-game capture: BANISHED_CAPTURE_INGAME=N arms a RenderDoc
    // frame capture N in-game frames after the first drawn frame (default 120,
    // ~enough for the camera to settle on the start town). Requires the exe to
    // be launched under RenderDoc (renderdoc.dll injected). Used to inspect the
    // terrain draw's PS bindings + shadow cbuffer with shadows ON.
    {
        static int s_capArm = -2;      // -2=unread, -1=disabled, >=0 = countdown to arm
        if (s_capArm == -2) {
            const char* e = Opt().captureIngame;
            s_capArm = (e && e[0]) ? (atoi(e) > 0 ? atoi(e) : 120) : -1;
        }
        if (s_capArm >= 0) {
            if (s_capArm == 0) {
                RenderDocIntegration::RequestCaptureInFrames(0);
            }
            --s_capArm;
        }
    }

    TRACE("[GameUpdateLoop::Draw] ENTER (alpha=%.3f)\n", alpha);

    // Step 1: Check pause — if paused, force alpha to 1.0 (no interpolation)
    bool paused = m_userInterface->PausesGame();  // B.3.6 bridge
    bool entityUIPaused = (*(char*)((uintptr_t)m_toolManager + 0x30) != 0);
    if (paused || entityUIPaused) {
        alpha = kRdata_14017214c; // 1.0f
    }

    // Step 2: Set time on video context
    TRACE("[GameUpdateLoop::Draw] setting video context time...\n");
    {
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        int64_t engineFrameTime = reinterpret_cast< ::Engine::MainLoop*>(engine)->_frameUpdateTime;   // B.3.8 bridge

        float gameSeconds = reinterpret_cast<const System::Time*>((void*)&m_gameTime)->AsSeconds();   // B.3.4b bridge
        float frameSeconds = reinterpret_cast<const System::Time*>((void*)&engineFrameTime)->AsSeconds();   // B.3.4b bridge
        reinterpret_cast<Video::Context*>(videoCtx)->SetTime(
            gameSeconds + frameSeconds * alpha);   // B.3.5 bridge
    }
    TRACE("[GameUpdateLoop::Draw] video context time set\n");

    // Step 3: Image capture terrain update check
    TRACE("[GameUpdateLoop::Draw] checking image capture terrain...\n");
    {
        uint8_t* imageCapture = (uint8_t*)m_imageCapture;
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        // ImageCapture::DrawCheck (exe FUN_1400a5140) is our clean port
        // Minimap_PaintTerrain. It fires
        // once — when the minimap's terrain GPU buffer (imageCapture+0x08) is still
        // null — to render the terrain top-down into the minimap pixel arrays.
        if (*(int64_t*)(imageCapture + 8) == 0) {
            // Remove and re-add global objects for terrain update
            void* gameWorld = GameUpdateLoop::_singleton;
            void* graphicsScene = static_cast<GameUpdateLoop*>(gameWorld)->m_graphicsScene;   // B.3.8 bridge

            Graphics::Scene* gs = reinterpret_cast<Graphics::Scene*>(graphicsScene);   // B.3.5 bridge
            gs->RemoveGlobalObject(m_sunLight);       // B.3.5 bridge
            gs->RemoveGlobalObject(m_worldGeometry);  // B.3.5 bridge

            Minimap_PaintTerrain(imageCapture, videoCtx, m_worldGeometry, m_sunLight);   // 0x1400a5140

            gs->AddGlobalObject(m_sunLight);       // B.3.5 bridge
            gs->AddGlobalObject(m_worldGeometry);  // B.3.5 bridge
        }
    }
    TRACE("[GameUpdateLoop::Draw] image capture terrain done\n");

    // Step 4: Camera update
    TRACE("[GameUpdateLoop::Draw] updating player camera...\n");
    Reimpl_FUN_140098660(m_playerController,
                         kRdata_1401721dc, kRdata_140172174, kRdata_140172240);   // 0x140098660
    TRACE("[GameUpdateLoop::Draw] player camera done\n");

    // Step 5: Weather draw update
    TRACE("[GameUpdateLoop::Draw] calling Weather::DrawUpdate...\n");
    {
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        // PORT GAP 0x1400938d0 Weather::DrawUpdate: not ported; retail calls it here
        // (RCX=weather, RDX=videoCtx).
        // Native time-of-day sun/lighting. The original Weather::DrawUpdate
        // (above) no-ops in standalone (not reimplemented + Environment.rsc not
        // loaded), so drive the day cycle directly from the real base-game
        // keyframes. Animates sun elevation + light/ambient/fog colors so
        // shadows track the in-game day (see weather_sun.cpp).
        Weather_SunUpdate(this, videoCtx);

        // The original Weather::DrawUpdate (exe 0x1400938d0) ALSO pushes four
        // terrain weather params from the Weather struct into the Context
        // cbuffer (mirrored at ctx+0x288 and ctx+0x3b8, dirty bit 0x10800):
        //   weather+0x1c = SNOW COVERAGE   (FUN_140093420: saturate((snowAccum -
        //                  thresh) * scale))
        //   weather+0x20 = seasonal fade A
        //   weather+0x24 = seasonal fade B
        //   weather+0x28 = max active weather (rain/snow) intensity
        // In standalone DrawUpdate no-ops AND the weather sim (Weather::ResetTime
        // 0x1400944e0 -> FUN_140093420) never runs, so these were never written
        // and the terrain shader read stale/garbage from those cbuffer slots —
        // showing snow on ~half the ground. The Weather ctor (FUN_140093f60)
        // zero-inits all four, which is exactly the spring/clear/no-snow state,
        // so push them faithfully (they animate for free once the snow sim is
        // wired). Set BANISHED_FORCE_SNOW=<float> to override the coverage for
        // testing.
        {
            uint8_t* weather = reinterpret_cast<uint8_t*>(m_weather);
            if (weather && videoCtx) {
                uint8_t* ctx = reinterpret_cast<uint8_t*>(videoCtx);
                uint32_t s0 = *reinterpret_cast<uint32_t*>(weather + 0x1c);  // snow coverage
                uint32_t s1 = *reinterpret_cast<uint32_t*>(weather + 0x20);
                uint32_t s2 = *reinterpret_cast<uint32_t*>(weather + 0x24);
                uint32_t s3 = *reinterpret_cast<uint32_t*>(weather + 0x28);
                static int s_forceSnow = -1;
                static float s_snowVal = 0.0f;
                if (s_forceSnow < 0) {
                    const char* e = Opt().forceSnow;
                    s_forceSnow = (e && e[0]) ? 1 : 0;
                    if (s_forceSnow) s_snowVal = (float)atof(e);
                }
                if (s_forceSnow) s0 = *reinterpret_cast<uint32_t*>(&s_snowVal);
                *reinterpret_cast<uint32_t*>(ctx + 0x3b8) = s0;
                *reinterpret_cast<uint32_t*>(ctx + 0x3bc) = s1;
                *reinterpret_cast<uint32_t*>(ctx + 0x3c0) = s2;
                *reinterpret_cast<uint32_t*>(ctx + 0x3c4) = s3;
                *reinterpret_cast<uint32_t*>(ctx + 0x288) = s0;
                *reinterpret_cast<uint32_t*>(ctx + 0x28c) = s1;
                *reinterpret_cast<uint32_t*>(ctx + 0x290) = s2;
                *reinterpret_cast<uint32_t*>(ctx + 0x294) = s3;
                *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x10800;
            }
        }
    }
    TRACE("[GameUpdateLoop::Draw] Weather::DrawUpdate done\n");

    // Step 6: Copy renderer shadow mode from engine.
    // renderer+0x3c is the shadow mode: Renderer::AddScene (Runtime 0x180105700) stores it
    // as the main draw group's +0xe8, and Renderer::Draw (0x180104bb0) reads it back into
    // renderer+0x3c before dispatching each global object's vtable+0x48 (DrawShadowMap) /
    // vtable+0x50 (Draw). SunLight::DrawShadowMap (0x180106bc0) is gated by
    // `if (renderer+0x3c != 0)`; SunLight::Draw (0x1801067b0, main pass) is NOT — it always
    // runs Context::SetLight + Renderer::DrawItemList to light and render the terrain.
    {
        uint32_t shadowMode = 0;
        // The shadow-mode source field. The ORIGINAL GameUpdateLoop::Draw (exe 0x140096010)
        // sets renderer+0x3c from *(*g_pVideoConfigSingleton + 0x30) — the shadow-QUALITY
        // setting (index, 0=off, >0=on), NOT engine+0x30 (g_pMainLoopSingleton ==
        // MainLoop, so engine+0x30 is garbage). Shadows are DEFAULT-ON: mirror the
        // original and read VideoConfig+0x30 so SunLight::DrawShadowMap engages (verified
        // crash-free; terrain + buildings cast/receive). The day cycle is driven by
        // Weather_SunUpdate above.
        if (::Video::Configuration::_singleton) {
            shadowMode = *(uint32_t*)((uintptr_t)::Video::Configuration::_singleton + 0x30);
        }
        *(uint32_t*)((uintptr_t)m_renderer + 0x3c) = shadowMode;
    }

    // [diag] BANISHED_LOD_CENSUS — replicate MeshInstance's distance-based detail
    // pick with the very camera this frame is about to draw with, and count the
    // flips. The scene census cleared every CULL input; the LOD pick reads a
    // different set (the union with the PREVIOUS world box, and the mesh's own
    // distance table), so it needs its own measurement.
    SceneWatch_LodCensus(m_graphicsScene,
                         (const void*)((uintptr_t)m_playerController + 0x34),
                         // The MAIN scene pass's variant index, measured not guessed:
                         // Renderer::Draw hands BeginDrawGroup the scene entry's +0xec,
                         // which is AddScene's param_6 and is a literal 0 there. (The
                         // obvious guess, renderer+0x3c, is the shader-DETAIL level that
                         // rides to Context+0xc; the shadow cascades use 1 and 2.)
                         0);

    // Step 7: Graphics::Renderer::Draw
    TRACE("[GameUpdateLoop::Draw] calling Renderer::Draw...\n");
    {
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        reinterpret_cast<Graphics::Renderer*>(m_renderer)->Draw(
            *reinterpret_cast<Video::Context*>(videoCtx), alpha,
            *reinterpret_cast<const Graphics::Scene*>(m_graphicsScene),
            *reinterpret_cast<const Graphics::Camera*>(
                (uintptr_t)m_playerController + 0x34));   // B.3.5 bridge
    }
    TRACE("[GameUpdateLoop::Draw] Renderer::Draw done\n");

    // [diag] BANISHED_LOD_CENSUS — count what the frame actually DREW. The lists
    // survive until the next Renderer::Reset, so read them here, right after Draw.
    SceneWatch_DrawItemCensus(m_renderer);

    // Step 8: SaveLoad::Draw if active
    if (m_saveLoad != nullptr) {
        TRACE("[GameUpdateLoop::Draw] calling SaveLoad::Draw...\n");
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        // Save::SaveLoad::Draw — the ExternalPtr<Material> is a bare register value (B7-5).
        m_saveLoad->Draw(   // B.3.7 bridge
            *reinterpret_cast<Video::Context*>(videoCtx),
            Object::ExternalPtr<Graphics::Material>(m_uiMaterial));
        TRACE("[GameUpdateLoop::Draw] SaveLoad::Draw done\n");
    }

    // Step 9: Quicksave screenshot
    if (m_quickSavePressed != 0) {
        TRACE("[GameUpdateLoop::Draw] processing quicksave screenshot...\n");
        GameUpdateLoop_TakeScreenshot((void*)this);   // 0x140095da0
        TRACE("[GameUpdateLoop::Draw] quicksave screenshot done\n");
    }

    // Step 10: Set camera matrices on video context for UI
    TRACE("[GameUpdateLoop::Draw] setting camera matrices...\n");
    {
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        uintptr_t pc = (uintptr_t)m_playerController;

        // Original copies Transform3 (12 floats at +0x98..+0xC4 = 48 bytes) and projection
        // matrix (16 floats at +0xC8) from PlayerController onto the stack, then calls
        // AsMatrix → SetCamera. Math::Transform3 is a 48-byte struct (3x4). Copying only
        // 7 floats (the earlier bug) left AsMatrix reading 20 bytes of stack garbage —
        // including the camera translation — producing a corrupt view matrix that threw
        // the 3D placement overlay behind the camera (RenderDoc VS-out w<0). Terrain was
        // unaffected (drawn earlier inside Renderer::Draw with its own camera).
        // AsMatrix returns Matrix by value (64 bytes) — requires hidden return pointer ABI.
        uint8_t transform3[48]; // 12 floats = Transform3 (48 bytes)
        for (int i = 0; i < 12; i++) {
            ((uint32_t*)transform3)[i] = *(uint32_t*)(pc + 0x98 + i * 4);
        }

        Math::Matrix viewMatrix =
            reinterpret_cast<const Math::Transform3*>(transform3)->AsMatrix();

        // Projection matrix at +0xC8 (16 floats = 64 bytes)
        uint8_t projMatrix[64];
        for (int i = 0; i < 16; i++) {
            ((uint32_t*)projMatrix)[i] = *(uint32_t*)(pc + 0xC8 + i * 4);
        }

        reinterpret_cast<Video::Context*>(videoCtx)->SetCamera(
            viewMatrix, *reinterpret_cast<const Math::Matrix*>(projMatrix));   // B.3.5 bridge
    }
    TRACE("[GameUpdateLoop::Draw] camera matrices set\n");

    // Step 11: EntityUI toolbar draw (via vtable call on EntityUI's current tool)
    TRACE("[GameUpdateLoop::Draw] calling EntityUI toolbar draw...\n");
    {
        uintptr_t entityUI = (uintptr_t)m_toolManager;
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge

        // Check if UI has input focus or pauses game -> pass as flag
        bool hasInput = (m_userInterface->_mouseCaptured != 0);   // B.3.8 bridge
        bool pauseCheck = m_userInterface->PausesGame();  // B.3.6 bridge
        uint64_t drawFlag = (hasInput || pauseCheck) ? 1 : 0;

        // EntityUI+0x28 is InternalBasePtr to current tool
        void* tool = *(void**)(entityUI + 0x28);
        if (tool) {
            // Caller batch CB6: slot 8 (vtable +0x40) `Tool::Draw(context, active)`.
            // The receiver is PROVED by its only writer, Toolbar_ShowGroup_Impl
            // (toolbardialog.cpp): it stores the Factory::CreateObject result only
            // after IsDerivedFrom(DWORD_1401c6eb8), and that cell is TypeId(Tool).
            // The old typedef passed the flag as a qword; the slot reads its low byte.
            reinterpret_cast<Tool*>(tool)->Draw(videoCtx, static_cast<char>(drawFlag));   // B.3.8 bridge
        }
    }
    TRACE("[GameUpdateLoop::Draw] EntityUI toolbar draw done\n");

    // Step 12: UI::UserInterface::Draw
    TRACE("[GameUpdateLoop::Draw] calling UI::UserInterface::Draw...\n");
    {
        void* engine = ::Engine::MainLoop::_singleton;
        void* videoCtx = reinterpret_cast< ::Engine::MainLoop*>(engine)->_context;   // B.3.8 bridge
        m_userInterface->Draw(*reinterpret_cast<Video::Context*>(videoCtx));  // B.3.6 bridge
    }
    TRACE("[GameUpdateLoop::Draw] UI::UserInterface::Draw done\n");

    // Step 13: Quickload screenshot (after UI draw)
    if (m_quickLoadPressed != 0) {
        TRACE("[GameUpdateLoop::Draw] processing quickload screenshot...\n");
        GameUpdateLoop_TakeScreenshot((void*)this);   // 0x140095da0
        TRACE("[GameUpdateLoop::Draw] quickload screenshot done\n");
    }

    StressHarness_DrawEnd();   // [STRESS]
    TRACE("[GameUpdateLoop::Draw] EXIT\n");
}



// ============================================================

void GameUpdateLoop::Startup() {
    // [HUD-DIAG] Unconditional one-shot: confirm Startup is invoked at all, and
    // whether the world is already built (m_game set) when it runs. If Startup
    // fires before Load (m_game==null), it early-returns forever and the HUD
    // would never be created — an ordering bug we must catch here.
    {
        static bool s_called = false;
        if (!s_called) {
            s_called = true;
            fprintf(stderr, "[HUD-DIAG] Startup CALLED self=%p m_game=%p "
                    "m_userInterface=%p m_resourceList(+0x08)=%p\n",
                    (void*)this,
                    (void*)m_game,
                    (void*)m_userInterface,
                    (void*)_resourceList);
            fflush(stderr);
        }
    }

    // Stage C entry guard (TEMPORARY): see GameUpdateLoop_Update — must run
    // before EnsureInit (which null-derefs an unregistered dispatch slot in
    // standalone). On the new-game path m_game (+0xF8) is null until
    // GameUpdateLoop::Load builds the world; Startup dereferences m_userInterface
    // (+0x98, also null) below. Skip cleanly until Load is implemented.
    if (m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Startup] world not built "
                    "(m_game==null) — Stage C cliff, skipping startup\n");
            fflush(stderr);
        }
        return;
    }


    TRACE("[GameUpdateLoop::Startup] ENTER (self=%p)\n", (void*)this);

    // Set audio scene on UI
    TRACE("[GameUpdateLoop::Startup] setting audio scene on UI...\n");
    m_userInterface->_audioScene = m_audioScene;   // B.3.8 bridge

    // Setup UI references (cursors, lists, etc.)
    TRACE("[GameUpdateLoop::Startup] calling SetupUIRefs...\n");
    ApplyKeyMapToUI_Impl(g_gameContainer, m_userInterface);   // 0x140099260
    // Report the OUTCOME. "SetupUIRefs done" printed identically for the whole time
    // 0x140099260 was unregistered and the call was a no-op; the fields it writes are
    // the only evidence it ran. uiScale 0 == never applied (edittool.cpp:674 reads it).
    {
        uintptr_t ui = (uintptr_t)m_userInterface;
        std::fprintf(stderr, "[Startup] SetupUIRefs: uiScale=%.3f sndButtonDown=%p sndAlert=%p\n",
                     ui ? *(float*)(ui + 0xDC) : -1.0f,
                     ui ? *(void**)(ui + 0xB8) : nullptr,
                     ui ? *(void**)(ui + 0xC8) : nullptr);
        std::fflush(stderr);
    }

    // Set default cursor
    TRACE("[GameUpdateLoop::Startup] setting default cursor...\n");
    {
        void* keyMapMgr = g_gameContainer;  // [GUARD-M5]
        if (keyMapMgr) {
            uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x30);
            m_userInterface->SetCursor(  // B.3.6 bridge
                Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(cursorVal)));
        }
    }

    // Enable cursor
    TRACE("[GameUpdateLoop::Startup] enabling cursor...\n");
    m_userInterface->EnableCursor(true);  // B.3.6 bridge

    // ProfessionList startup — builds the sorted profession ARRAY at profList+0x20/+0x28
    // (FUN_1400a90a0) for the profession UI panel. m_professionList (gw+0xb0) is normally
    // DEFERRED (null) in the standalone slice; it goes live under BANISHED_LIVE_PROFESSIONLIST.
    // ProfessionList_Startup_Impl is the port of 0x1400a90a0 (B3R: called by name; its
    // readiness guard went with the door).
    TRACE("[GameUpdateLoop::Startup] calling ProfessionList::Startup...\n");
    if (m_professionList)
        ProfessionList_Startup_Impl(m_professionList);   // 0x1400a90a0
    TRACE("[GameUpdateLoop::Startup] ProfessionList::Startup done\n");

    // Game HUD dialogs (Toolbar/EventNotify/AutoSave) are DEFERRED in the M5 first
    // slice: they require the loop's resource list (m_resourceList, +0x08 — the
    // standalone loader does not populate it yet, so ExternalList::Find faults) AND
    // their exe dialog ctors (not reimplemented). Disable the whole section until
    // both are wired. [GUARD-M5]
    // [HUD-DIAG] One-shot probe: did the preload populate the loop's resource
    // list (+0x08)? And does ExternalList::Find resolve the toolbar Dialog
    // resource out of it? This is the gate for enabling HUD creation below.
    {
        static bool s_diag = false;
        if (!s_diag) {
            s_diag = true;
            void* rl = _resourceList;
            fprintf(stderr, "[HUD-DIAG] probe: m_resourceList=%p DialogTypeId=%u\n",
                    rl, UI::Dialog::TypeId());
            fflush(stderr);
            if (rl) {
                System::Filename fnbuf("Dialog/Toolbar.rsc");
                void* foundBase = nullptr;
                uint32_t typeInfo = UI::Dialog::TypeId();
                foundBase = reinterpret_cast<const ::Object::ExternalList*>(rl)
                                ->Find(fnbuf, typeInfo)._ptr;
                fprintf(stderr, "[HUD-DIAG] ExternalList::Find(\"Dialog/Toolbar.rsc\") -> foundBase=%p\n",
                        foundBase);
                fflush(stderr);
            } else {
                fprintf(stderr, "[HUD-DIAG] Find probe skipped (resourceList null)\n");
                fflush(stderr);
            }
        }
    }

    // HUD bring-up — the in-game Toolbar (Keystone F). Gated behind the env var
    // BANISHED_LIVE_TOOLBAR (default OFF → the default build stays byte-identical).
    // The resource is resolved via ExternalFactory::FindOrLoad (m_resourceList is
    // empty in standalone — the preload that populates it is off by default).
    // The Toolbar ctor (0x1400b9910 -> ToolbarDialog_Ctor_Impl) is already registered.
    //
    // NOTE: the toolbar dialog constructs + ticks crash-free with BANISHED_LIVE_TOOLBAR
    // alone, but its BUTTONS come from the "Toolbar" item externals, which are loaded by
    // the resource preload. So a fully POPULATED, visibly-rendered toolbar (10 buttons)
    // needs BANISHED_LIVE_TOOLBAR=1 *and* BANISHED_GAME_PRELOAD=1 — verified rendering
    // the real bottom-right toolbar bar on screen, crash-free. With LIVE_TOOLBAR alone
    // the dialog is present but empty (itemCount=0).
    const bool kCreateToolbar = true;

    // Captured toolbar dialog — the EventNotify ribbon parents to it (positions the
    // ribbon just below the toolbar; see EventNotifyDialog_Update). Null if the
    // toolbar was not created (LIVE_TOOLBAR off), in which case the ribbon Update
    // early-returns on the null parent, so we skip creating the ribbon too.
    void* g_ribbonParentToolbar = nullptr;

    // Create and add Toolbar dialog
    TRACE("[GameUpdateLoop::Startup] creating Toolbar dialog...\n");
    if (kCreateToolbar) {
        // (ENDGAME B.3.9 TR: ToolbarStandalone_Init and its file are gone -- the
        // toolbar reads Toolbar / ToolbarDescription / UI element TypeId() directly.)
        System::Filename filename("Dialog/Toolbar.rsc");

        // Find the Dialog resource. First try the loop's resource list (populated
        // when preload is on); if that is empty or misses, fall back to
        // ExternalFactory::FindOrLoad (the path OpenToolDialog/ToggleAll use — loads
        // the .rsc directly, robust in standalone where m_resourceList is empty).
        void* foundBase = nullptr;
        uint32_t typeInfo = UI::Dialog::TypeId();   // retail: [?_typeInfo@Dialog@UI] (0x140097c83)
        if (_resourceList)
            foundBase = reinterpret_cast<const ::Object::ExternalList*>(_resourceList)
                            ->Find(filename, typeInfo)._ptr;
        if (foundBase == nullptr &&
            ::Object::ExternalFactory::_singleton) {
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            foundBase = const_cast<::Object::External*>(
                ::Object::ExternalFactory::_singleton
                    ->FindOrLoad(filename));                                 // B.3.3 bridge
        }

        void* dialogRes = nullptr;
        if (foundBase != nullptr) {
            if (::Object::IsDerivedFromNullSafe(foundBase, UI::Dialog::TypeId())) {   // 0x140097cba
                dialogRes = foundBase;
            }
        }

        fprintf(stderr, "[HUD] Toolbar: foundBase=%p dialogRes=%p UI=%p\n",
                foundBase, dialogRes, (void*)m_userInterface);
        fflush(stderr);

        // Allocate and construct ToolbarDialog (0x108 bytes). [GUARD-M5]
        void* mem = System::MemoryAllocator::MemAlloc(0x108, 8);
        void* toolbar = nullptr;
        // Retail (0x140097cf1) constructs whenever the allocation succeeded; the
        // `dialogRes != nullptr` skip that sat here coped with a failed type-id
        // resolution, which cannot happen now (ENDGAME B.3.9 CB).
        if (mem != nullptr) {
            int64_t nullParent[1] = {0};
            new (nullParent) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            toolbar = ToolbarDialog_Ctor_Impl(mem, m_userInterface, dialogRes, nullParent, nullptr);   // 0x1400b9910
        }

        // Add to UI (only if the dialog was actually constructed).
        if (toolbar) {
            g_ribbonParentToolbar = toolbar;  // parent for the EventNotify ribbon
            int64_t dialogPtr[1];
            new (dialogPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(toolbar));   // B.3.3 bridge
            m_userInterface->AddDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dialogPtr));
            fprintf(stderr, "[HUD] Toolbar CONSTRUCTED + added to UI: toolbar=%p itemCount(+0xD8)=%d "
                    "buttonCount(+0xC8)=%d speedCount(+0x100)=%d\n", toolbar,
                    *(int32_t*)((uintptr_t)toolbar + 0xD8), *(int32_t*)((uintptr_t)toolbar + 0xC8),
                    *(int32_t*)((uintptr_t)toolbar + 0x100));
        } else {
            fprintf(stderr, "[HUD] Toolbar NOT constructed (mem=%p) — freeing\n", mem);
            if (mem) System::MemoryAllocator::MemFree(mem);
        }
        fflush(stderr);
    }
    TRACE("[GameUpdateLoop::Startup] Toolbar dialog added\n");

    // Create and add EventNotify ribbon (BANISHED_LIVE_EVENTS). Shows recent game
    // events as a strip below the toolbar. Uses the FindOrLoad fallback (empty
    // m_resourceList in standalone) like the toolbar, and parents to the toolbar
    // dialog so Update can position the ribbon. Skipped if the toolbar is absent
    // (the ribbon Update early-returns on a null parent anyway).
    TRACE("[GameUpdateLoop::Startup] creating EventNotify dialog...\n");
    if (g_ribbonParentToolbar != nullptr) {
        System::Filename filename("Dialog/EventNotify.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = UI::Dialog::TypeId();   // retail: [?_typeInfo@Dialog@UI]
        if (_resourceList)
            foundBase = reinterpret_cast<const ::Object::ExternalList*>(_resourceList)
                            ->Find(filename, typeInfo)._ptr;
        if (foundBase == nullptr &&
            ::Object::ExternalFactory::_singleton) {
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            foundBase = const_cast<::Object::External*>(
                ::Object::ExternalFactory::_singleton
                    ->FindOrLoad(filename));                                 // B.3.3 bridge
        }
        void* dialogRes = nullptr;
        if (foundBase != nullptr && ::Object::IsDerivedFromNullSafe(foundBase, UI::Dialog::TypeId()))
            dialogRes = foundBase;

        fprintf(stderr, "[HUD] EventNotify: foundBase=%p dialogRes=%p parentToolbar=%p\n",
                foundBase, dialogRes, g_ribbonParentToolbar);
        fflush(stderr);

        void* mem = System::MemoryAllocator::MemAlloc(0xF0, 8);
        void* eventNotify = nullptr;
        if (mem != nullptr && dialogRes != nullptr) {
            int64_t parentIBP[1];
            new (parentIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(g_ribbonParentToolbar));   // B.3.3 bridge
            eventNotify = EventNotifyDialog_Ctor_Standalone(mem, m_userInterface, dialogRes, parentIBP);
        }

        if (eventNotify) {
            int64_t dialogPtr[1];
            new (dialogPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(eventNotify));   // B.3.3 bridge
            m_userInterface->AddDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dialogPtr));
            fprintf(stderr, "[HUD] EventNotify ribbon CONSTRUCTED + added to UI: %p\n", eventNotify);
            fflush(stderr);
        } else if (mem) {
            System::MemoryAllocator::MemFree(mem);
        }
    }
    TRACE("[GameUpdateLoop::Startup] EventNotify dialog added\n");

    // Create and add AutoSave dialog
    TRACE("[GameUpdateLoop::Startup] creating AutoSave dialog...\n");
    // The whole autosave FEATURE hangs off this dialog:
    // its timer (started by AutoSaveDialog::Slot4 from the Options interval) is the
    // only clock, and its timer-event handler (Slot8 -> Dialog_AutoSaveHandler) is the
    // only caller of SaveLoad::SetOperation(SAVE). While this block was hardcoded off
    // the game never autosaved at all - the Options 'Autosave' setting was inert.
    // Uses the ExternalFactory::FindOrLoad fallback like the toolbar/EventNotify,
    // since m_resourceList is empty in standalone. Parented to nothing (retail
    // passes a null parent IBP here too).
    {
        System::Filename filename("Dialog/AutoSave.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = UI::Dialog::TypeId();   // retail: [?_typeInfo@Dialog@UI]
        if (_resourceList)
            foundBase = reinterpret_cast<const ::Object::ExternalList*>(_resourceList)
                            ->Find(filename, typeInfo)._ptr;
        if (foundBase == nullptr &&
            ::Object::ExternalFactory::_singleton) {
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            foundBase = const_cast<::Object::External*>(
                ::Object::ExternalFactory::_singleton
                    ->FindOrLoad(filename));                                 // B.3.3 bridge
        }
        void* dialogRes = nullptr;
        if (foundBase != nullptr && ::Object::IsDerivedFromNullSafe(foundBase, UI::Dialog::TypeId()))
            dialogRes = foundBase;

        void* mem = System::MemoryAllocator::MemAlloc(0xD0, 8);
        void* autoSave = nullptr;
        if (mem != nullptr && dialogRes != nullptr) {
            int64_t nullParent[1] = {0};
            new (nullParent) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            autoSave = AutoSaveDialog_Ctor_Standalone(mem, m_userInterface, dialogRes, nullParent);
        }

        if (autoSave) {
            int64_t dialogPtr[1];
            new (dialogPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(autoSave));   // B.3.3 bridge
            m_userInterface->AddDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dialogPtr));
        } else if (mem) {
            System::MemoryAllocator::MemFree(mem);
        }
        fprintf(stderr, "[AutoSave] dialog res=%p instance=%p (null instance = no autosave)\n",
                dialogRes, autoSave);
        fflush(stderr);
    }
    TRACE("[GameUpdateLoop::Startup] AutoSave dialog added\n");

    // Set fullscreen fade
    TRACE("[GameUpdateLoop::Startup] setting fullscreen fade...\n");
    m_userInterface->SetFullscreenFade(true, 0xFF000000, true);  // B.3.6 bridge
    m_userInterface->SetFullscreenFade(false, 0xFF000000, false);  // B.3.6 bridge
    TRACE("[GameUpdateLoop::Startup] fullscreen fade set\n");

    TRACE("[GameUpdateLoop::Startup] calling EntityUI::Startup...\n");
    // PORT GAP 0x140101ee0 EntityUI::Startup: not ported; retail calls it here
    // (RCX=entityUI). Standalone builds the tool-mode tree itself (Phase 3) so a
    // build-button click resolves the PlacementTool.
    EntityUI_BuildToolModeTree_SA(m_toolManager);
    // ...and its second half: open every dialog registered in the EntityUI+0x50 tree.
    // That tree is how a tutorial asks for its panel (FinishTutorialInstance::Slot4 ->
    // 0x1401001f0 only RECORDS the request); nothing else opens it. Empty on a plain
    // new game, so this is a no-op there — hence no live gate.
    EntityUI_OpenRegisteredDialogs_SA(m_toolManager);
    TRACE("[GameUpdateLoop::Startup] EntityUI::Startup done\n");

    TRACE("[GameUpdateLoop::Startup] EXIT\n");
}


// ============================================================
// GameUpdateLoop::Shutdown (0x1400977C0) — Teardown all subsystems
// ============================================================

extern "C" void RedzoneAlloc_ArmTeardownCheck();   // standalone/redzone_alloc.cpp
void GameUpdateLoop::Shutdown() {
    PerfX::Reset();   // [PerfX] drop cached model pointers before the world is freed
    RedzoneAlloc_ArmTeardownCheck();   // no-op unless BANISHED_REDZONE=watch + BANISHED_TEARDOWN_HEAPCHECK=1

    TRACE("[GameUpdateLoop::Shutdown] ENTER (self=%p)\n", (void*)this);

    // Stage C entry guard (TEMPORARY): on the new-game path the world was never
    // built (GameUpdateLoop::Load is a stub), so m_game (+0xF8) is null and the
    // teardown below dereferences unbuilt subsystems (m_toolManager, m_userInterface,
    // m_audioScene, m_citizenList). Nothing was constructed, so there is nothing
    // to tear down — return. (Destroy still runs afterward to free the loop.)
    if (m_game == nullptr) {
        fprintf(stderr, "[GameUpdateLoop::Shutdown] world not built (m_game==null) "
                "— Stage C cliff, nothing to tear down\n");
        fflush(stderr);
        return;
    }

    // Clear achievement event handler
    TRACE("[GameUpdateLoop::Shutdown] clearing achievement event handler...\n");
    if (::Achievement::AchievementManager::_singleton) {
        void* achievementMgr = ::Achievement::AchievementManager::_singleton;
        reinterpret_cast<Achievement::AchievementManager*>(achievementMgr)->SetEventHandler(nullptr);   // B.3.7 bridge
    }

    // Clear game flags (remove 0x100 bit)
    TRACE("[GameUpdateLoop::Shutdown] clearing game flags...\n");
    {
        void* gameWorld = GameUpdateLoop::_singleton;
        uint32_t* flags = &static_cast<GameUpdateLoop*>(gameWorld)->m_gameFlags;   // B.3.8 bridge
        *flags = *flags & 0xFFFFFEFF;
    }

    // EntityUI::SetTool(nullptr)
    TRACE("[GameUpdateLoop::Shutdown] calling EntityUI::SetTool(0)...\n");
    Toolbar_ShowGroup_Impl(m_toolManager, 0);   // 0x140102b60

    // Disable cursor
    TRACE("[GameUpdateLoop::Shutdown] disabling cursor...\n");
    m_userInterface->EnableCursor(false);  // B.3.6 bridge

    // Stop all audio
    TRACE("[GameUpdateLoop::Shutdown] stopping all audio...\n");
    reinterpret_cast<Audio::Scene*>(m_audioScene)->StopAll();   // B.3.7 bridge

    // Citizen housing reassignment loop
    TRACE("[GameUpdateLoop::Shutdown] reassigning citizen housing...\n");
    {
        uint8_t* citizenList = (uint8_t*)m_citizenList;
        int32_t citizenCount = *(int32_t*)(citizenList + 0xa8);
        void** citizenArray = *(void***)(citizenList + 0xa0);

        for (int i = 0; i < citizenCount; i++) {
            void* citizen = citizenArray[i];

            uint8_t* cit = (uint8_t*)citizen;
            // Check if citizen needs housing reassignment:
            // age (float at +0x3c) >= max age (from profession at +0x288 + 0x44)
            // and housing ptr at +0x208 == 0
            float age = *(float*)(cit + 0x3c);
            float maxAge = *(float*)(*(uintptr_t*)(cit + 0x288) + 0x44);
            void* housing = *(void**)(cit + 0x208);

            if (age >= maxAge && housing == nullptr) {
                // Get profession list town hall ptr
                void* profList = m_professionList;
                int64_t townHall = *(int64_t*)((uintptr_t)profList + 0x30);

                // Check if citizen's current building != townHall or housing != null
                int64_t citBuilding = *(int64_t*)(cit + 0x1f0);
                int64_t citHousingRef = *(int64_t*)(cit + 0x248);

                int64_t nullRef[1] = {0};
                new (nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

                if (citBuilding != townHall || citHousingRef != nullRef[0]) {
                    CitizenComp_AssignHousing(static_cast<CitizenComponent*>(citizen), nullRef);   // 0x140111ee0
                    *(int64_t*)(cit + 0x1f0) = townHall;
                }
            }
        }
    }
    TRACE("[GameUpdateLoop::Shutdown] citizen housing done\n");

    // [OBJAUDIT] Wave 9 lane N, Finding 3. The periodic scan above only runs from
    // Update, so the handle table has NEVER been checked during teardown — which is
    // the only place the intermittent allocator fault fires. Mode (A) in
    // Object/internal_factory_audit.h ("register without unregister": the next
    // ~Internal swap-removes a freed object and writes `*(int*)(freedPtr+0x08) = idx`,
    // a 4-byte store into a reused block) has exactly the signature of that fault —
    // a small integer landing on half of something live. Bracketing EntityList's
    // shutdown separates "the table was already corrupt when teardown started" from
    // "teardown corrupted it", which no run so far can tell apart.
    // EntityList shutdown
    TRACE("[GameUpdateLoop::Shutdown] calling EntityList::Shutdown...\n");
    EntityList_Shutdown_c3d30(m_entityList);   // 0x1400c3d30
    TRACE("[GameUpdateLoop::Shutdown] EntityList::Shutdown done\n");

    // Remove global scene objects
    TRACE("[GameUpdateLoop::Shutdown] removing global scene objects...\n");
    {
        void* gameWorld = GameUpdateLoop::_singleton;
        void* graphicsScene = static_cast<GameUpdateLoop*>(gameWorld)->m_graphicsScene;   // B.3.8 bridge
        Graphics::Scene* gs = reinterpret_cast<Graphics::Scene*>(graphicsScene);   // B.3.5 bridge
        gs->RemoveGlobalObject(m_worldGeometry);  // B.3.5 bridge
        gs->RemoveGlobalObject(m_sunLight);       // B.3.5 bridge
        gs->RemoveGlobalObject(m_skyMesh);        // B.3.5 bridge
    }

    // Delete scenario instance
    if (m_scenarioInstance != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown] deleting scenario instance...\n");
        reinterpret_cast< ::Object::InternalBasePtr*>(&m_scenarioInstance)->DeletePtr(
            );   // B.3.3 bridge
    }

    // Free/destroy all subsystems
    TRACE("[GameUpdateLoop::Shutdown] destroying subsystems...\n");

    // Random
    if (m_random != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing Random...\n");
        System::MemoryAllocator::MemFree(m_random);
    }

    // PlayerController
    if (m_playerController != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing PlayerController...\n");
        System::MemoryAllocator::MemFree(m_playerController);
    }

    // Renderer
    if (m_renderer != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Renderer...\n");
        RtGraphics_Renderer_Dtor(static_cast<RtGraphics::Renderer*>(m_renderer));
        System::MemoryAllocator::MemFree(m_renderer);
    }

    // EntityList
    if (m_entityList != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EntityList...\n");
        EntityList_Dtor_c3e60(m_entityList);   // 0x1400c3e60
        System::MemoryAllocator::MemFree(m_entityList);
    }

    // UserInterface
    if (m_userInterface != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying UserInterface...\n");
        // Call Runtime destructor
        RtUi_UserInterface_Dtor(m_userInterface);
        System::MemoryAllocator::MemFree(m_userInterface);
    }

    // CitizenBehavior
    if (m_citizenList != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying CitizenBehavior...\n");
        CitizenList_Dtor_907b0(m_citizenList);   // 0x1400907b0
        System::MemoryAllocator::MemFree(m_citizenList);
    }

    // Game
    if (m_game != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Game...\n");
        Game_Dtor_9ed60(reinterpret_cast<int64_t>(m_game));   // 0x14009ed60
        System::MemoryAllocator::MemFree(m_game);
    }

    // Tutorial
    if (m_tutorial != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Tutorial...\n");
        Tutorial_Dtor_f8ff0(m_tutorial);   // 0x1400f8ff0
        System::MemoryAllocator::MemFree(m_tutorial);
    }

    // EntityUI
    if (m_toolManager != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EntityUI...\n");
        EntityUI_Dtor_1024d0(m_toolManager);   // 0x1401024d0
        System::MemoryAllocator::MemFree(m_toolManager);
    }

    // DayCounter
    if (m_dayCounter != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing DayCounter...\n");
        System::MemoryAllocator::MemFree(m_dayCounter);
    }

    // Weather
    if (m_weather != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Weather...\n");
        Game_WeatherDtor(m_weather);   // 0x140093eb0
        System::MemoryAllocator::MemFree(m_weather);
    }

    // EventNotifier
    if (m_eventNotifier != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EventNotifier...\n");
        Game_EventNotifierCleanup((void*)((uintptr_t)m_eventNotifier + 0x18));   // 0x14000a8a0
        System::MemoryAllocator::MemFree(m_eventNotifier);
    }

    // Graphics::Scene
    if (m_graphicsScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Graphics::Scene...\n");
        RtGraphics_Scene_Dtor(static_cast<RtGraphics::Scene*>(m_graphicsScene));
        System::MemoryAllocator::MemFree(m_graphicsScene);
    }

    // Audio::Scene
    if (m_audioScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Audio::Scene...\n");
        RtAudio_Scene_Dtor(m_audioScene);
        System::MemoryAllocator::MemFree(m_audioScene);
    }

    // Picking::Scene
    if (m_pickingScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Picking::Scene...\n");
        static_cast< ::Picking::Scene*>(m_pickingScene)->::Picking::Scene::~Scene();
        System::MemoryAllocator::MemFree(m_pickingScene);
    }

    // WorldGeometry, SunLight, SkyMesh — via scalar deleting dtor (vtable[0] with flags=1)
    if (m_worldGeometry != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying WorldGeometry...\n");
        // Caller batch CB6: slot 0 of the real class (proved as in Update step 8).
        delete m_worldGeometry;   // B.3.8 bridge
    }
    if (m_sunLight != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SunLight...\n");
        delete m_sunLight;   // B.3.x bridge (slot 0, free)
    }
    if (m_skyMesh != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SkyMesh...\n");
        delete m_skyMesh;   // B.3.x bridge (slot 0, free)
    }

    // AnimationManager
    if (m_animationManager != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying AnimationManager...\n");
        RtGraphics_AnimationManager_Dtor(static_cast<RtGraphics::AnimationManager*>(m_animationManager));
        System::MemoryAllocator::MemFree(m_animationManager);
    }

    // CharacterGrid (Morton quadtree at +0x88)
    if (m_characterGrid != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying CharacterGrid...\n");
        Game_MusicPlayerDtor(m_characterGrid);   // 0x140018220
        System::MemoryAllocator::MemFree(m_characterGrid);
    }

    // SaveLoad
    if (m_saveLoad != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SaveLoad...\n");
        RtSave_SaveLoad_Dtor(m_saveLoad);
        System::MemoryAllocator::MemFree(m_saveLoad);
    }

    // ImageCapture
    if (m_imageCapture != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying ImageCapture...\n");
        Game_ImageCaptureDtor(m_imageCapture);   // 0x1400a4fc0
        System::MemoryAllocator::MemFree(m_imageCapture);
    }

    // MusicPlayer (0x18-byte POD; retail frees it with no dtor)
    if (m_musicPlayer != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing MusicPlayer...\n");
        System::MemoryAllocator::MemFree(m_musicPlayer);
    }

    TRACE("[GameUpdateLoop::Shutdown] EXIT\n");
}


// ============================================================
// CalcWindFactor (0x1401027c0) — really "camera distance to focus"
// ============================================================
// Feeds Audio::Scene::Update's last parameter. Named fn_CalcWindFactor here, but
// the disassembly is a plain distance:
//
//   RBX  = [g_pGameWorldSingleton + 0xE0]        playerController
//   copy Camera (playerCtrl+0x34) to a stack temp via slot 0x140152940
//   diff = camera.position (copy+0x88..0x90) - [RBX+0x1c/0x20/0x24]   focus/anchor
//   Vector3::Dot(diff, diff) -> XMM0 ; JMP 0x140132010 (sqrtf)
//
// So: |cameraPos - focusPos|, returned as a FLOAT in XMM0. The Time* argument is
// spilled at entry and never read — keep it unused; making it drive a real
// oscillation would invent behaviour retail never had.
//
// DEVIATION: retail copy-constructs the whole Camera onto the stack and reads the
// copy's +0x88. We read playerCtrl+0x34+0x88 directly. The position is a copied
// field, not one the copy ctor could synthesise, so the values are identical — and
// this avoids taking a dependency on dispatch slot 0x140152940, which nothing else
// in the tree registers.
static float CalcWindFactor_Impl(void* /*time — dead in retail*/) {
    void* gw = GameUpdateLoop::_singleton;
    if (!gw) return 0.0f;
    uintptr_t playerCtrl = reinterpret_cast<uintptr_t>(
        static_cast<GameUpdateLoop*>(gw)->m_playerController);   // B.3.8 bridge
    if (!playerCtrl) return 0.0f;

    const float* camPos = (const float*)(playerCtrl + 0x34 + 0x88);
    float dx = camPos[0] - *(float*)(playerCtrl + 0x1c);
    float dy = camPos[1] - *(float*)(playerCtrl + 0x20);
    float dz = camPos[2] - *(float*)(playerCtrl + 0x24);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

extern "C" float CalcWindFactor_Reg(void* time) { return CalcWindFactor_Impl(time); }

// ============================================================
// GameUpdateLoop::OnScenarioComplete (0x140096830)
// ============================================================
// Called when the scenario achievement is met or has failed.
// Sets m_scenarioCompleted flag and fires an event notification.

void GameUpdateLoop::OnScenarioComplete() {

    TRACE("[GameUpdateLoop::OnScenarioComplete] ENTER (self=%p)\n", (void*)this);

    // Set completed flag
    m_scenarioCompleted = 1;

    // Check if the achievement has a parent Achievement object
    void* achievementInstance = m_scenarioInstance;
    void* achievement = *(void**)((uintptr_t)achievementInstance + 0x20);

    if (achievement == nullptr) {
        // No parent achievement — simple notification
        // Event type: 0x1d (win) or 0x1c (fail) depending on IsMet
        TRACE("[GameUpdateLoop::OnScenarioComplete] no parent achievement, firing simple notification...\n");
        int64_t nullEntity[1] = {0};
        new (nullEntity) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        bool isMet = m_scenarioInstance->IsMet();   // B.3.7 bridge

        // 0x1c = scenario won (28), 0x1d = scenario failed (29). IsMet=1 -> 0x1c.
        // (The IDs were documented inverted here; the live string table settles it —
        // the named sibling 0x1b reads "You've failed to meet a goal in the scenario".)
        int eventType = 0x1d - (int)isMet;
        EventLog_AddEvent(m_eventNotifier, eventType, 4, nullEntity);   // 0x140095580
    } else {
        // Has parent achievement — notification with achievement title
        TRACE("[GameUpdateLoop::OnScenarioComplete] has parent achievement, firing named notification...\n");

        // Get achievement title (non-trivial return: this, retval)
        System::String titleStr;
        // B.3.7 bridge: GetTitle returns System::String BY VALUE; constructed
        // straight into titleStr's storage (one AddRef, as the door did).
        ::new (&titleStr) System::String(
            reinterpret_cast<const Achievement::Achievement*>(achievement)->GetTitle());   // B.3.7 bridge

        // Build UI::String from achievement name (B.3.6: the old `uint8_t uiStr[24]`
        // scratch, whose only uses were this ctor and the conversion below).
        UI::String uiStr(
            Object::ExternalPtr<UI::StringTable>(  // B.3.6 bridge
                reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)achievement + 0x20))),
            titleStr);

        // Retail destructs the title String here — it reuses that same stack slot
        // for the UI::String -> System::String conversion below.
        titleStr = System::String();

        // Convert to System::String (member, non-trivial return: this, retval)
        System::String sysStr(uiStr.operator System::String());

        int64_t nullEntity[1] = {0};
        new (nullEntity) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        bool isMet = m_scenarioInstance->IsMet();   // B.3.7 bridge

        // 0x1a = scenario won with name (26), 0x1b = scenario failed with name (27)
        int eventType = 0x1b - (int)isMet;
        EventLog_AddEventWithName(m_eventNotifier, eventType, 4, nullEntity, &sysStr);   // 0x140095750
    }

    TRACE("[GameUpdateLoop::OnScenarioComplete] EXIT\n");
}


// ============================================================
// GameUpdateLoop::Destroy (0x140019DD0) — Scalar deleting destructor
// ============================================================
// Ghidra decompile:
//   1. Set vtable to GameUpdateLoop vtable
//   2. Clear g_pGameWorldSingleton to NULL
//   3. Destroy System::String at +0x28 (m_saveName)
//   4. Call UpdateLoop::~UpdateLoop (base dtor in Runtime)
//   5. If flags & 1, free memory

// ENDGAME B.3.8 LOOP: step 2 is the destructor's body; steps 3 and 4 (~String
// on m_saveName, ~UpdateLoop) are the compiler's member and base chain, in that
// order (step 1, the restamp, is the compiler's too). The old slot reached the
// base through the `??1UpdateLoop@Engine@@` door, which shares its body with
// the real ~UpdateLoop (RB). Step 5, the free, is UpdateLoop's operator delete
// (the virtual destructor's D0); nothing may touch the object after it, so the
// old EXIT trace is gone.
GameUpdateLoop::~GameUpdateLoop() {

    TRACE("[GameUpdateLoop::Destroy] ENTER (self=%p)\n", (void*)this);

    // Step 2: Clear game world singleton
    _singleton = nullptr;
}

// The exe cell 0x1401c3dd8 (B3R STORAGE; was the file-static
// g_storage_GameWorldSingleton of standalone/address_registry.cpp, read through
// g_pGameWorldSingleton / g_pGameSingleton).
GameUpdateLoop* GameUpdateLoop::_singleton = nullptr;


// ============================================================
// GameUpdateLoop::GameUpdateLoop (0x140098060)
// ============================================================
// Retail's ctor: `??0UpdateLoop@Engine@@` (the init list), the 0x14015e2f0
// stamp (the compiler's), then every field below — the same stores the three
// door-then-stamp construction sites outside this block write by hand
// (newgamedialog.cpp, saveloaddialog.cpp, tutorial_next_scenario.cpp).
GameUpdateLoop::GameUpdateLoop(void* savedState, uint64_t scenarioIbp, int32_t scenarioIndex,
                               const System::String& saveName, int32_t seed,
                               int32_t mapSize, bool disasters, void* mapConfig,
                               void* terrainConfig, void* climateConfig)
    : Engine::UpdateLoop(), m_saveName(saveName) {
    m_seed          = seed;
    m_mapSize       = mapSize;
    m_gameTime      = 0;
    m_mapConfig     = mapConfig;
    m_terrainConfig = terrainConfig;
    m_climateConfig = climateConfig;
    m_savedState    = savedState;
    // +0x60..+0xB8, in field order (retail's qword stores).
    m_graphicsScene = m_animationManager = m_pickingScene = m_audioScene = nullptr;
    m_musicPlayer = m_characterGrid = m_renderer = nullptr;
    m_userInterface  = nullptr;
    m_helpTopics     = nullptr;
    m_diseaseList    = nullptr;
    m_professionList = nullptr;
    m_uiMaterial     = nullptr;
    m_quickSavePressed = 0;
    m_quickLoadPressed = 0;
    m_nextLoop      = nullptr;
    m_gameFlags     = disasters ? 0u : 0xF0u;   // retail: p8 ? 0 : 0xF0
    // +0xD8..+0x148, in field order.
    m_random = nullptr;
    m_playerController = nullptr;
    m_entityList = m_citizenList = m_game = nullptr;
    m_sunLight      = nullptr;
    m_skyMesh       = nullptr;
    m_worldGeometry = nullptr;
    m_tutorial    = nullptr;
    m_toolManager = nullptr;
    m_dayCounter = m_weather = nullptr;
    m_eventNotifier = nullptr;
    m_saveLoad      = nullptr;
    m_imageCapture  = nullptr;
    m_scenarioInstance  = reinterpret_cast<Achievement::AchievementInstance*>(scenarioIbp);   // *param_3
    m_scenarioCompleted = 0;
    m_scenarioIndex     = scenarioIndex;
}

// ============================================================
// The small slots (were standalone/gameupdateloop_stubs.cpp)
// ============================================================

// [7] 0x1400959c0 — serializes loop state to/from a Save::SavedState. The
// body is SaveRestoreOut (gameupdateloop_serialize.cpp) because it takes
// retail's 4th argument, which only GameUpdateLoop::Load passes (directly, with
// a real pointer). Every virtual caller — Save::SaveLoad::UpdateSave and the
// ModDialog reload — hands a dead R9 that the save path never reads; nullptr
// here is the value the old table's slot received from UpdateSave.
void GameUpdateLoop::SaveRestore(bool saving, Save::SavedState* state) {
    SaveRestoreOut(saving, state, nullptr);
}

// [8] 0x140095970 — PORT GAP (retail releases the loop's GPU buffers on a D3D
// device-lost event) — B38 D3.2: the tree's no-op body, kept. Never fires on
// the load path or a normal frame.
void GameUpdateLoop::OnVideoError() {}

// [9] 0x140095930 — PORT GAP (retail recreates them after device-restored);
// pairs with OnVideoError. The tree's no-op body, kept.
void GameUpdateLoop::OnVideoRestart() {}

// [10] 0x140019d40 — LOAD-BEARING: returns the static string "Game/Game".
// The engine's UpdateLoop::LoadResources (Runtime 0x1800dd7d0, LOADER THREAD)
// calls this slot and, if it returns non-null, FindOrLoads "<ret>Resources.rsc"
// + its whole recursive dependency tree (the HUD/dialog/toolbar resources).
// The preload graduated to always-on 2026-09-13. The once-only log flag is a
// namespace-scope atomic (B6-5: this runs on the loader thread).
namespace { std::atomic<bool> s_gulResourceLogged{false}; }
const char* GameUpdateLoop::GetResourceFilename() {
    if (!s_gulResourceLogged.exchange(true)) {
        printf("[GUL::GetResourceFilename] preload ON -> returning \"Game/Game\" "
               "(engine will recursively preload the HUD/dialog/toolbar resource tree)\n");
        fflush(stdout);
    }
    return "Game/Game";
}

// B38_RULES D5: `Vtable()` is the COMPILER's table (the splash's "is the next
// loop a GameUpdateLoop" identity test reads it). ENDGAME B.3.9 EXA: the
// 0x14015e2f0 registration went with the last door-then-stamp builder.
DEFINE_EXE_VTABLE(GameUpdateLoop, "_ZTV14GameUpdateLoop")
