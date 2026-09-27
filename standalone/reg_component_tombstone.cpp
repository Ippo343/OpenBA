// Component registration wave — batch tombstone (Phase-1 live bring-up).
// Registers the exe addresses that TombstoneComponent's live Update/Startup path
// references so the guarded ResolveAddr(Module::Exe, ...) call sites in
// code/Game/Component/tombstonecomponent.cpp actually resolve.
//
// The integrator wires RegisterComponentBatch_tombstone() into address_registry.cpp
// and FillComponentTypeIdGlobals_tombstone() into game_main.cpp (after Engine::Startup).
//
// NOTE: every behavioral helper TombstoneComponent uses is already registered by an
// earlier wave — MarkForRemoval 0x1400c2ea0 + ArrayPush 0x140003110 (reg_action_hunt),
// Component::Serialize 0x1401189b0 (reg_component_economy), the IO::Serializer dispatch
// slots + secs-per-tick 0x140172234 (address_registry / reg_component_nature), the
// game-world singleton 0x1401c3dd8 (address_registry), and RandFloat via the engine
// func fn_Random_RandFloat. The ONLY thing missing is the TombstoneDescription type-id
// .rdata global, registered + filled below.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/TombstoneDescription.h"

// ------------------------------------------------------------------
// TombstoneDescription type-ID .rdata global (DAT_1401c4a68).
// TombstoneComponent::OnCreate (0x140057860) does IsDerivedFrom(_unk18, *0x1401c4a68)
// to validate the description before reading its [minTime,maxTime] lifetime fields. In
// the original exe this global is .data filled by TombstoneDescription_Register
// (0x14014b8e5 writes it). In standalone WE provide the storage (registered below at
// AddressRegistryInit) and FILL it after Engine::Startup — Object::GetTypeIdByName is a
// registration-list lookup, so it returns 0 until the type tree is populated. See
// FillComponentTypeIdGlobals_tombstone(), called from game_main.cpp after the
// Engine::Startup type-id probe (mirrors FillComponentTypeIdGlobals_nature). The live
// Startup body-init TRUSTS _unk18 directly, so it is robust even if IsDerivedFrom is
// unreliable in standalone, but the type-id global is registered for the faithful
// DLL-mode Serialize path and for completeness.
static uint32_t g_tid_TombstoneDescription = 0;  // 0x1401c4a68

extern "C" void FillComponentTypeIdGlobals_tombstone() {
    g_tid_TombstoneDescription =
        TombstoneDescription::TypeId();
}

void RegisterComponentBatch_tombstone() {
    // TombstoneDescription type-id global (filled after Engine::Startup — see above).
    RegisterExeAddr(0x1401c4a68, reinterpret_cast<uintptr_t>(&g_tid_TombstoneDescription));
}
