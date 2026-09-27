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

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/MessageDialog.h"
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/PageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "save/SaveLoad.h"   // ENDGAME B.3.7 callers (K1)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/SaveLoadDialog.h"
#include "Game/GameUpdateLoop.h"       // ENDGAME B.3.9 EXA: the real loop classes
#include "Game/GameLoadDialogLoopT.h"
#include "Game/GameLoaderLoop.h"
#include "Dialog/ui_dialogs.h"
// ENDGAME B.3.5 batch K1: the runtime type headers come FIRST — they reach
// runtime/types/volume/Volume.h, whose global-scope `struct Rectangle` is hidden by
// <wingdi.h>'s `Rectangle()` once <windows.h> has been seen through resolve.h.
#include "video/ImageBuffer.h"
#include "graphics/Material.h"
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
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time


// ============================================================
// Helper: Find element by name in dialog instance
// ============================================================
static uint64_t FindDialogElem(void* dialogInst, const char* name) {
    System::String nameStr(name);
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
    return result;
}

// ============================================================
// Helper: Find element by name in an Element tree, return InternalBasePtr
// ============================================================
static uint64_t FindChildElem(void* parentElem, const char* name) {
    System::String nameStr(name);
    uint64_t resultPtr = 0;
    ::new (&resultPtr) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge
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
    reinterpret_cast<UI::TableElement*>(tableElem)->SetSize(totalSlots / 3 + 1, 3);  // B.3.6 bridge

    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    void* saveLoad = ::Save::SaveLoad::_singleton;

    // --- Phase 3: Populate save file entries ---
    for (int i = 0; i < saveCount; i++) {
        void* saveInfo = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(i));  // B.3.7 bridge

        // Clone template element from dialog
        uint8_t templateBuf[16] = {};
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18))->GetTemplate(0));  // B.3.6 bridge
        void* templateObj = *(void**)templateBuf;

        // Clone via vtable[8]: templateObj->Clone(&cloneBuf, dialog)
        // (= UI::ElementDescription slot 8 CreateInstance, RETSLOT per B6-2)
        uint8_t cloneBuf[16] = {};
        reinterpret_cast<const UI::ElementDescription*>(templateObj)->CreateInstance(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18)));
        void* clonedElem = *(void**)cloneBuf;

        // Add cloned element to table at index i
        reinterpret_cast<UI::TableElement*>(*(void**)(s + 0x108))->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(&clonedElem), i);  // B.3.6 bridge

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
        reinterpret_cast<UI::Element*>((void*)entry[2])->SetFlagsOnOff(0x10000, 0);  // B.3.6 bridge

        // Set imageSaveIcon element ID for selection tracking (ID = i + 4)
        reinterpret_cast<UI::Element*>((void*)entry[2])->SetElementId(i + 4);  // B.3.6 bridge

        // Check if SaveInfo has a valid Sprite (thumbnail)
        bool hasSpriteThumb = reinterpret_cast<const UI::Sprite*>(saveInfo)->IsValid();  // B.3.6 bridge

        if (hasSpriteThumb) {
            // --- Direct sprite from save info ---
            // Copy the 16-byte Sprite from SaveInfo
            uint8_t sprite[16];
            memcpy(sprite, saveInfo, 16);

            reinterpret_cast<UI::ImageElement*>((void*)entry[2])->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge

            // Hide imageCorrupt overlay
            reinterpret_cast<UI::Element*>((void*)entry[4])->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        } else {
            // --- Build sprite from raw image data ---
            // Allocate MaterialInstance (0x60 bytes)
            void* matInst = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
            void* matInstFinal = nullptr;
            if (matInst) {
                ::new (matInst) Graphics::MaterialInstance();   // B.3.9 EXA: the real class (exe table 0x14015c400 is its own)
                matInstFinal = matInst;
            }
            entry[6] = (uint64_t)matInstFinal;

            // Load UIMaterialNoBlend material
            System::Filename matFilename("Material/UI/UIMaterialNoBlend.rsc");
            void* matExternal = const_cast<::Object::External*>(
                ::Object::ExternalFactory::_singleton  // B.3.3 bridge
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
            reinterpret_cast<Graphics::MaterialInstance*>(matInstFinal)->SetMaterial(
                Object::ExternalPtr<Graphics::Material>(
                    reinterpret_cast<Graphics::Material*>(
                        matExternal ? matExternal : nullptr)));   // B.3.5 bridge

            // Add texture: "diffuse" -> saveInfo's ImageBuffer at +0x18
            System::String diffuseStr("diffuse");
            reinterpret_cast<Graphics::MaterialInstance*>(matInstFinal)->AddTexture(
                *reinterpret_cast<const RtSystem::String*>(&diffuseStr),
                Object::ExternalPtr<Video::ImageBuffer>(
                    *reinterpret_cast<Video::ImageBuffer**>(
                        (uintptr_t)saveInfo + 0x18)));   // B.3.5 bridge

            reinterpret_cast<Graphics::MaterialInstance*>(matInstFinal)
                ->BuildParameters();   // B.3.5 bridge

            // Allocate SpriteSheet (0x58 bytes)
            void* sheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
            void* sheetFinal = nullptr;
            if (sheet) {
                ::new (sheet) UI::SpriteSheet();   // B.3.9 EXA: the real class (exe table 0x14015c3b0 is its own)
                sheetFinal = sheet;
            }

            // Set material on spritesheet
            *(void**)((uintptr_t)sheetFinal + 0x30) = matInstFinal;

            // Get image dimensions
            void* imageBuffer = *(void**)((uintptr_t)saveInfo + 0x18);
            int imgHeight = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                                ->Height();   // B.3.5 bridge
            int imgWidth = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                               ->Width();    // B.3.5 bridge

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
            imgHeight = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                            ->Height();   // B.3.5 bridge
            imgWidth = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                           ->Width();     // B.3.5 bridge
            reinterpret_cast<UI::SpriteSheet*>(sheetFinal)->AddSpriteInfo(  // B.3.6 bridge
                nameStr1, nameStr2, 0, 0, imgWidth, imgHeight);

            // Set sprite info with UV coordinates
            imageBuffer = *(void**)((uintptr_t)saveInfo + 0x18);
            int h = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                        ->Height();   // B.3.5 bridge
            int w = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)
                        ->Width();    // B.3.5 bridge
            reinterpret_cast<UI::SpriteSheet*>(sheetFinal)->SetSpriteInfo(  // B.3.6 bridge
                0, w, h, 0.0f, 0.0f, uvScale, uvScale);

            // Create Sprite from SpriteSheet
            uint8_t spriteBuf[16] = {};
            ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(sheetFinal)), 0);  // B.3.6 bridge

            reinterpret_cast<UI::ImageElement*>((void*)entry[2])->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge

            entry[7] = (uint64_t)sheetFinal;

            // Check save validity: status at saveInfo+0x44
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                // Valid save: hide imageCorrupt overlay
                reinterpret_cast<UI::Element*>((void*)entry[4])->SetFlagsOnOff(1, 0);  // B.3.6 bridge
            } else {
                // Invalid/corrupt: dim the icon
                reinterpret_cast<UI::Element*>((void*)entry[2])->SetColor(0xff808080);  // B.3.6 bridge
            }
        }

        // --- Set text parameters if save has valid data ---
        if (*(int*)((uintptr_t)saveInfo + 0x40) >= 0) {
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                // Valid save: show formatted date string
                System::String dateStr;
                fn_DateTime_AsString((void*)((uintptr_t)saveInfo + 0x30), &dateStr);
                reinterpret_cast<UI::TextElement*>((void*)entry[1])->SetParameter(0, dateStr);  // B.3.6 bridge
            } else {
                // Corrupt save: show "Corrupt" text from string table
                System::String corruptKey("Corrupt");
                uintptr_t keyMapObj = (uintptr_t)*g_pKeyMapSingleton;
                uint8_t uiStr[16] = {};
                ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(keyMapObj + 0xD8))), corruptKey);  // B.3.6 bridge
                System::String sysStr;
                ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge
                reinterpret_cast<UI::TextElement*>((void*)entry[1])->SetParameter(0, sysStr);  // B.3.6 bridge
            }

            // Set save name: saveInfo+0x10 is the name String
            reinterpret_cast<UI::TextElement*>((void*)entry[0])->SetParameter(0, *reinterpret_cast<const System::String*>((void*)((uintptr_t)saveInfo + 0x10)));  // B.3.6 bridge
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
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18))->GetTemplate(0));  // B.3.6 bridge
        void* templateObj = *(void**)templateBuf;

        uint8_t cloneBuf[16] = {};
        reinterpret_cast<const UI::ElementDescription*>(templateObj)->CreateInstance(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18)));
        void* clonedElem = *(void**)cloneBuf;

        // Add to table
        reinterpret_cast<UI::TableElement*>(*(void**)(s + 0x108))->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(&clonedElem), i);  // B.3.6 bridge

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
        reinterpret_cast<UI::Element*>((void*)entry[4])->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>((void*)entry[5])->SetFlagsOnOff(1, 0);  // B.3.6 bridge

        // Dim border (SetColor via vtable[10] = 0x80ffffff)
        {
            reinterpret_cast<UI::Element*>((void*)entry[3])->SetColor(0x80ffffff);  // B.3.6 bridge
        }

        // Dim icon (SetColor via vtable[10] = 0x20ffffff)
        {
            reinterpret_cast<UI::Element*>((void*)entry[2])->SetColor(0x20ffffff);  // B.3.6 bridge
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

void SaveLoadDialog::OnOpened() {
    uintptr_t s = (uintptr_t)this;

    // --- Find all named elements ---
    *(uint64_t*)(s + 0xF8)  = FindDialogElem(this, "labelLoad");
    *(uint64_t*)(s + 0x100) = FindDialogElem(this, "labelSave");
    *(uint64_t*)(s + 0x140) = FindDialogElem(this, "buttonSave");
    *(uint64_t*)(s + 0x148) = FindDialogElem(this, "buttonLoad");
    *(uint64_t*)(s + 0x150) = FindDialogElem(this, "buttonCancel");
    *(uint64_t*)(s + 0x158) = FindDialogElem(this, "buttonDelete");
    *(uint64_t*)(s + 0xF0)  = FindDialogElem(this, "mainPage");
    *(uint64_t*)(s + 0x120) = FindDialogElem(this, "textLoading");
    *(uint64_t*)(s + 0x128) = FindDialogElem(this, "textSaving");
    *(uint64_t*)(s + 0x130) = FindDialogElem(this, "textPleaseWait");
    *(uint64_t*)(s + 0x138) = FindDialogElem(this, "loadingImage");
    *(uint64_t*)(s + 0x108) = FindDialogElem(this, "tableSaveList");

    // --- Set button element IDs ---
    reinterpret_cast<UI::Element*>((void*)*(uint64_t*)(s + 0x140))->SetElementId(1);   // B.3.6 bridge; buttonSave
    reinterpret_cast<UI::Element*>((void*)*(uint64_t*)(s + 0x148))->SetElementId(2);   // B.3.6 bridge; buttonLoad
    reinterpret_cast<UI::Element*>((void*)*(uint64_t*)(s + 0x150))->SetElementId(0);   // B.3.6 bridge; buttonCancel
    reinterpret_cast<UI::Element*>((void*)*(uint64_t*)(s + 0x158))->SetElementId(3);   // B.3.6 bridge; buttonDelete

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
    ::Save::SaveLoad::_singleton->SetSaveSprite(reinterpret_cast<const UI::Sprite*>(spriteCopy));  // B.3.7 bridge

    // --- Set minimum operation time (0.75f seconds) ---
    uint8_t timeVal[8];
    fn_Time_Ctor_Float(timeVal, 0.75f);
    ::Save::SaveLoad::_singleton->SetMinOperationTime(*reinterpret_cast<const System::Time*>(timeVal));  // B.3.7 bridge

    // --- Start operation: 1 (list/scan), param = -2 or -3 based on mode ---
    uint8_t mode = *(uint8_t*)(s + 0xB8);
    int opParam = -2 - (int)(mode != 0);
    ::Save::SaveLoad::_singleton->SetOperation(static_cast<Save::SaveLoad::Operation>(1), opParam);  // B.3.7 bridge
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

void SaveLoadDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    uint8_t mode = *(uint8_t*)(s + 0xB8);
    void* saveLoad = ::Save::SaveLoad::_singleton;
    if (!saveLoad) return;

    // --- Show/hide load/save labels based on mode ---
    // labelLoad (+0xF8): visible when mode == 0 (save mode shows "load" label? no...)
    // Actually: labelLoad visible when NOT save mode, labelSave visible when save mode
    uint32_t isLoadMode = (uint32_t)(mode == 0);
    uint32_t isSaveMode = (uint32_t)(mode != 0);
    void* labelLoad = (void*)*(uint64_t*)(s + 0xF8);
    if (labelLoad) reinterpret_cast<UI::Element*>(labelLoad)->SetFlagsOnOff(isLoadMode, 0);  // B.3.6 bridge
    void* labelSave = (void*)*(uint64_t*)(s + 0x100);
    if (labelSave) reinterpret_cast<UI::Element*>(labelSave)->SetFlagsOnOff(isSaveMode, 0);  // B.3.6 bridge

    // buttonSave (+0x140): visible in save mode
    void* buttonSave = (void*)*(uint64_t*)(s + 0x140);
    if (buttonSave) reinterpret_cast<UI::Element*>(buttonSave)->SetFlagsOnOff(isSaveMode, 0);  // B.3.6 bridge
    // buttonLoad (+0x148): visible in load mode
    void* buttonLoad = (void*)*(uint64_t*)(s + 0x148);
    if (buttonLoad) reinterpret_cast<UI::Element*>(buttonLoad)->SetFlagsOnOff(isLoadMode, 0);  // B.3.6 bridge

    int operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge

    // Check if any operation is in progress or any deferred flag is set
    bool anyFlagsSet = (*(uint8_t*)(s + 0xBB) != 0) ||
                       (*(uint8_t*)(s + 0xB9) != 0) ||
                       (*(uint8_t*)(s + 0xBA) != 0) ||
                       (*(uint8_t*)(s + 0xBC) != 0);

    if (operation == 0 && !anyFlagsSet) {
        // --- IDLE STATE: show main page, update buttons and entry list ---
        void* pageElem = (void*)*(uint64_t*)(s + 0xF0);
        if (pageElem) reinterpret_cast<UI::PageElement*>(pageElem)->SetActivePage(1);  // B.3.6 bridge

        // Enable/disable buttonLoad based on selected save validity
        int selectedIdx = *(int*)(s + 0xC0);
        int saveCount = *(int*)(s + 0xC4);
        bool loadEnabled = false;
        if (saveCount >= 1 && selectedIdx >= 0) {
            void* saveInfo = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
            // saveInfo+0x44 == 2 means valid save
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                loadEnabled = true;
            }
        }

        // buttonLoad: SetEnabled via vtable[8]
        {
            void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
            if (loadBtn) {
                reinterpret_cast<UI::Element*>(loadBtn)->Enable(loadEnabled ? 1 : 0);  // B.3.6 bridge
            }
        }

        // buttonSave: always enabled
        {
            void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
            if (saveBtn) {
                reinterpret_cast<UI::Element*>(saveBtn)->Enable(1);  // B.3.6 bridge
            }
        }

        // buttonCancel: always enabled
        {
            void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
            if (cancelBtn) {
                reinterpret_cast<UI::Element*>(cancelBtn)->Enable(1);  // B.3.6 bridge
            }
        }

        // Check selected save for delete eligibility
        if (selectedIdx >= 0) {
            reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx);  // B.3.7 bridge
        }

        // buttonDelete: SetEnabled via vtable[8]
        {
            void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
            if (deleteBtn) {
                reinterpret_cast<UI::Element*>(deleteBtn)->Enable((uint64_t)(selectedIdx >= 0));  // B.3.6 bridge
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
            if (selectElem) reinterpret_cast<UI::Element*>(selectElem)->SetFlagsOnOff(notSelected ? 1 : 0, notSelected ? 0 : 1);  // B.3.6 bridge

            // entry+0x18: borderSave — shown when NOT selected, hidden when selected
            void* borderElem = (void*)*(uint64_t*)(entryBase + 0x18);
            if (borderElem) reinterpret_cast<UI::Element*>(borderElem)->SetFlagsOnOff(notSelected ? 0 : 1, notSelected ? 1 : 0);  // B.3.6 bridge
        }
        return;
    }

    // --- OPERATION IN PROGRESS: show loading page ---
    // Rotate loading spinner image
    void* spinnerImg = (void*)*(uint64_t*)(s + 0x138);
    if (spinnerImg) reinterpret_cast<UI::ImageElement*>(spinnerImg)->SetRotation(*(float*)(s + 0xC8));  // B.3.6 bridge

    // Switch to loading page (page 0)
    void* pageElemOp = (void*)*(uint64_t*)(s + 0xF0);
    if (pageElemOp) reinterpret_cast<UI::PageElement*>(pageElemOp)->SetActivePage(0);  // B.3.6 bridge

    // Disable all four buttons
    {
        void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
        if (loadBtn) {
            reinterpret_cast<UI::Element*>(loadBtn)->Enable(0);  // B.3.6 bridge
        }
    }
    {
        void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
        if (saveBtn) {
            reinterpret_cast<UI::Element*>(saveBtn)->Enable(0);  // B.3.6 bridge
        }
    }
    {
        void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
        if (cancelBtn) {
            reinterpret_cast<UI::Element*>(cancelBtn)->Enable(0);  // B.3.6 bridge
        }
    }
    {
        void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
        if (deleteBtn) {
            reinterpret_cast<UI::Element*>(deleteBtn)->Enable(0);  // B.3.6 bridge
        }
    }

    // --- Show/hide status text based on operation type ---
    // Operation types: 1 = listing, 2 = saving, 4 = deleting
    void* textPleaseWait = (void*)*(uint64_t*)(s + 0x130);
    void* textLoading = (void*)*(uint64_t*)(s + 0x120);
    void* textSaving = (void*)*(uint64_t*)(s + 0x128);
    if (operation == 1) {
        // Listing: show textPleaseWait, hide textLoading, hide textSaving
        if (textPleaseWait) reinterpret_cast<UI::Element*>(textPleaseWait)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        if (textLoading) reinterpret_cast<UI::Element*>(textLoading)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        if (textSaving) reinterpret_cast<UI::Element*>(textSaving)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
    } else if (operation == 2) {
        // Saving: hide textPleaseWait, show textLoading, hide textSaving
        if (textPleaseWait) reinterpret_cast<UI::Element*>(textPleaseWait)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        if (textLoading) reinterpret_cast<UI::Element*>(textLoading)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        if (textSaving) reinterpret_cast<UI::Element*>(textSaving)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
    } else if (operation == 4) {
        // Deleting: hide textPleaseWait, hide textLoading, show textSaving
        if (textPleaseWait) reinterpret_cast<UI::Element*>(textPleaseWait)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        if (textLoading) reinterpret_cast<UI::Element*>(textLoading)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        if (textSaving) reinterpret_cast<UI::Element*>(textSaving)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
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

void SaveLoadDialog::OnUpdate(const System::Time& time) {
    uintptr_t s = (uintptr_t)this;
    void* saveLoad = ::Save::SaveLoad::_singleton;

    // Phase 1: accumulate spinner rotation while I/O is in progress
    int operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
    if (operation != 0) {
        float elapsed = time.AsSeconds();
        *(float*)(s + 0xC8) += elapsed * 180.0f;
    }

    // Re-check — may have just completed this frame
    operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
    if (operation != 0) return;

    // Phase 2: I/O complete, process one deferred flag (priority order matches original)

    // deleteFlag (+0xBC): re-trigger list scan
    if (*(uint8_t*)(s + 0xBC) != 0) {
        *(uint16_t*)(s + 0xBB) = 1;   // sets +0xBB=1 (refresh), +0xBC=0 (clear delete)
        uint8_t mode = *(uint8_t*)(s + 0xB8);
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(1), -2 - (int)(mode != 0));  // B.3.7 bridge
        return;
    }

    // refreshFlag (+0xBB): populate file list
    if (*(uint8_t*)(s + 0xBB) != 0) {
        int count = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveCount();  // B.3.7 bridge
        *(int*)(s + 0xC4) = count;
        if (count <= *(int*)(s + 0xC0)) {
            *(int*)(s + 0xC0) = count - 1;
        }
        SaveLoadDialog_PopulateFileList(this);
        *(uint8_t*)(s + 0xBB) = 0;
        return;
    }

    // saveFlag (+0xB9): save done, close dialog (no game transition needed)
    if (*(uint8_t*)(s + 0xB9) != 0) {
        uint64_t basePtr = 0;
        new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)s));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge
        *(uint8_t*)(s + 0xB9) = 0;
        return;
    }

    // loadFlag (+0xBA): load done — close dialog and transition to game load screen
    if (*(uint8_t*)(s + 0xBA) != 0) {
        // Close the SaveLoad dialog
        uint64_t basePtr = 0;
        new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)s));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge

        // ENDGAME B.3.9 EXA: the three real loop classes, built as retail's
        // 0x1400b79b0 builds them.
        // --- Step 1: the inner GameUpdateLoop (0x160 bytes), ctor 0x140098060 ---
        // Retail: (savedState = GetSaveState(), &InternalBasePtr(0), -1, name,
        // 0, 0, p8 = 1, 0, 0, 0). p8 = 1 writes +0xD0 = 0.
        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        Engine::UpdateLoop* pGameLoop = nullptr;
        if (gameLoop) {
            void* savedState = reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetSaveState();  // B.3.7 bridge
            // Retail (0x1400b7b1b): `MOV [RSP+0xc0],RBX` with RBX = 0 — an EMPTY
            // String (null COW pointer), not String("") (a 1-byte "" buffer).
            const System::String nameStr;
            pGameLoop = ::new (gameLoop) GameUpdateLoop(
                savedState, /*scenarioIbp=*/0, /*scenarioIndex=*/-1, nameStr,
                0, 0, /*p8=*/true, nullptr, nullptr, nullptr);
        }

        // --- Step 2: the GameLoadDialogLoopT (0xb60 bytes) — the subclass ctor
        // retail inlines here ---
        void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
        Engine::UpdateLoop* pDlLoop = nullptr;
        if (dlLoop) {
            uint64_t delayTime = 0;
            fn_Time_Ctor_Float(&delayTime, 1.0f);
            System::String dialogNameStr;
            GetLoadingSoundString(&dialogNameStr);
            pDlLoop = ::new (dlLoop) GameLoadDialogLoopT(
                dialogNameStr, pGameLoop, *reinterpret_cast<const System::Time*>(&delayTime));
        }

        // --- Step 3: the outer GameLoaderLoop (0x48 bytes) ---
        void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
        void* pOuterLoop = nullptr;
        if (outerLoop) {
            uint64_t zeroTime = 0;
            fn_Time_Ctor_Float(&zeroTime, 0.0f);
            pOuterLoop = ::new (outerLoop) GameLoaderLoop(
                pDlLoop, *reinterpret_cast<const System::Time*>(&zeroTime));
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
    int r = reinterpret_cast<UI::UserInterface*>(ui)->ShowDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr));  // B.3.6 bridge
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
        ::Object::ExternalFactory::_singleton   // B.3.3 bridge
            ->FindOrLoad(filename));

    // IsDerivedFrom check — ensure it's a valid External (matches original _typeInfo_exref)
    void* checkedRes = nullptr;
    if (externalRes) {
        bool isExternal = fn_IsDerivedFrom(externalRes, *g_TypeId_Dialog);
        if (isExternal) checkedRes = externalRes;
    }

    // Allocate 0xC0 bytes for a DialogInstance shell
    void* dlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
    if (dlg) {
        // MessageDialog's real ctor (0x14006d300: DialogInstance(ui, externalRes, null
        // parent, 0, 0, 0), its own table, +0xb8 untouched) - the bytes the tree's
        // DialogInstance ctor door + MessageDialogAddr::Vtable stamp built (B38 B8-5).
        ::new (dlg) MessageDialog(reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10)),
                                  reinterpret_cast<const UI::Dialog*>(checkedRes),   // B.3.8 bridge
                                  Object::InternalPtr<UI::DialogInstance>());
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
        void* saveLoad = ::Save::SaveLoad::_singleton;
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(5), *(int*)(s + 0xC0));  // B.3.7 bridge
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
    void* saveLoad = ::Save::SaveLoad::_singleton;

    // Set minimum operation time (1.5f seconds — DAT_140172160)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetMinOperationTime(*reinterpret_cast<const System::Time*>(minTime));  // B.3.7 bridge

    // Check if the selected save slot already contains data
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
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
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveName(saveName, 2);  // B.3.7 bridge

    // Start save operation (4 = save, with selected slot index)
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(4), selectedIdx);  // B.3.7 bridge
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
    void* saveLoad = ::Save::SaveLoad::_singleton;

    // Set minimum operation time (1.5f seconds)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetMinOperationTime(*reinterpret_cast<const System::Time*>(minTime));  // B.3.7 bridge

    // If game world is active, ask user to confirm leaving the current game
    if (*g_pGameWorldSingleton != nullptr) {
        int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmLoad");
        if (result != 2) return;
    }

    // Iterate through mods in the selected save and warn about any incompatibilities
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
    int modCount = *(int*)((uintptr_t)saveInfo + 0x50);
    printf("[SaveLoadDialog::DoLoad] selectedIdx=%d saveInfo=%p save-mod-count=%d gameWorld=%p\n",
           selectedIdx, saveInfo, modCount,
           g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
    fflush(stdout);
    // The list CheckMod matches the save against is IO::Modification::_singleton.
    // Printed alongside so a non-zero CheckMod can be READ rather than guessed at.
    if (::IO::Modification::_singleton) {
        void* modSgl = ::IO::Modification::_singleton;
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
        int modStatus = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->CheckMod(selectedIdx, i);  // B.3.7 bridge
        {
            void* si = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
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
        void* saveInfo2 = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
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
            ::Object::ExternalFactory::_singleton   // B.3.3 bridge
                ->FindOrLoad(warnFilename));

        void* checkedWarn = nullptr;
        if (warnExt) {
            if (fn_IsDerivedFrom(warnExt, *g_TypeId_Dialog)) {
                checkedWarn = warnExt;
            }
        }

        // Allocate and construct the warning DialogInstance shell
        void* warnDlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        if (warnDlg) {
            // MessageDialog's real ctor (B38 B8-5; see SaveLoad_ShowConfirmDialog).
            ::new (warnDlg) MessageDialog(reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10)),
                                          reinterpret_cast<const UI::Dialog*>(checkedWarn),   // B.3.8 bridge
                                          Object::InternalPtr<UI::DialogInstance>());
        }

        // Find the TextElement inside the warning dialog and set mod info parameters
        // FindElementType on the dialog InternalPtr returns a TextElement InternalPtr
        uint64_t warnDlgPtr = 0;
        new (&warnDlgPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(warnDlg));   // B.3.3 bridge

        // Find TextElement within dialog (uses g_TypeId_TextElement)
        uint64_t textElemPtr = 0;
        ::new (&textElemPtr) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)warnDlgPtr)->FindElementType(*g_TypeId_TextElement));  // B.3.6 bridge

        // Check IsDerivedFrom TextElement
        void* rawElem = *(void**)&textElemPtr;
        bool isText = fn_IsDerivedFrom(rawElem, *g_TypeId_TextElement);
        void* textElem = isText ? rawElem : nullptr;

        uint64_t textPtr = 0;
        new (&textPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(textElem));   // B.3.3 bridge

        // SetParameter(0) = mod name, SetParameter(1) = mod name (again, as per decompilation)
        reinterpret_cast<UI::TextElement*>((void*)textPtr)->SetParameter(0, *reinterpret_cast<const System::String*>(modNameStr));  // B.3.6 bridge
        reinterpret_cast<UI::TextElement*>((void*)textPtr)->SetParameter(1, *reinterpret_cast<const System::String*>((void*)((uintptr_t)modNameStr + 8)));  // B.3.6 bridge

        // SetParameter(2) = formatted version string "%i (%s)"
        // saveInfo+0x14 = isBeta (uint, sub-field), saveInfo+0x18 = version int
        // Configuration::GetVersionString returns a const String*
        void* saveInfo3 = const_cast<Save::SaveLoad::SaveInfo*>(&reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetSaveInfo(selectedIdx));  // B.3.7 bridge
        void* modEntry = (void*)((uintptr_t)*(void**)((uintptr_t)saveInfo3 + 0x48) + (uintptr_t)i * 0x20);
        uint32_t isBeta   = *(uint32_t*)((uintptr_t)modEntry + 0x14);
        uint32_t modVer   = *(uint32_t*)((uintptr_t)modEntry + 0x18);
        System::String verStrBuf;
        fn_Configuration_GetVersionString(&verStrBuf, isBeta, (bool)(isBeta != 0));
        System::String fmtBuf = System::String::Format("%i (%s)", (unsigned)modVer, verStrBuf._data);
        reinterpret_cast<UI::TextElement*>((void*)textPtr)->SetParameter(2, fmtBuf);  // B.3.6 bridge

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
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(2), selectedIdx);  // B.3.7 bridge
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
        reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->Play(4);  // B.3.6 bridge
    }
    *(int*)(s + 0xC0) = index;
}

