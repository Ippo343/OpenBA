// code/Game/Dialog/moddetailsdialog.cpp
// Clean reimplementation of ModDetailsDialog methods.
//
// ModDetailsDialog (0xC8 bytes): Shows mod details with action buttons.
// Layout: DialogInstance base (0xB8), +0xB8 = sprite handle, +0xC0 = mod index
//
// Mod struct (0x80 bytes) field offsets:
//   +0x00: name (System::String)
//   +0x08: author (System::String)
//   +0x10: description (System::String)
//   +0x18: version major (uint32_t, passed to GetVersionString)
//   +0x1c: version number (uint32_t, displayed)
//   +0x50: enabled flag (uint8_t)
//   +0x58: path (System::String, pointer to char*)
//   +0x60: resource BST root (Filename* node)
//   +0x70: conflict array pointer (Filename*[])
//   +0x78: conflict count (int)

#include "Dialog/ui_dialogs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Modification.h"   // ENDGAME B.3.4 caller batch P
#include <cstdint>
#include <utility>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// File-local helper: ModDetails_StringArrayPush (FUN_14006e290)
// ============================================================
// Appends each byte of a System::String to a 1-byte-stride dynamic array,
// then destructs the string (takes ownership).
//
// arrayBase layout: [data* at +0][count int32 at +8][capacity int32 at +0xc]
// Equivalent to: for each char in str, push it as a byte; then dtor str.
//
// Original: FUN_14006e290.
//
// B.3.2 bridge: the array is Collection::Array<uint8_t> (stride 1, align 1 —
// retail's fn_MemAlloc align argument was 1, matching alignof(uint8_t)); reached
// via the real template directly rather than through GameColl::ArrayAt since
// neither of those bridge helpers covers a 1-byte stride.
static void ModDetails_StringArrayPush_Impl(void* arrayBasePtr, System::String&& str) {
    uintptr_t strData = (uintptr_t)str._data;
    int strLen = str.GetLength();
    auto& arr = *reinterpret_cast<Collection::Array<uint8_t>*>(arrayBasePtr);
    for (int i = 0; i < strLen; i++) {
        char ch = *(char*)(strData + i);
        arr.SetSize(arr.GetSize() + 1, (uint8_t)ch);
    }
    // `str` is bound by rvalue reference to a caller-owned temporary; the
    // caller's own scope (see call sites) releases it once this returns.
}

// ============================================================
// Helper: find element by name, cast to typed pointer
// ============================================================
static void* FindElem(void* dialog, const char* name) {
    System::String nameStr(name);
    uint8_t retval[16];
    fn_DialogInstance_FindElement(dialog, retval, &nameStr);
    return *(void**)retval;
}

// ============================================================
// Helper: BST leftmost node (in-order start)
// BST node layout: +0x00 = Filename data, +0x08 = parent, +0x10 = left, +0x18 = right
// ============================================================
static void* BSTLeftmost(void* root) {
    if (!root) return nullptr;
    void* cur = root;
    while (*(void**)((uintptr_t)cur + 0x10))
        cur = *(void**)((uintptr_t)cur + 0x10);
    return cur;
}

// ============================================================
// Helper: BST in-order successor
// ============================================================
static void* BSTNext(void* node) {
    if (!node) return nullptr;
    uintptr_t n = (uintptr_t)node;

    // If right child exists, go right then all the way left
    void* right = *(void**)(n + 0x18);
    if (right) {
        void* cur = right;
        while (*(void**)((uintptr_t)cur + 0x10))
            cur = *(void**)((uintptr_t)cur + 0x10);
        return cur;
    }

    // Go up: if we came from a left child, parent is next
    void* parent = *(void**)(n + 0x08);
    if (parent) {
        if (*(void**)((uintptr_t)parent + 0x10) == node)
            return parent;
        // Came from right — walk up until we come from a left child
        void* cur = node;
        while (parent && *(void**)((uintptr_t)parent + 0x18) == cur) {
            cur = parent;
            parent = *(void**)((uintptr_t)cur + 0x08);
        }
    }
    return parent;
}

