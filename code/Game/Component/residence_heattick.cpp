// code/Game/Component/residence_heattick.cpp
// ResidenceComponent state-2 (built) per-frame tick — faithful port of
// 0x140129970 (Ghidra label "ResidenceDescription_helper_a350_sub_9970"; it is
// the residence HEAT tick). Registered as the state-2 trans38Handler in the
// ResidenceComponent controller state table (residencecomponent.cpp) and
// dispatched every frame by StateMachine::Tick through the node vtable slot 7
// (see controller_state_table.h Trans38 — the exe slot-7 dispatcher 0x140037a00
// passes RDX=sm and R8=Time* dt through its tail-JMP).
//
// Retail logic (decompile-verified):
//   1. heatRemaining(+0x48, System::Time int64 countdown) -= *dt, every tick.
//   2. [audio] occupied-house chimney emitter upkeep (FUN_140002280/42960/42c60).
//   3. [mesh]  sub-part 6 when edible count > 0 (0x14012a9d0), sub-part 7 when
//              0x14012ab00 count > 0 — via FUN_14012c3f0 on the MeshInstance.
//   4. [abandon] occupancy==0 && storage non-empty && no pending action ->
//              push a ClearStorage action (0x14012ead0/ec60/eb50).
//   5. BURN: if worldTemp < desc->heatThreshold(+0x28) AND heatRemaining < 0
//      AND residentSlotCount(+0x58) > 0: scan the house StorageComponent
//      (finder 0x140006270) slots for a stack whose RawMaterialDescription
//      (tid DWORD_1401c7cb8) has FUEL flag (+0x5c bit 6, 0x40) and available
//      units (slot.count(+0x8) - slot.reserved(+0xC) > 0); then refill
//      heatRemaining = Time(desc->fuelBurnDuration(+0x2C)), subtract 1 unit
//      (0x14012d150), bump the town per-category consumed counter
//      (townMgr(+0xf0)+0x170 row, cat via 0x140088f90, +0xC field), break.
//
// STANDALONE: branch 2 (chimney audio) and 4 (abandon/ClearStorage, needs the
// un-ported 0x140003050 finder chain) are skipped. Branch 3 is LIVE: it is NOT
// a mesh op — 0x14012c3f0 is StatusIcon_Toggle (statusiconcomponent.cpp) and
// 0x140002f90 is the StatusIconComponent finder (Ghidra misnames both as
// MeshInstance ops). It CLEARS icon bit 6 (no-food) while the house has edible
// stock and bit 7 (no-firewood / cold) while it has fuel — the SET side lives
// in the Gather/Gather2 no-target branches (citizenai_gather.cpp). Branches
// 1 + 3 + 5 together close the retail warm/cold loop the building status icon
// displays.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- resolved helpers ------------------------------------------------------
// Entity_FindComponent_Storage (REAL storage finder; NOT the misnamed
// 0x140008090 residence finder) — writes an IBP holding the StorageComponent*.
typedef void* (*fn_FindStorage_t)(int64_t entity, void* outIBP);       // 0x140006270
// StorageComponent slot subtract: (storageComp, stackPtr, amount).
typedef void  (*fn_StorageSubtract_t)(void* storage, void* stack, int amount); // 0x14012d150
// Resource-category index for the townMgr+0x170 counter rows.
typedef int   (*fn_ResCategory_t)(void* storage, void* stack);         // 0x140088f90

static fn_FindStorage_t     s_fnFindStorage  = nullptr;
static fn_StorageSubtract_t s_fnSubtract     = nullptr;
static fn_ResCategory_t     s_fnCategory     = nullptr;
static uint32_t*            s_tidResidenceDescription = nullptr;       // DAT_1401c7d28

// Branch-3 status-icon deps (all registered reimpls):
typedef int   (*fn_GetResCount_t)(int64_t residenceComp);   // 0x14012a9d0 edible / 0x14012ab00 fuel
typedef void* (*fn_FindStatusIcon_t)(int64_t entity, void* outIBP);   // 0x140002f90 (StatusIconComponent)
typedef void  (*fn_StatusIconToggle_t)(void* comp, int bit, char set); // 0x14012c3f0
static fn_GetResCount_t      s_fnEdibleCount = nullptr;   // 0x14012a9d0
static fn_GetResCount_t      s_fnFuelCount   = nullptr;   // 0x14012ab00
static fn_FindStatusIcon_t   s_fnFindIcon    = nullptr;   // 0x140002f90
static fn_StatusIconToggle_t s_fnIconToggle  = nullptr;   // 0x14012c3f0
static bool s_resolved = false;

