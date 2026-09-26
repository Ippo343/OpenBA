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
#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)

// ============================================================
// DragTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_DragTool = nullptr;
static void* s_vtable_Tool     = nullptr;

static void EnsureDragToolVtables() {
    if (!s_vtable_DragTool) {
        s_vtable_DragTool = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool     = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// Object::Internal::~Internal — called from base dtor
// Resolved via the function pointer slot at 0x140152770
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// DragTool::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: DragTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002da20
void* DragTool_Dtor(DragTool* self, uint32_t flags) {
    EnsureDragToolVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_DragTool;
    self->vtable = s_vtable_Tool;

    // Call Object::Internal::~Internal
    if (s_fn_Internal_Dtor) {
        s_fn_Internal_Dtor(self);
    }

    // Conditionally free memory
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Resolved exe-internal function pointers (set once on first call)
// ============================================================

// FUN_140101270: ScreenToCell — ray-picks map cell under cursor.
// Returns cell index (row*gridSize + col) or -1 if off-map.
// Original: 0x140101270
typedef int (*fn_ScreenToCell_t)(uint64_t unused);
static fn_ScreenToCell_t g_fn_ScreenToCell = nullptr;

// FUN_1400ff9e0: OpenToolDialog — closes existing dialog, loads "Dialog/CreateBar.rsc",
// creates DialogInstance with ToolUI child vtable, adds to UI manager.
// Args: (dialogIBP_ptr, retval_ptr, descriptionPtr)
// Original: 0x1400ff9e0
typedef void* (*fn_OpenToolDialog_t)(void* dialogIBP, void* retval, uint64_t descriptionPtr);
static fn_OpenToolDialog_t g_fn_OpenToolDialog = nullptr;

// FUN_1400ffbf0: PlayTickSound — plays "tick" audio when endCell changes during drag.
// Checks a timer at +0x48 on the IBP storage to avoid rapid-fire.
// Args: (dialogIBP_ptr) — pointer to the InternalBasePtr (same as self->dialogPtr)
// Original: 0x1400ffbf0
typedef void (*fn_PlayTickSound_t)(uint64_t dialogVal);
static fn_PlayTickSound_t g_fn_PlayTickSound = nullptr;

// FUN_140005160: FindDescription — searches description component list for a specific
// description type (DAT_1401c7f58). Returns found ptr in retval.
// Args: (descPtr, retval*) -> retval*
// Original: 0x140005160
typedef void* (*fn_FindDescription_t)(uint64_t descPtr, void* retval);
static fn_FindDescription_t g_fn_FindDescription = nullptr;

// operator_impl_fc60: UpdateToolUI — updates the placement dialog with path info:
// cost text, status text, icon color/tint, and screen position tracking.
// Original: 0x1400ffc60
typedef void (*fn_UpdateToolUI_t)(void* dialogIBP, uint32_t tileW, uint32_t tileH,
    uint64_t costStr, char isEmpty, char isInvalid, char isSingle,
    uint32_t extraParam, int startCell, int endCell, void* box, void* transform);
static fn_UpdateToolUI_t g_fn_UpdateToolUI = nullptr;

// BridgeTool_Draw_sub_f980: CloseToolDialog — closes dialog if open, nulls the IBP.
// Args: (dialogIBP_ptr) — pointer to the InternalBasePtr<DialogInstance> field
// Original: 0x1400ff980
typedef void (*fn_CloseToolDialog_t)(void* dialogIBPPtr);
static fn_CloseToolDialog_t g_fn_CloseToolDialog = nullptr;

// FUN_1401000f0: PlayPlaceSound — plays "place success" sound effect via Audio::Scene.
// Uses sound ID at g_pKeyMapSingleton + 0x88.
// Original: 0x1401000f0
typedef void (*fn_PlayPlaceSound_t)(uint64_t unused);
static fn_PlayPlaceSound_t g_fn_PlayPlaceSound = nullptr;

// FUN_140100140: PlayErrorSound — plays "placement error" sound effect via Audio::Scene.
// Uses sound ID at g_pKeyMapSingleton + 0x90.
// Original: 0x140100140
typedef void (*fn_PlayErrorSound_t)(uint64_t unused);
static fn_PlayErrorSound_t g_fn_PlayErrorSound = nullptr;

// Volume::Box::Box() — default ctor
// Dispatch table at 0x1401513f8
typedef void* (*fn_Box_Ctor_t)(void* self);
static fn_Box_Ctor_t g_fn_BoxCtor = nullptr;

// SA no-op fallbacks for cosmetic drag helpers (sounds / dialog open+close). Their exe
// addresses are frequently unregistered in the standalone exe — RemovalTool is the first
// tool to actually exercise DragTool_OnKey — so a null call would fault. Pointing any null
// one at a no-op keeps the drag lifecycle robust; the essential BuildPath / Validate
// (CalcPath) path is unaffected.
static void  DragNoop_Sound(uint64_t) {}
static void  DragNoop_CloseDialog(void*) {}
static void* DragNoop_OpenDialog(void*, void*, uint64_t) { return nullptr; }

static bool g_funcsResolved = false;

static void ResolveDragToolFuncs() {
    if (g_funcsResolved) return;

    g_fn_ScreenToCell = reinterpret_cast<fn_ScreenToCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));
    g_fn_OpenToolDialog = reinterpret_cast<fn_OpenToolDialog_t>(
        ResolveAddr(Module::Exe, 0x1400ff9e0));
    g_fn_PlayTickSound = reinterpret_cast<fn_PlayTickSound_t>(
        ResolveAddr(Module::Exe, 0x1400ffbf0));
    g_fn_FindDescription = reinterpret_cast<fn_FindDescription_t>(
        ResolveAddr(Module::Exe, 0x140005160));
    g_fn_UpdateToolUI = reinterpret_cast<fn_UpdateToolUI_t>(
        ResolveAddr(Module::Exe, 0x1400ffc60));
    g_fn_CloseToolDialog = reinterpret_cast<fn_CloseToolDialog_t>(
        ResolveAddr(Module::Exe, 0x1400ff980));
    g_fn_PlayPlaceSound = reinterpret_cast<fn_PlayPlaceSound_t>(
        ResolveAddr(Module::Exe, 0x1401000f0));
    g_fn_PlayErrorSound = reinterpret_cast<fn_PlayErrorSound_t>(
        ResolveAddr(Module::Exe, 0x140100140));

    // Dispatch-table resolved ctors/dtors
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401513f8));
        if (d) g_fn_BoxCtor = reinterpret_cast<fn_Box_Ctor_t>(*d);
    }

    // Null cosmetic helpers -> no-ops so the drag lifecycle never faults on them in SA.
    if (!g_fn_PlayTickSound)   g_fn_PlayTickSound   = &DragNoop_Sound;
    if (!g_fn_PlayPlaceSound)  g_fn_PlayPlaceSound  = &DragNoop_Sound;
    if (!g_fn_PlayErrorSound)  g_fn_PlayErrorSound  = &DragNoop_Sound;
    if (!g_fn_CloseToolDialog) g_fn_CloseToolDialog = &DragNoop_CloseDialog;
    if (!g_fn_OpenToolDialog)  g_fn_OpenToolDialog  = &DragNoop_OpenDialog;

    g_funcsResolved = true;
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
// g_pKeyMapObjectExref needs double deref: slot -> ptr -> KeyMap
// (Ghidra shows: MOV RAX,[0x140151738]; MOV R8,[RAX]; MOV R14,[R8])
static inline void* GetKeyMap() {
    // Triple deref: slot → External ptr → wrapped ptr → KeyMap object
    // Original asm: MOV RAX,[slot]; MOV R8,[RAX]; MOV R14,[R8]
    return ***(void****)g_pKeyMapObjectExref;
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
uint64_t DragTool_OnKey(DragTool* self, char key) {
    ResolveDragToolFuncs();

    void* keyMap = GetKeyMap();

    // IMPORTANT: The original code re-reads [self+0x10] at each use site, NOT cached.
    // CalcPath/entity creation can trigger memory operations that invalidate cached pointers.
    // Use DIALOG_IBP() macro to always re-read fresh from the struct.
#define DIALOG_IBP() ((uint64_t*)self->dialogPtr)
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
        if (fn_KeyMap_Released && fn_KeyMap_Released(keyMap, 0, 5)) {
            // Reset both dragState and singleClick (original does MOV word [self+0x28], 0)
            self->dragState = 0;
            self->singleClick = 0;
        }
    }
    else if (self->dragState == 0) {
        // ---- Idle state: check for drag start ----
        // fn_KeyMap_Pressed at dispatch 0x140151758 — checks if button newly pressed
        bool pressed = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 5) : false;
        if (pressed) {
            int cell = g_fn_ScreenToCell((uint64_t)keyMap);
            self->startCell = cell;
            self->endCell = cell;

            if (cell >= 0) {
                // Begin dragging: set state, open placement dialog
                self->dragState = 1;
                self->singleClick = 0;
                uint64_t retval;
                // Original: FUN_1400ff9e0(*(longlong**)(self+0x10), &retval, *(self+0x20))
                // Passes self->dialogPtr (pointer to IBP), not &self->dialogPtr
                g_fn_OpenToolDialog(DIALOG_IBP(), &retval,
                    (uint64_t)self->descriptionPtr);
            }
        }
    }
    else {
        // ---- Dragging state: update path and handle release ----

        // Ensure dialog is open (re-open if it was somehow closed)
        // Original: CMP [dialogIBP], 0 — checks IBP value through the pointer
        if (*DIALOG_IBP() == 0) {
            uint64_t tempStr;
            g_fn_OpenToolDialog(DIALOG_IBP(), &tempStr,
                (uint64_t)self->descriptionPtr);
        }

        // Pick current cell under cursor
        int currentCell = g_fn_ScreenToCell((uint64_t)DIALOG_IBP());
        if (currentCell >= 0) {
            self->endCell = currentCell;
        }

        // If endCell changed, play tick sound and update lastEndCell
        if (self->lastEndCell != self->endCell) {
            self->lastEndCell = self->endCell;
            g_fn_PlayTickSound((uint64_t)DIALOG_IBP());
        }

        // ---- Call virtual BuildPath (vtable slot 14 = +0x70) ----
        // Signature: (self, startCell, endCell, &pathType, &tileH, &tileW, &extraParam) -> int
        // pathType: 0=normal, 1=single, 2=invalid
        typedef int (*fn_BuildPath_t)(void* self, int startCell, int endCell,
            int* pathType, uint32_t* tileH, uint32_t* tileW, uint64_t* extraParam);
        void** vtable = *(void***)self;
        fn_BuildPath_t buildPath = (fn_BuildPath_t)vtable[14]; // +0x70

        int pathType[2] = {0};
        uint32_t tileH[2] = {0};
        uint32_t tileW[2] = {0};
        uint64_t extraParam = 0;
        int segmentCount = buildPath(self, self->startCell, self->endCell,
            pathType, tileH, tileW, &extraParam);

        // ---- Cost tooltip (FindDescription -> cost string -> UpdateToolUI) ----
        // g_fn_UpdateToolUI (exe code 0x1400ffc60) may still resolve null in the standalone
        // exe if unregistered — in that case this whole COSMETIC block is skipped (the drag
        // + commit -> CalcPath still run and costString stays empty).
        System::String costString;
        if (g_fn_FindDescription && g_fn_UpdateToolUI) {
            uint64_t descResult = 0;
            g_fn_FindDescription((uint64_t)self->descriptionPtr, &descResult);

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
                if (g_fn_BoxCtor) g_fn_BoxCtor(box);

                // ---- Call UpdateToolUI (operator_impl_fc60 at 0x1400ffc60) ----
                g_fn_UpdateToolUI(
                    DIALOG_IBP(),                          // param_1
                    tileH[0],                              // param_2
                    tileW[0],                              // param_3
                    (uint64_t)&costString,                 // param_4: cost string
                    (segmentCount == 0) ? 1 : 0,           // param_5: isEmpty
                    (pathType[0] == 2) ? 1 : 0,            // param_6: isInvalid
                    (pathType[0] == 1) ? 1 : 0,            // param_7: isSingle
                    (uint32_t)extraParam,                   // param_8
                    self->startCell,                        // param_9
                    self->endCell,                          // param_10
                    box,                                    // param_11
                    &transform                              // param_12
                );
            }
        }

        // ---- Handle mouse release / commit ----
        if (self->singleClick != 0) {
            // Single-click mode: waiting for second click to confirm placement.
            // Original uses fn_KeyMap_Pressed (dispatch 0x140151758).
            bool pressed = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 5) : false;
            if (pressed) {
                if (currentCell < 0) {
                    // Invalid target cell
                    g_fn_PlayErrorSound(0);
                } else {
                    // Call virtual Validate (vtable slot 13 = +0x68)
                    typedef bool (*fn_Validate_t)(void* self, int startCell, int endCell);
                    fn_Validate_t validate = (fn_Validate_t)vtable[13];
                    bool valid = validate(self, self->startCell, self->endCell);

                    if (valid) {
                        g_fn_PlayPlaceSound(0);
                    } else {
                        g_fn_PlayErrorSound(0);
                    }
                }

                // Reset both dragState and singleClick, close dialog
                // Original: MOV word ptr [RDI + 0x28], 0x0
                self->dragState = 0;
                self->singleClick = 0;
                g_fn_CloseToolDialog(DIALOG_IBP());
            }
        } else {
            // Normal drag mode: check if mouse button was just released.
            // Original checks: Released(0,5) || GetValue(0,5)==0
            bool released = fn_KeyMap_Released ? fn_KeyMap_Released(keyMap, 0, 5) : false;
            if (!released) {
                short value = fn_KeyMap_GetValue ? fn_KeyMap_GetValue(keyMap, 0, 5) : 1;
                released = (value == 0);
            }

            if (released) {
                if (self->startCell == self->endCell) {
                    // User clicked without dragging — enter single-click mode
                    // (will commit on next click)
                    self->singleClick = 1;
                } else if (currentCell < 0) {
                    // Dragged to invalid cell — play error but do NOT close dialog
                    // (user can keep dragging to a valid cell)
                    g_fn_PlayErrorSound(0);
                } else {
                    // Valid drag release — validate and commit
                    typedef bool (*fn_Validate_t)(void* self, int startCell, int endCell);
                    fn_Validate_t validate = (fn_Validate_t)vtable[13];
                    bool valid = validate(self, self->startCell, self->endCell);

                    if (valid) {
                        g_fn_PlayPlaceSound(0);
                    } else {
                        g_fn_PlayErrorSound(0);
                    }

                    // Close dialog and reset dragState
                    // Original: MOV byte ptr [RDI + 0x28], 0x0 (only dragState, keeps singleClick)
                    g_fn_CloseToolDialog(DIALOG_IBP());
                    self->dragState = 0;
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
extern "C" int DragTool_CancelDrag_Impl(DragTool* self) {
    ResolveDragToolFuncs();
    if (self->dragState == 0) return 0;
    self->dragState = 0;
    self->singleClick = 0;   // retail: MOV word ptr [self+0x28], 0
    uint64_t* ibp = (uint64_t*)self->dialogPtr;
    if (ibp && *ibp && fn_InternalBasePtr_Ctor) {
        uint64_t tempIBP = *ibp;
        reinterpret_cast<UI::UserInterface*>(GetUserInterface())->CloseDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&tempIBP));
        new (&tempIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *ibp = tempIBP;
    }
    return 1;
}
