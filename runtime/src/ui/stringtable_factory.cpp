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
// ORDERING
// ---------------------------------------------------------------------------
// This file's job is to make the minted row's slot 0 hold a real body at
// RtDataMintInit() time. `RtUiPortInit()` (standalone/resolve_standalone.cpp
// :333) runs BEFORE `RtDataMintInit()` (:406), which is what makes it work.
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
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/StringTable.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_StringPair10

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

// ---- `?MemAlloc@System@@YAPEAX_KH@Z` 0x1800c7a50 ---------------------------
// BY ADDRESS on purpose, and the address is the LOCKED wrapper, not `?Allocate@`
// (0x1800c6e60) which retail calls with the lock already held. Retail inlines
// the whole lock/Allocate/unlock triple at the call site; calling the
// out-of-line wrapper reproduces it exactly, including the lock order.
// `ResolveAddr(Module::Runtime, ...)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this follows the
// tree — including the allocator port — without this
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
//
// IT IS NOT A CALL TO A BASE CONSTRUCTOR: retail open-codes the chain as three
// vptr stamps plus the zero stores, and the stamps name the MINTED tables,
// which is exactly right for the objects that reach this body (they wear the
// minted table). A member constructor here would stamp GCC's tables into a
// minted object — which is why UI::StringTable has no real ctor (B6-3).
void ConstructBody(UI::StringTable* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();          // 18011d8a1  dead store, kept
    *vp = ExternalVTable();      // 18011d8ab  dead store, kept
    self->_path.data      = nullptr;   // 18011d8c0  Object::External +0x08
    self->_refs._data     = nullptr;   // 18011d8c3  Object::External +0x10
    self->_refs._size     = 0;         // 18011d8c7  +0x18, one qword with
    self->_refs._capacity = 0;         //            capacity
    *vp = StringTableVTable();   // 18011d8cb
    self->_data     = nullptr;   // 18011d8d5
    self->_size     = 0;         // 18011d8d9 — one 8-byte store covers BOTH
    self->_capacity = 0;         //             _size and _capacity
}

void StringTableFactoryReport() {
    printf("[RtPort] UI::StringTable factory ATTENDANCE: ctor=%llu "
           "CreateNewObject=%llu (null=%llu) CreateInPlaceObject=%llu "
           "(null=%llu)\n",
           (unsigned long long)g_ctorEntries,
           (unsigned long long)g_newEntries, (unsigned long long)g_newNull,
           (unsigned long long)g_inPlaceEntries, (unsigned long long)g_inPlaceNull);
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
    ConstructBody(reinterpret_cast<UI::StringTable*>(self));   // B.3.6 bridge
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

    return n;
}

