// code/Game/Dialog/storageui.cpp
// Clean reimplementation of StorageUI panel methods.
//
// StorageUI (0x40 bytes): Storage contents display panel.
// Layout:
//   +0x18 = dialog instance (from DialogController base)
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x28 = entity data source (InternalBasePtr, from DialogController)
//   +0x30 = row array ptr (each 0x18: labelStorage ptr, numberCount ptr, rowElem ptr)
//   +0x38 = row array count (int32)
//   +0x3C = row array capacity (int32)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include "resolve.h"

// Helper: find element by name in parent, returns raw pointer
// Original pattern: FindElement → CheckPtr → deref
static void* StorageUI_FindChildElem(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_FindElement(parentElem, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);  // resolve IBP before deref
    void* result = *(void**)retBuf;
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
static uint64_t StorageUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    void* raw = StorageUI_FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    fn_InternalBasePtr_Ctor(bpBuf, raw);
    return *(uint64_t*)bpBuf;
}

// ============================================================
// StorageUI::Serialize (vtable +0x20) — Setup storage table
// ============================================================
// Finds "storageTable" (or "storageTable2" fallback). Counts resource
// types via ExternalFactory::FindNext. Resizes table to fit, clones
// template elements into each cell. Finds "labelStorage" and
// "numberCount" sub-elements in each row, stores in dynamic array.
// Finally calls Startup (via vtable +0x38).
//
// Original: 0x1400b8820.

void StorageUI_Serialize(StorageUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    // Find "storageTable", fallback to "storageTable2"
    void* tableRaw = StorageUI_FindChildElem(dialogElem, "storageTable");
    if (!tableRaw) {
        uint8_t nameBuf[8], retBuf[16];
        fn_String_Ctor_CStr(nameBuf, "storageTable2");
        fn_FindElement(dialogElem, retBuf, nameBuf);
        tableRaw = *(void**)retBuf;
        fn_String_Dtor(nameBuf);
    }

    // Wrap table as InternalBasePtr
    uint8_t tableBpBuf[8];
    fn_InternalBasePtr_Ctor(tableBpBuf, tableRaw);
    uintptr_t tableElem = *(uintptr_t*)tableBpBuf;

    // Count resource types via ExternalFactory::FindNext
    uint32_t resourceTypeCount = 0;
    void* factory = *g_pExternalFactorySingleton;
    void* ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_RawMaterialDescription, nullptr);
    while (ext != nullptr) {
        resourceTypeCount++;
        ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_RawMaterialDescription, ext);
    }

    // Get column count from table element +0xC4
    int colCount = *(int*)(tableElem + 0xC4);

    // Calculate row count: ceil(resourceTypeCount / colCount)
    int rowCount = ((int)(resourceTypeCount - 1 + (uint32_t)colCount) / colCount * colCount) / colCount;

    // Get template factory from first child element at +0xB8 (NOT vtable+0xB8)
    uint8_t childBuf[16];
    fn_Element_Child((void*)tableElem, childBuf, 0);
    void* firstChild = *(void**)childBuf;
    void* templateObj = *(void**)((uintptr_t)firstChild + 0xB8);

    // Ensure minimum row count from table's +0xC0
    int minRows = *(int*)(tableElem + 0xC0);
    if (rowCount < minRows) {
        rowCount = minRows;
    }

    // Resize the table
    fn_TableElement_SetSize((void*)tableElem, rowCount, colCount);

    // Clone template elements into each table cell
    int totalCells = fn_Element_ChildCount((void*)tableElem);
    for (int i = 0; i < totalCells; i++) {
        // Get dialog resource reference: *(dialogInstance+0x18)
        void* dialogInst = *(void**)(s + 0x18);
        void* dialogResource = *(void**)((uintptr_t)dialogInst + 0x18);

        // Clone via template vtable[8]: Clone(this, retBuf, dialogResource)
        void** templateVt = *(void***)templateObj;
        typedef void* (*fn_Clone_t)(void*, void*, void*);
        uint8_t cloneBuf[16];
        ((fn_Clone_t)templateVt[8])(templateObj, cloneBuf, dialogResource);

        // Set cloned element as child of table
        fn_TableElement_SetChild2((void*)tableElem, cloneBuf, i);
    }

    // Iterate all cells, find "labelStorage" and "numberCount" sub-elements
    totalCells = fn_Element_ChildCount((void*)tableElem);
    for (int i = 0; i < totalCells; i++) {
        // Get child row element — CheckPtr before deref (matches original)
        fn_Element_Child((void*)tableElem, childBuf, i);
        fn_InternalBasePtr_CheckPtr(childBuf);
        void* rowElem = *(void**)childBuf;

        // Find "labelStorage" element
        uint64_t labelStorage = StorageUI_FindElemAsBasePtr(rowElem, "labelStorage");

        // Find "numberCount" element
        uint64_t numberCount = StorageUI_FindElemAsBasePtr(rowElem, "numberCount");

        // Get raw child element ptr
        fn_Element_Child((void*)tableElem, childBuf, i);
        fn_InternalBasePtr_CheckPtr(childBuf);
        uint64_t rawChild = *(uint64_t*)childBuf;

        // Build 0x18-byte entry: {labelStorage, numberCount, rawChild}
        uint64_t entry[3];
        entry[0] = labelStorage;
        entry[1] = numberCount;
        entry[2] = rawChild;

        // Push into dynamic array at +0x30
        int oldCount = *(int*)(s + 0x38);
        int newCount = oldCount + 1;
        void* oldBuf = *(void**)(s + 0x30);
        *(int*)(s + 0x38) = newCount;

        if (*(int*)(s + 0x3C) < newCount) {
            int newCap = newCount;
            if (oldCount != 0) {
                newCap = newCount * 3;
            }
            *(int*)(s + 0x3C) = newCap;
            void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 0x18, 8);
            *(void**)(s + 0x30) = newBuf;
        }

        // Copy existing entries and append new one
        void* curBuf = *(void**)(s + 0x30);
        int copyStart = (oldBuf == curBuf) ? oldCount : 0;
        for (int k = copyStart; k < newCount; k++) {
            uint64_t* dst = (uint64_t*)((uintptr_t)curBuf + k * 0x18);
            if (dst) {
                uint64_t* src = entry;
                if (k < oldCount) {
                    src = (uint64_t*)((uintptr_t)oldBuf + k * 0x18);
                }
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            }
        }
        if (oldBuf != curBuf) {
            fn_MemFreeStatic(oldBuf);
        }
    }

    // Call Startup via vtable +0x38
    typedef void (*fn_Startup_t)(void*);
    void** vtable = *(void***)self;
    ((fn_Startup_t)vtable[7])(self);  // vtable +0x38 = index 7
}