// ============================================================
// Helper: search BST for filename match using Filename::operator<
// ============================================================
static bool BSTContains(void* root, void* filename) {
    System::Filename& fnameRef = *reinterpret_cast<System::Filename*>(filename); // B.3.1 bridge
    void* node = root;
    while (node) {
        System::Filename& nodeRef = *reinterpret_cast<System::Filename*>(node); // B.3.1 bridge
        if (fnameRef < nodeRef) {
            node = *(void**)((uintptr_t)node + 0x10); // left
        } else if (nodeRef < fnameRef) {
            node = *(void**)((uintptr_t)node + 0x18); // right
        } else {
            return true; // equal (neither < nor >)
        }
    }
    return false;
}

// ============================================================
// Slot4 (0x14006D700): Setup — populates mod details UI
// ============================================================
void ModDetailsDialog_Slot4(ModDetailsDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find 12 UI elements ---
    void* buttonOk = FindElem(self, "buttonOk");
    fn_Element_SetElementId(buttonOk, 0);
    *(uint32_t*)((uintptr_t)buttonOk + 0xC0) = 8;

    void* buttonCreate = FindElem(self, "buttonCreate");
    fn_Element_SetElementId(buttonCreate, 1);

    void* buttonUpdate = FindElem(self, "buttonUpdate");
    fn_Element_SetElementId(buttonUpdate, 2);

    void* imageIcon       = FindElem(self, "imageIcon");
    void* textName        = FindElem(self, "textName");
    void* textPath        = FindElem(self, "textPath");
    void* textAuthor      = FindElem(self, "textAuthor");
    void* textVersion     = FindElem(self, "textVersion");
    void* textDescription = FindElem(self, "textDescription");
    void* textConflictTitle = FindElem(self, "textConflictTitle");
    void* textConflictList  = FindElem(self, "textConflictList");
    void* textResourceList  = FindElem(self, "textResourceList");

    // --- Get mod data ---
    void* modification = *g_ppModificationSingleton;
    int modIndex = *(int*)(s + 0xC0);
    void* mod = const_cast<RtIO::Mod*>(
        &reinterpret_cast<const IO::Modification*>(modification)->GetModification(modIndex));   // B.3.4 bridge
    uintptr_t m = (uintptr_t)mod;

    // --- Hide create/update buttons ---
    fn_Element_Hide(buttonCreate, true);
    fn_Element_Hide(buttonUpdate, true);

    // --- Set icon sprite from +0xB8 sprite handle ---
    // Retail (0x14006dca6) calls dispatch slot 0x140151898 =
    // ??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z -- the
    // BY-INDEX ctor with index 0, same as ModDialog's row icon. The by-NAME
    // overload that used to be here dereferences its String* (READ of 0).
    uint8_t sprite[16];
    fn_Sprite_Ctor_ByIndex(sprite, *(void**)(s + 0xB8), 0);
    fn_ImageElement_SetSprite(imageIcon, sprite);

    // --- Set text parameters ---
    // Name (mod+0x00 = String)
    fn_TextElement_SetParameter(textName, 0, (void*)m);

    // Author (mod+0x08 = String)
    fn_TextElement_SetParameter(textAuthor, 0, (void*)(m + 0x08));

    // Path (mod+0x58 = String, dereference to char*)
    {
        System::String pathStr(*(const char**)(m + 0x58));
        fn_TextElement_SetParameter(textPath, 0, &pathStr);
    }

    // Version: "%i (%s)" with version number and version string
    {
        System::String versionStr;
        fn_Configuration_GetVersionString(&versionStr, *(uint32_t*)(m + 0x18), false);
        System::String formatted = System::String::Format("%i (%s)", (uint64_t)*(uint32_t*)(m + 0x1c),
                         versionStr._data);
        fn_TextElement_SetParameter(textVersion, 0, &formatted);
    }

    // Description (mod+0x10 = String)
    fn_TextElement_SetParameter(textDescription, 0, (void*)(m + 0x10));

    // --- Build conflict list markup ---
    int conflictCount = *(int*)(m + 0x78);
    System::String conflictResult("^jl^f0^c0^lb");

    for (int i = 0; i < conflictCount; i++) {
        void** conflictArray = *(void***)(m + 0x70);
        int cIdx = reinterpret_cast<const IO::Modification*>(modification)
                       ->Find(*reinterpret_cast<const RtSystem::Filename*>(conflictArray[i]));   // B.3.4 bridge
        void* cMod = const_cast<RtIO::Mod*>(
            &reinterpret_cast<const IO::Modification*>(modification)->GetModification(cIdx));   // B.3.4 bridge
        uintptr_t cm = (uintptr_t)cMod;

        // Color: ^li^c1 if enabled, ^li^c0 if disabled
        const char* prefix = (*(uint8_t*)(cm + 0x50) != 0) ? "^li^c1" : "^li^c0";

        // Build entry: prefix + name + " ^n[" + path + "]^n"
        const char* modName = *(const char**)cm;
        const char* modPath = *(const char**)(cm + 0x58);
        System::String entry = System::String::Format("%s%s ^n[%s]^n", (uint64_t)(uintptr_t)prefix,
                         (uint64_t)(uintptr_t)modName,
                         (uint64_t)(uintptr_t)modPath);
        // retail's operator+= returns a copy of the new value that the
        // original discarded (see the Append note); genuinely unused here.
        conflictResult += entry;
    }

    // Append closing tag
    {
        System::String closeTag("^le");
        conflictResult += closeTag;
    }

    if (conflictCount == 0) {
        // Hide conflict title and list when no conflicts
        fn_Element_SetFlagsOnOff(textConflictTitle, 1, 0);
        fn_Element_SetFlagsOnOff(textConflictList, 1, 0);
    } else {
        // Set conflict list text (DocumentTextElement)
        fn_DocumentTextElement_SetText(textConflictList, &conflictResult);
    }

    // --- Build resource list ---
    // Dynamic string array: { void* ptr; int count; int capacity; }
    uint8_t resArray[16];
    *(void**)resArray = nullptr;
    *(uint64_t*)(resArray + 8) = 0;

    // Push header
    {
        System::String hdr("^jl^f0^c0^lb");
        ModDetails_StringArrayPush_Impl(resArray, std::move(hdr));
    }

    // Traverse resource BST in-order
    void* bstRoot = *(void**)(m + 0x60);
    if (bstRoot) {
        void* node = BSTLeftmost(bstRoot);
        while (node) {
            // For each resource file, check conflicts with enabled mods
            bool hasConflict = false;
            bool higherPriority = false;

            for (int i = 0; i < conflictCount; i++) {
                void** cArray = *(void***)(m + 0x70);
                int cIdx = reinterpret_cast<const IO::Modification*>(modification)
                               ->Find(*reinterpret_cast<const RtSystem::Filename*>(cArray[i]));   // B.3.4 bridge
                void* cMod = const_cast<RtIO::Mod*>(
                    &reinterpret_cast<const IO::Modification*>(modification)->GetModification(cIdx));   // B.3.4 bridge

                if (*(uint8_t*)((uintptr_t)cMod + 0x50) != 0) {
                    void* cBstRoot = *(void**)((uintptr_t)cMod + 0x60);
                    if (BSTContains(cBstRoot, node)) {
                        hasConflict = true;
                        if (cIdx < modIndex) {
                            higherPriority = true;
                        }
                    }
                    // Restore modification pointer (decompilation shows reload)
                    modification = *g_ppModificationSingleton;
                }
            }

            // Color prefix
            const char* prefix;
            if (!hasConflict)
                prefix = "^li^c0";
            else if (higherPriority)
                prefix = "^li^c2";
            else
                prefix = "^li^c1";

            // Build entry: prefix + filename + "^n"
            const char* filename = *(const char**)node;
            System::String entryStr = System::String::Format("%s%s^n", (uint64_t)(uintptr_t)prefix,
                             (uint64_t)(uintptr_t)filename);
            ModDetails_StringArrayPush_Impl(resArray, std::move(entryStr));
            // Note: ModDetails_StringArrayPush_Impl reads entryStr in place; it
            // is released here when this iteration's scope ends.

            node = BSTNext(node);
        }
    }

    // Push footer
    {
        System::String ftr("^le");
        ModDetails_StringArrayPush_Impl(resArray, std::move(ftr));
    }

    // Null-terminate the byte buffer (original code does this inline after last
    // push). Same Collection::Array<uint8_t> as ModDetails_StringArrayPush_Impl.
    {
        auto& arr = *reinterpret_cast<Collection::Array<uint8_t>*>(resArray);
        arr.SetSize(arr.GetSize() + 1, (uint8_t)'\0');
    }

    // Build String from null-terminated buffer, set on textResourceList
    {
        char* data = (char*)*(void**)resArray;
        System::String resultStr(data ? data : "");
        fn_DocumentTextElement_SetText(textResourceList, &resultStr);
    }

    // Free resource array buffer
    fn_MemFreeStatic(*(void**)resArray);
}

