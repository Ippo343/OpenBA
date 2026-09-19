// code/Game/Dialog/saveloaddialog.cpp
// Clean reimplementation of SaveLoadDialog methods.
//
// SaveLoadDialog (0x160 bytes): Save/load file browser dialog.
// Extends DialogInstance (0xB8 base). Contains UI::Sprite at +0xD0.
//
// Layout:
//   +0xB8  mode (byte): 0 = save mode, nonzero = load mode
//   +0xB9  saveFlag (byte): set to 1 when save completes
//   +0xBA  loadFlag (byte): set to 1 when load completes
//   +0xBB  refreshFlag (byte): set to 1 to trigger file list refresh
//   +0xBC  deleteFlag (byte): set to 1 when delete completes
//   +0xC0  selectedIndex (int32): currently selected save slot (-1 = none)
//   +0xC4  saveCount (int32): number of save files
//   +0xC8  rotation (float): loading spinner angle
//   +0xD0  sprite (16 bytes): UI::Sprite sub-object
//   +0xE0  loaderLoop1 (ptr): used for save/load transition
//   +0xE8  loaderLoop2 (ptr): used for save/load transition
//   +0xF0  mainPage (InternalBasePtr): PageElement for page switching
//   +0xF8  labelLoad (InternalBasePtr)
//   +0x100 labelSave (InternalBasePtr)
//   +0x108 tableSaveList (InternalBasePtr)
//   +0x110 entryArray (ptr): dynamic array of 0x40-byte save entries
//   +0x118 entryCount (int32)
//   +0x120 textLoading (InternalBasePtr)
//   +0x128 textSaving (InternalBasePtr)
//   +0x130 textPleaseWait (InternalBasePtr)
//   +0x138 loadingImage (InternalBasePtr)
//   +0x140 buttonSave (InternalBasePtr)
//   +0x148 buttonLoad (InternalBasePtr)
//   +0x150 buttonCancel (InternalBasePtr)
//   +0x158 buttonDelete (InternalBasePtr)

#include "Dialog/ui_dialogs.h"
#include "Component/coll_bridge.h"
#include "options.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "ui/Dialogs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Modification.h"   // ENDGAME B.3.4 caller batch P
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Thumbnail vtable stamps — see code/Game/Dialog/thumb_vtables.cpp.
extern uintptr_t ThumbVtable_StampMaterialInstance(void* obj);
extern uintptr_t ThumbVtable_StampSpriteSheet(void* obj);

// ============================================================
// Helper: Find element by name in dialog instance
// ============================================================
static uint64_t FindDialogElem(void* dialogInst, const char* name) {
    System::String nameStr(name);
    uint64_t result = 0;
    fn_DialogInstance_FindElement(dialogInst, &result, &nameStr);
    return result;
}

// ============================================================
// Helper: Find element by name in an Element tree, return InternalBasePtr
// ============================================================
static uint64_t FindChildElem(void* parentElem, const char* name) {
    System::String nameStr(name);
    uint64_t resultPtr = 0;
    fn_FindElement(parentElem, &resultPtr, &nameStr);
    void* rawElem = (void*)resultPtr;
    // Wrap in InternalBasePtr
    uint64_t basePtr = 0;
    new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    return basePtr;
}

// ============================================================
// Helper: Virtual call to Element::SetColor(this, color) — vtable[10]
// ============================================================
static void Element_SetColor(void* elem, uint32_t color) {
    void* rawElem = *(void**)&elem;
    typedef void (*fn_SetColor_t)(void* self, uint32_t color);
    void** vtable = *(void***)rawElem;
    ((fn_SetColor_t)vtable[10])(rawElem, color);
}

// ============================================================
// Save entry structure (0x40 bytes per entry in the file list)
// ============================================================
// +0x00 labelSaveName  (InternalBasePtr: TextElement)
// +0x08 labelSaveDate  (InternalBasePtr: TextElement)
// +0x10 imageSaveIcon  (InternalBasePtr: ImageElement)
// +0x18 borderSave     (InternalBasePtr: Element)
// +0x20 selectSave     (InternalBasePtr: Element — not used here, used in Update)
// +0x28 imageCorrupt   (InternalBasePtr: Element)
// +0x30 materialInst   (MaterialInstance* — owned, destroyed on cleanup)
// +0x38 spriteSheet    (SpriteSheet* — owned, destroyed on cleanup)

// ============================================================
// PopulateFileList (0x1400b6ce0): Build/refresh the save file list UI
// ============================================================
// Called after the save scan operation completes. Clears the old entry
// array, resizes the table element, then for each save file:
//   1. Clones the dialog template row element
//   2. Finds child elements (name, date, icon, border, etc.)
//   3. If save has a valid thumbnail: sets sprite directly from SaveInfo
//   4. If save has raw image data: creates MaterialInstance + SpriteSheet,
//      wraps the image as a texture, builds a Sprite and assigns it
//   5. Sets save name and date text parameters
//   6. Fills remaining empty rows with dimmed placeholder entries
//
// Original: 0x1400b6ce0 (442 lines in Ghidra).

