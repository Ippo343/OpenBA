// code/Game/Tutorial/tutorial_wave.cpp
//
// The second half of the Tutorial ResolveAddr gap: 12 exe-internal helpers the
// already-ported Tutorial module resolves by address and which are registered
// NOWHERE, so every one of them is a null function pointer.
//
// The eight from notes/wave_remaining/lane2_tutorial.txt:
//   0x1400262B0  ExternalPtr<Tutorial>::Serialize        (fn_ExternalPtr_SerializeTutorial)
//   0x140028650  ExternalPtr<UI::Dialog>::Serialize      (fn_SerializeExternalPtr)
//   0x1400E1DB0  FinishTutorialInstance step driver      (fn_TutorialStepExecute)
//   0x1400E2330  TutorialSetupDialog::SetAllDialogFlags  (fn_TSD_SetDialogFlags)
//   0x1400E24F0  TutorialSetupDialog::FindHighlight      (fn_TSD_FindInstance)
//   0x1400E2700  TutorialSetupDialog::OpenHighlight      (fn_TSD_OpenDialog)
//   0x1400E29F0  TutorialSetupDialog::ShowListed         (fn_TSD_Slot9Helper)
//   0x1401001F0  FinishTutorialGoal tutorial-tree insert (fn_TutorialTreeInsert)
//
// plus four sub-callees of those that are equally unregistered and were not on
// the list (closure verified in Ghidra with get_function_callees on every one):
//   0x1400E2250  recursive element show/hide             (called by 2330)
//   0x14001AF90  ExternalRef copy-construct              (called by 2700)
//   0x14001AFE0  ExternalRef resolve + extract           (called by 2700)
//   0x1400E1260  FinishTutorialInstance step->UI refresh (called by 1DB0)
//
// Every address above was verified as a real function ENTRY with
// get_function_by_address (Entry == address) — the mid-function-resolve trap
// ([[reference-midfunction-raw-address-resolves]]) does not apply to any of them.
//
// STILL DARK, deliberately (see the report):
//   0x1400E0D70  "tutorial finished -> launch the scenario" transition. Its own
//                closure pulls in four more unported exe functions
//                (0x140013A60 / 0x140013AA0 / 0x140098060 GameUpdateLoop ctor /
//                0x140099130). TutWave_StepExecute logs and returns instead of
//                calling a null pointer, and says so.
// (0x1400E1870 FindUIElementByPath is now ported — see
//  code/Game/Tutorial/find_ui_element_by_path.cpp. Both call sites here,
//  ShowListed and OpenHighlight's by-path branch, are live.)
//
// Decompiled from Application-x64.exe; control flow checked against the
// disassembly, not just the Ghidra pseudo-C, for 0x1400E01D0 / 0x1400E1260 /
// 0x1400E2700 where the decompiler mis-renders the stack arguments.

#include "Component/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include "system/MemoryAllocator.h"
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"   // 0x140014890
#include "Object/factory_registration.h"
#include "Tutorial/TutorialSetupRestrictPlayer.h"
#include "Tutorial/TutorialHighlightDialog.h"
#include "Tutorial/TutorialUI.h"
#include "Game/serialize_externals.h"
#include "Game/PlayerToolManager.h"
#include "ui/UserInterface.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/TextElement.h"    // ENDGAME B.3.6 UI callers (K4)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K4)
#include "Tutorial/Tutorial.h"
#include "Tutorial/TutorialInstance.h"
#include "Tutorial/TutorialStep.h"
#include "Tutorial/TutorialSetup.h"
#include "Tutorial/TutorialSetupDialog.h"
#include "Tutorial/TutorialGoal.h"
#include "Tutorial/TutorialImage.h"
#include "Tutorial/exe_bodies.h"
#include "Entity/CitizenBehavior.h"
#include "Component/ComponentDescription.h"

namespace {

inline UI::UserInterface* UserInterface() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    return gw ? gw->m_userInterface : nullptr;
}

