#pragma once
// ComponentEntity doors — the C entry points that are still reached BY ADDRESS
// or from files outside the class's block. The class itself is
// code/Game/Entity/ComponentEntity.h (ENDGAME B.3.8 K1); its slots and helpers
// are members there.

#include "Entity/ComponentEntity.h"
#include <cstdint>

// Ctor from a description (0x1400c3720, the retail "CopyCtor"): registered at
// that address (fn_EntityFactory_CreateEntity), so the door stays. It
// placement-constructs the class on caller-allocated memory.
ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr);

// EntityList::RegisterEntity (0x1400c48a0): adds a freshly-instantiated entity
// (wrapped in an InternalBasePtr) to the world entity list under a category.
// Assigns id/owner/category-handle, registers update/draw buckets + main array
// + per-category bucket, runs the OnAdded lifecycle pass. Registered for
// standalone so fn_EntityList_RegisterEntity resolves to it.
void EntityList_RegisterEntity_Impl(void* entityList, void* entityIBP, int category);