void SaveLoadDialog::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t eventType, int32_t eventData, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    uintptr_t s = (uintptr_t)this;

    if (eventType == 2) {
        // List row selection change
        int saveCount = *(int*)(s + 0xC4);
        if (eventData > 3 && eventData < saveCount + 4) {
            SaveLoadDialog_SelectEntry(this, eventData - 4);
        }
        return;
    }

    if (eventType == 0x12) {
        if (eventData == 0) {
            // buttonCancel: close dialog
            void* ui = *(void**)(s + 0x10);
            uint64_t basePtr[2];
            new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(basePtr));  // B.3.6 bridge
            return;
        }
        if (eventData == 2) {
            SaveLoadDialog_DoLoad(this);
            return;
        }
        if (eventData == 1) {
            SaveLoadDialog_DoSave(this);
            return;
        }
        if (eventData == 3) {
            SaveLoadDialog_DoDelete(this);
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

// ENDGAME B.3.8 block DLG: SaveLoadDialog is a real class (Dialog/SaveLoadDialog.h);
// the hand-built table is gone. engine_funcs.cpp does not read 0x14015f900 back,
// so its registration went with the table; the ctor door stays.

// The ctor body of exe 0x1400b8270 after the DialogInstance base ctor: mode
// byte at +0xB8, flags/selection/rotation zeroed, Sprite default-ctor at +0xD0,
// loader/engine ptrs at +0xE0/+0xE8, +0xF0..+0x158 zero.
void SaveLoadDialog::InitFields(uint8_t modeArg, void* engineArg, void* gameWorldArg) {
    uintptr_t s = (uintptr_t)this;
    mode = modeArg;
    *(uint32_t*)(s + 0xB9) = 0;          // saveFlag/loadFlag/refreshFlag/deleteFlag
    *(uint32_t*)(s + 0xC0) = 0;          // selectedIndex
    *(uint32_t*)(s + 0xC8) = 0;          // spinner rotation
    ::new ((void*)(s + 0xD0)) UI::Sprite();
    engine = engineArg;
    gameWorld = gameWorldArg;
    for (uintptr_t off = 0xF0; off <= 0x158; off += 8)
        *(uint64_t*)(s + off) = 0;
}

SaveLoadDialog::SaveLoadDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                               Object::InternalPtr<UI::DialogInstance> parent,
                               uint8_t modeArg, void* engineArg, void* gameWorldArg)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0) {
    InitFields(modeArg, engineArg, gameWorldArg);
}