void SaveLoadDialog_PopulateFileList(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Phase 1: Destroy old entries (cleanup MaterialInstance/SpriteSheet objects) ---
    int oldEntryCount = *(int*)(s + 0x118);
    uintptr_t oldEntryData = *(uintptr_t*)(s + 0x110);

    for (int i = 0; i < oldEntryCount; i++) {
        uintptr_t entryBase = oldEntryData + (uintptr_t)i * 0x40;

        // Destroy MaterialInstance at +0x30 (scalar deleting destructor, vtable[0])
        void* matInst = *(void**)(entryBase + 0x30);
        if (matInst) {
            typedef void (*fn_Dtor_t)(void* self, int flags);
            void** vtable = *(void***)matInst;
            ((fn_Dtor_t)vtable[0])(matInst, 1);  // 1 = free memory
        }

        // Destroy SpriteSheet at +0x38 if materialInstance was non-null
        if (*(void**)(entryBase + 0x30) != nullptr) {
            void* sheet = *(void**)(entryBase + 0x38);
            if (sheet) {
                typedef void (*fn_Dtor_t)(void* self, int flags);
                void** vtable = *(void***)sheet;
                ((fn_Dtor_t)vtable[0])(sheet, 1);
            }
        }
    }

    // Free the entry array and reset
    fn_MemFreeStatic(*(void**)(s + 0x110));
    *(void**)(s + 0x110) = nullptr;
    *(uint64_t*)(s + 0x118) = 0;  // count + capacity = 0

    // --- Phase 2: Calculate table dimensions ---
    int saveCount = *(int*)(s + 0xC4);
    int minSlots = (saveCount < 6) ? 6 : saveCount;
    int totalSlots = minSlots + 2;
    // Round up to nearest multiple of 3
    totalSlots = ((totalSlots / 3) + ((totalSlots % 3) != 0 ? 1 : 0)) * 3;
    // Actually match the original: (n/3 + rounding) * 3
    // Original: (iVar10 / 3 + (iVar10 >> 0x1f) + (int)(((longlong)iVar10 / 3 + ...) >> 0x1f)) * 3
    // For positive values this is just: ((n + 2) / 3) * 3 = round up to multiple of 3
    // But actually looking more carefully: the original does integer division rounding
    // which for positive ints is just n / 3 * 3 (round DOWN to multiple of 3)
    totalSlots = (totalSlots / 3) * 3;

    // Set table size: (totalSlots/3 + 1) rows, 3 columns
    void* tableElem = *(void**)(s + 0x108);
    fn_TableElement_SetSize(tableElem, totalSlots / 3 + 1, 3);

    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    void* saveLoad = *g_pSaveLoadSingleton;

    // --- Phase 3: Populate save file entries ---
    for (int i = 0; i < saveCount; i++) {
        void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, i);

        // Clone template element from dialog
        uint8_t templateBuf[16] = {};
        fn_Dialog_GetTemplate(*(void**)(s + 0x18), templateBuf, 0);
        void* templateObj = *(void**)templateBuf;

        // Clone via vtable[8]: templateObj->Clone(&cloneBuf, dialog)
        typedef void* (*fn_Clone_t)(void* self, void* retbuf, void* dialog);
        uint8_t cloneBuf[16] = {};
        ((fn_Clone_t)(*(void***)templateObj)[8])(templateObj, cloneBuf, *(void**)(s + 0x18));
        void* clonedElem = *(void**)cloneBuf;

        // Add cloned element to table at index i
        fn_TableElement_SetChild2(*(void**)(s + 0x108), &clonedElem, i);

        // Initialize entry struct (8 fields, all zero initially)
        uint64_t entry[8] = {};

        // Find child elements in the cloned row
        entry[0] = FindChildElem((void*)clonedElem, "labelSaveName");   // +0x00: TextElement
        entry[1] = FindChildElem((void*)clonedElem, "labelSaveDate");   // +0x08: TextElement
        entry[2] = FindChildElem((void*)clonedElem, "imageSaveIcon");   // +0x10: ImageElement
        entry[3] = FindChildElem((void*)clonedElem, "borderSave");      // +0x18: Element
        entry[4] = FindChildElem((void*)clonedElem, "imageCorrupt");    // +0x20: Element (matches original stack layout)
        entry[5] = FindChildElem((void*)clonedElem, "selectSave");      // +0x28: Element (toggled by Update for selection)

        // Set imageSaveIcon flags: disable mouse-over highlight (0x10000)
        fn_Element_SetFlagsOnOff((void*)entry[2], 0x10000, 0);

        // Set imageSaveIcon element ID for selection tracking (ID = i + 4)
        fn_Element_SetElementId((void*)entry[2], i + 4);

        // Check if SaveInfo has a valid Sprite (thumbnail)
        bool hasSpriteThumb = fn_Sprite_IsValid(saveInfo);

        if (hasSpriteThumb) {
            // --- Direct sprite from save info ---
            // Copy the 16-byte Sprite from SaveInfo
            uint8_t sprite[16];
            memcpy(sprite, saveInfo, 16);

            fn_ImageElement_SetSprite((void*)entry[2], sprite);

            // Hide imageCorrupt overlay
            fn_Element_SetFlagsOnOff((void*)entry[4], 1, 0);
        } else {
            // --- Build sprite from raw image data ---
            // Allocate MaterialInstance (0x60 bytes)
            void* matInst = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
            void* matInstFinal = nullptr;
            if (matInst) {
                fn_MaterialInstance_Ctor(matInst);
                // Patch vtable to the exe's MaterialInstance vtable copy
                ThumbVtable_StampMaterialInstance(matInst);
                matInstFinal = matInst;
            }
            entry[6] = (uint64_t)matInstFinal;

            // Load UIMaterialNoBlend material
            System::Filename matFilename("Material/UI/UIMaterialNoBlend.rsc");
            void* matExternal = const_cast<::Object::External*>(
                reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)  // B.3.3 bridge
                    ->FindOrLoad(matFilename));

            void* matResource = nullptr;
            if (matExternal) {
                // IsDerivedFrom check for Material type
                if (fn_IsDerivedFrom(matExternal, *g_TypeId_TextElement)) {
                    matResource = matExternal;
                }
            }
            // Note: the IsDerivedFrom check in the original uses the same _typeInfo_exref
            // for all checks, but here we need the Material type. Since the original code
            // works correctly, the ExternalFactory::FindOrLoad result is passed directly.
            // The IsDerivedFrom is checking MaterialExternal type.
            fn_MaterialInstance_SetMaterial(matInstFinal,
                matExternal ? matExternal : nullptr);

            // Add texture: "diffuse" -> saveInfo's ImageBuffer at +0x18
            System::String diffuseStr("diffuse");
            fn_MaterialInstance_AddTexture(matInstFinal, &diffuseStr,
                *(void**)((uintptr_t)saveInfo + 0x18));

            fn_MaterialInstance_BuildParameters(matInstFinal);

            // Allocate SpriteSheet (0x58 bytes)
            void* sheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
            void* sheetFinal = nullptr;
            if (sheet) {
                fn_SpriteSheet_Ctor(sheet);
                ThumbVtable_StampSpriteSheet(sheet);
                sheetFinal = sheet;
            }

            // Set material on spritesheet
            *(void**)((uintptr_t)sheetFinal + 0x30) = matInstFinal;

            // Get image dimensions
            void* imageBuffer = *(void**)((uintptr_t)saveInfo + 0x18);
            int imgHeight = fn_ImageBuffer_Height(imageBuffer);
            int imgWidth = fn_ImageBuffer_Width(imageBuffer);

            // Set spritesheet dimensions (raw SpriteSheet fields; not our struct)
            System::String emptyStr1(""), emptyStr2("");
            *reinterpret_cast<System::String*>((uintptr_t)sheetFinal + 0x40) = emptyStr1;  // B.3.1 bridge
            *reinterpret_cast<System::String*>((uintptr_t)sheetFinal + 0x38) = emptyStr2;  // B.3.1 bridge
            *(int*)((uintptr_t)sheetFinal + 0x48) = imgWidth;
            *(int*)((uintptr_t)sheetFinal + 0x4C) = imgHeight;
            *(uint64_t*)((uintptr_t)sheetFinal + 0x50) = 0;

            // Add sprite info (full image as single sprite)
            System::String nameStr1(""), nameStr2("");

            imageBuffer = *(void**)((uintptr_t)saveInfo + 0x18);
            imgHeight = fn_ImageBuffer_Height(imageBuffer);
            imgWidth = fn_ImageBuffer_Width(imageBuffer);
            fn_SpriteSheet_AddSpriteInfo(sheetFinal, &nameStr1, &nameStr2,
                                          0, 0, imgWidth, imgHeight);

            // Set sprite info with UV coordinates
            imageBuffer = *(void**)((uintptr_t)saveInfo + 0x18);
            int h = fn_ImageBuffer_Height(imageBuffer);
            int w = fn_ImageBuffer_Width(imageBuffer);
            fn_SpriteSheet_SetSpriteInfo(sheetFinal, 0, w, h,
                                          0.0f, 0.0f, uvScale, uvScale);

            // Create Sprite from SpriteSheet
            uint8_t spriteBuf[16] = {};
            fn_Sprite_Ctor_ByIndex(spriteBuf, sheetFinal, 0);

            fn_ImageElement_SetSprite((void*)entry[2], spriteBuf);

            entry[7] = (uint64_t)sheetFinal;

            // Check save validity: status at saveInfo+0x44
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                // Valid save: hide imageCorrupt overlay
                fn_Element_SetFlagsOnOff((void*)entry[4], 1, 0);
            } else {
                // Invalid/corrupt: dim the icon
                typedef void (*fn_SetColor_t)(void* self, uint32_t color);
                void** vtable = *(void***)(void*)entry[2];
                ((fn_SetColor_t)vtable[10])((void*)entry[2], 0xff808080);
            }
        }

        // --- Set text parameters if save has valid data ---
        if (*(int*)((uintptr_t)saveInfo + 0x40) >= 0) {
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                // Valid save: show formatted date string
                System::String dateStr;
                fn_DateTime_AsString((void*)((uintptr_t)saveInfo + 0x30), &dateStr);
                fn_TextElement_SetParameter((void*)entry[1], 0, &dateStr);
            } else {
                // Corrupt save: show "Corrupt" text from string table
                System::String corruptKey("Corrupt");
                uintptr_t keyMapObj = (uintptr_t)*g_pKeyMapSingleton;
                uint8_t uiStr[16] = {};
                fn_UIString_Ctor(uiStr, *(uint64_t*)(keyMapObj + 0xD8), &corruptKey);
                System::String sysStr;
                fn_UIString_ToSystemString(uiStr, &sysStr);
                fn_TextElement_SetParameter((void*)entry[1], 0, &sysStr);
            }

            // Set save name: saveInfo+0x10 is the name String
            fn_TextElement_SetParameter((void*)entry[0], 0,
                                         (void*)((uintptr_t)saveInfo + 0x10));
        }

        // Push entry into array at +0x110.
        // B.3.2 bridge: SaveLoadDialog+0x110 is Collection::Array<{8 x uint64
        // fields}> (stride 0x40, count +0x118, capacity +0x11C) inside a struct
        // §B.2 has not converted, so it is reached at its offset. Was
        // FUN_1400211e0's own inlined SetSize (SaveEntryArray_Resize); the
        // MaterialInstance/SpriteSheet fields it carries are raw pointer bytes
        // here, destroyed separately in Phase 1 above, not by this growth.
        auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x40, 8>>(s, 0x110);
        rows.SetSize(rows.GetSize() + 1,
                     GameColl::Bytes<0x40, 8>(entry[0], entry[1], entry[2], entry[3],
                                    entry[4], entry[5], entry[6], entry[7]));
    }

    // --- Phase 4: Fill remaining empty rows ---
    for (int i = saveCount; i < totalSlots; i++) {
        // Clone template element
        uint8_t templateBuf[16] = {};
        fn_Dialog_GetTemplate(*(void**)(s + 0x18), templateBuf, 0);
        void* templateObj = *(void**)templateBuf;

        typedef void* (*fn_Clone_t)(void* self, void* retbuf, void* dialog);
        uint8_t cloneBuf[16] = {};
        ((fn_Clone_t)(*(void***)templateObj)[8])(templateObj, cloneBuf, *(void**)(s + 0x18));
        void* clonedElem = *(void**)cloneBuf;

        // Add to table
        fn_TableElement_SetChild2(*(void**)(s + 0x108), &clonedElem, i);

        // Initialize empty entry struct
        uint64_t entry[8] = {};

        // Find child elements (same 7 lookups)
        entry[0] = FindChildElem((void*)clonedElem, "labelSaveName");
        entry[1] = FindChildElem((void*)clonedElem, "labelSaveDate");
        entry[2] = FindChildElem((void*)clonedElem, "imageSaveIcon");
        entry[3] = FindChildElem((void*)clonedElem, "borderSave");
        entry[4] = FindChildElem((void*)clonedElem, "imageCorrupt");
        entry[5] = FindChildElem((void*)clonedElem, "selectSave");

        // Hide imageCorrupt and selectSave overlays for empty slots
        fn_Element_SetFlagsOnOff((void*)entry[4], 1, 0);
        fn_Element_SetFlagsOnOff((void*)entry[5], 1, 0);

        // Dim border (SetColor via vtable[10] = 0x80ffffff)
        {
            typedef void (*fn_SetColor_t)(void* self, uint32_t color);
            void** vtable = *(void***)(void*)entry[3];
            ((fn_SetColor_t)vtable[10])((void*)entry[3], 0x80ffffff);
        }

        // Dim icon (SetColor via vtable[10] = 0x20ffffff)
        {
            typedef void (*fn_SetColor_t)(void* self, uint32_t color);
            void** vtable = *(void***)(void*)entry[2];
            ((fn_SetColor_t)vtable[10])((void*)entry[2], 0x20ffffff);
        }
    }
}

