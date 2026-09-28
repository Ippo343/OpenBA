// engine/src/ui/workplaceui.cpp
// Clean reimplementation of WorkplaceUI game logic.
//
// WorkplaceUI (0x60 bytes) — workplace worker count panel.
// Extends DialogController. Fields:
//   +0x30 workPlaceCount, +0x38 maxCount (InternalBasePtr)
//   +0x40 workerCountUp, +0x48 workerCountDown (InternalBasePtr)
//   +0x50 jobCountUp, +0x58 jobCountDown (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Dialog/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Dialog/dialog_exe_helpers.h"   // B3R: Dialog_FindComponent_d6c0
#include "Work/exe_bodies.h"             // B3R: WorkBase_FindComponent_2ed0
#include "Action/exe_bodies.h"           // B3R: Pf_RecursiveChildSum_123070
#include "Object/internal_factory_audit.h"   // ObjAudit::IsRegistered (stale weak-ref guard)
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/VSlot.h"   // B.3.x W0: Object::SlotAddressOf (the [JobArrow] probe)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/LabelElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/WorkPlaceComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Component/WorkDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/exe_bodies.h"   // B3R: CitizenEmploy_Strat_*

// Original FUN_14000d6c0 — FindComponent with type DAT_1401c7008 (guard check only):
// Dialog_FindComponent_d6c0. Original FUN_140002ed0 — FindComponent with type
// DAT_1401c7900 (ResidenceComponent), what gets passed to FUN_140123070
// (GetOccupantCount): WorkBase_FindComponent_2ed0. Both called by name (B3R).

// ============================================================
// WorkplaceUI_AdjustWorkerCount (0x140087610)
// ============================================================
// Adjusts the max worker count on the entity's WorkplaceComponent by delta.
// Finds the WorkplaceComponent on the entity at panel+0x28, then finds
// the StorageLocationComponent on workplaceComp+0x10's entity (inner entity),
// lazy-initializes its cached max count at +0x30, then calls SetMaxWorkers
// with (cachedMax + delta).
//
// param_1 = WorkplaceUI* (self), param_2 = delta (+1 or -1)
static void WorkplaceUI_AdjustWorkerCount_Impl(void* self, int delta)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity from panel+0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find the WorkPlaceComponent (FUN_140002e10 — type DAT_1401c80e0)
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace((void*)entityPtr, &workComp);
    uintptr_t wc = workComp;

    // Find StorageLocationComponent on the inner entity at workComp+0x10
    uintptr_t innerEntity = *(uintptr_t*)(wc + 0x10);
    uint64_t slComp = 0;
    fn_FindComponent_Work((void*)innerEntity, &slComp);
    uintptr_t slc = slComp;

    // Lazy-init cached max count at slComp+0x30
    if (*(int*)(slc + 0x30) == 0) {
        int count = fn_StorageCountQuery((void*)slc);
        *(int*)(slc + 0x30) = count;
    }
    int currentMax = *(int*)(slc + 0x30);

    // Set new max workers on the workplace component
    WorkPlaceComponent_SetMaxWorkers(static_cast<int64_t>(wc), currentMax + delta);   // 0x14012fb00
}

// ============================================================
// WorkplaceUI_ToggleWorkplace (0x1400876d0)
// ============================================================
// Cycles through the workplace list, advancing the panel's current
// workplace index (at panel+0x60) and centering the camera on it.
// WorkplaceComponent at workComp+0x58 = workplace ptr array,
// workComp+0x60 = workplace count.
//
// param_1 = WorkplaceUI* (self)
static void WorkplaceUI_ToggleWorkplace_Impl(void* self)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity from panel+0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find the WorkComponent (FUN_140003050 — type DAT_1401c8070)
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace((void*)entityPtr, &workComp);
    uintptr_t wc = workComp;

    // Check if any workplaces exist
    if (*(int*)(wc + 0x60) == 0) return;

    // Advance panel's workplace index, wrap around
    *(int*)(s + 0x60) += 1;
    if (*(int*)(wc + 0x60) <= *(int*)(s + 0x60)) {
        *(int*)(s + 0x60) = 0;
    }

    // Get the workplace entity at workplaceArray[panelIndex]
    uintptr_t workplaceArrayBase = *(uintptr_t*)(wc + 0x58);
    uint64_t workplaceRef = *(uint64_t*)(workplaceArrayBase + (uintptr_t)*(int*)(s + 0x60) * 8);

    // Get inner entity ptr from workplaceRef+0x10
    uint64_t workplaceEntityInner = *(uint64_t*)(workplaceRef + 0x10);

    // Center camera on workplace entity
    void* gameWorld = GameUpdateLoop::_singleton;
    void* subsys = *(void**)((uintptr_t)gameWorld + 0x120);
    uint64_t parentRef = *(uint64_t*)(s + 0x18);
    Toolbar_CenterOnEntity_Impl(subsys, &parentRef, &workplaceEntityInner);
}

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    System::String nameStr(name);

    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge

    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    return *(uint64_t*)basePtrBuf;
}

