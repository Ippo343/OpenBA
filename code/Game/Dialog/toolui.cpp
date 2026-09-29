// code/Game/Dialog/toolui.cpp
// Clean reimplementation of ToolUI panel methods.
//
// ToolUI (0x58 bytes): Active tool/entity panel container.
// Extends DialogController (0x30 base).
// Layout:
//   +0x18 = dialog instance (InternalBasePtr, from DialogController)
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x28 = data source (InternalBasePtr, from DialogController)
//   +0x30 = description ptr (BuildComponent or Entity->description)
//   +0x38 = labelTitle element (InternalBasePtr)
//   +0x40 = flags: +0x40=pinned, +0x41=dragging
//   +0x44 = drag offset X (int32)
//   +0x48 = drag offset Y (int32)
//   +0x4C = flags: +0x4C=hasUIDesc, +0x4D=drag pending
//   +0x50 = pending mouse X (int32)
//   +0x54 = pending mouse Y (int32)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "options.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Player/exe_bodies.h"   // B3R: HighlightClear_Impl
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogControllerConfig.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TabElement.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ElementDescription.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "input/Input.h"  // ENDGAME B.3.7 callers (K3)
#include "Component/ComponentDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Component/ToolbarDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Component/UIDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/EntityUI.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/exe_bodies.h"
#include "Dialog/ToolUI.h"
#include "Component/Description.h"
#include "Game/PlayerToolManager.h"
#include "Player/Tool.h"
#include "Player/EditTool.h"

// code/Game/Object/factory_registration.cpp

// Forward declaration — defined in entityui.cpp
extern "C" bool EditTool_IsEditTool(void* tool);   // code/Game/Player/edittool.cpp
extern void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                        uint32_t x, uint32_t y);

// ============================================================
// UIDesc_AddControllers_Impl (was FUN_14012e100)
// ============================================================
// Iterates the controller entries in a UIDescription and registers each one
// with the dialog instance. Each entry is 0x28 bytes:
//   +0x00: uint32_t controllerTypeId
//   +0x08: ExternalPtr<ElementDescription> elementDesc
//   +0x10: ExternalPtr<DialogControllerConfig> config
//   +0x18: ExternalPtr<ElementDescription> tabDesc
//   +0x20: System::String elementName (8 bytes)
//
// For each entry, looks up the named element in the dialog. If the element
// is a TabElement, creates a new tab page via AddTab and uses the 5-arg
// AddController overload with the tab page. Otherwise, uses the 6-arg
// AddController overload with the element description and name string.
//
// UIDescription layout:
//   +0x30: void* entries array pointer
//   +0x38: int entry count

// WAVE 10 LANE UIORACLE — see the census note inside the loop below.
uint64_t g_uidescEntries = 0, g_uidescFound = 0, g_uidescTab = 0;

extern "C" void UIDescTabCensusDump() {
    fprintf(stderr,
        "[UIDescCensus] controller entries seen=%llu  named-element found=%llu  "
        "TAB PATH (calls AddTab)=%llu%s\n",
        (unsigned long long)g_uidescEntries,
        (unsigned long long)g_uidescFound,
        (unsigned long long)g_uidescTab,
        g_uidescEntries == 0
            ? "   <- UIDesc_AddControllers_Impl NEVER RAN on this route"
            : (g_uidescTab == 0
                 ? "   <- ran, but this route builds NO tabs: AddTab is unreachable here"
                 : ""));
    fflush(stderr);
}

// Registered at static-init, NOT on first entry to the function below. That
// distinction is the whole point: an atexit armed by the first call could never
// print the "NEVER RAN" case, which is exactly the case worth reporting.
namespace { struct UIDescCensusReg { UIDescCensusReg() { std::atexit(&UIDescTabCensusDump); } };
            UIDescCensusReg g_uidescCensusReg; }

