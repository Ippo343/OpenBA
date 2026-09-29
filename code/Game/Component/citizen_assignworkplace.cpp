// code/Game/Component/citizen_assignworkplace.cpp
// Phase-4 economy: faithful 1:1 port of CitizenComponent::AssignWorkplace and its
// two worker-roster helpers, so standalone mode can set a citizen's workplace
// (+0x238 / +0x240) and keep the workplace's per-profession worker roster in sync.
//
//   0x1401163d0  CitizenComponent_AssignWorkplace  (set +0x238/+0x240; re-roster; recurse family)
//   0x14012a550  CitizenComponent_helper_a550      (REMOVE worker from workplace roster)
//   0x14012a710  CitizenComponent_helper_a710      (ADD    worker to   workplace roster)
//   0x140129ff0  FUN_140129ff0                     (sum worker counts across all professions)
//   0x140129e20  CitizenComponent_helper_9e20      (qsort comparator, ported as static)
//
// DORMANT: no caller in code/Game yet (the lead wires it in a later brick). It must be
// crash-safe and byte-faithful now. The roster bookkeeping (find/compact/decrement in
// a550; grow-array append + qsort in a710; the count sum in 129ff0) is the FAITHFUL CORE
// and runs unconditionally. Every rendering/audio side effect is INERT — in standalone
// the Audio::Scene (*(gameWorld+0x78)) is often null and FindComponent can return a
// null-wrapped IBP — so those branches are guarded to no-op crash-safe, mirroring the
// established inert-branch pattern (code/Game/Component/citizencomponent.cpp roster read,
// reference_citizencomponent_field_map).
//
// Verified against Application-x64.exe disassembly (Ghidra):
//   AssignWorkplace ABI  RCX=self, RDX=workplaceIBP, R8D=slotIndex (0x1401163e2 MOV EBP,R8D)
//   a550/a710 ABI        RCX=storageComp, EDX=slotIndex, R8=selfIBP
//   Roster table         storage+0x50 -> array of {int64 data; int32 count; int32 cap} (stride 0x10)
//                        storage+0x58 = profession count (129ff0 loop bound)
//   Worker element       raw 8-byte component pointer (find stride 8; compact stride 8)
//   Citizen fields       +0x238 workplace IBP, +0x240 slotIndex, +0x260 family head,
//                        +0x268 family next, +0x258 (=600) parent/guardian
//   Dispatch slots       0x140152ab0 CheckPtr, 0x140152a90 IBP::ctor (via engine globals),
//                        0x140152a70 Vector3::operator==, 0x140152a00 MoveEmitter,
//                        0x140152a08 SetEmitterVolume, 0x140152a10 StopEffect3D,
//                        0x140152a18 GetEmitterCount, 0x140152a20 RemoveEmitter
//

#include <new>
#include "math/Math.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>   // qsort
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "audio/Audio.h"       // ENDGAME B.3.7 callers (K4)
#include "Component/exe_bodies.h"
#include "Component/CitizenComponent.h"     // B.4 W1 COMP-b: SetHome is a member
#include "Component/ResidenceComponent.h"
#include "Entity/ComponentEntity.h"          // FindComponent<T>
#include "Component/firecomponent_funcs.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_constants.h"

// ============================================================
// Roster array record: storage+0x50 points at an array of these, indexed by
// profession/slot (stride 0x10). Matches the exe's {data, count, cap} used by
// the grow-array helper 0x140003110 (data @ +0x00, count @ +0x08, cap @ +0x0C).
// ============================================================
struct RosterRec {
    int64_t data;    // +0x00  base of worker pointer array (8-byte raw component ptrs)
    int32_t count;   // +0x08  live worker count
    int32_t cap;     // +0x0C  allocated capacity
};
static_assert(sizeof(RosterRec) == 16, "RosterRec must be 16 bytes (0x10 stride)");

// Forward decls (extern "C" so the standalone registry can take their addresses).
extern "C" void CitizenComponent_AssignWorkplace(void* self, void* workplaceIBP, int slotIndex);
extern "C" void CitizenComponent_helper_a550(int64_t storageComp, int slotIndex, void* selfIBP);
extern "C" void CitizenComponent_helper_a710(int64_t storageComp, int slotIndex, void* selfIBP);
extern "C" uint64_t CitizenRoster_CountWorkers_129ff0(int64_t storageComp);