// ============================================================
// StorageUI::Startup (vtable +0x38) — Update storage contents
// ============================================================
// Gets entity's StateMachine component, iterates storage entries.
// For each entry with items, finds the ResourceType component.
// Builds a temp array sorted alphabetically by resource name.
// Displays items in table rows with sprite, name, and count.
// Hides unused rows.
//
// Original: 0x1400b8450.

void StorageUI_Startup(StorageUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity, check ComponentEntity
    void* entityRaw = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? entityRaw : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entity);
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find StorageComponent (type ID 0x1401c7e78) — NOT StateMachine!
    // Original calls FUN_140006270 which searches by StorageComponent type ID.
    uint64_t storageComp = 0;
    fn_FindComponent_Storage((void*)entityPtr, &storageComp);
    fn_InternalBasePtr_CheckPtr(&storageComp);

#ifdef _DEBUG
    printf("[StorageUI::Startup] self=%p entity=%p storageComp=0x%llx\n",
           (void*)self, entity, (unsigned long long)storageComp);
#endif

    // Build temp array of storage items
    void* sortedItems = nullptr;
    void* currentItems = nullptr;
    int itemCount = 0;
    int itemCapacity = 0;

    int smEntryCount = storageComp ? *(int*)(storageComp + 0x30) : 0;
#ifdef _DEBUG
    printf("[StorageUI::Startup] smEntryCount=%d (comp+0x28=%p comp+0x30=%d comp+0x38=%d)\n",
           smEntryCount,
           storageComp ? (void*)*(uintptr_t*)(storageComp + 0x28) : nullptr,
           storageComp ? *(int*)(storageComp + 0x30) : -1,
           storageComp ? *(int*)(storageComp + 0x38) : -1);
    fflush(stdout);
