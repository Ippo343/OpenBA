// ============================================================================
// runtime/src/ui/stringtable_factory.cpp — WAVE 26 LANE FACTORY
//
//   ??0StringTable@UI@@QEAA@XZ                                 0x18011d890 ord 421
//   ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z
//                                                              0x180002330 ord 1979
//   ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ      0x180002280 ord 2116
// ============================================================================
//
// WHY THIS FILE EXISTS. Cutover attempt 26 (docs/runtime_port/WAVE25_ATTEMPT26.md)
// did not crash — it failed fast on the mint's own per-slot trap, naming itself:
//
//   [RtMint] FATAL: minted BaseTypeInfo::factoryFn is not ported
//     vtable : ?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A
//     slot   : 0
//     target : ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
//
// `standalone/rt_data_mint.cpp:458` fills `BaseTypeInfo::factoryFn` from
// `RtPortLookupExport(t.factoryName)`, falling back to
// `RtPortLookupAddr(t.factoryVa)`, and installs `RtMintTiTrapStub_92_0` when
// BOTH miss. Nothing in the tree satisfied either door for this row.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// reference_grep_for_the_port_before_writing_it.md and
// reference_half_registered_port_reads_as_absent.md. Greps scoped to the source
// roots (NOT the repo root — .claude/worktrees/ holds sibling lane checkouts):
//
//   grep -rn 'StringTable@UI@@SAPEAVBase' runtime/ code/ standalone/ engine/ dx11/
//       -> only standalone/rt_data_mint_tables.inc (the minted row itself)
//   grep -rn '18011d890\|180002280\|180002330' <same roots>   -> NOTHING
//
// So: absent, in both the NAME and the ADDRESS direction.
//
// ---------------------------------------------------------------------------
// THE BODIES, READ OFF THE DISASSEMBLY (not the decompiler — Ghidra's C drops
// dereferences and whole arguments; reference_ghidra_c_output_vs_disassembly.md)
// ---------------------------------------------------------------------------
//
// 0x180002280 ?CreateNewObject@StringTable@UI@@:
//     MOV RBX,[0x1801fc718] ; ADD RBX,0x640     ; allocator singleton + lock
//     MOV RCX,RBX ; CALL [0x18014b390]          ; lock acquire
//     MOV EDX,0x30 ; LEA R8D,[RDX-0x28]         ; size 0x30, align 8
//     MOV RCX,[0x1801fc718] ; CALL 0x1800c6e60  ; ?Allocate@
//     MOV RDI,RAX ; MOV RCX,RBX ; CALL [0x18014b388]   ; lock release
//     TEST RDI,RDI ; JZ -> return 0
//     <the ctor at 0x18011d890, INLINED verbatim>
//     MOV RAX,RDI
//
// That lock / Allocate / unlock triple is instruction-for-instruction the same
// prologue `?CreateNewObject@Dialog@UI@@` (0x180038530) carries, which
// runtime/src/ui/dialog_factory.cpp already transcribes as a call to the
// out-of-line locked wrapper `?MemAlloc@System@@YAPEAX_KH@Z` (0x1800c7a50).
// The two prologues were compared side by side rather than assumed to match
// from resemblance (reference_a_written_recipe_can_be_wrong.md): the ONLY
// differing bytes are the size immediate (0x30 vs 0x88) and the LEA
// displacement that computes align 8 from it (-0x28 vs -0x80). The `LEA
// R8D,[RDX-0x28]` is a code-size trick for `MOV R8D,8`; it is written below as
// the literal 8, because that is the value, and the derivation would imply a
// relationship between size and alignment that does not exist.
//
// 0x180002330 ?CreateInPlaceObject@StringTable@UI@@:
//     TEST RCX,RCX ; JZ -> return 0      <- RETAIL'S null check, not a guard
//     <the same inlined ctor>
//     MOV RAX,RCX
//
// 0x18011d890 ??0StringTable@UI@@: the out-of-line ctor. Its eight stores are
// byte-for-byte the sequence both factories inline; the three copies were
// diffed against each other instruction by instruction, and they agree.
//
// TRANSCRIPTION CHOICE, stated because it is a choice. Both factories call ONE
// local `ConstructBody` rather than each re-transcribing the stores, and
// `RtUiStringTable_Ctor` is that same body exported under the ctor's own name.
// Retail has three copies because MSVC inlined one; keeping three copies here
// would create three things to keep in sync and would not be more faithful,
// only longer. This is dialog_factory.cpp's move in the same direction.
//
// THE TWO DEAD VPTR STORES ARE KEPT. `[this] = ??_7Base@`, then
// `[this] = ??_7External@`, then later `[this] = ??_7StringTable@` — the first
// two are the base constructors MSVC inlined and are unread in the byte-result
// sense. A port that writes only the final one is a DIFFERENT FUNCTION under
// any interruption, and a byte oracle would not notice — precisely when a
// "harmless" tidy-up survives review. Same reasoning, same words, as
// runtime/src/graphics/wave24_lane_gfxmat.cpp's ConstructBody.
//
// THE `MOV [RSP+0x28],RAX` / `MOV [RSP+0x40],RBX` STORES ARE NOT STATE. They
// are the MSVC exception-unwind frame's live-object records (`MOV [RSP],-2` at
// the top is the state index). Dead in a build with no unwind tables, and
// deliberately not reproduced — reproducing them would write locals nothing
// reads and imply state that is not there.
//
// ---------------------------------------------------------------------------
// THE THREE VTABLES ARE ALL EXPORTED BY NAME — none of them is an address door
// ---------------------------------------------------------------------------
// Checked against the export directory with pefile, by RVA, not asserted from
// a comment (reference_aeaa_is_private_not_unexported.md):
//   0x180154788  ??_7Base@Object@@6B@         ord 1389
//   0x1801547a0  ??_7External@Object@@6B@     ord 1432
//   0x180154630  ??_7StringTable@UI@@6B@      ord 1507
// Resolved through RT_DATA_SLOT, i.e. through the PORT REGISTRY, so if a later
// wave promotes `??_7StringTable@UI@@6B@` our stamp follows the promotion
// without this file changing.
//
// ---------------------------------------------------------------------------
// ONE KILL SWITCH: BANISHED_RT_UISTRTAB (default ON)
// ---------------------------------------------------------------------------
// DEFAULT-ON, unlike dialog_factory.cpp's opt-in BANISHED_RT_DLGFACTORY. That
// file is a Wave 11 EXPERIMENT about whether registering a factory redirects
// the DLL's creation path (its answer: no — the DLL reaches its factories
// through an in-module function pointer, which is not a door). This file is not
// that experiment. Its job is to make the minted row's slot 0 hold a real body
// at RtDataMintInit() time, and a registration that does not happen cannot do
// that. `RtUiPortInit()` (standalone/resolve_standalone.cpp:333) runs BEFORE
// `RtDataMintInit()` (:406), which is what makes the ordering work.
//
// WHAT THIS DOES *NOT* CLAIM. Registering these three names does not put our
// bodies on the DLL's own creation path while the DLL is present — Wave 11 lane
// OBJINT measured that it does not, for exactly this class of function. The
// claim is narrower and is the one the wall asked for: the minted
// `?_typeInfo@StringTable@UI@@` row's factoryFn stops being an abort trap.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/ui/StringTable.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE — entries, and NOT a verification of anything.
// ---------------------------------------------------------------------------
// A green readiness screen is not a route
// (reference_a_green_readiness_screen_is_not_a_route.md). A zero here means the
// row is registered and DEAD, which for this family is the EXPECTED reading
// while the DLL is present: Wave 11 lane OBJINT measured that the DLL reaches
// its own `CreateNewObject` through an in-module function pointer stamped by a
// static initialiser, and that is not a door
// (reference_a_ported_row_can_be_permanently_unpromotable.md). These counters
// exist so that a LATER cutover run — where the DLL is absent and the minted
// BaseTypeInfo IS the only node — can say whether the body was entered, and so
// that a zero cannot be quietly read as "verified".
uint64_t g_ctorEntries    = 0;
uint64_t g_newEntries     = 0;
uint64_t g_inPlaceEntries = 0;
uint64_t g_newNull        = 0;   // the allocator refused
uint64_t g_inPlaceNull    = 0;   // retail's own null branch was taken
uint64_t g_controlFired   = 0;   // effectiveness control, BANISHED_RT_UISTRTAB=2

