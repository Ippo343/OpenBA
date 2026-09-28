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
#include "engine_funcs.h"
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
// based utility dialog (Map/Help/Goal/…). Its Slot4 (+0x20, Setup) reads the
// dialog's data source (a ComponentEntity → its description), finds the title/
// close/drag elements, and — crucially — searches the description for a
// UIDescription and calls UIDesc_AddControllers_Impl, which injects each of the
// UIDescription's sub-controllers (for UtilityMap: the MapUI controller +
// Dialog/Map.rsc:content = imageMap/imageOverlay/frustum) at their _insertAt
// element ("userGroup0") via DialogInstance::AddController6. The generic no-op
// stub left Slot4 dead, so NO utility dialog ever built its inner content — the
// minimap was never instantiated. Wiring Slot4 to the real reimpl runs the whole
// injection chain. Slot9 (+0x48, event) is now wired too so the close (X) button
// works, and Slot8 (+0x40, per-frame drag-follow) so the title-bar drag actually
// moves the dialog: Runtime DialogInstance::OnUpdate (0x180115400) calls every
// controller's vtable+0x40 with (controller, Time&) each frame; Slot9's msg=2
// drag-init only sets the flags, Slot8 does the SetUpperLeft follow + writes the
// new position into the utility-dialog position BST (gameSubsys+0x50).
// Slot9 (+0x48) is ToolUI's OnElementEvent handler. It contains the buttonClose
// (element id 1, msg 0x12) -> UserInterface::CloseDialog logic that dismisses the
// dialog, plus focus (msg 8..10 -> BringDialogToFront) and drag-init (msg 2).
// DialogInstance::OnElementEvent (Runtime 0x180114ed0) dispatches every element
// event to each controller's vtable+0x48, so leaving this a no-op meant the map/
// Help/etc. dialogs' X button (and every close button) did nothing.
namespace toolui_sa {
    // ENDGAME B.3.9 CG2: ToolUI_Slot4 + UIDesc_AddControllers_Impl's exe-.rdata
    // type-id globals -> the class statics (ComponentDescription::TypeId() (cell
    // 0x1401c7318 — the engine_funcs label matches, verified by
    // ComponentDescription_Register), ComponentEntity::TypeId(),
    // ToolbarDescription::TypeId() (cell 0x1401c7f58), UIDescription::TypeId(),
    // UI::TabElement::TypeId()). All always answer, so EnsureTypeIds's lazy-fill
    // retired; the empty function and its call went with B3R.
    // Per-frame update (+0x40). Runtime DialogInstance::OnUpdate dispatches
    // (controller, Time&); the reimpl ignores the Time arg. Idle cost is two flag
    // reads (+0x41/+0x4D, both 0 unless Slot9 armed a drag). Guard the KeyMap
    // exref + the Runtime fns the drag path dereferences unconditionally.
    // OnElementEvent (+0x48). Dispatched with (self, &elementPtr, msg, id, x, y, ...);
    // the real handler closes the dialog on the buttonClose (id 1, msg 0x12).
}

