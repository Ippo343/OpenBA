// ============================================================================
// standalone/rt_data_mint.cpp — Wave 9 lane C2: mint the exported DATA symbols
// ============================================================================
//
// See rt_data_mint.h for what "minting" means and why registration is opt-in.
// This file holds the machinery; the data itself is generated into
// rt_data_mint_tables.inc by tools/rt_mint_gen.py from lane C's harvest.
//
// THE ONE RULE THAT SHAPES EVERYTHING HERE
//
// A minted vtable slot may hold exactly two things: OUR ported function, or a
// named trap. Never a DLL address. Wave 6 and Wave 7 both got away without
// hand-writing a vtable because the DLL exports its `??_7` symbols and a
// ported constructor could copy the bytes; at cutover there are no bytes to
// copy. Filling a slot from the DLL "for now" would produce a table that works
// until the moment it matters and then faults with no attribution at all.
//
// So the fill is done at runtime, from the port registry, not baked into the
// generated tables: the moment lane B registers `?Clean@Geometry@Graphics@@…`,
// that slot stops being a trap. No source change, no regeneration. The shadow
// report below counts, every run, how many slots are real and how many are
// still traps — that count is this lane's progress metric and lane B's
// worklist at the same time.
//
// RESIDUE DISCIPLINE (this cost a rewrite of the first draft)
//
// Nothing in this file may call ResolveExport / ResolveExportRaw /
// ResolveRuntimeRaw. All three record into the BANISHED_RT_STRICT residue
// census, and the shadow check asks about ~250 symbols on every pass — it
// would invent several hundred `[RtResidue]` / `[RtRawResidue]` entries and
// wreck the one measurement Wave 9's cutover gate actually depends on. The
// mint uses the residue-free doors instead: RtPortLookupExport /
// RtPortLookupAddr for our side, and GetProcAddress on the module handle plus
// the ASLR delta from GetModuleInfo() for the DLL's side.
// ============================================================================

#include "resolve.h"
#include "rt_data_mint.h"
#include "../runtime/types/object/TypeInfo.h"
#include "../runtime/types/system/Thread.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ============================================================
// Descriptors — the shapes tools/rt_mint_gen.py emits.
// ============================================================
enum RtMintSlotKind {
    RT_MINT_NAMED    = 0,  // the DLL exports this slot's target by name
    RT_MINT_UNNAMED  = 1,  // no export name; known only by address (lane D's pool)
    RT_MINT_PURECALL = 2,  // MSVC's pure-virtual trap, 0x18007ec28 — see below
};

typedef void (*RtMintTrapFn)();

struct RtMintVSlot {
    const char*        dllName;  // nullptr for RT_MINT_UNNAMED / RT_MINT_PURECALL
    unsigned long long dllVa;    // Ghidra VA of the target in the shipped DLL
    int                kind;
};

struct RtMintVtable {
    const char*         symbol;     // "??_7Geometry@Graphics@@6B@"
    unsigned long long  dllVa;      // Ghidra VA of the DLL's exported table
    int                 slotCount;
    const RtMintVSlot*  slots;
    void**              table;      // OUR minted storage
    const RtMintTrapFn* traps;      // one per slot, so a trap names its slot
    const char*         sizeProof;  // how slotCount was established
};

struct RtMintTypeInfo {
    const char*             symbol;
    unsigned long long      dllVa;
    RtObject::BaseTypeInfo* storage;
    const char*             name;
    const char*             parent;       // RAW, as the DLL's ctor receives it
    const char*             ext;
    const char*             factoryName;
    unsigned long long      factoryVa;
    RtMintTrapFn            factoryTrap;
    const char*             inPlaceName;
    unsigned long long      inPlaceVa;
    RtMintTrapFn            inPlaceTrap;
};

struct RtMintStorage {
    const char*        symbol;
    unsigned long long dllVa;
    unsigned int       size;
    void*              storage;
    const char*        note;
    // Non-null when the DLL's CRT CONSTRUCTS this static at load: the address
    // of the initialiser that does it. Section flags cannot tell you this --
    // every one of these is BSS in the file either way -- so the generator
    // reads the DLL's .CRT$XC array and sweeps it. A row with a ctorInit that
    // rt_data_mint.cpp does not reproduce is a build-time error in the
    // generator, not a silent zeroed mint.
    const char*        ctorInit;
    // A KNOWN CORRECT INITIAL VALUE, for a row whose retail writer stores a
    // compile-time constant. The third disposition for a row with no surviving
    // writer -- see EXTRA_INIT in tools/rt_mint_gen.py -- and the only one that
    // needs no port at all.
    const unsigned char* init;
    unsigned             initLen;
};

// ============================================================
// Traps. One function per (vtable, slot) and per (typeinfo, factory).
//
// The 30 RT_MINT_PURECALL slots are MSVC's pure-virtual-call handler: the
// concrete class never overrides that base virtual, and retail relies on it
// never being called. Lane C's recommendation, taken here: do NOT reuse
// UCRT's own anonymous _purecall. Retail's "never called" guarantee was true
// of retail's code, not automatically of a young port, and an anonymous abort
// costs someone a session working backward from a crash address to "which
// vtable, which slot". The named trap answers that in its first line.
//
// The same stub serves the not-yet-ported slots, with a different message:
// those are not unreachable-by-design, they are unbuilt, and hitting one is a
// scheduling fact (lane B has not reached that class) rather than a defect.
// ============================================================
static void RtMintTrapEnter(int vtableIndex, int slot);
static void RtMintTiTrapEnter(int typeInfoIndex, int which);

#define RT_MINT_TRAP(v, s) \
    static void RtMintTrapStub_##v##_##s() { RtMintTrapEnter(v, s); }
#define RT_MINT_TI_TRAP(t, s) \
    static void RtMintTiTrapStub_##t##_##s() { RtMintTiTrapEnter(t, s); }

#include "rt_data_mint_tables.inc"

static const int kVtableCount   = (int)(sizeof(kMintVtables)   / sizeof(kMintVtables[0]));
static const int kTypeInfoCount = (int)(sizeof(kMintTypeInfos) / sizeof(kMintTypeInfos[0]));
static const int kStorageCount  = (int)(sizeof(kMintStorage)   / sizeof(kMintStorage[0]));

// The layout the mint writes through. If any of these ever stops holding, the
// minted BaseTypeInfo objects are the wrong shape and Factory::Startup walks
// garbage — so it is a compile error, not a runtime surprise.
static_assert(sizeof(RtObject::BaseTypeInfo) == 0x38, "BaseTypeInfo must be 0x38 bytes");
static_assert(offsetof(RtObject::BaseTypeInfo, typeId) == 0x00, "BaseTypeInfo::typeId");
static_assert(offsetof(RtObject::BaseTypeInfo, name) == 0x08, "BaseTypeInfo::name");
static_assert(offsetof(RtObject::BaseTypeInfo, parentName) == 0x10, "BaseTypeInfo::parentName");
static_assert(offsetof(RtObject::BaseTypeInfo, fileExtension) == 0x18, "BaseTypeInfo::fileExtension");
static_assert(offsetof(RtObject::BaseTypeInfo, factoryFn) == 0x20, "BaseTypeInfo::factoryFn");
static_assert(offsetof(RtObject::BaseTypeInfo, inPlaceFactoryFn) == 0x28, "BaseTypeInfo::inPlaceFactoryFn");
static_assert(offsetof(RtObject::BaseTypeInfo, next) == 0x30, "BaseTypeInfo::next");
// ConstructLoader writes Loader::_loop at +0x30 by hand. That offset is only
// correct while the Thread base is exactly 0x30 bytes.
static_assert(sizeof(System::Thread) == 0x30,
              "Engine::Loader::_loop is at +0x30 because the Thread base is 0x30");

// ============================================================
// Trap bodies
// ============================================================
static void RtMintTrapDie(const char* what, const char* symbol, int slot,
                          const char* target) {
    fprintf(stderr,
        "\n[RtMint] FATAL: %s\n"
        "[RtMint]   vtable : %s\n"
        "[RtMint]   slot   : %d\n"
        "[RtMint]   target : %s\n"
        "[RtMint] A minted vtable slot was called through. Either the class was\n"
        "[RtMint] minted before its methods were ported, or a wrong-typed pointer\n"
        "[RtMint] reached this table. Nothing here is recoverable; failing fast.\n\n",
        what, symbol, slot, target ? target : "(no export name; DLL-internal)");
    fflush(stderr);
    abort();
}

static void RtMintTrapEnter(int vtableIndex, int slot) {
    const RtMintVtable& v = kMintVtables[vtableIndex];
    const RtMintVSlot&  s = v.slots[slot];
    RtMintTrapDie(s.kind == RT_MINT_PURECALL
                      ? "pure-virtual call on a minted vtable (retail leaves this "
                        "slot as _purecall; it should be unreachable)"
                      : "call through a NOT-YET-PORTED minted vtable slot",
                  v.symbol, slot, s.dllName);
}

static void RtMintTiTrapEnter(int typeInfoIndex, int which) {
    const RtMintTypeInfo& t = kMintTypeInfos[typeInfoIndex];
    RtMintTrapDie(which == 0 ? "minted BaseTypeInfo::factoryFn is not ported"
                             : "minted BaseTypeInfo::inPlaceFactoryFn is not ported",
                  t.symbol, which, which == 0 ? t.factoryName : t.inPlaceName);
}

// ============================================================
// Residue-free lookups
// ============================================================
static bool EnvOn(const char* name) {
    const char* e = getenv(name);
    return e && *e && strcmp(e, "0") != 0;
}

// The DLL's copy of an exported symbol, without touching the residue census.
static void* DllExport(const char* name) {
    const ModuleInfo* mi = GetModuleInfo(Module::Runtime);
    if (!mi || !mi->handle || !name) return nullptr;
    return (void*)GetProcAddress(mi->handle, name);
}

// A Ghidra VA translated into the loaded DLL, without touching the residue
// census. Returns nullptr when the DLL is not loaded — which is exactly the
// state cutover creates, and every caller below treats it as "no oracle".
static void* DllAddr(unsigned long long ghidraVa) {
    const ModuleInfo* mi = GetModuleInfo(Module::Runtime);
    if (!mi || !mi->handle || !ghidraVa) return nullptr;
    return (void*)(uintptr_t)((intptr_t)(uintptr_t)ghidraVa + mi->delta);
}

// A printable name for a row that may have none. An UNEXPORTED table head is
// carried by ADDRESS -- see EXTRA_VTABLES_BY_ADDR in tools/rt_mint_gen.py --
// so every message has to cope with symbol == nullptr.
static const char* SymLabel(const char* sym, unsigned long long va, char* buf, size_t n) {
    if (sym) return sym;
    snprintf(buf, n, "(unexported @0x%llx)", va);
    return buf;
}

// The DLL's copy of a minted row, by name when it has one and by address when
// it does not. The address form is not a weaker lookup -- it is the only one
// that exists for a table the DLL does not export -- but it IS a weaker CHECK:
// there is no export directory entry to cross-examine, so the "exported where
// the harvest said" comparison is skipped and counted separately rather than
// passing vacuously.
static void* DllRowBase(const char* sym, unsigned long long va) {
    return sym ? DllExport(sym) : DllAddr(va);
}

