// code/Game/Component/citizen_updatehealth.cpp
// CitizenComponent::UpdateHealth (0x1401127d0) + the complete disease subsystem it
// closes over. This was the last unported branch of the step-2.5 citizen lifecycle
// backlog ("deep closure" defer) — the closure turns out to be BOUNDED: 7 unported
// callees, everything below them already registered.
//
// Ported functions (all disasm-verified 2026-07-19):
//   0x1401127d0 UpdateHealth        — healthy: immunity tick + catch-disease roll;
//                                     sick: duration/spread-tick timers, recover-or-die
//   0x1401123b0 RecoverOrDie        — death roll (1/disease+0x40)/herbMult vs rand;
//                                     survive → clear disease + immunity from desc+0x9c
//   0x140112470 SpreadDisease       — contagion roll (disease+0x34), radius query
//                                     (disease+0x38) for citizens, infect 1st healthy
//   0x140112620 CatchDisease        — weighted pick (weight disease+0x3c) over the
//                                     disease ExternalList (gw+0xa8), then infect
//   0x140116b90 Infect              — guards, event-log post (type 0xb sev 3 + disease
//                                     name), set +0x68/+0x70/+0x78, FindWorkplace
//   0x1400893c0 AnyCitizenSick      — scan mgr(+0xa0/+0xa8) IBP array for +0x68 != 0
//   0x1400305e0 IntArrayResize      — generic 4-byte-elem grow-array resize+fill
//   0x140092990 Map_RadiusQuery     — quadtree walk (18dd0/18880, registered) collecting
//                                     entity links within radius, optional type-id filter
//
// CitizenComponent fields touched:
//   +0x50 health(float)  +0x60 time-since-roll(Time)  +0x68 Disease*  +0x70 immunity/
//   duration timer(Time) +0x78 spread-tick timer(Time) +0x228 herbalist-treatment link
//   +0x278 model-entity link (->+0x68 ->+0x158 pos)   +0x288 CitizenDescription
// CitizenDescription: +0x70 maxHealth, +0x7c/0x80..0x8c/0x90 sick-chance lerp table,
//   +0x94/+0x98 duration base/spread (months), +0x9c immunity months, +0x128 herb mult
// Disease resource (code/Game/Game/disease.cpp): +0x20 StringTable, +0x28 name String,
//   +0x30 spread-tick months, +0x34 contagion, +0x38 radius, +0x3c weight, +0x40 1/death
// GameWorld: +0x88 spatial map  +0xa8 disease ExternalList  +0xd8 Random  +0xf0 town
//   mgr (+0x1a8 health scale, +0xa0/+0xa8 citizen IBP array)  +0x138 event log
// Spatial map: +0x00 root size(float)  +0x10 entity records (stride 0x18: +0 link IBP
//   value, +8 next index — circular)  +0x30 per-node first-index[]  +0x50 per-node count[]
//
// Constants (read_memory-verified): 0x1401721b8=12.0f (months/yr, registered
// reg_component_fruit), 0x140172234=300.0f (secs/month, reg_citizen_needsupdate),
// 0x14017214c=1.0f, DAT_140172174=2.0f, DAT_140172280=-1.0f (inlined bit-exact).

#include <windows.h>
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/WorkPlaceComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenComponent.h"   // B.3.8 CB1a (TypeId)
#include "Game/Disease.h"                 // B.3.9 EXA: Disease::TypeId()
#include "options.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "math/Math.h"
#include "system/String.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include <new>
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"
#include "volume/Volume.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

namespace {

typedef uint8_t u8;

// Exe bodies called by name: CitizenComp_FindCareBuilding 0x140111590,
// AttendanceComponent_ReleaseLink59b0 0x1401159b0 (Entity_impl_59b0), the quadtree
// walker Pf_QuadtreeInit_18dd0 0x140018dd0 / Pf_QuadtreeSubdivide_18880 0x140018880,
// EventLog_AddCitizenEvent 0x140094e00.
// Runtime calls the original makes through exe import slots: V3 ctor3 0x140152a30,
// V3 splat 0x140151320, Dot 0x140151298 (all direct Math::Vector3 calls),
// Box::Intersect 0x140151e80 (RtVolume_Box_Intersect), StringTable::GetText(String&)
// 0x140151718.

// Disease type id. The .rdata global (0x1401c5948) is env-gated / unregistered
// in standalone — resolve the class's own record instead (D5, ENDGAME B.3.9 CG3;
// citizen's own 0x1401c7158 now reads CitizenComponent::TypeId() directly).
static uint32_t s_tidDisease = 0;

static bool s_init = false;

// Resolves no door: it only caches the Disease type id.
static void EnsureInit() {
    if (s_init) return;
    s_init = true;

    s_tidDisease = Disease::TypeId();   // B.3.9 EXA: the class's own record (was by name)
}

static inline void* GameWorld() {
    return GameUpdateLoop::_singleton;
}
// 0x1401721b8 = 12.0f, stored as its bit pattern (exe_constants.h).
static inline float MonthsPerYear() {
    float f;
    const uint32_t bits = kRdata_1401721b8;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
static inline float SecsPerMonth()  { return kRdata_140172234; }   // 0x140172234 = 300.0f

// Time(float seconds) -> int64 ticks, via the Runtime ctor.
static inline int64_t TimeFromSeconds(float secs) {
    int64_t t = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), secs);
    return t;
}

