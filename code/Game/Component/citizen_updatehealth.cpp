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

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstring>

namespace {

typedef uint8_t u8;

// ---- exe functions already ported+registered (reused via ResolveAddr) -------
typedef void  (*fn_FindWorkplace_t)(void* self);                 // 0x140111590
typedef void  (*fn_ReleaseLink_t)(void* self);                   // 0x1401159b0
typedef void  (*fn_QInit_t)(void* walker, float* map, uint32_t zLoBits, uint32_t zHiBits); // 0x140018dd0
typedef void  (*fn_QSubdiv_t)(void* walker);                     // 0x140018880
typedef void  (*fn_AddCitizenEvent_t)(void* eventLog, int eventType, int severity,
                                      void* entityIBP, void* nameStr, void* titleStr,
                                      void* extraStr);           // 0x140094e00
static fn_FindWorkplace_t   s_findWorkplace  = nullptr;
static fn_ReleaseLink_t     s_releaseLink    = nullptr;          // Entity_impl_59b0
static fn_QInit_t           s_qinit          = nullptr;
static fn_QSubdiv_t         s_qsubdiv        = nullptr;
static fn_AddCitizenEvent_t s_addCitizenEvent = nullptr;

// ---- Runtime engine calls via exe .rdata dispatch slots (export fallback) ---
// Original call sites use these slots: V3 ctor3 0x140152a30, V3 splat 0x140151320,
// Dot 0x140151298, Box::Intersect 0x140151e80, StringTable::GetText(String&) 0x140151718.
typedef void  (*fn_V3Ctor3_t)(void* self, float x, float y, float z);
typedef void  (*fn_V3Dot_t)(const void* self, void* retbuf, const void* other);
typedef bool  (*fn_BoxIntersect_t)(const void* self, const void* other);
typedef void* (*fn_GetTextByName_t)(void* table, void* retStr, const void* nameStr);
static fn_V3Ctor3_t      s_v3ctor3       = nullptr;
static fn_V3Dot_t        s_v3dot         = nullptr;
static fn_BoxIntersect_t s_boxIntersect  = nullptr;
static fn_GetTextByName_t s_getTextByName = nullptr;

// ---- rdata float constants (registered by earlier waves) --------------------
static const float* s_pMonthsPerYear = nullptr;  // 0x1401721b8 = 12.0f
static const float* s_pSecsPerMonth  = nullptr;  // 0x140172234 = 300.0f

// Type ids. The .rdata globals (0x1401c7158 citizen, 0x1401c5948 disease) are
// env-gated / unregistered in standalone — resolve by name like the other citizen
// ports ("private GetTypeIdByName copy" pattern; both types REGISTER_TYPE'd).
static uint32_t s_tidCitizen = 0;
static uint32_t s_tidDisease = 0;

static bool s_init = false;

static inline void* RSlot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}

static void EnsureInit() {
    if (s_init) return;
    s_init = true;

    s_findWorkplace  = reinterpret_cast<fn_FindWorkplace_t>(ResolveAddr(Module::Exe, 0x140111590));
    s_releaseLink    = reinterpret_cast<fn_ReleaseLink_t>(ResolveAddr(Module::Exe, 0x1401159b0));
    s_qinit          = reinterpret_cast<fn_QInit_t>(ResolveAddr(Module::Exe, 0x140018dd0));
    s_qsubdiv        = reinterpret_cast<fn_QSubdiv_t>(ResolveAddr(Module::Exe, 0x140018880));
    s_addCitizenEvent = reinterpret_cast<fn_AddCitizenEvent_t>(ResolveAddr(Module::Exe, 0x140094e00));

    s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(RSlot(0x140152a30));
    if (!s_v3ctor3) s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    s_v3dot = reinterpret_cast<fn_V3Dot_t>(RSlot(0x140151298));
    if (!s_v3dot) s_v3dot = reinterpret_cast<fn_V3Dot_t>(
        ResolveExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z"));
    s_boxIntersect = reinterpret_cast<fn_BoxIntersect_t>(RSlot(0x140151e80));
    if (!s_boxIntersect) s_boxIntersect = reinterpret_cast<fn_BoxIntersect_t>(
        ResolveExport("?Intersect@Box@Volume@@QEBA_NAEBV12@@Z"));
    s_getTextByName = reinterpret_cast<fn_GetTextByName_t>(RSlot(0x140151718));
    if (!s_getTextByName) s_getTextByName = reinterpret_cast<fn_GetTextByName_t>(
        ResolveExport("?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z"));

    s_pMonthsPerYear = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721b8));
    s_pSecsPerMonth  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));

    s_tidCitizen = ::Object::GetTypeIdByName("CitizenComponent");
    s_tidDisease = ::Object::GetTypeIdByName("Disease");
}