void UIDesc_AddControllers_Impl(void* uiDesc, void* dialogInstPtr, void* dataSource) {
    uintptr_t desc = (uintptr_t)uiDesc;
    int count = *(int*)(desc + 0x38);
    if (count <= 0) return;

    uintptr_t entries = *(uintptr_t*)(desc + 0x30);
    if (Opt().mapTrace) {
        printf("[UIDescAdd] uiDesc=%p count=%d entries=%p dataSource=%p\n",
               uiDesc, count, (void*)entries, dataSource);
        for (int j = 0; j < count && j < 4; j++) {
            uintptr_t e = entries + (uintptr_t)j * 0x28;
            uint64_t nameStrVal = *(uint64_t*)(e + 0x20);
            printf("[UIDescAdd]   entry[%d]@%p type=%u elemDesc=%p config=%p tabDesc=%p "
                   "nameStrPtr=0x%llx\n", j, (void*)e, *(uint32_t*)e,
                   *(void**)(e + 0x08), *(void**)(e + 0x10), *(void**)(e + 0x18),
                   (unsigned long long)nameStrVal);
        }
        fflush(stdout);
    }

    // WAVE 10 LANE UIORACLE — a REACHABILITY census, not a trace.
    //
    // `?AddTab@TabElement@UI@@` is ported and registered, and this loop is its
    // ONLY caller in the process (the DLL never calls its own AddTab — four
    // xrefs, not one a CALL). Its predictive oracle nevertheless reported
    // rounds=0 on every route driven, and `rounds=0` reads identically to "no
    // route exists" and to "the port is dead"
    // (reference_uncalled_port_is_not_verified.md). These three counters
    // separate those: they say how many controller entries this loop saw, how
    // many named an element at all, and how many took the TAB path. A route
    // that reports entries>0 and tab==0 has been MEASURED not to build tabs;
    // one that reports entries==0 never ran this function at all.
    //
    // Free when off; dumped by UIDescTabCensusDump() under BANISHED_UIDESC_CENSUS.
    extern uint64_t g_uidescEntries, g_uidescFound, g_uidescTab;
    for (int i = 0; i < count; i++) {
        ++g_uidescEntries;
        uintptr_t entry = entries + (uintptr_t)i * 0x28;

        // Look up the named element in the dialog
        void* dialogInst = *(void**)dialogInstPtr;

        void* foundElement = nullptr;
        void* nameStr = (void*)(entry + 0x20);
        ::new (&foundElement) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(*reinterpret_cast<const System::String*>(nameStr)));  // B.3.6 bridge

        if (foundElement != nullptr) ++g_uidescFound;

        if (foundElement == nullptr) {
            // No matching element found — use 6-arg AddController (non-tab path)
            uint32_t typeId = *(uint32_t*)entry;
            dialogInst = *(void**)dialogInstPtr;
            reinterpret_cast<UI::DialogInstance*>(dialogInst)->AddController(  // B.3.6 bridge
                typeId,
                reinterpret_cast<Object::Base*>(dataSource),
                Object::ExternalPtr<UI::DialogControllerConfig>(
                    reinterpret_cast<UI::DialogControllerConfig*>(*(void**)(entry + 0x10))),   // controllerConfig
                Object::ExternalPtr<UI::ElementDescription>(
                    reinterpret_cast<UI::ElementDescription*>(*(void**)(entry + 0x08))),     // elementDesc
                *reinterpret_cast<const System::String*>(entry + 0x20));    // nameStr
            continue;
        }

        // Check if the found element is a TabElement
        bool isTabElement = reinterpret_cast<const ::Object::Base*>(foundElement)->IsA(
            UI::TabElement::TypeId());   // B.3.x bridge (?IsA@Base@Object@@)

        if (!isTabElement) {
            // Not a tab — use 6-arg AddController (non-tab path)
            uint32_t typeId = *(uint32_t*)entry;
            dialogInst = *(void**)dialogInstPtr;
            reinterpret_cast<UI::DialogInstance*>(dialogInst)->AddController(  // B.3.6 bridge
                typeId,
                reinterpret_cast<Object::Base*>(dataSource),
                Object::ExternalPtr<UI::DialogControllerConfig>(
                    reinterpret_cast<UI::DialogControllerConfig*>(*(void**)(entry + 0x10))),   // controllerConfig
                Object::ExternalPtr<UI::ElementDescription>(
                    reinterpret_cast<UI::ElementDescription*>(*(void**)(entry + 0x08))),     // elementDesc
                *reinterpret_cast<const System::String*>(entry + 0x20));    // nameStr
            continue;
        }

        // Element is a TabElement — cast via IsDerivedFrom and use tab path
        ++g_uidescTab;
        bool derived = ::Object::IsDerivedFromNullSafe(foundElement, UI::TabElement::TypeId());
        void* tabElement = derived ? foundElement : nullptr;

        // Wrap in InternalBasePtr
        void* tabPtr = nullptr;
        new (&tabPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(tabElement));   // B.3.3 bridge

        // Create a new tab page: AddTab(tabElement, retval, tabDesc, elementDesc)
        // Non-trivial return ABI: this=RCX, retval=RDX, tabDesc=R8, elementDesc=R9
        void* tabPage = nullptr;
        ::new (&tabPage) Object::InternalPtr<UI::Element>(  // B.3.6 bridge
            reinterpret_cast<UI::TabElement*>(tabPtr)->AddTab(   // this (TabElement)
                Object::ExternalPtr<UI::ElementDescription>(
                    reinterpret_cast<UI::ElementDescription*>(*(void**)(entry + 0x18))),  // tabDesc
                Object::ExternalPtr<UI::ElementDescription>(
                    reinterpret_cast<UI::ElementDescription*>(*(void**)(entry + 0x08))))); // elementDesc

        // Use 5-arg AddController with the tab page element
        uint32_t typeId = *(uint32_t*)entry;
        dialogInst = *(void**)dialogInstPtr;
        reinterpret_cast<UI::DialogInstance*>(dialogInst)->AddController(  // B.3.6 bridge
            typeId,
            reinterpret_cast<Object::Base*>(dataSource),
            Object::ExternalPtr<UI::DialogControllerConfig>(
                reinterpret_cast<UI::DialogControllerConfig*>(*(void**)(entry + 0x10))),  // controllerConfig
            Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tabPage)));   // tab page element (the door read it through &tabPage)
    }
}

