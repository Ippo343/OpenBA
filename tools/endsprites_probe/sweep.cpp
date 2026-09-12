// ============================================================================
// tools/endsprites_probe/sweep.cpp — WAVE 23 LANE TAIL
//
//   An INPUT SWEEP oracle for the lane's two ported UI bodies:
//
//     ?EndAddingSprites@SpriteCapture@UI@@QEAAXXZ            0x1800D8A60
//     ?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z   0x1800034F0
//
//   Built and run by `mingw32-make endsprites-sweep`, modelled on the
//   `libm-sweep` target that already does exactly this for the CRT port.
// ============================================================================
//
// WHY A SEPARATE EXE AND NOT AN IN-GAME PROBE
// ---------------------------------------------------------------------------
// Lane TAIL obtained NO game-directory run (the lock was held all wave). That
// excuses an ATTENDANCE gap; it does not excuse leaving two ported bodies
// resting entirely on transcription when an oracle is constructible without a
// run at all. Both bodies are reachable from hand-built memory, so this harness
// LoadLibrary()s the shipped Runtime-x64.dll READ-ONLY and drives our body and
// retail's over the same inputs. Loading a DLL does not contend for the game
// directory the way a game run does, which is the whole point.
//
// THIS IS AN ORACLE, NOT A SELF-CHECK. Retail's bodies are reached by
// GetProcAddress on the shipped export table -- never through the port
// registry, which in this process does not exist (the registration seam below
// is stubbed to no-ops). So there is no way for our body to be compared with
// itself, which is the failure that left a Wave 5 probe dark for five waves
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED FOR EndAddingSprites, AND WHAT IS DELIBERATELY NOT
// ---------------------------------------------------------------------------
// Both subjects are seeded by memcpy from ONE identical buffer, so every field
// -- written or not -- starts equal. That makes it safe to compare the fields
// neither body writes (`sheetIndex`, `rotation`, `color`, `name`): they are
// equal by construction, and comparing them CATCHES a body that writes where
// retail does not. This is the opposite situation from
// reference_oracle_must_not_compare_unwritten_bytes.md, where one side held
// allocator poison and the other real bytes; the distinction is the seeding,
// and it is why the seeding is stated.
//
// NOT compared, deliberately: the three `CaptureArray` headers
// (`data`/`size`/`capacity`) of `_entries`, `_sheets` and `_sprites`. Neither
// body touches them. They are the subject of the MIRROR control below.
//
// ---------------------------------------------------------------------------
// THE TWO NEGATIVE CONTROLS -- both directions, because both bite
// ---------------------------------------------------------------------------
//   --negctl=compared    corrupts `ours._width` AFTER our body runs. `_width`
//                        is in the compared set on every case, so EVERY case
//                        must FAIL. A control that corrupts something the probe
//                        does not look at reports NOT CAUGHT and indicts a
//                        sound instrument
//                        (reference_control_must_pick_a_compared_subject.md).
//
//   --negctl=entry       corrupts a compared ENTRY field (`entry[0].x2`) and
//                        leaves `_width` alone. IT EXISTS BECAUSE CONTROL 1
//                        EXPOSED A HOLE IN ITSELF: corrupting `_width` makes
//                        the comparison short-circuit before the entry loop,
//                        and that run printed `entry-fields compared=0` -- so
//                        control 1 validated `_width` and NOTHING ELSE, while
//                        the 248 entry-field comparisons are most of what this
//                        sweep claims to do. Exactly 15 of the 22 cases must
//                        fail (16 End cases minus the empty one; the 6 Set
//                        cases are untouched). The effectiveness counter is
//                        what made the hole visible, which is the whole reason
//                        it is printed
//                        (reference_probe_needs_an_effectiveness_control.md).
//
//   --negctl=uncompared  corrupts `ours._sprites.size`, a field of a DIFFERENT
//                        array that neither body reads or writes and that the
//                        comparison deliberately excludes. EVERY case must
//                        still PASS. If this one is CAUGHT, the comparison set
//                        is wider than this banner claims and the banner is the
//                        thing that is wrong -- the mirror in
//                        reference_a_control_can_corrupt_a_byte_that_should_not_be_compared.md.
//
// The exe exits non-zero if a normal run diverges, if `compared` fails to be
// caught, or if `uncompared` IS caught. All three are failures of the
// instrument or of the port, and none of them is allowed to pass silently.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../runtime/types/ui/SpriteCapture.h"
#include "../../runtime/types/ui/SpriteSheet.h"

