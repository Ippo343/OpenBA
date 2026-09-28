// code/Game/Component/workplacecomponent.cpp
// Clean reimplementation of WorkPlaceComponent lifecycle methods from Application-x64.exe.
//
// WorkPlaceComponent (0x68 bytes) is a ControllerComponent subclass managing
// worker assignments to buildings.
//
// Vtable at 0x140164d30. Two non-trivial overrides:
//   Shutdown  (0x14012f120) — Component_Serialize + versioned worker array serialization
//   OnCreate  (0x14012f090) — BST lookup to register with profession list
//
// Decompiled from Application-x64.exe using Ghidra.

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include "Component/workplacecomponent_funcs.h"
#include "Component/WorkPlaceDescription.h"
#include "Component/CitizenComponent.h"
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "scene_watch.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"     // ActHunt_NotifyEntityChanged_c3470 (0x1400c3470)
#include "Game/exe_bodies.h"       // Reimpl_ExternalPtr_Serialize_StringTable (0x140018020)
#include "Player/exe_bodies.h"     // 0x140003ea0 / 0x140005780 / 0x14011e710
#include "Tutorial/exe_bodies.h"   // WorkComponent_CalcMaxWorkers (0x14012e5c0, the registered body)
#include "Work/exe_bodies.h"       // WorkBase_FindComponent_2f90 / _3050
#include "system/Value.h"          // RtSystem_Time_Ctor

// ============================================================
// Helpers
// ============================================================
// Import slot 0x140152a98 — Object::Base::IsDerivedFrom.
// FUN_140018020 Reimpl_ExternalPtr_Serialize_StringTable — serialize the +0x48 field
//   (version >= 1 path).
// FUN_140002cc0 CemeteryListErase_Reimpl — REMOVE an entity reference from a dynamic IBP
//   array (Ghidra-verified: scans base[0] for a value equal to *entityRef, shifts the
//   tail down, decrements the count at base+8). It is an UNregister, not an add.
// FUN_14012ee60 WorkPlaceComponent_OnCreateInit (below) — release every seated worker.

// ============================================================
// Reimpl of FUN_14012ee60 (WorkPlaceComponent_OnCreate_sub_ee60:
//   drain every worker, detaching its current job host and reattaching it here)
// ============================================================
// Worker InternalBasePtr array at +0x58 (count +0x60). For each worker (index 0,
// repeatedly — the helpers below remove it, shrinking the count): find its current
// job host (DWORD_1401c80e0 component on worker+0x248), detach via FUN_14012f7e0,
// then if a replacement was produced, reattach via FUN_14012fa20.
// 0x14012f7e0 WorkplaceComponent_Detach / 0x14012fa20 WorkplaceComponent_Attach (below).

