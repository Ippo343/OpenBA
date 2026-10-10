# LEFT_FREE: declarations left as free functions, and `void*`-first parameters in code/Game

Regenerated at main `cbb701d58` by `b8_session/leftfree/regen.py` (B8, the quality pass: the generating script is KEPT; B9 re-read every survivor: a `B9:` tag on the line wins over the B8 one). Checked mechanically by `acceptance.sh` lines 08 and 09 (an identifier counts as listed when it appears anywhere in this file). History: first written at `239c7591d` (B4 close); not regenerated through S4 .. B7 (411 declared names at the B8 kit); the B8 pass moved every member of a class in retail into that class and deleted the forwarders and erased cores -- what is left here is a SURVIVOR with its ruling on its declaration line (`// B8: free in retail -- <RCX evidence>`, `// B8: stays free -- <measurement>`).

Totals: **A = 102 declared names** (103 declarations) in `exe_bodies.h` / `*_funcs.h`; **B = 34 functions** with a `void*` first parameter in code/Game.

## A. Declarations left in exe_bodies.h / *_funcs.h

One row per declaration: the name, where, the retail address on the line, and the ruling written on the line (a `B8: free in retail` ruling quotes what RCX is at a retail caller; a `B8: stays free` tag quotes the measurement -- a function-pointer site, a B11 DIFF, a port-only body with no retail symbol, a harness route). A row with no B8 tag carries its own comment.


### code/Game/Action/exe_bodies.h (11)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `BreakTimeCheck` | 53 | 0x1400a9290 | 0x1400a9290 // B8: free in retail -- RCX is the System::Time it tests (a Runtime class; 0x140049021's caller accumulates it), an exe-side helper |
| `ResourceCategory` | 63 | 0x140088f90 | 0x140088f90 // B8: stays free -- free in retail: 0x140088f90 reads the material from RDX (0x140088faa MOV RBX,RDX) and never reads RCX, which its callers leave unset (0x14010806c: the storage item array) |
| `ActRm_ArrayResize16_da70` | 80 | 0x14000da70 | 0x14000da70 // B9: stays free -- erased core over holders that are NOT one Collection::Array: ConsumeRawMaterial::EntryList / PickupStoredRawMaterial::EntryList / MarketWork::CarryList are POD {data, count, capacity} triples (H5), TrackerComponent::EntryList i |
| `ActRm_VectorClone_d9d0` | 81 | 0x14000d9d0 | 0x14000d9d0 // B9: stays free -- Array<T>::operator= (0x14000d9d0: CMP RCX,RDX; CALL [MemFree] on the old data; one qword zero of size+capacity; MemAlloc(n<<4, 8); two-qword copy) over the same holders as 0x14000da70: three POD triples and one Collection::Arra |
| `HeadingToward` | 96 | 0x1400c29a0 | 0x1400c29a0 // B8: free in retail -- RCX is the world-position pointer (0x1400baaf9 MOV RCX,[RBP+0x77]), a float vector |
| `PathPairs_Clear_25b30` | 105 | 0x140025b30 | 0x140025b30 // B9: stays free -- the holder is not a Collection::Array: retail 0x140025b30 is Array<PathCellPair>::SetSize(0) constant-folded, ONE argument (0x140025b3a MOVSXD RBP,[RCX+8] / TEST EBP,EBP / JZ; MOV [RCX+8],EDI with EDI 0; the dead grow arm MemAl |
| `Pf_RayTriIntersect_aa810` | 117 | 0x1400aa810 | 0x1400aa810 // B8: free in retail -- RCX dead: its four calls (0x1400aac26..0x1400aacd6) never load it |
| `Pf_TileShapeLut_ae180` | 118 | 0x1400ae180 | 0x1400ae180 // B8: free in retail -- RCX dead: at 0x1400ad355 the caller leaves a byte in CL (its stack argument 5) |
| `EntityAnchorCell` | 142 | 0x140087fb0 | 0x140087fb0 // B8: free in retail -- RCX is the address of an InternalPtr slot (0x140111988 LEA RCX,[R11+0x10]), not an object |
| `RemoveAnimalActions` | 149 | 0x1400bbea0 | 0x1400bbea0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1400c816b LEA RCX,[RBP-0x31]), not an object |
| `Pf_RegionPairCache_Build` | 171 | 0x14009e8c0 | no retail address (the tail of 0x14009e8c0; caller code/Game/Game/region_seed.cpp) // B9: harness / port-only -- read: no retail symbol; 0x14009e8c0 builds the 0x38-byte cache in place (MemAlloc(0x38, 4), the field init, the 0x20000-entry ring resize 0x14001cc |