#endif
    if (smEntryCount > 0) {
        uintptr_t entryOffset = 0;
        itemCount = 0;

        for (int i = 0; i < smEntryCount; i++) {
            uintptr_t smArray = *(uintptr_t*)(storageComp + 0x28);
            uintptr_t entryPtr = smArray + entryOffset;
            int count = *(int*)(entryPtr + 8);

            if (count != 0) {
                uintptr_t resEntry = *(uintptr_t*)entryPtr;

                // Find ResourceType component in entry's component list
                int compCount = *(int*)(resEntry + 0x30);
                void* resType = nullptr;
#ifdef _DEBUG
                printf("[StorageUI::Startup]   entry[%d]: count=%d resEntry=%p compCount=%d\n",
                       i, count, (void*)resEntry, compCount);
#endif
                for (int c = 0; c < compCount; c++) {
                    void* comp = *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8);
                    typedef int (*fn_GetTypeId_t)(void*);
                    int typeId = ((fn_GetTypeId_t)(*(void***)comp)[2])(comp);
                    if (typeId == (int)*g_TypeId_RawMaterialDescription) {
                        if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                            resType = comp;
                        }
                        break;
                    }
                }

                // Build 0x18-byte item: {resEntry, resType, count(as low32)}
                uint64_t item[3];
                item[0] = resEntry;
                item[1] = (uint64_t)resType;
                item[2] = (uint64_t)count;

                // Push into temp array
                int newCount = itemCount + 1;
                if (newCount != itemCount) {
                    if (itemCapacity < newCount) {
                        itemCapacity = newCount;
                        if (itemCount != 0) {
                            itemCapacity = newCount * 3;
                        }
                        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton,
                                                    (uint64_t)itemCapacity * 0x18, 8);
                        sortedItems = newBuf;
                    }

                    int copyStart = (currentItems == sortedItems) ? itemCount : 0;
                    for (int k = copyStart; k < newCount; k++) {
                        uint64_t* dst = (uint64_t*)((uintptr_t)sortedItems + k * 0x18);
                        if (dst) {
                            uint64_t* src = item;
                            if (k < itemCount) {
                                src = (uint64_t*)((uintptr_t)currentItems + k * 0x18);
                            }
                            dst[0] = src[0];
                            dst[1] = src[1];
                            dst[2] = src[2];
                        }
                    }
                    if (currentItems != sortedItems) {
                        fn_MemFreeStatic(currentItems);
                    }
                    currentItems = sortedItems;
                    itemCount = newCount;
                }
            }

            entryOffset += 0x10;
        }

        // Sort items alphabetically by resource name
        if (itemCount > 1) {
            StorageUI_SortItems(sortedItems, itemCount, nullptr, nullptr);
        }
    }

    // Populate table rows with sorted items
#ifdef _DEBUG
    printf("[StorageUI::Startup] itemCount=%d totalRows=%d sortedItems=%p\n",
           itemCount, *(int*)(s + 0x38), sortedItems);
    fflush(stdout);
#endif
    int rowIdx = 0;
    if (itemCount > 0) {
        uintptr_t itemPtr = (uintptr_t)sortedItems + 0x10;  // start at first item's resType

        for (int i = 0; i < itemCount; i++) {
            if (rowIdx >= *(int*)(s + 0x38)) break;

            uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x18;

            uintptr_t resType = *(uintptr_t*)(itemPtr - 8);  // item[1] = resType
            int count = *(int*)itemPtr;  // item[2] = count

#ifdef _DEBUG
            {
                fn_InternalBasePtr_CheckPtr((void*)rowBase);
                void* labelE = *(void**)rowBase;
                printf("[StorageUI::Startup]   row[%d]: resType=%p count=%d labelElem=%p\n",
                       rowIdx, (void*)resType, count, labelE);
                fflush(stdout);
            }
#endif

            // Set sprite from resource type
            uint8_t spriteBuf[16];
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(resType + 0x20),
                           (void*)(resType + 0x30));
            fn_InternalBasePtr_CheckPtr((void*)rowBase);
            void* labelElem = *(void**)rowBase;
            fn_LabelElement_SetSprite(labelElem, spriteBuf);

            // Set text from resource type
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)(resType + 0x28),
                             (void*)(resType + 0x30));
            fn_InternalBasePtr_CheckPtr((void*)rowBase);
            labelElem = *(void**)rowBase;
            fn_TextElement_SetText(labelElem, uiStrBuf);

            // Set count
            fn_InternalBasePtr_CheckPtr((void*)(rowBase + 8));
            void* numberElem = *(void**)(rowBase + 8);
            fn_NumberElement_SetValueInt(numberElem, count);

            // Show both elements (flags: hide=0, show=1)
            fn_InternalBasePtr_CheckPtr((void*)rowBase);
            fn_Element_SetFlagsOnOff(*(void**)rowBase, 0, 1);
            fn_InternalBasePtr_CheckPtr((void*)(rowBase + 8));
            fn_Element_SetFlagsOnOff(*(void**)(rowBase + 8), 0, 1);

            rowIdx++;
            itemPtr += 0x18;
        }
    }

    // Hide unused rows
    int totalRows = *(int*)(s + 0x38);
    if (rowIdx < totalRows) {
        uintptr_t hideOffset = (uintptr_t)rowIdx * 0x18;
        for (int i = rowIdx; i < totalRows; i++) {
            uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + hideOffset;

            // Hide both elements (flags: hide=1, show=0) — CheckPtr before deref
            fn_InternalBasePtr_CheckPtr((void*)rowBase);
            fn_Element_SetFlagsOnOff(*(void**)rowBase, 1, 0);
            fn_InternalBasePtr_CheckPtr((void*)(rowBase + 8));
            fn_Element_SetFlagsOnOff(*(void**)(rowBase + 8), 1, 0);

            hideOffset += 0x18;
        }
    }

    // Free temp sorted items array
    fn_MemFreeStatic(currentItems);
}