// ============================================================
// ToolUI::Slot4 (vtable +0x20) — Setup/populate tool panel
// ============================================================
// Checks data source type: if BuildComponent, stores directly at +0x30.
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
               (void*)this, *reinterpret_cast<void**>((char*)this+0x28)); fflush(stdout); }
    uintptr_t s = (uintptr_t)this;

    // Determine description source from +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isBuildComp = ::Object::IsDerivedFromNullSafe(dataSource, ComponentDescription::TypeId());

    if (isBuildComp && dataSource != nullptr) {
        // Data source IS the description — check again and store
        void* ds2 = *(void**)(s + 0x28);
        bool check = ::Object::IsDerivedFromNullSafe(ds2, ComponentDescription::TypeId());
        *(void**)(s + 0x30) = check ? ds2 : nullptr;
    } else {
        // Check if data source is a ComponentEntity
        void* ds2 = *(void**)(s + 0x28);
        bool isEntity = ::Object::IsDerivedFromNullSafe(ds2, ComponentEntity::TypeId());
        if (isEntity && ds2 != nullptr) {
            void* ds3 = *(void**)(s + 0x28);
            bool check = ::Object::IsDerivedFromNullSafe(ds3, ComponentEntity::TypeId());
            void* entity = check ? ds3 : nullptr;
            // Store entity's description (+0x28) at self+0x30
            *(void**)(s + 0x30) = *(void**)((uintptr_t)entity + 0x28);
        }
    }

    // If the data source resolved to no description (+0x30 still null), there is
    // nothing to populate — the title/UIDescription sections below dereference
    // desc+0x30 unconditionally, so bail. In the exe the data source is always a
    // BuildComponent or a ComponentEntity so +0x30 is set; this guards the
    // standalone case where a raw description could reach here.
    if (*(void**)(s + 0x30) == nullptr) return;

    // Find "labelTitle" element via DialogInstance
    {
        uint8_t retBuf[16];
        System::String nameStr("labelTitle");
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        *(void**)(s + 0x38) = *(void**)retBuf;
    }

    // Set title text from ToolbarDescription in description
    if (*(void**)(s + 0x38) != nullptr) {
        void* desc = *(void**)(s + 0x30);
        int descCount = *(int*)((uintptr_t)desc + 0x30);
        uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);

        for (int i = 0; i < descCount; i++) {
            void* entry = *(void**)(descArray + (uintptr_t)i * 8);
            // Get TypeId via vtable[2]
            int typeId = reinterpret_cast<Object::Base*>(entry)->Type();  // B.3.8 bridge

            if (typeId == (int)ToolbarDescription::TypeId()) {
                bool check = ::Object::IsDerivedFromNullSafe(entry, ToolbarDescription::TypeId());
                if (check && entry != nullptr) {
                    // Create UI::String from entry's string table (+0x30) and name (+0x38)
                    uint8_t uiStrBuf[16];
                    ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)entry + 0x30))),
                                     *reinterpret_cast<const System::String*>((uintptr_t)entry + 0x38));  // B.3.6 bridge
                    // Set text on labelTitle element
                    reinterpret_cast<UI::TextElement*>(*(void**)(s + 0x38))->SetText(reinterpret_cast<const UI::String*>(uiStrBuf));  // B.3.6 bridge
                }
                break;
            }
        }
    }

    // Find "buttonClose" element, set ID 1
    {
        uint8_t retBuf[16];
        System::String nameStr("buttonClose");
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        void* closeBtn = *(void**)retBuf;

        if (closeBtn != nullptr) {
            reinterpret_cast<UI::Element*>(closeBtn)->SetElementId(1);  // B.3.6 bridge
        }
    }

    // Find "borderTitle" element — drag handle
    {
        void* borderTitle = nullptr;
        uint8_t retBuf[16];
        System::String nameStr("borderTitle");
        void* dialogInst = *(void**)(s + 0x18);
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        borderTitle = *(void**)retBuf;

        if (borderTitle == nullptr) {
            *(uint8_t*)(s + 0x40) = 0;
        } else {
            reinterpret_cast<UI::Element*>(borderTitle)->SetElementId(0);  // B.3.6 bridge
            reinterpret_cast<UI::Element*>(borderTitle)->SetFlagsOnOff(0x10000, 0);  // B.3.6 bridge
            *(uint16_t*)(s + 0x40) = 1;
        }
    }

    // Clear drag-pending flag
    *(uint8_t*)(s + 0x4C) = 0;

    // Search description for UIDescription and register sub-controllers
    {
        void* desc = *(void**)(s + 0x30);
        int descCount = *(int*)((uintptr_t)desc + 0x30);
        uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);
        if (Opt().mapTrace)
            printf("[ToolUI_Slot4] desc(self+0x30)=%p descCount=%d uiDescTid=%u\n",
                   desc, descCount, UIDescription::TypeId());

        for (int i = 0; i < descCount; i++) {
            void* entry = *(void**)(descArray + (uintptr_t)i * 8);
            int typeId = reinterpret_cast<Object::Base*>(entry)->Type();  // B.3.8 bridge
            if (Opt().mapTrace)
                printf("[ToolUI_Slot4]   descEntry[%d]=%p tid=%d\n", i, entry, typeId);

            if (typeId == (int)UIDescription::TypeId()) {
                bool check = ::Object::IsDerivedFromNullSafe(entry, UIDescription::TypeId());
                if (!check || entry == nullptr) return;

                // Register sub-controllers from UIDescription
                void* dialogInstPtr = (void*)(s + 0x18);
                UIDesc_AddControllers_Impl(entry, dialogInstPtr,
                                           *(void**)(s + 0x28));
                return;
            }
        }
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
static bool ToolUI_DialogHasEntityUI(uintptr_t s) {
    static uint32_t s_tidEntityUI = 0;
    if (!s_tidEntityUI) s_tidEntityUI = EntityUI::TypeId();
    if (!s_tidEntityUI) return false;
    uintptr_t di = *(uintptr_t*)(s + 0x18);
    if (di < 0x10000 || di > 0x00007FFFFFFFFFFFULL) return false;
    uint64_t ctrlBuf = 0;
    ::new (&ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(di)->GetController(s_tidEntityUI));  // B.3.6 bridge
    return ctrlBuf != 0;
}

