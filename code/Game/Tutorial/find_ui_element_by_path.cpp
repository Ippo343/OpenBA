// code/Game/Tutorial/find_ui_element_by_path.cpp
//
// 0x1400E1870 — FindUIElementByPath(outElementIBP, String const& path, outDialog).
//
// This is the resolver behind every tutorial highlight and every "grey out the
// UI except this one control" step. Four call sites, all previously guarded by
// `if (!fn_FindUIElementByPath) DarkOnce(...)`:
//   TutWave_ShowListedElements   (tutorial_wave.cpp)   — the unhide list
//   TutWave_OpenHighlightDialog  (tutorial_wave.cpp)   — branch B target
//   TutorialGoalButton_Check     (tutorialgoalbutton.cpp)
//   TutorialGoalCount case 6/... (tutorialgoalcount.cpp)
//
// The path is colon-separated and its LENGTH selects the lookup:
//   "Dialog:element"                     find dialog by type, element by name
//   "Dialog:Controller:element"          also narrow by controller type; look in
//                                        the controller's container first, then
//                                        fall back to the dialog
//   "Dialog:Controller:element:resource" additionally require the dialog's
//                                        ComponentEntity (+0x30) to have been
//                                        built from that resource file — this is
//                                        how a step points at ONE building's
//                                        panel rather than any panel of its kind
//
// Ported from the disassembly at 0x1400E1870-0x1400E1CB6; the Ghidra pseudo-C
// drops the String argument of both FindElement overloads and the type-filter
// argument of GetController, so the register assignments were read from the
// instruction stream (see [[reference-getcontroller-third-arg-is-a-type-filter]]).
//
// Splitting: retail calls FUN_1400E0C90, which builds a DynArray<String> by
// walking the path and cutting at every ':' (and at the end). We build the same
// parts with System::String::SubString so every part is a real System::String —
// the two FindElement overloads and String::operator!= all take String const&.

#include "engine_funcs.h"
#include "resolve.h"           // RegisterExeAddr (the 0x1400e1870 row stays: engine_funcs reads it)
#include "address_registry.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "ui/UserInterface.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"                       // ENDGAME B.3.6 UI callers (K4)
#include "Entity/ComponentEntity.h"
#include <new>
#include "Tutorial/exe_bodies.h"

namespace {

constexpr int kMaxParts = 8;   // retail's DynArray grows; no shipped path is longer


// 0x1400E0C90 — cut `path` at every ':' and once more at the end.
// Returns the number of parts written into `parts` (each a constructed String).
int SplitPath(const System::String& path, System::String* parts) {
    const char* data = path._data;
    int len = data ? path.GetLength() : 0;
    if (!data || len <= 0) return 0;

    int count = 0;
    int start = 0;
    for (int i = 0; i <= len && count < kMaxParts; ++i) {
        bool cut = (i < len && data[i] == ':') || (i == len);
        if (!cut) continue;
        // SubString's end is INCLUSIVE (retail: R9D = EBX-2 at a colon, EBX-1 at
        // the final character), so the cut point i is one past the last character
        // we want.
        parts[count] = path.SubString(start, i - 1);
        ++count;
        start = i + 1;
    }
    return count;
}

// 0x1400105B0 — narrow a Base* to ComponentEntity, or null.
void* NarrowComponentEntity(void* obj) {
    if (!obj) return nullptr;
    return Object::IsDerivedFromNullSafe(obj, ComponentEntity::TypeId()) ? obj : nullptr;
}

// The 4-part tail: does this dialog's entity come from the named resource file?
// Retail compares the entity description's path filename (extension stripped)
// against parts[3] and SKIPS the dialog when they differ.
bool ResourceNameMismatch(void* dialog, const System::String& wantName) {
    void* owner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(dialog) + 0x30);
    if (!owner) return false;
    void* entity = NarrowComponentEntity(owner);

    uintptr_t entityIBP = 0;
    new (&entityIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    if (entityIBP == 0) return false;


    void* description = *reinterpret_cast<void**>(entityIBP + 0x28);
    if (!description) return false;

    System::Filename fullPath =
        reinterpret_cast<const ::Object::External*>(description)->GetPath();  // B.3.3 bridge
    System::Filename leafName = fullPath.GetPathFilename(true);
    System::String   nameStr(leafName.AsString());

    return !(nameStr == wantName);
}

}  // namespace

