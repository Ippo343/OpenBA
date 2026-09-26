// code/Game/Dialog/goaldialog.cpp
// Clean reimplementation of GoalDialog game logic.
//
// GoalDialog (0xC8 bytes) — achievement goal progress display.
// Extends DialogInstance (0xB8 base).
// +0xB8 = goal entry array (stride 0x18), +0xC0 = goal count.
//
// Decompiled from Application-x64.exe.

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K1)
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/GoalDialog.h"
#include "Object/factory_registration.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// GoalDialog::Slot4 (vtable +0x20) — Setup: populate goal table
// ============================================================
// Finds "buttonOk" (ID 100), "tableGoal", and "textScenario".
// Gets GoalList from game world (+0x150). For each goal:
//   - Creates a row element from dialog template[0]
//   - Inserts into tableGoal via SetChild
//   - Finds "textGoal", "imageComplete", "imageFail" sub-elements
//   - Sets goal text on textGoal via UI::String
//   - Hides both complete/fail images initially
//   - Pushes {imageComplete, imageFail, textGoal} into array at +0xB8
// If scenario has an Achievement (+0x20), sets title on textScenario.
// Otherwise hides textScenario.
//
// Entry layout at +0xB8 (stride 0x18 per entry):
//   +0x00: Element* imageComplete (shown when goal is met)
//   +0x08: Element* imageFail (shown when goal has failed)
//   +0x10: TextElement* textGoal (goal description text)
//
// Original: 0x140069810.

