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
#include <cstdint>
#include <cstdio>

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
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find WorkplaceComponent (FUN_140002e10 = BuildState2/WorkplaceComponent)
    uint64_t workComp = 0;
    fn_FindComponent_BuildState2((void*)entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr((void*)&workComp);
    uintptr_t wc = workComp;

    // Find StorageLocationComponent on the inner entity at workComp+0x10
    fn_InternalBasePtr_CheckPtr((void*)(wc + 0x10));
    uintptr_t innerEntity = *(uintptr_t*)(wc + 0x10);
    uint64_t slComp = 0;
    fn_FindComponent_Workplace((void*)innerEntity, &slComp);
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
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find WorkplaceComponent
    uint64_t workComp = 0;
    fn_FindComponent_BuildState2((void*)entityPtr, &workComp);
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
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t result = 0;
    fn_FindElement(dialogElem, &result, nameBuf);

    fn_String_Dtor(nameBuf);
    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
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
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find WorkplaceComponent
    void* workComp = nullptr;
    fn_FindComponent_Workplace(entityInternal, &workComp);

    // Check sub-component at +0x18 for type DAT_1401c80a8
    void* subComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = fn_IsDerivedFrom(subComp, *g_TypeId_WorkplaceJobType);

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
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    fn_InternalBasePtr_CheckPtr((void*)entityBuf);
    void* entityPtr = *(void**)entityBuf;

    // Find BuildState2 component (FUN_140002e10 — type DAT_1401c8118)
    void* buildComp = nullptr;
    fn_FindComponent_BuildState2(entityPtr, &buildComp);
    fn_InternalBasePtr_CheckPtr(&buildComp);

    // Find WorkplaceComponent
    void* workComp = nullptr;
    fn_FindComponent_Workplace(entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr(&workComp);

    // Get WorkplaceJobType sub-component from workplace: +0x18
    void* jobSubComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = fn_IsDerivedFrom(jobSubComp, *g_TypeId_WorkplaceJobType);
    void* jobType = isJobType ? jobSubComp : nullptr;

    // Check if building is complete: buildComp +0x48 (profession ptr) must be non-null
    fn_InternalBasePtr_CheckPtr(&buildComp);
    if (*(uintptr_t*)((uintptr_t)buildComp + 0x48) == 0) {
        // Building not complete — hide the entire panel
#ifdef _DEBUG
        printf("[WorkPlaceUI] HIDDEN: buildComp=%p profession(+0x48)=NULL entity=%p\n", buildComp, entityPtr);
#endif
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 1, 0);
        return;
    }

    // Building complete — show the panel
    fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 0, 1);

#ifdef _DEBUG
    printf("[WorkPlaceUI] entity=%p buildComp=%p workComp=%p jobType=%p jt+0x20=%d prof=%p\n",
        entityPtr, buildComp, workComp, jobType,
        jobType ? (int)*(uint8_t*)((uintptr_t)jobType + 0x20) : -1,
        *(void**)((uintptr_t)buildComp + 0x48));