// ============================================================================
// WAVE 26 LANE FACTORY, SECOND PASS — the three `?_typeInfo@`-reading slots
// ============================================================================
//
// WHY THESE ARE HERE. Lane ATTEND ran cutover attempt 28 with the port above
// merged. The `[RtMint] FATAL` on `?CreateNewObject@StringTable@UI@@` was GONE
// and the screen read `2 PORTED, 204 ABORT-TRAP` from inside a real cutover
// boot. The boot then died a few instructions further along THE SAME call path:
//
//   [RtUnresolved] ??_7StringTable@UI@@6B@
//   Fault: READ of 0x10 at RtObject::GetTypeIdVirtual+0x1f
//     <- RtObject_Base_IsDerivedFrom+0x4e <- RtObject_External_Load+0x33c
//     <- ExternalFactory::FindOrLoad+0x2b1 <- Game::LoadInputNameTables+0x177
//
// `READ of 0x10` is a vtable read at +0x10, i.e. SLOT 2, through a null vptr —
// our `CreateNewObject` had stamped the object with a vtable the mint did not
// carry. Slot 2 of the regenerated `??_7StringTable@UI@@6B@` row
// (standalone/rt_data_mint_tables.inc kVSlots52) is `?Type@StringTable@UI@@`,
// which is exactly the function `GetTypeIdVirtual` is calling. The diagnosis
// and the slot index agree, independently.
//
// THE VTABLE ROW NEEDED NO NEW HARVEST RULE. `rt_mint_gen.py`'s population rule
// is "mint what our SOURCE names", and `StringTableVTable()` above names
// `"??_7StringTable@UI@@6B@"` as a literal — so `--check` reported
// `ENTERED (1): 0x180154630 ??_7StringTable@UI@@6B@` and a regeneration picked
// it up. **A ported factory that stamps its class's vtable through
// RT_DATA_SLOT drags that vtable into the mint for free.** That is a
// general fact about this family, not a fact about StringTable, and it is the
// single most useful thing this lane learned for Wave 27's generator.
//
// THE BODIES. Identical in shape to the eight in
// runtime/src/graphics/graphics_typeinfo_accessors.cpp (Wave 21 lane GFXVT) and
// the seven in object_typeinfo_accessors.cpp (Wave 11 lane OBJDATA). Read as
// DISASSEMBLY; the node is `?_typeInfo@StringTable@UI@@` at 0x1801fae08:
//
//   ?Type@StringTable@UI@@UEBAIXZ           0x180002170
//       MOV EAX,dword ptr [0x1801fae08]  / RET      = node+0x00  (typeId, DWORD)
//   ?TypeName@StringTable@UI@@UEBAPEBDXZ    0x180002190
//       MOV RAX,qword ptr [0x1801fae10]  / RET      = node+0x08  (name, QWORD)
//   ?GetFileVersion@StringTable@UI@@UEBAHXZ 0x18011d930
//       MOV EAX,0x1 / RET                            = the literal 1
//
// NONE OF THE THREE READS `this`. All three are declared with a `self`
// parameter anyway because they are VIRTUAL slots and the caller passes one in
// RCX; dropping the parameter would be a different function at the ABI level.
// `self` is deliberately unnamed-but-unused rather than removed.
//
// GetFileVersion returning a literal 1 is not an accident of this class: it is
// the Object::External file-version convention, and the value belongs to the
// SERIALIZER, not to the object. It is transcribed, not derived.
// ============================================================================

namespace {
// BaseTypeInfo's first two fields, the only two these bodies touch. Read with
// memcpy rather than a struct cast so no alignment or strict-aliasing
// assumption is made about storage that may be OURS (minted) or the DLL's,
// depending on whether the `?_typeInfo@` row is promoted — the same reading
// graphics_typeinfo_accessors.cpp uses, for the same reason.
constexpr size_t kTiTypeIdOffset = 0x00;
constexpr size_t kTiNameOffset   = 0x08;
static_assert(kTiTypeIdOffset == 0, "BaseTypeInfo::typeId is at +0x00");
static_assert(kTiNameOffset == 8,   "BaseTypeInfo::name is at +0x08");
static_assert(sizeof(uint32_t) == 4, "typeId is a DWORD (MOV EAX, not MOV RAX)");
static_assert(sizeof(const char*) == 8, "name is a QWORD (MOV RAX)");

const uint8_t* StringTableTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A");
}

uint64_t g_typeEntries = 0, g_typeNameEntries = 0, g_fileVersionEntries = 0;
}  // namespace

// ENDGAME B.3.6 block D: the three bodies are UI::StringTable's members; the
// doors keep their attendance counters and delegate with a qualified call.
namespace UI {

// ---- ??_7StringTable@UI@@6B@ slot 2 ----------------------------------------
uint32_t StringTable::Type() const {
    const uint8_t* node = StringTableTypeInfo();
    uint32_t v = 0;
    if (node) std::memcpy(&v, node + kTiTypeIdOffset, sizeof v);
    return v;
}

// ---- ??_7StringTable@UI@@6B@ slot 1 ----------------------------------------
const char* StringTable::TypeName() const {
    const uint8_t* node = StringTableTypeInfo();
    const char* v = nullptr;
    if (node) std::memcpy(&v, node + kTiNameOffset, sizeof v);
    return v;
}

// ---- ??_7StringTable@UI@@6B@ slot 3 ----------------------------------------
int32_t StringTable::GetFileVersion() const {
    return 1;                                       // 18011d930 MOV EAX,1
}

}  // namespace UI

