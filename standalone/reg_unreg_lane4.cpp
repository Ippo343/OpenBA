// standalone/reg_unreg_lane4.cpp
// Unregistered-slot wave, lane 4 (non-import .rdata/.data addresses).
//
// Lane 4's 50 addresses are NOT PE import slots, so dispatch_slot_map.py cannot name
// them. They fall into three kinds, and this file only closes two of them:
//
//   * .data 0x1401c4138-0x1401c7f90 — runtime type-id storage. Each address is the
//     first dword of one exe-side Object::BaseTypeInfo, written by that class's
//     `<Class>_RegisterType` / `<Class>Description_Register` during static init and
//     filled with the real CRC id by Object::Factory::Startup. Unregistered, every
//     `ResolveAddr(Exe, slot)` returns 0, so every `IsDerivedFrom(obj, *pTid)` /
//     `GetController(dlg, out, *pTid)` in the tree misses for EVERY object, forever,
//     with no crash and no log. That is the `0x1401c5a98` (Profession) trap that
//     silently zeroed 23 tutorial goals: the count comes back a hard 0 while every
//     visible mechanism still looks healthy.
//
//   * .rdata 0x14017xxxx — float constants. Same shape as RegisterRdataFloatConsts()
//     in address_registry.cpp: a hard `if (!p) return;` guard goes inert, and a soft
//     `p ? *p : <literal>` fallback silently uses whatever literal the porter guessed.
//     One of the three below is exactly that second case and is 6.7x off.
//
//   * .rdata 0x14015xxxx-0x14016xxxx — exe vtables. NOT registered here; see the
//     "DELIBERATELY NOT REGISTERED" section at the bottom for why each one is left.
//
// ── How the names were established (not guessed) ─────────────────────────────
// Every name below is the exe's OWN registration name, cross-checked two ways:
//   1. standalone/factory_registrations.csv — the Ghidra extract of every exe-side
//      BaseTypeInfo registration, matched on the STORAGE column (field 3), which is
//      the address itself. That is a bijection address -> name, taken from the
//      binary's own RegisterType call, not from any label in our tree.
//   2. Ghidra `list_globals` labels (TypeInfo_<Class> / g_BaseTypeInfo_<Class>) at
//      the identical addresses.
// Both agree on all 30. Where our tree's OWN global name disagrees with the binary,
// the binary wins and the disagreement is called out in the per-entry comment —
// several of our engine_funcs labels are long-standing misnomers, and registering
// the slot makes the ADDRESS hold retail's value regardless of what we named the
// pointer that reads it.
//
// ── Why registering these RESTORES retail rather than changing behaviour ──────
// Read the shared brief's rule: a consumer that null-guards-and-SKIPS is safe to
// restore; a consumer that null-guards-and-FALLS-BACK changes behaviour. Every
// consumer of these 30 does one of:
//   (a) skips (`if (!p) return false;`) — restoring is strictly correct;
//   (b) falls back BY NAME through ResolveExeTypeId(addr, name, cache) /
//       ResolveTypeIdByNameLazy(name) with the SAME name the binary registered —
//       restoring is value-identical and removes the fallback's ordering hazard.
// The two lane-4 addresses where that is NOT true are excluded; see the bottom.
//
// ── EVERY consumer of every address below was read, not just one ─────────────
// The mirror-image failure of this wave is a slot read under the WRONG NAME that
// works ONLY because it is unregistered (lane 3 found two of those in
// engine_funcs.cpp: 0x1401527f8 read as fn_DialogInstance_GetRoot is really
// ParticleInstance::SetParticle, 0x140152830 read as fn_AudioScene_PlayEffect is
// really Achievement::Goal::Goal). These bands have no import table to check against,
// so each address here was grepped across code/, engine/, standalone/ and runtime/ and
// EVERY consumer confirmed to want the same class. Result: unanimous on all 30. Where
// a consumer lazy-fills by name it uses the SAME name registered here (verified at
// upgradeui.cpp:230 "UpgradeUIConfig", tool_activation_standalone.cpp:193-194
// "Tool"/"ToolSetting", newgamedialog.cpp:61-68 MapGenerator/ClimateGenerator/
// TerrainGenerator/NameDatabase, toolbar_vtable_standalone.cpp:166 "Toolbar",
// debugui.cpp:1742 "Disease", gatherrawmaterials.cpp:297 "BuildDescription",
// absorbresourcecomponent.cpp:124 "Description", entityui.cpp:893 "UIDescription").
// The two apparent second-readers that disagreed were both chased to ground and are
// stale LABELS on code that never reads the address — see 0x1401c5da8 below, and the
// dead "ResourceComponent" fill under 0x1401c7040.
//
// ── One real behaviour change to expect, and it is the point ─────────────────
// Several of these ids gate a VALIDATION on a load path, and a validation that has
// been resolving to null has been silently passing everything. absorbresourcecomponent
// .cpp:114 states it outright: "a null typeId makes the generic body load without
// validation (cascade-safe)". After this registration those ExternalPtr/InternalPtr
// loads are type-checked again, so a restored pointer of the wrong class will now be
// NULLED where it previously survived. That is retail behaviour (0x1401c72e0,
// 0x1401c5da8, 0x1401c7c10 and 0x1401c5948 are all read at exactly such sites), but it
// is the one place where "restores retail" can look like a new bug in an A/B run.
//
// ── Two-phase, exactly like reg_food_typeid.cpp / reg_cleararea_typeids.cpp ───
//   RegisterUnregLane4TypeIds()  — from AddressRegistryInit, BEFORE EngineFuncsInit,
//                                  so every engine_funcs global points at our storage.
//   FillTypeIdGlobals_lane4()    — from game_main.cpp AFTER Object::Factory::Startup,
//                                  the only point at which the real CRC ids exist.
// Between those two calls the storage holds 0. That is safe: every by-name helper in
// the tree (ResolveExeTypeId, engine_funcs.cpp:21) guards the VALUE and treats 0 and
// 0xFFFFFFFF as "not resolved yet", so a consumer that runs early still falls back.
//
// Not env-gated, for the reason reg_cleararea_typeids.cpp gives: an unfilled type-id
// global is not a conservative default, it is a silent permanent miss.

