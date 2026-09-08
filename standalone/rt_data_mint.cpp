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
        bti->typeId        = 0xFFFFFFFFu;   // the ctor's sentinel; only
        bti->_pad04        = 0;             // Factory::Startup overwrites it
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
        if (strcmp(kMintVtables[i].symbol, "??_7Loader@Engine@@6B@") == 0)
            vt = kMintVtables[i].table;
    if (!vt) {
        fprintf(stderr, "[RtMint] CANNOT construct %s: ??_7Loader@Engine@@6B@ was "
                        "not minted.\n", s.symbol);
        return false;
    }
    ctor(s.storage, 0x40000);
    *(void**)s.storage = (void*)vt;                       // +0x00
    *(int*)((char*)s.storage + 0x30) = 0;                 // Loader::_loop
    g_loaderStorage = s.storage;
    atexit(&RtMintLoaderTeardown);
    return true;
}

static int g_constructed = 0;

static void MintConstructedStatics() {
    if (!(EnvOn("BANISHED_RT_MINT") || EnvOn("BANISHED_RT_MINT_CTOR"))) return;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        if (!s.ctorInit) continue;
        if (strcmp(s.symbol, "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA") == 0) {
            ++g_constructed;              // MintTypeInfos() already built it
        } else if (strcmp(s.symbol, "?_loader@LoaderLoop@Engine@@1VLoader@2@A") == 0) {
            if (ConstructLoader(s)) ++g_constructed;
        } else {
            // Unreachable via the generator, which refuses to emit an
            // unhandled ctorInit. Kept because "unreachable" is exactly the
            // claim this project has learned not to trust.
            fprintf(stderr, "[RtMint] FATAL: %s is CONSTRUCTED by the DLL at %s and "
                            "nothing here reproduces it; zeroed storage would "
                            "silently differ from the loaded DLL.\n",
                    s.symbol, s.ctorInit);
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
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        memset(s.storage, 0, s.size);
        // ?_firstTypeInfo@ is the head of the reflection chain. In the DLL it
        // is filled by static initialisers; ours is filled by MintTypeInfos.
        if (strcmp(s.symbol, "?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA") == 0)
            *(RtObject::BaseTypeInfo**)s.storage = g_mintTypeInfoHead;
    }
}

// Registration. OFF unless BANISHED_RT_MINT=1, and it must stay off until
// cutover: while Runtime-x64.dll is loaded, the DLL's own code reads the DLL's
// own singletons, so handing OUR callers a second, empty set of singletons
// splits the process's state in half. Wired now, with the DLL present, so the
// last commit of the wave is a flag flip and not new code.
static void RegisterMintedData() {
    for (int i = 0; i < kVtableCount; ++i)
        RegisterRuntimeExport(kMintVtables[i].symbol, (void*)kMintVtables[i].table);
    for (int i = 0; i < kTypeInfoCount; ++i)
        RegisterRuntimeExport(kMintTypeInfos[i].symbol, (void*)kMintTypeInfos[i].storage);
    for (int i = 0; i < kStorageCount; ++i)
        RegisterRuntimeExport(kMintStorage[i].symbol, kMintStorage[i].storage);
}

