// engine/src/ui/residenceui.cpp
// Clean reimplementation of ResidenceUI game logic.
//
// ResidenceUI (0x40 bytes) — residence info panel.
// Extends DialogController (0x30 base).
// +0x30 = dynamic entry array (0x30-byte entries), +0x38 = entry count.
//
// Each entry (0x30 bytes):
//   +0x00 = imageMale element ptr
//   +0x08 = imageFemale element ptr
//   +0x10 = textName element ptr
//   +0x18 = textAge element ptr
//   +0x20 = textProfession element ptr
//   +0x28 = buttonGoto element ptr
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// File-local helper: ResidenceUI_ArrayPush_Impl (FUN_140021020)
// ============================================================
// Resizes dynamic array to newCount entries (stride 0x30 bytes, no String fields).
// Entry layout: 6 × void* (imageMale, imageFemale, textName, textAge, textProfession, buttonGoto).
// arrayBase layout: [data* 8][count int32][capacity int32]
// Original: 0x140021020.
static void ResidenceUI_ArrayPush_Impl(void* arrayBase, int newCount, void* entry6) {
    uintptr_t* dataPtr  = (uintptr_t*)arrayBase;
    int* countPtr       = (int*)((uintptr_t)arrayBase + 8);
    int* capacityPtr    = (int*)((uintptr_t)arrayBase + 0xc);
    int oldCount        = *countPtr;

    if (newCount == oldCount) return;

    void* oldData = (void*)*dataPtr;
    *countPtr = newCount;

    if (*capacityPtr < newCount) {
        *capacityPtr = newCount;
        if (oldCount != 0 && newCount == oldCount + 1) {
            *capacityPtr = newCount * 3;
        }
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)*capacityPtr * 0x30, 8);
        *dataPtr = (uintptr_t)newData;
    }

    int startCopy = (oldData == (void*)*dataPtr) ? oldCount : 0;
    for (int i = startCopy; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*dataPtr + (uintptr_t)i * 0x30);
        if (dst != nullptr) {
            uint64_t* src = (i < oldCount)
                ? (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 0x30)
                : (uint64_t*)entry6;
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
            dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
        }
    }

    if (oldData != (void*)*dataPtr) {
        fn_MemFreeStatic(oldData);
    }
}

// Helper: find element by name in parent element, returns raw pointer
// Original pattern: FindElement → CheckPtr → deref
static void* FindChildElem(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_FindElement(parentElem, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);  // resolve IBP before deref
    void* result = *(void**)retBuf;
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
// Original pattern: FindElement → CheckPtr → deref → IsDerivedFrom → IBP_Ctor
static uint64_t FindElemAsBasePtr(void* parentElem, const char* name) {
    void* elem = FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    fn_InternalBasePtr_Ctor(bpBuf, elem);
    return *(uint64_t*)bpBuf;
}

// ============================================================
// ResidenceUI::Slot4 (vtable +0x20) — Setup resident table
// ============================================================
// Finds "residenceTable" (or "residenceLongTable" fallback), iterates
// rows to find child elements (buttonGoto, textName, imageMale, imageFemale,
// textAge, textProfession), stores in dynamic array at +0x30.
// Sets element IDs on buttonGoto: 300 + rowIndex.
//
// Original: 0x1400B5850.

void ResidenceUI_Slot4(ResidenceUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    // Try "residenceTable" first, fall back to "residenceLongTable"
    void* tableRaw = FindChildElem(dialogElem, "residenceTable");
    void* tableElem;

    if (!tableRaw) {
        // Fallback: find "residenceLongTable"
        void* longTable = FindChildElem(dialogElem, "residenceLongTable");
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, longTable);
        tableElem = *(void**)basePtrBuf;
    } else {
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, tableRaw);
        tableElem = *(void**)basePtrBuf;
    }

    // Get row count and column count from table element
    int rowCount = *(int*)((uintptr_t)tableElem + 0xC0);
    int colCount = *(int*)((uintptr_t)tableElem + 0xC4);

