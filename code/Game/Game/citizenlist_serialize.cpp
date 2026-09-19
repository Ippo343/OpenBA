// code/Game/Game/citizenlist_serialize.cpp
//
// CitizenBehavior::Serialize (exe 0x14008f760, Ghidra: TradeComponent_impl_f760)
// and its post-load spatial-map rebuild pass (exe 0x14008f580, Ghidra:
// TradeComponent_impl_f580). CitizenBehavior lives at GameUpdateLoop+0xF0
// (m_citizenList), 0x1C0 bytes — see code/Game/Entity/citizenbehavior.cpp for
// the rest of the class (Update/CountResources/BuildPathGraph/...).
//
// ============================================================
// CitizenBehaviorRaw field layout used by Serialize (byte offsets; Ghidra's
// decompile indexes `longlong *param_1` in qwords — every offset below is
// that index * 8). Each "IBP array" is a 0x10-byte header {void* data@+0,
// int32 count@+8, int32 cap@+0xC} — i.e. Collection::Array<T>;
// elements are 8-byte Object::InternalBasePtr slots.
//
//   +0x00  IBP[]  type=ComponentEntity                 (count field +0x08)
//   +0x10  IBP[]  type=ComponentEntity                 (count field +0x18)
//   +0x20  IBP[]  type=PopulationComponent (0x1401c4790)(count field +0x28)   [NOTE: order verified against disasm, see below]
//   +0x30  IBP[]  type=ComponentEntity                 (count field +0x38)
//   +0x40  IBP[]  type=CemeteryComponent  (0x1401c4090) (count field +0x48)
//   +0x50  IBP[]  type=HappinessComponent (0x1401c42c0) (count field +0x58)
//   +0x60  IBP[]  type=AttendanceComponent(0x1401c3f40) (count field +0x68)
//   +0x70  IBP[]  type=HealerComponent    (0x1401c4330) (count field +0x78)
//   +0x80  IBP[]  type=LivestockComponent (0x1401c7970) (count field +0x88)
//   +0x90  IBP[]  type=PastureComponent   (0x1401c7ba0) (count field +0x98)
//   +0xa0  IBP[]  type=CitizenComponent   (0x1401c7158) (count field +0xa8)
//          -> unconditional: FUN_140017a40(this+0x100, ser)  TREE #1 (string-table key)
//   +0xb0  IBP[]  type=HerdComponent      (0x1401c7820) (count field +0xb8)
//   +0xc0  IBP[]  type=PopulationComponent(0x1401c4790) (count field +0xc8)
//   +0xd0  IBP[]  type=BuildComponent     (0x1401c7008) (count field +0xd8)
//   +0xe0  IBP[]  type=WellComponent      (0x1401c4c28) (count field +0xe8)
//   +0xf0  IBP[]  type=DiseaseComponent   (0x1401c4170) (count field +0xf8)  [version>0 only]
//   +0x100 tree#1 {root@0,count@8}    node=0x38B {key,valHdr[16],parent,left,right,balance}
//   +0x110 ExternalPtr<ComponentDescription>[]          (count field +0x118)
//   +0x120 ExternalPtr<ComponentDescription>[]          (count field +0x128)
//   +0x130 int32-pair[] (stride 8)                      (count field +0x138)
//   +0x140 int-assoc[] (stride 0x10 outer)               (count field +0x148) [version>4 only]
//   +0x150 int-assoc[] (stride 0x10 outer, "stats")      (count field +0x158)
//   +0x160 tree#2 {root@0,count@8}  (BuildComponentRef key) [version>=2 live path]
//   +0x170 ProfessionResourceEntry[21] (stride 0x20)     (count field +0x178) [version>=4 live path]
//   +0x180 SummaryEntry[2] (stride 0x10)                 (count field +0x188)
//   +0x190 Time  (System::Time::Serialize)
//   +0x198 Time  (System::Time::Serialize)
//   +0x1a0 Time  (System::Time::Serialize)
//   +0x1a8 float (raw Serialize)
//   +0x1ac int32 (raw Serialize)   [version>1 only]
//   +0x1b0 int32 (raw Serialize)   [version>2 only]
//
// The exact array->offset mapping above was read directly off the decompile's
// `param_1 + N` arithmetic (N in qword units); the semantic type-id ->
// component-name mapping comes from cross-referencing the .rdata type-id
// address against this project's existing RegisterType/factory registrations
// (see per-field comments below).
//
// Dependencies (all verified against the live tree; nothing here is stubbed):
//   Collection::Array<T>::SetSize   — the exe's own monomorphisations
//                                     (0x1400032e0/0x1400021d0/0x140001bb0) reached
//                                     by name through Component/coll_bridge.h.
//   fn_InternalBasePtr_Serialize    — engine_funcs.h (0x140152aa0 dispatch slot)
//   fn_InternalBasePtr_CheckPtr     — engine_funcs.h
//   fn_IsDerivedFrom                — engine_funcs.h
//   g_fn_Serializer_Ctor/Dtor/GetVersion, g_fn_Serialize_Int/Bool, g_fn_Time_Serialize
//                                   — engine_funcs.h
//   fn_ExternalPtr_BuildSerialize   — engine_funcs.h, resolves to registered 0x140002340
//                                     (used as TREE #2's key resolver: Ghidra's
//                                     "Object_ExternalPtr_BuildComponentRef_Serialize")
//   FUN_140018020 (TREE #1's key resolver, "string-table" ExternalPtr variant) —
//                                     registered at 0x140018020 (Reimpl_ExternalPtr_
//                                     Serialize_StringTable); resolved locally here.
//   fn_Serializer_SerializeBulk     — engine_funcs.h, used for the int-assoc arrays'
//                                     inner int[] bulk serialize.
//   TradeWork_impl_4b40 (0x140014b40) — shared AVL retrace, already ported+registered
//                                     in code/Game/Player/tool_base_highlight.cpp.
//   Reimpl_ExternalPtrArray_Serialize (0x1400049c0) — already ported+registered in
//                                     code/Game/Game/serialize_externals_array.cpp.
//   TradeDescription_impl_e360 (0x14000e360) — DEAD-PATH ONLY (version<2 legacy tree,
//                                     see below); already ported+registered in
//                                     code/Game/Component/citizenai_housing_idle.cpp.
//
// Newly ported in THIS file (none registered elsewhere):
//   FUN_140017640 ("StatsArray::Init", stride-0x10 outer array resize + inner
//                  deep-copy on grow) — also resolves engine_funcs.h's
//                  fn_StatsArray_Init (used by CitizenBehavior_Ctor).
//   FUN_1400177c0 ("ProfResArray::Init", stride-0x20 array resize)          — also
//                  resolves fn_ProfResArray_Init.
//   FUN_14000f9e0 (inner stride-4 array deep-copy helper for the above)     — file-local.
//   FUN_140017f60 (stride-8 int-pair array resize)                          — file-local.
//   Type-id storage cells for HealerComponent (0x1401c4330) and WellComponent
//   (0x1401c4c28) — nobody registered these two .rdata cells yet; this file adds
//   them (same pattern as reg_component_animal.cpp) so RegisterType fills them in.
//
// DEAD-CODE PATHS (ported best-effort for completeness; UNREACHABLE at our fixed
// save version 6 — see `IO::Serializer::Serializer(local_1e8,param_2,6)` at the
// top of the original function, which pins every write to version 6, and no
// real save this project produces or is expected to load will carry version<2 or
// version<4 short of a pre-release Banished save file):
//   - version<2 legacy key/int BST (TradeComponent_helper_ebe0, 0x14000ebe0)
//   - version<4 legacy Time-array -> ProfessionResourceEntry conversion block
// Both are implemented but not soak-tested; if a genuinely ancient save ever
// exercises them, verify against Ghidra before trusting the result.

