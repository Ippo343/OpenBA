// code/Game/Dialog/townhall_helpers.cpp
//
// Faithful ports of the small exe helper functions the TownHallUI reimplementation
// (townhallui.cpp) reaches through engine_funcs fn_* pointers. Those pointers
// resolve NULL unless the address is registered, which left the town-hall panel's
// setup/refresh dead. reg (standalone/reg_townhallui.cpp) maps each exe address to
// these ports.
//
//   0x14006abf0  GroupRating_Init      — cache "whole"/"half" heart/star ImageElements
//   0x14006aed0  GroupRating_Update    — show N whole + optional half icon from value/max
//   0x140015800  ProfArrayPush         — growable push, 0x40-byte profession rows
//                  (0x140015dd0 row copy + deep entity-list copy, 0x140015e50 shrink free)
//   0x1400159b0  ProdArrayPush         — growable push, 0x28-byte production rows
//   0x1400149a0  AVL rebalance         — retrace for 0x28-byte nodes (parent@8 left@10
//                  right@18 bal@20 h@22), rotations 0x140014c70 (L) / 0x140014da0 (R)
//   0x140090f20  QueryProductionStat   — clamped range-sum over townMgr+0x150 series
//   0x140015200  InventorySort (asc)   — order by localized resource name
//   0x140015500  InventorySort (desc)  — order by count desc, name tie-break
//   0x140091180  CitizenList_RemoveWorker — demote one worker of a profession to laborer
//
// All decompiled + verified in Ghidra this session (rounding const DAT_140172114 = 0.5f;
// name-compare in the sort comparators is System::String::operator< over the UIString
// localized name; see the per-function notes below).

#include <new>
#include "ui/UserInterface.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Component/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"      // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/String.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/ImageElement.h"  // X::TypeId() (B.3.9 CG2)
#include "Component/WorkPlaceComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// AssignHousing (0x140111ee0) — sets citizen+0x248 (workplace roster link); ported in
// citizencomponent.cpp and registered in standalone. Same decl as citizen_employ_cascade.
class CitizenComponent;
extern void CitizenComp_AssignHousing(CitizenComponent* self, int64_t* buildingIBPptr);

namespace {

inline void IbpCtor(void* slot, void* obj) { new (slot) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge

// ---------------------------------------------------------------------------
// Growable ptr-array push, 8-byte elements (shared shape of the exe's inline
// pushes): base[0]=buf, base[8]=count(int), base[0xc]=cap(int) — exactly
// Collection::Array<uint64_t>'s {data,size,capacity} layout, so `base` is
// reached through the B.3.2 bridge rather than by hand.
// ---------------------------------------------------------------------------
void PushPtr(uint64_t* base, uint64_t value) {
    auto& arr = GameColl::ArrayAt<GameColl::Bytes<8, 8>>((void*)base, 0);
    arr.SetSize(arr.GetSize() + 1, GameColl::Bytes<8, 8>(value));
}

// Find a named child element under `parent` and cast it via IsDerivedFrom(typeId).
// Returns the raw element ptr (or null). Mirrors the exe's FindElement+cast idiom.
void* FindCastElem(void* parent, const char* name, uint32_t typeId) {
    System::String strBuf(name);
    uint64_t foundBuf[2] = {0, 0};
    ::new (static_cast<void*>(foundBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parent)->FindElement(strBuf));  // B.3.6 bridge
    void* elem = *(void**)foundBuf;
    if (elem && ::Object::IsDerivedFromNullSafe(elem, typeId)) return elem;
    return nullptr;
}

} // namespace

// ===========================================================================
// 0x14006abf0 — GroupRating::Init(rating, groupElemIBP*)
// rating (0x20 bytes): +0x00 whole buf/+0x08 cnt/+0x0c cap, +0x10 half buf/
// +0x18 cnt/+0x1c cap. For each child row of the group element, cache the
// "whole" and "half" ImageElements (heart/star icons) as InternalBasePtrs.
// ===========================================================================
extern "C" void TownHall_GroupRatingInit(void* rating, void* groupIBPPtr) {
    uint64_t* r = (uint64_t*)rating;
    int childCount = reinterpret_cast<const UI::Element*>(*(void**)groupIBPPtr)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < childCount; i++) {
        // child(i) -> FindElement("whole") -> cast ImageElement -> push
        uint64_t childBuf[2] = {0, 0};
        ::new (static_cast<void*>(childBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)groupIBPPtr)->Child(i));  // B.3.6 bridge
        void* whole = FindCastElem(*(void**)childBuf, "whole", UI::ImageElement::TypeId());
        uint64_t wholeIBP = 0;
        IbpCtor(&wholeIBP, whole);
        PushPtr(r, wholeIBP);

