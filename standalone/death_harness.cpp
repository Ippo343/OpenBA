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
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "options.h"
#include "Game/exe_constants.h"
#include "Entity/CitizenBehavior.h"           // B4: the town lists
#include "Entity/ComponentEntity.h"           // B4: FindComponent<ResidenceComponent>
#include "Component/CitizenComponent.h"       // B4: the citizen fields and members
#include "Component/CitizenDescription.h"     // B4: the description thresholds
#include "Component/ResidenceComponent.h"     // B4: IsFamilyFull
#include "Component/ResidenceDescription.h"   // B4: _maxResidentsPerFamily
#include "Component/CemeteryComponent.h"      // B4: plotCount
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// CitizenComponent_HandleLifeEvent (0x140116dd0) — Component/exe_bodies.h.
// CitizenComponent_sub_1fa0 (0x140111fa0) — re-parent a citizen: detach from the current
// guardian's family ring (+0x258 / the host's +0x260 ring) and attach to the new one.
// Passing a null IBP is exactly what CitizenComponent::Update does when a grown child
// finds its own home and moves out. Declared in Component/exe_bodies.h.
// CitizenComponent_AssignWorkplace (0x1401163d0) — MISNAMED in retail: this is the
// RESIDENCE assigner (sets +0x238 = home and rosters the citizen into the house's
// ResidenceComponent). Update calls it right after the move-out with the house sub_0e70
// found; the harness calls it with the house the citizen already lived in.
// Declared in Component/exe_bodies.h.