#include "engine_funcs.h"
#include <cstdio>
#include "resolve.h"
#include "address_registry.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B

// ============================================================
// Externs for already-ported/registered sibling functions
// ============================================================
extern "C" void TradeWork_impl_4b40(int64_t* root, int64_t* nodeVarAddr);          // 0x140014b40 (tool_base_highlight.cpp)

namespace {

// ------------------------------------------------------------
// Serializer mode helper
// ------------------------------------------------------------
inline bool SerializerLoading(void* serializer) {
    void* inner = *reinterpret_cast<void**>(serializer);
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(inner) + 8) == 0;
}

// ------------------------------------------------------------
// Lazily-resolved leaf pointers not already in engine_funcs.h
// ------------------------------------------------------------
typedef void (*fn_KeyResolve_t)(void* field, void* serializer);
fn_KeyResolve_t s_fn_StringTableKeyResolve = nullptr;   // 0x140018020 (TREE #1 key)
typedef void (*fn_TreeRetraceE360_t)(int64_t* root, int64_t* nodeVarAddr);
fn_TreeRetraceE360_t s_fn_TreeRetrace_e360 = nullptr;    // 0x14000e360 (DEAD-PATH tree)

uint32_t g_tid_HealerComponent = 0;   // 0x1401c4330 — HealerComponent BaseTypeInfo
uint32_t g_tid_WellComponent   = 0;   // 0x1401c4c28 — WellComponent BaseTypeInfo
const uint32_t* s_tid_HappinessComponent = nullptr;   // 0x1401c42c0 (registered by reg_citizenai_typeids.cpp)
const uint32_t* s_tid_LivestockComponent = nullptr;   // 0x1401c7970 (registered by reg_component_animal.cpp)
const uint32_t* s_tid_HerdComponent      = nullptr;   // 0x1401c7820 (registered by reg_component_animal.cpp)
const uint32_t* s_tid_DiseaseComponent   = nullptr;   // 0x1401c4170 (registered by reg_component_cropdisease.cpp)
const uint32_t* s_tid_PopulationComponent= nullptr;   // 0x1401c4790 (registered by reg_animal_serialize.cpp)
const uint32_t* s_tid_BuildComponent     = nullptr;   // 0x1401c7008

bool s_resolved = false;
void EnsureResolved() {
    if (s_resolved) return;
    s_fn_StringTableKeyResolve = reinterpret_cast<fn_KeyResolve_t>(
        ResolveAddr(Module::Exe, 0x140018020));
    s_fn_TreeRetrace_e360 = reinterpret_cast<fn_TreeRetraceE360_t>(
        ResolveAddr(Module::Exe, 0x14000e360));
    s_tid_HappinessComponent  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c42c0));
    s_tid_LivestockComponent  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7970));
    s_tid_HerdComponent       = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7820));
    s_tid_DiseaseComponent    = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4170));
    s_tid_PopulationComponent = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4790));
    s_tid_BuildComponent      = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7008));
    s_resolved = true;
}

