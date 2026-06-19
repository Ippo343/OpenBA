// Component GAP wave — batch C (visuals: particle/tornado/fire/decal/statusicon/ambient)
// Registers the closure-safe exe-helper addresses from batch C so the guarded
// ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ actually fire.
//
// The integrator wires RegisterComponentGap_C() into address_registry.cpp.
//
// Only TWO of batch C's 19 gap addresses are closure-safe leaves; the rest are
// complex defers (self-resolving passthroughs, uninitialized audio/scene/grid
// subsystems, or deep unregistered closures) or caller-bug fixes that don't need
// a new registration. See the batch C report for the full classification.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// ---- decalcomponent.cpp ----
// 0x1400b0d00 — TerrainDecalSystem release-one-slot (shared decal/radius/placement
// helper). Closure-safe: only sub-callees 0x14001ee40 / 0x14001c890 (both already
// registered) + MemFree. DecalComponent::OnCreate guards terrainDecalSys non-null.
extern void DecalComponent_TerrainDecalRemove(void* terrainDecalSys, int decalId);

// ---- statusiconcomponent.cpp ----
// 0x1400aa440 — StatusIcon geometry-bounds setup. Closure-safe leaf: writes geometry
// fields + one Graphics::Geometry::SetBounds (Runtime export, no exe sub-callees).
extern void StatusIconComponent_GeometrySetup(void* geometry, float param2, int spriteCount,
                                              void* param4, void* spriteSheet);

void RegisterComponentGap_C() {
    RegisterExeAddr(0x1400b0d00, reinterpret_cast<uintptr_t>(&DecalComponent_TerrainDecalRemove));
    RegisterExeAddr(0x1400aa440, reinterpret_cast<uintptr_t>(&StatusIconComponent_GeometrySetup));
}
#endif // STANDALONE_EXE