using UI::CaptureArray;
using UI::CaptureEntry;
using UI::SpriteCapture;

// The two ported bodies, linked in from the real wave files.
extern "C" void RtUi_SpriteCapture_EndAddingSprites(void* self);
extern "C" void RtUi_SpriteSheet_Set(void* self, int p1, int p2,
                                     const void* p3, const void* p4, bool p5);

namespace {

HMODULE g_rt = nullptr;

typedef void (*EndFn)(void*);
typedef void (*SetFn)(void*, int, int, const void*, const void*, bool);

EndFn g_theirEnd = nullptr;
SetFn g_theirSet = nullptr;

enum class NegCtl { None, Compared, Uncompared, Entry };
NegCtl g_negctl = NegCtl::None;

int g_cases = 0, g_failed = 0;
long long g_entryFieldsCompared = 0;   // EFFECTIVENESS: how much was looked at

// ---------------------------------------------------------------------------
// EndAddingSprites — the population
// ---------------------------------------------------------------------------
// The dimensions that actually discriminate for this body, chosen against the
// uniform-population failure in
// reference_uniform_population_is_not_a_verification.md (a ported StrCompare
// "worked" because every pair in its population shared a first byte):
//
//   * COUNT          0 / 1 / many          -- the `<= 0` gate and the loops
//   * sheetIndex     >= 0 vs < 0           -- the scissor exclusion, which is
//                                            the ONLY branch inside pass 1
//   * MIX            scissors interleaved  -- separates "excluded from the box"
//                                            from "still rebased"
//   * COORD SIGN     negative / zero / +   -- the CMOVL/CMOVG comparisons
//   * SEED BOUNDARY  at and across +-0x8000 -- the seeds themselves, the one
//                                            place an off-by-one in min/max
//                                            hides
//   * DEGENERATE     all-scissor, empty    -- retail's -65535 extent
//   * WIDE           near INT32 extremes   -- the `max - min + 1` overflow
//   * REPEATS        identical entries     -- a population of distinct values
//                                            cannot see an accumulator bug
struct Case {
    const char* name;
    int         count;
    int32_t     e[8][5];   // sheetIndex, x1, y1, x2, y2
};

const Case kCases[] = {
  { "empty (count=0, degenerate extent)",              0, {{0,0,0,0,0}} },
  { "single at origin",                                1, {{0, 0,0, 0,0}} },
  { "single positive box",                             1, {{0, 10,20, 30,40}} },
  { "single negative box",                             1, {{0, -30,-40, -10,-20}} },
  { "single straddling origin",                        1, {{0, -5,-6, 7,8}} },
  { "all scissors (degenerate extent, still rebased)", 3, {{-1, 1,2, 3,4},
                                                           {-2, 5,6, 7,8},
                                                           {-1, -9,-10, -11,-12}} },
  { "mixed: scissor excluded from box, rebased",       3, {{-1, -1000,-1000, 1000,1000},
                                                           { 0, 10,10, 20,20},
                                                           {-2, 2000,2000, 3000,3000}} },
  { "many, spread",                                    5, {{0, 5,5, 6,6},
                                                           {0, -50,-60, -40,-30},
                                                           {1, 100,200, 300,400},
                                                           {2, -1,-1, 1,1},
                                                           {0, 0,0, 0,0}} },
  { "repeats (identical entries)",                     4, {{0, 7,7, 9,9},
                                                           {0, 7,7, 9,9},
                                                           {0, 7,7, 9,9},
                                                           {0, 7,7, 9,9}} },
  { "at the seeds exactly (+-0x8000)",                 1, {{0, 0x8000, 0x8000,
                                                               -0x8000, -0x8000}} },
  { "across the seeds (wider than the seeds)",         2, {{0, 0x7fff, 0x7fff, 0x8001, 0x8001},
                                                           {0, -0x8001, -0x8001, -0x7fff, -0x7fff}} },
  { "one past each seed",                              2, {{0, 0x7fff,0x7fff, 0x7fff,0x7fff},
                                                           {0, -0x7fff,-0x7fff, -0x7fff,-0x7fff}} },
  { "near INT32 extremes (max-min+1 overflow)",        2, {{0, -2000000000, -2000000000,
                                                               -1999999999, -1999999999},
                                                           {0, 2000000000, 2000000000,
                                                               2000000001, 2000000001}} },
  { "inverted box (x2 < x1)",                          1, {{0, 50,60, 10,20}} },
  { "scissor first, real second",                      2, {{-1, 0,0, 0,0},
                                                           { 0, 3,4, 5,6}} },
  { "real first, scissor second",                      2, {{ 0, 3,4, 5,6},
                                                           {-1, 0,0, 0,0}} },
};

CaptureEntry g_entOurs[8];
CaptureEntry g_entTheirs[8];
CaptureEntry g_entSeed[8];

bool RunEndCase(const Case& c) {
    ++g_cases;

    // ---- one identical seed, copied to both sides ------------------------
    std::memset(g_entSeed, 0, sizeof g_entSeed);
    for (int i = 0; i < c.count; ++i) {
        g_entSeed[i].sheetIndex = c.e[i][0];
        g_entSeed[i].x1         = c.e[i][1];
        g_entSeed[i].y1         = c.e[i][2];
        g_entSeed[i].x2         = c.e[i][3];
        g_entSeed[i].y2         = c.e[i][4];
        // Fields NEITHER body writes, given distinctive non-zero values so
        // that a spurious write is visible rather than landing on a zero that
        // already looked right.
        g_entSeed[i].rotation   = 0.5f + static_cast<float>(i);
        g_entSeed[i].color      = 0xAABBCC00u + static_cast<uint32_t>(i);
        g_entSeed[i].name       = reinterpret_cast<char*>(0x1000 + i);
        g_entSeed[i].pad04      = 0x5A5A0000 + i;
    }
    std::memcpy(g_entOurs,   g_entSeed, sizeof g_entSeed);
    std::memcpy(g_entTheirs, g_entSeed, sizeof g_entSeed);

    SpriteCapture ours, theirs;
    std::memset(&ours,   0, sizeof ours);
    std::memset(&theirs, 0, sizeof theirs);
    ours._entries.data      = g_entOurs;
    ours._entries.size      = c.count;
    ours._entries.capacity  = 8;
    theirs._entries.data     = g_entTheirs;
    theirs._entries.size     = c.count;
    theirs._entries.capacity = 8;
    // Distinctive, identical starting values in the two arrays neither body
    // touches -- the MIRROR control's subject.
    ours._sprites.size = theirs._sprites.size = 0x1234;
    ours._sheets.size  = theirs._sheets.size  = 0x5678;

    RtUi_SpriteCapture_EndAddingSprites(&ours);

    if (g_negctl == NegCtl::Compared)   ours._width      += 1;
    if (g_negctl == NegCtl::Uncompared) ours._sprites.size += 1;
    // The THIRD control exists because the first one exposed a hole in itself:
    // with `_width` corrupted the comparison short-circuits before the entry
    // loop and the run reported `entry-fields compared=0`. So control 1 proves
    // only that `_width` is read, and the 248 entry-field comparisons -- the
    // bulk of what this sweep claims to check -- were validated by NOTHING.
    // This one corrupts a compared ENTRY field instead and leaves `_width`
    // alone, so the entry loop is entered and must do the catching.
    if (g_negctl == NegCtl::Entry && c.count > 0) g_entOurs[0].x2 += 1;

    g_theirEnd(&theirs);

    // ---- compare ---------------------------------------------------------
    bool ok = true;
    const char* what = "";
    if (ours._width  != theirs._width)  { ok = false; what = "_width"; }
    if (ok && ours._height != theirs._height) { ok = false; what = "_height"; }
    for (int i = 0; ok && i < c.count; ++i) {
        const CaptureEntry& a = g_entOurs[i];
        const CaptureEntry& b = g_entTheirs[i];
        g_entryFieldsCompared += 8;
        if (a.x1 != b.x1) { ok = false; what = "x1"; }
        else if (a.y1 != b.y1) { ok = false; what = "y1"; }
        else if (a.x2 != b.x2) { ok = false; what = "x2"; }
        else if (a.y2 != b.y2) { ok = false; what = "y2"; }
        // the four neither body writes -- equal by construction, so a
        // divergence here is a spurious write on one side
        else if (a.sheetIndex != b.sheetIndex) { ok = false; what = "sheetIndex (SPURIOUS WRITE)"; }
        else if (a.rotation != b.rotation)     { ok = false; what = "rotation (SPURIOUS WRITE)"; }
        else if (a.color != b.color)           { ok = false; what = "color (SPURIOUS WRITE)"; }
        else if (a.name != b.name)             { ok = false; what = "name (SPURIOUS WRITE)"; }
        if (!ok) std::printf("        entry[%d] diverged on %s\n", i, what);
    }

    if (!ok) ++g_failed;
    std::printf("  %-46s count=%-2d  ours(w=%d,h=%d) theirs(w=%d,h=%d)  %s%s%s\n",
                c.name, c.count, ours._width, ours._height,
                theirs._width, theirs._height,
                ok ? "OK" : "**DIVERGED on ", ok ? "" : what, ok ? "" : "**");
    return ok;
}

// ---------------------------------------------------------------------------
// Set@SpriteSheet — the population
// ---------------------------------------------------------------------------
// A refcounted System::String is [int32 length][int32 refcount][chars...] with
// the STORED pointer at `chars`, so refcount is at data-4 and the allocation
// base at data-8 -- the layout this lane's port reads and the one
// spritecapture_probe.cpp's RcString already uses.
//
// THE ONE BRANCH THIS SWEEP DOES NOT COVER, and why (a result, not a refusal):
// the refcount-reaches-ZERO arm hands `data - 8` to the DLL's real
// `MemoryAllocator::MemFree`. Feeding that a static buffer corrupts the
// allocator's heap, and allocating a genuine block first requires a live
// `MemoryAllocator` singleton, which does not exist in this harness -- it is
// created during Engine startup inside a game run, and this sweep exists
// precisely because no run was available. So every case below keeps the
// outgoing refcount ABOVE 1. The four other arms (pointer-identity
// short-circuit, null-old, null-new, and the refcount-still-nonzero path
// INCLUDING its dead `field = nullptr` store) are all covered.
struct RcStr {
    int32_t length;
    int32_t refcount;
    char    chars[24];
};

RcStr* MakeStr(RcStr* s, const char* text, int32_t rc) {
    std::memset(s, 0, sizeof *s);
    s->length   = static_cast<int32_t>(std::strlen(text));
    s->refcount = rc;
    std::strncpy(s->chars, text, sizeof(s->chars) - 1);
    return s;
}

struct SetCase {
    const char* name;
    int         p1, p2;
    bool        p5;
    bool        oldTexNull, oldGrpNull;   // start the members null?
    bool        newTexNull, newGrpNull;   // pass null String data?
    bool        identity;                 // pass the SAME pointer back in
};

const SetCase kSetCases[] = {
  { "fresh object (both members null)",      1,   2, false, true,  true,  false, false, false },
  { "replace both (rc stays > 0)",           7,   9, true,  false, false, false, false, false },
  { "pointer identity on both (no-op arm)", -1,  -2, false, false, false, false, false, true  },
  { "new values are null",                   0,   0, true,  false, false, true,  true,  false },
  { "old null, new null",                    5,  -5, false, true,  true,  true,  true,  false },
  { "extremes",  0x7fffffff, static_cast<int>(0x80000000u), true, false, false, false, false, false },
};

RcStr g_oT, g_oG, g_nT, g_nG;     // ours' four blocks
RcStr g_tT, g_tG, g_uT, g_uG;     // theirs' four blocks

bool RunSetCase(const SetCase& c) {
    ++g_cases;

    // Each side gets its OWN blocks: both bodies mutate refcounts, so sharing
    // them would make the second run see the first run's effect. The two sides
    // are therefore compared STRUCTURALLY (did the member land on *my* new
    // string?) plus refcount DELTAS, never by raw pointer equality -- the
    // pointers differ by construction.
    const int32_t kRc = 5;                       // never reaches 0; see banner
    MakeStr(&g_oT, "old-tex", kRc); MakeStr(&g_oG, "old-grp", kRc);
    MakeStr(&g_nT, "new-tex", kRc); MakeStr(&g_nG, "new-grp", kRc);
    MakeStr(&g_tT, "old-tex", kRc); MakeStr(&g_tG, "old-grp", kRc);
    MakeStr(&g_uT, "new-tex", kRc); MakeStr(&g_uG, "new-grp", kRc);

    SpriteSheet ours, theirs;
    std::memset(&ours, 0, sizeof ours);
    std::memset(&theirs, 0, sizeof theirs);

    ours.m_textureName  = c.oldTexNull ? nullptr : g_oT.chars;
    ours.m_groupName    = c.oldGrpNull ? nullptr : g_oG.chars;
    theirs.m_textureName = c.oldTexNull ? nullptr : g_tT.chars;
    theirs.m_groupName   = c.oldGrpNull ? nullptr : g_tG.chars;

    // p3 -> +0x38 (texture), p4 -> +0x40 (group)
    char* oursP3 = c.identity ? ours.m_textureName : (c.newTexNull ? nullptr : g_nT.chars);
    char* oursP4 = c.identity ? ours.m_groupName   : (c.newGrpNull ? nullptr : g_nG.chars);
    char* theirP3 = c.identity ? theirs.m_textureName : (c.newTexNull ? nullptr : g_uT.chars);
    char* theirP4 = c.identity ? theirs.m_groupName   : (c.newGrpNull ? nullptr : g_uG.chars);

    const int32_t oT0 = g_oT.refcount, oG0 = g_oG.refcount;
    const int32_t nT0 = g_nT.refcount, nG0 = g_nG.refcount;
    const int32_t tT0 = g_tT.refcount, tG0 = g_tG.refcount;
    const int32_t uT0 = g_uT.refcount, uG0 = g_uG.refcount;

    RtUi_SpriteSheet_Set(&ours, c.p1, c.p2, &oursP3, &oursP4, c.p5);
    if (g_negctl == NegCtl::Compared)   ours.m_field4C += 1;
    if (g_negctl == NegCtl::Uncompared) ours.m_field10 = reinterpret_cast<void*>(0xDEAD);
    g_theirSet(&theirs, c.p1, c.p2, &theirP3, &theirP4, c.p5);

    bool ok = true; const char* what = "";
    if (ours.m_field48 != theirs.m_field48) { ok = false; what = "m_field48 (p1)"; }
    else if (ours.m_field4C != theirs.m_field4C) { ok = false; what = "m_field4C (p2)"; }
    else if (ours.m_field50 != theirs.m_field50) { ok = false; what = "m_field50 (p5)"; }
    // structural: each side must have landed on ITS OWN new string
    else if ((ours.m_textureName == oursP3) != (theirs.m_textureName == theirP3)) {
        ok = false; what = "m_textureName landed differently";
    } else if ((ours.m_groupName == oursP4) != (theirs.m_groupName == theirP4)) {
        ok = false; what = "m_groupName landed differently";
    }
    // refcount ledger: our deltas must equal their deltas, block for block
    else if ((g_oT.refcount - oT0) != (g_tT.refcount - tT0)) { ok = false; what = "old texture refcount delta"; }
    else if ((g_oG.refcount - oG0) != (g_tG.refcount - tG0)) { ok = false; what = "old group refcount delta"; }
    else if ((g_nT.refcount - nT0) != (g_uT.refcount - uT0)) { ok = false; what = "new texture refcount delta"; }
    else if ((g_nG.refcount - nG0) != (g_uG.refcount - uG0)) { ok = false; what = "new group refcount delta"; }

    if (!ok) ++g_failed;
    std::printf("  %-40s f48=%d/%d f4C=%d/%d f50=%d/%d  rcDelta ours(%+d,%+d,%+d,%+d) theirs(%+d,%+d,%+d,%+d)  %s%s%s\n",
                c.name, ours.m_field48, theirs.m_field48,
                ours.m_field4C, theirs.m_field4C,
                (int)ours.m_field50, (int)theirs.m_field50,
                g_oT.refcount-oT0, g_oG.refcount-oG0, g_nT.refcount-nT0, g_nG.refcount-nG0,
                g_tT.refcount-tT0, g_tG.refcount-tG0, g_uT.refcount-uT0, g_uG.refcount-uG0,
                ok ? "OK" : "**DIVERGED on ", ok ? "" : what, ok ? "" : "**");
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--negctl=compared") == 0)   g_negctl = NegCtl::Compared;
        if (std::strcmp(argv[i], "--negctl=uncompared") == 0) g_negctl = NegCtl::Uncompared;
        if (std::strcmp(argv[i], "--negctl=entry") == 0)       g_negctl = NegCtl::Entry;
    }

