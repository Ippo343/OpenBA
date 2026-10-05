# The cast floor (B.4): what stays, how it is marked, how it is counted

Written by block E0 (W0), 2026-09-28. Rules: `../B4_RULES.md` M1-M5. Source: inventory E
(`INVENTORY_E_FLOOR_AND_TESTS.md`, `data/E_floor_and_tests.tsv`). Measure: `castmetric.py`.
Acceptance: `acceptance.sh` lines 01-03 read the ceiling table in §1 of this file.

A cast stays in the tree only in one of two tiers. Everything else is removed by naming the thing
(M5), and until it is, it counts in `castmetric.py`'s `total`.

- **Tier R** (M2): a genuine reinterpretation. It carries a trailing `// CAST-OK: <category>` on
  its line and goes to the `cast_ok` column, not to `total`.
- **Tier I** (M3): one of exactly three SHAPES. `castmetric.py` recognises them by shape, with no
  marker, and puts them in the `tier_i` column, not in `total`.

For every file, `(the metric before E0) == total + tier_i + cast_ok`. At E0's base (main
`319bce30`, no marker yet) that was **46,733 = 46,404 + 329 + 0**; after E0's markers it is
**46,733 = 46,222 + 329 + 182** (`data/METRIC_BASE_E0.tsv`).

