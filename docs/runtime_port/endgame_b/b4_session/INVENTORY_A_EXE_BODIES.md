# Inventory A — the C bodies of `code/Game` and the class each belongs to (B.4 block 0)

Owner A, read-only. Table: `data/A_exe_bodies.tsv` (879 rows). Script: `scripts/A_exe_bodies.py`.

## 1. The answer

- **879 C-body declarations** in 65 headers (98 scanned): the nine `exe_bodies.h` hold 650
  (Component 178, Action 145, Work 83, Game 82, Dialog 65, Player 51, Entity 25, Tutorial 21, Goal 0),
  the `*_funcs.h` hold 147, and eight other helper headers hold 82 (`ui_panels.h` 47, `ui_dialogs.h` 8, ...).
  They are defined in **249 files** that hold **15,858 `reinterpret_cast`** code lines between them.
  The table has **4,317 PROD reference lines**. **71 bodies have no PROD reference at all.**
- **Class found for 713 of 879 (151 classes); 166 have no class.** Confidence: **210 high** (checked
  against retail), 215 medium, 454 low.
- Where they go:
  - **81 virtual slots**: the address sits in a retail vtable.
  - **33 ctors/dtors**: retail stores the class's vtable into `[this]`.
  - **152 template instantiations**: ComponentEntity `FindComponent<T>` 65, `Collection::Set` 41,
    `Collection::Array` 40, `Object::ExternalPtr` 15.
  - **~400 non-virtual members**: RCX is used as an object. Retail cannot tell a member from a free
    function that takes a pointer.
  - **~50 free or static functions**: RCX is a scalar, unused, or an `IO::Serializer`.
- **The member is already DEFINED for 131 rows.** Those C bodies are a second body of an existing member:
  rewire the callers and delete the body; nothing is converted. It is declared but not defined for
  21 rows, the C body being the only one. It is not declared for 507 rows.
- **One retail address has 2 to 5 C bodies in 32 cases (71 rows).** **45 rows are misnamed**: the name
  says `Serialize` / `Shutdown` / `OnRemove` but the retail slot is `Startup` / `Persist` / `Tick`.

## 2. How the table was built (re-run: `python docs/runtime_port/endgame_b/b4_session/scripts/A_exe_bodies.py`)

- **Scope.** `code/Game/*/exe_bodies.h`, `code/Game/**/*_funcs.h`, `code/Game/Work/marketwork_bst_*.h`,
  plus `EXTRA_HEADERS` (`ui_dialogs.h`, `ui_panels.h`, `dialog_exe_helpers.h`, `citizen_icon_helpers.h`,
  `goal_exe_helpers.h`, `statvaluedispatch.h`, `Game/game.h`, `controller_state_table.h`).
- **Parsing.** Comments are stripped with `b4_session/codegrep.py strip`. A declaration is a
  namespace-scope function prototype or inline definition; namespace / `extern "C"` braces are
  transparent, class bodies are skipped.
- **Address**, in this order:
  1. the declaration's trailing `// 0x14…` (620 rows);
  2. the comment block right above it (158);
  3. `FUN_14…` in either;
  4. the comment above the DEFINITION (58);
  5. the B3R `P0*_declared.tsv` tables (8);
  6. Ghidra, by `Class_SlotN` vtable read or exact name (2).
  33 rows have no address; 13 of them say "no retail address" (standalone harness entries).
- **Retail pass** (Ghidra, `Application-x64.exe`, read-only through the `ProgramManager`; no
  `switch_program`, no transaction). The Java is embedded in the script (`--ghidra-script`). For each
  address it records:
  - `retail_name`: the Ghidra name. It is ours from the exe phase and is **evidence of the lowest rank**.
  - `retail_vtable`: every `.rdata` qword equal to the address. The table start is found by walking back
    over code pointers to the nearest named label, giving the slot index.
  - `retail_this`: a linear scan of the first 120 instructions tracking RCX and its register copies.
    It records the `[rcx+off]` offsets read, `rcx=scalar` (ECX/CL read) / `unused` / `fwd` (passed on
    to a callee), `stores-vtable=` (the table stored at `[this+0]`, i.e. a ctor or dtor),
    Ghidra's `param_1` type, `nparams`, and the retail callers.

  Its output is cached in the table's trailing columns and re-read on a re-run. The re-run is
  byte-identical without Ghidra: md5 checked twice.
