// engine/src/ui/entityui.cpp
// Clean reimplementation of EntityUI panel methods.
//
// EntityUI (0x48 bytes): Entity info panel.
// Layout: +0x28 = data source (entity), +0x30 = entity ptr,
//         +0x38 = checkPin element, +0x40 = pages element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// EntityTrackingSet_Remove (0x14002c690)
// ============================================================
// BST remove: walks tree to find node with key == param_2, calls BST_Delete.
// param_1 = pointer to BST root (root = *param_1)
// param_2 = InternalBasePtr key to remove
//
// BST node layout (at ptr 'this'):
//   +0x00: key (InternalBasePtr, uint64_t)
//   +0x20: left child ptr (node**)  [+0x20/8 = offset 4 from uint64_t* = offset 0x20]
//   +0x28: right child ptr (node**) [+0x28/8 = offset 5 = +0x28]
static void EntityTrackingSet_Remove_Impl(void* bstRoot, void* key)
{
    // param_1 is longlong* (root node double-ptr)
    uint8_t** rootPtr = (uint8_t**)bstRoot;
    uint8_t* node = (uint8_t*)*rootPtr;

    while (node != nullptr) {
        fn_InternalBasePtr_CheckPtr((void*)node);
        uint64_t nodeKey = *(uint64_t*)node;
        uint64_t searchKey = *(uint64_t*)key;

        fn_InternalBasePtr_CheckPtr(key);
        if (searchKey < nodeKey) {
            // Go left: *(node+0x20) = left child
            node = *(uint8_t**)(node + 0x20);
        } else if (searchKey <= nodeKey) {
            // Found the node — delete it
            fn_EntityTrackingSet_BST_Delete(bstRoot, (void*)node);
            return;
        } else {
            // Go right: *(node+0x28) = right child
            node = *(uint8_t**)(node + 0x28);
        }
    }
    // Not found — nothing to do
}

// ============================================================
// GetDescPage (0x140011320)
// ============================================================
// Searches entity description's component array (+0x28 = ptr, +0x30 = count)
// for a component with type ID == g_TypeId_UIDescription (DAT_1401c7f90).
// Returns the found component in retval. Component field +0x40 = page index.
static void GetDescPage_Impl(void* entityDesc, void* retval)
{
    void* result = nullptr;
    if (!entityDesc) { *(void**)retval = result; return; }

    int count = *(int*)((uintptr_t)entityDesc + 0x30);
    void** compArray = *(void***)((uintptr_t)entityDesc + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = compArray ? compArray[i] : nullptr;
        if (!comp) continue;

        // vtable[2] = GetTypeId (returns int typeId)
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int typeId = ((fn_GetTypeId_t)vtable[2])(comp);

        if (typeId == (int)*g_TypeId_UIDescription) {
            if (fn_IsDerivedFrom(comp, *g_TypeId_UIDescription)) {
                result = comp;
            }
            break;
        }
    }

    *(void**)retval = result;
}

// ============================================================
// BuildComplete (0x140109390)
// ============================================================
// Returns true if build is complete: ResourceComponent.totalRequired * multiplier <= delivered
// posComp+0x18 = description (checked as ResourceComponent)
// ResourceComponent+0x30 = totalRequired
// posComp+0x4C = multiplier (build count), posComp+0x48 = delivered count
static bool BuildComplete_Impl(void* posComp)
{
    if (!posComp) return false;
    // In SA the exe .rdata type-id global can be null (raw .rdata not resolvable);
    // treat "can't classify" as not-complete rather than dereferencing null.
    if (!g_TypeId_ResourceComponent) return false;

    void* desc = *(void**)((uintptr_t)posComp + 0x18);
    bool isResource = fn_IsDerivedFrom(desc, *g_TypeId_ResourceComponent);
    void* resourceComp = isResource ? desc : nullptr;

    if (!resourceComp) return false;

    int totalRequired = *(int*)((uintptr_t)resourceComp + 0x30);
    int multiplier    = *(int*)((uintptr_t)posComp + 0x4C);
    int delivered     = *(int*)((uintptr_t)posComp + 0x48);

    return totalRequired * multiplier <= delivered;
}

