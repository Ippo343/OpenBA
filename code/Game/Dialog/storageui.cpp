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
#include "Component/coll_bridge.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "system/String.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/LabelElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/NumberElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TableElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/ElementDescription.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// StorageUI::Startup is the per-frame content refresh (driven via the +0x40
// W_OnUpdate hack in standalone). Its _DEBUG bring-up logs used to fire every
// frame — with fflush to an unbuffered log — which tanked the framerate once a
// second entity panel could stay open (pin). Gate them behind BANISHED_DIALOG_TRACE
// (cached once) so they're opt-in. Kept inside _DEBUG so release builds are unchanged.
#ifdef _DEBUG
#endif

// Helper: find element by name in parent, returns raw pointer
// Original pattern: FindElement → CheckPtr → deref
static void* StorageUI_FindChildElem(void* parentElem, const char* name) {
    uint8_t retBuf[16];
    System::String nameStr(name);
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge
    void* result = *(void**)retBuf;
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
static uint64_t StorageUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    void* raw = StorageUI_FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(raw));   // B.3.3 bridge
    return *(uint64_t*)bpBuf;
}

namespace storageui_sa {

// ENDGAME B.3.9 CG2: TypeId_ComponentEntity -> ComponentEntity::TypeId(),
// TypeId_RawMaterialDescription -> RawMaterialDescription::TypeId(). Both class
// statics always answer, so the lazy-fill retires. (ENDGAME B.3.9 TR:
// fn_FindComponent_Storage reads StorageComponent::TypeId().) B3R: the empty
// EnsureGlobals that was left is gone with its calls.

// All globals StorageUI_Serialize/Startup dereference must be live before we run,
// or a null-deref would fault (MinGW has no __try/__except to catch it).
bool Ready() {
    // ENDGAME B.3.9 TR: the StorageComponent type-id conjunct is gone -- the class
    // static always answers (the cell pointer it tested was never null).
    return ::Object::ExternalFactory::_singleton != nullptr;
}

static void Trace(const char* what, void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    fprintf(stderr, "[StorageUI] %s self=%p ready=%d rows(+0x38)=%d\n",
            what, self, Ready() ? 1 : 0,
            self ? *reinterpret_cast<int*>(s + 0x38) : -1);
    fflush(stderr);
}



}  // namespace storageui_sa

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

// [4] 0x1400b8820
void StorageUI::OnCreated() {
    if (!storageui_sa::Ready() || !SA_DialogEntityAlive(this)) { storageui_sa::Trace("Serialize SKIP", this); return; }
    storageui_sa::Trace("Serialize", this);
    uintptr_t s = (uintptr_t)this;
    void* dialogElem = *(void**)(s + 0x20);

    // Find "storageTable", fallback to "storageTable2"
    void* tableRaw = StorageUI_FindChildElem(dialogElem, "storageTable");
    if (!tableRaw) {
        uint8_t retBuf[16];
        System::String nameBuf("storageTable2");
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameBuf));  // B.3.6 bridge
        tableRaw = *(void**)retBuf;
    }

    // Wrap table as InternalBasePtr
    uint8_t tableBpBuf[8];
    new (tableBpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(tableRaw));   // B.3.3 bridge
    uintptr_t tableElem = *(uintptr_t*)tableBpBuf;

    // Count resource types via ExternalFactory::FindNext
    uint32_t resourceTypeCount = 0;
    void* factory = ::Object::ExternalFactory::_singleton;
    const ::Object::ExternalFactory* efactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    const ::Object::External* ext =
        efactory->FindNext(RawMaterialDescription::TypeId(), nullptr);
    while (ext != nullptr) {
        resourceTypeCount++;
        ext = efactory->FindNext(RawMaterialDescription::TypeId(), ext);
    }

    // Get column count from table element +0xC4
    int colCount = *(int*)(tableElem + 0xC4);

    // Calculate row count: ceil(resourceTypeCount / colCount)
    int rowCount = ((int)(resourceTypeCount - 1 + (uint32_t)colCount) / colCount * colCount) / colCount;

    // Get template factory from first child element at +0xB8 (NOT vtable+0xB8)
    uint8_t childBuf[16];
    ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(0));  // B.3.6 bridge
    void* firstChild = *(void**)childBuf;
    void* templateObj = *(void**)((uintptr_t)firstChild + 0xB8);

    // Ensure minimum row count from table's +0xC0
    int minRows = *(int*)(tableElem + 0xC0);
    if (rowCount < minRows) {
        rowCount = minRows;
    }

    // Resize the table
    reinterpret_cast<UI::TableElement*>(tableElem)->SetSize(rowCount, colCount);  // B.3.6 bridge

    // Clone template elements into each table cell
    int totalCells = reinterpret_cast<const UI::Element*>(tableElem)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < totalCells; i++) {
        // Get dialog resource reference: *(dialogInstance+0x18)
        void* dialogInst = *(void**)(s + 0x18);
        void* dialogResource = *(void**)((uintptr_t)dialogInst + 0x18);

        // Clone via template vtable[8]: `templateObj` is the first child's Element+0xB8
        // `_description` (ExternalPtr<UI::ElementDescription>), so this is ElementDescription
        // slot 8 CreateInstance(ret, dialog) -- a RETSLOT virtual (RULE B6-2).
        uint8_t cloneBuf[16];
        reinterpret_cast<UI::ElementDescription*>(templateObj)->CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(dialogResource));  // B.3.6 bridge: ElementDescription slot 8

        // Set cloned element as child of table
        reinterpret_cast<UI::TableElement*>(tableElem)->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf), i);  // B.3.6 bridge
    }

    // Iterate all cells, find "labelStorage" and "numberCount" sub-elements
    totalCells = reinterpret_cast<const UI::Element*>(tableElem)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < totalCells; i++) {
        // Get child row element — CheckPtr before deref (matches original)
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(i));  // B.3.6 bridge
        void* rowElem = *(void**)childBuf;

        // Find "labelStorage" element
        uint64_t labelStorage = StorageUI_FindElemAsBasePtr(rowElem, "labelStorage");

        // Find "numberCount" element
        uint64_t numberCount = StorageUI_FindElemAsBasePtr(rowElem, "numberCount");

        // Get raw child element ptr
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(i));  // B.3.6 bridge
        uint64_t rawChild = *(uint64_t*)childBuf;

        // Build 0x18-byte entry {labelStorage, numberCount, rawChild} and push to
        // the row array at +0x30.
        // B.3.2 bridge: StorageUI+0x30 is Collection::Array<{labelStorage,
        // numberCount, rawChild}> (stride 0x18, count +0x38, capacity +0x3C).
        auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0x30);
        rows.SetSize(rows.GetSize() + 1,
                     GameColl::Bytes<0x18, 8>(labelStorage, numberCount, rawChild));
    }

    // Slot 7 (vtable +0x38), virtual as retail
    OnUpdateLayout();
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

