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
#include "resolve.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kMaxParts = 8;   // retail's DynArray grows; no shipped path is longer


// A System::String is one pointer; its length lives at data[-8].
inline const char* StrData(const void* str) { return *reinterpret_cast<const char* const*>(str); }
inline int StrLen(const void* str) {
    const char* d = StrData(str);
    return d ? *reinterpret_cast<const int32_t*>(d - 8) : 0;
}

// 0x1400E0C90 — cut `path` at every ':' and once more at the end.
// Returns the number of parts written into `parts` (each a constructed String).
int SplitPath(const void* path, uint8_t parts[][8]) {
    const char* data = StrData(path);
    int len = data ? StrLen(path) : 0;
    if (!data || len <= 0 || !fn_String_SubString) return 0;

    int count = 0;
    int start = 0;
    for (int i = 0; i <= len && count < kMaxParts; ++i) {
        bool cut = (i < len && data[i] == ':') || (i == len);
        if (!cut) continue;
        // SubString(self, retval, startIndex, endIndex) — end is INCLUSIVE
        // (retail: R9D = EBX-2 at a colon, EBX-1 at the final character), so the
        // cut point i is one past the last character we want.
        fn_String_SubString(const_cast<void*>(path), parts[count], start, i - 1);
        ++count;
        start = i + 1;
    }
    return count;
}

void FreeParts(uint8_t parts[][8], int count) {
    if (!fn_String_Dtor) return;
    for (int i = 0; i < count; ++i) fn_String_Dtor(parts[i]);
}

// 0x1400105B0 — narrow a Base* to ComponentEntity, or null.
void* NarrowComponentEntity(void* obj) {
    if (!obj || !fn_IsDerivedFrom || !g_TypeId_ComponentEntity) return nullptr;
    return fn_IsDerivedFrom(obj, *g_TypeId_ComponentEntity) ? obj : nullptr;
}

// The 4-part tail: does this dialog's entity come from the named resource file?
// Retail compares the entity description's path filename (extension stripped)
// against parts[3] and SKIPS the dialog when they differ.
bool ResourceNameMismatch(void* dialog, const void* wantName) {
    if (!fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return false;
    void* owner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(dialog) + 0x30);
    if (!owner) return false;
    void* entity = NarrowComponentEntity(owner);

    uintptr_t entityIBP = 0;
    fn_InternalBasePtr_Ctor(&entityIBP, entity);
    if (entityIBP == 0) return false;
    fn_InternalBasePtr_CheckPtr(&entityIBP);

    if (!fn_External_GetPath || !fn_Filename_GetPathFilename ||
        !fn_String_Ctor_Copy || !fn_Filename_Dtor || !fn_String_Dtor) return false;

    void* description = *reinterpret_cast<void**>(entityIBP + 0x28);
    if (!description) return false;

    uint8_t fullPath[8] = {};
    void* fp = fn_External_GetPath(description, fullPath);
    uint8_t leafName[8] = {};
    void* leaf = fn_Filename_GetPathFilename(fp ? fp : fullPath, leafName, true);
    uint8_t nameStr[8] = {};
    fn_String_Ctor_Copy(nameStr, leaf ? leaf : leafName);
    fn_Filename_Dtor(leafName);
    fn_Filename_Dtor(fullPath);

    bool differs = fn_String_Equal ? !fn_String_Equal(nameStr, wantName) : false;
    fn_String_Dtor(nameStr);
    return differs;
}

}  // namespace