// ============================================================
// EntityUI_SetEditMode (0x140102b60)
// ============================================================
// FORWARDER. Retail 0x140102b60 was ported twice, independently: once here as
// EntityUI_SetEditMode_Impl and once in toolbardialog.cpp as
// Toolbar_ShowGroup_Impl. Same object (gameWorld+0x120), same fields, same
// retail function. The single surviving body is Toolbar_ShowGroup_Impl; this
// name is kept because ~15 call sites use it, and it forwards unchanged.
//
// What this copy had WRONG, and why the other body won:
//   * Its in-order scan did `goto done_search` on the FIRST matching node.
//     Retail's inner component loop breaks, but the OUTER traversal runs to
//     the end of the tree, so the LAST match wins. The surviving body does
//     the full traversal.
// What this copy had RIGHT, and the other body did not:
//   * No early-out on foundTypeId == 0. The registry holds a node keyed 0 ->
//     EditTool, so mode 0 exact-matches it and CREATES the default EditTool.
//     That is the whole reason SetEditMode(pc, 0) installs an EditTool and
//     entity selection works. The other body returned early there; that
//     early-out is deleted, not inherited.
//
// Sets the player controller's active tool. Walks a BST at +0x10
// (tool type registry) to find the matching tool entry for the
// given mode. Creates a new tool controller via Factory::CreateObject
// and optionally a ToolSetting, then activates the tool.
//
// PlayerController layout:
//   +0x10: tool registry BST root (nodes: +0x00=typeId(4), +0x04=pad,
//          +0x08=toolTypeId(4), +0x10=settingPtr(8), +0x18=parent(8),
//          +0x20=left(8), +0x28=right(8))
//   +0x20: current mode (uint64)
//   +0x28: active tool (InternalBasePtr)
//   +0x80: override flag (char)
//   +0x88: override mode (uint64)
//
// BST node layout (0x30 bytes, keyed by typeId at +0x00):
//   +0x00: typeId (uint32_t) - key for BST ordering
//   +0x08: toolTypeId (uint32_t) - passed to Factory::CreateObject
//   +0x10: settingPtr (void*) - cached ToolSetting object
//   +0x18: parent (node*), +0x20: left (node*), +0x28: right (node*)

// The surviving port of 0x140102b60 (code/Game/Dialog/toolbardialog.cpp).
void Toolbar_ShowGroup_Impl(void* gameSubsys, int64_t groupId);

void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode)
{
    uintptr_t pc = (uintptr_t)playerController;

    // This name and Toolbar_ShowGroup_Impl are ONE retail function (0x140102b60);
    // they were two independent ports until they were collapsed. Both spellings
    // write +0x20, so a SetEditMode(pc,0) from anywhere clears the toolbar's
    // active group and tears down the live tool (then reinstalls the default
    // EditTool from the key-0 node). Log the RETURN ADDRESS so the
    // actual caller is identifiable instead of guessed at.
    {
        static const char* t = getenv("BANISHED_TOOL_TRACE");
        if (t && t[0] && t[0] != '0') {
            fprintf(stderr, "[ToolTrace] SETEDITMODE pc=%p mode=%p cur+0x20=%p cur+0x28=%p ret=%p\n",
                    playerController, (void*)mode, *(void**)(pc + 0x20),
                    *(void**)(pc + 0x28), __builtin_return_address(0));
            fflush(stderr);
        }
    }

    // Forward to the single implementation. int64_t vs uint64_t is a spelling
    // difference only - the field at +0x20 is one 8-byte slot either way.
    Toolbar_ShowGroup_Impl(playerController, (int64_t)mode);
}

// ============================================================
// EntityUI_SelectEntity (0x140102e30)
// ============================================================
// Inserts or updates entity in the tracking AVL BST at
// playerController+0x60. Increments count at +0x68 on insert.
//
// BST node (0x38 bytes):
//   +0x00: key (uint64_t, entity InternalBasePtr value)
//   +0x08: x position (int32)
//   +0x0C: y position (int32)
//   +0x10: active flag (int32, set to 1)
//   +0x18: parent (node*)
//   +0x20: left child (node*)
//   +0x28: right child (node*)
//   +0x30: balance factor (int16)
//   +0x32: height (int16)
//
// NOTE: This function is used in both entityui.cpp and toolui.cpp.

