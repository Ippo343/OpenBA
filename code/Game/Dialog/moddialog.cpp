// code/Game/Dialog/moddialog.cpp
// Clean reimplementation of ModDialog vtable methods.
//
// ModDialog (0xD8 bytes), vtable at 0x14015c870.
// Extends DialogInstance (0xB8 base).
//
// Layout:
//   +0xB8: dynamic array ptr (mod entry cache, stride 0x58)
//   +0xC0: array count
//   +0xC8: table element (InternalPtr)
//   +0xD0: zeroed
//
// Reimplemented:
//   Slot4 (0x14006F150) — Setup: populates mod list table, thumbnail pipeline
//   Slot8 (0x14006ED40) — OnCommand: handles all mod list button clicks
//
// Not hookable (Runtime thunks):
//   Slot7, Slot9 — 6-byte JMP [rip+xx] to Runtime, cannot be hooked
//
// Entry struct (0x58 bytes = 11 × 8):
//   +0x00: textModName   InternalPtr<Element>
//   +0x08: textModAuthor InternalPtr<Element>
//   +0x10: textModVersion InternalPtr<Element>
//   +0x18: textModDescription InternalPtr<Element>
//   +0x20: imageConflict InternalPtr<Element>
//   +0x28: checkLoaded   InternalPtr<Element>
//   +0x30: vtChild       InternalPtr<Element>  (first child via vtable[0x58])
//   +0x38: imageIcon     ImageElement* (InternalPtr)
//   +0x40: ImageBuffer*
//   +0x48: MaterialInstance*
//   +0x50: SpriteSheet*

#include "ui/Dialogs.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Thumbnail vtable stamps — see code/Game/Dialog/thumb_vtables.cpp.
extern uintptr_t ThumbVtable_StampMaterialInstance(void* obj);
extern uintptr_t ThumbVtable_StampSpriteSheet(void* obj);

// ============================================================
// Helper types
// ============================================================

// Vtable call helpers for Element (via raw vtable pointer reads)
typedef void* (*fn_ElementVt_Child_t)(void* self, void* retbuf, int index);
typedef void  (*fn_ElementVt_Hide_t)(void* self, int hide);

// ImageBuffer::Create: vtable+0x48 (via g_NoiseUI_ImageBufferVtable). Mangled:
//   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@
//   W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
// i.e. (ImageType, Usage, Format, uint mip, int w, int h, FilterMode, AddressMode x3,
//        Array<Array<uchar>>* levels). The LAST parameter is a POINTER: it lives in a
// stack slot, and passing it as `int 0` writes only the low dword, leaving the high
// dword as whatever was there (0x00007ffc... on the first Mods click) -- which
// ImageBufferData_Create then dereferenced as `levels->count`.
typedef bool (*fn_ImageBuffer_Create_t)(void* self, int type, int usage, int format,
                                        unsigned mip, int w, int h,
                                        int filter, int addrU, int addrV, int addrW,
                                        void* levels);

// ============================================================
// Filename equality check — compares two System::Filename/String objects
// by their underlying string data. Both are 8-byte COW pointers.
// ============================================================
static bool Filename_Equals(const void* filenameA, const void* filenameB) {
    const char* a = *(const char**)filenameA;
    const char* b = *(const char**)filenameB;
    if (a == b) return true;
    if (!a || !b) return false;
    // Compare using C string comparison (null-terminated)
    return strcmp(a, b) == 0;
}

// ============================================================
// ModDialog_RefreshDisplay_Impl (was fn_ModDialog_RefreshDisplay, FUN_14006eec0)
//
// Updates the mod list UI display. For each mod entry in the table:
//   1. Sets name, author, description, version text parameters
//   2. Checks for conflicts (same filename dependency in another enabled mod)
//   3. Shows/hides conflict icon
//   4. Copies thumbnail texture from mod data
//
// Entry struct (0x58 bytes) layout — see file header comment.
// ============================================================
static void ModDialog_RefreshDisplay_Impl(ModDialog* self);

// Exported under the retail name so standalone/reg_mods.cpp can register the
// retail address 0x14006eec0 to it. Seven exe call sites reach RefreshDisplay
// (ModDialog_OnRemove / _Serialize / the two move helpers / helper_e930 / two
// .rdata entries), and every one of them resolved to NULL before this.
extern "C" void ModDialog_RefreshDisplay(void* self) {
    ModDialog_RefreshDisplay_Impl(reinterpret_cast<ModDialog*>(self));
}