namespace workplaceui_sa {

// ENDGAME B.3.9 CG2: TypeId_ComponentEntity -> ComponentEntity::TypeId(),
// TypeId_WorkplaceComponent -> WorkPlaceComponent::TypeId() (cell 0x1401c80e0).
// Both class statics always answer, so the lazy-fill retires (and the empty
// EnsureGlobals that was left went with its calls, B3R).

bool Ready() {
    return true;
}

// Faithful visibility subset of WorkplaceUI_Startup (0x140087080): show the
// workPlace panel only while the building has an active workplace profession.
void UpdateVisibility(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    void* panelElem = *reinterpret_cast<void**>(s + 0x20);
    if (!panelElem) return;  // controller not bound to its element yet

    // Resolve the bound entity (data source at +0x28).
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // Find the WorkPlaceComponent (fn_FindComponent_WorkPlace → TypeId_WorkplaceComponent).
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace(entityPtr, &workComp);

    // Show only when the workplace has an active profession (+0x48 != 0); hidden
    // on a completed house/barn/pile whose construction workplace has cleared it.
    bool show = workComp != 0 &&
                *reinterpret_cast<uintptr_t*>(static_cast<uintptr_t>(workComp) + 0x48) != 0;

    reinterpret_cast<UI::Element*>(panelElem)->SetFlagsOnOff(show ? 0u : 1u, show ? 1u : 0u);  // B.3.6 bridge
}

// ------------------------------------------------------------------
// Count population — faithful subset of WorkplaceUI_Startup (0x140087080).
// ------------------------------------------------------------------
// The SA promotion originally ported ONLY the show/hide decision, so the
// workPlace panel's "X of Y" label (Dialog/Work.rsc:workPlaceCount) and the
// profession-count label (maxCount) stayed at their .rsc placeholders — the
// user-visible "builder count is blank" bug. Now that Builders are really seated
// (WorkPlaceComponent+0x60 roster grows via the natural builder flow), reproduce
// the retail count writes, which are all SA-safe (FindElement / TextElement::
// SetParameter / String::Format resolve; the exe-helper laborer/button-enable
// tail is deliberately still skipped — those buttons aren't wired in SA and that
// path calls FUN_14000d6c0 which resolves NULL in standalone):
//
//   workPlaceCount (self+0x30): profession sprite + param0 = roster (WPC+0x60,
//       seated workers/builders at THIS site) + param1 = capacity (the site's
//       WorkComponent+0x30 = max slots, lazily CalcMaxWorkers'd).
//   maxCount (self+0x38): param0 = town-wide count of citizens assigned to this
//       profession (the "Builder count live in the town" side of the ask).
//
// Runs for any workplace panel, so the count is correct on every building dialog,
// not just under-construction sites. `workComp` here is the WorkPlaceComponent
// (fn_FindComponent_WorkPlace → TypeId_WorkplaceComponent), matching retail's
// local_res18 — its +0x48 is the active profession, +0x60 the live roster.
// One-shot recursive element-tree dump: name (+0x10 = char* with length at [-8]),
// id (+0x88), child array (+0xa0) / count (+0xa8). Reveals the real panel structure so
// we can see which element carries the click and its assigned id.
void DumpTree(void* elem, int depth) {
    if (!elem || depth > 6) return;
    uintptr_t e = reinterpret_cast<uintptr_t>(elem);
    char* nm = *reinterpret_cast<char**>(e + 0x10);
    int id  = *reinterpret_cast<int*>(e + 0x88);
    int cc  = *reinterpret_cast<int*>(e + 0xa8);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0xa0);
    fprintf(stderr, "[Tree] %*selem=%p name='%s' id=0x%x children=%d\n",
            depth * 2, "", elem, nm ? nm : "", id, cc);
    if (cc > 0 && cc <= 64 && arr)
        for (int i = 0; i < cc; ++i) {
            void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
            if (c) DumpTree(c, depth + 1);
        }
}