// TutorialHighlightDialog type id (retail DAT_1401C6550) — the class's own
// registration (ENDGAME B.3.8: no lazy cache).
uint32_t HighlightTypeId() { return TutorialHighlightDialog::TypeId(); }
// TutorialUI type id (retail DAT_1401C67F0) — the class's own registration
// (ENDGAME B.3.8: no lazy cache). A 0 here makes StepExecute's controller
// narrow fail and the step panel is never bound.
uint32_t TutorialUITypeId() { return TutorialUI::TypeId(); }

// The CitizenBehavior +0x160 template tree (Goal/goal_exe_helpers.h): key
// +0x00, entities +0x08, count +0x10, left +0x20, right +0x28.
TemplateBucketNode* BstFind(TemplateBucketNode* node, const Object::External* key) {
    while (node) {
        const Object::External* nodeKey = node->key;
        if (key < nodeKey)      node = node->left;
        else if (nodeKey < key) node = node->right;
        else                    return node;
    }
    return nullptr;
}

// A System::String is empty when it has no data or its length (data - 8) is 0.
inline bool HasText(const System::String& s) {
    return s._data != nullptr && s.GetLength() != 0;
}

}  // namespace

// ===========================================================================
// 0x1400262B0 — Object::ExternalPtr<Tutorial>::Serialize
// 0x140028650 — Object::ExternalPtr<UI::Dialog>::Serialize
//
// Both are the stock ExternalPtr<T>::Serialize template body — byte-identical
// to FUN_14001B8E0 / FUN_140002340 / FUN_140027AD0 — differing only in the type
// used for the post-load IsDerivedFrom. 0x140028650's operand is the exe slot
// at 0x140151728, which standalone/address_registry.cpp already documents as
// UI::Dialog's type id.
//
// These two are why a Tutorial.rsc load could not work: every FinishTutorialGoal
// serialize skipped its Tutorial ExternalPtr and every TutorialSetupDialog
// serialize skipped its Dialog ExternalPtr, which does not merely lose the
// field — it mis-advances the stream cursor for everything that follows
// ([[reference-unresolved-rdata-const-deletes-guarded-test]] family).
// LEFT: ExternalPtrSerializeGeneric (Game/serialize_externals.h) takes void**.
// ===========================================================================
extern "C" void TutWave_SerializeTutorialRef(void* field, void* serializer) {
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(field), serializer,
                                nullptr, "Tutorial");
}

extern "C" void TutWave_SerializeDialogRef(void* field, void* serializer) {
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(field), serializer,
                                nullptr, "UI::Dialog");
}

// ===========================================================================
// 0x1401001F0 — the tutorial's insert into the PlayerToolManager's +0x50
// tracked set (a PlayerToolManager member in retail; GAME-b's class, kept
// free here).
//
// Upsert keyed by the tutorial's setup description. Existing node: active
// byte at +0x10 = 1, dword +0x08 = value, dword +0x0C = 0x20. Absent: build
// that same 0x18-byte record on the stack and hand it to the shared BST
// inserter at 0x140014890 (ToolbarUtility_BstInsert_4890).
// ===========================================================================
void TutWave_TutorialTreeInsert(PlayerToolManager* treeOwner, const ComponentDescription* tutorialId,
                                void* /*param3*/, uint32_t value) {
    if (!treeOwner) return;
    const void* key = tutorialId;

    // Walk. The node is TrackedEntityNode (Game/PlayerToolManager.h), keyed by
    // address.
    TrackedEntityNode* node = treeOwner->field_0x50;
    while (node) {
        const void* nodeKey = node->key;
        if (key < nodeKey) { node = node->left; continue; }
        if (nodeKey < key) { node = node->right; continue; }
        node->active = 1;
        node->x      = static_cast<int32_t>(value);
        node->y      = 0x20;
        return;
    }

    struct { const ComponentDescription* key; uint32_t value; uint32_t flags; uint8_t on; uint8_t _pad[7]; } rec;
    rec.key   = tutorialId;
    rec.value = value;
    rec.flags = 0x20;
    rec.on    = 1;
    for (int i = 0; i < 7; ++i) rec._pad[i] = 0;

    void* out[4] = {};
    // LEFT: 0x140014890's forwarder takes the root as int64_t* and the record
    // as uint64_t* (DLG-d's B4 FORWARD signature).
    ToolbarUtility_BstInsert_4890(reinterpret_cast<int64_t*>(&treeOwner->field_0x50), out,
                                  reinterpret_cast<uint64_t*>(&rec));   // 0x140014890
}