// Find the first component on `host` whose GetTypeId (vtable+0x10) == typeId.
static void* WPC_FindJobHostComponent(int64_t host, uint32_t typeId) {
    int count = *(int*)(host + 0x38);
    int64_t arr = *(int64_t*)(host + 0x30);
    for (int i = 0; i < count; i++) {
        int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
        void* comp = (void*)*slot;
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (tid == typeId) {
            void* c = (void*)*slot;
            if (Object::IsDerivedFromNullSafe(c, typeId)) return c;
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Reimpl bodies for the detach/attach helpers (0x14012f7e0 / 0x14012fa20).
// Their full exe FUN_ closure became registered in the gap wave, so they are now
// closure-safe to port. Sub-helpers used:
//   FUN_140002cc0 CemeteryListErase_Reimpl (worker-list remove)
//   FUN_1400c2b70 ModelComponentNotify_Reimpl (mesh visibility/state kick)
//   FUN_140002f90 WorkBase_FindComponent_2f90 (Entity_FindComponent_MeshInstance)
//   FUN_14012c3f0 CitizenComp_FUN_14012c3f0 (mesh-instance flag setter)
//   FUN_140005780 PlayerPath_FindComponent_5780 (IBP accessor)
//     + FUN_14011e710 PlayerPath_Sub_11e710 (bool getter)
//   FUN_140003110 (worker-list append)
//     + FUN_1400c3470 ActHunt_NotifyEntityChanged_c3470 (entity flag notify)
// SetAccepting pause path (0x14012fc00): the owner's BuildComponent state finder
// (0x140003ea0 PlayerPath_FindComponent_3ea0), the two citizen workplace finders the
// eviction loop dispatches between (0x14010ffc0 CitizenComp_sub_ffc0 / 0x140110470
// CitizenComp_sub_0470), and AssignHousing (0x140111ee0 CitizenComp_AssignHousing).

// Build-time roster-only mode (set by StartTown_AssignFoundingHousing). When true,
// Attach/Detach do ONLY the occupant-roster list mutation (+0x58/+0x60) and skip the
// gameplay-time Scene mesh-visual toggle (ModelKick + FUN_14012c3f0). The founding-housing
// shim runs during the StartTown world-BUILD, but the mesh toggle calls Graphics::Scene::
// RemoveObject/AddObject — a live-gameplay operation that DEADLOCKS when driven mid-build
// (the renderer scene isn't in a removable state and the inert StatusIconComponent's
// sceneObjId is stale). Vanilla houses founders per-TICK during gameplay (scene live), so
// suppressing the visual here is faithful to *when* the toggle is meant to run; the roster
// still tracks occupants. Default false → the per-tick gameplay AssignHousing path is
// unchanged (full faithful visual).
bool g_wpcRosterOnly = false;

// Reimpl of FUN_14012f7e0 (WorkplaceComponent worker DETACH): remove the worker IBP
// from the comp's worker list (+0x58), then if the comp is in its initial state
// (+0x60==0): kick the model (mode 3), set the mesh flags 2 and 3, and — when the
// worker's job slot matches the comp's description job (desc+0x20) and the entity is
// not already culled — hide the entity's mesh (clear AddObject / set update flag).
// Faithful 1:1 port; closure fully registered.
void WorkplaceComponent_Detach(int64_t comp, void* workerIbp) {
    CemeteryListErase_Reimpl((void*)(comp + 0x58), workerIbp);   // 0x140002cc0
    // Build-time roster-only mode (see g_wpcRosterOnly): stop after the roster removal,
    // skipping the gameplay-time Scene mesh-visual toggle that deadlocks mid-build.
    if (g_wpcRosterOnly) return;
    if (*(int*)(comp + 0x60) != 0) return;

    ModelComponentNotify_Reimpl(*(void**)(comp + 0x10), 3);   // 0x1400c2b70

    // Partial-world guard (see WorkplaceComponent_Attach): skip the status-icon visual
    // toggles when the standalone entity lacks the renderable StatusIconComponent (c7e08).
    uint64_t meshIbp = 0;
    WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp);   // 0x140002f90
    if (*(int64_t**)&meshIbp) CitizenComp_FUN_14012c3f0(*(int64_t**)&meshIbp, 2, '\0');   // 0x14012c3f0

    WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp);
    if (*(int64_t**)&meshIbp) CitizenComp_FUN_14012c3f0(*(int64_t**)&meshIbp, 3, '\0');

    if (*(char*)(comp + 0x50) == '\0') return;

    uint64_t accIbp = 0;
    PlayerPath_FindComponent_5780(*(uintptr_t*)(comp + 0x10), &accIbp);   // 0x140005780
    // Partial-world guard: on an inert standalone workplace, Accessor5780 yields a null
    // accessor object; passing it to Getter_e710 dereferences null+0x10 (READ of 0x10),
    // which crashed when a citizen switched workplaces during the per-frame update (that
    // path doesn't hold g_wpcRosterOnly). A null accessor means "nothing renderable to
    // toggle", so skip the visual tail — same as g != 0 (entity already culled).
    if (accIbp == 0) return;
    // The door read AL off 0x14011e710; the body returns uint64_t — take the same byte.
    char g = (char)PlayerPath_Sub_11e710(*(uintptr_t*)&accIbp);
    if (g != '\0') return;

    int64_t jobSlot = *(int64_t*)(comp + 0x48);
    if (jobSlot == 0) return;

    void* descObj = *(void**)(comp + 0x18);
    // Guard descObj before IsDerivedFrom (Runtime IsDerivedFrom has no null-check) and
    // desc before the +0x20 deref: an inert standalone workplace can carry a null/absent
    // description. A null desc means the job can't match -> skip the visual tail.
    void* desc = (descObj && Object::IsDerivedFromNullSafe(descObj, WorkPlaceDescription::TypeId())) ? descObj : nullptr;
    if (!desc || jobSlot != *(int64_t*)((uintptr_t)desc + 0x20)) return;

    uint64_t meshIbp2 = 0;
    WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp2);
    int64_t* mesh = *(int64_t**)&meshIbp2;
    if (!mesh) return;   // partial-world guard: no StatusIconComponent renderable
    if ((*(uint32_t*)(mesh[2] + 0x40) >> 10 & 1) != 0) return;

    uint32_t* pFlagsE = (uint32_t*)(mesh + 0x1e);
    *pFlagsE |= 0x10;
    // Each branch reads mesh+0x20 FRESH right before the OR/AND (the original reads
    // *(uint*)(plVar2+4) at the assignment, NOT a value captured before the branch —
    // the add-branch's MeshInstance::Update (vtable +0x40) can mutate this flags word).
    uint32_t changed;
    // NOTE this block is retail 0x14012c3f0 INLINED, and that function is ported in TWO
    // other places as well -- CitizenComp_FUN_14012c3f0 (citizencomponent.cpp, the copy
    // reg_component_people.cpp registers) and StatusIcon_Toggle (statusiconcomponent.cpp).
    // Three copies, each historically unsafe in a DIFFERENT half; see
    // [[reference_one_retail_fn_ported_twice_diverges]]. Touch one, check all three.
    //
    // Both hazards here are closed UPSTREAM by StatusIconComponent_CtorScalars
    // (statusiconcomponent.cpp), which builds the embedded geometry and sets
    // sceneObjId(+0x100) = -1 at CONSTRUCTION, so they hold for stream-restored icons too:
    //   * the clear arm below tests `>= 0` with NO sentinel repair, so an uninitialised
    //     sceneId of 0 used to PASS it and call Scene::RemoveObject(scene, 0) on a handle
    //     the scene never issued -- the non-terminating quadtree walk (d06def5), a FREEZE;
    //   * the set arm used to AddObject a geometry whose vtable was still 0 -- the corrupt
    //     scene node behind the `EXEC of <heap>` in Scene::Clean.
    if ((int)mesh[0x1e] == 0) {
        if ((int)*(uint32_t*)(mesh + 0x20) >= 0) {
            if (g_pGameWorldSingleton)
                reinterpret_cast<Graphics::Scene*>(
                    *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60))
                    ->RemoveObject(*(uint32_t*)(mesh + 0x20));   // B.3.5 bridge
            *(uint32_t*)(mesh + 0x20) = 0xffffffff;
        }
        uint32_t old4 = *(uint32_t*)(mesh + 4);
        uint32_t new4 = old4 & 0xfffffffb;
        *(uint32_t*)(mesh + 4) = new4;
        changed = new4 ^ old4;
    } else {
        if (g_pGameSettingsPtr && *g_pGameSettingsPtr &&
            ((uint8_t*)*g_pGameSettingsPtr)[0xc] != '\0' && (int)mesh[0x20] < 0) {
            if (g_pGameWorldSingleton) {
                uint32_t id = reinterpret_cast<Graphics::Scene*>(
                    *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60))
                    ->AddObject(reinterpret_cast<Graphics::Geometry*>(mesh + 5));   // B.3.5 bridge
                *(uint32_t*)(mesh + 0x20) = id;
                SceneWatch_Note(id, (void*)(mesh + 5), "WorkPlaceComponent carry-mesh");
            }
        }
        uint64_t t = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), 0.0f);   // import slot 0x140152b00
        // (**(code**)(*mesh + 0x40))(mesh, &time): `mesh` is the COMPONENT (its
        // Geometry is mesh+0x28; +0x10 entity / +0x20 flags below), so +0x40 is
        // Component::Tick, not MeshInstance::Update.
        reinterpret_cast<Component*>(mesh)->Tick(&t);   // B.3.x bridge (slot 8)
        uint32_t old4 = *(uint32_t*)(mesh + 4);   // FRESH after Update
        uint32_t new4 = old4 | 4;
        *(uint32_t*)(mesh + 4) = new4;
        changed = new4 ^ old4;
    }
    if ((changed & 0xffff) != 0 && mesh[2] != 0) {
        ActHunt_NotifyEntityChanged_c3470((void*)mesh[2]);   // 0x1400c3470
    }
}

