// ============================================================================
// runtime/src/ui/factory_ui1.cpp — WAVE 28 LANE FACTUI1
//
//   The `?CreateNewObject@` / `?CreateInPlaceObject@` factory PAIR for the 31
//   UI classes of the absent-factory roster that sort strictly before
//   `ImageDescription`, plus the 14 out-of-line constructors those factories
//   call.
// ============================================================================
//
// WHY THIS FILE EXISTS
// --------------------
// Cutover attempt 30 died on `?CreateNewObject@SpriteSheet@UI@@` — the mint's
// own per-slot FATAL. That is one member of a 206-slot family: 104 minted
// `?_typeInfo@...@` rows, each carrying `factoryFn` (+0x20) and
// `inPlaceFactoryFn` (+0x28). At one name per cutover boot, enumerating the
// family costs ~200 attempts. WAVE28_KICKOFF.md §2 puts three lanes on it;
// this is the first of the three, and `stringtable_factory.cpp` (Wave 26 lane
// FACTORY) is the pattern it copies, including the DEFAULT-ON kill switch.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE DOES *NOT* CLAIM  (WAVE28_KICKOFF.md §2.2 caveat 3)
// ---------------------------------------------------------------------------
// EVERY ROW REGISTERED HERE WILL REPORT ATTENDANCE 0 IN AN ORDINARY RUN, AND
// THAT IS EXPECTED, NOT A DEFECT. Wave 11 lane OBJINT MEASURED that the DLL
// reaches its own factories through an in-module function pointer stamped by a
// static initialiser, and an in-module pointer is not a door
// (reference_a_ported_row_can_be_permanently_unpromotable.md). The value of
// these ports is ENTIRELY AT CUTOVER. Thirty green attendance zeros MEASURE
// NOTHING, and this file's report says so in those words.
//
// ---------------------------------------------------------------------------
// WHERE THE BYTES CAME FROM
// ---------------------------------------------------------------------------
// `objdump -d` and `pefile` against `C:\GOG Games\Banished\Runtime-x64.dll` on
// disk (WAVE28_KICKOFF.md standing warning 2 — Ghidra was used for NOTHING in
// this file, not even a name: every symbol below was read out of the export
// directory by RVA). Every store row carries the retail instruction address it
// was transcribed from, so any line can be checked with one objdump.
//
// The transcription was mechanical: a scratchpad abstract interpreter walked
// each body instruction by instruction and ASSERTED that it could classify
// every single one, so no instruction was silently dropped — the failure mode
// of warning 3 (count the STORES, not the fields). Its output is the
// GENERATED SECTION below. Three facts it established, each an assertion that
// had to hold for the generator to emit anything at all:
//
//   * for the 13 classes whose ctor is INLINED into the factory, the store
//     list in `CreateNewObject` and the one in `CreateInPlaceObject` are
//     IDENTICAL, offset for offset, width for width, value for value;
//   * for the 14 classes whose ctor is OUT OF LINE, both factories call that
//     one ctor and perform NO stores of their own;
//   * four classes are ABSTRACT — both factories are literally
//     `xor eax,eax; ret`. Those four bytes were read individually
//     (0x18003b6e0, 0x18003b6f0, 0x18003b480, 0x18003b490, 0x180035450,
//     0x180035100, 0x180035110), not inferred from the pattern.
//
// STORES ARE REPRODUCED AT THEIR EXACT WIDTH AND OFFSET, VIA memcpy, IN
// RETAIL'S ORDER. That is not pedantry: several of these ctors perform
// UNALIGNED and OVERLAPPING stores that no struct declaration can express —
// `??0AnimatedImageElement@UI@@` writes 8 bytes of 0xFFFFFFFFFFFFFFFF at
// +0xd4, `??0ClipRegionElement@UI@@`'s factory writes 8 bytes of 0x64 at +0xc4
// and then 8 bytes of 0 at +0xcc. A field-by-field port with a header would
// have had to guess at those, and a byte oracle comparing only the aligned
// fields would not have noticed. The repeated `+0x00` vtable stores (Base,
// then External/Internal, then each intermediate class, then the leaf) are
// DEAD STORES and are KEPT, for the reason `wave24_lane_gfxmat.cpp` gives: a
// port that writes only the final one is a different function under any
// interruption.
//
// NO TYPE HEADER IS ADDED. The project convention is a binary-compatible
// struct with a static_assert per field (CLAUDE.md, Reimplementation Workflow
// step 2), and it is the right convention for a body that READS fields. These
// bodies read nothing; they write a fixed byte pattern whose only authority is
// the disassembly. Thirty-one invented headers with invented field names would
// have added thirty-one opportunities to mistype an offset and NO check the
// store table does not already carry — the store table IS the layout
// assertion, and it cites its source address per row.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN  (standing warning 8)
// ---------------------------------------------------------------------------
// Greps scoped to the source roots, by NAME and by ADDRESS:
//
//   grep -rn '??0AnimatedImage...|??0BorderDescription@UI|...|??0GraphElement@UI'
//        runtime/ code/ standalone/ engine/ dx11/    -> NOTHING
//   grep -rn 'CreateNewObject@<class>@UI' <same roots>
//        -> only standalone/rt_data_mint_tables.inc (the minted rows themselves)
//
// TWO hits that DID come back, and both changed the plan:
//
//   * `??0Element@UI@@QEAA@XZ` (0x180115e50) IS ALREADY PORTED —
//     `RtUi_Element_Ctor`, runtime/src/ui/element.cpp:1895, Wave 7 lane A.
//     Fourteen of the thirty-one ctors here call it. This file therefore does
//     NOT re-register it and does NOT re-implement it; it calls it through the
//     port-aware door by name, which reaches our port today and at cutover.
//   * `??0ElementDescription@UI@@QEAA@XZ` is already ported (dialog.cpp:2664).
//     Harmless here — `ElementDescription` turns out to be ABSTRACT, so no
//     factory in this file constructs one.
//
// The remaining fourteen ctor names are absent from the tree in both
// directions and are registered here.
//
// ---------------------------------------------------------------------------
// THE ALLOCATION
// ---------------------------------------------------------------------------
// Each `CreateNewObject` inlines the lock/Allocate/unlock triple:
//
//     rbx = [?_singleton@MemoryAllocator@System@@] + 0x640
//     EnterCriticalSection(rbx)                      ; *0x18014b390
//     Allocate([?_singleton@...], <size>, 8)         ; 0x1800c6e60
//     LeaveCriticalSection(rbx)                      ; *0x18014b388
//
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) is the
// out-of-line wrapper for exactly that triple. THAT WAS CHECKED INSTRUCTION BY
// INSTRUCTION against 0x1800c7a50's own disassembly rather than taken from
// `stringtable_factory.cpp`'s claim by resemblance
// (reference_a_written_recipe_can_be_wrong.md): same singleton, same +0x640
// critical section, same two import slots, same order. Calling it reproduces
// the call site exactly, including lock order.
//
// The allocation SIZE is read off the ALLOCATION (`mov $<size>,%edx`), not
// inferred from the stores. Two spellings appear for the alignment argument:
// `mov $0x8,%r8d`, and `lea -0x<size-8>(%rdx),%r8d` which derives 8 from the
// size already in edx. Both are decoded; every one of the 27 concrete classes
// comes out align 8.
//
// Retail's null checks are RETAIL'S — the allocation-failure branch in
// `CreateNewObject` and the caller-supplied-null branch in
// `CreateInPlaceObject`, each returning 0 in EAX. They are reproduced, not
// added (CLAUDE.md, "Don't 'fix' a faithful crash with a null check").
//
// ---------------------------------------------------------------------------
// REGISTRATION
// ---------------------------------------------------------------------------
// The registration must land BEFORE `RtDataMintInit()`, which is what makes it
// clear a trap. `RtUiPortInit()` is called at standalone/resolve_standalone.cpp
// :333 and `RtDataMintInit()` at :406 — READ, in this worktree, not assumed
// (caveat 2). This file is reached from `RtUiPortInit()`.
//
// BOTH DOORS are registered for every row, name and address: the mint tries the
// NAME first and the ADDRESS second, a name-only registration leaves any
// by-address consumer on the DLL, and an address-only one is invisible to the
// census (reference_by_address_door_dies_at_cutover.md).
//
// Every name registered here was verified PRESENT in the export directory by
// RVA with pefile before it was written down (standing warning 5:
// `ResolveExport` returns NULL silently for a name the DLL lacks and the
// `ResolveAddr` fallback hides it). The generator that emitted the table below
// looked each one up in the parsed export map and would have thrown a
// KeyError rather than emit a phantom.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "../../funcs/rt_port.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// The store IR
// ---------------------------------------------------------------------------
enum StoreKind : uint8_t { SK_IMM = 0, SK_VTABLE = 1 };