static void ModDialog_RefreshDisplay_Impl(ModDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* modSingleton = *(void**)g_ppModificationSingleton;
    if (!modSingleton) return;

    int modCount = fn_Modification_GetCount(modSingleton);
    if (modCount <= 0) return;

    uintptr_t entriesBase = *(uintptr_t*)(s + 0xB8);  // mod entry array

    for (int i = 0; i < modCount; i++) {
        uintptr_t entry = entriesBase + (uintptr_t)i * 0x58;

        // Get mod data
        void* mod = fn_Modification_GetMod(modSingleton, i);
        if (!mod) continue;

        uintptr_t m = (uintptr_t)mod;

        // Check for conflicts: another enabled mod depends on a file from this mod
        bool hasConflict = false;
        uint8_t enabled = *(uint8_t*)(m + 0x50);  // mod->m_enabled

        if (enabled != 0) {
            for (int j = 0; j < modCount; j++) {
                if (j == i) continue;

                void* otherMod = fn_Modification_GetMod(modSingleton, j);
                uintptr_t om = (uintptr_t)otherMod;
                uint8_t otherEnabled = *(uint8_t*)(om + 0x50);
                if (otherEnabled == 0) continue;

                // Check if this mod's filename appears in the other mod's dependency list
                int depCount = *(int*)(m + 0x78);  // mod->m_filenameArrayCount
                if (depCount <= 0) continue;

                uintptr_t depArray = *(uintptr_t*)(m + 0x70);  // mod->m_filenameArrayData
                for (int k = 0; k < depCount; k++) {
                    // Compare other mod's filename (+0x58) with this mod's dependency[k]
                    void* depFilename = (void*)(depArray + (int64_t)k * 8);
                    if (Filename_Equals((void*)(om + 0x58), depFilename)) {
                        hasConflict = true;
                        goto conflict_done;
                    }
                }
            }
        }
conflict_done:

        // Set text parameters: name, author, description
        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x00));
        fn_TextElement_SetParameter(*(void**)(entry + 0x00), 0, (void*)m);  // mod->m_name

        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x08));
        fn_TextElement_SetParameter(*(void**)(entry + 0x08), 0, (void*)(m + 0x08));  // mod->m_author

        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x18));
        fn_TextElement_SetParameter(*(void**)(entry + 0x18), 0, (void*)(m + 0x10));  // mod->m_description

        // Format version string: "versionMinor (versionString)"
        {
            uint32_t versionMajor = *(uint32_t*)(m + 0x18);
            uint32_t versionMinor = *(uint32_t*)(m + 0x1C);

            // GetVersionString returns a String for the version major (as game build version)
            uint8_t buildStr[8] = {};
            fn_Configuration_GetVersionString(buildStr, versionMajor, false);

            uint8_t formatted[8] = {};
            fn_String_Format(formatted, "%i (%s)", (uint64_t)versionMinor, *(const char**)buildStr);

            fn_InternalBasePtr_CheckPtr((void*)(entry + 0x10));
            fn_TextElement_SetParameter(*(void**)(entry + 0x10), 0, formatted);

            fn_String_Dtor(formatted);
            fn_String_Dtor(buildStr);
        }

        // Set enabled checkbox state: entry+0x28 = checkLoaded, element +0xD8 = enabled byte
        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x28));
        *(uint8_t*)(*(uintptr_t*)(entry + 0x28) + 0xD8) = enabled;

        // Update vtChild element with color (vtable[10] = offset +0x50)
        // Color encodes enabled/conflict state:
        //   0x20c0ffc0 = enabled, no conflict (green)
        //   0x40ff0000 = enabled, has conflict (red)
        //   0x10ffffff = disabled (white/transparent)
        uint32_t rowColor;
        if (enabled) {
            rowColor = hasConflict ? 0x40ff0000 : 0x20c0ffc0;
        } else {
            rowColor = 0x10ffffff;
        }
        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x30));
        {
            void* elem = *(void**)(entry + 0x30);
            void** vtable = *(void***)elem;
            typedef void (*fn_SetColor_t)(void*, uint32_t);
            ((fn_SetColor_t)vtable[10])(elem, rowColor);
        }

        // Show/hide conflict icon (entry+0x20 = imageConflict)
        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x20));
        fn_Element_SetFlagsOnOff(*(void**)(entry + 0x20),
                                  hasConflict ? 0 : 1,    // set bit 0 (hidden) when no conflict
                                  hasConflict ? 1 : 0);   // clear bit 0 (visible) when conflict

        // Copy thumbnail texture (48x48)
        if (fn_VideoDevice_Lock && fn_VideoDevice_Unlock && fn_ImageBuffer_CopyToTexture) {
            void* videoDevice = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
            if (videoDevice) {
                fn_VideoDevice_Lock(videoDevice);
                void* imageBuffer = *(void**)(entry + 0x40);
                void* thumbnailData = *(void**)(m + 0x30);  // mod->m_intArrayData (thumbnail pixels)
                fn_ImageBuffer_CopyToTexture(imageBuffer, thumbnailData, 0x30, 0x30);
                fn_VideoDevice_Unlock(videoDevice);
            }
        }
    }
}

