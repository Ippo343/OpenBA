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

#include "engine_funcs.h"
#include <windows.h>
#include <cstdio>
#include "resolve.h"
#include <cstdint>
#include <cstdlib>   // qsort
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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

// ------------------------------------------------------------
// Lazily-resolved exe sub-helpers + dispatch-slot fn ptrs. All null-guarded at each
// call: an unregistered/absent callee turns the call into a no-op.
// ------------------------------------------------------------
typedef void* (*fn_FindComp_t)(int64_t entity, void* retIBP);       // member ABI RCX,RDX
typedef void  (*fn_ModelComp2b70_t)(int64_t comp, uint32_t arg);    // 0x1400c2b70 model refresh
typedef void  (*fn_StatusIcon_t)(int64_t comp, int flagIdx, char v);// 0x14012c3f0 status-icon toggle
typedef char  (*fn_IsMarked_t)(int64_t obj);                        // 0x14012ead0 marked-for-destroy?
typedef void  (*fn_Reserve_t)(int64_t obj);                         // 0x14012eb50 material reserve
typedef void  (*fn_Audio2c60_t)(int64_t emData, uint32_t arg);      // 0x140042c60 emitter setup

// Runtime dispatch-slot signatures (Ghidra drops trailing args on indirect slot CALLs;
// verified from the disassembly register setup).
typedef char (*fn_Vec3Eq_t)(const void* a, const void* b);                             // slot a70
typedef void (*fn_MoveEmitter_t)(void* scene, uint32_t id, const void* from, const void* to); // slot a00
typedef void (*fn_SetVol_t)(void* scene, uint32_t id, const void* pos, float vol);     // slot a08
typedef void (*fn_StopEffect_t)(void* scene, uint32_t id);                             // slot a10
typedef int  (*fn_GetCount_t)(void* scene, uint32_t id);                               // slot a18
typedef void (*fn_RemoveEmitter_t)(void* scene, uint32_t id, const void* pos);         // slot a20
typedef void (*fn_GrowArray_t)(int64_t* arrayStruct, int newCount, void* elemIBP);     // 0x140003110

fn_FindComp_t      s_fn_FindCompStorage = nullptr;  // 0x140008090 Entity_FindComponent_Storage
fn_FindComp_t      s_fn_3050            = nullptr;   // 0x140003050 find (material/work) component
fn_FindComp_t      s_fn_3f60           = nullptr;    // 0x140003f60 find model/transform component
fn_FindComp_t      s_fn_2f90           = nullptr;    // 0x140002f90 find MeshInstance component
fn_FindComp_t      s_fn_2280           = nullptr;    // 0x140002280 find audio emitter data
fn_ModelComp2b70_t s_fn_2b70           = nullptr;    // 0x1400c2b70
fn_StatusIcon_t    s_fn_2c3f0          = nullptr;    // 0x14012c3f0
fn_IsMarked_t      s_fn_ead0           = nullptr;    // 0x14012ead0
fn_Reserve_t       s_fn_eb50           = nullptr;    // 0x14012eb50
fn_Audio2c60_t     s_fn_42c60          = nullptr;    // 0x140042c60
fn_GrowArray_t     s_fn_3110           = nullptr;    // 0x140003110

fn_Vec3Eq_t        s_slot_Vec3Eq       = nullptr;    // 0x140152a70
fn_MoveEmitter_t   s_slot_MoveEmitter  = nullptr;    // 0x140152a00
fn_SetVol_t        s_slot_SetVol       = nullptr;    // 0x140152a08
fn_StopEffect_t    s_slot_StopEffect   = nullptr;    // 0x140152a10
fn_GetCount_t      s_slot_GetCount     = nullptr;    // 0x140152a18
fn_RemoveEmitter_t s_slot_RemoveEmitter= nullptr;    // 0x140152a20

const float* s_p_uvScale = nullptr;                  // g_fUVScale @ 0x14017214c

bool s_resolved = false;

template <class T>
inline T R(uintptr_t a) { return reinterpret_cast<T>(ResolveAddr(Module::Exe, a)); }

