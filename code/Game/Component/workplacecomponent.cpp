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
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
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
#include "Entity/ComponentEntity.h"            // ComponentEntity::FindComponent<T>
#include "Entity/CitizenBehavior.h"
#include "Component/StatusIconComponent.h"     // 0x140002f90's class
#include "Component/DestroyComponent.h"        // 0x140005780's class
#include "Component/StateMachineComponent.h"   // 0x140003ea0's class
#include "Component/WorkComponent.h"           // 0x140003050's class
#include "Game/GameSettings.h"
#include "Game/Profession.h"
#include "Game/ProfessionList.h"

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

// ============================================================
// Reimpl bodies for the detach/attach helpers (0x14012f7e0 / 0x14012fa20).
// Their full exe FUN_ closure became registered in the gap wave, so they are now
// closure-safe to port. Sub-helpers used:
//   FUN_140002cc0 CemeteryListErase_Reimpl (worker-list remove)
//   FUN_1400c2b70 ModelComponentNotify_Reimpl (mesh visibility/state kick)
//   FUN_140002f90 FindComponent<StatusIconComponent>
//   FUN_14012c3f0 CitizenComp_FUN_14012c3f0 (mesh-instance flag setter)
//   FUN_140005780 FindComponent<DestroyComponent>
//     + FUN_14011e710 PlayerPath_Sub_11e710 (bool getter)
//   FUN_140003110 (worker-list append)
//     + FUN_1400c3470 ActHunt_NotifyEntityChanged_c3470 (entity flag notify)
// SetAccepting pause path (0x14012fc00): the owner's state machine finder
// (0x140003ea0 FindComponent<StateMachineComponent>), the two citizen workplace finders
// the eviction loop dispatches between (0x14010ffc0 CitizenComp_sub_ffc0 / 0x140110470
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

// The WorkPlaceDescription off +0x18, null when it is not one.
static WorkPlaceDescription* WPC_Description(const WorkPlaceComponent* wpc) {
    Description* d = wpc->_description;
    return Object::IsDerivedFromNullSafe(d, WorkPlaceDescription::TypeId())
        ? static_cast<WorkPlaceDescription*>(d) : nullptr;
}