void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                 uint32_t x, uint32_t y)
{
    uintptr_t pc = (uintptr_t)playerController;
    uint64_t entityKey = *(uint64_t*)entityPtr;
    uintptr_t* rootPtr = (uintptr_t*)(pc + 0x60);

    // Search BST for existing node with matching key
    uintptr_t node = *rootPtr;
    while (node != 0) {
        fn_InternalBasePtr_CheckPtr((void*)node);
        uint64_t nodeKey = *(uint64_t*)node;
        if (entityKey < nodeKey) {
            node = *(uintptr_t*)(node + 0x20);  // left
        } else {
            fn_InternalBasePtr_CheckPtr(entityPtr);
            if (entityKey <= nodeKey) {
                // Found — update position and active flag
                if (node != 0) {
                    *(uint8_t*)(node + 0x10) = 1;  // active = true
                    *(uint32_t*)(node + 0x08) = x;
                    *(uint32_t*)(node + 0x0C) = y;
                    return;
                }
                break;
            }
            node = *(uintptr_t*)(node + 0x28);  // right
        }
    }

    // Not found — insert new node
    // Prepare node data
    uint32_t activeFlag = 1;

    if (*rootPtr == 0) {
        // Empty tree — create root node
        uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
        if (newNode != nullptr) {
            *(uint64_t*)newNode = 0;
            *(uint64_t*)newNode = entityKey;
            *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
            *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
            *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
            newNode[3] = 0;  // parent = null
            newNode[4] = 0;  // left = null
            newNode[5] = 0;  // right = null
            *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;  // balance = 0
        }
        *rootPtr = (uintptr_t)newNode;
    } else {
        // Walk BST to find insertion point
        uintptr_t cur = *rootPtr;
        uintptr_t parent = 0;

        while (true) {
            parent = cur;
            fn_InternalBasePtr_CheckPtr((void*)cur);
            uint64_t curKey = *(uint64_t*)cur;

            if (curKey <= entityKey) {
                fn_InternalBasePtr_CheckPtr(entityPtr);
                if (entityKey <= curKey) {
                    // Duplicate key — should not happen, already handled above
                    return;
                }
                // Go right
                uintptr_t right = *(uintptr_t*)(cur + 0x28);
                if (right == 0) {
                    // Insert as right child
                    uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, 0x38, 8);
                    if (newNode != nullptr) {
                        *(uint64_t*)newNode = 0;
                        *(uint64_t*)newNode = entityKey;
                        *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
                        *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
                        *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
                        newNode[4] = 0;   // left
                        newNode[5] = 0;   // right
                        *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;
                        newNode[3] = parent;  // parent
                    }
                    *(uintptr_t*)(cur + 0x28) = (uintptr_t)newNode;
                    // Rebalance
                    uintptr_t nodeRef = (uintptr_t)newNode;
                    fn_AVL_Rebalance_0x38(rootPtr, &nodeRef);
                    break;
                }
                cur = right;
            } else {
                // Go left
                uintptr_t left = *(uintptr_t*)(cur + 0x20);
                if (left == 0) {
                    // Insert as left child
                    uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, 0x38, 8);
                    if (newNode != nullptr) {
                        *(uint64_t*)newNode = 0;
                        *(uint64_t*)newNode = entityKey;
                        *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
                        *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
                        *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
                        newNode[4] = 0;   // left
                        newNode[5] = 0;   // right
                        *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;
                        newNode[3] = parent;  // parent
                    }
                    *(uintptr_t*)(cur + 0x20) = (uintptr_t)newNode;
                    // Rebalance
                    uintptr_t nodeRef = (uintptr_t)newNode;
                    fn_AVL_Rebalance_0x38(rootPtr, &nodeRef);
                    break;
                }
                cur = left;
            }
        }
    }

    // Increment entity count
    *(int*)(pc + 0x68) = *(int*)(pc + 0x68) + 1;
}

// ============================================================
// EditTool_SelectEntity — NEEDS OPUS (0x1400fd3b0)
// ============================================================
// 100+ lines: iterates all dialogs finding ToolUI controllers,
// creates DialogInstance if entity doesn't have one yet.
// Kept as passthrough via fn_EditTool_SelectEntity.

// Slot4 (0x1400670D0): Startup — find elements, get entity, reset ToolUI flags.
void EntityUI_Slot4(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source (+0x28), check if it's a ComponentEntity
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    fn_InternalBasePtr_Ctor((void*)(s + 0x30), entity);

    // Find "buttonCenter" element and set ID 101 (0x65)
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonCenter");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        fn_Element_SetElementId(elem, 0x65);
    }

    // Find "checkPin" element → store at +0x38, set ID 100
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "checkPin");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        *(void**)(s + 0x38) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        fn_Element_SetElementId(*(void**)(s + 0x38), 100);
    }

    // Get dialog controller, check if it's a ToolUI type
    {
        void* dialogInst = *(void**)(s + 0x18);
        uint8_t ctrlBuf[16];
        fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
            if (isTool) toolUI = controller;
        }

        uint8_t toolBuf[16];
        fn_InternalBasePtr_Ctor(toolBuf, toolUI);
        void* toolPtr = *(void**)toolBuf;

        // Reset flags at controller+0x40 and +0x41
        *(uint8_t*)((uintptr_t)toolPtr + 0x40) = 0;
        if (*(uint8_t*)((uintptr_t)toolPtr + 0x40) == 0 &&
            *(uint8_t*)((uintptr_t)toolPtr + 0x41) != 0) {
            *(uint8_t*)((uintptr_t)toolPtr + 0x41) = 0;
        }
    }

    // Find "pages" element → store at +0x40
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "pages");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        *(void**)(s + 0x40) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }
}

// Slot6: Remove entity from world tracking set.
void EntityUI_Slot6(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Copy +0x30 (entity InternalBasePtr) to local
    uint64_t entityKey = *(uint64_t*)(s + 0x30);

    // Get BST root: g_pGameWorldSingleton -> +0x120 -> +0x60
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0x120);
    void* bstRoot = (void*)((uintptr_t)subObj + 0x60);

    EntityTrackingSet_Remove_Impl(bstRoot, &entityKey);
}

// ============================================================
// EntityUI::Slot7 (vtable +0x38) — Page switching logic
// ============================================================
// Determines which page to show based on entity state (build state,
// destroy state, etc.) and sets the PageElement active page.
// Also updates ToolUI controller's selection hint.
//
// Original: 0x140066D80.