namespace {

// The live Audio::Scene lives at gameWorld+0x78 (disasm: MOV RAX,[0x1401c3dd8];
// MOV RDI,[RAX+0x78]). Null in standalone unless the audio scene was built.
inline void* AudioScene() {
    if (GameUpdateLoop::_singleton == nullptr) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton) + 0x78);
}

inline RosterRec* Roster(int64_t storageComp, int slotIndex) {
    int64_t tableBase = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(storageComp) + 0x50);
    return reinterpret_cast<RosterRec*>(tableBase + static_cast<int64_t>(slotIndex) * 0x10);
}

// ------------------------------------------------------------
// qsort comparator — faithful port of CitizenComponent_helper_9e20 (0x140129e20).
// Elements are raw 8-byte citizen-component pointers. Ranks by "has-guardian"
// (+0x258 == 0 -> +0x1000) and "is-child/education-state==1" (+0x38 == 1 -> +0x100),
// preferring guardianed adults toward the front. CheckPtr resolves each lazy ptr on a
// stack copy before the field reads (exactly as the original does).
// ------------------------------------------------------------
int RosterCompare_9e20(const void* pa, const void* pb) {
    // qsort's element pointers: each element is a CitizenComponent pointer.
    const CitizenComponent* ea = *reinterpret_cast<CitizenComponent* const*>(pa);
    const CitizenComponent* eb = *reinterpret_cast<CitizenComponent* const*>(pb);

    const CitizenComponent* a258 = ea->_parent;   // +0x258 parent
    int a38 = ea->gender;                         // +0x38 gender

    const CitizenComponent* b258 = eb->_parent;
    int b38 = eb->gender;

    uint32_t ub = (b38 == 1) ? 0x100u : 0u;
    uint32_t ua = (a38 == 1) ? 0x100u : 0u;
    uint32_t lhs = ub | ((b258 == nullptr) ? 0x1000u : 0u);
    uint32_t rhs = ((a258 == nullptr) ? 0x1000u : 0u) | ua;
    return static_cast<int>(lhs - rhs);
}

}  // namespace

// ============================================================
// FUN_140129ff0 (0x140129ff0) — sum of worker counts across every profession slot.
// storage+0x58 = profession count; storage+0x50 = roster table (stride 0x10, count @ +8).
// The exe hand-unrolls the sum 4-wide; this is the identical scalar equivalent.
// ============================================================
extern "C" uint64_t CitizenRoster_CountWorkers_129ff0(int64_t storageComp) {
    uint8_t* s = reinterpret_cast<uint8_t*>(storageComp);
    int professions = *reinterpret_cast<int*>(s + 0x58);
    int64_t table = *reinterpret_cast<int64_t*>(s + 0x50);
    uint64_t total = 0;
    for (int i = 0; i < professions; ++i)
        total += static_cast<uint32_t>(*reinterpret_cast<int*>(table + static_cast<int64_t>(i) * 0x10 + 8));
    return total;
}

