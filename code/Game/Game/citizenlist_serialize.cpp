// code/Game/Game/citizenlist_serialize.cpp
//
// CitizenBehavior::Serialize (exe 0x14008f760, Ghidra: TradeComponent_impl_f760)
// and its post-load spatial-map rebuild pass CitizenBehavior::PostLoad (exe
// 0x14008f580, Ghidra: TradeComponent_impl_f580). CitizenBehavior lives at GameUpdateLoop+0xF0
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

#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "Game/Map.h"   // m_game's class (B.4 GAME-a)
#include "Entity/CitizenBehavior.h"         // B4 GAME-a: CitizenBehavior::Serialize / PostLoad
#include <cstdio>
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include "Component/LivestockComponent.h"
#include "Component/HerdComponent.h"
#include "Component/DiseaseComponent.h"
#include "Component/PopulationComponent.h"
#include "Component/BuildComponent.h"
#include "Component/MapComponent.h"
#include "Component/HealerComponent.h"       // ENDGAME B.3.9 CB: 0x1401c4330 cell class
#include "Component/WellComponent.h"         // ENDGAME B.3.9 CB: 0x1401c4c28 cell class
#include "Component/HappinessComponent.h"    // ENDGAME B.3.9 CB: 0x1401c42c0 cell class
#include "Component/CitizenComponent.h"      // ENDGAME B.3.9 CB: 0x1401c7158 cell class
#include "Component/PastureComponent.h"      // ENDGAME B.3.9 CB: 0x1401c7ba0 cell class
#include "Component/CemeteryComponent.h"     // ENDGAME B.3.9 CB: 0x1401c4090 cell class
#include "Component/AttendanceComponent.h"   // ENDGAME B.3.9 CB: 0x1401c3f40 cell class
#include "Entity/ComponentEntity.h"          // ENDGAME B.3.9 CB: 0x1401c5d70 cell class
#include <cstdint>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // caller batch CB6: Object::Base::Type()
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "Game/exe_bodies.h"
#include "Player/exe_bodies.h"              // TradeWork_impl_4b40
#include "Component/exe_bodies.h"           // CitizenAI_Reg_helper_e360, StorageComponent_DynArrayResize
#include "Component/wellcomponent_funcs.h"  // WellComp_AddCellEntry_88a80

// Already-ported sibling functions, called by name through their declaring headers:
//   0x140014b40 TradeWork_impl_4b40 (Player/exe_bodies.h), 0x14000e360
//   CitizenAI_Reg_helper_e360 (Component/exe_bodies.h), 0x140018020 / 0x140002340
//   Reimpl_ExternalPtr_Serialize_StringTable and 0x1400049c0
//   Reimpl_ExternalPtrArray_Serialize (Game/exe_bodies.h), 0x14000f090
//   StorageComponent_DynArrayResize (Component/exe_bodies.h), 0x140088a80
//   WellComp_AddCellEntry_88a80 (Component/wellcomponent_funcs.h).

