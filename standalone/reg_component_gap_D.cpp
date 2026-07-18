// Component GAP wave — batch D (nature + economy)
// Registers the closure-safe exe helper addresses (and caller-bug-corrected
// literals) -> our clean reimplementations so the guarded ResolveAddr(Module::Exe,
// ...) call sites in code/Game/Component/ actually fire in standalone mode.
//
// The integrator wires RegisterComponentGap_D() into address_registry.cpp.
//
// Classification of all 23 batch-D gap addresses is in the agent report. Only the
// CLOSURE-SAFE / closure-safe CALLER-BUG-FIXED ones are registered here; the rest
// are documented defers (deep/unregistered sub-callee closures, vtable-emitting
// ctors, or unavailable dispatch slots). A wrong/half-null port is silent
// corruption, strictly worse than the safe no-op skip.
#include "resolve.h"
#include <cstdint>

// ---- herdcomponent.cpp ----
extern char HerdComponent_Helper6b10(int64_t ref);          // 0x140106b10
// HERD cluster (final bottom-up land): impl_e510 + its only unregistered callee
// helper_1ad0 (whose callees FUN_140003110/FUN_140005500 are registered).
extern void  HerdComponent_Impl_e510(void* memberComp, void* newHerdIbp);    // 0x14012e510
extern void  HerdComponent_Helper1ad0(int64_t herdComp, void* animalIbp);    // 0x140121ad0

// ---- tradecomponent.cpp ----
extern void TradeComponent_ResizeItems(int64_t* field, int count);  // 0x14000e8d0
// TRADE String-array cluster (final bottom-up land): name-array resize (0x20 stride)
// + its two leaf sub-callees. All bottom out in MemAlloc/MemFree/String exports.
extern void TradeComponent_ResizeNames(int64_t* field, int count);             // 0x14000e760
extern int64_t* TradeComponent_CopyPtrArray(int64_t* dst, int64_t* src);       // 0x14000e6b0
extern void TradeComponent_DtorNameArray(void* unused, int64_t base, int count);// 0x14000ef10

// ---- buildcomponent.cpp ----
extern void* BuildComponent_FindStorage26a30(int64_t compHost, void* retbuf);  // 0x140026a30

// ---- terraformcomponent.cpp ----
// CALLER-BUG fix: the .cpp literal 0x1400a4b50 was a transposed-digit typo; the
// true callee in TerraformComponent::OnCreate is CitizenAI_impl_b150 @ 0x1400ab150.
extern void TerraformComponent_RegionRecalc(void* terrainGrid, int x, int y);   // 0x1400ab150

// ---- workplacecomponent.cpp ----
// Re-verify pass: their full exe FUN_ closure became registered in this wave
// (notably 0x14012c3f0 / 0x140005780 / 0x14011e710), so both flipped from DEFER to
// closure-safe. Ported faithfully; remaining sub-calls all resolve via engine_funcs
// (Scene Add/RemoveObject + Time ctor export fallbacks, MeshInstance::Update vtable).
extern void WorkplaceComponent_Detach(int64_t comp, void* workerIbp);   // 0x14012f7e0
extern void WorkplaceComponent_Attach(int64_t comp, void* workerIbp);   // 0x14012fa20

// ---- pasturecomponent.cpp ----
// 0x140105e20 AnimalComponent_SetBounds — pure leaf (g_world+0xf8 + self+0x28, no exe
// sub-callees). Already wired at pasturecomponent.cpp (s_fn_AnimalSetBounds). Lead add.
extern void PastureComponent_AnimalSetBounds(void* self, short* x, short* y,
                                             short* w, short* h, int size);

void RegisterComponentGap_D() {
    // herdcomponent.cpp — per-animal "migration target reached?" test (leaf).
    RegisterExeAddr(0x140106b10, reinterpret_cast<uintptr_t>(&HerdComponent_Helper6b10));

    // tradecomponent.cpp — 0xc-stride item-table resize (MemAlloc/MemFree only).
    RegisterExeAddr(0x14000e8d0, reinterpret_cast<uintptr_t>(&TradeComponent_ResizeItems));

    // buildcomponent.cpp — find first StorageComponent child (FindComponent loop).
    RegisterExeAddr(0x140026a30, reinterpret_cast<uintptr_t>(&BuildComponent_FindStorage26a30));

    // terraformcomponent.cpp — region-dirty recalc (corrected from typo 0x1400a4b50).
    RegisterExeAddr(0x1400ab150, reinterpret_cast<uintptr_t>(&TerraformComponent_RegionRecalc));

    // workplacecomponent.cpp — worker detach/attach (closure-safe after this wave).
    RegisterExeAddr(0x14012f7e0, reinterpret_cast<uintptr_t>(&WorkplaceComponent_Detach));
    RegisterExeAddr(0x14012fa20, reinterpret_cast<uintptr_t>(&WorkplaceComponent_Attach));

    // pasturecomponent.cpp — animal footprint-bounds clamp (pure leaf).
    RegisterExeAddr(0x140105e20, reinterpret_cast<uintptr_t>(&PastureComponent_AnimalSetBounds));

    // tradecomponent.cpp — String name-array resize cluster (bottom-up: parent + 2 leaves).
    RegisterExeAddr(0x14000e6b0, reinterpret_cast<uintptr_t>(&TradeComponent_CopyPtrArray));
    RegisterExeAddr(0x14000ef10, reinterpret_cast<uintptr_t>(&TradeComponent_DtorNameArray));
    RegisterExeAddr(0x14000e760, reinterpret_cast<uintptr_t>(&TradeComponent_ResizeNames));

    // herdcomponent.cpp — herd member assign cluster (bottom-up: impl_e510 + helper_1ad0).
    RegisterExeAddr(0x140121ad0, reinterpret_cast<uintptr_t>(&HerdComponent_Helper1ad0));
    RegisterExeAddr(0x14012e510, reinterpret_cast<uintptr_t>(&HerdComponent_Impl_e510));
}