// ============================================================
// Startup (vtable +0x20): Build save/load UI, populate file list
// ============================================================
// Original: 0x1400b7d00
//
// Finds 12 named UI elements and stores them in SaveLoadDialog fields.
// Sets button element IDs: buttonSave=1, buttonLoad=2, buttonCancel=0, buttonDelete=3.
// Sets cancel button close-on-click flag (+0xC0 = 8).
// Sets refresh flag (+0xBB = 1) to trigger file list population.
// Copies sprite data from DAT_1401c3de8+0xB8 (16 bytes) to SaveLoad singleton.
// Calls SetMinOperationTime(0.75f) and SetOperation(1, mode).

void SaveLoadDialog_Startup(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find all named elements ---
    *(uint64_t*)(s + 0xF8)  = FindDialogElem(self, "labelLoad");
    *(uint64_t*)(s + 0x100) = FindDialogElem(self, "labelSave");
    *(uint64_t*)(s + 0x140) = FindDialogElem(self, "buttonSave");
    *(uint64_t*)(s + 0x148) = FindDialogElem(self, "buttonLoad");
    *(uint64_t*)(s + 0x150) = FindDialogElem(self, "buttonCancel");
    *(uint64_t*)(s + 0x158) = FindDialogElem(self, "buttonDelete");
    *(uint64_t*)(s + 0xF0)  = FindDialogElem(self, "mainPage");
    *(uint64_t*)(s + 0x120) = FindDialogElem(self, "textLoading");
    *(uint64_t*)(s + 0x128) = FindDialogElem(self, "textSaving");
    *(uint64_t*)(s + 0x130) = FindDialogElem(self, "textPleaseWait");
    *(uint64_t*)(s + 0x138) = FindDialogElem(self, "loadingImage");
    *(uint64_t*)(s + 0x108) = FindDialogElem(self, "tableSaveList");

    // --- Set button element IDs ---
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x140), 1);   // buttonSave
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x148), 2);   // buttonLoad
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x150), 0);   // buttonCancel
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x158), 3);   // buttonDelete

    // --- Set cancel button close-on-click flag ---
    *(uint32_t*)(*(uintptr_t*)(s + 0x150) + 0xC0) = 8;

    // --- Set refresh flag ---
    *(uint8_t*)(s + 0xBB) = 1;

    // --- Copy sprite from DAT_1401c3de8 + 0xB8 and set on SaveLoad singleton ---
    uintptr_t keyMapObj = (uintptr_t)*g_pKeyMapSingleton;
    uint8_t spriteCopy[16];
    *(uint32_t*)(spriteCopy + 0)  = *(uint32_t*)(keyMapObj + 0xB8);
    *(uint32_t*)(spriteCopy + 4)  = *(uint32_t*)(keyMapObj + 0xBC);
    *(uint32_t*)(spriteCopy + 8)  = *(uint32_t*)(keyMapObj + 0xC0);
    *(uint32_t*)(spriteCopy + 12) = *(uint32_t*)(keyMapObj + 0xC4);
    fn_SaveLoad_SetSaveSprite(*g_pSaveLoadSingleton, spriteCopy);

    // --- Set minimum operation time (0.75f seconds) ---
    uint8_t timeVal[8];
    fn_Time_Ctor_Float(timeVal, 0.75f);
    fn_SaveLoad_SetMinOperationTime(*g_pSaveLoadSingleton, timeVal);

    // --- Start operation: 1 (list/scan), param = -2 or -3 based on mode ---
    uint8_t mode = *(uint8_t*)(s + 0xB8);
    int opParam = -2 - (int)(mode != 0);
    fn_SaveLoad_SetOperation(*g_pSaveLoadSingleton, 1, opParam);
}

// ============================================================
// Update (vtable +0x30): Refresh file list, update preview
// ============================================================
// Original: 0x1400b6880
//
// Two main states:
// 1. Operation active (op != 0) OR any flag set: Show loading page (page 0),
//    rotate loading spinner, show/hide text based on operation type.
// 2. Idle (op == 0, no flags): Show main page (page 1), enable/disable buttons,
//    iterate save entries to show/hide selection highlights.