namespace {

// ------------------------------------------------------------
// Serializer mode helper
// ------------------------------------------------------------
inline bool SerializerLoading(IO::Serializer* serializer) {
    return static_cast<int>(serializer->GetDirection()) == 0;   // IO::Serializer::GetDirection, retail 0x180006210
}

// The tree serializer's key resolver (TREE #1: 0x140018020, TREE #2: 0x140002340 --
// one body, Reimpl_ExternalPtr_Serialize_StringTable).
typedef void (*fn_KeyResolve_t)(void* field, void* serializer);

// ------------------------------------------------------------
// Registry census — the TownManager's 16 IBP registries in one line.
//
// Every town service reads one of these arrays (residences +0x20, cemeteries
// +0x40, attendance +0x60, ...) and every one of them is filled ONLY by a
// state-2 "enter" registrar, which a load never re-fires (the state machine is
// restored already in state 2). So on the load leg these counts ARE the town:
// if they come out zero, citizens have no homes, no jobs and no services, which
// looks like "everyone froze, then starved".
void RegistryCensus(const CitizenBehavior* cb, const char* leg, const char* when) {
    static const struct { int32_t CitizenBehavior::* count; const char* name; } kReg[] = {
        {&CitizenBehavior::field_0x08, "entA"},   {&CitizenBehavior::field_0x18, "entB"},
        {&CitizenBehavior::field_0x28, "residence"}, {&CitizenBehavior::field_0x38, "entC"},
        {&CitizenBehavior::cemeteryCount, "cemetery"},
        {&CitizenBehavior::happinessComponentCount, "happiness"},
        {&CitizenBehavior::attendanceComponentCount, "attendance"},
        {&CitizenBehavior::healerCount, "healer"},
        {&CitizenBehavior::livestockCount, "livestock"}, {&CitizenBehavior::pastureCount, "pasture"},
        {&CitizenBehavior::citizenCount, "citizen"}, {&CitizenBehavior::herdCount, "herd"},
        {&CitizenBehavior::populationCount, "population"},
        {&CitizenBehavior::buildComponentCount, "build"},
        {&CitizenBehavior::wellCount, "well"}, {&CitizenBehavior::diseaseCount, "disease"},
    };
    std::fprintf(stderr, "[TownCensus] %s %s:", leg, when);
    for (const auto& r : kReg)
        std::fprintf(stderr, " %s=%d", r.name, cb->*r.count);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

// ------------------------------------------------------------
// IBP array serialize — the recurring 15-times pattern:
//   count = hdr.count; Serialize(count); if(loading) resize(hdr,count);
//   for each slot: InternalBasePtr::Serialize(slot); if (!IsDerivedFrom(typeId)) slot=null;
// `data` is the first field of a 0x10-byte {T** data, int32 count, int32 cap}
// header (a Collection::Array<InternalPtr<T>>, declared as three fields).
// ------------------------------------------------------------
// ENDGAME B.3.9 CB: takes the type id by VALUE (the class static of the cell retail
// reads) and validates unconditionally, as retail does (0x14008f760: every array's
// `if (p && !IsDerivedFrom(p, [cell])) p = 0`). The old pointer form skipped the check
// whenever the cell read 0 -- which it always did for Healer (0x1401c4330), Well
// (0x1401c4c28) and Happiness (0x1401c42c0), so those three arrays were never validated.
template <class T>
void SerializeIBPArray(T*** data, int32_t* countField, IO::Serializer* serializer, uint32_t typeId) {
    int32_t count = *countField;
    serializer->Serialize(count, nullptr);
    bool loading = SerializerLoading(serializer);
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte InternalBasePtr slot walked below.
    if (loading) GameColl::AsArray<GameColl::Ptr8>(data).SetSize(count);

    T** arr = *data;
    for (int32_t i = 0; i < count; ++i) {
        T** elem = &arr[i];
        // LEFT: an InternalBasePtr slot (the IBP's raw pointer).
        reinterpret_cast<Object::InternalBasePtr*>(elem)->Serialize(*serializer);   // B.3.4 bridge
        T* val = *elem;
        if (val != nullptr) {
            if (!::Object::IsDerivedFromNullSafe(val, typeId)) {
                *elem = nullptr;
            }
        }
    }
}

// ------------------------------------------------------------
// FUN_14000f9e0 — deep-copy of an inner stride-4 (int32) array header
// (16 bytes: {ptr,count,cap}) used when 140017640 grows the outer array.
// ------------------------------------------------------------
void DeepCopyInner04Array(Collection::Array<int32_t>* d, const Collection::Array<int32_t>* s) {
    if (d == s) return;
    void* old = d->_data;
    if (old) System::MemoryAllocator::MemFree(old);
    d->_size = 0;
    d->_data = nullptr;
    int32_t cnt = s->_size;
    d->_size = cnt;
    if (cnt > 0) {
        d->_capacity = cnt;
        if (System::MemoryAllocator::_singleton)
            d->_data =
                static_cast<int32_t*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(cnt) << 2, 4));
    }
    int32_t newCnt = d->_size;
    if (newCnt > 0) {
        int32_t* dp = d->_data;
        int32_t* sp = s->_data;
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
    // The spec row's element is raw bytes; each element is one inner Array<int32>.
    DeepCopyInner04Array(reinterpret_cast<Collection::Array<int32_t>*>(d),
                         reinterpret_cast<const Collection::Array<int32_t>*>(s));
}
void InitIntAssocOuter(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0) = 0;
    *reinterpret_cast<uint64_t*>(d + 8) = 0;
}
void DestroyIntAssocOuter(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i) {
        uint8_t* e = base + static_cast<int64_t>(i) * 0x10;
        System::MemoryAllocator::MemFree(*reinterpret_cast<void**>(e + 0));
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

// ResizeStride0x10 / ResizeStride0x20 have external linkage (B3R R5): CitizenBehavior's
// ctor (code/Game/Entity/citizenbehavior.cpp) calls them by name through
// Game/exe_bodies.h. The file's anonymous namespace closes around them.
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

namespace {

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
void SerializeIntArray(IO::Serializer* serializer, int32_t count, int32_t* data) {
    serializer->Serialize(count, data, nullptr);
}

// `outer` is the first field of the {Array<int32>* data, count, cap} header.
void SerializeIntAssocArray(Collection::Array<int32_t>** outer, int32_t* countField,
                            IO::Serializer* serializer) {
    int32_t count = *countField;
    serializer->Serialize(count, nullptr);
    bool loading = SerializerLoading(serializer);
    if (loading) ResizeStride0x10(outer, count);

    Collection::Array<int32_t>* slot = *outer;
    for (int32_t i = 0; i < count; ++i) {
        int32_t innerCount = slot->_size;
        serializer->Serialize(innerCount, nullptr);
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400021d0, stride 4 align 4,
        // grown tail left as the allocation found it (no init arm). The bulk int
        // serializer below overwrites all `innerCount` elements either way.
        if (loading) GameColl::AsArray<GameColl::I32NoInit>(slot).SetSize(innerCount);
        int32_t* innerPtr = slot->_data;
        // Retail (0x1400900e7 for +0x150, 0x1400903b8 for +0x140) dispatches
        // through slot 0x140151278 = IO::Serializer::Serialize(int count,
        // int* data, name) — the INT-array overload, 4 bytes per element. The
        // elements really are int32 (the inner array is resized stride-4), and
        // 0x140152400 (fn_Serializer_SerializeBulk) is the BYTE overload, which
        // read a quarter of the bytes and desynced everything after it. Retail
        // never calls 0x140152400 anywhere in this function.
        SerializeIntArray(serializer, innerCount, innerPtr);
        ++slot;
    }
}

// ------------------------------------------------------------
// TREE #1 / TREE #2 — component-ref-keyed BST mapping key -> IBP[] array.
// Node (0x38 bytes): {uint64 key@0, valHdr[16]@8 (ptr,count,cap), parent@0x18,
// left@0x20, right@0x28, int32 balance@0x30}. Rebalance via TradeWork_impl_4b40.
// ------------------------------------------------------------
struct IbpArrayHeader {             // a Collection::Array<InternalPtr<ComponentEntity>>
    ComponentEntity** data;         // +0x00
    int32_t           count;        // +0x08
    int32_t           capacity;     // +0x0C
};
struct CompRefNode {
    uint64_t       key;             // +0x00 the ExternalPtr key (serialized by the key resolver)
    IbpArrayHeader values;          // +0x08
    CompRefNode*   parent;          // +0x18
    CompRefNode*   left;            // +0x20
    CompRefNode*   right;           // +0x28
    int32_t        balance;         // +0x30
    uint8_t        _pad34[4];       // +0x34
};
static_assert(sizeof(CompRefNode) == 0x38, "CompRefNode is MemAlloc(0x38)");
static_assert(offsetof(CompRefNode, values) == 0x08, "CompRefNode::values @ +0x08");
static_assert(offsetof(CompRefNode, parent) == 0x18, "CompRefNode::parent @ +0x18");
static_assert(offsetof(CompRefNode, left) == 0x20, "CompRefNode::left @ +0x20");
static_assert(offsetof(CompRefNode, right) == 0x28, "CompRefNode::right @ +0x28");
static_assert(offsetof(CompRefNode, balance) == 0x30, "CompRefNode::balance @ +0x30");

CompRefNode* AllocCompRefTreeNode(uint64_t key, CompRefNode* parent, const IbpArrayHeader& val) {
    if (!System::MemoryAllocator::_singleton) return nullptr;
    CompRefNode* n = static_cast<CompRefNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
    if (!n) return nullptr;
    n->key = key;
    n->values = val;                 // +0x08..+0x18: value header (moved, not copied-then-freed)
    n->parent = parent;              // +0x18
    n->left = nullptr;               // +0x20
    n->right = nullptr;              // +0x28
    n->balance = 0;                  // +0x30
    return n;
}

CompRefNode* CompRefTreeMin(CompRefNode* n) {
    if (!n) return nullptr;
    while (n->left) n = n->left;
    return n;
}
CompRefNode* CompRefTreeSucc(CompRefNode* n) {
    if (!n) return nullptr;
    if (n->right) return CompRefTreeMin(n->right);
    CompRefNode* parent = n->parent;
    while (parent && parent->right == n) {
        n = parent;
        parent = parent->parent;
    }
    return parent;
}

// `rootField` is the tree header's root slot (CitizenBehavior +0x100 / +0x160,
// declared void*), `countField` its count.
void SerializeCompRefTree(void* rootField, int32_t* countField, IO::Serializer* serializer,
                          fn_KeyResolve_t keyFn) {
    // LEFT: the root slot is declared void* (CitizenBehavior.h, block WORK-a).
    CompRefNode** rootPtr = static_cast<CompRefNode**>(rootField);
    int32_t count = *countField;
    serializer->Serialize(count, nullptr);
    bool loading = SerializerLoading(serializer);

    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            uint64_t key = 0;
            if (keyFn) keyFn(&key, serializer);
            IbpArrayHeader val = {nullptr, 0, 0};
            SerializeIBPArray(&val.data, &val.count, serializer, ComponentEntity::TypeId());

            if (*rootPtr == nullptr) {
                *rootPtr = AllocCompRefTreeNode(key, nullptr, val);
                *countField = *countField + 1;
                continue;
            }
            CompRefNode* cur = *rootPtr;
            CompRefNode* insertedNode = nullptr;
            bool dup = false;
            for (;;) {
                if (key < cur->key) {
                    CompRefNode* left = cur->left;
                    if (!left) {
                        insertedNode = AllocCompRefTreeNode(key, cur, val);
                        cur->left = insertedNode;
                        break;
                    }
                    cur = left;
                } else if (key <= cur->key) {
                    dup = true;   // matches original: already present, skip insert + count++
                    break;
                } else {
                    CompRefNode* right = cur->right;
                    if (!right) {
                        insertedNode = AllocCompRefTreeNode(key, cur, val);
                        cur->right = insertedNode;
                        break;
                    }
                    cur = right;
                }
            }
            if (!dup) {
                *countField = *countField + 1;
                // LEFT: the shared AVL retrace takes int64_t* (Player/exe_bodies.h).
                TradeWork_impl_4b40(reinterpret_cast<int64_t*>(rootPtr),
                                     reinterpret_cast<int64_t*>(&insertedNode));
            }
        }
    } else {
        CompRefNode* root = *rootPtr;
        CompRefNode* node = CompRefTreeMin(root);
        while (node) {
            if (keyFn) keyFn(&node->key, serializer);
            SerializeIBPArray(&node->values.data, &node->values.count, serializer, ComponentEntity::TypeId());
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
struct LegacyNode {
    uint64_t    key;         // +0x00
    int32_t     value;       // +0x08 (the high dword is zeroed with it: one qword store)
    uint32_t    valueHigh;   // +0x0C
    LegacyNode* parent;      // +0x10
    LegacyNode* left;        // +0x18
    LegacyNode* right;       // +0x20
    int32_t     balance;     // +0x28
    uint8_t     _pad2C[4];   // +0x2C
};
static_assert(sizeof(LegacyNode) == 0x30, "LegacyNode is MemAlloc(0x30)");
static_assert(offsetof(LegacyNode, value) == 0x08, "LegacyNode::value @ +0x08");
static_assert(offsetof(LegacyNode, parent) == 0x10, "LegacyNode::parent @ +0x10");
static_assert(offsetof(LegacyNode, left) == 0x18, "LegacyNode::left @ +0x18");
static_assert(offsetof(LegacyNode, right) == 0x20, "LegacyNode::right @ +0x20");
static_assert(offsetof(LegacyNode, balance) == 0x28, "LegacyNode::balance @ +0x28");

LegacyNode* AllocLegacyTreeNode(uint64_t key, LegacyNode* parent, int32_t value) {
    if (!System::MemoryAllocator::_singleton) return nullptr;
    LegacyNode* n = static_cast<LegacyNode*>(System::MemoryAllocator::MemAlloc(0x30, 8));
    if (!n) return nullptr;
    n->key = key;
    n->value = value;          // +0x08 / +0x0C: the value zero-extended (H7: two stores)
    n->valueHigh = 0;
    n->parent = parent;
    n->left = nullptr; n->right = nullptr;
    n->balance = 0;
    return n;
}
LegacyNode* LegacyTreeMin(LegacyNode* n) {
    if (!n) return nullptr;
    while (n->left) n = n->left;
    return n;
}
LegacyNode* LegacyTreeSucc(LegacyNode* n) {
    if (!n) return nullptr;
    if (n->right) return LegacyTreeMin(n->right);
    LegacyNode* parent = n->parent;
    while (parent && parent->right == n) {
        n = parent;
        parent = parent->parent;
    }
    return parent;
}
void SerializeLegacyKeyIntTree(void* rootField, int32_t* countField, IO::Serializer* serializer) {
    // LEFT: the root slot is declared void* (CitizenBehavior.h, block WORK-a).
    LegacyNode** rootPtr = static_cast<LegacyNode**>(rootField);
    int32_t count = *countField;
    serializer->Serialize(count, nullptr);
    bool loading = SerializerLoading(serializer);
    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            uint64_t key = 0;
            Reimpl_ExternalPtr_Serialize_StringTable(&key, serializer);
            int32_t value = 0;
            serializer->Serialize(value, nullptr);

            if (*rootPtr == nullptr) {
                *rootPtr = AllocLegacyTreeNode(key, nullptr, value);
                *countField = *countField + 1;
                continue;
            }
            LegacyNode* cur = *rootPtr;
            LegacyNode* insertedNode = nullptr;
            bool dup = false;
            for (;;) {
                if (key < cur->key) {
                    LegacyNode* left = cur->left;
                    if (!left) { insertedNode = AllocLegacyTreeNode(key, cur, value); cur->left = insertedNode; break; }
                    cur = left;
                } else if (key <= cur->key) {
                    dup = true; break;
                } else {
                    LegacyNode* right = cur->right;
                    if (!right) { insertedNode = AllocLegacyTreeNode(key, cur, value); cur->right = insertedNode; break; }
                    cur = right;
                }
            }
            if (!dup) {
                *countField = *countField + 1;
                // LEFT: the shared AVL retrace takes int64_t* (Component/exe_bodies.h).
                CitizenAI_Reg_helper_e360(reinterpret_cast<int64_t*>(rootPtr), reinterpret_cast<int64_t*>(&insertedNode));   // 0x14000e360
            }
        }
    } else {
        LegacyNode* node = LegacyTreeMin(*rootPtr);
        while (node) {
            Reimpl_ExternalPtr_Serialize_StringTable(&node->key, serializer);
            serializer->Serialize(node->value, nullptr);
            node = LegacyTreeSucc(node);
        }
    }
}

} // namespace