void PopulateCounts(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    void* panelElem = *reinterpret_cast<void**>(s + 0x20);
    if (!panelElem) return;

    // Bound entity (data source at +0x28).
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId())) return;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // WorkPlaceComponent (retail local_res18).
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace(entityPtr, &workComp);
    if (workComp == 0) return;
    uintptr_t w = static_cast<uintptr_t>(workComp);

    void* profession = *reinterpret_cast<void**>(w + 0x48);
    if (!profession) return;  // no active profession → panel hidden, nothing to show

    // Discover the two count elements once (they don't move); cache in the same
    // fields retail's Serialize uses (self+0x30 workPlaceCount, +0x38 maxCount).
    if (*reinterpret_cast<void**>(s + 0x30) == nullptr)
        *reinterpret_cast<uint64_t*>(s + 0x30) = FindElemAsBasePtr(panelElem, "workPlaceCount");
    if (*reinterpret_cast<void**>(s + 0x38) == nullptr)
        *reinterpret_cast<uint64_t*>(s + 0x38) = FindElemAsBasePtr(panelElem, "maxCount");
    void* elem30 = *reinterpret_cast<void**>(s + 0x30);   // workPlaceCount
    void* elem38 = *reinterpret_cast<void**>(s + 0x38);   // maxCount

    // Arm ALL FIVE interactive controls so their clicks route to OnRemove (vtable slot 9)
    // with the retail ids — matching WorkplaceUI_Serialize (0x140087940), which the SA
    // promotion skips. The panel has TWO spinners plus the cycle button, and the ORIGINAL
    // SA bug was arming only workerCountUp/Down: the visible spinner is jobsUpDown
    // (jobCountUp/Down) and the profession-icon cycle is workPlaceButton, so those clicks
    // arrived with element-id -1 and OnRemove ignored them. Element-id lives at +0x88; the
    // named ButtonElement is itself the element that fires (verified via a per-element id
    // stamp — the click carried the parent button's id, not a child's), so setting the id
    // on the found element is sufficient (no child recursion needed).
    //   +0x40 workerCountUp 0x1450   +0x48 workerCountDown 0x1451   (assign/unassign worker)
    //   +0x50 jobCountUp    0x1452   +0x58 jobCountDown    0x1453   (raise/lower job slots)
    //   workPlaceButton     0x1454   (cycle workplaces / center camera)
    struct ArmOne {
        static void go(uintptr_t self, int off, void* panel, const char* name, int id) {
            void** slot = reinterpret_cast<void**>(self + off);
            if (*slot != nullptr) return;
            *reinterpret_cast<uint64_t*>(self + off) = FindElemAsBasePtr(panel, name);
            void* e = *slot;
            if (e) reinterpret_cast<UI::Element*>(e)->SetElementId(id);  // B.3.6 bridge
        }
    };
    ArmOne::go(s, 0x40, panelElem, "workerCountUp",   0x1450);
    ArmOne::go(s, 0x48, panelElem, "workerCountDown", 0x1451);
    ArmOne::go(s, 0x50, panelElem, "jobCountUp",      0x1452);
    ArmOne::go(s, 0x58, panelElem, "jobCountDown",    0x1453);
    // workPlaceButton has no dedicated self slot in retail; find+arm it directly each pass
    // until it takes (idempotent — SetElementId just writes +0x88).
    {
        void* wpb = reinterpret_cast<void*>(FindElemAsBasePtr(panelElem, "workPlaceButton"));
        if (wpb &&
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(wpb) + 0x88) != 0x1454)
            reinterpret_cast<UI::Element*>(wpb)->SetElementId(0x1454);  // B.3.6 bridge
    }

    // Roster = seated workers/builders at this site (WorkPlaceComponent+0x60).
    uint32_t roster = *reinterpret_cast<uint32_t*>(w + 0x60);

    // Capacity = the site's WorkComponent (StorageLocationComponent, +0x30). Retail
    // resolves it on the WPC's parent entity (WPC+0x10) and lazily CalcMaxWorkers's
    // the +0x30 cache. The natural-builder flow (FindAvailableBuilding) usually has
    // already primed +0x30, so it is read directly; StorageCountQuery only fills it
    // when still zero. ENDGAME B.3.9 CG2: the "only when its type-id deps are live"
    // TypeId_WorkDescription readiness gate retired — WorkDescription::TypeId()
    // (the class static) always answers.
    uint32_t capacity = 0;
    {
        void* parentEntity = *reinterpret_cast<void**>(w + 0x10);
        uint64_t workC = 0;
        fn_FindComponent_Work(parentEntity, &workC);
        if (workC) {
            uintptr_t c = static_cast<uintptr_t>(workC);
            if (*reinterpret_cast<int*>(c + 0x30) == 0) {
                int n = fn_StorageCountQuery(reinterpret_cast<void*>(c));
                *reinterpret_cast<int*>(c + 0x30) = n;
            }
            capacity = *reinterpret_cast<uint32_t*>(c + 0x30);
        }
    }

    // workPlaceCount: profession sprite + "roster / capacity".
    if (elem30) {
        uint64_t sheet = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(profession) + 0x28);
        if (sheet) {
            uint8_t spriteBuf[16];
            ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(sheet)),
                           *reinterpret_cast<const System::String*>(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(profession) + 0x30)));  // B.3.6 bridge
            reinterpret_cast<UI::LabelElement*>(elem30)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge
        }
        {
            System::String fmt1 = System::String::Format("%d", static_cast<uint64_t>(roster));
            reinterpret_cast<UI::TextElement*>(elem30)->SetParameter(0, fmt1);  // B.3.6 bridge
            System::String fmt2 = System::String::Format("%d", static_cast<uint64_t>(capacity));
            reinterpret_cast<UI::TextElement*>(elem30)->SetParameter(1, fmt2);  // B.3.6 bridge
        }
    }

    // maxCount: town-wide count of adult, unassigned-task citizens whose profession
    // (citizen+0x1f0) matches this workplace's profession — the live "N builders in
    // the town" figure. Pure field reads over the citizen list (no exe helpers).
    if (elem38) {
        int assigned = 0;
        void* gw = GameUpdateLoop::_singleton;
        void* citizenList = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xf0) : nullptr;
        if (citizenList) {
            int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(citizenList) + 0xa8);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(citizenList) + 0xa0);
            for (int i = 0; i < count && arr; ++i) {
                uintptr_t citizen = *reinterpret_cast<uintptr_t*>(arr + static_cast<uintptr_t>(i) * 8);
                if (!citizen) continue;
                // CONTAINMENT, not a fix. CheckPtr is an EMPTY function in the shipping
                // Runtime (see internal_factory_audit.h): a stale weak ref in the town's
                // citizen array never self-invalidates, because retail clears refs
                // explicitly at removal — which our removal path does not yet do
                // everywhere. Without this, a freed-and-reused block reads garbage at
                // +0x288 and faults on desc+0x44 (live crash 2026-07-25: opening a
                // building panel under LIVE_DESTROY entity churn). Validate against the
                // InternalFactory handle table before dereferencing. The stale entry
                // itself still needs fixing at the removal site.
                if (!ObjAudit::IsRegistered(reinterpret_cast<const void*>(citizen))) continue;
                void* citizenDesc = *reinterpret_cast<void**>(citizen + 0x288);
                if (!citizenDesc) continue;
                // Adult (age > adult-age) AND no active task (citizen+0x208 == 0).
                if (*reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(citizenDesc) + 0x44)
                        > *reinterpret_cast<float*>(citizen + 0x3c)) continue;
                if (*reinterpret_cast<uintptr_t*>(citizen + 0x208) != 0) continue;
                if (*reinterpret_cast<void**>(citizen + 0x1f0) == profession) ++assigned;
            }
        }
        System::String fmt = System::String::Format("%d", static_cast<uint64_t>(static_cast<uint32_t>(assigned)));
        reinterpret_cast<UI::TextElement*>(elem38)->SetParameter(0, fmt);  // B.3.6 bridge
    }

}

// ------------------------------------------------------------------
// WorkPlaceComponent::SetMaxWorkers (0x14012fb00) — retail WorkPlaceUI_helper_fb00
// ------------------------------------------------------------------
// NOT reimplemented here: the address is already ported and registered
// (WorkPlaceComponent_SetMaxWorkers in code/Game/Component/destroycomponent_workerrelease.cpp,
// registered by standalone/reg_destroy_workers.cpp), so fn_WorkplaceComp_SetMaxWorkers
// resolves to the real body. Its header comment already anticipated exactly this caller:
// "the OTHER retail caller, WorkPlaceUI_AdjustWorkerCount (0x140087610), reaches it through
// fn_WorkplaceComp_SetMaxWorkers ... not on a live path today". Wiring the jobCount spinner
// to it is what puts it on that live path, which is retail's own structure (7610 -> fb00).
// It writes WorkComponent+0x30 = newMax and, while WPC+0x60 > newMax, re-homes roster[0]
// through sub_ffc0/sub_0470 + AssignHousing; it holds g_wpcRosterOnly across that loop
// itself, so this caller must not (and does not) duplicate the guard.

