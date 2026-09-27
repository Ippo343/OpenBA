// code/Game/Dialog/noiseui.cpp
// Clean reimplementation of NoiseUI panel methods.
//
// NoiseUI (0x70 bytes): Noise/terrain visualization debug panel.
// Extends DialogController (0x30 base).
//
// Layout:
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x30 = imageBuffer ptr (Video::ImageBuffer*)
//   +0x38 = materialInstance ptr (void*)
//   +0x40 = spriteSheet ptr (void*)
//   +0x48 = noisePersistance element (InternalBasePtr)
//   +0x50 = noiseOctaves element (InternalBasePtr)
//   +0x58 = noiseScale element (InternalBasePtr)
//   +0x60 = noiseCutOff element (InternalBasePtr)
//   +0x68 = noiseGradient element (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
// ENDGAME B.3.5 batch K1: the runtime type headers come FIRST — they reach
// runtime/types/volume/Volume.h, whose global-scope `struct Rectangle` is hidden by
// <wingdi.h>'s `Rectangle()` once <windows.h> has been seen through resolve.h.
#include "video/ImageBuffer.h"
#include "graphics/Material.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Random
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ImageElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)


// Helper: find element by name in parent element, cast to InternalBasePtr
static uint64_t NoiseUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    uint8_t retBuf[16], bpBuf[8];

    System::String nameStr(name);

    // fn_FindElement: member function, RCX=parentElem, RDX=retBuf, R8=nameStr
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge

    // Check the InternalBasePtr result
    void* raw = *(void**)retBuf;

    // Cast to specific type via IsDerivedFrom check
    // (Ghidra shows the pattern: CheckPtr -> deref -> IsDerivedFrom -> InternalBasePtr_Ctor)
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(raw));   // B.3.3 bridge
    uint64_t result = *(uint64_t*)bpBuf;

    return result;
}