static void EnsureFns() {
    if (s_resolved) return;
    s_fnFindStorage = reinterpret_cast<fn_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_fnSubtract    = reinterpret_cast<fn_StorageSubtract_t>(ResolveAddr(Module::Exe, 0x14012d150));
    s_fnCategory    = reinterpret_cast<fn_ResCategory_t>(ResolveAddr(Module::Exe, 0x140088f90));
    s_tidResidenceDescription =
        reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d28));
    s_fnEdibleCount = reinterpret_cast<fn_GetResCount_t>(ResolveAddr(Module::Exe, 0x14012a9d0));
    s_fnFuelCount   = reinterpret_cast<fn_GetResCount_t>(ResolveAddr(Module::Exe, 0x14012ab00));
    s_fnFindIcon    = reinterpret_cast<fn_FindStatusIcon_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_fnIconToggle  = reinterpret_cast<fn_StatusIconToggle_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_resolved = true;
}

static bool HeatTraceOn() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_HEAT_TRACE");
        on = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on == 1;
}

// Signature matches the Trans38 dispatcher tail-call: (component, sm, Time* dt).
extern "C" char ResidenceComponent_HeatTick(void* selfV, void* /*sm*/, void* dtP) {
    EnsureFns();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    if (!self) return 1;

    // 1. Heat countdown -= frame dt (unconditional, every tick).
    if (dtP) {
        *reinterpret_cast<int64_t*>(self + 0x48) -= *reinterpret_cast<int64_t*>(dtP);
    }

    // [2 (audio) and 4 (abandon) skipped — see file header.]

    // 3. Status-icon upkeep: while the house HAS food, clear icon bit 6
    //    (no-food); while it HAS firewood, clear bit 7 (no-firewood / cold).
    //    Faithful to the decompile: FUN_14012c3f0(*finder2f90(owner), bit, 0).
    if (s_fnFindIcon && s_fnIconToggle && fn_InternalBasePtr_CheckPtr) {
        if (s_fnEdibleCount && s_fnEdibleCount(static_cast<int64_t>(self)) > 0) {
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
            int64_t iconIBP = 0;
            s_fnFindIcon(*reinterpret_cast<int64_t*>(self + 0x10), &iconIBP);
            fn_InternalBasePtr_CheckPtr(&iconIBP);
            if (iconIBP) s_fnIconToggle(reinterpret_cast<void*>(iconIBP), 6, 0);
        }
        if (s_fnFuelCount && s_fnFuelCount(static_cast<int64_t>(self)) > 0) {
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
            int64_t iconIBP = 0;
            s_fnFindIcon(*reinterpret_cast<int64_t*>(self + 0x10), &iconIBP);
            fn_InternalBasePtr_CheckPtr(&iconIBP);
            if (iconIBP) s_fnIconToggle(reinterpret_cast<void*>(iconIBP), 7, 0);
        }
    }

    // 5. Burn branch.
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t wsim = *reinterpret_cast<uintptr_t*>(gw + 0x130);   // weather sim ptr
    if (!wsim || !fn_IsDerivedFrom || !s_tidResidenceDescription) return 1;
    float worldTemp = *reinterpret_cast<float*>(wsim + 0x18);

    void* descObj = *reinterpret_cast<void**>(self + 0x18);
    if (!descObj || !fn_IsDerivedFrom(descObj, *s_tidResidenceDescription)) return 1;
    uintptr_t desc = reinterpret_cast<uintptr_t>(descObj);
    float heatThreshold = *reinterpret_cast<float*>(desc + 0x28);

    if (HeatTraceOn()) {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[HeatTick] house=%p temp=%.2f thresh=%.2f burnDur=%.2f "
                    "heatLeft=%lld slots=%d\n", selfV, worldTemp, heatThreshold,
                    *reinterpret_cast<float*>(desc + 0x2c),
                    (long long)*reinterpret_cast<int64_t*>(self + 0x48),
                    *reinterpret_cast<int*>(self + 0x58));
            fflush(stderr);
        }
    }

    if (worldTemp >= heatThreshold) return 1;
    if (*reinterpret_cast<int64_t*>(self + 0x48) >= 0) return 1;
    if (*reinterpret_cast<int*>(self + 0x58) <= 0) return 1;

    // Find the house StorageComponent.
    if (!s_fnFindStorage || !fn_InternalBasePtr_CheckPtr) return 1;
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    int64_t storeIBP = 0;
    s_fnFindStorage(*reinterpret_cast<int64_t*>(self + 0x10), &storeIBP);
    fn_InternalBasePtr_CheckPtr(&storeIBP);
    uintptr_t store = static_cast<uintptr_t>(storeIBP);
    if (!store) return 1;

    int slotCount = *reinterpret_cast<int*>(store + 0x30);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(store + 0x28);
    bool burned = false;
    for (int i = 0; i < slotCount; ++i) {
        uintptr_t slot  = slots + static_cast<uintptr_t>(i) * 0x10;
        uintptr_t stack = *reinterpret_cast<uintptr_t*>(slot);
        if (!stack) continue;   // guard (retail assumes every slot has a stack)

        // Find the stack's RawMaterialDescription (GetTypeId via vtable+0x10,
        // then IsDerivedFrom — same idiom as fn_StorageComponent_QueryCount).
        void* mat = nullptr;
        if (g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription) {
            int compCount = *reinterpret_cast<int*>(stack + 0x30);
            uintptr_t comps = *reinterpret_cast<uintptr_t*>(stack + 0x28);
            for (int j = 0; j < compCount; ++j) {
                void* comp = *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(j) * 8);
                if (!comp) continue;
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                if (tid == *g_TypeId_RawMaterialDescription) {
                    if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) mat = comp;
                    break;
                }
            }
        }
        if (!mat) continue;     // guard (retail would deref null +0x5c)

        uint32_t matFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(mat) + 0x5c);
        int avail = *reinterpret_cast<int*>(slot + 8) - *reinterpret_cast<int*>(slot + 0xc);
        if (((matFlags >> 6) & 1) != 0 && avail > 0) {
            // Refill the countdown to the description burn duration, then burn 1.
            int64_t t = 0;
            if (fn_Time_Ctor_Float)
                fn_Time_Ctor_Float(&t, *reinterpret_cast<float*>(desc + 0x2c));
            *reinterpret_cast<int64_t*>(self + 0x48) = t;

            if (s_fnSubtract)
                s_fnSubtract(reinterpret_cast<void*>(store), reinterpret_cast<void*>(stack), 1);

            int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);
            if (s_fnCategory && townMgr) {
                int cat = s_fnCategory(reinterpret_cast<void*>(store),
                                       reinterpret_cast<void*>(stack));
                if (cat >= 0) {
                    // +0x170 holds a POINTER to the counter rows (0x20 stride,
                    // consumed count at +0xC) — same deref as the eat loop.
                    int64_t rows = *reinterpret_cast<int64_t*>(townMgr + 0x170);
                    if (rows)
                        *reinterpret_cast<int*>(rows + static_cast<int64_t>(cat) * 0x20 + 0xc) += 1;
                }
            }

            // One line per burn (one per house per burn-duration — low volume);
            // uncapped so long soak runs show the full consumption sawtooth.
            // (PERF 2026-09-05: gated -- 800 houses in winter made this unbuffered stderr
            // line 1.7% of the EntityList phase in the perf build.)
            if (HeatTraceOn()) {
                fprintf(stderr, "[HeatTick] house=%p BURN 1 fuel (temp=%.2f < %.2f) "
                        "slotLeft=%d refill=%.2fs\n", selfV, worldTemp, heatThreshold,
                        *reinterpret_cast<int*>(slot + 8),
                        *reinterpret_cast<float*>(desc + 0x2c));
                fflush(stderr);
            }
            burned = true;
            break;
        }
    }
    if (!burned) {
        // House is COLD with no fuel available: heatRemaining stays negative, the
        // warm-check 0x14012ac30 reads cold, and Gather2's no-target branch will
        // raise status-icon bit 7. This branch re-runs every frame while cold, so
        // throttle to ~1 line per 600 occurrences.
        static int n = 0;
        if ((n++ % 600) == 0) {
            fprintf(stderr, "[HeatTick] house=%p COLD no fuel available "
                    "(temp=%.2f < %.2f, heatLeft=%lld, storageSlots=%d)\n",
                    selfV, worldTemp, heatThreshold,
                    (long long)*reinterpret_cast<int64_t*>(self + 0x48), slotCount);
            fflush(stderr);
        }
    }
    return 1;
}
