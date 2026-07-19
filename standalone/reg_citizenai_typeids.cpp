// CitizenAI type-id wave — the deliberate global fill of the three "msg-id" .rdata
// slots that every prior wave left 0 + null-guarded (openfuncs step 2 remainder).
//
// Ghidra ground truth (2026-07-19; via each slot's *_RegisterType [DATA] writer):
// all three are Object::BaseTypeInfo BASE addresses (+0x00 = the CRC type id,
// BaseTypeInfo is 0x70 bytes — 0x1401c7158 is NOT CitizenAIComponent+0x70):
//   * DWORD_1401c70e8 = BaseTypeInfo("CitizenAIComponent") — CitizenAIComponent_RegisterType
//       (0x14014e735). Read by ~20 work-action ctors as the SendComponentMessage
//       "msgId": SendComponentMessage_Action_CopyCtor (0x1400cb1d0, registered in
//       reg_pf_ctors.cpp) copies exactly ONE dword (*param_3 -> action+0x30) — it is
//       a component-TYPE selector for message delivery, not a message code.
//   * DWORD_1401c7158 = BaseTypeInfo("CitizenComponent") — CitizenComponent_RegisterType
//       (0x14014e7b5). Read by the Action/Work Serialize type-checks
//       (IsDerivedFrom(entity, *c7158) = "is this a citizen entity") and by
//       Entity_FindComponent_Citizen (0x1400097a0). Earlier waves assumed
//       CitizenAIComponent — see the corrected note in reg_work_tail_trade.cpp.
//   * DAT_1401c42c0  = BaseTypeInfo("HappinessComponent") — HappinessComponent_RegisterType
//       (0x14014ace5). Read by the Leisure find scans (the CitizenAI ports keep
//       private GetTypeIdByName copies, so THEY work either way; this global serves
//       the other readers: TradeComponent_impl_f760, PlayAnimation_Action_Ctor4,
//       HappinessComponent_Controller_Slot12).
//
// GATED ON LIVE OPT-IN (registration-gated like reg_component_spawn): a default
// (env-off) run — including the standard `make sa-run` env set — is byte-for-byte
// unchanged. Rationale: with the gate ON,
// ~30 currently-dormant readers activate at once — Serialize IsDerivedFrom checks
// (autostart-inert, save/load-correct: see the 2026-07-19 reader sweep — every
// c7158 Action/Work consumer is Serialize/Validate-side) AND the SendComponentMessage
// action ctors, whose constructed actions then carry a real target-type id when they
// execute. The receive side (CitizenAIComponent message handling) is Phase-1
// territory; A/B the gate to observe. When off, ResolveAddr(0x1401cXXXX) stays 0
// and every reader keeps its null-guarded skip, exactly as before this wave.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include "Component/component_factory_base.h" // ComponentFactory::LiveEnabled
#include <cstdint>

// Backing storage (filled after Engine::Startup — Object::Factory::Startup assigns
// the real CRC ids only then; filling at registration time would store 0xFFFFFFFF).
static uint32_t g_tid_CitizenAIComponent = 0;  // 0x1401c70e8
static uint32_t g_tid_CitizenComponent   = 0;  // 0x1401c7158
static uint32_t g_tid_HappinessComponent = 0;  // 0x1401c42c0

extern "C" void FillTypeIdGlobals_citizenai() {
    // No-op unless the storage was registered (gate on); harmless otherwise.
    g_tid_CitizenAIComponent = ::Object::GetTypeIdByName("CitizenAIComponent");
    g_tid_CitizenComponent   = ::Object::GetTypeIdByName("CitizenComponent");
    g_tid_HappinessComponent = ::Object::GetTypeIdByName("HappinessComponent");
}

void RegisterCitizenAITypeIds() {
    // Per-global gates, staged by blast radius (2026-07-19 reader sweep): c42c0 has ONE
    // .rdata reader (actionctors happiness message target); c70e8 feeds the ~20
    // SendComponentMessage action-ctor target slots + the profession-UI finder; c7158
    // has ~30 readers incl. the Serialize IBP-validators (which CLEAR refs on
    // IsDerivedFrom mismatch — verify vtable GetTypeId returns the same CRC as
    // GetTypeIdByName before enabling around save/load) and the child-birth
    // FindCompByTypeId (citizencomponent.cpp:1906). BANISHED_LIVE_CITIZENAI_TIDS (or
    // the global BANISHED_LIVE_COMPONENTS, via LiveEnabled) turns on all three.
    // NOTE: deliberately NOT "BANISHED_LIVE_CITIZENAI" — the Makefile's sa-run already
    // sets that env for the handler/statemachine bring-up; reusing it would silently
    // flip these fills on for every standard test run.
    const bool all = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZENAI_TIDS");
    if (all || ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZENAI_HAPPYID"))
        RegisterExeAddr(0x1401c42c0, reinterpret_cast<uintptr_t>(&g_tid_HappinessComponent));
    if (all || ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZENAI_AIID"))
        RegisterExeAddr(0x1401c70e8, reinterpret_cast<uintptr_t>(&g_tid_CitizenAIComponent));
    if (all || ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZENAI_CITID"))
        RegisterExeAddr(0x1401c7158, reinterpret_cast<uintptr_t>(&g_tid_CitizenComponent));
}