        // child(i) -> FindElement("half") -> cast ImageElement -> push
        uint64_t childBuf2[2] = {0, 0};
        ::new (static_cast<void*>(childBuf2)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)groupIBPPtr)->Child(i));  // B.3.6 bridge
        void* half = FindCastElem(*(void**)childBuf2, "half", UI::ImageElement::TypeId());
        uint64_t halfIBP = 0;
        IbpCtor(&halfIBP, half);
        PushPtr(r + 2, halfIBP);

        childCount = reinterpret_cast<const UI::Element*>(*(void**)groupIBPPtr)->ChildCount();  // B.3.6 bridge — faithful re-read
    }
}

// ===========================================================================
// 0x14006aed0 — GroupRating::Update(rating, value, max)
// halfSteps = round(value/max * 2N). Whole icon i shows while 2i+1 < halfSteps;
// the half icon shows only at 2i+1 == halfSteps. (DAT_140172114 = 0.5f.)
// ===========================================================================
extern "C" void TownHall_GroupRatingUpdate(void* rating, float value, float maxValue) {
    uint64_t* r = (uint64_t*)rating;
    int wholeCount = *(int*)((uintptr_t)r + 8);
    int halfSteps = (int)((value / maxValue) * (float)(wholeCount * 2) + 0.5f);
    int odd = 1;
    for (int i = 0; i < wholeCount; i++, odd += 2) {
        uint64_t* wholeSlot = (uint64_t*)(r[0] + (uintptr_t)i * 8);
        reinterpret_cast<UI::Element*>(*(void**)wholeSlot)->SetFlagsOnOff((odd >= halfSteps) ? 1u : 0u, (odd < halfSteps) ? 1u : 0u);  // B.3.6 bridge
        uint64_t* halfSlot = (uint64_t*)(r[2] + (uintptr_t)i * 8);
        reinterpret_cast<UI::Element*>(*(void**)halfSlot)->SetFlagsOnOff((odd != halfSteps) ? 1u : 0u, (odd == halfSteps) ? 1u : 0u);  // B.3.6 bridge
    }
}

// ===========================================================================
// 0x140015800 — profession-row array push (0x40-byte rows).
// Row: +0x00 counts, +0x08..+0x28 five element IBPs, +0x30 entity-list buf,
// +0x38 entity-list cnt/cap. Row copy (0x140015dd0) deep-copies the entity
// list (0x140015ec0-style ptr-array clone); realloc frees every old row's
// list; shrink (0x140015e50) frees the removed rows' lists.
// ===========================================================================
static void ProfRow_Copy(uint64_t* dst, const uint64_t* src) {
    *(uint32_t*)dst = *(const uint32_t*)src;
    *((uint32_t*)dst + 1) = *((const uint32_t*)src + 1);
    for (int q = 1; q <= 5; q++) { dst[q] = 0; dst[q] = src[q]; }
    dst[6] = 0;   // entity list buf
    dst[7] = 0;   // entity list cnt/cap
    // deep-copy entity list (ResidenceComponent_helper_5ec0)
    int cnt = (int)*(const uint32_t*)((uintptr_t)src + 0x38);
    *(uint32_t*)((uintptr_t)dst + 0x38) = (uint32_t)cnt;
    if (cnt > 0) {
        *(uint32_t*)((uintptr_t)dst + 0x3c) = (uint32_t)cnt;
        dst[6] = (uint64_t)System::MemoryAllocator::MemAlloc((uint64_t)cnt << 3, 8);
        for (int i = 0; i < cnt; i++) {
            uint64_t* d = (uint64_t*)(dst[6] + (uintptr_t)i * 8);
            *d = 0;
            *d = *(const uint64_t*)(src[6] + (uintptr_t)i * 8);
        }
    }
}