### code/Game/Component/decalcomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `DecalComponent_Serialize` | 9 | 0x14011b7d0 | 0x14011b7d0 B8: stays free -- read, not moved: retail's slot [4] Startup body, never installed (DecalComponent.h TREE DIVERGENCE: the tree's Startup is a different body); no caller; installing it is behaviour: -- Phase D SLOTS: installed as DecalComponent::Startup |

### code/Game/Component/diseasecomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `DiseaseComp_GrownIdentity` | 40 | 0x140048375 | B8: free in retail -- RCX is the out slot (0x140048375 LEA RCX,[RSP+0x40]), RDX the entity slot const Object::InternalPtr<ComponentEntity>* entitySlot); // 0x140047f80 |

### code/Game/Component/exe_bodies.h (16)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `TownRegistry_RegisterResidence` | 57 | 0x140129de0 | the append inside ResidenceComponent's state-2 enter 0x140129de0 (townMgr+0x20) // B9: stays free -- read: it IS retail 0x140129de0's whole body (ResidenceComponent::EnterBuilt: [RCX+0x10] copied to a local, townMgr [[0x1401c3dd8]+0xf0], SetSizeFill(count + 1) |
| `CitizenEmploy_SeatWorkers` | 58 | 0x14007dd10 | no retail address: the headless driver of dd10's (0x14007dd10) inner loop // B9: harness / port-only -- read: its callers are standalone harnesses only (food_workers_harness.cpp, orchard_workers_harness.cpp, stress_harness.cpp); retail seats workers in 0x14007 |
| `CitizenAI_PerformWorkCtor4_c1070` | 75 | 0x1400c1070 | 0x1400c1070 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PerformWorkCtor5_bd070` | 76 | 0x1400bd070 | 0x1400bd070 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PerformWorkCtor6_bd420` | 77 | 0x1400bd420 | 0x1400bd420 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PerformWorkCtor8_bd790` | 78 | 0x1400bd790 | 0x1400bd790 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PerformWorkCtor10_bccb0` | 79 | 0x1400bccb0 | 0x1400bccb0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PerformWorkCtor11_bc790` | 80 | 0x1400bc790 | 0x1400bc790 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_Pathfinding_NavigateTo_bda50` | 81 | 0x1400bda50 | 0x1400bda50 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_Pathfinding_WalkTowards_bb490` | 82 | 0x1400bb490 | 0x1400bb490 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_PlayAnimationCtor4_ba6b0` | 83 | 0x1400ba6b0 | 0x1400ba6b0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400c1070: 0x1401162e3 LEA RCX,[RBP-0x39]), not an object |
| `CitizenAI_Reg_helper_e360` | 106 | 0x14000e360 | 0x14000e360 // B8: stays free -- already the template over the node type (explicit instantiations in citizenai_housing_idle.cpp); nothing erased |
| `CitizenComp_Entity_impl_ef40` | 115 | 0x14008ef40 | 0x14008ef40 (param_1 unused) // B8: free in retail -- RCX dead (0x14008df22: RDX / R8 / R9 loaded, RCX not) |
| `CitizenComp_SortByProfession_02a0` | 119 | 0x1400302a0 | 0x1400302a0 // B8: free in retail -- introsort helper: RCX the array base, EDX the count, R8 the compare (0x1401107c0 MOV RCX,R14) |
| `FrustumOutline_ProjectCorners` | 148 | 0x14006c5b0 | 0x14006c5b0 // B8: free in retail -- RCX is the output corner array (0x14006bf31 LEA RCX,[RBP-0x39]) |
| `Pasture_FillAllPastures` | 181 | 0x140060290 | 0x140060290 // B8: free in retail -- no parameters (0x140060e76, the debug menu, loads none) |

### code/Game/Component/mapcomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `MapComponent_Draw` | 17 | 0x140124370 | 0x140124370 B8: stays free -- read, not moved: retail's slot [6] OnAdded body, never installed (the tree's MapComponent::OnAdded is a different body, mapcomponent.cpp); no caller; installing it is behaviour: |

