// ============================================================================
// runtime/src/object/factory_dispatch_probe.cpp — WAVE 11 FOLLOW-UP 5, lane TINFO
// ============================================================================
//
// THE QUESTION THIS EXISTS TO ANSWER, AND WHY IT NEEDED AN INSTRUMENT.
//
// Follow-up 2 closed with a binding rule for this lane:
//
//     "a typeinfo row may be promoted only if its factoryFn/inPlaceFactoryFn is
//      ported, or that type is provably never constructed through
//      External::Load. Read tiFactoriesTrapped."
//
// Both rows this lane was handed — `?_typeInfo@CompiledExternal@Object@@` and
// `?_typeInfo@Internal@Object@@` — carry an ABORT TRAP in both factory slots
// (standalone/rt_data_mint_tables.inc), because neither `?CreateNewObject@` nor
// `?CreateInPlaceObject@` is ported for either class. So the promotion is safe
// only if a minted node can never be the `info` a live dispatch site reads.
//
// That is a question about POINTERS, and the tree contains THREE dispatch sites
// (the brief expected one — see the lane doc):
//
//   runtime/src/object/external_load.cpp:351   Object::External::Load
//   runtime/src/object/type_system.cpp:352     Object::Factory::CreateObject
//   runtime/src/ui/dialog_create.cpp:551       UI::DialogInstance::AddControllerTab
//
// ALL THREE reach `info` the same way: walk `?_singleton@Factory@Object@@`'s
// AVL tree by typeId and read `TypeNode+0x10`. None of them resolves a
// `?_typeInfo@` symbol by name. So the model says a promotion cannot move what
// they see — the tree's nodes were filled by whoever registered the type, and
// the minted nodes hang off `g_mintTypeInfoHead`, a private chain no
// `Factory::Startup` walks (standalone/rt_data_mint.cpp).
//
// A MODEL IS NOT A MEASUREMENT. This probe records, per site, every dispatch
// and classifies the `info` pointer as MINTED (inside our
// `kTypeInfoStorage[104]`) or OTHER (our own image — the 312 exe types
// registered by
// code/Game/Object/factory_registration.cpp). `minted=0` across a real run,
// with the two controls below green, is the answer.
//
// TWO CONTROLS, SCORED SEPARATELY — because a zero from a screen that never
// ran, or from a predicate stuck on `false`, is the exact failure this project
// has been burned by repeatedly.
//
//   C1  PREDICATE. `RtMintIsMintedTypeInfo` is asked about
//       `&kTypeInfoStorage[20]` (which MUST read true) and about the DLL's own
//       node for the same row (which MUST read false). A predicate that cannot
//       say "yes" cannot report a "no".
//   C2  LIVENESS. Per site, `dispatches`. A site with `dispatches=0` did not
//       run on this route and its `minted=0` is NOT evidence — the report says
//       so in those words rather than printing a clean-looking zero.
//
// The probe is ALWAYS ON. It is three integer increments and a range compare on
// a path that runs a few thousand times a boot, and its whole value is that its
// zero is trustworthy; an env gate would make the zero conditional on somebody
// remembering to set it.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "factory_dispatch_probe.h"
#include "rt_data_mint.h"   // RtMintIsMintedTypeInfo / RtMintTypeInfoStorageProbe
#include "resolve.h"        // ResolveExportRaw

namespace {

// ---- the three sites, in the order they are listed in the banner ----------
struct Site {
    const char* name;
    uint64_t    dispatches;
    uint64_t    minted;     // info is one of OUR minted BaseTypeInfo nodes
    uint64_t    other;      // info is elsewhere (our image: the 312 exe types)
    uint64_t    null;       // info was null (retail's own unguarded shape)
};

Site g_site[RT_FACTORY_DISPATCH_SITE_COUNT] = {
    { "Object::External::Load",                0, 0, 0, 0 },
    { "Object::Factory::CreateObject",         0, 0, 0, 0 },
    { "UI::DialogInstance::AddControllerTab",  0, 0, 0, 0 },
};

// ---- the names dispatched through, so the report can say WHICH types are
//      created this way. Bounded; overflow is reported rather than dropped.
struct NameCount { const char* name; uint64_t n; };
// 512, not 96. The first run of this probe filled a 96-entry table and
// reported 102,486 dispatches whose name did not fit -- which left the ONE
// question the probe exists to answer ("is CompiledExternal or Internal ever
// created through a factory?") unanswerable from this list. The cap is now
// well above the DLL's 104 typeinfo rows plus the 312 exe types, and
// g_subject below is immune to it either way.
NameCount g_names[512];
int       g_nameCount   = 0;
uint64_t  g_namesDropped = 0;

// ---- CAP-IMMUNE counters for the two rows this lane was handed ------------
// A bounded name table can drop the very name the lane is asking about, and a
// dropped name reads exactly like a name that never appeared. These two never
// depend on the table's capacity.
struct Subject { const char* name; uint64_t n; };
Subject g_subject[2] = { { "CompiledExternal", 0 }, { "Internal", 0 } };

void NoteSubject(const char* name) {
    if (!name) return;
    for (Subject& s : g_subject)
        if (std::strcmp(s.name, name) == 0) { ++s.n; return; }
}

void NoteName(const char* name) {
    if (!name) return;
    for (int i = 0; i < g_nameCount; ++i) {
        if (g_names[i].name == name || std::strcmp(g_names[i].name, name) == 0) {
            ++g_names[i].n;
            return;
        }
    }
    if (g_nameCount >= (int)(sizeof g_names / sizeof g_names[0])) { ++g_namesDropped; return; }
    g_names[g_nameCount].name = name;
    g_names[g_nameCount].n    = 1;
    ++g_nameCount;
}

}  // namespace