static inline void* GameWorld() {
    return g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
}
static inline float MonthsPerYear() { return s_pMonthsPerYear ? *s_pMonthsPerYear : 12.0f; }
static inline float SecsPerMonth()  { return s_pSecsPerMonth  ? *s_pSecsPerMonth  : 300.0f; }

// Time(float seconds) -> int64 ticks, via the Runtime ctor.
static inline int64_t TimeFromSeconds(float secs) {
    int64_t t = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, secs);
    return t;
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
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
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
        *base = (uintptr_t)fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)*capPtr * 4, 4);
    }
    uintptr_t newData = *base;

    int start = (oldData == newData) ? oldCount : 0;
    for (int i = start; i < newCount; i++) {
        *(uint32_t*)(newData + (uintptr_t)i * 4) =
            (i < oldCount) ? *(const uint32_t*)(oldData + (uintptr_t)i * 4) : *fill;
    }
    if (oldData != newData && oldData) fn_MemFreeStatic((void*)oldData);
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
    if (!map || !pos || !radius || !outArr || !s_qinit || !s_qsubdiv ||
        !s_v3ctor3 || !s_v3dot || !s_boxIntersect)
        return out;

    // 0x2430-byte walker (0xFF records * 0x24 + header + map ptr at +0x2428).
    alignas(16) u8 walker[0x2430];
    s_qinit(walker, map, 0xbf800000u /* -1.0f bits */, 0x3f800000u /* +1.0f bits */);

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
        if (!s_boxIntersect(qbox, nbox)) { (*top)--; continue; }

        int first = *(int*)(*(uintptr_t*)((uintptr_t)map + 0x30) + (uintptr_t)nodeId * 4);
        if (first >= 0) {
            uintptr_t recs = *(uintptr_t*)((uintptr_t)map + 0x10);
            int idx = first;
            do {
                uintptr_t linkIBP = *(uintptr_t*)(recs + (uintptr_t)idx * 0x18);

                bool accept = true;
                if (typeId != 0) {
                    accept = false;
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&linkIBP);
                    uintptr_t entity = *(uintptr_t*)(linkIBP + 0x10);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
                    int compCount = *(int*)(entity + 0x38);
                    uintptr_t comps = *(uintptr_t*)(entity + 0x30);
                    for (int c = 0; c < compCount; c++) {
                        void* compSlot = (void*)(comps + (uintptr_t)c * 8);
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(compSlot);
                        uintptr_t comp = *(uintptr_t*)compSlot;
                        typedef int (*fn_GetTypeId_t)(void* self);
                        fn_GetTypeId_t getId =
                            *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
                        if (getId((void*)comp) == typeId) { accept = true; break; }
                    }
                }

                if (accept) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&linkIBP);
                    uintptr_t link = linkIBP;
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(link + 0x68));
                    uintptr_t model = *(uintptr_t*)(link + 0x68);
                    float diff[3];
                    s_v3ctor3(diff, *(float*)(model + 0x158) - pos[0],
                                    *(float*)(model + 0x15c) - pos[1],
                                    *(float*)(model + 0x160) - pos[2]);
                    float dist2 = 0.0f;
                    s_v3dot(diff, &dist2, diff);
                    if (dist2 < r2) {
                        // inline 8-byte grow-array push (×3 capacity rule)
                        int count = *(int*)((uintptr_t)arr + 8);
                        uintptr_t oldData = *arr;
                        *(int*)((uintptr_t)arr + 8) = count + 1;
                        int* capPtr = (int*)((uintptr_t)arr + 0xc);
                        if (*capPtr < count + 1) {
                            *capPtr = count + 1;
                            if (count != 0) *capPtr = (count + 1) * 3;
                            *arr = (uintptr_t)fn_MemAlloc(g_pMemAllocSingleton,
                                                          (uint64_t)*capPtr * 8, 8);
                        }
                        uintptr_t newData = *arr;
                        int start = (oldData == newData) ? count : 0;
                        for (int i = start; i <= count; i++) {
                            *(uintptr_t*)(newData + (uintptr_t)i * 8) =
                                (i < count) ? *(uintptr_t*)(oldData + (uintptr_t)i * 8)
                                            : linkIBP;
                        }
                        if (oldData != newData && oldData) fn_MemFreeStatic((void*)oldData);
                    }
                }

                idx = *(int*)(recs + (uintptr_t)idx * 0x18 + 8);
            } while (idx != first);
        }

        s_qsubdiv(walker);   // pop node / push its children
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
    if (!self || !disease || !gw) return;

    if (*(int64_t*)(s + 0x68) != 0) return;
    if (*(int64_t*)(s + 0x70) > 0) return;
    uintptr_t p1f0 = *(uintptr_t*)(s + 0x1f0);
    if (p1f0 && *(char*)(p1f0 + 0x4d) != 0 && *(int64_t*)(s + 0x248) != 0) return;

    void* rng = *(void**)((uintptr_t)gw + 0xd8);

    if (!CitizenComp_AnyCitizenSick(*(void**)((uintptr_t)gw + 0xf0))) {
        // Post "outbreak of {disease}" via the registered AddCitizenEvent
        // (0x140094e00): identical entry to the original inline block — color
        // 0xffff6060 (sev 3), type 0xb, icon 1, popup -1, tile -1 (null entity) —
        // including the drop-to-1x-speed tail on the settings +0x17 byte.
        void* eventLog = *(void**)((uintptr_t)gw + 0x138);
        void* nameTable = *(void**)((uintptr_t)disease + 0x20);
        u8 diseaseName[8] = {0};
        bool haveName = false;
        if (nameTable && s_getTextByName) {
            s_getTextByName(nameTable, diseaseName, (u8*)disease + 0x28);
            haveName = true;
        }
        if (s_addCitizenEvent && eventLog) {
            // {3}/{4} unused by the type-0xb template; AddCitizenEvent maps null -> "".
            s_addCitizenEvent(eventLog, 0xb, 3, nullptr,
                              haveName ? diseaseName : nullptr, nullptr, nullptr);
        }
        if (haveName && fn_String_Dtor) fn_String_Dtor(diseaseName);
    }

    *(void**)(s + 0x68) = disease;
    u8* desc = *(u8**)(s + 0x288);
    float tol = fn_Random_RandFloatTol(rng);   // real fn (engine_funcs shim), not a ptr
    *(int64_t*)(s + 0x70) = TimeFromSeconds(
        (tol * *(float*)(desc + 0x98) + *(float*)(desc + 0x94)) * SecsPerMonth());
    *(int64_t*)(s + 0x78) = TimeFromSeconds(
        *(float*)((uintptr_t)disease + 0x30) * SecsPerMonth());
    if (s_findWorkplace) s_findWorkplace(self);
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
    if (!self || !gw || !fn_IsDerivedFrom || !fn_ExternalList_GetResourceCount ||
        !fn_Random_RandFloat || s_tidDisease == 0)
        return;
    if (CitizenComp_AnyCitizenSick(*(void**)((uintptr_t)gw + 0xf0))) return;

    void* rng = *(void**)((uintptr_t)gw + 0xd8);
    void* list = *(void**)((uintptr_t)gw + 0xa8);
    if (!list) return;
    int count = fn_ExternalList_GetResourceCount(list);

    struct { uintptr_t data; int count; int cap; } cum = { 0, 0, 0 };
    float total = 0.0f;
    for (int i = 0; i < count; i++) {
        uintptr_t entry = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)list + 0x20) +
                                        (uintptr_t)i * 8);
        void* d = fn_IsDerivedFrom((void*)entry, s_tidDisease) ? (void*)entry : nullptr;
        total += *(float*)((uintptr_t)d + 0x3c);   // faithful: NULL deref if not Disease
        uint32_t bits; memcpy(&bits, &total, 4);
        CitizenComp_IntArrayResize(&cum, cum.count + 1, &bits);
    }

    float target = fn_Random_RandFloat(rng) * total;
    float prev = 0.0f;
    for (int i = 0; i < count; i++) {
        float c = ((float*)cum.data)[i];
        if (prev <= target && target < c) {
            uintptr_t entry = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)list + 0x20) +
                                            (uintptr_t)i * 8);
            void* d = fn_IsDerivedFrom((void*)entry, s_tidDisease) ? (void*)entry : nullptr;
            CitizenComp_Infect(self, d);
            break;
        }
        prev = c;
    }
    if (cum.data) fn_MemFreeStatic((void*)cum.data);
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
    if (!self || !gw || !fn_Random_RandFloat) return;
    void* disease = *(void**)(s + 0x68);
    if (!disease) return;

    void* rng = *(void**)((uintptr_t)gw + 0xd8);
    if (fn_Random_RandFloat(rng) > *(float*)((uintptr_t)disease + 0x34)) return;

    float radius = *(float*)((uintptr_t)disease + 0x38);
    uint32_t typeId = s_tidCitizen;
    if (g_TypeId_CitizenComponent_FC && *g_TypeId_CitizenComponent_FC)
        typeId = *g_TypeId_CitizenComponent_FC;   // original reads DAT_1401c7158
    if (typeId == 0) return;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(s + 0x278);
    uintptr_t link = *(uintptr_t*)(s + 0x278);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(link + 0x68));
    uintptr_t model = *(uintptr_t*)(link + 0x68);
    float pos[3] = { *(float*)(model + 0x158), *(float*)(model + 0x15c),
                     *(float*)(model + 0x160) };

    struct { uintptr_t data; int count; int cap; } found = { 0, 0, 0 };
    void* scratch = nullptr;
    Map_RadiusQuery_92990(*(float**)((uintptr_t)gw + 0x88), &scratch, pos, &radius,
                          (int)typeId, &found);

    for (int i = 0; i < found.count; i++) {
        void* slot = (void*)(found.data + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        uintptr_t entity = *(uintptr_t*)(*(uintptr_t*)slot + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(s + 0x10);
        if (entity == *(uintptr_t*)(s + 0x10)) continue;   // skip self

        int64_t entityIBP = (int64_t)entity;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityIBP);
        int64_t citizenIBP = 0;
        Entity_FindComponentByExeTypeId((void*)entityIBP, &citizenIBP, &typeId);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&citizenIBP);
        if (citizenIBP && *(int64_t*)((uintptr_t)citizenIBP + 0x68) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&citizenIBP);
            CitizenComp_Infect((void*)citizenIBP, disease);
            break;
        }
    }
    fn_MemFreeStatic((void*)found.data);   // original frees even when null
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
    if (!self || !gw || !fn_Random_RandFloat) return 1;

    float r = fn_Random_RandFloat(*(void**)((uintptr_t)gw + 0xd8));
    u8* desc = *(u8**)(s + 0x288);
    float mult = (*(int64_t*)(s + 0x228) != 0) ? *(float*)(desc + 0x128) : 1.0f;
    void* disease = *(void**)(s + 0x68);
    float deathChance = (1.0f / *(float*)((uintptr_t)disease + 0x40)) / mult;

    if (deathChance >= r) {                     // COMISS deathChance,r; !JC
        if (s_releaseLink) s_releaseLink(self);
        return 0;                               // dead
    }
    *(int64_t*)(s + 0x78) = 0;
    *(int64_t*)(s + 0x70) = TimeFromSeconds(*(float*)(desc + 0x9c) * SecsPerMonth());
    *(int64_t*)(s + 0x68) = 0;
    if (s_releaseLink) s_releaseLink(self);
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

        if (!(B > 0.0f)) {                       // COMISS 0,B; !JC (B <= 0 or NaN)
            *(int64_t*)(s + 0x60) = 0;
            return 1;
        }
        float elapsed = fn_Time_AsSeconds ? fn_Time_AsSeconds(s + 0x60) : 0.0f;
        *(int64_t*)(s + 0x60) = 0;
        float denom = A * MonthsPerYear() * SecsPerMonth() * B;
        float ratio = elapsed / denom;
        float r = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 1.0f;
        if (ratio > r) CitizenComp_CatchDisease(self);
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