// ============================================================
// [Plague] soak instrumentation (DIAG ONLY — no behaviour change)
// ============================================================
// Every line reports an OUTCOME, printed from inside the guard it describes, with
// both arms covered. Default ON; kill with BANISHED_PLAGUE_TRACE=0. The only
// high-frequency site (the per-citizen catch roll) is a periodic census, not a
// per-call line; every other event is inherently rare.
static bool PlagueTraceOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = Opt().plagueTrace;
        en = (e && (e[0] == '0' || std::strcmp(e, "false") == 0)) ? 0 : 1;
    }
    return en == 1;
}

// Catch-roll census. Printed every kCensusPeriod rolls so a soak gets a steady
// pulse whether or not anyone ever gets ill — a silent log would otherwise be
// indistinguishable from "the roll never runs".
static const long kCensusPeriod = 4000;
static long s_rollCalls = 0, s_rollSkippedNoWindow = 0, s_rollFired = 0;
static long s_rollEpidemicBlocked = 0;
static double s_rollChanceSum = 0.0;
static float  s_rollChanceMax = 0.0f;

// Periodic census of the catch roll — the "not fired" arm. Without this a soak in
// which nobody ever falls ill produces an empty log, which reads identically to the
// roll never running at all.
static void PlagueCensusTick() {
    if (!PlagueTraceOn()) return;
    if (s_rollCalls % kCensusPeriod != 0) return;
    fprintf(stderr,
            "[Plague] CENSUS rolls=%ld fired=%ld noWindow(B<=0)=%ld epidemicBlocked=%ld "
            "meanChance=%.8f maxChance=%.8f\n",
            s_rollCalls, s_rollFired, s_rollSkippedNoWindow, s_rollEpidemicBlocked,
            (s_rollCalls > s_rollSkippedNoWindow)
                ? s_rollChanceSum / (double)(s_rollCalls - s_rollSkippedNoWindow) : 0.0,
            s_rollChanceMax);
    fflush(stderr);
}

// One-shot tuning report: the numbers the cure arm depends on. Valid on the first
// UpdateHealth call — the CitizenDescription is fully populated by then.
static bool s_bootReported = false;

// Hospital census. RE-ARMS until it has proof the registry is populated, because an
// assertion of absence is only valid once the list is known to be non-empty.
//
// ⚠ WHAT THIS LIST ACTUALLY IS (root-caused 2026-08-22 after a smoke run reported
// "buildings=0" on a town that plainly had buildings):
// FindCareBuilding -> sub_1930 (0x140111930) walks townMgr+0x60 / count +0x68, and that
// is the TOWN ATTENDANCE REGISTRY, not a building list. It is appended by
// AttendanceComponent state-2 enter (retail 0x140043c90 — the OPEN transition), so a
// building appears in it only once it HAS an AttendanceComponent AND that component has
// opened. The real building/house registry is a different array, townMgr+0x20 / +0x28
// (the one Game::StartRandomFire and the employment scan use).
// The earlier label "buildings=" was therefore wrong, and wrong in the worst direction:
// it made an empty attendance registry read as an empty town. Both counts are now
// printed so the two cases can never be confused again.
static bool s_censusDone = false;
static long s_censusTries = 0;

static void PlagueHospitalCensus(void* gw, void* healerProf) {
    if (s_censusDone) return;
    ++s_censusTries;

    uintptr_t mgr = gw ? *(uintptr_t*)((uintptr_t)gw + 0xf0) : 0;
    if (!mgr) {
        if (s_censusTries == 1) {
            fprintf(stderr, "[Plague] BOOT: town manager (gw+0xf0) NULL — census re-arming\n");
            fflush(stderr);
        }
        return;
    }

    int attendance = *(int*)(mgr + 0x68);          // townMgr+0x60/+0x68 — what sub_1930 walks
    int buildings  = *(int*)(mgr + 0x28);          // townMgr+0x20/+0x28 — the real building list
    int citizens   = *(int*)(mgr + 0xa8);

    if (attendance <= 0) {
        // NOT an assertion of absence. Say plainly that we cannot answer yet.
        if (s_censusTries == 1 || (s_censusTries % 5000) == 0) {
            fprintf(stderr,
                    "[Plague] BOOT: hospital census DEFERRED — attendance registry "
                    "(townMgr+0x60/+0x68) is EMPTY, so there is nothing to search yet. "
                    "buildings(townMgr+0x28)=%d citizens(+0xa8)=%d try=%ld. A building "
                    "joins that registry only when its AttendanceComponent reaches state 2 "
                    "(the OPEN transition), which is after StartTown. Re-arming.\n",
                    buildings, citizens, s_censusTries);
            fflush(stderr);
        }
        if (s_censusTries >= 200000) {
            s_censusDone = true;
            fprintf(stderr,
                    "[Plague] BOOT: hospital census GAVE UP after %ld tries — the attendance "
                    "registry never became non-empty. buildings(townMgr+0x28)=%d. If the town "
                    "has buildings, nothing is completing the AttendanceComponent OPEN "
                    "transition, and FindCareBuilding / FindStorage / TryEnrolInSchool are ALL "
                    "blind for the same reason. That is the thing to chase, not the hospital.\n",
                    s_censusTries, buildings);
            fflush(stderr);
        }
        return;
    }

    // Registry is populated — an answer here is now meaningful.
    s_censusDone = true;
    uint32_t tidWpc = WorkPlaceComponent::TypeId();
    uintptr_t arr = *(uintptr_t*)(mgr + 0x60);
    int withWpc = 0, matches = 0;
    for (int i = 0; i < attendance && arr && tidWpc; i++) {
        uintptr_t comp = *(uintptr_t*)(arr + (uintptr_t)i * 8);
        if (!comp) continue;
        uintptr_t entity = *(uintptr_t*)(comp + 0x10);
        if (!entity) continue;
        int n = *(int*)(entity + 0x38);
        uintptr_t comps = *(uintptr_t*)(entity + 0x30);
        for (int j = 0; j < n && comps; j++) {
            uintptr_t c = *(uintptr_t*)(comps + (uintptr_t)j * 8);
            if (!c) continue;
            if (reinterpret_cast<Object::Base*>(c)->Type() != tidWpc) continue;   // B.3.8 bridge (slot 2)
            ++withWpc;
            if (*(void**)(c + 0x48) == healerProf && healerProf != nullptr) ++matches;
            break;
        }
    }
    fprintf(stderr,
            "[Plague] BOOT: hospital census — attendanceRegistry(townMgr+0x68)=%d "
            "withWorkPlaceComponent=%d matchingHealerProfession=%d "
            "buildings(townMgr+0x28)=%d tidWorkPlaceComponent=%u settledAfter=%ld tries%s\n",
            attendance, withWpc, matches, buildings, tidWpc, s_censusTries,
            (matches == 0) ? "  <== NO HOSPITAL IN THE ATTENDANCE REGISTRY: FindCareBuilding"
                             " can never link, so the cure arm is unreachable. Check that the"
                             " hospital template carries an AttendanceDescription and that its"
                             " AttendanceComponent has OPENED." : "");
    fflush(stderr);
}

