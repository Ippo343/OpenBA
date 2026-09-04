// standalone/stress_harness.cpp
// BANISHED_STRESS — large-scale stress test + performance baseline. See stress_harness.h
// for the knob list. Three responsibilities live here:
//
//   1. ENV BUNDLE (StressHarness_Init, from WinMain before anything reads env): turn the
//      single BANISHED_STRESS=1 into the autostart/medium-map/uncapped/pinned-camera
//      configuration a baseline needs, without overriding anything the user set.
//      The WORLD content (buildings, citizens, food) is built by starttown.cpp, which
//      asks this module for its targets — see StartTown_StressSpawnStructures and the
//      stress-family loop in StartTown_SpawnCitizens.
//
//   2. METRICS: wall-clock brackets around GameUpdateLoop::Update and ::Draw, the
//      SIMPROF phase split (taken from gameupdateloop.cpp's accumulators), a population/
//      employment/entity census, process memory, and a per-component-type Update profile
//      fed from EntityList_Update's dispatch loop. One CSV row per BANISHED_STRESS_INTERVAL
//      frames to standalone_stress.csv; the component profile goes to
//      standalone_stress_compprof.txt. tools/stress_report.py turns the CSV into a
//      summary and a baseline diff.
//
//   3. STAFFING: a periodic pass that seats workers at every workplace up to its authored
//      _defaultWorkers through the REAL seat cascade (CitizenEmploy_SeatWorkers, the same
//      cbf0->ca20->c5f0->c4b0 chain a spinner click runs). Nothing is assigned by hand;
//      after the seat everything is organic. A workplace that is not yet accepting
//      (state-13 enter for the ResourceProduce family) is simply retried next pass.
//
// Everything here is fprintf(stderr)-based so it survives the release log strip.
#include "stress_harness.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName / GetTypeNameById
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PSAPI_VERSION 2
#include <psapi.h>

extern "C" int CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count);
// gameupdateloop.cpp — SIMPROF phase accumulators reserved for this harness.
extern "C" int         GameUpdateLoop_SimProfTake(double* outMs, int maxN);
extern "C" const char* GameUpdateLoop_SimProfName(int phase);

int g_compProfOn = 0;

namespace {

// ── configuration ────────────────────────────────────────────────────────────
int   s_enabled   = -1;      // -1 unresolved
int   s_citizens  = 800;
int   s_sets      = 3;
int   s_frames    = 6000;
int   s_interval  = 600;
int   s_speed     = 10;
int   s_foodMult  = 0;       // 0 = derive from citizens
int   s_staff     = 1;
char  s_csvPath[MAX_PATH] = "standalone_stress.csv";

bool EnvTrue(const char* n) {
    const char* v = std::getenv(n);
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}
int EnvInt(const char* n, int dflt) {
    const char* v = std::getenv(n);
    return (v && v[0]) ? std::atoi(v) : dflt;
}
void SetIfUnset(const char* n, const char* v) {
    const char* cur = std::getenv(n);
    if (cur && cur[0]) return;
    _putenv_s(n, v);
}

void Resolve() {
    if (s_enabled >= 0) return;
    s_enabled  = EnvTrue("BANISHED_STRESS") ? 1 : 0;
    if (!s_enabled) return;
    s_citizens = EnvInt("BANISHED_STRESS_CITIZENS", s_citizens);
    s_sets     = EnvInt("BANISHED_STRESS_SETS", s_sets);
    s_frames   = EnvInt("BANISHED_STRESS_FRAMES", s_frames);
    s_interval = EnvInt("BANISHED_STRESS_INTERVAL", s_interval);
    s_speed    = EnvInt("BANISHED_STRESS_SPEED", s_speed);
    s_foodMult = EnvInt("BANISHED_STRESS_FOOD_MULT", 0);
    s_staff    = EnvInt("BANISHED_STRESS_STAFF", 1);
    g_compProfOn = EnvInt("BANISHED_STRESS_COMPPROF", 1) ? 1 : 0;
    if (const char* p = std::getenv("BANISHED_STRESS_CSV")) if (p[0]) {
        std::strncpy(s_csvPath, p, sizeof(s_csvPath) - 1);
        s_csvPath[sizeof(s_csvPath) - 1] = 0;
    }
    if (s_citizens < 0) s_citizens = 0;
    if (s_sets < 0) s_sets = 0;
    if (s_interval < 60) s_interval = 60;
    if (s_speed < 1) s_speed = 1;
    if (s_foodMult <= 0) s_foodMult = s_citizens > 10 ? s_citizens / 10 : 1;
}

// ── timing ───────────────────────────────────────────────────────────────────
long long QpcNow() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
double QpcMs(long long ticks) {
    static long long freq = 0;
    if (!freq) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = f.QuadPart; }
    return freq ? 1000.0 * (double)ticks / (double)freq : 0.0;
}