// ============================================================
// ToolUI::Slot8 (vtable +0x40) — Mouse drag handler
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
    // reads (+0x41/+0x4D, both 0 unless Slot9 armed a drag). Guard the KeyMap
    // exref + the Runtime fns the drag path dereferences unconditionally.
    if (Input::Configuration::_singleton == nullptr
        || GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t s = (uintptr_t)this;

    // --- Standalone drag-enable for entity / citizen info panels ---------------
    // Retail arms the entity panel's title-bar drag through EntityUI's per-frame
    // input handler (exe 0x140066AD0 EntityUI::Slot8), which UNCONDITIONALLY sets
    // this ToolUI controller's +0x4C (drag-enable) = 1 every frame. In standalone
    // that EntityUI input slot is a StubNop — its +0x40 per-frame dispatch runs the
    // page-switching Slot7 instead — so +0x4C is never set and the entity/citizen
    // panel can never arm a drag (Slot9's mouse-down falls through both the +0x40
    // borderTitle branch and the +0x4C pending branch). Utility dialogs are
    // unaffected: they drag via the borderTitle handle (+0x40, set in Slot4) and
    // their datasource is a ComponentDescription, not a ComponentEntity.
    //
    // Mirror EntityUI's drag-enable here, in ToolUI's OWN per-frame slot (this
    // function is g_vt[8], dispatched every frame by DialogInstance::OnUpdate), but
    // only for entity dialogs. This arms the pending-drag path that Slot9's
    // mouse-down triggers and this function then follows.
    //
    // Scope it DIALOG-side — "does this dialog carry an EntityUI controller?" via
    // GetController — never by type-checking the datasource entity. Retail's own
    // drag-arm (EntityUI::Slot8, 0x140066AD0) works the same way: it exists only on
    // entity dialogs and sets ToolUI+0x4C without ever touching the entity. The
    // previous IsDerivedFrom(*(s+0x28)) here crashed live 2026-07-20: during the
    // upgrade-path demolition teardown (state-4 "storage drained -> 5") the entity
    // pointer at +0x28 is still non-null while its class-info is already torn
    // down, so IsDerivedFrom read a nulled typeinfo (+0x10) inside Runtime.
    bool entityDialog = ToolUI_DialogHasEntityUI(s);
    if (entityDialog)
        *(uint8_t*)(s + 0x4C) = 1;

    // Pending drag: check mouse movement threshold
    if (*(char*)(s + 0x4D) != 0) {
        void* keyMap = Input::Configuration::_singleton->_keyMap;

        // Get mouse X, scaled by dialog scale factor
        void* dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisX = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0);  // B.3.7 bridge
        float scaleX = *(float*)((uintptr_t)dialogElem + 0xDC);

        // Get mouse Y
        dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisY = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 1);  // B.3.7 bridge
        float scaleY = *(float*)((uintptr_t)dialogElem + 0xDC);

        int mouseX = (int)((float)(int)axisX * scaleX);
        int mouseY = (int)((float)(int)axisY * scaleY);

        int deltaX = mouseX - *(int*)(s + 0x50);
        int deltaY = mouseY - *(int*)(s + 0x54);
        if (deltaX < 0) deltaX = -deltaX;
        if (deltaY < 0) deltaY = -deltaY;

        if (deltaX > 5 || deltaY > 5) {
            // Start drag: calc offset from dialog origin
            *(uint8_t*)(s + 0x4D) = 0;
            *(uint8_t*)(s + 0x40) = 1;

            // FUN_14007b940 inline: set dragging flag, calc offsets
            *(uint8_t*)(s + 0x41) = 1;
            *(int*)(s + 0x44) = *(int*)(s + 0x50) - *(int*)(*(uintptr_t*)(s + 0x18) + 0x88);
            *(int*)(s + 0x48) = *(int*)(s + 0x54) - *(int*)(*(uintptr_t*)(s + 0x18) + 0x8C);
        }

        // Check if mouse button released
        short btn = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 5);  // B.3.7 bridge
        if (btn == 0) {
            *(uint8_t*)(s + 0x4D) = 0;
        }
    }

    // Active drag: move dialog to follow mouse
    if (*(char*)(s + 0x41) != 0 && *(char*)(s + 0x40) != 0) {
        void* keyMap = Input::Configuration::_singleton->_keyMap;

        if (!*(uintptr_t*)(s + 0x18)) { /* dialog instance null, skip drag */ }
        else {
        void* dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisX = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0);  // B.3.7 bridge
        float scaleX = *(float*)((uintptr_t)dialogElem + 0xDC);

        dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisY = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 1);  // B.3.7 bridge
        float scaleY = *(float*)((uintptr_t)dialogElem + 0xDC);

        int newX = (int)((float)(int)axisX * scaleX) - *(int*)(s + 0x44);
        int newY = (int)((float)(int)axisY * scaleY) - *(int*)(s + 0x48);
        if (newX < 0) newX = 0;
        if (newY < 0) newY = 0;

        // Move dialog
        reinterpret_cast<UI::DialogInstance*>(*(void**)(s + 0x18))->SetUpperLeft(newX, newY, false);  // B.3.6 bridge

        // Update entity tracking BST with new position
        uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120);
        uintptr_t bstRoot = *(uintptr_t*)(worldObj + 0x50);

        // BST lookup by entity key (+0x30)
        uint64_t entityKey = *(uint64_t*)(s + 0x30);
        uintptr_t node = bstRoot;
        while (node != 0) {
            uint64_t nodeKey = *(uint64_t*)node;
            if (entityKey < nodeKey) {
                node = *(uintptr_t*)(node + 0x20);  // left child
            } else if (entityKey > nodeKey) {
                node = *(uintptr_t*)(node + 0x28);  // right child
            } else {
                // Found: update position
                *(int*)(node + 0x08) = newX;
                *(int*)(node + 0x0C) = newY;
                break;
            }
        }

        // If entity is a ComponentEntity, update via SelectEntity.
        // Same landmine as the drag-arm block above: IsDerivedFrom on a datasource
        // whose entity is mid-teardown reads nulled class-info. Only reachable
        // mid-drag, but a demolition can complete during a drag — gate on the
        // dialog-side entityDialog fact (so description datasources are never
        // probed) and on SA_DialogEntityAlive (weak-ref null / 0x400 marked), which
        // is only meaningful/safe once we know the datasource is an entity.
        void* dataSource = *(void**)(s + 0x28);
        bool isEntity = entityDialog && dataSource != nullptr &&
                        SA_DialogEntityAlive((void*)s) &&
                        ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
        if (isEntity && dataSource != nullptr) {
            void* ds2 = *(void**)(s + 0x28);
            bool check = ::Object::IsDerivedFromNullSafe(ds2, ComponentEntity::TypeId());
            void* entity = check ? ds2 : nullptr;

            uint8_t entityBuf[8];
            new (entityBuf) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

            EntityUI_SelectEntity_Impl(
                *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120),
                entityBuf, newX, newY);
        }

        // Check if mouse button released
        short btn = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 5);  // B.3.7 bridge
        if (btn == 0) {
            *(uint8_t*)(s + 0x41) = 0;
        }
        } // end else (dialog instance not null)
    }
}