static void PlagueBootReport(u8* citizen) {
    if (!PlagueTraceOn()) return;

    void* gw = GameWorld();
    u8* desc = *(u8**)(citizen + 0x288);
    if (!desc) {
        if (!s_bootReported) {
            s_bootReported = true;
            fprintf(stderr, "[Plague] BOOT: citizen+0x288 (CitizenDescription) is NULL — "
                            "no tuning available\n");
            fflush(stderr);
        }
        return;
    }

    void* healerProf = *(void**)(desc + 0x30);

    if (!s_bootReported) {
        s_bootReported = true;
        float careMult = *(float*)(desc + 0x128);
        fprintf(stderr,
                "[Plague] BOOT: careMult(desc+0x128)=%.4f%s healerProf(desc+0x30)=%p "
                "maxHealth(+0x70)=%.3f durBase(+0x94)=%.3f durSpread(+0x98)=%.3f "
                "immunityMonths(+0x9c)=%.3f diseaseList(gw+0xa8)=%p tidDisease=%u\n",
                careMult,
                (careMult == 0.0f) ? "  <== ZERO: (1/recipMortality)/0 is INFINITE, every"
                                     " treated citizen dies instantly" : "",
                healerProf, *(float*)(desc + 0x70), *(float*)(desc + 0x94),
                *(float*)(desc + 0x98), *(float*)(desc + 0x9c),
                gw ? *(void**)((uintptr_t)gw + 0xa8) : nullptr, s_tidDisease);
        fflush(stderr);
    }

    PlagueHospitalCensus(gw, healerProf);
}

} // namespace

// ============================================================
// 0x1400893c0 — any citizen currently diseased?
// ============================================================
// mgr = *(gw+0xf0): IBP array at +0xa0, count at +0xa8. Returns 1 on the first
// entry whose +0x68 (Disease*) is set — the game allows one epidemic at a time.
extern "C" char CitizenComp_AnyCitizenSick(void* mgr) {
    EnsureInit();
    if (!mgr) return 0;
    int count = *(int*)((uintptr_t)mgr + 0xa8);
    uintptr_t arr = *(uintptr_t*)((uintptr_t)mgr + 0xa0);
    for (int i = 0; i < count; i++) {
        void* slot = (void*)(arr + (uintptr_t)i * 8);
        uintptr_t citizen = *(uintptr_t*)slot;
        if (*(int64_t*)(citizen + 0x68) != 0) return 1;
    }
    return 0;
}

// ============================================================
// 0x1400305e0 — generic 4-byte-element grow-array resize + fill
// ============================================================
// arr = {data* @+0, count @+8, capacity @+0xc}. Resize to newCount: triple-capacity
// heuristic on a single-element grow, copy preserved elements, fill new slots with
// *fill. (The original's unrolled copy loop is semantically this.)
extern "C" void CitizenComp_IntArrayResize(void* arrBase, int newCount, const uint32_t* fill) {
    EnsureInit();
    uintptr_t* base = (uintptr_t*)arrBase;
    int oldCount = *(int*)((uintptr_t)base + 8);
    if (newCount == oldCount) return;

    uintptr_t oldData = *base;
    *(int*)((uintptr_t)base + 8) = newCount;
    int* capPtr = (int*)((uintptr_t)base + 0xc);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *capPtr = newCount * 3;
        *base = (uintptr_t)System::MemoryAllocator::MemAlloc((uint64_t)*capPtr * 4, 4);
    }
    uintptr_t newData = *base;

    int start = (oldData == newData) ? oldCount : 0;
    for (int i = start; i < newCount; i++) {
        *(uint32_t*)(newData + (uintptr_t)i * 4) =
            (i < oldCount) ? *(const uint32_t*)(oldData + (uintptr_t)i * 4) : *fill;
    }
    if (oldData != newData && oldData) System::MemoryAllocator::MemFree((void*)oldData);
}