#endif
    if (*(uint8_t*)((uintptr_t)jobType + 0x20) == 0) {
        // Check for laborer profession
        void* professionId = *(void**)((uintptr_t)buildComp + 0x48);
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
    fn_InternalBasePtr_CheckPtr(&buildComp);
    uint32_t workerCount = *(uint32_t*)((uintptr_t)buildComp + 0x60);

    // Get WorkplaceComponent from buildComp's parent entity (+0x10)
    fn_InternalBasePtr_CheckPtr(&buildComp);
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buildComp + 0x10));
    void* parentEntity = *(void**)((uintptr_t)buildComp + 0x10);
    void* wpComp2 = nullptr;
    fn_FindComponent_Workplace(parentEntity, &wpComp2);
    fn_InternalBasePtr_CheckPtr(&wpComp2);

    // Get max count — lazy-init cached value at +0x30
    if (*(int*)((uintptr_t)wpComp2 + 0x30) == 0) {
        int count = fn_StorageCountQuery(wpComp2);
        *(int*)((uintptr_t)wpComp2 + 0x30) = count;
    }
    uint32_t maxCount = *(uint32_t*)((uintptr_t)wpComp2 + 0x30);

    // Get profession sprite from buildComp +0x48 -> +0x28 (sheet) / +0x30 (name)
    {
        uintptr_t profPtr = *(uintptr_t*)((uintptr_t)buildComp + 0x48);
        uint8_t spriteBuf[16];
        fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(profPtr + 0x28),
                        (void*)(profPtr + 0x30));
        fn_LabelElement_SetSprite(*(void**)(s + 0x30), spriteBuf);
    }

    // Set worker count text parameter 0 on workPlaceCount
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%d", workerCount);
        fn_TextElement_SetParameter(*(void**)(s + 0x30), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    // Set max count text parameter 1 on workPlaceCount
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%d", maxCount);
        fn_TextElement_SetParameter(*(void**)(s + 0x30), 1, fmtBuf);
        fn_String_Dtor(fmtBuf);
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
        if (*(float*)((uintptr_t)citizen + 0x3c) <=
            *(float*)((uintptr_t)citizenProf + 0x44)) continue;
        if (*(uintptr_t*)((uintptr_t)citizen + 0x208) != 0) continue;

        // Check if assigned to this workplace's profession
        void* assignedProf = *(void**)((uintptr_t)citizen + 0x1f0);
        if (assignedProf == *(void**)((uintptr_t)buildComp + 0x48)) {
            workplaceWorkerCount++;
        }
        if (assignedProf == laborerProf) {
            laborerCount++;
        }
    }

    // Set assigned worker count on buttonPrevious (+0x38) text
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%d", workplaceWorkerCount);
        fn_TextElement_SetParameter(*(void**)(s + 0x38), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
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
    // Use workComp (from main entity), NOT wpComp2 (from inner entity)
    fn_InternalBasePtr_CheckPtr(&workComp);
    if (*(int*)((uintptr_t)workComp + 0x30) == 0) {
        int count = fn_StorageCountQuery(workComp);
        *(int*)((uintptr_t)workComp + 0x30) = count;
    }
    int wpMaxCount = *(int*)((uintptr_t)workComp + 0x30);

#ifdef _DEBUG
    printf("[WorkPlaceUI] workers=%u/%u assigned=%d laborers=%d wpMax=%d jt+0x28=%d jt+0x2c=%d jt+0x34=%d\n",
        workerCount, maxCount, workplaceWorkerCount, laborerCount, wpMaxCount,
        jobType ? *(int*)((uintptr_t)jobType + 0x28) : -1,
        jobType ? *(int*)((uintptr_t)jobType + 0x2c) : -1,
        jobType ? (int)*(uint8_t*)((uintptr_t)jobType + 0x34) : -1);
#endif

    // Check if this is a laborer profession
    fn_InternalBasePtr_CheckPtr(&buildComp);
    if (*(void**)((uintptr_t)buildComp + 0x48) ==
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

// ============================================================
// WorkplaceUI::OnRemove (vtable +0x48) — Button dispatch
// ============================================================
// Handles worker count up/down (0x1450/0x1451), job count up/down
// (0x1452/0x1453), and workplace button toggle (0x1454).
//
// Original: 0x1400877e0.

void WorkplaceUI_OnRemove(WorkPlaceUI* self, void* param2, int msg, int id) {
    (void)param2;
    if (msg != 0x12) return;
#ifdef _DEBUG
    printf("[WorkPlaceUI] OnRemove: msg=0x%x id=0x%x\n", msg, id);
#endif

    uintptr_t s = (uintptr_t)self;

    // Get entity from data source
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    void* entityPtr = *(void**)entityBuf;

    // Find BuildState2 component
    void* buildComp = nullptr;
    fn_FindComponent_BuildState2(entityPtr, &buildComp);

    // Get citizen list singleton
    void* citizenList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf0);

    if (id == 0x1450) {
        // workerCountUp — request new worker for profession
        uint64_t profId = *(uint64_t*)((uintptr_t)buildComp + 0x48);
        uint64_t retBuf = 0;
        fn_CitizenList_RequestWorker(citizenList, &retBuf, profId);
        uint64_t workerSlot = retBuf;
        uint8_t assignBuf[16];
        fn_CitizenList_AssignWorker(citizenList, assignBuf, &workerSlot, profId);
    } else if (id == 0x1451) {
        // workerCountDown — remove worker from profession
        uint64_t profId = *(uint64_t*)((uintptr_t)buildComp + 0x48);
        fn_CitizenList_RemoveWorker(citizenList, profId);
    } else if (id == 0x1452) {
        // jobCountUp — increase job count
        WorkplaceUI_AdjustWorkerCount_Impl(self, 1);
    } else if (id == 0x1453) {
        // jobCountDown — decrease job count
        WorkplaceUI_AdjustWorkerCount_Impl(self, -1);
    } else if (id == 0x1454) {
        // workPlaceButton — toggle workplace enabled state
        WorkplaceUI_ToggleWorkplace_Impl(self);
    }
}

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
// tail of Startup is deliberately skipped in SA: it calls two exe helper functions
// via ResolveAddr(Module::Exe,…) (FUN_14000d6c0 / FUN_140002ed0) that resolve NULL
// in standalone, and iterates the citizen list. The user-visible requirement is
// that the panel disappears on the storage/house dialogs; a shown production panel
// keeps its .rsc default content exactly as the StubNop already left it (no
// regression). This mirrors the StorageUI/StorageCapacityUI/ResidenceUI promotions.
//
// Vtable offsets (verified from the exe WorkPlaceUI vtable @ 0x14015e030):
//   +0x00 (idx0) ScalarDtor   +0x10 (idx2) GetTypeId
//   +0x20 (idx4) Serialize (0x140087940) — element discovery (skipped in SA)
//   +0x38 (idx7) Startup   (0x140087080) — full refresh (visibility subset in SA)
//   +0x40 (idx8)           — base DialogController::OnUpdate (per-frame). The
//                framework dispatches ONLY +0x40 each frame, so the visibility
//                refresh is driven here so it stays live as buildings complete.
//   +0x48 (idx9) OnRemove  (0x1400877e0) — button dispatch (no-op in SA; the
//                worker/job spinners aren't wired without Serialize's element IDs).
#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdlib>

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
    // No owned heap fields (Serialize is skipped in SA, so +0x30..+0x58 stay the
    // zero-filled InternalBasePtr values the factory set). Just free self.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// The exe-.rdata type-id globals the visibility check dereferences are NULL in
// standalone. Lazy-fill from the live runtime type registry (valid after
// Object::Factory::Startup). "WorkPlaceComponent" is the registered name that
// fn_FindComponent_BuildState2 searches by (slot 0x1401c80e0).
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
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    fn_InternalBasePtr_CheckPtr(entityBuf);
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // Find the WorkPlaceComponent (fn_FindComponent_BuildState2 → g_TypeId_WorkplaceComponent).
    uint64_t workComp = 0;
    fn_FindComponent_BuildState2(entityPtr, &workComp);
    fn_InternalBasePtr_CheckPtr(&workComp);

    // Show only when the workplace has an active profession (+0x48 != 0); hidden
    // on a completed house/barn/pile whose construction workplace has cleared it.
    bool show = workComp != 0 &&
                *reinterpret_cast<uintptr_t*>(static_cast<uintptr_t>(workComp) + 0x48) != 0;

    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        static int n = 0;
        if (n++ < 10) {
            fprintf(stderr, "[WorkPlaceUI] self=%p workComp=0x%llx prof=0x%llx show=%d\n",
                    self, (unsigned long long)workComp,
                    workComp ? (unsigned long long)*reinterpret_cast<uintptr_t*>(
                        static_cast<uintptr_t>(workComp) + 0x48) : 0ull, show ? 1 : 0);
            fflush(stderr);
        }
    }

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
// (fn_FindComponent_BuildState2 → g_TypeId_WorkplaceComponent), matching retail's
// local_res18 — its +0x48 is the active profession, +0x60 the live roster.
void PopulateCounts(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    void* panelElem = *reinterpret_cast<void**>(s + 0x20);
    if (!panelElem) return;

    // Bound entity (data source at +0x28).
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity)) return;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, dataSource);
    fn_InternalBasePtr_CheckPtr(entityBuf);
    void* entityPtr = *reinterpret_cast<void**>(entityBuf);

    // WorkPlaceComponent (retail local_res18).
    uint64_t workComp = 0;
    fn_FindComponent_BuildState2(entityPtr, &workComp);
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

    // Arm the worker up/down spinner buttons (self+0x40/+0x48) so their clicks route to
    // OnRemove (vtable slot 9) as ids 0x1450/0x1451 — the per-building equivalent of the
    // town-hall Professions spinner. Element ids are set once on discovery. The retail
    // laborer-availability enable math is skipped (needs exe helpers); the buttons keep
    // their .rsc-default clickable state and a click with no convertible laborer / no
    // seated worker simply no-ops in the seat cascade.
    if (*reinterpret_cast<void**>(s + 0x40) == nullptr) {
        *reinterpret_cast<uint64_t*>(s + 0x40) = FindElemAsBasePtr(panelElem, "workerCountUp");
        void* up = *reinterpret_cast<void**>(s + 0x40);
        if (up && fn_Element_SetElementId) fn_Element_SetElementId(up, 0x1450);
    }
    if (*reinterpret_cast<void**>(s + 0x48) == nullptr) {
        *reinterpret_cast<uint64_t*>(s + 0x48) = FindElemAsBasePtr(panelElem, "workerCountDown");
        void* down = *reinterpret_cast<void**>(s + 0x48);
        if (down && fn_Element_SetElementId) fn_Element_SetElementId(down, 0x1451);
    }

    // Roster = seated workers/builders at this site (WorkPlaceComponent+0x60).
    uint32_t roster = *reinterpret_cast<uint32_t*>(w + 0x60);

    // Capacity = the site's WorkComponent (StorageLocationComponent, +0x30). Retail
    // resolves it on the WPC's parent entity (WPC+0x10) and lazily CalcMaxWorkers's
    // the +0x30 cache. The natural-builder flow (FindAvailableBuilding) usually has
    // already primed +0x30, so it is read directly; StorageCountQuery only fills it
    // when still zero, and only when its type-id deps are live (else capacity=0).
    uint32_t capacity = 0;
    if (g_TypeId_StorageLocationComponent && *g_TypeId_StorageLocationComponent) {
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(w + 0x10));
        void* parentEntity = *reinterpret_cast<void**>(w + 0x10);
        uint64_t workC = 0;
        fn_FindComponent_Workplace(parentEntity, &workC);
        fn_InternalBasePtr_CheckPtr(&workC);
        if (workC) {
            uintptr_t c = static_cast<uintptr_t>(workC);
            if (*reinterpret_cast<int*>(c + 0x30) == 0 &&
                g_TypeId_WorkplaceJobType && *g_TypeId_WorkplaceJobType) {
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
        if (fn_TextElement_SetParameter && fn_String_Format) {
            uint8_t fmt[8];
            fn_String_Format(fmt, "%d", static_cast<uint64_t>(roster));
            fn_TextElement_SetParameter(elem30, 0, fmt);
            fn_String_Dtor(fmt);
            fn_String_Format(fmt, "%d", static_cast<uint64_t>(capacity));
            fn_TextElement_SetParameter(elem30, 1, fmt);
            fn_String_Dtor(fmt);
        }
    }

    // maxCount: town-wide count of adult, unassigned-task citizens whose profession
    // (citizen+0x1f0) matches this workplace's profession — the live "N builders in
    // the town" figure. Pure field reads over the citizen list (no exe helpers).
    if (elem38 && fn_TextElement_SetParameter && fn_String_Format) {
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
                void* citizenDesc = *reinterpret_cast<void**>(citizen + 0x288);
                if (!citizenDesc) continue;
                // Adult (age > adult-age) AND no active task (citizen+0x208 == 0).
                if (*reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(citizenDesc) + 0x44)
                        > *reinterpret_cast<float*>(citizen + 0x3c)) continue;
                if (*reinterpret_cast<uintptr_t*>(citizen + 0x208) != 0) continue;
                if (*reinterpret_cast<void**>(citizen + 0x1f0) == profession) ++assigned;
            }
        }
        uint8_t fmt[8];
        fn_String_Format(fmt, "%d", static_cast<uint64_t>(static_cast<uint32_t>(assigned)));
        fn_TextElement_SetParameter(elem38, 0, fmt);
        fn_String_Dtor(fmt);
    }

    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        static int n = 0;
        if (n++ < 20)
            fprintf(stderr, "[WorkPlaceUI] counts prof=%p roster=%u cap=%u elem30=%p elem38=%p\n",
                    profession, roster, capacity, elem30, elem38), fflush(stderr);
    }
}

