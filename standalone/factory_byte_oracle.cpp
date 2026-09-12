// ============================================================================
// standalone/factory_byte_oracle.cpp — WAVE 29 LANE VERIFY
//
// The promotion of Wave 28 lane FACTUI1's offline byte oracle
// (docs/runtime_port/wave28_factui1_oracle.cpp.txt), CHANGED IN ONE WAY THAT
// DECIDES WHETHER IT IS AN ORACLE AT ALL.
//
// ---------------------------------------------------------------------------
// WHY THE ORIGINAL COULD NOT BE PROMOTED AS WRITTEN
// ---------------------------------------------------------------------------
// FACTUI1's oracle is a separate process that LoadLibrary's the retail DLL. Its
// THEIRS side calls `GetProcAddress("?CreateInPlaceObject@...")` — the real
// shipped body. Its OURS side is `OursCtor()`, which walks `sp.stores` and
// replays the table itself. It never calls the port.
//
// The port (`runtime/src/ui/factory_ui1.cpp`, `rt_factories_core.cpp`) is ALSO
// a table interpreter over that same table. So the oracle's ours-side is a
// SECOND COPY of the port's interpreter, reading the SAME data. What it proves
// is that the TABLE matches retail. What it cannot see is any defect in the
// interpreter, because both sides of the "ours" comparison share it — and a
// second copy of a loop is exactly where a shared misunderstanding survives
// (reference_two_checks_one_hidden_premise.md). It is the same structural
// weakness as the ~250 RT_DIFF_SHADOW self-checks
// (reference_shadow_marker_is_not_an_oracle.md) and as a probe whose ours-side
// reaches the body by address
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md):
// in all three the ours-side is not the shipped thing.
//
// That is a real property of the instrument, not a stylistic complaint, and it
// is why this file exists instead of a copy of that one under tools/.
//
// ---------------------------------------------------------------------------
// WHAT THIS ONE DOES INSTEAD
// ---------------------------------------------------------------------------
//   OURS   = `r.inPlaceFn` / `r.newFn` — THE REGISTERED FUNCTION POINTER, the
//            exact bytes the process would dispatch at cutover. Taken from the
//            roster, not resolved by name, so the "ResolveExport falls through
//            to the DLL for an unported symbol" hazard cannot apply
//            (reference_converting_a_door_is_not_satisfying_it.md), and the
//            "ResolveAddr checks the port registry FIRST" hazard cannot either
//            (reference_resolveaddr_consults_port_registry_first.md).
//   THEIRS = `ResolveExportRaw(name)` — GetProcAddress on the shipped DLL,
//            never ResolveExport, which would hand back OUR function and make
//            the oracle compare us with ourselves.
//
// ---------------------------------------------------------------------------
// THE EXCLUSION IS DERIVED, NOT LISTED
// ---------------------------------------------------------------------------
// FACTUI1 excluded +0x08..+0x0f by hand, as the Object::Internal handle id, and
// its own §"IsInternalRooted" post-mortem records that the hand-written scope
// was wrong in BOTH directions on different rows.
//
// This one runs RETAIL TWICE, on two separate buffers, and excludes exactly the
// bytes on which retail disagrees with ITSELF. That mask is measured per class,
// per run, from the subject — so it cannot be too wide or too narrow for a
// reason nobody checked, and it needs no roster of which classes reach
// Internal. It is printed, per class, as `nondet=N`.
//
// WHAT THAT CONSTRUCTION CANNOT SEE — raised by lane INSTR as a design review
// of this description (INSTR states, and I record, that it reviewed the PROSE,
// not this file; it was uncommitted when the review was written). Both
// directions are real and they are NOT symmetric:
//
//   FALSE FAIL (self-limiting, survivable). A byte that is constant WITHIN a
//   run but varies BETWEEN configurations — an ASLR-derived pointer, a
//   once-per-process id, a cached arena address — is identical in both retail
//   runs, so it is NOT masked, enters the compared set, and can diverge against
//   ours for a reason that is not a port defect. This shows up as a FAIL and
//   somebody investigates it. That is the good direction.
//
//   MASKED DEFECT (NOT self-limiting). A byte that ours gets wrong and that is
//   ALSO nondeterministic in retail is excluded permanently and can never fail.
//   THE BUILT-IN NEGATIVE CONTROL CANNOT SEE THIS: it flips a byte chosen FROM
//   the compared set, so by construction it can only ever prove the compared set
//   is scored, never that the masked set is too big.
//
// The one number that lets a reader price that is `masked=` beside `cmp=`,
// printed per class below on INSTR's ask. A class that passes with 90% of its
// bytes masked passed for a reason that has nothing to do with our port, and
// without that column nothing in the output distinguishes it from a class that
// passed on all of them.
//
// ---------------------------------------------------------------------------
// THREE CONTROLS, SCORED SEPARATELY (kickoff warnings 7 and 9)
// ---------------------------------------------------------------------------
//   EFFECTIVENESS  `cmp=` — how many bytes were actually COMPARED for this
//                  class. A class with cmp=0 is reported VACUOUS, never PASS.
//                  64 green comparisons meant nothing once this was asked
//                  (reference_probe_needs_an_effectiveness_control.md).
//   NEGATIVE       after a clean compare, one COMPARED byte of our buffer is
//                  flipped and the comparison re-run. It MUST fail. A control
//                  that corrupts a byte nothing compares reports NOT CAUGHT and
//                  indicts a sound instrument
//                  (reference_control_must_pick_a_compared_subject.md) — so the
//                  byte is chosen FROM the compared set, not from the buffer.
//   POSITIVE       the abstract rows (allocSize == 0) must have retail return
//                  NULL from CreateNewObject. If retail returns non-null there,
//                  the roster's `allocSize 0` claim is wrong and the whole
//                  table is suspect (reference_retail_ships_empty_bodies.md
//                  cuts the other way here: retail's empty body is the CLAIM
//                  being tested, not an excuse).
//
// ---------------------------------------------------------------------------
// WHERE IT RUNS -- MEASURED, NOT CHOSEN, AND TWO OBVIOUS PLACEMENTS FAILED
// ---------------------------------------------------------------------------
// From `ToolHarness_Tick` at in-game frame 120, one shot. That is the first
// point in the process where the ENGINE'S OWN MemoryAllocator is live, and it
// is the allocator these constructors will use at cutover, so it is the right
// one rather than merely a working one.
//
// The two placements that failed first, recorded because "just run it earlier"
// is the obvious fix and it is wrong twice over:
//   PROCESS EXIT PATH  ACCESS_VIOLATION writing +0x648 inside
//                      Runtime-x64.dll's MemAlloc, on the THIRD row -- the
//                      allocator's lock at singleton+0x640 with the singleton
//                      already NULL. Some of these ctors allocate an inner
//                      Collection::Array; by exit the engine has torn its
//                      allocator down.
//   BEFORE GameMain    the singleton does not exist yet (Engine::Startup makes
//                      it), so the screen aborted having compared 0 rows.
// A bootstrap of our own allocator was also tried at the exit path and its
// `Startup` RETURNED 0 -- recorded because it LOOKED like it worked (the
// singleton slot was populated) and the next allocation died anyway. A
// populated slot is not a started allocator.
//
// COST: FACTUI1 ran offline because an Element-derived ctor registers a handle
// that would dangle in-game. Half of that is defused here -- the buffers are
// file-scope statics that outlive the process, so nothing can dangle. The other
// half is NOT: this does insert roster-class handles into a live
// InternalFactory mid-game. That is the price of a live allocator, it is paid
// only on an explicitly armed run, and it is stated rather than buried.
//
// ARMED WITH BANISHED_FACTORY_ORACLE=1, off by default.
// The report prints its arm state unconditionally.
// ============================================================================

