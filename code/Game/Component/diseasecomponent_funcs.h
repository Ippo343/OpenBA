#pragma once
// Declarations for the ported DiseaseComponent bodies
// (code/Game/Component/diseasecomponent.cpp).
//
// ⚠ DiseaseComponent is NOT citizen sickness. Citizen illness is the `Disease`
// resource driven by CitizenComponent::UpdateHealth (citizen_updatehealth.cpp).
// DiseaseComponent is the per-BUILDING component behind retail Banished's crop
// INFESTATION disaster — attached to crop fields, orchards and pastures:
//   * a field becomes infested (seasonal roll, weighted by whether the crop was
//     rotated since the last cure),
//   * its plants die one at a time with their yield zeroed — or, on a pasture,
//     its animals die one at a time,
//   * it spreads to nearby entities growing the SAME crop,
//   * harvesting / clearing / re-cropping the field cures it.
//
// Vtable 0x140159290. Size 0x58. Slot numbering was taken from the VTABLE BYTES,
// not Ghidra's symbol names, which are shifted by one slot for this class:
//   [3] Serialize 0x140048ca0   [4] Startup 0x140048c50   [6] OnAdded 0x140048c00
//   [7] OnRemoved 0x140048bb0   [9] Update  0x140048b90
//
// All bodies are gated on BANISHED_LIVE_CROPDISEASE (opt-in; unproven feature, so
// deliberately NOT in the default-on table).

#include "DiseaseComponent.h"
#include <cstdint>

// --- vtable slots -----------------------------------------------------------
extern "C" void DiseaseComponent_Update(DiseaseComponent* self, int64_t* deltaTime); // 0x140048b90
extern "C" void DiseaseComponent_Startup(DiseaseComponent* self);                    // 0x140048c50
extern "C" void DiseaseComponent_OnAdded(DiseaseComponent* self);                    // 0x140048c00
extern "C" void DiseaseComponent_OnRemoved(DiseaseComponent* self);                  // 0x140048bb0
extern "C" void DiseaseComponent_Serialize(DiseaseComponent* self, void* stream);    // 0x140048ca0

// --- behaviour bodies -------------------------------------------------------
extern "C" bool DiseaseComp_CheckCure(DiseaseComponent* self);                        // 0x1400481b0
extern "C" void DiseaseComp_Cure(DiseaseComponent* self);                             // 0x140048320
extern "C" void DiseaseComp_Infect(DiseaseComponent* self);                           // 0x1400483a0
extern "C" void DiseaseComp_InfectionTick(DiseaseComponent* self, int64_t* deltaTime);// 0x140048610
extern "C" void DiseaseComp_ActiveDisease(DiseaseComponent* self, int64_t* deltaTime);// 0x140048840
extern "C" void DiseaseComp_KillOnePlant(DiseaseComponent* self);                     // 0x140047b40
extern "C" void DiseaseComp_KillOneAnimal(DiseaseComponent* self);                    // 0x140047d70

// --- shared leaves ----------------------------------------------------------
// "Is the crop recorded at the last cure still planted on my tiles?" — the
// crop-rotation mechanic. Compares tile cell+0x10 (the grown-resource identity
// raster) across the footprint.
extern "C" bool  DiseaseComp_SameCropProbe(void* map, int x, int y, int w, int h,
                                           int64_t wantIdentity);                     // 0x14009fb60
// What this entity is currently growing (pasture livestock / tile plant / the crop
// the field is merely SET to). `entityIBPSlot` points at a slot holding the entity.
extern "C" void* DiseaseComp_GrownIdentity(uint64_t* out, void* entityIBPSlot);       // 0x140047f80
// DiseaseComponent sibling finder (only exe caller is the spread block).
extern "C" void* DiseaseComp_FindSibling(int64_t entity, void* outIBP);               // 0x1400050a0
// ParticleComponent: fade every emitter OUT (sign-flipped fade rate). Sibling of
// ParticleComponent_StartEmitterFade (0x140053660) in particlecomponent.cpp.
extern "C" void  ParticleComponent_StopEmitterFade(void* self);                       // 0x1400537f0

// Resolve the leaves this module calls (safe to call more than once).
void DiseaseComponent_InitFuncs();