### code/Game/Component/pickingcomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `PickingComponent_RegisterMeshForEntity` | 11 | 0x1400c48a0 | B9: harness / port-only -- read: no retail body; at both callers retail's EntityList::RegisterEntity (0x1400c48a0) broadcast runs PickingComponent::OnAdded (= RefreshMesh 0x140128f50) itself, so the explicit call right after RegisterEntity (tool_base_tile.cpp, |

### code/Game/Component/rawmaterialcomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `RawMaterialComponent_Update` | 15 | 0x140129130 | 0x140129130 B8: stays free -- read, not moved: retail's slot [9] Update body, never installed (the tree's RawMaterialComponent::Update is the inert PORT GAP stub); no caller; installing it is behaviour: |

### code/Game/Component/spawncomponent_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `SpawnComponent_ReportDoSpawnCensus` | 9 |  | B8: harness / port-only -- diagnostic print of the DoSpawn tallies, no retail counterpart |

### code/Game/Component/tradecomponent_funcs.h (2)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `TradeMerchantOrders_Destroy` | 15 | 0x14000ef10 | 0x14000ef10 // B9: stays free -- measured: 0x14000ef10 is Array<TradeMerchantOrder>'s element destroy (RCX the array, unread: ~Array 0x14000e730 MOV RBX,RCX / CALL; RDX the first element, R8D the count), the inlined ~TradeMerchantOrder over its goods ~Array wi |
| `TradeResourceTree_NewNode` | 20 | 0x140059ede | B9: stays free -- no retail body: retail inlines `new Node(key, value, parent)` at every insert, placement-new null test included (0x140059ede MemAlloc(0x30, 8) / 0x140059ee7 TEST RAX,RAX, and 0x140059f53 / 0x140059f5c; the 0x14000ebe0 load insert); the spelli |

### code/Game/Dialog/debugui_funcs.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `DebugUI_Slot9` | 9 | 0x14005fd20 | B9: harness / port-only -- read: case 329 calls 0x14005fd20 (the tornado spawn) without loading RCX (0x140060ea5; only case 327 does MOV RCX,RBX), and no DebugUI exists on the disaster route: retail makes one only through its type factory 0x140010ec0 (Register |

### code/Game/Dialog/exe_bodies.h (3)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `Avl38SetInsert` | 78 | 0x140014890 | 0x140014890 // B8: stays free -- already typed (B.4 B13): a typed overload per node type over the one template core BstInsertCore<Node> (0x140014890) (toolbar_utility_helpers.cpp); nothing erased |
| `TownHall_InventorySortAsc` | 89 | 0x140015200 | 0x140015200 // B8: free in retail -- sort helper: RCX the item array base, EDX the count (0x14007ca2d MOV RCX,R12) |
| `TownHall_InventorySortDesc` | 90 | 0x140015500 | 0x140015500 // B8: free in retail -- sort helper: RCX the item array base, EDX the count (0x14007ca44, as Asc) |

### code/Game/Game/exe_bodies.h (6)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `NameDB_GenerateName` | 39 | 0x1400a4b30 | 0x1400a4b30 // B8: stays free -- free in retail: RCX is the String return slot (0x1400a4c25 MOV RCX,R12 before the String ctor), RDX the NameDatabase; an MSVC member's RCX is this (and the port's db null test has no this to sit on) |
| `NameDB_PickAndGenerate` | 40 | 0x1400a4c60 | 0x1400a4c60 // B8: stays free -- free in retail: no NameDatabase argument (RCX the String return slot, RDX the Random, R8B the flag; it loads Female / Male.ndb itself, 0x1400a4c86) |
| `EventLog_TileIndex_Reg` | 51 | 0x140095030 | 0x140095030 (RCX dead) // B8: free in retail -- RCX dead: at 0x1400484b0 it is what the import call [0x140151ee8] before it left |
| `Game_TriggerDisasterByType_Impl` | 58 | 0x140060110 | B8: free in retail -- first parameter the disaster type (an int), no object |
| `Reimpl_Array_Resize_Stride08` | 124 | 0x140001bb0 | 0x140001bb0 // B9: stays free -- erased core over holders that are not one Collection::Array: WorldGeometry::_batchMaterials (WorldGeometryArray), the +0x1E0 outlines and roadtool's +0x218 refs are RawArray-shaped headers, AmbientEmitterDescription::_emitters  |
| `TownRegistry_RegisterWorkplace` | 143 | 0x14012f520 | B9: harness / port-only -- read: no retail body; retail registers a workplace from WorkPlaceComponent's OnCreate (0x14012f520 -> CitizenBehavior::RegisterWorkplace 0x1400895a0, the +0x10 entity copied to a stack local); this is the standalone's spawn-time wiri |

