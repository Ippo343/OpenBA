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
#include "Game/ProfessionList.h"
#include "system/String.h"
#include "runtime/types/collection/Collection.h"
#include <cstdint>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"      // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/String.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/ImageElement.h"  // X::TypeId() (B.3.9 CG2)
#include "Component/WorkPlaceComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/CitizenComponent.h"
#include "Component/CitizenDescription.h"
#include "Component/RawMaterialDescription.h"
#include "Entity/CitizenBehavior.h"
#include "Entity/ComponentEntity.h"
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// B4 DLG-d: every entry point below keeps its `void*` signature (B4 FORWARD):
// the callers are townhallui.cpp / workplaceui.cpp (DLG-e, running) and
// professionui.cpp. Each names its argument once, as the retail object below.

namespace {

// 0x14006abf0 / 0x14006aed0's argument: a rating group, the whole and the
// half icon of each child row (two Collection::Array<InternalPtr<ImageElement>>).
struct RatingGroup {
    Collection::Array<UI::Element*> whole;   // +0x00
    Collection::Array<UI::Element*> half;    // +0x10
};
static_assert(sizeof(RatingGroup) == 0x20, "rating group 0x20");

// The row's element of that name, narrowed to an ImageElement (null if not one).
UI::Element* FindImage(UI::Element* row, const char* name) {
    UI::Element* elem = row->FindElement(System::String(name)).Get();
    if (elem && ::Object::IsDerivedFromNullSafe(elem, UI::ImageElement::TypeId())) return elem;
    return nullptr;
}

} // namespace

// ===========================================================================
// 0x14006abf0 — GroupRating::Init(rating, groupElemIBP*)
// For each child row of the group element, cache the "whole" and "half"
// ImageElements (heart/star icons) as InternalPtrs.
// ===========================================================================
extern "C" void TownHall_GroupRatingInit(void* rating, void* groupIBPPtr) {
    RatingGroup* group = reinterpret_cast<RatingGroup*>(rating);
    const Object::InternalPtr<UI::Element>* groupElem =
        reinterpret_cast<const Object::InternalPtr<UI::Element>*>(groupIBPPtr);
    int childCount = groupElem->Get()->ChildCount();
    for (int i = 0; i < childCount; i++) {
        // child(i) -> FindElement("whole") -> cast ImageElement -> push
        UI::Element* whole = FindImage(groupElem->Get()->Child(i).Get(), "whole");
        group->whole.SetSize(group->whole.GetSize() + 1, whole);

        // child(i) -> FindElement("half") -> cast ImageElement -> push
        UI::Element* half = FindImage(groupElem->Get()->Child(i).Get(), "half");
        group->half.SetSize(group->half.GetSize() + 1, half);

        childCount = groupElem->Get()->ChildCount();   // faithful re-read
    }
}

// ===========================================================================
// 0x14006aed0 — GroupRating::Update(rating, value, max)
// halfSteps = round(value/max * 2N). Whole icon i shows while 2i+1 < halfSteps;
// the half icon shows only at 2i+1 == halfSteps. (DAT_140172114 = 0.5f.)
// ===========================================================================
extern "C" void TownHall_GroupRatingUpdate(void* rating, float value, float maxValue) {
    RatingGroup* group = reinterpret_cast<RatingGroup*>(rating);
    int wholeCount = group->whole.GetSize();
    int halfSteps = (int)((value / maxValue) * (float)(wholeCount * 2) + 0.5f);
    int odd = 1;
    for (int i = 0; i < wholeCount; i++, odd += 2) {
        group->whole[i]->SetFlagsOnOff((odd >= halfSteps) ? 1u : 0u, (odd < halfSteps) ? 1u : 0u);
        group->half[i]->SetFlagsOnOff((odd != halfSteps) ? 1u : 0u, (odd == halfSteps) ? 1u : 0u);
    }
}