// [7] 0x1400b8450
void StorageUI::OnUpdateLayout() {
    if (!storageui_sa::Ready() || !SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    // Get entity, check ComponentEntity
    void* entityRaw = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entityRaw, ComponentEntity::TypeId());
    void* entity = isEntity ? entityRaw : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find StorageComponent (type ID 0x1401c7e78) — NOT StateMachine!
    // Original calls FUN_140006270 which searches by StorageComponent type ID.
    uint64_t storageComp = 0;
    fn_FindComponent_Storage((void*)entityPtr, &storageComp);

#ifdef _DEBUG
#endif

    // Build temp array of storage items
    void* sortedItems = nullptr;
    void* currentItems = nullptr;
    int itemCount = 0;
    int itemCapacity = 0;

    int smEntryCount = storageComp ? *(int*)(storageComp + 0x30) : 0;
#ifdef _DEBUG
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
#endif
                for (int c = 0; c < compCount; c++) {
                    void* comp = *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8);
                    int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
                    if (typeId == (int)RawMaterialDescription::TypeId()) {
                        if (::Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) {
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

                // Push into temp array.
                // B.3.2 SKIPPED: this array's header is not a contiguous struct
                // field (unlike +0x30 above) but four independent locals
                // (sortedItems/currentItems/itemCount/itemCapacity), and its
                // lifetime is threaded through System::MemoryAllocator::MemFree(currentItems) far
                // below (StorageUI_Startup's tail) plus a read/sort call in
                // between. Wrapping it in Collection::Array<T> would need a
                // restructure (a local ArrayHdr-shaped object whose destructor
                // would race the existing explicit free) rather than a targeted
                // per-site edit, so it is left as the hand-rolled growth.
                int newCount = itemCount + 1;
                if (newCount != itemCount) {
                    if (itemCapacity < newCount) {
                        itemCapacity = newCount;
                        if (itemCount != 0) {
                            itemCapacity = newCount * 3;
                        }
                        void* newBuf = System::MemoryAllocator::MemAlloc((uint64_t)itemCapacity * 0x18, 8);
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
                        System::MemoryAllocator::MemFree(currentItems);
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
#endif

            // Set sprite from resource type
            uint8_t spriteBuf[16];
            ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(resType + 0x20))),
                           *reinterpret_cast<const System::String*>(resType + 0x30));  // B.3.6 bridge
            void* labelElem = *(void**)rowBase;
            reinterpret_cast<UI::LabelElement*>(labelElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge

            // Set text from resource type
            uint8_t uiStrBuf[16];
            ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(resType + 0x28))),
                             *reinterpret_cast<const System::String*>(resType + 0x30));  // B.3.6 bridge
            labelElem = *(void**)rowBase;
            reinterpret_cast<UI::TextElement*>(labelElem)->SetText(reinterpret_cast<const UI::String*>(uiStrBuf));  // B.3.6 bridge

            // Set count
            void* numberElem = *(void**)(rowBase + 8);
            reinterpret_cast<UI::NumberElement*>(numberElem)->SetValue(static_cast<int32_t>(count));  // B.3.6 bridge

            // Show both elements (flags: hide=0, show=1)
            reinterpret_cast<UI::Element*>(*(void**)rowBase)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
            reinterpret_cast<UI::Element*>(*(void**)(rowBase + 8))->SetFlagsOnOff(0, 1);  // B.3.6 bridge

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
            reinterpret_cast<UI::Element*>(*(void**)rowBase)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
            reinterpret_cast<UI::Element*>(*(void**)(rowBase + 8))->SetFlagsOnOff(1, 0);  // B.3.6 bridge

            hideOffset += 0x18;
        }
    }

    // Free temp sorted items array
    System::MemoryAllocator::MemFree(currentItems);
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
    ::new (uiStrA) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(resA + 0x28))), *reinterpret_cast<const System::String*>(resA + 0x30));  // B.3.6 bridge
    System::String tempA;
    ::new (&tempA) System::String(reinterpret_cast<const UI::String*>(uiStrA)->operator System::String());  // B.3.6 bridge; UIString → System::String conversion
    System::String sysStrA(tempA);

    // Build UIString -> convert to System::String for item B
    uint8_t uiStrB[16];
    ::new (uiStrB) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(resB + 0x28))), *reinterpret_cast<const System::String*>(resB + 0x30));  // B.3.6 bridge
    System::String tempB;
    ::new (&tempB) System::String(reinterpret_cast<const UI::String*>(uiStrB)->operator System::String());  // B.3.6 bridge; UIString → System::String conversion
    System::String sysStrB(tempB);

    return sysStrA < sysStrB;
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