// ------------------------------------------------------------------
// jobCountUp / jobCountDown body — retail WorkPlaceUI_Update_sub_7610 (0x140087610)
// ------------------------------------------------------------------
//   wpc = FindWorkPlaceComponent(entity);                       // FUN_140002e10
//   wc  = FindWorkComponent(*(wpc + 0x10));                     // FUN_140003050
//   if (wc->+0x30 == 0) wc->+0x30 = CalcStartingJobCount(wc);   // FUN_14012e5c0
//   SetMaxWorkers(wpc, wc->+0x30 + delta);                      // 0x14012fb00
// No clamping here — retail bounds the player with the button-enable tail in Startup
// (0x1400874c0..0x1400875e0), which IS ported and live: WorkplaceUI_Startup at the top of
// this file computes `wpMaxCount` from WorkComponent+0x30 and drives the two arrows'
// enable state (up: wpMaxCount < WorkDescription+0x2c _maxWorkerCount; down: wpMaxCount >
// +0x28 _minWorkerCount, or > 1 on the special profession branch), and W_OnUpdate/W_Startup
// call it every frame. So the spinner is bounded by
// retail's own mechanism — a greyed arrow — not by a clamp bolted onto the handler.
static void SA_AdjustJobCount(void* self, int delta) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId())) return;
    uint8_t eb[8];
    new (eb) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    void* entityPtr = *reinterpret_cast<void**>(eb);

    uint64_t wpc = 0;
    fn_FindComponent_WorkPlace(entityPtr, &wpc);
    // SA deviation (named): retail's 0x140087610 derefs both of these unguarded, because the
    // workPlace panel is only ever instanced for a real workplace entity. In SA the panel
    // template is shared and can be bound to an entity without a WorkPlace/Work component,
    // so bail with a logged reason instead of faulting on a binding bug we already know about.
    if (!wpc) {
        fprintf(stderr, "[WorkPlaceUI] jobCount delta=%+d ABORTED: no WorkPlaceComponent on "
                        "entity=%p\n", delta, entityPtr);
        fflush(stderr);
        return;
    }

    uint64_t workC = 0;
    fn_FindComponent_Work(
        *reinterpret_cast<void**>(static_cast<uintptr_t>(wpc) + 0x10), &workC);
    if (!workC) {   // same named SA deviation as the wpc bail above
        fprintf(stderr, "[WorkPlaceUI] jobCount delta=%+d ABORTED: no WorkComponent on wpc=0x%llx\n",
                delta, static_cast<unsigned long long>(wpc));
        fflush(stderr);
        return;
    }
    uintptr_t c = static_cast<uintptr_t>(workC);
    // ENDGAME B.3.9 CG2: TypeId_WorkDescription readiness gate retired —
    // WorkDescription::TypeId() (the class static) always answers.
    if (*reinterpret_cast<int*>(c + 0x30) == 0) {
        *reinterpret_cast<int*>(c + 0x30) = fn_StorageCountQuery(reinterpret_cast<void*>(c));
    }
    const int cur = *reinterpret_cast<int*>(c + 0x30);

    // No g_wpcRosterOnly wrapper here: the ported SetMaxWorkers body holds that guard across
    // its own eviction loop (destroycomponent_workerrelease.cpp), so setting it again would
    // only obscure which layer owns it.
    const int rosterBefore = *reinterpret_cast<int*>(static_cast<uintptr_t>(wpc) + 0x60);
    WorkPlaceComponent_SetMaxWorkers(static_cast<int64_t>(wpc), cur + delta);   // 0x14012fb00

    // Report the OUTCOME in both arms: the observed before/after of WorkComponent+0x30 (the
    // field this whole open is about) and of the roster, or the reason nothing happened.
    fprintf(stderr, "[WorkPlaceUI] jobCount delta=%+d %s WorkComponent+0x30 %d->%d (asked %d) "
                    "roster %d->%d\n",
            delta, "applied",
            cur, *reinterpret_cast<int*>(c + 0x30), cur + delta,
            rosterBefore, *reinterpret_cast<int*>(static_cast<uintptr_t>(wpc) + 0x60));
    fflush(stderr);
}




}  // namespace workplaceui_sa

// ============================================================
// WorkplaceUI::Serialize (vtable +0x20) — Setup/refresh
// ============================================================
// Finds 6 named elements in the dialog, stores them, sets
// element IDs on the 4 buttons plus the workPlaceButton.
// Checks entity component to conditionally hide job count buttons.
//
// Original: 0x140087940.

// [4] 0x140087940
void WorkPlaceUI::OnCreated() {
    if (!workplaceui_sa::Ready() || !SA_DialogEntityAlive(this)) return;
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // --- Find and store named elements ---
    this->data30 = FindElemAsBasePtr(dialogElem, "workPlaceCount");
    this->data38 = FindElemAsBasePtr(dialogElem, "maxCount");
    this->data40 = FindElemAsBasePtr(dialogElem, "workerCountUp");
    this->data48 = FindElemAsBasePtr(dialogElem, "workerCountDown");
    this->data50 = FindElemAsBasePtr(dialogElem, "jobCountUp");
    this->data58 = FindElemAsBasePtr(dialogElem, "jobCountDown");

    // --- Set element IDs on buttons ---
    reinterpret_cast<UI::Element*>(this->data40)->SetElementId(0x1450);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(this->data48)->SetElementId(0x1451);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(this->data50)->SetElementId(0x1452);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(this->data58)->SetElementId(0x1453);  // B.3.6 bridge

    // Find and set workPlaceButton element ID
    uint64_t workplaceBtn = FindElemAsBasePtr(dialogElem, "workPlaceButton");
    reinterpret_cast<UI::Element*>(workplaceBtn)->SetElementId(0x1454);  // B.3.6 bridge

    // --- Check entity component for job count visibility ---
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    // Find the WorkComponent (FUN_140003050 — type DAT_1401c8070)
    void* workComp = nullptr;
    fn_FindComponent_Work(entityInternal, &workComp);

    // Check the WorkComponent's WorkDescription at +0x18 (DAT_1401c80a8)
    //
    // DECLARED DEVIATION (2026-08-15, when idx4 was first wired into the vtable).
    // Retail derefs workComp unguarded here because a WorkPlaceUI is only ever built
    // over an entity that carries a WorkComponent. Ours is not: the workPlace panel is
    // part of a SHARED dialog template that the framework instantiates for every
    // selectable building, so this pass also runs over houses, barns and piles that
    // legitimately have no WorkComponent — the case CLAUDE.md names as the one where a
    // guard belongs. The buttons found above keep their .rsc default state, which is
    // exactly what the un-wired idx4 left them at, so skipping here is a strict no-op
    // for those buildings rather than a behaviour change.
    if (!workComp) return;
    void* subComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = ::Object::IsDerivedFromNullSafe(subComp, WorkDescription::TypeId());

    if (isJobType && *(uint8_t*)((uintptr_t)subComp + 0x20) == 0) {
        // Hide job count up/down buttons if sub-component flag is zero
        reinterpret_cast<UI::Element*>(this->data50)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(this->data58)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
    }
}

