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
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/internal_factory_audit.h"   // ObjAudit::IsRegistered (stale weak-ref guard)
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Original FUN_14000d6c0 — FindComponent with type DAT_1401c7008 (guard check only)
typedef void* (*fn_OrigFindComp_t)(void* entity, void* retval);
static fn_OrigFindComp_t fn_OrigFindCompPosition = nullptr;
// Original FUN_140002ed0 — FindComponent with type DAT_1401c7900 (ResidenceComponent)
// This is what gets passed to FUN_140123070 (GetOccupantCount)
static fn_OrigFindComp_t fn_OrigFindCompResidence = nullptr;

static void EnsureOrigFindComps() {
    if (!fn_OrigFindCompPosition)
        fn_OrigFindCompPosition = (fn_OrigFindComp_t)
            ResolveAddr(Module::Exe, 0x14000d6c0);
    if (!fn_OrigFindCompResidence)
        fn_OrigFindCompResidence = (fn_OrigFindComp_t)
            ResolveAddr(Module::Exe, 0x140002ed0);
}

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
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find the WorkPlaceComponent (FUN_140002e10 — type DAT_1401c80e0)
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace((void*)entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr((void*)&workComp);
    uintptr_t wc = workComp;

    // Find StorageLocationComponent on the inner entity at workComp+0x10
    fn_InternalBasePtr_CheckPtr((void*)(wc + 0x10));
    uintptr_t innerEntity = *(uintptr_t*)(wc + 0x10);
    uint64_t slComp = 0;
    fn_FindComponent_Work((void*)innerEntity, &slComp);
    fn_InternalBasePtr_CheckPtr((void*)&slComp);
    uintptr_t slc = slComp;

    // Lazy-init cached max count at slComp+0x30
    if (*(int*)(slc + 0x30) == 0) {
        int count = fn_StorageCountQuery((void*)slc);
        *(int*)(slc + 0x30) = count;
    }
    int currentMax = *(int*)(slc + 0x30);

    // Set new max workers on the workplace component
    fn_WorkplaceComp_SetMaxWorkers((void*)wc, currentMax + delta);
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
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find the WorkComponent (FUN_140003050 — type DAT_1401c8070)
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace((void*)entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr((void*)&workComp);
    uintptr_t wc = workComp;

    // Check if any workplaces exist
    if (*(int*)(wc + 0x60) == 0) return;

    // Advance panel's workplace index, wrap around
    *(int*)(s + 0x60) += 1;
    fn_InternalBasePtr_CheckPtr((void*)&workComp);
    if (*(int*)(wc + 0x60) <= *(int*)(s + 0x60)) {
        *(int*)(s + 0x60) = 0;
    }

    // Get the workplace entity at workplaceArray[panelIndex]
    fn_InternalBasePtr_CheckPtr((void*)&workComp);
    uintptr_t workplaceArrayBase = *(uintptr_t*)(wc + 0x58);
    uint64_t workplaceRef = *(uint64_t*)(workplaceArrayBase + (uintptr_t)*(int*)(s + 0x60) * 8);

    // Get inner entity ptr from workplaceRef+0x10
    fn_InternalBasePtr_CheckPtr((void*)&workplaceRef);
    uint64_t workplaceEntityInner = *(uint64_t*)(workplaceRef + 0x10);

    // Center camera on workplace entity
    void* gameWorld = *g_pGameWorldSingleton;
    void* subsys = *(void**)((uintptr_t)gameWorld + 0x120);
    uint64_t parentRef = *(uint64_t*)(s + 0x18);
    Toolbar_CenterOnEntity_Impl(subsys, &parentRef, &workplaceEntityInner);
}

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    System::String nameStr(name);

    uint64_t result = 0;
    fn_FindElement(dialogElem, &result, &nameStr);

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

// ============================================================
// WorkplaceUI::Serialize (vtable +0x20) — Setup/refresh
// ============================================================
// Finds 6 named elements in the dialog, stores them, sets
// element IDs on the 4 buttons plus the workPlaceButton.
// Checks entity component to conditionally hide job count buttons.
//
// Original: 0x140087940.

void WorkplaceUI_Serialize(WorkPlaceUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // --- Find and store named elements ---
    self->data30 = FindElemAsBasePtr(dialogElem, "workPlaceCount");
    self->data38 = FindElemAsBasePtr(dialogElem, "maxCount");
    self->data40 = FindElemAsBasePtr(dialogElem, "workerCountUp");
    self->data48 = FindElemAsBasePtr(dialogElem, "workerCountDown");
    self->data50 = FindElemAsBasePtr(dialogElem, "jobCountUp");
    self->data58 = FindElemAsBasePtr(dialogElem, "jobCountDown");

    // --- Set element IDs on buttons ---
    fn_Element_SetElementId((void*)self->data40, 0x1450);
    fn_Element_SetElementId((void*)self->data48, 0x1451);
    fn_Element_SetElementId((void*)self->data50, 0x1452);
    fn_Element_SetElementId((void*)self->data58, 0x1453);

    // Find and set workPlaceButton element ID
    uint64_t workplaceBtn = FindElemAsBasePtr(dialogElem, "workPlaceButton");
    fn_Element_SetElementId((void*)workplaceBtn, 0x1454);

    // --- Check entity component for job count visibility ---
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
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
    bool isJobType = fn_IsDerivedFrom(subComp, *g_TypeId_WorkDescription);

    if (isJobType && *(uint8_t*)((uintptr_t)subComp + 0x20) == 0) {
        // Hide job count up/down buttons if sub-component flag is zero
        fn_Element_SetFlagsOnOff((void*)self->data50, 1, 0);
        fn_Element_SetFlagsOnOff((void*)self->data58, 1, 0);
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

void WorkplaceUI_Startup(WorkPlaceUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    void* entityPtr = *(void**)entityBuf;

    // Find the WorkPlaceComponent (FUN_140002e10 — type DAT_1401c80e0). Retail
    // also runs a discarded IsDerivedFrom(wpComp+0x18, WorkPlaceDescription) here.
    void* wpComp = nullptr;
    fn_FindComponent_WorkPlace(entityPtr, &wpComp);
    fn_InternalBasePtr_CheckPtr(&wpComp);

    // Find the WorkComponent (FUN_140003050 — type DAT_1401c8070)
    void* workComp = nullptr;
    fn_FindComponent_Work(entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr(&workComp);

    // Get the WorkDescription off the WorkComponent: +0x18
    void* jobSubComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = fn_IsDerivedFrom(jobSubComp, *g_TypeId_WorkDescription);
    void* jobType = isJobType ? jobSubComp : nullptr;

    // Check if building is complete: wpComp +0x48 (profession ptr) must be non-null
    fn_InternalBasePtr_CheckPtr(&wpComp);
    if (*(uintptr_t*)((uintptr_t)wpComp + 0x48) == 0) {
        // Building not complete — hide the entire panel
#ifdef _DEBUG
        printf("[WorkPlaceUI] HIDDEN: wpComp=%p profession(+0x48)=NULL entity=%p\n", wpComp, entityPtr);
#endif
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 1, 0);
        return;
    }

    // Building complete — show the panel
    fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 0, 1);

#ifdef _DEBUG
    printf("[WorkPlaceUI] entity=%p wpComp=%p workComp=%p jobType=%p jt+0x20=%d prof=%p\n",
        entityPtr, wpComp, workComp, jobType,
        jobType ? (int)*(uint8_t*)((uintptr_t)jobType + 0x20) : -1,
        *(void**)((uintptr_t)wpComp + 0x48));
#endif
    if (*(uint8_t*)((uintptr_t)jobType + 0x20) == 0) {
        // Check for laborer profession
        void* professionId = *(void**)((uintptr_t)wpComp + 0x48);
        void* profList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xb0);
        void* laborerProf = *(void**)((uintptr_t)profList + 0x38);

        if (professionId == laborerProf) {
            // Guard check: FUN_14000d6c0 (type DAT_1401c7008)
            EnsureOrigFindComps();
            fn_InternalBasePtr_CheckPtr(entityBuf);
            uint64_t posCheck = 0;
            fn_OrigFindCompPosition(*(void**)entityBuf, &posCheck);
            if (posCheck != 0) goto showUpDown;
        }

        // Hide job count up/down
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x50), 1, 0);
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x58), 1, 0);
    } else {
showUpDown:
        // Show job count up/down
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x50), 0, 1);
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x58), 0, 1);
    }

    // Get worker count and max worker count
    fn_InternalBasePtr_CheckPtr(&wpComp);
    uint32_t workerCount = *(uint32_t*)((uintptr_t)wpComp + 0x60);

    // Get the WorkComponent from the WorkPlaceComponent's parent entity (+0x10)
    fn_InternalBasePtr_CheckPtr(&wpComp);
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)wpComp + 0x10));
    void* parentEntity = *(void**)((uintptr_t)wpComp + 0x10);
    void* parentWorkComp = nullptr;
    fn_FindComponent_Work(parentEntity, &parentWorkComp);
    fn_InternalBasePtr_CheckPtr(&parentWorkComp);

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
        fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(profPtr + 0x28),
                        (void*)(profPtr + 0x30));
        fn_LabelElement_SetSprite(*(void**)(s + 0x30), spriteBuf);
    }

    // Set worker count text parameter 0 on workPlaceCount
    {
        System::String fmtStr = System::String::Format("%d", workerCount);
        fn_TextElement_SetParameter(*(void**)(s + 0x30), 0, &fmtStr);
    }

    // Set max count text parameter 1 on workPlaceCount
    {
        System::String fmtStr = System::String::Format("%d", maxCount);
        fn_TextElement_SetParameter(*(void**)(s + 0x30), 1, &fmtStr);
    }

    // Iterate citizens to count workers assigned to this workplace
    void* citizenList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf0);
    void* profList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xb0);
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
        fn_TextElement_SetParameter(*(void**)(s + 0x38), 0, &fmtStr);
    }

    // Enable/disable workerCountUp (+0x40) based on laborer count > 0
    {
        void* btnUp = *(void**)(s + 0x40);
        void** vtbl = *(void***)btnUp;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnUp, laborerCount > 0);
    }

    // Enable/disable workerCountDown (+0x48) based on workers assigned > 0
    {
        void* btnDown = *(void**)(s + 0x48);
        void** vtbl = *(void***)btnDown;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnDown, (int)workplaceWorkerCount > 0);
    }

    // Job count up/down enable based on workplace capacity
    // Use workComp (from main entity), NOT parentWorkComp (from inner entity)
    fn_InternalBasePtr_CheckPtr(&workComp);
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
    fn_InternalBasePtr_CheckPtr(&wpComp);
    if (*(void**)((uintptr_t)wpComp + 0x48) ==
        *(void**)((uintptr_t)profList + 0x38)) {
        // Laborer building — check position component for position-based limit
        // Guard check: FUN_14000d6c0 (type DAT_1401c7008)
        EnsureOrigFindComps();
        fn_InternalBasePtr_CheckPtr(entityBuf);
        uint64_t posGuard = 0;
        fn_OrigFindCompPosition(*(void**)entityBuf, &posGuard);
        if (posGuard != 0) {
            int maxCap = *(int*)((uintptr_t)jobType + 0x2c);
            // Get ResidenceComponent via FUN_140002ed0 (type DAT_1401c7900)
            fn_InternalBasePtr_CheckPtr(entityBuf);
            uint64_t resIBP = 0;
            fn_OrigFindCompResidence(*(void**)entityBuf, &resIBP);
            fn_InternalBasePtr_CheckPtr(&resIBP);
            int posCount = fn_ResidenceComponent_GetOccupantCount((void*)resIBP, 1);
            // Use min(jobType max, position count) as the real limit
            if (posCount < maxCap) {
                fn_InternalBasePtr_CheckPtr(entityBuf);
                uint64_t resIBP2 = 0;
                fn_OrigFindCompResidence(*(void**)entityBuf, &resIBP2);
                fn_InternalBasePtr_CheckPtr(&resIBP2);
                maxCap = fn_ResidenceComponent_GetOccupantCount((void*)resIBP2, 1);
            }
            // Enable up if current max < capacity limit
            void* btnUp = *(void**)(s + 0x50);
            void** vtbl = *(void***)btnUp;
            typedef void (*VCall_Enable_t)(void*, int);
            ((VCall_Enable_t)vtbl[8])(btnUp, wpMaxCount < maxCap);

            // Enable down if current max > 1
            void* btnDown = *(void**)(s + 0x58);
            void** vtbl2 = *(void***)btnDown;
            ((VCall_Enable_t)vtbl2[8])(btnDown, wpMaxCount > 1);
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
            std::fprintf(stderr, "[JobArrow] jobType=%p wpMaxCount=%d min(+0x28)=%d "
                         "max(+0x2c)=%d -> upEnabled=%d downEnabled=%d | "
                         "jobUp=%p vt=%p vt[8]=%p | jobDown=%p vt[8]=%p | "
                         "wkUp=%p vt[8]=%p | wkDown=%p vt[8]=%p\n",
                         (void*)jobType, wpMaxCount, jobMinCount, jobMax,
                         wpMaxCount < jobMax ? 1 : 0,
                         (wpMaxCount != jobMinCount && wpMaxCount > jobMinCount) ? 1 : 0,
                         bU, bU ? *(void**)bU : nullptr,
                         bU ? (*(void***)bU)[8] : nullptr,
                         bD, bD ? (*(void***)bD)[8] : nullptr,
                         bWU, bWU ? (*(void***)bWU)[8] : nullptr,
                         bWD, bWD ? (*(void***)bWD)[8] : nullptr);
            std::fflush(stderr);
        }
    }
    {
        void* btnUp = *(void**)(s + 0x50);
        void** vtbl = *(void***)btnUp;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnUp, wpMaxCount < *(int*)((uintptr_t)jobType + 0x2c));
    }
    {
        void* btnDown = *(void**)(s + 0x58);
        void** vtbl = *(void***)btnDown;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnDown, wpMaxCount != jobMinCount && wpMaxCount > jobMinCount);
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
#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdlib>
#include "dialog_controller_vtable.h"

