# B7 Block 0 — the named-prerequisite inventory

Written 2026-10-09 by the coordinator (read-only) at main `a4944195c` from the 23 tagged lines
(`b4_session/LEFT_FREE.md` §C "DEFERRED: named prerequisite"), the measurements already on each line
(B4 S4 RT2 / LAST, Runtime-x64.dll disassembly) and the type headers. Line numbers are today's.
Verdicts: **TYPE** (the prerequisite supplied, then a typing; expect `B7:`), **BEHAVIOUR** (a store /
order / destruction point retail lacks or has goes with it; `behaviour:`), **MEASURE** (the shape is
decided by B11 at the site; the fallback is a re-tag with the measurement), **RULING** (coordinator's
decision stated in B7_RULES D8).

## A. The 23 casts by unit

### A.1 Unit 1 — the inline release (RT block, 11 casts)

Blocker on every line: `String()` 0x180006760, `String(const String&)` 0x180006cb0, `~String` 0x180006c80,
`String::Release` 0x1800c9870 and `rtcoll::ArrayDtorImpl` are OUT-OF-LINE in the tree
(`system/string.cpp:308 / 316 / 327 / 132`, `collection/array.cpp:254`) where retail open-codes them at
the measured callers. Prerequisite: header-inline definitions with the same reads / stores (B7_RULES D4).

