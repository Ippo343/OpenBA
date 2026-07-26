// standalone/death_harness.cpp
// Diagnostic driver: kill N citizens on a chosen frame so the removal path
// (HandleLifeEvent -> MarkForRemoval -> the +0x38 broadcast -> CitizenComponent::
// OnRemove) can be exercised deterministically. Natural deaths are decades of game
// time away in a fresh start-town, which makes the citizen-removal teardown
// effectively untestable otherwise.
//
// Same shape as tool_harness.cpp: env-gated, one-shot, ticked from
// GameUpdateLoop::Update. No-op unless BANISHED_KILL_CITIZENS is set.
//
//   BANISHED_KILL_CITIZENS=<n>   how many citizens to kill (0 / unset = disabled)
//   BANISHED_KILL_AT_FRAME=<f>   frame to fire on (default 900)
//   BANISHED_FAMILY_CENSUS=<f>   every f frames, report how many citizens carry family
//                                links — spouse +0x250, ring head +0x260, partner +0x208
//   BANISHED_KILL_FAMILY=<n>     poll until citizens with a family link EXIST, then kill
//                                up to n of them. This is the only practical way to reach
//                                CitizenComponent::OnRemove's step-4 family branch: the
//                                start town begins with every link null, so an untargeted
//                                kill (or a natural death) exercises the empty path.
//
// Pair it with BANISHED_LIVE_CITIZEN_ONREMOVE=1 and BANISHED_ONREMOVE_TRACE=1 to see
// each teardown, and with BANISHED_OBJAUDIT=1 to watch the object tally.
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" uint64_t CitizenComponent_HandleLifeEvent(void*, int, const char*); // 0x140116dd0
// CitizenComponent_sub_1fa0 (0x140111fa0) — re-parent a citizen: detach from the current
// guardian's family ring (+0x258 / the host's +0x260 ring) and attach to the new one.
// Passing a null IBP is exactly what CitizenComponent::Update does when a grown child
// finds its own home and moves out.
extern void CitizenComp_sub_1fa0(int64_t selfV, void* param2);
// CitizenComponent_AssignWorkplace (0x1401163d0) — MISNAMED in retail: this is the
// RESIDENCE assigner (sets +0x238 = home and rosters the citizen into the house's
// ResidenceComponent). Update calls it right after the move-out with the house sub_0e70
// found; the harness calls it with the house the citizen already lived in.
extern "C" void CitizenComponent_AssignWorkplace(void* self, void* workplaceIBP, int slotIndex);

namespace {
int  s_count       = -1;   // -1 = not yet read from the environment
int  s_atFrame     = 900;
int  s_censusEvery = 0;
int  s_killFamily  = 0;
int  s_marryHarness = 0;
int  s_birthTrace   = 0;   // BANISHED_BIRTH_TRACE — per-citizen detail lines for the birth gate
int  s_moved        = 0;   // citizens the marriage harness has made independent so far
bool s_fired       = false;
bool s_firedFamily = false;
bool s_firedMarry  = false;

void Init() {
    if (s_count >= 0) return;
    const char* n = std::getenv("BANISHED_KILL_CITIZENS");
    s_count = (n && n[0]) ? std::atoi(n) : 0;
    const char* f = std::getenv("BANISHED_KILL_AT_FRAME");
    if (f && f[0]) s_atFrame = std::atoi(f);
    const char* c = std::getenv("BANISHED_FAMILY_CENSUS");
    s_censusEvery = (c && c[0]) ? std::atoi(c) : 0;
    const char* kf = std::getenv("BANISHED_KILL_FAMILY");
    s_killFamily = (kf && kf[0]) ? std::atoi(kf) : 0;
    const char* mh = std::getenv("BANISHED_MARRY_HARNESS");
    s_marryHarness = (mh && mh[0]) ? std::atoi(mh) : 0;
    const char* bt = std::getenv("BANISHED_BIRTH_TRACE");
    s_birthTrace = (bt && bt[0]) ? std::atoi(bt) : 0;
    if (s_birthTrace > 0)
        std::fprintf(stderr, "[BirthGate] ENABLED — walking the reproduction gate at each "
                     "census, up to %d per-citizen detail line(s)\n", s_birthTrace);
    if (s_count > 0) {
        std::fprintf(stderr, "[DeathHarness] ENABLED — killing %d citizen(s) at frame %d\n",
                     s_count, s_atFrame);
    }
    if (s_censusEvery > 0)
        std::fprintf(stderr, "[DeathHarness] family census every %d frames\n", s_censusEvery);
    if (s_killFamily > 0)
        std::fprintf(stderr, "[DeathHarness] will kill %d citizen(s) WITH family links "
                     "once any exist\n", s_killFamily);
    if (s_marryHarness > 0)
        std::fprintf(stderr, "[MarryHarness] ENABLED — will move up to %d grown, unmarried "
                     "citizen(s) out of their guardian's household once any are old enough\n",
                     s_marryHarness);
    std::fflush(stderr);
}

// The town citizen list: array at townMgr+0xa0, count at +0xa8.
bool TownList(int64_t** outList, int* outCount, int64_t* outTownMgr) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);
    if (!townMgr) return false;
    *outTownMgr = townMgr;
    *outList  = *reinterpret_cast<int64_t**>(townMgr + 0xa0);
    *outCount = *reinterpret_cast<int*>(townMgr + 0xa8);
    return *outList != nullptr && *outCount > 0;
}

