#pragma once
// Function declarations for deeply reimplemented GrowthComponent methods.
// Called from engine/reimpl/growth.cpp wrappers.

#include "GrowthComponent.h"
#include <cstdint>

// ============================================================
// GrowthComponent -- tree/crop growth with seasonal temperature modulation
// Vtable: 0x140163590
// Size: 0x50 bytes
// ============================================================

// Update (vtable slot [9], 0x140120050): Main growth tick.
// 1. Computes base growth rate from config (1.0 / (config+0x28 * 60.0))
// 2. Applies seasonal temperature modulation if config+0x2C flag set
// 3. Accumulates growth: owner+0x28 += dt * growthRate * tempModifier
// 4. If growth < 0 and not flagged: reset and harvest
// 5. If growth > threshold (config+0x54): spawn sub-resources on timer
// 6. If growth > growthScale and conditions met: harvest, reset growth
void GrowthComponent_Update(GrowthComponent* self, int64_t* deltaTime);

// ApplyVisualScale (vtable slot [10], +0x50, 0x14011fea0): eases the model's
// MeshInstance visual scale toward the current growth value. Dispatched by
// EntityList::PostUpdateNodeHandler for components carrying flag bit 4, which
// GrowthComponent_Update sets whenever growth advances.
void GrowthComponent_ApplyVisualScale(GrowthComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void GrowthComponent_InitFuncs();
