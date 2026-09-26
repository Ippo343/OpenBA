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
#include "system/String.h"
#include "system/Filename.h"
#include "Component/coll_bridge.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)

// ============================================================
// File-local helper: CalcWorkerSlots (FUN_14012e5c0)
// ============================================================
// Given a StorageLocationComponent (param_1):
//   - Checks that param_1+0x18 is a WorkplaceJobType (DAT_1401c80a8)
//   - If no attendance override (pBVar3[0x34]==0): return pBVar3[0x24] (base max workers)
//   - Otherwise: look up BuildComponent on the entity at param_1+0x10,
//     compute scaled slots = floor(buildComp.occupants * buildComp.maxOccupants / jobType.totalSlots
//                                   * maxWorkers + 1.0f), clamped to [1, maxWorkers].
//
// StorageLocationComponent (param_1):
//   +0x10: InternalBasePtr -> entity
//   +0x18: InternalBasePtr -> WorkplaceJobType object
//
// WorkplaceJobType (pBVar3) fields used:
//   +0x24: int maxWorkers
//   +0x30: int totalSlots
//   +0x34: byte hasAttendance (0 = no, else yes)
//
// BuildComponent used for attendance scaling:
//   +0x2c: int16 maxOccupants
//   +0x2e: int16 currentOccupants
//
// Original: FUN_14012e5c0.
//
static int CalcWorkerSlots_Impl(uintptr_t storageLocComp) {
    // Get WorkplaceJobType object from +0x18 (InternalBasePtr)
    void* jobType = *(void**)(storageLocComp + 0x18);
    if (!jobType) return 0;

    // IsDerivedFrom check
    bool isJobType = fn_IsDerivedFrom(jobType, *g_TypeId_WorkDescription);
    if (!isJobType) return 0;

    uintptr_t jt = (uintptr_t)jobType;
    int maxWorkers = *(int*)(jt + 0x24);

    // If no attendance scaling, just return base max
    if (*(uint8_t*)(jt + 0x34) == 0) {
        return maxWorkers;
    }

    // Find BuildComponent on the entity at +0x10
    uintptr_t entity = *(uintptr_t*)(storageLocComp + 0x10);

    uint64_t buildCompRet = 0;
    fn_FindComponent_Build((void*)entity, &buildCompRet);
    uintptr_t buildComp = buildCompRet;

    if (!buildComp) return maxWorkers > 0 ? maxWorkers : 1;

    // Scale: (currentOccupants * maxOccupants / totalSlots) * maxWorkers + 1.0f
    int16_t maxOcc     = *(int16_t*)(buildComp + 0x2c);
    int16_t curOcc     = *(int16_t*)(buildComp + 0x2e);
    int     totalSlots = *(int*)(jt + 0x30);
    // DAT_14017214c = 1.0f
    int result = (int)(((float)(curOcc * maxOcc) / (float)totalSlots) * (float)maxWorkers + 1.0f);
    if (result > maxWorkers) result = maxWorkers;
    if (result < 1)          result = 1;
    return result;
}

// ============================================================
// File-local helpers: AVL BST rotations + rebalance
// ============================================================
// BST node layout (used by the profession BST at self+0x60):
//   +0x00: void* key  (ExternalPtr to profession description)
//   +0x08: int   index
//   +0x10: void* parent
//   +0x18: void* left (field called "left" / "18" in Ghidra)
//   +0x20: void* right (field called "right" / "20" in Ghidra)
//   +0x28: int16 balance  (right_height - left_height)
//   +0x2a: int16 height   (max(left_h, right_h) + 1, or 0 if null)
//
// FUN_14000e580: promotes node's +0x18 child to this node's position (right-rotation)
// FUN_14000e450: promotes node's +0x20 child to this node's position (left-rotation)

static void BST_UpdateNode(uintptr_t n) {
    // Recompute balance and height from children.
    int16_t lh = 0, rh = 0;
    uintptr_t lc = *(uintptr_t*)(n + 0x18);
    uintptr_t rc = *(uintptr_t*)(n + 0x20);
    if (lc) lh = (int16_t)(*(int16_t*)(lc + 0x2a) + 1);
    if (rc) rh = (int16_t)(*(int16_t*)(rc + 0x2a) + 1);
    *(int16_t*)(n + 0x28) = (int16_t)(rh - lh);
    *(int16_t*)(n + 0x2a) = (rh > lh) ? rh : lh;
}