At the B.4 close (branch `b4z-MARKERS`, after the audit's rulings): `castmetric.py code runtime
standalone dx11 engine --floor` reports **cast_ok 735 (681 lines), tier_i 451, BAD 0, STRAY 0**,
and the tree metric is **502** (code runtime standalone engine: 262; dx11/ is the next step's
scope).

## Changes at the B.4 close

The independent audit (`AUDIT.md`, read at `7230f96bb`; every ruling ACCEPTED by the coordinator)
re-cut this file. Part A: `elem_stride` RE-DEFINED as the type-erased container / tree core; four
NEW categories (`const_api`, `user_data`, `null_receiver`, `static_init`); the tier-I allocation
shape grown to every allocator that returns fresh untyped storage (§3); the `this` / `self` bar
lifted for `elem_stride` views as `ArrayHdr` only (§4); `null_receiver` amends §5. Part C: the five
tree-only vptr guards marked `crash_walk` / `vslot` lost their markers (DEFERRED to the
retail-difference task); `tool_base_highlight.cpp`'s five uint32-parameter stores lost `float_bits`
(Ghidra typed retail's float parameters as integers: WORK, B6); six harness / log reads of a vtable
word (`force_work.cpp`, `game_main.cpp`) were re-filed `vslot` -> `crash_walk`; the two malloc lines
marked `win32` / `alloc_header` (`perf_terrain.cpp`, `linkedlist.cpp`) are tier I now. Every
ceiling in §1 is the count MEASURED after those markers, not an estimate.

## 1. The ceiling, by category

`acceptance.sh` line 02 fails when a category's `cast_ok` exceeds its ceiling. Since the B.4 close
each ceiling is the MEASURED count (`castmetric.py code runtime standalone dx11 engine --floor`,
"cast_ok per category", casts not lines). Raising one needs the coordinator, with the lines; a new
category needs the coordinator (M2).

| category | ceiling | lines | what the cast does | change at the B.4 close |
|---|---:|---:|---|---|
| `alloc_header` | 64 | 61 | reads / writes an allocator block's boundary tag, free-list link, footer, canary or range page arithmetic: memory the allocator owns as raw bytes | 65 -> 64: `linkedlist.cpp`'s `malloc` line is tier I (§3) |
| `ptr_int` | 133 | 124 | a pointer used as an integer: unsigned pointer compares, byte distance between two addresses, pointer bits grafted, a pointer printed / returned as an integer | as measured; +`founding_spawn.cpp`'s log key, +`userinterface_showdialog.cpp`'s defect-2 control (the parent pointer's low dword as the result) |
| `crash_walk` | 128 | 110 | crash / hang / watchpoint diagnostics: RIP and RSP from a CONTEXT, return addresses as module offsets, raw stack words, readable-memory probes, raw memory dumps, harness / log reads of a vtable word | as measured; -4 tree-only guards (part C), +6 re-filed from `vslot`, +4 `tool_base_render.cpp` tripwire (`block`), +1 `stress_harness.cpp` profiler key |
| `win32` | 85 | 83 | a Win32 / CRT API's own untyped parameter or result: `LPVOID` thread parameters, `LPARAM` message structs, `BYTE*` registry buffers, `intptr_t` OS handles, a CRITICAL_SECTION kept as opaque bytes because the type header cannot include `<windows.h>` | as measured; `perf_terrain.cpp`'s `malloc` is tier I |
| `float_bits` | 12 | 10 | a float read or written as its bit pattern | 25 -> 12 measured: `citizencomponent.cpp`'s fabs mask (2) and `tool_base_highlight_tess.cpp`'s by-value Vector2 packing (10, which the audit calls QUESTIONABLE); the five `tool_base_highlight.cpp` stores left it |
| `string_header` | 37 | 35 | the `System::String` `{length, refCount}` header at `data - 8` | as measured |
| `pe_walk` | 12 | 11 | walks a loaded module's PE headers and export directory | unchanged |
| `byte_io` | 38 | 36 | a byte buffer handed to / read from a stream, CRC or file API | as measured |
| `elem_stride` | 172 | 157 | the ONE ported type-erased container / tree core (definition below) | RE-DEFINED by the audit (A); 110 -> 172 |
| `gpu_map` | 0 | 0 | memory returned by a GPU buffer map | nothing marked (dx11/ is the next step) |
| `com_iid` | 2 | 2 | the `void**` out-parameter of a COM `QueryInterface` / `GetBuffer` by IID | unchanged |
| `crt` | 11 | 11 | the ported CRT's own `void*` / `char*` parameters (qsort and its comparators' `const void*`, `_stricmp`) | as measured |
| `hash` | 5 | 5 | a pointer hashed as an integer | unchanged |
| `vslot` | 2 | 2 | `Object::SlotAddressOf` (`VSlot.h`), the one sanctioned look into a compiler vtable, and `splash_loop_stubs.cpp`'s retail vptr identity compare (GameUpdateLoop has no type id) | 8 -> 2: six log / harness reads re-filed `crash_walk`, `buildrequirementui.cpp`'s guard unmarked |
| `dx11_seam` | 20 | 20 | deferred: the VideoDX11 seam, removed by plan §4.1 (`CompanionData.h`'s 12 `DeletingDtor` arms, `Context.h`, `video_interface.cpp`) | 39 -> 20 measured: dx11/ itself carries no marker yet |
| `const_api` | 3 | 3 | NEW: the const a retail Runtime API puts on its result (`PEBV`), dropped ONCE in the typed wrapper that mirrors retail's inlined template (`External.h` `FindAs` / `FindNextAs` / `FindOrLoadAs`); never at a call site | new |
| `user_data` | 8 | 8 | NEW: a `void*` that a RETAIL engine API (signature `PEAX`) stores for its caller and hands back unchanged, or a message payload whose type the message id selects, cast back to the type the registration stored. Not for a tree-only API, nor for a `void*` a template could carry | new |
| `null_receiver` | 2 | 2 | NEW: retail's faithful read through a null receiver, spelled as the faulting address (`work_act_consume.cpp`, §5) | new |
| `static_init` | 1 | 1 | NEW: the view of a file-static byte table that emulates MSVC's guarded function-local static (`wave29_lane_slots.cpp` `g_identityTable`: zero `.data`, filled by the first Update; a typed array would be constant-initialised to identity) | new |
| **total** | **735** | **681** | | |

**`elem_stride`, as re-defined at the B.4 close.** The one ported type-erased container / tree
core: its view of a container's header, element storage or node handed to it untyped; its
spec-row element callbacks (`void* dst / src`); and a typed wrapper's single hand-off of a typed
container into it.
- QUALIFIES: inside `array.cpp` / `array_destroy.cpp`, `Collection.h`, `array_graphics_anim.cpp`,
  the avl / set cores (`set_avl.h`'s `ElementOf`), `work_base_bst.cpp` / `work_base_cellbst.cpp` /
  `work_base_reimpl.cpp`, the one-body erase / load / buy-entries cores (`cemeterycomponent.cpp`,
  `componentdescription.cpp`, `tradeui_buyentries.cpp`), the ExternalPtr serialize core
  (`Game/exe_bodies.h`, `serialize_externals.h`), spec-row element ops, and typed-wrapper hand-off
  lines (`Work/exe_bodies.h`, `imagebuffer_platform_serialize.cpp`, `mainloop_video.cpp`).
- Does NOT qualify: a CALLER outside the core casting its own raw triple or member to a core type
  (H5: it falls with the class); `coll_bridge.h` (an offset accessor that dies with its last
  caller); any `+ 0x…` operand; a field write by offset inside an element op; a stack byte image
  viewed as a record to skip its ctor (`wave9_lane_gfxd.cpp` `AddScene`'s `raw`).
- Macro bodies: a cast on a `#define` continuation line (ending in `\`) cannot carry a `//`
  marker; 16 such lines in `array.cpp` / `array_destroy.cpp` are floor by the ruling but stay in
  `total` until the tool reads a marker there or the entries are written as functions.

E's own names map as: `crash_diag` → `crash_walk`, `pe_image` → `pe_walk`, `value_pun` →
`float_bits`, `container_storage` → `elem_stride`, `gpu_mapped` → `gpu_map`, `vtable_diag` →
`vslot`. E's three tier-I categories (`placement_new`, `print_addr`, `alloc_result`) are shapes,
not markers (§3).

## 2. The floor, by file

HISTORICAL: this table is E0's estimate at the start of B.4 and is not maintained. The measure is
`castmetric.py <dirs> --floor` ("cast_ok per file and category").

Measured: `castmetric.py <file> --floor` after E0. Estimate: inventory E's tier-R occurrences,
mapped to the categories above; the W1 / W2 block that owns the file marks it and replaces the
estimate with its measured number in its hand-in.

| file | casts by category | source |
|---|---|---|
| `runtime/src/audio/audio_stream_win.cpp` | win32 3 | marked (E0) |
| `runtime/src/collection/introsort_msvc.h` | ptr_int 4 | marked (E0) |
| `runtime/src/crt/msvc_qsort.cpp` | crt 3 | marked (E0) |
| `runtime/src/crt/msvc_stricmp.cpp` | crt 2 | marked (E0) |
| `runtime/src/system/allocator.cpp` | alloc_header 36, hash 1, ptr_int 2 | marked (E0) |
| `runtime/src/system/allocator_win.cpp` | win32 5 | marked (E0) |
| `runtime/src/system/thread_win.cpp` | win32 2 | marked (E0) |
| `runtime/types/object/VSlot.h` | vslot 1 | marked (E0) |
| `runtime/types/system/String.h` | string_header 4 | marked (E0) |
| `standalone/alloc_stats.cpp` | hash 1, win32 2 | marked (E0) |
| `standalone/hw_watch.cpp` | crash_walk 10, win32 1 | marked (E0) |
| `standalone/main.cpp` (crash handler, stack walkers) | crash_walk 30, win32 2 | marked (E0) |
| `standalone/main.cpp` (the rest: registry guard, memory probe, window hook) | crash_walk 3, win32 9 | E0 estimate, for the STANDALONE block: :127 :213 :219 :229 `BYTE*` registry buffers, :1076 :1078 (2) `HBRUSH` / `LONG_PTR`, :1088 :1105 `LPARAM` structs (win32); :172 :173 the VirtualQuery probe, :809 `(LPCSTR)vtbl` module lookup (crash_walk) |
| `standalone/redzone_alloc.cpp` | alloc_header 26, crash_walk 26, ptr_int 4, win32 2 | marked (E0) |
| `standalone/sampler.cpp` | crash_walk 3, pe_walk 12 | marked (E0) |
| `code/Game/Action/action_pf_road.cpp` | float_bits 1 | E estimate |
| `code/Game/Action/action_pf_roots_misc.cpp` | float_bits 2 | E estimate |
| `code/Game/Action/action_pf_roots_road.cpp` | float_bits 1 | E estimate |
| `code/Game/Component/citizenai_actionctors.cpp` | float_bits 6 | E estimate |
| `code/Game/Component/citizencomponent.cpp` | float_bits 1 | E estimate |
| `code/Game/Component/decalcomponent.cpp` | float_bits 4 | E estimate |
| `code/Game/Component/destroycomponent_workerrelease.cpp` | ptr_int 1 | E estimate |
| `code/Game/Component/storagedisplaycomponent.cpp` | float_bits 1 | E estimate |
| `code/Game/Component/terraformcomponent.cpp` | gpu_map 1 | E estimate |
| `code/Game/Component/tornadocomponent.cpp` | float_bits 3 | E estimate |
| `code/Game/Dialog/destroyui.cpp` | ptr_int 1 | E estimate |
| `code/Game/Dialog/optionsdialog.cpp` | ptr_int 4 | E estimate |
| `code/Game/Dialog/professionui.cpp` | ptr_int 6 | E estimate |
| `code/Game/Dialog/townhallui.cpp` | ptr_int 2 | E estimate |
| `code/Game/Game/gameupdateloop.cpp` | float_bits 1 | E estimate |
| `code/Game/Game/menuscene.cpp` | ptr_int 4 | E estimate |
| `code/Game/Game/water_contour_mesh.cpp` | byte_io 1 | E estimate |
| `code/Game/Game/worldsetup_reload.cpp` | ptr_int 1 | E estimate |
| `code/Game/Goal/statvaluedispatch.cpp` | ptr_int 8 | E estimate |
| `code/Game/Player/fencedtool_calcpath.cpp` | float_bits 1 | E estimate |
| `code/Game/Player/tool_base_highlight.cpp` | float_bits 1, ptr_int 1 | E estimate |
| `code/Game/Player/tool_base_highlight_tess.cpp` | ptr_int 1 | E estimate |
| `code/Game/Player/tool_base_path.cpp` | ptr_int 3 | E estimate |
| `code/Game/Work/plantarea.cpp` | float_bits 1 | E estimate |
| `code/Game/Work/work_act_hunt.cpp` | ptr_int 1 | E estimate |
| `dx11/funcs/dx11_seam.h` | dx11_seam 1 | E estimate |
| `dx11/src/buffers.cpp` | dx11_seam 3, gpu_map 1 | E estimate |
| `dx11/src/contextdata.cpp` | com_iid 1, dx11_seam 2 | E estimate |
| `dx11/src/devicedata.cpp` | com_iid 1, dx11_seam 6, win32 1 | E estimate |
| `dx11/src/imagebufferdata.cpp` | dx11_seam 1 | E estimate |
| `dx11/src/renderstatedata.cpp` | dx11_seam 3 | E estimate |
| `dx11/src/shaderprograms.cpp` | dx11_seam 2 | E estimate |
| `dx11/src/smallclasses.cpp` | dx11_seam 2 | E estimate |
| `dx11/src/vertexlayoutdata.cpp` | dx11_seam 1 | E estimate |
| `runtime/src/application/application_win.cpp` | ptr_int 1 | E estimate |
| `runtime/src/audio/audio.cpp` | byte_io 4 | E estimate |
| `runtime/src/audio/audio_device.cpp` | win32 1 | E estimate |
| `runtime/src/audio/vtbatch7b_stream_run_win.cpp` | win32 1 | E estimate |
| `runtime/src/collection/array_graphics_mesh.cpp` | string_header 2 | E estimate |
| `runtime/src/collection/avl_trees.cpp` | elem_stride 2, string_header 2 | E estimate |
| `runtime/src/collection/introsort.cpp` | ptr_int 2 | E estimate |
| `runtime/src/collection/set_avl_pathid.cpp` | string_header 3 | E estimate |
| `runtime/src/engine/engine_platform_win.cpp` | win32 1 | E estimate |
| `runtime/src/graphics/animationstate_transforms.cpp` | float_bits 1 | E estimate |
| `runtime/src/graphics/graphicsmesh_vtable.cpp` | string_header 1 | E estimate |
| `runtime/src/graphics/renderer.cpp` | byte_io 1 | E estimate |
| `runtime/src/graphics/wave27_lane_gfxbig.cpp` | ptr_int 4 | E estimate |
| `runtime/src/io/closure_batch2b_win.cpp` | string_header 2 | E estimate |
| `runtime/src/io/file_stream.cpp` | string_header 2 | E estimate |
| `runtime/src/io/filesystem_createfile_win.cpp` | string_header 2 | E estimate |
| `runtime/src/io/filesystem_timestamp_win.cpp` | float_bits 1, ptr_int 1 | E estimate |
| `runtime/src/io/modification_setupfilesystem.cpp` | string_header 1 | E estimate |
| `runtime/src/io/persistence_registry_win.cpp` | win32 2 | E estimate |
| `runtime/src/io/persistence_worker_win.cpp` | hash 1 | E estimate |
| `runtime/src/system/compression.cpp` | ptr_int 2 | E estimate |
| `runtime/src/system/string.cpp` | string_header 1 | E estimate |
| `runtime/src/system/widestring.cpp` | string_header 2 | E estimate |
| `runtime/src/ui/spritecapture.cpp` | string_header 2 | E estimate |
| `runtime/src/video/context.cpp` | byte_io 1 | E estimate |
| `runtime/src/video/context_lifecycle.cpp` | byte_io 1 | E estimate |
| `runtime/src/video/video_mode_sort.cpp` | ptr_int 2 | E estimate |
| `runtime/types/collection/Collection.h` | elem_stride 3 | E estimate |
| `standalone/boot_state_watchdog.cpp` | win32 1 | E estimate |
| `standalone/perf_experimental.cpp` | ptr_int 5 | E estimate |
| `standalone/renderdoc_integration.cpp` | win32 1 | E estimate |
| `standalone/scene_watch.cpp` | ptr_int 1 | E estimate |
| `standalone/stress_harness.cpp` | ptr_int 1 | E estimate |
| `standalone/video_interface.cpp` | dx11_seam 6 | E estimate |

## 3. Tier I: the three shapes (no marker)

`castmetric.py` exempts a COUNTED cast by its shape alone (`tier_i_shape()`); `--detail` prints
it as `tier_i:<shape>`. Nothing else is exempt by shape. A shape whose operand holds a `+ 0x…`
offset is NOT exempt: `new ((void*)(s + 0x30)) T` constructs a field by offset and falls with
the field's name.

1. **`placement_new`**: `new (static_cast<void*>(x)) T(...)`, `::new (static_cast<void*>(x))`,
   `new ((void*)x) T(...)`: the cast is the whole placement argument.
   `code/Game/Entity/componententity.cpp:258`:
   `new ((void*)ibpAddr) ::Object::InternalBasePtr(nullptr);`
   (`static_cast<void*>(x)` of a typed `x` is not counted by the metric at all; the shape
   matters for `(void*)x` of an integer or an unknown.)
2. **`print_arg`**: a `void*` cast (any spelling: `(void*)`, `static_cast<void*>`,
   `reinterpret_cast<void*>`, const or not) that is a WHOLE argument of a printf-family or
   TRACE-style call: `printf fprintf sprintf snprintf _snprintf sprintf_s snprintf_s wprintf
   fwprintf swprintf _snwprintf wsprintfA wsprintfW`, the variadic loggers `RZOut RZBoth CrashLogf
   HangLogf` and `System::String::Format FormatIndexed`, and the variadic macros `TRACE DLOG MO_LOG
   REPO_DIAG CEM_TRACE TORNADO_TRACE` (`PRINTF_FAMILY` in `castmetric.py`; the list was read from
   every callee a `"%p"` format reaches in PROD). `standalone/main.cpp:513`:
   `CrashLogf("VCALL-of-0: this=%p vtable=%p\n", target, (void*)vt);`
   The value printed may itself be read by offset: `(void*)*reinterpret_cast<int64_t*>(p + 0x68)`
   exempts the outer `(void*)` only; the inner read counts.
3. **`alloc_result`**: `static_cast<T*>(alloc(…))` and `(T*)alloc(…)`, the call being the WHOLE
   operand (qualified or through `->` / `::` / `.`), where `alloc` is one of the callees of
   `castmetric.py`'s `_RE_ALLOC_CALL` (grown at the B.4 close, AUDIT.md A `alloc_shape`):
   `(?:Rt)?MemAlloc\w*` (`MemAlloc`, `MemAllocS`, `RtMemAlloc`, …), `Allocate\w*`,
   `InternalMemAlloc`, `TypeChildAlloc`, `\w*OperatorNew` (a class's ported `operator new`),
   `operator new`, `malloc`, `realloc`, `calloc`.
   `runtime/src/application/message_callbacks.cpp:130`:
   `static_cast<Node*>(MemAlloc(0x30, 8));`
   The proof is the same for every callee: it returns fresh, untyped storage and the cast gives a
   type to new memory; no existing object is reinterpreted. A cast of a `void* p` LOCAL that holds
   the result is not the shape (put the call in the operand). `reinterpret_cast<T*>(VirtualAlloc(…))`
   is not this shape (a Win32 result: `win32`).

At E0: `print_arg` 222, `alloc_result` 106, `placement_new` 1: **329** counted casts. At the B.4
close (all PROD dirs): `print_arg` 38, `alloc_result` 413, `placement_new` 0: **451**.

## 4. How to add a marker

1. Run `castmetric.py <file> --detail`. Every line it prints with a spelling column
   (`reinterpret`, `cstyle_ptr`, `static_void`, …) is a counted cast.
2. Read the cast and decide what it DOES (M2). Where it sits does not matter: a crash handler's
   read of a class field by offset is an offset read, not `crash_walk`.
3. Append `  // CAST-OK: <category>` to the END of the line the cast STARTS on (the line
   `--detail` names; for a cast split over two lines, the first). One marker covers every counted
   cast on its line, so a line with a genuine cast and a falling one is split first or left
   unmarked. A line that already ends in a `//` comment gets the marker after it
   (`// footer  // CAST-OK: alloc_header`); a marker in a `/* */` comment or a string is not
   read. Append only: the marker never moves or reflows code (W0 blocks share files).
4. Run `castmetric.py <file> --floor`. The line must not appear under `BAD MARKER` or
   `STRAY MARKER`; the category's `cast_ok` must stay at or under §1's ceiling.

A **BAD MARKER** is a marked line where one of its counted casts has an operand holding a
`+ 0x…` offset, has `self` or `this` as its whole operand, is an offset-accessor call, or where
the category is not in §1. Its casts stay in `total`. A **STRAY MARKER** is a marked line with no
counted cast (a cast removed later, or a marker on the wrong line of a multi-line statement).

**The one exception to the `this` / `self` bar (B.4 close, AUDIT.md A).** For `elem_stride` ONLY,
a cast whose whole operand is `this` / `self` is accepted when the line views it as the erased
core's header type `rtcoll::ArrayHdr` (= `Collection::Array<uint8_t>`): `Collection.h`'s
`Array<T>` members handing their own header to `ArrayAssignCore` / `SetSizeCore` /
`ClearCallback`, and `array_destroy.cpp`'s entries viewing their `void* self` receiver. It is
safe because the operand is the container's OWN header, layout-identical to the target by
`static_assert`, and no base class can express it: the core is `Array<uint8_t>`, a sibling
instantiation, not a base. `castmetric.py`'s `bad_operand` keys the exception on the category and
on `ArrayHdr` appearing on the line; any other `self` operand (an element op whose `void* dst`
parameter is named `self`, a caller passing its receiver as an integer) stays BAD.

## 5. What is NOT a floor cast

These look like the floor and are not; they fall with the class, field or signature that names
them (inventory E §5, and what E0 found reading its 13 files):

- **Serializer bodies** read FIELDS by offset (`Serialize(*(float*)At(this, 0x3c))`), cast a
  `uint8_t ser[]` buffer to `IO::Serializer*` and placement-construct it. Only persistence's CRC,
  `WriteFile` / `RegQueryValueEx` buffers and a few byte copies are `byte_io`.
- **`*reinterpret_cast<unsigned*>(&field)` into `Serialize`** (134, `serialize_signedness`): falls
  when the field has retail's type (a signedness decision, H8). Only where retail's field is
  `int` AND retail calls the `uint` overload is the pun retail's own, and then it is `byte_io`.
- **`runtime/src/collection/array.cpp`** (305): element copy-constructors writing element fields
  by offset. **`avl_trees.cpp`** node links by offset. Only `Collection.h`'s stride (and String
  refcounts inside the sets) stay.
- **dx11 COM fields kept as `void*`** (36 `reinterpret_cast<ID3D11X**>(&self->field)`): a
  forward-declared interface type removes them. Of dx11's 151 casts, 10 are floor and 27 seam.
- **HANDLEs in integer fields**: 41 `reinterpret_cast<HANDLE>(file->winHandle)` because
  `RtIO::FileInfo::winHandle` is `int64_t`; `thread_win.cpp:267-337`'s
  `reinterpret_cast<HANDLE>(_hThread / _hStart / _hStop / _hFinished)` the same (the fields are
  `void*`, which IS `HANDLE`: once they are declared `HANDLE` the casts are no-ops to delete).
- **`self` / `this` receivers**: `reinterpret_cast<LPCRITICAL_SECTION>(this)` in
  `thread_win.cpp:98 :99 :106 :242 :247` is `System::CriticalSection` reading its own storage; it
  falls when the class declares its CRITICAL_SECTION storage as a named member (B12).
- **An integer handed to a C body's `void*` parameter** (`untyped_param`, 31): the fix is the
  declaration's signature, not the call.
- **`(int*)0x0` / `(void*)0x0`** Ghidra null literals: `nullptr`.
- **Product code reading a vtable by index** (`vtable_read`): `thread_win.cpp:330-335`
  (ThreadFunc's two slot calls, kept raw deliberately until the Thread hierarchy is typed) and the
  factory self-test / swap-chain reads in `standalone/main.cpp`.
- **POD shadows** (`RtX::Foo` ↔ `X::Foo`, `rtcoll::ArrayHdr*` of a real `Array`, as at
  `audio_stream_win.cpp:322`).
- **Offset reads inside instruments**: `redzone_alloc.cpp`'s reads of `MemoryAllocator` fields
  through `reinterpret_cast<uint8_t*>(System::MemoryAllocator::_singleton) + 0x00 / 0x08 / 0x28 /
  0x640` and of `MemoryRange` at `+0x00 / +0x08` (44 casts) are field reads: `_singleton->ranges`,
  `->rangeCount`, `->freeLists[b]`, `->lock`. `main.cpp`'s `Video::Configuration` / `Video::Device`
  reads at `+0x04..+0x100` likewise.
- **The two faithful null-receiver reads** in `work_act_consume.cpp`
  (`*reinterpret_cast<uint32_t*>(0x50)`, `*reinterpret_cast<ConsumeProduceRecipe**>(0x28)`): AMENDED
  at the B.4 close (AUDIT.md A, `null_receiver`). They do NOT become a named field read through a
  null pointer: a typed `nullptr->field` is undefined behaviour that GCC rewrites into a trap
  (`-fisolate-erroneous-paths`), not retail's access violation at the field's address. They stay
  spelled as the faulting address, marked `null_receiver` (ceiling 2), and stay a read (hard rule:
  no null check).
- **Casts that are no reinterpretation at all** but that `castmetric.py` counts (its operand
  types are inferred, not compiled): identity casts between `HANDLE` and `void*`
  (`hw_watch.cpp:56`, `sampler.cpp:197`), `(uintptr_t)ctx.Rip` of a `DWORD64` (`sampler.cpp:133`),
  and integer-to-integer `(int64_t)` casts whose member name is a pointer elsewhere in the tree
  (`allocator.cpp:1017 :1018 :1020 :1021`, `main.cpp:718`). They are deleted, not marked.
- `ptr_int` estimates include about 10 pointer compares inside raw BST nodes
  (`statvaluedispatch.cpp:870`): those fall once the node is typed. `float_bits` estimates include
  about 3 writes of integer 0 into a float slot (`action_pf_roots_road.cpp:190`).