inline bool HasFamily(int64_t c) {
    return c && (*reinterpret_cast<int64_t*>(c + 0x250) != 0 ||
                 *reinterpret_cast<int64_t*>(c + 0x260) != 0);
}

// ---- Birth-gate walk (BANISHED_BIRTH_TRACE=<n>) ------------------------------------
// Open A of the next-session brief: the town has couples but zero births, and the
// reproduction timer (+0x4C) only ever counts down — never gets seeded. The seed lives in
// CitizenComponent::Update behind a chain of tests, and "no births" alone does not say
// which one nobody passes. This walks the SAME chain, in the same order, read-only, and
// reports the first test each female fails. It calls nothing that mutates state: the two
// resolved helpers (FindComponent_Residence, helper_a4f0) are both pure queries.
//
// The chain, from Update's LAB_140113873 tail:
//   1. +0x38 == 0            female
//   2. +0x3c >= desc+0x44    adult
//   3. +0x208 == 0           not a student/partner-in-training
//   4. NOT hungry/cold       hunger(+0x44) <= warn, warmth(+0x48) <= warn — a citizen over
//                            either warn level has its +0x4C ZEROED every tick, so the
//                            timer can never run down to fire
//   5. +0x250 != 0           married
//   6. *(+0x238) != 0        housed
//   7. +0x4C == 0            timer idle (a NEGATIVE +0x4C is the post-birth cooldown)
//   8. desc+0x50 <= age <= desc+0x54    within the childbearing window
//   9. residence component found off the home entity
//  10. !helper_a4f0          household not full: desc+0x24 > slot[+0x240].count
// Passing all ten is what seeds +0x4C = desc+0x58 and starts the countdown to a birth.
//
// n = how many per-citizen detail lines to print (the aggregate always prints when set).
typedef void* (*fn_FindCompResidence_t)(int64_t entity, void* outIBP);
typedef bool  (*fn_HouseholdFull_t)(int64_t residenceComp, int slotIdx);
fn_FindCompResidence_t s_fnFindResidence = nullptr;
fn_HouseholdFull_t     s_fnHouseholdFull = nullptr;
float* s_pHungerFrac = nullptr;   // DAT_140172114
float* s_pWarmthFrac = nullptr;   // DAT_140172100

const char* const kGateName[12] = {
    "PASSES-ALL",       // 0
    "male",             // 1
    "child",            // 2
    "student(+0x208)",  // 3
    "hungry/cold",      // 4
    "unmarried",        // 5
    "homeless",         // 6
    "timerBusy",        // 7
    "ageWindow",        // 8
    "noResidence",      // 9
    "householdFull",    // 10
    "noDesc",           // 11
};