extern "C" uint32_t RtUiStringTable_Type(const void* self) {
    ++g_typeEntries;
    return reinterpret_cast<const UI::StringTable*>(self)->UI::StringTable::Type();  // B.3.6 bridge
}
extern "C" const char* RtUiStringTable_TypeName(const void* self) {
    ++g_typeNameEntries;
    return reinterpret_cast<const UI::StringTable*>(self)                       // B.3.6 bridge
               ->UI::StringTable::TypeName();
}
extern "C" int32_t RtUiStringTable_GetFileVersion(const void* self) {
    ++g_fileVersionEntries;
    return reinterpret_cast<const UI::StringTable*>(self)                       // B.3.6 bridge
               ->UI::StringTable::GetFileVersion();
}

// THE `if (node)` IN THE TWO ACCESSORS IS NOT A FAITHFULNESS BREAK, and it is
// the one place in this file where our standalone genuinely differs from retail
// (CLAUDE.md "Don't 'fix' a faithful crash with a null check"). Retail's body is
// a RIP-relative load from a FIXED address that always exists in its own image;
// ours goes through RT_DATA_SLOT, which can legitimately return null when the
// symbol is neither minted nor resolvable — a state retail has no equivalent
// of. Returning 0/nullptr there is what the surrounding code already handles;
// dereferencing null would report OUR resolve failure as a crash in a body that
// is byte-correct. Stated here rather than left implicit.

int RtUiStringTableAccessorsRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@StringTable@UI@@UEBAIXZ",
                          (void*)&RtUiStringTable_Type);
    RegisterRuntimeAddr(0x180002170ull, (void*)&RtUiStringTable_Type);            ++n;
    RegisterRuntimeExport("?TypeName@StringTable@UI@@UEBAPEBDXZ",
                          (void*)&RtUiStringTable_TypeName);
    RegisterRuntimeAddr(0x180002190ull, (void*)&RtUiStringTable_TypeName);        ++n;
    RegisterRuntimeExport("?GetFileVersion@StringTable@UI@@UEBAHXZ",
                          (void*)&RtUiStringTable_GetFileVersion);
    RegisterRuntimeAddr(0x18011d930ull, (void*)&RtUiStringTable_GetFileVersion);  ++n;
    return n;
}

// ============================================================================
// WAVE 26 LANE FACTORY, THIRD PASS — slots 6, 7, 8 of ??_7StringTable@UI@@6B@
// ============================================================================
//
// After the second pass the row read `slots=9 ported=4 TRAP=5` (measured, run E).
// The five were slot 0 (the deleting destructor),
// Serialize, Reconstruct, Destruct and Construct. Three of those five are
// transcriptions of things this file already contains, and they are here:
//
//   ?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ    0x180002400  slot 8
//   ?Destruct@StringTable@UI@@UEAAXXZ                         0x180002460  slot 7
//   ?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ  0x180002390  slot 6
//
// `?Construct@` IS `?CreateInPlaceObject@` — not "the same shape as", the same
// instructions: null check on RCX, the eight ctor stores, MOV RAX,RCX. The two
// were diffed instruction by instruction rather than assumed equal from their
// matching lengths.
//
// `?Destruct@` is three instructions and it is a TAIL CALL THROUGH THE OBJECT'S
// OWN VPTR, not a direct call to the destructor:
//     MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]
// i.e. `self->vtable[0](self, 0)` — slot 0 with flag 0, "destroy in place, do
// not free". It MUST be reproduced as a virtual dispatch. Calling
// `??1StringTable@UI@@` directly here would be the same thing only while slot 0
// is retail's, and would silently stop following a promotion.
//
// `?Reconstruct@` is `self->vtable[0](self, 0)` followed by the same eight ctor
// stores, returning self. It has NO null check — that is retail's choice, and
// the unguarded deref is kept (CLAUDE.md: retail's unguarded derefs are
// load-bearing).
//
// *** WAVE 27 LANE FRONTIER CLOSED THE LAST TWO. *** The paragraph that stood
// here said slot 0 (0x180002740) and `?Serialize@StringTable@UI@@` (0x18011d940)
// were NOT PORTED and the row was 7/9. Both are below, in the FOURTH PASS
// section at the bottom of this file. The row is now 9/9 on the STATIC screen.
//
// 9/9 IS NOT "PROMOTED". Promotion turns on owning the function that STAMPS the
// table, which this lane did NOT establish - see the residual in
// docs/runtime_port/WAVE27_LANE_FRONTIER.md, which names its addressee.
// ============================================================================