// Roster-only guard defined at global scope in workplacecomponent.cpp (suppresses the
// gameplay-time Scene mesh-visual block in WorkplaceComponent Attach/Detach). Declared here
// at GLOBAL scope so the reference inside the namespace binds to ::g_wpcRosterOnly.
extern bool g_wpcRosterOnly;

namespace workplaceui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("WorkPlaceUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    // No owned heap fields: +0x30..+0x58 hold InternalBasePtrs to elements the DIALOG
    // owns (idx4 discovers them, it does not allocate them), so there is nothing to
    // release here. Just free self.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// The exe-.rdata type-id globals the visibility check dereferences are NULL in
// standalone. Lazy-fill from the live runtime type registry (valid after
// Object::Factory::Startup). "WorkPlaceComponent" is the registered name that
// fn_FindComponent_WorkPlace searches by (slot 0x1401c80e0).
void EnsureGlobals() {
    static uint32_t s_ce = 0, s_wpc = 0;
    if (!g_TypeId_ComponentEntity) {
        s_ce = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ce) g_TypeId_ComponentEntity = &s_ce;
    }
    if (!g_TypeId_WorkplaceComponent) {
        s_wpc = ::Object::ResolveTypeIdByNameLazy("WorkPlaceComponent");
        if (s_wpc) g_TypeId_WorkplaceComponent = &s_wpc;
    }
}