// FUN_14000e580 — promotes node->left (+0x18) child (right-rotation at node)
static uintptr_t BST_RotateRight_e580(uintptr_t* rootPtr, uintptr_t node) {
    uintptr_t parent = *(uintptr_t*)(node + 0x10);
    uintptr_t pivot  = *(uintptr_t*)(node + 0x18);  // left child becomes new root

    if (node == *rootPtr) *rootPtr = pivot;
    if (parent) {
        if (*(uintptr_t*)(parent + 0x18) == node) *(uintptr_t*)(parent + 0x18) = pivot;
        else                                        *(uintptr_t*)(parent + 0x20) = pivot;
    }
    *(uintptr_t*)(pivot + 0x10) = parent;

    // node.left = pivot.right; if pivot.right, reparent it
    uintptr_t pivotRight = *(uintptr_t*)(pivot + 0x20);
    *(uintptr_t*)(node + 0x18) = pivotRight;
    if (pivotRight) *(uintptr_t*)(pivotRight + 0x10) = node;

    *(uintptr_t*)(pivot + 0x20) = node;
    *(uintptr_t*)(node  + 0x10) = pivot;

    // Update heights in correct order
    uintptr_t pr = *(uintptr_t*)(pivot + 0x18);
    if (pr) BST_UpdateNode(pr);
    uintptr_t pr2 = *(uintptr_t*)(pivot + 0x20);
    if (pr2) BST_UpdateNode(pr2);
    BST_UpdateNode(pivot);
    return pivot;
}

// FUN_14000e450 — promotes node->right (+0x20) child (left-rotation at node)
static uintptr_t BST_RotateLeft_e450(uintptr_t* rootPtr, uintptr_t node) {
    uintptr_t parent = *(uintptr_t*)(node + 0x10);
    uintptr_t pivot  = *(uintptr_t*)(node + 0x20);  // right child becomes new root

    if (node == *rootPtr) *rootPtr = pivot;
    if (parent) {
        if (*(uintptr_t*)(parent + 0x18) == node) *(uintptr_t*)(parent + 0x18) = pivot;
        else                                        *(uintptr_t*)(parent + 0x20) = pivot;
    }
    *(uintptr_t*)(pivot + 0x10) = parent;

    // node.right = pivot.left; if pivot.left, reparent it
    uintptr_t pivotLeft = *(uintptr_t*)(pivot + 0x18);
    *(uintptr_t*)(node + 0x20) = pivotLeft;
    if (pivotLeft) *(uintptr_t*)(pivotLeft + 0x10) = node;

    *(uintptr_t*)(pivot + 0x18) = node;
    *(uintptr_t*)(node  + 0x10) = pivot;

    uintptr_t pl = *(uintptr_t*)(pivot + 0x18);
    if (pl) BST_UpdateNode(pl);
    uintptr_t pl2 = *(uintptr_t*)(pivot + 0x20);
    if (pl2) BST_UpdateNode(pl2);
    BST_UpdateNode(pivot);
    return pivot;
}

// BSTRebalance_Impl (FUN_140013480): Walk up from startNode, updating heights/balance
// and performing AVL rotations as needed.
// treeRootPtr: pointer to the BST root field (e.g. &self->bstRoot)
// startNodePtr: pointer to the node to begin from (advances to parent each iteration)
static void BSTRebalance_Impl(uintptr_t* treeRootPtr, uintptr_t* startNodePtr) {
    uintptr_t cur = *startNodePtr;
    while (cur != 0) {
        uintptr_t curNode = *startNodePtr;
        uintptr_t nextParent = *(uintptr_t*)(curNode + 0x10);

        // Recompute balance/height
        int16_t lh = 0, rh = 0;
        uintptr_t lc = *(uintptr_t*)(curNode + 0x18);
        uintptr_t rc = *(uintptr_t*)(curNode + 0x20);
        if (lc) lh = (int16_t)(*(int16_t*)(lc + 0x2a) + 1);
        if (rc) rh = (int16_t)(*(int16_t*)(rc + 0x2a) + 1);
        *(int16_t*)(curNode + 0x28) = (int16_t)(rh - lh);
        *(int16_t*)(curNode + 0x2a) = (rh > lh) ? rh : lh;

        // Right-heavy (balance > 1): need left-rotation
        uintptr_t pivot = *(uintptr_t*)(curNode + 0x20);
        if (*(int16_t*)(curNode + 0x28) > 1 && pivot != 0) {
            // If right child is left-heavy (balance < 0): double rotation (right-left)
            if (*(int16_t*)(pivot + 0x28) < 1 && *(int16_t*)(pivot + 0x28) < 0) {
                BST_RotateRight_e580(treeRootPtr, pivot);
                curNode = *startNodePtr;
            }
            BST_RotateLeft_e450(treeRootPtr, curNode);
        }

        curNode = *startNodePtr;
        // Left-heavy (balance < -1): need right-rotation
        pivot = *(uintptr_t*)(curNode + 0x18);
        if (*(int16_t*)(curNode + 0x28) < -1 && pivot != 0) {
            // If left child is right-heavy (balance > 0): double rotation (left-right)
            if (*(int16_t*)(pivot + 0x28) > -1 && *(int16_t*)(pivot + 0x28) > 0) {
                BST_RotateLeft_e450(treeRootPtr, pivot);
                curNode = *startNodePtr;
            }
            BST_RotateRight_e580(treeRootPtr, curNode);
        }

        *startNodePtr = nextParent;
        cur = nextParent;
    }
}