namespace {
// Vtable slot 0 of Object::Base is the scalar deleting destructor:
//   flag 0 = destroy in place, flag 1 = destroy and free.
// Same typedef, same comment, as runtime/src/graphics/wave24_lane_gfxmat.cpp.
using StSlot0Fn = void* (*)(void* self, int flag);

uint64_t g_constructEntries = 0, g_destructEntries = 0, g_reconstructEntries = 0;
}  // namespace

namespace UI {

// ---- slot 8 ----------------------------------------------------------------
// Retail's null test (180002411 / 180002457) is in the DOOR.
Object::External* StringTable::Construct() {
    ConstructBody(this);
    return this;                                    // 180002452 MOV RAX,RCX
}

// ---- slot 7 ----------------------------------------------------------------
// Slot 0 dispatched VIRTUALLY on purpose: the most-derived deleting dtor runs.
void StringTable::Destruct() {
    StSlot0Fn* vt = *reinterpret_cast<StSlot0Fn**>(this);   // 180002460 MOV RAX,[RCX]
    vt[0](this, 0);                                         // 180002465 JMP [RAX], EDX=0
}

// ---- slot 6 ----------------------------------------------------------------
Object::External* StringTable::Reconstruct() {
    StSlot0Fn* vt = *reinterpret_cast<StSlot0Fn**>(this);   // 1800023a2
    vt[0](this, 0);                                         // 1800023a7 CALL [RAX], EDX=0
    ConstructBody(this);
    return this;                                            // 1800023ea MOV RAX,RBX
}

}  // namespace UI

extern "C" void* RtUiStringTable_Construct(void* self) {
    ++g_constructEntries;
    if (self == nullptr) return nullptr;            // 180002411 / 180002457
    return reinterpret_cast<UI::StringTable*>(self)->UI::StringTable::Construct();  // B.3.6 bridge
}
extern "C" void RtUiStringTable_Destruct(void* self) {
    ++g_destructEntries;
    reinterpret_cast<UI::StringTable*>(self)->UI::StringTable::Destruct();          // B.3.6 bridge
}
extern "C" void* RtUiStringTable_Reconstruct(void* self) {
    ++g_reconstructEntries;
    return reinterpret_cast<UI::StringTable*>(self)->UI::StringTable::Reconstruct();  // B.3.6 bridge
}

