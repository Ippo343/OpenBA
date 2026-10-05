# B.4 close: the independent read-only audit

Read at main `7230f96bb` (wave Y round 3 closed, metric 838). Main moved during the read: `acceptance.sh`, run a
few minutes after the per-line read, saw `cast_ok` 668 against the 667 the read used. Every row is keyed by file +
snippet. Inputs: `castmetric.py code runtime standalone engine --detail` / `--floor` (dx11/ excluded), the
`// LEFT:` tag on or above each line, `data/pass2y/out/*` (units and verdicts). The per-line table is
`data/audit/lines.tsv` (598 rows, header first). Nothing was built, run or edited outside these two files.

## Totals: counted now, outside dx11/

| proposed disposition | rows | what happens to them |
|---|---:|---|
| CAST-OK (a marker to add, existing or new category) | 94 | elem_stride 75 (erased core), user_data 8 (new), crash_walk 6, null_receiver 2 (new), ptr_int 1, vslot 1, static_init 1 (new) |
| TIER-I (grown `alloc_result` shape) | 35 | malloc / realloc 14, class operator new 8, RtMemAlloc 7, InternalMemAlloc 5, TypeChildAlloc 1 |
| WORK (a doable unit, still to execute) | 287 | 48 const_find (one typed wrapper), 20 H8, 174 inventory "doable" units still on the tree, 45 new or re-cut by this audit |
| STALE (the type exists: convert by name) | 4 | array.cpp:2009-2011 (RendererDrawGroup `_items[k]`), external_load.cpp:313 (static_cast to External) |
| DEFERRED, retail-difference task | 53 | known_port_issues 14 rows (and three tree-only divergences) |
| DEFERRED, dx11 seam step | 5 | boot_window.cpp:49, main.cpp:880 / :952 (x2 each) |
| DEFERRED, named prerequisite | 120 | RtSystem::String / Filename mirrors 19+3+3, RtGraphics ctor/dtor doors 21, H5 raw-storage classes (Mod, PathStackEntry, MeshInstance::_animation, EventLog, CharacterGrid, Minimap, Frustum ...), coll_bridge 7, bare-T* InternalPtr 7, WideString slots 7, Thread slots 4 |
| **total** | **598** | |

So the floor after the rulings is: today's 667 markers (re-cut below) + 94 new markers + 35 tier-I lines, and the
`total` that remains is the 178 DEFERRED rows plus whatever of the 291 WORK / STALE rows is not executed.

## A. Rulings, one per class

**const_find (48: FindOrLoad / Find / FindNext results held non-const).** Not a reinterpretation, and not floor as it
is now: 48 call sites each spell the same const drop. Retail's API returns `const External*` (mangled `PEBV`) and
retail's slots hold non-const `T*` (`ExternalPtr<T>`): retail itself dropped the const in ONE inlined template. Ruling:
the typed-wrapper rule (user decision 2) applies. Add `ExternalFactory::FindOrLoadAs<T>` / `FindAs<T>` / `FindNextAs<T>`
(downcast + const drop in one place). Each wrapper's single `const_cast` carries a NEW category **`const_api`**: "the
const a retail Runtime API puts on its result (`PEBV`), dropped once in the typed wrapper that mirrors retail's inlined
template". It qualifies only inside those wrappers, and never at a call site. Ceiling 3. No §4 relaxation. The 48 lines
become WORK: call the wrapper. This includes debugui.cpp's factory walk (339 / 491 / 629) and the FindNext walks
(game.cpp:140, professionlist.cpp:151, mainloop_video.cpp:489, modification_setupfilesystem.cpp:439).

**retail_diff:\<row\> (53 rows; the inventory counted 50).** NOT floor. They stay in `total` as DEFERRED to the
retail-difference task (known_port_issues 13-14). Three tree-only divergences tagged `retail_diff:none` join them:
splash_loop_stubs.cpp:265 / menudialogloopt_stubs.cpp:507 (the standalone first-tick clamp) and citizenlist_serialize.cpp:233
(the version-5 clamp). gameupdateloop_serialize.cpp:230 (the int overload where retail calls the uint one) is also a
retail difference, not H8. One row has no number: citizencomponent.cpp:2416 (tagged row 705) is not in the inventory's
row list, so check that row 705 exists.