### code/Game/Game/menudialogloopt_funcs.h (3)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `InitState` | 88 | 0x14009a930 | port-only: the chunked PhaseB of Load 0x14009a930 (Game/menudialogloopt.cpp) |
| `TickChunk` | 89 | 0x14009a930 | port-only: the chunked PhaseB of Load 0x14009a930 (Game/menudialogloopt.cpp) |
| `MenuDialogLoopT_TickAudio` | 150 |  | B8: free -- no retail symbol (the audio tick MenuDLT::Update inlines); first parameter the frame's System::Time, no object |

### code/Game/Player/edittool_funcs.h (7)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `EditTool_IsEditTool` | 12 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `EditTool_HarnessAcquire` | 13 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `EditTool_HarnessSelectEntity` | 14 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `EditTool_HarnessSelectCitizen` | 15 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `EditTool_HarnessSelectNearby` | 16 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `PlacementTool_Factory_SA` | 21 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |
| `RemovalTool_Factory_SA` | 23 |  | B8: harness / port-only -- standalone driver entry, no retail symbol |

### code/Game/Player/exe_bodies.h (17)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `UpdateToolUI_ebb0` | 45 | 0x1400febb0 | 0x1400febb0 // B8: free in retail -- RCX dead: 0x1400ffe72 loads RDX / R8 / R9 only |
| `DragTool_PlayPlaceSound_1000f0` | 48 | 0x1401000f0 | 0x1401000f0 // B8: free in retail -- RCX dead: at 0x1400fc91f it is what the virtual call [RAX+0x68] before it left |
| `DragTool_PlayErrorSound_100140` | 49 | 0x140100140 | 0x140100140 // B8: free in retail -- RCX dead: at 0x1400fc926 it is what the virtual call [RAX+0x68] before it left |
| `DragPlacementTool_ClearPathEntities_1018d0` | 50 | 0x1401018d0 | 0x1401018d0 // B8: free in retail -- RCX dead: 0x1400fb9e0 loads EDX / R8D / R9D only |
| `PlayerEdit_AvgTerrainHeight_101730` | 53 | 0x140101730 | 0x140101730 // B8: free in retail -- RCX dead: 0x1400dc25d does not load it |
| `DecalVertexSetInsert` | 82 | 0x14001f6b0 | 0x14001f6b0 // B8: stays free -- already typed on its one node type (AvlTreeHead<DecalVertexNode>); nothing erased |
| `DecalVertexSetFind` | 83 | 0x14001fc00 | 0x14001fc00 (0x1400ace40 passes the corner record itself) // B8: stays free -- already typed on its one node type (AvlTreeHead<DecalVertexNode>); nothing erased |
| `PlayerRay_GetMouseRay_1005a0` | 96 | 0x1401005a0 | 0x1401005a0 // B8: free in retail -- RCX dead: 0x1400fd0d9's caller does not load it (0x1401012a1 passes its own through) |
| `PlayerRay_ScreenToCell_101270` | 97 | 0x140101270 | 0x140101270 // B8: free in retail -- RCX dead: at 0x1400fc76c it is what the call [0x140151758] before it left |
| `PlayerScreenRect_BuildCorners_ec70` | 104 | 0x1400fec70 | 0x1400fec70 // B8: free in retail -- RCX dead: 0x1400ffe56 loads RDX / R8 / R9 only |
| `PlayerScreenRect_AccumulateRect_fe960` | 105 | 0x1400fe960 | 0x1400fe960 // B8: free in retail -- RCX dead: at 0x1400ffeeb it is what the call [0x1401528a8] before it left |
| `PlayerScreenRect_EntityRect_0380` | 106 | 0x140100380 | 0x140100380 // B8: free in retail -- RCX unread: 0x1400fceac's RCX is the entity value it stores into the RDX slot |
| `PlayerTileRender_DrawOverlay_08c0` | 112 | 0x1401008c0 | 0x1401008c0 // B8: free in retail -- RCX dead: 0x1400fc024 does not load it |
| `PlayerTileRender_BuildRecords_1380` | 113 | 0x140101380 | 0x140101380 // B8: free in retail -- RCX dead: 0x1400fdde3 does not load it |
| `Avl38SetRemove` | 121 | 0x140025740 | 0x140025740 // B8: stays free -- already typed (B.4 B13): a typed overload per node type over the one template core Avl38Remove<Node> (0x140025740) (tool_base_trade.cpp); nothing erased |
| `Avl38SetRemove` | 121 | 0x140025740 | 0x140025740 // B8: stays free -- already typed (B.4 B13): a typed overload per node type over the one template core Avl38Remove<Node> (0x140025740) (tool_base_trade.cpp); nothing erased |
| `Avl38SetFindRemove` | 123 | 0x14002c690 | 0x14002c690 // B8: stays free -- already typed (B.4 B13): a typed overload per node type over the one template core Avl38FindRemove<Node> (0x14002c690) (tool_base_trade.cpp); nothing erased |