// ------------------------------------------------------------
// Registry census — the TownManager's 16 IBP registries in one line.
//
// Every town service reads one of these arrays (residences +0x20, cemeteries
// +0x40, attendance +0x60, ...) and every one of them is filled ONLY by a
// state-2 "enter" registrar, which a load never re-fires (the state machine is
// restored already in state 2). So on the load leg these counts ARE the town:
// if they come out zero, citizens have no homes, no jobs and no services, which
// looks like "everyone froze, then starved".
void RegistryCensus(uint8_t* base, const char* leg, const char* when) {
    static const struct { int off; const char* name; } kReg[] = {
        {0x08, "entA"},   {0x18, "entB"},   {0x28, "residence"}, {0x38, "entC"},
        {0x48, "cemetery"}, {0x58, "happiness"}, {0x68, "attendance"}, {0x78, "healer"},
        {0x88, "livestock"}, {0x98, "pasture"}, {0xa8, "citizen"}, {0xb8, "herd"},
        {0xc8, "population"}, {0xd8, "build"}, {0xe8, "well"}, {0xf8, "disease"},
    };
    std::fprintf(stderr, "[TownCensus] %s %s:", leg, when);
    for (const auto& r : kReg)
        std::fprintf(stderr, " %s=%d", r.name, *reinterpret_cast<int32_t*>(base + r.off));
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

// ------------------------------------------------------------
// IBP array serialize — the recurring 15-times pattern:
//   count = hdr.count; Serialize(count); if(loading) resize(hdr,count);
//   for each slot: InternalBasePtr::Serialize(slot); if (!IsDerivedFrom(typeId)) slot=null;
// hdr points at a 0x10-byte {void* data, int32 count, int32 cap} header.
// ------------------------------------------------------------
void SerializeIBPArray(uint8_t* hdr, void* serializer, const uint32_t* typeIdPtr) {
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge
    bool loading = SerializerLoading(serializer);
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte InternalBasePtr slot walked below.
    if (loading) GameColl::AsArray<GameColl::Ptr8>(hdr).SetSize(count);

    uint8_t* arr = *reinterpret_cast<uint8_t**>(hdr);
    for (int32_t i = 0; i < count; ++i) {
        uint8_t* elem = arr + static_cast<size_t>(i) * 8;
        fn_InternalBasePtr_Serialize(elem, serializer);
        void* val = *reinterpret_cast<void**>(elem);
        if (val != nullptr && typeIdPtr && *typeIdPtr) {
            if (!fn_IsDerivedFrom(val, *typeIdPtr)) {
                *reinterpret_cast<uint64_t*>(elem) = 0;
            }
        }
    }
}

// ------------------------------------------------------------
// FUN_14000f9e0 — deep-copy of an inner stride-4 (int32) array header
// (16 bytes: {ptr,count,cap}) used when 140017640 grows the outer array.
// ------------------------------------------------------------
void DeepCopyInner04Array(void* dstElem, void* srcElem) {
    if (dstElem == srcElem) return;
    uint8_t* d = static_cast<uint8_t*>(dstElem);
    uint8_t* s = static_cast<uint8_t*>(srcElem);
    void* old = *reinterpret_cast<void**>(d + 0);
    if (old && fn_MemFreeStatic) fn_MemFreeStatic(old);
    *reinterpret_cast<int32_t*>(d + 8) = 0;
    *reinterpret_cast<void**>(d + 0) = nullptr;
    int32_t cnt = *reinterpret_cast<int32_t*>(s + 8);
    *reinterpret_cast<int32_t*>(d + 8) = cnt;
    if (cnt > 0) {
        *reinterpret_cast<int32_t*>(d + 0xC) = cnt;
        if (fn_MemAlloc && g_pMemAllocSingleton)
            *reinterpret_cast<void**>(d + 0) =
                fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(cnt) << 2, 4);
    }
    int32_t newCnt = *reinterpret_cast<int32_t*>(d + 8);
    if (newCnt > 0) {
        int32_t* dp = *reinterpret_cast<int32_t**>(d + 0);
        int32_t* sp = *reinterpret_cast<int32_t**>(s + 0);
        if (dp && sp) for (int32_t i = 0; i < newCnt; ++i) dp[i] = sp[i];
    }
}