// 0x14012f7e0 — worker DETACH: remove the worker from the roster (+0x58), then if the
// roster is now empty: kick the model (mode 3), clear the icon flags 2 and 3, and —
// when the workplace is accepting, the entity is not being destroyed, the profession
// is the description's (desc+0x20) and the entity is not culled — raise icon bit 4
// (the inlined 0x14012c3f0 set arm).
void WorkPlaceComponent::Detach(Object::InternalPtr<CitizenComponent>* worker) {
    CemeteryListErase_Reimpl(&workers, worker);   // 0x140002cc0
    // Build-time roster-only mode (see g_wpcRosterOnly): stop after the roster removal,
    // skipping the gameplay-time Scene mesh-visual toggle that deadlocks mid-build.
    if (g_wpcRosterOnly) return;
    if (workerCount != 0) return;

    ModelComponentNotify_Reimpl(_entity, 3);   // 0x1400c2b70

    // Partial-world guard (see Attach): skip the status-icon visual toggles when the
    // standalone entity lacks the renderable StatusIconComponent (c7e08).
    StatusIconComponent* iconA = _entity->FindComponent<StatusIconComponent>();   // 0x140002f90
    if (iconA) iconA->SetFlag(2, '\0');   // 0x14012c3f0

    StatusIconComponent* iconB = _entity->FindComponent<StatusIconComponent>();
    if (iconB) iconB->SetFlag(3, '\0');

    if (!isAccepting) return;

    // Partial-world guard: on an inert standalone workplace, 0x140005780 yields a null
    // DestroyComponent; passing it to Getter_e710 dereferences null+0x10 (READ of 0x10),
    // which crashed when a citizen switched workplaces during the per-frame update (that
    // path doesn't hold g_wpcRosterOnly). A null accessor means "nothing renderable to
    // toggle", so skip the visual tail — same as g != 0 (entity already culled).
    DestroyComponent* destroy = _entity->FindComponent<DestroyComponent>();   // 0x140005780
    if (!destroy) return;
    // 0x14011e710 DestroyComponent::InRemovalState (the tree's PlayerPath_Sub_11e710).
    bool g = destroy->InRemovalState();
    if (g) return;

    Profession* jobSlot = profession;
    if (jobSlot == nullptr) return;

    // Guard the description before IsDerivedFrom (Runtime IsDerivedFrom has no
    // null-check) and before the +0x20 read: an inert standalone workplace can carry a
    // null/absent description. A null desc means the job can't match -> skip the tail.
    WorkPlaceDescription* desc = _description ? WPC_Description(this) : nullptr;
    if (!desc || jobSlot != desc->_profession0) return;

    StatusIconComponent* mesh = _entity->FindComponent<StatusIconComponent>();
    if (!mesh) return;   // partial-world guard: no StatusIconComponent renderable
    if ((mesh->_entity->_entityFlags >> 10 & 1) != 0) return;

    mesh->_geometry.activeMask |= 0x10;
    // Each branch reads the component flags FRESH right before the OR/AND (the original
    // reads *(uint*)(plVar2+4) at the assignment, NOT a value captured before the branch —
    // the add-branch's Tick (vtable +0x40) can mutate this flags word).
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
    // (The mask was just OR'ed with 0x10, so the clear arm is retail's dead arm.)
    if (mesh->_geometry.activeMask == 0) {
        if (mesh->iconId >= 0) {
            if (GameUpdateLoop::_singleton)
                GameUpdateLoop::_singleton->m_graphicsScene
                    ->RemoveObject(mesh->iconId);   // B.3.5 bridge
            mesh->iconId = -1;
        }
        uint32_t old4 = mesh->_flags;
        uint32_t new4 = old4 & 0xfffffffb;
        mesh->_flags = new4;
        changed = new4 ^ old4;
    } else {
        if (g_gameSettings != nullptr &&
            g_gameSettings->statusIconEnable != '\0' && mesh->iconId < 0) {
            if (GameUpdateLoop::_singleton) {
                uint32_t id = GameUpdateLoop::_singleton->m_graphicsScene
                    ->AddObject(&mesh->_geometry);   // B.3.5 bridge
                mesh->iconId = static_cast<int32_t>(id);
                SceneWatch_Note(id, &mesh->_geometry, "WorkPlaceComponent carry-mesh");
            }
        }
        System::Time t = {0};
        RtSystem_Time_Ctor(&t, 0.0f);   // import slot 0x140152b00
        // (**(code**)(*mesh + 0x40))(mesh, &time): Component::Tick, slot 8.
        mesh->Tick(&t);
        uint32_t old4 = mesh->_flags;   // FRESH after Tick
        uint32_t new4 = old4 | 4;
        mesh->_flags = new4;
        changed = new4 ^ old4;
    }
    if ((changed & 0xffff) != 0 && mesh->_entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(mesh->_entity);   // 0x1400c3470
    }
}

