// code/Game/Dialog/townhallui.cpp
// Clean reimplementation of TownHallUI vtable methods.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0.
// Extends DialogController (0x30 base). Multi-tab town overview panel.
//
// All 15 TownHallUI methods are now fully reimplemented as clean C++.
// No exe fn_* pointers remain — all game logic is in this file.
//
// Fully reimplemented:
//   InitGeneral, InitGraph, InitProfessions, InitProduction,
//   InitInventory, InitAgriculture, UpdateGraph, UpdateGeneral,
//   UpdateProfessions, UpdateProduction, UpdateAgriculture,
//   UpdateInventory, UpdateAll, OnProfessionButton, OnProfessionSpinner

#include "ui/Panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>


// ============================================================
// Helpers
// ============================================================

// Find element by name in dialog, wrap as InternalBasePtr
static uint64_t TownHall_FindElemRaw(void* dialogElem, const char* name) {
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, name);

    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(dialogElem, foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;

    uint64_t result = 0;
    fn_InternalBasePtr_Ctor(&result, elem);

    fn_String_Dtor(strBuf);
    return result;
}

// Find element by name with IsDerivedFrom check
static uint64_t TownHall_FindElem(void* dialogElem, const char* name, uint32_t typeId) {
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, name);

    fn_InternalBasePtr_CheckPtr(&dialogElem);
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(dialogElem, foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;

    void* castElem = nullptr;
    if (elem) {
        if (fn_IsDerivedFrom(elem, typeId)) castElem = elem;
    }

    uint64_t result = 0;
    fn_InternalBasePtr_Ctor(&result, castElem);

    fn_String_Dtor(strBuf);
    return result;
}

// ============================================================
// TownHallUI::InitGeneral (was FUN_1400807f0)
// ============================================================
void TownHallUI_InitGeneral(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    self->numberYear     = TownHall_FindElemRaw(dialogElem, "numberYear");
    self->textMonth      = TownHall_FindElemRaw(dialogElem, "textMonth");
    self->numberHomes    = TownHall_FindElemRaw(dialogElem, "numberHomes");
    self->numberCitizens = TownHall_FindElemRaw(dialogElem, "numberCitizens");
    self->numberFamily   = TownHall_FindElemRaw(dialogElem, "numberFamily");
    self->numberAdults   = TownHall_FindElemRaw(dialogElem, "numberAdults");
    self->numberStudents = TownHall_FindElemRaw(dialogElem, "numberStudents");
    self->numberChildren = TownHall_FindElemRaw(dialogElem, "numberChildren");
    self->percentClothed = TownHall_FindElemRaw(dialogElem, "percentClothed");
    self->percentEducated = TownHall_FindElemRaw(dialogElem, "percentEducated");

    uint64_t heartsElem = TownHall_FindElemRaw(dialogElem, "groupHearts");
    fn_GroupRating_Init((void*)(s + 0xB0), &heartsElem);

    uint64_t starsElem = TownHall_FindElemRaw(dialogElem, "groupStars");
    fn_GroupRating_Init((void*)(s + 0xD0), &starsElem);
}

// ============================================================
// TownHallUI::InitGraph (was FUN_14007f0a0)
// ============================================================
void TownHallUI_InitGraph(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    self->graphData = TownHall_FindElemRaw(dialogElem, "graphData");
    self->textXMin  = TownHall_FindElemRaw(dialogElem, "textXMin");
    self->timeCombo = TownHall_FindElemRaw(dialogElem, "timeCombo");
    self->graphCombo = TownHall_FindElemRaw(dialogElem, "graphCombo");

    fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
    fn_Element_SetElementId((void*)self->timeCombo, 0x1964);

    fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
    fn_Element_SetElementId((void*)self->graphCombo, 0x1965);

    uint64_t* numberPtrs = &self->number0;
    for (int i = 0; i < 5; i++) {
        uint8_t fmtBuf[16];
        fn_String_Format(fmtBuf, "number%i", (uint64_t)i);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
        uint64_t fb[2] = {0, 0};
        fn_FindElement(*(void**)(s + 0x20), fb, fmtBuf);
        fn_InternalBasePtr_CheckPtr(fb);
        uint64_t bp = 0;
        fn_InternalBasePtr_Ctor(&bp, *(void**)fb);
        numberPtrs[i] = bp;
        fn_String_Dtor(fmtBuf);
    }

    uint64_t* labelPtrs = &self->label0;
    for (int i = 0; i < 4; i++) {
        uint8_t fmtBuf[16];
        fn_String_Format(fmtBuf, "label%i", (uint64_t)i);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
        uint64_t fb[2] = {0, 0};
        fn_FindElement(*(void**)(s + 0x20), fb, fmtBuf);
        fn_InternalBasePtr_CheckPtr(fb);
        uint64_t bp = 0;
        fn_InternalBasePtr_Ctor(&bp, *(void**)fb);
        labelPtrs[i] = bp;
        fn_String_Dtor(fmtBuf);
    }

    // Load string tables
    {
        uint8_t filenameBuf[16];
        fn_Filename_CtorCStr(filenameBuf, "Dialog/StringTable.rsc:graphYears");
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
        self->graphYearsTable = (ext && fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) ? (uint64_t)ext : 0;
        fn_Filename_Dtor(filenameBuf);
    }
    {
        uint8_t filenameBuf[16];
        fn_Filename_CtorCStr(filenameBuf, "Dialog/StringTable.rsc:graphTypes");
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
        self->graphTypesTable = (ext && fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) ? (uint64_t)ext : 0;
        fn_Filename_Dtor(filenameBuf);
    }

    // Populate time combo
    for (int i = 0; i < 7; i++) {
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor_Index(uiStrBuf, self->graphYearsTable, i);
        uint8_t sysStrBuf[8] = {0};
        fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
        fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
        uint8_t emptySprite[16];
        fn_Sprite_Default_Ctor(emptySprite);
        fn_ComboElement_AddItem((void*)self->timeCombo, emptySprite, sysStrBuf, nullptr);
        fn_String_Dtor(sysStrBuf);
    }

    // Populate graph combo (skip empty entries)
    int typeCount = fn_StringTable_GetCount((void*)self->graphTypesTable);
    int maxEntries = (13 < typeCount) ? typeCount : 13;
    for (int i = 0; i < maxEntries; i++) {
        uint8_t textBuf[8] = {0};
        fn_StringTable_GetText((void*)self->graphTypesTable, textBuf, i);
        void* strPtr = *(void**)textBuf;
        bool isEmpty = !strPtr || (*(int*)((uintptr_t)strPtr - 8) == 0);
        fn_String_Dtor(textBuf);

        if (!isEmpty) {
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor_Index(uiStrBuf, self->graphTypesTable, i);
            uint8_t sysStrBuf[8] = {0};
            fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
            fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
            uint8_t emptySprite[16];
            fn_Sprite_Default_Ctor(emptySprite);
            fn_ComboElement_AddItem((void*)self->graphCombo, emptySprite, sysStrBuf, nullptr);
            fn_String_Dtor(sysStrBuf);
        }
    }

    fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
    fn_ComboElement_SetCurrentItem((void*)self->timeCombo, 0);
    fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
    fn_ComboElement_SetCurrentItem((void*)self->graphCombo, 0);

    TownHallUI_UpdateGraph(self);
}

// ============================================================
// Helper: cast element via IsDerivedFrom check
// ============================================================
static void* CastElement(void* elem, uint32_t* typeIdPtr) {
    if (!elem || !typeIdPtr) return nullptr;
    if (fn_IsDerivedFrom(elem, *typeIdPtr)) return elem;
    return nullptr;
}

// ============================================================
// TownHallUI::InitProfessions (was FUN_140080160)
// ============================================================
// Sets up the professions table: finds "tableProfessions", sizes it
// to profession count, clones rows from template, extracts child
// elements (label, numberEdit, number, spinner, button) per row,
// inserts profession type into BST, sets text/sprite/tooltip/IDs,
// and pushes profession entries into the dynamic array at +0x40.
void TownHallUI_InitProfessions(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "tableProfessions" and cast to TableElement
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, "tableProfessions");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));

    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* foundElem = *(void**)foundBuf;
    void* tableElem = CastElement(foundElem, g_TypeId_TableElement);
    uint64_t tablePtr = 0;
    fn_InternalBasePtr_Ctor(&tablePtr, (void*)tableElem);
    fn_String_Dtor(strBuf);

    // Get profession list from game singleton
    uintptr_t profList = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0);

    // Get template row (Child(0)) and its vtable clone function
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    uint64_t childBuf[2] = {0, 0};
    fn_Element_Child((void*)tablePtr, childBuf, 0);
    fn_InternalBasePtr_CheckPtr(childBuf);
    uintptr_t templateChild = *(uintptr_t*)childBuf;
    // Read the clonable object at field +0xB8 of the template element
    // (NOT vtable+0xB8 — that hits .rdata type name strings)
    uintptr_t cloneObj = *(uintptr_t*)(templateChild + 0xb8);

    // Set table size to profession count x 1
    int profCount = *(int*)(profList + 0x28);
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    fn_TableElement_SetSize((void*)tablePtr, profCount, 1);

    // Iterate rows
    for (int i = 0; i < profCount; i++) {
        // Clone template row into table slot
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        uintptr_t dialogInst = *(uintptr_t*)(*(uintptr_t*)(s + 0x18) + 0x18);

        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
        void* tbl = (void*)tablePtr;

        // Call vtable clone: cloneObj->vtable[+0x40](cloneObj, retBuf, dialogInst)
        typedef void* (*CloneFunc_t)(void* self, void* retBuf, uint64_t dialogInst);
        CloneFunc_t cloneFunc = *(CloneFunc_t*)(*(uintptr_t*)cloneObj + 0x40);
        uint8_t cloneBuf[16];
        void* clonedElem = cloneFunc((void*)cloneObj, cloneBuf, dialogInst);

        fn_TableElement_SetChild3(tbl, clonedElem, i, 0);

        // Get the cloned row element
        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
        uint64_t rowBuf[2] = {0, 0};
        fn_Element_Child((void*)tablePtr, rowBuf, i);
        fn_InternalBasePtr_CheckPtr(rowBuf);
        void* rowElem = *(void**)rowBuf;

        // Get row's child(1) — the spinner/content container
        fn_InternalBasePtr_CheckPtr(rowBuf);
        uint64_t contentBuf[2] = {0, 0};
        fn_Element_Child(rowElem, contentBuf, 1);
        fn_InternalBasePtr_CheckPtr(contentBuf);
        void* contentElem = *(void**)contentBuf;

        // Profession entry struct (0x40 bytes for ProfArrayPush):
        // +0x00: profession index (int)
        // +0x08: label element (InternalBasePtr) — LabelElement
        // +0x10: numberEdit element (InternalBasePtr) — NumberEditElement
        // +0x18: number element (InternalBasePtr) — NumberElement
        // +0x20: spinner element (InternalBasePtr) — NumberElement
        // +0x28: button element (InternalBasePtr) — ButtonElement
        // +0x30: entity list ptr
        // +0x38: entity list count
        uint8_t profEntry[0x40];
        memset(profEntry, 0, sizeof(profEntry));

        // Row child 0: LabelElement (profession icon/name)
        fn_InternalBasePtr_CheckPtr(rowBuf);
        uint64_t c0Buf[2] = {0, 0};
        fn_Element_Child(rowElem, c0Buf, 0);
        fn_InternalBasePtr_CheckPtr(c0Buf);
        void* c0Elem = CastElement(*(void**)c0Buf, g_TypeId_LabelElement);
        uint64_t labelIBP = 0;
        fn_InternalBasePtr_Ctor(&labelIBP, c0Elem);
        *(uint64_t*)(profEntry + 0x08) = labelIBP;

        // Content child 0: NumberEditElement (desired worker count spinner)
        fn_InternalBasePtr_CheckPtr(contentBuf);
        uint64_t c1Buf[2] = {0, 0};
        fn_Element_Child(contentElem, c1Buf, 0);
        fn_InternalBasePtr_CheckPtr(c1Buf);
        void* c1Elem = CastElement(*(void**)c1Buf, g_TypeId_NumberEditElement);
        uint64_t numEditIBP = 0;
        fn_InternalBasePtr_Ctor(&numEditIBP, c1Elem);
        *(uint64_t*)(profEntry + 0x10) = numEditIBP;

        // Row child 2: NumberElement (current count display)
        fn_InternalBasePtr_CheckPtr(rowBuf);
        uint64_t c2Buf[2] = {0, 0};
        fn_Element_Child(rowElem, c2Buf, 2);
        fn_InternalBasePtr_CheckPtr(c2Buf);
        void* c2Elem = CastElement(*(void**)c2Buf, g_TypeId_SpinnerElement);
        uint64_t numIBP = 0;
        fn_InternalBasePtr_Ctor(&numIBP, c2Elem);
        *(uint64_t*)(profEntry + 0x18) = numIBP;

        // Row child 3: NumberElement (another counter)
        fn_InternalBasePtr_CheckPtr(rowBuf);
        uint64_t c3Buf[2] = {0, 0};
        fn_Element_Child(rowElem, c3Buf, 3);
        fn_InternalBasePtr_CheckPtr(c3Buf);
        void* c3Elem = CastElement(*(void**)c3Buf, g_TypeId_SpinnerElement);
        uint64_t spinIBP = 0;
        fn_InternalBasePtr_Ctor(&spinIBP, c3Elem);
        *(uint64_t*)(profEntry + 0x20) = spinIBP;

        // Row child 4: ButtonElement (go-to button)
        fn_InternalBasePtr_CheckPtr(rowBuf);
        uint64_t c4Buf[2] = {0, 0};
        fn_Element_Child(rowElem, c4Buf, 4);
        fn_InternalBasePtr_CheckPtr(c4Buf);
        void* c4Elem = CastElement(*(void**)c4Buf, g_TypeId_HelpButtonElem);
        uint64_t btnIBP = 0;
        fn_InternalBasePtr_Ctor(&btnIBP, c4Elem);
        *(uint64_t*)(profEntry + 0x28) = btnIBP;

        // Get profession type entry: profList->types[i]
        uint64_t profTypeId = *(uint64_t*)(*(uintptr_t*)(profList + 0x20) + (uintptr_t)i * 8);
        *(int*)(profEntry + 0x00) = i;  // profession index

        // Insert into profession BST at +0x50
        uint64_t* pBSTRoot = (uint64_t*)(s + 0x50);
        uint64_t* curNode = (uint64_t*)*pBSTRoot;

        if (curNode == nullptr) {
            // Allocate first BST node (0x30 bytes)
            uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
            if (newNode) {
                newNode[0] = profTypeId;          // +0x00: key
                newNode[1] = (uint64_t)i;         // +0x08: index
                newNode[2] = 0;                    // +0x10: parent
                newNode[3] = 0;                    // +0x18: left
                newNode[4] = 0;                    // +0x20: right
                *(uint32_t*)(newNode + 5) = 0;     // +0x28: pad
            }
            *pBSTRoot = (uint64_t)newNode;
        } else {
            // Walk BST and insert
            uint64_t* parent = nullptr;
            bool inserted = false;
            while (!inserted) {
                parent = curNode;
                if (profTypeId < curNode[0]) {
                    // Go left
                    if (curNode[3] == 0) {
                        uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
                        if (newNode) {
                            newNode[0] = profTypeId;
                            newNode[1] = (uint64_t)i;
                            newNode[2] = (uint64_t)parent;
                            newNode[3] = 0;
                            newNode[4] = 0;
                            *(uint32_t*)(newNode + 5) = 0;
                        }
                        parent[3] = (uint64_t)newNode;
                        // Rebalance
                        fn_BST_Rebalance_0x30((void*)pBSTRoot, (void*)&curNode);
                        inserted = true;
                    } else {
                        curNode = (uint64_t*)curNode[3];
                    }
                } else if (profTypeId > curNode[0]) {
                    // Go right
                    if (curNode[4] == 0) {
                        uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
                        if (newNode) {
                            newNode[0] = profTypeId;
                            newNode[1] = (uint64_t)i;
                            newNode[2] = (uint64_t)parent;
                            newNode[3] = 0;
                            newNode[4] = 0;
                            *(uint32_t*)(newNode + 5) = 0;
                        }
                        parent[4] = (uint64_t)newNode;
                        fn_BST_Rebalance_0x30((void*)pBSTRoot, (void*)&curNode);
                        inserted = true;
                    } else {
                        curNode = (uint64_t*)curNode[4];
                    }
                } else {
                    // Equal — duplicate, skip (go to label setup)
                    inserted = true;
                }
            }
        }
        *(int*)(s + 0x58) = *(int*)(s + 0x58) + 1;

        // Set label text from profession description
        // profTypeId points to profession type object:
        //   +0x20 = StringTable pointer
        //   +0x28 = SpriteSheet pointer
        //   +0x30 = name key (System::String within object)
        //   +0x38 = tooltip key (System::String within object)
        uint8_t uiStr[16];
        fn_UIString_Ctor(uiStr, *(uint64_t*)(profTypeId + 0x20), (void*)(profTypeId + 0x30));
        uint8_t uiStrCopy[16];
        memcpy(uiStrCopy, uiStr, 16);
        fn_InternalBasePtr_CheckPtr((void*)&labelIBP);
        fn_TextElement_SetText((void*)labelIBP, uiStrCopy);

        // Set sprite on label
        uint8_t sprBuf[16];
        fn_Sprite_Ctor(sprBuf, *(uint64_t*)(profTypeId + 0x28), (void*)(profTypeId + 0x30));
        uint8_t sprCopy[16];
        memcpy(sprCopy, sprBuf, 16);
        fn_InternalBasePtr_CheckPtr((void*)&labelIBP);
        fn_LabelElement_SetSprite((void*)labelIBP, sprCopy);

        // Set tooltip
        uint8_t ttStr[16];
        fn_UIString_Ctor(ttStr, *(uint64_t*)(profTypeId + 0x20), (void*)(profTypeId + 0x38));
        uint8_t ttCopy[16];
        memcpy(ttCopy, ttStr, 16);
        fn_InternalBasePtr_CheckPtr((void*)&labelIBP);
        fn_Element_SetToolTip((void*)labelIBP, ttCopy);

        // Set spinner element IDs on the content container (child 1 of row)
        fn_InternalBasePtr_CheckPtr(contentBuf);
        fn_SpinnerElement_SetElementId(contentElem, i + 0x1966);

        // Set inner child(0) of content container with same ID
        fn_InternalBasePtr_CheckPtr(contentBuf);
        uint64_t spinChildBuf[2] = {0, 0};
        fn_Element_Child(contentElem, spinChildBuf, 0);
        fn_InternalBasePtr_CheckPtr(spinChildBuf);
        fn_Element_SetElementId(*(void**)spinChildBuf, i + 0x1966);

        // Set button element ID
        fn_InternalBasePtr_CheckPtr((void*)&btnIBP);
        fn_Element_SetElementId((void*)btnIBP, i + 0x1999);

        // Push profession entry into dynamic array at +0x40
        fn_ProfArrayPush((void*)(s + 0x40), *(int*)(s + 0x48) + 1, profEntry);

        // Free entity list if allocated (profEntry+0x30 was zeroed, so nothing to free)
        fn_MemFreeStatic(*(void**)(profEntry + 0x30));
        *(uint64_t*)(profEntry + 0x30) = 0;
        *(uint64_t*)(profEntry + 0x38) = 0;
    }
}