// ============================================================
// 0x140092990 — quadtree radius query
// ============================================================
// Collects entity-link IBP values within `radius` of `pos` into the 8-byte-element
// grow-array outArr {data,count,cap}. typeId != 0 additionally requires the entity to
// own a component whose GetTypeId() == typeId (found component only FILTERS — the
// pushed value is always the entity link). Drives the registered quadtree walker
// 0x140018dd0/0x140018880; walker record = {pos.xyz @+0, ext.xyz @+0xc, nodeId @+0x18},
// stride 0x24, base walker+0x24, top index at walker+0x8. Node box =
// [pos - (2ex,2ey,ez) + (0,0,-1), pos + (2ex,2ey,ez) + (0,0,+1)]  (the (0,0,∓1) z pads
// are the walker's +0x0c/+0x18 vectors, seeded from the init's -1.0f/+1.0f args).
extern "C" void* Map_RadiusQuery_92990(float* map, void** out, float* pos, float* radius,
                                       int typeId, void* outArr) {
    EnsureInit();
    if (out) *out = nullptr;
    if (!map || !pos || !radius || !outArr)
        return out;

    // 0x2430-byte walker (0xFF records * 0x24 + header + map ptr at +0x2428).
    alignas(16) u8 walker[0x2430];
    Pf_QuadtreeInit_18dd0(reinterpret_cast<uintptr_t>(walker), map,
                          0xbf800000u /* -1.0f bits */, 0x3f800000u /* +1.0f bits */);

    float r  = *radius;
    float r2 = r * r;

    // Query box {min, max} around pos.
    float qbox[6] = { pos[0] - r, pos[1] - r, pos[2] - r,
                      pos[0] + r, pos[1] + r, pos[2] + r };

    uintptr_t* arr = (uintptr_t*)outArr;   // {data @+0, count @+8, cap @+0xc}

    int* top = (int*)(walker + 0x8);
    while (*top >= 0) {
        u8* rec = walker + 0x24 + (uintptr_t)(*top) * 0x24;
        int nodeId = *(int*)(rec + 0x18);
        int nodeCount = *(int*)(*(uintptr_t*)((uintptr_t)map + 0x50) + (uintptr_t)nodeId * 4);
        if (nodeCount <= 0) { (*top)--; continue; }

        float* rp = (float*)rec;           // pos.xyz
        float* re = (float*)(rec + 0xc);   // ext.xyz
        float sx = 2.0f * re[0], sy = 2.0f * re[1], sz = 1.0f * re[2];
        float* zlo = (float*)(walker + 0x0c);   // (0, 0, -1)
        float* zhi = (float*)(walker + 0x18);   // (0, 0, +1)
        float nbox[6] = { rp[0] - sx + zlo[0], rp[1] - sy + zlo[1], rp[2] - sz + zlo[2],
                          rp[0] + sx + zhi[0], rp[1] + sy + zhi[1], rp[2] + sz + zhi[2] };
        if (!RtVolume_Box_Intersect(reinterpret_cast<const struct Box*>(qbox),
                                    reinterpret_cast<const struct Box*>(nbox))) { (*top)--; continue; }

        int first = *(int*)(*(uintptr_t*)((uintptr_t)map + 0x30) + (uintptr_t)nodeId * 4);
        if (first >= 0) {
            uintptr_t recs = *(uintptr_t*)((uintptr_t)map + 0x10);
            int idx = first;
            do {
                uintptr_t linkIBP = *(uintptr_t*)(recs + (uintptr_t)idx * 0x18);

                bool accept = true;
                if (typeId != 0) {
                    accept = false;
                    uintptr_t entity = *(uintptr_t*)(linkIBP + 0x10);
                    int compCount = *(int*)(entity + 0x38);
                    uintptr_t comps = *(uintptr_t*)(entity + 0x30);
                    for (int c = 0; c < compCount; c++) {
                        void* compSlot = (void*)(comps + (uintptr_t)c * 8);
                        uintptr_t comp = *(uintptr_t*)compSlot;
                        if (static_cast<int>(reinterpret_cast<Object::Base*>(comp)->Type()) == typeId) { accept = true; break; }   // B.3.8 bridge (slot 2)
                    }
                }

                if (accept) {
                    uintptr_t link = linkIBP;
                    uintptr_t model = *(uintptr_t*)(link + 0x68);
                    Math::Vector3 diff(*(float*)(model + 0x158) - pos[0],
                                       *(float*)(model + 0x15c) - pos[1],
                                       *(float*)(model + 0x160) - pos[2]);
                    float dist2 = diff.Dot(diff).v;
                    if (dist2 < r2) {
                        // inline 8-byte grow-array push (×3 capacity rule)
                        int count = *(int*)((uintptr_t)arr + 8);
                        uintptr_t oldData = *arr;
                        *(int*)((uintptr_t)arr + 8) = count + 1;
                        int* capPtr = (int*)((uintptr_t)arr + 0xc);
                        if (*capPtr < count + 1) {
                            *capPtr = count + 1;
                            if (count != 0) *capPtr = (count + 1) * 3;
                            *arr = (uintptr_t)System::MemoryAllocator::MemAlloc((uint64_t)*capPtr * 8, 8);
                        }
                        uintptr_t newData = *arr;
                        int start = (oldData == newData) ? count : 0;
                        for (int i = start; i <= count; i++) {
                            *(uintptr_t*)(newData + (uintptr_t)i * 8) =
                                (i < count) ? *(uintptr_t*)(oldData + (uintptr_t)i * 8)
                                            : linkIBP;
                        }
                        if (oldData != newData && oldData) System::MemoryAllocator::MemFree((void*)oldData);
                    }
                }

                idx = *(int*)(recs + (uintptr_t)idx * 0x18 + 8);
            } while (idx != first);
        }

        Pf_QuadtreeSubdivide_18880(reinterpret_cast<uintptr_t>(walker));   // pop node / push its children
    }
    return out;
}

