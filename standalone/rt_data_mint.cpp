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
// Nothing in this file may call ResolveExport. It records into the residue
// census, and the mint asks about ~250 symbols on every pass — it would invent
// several hundred `[RtResidue]` entries and wreck the one measurement the
// cutover gate depends on. The mint uses the residue-free doors instead:
// RtPortLookupExport / RtPortLookupAddr.
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

// A printable name for a row that may have none. An UNEXPORTED table head is
// carried by ADDRESS -- see EXTRA_VTABLES_BY_ADDR in tools/rt_mint_gen.py --
// so every message has to cope with symbol == nullptr.
static const char* SymLabel(const char* sym, unsigned long long va, char* buf, size_t n) {
    if (sym) return sym;
    snprintf(buf, n, "(unexported @0x%llx)", va);
    return buf;
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
// registration that does not land turns that row's slots into
// named abort traps -- and the promotion would then publish a TRAP TABLE into
// live objects. The build is clean, the link succeeds, and the first virtual
// call aborts. `??_7BinaryFileStream@IO@@6B@` and the other five promoted
// vtables have carried that exposure unguarded since Wave 19; this counter is
// the first thing in the tree that can answer the question at all.
static int g_vtableTraps[kVtableCount];

int RtMintVtableTrapCount(const char* symbol) {
    if (!symbol) return -1;

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

static void MintTypeInfos() {
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
// events.
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
        // RCX=0, and nothing before it could: the last run down this path
        // predated the first null-symbol row.
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

// Registration. It must stay off until
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
// Registration is also addressable one row at a time, through
// RtMintRegisterRow(symbol): both doors (name + address) for exactly one row,
// idempotent, returns false for an unknown symbol.
// standalone/cutover_manifest.cpp publishes its REQUIRED rows through it.
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
// WAVE 11 FOLLOW-UP 5 (lane TINFO). Which rows were registered individually,
// for the readiness report. The bookkeeping lives HERE, at the one choke point
// every caller goes through.
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
// A PROMOTED row opens its two doors from standalone/reg_mint_promoted.cpp with
// the symbol spelled as a LITERAL at the RegisterRuntimeExport call site, so
// tools/runtime_census.py can see it. This function is only the CHECKING half:
// prove the symbol is really a minted row, prove the VA the promotion file
// spells matches the generated table, and record it. If the generator ever
// drops or moves the row, the promotion dies loudly at boot instead of
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
    if (!trapped) return;
    printf("[RtMint] %d trapped factory slot(s) remain: EVERY ONE of them is a "
           "cutover wall of the shape attempt 26 hit.\n", trapped);
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
    RegisterMintedData();

    // WAVE 25 lane MANIFEST -- the 46 RT_UNPROMOTABLE-tagged gap rows, whose
    // verdicts are ROUTE-SCOPED. 44 of them INVERT to REQUIRED once the DLL is
    // gone; two must still be held. standalone/cutover_manifest.cpp publishes
    // exactly the ones the generated manifest says to publish and NAMES the
    // ones it holds.
    //
    // Placed HERE for the same reason RegisterMintedData() is placed above
    // MintConstructedStatics(): registration must precede construction, or the
    // Thread ctor inside ConstructLoader() caches a null for the life of the
    // process (see the banner above RegisterMintedData).
    RtCutoverPublishManifest();

    MintConstructedStatics();   // after MintStorage: it memsets the block

    printf("[RtMint] minted %d vtables (%d slots: %d ported, %d unported-trap, "
           "%d purecall-trap), %d BaseTypeInfo, %d storage symbols (%zu bytes)\n",
           kVtableCount, g_counts.slots, g_counts.filled, g_counts.unportedTraps,
           g_counts.pureTraps, kTypeInfoCount, kStorageCount, sizeof(kStorageBytes));
    printf("[RtMint] %d of %d vtables are COMPLETE (every non-purecall slot is "
           "our port)\n", g_counts.vtablesComplete, kVtableCount);

    RtMintReportTypeInfoFactories();
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