long long s_runT0        = 0;     // first Update of the first stress world
long long s_updT0        = 0;
long long s_drawT0       = 0;
long long s_lastDrawEntry = 0;
// Window accumulators (reset every CSV row).
double s_updMsSum = 0, s_drawMsSum = 0;
int    s_updN = 0, s_drawN = 0;
long long s_simIters = 0;
std::vector<float> s_updSamples;   // per-Update ms, for p95
std::vector<float> s_frameSamples; // Draw entry-to-entry ms, for p95 / fps
long long s_lastRow = 0;           // QPC at the previous row (wall fps)

// World-relative frame counter: s_frameNum is a process counter.
void* s_world       = nullptr;
int   s_worldSeenAt = 0;
int   s_worldFrame  = 0;
int   s_worldNo     = 0;

FILE* s_csv = nullptr;
FILE* s_compOut = nullptr;

bool Readable(const void* p) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    return a >= 0x10000 && a < 0x00007FFFFFFFFFFFULL;
}

float Pct(std::vector<float>& v, double p) {
    if (v.empty()) return 0.0f;
    size_t k = (size_t)((v.size() - 1) * p);
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

// ── census ───────────────────────────────────────────────────────────────────
struct Census {
    int citizens = 0, home238 = 0, work248 = 0, busy = 0, entities = -1;
    double ageMean = 0, hungerMean = 0;
};

Census TakeCensus(void* gw) {
    Census c;
    if (!Readable(gw)) return c;
    const uintptr_t g = reinterpret_cast<uintptr_t>(gw);
    const uintptr_t el = *reinterpret_cast<uintptr_t*>(g + 0xE8);
    if (Readable(reinterpret_cast<void*>(el))) {
        int n = *reinterpret_cast<int*>(el + 0x10);
        if (n >= 0 && n < 10000000) c.entities = n;
    }
    const uintptr_t town = *reinterpret_cast<uintptr_t*>(g + 0xF0);
    if (!Readable(reinterpret_cast<void*>(town))) return c;
    uint8_t* arr = *reinterpret_cast<uint8_t**>(town + 0xA0);
    const int n = *reinterpret_cast<int*>(town + 0xA8);
    if (!arr || n <= 0 || n > 100000) return c;
    double age = 0, hunger = 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t cz = *reinterpret_cast<uintptr_t*>(arr + (size_t)i * 8);
        if (!Readable(reinterpret_cast<void*>(cz))) continue;
        ++c.citizens;
        age    += *reinterpret_cast<float*>(cz + 0x3C);
        hunger += *reinterpret_cast<float*>(cz + 0x44);
        if (*reinterpret_cast<uintptr_t*>(cz + 0x238)) ++c.home238;
        if (*reinterpret_cast<uintptr_t*>(cz + 0x248)) ++c.work248;
        const uintptr_t ch = *reinterpret_cast<uintptr_t*>(cz + 0x278);
        if (Readable(reinterpret_cast<void*>(ch)) && *reinterpret_cast<int32_t*>(ch + 0x60) >= 0)
            ++c.busy;
    }
    if (c.citizens) { c.ageMean = age / c.citizens; c.hungerMean = hunger / c.citizens; }
    return c;
}

void MemoryMB(double* ws, double* priv) {
    PROCESS_MEMORY_COUNTERS_EX pm; std::memset(&pm, 0, sizeof(pm)); pm.cb = sizeof(pm);
    *ws = *priv = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm), sizeof(pm))) {
        *ws   = (double)pm.WorkingSetSize / (1024.0 * 1024.0);
        *priv = (double)pm.PrivateUsage   / (1024.0 * 1024.0);
    }
}

// ── per-component-type profile ───────────────────────────────────────────────
struct CompSlot { void* vtbl; uint32_t typeId; long long ticks; long long calls; };
constexpr int kCompSlots = 1024;          // power of two; ~200 component types exist
CompSlot s_comp[kCompSlots];

