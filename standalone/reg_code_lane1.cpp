// standalone/reg_code_lane1.cpp
// ============================================================================
// Unported exe `.text` wave — LANE 1 registration.
// Session 2026-09-04, branch wave/upgrade-path-complete.
// Shared brief: docs/waves/code_fns_wave.md; lane list: docs/waves/lanes/code_lane1.txt.
//
// Lane 1 audited 9 addresses. EXACTLY ONE is a real gap and is registered below.
// The other eight are documented in the lane report; the short form, so nobody
// re-opens them from this file alone:
//
//   0x140002280  FindComponent<AmbientEmitterComponent>  — DELIBERATE, LEAVE.
//                The port exists and is correct (entity_finders_disaster.cpp) but
//                arming it crashes: retail CitizenComponent_helper_a710
//                (0x14012a710) does `lVar8 = *(qword*)(emitter+0x38);
//                if (*(uint*)(lVar8+0x10) == 0)` with NO guard, so an emitter
//                component whose array at +0x38 was never allocated faults on
//                READ of 0x10. Measured 2026-08-30. NOTE the reason recorded in
//                entity_finders_disaster.cpp:50 is STALE — it blames
//                "AmbientEmitterComponent::OnAdded (0x140042e10) is a no-op stub",
//                but that OnAdded has been live and default-on since 1b7a50b
//                (2026-06-30), i.e. it was already live when the crash was
//                measured. The real gap is narrower: Draw/OnAdded only resizes
//                +0x38 when GetBuildingData() finds an AmbientEmitterDescription,
//                so buildings without one keep +0x38 null.
//   0x140004920  CreateFencedDescription::CreateInPlace — DEAD as an address.
//                Retail's only xrefs are DATA (the RegisterType table at
//                0x14014aac4/ad9 and the vtable at 0x1401591e8); we already own
//                both (createfenceddescription.cpp REGISTER_TYPE + vtable slot 9).
//                engine_funcs' fn_CreateFencedDescription_Ctor has zero callers.
//   0x1400192a0  ExternalPtr<EnvironmentDescription>::Serialize — ALREADY DONE
//                this session in reg_unreg_lane1.cpp:220. The lane list was cut
//                from a census taken before that commit.
//   0x14001b1d0  ExternalRef lazy resolve — DEAD as an address. Every caller uses
//                the inline ResolveExternalByName_Impl (engine_funcs.h:1666);
//                engine_funcs' `fn_StringHash` is both unused and a misnomer.
//   0x14001c3c0  MusicList entry-array resize — null-guard-and-FALLBACK, and
//                musiclist.cpp's MusicList_ResizeEntries_Fallback was diffed
//                against the retail body and is equivalent. Sole retail caller is
//                MusicList::Serialize (0x1400a3e90).
//   0x14001fdb0  Struct60B sub-array deep copy, stride 0x14 — same: guarded, and
//   0x14001fe60  Struct60B sub-array deep copy, stride 0x02 — serialize_struct60b.cpp's
//                SubArrayDeepCopy was diffed against both retail bodies and matches.
//   0x140021790  Toolbar item introsort by +0x50 — already replaced by an inline
//                insertion sort (toolbardialog.cpp:1235); fn_ToolbarSort is unused.
//
// The integrator wires RegisterCodeLane1Batch() into address_registry.cpp.
// ============================================================================

#include "resolve.h"
#include <cstdint>

// code/Game/Game/unreg_lane1_mapgenerator_dtorbody.cpp
extern "C" void MapGenerator_DtorBody_1aea0(void* self);   // FUN_14001aea0

extern "C" void RegisterCodeLane1Batch() {
    // ------------------------------------------------------------------
    // 0x14001aea0 — MapGenerator::~MapGenerator (destructor body)
    // ------------------------------------------------------------------
    // BROKEN WITH IT UNREGISTERED: mapgenerator.cpp:74 guards the call
    // (`if (s_fn_DtorBody) s_fn_DtorBody(self);`), so the entire body was
    // skipped. That leaked 3 Strings + 7 dynamic arrays + the +0x60 buffer on
    // every MapGenerator teardown, and — the part that is not just a leak —
    // MapGenerator_Cleanup (0x14001ae30) is dtor(self,0) followed by
    // MapGenerator_Init(self), so a reset-in-place was overwriting live array
    // headers instead of releasing them first.
    //
    // EVIDENCE: retail's only code caller of 0x14001aea0 is
    // MapGenerator_ScalarDtor at 0x14001ae7f (Ghidra xrefs; the other two are
    // .pdata 0x1401e35ec and the vtable 0x1401950d0), and that scalar dtor is
    // exactly what mapgenerator.cpp:73 reimplements. Null-guard-and-SKIP, so
    // registering restores retail rather than changing behaviour.
    //
    // CLOSURE: all five callees are already registered — String::~String
    // (slot 0x140152af0), MemFree (0x140152b68), External::~External
    // (0x140151ea0), and the two array destroyers 0x14001b030 / 0x14001b0a0
    // (reg_game_leaf.cpp:38-39).
    RegisterExeAddr(0x14001aea0,
                    reinterpret_cast<uintptr_t>(&MapGenerator_DtorBody_1aea0));
}