- **Class attribution**, the first rule that fires wins:
  1. vtable slot: **high**;
  2. the stored vtable: **high**;
  3. a retail receiver shape (`+0x30/+0x38` with a `FindComp*` name gives ComponentEntity;
     `+0/+8/+0xc` with an Array verb gives `Collection::Array`): **high**;
  4. a tree class header declares a member carrying this address in its comment: **medium**;
  5. the `ComponentEntity` field set: medium;
  6. `IO::Serializer` receiver: a persist helper, no class;
  7. Ghidra namespace, or first parameter typed `Class*` in the tree: medium;
  8. Ghidra name prefix plus RCX used as an object: medium or low;
  9. every retail caller is one class's function: low;
  10. the body-name prefix, or the defining-file stem: **low**.

  Ghidra's auto-typed `param_1` (`Internal*`, `InternalBasePtr*`, `Thread*`, `Serializer*`, `Base*`)
  is excluded: it was noise on 80+ rows.
- **Columns.**

  | column | meaning |
  |---|---|
  | `addr` | the address |
  | `name` | the body |
  | `module` | the module |
  | `defining_file` | `file:line` of the definition(s), from the whole PROD tree |
  | `class` | the class found |
  | `class_header` | where the class is defined (`none` = no header) |
  | `member_declared` | the member name found in that class: by address in the header, else by the name after `Class_`. `addr-in-header` = the address appears in the header but not on a member line. `no` = not declared. |
  | `member_defined` | `yes:<file>` when `Class::member` has a DEFINITION |
  | `static_or_member` | member / member(virtual) / member(ctor/dtor) / member-template / member? / member-of-unknown / static? / free |
  | `callers` | PROD code lines naming the body, minus its declaration and definition lines |
  | `confidence` | high / medium / low |
  | `note` | the evidence, plus the flags `NAME MISLEADS`, `MEMBER NAME MISLEADS`, `CONFLICT`, `SAME ADDRESS`, `member already DEFINED` |

  Extra columns: `decl`, `first_param`, `caller_files`, `retail_*`.

## 3. Counts

- **Per module** (rows · high/medium/low · no class):

  | module | rows | high | medium | low | no class |
  |---|---:|---:|---:|---:|---:|
  | Component | 285 | 65 | 83 | 137 | 24 |
  | Action | 145 | 31 | 17 | 97 | 54 |
  | Dialog | 125 | 49 | 48 | 28 | 8 |
  | Game | 99 | 23 | 16 | 60 | 25 |
  | Work | 96 | 17 | 22 | 57 | 21 |
  | Player | 55 | 3 | 6 | 46 | 20 |
  | Entity | 50 | 21 | 22 | 7 | 5 |
  | Tutorial | 21 | 0 | 0 | 21 | 9 |
  | Goal | 3 | 1 | 1 | 1 | 0 |

  PROD reference lines: Component 1,238, Action 944, Work 739, Game 529, Entity 286, Dialog 280,
  Player 225, Tutorial 66, Goal 10.
- **By first parameter:** `void*` 398, integer or `uintptr_t` 280, already a typed pointer 191,
  none or scalar 10.