#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct TypeIdSlot {
    uintptr_t   addr;
    const char* name;    // the exe's own RegisterType name (factory_registrations.csv)
    uint32_t    value;   // live storage the exe-faithful *(uint32_t*)addr reads resolve to
};

// Ordered by address. `load=` is the per-address hit count inside the save-load
// window from the BANISHED_UNREG_CENSUS run in census.txt; those rows are the ones
// this wave is aimed at.
TypeIdSlot g_slots[] = {
    // load=0. ImmigrationComponent. engine_funcs.cpp:2349 sets
    // g_TypeId_ImmigrationComponent from here, i.e. to nullptr — every nomad/
    // immigration component lookup has been answering "this entity has no
    // ImmigrationComponent" for the life of the project.
    { 0x1401c4480, "ImmigrationComponent", 0 },

    // load=0. EntityUI. Our label for this slot (g_TypeId_PageToggleUI,
    // engine_funcs.cpp:2224) is a MISNOMER — PageToggleUI is a different class whose
    // own slot is 0x1401c5408 and which nothing in this tree reads. statusiconui.cpp
    // :327-338 already audited this and deliberately stopped filling the global by
    // name, because feeding it the real PageToggleUI id was WRONG-BUT-NONZERO and
    // silently disabled camera-follow on every "go to entity" click. Its two live
    // readers (engine_funcs.h:4671 Toolbar_CenterOnEntity_Impl, edittool.cpp:289)
    // now both resolve "EntityUI" for themselves, so this registration is
    // value-identical for them and finally makes the ADDRESS itself truthful.
    { 0x1401c4f00, "EntityUI", 0 },

    // load=0. ModBrowseUI (engine_funcs.cpp:3776 g_TypeId_BrowserController — our
    // label again differs from the binary; the binary's registrar is
    // RegisterType_ModBrowseUI). Consumer skips on null, so restoring is safe.
    { 0x1401c5210, "ModBrowseUI", 0 },

    // load=0. ResourceLimitUIConfig (engine_funcs.cpp:2239 g_TypeId_ResourceLimitType).
    { 0x1401c5520, "ResourceLimitUIConfig", 0 },

    // load=0. CaptureBoxElement. engine_funcs.cpp:3638 explicitly notes this one is a
    // "direct dword" rather than the double-indirect exref its neighbours use — which
    // is exactly why the reg_element_typeids.cpp batch (which registers exref slots by
    // pointing them at Runtime BaseTypeInfo exports) does not cover it and it was left
    // null. Ghidra label at the address is literally g_TypeId_CaptureBoxElement.
    { 0x1401c5638, "CaptureBoxElement", 0 },

    // load=0. StatusIconUIConfig. engine_funcs.h:622 already records that
    // 0x1401c56e0's writer is StatusIconUIConfig_Register, while engine_funcs.cpp:2222
    // still assigns it to g_TypeId_DialogController. statusiconui.cpp:329 confirms
    // that global now has ZERO readers tree-wide, so this registration cannot
    // re-poison anything; it makes the address hold what retail holds.
    { 0x1401c56e0, "StatusIconUIConfig", 0 },

    // load=0. ToolUI. Read raw by edittool.cpp:255 (s_pTypeId_ToolUI) and
    // tool_base_trade.cpp:478 (s_pCtrlTypeId) — both cache the POINTER and were
    // holding null, so the GetController(ToolUI) narrow could never fire.
    { 0x1401c5788, "ToolUI", 0 },

    // load=0. UpgradeUIConfig — its +0x20 holds the upgrade target ComponentEntity
    // template (engine_funcs.cpp:2675). Null id => the upgrade panel can never
    // classify its own config object.
    { 0x1401c5868, "UpgradeUIConfig", 0 },

    // load=0. ClimateGenerator. Read by statvaluedispatch.cpp:87 (s_tidClimateGen) —
    // a null there makes the goal/stat dispatcher unable to recognise the climate
    // generator, which is one of the sources a scenario goal can read.
    { 0x1401c5910, "ClimateGenerator", 0 },

    // load=16 — BY FAR the most-resolved unregistered address in the whole census.
    // "Disease" (an Object::External; registered inside ClimateGenerator_StaticInit).
    // Read by citizencomponent_serialize.cpp:246 to validate the ExternalPtr<Disease>
    // at citizen+0x68 on EVERY citizen record, and by citizen_updatehealth.cpp:82.
    // A validation read that resolves to null on a SERIALIZER path is the dangerous
    // shape: disaster_harness.cpp:471 already records that the lazy heal is dead
    // "because ResolveAddr(Exe, 0x1401c5948) returns 0 in standalone".
    { 0x1401c5948, "Disease", 0 },

    // load=0. MapGenerator — mapgenerator.cpp:367 documents MapGenerator_GetTypeId as
    // `mov eax,[0x1401c59b8]`, so our reimplementation of that virtual returns 0.
    { 0x1401c59b8, "MapGenerator", 0 },

    // load=0. NameDatabase (engine_funcs.cpp:3230). The town/citizen name generator's
    // resource lookup classifies the loaded .rsc against this id.
    { 0x1401c5a60, "NameDatabase", 0 },

    // load=0. TerrainGenerator — same GetTypeId shape as MapGenerator
    // (terraingenerator.cpp:371) plus statvaluedispatch.cpp:86.
    { 0x1401c5b40, "TerrainGenerator", 0 },

    // load=1. "Action" — NOT ComponentEntity. engine/types/ComponentEntity.h:48 calls
    // this TypeId_CE_Ext ("ComponentEntity external type id"); the Ghidra label at the
    // address is TypeInfo_Action and the CSV registrar is Action's. ComponentEntity's
    // real slot is 0x1401c5d70, 0x38 lower (one BaseTypeInfo stride) and already
    // registered by reg_action_child_ctors2.cpp. The one live consumer,
    // charactercomponent_serialize.cpp:84, already names it correctly ("Action =
    // 0x1401c5da8, array loop + IsDerivedFrom site"), and it is on the LOAD path: with
    // the id null the per-element validation of the character's action array is
    // skipped, so a restored action of the wrong class is kept instead of nulled.
    //
    // The second-reader claim was chased down and is a LABEL bug, not a conflict:
    // componententity_funcs.h:55 documents ComponentEntity's "Slot7 GetTypeId2" as
    // returning *(uint32_t*)0x1401c5da8, and Ghidra names the 7-byte stub at
    // 0x140021c70 "ComponentEntity_OnCreate". Decompiled, that stub is
    // `return TypeInfo_Action;` — it is Action::GetTypeId, mis-attached to the
    // ComponentEntity vtable in both our header and Ghidra's symbol. Every real xref
    // agrees: writer Action_RegisterType @0x14014d035, and the two other readers
    // (0x140021d64, 0x1400c5121) are both `MOV EDX,[0x1401c5da8]` feeding
    // CALL [0x140152a98] (IsDerivedFrom) with a null-the-pointer-on-failure tail —
    // the same Action-typed pointer validation. Our ComponentEntity_Slot7 inline
    // returns a hard 0 and never reads the address, so it is not a consumer at all.
    { 0x1401c5da8, "Action", 0 },

    // load=0. Toolbar. engine_funcs.cpp:2841 g_TypeId_ToolbarItem; the standalone
    // toolbar path (toolbar_vtable_standalone.cpp:30) documents the same name.
    { 0x1401c6128, "Toolbar", 0 },

    // load=0. Weather. serialize_externals.cpp:11 records that external-slot #10 reads
    // its typeId "directly from a dword at 0x1401c6198" — i.e. the world-serializer's
    // weather external is validated against a null id today.
    { 0x1401c6198, "Weather", 0 },

    // load=1. TimeGoal — timegoalinstance.cpp:84 caches this pointer as
    // s_TypeInfo_TimeGoal. Scenario/tutorial goal instances are restored during the
    // load, and a goal whose class cannot be identified is the exact family of bug
    // that made 23 tutorial goals auto-pass.
    { 0x1401c62e8, "TimeGoal", 0 },

    // load=0. EditTool (engine_funcs.cpp:2698 g_TypeId_EditTool — label agrees).
    { 0x1401c6dd8, "EditTool", 0 },

    // load=0. "Tool" (the Object::Internal base of the whole tool family; registered
    // by Tool_StaticInit). engine_funcs.cpp:2709 calls it g_TypeId_ToolController;
    // nomadcomponent.cpp:182 reads it raw. tool_activation_standalone.cpp:190 already
    // notes both this and the ToolSetting slot below resolve to null.
    { 0x1401c6eb8, "Tool", 0 },

    // load=0. ToolSetting — the pair of the above, same call site.
    { 0x1401c6ef0, "ToolSetting", 0 },

    // load=1. BuildComponent. dialog_exe_helpers.cpp:36 says outright that this is
    // "NULL in standalone" and guards the WorkplaceUI FindComponent on it.
    { 0x1401c7008, "BuildComponent", 0 },

    // load=0. BuildDescription. engine_funcs.cpp:2223 calls it
    // g_TypeId_ResourceComponent, which ui_dialog_stub_factories.cpp:129 already flags
    // as misleading. This one is worth reading twice, because two files lazy-fill the
    // same global with DIFFERENT names — entityui.cpp:897 uses "ResourceComponent",
    // ui_dialog_stub_factories.cpp:136 uses "BuildDescription". Only one of those can
    // ever fire, and it is not the one the label suggests: there is NO type registered
    // under the name "ResourceComponent" anywhere (checked against
    // factory_registrations.csv and every REGISTER_TYPE in code/Game — the near-misses
    // are AbsorbResourceComponent / ClearResourceComponent / NaturalResourceComponent),
    // so entityui.cpp:897's ResolveTypeIdByNameLazy returns 0 and that fill is dead
    // code. The live fill is the "BuildDescription" one, which is also what Ghidra
    // says: the registrar at the address is BuildDescription_Register, and retail's own
    // EntityUI_helper_9390 (the BuildComplete_Impl original) READS 0x1401c7040. The
    // three other live consumers name it BuildDescription too
    // (buildcomponent_state1.cpp:367, buildcomponent_state8.cpp:222,
    // gatherrawmaterials.cpp:297). Unanimous.
    { 0x1401c7040, "BuildDescription", 0 },

    // load=1. "Description" — the Object::External base of every *Description class.
    // absorbresourcecomponent.cpp:120 caches it. NOTE the address arithmetic here:
    // this is the BASE of the Description BaseTypeInfo, and 0x1401c72e8 (also in this
    // lane) is that same struct's +0x08 NAME pointer, NOT a second type id. See the
    // bottom of this file.
    { 0x1401c72e0, "Description", 0 },

    // load=1. ControllerComponent — statemachinecomponent.cpp:91 caches it as
    // s_TypeId_ControllerComponent. Every state-machine component restored from the
    // save runs through that file.
    { 0x1401c73c0, "ControllerComponent", 0 },

    // load=1. DecalDescription — decalcomponent.cpp:229/578, decaldescription.cpp:6
    // ("BaseTypeInfo @ 0x1401c75b8") and roadtool.cpp:110 all read it. Decals are
    // rebuilt from the save stream, so a null id here is a load-path miss.
    { 0x1401c75b8, "DecalDescription", 0 },

    // load=0. FloorDescription (floorcomponent.cpp:109). The dock/bridge deck walk
    // height depends on FloorComponent finding its description.
    { 0x1401c7708, "FloorDescription", 0 },

    // load=1. PickingComponent. Read by buildcomponent.cpp:96/347 (block 2 collects
    // these into +0x68) and by charactercomponent_serialize.cpp:82 on the LOAD path.
    { 0x1401c7c10, "PickingComponent", 0 },

    // load=1. StorageDescription — storagecomponent.cpp:99, buildcomponent_state8.cpp
    // :753 and town_storage_seed.cpp:110. Storage is restored from the stream.
    { 0x1401c7eb0, "StorageDescription", 0 },

    // load=0. ToolbarDescription — tool_base_drag.cpp:40, tool_base_createbar.cpp:117
    // and tool_base_toolui.cpp:246 each re-resolve it independently and each get null.
    { 0x1401c7f58, "ToolbarDescription", 0 },

    // load=0. UIDescription — engine_funcs.cpp:3269. entityui.cpp:893 documents the
    // symptom precisely: with this null, EntityUI::Slot7 cannot read the description's
    // default page index and the panel stays on the empty default page (blank body).
    // It currently survives only because entityui.cpp lazy-fills by the same name.
    { 0x1401c7f90, "UIDescription", 0 },
};
const int kSlotCount = (int)(sizeof(g_slots) / sizeof(g_slots[0]));

