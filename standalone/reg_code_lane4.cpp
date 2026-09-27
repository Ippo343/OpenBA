// standalone/reg_code_lane4.cpp
//
// LANE 4 of the unported-exe-`.text` wave (2026-09-04, docs/waves/code_fns_wave.md).
// Registers the four addresses in the lane whose reimplementation ALREADY EXISTS
// in the tree and whose retail ABI we can bind exactly. Every one of these was
// resolved through `ResolveAddr(Module::Exe, ...)` by engine_funcs / a dialog and
// got 0 back, so the `fn_*` pointer built from it was permanently NULL.
//
// Nothing here changes behaviour on a default run TODAY (see per-row notes: all
// four had zero live call sites, because each caller was long ago rewritten to
// call the local C++ port by name). What it buys is that the `fn_*` pointers stop
// being NULL landmines for the next caller that is ported retail-shaped, and four
// rows leave the unregistered census. The rows in this lane that would have
// CHANGED behaviour (0x1400938d0 Weather::DrawUpdate, 0x14009e8c0 the region /
// pathfinder system build) are deliberately NOT registered — see the report.
//
// Integrator: call RegisterCodeLane4() from the registration sequence in
// standalone/address_registry.cpp alongside the other reg_*.cpp entry points.

#include "resolve.h"
#include "engine_funcs.h"
#include "system/Value.h"     // ENDGAME B.3.4b System callers: System::Time
#include <cstdint>

// ------------------------------------------------------------------
// 0x140089ed0 CitizenBehavior::CountResources
// 0x14008c100 CitizenBehavior::CheckThresholds
// 0x14008a730 CitizenBehavior::CollectStats
//
// All three are called from exactly ONE retail site: Entity_impl_c2b0
// (0x14008c2b0 = CitizenBehavior::Update) at 0x14008c2de / 0x14008c2e9 /
// 0x14008c36c. That caller is ported (code/Game/Entity/citizenbehavior.cpp:301)
// and registered (standalone/reg_citizenbehavior_tick.cpp), and it calls the
// three C++ ports directly by name — which is why the town tick works today
// with these three addresses unregistered.
//
// The only other xrefs are each function's own .pdata unwind record plus two
// isolated .rdata words for CollectStats (0x140191240 / 0x1401912d0) that have
// no xrefs of their own, i.e. xdata scope-table entries, NOT a dispatch table.
//
// So: no live consumer, but `fn_CitizenBehavior_{CountResources,CheckThresholds,
// CollectStats}` (engine/funcs/engine_funcs.cpp:4837-4842) are NULL today, and
// citizenbehavior.cpp:1750 already has a commented-out call through one of them.
// The ports' signatures are byte-identical to retail's, so bind them.
// ------------------------------------------------------------------
struct CitizenBehaviorRaw;
extern void CitizenBehavior_CountResources(CitizenBehaviorRaw* self);                     // 0x140089ed0
extern void CitizenBehavior_CheckThresholds(CitizenBehaviorRaw* self, int64_t* deltaTime); // 0x14008c100
extern void CitizenBehavior_CollectStats(CitizenBehaviorRaw* self);                       // 0x14008a730

// ------------------------------------------------------------------
// 0x140099c20 KeyMap::BuildKeyName
//
// Retail ABI (Ghidra, FUN_140099c20):
//     String* f(void* keyMapContainer, String* retval /*UI::String, 16B*/,
//               int device, uint64_t inputIndex)
// RCX = the KeyMap CONTAINER (its +0xD0 is the keyNames StringTable), RDX = the
// caller-provided return buffer, R8 = device (<0 => "Unbound"), R9 = key index.
// Returns RDX.
//
// Our port is Game::Hotkey_BuildKeyName (code/Game/Dialog/hotkeytooltipui.cpp:74),
// which drops param_1 and re-derives the same container from *g_pKeyMapSingleton.
// That substitution is already justified in that file's comment (every retail call
// site passes exactly that singleton), but it means the port CANNOT be registered
// at this address directly — the shapes differ by a leading parameter and the
// return value. Registering the port raw would be the "lifted ABI" landmine from
// reference_lifted_abi_swap_invisible_to_the_proven_caller. Hence this shim.
//
// Retail callers: HotKeyToolTipUI_Serialize (x2), FUN_1400729a0 /
// FUN_140073490 (the OptionsDialog key-bind rows — both superseded by
// code/Game/Dialog/optionsdialog.cpp), TutorialImageHotKey_Update,
// TutorialImageInputKey_Update. Every one of those is ported and calls
// Hotkey_BuildKeyName by name, so this registration is currently only about
// making engine_funcs' fn_GetInputKeyName (declared with the correct 4-param
// retail signature at engine_funcs.h:605, NULL since it was added) non-null.
// ------------------------------------------------------------------
namespace Game { void Hotkey_BuildKeyName(void* retval, int device, int keyIndex); }

static void* Lane4_KeyMap_BuildKeyName(void* /*keyMapContainer*/, void* retval,
                                       int device, uint64_t inputIndex) {
    // param_1 intentionally ignored: the port re-derives the identical container
    // from *g_pKeyMapSingleton (see hotkeytooltipui.cpp). Retail returns RDX.
    Game::Hotkey_BuildKeyName(retval, device, static_cast<int>(inputIndex));
    return retval;
}

// ------------------------------------------------------------------
// 0x1400932a0 Game::GetMonthFromTime
//
// Retail body (verified against the decompile):
//     float s = System::Time::AsSeconds(weatherMgr + 8);
//     int m = (int)((s / 3600.0f) * 12.0f);   // DAT_14017225c / DAT_1401721b8
//     return m > 11 ? 0 : m;
//
// Sole retail caller is DebugUI_Slot9 at 0x140060c0a, which is ported
// (code/Game/Dialog/debugui.cpp:1058) and calls its own file-static copy
// DebugUI_GetMonthFromTime_Impl. Because that copy is `static`, it cannot be
// referenced from here; this is a byte-identical re-statement of the same three
// lines, with the same two hardcoded .rdata constants (the exe's .rdata is not
// mapped in standalone, so ResolveAddr on them returns 0 — debugui.cpp hardcodes
// them for the same reason).
// ------------------------------------------------------------------
static int Lane4_Game_GetMonthFromTime(void* weatherMgr) {
    float seconds = reinterpret_cast<const System::Time*>(
        reinterpret_cast<uintptr_t>(weatherMgr) + 8)->AsSeconds();   // B.3.4b bridge
    int month = static_cast<int>((seconds / 3600.0f) * 12.0f);
    if (month > 11) month = 0;
    return month;
}

extern "C" void RegisterCodeLane4() {
    RegisterExeAddr(0x140099c20, reinterpret_cast<uintptr_t>(&Lane4_KeyMap_BuildKeyName));
    RegisterExeAddr(0x1400932a0, reinterpret_cast<uintptr_t>(&Lane4_Game_GetMonthFromTime));
}