void EntityUI_Slot7(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

        // (risky in SA, cosmetic only); the page-selection below is what makes the
        // entity/citizen content actually render, so it always runs.

    // Find components on entity to determine page state
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    void* entityPtr = *(void**)(s + 0x30);

    void* buildStateComp = nullptr;
    fn_FindComponent_BuildState(entityPtr, &buildStateComp);

    void* posComp = nullptr;
    fn_FindComponent_Position(entityPtr, &posComp);

    void* smComp = nullptr;  // StateMachine/animation component
    fn_FindComponent_SM(entityPtr, &smComp);

    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityPtr, &destroyComp);

    // Determine page index based on component state
    int pageIdx;

    int8_t state52 = 0, state50 = 0;
    if (buildStateComp == 0) {
        pageIdx = -1;
    } else {
        // Check build state: +0x52 (if negative) use +0x50
        state52 = *(int8_t*)((uintptr_t)buildStateComp + 0x52);
        state50 = *(int8_t*)((uintptr_t)buildStateComp + 0x50);
        pageIdx = (state52 < 0) ? (int)state50 : (int)state52;
    }

    if (posComp == 0 && destroyComp == 0) {
        if (smComp == 0 || pageIdx != 0) {
            // Get default page from entity's description
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
            void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
            void* descResult = nullptr;
            GetDescPage_Impl(entityDesc, &descResult);
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x40));
            pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
        } else {
            pageIdx = 2;
        }
    } else {
        // Build/destroy in progress — sequential checks matching original flow
        if (pageIdx >= 3 && pageIdx <= 7) {
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x40));
            pageIdx = 3;
        } else {
            // Check 2: if posComp exists, verify build is complete
            bool gotoPage2 = false;
            if (posComp != 0) {
                fn_InternalBasePtr_CheckPtr(&posComp);
                bool complete = BuildComplete_Impl((void*)posComp);
                if (!complete) {
                    gotoPage2 = true;
                }
            }
            // Check 3: if not redirected to page 2 and pageIdx != 0, get desc page
            if (!gotoPage2 && pageIdx != 0) {
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
                void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
                void* descResult = nullptr;
                GetDescPage_Impl(entityDesc, &descResult);

                fn_InternalBasePtr_CheckPtr((void*)(s + 0x40));
                void* pagesElem = *(void**)(s + 0x40);
                // Retail (0x140066d80) assigns the ACTIVE page into the same register it
                // then conditionally overwrites:
                //     iVar6 = GetActivePage(pages);
                //     if (iVar6 != 4) iVar6 = descPage;
                //     SetActivePage(pages, iVar6);
                // i.e. page 4 -- the user page, which is the production-statistics page --
                // is DELIBERATELY preserved across this per-frame pass. Our port kept the
                // `!= 4` test but not the assignment, so on page 4 it wrote back the STALE
                // build-state pageIdx instead of 4: clicking the statistics button showed
                // the production page for one frame and the panel snapped back to the
                // default page every frame after (the reported 'statistics flicker').
                int activePage = pagesElem ? fn_PageElement_GetActivePage(pagesElem) : 0;
                const int staleIdx = pageIdx;   // what the pre-fix port wrote back
                pageIdx = activePage;
                if (activePage != 4) {
                    pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
                }
                // PAGE-4 STICK TEST (BANISHED_PAGE4_TEST=1, test-only). Forces the user
                // page once -- the same state a click on the production-statistics button
                // leaves behind -- then reports, per pass, the page this function decided
                // AND the page the pre-fix code would have written. One run shows both
                // arms, so 'it sticks now' is measured rather than argued.
                {
                    static int s_on = -1;
                    if (s_on < 0) {
                        const char* e = std::getenv("BANISHED_PAGE4_TEST");
                        s_on = (e && e[0] && e[0] != '0') ? 1 : 0;
                    }
                    if (s_on == 1 && pagesElem) {
                        static long long n = 0;
                        static bool forced = false;
                        ++n;
                        if (!forced && n > 120) {
                            forced = true;
                            fn_PageElement_SetActivePage(pagesElem, 4);
                            pageIdx = 4;   // this pass must not overwrite its own force
                            std::fprintf(stderr, "[Page4Test] forced active page 4\n");
                        } else if (forced && (n % 30) == 0) {
                            std::fprintf(stderr,
                                "[Page4Test] entry activePage=%d -> writing %d (pre-fix would write %d)\n",
                                activePage, pageIdx, (activePage == 4) ? staleIdx : pageIdx);
                        }
                        std::fflush(stderr);
                    }
                }
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x40));
            } else {
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x40));
                pageIdx = 2;
            }
        }
    }

    // One-shot page-routing diagnostic (BANISHED_PAGE_TRACE). Dumps every input feeding
    // the page decision plus the chosen page, so we can see WHICH branch sends an entity
    // (e.g. the default stockpile) to the under-construction build page (2) instead of its
    // operational page. Rate-limited to distinct entity pointers so a per-frame Slot7 doesn't
    // spam the log.
    {
        static int pt = -1;
        if (pt < 0) { const char* e = std::getenv("BANISHED_PAGE_TRACE"); pt = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (pt == 1) {
            static void* s_lastEnt = nullptr;
            if (entityPtr != s_lastEnt) {
                s_lastEnt = entityPtr;
                bool complete = (posComp != 0) ? BuildComplete_Impl((void*)posComp) : false;
                std::fprintf(stderr,
                    "[PageTrace] ent=%p buildStateComp(0x1c7d98)=%p state52=%d state50=%d | "
                    "posComp/BuildComp(0x1c7008)=%p destroyComp=%p smComp(0x1c71c8)=%p | "
                    "BuildComplete=%d -> pageIdx=%d\n",
                    entityPtr, buildStateComp, (int)state52, (int)state50,
                    posComp, destroyComp, smComp, (int)complete, pageIdx);
                std::fflush(stderr);
            }
        }
    }

    void* pagesElemFinal = *(void**)(s + 0x40);
    if (pagesElemFinal) {
        fn_PageElement_SetActivePage(pagesElemFinal, pageIdx);
    }
}