// ---- ToolUI: PROMOTED to the real Slot4 reimpl (map-render bring-up) ----------
// ToolUI is StandardDialog.rsc's controller — created for EVERY StandardDialog-
// based utility dialog (Map/Help/Goal/…). Its OnCreated (+0x20) reads the
// dialog's data source (a ComponentEntity → its description), finds the title/
// close/drag elements, and — crucially — searches the description for a
// UIDescription and calls UIDesc_AddControllers_Impl, which injects each of the
// UIDescription's sub-controllers (for UtilityMap: the MapUI controller +
// Dialog/Map.rsc:content = imageMap/imageOverlay/frustum) at their _insertAt
// element ("userGroup0") via DialogInstance::AddController. OnElementEvent (+0x48)
// closes the dialog on the buttonClose (element id 1, msg 0x12), brings it to the
// front on focus (msg 8..10) and arms a drag (msg 2); OnUpdate (+0x40, dispatched
// every frame by DialogInstance::OnUpdate 0x180115400) follows the drag and
// writes the new position into the tool manager's +0x50 set.
//
// ENDGAME B.3.9 CG2: the exe-.rdata type-id globals are the class statics
// (ComponentDescription::TypeId() (cell 0x1401c7318), ComponentEntity::TypeId(),
// ToolbarDescription::TypeId() (cell 0x1401c7f58), UIDescription::TypeId(),
// UI::TabElement::TypeId()).

// The description's first sub-description of class T (IsDerivedFrom-checked),
// the loop OnCreated runs twice: `found` says whether a T-typed entry was met.
template <class T>
static T* ToolUI_FindSubDescription(ComponentDescription* desc, bool& found) {
    found = false;
    for (int i = 0; i < desc->_subDescCount; i++) {
        Description* entry = desc->_subDescs[i];
        if ((uint32_t)entry->Type() == T::TypeId()) {
            found = true;
            if (::Object::IsDerivedFromNullSafe(entry, T::TypeId()) && entry != nullptr)
                return static_cast<T*>(entry);
            return nullptr;
        }
    }
    return nullptr;
}

// The named element of the owning dialog.
static UI::Element* ToolUI_FindElement(UI::DialogInstance* dialog, const char* name) {
    System::String nameStr(name);
    return dialog->FindElement(nameStr).Get();
}