    const char* kDll = "C:/GOG Games/Banished/Runtime-x64.dll";
    g_rt = LoadLibraryA(kDll);
    if (!g_rt) { std::printf("FATAL: cannot load %s (%lu)\n", kDll, GetLastError()); return 2; }

    g_theirEnd = reinterpret_cast<EndFn>(reinterpret_cast<void*>(
        GetProcAddress(g_rt, "?EndAddingSprites@SpriteCapture@UI@@QEAAXXZ")));
    g_theirSet = reinterpret_cast<SetFn>(reinterpret_cast<void*>(
        GetProcAddress(g_rt, "?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z")));
    if (!g_theirEnd || !g_theirSet) {
        std::printf("FATAL: shipped bodies did not resolve (End=%p Set=%p)\n",
                    (void*)g_theirEnd, (void*)g_theirSet);
        return 2;
    }

    std::printf("=== EndAddingSprites / SpriteSheet::Set input sweep "
                "(negctl=%s) ===\n",
                g_negctl == NegCtl::None ? "none" :
                g_negctl == NegCtl::Compared ? "compared" : "uncompared");

    std::printf("\n-- ?EndAddingSprites@SpriteCapture@UI@@ --\n");
    for (const Case& c : kCases) RunEndCase(c);

    std::printf("\n-- ?Set@SpriteSheet@UI@@ --\n");
    for (const SetCase& c : kSetCases) RunSetCase(c);