void BirthGate(int frameNum, int64_t* list, int n) {
    if (s_birthTrace <= 0) return;
    if (!s_fnFindResidence) {
        // 0x140008090 is Entity::FindComponent<ResidenceComponent> (NOT storage — 0x140006270
        // is the storage one; the two are a standing naming trap in this codebase).
        s_fnFindResidence = reinterpret_cast<fn_FindCompResidence_t>(
            ResolveAddr(Module::Exe, 0x140008090));
        s_fnHouseholdFull = reinterpret_cast<fn_HouseholdFull_t>(
            ResolveAddr(Module::Exe, 0x14012a4f0));
        s_pHungerFrac = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
        s_pWarmthFrac = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172100));
    }
    const float hungerFrac = s_pHungerFrac ? *s_pHungerFrac : 0.5f;
    const float warmthFrac = s_pWarmthFrac ? *s_pWarmthFrac : 0.5f;

    int tally[12] = {0};
    int printed = 0;
    // Thresholds read off the first citizen with a description, so the numbers have a scale.
    float hWarn = 0.0f, wWarn = 0.0f, bearMin = 0.0f, bearMax = 0.0f;
    float timerSeed = 0.0f, occupantCap = 0.0f;
    bool  haveDesc = false;

    for (int i = 0; i < n; i++) {
        int64_t c = list[i];
        if (!c) continue;
        int64_t desc = *reinterpret_cast<int64_t*>(c + 0x288);
        if (!desc) { tally[11]++; continue; }

        const float hungerWarn = (*reinterpret_cast<float*>(desc + 0xD0) -
                                  *reinterpret_cast<float*>(desc + 0xCC)) * hungerFrac +
                                 *reinterpret_cast<float*>(desc + 0xCC);
        const float warmthWarn = (*reinterpret_cast<float*>(desc + 0xF0) -
                                  *reinterpret_cast<float*>(desc + 0xEC)) * warmthFrac +
                                 *reinterpret_cast<float*>(desc + 0xEC);
        if (!haveDesc) {
            haveDesc = true;
            hWarn = hungerWarn; wWarn = warmthWarn;
            bearMin = *reinterpret_cast<float*>(desc + 0x50);
            bearMax = *reinterpret_cast<float*>(desc + 0x54);
            timerSeed = *reinterpret_cast<float*>(desc + 0x58);
            // The occupant cap lives on the RESIDENCE description, not the citizen's —
            // filled in below from the first residence we successfully resolve.
        }

        const float age    = *reinterpret_cast<float*>(c + 0x3c);
        const float hunger = *reinterpret_cast<float*>(c + 0x44);
        const float warmth = *reinterpret_cast<float*>(c + 0x48);
        const float timer  = *reinterpret_cast<float*>(c + 0x4C);
        const int64_t home = *reinterpret_cast<int64_t*>(c + 0x238);
        const int   slotIdx = *reinterpret_cast<int32_t*>(c + 0x240);

        int gate = 0;
        int64_t residence = 0;
        if (*reinterpret_cast<int32_t*>(c + 0x38) != 0)                gate = 1;
        else if (age < *reinterpret_cast<float*>(desc + 0x44))         gate = 2;
        else if (*reinterpret_cast<int64_t*>(c + 0x208) != 0)          gate = 3;
        else if (hungerWarn < hunger || warmthWarn < warmth)           gate = 4;
        else if (*reinterpret_cast<int64_t*>(c + 0x250) == 0)          gate = 5;
        else if (home == 0)                                            gate = 6;
        else if (timer != 0.0f)                                        gate = 7;
        else if (age < *reinterpret_cast<float*>(desc + 0x50) ||
                 age > *reinterpret_cast<float*>(desc + 0x54))         gate = 8;
        else {
            int64_t outIBP = 0;
            void* res = s_fnFindResidence ? s_fnFindResidence(home, &outIBP) : nullptr;
            residence = res ? *reinterpret_cast<int64_t*>(res) : 0;
            if (!residence)                                            gate = 9;
            else if (s_fnHouseholdFull && s_fnHouseholdFull(residence, slotIdx)) gate = 10;
        }
        tally[gate]++;

        // Detail lines: the interesting citizens are the ones deep in the chain, so print
        // anything past the married gate first — a wall of "male"/"child" says nothing.
        if (printed < s_birthTrace && (gate == 0 || gate >= 4)) {
            printed++;
            float cap = -1.0f;
            if (residence) {
                int64_t rdesc = *reinterpret_cast<int64_t*>(residence + 0x18);
                if (rdesc) {
                    cap = static_cast<float>(*reinterpret_cast<int32_t*>(rdesc + 0x24));
                    if (occupantCap == 0.0f) occupantCap = cap;
                }
            }
            std::fprintf(stderr,
                "[BirthGate]   c=%p BLOCKED-AT=%s age=%.2f hunger=%.1f/%.1f warmth=%.1f/%.1f "
                "timer(+0x4C)=%.2f score(+0x50)=%.2f spouse=%d home=%d slot=%d occupantCap=%.0f\n",
                reinterpret_cast<void*>(c), kGateName[gate], age, hunger, hungerWarn,
                warmth, warmthWarn, timer, *reinterpret_cast<float*>(c + 0x50),
                *reinterpret_cast<int64_t*>(c + 0x250) != 0 ? 1 : 0,
                home != 0 ? 1 : 0, slotIdx, cap);
        }
    }

    std::fprintf(stderr,
        "[BirthGate] frame %d: PASSES-ALL=%d | male=%d child=%d student=%d hungry/cold=%d "
        "unmarried=%d homeless=%d timerBusy=%d ageWindow=%d noResidence=%d householdFull=%d\n",
        frameNum, tally[0], tally[1], tally[2], tally[3], tally[4], tally[5], tally[6],
        tally[7], tally[8], tally[9], tally[10]);
    std::fprintf(stderr,
        "[BirthGate]   thresholds: hungerWarn=%.1f warmthWarn=%.1f (frac %.2f/%.2f) "
        "bearingAge=[%.2f,%.2f] timerSeed(desc+0x58)=%.2f occupantCap(rdesc+0x24)=%.0f\n",
        hWarn, wWarn, hungerFrac, warmthFrac, bearMin, bearMax, timerSeed, occupantCap);
    // The roll itself, so a failed attempt is interpretable. Update computes
    //   pSuccess = (score(+0x50) / desc+0x70) * (desc+0x64 - desc+0x68) + desc+0x68
    // and a birth happens when RandFloat() < pSuccess; otherwise +0x4C is reseeded to
    // -(rand * desc+0x60 + desc+0x5C), the post-attempt cooldown.
    if (haveDesc) {
        for (int i = 0; i < n; i++) {
            int64_t c = list[i];
            int64_t d = c ? *reinterpret_cast<int64_t*>(c + 0x288) : 0;
            if (!d) continue;
            std::fprintf(stderr,
                "[BirthGate]   roll consts: chanceLow(desc+0x68)=%.4f chanceHigh(desc+0x64)=%.4f "
                "scoreMax(desc+0x70)=%.2f cooldownBase(desc+0x5C)=%.1f range(desc+0x60)=%.1f\n",
                *reinterpret_cast<float*>(d + 0x68), *reinterpret_cast<float*>(d + 0x64),
                *reinterpret_cast<float*>(d + 0x70), *reinterpret_cast<float*>(d + 0x5C),
                *reinterpret_cast<float*>(d + 0x60));
            break;
        }
    }
    std::fflush(stderr);
}

