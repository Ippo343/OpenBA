#pragma once
// Function declarations for deeply reimplemented StatusIconComponent methods.
// Called from engine/reimpl/statusiconcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "StatusIconComponent.h"
#include <cstdint>

// ============================================================
// StatusIconComponent — floating status icon display component
// Vtable: 0x140164620
// Size: 0x108 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14012bff0): Serializes icon field.
// Calls Component_Serialize base, then serializes fieldF0 as uint via
// IO::Serializer(version 0). On load, writes back to +0xF0.
void StatusIconComponent_Shutdown(StatusIconComponent* self, void* stream);

// Serialize (vtable slot [4], 0x14012bed0): Reads sprite data from building description.
// Gets StatusIconDescription via IsDerivedFrom, reads SpriteSheet::GetSpriteCount,
// calls helper to set up geometry with sprite dimensions and sheet.
void StatusIconComponent_Serialize(StatusIconComponent* self);

// OnRemove (vtable slot [8], 0x14012bbd0): Manages Scene add/remove, positions icon.
// Complex function: checks game settings icon visibility flag (+0xC byte),
// adds/removes from Scene, scales based on game settings, positions based on
// entity's model bounding box or map coordinates. Very complex (160+ instructions).
void StatusIconComponent_OnRemove(StatusIconComponent* self);

// OnUpdate2 (vtable slot [11], 0x14012bf40): Updates icon visibility in Scene.
// Calls Geometry vtable +0x20 (Draw/Update), saves/restores fieldF0 value,
// if modelId < 0 and fieldF0 != 0: adds to Scene, sets flags, notifies entity,
// then calls vtable +0x40 (OnRemove) with Time(0) to position.
void StatusIconComponent_OnUpdate2(StatusIconComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void StatusIconComponent_InitFuncs();