// ===========================================================================
// 0x1400E2250 — recursive element show/hide.
//
// SetFlagsOnOff(elem, show ? 0 : 0x1000000, show ? 0x1000000 : 0), then recurse
// into every child that resolves non-null. 0x1000000 is the hidden bit.
// The double Child() call is retail's, not a transcription slip: the first
// fetch is only a null test, the second is the one that is recursed into, and
// ChildCount is re-read every iteration because a child may add siblings.
// ===========================================================================
void TutorialSetupDialog::SetElementFlagsRecursive(Object::InternalPtr<UI::Element>* elemIBP, char show) {
    if (!elemIBP) return;

    UI::Element* elem = elemIBP->Get();
    if (!elem) return;

    uint32_t hideFlags = show ? 0u : 0x1000000u;
    uint32_t showFlags = show ? 0x1000000u : 0u;
    elem->SetFlagsOnOff(hideFlags, showFlags);

    int i = 0;
    int n = elemIBP->Get()->ChildCount();
    while (i < n) {
        Object::InternalPtr<UI::Element> probe = elemIBP->Get()->Child(i);
        if (probe.Get() != nullptr) {
            Object::InternalPtr<UI::Element> child = elemIBP->Get()->Child(i);
            SetElementFlagsRecursive(&child, show);
        }
        ++i;
        n = elemIBP->Get()->ChildCount();
    }
}

// ===========================================================================
// 0x1400E2330 — show/hide EVERY open dialog's element tree.
//
// This is the "restrict the player to what the tutorial step allows" mechanic:
// with show=0 it hides the entire UI, and ShowListed (0x1400E29F0) then unhides
// only the paths named in the step's first string vector.
// ===========================================================================
void TutorialSetupDialog::SetAllDialogFlags(char show) {
    UI::UserInterface* ui = UserInterface();
    if (!ui) return;

    // `_dialogs.GetFirst()` IS retail's GetFirstDialog (0x180031150, which is
    // literally `*retval = this->_dialogs._head`); the iterator is the bare node
    // pointer, so `it.node` is what retail hands to CheckPtr.
    int dialogs = 0;
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        // LEFT: UserInterface's list element is `DialogInstancePtr` = void* (R-UI's typedef).
        UI::DialogInstance* dialog = reinterpret_cast<UI::DialogInstance*>(it.node->element);
        UI::Element* root = dialog->GetRoot().Get();
        if (root) {
            // Retail applies the flags to the root itself and then walks the
            // children — the root pass is inlined here, the children go through
            // the shared recursive helper (same code, same order).
            uint32_t hideFlags = show ? 0u : 0x1000000u;
            uint32_t showFlags = show ? 0x1000000u : 0u;
            root->SetFlagsOnOff(hideFlags, showFlags);

            int i = 0;
            int n = root->ChildCount();
            while (i < n) {
                Object::InternalPtr<UI::Element> probe = root->Child(i);
                if (probe.Get() != nullptr) {
                    Object::InternalPtr<UI::Element> child = root->Child(i);
                    SetElementFlagsRecursive(&child, show);
                }
                ++i;
                n = root->ChildCount();
            }
        }
        ++dialogs;
    }
}