#include "resolve.h"
#include "../runtime/src/object/rt_factories.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace {

// Big enough for every class in the roster; the loop refuses anything larger
// rather than writing past the end (checked, not assumed).
constexpr size_t kBufBytes = 0x1000;
unsigned char g_ours  [kBufBytes];
unsigned char g_theirs[kBufBytes];
unsigned char g_theirs2[kBufBytes];
unsigned char g_ours2  [kBufBytes];

constexpr unsigned char kPoisonOurs   = 0xA5;
constexpr unsigned char kPoisonTheirs = 0x5A;

using InPlaceFn = void* (*)(void*);
using NewFn     = void* (*)();

bool ArmedOracle() {
    const char* e = getenv("BANISHED_FACTORY_ORACLE");
    return e && *e && strcmp(e, "0") != 0;
}

struct Verdict { int cmp; int bad; int firstBad; int nondet; };

// Aggregates for the masked-coverage line. A per-class number nobody totals is
// a number nobody reads.
long long g_totCmp = 0, g_totMasked = 0, g_totBytes = 0;

// Compare, skipping (a) bytes neither side wrote and (b) bytes retail disagreed
// with itself about.
Verdict Compare(size_t size) {
    Verdict v{0, 0, -1, 0};
    for (size_t o = 0; o < size; ++o) {
        // MASKED: EITHER side disagreed with ITSELF across two constructions,
        // so the byte is nondeterministic and is excluded from scoring. It can
        // NEVER fail; counted and printed, because the built-in negative
        // control is structurally unable to report it.
        //
        // THE OURS HALF OF THIS MASK WAS MISSING IN THE FIRST VERSION AND IT
        // MANUFACTURED FAILURES. Measured across three runs of this screen,
        // Graphics::EdgeMesh +0x60 read ours=98 / ours=90 / ours=60 while
        // retail read 68 every time: a byte OUR side varies and retail does
        // not. A one-sided mask scored that as a port divergence three times.
        // Direction: INFLATING -- it manufactures work, which is the
        // self-limiting direction, but it is still wrong.
        if (g_theirs[o] != g_theirs2[o] || g_ours[o] != g_ours2[o]) { ++v.nondet; continue; }
        const bool ourW   = g_ours[o]   != kPoisonOurs;
        const bool theirW = g_theirs[o] != kPoisonTheirs;
        if (!ourW && !theirW) continue;      // neither side wrote it
        ++v.cmp;
        if (ourW != theirW || g_ours[o] != g_theirs[o]) {
            ++v.bad;
            if (v.firstBad < 0) v.firstBad = (int)o;
        }
    }
    return v;
}

}  // namespace