#ifdef _DEBUG
    printf("[ResidenceUI::Slot4] tableElem=%p rowCount=%d colCount=%d\n",
           tableElem, rowCount, colCount);
    fflush(stdout);
#endif

    for (int i = 0; i < rowCount; i++) {
        // 0x30-byte entry: imageMale, imageFemale, textName, textAge, textProfession, buttonGoto
        uint64_t entry[6] = {0, 0, 0, 0, 0, 0};
        uint8_t childBuf[8];

        // Original uses flat table indices: row * colCount + column
        // Column layout: 0=buttonGoto, 1=textName, 2=imageMale/Female, 3=textAge, 4=textProfession
        int base = i * colCount;

        // Col 0: buttonGoto
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 0);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[5] = FindElemAsBasePtr(*(void**)childBuf, "buttonGoto");
        fn_InternalBasePtr_CheckPtr(&entry[5]);
        if (entry[5]) fn_Element_SetElementId((void*)entry[5], 300 + i);

        // Col 1: textName
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 1);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[2] = FindElemAsBasePtr(*(void**)childBuf, "textName");

        // Col 2: imageMale + imageFemale (container cell)
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 2);
        fn_InternalBasePtr_CheckPtr(childBuf);
        void* iconCell = *(void**)childBuf;
        entry[0] = FindElemAsBasePtr(iconCell, "imageMale");

        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 2);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[1] = FindElemAsBasePtr(*(void**)childBuf, "imageFemale");

        // Col 3: textAge
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 3);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[3] = FindElemAsBasePtr(*(void**)childBuf, "textAge");

        // Col 4: textProfession
        fn_InternalBasePtr_CheckPtr(&tableElem);
        fn_Element_Child(tableElem, childBuf, base + 4);
        fn_InternalBasePtr_CheckPtr(childBuf);
        entry[4] = FindElemAsBasePtr(*(void**)childBuf, "textProfession");

#ifdef _DEBUG
        if (i < 3) {
            printf("[ResidenceUI::Slot4]   row[%d] base=%d: imgM=%llx imgF=%llx txtN=%llx txtA=%llx txtP=%llx btn=%llx\n",
                   i, base, entry[0], entry[1], entry[2], entry[3], entry[4], entry[5]);
            fflush(stdout);
        }
#endif

        // Push entry into dynamic array at +0x30
        // FUN_140021020: array resize/push with 0x30-byte elements
        ResidenceUI_ArrayPush_Impl((void*)(s + 0x30), *(int*)(s + 0x38) + 1, entry);
    }
}

// ============================================================
// ResidenceUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles resident row button clicks. Each row has element ID = 300 + index.
// Clicking navigates camera to that resident citizen.
//
// Original: 0x1400B5820.

// ============================================================
// ResidenceUI::Slot7 (vtable +0x38) — Update resident display
// ============================================================
// Iterates storage component rooms to display resident info per row.
// For each occupied slot: show gender icons, set name/age/profession text.
// For unused rows beyond active count: hide and clear.
//
// Storage layout: +0x50 = room array ptr (0x10-byte entries: ptr, count),
//                 +0x58 = room count.
// Each resident ptr -> citizen entity with name at +0x28, gender at +0x38,
// age at +0x3c, profession at +0x1f0, description at +0x288, death at +0x208.
//
// Original: 0x1400B51F0.

