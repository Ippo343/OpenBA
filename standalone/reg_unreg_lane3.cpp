// standalone/reg_unreg_lane3.cpp
//
// Unregistered-dispatch-slot wave, Lane 3 (graphics / material / particle / math /
// string). See docs/waves/unregistered_slots_wave.md for the shared brief: the exe
// has a real PE import table (833 Runtime-x64.dll imports at 0x140151218-0x140152c18),
// and every RegisterDispatchSlot pair below is filling in one of those import slots
// with the exact Runtime export it names in the exe's own import string table
// (verified via tools/dispatch_slot_map.py against docs/waves/lanes/lane3.txt).
//
// Call RegisterUnregLane3() once during startup, alongside the other reg_*.cpp waves
// (e.g. right after RegisterDispatchSlots() in standalone/address_registry.cpp).
//
// Data slots (BaseTypeInfo*/int statics) use the exact same storage-indirection
// pattern as function slots: retail's exe .rdata import cell holds a POINTER VALUE
// (either "the imported function's address" or "the address of the imported Runtime
// global"), and callers do `T* p = *(T**)ResolveAddr(Exe, addr)`. So RegisterExeAddr
// must point at storage that HOLDS that pointer value, not at the value itself — same
// as address_registry.cpp's local RegisterDispatchSlot() helper. We can't reuse that
// helper (it's static to address_registry.cpp), so we keep an identical local one.

#include "resolve.h"
#include <cstdint>
#include <unordered_map>

