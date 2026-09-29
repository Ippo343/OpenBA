# Inventory C — raw offset reads in PROD (B.4 block 0, §3b.2 and §3b.5)

Table: `data/C_offsets.tsv` (22,578 rows, one per offset access). Script: `scripts/C_offsets.py`
(re-run from the repo root, no arguments, ~10 s, deterministic — two runs compared byte for byte; it
rewrites the TSV and prints every summary quoted below on stdout).

## 1. The answer

1. PROD holds **22,578 raw offset accesses on 21,253 code lines**: `reinterpret_cast` 12,417 · **C-style
   8,050** · helper calls 1,097 · byte-pointer arithmetic `p + K` 659 · `bytes[K]` 331 · `static_cast` 23 ·
   `const_cast` 1. code/Game holds 18,419 (9,676 `reinterpret_cast`, 8,035 C-style).
2. **The receiver's class is known for 7,802 rows (35%)**; 14,776 (65%) read through a pointer whose class
   the code does not say (§4). The 7,802 rows hit **2,107 distinct class+offset pairs in 333 classes**:
   named and asserted 852 pairs (3,363 rows) · named, not asserted 262 (795) · **guess name 601 (2,655)**
   · inside a declared field / byte storage 118 (326) · inside a `_pad` 41 (148) · not declared 85 (210)
   · **past `sizeof(class)` 148 (305)** — the receiver is a derived class or the class is wrong.
3. **Cross-module classes (header first, alone):** GameUpdateLoop (896 reads, 10 modules), CitizenComponent
   (562, 8), ModelComponent (224, 8), CitizenBehavior (205, 8), StorageComponent (161, 8), EntityList
   (117, 7), BuildComponent (122, 6), WorkPlaceComponent (120, 6), EntityUI (101, 5), CitizenDescription
   (92, 5), Profession (59, 5); Video::Device / Video::Configuration (9 and 5 modules) are already named.
4. **Widths disagree** on 117 class+offset pairs between sites (50 of them among strong-evidence rows
   only), and a read's width disagrees with the header's declared type on 650 rows (size) + 134 (float
   vs int) + 535 (signedness). These are rule-sheet questions (§5, §6).

## 2. How the table was built

Files: `code/ runtime/ standalone/ dx11/ engine/`, `.cpp .h .hpp .inc .c`, excluding `standalone/build*`,
`/build/`, `/build-`, `.claude/worktrees`. Comments are blanked with `codegrep.py`'s `strip()` made
length-preserving (so `// +0x..` field comments stay addressable in headers); string contents are blanked.

What counts as a row (`kind`):
- `cast` — `reinterpret_cast<T*>(B + K)`, `(T*)(B + K)`, `static_cast<T*>(B + K)`, casts nested freely
  (`*(float*)((uintptr_t)self + 0x3c)`), multi-line, sums with index terms (`base + 0x3c + i*0x24`: `indexed`
  column). `cast/typedarith`: `int32_t* p; *(p + 6)` scaled to bytes (0x18).
- `addr-cast` — `static_cast<void*>(p + K)` / `reinterpret_cast<void*>(…)`: a field address (placement new).
- `index` — `reinterpret_cast<T*>(B)[n]`, `((T*)B)[n]` (offset n·sizeof T). `(T*)x[n]` casts `x[n]` and is
  NOT an index (C precedence; a bug the first hand-check found).
- `addr` — a byte pointer (`uint8_t*`/`char*` variable, or a byte-view cast) plus a literal not under an
  outer pointer cast (`ScalarInt(rw, p + 0x58)`, `memset(dst + 0x10, …)`); width `-`.
- `byte` — `bytes[K]` on a `uint8_t*`/`char*` variable.
- `helper:NAME` — calls of the offset helpers the script finds itself (§2a).
`K` is a literal (hex or decimal), a named constant (`constexpr`/`const`/`enum`/`#define` in the file or any
header, name clashes dropped), or a constant expression (`0x10 * 2`). Negative offsets (194, array headers
at `p - 8`) are kept.