void Census(int frameNum) {
    int64_t* list; int n; int64_t townMgr;
    if (!TownList(&list, &n, &townMgr)) {
        // TownList fails when the citizen array is null OR the count is 0 — i.e. an EXTINCT
        // town looks exactly like "no census at all". Silence then reads as a hung process
        // (it did, 2026-07-25: a FORCE_SPEED=40 soak went quiet and three gdb samples inside
        // the DX11 Present path were misread as a stall, when the town had simply died out —
        // RTSS's frame limiter blocks in Present, so that is where an IDLE run always samples).
        // Say it out loud instead.
        std::fprintf(stderr, "[FamilyCensus] frame %d: NO CITIZENS — town list is empty or "
                     "unavailable (extinct town, or the world is not built yet)\n", frameNum);
        std::fflush(stderr);
        return;
    }
    int spouse = 0, ring = 0, partner = 0, host = 0;
    // Gender (+0x38): 0 = female, non-zero = male. Retail's per-citizen init writes it for
    // every colonist; the founding spawner reproduces it from the mesh variant. A town that
    // reports female == citizens is one where nobody can marry (the opposite-gender test in
    // helper_fe40 can never pass) and every adult passes the reproduction gate — the exact
    // symptom of the gender write being skipped. Tracked here so the A/B is measurable.
    int female = 0, male = 0;
    // Marriage-gate breakdown. CitizenComponent::Update's partner block (the tail at
    // 0x140113827) only calls the finder when ALL of these hold:
    //     +0x258 (guardian/host citizen) == 0
    //     +0x238 (home) != 0
    //     +0x250 (spouse) == 0
    //     +0x3c (age) >= desc+0x48 (marriage age)
    // A soak that reports "0 marriages" is uninformative without knowing which of the
    // four is the one nobody passes, so count them separately. `eligible` is the number
    // of citizens that actually reach the finder on a given tick.
    int adults = 0, children = 0;
    int overMarryAge = 0, unmarried = 0, noGuardian = 0, hasHome = 0, eligible = 0;
    float youngest = 1e9f, oldest = -1e9f, marryAge = -1.0f, adultAge = -1.0f;
    // Survival state. The standalone town has no food economy: houses are seeded with a
    // fixed stock and nothing replaces it, so hunger (+0x44) and thirst (+0x48) climb until
    // the whole cohort starves at once. Without these, a soak just ends and the census goes
    // quiet — which is exactly how the 2026-07-25 die-off was first misread as a hang.
    float hungerMax = 0.0f, hungerSum = 0.0f, thirstMax = 0.0f, thirstSum = 0.0f;
    int   sick = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = list[i];
        if (!c) continue;
        bool noSpouse = *reinterpret_cast<int64_t*>(c + 0x250) == 0;
        if (!noSpouse) spouse++; else unmarried++;
        if (*reinterpret_cast<int64_t*>(c + 0x260) != 0) ring++;
        if (*reinterpret_cast<int64_t*>(c + 0x208) != 0) partner++;
        bool guardianFree = *reinterpret_cast<int64_t*>(c + 0x258) == 0;
        if (!guardianFree) host++;
        if (*reinterpret_cast<int32_t*>(c + 0x38) == 0) female++; else male++;

        float hunger = *reinterpret_cast<float*>(c + 0x44);
        float thirst = *reinterpret_cast<float*>(c + 0x48);
        hungerSum += hunger; thirstSum += thirst;
        if (hunger > hungerMax) hungerMax = hunger;
        if (thirst > thirstMax) thirstMax = thirst;
        if (*reinterpret_cast<int64_t*>(c + 0x68) != 0) sick++;

        int64_t desc = *reinterpret_cast<int64_t*>(c + 0x288);
        if (!desc) continue;
        float age = *reinterpret_cast<float*>(c + 0x3c);
        adultAge  = *reinterpret_cast<float*>(desc + 0x44);
        marryAge  = *reinterpret_cast<float*>(desc + 0x48);
        if (age < youngest) youngest = age;
        if (age > oldest)   oldest   = age;
        if (age >= adultAge) adults++; else children++;
        if (!noSpouse) continue;                 // already married: not a candidate
        bool old_enough = age >= marryAge;
        bool home       = *reinterpret_cast<int64_t*>(c + 0x238) != 0;
        if (old_enough)   overMarryAge++;
        if (guardianFree) noGuardian++;
        if (home)         hasHome++;
        if (old_enough && guardianFree && home) eligible++;
    }
    static int s_prevN = -1;
    int delta = (s_prevN < 0) ? 0 : n - s_prevN;
    s_prevN = n;
    std::fprintf(stderr, "[FamilyCensus] frame %d: %d citizens (%+d) — spouse(+0x250)=%d "
                 "ring(+0x260)=%d partner(+0x208)=%d host(+0x258)=%d "
                 "gender(+0x38) female=%d male=%d\n",
                 frameNum, n, delta, spouse, ring, partner, host, female, male);
    // A grown child can only stop being someone's dependant (+0x258) by finding a home of
    // its OWN: CitizenComponent::Update -> sub_0e70 -> sub_1fa0(self, null). sub_0e70 scans
    // the town building registry at townMgr+0x20/+0x28 and the profession/category BST at
    // townMgr+0x100. Both are populated by WorkPlaceComponent::OnCreate in retail and are
    // empty in standalone's start town, so report them next to the gate counts — an
    // "ELIGIBLE=0 / noGuardian=0" census means nothing without knowing whether the finder
    // it depends on had anything to find.
    int64_t reg   = *reinterpret_cast<int64_t*>(townMgr + 0x20);
    int     regN  = *reinterpret_cast<int32_t*>(townMgr + 0x28);
    int64_t bst   = *reinterpret_cast<int64_t*>(townMgr + 0x100);
    int     bldgN = *reinterpret_cast<int32_t*>(townMgr + 0x68);
    std::fprintf(stderr, "[FamilyCensus]   townMgr=%p buildingRegistry(+0x20)=%p count(+0x28)=%d "
                 "profBST(+0x100)=%p bldgCount(+0x68)=%d\n",
                 reinterpret_cast<void*>(townMgr), reinterpret_cast<void*>(reg), regN,
                 reinterpret_cast<void*>(bst), bldgN);
    std::fprintf(stderr, "[FamilyCensus]   ages: %d adult / %d child (min=%.2f max=%.2f "
                 "adultAge=%.2f marryAge=%.2f) — unmarried=%d of which overMarryAge=%d "
                 "noGuardian=%d hasHome=%d ELIGIBLE=%d\n",
                 adults, children, (youngest > 1e8f ? 0.0f : youngest),
                 (oldest < -1e8f ? 0.0f : oldest), adultAge, marryAge,
                 unmarried, overMarryAge, noGuardian, hasHome, eligible);
    // desc+0xcc/+0xd0 are the hunger warn/death thresholds, +0xec/+0xf0 the thirst ones —
    // read them off the last citizen's description so the numbers have a scale.
    float hungerDeath = 0.0f, thirstDeath = 0.0f;
    for (int i = 0; i < n; i++) {
        int64_t c = list[i];
        int64_t d = c ? *reinterpret_cast<int64_t*>(c + 0x288) : 0;
        if (d) { hungerDeath = *reinterpret_cast<float*>(d + 0xd0);
                 thirstDeath = *reinterpret_cast<float*>(d + 0xf0); break; }
    }
    std::fprintf(stderr, "[FamilyCensus]   survival: hunger avg=%.1f max=%.1f (death %.1f) "
                 "thirst avg=%.1f max=%.1f (death %.1f) sick(+0x68)=%d\n",
                 n ? hungerSum / n : 0.0f, hungerMax, hungerDeath,
                 n ? thirstSum / n : 0.0f, thirstMax, thirstDeath, sick);
    std::fflush(stderr);
    BirthGate(frameNum, list, n);
}
}  // namespace

