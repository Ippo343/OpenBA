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

// ENDGAME B.3.5 batch K1: the runtime type headers come FIRST — they reach
// runtime/types/volume/Volume.h, whose global-scope `struct Rectangle` is hidden by
// <wingdi.h>'s `Rectangle()` once <windows.h> has been seen through resolve.h.
#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "video/ImageBuffer.h"
#include "video/Device.h"
#include "graphics/Material.h"
#include "ui/Dialogs.h"
#include "options.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Modification.h"   // ENDGAME B.3.4 caller batch P
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Thumbnail vtable stamps — see code/Game/Dialog/thumb_vtables.cpp.
extern uintptr_t ThumbVtable_StampMaterialInstance(void* obj);
extern uintptr_t ThumbVtable_StampSpriteSheet(void* obj);

// ============================================================
// Helper types
// ============================================================

// Vtable call helpers for Element (via raw vtable pointer reads)
typedef void* (*fn_ElementVt_Child_t)(void* self, void* retbuf, int index);
typedef void  (*fn_ElementVt_Hide_t)(void* self, int hide);

// ENDGAME B.3.5 batch K1: the hand-built `fn_ImageBuffer_Create_t` slot typedef for
// vtable+0x48 went with the slot read below, which is now a virtual call on
// `Video::ImageBuffer`. Keeping the history the typedef carried, because the member's
// signature is what makes it safe: the LAST parameter of
//   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@
//   W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
// is a POINTER (`Array<Array<uchar>>* levels`) living in a stack slot, and passing it as
// `int 0` wrote only the low dword — leaving the high dword as whatever was there
// (0x00007ffc... on the first Mods click), which ImageBufferData_Create then
// dereferenced as `levels->count`. The C++ member types it, so that cannot recur.

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

    int modCount = reinterpret_cast<const IO::Modification*>(modSingleton)->GetCount();   // B.3.4 bridge
    if (modCount <= 0) return;

    uintptr_t entriesBase = *(uintptr_t*)(s + 0xB8);  // mod entry array

    for (int i = 0; i < modCount; i++) {
        uintptr_t entry = entriesBase + (uintptr_t)i * 0x58;

        // Get mod data
        void* mod = const_cast<RtIO::Mod*>(
            &reinterpret_cast<const IO::Modification*>(modSingleton)->GetModification(i));   // B.3.4 bridge
        if (!mod) continue;

        uintptr_t m = (uintptr_t)mod;

        // Check for conflicts: another enabled mod depends on a file from this mod
        bool hasConflict = false;
        uint8_t enabled = *(uint8_t*)(m + 0x50);  // mod->m_enabled

        if (enabled != 0) {
            for (int j = 0; j < modCount; j++) {
                if (j == i) continue;

                void* otherMod = const_cast<RtIO::Mod*>(
                    &reinterpret_cast<const IO::Modification*>(modSingleton)->GetModification(j));   // B.3.4 bridge
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
        reinterpret_cast<UI::TextElement*>(*(void**)(entry + 0x00))->SetParameter(0, *reinterpret_cast<const System::String*>((void*)m));  // B.3.6 bridge; mod->m_name

        reinterpret_cast<UI::TextElement*>(*(void**)(entry + 0x08))->SetParameter(0, *reinterpret_cast<const System::String*>((void*)(m + 0x08)));  // B.3.6 bridge; mod->m_author

        reinterpret_cast<UI::TextElement*>(*(void**)(entry + 0x18))->SetParameter(0, *reinterpret_cast<const System::String*>((void*)(m + 0x10)));  // B.3.6 bridge; mod->m_description

        // Format version string: "versionMinor (versionString)"
        {
            uint32_t versionMajor = *(uint32_t*)(m + 0x18);
            uint32_t versionMinor = *(uint32_t*)(m + 0x1C);

            // GetVersionString returns a String for the version major (as game build version)
            System::String buildStr;
            fn_Configuration_GetVersionString(&buildStr, versionMajor, false);

            System::String formatted = System::String::Format("%i (%s)", (uint64_t)versionMinor, buildStr._data);

            reinterpret_cast<UI::TextElement*>(*(void**)(entry + 0x10))->SetParameter(0, formatted);  // B.3.6 bridge
        }

        // Set enabled checkbox state: entry+0x28 = checkLoaded, element +0xD8 = enabled byte
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
        {
            void* elem = *(void**)(entry + 0x30);
            reinterpret_cast<UI::Element*>(elem)->SetColor(rowColor);  // B.3.6 bridge
        }

        // Show/hide conflict icon (entry+0x20 = imageConflict)
        reinterpret_cast<UI::Element*>(*(void**)(entry + 0x20))->SetFlagsOnOff(  // B.3.6 bridge
                                  hasConflict ? 0 : 1,    // set bit 0 (hidden) when no conflict
                                  hasConflict ? 1 : 0);   // clear bit 0 (visible) when conflict

        // Copy thumbnail texture (48x48)
        {
            void* videoDevice = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
            if (videoDevice) {
                reinterpret_cast<Video::Device*>(videoDevice)->Lock();   // B.3.5 bridge
                void* imageBuffer = *(void**)(entry + 0x40);
                void* thumbnailData = *(void**)(m + 0x30);  // mod->m_intArrayData (thumbnail pixels)
                reinterpret_cast<Video::ImageBuffer*>(imageBuffer)
                    ->CopyToTexture(thumbnailData, 0x30, 0x30);          // B.3.5 bridge
                reinterpret_cast<Video::Device*>(videoDevice)->Unlock(); // B.3.5 bridge
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
        System::String retbuf("buttonCancel");
        uint8_t elemBuf[8];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(retbuf));  // B.3.6 bridge
        void* elem = *(void**)elemBuf;
        reinterpret_cast<UI::Element*>(elem)->SetElementId(100);  // B.3.6 bridge
        *(uint32_t*)(*(uintptr_t*)elemBuf + 0xC0) = 8;
    }

    // --- buttonOk (id=0x65) ---
    uint8_t buttonOkBuf[8] = {};
    {
        System::String retbuf("buttonOk");
        uint8_t elemBuf[8];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(retbuf));  // B.3.6 bridge
        void* elem = *(void**)elemBuf;
        reinterpret_cast<UI::Element*>(elem)->SetElementId(0x65);  // B.3.6 bridge
        *(uint64_t*)buttonOkBuf = *(uint64_t*)elemBuf;
    }
    (void)buttonOkBuf;

    // --- tableMods: find and store at self+0xC8 ---
    {
        System::String retbuf("tableMods");
        uint8_t elemBuf[8];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(retbuf));  // B.3.6 bridge
        void* elem = *(void**)elemBuf;
        // Store InternalPtr at +0xC8
        new ((void*)(s + 0xC8)) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
    }

    void* modSingleton = *(void**)g_ppModificationSingleton;
    int modCount = reinterpret_cast<const IO::Modification*>(modSingleton)->GetCount();   // B.3.4 bridge
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;

    for (int i = 0; i < modCount; i++) {
        // Clone template element from Dialog, add to tableMods
        uint8_t templateBuf[8];
        void* dialog = *(void**)(s + 0x18);
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(dialog)->GetTemplate(0));  // B.3.6 bridge
        void* templateObj = *(void**)templateBuf;

        // Call vtable[8] (offset 0x40) to clone: (templateObj, retbuf, dialog)
        // (= UI::ElementDescription slot 8 CreateInstance, RETSLOT per B6-2)
        uint8_t cloneBuf[8];
        reinterpret_cast<const UI::ElementDescription*>(templateObj)->CreateInstance(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(dialog));
        void* clonedElem = *(void**)cloneBuf;

        // Add cloned element to tableMods
        void* tableElem = *(void**)(s + 0xC8);

        reinterpret_cast<UI::Element*>(tableElem)->AddChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(&clonedElem));  // B.3.6 bridge

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
            System::String strBuf(e.name);
            uint8_t retBuf[8];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(clonedElem)->FindElement(strBuf));  // B.3.6 bridge
            void* found = *(void**)retBuf;
            new (e.buf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
        }

        // Get vtable[0x58] child (Child(0)) of cloned element
        uint8_t vtChildBuf[8] = {};
        {
            // vtable[0x58/8] = vtable[11]. The old comment here called it
            // Element::Child(int); slot 11 of every UI::Element table is
            // GetBorder (RETSLOT, B6-2), which takes no index -- the `0` the old
            // call passed was never read.
            uint8_t childRetBuf[8];
            reinterpret_cast<UI::Element*>(clonedElem)->GetBorder(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(childRetBuf));
            void* child = *(void**)childRetBuf;
            new (vtChildBuf) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(child));   // B.3.3 bridge
        }

        // Assign element IDs
        reinterpret_cast<UI::Element*>(*(void**)checkLoadedBuf)->SetElementId(i + 1000);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)buttonTopBuf)->SetElementId(i + 4000);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)buttonDownBuf)->SetElementId(i + 3000);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)buttonUpBuf)->SetElementId(i + 2000);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(void**)buttonDetailsBuf)->SetElementId(i + 5000);  // B.3.6 bridge

        // Hide Up+Top for first mod (vtable[8], arg=0 matches original). Slot 8 of
        // every UI::Element table is Enable(bool), not Hide (the old comment's
        // name), so what this does is DISABLE the buttons (B36_RULES 2.2).
        if (i == 0) {
            reinterpret_cast<UI::Element*>(*(void**)buttonUpBuf)->Enable(0);  // B.3.6 bridge
            reinterpret_cast<UI::Element*>(*(void**)buttonTopBuf)->Enable(0);  // B.3.6 bridge
        }
        // Hide Down for last mod
        if (i == modCount - 1) {
            reinterpret_cast<UI::Element*>(*(void**)buttonDownBuf)->Enable(0);  // B.3.6 bridge
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
            // B.3.5 bridge — 0x48/8 == slot 9 of `Video::ImageBuffer`, which IS `Create`
            // (B35_RULES 2.3 item 5). PROVED receiver: 0x68 bytes (== sizeof) from
            // MemAlloc, run through `??0ImageBuffer@Video@@`, then stamped with the exe's
            // own NoiseUI ImageBuffer vtable — so the virtual call reads that live table at
            // exactly the +0x48 the slot read did.
            reinterpret_cast<Video::ImageBuffer*>(imgBuf)->Create(
                0, 2, 5, 0u, 0x30, 0x30, 1, 1, 1, 1, nullptr);   // B.3.5 bridge
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
            System::Filename filenameBuf("Material/UI/UIMaterial.rsc");
            void* extFactory = *(void**)g_pExternalFactorySingleton;
            void* ext = const_cast<::Object::External*>(
                reinterpret_cast<::Object::ExternalFactory*>(extFactory)   // B.3.3 bridge
                    ->FindOrLoad(filenameBuf));
            void* material = nullptr;
            if (ext) {
                bool derived = fn_IsDerivedFrom(ext, g_TypeId_Material ? *g_TypeId_Material : 0);
                if (derived) material = ext;
            }
            reinterpret_cast<Graphics::MaterialInstance*>(matInst)->SetMaterial(
                Object::ExternalPtr<Graphics::Material>(
                    reinterpret_cast<Graphics::Material*>(material)));   // B.3.5 bridge
        }

        // AddTexture("diffuse", imageBuffer)
        {
            System::String strBuf("diffuse");
            reinterpret_cast<Graphics::MaterialInstance*>(matInst)->AddTexture(
                *reinterpret_cast<const RtSystem::String*>(&strBuf),
                Object::ExternalPtr<Video::ImageBuffer>(
                    reinterpret_cast<Video::ImageBuffer*>(imgBuf)));   // B.3.5 bridge
        }

        reinterpret_cast<Graphics::MaterialInstance*>(matInst)
            ->BuildParameters();   // B.3.5 bridge

        // --- Create SpriteSheet (0x58 bytes) ---
        void* sprSheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
        entry.spriteSheet = nullptr;
        if (sprSheet) {
            fn_SpriteSheet_Ctor(sprSheet);
            ThumbVtable_StampSpriteSheet(sprSheet);
            entry.spriteSheet = sprSheet;

            // Set materialInstance at +0x30
            *(void**)((uintptr_t)sprSheet + 0x30) = matInst;

            // operator= empty strings at +0x38 and +0x40 (raw SpriteSheet fields; not our struct)
            System::String emptyStr1, emptyStr2;
            *reinterpret_cast<System::String*>((uintptr_t)sprSheet + 0x40) = emptyStr1;  // B.3.1 bridge
            *reinterpret_cast<System::String*>((uintptr_t)sprSheet + 0x38) = emptyStr2;  // B.3.1 bridge

            // Set w/h at +0x48/+0x4c to 48 (0x30)
            *(uint32_t*)((uintptr_t)sprSheet + 0x48) = 0x30;
            *(uint32_t*)((uintptr_t)sprSheet + 0x4c) = 0x30;
            // Zero +0x50
            *(uint64_t*)((uintptr_t)sprSheet + 0x50) = 0;

            // AddSpriteInfo(sheet, empty, empty, 0, 0, 48, 48)
            uint64_t emptyA = 0, emptyB = 0;
            reinterpret_cast<UI::SpriteSheet*>(sprSheet)->AddSpriteInfo(  // B.3.6 bridge
                *reinterpret_cast<const System::String*>(&emptyA),
                *reinterpret_cast<const System::String*>(&emptyB), 0, 0, 0x30, 0x30);

            // SetSpriteInfo(sheet, 0, 48, 48, 0.0, 0.0, uvScale, uvScale)
            reinterpret_cast<UI::SpriteSheet*>(sprSheet)->SetSpriteInfo(  // B.3.6 bridge
                0, 0x30, 0x30, 0.0f, 0.0f, uvScale, uvScale);
        }

        // Create Sprite by index and SetSprite on imageIcon
        {
            uint8_t spriteBuf[16];
            ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(sprSheet)), 0);  // B.3.6 bridge
            void* imageIconElem = *(void**)imageIconBuf;
            reinterpret_cast<UI::ImageElement*>(imageIconElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge
        }

        // Push entry into self->m_entries (array at +0xB8, count at +0xC0)
        int newCount = *(int*)(s + 0xC0) + 1;
        fn_ModDialog_ArrayPush((void*)(s + 0xB8), newCount, &entry);
    }

    // --- Post-loop: refresh display, add browser controller, hide browse UI ---
    ModDialog_RefreshDisplay_Impl(self);

    // Load "Dialog/Mod.rsc:browser", add as controller with type g_TypeId_BrowserController
    {
        System::Filename filenameBuf("Dialog/Mod.rsc:browser");
        void* extFactory = *(void**)g_pExternalFactorySingleton;
        void* ext = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(extFactory)   // B.3.3 bridge
                ->FindOrLoad(filenameBuf));

        // Pass browser external directly; original has IsDerivedFrom check but the
        // _typeInfo_exref is context-dependent (Ghidra merges refs) and the browser
        // controller IS the right type — passing null would silently break the UI.
        void* browserExt = ext;

        // AddController(self, typeId, 0, 0, browserExt, "groupBrowse")
        System::String groupStr("groupBrowse");
        uint32_t typeId = g_TypeId_BrowserController ? *g_TypeId_BrowserController : 0;
        reinterpret_cast<UI::DialogInstance*>(self)->AddController(  // B.3.6 bridge
            typeId, static_cast<Object::Base*>(nullptr),
            Object::ExternalPtr<UI::DialogControllerConfig>(nullptr),
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(browserExt)),
            groupStr);
    }

    // Hide "buttonBrowse"
    {
        System::String strBuf("buttonBrowse");
        uint8_t elemBuf[8];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(strBuf));  // B.3.6 bridge
        void* elem = *(void**)elemBuf;
        reinterpret_cast<UI::Element*>(elem)->Hide(true);  // B.3.6 bridge
    }

    // Hide "groupProgress"
    {
        System::String strBuf("groupProgress");
        uint8_t elemBuf[8];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(strBuf));  // B.3.6 bridge
        void* elem = *(void**)elemBuf;
        reinterpret_cast<UI::Element*>(elem)->Hide(true);  // B.3.6 bridge
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
    uintptr_t checkElem = *(uintptr_t*)(entry + 0x28);
    reinterpret_cast<IO::Modification*>(modSingleton)
        ->Enable(index, *(uint8_t*)(checkElem + 0xD8) != 0);   // B.3.4 bridge

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
        reinterpret_cast<IO::Modification*>(modSingleton)->Swap(index, index - 1);   // B.3.4 bridge
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
    System::Filename filenameBuf("Dialog/ModDetails.rsc");
    void* extFactory = *(void**)g_pExternalFactorySingleton;
    void* ext = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(extFactory)   // B.3.3 bridge
            ->FindOrLoad(filenameBuf));
    // IsDerivedFrom check — resource is always valid Dialog if load succeeds
    void* dialog = ext;

    // Allocate ModDetailsDialog (0xC8 = 200 bytes)
    void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* dialogInst = nullptr;
    if (newDialog) {
        // Get sprite handle from entry+0x50 (SpriteSheet ptr)
        uintptr_t entry = *(uintptr_t*)(s + 0xB8) + (uintptr_t)index * 0x58;
        uint64_t spriteHandle = *(uint64_t*)(entry + 0x50);

        // Construct DialogInstance base (0xB8)
        uint64_t nullRef;
        new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
    new (&dialogRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dialogInst));   // B.3.3 bridge
    reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));  // B.3.6 bridge
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
        System::Filename filenameBuf(gameWorld == nullptr ? "Dialog/Reload.rsc:menu"
                                                            : "Dialog/Reload.rsc:game");
        void* extFactory = *(void**)g_pExternalFactorySingleton;
        void* ext = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(extFactory)   // B.3.3 bridge
                ->FindOrLoad(filenameBuf));
        void* dialog = ext;  // IsDerivedFrom check always passes for valid .rsc

        // Create ReloadDialog (0xC0 bytes)
        void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        void* dialogInst = nullptr;
        if (newDialog) {
            uint64_t nullRef;
            new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            fn_DialogInstance_Ctor(newDialog, *(void**)(s + 0x10), dialog, &nullRef, 0, 0, 0);
            if (g_ReloadDialog_Vtable) {
                *(uintptr_t*)newDialog = g_ReloadDialog_Vtable;
            }
            dialogInst = newDialog;
        }

        // ShowDialog (modal) — blocks until user responds
        uint64_t dialogRef;
        new (&dialogRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(dialogInst));   // B.3.3 bridge
        // BANISHED_MODAL_ANSWER=<n>: unattended harness only. ShowDialog pumps its
        // own message loop until a button is pressed, which no driver can reach, so
        // the answer is injected here instead (1/2 = confirm, anything else = cancel).
        int result;
        if (Opt().modalAnswer) {
            result = Opt().modalAnswerValue;
            printf("[ModDialog::Apply] BANISHED_MODAL_ANSWER=%d (reload dialog skipped)\n", result);
            reinterpret_cast< ::Object::InternalBasePtr*>(&dialogRef)->DeletePtr();   // B.3.3 bridge
        } else {
            result = reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->ShowDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));  // B.3.6 bridge
            reinterpret_cast< ::Object::InternalBasePtr*>(&dialogRef)->DeletePtr();   // B.3.3 bridge
        }
        printf("[ModDialog::Apply] dirty=1 gameWorld=%p reload answer=%d\n", gameWorld, result);

        if ((unsigned)(result - 1) < 2) {
            // User confirmed: save and reload
            reinterpret_cast<IO::Modification*>(modSingleton)->SaveList();   // B.3.4 bridge
            fn_ModDialog_CloseAndReload((void*)self);

            if (gameWorld == nullptr) {
                // Menu context: simple reload via +0xD0 controller
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
                new (&nullRef2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

                // Trigger game reload with saved state
                fn_Game_ReloadWithSave(*(void**)(s + 0xD0), statePtr, (void*)(uintptr_t)-1, &nullRef2);
            }
        } else {
            // User cancelled: refresh list to undo changes
            reinterpret_cast<IO::Modification*>(modSingleton)->RefreshList();   // B.3.4 bridge
        }
    }

    // Close the mod dialog
    uint64_t closeRef;
    new (&closeRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
    reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&closeRef));  // B.3.6 bridge
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
            reinterpret_cast<IO::Modification*>(modSingleton)->RefreshList();   // B.3.4 bridge

            // Build InternalBasePtr for CloseDialog
            void* dialogPtr = nullptr;
            new (&dialogPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge

            void* ui = *(void**)(s + 0x10);
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogPtr));  // B.3.6 bridge
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
            reinterpret_cast<IO::Modification*>(modSingleton)->Swap(id - 2000, id - 2001);   // B.3.4 bridge
            ModDialog_RefreshDisplay_Impl(self);
            goto done;
        }

        if (id >= 3000 && id < 4000) {
            // Move down: swap(id-3000, id-2999)
            reinterpret_cast<IO::Modification*>(modSingleton)->Swap(id - 3000, id - 2999);   // B.3.4 bridge
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
    // QUALIFIED (B36_RULES 3.2): an unqualified call would dispatch through
    // ModDialog's hand-built table straight back into this slot body.
    reinterpret_cast<UI::DialogInstance*>((void*)self)->UI::DialogInstance::OnElementEvent(  // B.3.6 bridge
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(&elementPtr),
        msg, id, p5, p6, p7, reinterpret_cast<void*>(p8));
}