// ============================================================
// ModDialog::Slot4 — Setup (0x14006F150)
// ============================================================
//
// Finds buttonCancel/buttonOk/tableMods, then for each installed mod:
//   1. Clones the dialog template element and adds it to tableMods
//   2. Finds 10 child elements by name, assigns IDs (1000+/2000+/3000+/4000+/5000+i)
//   3. Hides buttonUp+buttonTop for first mod, buttonDown for last
//   4. Builds 48x48 ImageBuffer + MaterialInstance + SpriteSheet thumbnail
//   5. Pushes 0x58-byte entry struct into self->m_entries array
// After loop: calls RefreshDisplay, loads browser controller, hides browse UI.

void ModDialog_Slot4(ModDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- buttonCancel (id=100, set +0xC0=8) ---
    {
        uint8_t retbuf[8];
        fn_String_Ctor_CStr(retbuf, "buttonCancel");
        uint8_t elemBuf[8];
        fn_DialogInstance_FindElement(self, elemBuf, retbuf);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        void* elem = *(void**)elemBuf;
        fn_Element_SetElementId(elem, 100);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        *(uint32_t*)(*(uintptr_t*)elemBuf + 0xC0) = 8;
        fn_String_Dtor(retbuf);
    }

    // --- buttonOk (id=0x65) ---
    uint8_t buttonOkBuf[8] = {};
    {
        uint8_t retbuf[8];
        fn_String_Ctor_CStr(retbuf, "buttonOk");
        uint8_t elemBuf[8];
        fn_DialogInstance_FindElement(self, elemBuf, retbuf);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        void* elem = *(void**)elemBuf;
        fn_Element_SetElementId(elem, 0x65);
        fn_String_Dtor(retbuf);
        *(uint64_t*)buttonOkBuf = *(uint64_t*)elemBuf;
    }
    (void)buttonOkBuf;

    // --- tableMods: find and store at self+0xC8 ---
    {
        uint8_t retbuf[8];
        fn_String_Ctor_CStr(retbuf, "tableMods");
        uint8_t elemBuf[8];
        fn_DialogInstance_FindElement(self, elemBuf, retbuf);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        void* elem = *(void**)elemBuf;
        // Store InternalPtr at +0xC8
        fn_InternalBasePtr_Ctor((void*)(s + 0xC8), elem);
        fn_String_Dtor(retbuf);
    }

    void* modSingleton = *(void**)g_ppModificationSingleton;
    int modCount = fn_Modification_GetCount(modSingleton);
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;

    for (int i = 0; i < modCount; i++) {
        // Clone template element from Dialog, add to tableMods
        uint8_t templateBuf[8];
        void* dialog = *(void**)(s + 0x18);
        fn_Dialog_GetTemplate(dialog, templateBuf, 0);
        void* templateObj = *(void**)templateBuf;

        // Call vtable[8] (offset 0x40) to clone: (templateObj, retbuf, dialog)
        typedef void* (*fn_Clone_t)(void* self, void* retbuf, void* dialog);
        uint8_t cloneBuf[8];
        ((fn_Clone_t)(*(void***)templateObj)[8])(templateObj, cloneBuf, dialog);
        void* clonedElem = *(void**)cloneBuf;

        // Add cloned element to tableMods
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
        void* tableElem = *(void**)(s + 0xC8);

        fn_Element_AddChild(tableElem, &clonedElem);

        // Find child elements by name in the cloned row
        uint8_t textModNameBuf[8] = {};
        uint8_t textModAuthorBuf[8] = {};
        uint8_t textModVersionBuf[8] = {};
        uint8_t textModDescBuf[8] = {};
        uint8_t checkLoadedBuf[8] = {};
        uint8_t imageConflictBuf[8] = {};
        uint8_t imageIconBuf[8] = {};
        uint8_t buttonTopBuf[8] = {};
        uint8_t buttonUpBuf[8] = {};
        uint8_t buttonDownBuf[8] = {};
        uint8_t buttonDetailsBuf[8] = {};

        struct {
            const char* name;
            uint8_t* buf;
        } findList[] = {
            { "textModName",        textModNameBuf },
            { "textModAuthor",      textModAuthorBuf },
            { "textModDescription", textModDescBuf },
            { "textModVersion",     textModVersionBuf },
            { "checkLoaded",        checkLoadedBuf },
            { "imageConflict",      imageConflictBuf },
            { "imageIcon",          imageIconBuf },
            { "buttonTop",          buttonTopBuf },
            { "buttonUp",           buttonUpBuf },
            { "buttonDown",         buttonDownBuf },
            { "buttonDetails",      buttonDetailsBuf },
        };

        for (auto& e : findList) {
            uint8_t strBuf[8];
            fn_String_Ctor_CStr(strBuf, e.name);
            fn_InternalBasePtr_CheckPtr(cloneBuf);
            uint8_t retBuf[8];
            fn_FindElement(clonedElem, retBuf, strBuf);
            fn_InternalBasePtr_CheckPtr(retBuf);
            void* found = *(void**)retBuf;
            fn_InternalBasePtr_Ctor(e.buf, found);
            fn_String_Dtor(strBuf);
        }

        // Get vtable[0x58] child (Child(0)) of cloned element
        uint8_t vtChildBuf[8] = {};
        {
            fn_InternalBasePtr_CheckPtr(cloneBuf);
            void** vt = *(void***)clonedElem;
            // vtable[0x58/8] = vtable[11] = Element::Child(int)
            typedef void* (*fn_Child_t)(void* self, void* retbuf, int index);
            uint8_t childRetBuf[8];
            ((fn_Child_t)vt[11])(clonedElem, childRetBuf, 0);
            fn_InternalBasePtr_CheckPtr(childRetBuf);
            void* child = *(void**)childRetBuf;
            fn_InternalBasePtr_Ctor(vtChildBuf, child);
        }

        // Assign element IDs
        fn_InternalBasePtr_CheckPtr(checkLoadedBuf);
        fn_Element_SetElementId(*(void**)checkLoadedBuf, i + 1000);
        fn_InternalBasePtr_CheckPtr(buttonTopBuf);
        fn_Element_SetElementId(*(void**)buttonTopBuf, i + 4000);
        fn_InternalBasePtr_CheckPtr(buttonDownBuf);
        fn_Element_SetElementId(*(void**)buttonDownBuf, i + 3000);
        fn_InternalBasePtr_CheckPtr(buttonUpBuf);
        fn_Element_SetElementId(*(void**)buttonUpBuf, i + 2000);
        fn_InternalBasePtr_CheckPtr(buttonDetailsBuf);
        fn_Element_SetElementId(*(void**)buttonDetailsBuf, i + 5000);

        // Hide Up+Top for first mod (vtable[8] = Element::Hide virtual, arg=0 matches original)
        if (i == 0) {
            fn_InternalBasePtr_CheckPtr(buttonUpBuf);
            void** upVt = *(void***)*(void**)buttonUpBuf;
            typedef void (*fn_VHide_t)(void*, int);
            ((fn_VHide_t)upVt[8])(*(void**)buttonUpBuf, 0);
            fn_InternalBasePtr_CheckPtr(buttonTopBuf);
            void** topVt = *(void***)*(void**)buttonTopBuf;
            ((fn_VHide_t)topVt[8])(*(void**)buttonTopBuf, 0);
        }
        // Hide Down for last mod
        if (i == modCount - 1) {
            fn_InternalBasePtr_CheckPtr(buttonDownBuf);
            void** downVt = *(void***)*(void**)buttonDownBuf;
            typedef void (*fn_VHide_t)(void*, int);
            ((fn_VHide_t)downVt[8])(*(void**)buttonDownBuf, 0);
        }

        // Build 0x58-byte entry struct (11 × 8 bytes of InternalPtrs + raw ptrs)
        struct ModEntry {
            uint64_t textModName;    // +0x00
            uint64_t textModAuthor;  // +0x08
            uint64_t textModVersion; // +0x10
            uint64_t textModDesc;    // +0x18
            uint64_t imageConflict;  // +0x20
            uint64_t checkLoaded;    // +0x28
            uint64_t vtChild;        // +0x30
            uint64_t imageIcon;      // +0x38
            void*    imageBuffer;    // +0x40
            void*    matInstance;    // +0x48
            void*    spriteSheet;    // +0x50
        } entry;

        entry.textModName   = *(uint64_t*)textModNameBuf;
        entry.textModAuthor = *(uint64_t*)textModAuthorBuf;
        entry.textModVersion = *(uint64_t*)textModVersionBuf;
        entry.textModDesc   = *(uint64_t*)textModDescBuf;
        entry.imageConflict = *(uint64_t*)imageConflictBuf;
        entry.checkLoaded   = *(uint64_t*)checkLoadedBuf;
        entry.vtChild       = *(uint64_t*)vtChildBuf;
        entry.imageIcon     = *(uint64_t*)imageIconBuf;

        // --- Create 48x48 ImageBuffer ---
        void* imgBuf = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
        entry.imageBuffer = nullptr;
        if (imgBuf) {
            fn_ImageBuffer_Ctor(imgBuf);
            *(uintptr_t*)imgBuf = g_NoiseUI_ImageBufferVtable; // same vtable as NoiseUI
            // Create: vtable[0x48/8]=vtable[9](self, 0, 2, 5, mip=0, w=48, h=48, 1,1,1,1, levels=null)
            void** ivt = *(void***)imgBuf;
            ((fn_ImageBuffer_Create_t)ivt[9])(imgBuf, 0, 2, 5, 0, 0x30, 0x30, 1, 1, 1, 1, nullptr);
            entry.imageBuffer = imgBuf;
        }

        // --- Create MaterialInstance (0x60 bytes) ---
        void* matInst = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
        entry.matInstance = nullptr;
        if (matInst) {
            fn_MaterialInstance_Ctor(matInst);
            ThumbVtable_StampMaterialInstance(matInst);
            entry.matInstance = matInst;
        }

        // Load "Material/UI/UIMaterial.rsc", IsDerivedFrom Material, SetMaterial
        {
            uint8_t filenameBuf[8];
            fn_Filename_CtorCStr(filenameBuf, "Material/UI/UIMaterial.rsc");
            void* extFactory = *(void**)g_pExternalFactorySingleton;
            void* ext = fn_ExternalFactory_FindOrLoad(extFactory, filenameBuf);
            void* material = nullptr;
            if (ext) {
                bool derived = fn_IsDerivedFrom(ext, g_TypeId_Material ? *g_TypeId_Material : 0);
                if (derived) material = ext;
            }
            fn_MaterialInstance_SetMaterial(matInst, material);
            fn_Filename_Dtor(filenameBuf);
        }

        // AddTexture("diffuse", imageBuffer)
        {
            uint8_t strBuf[8];
            fn_String_Ctor_CStr(strBuf, "diffuse");
            fn_MaterialInstance_AddTexture(matInst, strBuf, imgBuf);
            fn_String_Dtor(strBuf);
        }

        fn_MaterialInstance_BuildParameters(matInst);

        // --- Create SpriteSheet (0x58 bytes) ---
        void* sprSheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
        entry.spriteSheet = nullptr;
        if (sprSheet) {
            fn_SpriteSheet_Ctor(sprSheet);
            ThumbVtable_StampSpriteSheet(sprSheet);
            entry.spriteSheet = sprSheet;

            // Set materialInstance at +0x30
            *(void**)((uintptr_t)sprSheet + 0x30) = matInst;

            // operator= empty strings at +0x38 and +0x40
            uint64_t emptyStr1 = 0, emptyStr2 = 0;
            fn_String_Assign((void*)((uintptr_t)sprSheet + 0x40), &emptyStr1);
            fn_String_Assign((void*)((uintptr_t)sprSheet + 0x38), &emptyStr2);

            // Set w/h at +0x48/+0x4c to 48 (0x30)
            *(uint32_t*)((uintptr_t)sprSheet + 0x48) = 0x30;
            *(uint32_t*)((uintptr_t)sprSheet + 0x4c) = 0x30;
            // Zero +0x50
            *(uint64_t*)((uintptr_t)sprSheet + 0x50) = 0;

            // AddSpriteInfo(sheet, empty, empty, 0, 0, 48, 48)
            uint64_t emptyA = 0, emptyB = 0;
            fn_SpriteSheet_AddSpriteInfo(sprSheet, &emptyA, &emptyB, 0, 0, 0x30, 0x30);

            // SetSpriteInfo(sheet, 0, 48, 48, 0.0, 0.0, uvScale, uvScale)
            fn_SpriteSheet_SetSpriteInfo(sprSheet, 0, 0x30, 0x30, 0.0f, 0.0f, uvScale, uvScale);
        }

        // Create Sprite by index and SetSprite on imageIcon
        {
            uint8_t spriteBuf[16];
            fn_Sprite_Ctor_ByIndex(spriteBuf, sprSheet, 0);
            void* imageIconElem = *(void**)imageIconBuf;
            fn_ImageElement_SetSprite(imageIconElem, spriteBuf);
        }

        // Push entry into self->m_entries (array at +0xB8, count at +0xC0)
        int newCount = *(int*)(s + 0xC0) + 1;
        fn_ModDialog_ArrayPush((void*)(s + 0xB8), newCount, &entry);
    }

    // --- Post-loop: refresh display, add browser controller, hide browse UI ---
    ModDialog_RefreshDisplay_Impl(self);

    // Load "Dialog/Mod.rsc:browser", add as controller with type g_TypeId_BrowserController
    {
        uint8_t filenameBuf[8];
        fn_Filename_CtorCStr(filenameBuf, "Dialog/Mod.rsc:browser");
        void* extFactory = *(void**)g_pExternalFactorySingleton;
        void* ext = fn_ExternalFactory_FindOrLoad(extFactory, filenameBuf);

        // Pass browser external directly; original has IsDerivedFrom check but the
        // _typeInfo_exref is context-dependent (Ghidra merges refs) and the browser
        // controller IS the right type — passing null would silently break the UI.
        void* browserExt = ext;

        fn_Filename_Dtor(filenameBuf);

        // AddController(self, typeId, 0, 0, browserExt, "groupBrowse")
        uint8_t groupStr[8];
        fn_String_Ctor_CStr(groupStr, "groupBrowse");
        uint32_t typeId = g_TypeId_BrowserController ? *g_TypeId_BrowserController : 0;
        fn_DialogInstance_AddController6(self, typeId, 0, 0, browserExt, groupStr);
        fn_String_Dtor(groupStr);
    }

    // Hide "buttonBrowse"
    {
        uint8_t strBuf[8];
        fn_String_Ctor_CStr(strBuf, "buttonBrowse");
        uint8_t elemBuf[8];
        fn_DialogInstance_FindElement(self, elemBuf, strBuf);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        void* elem = *(void**)elemBuf;
        fn_Element_Hide(elem, true);
        fn_String_Dtor(strBuf);
    }

    // Hide "groupProgress"
    {
        uint8_t strBuf[8];
        fn_String_Ctor_CStr(strBuf, "groupProgress");
        uint8_t elemBuf[8];
        fn_DialogInstance_FindElement(self, elemBuf, strBuf);
        fn_InternalBasePtr_CheckPtr(elemBuf);
        void* elem = *(void**)elemBuf;
        fn_Element_Hide(elem, true);
        fn_String_Dtor(strBuf);
    }
}

