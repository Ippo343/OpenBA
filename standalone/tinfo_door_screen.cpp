// ============================================================================
// standalone/tinfo_door_screen.cpp — WAVE 24 lane TINFO
//
// THE QUESTION, AND WHY THE EXISTING MACHINERY COULD NOT ANSWER IT.
//
// Wave 23's handover item 1 says a promoted `?_typeInfo@` row "publishes a
// ZEROED BaseTypeInfo", and that the standing reading is that such rows are
// "inert — promoted but never dispatched". Both halves are claims about OUR
// NAME DOOR: about what `ResolveExport("?_typeInfo@X@Y@@…")` hands a caller
// once `reg_mint_promoted.cpp` has registered that symbol.
//
// `runtime/src/object/factory_dispatch_probe.cpp` — the instrument the kickoff
// points at — measures a DIFFERENT door, and measures it well: it watches the
// three sites that reach a `BaseTypeInfo*` by walking
// `?_singleton@Factory@Object@@`'s AVL tree, and it has reported
// `0 of ~101k dispatches through a MINTED node` with both controls green on
// every run since Wave 11. That answers "can a promotion put one of the 104
// factory abort traps in front of a live `factoryFn` dispatch?" — no.
//
// It cannot answer this lane's question, because **no AVL-tree walk consults a
// `?_typeInfo@` symbol by name**, so a promotion is invisible to it by
// construction. `minted=0` there is a fact about the tree, not about the door.
// The name door has never had an instrument.
//
// WHAT THIS SCREEN MEASURES, per minted `?_typeInfo@` row (all 104, not just
// the promoted ones — the un-promoted rows are the classifier's negative case):
//
//   door=      what ResolveExport hands a caller: MINTED (our node), DLL, or
//              NULL. The promotion's whole effect is this column.
//   zeroed=    is the node the caller receives actually zero-filled, i.e. the
//              handover's premise? Evaluated on the node the door returned.
//   idcmp=     minted typeId vs the loaded DLL node's typeId.
//   namecmp=   minted `name` string vs the DLL node's, by content.
//   resolves=  how many times THIS SYMBOL came through ResolveExport this run.
//              "inert" is a claim about this number and nothing else.
//
// FOUR CONTROLS, SCORED SEPARATELY. Three of them exist because a screen of
// this shape has produced a confident wrong answer in this project before.
//
//   C1  PREDICATE, both directions. `RtMintIsMintedTypeInfo` is asked about
//       `RtMintTypeInfoStorageProbe(0)` (MUST be true) and about the DLL's own
//       node for row 0 via ResolveExportRaw (MUST be false). A predicate stuck
//       on `false` would report every row `door=DLL` and read as "promotion
//       changes nothing", which is exactly the conclusion this lane was sent
//       to test. Without C1 that reading is unfalsifiable.
//
//   C2  THE ZEROED DETECTOR IS HANDED A POSITIVE SHAPE. `zeroed=NO` on 104
//       rows is an ABSENCE result, and an absence screen with no positive case
//       is indistinguishable from a dead parser. So `IsZeroed` is run against a
//       locally zero-initialised BaseTypeInfo, which MUST come back YES, and
//       against the DLL's live node for row 0, which MUST come back NO.
//
//   C3  DISCRIMINATION / EFFECTIVENESS. The door classifier is only meaningful
//       if it can produce BOTH answers on this run. The screen prints the
//       MINTED/DLL split over all 104 rows. All-MINTED or all-DLL is reported
//       as RED: a column with one value never told you anything, and "27 of
//       104" is the shape that proves the classifier read something.
//
//   C4  ATTENDANCE LIVENESS. `resolves=0` on every row means the counter is
//       dead, not that the doors are quiet. The total is printed and a zero
//       says in words that the per-row zeros are NOT evidence.
//
// ALWAYS ON, like the factory probe and for the same reason: its entire value
// is that a zero in it can be trusted, and an env gate makes the zero
// conditional on somebody having remembered to set it.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "rt_data_mint.h"
#include "resolve.h"

#include "../runtime/types/object/TypeInfo.h"