// ============================================================
// WorkplaceUI::Startup (vtable +0x38) — Update worker counts
// ============================================================
// Queries entity components to display worker count, max count,
// and enables/disables worker up/down and job up/down buttons
// based on current state.
//
// Original: 0x140087080.

// [7] 0x140087080
void WorkPlaceUI::OnUpdateLayout() {
    if (!workplaceui_sa::Ready() || !SA_DialogEntityAlive(this)) return;
    workplaceui_sa::UpdateVisibility(this);
    workplaceui_sa::PopulateCounts(this);
    uintptr_t s = (uintptr_t)this;

    // Get entity from data source
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    void* entityPtr = *(void**)entityBuf;

    // Find the WorkPlaceComponent (FUN_140002e10 — type DAT_1401c80e0). Retail
    // also runs a discarded IsDerivedFrom(wpComp+0x18, WorkPlaceDescription) here.
    void* wpComp = nullptr;
    fn_FindComponent_WorkPlace(entityPtr, &wpComp);

    // Find the WorkComponent (FUN_140003050 — type DAT_1401c8070)
    void* workComp = nullptr;
    fn_FindComponent_Work(entityPtr, &workComp);

    // Get the WorkDescription off the WorkComponent: +0x18
    void* jobSubComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = ::Object::IsDerivedFromNullSafe(jobSubComp, WorkDescription::TypeId());
    void* jobType = isJobType ? jobSubComp : nullptr;

    // Check if building is complete: wpComp +0x48 (profession ptr) must be non-null
    if (*(uintptr_t*)((uintptr_t)wpComp + 0x48) == 0) {
        // Building not complete — hide the entire panel
#ifdef _DEBUG
        printf("[WorkPlaceUI] HIDDEN: wpComp=%p profession(+0x48)=NULL entity=%p\n", wpComp, entityPtr);
#endif
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        return;
    }

    // Building complete — show the panel
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->SetFlagsOnOff(0, 1);  // B.3.6 bridge

#ifdef _DEBUG
    printf("[WorkPlaceUI] entity=%p wpComp=%p workComp=%p jobType=%p jt+0x20=%d prof=%p\n",
        entityPtr, wpComp, workComp, jobType,
        jobType ? (int)*(uint8_t*)((uintptr_t)jobType + 0x20) : -1,
        *(void**)((uintptr_t)wpComp + 0x48));
#endif
    if (*(uint8_t*)((uintptr_t)jobType + 0x20) == 0) {
        // Check for laborer profession
        void* professionId = *(void**)((uintptr_t)wpComp + 0x48);
        void* profList = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0xb0);
        void* laborerProf = *(void**)((uintptr_t)profList + 0x38);

        if (professionId == laborerProf) {
            // Guard check: FUN_14000d6c0 (type DAT_1401c7008)
            uint64_t posCheck = 0;
            Dialog_FindComponent_d6c0(*(void**)entityBuf, &posCheck);   // 0x14000d6c0
            if (posCheck != 0) goto showUpDown;
        }

        // Hide job count up/down
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x50))->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x58))->SetFlagsOnOff(1, 0);  // B.3.6 bridge
    } else {
showUpDown:
        // Show job count up/down
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x50))->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x58))->SetFlagsOnOff(0, 1);  // B.3.6 bridge
    }

    // Get worker count and max worker count
    uint32_t workerCount = *(uint32_t*)((uintptr_t)wpComp + 0x60);

    // Get the WorkComponent from the WorkPlaceComponent's parent entity (+0x10)
    void* parentEntity = *(void**)((uintptr_t)wpComp + 0x10);
    void* parentWorkComp = nullptr;
    fn_FindComponent_Work(parentEntity, &parentWorkComp);

    // Get max count — lazy-init cached value at +0x30
    if (*(int*)((uintptr_t)parentWorkComp + 0x30) == 0) {
        int count = fn_StorageCountQuery(parentWorkComp);
        *(int*)((uintptr_t)parentWorkComp + 0x30) = count;
    }
    uint32_t maxCount = *(uint32_t*)((uintptr_t)parentWorkComp + 0x30);

    // Get profession sprite from wpComp +0x48 -> +0x28 (sheet) / +0x30 (name)
    {
        uintptr_t profPtr = *(uintptr_t*)((uintptr_t)wpComp + 0x48);
        uint8_t spriteBuf[16];
        ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(profPtr + 0x28))),
                        *reinterpret_cast<const System::String*>(profPtr + 0x30));  // B.3.6 bridge
        reinterpret_cast<UI::LabelElement*>(*(void**)(s + 0x30))->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge
    }

    // Set worker count text parameter 0 on workPlaceCount
    {
        System::String fmtStr = System::String::Format("%d", workerCount);
        reinterpret_cast<UI::TextElement*>(*(void**)(s + 0x30))->SetParameter(0, fmtStr);  // B.3.6 bridge
    }

    // Set max count text parameter 1 on workPlaceCount
    {
        System::String fmtStr = System::String::Format("%d", maxCount);
        reinterpret_cast<UI::TextElement*>(*(void**)(s + 0x30))->SetParameter(1, fmtStr);  // B.3.6 bridge
    }

    // Iterate citizens to count workers assigned to this workplace
    void* citizenList = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0xf0);
    void* profList = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0xb0);
    void* laborerProf = *(void**)((uintptr_t)profList + 0x30);  // laborer entry

    int workplaceWorkerCount = 0;
    int laborerCount = 0;

    int citizenCount = *(int*)((uintptr_t)citizenList + 0xa8);
    for (int i = 0; i < citizenCount; i++) {
        uintptr_t citizen = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)citizenList + 0xa0) + i * 8);

        // Check if citizen is adult and has profession assigned
        void* citizenProf = *(void**)((uintptr_t)citizen + 0x288);
        // Retail 0x1400873d2: COMISS XMM0,[RCX+0x44] / JC — CF is set only when
        // XMM0 is strictly BELOW, so the skip is `<`, not `<=`. A citizen sitting
        // exactly on the adulthood threshold counts as an adult in retail and was
        // being dropped here.
        const float citAge = *(float*)((uintptr_t)citizen + 0x3c);
        const float adultAge = *(float*)((uintptr_t)citizenProf + 0x44);
        const uintptr_t gate208 = *(uintptr_t*)((uintptr_t)citizen + 0x208);
        if (citAge < adultAge) continue;
        if (gate208 != 0) continue;

        // Check if assigned to this workplace's profession
        void* assignedProf = *(void**)((uintptr_t)citizen + 0x1f0);
        if (assignedProf == *(void**)((uintptr_t)wpComp + 0x48)) {
            workplaceWorkerCount++;
        }
        if (assignedProf == laborerProf) {
            laborerCount++;
        }
    }

    // Set assigned worker count on buttonPrevious (+0x38) text
    {
        System::String fmtStr = System::String::Format("%d", workplaceWorkerCount);
        reinterpret_cast<UI::TextElement*>(*(void**)(s + 0x38))->SetParameter(0, fmtStr);  // B.3.6 bridge
    }

    // Enable/disable workerCountUp (+0x40) based on laborer count > 0
    {
        void* btnUp = *(void**)(s + 0x40);
        reinterpret_cast<UI::Element*>(btnUp)->Enable(laborerCount > 0);  // B.3.6 bridge: Element slot 8 Enable
    }

    // Enable/disable workerCountDown (+0x48) based on workers assigned > 0
    {
        void* btnDown = *(void**)(s + 0x48);
        reinterpret_cast<UI::Element*>(btnDown)->Enable((int)workplaceWorkerCount > 0);  // B.3.6 bridge: Element slot 8 Enable
    }

    // Job count up/down enable based on workplace capacity
    // Use workComp (from main entity), NOT parentWorkComp (from inner entity)
    if (*(int*)((uintptr_t)workComp + 0x30) == 0) {
        int count = fn_StorageCountQuery(workComp);
        *(int*)((uintptr_t)workComp + 0x30) = count;
    }
    int wpMaxCount = *(int*)((uintptr_t)workComp + 0x30);

    // stderr, NOT stdout, and no longer _DEBUG-only. stdout stalls in this build (the
    // debug log froze at 3.5 KB while stderr ran the whole session), so this line has
    // been invisible for its entire life — a diagnostic that produces nothing is
    // indistinguishable from one that never ran. It is the discriminator for
    // "6/6 assigned but no citizens available": `workers` comes from WPC+0x60 while
    // `assigned`/`laborers` come from the citizen loop, so if the panel shows 6/6 and
    // this prints assigned=0 laborers=0, the loop is what is broken, not the roster.
    fprintf(stderr,
        "[WorkPlaceUI] workers=%u/%u assigned=%d laborers=%d wpMax=%d citizens=%d "
        "wpProf=%p laborProf30=%p jt+0x28=%d jt+0x2c=%d jt+0x34=%d\n",
        workerCount, maxCount, workplaceWorkerCount, laborerCount, wpMaxCount,
        citizenCount, *(void**)((uintptr_t)wpComp + 0x48), laborerProf,
        jobType ? *(int*)((uintptr_t)jobType + 0x28) : -1,
        jobType ? *(int*)((uintptr_t)jobType + 0x2c) : -1,
        jobType ? (int)*(uint8_t*)((uintptr_t)jobType + 0x34) : -1);
    fflush(stderr);

    // Check if this is a laborer profession
    if (*(void**)((uintptr_t)wpComp + 0x48) ==
        *(void**)((uintptr_t)profList + 0x38)) {
        // Laborer building — check position component for position-based limit
        // Guard check: FUN_14000d6c0 (type DAT_1401c7008)
        uint64_t posGuard = 0;
        Dialog_FindComponent_d6c0(*(void**)entityBuf, &posGuard);   // 0x14000d6c0
        if (posGuard != 0) {
            int maxCap = *(int*)((uintptr_t)jobType + 0x2c);
            // Get ResidenceComponent via FUN_140002ed0 (type DAT_1401c7900)
            uint64_t resIBP = 0;
            WorkBase_FindComponent_2ed0(*(uintptr_t*)entityBuf, &resIBP);   // 0x140002ed0
            // 0x140123070 Pf_RecursiveChildSum_123070 (door misnamed
            // fn_ResidenceComponent_GetOccupantCount)
            int posCount = Pf_RecursiveChildSum_123070(static_cast<int64_t>(resIBP), 1);
            // Use min(jobType max, position count) as the real limit
            if (posCount < maxCap) {
                uint64_t resIBP2 = 0;
                WorkBase_FindComponent_2ed0(*(uintptr_t*)entityBuf, &resIBP2);   // 0x140002ed0
                maxCap = Pf_RecursiveChildSum_123070(static_cast<int64_t>(resIBP2), 1);   // 0x140123070
            }
            // Enable up if current max < capacity limit
            void* btnUp = *(void**)(s + 0x50);
            reinterpret_cast<UI::Element*>(btnUp)->Enable(wpMaxCount < maxCap);  // B.3.6 bridge: Element slot 8 Enable

            // Enable down if current max > 1
            void* btnDown = *(void**)(s + 0x58);
            reinterpret_cast<UI::Element*>(btnDown)->Enable(wpMaxCount > 1);  // B.3.6 bridge: Element slot 8 Enable
            return;
        }
    }

    // Default job count button logic
    int jobMinCount = *(int*)((uintptr_t)jobType + 0x28);

    // TEMPORARY INSTRUMENTATION (2026-08-15). The hunting cabin's job up-arrow is
    // greyed out in the UI, so a laborer can never be assigned to the hunt work class
    // organically -- the lodge is only ever staffed by the FOOD_WORKERS harness. The
    // up-arrow's ONLY enable condition is wpMaxCount < jobType+0x2c, so a zero (or
    // wrong-offset) max greys it permanently. Print BOTH sides of that comparison plus
    // the predicate's outcome, not just the inputs. NOTE engine_funcs.h's
    // WorkDescription field names are known-wrong, so +0x28/+0x2c may not be min/max.
    {
        static long long s_n = 0;
        if ((++s_n % 120) == 1) {
            const int jobMax = *(int*)((uintptr_t)jobType + 0x2c);
            // The predicate measured TRUE (wpMax=3 < max=4) yet the player still sees a
            // greyed arrow, so the enable VALUE is not the problem -- the next suspect is
            // the call that carries it. Report the receiver of each arrow's vtable[8] so
            // an unwired / stub slot 8 (which would swallow the enable and leave every
            // arrow rendering greyed) is distinguishable from a genuine disable. Symbolize
            // these with nm against standalone/build/reimpl-x64.exe.
            void* bU = *(void**)(s + 0x50); void* bD = *(void**)(s + 0x58);
            void* bWU = *(void**)(s + 0x40); void* bWD = *(void**)(s + 0x48);
            // The slot comes from the member, never a literal index (B3X_RULES X3).
            auto enableSlot = [](void* b) -> void* {
                return b ? Object::SlotAddressOf(b, &UI::Element::Enable) : nullptr;
            };
            std::fprintf(stderr, "[JobArrow] jobType=%p wpMaxCount=%d min(+0x28)=%d "
                         "max(+0x2c)=%d -> upEnabled=%d downEnabled=%d | "
                         "jobUp=%p vt=%p Enable=%p | jobDown=%p Enable=%p | "
                         "wkUp=%p Enable=%p | wkDown=%p Enable=%p\n",
                         (void*)jobType, wpMaxCount, jobMinCount, jobMax,
                         wpMaxCount < jobMax ? 1 : 0,
                         (wpMaxCount != jobMinCount && wpMaxCount > jobMinCount) ? 1 : 0,
                         bU, bU ? *(void**)bU : nullptr, enableSlot(bU),
                         bD, enableSlot(bD),
                         bWU, enableSlot(bWU),
                         bWD, enableSlot(bWD));
            std::fflush(stderr);
        }
    }
    {
        void* btnUp = *(void**)(s + 0x50);
        reinterpret_cast<UI::Element*>(btnUp)->Enable(wpMaxCount < *(int*)((uintptr_t)jobType + 0x2c));  // B.3.6 bridge: Element slot 8 Enable
    }
    {
        void* btnDown = *(void**)(s + 0x58);
        reinterpret_cast<UI::Element*>(btnDown)->Enable(wpMaxCount != jobMinCount && wpMaxCount > jobMinCount);  // B.3.6 bridge: Element slot 8 Enable
    }
}