// Reimpl of FUN_14012fa20 (WorkplaceComponent worker ATTACH): if the comp is in its
// initial state (+0x60==0) and the worker's job slot matches the comp's description
// job (desc+0x20), kick the model (mode 2); always set the mesh flag (mode 4) and
// append the worker IBP to the worker list (+0x58, new count = +0x60+1).
// Faithful 1:1 port; closure fully registered.
void WorkplaceComponent_Attach(int64_t comp, void* workerIbp) {
    if (!g_wpcRosterOnly) {
        if (*(int*)(comp + 0x60) == 0) {
            void* descObj = *(void**)(comp + 0x18);
            int64_t jobSlot = *(int64_t*)(comp + 0x48);
            // Guard descObj before IsDerivedFrom (no null-check) and desc before the +0x20
            // deref — an inert standalone workplace may have a null description.
            void* desc = (descObj && Object::IsDerivedFromNullSafe(descObj, WorkPlaceDescription::TypeId())) ? descObj : nullptr;
            if (desc && jobSlot == *(int64_t*)((uintptr_t)desc + 0x20)) {
                ModelComponentNotify_Reimpl(*(void**)(comp + 0x10), 2);   // 0x1400c2b70
            }
        }

        uint64_t meshIbp = 0;
        WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp);   // 0x140002f90
        // Faithful-equivalent partial-world guard: the original always toggles the building's
        // status-icon mesh (FUN_14012c3f0) because every real building carries the renderable
        // StatusIconComponent (c7e08). A standalone-spawned entity may lack it (inert factory /
        // template not fully materialised) → the finder returns a null component and
        // FUN_14012c3f0 faults reading mesh+0x10. Skip only the visual toggle when absent; the
        // occupant roster append below still runs, so the house tracks its occupants regardless.
        int64_t* mesh = *(int64_t**)&meshIbp;
        if (mesh) CitizenComp_FUN_14012c3f0(mesh, 4, '\0');   // 0x14012c3f0
    }

    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>((void*)(comp + 0x58))
        .SetSizeFill(*(int*)(comp + 0x60) + 1, workerIbp);
}

// ============================================================
// Pause-path eviction: drain the roster, re-homing each worker (0x14012fc00 tail)
// ============================================================
// Retail's loop always takes roster entry [0], asks the citizen's own workplace finder
// for somewhere else to work (sub_ffc0 when the profession's +0x4c "assigned" byte is
// clear, sub_0470 when set — the same dispatch CitizenComp_helper_ff80 wraps), and
// AssignHousing's it there. AssignHousing detaches from the old workplace first, which is
// what decrements +0x60 and makes the loop terminate.
//
// TWO STANDALONE DEVIATIONS:
//  1. g_wpcRosterOnly is held across the whole loop. Without it each AssignHousing drives
//     the Detach/Attach status-icon visuals, i.e. FUN_14012c3f0 -> Graphics::Scene::
//     RemoveObject on the building's StatusIconComponent. Our components come from the
//     inert factory (zero-filled from +0x28), so a never-registered icon reads scene id 0
//     instead of -1 and RemoveObject(scene, 0) removes whichever object genuinely owns id
//     0 — the engine's list walk then spins and the game freezes in-world (the failure
//     mode fixed at source for HighlightComponent in 0f97d16). Roster-only keeps the list
//     mutation, which is all the loop's termination depends on.
//  2. A progress guard. Retail's `while (+0x60 > 0)` assumes the finder never hands back
//     THIS workplace; if it did, the count never drops. An unterminated loop HERE is an
//     in-world freeze rather than a crash — the worst failure mode this codebase has (see
//     reference_inert_component_sentinel_fields) — so it breaks out loudly instead. This
//     is the ONLY deviation left in this function; it cannot fire on well-formed state.
//
// HISTORY (2026-07-25): this loop faulted twice on live clicks — READ of 0x8, then READ of
// 0xffffffffffffffff inside AssignHousing. Neither was a bug in this port. BuildUI_Slot9
// was handing SetAccepting a StorageLocationComponent (c8070) instead of the
// WorkPlaceComponent (c80e0), because engine_funcs.h's fn_FindComponent_Work is
// MISNAMED — it is the c8070 finder. So +0x50/+0x58/+0x60 were read off the wrong object
// and "roster base == 8 with count > 0" was simply an unrelated field. Fixed at the source
// in buildui.cpp; the plausibility/entity checks that were added here to survive the
// garbage have been REMOVED rather than left as permanent scar tissue, since with the
// correct component every operand is a live object (verified live: two pause clicks, real
// citizens evicted, zero guard trips).

static void WorkPlaceComponent_EvictRoster(int64_t comp) {
    const bool savedRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    int guard = 0;
    while (*(int*)(comp + 0x60) > 0) {
        int before = *(int*)(comp + 0x60);

        int64_t* roster = *(int64_t**)(comp + 0x58);
        int64_t worker = *roster;

        // Pick the finder off the worker's profession record (+0x1f0 -> +0x4c).
        int64_t profession = *(int64_t*)(worker + 0x1f0);
        int64_t newWorkplaceIbp = 0;
        if (*(char*)(profession + 0x4c) == '\0') {
            CitizenComp_sub_ffc0(worker, &newWorkplaceIbp);   // 0x14010ffc0
        } else {
            CitizenComp_sub_0470(worker, &newWorkplaceIbp);   // 0x140110470
        }

        // Re-read the roster base — the finders above can reallocate it.
        int64_t* rosterNow = *(int64_t**)(comp + 0x58);
        CitizenComp_AssignHousing(reinterpret_cast<CitizenComponent*>(*rosterNow),
                                  &newWorkplaceIbp);   // 0x140111ee0

        if (*(int*)(comp + 0x60) >= before || ++guard > 4096) {
            std::fprintf(stderr, "[WorkPlace] pause eviction made no progress on comp=%p "
                         "(roster %d -> %d, worker=%p) — breaking to avoid a spin\n",
                         (void*)comp, before, *(int*)(comp + 0x60), (void*)worker);
            std::fflush(stderr);
            break;
        }
    }

    g_wpcRosterOnly = savedRosterOnly;
}