// ============================================================
// TownHallUI::InitProduction (was FUN_14007fc30)
// ============================================================
// Sets up the production limits table. Finds "limits" TableElement,
// iterates its rows (linear children in groups of colCount),
// extracting 5 NumberElement columns per row into the production
// limits array at +0xF0. Then finds combo elements and populates
// the production year combo from the graphYears string table.
void TownHallUI_InitProduction(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "limits" and cast to TableElement
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, "limits");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* foundElem = *(void**)foundBuf;
    void* tableElem = CastElement(foundElem, g_TypeId_TableElement);
    uint64_t tablePtr = 0;
    fn_InternalBasePtr_Ctor(&tablePtr, (void*)tableElem);
    fn_String_Dtor(strBuf);

    // Read column count from table
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int colCount = *(int*)((uintptr_t)tablePtr + 0xc4);

    // Iterate rows of the table
    // Children are laid out linearly. Each "row" spans colCount children.
    // Column 0 = row label (skipped), columns 1-5 = NumberElements.
    // childBase starts at 2 (first data column after header row),
    // increments by colCount each row.
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int totalChildren = *(int*)((uintptr_t)tablePtr + 0xc0);
    int childBase = 2;  // starts at child index 2 (skip first row header)

    for (int row = 0; row < totalChildren; row++) {
        uint8_t entry[0x28];  // 5 * 8 = 0x28 bytes per production entry
        memset(entry, 0, sizeof(entry));

        // 5 NumberElement columns per row at indices:
        //   childBase-1, childBase, childBase+1, childBase+2, childBase+3
        int indices[5] = {childBase - 1, childBase, childBase + 1, childBase + 2, childBase + 3};
        for (int col = 0; col < 5; col++) {
            fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
            uint64_t cBuf[2] = {0, 0};
            fn_Element_Child((void*)tablePtr, cBuf, indices[col]);
            fn_InternalBasePtr_CheckPtr(cBuf);
            void* cElem = CastElement(*(void**)cBuf, g_TypeId_SpinnerElement);
            uint64_t ibp = 0;
            fn_InternalBasePtr_Ctor(&ibp, cElem);
            *(uint64_t*)(entry + col * 8) = ibp;
        }

        // Push entry into production limits array at +0xF0
        fn_ProdArrayPush((void*)(s + 0xf0), *(int*)(s + 0xf8) + 1, entry);

        childBase += colCount;  // advance to next row
    }

    // Find "productionCombo" ComboElement
    self->productionCombo = TownHall_FindElem(
        *(void**)(s + 0x20), "productionCombo", *g_TypeId_ComboElement);

    // Find "limitPrevUsedHeader" TextElement
    self->limitPrevUsedHdr = TownHall_FindElem(
        *(void**)(s + 0x20), "limitPrevUsedHeader", *g_TypeId_TextElement);

    // Find "limitPrevProducedHeader" TextElement
    self->limitPrevProdHdr = TownHall_FindElem(
        *(void**)(s + 0x20), "limitPrevProducedHeader", *g_TypeId_TextElement);

    // Load graphYears string table and populate combo
    uint8_t fnameBuf[16];
    fn_Filename_CtorCStr(fnameBuf, "Dialog/StringTable.rsc:graphYears");
    void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnameBuf);
    uint64_t yearsTable = 0;
    if (ext && fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) {
        yearsTable = (uint64_t)ext;
    }
    fn_Filename_Dtor(fnameBuf);

    // Add 7 combo items from graphYears string table
    for (int i = 0; i < 7; i++) {
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor_Index(uiStrBuf, yearsTable, i);
        uint8_t sysStrBuf[8] = {0};
        fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
        fn_InternalBasePtr_CheckPtr((void*)&self->productionCombo);
        uint8_t emptySprite[16];
        fn_Sprite_Default_Ctor(emptySprite);
        fn_ComboElement_AddItem((void*)self->productionCombo, emptySprite, sysStrBuf, nullptr);
        fn_String_Dtor(sysStrBuf);
    }
    fn_InternalBasePtr_CheckPtr((void*)&self->productionCombo);
    fn_ComboElement_SetCurrentItem((void*)self->productionCombo, 0);
}

// ============================================================
// TownHallUI::InitInventory (was FUN_14007f660)
// ============================================================
// Sets up the inventory table. Finds "inventoryTable" TableElement,
// counts resource types via ExternalFactory, sizes/clones table rows,
// extracts LabelElement + NumberElement pairs from each cell,
// pushes them into the inventory table array at +0x118.
// Then finds and populates inventoryCombo and sortCombo.
void TownHallUI_InitInventory(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "inventoryTable" and cast to TableElement
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, "inventoryTable");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* foundElem = *(void**)foundBuf;
    void* tableElem = CastElement(foundElem, g_TypeId_TableElement);
    uint64_t tablePtr = 0;
    fn_InternalBasePtr_Ctor(&tablePtr, (void*)tableElem);
    fn_String_Dtor(strBuf);

    // Count resource types via ExternalFactory::FindNext
    int resourceCount = 0;
    void* factory = *g_pExternalFactorySingleton;
    uint32_t resTypeId = *(uint32_t*)g_TypeId_RawMaterialDescription;
    void* extIter = fn_ExternalFactory_FindNext(factory, resTypeId, nullptr);
    while (extIter) {
        resourceCount++;
        extIter = fn_ExternalFactory_FindNext(factory, resTypeId, extIter);
    }

    // Get column count and template row clone vtable
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int colCount = *(int*)((uintptr_t)tablePtr + 0xc4);

    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    uint64_t childBuf[2] = {0, 0};
    fn_Element_Child((void*)tablePtr, childBuf, 0);
    fn_InternalBasePtr_CheckPtr(childBuf);
    uintptr_t templateChild = *(uintptr_t*)childBuf;
    uintptr_t cloneObj = *(uintptr_t*)(templateChild + 0xb8);

    // Size table: ceil(resourceCount / colCount) rows
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int rowCount = ((colCount - 1 + resourceCount) / colCount * colCount) / colCount;
    fn_TableElement_SetSize((void*)tablePtr, rowCount, colCount);

    // Clone template rows into all cells
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int totalChildren = fn_Element_ChildCount((void*)tablePtr);
    for (int i = 0; i < totalChildren; i++) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        uint64_t dialogInst = *(uint64_t*)(*(uintptr_t*)(s + 0x18) + 0x18);

        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);

        typedef void* (*CloneFunc_t)(void* self, void* retBuf, uint64_t dialogInst);
        CloneFunc_t cloneFunc = *(CloneFunc_t*)(*(uintptr_t*)cloneObj + 0x40);
        uint8_t cloneBuf[16];
        void* clonedElem = cloneFunc((void*)cloneObj, cloneBuf, dialogInst);

        fn_TableElement_SetChild2((void*)tablePtr, clonedElem, i);

        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    }

    // Extract label + number elements from each cell
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    totalChildren = fn_Element_ChildCount((void*)tablePtr);
    for (int i = 0; i < totalChildren; i++) {
        // Get cell child
        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
        uint64_t cellBuf[2] = {0, 0};
        fn_Element_Child((void*)tablePtr, cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);

        // Get cell->child(0)->child(0) = LabelElement
        uint64_t sub0[2] = {0, 0};
        fn_Element_Child(*(void**)cellBuf, sub0, 0);
        fn_InternalBasePtr_CheckPtr(sub0);
        void* labelElem = CastElement(*(void**)sub0, g_TypeId_LabelElement);
        uint64_t labelIBP = 0;
        fn_InternalBasePtr_Ctor(&labelIBP, labelElem);

        // Get cell->child(0)->child(1) = NumberElement
        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
        uint64_t cellBuf2[2] = {0, 0};
        fn_Element_Child((void*)tablePtr, cellBuf2, i);
        fn_InternalBasePtr_CheckPtr(cellBuf2);
        uint64_t sub1[2] = {0, 0};
        fn_Element_Child(*(void**)cellBuf2, sub1, 1);
        fn_InternalBasePtr_CheckPtr(sub1);
        void* numElem = CastElement(*(void**)sub1, g_TypeId_SpinnerElement);
        uint64_t numIBP = 0;
        fn_InternalBasePtr_Ctor(&numIBP, numElem);

        // Push 0x10-byte entry (label, number) into inventory table array at +0x118
        // Manual dynamic array growth (matching decompiled pattern)
        uint32_t count = *(uint32_t*)(s + 0x120);
        uint32_t newCount = count + 1;
        if (newCount != count) {
            void* oldBuf = *(void**)(s + 0x118);
            *(uint32_t*)(s + 0x120) = newCount;
            if (*(int*)(s + 0x124) < (int)newCount) {
                uint32_t newCap = newCount;
                if (count != 0) newCap = newCount * 3;
                *(uint32_t*)(s + 0x124) = newCap;
                void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 16, 8);
                *(void**)(s + 0x118) = newBuf;
            }
            // Copy old entries and append new
            int copyStart = 0;
            if (oldBuf == *(void**)(s + 0x118)) {
                copyStart = (int)count;
            }
            for (int j = copyStart; j < (int)newCount; j++) {
                uint64_t* dst = (uint64_t*)(*(uintptr_t*)(s + 0x118) + (uintptr_t)j * 16);
                if (dst) {
                    if (j < (int)count) {
                        // Copy from old buffer
                        uint64_t* src = (uint64_t*)((uintptr_t)oldBuf + (uintptr_t)j * 16);
                        dst[0] = src[0];
                        dst[1] = src[1];
                    } else {
                        // New entry
                        dst[0] = labelIBP;
                        dst[1] = numIBP;
                    }
                }
            }
            if (oldBuf != *(void**)(s + 0x118)) {
                fn_MemFreeStatic(oldBuf);
            }
        }
    }

    // Load inventoryType string table
    uint8_t fnameBuf[16];
    fn_Filename_CtorCStr(fnameBuf, "Dialog/StringTable.rsc:inventoryType");
    void* extST = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnameBuf);
    uint64_t invTypeTable = 0;
    if (extST && fn_IsDerivedFrom(extST, *g_TypeId_StringTable)) {
        invTypeTable = (uint64_t)extST;
    }
    fn_Filename_Dtor(fnameBuf);

    // Find "inventoryCombo" ComboElement
    self->inventoryCombo = TownHall_FindElem(
        *(void**)(s + 0x20), "inventoryCombo", *g_TypeId_ComboElement);

    // Populate inventory combo with 3 items
    for (int i = 0; i < 3; i++) {
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor_Index(uiStrBuf, invTypeTable, i);
        uint8_t sysStrBuf[8] = {0};
        fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
        fn_InternalBasePtr_CheckPtr((void*)&self->inventoryCombo);
        uint8_t emptySprite[16];
        fn_Sprite_Default_Ctor(emptySprite);
        fn_ComboElement_AddItem((void*)self->inventoryCombo, emptySprite, sysStrBuf, nullptr);
        fn_String_Dtor(sysStrBuf);
    }
    fn_InternalBasePtr_CheckPtr((void*)&self->inventoryCombo);
    fn_ComboElement_SetCurrentItem((void*)self->inventoryCombo, 0);

    // Find "sortCombo" ComboElement
    self->sortCombo = TownHall_FindElem(
        *(void**)(s + 0x20), "sortCombo", *g_TypeId_ComboElement);
    fn_InternalBasePtr_CheckPtr((void*)&self->sortCombo);
    fn_ComboElement_SetCurrentItem((void*)self->sortCombo, 0);
}