### code/Game/Tutorial/exe_bodies.h (1)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `FindUIElementByPath_Impl` | 29 | 0x140152a90 | B8: stays free -- free in retail: RCX is the InternalPtr<UI::Element> return slot, constructed through the InternalBasePtr ctor (CALL [0x140152a90] at 0x1400e191c) const System::String* pathStr, UI::DialogInstance** outDialog); // 0x1400e1870 |

### code/Game/Work/exe_bodies.h (30)

| name | line | retail | ruling / comment on the line |
|---|---:|---|---|
| `BuildStructure_SetupBuildActions` | 23 | 0x1400c0690 | 0x1400c0690 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1400e571f LEA RCX,[RBP+0x7f]), not an object |
| `BuildStructure_ConstructForSite` | 29 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `BuildStructure_ForceDrive` | 30 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `BuildStructure_DepsReady` | 31 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `ForesterWork_ExecuteReady` | 34 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `ForesterWork_ForceDrive` | 35 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `GatherRawMaterials_ConstructForSite` | 38 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `GatherRawMaterials_ForceDrive` | 39 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `GatherRawMaterials_DepsReady` | 40 |  | no retail address (standalone harness entry) // B8: harness / port-only -- standalone driver entry, no retail symbol |
| `PastureWork_CreateForEntity` | 48 |  | no retail address (local extern in pasturecomponent_states.cpp) // B8: harness / port-only -- a constructor door with no retail symbol (pasturecomponent_states.cpp's work post) |
| `WorkAct_HarvestAreaCtor_c1530` | 57 | 0x1400c1530 | 0x1400c1530 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_ConsumeProduceCtor_bef70` | 60 | 0x1400bef70 | 0x1400bef70 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_HuntWorkCtor_bbbc0` | 64 | 0x1400bbbc0 | 0x1400bbbc0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_ShowToolCtor3_bdd20` | 66 | 0x1400bdd20 | 0x1400bdd20 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_GatherRawMatCtor_bf790` | 71 | 0x1400bf790 | 0x1400bf790 // B8: free in retail -- action factory (builds the Action and pushes it; first parameter its out slot), not an object |
| `WorkAct_PerformWorkCopyCtor_ba9f0` | 74 | 0x1400ba9f0 | 0x1400ba9f0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_PerformWorkCtor_bffd0` | 75 | 0x1400bffd0 | 0x1400bffd0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_GatherWorkCtor_bb7b0` | 78 | 0x1400bb7b0 | 0x1400bb7b0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_FishWorkCtor_beae0` | 79 | 0x1400beae0 | 0x1400beae0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_PerformWorkCtor_c1eb0` | 86 | 0x1400c1eb0 | 0x1400c1eb0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_PerformWorkCtor9_c0ad0` | 88 | 0x1400c0ad0 | 0x1400c0ad0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_PlayAnimationCtor_c2490` | 89 | 0x1400c2490 | 0x1400c2490 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_GrazingWorkCtor_bb0a0` | 90 | 0x1400bb0a0 | 0x1400bb0a0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkAct_PlantWorkCtor_be6e0` | 91 | 0x1400be6e0 | 0x1400be6e0 // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object |
| `WorkCellBST_Insert_9a40` | 100 | 0x140029a40 | 0x140029a40 // B9: stays free -- measured with the 8-byte family (5738a31c7: typed out slots SAME, the template done): the one body 0x140029a40 serves element types that name their key / value differently -- WorkCellAssignment {cell, citizen} (ClearArea / Plan |
| `WorkCellBST_Serialize_9de0` | 104 | 0x140029de0 | 0x140029de0 // B9: stays free -- its load path inserts through 0x140029a40 (stays free, above: the element types' key / value names differ), its save path reads WorkCellAssignment's {cell, citizen} for every set it is handed |
| `WorkImpl_Sqrtf_132010` | 130 | 0x140132010 | 0x140132010 // B8: free in retail -- the CRT sqrtf (0x140132010): the float in XMM0, no object |
| `WorkLeaf_BstRemoveByIBP_298a0` | 159 | 0x1400298a0 | 0x1400298a0 // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased |
| `WorkLeaf_BstInsertByIBP_29920` | 160 | 0x140029920 | 0x140029920 // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased |
| `WorkLeaf_BstSerializeIBPpair_29c10` | 161 | 0x140029c10 | 0x140029c10 // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased |

