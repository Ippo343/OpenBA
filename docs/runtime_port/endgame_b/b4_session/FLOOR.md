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

## 1. The ceiling, by category

`acceptance.sh` line 02 fails when a category's `cast_ok` exceeds its ceiling. The ceiling is
E0's MEASURED count in the 13 files it marked, plus inventory E's ESTIMATE for the files the W1 /
W2 blocks will mark (E counted occurrences with its own classifier; `castmetric.py` counts per
spelling, so a W1 block may measure a little more or less). A block that needs more than the
ceiling says so in its hand-in with the lines; the coordinator raises the number here. A new
category needs the coordinator (M2).

| category | ceiling | marked by E0 | still to mark (estimate) | what the cast does |
|---|---:|---:|---:|---|
| `alloc_header` | 62 | 62 | 0 | reads / writes an allocator block's boundary tag, free-list link, footer, canary or range page arithmetic: memory the allocator owns as raw bytes |
| `ptr_int` | 62 | 10 | 52 | a pointer used as an integer: unsigned pointer compares, byte distance between two addresses, pointer bits grafted |
| `crash_walk` | 72 | 69 | 3 | crash / hang / watchpoint diagnostics: RIP and RSP from a CONTEXT, return addresses as module offsets, raw stack words, readable-memory probes, raw memory dumps |
| `win32` | 56 | 17 | 17 | a Win32 / CRT API's own untyped parameter or result: `LPVOID` thread parameters, `LPARAM` message structs, `BYTE*` registry buffers, `intptr_t` OS handles, a CRITICAL_SECTION kept as opaque bytes because the type header cannot include `<windows.h>` |
| `float_bits` | 25 | 0 | 25 | a float read or written as its bit pattern |
| `string_header` | 24 | 4 | 20 | the `System::String` `{length, refCount}` header at `data - 8` |
| `pe_walk` | 12 | 12 | 0 | walks a loaded module's PE headers and export directory |
| `byte_io` | 8 | 0 | 8 | a byte buffer handed to / read from a stream, CRC or file API |
| `elem_stride` | 5 | 0 | 5 | container storage stepped by element size (`Collection.h`) |
| `gpu_map` | 2 | 0 | 2 | memory returned by a GPU buffer map |
| `com_iid` | 2 | 0 | 2 | the `void**` out-parameter of a COM `QueryInterface` / `GetBuffer` by IID |
| `crt` | 9 | 5 | 0 | the ported CRT's own `void*` / `char*` parameters (qsort, `_stricmp`) |
| `hash` | 3 | 2 | 1 | a pointer hashed as an integer |
| `vslot` | 1 | 1 | 0 | `Object::SlotAddressOf`, the one sanctioned look into a compiler vtable (diagnostics) |
| `dx11_seam` | 39 | 0 | 27 | (raised 27 -> 39 by R-VIDEO: the 12 `DeletingDtor` vector arms in `runtime/types/video/CompanionData.h`) deferred: the VideoDX11 seam, removed by plan §4.1 (`dx11/funcs/dx11_seam.*`, `Video::*Data` self-casts, video_interface's Dx9Placeholder) |
| **total** | **382** | **182** | **162** | |

E's own names map as: `crash_diag` → `crash_walk`, `pe_image` → `pe_walk`, `value_pun` →
`float_bits`, `container_storage` → `elem_stride`, `gpu_mapped` → `gpu_map`, `vtable_diag` →
`vslot`. E's three tier-I categories (`placement_new`, `print_addr`, `alloc_result`) are shapes,
not markers (§3).

## 2. The floor, by file

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
3. **`alloc_result`**: `static_cast<T*>(MemAlloc…(…))` / `static_cast<T*>(Allocate…(…))` and
   `(T*)MemAlloc…(…)`, the call being the WHOLE operand (qualified or through `->`).
   `runtime/src/application/message_callbacks.cpp:130`:
   `static_cast<Node*>(MemAlloc(0x30, 8));`
   `reinterpret_cast<T*>(VirtualAlloc(…))` is not this shape (a Win32 result: `win32`).

At E0: `print_arg` 222, `alloc_result` 106, `placement_new` 1: **329** counted casts.

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
- **The two faithful null-receiver reads** at `work_act_consume.cpp:268` / `:302`
  (`*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(0x50))`): they become a NAMED field read
  through a null pointer and stay a read (hard rule: no null check).
- **Casts that are no reinterpretation at all** but that `castmetric.py` counts (its operand
  types are inferred, not compiled): identity casts between `HANDLE` and `void*`
  (`hw_watch.cpp:56`, `sampler.cpp:197`), `(uintptr_t)ctx.Rip` of a `DWORD64` (`sampler.cpp:133`),
  and integer-to-integer `(int64_t)` casts whose member name is a pointer elsewhere in the tree
  (`allocator.cpp:1017 :1018 :1020 :1021`, `main.cpp:718`). They are deleted, not marked.
- `ptr_int` estimates include about 10 pointer compares inside raw BST nodes
  (`statvaluedispatch.cpp:870`): those fall once the node is typed. `float_bits` estimates include
  about 3 writes of integer 0 into a float slot (`action_pf_roots_road.cpp:190`).
