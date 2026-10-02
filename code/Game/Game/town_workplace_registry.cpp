// code/Game/Game/town_workplace_registry.cpp
//
// Phase-1 economy foundation: the TownManager per-profession workplace registry
// (townMgr+0x100 AVL BST). Faithful 1:1 port of the exe's lazy find-or-create
// node insert. This is the piece that makes spawned workplaces FINDABLE by the
// citizen employment finders (FindAvailableBuilding 0x14008d100 / FindWorkplace
// core sub_1930 0x140111930), which is the gate for citizens to get jobs and stop
// random-wandering. See ECONOMY_PLAN.md + reference_town_registry_node_create.
//
// The AVL insert cluster it depends on is ALREADY ported+registered (Keystone A):
//   0x140017230  wrapper (ResidenceComponent_impl_7230, keystone_a_restree.cpp)
//   0x1400172f0  find-or-create   0x140017460 node-alloc   0x140014b40 rebalance
//   0x140003110  growable-array 8-byte value append (ActHunt_DynArraySetCount_3110)
// TownManager (= Ghidra-mislabeled TradeComponent) = *(gameWorld+0xf0); ctor
// 0x140090a30 (already Load step 10). Node = 0x38 bytes: +0x00 key(profession id) /
// +0x08 array.data / +0x10 count / +0x14 cap / +0x18 parent / +0x20 left /
// +0x28 right / +0x30 balance.

#include "options.h"             // Opt().liveSeatShuffle
#include "Action/exe_bodies.h"    // ActHunt_DynArraySetCount_3110
#include "Component/exe_bodies.h" // ResidenceComponent_impl_7230, AttendanceComponent_GetJobHostComponent
#include "Tutorial/exe_bodies.h"  // WorkComponent_CalcMaxWorkers
#include <new>
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "object/TypeInfo.h"                 // caller batch CB6: Object::Base::Type()
#include "Component/WorkPlaceComponent.h"   // caller batch CB6: TypeId()
#include "Component/WorkComponent.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "Game/exe_bodies.h"
#include "Entity/CitizenBehavior.h"   // the town manager (GameUpdateLoop +0xF0)
#include "Entity/ComponentEntity.h"

// Called by name:
//   0x140017230 ResidenceComponent_impl_7230 -- Keystone-A insert wrapper; returns param_2
//               (&out), *out = the found/created node (Component/exe_bodies.h)
//   0x140003110 ActHunt_DynArraySetCount_3110 -- growable 8-byte-element array: set count
//               to newCount + append *elem (Action/exe_bodies.h)
//   0x140089470 TownRegistry_ShuffleProfessionList -- per-profession building-list
//               shuffle; registered (and so reached) only under Opt().liveSeatShuffle
//               (BANISHED_LIVE_SEAT_SHUFFLE): the option is tested at the call site
//   0x14012e5c0 WorkComponent_CalcMaxWorkers -- max worker slots (Tutorial/exe_bodies.h;
//               the registered, live one of the two bodies of this address)
//   0x140002e10 AttendanceComponent_GetJobHostComponent -- find the job host into an
//               IBP (Component/exe_bodies.h)

