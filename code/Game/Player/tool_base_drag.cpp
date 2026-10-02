// code/Game/Player/tool_base_drag.cpp
// Player drag-tool shared exe sub-helpers (DragTool / DragPlacementTool).
//
// Each function is a 1:1 port of the decompiled exe helper; do not "improve"
// behavior. Ghidra-verified against the disassembly.
//
// B.4 PLAYER: the helpers read their receivers by name. The tool's owner (Tool
// +0x10) is the PlayerToolManager: its +0x00 is the open tool dialog, its +0x48
// the tick-sound throttle. The manager's header is block GAME-b's, so these stay
// free functions taking the typed manager (LEFT as members, listed).

#include <new>
#include "runtime/types/ui/DialogInstance.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"
#include "Component/ToolbarDescription.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include "Entity/EntityList.h"
#include "Game/Map.h"
#include "Game/PlayerToolManager.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include "Player/exe_bodies.h"

// ============================================================
// FindDescription (FUN_140005160) — closure-safe
// ============================================================
// Scans a ComponentDescription's sub-descriptions for the ToolbarDescription
// (type id cell 0x1401c7f58), writing the first match straight into *ret (a plain
// slot — NO InternalBasePtr_Ctor, NO CheckPtr). A type match that fails
// IsDerivedFrom ends the search with null.
ToolbarDescription** DragTool_FindDescription_5160(const ComponentDescription* description,
                                                   ToolbarDescription** ret) {
    const uint32_t tid = ToolbarDescription::TypeId();
    ToolbarDescription* found = nullptr;
    if (tid) {
        int n = description->_subDescCount;
        Description** subs = description->_subDescs;
        for (int i = 0; i < n; i++) {
            // Type() is the description's virtual slot 2 (+0x10).
            if (subs[i]->Type() == tid) {
                Description* c = subs[i];
                found = Object::IsDerivedFromNullSafe(c, tid) ? static_cast<ToolbarDescription*>(c)
                                                             : nullptr;
                break;
            }
        }
    }
    *ret = found;
    return ret;
}

// B4 FORWARD: 1 outside caller (Component/diseasecomponent.cpp), the old signature.
void* DragTool_FindDescription_5160(uintptr_t descList, void* retval) {
    return DragTool_FindDescription_5160(
        reinterpret_cast<const ComponentDescription*>(descList),   // B4 FORWARD: the caller's spelling
        reinterpret_cast<ToolbarDescription**>(retval));           // B4 FORWARD: the caller's spelling
}

// ============================================================
// CloseToolDialog (BridgeTool_Draw_sub_f980, 0x1400ff980) — closure-safe
// ============================================================
// Closes the tool dialog the manager holds (+0x00) if there is one, then nulls it.
// Original:
//   local = *param_1;
//   if (local != 0) {
//     UI::UserInterface::CloseDialog(*(UserInterface**)(gameWorld + 0x98), &local);
//     InternalBasePtr_Ctor(&local, 0);
//     *param_1 = local;
//   }
// param_1 is the tool's owner, the PlayerToolManager (Tool +0x10).
void DragTool_CloseToolDialog_ff980(PlayerToolManager* owner) {
    if (owner->field_0x00 != nullptr) {
        Object::InternalPtr<UI::DialogInstance> local(owner->field_0x00);
        GameUpdateLoop::_singleton->m_userInterface->CloseDialog(&local);
        owner->field_0x00 = nullptr;
    }
}

// ============================================================
// PlayTickSound (FUN_1400ffbf0) — closure-safe
// ============================================================
// Plays the "tick" audio when endCell changes during drag, throttled by the
// manager's Time at +0x48. Sound: GameContainer::placementMoveEffect (+0x80);
// cooldown DAT_1401720d4 = 0.05f.
// Original:
//   if (*(longlong*)(param_1 + 0x48) < 1) {
//     Time t; Time::Time(&t, 0.0);
//     Audio::Scene::PlayEffect(*(Scene**)(gameWorld + 0x78),
//                              *(undefined8*)(container + 0x80), 1, &t);
//     *(undefined8*)(param_1 + 0x48) = *(undefined8*)Time::Time(&t, 0.05f);
//   }
// param_1 is the tool's owner, the PlayerToolManager (Tool +0x10).
void DragTool_PlayTickSound_ffbf0(PlayerToolManager* owner) {
    if (owner->field_0x48 < 1) {
        System::Time t;
        RtSystem_Time_Ctor(&t, 0.0f);
        GameUpdateLoop::_singleton->m_audioScene->PlayEffect(
            g_gameContainer->placementMoveEffect, static_cast<Audio::Scene::SoundType>(1), t);

        // Re-set the throttle timer to 0.05s (DAT_1401720d4 == 0.05f).
        System::Time throttle;
        RtSystem_Time_Ctor(&throttle, 0.05f);
        owner->field_0x48 = throttle._ticks;
    }
}