// ============================================================
// Dynamic array push helpers (8-byte element arrays)
// ============================================================
// B.3.2 bridge: `self+ptrOffset` is Collection::Array<uint64_t> (stride 8,
// count +0x08, capacity +0x0C off ptrOffset) inside a struct §B.2 has not
// converted, so it is reached at its offset.
static void PushToArray8(uintptr_t self, int ptrOffset, uint64_t value) {
    auto& arr = GameColl::ArrayAt<GameColl::Bytes<8, 8>>(self, ptrOffset);
    arr.SetSize(arr.GetSize() + 1, GameColl::Bytes<8, 8>(value));
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
    System::String nameStr("professionTable");
    uint64_t tableResult = 0;
    ::new (&tableResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge

    void* tableElem = (void*)tableResult;

    // Get column count from table element at +0xC4
    int colCount = *(int*)((uintptr_t)tableElem + 0xC4);

    // Get profession count from game world singleton
    void* worldSingleton = *g_pGameWorldSingleton;
    void* profListPtr = *(void**)((uintptr_t)worldSingleton + 0xB0);
    int profCount = *(int*)((uintptr_t)profListPtr + 0x28);


    // Get template factory: first child of table -> element+0xB8
    uint64_t firstChildBuf = 0;
    ::new (&firstChildBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(0));  // B.3.6 bridge
    void* firstChild = (void*)firstChildBuf;
    // Factory is at element+0xB8 (NOT vtable+0xB8)
    void** factory = *(void***)((uintptr_t)firstChild + 0xB8);

    // Set table size: ceil(profCount / colCount) rows
    int rowCount = ((colCount - 1 + profCount) / colCount * colCount) / colCount;
    fn_TableElement_SetSize(tableElem, rowCount, colCount);


    // Phase 1: Populate table cells from template factory
    int col = 0;
    int row = 0;

    for (int i = 0; i < profCount; i++) {
        // Template data from dataSource (self+0x18) -> +0x18
        uintptr_t templateData = *(uintptr_t*)(*(uintptr_t*)(s + 0x18) + 0x18);

        // Clone template via factory vtable slot 8: factory->vt[8](factory, retval, templateData)
        typedef uint64_t (*fn_FactoryClone_t)(void* factoryThis, void* retval, uintptr_t tmplData);
        uint64_t cloneBuf = 0;
        uintptr_t factoryFnAddr = *(uintptr_t*)(*(uintptr_t*)factory + 0x40);
        uint64_t cloned = ((fn_FactoryClone_t)factoryFnAddr)(
            factory, &cloneBuf, templateData);

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
        // Re-read the flat-index stride from the table each iteration.
        // The original (0x140076eb4) does `IMUL EBX,R13D` with EBX = *(table+0xC4)
        // then `ADD EBX,R12D`, i.e. flatIdx = (+0xC4) * fastIdx + slowIdx, where the
        // fast index (tableCol here) wraps at +0xC0. +0xC4 is the column stride (=2),
        // +0xC0 is the row count (=10). Using +0xC0 as the stride and multiplying the
        // SLOW index instead collapses the intended COLUMN-MAJOR layout (builder below
        // laborer, as retail draws it) into row-major (builder to the right). Match the
        // original exactly: stride = +0xC4, flatIdx = stride*tableCol + tableRow.
        int stride = *(int*)((uintptr_t)tableResult + 0xC4);

        int flatIdx = stride * tableCol + tableRow;
        uint64_t cellElem = 0;
        ::new (&cellElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableResult)->Child(flatIdx));  // B.3.6 bridge

        // Get 4 children of cell (indices from disassembly: 0, 1, 3, 4):
        //   child[0] = text/label element (name + sprite)
        //   child[1] = spinner element (worker count)
        //   child[3] = number element (actual count)
        //   child[4] = button element (goto)
        uint64_t textLabelBuf = 0;
        ::new (&textLabelBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(cellElem)->Child(0));  // B.3.6 bridge

        uint64_t spinnerBuf = 0;
        ::new (&spinnerBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(cellElem)->Child(1));  // B.3.6 bridge

        uint64_t numberBuf = 0;
        ::new (&numberBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(cellElem)->Child(3));  // B.3.6 bridge

        uint64_t buttonBuf = 0;
        ::new (&buttonBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(cellElem)->Child(4));  // B.3.6 bridge

        // Set button element ID: idBase + 200
        reinterpret_cast<UI::Element*>(buttonBuf)->SetElementId(idBase + 200);  // B.3.6 bridge

        // Get profession ExternalPtr from array
        void* profDesc = *(void**)((uintptr_t)profArray + (uintptr_t)i * 8);

        // Set element name from profession file path
        {
            // `GetPath` returns by value through a hidden return slot and the callee
            // CONSTRUCTS into it; copy-initialising the local is the same store.
            System::Filename pathObj =
                reinterpret_cast<const ::Object::External*>(profDesc)->GetPath();  // B.3.3 bridge
            System::Filename identObj = pathObj.GetPathIdentifer();
            reinterpret_cast<UI::Element*>(spinnerBuf)->SetName(*reinterpret_cast<const System::String*>(&identObj));  // B.3.6 bridge
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
                    uintptr_t nodePtr = (uintptr_t)newNode;
                    BSTRebalance_Impl((uintptr_t*)(s + 0x60), &nodePtr);
                }
            }
        }
        *(int*)(s + 0x68) = *(int*)(s + 0x68) + 1;

        // Set text, sprite, tooltip on text/label element from profession description
        uint8_t uiStrBuf[16];
        ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)profDesc + 0x20))),
                         *reinterpret_cast<const System::String*>((uintptr_t)profDesc + 0x30));  // B.3.6 bridge
        fn_TextElement_SetText((void*)textLabelBuf, uiStrBuf);

        uint8_t spriteBuf[16];
        ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)profDesc + 0x28))),
                        *reinterpret_cast<const System::String*>((uintptr_t)profDesc + 0x30));  // B.3.6 bridge
        fn_LabelElement_SetSprite((void*)textLabelBuf, spriteBuf);

        // Tooltip: UI::String from ExternalPtr + string name at +0x38
        uint8_t tooltipBuf[16];
        ::new (tooltipBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)profDesc + 0x20))),
                         *reinterpret_cast<const System::String*>((uintptr_t)profDesc + 0x38));  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(textLabelBuf)->SetToolTip(*reinterpret_cast<const UI::String*>(tooltipBuf));  // B.3.6 bridge

        // Set user data and element ID on spinner
        // Original only sets user data + child element ID, but the event system
        // sends the spinner's own element ID as param4 in msg 0x15 notifications.
        // Without SetElementId on the spinner itself, param4 is -1 and Slot9 never
        // dispatches to HandleSpinner.
        reinterpret_cast<UI::Element*>(spinnerBuf)->SetUserData(reinterpret_cast<Object::Base*>((uintptr_t)idBase));  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(spinnerBuf)->SetElementId(idBase);  // B.3.6 bridge

        // Get child[0] of spinner: set its element ID (original does this too)
        uint64_t spinnerChild0 = 0;
        ::new (&spinnerChild0) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(spinnerBuf)->Child(0));  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(spinnerChild0)->SetElementId(idBase);  // B.3.6 bridge

        // Get child[0] again, verify NumberEditElement, wrap in IBP for array push
        uint64_t numEditBuf = 0;
        ::new (&numEditBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(spinnerBuf)->Child(0));  // B.3.6 bridge
        void* numEditRaw = (void*)numEditBuf;
        void* numEditChecked = nullptr;
        if (fn_IsDerivedFrom && g_TypeId_NumberEditElement) {
            if (fn_IsDerivedFrom(numEditRaw, *g_TypeId_NumberEditElement))
                numEditChecked = numEditRaw;
        } else {
            numEditChecked = numEditRaw;
        }
        uint64_t numEditIBP = 0;
        new (&numEditIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(numEditChecked));   // B.3.3 bridge

        // Push spinner's NumberEditElement to spinner array at +0x40
        PushToArray8(s, 0x40, numEditIBP);

        // Push number element to number array at +0x50
        PushToArray8(s, 0x50, (uint64_t)numberBuf);

        // If this profession is the transient "default/laborer" profession (profList+0x30,
        // _default="laborer" in Profession.rsc), hide the two worker-count elements that
        // are meaningless for the unassigned-laborer pool: child[3] "numberAvailable" and
        // child[2] "ofText" (the "N of M" phrase). The spinner (locked in Slot8) and the
        // child[4] "buttonGoto" (cycle-to-a-laborer) REMAIN visible — retail keeps them.
        // NB: retail hides child index 2 here (verified in the 0x140076CA0 disassembly:
        // `MOV R8D,0x2` before the Child call), NOT index 4 — hiding 4 wrongly removed the
        // laborer's goto button.
        void* noneProfession = *(void**)((uintptr_t)profListPtr + 0x30);
        if (profDesc == noneProfession) {
            reinterpret_cast<UI::Element*>(numberBuf)->SetFlagsOnOff(1, 0);   // B.3.6 bridge; child[3] numberAvailable

            uint64_t ofTextChild = 0;
            ::new (&ofTextChild) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(cellElem)->Child(2)); // B.3.6 bridge; child[2] ofText
            reinterpret_cast<UI::Element*>(ofTextChild)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        }

        // Advance table position
        tableCol++;
        if (tableCol >= *(int*)((uintptr_t)tableResult + 0xC0)) {
            tableRow++;
            tableCol = 0;
        }

        idBase++;
    }

    // Resize counts array to profCount.
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400021d0, stride 4 align 4,
    // grown tail left as the allocation found it (no init arm). Slot8 zeroes it below.
    GameColl::ArrayAt<GameColl::I32NoInit>(s, 0x30).SetSize(profCount);

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
        // CheckPtr on citizen IBP (original does this)
        uintptr_t citizen = *(uintptr_t*)(citizenArray + (uintptr_t)i * 8);

        // Check if citizen is adult: age (float at +0x3C) >= birthAge (float at desc+0x44)
        uintptr_t citizenDesc = *(uintptr_t*)(citizen + 0x288);
        float birthAge = *(float*)(citizenDesc + 0x44);
        float currentAge = *(float*)(citizen + 0x3C);
        if (currentAge < birthAge) continue;

        // Check if citizen is not dead: deathPtr at +0x208 must be 0
        if (*(uintptr_t*)(citizen + 0x208) != 0) continue;

        // CheckPtr again before reading profession (original does double CheckPtr)
        citizen = *(uintptr_t*)(citizenArray + (uintptr_t)i * 8);

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
    void** firstSpinnerPtr = (void**)(*(uintptr_t*)(s + 0x40));
    fn_NumberEditElement_SetValue(*firstSpinnerPtr, counts[0]);
    firstSpinnerPtr = (void**)(*(uintptr_t*)(s + 0x40));
    fn_NumberEditElement_SetMinMax(*firstSpinnerPtr, counts[0], counts[0]);

    // Update remaining spinners
    int spinnerCount = *(int*)(s + 0x48);
    for (int i = 1; i < spinnerCount; i++) {
        void** spinnerPtr = (void**)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8);
        fn_NumberEditElement_SetValue(*spinnerPtr, counts[i]);
        spinnerPtr = (void**)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8);
        // Min=0, Max=counts[i]+counts[0] (can reassign from "None" profession)
        fn_NumberEditElement_SetMinMax(*spinnerPtr, 0, counts[i] + counts[0]);
    }

    // Update number elements with actual worker counts per profession
    uintptr_t worldProfList = *(uintptr_t*)((uintptr_t)worldSingleton + 0xB0);
    void* profArrayPtr = *(void**)(worldProfList + 0x20);

    for (int p = 0; p < profCount; p++) {
        uintptr_t profKey = *(uintptr_t*)((uintptr_t)profArrayPtr + (uintptr_t)p * 8);

        // Look up profession in world BST at citizenMgr+0x100 to get entity count
        uint8_t* bstNode = *(uint8_t**)(citizenMgr + 0x100);
        int entityCount = 0;

        while (bstNode) {
            uintptr_t nodeKey = *(uintptr_t*)(bstNode + 0x00);
            if (profKey < nodeKey) {
                bstNode = *(uint8_t**)(bstNode + 0x20);
            } else if (profKey > nodeKey) {
                bstNode = *(uint8_t**)(bstNode + 0x28);
            } else {
                entityCount = *(int*)(bstNode + 0x10);
                break;
            }
        }

        int totalWorkers = 0;
        for (int e = 0; e < entityCount; e++) {
            // Re-search BST each iteration (original does this to get fresh entity list)
            uint8_t* bstNode2 = *(uint8_t**)(citizenMgr + 0x100);
            uintptr_t freshEntityList = 0;
            while (bstNode2) {
                uintptr_t nk = *(uintptr_t*)(bstNode2 + 0x00);
                if (profKey < nk) { bstNode2 = *(uint8_t**)(bstNode2 + 0x20); }
                else if (profKey > nk) { bstNode2 = *(uint8_t**)(bstNode2 + 0x28); }
                else { freshEntityList = *(uintptr_t*)(bstNode2 + 0x08); break; }
            }
            if (!freshEntityList) continue;

            // CheckPtr on entity IBP (original does this)
            void** entityIBP = (void**)(freshEntityList + (uintptr_t)e * 8);
            uintptr_t entityBP = *(uintptr_t*)entityIBP;
            if (!entityBP) continue;

            // Find WorkplaceComponent on entity
            int compCount = *(int*)(entityBP + 0x38);
            uintptr_t compArray = *(uintptr_t*)(entityBP + 0x30);
            uintptr_t workplaceComp = 0;

            for (int c = 0; c < compCount; c++) {
                void** compIBP = (void**)(compArray + (uintptr_t)c * 8);
                uintptr_t comp = *(uintptr_t*)compIBP;
                if (!comp) continue;
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
                // Wrap in IBP and CheckPtr (original does InternalBasePtr_Ctor + CheckPtr)
                uint64_t wpcIBP = 0;
                new (&wpcIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>((void*)workplaceComp));   // B.3.3 bridge
                workplaceComp = (uintptr_t)wpcIBP;

                // Check if workplace is enabled (+0x50 != 0)
                if (workplaceComp && *(uint8_t*)(workplaceComp + 0x50) != 0) {
                    // Find StorageLocationComponent on parent entity via +0x10
                    uintptr_t parentEntity = *(uintptr_t*)(workplaceComp + 0x10);
                    if (!parentEntity) continue;
                    int pCompCount = *(int*)(parentEntity + 0x38);
                    uintptr_t pCompArray = *(uintptr_t*)(parentEntity + 0x30);
                    uintptr_t storageLocComp = 0;

                    for (int c = 0; c < pCompCount; c++) {
                        void** compIBP = (void**)(pCompArray + (uintptr_t)c * 8);
                        uintptr_t comp = *(uintptr_t*)compIBP;
                        if (!comp) continue;
                        typedef int (*fn_GetTypeId_t)(void*);
                        void** vtbl = *(void***)(comp);
                        int typeId = ((fn_GetTypeId_t)vtbl[2])((void*)comp);
                        if (typeId == (int)*g_TypeId_WorkComponent) {
                            if (fn_IsDerivedFrom((void*)comp, *g_TypeId_WorkComponent)) {
                                storageLocComp = comp;
                            }
                            break;
                        }
                    }

                    if (storageLocComp) {
                        // Wrap in IBP and CheckPtr (original does InternalBasePtr_Ctor + CheckPtr)
                        uint64_t slcIBP = 0;
                        new (&slcIBP) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>((void*)storageLocComp));   // B.3.3 bridge
                        storageLocComp = (uintptr_t)slcIBP;

                        if (storageLocComp) {
                            int workerSlots = *(int*)(storageLocComp + 0x30);
                            if (workerSlots == 0) {
                                workerSlots = CalcWorkerSlots_Impl(storageLocComp);
                                *(int*)(storageLocComp + 0x30) = workerSlots;
                            }
                            totalWorkers += workerSlots;
                        }
                    }
                }
            }
        }

        // Set number element value (with CheckPtr, matching original)
        void** numElemPtr = (void**)(*(uintptr_t*)(s + 0x50) + (uintptr_t)p * 8);
        fn_NumberElement_SetValueInt(*numElemPtr, totalWorkers);
    }
}