namespace {

// Scan an entity's component list (entity+0x30 = IBP array, entity+0x38 = count)
// for the first component whose GetTypeId (vtable +0x10) equals `typeId`. Mirrors
// FUN_14008d100's two inline scan loops exactly: it stops at the first GetTypeId
// match and returns it only if IsDerivedFrom(comp, typeId) also holds — otherwise
// null (it does NOT keep scanning past a GetTypeId hit). (Not FindComponent<T>:
// the tree skips a null slot; known_port_issues 14d.)
Component* FindComponentByTypeId(const ComponentEntity* entity, uint32_t typeId) {
    int i = 0;
    while (i < entity->_componentCount) {                          // count re-read (faithful)
        Component* comp = entity->_components[i].Get();
        if (comp == nullptr) { ++i; continue; }
        // Slot 2 on an entity component-array element.
        uint32_t tid = comp->Type();
        if (tid == typeId) {
            Component* compVal = entity->_components[i].Get();
            return (::Object::IsDerivedFromNullSafe(compVal, typeId)) ? compVal : nullptr;
        }
        ++i;
    }
    return nullptr;
}

// One node of CitizenBehavior's +0x100 profession tree (0x38 bytes, key
// ExternalPtr<Profession> compared as an unsigned qword; the node's value is an
// Array<InternalPtr<ComponentEntity>> of workplace buildings). The same record
// Dialog/townhall_helpers.cpp declares for its reader; CitizenBehavior::field_0x100
// is `void*` (WORK-a's header types no node), hence the one view below.
struct ProfessionTreeNode {
    uint64_t             key;              // +0x00
    ComponentEntity**    workplaces;       // +0x08 Array<InternalPtr<ComponentEntity>> data
    int32_t              workplaceCount;   // +0x10
    int32_t              workplaceCap;     // +0x14
    ProfessionTreeNode*  parent;           // +0x18
    ProfessionTreeNode*  left;             // +0x20
    ProfessionTreeNode*  right;            // +0x28
    int32_t              balance;          // +0x30
    int32_t              _pad34;           // +0x34
};
static_assert(sizeof(ProfessionTreeNode) == 0x38, "profession tree node 0x38");
static_assert(offsetof(ProfessionTreeNode, workplaces) == 0x08, "node +0x08");
static_assert(offsetof(ProfessionTreeNode, workplaceCount) == 0x10, "node +0x10");
static_assert(offsetof(ProfessionTreeNode, left) == 0x20, "node +0x20");
static_assert(offsetof(ProfessionTreeNode, right) == 0x28, "node +0x28");

// The town manager arrives as the int64_t its outside callers hold (Component,
// Dialog, starttown); the tree root is CitizenBehavior::field_0x100.
CitizenBehavior* TownOf(int64_t townMgr) {
    return reinterpret_cast<CitizenBehavior*>(townMgr);   // LEFT: int64_t town parameter of outside callers
}
ProfessionTreeNode* TreeRoot(const CitizenBehavior* town) {
    return reinterpret_cast<ProfessionTreeNode*>(town->field_0x100);   // LEFT: CitizenBehavior::field_0x100 is void* (no node type in the header)
}
} // namespace

// ============================================================
// 0x1400895a0 — ResidenceComponent_impl_95a0 = AddWorkplaceToProfessionTree.
//   param_1 = townMgr (*(gameWorld+0xf0))
//   param_2 = InternalBasePtr* holding the workplace entity value (mirrors
//             WorkPlaceComponent_OnCreate's local copy of *(WPC+0x10))
//   param_3 = professionKey (= WorkPlaceComponent+0x48 = Profession+0x48 id)
// Walk the +0x100 BST by key; find-or-create the profession node; dedup the
// workplace against the node's entity array; append if absent. Faithful 1:1.
// ============================================================
extern "C" void ResidenceComponent_impl_95a0(int64_t param_1, void* param_2, uint64_t param_3) {
    CitizenBehavior* town = TownOf(param_1);

    // BST search for the key (key<node -> left[+0x20]; key>node -> right[+0x28]; == -> found).
    ProfessionTreeNode* node = TreeRoot(town);                         // root
    ProfessionTreeNode* found = nullptr;                               // puVar3
    while (node != nullptr) {
        if (param_3 < node->key) {                     // *node = key at +0x00
            node = node->left;                                         // left (+0x20)
        } else if (param_3 <= node->key) {
            found = node;                                              // exact match
            break;
        } else {
            node = node->right;                                        // right (+0x28)
        }
    }

    // Key absent -> create the profession node (empty entity array).
    if (found == nullptr) {
        void* out = nullptr;
        int64_t emptyRec[2] = { 0, 0 };   // array record: +0x00 data / +0x08 count+cap
        uint64_t key = param_3;
        ResidenceComponent_impl_7230(reinterpret_cast<int64_t*>(&town->field_0x100),   // LEFT: 0x140017230's int64_t* root parameter (Component block's declaration)
                                     &out, &key, emptyRec);
        found = reinterpret_cast<ProfessionTreeNode*>(out);     // *out = the new node  // LEFT: 0x140017230 returns the node as void*
        if (found == nullptr) return;
    }

    // Dedup the workplace ref against the node's entity array (base +0x08, count +0x10).
    // param_2 points at the caller's InternalPtr<ComponentEntity> local.
    int count = found->workplaceCount;                // +0x10 count
    ComponentEntity** arrBase = found->workplaces;    // +0x08 array data
    ComponentEntity* const* ref = reinterpret_cast<ComponentEntity* const*>(param_2);   // LEFT: void* parameter of outside callers
    for (int i = 0; i < count; ++i) {
        if (arrBase[i] == *ref) {
            return;   // already registered — no duplicate
        }
    }

    // Append: 003110(&node.array (node+0x08), count+1, workplaceRef).
    ActHunt_DynArraySetCount_3110(&found->workplaces, count + 1, param_2);   // 0x140003110
}