- **Top classes** (rows, high, member declared / defined, PROD reference lines, defining files):

  | class | rows | high | declared/defined | references | files |
  |---|---:|---:|---|---:|---:|
  | *(none)* | 166 | – | – | 1,042 | 84 |
  | ComponentEntity (65 `FindComponent<T>` + members) | 65 | 57 | 4/4 | 887 | 27 |
  | CitizenComponent | 49 | 1 | 1/1 | 197 | 14 |
  | Collection::Set | 41 | 0 | 12/12 | 291 | 16 |
  | Collection::Array | 40 | 40 | 32/32 | 177 | 24 |
  | TradeUI | 24 | 0 | 0 | 59 | 6 |
  | CitizenAIComponent | 20 | 1 | 1/1 | 30 | 9 |
  | Object::ExternalPtr | 15 | 0 | 14/14 | 175 | 8 |
  | TradeWork | 15 | 0 | 0 | 54 | 11 |
  | TownHallUI | 15 | 0 | 0 | 19 | 1 |
  | PerformWork | 13 | 1 | 0 | 33 | 5 |
  | MenuDialogLoopT | 13 | 10 | 10/10 | 16 | 1 |
  | TradeComponent | 12 | 3 | 3/3 | 14 | 9 |
  | BuildStructure | 11 | 2 | 2/2 | 20 | 6 |
  | DiseaseComponent | 11 | 0 | 0 | 20 | 1 |
  | CitizenBehavior(Raw) | 11 | 0 | 0 | 5 | 1 |
  | OptionsDialog | 10 | 1 | 0 | 19 | 3 |
  | HerdComponent | 9 | 1 | 1/1 | 16 | 3 |
  | FireComponent | 8 | 0 | 0 | 22 | 2 |
  | RoadTool | 7 | 0 | 0 | 16 | 3 |
  | CharacterComponent | 6 | 0 | 0 | 104 | 6 |
  | AnimalComponent | 6 | 0 | 0 | 29 | 4 |
  | ResidenceComponent | 6 | 1 | 1/1 | 14 | 4 |
  | WorkPlaceComponent | 6 | 0 | 0 | 38 | 2 |
  | StateMachineComponent | 6 | 5 | 5/5 | 17 | 1 |
  | EntityList | 6 | 0 | 0 | 53 | 2 |
  | CreateNaturalResource | 6 | 1 | 1/1 | 10 | 5 |
  | WorkComponent | 5 | 1 | 1/1 | 68 | 2 |
  | StorageComponent | 5 | 0 | 0 | 35 | 3 |
  | TerraformComponent | 5 | 3 | 3/3 | 3 | 1 |

  151 classes in all. The rest hold 1 to 5 rows each; the per-class breakdown is in the table.
- **Unattributed 166, by body prefix:** `Pf_` (pathfinder / map grid) 35, `CitizenAI_` 10 (state
  helpers), `Reimpl_` 8, `fn_` 8, `Game_` 7, `WorkLeaf_` 6, `TutBase_` 6, `ActRm_` 5, `ActCNR_` 5,
  then `WorkBase_`, `WorkAct_`, `TownHall_`, `PlayerScreenRect_` 4 each. `Pf_*` is a class retail has
  (the path / region grid: `[rcx+0x68..0xa4]`, `+0x2428` for the quadtree) and the tree does not. It
  needs a header of its own, which is D's or the rule sheet's call.

## 4. Blind spots, and what was read by hand

1. **Retail cannot distinguish a non-virtual member from a free function that takes the object.** MSVC
   x64 puts `this` in RCX either way. `member` means "RCX is dereferenced as an object and the class
   evidence points at X". Only the vtable, ctor/dtor and template shapes are marked high. A static
   member cannot be told from a free function at all: `static?` is a guess.
2. **The RCX scan is linear, 120 instructions, and not flow-sensitive.** It misses a receiver first used
   after a branch past the window: `rcx=obj()` with an empty offset list is 50 rows. The ECX/CL
   "scalar" test can misfire on a function that tests the low byte of a pointer.
