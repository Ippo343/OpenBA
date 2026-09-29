# Inventory E: the cast floor and the tests (B.4 block 0, NEXT_SESSION §3b.4 and §3b.7)

Table: `data/E_floor_and_tests.tsv` (one row per file, 967 files). Script:
`scripts/E_floor_and_tests.py` (run it from the repo root with no arguments; 3 s). All numbers count
**occurrences** (one per cast), not lines. §3a counts lines, so standalone's 1,509 lines are
1,638 occurrences here, and tests' 526 lines are 556.

## 1. The answer

- **The PROD floor is 1,117 casts of 45,761 (2.4 %), plus 27 in the dx11 seam that §4.1 removes.**
  Only 360 of the 1,117 are `reinterpret_cast`, so the `reinterpret_cast` floor is **360 of 28,901**.
  "Legitimate" was decided by what each cast does. Where a cast sits did not count.
- The floor has two tiers:
  - **Tier R, genuine reinterpretations: 278 casts in 74 files.** They are allocator block tags 74,
    pointer-as-integer uses 58, the crash handler / stack walk 33, Win32 parameters 27, float bits
    25, String refcount/length headers 24, the PE export walk 16, byte I/O 8, container element
    stride 5, mapped GPU memory 2, COM `void**` IID out-params 2, the CRT 2, hash 1, VSlot 1.
  - **Tier I, casts spelled as casts that are C++ idioms rather than reinterpretations: 839.**
    They are `(void*)p` passed as a `%p` argument 478, `new (static_cast<void*>(this)) T()` 283
    (retail's Construct/Reconstruct slots, return slots, container memory), and one
    `(T*)MemAlloc(...)` per allocation 78.
- **Everything the brief suggested as floor is much smaller than its directory:**
  - The serializer bodies are **field reads by offset**. Genuine byte I/O is 8 casts plus
    persistence's CRC/WriteFile.
  - `Collection::Array<T>` storage is 6 casts in `Collection.h`. The 305 in
    `runtime/src/collection/array.cpp` are element copy constructors written by offset, and they fall.
  - The dx11 COM seam has **10 floor casts of 151**.
  - The hardware watchpoint code is **11**.
- **standalone/ (1,966 occurrences, 1,638 of them `reinterpret_cast`):**

  | part | files | casts | floor | reinterpret_cast | deferred |
  |---|---:|---:|---:|---:|---:|
  | instruments | 38 | 1,727 | 197 | 1,519 | 0 |
  | boot glue | 4 | 135 | 51 | 59 | 6 |
  | retail game logic living in standalone/ | 4 | 104 | 34 | 60 | 0 |

  Most instrument casts are harnesses reading game objects by offset. They fall with those classes,
  or when the harness is retired.
- **tests/unit (1,144 casts, 556 of them `reinterpret_cast`):** about **944 are the test's subject
  and stay**: vtable/slot probes, layout probes, pointer identity, sentinels, raw fixtures, placement
  new. About **200 fall** when the product class gets members: minted retail-slot tables 94, `void*`
  handed to a C body 52, door thunks 11, function-pointer tables 12, POD shadows 9, and about 22
  read by hand. Counting `reinterpret_cast` only, **about 488 stay and about 68 fall**.

## 2. How the table was built

- `strip()` is imported from `b3r_session/codegrep.py`, so comments are stripped and strings kept.
  Files are opened with `newline=''` (see §5, item 11).
- The script scans the PROD directories plus `tests/unit`. It excludes `/build`, `/build-` and
  `.claude/worktrees`.
- **Spellings counted.** Each is counted once per occurrence:
  - `reinterpret_cast<T>(…)`, with its template argument and a balanced-paren operand.
  - `static_cast<void…|uintptr_t|intptr_t…>`, but not `static_cast<void (*)(…)>`.
  - `const_cast<…>`.
  - C-style `(T*)x`. The cast must not follow `sizeof`, `decltype` or `return`, and a `(K::*)`
    member-pointer type is skipped.
  - C-style `(uintptr_t)x` and `(intptr_t)x`. A cast of an integer literal is dropped, and so are
    `(size_t)` and `(uint64_t)`: those are arithmetic far more often than they are pointers.
  - F's compiler pass is the exact count. This script classifies; F's metric counts.
- **Classification.** Each occurrence gets one category from its target type, its operand text,
  the line, the statement so far and the text after it:
  - An offset in the operand or applied to the result (`(u8*)(p)+0x58`, `[kX]`) makes it
    `offset_read`, before any floor rule is tried.
  - Operand types come from a per-file declaration lookup: a `float`/`FILETIME` local, or a raw
    `uint64_t buf[2]` array.
  - A file is whole-file floor (`FILE_FLOOR`, 8 entries) only after I read every cast in it.
- **FLOOR categories:** `alloc_header`, `string_header`, `byte_io`, `hash`, `value_pun` (float bits,
  FILETIME), `com_iid`, `win32`, `gpu_mapped`, `container_storage`, `ptr_int`, `vtable_diag`,
  `crash_diag`, `pe_image`, `crt`, `sentinel_addr` (an integer in a user-data API slot),
  `placement_new`, `print_addr`, `alloc_result`.
- **DEFER:** `dx11_seam`. It stays through B.4 and falls in §4.1: `Video::*Data` self-casts,
  `Dx11OriginalSlot`, `Dx9P_*`, `dx11_seam.{h,cpp}`, and video_interface's Dx9Placeholder.
- **Categories that fall:**
  - `offset_read`: `p + 0x…`.
  - `void_receiver`: `(X*)self`.
  - `pod_shadow`: `RtX::Foo` ↔ `X::Foo`, and `String&` reinterpretations.
  - `storage_local`: placement new into a local byte array standing in for an object.
  - `type_confusion`: a Ghidra local of the wrong width.
  - `serialize_signedness`: `*(unsigned*)&field` into `Serialize`.
  - `struct_as_array`: `(float*)&vec`.
  - `untyped_com_field` / `untyped_handle_field`: an `ID3D11X*` or `HANDLE` kept in a `void*` or
    `int64_t` field.
  - `untyped_param`: an integer handed to a C body's `void*` parameter.
  - `fnptr`, `vtable_read` (product code reading a vtable), `null_literal` (`(int*)0x0`),
    `null_field_deref`, and `other`.
- **Tests:**
  - SUBJECT: `t_vtable_probe`, `t_layout_probe`, `t_identity`, `t_sentinel`, `t_fixture`,
    `t_placement_new`, `t_print`.
  - Falls: `t_minted_table`, `t_void_receiver`, `t_door_thunk`, `t_fnptr`, `t_pod_shadow`,
    `t_other` (read by hand, §3).
- **TSV columns.** The first five follow the brief:
  - `file casts_total legit_count category reason`.
  - Then `area` (code / runtime / dx11 / engine / `standalone:{instrument,boot_glue,game_logic}` /
    tests), the per-spelling counts `reinterpret cstyle static_void const_cast`, `deferred_4_1`,
    `falls`, and `falls_by_kind`.
  - For a test file, `legit_count` is the number of SUBJECT casts.
- **Flags.**
  - `--dump PATH` writes every occurrence (`file line spelling category T operand`).
  - `--summary` prints the per-category and per-area sums.

## 3. Counts

**PROD by area** (all spellings; the last column is the floor's categories):

| area | casts | floor | deferred | floor by category |
|---|---:|---:|---:|---|
| code | 38,432 | 672 | 0 | print_addr 301, placement_new 237, alloc_result 76, ptr_int 33, value_pun 23, gpu 1, byte_io 1 |
| runtime | 5,207 | 153 | 0 | placement_new 44, alloc_header 28, string_header 24, win32 18, ptr_int 16, byte_io 7, container 5, print 5, crt 2, value_pun 2, hash 1, vtable_diag 1 |
| standalone | 1,966 | 282 | 6 | print 168, alloc_header 46, crash_diag 33, pe_image 16, ptr_int 9, win32 8, alloc_result 2 |
| dx11 | 151 | 10 | 21 | print 4, com_iid 2, placement_new 2, gpu 1, win32 1 |
| engine | 5 | 0 | 0 | (CitizenBehavior.h accessors are offset reads) |

**Casts that fall, PROD, by kind:**

| kind | casts |
|---|---:|
| offset_read | 25,179 |
| other (mostly receivers and element pointers typed as `void*` / `uint8_t*` / `uintptr_t*`) | 14,169 |
| void_receiver | 2,340 |
| pod_shadow | 1,431 |
| storage_local | 377 |
| type_confusion | 348 |
| fnptr | 286 |
| vtable_read | 185 |
| serialize_signedness | 134 |
| null_literal | 42 |
| untyped_handle_field | 41 |
| untyped_com_field | 36 |
| untyped_param | 31 |
| struct_as_array | 16 |
| null_field_deref | 2 |

**The top 30 files by floor count** (the floor count, then the file's total):

| floor | total | file | floor | total | file |
|---:|---:|---|---:|---:|---|
| 80 | 133 | standalone/redzone_alloc.cpp | 5 | 5 | runtime/src/system/allocator_win.cpp |
| 35 | 119 | standalone/main.cpp (crash handler) | 5 | 24 | runtime/src/system/thread_win.cpp |
| 28 | 28 | runtime/src/system/allocator.cpp | 5 | 234 | code/Game/Action/action_pf_avl.cpp |
| 18 | 1003 | code/Game/Dialog/tradeui.cpp | 4 | 4 | runtime/types/system/String.h |
| 18 | 456 | standalone/tool_harness.cpp | 4 | 4 | runtime/src/collection/introsort_msvc.h |
| 17 | 915 | code/Game/Dialog/townhallui.cpp | 4 | 31 | runtime/src/audio/audio.cpp |
| 16 | 16 | standalone/sampler.cpp | 4 | 91 | runtime/src/graphics/wave27_lane_gfxbig.cpp |
| 13 | 265 | code/Game/Game/goals_serialize.cpp | 4 | 51 | runtime/src/collection/avl_trees.cpp |
| 11 | 11 | standalone/hw_watch.cpp | 4 | 136 | code/Game/Entity/componententity.cpp |
| 11 | 372 | code/Game/Goal/statvaluedispatch.cpp | 3 | 40 | dx11/src/devicedata.cpp |
| 9 | 71 | code/Game/Game/menuscene.cpp | 3 | 9 | runtime/src/collection/set_avl_pathid.cpp |
| 9 | 9 | standalone/loader_loop_stubs.cpp | 3 | 14 | runtime/src/io/filesystem_createfile_win.cpp |
| 8 | 351 | code/Game/Dialog/professionui.cpp | 3 | 4 | runtime/src/audio/audio_stream_win.cpp |
| 6 | 18 | runtime/types/collection/Collection.h | 2 | 2 | runtime/src/crt/msvc_stricmp.cpp |
| 6 | 132 | code/Game/Component/citizenai_actionctors.cpp (float bits) | 1 | 1 | runtime/types/object/VSlot.h |

**Only 14 files are 100 % floor:**
- runtime: allocator.cpp, allocator_win.cpp, crt/msvc_stricmp.cpp, types/object/VSlot.h,
  types/system/String.h, collection/introsort_msvc.h, system/compression.cpp.
- standalone: hw_watch.cpp, sampler.cpp, alloc_stats.cpp, loader_loop_stubs.cpp,
  perf_terrain.cpp.
- dx11: exports.cpp and funcs/dx11_seam.cpp (these two hold 91 floor casts between them).

The other 1,026 floor casts sit in **329 files that also hold casts that fall**.

**standalone/, split** (the table's `area` column):
- **Boot glue.** main.cpp, video_interface.cpp, boot_window.cpp and alloc_cutover_arm.cpp carry
  casts. options.cpp, runtime_init.cpp, frame_limiter.cpp, engine_loader_gate.cpp and
  res_load_lock.cpp carry none.
  - main.cpp's floor is its crash handler and prints.
  - main.cpp still reads `Video::Configuration` at +0x04..+0x42 and `Video::Device` at
    +0x48..+0x100 by offset. Those reads fall.
- **Retail game logic that lives in standalone/.** Retail compiled these; they belong in
  `code/Game`:
  - `menudialogloopt_stubs.cpp` (MenuDialogLoopT, vtable 0x14015ed00)
  - `splash_loop_stubs.cpp` (the DialogLoaderLoopT subclass, 0x140158800)
  - `loader_loop_stubs.cpp` (the two LoaderLoop subclasses)
  - `chunked_scene_loader.cpp` (a non-retail split of MenuDLT::Load PhaseB)
- **Instruments: everything else (38 files).**
  - The genuine floor is `redzone_alloc` block walks, `sampler`, `hw_watch`, `alloc_stats`, and
    prints.
  - The rest (1,530) are harnesses reading game classes by offset. The five largest:

    | file | casts | floor |
    |---|---:|---:|
    | tool_harness.cpp | 456 | 18 |
    | food_panel_harness.cpp | 124 | 6 |
    | death_harness.cpp | 96 | 10 |
    | scene_watch.cpp | 90 | 1 |
    | stress_harness.cpp | 88 | 3 |

**tests/unit** (1,144 casts):

| category | casts | reinterpret_cast | stays? |
|---|---:|---:|---|
| t_vtable_probe | 319 | 247 | stays |
| t_identity | 238 | 12 | stays |
| t_placement_new | 104 | 0 | stays |
| t_layout_probe | 93 | 92 | stays |
| t_sentinel | 81 | 38 | stays |
| t_print | 32 | 32 | stays |
| t_fixture | 20 | 20 | stays |
| t_minted_table | 94 | 12 | falls |
| t_void_receiver | 52 | 5 | falls |
| t_fnptr | 12 | 12 | falls |
| t_door_thunk | 11 | 11 | falls |
| t_pod_shadow | 9 | 9 | falls |
| t_other | 79 | 66 | read by hand, below |

The 79 `t_other` casts, read by hand:
- **About 57 stay.** Retslot-order ABI calls (`GccOrder`/`MsOrder`), `uintptr_t(0x5000)` functional
  sentinels, address-identity checks, the String `[-1]`/`[-2]` header in string_test, vtable
  patch/restore in object_vtable_abi, list_test int↔void* elements, and raw fixture buffers.
- **About 22 fall.**
  - Seven `reinterpret_cast<System::Thread*>(&pod)` in system_abi_test: POD shadows that go when
    the POD goes.
  - Six `(float*)&vec` in interpolatedstate: these become `&v.x`.
  - io_vtable_abi 448/1086/2030 and b39_io_classes 213/214: test types reinterpreted as `IO::` types.
  - rb_engine 146.
  - set_test's `const_cast` of a literal.

Falls by test file:

| file | falls | what falls |
|---|---:|---|
| io_vtable_abi_test.cpp | 96 | 80 of them minted `(void*)&Unused, // +0x80` tables |
| ui_leaf_c2 | 16 | |
| io_streams | 11 | |
| system_abi | 10 | |
| object_vtable_abi | 9 | |
| achievement_goal_abi | 8 | ABI probes; really stay |

## 4. Blind spots

- **The script has no types.**
  - Operand types are guessed from names plus a per-file declaration search.
  - `ptr_int` includes about 10 pointer comparisons inside raw BST nodes (for example
    statvaluedispatch.cpp:870). Those fall once the node is typed.
  - `value_pun` includes about 3 writes of integer 0 into a float slot (action_pf_roots_road.cpp:190).
  - `alloc_result` (76 in code/Game) stays as ONE cast per allocation. When a node type gets
    retail's `operator new`, even that cast goes.
- **Macros and templates are counted once, at their definition.** Examples are `dx11_seam.h`'s
  row macro, `Collection.h`, and the ui_leaf test macros.
- **Functional casts are not counted.** `uintptr_t(x)` is one; so is `T*` through a typedef.
  - `memcpy` / union puns are not counted. VSlot.h's PMF→words `memcpy` is one; F counts it.
  - Other C-style integer casts are not counted either. `(uint64_t)ptr` is an example.
- **Multi-line casts.** A cast whose operand spans lines is classified from the text that follows,
  up to 160 characters. A printf argument on a continuation line is found through the statement
  text back to the previous `;`.
- **Hand checks.** 180 occurrences were checked by hand, plus the 79 `t_other` casts.
  - Sample A: 45 floor casts on the first pass had **10 errors**. The misses were a local byte
    array used as placement storage, `(void*)1` passed to a C body's `void*` parameter, and a float
    slot zeroed as an int.
  - Rules were added for all three. The next samples are the error rates after those fixes:
    - Sample B, 45 floor casts: **2 errors**. One was an int64 from `MemAlloc`; the other was
      `redzone_alloc`'s read of the allocator singleton.
    - Sample C, 45 non-floor casts in runtime, standalone and dx11: **2 errors**. One was a
      multi-line `%p` print; the other was a dx11 seam macro.
    - The rules were fixed again after B and C.
  - Sample D (final), 45 mixed casts: **1 wrong** on floor versus falls (statvaluedispatch:870) and
    1 wrong only in its label (a redzone module offset marked `alloc_header` where `crash_diag` is
    right).
  - **Final error rate: about 2 %, both directions.** `code/Game`'s `other` bucket (14k) was sampled
    but not read in full; it contains no floor pattern that the samples found.
- **Line numbers.** Some files contain lone `\r`. `codegrep.py` opens files in text mode, so its line
  numbers drift from the editor's in those files: citizenai_actionctors.cpp is 1 low. The script
  here opens files with `newline=''`.

## 5. What surprised me (where the code contradicts NEXT_SESSION §3a/§3b.4, ENDGAME B.4 or a comment)

1. **"Byte-level serializer reads" is almost empty.**
   - `*_serialize.cpp` in code/Game (gameupdateloop_serialize 81 cast lines, citizencomponent 33,
     animalwave 37 and others) read FIELDS by offset (`Serialize(*(float*)At(this,0x3c))`), cast a
     `uint8_t ser[]` buffer to `IO::Serializer*`, and construct it with placement new.
   - All of that falls.
   - What stays is persistence's CRC over blocks, `WriteFile`/`RegQueryValueEx` byte buffers, and 8
     `memcpy`/`memset`.
2. **134 `serialize_signedness` casts** (`*reinterpret_cast<unsigned int*>(&_field40)`,
   "0x98 = uint door"; toolbar.cpp, value_types.cpp, saveload_meta.cpp and others).
   - They fall only if the field is declared with the type retail serializes it as.
   - That is a signedness decision (§3d; known_port_issues 5), so **C/D must say, per field, what
     retail's type is**.
   - Where retail's field is `int` and its code calls the `uint` overload, the pun is retail's own
     and moves to tier R.
3. **`Collection::Array<T>` storage is not the 540 casts in runtime/src/collection.**
   - `array.cpp`'s 305 casts are per-instantiation element init/copy bodies that write element
     fields by offset (`d + 0x14`). They fall when the element types are named.
   - `avl_trees.cpp` reads node links by offset (`node + 0x10`). Those fall too.
   - What stays: Collection.h's element stride and placement new (6), plus String refcounts inside
     the sets (7).
4. **The dx11 "COM seam" is 10 genuine casts of 151.**
   - 36 are `reinterpret_cast<ID3D11X**>(&self->field)`. They exist only because
     `dx11/types/VideoDX11Data.h` declares COM fields `void*`. A forward-declared `struct ID3D11Buffer;`
     removes them with no `<d3d11.h>` in the header.
   - 27 are the §4.1 seam.
   - 23 are offset reads (`+0x100` ActiveDeviceData, `Video::Configuration+0x28`).
   - ENDGAME B.4's "zero outside COM interop in dx11/" would wave all 151 through; **the acceptance
     must name dx11 by file and category, not by directory.**
5. **HANDLEs.**
   - 41 `reinterpret_cast<HANDLE>(file->winHandle)` exist because `RtIO::FileInfo::winHandle` is
     declared `int64_t`. Retail's field is a HANDLE; declared `void*`, the casts disappear.
   - `HKEY*` from `&_key` is the same case.
6. **The hardware-watchpoint code is 11 casts.**
   - standalone's 1,509 `reinterpret_cast` lines are 93 % harnesses reading game classes by raw
     offset: tool_harness 456, food_panel 124, death 96, scene_watch 90, stress 88.
   - They are callers. They fall with those classes (or with the harness).
7. **Four retail classes' bodies live in standalone/:** MenuDialogLoopT, the splash
   DialogLoaderLoopT, and the two LoaderLoops. `chunked_scene_loader` is a non-retail restructuring of
   one of them.
8. **Tier I is larger than tier R.**
   - `%p` prints (478, mostly `(void*)this` in game-code TRACE/printf), `static_cast<void*>(this)`
     in every `*Description::Construct/Reconstruct` (237), and `(T*)MemAlloc` (78) are all spelled
     as casts.
   - `-Wold-style-cast` and F's `static_void` column count them.
   - Without a shape exemption, **the floor is 1,117, not 278.**
9. **Two faithful null-receiver reads are spelled as absolute addresses:**
   `*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(0x50))` at work_act_consume.cpp:268 and
   `(…0x28)` at :302.
   - Retail reads through RCX=0 there. Hard rule: this is not to be "fixed".
   - When the field is named, the read becomes a null `foundComp1->field` read and must stay a read.
10. **42 `(int*)0x0` / `(void*)0x0` Ghidra null literals** are counted by `-Wold-style-cast`. Each
    falls to `nullptr`.
11. **`codegrep.py` opens files with universal newlines.** A file with a lone `\r` shifts every later
    line number, so a line quoted from `codegrep.py` output can point at the wrong line
    (citizenai_actionctors.cpp).
12. **31 `untyped_param` casts.** Integers are handed to C bodies declared with a `void*` parameter:
    `Pf_EnqueueCmd1_a6a30(…, reinterpret_cast<void*>(1), …)` (param_3 is an int in retail), and
    `Pf_EnqueueCmd2`. The fix is the exe_bodies.h signature (A's table), not the call site.

## 6. Proposal: floor, markers, and how it splits

**The acceptance number** is a sum of three parts, and each part has its own check:
- **Tier R: 278 casts, marked inline:** `// CAST-OK: <category>`, one of the 14 tier-R names
  above.
  - The acceptance counts markers per category and requires each category to be ≤ the number in
    this table.
  - It also re-runs this classifier on each marked line. A marker on a line with an operand offset
    (`+ 0x…`) or a `self` receiver fails.
  - Cost: 278 one-line edits in 74 files. 155 of them are in 13 files that belong to no W1 class
    block (allocator, crt, VSlot, String, hw_watch, sampler, redzone, main's crash handler,
    thread_win, audio_stream_win), so those markers conflict with nothing.
- **Tier I: 839 casts, exempted by SHAPE in `castmetric.py` (F), not by marker.** The three shapes:
  - `new (static_cast<void*>(X))`, where X is `this`, a return slot, or a non-array pointer.
  - A `void*` / `uintptr_t` cast that is an argument of a printf-family call.
  - `static_cast<T*>(MemAlloc|Allocate…(…))`.
  - A marker on 839 sites would be noise. The shapes are exact.
- **The file allowlist is only for the 14 files that are 100 % floor** (91 casts), and markers make
  even that optional.
  - An allowlist of all 343 floor files would hide the 21,000+ casts that fall in those same files.
    tradeui.cpp alone holds 18 floor casts and 985 that fall. That is why I recommend markers over
    a file list.
- **Deferred: 27 dx11 seam casts**, named by file (`dx11/src/*data.cpp` self-casts,
  `dx11/funcs/dx11_seam.*`, `standalone/video_interface.cpp`). They carry `// CAST-OK: dx11_seam`
  until §4.1.

**Blocks (disjoint file sets):**
- **E-0, with W0 (headers first), one owner.**
  - Add the tier-R markers in the 13 files that no W1 class block owns: runtime/src/system/{allocator,
    allocator_win, thread_win}, runtime/src/crt/*, runtime/types/object/VSlot.h,
    runtime/types/system/String.h, runtime/src/collection/introsort_msvc.h, standalone/{hw_watch,
    sampler, redzone_alloc, alloc_stats, main} and runtime/src/audio/audio_stream_win.cpp.
    That is 155 casts. No behaviour.
  - W0 must first decide on the typing changes that remove untyped fields. They are **header
    changes, so W0 owns them:**
    - `RtIO::FileInfo::winHandle` → `void*` (41 casts).
    - dx11 COM fields → forward-declared interfaces (36).
    - The `serialize_signedness` fields (134): retail type per field, from D/C. **This one can
      change behaviour:** a field that becomes `unsigned` changes every signed compare or divide
      that reads it.
- **W1 class blocks.** Each W1 block also marks the tier-R casts in its own files: the other 61 of
  the 74 files, 123 casts. Its hand-in states the file's floor from this TSV; after the block, the
  file's count must equal that floor.
- **standalone/.**
  - The 4 game-logic files move to `code/Game/Game/` inside the block that owns MenuDialogLoopT and
    the LoaderLoops. The move itself is behaviour-neutral.
  - Each instrument file goes with the class it reads most:
    - tool_harness → the Player tools block.
    - food_* / orchard_* / pasture / cropfield → Component.
    - scene_watch → Graphics::Scene.
    - redzone_alloc → System::MemoryAllocator.
  - Otherwise the coordinator retires it. A harness for a finished wave is 1,530 casts that
    nobody needs to convert.
- **tests/unit.** The 200 test casts that fall belong to the block that owns the product class:
  - io_vtable_abi_test (96, the minted tables) falls with `runtime/src/io/serializer_class.cpp`'s
    retail-slot `vtable_read` of the Encoding factory, which is itself one of the 185
    `vtable_read`s.
  - ui_leaf_* → UI.
  - system_abi_test's POD shadows → System.

**Names that change behaviour if done carelessly:**
- The serialize-signedness fields (above).
- The two null-receiver reads (§5, item 9).
- `ptr_int`'s bad-pointer guards: `(uintptr_t)comp < 0x10000` at townhallui.cpp:1274 is a
  non-retail guard. It is recorded here, not endorsed.