void SaveLoadDialog_Update(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    uint8_t mode = *(uint8_t*)(s + 0xB8);
    void* saveLoad = *g_pSaveLoadSingleton;
    if (!saveLoad) return;

    // --- Show/hide load/save labels based on mode ---
    // labelLoad (+0xF8): visible when mode == 0 (save mode shows "load" label? no...)
    // Actually: labelLoad visible when NOT save mode, labelSave visible when save mode
    uint32_t isLoadMode = (uint32_t)(mode == 0);
    uint32_t isSaveMode = (uint32_t)(mode != 0);
    void* labelLoad = (void*)*(uint64_t*)(s + 0xF8);
    if (labelLoad) fn_Element_SetFlagsOnOff(labelLoad, isLoadMode, 0);
    void* labelSave = (void*)*(uint64_t*)(s + 0x100);
    if (labelSave) fn_Element_SetFlagsOnOff(labelSave, isSaveMode, 0);

    // buttonSave (+0x140): visible in save mode
    void* buttonSave = (void*)*(uint64_t*)(s + 0x140);
    if (buttonSave) fn_Element_SetFlagsOnOff(buttonSave, isSaveMode, 0);
    // buttonLoad (+0x148): visible in load mode
    void* buttonLoad = (void*)*(uint64_t*)(s + 0x148);
    if (buttonLoad) fn_Element_SetFlagsOnOff(buttonLoad, isLoadMode, 0);

    int operation = fn_SaveLoad_GetOperation(saveLoad);

    // Check if any operation is in progress or any deferred flag is set
    bool anyFlagsSet = (*(uint8_t*)(s + 0xBB) != 0) ||
                       (*(uint8_t*)(s + 0xB9) != 0) ||
                       (*(uint8_t*)(s + 0xBA) != 0) ||
                       (*(uint8_t*)(s + 0xBC) != 0);

    if (operation == 0 && !anyFlagsSet) {
        // --- IDLE STATE: show main page, update buttons and entry list ---
        void* pageElem = (void*)*(uint64_t*)(s + 0xF0);
        if (pageElem) fn_PageElement_SetActivePage(pageElem, 1);

        // Enable/disable buttonLoad based on selected save validity
        int selectedIdx = *(int*)(s + 0xC0);
        int saveCount = *(int*)(s + 0xC4);
        bool loadEnabled = false;
        if (saveCount >= 1 && selectedIdx >= 0) {
            void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
            // saveInfo+0x44 == 2 means valid save
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                loadEnabled = true;
            }
        }

        // buttonLoad: SetEnabled via vtable[8]
        {
            void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
            if (loadBtn) {
                void** vtable = *(void***)loadBtn;
                typedef void (*fn_SetEnabled_t)(void*, uint64_t);
                ((fn_SetEnabled_t)vtable[8])(loadBtn, loadEnabled ? 1 : 0);
            }
        }

        // buttonSave: always enabled
        {
            void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
            if (saveBtn) {
                void** vtable = *(void***)saveBtn;
                typedef void (*fn_SetEnabled_t)(void*, uint64_t);
                ((fn_SetEnabled_t)vtable[8])(saveBtn, 1);
            }
        }

        // buttonCancel: always enabled
        {
            void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
            if (cancelBtn) {
                void** vtable = *(void***)cancelBtn;
                typedef void (*fn_SetEnabled_t)(void*, uint64_t);
                ((fn_SetEnabled_t)vtable[8])(cancelBtn, 1);
            }
        }

        // Check selected save for delete eligibility
        if (selectedIdx >= 0) {
            fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        }

        // buttonDelete: SetEnabled via vtable[8]
        {
            void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
            if (deleteBtn) {
                void** vtable = *(void***)deleteBtn;
                typedef void (*fn_SetEnabled_t)(void*, uint64_t);
                ((fn_SetEnabled_t)vtable[8])(deleteBtn, (uint64_t)(selectedIdx >= 0));
            }
        }

        // --- Iterate save entries: show/hide selection highlighting ---
        if (saveCount < 1) return;

        uintptr_t entryArray = *(uintptr_t*)(s + 0x110);
        for (int i = 0; i < saveCount; i++) {
            uintptr_t entryBase = entryArray + (uintptr_t)i * 0x40;
            bool notSelected = (i != selectedIdx);

            // entry+0x28: selectSave — shown when selected, hidden otherwise
            void* selectElem = (void*)*(uint64_t*)(entryBase + 0x28);
            if (selectElem) fn_Element_SetFlagsOnOff(selectElem, notSelected ? 1 : 0, notSelected ? 0 : 1);

            // entry+0x18: borderSave — shown when NOT selected, hidden when selected
            void* borderElem = (void*)*(uint64_t*)(entryBase + 0x18);
            if (borderElem) fn_Element_SetFlagsOnOff(borderElem, notSelected ? 0 : 1, notSelected ? 1 : 0);
        }
        return;
    }

    // --- OPERATION IN PROGRESS: show loading page ---
    // Rotate loading spinner image
    void* spinnerImg = (void*)*(uint64_t*)(s + 0x138);
    if (spinnerImg) fn_ImageElement_SetRotation(spinnerImg, *(float*)(s + 0xC8));

    // Switch to loading page (page 0)
    void* pageElemOp = (void*)*(uint64_t*)(s + 0xF0);
    if (pageElemOp) fn_PageElement_SetActivePage(pageElemOp, 0);

    // Disable all four buttons
    {
        void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
        if (loadBtn) {
            void** vtable = *(void***)loadBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(loadBtn, 0);
        }
    }
    {
        void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
        if (saveBtn) {
            void** vtable = *(void***)saveBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(saveBtn, 0);
        }
    }
    {
        void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
        if (cancelBtn) {
            void** vtable = *(void***)cancelBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(cancelBtn, 0);
        }
    }
    {
        void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
        if (deleteBtn) {
            void** vtable = *(void***)deleteBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(deleteBtn, 0);
        }
    }

    // --- Show/hide status text based on operation type ---
    // Operation types: 1 = listing, 2 = saving, 4 = deleting
    void* textPleaseWait = (void*)*(uint64_t*)(s + 0x130);
    void* textLoading = (void*)*(uint64_t*)(s + 0x120);
    void* textSaving = (void*)*(uint64_t*)(s + 0x128);
    if (operation == 1) {
        // Listing: show textPleaseWait, hide textLoading, hide textSaving
        if (textPleaseWait) fn_Element_SetFlagsOnOff(textPleaseWait, 0, 1);
        if (textLoading) fn_Element_SetFlagsOnOff(textLoading, 1, 0);
        if (textSaving) fn_Element_SetFlagsOnOff(textSaving, 1, 0);
    } else if (operation == 2) {
        // Saving: hide textPleaseWait, show textLoading, hide textSaving
        if (textPleaseWait) fn_Element_SetFlagsOnOff(textPleaseWait, 1, 0);
        if (textLoading) fn_Element_SetFlagsOnOff(textLoading, 0, 1);
        if (textSaving) fn_Element_SetFlagsOnOff(textSaving, 1, 0);
    } else if (operation == 4) {
        // Deleting: hide textPleaseWait, hide textLoading, show textSaving
        if (textPleaseWait) fn_Element_SetFlagsOnOff(textPleaseWait, 1, 0);
        if (textLoading) fn_Element_SetFlagsOnOff(textLoading, 1, 0);
        if (textSaving) fn_Element_SetFlagsOnOff(textSaving, 0, 1);
    }
}