// ============================================================
// EntityUI::Slot8 (vtable +0x40) — Input handler
// ============================================================
// Reads mouse position from KeyMap, scales by dialog scale factor.
// Gets ToolUI controller, sets +0x4C = 1, syncs map visibility mode.
// On key 5 press: checks if mouse is over this dialog and entity exists,
// then selects the entity in the edit tool (creating one if needed).
//
// Original: 0x140066AD0.

void EntityUI_Slot8(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get KeyMap singleton (triple deref from exref: slot → External → KeyMap)
    void* keyMap = ***(void****)g_pKeyMapObjectExref;

    // Get dialog element from +0x18 -> +0x10 (dialog root element)
    void* dialogInst = *(void**)(s + 0x18);
    void* dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);

    // Read mouse axes and scale by dialog scale factor (+0xDC on element)
    short axisX = fn_KeyMap_GetValue(keyMap, 0, 0);
    int mouseX = (int)((float)(int)axisX * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Re-read element since CheckPtr pattern accesses +0x18 again
    dialogInst = *(void**)(s + 0x18);
    dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);
    short axisY = fn_KeyMap_GetValue(keyMap, 0, 1);
    int mouseY = (int)((float)(int)axisY * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Get ToolUI controller from dialog
    dialogInst = *(void**)(s + 0x18);
    uint8_t ctrlBuf[16];
    fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
    void* controller = *(void**)ctrlBuf;

    void* toolUI = nullptr;
    if (controller) {
        bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
        if (isTool) toolUI = controller;
    }
    uint8_t toolBuf[8];
    fn_InternalBasePtr_Ctor(toolBuf, toolUI);
    uintptr_t toolPtr = *(uintptr_t*)toolBuf;

    // Set controller +0x4C = 1 (input active flag)
    *(uint8_t*)(toolPtr + 0x4C) = 1;

    // Sync map visibility mode: compare controller +0x40 with checkPin element +0xD8
    char checkVal = *(char*)(*(uintptr_t*)(s + 0x38) + 0xD8);
    char ctrlVal = *(char*)(toolPtr + 0x40);

    if (ctrlVal != checkVal) {
        *(char*)(*(uintptr_t*)(s + 0x38) + 0xD8) = ctrlVal;
    }

    // Check key 5 press (left mouse button / select)
    if (!fn_KeyMap_Pressed(keyMap, 0, 5))
        return;

    // Get dialog at mouse position
    dialogInst = *(void**)(s + 0x18);
    void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);

    uint8_t dialogAtPosBuf[16];
    fn_UI_GetFirstContainedDialog(userInterface, dialogAtPosBuf, mouseX, mouseY);
    void* dialogAtPos = *(void**)dialogAtPosBuf;

    // Check if the dialog at mouse position is this dialog, and entity exists
    dialogInst = *(void**)(s + 0x18);
    if (dialogAtPos != dialogInst || *(uintptr_t*)(s + 0x30) == 0)
        return;

    // Get current tool from player controller (IBP at +0x28, needs CheckPtr)
    void* playerController = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
    void* currentTool = *(void**)((uintptr_t)playerController + 0x28);
    fn_InternalBasePtr_CheckPtr(&currentTool);

    // Check if current tool is EditTool
    bool isEditTool = fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool);
    void* editTool = isEditTool ? currentTool : nullptr;

    uint8_t editToolBuf[8];
    fn_InternalBasePtr_Ctor(editToolBuf, editTool);

    if (*(void**)editToolBuf == nullptr) {
        // No EditTool active — deselect first, then get new EditTool
        EntityUI_SetEditMode_Impl(playerController, 0);

        // Re-read current tool after deselect (with CheckPtr)
        currentTool = *(void**)((uintptr_t)playerController + 0x28);
        fn_InternalBasePtr_CheckPtr(&currentTool);
        isEditTool = fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool);
        void* newEditTool = isEditTool ? currentTool : nullptr;

        fn_InternalBasePtr_Ctor(editToolBuf, newEditTool);
    }

    // Select the entity in the edit tool
    void* editToolPtr = *(void**)editToolBuf;
    if (editToolPtr) {
        void* entityPtr = *(void**)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(editToolBuf);
        fn_EditTool_SelectEntity(*(void**)editToolBuf, &entityPtr);
    }
}

// ============================================================
// EntityUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles button clicks:
//   ID 1 = close entity panel (set edit mode 0)
//   ID 0x65 (101) = center camera on entity
//   ID 100 = pin/unpin entity tracking
//
// Original: 0x140067050.