// ============================================================
// ModDialog_ToggleMod_Impl (was fn_ModDialog_ToggleMod, FUN_14006e930)
//
// Pushes the row's checkbox state into the mod list, then refreshes.
//
// Retail (0x14006e930) does NOT read the model: it takes row `index` of the
// entry array (+0xB8, stride 0x58), CheckPtr's the checkLoaded InternalPtr at
// entry+0x28, and passes THAT element's already-toggled bool at +0xd8 to
// Modification::Enable. The previous port read Mod::enabled and inverted it,
// which only agrees while widget and model are in lockstep, and it is why the
// no-mods crash read 0xd0 (GetModification on an empty list) instead of
// faulting on the row itself. No null check: with the row present the deref is
// load-bearing, as in retail; the row's presence is the caller's contract
// (standalone/autostart.cpp gates its fabricated clicks on the row count).
// ============================================================
static void ModDialog_ToggleMod_Impl(ModDialog* self, int index) {
    uintptr_t s = (uintptr_t)self;
    void* modSingleton = *(void**)g_ppModificationSingleton;

    uintptr_t entry = *(uintptr_t*)(s + 0xB8) + (uintptr_t)(int64_t)index * 0x58;
    fn_InternalBasePtr_CheckPtr((void*)(entry + 0x28));
    uintptr_t checkElem = *(uintptr_t*)(entry + 0x28);
    fn_Modification_Enable(modSingleton, index, *(uint8_t*)(checkElem + 0xD8) != 0);

    ModDialog_RefreshDisplay_Impl(self);
}