namespace {

// Local slot storage — mirrors address_registry.cpp's GetSlotStorage()/
// RegisterDispatchSlot() exactly. Each entry's ADDRESS (not its value) is what
// gets registered, because retail code does `fn = *(fn_t*)ResolveAddr(...)`.
std::unordered_map<uintptr_t, uintptr_t>& Lane3SlotStorage() {
    static std::unordered_map<uintptr_t, uintptr_t> slots;
    return slots;
}

void RegisterLane3Slot(uintptr_t ghidra_addr, uintptr_t value) {
    if (!value) return;  // don't wire a slot to a name ResolveExport couldn't find
    auto& slots = Lane3SlotStorage();
    slots[ghidra_addr] = value;
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

} // namespace

void RegisterUnregLane3() {
    // ------------------------------------------------------------------
    // Math — trivial ctors/ops already dead-reckoned by ResolveExport fallbacks
    // at every call site we found (action_pf_geom.cpp, action_pf_roots_misc.cpp,
    // citizen_updatehealth.cpp, edittool.cpp). Registering the real exe slot is a
    // no-op for those (same function pointer either way) but completes the PE
    // table faithfully and costs nothing if some other/future caller reads the
    // slot directly without a name fallback.
    // ------------------------------------------------------------------
    RegisterLane3Slot(0x140151e80,
        (uintptr_t)ResolveExport("?Intersect@Box@Volume@@QEBA_NAEBV12@@Z"));   // Box::Intersect

    // Math::Vector3::Vector3(const Scalar&) — REAL FIX. action_pf_road.cpp's
    // Pf_EntityResolvePos_2e20 (0x140122e20) has TWO fallback paths (lines ~625,
    // ~655) that build a zero Vector3 into a caller-owned, NOT pre-zeroed
    // retVec3 out-buffer via `if (s_fn_Vec3CtorScalar) s_fn_Vec3CtorScalar(...)`
    // with NO ResolveExport fallback and no "else zero it manually" (contrast
    // with the sibling zero-fill at line ~688, which already does this by hand).
    // Unregistered, those two paths returned retVec3 UNTOUCHED — i.e. whatever
    // garbage was on the caller's stack — instead of retail's zero vector, to a
    // pathfinding position resolver. citizenai_growup.cpp's use of the same slot
    // (line 510) is safe either way (its vecBuf is pre-zeroed), and
    // action_pf_geom.cpp/action_pf_roots_misc.cpp self-heal via ResolveExport.
    RegisterLane3Slot(0x140151320,
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@AEBVScalar@1@@Z"));

    // ------------------------------------------------------------------
    // Volume::Rectangle::Rectangle() — REAL FIX. code/Game/Player/edittool.cpp's
    // EnsureEditFuncs() resolves this SOLELY via the exe IAT slot (resolveIAT,
    // no ResolveExport fallback), then EditTool's info-panel-position code
    // (~line 579-581) does:
    //     uint8_t rectangle[32]; memset(rectangle, 0, sizeof(rectangle));
    //     if (s_fn_RectangleCtor) s_fn_RectangleCtor(rectangle);
    // Ghidra-verified (Runtime 0x18005ddd0, the parameterless overload — there
    // are two sibling Rectangle ctors at 0x18005dd90/0x18005ddf0, copy-ctor and
    // 2-Vector2 ctor, NOT this one):
    //     *(float*)this      = 0x4cbebc20;   // ~1.0e8f
    //     *(float*)(this+4)  = 0x4cbebc20;
    //     *(float*)(this+8)  = 0xccbebc20;   // ~-1.0e8f
    //     *(float*)(this+0xc)= 0xccbebc20;
    // i.e. min=+BIG, max=-BIG — the standard "empty, ready to be unioned into"
    // sentinel, NOT zero. With the slot unregistered our memset(0) silently
    // substitutes {0,0,0,0}, so the entity-bounds accumulation this rect feeds
    // (Entity_impl_0380) starts from a real (0,0) corner instead of an
    // impossible one, corrupting the resulting screen rect (and therefore the
    // info panel's on-screen position) whenever the entity's true bounds don't
    // straddle zero. tool_base_screenrect.cpp / tool_base_createbar.cpp use the
    // SAME retail function but resolve it directly via ResolveExport, so they
    // are unaffected either way.
    RegisterLane3Slot(0x1401528a8,
        (uintptr_t)ResolveExport("??0Rectangle@Volume@@QEAA@XZ"));

    // ------------------------------------------------------------------
    // System::Random::RandFloatTol — REAL FIX, same defect class as the
    // RandTol/RandFloat fix already made for slots 0x1401512f0/0x1401512f8
    // (see standalone/address_registry.cpp: "the organic herder's grazing-
    // position roll had a null fn ptr"). This is the sibling slot for the
    // *tolerance* variant, and it is used by:
    //   - code/Game/Component/herdcomponent.cpp:518-519 — the animal-spawn
    //     scatter offset (rx, ry). Unregistered -> `s_fn_RandFloatTol ? ... :
    //     0.0f`, so EVERY newly spawned herd animal lands at the exact center
    //     of animalArea instead of scattered within it.
    //   - code/Game/Component/growthcomponent.cpp:756 — GrowthComponent::Startup
    //     growthScale randomization (config+0x20 > 0 path); same null->0.0f
    //     fallback removes the intended per-instance size jitter.
    //   - code/Game/Action/action_base_cnr.cpp:464-470 — child/root placement
    //     rotation and offset roll; same fallback-to-0.0f defect.
    // None of these three call sites has a ResolveExport fallback, so this was
    // a hard behavioral gap (deterministic 0 instead of retail's randomized
    // value) on every hit, not just a dormant path.

    // ------------------------------------------------------------------
    // Engine::UpdateLoop::IsRunning — REAL FIX (history: since B.3.7 the one
    // consumer, populationgoal.cpp, calls the member and fn_IsRunning is gone;
    // the row keeps the slot table faithful). engine_funcs.cpp resolved
    // fn_IsRunning SOLELY from this exe slot with no ResolveExport fallback.
    // Its only consumer, code/Game/Goal/populationgoal.cpp:139-142
    // (PopulationGoal_Slot8, the population-goal completion check), did:
    //     if (fn_IsRunning) { if (!fn_IsRunning(gameWorld)) return 0; }
    // — a null-guard-and-SKIP. Unregistered, the IsRunning gate is skipped
    // entirely and the goal is checked (and can report "met") even while the
    // world/UpdateLoop is not actually running (e.g. mid-load or paused),
    // which retail's decompile explicitly guards against. Registering
    // restores the retail guard; this is the textbook "null-guard-and-skip is
    // safe to fix" case from the wave brief.

    // ------------------------------------------------------------------
    // Graphics::MaterialInstance ctor / SetMaterial / AddTexture /
    // BuildParameters, and the MaterialInstance BaseTypeInfo* slot itself.
    // engine_funcs.cpp ALREADY has a full "if (!fn_X) fn_X = ResolveExport(...)"
    // fallback for all four functions (lines ~3789-3803), explicitly commented
    // as having been added because these exe slots stay null in standalone
    // (MapUI::SetupImage never bound its minimap textures). So registering the
    // exe slots here changes NOTHING for those four — same Runtime function
    // pointer either way — but completes the PE table faithfully.
    // (0x140151838 ??0MaterialInstance: row deleted at ENDGAME B.3.9 EXA — its
    // one reader, engine_funcs' fn_MaterialInstance_Ctor, went with the builders.)

    // (0x140151350 Graphics::MaterialInstance type-info import slot: row deleted
    // at ENDGAME B.3.9 TI-REST -- every reader uses
    // Graphics::MaterialInstance::TypeId().)

    // (0x140151858 Graphics::Material, 0x140151f20 Graphics::GraphicsMesh and
    // 0x140151420 Graphics::Particle type-info import slots: rows deleted at ENDGAME
    // B.3.9 TR -- every reader uses the class's TypeId().)

    // (0x140151f38 Graphics::ParticleInstance::_frameIndex: row deleted at ENDGAME
    // B.3.9 SS2 — GameUpdateLoop::Update increments the class static.)

    // ------------------------------------------------------------------
    // String / Filename operators.
    // ------------------------------------------------------------------

    // System::String::operator+= and System::Filename::operator< — REAL FIX.
    // code/Game/Dialog/moddetailsdialog.cpp calls both UNGUARDED:
    //     if (fn_Filename_Less(filename, node)) { ... }              // line ~131
    //     else if (fn_Filename_Less(node, filename)) { ... }         // line ~133
    //     fn_String_AppendStr(conflictResult, entry);                // line ~235
    //     fn_String_AppendStr(conflictResult, closeTag);             // line ~243
    // engine_funcs.cpp resolves both SOLELY from these exe slots with no
    // ResolveExport fallback (unlike Filename::operator=, 0x1401529e0, which
    // has one — see below). Unregistered, opening the Mod Details dialog and
    // reaching its conflict-list building code is an EXEC-of-0 crash — the
    // same defect class as the "fn_String_Append EXEC'd 0 on ~1 name in 8"
    // bug already fixed in the new-game name generator (repo memory:
    // reference_stub_comment_outlived_the_port_that_replaced_it /
    // project_newgame_menu_name_generator). Confirmed these two names ARE
    // real Runtime-x64.dll exports (they're literally the exe's own PE import
    // strings at these addresses per docs/waves/lanes/lane3.txt), so the
    // "non-exported" comment in engine_funcs.cpp predates this session's
    // PE-import-table discovery and is stale.
    RegisterLane3Slot(0x140151a00,
        (uintptr_t)ResolveExport("??YString@System@@QEAA?AV01@AEBV01@@Z"));   // String::operator+=
    RegisterLane3Slot(0x140151a20,
        (uintptr_t)ResolveExport("??MFilename@System@@QEBA_NAEBV01@@Z"));    // Filename::operator<

    // System::operator+(String, const char*) / (String, String) — used only by
    // code/Game/Dialog/spritecaptureui.cpp, both call sites already
    // null-guarded (`if (fn_String_Concat)` / `if (fn_String_ConcatSS &&
    // editText)`), so unregistered they silently produce an incomplete sprite-
    // capture output filename rather than crashing. Registering restores the
    // full path concatenation; low-traffic (Sprite Capture is a dev/modding
    // tool, not mainline gameplay) but a genuine correctness fix with no
    // fallback otherwise. fn_String_ConcatSS2 (engine_funcs.h) is bound to the
    // same 0x140151a08 slot as fn_String_Concat but has no callers found.
    RegisterLane3Slot(0x140151a08,
        (uintptr_t)ResolveExport("??HSystem@@YA?AVString@0@AEBV10@PEBD@Z"));  // operator+(String,const char*)

    // System::Filename::operator= — already has a full ResolveExport fallback
    // in engine_funcs.cpp (~line 5058-5063), so this is a no-op; registered
    // for PE-table completeness only.

    // ------------------------------------------------------------------
    // DELIBERATELY NOT REGISTERED — see report for the two addresses this
    // wave decided to leave alone (0x1401527f8 ParticleInstance::SetParticle
    // and 0x140152830 Achievement::Goal::Goal), both because an EXISTING
    // engine_funcs.cpp consumer reads that same exe slot under a different,
    // mismatched name/signature, and registering the address's REAL retail
    // content would silently rebind that unrelated consumer to the wrong
    // function. Neither has a live consumer that actually wants the real
    // content today, so leaving them unregistered costs nothing and avoids
    // the landmine. See standalone/address_registry.cpp / engine_funcs.cpp
    // call sites at 0x1401527f8 (mislabeled "DialogInstance::GetRoot") and
    // 0x140152830 (mislabeled "Audio::Scene::PlayEffect").
    // ------------------------------------------------------------------
}