// ============================================================
// ToolUI::OnCreated (vtable +0x20) — Setup/populate tool panel
// ============================================================
// Checks data source type: if a ComponentDescription, stores it at +0x30.
// If ComponentEntity, stores entity->description at +0x30.
// Finds "labelTitle" element and sets title text from ToolbarDescription.
// Finds "buttonClose" and sets ID 1.
// Finds "borderTitle" and configures drag handle (flags 0x10000, ID 0).
// Searches description for UIDescription and registers sub-controllers.
//
// Original: 0x14007BD70.

// [4] 0x14007bd70
void ToolUI::OnCreated() {
    static bool s_logged=false;
    if(!s_logged && Opt().mapTrace){ s_logged=true;
        printf("[ToolUI::Slot4] dispatched self=%p ds(+0x28)=%p\n",
               static_cast<void*>(this), static_cast<void*>(_dataSource)); fflush(stdout); }

    // Determine description source from the data source
    Object::Base* dataSource = _dataSource;
    bool isDescription = ::Object::IsDerivedFromNullSafe(dataSource, ComponentDescription::TypeId());

    if (isDescription && dataSource != nullptr) {
        // Data source IS the description — check again and store
        Object::Base* ds2 = _dataSource;
        bool check = ::Object::IsDerivedFromNullSafe(ds2, ComponentDescription::TypeId());
        description = check ? static_cast<ComponentDescription*>(ds2) : nullptr;
    } else {
        // Check if data source is a ComponentEntity
        Object::Base* ds2 = _dataSource;
        bool isEntity = ::Object::IsDerivedFromNullSafe(ds2, ComponentEntity::TypeId());
        if (isEntity && ds2 != nullptr) {
            Object::Base* ds3 = _dataSource;
            bool check = ::Object::IsDerivedFromNullSafe(ds3, ComponentEntity::TypeId());
            ComponentEntity* entity = check ? static_cast<ComponentEntity*>(ds3) : nullptr;
            // Store the entity's description
            description = entity->_description;
        }
    }

    // If the data source resolved to no description (still null), there is
    // nothing to populate — the title/UIDescription sections below read the
    // description unconditionally, so bail. In the exe the data source is always a
    // description or a ComponentEntity so it is set; this guards the
    // standalone case where a raw description could reach here.
    if (description == nullptr) return;

    // Find "labelTitle" element via DialogInstance
    labelTitle = static_cast<UI::TextElement*>(ToolUI_FindElement(_dialog, "labelTitle"));

    // Set title text from ToolbarDescription in description
    if (labelTitle != nullptr) {
        bool found = false;
        ToolbarDescription* toolbar = ToolUI_FindSubDescription<ToolbarDescription>(description, found);
        if (toolbar != nullptr) {
            // UI::String from the entry's string table and title
            UI::String title(Object::ExternalPtr<UI::StringTable>(toolbar->_stringTable), toolbar->_str1);
            labelTitle->SetText(&title);
        }
    }

    // Find "buttonClose" element, set ID 1
    {
        UI::Element* closeBtn = ToolUI_FindElement(_dialog, "buttonClose");
        if (closeBtn != nullptr) {
            closeBtn->SetElementId(1);
        }
    }

    // Find "borderTitle" element — drag handle
    {
        UI::Element* borderTitle = ToolUI_FindElement(_dialog, "borderTitle");
        if (borderTitle == nullptr) {
            pinned = 0;
        } else {
            borderTitle->SetElementId(0);
            borderTitle->SetFlagsOnOff(0x10000, 0);
            // retail: one word store of 1 at +0x40 (pinned = 1, dragging = 0)
            pinned = 1;
            dragging = 0;
        }
    }

    // Clear the drag-to-pin flag
    pinOnDrag = 0;

    // Search description for UIDescription and register sub-controllers
    {
        if (Opt().mapTrace)
            printf("[ToolUI_Slot4] desc(self+0x30)=%p descCount=%d uiDescTid=%u\n",
                   static_cast<void*>(description), description->_subDescCount, UIDescription::TypeId());

        bool found = false;
        UIDescription* uiDesc = ToolUI_FindSubDescription<UIDescription>(description, found);
        if (!found || uiDesc == nullptr) return;

        // Register sub-controllers from UIDescription
        UIDesc_AddControllers_Impl(uiDesc, &_dialog, _dataSource);
    }
}