Columns: `file line class offset width_type header field_name state module kind conf origin indexed
function hdr_type spelling`. `width_type` is the cast's pointee (`ptr` = a pointer-to-pointer read, `-` =
an address). `hdr_type` is the header field's declared type at that offset. `spelling` is the OUTER spelling.

`state`: `named` (field starts at the offset, real name, `static_assert(offsetof)` exists anywhere in PROD) ·
`named_noassert` · `guess` (`field_0x48`, `data3C`, `_pad84`, `unk…`, `_f44`, `_u2c`, or a name ending in
its own hex offset) · `inside` (inside a declared scalar or real-named byte storage, e.g.
`CitizenComponent::meshInstance[0x120]`, or a qword `data1E0` read at +4) · `inpad` (inside a `_pad`/raw
array) · `undeclared` · `beyond` (offset ≥ `sizeof(class)`) · `unknown_class`. For the brief's four
states: named = `named`; guess = `guess`; undeclared = `undeclared`+`inside`+`inpad`+`named_noassert`
needs an assert; `beyond` is a class question, not a header one. No row came out `no_header`: every class
the code names has a definition somewhere (see §5 for the engine/types-only ones).

Header index: every `class/struct/union X {…}` in PROD (namespace tracked; a short name defined in two
namespaces also gets `Ns::X` keys — `Video::Configuration` vs `System::Configuration`); field offsets from
4,097 `static_assert(offsetof)`, then the field's `// +0xNN` comment, then a sequential layout (primitive
sizes, `sizeof` asserts, base class size, vptr). 18 of 1,068 classes keep a field with no computable offset.
When a name has several definitions the `.h` under `code/Game`/`runtime/types` wins over the
`engine/types` POD forwarder and over a `.cpp`-local struct.

`conf` (how the class was found), strongest first: `this` · `decl`/`param` (typed `Foo*`) · `cast` ·
`singleton` (`X::_singleton`) · `alias` (a local initialised from one of these, followed through casts,
reassignments and ternaries) · `member`/`member-field` · `field` (pointee typed in the header) · `return`
(a one-line getter: `GameWorld()` → `GameUpdateLoop::_singleton`) · `wrapper` (`InternalPtr<X>` read at +0)
· weak: `fname`/`fname-abbrev` (a `void* self` of a body named `Class_Member`/`RtNs_Class_Member`/
`Ctor_Class`, the class part nearest the member name wins) · `findname` (`fn_FindComponent_WorkPlace(&x)`)
· `field-comment` (the header comment of the pointer field names the class) · `varname` (`citizenIBP`,
`storage`; a two-word class only, generic names refused). 5,508 of the 7,802 resolved rows are strong;
2,294 weak (varname 936, fname 818, fname-abbrev 294, findname 135, field-comment 111).