extern "C" void* FindUIElementByPath_Impl(void* outElementIBP, void* pathStr, uint64_t* outDialog) {
    if (!outElementIBP) return outElementIBP;
    uint64_t* out = reinterpret_cast<uint64_t*>(outElementIBP);
    *out = 0;

    if (!pathStr || StrData(pathStr) == nullptr || StrLen(pathStr) == 0) {
        return outElementIBP;
    }

    uint8_t parts[kMaxParts][8] = {};
    int partCount = SplitPath(pathStr, parts);
    // Fewer than two parts is not a path — retail returns a null InternalBasePtr.
    if (partCount < 2) {
        FreeParts(parts, partCount); return outElementIBP;
    }

    if (!g_pFactorySingleton || !*g_pFactorySingleton || !fn_Factory_GetTypeIdFromName ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton ||
        !fn_UI_GetFirstDialog || !fn_IsA || !fn_InternalBasePtr_CheckPtr) {
        FreeParts(parts, partCount);
        return outElementIBP;
    }
    void* factory = *reinterpret_cast<void**>(*g_pFactorySingleton);
    if (!factory) { FreeParts(parts, partCount); return outElementIBP; }

    uint32_t dialogTypeId = fn_Factory_GetTypeIdFromName(factory, StrData(parts[0]), true);
    if (dialogTypeId == 0) {
        FreeParts(parts, partCount); return outElementIBP;
    }

    uint32_t controllerTypeId = 0;
    if (partCount >= 3)
        controllerTypeId = fn_Factory_GetTypeIdFromName(factory, StrData(parts[1]), true);

    void* ui = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);
    if (!ui) {
        FreeParts(parts, partCount); return outElementIBP;
    }

    // Iterator node: [0] = InternalBasePtr(DialogInstance*), [0x10] = next node.
    uintptr_t nodeBuf[4] = {};
    fn_UI_GetFirstDialog(ui, nodeBuf);
    uintptr_t node = nodeBuf[0];

    uint64_t found = 0;
    for (; node != 0 && found == 0; node = *reinterpret_cast<uintptr_t*>(node + 0x10)) {
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
        void* dialog = *reinterpret_cast<void**>(node);
        if (!dialog || !fn_IsA(dialog, dialogTypeId)) continue;

        if (controllerTypeId == 0 || partCount < 3) {
            // "Dialog:element" — the element is looked up on the dialog itself.
            if (outDialog) *outDialog = reinterpret_cast<uint64_t>(dialog);
            if (!fn_DialogInstance_FindElement) continue;
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
            uint64_t elemIBP = 0;
            fn_DialogInstance_FindElement(dialog, &elemIBP, parts[1]);
            if (elemIBP) found = elemIBP;
            continue;
        }

        // "Dialog:Controller:element[:resource]"
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
        uint64_t controller = 0;
        if (fn_DialogInstance_GetController)
            fn_DialogInstance_GetController(dialog, &controller, controllerTypeId);
        if (controller == 0) continue;

        if (partCount == 4) {
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
            if (ResourceNameMismatch(*reinterpret_cast<void**>(node), parts[3])) continue;
        }

        if (outDialog) *outDialog = reinterpret_cast<uint64_t>(dialog);

        // The controller's container first...
        uint64_t elemIBP = 0;
        if (fn_DialogController_GetContainer && fn_Element_FindElement) {
            fn_InternalBasePtr_CheckPtr(&controller);
            uint64_t containerIBP = 0;
            fn_DialogController_GetContainer(reinterpret_cast<void*>(controller), &containerIBP);
            if (containerIBP) {
                fn_InternalBasePtr_CheckPtr(&containerIBP);
                fn_Element_FindElement(reinterpret_cast<void*>(containerIBP), &elemIBP, parts[2]);
            }
        }
        // ...then the dialog itself.
        if (elemIBP == 0 && fn_DialogInstance_FindElement) {
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
            fn_DialogInstance_FindElement(dialog, &elemIBP, parts[2]);
        }
        if (elemIBP) found = elemIBP;
    }

    *out = found;
    FreeParts(parts, partCount);
    return outElementIBP;
}

extern "C" void RegisterFindUIElementByPath() {
    RegisterExeAddr(0x1400e1870, reinterpret_cast<uintptr_t>(&FindUIElementByPath_Impl));
}