3. **The address from a comment above the definition (58 rows) can name a callee, not the body.**
   Seven of them collide with another row's header address and are flagged `ALSO another row's header
   address`. Read by hand:
   - `EntityList_FlagAggregation` and `PopulationComponent_OnRemove` are genuine second bodies.
   - `TradeUI_RefreshOrderQuantityDisplay` @0x140006270 is WRONG: that address is `FindComponent<Storage>`,
     a callee.
4. **Ghidra's `g_*_Vtable` labels are wrong twice.** `g_PauseDialog_Vtable` 0x14015f550 holds
   `MainMenuDialog_*` slots (the tree's `MainMenuDialog.h` says table 0x14015f550).
   `g_ReloadDialog_Vtable` 0x14015c480 holds `MessageDialog_*` slots. The script ranks `vtable_X`
   first, then the tree's `table 0x…` banner, then `g_X_Vtable`.
5. **`callers` counts lines naming the body.** Overloads and same-named file-local functions are
   counted together. `FindComponentByTypeId` has 10 definitions in 10 files. A call through a
   function-pointer table is counted only where the name is spelled.
6. **Member names are matched by the address comment in the class header, else by `name` minus
   `Class_`.** A member declared with no address comment and under another name reads `no`. The 52
   `addr-in-header` rows have the address in the header but not on a declaration line; read them by hand.
7. **`low` rows (454) are name- or file-level evidence only.** They are the W1 owner's to confirm with
   the retail receiver before moving.

**Hand sample: 40 rows** (random, seed 4). Five rules were fixed on the result, then the rows re-checked:
- `ARRAYISH` no longer matches a bare "Array": `CitizenUI_BuildIconArray_Impl` had been marked high.
- `Reverse` added to the Array verbs (`Tornado_ParticleReverse`).
- A Serializer receiver makes a persist helper, not a member (`TutBase_SerializeSetupArray`,
  `OrchardComponent_SerializeRefEntries`).
- `SerializeArray` mapped.
- `FindComponentByTypeId` (3 params) added to the ComponentEntity shape.

After the fixes, **2 of 40 wrong (5%), 3 uncertain (8%); all 5 are low or medium; 0 of 9 high wrong.**
- **Wrong:** `ActMv_CitizenBreakCheck_116650` reads `[p+0x1f8]` and `[p+0x30]`: a CitizenComponent,
  not the `PerformWork` its callers suggest. `CitizenComp_SortByProfession_02a0` is a sort helper, not
  a CitizenComponent member.
- **Uncertain:** `CitizenAI_PerformWorkCtor6` (a factory that builds a PerformWork: static, not
  `this`), `HerdComponent_Impl_e510`, `PastureComponent_ReassignEntityRef`.

Expect ~5–10% error in `low`, ~0 in `high`.

## 5. What surprised me

1. **§3a says "seven `exe_bodies.h`" with Dialog 39, Game 67, Component 172.** There are nine. Entity
   has 25; Goal's declares nothing. Counting declarations (multi-line, class-typed, the inline bodies
   moved from `engine_funcs.h`) gives Dialog 65, Game 82, Component 178. The C bodies are not only in
   `exe_bodies.h`: 229 more are in `*_funcs.h` and helper headers.
2. **131 C bodies already have a DEFINED member for the same retail function.** Examples:
   - `ActHunt_NotifyEntityChanged_c3470`, `EntityList_FlagAggregation`, `ComponentEntity_RefreshEntityFlags`:
     three bodies of `ComponentEntity::RefreshEntityFlags`, which is itself defined.
   - The Action copy ctors.
   - The component slot bodies.

   For these the conversion is "rewire callers, delete the body", not "make it a member".
3. **32 retail addresses carry 2 to 5 C bodies:**
   - the `FindComponent<T>` family: every module grew its own copy (`fn_FindComponent_Storage`,
     `WorkBase_FindComponent_6270`);
   - `StorageComponent_QueryCount` ×3;
   - `CitizenBehavior_Find{Any,Idle,Working,Student}` and `CitizenEmploy_Strat_*`: two bodies each;
   - `MenuDialogLoopT` 0x14009a930 split into five phase bodies (`LoadScene_PhaseA`, `LoadSceneA1`,
     `…PhaseB`, `…PhaseC`, `LoadMusic`): one retail `Load` in pieces.

   A **split body is not a member**: its pieces are the standalone's loader, and merging them back
   changes behaviour.
4. **45 body names mislead about WHICH member.** The retail vtable slot contradicts the verb, and W1
   must name members from the vtable, not from the body:
   - `PastureComponent_Shutdown`, `TradeComponent_Serialize`, `WildAnimalComponent_Serialize` are `Persist`.
   - `StateMachineComponent_Serialize`, `DecalComponent_Serialize`, `TombstoneComponent_Serialize` are `Startup`.
   - `StateMachineComponent_OnRemove`, `NaturalResourceComponent_OnRemove`, `TornadoComponent_OnRemove` are `Tick`.
   - `ResidenceComponent_OnCreate`, `TradeComponent_OnCreate`, `ZoneComponent_OnCreate` are `OnRemove`.
   - `SpriteCaptureUI_Serialize` is `OnCreated`.
   - The `*_Draw` bodies are `OnAdded`.
5. **The body names the wrong CLASS 5 times** (`NAME MISLEADS`), besides 19 `CONFLICT`s with the
   Ghidra name:
   - `MessageDialog_Ctor_Standalone` stores the table the tree calls MessageDialog, but Ghidra's label
     says ReloadDialog.
   - `ToolUI_OpenToolDialog_ff9e0` constructs a `ToolUI_ChildDialog`, which has no header in the tree.
   - `CropUI_Slot8`'s slot is shared by the CropUI, OrchardUI and PastureUI tables: a base-class member.
   - The `*_EnterBuilt` / `*_EnterDemolish` state handlers (Cemetery, Happiness, Healer, Well) are
     named for the Component; Ghidra puts them in the `*Description` (the controller). Read these
     before choosing the class.
6. **Five header or comment "addresses" are DATA, not code.**
   - `fn_FindComponent_{Residence,MultiObject,Fire,Unknown7ee8}` carry the `DAT_` type-id cell's
     address.
   - `CitizenBehavior_Ctor` carries 0x140172234 (`.rdata`).
7. **71 declared bodies have no PROD reference** (tests reference none of them).
   - 35 are second bodies of a defined member.
   - 18 are the empty `Persist` / `OnClosed` inline slot bodies in `ui_panels.h`.
   They can go before W1 as a separate no-behaviour commit, after a read of each.
8. **`FindComponentByTypeId` is defined 10 times:** 9 file-local overloads in 9 `.cpp` files, plus
   the inline in `Entity/exe_bodies.h`. All are retail 0x1400c3680, i.e. `ComponentEntity`'s generic
   find.

## 6. Proposal: blocks with disjoint file sets

A block owns the class headers plus every file defining their bodies. Measured by union-find over
"classes sharing a defining file". With every file counted, one component holds 31 classes, 62 files
and 7,066 casts. **Four grab-bag files glue it together:** `Dialog/ui_panels.h` (18 classes),
`Action/action_pf_ctors.cpp` (8), `Action/action_child_ctors2.cpp` (6), `Action/action_child_ctors.cpp`
(4). The casts below are `reinterpret_cast` code lines in the whole defining files.

- **W0.5, first, no behaviour:**
  1. Delete the 71 unreferenced bodies after reading each.
  2. Move the bodies of the four glue files into their classes' files, one commit per file, compared
     per object file.

  After that, 82 components remain; the largest has 8 classes.
- **B-T, templates and ComponentEntity. Must come FIRST and alone.** Their callers are in every module:
  1,530 reference lines.
  - Files: `ComponentEntity.h`, `runtime/types/collection/Collection.h`, `object/External.h`, plus the
    72 files defining the 161 bodies.
  - Rows: `FindComponent<T>` 65 (34 files, 2,147 casts), Array 40 (24 files, 1,662), Set 41 (16 files,
    1,048), ExternalPtr 15 (8 files, 311).
  - Most rows are already DEFINED members (`SetSize`, `Insert/Find/Remove`, `Serialize/SerializeArray`),
    so this is caller rewiring plus deletion.
  - ComponentEntity has no `FindComponent<T>`: adding it is the one new member.
- **Class blocks** (rows · files · casts), once the glue files are split:

  | block | rows | files | casts |
  |---|---:|---:|---:|
  | CitizenComponent + CitizenAIComponent + ResidenceComponent + StorageComponent + BuildRoad + the tutorial dialogs they share | 90 | 28 | 2,688 |
  | BuildStructure + CreateRawMaterial + Herd/Population/Trade/Zone components + RoadTool (the `action_pf_*` files) | 53 | 20 | 2,013 |
  | CharacterComponent + the Player tools (Edit/Placement/Removal/Quarry) + EntityUI + TradeWork + Decal | 42 | 24 | 1,800 |
  | the Action ctors (Animal, MoveTo, PerformWork, PlayAnimation, LocationWork, PickupRawMaterial) | 31 | 7 | 534 |
  | MenuDialogLoopT + OptionsDialog | 25 | 3 | 278 |
  | TradeUI + TradeDescription | 25 | 6 | 392 |
  | TownHallUI | 15 | 1 | 228 |
  | CitizenBehavior + PathGraph | 14 | 2 | 272 |
  | Create/Animate/RemoveNaturalResource + MapComponent | 11 | 4 | 315 |
  | 70 small blocks of 1–3 classes | ≤11 | | |

  The two largest should be split along the `SAME ADDRESS` duplicates once those are deleted.
- **B-U, the 166 unattributed rows** (84 files, 6,132 casts): read before any block claims them.
  `Pf_*` (35) needs a pathfinder/grid class header first: the D owner or the rule sheet.
- **Behaviour risks the blocks must name:**
  - The 45 slot-name mismatches: name the member from the vtable.
  - The 5 class mismatches, and the Description-vs-Component state handlers.
  - The split `MenuDialogLoopT::Load` (keep the pieces, do not merge).
  - Duplicate bodies that are not byte-identical: `EntityList_FlagAggregation` delegates, but
    `ActHunt_NotifyEntityChanged_c3470` is its own copy. Compare each copy against the member before
    deleting it, because a copy can differ in width or guard.
  - `CitizenBehavior_Find*` against Ghidra's `CitizenComponent_impl_*` (receiver: the CitizenBehavior
    list, confirm).