int RtUiStringTableLifecycleRegister() {
    int n = 0;
    RegisterRuntimeExport("?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUiStringTable_Construct);
    RegisterRuntimeAddr(0x180002400ull, (void*)&RtUiStringTable_Construct);       ++n;
    RegisterRuntimeExport("?Destruct@StringTable@UI@@UEAAXXZ",
                          (void*)&RtUiStringTable_Destruct);
    RegisterRuntimeAddr(0x180002460ull, (void*)&RtUiStringTable_Destruct);        ++n;
    RegisterRuntimeExport("?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUiStringTable_Reconstruct);
    RegisterRuntimeAddr(0x180002390ull, (void*)&RtUiStringTable_Reconstruct);     ++n;
    return n;
}


// ============================================================================
// WAVE 27 LANE FRONTIER, FOURTH PASS - the last two ??_7StringTable@UI@@6B@
// slots: slot 0 (0x180002740) and slot 5 `?Serialize@` (0x18011d940)
// ============================================================================
//
// *** THE ADDRESS IN THE HANDOVER WAS RIGHT AND GHIDRA DISAGREED. ***
// WAVE26_CLOSED.md section 5 names slot 0 as 0x180002740. Ghidra's analysis of
// the RETAIL dll places that address MID-INSTRUCTION inside a neighbouring
// function, which reads exactly like a wrong address in a handover. It is not:
// the vtable's own bytes, read straight out of the PE at RVA 0x154630 with
// pefile, are
//     +0x00 0x180002740   +0x08 0x180002190   +0x10 0x180002170
//     +0x18 0x18011d930   +0x20 0x1800066e0   +0x28 0x18011d940
//     +0x30 0x180002390   +0x38 0x180002460   +0x40 0x180002400
// which is `kVSlots52` in standalone/rt_data_mint_tables.inc, entry for entry.
// The disassembly below was then taken with an EXPLICIT program= argument (see
// the INSTRUMENT DEFECT section of the lane doc - the MCP's notion of "current
// program" was the PROFILE dll while every listing tool reported retail).
//
// ---------------------------------------------------------------------------
// SLOT 0 - the scalar/vector deleting destructor, 0x180002740
// ---------------------------------------------------------------------------
// Transcribed from 0x180002740..0x1800027cd. flag is EDX:
//   bit 1 (0x2) = this is an ARRAY block; bit 0 (0x1) = also FREE the storage.
//
//     TEST DL,0x2 ; JZ scalar
//     ; --- vector arm ---
//     LEA R9,[0x18011d8f0]            ??1StringTable@UI@@, the per-element dtor
//     MOV R8D,[RCX-0x8]               the element COUNT, stored before the block
//     MOV EDX,0x30                    sizeof(UI::StringTable)
//     CALL 0x18007ec98                MSVC eh_vector_destructor_iterator
//     TEST SIL,0x1 ; JZ .
//     LEA RCX,[RBX-0x8] ; CALL 0x180006a10      free the block, INCLUDING the
//                                               8-byte count header
//     LEA RAX,[RBX-0x8] ; return
//     ; --- scalar arm ---
//     LEA RAX,[0x180154630] ; MOV [RCX],RAX     re-stamp OUR vptr
//     ADD RCX,0x20 ; CALL 0x180004a90           ~Array<TextEntry>
//     MOV RCX,RBX  ; CALL 0x1800bc950           ~External
//     TEST SIL,0x1 ; JZ .
//     MOV RCX,RBX  ; CALL 0x180002220           ??3StringTable@UI@@ (guarded free)
//     MOV RAX,RBX  ; return
//
// The scalar arm is `??1StringTable@UI@@UEAA@XZ` (0x18011d8f0) INLINED - the two
// were diffed instruction by instruction, not assumed equal: 0x18011d906..
// 0x18011d922 is the same LEA/MOV/ADD/CALL/MOV/JMP sequence, with retail's
// exported dtor tail-JMPing to ~External where slot 0 CALLs it and then falls
// through to the free. So the port below calls the exported dtor rather than
// re-transcribing its three steps, which is also what makes it follow a future
// promotion of `??1StringTable@UI@@` instead of freezing today's body.
//
// *** THE VECTOR ARM OPENS ONE DOOR AND IT IS NAMED HERE, IN THE SHAPE WAVE 26
// ASKED FOR (WAVE27_KICKOFF.md section 4 item 10). ***
//   SYMBOL:      0x18007ec98, MSVC's `eh vector destructor iterator`.
//   CONDITION:   nothing in this tree registers it today (grepped across
//                runtime/ code/ standalone/ engine/ dx11/: it appears only in
//                COMMENTS, in array_destroy.cpp and io/binary_writer.cpp).
//   CONSEQUENCE: at cutover, a call that reaches the vector arm is an EXEC-of-0
//                at this call site - the same failure attempt 29 took one level
//                up in external_load.cpp.
// It is left as an address door rather than hand-rolled because the iteration
// ORDER and the EH semantics are the whole content of that helper and this lane
// did not read it. The arm is believed unreachable in practice - UI::StringTable
// is created only through `?CreateNewObject@`, which is a SINGLE-object
// MemAlloc(0x30, 8) with no count header - but "believed unreachable" is why
// the counter below exists rather than an assertion.
// ADDRESSEE: the lane that ports MSVC's eh_vector helpers, or whichever cutover
// attempt first reports an EXEC-of-0 at RtUiStringTable_ScalarDelete.
//
// ---------------------------------------------------------------------------
// SLOT 5 - `?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z`, 0x18011d940
// ---------------------------------------------------------------------------
// The SAME `IO::Serializer` 16-byte facade `Object::External::Load` documents at
// length (runtime/src/object/external_load.cpp) - a {Stream*, impl} pair on the
// stack, handed to the Encoding's factory with only the stream filled in. That
// file's account was used to SHAPE this one and every offset below was re-read
// from 0x18011d940's own listing:
//
//     ser.stream = stream                       18011d957  [RSP+0x28]
//     ser.impl   = 0                            18011d95c  [RSP+0x30]
//     enc  = stream->encoding                   18011d965  [RDX+0x10]
//     slot = (stream->direction == 0) ? +0x18 : +0x20      18011d96f/97a/97f
//     ser.impl = enc->vtbl[slot](enc, &ser, 0)
//     count = self->size                        18011d987  [RBX+0x28]
//     ser.impl->vtbl[0xa0](impl, &count, 0)     18011d99e  the uint32 overload
//     if (stream->direction == 0)               18011d9a9
//         Array<TextEntry>::SetSize(&self->data, count)    18011d9b7 -> 0x180078c90
//     e = self->data                            18011d9bc
//     for (i = 0; i < count; ++i, e += 0x10) {
//         String::Serialize(e,       &ser)      18011d9d8 -> 0x1800c9930
//         String::Serialize(e + 0x8, &ser)      18011d9e6
//     }
//     if (ser.impl) ser.impl->vtbl[0](ser.impl, 1)         18011da06
//     return true                               18011da08  MOV AL,0x1
//
// TWO THINGS THAT ARE EASY TO GET WRONG AND WERE READ, NOT ASSUMED:
//   * the loop bound is the LOCAL `count` at [RSP+0x50] (`MOV EAX,[RSP+0x50]`
//     at 18011d9c0), NOT `self->size` re-read. On the read path SetSize has
//     just made them equal; on the WRITE path nothing changes either. It is
//     transcribed as the local because that is what retail reads.
//   * `e` is latched BEFORE the loop (`MOV RBX,[RBX+0x20]` at 18011d9bc), so a
//     reallocation inside the loop would not be seen. Retail's own choice.
//
// RETURN IS UNCONDITIONALLY TRUE. There is no failure path: the only branches
// are the direction test and the count loop. Not simplified - there is nothing
// to simplify; recorded because "a Serialize returning bool" invites a guess.
// ============================================================================

namespace {

// The count MSVC stores 8 bytes before an array-new block.
using EhVecDtorFn = void  (*)(void* base, uint64_t size, int count, void (*dtor)(void*));
using FreeFn      = void  (*)(void* p);
using DtorFn      = void  (*)(void* self);
using SerFactoryFn = void* (*)(void* encoding, void* ser, int flag);
using SerU32Fn     = void  (*)(void* impl, uint32_t* v, const char* name);
using DeleteFn     = void* (*)(void* self, int flag);
using ArraySetSizeFn = void (*)(void* arr, int n);
using StrSerFn       = void (*)(void* str, void* ser);

uint64_t g_slot0Scalar = 0, g_slot0Vector = 0, g_slot0Freed = 0;
uint64_t g_serEntries  = 0, g_serRead = 0, g_serElems = 0, g_serNullImpl = 0;

// `IO::Serializer` - the 16-byte facade. Identical to the struct
// external_load.cpp defines; duplicated rather than shared because that file
// exposes no header, which is how every sibling in runtime/src/ is wired.
struct SerializerPair {
    void* stream;   // +0x00
    void* impl;     // +0x08
};
static_assert(sizeof(SerializerPair) == 0x10, "the pair retail keeps on the stack");

// ATTENDANCE, and the same disclaimer the factory report above carries: a zero
// here means REGISTERED AND NOT REACHED, which is the expected reading with the
// DLL present, and is NOT evidence either body is correct. The vector counter is
// the one to watch: a non-zero g_slot0Vector on a cutover run means the door
// named in the banner (0x18007ec98) is live after all.
void StringTableSlotsReport() {
    printf("[RtPort] UI::StringTable slot ATTENDANCE: slot0 scalar=%llu "
           "vector=%llu freed=%llu | Serialize=%llu (read=%llu elems=%llu "
           "nullImpl=%llu)\n",
           (unsigned long long)g_slot0Scalar, (unsigned long long)g_slot0Vector,
           (unsigned long long)g_slot0Freed,  (unsigned long long)g_serEntries,
           (unsigned long long)g_serRead,     (unsigned long long)g_serElems,
           (unsigned long long)g_serNullImpl);
    if (g_serEntries != 0 && g_serElems == 0) {
        printf("[RtPort] UI::StringTable Serialize: entered %llu times and "
               "replayed ZERO elements -- the loop never ran, so `calls` here "
               "says nothing about the element path "
               "(reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).\n",
               (unsigned long long)g_serEntries);
    }
    if (g_slot0Vector != 0) {
        printf("[RtPort] UI::StringTable slot 0: THE VECTOR ARM RAN (%llu). The "
               "0x18007ec98 door named in this file's banner is LIVE; it is "
               "unregistered and is an EXEC-of-0 at cutover.\n",
               (unsigned long long)g_slot0Vector);
    }
    fflush(stdout);
}
struct SlotsReport {
    ~SlotsReport() {
        if (g_slot0Scalar || g_slot0Vector || g_serEntries) StringTableSlotsReport();
    }
};
SlotsReport g_slotsReport;

// A slot of an object's own vtable, by BYTE offset, so the call site reads the
// way the disassembly does.
void* VSlotOf(const void* obj, size_t byteOffset) {
    void** vtbl = *reinterpret_cast<void** const*>(obj);
    return vtbl[byteOffset / sizeof(void*)];
}

}  // namespace

// ---- slot 0 ----------------------------------------------------------------
extern "C" void* RtUiStringTable_ScalarDelete(void* self, int flag) {
    uint8_t* p = static_cast<uint8_t*>(self);

    if (flag & 2) {                                          // 180002762
        ++g_slot0Vector;
        // THE DOOR. See the banner: 0x18007ec98 is unregistered in this tree.
        EhVecDtorFn ehVec = reinterpret_cast<EhVecDtorFn>(
            ResolveAddr(Module::Runtime, 0x18007ec98));
        DtorFn elemDtor = reinterpret_cast<DtorFn>(
            ResolveAddr(Module::Runtime, 0x18011d8f0));      // ??1StringTable@UI@@
        const int count = *reinterpret_cast<const int32_t*>(p - 8);   // 18000276e
        ehVec(p, kStringTableSize, count, elemDtor);         // 180002777
        if (flag & 1) {                                      // 18000277c
            ++g_slot0Freed;
            reinterpret_cast<FreeFn>(ResolveAddr(Module::Runtime, 0x180006a10))(p - 8);
        }
        return p - 8;                                        // 18000278b
    }

    ++g_slot0Scalar;
    // The scalar arm IS ??1StringTable@UI@@ inlined (banner). Going through the
    // exported dtor keeps this slot following a promotion of that row instead of
    // freezing today's three steps, and ResolveAddr is registry-first so it
    // reaches our body the moment one exists.
    reinterpret_cast<DtorFn>(ResolveAddr(Module::Runtime, 0x18011d8f0))(self);

    if (flag & 1) {                                          // 1800027ad
        ++g_slot0Freed;
        // ??3StringTable@UI@@SAXPEAX@Z 0x180002220 - a null-guarded MemFree
        // (TEST RCX,RCX / JZ ret, then the allocator Enter / Free / Leave
        // triple). EXPORTED, so this is a NAME door, not an address one:
        // AEAA/SAXPEAX means the access specifier, not "unexported"
        // (reference_aeaa_is_private_not_unexported.md).
        RT_EXPORT_ONCE(FreeFn, "??3StringTable@UI@@SAXPEAX@Z")(self);
    }
    return self;                                             // 1800027bb
}

// ---- slot 5 ----------------------------------------------------------------
namespace UI {

bool StringTable::Serialize(void* streamV) {
    uint8_t*       stream = static_cast<uint8_t*>(streamV);

    SerializerPair ser;
    ser.stream = stream;                                     // 18011d957
    ser.impl   = nullptr;                                    // 18011d95c

    void*    enc       = *reinterpret_cast<void**>(stream + 0x10);   // 18011d965
    const int32_t dir  = *reinterpret_cast<const int32_t*>(stream + 0x8);
    const size_t  slot = (dir == 0) ? 0x18 : 0x20;                   // 18011d96f
    ser.impl = reinterpret_cast<SerFactoryFn>(VSlotOf(enc, slot))(enc, &ser, 0);

    int32_t count = _size;                                   // 18011d987
    reinterpret_cast<SerU32Fn>(VSlotOf(ser.impl, 0xa0))(
        ser.impl, reinterpret_cast<uint32_t*>(&count), nullptr);     // 18011d99e

    // The direction is RE-READ from the stream here, not cached from above:
    // 18011d9a4 reloads [RSP+0x28] and 18011d9a9 tests [RAX+0x8] again.
    if (*reinterpret_cast<const int32_t*>(stream + 0x8) == 0) {      // 18011d9a9
        ++g_serRead;
        // ENDGAME B.3.2 step 2 — `_data`/`_size`/`_capacity` are the
        // {data,size,capacity} triple `Collection::Array<TextEntry>` would use,
        // contiguous and asserted at StringTable.h's offsets, and
        // `kSpec_StringPair10` is NON-TRIVIAL, so the call bridges through it.
        rtcoll::AsArray<&rtcoll::kSpec_StringPair10>(&_data)
            ->SetSize(count);                                 // 18011d9b7, retail 0x180078c90
    }

    uint8_t* e = reinterpret_cast<uint8_t*>(_data);          // 18011d9bc
    StrSerFn strSer = reinterpret_cast<StrSerFn>(
        ResolveAddr(Module::Runtime, 0x1800c9930));          // String::Serialize
    for (int32_t i = 0; i < count; ++i, e += 0x10) {         // bound is the LOCAL
        ++g_serElems;
        strSer(e,       &ser);                               // 18011d9d8
        strSer(e + 0x8, &ser);                               // 18011d9e6
    }

    if (ser.impl != nullptr) {                               // 18011d9f9
        reinterpret_cast<DeleteFn>(VSlotOf(ser.impl, 0))(ser.impl, 1);   // 18011da06
    } else {
        ++g_serNullImpl;
    }
    return true;                                             // 18011da08
}

// ~StringTable: NON-VIRTUAL and EMPTY; `RtUiStringTable_Dtor`
// (stringtable_lifecycle.cpp) is the registered door that does retail's work.
StringTable::~StringTable() {}

}  // namespace UI

extern "C" bool RtUiStringTable_Serialize(void* selfV, void* streamV) {
    ++g_serEntries;
    return reinterpret_cast<UI::StringTable*>(selfV)                             // B.3.6 bridge
               ->UI::StringTable::Serialize(streamV);
}

int RtUiStringTableSlotsRegister() {
    int n = 0;
    // SLOT 0 IS ADDRESS-ONLY, and that is checked rather than assumed: the
    // retail export directory (pefile, 30 StringTable@UI symbols) contains no
    // `??_G`/`??_E` entry for this class, so there is no name to register under.
    RegisterRuntimeAddr(0x180002740ull, (void*)&RtUiStringTable_ScalarDelete);    ++n;
    RegisterRuntimeExport("?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUiStringTable_Serialize);
    RegisterRuntimeAddr(0x18011d940ull, (void*)&RtUiStringTable_Serialize);       ++n;
    return n;
}