// ------------------------------------------------------------
// ENDGAME B.3.2 step 2: the three resize bodies below WERE hand-written
// copies of `rtcoll::SetSizeCore` (runtime/src/collection/array.cpp). Each is
// now one `Collection::Array<>::SetSize` driven by an `ArraySpec` row that
// spells out the site's element ops. Compared arm for arm before the swap:
// exact-equal early return, `_size` stored before the capacity test,
// treble-on-single-append-from-non-empty, copy the preserved prefix only when
// the block moved, init the new tail, destroy the truncated tail in place /
// the whole old buffer on a realloc, then free. Two deliberate differences,
// identical to the ones code/Game/Goal/goal_exe_helpers.cpp documents: the
// allocation goes through the LOCKING static `MemAlloc` (0x1800c7a50) instead
// of the unlocked instance `Allocate` this file reached through `fn_MemAlloc`
// (same allocator, same heap), and the "fn_MemAlloc unavailable" scaffold
// guards are gone (fn_MemAlloc is resolved before any serializer runs).
// ------------------------------------------------------------
namespace {

// FUN_140017640 element: one inner {int32* data, int32 count, int32 cap}
// header, stride 0x10, align 8. Copy-construct = zero the header then the
// 0x14000f9e0 deep copy (an operator= over a zeroed lhs); init = zero the
// header; destroy = free the inner buffer and zero the header. The old body
// zeroed the header only on the in-place truncation arm and not on the
// realloc arm; there the header lives in the old block that is freed on the
// very next line, so the two extra stores are unobservable. (The second port
// of the same address, gameupdateloop_load.cpp, zeroes on both arms — this
// row reproduces that one exactly.)
void AssignIntAssocOuter(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0) = 0;
    *reinterpret_cast<uint64_t*>(d + 8) = 0;
    DeepCopyInner04Array(d, const_cast<uint8_t*>(s));
}
void InitIntAssocOuter(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0) = 0;
    *reinterpret_cast<uint64_t*>(d + 8) = 0;
}
void DestroyIntAssocOuter(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i) {
        uint8_t* e = base + static_cast<int64_t>(i) * 0x10;
        if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(e + 0));
        *reinterpret_cast<uint64_t*>(e + 0) = 0;
        *reinterpret_cast<uint64_t*>(e + 8) = 0;
    }
}
const rtcoll::ArraySpec kSpec_IntAssocOuter = {          // exe 0x140017640
    "Array<Array<int32>>", 0x10, 8,
    &AssignIntAssocOuter, &InitIntAssocOuter, &DestroyIntAssocOuter, false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using IntAssocOuterArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                             Collection::ExplicitSpec<&kSpec_IntAssocOuter>>;

// FUN_1400177c0 element: ProfessionResourceEntry, stride 0x20, align 8.
// Copy = whole-stride byte copy; init = {0, 100, 0, 0} written as
// u64@0, u64@8, u32@0x10, u64@0x18, so +0x14..+0x17 is left untouched;
// trivially destructible.
void AssignProfResEntry(uint8_t* d, const uint8_t* s) { std::memcpy(d, s, 0x20); }
void InitProfResEntry(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x08) = 100;
    *reinterpret_cast<uint32_t*>(d + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
}
const rtcoll::ArraySpec kSpec_ProfResEntry = {           // exe 0x1400177c0
    "Array<ProfessionResourceEntry>", 0x20, 8,
    &AssignProfResEntry, &InitProfResEntry, nullptr, true,
    /*assign unwritten*/ 0,    0,
    /*init   unwritten*/ 0x14, 0x18,
    /*seed zero       */ 0,    0 };
using ProfResEntryArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                            Collection::ExplicitSpec<&kSpec_ProfResEntry>>;

// FUN_140017f60 element: two int32, stride 8, align 4. Copy = 8-byte copy,
// init = zero, trivially destructible.
void AssignIntPair(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
}
void InitIntPair(uint8_t* d) { *reinterpret_cast<uint64_t*>(d) = 0; }
const rtcoll::ArraySpec kSpec_IntPair = {                // exe 0x140017f60
    "Array<{int32,int32}>", 8, 4,
    &AssignIntPair, &InitIntPair, nullptr, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using IntPairArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                       Collection::ExplicitSpec<&kSpec_IntPair>>;

}  // namespace

// ------------------------------------------------------------
// FUN_140017640 ("StatsArray::Init") — stride-0x10 outer array resize
// {ptr,count/cap}[N]; each element itself a stride-4 int array header,
// deep-copied via DeepCopyInner04Array on grow, freed on shrink.
// Same address engine_funcs.h's fn_StatsArray_Init resolves.
// ------------------------------------------------------------
void ResizeStride0x10(void* hdr, int n) {
    // B.3.2 bridge: CitizenBehavior+0x140/+0x150 are still raw 0x10-byte
    // headers in an unconverted exe struct.
    reinterpret_cast<IntAssocOuterArray*>(hdr)->SetSize(n);
}

// ------------------------------------------------------------
// FUN_1400177c0 ("ProfResArray::Init") — stride-0x20 resize; new slots
// default to {0, 100, 0, 0} (matches ProfessionResourceEntry ctor defaults).
// Same address engine_funcs.h's fn_ProfResArray_Init resolves.
// ------------------------------------------------------------
void ResizeStride0x20(void* hdr, int n) {
    // B.3.2 bridge: CitizenBehavior+0x170 is still a raw header.
    reinterpret_cast<ProfResEntryArray*>(hdr)->SetSize(n);
}

// ------------------------------------------------------------
// FUN_140017f60 — stride-8 (two int32) array resize; grows ×3 on the
// classic "exactly one past capacity" pattern, zero-fills new entries.
// ------------------------------------------------------------
void ResizeStride08Pair(void* hdr, int newCount) {
    // B.3.2 bridge: CitizenBehavior+0x130 is still a raw header.
    reinterpret_cast<IntPairArray*>(hdr)->SetSize(newCount);
}

// ------------------------------------------------------------
// int-assoc array (stride-0x10 outer, resized via ResizeStride0x10; each
// slot is itself a stride-4 int32[] array resized via Array<I32NoInit>::SetSize,
// bulk-serialized via fn_Serializer_SerializeBulk). Used at +0x140 (v>4)
// and +0x150 ("stats").
// ------------------------------------------------------------
// IO::Serializer::Serialize(int count, int* data, name) — exe dispatch slot
// 0x140151278. Distinct from fn_Serializer_SerializeBulk (slot 0x140152400),
// which is the BYTE-width overload.
void SerializeIntArray(void* serializer, int32_t count, void* data) {
    typedef void (*FnSerIntArr_t)(void*, int, void*, const char*);
    static FnSerIntArr_t s_fn = nullptr;
    static bool s_tried = false;
    if (!s_fn && !s_tried) {
        s_tried = true;
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151278));
        if (slot && *slot) s_fn = reinterpret_cast<FnSerIntArr_t>(*slot);
        else {
            std::fprintf(stderr, "[CitizenList::Serialize] dispatch slot 0x140151278 "
                    "(Serialize int-array) unresolved — int-assoc arrays will "
                    "desync the rest of the stream\n");
            std::fflush(stderr);
        }
    }
    if (s_fn) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, reinterpret_cast<int*>(data), nullptr);   // B.3.4 bridge
}

void SerializeIntAssocArray(uint8_t* hdr, void* serializer) {
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge
    bool loading = SerializerLoading(serializer);
    if (loading) ResizeStride0x10(hdr, count);

    uint8_t* slot = *reinterpret_cast<uint8_t**>(hdr);
    for (int32_t i = 0; i < count; ++i) {
        int32_t innerCount = *reinterpret_cast<int32_t*>(slot + 8);
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(innerCount, nullptr);   // B.3.4 bridge
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400021d0, stride 4 align 4,
        // grown tail left as the allocation found it (no init arm). The bulk int
        // serializer below overwrites all `innerCount` elements either way.
        if (loading) GameColl::AsArray<GameColl::I32NoInit>(slot).SetSize(innerCount);
        void* innerPtr = *reinterpret_cast<void**>(slot + 0);
        // Retail (0x1400900e7 for +0x150, 0x1400903b8 for +0x140) dispatches
        // through slot 0x140151278 = IO::Serializer::Serialize(int count,
        // int* data, name) — the INT-array overload, 4 bytes per element. The
        // elements really are int32 (the inner array is resized stride-4), and
        // 0x140152400 (fn_Serializer_SerializeBulk) is the BYTE overload, which
        // read a quarter of the bytes and desynced everything after it. Retail
        // never calls 0x140152400 anywhere in this function.
        SerializeIntArray(serializer, innerCount, innerPtr);
        slot += 0x10;
    }
}

