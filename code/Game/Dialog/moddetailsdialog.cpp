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
#include "engine_funcs.h"
#include <cstdint>

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
static void ModDetails_StringArrayPush_Impl(void* arrayBasePtr, void* strPtr) {
    uintptr_t* base = (uintptr_t*)arrayBasePtr;
    uintptr_t strData = *(uintptr_t*)strPtr;
    int strLen = 0;
    if (strData) {
        strLen = *(int*)(strData - 8);
    }
    for (int i = 0; i < strLen; i++) {
        char ch = *(char*)(strData + i);
        // Byte-array push of one element
        void* oldData = (void*)*base;
        int* countPtr    = (int*)((uintptr_t)base + 8);
        int* capacityPtr = (int*)((uintptr_t)base + 0xC);
        int oldCount = *countPtr;
        int newCount = oldCount + 1;
        *countPtr = newCount;
        if (*capacityPtr < newCount) {
            int newCap = newCount;
            if (newCount == oldCount + 1 && oldCount != 0)
                newCap = newCount * 3;
            *capacityPtr = newCap;
            void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uintptr_t)newCap, 1);
            if (oldData && oldData != newData) {
                for (int j = 0; j < oldCount; j++)
                    ((char*)newData)[j] = ((char*)oldData)[j];
                fn_MemFreeStatic(oldData);
            }
            *base = (uintptr_t)newData;
        }
        ((char*)*base)[oldCount] = ch;
    }
    // Dtor the string (function takes ownership)
    fn_String_Dtor(strPtr);
}