static bool DllContains(void* p) {
    const ModuleInfo* mi = GetModuleInfo(Module::Runtime);
    if (!mi || !mi->handle || !p) return false;
    // The DLL's own image span, derived from the two ends we know: the
    // preferred base and Ghidra's highest observed address. Generous on
    // purpose — this is only used to say "that qword looks like a pointer
    // into the DLL", never to dispatch.
    uintptr_t base = (uintptr_t)((intptr_t)0x180000000ll + mi->delta);
    uintptr_t v = (uintptr_t)p;
    return v >= base && v < base + 0x300000u;
}

// OUR port of a slot target, or nullptr if lane B has not registered it.
static void* OurPort(const RtMintVSlot& s) {
    if (s.kind == RT_MINT_PURECALL) return nullptr;
    if (s.kind == RT_MINT_NAMED && s.dllName) {
        void* p = RtPortLookupExport(s.dllName);
        if (p) return p;
    }
    return RtPortLookupAddr((uintptr_t)s.dllVa);
}

// ============================================================
// The mint
// ============================================================
static bool g_minted = false;

struct MintCounts {
    int slots = 0, filled = 0, pureTraps = 0, unportedTraps = 0;
    int vtablesComplete = 0;          // every non-purecall slot is ours
    int tiFactoriesFilled = 0, tiFactoriesTrapped = 0;
};
static MintCounts g_counts;

// Head of OUR BaseTypeInfo chain. Deliberately NOT the DLL's
// `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA`: head-inserting 14 minted
// duplicates into the live chain while the DLL's own static initialisers have
// already registered the same 14 types would give Factory::Startup two nodes
// per name. Ours is a separate chain until registration mode makes our
// `_firstTypeInfo` storage the only one there is.
static RtObject::BaseTypeInfo* g_mintTypeInfoHead = nullptr;

// The ctor's parent-name rule, transcribed from
// RtObject_BaseTypeInfo_Ctor (runtime/src/object/type_system.cpp), which
// itself transcribes retail 0x180006320: keep everything after the LAST ':'.
static const char* StripParent(const char* p) {
    if (!p) return nullptr;
    const char* q = p;
    for (const char* s = p; *s; ++s)
        if (*s == ':') q = s + 1;
    return q;
}

// WAVE 25 lane VTGFX. Per-row unported-trap count, recorded while MintVtables
// fills. This exists so a PROMOTION can refuse itself at runtime.
//
// THE HAZARD IT CLOSES, and it is one no build check can see (kickoff §3.6):
// a promoted vtable's slots are filled from the LIVE port registry, so any
// wave kill switch that suppresses a registration (BANISHED_RT_GRAPHICS=0,
// BANISHED_RT_GFXMAT=0, BANISHED_RT_GFXTI=0, ...) turns that row's slots into
// named abort traps -- and the promotion would then publish a TRAP TABLE into
// live objects. The build is clean, the link succeeds, and the first virtual
// call aborts. `??_7BinaryFileStream@IO@@6B@` and the other five promoted
// vtables have carried that exposure unguarded since Wave 19; this counter is
// the first thing in the tree that can answer the question at all.
static int g_vtableTraps[kVtableCount];

// The count of synthetic trap injections performed this run. Printed by the
// promotion guard as its EFFECTIVENESS COUNTER: "0 refusals" from a guard that
// was never handed anything to refuse is not a measurement.
static int g_trapControlInjections = 0;
int RtMintTrapControlInjections() { return g_trapControlInjections; }

int RtMintVtableTrapCount(const char* symbol) {
    if (!symbol) return -1;

    // ---- NEGATIVE CONTROL (Wave 25 lane SWEEP) -----------------------------
    // VTGFX shipped this counter with a refusal path that had NEVER FIRED and
    // had no control, and said so. A guard that has never refused anything is
    // an unvalidated screen: its green means nothing, and if it were wired up
    // backwards nothing in the tree would notice.
    //
    // BANISHED_RT_MINT_TRAP_CONTROL=<mangled symbol> makes THIS function report
    // a synthetic trap count for that one symbol. It corrupts EXACTLY the value
    // the guard compares -- the trap count -- and nothing else, which is the
    // rule both directions of this project's control failures come from. The
    // promotion for that symbol must then REFUSE ITSELF and say so on stderr;
    // if it promotes anyway, the guard is broken.
    //
    // Semicolon-separated list, so several rows can be armed at once, and the
    // literal `*` arms every row (the whole-harness sweep).
    const char* ctl = getenv("BANISHED_RT_MINT_TRAP_CONTROL");
    if (ctl && *ctl) {
        bool armed = (strcmp(ctl, "*") == 0);
        if (!armed) {
            const size_t len = strlen(symbol);
            for (const char* q = ctl; *q; ) {
                const char* e = strchr(q, ';');
                const size_t n = e ? (size_t)(e - q) : strlen(q);
                if (n == len && strncmp(q, symbol, n) == 0) { armed = true; break; }
                if (!e) break;
                q = e + 1;
            }
        }
        if (armed) {
            ++g_trapControlInjections;
            fprintf(stderr, "[RtMint] TRAP CONTROL: reporting a synthetic trap "
                            "count of 99 for %s -- its promotion MUST refuse.\n", symbol);
            fflush(stderr);
            return 99;
        }
    }
    for (int i = 0; i < kVtableCount; ++i)
        if (kMintVtables[i].symbol && strcmp(kMintVtables[i].symbol, symbol) == 0)
            return g_vtableTraps[i];
    return -1;   // not a minted VTABLE row -- NOT "zero traps"
}

static void MintVtables() {
    for (int vi = 0; vi < kVtableCount; ++vi) {
        const RtMintVtable& v = kMintVtables[vi];
        int missing = 0;
        for (int i = 0; i < v.slotCount; ++i) {
            const RtMintVSlot& s = v.slots[i];
            ++g_counts.slots;
            if (s.kind == RT_MINT_PURECALL) {
                v.table[i] = (void*)v.traps[i];
                ++g_counts.pureTraps;
                continue;
            }
            void* ours = OurPort(s);
            if (ours) {
                v.table[i] = ours;
                ++g_counts.filled;
            } else {
                v.table[i] = (void*)v.traps[i];
                ++g_counts.unportedTraps;
                ++missing;
            }
        }
        g_vtableTraps[vi] = missing;
        if (missing == 0) ++g_counts.vtablesComplete;
    }
}

// WAVE 10C lane TIDENT. How many minted rows should have their `name` aliased
// to the DLL's own pointer, as the ptr= screen's effectiveness control. Read
// once, from BANISHED_RT_MINT_PTRALIAS; 0 (the default) is off.
static int g_ptrAliasRows = 0;

static void MintTypeInfos() {
    {
        const char* e = getenv("BANISHED_RT_MINT_PTRALIAS");
        g_ptrAliasRows = e ? atoi(e) : 0;
        if (g_ptrAliasRows > 0)
            printf("[RtMint] BANISHED_RT_MINT_PTRALIAS=%d -- aliasing the first "
                   "%d rows' name pointer to the DLL's, to prove the ptr= screen "
                   "is not stuck at \"differs\"\n",
                   g_ptrAliasRows, g_ptrAliasRows);
    }
    int aliasApplied = 0;
    // Fields first. `next` is left for the chain pass below — it is the one
    // field the harvest could NOT read (it is 0x0 on disk for all 14 because
    // the chain is built by running constructors), so it is the one field that
    // has to come from OUR order rather than from the DLL.
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        RtObject::BaseTypeInfo* bti = t.storage;
        // ---- typeId: the REAL id, not the ctor sentinel --------------------
        //
        // This line used to be `bti->typeId = 0xFFFFFFFFu`, faithfully copying
        // what retail's BaseTypeInfo ctor writes, on the reasoning that only
        // Factory::Startup may overwrite it. That was faithful and WRONG for a
        // minted node, and Wave 10 lane UIDATA measured the consequence: our
        // nodes hang off g_mintTypeInfoHead, a chain no Startup walks, so the
        // sentinel was not a temporary state -- it was the final value, and
        // every reader of a registered `?_typeInfo@` (all of them read exactly
        // this dword) would have taken 0xFFFFFFFF where retail has a CRC. No
        // crash, no log line, just lookups that never match. UIDATA recorded
        // that as unfixable short of cutover.
        //
        // It is fixable. The id is not information the DLL holds that we
        // cannot recover: it is a pure function of `name`, and we already have
        // that function -- Startup's own `info->typeId = TypeNameCRC(info->name)`
        // (runtime/src/object/type_system.cpp), now callable as
        // RtObject::TypeIdFromName. Stamping it here is not a new claim about
        // retail; it is Startup's line, run earlier, on a node Startup will
        // never reach.
        //
        // Faithfulness: the value is IDENTICAL to what a Startup walk would
        // write, so if our chain ever does get walked, the walk is idempotent.
        // The transition 0xFFFFFFFF -> CRC is not observable by anyone: nothing
        // reads a minted node between MintTypeInfos() and registration.
        //
        // And it is CHECKED, not asserted: the shadow report below now compares
        // this dword against the loaded DLL's own node for all 104 rows instead
        // of printing it as INFO. That comparison is the whole warrant for
        // promoting a `?_typeInfo@` row.
        bti->typeId        = t.name ? RtObject::TypeIdFromName(t.name) : 0xFFFFFFFFu;
        bti->_pad04        = 0;
        bti->name          = t.name;
        // WAVE 10C lane TIDENT -- the ptr= screen's EFFECTIVENESS CONTROL.
        //
        // `t.name` is a string literal in OUR image, so `bti->name` is by
        // construction a DIFFERENT POINTER from the DLL node's `name`, for all
        // 104 rows. A screen that reports "differs" for every row cannot tell
        // "it read the value" from "it is stuck on yes" -- so the control has
        // to drive it the OTHER way. BANISHED_RT_MINT_PTRALIAS=<n> re-points
        // the first n rows' `name` at the DLL's own pointer; the ptr= screen
        // must then report those n rows as IDENTICAL and the rest as differing.
        // Default off, and it names itself in the log when it is on.
        if (g_ptrAliasRows > 0 && aliasApplied < g_ptrAliasRows) {
            RtObject::BaseTypeInfo* d =
                (RtObject::BaseTypeInfo*)DllExport(t.symbol);
            if (d && d->name) {
                bti->name = d->name;
                ++aliasApplied;
                printf("[RtMint] PTRALIAS %s: name -> the DLL's own pointer %p "
                       "(effectiveness control; content unchanged)\n",
                       t.symbol, (const void*)d->name);
            }
        }
        bti->parentName    = StripParent(t.parent);
        bti->fileExtension = t.ext;

        void* f = t.factoryName ? RtPortLookupExport(t.factoryName) : nullptr;
        if (!f) f = RtPortLookupAddr((uintptr_t)t.factoryVa);
        if (f) { bti->factoryFn = f; ++g_counts.tiFactoriesFilled; }
        else   { bti->factoryFn = (void*)t.factoryTrap; ++g_counts.tiFactoriesTrapped; }

        void* p = t.inPlaceName ? RtPortLookupExport(t.inPlaceName) : nullptr;
        if (!p) p = RtPortLookupAddr((uintptr_t)t.inPlaceVa);
        if (p) { bti->inPlaceFactoryFn = p; ++g_counts.tiFactoriesFilled; }
        else   { bti->inPlaceFactoryFn = (void*)t.inPlaceTrap; ++g_counts.tiFactoriesTrapped; }

        bti->next = nullptr;
    }
    // The control must announce when it did NOT fire, or a run in which
    // DllExport came back null would read as "the screen did not move" and be
    // scored as the screen's failure rather than the control's.
    if (g_ptrAliasRows > 0)
        printf("[RtMint] PTRALIAS: %d of %d requested row(s) aliased%s\n",
               aliasApplied, g_ptrAliasRows,
               aliasApplied == 0 ? "  <-- CONTROL DID NOT FIRE; the ptr= count "
                                   "below is NOT evidence either way" : "");

    // The chain. Factory::Startup (0x1800c0630) walks it once, CRCs each
    // name, and resolves each type's PARENT by name — so a walk that reaches a
    // child before its parent is the order-dependent failure lane C warned
    // about. We cannot know retail's linker order and do not need to: we can
    // build an order that is safe by construction, with every parent ahead of
    // its children in walk order. Head-insert in reverse, so the walk comes
    // out parents-first.
    //
    // The relation is by NAME (a parent may live outside these 14, in which
    // case the type is a root as far as this chain is concerned) and the graph
    // is tiny, so an O(n^2) insertion sort is the whole algorithm.
    int order[kTypeInfoCount];
    bool placed[kTypeInfoCount];
    for (int i = 0; i < kTypeInfoCount; ++i) placed[i] = false;
    int n = 0;
    for (int pass = 0; pass < kTypeInfoCount && n < kTypeInfoCount; ++pass) {
        for (int i = 0; i < kTypeInfoCount; ++i) {
            if (placed[i]) continue;
            const char* parent = StripParent(kMintTypeInfos[i].parent);
            bool parentPending = false;
            if (parent && *parent) {
                for (int j = 0; j < kTypeInfoCount; ++j) {
                    if (placed[j] || j == i) continue;
                    if (kMintTypeInfos[j].name && strcmp(kMintTypeInfos[j].name, parent) == 0) {
                        parentPending = true;
                        break;
                    }
                }
            }
            if (!parentPending) { order[n++] = i; placed[i] = true; }
        }
    }
    // A cycle would leave entries unplaced. Impossible for a class hierarchy,
    // but silence here would be a chain missing types, so say it out loud.
    for (int i = 0; i < kTypeInfoCount; ++i) {
        if (!placed[i]) {
            fprintf(stderr, "[RtMint] WARNING: %s left out of the type chain "
                            "(parent cycle?)\n", kMintTypeInfos[i].symbol);
            order[n++] = i;
        }
    }
    g_mintTypeInfoHead = nullptr;
    for (int k = n - 1; k >= 0; --k) {           // reverse head-insert =>
        RtObject::BaseTypeInfo* b = kMintTypeInfos[order[k]].storage;
        b->next = g_mintTypeInfoHead;            // walk order is order[0..n)
        g_mintTypeInfoHead = b;                  // i.e. parents first
    }
}

