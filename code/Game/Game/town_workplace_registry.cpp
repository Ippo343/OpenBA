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
#include "engine_funcs.h"
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
// null (it does NOT keep scanning past a GetTypeId hit).
void* FindComponentByTypeId(int64_t entity, uint32_t typeId) {
    int i = 0;
    while (i < *reinterpret_cast<int*>(entity + 0x38)) {           // count re-read (faithful)
        void* elem = reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(entity + 0x30) + (int64_t)i * 8);
        void* comp = *reinterpret_cast<void**>(elem);
        if (comp == nullptr) { ++i; continue; }
        // Slot 2 on an entity component-array element (proved Object::Base).
        uint32_t tid = reinterpret_cast<const Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (tid == typeId) {
            void* elem2 = reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(entity + 0x30) + (int64_t)i * 8);
            void* compVal = *reinterpret_cast<void**>(elem2);
            return (::Object::IsDerivedFromNullSafe(compVal, typeId)) ? compVal : nullptr;
        }
        ++i;
    }
    return nullptr;
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

    // BST search for the key (key<node -> left[+0x20]; key>node -> right[+0x28]; == -> found).
    uint64_t* node = *reinterpret_cast<uint64_t**>(param_1 + 0x100);   // root
    uint64_t* found = nullptr;                                         // puVar3
    while (node != nullptr) {
        if (param_3 < node[0]) {                       // *node = key at +0x00
            node = reinterpret_cast<uint64_t*>(node[4]);               // left (+0x20)
        } else if (param_3 <= node[0]) {
            found = node;                                              // exact match
            break;
        } else {
            node = reinterpret_cast<uint64_t*>(node[5]);               // right (+0x28)
        }
    }

    // Key absent -> create the profession node (empty entity array).
    if (found == nullptr) {
        void* out = nullptr;
        int64_t emptyRec[2] = { 0, 0 };   // array record: +0x00 data / +0x08 count+cap
        uint64_t key = param_3;
        ResidenceComponent_impl_7230(reinterpret_cast<int64_t*>(param_1 + 0x100),
                                     &out, &key, emptyRec);
        found = reinterpret_cast<uint64_t*>(out);     // *out = the new node
        if (found == nullptr) return;
    }

    // Dedup the workplace ref against the node's entity array (base +0x08, count +0x10).
    int count = static_cast<int>(found[2]);           // +0x10 count (low dword)
    int64_t arrBase = static_cast<int64_t>(found[1]); // +0x08 array data
    for (int i = 0; i < count; ++i) {
        if (*reinterpret_cast<int64_t*>(arrBase + static_cast<int64_t>(i) * 8) ==
            *reinterpret_cast<int64_t*>(param_2)) {
            return;   // already registered — no duplicate
        }
    }

    // Append: 003110(&node.array (node+0x08), count+1, workplaceRef).
    ActHunt_DynArraySetCount_3110(reinterpret_cast<int64_t*>(found + 1), count + 1, param_2);   // 0x140003110
}

