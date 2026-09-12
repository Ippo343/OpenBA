// ============================================================================
// standalone/cutover_manifest.cpp -- WAVE 25 lane MANIFEST
//
// THE 46 TAGGED GAP ROWS, PUBLISHED CONDITIONALLY
// ------------------------------------------------
// 46 of the runtime census gap rows carry an `RT_UNPROMOTABLE` tag written by
// the lane that adjudicated them. That adjudication was made ON THE
// DLL-PRESENT ROUTE, and `UNPROMOTABLE` is ROUTE-SCOPED, not permanent: for 44
// of the 46 the verdict INVERTS the moment `Runtime-x64.dll` is no longer
// loaded, because the storage the row names ceases to exist and our mint
// becomes its only owner.
//
// This file is the thing cutover attempt 26 consumes. It does NOT decide
// anything: every disposition is generated into `cutover_manifest.inc` by
// `tools/cutover_manifest.py` from (a) the row's own `[SCOPE:]` token, (b) its
// REASON CLASS field, and (c) a measured by-name-door count. Re-derive with
//
//     python tools/cutover_manifest.py --check
//
// which fails if the generated header is stale, if any REQUIRED row has no
// minted storage, or if any tag has gone back to declaring no scope.
//
// WHY THIS EXISTS WHEN `BANISHED_RT_MINT=1` ALREADY REGISTERS EVERYTHING
// ---------------------------------------------------------------------
// Attempt 25 (WAVE24_LANE_CUTOVER.md) reached its wall with the blanket switch
// on, and it registered clean -- `[RtMint] registered 201 by name, 207 by
// address`. That switch is a SLEDGEHAMMER: it publishes all 205 minted rows,
// including the rows a lane adjudicated as harmful ON EVERY ROUTE. Today that
// is exactly one row (`??_7Dialog@UI@@6B@`, whose harm is scoped PERMANENT and
// is blocked behind the `?_firstTypeInfo@` split brain), and one more that no
// lane has ever adjudicated FOR cutover at all. The blanket switch cannot tell
// those apart from the 44 that must be published, and **a build check cannot
// detect a misregistered port** (kickoff section 3 item 6).
//
// So this file EXTENDS the mint's existing per-row door -- `RtMintRegisterRow`,
// the same call `reg_mint_promoted.cpp` promotions and `BANISHED_RT_MINT_ROWS`
// both go through -- rather than inventing a second publisher. It is additive
// and idempotent: a row the blanket switch already registered is re-registered
// to the same storage, which is a no-op.
//
// WHAT IT REPORTS, AND WHY THAT IS THE POINT
// ------------------------------------------
// The dispositions are not all "publish":
//
//   REQUIRED     publish. Either the tag states the inversion in its own
//                words, or the row has by-name doors that resolve NULL with
//                no DLL loaded. **NO-OP is not the same as NOT-PUBLISHING** --
//                thirteen Video/Object rows are NO-OP on every route and still
//                have to be published, because a NO-OP verdict is about
//                DISPATCH and a door is about RESOLUTION.
//   WITHHELD     do NOT publish, even at cutover. The harm is scoped
//                PERMANENT. If such a row also has by-name doors, those doors
//                WILL resolve NULL -- this file says so by name rather than
//                letting attempt 26 discover it as an anonymous EXEC-of-0.
//   CONDITIONAL  HARMFUL, scoped DLL-PRESENT, but no lane ever stated that it
//                inverts. A scope token says where the harm was MEASURED; it
//                is not a licence to publish. Reported, never registered.
//
// This file is deliberately loud on the rows it does NOT publish. A publisher
// that prints only its successes is the shape of instrument this project keeps
// paying for.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "resolve.h"
#include "rt_data_mint.h"

namespace {

enum CutoverDisposition {
    CUTOVER_REQUIRED,
    CUTOVER_WITHHELD,
    CUTOVER_CONDITIONAL,
    CUTOVER_SAFE,
    CUTOVER_UNDECLARED,
};

struct CutoverManifestRow {
    const char*        symbol;
    CutoverDisposition disposition;
    bool               minted;
    unsigned long long dllVa;
    const char*        ns;
    int                doors;   // by-name doors measured in the tree
};

const CutoverManifestRow kManifest[] = {
#define CUTOVER_ROW(sym, disp, minted, va, ns, doors) { sym, disp, minted, va, ns, doors },
#include "cutover_manifest.inc"
#undef CUTOVER_ROW
};

const int kManifestCount = (int)(sizeof(kManifest) / sizeof(kManifest[0]));

const char* DispName(CutoverDisposition d) {
    switch (d) {
        case CUTOVER_REQUIRED:    return "REQUIRED";
        case CUTOVER_WITHHELD:    return "WITHHELD";
        case CUTOVER_CONDITIONAL: return "CONDITIONAL";
        case CUTOVER_SAFE:        return "SAFE";
        default:                  return "UNDECLARED";
    }
}

}  // namespace