// ============================================================
// STANDALONE promotion — real StorageUI controller vtable
// ============================================================
// The citizen (and building) info panel's Inventory/Storage tab is driven by a
// StorageUI controller bound to the panel's `storageTable`. In standalone this
// type was a StubNop (all slots no-op, see ui_dialog_stub_factories.cpp — that
// stub registration is REMOVED there to avoid a duplicate REGISTER_TYPE), so the
// tab never built or populated its rows and stayed on the .rsc template cell that
// renders "0". This block installs a real per-class vtable wiring the two
// reimplemented methods above (StorageUI_Serialize builds+sizes the table and
// caches the row element ptrs; StorageUI_Startup fills the rows from the entity's
// StorageComponent), following the CitizenUI/EntityUI promotion pattern.
//
// Vtable offsets (verified from the exe StorageUI vtable @ 0x14015f970):
//   +0x00 (idx0) ScalarDtor
//   +0x10 (idx2) GetTypeId
//   +0x20 (idx4) Serialize = table build (setup; framework calls once). It calls
//                Startup (via vtable[7]) at its end to do the initial populate.
//   +0x38 (idx7) Startup   = populate rows from the entity's carried inventory.
//   +0x40 (idx8)           = base DialogController::OnUpdate (per-frame; the
//                framework DialogInstance::OnUpdate dispatches ONLY +0x40 each
//                frame, Runtime 0x180115400). Base is a content no-op, so we drive
//                the refresh here: build once (if the framework's idx4 setup didn't
//                run yet), then re-populate every frame so the counts stay live.
//
// Every exe-.rdata type-id these methods dereference is filled from the live
// factory tree (RawMaterialDescription/StorageComponent are already registered in
// standalone; ComponentEntity is lazy-resolved here). W_* skip (not fault) if a
// critical id or the ExternalFactory singleton has not resolved yet.


// ~StorageUI: the tree's own teardown (its hand-built slot 0 ran this, then freed
// WITHOUT ~DialogController); moved here unchanged. The compiler now chains
// ~DialogController -> ~Internal after it.
StorageUI::~StorageUI() {
    void* self = this;
    // Free the cached row-element array at +0x30 (mirrors the exe ScalarDtor).
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* rows = *reinterpret_cast<void**>(s + 0x30);
    if (rows) System::MemoryAllocator::MemFree(rows);
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
}



// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void StorageUI::OnUpdate(const System::Time& /*time*/) {
    if (!storageui_sa::Ready() || !SA_DialogEntityAlive(this)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<int*>(s + 0x38) == 0) {
        StorageUI::OnCreated();
    } else {
        StorageUI::OnUpdateLayout();
    }
}

// create 0x140021330: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* StorageUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* StorageUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageUI());
}

DEFINE_EXE_TYPE(StorageUI, "UI::DialogController")