// ============================================================
// WorkPlaceComponent::SetAccepting — faithful port of Entity_impl_fc00 (0x14012fc00).
// ============================================================
// Retail fires this when a workplace comes online. For accepting=true it: stores the
// accepting flag (+0x50), clears the "closed"/"manually-disabled" status-icon bits (1,0),
// then — when the workplace is UNSTAFFED on its designated job (roster +0x60 == 0 &&
// jobSlot +0x48 == desc.job desc+0x20) — RAISES the "no workers" warning bit 4 (0x10) on
// the building's StatusIconComponent. This is the SOLE faithful raiser of that warning;
// once a worker is attached, WorkplaceComponent_Attach clears the bit again
// (MeshFlag(mesh,4,0)). The 2D panel (statusiconui.cpp) reads the resulting +0xF0 bitfield.
//
// The status-icon mesh is located and toggled through the SAME registered exe helpers
// Attach/Detach use — the finder Entity_FindComponent_MeshInstance (0x140002f90, keyed off
// the StatusIconComponent type id) and the flag setter FUN_14012c3f0 (0x14012c3f0). Both
// resolve in standalone (seeded by reg_economy_typeids / reg_component_people), so the
// warning is driven by the real toggle code path, not a second port. The raise's cull-guard
// and Scene Add/position/notify all live inside FUN_14012c3f0, so a plain MeshFlag(mesh,4,1)
// reproduces the original's inlined raise byte-for-byte.
//
// The accepting=false (PAUSE) path does three more things, all ported below:
//   a. raise the "closed" indicator — bit 0 when the building's BuildComponent state
//      (found via 0x140003ea0, byte +0x52 or +0x50 when +0x52 is negative) is one of
//      {0,1,8} ("(s & 0xf6) == 0 && s != 9"), bit 1 otherwise
//   b. clear the worker/warning icons — flags 2, 3 and 4
//   c. EVICT the whole roster: repeatedly take entry [0], find it somewhere else to work
//      (sub_ffc0 / sub_0470, picked on profession+0x4c) and AssignHousing it there
//
// On (a): the original inlines FUN_14012c3f0's body rather than calling it, which is why
// the decompile shows an unreachable "mask == 0" arm right after an OR — that arm is the
// generic helper's CLEAR path, dead for a SET. `mask |= 1 << !normalState` plus the tail is
// bit-for-bit FUN_14012c3f0(mesh, normalState ? 0 : 1, 1), so the call below is faithful,
// not an approximation. (Same helper the citizen paths use: it is generic over the
// StatusIconComponent layout — mask +0xf0, scene id +0x100 — not citizen-specific.)
// NB: this is not the only writer of +0x50. A raw field write lives at
// buildcomponent_state1.cpp:427 (construction-site enable, no status icon), and the
// ctor default is set in WorkPlaceComponent_FinishLiveVtable above. A grep for
// "SetAccepting" will miss both — see the block at buildcomponent_state1.cpp for why
// that matters.
void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting) {
    *(char*)(comp + 0x50) = accepting ? 1 : 0;


    // Locate the building's StatusIconComponent (the "mesh" the icon toggles hang off).
    uint64_t meshIbp = 0;
    WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp);   // 0x140002f90
    int64_t* mesh = *(int64_t**)&meshIbp;
    if (!mesh) {
        // Partial-world: no renderable icon. The eviction below is gameplay state, not
        // visuals, so it must still run on the pause path.
        if (!accepting) WorkPlaceComponent_EvictRoster(comp);
        return;
    }

    if (accepting) {
        // Clear the "closed"/"disabled" indicator bits (1 then 0) — a now-accepting
        // workplace is no longer showing the closed sign.
        CitizenComp_FUN_14012c3f0(mesh, 1, '\0');   // 0x14012c3f0
        CitizenComp_FUN_14012c3f0(mesh, 0, '\0');
    } else {
        // (a) raise the closed indicator, keyed on the owner's build state.
        uint64_t stateIbp = 0;
        PlayerPath_FindComponent_3ea0(*(uintptr_t*)(comp + 0x10), &stateIbp);   // 0x140003ea0
        // Partial-world guard: a standalone workplace can lack the BuildComponent. The
        // original derefs it unguarded; with none present treat the building as "normal"
        // (the state the start-town / force-completed buildings are actually in).
        uint8_t state = 0;
        if (stateIbp) {
            state = *(uint8_t*)(stateIbp + 0x52);
            if ((int8_t)state < 0) state = *(uint8_t*)(stateIbp + 0x50);
        }
        bool normalState = ((state & 0xf6) == 0) && (state != 9);
        CitizenComp_FUN_14012c3f0(mesh, normalState ? 0 : 1, '\x01');
    }

    if (!accepting) {
        // (b) clear the worker/warning icons. Each toggle re-finds the mesh in the
        // original; the finder is pure, so one lookup is equivalent.
        CitizenComp_FUN_14012c3f0(mesh, 2, '\0');
        CitizenComp_FUN_14012c3f0(mesh, 3, '\0');
        CitizenComp_FUN_14012c3f0(mesh, 4, '\0');
    } else if (*(int*)(comp + 0x60) == 0) {
        // Raise the "no workers" warning (bit 4) only for an UNSTAFFED workplace still on
        // its designated job (roster empty && jobSlot == desc.job) — the faithful raise.
        void* descObj = *(void**)(comp + 0x18);
        void* desc = (Object::IsDerivedFromNullSafe(descObj, WorkPlaceDescription::TypeId())) ? descObj : nullptr;
        if (desc && *(int64_t*)(comp + 0x48) == *(int64_t*)((uintptr_t)desc + 0x20)) {
            // Re-find the mesh (mirrors the original's per-call finder) and raise bit 4. The
            // cull-guard + Scene Add + positioning + entity-notify all run inside the toggle.
            meshIbp = 0;
            WorkBase_FindComponent_2f90(*(uintptr_t*)(comp + 0x10), &meshIbp);
            mesh = *(int64_t**)&meshIbp;
            if (mesh) CitizenComp_FUN_14012c3f0(mesh, 4, '\x01');
        }
    }

    // (c) evict the roster.
    if (!accepting) WorkPlaceComponent_EvictRoster(comp);
}

// Apply the OCCUPIED end-state visual for a house whose roster was filled in roster-only
// mode (g_wpcRosterOnly suppressed the Scene mesh-visual during the build-time founding
// pass — see StartTown_AssignFoundingHousing). This reproduces the visual the attach would
// have driven for the FIRST occupant: ModelKick(mode 2) — notify the building's components
// to switch to their occupied appearance (e.g. smoke/lit windows via ModelComponent
// vtable +0x60) — plus the StatusIconComponent mesh-flag toggle. It is roster-INDEPENDENT
// (forces the kick once for the now-occupied house) because by call time the roster is
// already full (+0x60 != 0), so the original's "first occupant" gate no longer holds.
// MUST be called at GAMEPLAY time (live scene), never during world-build, or Scene::Remove
// Object deadlocks. All visual calls are null-guarded for partial-world entities.
void WorkplaceComponent_ApplyOccupiedVisual(int64_t comp) {
    // ModelKick(mode 2) — notify the building's components of the occupied state, gated (as
    // the original attach is) on this WorkPlaceComponent being the residence/job the
    // description designates (jobSlot == desc+0x20). This is the faithful notify; it is
    // crash-free. NOTE on visibility: ModelComponent's notify slot (vtable +0x60 =
    // 0x140118ab0) is an EMPTY no-op even in the original exe, and the houses' other
    // visual-bearing components (AmbientEmitterComponent smoke, StatusIconComponent icon)
    // are INERT in standalone (spawned but never scene-registered), so this notify produces
    // no visible change yet — it's correct scaffolding awaiting those components' bring-up.
    void* descObj = *(void**)(comp + 0x18);
    int64_t jobSlot = *(int64_t*)(comp + 0x48);
    void* desc = (Object::IsDerivedFromNullSafe(descObj, WorkPlaceDescription::TypeId())) ? descObj : nullptr;
    if (desc && jobSlot == *(int64_t*)((uintptr_t)desc + 0x20)) {
        ModelComponentNotify_Reimpl(*(void**)(comp + 0x10), 2);   // 0x1400c2b70
    }

    // The StatusIconComponent mesh-flag toggle (FUN_14012c3f0) is DELIBERATELY NOT driven
    // here. It clears the icon flag → flags==0 → Graphics::Scene::RemoveObject(sceneObjId).
    // In standalone the inert StatusIconComponent was never scene-registered, so its mesh
    // sceneObjId (+0x20) holds an uninitialised non-negative value; RemoveObject then SPINS
    // forever on a scene-object id that isn't in the scene (verified by gdb stack-sampling a
    // hung run, even at gameplay frame 3). The original is safe only because the icon is a
    // real registered scene object. Re-enable once StatusIconComponent::OnAdded scene-
    // registers the icon (a rendering follow-up) — the icon is already invisible while inert,
    // so skipping its "hide" toggle changes nothing observable.
}