// ============================================================
// TownHallUI::InitAgriculture (was FUN_14007ebf0)
// ============================================================
// Sets up the agriculture table. Iterates ExternalFactory for
// ComponentDescription entities, finds SelectDescription sub-components,
// inserts matching building descriptions into BST at +0x1D8.
// Then sizes/clones table rows and extracts LabelElement from
// each cell into the dynamic array at +0x1C8.
void TownHallUI_InitAgriculture(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "agriTable" and cast to TableElement
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, "agriTable");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* foundElem = *(void**)foundBuf;
    void* tableElem = CastElement(foundElem, g_TypeId_TableElement);
    uint64_t tablePtr = 0;
    fn_InternalBasePtr_Ctor(&tablePtr, (void*)tableElem);
    fn_String_Dtor(strBuf);

    // Iterate ExternalFactory for ComponentDescription entities
    void* factory = *g_pExternalFactorySingleton;
    uint32_t buildTypeId = *g_TypeId_ComponentDescription_FC;
    uint32_t selectDescId = *g_TypeId_SelectDescription;

    void* buildDesc = fn_ExternalFactory_FindNext(factory, buildTypeId, nullptr);
    while (buildDesc) {
        // Cast to ComponentDescription (IsDerivedFrom check)
        void* buildComp = nullptr;
        if (fn_IsDerivedFrom(buildDesc, buildTypeId)) {
            buildComp = buildDesc;
        }

        if (buildComp) {
            // Iterate sub-components to find SelectDescription
            int subCount = *(int*)((uintptr_t)buildComp + 0x30);
            uintptr_t subArray = *(uintptr_t*)((uintptr_t)buildComp + 0x28);

            for (int j = 0; j < subCount; j++) {
                uintptr_t subComp = *(uintptr_t*)(subArray + (uintptr_t)j * 8);
                // Check if sub-component's type matches SelectDescription
                // Virtual call at vtable+0x10 returns type ID
                typedef int (*GetTypeId_t)(void*);
                GetTypeId_t getTypeId = *(GetTypeId_t*)(*(uintptr_t*)subComp + 0x10);
                int subTypeId = getTypeId((void*)subComp);

                if (subTypeId == (int)selectDescId) {
                    // Found a SelectDescription sub-component
                    void* subObj = *(void**)(subArray + (uintptr_t)j * 8);
                    if (fn_IsDerivedFrom(subObj, selectDescId) && subObj) {
                        // Insert buildComp into agriculture BST at +0x1D8
                        uint64_t* pBSTRoot = (uint64_t*)(s + 0x1D8);
                        uint64_t key = (uint64_t)buildComp;
                        uint64_t* curNode = (uint64_t*)*pBSTRoot;

                        if (curNode == nullptr) {
                            // First node (0x28 bytes)
                            uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
                            if (newNode) {
                                newNode[0] = key;
                                newNode[1] = 0;  // parent
                                newNode[2] = 0;  // left
                                newNode[3] = 0;  // right
                                *(uint32_t*)(newNode + 4) = 0;
                            }
                            *pBSTRoot = (uint64_t)newNode;
                            *(int*)(s + 0x1E0) = *(int*)(s + 0x1E0) + 1;
                        } else {
                            // Walk BST and insert
                            uint64_t* parent = nullptr;
                            bool inserted = false;
                            while (!inserted) {
                                parent = curNode;
                                if (key < curNode[0]) {
                                    if (curNode[2] == 0) {
                                        uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
                                        if (newNode) {
                                            newNode[0] = key;
                                            newNode[1] = (uint64_t)parent;
                                            newNode[2] = 0;
                                            newNode[3] = 0;
                                            *(uint32_t*)(newNode + 4) = 0;
                                        }
                                        parent[2] = (uint64_t)newNode;
                                        fn_BST_Rebalance_0x28((void*)pBSTRoot, (void*)&curNode);
                                        inserted = true;
                                    } else {
                                        curNode = (uint64_t*)curNode[2];
                                    }
                                } else if (key > curNode[0]) {
                                    if (curNode[3] == 0) {
                                        uint64_t* newNode = (uint64_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
                                        if (newNode) {
                                            newNode[0] = key;
                                            newNode[1] = (uint64_t)parent;
                                            newNode[2] = 0;
                                            newNode[3] = 0;
                                            *(uint32_t*)(newNode + 4) = 0;
                                        }
                                        parent[3] = (uint64_t)newNode;
                                        fn_BST_Rebalance_0x28((void*)pBSTRoot, (void*)&curNode);
                                        inserted = true;
                                    } else {
                                        curNode = (uint64_t*)curNode[3];
                                    }
                                } else {
                                    // Duplicate — already in BST
                                    inserted = true;
                                }
                            }
                            *(int*)(s + 0x1E0) = *(int*)(s + 0x1E0) + 1;
                        }
                    }
                    break;  // Found matching sub-component, stop inner loop
                }
            }
        }

        buildDesc = fn_ExternalFactory_FindNext(factory, buildTypeId, buildDesc);
    }

    // Now set up the table with agriculture BST count
    int agriCount = *(int*)(s + 0x1E0);
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int colCount = *(int*)((uintptr_t)tablePtr + 0xc4);

    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    uint64_t templateBuf[2] = {0, 0};
    fn_Element_Child((void*)tablePtr, templateBuf, 0);
    fn_InternalBasePtr_CheckPtr(templateBuf);
    uintptr_t templateChild = *(uintptr_t*)templateBuf;
    uintptr_t cloneObj = *(uintptr_t*)(templateChild + 0xb8);

    // Size table: ceil(agriCount / colCount) rows
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int rowCount = ((colCount - 1 + agriCount) / colCount * colCount) / colCount;
    fn_TableElement_SetSize((void*)tablePtr, rowCount, colCount);

    // Clone template rows into all cells
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    int totalChildren = fn_Element_ChildCount((void*)tablePtr);
    for (int i = 0; i < totalChildren; i++) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        uint64_t dialogInst = *(uint64_t*)(*(uintptr_t*)(s + 0x18) + 0x18);

        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);

        typedef void* (*CloneFunc_t)(void* self, void* retBuf, uint64_t dialogInst);
        CloneFunc_t cloneFunc = *(CloneFunc_t*)(*(uintptr_t*)cloneObj + 0x40);
        uint8_t cloneBuf[16];
        void* clonedElem = cloneFunc((void*)cloneObj, cloneBuf, dialogInst);

        fn_TableElement_SetChild2((void*)tablePtr, clonedElem, i);

        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    }

    // Extract LabelElement from each cell into array at +0x1C8
    fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
    totalChildren = fn_Element_ChildCount((void*)tablePtr);
    for (int i = 0; i < totalChildren; i++) {
        fn_InternalBasePtr_CheckPtr((void*)&tablePtr);
        uint64_t cellBuf[2] = {0, 0};
        fn_Element_Child((void*)tablePtr, cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);

        // Cast child to LabelElement
        void* labelElem = CastElement(*(void**)cellBuf, g_TypeId_LabelElement);
        uint64_t labelIBP = 0;
        fn_InternalBasePtr_Ctor(&labelIBP, labelElem);

        // Push into dynamic array at +0x1C8 (8-byte entries)
        uint32_t count = *(uint32_t*)(s + 0x1D0);
        uint32_t newCount = count + 1;
        if (newCount != count) {
            void* oldBuf = *(void**)(s + 0x1C8);
            *(uint32_t*)(s + 0x1D0) = newCount;
            if (*(int*)(s + 0x1D4) < (int)newCount) {
                uint32_t newCap = newCount;
                if (count != 0) newCap = newCount * 3;
                *(uint32_t*)(s + 0x1D4) = newCap;
                void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
                *(void**)(s + 0x1C8) = newBuf;
            }
            int copyStart = 0;
            if (oldBuf == *(void**)(s + 0x1C8)) {
                copyStart = (int)count;
            }
            for (int j = copyStart; j < (int)newCount; j++) {
                uint64_t* dst = (uint64_t*)(*(uintptr_t*)(s + 0x1C8) + (uintptr_t)j * 8);
                if (dst) {
                    if (j < (int)count) {
                        uint64_t* src = (uint64_t*)((uintptr_t)oldBuf + (uintptr_t)j * 8);
                        *dst = *src;
                    } else {
                        *dst = labelIBP;
                    }
                }
            }
            if (oldBuf != *(void**)(s + 0x1C8)) {
                fn_MemFreeStatic(oldBuf);
            }
        }
    }
}

// ============================================================
// TownHallUI::UpdateGraph (was FUN_14007e5b0)
// ============================================================
// Reads time/graph combo selections, populates the graph element
// with historical data series, and updates y-axis labels.
void TownHallUI_UpdateGraph(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Cache game tick
    self->cachedGameTick = *(int*)(*(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0) + 0x150) + 8);

    // Read combo selections
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
    int timeIdx = *(int*)(*(uintptr_t*)(s + 0x168) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
    int graphIdx = *(int*)(*(uintptr_t*)(s + 0x170) + 0x118);

    // Data series indices (-1 = hidden)
    int graphSeries[5] = {-1, -1, -1, -1, 0};
    uint32_t colors[4] = {0xffffff00, 0xff00ff00, 0xff8080ff, 0xffff0000};

    int stepSize = 0, pointCount = 0;
    uint32_t ticksPerStep = 0;

    switch (timeIdx) {
    case 0: stepSize=1;  pointCount=0x30; ticksPerStep=0xc;  break;
    case 1: stepSize=1;  pointCount=0x60; ticksPerStep=0x18; break;
    case 2: stepSize=2;  pointCount=0x78; ticksPerStep=5;    break;
    case 3: stepSize=4;  pointCount=0x78; ticksPerStep=10;   break;
    case 4: stepSize=8;  pointCount=0x96; ticksPerStep=0x19; break;
    case 5: stepSize=0x10; pointCount=0x96; ticksPerStep=0x32; break;
    case 6: stepSize=0x20; pointCount=0x96; ticksPerStep=100; break;
    }

    switch (graphIdx) {
    case 0:  graphSeries[0]=0;    graphSeries[1]=1;  graphSeries[2]=2;  graphSeries[3]=3;  break;
    case 1:  graphSeries[0]=4;    graphSeries[1]=6;  graphSeries[2]=5;  graphSeries[3]=7;  break;
    case 2:  graphSeries[0]=0x17; graphSeries[1]=-1; break;
    case 3:  graphSeries[0]=8;    graphSeries[1]=-1; break;
    case 4:  graphSeries[0]=0xb;  graphSeries[1]=-1; break;
    case 5:  graphSeries[0]=0xe;  graphSeries[1]=-1; break;
    case 6:  graphSeries[0]=0x11; graphSeries[1]=-1; break;
    case 7:  graphSeries[0]=0x23; graphSeries[1]=-1; break;
    case 8:  graphSeries[0]=0x14; graphSeries[1]=-1; break;
    case 9:  graphSeries[0]=0x1a; graphSeries[1]=-1; break;
    case 10: graphSeries[0]=0x1d; graphSeries[1]=-1; break;
    case 11: graphSeries[0]=0x20; graphSeries[1]=-1; break;
    case 12: graphSeries[0]=0x26; graphSeries[1]=-1; break;
    case 13: graphSeries[0]=0x29; graphSeries[1]=-1; break;
    case 14: graphSeries[0]=0x2c; graphSeries[1]=-1; break;
    case 15: graphSeries[0]=0x2f; graphSeries[1]=-1; break;
    case 16: graphSeries[0]=0x32; graphSeries[1]=-1; break;
    case 17: graphSeries[0]=0x35; graphSeries[1]=-1; break;
    case 18: graphSeries[0]=0x38; graphSeries[1]=-1; break;
    case 19: graphSeries[0]=0x3b; graphSeries[1]=-1; break;
    case 20: graphSeries[0]=0x3e; graphSeries[1]=-1; break;
    case 21: graphSeries[0]=0x41; graphSeries[1]=-1; break;
    case 22: graphSeries[0]=0x44; graphSeries[1]=-1; break;
    }

    pointCount += 1;
    graphSeries[4] = pointCount;

    // Set x-axis step
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    *(uint32_t*)(*(uintptr_t*)(s + 0x160) + 0xe4) = ticksPerStep;

    // Set x-axis min label (UIString from graphYearsTable at timeIdx)
    uint8_t xMinStr[16];
    uint64_t* pXMin = (uint64_t*)fn_UIString_Ctor_Index(xMinStr, self->graphYearsTable, timeIdx);
    uint8_t xMinCopy[16];
    memcpy(xMinCopy, pXMin, 16);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x178));
    fn_TextElement_SetText(*(void**)(s + 0x178), xMinCopy);

    // Remove existing graphs
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    fn_GraphElement_RemoveGraphs(*(void**)(s + 0x160));

    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    int currentTick = *(int*)(*(uintptr_t*)(gameData + 0x150) + 8);
    int maxVal = 0;
    int graphCount = 0;

    for (int g = 0; g < 4; g++) {
        int seriesIdx = graphSeries[g];
        uint64_t* labelPtr = &self->label0 + g;

        if (seriesIdx < 0) {
            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 1, 0);
        } else {
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
            fn_GraphElement_AddGraph(*(void**)(s + 0x160), colors[g]);

            int startIdx;
            if (currentTick / stepSize < pointCount) {
                int padCount = pointCount - currentTick / stepSize;
                for (int p = 0; p < padCount; p++) {
                    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
                    fn_GraphElement_AddGraphPoint(*(void**)(s + 0x160), graphCount, 0);
                }
                startIdx = 0;
            } else {
                startIdx = currentTick - pointCount * stepSize;
            }

            for (int64_t idx = startIdx; idx < currentTick; idx += stepSize) {
                uintptr_t histBase = *(uintptr_t*)(*(uintptr_t*)(gameData + 0x150) + (uintptr_t)seriesIdx * 0x10);
                int val = *(int*)(histBase + idx * 4);
                if (val > maxVal) maxVal = val;
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
                fn_GraphElement_AddGraphPoint(*(void**)(s + 0x160), graphCount, val);
            }

            // Set label color
            fn_InternalBasePtr_CheckPtr(labelPtr);
            *(uint32_t*)(*(uintptr_t*)labelPtr + 0x128) = colors[g];

            // Set label text from string table
            fn_InternalBasePtr_CheckPtr(labelPtr);
            uint64_t lblST = *(uint64_t*)(*(uintptr_t*)labelPtr + 0xd8);
            uint8_t lblStr[16];
            uint64_t* pLbl = (uint64_t*)fn_UIString_Ctor_Index(lblStr, lblST, seriesIdx);
            uint8_t lblCopy[16];
            memcpy(lblCopy, pLbl, 16);
            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_TextElement_SetText(*(void**)labelPtr, lblCopy);

            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 0, 1);

            pointCount = graphSeries[4];
            graphCount++;
        }
    }

    // Round up y-axis max
    if (maxVal < 5) maxVal = 5;
    int scale;
    if      (maxVal < 10)   scale = 1;
    else if (maxVal < 100)  scale = 10;
    else if (maxVal < 1000) scale = 100;
    else                    scale = 1000;
    int yMax = ((maxVal - 1 + scale) / scale) * scale;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    *(float*)(*(uintptr_t*)(s + 0x160) + 0xe0) = (float)yMax;

    // Y-axis labels: max, 3/4, 1/2, 1/4
    float quarter = (float)yMax * 0.25f;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x180));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x180), yMax);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x188));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x188), (int)(quarter * 3.0f));
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x190));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x190), (int)(quarter * 2.0f));
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x198));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x198), (int)quarter);
}