// ------------------------------------------------------------
// TREE #1 / TREE #2 — component-ref-keyed BST mapping key -> IBP[] array.
// Node (0x38 bytes): {uint64 key@0, valHdr[16]@8 (ptr,count,cap), parent@0x18,
// left@0x20, right@0x28, int32 balance@0x30}. Rebalance via TradeWork_impl_4b40.
// ------------------------------------------------------------
uint64_t* AllocCompRefTreeNode(uint64_t key, uint64_t* parent, const uint8_t valBuf[16]) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    uint64_t* n = reinterpret_cast<uint64_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
    if (!n) return nullptr;
    n[0] = key;
    std::memcpy(&n[1], valBuf, 16);          // +0x08..+0x18: value header (moved, not copied-then-freed)
    n[3] = reinterpret_cast<uint64_t>(parent); // +0x18
    n[4] = 0;                                 // +0x20 left
    n[5] = 0;                                 // +0x28 right
    *reinterpret_cast<int32_t*>(&n[6]) = 0;   // +0x30 balance
    return n;
}

uint64_t* CompRefTreeMin(uint64_t* n) {
    if (!n) return nullptr;
    while (reinterpret_cast<uint64_t*>(n[4])) n = reinterpret_cast<uint64_t*>(n[4]);
    return n;
}
uint64_t* CompRefTreeSucc(uint64_t* n) {
    if (!n) return nullptr;
    if (n[5]) return CompRefTreeMin(reinterpret_cast<uint64_t*>(n[5]));
    uint64_t* parent = reinterpret_cast<uint64_t*>(n[3]);
    while (parent && reinterpret_cast<uint64_t*>(parent[5]) == n) {
        n = parent;
        parent = reinterpret_cast<uint64_t*>(parent[3]);
    }
    return parent;
}

void SerializeCompRefTree(uint8_t* hdr, void* serializer, fn_KeyResolve_t keyFn) {
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge
    bool loading = SerializerLoading(serializer);

    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            uint64_t key = 0;
            if (keyFn) keyFn(&key, serializer);
            uint8_t valBuf[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            SerializeIBPArray(valBuf, serializer, g_TypeId_ComponentEntity);

            uint64_t** rootPtr = reinterpret_cast<uint64_t**>(hdr);
            if (*rootPtr == nullptr) {
                *rootPtr = AllocCompRefTreeNode(key, nullptr, valBuf);
                *reinterpret_cast<int32_t*>(hdr + 8) = *reinterpret_cast<int32_t*>(hdr + 8) + 1;
                continue;
            }
            uint64_t* cur = *rootPtr;
            uint64_t* insertedNode = nullptr;
            bool dup = false;
            for (;;) {
                if (key < cur[0]) {
                    uint64_t* left = reinterpret_cast<uint64_t*>(cur[4]);
                    if (!left) {
                        insertedNode = AllocCompRefTreeNode(key, cur, valBuf);
                        cur[4] = reinterpret_cast<uint64_t>(insertedNode);
                        break;
                    }
                    cur = left;
                } else if (key <= cur[0]) {
                    dup = true;   // matches original: already present, skip insert + count++
                    break;
                } else {
                    uint64_t* right = reinterpret_cast<uint64_t*>(cur[5]);
                    if (!right) {
                        insertedNode = AllocCompRefTreeNode(key, cur, valBuf);
                        cur[5] = reinterpret_cast<uint64_t>(insertedNode);
                        break;
                    }
                    cur = right;
                }
            }
            if (!dup) {
                *reinterpret_cast<int32_t*>(hdr + 8) = *reinterpret_cast<int32_t*>(hdr + 8) + 1;
                TradeWork_impl_4b40(reinterpret_cast<int64_t*>(hdr),
                                     reinterpret_cast<int64_t*>(&insertedNode));
            }
        }
    } else {
        uint64_t* root = *reinterpret_cast<uint64_t**>(hdr);
        uint64_t* node = CompRefTreeMin(root);
        while (node) {
            if (keyFn) keyFn(node, serializer);
            SerializeIBPArray(reinterpret_cast<uint8_t*>(node) + 8, serializer, g_TypeId_ComponentEntity);
            node = CompRefTreeSucc(node);
        }
    }
}

