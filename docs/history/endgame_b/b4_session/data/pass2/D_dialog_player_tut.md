# Region D_dialog_player_tut: blockers (1,128 casts, 109 files)

Attribution is by snippet and nearest LEFT comment, spot-checked against the headers; per-file splits sum to the file count (estimates).

## Blockers by casts: key | casts | files | header that changes | the change
- `LOCAL:TradeComponent` | 102 | 4 | src/game/Component/TradeComponent.h | header typed (TradeBuyOrder/Manifest/Node/MerchantOrder); TradeUI reads raw offs
- `LOCAL:CitizenBehavior` | 88 | 8 | src/game/Entity/CitizenBehavior.h | field_0x110/0x118 resource list typed; read at +0x118/+0x110
- `NOCLASS:HighlightTessSlot (0x60 stride at tool+0x1f0)` | 83 | 1 | src/game/Player/ (new header) | slot {vb{ptr,cnt,cap}+8, ib+0x18, rect +0x2c..0x38, yaw +0x40, mode +0x44, UV +0
- `NOCLASS:CitizenBehavior bucket tree node (+0x100/+0x160, 0x38 bytes)` | 37 | 5 | src/game/Entity/CitizenBehavior.h | field_0x100/0x160 are void* roots; node {key,list,count,..,+0x20/+0x28 children}
- `FIELD:HighlightComponent::fieldF8` | 36 | 1 | src/game/Component/HighlightComponent.h | void* +0xF8 (EdgeMesh*/mesh arrays) read at +0xc0/+0xc4/+0xd8
- `LOCAL:GameUpdateLoop` | 34 | 10 | src/game/Game/GameUpdateLoop.h | gw = (int64_t)_singleton; use typed members m_game/m_worldGeometry
- `SIG:Serialize-stream` | 33 | 30 | src/engine/object/External.h + each class Serialize  | Serialize(void* stream) slot
- `LOCAL:EventLog` | 30 | 3 | src/game/Game/EventLog.h | EventLog/EventLogEntry headers exist; raw offset reads (+0x18 events, +0x20 coun
- `LOCAL:Profession` | 28 | 6 | src/game/Game/Profession.h | stringName is already System::String; cast redundant
- `FIELD:ComponentDescription::_subDescs` | 27 | 9 | src/game/Component/ComponentDescription.h | Description** read as Object::Base*; add typed accessor / Description->Base rela
- `ERASED:Avl38 helpers (0x140014b40/0x14001fa80 retrace)` | 27 | 5 | src/game/Player/exe_bodies.h | shared AVL retrace takes int64_t*/Avl38Node**
- `LOCAL:HighlightComponent` | 22 | 1 | src/game/Component/HighlightComponent.h | Impl_* take int64_t self / void* self; type to HighlightComponent*/EditTool*
- `CONST:FindOrLoad` | 22 | 15 | src/engine/object FindOrLoad | const_cast of FindOrLoad StringTable
- `SIG:C-door:Component/exe_bodies.h` | 21 | 2 | src/game/Component/exe_bodies.h | C doors take the manager as int64_t
- `SIG:Dialog-ctor-params` | 21 | 7 | src/game/Dialog/*Dialog.h ctor decls + Dialog::Create | ctor/create take void* ui, parentIBP, extRes
- `SIG:C-door:Action/exe_bodies.h` | 20 | 6 | src/game/Action/exe_bodies.h | body takes int64_t receiver
- `LOCAL:ComponentEntity` | 19 | 3 | src/game/Entity/ComponentEntity.h | entity component walk (+0x30/+0x38) and WorkPlaceComponent +0x10/+0x50
- `H5:HighlightComponent::edgeMeshInstance` | 18 | 1 | src/game/Component/HighlightComponent.h | embedded EdgeMeshInstance (+0x28, 0xD0 bytes) raw uint8_t[]; type as member
- `LOCAL:WeatherSimulation` | 17 | 1 | src/game/Game/WeatherSimulation.h | m_weather typed; drawMgr/particle list read by index/offset
- `LOCAL:TradeDescription` | 17 | 1 | src/game/Component/TradeDescription.h | merchant record 0x48 stride (TradeMerchant typed) read at +0x20/+0x28
- `LOCAL:StorageComponent` | 16 | 2 | src/game/Component/StorageComponent.h | storageSlots is typed Item*; items read by offset (+8 amount,+0xC reserve)
- `LOCAL:TrackerComponent` | 16 | 1 | src/game/Component/TrackerComponent.h | tracker +0x28/+0x30/+0x40, item array 0x10 stride; verify header fields
- `LOCAL:Calendar` | 14 | 3 | src/game/Game/Calendar.h | m_dayCounter already Calendar*; +8 is yearTime (System::Time)
- `FIELD:EditTool::entityRef` | 14 | 2 | src/game/Player/EditTool.h | entityRef/lastEntityRef uint64_t; should be InternalPtr<ComponentEntity>
- `POD:RtIO::Mod` | 14 | 2 | src/engine/io (RtIO::Mod/ModArray) | Mod Strings as char*, ModArray::data void*
- `LOCAL:StorageLocationComponent` | 13 | 1 | src/game/Component/StorageLocationComponent.h | +0x10/+0x18/+0x30 reads; also WorkDescription +0x24/+0x30/+0x34 and BuildCompone
- `LOCAL:StateMachineComponent` | 12 | 1 | src/game/Component/StateMachineComponent.h | +0x50/+0x52 are StateMachine::currentInput/pendingState (declared)
- `OTHER:file-local Math thunks take void*` | 12 | 1 | src/game/Player/tool_base_highlight_tess.cpp | Transform2/Vector2 thunks declared void*; retype params
- `LOCAL:CitizenComponent` | 12 | 2 | src/game/Component/CitizenComponent.h | header declares age/profession/school/description; raw offset reads
- `FIELD:SpriteSheet::m_field30` | 12 | 6 | src/engine/ui/SpriteSheet.h | uint64_t ExternalPtr<MaterialInstance>
- `H5:MapGenerator::_field20` | 12 | 2 | src/game/Game/MapGenerator.h | _field20 void* ExternalPtr<StringTable>; _text char*
- `NOCLASS:Map tile AVL node (+0x30 grid)` | 12 | 2 | src/game/Game/Map.h | worldGrid +0x30 tree; node has no class
- `NOCLASS:TownHall row records (RatingGroup/ProfRow/ProdRow/InvEntry)` | 12 | 1 | src/game/Dialog/TownHallUI.h | row structs file-local; entries arrive as void*
- `SIG:PlayerToolManager-void*-receiver` | 10 | 2 | src/game/Dialog/toolbardialog.cpp + Player/exe_bodies. | dialog slots hand tool manager / group id as void*/int64_t

LOCAL total: 467 of 1128 casts. Smaller blockers (<=3 casts) are in the tsv.

## 10 largest LOCAL files (casts)
- Dialog/tradeui.cpp: 104 (CitizenBehavior, StateMachineComponent, StorageComponent, TradeComponent)
- Dialog/professionui.cpp: 67 (CitizenBehavior, ComponentEntity, Profession, StorageLocationComponent)
- Dialog/debugui.cpp: 35 (Calendar, CitizenBehavior, Disease, InteractComponent, WeatherSimulation)
- Player/tool_base_render.cpp: 30 (GameUpdateLoop, HighlightComponent)
- Dialog/townhallui.cpp: 29 (Calendar, CitizenBehavior, Profession, StorageComponent)
- Dialog/residenceui.cpp: 26 (CitizenComponent, Profession, ResidenceComponent)
- Dialog/eventnotifydialog.cpp: 22 (EventLog, GameUpdateLoop)
- Dialog/tradeui_buyentries.cpp: 20 (ComponentEntity, TradeDescription)
- Dialog/tradeui_leaves.cpp: 20 (TradeComponent)
- Dialog/productionui.cpp: 19 (Calendar, CitizenBehavior, TrackerComponent)

## Classes with NO header yet (NOCLASS keys)
- HighlightTessSlot (0x60 stride at tool+0x1f0): 83 casts (slot {vb{ptr,cnt,cap}+8, ib+0x18, rect +0x2c..0x38, yaw +0x40, mode +0x44, UV +0)
- CitizenBehavior bucket tree node (+0x100/+0x160, 0x38 bytes): 37 casts (field_0x100/0x160 are void* roots; node {key,list,count,..,+0x20/+0x28 children})
- Map tile AVL node (+0x30 grid): 12 casts (worldGrid +0x30 tree; node has no class)
- TownHall row records (RatingGroup/ProfRow/ProdRow/InvEntry): 12 casts (row structs file-local; entries arrive as void*)
- Tracker item array (16-byte entries): 10 casts ({ptr,count|cap} descriptor; entries 0x10)
- HighlightDecalEntry (field_0x44/0x48 raw dwords): 10 casts (retail stores raw 32-bit words as floats at +0x48..+0x58)
- PathSegment in AvlNode28 element: 3 casts (AvlNode28 element declared as 8 byte segment)
- Bridge path SegNode (0x18): 1 casts (node from MemAlloc(0x18,8))
- tooltip user data (two Strings): 1 casts ()

## What the brief / LEFT comments got wrong
- Most LEFT reasons in this region are STALE. GameUpdateLoop now declares m_citizenList (CitizenBehavior*), m_game (Map*),
  m_dayCounter (Calendar*), m_eventNotifier (EventLog*), m_weather (WeatherSimulation*), m_mapConfig/m_terrainConfig/
  m_climateConfig, m_audioScene, m_imageCapture, m_professionList. Calendar.h, EventLog.h, WeatherSimulation.h exist with offsets.
  The casts on those members are redundant (delete the cast) or raw offset reads of an already typed class.
- TradeComponent.h (COMP-f) is typed (TradeMerchantOrder/BuyOrder/ManifestLine/ResourceNode); its comment says it is PUBLIC
  "while unconverted readers (TradeUI) read them". TradeDescription::_pairs/_merchants are typed too. All 5 tradeui*.cpp are LOCAL.
- StateMachineComponent exposes StateMachine {currentInput +0x50, pendingState +0x52}: the "COMP-e" reads are LOCAL.
- Profession::stringName is already System::String; reinterpret_cast<const System::String*>(&x->stringName) is redundant.
- ComponentDescription::_subDescs is Description** (not void**); the remaining cast is Description* -> Object::Base*.
- CitizenComponent (+0x3C age, +0x1F0 profession, +0x208 school, +0x288 description) and CitizenBehavior (+0xA0/+0xA8 citizen
  array, +0x170 professionResources, +0x110/+0x118 resource list) are declared; field_0x100/0x160 (bucket trees) stay void*.
- LOCAL counts include casts where the operand is an int64_t/uintptr_t local derived from a typed pointer; fix by typing the local.
- tool_base_highlight_tess.cpp (95) is one Ghidra-lifted function over a 0x60-stride slot array at param_1+0x1f0; it needs a slot
  struct (NOCLASS) rather than a header edit elsewhere.