// 0x14012fa20 — worker ATTACH: if the roster is empty and the profession is the
// description's (desc+0x20), kick the model (mode 2); clear icon bit 4; append the
// worker to the roster (+0x58, new count = +0x60 + 1).
void WorkPlaceComponent::Attach(Object::InternalPtr<CitizenComponent>* worker) {
    if (!g_wpcRosterOnly) {
        if (workerCount == 0) {
            Profession* jobSlot = profession;
            // Guard the description before IsDerivedFrom (no null-check) and before the
            // +0x20 read — an inert standalone workplace may have a null description.
            WorkPlaceDescription* desc = _description ? WPC_Description(this) : nullptr;
            if (desc && jobSlot == desc->_profession0) {
                ModelComponentNotify_Reimpl(_entity, 2);   // 0x1400c2b70
            }
        }

        // Faithful-equivalent partial-world guard: the original always toggles the building's
        // status-icon mesh (FUN_14012c3f0) because every real building carries the renderable
        // StatusIconComponent (c7e08). A standalone-spawned entity may lack it (inert factory /
        // template not fully materialised) → the finder returns a null component and
        // FUN_14012c3f0 faults reading mesh+0x10. Skip only the visual toggle when absent; the
        // occupant roster append below still runs, so the house tracks its occupants regardless.
        StatusIconComponent* mesh = _entity->FindComponent<StatusIconComponent>();   // 0x140002f90
        if (mesh) mesh->SetFlag(4, '\0');   // 0x14012c3f0
    }

    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>(&workers).SetSizeFill(workerCount + 1, worker);
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

void WorkPlaceComponent::EvictRoster() {
    const bool savedRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    int guard = 0;
    while (workerCount > 0) {
        int before = workerCount;

        CitizenComponent* worker = workers[0].Get();

        // Pick the finder off the worker's profession record (+0x1f0 -> +0x4c).
        ComponentEntity* newWorkplace = nullptr;
        if (!worker->profession->field4C) {
            newWorkplace = worker->FindWorkplace();         // 0x14010ffc0
        } else {
            newWorkplace = worker->FindRankedWorkplace();   // 0x140110470
        }

        // Re-read the roster base — the finders above can reallocate it.
        workers[0].Get()->SetWorkplace(newWorkplace);   // 0x140111ee0 (the tree's "AssignHousing")

        if (workerCount >= before || ++guard > 4096) {
            std::fprintf(stderr, "[WorkPlace] pause eviction made no progress on comp=%p "
                         "(roster %d -> %d, worker=%p) — breaking to avoid a spin\n",
                         static_cast<void*>(this), before, workerCount, static_cast<void*>(worker));
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
// profession +0x48 == desc.profession desc+0x20) — RAISES the "no workers" warning bit 4
// (0x10) on the building's StatusIconComponent. This is the SOLE faithful raiser of that
// warning; once a worker is attached, Attach clears the bit again (MeshFlag(mesh,4,0)).
// The 2D panel (statusiconui.cpp) reads the resulting +0xF0 bitfield.
//
// The status-icon component is found through FindComponent<StatusIconComponent>
// (0x140002f90) and toggled through the flag setter FUN_14012c3f0 (0x14012c3f0). The
// raise's cull-guard and Scene Add/position/notify all live inside FUN_14012c3f0, so a
// plain MeshFlag(mesh,4,1) reproduces the original's inlined raise byte-for-byte.
//
// The accepting=false (PAUSE) path does three more things, all ported below:
//   a. raise the "closed" indicator — bit 0 when the building's state machine state
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
// ctor default is set in the ctor below. A grep for "SetAccepting" will miss both — see
// the block at buildcomponent_state1.cpp for why that matters.
void WorkPlaceComponent::SetAccepting(bool accepting) {
    isAccepting = accepting ? 1 : 0;

    // Locate the building's StatusIconComponent (the "mesh" the icon toggles hang off).
    // LEFT: 0x14012c3f0 takes the icon as int64_t* (COMP-b's declaration).
    StatusIconComponent* mesh = _entity->FindComponent<StatusIconComponent>();   // 0x140002f90
    if (!mesh) {
        // Partial-world: no renderable icon. The eviction below is gameplay state, not
        // visuals, so it must still run on the pause path.
        if (!accepting) EvictRoster();
        return;
    }

    if (accepting) {
        // Clear the "closed"/"disabled" indicator bits (1 then 0) — a now-accepting
        // workplace is no longer showing the closed sign.
        mesh->SetFlag(1, '\0');   // 0x14012c3f0
        mesh->SetFlag(0, '\0');
    } else {
        // (a) raise the closed indicator, keyed on the owner's state machine state.
        StateMachineComponent* sm = _entity->FindComponent<StateMachineComponent>();   // 0x140003ea0
        // Partial-world guard: a standalone workplace can lack the state machine. The
        // original derefs it unguarded; with none present treat the building as "normal"
        // (the state the start-town / force-completed buildings are actually in).
        // The pending state (+0x52), else the requested one (+0x50).
        uint8_t state = 0;
        if (sm) {
            state = static_cast<uint8_t>(sm->stateMachine.pendingState);
            if (sm->stateMachine.pendingState < 0) state = static_cast<uint8_t>(sm->stateMachine.currentInput);
        }
        bool normalState = ((state & 0xf6) == 0) && (state != 9);
        mesh->SetFlag(normalState ? 0 : 1, '\x01');
    }

    if (!accepting) {
        // (b) clear the worker/warning icons. Each toggle re-finds the mesh in the
        // original; the finder is pure, so one lookup is equivalent.
        mesh->SetFlag(2, '\0');
        mesh->SetFlag(3, '\0');
        mesh->SetFlag(4, '\0');
    } else if (workerCount == 0) {
        // Raise the "no workers" warning (bit 4) only for an UNSTAFFED workplace still on
        // its designated job (roster empty && profession == desc.profession) — the faithful raise.
        WorkPlaceDescription* desc = WPC_Description(this);
        if (desc && profession == desc->_profession0) {
            // Re-find the mesh (mirrors the original's per-call finder) and raise bit 4. The
            // cull-guard + Scene Add + positioning + entity-notify all run inside the toggle.
            mesh = _entity->FindComponent<StatusIconComponent>();
            if (mesh) mesh->SetFlag(4, '\x01');
        }
    }

    // (c) evict the roster.
    if (!accepting) EvictRoster();
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
void WorkPlaceComponent::ApplyOccupiedVisual() {
    // ModelKick(mode 2) — notify the building's components of the occupied state, gated (as
    // the original attach is) on this WorkPlaceComponent being the residence/job the
    // description designates (profession == desc+0x20). This is the faithful notify; it is
    // crash-free. NOTE on visibility: ModelComponent's notify slot (vtable +0x60 =
    // 0x140118ab0) is an EMPTY no-op even in the original exe, and the houses' other
    // visual-bearing components (AmbientEmitterComponent smoke, StatusIconComponent icon)
    // are INERT in standalone (spawned but never scene-registered), so this notify produces
    // no visible change yet — it's correct scaffolding awaiting those components' bring-up.
    Profession* jobSlot = profession;
    WorkPlaceDescription* desc = WPC_Description(this);
    if (desc && jobSlot == desc->_profession0) {
        ModelComponentNotify_Reimpl(_entity, 2);   // 0x1400c2b70
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

// 0x14012ee60 — release every seated worker: detach each from its current workplace
// and reattach it to the (empty) replacement.
void WorkPlaceComponent::ReleaseWorkers() {
    while (workerCount > 0) {
        // Termination depends on the detach below removing worker[0] from this
        // component's array (+0x58 / count +0x60) — which it only does when the
        // worker's own workplace (+0x248) still names this workplace.
        // Retail holds that invariant; standalone seats workers through several
        // paths (WorkPlaceUI spinners, the builder pool auto-seat), so a stale
        // back-pointer would spin here forever. A HANG is far worse than a leaked
        // roster slot, so drop the entry ourselves if an iteration made no progress.
        const int countBefore = workerCount;

        ComponentEntity* emptyHost = nullptr;   // the InternalBasePtr(nullptr) retail builds

        CitizenComponent* worker = workers[0].Get();

        if (worker->workplace != nullptr) {
            WorkPlaceComponent* comp = worker->workplace->FindComponent<WorkPlaceComponent>();
            Object::InternalPtr<CitizenComponent> workerIbp(worker);
            comp->Detach(&workerIbp);   // 0x14012f7e0
        }

        worker->workplace = emptyHost;
        if (emptyHost != nullptr) {
            WorkPlaceComponent* comp = worker->workplace->FindComponent<WorkPlaceComponent>();
            Object::InternalPtr<CitizenComponent> workerIbp(worker);
            comp->Attach(&workerIbp);   // 0x14012fa20
        }

        if (workerCount >= countBefore) {
            // No progress — shift worker[0] out by hand and keep going.
            int n = workerCount;
            Object::InternalPtr<CitizenComponent>* arr = workers;
            if (arr) for (int i = 1; i < n; ++i) arr[i - 1] = arr[i];
            workerCount = n - 1;
            static int s_reported = 0;
            if (s_reported++ < 10) {
                printf("[WorkPlaceComponent] release: worker[0]=%p did not detach "
                            "(its +0x248 job host does not name this workplace %p); "
                            "dropped it by hand, roster %d -> %d\n",
                            static_cast<void*>(worker), static_cast<void*>(this), n, n - 1);
            }
        }
    }
}

// town_workplace_registry.cpp — 0x1400895a0, add the owner entity to the profession
// bucket for `key` in the town BST (townMgr+0x100), creating the bucket if absent.
extern "C" void ResidenceComponent_impl_95a0(int64_t townMgr, void* ownerRefLocal, uint64_t key);

// 0x14012f670 — leave the Builder profession: drop the owner from the Builder bucket of
// the town's profession tree, refresh the WorkComponent's capacity, hand the posted work
// over to the natural profession, seat it, and release every worker.
void WorkPlaceComponent::LeaveBuilderProfession() {
    if (GameUpdateLoop::_singleton == nullptr) return;
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;

    Profession* key = profession;
    ProfessionList* profReg = gw->m_professionList;
    Profession* builderKey = profReg ? profReg->field_0x38 : nullptr;
    if (key == nullptr || key != builderKey) {
        return;   // not a construction workplace
    }

    // Remove the owner entity from the Builder bucket in the profession BST.
    // The tree is CitizenBehavior +0x100 (ProfessionBucket nodes, the key compared by
    // address).
    ComponentEntity* ownerRef = _entity;
    CitizenBehavior* townMgr = gw->m_citizenList;
    ProfessionBucket* node = townMgr ? townMgr->field_0x100 : nullptr;
    while (node) {
        if (key < node->key) {
            node = node->left;          // +0x20
        } else {
            if (key <= node->key) {
                ComponentEntity* localRef[4] = { ownerRef, nullptr, nullptr, nullptr };
                CemeteryListErase_Reimpl(&node->entities, localRef);   // 0x140002cc0 (the bucket's Array, +0x08)
                break;
            }
            node = node->right;         // +0x28
        }
    }

    // Refresh the WorkComponent's worker capacity (+0x30 = CalcMaxWorkers).
    // 0x140003050 FindComponent<WorkComponent>; 0x14012e5c0 WorkComponent_CalcMaxWorkers
    // (the registered body — WorkComponent::CalcMaxWorkers is the other port of that
    // address).
    WorkComponent* workComp = _entity->FindComponent<WorkComponent>();
    if (workComp)
        workComp->workState = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(workComp));   // LEFT: SIG WorkComponent_CalcMaxWorkers

    // The natural profession comes from the linked WorkPlaceDescription (+0x20). Retail
    // reads it through IsDerivedFrom(desc, DAT_1401c8118) — and if that check FAILS retail
    // dereferences null+0x20, i.e. it crashes rather than resetting the key to 0. So the
    // check is an assertion, not a branch, and there is no retail behaviour to be faithful
    // to on the failing side. We run it for its diagnostic value and, when it rejects a
    // description we do have (the partial-factory-tree landmine — see
    // MapComponent_DetachOccupancy_4750), log once and trust the linked description anyway.
    // A genuinely null desc yields key 0 = "no profession", the correct end state for a house.
    Description* desc = _description;
    if (desc &&
        !Object::IsDerivedFromNullSafe(desc, WorkPlaceDescription::TypeId())) {
        static bool s_warnedDesc = false;
        if (!s_warnedDesc) {
            s_warnedDesc = true;
            printf("[WorkPlaceComponent] LeaveBuilderProfession: WPC+0x18=%p is not a "
                   "WorkPlaceDescription (type id %u) — retail would fault here; trusting "
                   "it and reading +0x20 anyway\n", static_cast<void*>(desc), WorkPlaceDescription::TypeId());
        }
    }
    Profession* naturalKey = desc
        ? static_cast<WorkPlaceDescription*>(desc)->_profession0 : nullptr;

    // Hand any in-flight work item over from Builder to the natural profession, so it stops
    // being offered to builders the moment the building is finished. FUN_1400f8400 —
    // citizenai_work_scheduler.cpp CitizenAI_WorkScheduler_Rekey.
    // The rekey takes the scheduler and its keys as integers (wave X).
    if (workComp && workComp->workId >= 0) {
        uintptr_t sched = (uintptr_t)gw->m_tutorial;   // LEFT: SIG CitizenAI_WorkScheduler_Rekey
        if (sched)
            CitizenAI_WorkScheduler_Rekey(sched, static_cast<uint32_t>(workComp->workId),
                    (uint64_t)profession, (uint64_t)naturalKey);   // LEFT: SIG CitizenAI_WorkScheduler_Rekey
    }

    profession = naturalKey;
    ReleaseWorkers();   // ee60 — release every seated worker
}

// ============================================================
// WorkPlaceComponent::EnterBuilt (0x14012edf0) — the controller's state-2 ("built")
// ENTER handler.
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

// First-N bring-up trace for the three controller enters (stderr, unconditional —
// these fire a handful of times per run and are the proof the SM dispatches them).
static void WPC_TraceEnter(const char* which, const WorkPlaceComponent* comp) {
    static int n = 0;
    if (n >= 24) return;
    ++n;
    fprintf(stderr, "[WPCCtrl] %s enter: comp=%p key(+0x48)=%p roster(+0x60)=%d\n",
            which, static_cast<const void*>(comp), static_cast<void*>(comp->profession),
            comp->workerCount);
    fflush(stderr);
}

char WorkPlaceComponent::EnterBuilt() {
    WPC_TraceEnter("state-2 (built/edf0)", this);
    LeaveBuilderProfession();

    Profession* key = profession;
    if (key == nullptr) return 0;   // no profession (house / storage) -> nothing to register
    if (GameUpdateLoop::_singleton == nullptr) return 0;
    // 0x1400895a0 takes the town manager and the key as integers (Game/exe_bodies.h, wave X).
    uintptr_t townMgr = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: SIG ResidenceComponent_impl_95a0
    if (!townMgr) return 0;
    ComponentEntity* ownerRef = _entity;
    ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerRef, (uint64_t)key);   // LEFT: SIG ResidenceComponent_impl_95a0
    return 0;
}

// 0x14012ee40 — state 0 ("construction") ENTER: become a Builder workplace (0x14012f520).
char WorkPlaceComponent::EnterConstruction() {
    WPC_TraceEnter("state-0 (construction/f520)", this);
    EnterBuilderProfession();
    return 0;
}

// 0x14012ed60 — state 3 ("demolish") ENTER: ee60 (release every worker) + the BST erase on
// the +0x48 key. The ported OnRemove body is the same pair in erase-first order; the
// halves touch disjoint state (+0x48/BST vs the +0x58 roster), so the order is immaterial.
char WorkPlaceComponent::EnterDemolish() {
    WPC_TraceEnter("state-3 (demolish/ed60)", this);
    WorkPlaceComponent::OnRemove();   // the ported slot-7 body
    return 0;
}

// ---- B4 FORWARD: the C spellings outside callers still use; the closing wave rewires
// them. SetAccepting: attendancecomponent(_states),
// cropfieldcomponent(_states), destroycomponent_workerrelease, orchardcomponent(_states),
// pasturecomponent_states, resourceproducecomponent_states, Dialog/buildui.cpp,
// Game/starttown.cpp. OnCreateInit: destroycomponent_workerrelease.
// LeaveBuilderProfession: destroycomponent_states (the null test is the tree's).
// ApplyOccupiedVisual: Game/starttown.cpp.
void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting) {
    ((WorkPlaceComponent*)comp)->SetAccepting(accepting);   // B4 FORWARD
}
void WorkPlaceComponent_OnCreateInit(void* selfP) {
    ((WorkPlaceComponent*)selfP)->ReleaseWorkers();   // B4 FORWARD
}
extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* selfP) {
    if (!selfP) return;
    ((WorkPlaceComponent*)selfP)->LeaveBuilderProfession();   // B4 FORWARD
}
void WorkplaceComponent_ApplyOccupiedVisual(int64_t comp) {
    ((WorkPlaceComponent*)comp)->ApplyOccupiedVisual();   // B4 FORWARD
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

static char WPC_EnterConstructionState(WorkPlaceComponent* c, void* /*sm*/) { return c->EnterConstruction(); }   // 0x14012ee40
static char WPC_EnterBuiltStateNode(WorkPlaceComponent* c, void* /*sm*/)    { return c->EnterBuilt(); }          // 0x14012edf0
static char WPC_EnterDemolishState(WorkPlaceComponent* c, void* /*sm*/)     { return c->EnterDemolish(); }       // 0x14012ed60

using WPCCtrlDispatch = ::ControllerLive::Dispatcher<WPCCtrlTag>;

// Built ONCE at static-init time, as retail's state-table builder does (B38 D6/D7).
static void WorkPlaceComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    auto& t = WPCCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 0, &WPC_EnterConstructionState,
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 2, &WPC_EnterBuiltStateNode,
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, &WPC_EnterDemolishState,
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
    : _controller(this), profession(nullptr), isAccepting(1), _pad51{},
      workers(nullptr), workerCount(0), workerCapacity(0) {
    OrFlagsInCtor(0x2);
    WorkPlaceComponent_RecordCensus(this);
}

// 0x14012f270: MemFree(+0x58), +0x58 = +0x60 = 0, then the restamps and ~Internal
// (the compiler chain).
WorkPlaceComponent::~WorkPlaceComponent() {
    Object::detail::InternalMemFree(workers);
    workers = nullptr;
    workerCount = 0;       // retail: one qword 0 over +0x60..+0x67
    workerCapacity = 0;
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

void WorkPlaceComponent::Persist(IO::Stream& stream) {
    // Base Component serialization
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Empty serializer block (version 0, immediately destroyed)
    { IO::Serializer scope(stream, 0); }

    // Main serializer block (version 1)
    IO::Serializer ser(stream, 1);

    // Serialize the worker count from +0x60
    int countBuf = workerCount;
    ser.Serialize(countBuf, nullptr);

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
    const bool loadingLeg = ser._stream && (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    // B.3.2 bridge: FUN_1400032e0 is Collection::Array<void*>::SetSize(n)
    if (loadingLeg) {
        GameColl::AsArray<GameColl::Ptr8>(&workers).SetSize(countBuf);
    }

    // Serialize each worker InternalBasePtr, nulled when it is not a CitizenComponent
    // (import slot 0x140152a98).
    Object::InternalPtr<CitizenComponent>* workerArray = workers;
    int64_t count = (int64_t)countBuf;
    if (count > 0 && workerArray) {
        for (int64_t i = 0; i < count; i++)
            workerArray[i].Serialize(ser, CitizenComponent::TypeId());
    }

    // Serialize isAccepting bool at +0x50
    ser.Serialize(isAccepting, nullptr);

    // Version check for +0x48 field
    int version = ser.GetVersion();
    if (version < 1) {
        // Legacy path: read from description component via IsDerivedFrom
        Description* obj = _description;
        bool derived = obj->IsDerivedFrom(WorkPlaceDescription::TypeId());   // import slot 0x140152a98
        if (derived) {
            profession = static_cast<WorkPlaceDescription*>(obj)->_profession0;
        }
    } else {
        // Version >= 1: serialize via FUN_140018020
        Reimpl_ExternalPtr_Serialize_StringTable(&profession, &ser);
    }
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
    Profession* professionId = profession;
    if (professionId != nullptr && GameUpdateLoop::_singleton != nullptr) {
        ComponentEntity* ownerRef = _entity;

        // The town's profession tree: CitizenBehavior +0x100 (ProfessionBucket nodes).
        CitizenBehavior* gameState = GameUpdateLoop::_singleton->m_citizenList;
        ProfessionBucket* node = gameState->field_0x100;
        Profession* const key = professionId;

        bool found = false;
        while (node != nullptr) {
            if (key < node->key) {
                node = node->left;   // +0x20
            } else if (key > node->key) {
                node = node->right;  // +0x28
            } else {
                // Found: UNregister the owner entity from this bucket's entity array.
                ComponentEntity* localRef[4];
                localRef[0] = ownerRef;
                CemeteryListErase_Reimpl(&node->entities, localRef);   // 0x140002cc0 (+0x08)
                found = true;
                break;
            }
        }

        if (!found) {
            ReleaseWorkers();   // 0x14012ee60
            return;
        }
    }

    // Always call fallback init
    ReleaseWorkers();   // 0x14012ee60
}

DEFINE_EXE_TYPE(WorkPlaceComponent, "ControllerComponent")
