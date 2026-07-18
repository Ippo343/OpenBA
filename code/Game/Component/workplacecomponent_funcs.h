#pragma once
// Function declarations for deeply reimplemented WorkPlaceComponent methods.
// Called from engine/reimpl/workplacecomponent.cpp wrappers.

#include "WorkPlaceComponent.h"
#include <cstdint>

// ============================================================
// WorkPlaceComponent — worker assignment management
// Vtable: 0x140164d30
// Size: 0x68 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14012f120): Complex serialization.
// Component_Serialize + empty serializer + versioned block with worker array
// at +0x58 (count at +0x60), IsDerivedFrom validation, isAccepting bool.
void WorkPlaceComponent_Shutdown(WorkPlaceComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14012f090): Register with profession list.
// Reads +0x48 (profession ID), looks it up in BST at game state +0xf0+0x100.
// If found, registers owner. Falls back to FUN_14012ee60 init.
void WorkPlaceComponent_OnCreate(WorkPlaceComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void WorkPlaceComponent_InitFuncs();
