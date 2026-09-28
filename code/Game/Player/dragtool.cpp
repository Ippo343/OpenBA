// code/Game/Player/dragtool.cpp
// Clean reimplementation of DragTool vtable methods from Application-x64.exe.
//
// DragTool (ABSTRACT, 0x38 bytes) — base class for click-and-drag placement tools.
// Extends Tool (0x28 bytes), vtable at 0x140161c10 (15 slots).
//
// Sub-classes: BridgeTool, RoadTool, WallTool, etc.
// DragTool::OnKey implements the shared drag lifecycle:
//   1. key==0 (key up): close dialog if open, check Released -> reset dragState
//   2. key!=0 && dragState==0 (idle): check Pressed -> pick start cell, open dialog
//   3. key!=0 && dragState==1 (dragging): update end cell, call virtual BuildPath,
//      update UI via operator_impl_fc60, handle release -> commit or cancel
//
// Decompiled from Application-x64.exe.

#include "Player/dragtool.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "Player/exe_bodies.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "input/Input.h"        // ENDGAME B.3.7 callers (K3)
#include "Object/exe_type.h"   // DEFINE_EXE_ABSTRACT_TYPE

// ============================================================
// DragTool members (dragtool.h). ENDGAME B.3.8 TOOL: the hand-rolled
// `DragTool_Dtor` (it restamped two UNREGISTERED tables, i.e. stored a null
// vptr, B38 D9 #3) is gone; the compiler's dtor chain restamps the real ones.
// ============================================================

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(DragTool, "Tool")

// [6] 0x1400fc6b0 OnKey and [9] 0x1400fc640 OnCancel: bodies below.

// ============================================================
// Exe helpers, called by name (Player/exe_bodies.h):
//   0x140101270 PlayerRay_ScreenToCell_101270 -- ray-picks the map cell under the
//               cursor: row*gridSize + col, or -1 if off-map
//   0x1400ff9e0 ToolUI_OpenToolDialog_ff9e0 -- closes an existing dialog, loads
//               "Dialog/CreateBar.rsc", creates the DialogInstance with its ToolUI
//               child, adds it to the UI manager. Args: (dialogIBP_ptr, retval_ptr,
//               descriptionPtr)
//   0x1400ffbf0 DragTool_PlayTickSound_ffbf0 -- the "tick" when endCell changes
//               during a drag (a timer at +0x48 on the IBP storage stops rapid-fire)
//   0x140005160 DragTool_FindDescription_5160 -- the description (DAT_1401c7f58)
//               search; the found pointer lands in retval
//   0x1400ffc60 UpdateToolUI_fc60 (operator_impl_fc60) -- cost text, status text,
//               icon tint and screen position of the placement dialog
//   0x1400ff980 DragTool_CloseToolDialog_ff980 -- closes the dialog if open, nulls
//               the IBP
//   0x1401000f0 DragTool_PlayPlaceSound_1000f0 / 0x140100140
//   DragTool_PlayErrorSound_100140 -- the place / error sounds (sound ids at
//               g_pKeyMapSingleton + 0x88 / + 0x90)
// All eight are registered, so the SA no-op fallbacks that used to stand in for a
// null resolve are gone with the resolver.
// Import slot 0x1401513f8 (Volume::Box::Box) is NOT filled in standalone (a STUB):
// its reader below is its null arm.
// ============================================================

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
// g_pInputConfigSingletonExref needs double deref: slot -> ptr -> KeyMap
// (Ghidra shows: MOV RAX,[0x140151738]; MOV R8,[RAX]; MOV R14,[R8])
static inline void* GetKeyMap() {
    // Triple deref: slot → External ptr → wrapped ptr → KeyMap object
    // Original asm: MOV RAX,[slot]; MOV R8,[RAX]; MOV R14,[R8]
    return ***(void****)g_pInputConfigSingletonExref;
}

// ============================================================
// Helper: get UserInterface from GameWorld singleton
// ============================================================
static inline void* GetUserInterface() {
    return *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x98);
}