// ============================================================
// TownHallUI::UpdateGeneral (was FUN_14007d030)
// ============================================================
void TownHallUI_UpdateGeneral(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);

    int totalCitizens = *(int*)(gameData + 0xa8);
    int adults = 0, students = 0, children = 0, familyCount = 0;
    float clothedSum = 0.0f, educatedSum = 0.0f, happinessSum = 0.0f, healthSum = 0.0f;
    float perCitizen = (totalCitizens > 0) ? (1.0f / (float)totalCitizens) : 0.0f;

    for (int i = 0; i < totalCitizens; i++) {
        uintptr_t citizen = *(uintptr_t*)(*(uintptr_t*)(gameData + 0xa0) + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);

        float age = *(float*)(citizen + 0x3c);
        float adultAge = *(float*)(*(uintptr_t*)(citizen + 0x288) + 0x44);
        uintptr_t school = *(uintptr_t*)(citizen + 0x208);

        if (age < adultAge || school != 0) {
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            if (school == 0) { children++; }
            else             { students++; }
        } else {
            adults++;
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            if (*(int*)(*(uintptr_t*)(citizen + 0x288) + 0x118) <= *(int*)(citizen + 0x210)) {
                clothedSum += 1.0f;
            }
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            uintptr_t home = *(uintptr_t*)(citizen + 0x250);
            if (home == 0) {
                fn_InternalBasePtr_CheckPtr((void*)&citizen);
                if (*(uintptr_t*)(citizen + 0x250) == 0) {
                    familyCount++;
                }
            } else {
                fn_InternalBasePtr_CheckPtr((void*)&citizen);
                if (*(int*)(citizen + 0x38) == 0) {
                    familyCount++;
                }
            }
        }

        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        healthSum += *(float*)(citizen + 0x80);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        happinessSum += *(float*)(citizen + 0x50);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        if (*(int*)(citizen + 0x1e4) != 0) {
            educatedSum += 1.0f;
        }
    }

    float healthAvg = healthSum * perCitizen;
    float educatedPct = educatedSum * perCitizen * 100.0f;
    float clothedPct = (adults > 0) ? ((clothedSum / (float)adults) * 100.0f) : 0.0f;

    // Year
    int year = **(int**)(*(uintptr_t*)g_pGameSingleton + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x60));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x60), year);

    // Month
    float seconds = fn_Time_AsSeconds((void*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0x128) + 8));
    int monthIdx = (int)((seconds / 3600.0f) * 12.0f);
    if (monthIdx > 11) monthIdx = 0;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x68));
    uint64_t monthST = *(uint64_t*)(*(uintptr_t*)(s + 0x68) + 0xd8);
    uint8_t monthStr[16];
    uint64_t* pMonth = (uint64_t*)fn_UIString_Ctor_Index(monthStr, monthST, monthIdx);
    uint8_t monthCopy[16];
    memcpy(monthCopy, pMonth, 16);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x68));
    fn_TextElement_SetText(*(void**)(s + 0x68), monthCopy);

    // Home count
    int homeCount = *(int*)(gameData + 0x28);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x70));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x70), homeCount);

    // Citizen counts
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x80));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x80), totalCitizens);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x88));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x88), adults);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x90));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x90), students);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x98));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x98), children);

    // Educated % (at +0xa0)
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)(uint32_t)(int)(educatedPct + 0.5f));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xa0));
        fn_TextElement_SetParameter(*(void**)(s + 0xa0), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    // Clothed % (at +0xa8)
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)(uint32_t)(int)(clothedPct + 0.5f));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xa8));
        fn_TextElement_SetParameter(*(void**)(s + 0xa8), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    // Family count
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x78));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x78), familyCount);

    // Hearts (happiness) and Stars (health) ratings
    if (totalCitizens < 1) {
        fn_GroupRating_Update((void*)(s + 0xb0), 0.0f, 1.0f);
        fn_GroupRating_Update((void*)(s + 0xd0), 0.0f, 1.0f);
    } else {
        uintptr_t firstCitizen = **(uintptr_t**)(gameData + 0xa0);
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        void* desc = *(void**)(firstCitizen + 0x18);
        void* citizenDesc = fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription) ? desc : nullptr;

        fn_GroupRating_Update((void*)(s + 0xb0), happinessSum * perCitizen,
                              *(float*)((uintptr_t)citizenDesc + 0x70));

        // Re-read first citizen for health max
        firstCitizen = **(uintptr_t**)(gameData + 0xa0);
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        desc = *(void**)(firstCitizen + 0x18);
        citizenDesc = fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription) ? desc : nullptr;

        fn_GroupRating_Update((void*)(s + 0xd0), healthAvg,
                              *(float*)((uintptr_t)citizenDesc + 0xa4));
    }
}

// ============================================================
// TownHallUI::UpdateProfessions (was FUN_14007ccb0)
// ============================================================
void TownHallUI_UpdateProfessions(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int comboIdx = *(int*)(*(uintptr_t*)(s + 0x100) + 0x118);

    uint32_t yearSpan = 0;
    switch (comboIdx) {
    case 0: yearSpan = 1; break;
    case 1: yearSpan = 2; break;
    case 2: yearSpan = 5; break;
    case 3: yearSpan = 10; break;
    case 4: yearSpan = 25; break;
    case 5: yearSpan = 50; break;
    case 6: yearSpan = 100; break;
    }

    // Set header text parameters
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)yearSpan);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
        fn_TextElement_SetParameter(*(void**)(s + 0x108), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)yearSpan);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_TextElement_SetParameter(*(void**)(s + 0x110), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    int* timePtr = *(int**)(*(uintptr_t*)g_pGameSingleton + 0x128);
    int currentYearTick = timePtr[0] * 0x30 - 0x30;
    float secs = fn_Time_AsSeconds((void*)(timePtr + 2));
    uint32_t monthTick = (uint32_t)((secs / 3600.0f) * 48.0f);
    if ((int)monthTick > 0x2f) monthTick = 0;
    int endTick = monthTick + currentYearTick;
    int prevEnd = timePtr[0] * 0x30 - 0x31;
    int prevStart = (timePtr[0] - (int)yearSpan - 1) * 0x30;

    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    uint32_t resIdx = 10;
    int prodCount = *(int*)(s + 0xf8);

    if (prodCount > 0) {
        int64_t entryOff = 0, dataOff = 0;
        for (int i = 0; i < prodCount; i++) {
            int stock = *(int*)(*(uintptr_t*)(gameData + 0x170) + dataOff);

            uintptr_t entry = *(uintptr_t*)(s + 0xf0) + entryOff;
            fn_InternalBasePtr_CheckPtr((void*)entry);
            fn_NumberElement_SetValueInt(*(void**)entry, stock);

            uintptr_t prodData = *(uintptr_t*)(gameData + 0x170);
            int usedCur = fn_QueryProductionStat((void*)gameData, resIdx - 1, currentYearTick, endTick);
            int usedStock = *(int*)(prodData + 0xc + dataOff);
            int prodCur = fn_QueryProductionStat((void*)gameData, resIdx, currentYearTick, endTick);
            int prodStock = *(int*)(prodData + 0x10 + dataOff);
            int usedPrev = fn_QueryProductionStat((void*)gameData, resIdx - 1, prevStart, prevEnd);
            int prodPrev = fn_QueryProductionStat((void*)gameData, resIdx, prevStart, prevEnd);

            uintptr_t arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 8));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 8), usedCur + usedStock);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x10));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x10), prodCur + prodStock);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x18));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x18), usedPrev);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x20));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x20), prodPrev);

            resIdx += 3;
            dataOff += 0x20;
            entryOff += 0x28;
        }
    }
}

// ============================================================
// BST in-order traversal helpers for TownHallUI
// ============================================================

// invBST node layout (0x38 bytes, node children at +0x20/+0x28):
//   [0x00] key (entity ptr / uint64_t)
//   [0x08] count0 (int32) — used count
//   [0x0C] count1 (int32) — produced count
//   [0x10] count2 (int32) — storage count
//   [0x14] pad
//   [0x18] parent (ptr)  -- actually data2 in some layouts
//   [0x20] left child (ptr)
//   [0x28] right child (ptr)
//   [0x30] balance/color (uint32)

// agriBST node layout (0x28 bytes, node children at +0x10/+0x18):
//   [0x00] key (entity ptr)
//   [0x08] parent (ptr)
//   [0x10] left child (ptr)
//   [0x18] right child (ptr)
//   [0x20] balance (uint32)

// Go to leftmost node of a BST subtree (children at leftOff/rightOff)
static void* BST_Leftmost(void* node, int leftOff) {
    if (!node) return nullptr;
    while (*(void**)((uintptr_t)node + leftOff))
        node = *(void**)((uintptr_t)node + leftOff);
    return node;
}

// In-order successor for invBST (left=+0x20, right=+0x28, parent=+0x18)
static void* InvBST_Next(void* node) {
    if (!node) return nullptr;
    uintptr_t n = (uintptr_t)node;
    void* right = *(void**)(n + 0x28);
    if (right) {
        return BST_Leftmost(right, 0x20);
    }
    // Go up: find first ancestor where we came from the left
    void* parent = *(void**)(n + 0x18);
    while (parent) {
        uintptr_t p = (uintptr_t)parent;
        if (*(void**)(p + 0x20) == node) return parent; // came from left child
        if (*(void**)(p + 0x28) == node) {
            node = parent;
            parent = *(void**)(p + 0x18);
        } else {
            break;
        }
    }
    return nullptr;
}

// In-order successor for agriBST (left=+0x10, right=+0x18, parent=+0x08)
static void* AgriBST_Next(void* node) {
    if (!node) return nullptr;
    uintptr_t n = (uintptr_t)node;
    void* right = *(void**)(n + 0x18);
    if (right) {
        return BST_Leftmost(right, 0x10);
    }
    void* parent = *(void**)(n + 0x08);
    while (parent) {
        uintptr_t p = (uintptr_t)parent;
        if (*(void**)(p + 0x10) == node) return parent;
        if (*(void**)(p + 0x18) == node) {
            node = parent;
            parent = *(void**)(p + 0x08);
        } else {
            break;
        }
    }
    return nullptr;
}

// Find entity component by type ID
// Note: these entities use +0x28/+0x30 layout (not +0x30/+0x38 like ComponentEntity)
static void* TownHall_FindComponentByTypeId(uintptr_t entity, uint32_t typeId) {
    if (!entity) {
        return nullptr;
    }
    void** compArray = *(void***)(entity + 0x28);
    int compCount = *(int*)(entity + 0x30);

    if (!compArray) {
        return nullptr;
    }
    if (compCount < 0 || compCount > 200) {
        return nullptr;
    }

    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if ((uintptr_t)comp < 0x10000 || (uintptr_t)comp >= 0xFFFFFFFF00000000ULL) {
            continue;
        }
        // vtable slot 2 (+0x10) = GetTypeId()
        void** vtable = *(void***)comp;
        if (!vtable) {
            continue;
        }
        typedef int (*fn_GetTypeId_t)(void*);
        int id = ((fn_GetTypeId_t)vtable[2])(comp);
        if ((uint32_t)id == typeId) {
            bool derived = fn_IsDerivedFrom(comp, typeId);
            if (derived)
                return comp;
            break;
        }
    }
    return nullptr;
}