// Dialog-side "is this an entity info panel?" — probes the DIALOG for an
// EntityUI controller instead of type-checking the datasource entity. During
// demolition teardown (upgrade path, destroy SM state 4->5) the datasource
// pointer at controller+0x28 stays non-null while the entity's class-info is
// already torn down, so ::Object::IsDerivedFromNullSafe(datasource, ...) reads a nulled
// typeinfo (+0x10) inside Runtime — AV'd live 2026-07-20. Retail never probes
// the datasource in per-frame dialog slots; its drag-arm (EntityUI::Slot8,
// 0x140066AD0) is scoped by construction: EntityUI exists only on entity
// dialogs. This helper reproduces that scoping dialog-side, entity untouched.
// The address window is the tree's guard (a stale dialog pointer), not retail's.
static bool ToolUI_DialogHasEntityUI(const UI::DialogInstance* dialog) {
    static uint32_t s_tidEntityUI = 0;
    if (!s_tidEntityUI) s_tidEntityUI = EntityUI::TypeId();
    if (!s_tidEntityUI) return false;
    uintptr_t di = reinterpret_cast<uintptr_t>(dialog);  // CAST-OK: ptr_int
    if (di < 0x10000 || di > 0x00007FFFFFFFFFFFULL) return false;
    return dialog->GetController(s_tidEntityUI).Get() != nullptr;
}

// The mouse position in UI units (KeyMap axes 0 / 1 scaled by the dialog's
// UserInterface _uiScale), as OnUpdate reads it.
static int ToolUI_MouseX(const Input::KeyMap* keyMap, const UI::DialogInstance* dialog) {
    short axisX = keyMap->GetValue(0, 0);
    float scaleX = dialog->_ui->_uiScale;
    return (int)((float)(int)axisX * scaleX);
}
static int ToolUI_MouseY(const Input::KeyMap* keyMap, const UI::DialogInstance* dialog) {
    short axisY = keyMap->GetValue(0, 1);
    float scaleY = dialog->_ui->_uiScale;
    return (int)((float)(int)axisY * scaleY);
}

// The node of the tool manager's +0x50 set keyed by `key` (the ToolUI's
// description; the set also holds entity keys: the compare is by address).
static TrackedEntityNode* ToolUI_FindTracked(const void* key) {
    TrackedEntityNode* node = GameUpdateLoop::_singleton->m_toolManager->field_0x50;
    while (node != nullptr) {
        const void* nodeKey = node->key;
        if (key < nodeKey) {
            node = node->left;
        } else if (key > nodeKey) {
            node = node->right;
        } else {
            return node;
        }
    }
    return nullptr;
}

// ============================================================
// ToolUI::OnUpdate (vtable +0x40) — Mouse drag handler
// ============================================================
// Handles two drag modes:
// 1. Pending drag (+0x4D): waits for mouse to move >5px, then starts drag.
// 2. Active drag (+0x41): moves dialog to follow mouse, updates entity
//    tracking BST position. Stops when mouse button released.
//
// Original: 0x14007B9B0.

