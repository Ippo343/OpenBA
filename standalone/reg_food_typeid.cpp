// standalone/reg_food_typeid.cpp
// Food-production registrations.
//
// ENDGAME B.3.9 TR: the eight Herbalist / CropField / Fishing / Hunt component +
// description type-id storage cells (0x1401c43a0 .. 0x1401c78c8, filled by name after
// Object::Factory::Startup) are gone -- every reader reads X::TypeId() of the class at
// the cell. The .rdata constant and the maturity door below stay.
#include "resolve.h"
#include <cstdint>

// .rdata float constants used by the food controllers. These are NOT auto-available:
// every one has to be registered by address or ResolveAddr hands back 0, and a null
// constant here is silent -- the crop field's harvest trigger simply returned early
// forever with no log of its own. 0x140172144 = 0x3F7D70A4 = 0.99f, the maturity
// threshold the state-10 per-frame compares mean crop growth against; without it the
// ripeness ARM of the trigger can never be evaluated, so a field that plants perfectly
// still never harvests. Its siblings (0x1401721d4 = 33.0f, 0x14017225c = 3600.0f) were
// already registered elsewhere, which is exactly why only this one arm was dead.
static const float k_rdata_172144 = 0.99f;

// 0x14011b410 = mean crop maturity. CropUI's per-frame slot resolves this by address
// (retail CropUI_Update calls it to drive the yield bar), so it has to be registered
// even though the crop-field state machine reaches the same body directly.
extern "C" float CropFieldComponent_Maturity(void* comp);
void RegisterFoodTypeIds() {
    RegisterExeAddr(0x140172144, reinterpret_cast<uintptr_t>(&k_rdata_172144));
    RegisterExeAddr(0x14011b410, reinterpret_cast<uintptr_t>(&CropFieldComponent_Maturity));
}