struct Store {
    uint16_t    off;      // byte offset from `this`
    uint8_t     width;    // 1, 2, 4 or 8 — the instruction's operand size
    uint8_t     kind;
    uint64_t    imm;      // SK_IMM
    const char* sym;      // SK_VTABLE: the "??_7...@6B@" export name
    uint64_t    srcVa;    // the retail instruction this row was read from
};

struct Spec {
    const char*  cls;
    uint64_t     size;        // 0 == ABSTRACT: both factories return nullptr
    int          align;
    const char*  baseCtor;    // mangled name, or nullptr
    const Store* stores;
    uint16_t     nStores;
    const char*  newName;   uint64_t newVa;
    const char*  ipName;    uint64_t ipVa;    // nullptr: slot not owned here
    const char*  ctorName;  uint64_t ctorVa;  // out-of-line ctor, or nullptr
};

#include "factory_ui1_tables.inc"

constexpr int kSpecCount = (int)(sizeof(kSpecs) / sizeof(kSpecs[0]));

// ---------------------------------------------------------------------------
// ATTENDANCE — entries, and NOT a verification of anything.
// ---------------------------------------------------------------------------
// A zero here means the row is registered and DEAD, which for this family is
// the EXPECTED reading while the DLL is present (see the banner). These
// counters exist so that a LATER cutover run — where the DLL is absent and the
// minted BaseTypeInfo IS the only node — can say whether the body was entered,
// and so that a zero cannot be quietly read as "verified".
uint64_t g_new[kSpecCount]      = {};
uint64_t g_inplace[kSpecCount]  = {};
uint64_t g_ctor[kSpecCount]     = {};
uint64_t g_newNull[kSpecCount]  = {};   // the allocator refused
uint64_t g_ipNull[kSpecCount]   = {};   // retail's own null branch was taken