extern "C" void RtFactoryByteOracle() {
    const bool armed = ArmedOracle();
    // ---- THE ALLOCATOR MUST BE UP -----------------------------------------
    // MEASURED, not assumed. The first placement of this call was the process
    // EXIT path, after GameMain returned, and it died on the third row with an
    // ACCESS_VIOLATION writing 0x648 inside Runtime-x64.dll+0xC7A8A — that is
    // `?MemAlloc@` taking the allocator's lock at singleton+0x640 with the
    // singleton already NULL. Some of these constructors allocate (an inner
    // Collection::Array), and by exit the engine has torn the allocator down.
    // This guard makes the dependency EXPLICIT, so a future move of the call
    // site gets a sentence instead of an access violation.
    void** allocSlot = (void**)ResolveExportRaw(
        "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    if (armed && (!allocSlot || !*allocSlot)) {
        printf("[FactOracle] ABORTED BEFORE ANY COMPARISON: the "
               "System::MemoryAllocator singleton is %s. Constructors in this "
               "roster allocate, so running "
               "here would fault rather than measure. This is a PLACEMENT "
               "failure, not a port finding.\n",
               allocSlot ? "NULL" : "unresolvable");
        fflush(stdout);
        return;
    }
    const RtFactoryReg* rows[256];
    const size_t total = RtFactoryRoster(rows, sizeof(rows)/sizeof(rows[0]));
    const size_t n     = total < 256 ? total : 256;

    printf("[FactOracle] ARM=%s (BANISHED_FACTORY_ORACLE)  roster=%zu rows"
                "%s\n",
                armed ? "ARMED" : "UNARMED -- no class below was compared",
                total, total > 256 ? "  *** TRUNCATED, widen `rows` ***" : "");
    if (!armed || n == 0) { fflush(stdout); return; }
    printf("[FactOracle] SUBJECT: our REGISTERED factory bodies "
                "(RtFactoryReg::{new,inPlace}Fn) vs the shipped DLL's exports "
                "via ResolveExportRaw. NOT the table vs the DLL -- see the file "
                "banner for why that distinction is the whole point.\n");

    int checked = 0, pass = 0, vacuous = 0, fail = 0, skipped = 0;
    int negRun = 0, negCaught = 0;
    int absChecked = 0, absPass = 0;

    for (size_t i = 0; i < n; ++i) {
        const RtFactoryReg& r = *rows[i];
        const RtFactoryClass& c = *r.cls;

        // ---- abstract rows: the claim is "retail returns null" -------------
        if (c.allocSize == 0) {
            NewFn theirsN = (NewFn)ResolveExportRaw(r.newName);
            if (!theirsN) { ++skipped; continue; }
            ++absChecked;
            void* t = theirsN();
            if (t == nullptr) ++absPass;
            else printf("[FactOracle] %-34s ABSTRACT CLAIM WRONG: retail "
                             "returned %p from %s\n", c.typeName, t, r.newName);
            continue;
        }

        if (c.allocSize > kBufBytes) {
            printf("[FactOracle] %-34s SKIP (0x%x bytes > buffer)\n",
                        c.typeName, c.allocSize);
            ++skipped; continue;
        }
        InPlaceFn ours   = (InPlaceFn)r.inPlaceFn;
        InPlaceFn theirs = (InPlaceFn)ResolveExportRaw(r.inPlaceName);
        if (!ours || !theirs) {
            printf("[FactOracle] %-34s SKIP (%s)\n", c.typeName,
                        !ours ? "no ours fn" : "retail export missing");
            ++skipped; continue;
        }

        memset(g_ours,   kPoisonOurs,   c.allocSize);
        memset(g_theirs, kPoisonTheirs, c.allocSize);
        memset(g_theirs2, kPoisonTheirs, c.allocSize);
        memset(g_ours2,  kPoisonOurs,  c.allocSize);
        ours(g_ours);
        ours(g_ours2);          // ours against itself -> the other half of the mask
        theirs(g_theirs);
        theirs(g_theirs2);      // retail against itself -> the nondet mask

        Verdict v = Compare(c.allocSize);
        ++checked;
        if (v.cmp == 0) {
            ++vacuous;
            printf("[FactOracle] %-34s VACUOUS  0 bytes compared "
                        "(size=0x%x nondet=%d) -- this row is NOT verified\n",
                        c.typeName, c.allocSize, v.nondet);
            continue;
        }
        if (v.bad) {
            ++fail;
            printf("[FactOracle] %-34s FAIL %d/%d compared bytes differ, "
                        "first +0x%x (ours %02x theirs %02x, nondet=%d)\n",
                        c.typeName, v.bad, v.cmp, v.firstBad,
                        g_ours[v.firstBad], g_theirs[v.firstBad], v.nondet);
            continue;
        }
        ++pass;
        // ---- NEGATIVE CONTROL on a COMPARED byte --------------------------
        int victim = -1;
        for (size_t o = 0; o < c.allocSize && victim < 0; ++o) {
            if (g_theirs[o] != g_theirs2[o] || g_ours[o] != g_ours2[o]) continue;
            if (g_ours[o] == kPoisonOurs && g_theirs[o] == kPoisonTheirs) continue;
            victim = (int)o;
        }
        if (victim >= 0) {
            ++negRun;
            // BOTH COPIES. The moment the mask became two-sided this control
            // started flipping ONLY g_ours, which made the victim byte look
            // nondeterministic -- so it was MASKED instead of compared and the
            // control read 0/20 NOT CAUGHT on a screen that was fine. It
            // announced itself loudly rather than passing quietly, which is the
            // only reason it cost minutes instead of a wave. Corrupt the value
            // CONSISTENTLY, the way a real port defect would appear.
            g_ours[victim] ^= 0x01;
            g_ours2[victim] ^= 0x01;
            Verdict nv = Compare(c.allocSize);
            if (nv.bad) ++negCaught;
            else printf("[FactOracle] %-34s *** NEGATIVE CONTROL NOT CAUGHT "
                             "at +0x%x -- do not read this row's PASS ***\n",
                             c.typeName, victim);
            g_ours[victim] ^= 0x01;
        }
        // cmp= and masked= TOGETHER. `masked` is the set this screen threw away
        // as retail-nondeterministic; a high masked count means this OK was won
        // over a small part of the object. INSTR's ask, and the one failure
        // direction the negative control cannot reach.
        printf("[FactOracle] %-34s OK   cmp=%d masked=%d of 0x%x bytes\n",
                    c.typeName, v.cmp, v.nondet, c.allocSize);
        g_totCmp += v.cmp; g_totMasked += v.nondet; g_totBytes += c.allocSize;
    }

    printf("[FactOracle] RESULT: %d concrete rows checked -- %d byte-identical, "
                "%d FAIL, %d VACUOUS (0 bytes compared), %d skipped\n",
                checked, pass, fail, vacuous, skipped);
    printf("[FactOracle] COVERAGE: %lld bytes compared, %lld MASKED as "
                "retail-nondeterministic, over %lld object bytes in the passing "
                "rows. The masked set can never fail and the negative control "
                "below cannot see it -- read this line before the PASS count.\n",
                g_totCmp, g_totMasked, g_totBytes);
    printf("[FactOracle] CONTROL negative: %d/%d passing rows caught a "
                "one-bit corruption of a COMPARED byte%s\n",
                negCaught, negRun,
                (negRun && negCaught == negRun) ? "" :
                "  *** a row that cannot be made to fail is not a screen ***");
    printf("[FactOracle] CONTROL abstract: %d/%d rows whose table says "
                "allocSize=0 had retail return NULL from CreateNewObject\n",
                absPass, absChecked);
    printf("[FactOracle] WHAT THIS DOES NOT CLAIM: it compares CONSTRUCTED "
                "BYTES on the in-place door. It does not prove anything CALLED "
                "these bodies in play -- attendance for this family is 0 by "
                "design, because the DLL reaches its own factories through a "
                "static-initialiser stamp and a stamp is not a door.\n");
    fflush(stdout);
}