// ============================================================
// 0x140116b90 — infect a citizen with `disease`
// ============================================================
// Skips if already sick, still immune (+0x70 > 0), or (flag *(+0x1f0)+0x4d set AND
// housed +0x248 != 0). Posts the type-0xb (sev 3) event UNLESS an epidemic is already
// active (AnyCitizenSick) — the text formats {0}=season {1}=year {2}=disease name from
// the disease's own StringTable (+0x20) keyed by its name String (+0x28). Then:
// +0x68 = disease, +0x70 = Time((RandFloatTol*desc+0x98 + desc+0x94) * 300),
// +0x78 = Time(disease+0x30 * 300), FindWorkplace (sick citizens go home / seek herbs).
extern "C" void CitizenComp_Infect(void* self, void* disease) {
    EnsureInit();
    u8* s = (u8*)self;
    void* gw = GameWorld();
    if (!self || !disease || !gw) {
        if (PlagueTraceOn())
            fprintf(stderr, "[Plague] INFECT skip=nullarg citizen=%p disease=%p gw=%p\n",
                    self, disease, gw), fflush(stderr);
        return;
    }

    if (*(int64_t*)(s + 0x68) != 0) {
        if (PlagueTraceOn())
            fprintf(stderr, "[Plague] INFECT skip=alreadySick citizen=%p\n", self), fflush(stderr);
        return;
    }
    if (*(int64_t*)(s + 0x70) > 0) {
        if (PlagueTraceOn())
            fprintf(stderr, "[Plague] INFECT skip=immune citizen=%p immunityTicks=%lld\n",
                    self, (long long)*(int64_t*)(s + 0x70)), fflush(stderr);
        return;
    }
    uintptr_t p1f0 = *(uintptr_t*)(s + 0x1f0);
    if (p1f0 && *(char*)(p1f0 + 0x4d) != 0 && *(int64_t*)(s + 0x248) != 0) {
        if (PlagueTraceOn())
            fprintf(stderr, "[Plague] INFECT skip=exemptProfessionAndHoused citizen=%p\n",
                    self), fflush(stderr);
        return;
    }

    void* rng = *(void**)((uintptr_t)gw + 0xd8);

    if (!CitizenComp_AnyCitizenSick(*(void**)((uintptr_t)gw + 0xf0))) {
        // Post "outbreak of {disease}" via the registered AddCitizenEvent
        // (0x140094e00): identical entry to the original inline block — color
        // 0xffff6060 (sev 3), type 0xb, icon 1, popup -1, tile -1 (null entity) —
        // including the drop-to-1x-speed tail on the settings +0x17 byte.
        void* eventLog = *(void**)((uintptr_t)gw + 0x138);
        void* nameTable = *(void**)((uintptr_t)disease + 0x20);
        System::String diseaseName;
        bool haveName = false;
        if (nameTable) {
            ::new (&diseaseName) System::String(
                reinterpret_cast<const UI::StringTable*>(nameTable)->GetText(  // B.3.6 bridge
                    *reinterpret_cast<const System::String*>((u8*)disease + 0x28)));
            haveName = true;
        }
        if (eventLog) {
            // {3}/{4} unused by the type-0xb template; AddCitizenEvent maps null -> "".
            EventLog_AddCitizenEvent(eventLog, 0xb, 3, nullptr,
                                     haveName ? &diseaseName : nullptr, nullptr, nullptr);
        }
    }

    *(void**)(s + 0x68) = disease;
    u8* desc = *(u8**)(s + 0x288);
    float tol = fn_Random_RandFloatTol(rng);   // real fn (engine_funcs shim), not a ptr
    *(int64_t*)(s + 0x70) = TimeFromSeconds(
        (tol * *(float*)(desc + 0x98) + *(float*)(desc + 0x94)) * SecsPerMonth());
    *(int64_t*)(s + 0x78) = TimeFromSeconds(
        *(float*)((uintptr_t)disease + 0x30) * SecsPerMonth());
    CitizenComp_FindCareBuilding(static_cast<CitizenComponent*>(self));   // 0x140111590

    // Both arms: whether the care link actually got made. +0x228 == 0 here means
    // FindCareBuilding (0x140111590 -> sub_1930 0x140111930) found no building whose
    // WorkPlaceComponent+0x48 matches CitizenDescription+0x30 — no hospital, or the
    // profession ref is null. Without it RecoverOrDie uses careMult 1.0.
    if (PlagueTraceOn()) {
        fprintf(stderr,
                "[Plague] INFECT ok citizen=%p disease=%p durationTicks=%lld "
                "spreadTicks=%lld care(+0x228)=%s finderResolved=%d\n",
                self, disease, (long long)*(int64_t*)(s + 0x70),
                (long long)*(int64_t*)(s + 0x78),
                (*(int64_t*)(s + 0x228) != 0) ? "LINKED" : "NOT-LINKED",
                1);   // the finder is a direct call: always resolved
        fflush(stderr);
    }
}

