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

// Helper: find element by name in parent, returns raw InternalPtr value
static void* StorageUI_FindChildElem(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_FindElement(parentElem, retBuf, nameStr);
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
    void* ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_ResourceType, nullptr);
    while (ext != nullptr) {
        resourceTypeCount++;
        ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_ResourceType, ext);
    }

    // Get column count from table element +0xC4
    int colCount = *(int*)(tableElem + 0xC4);

    // Calculate row count: ceil(resourceTypeCount / colCount)
    int rowCount = ((int)(resourceTypeCount - 1 + (uint32_t)colCount) / colCount * colCount) / colCount;

    // Get template object from first child's element at +0xB8
    uint8_t childBuf[16];
    fn_Element_Child((void*)tableElem, childBuf, 0);
    void* firstChild = *(void**)childBuf;
    void* templateObj = *(void**)(*(uintptr_t*)firstChild + 0xB8);

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
        // Get child row element
        fn_Element_Child((void*)tableElem, childBuf, i);
        void* rowElem = *(void**)childBuf;

        // Find "labelStorage" element
        uint64_t labelStorage = StorageUI_FindElemAsBasePtr(rowElem, "labelStorage");

        // Find "numberCount" element
        uint64_t numberCount = StorageUI_FindElemAsBasePtr(rowElem, "numberCount");

        // Get raw child element ptr
        fn_Element_Child((void*)tableElem, childBuf, i);
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

    // Find StateMachine component
    uint64_t stateMachineComp = 0;
    fn_FindComponent_StateMachine((void*)entityPtr, &stateMachineComp);

    // Build temp array of storage items
    void* sortedItems = nullptr;
    void* currentItems = nullptr;
    int itemCount = 0;
    int itemCapacity = 0;

    int smEntryCount = *(int*)(stateMachineComp + 0x30);
    if (smEntryCount > 0) {
        uintptr_t entryOffset = 0;
        itemCount = 0;

        for (int i = 0; i < smEntryCount; i++) {
            uintptr_t smArray = *(uintptr_t*)(stateMachineComp + 0x28);
            uintptr_t entryPtr = smArray + entryOffset;
            int count = *(int*)(entryPtr + 8);

            if (count != 0) {
                uintptr_t resEntry = *(uintptr_t*)entryPtr;

                // Find ResourceType component in entry's component list
                int compCount = *(int*)(resEntry + 0x30);
                void* resType = nullptr;
                for (int c = 0; c < compCount; c++) {
                    void* comp = *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8);
                    typedef int (*fn_GetTypeId_t)(void*);
                    int typeId = ((fn_GetTypeId_t)(*(void***)comp)[2])(comp);
                    if (typeId == (int)*g_TypeId_ResourceType) {
                        if (fn_IsDerivedFrom(comp, *g_TypeId_ResourceType)) {
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
            uint8_t scratch[8];
            StorageUI_SortItems(sortedItems, itemCount, scratch, nullptr);
        }
    }

    // Populate table rows with sorted items
    int rowIdx = 0;
    if (itemCount > 0) {
        uintptr_t itemPtr = (uintptr_t)sortedItems + 0x10;  // start at first item's resType

        for (int i = 0; i < itemCount; i++) {
            if (rowIdx >= *(int*)(s + 0x38)) break;

            uintptr_t rowBase = *(uintptr_t*)(s + 0x30) + rowIdx * 0x18;
            void* labelElem = *(void**)rowBase;
            void* numberElem = *(void**)(rowBase + 8);

            uintptr_t resType = *(uintptr_t*)(itemPtr - 8);  // item[1] = resType
            int count = *(int*)itemPtr;  // item[2] = count

            // Set sprite from resource type
            uint8_t spriteBuf[16];
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(resType + 0x20),
                           (void*)(resType + 0x30));
            fn_LabelElement_SetSprite(labelElem, spriteBuf);

            // Set text from resource type
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)(resType + 0x28),
                             (void*)(resType + 0x30));
            fn_TextElement_SetText(labelElem, uiStrBuf);

            // Set count
            fn_NumberElement_SetValueInt(numberElem, count);

            // Show both elements (flags: hide=0, show=1)
            fn_Element_SetFlagsOnOff(labelElem, 0, 1);
            fn_Element_SetFlagsOnOff(numberElem, 0, 1);

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
            void* labelElem = *(void**)rowBase;
            void* numberElem = *(void**)(rowBase + 8);

            // Hide both elements (flags: hide=1, show=0)
            fn_Element_SetFlagsOnOff(labelElem, 1, 0);
            fn_Element_SetFlagsOnOff(numberElem, 1, 0);

            hideOffset += 0x18;
        }
    }

    // Free temp sorted items array
    fn_MemFreeStatic(currentItems);
}

