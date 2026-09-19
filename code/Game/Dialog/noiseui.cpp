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
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Random

// Thumbnail vtable stamps — see code/Game/Dialog/thumb_vtables.cpp.
extern uintptr_t ThumbVtable_StampMaterialInstance(void* obj);
extern uintptr_t ThumbVtable_StampSpriteSheet(void* obj);

// Helper: find element by name in parent element, cast to InternalBasePtr
static uint64_t NoiseUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    uint8_t retBuf[16], bpBuf[8];

    System::String nameStr(name);

    // fn_FindElement: member function, RCX=parentElem, RDX=retBuf, R8=nameStr
    fn_FindElement(parentElem, retBuf, &nameStr);

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
        fn_MaterialInstance_Ctor(matInst);
        ThumbVtable_StampMaterialInstance(matInst);
        materialPtr = matInst;
    }
    *materialOut = materialPtr;

    // --- Load UIMaterialNoBlend resource ---
    // Filename's ctor already normalises delimiters, so the separate
    // AdjustDelimiters call retail made on the raw buffer is redundant here.
    System::Filename filename("Material/UI/UIMaterialNoBlend.rsc");

    void* ext = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
            ->FindOrLoad(filename));
    void* material = nullptr;
    if (ext) {
        if (g_TypeId_Material && fn_IsDerivedFrom(ext, *g_TypeId_Material)) {
            material = ext;
        }
    }
    fn_MaterialInstance_SetMaterial(*materialOut, material);

    // --- Add ImageBuffer as "diffuse" texture ---
    System::String diffuseStr("diffuse");
    fn_MaterialInstance_AddTexture(*materialOut, &diffuseStr, imageBuffer);

    fn_MaterialInstance_BuildParameters(*materialOut);

    // --- Allocate and construct SpriteSheet (0x58 bytes) ---
    void* sheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    void* spriteSheet = nullptr;
    if (sheet) {
        fn_SpriteSheet_Ctor(sheet);
        ThumbVtable_StampSpriteSheet(sheet);
        spriteSheet = sheet;
    }
    *spriteSheetOut = spriteSheet;

    // Set material on sprite sheet at +0x30
    *(void**)((uintptr_t)spriteSheet + 0x30) = *materialOut;

    // Get image dimensions
    int imgW = fn_ImageBuffer_Width(imageBuffer);
    int imgH = fn_ImageBuffer_Height(imageBuffer);

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
    imgW = fn_ImageBuffer_Width(imageBuffer);
    imgH = fn_ImageBuffer_Height(imageBuffer);
    System::String nameEmpty;
    System::String groupEmpty;
    fn_SpriteSheet_AddSpriteInfo(*spriteSheetOut, &nameEmpty, &groupEmpty, 0, 0, imgW, imgH);

    // Set sprite info with UV scale
    imgW = fn_ImageBuffer_Width(imageBuffer);
    imgH = fn_ImageBuffer_Height(imageBuffer);
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    fn_SpriteSheet_SetSpriteInfo(*spriteSheetOut, 0, imgW, imgH, 0.0f, 0.0f, uvScale, uvScale);

    // --- Find "noiseImage" element and set sprite ---
    System::String elemNameStr("noiseImage");

    void* dialogElem = *(void**)(s + 0x20);
    uint64_t foundElem = 0;
    fn_FindElement(dialogElem, &foundElem, &elemNameStr);

    void* rawElem = (void*)foundElem;

    void* imageElem = nullptr;
    if (rawElem && g_TypeId_ImageElement &&
        fn_IsDerivedFrom(rawElem, *g_TypeId_ImageElement)) {
        imageElem = rawElem;
    }
    uint64_t imageElemPtr = 0;
    new ((void*)&imageElemPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(imageElem));   // B.3.3 bridge

    // Create Sprite from SpriteSheet (index 0) and set on element
    uint8_t spriteBuf[16];
    fn_Sprite_Ctor_ByIndex(spriteBuf, *spriteSheetOut, 0);
    fn_ImageElement_SetSprite((void*)imageElemPtr, spriteBuf);
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
    fn_ImageBuffer_CopyToTexture(*(void**)(s + 0x30), pixels, 0x100, 0x100);
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
        fn_ImageBuffer_Ctor(imgBuf);
        // Set vtable to NoiseUI-specific ImageBuffer vtable
        *(uintptr_t*)imgBuf = g_NoiseUI_ImageBufferVtable;
        imageBuffer = imgBuf;
    }
    *(void**)(s + 0x30) = imageBuffer;

    // Call ImageBuffer::Create virtual (vtable +0x48)
    // Signature: Create(ImageType=0, Usage=2, Format=5, mip=0, width=256,
    //                    height=256, FilterMode=1, AddressMode=1,1,1,
    //                    Array<Array<uchar>>* levels=null)
    // The last parameter is a POINTER in a stack slot -- passing it as an int
    // leaves the high dword uninitialised (that exact bug crashed ModDialog).
    typedef bool (*fn_ImageBuffer_Create_t)(void* self, int type, int usage, int format,
                                             unsigned mip, int w, int h, int filter,
                                             int addrU, int addrV, int addrW,
                                             void* levels);
    uintptr_t vtable = *(uintptr_t*)imageBuffer;
    fn_ImageBuffer_Create_t fnCreate =
        *(fn_ImageBuffer_Create_t*)(vtable + 0x48);
    fnCreate(imageBuffer, 0, 2, 5, 0, 0x100, 0x100, 1, 1, 1, 1, nullptr);

    // Allocate 256*256 ARGB pixel buffer, fill with black (0xFF000000)
    uint32_t* pixels = (uint32_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x40000, 4);
    uint32_t* p = pixels;
    for (int i = 0; i < 0x10000; i++) {
        *p = 0xFF000000;
        p++;
    }

    // Copy pixel data to ImageBuffer texture
    fn_ImageBuffer_CopyToTexture(*(void**)(s + 0x30), pixels, 0x100, 0x100);
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

    fn_FindElement(dialogElem, retBuf, &nameStr);
    void* raw = *(void**)retBuf;
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(raw));   // B.3.3 bridge

    // Set element ID on the button
    void* buttonElem = *(void**)bpBuf;
    fn_Element_SetElementId(buttonElem, 1000);
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