// ===========================================================================
// 0x140015800 — profession-row array push (0x40-byte rows).
// Row: +0x00 counts, +0x08..+0x28 five element InternalPtrs, +0x30 the
// entity list. Row copy (0x140015dd0) deep-copies the entity list
// (0x140015ec0-style ptr-array clone); realloc frees every old row's list;
// shrink (0x140015e50) frees the removed rows' lists.
// ===========================================================================
namespace {

// TownHallUI's profession row (stride 0x40; TownHallUI.h, DLG-e's, names it
// ProfessionRow), as this copy reads it.
struct ProfRow {
    uint32_t          workers;          // +0x00
    uint32_t          slots;            // +0x04
    void*             elements[5];      // +0x08..+0x28 the row's element InternalPtrs
    ComponentEntity** entities;         // +0x30 Collection::Array<InternalPtr<ComponentEntity>>
    int32_t           entityCount;      // +0x38
    int32_t           entityCapacity;   // +0x3C
};
static_assert(sizeof(ProfRow) == 0x40, "profession row stride 0x40");
static_assert(offsetof(ProfRow, entities) == 0x30, "");

// The array header the caller hands over (TownHallUI +0x40).
struct ProfRowArray {
    ProfRow* data;       // +0x00
    int32_t  size;       // +0x08
    int32_t  capacity;   // +0x0C
};

void ProfRow_Copy(ProfRow* dst, const ProfRow* src) {
    dst->workers = src->workers;
    dst->slots = src->slots;
    for (int q = 0; q < 5; q++) dst->elements[q] = src->elements[q];
    dst->entities = nullptr;
    dst->entityCount = 0;
    dst->entityCapacity = 0;
    // deep-copy entity list (ResidenceComponent_helper_5ec0)
    int cnt = src->entityCount;
    dst->entityCount = cnt;
    if (cnt > 0) {
        dst->entityCapacity = cnt;
        dst->entities = static_cast<ComponentEntity**>(
            System::MemoryAllocator::MemAlloc((uint64_t)cnt << 3, 8));
        for (int i = 0; i < cnt; i++) dst->entities[i] = src->entities[i];
    }
}

// The removed row's entity list goes (0x140015e50)
void ProfRow_Free(ProfRow* row) {
    System::MemoryAllocator::MemFree(row->entities);
    row->entities = nullptr;
    row->entityCount = 0;
    row->entityCapacity = 0;
}

} // namespace

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
    ProfRowArray* rows = reinterpret_cast<ProfRowArray*>(arrayBase);
    const ProfRow* fill = reinterpret_cast<const ProfRow*>(entry);
    int oldCount = rows->size;
    if (newCount == oldCount) return;
    ProfRow* oldBuf = rows->data;
    rows->size = newCount;
    if (rows->capacity < newCount) {
        int newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) newCap = newCount * 3;
        rows->capacity = newCap;
        rows->data = static_cast<ProfRow*>(
            System::MemoryAllocator::MemAlloc((uint64_t)newCap << 6, 8));
    }
    int copyStart = (oldBuf == rows->data) ? oldCount : 0;
    for (int i = copyStart; i < newCount; i++) {
        ProfRow* dst = &rows->data[i];
        if (!dst) continue;
        const ProfRow* src = (i < oldCount) ? &oldBuf[i] : fill;
        ProfRow_Copy(dst, src);
    }
    if (oldBuf == rows->data) {
        if (newCount < oldCount) {
            // shrink in place — free removed rows' entity lists (0x140015e50)
            for (int i = newCount; i < oldCount; i++) ProfRow_Free(&oldBuf[i]);
        }
    } else {
        // realloc'd — every old row was deep-copied; free old lists + buffer
        for (int i = 0; i < oldCount; i++) ProfRow_Free(&oldBuf[i]);
        System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ===========================================================================
// 0x1400159b0 — production-limits array push (0x28-byte rows, plain copy).
// This is literally retail's Collection::Array<T>::SetSize(n, fill) (stride
// 0x28, plain copy rows, no per-element destroy) — every newly-added row
// [oldCount, newCount) is filled from the same `entry`, not just the tail,
// which is exactly rtcoll::SetSizeCore's fill loop.
// ===========================================================================
namespace {
// TownHallUI's production row (stride 0x28; TownHallUI.h's ProductionRow): five
// NumberElement InternalPtrs.
struct ProdRow { void* elements[5]; };
static_assert(sizeof(ProdRow) == 0x28, "production row stride 0x28");
} // namespace

extern "C" void TownHall_ProdArrayPush(void* arrayBase, int newCount, void* entry) {
    Collection::Array<ProdRow>* rows = reinterpret_cast<Collection::Array<ProdRow>*>(arrayBase);
    rows->SetSize(newCount, *reinterpret_cast<const ProdRow*>(entry));
}

// ===========================================================================
// 0x1400149a0 — AVL retrace for 0x28-byte nodes: key@0, parent@+0x08,
// left@+0x10, right@+0x18, balance int16@+0x20, height int16@+0x22.
// Rotations: 0x140014c70 (rotate-left), 0x140014da0 (rotate-right).
// ===========================================================================
namespace {

struct Avl28Node {
    uint64_t   key;       // +0x00
    Avl28Node* parent;    // +0x08
    Avl28Node* left;      // +0x10
    Avl28Node* right;     // +0x18
    int16_t    balance;   // +0x20
    int16_t    height;    // +0x22
    int32_t    _pad24;    // +0x24
};
static_assert(sizeof(Avl28Node) == 0x28, "AVL node 0x28");
static_assert(offsetof(Avl28Node, balance) == 0x20, "");
static_assert(offsetof(Avl28Node, height) == 0x22, "");

inline int16_t NodeH(const Avl28Node* n) { return n ? (int16_t)(n->height + 1) : 0; }

void Avl28_Refresh(Avl28Node* n) {
    if (!n) return;
    int16_t hl = NodeH(n->left);
    int16_t hr = NodeH(n->right);
    n->height = (hr < hl) ? hl : hr;
    n->balance = (int16_t)(hr - hl);
}

// 0x140014c70 — rotate-left around `n` (right child rises).
Avl28Node* Avl28_RotL(Avl28Node** root, Avl28Node* n) {
    Avl28Node* r = n->right;
    if (n == *root) *root = r;
    Avl28Node* p = n->parent;
    if (p) {
        if (p->left == n) p->left = r;
        else              p->right = r;
    }
    r->parent = p;
    Avl28Node* rl = r->left;
    n->right = rl;
    if (rl) rl->parent = n;
    r->left = n;
    n->parent = r;
    Avl28_Refresh(r->left);
    Avl28_Refresh(r->right);
    Avl28_Refresh(r);
    return r;
}

// 0x140014da0 — rotate-right around `n` (left child rises).
Avl28Node* Avl28_RotR(Avl28Node** root, Avl28Node* n) {
    Avl28Node* l = n->left;
    if (n == *root) *root = l;
    Avl28Node* p = n->parent;
    if (p) {
        if (p->left == n) p->left = l;
        else              p->right = l;
    }
    l->parent = p;
    Avl28Node* lr = l->right;
    n->left = lr;
    if (lr) lr->parent = n;
    l->right = n;
    n->parent = l;
    Avl28_Refresh(l->left);
    Avl28_Refresh(l->right);
    Avl28_Refresh(l);
    return l;
}

} // namespace

extern "C" void TownHall_AvlRebalance28(void* treeRoot, void* nodePtr) {
    Avl28Node** root = reinterpret_cast<Avl28Node**>(treeRoot);
    Avl28Node** cur = reinterpret_cast<Avl28Node**>(nodePtr);
    while (*cur != nullptr) {
        Avl28Node* n = *cur;
        Avl28Node* parent = n->parent;
        int16_t hl = NodeH(n->left);
        int16_t hr = NodeH(n->right);
        n->height = (hr < hl) ? hl : hr;
        n->balance = (int16_t)(hr - hl);
        if ((int16_t)(hr - hl) > 1) {
            Avl28Node* r = n->right;
            if (r) {
                if (r->balance < 0) { Avl28_RotR(root, r); n = *cur; }
                Avl28_RotL(root, n);
            }
        }
        n = *cur;
        if (n->balance < -1) {
            Avl28Node* l = n->left;
            if (l) {
                if (l->balance > 0) { Avl28_RotL(root, l); n = *cur; }
                Avl28_RotR(root, n);
            }
        }
        *cur = parent;
    }
}

// ===========================================================================
// 0x140090f20 — sum a town history series (CitizenBehavior +0x150) over
// [startTick, endTick] (clamped to [0, count-1]).
// ===========================================================================
extern "C" int TownHall_QueryProductionStat(void* townMgr, int seriesIdx, int startTick, int endTick) {
    const CitizenBehavior* town = reinterpret_cast<const CitizenBehavior*>(townMgr);
    const Collection::Array<int32_t>* seriesBase = town->field_0x150;
    if (!seriesBase) return 0;
    const Collection::Array<int32_t>& series = seriesBase[seriesIdx];
    const int32_t* data = series.GetData();
    int count = series.GetSize();
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

struct InvEntry {
    uint64_t                entity;   // +0x00
    RawMaterialDescription* desc;     // +0x08
    int                     value;    // +0x10
    int                     pad;      // +0x14
};
static_assert(sizeof(InvEntry) == 0x18, "inventory entry 0x18");

// System::String is a char* with the length stored as int at ptr-8.
bool SysStrLess(const System::String& a, const System::String& b) {
    const char* sa = a._data;
    const char* sb = b._data;
    int la = sa ? RtSystem::StringLength(sa) : 0;
    int lb = sb ? RtSystem::StringLength(sb) : 0;
    int n = (la < lb) ? la : lb;
    int c = (n > 0) ? memcmp(sa, sb, (size_t)n) : 0;
    if (c != 0) return c < 0;
    return la < lb;
}

// The raw material's localized name: UIString(desc +0x28 table, +0x30 key).
System::String LocalizedName(const RawMaterialDescription* desc) {
    UI::String text(::Object::ExternalPtr<UI::StringTable>(desc->_stringTable), desc->_name);
    return text.operator System::String();
}

// Localized display-name compare of two RawMaterialDescription components.
bool InvNameLess(const InvEntry* a, const InvEntry* b) {
    if (!a->desc || !b->desc) return a->desc && !b->desc ? true : false;
    System::String sysA = LocalizedName(a->desc);
    System::String sysB = LocalizedName(b->desc);
    return SysStrLess(sysA, sysB);
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
    if (count > 1) InvInsertionSort(reinterpret_cast<InvEntry*>(items), count, &InvNameLess);
}
extern "C" void TownHall_InventorySortDesc(void* items, int count, void*, void*) {
    if (count > 1) InvInsertionSort(reinterpret_cast<InvEntry*>(items), count, &InvDescLess);
}

// ===========================================================================
// 0x140091180 — CitizenList::RemoveWorker(townMgr, professionKey): demote one
// worker of the profession back to laborer. Faithful three-pass order:
//   1. an adult, non-student citizen of the profession with NO workplace
//      — the cheapest demotion;
//   2. else the profession's workplace (the town's profession tree, +0x100)
//      with the most rostered workers — pull its first rostered worker;
//   3. else any adult, non-student citizen of the profession.
// Demotion = SetWorkplace(null) then profession = laborer (ProfessionList +0x30).
// Already-laborer-and-unassigned citizens return unchanged.
// ===========================================================================
namespace {

// The town's profession tree node (0x38; CitizenBehavior +0x100): the key is
// the ExternalPtr<Profession>, compared as an unsigned qword; the value the
// profession's workplaces.
struct ProfessionTreeNode {
    uint64_t             key;              // +0x00
    ComponentEntity**    workplaces;       // +0x08 Array<InternalPtr<ComponentEntity>> data
    int32_t              workplaceCount;   // +0x10
    int32_t              _pad14;           // +0x14
    ProfessionTreeNode*  parent;           // +0x18
    ProfessionTreeNode*  left;             // +0x20
    ProfessionTreeNode*  right;            // +0x28
    int32_t              balance;          // +0x30
    int32_t              _pad34;           // +0x34
};
static_assert(sizeof(ProfessionTreeNode) == 0x38, "profession tree node 0x38");
static_assert(offsetof(ProfessionTreeNode, left) == 0x20, "");

// The workplace's WorkPlaceComponent (the tree's null entity test kept).
WorkPlaceComponent* FindWorkPlaceComp(ComponentEntity* entity) {
    if (!entity) return nullptr;
    return entity->FindComponent<WorkPlaceComponent>();
}

// An adult (age >= adultAge), not at school
bool IsWorkingAge(const CitizenComponent* cz) {
    return cz->description->adultAge <= cz->age && cz->school == nullptr;
}

// SetWorkplace(null) + profession = laborer. Returns without touching the
// citizen if it is already an unassigned laborer.
void DemoteToLaborer(CitizenComponent* citizen, Profession* laborer) {
    if (citizen->profession == laborer && citizen->workplace == nullptr) {
        return;
    }
    citizen->SetWorkplace(nullptr);   // 0x140111ee0
    citizen->profession = laborer;
}

} // namespace

extern "C" void TownHall_RemoveWorker(void* citizenList, uint64_t professionId) {
    CitizenBehavior* town = reinterpret_cast<CitizenBehavior*>(citizenList);
    if (!town || GameUpdateLoop::_singleton == nullptr) return;
    const ProfessionList* professions = GameUpdateLoop::_singleton->m_professionList;
    if (!professions) return;
    Profession* laborer = professions->field_0x30;
    // The key the callers pass: the ExternalPtr<Profession> as a qword.
    const Profession* profession = reinterpret_cast<const Profession*>(professionId);  // CAST-OK: ptr_int

    // Pass 1: idle (no-workplace) adult worker of this profession.
    int cnt = town->citizenCount;
    for (int i = 0; i < cnt; i++) {
        CitizenComponent* cz = town->citizens[i];
        if (IsWorkingAge(cz) && cz->profession == profession && cz->workplace == nullptr) {
            DemoteToLaborer(cz, laborer);
            return;
        }
    }

    // Pass 2: the profession's tree node — the workplace with the most
    // rostered workers.
    // LEFT: CitizenBehavior::field_0x100 is `void*` (GAME-a's header; its node type is not declared there).
    ProfessionTreeNode* node = reinterpret_cast<ProfessionTreeNode*>(town->field_0x100);
    ProfessionTreeNode* found = nullptr;
    while (node) {
        uint64_t k = node->key;
        if (professionId < k)      node = node->left;
        else if (professionId > k) node = node->right;
        else { found = node; break; }
    }
    if (found && found->workplaceCount > 0) {
        int wpCount = found->workplaceCount;
        ComponentEntity* bestEntity = nullptr;
        int bestWorkers = 0;
        for (int i = 0; i < wpCount; i++) {
            ComponentEntity* ent = found->workplaces[i];
            WorkPlaceComponent* comp = FindWorkPlaceComp(ent);
            if (!comp) continue;
            int workers = comp->workerCount;
            if (bestWorkers < workers) { bestEntity = ent; bestWorkers = workers; }
        }
        if (bestEntity) {
            WorkPlaceComponent* comp = FindWorkPlaceComp(bestEntity);
            if (comp && comp->workers != nullptr) {
                CitizenComponent* worker = comp->workers[0].Get();
                if (worker) { DemoteToLaborer(worker, laborer); return; }
            }
        }
    }

    // Pass 3: any adult, non-student citizen of the profession.
    cnt = town->citizenCount;
    for (int i = 0; i < cnt; i++) {
        CitizenComponent* cz = town->citizens[i];
        if (IsWorkingAge(cz) && cz->profession == profession) {
            DemoteToLaborer(cz, laborer);
            return;
        }
    }
}