// ------------------------------------------------------------
// DEAD-PATH: version<2 legacy key/int BST (TradeComponent_helper_ebe0,
// 0x14000ebe0). Node (0x30 bytes): {key@0,int32 value@8,parent@0x10,
// left@0x18,right@0x20,balance@0x28}. Rebalance via TradeDescription_impl_e360.
// Never reached at our fixed save version (6); ported for completeness only.
// ------------------------------------------------------------
uint64_t* AllocLegacyTreeNode(uint64_t key, uint64_t* parent, int32_t value) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    uint64_t* n = reinterpret_cast<uint64_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
    if (!n) return nullptr;
    n[0] = key;
    n[1] = static_cast<uint64_t>(static_cast<uint32_t>(value));
    n[2] = reinterpret_cast<uint64_t>(parent);
    n[3] = 0; n[4] = 0;
    *reinterpret_cast<int32_t*>(&n[5]) = 0;
    return n;
}
uint64_t* LegacyTreeMin(uint64_t* n) {
    if (!n) return nullptr;
    while (reinterpret_cast<uint64_t*>(n[3])) n = reinterpret_cast<uint64_t*>(n[3]);
    return n;
}
uint64_t* LegacyTreeSucc(uint64_t* n) {
    if (!n) return nullptr;
    if (n[4]) return LegacyTreeMin(reinterpret_cast<uint64_t*>(n[4]));
    uint64_t* parent = reinterpret_cast<uint64_t*>(n[2]);
    while (parent && reinterpret_cast<uint64_t*>(parent[4]) == n) {
        n = parent;
        parent = reinterpret_cast<uint64_t*>(parent[2]);
    }
    return parent;
}
void SerializeLegacyKeyIntTree(uint8_t* hdr, void* serializer) {
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge
    bool loading = SerializerLoading(serializer);
    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            uint64_t key = 0;
            if (fn_ExternalPtr_BuildSerialize) fn_ExternalPtr_BuildSerialize(&key, serializer);
            int32_t value = 0;
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(value, nullptr);   // B.3.4 bridge

            uint64_t** rootPtr = reinterpret_cast<uint64_t**>(hdr);
            if (*rootPtr == nullptr) {
                *rootPtr = AllocLegacyTreeNode(key, nullptr, value);
                *reinterpret_cast<int32_t*>(hdr + 8) = *reinterpret_cast<int32_t*>(hdr + 8) + 1;
                continue;
            }
            uint64_t* cur = *rootPtr;
            uint64_t* insertedNode = nullptr;
            bool dup = false;
            for (;;) {
                if (key < cur[0]) {
                    uint64_t* left = reinterpret_cast<uint64_t*>(cur[3]);
                    if (!left) { insertedNode = AllocLegacyTreeNode(key, cur, value); cur[3] = reinterpret_cast<uint64_t>(insertedNode); break; }
                    cur = left;
                } else if (key <= cur[0]) {
                    dup = true; break;
                } else {
                    uint64_t* right = reinterpret_cast<uint64_t*>(cur[4]);
                    if (!right) { insertedNode = AllocLegacyTreeNode(key, cur, value); cur[4] = reinterpret_cast<uint64_t>(insertedNode); break; }
                    cur = right;
                }
            }
            if (!dup) {
                *reinterpret_cast<int32_t*>(hdr + 8) = *reinterpret_cast<int32_t*>(hdr + 8) + 1;
                if (s_fn_TreeRetrace_e360)
                    s_fn_TreeRetrace_e360(reinterpret_cast<int64_t*>(hdr), reinterpret_cast<int64_t*>(&insertedNode));
            }
        }
    } else {
        uint64_t* node = LegacyTreeMin(*reinterpret_cast<uint64_t**>(hdr));
        while (node) {
            if (fn_ExternalPtr_BuildSerialize) fn_ExternalPtr_BuildSerialize(node, serializer);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int32_t*>(&node[1]), nullptr);   // B.3.4 bridge
            node = LegacyTreeSucc(node);
        }
    }
}

} // namespace

