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
#include "engine_funcs.h"
#include <cstdint>

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
    fn_MapUI_SetupImage(self, terrainBuffer,
                        (void**)(s + 0x38), (void**)(s + 0x30),
                        '\0', "imageMap");

    // Setup overlay image (with blend, element "imageOverlay")
    void* overlayBuffer = *(void**)((uintptr_t)mapComponent + 0x10);
    fn_MapUI_SetupImage(self, overlayBuffer,
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
    fn_MapUI_HandleClick(param2, clickX - elemX, clickY - elemY, elemW, elemH);
}