// B.3.2 NOT CONVERTED to Collection::Array<T>::SetSize. This row is byteCopy in
// the copy direction (ProfRow_Copy deep-copies the +0x30 entity list) but ALSO
// has a per-element DESTROY step: both the shrink arm and the realloc arm free
// every removed/old row's entity list before the buffer goes. Collection.h's
// banner names that combination explicitly as one C++ traits cannot produce —
// `is_trivially_copyable` implies a trivial destructor — so it needs a
// hand-derived ArraySpec (kSpec_PkgEntry / kSpec_StringExt are the Runtime-side
// examples) rather than the trait-derived blob. It stays hand-rolled until this
// array gets its own spec or its owning struct becomes a real class (§B.2).
extern "C" void TownHall_ProfArrayPush(void* arrayBase, int newCount, void* entry) {
    uint64_t* base = (uint64_t*)arrayBase;
    int oldCount = *(int*)((uintptr_t)base + 8);
    if (newCount == oldCount) return;
    void* oldBuf = (void*)base[0];
    *(int*)((uintptr_t)base + 8) = newCount;
    if (*(int*)((uintptr_t)base + 0xc) < newCount) {
        int newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) newCap = newCount * 3;
        *(int*)((uintptr_t)base + 0xc) = newCap;
        base[0] = (uint64_t)System::MemoryAllocator::MemAlloc((uint64_t)newCap << 6, 8);
    }
    int copyStart = (oldBuf == (void*)base[0]) ? oldCount : 0;
    for (int i = copyStart; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(base[0] + (uintptr_t)i * 0x40);
        if (!dst) continue;
        const uint64_t* src = (i < oldCount)
            ? (const uint64_t*)((uintptr_t)oldBuf + (uintptr_t)i * 0x40)
            : (const uint64_t*)entry;
        ProfRow_Copy(dst, src);
    }
    if (oldBuf == (void*)base[0]) {
        if (newCount < oldCount) {
            // shrink in place — free removed rows' entity lists (0x140015e50)
            for (int i = newCount; i < oldCount; i++) {
                uint64_t* row = (uint64_t*)((uintptr_t)oldBuf + (uintptr_t)i * 0x40);
                System::MemoryAllocator::MemFree((void*)row[6]);
                row[6] = 0; row[7] = 0;
            }
        }
    } else {
        // realloc'd — every old row was deep-copied; free old lists + buffer
        for (int i = 0; i < oldCount; i++) {
            uint64_t* row = (uint64_t*)((uintptr_t)oldBuf + (uintptr_t)i * 0x40);
            System::MemoryAllocator::MemFree((void*)row[6]);
            row[6] = 0; row[7] = 0;
        }
        System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ===========================================================================
// 0x1400159b0 — production-limits array push (0x28-byte rows, plain copy).
// B.3.2 bridge: this is literally retail's Collection::Array<T>::SetSize(n,
// fill) (stride 0x28, plain byte-copy rows, no per-element destroy) — every
// newly-added row [oldCount, newCount) is filled from the same `entry`, not
// just the tail, which is exactly rtcoll::SetSizeCore's fill loop.
// ===========================================================================
extern "C" void TownHall_ProdArrayPush(void* arrayBase, int newCount, void* entry) {
    auto& arr = GameColl::ArrayAt<GameColl::Bytes<0x28, 8>>(arrayBase, 0);
    arr.SetSize(newCount, *reinterpret_cast<const GameColl::Bytes<0x28, 8>*>(entry));
}

// ===========================================================================
// 0x1400149a0 — AVL retrace for 0x28-byte nodes: key@0, parent@+0x08,
// left@+0x10, right@+0x18, balance int16@+0x20, height int16@+0x22.
// Rotations: 0x140014c70 (rotate-left), 0x140014da0 (rotate-right).
// ===========================================================================
namespace {

inline int16_t NodeH(uintptr_t n) { return n ? (int16_t)(*(int16_t*)(n + 0x22) + 1) : 0; }

void Avl28_Refresh(uintptr_t n) {
    if (!n) return;
    int16_t hl = NodeH(*(uintptr_t*)(n + 0x10));
    int16_t hr = NodeH(*(uintptr_t*)(n + 0x18));
    *(int16_t*)(n + 0x22) = (hr < hl) ? hl : hr;
    *(int16_t*)(n + 0x20) = (int16_t)(hr - hl);
}

// 0x140014c70 — rotate-left around `n` (right child rises).
uintptr_t Avl28_RotL(uintptr_t* root, uintptr_t n) {
    uintptr_t r = *(uintptr_t*)(n + 0x18);
    if (n == *root) *root = r;
    uintptr_t p = *(uintptr_t*)(n + 0x08);
    if (p) {
        if (*(uintptr_t*)(p + 0x10) == n) *(uintptr_t*)(p + 0x10) = r;
        else                              *(uintptr_t*)(p + 0x18) = r;
    }
    *(uintptr_t*)(r + 0x08) = p;
    uintptr_t rl = *(uintptr_t*)(r + 0x10);
    *(uintptr_t*)(n + 0x18) = rl;
    if (rl) *(uintptr_t*)(rl + 0x08) = n;
    *(uintptr_t*)(r + 0x10) = n;
    *(uintptr_t*)(n + 0x08) = r;
    Avl28_Refresh(*(uintptr_t*)(r + 0x10));
    Avl28_Refresh(*(uintptr_t*)(r + 0x18));
    Avl28_Refresh(r);
    return r;
}

// 0x140014da0 — rotate-right around `n` (left child rises).
uintptr_t Avl28_RotR(uintptr_t* root, uintptr_t n) {
    uintptr_t l = *(uintptr_t*)(n + 0x10);
    if (n == *root) *root = l;
    uintptr_t p = *(uintptr_t*)(n + 0x08);
    if (p) {
        if (*(uintptr_t*)(p + 0x10) == n) *(uintptr_t*)(p + 0x10) = l;
        else                              *(uintptr_t*)(p + 0x18) = l;
    }
    *(uintptr_t*)(l + 0x08) = p;
    uintptr_t lr = *(uintptr_t*)(l + 0x18);
    *(uintptr_t*)(n + 0x10) = lr;
    if (lr) *(uintptr_t*)(lr + 0x08) = n;
    *(uintptr_t*)(l + 0x18) = n;
    *(uintptr_t*)(n + 0x08) = l;
    Avl28_Refresh(*(uintptr_t*)(l + 0x10));
    Avl28_Refresh(*(uintptr_t*)(l + 0x18));
    Avl28_Refresh(l);
    return l;
}

} // namespace

extern "C" void TownHall_AvlRebalance28(void* treeRoot, void* nodePtr) {
    uintptr_t* root = (uintptr_t*)treeRoot;
    uintptr_t* cur = (uintptr_t*)nodePtr;
    while (*cur != 0) {
        uintptr_t n = *cur;
        uintptr_t parent = *(uintptr_t*)(n + 0x08);
        int16_t hl = NodeH(*(uintptr_t*)(n + 0x10));
        int16_t hr = NodeH(*(uintptr_t*)(n + 0x18));
        *(int16_t*)(n + 0x22) = (hr < hl) ? hl : hr;
        *(int16_t*)(n + 0x20) = (int16_t)(hr - hl);
        if ((int16_t)(hr - hl) > 1) {
            uintptr_t r = *(uintptr_t*)(n + 0x18);
            if (r) {
                if (*(int16_t*)(r + 0x20) < 0) { Avl28_RotR(root, r); n = *cur; }
                Avl28_RotL(root, n);
            }
        }
        n = *cur;
        if (*(int16_t*)(n + 0x20) < -1) {
            uintptr_t l = *(uintptr_t*)(n + 0x10);
            if (l) {
                if (*(int16_t*)(l + 0x20) > 0) { Avl28_RotL(root, l); n = *cur; }
                Avl28_RotR(root, n);
            }
        }
        *cur = parent;
    }
}

// ===========================================================================
// 0x140090f20 — sum a townMgr+0x150 history series over [startTick, endTick]
// (clamped to [0, count-1]). Series i: +0x00 data (int*), +0x08 count.
// ===========================================================================
extern "C" int TownHall_QueryProductionStat(void* townMgr, int seriesIdx, int startTick, int endTick) {
    uintptr_t seriesBase = *(uintptr_t*)((uintptr_t)townMgr + 0x150);
    if (!seriesBase) return 0;
    uintptr_t series = seriesBase + (uintptr_t)seriesIdx * 0x10;
    int* data = *(int**)series;
    int count = (int)*(int64_t*)(series + 8);
    if (startTick < 0) startTick = 0;
    if (endTick >= count) endTick = count - 1;
    if (!data || startTick > endTick) return 0;
    int sum = 0;
    for (int i = startTick; i <= endTick; i++) sum += data[i];
    return sum;
}

// ===========================================================================
// 0x140015200 / 0x140015500 — sort the UpdateProduction display array
// (0x18-byte entries: +0x00 entity, +0x08 RawMaterialDescription*, +0x10 count).
// Comparators verified in Ghidra (operator_impl_c710 / operator_impl_c620):
//   asc:  localized name (UIString(desc+0x28 table, desc+0x30 key) -> System::String) <
//   desc: count greater-first, name ascending as tie-break
// The exe uses an introsort; ordering semantics are what matter for the UI, so a
// stable insertion sort with the same comparators is used here (N is small).
// ===========================================================================
namespace {

struct InvEntry { uint64_t entity; uintptr_t desc; int value; int pad; };

// System::String is a char* with the length stored as int at ptr-8.
bool SysStrLess(void* a, void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    int la = sa ? *(const int*)(sa - 8) : 0;
    int lb = sb ? *(const int*)(sb - 8) : 0;
    int n = (la < lb) ? la : lb;
    int c = (n > 0) ? memcmp(sa, sb, (size_t)n) : 0;
    if (c != 0) return c < 0;
    return la < lb;
}

// Localized display-name compare of two RawMaterialDescription components:
// UIString(desc+0x28 stringTable, desc+0x30 nameKey) -> System::String -> less.
bool InvNameLess(const InvEntry* a, const InvEntry* b) {
    if (!a->desc || !b->desc) return a->desc && !b->desc ? true : false;
    uint8_t uiA[16], uiB[16];
    System::String sysA, sysB;
    ::new (static_cast<void*>(uiA)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(a->desc + 0x28))), *reinterpret_cast<const System::String*>(a->desc + 0x30));  // B.3.6 bridge
    ::new (static_cast<void*>(&sysA)) System::String(reinterpret_cast<const UI::String*>(uiA)->operator System::String());  // B.3.6 bridge
    ::new (static_cast<void*>(uiB)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(b->desc + 0x28))), *reinterpret_cast<const System::String*>(b->desc + 0x30));  // B.3.6 bridge
    ::new (static_cast<void*>(&sysB)) System::String(reinterpret_cast<const UI::String*>(uiB)->operator System::String());  // B.3.6 bridge
    bool less = SysStrLess(&sysA, &sysB);
    return less;
}