// ===========================================================================
// 0x1400E24F0 — find the currently open TutorialHighlightDialog.
//
// Early-out: if the setup has neither a highlight target path (+0x40, an empty
// System::String reads its length at ptr-8) nor a resolved Dialog External
// (+0x50), it resolves the cached External at +0x58 and gives up when that is
// null too. Otherwise it scans the open-dialog list for one whose IsA matches
// the TutorialHighlightDialog type id.
// ===========================================================================
Object::InternalPtr<UI::DialogInstance> TutorialSetupDialog::FindHighlightDialog() {
    bool havePath = HasText(HighlightPath());
    bool haveDialog = (highlightDialog != nullptr);
    if (!havePath && !haveDialog) {
        // FUN_14001B1D0 (ResolveExternalByName_Impl).
        ResolveExternalByName_Impl(&toolbarRef);
        if (toolbarRef.resolved == nullptr) return nullptr;
    }

    UI::UserInterface* ui = UserInterface();
    if (!ui) return nullptr;
    uint32_t tid = HighlightTypeId();
    if (!tid) return nullptr;   // type not registered yet — a safe no-match

    // Same list, same order as retail's GetFirstDialog walk (see above).
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        // LEFT: UserInterface's list element is `DialogInstancePtr` = void* (R-UI's typedef).
        UI::DialogInstance* dialog = reinterpret_cast<UI::DialogInstance*>(it.node->element);
        if (dialog->IsA(tid)) {
            return dialog;
        }
    }
    return nullptr;
}

// ===========================================================================
// 0x14001AF90 — copy-construct a NamedExternalRef {String, generation, cached*}.
// 0x14001AFE0 — resolve one and hand back the cached External*, destroying the
//               temporary String the copy owns.
// Used as a pair by OpenHighlightDialog to look the toolbar External up
// without disturbing the setup's own cached reference.
// ===========================================================================
static NamedExternalRef* TutWave_ExternalRefCopy(NamedExternalRef* dst, const NamedExternalRef* src) {
    if (!dst || !src) return dst;
    new (dst->name) System::String(src->Name());
    dst->generation = src->generation;
    dst->resolved   = src->resolved;
    return dst;
}

static const Object::External* TutWave_ExternalRefResolve(NamedExternalRef* ref) {
    if (!ref) return nullptr;
    ResolveExternalByName_Impl(ref);
    const Object::External* out = ref->resolved;
    ref->Name().~String();
    return out;
}

