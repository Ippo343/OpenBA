// code/Game/Player/tool_base_drag.cpp
// Player drag-tool shared exe sub-helpers (DragTool / DragPlacementTool).
//
// These are exe sub-helpers resolved via ResolveAddr(Module::Exe, 0xADDR) from
// code/Game/Player/dragtool.cpp and code/Game/Player/dragplacementtool.cpp. They
// resolve to 0 (the guarded call is silently skipped) unless registered. This file
// provides faithful 1:1 reimplementations; standalone/reg_player_drag.cpp registers
// the closure-safe subset (RegisterPlayerDragBatch).
//
// Player registration wave (P1 — drag). Each function is a 1:1 port of the decompiled
// exe helper; do not "improve" behavior. Ghidra-verified against the disassembly.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// FindDescription (FUN_140005160) — closure-safe
// ============================================================
// Scans a description component list for a description matching type-ID
// DWORD_1401c7f58, writing the first match straight into *param_2 (a plain
// undefined8* slot — NO InternalBasePtr_Ctor, NO CheckPtr). Container layout:
// ptr-array base at param_1+0x28, count at param_1+0x30. Mirrors the
// WorkAcc_FindComponent_6330 shape (same straight-slot write form).
//
// Caller (dragtool.cpp DragTool::OnKey): g_fn_FindDescription(descPtr, &descResult)
//   typedef void* (*fn_FindDescription_t)(uint64_t descPtr, void* retval);
//
// Closure: fn_IsDerivedFrom (engine_funcs export), type-ID global 0x1401c7f58
// (resolved lazily via ResolveAddr). No exe sub-calls.
static uint32_t s_tid_c7f58 = 0;   // DWORD_1401c7f58 = "ToolbarDescription" type-id (value)
static bool s_tidC7f58Init = false;

void* DragTool_FindDescription_5160(uintptr_t descList, void* retval) {
    if (!s_tidC7f58Init) {
        // The raw .rdata global resolves to null in standalone → the scan below would
        // no-op (the "no cost in CreateBar" symptom), so fall back to the name-keyed
        // lookup (the proven Startup/createbar pattern).
        const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7f58));
        if (p) s_tid_c7f58 = *p;
        if (!s_tid_c7f58) s_tid_c7f58 = ::Object::GetTypeIdByName("ToolbarDescription");
        s_tidC7f58Init = true;
    }
    void* found = nullptr;
    if (s_tid_c7f58) {
        int n = *reinterpret_cast<int*>(descList + 0x30);
        uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(descList + 0x28);
        for (int i = 0; i < n; i++) {
            void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
            // GetTypeId is component vtable slot +0x10.
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == s_tid_c7f58) {
                void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid_c7f58)) ? c : nullptr;
                break;
            }
        }
    }
    *reinterpret_cast<void**>(retval) = found;
    return retval;
}

// ============================================================
// CloseToolDialog (BridgeTool_Draw_sub_f980, 0x1400ff980) — closure-safe
// ============================================================
// Closes the placement dialog if the IBP has a live pointer, then nulls it.
// Original:
//   local = *param_1;
//   if (local != 0) {
//     UI::UserInterface::CloseDialog(*(UserInterface**)(gameWorld + 0x98));
//     InternalBasePtr_Ctor(&local, 0);
//     *param_1 = local;
//   }
//
// Caller (dragtool.cpp): g_fn_CloseToolDialog(DIALOG_IBP())  — param_1 = ptr-to-IBP.
//
// Closure: fn_UI_CloseDialog, fn_InternalBasePtr_Ctor (engine_funcs exports);
// g_pGameWorldSingleton (singleton global). No exe sub-calls.
void DragTool_CloseToolDialog_ff980(uint64_t* param_1) {
    uint64_t local = *param_1;
    if (local != 0) {
        void* ui = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);
        if (fn_UI_CloseDialog) fn_UI_CloseDialog(ui, &local);
        if (fn_InternalBasePtr_Ctor) new (&local) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *param_1 = local;
    }
}