bool Ready() {
    return g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
        && g_TypeId_WorkplaceComponent && *g_TypeId_WorkplaceComponent;
}

// Faithful visibility subset of WorkplaceUI_Startup (0x140087080): show the
// workPlace panel only while the building has an active workplace profession.
void UpdateVisibility(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    void* panelElem = *reinterpret_cast<void**>(s + 0x20);
    if (!panelElem) return;  // controller not bound to its element yet

    // Resolve the bound entity (data source at +0x28).
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr(entityBuf);
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // Find the WorkPlaceComponent (fn_FindComponent_WorkPlace → g_TypeId_WorkplaceComponent).
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace(entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr(&workComp);

    // Show only when the workplace has an active profession (+0x48 != 0); hidden
    // on a completed house/barn/pile whose construction workplace has cleared it.
    bool show = workComp != 0 &&
                *reinterpret_cast<uintptr_t*>(static_cast<uintptr_t>(workComp) + 0x48) != 0;


    fn_Element_SetFlagsOnOff(panelElem, show ? 0u : 1u, show ? 1u : 0u);
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
// (fn_FindComponent_WorkPlace → g_TypeId_WorkplaceComponent), matching retail's
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
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity)) return;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr(entityBuf);
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // WorkPlaceComponent (retail local_res18).
    uint64_t workComp = 0;
    fn_FindComponent_WorkPlace(entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr(&workComp);
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
            if (e && fn_Element_SetElementId) fn_Element_SetElementId(e, id);
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
        if (wpb && fn_Element_SetElementId &&
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(wpb) + 0x88) != 0x1454)
            fn_Element_SetElementId(wpb, 0x1454);
    }

    // Roster = seated workers/builders at this site (WorkPlaceComponent+0x60).
    uint32_t roster = *reinterpret_cast<uint32_t*>(w + 0x60);

    // Capacity = the site's WorkComponent (StorageLocationComponent, +0x30). Retail
    // resolves it on the WPC's parent entity (WPC+0x10) and lazily CalcMaxWorkers's
    // the +0x30 cache. The natural-builder flow (FindAvailableBuilding) usually has
    // already primed +0x30, so it is read directly; StorageCountQuery only fills it
    // when still zero, and only when its type-id deps are live (else capacity=0).
    uint32_t capacity = 0;
    if (g_TypeId_WorkComponent && *g_TypeId_WorkComponent) {
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(w + 0x10));
        void* parentEntity = *reinterpret_cast<void**>(w + 0x10);
        uint64_t workC = 0;
        fn_FindComponent_Work(parentEntity, &workC);
        fn_InternalBasePtr_CheckPtr(&workC);
        if (workC) {
            uintptr_t c = static_cast<uintptr_t>(workC);
            if (*reinterpret_cast<int*>(c + 0x30) == 0 &&
                g_TypeId_WorkDescription && *g_TypeId_WorkDescription) {
                int n = fn_StorageCountQuery(reinterpret_cast<void*>(c));
                *reinterpret_cast<int*>(c + 0x30) = n;
            }
            capacity = *reinterpret_cast<uint32_t*>(c + 0x30);
        }
    }

    // workPlaceCount: profession sprite + "roster / capacity".
    if (elem30) {
        uint64_t sheet = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(profession) + 0x28);
        if (sheet && fn_Sprite_Ctor && fn_LabelElement_SetSprite) {
            uint8_t spriteBuf[16];
            fn_Sprite_Ctor(spriteBuf, sheet,
                           reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(profession) + 0x30));
            fn_LabelElement_SetSprite(elem30, spriteBuf);
        }
        if (fn_TextElement_SetParameter) {
            System::String fmt1 = System::String::Format("%d", static_cast<uint64_t>(roster));
            fn_TextElement_SetParameter(elem30, 0, &fmt1);
            System::String fmt2 = System::String::Format("%d", static_cast<uint64_t>(capacity));
            fn_TextElement_SetParameter(elem30, 1, &fmt2);
        }
    }

    // maxCount: town-wide count of adult, unassigned-task citizens whose profession
    // (citizen+0x1f0) matches this workplace's profession — the live "N builders in
    // the town" figure. Pure field reads over the citizen list (no exe helpers).
    if (elem38 && fn_TextElement_SetParameter) {
        int assigned = 0;
        void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
        void* citizenList = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xf0) : nullptr;
        if (citizenList) {
            int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(citizenList) + 0xa8);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(citizenList) + 0xa0);
            for (int i = 0; i < count && arr; ++i) {
                uintptr_t citizen = *reinterpret_cast<uintptr_t*>(arr + static_cast<uintptr_t>(i) * 8);
                fn_InternalBasePtr_CheckPtr(&citizen);
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
        fn_TextElement_SetParameter(elem38, 0, &fmt);
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
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity)) return;
    uint8_t eb[8];
    new (eb) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr(eb);
    void* entityPtr = *reinterpret_cast<void**>(eb);

    uint64_t wpc = 0;
    fn_FindComponent_WorkPlace(entityPtr, &wpc);
    fn_InternalBasePtr_CheckPtr(&wpc);
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

    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(static_cast<uintptr_t>(wpc) + 0x10));
    uint64_t workC = 0;
    fn_FindComponent_Work(
        *reinterpret_cast<void**>(static_cast<uintptr_t>(wpc) + 0x10), &workC);
    fn_InternalBasePtr_CheckPtr(&workC);
    if (!workC) {   // same named SA deviation as the wpc bail above
        fprintf(stderr, "[WorkPlaceUI] jobCount delta=%+d ABORTED: no WorkComponent on wpc=0x%llx\n",
                delta, static_cast<unsigned long long>(wpc));
        fflush(stderr);
        return;
    }
    uintptr_t c = static_cast<uintptr_t>(workC);
    if (*reinterpret_cast<int*>(c + 0x30) == 0 &&
        g_TypeId_WorkDescription && *g_TypeId_WorkDescription) {
        *reinterpret_cast<int*>(c + 0x30) = fn_StorageCountQuery(reinterpret_cast<void*>(c));
    }
    const int cur = *reinterpret_cast<int*>(c + 0x30);

    // No g_wpcRosterOnly wrapper here: the ported SetMaxWorkers body holds that guard across
    // its own eviction loop (destroycomponent_workerrelease.cpp), so setting it again would
    // only obscure which layer owns it.
    const int rosterBefore = *reinterpret_cast<int*>(static_cast<uintptr_t>(wpc) + 0x60);
    if (fn_WorkplaceComp_SetMaxWorkers)
        fn_WorkplaceComp_SetMaxWorkers(reinterpret_cast<void*>(static_cast<uintptr_t>(wpc)),
                                       cur + delta);

    // Report the OUTCOME in both arms: the observed before/after of WorkComponent+0x30 (the
    // field this whole open is about) and of the roster, or the reason nothing happened.
    fprintf(stderr, "[WorkPlaceUI] jobCount delta=%+d %s WorkComponent+0x30 %d->%d (asked %d) "
                    "roster %d->%d\n",
            delta, fn_WorkplaceComp_SetMaxWorkers
                 ? "applied"
                 : "NOT APPLIED (fn_WorkplaceComp_SetMaxWorkers unresolved; 0x14012fb00 should "
                   "be registered by reg_destroy_workers.cpp)",
            cur, *reinterpret_cast<int*>(c + 0x30), cur + delta,
            rosterBefore, *reinterpret_cast<int*>(static_cast<uintptr_t>(wpc) + 0x60));
    fflush(stderr);
}

