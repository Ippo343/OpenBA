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
#include <cstdint>

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

    // Center camera: fn_Toolbar_CenterOnEntity(gameWorld+0x120, panel+0x18, workplaceEntityInner)
    void* gameWorld = *g_pGameWorldSingleton;
    void* subsys = *(void**)((uintptr_t)gameWorld + 0x120);
    uint64_t parentRef = *(uint64_t*)(s + 0x18);
    fn_Toolbar_CenterOnEntity(subsys, &parentRef, &workplaceEntityInner);
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
    void* entityPtr = *(void**)entityBuf;

    // Find BuildState2 component (FUN_140002e10 — type DAT_1401c8118)
    void* buildComp = nullptr;
    fn_FindComponent_BuildState2(entityPtr, &buildComp);

    // Find WorkplaceComponent
    void* workComp = nullptr;
    fn_FindComponent_Workplace(entityPtr, &workComp);

    // Get WorkplaceJobType sub-component from workplace: +0x18
    void* jobSubComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = fn_IsDerivedFrom(jobSubComp, *g_TypeId_WorkplaceJobType);
    void* jobType = isJobType ? jobSubComp : nullptr;

    // Check if building is complete: buildComp +0x48 (profession ptr) must be non-null
    if (*(uintptr_t*)((uintptr_t)buildComp + 0x48) == 0) {
        // Building not complete — hide the entire panel
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 1, 0);
        return;
    }

    // Building complete — show the panel
    fn_Element_SetFlagsOnOff(*(void**)(s + 0x20), 0, 1);

    // Check if job count up/down should be visible
    if (*(uint8_t*)((uintptr_t)jobType + 0x20) == 0) {
        // Check for laborer profession
        void* professionId = *(void**)((uintptr_t)buildComp + 0x48);
        void* profList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xb0);
        void* laborerProf = *(void**)((uintptr_t)profList + 0x38);

        if (professionId == laborerProf) {
            // Check for PositionComponent (FUN_14000d6c0) — additional check for lab
            void* posComp = nullptr;
            fn_FindComponent_Position(entityPtr, &posComp);
            if (posComp != nullptr) goto showUpDown;
        }

        // Hide worker up/down
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x50), 1, 0);
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x58), 0, 1);
    } else {
showUpDown:
        // Show worker up/down
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x50), 0, 1);
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x58), 1, 0);
    }

    // Get worker count and max worker count
    uint32_t workerCount = *(uint32_t*)((uintptr_t)buildComp + 0x60);

    // Get WorkplaceComponent from buildComp's parent entity (+0x10)
    void* parentEntity = *(void**)((uintptr_t)buildComp + 0x10);
    void* wpComp2 = nullptr;
    fn_FindComponent_Workplace(parentEntity, &wpComp2);

    // Get max count — lazy-init cached value at +0x30
    if (*(int*)((uintptr_t)wpComp2 + 0x30) == 0) {
        int count = fn_StorageCountQuery(wpComp2);
        *(int*)((uintptr_t)wpComp2 + 0x30) = count;
    }
    uint32_t maxCount = *(uint32_t*)((uintptr_t)wpComp2 + 0x30);

    // Get profession sprite from buildComp +0x48 -> +0x28/+0x30
    {
        uintptr_t profPtr = *(uintptr_t*)((uintptr_t)buildComp + 0x48);
        // Build sprite from profession +0x28 (sprite sheet) and +0x30 (index range)
        uint8_t spriteBuf[16];
        // Use LabelElement::SetSprite on workPlaceCount (+0x30)
        *(uint64_t*)spriteBuf = *(uint64_t*)(profPtr + 0x28);
        *(uint64_t*)(spriteBuf + 8) = 0; // padding
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

    // Enable/disable workerCountDown (+0x48) based on laborer count > 0
    {
        void* btnDown = *(void**)(s + 0x48);
        void** vtbl = *(void***)btnDown;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnDown, laborerCount > 0);
    }

    // Enable/disable workerCountUp (+0x40) based on worker availability
    {
        void* btnUp = *(void**)(s + 0x40);
        void** vtbl = *(void***)btnUp;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnUp, (int)workplaceWorkerCount > 0);
    }

    // Job count up/down visibility based on workplace capacity
    int wpMaxCount = *(int*)((uintptr_t)wpComp2 + 0x30);

    // Check if this is a laborer profession
    if (*(void**)((uintptr_t)buildComp + 0x48) ==
        *(void**)((uintptr_t)profList + 0x38)) {
        // Laborer building — check position component for additional limit
        void* posComp = nullptr;
        fn_FindComponent_Position(entityPtr, &posComp);
        if (posComp != nullptr) {
            int posCount = *(int*)((uintptr_t)jobType + 0x2c);
            // Get position-based count
            void* posComp2 = nullptr;
            fn_FindComponent_Position(entityPtr, &posComp2);
            // Enable up if capacity < posCount
            void* btnUp = *(void**)(s + 0x50);
            void** vtbl = *(void***)btnUp;
            typedef void (*VCall_Enable_t)(void*, int);
            ((VCall_Enable_t)vtbl[8])(btnUp, wpMaxCount < posCount);

            void* btnDown = *(void**)(s + 0x58);
            void** vtbl2 = *(void***)btnDown;
            ((VCall_Enable_t)vtbl2[8])(btnDown, wpMaxCount > 1 && wpMaxCount != posCount);
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