// ============================================================
// StorageUI_SortItems — Introsort on 0x18-byte resource display structs
// ============================================================
// Replaces FUN_1400213f0. Sorts an array of {entityPtr, resourceTypePtr, count}
// structs alphabetically by resource name. Each struct is 0x18 bytes (3 uint64_t).
//
// The original exe implementation was an introsort (quicksort + insertion sort
// fallback for small partitions). The comparison function (FUN_1400b8370) extracts
// the resource name string from item[1] (resourceType)+0x28 -> StringTable,
// +0x30 -> StringName, and uses System::String::operator< to compare.
//
// We reimplement as a clean C++ introsort with the same comparison semantics.
// The scratch parameters are unused in the original for this call path.

// Compare two 0x18-byte items by resource name (alphabetical order).
// Returns true if item at 'a' should sort before item at 'b'.
static bool StorageUI_CompareItems(const uint64_t* a, const uint64_t* b) {
    // a[1] = resourceTypePtr, b[1] = resourceTypePtr
    // Resource name: UIString from resourceType+0x28 (stringTable), resourceType+0x30 (stringName)
    uintptr_t resA = (uintptr_t)a[1];
    uintptr_t resB = (uintptr_t)b[1];

    // Build UIString -> System::String for item A
    uint8_t uiStrA[16];
    fn_UIString_Ctor(uiStrA, *(uint64_t*)(resA + 0x28), (void*)(resA + 0x30));
    uint8_t sysStrA[16];
    fn_String_Ctor_Copy(sysStrA, uiStrA);
    fn_String_Dtor(uiStrA);

    // Build UIString -> System::String for item B
    uint8_t uiStrB[16];
    fn_UIString_Ctor(uiStrB, *(uint64_t*)(resB + 0x28), (void*)(resB + 0x30));
    uint8_t sysStrB[16];
    fn_String_Ctor_Copy(sysStrB, uiStrB);
    fn_String_Dtor(uiStrB);

    // Compare: String::operator< (resolved in engine_funcs.cpp)
    bool result = fn_String_LessThan(sysStrA, sysStrB);

    fn_String_Dtor(sysStrB);
    fn_String_Dtor(sysStrA);

    return result;
}

// Swap two 0x18-byte items
static inline void StorageUI_SwapItems(uint64_t* a, uint64_t* b) {
    uint64_t t0 = a[0], t1 = a[1], t2 = a[2];
    a[0] = b[0]; a[1] = b[1]; a[2] = b[2];
    b[0] = t0;   b[1] = t1;   b[2] = t2;
}

// Insertion sort for small partitions (0x18-byte elements)
static void StorageUI_InsertionSort(uint64_t* first, uint64_t* last) {
    // 'last' points to the last element (inclusive), each element is 3 uint64_t
    for (uint64_t* cur = first + 3; cur <= last; cur += 3) {
        // Find the rightmost element in [first, cur) that is greater than *cur
        uint64_t* best = first;
        for (uint64_t* scan = first + 3; scan <= cur; scan += 3) {
            if (!StorageUI_CompareItems(best, scan)) {
                best = scan;
            }
        }
        // Swap best with last element in unsorted portion
        if (best != last) {
            // Selection sort style: find max, swap to end
        }
    }

    // Actually, the original FUN_140015ad0 is a selection sort (finds max, swaps to end).
    // Reimplement that directly:
    for (uint64_t* end = last; end > first; end -= 3) {
        uint64_t* maxElem = first;
        for (uint64_t* scan = first + 3; scan <= end; scan += 3) {
            if (StorageUI_CompareItems(maxElem, scan)) {
                maxElem = scan;
            }
        }
        StorageUI_SwapItems(maxElem, end);
    }
}