// ============================================================
// DragTool::OnKey — Main input handler (vtable +0x30, Slot6)
// ============================================================
// Original: 0x1400fc6b0 (217 instructions)
//
// The key parameter:
//   key == 0: key-up event (mouse button released)
//   key != 0: key-down / held event (mouse button pressed/held)
//
// State machine:
//   dragState==0 (idle):
//     On key-down with Pressed(0,5): pick start cell, set startCell=endCell,
//     if valid: set dragState=1, open dialog
//
//   dragState==1 (dragging):
//     Ensure dialog is open, pick current cell, update endCell
//     If endCell changed: play tick, update lastEndCell
//     Call virtual BuildPath (+0x70) for segment count + path type
//     Find description, extract cost string
//     Call UpdateToolUI for UI overlay
//     On release: check singleClick behavior
//
//   key==0: close dialog if open, if Released(0,5): reset dragState+singleClick
//
char DragTool::OnKey(char key) {
    void* keyMap = GetKeyMap();

    // IMPORTANT: The original code re-reads [self+0x10] at each use site, NOT cached.
    // CalcPath/entity creation can trigger memory operations that invalidate cached pointers.
    // Use DIALOG_IBP() macro to always re-read fresh from the struct.
#define DIALOG_IBP() ((uint64_t*)dialogPtr)
    // (no cached variable — always use DIALOG_IBP() to re-read fresh)

    if (key == 0) {
        // ---- Key-up event: cleanup ----

        // Close dialog if the IBP has a live pointer
        // Original: CMP [RBX], 0 — dereferences the pointer-to-IBP
        if (DIALOG_IBP() != nullptr && *DIALOG_IBP() != 0) {
            uint64_t tempIBP = *DIALOG_IBP();  // copy the IBP value
            reinterpret_cast<UI::UserInterface*>(GetUserInterface())->CloseDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&tempIBP));
            new (&tempIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *DIALOG_IBP() = tempIBP;  // write null IBP back through the pointer
        }

        // Check if the primary action button was released
        // ENDGAME B.3.7: fn_KeyMap_Released was bound only to the unregistered exe
        // slot 0x1401528f0 and was always null, so this reset never ran; the member
        // call restores retail's key-up path (CALLER_SURFACE.md 6, defect 1).
        if (reinterpret_cast<const Input::KeyMap*>(keyMap)->Released(0, 5)) {  // B.3.7 bridge
            // Reset both dragState and singleClick (original does MOV word [self+0x28], 0)
            dragState = 0;
            singleClick = 0;
        }
    }
    else if (dragState == 0) {
        // ---- Idle state: check for drag start ----
        // fn_KeyMap_Pressed at dispatch 0x140151758 — checks if button newly pressed
        bool pressed = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
        if (pressed) {
            int cell = PlayerRay_ScreenToCell_101270((uintptr_t)keyMap);
            startCell = cell;
            endCell = cell;

            if (cell >= 0) {
                // Begin dragging: set state, open placement dialog
                dragState = 1;
                singleClick = 0;
                uint64_t retval;
                // Original: FUN_1400ff9e0(*(longlong**)(self+0x10), &retval, *(self+0x20))
                // Passes self->dialogPtr (pointer to IBP), not &self->dialogPtr
                ToolUI_OpenToolDialog_ff9e0(DIALOG_IBP(), &retval,
                    (uint64_t)descriptionPtr);
            }
        }
    }
    else {
        // ---- Dragging state: update path and handle release ----

        // Ensure dialog is open (re-open if it was somehow closed)
        // Original: CMP [dialogIBP], 0 — checks IBP value through the pointer
        if (*DIALOG_IBP() == 0) {
            uint64_t tempStr;
            ToolUI_OpenToolDialog_ff9e0(DIALOG_IBP(), &tempStr,
                (uint64_t)descriptionPtr);
        }

        // Pick current cell under cursor
        int currentCell = PlayerRay_ScreenToCell_101270((uintptr_t)DIALOG_IBP());
        if (currentCell >= 0) {
            endCell = currentCell;
        }

        // If endCell changed, play tick sound and update lastEndCell
        if (lastEndCell != endCell) {
            lastEndCell = endCell;
            DragTool_PlayTickSound_ffbf0((uintptr_t)DIALOG_IBP());
        }

        // ---- Virtual CalcCost (slot 14, +0x70) ----
        // (startCell, endCell, &pathType, &tileH, &tileW, &extraParam) -> segment count
        // pathType: 0=normal, 1=single, 2=invalid
        uint32_t pathType[2] = {0};
        int tileH[2] = {0};
        int tileW[2] = {0};
        int extraParam = 0;
        int segmentCount = CalcCost(startCell, endCell, pathType, tileH, tileW, &extraParam);

        // ---- Cost tooltip (FindDescription -> cost string -> UpdateToolUI) ----
        System::String costString;
        {
            uint64_t descResult = 0;
            DragTool_FindDescription_5160((uintptr_t)descriptionPtr, &descResult);

            if (descResult != 0) {
                // Description has a string array at +0x50 (ptr) and +0x58 (count).
                bool costOverflow = (segmentCount >= *(int*)((uintptr_t)descResult + 0x58));

                if (!costOverflow) {
                    uintptr_t strArrayBase = *(uintptr_t*)((uintptr_t)descResult + 0x50);
                    // Retail copy-constructs a temporary from the array slot, copies that
                    // into costString, then releases the temporary. The scope does both.
                    System::String tempString(
                        *reinterpret_cast<const System::String*>(
                            strArrayBase + (uint64_t)segmentCount * 8));
                    costString = tempString;
                }
                // else: retail's srcString is the null String, so costString stays empty
                // and its release is a no-op.

                // ---- Init Transform and Box for UI positioning ----
                // ??0Transform@Math@@QEAA@XZ  retail 0x180015080 (was the
                // 0x140151328 dispatch slot)
                Math::Transform transform;
                uint8_t box[24];        // Volume::Box
                // PORT GAP 0x1401513f8 Volume::Box::Box: the import slot is not filled
                // in standalone; retail calls it here.

                // ---- Call UpdateToolUI (operator_impl_fc60 at 0x1400ffc60) ----
                UpdateToolUI_fc60(
                    DIALOG_IBP(),                          // param_1
                    tileH[0],                              // param_2
                    tileW[0],                              // param_3
                    &costString,                           // param_4: cost string
                    (segmentCount == 0) ? 1 : 0,           // param_5: isEmpty
                    (pathType[0] == 2) ? 1 : 0,            // param_6: isInvalid
                    (pathType[0] == 1) ? 1 : 0,            // param_7: isSingle
                    extraParam,                             // param_8
                    startCell,                        // param_9
                    endCell,                          // param_10
                    box,                                    // param_11
                    &transform                              // param_12
                );
            }
        }

        // ---- Handle mouse release / commit ----
        if (singleClick != 0) {
            // Single-click mode: waiting for second click to confirm placement.
            // Original uses fn_KeyMap_Pressed (dispatch 0x140151758).
            bool pressed = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
            if (pressed) {
                if (currentCell < 0) {
                    // Invalid target cell
                    DragTool_PlayErrorSound_100140(0);
                } else {
                    // Virtual CalcPath (slot 13, +0x68): validate + commit
                    bool valid = CalcPath(startCell, endCell);

                    if (valid) {
                        DragTool_PlayPlaceSound_1000f0(0);
                    } else {
                        DragTool_PlayErrorSound_100140(0);
                    }
                }

                // Reset both dragState and singleClick, close dialog
                // Original: MOV word ptr [RDI + 0x28], 0x0
                dragState = 0;
                singleClick = 0;
                DragTool_CloseToolDialog_ff980(DIALOG_IBP());
            }
        } else {
            // Normal drag mode: check if mouse button was just released.
            // Original checks: Released(0,5) || GetValue(0,5)==0
            // ENDGAME B.3.7: Released used to be always false (see the key-up
            // branch above); it now reports the release itself.
            bool released = reinterpret_cast<const Input::KeyMap*>(keyMap)->Released(0, 5);  // B.3.7 bridge
            if (!released) {
                short value = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 5);  // B.3.7 bridge
                released = (value == 0);
            }

            if (released) {
                if (startCell == endCell) {
                    // User clicked without dragging — enter single-click mode
                    // (will commit on next click)
                    singleClick = 1;
                } else if (currentCell < 0) {
                    // Dragged to invalid cell — play error but do NOT close dialog
                    // (user can keep dragging to a valid cell)
                    DragTool_PlayErrorSound_100140(0);
                } else {
                    // Valid drag release — virtual CalcPath (slot 13): validate and commit
                    bool valid = CalcPath(startCell, endCell);

                    if (valid) {
                        DragTool_PlayPlaceSound_1000f0(0);
                    } else {
                        DragTool_PlayErrorSound_100140(0);
                    }

                    // Close dialog and reset dragState
                    // Original: MOV byte ptr [RDI + 0x28], 0x0 (only dragState, keeps singleClick)
                    DragTool_CloseToolDialog_ff980(DIALOG_IBP());
                    dragState = 0;
                }
            }
        }

    }

    return 1; // Always returns 1 (event consumed)
}

// ============================================================
// DragTool cancel / "can-deactivate?" — vtable +0x48, Slot9
// ============================================================
// Original: 0x1400fc640, shared across the DragTool family (Ghidra mislabels it
// BridgeTool_Update). EntityUI's right-click path asks this before tearing the tool
// down: mid-drag it cancels the drag (clears dragState+singleClick, closes the tool
// dialog) and returns 1 so the tool STAYS; idle it returns 0 and teardown proceeds.
// The Tool_Ret0 stub in that slot meant a right-click mid-drag tore the tool down
// with the cost dialog still open — the "icon left on screen" dismiss bug.
char DragTool::OnCancel() {
    if (dragState == 0) return 0;
    dragState = 0;
    singleClick = 0;   // retail: MOV word ptr [self+0x28], 0
    uint64_t* ibp = (uint64_t*)dialogPtr;
    if (ibp && *ibp) {
        uint64_t tempIBP = *ibp;
        reinterpret_cast<UI::UserInterface*>(GetUserInterface())->CloseDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&tempIBP));
        new (&tempIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *ibp = tempIBP;
    }
    return 1;
}