// ============================================================
// CitizenComponent_helper_a550 (0x14012a550) — REMOVE a worker from a workplace roster.
//   storageComp = the workplace's Storage/Residence component
//   slotIndex   = profession slot in the roster table
//   selfIBP     = InternalBasePtr wrapping the citizen component to remove
// Roster bookkeeping (find + compact + decrement) is the faithful core and runs
// unconditionally. When the building goes fully empty, the exe tears down the status
// icon + 3D audio emitter — that whole branch is inert and guarded here.
// ============================================================
extern "C" void CitizenComponent_helper_a550(int64_t storageComp, int slotIndex, void* selfIBP) {
    uint8_t* s = reinterpret_cast<uint8_t*>(storageComp);

    // --- Find the worker's index in this profession's array (raw 8-byte ptrs). ---
    RosterRec* rec = Roster(storageComp, slotIndex);
    int idx = -1;
    {
        int count = rec->count;
        int64_t arr = rec->data;
        for (int i = 0; i < count; ++i) {
            if (*reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(i) * 8) ==
                *reinterpret_cast<int64_t*>(selfIBP)) {
                idx = i;
                break;
            }
        }
    }

    // --- Compact the array down over the removed slot, then decrement count. ---
    // (Faithful to 0x14012a5b6..603, which reloads the table pointer here.)
    rec = Roster(storageComp, slotIndex);
    int cnt = rec->count;
    // idx >= 0 is guaranteed by the caller invariant (the citizen being removed was
    // assigned to this workplace/slot, hence present in its roster). The original does
    // an out-of-bounds array[-1] shift on the assert-only "can't find item in priority
    // list" path; guarding the shift keeps that impossible path a crash-safe no-op
    // while remaining byte-identical for every real removal.
    if (idx >= 0) {
        int64_t base = rec->data;
        for (int i = idx; i + 1 < cnt; ++i) {
            int64_t* slot = reinterpret_cast<int64_t*>(base + static_cast<int64_t>(i) * 8);
            if (slot)
                *slot = *reinterpret_cast<int64_t*>(base + static_cast<int64_t>(i + 1) * 8);
        }
    }
    rec->count = cnt - 1;   // DEC dword[rec+8] — unconditional, as in the original.

    // --- Inert: building became empty -> clear status icon + tear down audio emitter. ---
    if (CitizenRoster_CountWorkers_129ff0(storageComp) == 0) {
        // Status-icon toggles (flags 6 and 7 off). MeshInstance may be null-wrapped.
        for (int flag = 6; flag <= 7; ++flag) {
            int64_t meshIBP[2] = {0, 0};
            WorkBase_FindComponent_2f90(*reinterpret_cast<uintptr_t*>(s + 0x10), meshIBP);
            if (meshIBP[0])
                CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(meshIBP[0]),
                                          static_cast<uint8_t>(flag), 0);
        }
        // Model refresh (variant 3 = idle/empty).
        ModelComponentNotify_Reimpl(*reinterpret_cast<void**>(s + 0x10), 3);
        // 3D positional audio teardown.
        {
            int64_t emIBP[2] = {0, 0};
            // PORT GAP 0x140002280 Entity::FindAudioComponent: not ported; retail calls it
            // here (entity, emIBP). The null arm leaves emIBP null, so the teardown below
            // does not run.
            int64_t emData = emIBP[0];
            void* scene = AudioScene();
            if (emData && scene) {
                int64_t desc = *reinterpret_cast<int64_t*>(emData + 0x38);
                uint32_t id = *reinterpret_cast<uint32_t*>(desc + 0x10);
                const void* pos = reinterpret_cast<const void*>(emData + 0x28);
                reinterpret_cast<Audio::Scene*>(scene)->RemoveEmitter(
                    id, *reinterpret_cast<const Vector3*>(pos));   // B.3.7 bridge
                int remaining = reinterpret_cast<Audio::Scene*>(scene)->GetEmitterCount(id);   // B.3.7 bridge
                if (remaining == 0) reinterpret_cast<Audio::Scene*>(scene)->StopEffect3D(id);   // B.3.7 bridge
                *reinterpret_cast<uint32_t*>(desc + 0x10) = 0;
            }
        }
    }
}