// Resolve a runtime fn ptr stored in an exe .rdata dispatch slot.
inline void* RSlot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}

void EnsureFns() {
    if (s_resolved) return;
    s_resolved = true;

    s_fn_FindCompStorage = R<fn_FindComp_t>(0x140008090);
    s_fn_3050  = R<fn_FindComp_t>(0x140003050);
    s_fn_3f60  = R<fn_FindComp_t>(0x140003f60);
    s_fn_2f90  = R<fn_FindComp_t>(0x140002f90);
    s_fn_2280  = R<fn_FindComp_t>(0x140002280);
    s_fn_2b70  = R<fn_ModelComp2b70_t>(0x1400c2b70);
    s_fn_2c3f0 = R<fn_StatusIcon_t>(0x14012c3f0);
    s_fn_ead0  = R<fn_IsMarked_t>(0x14012ead0);
    s_fn_eb50  = R<fn_Reserve_t>(0x14012eb50);
    s_fn_42c60 = R<fn_Audio2c60_t>(0x140042c60);
    s_fn_3110  = R<fn_GrowArray_t>(0x140003110);

    s_slot_Vec3Eq        = reinterpret_cast<fn_Vec3Eq_t>(RSlot(0x140152a70));
    s_slot_MoveEmitter   = reinterpret_cast<fn_MoveEmitter_t>(RSlot(0x140152a00));
    s_slot_SetVol        = reinterpret_cast<fn_SetVol_t>(RSlot(0x140152a08));
    s_slot_StopEffect    = reinterpret_cast<fn_StopEffect_t>(RSlot(0x140152a10));
    s_slot_GetCount      = reinterpret_cast<fn_GetCount_t>(RSlot(0x140152a18));
    s_slot_RemoveEmitter = reinterpret_cast<fn_RemoveEmitter_t>(RSlot(0x140152a20));

    s_p_uvScale = R<const float*>(0x14017214c);
}


