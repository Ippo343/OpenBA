// code/Game/Dialog/mapui.cpp
// Clean reimplementation of MapUI methods.
//
// MapUI (0x50 bytes): Minimap display panel.
// Extends DialogController (0x30 base).
//
// Layout:
//   +0x30  materialInstance (ptr): material for map image
//   +0x38  spriteSheet (ptr): sprite sheet for map image
//   +0x40  overlayMaterial (ptr): material for overlay image
//   +0x48  overlaySpriteSheet (ptr): sprite sheet for overlay

#include "Dialog/ui_panels.h"
#include "options.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Thumbnail vtable stamps — see code/Game/Dialog/thumb_vtables.cpp.
extern uintptr_t ThumbVtable_StampMaterialInstance(void* obj);
extern uintptr_t ThumbVtable_StampSpriteSheet(void* obj);

// ============================================================
// SetupImage (0x14006cb00): Create MaterialInstance + SpriteSheet
// from an ImageBuffer, find the named image element, set sprite.
// ============================================================
// Allocates MaterialInstance (0x60) and SpriteSheet (0x58),
// loads material ("UIMaterialNoBlend.rsc" or "UIMaterial.rsc"),
// adds the ImageBuffer as "diffuse" texture, builds parameters,
// configures sprite info, finds named element, sets sprite.
// If useBlend is set, also sets flags and element ID 1000.
//
// Original: FUN_14006cb00
void MapUI_SetupImage(void* self, void* imageBuffer, void** spriteSheetOut,
                      void** materialOut, char useBlend, const char* elementName) {
    uintptr_t s = (uintptr_t)self;
    bool trc = Opt().mapTrace != nullptr;
    #define MAPTRC(msg) do { if (trc) { printf("[SetupImg] " msg " (elem=%s)\n", elementName); fflush(stdout);} } while(0)
    MAPTRC("enter");

    // Defensive (standalone bring-up): the minimap ImageBuffer must exist — the
    // very first thing below is fn_ImageBuffer_Width(imageBuffer). If the GPU
    // buffer was never created (Minimap_CreateAndUpload not yet run) skip rather
    // than fault. Also require the core primitives.
    if (!imageBuffer || !fn_MemAlloc || !g_pMemAllocSingleton ||
        !fn_MaterialInstance_Ctor || !fn_SpriteSheet_Ctor ||
        !fn_ImageBuffer_Width || !fn_FindElement) {
        if (trc) printf("[SetupImg] GUARD-RETURN elem=%s imgBuf=%p MemAlloc=%p Alloc*=%p "
               "MatCtor=%p SSCtor=%p ImgW=%p FindElem=%p\n", elementName, imageBuffer,
               (void*)fn_MemAlloc, g_pMemAllocSingleton, (void*)fn_MaterialInstance_Ctor,
               (void*)fn_SpriteSheet_Ctor, (void*)fn_ImageBuffer_Width, (void*)fn_FindElement);
        *materialOut = nullptr;
        *spriteSheetOut = nullptr;
        return;
    }

    // --- Allocate and construct MaterialInstance (0x60 bytes) ---
    void* matInst = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    void* materialPtr = nullptr;
    if (matInst) {
        fn_MaterialInstance_Ctor(matInst);
        // Set vtable to the exe's MaterialInstance vtable copy (0x14015c400).
        ThumbVtable_StampMaterialInstance(matInst);
        materialPtr = matInst;
    }
    *materialOut = materialPtr;

    // --- Load material resource ---
    uint8_t filenameBuf[16];
    if (useBlend == '\0') {
        fn_String_Ctor_CStr(filenameBuf, "Material/UI/UIMaterialNoBlend.rsc");
    } else {
        fn_String_Ctor_CStr(filenameBuf, "Material/UI/UIMaterial.rsc");
    }
    fn_Filename_AdjustDelimiters(filenameBuf);

    MAPTRC("material alloc+ctor done");
    void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    void* material = nullptr;
    if (ext) {
        if (g_TypeId_Material && fn_IsDerivedFrom(ext, *g_TypeId_Material)) {
            material = ext;
        }
    }
    fn_MaterialInstance_SetMaterial(*materialOut, material);
    fn_String_Dtor(filenameBuf);

    // --- Add ImageBuffer as "diffuse" texture ---
    uint8_t diffuseStr[8];
    fn_String_Ctor_CStr(diffuseStr, "diffuse");
    MAPTRC("SetMaterial done, add diffuse texture");
    fn_MaterialInstance_AddTexture(*materialOut, diffuseStr, imageBuffer);
    fn_String_Dtor(diffuseStr);

    MAPTRC("BuildParameters");
    fn_MaterialInstance_BuildParameters(*materialOut);
    MAPTRC("BuildParameters done, alloc spritesheet");

    // --- Allocate and construct SpriteSheet (0x58 bytes) ---
    void* sheet = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    void* spriteSheet = nullptr;
    if (sheet) {
        fn_SpriteSheet_Ctor(sheet);
        // Set vtable to the exe's SpriteSheet vtable copy (0x14015c3b0).
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
    uint64_t emptyStr1 = 0, emptyStr2 = 0;
    fn_String_Assign((void*)(ss + 0x40), &emptyStr1);
    fn_String_Assign((void*)(ss + 0x38), &emptyStr2);
    *(int*)(ss + 0x48) = imgW;
    *(int*)(ss + 0x4c) = imgH;
    *(uint8_t*)(ss + 0x50) = 0;
    fn_String_Dtor(&emptyStr2);
    fn_String_Dtor(&emptyStr1);

    // Add sprite info: empty name, empty group, full image rect
    imgW = fn_ImageBuffer_Width(imageBuffer);
    imgH = fn_ImageBuffer_Height(imageBuffer);
    uint64_t nameEmpty = 0, groupEmpty = 0;
    MAPTRC("AddSpriteInfo");
    fn_SpriteSheet_AddSpriteInfo(*spriteSheetOut, &nameEmpty, &groupEmpty, 0, 0, imgW, imgH);
    fn_String_Dtor(&nameEmpty);
    fn_String_Dtor(&groupEmpty);
    MAPTRC("AddSpriteInfo done");

    // Set sprite info with UV scale
    imgW = fn_ImageBuffer_Width(imageBuffer);
    imgH = fn_ImageBuffer_Height(imageBuffer);
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    fn_SpriteSheet_SetSpriteInfo(*spriteSheetOut, 0, imgW, imgH, 0.0f, 0.0f, uvScale, uvScale);

    // --- Find named image element and set sprite ---
    uint8_t elemNameStr[8];
    fn_String_Ctor_CStr(elemNameStr, elementName);

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    void* dialogElem = *(void**)(s + 0x20);
    uint64_t foundElem = 0;
    fn_FindElement(dialogElem, &foundElem, elemNameStr);

    fn_InternalBasePtr_CheckPtr((void*)&foundElem);
    void* rawElem = (void*)foundElem;

    // Cast to ImageElement via IsDerivedFrom
    void* imageElem = nullptr;
    if (rawElem && g_TypeId_ImageElement &&
        fn_IsDerivedFrom(rawElem, *g_TypeId_ImageElement)) {
        imageElem = rawElem;
    }
    if (Opt().mapTrace)
        printf("[SetupImage] elem '%s': raw=%p imageElem=%p mat=%p sheet=%p\n",
               elementName, rawElem, imageElem, *materialOut, *spriteSheetOut);
    uint64_t imageElemPtr = 0;
    fn_InternalBasePtr_Ctor((void*)&imageElemPtr, imageElem);
    fn_String_Dtor(elemNameStr);

    // Create Sprite from SpriteSheet (index 0) and set on element
    uint8_t spriteBuf[16];
    fn_Sprite_Ctor_ByIndex(spriteBuf, *spriteSheetOut, 0);
    fn_InternalBasePtr_CheckPtr((void*)&imageElemPtr);
    fn_ImageElement_SetSprite((void*)imageElemPtr, spriteBuf);

    // If blend mode, set flags and element ID
    if (useBlend != '\0') {
        fn_InternalBasePtr_CheckPtr((void*)&imageElemPtr);
        fn_Element_SetFlagsOnOff((void*)imageElemPtr, 0x10000, 0);
        fn_InternalBasePtr_CheckPtr((void*)&imageElemPtr);
        fn_Element_SetElementId((void*)imageElemPtr, 1000);
    }
}

// ============================================================
// HandleClick (0x14006c850): Move camera to map click position
// ============================================================
// Translates minimap click coordinates into camera movement.
// Gets current camera frustum, projects corners to ground plane,
// and computes new camera position based on click offset.
//
// Original: FUN_14006c850
//
// NOTE: This function relies heavily on camera/frustum internals
// (Graphics::Camera::GetFrustum, Volume::Frustum::GetCorners,
// Math::Transform3::Rotate, Math::Vector3 operations) which are
// complex to reimplement. The core logic is:
//   1. Validate click is within bounds
//   2. Get camera frustum corners, project far corners to ground
//   3. Average the projected positions (camera center)
//   4. Compute new position offset: (click - center) * worldScale
//   5. Write to camera target position
//
// For now, we implement the final coordinate mapping directly,
// which is the essential game logic. The frustum projection
// (FUN_14006c5b0) is called as a resolved function pointer since
// it depends on deep math/camera internals.

// fn_MapUI_ProjectCornersToGround is resolved globally in engine_funcs.cpp

void MapUI_HandleClick(void* param1, int clickX, int clickY, int mapWidth, int mapHeight) {
    (void)param1;

    if (clickX < 0 || clickX >= mapWidth || clickY < 0 || clickY >= mapHeight)
        return;

    if (!fn_MapUI_ProjectCornersToGround) return;

    // FUN_14006c5b0 fills an array of 4 projected ground-plane corner positions
    // (Vector3, 12 bytes each) plus 4 frustum corners = 8 Vector3s total
    float fullBuf[8 * 3];
    for (int i = 0; i < 8 * 3; i++) fullBuf[i] = 0.0f;

    // Call the ground projection function — fills corner positions from camera frustum
    fn_MapUI_ProjectCornersToGround(fullBuf);

    // The original also averages the 4 projected corners (0.25 * Σcorners) but
    // DISCARDS the result — dead code confirmed in the 0x14006c850 disassembly —
    // so the camera move depends only on the click position and map grid size.

    // Get game world and camera data
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gw = (uintptr_t)gameWorld;

    // Camera controller is at gameWorld+0xE0
    uintptr_t camCtrl = *(uintptr_t*)(gw + 0xE0);

    // Map grid size from gameWorld+0xF8 (int* -> int)
    int mapGridSize = **(int**)(gw + 0xF8);

    // Compute world-space scale factor: mapGridSize / mapWidth
    float worldScale = (float)mapGridSize / (float)mapWidth;

    // Set camera focus at camCtrl+0x1C/0x20/0x24 (same field WASD panning drives).
    // Maps click position: X = (clickX - halfWidth) * scale, Y = (halfHeight - clickY) * scale
    *(float*)(camCtrl + 0x1C) = (float)(clickX - (mapWidth >> 1)) * worldScale;
    *(float*)(camCtrl + 0x20) = (float)((mapHeight >> 1) - clickY) * worldScale;
    *(float*)(camCtrl + 0x24) = 0.0f;
}

// ============================================================
// Slot4 (vtable +0x20): Setup map and overlay images
// ============================================================
// Original: 0x14006CA80
//
// Gets the map component from game world (+0x148), which holds two
// ImageBuffers: +0x08 (terrain map) and +0x10 (overlay).
// For each, calls the helper to create MaterialInstance + SpriteSheet,
// find the image element, and set the sprite.

void MapUI_Slot4(MapUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* gameWorld = *g_pGameWorldSingleton;
    void* mapComponent = *(void**)((uintptr_t)gameWorld + 0x148);

    // Setup terrain map image (no blend, element "imageMap")
    void* terrainBuffer = *(void**)((uintptr_t)mapComponent + 0x08);
    MapUI_SetupImage(self, terrainBuffer,
                     (void**)(s + 0x38), (void**)(s + 0x30),
                     '\0', "imageMap");

    // Setup overlay image (with blend, element "imageOverlay")
    void* overlayBuffer = *(void**)((uintptr_t)mapComponent + 0x10);
    MapUI_SetupImage(self, overlayBuffer,
                     (void**)(s + 0x48), (void**)(s + 0x40),
                     '\x01', "imageOverlay");
}

// ============================================================
// Slot9 (vtable +0x48): Handle map click — move camera
// ============================================================
// Original: 0x14006C9E0
//
// When msg==2 (mouse click) and id==1000 (overlay element),
// reads the element's position/size from the clicked element,
// calculates relative click position, and moves the camera.

void MapUI_Slot9(MapUI* self, void* param2, int msg, int id,
                 int clickX, int clickY) {
    (void)self;
    if (msg != 2 || id != 1000) return;

    // param2 is an InternalBasePtr to the clicked element
    fn_InternalBasePtr_CheckPtr(param2);
    uintptr_t elem = *(uintptr_t*)param2;

    // Read element position and size
    int elemX = *(int*)(elem + 0x68);  // element X position
    int elemY = *(int*)(elem + 0x6C);  // element Y position
    int elemW = *(int*)(elem + 0x60);  // element width
    int elemH = *(int*)(elem + 0x64);  // element height

    // Translate to map-relative coordinates and move camera
    MapUI_HandleClick(param2, clickX - elemX, clickY - elemY, elemW, elemH);
}

// ============================================================
