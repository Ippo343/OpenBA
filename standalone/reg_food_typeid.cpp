// standalone/reg_food_typeid.cpp
// Storage + fill for the food-production type-id .rdata globals.
//
// Every slot below was registered NOWHERE, so ResolveAddr(Exe, slot) returned 0 and
// each consumer's `IsDerivedFrom(obj, *pTypeId)` test silently failed for every object,
// forever. That is not a safe default — it is a permanent miss that looks like correct
// code doing nothing, the same trap reg_orchard_typeid.cpp was written to close.
//
// MEASURED 2026-08-11, which is what identified this: with the fishing dock live and
// placed on genuinely good water (the placement harness scored 351 fishable tiles in
// radius from the TEMPLATE's mask 0x00000180), the runtime produce tick reported
//     [FishTrace] ProduceTick ... r=14 mask=0x00000000 matching=0/611 rate=0.000
// The dock was in the right place, the radius resolved, the tile scan ran — and the
// accepted-terrain mask was zero because FishingComponent_ProduceTick could not resolve
// 0x1401c7698 to test whether owner+0x18 really is a FishingDescription. A dock that
// rates 0.000 on perfect water is indistinguishable from a broken port.
//
// Slots (all verified against standalone/factory_registrations.csv):
//   0x1401c43a0  HerbalistComponent    (HerbalistComponent_RegisterType)
//   0x1401c43d8  HerbalistDescription  (HerbalistDescription_Register)
//   0x1401c7510  CropFieldComponent    (CropFieldComponent_RegisterType)
//   0x1401c7548  CropFieldDescription  (CropFieldDescription_Register)
//   0x1401c7660  FishingComponent      (FishingComponent_RegisterType)
//   0x1401c7698  FishingDescription    (FishingDescription_Register)
//   0x1401c7890  HuntComponent         (HuntComponent_RegisterType)
//   0x1401c78c8  HuntDescription       (HuntDescription_Register)
//
// RadiusDescription (0x1401c4870) is deliberately absent: it already has storage from
// reg_radius_typeid, and registering a second one would silently split the slot.
//
// Two-phase, exactly like reg_orchard_typeid.cpp:
//   RegisterFoodTypeIds()      — from AddressRegistryInit, BEFORE EngineFuncsInit, so
//                                every engine_funcs global points at our storage.
//   FillTypeIdGlobals_food()   — from game_main.cpp AFTER Object::Factory::Startup, the
//                                only point at which the real CRC ids exist (filling
//                                earlier stores the 0xFFFFFFFF sentinel).
//
// NOT gated on an env opt-in, for the same reason as the orchard slots: an unfilled
// type-id global is not a conservative default, it is a silent permanent miss.

#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

namespace {

struct Slot {
    uintptr_t   addr;
    const char* name;
    uint32_t    value;
};

// Order matches the address order above; `value` is the live storage the exe-faithful
// `*(uint32_t*)0x1401cXXXX` reads resolve to.
Slot g_slots[] = {
    { 0x1401c43a0, "HerbalistComponent",   0 },
    { 0x1401c43d8, "HerbalistDescription", 0 },
    { 0x1401c7510, "CropFieldComponent",   0 },
    { 0x1401c7548, "CropFieldDescription", 0 },
    { 0x1401c7660, "FishingComponent",     0 },
    { 0x1401c7698, "FishingDescription",   0 },
    { 0x1401c7890, "HuntComponent",        0 },
    { 0x1401c78c8, "HuntDescription",      0 },
};
const int kSlotCount = (int)(sizeof(g_slots) / sizeof(g_slots[0]));

}  // namespace

extern "C" void FillTypeIdGlobals_food() {
    int filled = 0, empty = 0;
    for (int i = 0; i < kSlotCount; ++i) {
        g_slots[i].value = ::Object::GetTypeIdByName(g_slots[i].name);
        // Verify the VALUE, not the pointer: a registered slot holding 0 is
        // indistinguishable from an unregistered one at every call site. Report the
        // outcome in BOTH arms so an empty log can never read as success.
        if (g_slots[i].value == 0) {
            ++empty;
            std::fprintf(stderr, "[FoodTypeId] ERROR: GetTypeIdByName(\"%s\") returned 0 — "
                         "slot 0x%llx is live but EMPTY; every IsDerivedFrom against it "
                         "will still miss\n",
                         g_slots[i].name, (unsigned long long)g_slots[i].addr);
        } else {
            ++filled;
            std::fprintf(stderr, "[FoodTypeId] 0x%llx %-21s = %u (0x%08X)\n",
                         (unsigned long long)g_slots[i].addr, g_slots[i].name,
                         g_slots[i].value, g_slots[i].value);
        }
    }
    std::fprintf(stderr, "[FoodTypeId] %d/%d slot(s) filled, %d empty\n",
                 filled, kSlotCount, empty);
    std::fflush(stderr);
}

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
    for (int i = 0; i < kSlotCount; ++i)
        RegisterExeAddr(g_slots[i].addr, reinterpret_cast<uintptr_t>(&g_slots[i].value));
    RegisterExeAddr(0x140172144, reinterpret_cast<uintptr_t>(&k_rdata_172144));
    RegisterExeAddr(0x14011b410, reinterpret_cast<uintptr_t>(&CropFieldComponent_Maturity));
}