// ============================================================
// Load (vtable +0x38): Process timed operations, trigger state transitions
// ============================================================
// Original: 0x1400b79b0
//
// Timer/state machine called every frame.
//
// Phase 1 — While Save::SaveLoad operation is non-zero (file I/O in progress):
//   Accumulate rotation angle at +0xC8 (180 deg/sec from time param).
//
// Phase 2 — When operation reaches 0 (I/O complete), process one deferred flag:
//
//   deleteFlag (+0xBC): delete completed.
//     Write *(uint16_t*)(+0xBB) = 1 (sets +0xBB=1, +0xBC=0), start re-list operation.
//
//   refreshFlag (+0xBB): file list needs refresh.
//     Update saveCount (+0xC4), clamp selectedIndex (+0xC0), populate file list, clear flag.
//
//   saveFlag (+0xB9): save completed.
//     Close this dialog via UI::UserInterface::CloseDialog, clear flag.
//
//   loadFlag (+0xBA): load completed — transition to game.
//     Close this dialog.
//     Allocate GameUpdateLoop (0x160 bytes), construct via GameUpdateLoop ctor.
//     Allocate GameLoadDialogLoopT (0xb60 bytes), construct via DialogLoaderLoopT base ctor,
//       then patch vtable to exe subclass at PTR_FUN_140158800, set extra fields.
//     Allocate outer LoaderLoop (0x48 bytes), construct via LoaderLoop ctor with 0.0s delay,
//       patch vtable to PTR_FUN_1401588d0.
//     Set the outer LoaderLoop as the next loop on the engine (via field +0xE0/+0xE8).
//     Clear loadFlag.
//
// All constructors are available: GameUpdateLoop via exe address, LoaderLoop and
// DialogLoaderLoopT via Runtime-x64.dll exports. Vtable patches replicate the
// exe subclass polymorphism (the base Runtime ctors set the base vtable; exe code
// overwrites with the subclass vtable immediately after).

void SaveLoadDialog_Load(SaveLoadDialog* self, void* time, void** loaderLoopOut, uint64_t param4) {
    (void)loaderLoopOut;
    (void)param4;
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Phase 1: accumulate spinner rotation while I/O is in progress
    int operation = fn_SaveLoad_GetOperation(saveLoad);
    if (operation != 0) {
        float elapsed = fn_Time_AsSeconds(time);
        *(float*)(s + 0xC8) += elapsed * 180.0f;
    }

    // Re-check — may have just completed this frame
    operation = fn_SaveLoad_GetOperation(saveLoad);
    if (operation != 0) return;

    // Phase 2: I/O complete, process one deferred flag (priority order matches original)

    // deleteFlag (+0xBC): re-trigger list scan
    if (*(uint8_t*)(s + 0xBC) != 0) {
        *(uint16_t*)(s + 0xBB) = 1;   // sets +0xBB=1 (refresh), +0xBC=0 (clear delete)
        uint8_t mode = *(uint8_t*)(s + 0xB8);
        fn_SaveLoad_SetOperation(saveLoad, 1, -2 - (int)(mode != 0));
        return;
    }

    // refreshFlag (+0xBB): populate file list
    if (*(uint8_t*)(s + 0xBB) != 0) {
        int count = fn_SaveLoad_GetSaveCount(saveLoad);
        *(int*)(s + 0xC4) = count;
        if (count <= *(int*)(s + 0xC0)) {
            *(int*)(s + 0xC0) = count - 1;
        }
        SaveLoadDialog_PopulateFileList(self);
        *(uint8_t*)(s + 0xBB) = 0;
        return;
    }

    // saveFlag (+0xB9): save done, close dialog (no game transition needed)
    if (*(uint8_t*)(s + 0xB9) != 0) {
        uint64_t basePtr = 0;
        new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)s));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, &basePtr);
        *(uint8_t*)(s + 0xB9) = 0;
        return;
    }

    // loadFlag (+0xBA): load done — close dialog and transition to game load screen
    if (*(uint8_t*)(s + 0xBA) != 0) {
        // Close the SaveLoad dialog
        uint64_t basePtr = 0;
        new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)s));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, &basePtr);

        // --- Step 1: Allocate and construct the inner GameUpdateLoop (0x160 bytes) ---
        // GameUpdateLoop_Ctor reimplemented inline (was FUN_140098060).
        // Calls UpdateLoop base ctor, patches vtable, initializes all fields.
        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        void* pGameLoop = nullptr;
        if (gameLoop) {
            void* savedState = fn_SaveLoad_GetSaveState(saveLoad);

            fn_UpdateLoop_Ctor(gameLoop);
            *(void**)gameLoop = (void*)g_GameUpdateLoop_Vtable;

            // +0x28: empty name string. Ownership is transferred into gameLoop by raw
            // copy (matching retail): null the local out afterward so its destructor
            // (which never fires in the original) is a no-op rather than a double-free.
            System::String nameStr("");
            *(uint64_t*)((uintptr_t)gameLoop + 0x28) = *reinterpret_cast<uint64_t*>(&nameStr);
            nameStr._data = nullptr;
            *(uint32_t*)((uintptr_t)gameLoop + 0x30) = 0;   // p6
            *(uint32_t*)((uintptr_t)gameLoop + 0x34) = 0;   // p7
            *(uint64_t*)((uintptr_t)gameLoop + 0x38) = 0;
            *(uint64_t*)((uintptr_t)gameLoop + 0x40) = 0;   // p9
            *(uint64_t*)((uintptr_t)gameLoop + 0x48) = 0;   // p10
            *(uint64_t*)((uintptr_t)gameLoop + 0x50) = 0;   // p11
            *(uint64_t*)((uintptr_t)gameLoop + 0x58) = (uint64_t)savedState;
            // Zero remaining fields +0x60..+0xC0
            for (uintptr_t off = 0x60; off <= 0xC0; off += 8)
                *(uint64_t*)((uintptr_t)gameLoop + off) = 0;
            *(uint16_t*)((uintptr_t)gameLoop + 0xC0) = 0;
            *(uint64_t*)((uintptr_t)gameLoop + 0xC8) = 0;
            // p8=1 means disasterOff=false -> 0 (not 0xF0)
            *(uint32_t*)((uintptr_t)gameLoop + 0xD0) = 0;
            for (uintptr_t off = 0xD8; off <= 0x148; off += 8)
                *(uint64_t*)((uintptr_t)gameLoop + off) = 0;
            // +0x150: loaderLoopPtr (null)
            *(uint64_t*)((uintptr_t)gameLoop + 0x150) = 0;
            *(uint8_t*)((uintptr_t)gameLoop + 0x158) = 0;
            *(uint32_t*)((uintptr_t)gameLoop + 0x15C) = 0xFFFFFFFF;  // opFlag

            pGameLoop = gameLoop;
        }

        // --- Step 2: Allocate and construct the GameLoadDialogLoopT (0xb60 bytes) ---
        // Base: Engine::DialogLoaderLoopT (Runtime export), then patch exe subclass vtable.
        void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
        void* pDlLoop = nullptr;
        if (dlLoop) {
            // Build Time from constant (1.0f delay)
            uint64_t delayTime[1];
            fn_Time_Ctor_Float((void*)delayTime, 1.0f);

            // GetLoadingSoundString reimplemented inline: generates random loading sound variant
            System::String dialogNameStr;
            GetLoadingSoundString(&dialogNameStr);

            // Construct base Engine::DialogLoaderLoopT
            fn_DialogLoaderLoopT_Ctor(dlLoop, &dialogNameStr, pGameLoop, (void*)delayTime);

            // Patch vtable to exe subclass (GameLoadDialogLoopT)
            *(void**)dlLoop = (void*)g_GameLoadDialogLoopT_Vtable;

            // Initialize extra fields added by exe subclass
            uintptr_t dl = (uintptr_t)dlLoop;
            *(uint32_t*)(dl + 0x188) = 0xff000000;
            *(uint32_t*)(dl + 0x18c) = 0xff000000;
            *(uint8_t*)(dl  + 0x190) = 0;  // [400]
            fn_Random_Default_Ctor((void*)(dl + 0x194));
            *(uint64_t*)(dl + 0xb58) = 0;
            *(uint8_t*)(dl  + 0x40)  = 0;  // m_autoAdvance = 0 (see LoaderLoop.h)

            pDlLoop = dlLoop;
        }

        // --- Step 3: Allocate and construct the outer LoaderLoop (0x48 bytes) ---
        void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
        void* pOuterLoop = nullptr;
        if (outerLoop) {
            uint64_t zeroTime[1];
            fn_Time_Ctor_Float((void*)zeroTime, 0.0f);
            fn_LoaderLoop_Ctor(outerLoop, pDlLoop, (void*)zeroTime);
            *(void**)outerLoop = (void*)g_GameLoaderLoop_Vtable;
            pOuterLoop = outerLoop;
        }

        // --- Step 4: Set the outer LoaderLoop as the engine's next loop ---
        // SetDialogLoaderNextLoop and SetEngineNextLoop now reimplemented inline.
        uintptr_t loaderLoop1 = *(uintptr_t*)(s + 0xE0);
        if (loaderLoop1 == 0) {
            uintptr_t engPtr = *(uintptr_t*)(s + 0xE8);
            SetEngineNextLoop(engPtr, pOuterLoop);
        } else {
            SetDialogLoaderNextLoop(loaderLoop1, pOuterLoop);
        }

        *(uint8_t*)(s + 0xBA) = 0;
    }
}