**alloc_shape:\<allocator\> (38).** Grow the tier-I shape. In `_RE_ALLOC_CALL`, the callee name becomes
`(?:Rt)?MemAlloc\w*|Allocate\w*|InternalMemAlloc|TypeChildAlloc|\w*OperatorNew|operator\s+new|malloc|realloc|calloc`
(qualified or through `->` / `::`, the call still the WHOLE operand). The proof is the same as for `alloc_result`: the
callee returns fresh, untyped storage, and the cast gives a type to new memory. No existing object is reinterpreted.
`RtMemAlloc` fails today only because the regex anchors on `MemAlloc`. Not covered: sunlight.cpp:198 and
compression.cpp:469 cast a `void* p` local (WORK: put the call in the operand). clearareacomponent.cpp:193 is a harness
that builds a zeroed component in a stack buffer without its ctor (WORK: construct it, or retire the harness). That
leaves 35 TIER-I rows. perf_terrain.cpp:25 (a malloc marked `win32`) also moves to the shape.

**h8 (21 tagged; 20 here + gameupdateloop_serialize.cpp:230 above).** NOT a floor class. Split three ways:
(a) 12 bool puns (`*reinterpret_cast<bool*>(&uint8field)` into the bool overload, raw-byte stores into bool fields):
WORK. Retail's overload is bool, so declare the field AND its parameter chain `bool`; a bool-to-bool copy is a raw
byte copy. B11 must show that no `!= 0` normalisation was added. Only where B11 shows one does the site keep the pun, as
`byte_io`, with that evidence.
(b) 7 signedness puns (componententity.cpp:531, value_types.cpp:227, elementdescription_serialize.cpp:158,
statemachinecomponent.cpp:528-531): WORK. Read retail's field type. FLOOR §5 already rules that retail int + uint
overload means `byte_io`; otherwise retype the field.
(c) string.cpp:976 (a byte buffer viewed as UTF-16): WORK. Allocate it as `uint16_t*` (tier-I).
The same ruling covers four lines ALREADY marked `byte_io` with the bool-slot reason (wave27_lane_gfxpart.cpp:258,
package_file_serialize.cpp:158, internal.cpp:228, numberdescription.cpp:109). Today identical puns are marked in one
file and LEFT in another.