void CompProfReport(int frame) {
    struct Row { const CompSlot* s; };
    std::vector<const CompSlot*> rows;
    long long total = 0;
    for (int i = 0; i < kCompSlots; ++i) if (s_comp[i].vtbl && s_comp[i].calls) {
        rows.push_back(&s_comp[i]); total += s_comp[i].ticks;
    }
    if (rows.empty()) return;
    std::sort(rows.begin(), rows.end(),
              [](const CompSlot* a, const CompSlot* b) { return a->ticks > b->ticks; });
    if (!s_compOut) s_compOut = std::fopen("standalone_stress_compprof.txt", "w");
    FILE* outs[2] = { stderr, s_compOut };
    for (FILE* o : outs) {
        if (!o) continue;
        std::fprintf(o, "[COMPPROF] world#%d frame=%d window=%d frames  component Update total %.2f ms/frame\n",
                     s_worldNo, frame, s_interval, QpcMs(total) / s_interval);
        int shown = 0;
        for (const CompSlot* s : rows) {
            if (++shown > 30) break;
            const char* name = ::Object::GetTypeNameById(s->typeId);
            std::fprintf(o, "[COMPPROF]   %-34s %8.3f ms/frame %6.2f%%  calls/frame=%8.1f  %.4f us/call\n",
                         name && name[0] ? name : "?", QpcMs(s->ticks) / s_interval,
                         total ? 100.0 * (double)s->ticks / (double)total : 0.0,
                         (double)s->calls / s_interval,
                         1000.0 * QpcMs(s->ticks) / (double)s->calls);
        }
        std::fflush(o);
    }
    for (int i = 0; i < kCompSlots; ++i) { s_comp[i].ticks = 0; s_comp[i].calls = 0; }
}

// ── staffing pass ────────────────────────────────────────────────────────────
constexpr int kStaffFirst  = 600;
constexpr int kStaffRetry  = 300;
constexpr int kStaffDense  = 6000;   // retry every kStaffRetry until here, then every 1800
int s_staffPasses = 0;

void StaffPass(void* gw, int worldFrame) {
    const uintptr_t g = reinterpret_cast<uintptr_t>(gw);
    const uintptr_t townMgr    = *reinterpret_cast<uintptr_t*>(g + 0xF0);
    const uintptr_t entityList = *reinterpret_cast<uintptr_t*>(g + 0xE8);
    if (!townMgr || !entityList) return;
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    const int n = *reinterpret_cast<int32_t*>(entityList + 0x10);
    if (!arr || n <= 0) return;

    // 0x140002e10 = FindComponent_WorkPlace, 0x140003050 = FindComponent_Work (both
    // Ghidra-misnamed; see reference_misnamed_component_finders).
    typedef void* (*fn_Find_t)(int64_t entity, void* outIBP);
    static fn_Find_t findWPC = nullptr, findWork = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        findWPC  = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002e10));
        findWork = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    }
    if (!findWPC || !findWork) {
        std::fprintf(stderr, "[Stress::staff] component finders unresolved (wpc=%p work=%p) — no staffing\n",
                     (void*)findWPC, (void*)findWork);
        return;
    }

    struct Need { uint64_t prof; int want; int have; int buildings; };
    std::vector<Need> needs;
    int workplaces = 0, notAccepting = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent) continue;
        if (*reinterpret_cast<uint32_t*>((uint8_t*)ent + 0x40) & 0x400) continue;   // removed
        uint64_t wpc = 0;
        findWPC(reinterpret_cast<int64_t>(ent), &wpc);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpc);
        if (!wpc) continue;
        const uint64_t prof = *reinterpret_cast<uint64_t*>(wpc + 0x48);
        if (!prof) continue;
        ++workplaces;
        if (*reinterpret_cast<uint8_t*>(wpc + 0x50) == 0) { ++notAccepting; continue; }
        const int have = (int)*reinterpret_cast<uint32_t*>(wpc + 0x60);   // live roster
        int want = 1;
        uint64_t wc = 0;
        findWork(reinterpret_cast<int64_t>(ent), &wc);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wc);
        if (wc) {
            const uintptr_t wd = *reinterpret_cast<uintptr_t*>(wc + 0x18);   // WorkDescription
            if (wd) {
                const int dflt = *reinterpret_cast<int32_t*>(wd + 0x24);
                const int mx   = *reinterpret_cast<int32_t*>(wd + 0x2c);
                want = dflt > 0 ? dflt : 1;
                if (mx > 0 && want > mx) want = mx;
            }
            // Effective capacity as the panel enforces it (workComp+0x30), when computed.
            const int cap = *reinterpret_cast<int32_t*>(wc + 0x30);
            if (cap > 0 && want > cap) want = cap;
        }
        Need* nd = nullptr;
        for (Need& x : needs) if (x.prof == prof) { nd = &x; break; }
        if (!nd) { needs.push_back({prof, 0, 0, 0}); nd = &needs.back(); }
        nd->want += want; nd->have += have; ++nd->buildings;
    }

    int seatedTotal = 0, wanted = 0;
    for (Need& x : needs) {
        const int missing = x.want - x.have;
        if (missing <= 0) continue;
        wanted += missing;
        const int got = CitizenEmploy_SeatWorkers((int64_t)townMgr, x.prof, missing);
        seatedTotal += got;
        if (got || s_staffPasses == 0)
            std::fprintf(stderr, "[Stress::staff]   prof=0x%llx buildings=%d want=%d have=%d -> seated %d/%d\n",
                         (unsigned long long)x.prof, x.buildings, x.want, x.have, got, missing);
    }
    std::fprintf(stderr, "[Stress::staff] pass#%d worldFrame=%d workplaces=%d (notAccepting=%d) "
                         "professions=%zu missing=%d seated=%d\n",
                 s_staffPasses, worldFrame, workplaces, notAccepting, needs.size(), wanted, seatedTotal);
    std::fflush(stderr);
    ++s_staffPasses;
}