// idx8 (+0x40): per-frame — keep the panel's visibility in sync as buildings
// finish construction (profession clears → panel hides), and refresh the live
// worker/builder counts (roster grows as builders are seated).
// ── The worker-assignment half, restored 2026-08-01 ──────────────────────────
// The block comment above (the "Vtable offsets" note) said the tail of Startup was
// "deliberately skipped in SA" because it calls FUN_14000d6c0 / FUN_140002ed0,
// "that resolve NULL in standalone". THAT IS NO LONGER TRUE and has not been for a
// while: 0x14000d6c0 is registered at standalone/reg_dialog.cpp:22 and 0x140002ed0
// at standalone/reg_work_base.cpp:39. The comment was accurate when written and
// aged into a lie — the same failure mode as the worldgen river-polyline copy.
//
// The cost of that stale deferral was the user-visible bug: UpdateVisibility +
// PopulateCounts render the roster ("6/6") but NEVER touch the worker up/down
// buttons, because the only code that enables them is the citizen loop in the
// skipped tail (it enables workerCountUp iff laborerCount > 0 and workerCountDown
// iff this workplace has assigned workers). With the tail skipped, both buttons sit
// at their .rsc default disabled state forever — which presents exactly as
// "no citizens are available to work", clicking does nothing, and yet the count
// still reads 6/6 because that half DID run.
//
// So run the full ported Startup. It is a superset: it recomputes the same counts
// and additionally sets the button enable states.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    UpdateVisibility(self);
    PopulateCounts(self);
    WorkplaceUI_Startup(reinterpret_cast<WorkPlaceUI*>(self));
}

