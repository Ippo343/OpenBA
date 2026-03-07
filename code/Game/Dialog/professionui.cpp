// code/Game/Dialog/professionui.cpp
// Clean reimplementation of ProfessionUI game logic.
//
// ProfessionUI (0x78 bytes) -- profession management panel.
// Extends DialogController (0x30 base). Has Ctor at 0x1400775a0.
//
// Struct layout:
//   +0x00: DialogController base (0x30)
//   +0x30: int* countsArray          // per-profession citizen count
//   +0x38: (unused/zeroed)
//   +0x40: void** spinnerElements    // InternalBasePtr[] for spinner elements
//   +0x48: uint32_t spinnerCount
//   +0x4C: uint32_t spinnerCapacity
//   +0x50: void** numberElements     // InternalBasePtr[] for number display elements
//   +0x58: uint32_t numberCount
//   +0x5C: uint32_t numberCapacity
//   +0x60: void* bstRoot             // BST root (profession -> index mapping)
//   +0x68: uint32_t bstNodeCount
//   +0x6C: (padding)
//   +0x70: uint32_t rotationIndex    // for cycling through citizens in Slot9
//   +0x74: (padding)
//
// BST node layout (0x30 bytes):
//   +0x00: void* key  (ExternalPtr to profession description)
//   +0x08: int index   (profession table index)
//   +0x0C: (padding)
//   +0x10: void* parent
//   +0x18: void* left
//   +0x20: void* right
//   +0x28: int color (red-black flag)
//   +0x2C: (padding)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Dynamic array push helpers (8-byte element arrays)
// ============================================================
static void PushToArray8(uintptr_t self, int ptrOffset, uint64_t value) {
    uintptr_t dataField = self + ptrOffset;
    uintptr_t cntField  = self + ptrOffset + 0x08;
    uintptr_t capField  = self + ptrOffset + 0x0C;

    uint32_t count = *(uint32_t*)cntField;
    uint32_t newCount = count + 1;
    if (newCount == count) return;

    void* oldData = *(void**)dataField;
    *(uint32_t*)cntField = newCount;

    if ((int32_t)*(uint32_t*)capField < (int32_t)newCount) {
        uint32_t newCap = newCount;
        if (count != 0) newCap = newCount * 3;
        *(uint32_t*)capField = newCap;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)(int32_t)newCap * 8, 8);
        *(void**)dataField = newData;
    }

    uint32_t copyStart = 0;
    if (oldData == *(void**)dataField) {
        copyStart = count;
    }
    for (uint32_t i = copyStart; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*(uintptr_t*)dataField + (uintptr_t)i * 8);
        if (dst) {
            if ((int32_t)i < (int32_t)count) {
                uint64_t* src = (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 8);
                *dst = *src;
            } else {
                *dst = value;
            }
        }
    }
    if (oldData != *(void**)dataField) {
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// ProfessionUI::Slot5 (vtable +0x28) -- Setup/populate
// ============================================================
// Finds "professionTable", populates it with profession entries.
// For each profession: creates table cell from template, finds child
// elements (label, spinner, number, button), sets name/sprite/text/IDs,
// inserts into BST, pushes elements to arrays.
//
// Original: 0x140076CA0.

void ProfessionUI_Slot5(ProfessionUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "professionTable" element
    void* dialogElem = *(void**)(s + 0x20);
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "professionTable");
    uint64_t tableResult = 0;
    fn_FindElement(dialogElem, &tableResult, nameBuf);
    fn_String_Dtor(nameBuf);

    void* tableElem = (void*)tableResult;

    // Get column count from table element at +0xC4
    int colCount = *(int*)((uintptr_t)tableElem + 0xC4);

    // Get profession count from game world singleton
    void* worldSingleton = *g_pGameWorldSingleton;
    void* profListPtr = *(void**)((uintptr_t)worldSingleton + 0xB0);
    int profCount = *(int*)((uintptr_t)profListPtr + 0x28);

    // Get template factory: first child of table -> vtable+0xB8
    uint64_t firstChildBuf = 0;
    fn_Element_Child(tableElem, &firstChildBuf, 0);
    void* firstChild = (void*)firstChildBuf;
    uintptr_t firstChildVtbl = *(uintptr_t*)firstChild;
    void** factoryVtbl = *(void***)(firstChildVtbl + 0xB8);

    // Set table size: ceil(profCount / colCount) rows
    int rowCount = ((colCount - 1 + profCount) / colCount * colCount) / colCount;
    fn_TableElement_SetSize(tableElem, rowCount, colCount);

    // Phase 1: Populate table cells from template factory
    int col = 0;
    int row = 0;
    uintptr_t templateData = *(uintptr_t*)(firstChildVtbl + 0x18);

    for (int i = 0; i < profCount; i++) {
        // Clone template via factory: factoryVtbl->vt[8](factory, retval, templateData)
        typedef uint64_t (*fn_FactoryClone_t)(void* factory, void* retval, uintptr_t templateData);
        uint64_t cloneBuf = 0;
        uintptr_t factoryFnAddr = *(uintptr_t*)((uintptr_t)*factoryVtbl + 0x40);
        uint64_t cloned = ((fn_FactoryClone_t)factoryFnAddr)(
            factoryVtbl, &cloneBuf, templateData);

        fn_TableElement_SetChild3(tableElem, (void*)cloned, col, row);

        col++;
        if (col >= *(int*)((uintptr_t)tableElem + 0xC0)) {
            row++;
            col = 0;
        }
    }

    // Phase 2: For each profession, set up child elements
    int tableCol = 0;
    int tableRow = 0;
    int idBase = 300;
    void* profArray = *(void**)((uintptr_t)profListPtr + 0x20);

    for (int i = 0; i < profCount; i++) {
        // Get row element from table
        uint64_t rowElem = 0;
        fn_Element_Child(tableElem, &rowElem, tableRow);

        // Get cell element (the i-th child column within this row)
        uint64_t cellElem = 0;
        fn_Element_Child((void*)rowElem, &cellElem, tableCol);

        // Get 4 children of cell:
        //   child[0] = text/label element (name + sprite)
        //   child[1] = spinner element (worker count)
        //   child[2] = number element (actual count)
        //   child[3] = button element (goto)
        uint64_t textLabelBuf = 0;
        fn_Element_Child((void*)cellElem, &textLabelBuf, 0);
        void* textLabel = (void*)textLabelBuf;

        uint64_t spinnerBuf = 0;
        fn_Element_Child((void*)cellElem, &spinnerBuf, 1);
        void* spinner = (void*)spinnerBuf;

        uint64_t numberBuf = 0;
        fn_Element_Child((void*)cellElem, &numberBuf, 2);
        void* numberElem = (void*)numberBuf;

        uint64_t buttonBuf = 0;
        fn_Element_Child((void*)cellElem, &buttonBuf, 3);
        void* button = (void*)buttonBuf;

        // Set button element ID: idBase + 200
        fn_Element_SetElementId(button, idBase + 200);

        // Get profession ExternalPtr from array
        void* profDesc = *(void**)((uintptr_t)profArray + (uintptr_t)i * 8);

        // Set element name from profession file path
        if (fn_External_GetPath && fn_Filename_GetPathIdentifier) {
            void* pathResult = fn_External_GetPath(profDesc);
            uint8_t identBuf[8];
            fn_Filename_GetPathIdentifier(pathResult, identBuf);
            fn_Element_SetName(spinner, identBuf);
        }

        // BST insert: key=profDesc, value=idBase-300 (=i)
        int bstIndex = idBase - 300;
        void** bstRootPtr = (void**)(s + 0x60);
        void* existingRoot = *bstRootPtr;

        if (existingRoot == nullptr) {
            // First node -- allocate and set as root
            uint8_t* newNode = (uint8_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
            if (newNode) {
                *(void**)(newNode + 0x00) = profDesc;
                *(int*)(newNode + 0x08) = bstIndex;
                *(int*)(newNode + 0x0C) = 0;
                *(void**)(newNode + 0x10) = nullptr;
                *(void**)(newNode + 0x18) = nullptr;
                *(void**)(newNode + 0x20) = nullptr;
                *(int*)(newNode + 0x28) = 0;
                *bstRootPtr = newNode;
            }
        } else {
            // BST insert + rebalance
            uint8_t* current = (uint8_t*)existingRoot;
            uint8_t* newNode = (uint8_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
            if (newNode) {
                *(void**)(newNode + 0x00) = profDesc;
                *(int*)(newNode + 0x08) = bstIndex;
                *(int*)(newNode + 0x0C) = 0;
                *(void**)(newNode + 0x10) = nullptr;
                *(void**)(newNode + 0x18) = nullptr;
                *(void**)(newNode + 0x20) = nullptr;
                *(int*)(newNode + 0x28) = 0;

                bool inserted = false;
                while (!inserted) {
                    void* curKey = *(void**)(current + 0x00);
                    if ((uintptr_t)profDesc > (uintptr_t)curKey) {
                        void* right = *(void**)(current + 0x20);
                        if (right == nullptr) {
                            *(void**)(newNode + 0x10) = current;
                            *(void**)(current + 0x20) = newNode;
                            inserted = true;
                        } else {
                            current = (uint8_t*)right;
                        }
                    } else if ((uintptr_t)profDesc < (uintptr_t)curKey) {
                        void* left = *(void**)(current + 0x18);
                        if (left == nullptr) {
                            *(void**)(newNode + 0x10) = current;
                            *(void**)(current + 0x18) = newNode;
                            inserted = true;
                        } else {
                            current = (uint8_t*)left;
                        }
                    } else {
                        // Duplicate key
                        fn_MemFreeStatic(newNode);
                        inserted = true;
                    }
                }
                if (*(void**)(newNode + 0x10) != nullptr) {
                    void* nodePtr = newNode;
                    fn_BSTRebalance((void*)(s + 0x60), &nodePtr);
                }
            }
        }
        *(int*)(s + 0x68) = *(int*)(s + 0x68) + 1;

        // Set text, sprite, tooltip on text/label element from profession description
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)profDesc + 0x20),
                         (void*)((uintptr_t)profDesc + 0x30));
        fn_TextElement_SetText(textLabel, uiStrBuf);

        uint8_t spriteBuf[16];
        fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)profDesc + 0x28),
                        (void*)((uintptr_t)profDesc + 0x30));
        fn_LabelElement_SetSprite(textLabel, spriteBuf);

        // Tooltip: UI::String from ExternalPtr (single arg)
        uint8_t tooltipBuf[16];
        fn_UIString_Ctor_Entry(tooltipBuf, *(uint64_t*)((uintptr_t)profDesc + 0x20));
        fn_Element_SetToolTip(textLabel, tooltipBuf);

        // Set spinner element ID
        fn_SpinnerElement_SetElementId(spinner, idBase);

        // Set child[0] of spinner same ID
        uint64_t spinnerChild0 = 0;
        fn_Element_Child(spinner, &spinnerChild0, 0);
        fn_Element_SetElementId((void*)spinnerChild0, idBase);

        // Get child[1] of spinner, store as NumberEditElement pointer
        uint64_t spinnerChild1 = 0;
        fn_Element_Child(spinner, &spinnerChild1, 1);
        void* numEditElem = (void*)spinnerChild1;

        // Push spinner's NumberEditElement to spinner array at +0x40
        PushToArray8(s, 0x40, (uint64_t)numEditElem);

        // Push number element to number array at +0x50
        PushToArray8(s, 0x50, (uint64_t)numberElem);

        // If this profession is the "None" profession (at profList+0x30), hide button and last row child
        void* noneProfession = *(void**)((uintptr_t)profListPtr + 0x30);
        if (profDesc == noneProfession) {
            fn_Element_SetFlagsOnOff(numberElem, 1, 0);

            uint64_t lastChild = 0;
            fn_Element_Child((void*)cellElem, &lastChild, 4);
            fn_Element_SetFlagsOnOff((void*)lastChild, 1, 0);
        }

        // Advance table position
        tableCol++;
        if (tableCol >= *(int*)((uintptr_t)tableElem + 0xC0)) {
            tableRow++;
            tableCol = 0;
        }

        idBase++;
    }

    // Resize counts array to profCount
    fn_ExeArrayResize2((void*)(s + 0x30), profCount);

    // Call Slot8 (vtable +0x40) with Time(0.0)
    uint8_t timeBuf[8];
    fn_Time_Ctor_Float(timeBuf, 0.0f);
    typedef void (*fn_VSlot8_t)(void* self, void* time);
    void** vtbl = *(void***)self;
    ((fn_VSlot8_t)vtbl[8])(self, timeBuf);
}