// ============================================================
// 0x140112620 — catch a disease (weighted random pick)
// ============================================================
// No-op while an epidemic is active. Builds a cumulative-weight array over the
// disease ExternalList (gw+0xa8, weight = disease+0x3c), rolls rand*total, picks the
// bucket, infects self. Non-Disease entries contribute a NULL deref in the original;
// here the type-id gate skips the whole body if "Disease" is unresolved.
extern "C" void CitizenComp_CatchDisease(void* self) {
    EnsureInit();
    void* gw = GameWorld();
    if (!self || !gw || s_tidDisease == 0) {
        // One-shot: this is a structural condition, true for every citizen every tick.
        static bool once = false;
        if (PlagueTraceOn() && !once && (once = true))
            fprintf(stderr, "[Plague] CATCH bail=unresolved citizen=%p gw=%p "
                    "tidDisease=%u\n",
                    self, gw, s_tidDisease), fflush(stderr);
        return;
    }
    if (CitizenComp_AnyCitizenSick(*(void**)((uintptr_t)gw + 0xf0))) {
        ++s_rollEpidemicBlocked;
        return;   // censused, not logged per-call: happens for every citizen every tick
    }

    void* rng = *(void**)((uintptr_t)gw + 0xd8);
    void* list = *(void**)((uintptr_t)gw + 0xa8);
    if (!list) {
        // One-shot: structural, true for every citizen every tick while the flag is off.
        static bool once = false;
        if (PlagueTraceOn() && !once && (once = true))
            fprintf(stderr, "[Plague] CATCH bail=noDiseaseList (gw+0xa8 NULL — set "
                    "BANISHED_LIVE_DISEASELIST=1) citizen=%p\n", self), fflush(stderr);
        return;
    }
    int count = reinterpret_cast<const ::Object::ExternalList*>(list)  // B.3.3 bridge
                    ->GetResourceCount();

    struct { uintptr_t data; int count; int cap; } cum = { 0, 0, 0 };
    float total = 0.0f;
    for (int i = 0; i < count; i++) {
        uintptr_t entry = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)list + 0x20) +
                                        (uintptr_t)i * 8);
        void* d = Object::IsDerivedFromNullSafe((void*)entry, s_tidDisease) ? (void*)entry : nullptr;
        total += *(float*)((uintptr_t)d + 0x3c);   // faithful: NULL deref if not Disease
        uint32_t bits; memcpy(&bits, &total, 4);
        CitizenComp_IntArrayResize(&cum, cum.count + 1, &bits);
    }

    float target = reinterpret_cast<System::Random*>(rng)->RandFloat() * total;   // B.3.4b bridge
    float prev = 0.0f;
    int picked = -1;
    for (int i = 0; i < count; i++) {
        float c = ((float*)cum.data)[i];
        if (prev <= target && target < c) {
            uintptr_t entry = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)list + 0x20) +
                                            (uintptr_t)i * 8);
            void* d = Object::IsDerivedFromNullSafe((void*)entry, s_tidDisease) ? (void*)entry : nullptr;
            picked = i;
            if (PlagueTraceOn()) {
                fprintf(stderr, "[Plague] CATCH draw citizen=%p entries=%d totalWeight=%.4f "
                        "target=%.4f -> idx=%d disease=%p\n",
                        self, count, total, target, i, d);
                fflush(stderr);
            }
            CitizenComp_Infect(self, d);
            break;
        }
        prev = c;
    }
    // Other arm: no bucket matched (total weight 0, or an empty list).
    if (picked < 0 && PlagueTraceOn()) {
        fprintf(stderr, "[Plague] CATCH draw citizen=%p entries=%d totalWeight=%.4f "
                "target=%.4f -> NO BUCKET (nothing drawn)\n", self, count, total, target);
        fflush(stderr);
    }
    if (cum.data) System::MemoryAllocator::MemFree((void*)cum.data);
}