// ============================================================
// Unattended-harness escape for the modal dialogs on the save/load path
// ============================================================
// UI::UserInterface::ShowDialog (Runtime 0x18011fd10) is a NESTED MODAL LOOP: it
// re-enters Engine::MainLoop::Frame until the dialog is removed. No autostart driver
// can reach it -- AutoStart::TickMenu runs INSIDE that nested loop with its phase
// already Done -- so an unanswered modal here is an unbounded stall that reads exactly
// like "the menu never advances and no world is created". Same shape and same remedy as
// ModDialog::Apply BANISHED_MODAL_ANSWER (code/Game/Dialog/moddialog.cpp).
// 2 is the "confirm" answer every caller on this path tests for.
// True when a harness env var says nobody is at the keyboard. Same list as
// HarnessGuardUnattendedBoot (standalone/main.cpp) -- kept in step deliberately: both
// answer the question "may we take a decision retail would put to the player?".
// The same six drivers standalone/main.cpp's unattended-boot guard keys on;
// options.cpp folds them into one field.
static bool SaveLoad_UnattendedRun() { return Opt().unattendedBoot; }

static int SaveLoad_ModalShowDialog(void* ui, void* dlgPtr, const char* what) {
    const bool forced = Opt().modalAnswer;
    if (forced || SaveLoad_UnattendedRun()) {
        int answer = forced ? Opt().modalAnswerValue : 2;
        printf("[SaveLoadDialog] unattended run: answering modal \"%s\" with %d "
               "(ShowDialog skipped; override with BANISHED_MODAL_ANSWER)\n",
               what ? what : "?", answer);
        fflush(stdout);
        return answer;
    }
    printf("[SaveLoadDialog] entering MODAL ShowDialog \"%s\" -- re-enters MainLoop::Frame "
           "and blocks until a human clicks; set BANISHED_MODAL_ANSWER=2 to run unattended\n",
           what ? what : "?");
    fflush(stdout);
    int r = fn_UI_ShowDialog(ui, dlgPtr);
    printf("[SaveLoadDialog] modal \"%s\" answered %d\n", what ? what : "?", r);
    fflush(stdout);
    return r;
}

// ============================================================
// Helper: create a confirm-style MessageDialog from a .rsc path string
// ============================================================
// Loads the External resource by filename, verifies it is a DialogInstance,
// allocates a 0xC0-byte DialogInstance shell, constructs it, patches the vtable
// to MessageDialog (0x14015c480), and calls UI::ShowDialog.
// Returns the ShowDialog result (2 = confirmed/Yes, other = cancelled).
//
// This pattern appears identically in DoDelete, DoSave, and DoLoad.
static int SaveLoad_ShowConfirmDialog(void* self, const char* rscPath) {
    uintptr_t s = (uintptr_t)self;

    // Load the .rsc External resource
    System::Filename filename(rscPath);
    void* externalRes = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
            ->FindOrLoad(filename));

    // IsDerivedFrom check — ensure it's a valid External (matches original _typeInfo_exref)
    void* checkedRes = nullptr;
    if (externalRes) {
        bool isExternal = fn_IsDerivedFrom(externalRes, *g_pExternalTypeId);
        if (isExternal) checkedRes = externalRes;
    }

    // Allocate 0xC0 bytes for a DialogInstance shell
    void* dlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
    if (dlg) {
        // Construct base DialogInstance(ui, externalRes, nullParent, 0, 0, 0)
        uint64_t nullParent = 0;
        fn_DialogInstance_Ctor(dlg, *(void**)(s + 0x10), checkedRes, &nullParent, 0, 0, 0);
        // Patch vtable to MessageDialog
        *(void**)dlg = (void*)ResolveAddr(Module::Exe, MessageDialogAddr::Vtable);
    }

    // ShowDialog(ui, InternalPtr<DialogInstance>)
    uint64_t dlgPtr = 0;
    new (&dlgPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dlg));   // B.3.3 bridge
    int result = SaveLoad_ModalShowDialog(*(void**)(s + 0x10), &dlgPtr, rscPath);
    reinterpret_cast< ::Object::InternalBasePtr*>(&dlgPtr)->DeletePtr();   // B.3.3 bridge
    return result;
}

// ============================================================
// DoDelete (0x1400b5e50): Show confirmDelete dialog, then trigger delete operation.
// ============================================================
// Shows "Dialog/SaveLoad.rsc:confirmDelete" dialog.
// If user confirms (result == 2): sets deleteFlag (+0xBC = 1), starts operation 5.
// Original: 0x1400b5e50.

static void SaveLoadDialog_DoDelete(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmDelete");
    if (result == 2) {
        *(uint8_t*)(s + 0xBC) = 1;
        void* saveLoad = *g_pSaveLoadSingleton;
        fn_SaveLoad_SetOperation(saveLoad, 5, *(int*)(s + 0xC0));
    }
}

// ============================================================
// DoSave (0x1400b5fd0): Optional confirm-overwrite, then trigger save operation.
// ============================================================
// Sets min operation time (1.5f seconds).
// Gets save info for selected index: if save slot already has data (validState >= 0),
//   shows "Dialog/SaveLoad.rsc:confirmSave" dialog; if not confirmed, returns.
// Sets saveFlag (+0xB9 = 1), copies save name from text element at +0xE8+0x28,
//   calls SetSaveName, then SetOperation(4, selectedIndex).
// Original: 0x1400b5fd0.

static void SaveLoadDialog_DoSave(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Set minimum operation time (1.5f seconds — DAT_140172160)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    fn_SaveLoad_SetMinOperationTime(saveLoad, minTime);

    // Check if the selected save slot already contains data
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
    int validState = *(int*)((uintptr_t)saveInfo + 0x40);
    if (validState >= 0) {
        // Slot is occupied — ask user to confirm overwrite
        int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmSave");
        if (result != 2) return;  // user cancelled
    }

    // Mark save pending
    *(uint8_t*)(s + 0xB9) = 1;

    // Copy save name from the text element at +0xE8 (the name input field)
    // FUN_140011f90 inlined: String::String(retval, *(dialog+0xE8) + 0x28)
    void* nameSource = *(void**)(s + 0xE8);
    System::String saveName(
        *reinterpret_cast<const System::String*>((uintptr_t)nameSource + 0x28));  // B.3.1 bridge
    fn_SaveLoad_SetSaveName(saveLoad, &saveName, 2);

    // Start save operation (4 = save, with selected slot index)
    fn_SaveLoad_SetOperation(saveLoad, 4, selectedIdx);
}

// ============================================================
// DoLoad (0x1400b61e0): Confirm load, check mod compatibility, then trigger load.
// ============================================================
// Sets min operation time (1.5f seconds).
// If game world is loaded (DAT_1401c3dd8 != 0):
//   Shows "Dialog/SaveLoad.rsc:confirmLoad" dialog; if not confirmed, returns.
// Iterates through all mods in the selected save's mod list:
//   For each mod with a non-zero CheckMod status, shows a mod-warning dialog
//   ("modsOutOfDate", "modsVersion", "modsChanged", or "modsMissing") with
//   mod name/version info set as TextElement parameters.
//   If user declines any warning, returns without loading.
// Sets loadFlag (+0xBA = 1), starts operation 2 (load) with selected index.
// Original: 0x1400b61e0.

