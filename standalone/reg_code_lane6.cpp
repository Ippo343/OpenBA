// standalone/reg_code_lane6.cpp
// Unported exe .text wave (2026-09-04), LANE 6 — the five rows of
// docs/waves/lanes/code_lane6.txt whose retail body is ALREADY fully ported in
// this tree and whose exe address was simply never registered.
//
// Nothing here changes behaviour today: every one of the five is reached in
// standalone by C++ symbol (a vtable slot we build, or a direct call), and the
// only remaining consumer of the *address* is an `fn_* = ResolveAddr(...)` in an
// InitFuncs that has ZERO callers. Registering them (a) makes those pointers
// truthful instead of silently null, (b) closes the row in the unregistered
// census, and (c) removes a live trap: the next person who wires a retail-shaped
// caller through ResolveAddr gets the real function instead of a 0.
//
// (The census counts ResolveAddr MISSES, not calls — `total=1` on each of these
// rows is the single InitFuncs resolve, which is why none of them has ever
// produced an EXEC-of-0.)
//
// ENDGAME B.3.8 close: the DragTool::OnKey, EntityUI::OpenPauseMenu and
// ToolbarDescription cleanup rows went with the dead engine_funcs pointers that were
// their only readers; the DecalComponent::Draw row is what is left.
//
// The other three rows of the lane are DELIBERATE leaves and are NOT registered
// here — see the report / the notes at their call sites:
//   0x1400fcc90  EditTool deselect+close      — a live cc90 fires EVERY FRAME from
//                                               edittool.cpp:622 (our 0x140100380
//                                               screen-rect helper fails for
//                                               citizens) and wipes the outline.
//   0x140101ee0  EntityUI::Startup            — gameupdateloop.cpp:2556 runs the SA
//                                               tree builder unconditionally, so
//                                               registering DOUBLE-inserts the
//                                               8 tool-mode AVL nodes.
//   0x1401026a0  EntityUI::Update             — gameupdateloop.cpp:1417-1423 runs
//                                               Camera_UpdateHotkeys/Movement +
//                                               EntityUI_ToolTick_SA first, so
//                                               registering double-drives the
//                                               camera and the active tool.
#include "resolve.h"
#include <cstdint>

#include "Component/decalcomponent_funcs.h"        // DecalComponent_Draw
#include "Component/statusiconcomponent_funcs.h"   // StatusIconComponent type

// central live-feature gate (default-on table + env kill-switch)

void RegisterCodeLane6Batch() {
    // 0x14011b560 — DecalComponent::Draw (the terrain ground decal: a StorageYard's
    // brown dirt foundation and every other DecalDescription-bearing building).
    // Fully ported as DecalComponent_Draw and installed into DecalComponent's live
    // vtable slot 6.
    // decalcomponent.cpp's own s_fn_OriginalDraw = ResolveAddr(0x14011b560) is used
    // only by a _DEBUG printf.
    RegisterExeAddr(0x14011b560, reinterpret_cast<uintptr_t>(&DecalComponent_Draw));

    // 0x14012bbd0 — StatusIconComponent slot 8 (the icon POSITIONING pass) is a real
    // virtual since ENDGAME B.3.8 and statusiconcomponent.cpp no longer reads its own
    // address back, so its row is gone.
}