// ── CSV row ──────────────────────────────────────────────────────────────────
void OpenCsv() {
    if (s_csv) return;
    s_csv = std::fopen(s_csvPath, "w");
    if (!s_csv) { std::fprintf(stderr, "[Stress] cannot open %s\n", s_csvPath); return; }
    SYSTEMTIME st; GetLocalTime(&st);
    std::fprintf(s_csv, "# banished stress run %04d-%02d-%02d %02d:%02d:%02d build=%s %s\n",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, __DATE__, __TIME__);
    std::fprintf(s_csv, "# citizens=%d sets=%d speed=%d frames=%d interval=%d seed=%s map=%s opt=%s\n",
                 s_citizens, s_sets, s_speed, s_frames, s_interval,
                 std::getenv("BANISHED_SEED") ? std::getenv("BANISHED_SEED") : "?",
                 std::getenv("BANISHED_MAP_SIZE") ? std::getenv("BANISHED_MAP_SIZE") : "?",
#ifdef __OPTIMIZE__
                 "on"
#else
                 "off"
#endif
    );
    std::fprintf(s_csv, "wall_s,world,frame,speed,sim_iters,fps,frame_ms_mean,frame_ms_p95,"
                        "update_ms_mean,update_ms_p95,update_ms_per_iter,draw_ms_mean,"
                        "ph_paircache_ms,ph_animation_ms,ph_entitylist_ms,ph_weather_ms,"
                        "ph_citizenlist_ms,ph_postsim_ms,"
                        "entities,citizens,home238,work248,busy,age_mean,hunger_mean,ws_mb,private_mb\n");
    std::fflush(s_csv);
}