// ============================================================
// StorageUI_SortItems — Introsort on 0x18-byte resource display structs
// ============================================================
// Faithful reimplementation of FUN_1400213f0.
// Sorts an array of {entityPtr, resourceTypePtr, count} structs (0x18 bytes each)
// alphabetically by resource name using introsort (quicksort + selection sort fallback).
//
// Comparison (FUN_1400b8370): extracts resource name via UIString from
// item[1]+0x28 (stringTable) and item[1]+0x30 (stringName), then uses
// System::String::operator< to compare.

// Compare: returns true if name(a) < name(b)
static bool StorageUI_CompareItems(const uint64_t* a, const uint64_t* b) {
    uintptr_t resA = (uintptr_t)a[1];
    uintptr_t resB = (uintptr_t)b[1];

    // Build UIString -> convert to System::String for item A
    uint8_t uiStrA[16];
    fn_UIString_Ctor(uiStrA, *(uint64_t*)(resA + 0x28), (void*)(resA + 0x30));
    uint8_t tempA[8] = {};
    fn_UIString_ToSystemString(uiStrA, tempA);  // UIString → System::String conversion
    uint8_t sysStrA[8];
    fn_String_Ctor_Copy(sysStrA, tempA);
    fn_String_Dtor(tempA);

    // Build UIString -> convert to System::String for item B
    uint8_t uiStrB[16];
    fn_UIString_Ctor(uiStrB, *(uint64_t*)(resB + 0x28), (void*)(resB + 0x30));
    uint8_t tempB[8] = {};
    fn_UIString_ToSystemString(uiStrB, tempB);  // UIString → System::String conversion
    uint8_t sysStrB[8];
    fn_String_Ctor_Copy(sysStrB, tempB);
    fn_String_Dtor(tempB);

    bool result = fn_String_LessThan(sysStrA, sysStrB);

    fn_String_Dtor(sysStrB);
    fn_String_Dtor(sysStrA);

    return result;
}

// Swap two 0x18-byte items (3 uint64_t)
static inline void StorageUI_SwapItems(uint64_t* a, uint64_t* b) {
    uint64_t t0 = a[0], t1 = a[1], t2 = a[2];
    a[0] = b[0]; a[1] = b[1]; a[2] = b[2];
    b[0] = t0;   b[1] = t1;   b[2] = t2;
}

// Selection sort for small partitions (operator_impl_5ad0 / FUN_140015ad0)
// Finds max element and swaps it to end, shrinking unsorted region.
static void StorageUI_SelectionSort(uint64_t* first, uint64_t* last) {
    for (uint64_t* end = last; first < end; end -= 3) {
        uint64_t* best = first;
        uint64_t* scan = first;
        while (scan + 3 <= end) {
            if (StorageUI_CompareItems(best, scan + 3)) {
                best = scan + 3;
            }
            scan += 3;
        }
        StorageUI_SwapItems(best, end);
    }
}