**erased_self + ERASED core views + macro (75 rows).** A floor category is warranted, and it is `elem_stride` RE-DEFINED,
not a new name. The coordinator already raised `elem_stride` 5 -> 18 for adapters and hand-offs, and 110 casts now carry
it, mostly views of the erased core (Collection.h:461 `&src` as ArrayHdr, Work/exe_bodies.h's wrapper hand-offs,
external_set.h's node elements). New definition: "**the one ported type-erased container / tree core: its view of a
container's header, element storage or node handed to it untyped; its spec-row element callbacks (`void* dst / src`);
and a typed wrapper's single hand-off of a typed container into it**".
It qualifies inside `array.cpp` / `array_destroy.cpp` (macros included), `Collection.h`, `array_graphics_anim.cpp`, the
avl / set cores, `work_base_bst.cpp` / `work_base_cellbst.cpp` / `work_base_reimpl.cpp`, the one-body erase / load /
buy-entries cores (cemeterycomponent.cpp:148-157, componentdescription.cpp:66, tradeui_buyentries.cpp:137), the
ExternalPtr serialize core (exe_bodies.h:180, serialize_externals.h:38), spec-row element ops, and typed-wrapper
hand-off lines.
It does NOT qualify: a CALLER outside the core casting its own raw triple or member to a core type (H5: falls with the
class), `coll_bridge.h` (an offset accessor that dies with its last caller), any `+ 0x…` operand, or a field write by
offset inside an element op (wave9_lane_gfxd.cpp:351).
§4 relaxation: the `this` / `self` bar is lifted for this category only, and only where the cast target is
`rtcoll::ArrayHdr` (= `Collection::Array<uint8_t>`) or the core's own header type. That is safe because the operand is the
container's own header, layout-identical to the target by `static_assert`, and a base class cannot express it: the core
is `Array<uint8_t>`, a sibling instantiation, not a base. The tool needs the exception keyed on (category, target type),
not a blanket lift. Measured: 110 + 75 = 185, plus about 6 wrapper hand-offs that the WORK rows below create. Ceiling 191.
Callers that are NOT the core and stay WORK: four WorkCellBST callers (action_base_complete.cpp:104,
citizenai_work_scheduler.cpp:301 / :609 / :642: a typed wrapper per header type), eight rtsetavl node views
(application_win, platform_startup_win, input_platform_shutdown_win, device_startup, filesystem_createfile_win:597,
filesystem_packages:654: one typed element accessor), keymap_save.cpp:308 (FIELD), scene.cpp:594 / element.cpp:671
(call an Array member), and worldgen_f390_ranges.cpp:231 (unify the twin element type).
Also not floor: storagecomponent.cpp:135 (DEFERRED with StorageComponent's H5 arrays), and Internal.h:363 / :372
(DEFERRED: InternalPtr<T> is instantiated over non-Internal T).
scene_watch.cpp:582-584 is filed `erased_self` but is an offset read of ModelComponent fields from its embedded
MeshInstance (`g + 0x13c`): WORK, not the core.

**vptr_probe (14).** Split, with no new category.
- thread_win.cpp:285-290 (4): DEFERRED until System::Thread's slots are typed. FLOOR §5 already says product-code
  vtable reads are not floor.
- splash_loop_stubs.cpp:428: `vslot`, widened by one line. It is retail's own vptr identity compare, and GameUpdateLoop
  has no type id, so a slot would change its vtable.
- tool_base_render.cpp:636 / 642 / 661 / 662 / 723 (the recycled-block tripwire): `crash_walk`, after the integer
  `self` is renamed `block`. It is not a receiver view, and FLOOR §4's bar on `self` is about receivers.
- stress_harness.cpp:566 (profiler key): `crash_walk`. tool_harness.cpp:368 is marked that way already.
- cropfield_force_start.cpp:91 / 94: WORK. Test `Object::SlotAddressOf(node, &Controller::PostEvent)` and call it
  typed, as buildrequirementui.cpp:85 does.
- pickup.cpp:514: DEFERRED to the retail-difference task / NEXT_SESSION §0.2. It is a port guard retail lacks.

**callback_ctx (9).** A NEW category is warranted: **`user_data`**, "a `void*` that a retail engine API (signature
`PEAX`) stores for its caller and hands back unchanged, or a message payload whose type the message id selects, cast
back to the type the matching registration stored". It qualifies only where the retail signature is `void*` and the
target is the registered type. It does NOT qualify for a tree-only API, or for a `void*` that a template could carry.
Ceiling 8: citizen_needsupdate.cpp:147, edittool.cpp:585 / 759, pathtool.cpp:143, wave29_lane_avbody.cpp:325,
userinterface_standalone.cpp:125 / 171, modal_driver.cpp:227. No §4 relaxation.
titledesctooltipui.cpp:53 is not this. Its slot is `Object::Base*`, so it is a downcast waiting on the unported producer:
DEFERRED.

**const_slot (3).** NOT floor; WORK. Declare `_generation` `mutable` (imagebuffer_derived.cpp:617,
wave23_lane_gfxbody.cpp:620): this is codegen-neutral, and retail's const slot writes the field. Declare the instance's
`_achievement` `const Achievement*` if nothing writes through it (achievement_state.cpp:217). All three are `this`
operands, which §4 bars anyway.

**dx11_seam (5).** DEFERRED to the seam step (plan §4.1). The `+ 0x18` swap-chain reads in main.cpp bar a marker, so
they stay in `total` and do not take a `dx11_seam` marker.

**Singles.**
- null_receiver (work_act_consume.cpp:264 / 298): a NEW category, **`null_receiver`**, ceiling 2. "Retail's faithful
  read through a null receiver, spelled as the faulting address." A typed `nullptr->field` is UB that GCC rewrites into
  a trap (-fisolate-erroneous-paths), which is not retail's access violation. Amend FLOOR §5, whose text says these
  "become a NAMED field read".
- slot_pun (gatherrawmaterials.cpp:362): DEFERRED to the retail-difference task. It is a retail quirk: record it in
  retail_bugs.md, and pass what the callee reads in a `behaviour:` commit.
- timeOutSlot (keystone_a_removalcalcpath.cpp:298): WORK. Read whether the int written through `outState` is ever read
  back as the Time; if not, split it into two typed locals.
- g_identityTable (wave29_lane_slots.cpp:124): a NEW category, **`static_init`**, ceiling 1. It is the view of a
  file-static byte table that emulates MSVC's guarded function-local static (zero `.data`, filled by the first Update).
  A typed array would be constant-initialised to identity.
- `_items[pass]` (wave9_lane_gfx.cpp:399 / 400, `+ 0x1c0`): WORK. Index `group->_items[pass]` by name; every caller
  passes 0..2, and B11 proves the codegen.

## B. Per line: `data/audit/lines.tsv`

These rows matter most:
- The **STALE** rows: array.cpp:2009-2011 and external_load.cpp:313.
- The audit's own **WORK** groups:
  - the BaseTypeInfo factory function typing (7 lines: type_reflection.cpp:209 / 213, main.cpp:663,
    dialog_create.cpp:449, exe_type.h:99 / 100, forester_force_harness.cpp:185). `factoryFn` stays `void*` only so that
    main.cpp can print it, and a `(void*)` print argument is tier-I.
  - `SortByKey70(IO::PackageFile**, int)` (3 lines).
  - the dead-argument parameters (tornadocomponent.cpp:502 / 538, citizenai_findhelpers.cpp:479: declare the ignored
    parameter's type so that no cast is needed).
  - founding_spawn.cpp:321 is `ptr_int` (a log key), not a SIG.
- An inventory mis-tag: menudialogloopt.cpp:552 is listed `RULING:const_find` but is the Renderer ctor door (POD).

## C. The existing markers, re-cut from a read of every `CAST-OK` line

`--floor`: 667 casts on 614 lines, BAD 0, STRAY 0. The accessor definitions outside dx11 are `Arr` and `I32`
(gameupdateloop_teardown.cpp: WORK rows) and `ArrayAt` (coll_bridge.h: DEFERRED). `CanaryPtr` is a tier-R helper.

| category | measured | FLOOR §1 ceiling | proposed ceiling | read |
|---|---:|---:|---:|---|
| alloc_header | 65 | 65 | 65 | allocator.cpp 30, redzone 28, linkedlist 4 lines: all boundary tags / canaries |
| ptr_int | 131 | 62 | 129 | -3 register-garbage returns typed as integers (gatherrawmaterials.cpp:449 `_present._data & ~0xFF` is a bool in AL; work_base_walkpos.cpp:156; tool_base_highlight_tess.cpp:304): WORK, B6 bool return / parameter; +1 founding_spawn.cpp:321. About 30 lines are combo-box item data (`(void*)(intptr_t)i`), which is correct |
| crash_walk | 121 | 86 | 129 | -4 WRONG (below); +6 from vslot (force_work.cpp:99 / 102 / 104, game_main.cpp:568 / 596 / 680 are harness / log reads of the vtable word); +6 from vptr_probe |
| win32 | 87 | 56 | 86 | -1 WRONG: perf_terrain.cpp:25 is a CRT malloc result (tier-I under the grown shape). audio_play_platform.cpp:338 (a tree record viewed as `IXAudio2VoiceCallback*`) needs a second read |
| float_bits | 17 | 25 | 12 | -5 WRONG: tool_base_highlight.cpp:269-273 store uint32 parameters into a float[5]; the parameters are retail floats that Ghidra typed `uint` (B6: type them float). tool_base_highlight_tess.cpp:178-196 (10 casts: Ghidra CONCAT packing float pairs into qwords for by-value Vector2s) is QUESTIONABLE, the same kind of finding. The genuine one is citizencomponent.cpp:1149 / 1150 (the fabs mask) |
| string_header | 37 | 24 | 37 | Correct by act. 21 of them are inlined String release / adopt over `void*` String members (constant_buffer.cpp 7, device_enum_types.cpp 5, context_lifecycle.cpp 4, renderer.cpp 2, text_codec.cpp:496, toolbardialog.cpp:591, gameupdateloop_serialize.cpp:680) and fall with those classes' H5 String members. They are kept because retail's inlined spellings differ by an instruction (device_enum_types "spelling B") |
| pe_walk | 12 | 12 | 12 | sampler.cpp |
| byte_io | 42 | 17 | 42 | streams / compression / crc / transcoders / settings magic are correct; the 4 bool-slot and 3 signedness lines are under the H8 ruling (A) |
| elem_stride | 110 | 18 | 191 | re-defined as the erased core (A). array_destroy.cpp:230 / 231 `slot + 8` zero stores are inside the core (correct) |
| com_iid | 2 | 2 | 2 | |
| crt | 11 | 9 | 11 | +2 qsort comparators (residencecomponent.cpp, work_leaf_clear.cpp, saveload_saveinfo.cpp: qsort's own `const void*`) |
| hash | 5 | 5 | 5 | |
| vslot | 8 | 1 | 2 | VSlot.h:38 + splash_loop_stubs.cpp:428. -6 to crash_walk; -1 WRONG (below) |
| dx11_seam | 19 (+ dx11/) | 39 | 39 | CompanionData.h 12, Context.h:325, video_interface.cpp 6 |
| gpu_map | 0 | 2 | 2 | |
| new: const_api / user_data / null_receiver / static_init | - | - | 3 / 8 / 2 / 1 | (A) |

**WRONG markers.** In each, the cast is a guard that retail lacks; FLOOR §4.2 says it falls with the guard
(retail-difference task / NEXT_SESSION §0.2), so these are not diagnostics:
- destroycomponent_states.cpp:211 `*reinterpret_cast<void* const*>(node)`, crash_walk ("the tree's vptr guard", row 391)
- statusiconcomponent.cpp:63 `*reinterpret_cast<void* const*>(&c->_geometry)`, crash_walk (STANDALONE GUARD D4)
- townhallui.cpp:1087 `void** vtable = *(void***)comp`, crash_walk (vtable-null guard)
- playanimation.cpp:83 `void* vt = *reinterpret_cast<void* const*>(target)`, crash_walk ([PlayAnimGuard] aborts the action)
- buildrequirementui.cpp:84 `void** vtable = *(void***)comp`, vslot (the "standalone sanity probe")

The ptr_int lines correctly classed by act but belonging to the same guard list: townhallui.cpp:1083,
entitylist.cpp:833 / 844, destroycomponent_workerrelease.cpp:98, edittool.cpp:213, and the `OkAddr` / `SaneHeapPtr`
probes in citizenui.cpp, residenceui.cpp, buildrequirementui.cpp:61 and followdialog.cpp. They leave with the guards;
list them in the retail-difference task's inventory.

`crash_walk` growth: componententity.cpp:450-500 is a trace that scans a serializer stream by raw offsets inside product
code. The marker is right; the trace is a deletion candidate. entity_comp_validator.cpp (13) and scene_watch.cpp (6) are
diagnostics, and they are correct.

## D. Acceptance scripts (both read; both read-only greps, both run)

`b3r_session/acceptance.sh`, R1-R16: registry / resolver / mint names (R1-R5b, R8-R13), the deleted files (R6),
`engine/funcs` empty (R7). R14-R16 grep a new-game log, need the stepclose log directory, and are skipped without it.
**At target: 0 off.** It needs the new-game log at the close for R14-R16, and R16 pins `calls=5683` (NEXT_SESSION §1
also says 5683).

`b4_session/acceptance.sh`: **10 of 12 off.**
- 01: PROD total = 814, including dx11's 216, against a target of 0. It cannot pass at B.4's close, because dx11/ is
  the next step's scope. Measure PROD minus dx11/ for B.4, or close B.4 with dx11 as a recorded exception.
- 02: 8 categories are over their ceilings. FLOOR §1 needs the re-cut above, and the four new categories added to both
  `CATEGORIES` and the FLOOR table, or 02 reads them as ceiling 0.
- 03: passes.
- 04: 8 accessor definitions: Arr, I32, ArrayAt, and dx11's Field x2, FieldPtr, IbI32, IbU32.
- 05: 20 accessor calls.
- 06: pun 2. One is wave9_lane_gfxd.cpp:351 (WORK); the other is in dx11.
- 07: 362 `B.3.x bridge` comment lines. These are comments, not casts: a mechanical sweep of the stale bridge comments.
- 08 / 09: 424 / 125, because `LEFT_FREE.md` does not exist yet (it belongs to the closing wave).
- 10: 1243 unasserted fields (a lower bound).
- 11: 57 Rt<Ns> mirrors.
- 12: passes.

The doors acceptance (b3r) is at target. The by-address launcher lookup is gone: R9 / R10 count 0.

## E. The three items NEXT_SESSION §5 records "for the audit"

- **WeatherSimulation's array zero stores precede the PerlinNoise ctor** (retail's follow it): NOT in
  `known_port_issues.md`. It is only in GATE_TABLE row 115 (`33df7a1a`). Record it as a 14-row (an order difference,
  value-identical) in a free row 705-709, and leave it for the retail-difference task.
- **The Set ctor writes its pad dword**: recorded, rows 640 (Work leaves), 651 (EntityList +0x74 / +0x84) and 652
  (CitizenBehavior +0x10C / +0x16C). Disposition: retail-difference task. Give `Collection::Set` a ctor that leaves
  `_pad0c` unwritten, in one `behaviour:` commit, after checking every reader that assumed it zero.
- **`ClearStorage::Execute` passes the wrong component** (row 643) and **`HuntComponent::Update` reads the pointer bits
  as ticks** (row 511): both recorded. They are real defects: DEFERRED to the retail-difference task, as `behaviour:`
  fixes. huntcomponent.cpp:63 is the one counted cast of the two.

## Not read / limits

- The 399 tier-I lines were not re-read (the shapes are mechanical).
- The 174 inventory "doable" rows carry the inventory's verdict. This audit only re-read their tags; it did not re-derive
  them from retail. Re-derive them from retail before executing.
- No retail binary was consulted. The H8 field types and the timeOutSlot cross-read are left to the executor, as the
  rulings say.