extern "C" void* FindUIElementByPath_Impl(void* outElementIBP, void* pathStr, uint64_t* outDialog) {
    if (!outElementIBP) return outElementIBP;
    uint64_t* out = reinterpret_cast<uint64_t*>(outElementIBP);
    *out = 0;

    if (!pathStr) return outElementIBP;
    const System::String& path = *reinterpret_cast<const System::String*>(pathStr);
    if (path._data == nullptr || path.GetLength() == 0) {
        return outElementIBP;
    }

    System::String parts[kMaxParts];
    int partCount = SplitPath(path, parts);
    // Fewer than two parts is not a path — retail returns a null InternalBasePtr.
    if (partCount < 2) {
        return outElementIBP;
    }

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) {
        return outElementIBP;
    }
    void* factory = ::Object::Factory::_singleton;
    if (!factory) return outElementIBP;

    uint32_t dialogTypeId = reinterpret_cast<const ::Object::Factory*>(factory)->GetTypeIdFromName(
        parts[0]._data, true);   // B.3.3 bridge
    if (dialogTypeId == 0) {
        return outElementIBP;
    }

    uint32_t controllerTypeId = 0;
    if (partCount >= 3)
        controllerTypeId = reinterpret_cast<const ::Object::Factory*>(factory)->GetTypeIdFromName(
            parts[1]._data, true);   // B.3.3 bridge

    UI::UserInterface* ui = *reinterpret_cast<UI::UserInterface**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);
    if (!ui) {
        return outElementIBP;
    }

    // The open-dialog list. `_dialogs.GetFirst()` IS retail's GetFirstDialog
    // (0x180031150 = `*retval = this->_dialogs._head`), and the iterator is the
    // bare node pointer, so `it.node` is the same address retail hands to
    // CheckPtr and `it.node->element` the InternalBasePtr(DialogInstance*).
    uint64_t found = 0;
    for (auto it = ui->_dialogs.GetFirst(); it && found == 0; ++it) {
        void* dialog = it.node->element;
        if (!dialog ||
            !reinterpret_cast<const Object::Base*>(dialog)->IsA(dialogTypeId)) continue;   // B.3.x bridge

        if (controllerTypeId == 0 || partCount < 3) {
            // "Dialog:element" — the element is looked up on the dialog itself.
            if (outDialog) *outDialog = reinterpret_cast<uint64_t>(dialog);
            uint64_t elemIBP = 0;
            ::new (&elemIBP) Object::InternalPtr<UI::Element>(
                reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(parts[1]));  // B.3.6 bridge
            if (elemIBP) found = elemIBP;
            continue;
        }

        // "Dialog:Controller:element[:resource]"
        uint64_t controller = 0;
        ::new (&controller) Object::InternalPtr<UI::DialogController>(
            reinterpret_cast<UI::DialogInstance*>(dialog)->GetController(controllerTypeId));  // B.3.6 bridge
        if (controller == 0) continue;

        if (partCount == 4) {
            if (ResourceNameMismatch(it.node->element, parts[3])) continue;
        }

        if (outDialog) *outDialog = reinterpret_cast<uint64_t>(dialog);

        // The controller's container first...
        uint64_t elemIBP = 0;
        {
            uint64_t containerIBP = 0;
            ::new (&containerIBP) Object::InternalPtr<UI::Element>(
                reinterpret_cast<UI::DialogController*>(controller)->GetContainer());  // B.3.6 bridge
            if (containerIBP) {
                ::new (&elemIBP) Object::InternalPtr<UI::Element>(
                    reinterpret_cast<UI::Element*>(containerIBP)->FindElement(parts[2]));  // B.3.6 bridge
            }
        }
        // ...then the dialog itself.
        if (elemIBP == 0) {
            ::new (&elemIBP) Object::InternalPtr<UI::Element>(
                reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(parts[2]));  // B.3.6 bridge
        }
        if (elemIBP) found = elemIBP;
    }

    *out = found;
    return outElementIBP;
}

extern "C" void RegisterFindUIElementByPath() {
    RegisterExeAddr(0x1400e1870, reinterpret_cast<uintptr_t>(&FindUIElementByPath_Impl));
}