// ============================================================
// CitizenComponent_helper_a710 (0x14012a710) — ADD a worker to a workplace roster.
//   storageComp / slotIndex / selfIBP as above.
// Material reservation (guarded) + inert audio reposition (guarded) run first; the
// FAITHFUL CORE is: model refresh when the profession slot was empty (guarded), then
// grow-array append of the citizen ptr, then qsort the slot when it now holds >1 worker.
// ============================================================
extern "C" void CitizenComponent_helper_a710(int64_t storageComp, int slotIndex, void* selfIBP) {
    uint8_t* s = reinterpret_cast<uint8_t*>(storageComp);

    // --- Material reserve (guarded; the found component may be null-wrapped). ---
    int64_t matIBP[2] = {0, 0};
    WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(s + 0x10), matIBP);
    if (matIBP[0] && WorkComponent_CheckWorkSlot(reinterpret_cast<void*>(matIBP[0]))) {
        WorkComponent_RegisterWorkSlot(reinterpret_cast<void*>(matIBP[0]));
    }

    // --- Inert: building was empty -> move emitter to model position + set volume. ---
    if (CitizenRoster_CountWorkers_129ff0(storageComp) == 0) {
        int64_t emIBP[2] = {0, 0};
        // PORT GAP 0x140002280 Entity::FindAudioComponent: not ported; retail calls it
        // here (entity, emIBP). The null arm leaves emIBP null, so the emitter block
        // below does not run.
        // Model/transform position (guarded; component may be null-wrapped).
        float pos[3] = {0.0f, 0.0f, 0.0f};
        {
            int64_t mdlIBP[2] = {0, 0};
            void* r = ActCNR_FindComponent_3f60(*reinterpret_cast<uintptr_t*>(s + 0x10), mdlIBP);
            int64_t modelComp = r ? *reinterpret_cast<int64_t*>(r) : 0;
            if (modelComp) {
                pos[0] = *reinterpret_cast<float*>(modelComp + 0x158);
                pos[1] = *reinterpret_cast<float*>(modelComp + 0x15c);
                pos[2] = *reinterpret_cast<float*>(modelComp + 0x160);
            }
        }
        int64_t emData = emIBP[0];
        void* scene = AudioScene();
        if (emData) {
            const void* emPos = reinterpret_cast<const void*>(emData + 0x28);
            (void)emPos;
            // PORT GAP 0x140152a70 Math::Vector3::operator==: not ported; retail calls it
            // here (pos, emPos). The null arm answers "equal".
            char same = 1;
            if (!same) {
                if (scene) {
                    int nEm = *reinterpret_cast<int*>(emData + 0x40);
                    int64_t emArr = *reinterpret_cast<int64_t*>(emData + 0x38);
                    for (int i = 0; i < nEm; ++i) {
                        uint32_t id = *reinterpret_cast<uint32_t*>(emArr + static_cast<int64_t>(i) * 0x10);
                        if (id != 0)
                            reinterpret_cast<Audio::Scene*>(scene)->MoveEmitter(
                                id, *reinterpret_cast<const Vector3*>(emPos),
                                *reinterpret_cast<const Vector3*>(pos));   // B.3.7 bridge
                    }
                }
                *reinterpret_cast<float*>(emData + 0x28) = pos[0];
                *reinterpret_cast<float*>(emData + 0x2c) = pos[1];
                *reinterpret_cast<float*>(emData + 0x30) = pos[2];
            }
            FireComp_StartAudioEmitter_42c60(reinterpret_cast<void*>(emData), 1);
            int64_t desc = *reinterpret_cast<int64_t*>(emData + 0x38);
            if (*reinterpret_cast<uint32_t*>(desc + 0x10) == 0) {
                *reinterpret_cast<uint32_t*>(desc + 0x14) = 0x3f800000;  // 1.0f
            } else {
                float uv = kRdata_14017214c;   // g_fUVScale 0x14017214c
                if (*reinterpret_cast<float*>(desc + 0x14) != uv) {
                    *reinterpret_cast<uint32_t*>(desc + 0x14) = 0x3f800000;  // 1.0f
                    if (scene)
                        reinterpret_cast<Audio::Scene*>(scene)->SetEmitterVolume(
                            *reinterpret_cast<uint32_t*>(desc + 0x10),
                            *reinterpret_cast<const Vector3*>(emData + 0x28), uv);   // B.3.7 bridge
                }
            }
        }
    }

    // --- FAITHFUL CORE: model refresh on first worker of this profession, then append. ---
    RosterRec* rec = Roster(storageComp, slotIndex);
    if (rec->count == 0) {
        ModelComponentNotify_Reimpl(*reinterpret_cast<void**>(s + 0x10), 2);   // variant 2 = staffed
    }
    // Grow the slot array to count+1 and append the citizen pointer (*selfIBP).
    ActHunt_DynArraySetCount_3110(rec, rec->count + 1, selfIBP);
    // Re-read count (3110 updated it) and sort the slot when it holds >1 worker.
    int newCount = rec->count;
    if (newCount > 1)
        qsort(reinterpret_cast<void*>(rec->data), static_cast<size_t>(newCount), 8, &RosterCompare_9e20);
}

