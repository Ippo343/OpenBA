#pragma once
// DiseaseComponent / DiseaseDescription — binary-compatible struct layouts.
// Application-x64.exe, build 170910. sizeof(DiseaseComponent) = 0x58.
//
// ⚠ NAME TRAP. "DiseaseComponent" is NOT the citizen-plague component. Citizen
// illness is the `Disease` *resource* (Object::External, Game/Disease/Diseases.rsc)
// driven by CitizenComponent::UpdateHealth — see code/Game/Component/citizen_updatehealth.cpp.
// DiseaseComponent is a PER-BUILDING component implementing:
//   * crop-field / orchard INFESTATION (retail's "crop is infested" disaster), and
//   * livestock disease on a pasture (the herd dies off one animal at a time).
// The exe has no "Infest"/"Blight"/"Pest" string anywhere; the mechanic's own retail
// names are the DebugUI elements `cropDiseaseButton` and `noCropDiseaseCheck`.
//
// Vtable 0x140159290 — slots verified by READING THE VTABLE BYTES, not Ghidra's
// symbol names, which are shifted by one slot here:
//   [0] 0x140004f80 scalar dtor      [1] 0x140004da0     [2] 0x140004d90 GetTypeId
//   [3] 0x140048ca0 Serialize        (Ghidra: "..._Shutdown")
//   [4] 0x140048c50 Startup          (Ghidra: "..._Serialize")
//   [5] 0x140118a60 (base)
//   [6] 0x140048c00 OnAdded          (Ghidra: "..._Draw")   -> registry append
//   [7] 0x140048bb0 OnRemoved        (Ghidra: "..._OnCreate")-> registry erase + Cure
//   [8] 0x140118a70 (base)
//   [9] 0x140048b90 Update           -> ActiveDisease / InfectionTick dispatch
//   [10..] base slots 0x140118a90..
//
// Constructor (0x140048db0) zeroes +0x28..+0x40 (4 qwords), +0x48 (dword), +0x50
// (qword) and ORs the Component capability flags at +0x20 with 0x8B.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace DiseaseComponentAddr {
    constexpr uintptr_t Vtable            = 0x140159290;
    constexpr uintptr_t DescriptionVtable = 0x140159310;

    // Bodies (all ported in code/Game/Component/diseasecomponent.cpp).
    constexpr uintptr_t CheckCure      = 0x1400481b0;
    constexpr uintptr_t Cure           = 0x140048320;
    constexpr uintptr_t Infect         = 0x1400483a0;
    constexpr uintptr_t InfectionTick  = 0x140048610;
    constexpr uintptr_t ActiveDisease  = 0x140048840;
    constexpr uintptr_t Update         = 0x140048b90;
    constexpr uintptr_t Startup        = 0x140048c50;
    constexpr uintptr_t OnAdded        = 0x140048c00;
    constexpr uintptr_t OnRemoved      = 0x140048bb0;
    constexpr uintptr_t Serialize      = 0x140048ca0;
    constexpr uintptr_t SameCropProbe  = 0x14009fb60;  // "helper_fb60"
    constexpr uintptr_t GrownIdentity  = 0x140047f80;
    constexpr uintptr_t KillOnePlant   = 0x140047b40;  // Ghidra "AnimateNaturalResource_impl_7b40"
    constexpr uintptr_t KillOneAnimal  = 0x140047d70;
}

// ------------------------------------------------------------------
// DiseaseComponent — 0x58 bytes
// ------------------------------------------------------------------
// ENDGAME B.3.8 CIT: the POD is gone — `DiseaseComponent` is a real class now
// (code/Game/Component/DiseaseComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/DiseaseComponent.h"

// ------------------------------------------------------------------
// DiseaseDescription — 0x48 bytes (Object::External 0x20 base + params)
// ------------------------------------------------------------------
// Stream order from Serialize 0x140048e40: two ints, six floats, one int.
struct DiseaseDescription {
    uint8_t  _base[0x20];   // +0x00  Object::External

    int32_t  firstMonth;    // +0x20  earliest month index (0..11) an outbreak may start
    int32_t  lastMonth;     // +0x24  latest month index
    // ⚠ The two rotation fields are named BACKWARDS in the authored .rsc, and the
    // authored values make the mistake convincing rather than obvious — swap them and
    // the mechanic still "works" in a trace, just inverted. The probe at 0x14009fb60
    // returns TRUE when the cured crop is STILL planted, and retail 0x140048610 reads
    // +0x2c on TRUE, +0x28 on FALSE. So:
    //     +0x28 = _lowSpawnChance  = 200 years  <- crop CHANGED  (rotating is SAFER)
    //     +0x2c = _highSpawnChance = 150 years  <- crop UNCHANGED (repeating is RISKIER)
    // "low/high chance" refers to the chance, so the LOW-chance field carries the
    // LONGER interval. Both are mean YEARS, not probabilities.
    float    yearsIfChanged;// +0x28  _lowSpawnChance  — mean YEARS, crop CHANGED since cure
    float    yearsIfSame;   // +0x2c  _highSpawnChance — mean YEARS, crop UNCHANGED
    float    rollMonths;    // +0x30  months per infection roll (also scales the odds)
    float    killMonths;    // +0x34  months per kill tick (one plant / one animal)
    float    spreadMonths;  // +0x38  months per spread attempt
    float    durationMonths;// +0x3c  how long an outbreak lasts
    int32_t  spreadRadius;  // +0x40  spread offset, tiles (RandTol bound)
    uint8_t  _pad44[4];     // +0x44
};

static_assert(sizeof(DiseaseDescription) == 0x48, "DiseaseDescription size mismatch");
static_assert(offsetof(DiseaseDescription, firstMonth)     == 0x20, "");
static_assert(offsetof(DiseaseDescription, lastMonth)      == 0x24, "");
static_assert(offsetof(DiseaseDescription, yearsIfChanged) == 0x28, "");
static_assert(offsetof(DiseaseDescription, yearsIfSame)    == 0x2c, "");
static_assert(offsetof(DiseaseDescription, rollMonths)     == 0x30, "");
static_assert(offsetof(DiseaseDescription, killMonths)     == 0x34, "");
static_assert(offsetof(DiseaseDescription, spreadMonths)   == 0x38, "");
static_assert(offsetof(DiseaseDescription, durationMonths) == 0x3c, "");
static_assert(offsetof(DiseaseDescription, spreadRadius)   == 0x40, "");