// ---- `?MemAlloc@System@@YAPEAX_KH@Z` 0x1800c7a50 ---------------------------
// BY ADDRESS on purpose, and the address is the LOCKED wrapper, not `?Allocate@`
// (0x1800c6e60) which retail calls with the lock already held. Retail inlines
// the whole lock/Allocate/unlock triple at the call site; calling the
// out-of-line wrapper reproduces it exactly, including the lock order.
// `ResolveAddr(Module::Runtime, ...)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this follows the
// tree — including the allocator port behind BANISHED_RT_ALLOC — without this
// file changing. Re-resolve on a miss rather than freezing a null: a caller
// that ran before the allocator's arming path supplied this address would
// otherwise poison the static for the life of the process.
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}

// THE ALLOCATION SHAPE, read off 0x1800022b1-0x1800022c1 (see banner).
constexpr uint64_t kStringTableSize  = 0x30;
constexpr int      kStringTableAlign = 8;

void* BaseVTable()        { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()    { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* StringTableVTable() { return RT_DATA_SLOT(void*, "??_7StringTable@UI@@6B@"); }

// The eight stores, in retail's order. Shared by all three entry points for the
// reason the banner gives.
void ConstructBody(UiStringTable* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();          // 18011d8a1  dead store, kept
    *vp = ExternalVTable();      // 18011d8ab  dead store, kept
    self->external_08 = nullptr; // 18011d8c0
    self->external_10 = nullptr; // 18011d8c3
    self->external_18 = nullptr; // 18011d8c7
    *vp = StringTableVTable();   // 18011d8cb
    self->data     = nullptr;    // 18011d8d5
    self->size     = 0;          // 18011d8d9 — one 8-byte store covers BOTH
    self->capacity = 0;          //             _size and _capacity
}

void StringTableFactoryReport() {
    printf("[RtPort] UI::StringTable factory ATTENDANCE: ctor=%llu "
           "CreateNewObject=%llu (null=%llu) CreateInPlaceObject=%llu "
           "(null=%llu) control=%llu\n",
           (unsigned long long)g_ctorEntries,
           (unsigned long long)g_newEntries, (unsigned long long)g_newNull,
           (unsigned long long)g_inPlaceEntries, (unsigned long long)g_inPlaceNull,
           (unsigned long long)g_controlFired);
    if (g_ctorEntries == g_controlFired) {
        printf("[RtPort] UI::StringTable factory VERDICT: the ONLY entries were "
               "the control's own. Nothing in this run reached the port through "
               "a door -- EXPECTED with the DLL present (Wave 11 lane OBJINT), "
               "and NOT evidence the body is correct.\n");
    }
    fflush(stdout);
}
struct FactoryReport { ~FactoryReport() { if (g_ctorEntries) StringTableFactoryReport(); } };
FactoryReport g_factoryReport;

}  // namespace

// ============================================================================
// ??0StringTable@UI@@QEAA@XZ                                    0x18011d890
// ============================================================================
extern "C" void* RtUiStringTable_Ctor(void* self) {
    ++g_ctorEntries;
    ConstructBody(static_cast<UiStringTable*>(self));
    return self;                                  // 18011d8dd MOV RAX,RCX
}

// ============================================================================
// ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z 0x180002330
// ============================================================================
//
//     Base* StringTable::CreateInPlaceObject(void* p)
//     { if (!p) return nullptr; new (p) StringTable(); return p; }
//
// THE NULL CHECK IS RETAIL'S, not a guard added here: 0x180002341
// `TEST RCX,RCX / JZ 0x180002387`, and the null branch returns 0 in EAX.
extern "C" void* RtUiStringTable_CreateInPlaceObject(void* p) {
    ++g_inPlaceEntries;
    if (p == nullptr) { ++g_inPlaceNull; return nullptr; }   // 180002341 / 180002387
    RtUiStringTable_Ctor(p);
    return p;                                                // 180002382 MOV RAX,RCX
}

// ============================================================================
// ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ        0x180002280
// ============================================================================
//
//     Base* StringTable::CreateNewObject()
//     { void* p = System::MemAlloc(0x30, 8); if (!p) return nullptr;
//       new (p) StringTable(); return p; }
//
// The null check at 0x1800022da is retail's and covers an allocation failure.
extern "C" void* RtUiStringTable_CreateNewObject() {
    void* p = MemAllocLocked(kStringTableSize, kStringTableAlign);
    ++g_newEntries;
    if (p == nullptr) { ++g_newNull; return nullptr; }        // 1800022da / 18000231d
    RtUiStringTable_Ctor(p);
    return p;                                                // 180002318 MOV RAX,RDI
}

// ============================================================================
// Registration
// ============================================================================
int RtUiStringTableFactoryRegister() {
    const char* v = std::getenv("BANISHED_RT_UISTRTAB");
    if (v && v[0] == '0') return 0;

    int n = 0;
    // BOTH doors for each row. The mint tries the NAME first and the ADDRESS
    // second; a name-only registration would leave any by-address consumer on
    // the DLL, and an address-only one is invisible to the census
    // (reference_by_address_door_dies_at_cutover.md).
    RegisterRuntimeExport("??0StringTable@UI@@QEAA@XZ", (void*)&RtUiStringTable_Ctor);
    RegisterRuntimeAddr(0x18011d890, (void*)&RtUiStringTable_Ctor);                 ++n;

    RegisterRuntimeExport("?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiStringTable_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180002330, (void*)&RtUiStringTable_CreateInPlaceObject);  ++n;

    RegisterRuntimeExport("?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiStringTable_CreateNewObject);
    RegisterRuntimeAddr(0x180002280, (void*)&RtUiStringTable_CreateNewObject);      ++n;

    // ---- effectiveness control ---------------------------------------------
    // BANISHED_RT_UISTRTAB=2. IN-PLACE ONLY: it allocates nothing, so it cannot
    // perturb the arena of the run it is validating, and its subject is
    // entirely caller-owned. The buffer is 0x30 bytes, the ctor's exact written
    // extent, so an over-write would land in the redzone rather than silently.
    //
    // The object is a stack temporary that is never registered and never
    // destroyed, deliberately: `??1StringTable@UI@@` frees the entry array and
    // its refcounted strings, and on a freshly constructed object every one of
    // those is null; running it would exercise a teardown this control is not
    // measuring and would put a free on the arena the run is using.
    //
    // The control picks a subject the screen actually reads: the attendance
    // counter and the returned pointer (reference_control_must_pick_a_compared_
    // subject.md). It is NOT scored on the vptr, which is a DLL pointer here and
    // a promoted pointer later — a screen on that would report a divergence
    // that is expected, not a defect.
    if (v && v[0] == '2') {
        alignas(8) unsigned char scratch[kStringTableSize];
        const uint64_t before = g_inPlaceEntries;
        void* got = RtUiStringTable_CreateInPlaceObject(scratch);
        const UiStringTable* st = reinterpret_cast<const UiStringTable*>(scratch);
        const bool zeroed = st->external_08 == nullptr && st->external_10 == nullptr &&
                            st->external_18 == nullptr && st->data == nullptr &&
                            st->size == 0 && st->capacity == 0;
        g_controlFired = (g_inPlaceEntries > before && got == scratch && zeroed) ? 1u : 0u;
        printf("[RtPort] UI::StringTable CONTROL %s (attendance %llu -> %llu, "
               "returned %sthe buffer, tail %szeroed, vptr=%p)\n",
               g_controlFired ? "FIRED" : "*** DID NOT FIRE ***",
               (unsigned long long)before, (unsigned long long)g_inPlaceEntries,
               got == scratch ? "" : "NOT ", zeroed ? "" : "NOT ",
               (void*)st->vptr);
        if (!g_controlFired)
            printf("[RtPort] UI::StringTable *** the attendance zeros below prove "
                   "NOTHING either way.\n");
        fflush(stdout);
    }
    return n;
}