## B. code/Game functions with a `void*` first parameter

One row per name (every definition / declaration found). "in A" = declared in an exe_bodies.h / *_funcs.h header (section A). Those not in A are the harness / port-only bodies and the dead-RCX free functions B8 MISC ruled (their callers pass six different types; retail never reads RCX).

| name | file:line | signature | in A |
|---|---|---|---|
| `Avl38FreeSubtree` | Player/exe_bodies.h:243 | `void Avl38FreeSubtree(const void* context, Node* node)` | no |
| `CitUI_OkPtr` | Dialog/citizenui.cpp:78 | `static inline bool CitUI_OkPtr(const void* p)` | no |
| `CitizenComp_Entity_impl_ef40` | Component/citizencomponent.cpp:1780 +1 | `ComponentDescription** CitizenComp_Entity_impl_ef40(const void* param_1, ComponentDescription** param_2, ComponentEntity* const* param_3, in` | yes |
| `CreateInPlaceObject` | Object/exe_type.h:87 | `static ::Object::Base* CreateInPlaceObject(void* storage)` | no |
| `DragPlacementTool_ClearPathEntities_1018d0` | Player/exe_bodies.h:50 +1 | `void DragPlacementTool_ClearPathEntities_1018d0(const void* , int minCol, int minRow, int maxCol, int maxRow, const ComponentDescription* en` | yes |
| `DragTool_PlayErrorSound_100140` | Player/exe_bodies.h:49 +1 | `void DragTool_PlayErrorSound_100140(const void* )` | yes |
| `DragTool_PlayPlaceSound_1000f0` | Player/exe_bodies.h:48 +1 | `void DragTool_PlayPlaceSound_1000f0(const void* )` | yes |
| `Entity_ReassignCost_6c60` | Component/citizen_reassign_workers.cpp:133 | `extern " " uint64_t Entity_ReassignCost_6c60(const void* , ResidenceComponent* const* param_2, ResidenceComponent* const* param_3)` | no |
| `EventLog_TileIndex_Reg` | Game/eventlog.cpp:186 +1 | `extern " " int EventLog_TileIndex_Reg(void* , const ::Object::InternalBasePtr* entityIBP)` | yes |
| `Executable` | Game/force_work.cpp:83 +1 | `bool Executable(const void* p)` | no |
| `FD_OkPtr` | Dialog/followdialog.cpp:49 | `static inline bool FD_OkPtr(const void* ptr)` | no |
| `FindAvailableBuilding` | Entity/CitizenBehavior.h:456 | `void* FindAvailableBuilding(void* out, const Profession* professionKey)` | no |
| `GetBuyEntries` | Component/TradeComponent.h:172 | `void GetBuyEntries(void* outArray)` | no |
| `MemFree` | Game/gameupdateloop_teardown.cpp:65 | `inline void MemFree(void* p)` | no |
| `Pf_RayTriIntersect_aa810` | Action/action_pf_geom.cpp:132 +1 | `int Pf_RayTriIntersect_aa810(void* , const float* P, const float* D, const float* maxT, const float* V0, const float* V1, const float* V2, f` | yes |
| `Pf_TileShapeLut_ae180` | Action/action_pf_geom.cpp:184 +1 | `int Pf_TileShapeLut_ae180(void* , char p2, char p3, char p4, char p5, char p6, char p7, char p8, char p9)` | yes |
| `PickXfCtorMember` | Component/citizenai_executor.cpp:269 | `static Math::Transform* PickXfCtorMember(void* xformOut, const Math::Transform* transformIn)` | no |
| `Plausible` | Component/destroycomponent_workerrelease.cpp:97 | `static bool Plausible(const void* p)` | no |
| `PlayerEdit_AvgTerrainHeight_101730` | Player/exe_bodies.h:53 +1 | `float PlayerEdit_AvgTerrainHeight_101730(void* , int p2, int p3, int p4, int p5)` | yes |
| `PlayerRay_GetMouseRay_1005a0` | Player/exe_bodies.h:96 +1 | `extern " " uint8_t PlayerRay_GetMouseRay_1005a0(const void* , float* outOrigin, float* outDir)` | yes |
| `PlayerRay_ScreenToCell_101270` | Player/exe_bodies.h:97 +1 | `extern " " int PlayerRay_ScreenToCell_101270(const void* )` | yes |
| `PlayerScreenRect_EntityRect_0380` | Player/exe_bodies.h:106 +1 | `extern " " uint8_t PlayerScreenRect_EntityRect_0380(const void* , ComponentEntity* const* param_2, struct Rectangle* param_3)` | yes |
| `PlayerTileRender_BuildRecords_1380` | Player/exe_bodies.h:113 +1 | `extern " " void PlayerTileRender_BuildRecords_1380(const void* , Video::Context* ctx, int param_3, int param_4, uint32_t param_5, int param_` | yes |
| `PlayerTileRender_DrawOverlay_08c0` | Player/exe_bodies.h:112 +1 | `extern " " void PlayerTileRender_DrawOverlay_08c0(const void* , Video::Context* ctx, const Collection::Array<ToolTileRecord>* param_3, const` | yes |
| `PtrCorrupt` | Entity/entitylist.cpp:194 | `static inline bool PtrCorrupt(const void* pv)` | no |
| `RUI_OkPtr` | Dialog/residenceui.cpp:64 | `static inline bool RUI_OkPtr(const void* ptr)` | no |
| `Readable` | Game/force_work.cpp:56 +1 | `bool Readable(const void* p, size_t n)` | no |
| `Reimpl_Array_Resize_Stride08` | Game/exe_bodies.h:124 +1 | `extern " " void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount)` | yes |
| `RosterCompare_9e20` | Component/residencecomponent.cpp:271 | `int RosterCompare_9e20(const void* pa, const void* pb)` | no |
| `SI_Plausible` | Component/statusiconcomponent.cpp:336 | `static inline bool SI_Plausible(const void* p)` | no |
| `UpdateToolUI_ebb0` | Player/exe_bodies.h:45 +1 | `extern " " uint32_t UpdateToolUI_ebb0(const void* , const ::Box* box, const Math::Transform* transform, Math::Vector3* outCorners)` | yes |
| `ValidateTask` | Work/Work.h:146 | `bool ValidateTask(void* unusedRdx, CitizenComponent** citizen, CompleteTask** out)` | no |
| `WeldHash` | Game/water_contour_mesh.cpp:172 | `uint32_t WeldHash(const void* key, int stride)` | no |
| `WorkLeaf_TileDistCompare` | Work/work_leaf_clear.cpp:313 | `int WorkLeaf_TileDistCompare(const void* a, const void* b)` | no |