// ============================================================
// ToolUI::Slot9 (vtable +0x48) — Event handler
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
    uintptr_t s = (uintptr_t)this;

    if (msg == 2) {
        if (id == 0) {
            if (*(char*)(s + 0x40) != 0) {
                // Already pinned — start drag immediately
                // FUN_14007b940 inline
                *(uint8_t*)(s + 0x41) = 1;
                *(int*)(s + 0x44) = mouseX - *(int*)(*(uintptr_t*)(s + 0x18) + 0x88);
                *(int*)(s + 0x48) = mouseY - *(int*)(*(uintptr_t*)(s + 0x18) + 0x8C);
                return;
            }

            if (*(char*)(s + 0x4C) != 0) {
                // Has UI desc — start pending drag
                *(uint8_t*)(s + 0x4D) = 1;
                *(int*)(s + 0x50) = mouseX;
                *(int*)(s + 0x54) = mouseY;
                return;
            }
        }
    } else if (msg == 5) {
        // Mouse up — cancel both drag modes
        *(uint8_t*)(s + 0x41) = 0;
        *(uint8_t*)(s + 0x4D) = 0;
        return;
    } else if (msg > 7 && msg < 0x0B) {
        // Focus events (8, 9, 10) — bring dialog to front
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        reinterpret_cast<UI::UserInterface*>(userInterface)->BringDialogToFront(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));  // B.3.6 bridge
        return;
    } else if (msg == 0x12 && id == 1) {
        // Close button — close dialog and remove from entity tracking
        // Before closing an ENTITY info panel, drop the selecting EditTool's stale
        // dialog reference. Retail tears the tool down on close (EntityUI::Slot9 ->
        // SetEditMode(0)); that slot is stubbed in standalone, so otherwise the
        // EditTool keeps ticking (EditTool_Startup_Impl, the +0x38 per-frame slot) on
        // the now-freed dialog and faults next frame inside GetController — CheckPtr
        // marks the dead ref with the -1 sentinel and GetController(-1) reads
        // 0xffffffffffffffff. Clearing the tool's selection (+0x28) makes its tick
        // early-out (it bails when entityRef == 0). Scoped to entity dialogs
        // (this ToolUI's datasource at +0x28 is a ComponentEntity) so utility dialogs
        // (map/help) that share this close path don't lose the user's active tool.
        // Entity-ness established dialog-side + aliveness-gated before the
        // IsDerivedFrom probe: clicking close during the demolition-teardown window
        // would otherwise hit the nulled-class-info AV (see ToolUI_DialogHasEntityUI).
        // On a dead entity the ref-drop is skipped — the EditTool tick's -1-sentinel
        // guard clears the stale tool refs next frame — and the dialog still closes.
        if (GameUpdateLoop::_singleton != nullptr &&
            ToolUI_DialogHasEntityUI(s) && SA_DialogEntityAlive((void*)s)) {
            void* ds = *(void**)(s + 0x28);
            if (ds && ::Object::IsDerivedFromNullSafe(ds, ComponentEntity::TypeId())) {
                uintptr_t pc = *(uintptr_t*)((uintptr_t)GameUpdateLoop::_singleton + 0x120);
                if (pc) {
                    void* activeTool = *(void**)(pc + 0x28);
                    // ONLY an EditTool keeps a selected entity at +0x30. When a build
                    // tool is armed instead, that offset is DragTool's endCell /
                    // lastEndCell int pair, and cb40 would hand those two ints to the
                    // component finder as an entity pointer (measured 2026-08-19:
                    // FencedTool+0x30 = 0xffffffff00008263 -> AV in the finder), while
                    // the two stores below would clobber the tool's drag state. A build
                    // tool has no selection to clear, so skip the whole block.
                    // EntityList_ClearToolSelectionForDyingEntities already gates the
                    // same way on the same field.
                    if (activeTool && EditTool_IsEditTool(activeTool)) {
                        // Deselect like retail cc90 steps (1)+(2): clear the
                        // selection highlight (Entity_impl_cb40 reads the entity
                        // from tool+0x30, so call it BEFORE nulling), then drop
                        // both the dialog ref (+0x28) and the selected-entity ref
                        // (+0x30). Leaving +0x30 set kept the outline lit after
                        // close AND blocked re-selecting the same building
                        // (EditTool::OnKey's `lastEntityRef != foundEntity` guard
                        // and d3b0's step-1 early-out both saw it as still
                        // selected). cb40 is our tool_base_render.cpp port,
                        // HighlightClear_Impl, registered unconditionally (the
                        // BANISHED_SELECTION_OUTLINE gate is gone): called by name (B3R).
                        HighlightClear_Impl(activeTool);   // 0x1400fcb40
                        *(uint64_t*)((uintptr_t)activeTool + 0x28) = 0;
                        *(uint64_t*)((uintptr_t)activeTool + 0x30) = 0;
                    }
                }
            }
        }
        // Standalone dialogs/controllers are only partially initialized. Validate the
        // pointers this path dereferences before use so a bad field can't fault: the
        // CloseDialog fn, the world singleton (walked for the tracking BST below), and
        // the dialog-instance pointer behind the IBP at +0x18 (deref'd for its
        // UserInterface at +0x10). Range check matches CitUI_OkPtr (canonical Win64
        // user-space heap window).
        if (GameUpdateLoop::_singleton == nullptr)
            return;
        {
            uintptr_t di = *(uintptr_t*)(s + 0x18);
            if (di < 0x10000 || di > 0x00007FFFFFFFFFFFULL) return;
        }
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));  // B.3.6 bridge

        // Remove from entity tracking BST
        uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(GameUpdateLoop::_singleton) + 0x120);
        uintptr_t bstRoot = *(uintptr_t*)(worldObj + 0x50);

        uint64_t entityKey = *(uint64_t*)(s + 0x30);
        uintptr_t node = bstRoot;
        while (node != 0) {
            uint64_t nodeKey = *(uint64_t*)node;
            if (entityKey < nodeKey) {
                node = *(uintptr_t*)(node + 0x20);
            } else if (entityKey > nodeKey) {
                node = *(uintptr_t*)(node + 0x28);
            } else {
                // Found: clear pin flag
                *(uint8_t*)(node + 0x10) = 0;
                return;
            }
        }
    }
}