void EntityUI_Slot9(EntityUI* self, void* param2, int msg, int id) {
    (void)param2;
    if (msg != 0x12) return;

    uintptr_t s = (uintptr_t)self;
    void* playerController = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);

    if (id == 1) {
        // Close entity panel — set edit mode to 0
        EntityUI_SetEditMode_Impl(playerController, 0);
        return;
    }

    if (id == 0x65) {
        // Center camera on entity (buttonCenter). Faithful to the exe center block
        // (0x140066880..0x140066a8c) — the same position math as
        // Toolbar_CenterOnEntity_Impl Section 2:
        //   - ModelComponent present AND no MultiObjectComponent  -> single object
        //     (citizen/animal/small building): use the model's world Transform
        //     translation (ModelComponent+0x148 Transform, pos at +0x158/+0x15C).
        //   - otherwise -> multi-object building: midpoint of the MapComponent grid
        //     footprint's far corner and origin corner, each mapped tile->world via
        //     fn_MapGetTerrainPos, then averaged (DAT_140172114 = 0.5).
        // Writes ONLY the camera focus target (game+0xe0 +0x1c/+0x20/+0x24, z forced
        // 0 as retail does); the camera smoothly slides to it. No re-select / no snap
        // (that is what distinguishes this from Toolbar_CenterOnEntity, which also
        // reopens the target dialog).
        uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
        void* entityPtr = *(void**)(s + 0x30);

        float focus[2] = {0.0f, 0.0f};
        bool gotPos = false;

        uint64_t modelRef = 0;
        fn_FindComponent_Character(entityPtr, &modelRef);
        if (modelRef != 0) {
            uint64_t multiRef = 0;
            fn_FindComponent_MultiObject(entityPtr, &multiRef);
            if (multiRef == 0) {
                fn_InternalBasePtr_CheckPtr(&modelRef);
                uintptr_t model = (uintptr_t)modelRef;
                focus[0] = *(float*)(model + 0x158);
                focus[1] = *(float*)(model + 0x15C);
                gotPos = true;
            }
        }

        if (!gotPos) {
            uint64_t buildRef = 0;
            fn_FindComponent_Build(entityPtr, &buildRef);
            if (buildRef != 0) {
                uintptr_t build = (uintptr_t)buildRef;
                int* mapGrid = *(int**)(game + 0xf8);
                int gridSize = *mapGrid;
                float halfGrid = (float)(gridSize >> 1);

                int16_t posX = *(int16_t*)(build + 0x28);
                int16_t posY = *(int16_t*)(build + 0x2a);
                int16_t extX = *(int16_t*)(build + 0x2c);
                int16_t extY = *(int16_t*)(build + 0x2e);

                int idx1 = (int)extX + ((int)extY + (int)posY) * gridSize + (int)posX;
                float inXY1[3] = { (float)(idx1 % gridSize) - halfGrid,
                                   (float)(idx1 / gridSize) - halfGrid, 0.0f };
                float pos1[3] = {0.0f, 0.0f, 0.0f};
                if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(mapGrid, pos1, inXY1, '\0', nullptr);

                int idx2 = gridSize * (int)posY + (int)posX;
                float inXY2[3] = { (float)(idx2 % gridSize) - halfGrid,
                                   (float)(idx2 / gridSize) - halfGrid, 0.0f };
                float pos2[3] = {0.0f, 0.0f, 0.0f};
                if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(mapGrid, pos2, inXY2, '\0', nullptr);

                focus[0] = (pos1[0] + pos2[0]) * 0.5f;
                focus[1] = (pos1[1] + pos2[1]) * 0.5f;
                gotPos = true;
            }
        }

        if (gotPos) {
            uintptr_t cameraTarget = *(uintptr_t*)(game + 0xe0);
            *(float*)(cameraTarget + 0x1c) = focus[0];
            *(float*)(cameraTarget + 0x20) = focus[1];
            *(float*)(cameraTarget + 0x24) = 0.0f;
        }
        return;
    }

    if (id == 100) {
        // Pin/unpin: toggle checkPin state and update entity tracking

        // Get ToolUI controller
        void* dialogInst = *(void**)(s + 0x18);
        uint8_t ctrlBuf[16];
        fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
            if (isTool) toolUI = controller;
        }
        uint8_t toolBuf[8];
        fn_InternalBasePtr_Ctor(toolBuf, toolUI);
        void* toolPtr = *(void**)toolBuf;

        // The pin state lives on the checkPin element (+0x38, set in Slot4) and the
        // pin flag on the sibling ToolUI controller (+0x40). Both must exist to pin;
        // in standalone a partially-built dialog could be missing either, so guard
        // before dereferencing them.
        if (toolPtr == nullptr || *(uintptr_t*)(s + 0x38) == 0) return;

        // Read current pin state from checkPin element (+0x38) -> +0xD8
        char pinState = *(char*)(*(uintptr_t*)(s + 0x38) + 0xd8);

        // Set ToolUI +0x40 flag to pin state
        *(char*)((uintptr_t)toolPtr + 0x40) = pinState;
        if (pinState == 0 && *(char*)((uintptr_t)toolPtr + 0x41) != 0) {
            *(char*)((uintptr_t)toolPtr + 0x41) = 0;
        }

        // If unpinned, remove entity from tracking set
        if (*(char*)(*(uintptr_t*)(s + 0x38) + 0xd8) == 0) {
            uint64_t entityKey = *(uint64_t*)(s + 0x30);
            uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
            EntityTrackingSet_Remove_Impl(
                (void*)(worldObj + 0x50),
                &entityKey);
            return;
        }

        // If pinned, select entity in edit tool
        uint32_t dialogX = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x88);
        uint32_t dialogY = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x8c);
        void* entityKey = *(void**)(s + 0x30);
        EntityUI_SelectEntity_Impl(playerController, &entityKey, dialogX, dialogY);
        return;
    }
}