// ============================================================
// Constructed statics
//
// The sweep behind this (tools/rt_mint_gen.py, find_crt_init_table +
// scan_ctor_refs) reads the DLL's own .CRT$XC array -- 106 initialisers -- and
// asks, per exported DATA symbol, whether any of them writes it. That is the
// only honest way to answer "is this really just BSS": the section flags say
// BSS for a constructed object too, and a file-based harvest can never see the
// difference. Result, all 81 swept:
//
//   * 14 ?_typeInfo@ + ?_firstTypeInfo@ -- constructed, and MintTypeInfos()
//     already reproduces them.
//   * ?_loader@LoaderLoop@Engine@@ -- constructed by ONE initialiser
//     (0x180148fe0), reproduced here.
//   * ?_rng@Particle@Graphics@@ -- NOT constructed by anything. This corrects
//     lane C2's own first-pass claim that it needed a seed: retail never seeds
//     it, so the mint's zeroed storage is exactly the loaded DLL's state, and
//     ADDING a seed would be the divergence. See WAVE9_LANE_C2.md.
//   * every singleton and every other .data static -- not constructed.
//
// Gated, because constructing ?_loader starts a real OS thread and three
// events. Off in an ordinary run; BANISHED_RT_MINT_CTOR=1 runs the
// constructions WITHOUT registering the mint, so the shadow check can compare
// what we built against the DLL's live object while both exist.
// ============================================================
static void* g_loaderStorage = nullptr;

static void RtMintLoaderTeardown() {
    // Retail's own atexit (0x18014a580) is ??1Thread with the Thread vtable
    // restamped first -- which is precisely what our ported dtor does.
    typedef void (*ThreadDtorFn)(void*);
    ThreadDtorFn dtor = (ThreadDtorFn)RtPortLookupExport("??1Thread@System@@QEAA@XZ");
    if (dtor && g_loaderStorage) dtor(g_loaderStorage);
    g_loaderStorage = nullptr;
}

// FUN_180148fe0, transcribed:
//     System::Thread::Thread(&_loader, 0x40000);   // 256 KB stack
//     _loader.vtable = ??_7Loader@Engine@@6B@;
//     _loader._loop  = nullptr;                    // +0x30
//     atexit(FUN_18014a580);
static bool ConstructLoader(const RtMintStorage& s) {
    typedef void* (*ThreadCtorFn)(void*, unsigned int);
    ThreadCtorFn ctor = (ThreadCtorFn)RtPortLookupExport("??0Thread@System@@QEAA@I@Z");
    if (!ctor) {
        fprintf(stderr, "[RtMint] CANNOT construct %s: ??0Thread@System@@QEAA@I@Z "
                        "is not ported. Left zeroed -- its vtable is NULL.\n", s.symbol);
        return false;
    }
    void** vt = nullptr;
    for (int i = 0; i < kVtableCount; ++i)
        // `symbol &&` is NOT optional: a vtable row carried by ADDRESS has a
        // NULL symbol by design (lane C2, c77b26f -- the IO::Encoding table at
        // 0x1801554a8 is unexported, so contents+address is all it has).  This
        // was the tree's ONLY unguarded strcmp on a .symbol field; every other
        // site in this file already wrote `s.symbol && strcmp(...)`.  The
        // second controlled DLL-absent boot faulted here in ucrtbase with
        // RCX=0, and nothing before it could: this path runs only under
        // BANISHED_RT_MINT / _MINT_CTOR, and the last such run predated the
        // first null-symbol row.
        if (kMintVtables[i].symbol &&
            strcmp(kMintVtables[i].symbol, "??_7Loader@Engine@@6B@") == 0)
            vt = kMintVtables[i].table;
    if (!vt) {
        fprintf(stderr, "[RtMint] CANNOT construct %s: ??_7Loader@Engine@@6B@ was "
                        "not minted.\n", s.symbol);
        return false;
    }
    ctor(s.storage, 0x40000);
    *(void**)s.storage = (void*)vt;                       // +0x00
    // _loop is a void* (UpdateLoop*), 8 bytes -- this was *(int*)0 until
    // Wave 9, which zeroed only the LOW HALF.  Harmless in practice only
    // because the storage is BSS and already zero, so nothing ever
    // depended on it; but a 4-byte write to an 8-byte pointer field is
    // this project's wild-write family in miniature, and it would have
    // become real the moment this object was ever reconstructed in place.
    *(void**)((char*)s.storage + 0x30) = nullptr;         // Loader::_loop
    g_loaderStorage = s.storage;
    atexit(&RtMintLoaderTeardown);
    return true;
}

// FUN_180149650, the initialiser that seeds ?_rng@Particle@Graphics@@,
// transcribed:
//
//     mt[0] = (uint32)_time64(NULL);
//     for (i = 1; i < 0x270; ++i)
//         mt[i] = 0x6c078965 * (mt[i-1] ^ (mt[i-1] >> 30)) + i;
//
// i.e. MT19937's standard init_genrand, seeded from the WALL CLOCK. Two
// consequences, both worth stating because this lane got them wrong in both
// directions before measuring:
//
//  * The VALUE cannot match retail's and does not need to. The seed is
//    time(), so two runs of the SHIPPED DLL do not match each other either.
//    Nobody should later "fix" a mismatch here.
//  * Seeding at all is NOT optional. An all-zero MT19937 twists to an all-zero
//    state forever (NextGroup, 0x1800c7dd0, is a pure twist with no seeding
//    path), so zeroed storage would make every Graphics::Particle draw return
//    0 -- plausible numbers, no crash, no symptom a test would catch.
//
// `index` at +0x00 is deliberately NOT written: retail leaves it zero from BSS,
// which is what forces the first Rand() to regenerate.
static bool ConstructParticleRng(const RtMintStorage& s) {
    if (s.size < 4 + 624 * 4) {
        fprintf(stderr, "[RtMint] CANNOT construct %s: minted storage is %u bytes, "
                        "MT19937 needs %u\n", s.symbol, s.size, 4u + 624u * 4u);
        return false;
    }
    unsigned int* mt = (unsigned int*)((char*)s.storage + 4);   // +0x04 .. +0x9c4
    mt[0] = (unsigned int)time(nullptr);
    for (unsigned int i = 1; i < 624; ++i)
        mt[i] = 0x6c078965u * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
    return true;
}

static int g_constructed = 0;

static void MintConstructedStatics() {
    if (!(EnvOn("BANISHED_RT_MINT") || EnvOn("BANISHED_RT_MINT_CTOR"))) return;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        if (!s.ctorInit) continue;
        if (s.symbol && strcmp(s.symbol, "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA") == 0) {
            ++g_constructed;              // MintTypeInfos() already built it
        } else if (s.symbol && strcmp(s.symbol, "?_loader@LoaderLoop@Engine@@1VLoader@2@A") == 0) {
            if (ConstructLoader(s)) ++g_constructed;
        } else if (s.symbol && strcmp(s.symbol, "?_rng@Particle@Graphics@@1VRandom@System@@A") == 0) {
            if (ConstructParticleRng(s)) ++g_constructed;
        } else {
            // Unreachable via the generator, which refuses to emit an
            // unhandled ctorInit. Kept because "unreachable" is exactly the
            // claim this project has learned not to trust.
            char sbuf2[64];
            fprintf(stderr, "[RtMint] FATAL: %s is CONSTRUCTED by the DLL at %s and "
                            "nothing here reproduces it; zeroed storage would "
                            "silently differ from the loaded DLL.\n",
                    SymLabel(s.symbol, s.dllVa, sbuf2, sizeof(sbuf2)), s.ctorInit);
            fflush(stderr);
            abort();
        }
    }
    int owed = 0;
    for (int i = 0; i < kStorageCount; ++i)
        if (kMintStorage[i].ctorInit) ++owed;
    printf("[RtMint] constructed %d of the %d statics the DLL's CRT builds at load\n",
           g_constructed, owed);
}

