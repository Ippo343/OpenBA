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
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// ---- herdcomponent.cpp ----
extern char HerdComponent_Helper6b10(int64_t ref);          // 0x140106b10

// ---- tradecomponent.cpp ----
extern void TradeComponent_ResizeItems(int64_t* field, int count);  // 0x14000e8d0

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
}
#endif // STANDALONE_EXE