void GoalDialog::OnOpened() {
    uintptr_t s = (uintptr_t)this;

    // Find "buttonOk" and set element ID 100
    {
        System::String nameStr("buttonOk");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        void* buttonOk = *(void**)retBuf;
        if (buttonOk) reinterpret_cast<UI::Element*>(buttonOk)->SetElementId(100);  // B.3.6 bridge
    }

    // Get GoalList from game world singleton at +0x150
    uintptr_t goalListObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x150);

    if (goalListObj != 0) {
        // GoalList is at goalListObj+0x10
        void* goalList = (void*)(goalListObj + 0x10);

        // Find "tableGoal"
        void* tableGoal = nullptr;
        {
            System::String nameStr("tableGoal");
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
            tableGoal = *(void**)retBuf;
        }

        int goalCount = reinterpret_cast<const Achievement::GoalList*>(goalList)->GetGoalCount();  // B.3.7 bridge
        void* dialog = *(void**)(s + 0x18); // Dialog* at this+0x18

        for (int i = 0; i < goalCount; i++) {
            // Create row element from dialog template[0]
            uint8_t templateBuf[16] = {0};
            ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(dialog)->GetTemplate(0));  // B.3.6 bridge
            void* elemDesc = *(void**)templateBuf;

            // Call ElementDescription::Create(this, retval, dialog) via vtable+0x40
            uint8_t newElemBuf[16] = {0};
            if (elemDesc) {
                reinterpret_cast<const UI::ElementDescription*>(elemDesc)->CreateInstance(  // B.3.6 bridge
                    reinterpret_cast<Object::InternalPtr<UI::Element>*>(newElemBuf),
                    reinterpret_cast<const UI::Dialog*>(dialog));
            }

            // Insert the new element as a child of the table
            if (tableGoal) {
                reinterpret_cast<UI::TableElement*>(tableGoal)->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(newElemBuf), i);  // B.3.6 bridge
            }

            // Find sub-elements within the new row
            void* newElem = *(void**)newElemBuf;
            void* imageComplete = nullptr;
            void* imageFail = nullptr;
            void* textGoal = nullptr;

            // Find "textGoal"
            {
                System::String nameStr("textGoal");
                uint8_t retBuf[16];
                ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(newElem)->FindElement(nameStr));  // B.3.6 bridge
                textGoal = *(void**)retBuf;
            }

            // Find "imageComplete"
            {
                System::String nameStr("imageComplete");
                uint8_t retBuf[16];
                ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(newElem)->FindElement(nameStr));  // B.3.6 bridge
                imageComplete = *(void**)retBuf;
            }

            // Find "imageFail"
            {
                System::String nameStr("imageFail");
                uint8_t retBuf[16];
                ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(newElem)->FindElement(nameStr));  // B.3.6 bridge
                imageFail = *(void**)retBuf;
            }

            // Get goal text and set on textGoal element
            {
                // Get GoalInstance at index i
                uint8_t goalInstBuf[16] = {0};
                ::new (goalInstBuf) Object::InternalPtr<Achievement::GoalInstance>(reinterpret_cast<const Achievement::GoalList*>(goalList)->GetGoal(i));  // B.3.7 bridge
                void* goalInstPtr = *(void**)goalInstBuf;

                // GoalInstance -> Goal at +0x18
                void* goal = *(void**)((uintptr_t)goalInstPtr + 0x18);

                // Get goal text (non-trivial return: fills retval with System::String)
                System::String textStr;
                ::new (&textStr) System::String(reinterpret_cast<const Achievement::Goal*>(goal)->GetText());  // B.3.7 bridge

                // Read StringTable from Goal+0x20 (ExternalPtr<StringTable>)
                uint64_t stringTable = *(uint64_t*)((uintptr_t)goal + 0x20);

                // Construct UI::String from StringTable + text
                uint8_t uiStr[16] = {0};
                ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), textStr);  // B.3.6 bridge

                // Set text on textGoal element
                if (textGoal) {
                    reinterpret_cast<UI::TextElement*>(textGoal)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
                }
            }

            // Hide both complete and fail images initially
            if (imageComplete) reinterpret_cast<UI::Element*>(imageComplete)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
            if (imageFail) reinterpret_cast<UI::Element*>(imageFail)->SetFlagsOnOff(1, 0);  // B.3.6 bridge

            // Push entry {imageComplete, imageFail, textGoal} to array at +0xB8.
            // B.3.2 bridge: GoalDialog+0xB8 is Collection::Array<{imageComplete,
            // imageFail, textGoal}> (stride 0x18, count +0xC0, capacity +0xC4)
            // inside a struct §B.2 has not converted, so it is reached at its
            // offset. The original was GoalDialog::Slot4's own inlined copy of
            // Collection::Array<T>::SetSize — NOT FUN_140011750 (that one has a
            // 0x30 stride with a String at +0x20 — different array).
            auto& goals = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0xB8);
            goals.SetSize(goals.GetSize() + 1,
                          GameColl::Bytes<0x18, 8>((uint64_t)imageComplete, (uint64_t)imageFail,
                                         (uint64_t)textGoal));
        }
    }

    // Find "textScenario" element
    void* textScenario = nullptr;
    {
        System::String nameStr("textScenario");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        textScenario = *(void**)retBuf;
    }

    // If GoalList has an Achievement at +0x20, set its title on textScenario
    if (goalListObj != 0 && *(void**)(goalListObj + 0x20) != nullptr) {
        void* achievement = *(void**)(goalListObj + 0x20);

        // Get title string (non-trivial return)
        System::String titleStr;
        ::new (&titleStr) System::String(reinterpret_cast<const Achievement::Achievement*>(achievement)->GetTitle());  // B.3.7 bridge

        // Read StringTable from Achievement+0x20
        uint64_t stringTable = *(uint64_t*)((uintptr_t)achievement + 0x20);

        // Construct UI::String from StringTable + title
        uint8_t uiStr[16] = {0};
        ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), titleStr);  // B.3.6 bridge

        // Convert UI::String to System::String for SetParameter
        System::String sysStr;
        ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge

        // Set as parameter 0 on textScenario
        if (textScenario) {
            reinterpret_cast<UI::TextElement*>(textScenario)->SetParameter(0, sysStr);  // B.3.6 bridge
        }
    } else {
        // No achievement/scenario — hide textScenario
        if (textScenario) reinterpret_cast<UI::Element*>(textScenario)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
    }
}