// ---- `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` 0x1800c7a50 -----------
// BY ADDRESS on purpose, and the address is the LOCKED wrapper, not
// `?Allocate@` (0x1800c6e60) which retail calls with the lock already held.
// `ResolveAddr(Module::Runtime, ...)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this follows the
// tree — including the allocator port — without this
// file changing. Re-resolve on a miss rather than freezing a null.
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}

// A `??_7...@6B@` vtable pointer, through the port-aware caching door so that a
// PROMOTED mint row is picked up rather than the DLL's table. Never caches a
// null (RT_DATA_SLOT's contract); one cache per slot, keyed by the Store row.
void* VTableFor(const Store& s) {
    // A tiny pointer-keyed cache. Every Store row's `sym` is a string literal
    // with a stable address, so pointer identity is exact and needs no strcmp;
    // the whole family uses well under 64 distinct vtable names, so a linear
    // scan is both trivially fast and trivially auditable. A NULL answer is
    // NEVER cached (RT_DATA_SLOT's contract, and for a mint row it matters:
    // the symbol may be supplied later).
    struct Entry { const char* key; void* val; };
    static Entry cache[64] = {};
    static int   used = 0;
    for (int i = 0; i < used; ++i)
        if (cache[i].key == s.sym) return cache[i].val;
    void* v = ResolveExport(s.sym);
    if (v && used < 64) { cache[used].key = s.sym; cache[used].val = v; ++used; }
    return v;
}

// Apply retail's store list at retail's widths and offsets, in retail's order.
void ApplyStores(void* self, const Spec& sp) {
    unsigned char* p = static_cast<unsigned char*>(self);
    for (uint16_t i = 0; i < sp.nStores; ++i) {
        const Store& s = sp.stores[i];
        uint64_t v = (s.kind == SK_VTABLE)
                       ? reinterpret_cast<uint64_t>(VTableFor(s))
                       : s.imm;
        std::memcpy(p + s.off, &v, s.width);   // little-endian, exact width
    }
}

using CtorFn = void* (*)(void*);

