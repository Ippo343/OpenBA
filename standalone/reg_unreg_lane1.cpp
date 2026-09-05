// standalone/reg_unreg_lane1.cpp
// ============================================================================
// Unregistered dispatch-slot wave — LANE 1 (save/load + object core).
// Session 2026-09-04, branch wave/upgrade-path-complete.
// See docs/waves/unregistered_slots_wave.md for the shared brief.
//
// Every address below is a real PE import slot of Application-x64.exe
// (verified with tools/dispatch_slot_map.py). The names are taken verbatim
// from the exe's own import table, so `dispatch_slot_map.py --check` validates
// this file too — it scans standalone/reg_*.cpp for exactly this call shape.
//
// CROSS-CONSUMER AUDIT (done before adding anything here):
// Every address below was grepped across code/, engine/, standalone/, runtime/
// and dx11/ and EVERY reader was read, because of the defect class lane 3
// found: a slot read under the WRONG NAME that works only because the slot is
// unregistered and control falls through to a name-based fallback. Registering
// such a slot silently rebinds that reader to a different function.
//   - 0x1401515e0, 0x140152870, 0x1400192a0: exactly one reader each, and its
//     local name and call shape match the PE symbol.
//   - 0x1401529f8 / 0x140151218 / 0x140151220 / 0x140151228: one reader each
//     (engine_funcs.cpp), and each already falls back to ResolveExport() with
//     the IDENTICAL mangled name — the fallback is its own proof of the name.
//   - 0x140151610 / 0x140151d18: same, name-matched ResolveExport fallbacks.
//   - 0x140151440: no fallback, but engine_funcs.cpp:4374 independently
//     resolves ?_singleton@MainLoop@Engine@@1PEAV12@EA into
//     g_pMainLoopSingleton, and gameupdateloop.cpp:457 already assigns that to
//     g_pEngineSingleton — so the object identity is confirmed twice over.
//   - 0x1401514f0: one reader, used purely as a data exref; both consumers of
//     g_ppAchievementManager call fn_AchievementManager_*.
// None of the eleven has a second reader under a conflicting name.
//
// WHAT IS **NOT** HERE, and why (read before adding it):
//
//   0x140152a90  ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z
//                DELIBERATELY LEFT OUT — registering it TODAY is actively
//                harmful. code/Game/Component/cropfieldcomponent.cpp:145 binds
//                THIS slot to `s_fn_IBP_Serialize2` and calls it as
//                InternalBasePtr::Serialize(ibp, serializer). Retail's
//                CropFieldComponent_helper_36c0 (0x1400336c0) calls slot
//                0x140152aa0 there, not 0x140152a90 (disassembly:
//                `1400336fa CALL [0x140152ac8] ... 140033716 CALL [0x140152aa0]
//                 ... 14003373e CALL [0x140152aa0] ... 14003376b CALL
//                 [0x140152a58]` — 0x140152a90 appears nowhere).
//                This is the SAME defect family as lane 3's 0x1401527f8
//                (GetRoot/SetParticle) and 0x140152830, and lane 2's
//                0x140151a60 — a slot read under the wrong name that survives
//                only because it is null. It is the only one of the four that
//                is NOT dead: it sits in the save/load stream.
//                The ctor body is `*this = param_1` (Runtime 0x1800062a0), so
//                with the slot registered that call site would store the
//                *Serializer pointer* into the entity-ref IBP and then hand it
//                to IsDerivedFrom, which reads it as an Object::Base vtable.
//                Register this ONLY in the same commit as patch P3 in the lane
//                report (cropfieldcomponent.cpp 0x140152a90 -> 0x140152aa0).
//
//   0x14009e8c0  CreateNaturalResource_impl_sub_e8c0 — a large, unported
//                region/pathfinder rebuild. Not a dispatch slot; needs a port,
//                not a registration. Documented deviation already lives in
//                code/Game/Game/gamegrid_serialize.cpp:44.
// ============================================================================
#include "resolve.h"
#include <cstdint>
#include <map>

// code/Game/Game/serialize_externals.cpp
extern "C" void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(void* refField, void* serializer);