    std::printf("\ncases=%d failed=%d  entry-fields compared=%lld\n",
                g_cases, g_failed, g_entryFieldsCompared);

    // ---- verdict, including the two control expectations -----------------
    if (g_negctl == NegCtl::Compared) {
        // Every case must fail: _width and m_field4C are compared everywhere.
        if (g_failed != g_cases) {
            std::printf("NEGATIVE CONTROL FAILED: corrupted a COMPARED field and "
                        "only %d of %d cases noticed. The sweep is not looking at "
                        "what this banner says it looks at.\n", g_failed, g_cases);
            return 1;
        }
        std::printf("NEGATIVE CONTROL (compared) OK: all %d cases caught it.\n", g_cases);
        return 0;
    }
    if (g_negctl == NegCtl::Entry) {
        // 16 EndAddingSprites cases, of which the empty one has no entry to
        // corrupt, plus 6 Set cases this control does not touch. So the
        // expectation is EXACT and stated as a number, not as "some".
        const int kExpected = 15;
        if (g_failed != kExpected) {
            std::printf("ENTRY CONTROL FAILED: corrupted entry[0].x2 and %d of "
                        "%d cases noticed; expected exactly %d (every "
                        "EndAddingSprites case with count>0). The entry-field "
                        "comparison is not doing what it claims.\n",
                        g_failed, g_cases, kExpected);
            return 1;
        }
        if (g_entryFieldsCompared == 0) {
            std::printf("ENTRY CONTROL FAILED: zero entry fields were compared, "
                        "so this control proved nothing.\n");
            return 1;
        }
        std::printf("ENTRY CONTROL OK: exactly %d cases caught it and %lld entry "
                    "fields were compared -- the entry loop is live.\n",
                    g_failed, g_entryFieldsCompared);
        return 0;
    }
    if (g_negctl == NegCtl::Uncompared) {
        if (g_failed != 0) {
            std::printf("MIRROR CONTROL FAILED: corrupted a field this sweep "
                        "claims NOT to compare and %d cases flagged it. The "
                        "compared set is wider than documented.\n", g_failed);
            return 1;
        }
        std::printf("MIRROR CONTROL OK: corrupting an uncompared field changed "
                    "nothing, as it must.\n");
        return 0;
    }
    if (g_failed) { std::printf("SWEEP FAILED\n"); return 1; }
    std::printf("SWEEP CLEAN\n");
    return 0;
}