typedef bool (*InvLess_t)(const InvEntry*, const InvEntry*);

void InvInsertionSort(InvEntry* items, int count, InvLess_t less) {
    for (int i = 1; i < count; i++) {
        InvEntry key = items[i];
        int j = i - 1;
        while (j >= 0 && less(&key, &items[j])) { items[j + 1] = items[j]; j--; }
        items[j + 1] = key;
    }
}

bool InvDescLess(const InvEntry* a, const InvEntry* b) {
    if (a->value > b->value) return true;
    if (a->value < b->value) return false;
    return InvNameLess(a, b);
}

} // namespace

extern "C" void TownHall_InventorySortAsc(void* items, int count, void*, void*) {
    if (count > 1) InvInsertionSort((InvEntry*)items, count, &InvNameLess);
}
extern "C" void TownHall_InventorySortDesc(void* items, int count, void*, void*) {
    if (count > 1) InvInsertionSort((InvEntry*)items, count, &InvDescLess);
}

// ===========================================================================
// 0x140091180 — CitizenList::RemoveWorker(townMgr, professionKey): demote one
// worker of the profession back to laborer. Faithful three-pass order:
//   1. an adult, non-student citizen of the profession with NO workplace
//      (+0x248 == 0) — the cheapest demotion;
//   2. else the profession's workplace (profession-AVL at townMgr+0x100) with
//      the most rostered workers (WorkPlaceComponent+0x60) — pull its first
//      rostered worker (comp+0x58[0]);
//   3. else any adult, non-student citizen of the profession.
// Demotion = AssignHousing(citizen, null) then citizen+0x1f0 = laborer
// (profList+0x30). Already-laborer-and-unassigned citizens return unchanged.
// ===========================================================================
namespace {

typedef uint32_t (*GetTypeId_t)(void*);

// Find the WorkPlaceComponent (c80e0) on a ComponentEntity (+0x30 IBP array,
// +0x38 count) — the exe's inline GetTypeId scan + IsDerivedFrom cast.
void* FindWorkPlaceComp(uintptr_t entity) {
    if (!entity) return nullptr;
    // ENDGAME B.3.9 CG2: TypeId_WorkplaceComponent binds cell 0x1401c80e0, whose
    // retail class is WorkPlaceComponent -> WorkPlaceComponent::TypeId().
    uint32_t tid = WorkPlaceComponent::TypeId();
    uintptr_t arr = *(uintptr_t*)(entity + 0x30);
    int cnt = *(int*)(entity + 0x38);
    for (int i = 0; i < cnt; i++) {
        void* slot = (void*)(arr + (uintptr_t)i * 8);
        void* comp = *(void**)slot;
        if (!comp) continue;
        uint32_t id = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (id == tid) {
            void* c = *(void**)slot;
            return ::Object::IsDerivedFromNullSafe(c, tid) ? c : nullptr;
        }
    }
    return nullptr;
}

// AssignHousing(citizen, null) + citizen+0x1f0 = laborer. Returns without
// touching the citizen if it is already an unassigned laborer.
void DemoteToLaborer(uintptr_t citizen, uintptr_t laborerKey) {
    int64_t nullIBP = 0;
    IbpCtor(&nullIBP, nullptr);
    if (*(uintptr_t*)(citizen + 0x1f0) == laborerKey &&
        (*(int64_t*)(citizen + 0x248) == nullIBP)) {
        return;
    }
    int64_t local = nullIBP;
    CitizenComp_AssignHousing((CitizenComponent*)citizen, &local);
    *(uintptr_t*)(citizen + 0x1f0) = laborerKey;
}

} // namespace