// Introsort: quicksort with selection sort fallback for <9 elements.
// Faithful reimplementation of FUN_1400213f0.
void StorageUI_SortItems(void* items, int count, void* /*scratch*/, void* /*unused*/) {
    if (count < 2) return;

    uint64_t* param_1 = (uint64_t*)items;  // lo pointer (mutable)
    uint64_t* plVar11 = param_1 + (int64_t)(count - 1) * 3;  // hi pointer

    // Explicit stack matching original (two parallel arrays of 62 entries)
    uint64_t* stackLo[62];
    uint64_t* stackHi[62];
    int64_t lVar7 = 0;  // stack depth

    for (;;) {
        int64_t span = ((int64_t)plVar11 - (int64_t)param_1) / 0x18 + 1;

        if (span < 9) {
            // Selection sort for small partition
            if (param_1 < plVar11) {
                StorageUI_SelectionSort(param_1, plVar11);
            }
            goto pop;
        }

        // Median-of-three pivot selection
        {
            uint64_t* plVar8 = param_1 + (span >> 1) * 3;  // mid = pivot

            if (StorageUI_CompareItems(plVar8, param_1))
                StorageUI_SwapItems(param_1, plVar8);
            if (StorageUI_CompareItems(plVar11, param_1))
                StorageUI_SwapItems(param_1, plVar11);
            if (StorageUI_CompareItems(plVar11, plVar8))
                StorageUI_SwapItems(plVar8, plVar11);

            // Partition
            uint64_t* plVar6 = param_1;   // forward scan pointer
            uint64_t* plVar9 = plVar11;   // backward scan pointer
            uint64_t* plVar10;            // trailing pointer (one behind plVar9)

        partition:
            // Forward scan: find element where pivot < elem
            if (plVar6 < plVar8) {
                do {
                    plVar6 += 3;
                    if (plVar8 <= plVar6) goto forward_past_pivot;
                } while (!StorageUI_CompareItems(plVar8, plVar6));
                if (plVar8 <= plVar6) goto forward_past_pivot;
            } else {
            forward_past_pivot:
                do {
                    plVar6 += 3;
                    if (plVar11 < plVar6) break;
                } while (!StorageUI_CompareItems(plVar8, plVar6));
            }

            // Backward scan: find element where pivot < elem is false
            do {
                plVar10 = plVar9;
                plVar9 = plVar10 - 3;
                if (plVar9 <= plVar8) break;
            } while (StorageUI_CompareItems(plVar8, plVar9));

            if (plVar6 <= plVar9) {
                // Swap and continue partitioning
                StorageUI_SwapItems(plVar6, plVar9);
                if (plVar8 == plVar9) {
                    plVar8 = plVar6;  // pivot moved
                }
                goto partition;
            }

            // Find final pivot boundary (plVar10 tracks the split point)
            if (plVar8 < plVar10) {
                do {
                    plVar10 -= 3;
                    if (plVar10 <= plVar8) goto scan_left;
                    bool less = StorageUI_CompareItems(plVar10, plVar8);
                    if (less) break;
                    bool greater = StorageUI_CompareItems(plVar8, plVar10);
                    if (greater) break;
                } while (true);
                if (plVar10 <= plVar8) goto scan_left;
            } else {
            scan_left:
                do {
                    plVar10 -= 3;
                    if (plVar10 <= param_1) break;
                    bool less = StorageUI_CompareItems(plVar10, plVar8);
                    if (less) break;
                    bool greater = StorageUI_CompareItems(plVar8, plVar10);
                    if (greater) break;
                } while (true);
            }

            // Decide which partition to push vs iterate
            int64_t rightBytes = (int64_t)plVar11 - (int64_t)plVar6;
            int64_t rightDiv = rightBytes / 6 + (rightBytes >> 63);
            int64_t param_3 = (rightDiv >> 2) - (rightDiv >> 63);
            int64_t leftSize = ((int64_t)plVar10 - (int64_t)param_1) / 0x18;

            if (param_3 <= leftSize) {
                // Left partition is large: push left, iterate right
                if (param_1 < plVar10) {
                    stackLo[lVar7] = param_1;
                    stackHi[lVar7] = plVar10;
                    lVar7++;
                }
                param_1 = plVar6;
                if (plVar11 <= plVar6) goto pop;
            } else {
                // Right partition is large: push right, iterate left
                if (plVar6 < plVar11) {
                    stackLo[lVar7] = plVar6;
                    stackHi[lVar7] = plVar11;
                    lVar7++;
                }
                plVar11 = plVar10;
                if (plVar10 <= param_1) goto pop;
            }
            continue;
        }

    pop:
        lVar7--;
        if (lVar7 < 0) return;
        param_1 = stackLo[lVar7];
        plVar11 = stackHi[lVar7];
    }
}