// ===========================================================================
// 0x1400E2700 — open the tutorial highlight dialog for the current setup.
//
// Two branches, both ending in MemAlloc(0xF0) + TutorialHighlightDialogCtor +
// UserInterface::AddDialog:
//
//  A. TOOLBAR-CELL branch, taken when the setup's cached External at +0x68 is
//     non-null AND the template's entity count exceeds the setup's index
//     (+0x70). The entity comes out of the CitizenBehavior +0x160 tree, keyed
//     by that External: node+0x08 is the entity array, node+0x10 the count.
//     Ctor gets parent=0, +0xE0=0, +0xD8=setup+0x48, +0xE8=entity.
//  B. ELEMENT-PATH branch otherwise: FindUIElementByPath(&elem, setup+0x40,
//     &dialog); bail if the element is not found. Ctor gets parent=dialog,
//     +0xE0=element, +0xD8=setup+0x48, +0xE8=0.
//
// Guard rails that are RETAIL, not ours: the same +0x40/+0x50/+0x58/+0x68
// early-out as FindHighlightDialog, and a hard skip while
// UserInterface::GetAddDialogCount() > 0 (never stack a highlight on a frame
// that is already adding dialogs). `checkExisting` non-zero additionally skips
// when a highlight dialog is already open.
// ===========================================================================
void TutorialSetupDialog::OpenHighlightDialog(char checkExisting) {
    bool havePath = HasText(HighlightPath());
    bool haveDialog = (highlightDialog != nullptr);
    if (!havePath && !haveDialog) {
        ResolveExternalByName_Impl(&toolbarRef);
        if (toolbarRef.resolved == nullptr) return;
    }

    UI::UserInterface* ui = UserInterface();
    if (!ui) return;
    if (ui->GetAddDialogCount() > 0) return;

    Object::InternalPtr<UI::DialogInstance> existing = FindHighlightDialog();
    if (checkExisting != 0 && existing.Get() != nullptr) return;

    ResolveExternalByName_Impl(&toolbarRef);

    ComponentEntity* targetElem = nullptr;          // -> ctor arg that lands at +0xE8 (branch A)
    UI::DialogInstance* parentIBP = nullptr;        // -> ctor R9 parent
    Object::InternalPtr<UI::Element> elemIBP;       // -> ctor arg that lands at +0xE0 (branch B)
    bool     branchA    = false;

    if (toolbarRef.resolved != nullptr) {
        GameUpdateLoop* gw = GameUpdateLoop::_singleton;
        CitizenBehavior* mgr = gw ? gw->m_citizenList : nullptr;
        TemplateBucketNode* root = mgr ? TemplateTreeRoot(mgr) : nullptr;

        NamedExternalRef refCopy{};
        TutWave_ExternalRefCopy(&refCopy, &toolbarRef);
        const Object::External* key = TutWave_ExternalRefResolve(&refCopy);

        TemplateBucketNode* found = BstFind(root, key);
        int count = found ? found->count : 0;
        int index = toolbarIndex;

        if (count > index) {
            NamedExternalRef refCopy2{};
            TutWave_ExternalRefCopy(&refCopy2, &toolbarRef);
            const Object::External* key2 = TutWave_ExternalRefResolve(&refCopy2);
            TemplateBucketNode* found2 = BstFind(root, key2);
            if (found2 && found2->entities) {
                targetElem = found2->entities[index];
            }
            branchA = true;
        }
    }

    if (!branchA) {
        // Branch B: FindUIElementByPath (exe 0x1400E1870).
        FindUIElementByPath_Impl(&elemIBP, &HighlightPath(), &parentIBP);
        if (elemIBP.Get() == nullptr) return;
    }

    // MemAlloc(0xF0) + TutorialHighlightDialog::TutorialHighlightDialog
    // (0x1400E01D0) — the real class now (ENDGAME B.3.8).
    void* mem = System::MemoryAllocator::MemAlloc(sizeof(TutorialHighlightDialog), 8);
    TutorialHighlightDialog* dlg = nullptr;
    if (mem) {
        int32_t valD8 = arrowRotation;
        const UI::Dialog* dialog = highlightDialog;
        if (branchA) {
            dlg = ::new (mem) TutorialHighlightDialog(ui, dialog,
                                                      Object::InternalPtr<UI::DialogInstance>(),
                                                      nullptr, valD8,
                                                      targetElem);
        } else {
            dlg = ::new (mem) TutorialHighlightDialog(ui, dialog,
                                                      Object::InternalPtr<UI::DialogInstance>(parentIBP),
                                                      elemIBP.Get(), valD8,
                                                      nullptr);
        }
    }

    Object::InternalPtr<UI::DialogInstance> dlgIBP(dlg);
    ui->AddDialog(&dlgIBP);
}

// ===========================================================================
// 0x1400E29F0 — hide everything, then unhide the paths this setup names.
//
// The first string vector (+0x20 data / +0x28 count) holds element paths;
// each resolves through FindUIElementByPath and is re-shown recursively. This
// is the whole "the tutorial greys out the UI except the button it wants you
// to press" effect.
// ===========================================================================
void TutorialSetupDialog::ShowListedElements() {
    SetAllDialogFlags('\0');

    int count = static_cast<int32_t>(shownPathCount);
    if (count <= 0) return;

    System::String* arr = shownPaths;
    if (!arr) return;
    for (int i = 0; i < count; ++i) {
        Object::InternalPtr<UI::Element> elem;
        FindUIElementByPath_Impl(&elem, &arr[i], nullptr);
        if (elem.Get() != nullptr) {
            Object::InternalPtr<UI::Element> copy = elem;
            SetElementFlagsRecursive(&copy, '\x01');
        }
    }
}