static void MintStorage() {
    // Nothing to copy. Every one of these 28 symbols is BSS in the DLL — the
    // harvest read b'' from all of them — so "minting" is owning zeroed bytes
    // and letting the existing writer fill them. kStorageBytes is a static
    // array and therefore already zero; this loop exists to say so, and to
    // seed the one entry that has a non-null initial value in OUR world.
    int seeded = 0;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        memset(s.storage, 0, s.size);
        if (s.init && s.initLen && s.initLen <= s.size) {
            memcpy(s.storage, s.init, s.initLen);
            ++seeded;
        }
        // ?_firstTypeInfo@ is the head of the reflection chain. In the DLL it
        // is filled by static initialisers; ours is filled by MintTypeInfos.
        if (s.symbol && strcmp(s.symbol, "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA") == 0)
            *(RtObject::BaseTypeInfo**)s.storage = g_mintTypeInfoHead;
    }
    if (seeded)
        printf("[RtMint] seeded %d storage row(s) with a known constant value\n", seeded);
}

// Registration. OFF unless BANISHED_RT_MINT=1, and it must stay off until
// cutover: while Runtime-x64.dll is loaded, the DLL's own code reads the DLL's
// own singletons, so handing OUR callers a second, empty set of singletons
// splits the process's state in half. Wired now, with the DLL present, so the
// last commit of the wave is a flag flip and not new code.
// BOTH DOORS, always. RegisterRuntimeExport only redirects a caller that asks
// by NAME; 30 sites in this tree ask for exported DATA by ADDRESS instead
// (`ResolveAddr(Module::Runtime, 0x1801fe7b8)` and friends), and those would
// have walked straight past a mint registered under a name only -- the same
// shape of miss as round 5's raw-door reads, one door over. Registering the
// address costs one map insert per row and closes it.
//
// It is also the ONLY door for a row with no exported name at all.
//
// ---------------------------------------------------------------------------
// PER-ROW SELECTIVITY (Wave 10 lane MINTSEL)
// ---------------------------------------------------------------------------
//
// Until this change RegisterMintedData() was ONE switch over all 197 rows, and
// that made a per-row question unanswerable in practice. Lane UIDATA hit it
// head on: the four UI vtables were not gated because anyone had judged UI
// unsafe, they were gated because they shared a switch with 184 other rows --
// and the switch does not even boot. Its blocker
// (?_singleton@MemoryAllocator@System@@ published as our zeroed storage, then
// EnterCriticalSection on a null self) lives entirely in the 38 STORAGE rows
// and has nothing to do with any vtable or any BaseTypeInfo.
//
// So registration is now addressable one row at a time, through THREE doors,
// in increasing order of commitment:
//
//   1. RtMintRegisterRow(symbol) -- the primitive. Both doors (name + address)
//      for exactly one row, idempotent, returns false for an unknown symbol.
//      Everything below goes through it, so there is no second copy of the
//      "register both doors" rule to drift.
//
//   2. BANISHED_RT_MINT_ROWS=<csv> -- registers the named rows and nothing
//      else. This is the measuring instrument: it lets a candidate row be run
//      in a real boot WITHOUT the storage rows that make the global switch
//      fatal. A token may be a full mangled symbol or a `pre*` prefix.
//
//   3. standalone/reg_mint_promoted.cpp -- rows PROMOTED, i.e. registered
//      unconditionally on every run because they have been measured safe.
//      A promoted row is a port by this project's own rule (not gated), and
//      the census can see it because that file spells the symbol as a literal
//      at a RegisterRuntimeExport call site.
//
// BANISHED_RT_MINT=1 keeps its old meaning exactly: all 197 rows. It remains a
// hazard, not a route.
// ---------------------------------------------------------------------------

// One addressable row, whatever table it came from.
struct RtMintRow {
    const char*        symbol;    // may be nullptr (address-only rows)
    unsigned long long dllVa;
    void*              storage;
    const char*        kind;      // "vtable" / "typeinfo" / "storage"
    int                index;     // index within that kind's table
};

static bool MintRowAt(int kind, int i, RtMintRow* out) {
    if (kind == 0) {
        if (i < 0 || i >= kVtableCount) return false;
        const RtMintVtable& v = kMintVtables[i];
        *out = { v.symbol, v.dllVa, (void*)v.table, "vtable", i };
    } else if (kind == 1) {
        if (i < 0 || i >= kTypeInfoCount) return false;
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        *out = { t.symbol, t.dllVa, (void*)t.storage, "typeinfo", i };
    } else {
        if (i < 0 || i >= kStorageCount) return false;
        const RtMintStorage& s = kMintStorage[i];
        *out = { s.symbol, s.dllVa, s.storage, "storage", i };
    }
    return true;
}
static int MintRowCount(int kind) {
    return kind == 0 ? kVtableCount : kind == 1 ? kTypeInfoCount : kStorageCount;
}

static bool FindMintRow(const char* symbol, RtMintRow* out) {
    if (!symbol) return false;
    for (int k = 0; k < 3; ++k)
        for (int i = 0; i < MintRowCount(k); ++i) {
            RtMintRow r;
            MintRowAt(k, i, &r);
            if (r.symbol && strcmp(r.symbol, symbol) == 0) { *out = r; return true; }
        }
    return false;
}

// Register ONE row through both doors. `?_loader@` carries two interior
// offsets that an exact-match address registry cannot cover, so they travel
// with their row rather than living in a second loop that could be forgotten.
// WAVE 11 FOLLOW-UP 5 (lane TINFO). Every row registered outside
// reg_mint_promoted.cpp is ARMED FOR MEASUREMENT, and the readiness report
// prints it for the same reason it prints a promoted one. The bookkeeping lives
// HERE, at the one choke point both callers go through -- putting it in
// RtMintRegisterRow missed the BANISHED_RT_MINT_ROWS loop, which calls this
// function directly, so the first armed run printed no line at all.
static const char* g_armedRows[64];
static int         g_armedRowCount = 0;

bool RtMintRowIsArmed(const char* symbol) {
    if (!symbol) return false;
    for (int i = 0; i < g_armedRowCount; ++i)
        if (strcmp(g_armedRows[i], symbol) == 0) return true;
    return false;
}

static int RegisterOneMintRow(const RtMintRow& r) {
    if (r.symbol && !RtMintRowIsArmed(r.symbol) &&
        g_armedRowCount < (int)(sizeof(g_armedRows) / sizeof(g_armedRows[0])))
        g_armedRows[g_armedRowCount++] = r.symbol;
    int doors = 0;
    if (r.symbol) { RegisterRuntimeExport(r.symbol, r.storage); ++doors; }
    RegisterRuntimeAddr((uintptr_t)r.dllVa, r.storage); ++doors;
    if (r.symbol && strcmp(r.symbol, "?_loader@LoaderLoop@Engine@@1VLoader@2@A") == 0) {
        // engine/funcs/engine_funcs.cpp reads Loader::runFlags at +0x08 and
        // Loader::hFinished at +0x28 by their own addresses.
        RegisterRuntimeAddr((uintptr_t)r.dllVa + 0x08, (char*)r.storage + 0x08);
        RegisterRuntimeAddr((uintptr_t)r.dllVa + 0x28, (char*)r.storage + 0x28);
        doors += 2;
    }
    return doors;
}

bool RtMintRegisterRow(const char* symbol) {
    RtMintRow r;
    if (!FindMintRow(symbol, &r)) return false;
    RegisterOneMintRow(r);
    return true;
}

// ---- promotion bookkeeping -------------------------------------------------
//
// A PROMOTED row is registered on every run, so the two doors are opened from
// standalone/reg_mint_promoted.cpp with the symbol spelled as a LITERAL at the
// RegisterRuntimeExport call site. That is deliberate and it is not
// decoration: tools/runtime_census.py excludes this file (MINT_FILE) from the
// ported side entirely, and its non-literal scan cannot see a name that
// arrives as `v.symbol` from a table walk. A promotion registered from in here
// would be live in the process and invisible to the census -- the exact shape
// of reference_half_registered_port_reads_as_absent.md.
//
// So this function's job is only the CHECKING half: prove the symbol is really
// a minted row, prove the VA the promotion file spells matches the generated
// table, and record it so the readiness report can mark it. If the generator
// ever drops or moves the row, the promotion dies loudly at boot instead of
// registering storage for something else.
static const char* g_promoted[64];
static int         g_promotedCount = 0;

bool RtMintRowIsPromoted(const char* symbol) {
    if (!symbol) return false;
    for (int i = 0; i < g_promotedCount; ++i)
        if (strcmp(g_promoted[i], symbol) == 0) return true;
    return false;
}

void* RtMintPromotionStorage(const char* symbol, unsigned long long expectVa) {
    RtMintRow r;
    if (!FindMintRow(symbol, &r)) {
        fprintf(stderr, "[RtMint] FATAL: reg_mint_promoted.cpp promotes \"%s\", "
                        "which is NOT a minted row. Either the symbol is "
                        "misspelled or tools/rt_mint_gen.py no longer emits it.\n",
                symbol);
        fflush(stderr);
        abort();
    }
    if (r.dllVa != expectVa) {
        fprintf(stderr, "[RtMint] FATAL: reg_mint_promoted.cpp promotes \"%s\" at "
                        "0x%llx, but the generated table says 0x%llx. Registering "
                        "the wrong address would hand one symbol's callers another "
                        "symbol's storage.\n", symbol, expectVa, r.dllVa);
        fflush(stderr);
        abort();
    }
    if (g_promotedCount < (int)(sizeof(g_promoted) / sizeof(g_promoted[0])))
        g_promoted[g_promotedCount++] = r.symbol;
    return r.storage;
}

void* RtMintRowStorage(const char* symbol) {
    RtMintRow r;
    return FindMintRow(symbol, &r) ? r.storage : nullptr;
}

unsigned long long RtMintRowDllVa(const char* symbol) {
    RtMintRow r;
    return FindMintRow(symbol, &r) ? r.dllVa : 0ull;
}

// BANISHED_RT_MINT_ROWS=<csv>. A token matches a row's symbol exactly, or as a
// prefix when it ends in '*'. Every token MUST match at least one row: a typo
// that silently registers nothing would look exactly like "the row is safe",
// which is the failure mode this whole lane exists to avoid.
static int RegisterMintedRowsFromEnv() {
    const char* spec = getenv("BANISHED_RT_MINT_ROWS");
    if (!spec || !*spec) return 0;
    int rows = 0;
    const char* p = spec;
    while (*p) {
        while (*p == ',' || *p == ' ') ++p;
        const char* q = p;
        while (*q && *q != ',') ++q;
        int len = (int)(q - p);
        while (len > 0 && p[len - 1] == ' ') --len;
        if (len > 0) {
            bool prefix = (p[len - 1] == '*');
            int mlen = prefix ? len - 1 : len;
            int hits = 0;
            for (int k = 0; k < 3; ++k)
                for (int i = 0; i < MintRowCount(k); ++i) {
                    RtMintRow r;
                    MintRowAt(k, i, &r);
                    if (!r.symbol) continue;
                    bool match = prefix ? (strncmp(r.symbol, p, (size_t)mlen) == 0)
                                        : ((int)strlen(r.symbol) == mlen &&
                                           strncmp(r.symbol, p, (size_t)mlen) == 0);
                    if (!match) continue;
                    RegisterOneMintRow(r);
                    ++hits;
                    ++rows;
                    printf("[RtMint] ROW ON (env): %-8s %s\n", r.kind, r.symbol);
                }
            if (hits == 0) {
                fprintf(stderr, "[RtMint] FATAL: BANISHED_RT_MINT_ROWS token \"%.*s\" "
                                "matches no minted row. A token that matches nothing "
                                "registers nothing, which reads exactly like a row "
                                "that was tested and found safe.\n", len, p);
                fflush(stderr);
                abort();
            }
        }
        p = q;
    }
    printf("[RtMint] BANISHED_RT_MINT_ROWS registered %d row(s) selectively\n", rows);
    return rows;
}

