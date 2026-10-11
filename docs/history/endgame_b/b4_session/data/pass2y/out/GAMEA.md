# Wave Y inventory: GAMEA (`src/game/Game/`, `Tutorial/`, `Goal/`, `Object/`)

181 cast lines, 69 units (`GAMEA.tsv` per line, `GAMEA_units.tsv` per unit). Retail dtor orders were read
from the disassembly (Application-x64.exe) for MenuDialogLoopT, TutorialStep, Map and MenuScene. The other
orders come from the tree's own quoted disassembly comments.

| kind | casts | units |
|---|---|---|
| CLASS | 64 | 27 |
| RULING | 57 | 10 |
| POD | 18 | 4 |
| SIG | 28 | 21 |
| LOCAL | 11 | 5 |
| ERASED | 3 | 2 |

## Doable classes: one commit each
- **MenuDialogLoopT (10).** Retail 0x14001a310 has an empty body. It frees +0xbf0, +0xbe0, +0xbd0, then ~Audio::Scene +0xb30, which is reverse declaration order.
  Type `_audioScene` together with the three arrays. Ledger line 356 holds only for the arrays typed alone.
  Prerequisite (RT): `Audio::Scene` needs a C++ default ctor; today it is `RtAudio_Scene_Ctor`.
  The ctor's memset must stop before `_particleArray`. `_dlcShip` (line 104) is separate: its class is unsettled.
- **TutorialStep (5).** Retail 0x1400e33f0 frees +0x60, +0x50, +0x40, then ~String +0x38 and +0x30, which is reverse declaration order.
  Type all five members together. Ledger line 459 ("order reversed") applies only to the arrays typed alone.
- **Map (3).** Retail 0x14009ed60 frees +0x68, +0x70, +0x78, +0xE8 in the body. Then come cellQueue +0xA8, cells +0x80 and the tail ~Picking::Scene +0x08.
  Type pickingScene, cells and cellQueue together, and keep the first four in the body.
  Prerequisite (RT): a C++ `Picking::Scene` ctor; today it is `RtPicking_Scene_Ctor`.
- **MenuScene (1).** Retail 0x1400a3970 deletes +0x28, calls 0x14001b860 on +0x1E8 and then +0x1C8, then frees the arrays and the String.
  Move `MenuSceneTreeMap` into the header with 0x14001b860 as its dtor.
- **PathGraph (3).** 0x1400171b0 (the tree, then +0x00) becomes `~PathGraph`. The helpers B, Arr and I32 go once PlayerToolManager line 183 is done.
- **NamedExternalRef (4) + TutorialGoalCount (1).** TutorialGoalCount's 0x140027820 frees +0x48, then +0x30, which is reverse order once `name` is a String.
  One commit owns all 9 owner classes. Each owner's dtor order still has to be read.
  The header's "out-of-line String()" objection has precedent against it: about 20 code/Game headers already hold String members.
- **FIELD only (no lifecycle):**
  - PlayerToolManager: currentTool becomes `InternalPtr<Tool>`, and the {root, count} pair becomes one header.
  - ToolRegistryNode, TrackedEntityNode, MapCell, EntityList and CitizenBehavior arrays: each slot becomes `InternalPtr<T>`. InternalBasePtr has an inline zero ctor and a defaulted dtor.
  - ClimateGenerator: `float[48]`.
  - NameDatabase: uint32 4-gram, `Array<char>`.
  - PlayerController: `Vector3 target`, initialised in the mem-init list.
  - Video::Context: `_unknown20f0` as a pointer.
  - Graphics::Geometry: InterpolatedState.
  - EnvironmentDescription: COMP partition.
- **WorldGeometry (1).** doable-with: everything from +0xe8 to +0x220 in one commit.
  Retail 0x1400aed50 frees +0x148 and +0x140, then +0x220 down to +0x1a0, the buffers +0x180 to +0x100, and the +0xe8 tree, in reverse declaration order.

## Blocked
- **CharacterGrid (2) / Minimap (2).** The order is retail's (0x140018220 frees +0x50 down to +0x10; 0x1400a4fc0 frees images, then +0x28, +0x18).
  The block is gameupdateloop_load.cpp: trivial-construct asserts at :525 and :454, and memsets at :528 and :1012. charmanager_seed.cpp:76 also memsets.
- **EventLog (3).** It has two element rows: grow 0x140019b70 and resize 0x1400199a0 have different unwritten extents and init.
  It also has a trivial assert (:450) and a memset (:912).
- **EventLogEntry (1).** It is a stack image; a real local runs String's ctor and dtor.
- **Frustum (3).** The implicit ctor zeroes six Vector4 planes.
- **System::Random (1) and Engine::Engine (1).** Both are ctor doors.
- **Video::Context `_pixelConstantStaging` (1).** Vector4 has a zeroing ctor (ledger X-RL FIELD left).
- **CitizenBehavior line 9.** +0x160 holds two node types on the dead pre-v2 path.
- **SIG WorkCellBST_Remove_ca60 / WorkBST_BSTRemove_a6e0 (2).** Blocked by ledger round 1: one agent must own every Work leaf.

## PODs (18): ctor/dtor doors take the mirrors
RtGraphics::Renderer (7), Scene (7), AnimationManager (2) and RtSystem::CriticalSection (2). Field identity was not checked: that is R-GFX / R-SYS work.
## SIG / ERASED (31)
- Resize C entries take other owners' raw triples as `void*`: Stride20, 08, 14, 10_Spawn, Struct60B, the ByteHeader / Outline entries and WeatherArray. Each goes when its owners' headers are typed.
- The ExternalPtr thunk family is ForwardRef, the void** ExternalPtrSerializeGeneric, SerArray and ArraySerializeExternalGeneric. The erased core ExternalPtrSerializeBody needs a FLOOR? ruling.
- DECLARE_EXE_TYPE returns void* (3 lines, about 630 definitions). The RgPathVec / WgPathVec twin spec rows are one ERASED line.
- ToolbarUtility_BstInsert_4890 (4), MapTileGrid_Build, Pathfinder_Seed, CanonicalMapGrid, CitizenComp_DeferredInit_117080, CitizenBehavior::Update(int64_t*) and Context::BindIndexBuffer (const) are one line each.

## LOCAL: stale tags (11)
- **game_main.cpp:205-211 (4).** Device.h:186-187 already holds `IXAudio27*` / `IXAudio2MasteringVoice*`. Call DestroyVoice() and Release().
- **gamegrid_serialize.cpp:118, 136 (3).** Use a 9-byte copy (B11 to confirm).
- **citizen_names.cpp:65 (1).** The dword store can be a 4-byte memcpy.
- **Diagnostic stack images (3).** The round-trip SavedState probe (2) and the BuildingGoal probe (1).

## RULING totals (57)
| ruling | casts |
|---|---|
| const_find (FindOrLoad / Find / FindNext) | 29 |
| alloc_shape:malloc (water_contour_mesh 13, round-trip 1) | 14 |
| h8 (climate, terrain, settings bool; event colour uint) | 4 |
| retail_diff:none (version-5 clamp; standalone first-tick Time clamp x2), no row yet | 3 |
| retail_diff:95 | 2 |
| retail_diff:13 / 61 / 63 / 470 | 1 each |
| vptr_probe (splash_loop_stubs.cpp:428) | 1 |