// ============================================================
// CitizenList_Serialize_8f760 — CitizenBehavior::Serialize
// ============================================================
extern "C" void CitizenList_Serialize_8f760(void* self, void* stream, void* /*p3*/, int* /*outParam*/) {
    if (!self || !stream) return;
    EnsureResolved();
    uint8_t* base = static_cast<uint8_t*>(self);

    alignas(8) uint8_t serBuf[0x18];
    void* ser = serBuf;
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 6);   // B.3.4 bridge

    const char* leg = SerializerLoading(ser) ? "LOAD" : "SAVE";
    RegistryCensus(base, leg, "enter");

    SerializeIBPArray(base + 0x00, ser, g_TypeId_ComponentEntity);
    SerializeIBPArray(base + 0x10, ser, g_TypeId_ComponentEntity);
    SerializeIBPArray(base + 0x30, ser, g_TypeId_ComponentEntity);
    SerializeIBPArray(base + 0x20, ser, g_TypeId_ComponentEntity);
    SerializeIBPArray(base + 0x70, ser, &g_tid_HealerComponent);
    SerializeIBPArray(base + 0xa0, ser, g_TypeId_CitizenComponent_FC);

    // Unconditional TREE #1 (residence/path-graph style, string-table key).
    SerializeCompRefTree(base + 0x100, ser, s_fn_StringTableKeyResolve);

    SerializeIBPArray(base + 0x90, ser, g_TypeId_PastureComponent);
    SerializeIBPArray(base + 0x40, ser, g_TypeId_CemeteryComponent);
    SerializeIBPArray(base + 0x50, ser, s_tid_HappinessComponent);
    SerializeIBPArray(base + 0x60, ser, g_TypeId_AttendanceComponent);
    SerializeIBPArray(base + 0x80, ser, s_tid_LivestockComponent);
    SerializeIBPArray(base + 0xb0, ser, s_tid_HerdComponent);

    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0) {   // B.3.4 bridge
        SerializeIBPArray(base + 0xf0, ser, s_tid_DiseaseComponent);
    }

    SerializeIBPArray(base + 0xc0, ser, s_tid_PopulationComponent);
    SerializeIBPArray(base + 0xd0, ser, s_tid_BuildComponent);

    // ExternalPtr<ComponentDescription>[] arrays, resized via Array<Ptr8>::SetSize
    // (FUN_140001bb0) then bulk-serialized via Reimpl_ExternalPtrArray_Serialize
    // (0x1400049c0), matching FUN_140001bb0(hdr,count) + FUN_1400049c0(ser,count,ptr,p4).
    {
        int32_t count = *reinterpret_cast<int32_t*>(base + 0x118);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte ExternalPtr<ComponentDescription>.
        if (SerializerLoading(ser))
            GameColl::ArrayAt<GameColl::Ptr8>(base, 0x110).SetSize(count);
        // Reimpl_ExternalPtrArray_Serialize is registered at 0x1400049c0; its
        // signature (serializer,count,arrayBase,deadInit) differs from the
        // single-field fn_ExternalPtr_BuildSerialize typedef, so resolve locally.
        typedef void (*fn_ExtArr_t)(void*, int, long long, void*);
        static fn_ExtArr_t s_extArr = reinterpret_cast<fn_ExtArr_t>(ResolveAddr(Module::Exe, 0x1400049c0));
        if (s_extArr) s_extArr(ser, count, reinterpret_cast<long long>(*reinterpret_cast<void**>(base + 0x110)), nullptr);
    }
    {
        int32_t count = *reinterpret_cast<int32_t*>(base + 0x128);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte ExternalPtr<ComponentDescription>.
        if (SerializerLoading(ser))
            GameColl::ArrayAt<GameColl::Ptr8>(base, 0x120).SetSize(count);
        typedef void (*fn_ExtArr_t)(void*, int, long long, void*);
        static fn_ExtArr_t s_extArr = reinterpret_cast<fn_ExtArr_t>(ResolveAddr(Module::Exe, 0x1400049c0));
        if (s_extArr) s_extArr(ser, count, reinterpret_cast<long long>(*reinterpret_cast<void**>(base + 0x120)), nullptr);
    }

    g_fn_Time_Serialize(base + 0x190, ser);
    g_fn_Time_Serialize(base + 0x198, ser);

    // "Stats" int-assoc array at +0x150.
    SerializeIntAssocArray(base + 0x150, ser);

    // TREE #2 (BuildComponentRef key) on the live path (version>=2, always true
    // at version 6); legacy key/int BST otherwise (DEAD — see file header).
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() < 2) {   // B.3.4 bridge
        SerializeLegacyKeyIntTree(base + 0x160, ser);
    } else {
        SerializeCompRefTree(base + 0x160, ser,
            reinterpret_cast<fn_KeyResolve_t>(fn_ExternalPtr_BuildSerialize));
    }
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() == 5) {   // B.3.4 bridge
        ResizeStride0x10(base + 0x160 /*unused for v==5 clamp — mirrors 0x28 below*/, 0);
    }

    SerializeIBPArray(base + 0xe0, ser, &g_tid_WellComponent);

    // Stride-8 int-pair array at +0x130.
    {
        int32_t count = *reinterpret_cast<int32_t*>(base + 0x138);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
        bool loading = SerializerLoading(ser);
        if (loading) ResizeStride08Pair(base + 0x130, count);
        int32_t* pairs = *reinterpret_cast<int32_t**>(base + 0x130);
        for (int32_t i = 0; i < count; ++i) {
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(pairs[i * 2 + 0], nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(pairs[i * 2 + 1], nullptr);   // B.3.4 bridge
        }
    }

    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 4) {   // B.3.4 bridge
        SerializeIntAssocArray(base + 0x140, ser);
    }
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() == 5) {   // B.3.4 bridge
        ResizeStride0x10(base + 0x140, 0);
    }

    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() < 4) {   // B.3.4 bridge
        // DEAD-PATH (version<4): legacy Time-array -> ProfessionResourceEntry /
        // SummaryEntry conversion. Never reached at our fixed version 6 — see file
        // header. Left unimplemented (no-op) rather than risk a wrong-stride write
        // into +0x170/+0x180 from an unverifiable Ghidra pointer-arithmetic reading;
        // a real pre-release save hitting this path needs a fresh Ghidra pass before
        // it can be trusted, so we intentionally stop short of guessing here.
    } else {
        int32_t profCount = *reinterpret_cast<int32_t*>(base + 0x178);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(profCount, nullptr);   // B.3.4 bridge
        bool loading = SerializerLoading(ser);
        if (loading) ResizeStride0x20(base + 0x170, profCount);
        if (profCount > 0) {
            uint8_t* entry = *reinterpret_cast<uint8_t**>(base + 0x170) + 8;
            for (int32_t i = 0; i < profCount; ++i) {
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry - 0x08), nullptr);   // B.3.4 bridge
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry - 0x04), nullptr);   // B.3.4 bridge
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry + 0x00), nullptr);   // B.3.4 bridge
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry + 0x04), nullptr);   // B.3.4 bridge
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry + 0x08), nullptr);   // B.3.4 bridge
                g_fn_Time_Serialize(entry + 0x0C, ser);
                entry += 0x20;
            }
        }
        if (*reinterpret_cast<int32_t*>(base + 0x178) != 0x15) {
            ResizeStride0x20(base + 0x170, 0x15);
        }

        int32_t sumCount = *reinterpret_cast<int32_t*>(base + 0x188);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(sumCount, nullptr);   // B.3.4 bridge
        if (loading) {
            // FUN_14000f090 — already registered (StorageComponent_DynArrayResize et al).
            typedef void (*fn_Resize10_t)(void*, int);
            static fn_Resize10_t s_resize10 = reinterpret_cast<fn_Resize10_t>(ResolveAddr(Module::Exe, 0x14000f090));
            if (s_resize10) s_resize10(base + 0x180, sumCount);
        }
        if (sumCount > 0) {
            uint8_t* entry = *reinterpret_cast<uint8_t**>(base + 0x180) + 8;
            for (int32_t i = 0; i < sumCount; ++i) {
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry - 0x08), nullptr);   // B.3.4 bridge
                reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(entry - 0x04), nullptr);   // B.3.4 bridge
                g_fn_Time_Serialize(entry + 0x00, ser);
                entry += 0x10;
            }
        }
    }

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(base + 0x1a8), nullptr);   // B.3.4 bridge
    g_fn_Time_Serialize(base + 0x1a0, ser);

    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 1) {   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(base + 0x1ac), nullptr);   // B.3.4 bridge
    }
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 2) {   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(base + 0x1b0), nullptr);   // B.3.4 bridge
    }

    RegistryCensus(base, leg, "exit ");
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// CitizenList_PostLoad_8f580 — CitizenBehavior post-load pass.
// Rebuilds a spatial grid-cell map (townMgr+0x140, stride-0x10 outer array
// sized (mapWidth/8)^2) from the residence array (+0x138/+0x130) and the
// profession-BST-derived entity list (+0xe0/+0xe8), inserting each entity's
// grid cell into the map via FUN_140088a80 (already ported+registered as
// WellComp_AddCellEntry_88a80 — reused here as-is, same address/signature).
// ============================================================
typedef void (*fn_AddCellEntry_t)(void* self, int cellX, int cellY, int idOrSlot);
static fn_AddCellEntry_t s_fn_AddCellEntry = nullptr;

// DWORD_1401c79e0 — the component type-id validated inside PostLoad's inner
// scan (the "position"-like component providing cell coords at +0x28/+0x2a).
// Not identified against a named component elsewhere in the project; resolved
// lazily by raw .rdata address like every other DWORD_/DAT_ type-id cell here.
static uint32_t DWORD_1401c79e0_TypeId() {
    static const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    return p ? *p : 0;
}