// ============================================================
// PlayTickSound (FUN_1400ffbf0) — closure-safe
// ============================================================
// Plays the "tick" audio when endCell changes during drag, throttled by a
// timer stored at param_1+0x48 (System::Time, 8 bytes). Sound ID at
// g_pKeyMapSingleton+0x80; cooldown DAT_1401720d4 = 0.05f.
// Original:
//   if (*(longlong*)(param_1 + 0x48) < 1) {
//     Time t; Time::Time(&t, 0.0);
//     Audio::Scene::PlayEffect(*(Scene**)(gameWorld + 0x78),
//                              *(undefined8*)(keyMap + 0x80), 1, &t);
//     *(undefined8*)(param_1 + 0x48) = *(undefined8*)Time::Time(&t, 0.05f);
//   }
//
// Caller (dragtool.cpp): g_fn_PlayTickSound((uint64_t)DIALOG_IBP())
//   — param_1 is the IBP value (object base on which +0x48 holds the timer).
//
// Closure: fn_Time_Ctor_Float, fn_Audio_Scene_PlayEffect (engine_funcs exports);
// g_pGameWorldSingleton, g_pKeyMapSingleton (singleton globals). No exe sub-calls.
void DragTool_PlayTickSound_ffbf0(uintptr_t param_1) {
    if (*reinterpret_cast<int64_t*>(param_1 + 0x48) < 1) {
        uint64_t timeBuf[4] = {0};  // System::Time (8 bytes used)
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeBuf, 0.0f);

        void* scene = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x78);
        uint64_t soundId = *reinterpret_cast<uint64_t*>(
            reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton) + 0x80);
        if (fn_Audio_Scene_PlayEffect) fn_Audio_Scene_PlayEffect(scene, soundId, 1, timeBuf);

        // Re-set the throttle timer to 0.05s (DAT_1401720d4 == 0.05f).
        uint64_t throttle[4] = {0};
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(throttle, 0.05f);
        *reinterpret_cast<uint64_t*>(param_1 + 0x48) = throttle[0];
    }
}

// ============================================================
// PlayPlaceSound (FUN_1401000f0) — closure-safe
// ============================================================
// Plays the "place success" sound. Sound ID at g_pKeyMapSingleton+0x88.
// Original:
//   Time t; Time::Time(&t, 0.0);
//   Audio::Scene::PlayEffect(*(Scene**)(gameWorld + 0x78),
//                            *(undefined8*)(keyMap + 0x88), 1, &t);
//
// Caller (dragtool.cpp): g_fn_PlayPlaceSound(0). param_1 is unused (only seeds
// the unused first slot of the Time stack buffer in the original).
//
// Closure: fn_Time_Ctor_Float, fn_Audio_Scene_PlayEffect; g_pGameWorldSingleton,
// g_pKeyMapSingleton. No exe sub-calls.
void DragTool_PlayPlaceSound_1000f0(uint64_t /*unused*/) {
    uint64_t timeBuf[4] = {0};
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeBuf, 0.0f);
    void* scene = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x78);
    uint64_t soundId = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton) + 0x88);
    if (fn_Audio_Scene_PlayEffect) fn_Audio_Scene_PlayEffect(scene, soundId, 1, timeBuf);
}

// ============================================================
// PlayErrorSound (FUN_140100140) — closure-safe
// ============================================================
// Plays the "placement error" sound. Sound ID at g_pKeyMapSingleton+0x90.
// Identical to PlayPlaceSound except the sound-ID offset.
//
// Caller (dragtool.cpp): g_fn_PlayErrorSound(0).
//
// Closure: fn_Time_Ctor_Float, fn_Audio_Scene_PlayEffect; g_pGameWorldSingleton,
// g_pKeyMapSingleton. No exe sub-calls.
void DragTool_PlayErrorSound_100140(uint64_t /*unused*/) {
    uint64_t timeBuf[4] = {0};
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeBuf, 0.0f);
    void* scene = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x78);
    uint64_t soundId = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton) + 0x90);
    if (fn_Audio_Scene_PlayEffect) fn_Audio_Scene_PlayEffect(scene, soundId, 1, timeBuf);
}

// ============================================================
// ClearPathEntities (FUN_1401018d0) — closure-safe
// ============================================================
// Iterates the rectangular grid range [param2..param4] x [param3..param5]; for each
// cell whose stored entity (map grid cell +0x28) matches the entity-template param_6,
// marks the entity for removal (FUN_1400c2ea0) and pushes it onto the entity-list
// removal array at (entityList + 0xd0) via FUN_140003110 — unless its flag bit 10
// (0x400 @ entity+0x40) is already set.
//
// Caller (dragplacementtool.cpp DragPlacementTool::CalcPath):
//   s_fn_ClearPathEntities(self, minCol, minRow, maxCol, maxRow, entityTemplate)
//   typedef void (*fn_ClearPathEntities_t)(void* self, int minCol, int minRow,
//                                          int maxCol, int maxRow, int64_t entityTemplate);
//
// Original maps: param_2=minCol, param_3=minRow, param_4=maxCol, param_5=maxRow,
//   param_6=entityTemplate. Outer loop lVar4 over rows [param_3..param_5], inner
//   loop lVar3 over cols [param_2..param_4]. Cell stride 0x30, cell array at
//   (mapGridSizePtr + 0x20) [byte offset 0x100], gridSize at *mapGridSizePtr.
//
// Closure:
//   - FUN_1400c2ea0  (MarkEntityRemoval)   — REGISTERED (Action wave)
//   - FUN_140003110  (DynArraySetCount)    — REGISTERED (Action wave)
//   - fn_InternalBasePtr_CheckPtr           — engine_funcs export
//   - g_pGameWorldSingleton                 — singleton global
typedef void (*fn_MarkEntityRemoval_c2ea0_t)(uintptr_t entity);
typedef void (*fn_DynArraySetCount_003110_t)(int64_t* arr, int newCount, void* elem);
static fn_MarkEntityRemoval_c2ea0_t s_fn_MarkRemoval = nullptr;
static fn_DynArraySetCount_003110_t s_fn_ArrSetCount = nullptr;
static bool s_clearPathInit = false;

