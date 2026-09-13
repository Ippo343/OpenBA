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
// REGISTRATION-GATED (like reg_component_spawn): when a gate is off, ResolveAddr of that
// slot stays 0 and every reader keeps its null-guarded skip.
//
// c7158 GRADUATED 2026-08-26 — BANISHED_LIVE_CITIZENAI_CITID is in the default-on table, so it
// is ON by default and the env var is now a KILL-SWITCH (=0). A default run is therefore
// NO LONGER byte-for-byte unchanged at this slot: the ~30 c7158 readers see a real CRC.
// That is the same CRC several of them already reached by private GetTypeIdByName copies,
// which is why the A/B showed no behavioural delta beyond the slot ceasing to be
// UNRESOLVED. See the graduation note in component_factory_base.cpp for the measurement
// and for what it does not cover (no save/load in the soak).
//
// c70e8 and c42c0 REMAIN OPT-IN. Turning c70e8 on activates ~20 SendComponentMessage
// action ctors, whose constructed actions then carry a real target-type id when they
// execute; the receive side (CitizenAIComponent message handling) is Phase-1 territory.
// A/B those gates separately before graduating either.
#include "resolve.h"
#include "Object/factory_registration.h"      // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

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

    // Report the OUTCOME, not the intent: for each slot print whether the registry
    // actually resolves it (gate on) and what a reader will now see there. A slot that
    // says reg=0 is still dormant no matter what the gate string was.
    auto slot = [](uintptr_t a) { return ResolveAddr(Module::Exe, a); };
    fprintf(stderr, "[CITID] c70e8(CitizenAI) reg=%p val=%u | c7158(Citizen) reg=%p val=%u"
                    " | c42c0(Happiness) reg=%p val=%u\n",
            (void*)slot(0x1401c70e8), g_tid_CitizenAIComponent,
            (void*)slot(0x1401c7158), g_tid_CitizenComponent,
            (void*)slot(0x1401c42c0), g_tid_HappinessComponent);
    fflush(stderr);
}

void RegisterCitizenAITypeIds() {
    // Per-global gates, staged by blast radius (2026-07-19 reader sweep): c42c0 has ONE
    // .rdata reader (actionctors happiness message target); c70e8 feeds the ~20
    // SendComponentMessage action-ctor target slots + the profession-UI finder; c7158
    // has ~30 readers incl. the Serialize IBP-validators (which CLEAR refs on
    // IsDerivedFrom mismatch — verify vtable GetTypeId returns the same CRC as
    // GetTypeIdByName before enabling around save/load) and the child-birth
    // FindCompByTypeId (citizencomponent.cpp:1906).
    //
    // ENDGAME C.2 (2026-09-13): c7158 graduated (it had been default-on). The other
    // two fills (c42c0 HappinessComponent, c70e8 CitizenAIComponent) were opt-in and
    // OFF on every default run, so they are deleted with their flags rather than
    // switched on untested; their storage globals stay for when they are brought up.
    RegisterExeAddr(0x1401c7158, reinterpret_cast<uintptr_t>(&g_tid_CitizenComponent));
}