// ------------------------------------------------------------
// Convenience for the standalone spawn-time registration wiring: given a live
// WorkPlaceComponent, register its owner building into the profession BST. Mirrors
// WorkPlaceComponent_OnCreate's copy of *(WPC+0x10) into a stack local before insert.
// Skips key==0 (residence-roster WorkPlaceComponents, e.g. houses — not employers).
// ------------------------------------------------------------
extern "C" void TownRegistry_RegisterWorkplace(int64_t townMgr, void* workplaceComp) {
    if (townMgr == 0 || workplaceComp == nullptr) return;
    const WorkPlaceComponent* wpc = reinterpret_cast<const WorkPlaceComponent*>(workplaceComp);   // LEFT: void* parameter of outside callers
    uint64_t key = reinterpret_cast<uint64_t>(wpc->profession);   // CAST-OK: ptr_int (the tree key is the pointer compared unsigned)
    if (key == 0) return;   // no profession -> not an employer workplace
    ComponentEntity* refLocal = wpc->_entity;
    ResidenceComponent_impl_95a0(townMgr, &refLocal, key);
}

// ============================================================
// 0x140089470 — TownRegistry_ShuffleProfessionList. Fisher-Yates shuffle of ONE
// profession node's building array, in place. FindAvailableBuilding calls it as its
// step 1, which is what makes retail's "which building gets the next worker" pick
// round-robin instead of always-the-first-registered.
//   param_1 = townMgr   param_2 = professionKey
// Walks the +0x100 BST for the key; bails if absent or count <= 1; otherwise swaps
// each slot i with System::Random::Rand(*(gw+0xd8), count). Faithful 1:1 — note the
// original's redundant `count == 1` / `count < 2` / `count < 1` early-outs, all of
// which collapse to "nothing to shuffle".
//
// NOT the same function as FUN_1400896a0 (Shuffle896a0 in citizen_employ_cascade.cpp)
// — that one shuffles the town HOUSE list for strategy 3. Different array, different
// caller.
//
// BEHAVIOUR CHANGE, therefore OPT-IN: until now this address resolved to null in
// standalone and FindAvailableBuilding silently skipped step 1, making seat order
// deterministic (always the first registered building with room). Registering it
// makes seating random, which is retail-correct but is a real, observable change to
// every seat decision. It is gated in reg_town_workplace_registry.cpp behind
// BANISHED_LIVE_SEAT_SHUFFLE and is deliberately NOT in the default-on table: unproven
// behaviour starts opt-in, and this one wants an explicit A/B before it graduates.
// ============================================================
extern "C" void TownRegistry_ShuffleProfessionList(int64_t townMgr, uint64_t professionKey) {
    if (townMgr == 0 || GameUpdateLoop::_singleton == nullptr) return;

    // BST search for the key (same walk as the reader below).
    ProfessionTreeNode* node = TreeRoot(TownOf(townMgr));
    ProfessionTreeNode* found = nullptr;
    while (node != nullptr) {
        if (professionKey < node->key)       node = node->left;   // left
        else if (professionKey <= node->key) { found = node; break; }
        else                                 node = node->right;  // right
    }
    if (found == nullptr) return;

    uint32_t count = static_cast<uint32_t>(found->workplaceCount);   // +0x10 count
    if (static_cast<int>(count) < 2) return;                 // 0/1 entries -> nothing to do

    System::Random* rng = GameUpdateLoop::_singleton->m_random;
    if (!rng) return;

    for (int64_t i = 0; i < static_cast<int>(count); ++i) {
        uint32_t j = rng->Rand(static_cast<uint32_t>(count));
        if (static_cast<uint32_t>(i) == j) continue;
        ComponentEntity** base = found->workplaces;          // +0x08 array data
        ComponentEntity* tmp = base[i];
        base[i] = base[static_cast<int>(j)];
        base[static_cast<int>(j)] = tmp;
    }
}