// Find component on a ComponentEntity by type ID
// ComponentEntity layout: +0x30 = IBP array, +0x38 = count
// Each array element is an InternalBasePtr that needs CheckPtr before deref.
static void* TownHall_FindComponentByTypeId_CE(uintptr_t entity, uint32_t typeId) {
    if (!entity) {
        return nullptr;
    }
    uintptr_t compArrayBase = *(uintptr_t*)(entity + 0x30);
    int compCount = *(int*)(entity + 0x38);

    if (!compArrayBase) {
        return nullptr;
    }
    if (compCount < 0 || compCount > 200) {
        return nullptr;
    }

    for (int i = 0; i < compCount; i++) {
        // Each element is an IBP — CheckPtr it before reading
        void* ibpSlot = (void*)(compArrayBase + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(ibpSlot);
        void* comp = *(void**)ibpSlot;
        if (!comp) continue;

        void** vtable = *(void***)comp;
        if (!vtable) continue;

        typedef int (*fn_GetTypeId_t)(void*);
        int id = ((fn_GetTypeId_t)vtable[2])(comp);
        if ((uint32_t)id == typeId) {
            bool derived = fn_IsDerivedFrom(comp, typeId);
            if (derived)
                return comp;
            break;
        }
    }
    return nullptr;
}

// Look up a key in invBST (+0x138). Returns node or nullptr.
static void* InvBST_Find(void* root, uint64_t key) {
    void* node = root;
    while (node) {
        uint64_t nodeKey = *(uint64_t*)node;
        if (key < nodeKey)
            node = *(void**)((uintptr_t)node + 0x20);
        else if (key > nodeKey)
            node = *(void**)((uintptr_t)node + 0x28);
        else
            return node;
    }
    return nullptr;
}

// Element vtable call at +0x50 (SetValueLimit/SetDisplayMode)
static void Element_VCall50(void* elem, int val) {
    uintptr_t vtable = *(uintptr_t*)elem;
    auto fn = *(void (**)(void*, int))(vtable + 0x50);
    fn(elem, val);
}

// Element vtable call at +0x40 (SetVisible/SetEnabled)
static void Element_VCall40(void* elem, int val) {
    uintptr_t vtable = *(uintptr_t*)elem;
    auto fn = *(void (**)(void*, int))(vtable + 0x40);
    fn(elem, val);
}

// ============================================================
// TownHallUI::UpdateProduction (was FUN_14007c7f0)
// ============================================================
// Traverses invBST (+0x138) in-order, collects entries with
// ResourceType components, sorts them, then updates the
// inventory table UI elements (+0x118 array).
void TownHallUI_UpdateProduction(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Read combo selections
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
    int invComboIdx = *(int*)(*(uintptr_t*)(s + 0x128) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
    int sortComboIdx = *(int*)(*(uintptr_t*)(s + 0x130) + 0x118);

    // Byte offset into BST node for the selected counter
    int counterOffset = invComboIdx * 4 + 8;

    // Collect entries from invBST in-order traversal
    struct ProdEntry {
        uint64_t entityPtr;
        void*    resourceTypeComp;
        int      value;
        int      _pad;
    };

    ProdEntry* entries = nullptr;
    int entryCount = 0;
    int entryCap = 0;

    void* bstRoot = *(void**)(s + 0x138);
    if (bstRoot) {
        // Start at leftmost node
        void* node = BST_Leftmost(bstRoot, 0x20);
        while (node) {
            uintptr_t n = (uintptr_t)node;
            uint64_t entityPtr = *(uint64_t*)n;

            // Find ResourceType component on entity
            void* resourceComp = TownHall_FindComponentByTypeId(entityPtr, *g_TypeId_RawMaterialDescription);

            // Read counter value at selected index
            int counterVal = *(int*)(n + counterOffset);

            // Push entry into dynamic array
            int newCount = entryCount + 1;
            if (newCount != entryCount) {
                if (entryCap < newCount) {
                    entryCap = newCount;
                    if (entryCount != 0) entryCap = newCount * 3;
                    ProdEntry* newBuf = (ProdEntry*)fn_MemAlloc(g_pMemAllocSingleton,
                        (uint64_t)entryCap * (uint64_t)sizeof(ProdEntry), 8);
                    int copyStart = 0;
                    if (entries == newBuf) copyStart = entryCount;
                    for (int i = copyStart; i < newCount; i++) {
                        ProdEntry* dst = &newBuf[i];
                        if (i < entryCount) {
                            *dst = entries[i];
                        } else {
                            dst->entityPtr = entityPtr;
                            dst->resourceTypeComp = resourceComp;
                            dst->value = counterVal;
                            dst->_pad = 0;
                        }
                    }
                    if (entries != newBuf) fn_MemFreeStatic(entries);
                    entries = newBuf;
                } else {
                    entries[entryCount].entityPtr = entityPtr;
                    entries[entryCount].resourceTypeComp = resourceComp;
                    entries[entryCount].value = counterVal;
                    entries[entryCount]._pad = 0;
                }
                entryCount = newCount;
            }

            node = InvBST_Next(node);
        }
    }

    // Sort entries
    if (sortComboIdx == 0) {
        if (entryCount > 1)
            fn_InventorySort_Asc(entries, entryCount, &sortComboIdx, nullptr);
    } else {
        if (entryCount > 1)
            fn_InventorySort_Desc(entries, entryCount, &sortComboIdx, nullptr);
    }

    // Update UI elements for each entry
    int displayIdx = 0;
    if (entryCount > 0) {
        int64_t elemOff = 0;
        for (int i = 0; i < entryCount; i++) {
            uint64_t entityPtr = entries[i].entityPtr;

            // Find ValueDescription component on entity
            void* valDesc = TownHall_FindComponentByTypeId(entityPtr, *g_TypeInfo_ValueDescription);
            if (!valDesc) {
                elemOff += 0x10;
                continue;
            }

            // Create sprite from ValueDescription+0x20 (spriteSheet) and +0x28 (spriteName)
            uint8_t spriteBuf[16];
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)valDesc + 0x20),
                          (void*)((uintptr_t)valDesc + 0x28));

            // Set sprite on label element
            void* labelElem = *(void**)(*(uintptr_t*)(s + 0x118) + elemOff);
            fn_InternalBasePtr_CheckPtr(&labelElem);
            fn_LabelElement_SetSprite(labelElem, spriteBuf);

            // Create UIString from ValueDescription+0x30 (stringTable) and +0x38 (stringName)
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)valDesc + 0x30),
                            (void*)((uintptr_t)valDesc + 0x38));

            // Set text on label element
            void* labelElem2 = *(void**)(*(uintptr_t*)(s + 0x118) + elemOff);
            fn_InternalBasePtr_CheckPtr(&labelElem2);
            fn_TextElement_SetText(labelElem2, uiStrBuf);

            // Set number value on the number element (+8 from label)
            uintptr_t tableBase = *(uintptr_t*)(s + 0x118);
            fn_InternalBasePtr_CheckPtr((void*)(tableBase + elemOff + 8));
            fn_NumberElement_SetValueInt(*(void**)(tableBase + elemOff + 8), entries[i].value);

            // Set active flag (vtable +0x50 with -1)
            tableBase = *(uintptr_t*)(s + 0x118);
            fn_InternalBasePtr_CheckPtr((void*)(tableBase + elemOff + 8));
            void* numElem = *(void**)(tableBase + elemOff + 8);
            Element_VCall50(numElem, -1);

            elemOff += 0x10;
        }
        displayIdx = entryCount;
    }

    // Clear remaining entries
    if (displayIdx < (int)self->invTableCount) {
        int64_t elemOff = (int64_t)displayIdx << 4;
        for (int i = displayIdx; i < (int)self->invTableCount; i++) {
            // Empty sprite
            uint8_t emptySprite[16];
            fn_Sprite_Default_Ctor(emptySprite);
            void* labelElem = *(void**)(*(uintptr_t*)(s + 0x118) + elemOff);
            fn_InternalBasePtr_CheckPtr(&labelElem);
            fn_LabelElement_SetSprite(labelElem, emptySprite);

            // Empty string
            uint8_t emptyStr[16];
            fn_UIString_Ctor(emptyStr, 0, nullptr);
            void* labelElem2 = *(void**)(*(uintptr_t*)(s + 0x118) + elemOff);
            fn_InternalBasePtr_CheckPtr(&labelElem2);
            fn_TextElement_SetText(labelElem2, emptyStr);

            // Set value to 0
            uintptr_t tableBase = *(uintptr_t*)(s + 0x118);
            fn_InternalBasePtr_CheckPtr((void*)(tableBase + elemOff + 8));
            fn_NumberElement_SetValueInt(*(void**)(tableBase + elemOff + 8), 0);

            // Set inactive flag (vtable +0x50 with 0)
            tableBase = *(uintptr_t*)(s + 0x118);
            fn_InternalBasePtr_CheckPtr((void*)(tableBase + elemOff + 8));
            void* numElem = *(void**)(tableBase + elemOff + 8);
            Element_VCall50(numElem, 0);

            elemOff += 0x10;
        }
    }

    fn_MemFreeStatic(entries);
}

// ============================================================
// TownHallUI::UpdateAgriculture (was FUN_14007c220)
// ============================================================
// Builds temp BST from entity list, traverses agriBST (+0x1D8)
// in-order, updates agriculture label elements with sprites
// and text from SelectDescription components.
void TownHallUI_UpdateAgriculture(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);

    // Build temporary BST from entity list at gameData+0x120/+0x128
    // agriBST nodes: key=entityPtr, left=+0x10, right=+0x18, parent=+0x08
    void* tempBSTRoot = nullptr;
    int tempBSTCount = 0;
    int entityCount = *(int*)(gameData + 0x128);

    for (int i = 0; i < entityCount; i++) {
        uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(gameData + 0x120) + (uintptr_t)i * 8);

        if (!tempBSTRoot) {
            // First node — allocate root
            void* node = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
            if (node) {
                *(uint64_t*)node = entityPtr;
                *(uint64_t*)((uintptr_t)node + 0x08) = 0; // parent
                *(uint64_t*)((uintptr_t)node + 0x10) = 0; // left
                *(uint64_t*)((uintptr_t)node + 0x18) = 0; // right
                *(uint32_t*)((uintptr_t)node + 0x20) = 0; // balance
                tempBSTRoot = node;
            }
        } else {
            // Insert into existing BST
            void* current = tempBSTRoot;
            void* parent = nullptr;
            while (current) {
                uint64_t curKey = *(uint64_t*)current;
                if (entityPtr == curKey) goto next_entity; // already exists
                parent = current;
                if (entityPtr < curKey)
                    current = *(void**)((uintptr_t)current + 0x10);
                else
                    current = *(void**)((uintptr_t)current + 0x18);
            }
            // Allocate new node
            void* node = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
            if (node) {
                *(uint64_t*)node = entityPtr;
                *(uint64_t*)((uintptr_t)node + 0x08) = (uint64_t)parent;
                *(uint64_t*)((uintptr_t)node + 0x10) = 0;
                *(uint64_t*)((uintptr_t)node + 0x18) = 0;
                *(uint32_t*)((uintptr_t)node + 0x20) = 0;
                if (entityPtr < *(uint64_t*)parent)
                    *(void**)((uintptr_t)parent + 0x10) = node;
                else
                    *(void**)((uintptr_t)parent + 0x18) = node;
                // Rebalance
                uint64_t newNodeAddr = (uint64_t)node;
                fn_BST_Rebalance_0x28(&tempBSTRoot, (void*)&newNodeAddr);
            }
        }
        tempBSTCount++;
next_entity: ;
    }

    // Traverse agriBST (+0x1D8) in-order
    void* agriBSTRoot = *(void**)(s + 0x1D8);
    int displayIdx = 0;

    if (agriBSTRoot) {
        // Go to leftmost node (children at +0x10/+0x18)
        void* node = BST_Leftmost(agriBSTRoot, 0x10);

        while (node) {
            uint64_t entityPtr = *(uint64_t*)node;

            // Look up entity in temp BST
            void* found = nullptr;
            void* searchNode = tempBSTRoot;
            while (searchNode) {
                uint64_t sk = *(uint64_t*)searchNode;
                if (entityPtr < sk)
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                else if (entityPtr > sk)
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                else {
                    found = searchNode;
                    break;
                }
            }

            if (found) {
                // Find SelectDescription component (DAT_1401c48a8) on entity
                void* selectDesc = TownHall_FindComponentByTypeId(entityPtr, *g_TypeId_SelectDescription);


                // Original always sets sprite/text when found in BST (even if selectDesc is NULL)
                // Create sprite from SelectDescription+0x20/+0x28
                uint8_t spriteBuf[16];
                fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)selectDesc + 0x20),
                              (void*)((uintptr_t)selectDesc + 0x28));

                uintptr_t labelPtr = *(uintptr_t*)(s + 0x1C8) + (uintptr_t)displayIdx * 8;
                fn_InternalBasePtr_CheckPtr((void*)labelPtr);
                fn_LabelElement_SetSprite(*(void**)labelPtr, spriteBuf);

                // Create UIString from SelectDescription+0x30/+0x38
                uint8_t uiStrBuf[16];
                fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)selectDesc + 0x30),
                                (void*)((uintptr_t)selectDesc + 0x38));

                labelPtr = *(uintptr_t*)(s + 0x1C8) + (uintptr_t)displayIdx * 8;
                fn_InternalBasePtr_CheckPtr((void*)labelPtr);
                fn_TextElement_SetText(*(void**)labelPtr, uiStrBuf);

                // Vtable +0x50 call (SetColor) — 0xFFC0C0C0 = opaque silver/gray
                labelPtr = *(uintptr_t*)(s + 0x1C8) + (uintptr_t)displayIdx * 8;
                fn_InternalBasePtr_CheckPtr((void*)labelPtr);
                Element_VCall50(*(void**)labelPtr, (int)0xFFC0C0C0);
            } else {
            }

            // Show element (SetFlagsOnOff hide=0, show=1)
            uintptr_t labelPtr = *(uintptr_t*)(s + 0x1C8) + (uintptr_t)displayIdx * 8;
            fn_InternalBasePtr_CheckPtr((void*)labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 0, 1);

            displayIdx++;

            // Move to next node in-order (children at +0x10/+0x18, parent at +0x08)
            node = AgriBST_Next(node);
        }
    }

    // Hide remaining label elements
    if (displayIdx < *(int*)(s + 0x1D0)) {
        for (int i = displayIdx; i < *(int*)(s + 0x1D0); i++) {
            uintptr_t labelPtr = *(uintptr_t*)(s + 0x1C8) + (uintptr_t)i * 8;
            fn_InternalBasePtr_CheckPtr((void*)labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 1, 0);
        }
    }

    // Free temporary BST
    if (tempBSTRoot) {
        fn_BST_FreeNodes(&tempBSTRoot, *(void**)((uintptr_t)tempBSTRoot + 0x10));
        fn_BST_FreeNodes(&tempBSTRoot, *(void**)((uintptr_t)tempBSTRoot + 0x18));
        fn_MemFreeStatic(tempBSTRoot);
    }
}