// ============================================================
// ProfessionUI_HandleSpinner (was FUN_1400763c0)
// ============================================================
// Handles spinner value change for profession assignment.
// When user adjusts the spinner for a profession:
//   - If new value > current count: add citizens to that profession
//     (tries idle -> working -> student -> any fallback)
//   - If new value < current count: remove citizens from that profession
// Then refreshes the display via Slot8 (vtable +0x40).
//
// Original: 0x1400763c0.

static void ProfessionUI_HandleSpinner(ProfessionUI* self, int profIndex) {
    // NO g_wpcRosterOnly guard (removed 2026-08-17). It used to be held here because the
    // Attach/Detach visual block "faults on inert standalone objects (READ of 0x10 in
    // Detach's e710 getter when the player lowered a builder count)". That exact fault is
    // now guarded at the source — workplacecomponent.cpp:276 returns early on a null
    // accessor — so the premise expired and the guard outlived it. Holding it here was not
    // free: the same flag gates Attach's clear of the "no workers" status bit, so every
    // seat made through this spinner left the ? icon up over a fully-staffed building.
    // g_wpcRosterOnly is a world-BUILD shim (see its definition); a spinner click is
    // gameplay time, which is exactly when the visual is meant to run. See the block above
    // WorkPlaceUI idx9 in workplaceui.cpp for the full measurement.
    uintptr_t s = (uintptr_t)self;
    int64_t idx = (int64_t)profIndex;

    // Get spinner element at this index and read desired value
    uintptr_t spinnerArray = *(uintptr_t*)(s + 0x40);
    void* spinnerElem = *(void**)(spinnerArray + idx * 8);
    int desired = fn_NumberEditElement_GetValue(spinnerElem);

    // Get current count from counts array
    int* counts = *(int**)(s + 0x30);
    int current = counts[profIndex];


    // Add citizens if desired > current
    if (current < desired) {
        uint64_t toAdd = (uint64_t)(uint32_t)(desired - current);
        do {
            void* citizenList = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xF0);
            // Triple deref: world+0xB0 → profList, +0x20 → profArray, +idx*8 → profId
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xB0) + 0x20) + idx * 8);

            // Request a worker slot
            uint64_t retBuf[2] = {0, 0};
            fn_CitizenList_RequestWorker(citizenList, retBuf, profId);
            uint64_t slot = retBuf[0];

            // Try to find a citizen: idle -> working -> student -> any
            uint64_t slotCopy = slot;
            uint8_t findBuf[8] = {0};
            void* result = fn_CitizenList_FindIdle(citizenList, findBuf, &slotCopy, profId);
            int64_t found = *(int64_t*)result;

            if (found == 0) {
                if (slot != 0) {
                    slotCopy = slot;
                    uint8_t fb2[8] = {0};
                    result = fn_CitizenList_FindWorking(citizenList, fb2, &slotCopy, profId);
                    found = *(int64_t*)result;
                    if (found != 0) goto next_iter;
                }
                slotCopy = slot;
                uint8_t fb3[8] = {0};
                result = fn_CitizenList_FindStudent(citizenList, fb3, &slotCopy, profId);
                found = *(int64_t*)result;

                if (found == 0) {
                    slotCopy = slot;
                    uint64_t anyBuf = 0;
                    fn_CitizenList_FindAny(citizenList, &anyBuf, &slotCopy, profId);
                    found = (int64_t)anyBuf;
                }
            }