// ============================================================
// EntityUI::Shutdown (vtable +0x18) — No-op
// ============================================================

// ============================================================
// STANDALONE promotion — real EntityUI controller vtable
// ============================================================
// EntityUI is the common outer frame controller for every entity info panel
// (center/pin/pages + tracks the selected entity). In standalone it was a StubNop,
// so its Slot4 never ran — which left the controller's entity ref (+0x30) null. The
// EditTool select-repositions the panel beside the clicked entity by reading that
// +0x30 ref, so with the stub it opened top-left. Wiring Slot4 sets +0x30 (fixes
// positioning) and discovers the buttonCenter/checkPin/pages elements.
//
// Vtable offsets (verified from the exe EntityUI vtable @ 0x14015bb90):
//   +0x20 (idx4) Slot4 = Startup/Setup  (sets +0x30 entity ref; find elements)
//   +0x30 (idx6) Slot6 = remove entity from the world tracking set
//   +0x38 (idx7) Slot7 = page switching (driven per-frame via W_OnUpdate on +0x40)
//   +0x48 (idx9) Slot9 = OnElementEvent: buttonCenter (0x65) + checkPin (100) buttons
// Slot9 is wired (W_Slot9) so the panel's "center" and "pin" options work — for every
// entity dialog, since EntityUI is the shared frame controller. It forwards only the
// center/pin ids; the close button (id 1) stays with the sibling ToolUI::Slot9 to
// avoid retail's SetEditMode(0) tool-teardown (see W_Slot9). Slot8 (the real +0x40
// input handler) remains a no-op — its +0x40 slot is reused to drive Slot7 per-frame,
// and clicking entities works via EditTool::OnKey. The panel's close/drag come from
// the ToolUI controller (already wired).
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>

namespace entityui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("EntityUI");
    return c;
}
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void EnsureGlobals() {
    static uint32_t s_ce = 0, s_tool = 0, s_uidesc = 0;
    if (!g_TypeId_ComponentEntity) { s_ce   = ::Object::ResolveTypeIdByNameLazy("ComponentEntity"); if (s_ce)   g_TypeId_ComponentEntity = &s_ce; }
    if (!g_TypeId_ToolUI)          { s_tool = ::Object::ResolveTypeIdByNameLazy("ToolUI");          if (s_tool) g_TypeId_ToolUI = &s_tool; }
    // EntityUI::Slot7 (page switching) reads the entity description's UIDescription
    // component (+0x40 = default page index) via GetDescPage_Impl, which derefs
    // *g_TypeId_UIDescription. In SA that exe .rdata global is null, so resolve the
    // type-id from the live factory tree. Without it, Slot7 can't pick the citizen's
    // content page and the panel stays on the empty default page (blank body).
    if (!g_TypeId_UIDescription)   { s_uidesc = ::Object::ResolveTypeIdByNameLazy("UIDescription"); if (s_uidesc) g_TypeId_UIDescription = &s_uidesc; }
    // Building panels reach BuildComplete_Impl (page selection for in-progress
    // construction), which classifies the position component's ResourceComponent.
    static uint32_t s_res = 0;
    if (!g_TypeId_ResourceComponent) { s_res = ::Object::ResolveTypeIdByNameLazy("ResourceComponent"); if (s_res) g_TypeId_ResourceComponent = &s_res; }

    // EntityUI::Slot9's buttonCenter (id 0x65) finds the entity's ModelComponent
    // (single object -> use its Transform) or, failing that, its MapComponent grid
    // footprint (multi-object building). Those finders read exe .rdata type-id
    // globals that ResolveAddr leaves NULL in standalone, so fill them from the live
    // factory tree. The type-ids used here: g_TypeId_ModelComponent_FC is
    // DAT_1401c7a50 = ModelComponent, and g_TypeId_MapComponent_FC is DAT_1401c79e0
    // = MapComponent (written by MapComponent_RegisterType) — verified via Ghidra
    // DATA xrefs. Without these the center button would find no component and
    // (build path) null-deref.
    static uint32_t s_model = 0, s_multi = 0, s_mapc = 0;
    if (!g_TypeId_ModelComponent_FC)    { s_model = ::Object::ResolveTypeIdByNameLazy("ModelComponent");       if (s_model) g_TypeId_ModelComponent_FC = &s_model; }
    if (!g_TypeId_MultiObjectComponent)  { s_multi = ::Object::ResolveTypeIdByNameLazy("MultiObjectComponent"); if (s_multi) g_TypeId_MultiObjectComponent = &s_multi; }
    if (!g_TypeId_MapComponent_FC)  { s_mapc  = ::Object::ResolveTypeIdByNameLazy("MapComponent");         if (s_mapc)  g_TypeId_MapComponent_FC = &s_mapc; }

    static bool s_logged = false;
    if (!s_logged && std::getenv("BANISHED_DIALOG_TRACE")) {
        s_logged = true;
        fprintf(stderr, "[EntityUI] globals: CE=%p ToolUI=%p UIDesc=%p\n",
                (void*)g_TypeId_ComponentEntity, (void*)g_TypeId_ToolUI,
                (void*)g_TypeId_UIDescription);
        fflush(stderr);
    }
}