// NOTE - the older duplicate port of retail 0x1400877e0 that used to live here
// (WorkplaceUI_OnRemove) has been REMOVED. It was fully written but wired into no
// vtable, so it never executed: slot 9 is workplaceui_sa::W_OnRemove, further down
// this file. Keeping both was an active trap - a diagnostic added to the dead copy
// during the 2026-08-25 seat investigation never fired, which cost a measurement
// cycle. The live copy is also the CORRECT one: it routes jobCountUp/Down
// (0x1452/0x1453) to SetMaxWorkers, where the dead copy ran the seat cascade for
// both spinner pairs. If you need the old body, it is in git history.

// ============================================================
// STANDALONE promotion — real WorkPlaceUI controller vtable
// ============================================================
// The WorkPlaceUI panel (Dialog/Work.rsc:workPlace — the "workers X of Y" label
// with the up/down worker/job spinners) is listed in the UIDescription of every
// building that has a WorkPlaceComponent, INCLUDING non-production ones like the
// WoodHouse/StoneHouse and StorageBarn: those carry a *construction* workplace so
// citizens can be assigned to build them. Their WorkPlaceDescription has no
// profession ("no actual work done here once the building is built"), so once the
// building is complete the workplace's profession (WorkPlaceComponent+0x48) is
// null and retail's WorkplaceUI::Startup HIDES the whole panel — that is why a
// finished house / barn / stockpile shows only the center/pin/close chrome plus its
// residence/storage content, and no worker spinner.
//
// In standalone this type was a StubNop (all slots no-op, see ui_dialog_stub_
// factories.cpp — that stub registration is REMOVED there to avoid a duplicate
// REGISTER_TYPE), so Startup never ran and the construction-workplace spinner
// stayed visible on completed houses/barns. This installs a real per-class vtable
// that drives the exact show/hide decision from Startup's head:
//     show  ⇔  WorkPlaceComponent found  AND  its profession (+0x48) != 0
// i.e. hide on a built house/barn/pile (no active profession), show while it is
// still under construction and on genuine production workplaces.
//
// Only the visibility decision is ported here — the heavy worker-count population
// ⚠ STALE AS OF 2026-08-01 — the tail of Startup USED to be skipped in SA because
// FUN_14000d6c0 / FUN_140002ed0 "resolve NULL in standalone". Both are registered
// now (reg_dialog.cpp:22, reg_work_base.cpp:39) and the full Startup runs from
// W_Startup/W_OnUpdate. Skipping it was what left the worker up/down buttons
// permanently disabled. Kept here only to record why the gap existed. The user-visible requirement is
// that the panel disappears on the storage/house dialogs; a shown production panel
// keeps its .rsc default content exactly as the StubNop already left it (no
// regression). This mirrors the StorageUI/StorageCapacityUI/ResidenceUI promotions.
//
// Vtable offsets (verified from the exe WorkPlaceUI vtable @ 0x14015e030):
//   +0x00 (idx0) ScalarDtor   +0x10 (idx2) GetTypeId
//   +0x20 (idx4) Serialize (0x140087940) — element discovery (WIRED 2026-08-15)
//   +0x38 (idx7) Startup   (0x140087080) — full refresh (visibility subset in SA)
//   +0x40 (idx8)           — base DialogController::OnUpdate (per-frame). The
//                framework dispatches ONLY +0x40 each frame, so the visibility
//                refresh is driven here so it stays live as buildings complete.
//   +0x48 (idx9) OnRemove  (0x1400877e0) — button dispatch. It reads the element ids
//                idx4 stamps (0x1450..0x1454), so it only became reachable once idx4
//                was wired; before that the note here read "no-op in SA".