void WorkPlaceComponent_OnCreateInit(void* selfP) {    uintptr_t self = (uintptr_t)selfP;


    while (*(int*)(self + 0x60) > 0) {
        // Termination depends on the detach below removing worker[0] from this
        // component's array (+0x58 / count +0x60) — which it only does when the
        // worker's own job-host back-pointer (+0x248) still names this workplace.
        // Retail holds that invariant; standalone seats workers through several
        // paths (WorkPlaceUI spinners, the builder pool auto-seat), so a stale
        // back-pointer would spin here forever. A HANG is far worse than a leaked
        // roster slot, so drop the entry ourselves if an iteration made no progress.
        const int countBefore = *(int*)(self + 0x60);

        int64_t emptyIbp = 0;
        new (&emptyIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        int64_t* workerSlot = *(int64_t**)(self + 0x58);
        int64_t worker = *workerSlot;
        int64_t* pJobHost = (int64_t*)(worker + 0x248);

        if (*pJobHost != 0) {
            int64_t host = *pJobHost;
            void* comp = WPC_FindJobHostComponent(host, WorkPlaceComponent::TypeId());
            int64_t compIbp = 0;
            new (&compIbp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
            int64_t workerIbp = 0;
            new (&workerIbp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)worker));   // B.3.3 bridge
            WorkplaceComponent_Detach(compIbp, &workerIbp);   // 0x14012f7e0
        }

        *pJobHost = emptyIbp;
        if (emptyIbp != 0) {
            int64_t host = *pJobHost;
            void* comp = WPC_FindJobHostComponent(host, WorkPlaceComponent::TypeId());
            int64_t compIbp = 0;
            new (&compIbp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
            int64_t workerIbp = 0;
            new (&workerIbp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)worker));   // B.3.3 bridge
            WorkplaceComponent_Attach(compIbp, &workerIbp);   // 0x14012fa20
        }

        if (*(int*)(self + 0x60) >= countBefore) {
            // No progress — shift worker[0] out by hand and keep going.
            int n = *(int*)(self + 0x60);
            int64_t* arr = *(int64_t**)(self + 0x58);
            if (arr) for (int i = 1; i < n; ++i) arr[i - 1] = arr[i];
            *(int*)(self + 0x60) = n - 1;
            static int s_reported = 0;
            if (s_reported++ < 10) {
                printf("[WorkPlaceComponent] release: worker[0]=%p did not detach "
                            "(its +0x248 job host does not name this workplace %p); "
                            "dropped it by hand, roster %d -> %d\n",
                            (void*)worker, selfP, n, n - 1);
            }
        }
    }
}

// town_workplace_registry.cpp — 0x1400895a0, add the owner entity to the profession
// bucket for `key` in the town BST (townMgr+0x100), creating the bucket if absent.
extern "C" void ResidenceComponent_impl_95a0(int64_t townMgr, void* ownerRefLocal, uint64_t key);



extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* selfP) {    if (!selfP || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    uintptr_t gw   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);

    uint64_t key = *reinterpret_cast<uint64_t*>(self + 0x48);
    uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uint64_t builderKey = profReg ? *reinterpret_cast<uint64_t*>(profReg + 0x38) : 0;
    if (key == 0 || key != builderKey) {
        return;   // not a construction workplace
    }

    // Remove the owner entity from the Builder bucket in the profession BST.
    int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    uint64_t* node = townMgr ? *reinterpret_cast<uint64_t**>(townMgr + 0x100) : nullptr;
    while (node) {
        if (key < node[0]) {
            node = reinterpret_cast<uint64_t*>(node[4]);          // left
        } else {
            if (key <= node[0]) {
                uint64_t localRef[4] = { static_cast<uint64_t>(ownerRef), 0, 0, 0 };
                CemeteryListErase_Reimpl(reinterpret_cast<void*>(node + 1), localRef);   // 0x140002cc0
                break;
            }
            node = reinterpret_cast<uint64_t*>(node[5]);          // right
        }
    }

    // Refresh the WorkComponent's worker capacity (+0x30 = CalcMaxWorkers).
    // 0x140003050 WorkBase_FindComponent_3050; 0x14012e5c0 WorkComponent_CalcMaxWorkers
    // (the registered body — fn_StorageCountQuery is the other port of that address).
    uintptr_t workComp = 0;
    WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(self + 0x10), &workComp);
    if (workComp)
        *reinterpret_cast<int32_t*>(workComp + 0x30) = WorkComponent_CalcMaxWorkers(workComp);

    // The natural profession comes from the linked WorkPlaceDescription (+0x20). Retail
    // reads it through IsDerivedFrom(desc, DAT_1401c8118) — and if that check FAILS retail
    // dereferences null+0x20, i.e. it crashes rather than resetting the key to 0. So the
    // check is an assertion, not a branch, and there is no retail behaviour to be faithful
    // to on the failing side. We run it for its diagnostic value and, when it rejects a
    // description we do have (the partial-factory-tree landmine — see
    // MapComponent_DetachOccupancy_4750), log once and trust the linked description anyway.
    // A genuinely null desc yields key 0 = "no profession", the correct end state for a house.
    void* desc = *reinterpret_cast<void**>(self + 0x18);
    if (desc &&
        !Object::IsDerivedFromNullSafe(desc, WorkPlaceDescription::TypeId())) {
        static bool s_warnedDesc = false;
        if (!s_warnedDesc) {
            s_warnedDesc = true;
            printf("[WorkPlaceComponent] LeaveBuilderProfession: WPC+0x18=%p is not a "
                   "WorkPlaceDescription (type id %u) — retail would fault here; trusting "
                   "it and reading +0x20 anyway\n", desc, WorkPlaceDescription::TypeId());
        }
    }
    uint64_t naturalKey = desc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;

    // Hand any in-flight work item over from Builder to the natural profession, so it stops
    // being offered to builders the moment the building is finished. FUN_1400f8400 —
    // citizenai_work_scheduler.cpp CitizenAI_WorkScheduler_Rekey.
    if (workComp && *reinterpret_cast<int32_t*>(workComp + 0x28) >= 0) {
        uintptr_t sched = *reinterpret_cast<uintptr_t*>(gw + 0x118);
        if (sched)
            CitizenAI_WorkScheduler_Rekey(sched, *reinterpret_cast<uint32_t*>(workComp + 0x28),
                    *reinterpret_cast<uint64_t*>(self + 0x48), naturalKey);
    }

    *reinterpret_cast<uint64_t*>(self + 0x48) = naturalKey;
    WorkPlaceComponent_OnCreateInit(selfP);   // ee60 — release every seated worker
}