namespace {

// Backing storage for the .rdata dispatch slots below. RegisterExeAddr maps the
// Ghidra address of the SLOT to the address of a word holding the value, which
// is what every consumer reads:  fn = *(fn_t*)ResolveAddr(Module::Exe, addr).
// std::map is used (not unordered_map) so the node address stays stable.
std::map<uintptr_t, uintptr_t>& SlotStorage() {
    static std::map<uintptr_t, uintptr_t> s;
    return s;
}

void RegisterDispatchSlot(uintptr_t ghidra_addr, uintptr_t value) {
    auto& slots = SlotStorage();
    slots[ghidra_addr] = value;
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

}  // namespace

extern "C" void RegisterUnregLane1Batch() {

    // ------------------------------------------------------------------
    // 0x1401515e0 — Object::Internal::Persist(Stream&)
    // ------------------------------------------------------------------
    // Consumer: code/Game/Component/absorbresourcecomponent.cpp:89, which is
    // the reimpl of FUN_1401189b0 — the Component base Serialize used by EVERY
    // component subclass. It opens with `if (s_cs_Persist) s_cs_Persist(...)`,
    // so with the slot unregistered the base call was silently SKIPPED on both
    // the save and the load leg. That is the null-guard-and-skip shape the
    // brief warns about, and the same shape as the closed GoalInstance::Persist
    // desync.
    //
    // MEASURED, and it matters: this one is NOT a desync. Runtime 0x1800bfda0
    // is three bytes — `C2 00 00` = RET 0. Object::Internal::Persist is an
    // empty virtual; it moves ZERO stream bytes. Registered here anyway so the
    // call is byte-faithful and nobody re-opens this as a suspect.
    RegisterDispatchSlot(0x1401515e0,
        (uintptr_t)ResolveExport("?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z"));

    // ------------------------------------------------------------------
    // 0x140152870 — Object::External::_typeInfo  (DATA, not a function)
    // ------------------------------------------------------------------
    // A static BaseTypeInfo object in Runtime; the import slot holds its
    // ADDRESS. Retail FUN_1400265b0 (the StatGoal ExternalPtr resolver) does
    //     MOV RAX,[0x140152870] ; MOV EDX,[RAX] ; CALL [0x140152a98]
    // i.e. IsDerivedFrom(found, *(uint*)&External::_typeInfo).
    //
    // Consumer: code/Game/Goal/goal_exe_helpers.cpp:95 reads exactly that
    // double-indirect. standalone/reg_goal.cpp:8 documents relying on the slot
    // being unregistered so the type check is SKIPPED. That was a defensible
    // stopgap, but it is a deviation, and the check it skips is the cheapest
    // possible one: the expected type is the BASE class External, so every
    // object ExternalFactory::Find can return passes it. Registering restores
    // retail with no reachable change in outcome.
    //   Safety of the surviving guard: goal_exe_helpers.cpp reads
    //   `(!pTypeId || (fn_IsDerivedFrom && fn_IsDerivedFrom(...)))`. Making
    //   pTypeId non-null means fn_IsDerivedFrom must be non-null or the result
    //   is nulled — it always is (engine_funcs.cpp:1742 ResolveExport, with a
    //   Runtime-address fallback and a null-safe trampoline).
    RegisterDispatchSlot(0x140152870,
        (uintptr_t)ResolveExport("?_typeInfo@External@Object@@0VBaseTypeInfo@2@A"));

    // ------------------------------------------------------------------
    // 0x1401529f8 / 0x140151218 / 0x140151220 / 0x140151228
    //   IO::Serializer::GetEncoding
    //   Object::ExternalFactory::IsReferenceModeEnabled
    //   Object::ExternalFactory::GetReferenceId
    //   Object::ExternalFactory::GetPathForReferenceId
    // ------------------------------------------------------------------
    // Consumer: engine/funcs/engine_funcs.cpp:5017-5056, the reference-mode
    // block used by code/Game/Action/action_base_move.cpp's
    // ExternalPtr::Serialize. Each of the four is dispatch-or-fallback:
    //     slot && *slot ? *slot : ResolveExport(<same mangled name>)
    // so they already reach the correct Runtime function today. Registering
    // changes NOTHING functionally — it just makes the primary path the live
    // one and removes four rows from the unregistered census so the next sweep
    // does not re-audit them. Explicitly: reference mode is NOT stubbed to
    // false in our port, so the "resource first parsed inside the
    // reference-mode window deserializes corrupt" note does not apply here.
    RegisterDispatchSlot(0x1401529f8,
        (uintptr_t)ResolveExport("?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ"));
    RegisterDispatchSlot(0x140151218,
        (uintptr_t)ResolveExport("?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ"));
    RegisterDispatchSlot(0x140151220,
        (uintptr_t)ResolveExport("?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z"));
    RegisterDispatchSlot(0x140151228,
        (uintptr_t)ResolveExport("?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z"));

    // ------------------------------------------------------------------
    // Singleton DATA slots. Each import slot holds the ADDRESS of a Runtime
    // `_singleton` POINTER variable, so the consumer pattern is
    //     exref = ResolveAddr(...);  pSingleton = (void**)*exref;  obj = *pSingleton;
    // ResolveExport returns the address of the variable — exactly the value the
    // real loader writes into the slot. The cell may still be null until the
    // owning ctor runs; the pointer-to-cell is valid from DLL load.
    // ------------------------------------------------------------------

    // 0x140151610 — Save::SaveLoad::_singleton.
    // engine_funcs.cpp:2440 already falls back to ResolveExport with the same
    // name, so this is a no-op today. Registered for census hygiene.
    RegisterDispatchSlot(0x140151610,
        (uintptr_t)ResolveExport("?_singleton@SaveLoad@Save@@0PEAV12@EA"));

    // 0x140151d18 — IO::FileSystem::_singleton.
    // Read TWICE with different results: engine_funcs.cpp:2608 sets
    // g_pFileSystemSingleton2 and HAS a ResolveExport fallback, but
    // engine_funcs.cpp:4257 sets g_pFileSystemSingleton and has NONE — so that
    // global is NULL today, and code/Game/Dialog/spritecaptureui.cpp:179 gets
    // a null FileSystem instance. Registering fixes the second read.
    RegisterDispatchSlot(0x140151d18,
        (uintptr_t)ResolveExport("?_singleton@FileSystem@IO@@0PEAV12@EA"));

    // 0x140151440 — Engine::MainLoop::_singleton -> g_pEngineSingleton.
    // engine_funcs.cpp:2322 has NO fallback, so g_pEngineSingleton was null
    // through all of boot. gameupdateloop.cpp:457 patches it up later
    // (`g_pEngineSingleton = g_pMainLoopSingleton`, which engine_funcs.cpp:4374
    // resolves from the very same export) — but only once GameUpdateLoop runs.
    // Everything that reads it earlier saw null: particlecomponent.cpp:546
    // (the fixed-timestep source), debugui.cpp:860, moddialog.cpp:707,
    // mod_dialog_vtables.cpp:436. Registering makes it correct from
    // EngineFuncsInit; the value is identical to the late fixup, so the
    // GameUpdateLoop path simply stops firing.
    RegisterDispatchSlot(0x140151440,
        (uintptr_t)ResolveExport("?_singleton@MainLoop@Engine@@1PEAV12@EA"));

    // 0x1401514f0 — Achievement::AchievementManager::_singleton.
    // engine_funcs.cpp:2649 does g_ppAchievementManager = (void***)ResolveAddr(...)
    // — the SLOT address, so the consumer triple-derefs. Unregistered that is a
    // flat null: code/Game/Dialog/achievementdialog.cpp:38 and :219 deref it
    // UNGUARDED (`void* manager = **g_ppAchievementManager;`) and would fault
    // if that dialog ever opened.
    // Behaviour note, deliberate: nothing in our port constructs the manager
    // yet, so **g_ppAchievementManager stays null and every guarded site
    // (gameupdateloop.cpp:1346 Update, :2594 Shutdown) keeps skipping exactly
    // as before. The delta only appears once the manager is constructed, at
    // which point ticking it IS retail. GameUpdateLoop::Load "step 30" is a
    // comment with no body (gameupdateloop_load.cpp:1310) — this registration
    // does not resurrect it; that still needs porting.
    RegisterDispatchSlot(0x1401514f0,
        (uintptr_t)ResolveExport("?_singleton@AchievementManager@Achievement@@0PEAV12@EA"));

    // ------------------------------------------------------------------
    // 0x1400192a0 — Object::ExternalPtr<EnvironmentDescription>::Serialize
    // ------------------------------------------------------------------
    // An EXE CODE address (not a dispatch slot). The reimpl
    // Reimpl_ExternalPtr_Serialize_EnvironmentDesc exists in
    // code/Game/Game/serialize_externals.cpp:246 but was registered NOWHERE, so
    // gameupdateloop_serialize.cpp:204 resolved it to null.
    // NOT a desync: WeatherSim_Serialize_93c90 (line 336) is
    // dispatch-or-fallback and the fallback calls that very same body, so the
    // bytes were always moved. Registering makes the primary path live and
    // covers any future caller that lacks the fallback.
    RegisterExeAddr(0x1400192a0,
        reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_EnvironmentDesc));

    // ------------------------------------------------------------------
    // UNBLOCKED 2026-09-04: patch P3 is applied, so cropfieldcomponent.cpp no
    // longer reads this ctor slot under the name IBP::Serialize. Registering it
    // now un-skips four ctor call sites that have no fallback.
    // ------------------------------------------------------------------
    RegisterDispatchSlot(0x140152a90,
        (uintptr_t)ResolveExport("??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z"));
}