## C. Casts left, by reason

Generated by `leftfree_c.py` (this directory) at main `cbb701d58`. Grouping is by the words of the tag on each line.

2 counted casts outside `dx11/` (one row per cast; a line with two casts appears twice).

| group | casts |
|---|---:|
| DEFERRED: retail-difference task (known_port_issues 13-14) | 0 |
| DEFERRED: named prerequisite | 0 |
| DEFERRED: dx11 seam step | 0 |
| RULED: stays listed (FLOOR section 4 bars a marker on a this / self operand) | 2 |
| WORK (blocker on the line) | 0 |

### RULED: stays listed (FLOOR section 4 bars a marker on a this / self operand) (2)

| file:line | reason on the line |
|---|---|
| `code/Game/Player/tool_base_render.cpp:730` | FLOOR? vtable probe: ReportBadSelectionRef keeps the tool as an integer so GCC cannot devirtualise its Type() probe (EditTool is final); S4 ruling: no marker (FLOOR section 4 bars a marker on a this / self operand), the cast stays listed |
| `runtime/src/achievement/achievement_state.cpp:217` | audit WORK, left: retail's const CreateInstance stores `this` into the instance's non-const ExternalPtr<Achievement> (+0x20), which code/ readers (goaldialog, gameupdateloop) use non-const; a const pointee would change ExternalPtr<const T> across the readers;  |