// ============================================================
// SetupImage (0x140071890): Create MaterialInstance + SpriteSheet
// for the noise preview, hardcoded to "UIMaterialNoBlend.rsc"
// and "noiseImage" element name.
// ============================================================
// Nearly identical to MapUI_SetupImage but without blend option.
// Allocates MaterialInstance (0x60) and SpriteSheet (0x58),
// loads UIMaterialNoBlend material, adds ImageBuffer as "diffuse",
// builds parameters, configures sprite info, finds "noiseImage"
// element and sets the sprite.
//
// Original: FUN_140071890
void NoiseUI_SetupImage(void* self, void* imageBuffer,
                        void** spriteSheetOut, void** materialOut) {
    uintptr_t s = (uintptr_t)self;

    // --- Allocate and construct MaterialInstance (0x60 bytes) ---
    void* matInst = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    void* materialPtr = nullptr;
    if (matInst) {
        ::new (matInst) Graphics::MaterialInstance();   // B.3.9 EXA: the real class (exe table 0x14015c400 is its own)
        materialPtr = matInst;
    }
    *materialOut = materialPtr;

    // --- Load UIMaterialNoBlend resource ---
    // Filename's ctor already normalises delimiters, so the separate
    // AdjustDelimiters call retail made on the raw buffer is redundant here.
    System::Filename filename("Material/UI/UIMaterialNoBlend.rsc");

    void* ext = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton   // B.3.3 bridge
            ->FindOrLoad(filename));
    void* material = nullptr;
    if (ext) {
        if (fn_IsDerivedFrom(ext, Graphics::Material::TypeId())) {
            material = ext;
        }
    }
    reinterpret_cast<Graphics::MaterialInstance*>(*materialOut)->SetMaterial(
        Object::ExternalPtr<Graphics::Material>(
            reinterpret_cast<Graphics::Material*>(material)));   // B.3.5 bridge

    // --- Add ImageBuffer as "diffuse" texture ---
    System::String diffuseStr("diffuse");
    reinterpret_cast<Graphics::MaterialInstance*>(*materialOut)->AddTexture(
        *reinterpret_cast<const RtSystem::String*>(&diffuseStr),
        Object::ExternalPtr<Video::ImageBuffer>(
            reinterpret_cast<Video::ImageBuffer*>(imageBuffer)));   // B.3.5 bridge

    reinterpret_cast<Graphics::MaterialInstance*>(*materialOut)
        ->BuildParameters();   // B.3.5 bridge

    // --- Allocate and construct SpriteSheet (0x58 bytes) ---
    void* sheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    void* spriteSheet = nullptr;
    if (sheet) {
        ::new (sheet) UI::SpriteSheet();   // B.3.9 EXA: the real class (exe table 0x14015c3b0 is its own)
        spriteSheet = sheet;
    }
    *spriteSheetOut = spriteSheet;

    // Set material on sprite sheet at +0x30
    *(void**)((uintptr_t)spriteSheet + 0x30) = *materialOut;

    // Get image dimensions
    int imgW = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Width();    // B.3.5 bridge
    int imgH = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Height();   // B.3.5 bridge

    // Set sprite sheet name/group strings (empty) and dimensions
    uintptr_t ss = (uintptr_t)*spriteSheetOut;
    System::String emptyStr1;
    System::String emptyStr2;
    *reinterpret_cast<System::String*>(ss + 0x40) = emptyStr1;  // B.3.1 bridge
    *reinterpret_cast<System::String*>(ss + 0x38) = emptyStr2;  // B.3.1 bridge
    *(int*)(ss + 0x48) = imgW;
    *(int*)(ss + 0x4c) = imgH;
    *(uint8_t*)(ss + 0x50) = 0;

    // Add sprite info: empty name, empty group, full image rect
    imgW = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Width();    // B.3.5 bridge
    imgH = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Height();   // B.3.5 bridge
    System::String nameEmpty;
    System::String groupEmpty;
    reinterpret_cast<UI::SpriteSheet*>(*spriteSheetOut)->AddSpriteInfo(nameEmpty, groupEmpty, 0, 0, imgW, imgH);  // B.3.6 bridge

    // Set sprite info with UV scale
    imgW = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Width();    // B.3.5 bridge
    imgH = reinterpret_cast<const Video::ImageBuffer*>(imageBuffer)->Height();   // B.3.5 bridge
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    reinterpret_cast<UI::SpriteSheet*>(*spriteSheetOut)->SetSpriteInfo(0, imgW, imgH, 0.0f, 0.0f, uvScale, uvScale);  // B.3.6 bridge

    // --- Find "noiseImage" element and set sprite ---
    System::String elemNameStr("noiseImage");

    void* dialogElem = *(void**)(s + 0x20);
    uint64_t foundElem = 0;
    ::new (&foundElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(elemNameStr));  // B.3.6 bridge

    void* rawElem = (void*)foundElem;

    void* imageElem = nullptr;
    if (rawElem && fn_IsDerivedFrom(rawElem, UI::ImageElement::TypeId())) {
        imageElem = rawElem;
    }
    uint64_t imageElemPtr = 0;
    new ((void*)&imageElemPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(imageElem));   // B.3.3 bridge

    // Create Sprite from SpriteSheet (index 0) and set on element
    uint8_t spriteBuf[16];
    ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*spriteSheetOut)), 0);  // B.3.6 bridge
    reinterpret_cast<UI::ImageElement*>(imageElemPtr)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge
}

// ============================================================
// GenerateNoise (0x1400711D0): Generate Perlin noise texture
// ============================================================
// Reads 5 slider values (persistance, octaves, scale, cutOff, gradient),
// creates a PerlinNoise generator with random seeds, renders a 256x256
// grayscale texture, and copies it to the ImageBuffer.
//
// PerlinNoise struct (0x10 bytes):
//   +0x00: float persistance
//   +0x04: float octaves
//   +0x08: int seed1
//   +0x0C: int seed2
//
// Original: FUN_1400711D0

// PerlinNoise and Random function pointers are resolved globally in
// engine_funcs.cpp (fn_PerlinNoise_Ctor, fn_PerlinNoise_Generate,
// fn_Random_Default_Ctor, fn_Random_RandTol). Float parsing now goes through
// System::String::ToFloat directly.