// ============================================================
// PlayPlaceSound (FUN_1401000f0) — closure-safe
// ============================================================
// Plays the "place success" sound: GameContainer::placementThunkEffect (+0x88).
// Original:
//   Time t; Time::Time(&t, 0.0);
//   Audio::Scene::PlayEffect(*(Scene**)(gameWorld + 0x78),
//                            *(undefined8*)(container + 0x88), 1, &t);
// RCX is never read (the callers leave the tool or 0 in it).
void DragTool_PlayPlaceSound_1000f0(const void* /*unused*/) {
    System::Time t;
    RtSystem_Time_Ctor(&t, 0.0f);
    GameUpdateLoop::_singleton->m_audioScene->PlayEffect(
        g_gameContainer->placementThunkEffect, static_cast<Audio::Scene::SoundType>(1), t);
}

// ============================================================
// PlayErrorSound (FUN_140100140) — closure-safe
// ============================================================
// Plays the "placement error" sound: GameContainer::placementErrorEffect (+0x90).
// Identical to PlayPlaceSound except the effect. RCX is never read.
void DragTool_PlayErrorSound_100140(const void* /*unused*/) {
    System::Time t;
    RtSystem_Time_Ctor(&t, 0.0f);
    GameUpdateLoop::_singleton->m_audioScene->PlayEffect(
        g_gameContainer->placementErrorEffect, static_cast<Audio::Scene::SoundType>(1), t);
}

// ============================================================
// ClearPathEntities (FUN_1401018d0) — closure-safe
// ============================================================
// Iterates the rectangular grid range [param2..param4] x [param3..param5]; for each
// cell whose blocker entity's description (entity +0x28) is param_6, marks the
// entity for removal (0x1400c2ea0) and pushes it onto the entity list's removal
// array (+0xD0) via 0x140003110 — unless its flag bit 10 (0x400 @ entity+0x40) is
// already set. RCX (the tool) is never read.
//
// Original maps: param_2=minCol, param_3=minRow, param_4=maxCol, param_5=maxRow,
//   param_6=entityTemplate. Outer loop over rows [param_3..param_5], inner loop over
//   cols [param_2..param_4]. Cell stride 0x30 (MapCell), cell array Map +0x80.
void DragPlacementTool_ClearPathEntities_1018d0(const void* /*unused*/, int minCol, int minRow,
                                                int maxCol, int maxRow,
                                                const ComponentDescription* entityTemplate) {
    if (entityTemplate == nullptr) return;

    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    Map* map = gw->m_game;
    int gridSize = map->size;
    MapCell* cells = map->cells;

    for (int64_t row = minRow; row <= maxRow; row++) {
        for (int64_t col = minCol; col <= maxCol; col++) {
            ComponentEntity* cellEntity = cells[static_cast<int64_t>(gridSize) * row + col].blocker;
            if (cellEntity != nullptr) {
                ComponentEntity* local = cellEntity;
                if (local->_description == entityTemplate) {
                    ComponentEntity* resolved = local;
                    EntityList* entityList = gw->m_entityList;
                    // skip if flag bit 10 (0x400) already set at entity+0x40
                    if ((resolved->_entityFlags >> 10 & 1) == 0) {
                        MarkEntityForRemoval(resolved);
                        ActHunt_DynArraySetCount_3110(&entityList->removalList,
                                                      entityList->removalList.count + 1,
                                                      &resolved);
                    }
                }
            }
        }
    }
}

// ============================================================
// DEFERRED notes kept from the port:
//  - 0x1400ff5d0 (cited as "TerrainValidate" in dragplacementtool.cpp) is
//    MID-FUNCTION (inside 0x1400feec0-0x1400ff6d3). The TRUE callee in
//    DragPlacementTool_Slot14 (0x1400fbae0) is 0x14009f5d0.
// ============================================================