next_iter:
            toAdd--;
        } while (toAdd != 0);
    }

    // Remove citizens if desired < current
    if (desired < current) {
        uint64_t toRemove = (uint64_t)(uint32_t)(current - desired);
        do {
            void* citizenList = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xF0);
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xB0) + 0x20) + idx * 8);
            fn_CitizenList_RemoveWorker(citizenList, profId);
            toRemove--;
        } while (toRemove != 0);
    }

    // Refresh display: call Slot8 (vtable +0x40) with Time(0.0)
    uint8_t timeBuf[8];
    fn_Time_Ctor_Float(timeBuf, 0.0f);
    typedef void (*fn_VSlot8_t)(void* self, void* time);
    void** vtbl = *(void***)self;
    ((fn_VSlot8_t)vtbl[8])(self, timeBuf);
}

// ============================================================
// ProfessionUI_HandleButton (was FUN_140076a30)
// ============================================================
// Handles button click to find and center camera on a citizen of
// the given profession. Cycles through eligible citizens using
// a rotation index (self+0x70) that wraps around.
//
// Only considers adult, alive citizens (age >= birthAge, death ptr == 0).
// Calls Toolbar_CenterOnEntity_Impl to snap the camera and select
// the citizen in the EditTool.
//
// Original: 0x140076a30.

