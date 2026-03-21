#pragma once
// Function declarations for deeply reimplemented TutorialImage methods.
// Called from engine/reimpl/tutorialimage.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "TutorialImages.h"
#include <cstdint>

// ============================================================
// TutorialImage — base tutorial image class
// Vtable: 0x140160a40 (10 slots), Size: 0x30
// Extends Object::External
// ============================================================

// Slot0 (vtable +0x00, 0x140027a80): Scalar destructor.
// Sets vtable, calls External::~External, optionally frees memory.
void* TutorialImage_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140027970): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c6590)

// Slot2 (vtable +0x10, 0x140027960): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c6590) — type info pointer

// Slot3 (vtable +0x18, 0x1400e03a0): GetDescription (6b, unhookable).
// Returns constant 0x1000.
inline int TutorialImage_GetDescription(void* /*self*/) { return 0x1000; }

// Slot4 (vtable +0x20, 0x14003a912): Base class Compile thunk (unhookable).
// Delegates to Object::External::Compile.

// Slot5 (vtable +0x28, 0x1400e0330): Serialize.
// Creates Serializer, serializes imageWidth(+0x28), imageHeight(+0x2C),
// then serializes imageRef ExternalPtr(+0x20). Returns 1.
void TutorialImage_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140027a30): Cleanup.
// Calls Dtor(self,0) to run destructor without freeing,
// then re-initializes via External::External, sets vtable, zeroes fields.
void TutorialImage_Cleanup(void* self);

// Slot7 (vtable +0x38, 0x140001640): Base dispatch (8b, unhookable).
// Calls vtable[0](self, 0) — scalar dtor with flags=0.

// Slot8 (vtable +0x40, 0x1400279e0): Ctor/Init (factory constructor).
// If self is null, returns null. Otherwise calls External::External,
// sets vtable, zeroes imageRef/imageWidth/imageHeight.
void* TutorialImage_Ctor(void* self);

// Slot9 (vtable +0x48, 0x1400e0300): CreateElement.
// Calls Dialog::CreateElement(dialog, param_retval, self->imageRef).
// param1=retval, param2=dialog. Returns param1.
void TutorialImage_Slot9(void* self, void* param1, void* param2);