// ============================================================
// 0x14008d100 — FindAvailableBuilding = the READER of the townMgr+0x100 profession
//   BST. Given a professionKey, returns (in *out) an available workplace building
//   entity for that profession — one that (a) has a WorkPlaceComponent whose live
//   worker count (+0x60) is below its WorkComponent's capacity (+0x30), and
//   (b) is currently reachable. If none qualify (or the key is absent / BST empty)
//   it writes an empty InternalBasePtr into *out. Faithful 1:1 port.
//     param_1 = townMgr (*(gameWorld+0xf0))
//     param_2 = out    (InternalBasePtr* the caller receives; also the return value)
//     param_3 = professionKey
// ============================================================
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey) {

    // Type-ids resolved self-contained (do NOT read the shared .rdata DWORDs):
    // the classes' TypeId() (caller batch CB6; was a lazy by-name cache).
    const uint32_t s_tidWorkPlace = WorkPlaceComponent::TypeId();   // DWORD_1401c80e0
    const uint32_t s_tidWork      = WorkComponent::TypeId();        // DWORD_1401c8070

    // Step 1: optimize/sort the profession node's building list (OPTIONAL — guard).
    if (Opt().liveSeatShuffle) TownRegistry_ShuffleProfessionList(townMgr, professionKey);   // 0x140089470

    // Step 2: walk the +0x100 profession BST for the key (key<node -> left[+0x20];
    // key>node -> right[+0x28]; == -> found).
    ProfessionTreeNode* node = TreeRoot(TownOf(townMgr));   // root
    ProfessionTreeNode* found = nullptr;
    while (node != nullptr) {
        if (professionKey < node->key) {                  // *node = key at +0x00
            node = node->left;                            // left  (+0x20)
        } else if (professionKey <= node->key) {
            found = node;                                 // exact match
            break;
        } else {
            node = node->right;                           // right (+0x28)
        }
    }


    // Key absent -> empty result.
    if (found == nullptr) {
        new (out) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        return out;
    }

    // Step 3: scan the profession node's building array (base +0x08, count +0x10).
    int count = found->workplaceCount;                     // +0x10 count
    for (int i = 0; i < count; ++i) {
        // building entity ref (InternalBasePtr value) at node.arr[i].
        ComponentEntity* buildingIBP = found->workplaces[i];
        ComponentEntity* building = buildingIBP;
        if (building == nullptr) continue;

        // 3a: find its WorkPlaceComponent; wrap in an IBP (matches original).
        ::Object::InternalPtr<WorkPlaceComponent> wpcIBP(
            static_cast<WorkPlaceComponent*>(FindComponentByTypeId(building, s_tidWorkPlace)));
        WorkPlaceComponent* wpcPtr = wpcIBP.Get();
        if (wpcPtr == nullptr) continue;                  // defensive (never in valid data)

        int workerCount = wpcPtr->workerCount;            // WPC+0x60 = live workers

        // owning entity is the embedded IBP at WPC+0x10 — CheckPtr then deref.
        ComponentEntity* owner = wpcPtr->_entity;
        if (owner == nullptr) continue;                   // defensive

        // 3b: find the WorkComponent on the owning entity; wrap in an IBP.
        ::Object::InternalPtr<WorkComponent> wcIBP(
            static_cast<WorkComponent*>(FindComponentByTypeId(owner, s_tidWork)));
        WorkComponent* wcPtr = wcIBP.Get();
        if (wcPtr == nullptr) continue;                   // defensive

        // capacity = WorkComponent+0x30; lazily compute+cache via CalcMaxWorkers.
        int capRaw = wcPtr->workState;
        int capacity = capRaw;
        if (capacity == 0) {
            // The B4 FORWARD (its two guards before the member's body stay: known_port_issues row 80).
            capacity = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(wcPtr));   // 0x14012e5c0  // LEFT: the forwarder's uintptr_t receiver (TUT block)
            wcPtr->workState = capacity;
        }

        // 3c: room available -> reachability probe; if reachable, this is our pick.
        char accepting = -1;
        bool reachOk = false;
        if (workerCount < capacity) {
            ::Object::InternalPtr<WorkPlaceComponent> host;   // the 16-byte scratch (matches local_50)
            {
                // 0x140002e10 = FindComponent<WorkPlaceComponent> into its return buffer.
                void* reach = AttendanceComponent_GetJobHostComponent(building, &host);
                if (reach) {
                    accepting = host->isAccepting;
                    reachOk = (accepting != 0);
                }
                if (reachOk) {
                    new (out) ::Object::InternalPtr<ComponentEntity>(building);   // *out = building entity ptr
                    return out;
                }
            }
        }
    }

    // No qualifying building -> empty result.
    new (out) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    return out;
}
