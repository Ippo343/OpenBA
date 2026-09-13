// standalone/reg_w2_lane3.cpp
// `.rdata`/`.data` census wave 2, lane 3 — the five `.data` globals.
//
// Four of the five are triaged as LEAVE (see the report and the notes at the bottom
// of this file); this file registers the ONE address that is a clean, value-neutral
// restore:
//
//   0x1401c4758 — Object::BaseTypeInfo("ParticleDescription"), written by
//   ParticleDescription_Register (0x14014b540, factory_registrations.csv:45) during
//   static init and filled with the real CRC by Object::Factory::Startup. +0x00 of a
//   BaseTypeInfo is the type id, so this is 4 bytes of dword storage — the same kind
//   lane 4's 30 slots are, and it is registered the same two-phase way (storage at
//   AddressRegistryInit, value after Factory::Startup).
//
// WHY THIS IS SAFE (shared brief, step 3): the only consumer in our tree that reads
// the ADDRESS is particlecomponent.cpp:112, and it is the soft-fallback shape:
//     if (s_pParticleDescTypeId && *s_pParticleDescTypeId != 0 &&
//         *s_pParticleDescTypeId != 0xFFFFFFFF) return *s_pParticleDescTypeId;
//     cached = ::Object::GetTypeIdByName("ParticleDescription");
// i.e. it falls back BY NAME to the SAME name the binary registered, and it re-checks
// the slot's VALUE (not just the pointer), so a fill that lands late still degrades to
// the by-name path rather than returning 0. Two more files keep their own private
// by-name caches of the identical string and never touch the address at all
// (particledescription.cpp:168, tornadocomponent.cpp:1082). All three therefore already
// converge on one CRC; registering changes no value, it only removes the duplication
// and the ordering hazard.
//
// Retail reads DAT_1401c4758 from 14 sites — ParticleComponent Draw/Slot11/Slot12/
// OnRemove/sub_2480/helper_2560, the emitter start/stop/fade family (0x140053660,
// 0x140053720, 0x1400537a0, 0x1400537f0) and DiseaseComponent_Infect — so this is a
// real, hot, load-path id, not a vestigial resolve.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
// Backing storage. Left 0 until FillTypeIdGlobals_w2lane3() runs: Object::Factory::
// Startup is what assigns the CRCs, and filling at registration time would store
// 0xFFFFFFFF (the "not yet registered" sentinel), which the consumer above explicitly
// rejects anyway.
uint32_t g_tid_ParticleDescription = 0;
}  // namespace

// ── Phase 2: fill, from game_main.cpp after Object::Factory::Startup ─────────
extern "C" void FillTypeIdGlobals_w2lane3() {
    g_tid_ParticleDescription = ::Object::GetTypeIdByName("ParticleDescription");
    // Report the VALUE, not the pointer: a registered slot holding 0 is
    // indistinguishable from an unregistered one at every call site.
    if (g_tid_ParticleDescription == 0) {
        std::fprintf(stderr,
            "[W2Lane3] ERROR: GetTypeIdByName(\"ParticleDescription\") returned 0 — "
            "slot 0x1401c4758 is live but EMPTY; every reader falls back by name\n");
    }
    std::fflush(stderr);
}

// ── Phase 1: storage, from AddressRegistryInit before EngineFuncsInit ────────
void RegisterW2Lane3TypeIds() {
    RegisterExeAddr(0x1401c4758, reinterpret_cast<uintptr_t>(&g_tid_ParticleDescription));
}

// ═══════════════════════════════════════════════════════════════════════════
// DELIBERATELY NOT REGISTERED — the other four lane addresses. Each is a REAL
// defect, but none of them is fixed by registering the address, and for two of
// them registering makes things worse. Details in the lane report; summary here
// so the next sweep does not "close" them from this file.
//
// 0x1401c4138 — CreatePathDescription's BaseTypeInfo (writer CreatePathDescription_
//   RegisterType 0x14014ab25; factory_registrations.csv:17). Retail's only readers are
//   EntityUI_impl_1ee0 and CreatePathDescription_Slot2 — GameUpdateLoop::Startup is
//   NOT among them. Our gameupdateloop.cpp:663 reads it into `g_pDialogTypeId` under
//   the name "Dialog", which is a misidentification: there is no exe-side "Dialog"
//   BaseTypeInfo at all (Dialog is a Runtime-DLL class, self-registered), which is why
//   the by-name fallback at :668 works. But that fallback is installed ONLY under
//   `if (!g_pDialogTypeId)`. Registering this address makes the pointer non-null
//   holding CreatePathDescription's CRC, the fallback stops firing, and
//   IsDerivedFrom(Dialog/Toolbar.rsc, CreatePathDescriptionId) at :2394 fails — i.e.
//   NO TOOLBAR. Fix the code first (drop the ResolveAddr, always resolve "Dialog" by
//   name); that is a code/Game edit and is described in the report, not made here.
//   Our own CreatePathDescription port keeps a private by-name cache
//   (createpathdescription.cpp:50), so the slot has no legitimate reader either.
//
// 0x1401c72e8 — NOT A TYPE ID, and registering 4 bytes of dword storage here would be
//   the "wrong KIND" failure the brief warns about. It is the Description
//   BaseTypeInfo's +0x08 field: Description_Register (0x14014ea75) constructs the
//   BaseTypeInfo at 0x1401c72e0, and the exe has an adjacent pair of one-instruction
//   accessor thunks — 0x140031c50 `MOV EAX,dword [0x1401c72e0]` (the dword type id)
//   and 0x140031c60 `MOV RAX,qword [0x1401c72e8]` (a QWORD, reached from the Component
//   base vtable slot at 0x1401629a8). 0x140031c60 is this address's ONLY xref.
//   THE REAL DEFECT: three of our files read it as a uint32_t type id named
//   `TypeInfo_ComponentEntity` and, because it resolves null, silently SKIP their
//   IsDerivedFrom validation:
//       code/Game/Component/multiobjectcomponent.cpp:214  (guard at :103)
//       code/Game/Component/nomadcomponent.cpp:507        (guard at :91)
//       code/Game/Component/pickingcomponent.cpp:324      (guard at :71)
//   Retail's counterparts read 0x1401c5d70 = g_BaseTypeInfo_ComponentEntity —
//   confirmed by Ghidra xref, not by name: MultiObjectComponent_Controller_Slot13
//   reads it at 0x140125936, NomadComponent_Controller_Slot13 at 0x14004fca5,
//   PickingComponent_Shutdown at 0x140128bf9, and those are exactly the three retail
//   functions those files port. 0x1401c5d70 is ALREADY registered and filled
//   (reg_action_child_ctors2.cpp). The fix is a one-token address change in those
//   three files — a code/Game edit, described in the report.
//
// 0x1401c42c0 (HappinessComponent) / 0x1401c70e8 (CitizenAIComponent) — ALREADY
//   registered by reg_citizenai_typeids.cpp:83/85, but behind the still-opt-in gates
//   BANISHED_LIVE_CITIZENAI_HAPPYID / _AIID (only the third of that trio, _CITID, has
//   graduated into the default-on table). That is why they still appear in the census.
//   Registering them from here would create a SECOND storage dword for the same
//   address and split the slot on init order — the trap reg_food_typeid.cpp calls out
//   for RadiusDescription. Graduating the two gates is that wave's call.