// ============================================================
// CitizenBehavior::Serialize (0x14008f760)
// ============================================================
void CitizenBehavior::Serialize(IO::Stream& stream, int* /*outParam*/) {
    IO::Serializer serializer(stream, 6);
    IO::Serializer* ser = &serializer;

    const char* leg = SerializerLoading(ser) ? "LOAD" : "SAVE";
    RegistryCensus(this, leg, "enter");

    SerializeIBPArray(&field_0x00, &field_0x08, ser, ComponentEntity::TypeId());
    SerializeIBPArray(&field_0x10, &field_0x18, ser, ComponentEntity::TypeId());
    SerializeIBPArray(&field_0x30, &field_0x38, ser, ComponentEntity::TypeId());
    SerializeIBPArray(&field_0x20, &field_0x28, ser, ComponentEntity::TypeId());
    SerializeIBPArray(&healers, &healerCount, ser, HealerComponent::TypeId());
    SerializeIBPArray(&citizens, &citizenCount, ser, CitizenComponent::TypeId());

    // Unconditional TREE #1 (residence/path-graph style, string-table key).
    SerializeCompRefTree(&field_0x100, &field_0x108, ser, &Reimpl_ExternalPtr_Serialize_StringTable);   // 0x140018020

    SerializeIBPArray(&pastures, &pastureCount, ser, PastureComponent::TypeId());
    SerializeIBPArray(&cemeteries, &cemeteryCount, ser, CemeteryComponent::TypeId());
    SerializeIBPArray(&happinessComponents, &happinessComponentCount, ser, HappinessComponent::TypeId());
    SerializeIBPArray(&attendanceComponents, &attendanceComponentCount, ser, AttendanceComponent::TypeId());
    SerializeIBPArray(&livestock, &livestockCount, ser, LivestockComponent::TypeId());
    SerializeIBPArray(&herds, &herdCount, ser, HerdComponent::TypeId());

    if (ser->GetVersion() > 0) {
        SerializeIBPArray(&diseases, &diseaseCount, ser, DiseaseComponent::TypeId());
    }

    SerializeIBPArray(&populations, &populationCount, ser, PopulationComponent::TypeId());
    SerializeIBPArray(&buildComponents, &buildComponentCount, ser, BuildComponent::TypeId());

    // ExternalPtr<ComponentDescription>[] arrays, resized via Array<Ptr8>::SetSize
    // (FUN_140001bb0) then bulk-serialized via Reimpl_ExternalPtrArray_Serialize
    // (0x1400049c0), matching FUN_140001bb0(hdr,count) + FUN_1400049c0(ser,count,ptr,p4).
    {
        int32_t count = field_0x118;
        ser->Serialize(count, nullptr);
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte ExternalPtr<ComponentDescription>.
        if (SerializerLoading(ser))
            GameColl::AsArray<GameColl::Ptr8>(&field_0x110).SetSize(count);
        // LEFT: the array body takes the element base as long long (Game/exe_bodies.h,
        // serialize_externals_array.cpp).
        Reimpl_ExternalPtrArray_Serialize(ser, count, reinterpret_cast<long long>(field_0x110), nullptr);   // 0x1400049c0
    }
    {
        int32_t count = field_0x128;
        ser->Serialize(count, nullptr);
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte ExternalPtr<ComponentDescription>.
        if (SerializerLoading(ser))
            GameColl::AsArray<GameColl::Ptr8>(&field_0x120).SetSize(count);
        Reimpl_ExternalPtrArray_Serialize(ser, count, reinterpret_cast<long long>(field_0x120), nullptr);   // 0x1400049c0
    }

    // LEFT: the three clocks are declared int64_t (CitizenBehavior.h, block WORK-a).
    reinterpret_cast<System::Time*>(&field_0x190)->Serialize(*ser);   // B.3.4b bridge
    reinterpret_cast<System::Time*>(&field_0x198)->Serialize(*ser);   // B.3.4b bridge

    // "Stats" int-assoc array at +0x150.
    SerializeIntAssocArray(&field_0x150, &field_0x158, ser);

    // TREE #2 (BuildComponentRef key) on the live path (version>=2, always true
    // at version 6); legacy key/int BST otherwise (DEAD — see file header).
    if (ser->GetVersion() < 2) {
        SerializeLegacyKeyIntTree(&field_0x160, &field_0x168, ser);
    } else {
        SerializeCompRefTree(&field_0x160, &field_0x168, ser,
            &Reimpl_ExternalPtr_Serialize_StringTable);   // 0x140002340
    }
    if (ser->GetVersion() == 5) {
        // LEFT: resizes the +0x160 TREE header as an array of arrays (the tree's, dead
        // at version 6; retail sizes +0x150 to 0x47 here on a load, 0x14008fe50).
        ResizeStride0x10(&field_0x160 /*unused for v==5 clamp — mirrors 0x28 below*/, 0);
    }

    SerializeIBPArray(&wells, &wellCount, ser, WellComponent::TypeId());

    // Stride-8 int-pair array at +0x130.
    {
        int32_t count = field_0x138;
        ser->Serialize(count, nullptr);
        bool loading = SerializerLoading(ser);
        if (loading) ResizeStride08Pair(&field_0x130, count);
        int32_t* pairs = field_0x130;
        for (int32_t i = 0; i < count; ++i) {
            ser->Serialize(pairs[i * 2 + 0], nullptr);
            ser->Serialize(pairs[i * 2 + 1], nullptr);
        }
    }

    if (ser->GetVersion() > 4) {
        SerializeIntAssocArray(&field_0x140, &field_0x148, ser);
    }
    if (ser->GetVersion() == 5) {
        ResizeStride0x10(&field_0x140, 0);
    }

    if (ser->GetVersion() < 4) {
        // DEAD-PATH (version<4): legacy Time-array -> ProfessionResourceEntry /
        // SummaryEntry conversion. Never reached at our fixed version 6 — see file
        // header. Left unimplemented (no-op) rather than risk a wrong-stride write
        // into +0x170/+0x180 from an unverifiable Ghidra pointer-arithmetic reading;
        // a real pre-release save hitting this path needs a fresh Ghidra pass before
        // it can be trusted, so we intentionally stop short of guessing here.
    } else {
        int32_t profCount = professionResourceCount;
        ser->Serialize(profCount, nullptr);
        bool loading = SerializerLoading(ser);
        if (loading) ResizeStride0x20(&professionResources, profCount);
        if (profCount > 0) {
            ProfessionResourceEntry* entry = professionResources;
            for (int32_t i = 0; i < profCount; ++i) {
                ser->Serialize(entry->currentCount, nullptr);
                ser->Serialize(entry->previousCount, nullptr);
                ser->Serialize(entry->maxCount, nullptr);
                ser->Serialize(entry->field_0x0C, nullptr);
                ser->Serialize(entry->_field10, nullptr);
                // LEFT (a defect, kept: the stream must stay byte-identical): retail
                // serializes the entry's Time at +0x18 (0x14008ff9d, Time::Serialize(
                // piVar7 + 4) with piVar7 = entry + 8); the tree has always used +0x14.
                reinterpret_cast<System::Time*>(&entry->_field14)->Serialize(*ser);   // B.3.4b bridge
                ++entry;
            }
        }
        if (professionResourceCount != 0x15) {
            ResizeStride0x20(&professionResources, 0x15);
        }

        int32_t sumCount = notifyEntryCount;
        ser->Serialize(sumCount, nullptr);
        if (loading) {
            StorageComponent_DynArrayResize(&notifyEntries, sumCount);   // FUN_14000f090
        }
        if (sumCount > 0) {
            ResourceNotifyEntry* entry = notifyEntries;
            for (int32_t i = 0; i < sumCount; ++i) {
                ser->Serialize(entry->current, nullptr);
                ser->Serialize(entry->maxCapacity, nullptr);
                reinterpret_cast<System::Time*>(&entry->timer)->Serialize(*ser);   // B.3.4b bridge
                ++entry;
            }
        }
    }

    ser->Serialize(field_0x1A8, nullptr);
    reinterpret_cast<System::Time*>(&field_0x1A0)->Serialize(*ser);   // B.3.4b bridge

    if (ser->GetVersion() > 1) {
        ser->Serialize(field_0x1AC, nullptr);
    }
    if (ser->GetVersion() > 2) {
        ser->Serialize(field_0x1B0, nullptr);
    }

    RegistryCensus(this, leg, "exit ");
}