// ---- Marriage harness (BANISHED_MARRY_HARNESS=<n>) --------------------------------
// The marriage block in CitizenComponent::Update only runs for a citizen with
//   +0x258 (guardian) == 0, +0x238 (home) != 0, +0x250 (spouse) == 0, age >= desc+0x48.
// A citizen born into a family clears the FIRST of those only by finding a home of its
// own — Update -> sub_0e70 -> sub_1fa0(self, null). sub_0e70 scans the town building
// registry (townMgr+0x20/+0x28) and the profession BST (townMgr+0x100).
//
// HISTORY: when this harness was written, both registries were empty in the standalone
// start town, so no citizen could ever move out and the marriage code was untestable.
// The town-registry wave (07155bb, 2026-07-26) fixed that — the residence state-2 enter
// now fills townMgr+0x20 and ORGANIC move-out + marriage are proven in the standing run
// (spouse 12 -> 14 by frame 3000, given a free house via BANISHED_BUILD_DEMO_HOUSE=1).
// The harness stays for tests that need a couple NOW rather than after an organic
// move-out, or without a spare house: it needs neither.
//
// This harness performs the move-out the retail way: sub_1fa0(citizen, nullIBP), the
// same call Update and the marriage block itself make. Leaving the household also drops
// the citizen out of its guardian's house (+0x238 goes to 0), and a homeless citizen
// still fails the marriage gate — so, exactly as Update does one line later, the harness
// re-assigns a home immediately: the very house the citizen already lived in, which
// keeps the residence roster count unchanged. It does NOT touch +0x250 or the age gate;
// everything after this is the game's own code path.
void MoveOutGrownChildren(int frameNum) {
    int64_t* list; int n; int64_t townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    for (int i = 0; i < n && s_moved < s_marryHarness; i++) {
        int64_t c = list[i];
        if (!c) continue;
        if (*reinterpret_cast<int64_t*>(c + 0x250) != 0) continue;   // already married
        if (*reinterpret_cast<int64_t*>(c + 0x258) == 0) continue;   // already independent
        int64_t home = *reinterpret_cast<int64_t*>(c + 0x238);
        if (home == 0) continue;                                     // no home to move back into
        int64_t desc = *reinterpret_cast<int64_t*>(c + 0x288);
        if (!desc) continue;
        float age = *reinterpret_cast<float*>(c + 0x3c);
        if (age < *reinterpret_cast<float*>(desc + 0x48)) continue;  // not old enough yet

        int slot = *reinterpret_cast<int32_t*>(c + 0x240);
        int64_t nullIBP = 0;
        CitizenComp_sub_1fa0(c, &nullIBP);
        int64_t guardian = *reinterpret_cast<int64_t*>(c + 0x258);
        int64_t homeIBP = home;
        CitizenComponent_AssignWorkplace(reinterpret_cast<void*>(c), &homeIBP, slot);
        int64_t homeAfter = *reinterpret_cast<int64_t*>(c + 0x238);
        std::fprintf(stderr, "[MarryHarness] frame %d: moved out comp=%p age=%.2f gender=%d "
                     "— guardian(+0x258) %s, home(+0x238) %p -> %p\n", frameNum,
                     reinterpret_cast<void*>(c), age, *reinterpret_cast<int32_t*>(c + 0x38),
                     guardian == 0 ? "cleared" : "STILL SET",
                     reinterpret_cast<void*>(home), reinterpret_cast<void*>(homeAfter));
        std::fflush(stderr);
        if (guardian == 0 && homeAfter != 0) ++s_moved;
    }
    if (s_moved >= s_marryHarness) {
        s_firedMarry = true;
        std::fprintf(stderr, "[MarryHarness] %d citizen(s) independent and housed — done\n",
                     s_moved);
        std::fflush(stderr);
    }
}

