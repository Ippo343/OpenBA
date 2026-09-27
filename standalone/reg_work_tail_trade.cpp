// standalone/reg_work_tail_trade.cpp
// Keystone B tail — TradeWork (CitizenComponent work-tracking) AVL serialize cascade.
// Registers the three exe addresses ported in code/Game/Work/tradework_serialize.cpp and
// fills the one type-id .rdata global those bodies read (DWORD_1401c7158).
//
// Wired via RegisterWorkTailTradeBatch() in RegisterCodeFunctions() and
// FillTypeIdGlobals_worktail_trade() (alongside the other FillTypeIdGlobals_*) in
// code/Game/Game/game_main.cpp, AFTER Engine::Startup (Object::Factory::Startup assigns
// the real CRC type ids only then; before that GetTypeIdByName returns 0).
//
// Closure (bottom-up): 0x140025920 retrace (already registered by reg_player_trade.cpp),
// 0x140002340 ExternalPtr serialize (already registered in address_registry.cpp; called
// via fn_ExternalPtr_BuildSerialize, NOT re-registered here), static MemAlloc (export),
// InternalBasePtr CheckPtr/Serialize + IsDerivedFrom + Ser-int slot (engine_funcs).
//
// Every address verified as a real function ENTRY in Ghidra; ports are faithful 1:1
// against the disassembly (offset by offset).
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>

// Ports (code/Game/Work/tradework_serialize.cpp).
extern "C" void  TradeWork_Serialize_c8a0(int64_t* param_1, void* param_2);          // 0x14002c8a0
extern "C" void* TradeWork_Insert_c710(int64_t* param_1, void** param_2, int64_t p3); // 0x14002c710
extern "C" void* TradeWork_NodeAlloc_c830(void* unused, uint64_t* src, int64_t parent); // 0x14002c830

// TradeWork::Slot4 (0x1400f5d40) is a real virtual since ENDGAME B.3.8 and nothing read
// its address back, so its row is gone.

// Type-id read by the c8a0 IsDerivedFrom. DWORD_1401c7158 = CitizenComponent
// (CORRECTED 2026-07-19: the slot's [DATA] writer is CitizenComponent_RegisterType
// @0x14014e7b5 — Object::BaseTypeInfo(&DWORD_1401c7158, "CitizenComponent", ...).
// The earlier "CitizenAIComponent" claim came from the 0x1400097a0 finder's Ghidra
// name, which inherited the same wrong assumption. BaseTypeInfo is 0x70 bytes;
// CitizenAIComponent's block is the adjacent one at 0x1401c70e8.)
//
// SUPERSEDED 2026-08-26: the "separate, deliberate task" named at the end of this note
// HAPPENED — BANISHED_LIVE_CITIZENAI_CITID graduated to default-on, so 0x1401c7158 IS now
// registered and filled (standalone/reg_citizenai_typeids.cpp). The ~30 readers listed
// below therefore see a live CRC on a default run. The private static here is harmless and
// stays (it is the same value), but the reason given for it no longer holds. Read the
// paragraph below as history, not as current policy.
//
// IMPORTANT: this is a PRIVATE static for c8a0 only — we deliberately do NOT
// RegisterExeAddr(0x1401c7158) to the global registry. ~30 existing reimpls (completetask,
// createrawmaterial, dropoff, huntanimal, the PerformWork ctors, Entity_FindComponent_CitizenAI,
// …) read 0x1401c7158 via ResolveAddr and currently null-guard it (it is unregistered →
// they skip their type checks). Publishing a filled value here would suddenly activate all
// those checks — including un-dormanting CitizenAI component finds into still-inert
// components (Phase-1 not done for CitizenAI) — a cross-cutting regression risk that does
// not belong in a Work-gap-closure wave. Filling c7158 globally is a separate, deliberate
// task. c8a0 (a trade-serialize path autostart does not exercise) uses its own static and
// skips validation when the id is unresolved (0) — cascade-safe.
extern "C" {
uint32_t g_tid_TradeWork_CitizenComponent = 0;  // private c8a0 copy of DWORD_1401c7158
}

extern "C" void FillTypeIdGlobals_worktail_trade() {
    // "CitizenComponent", matching the retail slot value (see corrected note above);
    // was "CitizenAIComponent" — a latent wrong-id in the (autostart-unexercised)
    // c8a0 serialize validation.
    g_tid_TradeWork_CitizenComponent = ::Object::GetTypeIdByName("CitizenComponent");
}

void RegisterWorkTailTradeBatch() {
    RegisterExeAddr(0x14002c830, reinterpret_cast<uintptr_t>(&TradeWork_NodeAlloc_c830));
    RegisterExeAddr(0x14002c710, reinterpret_cast<uintptr_t>(&TradeWork_Insert_c710));
    RegisterExeAddr(0x14002c8a0, reinterpret_cast<uintptr_t>(&TradeWork_Serialize_c8a0));
    // NB: 0x1401c7158 is intentionally NOT registered here (see note above).
}