// ── .rdata float constants ───────────────────────────────────────────────────
// Values are the literal 4-byte patterns read out of Application-x64.exe via Ghidra
// read_memory, stored as bit patterns so the float is exact — not re-typed decimals.
// None of these three is in reg_rdata_float_consts.cpp's 25-entry table.

// 0x1401720c0 = 0x3C23D700 = 0.00999999977f (0.01). growthcomponent.cpp:526 reads it
// as `s_fScaleNewW ? *s_fScaleNewW : 0.01f` — the fallback happens to match, so this
// entry changes nothing today. It is registered anyway so the constant stops
// depending on a hand-typed literal staying in sync with the binary.
const uint32_t k_bits_1720c0 = 0x3C23D700u;

// 0x1401720f4 = 0x3E19999A = 0.15f (read directly at the address, not derived by
// offset). THIS ONE IS A LIVE WRONG-VALUE BUG, and it is exactly the soft-fallback
// trap reg_rdata_float_consts.cpp was written about — TWO consumers read the same
// address and guess DIFFERENT fallbacks:
//   growthcomponent.cpp:527  `s_fCropSpreadDiv ? *s_fCropSpreadDiv : 0.15f`  — exact.
//   action_base_cnr.cpp:269  `s_p_DAT_1401720f4 ? *s_p_DAT_1401720f4 : 1.0f` — NOT.
// The second is a DIVISOR in FUN_140120db0's create-natural-resource chain, so with
// the slot unregistered it silently divides by 1.0 instead of 0.15 — a 6.7x error,
// no crash, no trace, no log line of its own. Registering makes both correct; there
// is no conflict of IDENTITY here, only of guessed literal.
const uint32_t k_bits_1720f4 = 0x3E19999Au;

