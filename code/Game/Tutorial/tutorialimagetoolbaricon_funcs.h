#pragma once
// Function declarations for deeply reimplemented TutorialImageToolbarIcon methods.
// Called from engine/reimpl/tutorialimagetoolbaricon.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "TutorialImages.h"
#include <cstdint>

// ============================================================
// TutorialImageToolbarIcon -- toolbar icon image display
// Vtable: 0x140160b90 (10 slots), Size: 0x48
// Extends TutorialImage
// ============================================================

// Slot0: Dtor (0x140028090)
// Sets own vtable, destroys String at +0x30, sets base vtable,
// calls External dtor, conditional free.
void* TutorialImageToolbarIcon_Dtor(void* self, uint32_t flags);

// Slot1: GetTypeInfo (0x140028010, 8b unhookable)
// Returns DAT_1401c6638.

// Slot2: GetTypeId (0x140028000, 7b unhookable)
// Returns TypeInfo_TutorialImageToolbarIcon.

// Slot3: GetDescription (0x1400e0bd0, 6b unhookable)
// Returns 0x1001.
inline int TutorialImageToolbarIcon_GetDescription(void* /*self*/) { return 0x1001; }

// Slot4: Serialize thunk (0x14003a912, unhookable)

// Slot5: Serialize (0x1400e0af0)
// Creates Serializer, serializes +0x28 and +0x2C as ints,
// serializes +0x20 ExternalPtr via TutorialImage_sub_7ad0,
// creates second Serializer, serializes String at +0x30,
// if loading (stream mode == 0) calls FUN_140027de0 to resolve toolbar.
void TutorialImageToolbarIcon_Serialize(void* self, void* stream);

// Slot6: Cleanup (0x140028070)
// Calls own Dtor(flags=0), then calls real Ctor to reinitialize.
void TutorialImageToolbarIcon_Cleanup(void* self);

// Slot7: Base dispatch (0x140001640, 8b unhookable)

// Slot8: Ctor/Init (real Ctor at 0x1400e0c30, wrapper at 0x140028060)
// Calls External ctor, sets base vtable, zeros fields, sets own vtable,
// initializes String at +0x30, sets +0x38 to -1, zeros +0x40.
void* TutorialImageToolbarIcon_Ctor(void* self);

// Slot9: Slot9/Update (0x1400e0940)
// Creates UI element, finds ImageElement child, resolves toolbar icon,
// creates sprite from toolbar data, sets sprite on ImageElement.
void TutorialImageToolbarIcon_Slot9(void* self, void* param1, void* param2);