// ============================================================
// TownHallUI::UpdateInventory (was FUN_14007def0)
// ============================================================
// For each profession, zeros counters, resizes entity arrays,
// iterates entities to count workers per profession, finds
// WorkplaceComponent/StorageLocationComponent to calculate
// max worker slots, updates NumberEditElement values.
void TownHallUI_UpdateInventory(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    uintptr_t profListBase = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0);
    int profCount = *(int*)(profListBase + 0x28);

    // Phase 1: Zero counters and resize entity arrays for each profession
    for (int p = 0; p < profCount; p++) {
        uintptr_t profEntry = *(uintptr_t*)(s + 0x40) + (uintptr_t)p * 0x40;
        *(int*)(profEntry + 0x00) = 0;  // worker count = 0
        *(int*)(profEntry + 0x04) = 0;  // secondary count = 0
        // Resize entity ptr array at +0x30 to 0 elements
        fn_DynArray_Resize((void*)(profEntry + 0x30), 0);
    }

    // Phase 2: For each profession, iterate entity BST at gameData+0x100
    // to find entities matching profession type, count worker slots
    for (int p = 0; p < profCount; p++) {
        int64_t profEntryOff = (int64_t)p * 0x40;
        uint64_t profTypeId = *(uint64_t*)(*(uintptr_t*)(profListBase + 0x20) + (uintptr_t)p * 8);

        // Look up profession type in entity BST at gameData+0x100
        void* bstNode = *(void**)(gameData + 0x100);
        while (bstNode) {
            uint64_t nodeKey = *(uint64_t*)bstNode;
            if (profTypeId < nodeKey)
                bstNode = *(void**)((uintptr_t)bstNode + 0x20);
            else if (profTypeId > nodeKey)
                bstNode = *(void**)((uintptr_t)bstNode + 0x28);
            else
                break;
        }

        int slotCount = 0;
        if (bstNode) {
            slotCount = (int)*(uint64_t*)((uintptr_t)bstNode + 0x10);
        }

        // For each slot in the profession BST node
        for (int slot = 0; slot < slotCount; slot++) {
            uintptr_t entityPtr = *(uintptr_t*)(*(uintptr_t*)((uintptr_t)bstNode + 0x08) +
                                                (uintptr_t)slot * 8);

            // Push entity into profession entry's entity array
            uintptr_t profEntry = *(uintptr_t*)(s + 0x40);
            int curCount = *(int*)(profEntry + 0x38 + profEntryOff);
            int newCount = curCount + 1;
            if (newCount != curCount) {
                void* oldArr = *(void**)(profEntry + 0x30 + profEntryOff);
                *(int*)(profEntry + 0x38 + profEntryOff) = newCount;
                if (*(int*)(profEntry + 0x3c + profEntryOff) < newCount) {
                    int newCap = newCount;
                    if (newCount == curCount + 1 && curCount != 0)
                        newCap = newCount * 3;
                    *(int*)(profEntry + 0x3c + profEntryOff) = newCap;
                    void* newArr = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap << 3, 8);
                    *(void**)(profEntry + 0x30 + profEntryOff) = newArr;
                }
                int copyStart = 0;
                if (oldArr == *(void**)(profEntry + 0x30 + profEntryOff))
                    copyStart = curCount;
                void* dstArr = *(void**)(profEntry + 0x30 + profEntryOff);
                for (int i = copyStart; i < newCount; i++) {
                    uint64_t* dst = (uint64_t*)((uintptr_t)dstArr + (uintptr_t)i * 8);
                    *dst = 0;
                    if (i < curCount)
                        *dst = *(uint64_t*)((uintptr_t)oldArr + (uintptr_t)i * 8);
                    else
                        *dst = entityPtr;
                }
                if (oldArr != *(void**)(profEntry + 0x30 + profEntryOff))
                    fn_MemFreeStatic(oldArr);
            }

            // Check entity for WorkplaceComponent -> active flag + worker count
            fn_InternalBasePtr_CheckPtr((void*)&entityPtr);
            uintptr_t entity = entityPtr;

            // Find WorkplaceComponent (DAT_1401c80e0) — ComponentEntity layout
            void* workplaceComp = TownHall_FindComponentByTypeId_CE(entity, *g_TypeId_WorkplaceComponent);

            uint64_t wpBP = 0;
            fn_InternalBasePtr_Ctor(&wpBP, workplaceComp);
            fn_InternalBasePtr_CheckPtr(&wpBP);

            // Check if entity is active (workplace+0x50 != 0)
            if (*(uint8_t*)(wpBP + 0x50) != 0) {
                uintptr_t profEntryBase = *(uintptr_t*)(s + 0x40);
                fn_InternalBasePtr_CheckPtr((void*)&entityPtr);
                uintptr_t entity2 = entityPtr;

                // Find WorkplaceComponent again to get StorageLocationComponent
                void* workplaceComp2 = TownHall_FindComponentByTypeId_CE(entity2, *g_TypeId_WorkplaceComponent);

                uint64_t wpBP2 = 0;
                fn_InternalBasePtr_Ctor(&wpBP2, workplaceComp2);
                fn_InternalBasePtr_CheckPtr(&wpBP2);

                // Get parent entity (wpBP2+0x10)
                fn_InternalBasePtr_CheckPtr((void*)(wpBP2 + 0x10));
                uintptr_t parentEntity = *(uintptr_t*)(wpBP2 + 0x10);

                // Find StorageLocationComponent (DAT_1401c8070) — ComponentEntity layout
                void* storageLocComp = TownHall_FindComponentByTypeId_CE(parentEntity, *g_TypeId_WorkComponent);

                uint64_t slBP = 0;
                fn_InternalBasePtr_Ctor(&slBP, storageLocComp);
                fn_InternalBasePtr_CheckPtr(&slBP);

                // Get max count
                int maxCount = *(int*)(slBP + 0x30);
                if (maxCount == 0) {
                    maxCount = fn_StorageCountQuery((void*)slBP);
                    *(int*)(slBP + 0x30) = maxCount;
                }

                int* counterPtr = (int*)(profEntryBase + 4 + profEntryOff);
                *counterPtr += maxCount;
            }
        }

        // Update number+count display
        uintptr_t profEntry = *(uintptr_t*)(s + 0x40) + profEntryOff;
        fn_InternalBasePtr_CheckPtr((void*)(profEntry + 0x18));
        fn_NumberElement_SetValueInt(*(void**)(profEntry + 0x18), *(int*)(profEntry + 0x04));

        int entityArrayCount = *(int*)(profEntry + 0x38);
        fn_InternalBasePtr_CheckPtr((void*)(profEntry + 0x20));
        fn_NumberElement_SetValueInt(*(void**)(profEntry + 0x20), entityArrayCount);

        // Set visibility via vtable+0x40
        fn_InternalBasePtr_CheckPtr((void*)(profEntry + 0x28));
        void* visElem = *(void**)(profEntry + 0x28);
        Element_VCall40(visElem, entityArrayCount > 0 ? 1 : 0);
    }

    // Phase 3: Count adult citizens per profession
    int citizenCount = *(int*)(gameData + 0xa8);
    for (int c = 0; c < citizenCount; c++) {
        uintptr_t citizen = *(uintptr_t*)(*(uintptr_t*)(gameData + 0xa0) + (uintptr_t)c * 8);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);

        float age = *(float*)(citizen + 0x3c);
        float adultAge = *(float*)(*(uintptr_t*)(citizen + 0x288) + 0x44);
        uintptr_t school = *(uintptr_t*)(citizen + 0x208);

        if (age >= adultAge && school == 0) {
            // Adult citizen — look up profession in profBST (+0x50)
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            uint64_t profId = *(uint64_t*)(citizen + 0x1f0);
            void* profNode = *(void**)(s + 0x50);
            while (profNode) {
                uint64_t nodeKey = *(uint64_t*)profNode;
                if (profId < nodeKey)
                    profNode = *(void**)((uintptr_t)profNode + 0x18);
                else if (profId > nodeKey)
                    profNode = *(void**)((uintptr_t)profNode + 0x20);
                else
                    break;
            }
            if (profNode) {
                int profIdx = (int)*(uint64_t*)((uintptr_t)profNode + 0x08);
                int* counterPtr = (int*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)profIdx * 0x40);
                *counterPtr += 1;
            }
        }
    }

    // Phase 4: Update spinner elements with final counts
    // First profession entry
    int* firstEntry = *(int**)(s + 0x40);
    fn_InternalBasePtr_CheckPtr((void*)(firstEntry + 4));
    fn_NumberEditElement_SetValue(*(void**)(firstEntry + 4), *firstEntry);
    firstEntry = *(int**)(s + 0x40);
    fn_InternalBasePtr_CheckPtr((void*)(firstEntry + 4));
    fn_NumberEditElement_SetMinMax(*(void**)(firstEntry + 4), *firstEntry, *firstEntry);

    // Remaining profession entries
    int totalProfs = *(int*)(s + 0x48);
    for (int p = 1; p < totalProfs; p++) {
        uintptr_t profEntry = *(uintptr_t*)(s + 0x40) + (uintptr_t)p * 0x40;
        fn_InternalBasePtr_CheckPtr((void*)(profEntry + 0x10));
        fn_NumberEditElement_SetValue(*(void**)(profEntry + 0x10), *(int*)(profEntry));

        int* firstEntryPtr = *(int**)(s + 0x40);
        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)firstEntryPtr + (uintptr_t)p * 0x40 + 0x10));
        fn_NumberEditElement_SetMinMax(
            *(void**)((uintptr_t)firstEntryPtr + (uintptr_t)p * 0x40 + 0x10),
            0, *firstEntryPtr + *(int*)((uintptr_t)firstEntryPtr + (uintptr_t)p * 0x40));
    }
}

// ============================================================
// TownHallUI::UpdateAll (was FUN_14007d590)
// ============================================================
// Collects entities from 4 entity lists in game data, zeros
// and populates invBST counters, then updates inventory and
// agriculture table UI based on component types.
void TownHallUI_UpdateAll(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    uintptr_t* gdPtr = (uintptr_t*)gameData;

    // Phase 1: For each tracked entity (gameData+0x110/+0x118),
    // zero or insert entry in invBST
    int trackedCount = *(int*)(gameData + 0x118);
    for (int i = 0; i < trackedCount; i++) {
        uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(gameData + 0x110) + (uintptr_t)i * 8);

        // Look up in invBST
        void* bstNode = InvBST_Find(*(void**)(s + 0x138), entityPtr);
        if (bstNode) {
            // Zero the counters
            *(uint64_t*)((uintptr_t)bstNode + 0x08) = 0;
            *(uint32_t*)((uintptr_t)bstNode + 0x10) = 0;
        } else {
            // Insert new node with zeroed counters
            struct { uint64_t key; uint64_t val1; uint32_t val2; } entry;
            entry.key = entityPtr;
            entry.val1 = 0;
            entry.val2 = 0;
            uint64_t retNode = 0;
            fn_BST_Insert_0x14((void*)(s + 0x138), &retNode, &entry);
        }
    }

    // Phase 2: Collect all entities from 4 entity lists
    // Each list is at gameData[i*2] = array ptr, gameData[i*2+1] = count
    void* entityBuf = nullptr;
    int entityCount = 0;
    int entityCap = 0;

    for (int listIdx = 0; listIdx < 4; listIdx++) {
        uintptr_t arrayPtr = gdPtr[listIdx * 2];
        int listCount = (int)gdPtr[listIdx * 2 + 1];

        for (int j = 0; j < listCount; j++) {
            uint64_t entityPtr = *(uint64_t*)(arrayPtr + (uintptr_t)j * 8);
            int newCount = entityCount + 1;
            if (newCount != entityCount) {
                if (entityCap < newCount) {
                    int newCap = newCount;
                    if (entityCount != 0) newCap = newCount * 3;
                    entityCap = newCap;
                    void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap << 3, 8);
                    int copyStart = 0;
                    if (entityBuf == newBuf) copyStart = entityCount;
                    for (int k = copyStart; k < newCount; k++) {
                        uint64_t* dst = (uint64_t*)((uintptr_t)newBuf + (uintptr_t)k * 8);
                        *dst = 0;
                        if (k < entityCount)
                            *dst = *(uint64_t*)((uintptr_t)entityBuf + (uintptr_t)k * 8);
                        else
                            *dst = entityPtr;
                    }
                    if (entityBuf != newBuf) fn_MemFreeStatic(entityBuf);
                    entityBuf = newBuf;
                } else {
                    *(uint64_t*)((uintptr_t)entityBuf + (uintptr_t)entityCount * 8) = entityPtr;
                }
                entityCount = newCount;
            }
        }
    }

    // Phase 3: For each collected entity, find components and update BST counters
    uint64_t* entities = (uint64_t*)entityBuf;
    for (int i = 0; i < entityCount; i++) {
        uint64_t entityPtr = entities[i];

        // Find StorageComponent_FC (DAT_1401c7e78) — ComponentEntity layout
        fn_InternalBasePtr_CheckPtr((void*)&entityPtr);
        void* storageComp = TownHall_FindComponentByTypeId_CE(entityPtr, *g_TypeId_StorageComponent_FC);
        uint64_t storageBP = 0;
        fn_InternalBasePtr_Ctor(&storageBP, storageComp);

        // Find StateMachineComponent (DAT_1401c7d98) — ComponentEntity layout
        fn_InternalBasePtr_CheckPtr((void*)&entityPtr);
        void* buildStateComp = TownHall_FindComponentByTypeId_CE(entityPtr, *g_TypeId_StateMachineComponent_FC);
        uint64_t buildStateBP = 0;
        fn_InternalBasePtr_Ctor(&buildStateBP, buildStateComp);

        int counterIndex; // which of the 3 counters to increment (offset = counterIndex*4 + 8)
        if (buildStateBP != 0) {
            counterIndex = 2; // under construction
        } else {
            // Check for TradeComponent (DAT_1401c4b80) — ComponentEntity layout
            fn_InternalBasePtr_CheckPtr((void*)&entityPtr);
            void* buildingComp = TownHall_FindComponentByTypeId_CE(entityPtr, *g_TypeId_TradeComponent_FC);
            uint64_t buildingBP = 0;
            fn_InternalBasePtr_Ctor(&buildingBP, buildingComp);
            if (buildingBP != 0)
                counterIndex = 1; // has building component
            else
                counterIndex = 0; // basic entity
        }

        // Iterate StorageComponent resource entries
        fn_InternalBasePtr_CheckPtr(&storageBP);
        if (storageBP == 0) continue;
        int resourceCount = (int)*(uint64_t*)(storageBP + 0x30);

        for (int r = 0; r < resourceCount; r++) {
            fn_InternalBasePtr_CheckPtr(&storageBP);
            uint64_t resourceEntity = *(uint64_t*)(*(uintptr_t*)(storageBP + 0x28) + (uintptr_t)r * 0x10);
            int resourceQty = *(int*)(*(uintptr_t*)(storageBP + 0x28) + (uintptr_t)r * 0x10 + 8);

            // Look up resource entity in invBST
            void* bstNode = InvBST_Find(*(void**)(s + 0x138), resourceEntity);
            if (bstNode) {
                // Increment the appropriate counter
                int* counterPtr = (int*)((uintptr_t)bstNode + counterIndex * 4 + 8);
                *counterPtr += resourceQty;
            }
        }
    }

    fn_MemFreeStatic(entityBuf);
}

// ============================================================
// TownHallUI::OnProfessionButton (was FUN_14007dca0)
// ============================================================
void TownHallUI_OnProfessionButton(TownHallUI* self, int index) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t entryBase = (uintptr_t)index * 0x40 + self->profEntryArray;

    int entityCount = *(int*)(entryBase + 0x38);
    if (entityCount == 0) return;

    self->profButtonIdx++;
    int cycleIdx = (int)self->profButtonIdx;
    if (cycleIdx >= entityCount) cycleIdx = 0;
    self->profButtonIdx = (uint32_t)cycleIdx;

    uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(entryBase + 0x30) + (uintptr_t)cycleIdx * 8);
    uint64_t dialogInst = *(uint64_t*)(s + 0x18);
    uintptr_t entityController = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0x120);
    Toolbar_CenterOnEntity_Impl((void*)entityController, &dialogInst, &entityPtr);
}