static void RegisterMintedData() {
    int names = 0, addrs = 0;
    for (int i = 0; i < kVtableCount; ++i) {
        const RtMintVtable& v = kMintVtables[i];
        if (v.symbol) { RegisterRuntimeExport(v.symbol, (void*)v.table); ++names; }
        RegisterRuntimeAddr((uintptr_t)v.dllVa, (void*)v.table); ++addrs;
    }
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        if (t.symbol) { RegisterRuntimeExport(t.symbol, (void*)t.storage); ++names; }
        RegisterRuntimeAddr((uintptr_t)t.dllVa, (void*)t.storage); ++addrs;
    }
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        if (s.symbol) { RegisterRuntimeExport(s.symbol, s.storage); ++names; }
        RegisterRuntimeAddr((uintptr_t)s.dllVa, s.storage); ++addrs;
    }
    // Interior fields our tree resolves by address. A minted object's BASE
    // address is registered above, but `engine/funcs/engine_funcs.cpp` reads
    // Loader::runFlags at _loader+0x08 and Loader::hFinished at +0x28 by their
    // own addresses, and an exact-match address registry does not cover an
    // interior offset. Registering the two it actually asks for is the whole
    // fix; a new interior read would need a row here, which is why the audit
    // reports them.
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        if (!s.symbol || strcmp(s.symbol, "?_loader@LoaderLoop@Engine@@1VLoader@2@A") != 0)
            continue;
        RegisterRuntimeAddr((uintptr_t)s.dllVa + 0x08, (char*)s.storage + 0x08);
        RegisterRuntimeAddr((uintptr_t)s.dllVa + 0x28, (char*)s.storage + 0x28);
        addrs += 2;
    }
    printf("[RtMint] registered %d by name, %d by address\n", names, addrs);
}

// ============================================================
// WAVE 26 LANE FACTORY — THE FACTORY-FAMILY ROSTER
// ============================================================
//
// Attempt 26 died on ONE minted BaseTypeInfo factory and named it. A cutover
// boot is a GATE, not a PLANNER (reference_the_boot_is_a_gate_not_a_planner.md)
// — it can only name what it reached before faulting, so at one name per
// attempt the family behind this wall costs one boot per row to enumerate.
//
// This screen enumerates it in ONE run, and it does so through the SAME two
// doors MintTypeInfos() itself uses (RtPortLookupExport by name, then
// RtPortLookupAddr by VA) — it re-reads the verdict the mint already recorded
// in the storage block, so it CANNOT disagree with the mint. That is the point:
// a second implementation of "is it ported?" would be a second thing to be
// wrong (reference_two_checks_one_hidden_premise.md).
//
// WHAT IT EXAMINES, stated because a screen that cannot name its subject
// manufactures work (reference_a_screen_that_cannot_name_its_subject.md): all
// kTypeInfoCount minted `?_typeInfo@...@` rows, both factory slots of each, by
// comparing the minted `factoryFn`/`inPlaceFactoryFn` against that row's own
// trap stub address. Rows whose factory name is nullptr (`Base@Object@` is one)
// have NO factory in retail either and are counted separately, not as unported.
//
// It is NOT a promotion screen and NOT a route: a filled slot means the mint
// found OUR body, not that anything dispatches through it.
static void RtMintReportTypeInfoFactories() {
    int nullName = 0, filled = 0, trapped = 0;
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        const RtObject::BaseTypeInfo* b = t.storage;
        if (!t.factoryName && !t.factoryVa) ++nullName;
        else if (b->factoryFn == (void*)t.factoryTrap) ++trapped; else ++filled;
        if (!t.inPlaceName && !t.inPlaceVa) ++nullName;
        else if (b->inPlaceFactoryFn == (void*)t.inPlaceTrap) ++trapped; else ++filled;
    }
    printf("[RtMint] BaseTypeInfo factories: %d rows x 2 slots = %d; %d PORTED, "
           "%d ABORT-TRAP, %d absent in retail too (no factory name)\n",
           kTypeInfoCount, kTypeInfoCount * 2, filled, trapped, nullName);
    if (trapped)
        printf("[RtMint] %d trapped factory slot(s) remain: EVERY ONE of them is "
               "a cutover wall of the shape attempt 26 hit. Set "
               "BANISHED_RT_MINT_FACTORYLIST=1 to print the roster.\n", trapped);

    if (!EnvOn("BANISHED_RT_MINT_FACTORYLIST")) return;
    printf("[RtMint] --- unported minted BaseTypeInfo factories ---\n");
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        const RtObject::BaseTypeInfo* b = t.storage;
        if (t.factoryName && b->factoryFn == (void*)t.factoryTrap)
            printf("[RtMint]   [%3d] slot0 0x%09llx  %s\n", i,
                   (unsigned long long)t.factoryVa, t.factoryName);
        if (t.inPlaceName && b->inPlaceFactoryFn == (void*)t.inPlaceTrap)
            printf("[RtMint]   [%3d] slot1 0x%09llx  %s\n", i,
                   (unsigned long long)t.inPlaceVa, t.inPlaceName);
    }
    printf("[RtMint] --- end roster ---\n");
    fflush(stdout);
}

void RtDataMintInit() {
    if (g_minted) return;
    g_minted = true;

    MintVtables();
    MintTypeInfos();
    MintStorage();

    // REGISTRATION MUST PRECEDE CONSTRUCTION. Found by Wave 9 lane SYS2 while
    // chasing ??_7Thread@System@@6B@, which read as an unminted vtable and was
    // nothing of the kind -- the mint has always carried that row.
    //
    // MintConstructedStatics() reproduces the three statics the DLL's CRT builds
    // at load, and ConstructLoader() among them CONSTRUCTS A System::Thread. That
    // constructor resolves ??_7Thread@System@@6B@. With RegisterMintedData() still
    // to come, the resolve missed, and the call site cached the null in a plain
    // function-local static -- so the symbol stayed null for the life of the
    // process even though the mint published it microseconds later. The port was
    // correct, the mint was correct, and the ORDER was wrong.
    //
    // Registering first is safe: RegisterMintedData only publishes pointers to
    // storage and vtables that MintStorage/MintVtables have already laid out, and
    // it constructs nothing. Doing it here is what lets the three constructors
    // below resolve minted symbols at all. (MintConstructedStatics still has to
    // follow MintStorage, which memsets the block -- that ordering is unchanged.)
    //
    // Note the two [RtMint] log lines now appear in the opposite order: the
    // "registered N by name" line precedes "constructed N of the M statics".
    if (EnvOn("BANISHED_RT_MINT")) RegisterMintedData();

    // PROMOTED rows: registered on every run, with no env var, because they
    // have been measured safe one at a time. See standalone/reg_mint_promoted.cpp
    // for the per-row warrant. Skipped when the global switch already
    // registered everything, so the counts do not double-report.
    if (!EnvOn("BANISHED_RT_MINT")) RtMintRegisterPromotedRows();

    // Selective, opt-in, for MEASURING a candidate row before promoting it.
    // After the promoted rows, so a promoted row named here is a harmless
    // no-op rather than a different binding.
    RegisterMintedRowsFromEnv();

    // WAVE 25 lane MANIFEST -- the 46 RT_UNPROMOTABLE-tagged gap rows, whose
    // verdicts are ROUTE-SCOPED. 44 of them INVERT to REQUIRED once the DLL is
    // gone; two must still be held. standalone/cutover_manifest.cpp publishes
    // exactly the ones the generated manifest says to publish and NAMES the
    // ones it holds, which the blanket BANISHED_RT_MINT switch cannot do.
    //
    // Placed HERE for the same reason RegisterMintedData() is placed above
    // MintConstructedStatics(): registration must precede construction, or the
    // Thread ctor inside ConstructLoader() caches a null for the life of the
    // process (see the banner above RegisterMintedData).
    RtCutoverPublishManifest(EnvOn("BANISHED_RT_CUTOVER_MANIFEST"));

    MintConstructedStatics();   // after MintStorage: it memsets the block

    printf("[RtMint] minted %d vtables (%d slots: %d ported, %d unported-trap, "
           "%d purecall-trap), %d BaseTypeInfo, %d storage symbols (%zu bytes)\n",
           kVtableCount, g_counts.slots, g_counts.filled, g_counts.unportedTraps,
           g_counts.pureTraps, kTypeInfoCount, kStorageCount, sizeof(kStorageBytes));
    printf("[RtMint] %d of %d vtables are COMPLETE (every non-purecall slot is "
           "our port); registration is %s\n",
           g_counts.vtablesComplete, kVtableCount,
           EnvOn("BANISHED_RT_MINT") ? "ON (BANISHED_RT_MINT=1)"
                                     : "off (set BANISHED_RT_MINT=1 to go live)");

    RtMintReportTypeInfoFactories();

    // RegisterMintedData() has already run, above MintConstructedStatics().
    if (EnvOn("BANISHED_RT_MINT_SHADOW")) RtDataMintShadowReport("init");
}

