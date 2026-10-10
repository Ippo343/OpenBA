# B4 FORWARD forwarder inventory (read-only; heuristic classification)

Source: 478 marker lines in 176 files -> 306 forwarder functions (+6 marker mentions that are not forwarders).
Table: FORWARDERS.tsv. Caller sites counted over code runtime standalone dx11 engine tests, comments/strings stripped,
definition and declaration lines excluded; `*self:` = callers inside the defining file; `ptr` = name used without a call.

## Totals per flag (306 forwarders, 1005 outside-file call sites)
- ZERO: 74
- TYPED: 92
- UNTYPED: 140
- NOTFWD (marker mention only / local helper, not counted above): 6
Flag rule: ZERO = no call or pointer use anywhere; UNTYPED = any site passes a void*/integer/raw-offset value, a ptr use,
or a variable whose type is not the member class (e.g. node types of the generic AVL cores); else TYPED. A cast around a
typed variable (`reinterpret_cast<int64_t>(p)` with `T* p`) counts as TYPED (rewire = drop the cast). Variable types come from
the nearest declaration above the call (backward text search), so spot-check before a bulk rewire.

## Forwarders per defining module
Game/Action 117, Game/Component 89, Game/Dialog 24, Game/Player 23, Game/Game 21, Game/Work 15, Game/Entity 7, runtime/src/io 4, runtime/src/ui 3, Game/Tutorial 2, runtime/src/save 1

## Caller files with the most forwarder call sites (top 25; site count / distinct forwarders)
- src/game/Component/citizenai_actionctors.cpp: 57 / 11
- src/game/Work/work_base_actionctor.cpp: 41 / 13
- standalone/tool_harness.cpp: 25 / 14
- src/game/Work/work_act_perform.cpp: 24 / 11
- src/game/Component/citizencomponent.cpp: 23 / 13
- src/game/Component/destroycomponent_states.cpp: 19 / 12
- src/game/Component/workplacecomponent.cpp: 19 / 8
- src/game/Work/work_act_path.cpp: 18 / 10
- src/game/Work/consumeproduce.cpp: 18 / 9
- src/game/Work/work_act_rawmat.cpp: 17 / 8
- src/game/Component/pasturecomponent_states.cpp: 16 / 10
- src/game/Game/starttown.cpp: 16 / 11
- src/game/Component/growthcomponent.cpp: 14 / 8
- src/game/Work/build_actions.cpp: 14 / 7
- src/game/Work/work_act_hunt.cpp: 14 / 8
- src/game/Work/marketwork.cpp: 14 / 11
- src/game/Work/buildstructure.cpp: 14 / 8
- src/game/Work/work_act_consume.cpp: 13 / 11
- src/game/Component/buildcomponent_state8.cpp: 13 / 6
- src/game/Work/tradework.cpp: 13 / 8
- src/game/Game/gameupdateloop.cpp: 12 / 11
- src/game/Dialog/debugui.cpp: 12 / 11
- src/game/Component/citizenai_findhelpers.cpp: 12 / 8
- src/game/Component/orchardcomponent_states.cpp: 12 / 5
- src/game/Component/citizenai_gather.cpp: 11 / 6

## Odd
- Not forwarders: FireComponent::Extinguish, PlayerToolManager::CenterOnEntity, TradeUI::OnTradeButton, MenuDialogLoopT::CreateLoopDialog (comment mentions);
  Manager (tool_activation_standalone.cpp) and FindInto / ForwardFindComponent (file-local templates carrying the marker for the named finders below them).
- Marker on the helper, forwarders found by their bodies: the 5 WorkBase_FindComponent_* (work_base_reimpl.cpp) and 3 Entity_FindComponent_* (entity_finders_disaster.cpp).
- Three forwarders (EntityList_PostUpdate/RemoveEntity/Serialize) are marked only on their declaration in Entity/entitylist_funcs.h; the definitions in entity/entitylist.cpp carry no marker.
- Member-overload forwarders found by hand (names too common to grep): DialogInstance::Update (runtime/types/ui/DialogInstance.h:443), PerformWork::PerformWork(void*...) (Action/PerformWork.h:30).
- FULL-BODY rows (36): the marker sits on a receiver cast inside a real body kept with a void* signature (e.g. the resourceproduce_creatework CreateWork family,
  PlacementTool_Harness*, ToolbarUtility_BstInsert_4890, TradeUI_GetResourcePrice_a520, ResolveExternalByName_Impl); rewiring means typing the receiver, not deleting a one-liner.
- Forwarders with no declaration found (8): BoatComponent_SetDestination, BoatRoute_FindBoatComponent, ConsumeProduce_RecipeLabelAt, DialogCreateElement, Entity_FindComponent_AmbientEmitter_2280, ForwardFindComponent, MakeReaderWriter, RtIo_Persistence_GetInt
- Overloaded names (callers may bind the typed overload, so UNTYPED may be overstated): DialogInstance::Update, DragTool_FindDescription_5160, PerformWork::PerformWork, PlayerPath_CharComp_f5d0, Player_HighlightAdd_55430, RtSave_MetaData_CopyCtor
- ptr uses outside the defining file: ActHunt_DynArraySetCount_3110 src/game/Action/action_base_cnr.cpp(1) *self:src/game/Acti; ActHunt_MarkEntityRemoval_c2ea0 src/game/Component/absorbresourcecomponent.cpp(1) code/Game; AttendanceComponent_GetJobHostComponent *self:src/game/Component/attendancecomponent.cpp(1,ptr 1) c; EntityList_RegisterEntity_Impl src/game/Action/action_base_cnr.cpp(1) src/game/Action/act; FireComponent_IsBeingDemolished_11e690 src/game/Component/firecomponent_ignite.cpp(1) src/game/Co; ForwardFindComponent *self:src/game/Work/work_base_reimpl.cpp(5,ptr 5); RemovalCalc_StateMachine_dfa0 src/game/Dialog/upgradeui.cpp(2,ptr 1); TutWave_TutorialTreeInsert src/game/Tutorial/finishtutorialinstance.cpp(2,ptr 1); UpdateToolUI_fc60 standalone/tool_harness.cpp(2,ptr 1); WorkPlaceComponent_SetAccepting src/game/Component/attendancecomponent.cpp(1) src/game/Com
- Runtime-side forwarders present: RtIo_Persistence_Get/Set*, RtSave_MetaData_CopyCtor, DialogInstance::Update, MakeReaderWriter, DialogCreateElement.
- Many ZERO rows still sit in exe_bodies.h / *_funcs.h declarations: delete the declaration with the definition.