// 0x1401721cc = 0x41F00000 = 30.0f. Two consumers: g_pNotifyTimerDuration
// (engine_funcs.cpp:2923), dereferenced UNCONDITIONALLY at eventnotifydialog.cpp:425
// and only saved from a null-deref by the lazy fill at eventnotifydialog.cpp:974; and
// entry [5] of the autosave interval table (autosavedialog.cpp:78). The lazy fill
// happens to use the same 30.0f, so this is value-neutral — but it removes an
// unconditional deref whose safety currently depends on EnsureGlobals() having run
// first, which is exactly the ordering hazard this wave is about.
const uint32_t k_bits_1721cc = 0x41F00000u;

}  // namespace

// ── Phase 2: fill, from game_main.cpp after Object::Factory::Startup ─────────
extern "C" void FillTypeIdGlobals_lane4() {
    int filled = 0, empty = 0;
    for (int i = 0; i < kSlotCount; ++i) {
        g_slots[i].value = ::Object::GetTypeIdByName(g_slots[i].name);
        // Report the VALUE, not the pointer, and report in BOTH arms: a registered
        // slot holding 0 is indistinguishable from an unregistered one at every call
        // site, and an all-quiet log must never be readable as success.
        if (g_slots[i].value == 0) {
            ++empty;
            std::fprintf(stderr,
                "[Lane4TypeId] ERROR: GetTypeIdByName(\"%s\") returned 0 — slot 0x%llx is "
                "live but EMPTY; every IsDerivedFrom/GetController against it still misses\n",
                g_slots[i].name, (unsigned long long)g_slots[i].addr);
        } else {
            ++filled;
        }
    }
    std::fprintf(stderr, "[Lane4TypeId] %d/%d slot(s) filled, %d empty\n",
                 filled, kSlotCount, empty);
    std::fflush(stderr);
}