### 2a. Offset helpers (117 definitions found, 81 used with a literal offset, 1,097 table rows)
A helper is a ≤400-char function `NAME(T* p, int/size_t off …)` whose body adds `off` to `p` or calls a
helper that does. The same names are redefined per file (`At` ×11, `Field` ×8, `F`, `I32`, `P`, `B`). Top
uses: `Rd<T>` graphics/mesh.cpp 78 · `At<T>` animalcomponent.cpp 71 · `P` gameupdateloop_teardown.cpp 68 ·
`I32` ui/element_common.h 63 · `SetF` wave28_lane_partsys.cpp 50 · `At` citizencomponent_serialize.cpp 48 ·
`W64` engine/singleton_ctors.cpp 45 (+`W32` 19, `W16` 7, `W8` 7) · `Field<T>` menudialogloopt.cpp 44 ·
`Field<T>` video/context_bind.cpp 36 (+context_ctor.cpp 20) · `At` animalwave_serialize.cpp 33 · `F`
wave28 27 / wave27 25 · `At` tradecomponent_serialize.cpp 25 · `I32` audio_scene_update.cpp 23 · `IbSet`
dx11/imagebufferdata.cpp 20 · `I32` ui/widget_common.h 19 (+`PTR` 13, `U32`, `U8`, `F32`) · `F<T>`
ui/combodialog_ctor.cpp 19 · `StoreF` video/batchc_context_setlight.cpp 16. The full list with
definition line, width and use count is the script's last section. No macro wraps an offset read
(`DIALOG_IBP()` and `RTGC_LAYERS()` are casts, not offset reads). vtable-slot helpers (`VSlot`, `Slot`,
`ComSlot`) are not field reads and are excluded (inventory E's floor).

## 3. Counts

Per top directory (rows · resolved · the states of resolved rows):

| dir | rows | resolved | named | named_noassert | guess | inside+inpad | undeclared | beyond |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| code/Game | 18,419 | 6,802 | 2,848 | 555 | 2,603 | 392 | 129 | 275 |
| runtime/src | 3,097 | 766 | 363 | 225 | 43 | 69 | 36 | 30 |
| standalone | 983 | 201 | 143 | 15 | 9 | 13 | 21 | 0 |
| dx11 | 52 | 9 | 9 | 0 | 0 | 0 | 0 | 0 |
| runtime/types | 25 | 24 | 0 | 0 | 0 | 0 | 24 | 0 |
| engine | 2 | 0 | | | | | | |

Per module, rows and the share whose class is known: G/Component 6,003 (46%) · G/Dialog 3,727 (57%) ·
G/Work 2,365 (30%) · G/Game 2,220 (11%) · G/Action 2,053 (17%) · G/Player 1,160 (19%) · standalone 983
(20%) · rt/graphics 927 (21%) · rt/ui 903 (36%) · rt/collection 442 (0%) · G/Tutorial 338 (53%) · G/Goal
292 (16%) · G/Entity 253 (36%) · rt/video 198 (33%) · rt/engine 168 (36%) · rt/io 125 · rt/audio 79 ·
rt/save 77 · rt/system 75 · rt/picking 62 · dx11/src 52.

Top 30 classes by reads (distinct offsets: named / noassert / guess / inside+inpad / undecl / beyond; modules):

| class | reads | offs | nm | na | guess | in | und | bey | mods | header |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| GameUpdateLoop | 896 | 32 | 29 | 3 | 0 | 0 | 0 | 0 | 10 | code/Game/Game/GameUpdateLoop.h |
| CitizenComponent | 562 | 57 | 3 | 0 | 48 | 6 | 0 | 0 | 8 | code/Game/Component/CitizenComponent.h |
| ModelComponent | 224 | 26 | 6 | 0 | 1 | 19 | 0 | 0 | 8 | code/Game/Component/ModelComponent.h |
| CitizenBehavior | 205 | 42 | 0 | 12 | 2 | 28 | 0 | 0 | 8 | engine/types/CitizenBehavior.h (POD only) |
| TradeUI | 193 | 35 | 3 | 0 | 31 | 1 | 0 | 0 | 1 | code/Game/Dialog/TradeUI.h |
| UserInterface | 181 | 42 | 22 | 12 | 1 | 7 | 0 | 0 | 3 | runtime/types/ui/UserInterface.h |
| StorageComponent | 161 | 9 | 3 | 0 | 3 | 1 | 0 | 2 | 8 | code/Game/Component/StorageComponent.h |
| CitizenAIComponent | 155 | 30 | 15 | 0 | 4 | 4 | 0 | 7 | 3 | code/Game/Component/CitizenAIComponent.h |
| TownHallUI | 150 | 49 | 8 | 32 | 0 | 0 | 9 | 0 | 1 | code/Game/Dialog/TownHallUI.h |
| BuildComponent | 122 | 14 | 6 | 0 | 4 | 0 | 4 | 0 | 6 | code/Game/Component/BuildComponent.h |
| TradeWork | 122 | 41 | 3 | 0 | 3 | 0 | 0 | 35 | 3 | code/Game/Work/TradeWork.h |
| WorkPlaceComponent | 120 | 10 | 5 | 0 | 3 | 0 | 2 | 0 | 6 | code/Game/Component/WorkPlaceComponent.h |
| EntityList | 117 | 16 | 9 | 4 | 0 | 0 | 0 | 3 | 7 | engine/types/EntityList.h (POD only) |
| EntityUI | 101 | 23 | 4 | 0 | 3 | 3 | 0 | 13 | 5 | code/Game/Dialog/EntityUI.h |
| CharacterComponent | 95 | 19 | 2 | 0 | 14 | 0 | 0 | 3 | 3 | code/Game/Component/CharacterComponent.h |
| SaveLoadDialog | 95 | 27 | 7 | 6 | 14 | 0 | 0 | 0 | 1 | code/Game/Dialog/SaveLoadDialog.h |
| CitizenDescription | 92 | 36 | 1 | 0 | 33 | 2 | 0 | 0 | 5 | code/Game/Component/CitizenDescription.h |
| AnimalComponent | 89 | 13 | 3 | 0 | 7 | 3 | 0 | 0 | 1 | code/Game/Component/AnimalComponent.h |
| OrchardComponent | 77 | 7 | 4 | 0 | 3 | 0 | 0 | 0 | 3 | code/Game/Component/OrchardComponent.h |
| Video::Configuration | 76 | 19 | 18 | 0 | 1 | 0 | 0 | 0 | 5 | runtime/types/video/Device.h |
| ToolUI | 71 | 14 | 4 | 10 | 0 | 0 | 0 | 0 | 2 | code/Game/Dialog/ToolUI.h |
| MoveTo | 70 | 19 | 3 | 0 | 8 | 8 | 0 | 0 | 1 | code/Game/Action/MoveTo.h |
| TradeComponent | 69 | 21 | 5 | 0 | 9 | 2 | 0 | 5 | 3 | code/Game/Component/TradeComponent.h |
| GrowthComponent | 67 | 9 | 5 | 0 | 4 | 0 | 0 | 0 | 1 | code/Game/Component/GrowthComponent.h |
| ToolbarDialog | 65 | 14 | 14 | 0 | 0 | 0 | 0 | 0 | 1 | code/Game/Dialog/ToolbarDialog.h |
| Video::Device | 62 | 14 | 14 | 0 | 0 | 0 | 0 | 0 | 9 | runtime/types/video/Device.h |
| TornadoComponent | 60 | 11 | 8 | 3 | 0 | 0 | 0 | 0 | 1 | code/Game/Component/TornadoComponent.h |
| Profession | 59 | 6 | 5 | 0 | 1 | 0 | 0 | 0 | 5 | code/Game/Game/Profession.h |
| MapComponent | 58 | 11 | 9 | 0 | 0 | 0 | 0 | 2 | 2 | code/Game/Component/MapComponent.h |
| ParticleComponent | 57 | 10 | 3 | 0 | 5 | 0 | 0 | 2 | 2 | code/Game/Component/ParticleComponent.h |

(`named` on a component often means the `Component` base fields `_entity` +0x10, `_description` +0x18,
`_flags` +0x20; the class's own fields are the guess column.) 87 classes are read from 2+ modules, 39 from 3+.

Reads through a singleton: GameUpdateLoop 719 · Video::Configuration 74 · Video::Device 36 ·
MemoryAllocator 16 · MainLoop 10 · Object::ExternalFactory 7 · Input::Configuration/Device 5 each.

Top files by rows: citizencomponent.cpp 479 (211 of them CitizenComponent) · tradeui.cpp 393 ·
townhallui.cpp 387 · starttown.cpp 283 (15 resolved) · rt/collection/array.cpp 280 (0) · toolbardialog.cpp
241 · standalone/tool_harness.cpp 237 · statvaluedispatch.cpp 229 · action_pf_region.cpp 223 (0) ·
marketwork.cpp 223 · debugui.cpp 212 · action_pf_astar.cpp 211 (0) · gameupdateloop_load.cpp 185.

## 4. Blind spots

- **65% of rows have no class.** Causes (rows): pointer loaded from a field whose type is `void*`/`uint64_t`
  3,827 · a non-self parameter (`d`, `dst`, `node`, `entity`, `param_2`) 3,153 · a base expression the
  resolver does not follow (array elements `list[i]`, `base + i*0x60`, ternaries of calls) 3,151 · a `void*
  self` of a body family with no class 1,454 (`Pf_*` 618, `Reimpl_*` 70, `CitizenList_*` 60, `Impl_*` 50,
  `StartTown_*` 45, `ActCNR_`/`ActRm_`/`ActMv_`/`ActHunt_`/`ActChild_` 126, `TradeCtrl_` 32, `TutWave_` 28) ·
  a call's return 991 (`MemAlloc` 347: a fresh object; `FindComp`/`FindOn`/`FindComponentOnEntity` 120) ·
  an untyped member 725 · an out-parameter 653 (`new(&)` 184, `FloorComponent_FindResData` 115) · other 822.
  Typing 209 known pointer fields would resolve 952 of the loaded rows: `GameUpdateLoop` +0xF8 (109) /
  +0xE0 (26) / +0x128 (19) / +0x138 (13) / +0x148 (10), `StorageComponent`+0x28 (29), `CitizenAIComponent`
  +0x38 (20), `MoveTo`+0x28 (15), `GrowthComponent`+0x38 (12), `FollowDialog`+0xB8 (11). The `*(X+0x0)`
  entries (`CitizenComponent` 131, `BstRoot` 41, `UserInterface` 20) are mostly a variable holding the
  ADDRESS of an `InternalBasePtr` slot (`*(int64_t*)citizenIBP`), not vtable reads.
- **Not in the table:** 2,391 casts whose offset is not a literal or named constant (`p + off`, `base +
  i*stride`); 433 helper calls with a computed offset; 166 typed-pointer sums whose element size is unknown;
  10 `Foo* + n` element sums; vtable-slot helper calls. Reads spelled as members (`self->field`) are not
  raw and not counted. A `(T*)p` read at offset 0 (no `+`) is not an offset read and not counted.
- **Weak evidence.** 2,294 resolved rows rest on a body name, a variable name, a finder's name or a header
  comment. Body names mislead exactly as `known_port_issues` 6 says (§5); `beyond` catches some.
- **Header side.** Offsets computed from asserts, `// +0x` comments and a sequential layout; unions and
  bitfields stop the sequence until the next anchor; 18 classes keep a field with no offset.
  `.cpp`-local structs are indexed too (`BstNode` in tutorial_wave.cpp, `BstRoot` in
  tool_base_highlight_tess.cpp) because nothing else declares them.
- **Hand check (175 rows, four samples, each read at the source line and its function).** Sample 1 (50
  random, first script): 5 wrong — `(T*)x[n]` taken as an index; `transformObj` given its body's class;
  `ExternalPtr<X>` comment read as class `ExternalPtr`; `cpDescV` given class `ConsumeProduce`;
  `WorkplaceUI_*` missed `WorkPlaceUI` (case). Sample 2 (50): 3 of 30 resolved wrong — `InternalPtr<Element>`
  gave `Internal`; `CitizenComp_StorageComponent_QueryCount(self)` gave CitizenComponent (it is a
  StorageComponent); a `FindElement()` element is a subclass of `Element` (now `beyond`); 2 of 20 unknown
  were resolvable (`Ctor_ExternalFactory`). All fixed. Sample 3 (50, final rules): 1 of 32 resolved wrong
  (`CitizenAI_State_AdultTransition(param_1)` → `State`; fixed: one-word classes are never taken from a body
  name), 0 of 18 unknown rows wrong. Sample 4 (25 weak-evidence rows): 2 wrong — `ResidenceComponent_helper_5ec0`
  receives a growable array, not a ResidenceComponent; `TradeWork_impl_04d0_Body` reads +0x1C0 of a 0x58-byte
  TradeWork (flagged `beyond`). **Offset and width were right on every checked row after the precedence fix.**
  Estimated class error: ~2% on strong rows, ~8% on weak rows (≈4% of resolved rows overall). Every
  unknown_class row checked was a genuine offset access (0 of 54 false positives).

## 5. What surprised me (code vs NEXT_SESSION §3a and comments)

1. **C-style offset reads are as common as `reinterpret_cast` ones**: 8,035 in code/Game against 9,676.
   §3a's "7,097 casts of the shape `reinterpret_cast<T>(p + 0x…)`" is a single-line count: the table has
   9,676 `reinterpret_cast` offset sites in code/Game (multi-line reads and `reinterpret_cast<T*>(
   reinterpret_cast<uint8_t*>(self) + 0x288)` forms included).
2. **`static_cast<T*>(static_cast<uintptr_t>(x) + 0x…)` barely exists** (23 `static_cast` offset sites,
   mostly `::new (static_cast<void*>(e + 0xb8))`). The `uintptr_t` spelling hides inside C-style casts:
   `*(int*)((uintptr_t)this + 0x48)`.
3. **GameUpdateLoop's header already names every offset the bodies read** (32 offsets: 29 named and
   asserted, 3 not asserted; 896 reads, 719 through `_singleton` — §3b.3's ≈880 holds). Its problem is the
   pointer fields declared `void*`: 109 reads go through `m_game` +0xF8 and die there.
4. **GameUpdateLoop+0x120 is `m_entityUI` in the header but `camera` in tutorialsetupcamera.cpp:81 and
   `gameSys` in toolbardialog.cpp:1543** (which reads an `isPaused` byte at its +0x30). One of them is wrong
   (inventory D should settle it).
5. **CitizenComponent** (§3b.3 "654 casts in one file"): citizencomponent.cpp has 602 `reinterpret_cast` code
   lines and 479 offset sites, only 211 of them CitizenComponent's own fields (the rest are
   CitizenDescription via +0x288, GameUpdateLoop, CharacterComponent). Its header declares every offset
   0x28–0x290, but 48 of the 57 read offsets under guess names (`data3C`) and with the wrong type: bodies
   read `float` at +0x3C…+0x50 and +0x80 where the header says `uint32_t` (66 rows).
6. **CitizenBehavior (205 reads, 8 modules) and EntityList (117, 7) have only `engine/types` POD headers**;
   28 of CitizenBehavior's 42 read offsets fall in its `_pad` arrays.
7. **Body names mislead** (known_port_issues 6 confirmed): `CitizenComp_StorageComponent_QueryCount` works on a
   StorageComponent; `ResidenceComponent_helper_5ec0` on a `{data, count, capacity}` array;
   `TradeWork_impl_*` bodies in `code/Game/Game/geometry_serialize.cpp` read 35 offsets past TradeWork's 0x58
   bytes; `MenuScene_TransformInit_Standalone(transformObj)` initialises a Transform. 305 rows are `beyond`.
8. **Header types contradict the reads** on 650 rows by size (`WorkPlaceComponent+0x60 void*` read as `int`
   19×, `ModelComponent+0x148/+0x164/+0x28` byte storage read as `Transform`/`int16_t`/`Geometry`,
   `StorageComponent+0x38 void*` read as `int` 11×, `EntityList+0x8`, `Profession+0x28`), 134 float-vs-int,
   535 signedness. The headers were written from ctor stores; the bodies read the real types.

## 6. Proposal: blocks with disjoint file sets

A file belongs to the block of its OWNER class (the class it reads most through `this`/`self`/a typed
parameter). Rows = offset sites in the block's files (the cast count F measures is larger).

**W0-H (first, alone, headers only — every other block reads these):** GameUpdateLoop (type its pointer
fields: +0xE0/+0xE8/+0xF0/+0xF8/+0x118/+0x120/+0x128/+0x138/+0x148), CitizenComponent (rename 48 guess
fields, retype the floats), CitizenDescription, ModelComponent (split `meshInstance`/byte storage: 19
offsets inside it), CitizenBehavior and EntityList (a `code/Game` header each; 28 offsets in `_pad`),
StorageComponent, BuildComponent, WorkPlaceComponent, EntityUI, Profession, CharacterComponent. 12
headers, 0 body files; the compiler checks it (`offsetof` asserts).

| block | owner classes | files | rows |
|---|---|---:|---:|
| B1 citizen | CitizenComponent, CitizenAIComponent, CitizenDescription, CharacterComponent | 23 | 1,633 |
| B2 trade | TradeComponent, TradeUI, TradeWork (+ the TradeWork_impl bodies whose receiver is not TradeWork) | 18 | 1,358 |
| B3 dialogs | TownHallUI, ToolbarDialog, DebugUI, ProfessionUI, SaveLoadDialog, OptionsDialog, WorkPlaceUI, EventNotifyDialog, CitizenUI, HelpUI, EntityUI, ToolUI, … | 55 | 3,166 |
| B4 components | Orchard, Particle, Map, Pasture, Build, Growth, Herd, Tornado, CropField, Population, WorkPlace, Immigration, Animal, StateMachine, StatusIcon, Residence, Destroy, ClearArea, Model, … | 77 | 3,269 |
| B5 work | MarketWork, ConsumeProduce, PlantArea, GatherRawMaterials, PastureWork, ForesterWork, HarvestOrchard, … | 28 | 1,665 |
| B6 actions + tools | MoveTo, PickupStoredRawMaterial, … ; PathTool, EditTool | 26 | 819 |
| B7 game + tutorial + goal | EntityList, PathGraph, PathfinderThread, MenuDialogLoopT, Tutorial* | 24 | 994 |
| B8 runtime | UserInterface (27 ui files), MeshInstance/ParticleSystemInstance/Animation/EdgeMesh (14 graphics), Context (10 video), ExternalFactory, io, system, collection | 69 | 1,710 |
| B9 standalone | two harness files with an owner class | 2 | 75 |
| U no owner | Pf_* pathfinding (Action), StartTown/WorldGen (Game), statvaluedispatch (Goal), collection/array.cpp, standalone harness, CitizenList, removal-calc (Player) | 333 | 7,884 |

B1–B9 are disjoint by construction (one owner per file). U is not a conversion block: its bodies have no
retail class yet; they need class identification first (Ghidra `this` use) and belong to inventory A/B.
Order: W0-H alone → B1 and B2 (the two largest cross-read owners) → B3–B8 in parallel.

**Would change behaviour (each a `behaviour:` commit if retail disagrees, known_port_issues 5):**
- Size conflicts between sites on 50 class+offset pairs among strong-evidence rows (script section "width
  conflicts"), e.g. `CitizenComponent+0x44` written as `int64_t` 0 at citizencomponent.cpp:273 (it also
  zeroes +0x48) while 8 sites read a 32-bit float; `GameUpdateLoop+0xD0` read as `uint8_t` (4) and
  `uint32_t` (16); `EntityUI+0x30` `uint64_t` (9) vs `uint8_t` (5); `UserInterface+0x20/+0x30/+0x50`
  `int32` vs one qword store each; `HarvestArea/HarvestOrchard/PlantArea/PlantOrchard+0x60` int vs qword;
  `SaveLoadDialog+0xB9` byte vs dword. Many are qword stores zeroing two dword fields (retail's merged
  stores); each needs its retail instruction before it becomes one field write.
- Signedness: 98 pairs (`WorkPlaceComponent+0x60`, `CitizenBehavior+0xA8`, `WorkComponent+0x30`,
  `ModelComponent+0x164/+0x166` int16 vs uint16, `StatusIconComponent+0x100`).
- Float vs int: 19 pairs (`SliderElement+0x128` 31 sites, `ModelComponent+0x168`, `CitizenComponent+0x48/
  +0x4C/+0x80`, `CitizenDescription+0x70/+0xD0/+0xF0`) — type puns; the field type must follow retail.
- Receivers that are not the body name's class: the 305 `beyond` rows and §5.7.
