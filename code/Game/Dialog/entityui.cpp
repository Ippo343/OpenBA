// engine/src/ui/entityui.cpp
// Clean reimplementation of EntityUI panel methods.
//
// EntityUI (0x48 bytes): Entity info panel.
// Layout: +0x28 = data source (entity), +0x30 = entity ptr,
//         +0x38 = checkPin element, +0x40 = pages element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/PageElement.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "input/Input.h"  // ENDGAME B.3.7 callers (K3)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/MapComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/ModelComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/MultiObjectComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/UIDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/ToolUI.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Component/BuildDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Player/EditTool.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Dialog/exe_bodies.h"
#include "Player/exe_bodies.h"   // B3R: PlayerTrade_BSTRemove_25740, PlayerTrade_Retrace_25920
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

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
        uint64_t nodeKey = *(uint64_t*)node;
        uint64_t searchKey = *(uint64_t*)key;

        if (searchKey < nodeKey) {
            // Go left: *(node+0x20) = left child
            node = *(uint8_t**)(node + 0x20);
        } else if (searchKey <= nodeKey) {
            // Found the node — delete it
            PlayerTrade_BSTRemove_25740(reinterpret_cast<int64_t*>(bstRoot), (void*)node);   // 0x140025740
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
// for a component with type ID == TypeId_UIDescription (DAT_1401c7f90).
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
        int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == (int)UIDescription::TypeId()) {
            if (::Object::IsDerivedFromNullSafe(comp, UIDescription::TypeId())) {
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

    void* desc = *(void**)((uintptr_t)posComp + 0x18);
    bool isResource = ::Object::IsDerivedFromNullSafe(desc, BuildDescription::TypeId());
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
        uint64_t nodeKey = *(uint64_t*)node;
        if (entityKey < nodeKey) {
            node = *(uintptr_t*)(node + 0x20);  // left
        } else {
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
        uintptr_t* newNode = (uintptr_t*)System::MemoryAllocator::MemAlloc(0x38, 8);
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
            uint64_t curKey = *(uint64_t*)cur;

            if (curKey <= entityKey) {
                if (entityKey <= curKey) {
                    // Duplicate key — should not happen, already handled above
                    return;
                }
                // Go right
                uintptr_t right = *(uintptr_t*)(cur + 0x28);
                if (right == 0) {
                    // Insert as right child
                    uintptr_t* newNode = (uintptr_t*)System::MemoryAllocator::MemAlloc(0x38, 8);
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
                    PlayerTrade_Retrace_25920(reinterpret_cast<int64_t*>(rootPtr), reinterpret_cast<int64_t*>(&nodeRef));   // 0x140025920
                    break;
                }
                cur = right;
            } else {
                // Go left
                uintptr_t left = *(uintptr_t*)(cur + 0x20);
                if (left == 0) {
                    // Insert as left child
                    uintptr_t* newNode = (uintptr_t*)System::MemoryAllocator::MemAlloc(0x38, 8);
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
                    PlayerTrade_Retrace_25920(reinterpret_cast<int64_t*>(rootPtr), reinterpret_cast<int64_t*>(&nodeRef));   // 0x140025920
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

// ENDGAME B.3.9: ComponentEntity / ToolUI / UIDescription / BuildDescription (was
// TypeId_ResourceComponent, itself healed here under the WRONG by-name guess
// "ResourceComponent" -- Table 1's authority for cell 0x1401c7040 is BuildDescription;
// the heal never actually ran, since the pointer is set unconditionally at
// EngineFuncsInit before this ever fires) / ModelComponent / MultiObjectComponent /
// MapComponent / EditTool all have a ported TypeId() now, so the entityui_sa::EnsureGlobals()
// lazy-cache namespace is dead and its call sites below are removed.

// Slot4 (0x1400670D0): Startup — find elements, get entity, reset ToolUI flags.
// [4] 0x1400670d0
void EntityUI::OnCreated() {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    // Get entity from data source (+0x28), check if it's a ComponentEntity
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    new ((void*)(s + 0x30)) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

    // Find "buttonCenter" element and set ID 101 (0x65)
    {
        System::String nameStr("buttonCenter");
        uint8_t retBuf[16];
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;
        reinterpret_cast<UI::Element*>(elem)->SetElementId(0x65);  // B.3.6 bridge
    }

    // Find "checkPin" element → store at +0x38, set ID 100
    {
        System::String nameStr("checkPin");
        uint8_t retBuf[16];
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        *(void**)(s + 0x38) = *(void**)retBuf;
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x38))->SetElementId(100);  // B.3.6 bridge
    }

    // Get dialog controller, check if it's a ToolUI type
    {
        void* dialogInst = *(void**)(s + 0x18);
        uint8_t ctrlBuf[16];
        ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialogInst)->GetController(ToolUI::TypeId()));  // B.3.6 bridge
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = ::Object::IsDerivedFromNullSafe(controller, ToolUI::TypeId());
            if (isTool) toolUI = controller;
        }

        uint8_t toolBuf[16];
        new (toolBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(toolUI));   // B.3.3 bridge
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
        System::String nameStr("pages");
        uint8_t retBuf[16];
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        *(void**)(s + 0x40) = *(void**)retBuf;
    }
}

