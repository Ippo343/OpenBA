// standalone/school_soak_probe.cpp
//
// BANISHED_SCHOOL_SOAK=<n frames> — the ORGANIC-ENROLMENT watch.
//
// School enrolment is not a per-tick service: CitizenComponent::TryEnrolInSchool
// (0x140111840, long misnamed TryFindPartner) has exactly ONE caller, the child->adult
// edge in CitizenComponent::Update — `!isAdult && adultAge <= newAge && +0x208 == 0`.
// So an empty school proves nothing unless somebody actually grew up during the run, and
// a 60-second soak with twelve minors in town produces zero enrolments while being
// completely healthy. That ambiguity is what this probe removes: it reports the age
// distribution, the closing gap to the adult threshold, and the measured aging rate, so
// "not yet" and "broken" stop looking alike.
//
// Every n frames it prints:
//   pop / minors / already-enrolled(+0x208) / adults
//   the OLDEST minor's age and its remaining gap to adultAge (desc+0x44)
//   the observed age delta since the last report, and the frames-to-adult ETA it implies
//   every AttendanceComponent's queue depth out of the town registry (townMgr+0x60)
//
// The ETA is the number that makes a soak plannable — without it, choosing a run length
// is guesswork. Enrolments themselves are announced the moment the count moves.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

int   s_interval = -2;         // -2 = env unread, -1 = disabled
int   s_lastFrame = 0;
float s_lastOldestMinorAge = -1.0f;
int   s_lastEnrolled = -1;

void Init() {
    if (s_interval != -2) return;
    const char* e = std::getenv("BANISHED_SCHOOL_SOAK");
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : 1800;
    std::fprintf(stderr, "[SchoolSoak] armed: age/enrolment census every %d frames\n", s_interval);
    std::fflush(stderr);
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

}  // namespace

extern "C" void SchoolSoakProbe_Tick(int frameNum) {
    Init();
    if (s_interval <= 0 || frameNum == 0 || (frameNum % s_interval) != 0) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t tm = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xf0);
    if (!tm) return;
    const uintptr_t czArr = *reinterpret_cast<uintptr_t*>(tm + 0xa0);
    const int czN = *reinterpret_cast<int32_t*>(tm + 0xa8);

    int pop = 0, minors = 0, enrolled = 0, adults = 0;
    float oldestMinor = -1.0f, adultAge = -1.0f, agingRate = -1.0f;
    for (int i = 0; i < czN && czArr; ++i) {
        void** cs = reinterpret_cast<void**>(czArr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        const uintptr_t cz = reinterpret_cast<uintptr_t>(*cs);
        if (!cz) continue;
        ++pop;
        const uintptr_t rec = *reinterpret_cast<uintptr_t*>(cz + 0x288);
        if (!rec) continue;
        const float age = *reinterpret_cast<float*>(cz + 0x3c);
        adultAge  = *reinterpret_cast<float*>(rec + 0x44);
        agingRate = *reinterpret_cast<float*>(rec + 0x40);
        if (*reinterpret_cast<uint64_t*>(cz + 0x208)) ++enrolled;
        if (age < adultAge) { ++minors; if (age > oldestMinor) oldestMinor = age; }
        else ++adults;
    }

    // Age velocity, measured rather than derived: dt_sec * desc+0x40 per sim iteration,
    // and the iteration count per frame is the speed multiplier, so the only honest
    // number is the observed one.
    // ⚠ The oldest minor's age can go DOWN between reports, and that is the healthy case:
    // it means the previous holder crossed the adult edge and a younger child inherited
    // the title. Treat a drop as evidence of a graduation, not as a bad sample.
    const int dFrames = frameNum - s_lastFrame;
    float perFrame = -1.0f;
    bool graduated = false;
    if (s_lastOldestMinorAge >= 0.0f && oldestMinor >= 0.0f && dFrames > 0) {
        if (oldestMinor >= s_lastOldestMinorAge)
            perFrame = (oldestMinor - s_lastOldestMinorAge) / static_cast<float>(dFrames);
        else
            graduated = true;
    }

    std::fprintf(stderr, "[SchoolSoak] frame %d: pop=%d adults=%d minors=%d enrolled=%d  "
                 "adultAge=%.2f agingRate=%.4f/s\n",
                 frameNum, pop, adults, minors, enrolled, adultAge, agingRate);
    if (minors > 0 && oldestMinor >= 0.0f) {
        const float gap = adultAge - oldestMinor;
        if (perFrame > 0.0f)
            std::fprintf(stderr, "[SchoolSoak]   oldest minor age=%.3f gap=%.3f  "
                         "measured %.5f age/frame -> ETA %.0f frames to the adult edge\n",
                         oldestMinor, gap, perFrame, gap / perFrame);
        else
            std::fprintf(stderr, "[SchoolSoak]   oldest minor age=%.3f gap=%.3f  %s\n",
                         oldestMinor, gap,
                         graduated ? "(the previous oldest minor crossed the adult edge — a "
                                     "younger child now leads; no velocity from this sample)"
                                   : "(no age velocity yet — next report gives the ETA)");
    } else if (minors == 0) {
        std::fprintf(stderr, "[SchoolSoak]   no minors in town: enrolment fires only on the "
                     "child->adult edge, so nothing can enrol until a birth grows up\n");
    }

    // Every attendance building's queue, straight out of the registry sub_1930 scans.
    const uintptr_t bArr = *reinterpret_cast<uintptr_t*>(tm + 0x60);
    const int bN = *reinterpret_cast<int32_t*>(tm + 0x68);
    const uint32_t tidWpc = Tid("WorkPlaceComponent");
    for (int i = 0; i < bN && bArr && i < 8; ++i) {
        void** bs = reinterpret_cast<void**>(bArr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(bs);
        const uintptr_t b = reinterpret_cast<uintptr_t>(*bs);
        if (!b) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(b + 0x10));
        void* sub = *reinterpret_cast<void**>(b + 0x10);
        void* wpc = FindComp(sub, tidWpc);
        const int staff = wpc ? *reinterpret_cast<int32_t*>(
                                reinterpret_cast<uintptr_t>(wpc) + 0x60) : -1;
        std::fprintf(stderr, "[SchoolSoak]   attendance[%d] comp=%p queue(+0x30)=%d staff=%d%s\n",
                     i, reinterpret_cast<void*>(b), *reinterpret_cast<int32_t*>(b + 0x30), staff,
                     staff == 0 ? "  <-- UNSTAFFED: enrolment refuses an unstaffed building" : "");
    }

    if (s_lastEnrolled >= 0 && enrolled > s_lastEnrolled)
        std::fprintf(stderr, "[SchoolSoak]   *** ORGANIC ENROLMENT: %d -> %d students; the "
                     "child->adult edge fired on its own ***\n", s_lastEnrolled, enrolled);

    s_lastFrame = frameNum;
    s_lastOldestMinorAge = oldestMinor;
    s_lastEnrolled = enrolled;
    std::fflush(stderr);
}
