#pragma once
// Function declarations for deeply reimplemented TutorialImageHotKey methods.
// Called from engine/reimpl/tutorialimagehotkey.cpp wrappers.

#include "TutorialImages.h"
#include <cstdint>

// ============================================================
// TutorialImageHotKey — displays hot key image in tutorial steps.
// Vtable: 0x140160ab0 (10 slots), Size: 0x48
// Extends TutorialImage -> Object::External
// ============================================================

// Slot0 (vtable +0x00, 0x140027d70): Scalar deleting destructor.
// Sets vtable to TutorialImageHotKey, destroys String at +0x30,
// sets vtable to TutorialImage, calls External::~External, optionally frees.
void* TutorialImageHotKey_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140027cf0): GetTypeInfo — 8 bytes, unhookable.
// Returns DAT_1401c65c8 (type info pointer).

// Slot2 (vtable +0x10, 0x140027ce0): GetTypeId — 7 bytes, unhookable.
// Returns TypeInfo_TutorialImageHotKey (type ID).

// Slot3 (vtable +0x18, 0x1400e0620): GetDescription — 6 bytes, unhookable.
// Returns constant 0x1001.
inline int TutorialImageHotKey_GetDescription(void* /*self*/) { return 0x1001; }

// Slot4 (vtable +0x20, 0x14003a912): Serialize thunk — unhookable.

// Slot5 (vtable +0x28, 0x1400e0540): Serialize.
// Serializes base TutorialImage fields (+0x28, +0x2c, +0x20 external ref),
// then HotKey-specific String at +0x30 and resolves external ref if loading.
void TutorialImageHotKey_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140027d50): Cleanup.
// Calls dtor(self,0) then re-initializes via Ctor.
void TutorialImageHotKey_Cleanup(void* self);

// Slot7 (vtable +0x38, 0x140001640): Base dispatch — 8 bytes, unhookable.

// Slot8 (vtable +0x40, 0x140027d40 wrapper -> 0x1400e0680 real): Constructor/Init.
// Calls External::External, sets vtable chain, zeroes fields.
void* TutorialImageHotKey_Ctor(void* self);

// Slot9 (vtable +0x48, 0x1400e0420): Display logic.
// Creates UI element from imageRef, finds TextElement child, resolves hotkey
// name via KeyMap, sets text on the TextElement.
void* TutorialImageHotKey_Slot9(void* self, void* retIBP, void* dialog);