// ============================================================
// ModDialog_MoveToTop_Impl (was fn_ModDialog_MoveToTop, FUN_14006e660)
//
// Moves a mod to position 0 by repeatedly swapping with the entry above.
// Then does a full refresh display (same as ModDialog_RefreshDisplay_Impl).
// ============================================================
static void ModDialog_MoveToTop_Impl(ModDialog* self, int index) {
    void* modSingleton = *(void**)g_ppModificationSingleton;

    // Swap up to top
    while (index > 0) {
        fn_Modification_Swap(modSingleton, index, index - 1);
        index--;
    }

    ModDialog_RefreshDisplay_Impl(self);
}

// ============================================================
// ModDialog_ShowDetails_Impl (was fn_ModDialog_ShowDetails, FUN_14006e490)
//
// Opens the ModDetailsDialog for a specific mod index.
// Loads "Dialog/ModDetails.rsc", creates a ModDetailsDialog (0xC8 bytes),
// sets vtable to PTR_FUN_14015c670, copies sprite handle from entry+0x50,
// stores mod index at +0xC0, then adds the dialog to the UI.
// ============================================================
static void ModDialog_ShowDetails_Impl(ModDialog* self, int index) {
    uintptr_t s = (uintptr_t)self;

    // Load dialog resource
    uint8_t filenameBuf[8];
    fn_Filename_CtorCStr(filenameBuf, "Dialog/ModDetails.rsc");
    void* extFactory = *(void**)g_pExternalFactorySingleton;
    void* ext = fn_ExternalFactory_FindOrLoad(extFactory, filenameBuf);
    // IsDerivedFrom check — resource is always valid Dialog if load succeeds
    void* dialog = ext;
    fn_Filename_Dtor(filenameBuf);

    // Allocate ModDetailsDialog (0xC8 = 200 bytes)
    void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* dialogInst = nullptr;
    if (newDialog) {
        // Get sprite handle from entry+0x50 (SpriteSheet ptr)
        uintptr_t entry = *(uintptr_t*)(s + 0xB8) + (uintptr_t)index * 0x58;
        uint64_t spriteHandle = *(uint64_t*)(entry + 0x50);

        // Construct DialogInstance base (0xB8)
        uint64_t nullRef;
        fn_InternalBasePtr_Ctor(&nullRef, nullptr);
        fn_DialogInstance_Ctor(newDialog, *(void**)(s + 0x10), dialog, &nullRef, 0, 0, 0);

        // Set vtable to ModDetailsDialog
        if (g_ModDetailsDialog_Vtable) {
            *(uintptr_t*)newDialog = g_ModDetailsDialog_Vtable;
        }

        // ModDetailsDialog fields: +0xB8 = sprite handle, +0xC0 = mod index
        *(uint64_t*)((uintptr_t)newDialog + 0xB8) = spriteHandle;
        *(int32_t*)((uintptr_t)newDialog + 0xC0) = index;

        dialogInst = newDialog;
    }

    // Add to UI
    uint64_t dialogRef;
    fn_InternalBasePtr_Ctor(&dialogRef, dialogInst);
    fn_UI_AddDialog(*(void**)(s + 0x10), &dialogRef);
}