// ============================================================
// Helper: find element by name, cast to typed pointer
// ============================================================
static void* FindElem(void* dialog, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint8_t retval[16];
    fn_DialogInstance_FindElement(dialog, retval, nameStr);
    fn_String_Dtor(nameStr);
    fn_InternalBasePtr_CheckPtr(retval);
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
    void* node = root;
    while (node) {
        if (fn_Filename_Less(filename, node)) {
            node = *(void**)((uintptr_t)node + 0x10); // left
        } else if (fn_Filename_Less(node, filename)) {
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
    void* mod = fn_Modification_GetMod(modification, modIndex);
    uintptr_t m = (uintptr_t)mod;

    // --- Hide create/update buttons ---
    fn_Element_Hide(buttonCreate, true);
    fn_Element_Hide(buttonUpdate, true);

    // --- Set icon sprite from +0xB8 sprite handle ---
    uint8_t sprite[16];
    fn_Sprite_Ctor(sprite, *(uint64_t*)(s + 0xB8), 0);
    fn_ImageElement_SetSprite(imageIcon, sprite);

    // --- Set text parameters ---
    // Name (mod+0x00 = String)
    fn_TextElement_SetParameter(textName, 0, (void*)m);

    // Author (mod+0x08 = String)
    fn_TextElement_SetParameter(textAuthor, 0, (void*)(m + 0x08));

    // Path (mod+0x58 = String, dereference to char*)
    {
        uint8_t pathStr[8];
        fn_String_Ctor_CStr(pathStr, *(const char**)(m + 0x58));
        fn_TextElement_SetParameter(textPath, 0, pathStr);
        fn_String_Dtor(pathStr);
    }

    // Version: "%i (%s)" with version number and version string
    {
        uint8_t versionStr[8];
        fn_Configuration_GetVersionString(versionStr, *(uint32_t*)(m + 0x18), false);
        uint8_t formatted[8];
        fn_String_Format(formatted, "%i (%s)", (uint64_t)*(uint32_t*)(m + 0x1c),
                         *(const char**)versionStr);
        fn_TextElement_SetParameter(textVersion, 0, formatted);
        fn_String_Dtor(formatted);
        fn_String_Dtor(versionStr);
    }

    // Description (mod+0x10 = String)
    fn_TextElement_SetParameter(textDescription, 0, (void*)(m + 0x10));

    // --- Build conflict list markup ---
    int conflictCount = *(int*)(m + 0x78);
    uint8_t conflictResult[8];
    fn_String_Ctor_CStr(conflictResult, "^jl^f0^c0^lb");

    for (int i = 0; i < conflictCount; i++) {
        void** conflictArray = *(void***)(m + 0x70);
        int cIdx = fn_Modification_Find(modification, conflictArray[i]);
        void* cMod = fn_Modification_GetMod(modification, cIdx);
        uintptr_t cm = (uintptr_t)cMod;

        // Color: ^li^c1 if enabled, ^li^c0 if disabled
        const char* prefix = (*(uint8_t*)(cm + 0x50) != 0) ? "^li^c1" : "^li^c0";

        // Build entry: prefix + name + " ^n[" + path + "]^n"
        const char* modName = *(const char**)cm;
        const char* modPath = *(const char**)(cm + 0x58);
        uint8_t entry[8];
        fn_String_Format(entry, "%s%s ^n[%s]^n", (uint64_t)(uintptr_t)prefix,
                         (uint64_t)(uintptr_t)modName,
                         (uint64_t)(uintptr_t)modPath);
        fn_String_AppendStr(conflictResult, entry);
        fn_String_Dtor(entry);
    }

    // Append closing tag
    {
        uint8_t closeTag[8];
        fn_String_Ctor_CStr(closeTag, "^le");
        fn_String_AppendStr(conflictResult, closeTag);
        fn_String_Dtor(closeTag);
    }

    if (conflictCount == 0) {
        // Hide conflict title and list when no conflicts
        fn_Element_SetFlagsOnOff(textConflictTitle, 1, 0);
        fn_Element_SetFlagsOnOff(textConflictList, 1, 0);
    } else {
        // Set conflict list text (DocumentTextElement)
        fn_DocumentTextElement_SetText(textConflictList, conflictResult);
    }

    // --- Build resource list ---
    // Dynamic string array: { void* ptr; int count; int capacity; }
    uint8_t resArray[16];
    *(void**)resArray = nullptr;
    *(uint64_t*)(resArray + 8) = 0;

    // Push header
    {
        uint8_t hdr[8];
        fn_String_Ctor_CStr(hdr, "^jl^f0^c0^lb");
        ModDetails_StringArrayPush_Impl(resArray, hdr);
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
                int cIdx = fn_Modification_Find(modification, cArray[i]);
                void* cMod = fn_Modification_GetMod(modification, cIdx);

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
            uint8_t entryStr[8];
            fn_String_Format(entryStr, "%s%s^n", (uint64_t)(uintptr_t)prefix,
                             (uint64_t)(uintptr_t)filename);
            ModDetails_StringArrayPush_Impl(resArray, entryStr);
            // Note: ModDetails_StringArrayPush_Impl takes ownership and dtors strPtr.

            node = BSTNext(node);
        }
    }

    // Push footer
    {
        uint8_t ftr[8];
        fn_String_Ctor_CStr(ftr, "^le");
        ModDetails_StringArrayPush_Impl(resArray, ftr);
    }

    // Null-terminate the byte buffer (original code does this inline after last push)
    {
        int count = *(int*)(resArray + 8);
        int capacity = *(int*)(resArray + 12);
        int newCount = count + 1;
        char* oldPtr = (char*)*(void**)resArray;
        *(int*)(resArray + 8) = newCount;
        if (capacity < newCount) {
            int newCap = (count != 0) ? newCount * 3 : newCount;
            *(int*)(resArray + 12) = newCap;
            char* newBuf = (char*)fn_MemAlloc(g_pMemAllocSingleton, newCap, 1);
            *(void**)resArray = newBuf;
            if (oldPtr && newBuf != oldPtr) {
                for (int i = 0; i < count; i++)
                    newBuf[i] = oldPtr[i];
                fn_MemFreeStatic(oldPtr);
            }
            oldPtr = newBuf;
        }
        oldPtr[count] = '\0';
    }

    // Build String from null-terminated buffer, set on textResourceList
    {
        char* data = (char*)*(void**)resArray;
        uint8_t resultStr[8];
        fn_String_Ctor_CStr(resultStr, data ? data : "");
        fn_DocumentTextElement_SetText(textResourceList, resultStr);
        fn_String_Dtor(resultStr);
    }

    // Free resource array buffer
    fn_MemFreeStatic(*(void**)resArray);

    fn_String_Dtor(conflictResult);
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
    uint8_t filenameBuf[8];
    fn_Filename_CtorCStr(filenameBuf, rscPath);
    void* extFactory = *(void**)g_pExternalFactorySingleton;
    void* ext = fn_ExternalFactory_FindOrLoad(extFactory, filenameBuf);
    // IsDerivedFrom check for Dialog type — the resource IS a Dialog if it loads
    // from a .rsc file, so we pass it directly (matches original behavior where
    // the check always succeeds for valid resources).
    void* dialog = ext;
    fn_Filename_Dtor(filenameBuf);

    // Allocate and construct DialogInstance (0xB8 bytes)
    void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    void* dialogInst = nullptr;
    if (newDialog) {
        uint64_t nullRef;
        fn_InternalBasePtr_Ctor(&nullRef, nullptr);
        fn_DialogInstance_Ctor(newDialog, *(void**)(s + 0x10), dialog, &nullRef, 0, 0, 0);
        // Set vtable to ModUpdateDialog
        if (g_ModUpdateDialog_Vtable) {
            *(uintptr_t*)newDialog = g_ModUpdateDialog_Vtable;
        }
        dialogInst = newDialog;
    }

    // Add to UI
    uint64_t dialogRef;
    fn_InternalBasePtr_Ctor(&dialogRef, dialogInst);
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
        fn_InternalBasePtr_Ctor(basePtr, self);
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