// The reflected create path (create 0x140021160): the tree's factory handed out
// a zero-filled object; this keeps the zero fields over the default base ctor.
SaveLoadDialog::SaveLoadDialog() {
    InitFields(0, nullptr, nullptr);
    *(uint32_t*)((uintptr_t)this + 0xC4) = 0;   // saveCount
}

// [0] 0x1400211b0: ~SaveLoadDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* SaveLoadDialog::DeletingDtor(unsigned flags) {
    this->~SaveLoadDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140021160: MemAlloc(0x160, 8), then the ctor.
void* SaveLoadDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SaveLoadDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) SaveLoadDialog());
}
// createInPlace 0x1400211a0 (the tree's in-place build is kept, B8-13).
void* SaveLoadDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) SaveLoadDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4).
DEFINE_EXE_VTABLE(SaveLoadDialog, "_ZTV14SaveLoadDialog")

// The ctor DOOR, reached by address: fn_SaveLoadDialog_Ctor (engine_funcs.cpp)
// resolves 0x1400b8270. Retail's ctor caller path lands in OnOpened via the UI
// dialog-open machinery; the +0xBB refreshFlag=1 kick that triggers the initial
// file-list scan happens there.
extern "C" void* SaveLoadDialog_Ctor_8270(void* self, void* ui, void* extRes,
                                          void* parentIBP, uint8_t mode,
                                          void* enginePtr, void* gameWorldPtr) {
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) SaveLoadDialog(static_cast<UI::UserInterface*>(ui),
                                       static_cast<const UI::Dialog*>(extRes), parent,
                                       mode, enginePtr, gameWorldPtr);
}

void RegisterSaveLoadDialog() {
    RegisterExeAddr(0x1400b8270, reinterpret_cast<uintptr_t>(&SaveLoadDialog_Ctor_8270));
}