| file:line | what is cast | measured retail | fix after the prerequisite | verdict |
|---|---|---|---|---|
| `runtime/src/ui/documenttext.cpp:155` | `char**` slot viewed as `System::String&` | GetNextItem 0x1800cdf40: every release open-coded (DEC [data-4]; CALL 0x1800c7ad0 at 1800ce43f / ce780 / ce973 / cea38 / cea68 / ceacb / cebb2 / ced7e / cee00 / ceedb / cef9f; no ~String call) | `System::String` locals in GetNextItem; `Str()` / `ReleaseString` / `AssignString` helpers go where the member op has the same instructions (the self-assignment-first test of `AssignString` is `operator=`'s 0x1800c9810 shape) | TYPE / MEASURE (B11: MemFree the only call per release) |
| `:157` | the const view of 155 | same body | same | TYPE |
| `:159` | `char**` viewed as `System::Filename&` | GetNextItem's `wanted` / `path` released at 1800ce973 / cea38 / cea68, no ~Filename call | `System::Filename` locals | TYPE |
| `runtime/src/io/modification.cpp:272` | `void*` dst as `ModArrayOf<uint32_t>*` (0x180011110 `Array<int32>::operator=`) | RT2: the by-value `Collection::Array<uint32_t> GetIconData() const` shape built: -O2 added an EH pad (ArrayDtorImpl + _Unwind_Resume) in 0x180025530 and an out-of-line ArrayDtorImpl call in the caller; retail RefreshList inlines the free + zero stores (0x1800afb71..0x1800afbb6) | with the inline ArrayDtorImpl, re-build the by-value shape; B11 the caller for the inline free + three zero stores and no landing pad with a call | MEASURE |
| `:273` | `const void*` src as `const ModArrayOf<uint32_t>*` | source is `Collection::Array<uint32_t>` (Mod::ints, PackageFile::_iconData) | the entry takes `Collection::Array<uint32_t>&` / `const Collection::Array<uint32_t>&`; `ModArrayOf` goes if no header-type split remains | TYPE |
| `:286` | dst as `ModArrayOf<uint8_t>*` (0x180011220) | as 272 for 0x180025580 / `GetPreviewData` | as 272 | MEASURE |
| `:287` | src as `const ModArrayOf<uint8_t>*` | as 273 (Mod::bytes, PackageFile::_previewData) | as 273 | TYPE |
| `runtime/src/collection/avl_trees.cpp:268` | `void*` as `PtrArrayHdr*` in `PtrArrayDtor` (callers factory_dtors.cpp:165 / :209 hand `&TypeRecord::children`) | Startup 0x1800c06e3..0x1800c06fa builds the record on the stack (four zero qwords), Inserts it (0x1800c071f), ends its Array with TWO BARE ZERO STORES (0x1800c0728 / 0x1800c072d, the free elided) | `TypeRecord::children` becomes `Collection::Array<TypeRecord*>` ONLY if the inline ~Array at Startup's local comes out as the two stores (GCC must elide the `if (_data) MemFree` on a never-written local: B11); the node teardown in factory_dtors calls the member dtor once. Else: the function stays, re-tag with the new measurement | MEASURE |
| `runtime/src/collection/introsort.cpp:355` | `const void*` src as `const EmitterArray*` in `Array16Assign` (0x180050d30) | the 0x1800dfd10 caller (audio_scene_update.cpp:593) hands a bare zeroed int64[2]; retail builds a whole stack EffectSlot (0x1800dfcad..0x1800dfccc) and ends its positions with two bare zero stores after the call (0x1800dfd19 / 0x1800dfd1d) | a typed `EffectSlot` local at the caller (its `positions` array's inline ~Array = the two stores once the free is elided — B11); the source parameter typed `const EmitterArray&` | MEASURE |
| `runtime/src/graphics/wave24_lane_gfxmat.cpp:363` | `void*` field as `System::String*` in `SerString` (`MaterialTexture::name` is `char*`) | AddTexture 0x180103350 open-codes the pair's String (zero [RSP+0x28] 0x180103362, INC [RAX-4] 0x180103381, inline release 0x1801033a2..0x1801033c1); Draw 0x1800c2eb0 the same for "diffuse" (0x1800c31b4 / 0x1800c31cc / 0x1800c31e7..0x1800c3209) | `System::String MaterialTexture::name`; the element's array ops (array.cpp MaterialTextureAssign, saveload_draw's copies) become the template's; B11 AddTexture + Draw | MEASURE |
| `code/Game/Game/eventlog.cpp:286` | `uint8_t[0x28]` as `EventLogEntry*` (AddEventCore, the 0x140095030 family) | retail builds the entry on the stack: only the text is constructed (copy) and released right after the grow 0x140019b70, before the autoSlow tail | a typed `EventLogEntry` with a ctor taking the text (stores in retail's order: color, text, eventType, severity, tileIndex, iconIndex, popup) in a scoped block ending after the grow; `~String` inline = retail's release | BEHAVIOUR (the destruction point moves to a scope end that IS retail's point; say so) |

### A.2 Unit 2 — the ctor doors (CTOR block, 4 casts)

| file:line | what is cast | retail | fix | verdict |
|---|---|---|---|---|
| `code/Game/Game/exe_bodies.h:274` | `uint8_t rng[2520]` as `System::Random*` + `RtSystem_Random_Ctor` | 0x140099130: a 0x9c4-byte Random at a fixed frame offset, ctor 0x1800c7f20 (time-seeded), no dtor | `System::Random` gets `Random()` (0x1800c7f20) and `Random(uint32_t)` (0x1800c7ed0) as real ctors (Value.h's "trivially destructible" pin holds: retail has no ~Random); `System::Random random;` local. 2520 vs 2500: allocation size only. The seven other door callers (newgamedialog 841, noiseui 174, gameupdateloop_load 493 / 495 / 783, menudialogloopt_stubs 248, splash_loop_stubs 150) call the ctor; members built by the door inside a ctor become mem-initialisers at the retail position (B11 each ctor); the doors are deleted | TYPE (`B7:`), the member-position moves `behaviour:` if a store order changes |
| `code/Game/Game/game_main.cpp:334` | `EngineStorage{0x700}` memset 0 then `RtEngine_Engine_Ctor` | 0x1800dbf00: allocator ctor 0x1800c79a0 first, fifteen zero qwords +0x670..+0x6e8 skipping +0x698 (= handle), one 16-bit store at +0x6f0; NO memset; ~Engine 0x1800dbec0 at the end | `::Engine::Engine engine(hInstance);` (the ctor / dtor bodies move from engine_lifecycle.cpp's doors into the members; `Engine() = default` protected ctor goes); the memset is a store retail lacks and goes; the dtor at line 747's point (a scoped block if anything follows) | BEHAVIOUR (the memset; none observable: every field the ctor writes) |
| `code/Game/Game/minimap_paint.cpp:173` | `uint8_t[256]` as `Graphics::Renderer*` + door 0x180105180 | the Renderer on the stack; ~Renderer 0x1800180a0 right after ~Scene, after `context.Reset()` | a scoped block `{ Graphics::Renderer renderer; Graphics::Scene scene; … context.Reset(); }` — C++ destroys scene, then renderer = retail's order; the doors go with their last callers (gameupdateloop_load.cpp:1090's loader Scene read too) | TYPE (`B7:` if the block ends exactly at Reset) |
| `:175` | `uint8_t[256]` as `Graphics::Scene*` + door 0x180102730 | as above | as above | TYPE |

### A.3 Unit 3 — the zeroing ctors (CTOR block, 4 casts)

| file:line | what is cast | retail | fix | verdict |
|---|---|---|---|---|
| `code/Game/Game/gameupdateloop_load.cpp:574` | `uint8_t frustum[0xC0]` as `::Frustum*` to the door 0x1800fa1e0 | an untouched stack block handed to the perspective ctor, which writes planes[0..5] then isOrthographic | `Volume::Frustum` gets the real ctor `Frustum(const Scalar&, const Scalar&, const Scalar&, const Scalar&)` with the planes in the mem-initialiser (no zeroing first; store order B11 against 0x1800fa1e0); `::Frustum frustum(nearS, farS, fovXS, fovYS);` | TYPE |
| `:577` | `fr ? fr : frustum` as `const ::Frustum&` | the same object | goes with the door | TYPE |
| `:800` | `uint8_t frustum[0xC0] = {}` as `::Frustum*` | an untouched block (no zeroing) | the typed local; the `= {}` is a store retail lacks | BEHAVIOUR (none observable: the ctor writes every byte) |
| `code/Game/Game/gameupdateloop.cpp:1416` | `float[4]` row `pixel[4]` of `Context::PixelConstantStaging` as `Math::Vector4*` (also minimap_paint's row write) | one 16-byte MOVUPS into ctx+0x288; `Vector4()` 0x18001e0a0 zeroes (retail's, stays) | D6's ordered shapes: `Math::Vector4 pixel[5]` if Context's ctor B11s SAME (its memset subsumes the zeroing with no extra store at -O2); else a typed row accessor without a counted cast; else re-tag with the measurement | MEASURE |

### A.4 Unit 4 — the two leftovers and the two harness puns (LX block, 4 casts)

| file:line | what is cast | retail | fix | verdict |
|---|---|---|---|---|
| `runtime/src/collection/set_clear.cpp:210` | `RtIo::SettingsMap*` as `Collection::Set<Pair<String,String>>*` (`PairSet`) to call the template's `Clear` | 0x18000f7d0 IS that instantiation's Clear; ??0Persistence stores root (0x1800b8047) and count (0x1800b804a) only; ??1Persistence tail-JMPs 0x18000f7d0; Configuration zeroes its +0x10 header and clears by hand at 0x1800c518f | `Collection::Set<Collection::Pair<System::String, System::String>>` member in Persistence (+0x00) and Configuration (+0x10); `OwnedSettingsMap` / `SettingsMap` / `SettingsMapNode` / `rtsetclear::*` go; Map_Insert / Map_Find typed on the Set or shown to be `Set::Insert` / `Find` | BEHAVIOUR (H5: one commit per owner; the unit tests that pin the raw header rewritten) |
| `code/Game/Game/goals_serialize.cpp:86` | `WorkSchedulerTree*` as `uintptr_t*` to `WorkCellBST_Remove_ca60` | 0x14001ca60: one body (ICF) over five node / header types | the core becomes a template over the node type (as `AvlRebalance<>`), header-defined; the five `CAST-OK: elem_stride` wrappers and the erased core go; Insert / Serialize / BSTRemove_a6e0 likewise only if their bodies are layout-identical | TYPE |
| `code/Game/Component/citizenaicomponent.cpp:590` | the force-work building stored into `citizen->disease` (`Disease*` +0x68) | n/a (harness) | the store goes (B7_RULES D8a); readers of +0x68 under the harness retargeted to `home` | RULING |
| `code/Game/Component/clearareacomponent.cpp:191` | `uint8_t[sizeof(ClearAreaComponent)]` zeroed as `ClearAreaComponent*` in `ClearAreaComponent_HarnessSweep` | n/a (harness) | a real `ClearAreaComponent` (ctor / dtor read for the InternalFactory registration) with `_entity` set; else re-tag RULED: harness | RULING |

### A.5 Ruled, stay listed (3 — not this step's)

`code/Game/Player/tool_base_render.cpp:724` (the Type() probe), `runtime/src/achievement/achievement_state.cpp:217`
(const CreateInstance's `this` into a non-const ExternalPtr), `runtime/src/system/widestring.cpp:196`
(WideString shares `String::Release`; with D4 it is inline and the ruling may fall — the RT block reports).

## B. The residue rows (no cast), by block

| row | file | what | block |
|---|---|---|---|
| 82 | `tutorialgoalcount.cpp` FindOnEntity<T> null-entity return (0x1400df290) | SUSPECTED, read | RES |
| 83 | `tutorialsetuprestrictplayer.cpp` TutRestrict_ResolveRefOrLoad's FindOrLoad on miss (retail 0x14001b1d0 find-only) | SUSPECTED | RES |
| 84 | `goal_exe_helpers.cpp` NamedExternalRef::Resolve 0x1400265b0 load-on-miss + generation re-read | SUSPECTED | RES |
| 89 | `tutorialhighlightdialog.cpp` PlaceOnElement / PlaceOnEntity null tests | SUSPECTED | RES |
| 78 | `tool_base_highlight.cpp` AddDecal's byte store through `uint8_t*` vs `bool` fields (0x1400b0d80) | SUSPECTED; the field type is the question | RES |
| 98 | `town_storage_seed.cpp` null / IsDerivedFromNullSafe skips vs 0x140006270, 0x14012cd80 / c560 / cf10, 0x140107ff0, 0x140091660 | SUSPECTED | RES |
| 99 | `town_workplace_registry.cpp` FindComponentByTypeId + three continues (0x14008d100) | SUSPECTED | RES |
| 250 | `citizenai_findhelpers.cpp` / `citizenai_findtarget.cpp` null-tested scans (0x14008ec70 / 0x14008e370 / 0x14012ac30 / 0x14008dd30 / 0x1400178c0) | SUSPECTED | RES |
| 251 | `citizenai_gather.cpp` CooldownStamp / HideRenderWorkplaceMesh / FindRawMaterialChild (0x14010bc90 / 0x14010b3e0 / 0x14012ab00) | SUSPECTED | RES |
| 252 | `citizenai_executor.cpp` MoveTick tail / RegisterCitizenPickMesh (0x1401094c0, 0x140128f50) | SUSPECTED | RES |
| 291 | `citizenai_candidatelist.cpp` HasSufficientResources / 2 / CarryCount guards (0x1401156e0 / 0x1401157f0 / 0x140115de0) | SUSPECTED | RES |
| 330 | `citizen_reassign_workers.cpp` (0x1400d62a0 / 0x1400d6c60) returns and skips | SUSPECTED | RES |
| 310 G1 / G28 + 994 (0x140057b20) | `citizen_lifeevent_handler.cpp` and the files row 310 names | SUSPECTED / OWED | RES |
| 350 / 352 | `boatcomponent_route.cpp` / `boatcomponent_states.cpp` (0x1400456e0; 0x140044fda / 0x140045311) | SUSPECTED (the slot tests dropped; the null returns kept) | RES |
| 451 | `professionui.cpp` OnUpdate worker-slot walk (0x1400765a0) | SUSPECTED | RES |
| 443 | two ports of 0x140119470 (`Dialog/exe_bodies.h` fn_GetCurrentProductIndex, `Action/action_child_ctors2.cpp` PickConsumeRecipe) | one body, the guard goes, one port | RES |
| 692 | `edittool.cpp:213` pointer range test (0x1400fcc90) | SUSPECTED | RES |
| 920 | `mapcomponent.cpp` ctor's +0x30 / +0x34 stores (0x1401244d0) | CONFIRMED; the rotation readers' writers to show (D10) | RES |
| 921 | `workplacecomponent.cpp` SetAccepting's null StatusIconComponent return (0x14012fc8d-0x14012fcb5) | CONFIRMED; ruling D10 | RES |
| 922 | `controller_state_table.h` RequestStateTransition / `firecomponent_smstate.cpp` ForceSMState (0x14011e81c-0x14011e871) | CONFIRMED; ruling D10 | RES |
| 960 | `weather_particles.cpp` Configure's `if (material)` (0x1400d3f80's last store) | CONFIRMED; drop | RES |
| 962 | `pathfinder_seed.cpp` vs 0x14009e8c0 (runInline, the queued second command 4, Start before SetStepCost) | SUSPECTED; compare statement by statement | RES |
| G-bullets | the GatherRawMaterials site-ctor guards (`Work/gatherrawmaterials.cpp`), `Pf_RegionPairCacheTick_5d10`'s null tests (`Action/region_paircache.cpp`, Tick lines only) | SUSPECTED | RES |
| 992 | `professionlist.cpp:99-102` on the StringTable ExternalPtr instantiation (retail 0x1400a8e30 → 0x140018020) | OWED, FIRST | LX |
| 942 | `serialize_externals.cpp` IsDerivedFromOrTrust ignores its type | OWED, after 992 and every caller's instantiation | LX |
| 993 | IsDerivedFromNullSafe narrowings (rows 7, 8, 10, 40, 92; BuildRequirementUI / TownHallUI find templates) → member IsDerivedFrom | OWED, row 7 first | LX |
| 994 | G33 ToolUI deselect block, G30 StatusBarUI textSeason test (dialog parts) | OWED: read and rule | LX |
| 961 | the copied pathfinder race | nothing to do (watch the routes) | — |
| 5, entry 3, 34 | user decisions / a port owed | not this step | — |

## C. Counts

23 casts: RT 11, CTOR 8, LX 4 (2 + 2 harness). 3 ruled stay. Residue rows: RES 24 items, LX 4 rows.
Target at the close: PROD 26 → 3 (+ any residue re-tagged with a measurement, listed in LEFT_FREE §C).