void DragPlacementTool_ClearPathEntities_1018d0(void* self, int minCol, int minRow,
                                                int maxCol, int maxRow, int64_t entityTemplate) {
    if (!s_clearPathInit) {
        s_fn_MarkRemoval = reinterpret_cast<fn_MarkEntityRemoval_c2ea0_t>(
            ResolveAddr(Module::Exe, 0x1400c2ea0));
        s_fn_ArrSetCount = reinterpret_cast<fn_DynArraySetCount_003110_t>(
            ResolveAddr(Module::Exe, 0x140003110));
        s_clearPathInit = true;
    }
    if (entityTemplate == 0) return;

    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    // piVar1 = *(int**)(gameWorld + 0xf8): map grid; *piVar1 = gridSize;
    int* mapGrid = *reinterpret_cast<int**>(gw + 0xf8);
    int gridSize = *mapGrid;
    // cell array base: *(longlong*)(piVar1 + 0x20)  (int* index 0x20 == byte +0x80)
    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);

    for (int64_t row = minRow; row <= maxRow; row++) {
        for (int64_t col = minCol; col <= maxCol; col++) {
            uint64_t cellEntity = *reinterpret_cast<uint64_t*>(
                cellArr + (static_cast<int64_t>(gridSize) * row + col) * 0x30);
            if (cellEntity != 0) {
                uint64_t local = cellEntity;
                // Cell stores the entity; match against the entity-template at +0x28.
                if (*reinterpret_cast<int64_t*>(local + 0x28) == entityTemplate) {
                    uint64_t resolved = local;
                    void* entityList = *reinterpret_cast<void**>(gw + 0xe8);
                    // skip if flag bit 10 (0x400) already set at entity+0x40
                    if ((*reinterpret_cast<uint32_t*>(resolved + 0x40) >> 10 & 1) == 0) {
                        if (s_fn_MarkRemoval) s_fn_MarkRemoval(resolved);
                        if (s_fn_ArrSetCount) {
                            uintptr_t el = reinterpret_cast<uintptr_t>(entityList);
                            s_fn_ArrSetCount(
                                reinterpret_cast<int64_t*>(el + 0xd0),
                                *reinterpret_cast<int*>(el + 0xd8) + 1,
                                &resolved);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// DEFERRED (bodies NOT provided here — registering would be silent corruption):
//
//  - 0x1400ff9e0 OpenToolDialog: sets the new DialogInstance's vtable pointer to
//    &g_ToolUI_ChildDialog_Vtable, which resolves to exe vtable 0x14015f5a0 via
//    ResolveAddr. That vtable addr is NOT registered in standalone → resolves to 0
//    → broken dialog vtable. DEFER until 0x14015f5a0 is registered.
//
//  - 0x1400ffc60 UpdateToolUI (operator_impl_fc60): deep exe sub-callee closure —
//    operator_impl_fc60_sub_e600, FUN_14005e700, operator_impl_e420, FUN_14005e360,
//    operator_helper_ebb0, CharacterComponent_impl_ec70, FUN_1400fe960 — ALL
//    unregistered. DEFER.
//
//  - 0x140101270 ScreenToCell: calls FUN_1401005a0 (unregistered) and
//    QuarryComponent_impl_a530 (unregistered → QuarryComponent_helper_aa20
//    unregistered). DEFER until that terrain-pick chain lands.
//
// LEVEL-1 caller bug (NOT in my batch scope — report only, do not register):
//  - 0x1400ff5d0 (cited as "TerrainValidate" in dragplacementtool.cpp) is
//    MID-FUNCTION (inside MainMenuDialog_helper_eec0, 0x1400feec0-0x1400ff6d3).
//    The TRUE callee in DragPlacementTool_Slot14 (0x1400fbae0) is
//    CharacterComponent_impl_f5d0 @ 0x14009f5d0 (dropped-page typo: ff5d0 -> 9f5d0).
//    Same 10-arg signature. Fix the ResolveAddr literal in dragplacementtool.cpp.
// ============================================================