// ── Phase 1: storage, from AddressRegistryInit before EngineFuncsInit ────────
void RegisterUnregLane4TypeIds() {
    for (int i = 0; i < kSlotCount; ++i)
        RegisterExeAddr(g_slots[i].addr, reinterpret_cast<uintptr_t>(&g_slots[i].value));

    RegisterExeAddr(0x1401720c0, reinterpret_cast<uintptr_t>(&k_bits_1720c0));
    RegisterExeAddr(0x1401720f4, reinterpret_cast<uintptr_t>(&k_bits_1720f4));
    RegisterExeAddr(0x1401721cc, reinterpret_cast<uintptr_t>(&k_bits_1721cc));
}

// ═══════════════════════════════════════════════════════════════════════════
// DELIBERATELY NOT REGISTERED — lane-4 addresses examined and left alone.
// Recorded here so the next sweep does not "close" them without reading this.
//
// 0x1401c4138 — CreatePathDescription type id (writer CreatePathDescription_
//   RegisterType 0x14014ab25; Ghidra xrefs: EntityUI_impl_1ee0, CreatePathDescription_
//   Slot2). gameupdateloop.cpp:663 reads it into g_pDialogTypeId under the name
//   "Dialog", which is a MISIDENTIFICATION, and lines 665-671 then install a correct
//   by-name fallback ("Dialog" via ResolveTypeIdByNameLazy) precisely BECAUSE the slot
//   resolves to null. Registering it would make that pointer non-null holding
//   CreatePathDescription's id, the `if (!g_pDialogTypeId)` fallback would stop firing,
//   and the Toolbar.rsc lookup at gameupdateloop.cpp:2315/2325 would classify the
//   loaded resource against the wrong class — i.e. no toolbar. This is the shared
//   brief's "registering CHANGES behaviour" case, and here the change is a regression.
//   Fix first, register second: gameupdateloop.cpp:663 should stop reading 0x1401c4138
//   and resolve "Dialog" by name outright (or read the real Dialog slot). That is a
//   code/Game edit, so it is described, not made.
//
// 0x1401c72e8 — NOT A TYPE ID. It is the Description BaseTypeInfo's +0x08 field, i.e.
//   the `const char*` type NAME. Evidence: BaseTypeInfo stride is 0x38 and 0x1401c72e0
//   ("Description") + 0x38 = 0x1401c7318, so 0x1401c72e8 is interior; its single xref
//   is `MOV RAX,qword ptr [0x1401c72e8]` at 0x140031c60 — a QWORD read returning the
//   name, sitting immediately after `MOV EAX,dword ptr [0x1401c72e0]` at 0x140031c50
//   (the matching GetTypeId). Registering 4 bytes of type-id storage here would be
//   the "getting the KIND wrong" failure the brief warns about.
//   THE REAL DEFECT BEHIND IT: three of our files read this address as a uint32_t type
//   id named "TypeInfo_ComponentEntity" —
//       code/Game/Component/multiobjectcomponent.cpp:215
//       code/Game/Component/nomadcomponent.cpp:508
//       code/Game/Component/pickingcomponent.cpp:325
//   Retail's own MultiObjectComponent_Controller_Slot13 (0x140125880) validates with
//   `IsDerivedFrom(*ibp, g_BaseTypeInfo_ComponentEntity)` = 0x1401c5d70, which IS
//   already registered (reg_action_child_ctors2.cpp:64) and filled. So the fix is a
//   one-token change of 0x1401c72e8 -> 0x1401c5d70 in those three files; it is a
//   code/Game edit and is described, not made. Until then those three IsDerivedFrom
//   validations are dead — including MultiObjectComponent's, which runs on the LOAD
//   path and is what nulls out a restored child entity of the wrong class.
//
// 0x1401c42c0 (HappinessComponent, load=1) and 0x1401c70e8 (CitizenAIComponent) —
//   ALREADY registered, by reg_citizenai_typeids.cpp:83/85, but behind the opt-in
//   flags BANISHED_LIVE_CITIZENAI_HAPPYID / _AIID (only the third of that trio,
//   _CITID, has graduated into kDefaultOnFlags). That is why they still show up in the
//   census as unregistered. Registering them again from here would create a SECOND
//   storage dword for the same address and silently split the slot depending on init
//   order — the trap reg_food_typeid.cpp calls out for RadiusDescription. Graduating
//   them is that wave's call, not this one's.
//
// .rdata vtables 0x14015bd80 (EventPopupDialog), 0x14015be00 (FollowDialog),
// 0x14015c3b0 (SpriteSheet) / 0x14015c400 (MaterialInstance) (ModDialog thumbnails),
// 0x14015c6c0 (ModUploadDialog), 0x14015e350 (AchievementInstance), 0x14015f070
// (purecall placeholder), 0x14015f5a0 (ToolUI child dialog), 0x14015f980 (Toolbar
// external exref), 0x14015fa00 (ToolbarDialog), 0x140160bf8 (TutorialInstance),
// 0x140162920 (Component base), 0x140164520 (StateMachineComponent) —
//   NONE registered. A vtable registration is a static array of function pointers, and
//   every slot in these tables points at an exe function; pointing a live vtable at a
//   table of unported slots trades a null pointer (which the consumers guard) for an
//   EXEC-of-0 at an arbitrary later virtual call. Several already have hand-built
//   standalone replacements installed by other means (thumb_vtables.cpp,
//   toolbar_vtable_standalone.cpp, followdialog.cpp, tool_base_toolui.cpp), and
//   0x14015f5a0 is documented as DELIBERATELY unregistered at engine_funcs.cpp:2829
//   ("registering it poisons the menu's BuildId child"). 0x14015e350 is likewise
//   documented as an intentional fallback at gameupdateloop_load.cpp:1622 (keeping the
//   Runtime vtable, which differs only in the dtor, is "deliberately better than
//   installing a null"). See the lane-4 report for the one open question here: the
//   StateMachineComponent scalar dtor writes NULL into self->vtable and into
//   self+0x28 because 0x140164520/0x14015f070/0x140162920 all resolve to 0.