static void SaveLoadDialog_DoLoad(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Set minimum operation time (1.5f seconds)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    fn_SaveLoad_SetMinOperationTime(saveLoad, minTime);

    // If game world is active, ask user to confirm leaving the current game
    if (*g_pGameWorldSingleton != nullptr) {
        int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmLoad");
        if (result != 2) return;
    }

    // Iterate through mods in the selected save and warn about any incompatibilities
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
    int modCount = *(int*)((uintptr_t)saveInfo + 0x50);
    printf("[SaveLoadDialog::DoLoad] selectedIdx=%d saveInfo=%p save-mod-count=%d gameWorld=%p\n",
           selectedIdx, saveInfo, modCount,
           g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
    fflush(stdout);
    // The list CheckMod matches the save against is IO::Modification::_singleton.
    // Printed alongside so a non-zero CheckMod can be READ rather than guessed at.
    if (g_ppModificationSingleton && *g_ppModificationSingleton) {
        void* modSgl = *g_ppModificationSingleton;
        int installed = reinterpret_cast<const IO::Modification*>(modSgl)->GetCount();   // B.3.4 bridge
        printf("[SaveLoadDialog::DoLoad] installed mod count=%d\n", installed);
        for (int mi = 0; mi < installed; mi++) {
            void* m = const_cast<RtIO::Mod*>(
                &reinterpret_cast<const IO::Modification*>(modSgl)->GetModification(mi));   // B.3.4 bridge
            const char* path = m ? *(const char* const*)((uintptr_t)m + 0x58) : nullptr;
            printf("[SaveLoadDialog::DoLoad]   installed[%d] path=\"%s\"\n", mi, path ? path : "(null)");
        }
        fflush(stdout);
    }

    for (int i = 0; i < modCount; i++) {
        int modStatus = fn_SaveLoad_CheckMod(saveLoad, selectedIdx, i);
        {
            void* si = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
            void* nameBase = *(void**)((uintptr_t)si + 0x48);
            const char* nm = nameBase
                ? *(const char* const*)((uintptr_t)nameBase + (uintptr_t)i * 0x20)
                : nullptr;
            printf("[SaveLoadDialog::DoLoad]   save mod[%d] \"%s\" CheckMod=%d\n",
                   i, nm ? nm : "(null)", modStatus);
            fflush(stdout);
        }
        if (modStatus == 0) continue;  // mod is fine

        // saveInfo+0x48 = base of the save's ModInfo array; each entry is 0x20 bytes.
        // entry+0x00 is the mod's DISPLAY NAME ("Debug Menu") and is what the
        // warning dialog shows. It is NOT what CheckMod compares: CheckMod
        // (retail 0x1800c1090) concatenates FileSystem::GetDataDirectory() with
        // the String at entry+**0x08** -- the package path relative to the data
        // directory -- and matches that against IO::Modification's per-entry path
        // String at +0x58. The proof is one instruction, `LEA R8,[RBP+0x8]`
        // immediately before the System::operator+ call, where RBP is
        // modIndex*0x20 + saveInfo->modsData. So a save can name a mod whose
        // display name you recognise and still report missing, because the two
        // strings are different fields. Layout: runtime/types/save/SaveLoad.h.
        // (Wave 5 lane C, which ported CheckMod; this comment previously said
        // only "entry+0 = String", which is right for printing and wrong for
        // the comparison.)
        void* saveInfo2 = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        void* modNameBase = *(void**)((uintptr_t)saveInfo2 + 0x48);
        void* modNameStr = (void*)((uintptr_t)modNameBase + (uintptr_t)i * 0x20);  // String at stride 0x20

        // Select the appropriate warning dialog based on mod status
        const char* warnPath = nullptr;
        if      (modStatus == 2) warnPath = "Dialog/SaveLoad.rsc:modsOutOfDate";
        else if (modStatus == 3) warnPath = "Dialog/SaveLoad.rsc:modsVersion";
        else if (modStatus == 4) warnPath = "Dialog/SaveLoad.rsc:modsChanged";
        else                     warnPath = "Dialog/SaveLoad.rsc:modsMissing";

        // Load the warning dialog External resource
        System::Filename warnFilename(warnPath);
        void* warnExt = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
                ->FindOrLoad(warnFilename));

        void* checkedWarn = nullptr;
        if (warnExt) {
            if (fn_IsDerivedFrom(warnExt, *g_pExternalTypeId)) {
                checkedWarn = warnExt;
            }
        }

        // Allocate and construct the warning DialogInstance shell
        void* warnDlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        if (warnDlg) {
            uint64_t nullParent = 0;
            fn_DialogInstance_Ctor(warnDlg, *(void**)(s + 0x10), checkedWarn, &nullParent, 0, 0, 0);
            *(void**)warnDlg = (void*)ResolveAddr(Module::Exe, MessageDialogAddr::Vtable);
        }

        // Find the TextElement inside the warning dialog and set mod info parameters
        // FindElementType on the dialog InternalPtr returns a TextElement InternalPtr
        uint64_t warnDlgPtr = 0;
        new (&warnDlgPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(warnDlg));   // B.3.3 bridge

        // Find TextElement within dialog (uses g_TypeId_TextElement)
        uint64_t textElemPtr = 0;
        fn_DialogInstance_FindElementType((void*)warnDlgPtr, &textElemPtr, *g_TypeId_TextElement);

        // Check IsDerivedFrom TextElement
        void* rawElem = *(void**)&textElemPtr;
        bool isText = fn_IsDerivedFrom(rawElem, *g_TypeId_TextElement);
        void* textElem = isText ? rawElem : nullptr;

        uint64_t textPtr = 0;
        new (&textPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(textElem));   // B.3.3 bridge

        // SetParameter(0) = mod name, SetParameter(1) = mod name (again, as per decompilation)
        fn_TextElement_SetParameter((void*)textPtr, 0, modNameStr);
        fn_TextElement_SetParameter((void*)textPtr, 1, (void*)((uintptr_t)modNameStr + 8));

        // SetParameter(2) = formatted version string "%i (%s)"
        // saveInfo+0x14 = isBeta (uint, sub-field), saveInfo+0x18 = version int
        // Configuration::GetVersionString returns a const String*
        void* saveInfo3 = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        void* modEntry = (void*)((uintptr_t)*(void**)((uintptr_t)saveInfo3 + 0x48) + (uintptr_t)i * 0x20);
        uint32_t isBeta   = *(uint32_t*)((uintptr_t)modEntry + 0x14);
        uint32_t modVer   = *(uint32_t*)((uintptr_t)modEntry + 0x18);
        System::String verStrBuf;
        fn_Configuration_GetVersionString(&verStrBuf, isBeta, (bool)(isBeta != 0));
        System::String fmtBuf = System::String::Format("%i (%s)", (unsigned)modVer, verStrBuf._data);
        fn_TextElement_SetParameter((void*)textPtr, 2, &fmtBuf);

        // Show the warning dialog and check if user confirmed.
        // Retail (0x1400b61e0) copies the dialog InternalPtr into a scratch slot,
        // hands ShowDialog the COPY (it takes its argument by value and writes
        // through the slot it is given), and releases the ORIGINAL afterwards with
        // InternalBasePtr::DeletePtr. There is no String destructor anywhere on this
        // path.
        //
        // This block used to call `fn_String_Dtor(&showPtr)` on an InternalBasePtr
        // holding `warnDlg`, a MemAlloc(0xC0) user pointer. System::String::~String
        // does `*(int*)(data-4) -= 1`, and for a MemAlloc user pointer `data-4` is the
        // HIGH DWORD OF THE ALLOCATOR BOUNDARY TAG (`data-8`), not a refcount -- so it
        // decremented a live tag, borrowing into the tag's 41-bit size field and
        // setting bits 32..40. That is the "load-path wild write whose fault address is
        // a real pointer with +0x7FC in the high dword" (0x1FF << 32, times 4 = 0x7FC
        // in the high dword of block + size*4). It was never a wild write and never
        // save-specific: it is this line, and it fires on any load whose save names a
        // mod that is not installed.
        uint64_t showPtr = warnDlgPtr;
        int warnResult = SaveLoad_ModalShowDialog(*(void**)(s + 0x10), &showPtr, warnPath);
        reinterpret_cast< ::Object::InternalBasePtr*>(&warnDlgPtr)->DeletePtr();   // B.3.3 bridge

        if (warnResult != 2) return;  // user declined mod warning — abort load
    }

    // All checks passed — trigger load operation
    *(uint8_t*)(s + 0xBA) = 1;
    fn_SaveLoad_SetOperation(saveLoad, 2, selectedIdx);
}