// idx4 (+0x20): Serialize — retail 0x140087940, the element-discovery pass. Runs once
// when the dialog framework binds this controller to its element, finding the six named
// children and stamping the button element ids that idx9 (OnRemove) dispatches on.
void W_Serialize(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    WorkplaceUI_Serialize(reinterpret_cast<WorkPlaceUI*>(self));
}

// idx7 (+0x38): Startup — same visibility decision + count refresh on setup.
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    UpdateVisibility(self);
    PopulateCounts(self);
    WorkplaceUI_Startup(reinterpret_cast<WorkPlaceUI*>(self));
}

// idx9 (+0x48): OnRemove — per-building worker up/down spinner. Adds/removes ONE worker of
// this building's profession (WorkPlaceComponent+0x48), the per-building equivalent of the
// town-hall Professions spinner. Reuses the same registered cascade (RequestWorker =
// FindAvailableBuilding, then FindIdle/Working/Student/Any = cbf0/ca20/c5f0/c4b0 to convert
// a laborer; RemoveWorker to pull one).
//
// ⚠ THE g_wpcRosterOnly HOLD IS GONE (2026-08-17), and its removal is the fix for the
// stuck "?" icon. The old note here read "held under g_wpcRosterOnly so the gameplay-time
// Scene mesh-visual block in WorkplaceComponent Attach/Detach — which faults on inert SA
// objects (READ of 0x10) — is skipped". Two things were wrong with that:
//
//   1. g_wpcRosterOnly is a WORLD-BUILD shim. Its own definition (workplacecomponent.cpp
//      :221) says so: the mesh toggle calls Graphics::Scene::Remove/AddObject, which
//      deadlocks when driven mid-build, and "default false -> the per-tick gameplay
//      AssignHousing path is unchanged (full faithful visual)". A panel click is gameplay
//      time — exactly when the toggle IS meant to run. This path borrowed a build-time
//      shim into gameplay.
//   2. The READ of 0x10 it named was Detach's Getter_e710 on a null accessor, and that is
//      now guarded at the source (workplacecomponent.cpp:276, "a null accessor means
//      nothing renderable to toggle"). The premise expired when that guard landed; the
//      hold outlived it.
//
// The cost was not cosmetic-only in appearance: WorkPlaceComponent_SetAccepting raises the
// "no workers" warning (status bit 4) on an unstaffed workplace, and WorkplaceComponent_
// Attach's clear of that bit sits inside `if (!g_wpcRosterOnly)`. So EVERY seat a player
// made left the ? icon up over a fully-staffed building. Measured before: roster 0 -> 4
// with `icon 0x10 -> 0x10`. After: `icon 0x10 -> 0x0`.
void W_OnRemove(void* self, void* /*p2*/, int msg, int id, int /*p5*/, int /*p6*/) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    if (msg != 0x12) return;                        // button click
    if (id < 0x1450 || id > 0x1454) return;         // one of the 5 armed controls

    // workPlaceButton (0x1454): cycle through this profession's workplaces / center camera.
    // No roster mutation, so no g_wpcRosterOnly guard needed.
    if (id == 0x1454) { WorkplaceUI_ToggleWorkplace_Impl(self); return; }

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
    if (id == 0x1452 || id == 0x1453) { SA_AdjustJobCount(self, id == 0x1452 ? 1 : -1); return; }

    // Remaining ids are the worker assign/unassign pair.
    const bool add = (id == 0x1450);

    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity)) return;
    uint8_t eb[8];
    new (eb) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dataSource));   // B.3.3 bridge
    fn_InternalBasePtr_CheckPtr(eb);
    void* entityPtr = *reinterpret_cast<void**>(eb);

    uint64_t wpc = 0;
    fn_FindComponent_WorkPlace(entityPtr, &wpc);
    fn_InternalBasePtr_CheckPtr(&wpc);
    if (!wpc) return;
    uint64_t profId = *reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(wpc) + 0x48);
    if (!profId) return;

    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
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
        if (fn_CitizenList_RequestWorker && fn_CitizenList_FindIdle) {
            uint64_t retBuf[2] = {0, 0};
            fn_CitizenList_RequestWorker(citizenList, retBuf, profId);
            uint64_t slot = retBuf[0];
            uint64_t slotCopy = slot;
            uint8_t fb[8] = {0};
            void* result = fn_CitizenList_FindIdle(citizenList, fb, &slotCopy, profId);
            int64_t found = *reinterpret_cast<int64_t*>(result);
            if (found) who = "idle";
            // Faithful to retail driver dd10 (0x14007dd10): FindWorking (Strat_ca20,
            // 0x14008ca20) unconditionally anchors *availPtr, so retail gates it behind
            // `if (uVar4 != 0)` — only call it when RequestWorker/FindAvailableBuilding
            // returned a real building (slot != 0). Without this guard, a null building
            // (e.g. a just-placed foundation not yet a staffable workplace under BUILDCTRL)
            // reaches Strat_ca20's anchor -> null deref. FindIdle(cbf0)/FindStudent(c5f0)/
            // FindAny(c4b0) all guard *availPtr==0 themselves, so they stay in the chain.
            if (found == 0 && slot != 0 && fn_CitizenList_FindWorking) {
                slotCopy = slot; uint8_t fb2[8] = {0};
                result = fn_CitizenList_FindWorking(citizenList, fb2, &slotCopy, profId);
                found = *reinterpret_cast<int64_t*>(result);
                if (found) who = "working";
            }
            if (found == 0 && fn_CitizenList_FindStudent) {
                slotCopy = slot; uint8_t fb3[8] = {0};
                result = fn_CitizenList_FindStudent(citizenList, fb3, &slotCopy, profId);
                found = *reinterpret_cast<int64_t*>(result);
                if (found) who = "student";
            }
            if (found == 0 && fn_CitizenList_FindAny) {
                slotCopy = slot; uint64_t anyBuf = 0;
                fn_CitizenList_FindAny(citizenList, &anyBuf, &slotCopy, profId);
                if (anyBuf) who = "any";
            }
        }
    } else {  // 0x1451 — remove one worker of this profession
        who = "remove";
        if (fn_CitizenList_RemoveWorker) fn_CitizenList_RemoveWorker(citizenList, profId);
    }

}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        // idx4 Serialize (retail 0x140087940) — the element-discovery pass. Ported at
        // the top of this file since the original wave but never written into g_vt, so
        // the coverage probe measured it as still holding StubNop: "ported but unwired",
        // the trap this codebase keeps re-hitting. Wiring it is what populates
        // self->data30..data58 and stamps element ids 0x1450..0x1454 on the four
        // spinner buttons plus workPlaceButton — the ids idx9 (OnRemove) dispatches on.
        // Without it the worker/job spinners had no ids to arrive as at all.
        g_vt[4] = reinterpret_cast<void*>(&W_Serialize);
        g_vt[7] = reinterpret_cast<void*>(&W_Startup);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_OnRemove);
    }
} g_init;

// Wave 10D lane VTFID: retail's WorkPlaceUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "WorkPlaceUI", &StubNop);

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x68); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x68), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace workplaceui_sa

REGISTER_TYPE(WorkPlaceUI, "UI::DialogController",
              &workplaceui_sa::Factory, &workplaceui_sa::InPlace);