void ResidenceUI_Slot7(ResidenceUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find ResidenceComponent — original uses type ID at 0x1401c7cf0 (ResidenceComponent),
    // NOT StorageComponent (0x1401c7e78). Different component with room data at +0x50/+0x58.
    uintptr_t storageComp = 0;
    Entity_FindComponentByExeTypeId((void*)entityPtr, &storageComp, g_TypeId_ResidenceComponent);
    fn_InternalBasePtr_CheckPtr(&storageComp);

#ifdef _DEBUG
    printf("[ResidenceUI::Slot7] self=%p entity=%p resComp=0x%llx\n",
           (void*)self, entity, (unsigned long long)storageComp);
#endif

    float ageMul = *g_pAgeMultiplier;  // 5.0f
    int totalResidents = 0;
    int rowIdx = 0;

    if (!storageComp) return;

    // Outer loop: iterate room groups (storageComp+0x58 = group count)
    int groupCount = *(int*)(storageComp + 0x58);
#ifdef _DEBUG
    printf("[ResidenceUI::Slot7] groupCount=%d totalRowSlots=%d\n",
           groupCount, *(int*)(s + 0x38));
#endif

    if (groupCount > 0) {
        uintptr_t groupOffset = 0;

        for (int g = 0; g < groupCount; g++) {
            uintptr_t groupArrayBase = *(uintptr_t*)(storageComp + 0x50);
            int residentCount = *(int*)(groupArrayBase + 8 + groupOffset);

            if (residentCount > 0) {
                totalResidents += residentCount;
                int endRow = rowIdx + residentCount;
#ifdef _DEBUG
                printf("[ResidenceUI::Slot7]   group[%d]: residentCount=%d rowIdx=%d\n",
                       g, residentCount, rowIdx);
#endif

                for (int r = 0; r < residentCount; r++) {
                    // Original CheckPtr's the component IBP every iteration
                    fn_InternalBasePtr_CheckPtr(&storageComp);

                    // Get citizen ptr from resident list + CheckPtr
                    uintptr_t residentListPtr = *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + groupOffset);
                    uintptr_t citizen = *(uintptr_t*)(residentListPtr + r * 8);
                    fn_InternalBasePtr_CheckPtr(&citizen);

#ifdef _DEBUG
                    {
                        uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x30;
                        void* imgM = *(void**)(rowBase + 0x00);
                        void* imgF = *(void**)(rowBase + 0x08);
                        void* txtN = *(void**)(rowBase + 0x10);
                        printf("[ResidenceUI::Slot7]     r[%d] citizen=%p imgM=%p imgF=%p txtN=%p\n",
                               r, (void*)citizen, imgM, imgF, txtN);
                        fflush(stdout);
                    }
#endif

                    // Get row entry: 0x30-byte entries at self+0x30
                    uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x30;

                    // Check gender: citizen+0x38 != 0 means male
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    bool isMale = (*(int*)(citizen + 0x38) != 0);

                    // Show/hide gender icons — CheckPtr each element IBP in row array
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x08));
                    void* imageFemale = *(void**)(rowBase + 0x08);
                    if (imageFemale) {
                        if (isMale) {
                            fn_Element_SetFlagsOnOff(imageFemale, 1, 0);
                        } else {
                            fn_Element_SetFlagsOnOff(imageFemale, 0, 1);
                        }
                    }
                    fn_InternalBasePtr_CheckPtr((void*)rowBase);
                    void* imageMale = *(void**)rowBase;
                    if (imageMale) fn_Element_SetFlagsOnOff(imageMale, (uint32_t)!isMale, (uint32_t)isMale);

                    // Set name text: citizen+0x28 is the name String
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x10));
                    void* textName = *(void**)(rowBase + 0x10);
                    if (textName) fn_TextElement_SetParameter(textName, 0, (void*)(citizen + 0x28));

                    // Set age text: (int)(age * 5.0f) formatted as "%i"
                    fn_InternalBasePtr_CheckPtr(&citizen);
                    uint8_t ageStr[8];
                    int ageYears = (int)(*(float*)(citizen + 0x3c) * ageMul);
                    fn_String_Format(ageStr, "%i", (uint64_t)(uint32_t)ageYears);
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x18));
                    void* textAge = *(void**)(rowBase + 0x18);
                    if (textAge) fn_TextElement_SetParameter(textAge, 0, ageStr);
                    fn_String_Dtor(ageStr);

                    // Determine profession text based on age/death status
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x20));
                    void* textProfession = *(void**)(rowBase + 0x20);

                    fn_InternalBasePtr_CheckPtr(&citizen);
                    float age = *(float*)(citizen + 0x3c);
                    uintptr_t descPtr = *(uintptr_t*)(citizen + 0x288);
                    float oldAgeThreshold = *(float*)(descPtr + 0x44);
                    uintptr_t deadFlag = *(uintptr_t*)(citizen + 0x208);

                    // UI::String is 16 bytes. Construct from ExternalPtr<StringTable> entry.
                    // The UIString ctor from single ExternalPtr is at dispatch 0x140151300.
                    // We call it via fn_UIString_Ctor_Entry(retval, externalPtr).
                    // UI::String ctor needs 3 args: (retbuf, externalPtr at +0x20, stringName at +0x30)
                    // fn_UIString_Ctor_Entry only passes 2 — Ghidra drops R8 (the string name).
                    uint64_t uiStr[2];
                    if (age < oldAgeThreshold && deadFlag == 0) {
                        // Normal citizen: profession from citizen+0x1f0
                        uintptr_t profDesc = *(uintptr_t*)(citizen + 0x1f0);
                        fn_UIString_Ctor(uiStr, *(uint64_t*)(profDesc + 0x20), (void*)(profDesc + 0x30));
                    } else if (deadFlag != 0) {
                        // Dead: "deceased" from ProfessionList +0x48
                        void* world = *g_pGameWorldSingleton;
                        uintptr_t profList = *(uintptr_t*)((uintptr_t)world + 0xB0);
                        uintptr_t entry = *(uintptr_t*)(profList + 0x48);
                        fn_UIString_Ctor(uiStr, *(uint64_t*)(entry + 0x20), (void*)(entry + 0x30));
                    } else {
                        // Old/retired: "elderly" from ProfessionList +0x40
                        void* world = *g_pGameWorldSingleton;
                        uintptr_t profList = *(uintptr_t*)((uintptr_t)world + 0xB0);
                        uintptr_t entry = *(uintptr_t*)(profList + 0x40);
                        fn_UIString_Ctor(uiStr, *(uint64_t*)(entry + 0x20), (void*)(entry + 0x30));
                    }
                    if (textProfession) fn_TextElement_SetText(textProfession, uiStr);

                    // Activate buttonGoto: vtable[8](this)
                    fn_InternalBasePtr_CheckPtr((void*)(rowBase + 0x28));
                    void* buttonGoto = *(void**)(rowBase + 0x28);
                    if (buttonGoto) {
                        void** vtbl = *(void***)buttonGoto;
                        typedef void (*VCall_Activate_t)(void*);
                        ((VCall_Activate_t)vtbl[8])(buttonGoto);
                    }

                    rowIdx++;
                }
            }
            groupOffset += 0x10;
        }
    }

    // Hide unused rows beyond active resident count
    if (totalResidents < *(int*)(s + 0x38)) {
        int i = totalResidents;
        do {
            uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + (uintptr_t)i * 0x30;

            // Hide both gender icons
            void* imageMale = *(void**)(rowBase + 0x00);
            if (imageMale) fn_Element_SetFlagsOnOff(imageMale, 1, 0);
            void* imageFemale = *(void**)(rowBase + 0x08);
            if (imageFemale) fn_Element_SetFlagsOnOff(imageFemale, 1, 0);

            // Clear name text
            uint8_t emptyStr1[8];
            fn_String_Ctor_CStr(emptyStr1, "");
            void* textName = *(void**)(rowBase + 0x10);
            if (textName) fn_TextElement_SetParameter(textName, 0, emptyStr1);
            fn_String_Dtor(emptyStr1);

            // Clear age text
            uint8_t emptyStr2[8];
            fn_String_Ctor_CStr(emptyStr2, "");
            void* textAge = *(void**)(rowBase + 0x18);
            if (textAge) fn_TextElement_SetParameter(textAge, 0, emptyStr2);
            fn_String_Dtor(emptyStr2);

            // Reset profession text: fn_UIString_Ctor(retbuf, externalPtr from +0xD8, empty string)
            void* textProfession = *(void**)(rowBase + 0x20);
            if (textProfession) {
                uint8_t emptyStr3[8];
                fn_String_Ctor_CStr(emptyStr3, "");
                uint64_t origUIStr[2];
                fn_UIString_Ctor(origUIStr,
                    *(uint64_t*)((uintptr_t)textProfession + 0xD8), emptyStr3);
                fn_TextElement_SetText(textProfession, origUIStr);
                fn_String_Dtor(emptyStr3);
            }

            // Deactivate buttonGoto: vtable[8](this, 0)
            void* buttonGoto = *(void**)(rowBase + 0x28);
            if (buttonGoto) {
                void** vtbl = *(void***)buttonGoto;
                typedef void (*VCall_Deactivate_t)(void*, int);
                ((VCall_Deactivate_t)vtbl[8])(buttonGoto, 0);
            }

            i++;
        } while (i < *(int*)(s + 0x38));
    }
}