namespace {

// ---- attendance: how often each `?_typeInfo@` symbol came through the name
//      door this run. Keyed by the SYMBOL POINTER of the minted table row when
//      the pointers happen to match, and by content otherwise, because callers
//      spell the literal in their own translation unit.
struct Row {
    const char*                symbol;
    RtObject::BaseTypeInfo*    storage;
    uint64_t                   resolves;
};

const int kMaxRows = 160;          // 104 today; headroom, and it says so if it fills
Row  g_row[kMaxRows];
int  g_rows        = 0;
bool g_rowsBuilt   = false;
uint64_t g_unknown = 0;            // `?_typeInfo@` resolves matching no minted row

void BuildRows() {
    if (g_rowsBuilt) return;
    g_rowsBuilt = true;
    const int n = RtMintTypeInfoRowCount();
    for (int i = 0; i < n && g_rows < kMaxRows; ++i) {
        const char* sym = nullptr;
        void*       st  = nullptr;
        if (!RtMintTypeInfoRowAt(i, &sym, &st) || !sym) continue;
        g_row[g_rows].symbol   = sym;
        g_row[g_rows].storage  = (RtObject::BaseTypeInfo*)st;
        g_row[g_rows].resolves = 0;
        ++g_rows;
    }
}

// The handover's premise, as a predicate. A node is "zeroed" when it carries
// none of the five fields MintTypeInfos() stamps. Deliberately NOT `typeId==0`
// alone: 0 is a legal CRC and a one-field test would be a coin flip.
bool IsZeroed(const RtObject::BaseTypeInfo* b) {
    if (!b) return false;
    return b->typeId        == 0 &&
           b->name          == nullptr &&
           b->parentName    == nullptr &&
           b->fileExtension == nullptr &&
           b->factoryFn     == nullptr &&
           b->inPlaceFactoryFn == nullptr;
}

bool SameStr(const char* a, const char* b) {
    if (a == nullptr && b == nullptr) return true;
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
}

}  // namespace

// Called from ResolveExport, BEFORE its port-registry short-circuit, so a row
// that IS promoted is still counted — counting after the short-circuit would
// make every promoted row read `resolves=0` and manufacture the exact "inert"
// answer this screen exists to test.
void RtTInfoDoorNote(const char* mangled) {
    if (!mangled || mangled[0] != '?' || mangled[1] != '_') return;
    if (std::strncmp(mangled, "?_typeInfo@", 11) != 0) return;
    BuildRows();
    for (int i = 0; i < g_rows; ++i) {
        if (g_row[i].symbol == mangled ||
            std::strcmp(g_row[i].symbol, mangled) == 0) {
            ++g_row[i].resolves;
            return;
        }
    }
    ++g_unknown;
}