// ============================================================
// ModDialog_Apply_Impl (was fn_ModDialog_Apply, FUN_14006ea70)
//
// Applies mod changes: checks if mods have been modified (dirty flag),
// shows appropriate reload dialog (menu vs in-game), saves mod list,
// handles reload/restart sequence, then closes the dialog.
//
// Flow:
//   1. Check dirty flag at Modification[0] (first byte of singleton)
//   2. If dirty:
//      a. If gameWorld == nullptr: menu context -> load "Dialog/Reload.rsc:menu"
//      b. Else: game context -> load "Dialog/Reload.rsc:game"
//      c. Create ReloadDialog (0xC0 bytes, vtable PTR_FUN_14015c480)
//      d. ShowDialog() — blocks until user responds
//      e. If result 1 or 2 (OK/confirm):
//         - SaveList(), CloseAndReload()
//         - Menu: MainMenu_Reload(self+0xD0)
//         - Game: create SavedState, call game reload
//      f. If other: RefreshList (cancel)
//   3. Close dialog
// ============================================================
static void ModDialog_Apply_Impl(ModDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* modSingleton = *(void**)g_ppModificationSingleton;

    // Check dirty flag — first byte of Modification singleton
    if (*(uint8_t*)modSingleton != 0) {
        void* gameWorld = *g_pGameWorldSingleton;

        // Load reload dialog resource based on context (menu vs in-game)
        uint8_t filenameBuf[8];
        if (gameWorld == nullptr) {
            fn_Filename_CtorCStr(filenameBuf, "Dialog/Reload.rsc:menu");
        } else {
            fn_Filename_CtorCStr(filenameBuf, "Dialog/Reload.rsc:game");
        }
        void* extFactory = *(void**)g_pExternalFactorySingleton;
        void* ext = fn_ExternalFactory_FindOrLoad(extFactory, filenameBuf);
        void* dialog = ext;  // IsDerivedFrom check always passes for valid .rsc
        fn_Filename_Dtor(filenameBuf);

        // Create ReloadDialog (0xC0 bytes)
        void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        void* dialogInst = nullptr;
        if (newDialog) {
            uint64_t nullRef;
            fn_InternalBasePtr_Ctor(&nullRef, nullptr);
            fn_DialogInstance_Ctor(newDialog, *(void**)(s + 0x10), dialog, &nullRef, 0, 0, 0);
            if (g_ReloadDialog_Vtable) {
                *(uintptr_t*)newDialog = g_ReloadDialog_Vtable;
            }
            dialogInst = newDialog;
        }

        // ShowDialog (modal) — blocks until user responds
        uint64_t dialogRef;
        fn_InternalBasePtr_Ctor(&dialogRef, dialogInst);
        // BANISHED_MODAL_ANSWER=<n>: unattended harness only. ShowDialog pumps its
        // own message loop until a button is pressed, which no driver can reach, so
        // the answer is injected here instead (1/2 = confirm, anything else = cancel).
        int result;
        const char* forced = std::getenv("BANISHED_MODAL_ANSWER");
        if (forced && *forced) {
            result = std::atoi(forced);
            printf("[ModDialog::Apply] BANISHED_MODAL_ANSWER=%d (reload dialog skipped)\n", result);
            fn_InternalBasePtr_DeletePtr(&dialogRef);
        } else {
            result = fn_UI_ShowDialog(*(void**)(s + 0x10), &dialogRef);
            fn_InternalBasePtr_DeletePtr(&dialogRef);
        }
        printf("[ModDialog::Apply] dirty=1 gameWorld=%p reload answer=%d\n", gameWorld, result);

        if ((unsigned)(result - 1) < 2) {
            // User confirmed: save and reload
            fn_Modification_SaveList(modSingleton);
            fn_ModDialog_CloseAndReload((void*)self);

            if (gameWorld == nullptr) {
                // Menu context: simple reload via +0xD0 controller
                fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
                fn_MainMenu_Reload(*(void**)(s + 0xD0));
            } else {
                // Game context: allocate SavedState (0x10 bytes), construct it
                void* savedState = fn_MemAlloc(g_pMemAllocSingleton, 0x10, 4);
                void* statePtr = nullptr;
                if (savedState && fn_SavedState_Ctor) {
                    statePtr = fn_SavedState_Ctor(savedState);
                }

                // Call vtable[7] on Engine singleton sub-object at +0x20
                // to prepare the game for reload (dispatch slot 0x140151440)
                void* engine = g_pEngineSingleton ? *g_pEngineSingleton : nullptr;
                if (engine) {
                    uintptr_t inner = *(uintptr_t*)((uintptr_t)engine + 0x20);
                    if (inner) {
                        typedef void (*fn_PrepareReload_t)(void* self, bool param, void* savedState);
                        fn_PrepareReload_t fn = (fn_PrepareReload_t)(*(uintptr_t*)(*(uintptr_t*)inner + 0x38));
                        fn((void*)inner, true, statePtr);
                    }
                }

                // Build null InternalBasePtr
                uint64_t nullRef2;
                fn_InternalBasePtr_Ctor(&nullRef2, nullptr);

                // Trigger game reload with saved state
                fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
                fn_Game_ReloadWithSave(*(void**)(s + 0xD0), statePtr, (void*)(uintptr_t)-1, &nullRef2);
            }
        } else {
            // User cancelled: refresh list to undo changes
            fn_Modification_RefreshList(modSingleton);
        }
    }

    // Close the mod dialog
    uint64_t closeRef;
    fn_InternalBasePtr_Ctor(&closeRef, (void*)self);
    fn_UI_CloseDialog(*(void**)(s + 0x10), &closeRef);
}