// ============================================================
// 0x140112470 — try to spread the disease to a nearby citizen
// ============================================================
// Contagion roll (rand <= disease+0x34), then a radius query (disease+0x38) around
// this citizen's model position for entities owning a CitizenComponent; the first hit
// that is not self and not already sick gets infected with the same disease.
extern "C" void CitizenComp_SpreadDisease(void* self) {
    EnsureInit();
    u8* s = (u8*)self;
    void* gw = GameWorld();
    if (!self || !gw) return;
    void* disease = *(void**)(s + 0x68);
    if (!disease) return;

    void* rng = *(void**)((uintptr_t)gw + 0xd8);
    float contagionRoll = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
    float contagion = *(float*)((uintptr_t)disease + 0x34);
    if (contagionRoll > contagion) {
        if (PlagueTraceOn())
            fprintf(stderr, "[Plague] SPREAD citizen=%p roll=FAILED (%.4f > contagion %.4f)\n",
                    self, contagionRoll, contagion), fflush(stderr);
        return;
    }

    float radius = *(float*)((uintptr_t)disease + 0x38);
    uint32_t typeId = CitizenComponent::TypeId();   // cell 0x1401c7158 (D5)
    if (typeId == 0) return;

    uintptr_t link = *(uintptr_t*)(s + 0x278);
    uintptr_t model = *(uintptr_t*)(link + 0x68);
    float pos[3] = { *(float*)(model + 0x158), *(float*)(model + 0x15c),
                     *(float*)(model + 0x160) };

    struct { uintptr_t data; int count; int cap; } found = { 0, 0, 0 };
    void* victim = nullptr;   // diag only
    void* scratch = nullptr;
    Map_RadiusQuery_92990(*(float**)((uintptr_t)gw + 0x88), &scratch, pos, &radius,
                          (int)typeId, &found);

    for (int i = 0; i < found.count; i++) {
        void* slot = (void*)(found.data + (uintptr_t)i * 8);
        uintptr_t entity = *(uintptr_t*)(*(uintptr_t*)slot + 0x10);
        if (entity == *(uintptr_t*)(s + 0x10)) continue;   // skip self

        int64_t entityIBP = (int64_t)entity;
        int64_t citizenIBP = 0;
        Entity_FindComponentByExeTypeId((void*)entityIBP, &citizenIBP, &typeId);
        if (citizenIBP && *(int64_t*)((uintptr_t)citizenIBP + 0x68) == 0) {
            victim = (void*)citizenIBP;
            if (PlagueTraceOn()) {
                fprintf(stderr, "[Plague] SPREAD citizen=%p roll=PASSED (%.4f <= %.4f) "
                        "radius=%.2f hits=%d -> INFECTING victim=%p\n",
                        self, contagionRoll, contagion, radius, found.count, victim);
                fflush(stderr);
            }
            CitizenComp_Infect((void*)citizenIBP, disease);
            break;
        }
    }
    // Other arm: the roll passed but nobody in range was eligible.
    if (!victim && PlagueTraceOn()) {
        fprintf(stderr, "[Plague] SPREAD citizen=%p roll=PASSED (%.4f <= %.4f) "
                "radius=%.2f hits=%d -> NO ELIGIBLE VICTIM (all self or already sick)\n",
                self, contagionRoll, contagion, radius, found.count);
        fflush(stderr);
    }
    System::MemoryAllocator::MemFree((void*)found.data);   // original frees even when null
}

// ============================================================
// 0x1401123b0 — disease has run its course: recover or die
// ============================================================
// deathChance = (1 / disease+0x40) / (treated(+0x228) ? desc+0x128 : 1). rand < chance
// → die (release attendance link 59b0, return 0). Else survive: +0x78 = 0, +0x70 =
// immunity Time(desc+0x9c * 300), +0x68 = 0, release link, return 1.
extern "C" char CitizenComp_RecoverOrDie(void* self) {
    EnsureInit();
    u8* s = (u8*)self;
    void* gw = GameWorld();
    if (!self || !gw) return 1;

    float r = reinterpret_cast<System::Random*>(*(void**)((uintptr_t)gw + 0xd8))->RandFloat();   // B.3.4b bridge
    u8* desc = *(u8**)(s + 0x288);
    float mult = (*(int64_t*)(s + 0x228) != 0) ? *(float*)(desc + 0x128) : 1.0f;
    void* disease = *(void**)(s + 0x68);
    float deathChance = (1.0f / *(float*)((uintptr_t)disease + 0x40)) / mult;

    bool treated = (*(int64_t*)(s + 0x228) != 0);
    if (deathChance >= r) {                     // COMISS deathChance,r; !JC
        if (PlagueTraceOn()) {
            fprintf(stderr, "[Plague] RESOLVE citizen=%p outcome=DIED deathChance=%.5f "
                    "roll=%.5f recipMortality(+0x40)=%.3f careMult=%.4f care(+0x228)=%s\n",
                    self, deathChance, r, *(float*)((uintptr_t)disease + 0x40), mult,
                    treated ? "LINKED" : "NOT-LINKED");
            fflush(stderr);
        }
        AttendanceComponent_ReleaseLink59b0(self);   // 0x1401159b0
        return 0;                               // dead
    }
    *(int64_t*)(s + 0x78) = 0;
    *(int64_t*)(s + 0x70) = TimeFromSeconds(*(float*)(desc + 0x9c) * SecsPerMonth());
    *(int64_t*)(s + 0x68) = 0;
    if (PlagueTraceOn()) {
        fprintf(stderr, "[Plague] RESOLVE citizen=%p outcome=RECOVERED deathChance=%.5f "
                "roll=%.5f recipMortality(+0x40)=%.3f careMult=%.4f care(+0x228)=%s "
                "immunityTicks=%lld\n",
                self, deathChance, r, *(float*)((uintptr_t)disease + 0x40), mult,
                treated ? "LINKED" : "NOT-LINKED", (long long)*(int64_t*)(s + 0x70));
        fflush(stderr);
    }
    AttendanceComponent_ReleaseLink59b0(self);   // 0x1401159b0
    return 1;                                   // recovered
}

