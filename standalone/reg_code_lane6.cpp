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

#include "Player/dragtool.h"                       // DragTool_OnKey
#include "Component/decalcomponent_funcs.h"        // DecalComponent_Draw
#include "Component/statusiconcomponent_funcs.h"   // StatusIconComponent type
#include "Component/ToolbarDescription.h"

// central live-feature gate (default-on table + env kill-switch)

// The in-game pause-menu opener (code/Game/Dialog/toolbardialog.cpp). Retail is
// void(gameSubsys); ours hands the dialog back so an unattended driver can address
// it — a wider return is ABI-compatible with a void call site.
extern "C" void* EntityUI_OpenPauseMenu(void* gameSubsys);

// ToolbarDescription is a real class (code/Game/Component/ToolbarDescription.h): its
// slot 0 is the SCALAR-deleting dtor (flags&1 = MemFree self). Retail 0x14012db70 is
// the CLEANUP half only and takes ONE argument — calling a two-arg form directly from
// a one-arg retail call site would read a stale RDX and could MemFree a stack/embedded
// object. Adapt explicitly: flags 0 = do NOT free the storage.
static void ToolbarDescription_Cleanup_Adapter(void* self) {
    reinterpret_cast<ToolbarDescription*>(self)->ToolbarDescription::DeletingDtor(0u);   // B.3.8 bridge
}

void RegisterCodeLane6Batch() {
    // 0x1400fc6b0 — DragTool::OnKey, the shared drag state machine for
    // BridgeTool / RoadTool / FencedTool / DragPlacementTool (retail vtables
    // 0x140161b30 / 0x140161bc0 / 0x140161c40 / 0x140161e10 all hold it at +0x30).
    // Ported in full as DragTool_OnKey (code/Game/Player/dragtool.cpp) and wired
    // into each of our tool vtables by symbol. The address stayed unregistered, so
    // engine_funcs.cpp's fn_DragTool_OnKey has been null since it was added; it has
    // no callers today, which is the only reason that never showed up as a dead drag.
    RegisterExeAddr(0x1400fc6b0, reinterpret_cast<uintptr_t>(&DragTool_OnKey));

    // 0x140101ce0 — EntityUI::OpenPauseMenu (the IN-GAME menu: Resume / Save /
    // Options / Quit to Menu). Retail's two callers are ToolbarDialog_OnRemove case 3
    // (0x1400b96ae) and EntityUI::Update's lost-focus branch (0x140102740). Fully
    // ported as EntityUI_OpenPauseMenu; both of our call sites reach it by symbol.
    // engine_funcs.cpp still resolves this address into fn_Toolbar_ToggleAll — a
    // MISNOMER left over from when the function was thought to be a toolbar toggle
    // (see the header note above its typedef); that pointer has no callers.
    RegisterExeAddr(0x140101ce0, reinterpret_cast<uintptr_t>(&EntityUI_OpenPauseMenu));

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

    // 0x14012db70 — ToolbarDescription::~ToolbarDescription (the cleanup half: destroy
    // the String array at +0x50 and free it, destroy the four Strings at +0x48/+0x40/
    // +0x38/+0x28, restore the base vtable, chain to External::~External). The class's
    // DeletingDtor; registered through the one-arg adapter above because retail's
    // signature has no flags word.
    // engine_funcs.cpp's fn_ToolbarDescription_cleanup has no callers.
    RegisterExeAddr(0x14012db70,
                    reinterpret_cast<uintptr_t>(&ToolbarDescription_Cleanup_Adapter));
}