// ------------------------------------------------------------
// Convenience for the standalone spawn-time registration wiring: given a live
// WorkPlaceComponent, register its owner building into the profession BST. Mirrors
// WorkPlaceComponent_OnCreate's copy of *(WPC+0x10) into a stack local before insert.
// Skips key==0 (residence-roster WorkPlaceComponents, e.g. houses — not employers).
// ------------------------------------------------------------
extern "C" void TownRegistry_RegisterWorkplace(int64_t townMgr, void* workplaceComp) {
    if (townMgr == 0 || workplaceComp == nullptr) return;
    uint64_t key = *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(workplaceComp) + 0x48);
    if (key == 0) return;   // no profession -> not an employer workplace
    int64_t refLocal = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(workplaceComp) + 0x10);
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
    uint64_t* node = *reinterpret_cast<uint64_t**>(townMgr + 0x100);
    uint64_t* found = nullptr;
    while (node != nullptr) {
        if (professionKey < node[0])       node = reinterpret_cast<uint64_t*>(node[4]);  // left
        else if (professionKey <= node[0]) { found = node; break; }
        else                               node = reinterpret_cast<uint64_t*>(node[5]);  // right
    }
    if (found == nullptr) return;

    uint32_t count = static_cast<uint32_t>(found[2]);        // +0x10 count (low dword)
    if (static_cast<int>(count) < 2) return;                 // 0/1 entries -> nothing to do

    void* rng = GameUpdateLoop::_singleton->m_random;
    if (!rng) return;

    for (int64_t i = 0; i < static_cast<int>(count); ++i) {
        uint32_t j = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(count));   // B.3.4b bridge
        if (static_cast<uint32_t>(i) == j) continue;
        int64_t base = static_cast<int64_t>(found[1]);       // +0x08 array data
        int64_t tmp = *reinterpret_cast<int64_t*>(base + i * 8);
        *reinterpret_cast<int64_t*>(base + i * 8) =
            *reinterpret_cast<int64_t*>(base + static_cast<int64_t>(static_cast<int>(j)) * 8);
        *reinterpret_cast<int64_t*>(base + static_cast<int64_t>(static_cast<int>(j)) * 8) = tmp;
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
    uint64_t* node = *reinterpret_cast<uint64_t**>(townMgr + 0x100);   // root
    uint64_t* found = nullptr;
    while (node != nullptr) {
        if (professionKey < node[0]) {                    // *node = key at +0x00
            node = reinterpret_cast<uint64_t*>(node[4]);  // left  (+0x20)
        } else if (professionKey <= node[0]) {
            found = node;                                 // exact match
            break;
        } else {
            node = reinterpret_cast<uint64_t*>(node[5]);  // right (+0x28)
        }
    }


    // Key absent -> empty result.
    if (found == nullptr) {
        new (out) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        return out;
    }

    // Step 3: scan the profession node's building array (base +0x08, count +0x10).
    int count = static_cast<int>(found[2]);                // +0x10 count (low dword)
    for (int i = 0; i < count; ++i) {
        // building entity ref (InternalBasePtr value) at node.arr[i].
        int64_t buildingIBP = *reinterpret_cast<int64_t*>(static_cast<int64_t>(found[1]) + (int64_t)i * 8);
        int64_t building = buildingIBP;
        if (building == 0) continue;

        // 3a: find its WorkPlaceComponent; wrap in an IBP (matches original).
        void* wpc = FindComponentByTypeId(building, s_tidWorkPlace);
        int64_t wpcIBP = 0;
        new (&wpcIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wpc));   // B.3.3 bridge
        int64_t wpcPtr = wpcIBP;
        if (wpcPtr == 0) continue;                        // defensive (never in valid data)

        int workerCount = *reinterpret_cast<int*>(wpcPtr + 0x60);   // WPC+0x60 = live workers

        // owning entity is the embedded IBP at WPC+0x10 — CheckPtr then deref.
        int64_t owner = *reinterpret_cast<int64_t*>(wpcPtr + 0x10);
        if (owner == 0) continue;                         // defensive

        // 3b: find the WorkComponent on the owning entity; wrap in an IBP.
        void* wc = FindComponentByTypeId(owner, s_tidWork);
        int64_t wcIBP = 0;
        new (&wcIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wc));   // B.3.3 bridge
        int64_t wcPtr = wcIBP;
        if (wcPtr == 0) continue;                         // defensive

        // capacity = WorkComponent+0x30; lazily compute+cache via CalcMaxWorkers.
        int capRaw = *reinterpret_cast<int*>(wcPtr + 0x30);
        int capacity = capRaw;
        if (capacity == 0) {
            capacity = WorkComponent_CalcMaxWorkers(static_cast<uintptr_t>(wcPtr));   // 0x14012e5c0
            *reinterpret_cast<int*>(wcPtr + 0x30) = capacity;
        }

        // 3c: room available -> reachability probe; if reachable, this is our pick.
        char accepting = -1;
        bool reachOk = false;
        if (workerCount < capacity) {
            int64_t tmpIBP[2] = { 0, 0 };                 // 16-byte scratch IBP (matches local_50)
            {
                // 0x140002e10. The port is declared `void`; this reader reads RAX as
                // the IBP* (retail returns outIBP). The cast keeps the bits it reads
                // today (a behaviour: candidate, B3R).
                void* reach = reinterpret_cast<void* (*)(void*, void*)>(
                    &AttendanceComponent_GetJobHostComponent)(reinterpret_cast<void*>(building), tmpIBP);
                if (reach) {
                    accepting = *reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(reach) + 0x50);
                    reachOk = (accepting != 0);
                }
                if (reachOk) {
                    *reinterpret_cast<int64_t*>(out) = building;   // *out = building entity ptr
                    return out;
                }
            }
        }
    }

    // No qualifying building -> empty result.
    new (out) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    return out;
}