// ============================================================
// TownHallUI::OnProfessionSpinner (was FUN_14007dd10)
// ============================================================
void TownHallUI_OnProfessionSpinner(TownHallUI* self, int index) {
    // NO g_wpcRosterOnly guard (removed 2026-08-17, together with the two sibling
    // spinners this comment used to point at). The fault it named — READ of 0x10 in
    // Detach's e710 getter on a LOWER, which this handler does via
    // fn_CitizenList_RemoveWorker — is now guarded at the source
    // (workplacecomponent.cpp:276 returns early on a null accessor). "Our objects can't
    // survive it" stopped being true when that guard landed. Meanwhile the flag also
    // gates Attach's clear of the "no workers" status bit, so holding it left the ? icon
    // up over every building staffed from this tab. Measured on the lower path too: a
    // seat -> unseat -> re-seat cycle drives Detach's full visual tail with no fault.
    uintptr_t s = (uintptr_t)self;
    uintptr_t entryBase = *(uintptr_t*)(s + 0x40);
    int64_t entryOff = (int64_t)index * 0x40;

    // Get desired vs current count
    fn_InternalBasePtr_CheckPtr((void*)(entryBase + entryOff + 0x10));
    int desired = fn_NumberEditElement_GetValue(*(void**)(entryBase + entryOff + 0x10));
    int current = *(int*)(entryBase + entryOff);

    if (current < desired) {
        int toAdd = desired - current;
        for (int i = 0; i < toAdd; i++) {
            void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingleton + 0xf0);
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0) + 0x20 + (uintptr_t)index * 8);

            uint64_t retBuf[2] = {0, 0};
            fn_CitizenList_RequestWorker(citizenList, retBuf, profId);
            uint64_t slot = retBuf[0];

            // Try idle -> working -> student -> any
            uint64_t slotCopy = slot;
            uint8_t findBuf[8] = {0};
            void* result = fn_CitizenList_FindIdle(citizenList, findBuf, &slotCopy, profId);
            uint64_t found = *(uint64_t*)result;

            if (found == 0) {
                if (slot != 0) {
                    slotCopy = slot;
                    uint8_t fb2[8] = {0};
                    result = fn_CitizenList_FindWorking(citizenList, fb2, (void*)&slotCopy, profId);
                    found = *(uint64_t*)result;
                    if (found != 0) continue;
                }
                slotCopy = slot;
                uint8_t fb3[8] = {0};
                result = fn_CitizenList_FindStudent(citizenList, fb3, (void*)&slotCopy, profId);
                found = *(uint64_t*)result;

                if (found == 0) {
                    slotCopy = slot;
                    uint64_t anyBuf = 0;
                    fn_CitizenList_FindAny(citizenList, &anyBuf, (void*)&slotCopy, profId);
                }
            }
        }
    }

    if (desired < current) {
        int toRemove = current - desired;
        for (int i = 0; i < toRemove; i++) {
            void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingleton + 0xf0);
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0) + 0x20 + (uintptr_t)index * 8);
            fn_CitizenList_RemoveWorker(citizenList, profId);
        }
    }

    // Refresh display
    TownHallUI_UpdateInventory(self);
}

// ============================================================
// TownHallUI::Serialize (vtable +0x20, 0x140081000)
// ============================================================
void TownHallUI_Serialize(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, "tabs");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* element = *(void**)foundBuf;

    void* tabElement = nullptr;
    if (element && g_TypeId_TabElement) {
        if (fn_IsDerivedFrom(element, *g_TypeId_TabElement))
            tabElement = element;
    }

    uint64_t tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, tabElement);
    *(uint64_t*)(s + 0x30) = tempIBP;
    fn_String_Dtor(strBuf);

    TownHallUI_InitGeneral(self);
    TownHallUI_InitProfessions(self);
    TownHallUI_InitProduction(self);
    TownHallUI_InitInventory(self);
    TownHallUI_InitGraph(self);
    TownHallUI_InitAgriculture(self);
}

// ============================================================
// TownHallUI::Update (vtable +0x50, 0x140080db0)
// ============================================================
void TownHallUI_Update(TownHallUI* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    int tabIndex = *(int*)(*(uintptr_t*)(s + 0x30) + 0xf8);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int combo1 = *(int*)(*(uintptr_t*)(s + 0x100) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
    int combo2 = *(int*)(*(uintptr_t*)(s + 0x128) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
    int combo3 = *(int*)(*(uintptr_t*)(s + 0x130) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
    int combo4 = *(int*)(*(uintptr_t*)(s + 0x168) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
    int combo5 = *(int*)(*(uintptr_t*)(s + 0x170) + 0x118);

    uint64_t serializer[3] = {0, 0, 0};
    g_fn_Serializer_Ctor(serializer, stream, 1);

    g_fn_Serialize_Int(serializer, &tabIndex, nullptr);
    g_fn_Serialize_Int(serializer, &combo1, nullptr);
    g_fn_Serialize_Int(serializer, &combo2, nullptr);

    int version = g_fn_Serializer_GetVersion(serializer);
    if (version > 0) {
        g_fn_Serialize_Int(serializer, &combo3, nullptr);
    }
    g_fn_Serialize_Int(serializer, &combo4, nullptr);
    g_fn_Serialize_Int(serializer, &combo5, nullptr);

    if (*(int*)(serializer[0] + 8) == 0) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
        fn_TabElement_SetActiveTab(*(void**)(s + 0x30), tabIndex);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x100), combo1);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x128), combo2);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x130), combo3);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x168), combo4);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x170), combo5);
        TownHallUI_UpdateGraph(self);
    }

    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TownHallUI::Startup (vtable +0x38)
// ============================================================
void TownHallUI_Startup(TownHallUI* self) {
    TownHallUI_UpdateGeneral(self);
    TownHallUI_UpdateProfessions(self);
    TownHallUI_UpdateProduction(self);
    TownHallUI_UpdateAgriculture(self);
}

// ============================================================
// TownHallUI::OnCreate (vtable +0x40)
// ============================================================
void TownHallUI_OnCreate(TownHallUI* self) {
    uintptr_t gameSingleton = *(uintptr_t*)g_pGameSingleton;
    int currentTick = *(int*)(*(uintptr_t*)(*(uintptr_t*)(gameSingleton + 0xf0) + 0x150) + 8);

    if (self->cachedGameTick != currentTick) {
        self->cachedGameTick = currentTick;
        TownHallUI_UpdateGraph(self);
    }

    TownHallUI_UpdateInventory(self);
    TownHallUI_UpdateAll(self);
}

// ============================================================
// TownHallUI::OnRemove (vtable +0x48)
// ============================================================
void TownHallUI_OnRemove(TownHallUI* self, void* param2, int msg, int id) {
    uintptr_t gameSingleton = *(uintptr_t*)g_pGameSingleton;
    int profCount = *(int*)(*(uintptr_t*)(gameSingleton + 0xb0) + 0x28);

    if (msg == 0x12) {
        if (id > 0x1998 && id <= 0x1999 + profCount)
            TownHallUI_OnProfessionButton(self, id - 0x1999);
    } else if (msg == 0x14) {
        if (id == 0x1964 || id == 0x1965)
            TownHallUI_UpdateGraph(self);
    } else if (msg == 0x15) {
        if (id > 0x1965 && id <= 0x1966 + profCount)
            TownHallUI_OnProfessionSpinner(self, id - 0x1966);
    }
}

// ============================================================
// STANDALONE promotion — real TownHallUI controller vtable
// ============================================================
// The town-hall panel (Dialog/TownHall.rsc — the General statistics / Professions /
// Production / Inventory / Graph / Agriculture tab dialog opened by clicking a Town
// Hall) is driven by a TownHallUI controller. In standalone this type was a StubNop
// (all slots no-op, see ui_dialog_stub_factories.cpp — that stub registration is
// REMOVED there to avoid a duplicate REGISTER_TYPE), so the dialog opened with every
// field left on its .rsc template default: the "general statistics" numbers never
// populated. This installs a real per-class vtable wiring the fully-reimplemented
// methods above, following the StorageUI/WorkPlaceUI promotion pattern.
//
// Vtable offsets (verified from the exe TownHallUI vtable @ 0x14015dce0, see
// engine/reimpl/townhallui.cpp):
//   +0x00 (idx0)  ScalarDtor — frees the prof/prod/inv/agri arrays + 3 BSTs
//   +0x10 (idx2)  GetTypeId
//   +0x20 (idx4)  Serialize  = element discovery + InitGeneral/Professions/
//                 Production/Inventory/Graph/Agriculture (framework setup call)
//   +0x38 (idx7)  Startup    = UpdateGeneral/Professions/Production/Agriculture
//   +0x40 (idx8)  OnCreate   = per-frame: graph refresh on tick change +
//                 UpdateInventory + UpdateAll (the framework dispatches ONLY +0x40
//                 each frame, so the self-heal build + a throttled Startup refresh
//                 are driven here — keeps the General tab's month/counts live)
//   +0x48 (idx9)  OnEvent    = profession buttons 0x1999+, graph combos
//                 0x1964/0x1965 (msg 0x14), profession spinners 0x1966+ (msg 0x15)
//   +0x50 (idx10) Update     = tab/combo state persist (IO::Serializer stream)
//
// The exe helper functions these methods call through fn_* pointers are ported in
// townhall_helpers.cpp + citizen_employ_cascade.cpp and registered by
// standalone/reg_townhallui.cpp, so EngineFuncsInit resolves them in SA.
#include "Object/factory_registration.h"
#include <cstddef>
#include "dialog_controller_vtable.h"

namespace townhallui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TownHallUI");
    return c;
}

// ---- exe-.rdata type-id globals the TownHallUI paths dereference ----
// All lazily filled from the live runtime type registry (valid after
// Object::Factory::Startup). Registered names Ghidra-verified this session via the
// .rdata name-table / RegisterType writers (the engine_funcs labels now match):
//   exref 0x1401516c8 "g_TypeId_SpinnerElement"      -> UI::NumberElement
//   DAT   0x1401c7d98 "g_TypeId_StateMachineComponent_FC" -> StateMachineComponent
//   DAT   0x1401c4b80 "g_TypeId_TradeComponent_FC"   -> TradeComponent
//   DAT   0x1401c7318 "g_TypeId_ComponentDescription_FC"      -> ComponentDescription
void EnsureGlobals() {
    struct Entry { uint32_t** global; const char* name; };
    static uint32_t storage[20] = {0};
    static const Entry entries[] = {
        { &g_TypeId_TabElement,               "TabElement" },
        { &g_TypeId_TableElement,             "TableElement" },
        { &g_TypeId_LabelElement,             "LabelElement" },
        { &g_TypeId_TextElement,              "TextElement" },
        { &g_TypeId_SpinnerElement,           "NumberElement" },
        { &g_TypeId_NumberEditElement,        "NumberEditElement" },
        { &g_TypeId_ComboElement,             "ComboElement" },
        { &g_TypeId_HelpButtonElem,           "ButtonElement" },
        { &g_TypeId_ImageElement,             "ImageElement" },
        { &g_TypeId_StringTable,              "StringTable" },
        { &g_TypeId_CitizenDescription,       "CitizenDescription" },
        { &g_TypeInfo_ValueDescription,       "ValueDescription" },
        { &g_TypeId_SelectDescription,        "SelectDescription" },
        { &g_TypeId_ComponentDescription_FC,           "ComponentDescription" },
        { &g_TypeId_StateMachineComponent_FC,      "StateMachineComponent" },
        { &g_TypeId_TradeComponent_FC,        "TradeComponent" },
        // belt-and-braces: normally filled via the economy type-id registry
        { &g_TypeId_RawMaterialDescription,   "RawMaterialDescription" },
        { &g_TypeId_StorageComponent_FC,      "StorageComponent" },
        { &g_TypeId_WorkplaceComponent,       "WorkPlaceComponent" },
        { &g_TypeId_WorkComponent, "WorkComponent" },
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        if (*entries[i].global) continue;
        if (!storage[i]) storage[i] = ::Object::ResolveTypeIdByNameLazy(entries[i].name);
        if (storage[i]) *entries[i].global = &storage[i];
    }
}

// Every global/function the Serialize+Startup+per-frame paths dereference
// unguarded must be live, or a null-deref faults (no SEH under MinGW).
bool Ready() {
    if (!(g_TypeId_TabElement && g_TypeId_TableElement && g_TypeId_LabelElement &&
          g_TypeId_TextElement && g_TypeId_SpinnerElement && g_TypeId_NumberEditElement &&
          g_TypeId_ComboElement && g_TypeId_HelpButtonElem && g_TypeId_ImageElement &&
          g_TypeId_StringTable && g_TypeId_CitizenDescription && g_TypeInfo_ValueDescription &&
          g_TypeId_SelectDescription && g_TypeId_ComponentDescription_FC && g_TypeId_StateMachineComponent_FC &&
          g_TypeId_TradeComponent_FC && g_TypeId_RawMaterialDescription &&
          g_TypeId_StorageComponent_FC && g_TypeId_WorkplaceComponent &&
          g_TypeId_WorkComponent))
        return false;
    if (!(fn_GroupRating_Init && fn_GroupRating_Update && fn_ProfArrayPush &&
          fn_ProdArrayPush && fn_BST_Rebalance_0x30 && fn_BST_Rebalance_0x28 &&
          fn_BST_Insert_0x14 && fn_BST_FreeNodes && fn_QueryProductionStat &&
          fn_InventorySort_Asc && fn_InventorySort_Desc && fn_DynArray_Resize &&
          fn_TableElement_SetSize && fn_TableElement_SetChild2 && fn_TableElement_SetChild3 &&
          fn_GraphElement_RemoveGraphs && fn_GraphElement_AddGraph &&
          fn_GraphElement_AddGraphPoint && fn_StringTable_GetCount && fn_Time_AsSeconds &&
          fn_NumberEditElement_SetValue && fn_NumberEditElement_SetMinMax))
        return false;
    if (!(g_pExternalFactorySingleton && *g_pExternalFactorySingleton)) return false;
    return true;
}

// The town-side data the update paths read unguarded: townMgr (gw+0xf0) with its
// citizen roster, the profession list (gw+0xb0), the game clock (gw+0x128), and
// the townMgr history/production record blocks (+0x150 / +0x170) that the graph
// and production tabs index into.
bool TownReady() {
    if (!(g_pGameSingleton && *g_pGameSingleton)) return false;
    uintptr_t gw = *(uintptr_t*)g_pGameSingleton;
    uintptr_t townMgr = *(uintptr_t*)(gw + 0xf0);
    uintptr_t profList = *(uintptr_t*)(gw + 0xb0);
    if (!townMgr || !profList) return false;
    if (*(int*)(profList + 0x28) <= 0 || *(uintptr_t*)(profList + 0x20) == 0) return false;
    if (*(uintptr_t*)(gw + 0x128) == 0) return false;
    if (*(uintptr_t*)(townMgr + 0x150) == 0) return false;   // stat-history series
    if (*(uintptr_t*)(townMgr + 0x170) == 0) return false;   // production records
    return true;
}