void RtDataMintInit() {
    if (g_minted) return;
    g_minted = true;

    MintVtables();
    MintTypeInfos();
    MintStorage();
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

    if (EnvOn("BANISHED_RT_MINT")) RegisterMintedData();
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

    int ground = 0, groundBad = 0;      // harvest vs the DLL's live table
    int ours = 0, oursBad = 0;          // our slot vs our registered port
    int traps = 0, sizeSuspect = 0, absent = 0;

    for (int vi = 0; vi < kVtableCount; ++vi) {
        const RtMintVtable& v = kMintVtables[vi];
        void** dll = (void**)DllExport(v.symbol);
        if (!dll) {
            ++absent;
            printf("[RtMintShadow] MISSING: %s is not in the DLL's export directory\n",
                   v.symbol);
            continue;
        }
        // The DLL's exported table must sit where the harvest said it did. If
        // it does not, every slot VA below is being read from the wrong place
        // and the rest of this vtable's result is meaningless.
        if ((void*)dll != DllAddr(v.dllVa)) {
            ++groundBad;
            printf("[RtMintShadow] MISMATCH %s: exported at %p, harvest says %p\n",
                   v.symbol, (void*)dll, DllAddr(v.dllVa));
            continue;
        }
        for (int i = 0; i < v.slotCount; ++i) {
            const RtMintVSlot& s = v.slots[i];
            ++ground;
            void* expect = DllAddr(s.dllVa);
            if (dll[i] != expect) {
                ++groundBad;
                printf("[RtMintShadow] MISMATCH %s slot %d: DLL holds %p, harvest "
                       "recorded %p (%s)\n", v.symbol, i, dll[i], expect,
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
                       "says %p%s\n", v.symbol, i, mine, port,
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
                       v.symbol, adj);
            } else if (end > nextVt) {
                ++sizeSuspect;
                printf("[RtMintShadow] SIZE SUSPECT %s: minted %d slots, which runs %lld "
                       "qwords PAST %s - the table is too long\n",
                       v.symbol, v.slotCount,
                       (long long)(((char*)end - (char*)nextVt) / 8), adj);
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
        if (mine->typeId != dll->typeId) {
            ++tiIdInfo;
            printf("[RtMintShadow] INFO %s.typeId: minted 0x%08x (ctor sentinel), "
                   "DLL 0x%08x — expected once Factory::Startup has run\n",
                   t.symbol, mine->typeId, dll->typeId);
        }
        // next: NOT compared. See the header comment.
    }

    // ---- storage ----------------------------------------------------------
    int live = 0, dead = 0, ourNonZero = 0, shapeChecked = 0, shapeBad = 0;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        void** dll = (void**)DllExport(s.symbol);
        if (!dll) { ++absent; printf("[RtMintShadow] MISSING: %s is not exported\n", s.symbol); continue; }
        if ((void*)dll != DllAddr(s.dllVa)) {
            printf("[RtMintShadow] MISMATCH %s: exported at %p, harvest says %p\n",
                   s.symbol, (void*)dll, DllAddr(s.dllVa));
        }
        bool dllLive = false;
        for (unsigned b = 0; b < s.size; ++b)
            if (((unsigned char*)dll)[b]) { dllLive = true; break; }
        bool mineLive = false;
        for (unsigned b = 0; b < s.size; ++b)
            if (((unsigned char*)s.storage)[b]) { mineLive = true; break; }
        if (dllLive) ++live; else ++dead;
        if (mineLive) ++ourNonZero;
        printf("[RtMintShadow] storage %-52s %2u bytes  DLL=%s  minted=%s  ctor=%s  (%s)\n",
               s.symbol, s.size, dllLive ? "WRITTEN" : "still zero",
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
                    printf("[RtMintShadow]   SHAPE %s +0x%02x: DLL %s, minted %s\n",
                           s.symbol, k * 8, a ? "set" : "null", b ? "set" : "null");
                }
            }
            ++shapeChecked;
            if (differ) ++shapeBad;
        }
    }

    int ctorOwed = 0;
    for (int i = 0; i < kStorageCount; ++i)
        if (kMintStorage[i].ctorInit) ++ctorOwed;

    // ---- the summary, with its effectiveness control ----------------------
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
           "%d typeId INFO (next: NOT compared, built from our init order)\n",
           tiFields, tiBad, tiIdInfo);
    printf("[RtMintShadow] storage            : %d symbols, %d written by the DLL, "
           "%d still zero, %d of ours written (contents NOT compared: BSS)\n",
           kStorageCount, live, dead, ourNonZero);
    printf("[RtMintShadow] constructed statics: %d of %d built (shape compared on %d, "
           "%d SHAPE MISMATCH)   [BANISHED_RT_MINT_CTOR=1 to build them]\n",
           g_constructed, ctorOwed, shapeChecked, shapeBad);
    printf("[RtMintShadow] symbols absent from the DLL: %d\n", absent);

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