// [8] 0x14007b9b0
void ToolUI::OnUpdate(const System::Time& /*time*/) {
    // Per-frame update (+0x40). Runtime DialogInstance::OnUpdate dispatches
    // (controller, Time&); the body ignores the Time arg. Idle cost is two flag
    // reads (+0x41/+0x4D, both 0 unless OnElementEvent armed a drag). Guard the
    // KeyMap exref + the Runtime fns the drag path dereferences unconditionally.
    if (Input::Configuration::_singleton == nullptr
        || GameUpdateLoop::_singleton == nullptr) return;

    // --- Standalone drag-enable for entity / citizen info panels ---------------
    // Retail arms the entity panel's title-bar drag through EntityUI's per-frame
    // input handler (exe 0x140066AD0 EntityUI::Slot8), which UNCONDITIONALLY sets
    // this ToolUI controller's pinOnDrag = 1 every frame. In standalone that
    // EntityUI input slot is not dispatched per frame, so pinOnDrag is never set
    // and the entity/citizen panel can never arm a drag. Utility dialogs are
    // unaffected: they drag via the borderTitle handle (pinned, set in OnCreated)
    // and their datasource is a ComponentDescription, not a ComponentEntity.
    //
    // Scope it DIALOG-side — "does this dialog carry an EntityUI controller?" via
    // GetController — never by type-checking the datasource entity (a datasource
    // mid-teardown has a nulled class-info; crashed live 2026-07-20).
    bool entityDialog = ToolUI_DialogHasEntityUI(_dialog);
    if (entityDialog)
        pinOnDrag = 1;

    // Pending drag: check mouse movement threshold
    if (isDragPending != 0) {
        const Input::KeyMap* keyMap = Input::Configuration::_singleton->_keyMap;

        int mouseX = ToolUI_MouseX(keyMap, _dialog);
        int mouseY = ToolUI_MouseY(keyMap, _dialog);

        int deltaX = mouseX - dragStartX;
        int deltaY = mouseY - dragStartY;
        if (deltaX < 0) deltaX = -deltaX;
        if (deltaY < 0) deltaY = -deltaY;

        if (deltaX > 5 || deltaY > 5) {
            // Start drag: calc offset from dialog origin
            isDragPending = 0;
            pinned = 1;

            // FUN_14007b940 inline: set dragging flag, calc offsets
            dragging = 1;
            dragOffsetX = dragStartX - _dialog->_upperLeftX;
            dragOffsetY = dragStartY - _dialog->_upperLeftY;
        }

        // Check if mouse button released
        short btn = keyMap->GetValue(0, 5);
        if (btn == 0) {
            isDragPending = 0;
        }
    }

    // Active drag: move dialog to follow mouse
    if (dragging != 0 && pinned != 0) {
        const Input::KeyMap* keyMap = Input::Configuration::_singleton->_keyMap;

        if (!_dialog) { /* dialog instance null, skip drag */ }
        else {
        int newX = ToolUI_MouseX(keyMap, _dialog) - dragOffsetX;
        int newY = ToolUI_MouseY(keyMap, _dialog) - dragOffsetY;
        if (newX < 0) newX = 0;
        if (newY < 0) newY = 0;

        // Move dialog
        _dialog->SetUpperLeft(newX, newY, false);

        // Update the tracked position keyed by this panel's description
        TrackedEntityNode* node = ToolUI_FindTracked(description);
        if (node != nullptr) {
            node->x = newX;
            node->y = newY;
        }

        // If entity is a ComponentEntity, update via SelectEntity.
        // Same landmine as the drag-arm block above: IsDerivedFrom on a datasource
        // whose entity is mid-teardown reads nulled class-info. Only reachable
        // mid-drag, but a demolition can complete during a drag — gate on the
        // dialog-side entityDialog fact (so description datasources are never
        // probed) and on SA_DialogEntityAlive (weak-ref null / 0x400 marked), which
        // is only meaningful/safe once we know the datasource is an entity.
        Object::Base* dataSource = _dataSource;
        bool isEntity = entityDialog && dataSource != nullptr &&
                        SA_DialogEntityAlive(this) &&
                        ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
        if (isEntity && dataSource != nullptr) {
            Object::Base* ds2 = _dataSource;
            bool check = ::Object::IsDerivedFromNullSafe(ds2, ComponentEntity::TypeId());
            // The InternalPtr<ComponentEntity> retail hands 0x140102e30 by address.
            ComponentEntity* entity = check ? static_cast<ComponentEntity*>(ds2) : nullptr;

            EntityUI_SelectEntity_Impl(GameUpdateLoop::_singleton->m_toolManager,
                                       &entity, newX, newY);
        }

        // Check if mouse button released
        short btn = keyMap->GetValue(0, 5);
        if (btn == 0) {
            dragging = 0;
        }
        } // end else (dialog instance not null)
    }
}

// ============================================================
// ToolUI::OnElementEvent (vtable +0x48) — Event handler
// ============================================================
// Handles:
//   msg=2, id=0: mouse down — start drag or pending drag
//   msg=5: mouse up — cancel drag state
//   msg=8..10: focus events — bring dialog to front
//   msg=0x12, id=1: close button — close dialog, remove from tracking
//
// Original: 0x14007BC30.