extern "C" void CitizenList_PostLoad_8f580(void* self) {
    if (!self) return;
    EnsureResolved();
    if (!s_fn_AddCellEntry) {
        s_fn_AddCellEntry = reinterpret_cast<fn_AddCellEntry_t>(ResolveAddr(Module::Exe, 0x140088a80));
    }
    uint8_t* base = static_cast<uint8_t*>(self);

    if (*reinterpret_cast<int32_t*>(base + 0x148) >= 1) return;   // already populated

    int* mapGrid = *reinterpret_cast<int**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);
    int side = *mapGrid >> 3;
    ResizeStride0x10(base + 0x140, side * side);
    std::fprintf(stderr, "[CitPostLoad] mapGrid=%p mapWidth=%d side=%d cells=%d "
            "cellMap=%p residence{ptr=%p count=%d} ent{ptr=%p count=%d}\n",
            (void*)mapGrid, *mapGrid, side, side * side,
            *reinterpret_cast<void**>(base + 0x140),
            *reinterpret_cast<void**>(base + 0x130),
            *reinterpret_cast<int32_t*>(base + 0x138),
            *reinterpret_cast<void**>(base + 0xe0),
            *reinterpret_cast<int32_t*>(base + 0xe8));
    std::fflush(stderr);

    int idx = 0;
    int32_t residenceCount = *reinterpret_cast<int32_t*>(base + 0x138);
    if (residenceCount > 0 && s_fn_AddCellEntry) {
        // +0x130 is the STRIDE-8 int-pair array (ResizeStride08Pair in Serialize
        // above); retail walks it with `lVar11 += 8` and reads only the low int
        // of each pair (0x14008f5c8). Indexing it as int32[] read every second
        // element from the pair's HIGH half, which is not a cell index at all —
        // the resulting x/y ran off the 32x32 cell map and AddCellEntry wrote
        // through a wild record pointer.
        int32_t* residenceCells = *reinterpret_cast<int32_t**>(base + 0x130);
        int mapWidth = *mapGrid;
        for (; idx < residenceCount; ++idx) {
            int cell = residenceCells[idx * 2];
            if (cell < 0 || cell >= mapWidth * mapWidth) {
                std::fprintf(stderr, "[CitPostLoad] residence[%d] cell=%d pairHi=%d "
                        "x=%d y=%d cellIdx=%d (max %d)\n",
                        idx, cell, residenceCells[idx * 2 + 1],
                        cell % mapWidth, cell / mapWidth,
                        (mapWidth >> 3) * ((cell / mapWidth) >> 3) + ((cell % mapWidth) >> 3),
                        side * side);
                std::fflush(stderr);
            }
            s_fn_AddCellEntry(base, cell % mapWidth, cell / mapWidth, idx);
        }
    }

    idx = 0;
    int32_t entCount = *reinterpret_cast<int32_t*>(base + 0xe8);
    if (entCount > 0 && s_fn_AddCellEntry) {
        int64_t* entArr = *reinterpret_cast<int64_t**>(base + 0xe0);
        int slot = -1;
        for (; idx < entCount; ++idx, --slot) {
            uint64_t entIBP = entArr[idx];
            uint64_t comp = *reinterpret_cast<uint64_t*>(entIBP + 0x10);

            void* chosen = nullptr;
            int32_t compCount = *reinterpret_cast<int32_t*>(comp + 0x38);
            for (int32_t j = 0; j < compCount; ++j) {
                void** slotPtr = reinterpret_cast<void**>(*reinterpret_cast<uint64_t*>(comp + 0x30) + static_cast<int64_t>(j) * 8);
                void* cand = *slotPtr;
                if (!cand) continue;
                void** vt = *reinterpret_cast<void***>(cand);
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                uint32_t tid = reinterpret_cast<fn_GetTypeId_t>(vt[2])(cand);
                if (tid == DWORD_1401c79e0_TypeId()) {
                    slotPtr = reinterpret_cast<void**>(*reinterpret_cast<uint64_t*>(comp + 0x30) + static_cast<int64_t>(j) * 8);
                    void* obj = *slotPtr;
                    if (obj && fn_IsDerivedFrom(obj, DWORD_1401c79e0_TypeId())) chosen = obj;
                    break;
                }
            }

            uint8_t chosenIBP[8] = {0};
            new (chosenIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(chosen));   // B.3.3 bridge
            uint64_t chosenVal = *reinterpret_cast<uint64_t*>(chosenIBP);
            int16_t cx = chosenVal ? *reinterpret_cast<int16_t*>(chosenVal + 0x28) : 0;
            int16_t cy = chosenVal ? *reinterpret_cast<int16_t*>(chosenVal + 0x2a) : 0;
            s_fn_AddCellEntry(base, cx, cy, slot);
        }
    }
}

// ============================================================
// Registration
// ============================================================
void RegisterCitizenListSerialize() {
    RegisterExeAddr(0x14008f760, reinterpret_cast<uintptr_t>(&CitizenList_Serialize_8f760));
    RegisterExeAddr(0x14008f580, reinterpret_cast<uintptr_t>(&CitizenList_PostLoad_8f580));

    // Newly-ported helpers not registered elsewhere.
    RegisterExeAddr(0x140017640, reinterpret_cast<uintptr_t>(&ResizeStride0x10));
    RegisterExeAddr(0x1400177c0, reinterpret_cast<uintptr_t>(&ResizeStride0x20));
    RegisterExeAddr(0x140017f60, reinterpret_cast<uintptr_t>(&ResizeStride08Pair));

    // Type-id storage cells nobody registered yet (HealerComponent, WellComponent).
    RegisterExeAddr(0x1401c4330, reinterpret_cast<uintptr_t>(&g_tid_HealerComponent));
    RegisterExeAddr(0x1401c4c28, reinterpret_cast<uintptr_t>(&g_tid_WellComponent));
}