void EmitRow(void* gul, int frame) {
    OpenCsv();
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    const long long now = QpcNow();
    const double wall = QpcMs(now - s_runT0) / 1000.0;
    const double windowS = QpcMs(now - (s_lastRow ? s_lastRow : s_runT0)) / 1000.0;
    s_lastRow = now;

    int speed = 0;
    {
        void* eui = *reinterpret_cast<void**>((uint8_t*)gul + 0x120);
        if (eui && *(char*)((uint8_t*)eui + 0x30) == 0) speed = *(int32_t*)((uint8_t*)eui + 0x34);
    }
    const double fps = windowS > 0 ? s_drawN / windowS : 0.0;
    const double updMean = s_updN ? s_updMsSum / s_updN : 0.0;
    const double drawMean = s_drawN ? s_drawMsSum / s_drawN : 0.0;
    const double perIter = s_simIters ? s_updMsSum / (double)s_simIters : 0.0;
    double frameMean = 0;
    for (float f : s_frameSamples) frameMean += f;
    if (!s_frameSamples.empty()) frameMean /= s_frameSamples.size();
    const float updP95 = Pct(s_updSamples, 0.95), frameP95 = Pct(s_frameSamples, 0.95);

    double ph[8] = {0};
    const int phN = GameUpdateLoop_SimProfTake(ph, 8);   // ms totals since last take
    // phases: 0 PairCache 1 Animation 2 EntityList 3 Weather 4 CitizenList 5 PostSim 6 Total
    for (int i = 0; i < phN; ++i) ph[i] /= (s_updN ? s_updN : 1);

    Census c = TakeCensus(gw);
    double ws = 0, priv = 0; MemoryMB(&ws, &priv);

    if (s_csv) {
        std::fprintf(s_csv, "%.1f,%d,%d,%d,%lld,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                            "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                            "%d,%d,%d,%d,%d,%.2f,%.1f,%.1f,%.1f\n",
                     wall, s_worldNo, s_worldFrame, speed, s_simIters, fps, frameMean, frameP95,
                     updMean, updP95, perIter, drawMean,
                     ph[0], ph[1], ph[2], ph[3], ph[4], ph[5],
                     c.entities, c.citizens, c.home238, c.work248, c.busy, c.ageMean, c.hungerMean,
                     ws, priv);
        std::fflush(s_csv);
    }
    std::fprintf(stderr, "[Stress] t=%.0fs world#%d frame=%d speed=%d fps=%.1f frame=%.2fms(p95 %.2f) "
                         "update=%.2fms(p95 %.2f, %.3f/iter) draw=%.2fms | entities=%d citizens=%d "
                         "home=%d work=%d busy=%d hunger=%.0f | ws=%.0fMB priv=%.0fMB\n",
                 wall, s_worldNo, s_worldFrame, speed, fps, frameMean, frameP95,
                 updMean, updP95, perIter, drawMean, c.entities, c.citizens, c.home238, c.work248,
                 c.busy, c.hungerMean, ws, priv);
    std::fflush(stderr);
    (void)frame;

    if (g_compProfOn) CompProfReport(s_worldFrame);

    s_updMsSum = s_drawMsSum = 0; s_updN = s_drawN = 0; s_simIters = 0;
    s_updSamples.clear(); s_frameSamples.clear();
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
extern "C" void StressHarness_Init() {
    Resolve();
    if (!s_enabled) return;
    char buf[64];
    SetIfUnset("BANISHED_AUTOSTART", "1");
    SetIfUnset("BANISHED_MAP_SIZE", "medium");
    SetIfUnset("BANISHED_SEED", "12345");
    SetIfUnset("BANISHED_DIFFICULTY", "easy");
    SetIfUnset("BANISHED_CLIMATE", "fair");
    SetIfUnset("BANISHED_FPS_CAP", "off");
    std::snprintf(buf, sizeof(buf), "%d", s_speed);    SetIfUnset("BANISHED_FORCE_SPEED", buf);
    std::snprintf(buf, sizeof(buf), "%d", s_interval); SetIfUnset("BANISHED_SIMPROF", buf);
    if (s_frames > 0) {
        std::snprintf(buf, sizeof(buf), "%d", s_frames);
        SetIfUnset("BANISHED_PAUSEMENU", buf);
        SetIfUnset("BANISHED_PAUSEMENU_QUIT", "60");
        SetIfUnset("BANISHED_PAUSEMENU_BUTTON", "6");       // Quit Game -> clean exit
    }
    SetIfUnset("BANISHED_CAMERA_PIN", "town,90");
    // The buildings starttown.cpp can only site through their own dedicated blocks
    // (water adjacency, drag areas, animal-density siting). One of each.
    SetIfUnset("BANISHED_BUILD_DEMO_FISHINGDOCK", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_TRADINGPOST", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_CROPFIELD", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_ORCHARD", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_PASTURE", "1");
    SetIfUnset("BANISHED_PASTURE_AUTOTYPE", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_HUNTERLODGE", "1");
    SetIfUnset("BANISHED_BUILD_DEMO_GATHERERHUT", "1");
    // A field with no crop and an orchard with no tree type never open for workers
    // (accepting stays 0, so the seat cascade skips them). In retail the player picks
    // those in the panel; these two harnesses make the same choice for EVERY field and
    // orchard in the world a few frames in (cropfield_force_start.cpp mode 2 also posts
    // the plant command so the field does not wait for the seasonal window).
    SetIfUnset("BANISHED_CROPFIELD_FORCE_START", "2");
    SetIfUnset("BANISHED_ORCHARD_FORCE_START", "1");
    std::fprintf(stderr, "[Stress] ENABLED citizens=%d sets=%d speed=%d frames=%d interval=%d "
                         "foodMult=%d staff=%d compprof=%d csv=%s\n",
                 s_citizens, s_sets, s_speed, s_frames, s_interval, s_foodMult, s_staff,
                 g_compProfOn, s_csvPath);
    std::fflush(stderr);
}

extern "C" bool StressHarness_Enabled()        { Resolve(); return s_enabled == 1; }
extern "C" int  StressHarness_TargetCitizens() { Resolve(); return s_enabled == 1 ? s_citizens : 0; }
extern "C" int  StressHarness_Sets()           { Resolve(); return s_enabled == 1 ? s_sets : 0; }
extern "C" int  StressHarness_FoodMult()       { Resolve(); return s_enabled == 1 ? s_foodMult : 1; }

extern "C" void StressHarness_UpdateBegin(int frameNum) {
    if (!StressHarness_Enabled()) return;
    (void)frameNum;
    s_updT0 = QpcNow();
    if (!s_runT0) s_runT0 = s_updT0;
}

extern "C" void StressHarness_UpdateEnd(void* gul, int frameNum) {
    if (!StressHarness_Enabled() || !s_updT0) return;
    const long long now = QpcNow();
    const double ms = QpcMs(now - s_updT0);
    s_updMsSum += ms; ++s_updN;
    if (s_updSamples.size() < 8192) s_updSamples.push_back((float)ms);

    void* eui = *reinterpret_cast<void**>((uint8_t*)gul + 0x120);
    if (eui && *(char*)((uint8_t*)eui + 0x30) == 0) {
        int spd = *(int32_t*)((uint8_t*)eui + 0x34);
        if (spd > 0) s_simIters += spd;
    }

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (gw != s_world) {
        s_world = gw; s_worldSeenAt = frameNum; ++s_worldNo;
        s_runT0 = now; s_lastRow = 0; s_staffPasses = 0;
        s_updMsSum = s_drawMsSum = 0; s_updN = s_drawN = 0; s_simIters = 0;
        s_updSamples.clear(); s_frameSamples.clear();
        std::fprintf(stderr, "[Stress] world#%d %p first ticked at process frame %d\n",
                     s_worldNo, gw, frameNum);
    }
    s_worldFrame = frameNum - s_worldSeenAt + 1;

    if (s_worldFrame % s_interval == 0) EmitRow(gul, frameNum);

    if (s_staff && gw && s_worldFrame >= kStaffFirst) {
        const int since = s_worldFrame - kStaffFirst;
        const int period = s_worldFrame < kStaffDense ? kStaffRetry : 1800;
        if (since % period == 0) StaffPass(gw, s_worldFrame);
    }
}

extern "C" void StressHarness_DrawBegin() {
    if (!StressHarness_Enabled()) return;
    const long long now = QpcNow();
    if (s_lastDrawEntry) {
        const double period = QpcMs(now - s_lastDrawEntry);
        if (s_frameSamples.size() < 8192) s_frameSamples.push_back((float)period);
    }
    s_lastDrawEntry = now;
    s_drawT0 = now;
}

extern "C" void StressHarness_DrawEnd() {
    if (!StressHarness_Enabled() || !s_drawT0) return;
    s_drawMsSum += QpcMs(QpcNow() - s_drawT0); ++s_drawN;
}

extern "C" int CompProf_Begin(void* comp) {
    void* vtbl = *reinterpret_cast<void**>(comp);
    uintptr_t h = (reinterpret_cast<uintptr_t>(vtbl) >> 4) & (kCompSlots - 1);
    for (int probe = 0; probe < kCompSlots; ++probe) {
        const int idx = (int)((h + probe) & (kCompSlots - 1));
        CompSlot& s = s_comp[idx];
        if (s.vtbl == vtbl) return idx;
        if (!s.vtbl) {
            s.vtbl = vtbl;
            typedef uint32_t (*GetTypeId_t)(void*);
            void** vt = reinterpret_cast<void**>(vtbl);
            s.typeId = vt[2] ? reinterpret_cast<GetTypeId_t>(vt[2])(comp) : 0;
            s.ticks = 0; s.calls = 0;
            return idx;
        }
    }
    return -1;
}

extern "C" void CompProf_End(int slot, long long qpcTicks) {
    if (slot < 0) return;
    s_comp[slot].ticks += qpcTicks;
    ++s_comp[slot].calls;
}