extern "C" void CitizenDeathHarness_Tick(int frameNum) {
    Init();

    if (s_censusEvery > 0 && frameNum > 0 && (frameNum % s_censusEvery) == 0)
        Census(frameNum);

    // Poll until enough citizens have aged past the marriage age, then move them out once.
    if (s_marryHarness > 0 && !s_firedMarry && frameNum > 600 && (frameNum % 600) == 0)
        MoveOutGrownChildren(frameNum);

    // Targeted kill: wait for family links to exist, then take those citizens.
    if (s_killFamily > 0 && !s_firedFamily && frameNum > 300 && (frameNum % 300) == 0) {
        int64_t* list; int n; int64_t townMgr;
        if (TownList(&list, &n, &townMgr)) {
            int64_t victims[16];
            int want = 0;
            for (int i = 0; i < n && want < s_killFamily && want < 16; i++)
                if (HasFamily(list[i])) victims[want++] = list[i];
            if (want > 0) {
                s_firedFamily = true;
                std::fprintf(stderr, "[DeathHarness] frame %d: %d citizen(s) WITH family "
                             "links found — killing them (list=%d entries)\n",
                             frameNum, want, n);
                std::fflush(stderr);
                for (int i = 0; i < want; i++) {
                    int64_t v = victims[i];
                    std::fprintf(stderr, "[DeathHarness] killing family citizen comp=%p "
                                 "spouse=%p ring=%p\n", reinterpret_cast<void*>(v),
                                 *reinterpret_cast<void**>(v + 0x250),
                                 *reinterpret_cast<void**>(v + 0x260));
                    std::fflush(stderr);
                    CitizenComponent_HandleLifeEvent(reinterpret_cast<void*>(v), 3, nullptr);
                }
                std::fprintf(stderr, "[DeathHarness] family kill done — list = %d entries "
                             "(was %d)\n", *reinterpret_cast<int*>(townMgr + 0xa8), n);
                std::fflush(stderr);
            }
        }
    }

    if (s_count <= 0 || s_fired || frameNum < s_atFrame) return;
    s_fired = true;

    int64_t* list; int n; int64_t townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    std::fprintf(stderr, "[DeathHarness] frame %d: town citizen list = %d entries\n",
                 frameNum, n);

    // Snapshot first: each kill tears the victim out of this very list.
    int want = (s_count < n) ? s_count : n;
    int64_t victims[64];
    if (want > 64) want = 64;
    for (int i = 0; i < want; i++) victims[i] = list[i];

    for (int i = 0; i < want; i++) {
        if (!victims[i]) continue;
        std::fprintf(stderr, "[DeathHarness] killing citizen %d/%d comp=%p\n",
                     i + 1, want, reinterpret_cast<void*>(victims[i]));
        std::fflush(stderr);
        CitizenComponent_HandleLifeEvent(reinterpret_cast<void*>(victims[i]), 3, nullptr);
    }

    std::fprintf(stderr, "[DeathHarness] done — town citizen list = %d entries "
                 "(was %d; expect %d if OnRemove is live, unchanged if it is not)\n",
                 *reinterpret_cast<int*>(townMgr + 0xa8), n, n - want);
    std::fflush(stderr);
}