// ============================================================
// CitizenComponent::SetHome (0x1401163d0; tree and Ghidra: "AssignWorkplace")
//   home = the new home entity (the residence), slot = the household slot in it.
// If the assignment actually changes (+0x238 or +0x240 differ): remove from the old
// residence's household roster, store the new home + slot, add to the new roster, then
// apply the same assignment to every child on the +0x260/+0x268 circular ring.
// The rosters are ResidenceComponent's (a550 / a710 stay free bodies, LEFT: their
// receiver class's header is COMP-e's; int64_t receiver).
// ============================================================
void CitizenComponent::SetHome(ComponentEntity* newHome, int32_t slotIndex) {
    void* selfV = this;
    ComponentEntity** pWorkplace = &home;  // current home
    int32_t* pSlot = &homeSlot;
    ComponentEntity* newWp = newHome;

    if (*pWorkplace == newWp && *pSlot == slotIndex)
        return;   // no change

    // [diag] A citizen losing its home is how "everyone froze after a load"
    // starts: +0x238 goes null, the AI has nothing to plan, and the town starves.
    // Only the UN-assignment is interesting, and only its CALLER identifies which
    // rule decided the home was invalid -- so report the return address and let nm
    // name it (image base 0x140000000).
    if (*pWorkplace != nullptr && newWp == nullptr) {
        static int n = 0;
        if (n < 25) {
            ++n;
            std::fprintf(stderr, "[Unassign] #%d citizen=%p oldWp=%p slot %d->%d "
                         "caller=%p (static 0x%llx)\n", n, selfV,
                         (void*)*pWorkplace, *pSlot, slotIndex,
                         __builtin_return_address(0),
                         (unsigned long long)((uintptr_t)__builtin_return_address(0)
                             - (uintptr_t)GetModuleHandleW(nullptr) + 0x140000000ULL));
            std::fflush(stderr);
        }
    }

    // --- Remove from the old residence roster. ---
    if (*pWorkplace != nullptr) {
        ResidenceComponent* storage = (*pWorkplace)->FindComponent<ResidenceComponent>();
        CitizenComponent* selfIBP[2] = {this, nullptr};
        // Guard: a force-assigned / partially-built home may lack a ResidenceComponent
        // (null); the original relies on the invariant that an assigned home always
        // carries one. Skip the roster op crash-safe if absent.
        if (storage != nullptr)
            CitizenComponent_helper_a550(reinterpret_cast<int64_t>(storage), *pSlot, selfIBP);
    }

    // --- Store the new home + slot. ---
    *pWorkplace = newWp;
    *pSlot = slotIndex;

    // --- Add to the new residence roster. ---
    if (*pWorkplace != nullptr) {
        ResidenceComponent* storage = (*pWorkplace)->FindComponent<ResidenceComponent>();
        CitizenComponent* selfIBP[2] = {this, nullptr};
        if (storage != nullptr)
            CitizenComponent_helper_a710(reinterpret_cast<int64_t>(storage), *pSlot, selfIBP);
    }

    // --- Propagate to the children on the +0x260 circular ring. ---
    // head = _firstChild; each node's next = _nextSibling; loop until next wraps to head.
    CitizenComponent* node = _firstChild;
    if (node != nullptr) {
        do {
            node->SetHome(newHome, *pSlot);
            node = node->_nextSibling;
        } while (node != _firstChild);
    }
}

// B4 FORWARD: 3 outside caller(s) (Component/residencecomponent.cpp, Game/starttown.cpp,
// standalone/death_harness.cpp)
extern "C" void CitizenComponent_AssignWorkplace(void* selfV, void* workplaceIBP, int slotIndex) {
    reinterpret_cast<CitizenComponent*>(selfV)->SetHome(
        *reinterpret_cast<ComponentEntity**>(workplaceIBP), slotIndex);
}