// ============================================================
// ProfessionUI::Slot8 (vtable +0x40) -- Update counts
// ============================================================
// Iterates all citizens, counts how many are assigned to each
// profession (via BST lookup), then updates the NumberEditElement
// values and min/max ranges.
//
// Original: 0x1400765A0.

void ProfessionUI_Slot8(ProfessionUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* worldSingleton = *g_pGameWorldSingleton;
    uintptr_t citizenMgr = *(uintptr_t*)((uintptr_t)worldSingleton + 0xF0);
    void* profListPtr = *(void**)((uintptr_t)worldSingleton + 0xB0);
    int profCount = *(int*)((uintptr_t)profListPtr + 0x28);

    // Zero out counts array
    int* counts = *(int**)(s + 0x30);
    for (int i = 0; i < profCount; i++) {
        counts[i] = 0;
    }

    // Iterate all citizens
    int citizenCount = *(int*)(citizenMgr + 0xA8);
    uintptr_t citizenArray = *(uintptr_t*)(citizenMgr + 0xA0);

    for (int i = 0; i < citizenCount; i++) {
        uintptr_t citizen = *(uintptr_t*)(citizenArray + (uintptr_t)i * 8);

        // Check if citizen is adult: age (float at +0x3C) >= birthAge (float at desc+0x44)
        uintptr_t citizenDesc = *(uintptr_t*)(citizen + 0x288);
        float birthAge = *(float*)(citizenDesc + 0x44);
        float currentAge = *(float*)(citizen + 0x3C);
        if (currentAge < birthAge) continue;

        // Check if citizen is not dead: deathPtr at +0x208 must be 0
        if (*(uintptr_t*)(citizen + 0x208) != 0) continue;

        // Look up profession in BST
        void* profKey = *(void**)(citizen + 0x1F0);
        void* bstRoot = *(void**)(s + 0x60);

        // BST search
        uint8_t* node = (uint8_t*)bstRoot;
        int profIndex = -1;
        while (node) {
            uintptr_t nodeKey = *(uintptr_t*)(node + 0x00);
            if ((uintptr_t)profKey < nodeKey) {
                node = (uint8_t*)*(void**)(node + 0x18);  // left
            } else if ((uintptr_t)profKey > nodeKey) {
                node = (uint8_t*)*(void**)(node + 0x20);  // right
            } else {
                profIndex = *(int*)(node + 0x08);
                break;
            }
        }

        if (profIndex >= 0) {
            counts[profIndex]++;
        }
    }

    // Update first spinner (index 0): set value and min/max to same value (locked)
    void* firstSpinner = *(void**)(*(uintptr_t*)(s + 0x40));
    fn_NumberEditElement_SetValue(firstSpinner, counts[0]);
    fn_NumberEditElement_SetMinMax(firstSpinner, counts[0], counts[0]);

    // Update remaining spinners
    int spinnerCount = *(int*)(s + 0x48);
    for (int i = 1; i < spinnerCount; i++) {
        void* spinnerElem = *(void**)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8);
        fn_NumberEditElement_SetValue(spinnerElem, counts[i]);
        // Min=0, Max=counts[i]+counts[0] (can reassign from "None" profession)
        fn_NumberEditElement_SetMinMax(spinnerElem, 0, counts[i] + counts[0]);
    }

    // Update number elements with actual worker counts per profession
    uintptr_t worldProfList = *(uintptr_t*)((uintptr_t)worldSingleton + 0xB0);
    void* profArrayPtr = *(void**)(worldProfList + 0x20);

    for (int p = 0; p < profCount; p++) {
        uintptr_t profKey = *(uintptr_t*)((uintptr_t)profArrayPtr + (uintptr_t)p * 8);

        // Look up profession in world BST at citizenMgr+0x100
        uint8_t* bstNode = *(uint8_t**)(citizenMgr + 0x100);
        int entityCount = 0;
        uintptr_t entityListPtr = 0;

        while (bstNode) {
            uintptr_t nodeKey = *(uintptr_t*)(bstNode + 0x00);
            if (profKey < nodeKey) {
                bstNode = *(uint8_t**)(bstNode + 0x20);
            } else if (profKey > nodeKey) {
                bstNode = *(uint8_t**)(bstNode + 0x28);
            } else {
                entityCount = *(int*)(bstNode + 0x10);
                entityListPtr = *(uintptr_t*)(bstNode + 0x08);
                break;
            }
        }

        int totalWorkers = 0;
        for (int e = 0; e < entityCount; e++) {
            uintptr_t entityBP = *(uintptr_t*)(entityListPtr + (uintptr_t)e * 8);

            // Find WorkplaceComponent on entity
            int compCount = *(int*)(entityBP + 0x38);
            uintptr_t compArray = *(uintptr_t*)(entityBP + 0x30);
            uintptr_t workplaceComp = 0;

            for (int c = 0; c < compCount; c++) {
                uintptr_t comp = *(uintptr_t*)(compArray + (uintptr_t)c * 8);
                typedef int (*fn_GetTypeId_t)(void*);
                void** vtbl = *(void***)(comp);
                int typeId = ((fn_GetTypeId_t)vtbl[2])((void*)comp);
                if (typeId == (int)*g_TypeId_WorkplaceComponent) {
                    if (fn_IsDerivedFrom((void*)comp, *g_TypeId_WorkplaceComponent)) {
                        workplaceComp = comp;
                    }
                    break;
                }
            }

            if (workplaceComp) {
                // Check if workplace is enabled (+0x50 != 0)
                if (*(uint8_t*)(workplaceComp + 0x50) != 0) {
                    // Find StorageLocationComponent on parent entity
                    uintptr_t parentEntity = *(uintptr_t*)(workplaceComp + 0x10);
                    int pCompCount = *(int*)(parentEntity + 0x38);
                    uintptr_t pCompArray = *(uintptr_t*)(parentEntity + 0x30);
                    uintptr_t storageLocComp = 0;

                    for (int c = 0; c < pCompCount; c++) {
                        uintptr_t comp = *(uintptr_t*)(pCompArray + (uintptr_t)c * 8);
                        typedef int (*fn_GetTypeId_t)(void*);
                        void** vtbl = *(void***)(comp);
                        int typeId = ((fn_GetTypeId_t)vtbl[2])((void*)comp);
                        if (typeId == (int)*g_TypeId_StorageLocationComponent) {
                            if (fn_IsDerivedFrom((void*)comp, *g_TypeId_StorageLocationComponent)) {
                                storageLocComp = comp;
                            }
                            break;
                        }
                    }

                    if (storageLocComp) {
                        int workerSlots = *(int*)(storageLocComp + 0x30);
                        if (workerSlots == 0) {
                            workerSlots = fn_CalcWorkerSlots((void*)storageLocComp);
                            *(int*)(storageLocComp + 0x30) = workerSlots;
                        }
                        totalWorkers += workerSlots;
                    }
                }
            }
        }

        // Set number element value
        void* numElem = *(void**)(*(uintptr_t*)(s + 0x50) + (uintptr_t)p * 8);
        fn_NumberElement_SetValueInt(numElem, totalWorkers);
    }
}

// ============================================================
// ProfessionUI::Slot9 (vtable +0x48) -- Event handler
// ============================================================
// Handles spinner changes (msg 0x12) and button clicks (msg 0x15).
// Spinner: reassigns citizens between professions.
// Button: finds a citizen of that profession and centers camera on them.
//
// Original: 0x140076C10.

void ProfessionUI_Slot9(ProfessionUI* self, void* param2,
                         int param3, int param4) {
    void* worldSingleton = *g_pGameWorldSingleton;
    void* profListPtr = *(void**)((uintptr_t)worldSingleton + 0xB0);
    int profCount = *(int*)((uintptr_t)profListPtr + 0x28);

    if (param3 == 0x12) {
        // Spinner value changed -- reassign citizens
        if (param4 >= 300 && param4 <= profCount + 300) {
            if (fn_ProfessionUI_HandleSpinner) {
                fn_ProfessionUI_HandleSpinner(self, param4 - 300);
            }
        }
    }

    if (param3 == 0x15) {
        // Button clicked -- find and center on citizen
        if (param4 > 499 && param4 <= profCount + 500) {
            if (fn_ProfessionUI_HandleButton) {
                fn_ProfessionUI_HandleButton(self, param4 - 500);
            }
        }
    }
}
