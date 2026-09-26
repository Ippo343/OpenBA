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
struct DiseaseComponent {
    void*    vtable;        // +0x00
    void*    next;          // +0x08  (Component base link)
    void*    entity;        // +0x10  owning entity (InternalBasePtr storage)
    void*    description;   // +0x18  DiseaseDescription*
    uint32_t flags;         // +0x20  Component capability flags (ctor ORs 0x8B)
    uint8_t  _pad24[4];     // +0x24

    // All four are System::Time (int64 ticks). A Time ctor writes 8 BYTES — never
    // narrow any of these to int32.
    int64_t  rollAccum;     // +0x28  time accumulated toward the next infection roll
    int64_t  duration;      // +0x30  infestation time REMAINING (>0 == infested)
    int64_t  killAccum;     // +0x38  time accumulated toward the next kill tick
    int64_t  spreadAccum;   // +0x40  time accumulated toward the next spread attempt

    int32_t  isPasture;     // +0x48  1 when the entity has a PastureComponent
    uint8_t  _pad4C[4];     // +0x4C
    uint64_t curedIdentity; // +0x50  ExternalPtr: what was growing at the last Cure.
                            //        The same-crop probe compares tile cells to this.
};

static_assert(sizeof(DiseaseComponent) == 0x58, "DiseaseComponent size mismatch");
static_assert(offsetof(DiseaseComponent, vtable)        == 0x00, "");
static_assert(offsetof(DiseaseComponent, next)          == 0x08, "");
static_assert(offsetof(DiseaseComponent, entity)        == 0x10, "");
static_assert(offsetof(DiseaseComponent, description)   == 0x18, "");
static_assert(offsetof(DiseaseComponent, flags)         == 0x20, "");
static_assert(offsetof(DiseaseComponent, rollAccum)     == 0x28, "");
static_assert(offsetof(DiseaseComponent, duration)      == 0x30, "");
static_assert(offsetof(DiseaseComponent, killAccum)     == 0x38, "");
static_assert(offsetof(DiseaseComponent, spreadAccum)   == 0x40, "");
static_assert(offsetof(DiseaseComponent, isPasture)     == 0x48, "");
static_assert(offsetof(DiseaseComponent, curedIdentity) == 0x50, "");

// ------------------------------------------------------------------
// DiseaseDescription — ENDGAME B.3.8 K1: a real class now (B8-4, one
// definition per name). The field names and the +0x28/+0x2c rotation-name
// warning moved with it to code/Game/Component/DiseaseDescription.h.
// ------------------------------------------------------------------
#include "Component/DiseaseDescription.h"