void RtTInfoDoorReport() {
    BuildRows();

    printf("[TInfoDoor] === what the NAME DOOR hands a caller for each minted "
           "`?_typeInfo@` row (WAVE 24 lane TINFO) ===\n");
    if (g_rows == 0) {
        printf("[TInfoDoor] RED: no minted rows enumerated -- this run measures "
               "NOTHING\n");
        return;
    }

    int minted = 0, dll = 0, nul = 0;
    int zeroedMinted = 0, idChecked = 0, idBad = 0, nameChecked = 0, nameBad = 0;
    int promotedRows = 0, attendedPromoted = 0;
    uint64_t totalResolves = 0;

    for (int i = 0; i < g_rows; ++i) {
        const Row& r = g_row[i];
        // SNAPSHOT FIRST. The ResolveExport call below goes through the very
        // door this screen counts, so reading `r.resolves` afterwards would
        // report this instrument's own traffic as the subject's attendance —
        // the screen would then never print `resolves=0` for any row and
        // "inert" would be unfalsifiable from the other side.
        const uint64_t resolves = r.resolves;
        totalResolves += resolves;

        RtObject::BaseTypeInfo* door =
            (RtObject::BaseTypeInfo*)ResolveExport(r.symbol);
        RtObject::BaseTypeInfo* raw  =
            (RtObject::BaseTypeInfo*)ResolveExportRaw(r.symbol);

        const bool isMinted = door && RtMintIsMintedTypeInfo(door);
        const char* cls = !door ? "NULL" : (isMinted ? "MINTED" : "DLL");
        if (!door)          ++nul;
        else if (isMinted)  ++minted;
        else                ++dll;

        const bool promoted = RtMintRowIsPromoted(r.symbol);
        if (promoted) {
            ++promotedRows;
            if (resolves) ++attendedPromoted;
        }

        // The premise, evaluated on the node the door actually returns.
        const bool z = IsZeroed(door);
        if (isMinted && z) ++zeroedMinted;

        // Ground truth. `raw` is the loaded DLL's own node; after its
        // Factory::Startup its typeId is the real CRC, and before it the ctor
        // sentinel 0xFFFFFFFF, which is a fact about WHEN we looked and is
        // therefore excluded from the comparison rather than scored as a pass.
        bool idCmp = false, idOk = false, nameCmp = false, nameOk = false;
        if (door && raw && raw->typeId != 0xFFFFFFFFu) {
            idCmp = true; ++idChecked;
            idOk  = (door->typeId == raw->typeId);
            if (!idOk) ++idBad;
        }
        if (door && raw) {
            nameCmp = true; ++nameChecked;
            nameOk  = SameStr(door->name, raw->name);
            if (!nameOk) ++nameBad;
        }

        // Print every PROMOTED row (the subject) and any row that is a finding.
        const bool finding = (isMinted && z) || (idCmp && !idOk) ||
                             (nameCmp && !nameOk) || (!door);
        if (promoted || finding) {
            printf("[TInfoDoor] %-58s door=%-6s zeroed=%-3s idcmp=%-12s "
                   "namecmp=%-12s resolves=%llu%s\n",
                   r.symbol, cls, z ? "YES" : "NO",
                   idCmp ? (idOk ? "EQ" : "DIFFER(!)") : "not-comparable",
                   nameCmp ? (nameOk ? "EQ" : "DIFFER(!)") : "not-comparable",
                   (unsigned long long)resolves,
                   promoted ? "  [PROMOTED]" : "  [FINDING]");
        }
    }

    // ---- C1: the predicate can say YES and it can say NO --------------------
    const void* probe = RtMintTypeInfoStorageProbe(0);
    const char* row0  = nullptr;
    void*       ign   = nullptr;
    RtMintTypeInfoRowAt(0, &row0, &ign);
    const void* dllNode = row0 ? ResolveExportRaw(row0) : nullptr;
    const bool c1yes = probe   && RtMintIsMintedTypeInfo(probe);
    const bool c1no  = dllNode && !RtMintIsMintedTypeInfo(dllNode);
    printf("[TInfoDoor] CONTROL C1 (predicate) minted-node=%s dll-node=%s -> %s\n",
           probe   ? (c1yes ? "TRUE"  : "FALSE(!)") : "unavailable",
           dllNode ? (c1no  ? "FALSE" : "TRUE(!)")  : "unavailable",
           (c1yes && c1no) ? "GREEN"
                           : "RED -- the door= column above is NOT evidence");

    // ---- C2: the zeroed detector is handed a positive shape -----------------
    RtObject::BaseTypeInfo blank;
    std::memset(&blank, 0, sizeof blank);
    const bool c2yes = IsZeroed(&blank);
    const bool c2no  = dllNode && !IsZeroed((const RtObject::BaseTypeInfo*)dllNode);
    printf("[TInfoDoor] CONTROL C2 (zeroed detector) blank-node=%s live-dll-node=%s"
           " -> %s\n",
           c2yes ? "YES" : "NO(!)",
           dllNode ? (c2no ? "NO" : "YES(!)") : "unavailable",
           (c2yes && c2no) ? "GREEN"
                           : "RED -- the zeroed= column above is NOT evidence");

    // ---- C3: discrimination / effectiveness ---------------------------------
    const bool c3 = (minted > 0 && dll > 0);
    printf("[TInfoDoor] CONTROL C3 (discrimination) of %d rows: MINTED=%d DLL=%d "
           "NULL=%d -> %s\n", g_rows, minted, dll, nul,
           c3 ? "GREEN"
              : "RED -- the classifier produced ONE answer for every row; it "
                "cannot be shown to have read anything");

    // ---- C4: attendance liveness --------------------------------------------
    printf("[TInfoDoor] CONTROL C4 (attendance liveness) total `?_typeInfo@` "
           "name-door resolves=%llu (%llu matched no minted row) -> %s\n",
           (unsigned long long)totalResolves, (unsigned long long)g_unknown,
           totalResolves ? "GREEN"
                         : "RED -- the counter never fired; every resolves=0 "
                           "above is NOT evidence of inertness");

    // ---- the verdict, in the two halves the lane was asked for --------------
    printf("[TInfoDoor] VERDICT/WRONG : %d of %d rows the door returns from OUR "
           "minted storage are ZEROED; typeId compared on %d row(s), %d differ; "
           "name compared on %d row(s), %d differ\n",
           zeroedMinted, minted, idChecked, idBad, nameChecked, nameBad);
    printf("[TInfoDoor] VERDICT/INERT : %d promoted row(s), %d of them resolved "
           "by NAME at least once this run%s\n",
           promotedRows, attendedPromoted,
           (promotedRows && !attendedPromoted && totalResolves)
               ? "  <-- INERT on this route (the counter IS live: see C4)" : "");
}
