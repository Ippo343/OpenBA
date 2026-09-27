#pragma once
// Function declarations for deeply reimplemented OrchardComponent methods.

#include "OrchardComponent.h"
#include <cstdint>

void OrchardComponent_InitFuncs();

// OrchardComponent::GetGrowth (0x140051640) — average growth ratio 0.0-1.0 across
// the planted tree slots. Backs OrchardUI::Slot7's yield bar + percentage text.
float OrchardComponent_GetGrowth(void* selfV);
