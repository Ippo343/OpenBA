#pragma once
// Function declarations for deeply reimplemented StatusIconComponent methods.
// Called from engine/reimpl/statusiconcomponent.cpp wrappers.

#include "StatusIconComponent.h"
#include <cstdint>

// ============================================================
// StatusIconComponent — floating status icon display component
// Vtable: 0x140164620
// Size: 0x108 bytes
// ============================================================

// Serialize (vtable slot [4], 0x14012bed0): Reads sprite data from building description.
// Gets StatusIconDescription via IsDerivedFrom, reads SpriteSheet::GetSpriteCount,
// calls helper to set up geometry with sprite dimensions and sheet.
void StatusIconComponent_Serialize(StatusIconComponent* self);

// OnUpdate2 (vtable slot [11], 0x14012bf40): Updates icon visibility in Scene.
// Calls Geometry vtable +0x20 (Draw/Update), saves/restores fieldF0 value,
// if modelId < 0 and fieldF0 != 0: adds to Scene, sets flags, notifies entity,
// then calls vtable +0x40 (OnRemove) with Time(0) to position.
void StatusIconComponent_OnUpdate2(StatusIconComponent* self);

// Init: resolve all internal function pointers (called once at startup)