// ============================================================
// The shadow check — BANISHED_RT_MINT_SHADOW=1
//
// What it can and cannot compare, stated before the code, because getting
// this wrong is how an oracle reports 100% green while doing nothing:
//
//  * A minted vtable CANNOT be compared byte-for-byte against the DLL's. A
//    filled slot holds OUR function's address, which is a different number
//    from the DLL's by construction. Comparing the bytes would fail on every
//    slot that actually represents progress and pass on every slot that does
//    not. The comparison that means something is the MAPPING: for each slot,
//    the DLL's table must hold the function the harvest recorded, and our
//    table must hold either our registered port OF THAT SAME function or that
//    slot's trap.
//
//  * A singleton's bytes are compared against NOTHING. All 28 storage symbols
//    are BSS the DLL only ever writes at runtime; a byte comparison there is
//    the "oracle that compares bytes neither side writes" failure, which
//    reports either 100% green or 100% red and means neither. What IS worth
//    recording is which of them the running DLL has actually populated by the
//    time the report runs — ground truth that also dies at cutover.
//
//  * `BaseTypeInfo::next` is excluded, for the same reason with the opposite
//    sign: the DLL's `next` is a real value built by ITS static-init order,
//    ours is built by OURS, and they cannot agree. Every other field is a
//    link-time constant and IS compared.
//
//  * `BaseTypeInfo::typeId` is compared only before Factory::Startup has run.
//    Startup overwrites the DLL's sentinel with the real CRC and never touches
//    ours, so at "exit" phase a difference is expected and is reported as
//    information, not as a mismatch.
//
// EFFECTIVENESS CONTROL. The report prints, separately, how many comparisons
// were made and how many of them could have failed. "239 slots checked" means
// nothing on its own if 239 of them are traps compared against traps: the
// number that measures the port is `minted from our ported code`, and the
// report says so in as many words when it is zero.
// ============================================================
void RtDataMintShadowReport(const char* phase) {
    // "exit" relaxes exactly two comparisons -- BaseTypeInfo::typeId and the
    // constructed-static SHAPE -- because by then the DLL's objects have been
    // USED and ours have not.  Both are documented at their sites.
    const bool atExit = phase && !strcmp(phase, "exit");
    (void)atExit;
    // Opt-in at both call sites (RtDataMintInit and the exit dump in
    // standalone/main.cpp), so an ordinary run is byte-identical to one built
    // without this file.
    if (!EnvOn("BANISHED_RT_MINT_SHADOW")) return;
    if (!g_minted) {
        printf("[RtMintShadow] nothing minted yet (RtDataMintInit has not run)\n");
        return;
    }
    const ModuleInfo* mi = GetModuleInfo(Module::Runtime);
    const bool haveDll = mi && mi->handle;

    printf("\n[RtMintShadow] ==== phase=%s  DLL=%s ====\n", phase,
           haveDll ? "present (oracle live)" : "ABSENT (post-cutover; oracle gone)");
    if (!haveDll) {
        printf("[RtMintShadow] The DLL is not loaded, so there is nothing to compare\n"
               "[RtMintShadow] against. This is the expected state AFTER cutover and\n"
               "[RtMintShadow] the reason this check had to be run BEFORE it.\n");
        return;
    }

    int ground = 0, groundBad = 0, byAddrRows = 0;      // harvest vs the DLL's live table
    int ours = 0, oursBad = 0;          // our slot vs our registered port
    int traps = 0, sizeSuspect = 0, absent = 0;

    for (int vi = 0; vi < kVtableCount; ++vi) {
        const RtMintVtable& v = kMintVtables[vi];
        char lbuf[64];
        const char* label = SymLabel(v.symbol, v.dllVa, lbuf, sizeof(lbuf));
        void** dll = (void**)DllRowBase(v.symbol, v.dllVa);
        if (!dll) {
            ++absent;
            printf("[RtMintShadow] MISSING: %s is not in the DLL's export directory\n",
                   label);
            continue;
        }
        // The DLL's exported table must sit where the harvest said it did. If
        // it does not, every slot VA below is being read from the wrong place
        // and the rest of this vtable's result is meaningless.
        //
        // Only askable of a row that HAS an export. For an address-keyed row
        // the two sides are the same computation, so the comparison would pass
        // without doing any work -- counted, not performed.
        if (!v.symbol) {
            ++byAddrRows;
        } else if ((void*)dll != DllAddr(v.dllVa)) {
            ++groundBad;
            printf("[RtMintShadow] MISMATCH %s: exported at %p, harvest says %p\n",
                   label, (void*)dll, DllAddr(v.dllVa));
            continue;
        }
        for (int i = 0; i < v.slotCount; ++i) {
            const RtMintVSlot& s = v.slots[i];
            ++ground;
            void* expect = DllAddr(s.dllVa);
            if (dll[i] != expect) {
                ++groundBad;
                printf("[RtMintShadow] MISMATCH %s slot %d: DLL holds %p, harvest "
                       "recorded %p (%s)\n", label, i, dll[i], expect,
                       s.dllName ? s.dllName : "unnamed");
            }
            void* mine = v.table[i];
            if (mine == (void*)v.traps[i]) {
                ++traps;
                continue;
            }
            ++ours;
            void* port = OurPort(s);
            if (mine != port || mine == dll[i]) {
                ++oursBad;
                printf("[RtMintShadow] MISMATCH %s slot %d: minted %p, port registry "
                       "says %p%s\n", label, i, mine, port,
                       mine == dll[i] ? " — AND IT IS THE DLL'S OWN ADDRESS, which a "
                                        "minted slot must never hold" : "");
            }
        }
        // Slot COUNT.  A count that is too short mints a table shorter than the
        // class's real vtable, and the missing slots dispatch into whatever
        // follows in .rdata.  The generator proved each count one of two ways,
        // and the live re-check MUST match the proof it is re-checking:
        //
        //   adjacency  - the table ends where the next exported vtable begins.
        //                Re-check the ADDRESS: &dll[slotCount] must be exactly
        //                that neighbour's address.  (Until Wave 9 this arm ran
        //                the non-adjacency test below instead, which asks
        //                whether the qword past the end is a DLL pointer.  For
        //                an adjacency-proved table that qword IS the neighbour's
        //                slot 0, so the test was true BY CONSTRUCTION and fired
        //                on all 23 such vtables - a permanent red FAIL over zero
        //                defects.  All 23 were confirmed correct offline:
        //                gap-to-next-export == our minted count, every one.)
        //   otherwise  - nothing exported follows, so fall back to asking whether
        //                the qword past the end even looks like a DLL pointer.
        //
        // gap > slotCount is deliberately NOT a defect: .rdata padding between
        // two vtables is legal, so a larger gap is inconclusive rather than
        // short.  gap < slotCount IS one - we minted past the neighbour.
        const char* adj = (v.sizeProof && !strncmp(v.sizeProof, "adjacent-export:", 16))
                              ? v.sizeProof + 16 : nullptr;
        if (adj) {
            void* nextVt = DllExport(adj);
            void* end    = (void*)&dll[v.slotCount];
            if (!nextVt) {
                ++sizeSuspect;
                printf("[RtMintShadow] SIZE SUSPECT %s: size proof names %s, which is "
                       "NOT exported - the proof cannot be re-checked\n",
                       label, adj);
            } else if (end > nextVt) {
                ++sizeSuspect;
                printf("[RtMintShadow] SIZE SUSPECT %s: minted %d slots, which runs %lld "
                       "qwords PAST %s - the table is too long\n",
                       label, v.slotCount,
                       (long long)(((char*)end - (char*)nextVt) / 8), adj);
            }
        } else if (v.sizeProof && !strncmp(v.sizeProof, "ctor-stamped-neighbour:", 23)) {
            // Wave 19 lane MINT. This row is bounded by ANOTHER TABLE whose own
            // constructor names its address, not by the next export. So the
            // qword after the last slot IS a DLL pointer BY CONSTRUCTION - it is
            // the neighbour's slot 0 - and the tail probe below would flag every
            // such row forever. A screen that is permanently red is a screen
            // nobody reads, which is worse than the defect it was added for.
            //
            // The check INVERTS rather than being suppressed: if the tail is NOT
            // a DLL pointer then the neighbour this bound rests on is not there,
            // and the extent is the thing to doubt.
            void* tail = dll[v.slotCount];
            if (!DllContains(tail)) {
                ++sizeSuspect;
                printf("[RtMintShadow] SIZE SUSPECT %s: minted %d slots bounded by a "
                       "ctor-stamped neighbour, but the qword after the last slot "
                       "(%p) is NOT a DLL pointer - the neighbouring table the bound "
                       "rests on is not there [%s]\n", label, v.slotCount, tail,
                       v.sizeProof);
            }
        } else {
            void* tail = dll[v.slotCount];
            if (DllContains(tail)) {
                ++sizeSuspect;
                printf("[RtMintShadow] SIZE SUSPECT %s: minted %d slots but the qword "
                       "after the last one (%p) is a DLL pointer - the real table may "
                       "be longer [%s]\n", v.symbol, v.slotCount, tail,
                       v.sizeProof ? v.sizeProof : "no size proof");
            }
        }
    }

    // ---- BaseTypeInfo -----------------------------------------------------
    int tiFields = 0, tiBad = 0, tiIdInfo = 0;
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        RtObject::BaseTypeInfo* dll = (RtObject::BaseTypeInfo*)DllExport(t.symbol);
        RtObject::BaseTypeInfo* mine = t.storage;
        if (!dll) {
            ++absent;
            printf("[RtMintShadow] MISSING: %s is not exported\n", t.symbol);
            continue;
        }
        struct { const char* what; const char* a; const char* b; } str[] = {
            { "name",          mine->name,          dll->name },
            { "parentName",    mine->parentName,    dll->parentName },
            { "fileExtension", mine->fileExtension, dll->fileExtension },
        };
        for (auto& f : str) {
            ++tiFields;
            bool same = (f.a == nullptr && f.b == nullptr) ||
                        (f.a && f.b && strcmp(f.a, f.b) == 0);
            if (!same) {
                ++tiBad;
                printf("[RtMintShadow] MISMATCH %s.%s: minted \"%s\", DLL \"%s\"\n",
                       t.symbol, f.what, f.a ? f.a : "(null)", f.b ? f.b : "(null)");
            }
        }
        // The two factory pointers: ground truth only. Ours is our port (or a
        // trap) and so is a different address on purpose.
        ++tiFields;
        if (t.factoryVa && dll->factoryFn != DllAddr(t.factoryVa)) {
            ++tiBad;
            printf("[RtMintShadow] MISMATCH %s.factoryFn: DLL holds %p, harvest "
                   "recorded %p (%s)\n", t.symbol, dll->factoryFn,
                   DllAddr(t.factoryVa), t.factoryName ? t.factoryName : "?");
        }
        ++tiFields;
        if (t.inPlaceVa && dll->inPlaceFactoryFn != DllAddr(t.inPlaceVa)) {
            ++tiBad;
            printf("[RtMintShadow] MISMATCH %s.inPlaceFactoryFn: DLL holds %p, "
                   "harvest recorded %p (%s)\n", t.symbol, dll->inPlaceFactoryFn,
                   DllAddr(t.inPlaceVa), t.inPlaceName ? t.inPlaceName : "?");
        }
        // ---- typeId: now a REAL comparison, not an INFO line ------------
        //
        // Wave 10 lane MINTSEL. This used to be an unconditional INFO line,
        // because the mint stamped the ctor sentinel 0xFFFFFFFF and so could
        // never agree with the DLL. MintTypeInfos() now stamps
        // RtObject::TypeIdFromName(name) -- Factory::Startup's own hash -- so
        // the two SHOULD be equal and a difference is a defect, not a note.
        //
        // The one difference that is genuinely not a defect: the DLL's own
        // node still holds 0xFFFFFFFF because ITS Factory::Startup has not run
        // yet (this report can be asked for at "init", inside RuntimePortInit,
        // long before Engine::Startup step 6). That is a fact about WHEN we
        // looked, not about the value, so it is counted separately and named
        // as such. Folding it into the pass count would let a genuinely wrong
        // id hide behind an early report.
        ++tiFields;
        if (dll->typeId == 0xFFFFFFFFu) {
            ++tiIdInfo;
            printf("[RtMintShadow] INFO %s.typeId: minted 0x%08x, DLL still holds "
                   "the ctor sentinel at phase=%s (its Factory::Startup has not "
                   "run) -- NOT compared\n", t.symbol, mine->typeId, phase);
        } else if (mine->typeId != dll->typeId) {
            ++tiBad;
            printf("[RtMintShadow] MISMATCH %s.typeId: minted 0x%08x, DLL 0x%08x "
                   "(name \"%s\")\n", t.symbol, mine->typeId, dll->typeId,
                   t.name ? t.name : "(null)");
        }
        // next: NOT compared. See the header comment.
    }

    // ---- storage ----------------------------------------------------------
    int live = 0, dead = 0, ourNonZero = 0, shapeChecked = 0, shapeBad = 0, shapeInfo = 0;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        char sbuf[64];
        const char* slabel = SymLabel(s.symbol, s.dllVa, sbuf, sizeof(sbuf));
        void** dll = (void**)DllRowBase(s.symbol, s.dllVa);
        if (!dll) { ++absent; printf("[RtMintShadow] MISSING: %s is not exported\n", slabel); continue; }
        if (!s.symbol) {
            ++byAddrRows;
        } else if ((void*)dll != DllAddr(s.dllVa)) {
            printf("[RtMintShadow] MISMATCH %s: exported at %p, harvest says %p\n",
                   slabel, (void*)dll, DllAddr(s.dllVa));
        }
        // WHERE the first non-zero byte is, not just whether there is one.
        // "DLL=WRITTEN" on a 2500-byte object says nothing about WHAT was
        // written: a live MT19937 state and one stray byte 2000 bytes in look
        // identical. Lane C2's claim that Particle::_rng is never seeded was
        // exactly the kind of statement that boolean cannot check -- and it was
        // this print, added after the boolean raised the question, that showed
        // the first non-zero byte at +0x04 with index still 0 and refuted it.
        // Reported for anything bigger than a pointer.
        bool dllLive = false;
        unsigned dllFirst = 0;
        for (unsigned b = 0; b < s.size; ++b)
            if (((unsigned char*)dll)[b]) { dllLive = true; dllFirst = b; break; }
        bool mineLive = false;
        for (unsigned b = 0; b < s.size; ++b)
            if (((unsigned char*)s.storage)[b]) { mineLive = true; break; }
        if (dllLive) ++live; else ++dead;
        if (mineLive) ++ourNonZero;
        if (s.size > 8 && dllLive) {
            const unsigned char* d = (const unsigned char*)dll;
            printf("[RtMintShadow]   FIRST-NONZERO %s: DLL byte +0x%x = %02x %02x %02x %02x "
                   "%02x %02x %02x %02x\n", slabel, dllFirst,
                   d[dllFirst], dllFirst + 1 < s.size ? d[dllFirst + 1] : 0,
                   dllFirst + 2 < s.size ? d[dllFirst + 2] : 0,
                   dllFirst + 3 < s.size ? d[dllFirst + 3] : 0,
                   dllFirst + 4 < s.size ? d[dllFirst + 4] : 0,
                   dllFirst + 5 < s.size ? d[dllFirst + 5] : 0,
                   dllFirst + 6 < s.size ? d[dllFirst + 6] : 0,
                   dllFirst + 7 < s.size ? d[dllFirst + 7] : 0);
        }
        printf("[RtMintShadow] storage %-52s %2u bytes  DLL=%s  minted=%s  ctor=%s  (%s)\n",
               slabel, s.size, dllLive ? "WRITTEN" : "still zero",
               mineLive ? "written" : "zero",
               s.ctorInit ? s.ctorInit : "none (plain BSS)", s.note);

        // For a CONSTRUCTED static there IS something to compare, and it is
        // not the values: our vtable pointer, our HANDLEs and the DLL's are
        // different numbers by construction. What must agree is the SHAPE --
        // which qwords the constructor leaves non-null. That is what catches
        // the failure that matters (our ctor not creating the handles, or
        // leaving the vtable null), and it is honest about being weaker than
        // a value comparison. Only meaningful once the ctors have actually
        // run, i.e. under BANISHED_RT_MINT_CTOR / BANISHED_RT_MINT.
        if (s.ctorInit && g_constructed > 0 && s.size >= 8) {
            unsigned words = s.size / 8, differ = 0;
            for (unsigned k = 0; k < words; ++k) {
                bool a = ((unsigned long long*)dll)[k] != 0;
                bool b = ((unsigned long long*)s.storage)[k] != 0;
                if (a != b) {
                    ++differ;
                    // At EXIT this is expected and is INFORMATION, not a
                    // mismatch -- the same policy this file already applies to
                    // BaseTypeInfo::typeId, and for the same structural reason.
                    // The DLL's object is the one in SERVICE; ours is inert by
                    // design while the DLL is present, so a field the live
                    // object acquires during the run has nothing to acquire it
                    // on our side.  Loader::_loop (+0x30) is exactly that: the
                    // UpdateLoop the loader is loading FOR, null until someone
                    // asks it to load one.  Comparing a USED object against an
                    // UNUSED one is the "must not compare what neither side
                    // writes" trap wearing its other face -- here ONE side
                    // writes, by design.
                    //
                    // At INIT it stays a hard mismatch: right after the ctors
                    // run, both objects have had exactly one thing done to
                    // them, and that IS comparable.  This run had 0 SHAPE
                    // MISMATCH at init and 1 at exit -- which is itself the
                    // evidence that the field is written after construction.
                    printf("[RtMintShadow]   %s %s +0x%02x: DLL %s, minted %s\n",
                           atExit ? "SHAPE-INFO" : "SHAPE", slabel, k * 8,
                           a ? "set" : "null", b ? "set" : "null");
                }
            }
            ++shapeChecked;
            if (differ && !atExit) ++shapeBad;
            if (differ && atExit) ++shapeInfo;
        }
    }

    int ctorOwed = 0;
    for (int i = 0; i < kStorageCount; ++i)
        if (kMintStorage[i].ctorInit) ++ctorOwed;

    // ---- the summary, with its effectiveness control ----------------------
    // ---- PER-VTABLE READINESS (Wave 10 lane UIDATA) -----------------------
    //
    // The aggregate "195 of 379 non-purecall slots carry our own code" answers
    // a wave-wide question. It cannot answer the question a lane actually asks,
    // which is about ONE row: "is ??_7Element@UI@@6B@ safe to register today?"
    // Registration is per-symbol in principle (RegisterMintedData walks rows),
    // so the blocker is per-row too, and an aggregate hides which rows are
    // clean. This loop names them.
    //
    // It is also the LIVE answer to a question a source grep gets wrong. Lane
    // UIDATA first counted these four UI vtables' trap slots by grepping the
    // tree for `RegisterRuntimeExport("<slot name>")`, which is the literal-
    // grep screen this project has been burned by repeatedly
    // (reference_literal_grep_blind_to_variables.md): a registration whose
    // name arrives through a variable is invisible to it. This loop asks the
    // PORT REGISTRY itself, through the same OurPort() the mint fills with, so
    // it sees every door. The two agreed on all four UI rows — which is a
    // result, not an assumption, and is only a result because both were run.
    printf("\n[RtMintShadow] ---- per-vtable readiness ----\n");
    printf("[RtMintShadow] READY = every non-purecall slot is our port, so registering\n"
           "[RtMintShadow] this row hands callers a table with no abort-trap in it.\n");
    for (int vi = 0; vi < kVtableCount; ++vi) {
        const RtMintVtable& v = kMintVtables[vi];
        char rbuf[64];
        int rPort = 0, rTrap = 0, rPure = 0;
        for (int i = 0; i < v.slotCount; ++i) {
            if (v.slots[i].kind == RT_MINT_PURECALL) { ++rPure; continue; }
            if (OurPort(v.slots[i])) ++rPort; else ++rTrap;
        }
        printf("[RtMintShadow]   %-8s %-52s slots=%2d ported=%2d TRAP=%2d purecall=%2d%s\n",
               rTrap ? "BLOCKED" : "READY",
               SymLabel(v.symbol, v.dllVa, rbuf, sizeof(rbuf)),
               v.slotCount, rPort, rTrap, rPure,
               RtMintRowIsPromoted(v.symbol) ? "  [PROMOTED]"
                   : RtMintRowIsArmed(v.symbol) ? "  [ARMED for measurement]" : "");
        // NAME the trapped slots. "BLOCKED, 1 TRAP" is a number; the lane that
        // has to expire the blocker needs the symbol. Deliberately from the
        // LIVE registry (OurPort) and the generated table, never from a source
        // grep -- a slot registered under a name held in a variable is
        // invisible to a grep and would be reported as unported work that is
        // already done.
        for (int i = 0; rTrap && i < v.slotCount; ++i) {
            if (v.slots[i].kind == RT_MINT_PURECALL || OurPort(v.slots[i])) continue;
            printf("[RtMintShadow]              TRAP slot %2d  %s\n", i,
                   v.slots[i].dllName ? v.slots[i].dllName : "(unexported; known by address only)");
            if (!v.slots[i].dllName)
                printf("[RtMintShadow]                            DLL 0x%llx\n",
                       v.slots[i].dllVa);
        }
    }

    // ---- PER-TYPEINFO READINESS (Wave 10 lane MINTSEL) --------------------
    //
    // A `?_typeInfo@` row has no slots, so the vtable loop above says nothing
    // about it, and lane UIDATA's verdict on the nine UI ones ("blocked by a
    // silent wrong value, and nothing short of cutover fixes it") had no
    // instrument behind it -- it was read off the INFO lines that the mint's
    // 0xFFFFFFFF sentinel produced by construction.
    //
    // The criterion here is stated rather than assumed, because it is narrow:
    //
    //   READY-id  every consumer in this tree reads the FIRST DWORD and
    //             nothing else, and that dword is now verified EQUAL to the
    //             loaded DLL's. `idcmp` says whether the comparison was
    //             actually made this run (the DLL's node still holds the ctor
    //             sentinel before its own Factory::Startup, and an
    //             un-run comparison must never read as a pass).
    //
    //   fac=      the two factory pointers. Ours is a port or a TRAP; the DLL
    //             has its own. These are reachable only through a Factory tree
    //             our minted node is not in, so a trap here does not block a
    //             dword reader -- but it IS the difference between "safe for
    //             the consumers we have" and "safe for any consumer", and
    //             collapsing the two into one word is how a row gets promoted
    //             for a use nobody checked. It is printed, never hidden.
    //
    // strs= counts name/parentName/fileExtension disagreements with the DLL.
    printf("\n[RtMintShadow] ---- per-typeinfo readiness ----\n");
    printf("[RtMintShadow] READY = every string field and the typeId AGREE with the\n"
           "[RtMintShadow] loaded DLL this run. fac=trap is reported, not hidden: it\n"
           "[RtMintShadow] is unreachable for a dword reader but not for a tree walk.\n");
    int tiReady = 0, tiBlocked = 0;
    int tiPtrFields = 0, tiPtrDiff = 0, tiPtrDiffPromoted = 0;
    for (int i = 0; i < kTypeInfoCount; ++i) {
        const RtMintTypeInfo& t = kMintTypeInfos[i];
        RtObject::BaseTypeInfo* dll  = (RtObject::BaseTypeInfo*)DllExport(t.symbol);
        RtObject::BaseTypeInfo* mine = t.storage;
        int    strBad = 0;
        int    ptrDiff = 0;                     // lane TIDENT: identity, not content
        bool   idCompared = false, idOk = false;
        if (dll) {
            const char* a[3] = { mine->name, mine->parentName, mine->fileExtension };
            const char* b[3] = { dll->name,  dll->parentName,  dll->fileExtension };
            for (int k = 0; k < 3; ++k) {
                bool same = (a[k] == nullptr && b[k] == nullptr) ||
                            (a[k] && b[k] && strcmp(a[k], b[k]) == 0);
                if (!same) ++strBad;
                // SCORED SEPARATELY from strBad, and never folded into it or
                // into `ready`. strs= answers "does the string SAY the same
                // thing"; ptr= answers "is it the SAME OBJECT". A screen that
                // only ever computed strcmp is structurally incapable of
                // expressing the second answer, whatever value it prints.
                if (a[k] != b[k]) ++ptrDiff;
            }
            tiPtrFields += 3;
            tiPtrDiff   += ptrDiff;
            if (ptrDiff && RtMintRowIsPromoted(t.symbol)) ++tiPtrDiffPromoted;
            if (dll->typeId != 0xFFFFFFFFu) {
                idCompared = true;
                idOk = (mine->typeId == dll->typeId);
            }
        }
        int facTrap = 0;
        if (mine->factoryFn        == (void*)t.factoryTrap) ++facTrap;
        if (mine->inPlaceFactoryFn == (void*)t.inPlaceTrap) ++facTrap;
        const bool ready = dll && strBad == 0 && idCompared && idOk;
        if (ready) ++tiReady; else ++tiBlocked;
        // Only the UI rows and anything not READY are printed: 104 rows of
        // green would bury the four lines a reader needs.
        const bool isUi = t.symbol && strstr(t.symbol, "@UI@@") != nullptr;
        // ...and any row PROMOTED or ARMED for measurement this run: a
        // candidate whose whole purpose is to be read on this screen printed no
        // line at all the moment it went READY (WAVE 11 follow-up 5, lane TINFO).
        if (!ready || isUi || RtMintRowIsPromoted(t.symbol) || RtMintRowIsArmed(t.symbol))
            printf("[RtMintShadow]   %-8s %-58s id=0x%08x idcmp=%-3s strs=%d ptr=%d "
                   "fac=%s%s\n",
                   ready ? "READY" : "BLOCKED",
                   t.symbol ? t.symbol : "(unexported)",
                   mine->typeId, idCompared ? "yes" : "NO", strBad, ptrDiff,
                   facTrap == 0 ? "ported" : facTrap == 1 ? "1trap" : "2traps",
                   RtMintRowIsPromoted(t.symbol) ? "  [PROMOTED]"
                       : RtMintRowIsArmed(t.symbol) ? "  [ARMED for measurement]" : "");
    }
    printf("[RtMintShadow]   typeinfo rows: %d READY, %d BLOCKED (of %d; only the "
           "UI rows and the BLOCKED ones are listed)\n", tiReady, tiBlocked,
           kTypeInfoCount);

    // ---- ptr=  the POINTER-IDENTITY screen (WAVE 10C lane TIDENT) ---------
    //
    // WHY THIS EXISTS. Until this wave the only string screen was `strs=`, a
    // strcmp. Lane DLGV declined to promote ?_typeInfo@DialogController@ on the
    // ground that promoting it makes ?TypeName@DialogController@ hand out OUR
    // node's `name` POINTER instead of the DLL's -- and observed that the
    // instrument which had green-lit eleven already-live rows compares CONTENT,
    // so it could not have seen that failure mode had it mattered. It could
    // not: `strs=0` is a true statement about the characters and says nothing
    // whatever about identity. That is the shape of
    // reference_diagnostic_field_cannot_express_the_answer.md, so the field is
    // now separate, computed separately, and never folded into `ready`.
    //
    // WHAT IT REPORTS, AND WHY A NON-ZERO ptr= IS NOT A DEFECT. `bti->name` is
    // a string literal in our own image, so ptr= is 3-of-3 or 2-of-3 for
    // essentially every row and always will be. It is reported because the
    // question "does anything compare these pointers?" is answerable, was
    // answered, and the answer belongs beside the number:
    // docs/runtime_port/WAVE10C_LANE_TIDENT.md is a census of every consumer of
    // BaseTypeInfo::name in BOTH shipped binaries, and every one is a CONTENT
    // consumer. In Runtime-x64.dll the only static readers of any node's +0x08
    // are the 103 ?TypeName@ accessors themselves, none of which is CALLED
    // anywhere in the DLL; Factory::Startup CRCs the string, and
    // Factory::GetTypeName has no caller inside the DLL and is not imported by
    // the exe at all. In Application-x64.exe all 525 references to the 42
    // imported ?_typeInfo@ slots read the dword at +0x00 (the typeId) and NOT
    // ONE reads +0x08.
    //
    // So ptr= is a standing statement of a known, measured, benign difference,
    // in a column that CAN say otherwise -- not a pass hiding behind a metric
    // that could only ever have said one thing.
    printf("[RtMintShadow]   ptr= (identity, scored SEPARATELY from strs=): "
           "%d of %d string fields hold a DIFFERENT POINTER than the DLL's node; "
           "%d PROMOTED row(s) hand out at least one\n",
           tiPtrDiff, tiPtrFields, tiPtrDiffPromoted);
    printf("[RtMintShadow]   ^ EXPECTED, and measured to be harmless: every "
           "consumer of BaseTypeInfo::name in both shipped binaries compares "
           "CONTENT (see WAVE10C_LANE_TIDENT.md). Control: "
           "BANISHED_RT_MINT_PTRALIAS=<n> must drive this count DOWN by n.\n");

    printf("\n[RtMintShadow] ---- summary (phase=%s) ----\n", phase);
    printf("[RtMintShadow] vtables            : %d, %d slots\n", kVtableCount, g_counts.slots);
    printf("[RtMintShadow] ground truth       : %d slot comparisons vs the live DLL, "
           "%d MISMATCH\n", ground, groundBad);
    printf("[RtMintShadow] minted from OUR    : %d slots, %d MISMATCH   <-- the only "
           "number that measures the port\n", ours, oursBad);
    printf("[RtMintShadow] minted as traps    : %d (%d purecall by design, %d not yet "
           "ported)\n", traps, g_counts.pureTraps, g_counts.unportedTraps);
    printf("[RtMintShadow] complete vtables   : %d of %d\n",
           g_counts.vtablesComplete, kVtableCount);
    printf("[RtMintShadow] slot-count suspects: %d\n", sizeSuspect);
    printf("[RtMintShadow] BaseTypeInfo       : %d field comparisons, %d MISMATCH, "
           "%d typeId not-yet-comparable (DLL still at the ctor sentinel); "
           "next: NOT compared, built from our init order\n",
           tiFields, tiBad, tiIdInfo);
    printf("[RtMintShadow] storage            : %d symbols, %d written by the DLL, "
           "%d still zero, %d of ours written (contents NOT compared: BSS)\n",
           kStorageCount, live, dead, ourNonZero);
    printf("[RtMintShadow] constructed statics: %d of %d built (shape compared on %d, "
           "%d SHAPE MISMATCH, %d SHAPE-INFO)   [BANISHED_RT_MINT_CTOR=1 to build "
           "them]\n", g_constructed, ctorOwed, shapeChecked, shapeBad, shapeInfo);
    printf("[RtMintShadow] symbols absent from the DLL: %d\n", absent);
    printf("[RtMintShadow] carried by ADDRESS   : %d row(s) with no exported name -- "
           "the export-location cross-check does not apply to them and was not "
           "counted as a pass\n", byAddrRows);

    if (ours == 0) {
        printf("[RtMintShadow] *** EFFECTIVENESS: ZERO slots were minted from our own\n"
               "[RtMintShadow] *** ported code. Every green line above is the harvest\n"
               "[RtMintShadow] *** agreeing with the DLL it was read from. This run\n"
               "[RtMintShadow] *** proves the ground truth, NOT the port.\n");
    } else {
        printf("[RtMintShadow] effectiveness: %d of %d non-purecall slots carry our own\n"
               "[RtMintShadow] code; the remaining %d are lane B's worklist.\n",
               ours, ours + g_counts.unportedTraps, g_counts.unportedTraps);
    }
    const int bad = groundBad + oursBad + tiBad + sizeSuspect + absent + shapeBad;
    printf("[RtMintShadow] ==== %s: %d problem(s) ====\n\n",
           bad ? "FAIL" : "clean", bad);
    fflush(stdout);
}