// idx8 (+0x40): per-frame — keep the panel's visibility in sync as buildings
// finish construction (profession clears → panel hides), and refresh the live
// worker/builder counts (roster grows as builders are seated).
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    UpdateVisibility(self);
    PopulateCounts(self);
}

// idx7 (+0x38): Startup — same visibility decision + count refresh on setup.
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    UpdateVisibility(self);
    PopulateCounts(self);
}

// idx9 (+0x48): OnRemove — per-building worker up/down spinner. Adds/removes ONE worker of
// this building's profession (WorkPlaceComponent+0x48), the per-building equivalent of the
// town-hall Professions spinner. Reuses the same registered cascade (RequestWorker =
// FindAvailableBuilding, then FindIdle/Working/Student/Any = cbf0/ca20/c5f0/c4b0 to convert
// a laborer; RemoveWorker to pull one). Held under g_wpcRosterOnly so the gameplay-time Scene
// mesh-visual block in WorkplaceComponent Attach/Detach — which faults on inert SA objects
// (READ of 0x10) — is skipped, exactly like the town-hall path and the builder pool.
void W_OnRemove(void* self, void* /*p2*/, int msg, int id, int /*p5*/, int /*p6*/) {
    EnsureGlobals();
    if (!self || !Ready() || !SA_DialogEntityAlive(self)) return;
    if (msg != 0x12) return;                       // button click
    if (id != 0x1450 && id != 0x1451) return;      // workerCountUp / workerCountDown

    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* dataSource = *reinterpret_cast<void**>(s + 0x28);
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity)) return;
    uint8_t eb[8];
    fn_InternalBasePtr_Ctor(eb, dataSource);
    fn_InternalBasePtr_CheckPtr(eb);
    void* entityPtr = *reinterpret_cast<void**>(eb);

    uint64_t wpc = 0;
    fn_FindComponent_BuildState2(entityPtr, &wpc);
    fn_InternalBasePtr_CheckPtr(&wpc);
    if (!wpc) return;
    uint64_t profId = *reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(wpc) + 0x48);
    if (!profId) return;

    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    void* citizenList = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xf0) : nullptr;
    if (!citizenList) return;

    bool prevRoster = ::g_wpcRosterOnly;
    ::g_wpcRosterOnly = true;

    if (id == 0x1450) {
        // Add one worker: request a slot, then convert idle -> working -> student -> any.
        if (fn_CitizenList_RequestWorker && fn_CitizenList_FindIdle) {
            uint64_t retBuf[2] = {0, 0};
            fn_CitizenList_RequestWorker(citizenList, retBuf, profId);
            uint64_t slot = retBuf[0];
            uint64_t slotCopy = slot;
            uint8_t fb[8] = {0};
            void* result = fn_CitizenList_FindIdle(citizenList, fb, &slotCopy, profId);
            int64_t found = *reinterpret_cast<int64_t*>(result);
            if (found == 0 && fn_CitizenList_FindWorking) {
                slotCopy = slot; uint8_t fb2[8] = {0};
                result = fn_CitizenList_FindWorking(citizenList, fb2, &slotCopy, profId);
                found = *reinterpret_cast<int64_t*>(result);
            }
            if (found == 0 && fn_CitizenList_FindStudent) {
                slotCopy = slot; uint8_t fb3[8] = {0};
                result = fn_CitizenList_FindStudent(citizenList, fb3, &slotCopy, profId);
                found = *reinterpret_cast<int64_t*>(result);
            }
            if (found == 0 && fn_CitizenList_FindAny) {
                slotCopy = slot; uint64_t anyBuf = 0;
                fn_CitizenList_FindAny(citizenList, &anyBuf, &slotCopy, profId);
            }
        }
    } else {  // 0x1451 — remove one worker of this profession
        if (fn_CitizenList_RemoveWorker) fn_CitizenList_RemoveWorker(citizenList, profId);
    }

    ::g_wpcRosterOnly = prevRoster;
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[7] = reinterpret_cast<void*>(&W_Startup);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_OnRemove);
    }
} g_init;

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