// `info` is read BEFORE the call, so a dispatch that aborts in a trap stub is
// still counted — which is what makes the counter usable as a blocker rather
// than a post-mortem.
extern "C" void RtFactoryDispatchObserve(int site, const void* info) {
    if (site < 0 || site >= RT_FACTORY_DISPATCH_SITE_COUNT) return;
    Site& s = g_site[site];
    ++s.dispatches;
    if (!info) { ++s.null; return; }
    if (RtMintIsMintedTypeInfo(info)) {
        ++s.minted;
    } else {
        // Runtime-x64.dll is never mapped (ENDGAME.md A.1), so anything that
        // is not one of our minted nodes is in our own image.
        ++s.other;
        // `name` is at +0x08. Read only for the classification report; a null
        // or wild pointer here would be a finding in itself, so it is guarded
        // rather than dereferenced blindly (this is OUR instrument, not a
        // faithful body).
        const char* const* np = (const char* const*)((const unsigned char*)info + 8);
        NoteName(*np);
        NoteSubject(*np);
    }
}

extern "C" void RtFactoryDispatchReport() {
    uint64_t totalMinted = 0, totalDispatch = 0;
    for (const Site& s : g_site) { totalMinted += s.minted; totalDispatch += s.dispatches; }

    printf("[FactoryDispatch] === BaseTypeInfo::factoryFn dispatch census "
           "(WAVE 11 follow-up 5, lane TINFO) ===\n");
    for (const Site& s : g_site) {
        if (s.dispatches == 0) {
            printf("[FactoryDispatch] %-38s dispatches=0  -- SITE DID NOT RUN on "
                   "this route; its minted=0 is NOT evidence\n", s.name);
        } else {
            printf("[FactoryDispatch] %-38s dispatches=%llu  minted=%llu  "
                   "other=%llu  null=%llu\n", s.name,
                   (unsigned long long)s.dispatches, (unsigned long long)s.minted,
                   (unsigned long long)s.other,
                   (unsigned long long)s.null);
        }
    }

    // ---- C1: the predicate can say YES, and it can say NO --------------------
    const void* mintedNode = RtMintTypeInfoStorageProbe(20);   // CompiledExternal's row
    const void* dllNode    = ResolveExportRaw(
        "?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A");
    const bool c1yes = mintedNode && RtMintIsMintedTypeInfo(mintedNode);
    const bool c1no  = dllNode && !RtMintIsMintedTypeInfo(dllNode);
    printf("[FactoryDispatch] CONTROL C1 (predicate) minted-node=%s dll-node=%s  -> %s\n",
           mintedNode ? (c1yes ? "TRUE" : "FALSE(!)") : "unavailable",
           dllNode    ? (c1no  ? "FALSE" : "TRUE(!)") : "unavailable",
           (c1yes && c1no) ? "GREEN" : "RED -- the minted=0 above is NOT evidence");

    // ---- C2: at least one site ran ------------------------------------------
    printf("[FactoryDispatch] CONTROL C2 (liveness) total dispatches=%llu -> %s\n",
           (unsigned long long)totalDispatch,
           totalDispatch ? "GREEN" : "RED -- no site ran; this run measures nothing");

    printf("[FactoryDispatch] VERDICT: %llu of %llu dispatches went through a MINTED "
           "BaseTypeInfo%s\n", (unsigned long long)totalMinted,
           (unsigned long long)totalDispatch,
           totalMinted ? "  <-- A PROMOTED ROW'S FACTORY TRAP IS REACHABLE" : "");

    // The two rows this lane was handed, counted without going through the
    // bounded table above.
    for (const Subject& sub : g_subject)
        printf("[FactoryDispatch] SUBJECT type \"%s\" created through a factory "
               "pointer: %llu time(s)\n", sub.name, (unsigned long long)sub.n);

    if (g_nameCount) {
        printf("[FactoryDispatch] types created through a factory pointer "
               "(%d distinct%s):\n", g_nameCount,
               g_namesDropped ? ", TABLE FULL - some dropped" : "");
        for (int i = 0; i < g_nameCount; ++i)
            printf("[FactoryDispatch]     %-40s %llu\n",
                   g_names[i].name ? g_names[i].name : "(null name)",
                   (unsigned long long)g_names[i].n);
        if (g_namesDropped)
            printf("[FactoryDispatch]     ... %llu dispatch(es) whose type name did "
                   "not fit the table\n", (unsigned long long)g_namesDropped);
    }
}