// ============================================================
// WorkPlaceDescription_helper_edf0 (0x14012edf0) — the WorkPlaceComponent controller's
// state-2 ("built") ENTER handler.
// ============================================================
// State table proof (WorkPlaceDescription_helper_f3a0 @ 0x14012f3a0, the component's
// controller table builder): state 0 -> ee40, state 2 -> edf0, state 3 -> ed60. So the
// moment the entity's state machine enters state 2 (BuildComponent's 8->2 completion),
// retail runs f670 + re-register under whatever profession the building actually has.
//
// This is what makes the finished building's panel correct: WorkPlaceUI shows the worker
// spinner iff WPC+0x48 != 0 (workplaceui.cpp UpdateVisibility, faithful to Startup
// 0x140087080). Without this the Builder key survived completion, so a finished house
// kept showing the construction worker up/down buttons.
//
// Our WorkPlaceComponent is an inert component with no controller node, so the state
// machine cannot dispatch this. It is therefore driven from the build-completion site
// (BuildComponent_State8Finalize) — the same "collapse the missing controller into the
// site" pattern as MapComponent's cell occupancy. If WorkPlaceComponent is ever promoted
// to a live controller, move this call into its state-2 enter and delete the hook.
extern "C" void WorkPlaceComponent_EnterBuiltState(void* selfP) {
    if (!selfP) return;
    WorkPlaceComponent_LeaveBuilderProfession(selfP);

    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    uint64_t key = *reinterpret_cast<uint64_t*>(self + 0x48);
    if (key == 0) return;   // no profession (house / storage) -> nothing to register
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!townMgr) return;
    int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
    ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerRef, key);
}

// ============================================================
// WorkPlaceComponent_EnterBuiltState_Full — state-2 ("built") entry for ANY workplace
// ============================================================
// Retail dispatches edf0 from the WorkPlaceComponent controller's own state-2 enter, so
// EVERY workplace gets its profession seated the moment its entity's state machine
// reaches state 2. Our WorkPlaceComponent is inert (no controller node), and the port
// collapsed that dispatch into BuildComponent_State8Finalize — which is strictly
// narrower than retail's trigger. A player-placed CROP FIELD or ORCHARD completes
// through its OWN state machine and has no BuildComponent completion, so it never
// reached the hand-off at all:
//
//   MEASURED 2026-08-18, hand-built crop field: WPC+0x48 = NULL, and
//   [WorkPlaceUI] 0 shown / 5466 HIDDEN -- WorkplaceUI_Startup hides the whole
//   workPlace ribbon (worker up/down spinner AND the stop-sign icon) on +0x48 == 0.
//   The same session logged 8 construction hand-offs, all storage/houses/fishing dock,
//   and ZERO events for the crop field or orchard.
//
// TRIGGER: driven from StateMachineComponent on the entity's FIRST TICK and on any
// state change - NOT on state index 2. Retail keys the WPC controller off state 2,
// but a field has its own state space: a player-placed crop field sits in state 0
// until a crop is chosen (measured: zero transitions), so an index-keyed trigger
// never fires for the very buildings this exists to fix.
//
// EnterBuiltState alone does not fix them: it delegates to LeaveBuilderProfession, which
// bails on `key == 0`. A field never ran State1Enter, so it never held the Builder key
// there is nothing to "leave". The natural profession therefore has to be seated
// directly from the description, which is exactly what StartTown_ActivateWorkplace
// already does for spawned-complete buildings (starttown.cpp) -- and why demo-spawned
// fields have working arrows while player-built ones do not.
//
// A house or storage barn keeps key 0: their WorkPlaceDescription+0x20 is legitimately
// zero, so the natural-seat branch skips them and the profession BST (the EMPLOYER list)
// stays correct.
extern "C" void WorkPlaceComponent_EnterBuiltState_Full(void* selfP) {
    if (!selfP) return;
    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);

    // 1) The construction hand-off. No-op unless the Builder key is currently installed,
    //    so buildings that DID go through BuildComponent behave exactly as before.
    WorkPlaceComponent_EnterBuiltState(selfP);

    // 2) Never-constructed workplaces (crop field / orchard): seat the natural profession.
    if (*reinterpret_cast<uint64_t*>(self + 0x48) != 0) return;
    void* desc = *reinterpret_cast<void**>(self + 0x18);
    uint64_t natural = desc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;
    if (natural == 0) return;   // house / storage: correctly professionless

    *reinterpret_cast<uint64_t*>(self + 0x48) = natural;
    WorkPlaceComponent_SetAccepting(reinterpret_cast<int64_t>(selfP), true);

    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        if (townMgr) {
            int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
            ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerRef, natural);
        }
    }

}



// ===== ENDGAME B.3.8 MI-B — WorkPlaceComponent as a REAL class =====
// (WorkPlaceComponent.h). Before: COMPONENT_INERT_FACTORY expanded by hand (48-slot
// table filled per create, slots 3/7/16/17 patched per create, Inert_ScalarDtor in
// slot 0, Dispatcher<WPCCtrlTag> stamped into raw bytes at +0x28 plus the ctor
// defaults +0x40/+0x50 written by the factory). After: the compiler's 18-slot vtable,
// a real ctor/dtor chain, the controller a member object (B38_RULES D1, D2, D6).
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"