// [9] 0x14007bc30
void ToolUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t msg, int32_t id,
                            int32_t mouseX, int32_t mouseY, int32_t /*e*/, void* /*userData*/) {
    if (msg == 2) {
        if (id == 0) {
            if (pinned != 0) {
                // Already pinned — start drag immediately
                // FUN_14007b940 inline
                dragging = 1;
                dragOffsetX = mouseX - _dialog->_upperLeftX;
                dragOffsetY = mouseY - _dialog->_upperLeftY;
                return;
            }

            if (pinOnDrag != 0) {
                // Unpinned but pinnable — start a pending drag
                isDragPending = 1;
                dragStartX = mouseX;
                dragStartY = mouseY;
                return;
            }
        }
    } else if (msg == 5) {
        // Mouse up — cancel both drag modes
        dragging = 0;
        isDragPending = 0;
        return;
    } else if (msg > 7 && msg < 0x0B) {
        // Focus events (8, 9, 10) — bring dialog to front
        Object::InternalPtr<UI::DialogInstance> dialogRef(_dialog);
        _dialog->_ui->BringDialogToFront(&dialogRef);
        return;
    } else if (msg == 0x12 && id == 1) {
        // Close button — close dialog and remove from entity tracking
        // Before closing an ENTITY info panel, drop the selecting EditTool's stale
        // dialog reference. Retail tears the tool down on close (EntityUI::Slot9 ->
        // SetEditMode(0)); that slot is stubbed in standalone, so otherwise the
        // EditTool keeps ticking on the now-freed dialog and faults next frame
        // inside GetController. Clearing the tool's selection makes its tick
        // early-out. Scoped to entity dialogs (this ToolUI's datasource is a
        // ComponentEntity) so utility dialogs (map/help) that share this close path
        // don't lose the user's active tool. Entity-ness established dialog-side +
        // aliveness-gated before the IsDerivedFrom probe (see
        // ToolUI_DialogHasEntityUI). On a dead entity the ref-drop is skipped and
        // the dialog still closes.
        if (GameUpdateLoop::_singleton != nullptr &&
            ToolUI_DialogHasEntityUI(_dialog) && SA_DialogEntityAlive(this)) {
            Object::Base* ds = _dataSource;
            if (ds && ::Object::IsDerivedFromNullSafe(ds, ComponentEntity::TypeId())) {
                PlayerToolManager* pc = GameUpdateLoop::_singleton->m_toolManager;
                if (pc) {
                    Tool* activeTool = pc->currentTool;
                    // ONLY an EditTool keeps a selected entity at +0x30. When a build
                    // tool is armed instead, that offset is DragTool's endCell /
                    // lastEndCell int pair, and cb40 would hand those two ints to the
                    // component finder as an entity pointer (measured 2026-08-19),
                    // while the two stores below would clobber the tool's drag state.
                    // A build tool has no selection to clear, so skip the whole block.
                    if (activeTool && EditTool_IsEditTool(activeTool)) {
                        // Deselect like retail cc90 steps (1)+(2): clear the
                        // selection highlight (reads the entity from the tool's
                        // selection, so call it BEFORE nulling), then drop both the
                        // dialog ref and the selected-entity ref.
                        HighlightClear_Impl(activeTool);   // 0x1400fcb40
                        EditTool* editTool = static_cast<EditTool*>(activeTool);
                        editTool->entityRef = 0;
                        editTool->lastEntityRef = 0;
                    }
                }
            }
        }
        // Standalone dialogs/controllers are only partially initialized. Validate the
        // pointers this path dereferences before use so a bad field can't fault: the
        // world singleton (walked for the tracking BST below) and the dialog-instance
        // pointer (deref'd for its UserInterface). The address window matches
        // CitUI_OkPtr (canonical Win64 user-space heap window).
        if (GameUpdateLoop::_singleton == nullptr)
            return;
        {
            uintptr_t di = reinterpret_cast<uintptr_t>(_dialog);  // CAST-OK: ptr_int
            if (di < 0x10000 || di > 0x00007FFFFFFFFFFFULL) return;
        }
        Object::InternalPtr<UI::DialogInstance> dialogRef(_dialog);
        _dialog->_ui->CloseDialog(&dialogRef);

        // Clear the tracked entry's active flag
        TrackedEntityNode* node = ToolUI_FindTracked(description);
        if (node != nullptr) {
            node->active = 0;
            return;
        }
    }
}