void W_Slot4(void* self) {
    EnsureGlobals();
    if (!self || !g_TypeId_ComponentEntity || !g_TypeId_ToolUI) return;
    if (!SA_DialogEntityAlive(self)) return;
    EntityUI_Slot4(reinterpret_cast<EntityUI*>(self));
}
void W_Slot6(void* self) {
    if (!self || !SA_DialogEntityAlive(self)) return;
    EntityUI_Slot6(reinterpret_cast<EntityUI*>(self));
}
// Slot7 = page switching. The info panel's element tree has the entity/citizen
// content on a non-default page of the "pages" PageElement (e.g. the tabbed
// "pageTabGroup" for citizens). Slot7 reads the entity's default page and calls
// PageElement::SetActivePage — WITHOUT it the panel shows only the empty default
// page (chrome/title only). The SA build of EntityUI_Slot7 skips the cosmetic
// tool-hint highlight (EditTool/ToolUI vtable calls, #ifndef STANDALONE_EXE) and
// runs only the page-selection, which is what makes the body render.
void W_Slot7(void* self) {
    EnsureGlobals();
    if (!self || !g_TypeId_UIDescription) return;
    if (!SA_DialogEntityAlive(self)) return;
    EntityUI_Slot7(reinterpret_cast<EntityUI*>(self));
}
// +0x40 per-frame: the framework dispatches only vtable+0x40 each frame, so drive
// the page-selection Slot7 from here (the exe's real +0x40 is the input handler,
// intentionally left a no-op in SA — higher risk, no display payoff).
void W_OnUpdate(void* self, void* /*time*/) { W_Slot7(self); }

// +0x48 OnElementEvent — the button-command dispatch. Runtime DialogInstance::On-
// ElementEvent (0x180114ed0) forwards every element event to each controller's
// vtable+0x48, so this receives the frame buttons: buttonCenter (id 0x65, msg 0x12)
// and checkPin (id 100). Those are what implement the panel's "center" and "pin"
// options for EVERY entity info dialog (EntityUI is the shared frame controller on
// both building and citizen panels). Wiring this makes them functional in standalone.
//
// The close button (id 1) is intentionally NOT forwarded here: retail EntityUI::Slot9
// closes via SetEditMode(0), which tears down the active EditTool (vtable Shutdown +
// DeletePtr). In standalone the sibling ToolUI::Slot9 already handles the close (and
// drops the EditTool's stale dialog ref instead of destroying the tool — see
// toolui.cpp Slot9), so running SetEditMode here would re-introduce the very
// tool-teardown crash that path avoids. Skipping id 1 leaves close to ToolUI.
void W_Slot9(void* self, void* p2, int msg, int id, int /*mx*/, int /*my*/) {
    if (!self || msg != 0x12) return;
    if (id == 1) return;              // close -> handled by ToolUI::Slot9
    if (id != 0x65 && id != 100) return;
    EnsureGlobals();
    // pin (id 100) needs the ToolUI type-id to fetch the sibling controller; center
    // (id 0x65) tolerates null component type-ids (finders just return "not found").
    if (id == 100 && !g_TypeId_ToolUI) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    // Gate the entity read like every other EntityUI slot wrapper (W_Slot4/6/7):
    // EntityUI_Slot9 reads the cached datasource copy (s+0x30) for center (0x65) and
    // toggles tracking on it for pin (100). On a just-demolished building's still-open
    // panel that +0x30 weak-ref dangles, so a center/pin click would deref freed
    // memory (fn_FindComponent_Character). SA_DialogEntityAlive checks the +0x28
    // datasource (of which +0x30 is a copy) for null / marked-for-removal (0x400).
    if (!SA_DialogEntityAlive(self)) return;
    EntityUI_Slot9(reinterpret_cast<EntityUI*>(self), p2, msg, id);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);
        g_vt[6] = reinterpret_cast<void*>(&W_Slot6);
        g_vt[7] = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_Slot9);
    }
} g_init;

void Zero(void* p) { for (size_t o = 0; o < (size_t)0x48; o += 8) *reinterpret_cast<uint64_t*>((char*)p + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0x48, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace entityui_sa

REGISTER_TYPE(EntityUI, "UI::DialogController",
              &entityui_sa::Factory, &entityui_sa::InPlace);

// (original comment header retained below)
// ============================================================
// EntityUI::Shutdown (vtable +0x18) — No-op
// ============================================================
