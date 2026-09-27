// standalone/reg_unreg_lane2.cpp
//
// Lane 2 of the "unregistered dispatch-slot wave" (docs/waves/unregistered_slots_wave.md,
// 2026-09-04). Covers the 23 UI/dialog/input import slots in docs/waves/lanes/lane2.txt.
//
// Audit method: for every address, grep every consumer in code/ and engine/funcs/, then
// read the call site to see whether the consumer already has a ResolveExport-by-mangled-
// name (and/or direct Runtime-address) fallback that fires when the raw exe IAT slot comes
// back null. Registering a slot that already has a proven-working fallback changes nothing
// observable; registering one that does NOT (an unguarded call, or a guarded call that
// silently skips real work) restores retail behaviour.
//
// Result: 17 of the 23 addresses already resolve correctly today via a fallback in
// engine/funcs/engine_funcs.cpp or in the consuming .cpp itself (see the per-address
// table in the wave report). Only the six below were genuine standalone gaps.
//
// This file only supplies .rdata slot storage the existing guarded call sites already
// read via ResolveAddr(Module::Exe, addr) — it does not touch address_registry.cpp,
// engine_funcs.*, or code/Game/.

#include "resolve.h"
#include <cstdint>

void RegisterUnregLane2() {
    // ------------------------------------------------------------------
    // UI::String::String(ExternalPtr<StringTable>, String const& name) — the NAMED
    // single-arg-external ctor. Mangled name is IDENTICAL to the one
    // fn_UIString_Ctor (engine_funcs.cpp) already resolves via ResolveExport, so that
    // consumer (buildui.cpp's resource-row label/tooltip text) already works. The
    // ONLY thing that reads this exe dispatch slot directly is
    // fn_UIString_Ctor_Entry (engine_funcs.h/.cpp), which has a DIFFERENT (2-arg,
    // no name) typedef and currently has zero live callers — buildui.cpp switched
    // off it in favor of fn_UIString_Ctor/fn_UIString_DefaultCtor after finding the
    // slot unregistered (see buildui.cpp:328-337). Registering it is a correctness
    // fix for whoever calls fn_UIString_Ctor_Entry next; it changes nothing observable
    // today because there is no such caller yet.
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // DialogInstance::GetController — restores nomadcomponent.cpp's dead branch.
    // edittool.cpp/edittool_select.cpp/statusiconui.cpp/tutorial_wave.cpp all call
    // through engine_funcs.cpp's fn_DialogInstance_GetController, which already has
    // a ResolveExport + Runtime-address(0x180113320) fallback and works fine.
    // BUT code/Game/Component/nomadcomponent.cpp keeps its OWN private copy
    // (s_fn_GetController, Ensure3480()) that reads ONLY the raw exe dispatch slot
    // with no fallback of any kind:
    //     slot = ResolveAddr(Module::Exe, 0x140151768);
    //     if (slot) s_fn_GetController = *reinterpret_cast<fn_GetController_t*>(slot);
    // and the caller null-guard-and-SKIPS:
    //     if (s_fn_GetController) s_fn_GetController(entity, ctrlBuf);
    // (nomadcomponent.cpp:392,471). That call site is FUN_140103480's "close the
    // removed entity's open EntityUI info dialog" path — with the slot unregistered
    // it has been silently permanently dead in standalone: removing/destroying an
    // entity whose info panel is open never closes that panel. Registering the slot
    // restores retail (a null-guard-and-skip case, not a fallback-changes-behaviour
    // case — safe).
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // UI::SpriteSheet::SpriteSheet() / AddSpriteInfo / SetSpriteInfo, and
    // UI::Sprite::Sprite(ExternalPtr<SpriteSheet>, int index) — these four
    // (engine_funcs.cpp fn_SpriteSheet_Ctor/fn_SpriteSheet_AddSpriteInfo/
    // fn_SpriteSheet_SetSpriteInfo/fn_Sprite_Ctor_ByIndex) have NO ResolveExport or
    // Runtime-address fallback at all in engine_funcs.cpp — only the raw exe
    // dispatch slot, which is unregistered. Every consumer calls them UNGUARDED
    // (no null check on the function pointer itself):
    //   - code/Game/Dialog/moddialog.cpp:453,473,476,482 — builds each installed
    //     mod's thumbnail SpriteSheet/Sprite when the Mods dialog populates its list.
    //   - code/Game/Dialog/noiseui.cpp:103,131,139,164 — builds the noise-preview
    //     thumbnail sprite.
    //   - code/Game/Dialog/saveloaddialog.cpp:313,348,357,362 — builds each save
    //     slot's screenshot thumbnail sprite.
    //   - code/Game/Dialog/mapui.cpp:103,133,142,171 — same pattern, but that call
    //     site DOES have its own upfront `if (!fn_SpriteSheet_Ctor || ...) return;`
    //     guard (mapui.cpp:46), so it degrades gracefully; the other three do not.
    // With all four null, opening the Mods dialog with any mod installed, opening
    // the world-gen noise preview, or opening Save/Load (which builds a thumbnail
    // per save file) EXEC's a null function pointer. Registering restores retail;
    // no consumer treats "unregistered" as a deliberate fallback branch.
    // ------------------------------------------------------------------
    // (0x140151848 ??0SpriteSheet: row deleted at ENDGAME B.3.9 EXA — its one
    // reader, engine_funcs' fn_SpriteSheet_Ctor, went with the thumbnail builders.)

    // ------------------------------------------------------------------
    // The remaining 17 lane-2 addresses were audited and found to already resolve
    // correctly in standalone via a fallback elsewhere, so registering the raw exe
    // slot would be a no-op (same resulting function pointer). Left unregistered
    // deliberately — see the wave report for the call site read for each:
    //
    //   0x140151488 _typeInfo@StringTable  — g_TypeId_StringTable (engine_funcs.cpp);
    //       every consumer (optionsdialog.cpp LoadStringTable, inputbindings.cpp
    //       LoadInputNameTables, serialize_externals.cpp) already lazy-resolves the
    //       type id by name via Factory::GetTypeIdFromName/ResolveTypeIdByNameLazy
    //       when this is null.
    //   0x140151718 GetText@StringTable    — fn_StringTable_GetText resolves by
    //       ResolveExport; citizen_updatehealth.cpp and debugui.cpp both carry their
    //       own local ResolveExport fallback too.
    //   0x140151758 KeyMap::Pressed, 0x1401528f0 KeyMap::Released — every tree
    //       caller calls the Input::KeyMap member since B.3.7 (K3); the
    //       engine_funcs globals are deleted. (This line used to say
    //       fn_KeyMap_Released resolved by ResolveExport: it was bound ONLY to
    //       0x1401528f0 and was always null -- B37_RULES 6.3 defect 1.)
    //   0x1401517a8 GetRoot, 0x140151dd0 SetUpperLeft, 0x140152750 GetPaddedSize —
    //       engine_funcs.cpp fn_DialogInstance_GetRoot/SetUpperLeft/fn_Element_
    //       GetPaddedSize resolve by ResolveExport + Runtime-address fallback;
    //       edittool.cpp falls back to those when its own raw-slot read misses.
    //   0x140151a30 OnElementEvent, 0x140151a60 AddChild@Element — engine_funcs.cpp's
    //       fn_DialogInstance_OnElementEvent / fn_Element_AddChild both resolve by
    //       ResolveExport + Runtime-address fallback first, independent of these slots.
    //       (This note used to add: "engine_funcs.cpp reads exe slot 0x140151a60 as a
    //       FALLBACK for fn_DialogInstance_AddController — a naming mismatch, but
    //       harmless because that pointer's own ResolveExport already succeeds so the
    //       fallback never fires." The mismatch was real; the reassurance was NOT.
    //       That ResolveExport used a mangled name the DLL does not contain
    //       (?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBV...),
    //       so it returned null on every run and the fallback ALWAYS fired, binding
    //       fn_DialogInstance_AddController to Element::AddChild. It stayed harmless
    //       only because the pointer had zero callers. Both the pointer and its
    //       resolve were deleted 2026-09-07 by Wave 7 lane B — see
    //       docs/runtime_port/WAVE7_LANE_B.md §5 and the note in engine_funcs.h.)
    //   0x140151a90 / 0x1401529d0 AddController (6-arg / 5-arg tab), 0x1401529c8
    //       TabElement::AddTab — engine_funcs.cpp already falls back to hardcoded
    //       Runtime RVAs (0x180113470 / 0x1801133b0 / 0x1800da130) documented as
    //       "verified addresses" fixing exactly this gap in an earlier session.
    //   0x140151b70 / b78 / b80 Video/Audio Configuration + Input::Device
    //       singletons — engine_funcs.cpp already falls back to the Runtime DLL's
    //       own singleton storage address (Runtime 0x1801fe7a0 / 0x1801fe758, and
    //       ResolveExport of Input::Device::_singleton) when the exe exref is null;
    //       that storage address is the SAME thing the exe exref would have pointed
    //       to once populated, so registering is a no-op.
    //   0x140151ca8 UI::String default ctor — fn_UIString_DefaultCtor already falls
    //       back to ResolveExport("??0String@UI@@QEAA@XZ") (engine_funcs.cpp:2949-2951,
    //       comment confirms "resolves in standalone").
    //   0x1401529b0 SpriteSheet::GetSpriteCount — statusiconcomponent.cpp's
    //       s_fn_SpriteSheet_GetSpriteCount already falls back to
    //       ResolveExport("?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ") when the slot
    //       misses (this is the one lane-2 address the census shows resolved during
    //       the load window — that's this fallback firing, not the raw slot).
    // ------------------------------------------------------------------
}
