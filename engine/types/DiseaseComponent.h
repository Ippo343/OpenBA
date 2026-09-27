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

// ------------------------------------------------------------------
// DiseaseComponent — 0x58 bytes
// ------------------------------------------------------------------
// ENDGAME B.3.8 CIT: the POD is gone — `DiseaseComponent` is a real class now
// (code/Game/Component/DiseaseComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/DiseaseComponent.h"

// ------------------------------------------------------------------
// DiseaseDescription — ENDGAME B.3.8 K1: a real class now (B8-4, one
// definition per name). The field names and the +0x28/+0x2c rotation-name
// warning moved with it to code/Game/Component/DiseaseDescription.h.
// ------------------------------------------------------------------
#include "Component/DiseaseDescription.h"