extern "C" void TownHall_RemoveWorker(void* citizenList, uint64_t professionId) {
    uintptr_t townMgr = (uintptr_t)citizenList;
    if (!townMgr || GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gw = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t profList = *(uintptr_t*)(gw + 0xb0);
    if (!profList) return;
    uintptr_t laborer = *(uintptr_t*)(profList + 0x30);

    // Pass 1: idle (no-workplace) adult worker of this profession.
    int cnt = *(int*)(townMgr + 0xa8);
    for (int i = 0; i < cnt; i++) {
        uintptr_t cz = *(uintptr_t*)(*(uintptr_t*)(townMgr + 0xa0) + (uintptr_t)i * 8);
        if (*(float*)(*(uintptr_t*)(cz + 0x288) + 0x44) <= *(float*)(cz + 0x3c) &&
            *(uintptr_t*)(cz + 0x208) == 0 &&
            (*(uint64_t*)(cz + 0x1f0) == professionId) &&
            (*(uintptr_t*)(cz + 0x248) == 0)) {
            DemoteToLaborer(cz, laborer);
            return;
        }
    }

    // Pass 2: profession-AVL node (townMgr+0x100: key@0 arr@8 cnt@0x10
    // left@0x20 right@0x28) — the workplace with the most rostered workers.
    uintptr_t node = *(uintptr_t*)(townMgr + 0x100);
    uintptr_t found = 0;
    while (node) {
        uint64_t k = *(uint64_t*)node;
        if (professionId < k)      node = *(uintptr_t*)(node + 0x20);
        else if (professionId > k) node = *(uintptr_t*)(node + 0x28);
        else { found = node; break; }
    }
    if (found && *(int*)(found + 0x10) > 0) {
        int wpCount = *(int*)(found + 0x10);
        uintptr_t bestEntity = 0;
        int bestWorkers = 0;
        for (int i = 0; i < wpCount; i++) {
            uintptr_t ent = *(uintptr_t*)(*(uintptr_t*)(found + 8) + (uintptr_t)i * 8);
            void* comp = FindWorkPlaceComp(ent);
            if (!comp) continue;
            int workers = *(int*)((uintptr_t)comp + 0x60);
            if (bestWorkers < workers) { bestEntity = ent; bestWorkers = workers; }
        }
        if (bestEntity) {
            void* comp = FindWorkPlaceComp(bestEntity);
            if (comp && *(uintptr_t*)((uintptr_t)comp + 0x58) != 0) {
                uintptr_t worker = **(uintptr_t**)((uintptr_t)comp + 0x58);
                if (worker) { DemoteToLaborer(worker, laborer); return; }
            }
        }
    }

    // Pass 3: any adult, non-student citizen of the profession.
    cnt = *(int*)(townMgr + 0xa8);
    for (int i = 0; i < cnt; i++) {
        uintptr_t cz = *(uintptr_t*)(*(uintptr_t*)(townMgr + 0xa0) + (uintptr_t)i * 8);
        if (*(float*)(*(uintptr_t*)(cz + 0x288) + 0x44) <= *(float*)(cz + 0x3c) &&
            *(uintptr_t*)(cz + 0x208) == 0 &&
            (*(uint64_t*)(cz + 0x1f0) == professionId)) {
            DemoteToLaborer(cz, laborer);
            return;
        }
    }
}