// Helper: read float value from a slider element's text (+0xC8)
static float NoiseUI_ReadSliderFloat(uintptr_t sliderPtr) {
    uintptr_t elem = *(uintptr_t*)&sliderPtr;
    const char* text = *(const char**)(elem + 0xC8);
    return System::String::ToFloat(text);
}

void NoiseUI_GenerateNoise(void* self) {
    if (!fn_PerlinNoise_Ctor || !fn_PerlinNoise_Generate ||
        !fn_Random_Default_Ctor)
        return;

    uintptr_t s = (uintptr_t)self;

    // Read slider values
    float persistance = NoiseUI_ReadSliderFloat(*(uint64_t*)(s + 0x48));
    float octaves     = NoiseUI_ReadSliderFloat(*(uint64_t*)(s + 0x50));
    float scale       = NoiseUI_ReadSliderFloat(*(uint64_t*)(s + 0x58));
    float cutOff      = NoiseUI_ReadSliderFloat(*(uint64_t*)(s + 0x60));
    float gradient    = NoiseUI_ReadSliderFloat(*(uint64_t*)(s + 0x68));

    // Compute inverse scale (uvScale / scale)
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    float invScale = uvScale / scale;

    // Create Random with time-based seed, get two random seeds
    // Random is ~0xA60 bytes (Mersenne Twister state)
    uint8_t randomBuf[0xA80];
    fn_Random_Default_Ctor(randomBuf);
    int seed1 = reinterpret_cast<System::Random*>(randomBuf)->RandTol(static_cast<int32_t>(0x7ffffff));   // B.3.4 bridge
    int seed2 = reinterpret_cast<System::Random*>(randomBuf)->RandTol(static_cast<int32_t>(0x7ffffff));   // B.3.4 bridge

    // Create PerlinNoise (0x10 bytes)
    uint8_t noiseBuf[0x10];
    fn_PerlinNoise_Ctor(noiseBuf, persistance, octaves, seed2, seed1);

    // Allocate 256*256 pixel buffer
    uint32_t* pixels = (uint32_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x40000, 4);

    // Generate noise texture
    // Constants from exe .rdata:
    //   0.5f  (DAT_140172114) — noise bias (center around 0)
    //   255.0f (DAT_14017222c) — gradient divisor
    float halfVal = 0.5f;
    float gradientScale = 255.0f / gradient;

    uint32_t* pix = pixels;
    for (int y = 0; y < 0x100; y++) {
        for (int x = 0; x < 0x100; x++) {
            // Generate noise value at scaled position
            float noiseVal = fn_PerlinNoise_Generate(noiseBuf,
                                                      (float)x * invScale,
                                                      (float)y * invScale);

            // Map from [-1,1] to [0,1], subtract cutoff, clamp
            float val = (noiseVal * halfVal + halfVal) - cutOff;
            if (val < 0.0f) val = 0.0f;

            // Normalize to [0,1] range based on (1 - cutOff)
            val = val / (uvScale - cutOff);

            // Clamp to [0, 1]
            if (val > uvScale) val = uvScale;
            else if (val < 0.0f) val = 0.0f;

            // Quantize to gradient steps and back
            uint32_t gray = (uint32_t)(int64_t)((float)(int)(val * gradient) * gradientScale);
            if (gray > 0xFF) gray = 0xFF;

            // Pack as ARGB (opaque gray)
            *pix = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
            pix++;
        }
    }

    // Copy to ImageBuffer texture
    reinterpret_cast<Video::ImageBuffer*>(*(void**)(s + 0x30))
        ->CopyToTexture(pixels, 0x100, 0x100);   // B.3.5 bridge
    fn_MemFreeStatic(pixels);
}

// ============================================================
// NoiseUI::Slot4 (vtable +0x20) — Setup noise visualization
// ============================================================
// Original: 0x1400714C0
//
// Allocates a 256x256 ImageBuffer, fills with black, creates
// MaterialInstance + SpriteSheet via helper, finds 5 slider
// elements and the "buttonGenerate" button (ID 1000).

