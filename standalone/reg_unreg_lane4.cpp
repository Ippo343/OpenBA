// standalone/reg_unreg_lane4.cpp
// Unregistered-slot wave, lane 4 (non-import .rdata/.data addresses).
//
// ENDGAME B.3.9 TR: the lane's 30 exe type-id storage cells (the g_slots table filled
// by name after Object::Factory::Startup) are gone -- every reader reads X::TypeId() of
// the class at the cell (INVENTORY_C Table 1). What is left are the three .rdata float
// constants below.
#include "resolve.h"
#include <cstdint>

namespace {
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

void RegisterUnregLane4RdataConsts() {
    RegisterExeAddr(0x1401720c0, reinterpret_cast<uintptr_t>(&k_bits_1720c0));
    RegisterExeAddr(0x1401720f4, reinterpret_cast<uintptr_t>(&k_bits_1720f4));
    RegisterExeAddr(0x1401721cc, reinterpret_cast<uintptr_t>(&k_bits_1721cc));
}

// ═══════════════════════════════════════════════════════════════════════════
// DELIBERATELY NOT REGISTERED — lane-4 addresses examined and left alone.
// Recorded here so the next sweep does not "close" them without reading this.
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