// The live Audio::Scene lives at gameWorld+0x78 (disasm: MOV RAX,[0x1401c3dd8];
// MOV RDI,[RAX+0x78]). Null in standalone unless the audio scene was built.
inline void* AudioScene() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x78);
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
    int64_t ea = *reinterpret_cast<const int64_t*>(pa);
    int64_t eb = *reinterpret_cast<const int64_t*>(pb);

    int64_t a258 = *reinterpret_cast<int64_t*>(ea + 600);   // +0x258 guardian
    int a38 = *reinterpret_cast<int*>(ea + 0x38);           // +0x38 state

    int64_t b258 = *reinterpret_cast<int64_t*>(eb + 600);
    int b38 = *reinterpret_cast<int*>(eb + 0x38);

    uint32_t ub = (b38 == 1) ? 0x100u : 0u;
    uint32_t ua = (a38 == 1) ? 0x100u : 0u;
    uint32_t lhs = ub | ((b258 == 0) ? 0x1000u : 0u);
    uint32_t rhs = ((a258 == 0) ? 0x1000u : 0u) | ua;
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
    EnsureFns();
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
        if (s_fn_2f90 && s_fn_2c3f0) {
            for (int flag = 6; flag <= 7; ++flag) {
                int64_t meshIBP[2] = {0, 0};
                s_fn_2f90(*reinterpret_cast<int64_t*>(s + 0x10), meshIBP);
                if (meshIBP[0])
                    s_fn_2c3f0(meshIBP[0], flag, 0);
            }
        }
        // Model refresh (variant 3 = idle/empty).
        if (s_fn_2b70) {
            s_fn_2b70(*reinterpret_cast<int64_t*>(s + 0x10), 3);
        }
        // 3D positional audio teardown.
        if (s_fn_2280) {
            int64_t emIBP[2] = {0, 0};
            s_fn_2280(*reinterpret_cast<int64_t*>(s + 0x10), emIBP);
            int64_t emData = emIBP[0];
            void* scene = AudioScene();
            if (emData && scene) {
                int64_t desc = *reinterpret_cast<int64_t*>(emData + 0x38);
                uint32_t id = *reinterpret_cast<uint32_t*>(desc + 0x10);
                const void* pos = reinterpret_cast<const void*>(emData + 0x28);
                if (s_slot_RemoveEmitter) s_slot_RemoveEmitter(scene, id, pos);
                int remaining = s_slot_GetCount ? s_slot_GetCount(scene, id) : 0;
                if (remaining == 0 && s_slot_StopEffect) s_slot_StopEffect(scene, id);
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
    EnsureFns();
    uint8_t* s = reinterpret_cast<uint8_t*>(storageComp);

    // --- Material reserve (guarded; the found component may be null-wrapped). ---
    int64_t matIBP[2] = {0, 0};
    if (s_fn_3050) s_fn_3050(*reinterpret_cast<int64_t*>(s + 0x10), matIBP);
    if (matIBP[0] && s_fn_ead0 && s_fn_ead0(matIBP[0])) {
        if (s_fn_eb50) s_fn_eb50(matIBP[0]);
    }

    // --- Inert: building was empty -> move emitter to model position + set volume. ---
    if (CitizenRoster_CountWorkers_129ff0(storageComp) == 0) {
        int64_t emIBP[2] = {0, 0};
        if (s_fn_2280) {
            s_fn_2280(*reinterpret_cast<int64_t*>(s + 0x10), emIBP);
        }
        // Model/transform position (guarded; component may be null-wrapped).
        float pos[3] = {0.0f, 0.0f, 0.0f};
        if (s_fn_3f60) {
            int64_t mdlIBP[2] = {0, 0};
            void* r = s_fn_3f60(*reinterpret_cast<int64_t*>(s + 0x10), mdlIBP);
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
            char same = s_slot_Vec3Eq ? s_slot_Vec3Eq(pos, emPos) : 1;
            if (!same) {
                if (scene && s_slot_MoveEmitter) {
                    int nEm = *reinterpret_cast<int*>(emData + 0x40);
                    int64_t emArr = *reinterpret_cast<int64_t*>(emData + 0x38);
                    for (int i = 0; i < nEm; ++i) {
                        uint32_t id = *reinterpret_cast<uint32_t*>(emArr + static_cast<int64_t>(i) * 0x10);
                        if (id != 0) s_slot_MoveEmitter(scene, id, emPos, pos);
                    }
                }
                *reinterpret_cast<float*>(emData + 0x28) = pos[0];
                *reinterpret_cast<float*>(emData + 0x2c) = pos[1];
                *reinterpret_cast<float*>(emData + 0x30) = pos[2];
            }
            if (s_fn_42c60) s_fn_42c60(emData, 1);
            int64_t desc = *reinterpret_cast<int64_t*>(emData + 0x38);
            if (*reinterpret_cast<uint32_t*>(desc + 0x10) == 0) {
                *reinterpret_cast<uint32_t*>(desc + 0x14) = 0x3f800000;  // 1.0f
            } else {
                float uv = s_p_uvScale ? *s_p_uvScale : 1.0f;
                if (*reinterpret_cast<float*>(desc + 0x14) != uv) {
                    *reinterpret_cast<uint32_t*>(desc + 0x14) = 0x3f800000;  // 1.0f
                    if (scene && s_slot_SetVol)
                        s_slot_SetVol(scene, *reinterpret_cast<uint32_t*>(desc + 0x10),
                                      reinterpret_cast<const void*>(emData + 0x28), uv);
                }
            }
        }
    }

    // --- FAITHFUL CORE: model refresh on first worker of this profession, then append. ---
    RosterRec* rec = Roster(storageComp, slotIndex);
    if (rec->count == 0 && s_fn_2b70) {
        s_fn_2b70(*reinterpret_cast<int64_t*>(s + 0x10), 2);   // variant 2 = staffed
    }
    // Grow the slot array to count+1 and append the citizen pointer (*selfIBP).
    if (s_fn_3110)
        s_fn_3110(reinterpret_cast<int64_t*>(rec), rec->count + 1, selfIBP);
    // Re-read count (3110 updated it) and sort the slot when it holds >1 worker.
    int newCount = rec->count;
    if (newCount > 1)
        qsort(reinterpret_cast<void*>(rec->data), static_cast<size_t>(newCount), 8, &RosterCompare_9e20);
}

// ============================================================
// CitizenComponent::AssignWorkplace (0x1401163d0)
//   self         = CitizenComponent*
//   workplaceIBP = InternalBasePtr wrapping the new workplace entity (raw ptr @ [0])
//   slotIndex    = profession slot index within that workplace
// If the assignment actually changes (+0x238 or +0x240 differ): remove from the old
// workplace roster, store the new workplace+slot, add to the new roster, then apply the
// same assignment to every family member on the +0x260/+0x268 circular list.
// ============================================================
extern "C" void CitizenComponent_AssignWorkplace(void* selfV, void* workplaceIBP, int slotIndex) {
    EnsureFns();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);

    int64_t* pWorkplace = reinterpret_cast<int64_t*>(self + 0x238);  // current workplace IBP
    int32_t* pSlot = reinterpret_cast<int32_t*>(self + 0x240);
    int64_t newWp = *reinterpret_cast<int64_t*>(workplaceIBP);

    if (*pWorkplace == newWp && *pSlot == slotIndex)
        return;   // no change

    // [diag] A citizen losing its workplace is how "everyone froze after a load"
    // starts: +0x238 goes null, the AI has nothing to plan, and the town starves.
    // Only the UN-assignment is interesting, and only its CALLER identifies which
    // rule decided the job was invalid -- so report the return address and let nm
    // name it (image base 0x140000000).
    if (*pWorkplace != 0 && newWp == 0) {
        static int n = 0;
        if (n < 25) {
            ++n;
            std::fprintf(stderr, "[Unassign] #%d citizen=%p oldWp=%llx slot %d->%d "
                         "caller=%p (static 0x%llx)\n", n, selfV,
                         (unsigned long long)*pWorkplace, *pSlot, slotIndex,
                         __builtin_return_address(0),
                         (unsigned long long)((uintptr_t)__builtin_return_address(0)
                             - (uintptr_t)GetModuleHandleW(nullptr) + 0x140000000ULL));
            std::fflush(stderr);
        }
    }

    // --- Remove from the old workplace roster. ---
    if (*pWorkplace != 0) {
        int64_t storageIBP[2] = {0, 0};
        if (s_fn_FindCompStorage) s_fn_FindCompStorage(*pWorkplace, storageIBP);
        int64_t selfIBP[2] = {0, 0};
        if (fn_InternalBasePtr_Ctor) new (selfIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        // Guard: a force-assigned / partially-built workplace may lack a Storage
        // component (null-wrapped IBP); the original relies on the invariant that an
        // assigned workplace always carries one. Skip the roster op crash-safe if absent.
        if (storageIBP[0] != 0)
            CitizenComponent_helper_a550(storageIBP[0], *pSlot, selfIBP);
    }

    // --- Store the new workplace + slot. ---
    *pWorkplace = newWp;
    *pSlot = slotIndex;

    // --- Add to the new workplace roster. ---
    if (*pWorkplace != 0) {
        int64_t storageIBP[2] = {0, 0};
        if (s_fn_FindCompStorage) s_fn_FindCompStorage(*pWorkplace, storageIBP);
        int64_t selfIBP[2] = {0, 0};
        if (fn_InternalBasePtr_Ctor) new (selfIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        if (storageIBP[0] != 0)
            CitizenComponent_helper_a710(storageIBP[0], *pSlot, selfIBP);
    }

    // --- Propagate to the family/children on the +0x260 circular list. ---
    // head = *(self+0x260); each node's next = *(node+0x268); loop until next wraps to head.
    int64_t node = *reinterpret_cast<int64_t*>(self + 0x260);
    if (node != 0) {
        do {
            int64_t wpLocal = *reinterpret_cast<int64_t*>(workplaceIBP);  // fresh IBP holding the raw wp ptr
            CitizenComponent_AssignWorkplace(reinterpret_cast<void*>(node), &wpLocal, *pSlot);
            node = *reinterpret_cast<int64_t*>(node + 0x268);
        } while (node != *reinterpret_cast<int64_t*>(self + 0x260));
    }
}