// ============================================================
// GoalDialog::Slot6 (vtable +0x30) — Update goal entry visibility
// ============================================================
// Gets the GoalList from game world (+0x150), iterates each goal entry.
// For each goal: checks IsMet/HasFailed, sets visibility flags on
// corresponding UI elements (entry+0 for met, entry+8 for failed).
//
// Entry layout at +0xB8 (stride 0x18 per entry):
//   +0x00: Element* metIndicator (shown when goal is met)
//   +0x08: Element* failedIndicator (shown when goal has failed)
//   +0x10: (unused in Slot6)
//
// Original: 0x140069640.

void GoalDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    // Get GoalList from game world singleton at +0x150
    void* world = *g_pGameWorldSingleton;
    uintptr_t goalListObj = *(uintptr_t*)((uintptr_t)world + 0x150);

    int count = *(int*)(s + 0xC0);
    uintptr_t entryArray = *(uintptr_t*)(s + 0xB8);

    for (int i = 0; i < count; i++) {
        uintptr_t entryOffset = (uintptr_t)i * 0x18;

        // Get goal at index i from the GoalList
        uint64_t goalInst = 0;
        ::new (&goalInst) Object::InternalPtr<Achievement::GoalInstance>(reinterpret_cast<const Achievement::GoalList*>((void*)(goalListObj + 0x10))->GetGoal(i));  // B.3.7 bridge

        // Check if goal is met and if it has failed
        bool isMet = reinterpret_cast<const Achievement::GoalInstance*>((void*)goalInst)->IsMet();  // B.3.7 bridge
        bool hasFailed = reinterpret_cast<const Achievement::GoalInstance*>((void*)goalInst)->HasFailed();  // B.3.7 bridge

        // Met indicator: show if met, hide if not
        void* metElem = *(void**)(entryArray + entryOffset);
        reinterpret_cast<UI::Element*>(metElem)->SetFlagsOnOff(isMet ? 0 : 1, isMet ? 1 : 0);  // B.3.6 bridge

        // Failed indicator: show if failed, hide if not
        void* failedElem = *(void**)(entryArray + 8 + entryOffset);
        reinterpret_cast<UI::Element*>(failedElem)->SetFlagsOnOff(hasFailed ? 0 : 1, hasFailed ? 1 : 0);  // B.3.6 bridge
    }
}

// ============================================================
// GoalDialog::Slot7 (vtable +0x38) — no-op handler
// ============================================================
// 3-byte no-op: XOR EAX,EAX; RET. Returns 0.
// Original: 0x140069800.

void GoalDialog::OnUpdate(const System::Time& /*time*/) {}

// ============================================================
// GoalDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// When msg==0x12 (button click) and id==100 (buttonOk):
//   Wraps self in InternalBasePtr, calls UserInterface::CloseDialog.
//
// Original: 0x1400697C0.

void GoalDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 == 0x12 && param4 == 100) {
        uint8_t selfIBP[16];
        new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        reinterpret_cast<UI::UserInterface*>(*(void**)((uintptr_t)this + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
    }
}

// ============================================================
// Construction, destruction, reflection (ENDGAME B.3.8 block DLG)
// ============================================================

GoalDialog::GoalDialog() : dataB8(0), dataC0(0) {}

// MainMenuDialog's opener inlines this ctor: DialogInstance(ui, dialog,
// nullParent, 0, 0, 0), then the fields zeroed.
GoalDialog::GoalDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
             Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0), dataB8(0), dataC0(0) {}

// PORT GAP (B38 D2): retail's dtor 0x140011c10 also frees the entry array at
// +0xb8 and zeroes its header; the hand-built slot 0 did not (it leaked the
// array). Kept for this commit; the retail body is a separate titled commit.
GoalDialog::~GoalDialog() {}

// [0] 0x140011c10: ~GoalDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* GoalDialog::DeletingDtor(unsigned flags) {
    this->~GoalDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011b60: MemAlloc(0xc8, 8), then the ctor.
void* GoalDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(GoalDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) GoalDialog());
}
// createInPlace 0x140011bc0 (the tree's in-place build is kept, B8-13).
void* GoalDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) GoalDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4).
DEFINE_EXE_VTABLE(GoalDialog, "_ZTV10GoalDialog")