// Roster-only guard defined at global scope in workplacecomponent.cpp (suppresses the
// gameplay-time Scene mesh-visual block in WorkplaceComponent Attach/Detach). Declared here
// at GLOBAL scope so the reference inside the namespace binds to ::g_wpcRosterOnly.
extern bool g_wpcRosterOnly;




// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void WorkPlaceUI::OnUpdate(const System::Time& /*time*/) {
    WorkPlaceUI::OnUpdateLayout();
}

// [9] 0x1400877e0
void WorkPlaceUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t msg, int32_t id,
                                 int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (!workplaceui_sa::Ready() || !SA_DialogEntityAlive(this)) return;
    if (msg != 0x12) return;                        // button click
    if (id < 0x1450 || id > 0x1454) return;         // one of the 5 armed controls

    // workPlaceButton (0x1454): cycle through this profession's workplaces / center camera.
    // No roster mutation, so no g_wpcRosterOnly guard needed.
    if (id == 0x1454) { WorkplaceUI_ToggleWorkplace_Impl(this); return; }

    // The panel has TWO spinners, and retail (0x1400877e0) gives them DIFFERENT jobs:
    //
    //   0x1452 / 0x1453  jobCountUp/Down   -> WorkPlaceUI_Update_sub_7610(self, +/-1)
    //        raises/lowers the SLOT CAP (WorkComponent+0x30) via SetMaxWorkers; lowering
    //        evicts the surplus. It seats nobody — it only opens/closes seats.
    //   0x1450 / 0x1451  workerCountUp/Down -> FUN_14008d100 + CitizenComponent_impl_c3d0
    //        / CitizenComponent_impl_1180: the seat/unseat pair that actually converts a
    //        laborer into a worker of this profession (cbf0 -> ca20 -> c5f0 -> c4b0).
    //
    // The previous SA code ran the seat cascade for BOTH pairs, so +0x30 never moved and
    // the panel read "1/1" forever. Retail restored: jobCount now goes to SetMaxWorkers.
    // (No allocator change accompanies this: the periodic employment allocator
    // 0x1400d43b0 genuinely does skip WPC+0x60 == 0 in retail too — its Phase-2 need-table
    // is built under `if (*(int *)(wpc + 0x60) != 0)` — so retail has NO organic first-seat
    // either. The player's 0x1450 click IS the first-seat mechanism, and it is live here.)
    if (id == 0x1452 || id == 0x1453) { workplaceui_sa::SA_AdjustJobCount(this, id == 0x1452 ? 1 : -1); return; }

    // Remaining ids are the worker assign/unassign pair.
    const bool add = (id == 0x1450);

    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId())) return;
    uint8_t eb[8];
    new (eb) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    void* entityPtr = *reinterpret_cast<void**>(eb);

    uint64_t wpc = 0;
    fn_FindComponent_WorkPlace(entityPtr, &wpc);
    if (!wpc) return;
    uint64_t profId = *reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(wpc) + 0x48);
    if (!profId) return;

    void* gw = GameUpdateLoop::_singleton;
    void* citizenList = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xf0) : nullptr;
    if (!citizenList) return;

    // NO g_wpcRosterOnly HOLD HERE — see the block above idx9 for why the old one had to
    // go. This path runs at GAMEPLAY time, which is precisely when the visual toggle is
    // meant to run, so Attach/Detach do their full faithful work including the status-icon
    // clear.

    // Roster before the seat/pull, so the trace can show whether the click stuck.
    uint32_t rosterBefore = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(wpc) + 0x60);
    const char* who = "none";

    if (add) {
        // Add one worker: request a slot, then convert idle -> working -> student -> any.
        {
            uint64_t retBuf[2] = {0, 0};
            FindAvailableBuilding(reinterpret_cast<int64_t>(citizenList), retBuf, profId);   // 0x14008d100 (door misnamed fn_CitizenList_RequestWorker)
            uint64_t slot = retBuf[0];
            uint64_t slotCopy = slot;
            uint8_t fb[8] = {0};
            void* result = CitizenEmploy_Strat_cbf0(reinterpret_cast<int64_t>(citizenList), fb, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008cbf0
            int64_t found = *reinterpret_cast<int64_t*>(result);
            if (found) who = "idle";
            // Faithful to retail driver dd10 (0x14007dd10): FindWorking (Strat_ca20,
            // 0x14008ca20) unconditionally anchors *availPtr, so retail gates it behind
            // `if (uVar4 != 0)` — only call it when RequestWorker/FindAvailableBuilding
            // returned a real building (slot != 0). Without this guard, a null building
            // (e.g. a just-placed foundation not yet a staffable workplace under BUILDCTRL)
            // reaches Strat_ca20's anchor -> null deref. FindIdle(cbf0)/FindStudent(c5f0)/
            // FindAny(c4b0) all guard *availPtr==0 themselves, so they stay in the chain.
            if (found == 0 && slot != 0) {
                slotCopy = slot; uint8_t fb2[8] = {0};
                result = CitizenEmploy_Strat_ca20(reinterpret_cast<int64_t>(citizenList), fb2, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008ca20
                found = *reinterpret_cast<int64_t*>(result);
                if (found) who = "working";
            }
            if (found == 0) {
                slotCopy = slot; uint8_t fb3[8] = {0};
                result = CitizenEmploy_Strat_c5f0(reinterpret_cast<int64_t>(citizenList), fb3, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008c5f0
                found = *reinterpret_cast<int64_t*>(result);
                if (found) who = "student";
            }
            if (found == 0) {
                slotCopy = slot; uint64_t anyBuf = 0;
                CitizenEmploy_Strat_c4b0(reinterpret_cast<int64_t>(citizenList), &anyBuf, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008c4b0
                if (anyBuf) who = "any";
            }
        }
    } else {  // 0x1451 — remove one worker of this profession
        who = "remove";
        TownHall_RemoveWorker(citizenList, profId);   // 0x140091180
    }

}

// create 0x140016dc0: MemAlloc(0x68, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* WorkPlaceUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WorkPlaceUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WorkPlaceUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* WorkPlaceUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WorkPlaceUI());
}

DEFINE_EXE_TYPE(WorkPlaceUI, "UI::DialogController")

