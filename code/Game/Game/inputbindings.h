#pragma once
// code/Game/Game/inputbindings.h
// Default input-binding registration helper.
//
// Original: FUN_1400992f0 in Application-x64.exe.
// Walks the global KeyMap (via g_pKeyMapObjectExref triple deref), clears
// existing bindings, registers all 50 default bindings (mouse axes, hotkeys,
// camera presets, toolbar slots), then loads any saved overrides via
// Input::KeyMap::Load.
//
// Called by:
//   - PlacementTool_Ctor (game-startup binding install)
//   - OptionsDialog reset-to-defaults action
//   - FUN_140072740 (called from OptionsDialog accept)
//
// In standalone mode we invoke this from RunGameMain right after Engine::Startup
// so the KeyMap is populated before any dialog (including OptionsDialog) opens
// and ButtonElement::Update starts polling KeyMap::Pressed for hotkey hints.

namespace Game {

// Populates the global Input::KeyMap with the 50 default Banished bindings,
// then calls Input::KeyMap::Load to merge any user-saved overrides.
//
// No-op (with a printf) if g_pKeyMapObjectExref or any of fn_KeyMap_* is
// unresolved — never crashes if engine fns aren't ready.
void RegisterDefaultInputBindings();

} // namespace Game