// Is Runtime-x64.dll actually loaded right now? This is the SAME test
// RtDataMintShadowReport uses to print `DLL=ABSENT (post-cutover; oracle
// gone)`, deliberately: two different cutover detectors would be two different
// answers to one question.
bool RtCutoverDllAbsent() {
    const ModuleInfo* mi = GetModuleInfo(Module::Runtime);
    return !(mi && mi->handle);
}

// Publish the manifest. Returns the number of rows registered.
//
// `force` registers the REQUIRED rows even with the DLL present. That is what
// `BANISHED_RT_CUTOVER_MANIFEST=1` is for, and it is a MEASUREMENT knob, not a
// mode: with the DLL loaded, publishing these rows is precisely the split
// brain every one of those tags warns about. Nothing calls it with `force`
// except a lane that means to break its own run on purpose.
int RtCutoverPublishManifest(bool force) {
    const bool absent = RtCutoverDllAbsent();
    if (!absent && !force) {
        // Not cutover. Say nothing beyond one line -- an ordinary DLL-present
        // run must not grow 46 lines of output.
        printf("[CutoverManifest] DLL present -- %d tagged rows held "
               "(their verdicts are DLL-PRESENT-scoped and still apply)\n",
               kManifestCount);
        return 0;
    }

    printf("\n[CutoverManifest] ==== DLL %s ==== %d tagged gap rows\n",
           absent ? "ABSENT (cutover)" : "PRESENT (FORCED -- expect a split brain)",
           kManifestCount);

    int published = 0, withheld = 0, conditional = 0, unminted = 0, failed = 0;
    int nullDoors = 0;

    for (int i = 0; i < kManifestCount; ++i) {
        const CutoverManifestRow& r = kManifest[i];

        if (r.disposition != CUTOVER_REQUIRED) {
            if (r.disposition == CUTOVER_WITHHELD) ++withheld;
            else                                   ++conditional;
            // The loud half. A withheld row with doors is a PREDICTED fault,
            // named here before it happens.
            printf("[CutoverManifest] %-11s %-9s %s%s\n",
                   DispName(r.disposition), r.ns, r.symbol,
                   r.doors ? "" : "  (no by-name door)");
            if (r.doors) {
                nullDoors += r.doors;
                printf("[CutoverManifest]   ^ NOT published, and %d by-name "
                       "door(s) in this tree WILL resolve NULL. If attempt 26 "
                       "faults reaching this symbol, that is this line, not a "
                       "new discovery.\n", r.doors);
            }
            continue;
        }

        if (!r.minted) {
            // The Wave 24 `??_7Stream@Audio@@6B@` shape: a row nothing owns.
            // `RtMintRegisterRow` would simply return false here, which reads
            // as an ordinary miss; this says what it actually means.
            ++unminted;
            printf("[CutoverManifest] *** NO MINTED STORAGE *** %s (%s) -- "
                   "REQUIRED at cutover and nothing owns it. Its %d by-name "
                   "door(s) resolve NULL.\n", r.symbol, r.ns, r.doors);
            continue;
        }

        if (RtMintRegisterRow(r.symbol)) {
            ++published;
        } else {
            ++failed;
            printf("[CutoverManifest] *** REGISTER FAILED *** %s (%s) -- the "
                   "generated manifest says this is a minted row and the mint "
                   "disagrees. One of the two is stale; re-run "
                   "tools/cutover_manifest.py --check\n", r.symbol, r.ns);
        }
    }

    printf("[CutoverManifest] published %d, withheld %d, conditional %d, "
           "unminted %d, failed %d (of %d)\n",
           published, withheld, conditional, unminted, failed, kManifestCount);
    if (nullDoors) {
        printf("[CutoverManifest] %d by-name door(s) are KNOWINGLY left "
               "resolving NULL by the rows above. That is a published "
               "prediction, not an oversight.\n", nullDoors);
    }
    if (failed || unminted) {
        printf("[CutoverManifest] NOT CLEAN -- %d row(s) the manifest says "
               "must be published could not be.\n", failed + unminted);
    }
    return published;
}