// ── LIVE controller ──
// Retail wiring, Ghidra-verified against vtable 0x140164d30 + ctor 0x14012f2e0 + the
// state-table builder WorkPlaceDescription_helper_f3a0 (0x14012f3a0):
//   * GetDefaultState (slot 16, 0x140003730) returns 0.
//   * GetController (slot 17, 0x140004080) returns the embedded member at +0x28; the
//     ctor seats the member back-pointer +0x40 = self and defaults accepting +0x50 = 1.
//   * States: 0 enter=ee40(->f520 "enter construction": become a Builder workplace,
//     re-key pending work via f8400, register in the Builder bucket, publish capacity,
//     SetAccepting) transitions {1,3}; 2 enter=edf0 ("built": leave Builder, re-key to
//     the natural profession, register under it) transitions {3}; 3 enter=ed60
//     ("demolish": release workers + BST-erase) transitions {6,4,5}.

// destroycomponent_workerrelease.cpp — faithful f520 port (0x14012f520).
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc);

// First-N bring-up trace for the three controller enters (stderr, unconditional —
// these fire a handful of times per run and are the proof the SM dispatches them).
static void WPC_TraceEnter(const char* which, void* comp) {
    static int n = 0;
    if (n >= 24) return;
    ++n;
    fprintf(stderr, "[WPCCtrl] %s enter: comp=%p key(+0x48)=0x%llx roster(+0x60)=%d\n",
            which, comp,
            (unsigned long long)*reinterpret_cast<uint64_t*>(
                reinterpret_cast<uintptr_t>(comp) + 0x48),
            *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(comp) + 0x60));
    fflush(stderr);
}

static char WPC_EnterConstructionState(void* comp, void* /*sm*/) {   // ee40 -> f520
    WPC_TraceEnter("state-0 (construction/f520)", comp);
    WorkPlaceComponent_EnterBuilderProfession(reinterpret_cast<int64_t>(comp));
    return 0;
}
static char WPC_EnterBuiltStateNode(void* comp, void* /*sm*/) {      // edf0
    WPC_TraceEnter("state-2 (built/edf0)", comp);
    WorkPlaceComponent_EnterBuiltState(comp);
    return 0;
}
static char WPC_EnterDemolishState(void* comp, void* /*sm*/) {       // ed60
    // ed60 = ee60 (release every worker) + BST-erase on the +0x48 key. The ported
    // OnCreate body is the same pair in erase-first order; the halves touch disjoint
    // state (+0x48/BST vs the +0x58 roster), so the order is immaterial.
    WPC_TraceEnter("state-3 (demolish/ed60)", comp);
    reinterpret_cast<WorkPlaceComponent*>(comp)->WorkPlaceComponent::OnRemove();   // B.3.8 bridge (the ported slot-7 body)
    return 0;
}

using WPCCtrlDispatch = ::ControllerLive::Dispatcher<WPCCtrlTag>;

// Built ONCE at static-init time, as retail's state-table builder does (B38 D6/D7).
static void WorkPlaceComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = WPCCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 0, reinterpret_cast<void*>(&WPC_EnterConstructionState),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 2, reinterpret_cast<void*>(&WPC_EnterBuiltStateNode),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&WPC_EnterDemolishState),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static const bool s_wpcStateTableBuilt = (WorkPlaceComponent_PopulateStateTable(), true);


// ── WPC construction census (UNCONDITIONAL, diagnostic only) ────────────────
// Every WorkPlaceComponent in the standalone exe is built by the ctor below (the
// factory entries CreateNewObject / CreateInPlaceObject are its only callers):
// ComponentEntity_CtorFromDescription and the save-load path both go through
// Object::Factory::CreateObject -> the registration's factory, and nothing else
// inserts into an entity's component array.
//
// Recording every construction makes that claim FALSIFIABLE at runtime instead of
// only by reading code: WorkPlaceComponent_WasFactoryBuilt() answers per object.
static const int kWpcCensusCap = 256;
static void* s_wpcCensus[kWpcCensusCap];
static int   s_wpcCensusCount = 0;

extern "C" int WorkPlaceComponent_CensusCount() { return s_wpcCensusCount; }
extern "C" bool WorkPlaceComponent_WasFactoryBuilt(const void* p) {
    if (!p) return false;
    for (int i = 0; i < s_wpcCensusCount && i < kWpcCensusCap; ++i)
        if (s_wpcCensus[i] == p) return true;
    return false;
}
// True while the census is complete enough to answer WasFactoryBuilt negatively.
// Past the cap a "no" only means "not among the first kWpcCensusCap".
extern "C" bool WorkPlaceComponent_CensusComplete() {
    return s_wpcCensusCount <= kWpcCensusCap;
}

static void WorkPlaceComponent_RecordCensus(WorkPlaceComponent* wpc) {
    if (s_wpcCensusCount < kWpcCensusCap) s_wpcCensus[s_wpcCensusCount] = wpc;
    ++s_wpcCensusCount;
    if (s_wpcCensusCount == kWpcCensusCap + 1)
        std::fprintf(stderr, "[WPCCensus] census FULL (>%d) — WasFactoryBuilt can now "
                     "false-negative; raise kWpcCensusCap\n", kWpcCensusCap);
    std::fprintf(stderr, "[WPCCensus] #%d wpc=%p ctor-defaults (+0x40=%p +0x50=%d)\n",
                 s_wpcCensusCount, static_cast<void*>(wpc),
                 static_cast<void*>(wpc->_controller._owner), (int)wpc->isAccepting);
    std::fflush(stderr);
}

// 0x14012f2e0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer (+0x40); +0x48 = 0; +0x50 = 1
// (accepting: FindAvailableBuilding's gate reads it); +0x58 = +0x60 = 0; then
// `_flags |= 2` (0x82).
WorkPlaceComponent::WorkPlaceComponent()
    : _controller(this), workerData(nullptr), isAccepting(1), _pad51{},
      field58(nullptr), field60(nullptr) {
    OrFlagsInCtor(0x2);
    WorkPlaceComponent_RecordCensus(this);
}

// 0x14012f270: MemFree(+0x58), +0x58 = +0x60 = 0, then the restamps and ~Internal
// (the compiler chain).
WorkPlaceComponent::~WorkPlaceComponent() {
    Object::detail::InternalMemFree(field58);
    field58 = nullptr;
    field60 = nullptr;
}

// create 0x14003a030: MemAlloc(0x68, 8), then the ctor.
void* WorkPlaceComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WorkPlaceComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WorkPlaceComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* WorkPlaceComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WorkPlaceComponent());
}