static void ProfessionUI_HandleButton(ProfessionUI* self, int profIndex) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    void* citizenList = *(void**)(game + 0xF0);
    uintptr_t citizenMgr = (uintptr_t)citizenList;
    int citizenCount = *(int*)(citizenMgr + 0xA8);
    uintptr_t citizenArray = *(uintptr_t*)(citizenMgr + 0xA0);

    // Get the profession ID for this index
    // Triple deref: world+0xB0 → profList, +0x20 → profArray, +idx*8 → profId
    uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)(game + 0xB0) + 0x20) + (int64_t)profIndex * 8);

    // Count eligible (adult, alive) citizens
    int eligible = 0;
    for (int i = 0; i < citizenCount; i++) {
        uintptr_t citizen = *(uintptr_t*)(citizenArray + (uintptr_t)i * 8);

        // Check adult: age >= birthAge (desc+0x44)
        uintptr_t citizenDesc = *(uintptr_t*)(citizen + 0x288);
        float birthAge = *(float*)(citizenDesc + 0x44);
        float currentAge = *(float*)(citizen + 0x3C);
        if (currentAge < birthAge) continue;

        // Check alive: death ptr at +0x208 must be 0
        if (*(uintptr_t*)(citizen + 0x208) != 0) continue;

        eligible++;
    }

    if (eligible == 0) return;

    // Advance rotation index
    int* rotIdx = (int*)(s + 0x70);
    (*rotIdx)++;
    if (*rotIdx < 0) *rotIdx = 0;

    // Search for next matching citizen (same profession, adult, alive)
    int scanned = 0;
    while (true) {
        if (*rotIdx >= citizenCount) {
            *rotIdx = 0;
        }

        uintptr_t citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);

        // Check if citizen matches this profession
        if (*(uint64_t*)(citizen + 0x1F0) == profId) {
            citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);

            // Check adult and alive
            uintptr_t citizenDesc = *(uintptr_t*)(citizen + 0x288);
            float birthAge = *(float*)(citizenDesc + 0x44);
            float currentAge = *(float*)(citizen + 0x3C);

            if (currentAge >= birthAge && *(uintptr_t*)(citizen + 0x208) == 0) {
                // Found matching citizen -- check if same profession still (re-read after CheckPtr)
                citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);
                if (*(uint64_t*)(citizen + 0x1F0) != profId) {
                    // Profession changed between checks -- still go to this citizen
                    return;
                }

                // Get citizen's entity (at +0x10)
                citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);
                uint64_t entityPtr = *(uint64_t*)(citizen + 0x10);

                // Center camera on this citizen
                uint64_t dialogInst = *(uint64_t*)(s + 0x18);
                uintptr_t toolController = *(uintptr_t*)(game + 0x120);
                Toolbar_CenterOnEntity_Impl((void*)toolController, &dialogInst, &entityPtr);
                return;
            }
        }

        if (scanned >= eligible) {
            // Exhausted all eligible citizens without finding a match in this profession
            // Fall through to go to the current rotation citizen anyway
            citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);
            if (*(uint64_t*)(citizen + 0x1F0) != profId) {
                return;
            }

            citizen = *(uintptr_t*)(citizenArray + (uintptr_t)*rotIdx * 8);
            uint64_t entityPtr = *(uint64_t*)(citizen + 0x10);

            uint64_t dialogInst = *(uint64_t*)(s + 0x18);
            uintptr_t toolController = *(uintptr_t*)(game + 0x120);
            Toolbar_CenterOnEntity_Impl((void*)toolController, &dialogInst, &entityPtr);
            return;
        }

        (*rotIdx)++;
        scanned++;
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


    // Original logic: msg 0x15 handles BOTH spinner (300-range) and button (500-range).
    // msg 0x12 also handles button (500-range).
    if (param3 != 0x12) {
        if (param3 != 0x15) return;
        // msg 0x15: spinner value changed -- reassign citizens
        if (param4 >= 300 && param4 <= profCount + 300) {
            ProfessionUI_HandleSpinner(self, param4 - 300);
        }
    }
    // msg 0x12 or 0x15: button clicked -- find and center on citizen
    if (param4 > 499 && param4 <= profCount + 500) {
        ProfessionUI_HandleButton(self, param4 - 500);
    }
}
