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
#include <cstdint>

// Helper: find element by name in parent element, cast to InternalBasePtr
static uint64_t NoiseUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16], bpBuf[8];

    fn_String_Ctor_CStr(nameStr, name);

    // fn_FindElement: member function, RCX=parentElem, RDX=retBuf, R8=nameStr
    fn_FindElement(parentElem, retBuf, nameStr);

    // Check the InternalBasePtr result
    fn_InternalBasePtr_CheckPtr(retBuf);
    void* raw = *(void**)retBuf;

    // Cast to specific type via IsDerivedFrom check
    // (Ghidra shows the pattern: CheckPtr -> deref -> IsDerivedFrom -> InternalBasePtr_Ctor)
    fn_InternalBasePtr_Ctor(bpBuf, raw);
    uint64_t result = *(uint64_t*)bpBuf;

    fn_String_Dtor(nameStr);
    return result;
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
    // Signature: Create(format=0, type=2, usage=5, mipmap=0,
    //                    width=256, height=256, depth=1, arraySize=1,
    //                    sampleCount=1, sampleQuality=1, flags=0)
    typedef void (*fn_ImageBuffer_Create_t)(void* self, int fmt, int type, int usage,
                                             int mip, int w, int h, int depth,
                                             int arrSize, int sampleCount,
                                             int sampleQuality, int flags);
    uintptr_t vtable = *(uintptr_t*)imageBuffer;
    fn_ImageBuffer_Create_t fnCreate =
        *(fn_ImageBuffer_Create_t*)(vtable + 0x48);
    fnCreate(imageBuffer, 0, 2, 5, 0, 0x100, 0x100, 1, 1, 1, 1, 0);

    // Allocate 256*256 ARGB pixel buffer, fill with black (0xFF000000)
    uint32_t* pixels = (uint32_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x40000, 4);
    uint32_t* p = pixels;
    for (int i = 0; i < 0x10000; i++) {
        *p = 0xFF000000;
        p++;
    }

    // Copy pixel data to ImageBuffer texture
    fn_ImageBuffer_CopyToTexture(*(void**)(s + 0x30), pixels, 0x100, 0x100);
    fn_MemFree(g_pMemAllocSingleton, pixels);

    // Setup MaterialInstance + SpriteSheet, find "noiseImage" element
    fn_NoiseUI_SetupImage((void*)s, *(void**)(s + 0x30),
                          (void**)(s + 0x40), (void**)(s + 0x38));

    // Find slider elements and store as InternalBasePtrs
    void* dialogElem = *(void**)(s + 0x20);
    *(uint64_t*)(s + 0x48) = NoiseUI_FindElemAsBasePtr(dialogElem, "noisePersistance");
    *(uint64_t*)(s + 0x50) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseOctaves");
    *(uint64_t*)(s + 0x58) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseScale");
    *(uint64_t*)(s + 0x60) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseCutOff");
    *(uint64_t*)(s + 0x68) = NoiseUI_FindElemAsBasePtr(dialogElem, "noiseGradient");

    // Find "buttonGenerate" button, set element ID to 1000
    uint8_t nameStr[8], retBuf[16], bpBuf[8];
    fn_String_Ctor_CStr(nameStr, "buttonGenerate");

    fn_FindElement(dialogElem, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);
    void* raw = *(void**)retBuf;
    fn_InternalBasePtr_Ctor(bpBuf, raw);

    fn_String_Dtor(nameStr);

    // Set element ID on the button
    fn_InternalBasePtr_CheckPtr(bpBuf);
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
    fn_NoiseUI_GenerateNoise((void*)self);
}