// Slot6: Remove entity from world tracking set.
// [6] 0x140067090
void EntityUI::OnClosed() {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    // Copy +0x30 (entity InternalBasePtr) to local
    uint64_t entityKey = *(uint64_t*)(s + 0x30);

    // Get BST root: g_pGameWorldSingleton -> +0x120 -> +0x60
    void* worldSingleton = GameUpdateLoop::_singleton;
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

// [7] 0x140066d80
void EntityUI::OnUpdateLayout() {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

        // (risky in SA, cosmetic only); the page-selection below is what makes the
        // entity/citizen content actually render, so it always runs.

    // Find components on entity to determine page state
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
            void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
            void* descResult = nullptr;
            GetDescPage_Impl(entityDesc, &descResult);
            pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
        } else {
            pageIdx = 2;
        }
    } else {
        // Build/destroy in progress — sequential checks matching original flow
        if (pageIdx >= 3 && pageIdx <= 7) {
            pageIdx = 3;
        } else {
            // Check 2: if posComp exists, verify build is complete
            bool gotoPage2 = false;
            if (posComp != 0) {
                bool complete = BuildComplete_Impl((void*)posComp);
                if (!complete) {
                    gotoPage2 = true;
                }
            }
            // Check 3: if not redirected to page 2 and pageIdx != 0, get desc page
            if (!gotoPage2 && pageIdx != 0) {
                void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
                void* descResult = nullptr;
                GetDescPage_Impl(entityDesc, &descResult);

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
                int activePage = pagesElem ? reinterpret_cast<const UI::PageElement*>(pagesElem)->GetActivePage() : 0;  // B.3.6 bridge
                const int staleIdx = pageIdx;   // what the pre-fix port wrote back
                pageIdx = activePage;
                if (activePage != 4) {
                    pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
                }
            } else {
                pageIdx = 2;
            }
        }
    }

    void* pagesElemFinal = *(void**)(s + 0x40);
    if (pagesElemFinal) {
        reinterpret_cast<UI::PageElement*>(pagesElemFinal)->SetActivePage(pageIdx);  // B.3.6 bridge
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
    void* keyMap = ***(void****)g_pInputConfigSingletonExref;

    // Get dialog element from +0x18 -> +0x10 (dialog root element)
    void* dialogInst = *(void**)(s + 0x18);
    void* dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);

    // Read mouse axes and scale by dialog scale factor (+0xDC on element)
    short axisX = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0);  // B.3.7 bridge
    int mouseX = (int)((float)(int)axisX * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Re-read element since CheckPtr pattern accesses +0x18 again
    dialogInst = *(void**)(s + 0x18);
    dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);
    short axisY = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 1);  // B.3.7 bridge
    int mouseY = (int)((float)(int)axisY * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Get ToolUI controller from dialog
    dialogInst = *(void**)(s + 0x18);
    uint8_t ctrlBuf[16];
    ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialogInst)->GetController(ToolUI::TypeId()));  // B.3.6 bridge
    void* controller = *(void**)ctrlBuf;

    void* toolUI = nullptr;
    if (controller) {
        bool isTool = ::Object::IsDerivedFromNullSafe(controller, ToolUI::TypeId());
        if (isTool) toolUI = controller;
    }
    uint8_t toolBuf[8];
    new (toolBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(toolUI));   // B.3.3 bridge
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
    if (!reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5))  // B.3.7 bridge
        return;

    // Get dialog at mouse position
    dialogInst = *(void**)(s + 0x18);
    void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);

    uint8_t dialogAtPosBuf[16];
    ::new (dialogAtPosBuf) Object::InternalPtr<UI::DialogInstance>(reinterpret_cast<const UI::UserInterface*>(userInterface)->GetFirstContainedDialog(mouseX, mouseY));  // B.3.6 bridge
    void* dialogAtPos = *(void**)dialogAtPosBuf;

    // Check if the dialog at mouse position is this dialog, and entity exists
    dialogInst = *(void**)(s + 0x18);
    if (dialogAtPos != dialogInst || *(uintptr_t*)(s + 0x30) == 0)
        return;

    // Get current tool from player controller (IBP at +0x28, needs CheckPtr)
    void* playerController = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120);
    void* currentTool = *(void**)((uintptr_t)playerController + 0x28);

    // Check if current tool is EditTool
    bool isEditTool = ::Object::IsDerivedFromNullSafe(currentTool, EditTool::TypeId());
    void* editTool = isEditTool ? currentTool : nullptr;

    uint8_t editToolBuf[8];
    new (editToolBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(editTool));   // B.3.3 bridge

    if (*(void**)editToolBuf == nullptr) {
        // No EditTool active — deselect first, then get new EditTool
        EntityUI_SetEditMode_Impl(playerController, 0);

        // Re-read current tool after deselect (with CheckPtr)
        currentTool = *(void**)((uintptr_t)playerController + 0x28);
        isEditTool = ::Object::IsDerivedFromNullSafe(currentTool, EditTool::TypeId());
        void* newEditTool = isEditTool ? currentTool : nullptr;

        new (editToolBuf) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(newEditTool));   // B.3.3 bridge
    }

    // Select the entity in the edit tool
    void* editToolPtr = *(void**)editToolBuf;
    if (editToolPtr) {
        void* entityPtr = *(void**)(s + 0x30);
        EditTool_SelectEntity_Impl(*(void**)editToolBuf, &entityPtr);   // 0x1400fd3b0
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

// [9] 0x140067050
void EntityUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t msg, int32_t id,
                              int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    void* param2 = element;
    if (msg != 0x12) return;
    if (id == 1) return;              // close -> handled by ToolUI::Slot9
    if (id != 0x65 && id != 100) return;
    // pin (id 100) needs the ToolUI type-id to fetch the sibling controller; center
    // (id 0x65) tolerates null component type-ids (finders just return "not found").
    if (GameUpdateLoop::_singleton == nullptr) return;
    // Gate the entity read like every other EntityUI slot (4/6/7): this body reads
    // the cached datasource copy (s+0x30) for center (0x65) and toggles tracking on
    // it for pin (100). On a just-demolished building's still-open panel that +0x30
    // weak-ref dangles, so a center/pin click would deref freed memory
    // (fn_FindComponent_Character). SA_DialogEntityAlive checks the +0x28
    // datasource (of which +0x30 is a copy) for null / marked-for-removal (0x400).
    if (!SA_DialogEntityAlive(this)) return;
    (void)param2;
    if (msg != 0x12) return;

    uintptr_t s = (uintptr_t)this;
    void* playerController = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120);

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
        uintptr_t game = (uintptr_t)GameUpdateLoop::_singleton;
        void* entityPtr = *(void**)(s + 0x30);

        float focus[2] = {0.0f, 0.0f};
        bool gotPos = false;

        uint64_t modelRef = 0;
        fn_FindComponent_Character(entityPtr, &modelRef);
        if (modelRef != 0) {
            uint64_t multiRef = 0;
            fn_FindComponent_MultiObject(entityPtr, &multiRef);
            if (multiRef == 0) {
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
                Reimpl_MapGetTerrainPos(mapGrid, pos1, inXY1, '\0', nullptr);   // 0x14009f3e0

                int idx2 = gridSize * (int)posY + (int)posX;
                float inXY2[3] = { (float)(idx2 % gridSize) - halfGrid,
                                   (float)(idx2 / gridSize) - halfGrid, 0.0f };
                float pos2[3] = {0.0f, 0.0f, 0.0f};
                Reimpl_MapGetTerrainPos(mapGrid, pos2, inXY2, '\0', nullptr);   // 0x14009f3e0

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
        ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialogInst)->GetController(ToolUI::TypeId()));  // B.3.6 bridge
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = ::Object::IsDerivedFromNullSafe(controller, ToolUI::TypeId());
            if (isTool) toolUI = controller;
        }
        uint8_t toolBuf[8];
        new (toolBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(toolUI));   // B.3.3 bridge
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
            uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120);
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





// [8] 0x140066ad0
void EntityUI::OnUpdate(const System::Time& /*time*/) {
    EntityUI::OnUpdateLayout();
}


// create 0x140011270: MemAlloc(0x48, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* EntityUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EntityUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) EntityUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* EntityUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) EntityUI());
}

DEFINE_EXE_TYPE(EntityUI, "UI::DialogController")

// (original comment header retained below)
// ============================================================
// EntityUI::Shutdown (vtable +0x18) — No-op
// ============================================================