// ============================================================
// ResidenceUI_GoToCitizen — navigate camera to resident citizen
// ============================================================
// Iterates storage component rooms to find the citizen at the given
// flat index, then centers the camera on that citizen entity.
//
// Storage layout: +0x50 = room array ptr (0x10-byte entries: ptr, count),
//                 +0x58 = room count.
// Each room entry: [citizenListPtr 8][count 4][pad 4].
// citizenListPtr -> array of citizen entity pointers (8 bytes each).
// Citizen entity +0x10 = InternalBasePtr self-reference.
//
// Original: 0x1400B56A0.

static void ResidenceUI_GoToCitizen(ResidenceUI* self, int citizenIndex) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find StorageComponent on the residence entity
    uintptr_t storageComp = 0;
    fn_FindComponent_Storage((void*)entityPtr, &storageComp);

    // Iterate storage room groups to find citizen at flat index
    int flatIdx = 0;
    int groupCount = *(int*)(storageComp + 0x58);

    for (int g = 0; g < groupCount; g++) {
        uintptr_t groupArrayBase = *(uintptr_t*)(storageComp + 0x50);
        int residentCount = *(int*)(groupArrayBase + 8 + (uintptr_t)g * 0x10);

        for (int r = 0; r < residentCount; r++) {
            if (flatIdx == citizenIndex) {
                // Found the target citizen — re-read storageComp after CheckPtr
                fn_InternalBasePtr_CheckPtr(&storageComp);
                uintptr_t citizenPtr = *(uintptr_t*)(
                    *(uintptr_t*)(*(uintptr_t*)(storageComp + 0x50) + (uintptr_t)g * 0x10)
                    + (uintptr_t)r * 8);

                fn_InternalBasePtr_CheckPtr(&citizenPtr);

                // Get citizen's InternalBasePtr self-reference at +0x10
                uint64_t citizenBP = *(uint64_t*)(citizenPtr + 0x10);
                // Get parent dialog ptr from self+0x18
                uint64_t parentDialog = *(uint64_t*)(s + 0x18);

                // Center camera on this citizen via toolController (gameWorld+0x120)
                void* toolController = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
                Toolbar_CenterOnEntity_Impl(toolController, &parentDialog, &citizenBP);
                return;
            }
            flatIdx++;
        }
    }
}

void ResidenceUI_Slot9(ResidenceUI* self, void* param2, int msg, int id) {
    (void)param2;
    uintptr_t s = (uintptr_t)self;

    if (msg != 0x12) return;

    int count = *(int*)(s + 0x38);
    if (id >= 300 && id < 300 + count) {
        ResidenceUI_GoToCitizen(self, id - 300);
    }
}

// ============================================================