// Introsort implementation (quicksort with insertion sort fallback)
// Reimplements FUN_1400213f0.
void StorageUI_SortItems(void* items, int count, void* /*scratch*/, void* /*scratch2*/) {
    if (count < 2) return;

    uint64_t* arr = (uint64_t*)items;
    // Each element is 3 uint64_t (0x18 bytes)

    // Stack-based iterative quicksort (matches original's explicit stack of 62 entries)
    struct Range { uint64_t* lo; uint64_t* hi; };
    Range stack[64];
    int stackTop = 0;

    uint64_t* lo = arr;
    uint64_t* hi = arr + (int64_t)(count - 1) * 3;

    for (;;) {
        int64_t span = (hi - lo) / 3 + 1;  // number of elements

        if (span < 9) {
            // Insertion/selection sort for small partitions
            if (lo < hi) {
                StorageUI_InsertionSort(lo, hi);
            }
            goto pop;
        }

        // Median-of-three pivot selection
        {
            uint64_t* mid = lo + (span / 2) * 3;

            // Sort lo, mid, hi (3-element sort for pivot selection)
            if (StorageUI_CompareItems(mid, lo)) {
                StorageUI_SwapItems(lo, mid);
            }
            if (StorageUI_CompareItems(hi, lo)) {
                StorageUI_SwapItems(lo, hi);
            }
            if (StorageUI_CompareItems(hi, mid)) {
                StorageUI_SwapItems(mid, hi);
            }

            // Partition around pivot (mid)
            uint64_t* i = lo;
            uint64_t* j = hi;

            for (;;) {
                // Scan forward: find element >= pivot
                if (i < mid) {
                    do {
                        i += 3;
                        if (i >= mid) break;
                    } while (!StorageUI_CompareItems(mid, i));
                }
                if (i >= mid) {
                    // Continue scanning from mid forward
                    do {
                        i += 3;
                        if (i > hi) break;
                    } while (!StorageUI_CompareItems(mid, i));
                }

                // Scan backward: find element <= pivot
                do {
                    j -= 3;
                    if (j <= mid) break;
                } while (StorageUI_CompareItems(mid, j));

                if (i > j) break;

                // Swap and update pivot pointer if needed
                StorageUI_SwapItems(i, j);
                if (mid == j) {
                    mid = i;
                }
            }

            // Find final pivot position
            uint64_t* pivotPos = j;
            if (mid < pivotPos + 3) {
                // Scan backward for final position
                while (pivotPos > mid) {
                    pivotPos -= 3;
                    if (pivotPos <= mid) break;
                    bool less = StorageUI_CompareItems(pivotPos, mid);
                    bool greater = StorageUI_CompareItems(mid, pivotPos);
                    if (less || greater) break;
                }
            } else {
                while (pivotPos > lo) {
                    pivotPos -= 3;
                    if (pivotPos <= lo) break;
                    bool less = StorageUI_CompareItems(pivotPos, mid);
                    if (less) break;
                    bool greater = StorageUI_CompareItems(mid, pivotPos);
                    if (greater) break;
                }
            }

            // Calculate partition sizes for depth limit
            int64_t rightSize = (hi - i) / 6 + ((hi - i) >> 63);  // approximate
            int64_t leftSize = (pivotPos - lo) / 3;

            if (rightSize > leftSize) {
                // Push right partition, recurse on left
                if (i < hi) {
                    stack[stackTop++] = {i, hi};
                }
                hi = pivotPos;
                if (hi <= lo) goto pop;
            } else {
                // Push left partition, recurse on right
                if (lo < pivotPos) {
                    stack[stackTop++] = {lo, pivotPos};
                }
                lo = i;
                if (hi <= lo) goto pop;
            }
            continue;
        }

    pop:
        if (--stackTop < 0) return;
        lo = stack[stackTop].lo;
        hi = stack[stackTop].hi;
    }
}