// ============================================================================
// WAVE 11 FOLLOW-UP 5 (lane TINFO) — "is this BaseTypeInfo one of OURS?"
// ============================================================================
//
// Follow-up 2 made the 104 minted factory abort traps REACHABLE in principle:
// our ported `?Load@External@Object@@` creates objects through
// `BaseTypeInfo::factoryFn` (+0x20), and a promoted row whose factory is not
// ported carries `RtMintTiTrapStub_*`, which aborts.
//
// Whether a promotion actually puts a minted node in front of one of those
// dispatch sites is a question about POINTERS, not about names: every dispatch
// site in this tree reaches `info` by walking `?_singleton@Factory@Object@@`'s
// AVL tree and reading `TypeNode+0x10`. This predicate is what lets
// runtime/src/object/factory_dispatch_probe.cpp answer it by measurement
// instead of by argument — it says whether a given `BaseTypeInfo*` lies inside
// the minted storage array at all.
//
// It is an ADDRESS-RANGE test on our own static array, so it is exact, cheap,
// and independent of whether the row is registered — which matters, because
// the question is asked on runs where nothing is promoted.
bool RtMintIsMintedTypeInfo(const void* p) {
    const unsigned char* q  = (const unsigned char*)p;
    const unsigned char* lo = (const unsigned char*)&kTypeInfoStorage[0];
    const unsigned char* hi = lo + sizeof(kTypeInfoStorage);
    return q >= lo && q < hi;
}

// The probe's POSITIVE CONTROL for the predicate above: a pointer that MUST
// test true. Without it, a screen reporting "0 minted nodes dispatched" cannot
// be told apart from a predicate that is stuck on `false`.
const void* RtMintTypeInfoStorageProbe(int index) {
    if (index < 0 || index >= kTypeInfoCount) return nullptr;
    return &kTypeInfoStorage[index];
}

// ============================================================================
// WAVE 24 lane TINFO -- public enumeration of the minted BaseTypeInfo rows
// ============================================================================
// standalone/tinfo_door_screen.cpp needs to walk all 104 `?_typeInfo@` rows and
// ask, per row, what OUR NAME DOOR hands a caller. There was no public
// enumerator: every existing walk is a file-static loop in here. This exposes
// the one the screen needs and nothing more (symbol + storage), so the screen
// cannot reach into the table mutable internals.
int RtMintTypeInfoRowCount() { return kTypeInfoCount; }

bool RtMintTypeInfoRowAt(int i, const char** symbol, void** storage) {
    if (i < 0 || i >= kTypeInfoCount) return false;
    if (symbol)  *symbol  = kMintTypeInfos[i].symbol;
    if (storage) *storage = (void*)kMintTypeInfos[i].storage;
    return true;
}