namespace {
int  s_count       = -1;   // -1 = not yet read from the environment
int  s_atFrame     = 900;
int  s_censusEvery = 0;
int  s_killFamily  = 0;
int  s_marryHarness = 0;
int  s_birthTrace   = 0;   // BANISHED_BIRTH_TRACE — per-citizen detail lines for the birth gate
int  s_moved        = 0;   // citizens the marriage harness has made independent so far
int  s_forceBirth      = 0;   // BANISHED_FORCE_BIRTH — see the force-birth harness below
int  s_forceBirthFrame = 1200;
bool s_firedBirth      = false;
bool s_fired       = false;
bool s_firedFamily = false;
bool s_firedMarry  = false;

void Init() {
    if (s_count >= 0) return;
    const char* n = Opt().killCitizens;
    s_count = (n && n[0]) ? std::atoi(n) : 0;
    const char* f = Opt().killAtFrame;
    if (f && f[0]) s_atFrame = std::atoi(f);
    const char* c = Opt().familyCensus;
    s_censusEvery = (c && c[0]) ? std::atoi(c) : 0;
    const char* kf = Opt().killFamily;
    s_killFamily = (kf && kf[0]) ? std::atoi(kf) : 0;
    const char* mh = Opt().marryHarness;
    s_marryHarness = (mh && mh[0]) ? std::atoi(mh) : 0;
    const char* bt = Opt().birthTrace;
    s_birthTrace = (bt && bt[0]) ? std::atoi(bt) : 0;
    const char* fb = Opt().forceBirth;
    s_forceBirth = (fb && fb[0]) ? std::atoi(fb) : 0;
    const char* fbf = Opt().forceBirthFrame;
    if (fbf && fbf[0]) s_forceBirthFrame = std::atoi(fbf);
    if (s_forceBirth > 0)
        std::fprintf(stderr, "[BirthHarness] ENABLED — delivering %d baby(ies) at frame %d\n",
                     s_forceBirth, s_forceBirthFrame);
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
bool TownList(CitizenComponent*** outList, int* outCount, CitizenBehavior** outTownMgr) {
    if (GameUpdateLoop::_singleton == nullptr) return false;
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    CitizenBehavior* townMgr = gw->m_citizenList;      // +0xf0
    if (!townMgr) return false;
    *outTownMgr = townMgr;
    *outList  = townMgr->citizens;                     // +0xa0
    *outCount = townMgr->citizenCount;                 // +0xa8
    return *outList != nullptr && *outCount > 0;
}

inline bool HasFamily(CitizenComponent* c) {
    return c && (c->spouse != nullptr ||               // +0x250
                 c->_firstChild != nullptr);           // +0x260
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

void BirthGate(int frameNum, CitizenComponent** list, int n) {
    if (s_birthTrace <= 0) return;
    // DAT_140172114 / DAT_140172100 (.rdata).
    const float hungerFrac = kRdata_140172114;
    const float warmthFrac = kRdata_140172100;

    int tally[12] = {0};
    int printed = 0;
    // Thresholds read off the first citizen with a description, so the numbers have a scale.
    float hWarn = 0.0f, wWarn = 0.0f, bearMin = 0.0f, bearMax = 0.0f;
    float timerSeed = 0.0f, occupantCap = 0.0f;
    bool  haveDesc = false;

    for (int i = 0; i < n; i++) {
        CitizenComponent* c = list[i];
        if (!c) continue;
        const CitizenDescription* desc = c->description;          // +0x288
        if (!desc) { tally[11]++; continue; }

        const float hungerWarn = (desc->maxHunger - desc->_fcc) * hungerFrac +    // +0xD0, +0xCC
                                 desc->_fcc;
        const float warmthWarn = (desc->maxColdness - desc->_fec) * warmthFrac +  // +0xF0, +0xEC
                                 desc->_fec;
        if (!haveDesc) {
            haveDesc = true;
            hWarn = hungerWarn; wWarn = warmthWarn;
            bearMin = desc->minPregnancyAge;                       // +0x50
            bearMax = desc->maxPregnancyAge;                       // +0x54
            timerSeed = desc->pregnancyTime;                       // +0x58
            // The occupant cap lives on the RESIDENCE description, not the citizen's —
            // filled in below from the first residence we successfully resolve.
        }

        const float age    = c->age;                               // +0x3c
        const float hunger = c->hunger;                            // +0x44
        const float warmth = c->coldness;                          // +0x48
        const float timer  = c->pregnancyTimer;                    // +0x4C
        ComponentEntity* const home = c->home;                     // +0x238
        const int   slotIdx = c->homeSlot;                         // +0x240

        int gate = 0;
        ResidenceComponent* residence = nullptr;
        if (c->gender != 0)                                            gate = 1;
        else if (age < desc->adultAge)                                 gate = 2;
        else if (c->school != nullptr)                                 gate = 3;
        else if (hungerWarn < hunger || warmthWarn < warmth)           gate = 4;
        else if (c->spouse == nullptr)                                 gate = 5;
        else if (home == nullptr)                                      gate = 6;
        else if (timer != 0.0f)                                        gate = 7;
        else if (age < desc->minPregnancyAge ||
                 age > desc->maxPregnancyAge)                          gate = 8;
        else {
            // 0x140008090 is Entity::FindComponent<ResidenceComponent> (NOT storage — 0x140006270
            // is the storage one; the two are a standing naming trap in this codebase).
            residence = home->FindComponent<ResidenceComponent>();
            if (!residence)                                            gate = 9;
            else if (residence->IsFamilyFull(slotIdx))                 gate = 10;   // 0x14012a4f0
        }
        tally[gate]++;

        // Detail lines: the interesting citizens are the ones deep in the chain, so print
        // anything past the married gate first — a wall of "male"/"child" says nothing.
        if (printed < s_birthTrace && (gate == 0 || gate >= 4)) {
            printed++;
            float cap = -1.0f;
            if (residence) {
                // A ResidenceComponent's description is its ResidenceDescription.
                if (Description* rdesc = residence->_description) {    // +0x18
                    cap = static_cast<float>(
                        static_cast<ResidenceDescription*>(rdesc)->_maxResidentsPerFamily);   // +0x24
                    if (occupantCap == 0.0f) occupantCap = cap;
                }
            }
            std::fprintf(stderr,
                "[BirthGate]   c=%p BLOCKED-AT=%s age=%.2f hunger=%.1f/%.1f warmth=%.1f/%.1f "
                "timer(+0x4C)=%.2f score(+0x50)=%.2f spouse=%d home=%d slot=%d occupantCap=%.0f\n",
                static_cast<void*>(c), kGateName[gate], age, hunger, hungerWarn,
                warmth, warmthWarn, timer, c->health,             // +0x50
                c->spouse != nullptr ? 1 : 0,
                home != nullptr ? 1 : 0, slotIdx, cap);
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
            CitizenComponent* c = list[i];
            const CitizenDescription* d = c ? c->description : nullptr;   // +0x288
            if (!d) continue;
            std::fprintf(stderr,
                "[BirthGate]   roll consts: chanceLow(desc+0x68)=%.4f chanceHigh(desc+0x64)=%.4f "
                "scoreMax(desc+0x70)=%.2f cooldownBase(desc+0x5C)=%.1f range(desc+0x60)=%.1f\n",
                d->_f68, d->_f64, d->maxHealth, d->_f5c, d->_f60);
            break;
        }
    }
    std::fflush(stderr);
}

void Census(int frameNum) {
    CitizenComponent** list; int n; CitizenBehavior* townMgr;
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
    // Lifespan audit. CitizenComponent::Update kills on `*(p+0x40) <= *(p+0x3c)`
    // (maxAge <= age) via HandleLifeEvent(self, 2), and that handler ALWAYS returns 1
    // and ALWAYS queues the entity for removal — so an "overdue" citizen (age past its
    // own maxAge) that is still in this list means the old-age branch is not being
    // reached, not that it declined to fire. The founding spawn rolls
    // maxAge = RandFloat * desc+0x3c + desc+0x38, i.e. [15,17) with the retail
    // constants, so once the oldest passes 15 some deaths are due. Counting `overdue`
    // separates "nobody is old enough yet" from "the check never runs" — two states
    // that look identical on the population curve alone.
    int   overdue = 0, noMaxAge = 0;
    float maxAgeMin = 1e9f, maxAgeMax = -1e9f;
    for (int i = 0; i < n; i++) {
        CitizenComponent* c = list[i];
        if (!c) continue;
        bool noSpouse = c->spouse == nullptr;                  // +0x250
        if (!noSpouse) spouse++; else unmarried++;
        if (c->_firstChild != nullptr) ring++;                 // +0x260
        if (c->school != nullptr) partner++;                   // +0x208
        bool guardianFree = c->_parent == nullptr;             // +0x258
        if (!guardianFree) host++;
        if (c->gender == 0) female++; else male++;             // +0x38

        float hunger = c->hunger;                              // +0x44
        float thirst = c->coldness;                            // +0x48 (row 121)
        hungerSum += hunger; thirstSum += thirst;
        if (hunger > hungerMax) hungerMax = hunger;
        if (thirst > thirstMax) thirstMax = thirst;
        if (c->disease != nullptr) sick++;                     // +0x68

        const float cAge   = c->age;                           // +0x3c
        const float cMaxAge = c->maxAge;                       // +0x40
        if (cMaxAge <= 0.0f) noMaxAge++;
        else {
            if (cMaxAge < maxAgeMin) maxAgeMin = cMaxAge;
            if (cMaxAge > maxAgeMax) maxAgeMax = cMaxAge;
            if (cMaxAge <= cAge) overdue++;
        }

        const CitizenDescription* desc = c->description;      // +0x288
        if (!desc) continue;
        float age = c->age;                                    // +0x3c
        adultAge  = desc->adultAge;                            // +0x44
        marryAge  = desc->marriageAge;                         // +0x48
        if (age < youngest) youngest = age;
        if (age > oldest)   oldest   = age;
        if (age >= adultAge) adults++; else children++;
        if (!noSpouse) continue;                 // already married: not a candidate
        bool old_enough = age >= marryAge;
        bool home       = c->home != nullptr;                  // +0x238
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
    ComponentEntity** reg = townMgr->field_0x20;           // +0x20
    int     regN  = townMgr->field_0x28;                   // +0x28
    void*   bst   = townMgr->field_0x100;                  // +0x100
    int     bldgN = townMgr->attendanceComponentCount;     // +0x68
    std::fprintf(stderr, "[FamilyCensus]   townMgr=%p buildingRegistry(+0x20)=%p count(+0x28)=%d "
                 "profBST(+0x100)=%p bldgCount(+0x68)=%d\n",
                 static_cast<void*>(townMgr), static_cast<void*>(reg), regN,
                 bst, bldgN);
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
        CitizenComponent* c = list[i];
        const CitizenDescription* d = c ? c->description : nullptr;   // +0x288
        if (d) { hungerDeath = d->maxHunger;                          // +0xd0
                 thirstDeath = d->maxColdness; break; }               // +0xf0
    }
    std::fprintf(stderr, "[FamilyCensus]   survival: hunger avg=%.1f max=%.1f (death %.1f) "
                 "thirst avg=%.1f max=%.1f (death %.1f) sick(+0x68)=%d\n",
                 n ? hungerSum / n : 0.0f, hungerMax, hungerDeath,
                 n ? thirstSum / n : 0.0f, thirstMax, thirstDeath, sick);
    // ⚠`thirst` above is really WARMTH/COLD (+0x48) — a town can freeze at 20.0 while
    // hunger sits comfortably at 5. See project_population_sustainability_soak.
    std::fprintf(stderr, "[FamilyCensus]   lifespan(+0x40): OVERDUE(age>=maxAge)=%d "
                 "maxAge min=%.2f max=%.2f zero/unset=%d\n",
                 overdue, (maxAgeMin > 1e8f ? 0.0f : maxAgeMin),
                 (maxAgeMax < -1e8f ? 0.0f : maxAgeMax), noMaxAge);
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
    CitizenComponent** list; int n; CitizenBehavior* townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    for (int i = 0; i < n && s_moved < s_marryHarness; i++) {
        CitizenComponent* c = list[i];
        if (!c) continue;
        if (c->spouse != nullptr) continue;                          // already married (+0x250)
        if (c->_parent == nullptr) continue;                         // already independent (+0x258)
        ComponentEntity* home = c->home;                             // +0x238
        if (home == nullptr) continue;                               // no home to move back into
        const CitizenDescription* desc = c->description;            // +0x288
        if (!desc) continue;
        float age = c->age;                                          // +0x3c
        if (age < desc->marriageAge) continue;                       // not old enough yet (+0x48)

        int slot = c->homeSlot;                                      // +0x240
        c->SetParent(nullptr);                                       // 0x140111fa0 (null IBP)
        CitizenComponent* guardian = c->_parent;                     // +0x258
        c->SetHome(home, slot);                                      // 0x1401163d0
        ComponentEntity* homeAfter = c->home;                        // +0x238
        std::fprintf(stderr, "[MarryHarness] frame %d: moved out comp=%p age=%.2f gender=%d "
                     "— guardian(+0x258) %s, home(+0x238) %p -> %p\n", frameNum,
                     static_cast<void*>(c), age, c->gender,
                     guardian == nullptr ? "cleared" : "STILL SET",
                     static_cast<void*>(home), static_cast<void*>(homeAfter));
        std::fflush(stderr);
        if (guardian == nullptr && homeAfter != nullptr) ++s_moved;
    }
    if (s_moved >= s_marryHarness) {
        s_firedMarry = true;
        std::fprintf(stderr, "[MarryHarness] %d citizen(s) independent and housed — done\n",
                     s_moved);
        std::fflush(stderr);
    }
}

// ---- Force-birth harness (BANISHED_FORCE_BIRTH=<n>) -------------------------------
// An organic birth is unreachable in a test-length run: conception needs a married,
// housed, fed, in-window female with an idle +0x4C timer, and then desc+0x58 = 900
// seconds of gestation on top. An 11-minute soak at FORCE_SPEED=10 produced zero
// deliveries — the founding cohort ages out first.
//
// So call the DELIVERY leaf directly. This is not a shortcut around the code under
// test: CitizenComp_helper_1b90 (0x140111b90) is exactly what CitizenComponent::Update
// invokes when the gestation timer fires, and it owns the whole newborn path —
// allocate + copy-construct the child entity, DeferredInit_117080, and
// EntityList_RegisterEntity (0x1400c48a0), whose tail is the OnAddedPass that
// broadcasts component vtable slot 6. Everything the birth arm needs to exercise is
// inside it; only the gestation countdown is skipped.
//
// Requirements on the "mother": a description (+0x288) and a ModelComponent (+0x278,
// for the spawn position). A spouse is optional — helper_1b90 tests +0x250 before
// flagging it. Any founding adult qualifies.
//
//   BANISHED_FORCE_BIRTH=<n>        deliver n babies (0 / unset = disabled)
//   BANISHED_FORCE_BIRTH_FRAME=<f>  frame to fire on (default 1200)
//
// Pair with BANISHED_BIRTH_TRACE=1 to get the [Birth] BABY BORN line and the newborn's
// carry capacity, which is the point of the exercise.
// CitizenComp_helper_1b90 is declared in Component/exe_bodies.h.

namespace {
void ForceBirths(int frameNum) {
    CitizenComponent** list; int n; CitizenBehavior* townMgr;
    if (!TownList(&list, &n, &townMgr)) {
        std::fprintf(stderr, "[BirthHarness] frame %d: no town citizen list — nothing to do\n",
                     frameNum);
        std::fflush(stderr);
        return;
    }
    // Snapshot the mothers first: each birth appends to this very list.
    CitizenComponent* mothers[16];
    int want = 0;
    int skippedNoDesc = 0, skippedNoModel = 0;
    for (int i = 0; i < n && want < s_forceBirth && want < 16; i++) {
        CitizenComponent* c = list[i];
        if (!c) continue;
        if (c->description == nullptr) { skippedNoDesc++;  continue; }   // +0x288
        if (c->character == nullptr)   { skippedNoModel++; continue; }   // +0x278
        mothers[want++] = c;
    }
    std::fprintf(stderr, "[BirthHarness] frame %d: %d mother(s) selected from %d citizens "
                 "(skipped: noDesc=%d noModel=%d)\n",
                 frameNum, want, n, skippedNoDesc, skippedNoModel);
    std::fflush(stderr);
    if (want == 0) return;

    int before = townMgr->citizenCount;                    // +0xa8
    for (int i = 0; i < want; i++) {
        std::fprintf(stderr, "[BirthHarness] delivering %d/%d from mother=%p\n",
                     i + 1, want, static_cast<void*>(mothers[i]));
        std::fflush(stderr);
        mothers[i]->GiveBirth();                           // 0x140111b90
    }
    std::fprintf(stderr, "[BirthHarness] done — town citizen list %d -> %d "
                 "(+%d expected if CitizenComponent::OnAdd is live)\n",
                 before, townMgr->citizenCount, want);
    std::fflush(stderr);
}
}  // namespace

extern "C" void CitizenDeathHarness_Tick(int frameNum) {
    Init();

    if (s_forceBirth > 0 && !s_firedBirth && frameNum >= s_forceBirthFrame) {
        s_firedBirth = true;
        ForceBirths(frameNum);
    }

    if (s_censusEvery > 0 && frameNum > 0 && (frameNum % s_censusEvery) == 0)
        Census(frameNum);

    // Poll until enough citizens have aged past the marriage age, then move them out once.
    if (s_marryHarness > 0 && !s_firedMarry && frameNum > 600 && (frameNum % 600) == 0)
        MoveOutGrownChildren(frameNum);

    // Targeted kill: wait for family links to exist, then take those citizens.
    if (s_killFamily > 0 && !s_firedFamily && frameNum > 300 && (frameNum % 300) == 0) {
        CitizenComponent** list; int n; CitizenBehavior* townMgr;
        if (TownList(&list, &n, &townMgr)) {
            CitizenComponent* victims[16];
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
                    CitizenComponent* v = victims[i];
                    std::fprintf(stderr, "[DeathHarness] killing family citizen comp=%p "
                                 "spouse=%p ring=%p\n", static_cast<void*>(v),
                                 static_cast<void*>(v->spouse),          // +0x250
                                 static_cast<void*>(v->_firstChild));    // +0x260
                    std::fflush(stderr);
                    v->HandleLifeEvent(3, nullptr);                      // 0x140116dd0
                }
                std::fprintf(stderr, "[DeathHarness] family kill done — list = %d entries "
                             "(was %d)\n", townMgr->citizenCount, n);    // +0xa8
                std::fflush(stderr);
            }
        }
    }

    if (s_count <= 0 || s_fired || frameNum < s_atFrame) return;

    // BANISHED_KILL_WHEN_CEMETERY_READY=1 -- hold the kill until the town actually has a
    // graveyard that can accept a burial, instead of racing a fixed frame number.
    //
    // Needed because the ONLY way to get a cemetery with a usable plot array is to build
    // one with the real tool: the plot grid is derived from the MultiObjectComponent's
    // child entities (the gate tile), and the ringPlace demo spawner creates a bare
    // entity with no children, so its plot array stays empty forever. A fixed
    // BANISHED_KILL_AT_FRAME therefore fires either before the player has placed one or
    // long after, and a "nobody was buried" result proves nothing either way.
    //
    // Ready = at least one cemetery in the town list at townMgr+0x40 (count +0x48) whose
    // own plot count (cemComp+0x50) is non-zero.
    {
        static int s_waitCemetery = -1;
        if (s_waitCemetery < 0) {
            const char* e = Opt().killWhenCemeteryReady;
            s_waitCemetery = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
            if (s_waitCemetery)
                std::fprintf(stderr, "[DeathHarness] kill DEFERRED until a cemetery with a "
                             "non-empty plot array exists (build one with the real tool)\n"),
                std::fflush(stderr);
        }
        if (s_waitCemetery) {
            CitizenBehavior* tm = nullptr; CitizenComponent** l; int c;
            int readyPlots = 0, cemN = 0;
            if (TownList(&l, &c, &tm) && tm) {
                cemN = tm->cemeteryCount;                        // +0x48
                CemeteryComponent** arr = tm->cemeteries;        // +0x40
                for (int i = 0; i < cemN && arr; ++i) {
                    CemeteryComponent* comp = arr[i];
                    if (!comp) continue;
                    const int plots = comp->plotCount;           // +0x50
                    if (plots > readyPlots) readyPlots = plots;
                }
            }
            if (readyPlots <= 0) {
                static int s_nag = 0;
                if ((frameNum % 600) == 0 && s_nag < 20) { ++s_nag;
                    std::fprintf(stderr, "[DeathHarness] waiting: %d cemetery(ies) registered, "
                                 "best plot count = %d\n", cemN, readyPlots);
                    std::fflush(stderr);
                }
                return;
            }
            std::fprintf(stderr, "[DeathHarness] cemetery READY at frame %d: %d registered, "
                         "%d plots -- proceeding with the kill\n", frameNum, cemN, readyPlots);
            std::fflush(stderr);
        }
    }

    s_fired = true;

    CitizenComponent** list; int n; CitizenBehavior* townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    std::fprintf(stderr, "[DeathHarness] frame %d: town citizen list = %d entries\n",
                 frameNum, n);

    // Snapshot first: each kill tears the victim out of this very list.
    //
    // This used to snapshot into `int64_t victims[64]` and clamp `want` to 64, so
    // BANISHED_KILL_CITIZENS=500 silently killed 64. That cap is invisible in the log
    // except as the "N/64" in the per-kill line, and it makes any test that needs a
    // LARGE death toll -- the Tombstone achievement wants 400 graves -- quietly
    // unreachable. Heap-allocate instead; the snapshot is transient and one qword per
    // citizen even for a whole town.
    int want = (s_count < n) ? s_count : n;
    std::vector<CitizenComponent*> victims((size_t)(want > 0 ? want : 0));
    for (int i = 0; i < want; i++) victims[(size_t)i] = list[i];

    for (int i = 0; i < want; i++) {
        if (!victims[i]) continue;
        std::fprintf(stderr, "[DeathHarness] killing citizen %d/%d comp=%p\n",
                     i + 1, want, static_cast<void*>(victims[i]));
        std::fflush(stderr);
        victims[i]->HandleLifeEvent(3, nullptr);           // 0x140116dd0
    }

    std::fprintf(stderr, "[DeathHarness] done — town citizen list = %d entries "
                 "(was %d; expect %d if OnRemove is live, unchanged if it is not)\n",
                 townMgr->citizenCount, n, n - want);      // +0xa8
    std::fflush(stderr);
}