// ===========================================================================
// 0x1400E1260 — push the current TutorialStep into the TutorialUI panel.
//
// Ported from the disassembly (0x1400E1260-0x1400E1513); the Ghidra decompile
// renders the two UI::String constructions with different arity, which is wrong
// — both are the (retbuf, ExternalPtr<StringTable>, String const&) overload
// through the same dispatch slot 0x140151300.
//
//   step = tutorial->steps[self->stepIndex]
//   1. every setup in step (+0x50 data / +0x58 count): vtable slot 8
//   2. preText  = UI::String(step+0x28 stringTable, step+0x30) -> ui+0x38
//      postText = UI::String(step+0x28 stringTable, step+0x38) -> ui+0x40
//   3. each text element is shown iff its own resolved UI::String
//      (element+0xD8/+0xE0) IsValid
//   4. the image table at ui+0x48 is emptied and re-grown to its previous child
//      count with null children
//   5. every image in step (+0x40 data / +0x48 count): vtable slot 9 builds an
//      element against the dialog at ui+0x18, placed with
//      TableElement::SetChild(elem, image+0x2C, image+0x28)
// The text elements are UI::TextElements and the image table a
// UI::TableElement (TutorialUI stores them as Elements; retail calls the
// subclass members on them directly).
// ===========================================================================
void TutorialInstance::RefreshStepUI() {
    Tutorial* tutorial = tutorialDef;
    if (!tutorial) return;
    TutorialStep** steps = tutorial->steps;
    if (!steps) return;
    int idx = currentStep;
    TutorialStep* step = steps[idx];
    if (!step) return;

    // 1. setup objects — vtable slot 8 (+0x40)
    {
        int n = step->setupCount;
        TutorialSetup** data = step->setups;
        for (int i = 0; i < n && data; ++i) {
            TutorialSetup* obj = data[i];
            if (!obj) continue;
            obj->Begin();   // slot 8
        }
    }

    TutorialUI* ui = stepUI;
    if (!ui) return;   // no TutorialUI controller attached yet

    // 2/3. the two text elements
    {
        UI::StringTable* stringTable = step->stringTable;

        const struct { System::String* srcString; UI::Element* elem; } texts[2] = {
            { &step->TitleText(), ui->preText },
            { &step->DescText(),  ui->postText },
        };
        for (const auto& t : texts) {
            UI::String tmp(Object::ExternalPtr<UI::StringTable>(stringTable), *t.srcString);
            UI::Element* elem = t.elem;
            if (!elem) continue;
            static_cast<UI::TextElement*>(elem)->SetText(&tmp);
        }
        for (const auto& t : texts) {
            UI::Element* elem = t.elem;
            if (!elem) continue;
            UI::String cur = static_cast<UI::TextElement*>(elem)->GetText();
            bool valid = cur.IsValid();
            elem->SetFlagsOnOff(valid ? 0u : 1u, valid ? 1u : 0u);
        }
    }

    // 4. clear + re-grow the image table
    UI::Element* table = nullptr;
    {
        table = ui->imageTable;
        if (table) {
            int n = table->ChildCount();
            table->RemoveChildren();
            for (int i = 0; i < n; ++i) {
                Object::InternalPtr<UI::Element> nullChild;
                table->AddChild(&nullChild);
            }
        }
    }

    // 5. images — vtable slot 9 (+0x48) builds each element
    {
        int n = step->imageCount;
        TutorialImage** data = step->images;
        if (n > 0 && data && table) {
            UI::DialogInstance* dlgIBP = ui->_dialog;
            const UI::Dialog* dialog = dlgIBP ? dlgIBP->_dialog : nullptr;
            for (int i = 0; i < n; ++i) {
                TutorialImage* img = data[i];
                if (!img) continue;
                int32_t col = img->imageWidth;    // +0x28
                int32_t row = img->imageHeight;   // +0x2C
                Object::InternalPtr<UI::Element> retbuf;
                // slot 9, retslot shape (RCX this, RDX ret, R8 dialog)
                Object::InternalPtr<UI::Element>* elemPtr = img->CreateElement(&retbuf, dialog);
                static_cast<UI::TableElement*>(table)->SetChild(elemPtr ? elemPtr : &retbuf, row, col);
            }
        }
    }
}