// ============================================================
// WorkPlaceComponent::Shutdown (vtable slot [3], 0x14012f120)
// ============================================================
// Component_Serialize + empty serializer(v0) + versioned serializer(v1) with:
//   - worker count at +0x60, worker InternalBasePtr array at +0x58
//   - IsDerivedFrom validation on each worker ptr
//   - isAccepting bool at +0x50
//   - version < 1: read +0x48 from description component
//   - version >= 1: serialize +0x48 via FUN_140018020

void WorkPlaceComponent::Persist(void* stream) {
    // Base Component serialization
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Empty serializer block (version 0, immediately destroyed)
    alignas(8) uint8_t emptySerializer[24];
    ::new (static_cast<void*>(emptySerializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(emptySerializer)->~Serializer();   // B.3.4 bridge

    // Main serializer block (version 1)
    alignas(8) uint8_t serializer[16];  // Ghidra shows local_30[2] = 16 bytes
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Serialize worker count from +0x60
    int workerCount = *(int*)(reinterpret_cast<uintptr_t>(this) + 0x60);
    int countBuf = workerCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(countBuf, nullptr);   // B.3.4 bridge

    // On load, resize the worker array to the count just read.
    //
    // ⚠ This tested `*(int*)(serializer + 8) == 0`, but IO::Serializer +0x08 is
    // m_encoding — a POINTER, never 0 — so the resize NEVER ran on either leg.
    // The loading flag lives on the STREAM (MemoryStream +0x08), which is how
    // ComponentEntity_Serialize and goals_serialize.cpp test it. With the resize
    // dead, +0x58 stayed null on load and the `workerArray` guard below silently
    // skipped reading `count` worker IBPs that the save had written — and each of
    // those pulls a worker's ENTITY inline, so the save wrote hundreds of bytes
    // the load never consumed. That desynced the stream for every later object:
    // a subsequent entity's component record was read off misaligned bytes,
    // handing InternalFactory::PersistObject a bogus objId (0xAAA12C08, identical
    // across saves because it is stream content, not heap garbage) which it used
    // to index its id map out of bounds -> ACCESS_VIOLATION on load.
    const uintptr_t serStream = *(uintptr_t*)serializer;
    const bool loadingLeg = serStream && (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    // B.3.2 bridge: FUN_1400032e0 is Collection::Array<void*>::SetSize(n)
    if (loadingLeg) {
        GameColl::AsArray<GameColl::Ptr8>((void*)(reinterpret_cast<uintptr_t>(this) + 0x58)).SetSize(countBuf);
    }

    // Serialize each worker InternalBasePtr and validate type
    void* workerArray = *(void**)(reinterpret_cast<uintptr_t>(this) + 0x58);
    int64_t count = (int64_t)countBuf;
    if (count > 0 && workerArray) {
        uintptr_t ptr = (uintptr_t)workerArray;
        for (int64_t i = 0; i < count; i++) {
            reinterpret_cast<Object::InternalBasePtr*>((void*)ptr)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            // Validate: if ptr dereferences to non-null, check IsDerivedFrom
            void* obj = *(void**)ptr;
            if (obj != nullptr) {
                bool valid = reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(
                    CitizenComponent::TypeId());   // import slot 0x140152a98
                if (!valid) {
                    *(uint64_t*)ptr = 0;  // Clear invalid reference
                }
            }
            ptr += 8;  // InternalBasePtr is 8 bytes
        }
    }

    // Serialize isAccepting bool at +0x50
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&isAccepting), nullptr);   // B.3.4 bridge

    // Version check for +0x48 field
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version < 1) {
        // Legacy path: read from description component via IsDerivedFrom
        void* obj = _description;
        bool derived = reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(
            WorkPlaceDescription::TypeId());   // import slot 0x140152a98
        void* descComp = derived ? obj : nullptr;
        if (descComp) {
            *(uint64_t*)&workerData = *(uint64_t*)((uintptr_t)descComp + 0x20);
        }
    } else {
        // Version >= 1: serialize via FUN_140018020
        Reimpl_ExternalPtr_Serialize_StringTable(
            (void*)(reinterpret_cast<uintptr_t>(this) + 0x48), serializer);
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// ============================================================
// WorkPlaceComponent::OnRemovedFromWorld (vtable slot [7], 0x14012f090)
// ============================================================
// Slot 7 (+0x38) is the REMOVAL broadcast slot — FUN_1400c2ea0 (MarkForRemoval)
// dispatches it to every component whose +0x20 capability flags have bit 1 set
// (WorkPlaceComponent's are 0x82, so it qualifies). The Ghidra name "OnCreate" is a
// misnomer, the same one MapComponent carries: this is the TEARDOWN.
//
// It reads +0x48 (the profession key), finds that key's bucket in the town's
// profession BST (gameWorld+0xf0 -> +0x100), and REMOVES this component's owner
// entity from the bucket's entity array (FUN_140002cc0). Then, unconditionally,
// ee60 releases every seated worker.
//
// Why it matters in standalone: while a building is under construction its
// WorkPlaceComponent is registered under the Builder profession (the list
// CitizenBehavior_BuildPathGraph and FindAvailableBuilding walk). With the inert
// factory's no-op slot 7 the entry survived the entity's free, and the next
// BuildPathGraph read the freed entity through it — an ACCESS_VIOLATION at
// citizenbehavior.cpp:716 (fn_Entity_GetGridId on the stale IBP), observed on the
// first successful building upgrade. Same class as the MapComponent cell-occupancy
// and PickingComponent pick-mesh teardowns.

void WorkPlaceComponent::OnRemove() {
    uint64_t professionId = *(uint64_t*)(reinterpret_cast<uintptr_t>(this) + 0x48);
    if (professionId != 0 && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        // Build InternalBasePtr for owner
        uint64_t ownerRef = *(uint64_t*)(reinterpret_cast<uintptr_t>(this) + 0x10);

        // Get BST root from game world +0xf0 -> +0x100
        uintptr_t gameState = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        uint64_t* node = *(uint64_t**)(gameState + 0x100);

        bool found = false;
        while (node != nullptr) {
            if (professionId < *node) {
                node = (uint64_t*)node[4];  // left child
            } else if (professionId > *node) {
                node = (uint64_t*)node[5];  // right child
            } else {
                // Found: UNregister the owner entity from this bucket's entity array.
                uint64_t localRef[4];
                localRef[0] = ownerRef;
                CemeteryListErase_Reimpl((void*)(node + 1), (void*)localRef);   // 0x140002cc0
                found = true;
                break;
            }
        }

        if (!found) {
            WorkPlaceComponent_OnCreateInit(this);   // 0x14012ee60
            return;
        }
    }

    // Always call fallback init
    WorkPlaceComponent_OnCreateInit(this);   // 0x14012ee60
}

DEFINE_EXE_TYPE(WorkPlaceComponent, "ControllerComponent")