void NoiseUI_Slot4(NoiseUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Allocate ImageBuffer (0x68 bytes)
    void* imgBuf = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    void* imageBuffer = nullptr;
    if (imgBuf != nullptr) {
        ::new (imgBuf) Video::ImageBuffer();   // B.3.9 EXA: the real class (exe table 0x14015c8c0 is its own)
        imageBuffer = imgBuf;
    }
    *(void**)(s + 0x30) = imageBuffer;

    // Call ImageBuffer::Create virtual (vtable +0x48)
    // Signature: Create(ImageType=0, Usage=2, Format=5, mip=0, width=256,
    //                    height=256, FilterMode=1, AddressMode=1,1,1,
    //                    Array<Array<uchar>>* levels=null)
    // The last parameter is a POINTER in a stack slot -- passing it as an int
    // leaves the high dword uninitialised (that exact bug crashed ModDialog).
    //
    // B.3.5 bridge — 0x48/8 == slot 9 of `Video::ImageBuffer`, which IS `Create`
    // (B35_RULES 2.3 item 5). The receiver is PROVED a `Video::ImageBuffer`: the
    // block above takes 0x68 bytes (== sizeof(Video::ImageBuffer)) from MemAlloc,
    // runs them through `??0ImageBuffer@Video@@`, then stamps the exe's own NoiseUI
    // ImageBuffer vtable — so the virtual call reads that live table at exactly the
    // +0x48 the slot read did. Same shape as minimap_paint.cpp and
    // serialize_image_upload.cpp (batch I1).
    reinterpret_cast<Video::ImageBuffer*>(imageBuffer)->Create(
        /*type     */ 0,
        /*usage    */ 2,
        /*format   */ 5,
        /*flags    */ 0u,
        /*width    */ 0x100,
        /*height   */ 0x100,
        /*filter   */ 1,
        /*addressU */ 1,
        /*addressV */ 1,
        /*addressW */ 1,
        /*levels   */ nullptr);   // B.3.5 bridge

    // Allocate 256*256 ARGB pixel buffer, fill with black (0xFF000000)
    uint32_t* pixels = (uint32_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x40000, 4);
    uint32_t* p = pixels;
    for (int i = 0; i < 0x10000; i++) {
        *p = 0xFF000000;
        p++;
    }

    // Copy pixel data to ImageBuffer texture
    reinterpret_cast<Video::ImageBuffer*>(*(void**)(s + 0x30))
        ->CopyToTexture(pixels, 0x100, 0x100);   // B.3.5 bridge
    fn_MemFreeStatic(pixels);

    // Setup MaterialInstance + SpriteSheet, find "noiseImage" element
    NoiseUI_SetupImage((void*)s, *(void**)(s + 0x30),
                       (void**)(s + 0x40), (void**)(s + 0x38));

    // Find slider elements and store as InternalBasePtrs
    void* dialogElem = *(void**)(s + 0x20);
    *(uint64_t*)(s + 0x48) = NoiseUI_FindElemAsBasePtr(dialogElem, "noisePersistance");
    *(uint64_t*)(s + 0x50) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseOctaves");
    *(uint64_t*)(s + 0x58) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseScale");
    *(uint64_t*)(s + 0x60) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseCutOff");
    *(uint64_t*)(s + 0x68) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseGradient");

    // Find "buttonGenerate" button, set element ID to 1000
    uint8_t retBuf[16], bpBuf[8];
    System::String nameStr("buttonGenerate");

    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
    void* raw = *(void**)retBuf;
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(raw));   // B.3.3 bridge

    // Set element ID on the button
    void* buttonElem = *(void**)bpBuf;
    reinterpret_cast<UI::Element*>(buttonElem)->SetElementId(1000);  // B.3.6 bridge
}

// ============================================================
// NoiseUI::Slot9 (vtable +0x48) — Handle button click
// ============================================================
// Original: 0x1400714A0
//
// When msg==0x12 (button click) and id==1000 (buttonGenerate),
// regenerates the Perlin noise texture using current slider values.

void NoiseUI_Slot9(NoiseUI* self, void* param2, int msg, int id,
                   int param5, int param6) {
    (void)param2;
    (void)param5;
    (void)param6;

    if (msg != 0x12 || id != 1000) return;

    // Regenerate noise texture from slider values
    NoiseUI_GenerateNoise((void*)self);
}

// ============================================================