// ============================================================
// CitizenBehavior::PostLoad (0x14008f580) — the post-load pass.
// Rebuilds a spatial grid-cell map (+0x140, stride-0x10 outer array
// sized (mapWidth/8)^2) from the residence array (+0x138/+0x130) and the
// well list (+0xe0/+0xe8), inserting each entity's grid cell into the map via
// FUN_140088a80 (already ported+registered as WellComp_AddCellEntry_88a80 —
// reused here as-is, same address/signature).
// ============================================================
// DWORD_1401c79e0 — the component type-id validated inside PostLoad's inner
// scan (the "position"-like component providing cell coords at +0x28/+0x2a).
// Table 1: 0x1401c79e0 is MapComponent's cell.
static uint32_t DWORD_1401c79e0_TypeId() {
    return MapComponent::TypeId();
}

void CitizenBehavior::PostLoad() {
    if (field_0x148 >= 1) return;   // already populated

    Map* mapGrid = GameUpdateLoop::_singleton->m_game;
    int side = mapGrid->size >> 3;
    ResizeStride0x10(&field_0x140, side * side);
    std::fprintf(stderr, "[CitPostLoad] mapGrid=%p mapWidth=%d side=%d cells=%d "
            "cellMap=%p residence{ptr=%p count=%d} ent{ptr=%p count=%d}\n",
            static_cast<void*>(mapGrid), mapGrid->size, side, side * side,
            static_cast<void*>(field_0x140),
            static_cast<void*>(field_0x130),
            field_0x138,
            static_cast<void*>(wells),
            wellCount);
    std::fflush(stderr);

    int idx = 0;
    int32_t residenceCount = field_0x138;
    if (residenceCount > 0) {
        // +0x130 is the STRIDE-8 int-pair array (ResizeStride08Pair in Serialize
        // above); retail walks it with `lVar11 += 8` and reads only the low int
        // of each pair (0x14008f5c8). Indexing it as int32[] read every second
        // element from the pair's HIGH half, which is not a cell index at all —
        // the resulting x/y ran off the 32x32 cell map and AddCellEntry wrote
        // through a wild record pointer.
        int32_t* residenceCells = field_0x130;
        int mapWidth = mapGrid->size;
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
            // LEFT: the cell-map insert takes the town manager as int64_t
            // (Component/wellcomponent_funcs.h).
            WellComp_AddCellEntry_88a80(reinterpret_cast<int64_t>(this), cell % mapWidth, cell / mapWidth, idx);   // 0x140088a80
        }
    }

    idx = 0;
    int32_t entCount = wellCount;
    if (entCount > 0) {
        WellComponent** entArr = wells;
        int slot = -1;
        for (; idx < entCount; ++idx, --slot) {
            WellComponent* entIBP = entArr[idx];
            ComponentEntity* comp = entIBP->_entity;

            MapComponent* chosen = nullptr;
            int32_t compCount = comp->_componentCount;
            for (int32_t j = 0; j < compCount; ++j) {
                Object::InternalPtr<Component>* slotPtr = &comp->_components[j];
                Component* cand = slotPtr->Get();
                if (!cand) continue;
                // Slot 2 on an entity component-array element (proved Object::Base).
                uint32_t tid = cand->Type();
                if (tid == DWORD_1401c79e0_TypeId()) {
                    slotPtr = &comp->_components[j];
                    Component* obj = slotPtr->Get();
                    if (obj && ::Object::IsDerivedFromNullSafe(obj, DWORD_1401c79e0_TypeId()))
                        chosen = static_cast<MapComponent*>(obj);   // validated just above
                    break;
                }
            }

            // Retail builds an InternalPtr<MapComponent> temporary from the find.
            ::Object::InternalPtr<MapComponent> chosenRef(chosen);
            MapComponent* chosenVal = chosenRef.Get();
            int16_t cx = chosenVal ? static_cast<int16_t>(chosenVal->tileX0) : 0;
            int16_t cy = chosenVal ? static_cast<int16_t>(chosenVal->tileY0) : 0;
            WellComp_AddCellEntry_88a80(reinterpret_cast<int64_t>(this), cx, cy, slot);   // 0x140088a80
        }
    }
}