// ============================================================
// SaveLoadDialog::OnEvent (0x1400b67e0) — Slot8
// ============================================================
// Handles button clicks and list selection changes.
// eventType 2 = list selection change:
//   eventData in (3, saveCount+4) → select entry at (eventData-4)
// eventType 0x12 = button click:
//   eventData 0 = buttonCancel → close dialog
//   eventData 1 = buttonSave → save with optional confirm dialog
//   eventData 2 = buttonLoad → load with optional confirm dialogs
//   eventData 3 = buttonDelete → delete with confirm dialog
//
// Original: 0x1400b67e0.

// Helper: set selected index and play UI sound
static void SaveLoadDialog_SelectEntry(SaveLoadDialog* self, int index) {
    uintptr_t s = (uintptr_t)self;
    // FUN_1400b6790: if index changed, play sound 4 first
    int oldIndex = *(int*)(s + 0xC0);
    if (oldIndex != index) {
        fn_UI_Play(*(void**)(s + 0x10), 4);
    }
    *(int*)(s + 0xC0) = index;
}

void SaveLoadDialog_OnEvent(SaveLoadDialog* self, void* element, int eventType, int eventData) {
    uintptr_t s = (uintptr_t)self;

    if (eventType == 2) {
        // List row selection change
        int saveCount = *(int*)(s + 0xC4);
        if (eventData > 3 && eventData < saveCount + 4) {
            SaveLoadDialog_SelectEntry(self, eventData - 4);
        }
        return;
    }

    if (eventType == 0x12) {
        if (eventData == 0) {
            // buttonCancel: close dialog
            void* ui = *(void**)(s + 0x10);
            uint64_t basePtr[2];
            new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
            fn_UI_CloseDialog(ui, basePtr);
            return;
        }
        if (eventData == 2) {
            SaveLoadDialog_DoLoad(self);
            return;
        }
        if (eventData == 1) {
            SaveLoadDialog_DoSave(self);
            return;
        }
        if (eventData == 3) {
            SaveLoadDialog_DoDelete(self);
            return;
        }
    }
}

// ============================================================
// SaveLoadDialog vtable + ctor (save/load wave 2026-08-27)
// ============================================================
// Retail vtable @ 0x14015f900 (10 slots — verified via read_memory):
//   +0x00 Dtor            0x1400211b0
//   +0x08 TypeName        0x140021150
//   +0x10 TypeId          0x140021140
//   +0x18 Persist         0x14003a948 (runtime thunk)
//   +0x20 Startup         0x1400b7d00 → SaveLoadDialog_Startup (REIMPL'd)
//   +0x28 Shutdown        0x14003a94e (runtime thunk)
//   +0x30 OnUpdateLayout  0x1400b6880 → SaveLoadDialog_Update (REIMPL'd)
//   +0x38 OnUpdate        0x1400b79b0 → SaveLoadDialog_Load (REIMPL'd; the
//                         per-frame op tracker + loadFlag game transition)
//   +0x40 OnEvent         0x1400b67e0 → SaveLoadDialog_OnEvent (REIMPL'd)
//   +0x48 OnSerialize     0x14003a954 (runtime thunk)
// The ctor is 0x1400b8270 (fn_SaveLoadDialog_Ctor resolves it; it was
// unregistered, so the menu Load/Save buttons silently did nothing).

#include "Object/factory_registration.h"
#include "address_registry.h"

namespace {

void* GetSaveLoadDialogVtable();

void* SLDStub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetSaveLoadDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}
const char* SLDStub_TypeName(void* /*self*/) { return "SaveLoadDialog"; }
uint32_t SLDStub_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("SaveLoadDialog");
    return cached;
}
template <int SlotIdx>
void SLDStub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[SaveLoadDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
               SlotIdx, self);
    }
}

void* g_SaveLoadDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&SLDStub_Dtor),                 // +0x00
    reinterpret_cast<void*>(&SLDStub_TypeName),             // +0x08
    reinterpret_cast<void*>(&SLDStub_TypeId),               // +0x10
    reinterpret_cast<void*>(&SLDStub_RuntimeThunk<3>),      // +0x18 Persist
    reinterpret_cast<void*>(&SaveLoadDialog_Startup),       // +0x20
    reinterpret_cast<void*>(&SLDStub_RuntimeThunk<5>),      // +0x28 Shutdown
    reinterpret_cast<void*>(&SaveLoadDialog_Update),        // +0x30
    reinterpret_cast<void*>(&SaveLoadDialog_Load),          // +0x38
    reinterpret_cast<void*>(&SaveLoadDialog_OnEvent),       // +0x40
    reinterpret_cast<void*>(&SLDStub_RuntimeThunk<9>),      // +0x48
};

void* GetSaveLoadDialogVtable() {
    return reinterpret_cast<void*>(g_SaveLoadDialog_Vtable_Standalone);
}

} // namespace

// In-place ctor — clean reimpl of exe 0x1400b8270. Mirrors the disassembly:
// DialogInstance base ctor (parent IBP copied to a local, tail args 0,0,0),
// vtable patch, mode byte at +0xB8, flags/selection/rotation zeroed, Sprite
// default-ctor at +0xD0, loader/engine ptrs at +0xE0/+0xE8, +0xF0..+0x158 zero.
extern "C" void* SaveLoadDialog_Ctor_8270(void* self, void* ui, void* extRes,
                                          void* parentIBP, uint8_t mode,
                                          void* enginePtr, void* gameWorldPtr) {
    uintptr_t s = (uintptr_t)self;

    uint64_t ibpCopy = parentIBP ? *(uint64_t*)parentIBP : 0;
    fn_DialogInstance_Ctor(self, ui, extRes, &ibpCopy, 0, 0, 0);
    *(void**)self = GetSaveLoadDialogVtable();

    *(uint8_t*)(s + 0xB8) = mode;
    *(uint32_t*)(s + 0xB9) = 0;          // saveFlag/loadFlag/refreshFlag/deleteFlag
    *(uint32_t*)(s + 0xC0) = 0;          // selectedIndex
    *(uint32_t*)(s + 0xC8) = 0;          // spinner rotation
    if (fn_Sprite_DefaultCtor) fn_Sprite_DefaultCtor((void*)(s + 0xD0));
    *(void**)(s + 0xE0) = enginePtr;
    *(void**)(s + 0xE8) = gameWorldPtr;
    for (uintptr_t off = 0xF0; off <= 0x158; off += 8)
        *(uint64_t*)(s + off) = 0;

    // Retail's ctor caller path lands in SaveLoadDialog_Startup via the UI
    // dialog-open machinery (vtable +0x20); the +0xBB refreshFlag=1 kick that
    // triggers the initial file-list scan happens there.
    return self;
}

void RegisterSaveLoadDialog() {
    RegisterExeAddr(0x1400b8270, reinterpret_cast<uintptr_t>(&SaveLoadDialog_Ctor_8270));
    RegisterExeAddr(0x14015f900, reinterpret_cast<uintptr_t>(g_SaveLoadDialog_Vtable_Standalone));
}