static void Trace(const char* what, void* self) {
    fprintf(stderr, "[TownHallUI] %s self=%p ready=%d town=%d built=%d\n",
            what, self, Ready() ? 1 : 0, TownReady() ? 1 : 0,
            self ? *(int*)((uintptr_t)self + 0x48) : -1);
    fflush(stderr);
}

// InitProfessions pushed at least one row once Serialize has run.
inline bool Built(void* self) { return *(int*)((uintptr_t)self + 0x48) != 0; }

// Controller bound to its element + dialog instance (framework sets +0x18/+0x20).
inline bool Bound(void* self) {
    uintptr_t s = (uintptr_t)self;
    return *(void**)(s + 0x18) != nullptr && *(void**)(s + 0x20) != nullptr;
}

// One-shot gate diagnostic — prints WHICH component of Ready()/TownReady() is
// missing (a silently-skipping gate looks identical to "blank dialog"). Logs a
// handful of lines once per session; also callable at world-build time via
// TownHallSA_Diag (gameupdateloop.cpp) so the dialog-independent gates are
// visible without opening the panel.
void DiagPrint(const char* where, void* self) {
    struct N { const char* n; void* p; };
    const N fns[] = {
        {"fn_GroupRating_Init",   (void*)fn_GroupRating_Init},
        {"fn_GroupRating_Update", (void*)fn_GroupRating_Update},
        {"fn_ProfArrayPush",      (void*)fn_ProfArrayPush},
        {"fn_ProdArrayPush",      (void*)fn_ProdArrayPush},
        {"fn_BST_Rebalance_0x30", (void*)fn_BST_Rebalance_0x30},
        {"fn_BST_Rebalance_0x28", (void*)fn_BST_Rebalance_0x28},
        {"fn_BST_Insert_0x14",    (void*)fn_BST_Insert_0x14},
        {"fn_BST_FreeNodes",      (void*)fn_BST_FreeNodes},
        {"fn_QueryProductionStat",(void*)fn_QueryProductionStat},
        {"fn_InventorySort_Asc",  (void*)fn_InventorySort_Asc},
        {"fn_InventorySort_Desc", (void*)fn_InventorySort_Desc},
        {"fn_DynArray_Resize",    (void*)fn_DynArray_Resize},
        {"fn_TableElement_SetSize",   (void*)fn_TableElement_SetSize},
        {"fn_TableElement_SetChild2", (void*)fn_TableElement_SetChild2},
        {"fn_TableElement_SetChild3", (void*)fn_TableElement_SetChild3},
        {"fn_GraphElement_RemoveGraphs", (void*)fn_GraphElement_RemoveGraphs},
        {"fn_GraphElement_AddGraph",     (void*)fn_GraphElement_AddGraph},
        {"fn_GraphElement_AddGraphPoint",(void*)fn_GraphElement_AddGraphPoint},
        {"fn_StringTable_GetCount", (void*)fn_StringTable_GetCount},
        {"fn_Time_AsSeconds",       (void*)fn_Time_AsSeconds},
        {"fn_NumberEditElement_SetValue",  (void*)fn_NumberEditElement_SetValue},
        {"fn_NumberEditElement_SetMinMax", (void*)fn_NumberEditElement_SetMinMax},
    };
    const N tids[] = {
        {"TabElement", g_TypeId_TabElement}, {"TableElement", g_TypeId_TableElement},
        {"LabelElement", g_TypeId_LabelElement}, {"TextElement", g_TypeId_TextElement},
        {"NumberElement", g_TypeId_SpinnerElement}, {"NumberEditElement", g_TypeId_NumberEditElement},
        {"ComboElement", g_TypeId_ComboElement}, {"ButtonElement", g_TypeId_HelpButtonElem},
        {"ImageElement", g_TypeId_ImageElement}, {"StringTable", g_TypeId_StringTable},
        {"CitizenDescription", g_TypeId_CitizenDescription},
        {"ValueDescription", g_TypeInfo_ValueDescription},
        {"SelectDescription", g_TypeId_SelectDescription},
        {"ComponentDescription", g_TypeId_ComponentDescription_FC},
        {"StateMachineComponent", g_TypeId_StateMachineComponent_FC},
        {"TradeComponent", g_TypeId_TradeComponent_FC},
        {"RawMaterialDescription", g_TypeId_RawMaterialDescription},
        {"StorageComponent", g_TypeId_StorageComponent_FC},
        {"WorkPlaceComponent", g_TypeId_WorkplaceComponent},
        {"WorkComponent", g_TypeId_WorkComponent},
    };
    fprintf(stderr, "[TownHallSA] %s: self=%p bound=%d built=%d ready=%d town=%d extFactory=%d\n",
            where, self,
            self ? (Bound(self) ? 1 : 0) : -1,
            self ? (Built(self) ? 1 : 0) : -1,
            Ready() ? 1 : 0, TownReady() ? 1 : 0,
            (g_pExternalFactorySingleton && *g_pExternalFactorySingleton) ? 1 : 0);
    for (const N& f : fns)
        if (!f.p) fprintf(stderr, "[TownHallSA]   MISSING fn: %s\n", f.n);
    for (const N& t : tids)
        if (!t.p) fprintf(stderr, "[TownHallSA]   MISSING typeId: %s\n", t.n);
    if (g_pGameSingleton && *g_pGameSingleton) {
        uintptr_t gw = *(uintptr_t*)g_pGameSingleton;
        uintptr_t townMgr = *(uintptr_t*)(gw + 0xf0);
        uintptr_t profList = *(uintptr_t*)(gw + 0xb0);
        fprintf(stderr, "[TownHallSA]   gw=%p townMgr=%p profList=%p profCnt=%d clock=%p hist(+150)=%p prod(+170)=%p\n",
                (void*)gw, (void*)townMgr, (void*)profList,
                profList ? *(int*)(profList + 0x28) : -1,
                (void*)*(uintptr_t*)(gw + 0x128),
                townMgr ? (void*)*(uintptr_t*)(townMgr + 0x150) : nullptr,
                townMgr ? (void*)*(uintptr_t*)(townMgr + 0x170) : nullptr);
        // UpdateGeneral's two data sources: the town citizen list (+0xa0/+0xa8,
        // CitizenComponent_OnAdd) and the residence list (+0x20/+0x28, helper_9de0).
        if (townMgr)
            fprintf(stderr, "[TownHallSA]   citizens(+0xa8)=%d homes(+0x28)=%d\n",
                    *(int*)(townMgr + 0xa8), *(int*)(townMgr + 0x28));
    } else {
        fprintf(stderr, "[TownHallSA]   g_pGameSingleton=%p (*=%p)\n",
                (void*)g_pGameSingleton,
                g_pGameSingleton ? *(void**)g_pGameSingleton : nullptr);
    }
    fflush(stderr);
}

// idx4 (+0x20): setup — element discovery + the six Init* passes.
void W_Serialize(void* self) {
    EnsureGlobals();
    if (!self || !Bound(self) || !Ready() || !TownReady()) { Trace("Serialize SKIP", self); return; }
    if (Built(self)) return;   // idempotence: the framework or self-heal already built
    Trace("Serialize", self);
    TownHallUI_Serialize((TownHallUI*)self);
}

// idx7 (+0x38): full content refresh (General/Professions/Production/Agriculture).
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !Bound(self) || !Ready() || !TownReady()) return;
    if (!Built(self)) TownHallUI_Serialize((TownHallUI*)self);
    if (!Built(self)) return;
    TownHallUI_Startup((TownHallUI*)self);
}

// idx8 (+0x40): per-frame. Self-heal the build, run the faithful per-frame
// OnCreate (graph tick check + UpdateInventory + UpdateAll), and refresh the
// slower Startup content (General numbers, hearts/stars, professions) twice a
// second so the statistics stay live while the panel is open.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    static bool s_diagOnce = false;
    if (!s_diagOnce) { s_diagOnce = true; DiagPrint("first OnUpdate", self); }
    if (!self || !Bound(self) || !Ready() || !TownReady()) return;
    if (!Built(self)) {
        TownHallUI_Serialize((TownHallUI*)self);
        if (!Built(self)) return;
        TownHallUI_Startup((TownHallUI*)self);
        return;
    }
    TownHallUI_OnCreate((TownHallUI*)self);
    uint32_t* throttle = (uint32_t*)((uintptr_t)self + 0x1E4);  // _pad1E4 (struct tail gap)
    if ((*throttle)++ % 30 == 0) TownHallUI_Startup((TownHallUI*)self);
}

// idx9 (+0x48): events — profession go-to buttons, graph combos, worker spinners.
void W_OnEvent(void* self, void* param2, int msg, int id) {
    EnsureGlobals();
    if (!self || !Built(self) || !Ready() || !TownReady()) return;
    TownHallUI_OnRemove((TownHallUI*)self, param2, msg, id);
}

// idx10 (+0x50): tab/combo state persist over an IO stream.
void W_Update(void* self, void* stream) {
    EnsureGlobals();
    if (!self || !stream || !Built(self) || !Ready() || !TownReady()) return;
    if (!(g_fn_Serializer_Ctor && g_fn_Serializer_Dtor && g_fn_Serialize_Int &&
          g_fn_Serializer_GetVersion))
        return;
    TownHallUI_Update((TownHallUI*)self, stream);
}

// ---- destructor: free the owned containers (mirrors exe 0x1400810d0 via
// engine/reimpl/townhallui.cpp::Reimpl_ScalarDtor) ----

void FreeBST_10_18(void* node) {
    if (!node) return;
    FreeBST_10_18(*(void**)((uintptr_t)node + 0x10));
    FreeBST_10_18(*(void**)((uintptr_t)node + 0x18));
    fn_MemFreeStatic(node);
}
void FreeBST_20_28(void* node) {
    if (!node) return;
    FreeBST_20_28(*(void**)((uintptr_t)node + 0x20));
    FreeBST_20_28(*(void**)((uintptr_t)node + 0x28));
    fn_MemFreeStatic(node);
}
void FreeBST_18_20(void* node) {
    if (!node) return;
    FreeBST_18_20(*(void**)((uintptr_t)node + 0x18));
    FreeBST_18_20(*(void**)((uintptr_t)node + 0x20));
    fn_MemFreeStatic(node);
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = g_vt;
    uintptr_t s = (uintptr_t)self;
    if (fn_MemFreeStatic) {
        FreeBST_10_18(*(void**)(s + 0x1D8));
        *(uint64_t*)(s + 0x1D8) = 0; *(uint32_t*)(s + 0x1E0) = 0;
        fn_MemFreeStatic(*(void**)(s + 0x1C8));
        *(uint64_t*)(s + 0x1C8) = 0; *(uint64_t*)(s + 0x1D0) = 0;
        FreeBST_20_28(*(void**)(s + 0x138));
        *(uint64_t*)(s + 0x138) = 0; *(uint32_t*)(s + 0x140) = 0;
        fn_MemFreeStatic(*(void**)(s + 0x118));
        *(uint64_t*)(s + 0x118) = 0; *(uint64_t*)(s + 0x120) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xF0));
        *(uint64_t*)(s + 0xF0) = 0; *(uint64_t*)(s + 0xF8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xE0));
        *(uint64_t*)(s + 0xE0) = 0; *(uint64_t*)(s + 0xE8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xD0));
        *(uint64_t*)(s + 0xD0) = 0; *(uint64_t*)(s + 0xD8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xC0));
        *(uint64_t*)(s + 0xC0) = 0; *(uint64_t*)(s + 0xC8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xB0));
        *(uint64_t*)(s + 0xB0) = 0; *(uint64_t*)(s + 0xB8) = 0;
        FreeBST_18_20(*(void**)(s + 0x50));
        *(uint64_t*)(s + 0x50) = 0; *(uint32_t*)(s + 0x58) = 0;
        int profRows = *(int*)(s + 0x48);
        uintptr_t profArr = *(uintptr_t*)(s + 0x40);
        for (int i = 0; i < profRows && profArr; i++) {
            uintptr_t rowList = profArr + (uintptr_t)i * 0x40 + 0x30;
            fn_MemFreeStatic(*(void**)rowList);
            *(uint64_t*)rowList = 0; *(uint64_t*)(rowList + 8) = 0;
        }
        fn_MemFreeStatic(*(void**)(s + 0x40));
        *(uint64_t*)(s + 0x40) = 0; *(uint64_t*)(s + 0x48) = 0;
        if (flags & 1) fn_MemFreeStatic(self);
    }
    return self;
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0]  = reinterpret_cast<void*>(&Dtor);
        g_vt[2]  = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4]  = reinterpret_cast<void*>(&W_Serialize);
        g_vt[7]  = reinterpret_cast<void*>(&W_Startup);
        g_vt[8]  = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9]  = reinterpret_cast<void*>(&W_OnEvent);
        g_vt[10] = reinterpret_cast<void*>(&W_Update);
    }
} g_init;

// Wave 10D lane VTFID: retail's TownHallUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "TownHallUI", &StubNop);

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x1E8); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x1E8), 8);
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

}  // namespace townhallui_sa

// World-build-time gate diagnostic (called once from GameUpdateLoop::Update at an
// early frame, see gameupdateloop.cpp) — reports the dialog-independent gates
// (fn pointers / type-ids / town data) without needing the panel opened.
// NOTE (2026-08-09): this function has TWO jobs, and only one of them is a
// diagnostic. `EnsureGlobals()` PUBLISHES nine shared element type-id globals
// (TabElement/TableElement/LabelElement/TextElement/NumberElement/NumberEditElement/
// ComboElement/ButtonElement/…) by name, and because this is called unconditionally
// from GameUpdateLoop::Update, that publish happens at frame 120 of EVERY session
// whether or not the town-hall panel is ever opened. Other dialogs read those same
// globals (toolbardialog / optionsdialog / saveloaddialog / spritecaptureui), so the
// publish decides which of THEIR code paths are live rather than dormant.
//
// That means the publish is now load-bearing and must NOT be put behind an env gate:
// gating it would silently return those other readers to their null-pointer no-op
// path. Only the REPORTING is a diagnostic, so only the reporting is gated — which
// also stops a diagnostic from writing to the log on every default run. If the
// frame-120 publish is ever to be removed, the readers have to be migrated to
// private by-name resolves first (see controlui.cpp / pagetoggleui.cpp for the
// idiom); it is not a standalone cleanup.
extern "C" void TownHallSA_Diag(int frameNum) {
    static bool s_done = false;
    if (s_done || frameNum < 120) return;
    s_done = true;
    townhallui_sa::EnsureGlobals();   // load-bearing publish — deliberately ungated
}

REGISTER_TYPE(TownHallUI, "UI::DialogController",
              &townhallui_sa::Factory, &townhallui_sa::InPlace);