// ============================================================
// 0x1401127d0 — CitizenComponent::UpdateHealth
// ============================================================
// Returns 0 only when the citizen dies of disease (caller removes them).
// Healthy: tick down immunity (+0x70, clamp 0); lerp the catch-chance window from
// desc+0x7c..0x90 by (1 - health/maxHealth) then by the town health scale
// (*(gw+0xf0)+0x1a8); if the upper value B <= 0 just reset the roll timer; else roll
// elapsed(+0x60)/(A * 12 * 300 * B) vs rand and maybe catch a disease.
// Sick: tick +0x70 (duration) and +0x78 (spread); spread-tick expiry → SpreadDisease
// + re-arm from disease+0x30; duration expiry → RecoverOrDie.
extern "C" char CitizenComp_UpdateHealth(void* self, int64_t* dt) {
    EnsureInit();
    u8* s = (u8*)self;
    void* gw = GameWorld();
    if (!self || !dt || !gw) return 1;

    PlagueBootReport(s);

    if (*(int64_t*)(s + 0x68) == 0) {
        *(int64_t*)(s + 0x70) -= *dt;
        if (*(int64_t*)(s + 0x70) < 0) *(int64_t*)(s + 0x70) = 0;

        u8* desc = *(u8**)(s + 0x288);
        uintptr_t mgr = *(uintptr_t*)((uintptr_t)gw + 0xf0);
        void* rng = *(void**)((uintptr_t)gw + 0xd8);

        float t = 1.0f - *(float*)(s + 0x50) / *(float*)(desc + 0x70);
        float a = (*(float*)(desc + 0x84) - *(float*)(desc + 0x7c)) * t + *(float*)(desc + 0x7c);
        float b = (*(float*)(desc + 0x88) - *(float*)(desc + 0x80)) * t + *(float*)(desc + 0x80);
        float m = *(float*)(mgr + 0x1a8);
        float A = (*(float*)(desc + 0x8c) - a) * m + a;
        float B = (*(float*)(desc + 0x90) - b) * m + b;

        ++s_rollCalls;
        if (!(B > 0.0f)) {                       // COMISS 0,B; !JC (B <= 0 or NaN)
            ++s_rollSkippedNoWindow;
            *(int64_t*)(s + 0x60) = 0;
            PlagueCensusTick();
            return 1;
        }
        float elapsed = reinterpret_cast<const System::Time*>(s + 0x60)->AsSeconds();   // B.3.4b bridge
        *(int64_t*)(s + 0x60) = 0;
        float denom = A * MonthsPerYear() * SecsPerMonth() * B;
        float ratio = elapsed / denom;
        float r = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
        s_rollChanceSum += (double)ratio;
        if (ratio > s_rollChanceMax) s_rollChanceMax = ratio;
        // [Plague] ROLL-INPUTS -- diagnostic only. The census reported meanChance and
        // maxChance as exactly 0.00000000 over 200k rolls with the disease list loaded,
        // which the aggregate alone cannot explain: it is either elapsed==0 (the +0x60
        // accumulator never reaching this read) or a denom so large the ratio underflows
        // the 8-decimal print. Those need different fixes, so print the raw terms of the
        // first few rolls instead of guessing.
        {
            static int nInputs = 0;
            if (PlagueTraceOn() && nInputs < 6) {
                ++nInputs;
                fprintf(stderr, "[Plague] ROLL-INPUTS citizen=%p elapsed=%.9fs A=%.6f "
                        "B=%.6f monthsPerYear=%.2f secsPerMonth=%.2f denom=%.6f "
                        "ratio=%.9f roll=%.6f health(+0x50)=%.3f maxHealth(desc+0x70)=%.3f "
                        "townMult(mgr+0x1a8)=%.6f\n",
                        self, elapsed, A, B, MonthsPerYear(), SecsPerMonth(), denom,
                        ratio, r, *(float*)(s + 0x50), *(float*)(desc + 0x70), m);
                fflush(stderr);
            }
        }
        if (ratio > r) {
            ++s_rollFired;
            if (PlagueTraceOn()) {
                fprintf(stderr, "[Plague] ROLL citizen=%p FIRED chance=%.6f roll=%.6f "
                        "health=%.2f/%.2f A=%.4f B=%.4f elapsed=%.2fs\n",
                        self, ratio, r, *(float*)(s + 0x50), *(float*)(desc + 0x70),
                        A, B, elapsed);
                fflush(stderr);
            }
            CitizenComp_CatchDisease(self);
        }
        PlagueCensusTick();
        return 1;
    }

    // sick
    *(int64_t*)(s + 0x70) -= *dt;
    *(int64_t*)(s + 0x78) -= *dt;
    if (*(int64_t*)(s + 0x78) <= 0) {
        CitizenComp_SpreadDisease(self);
        void* disease = *(void**)(s + 0x68);
        *(int64_t*)(s + 0x78) = TimeFromSeconds(
            *(float*)((uintptr_t)disease + 0x30) * SecsPerMonth());
    }
    if (*(int64_t*)(s + 0x70) > 0) return 1;
    return CitizenComp_RecoverOrDie(self);      // original tail-jumps here
}
