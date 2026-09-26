#pragma once
// The two by-address doors onto TutorialInstance's members (ENDGAME B.3.8
// block GOAL). TutorialInstance is a real class now
// (code/Game/Tutorial/TutorialInstance.h); standalone/reg_w2_lane2.cpp's opt-in
// hand table still names these by address (B38_RULES D5).

#include "TutorialInstance.h"
#include <cstdint>

// Slot0 (vtable +0x00, 0x1400281a0): the QUALIFIED TutorialInstance::DeletingDtor.
void* TutorialInstance_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400e1520): the QUALIFIED TutorialInstance::Persist.
void TutorialInstance_Serialize(void* self, void* stream);
