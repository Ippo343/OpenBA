# B_component inventory (1,456 casts, 196 files)

Method: every cast line was read from source; classification by operand name + file context + header check. Per-file splits are estimates (variable-name rules, spot-checked), sums are exact (1,456).

## Blockers by casts (key | casts | files | header that changes | change)
- `SIG:Act/Pf/Citizen helper calls (int64_t/uintptr_t params)` | 189 | 61 | Component/exe_bodies.h | function takes pointer as integer/int64_t*
- `SIG:Serialize-stream` | 170 | 147 | Component/Component.h | Component::Persist(void* stream) -> IO::Stream& (and every override)
- `LOCAL:Map` | 151 | 34 | Game/Map.h | none: GameUpdateLoop::m_game is Map*, Map::cells is MapCell*; replace offset reads (+0x80, 0x30 stride, +0x08 
- `SIG:TableRegisterState/TableAddEvent` | 143 | 24 | Component/controller_state_table.h | declare TableRegisterState/AddEvent with typed fn-pointer params (state handlers) instead of void*
- `ERASED:Array<T>::SetSize hand-expanded instance` | 88 | 8 | Collection/Array template (runtime/types/collect | replace the hand-copied SetSize bodies (ambientemitter, storage, toolbar/ambientenv desc, charmanager, cemeter
- `FWD:B4-forwarders (82 receivers, one per class)` | 82 | 42 | Component/animalwave_serialize.cpp | B4 FORWARD forwarder: void*/int param cast to its class; retype the forwarder parameter
- `LOCAL:Component/ComponentEntity (raw +0x10/+0x18/+0x30/+0x38 reads)` | 77 | 31 | Component/Component.h; Entity/ComponentEntity.h | fields declared (Component::_entity/_description, ComponentEntity components array); mostly boatcomponent_rout
- `LOCAL:CitizenBehavior` | 50 | 23 | Entity/CitizenBehavior.h | none: m_citizenList is CitizenBehavior*; drop stale casts, read declared fields (+0x20/+0x28/+0x40/+0x60/+0xA0
- `LOCAL:EntityList` | 39 | 12 | Entity/EntityList.h | none: m_entityList is EntityList*; use removalList / IBPArray at +0xD0 instead of entityList+0xD0/+0xD8
- `NOCLASS:StateMachine` | 35 | 8 | Component/StateMachineComponent.h | new class for the StateMachineComponent +0x48 machine (+0 controller,+9 cur,+0xA/+0xB,+0x50..0x52 bytes)
- `NOCLASS:PathGraph pair cache (m_pairs / m_bstRoot node: 0x28 BST nodes, int pair` | 33 | 1 | Entity/CitizenBehavior.h | type PathGraph::m_pairs (int32 pairs) and m_bstRoot node struct (citizenbehavior.cpp only)
- `NOCLASS:CitizenBehavior profession/zone BST node (0x38 bytes, key at [0], L/R at` | 31 | 4 | Entity/CitizenBehavior.h | node struct for CitizenBehavior::field_0x100 / field_0x160 (void*); used by zone, workplace, immigration, citi
- `LOCAL:StorageComponent` | 25 | 5 | Component/StorageComponent.h | none: storageSlots is Item*; read Item fields (+8 count, +0xC reserved)
- `H5:CitizenBehavior::field_0x150 (statistics history, 8 int Arrays raw)` | 23 | 1 | Entity/CitizenBehavior.h | make field_0x150 a struct of 8 Array<int> (GameColl::ArrayAt<I32NoInit> casts)
- `NOCLASS:fire/ambient audio emitter record (+0x28 pos, +0x38 desc, +0x40 count)` | 21 | 3 | Component/FireComponent.h | struct for the audio emitter data block (residence/fire/ambient)
- `NOCLASS:EntityUI controller slots (+0x28 tool, tool+0x30 entity, entity+0x40 fla` | 21 | 1 | Dialog/EntityUI.h | EntityUI controller / tool classes (entitylist.cpp dialog code)
- `LOCAL:Calendar` | 18 | 6 | Game/Calendar.h | m_dayCounter already Calendar*; Time at +8 / year at +0 read raw
- `SIG:Component::Tick/Update(time)` | 17 | 17 | Component/Component.h | Component::Tick/Update take void* time
- `LOCAL:WeatherSimulation` | 13 | 6 | Game/WeatherSimulation.h | m_weather typed; +0x18 temperature read raw
- `NOCLASS:WorkScheduler goals-tree node (+0x08 value IBP, +0x18/+0x20 children)` | 13 | 1 | Game/WorkScheduler.h | goals tree node has no type
- `LOCAL:ModelDescription` | 12 | 4 | Component/ModelDescription.h | _meshGroups array count read raw
- `OTHER:InternalPtr member declared as plain T* / raw word; InternalBasePtr has no` | 11 | 9 | Object::InternalBasePtr (runtime/types/object) | Serialize of a bare pointer member through an IBP cast
- `NOCLASS:Action stack header (+0 items,+8 count,+0x10 cursor IBP,+0x18 idx)` | 11 | 1 | Action/ActionStack | stack header read by offset
- `LOCAL:EventLog` | 11 | 1 | Game/EventLog.h | EventLog/EventLogEntry declared; raw +0x18/+0x20 array and entry fields
- `NOCLASS:IO::Serializer/Stream internal words (+0x24 / +0x8C position walk)` | 10 | 1 | runtime/types/io/Serializer.h | diagnostic walk of serializer internals
- `FIELD:BoatComponent::waypointField (void*, Array<Vector3> header)` | 9 | 2 | Component/BoatComponent.h | type the waypoint array member
- `LOCAL:WorkScheduler` | 8 | 5 | Game/WorkScheduler.h | m_tutorial already WorkScheduler*; cast stale
- `NOCLASS:BoatDescription route-table entry (name String, +0x08 Vector3, +0x14 yaw` | 7 | 1 | Component/BoatDescription.h | table at desc+0x20 (+0x30 data,+0x38 count) untyped
- `ERASED:ArrayAt (coll_bridge.h B.3.2 bridge)` | 7 | 1 | Component/coll_bridge.h | raw Array header reinterpreted
- `POD:RtObject::Internal` | 6 | 6 | runtime/types/object/ | bridge from raw Internal word to the real class (B.3.3)
- `FLOOR:vslot` | 6 | 4 | n/a | vtable slot call through void***
- `LOCAL:NaturalResourceComponent` | 6 | 3 | Component/NaturalResourceComponent.h | byte at +0x2D written raw
- `NOCLASS:DragTool description record (+0x30 table, +0x40 name String)` | 6 | 1 | Dialog/ or Player/DragTool | record from DragTool_FindDescription_5160 untyped
- `OTHER:UI dialog / IBP handles held as raw words` | 6 | 2 | Dialog/ | InternalPtr<DialogInstance> kept as uint64_t
- `NOCLASS:StartCondition (starttown hands void*; +0xEC offX/+0xF0/+0xF4 radius/+0x` | 6 | 1 | Game/starttown.cpp | start-condition record has no class
- `NOCLASS:BST node (int key, +0x18 left, +0x20 right)` | 5 | 1 | Action/PathSearch.h | node type unknown
- `LOCAL:WorkPlaceComponent` | 5 | 1 | Component/WorkPlaceComponent.h | roster at +0x60 / work-slot +0x30 raw reads
- `LOCAL:Graphics::Scene` | 5 | 3 | Game/GameUpdateLoop.h | m_graphicsScene already Graphics::Scene*; cast stale
- `FIELD:ComponentEntity::_externals (void*, holds a System::Time)` | 5 | 2 | Entity/ComponentEntity.h | type _externals as System::Time / int64 ticks
- `FIELD:WorldGeometry::_arrayD8 (void*, int16_t heights)` | 4 | 4 | runtime/types/ WorldGeometry | type as int16_t*

## 10 largest LOCAL files (casts that need no outside change)
- Component/diseasecomponent.cpp: 30 (of 47) LOCAL:Calendar, LOCAL:CitizenBehavior, LOCAL:EntityList, LOCAL:EventLog, LOCAL:Map, LOCAL:NaturalResourceComponent
- Entity/citizenbehavior.cpp: 26 (of 92) LOCAL:CitizenBehavior, LOCAL:CitizenDescription, LOCAL:Map, LOCAL:StorageComponent
- Component/boatcomponent_route.cpp: 18 (of 45) LOCAL:Component/ComponentEntity, LOCAL:Map, LOCAL:ModelComponent
- Component/buildcomponent_state8.cpp: 18 (of 22) LOCAL:BuildComponent, LOCAL:CitizenBehavior, LOCAL:Component/ComponentEntity, LOCAL:ModelDescription, LOCAL:StorageComponent
- Component/cemeterycomponent.cpp: 17 (of 31) LOCAL:CitizenBehavior, LOCAL:Component/ComponentEntity, LOCAL:Map
- Component/destroycomponent_states.cpp: 16 (of 38) LOCAL:EntityList, LOCAL:Map, LOCAL:ModelDescription, LOCAL:PlayerToolManager, LOCAL:StorageComponent, LOCAL:WorkScheduler
- Component/growthcomponent.cpp: 15 (of 25) LOCAL:Component/ComponentEntity, LOCAL:Map, LOCAL:NaturalResourceComponent, LOCAL:WeatherSimulation
- Component/destroycomponent_state6.cpp: 14 (of 28) LOCAL:CitizenBehavior, LOCAL:CitizenComponent, LOCAL:Component/ComponentEntity, LOCAL:Graphics::GraphicsMesh, LOCAL:ModelDescription
- Component/orchardcomponent.cpp: 14 (of 23) LOCAL:Calendar, LOCAL:Component/ComponentEntity, LOCAL:Map
- Component/immigrationcomponent_states.cpp: 13 (of 34) LOCAL:Calendar, LOCAL:CitizenBehavior, LOCAL:Component/ComponentEntity, LOCAL:Map, LOCAL:ResidenceComponent
- LOCAL total 443 casts in 74 files

## Classes with no header / no class yet
- StateMachine (StateMachineComponent +0x48 machine; 35 casts) - the largest missing class
- CitizenBehavior BST node 0x38 bytes (field_0x100 / field_0x160 are void*) and PathGraph pair-cache node (m_pairs, m_bstRoot void*)
- Audio emitter data block (+0x28 pos, +0x38 desc, +0x40 count); fire speed record; BoatDescription route-table entry; Disease emitter geometry array
- WorkScheduler goals-tree node; EntityUI controller/tool slots; StartCondition (starttown void*); Action stack header; DragTool description record
- ComponentEntity::_externals is void* holding a System::Time; _handle byte [3] (category) not a field

## What the brief/LEFT comments got wrong (stale blockers)
- Most `LEFT: GameUpdateLoop::m_game/m_citizenList/m_entityList/m_weather/m_dayCounter/m_tutorial is void* (W0 header)` are STALE: GameUpdateLoop.h now types them (Map*, CitizenBehavior*, EntityList*, WeatherSimulation*, Calendar*, WorkScheduler*, Graphics::Scene*, Audio::Scene*, PlayerToolManager*, Minimap*). ~190 casts are removable with no outside change.
- `Map::cells` is MapCell* with a full field list (blocker, occupant +8, regionId, zone, footprintCount, terrainType +0x29, floorCount +0x2D); LEFT "Map::cells untyped (GAME-b)" is stale. Map::pickingScene stays H5 (uint8_t[0x60]).
- StorageComponent::storageSlots (Item*), ResidenceComponent::residentSlots (Family*), ModelDescription::_meshGroups (ModelMeshGroup*), ComponentDescription::_subDescs (Description**), Component::_description (Description*) are typed; LEFTs naming them void* are stale (citizenbehavior.cpp:1096 is a plain downcast).
- Minimap.h and EventLog.h/EventLogEntry exist; "Minimap has no class yet" is stale.
- controller_state_table.h, coll_bridge.h, exe_bodies.h count toward the region (12 + 8 + 11 casts) but are bridge/forwarder infrastructure, not call sites.
- Entity/citizenbehavior.cpp (92) is in this region though the brief excludes citizen* Component files.
- Caveat: LOCAL vs NOCLASS for variable-name-matched lines (e.g. `world`, `map`, `desc`) is an estimate; ~10% may belong to the neighbouring key.