// ===========================================================================
// 0x1400E1DB0 — the FinishTutorialInstance per-tick driver. This is the heart
// of the tutorial: it binds the TutorialUI controller on the first tick, ticks
// the current step's setups, polls its goals, and advances when all goals pass.
//
//   if self+0x18 (dialog IBP) is empty:
//       find the dialog hosting the tutorial's setup description
//         (PlayerToolManager::FindUtilityDialog, exe 0x140100270)
//       self+0x20 = that dialog's controller, if it IsDerivedFrom TutorialUI
//       RefreshStepUI(self)
//   step = tutorial->steps[self->stepIndex]
//   for each setup:  vtable slot 9 (+0x48)(setup, param2)
//   for each goal:   passed += vtable slot 8 (+0x40)(goal, param2, &selfIBP)
//   if passed == goalCount:
//       for each setup: vtable slot 10 (+0x50)(setup)
//       ++stepIndex
//       if more steps: RefreshStepUI(self)
//       else:          <tutorial complete>  (LoadNextScenario below)
// ===========================================================================
extern "C" { int g_TutUndoDepth = 0; int g_TutUndoIndex = -1; }

void TutorialInstance::StepExecute(void* param2, void* param3, uint64_t param4) {
    Tutorial* tutorial = tutorialDef;
    if (!tutorial) {
        // Both of retail's early returns are silent, which is fine in retail and
        // useless here: "the tutorial never appears" looks identical whether the
        // Tutorial definition never reached +0x30 or the dialog lookup missed.
        // One line each, once.
        static bool once = false;
        if (!once) { once = true;
            fprintf(stderr, "[Tutorial] StepExecute BAILED: tutInst+0x30 (Tutorial def) is NULL "
                            "- FinishTutorialInstance::Slot4 never copied it from the goal\n");
            fflush(stderr); }
        return;
    }

    if (stepDialog == nullptr) {
        UI::DialogInstance* dlg = PlayerToolManager::FindUtilityDialog(tutorial->setup);   // 0x140100270
        stepDialog = dlg;
        if (dlg == nullptr) {
            static bool once = false;
            if (!once) { once = true;
                fprintf(stderr, "[Tutorial] StepExecute BAILED: FindDialogByControllerType found no "
                                "dialog for tutorial=%p dialogExt=%p\n",
                        static_cast<void*>(tutorial),
                        static_cast<const void*>(tutorial->setup));
                fflush(stderr); }
            return;
        }

        uint32_t uiTid = TutorialUITypeId();
        TutorialUI* controller = nullptr;
        {
            // Third arg is a TYPE FILTER, not padding: retail is
            //   MOV R8D,[0x1401c67f0]   ; TutorialUI type id
            //   CALL [0x140151768]      ; DialogInstance::GetController
            // (0x1400e1e08). Ghidra renders the call as two arguments and hides it.
            // Passing 0 made GetController return NOTHING, so the controller was null,
            // the TutorialUI narrow below could not run, and RefreshStepUI had no panel
            // to write into — the tutorial panel rendered as a bare title and two "Ok"
            // rows. Measured as `ctrl=0 tid=0 (?) wantTutorialUI=857087488`.
            UI::DialogController* ctrl = dlg->GetController(uiTid).Get();
            if (ctrl && uiTid && Object::IsDerivedFromNullSafe(ctrl, uiTid)) {
                controller = static_cast<TutorialUI*>(ctrl);
            }
        }
        stepUI = controller;
        RefreshStepUI();
    }

    int stepIndex = currentStep;
    int stepCount = tutorial->stepCount;

    if (stepIndex >= stepCount) return;

    TutorialStep** steps = tutorial->steps;
    if (!steps) return;
    TutorialStep* step = steps[stepIndex];
    if (!step) return;

    // setups: slot 9
    {
        int n = step->setupCount;
        TutorialSetup** data = step->setups;
        for (int i = 0; i < n && data; ++i) {
            TutorialSetup* obj = data[i];
            if (!obj) continue;
            obj->Update(param2);   // slot 9
        }
    }

    // goals: slot 8, counts the ones that report complete
    int passed = 0;
    int goalCount = step->goalCount;
    {
        TutorialGoal** data = step->goals;
        for (int i = 0; i < goalCount && data; ++i) {
            TutorialGoal* obj = data[i];
            if (!obj) continue;
            Object::InternalPtr<TutorialInstance> selfIBP(this);
            // slot 8
            bool ok = obj->IsComplete(param2, &selfIBP);
            if (ok) ++passed;
        }
    }

    if (goalCount == 0 || passed != goalCount) return;

    // step complete: slot 10 on every setup, then advance.
    //
    // [STANDALONE DEVIATION — ordering] Retail walks this array strictly forward
    // (0x1400E1DB0: `pvVar6 + 8` per iteration). We undo every
    // TutorialSetupRestrictPlayer FIRST, then the rest in the original forward order.
    // Measured reason:
    //   TutorialSetupDialog::Slot10 (0x1400E2460) ends in UI::UserInterface::CloseDialog,
    //   and CloseDialog calls UI::UserInterface::UpdateEvents (Runtime 0x18011FC60),
    //   which DRAINS the queued toolbar command re-entrantly, in the middle of this
    //   loop. Captured on stepFirstHouse2 -> stepFirstHouse3:
    //     GoalButton bit22=1                     goal met (button pressed)
    //     [ToolTrace] OnEvent CLICK tutUndoDepth=1 tutUndoIdx=0 ret=<UpdateEvents>
    //     [ToolTrace] RESTRICT override groupId=<WoodHouse> -> NULL
    //     [ToolTrace] ShowGroup CREATE toolTypeId=...(EditTool)
    //   Setup[0] is the dialog setup; setup[1] is `disableSelect`, an all-default
    //   TutorialSetupRestrictPlayer that holds PlayerController+0x80=1 with +0x88=NULL.
    //   So the Wooden House command was dispatched while the OUTGOING step's "no tool"
    //   restriction was still armed: SetEditMode (0x140102B60) forced the group to NULL
    //   and installed an EditTool, and the placement tool — and with it the yellow target
    //   footprint, which PlacementTool::Draw (0x1400FDCC0) section 2 only renders while a
    //   placement tool is live — never came up.
    //   Undoing the restriction first means that drain sees no restriction at all, which
    //   is the same state retail's drain sees.
    {
        int n = step->setupCount;
        TutorialSetup** data = step->setups;
        // ENDGAME B.3.8: the class's own record (was a lazy GetTypeIdByName cache).
        const uint32_t s_restrictTid = TutorialSetupRestrictPlayer::TypeId();

        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < n && data; ++i) {
                TutorialSetup* obj = data[i];
                if (!obj) continue;
                uint32_t tid = obj->Type();
                const bool isRestrict = (s_restrictTid && tid == s_restrictTid);
                if ((pass == 0) != isRestrict) continue;
                // g_TutUndoDepth lets the toolbar trace say out loud when a UI command
                // is dispatched from inside this loop (see above).
                ++g_TutUndoDepth;
                g_TutUndoIndex = i;
                obj->End();   // slot 10
                --g_TutUndoDepth;
            }
        }
    }

    currentStep = stepIndex + 1;

    if (stepIndex + 1 < stepCount) {
        RefreshStepUI();
        return;
    }

    // Tutorial finished: boot the next scenario through a fresh GameUpdateLoop.
    //
    // This used to be a DarkOnce stub whose comment named four unported exe
    // functions as blockers. That comment was stale -- 0x140013A60 and
    // 0x140013AA0 are one-line String accessors, 0x140099130 was already inline
    // in engine_funcs.h, and 0x140098060 is a flat field-init. The port lives in
    // tutorial_next_scenario.cpp; see its header for the full retail body.
    //
    // Retail also calls FUN_140101A80 first (lane 1's tutorial child-dialog
    // opener). That one is still unported and is NOT on this path's critical
    // line -- 0D70 replaces the whole loop, dialogs included -- so it stays out.
    (void)param3; (void)param4;
    LoadNextScenario();
}