// ============================================================
// ModDetailsDialog_OpenDialog_Impl — opens a dialog from .rsc resource
// ============================================================
// Shared by OpenUpdateDialog and DeleteMod (both open "Dialog/ModUpdate.rsc").
// Original: FUN_14006d400 (OpenUpdateDialog) and FUN_14006d550 (DeleteMod).
// Both are structurally identical: load resource, create 0xB8 DialogInstance,
// set vtable to PTR_FUN_14015c6c0 (ModUpdateDialog), add to UI.
static void ModDetailsDialog_OpenDialog_Impl(void* self, const char* rscPath) {
    uintptr_t s = (uintptr_t)self;

    // Load dialog resource
    System::Filename filenameBuf(rscPath);
    void* extFactory = *(void**)g_pExternalFactorySingleton;
    void* ext = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(extFactory)   // B.3.3 bridge
            ->FindOrLoad(filenameBuf));
    // IsDerivedFrom check for Dialog type — the resource IS a Dialog if it loads
    // from a .rsc file, so we pass it directly (matches original behavior where
    // the check always succeeds for valid resources).
    void* dialog = ext;

    // Allocate and construct DialogInstance (0xB8 bytes)
    void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    void* dialogInst = nullptr;
    if (newDialog) {
        uint64_t nullRef;
        new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        fn_DialogInstance_Ctor(newDialog, *(void**)(s + 0x10), dialog, &nullRef, 0, 0, 0);
        // Set vtable to ModUpdateDialog
        if (g_ModUpdateDialog_Vtable) {
            *(uintptr_t*)newDialog = g_ModUpdateDialog_Vtable;
        }
        dialogInst = newDialog;
    }

    // Add to UI
    uint64_t dialogRef;
    new (&dialogRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dialogInst));   // B.3.3 bridge
    fn_UI_AddDialog(*(void**)(s + 0x10), &dialogRef);
}

// ============================================================
// Slot8 (0x14006D6A0): OnCommand — button dispatch.
// ============================================================
void ModDetailsDialog_Slot8(ModDetailsDialog* self, uint64_t /*param2*/,
                            int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 != 0x12)
        return;

    if (param4 == 0) {
        uint8_t basePtr[16];
        new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, basePtr);
    } else if (param4 == 1) {
        // OpenUpdateDialog — opens ModUpdate.rsc dialog
        ModDetailsDialog_OpenDialog_Impl(self, "Dialog/ModUpdate.rsc");
    } else if (param4 == 2) {
        // DeleteMod — also opens ModUpdate.rsc dialog (same behavior as OpenUpdateDialog)
        ModDetailsDialog_OpenDialog_Impl(self, "Dialog/ModUpdate.rsc");
    }
}
