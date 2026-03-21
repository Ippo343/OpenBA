#pragma once
// Function declarations for deeply reimplemented TutorialImageInputKey methods.
// Called from engine/reimpl/tutorialimageinputkey.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "TutorialImages.h"
#include <cstdint>

// ============================================================
// TutorialImageInputKey -- input key image display
// Vtable: 0x140160b20 (10 slots), Size: 0x38
// Extends TutorialImage
// ============================================================

// Slot0: Dtor (0x140027fa0)
// Sets own vtable, sets base vtable, calls External dtor, conditional free.
void* TutorialImageInputKey_Dtor(void* self, uint32_t flags);

// Slot1: GetTypeInfo (0x140027e90, 8b unhookable)
// Returns DAT_1401c6600.

// Slot2: GetTypeId (0x140027e80, 7b unhookable)
// Returns TypeInfo_TutorialImageInputKey.

// Slot3: GetDescription (0x1400e08a0, 6b unhookable)
// Returns 0x1001.
inline int TutorialImageInputKey_GetDescription(void* /*self*/) { return 0x1001; }

// Slot4: Serialize thunk (0x14003a912, unhookable)

// Slot5: Serialize (0x1400e07f0)
// Creates Serializer, serializes +0x28 and +0x2C as ints,
// serializes +0x20 ExternalPtr via TutorialImage_sub_7ad0,
// creates second Serializer, serializes +0x30 via stream vtable +0x98.
void TutorialImageInputKey_Serialize(void* self, void* stream);

// Slot6: Cleanup (0x140027f50)
// Calls own Dtor(flags=0), then External ctor, then reinits base+own vtable and fields.
void* TutorialImageInputKey_Cleanup(void* self);

// Slot7: Base dispatch (0x140001640, 8b unhookable)

// Slot8: Ctor/Init (0x140027f00)
// Null-check self, call External ctor, set base vtable, zero fields, set own vtable.
void* TutorialImageInputKey_Ctor(void* self);

// Slot9: Slot9/Update (0x1400e06e0)
// Creates UI element, finds TextElement child, looks up input key name
// from KeyMap, sets text on the TextElement.
void TutorialImageInputKey_Slot9(void* self, void* param1, void* param2);