// ============================================================
// ModDialog::Slot8 — OnCommand handler (0x14006ED40)
// ============================================================
//
// Dispatches button clicks by element ID ranges:
//   100         — Cancel: refresh mod list, close dialog
//   0x65 (101)  — OK/Apply: save changes, reload if needed, close
//   1000-1999   — Toggle mod enabled/disabled (index = id - 1000)
//   2000-2999   — Move mod up (swap index, index-1)
//   3000-3999   — Move mod down (swap index, index+1)
//   4000-4999   — Move mod to top of list
//   5000-5999   — Show mod details dialog
//
// Always calls base DialogInstance::OnElementEvent at the end.

void ModDialog_Slot8(ModDialog* self, void* param2, int msg, int id,
                     uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8) {
    uintptr_t s = (uintptr_t)self;

    if (msg == 0x12) {  // Button click event
        void* modSingleton = *(void**)g_ppModificationSingleton;

        if (id == 100) {
            // Cancel — refresh list and close
            fn_Modification_RefreshList(modSingleton);

            // Build InternalBasePtr for CloseDialog
            void* dialogPtr = nullptr;
            fn_InternalBasePtr_Ctor(&dialogPtr, (void*)self);

            void* ui = *(void**)(s + 0x10);
            fn_UI_CloseDialog(ui, &dialogPtr);
            goto done;
        }

        if (id == 0x65) {
            // OK/Apply
            ModDialog_Apply_Impl(self);
            goto done;
        }

        if (id >= 1000 && id < 2000) {
            // Toggle mod checkbox
            ModDialog_ToggleMod_Impl(self, id - 1000);
            goto done;
        }

        if (id >= 2000 && id < 3000) {
            // Move up: swap(id-2000, id-2001)
            fn_Modification_Swap(modSingleton, id - 2000, id - 2001);
            ModDialog_RefreshDisplay_Impl(self);
            goto done;
        }

        if (id >= 3000 && id < 4000) {
            // Move down: swap(id-3000, id-2999)
            fn_Modification_Swap(modSingleton, id - 3000, id - 2999);
            ModDialog_RefreshDisplay_Impl(self);
            goto done;
        }

        if (id >= 4000 && id < 5000) {
            // Move to top
            ModDialog_MoveToTop_Impl(self, id - 4000);
            goto done;
        }

        if (id >= 5000 && id < 6000) {
            // Show details
            ModDialog_ShowDetails_Impl(self, id - 5000);
        }
    }

done:
    // Always call base handler
    // Need to pass element pointer from param2
    void* elementPtr = *(void**)param2;
    fn_DialogInstance_OnElementEvent((void*)self, &elementPtr,
                                     msg, id, p5, p6, p7, p8);
}