// The base-class constructor retail CALLS rather than inlines. Resolved by name
// through the PORT-AWARE door: `??0Element@UI@@QEAA@XZ` and
// `??0Internal@Object@@QEAA@XZ` are already ported, and
// `??0DialogInstance@UI@@QEAA@XZ` is ported by THIS file, so at cutover all
// three reach our own bodies. A miss is named and retried, never frozen.
CtorFn BaseCtor(const Spec& sp) {
    if (!sp.baseCtor) return nullptr;
    return reinterpret_cast<CtorFn>(ResolveExport(sp.baseCtor));
}

// The constructor body: base ctor (if retail calls one), then the stores.
void* RunCtor(int i, void* self) {
    const Spec& sp = kSpecs[i];
    ++g_ctor[i];
    if (CtorFn base = BaseCtor(sp)) base(self);
    ApplyStores(self, sp);
    return self;                    // MSVC ctors return `this` in RAX
}

void* DoNew(int i) {
    const Spec& sp = kSpecs[i];
    ++g_new[i];
    if (sp.size == 0) return nullptr;                 // ABSTRACT: xor eax,eax
    void* p = MemAllocLocked(sp.size, sp.align);
    if (p == nullptr) { ++g_newNull[i]; return nullptr; }   // retail's branch
    return RunCtor(i, p);
}

void* DoInPlace(int i, void* p) {
    const Spec& sp = kSpecs[i];
    ++g_inplace[i];
    if (sp.size == 0) return nullptr;                 // ABSTRACT: xor eax,eax
    if (p == nullptr) { ++g_ipNull[i]; return nullptr; }    // retail's branch
    return RunCtor(i, p);
}

// One distinct function pointer per class per slot, which is what a registry
// door needs. All three have the retail raw ABI: `CreateNewObject` is a static
// with no arguments, `CreateInPlaceObject` a static taking one pointer, the
// ctor a `QEAA` thiscall whose `this` arrives in RCX and is returned in RAX —
// all three are plain free functions under the single Win64 convention.
template <int I> void* ThunkNew()            { return DoNew(I); }
template <int I> void* ThunkInPlace(void* p) { return DoInPlace(I, p); }
template <int I> void* ThunkCtor(void* p)    { return RunCtor(I, p); }

int RegisterOne(int i, void* fnNew, void* fnIp, void* fnCtor) {
    const Spec& sp = kSpecs[i];
    int n = 0;
    RegisterRuntimeExport(sp.newName, fnNew);
    RegisterRuntimeAddr((uintptr_t)sp.newVa, fnNew);                    ++n;
    if (sp.ipName) {
        RegisterRuntimeExport(sp.ipName, fnIp);
        RegisterRuntimeAddr((uintptr_t)sp.ipVa, fnIp);                  ++n;
    }
    if (sp.ctorName) {
        RegisterRuntimeExport(sp.ctorName, fnCtor);
        RegisterRuntimeAddr((uintptr_t)sp.ctorVa, fnCtor);              ++n;
    }
    return n;
}

template <std::size_t... I>
int RegisterAll(std::index_sequence<I...>) {
    int n = 0;
    ((n += RegisterOne((int)I, (void*)&ThunkNew<(int)I>,
                       (void*)&ThunkInPlace<(int)I>,
                       (void*)&ThunkCtor<(int)I>)), ...);
    return n;
}

void FactoryUi1Report() {
    uint64_t tn = 0, ti = 0, tc = 0;
    for (int i = 0; i < kSpecCount; ++i) { tn += g_new[i]; ti += g_inplace[i]; tc += g_ctor[i]; }
    printf("[RtPort] UI FACTUI1 ATTENDANCE: CreateNewObject=%llu "
           "CreateInPlaceObject=%llu ctor=%llu\n",
           (unsigned long long)tn, (unsigned long long)ti,
           (unsigned long long)tc);
    fflush(stdout);
}
struct Reporter { ~Reporter() { FactoryUi1Report(); } };
Reporter g_reporter;

}  // namespace

// ============================================================================
// Registration entry point — called unconditionally from RtUiPortInit(), which
// runs BEFORE RtDataMintInit().
// ============================================================================
int RtUiFactory1Register() {
    int n = RegisterAll(std::make_index_sequence<kSpecCount>{});

    printf("[RtPort] UI FACTUI1: %d rows registered across %d classes "
           "(attendance will be 0 with the DLL present -- see the file banner)\n",
           n, kSpecCount);
    return n;
}
