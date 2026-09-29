# Inventory F — every spelling of a reinterpretation (B.4 block 0, NEXT_SESSION §3b.6)

Owner F `all_spellings`. Read-only on product code. Base: main `b2a5434c` (working tree as of
2026-09-28). Outputs: this file, `data/F_all_spellings.tsv`, `scripts/F_all_spellings.py`, and
THE METRIC `castmetric.py` (this directory).

## 1. The answer

- **The summed metric, PROD (`code runtime standalone dx11 engine`): 46,733** reinterpretation
  spellings (occurrences, comments and string contents blanked). `tests/`: 813.
- Of those, `reinterpret_cast` is **28,913 (62%)**. The other 38% is what a `reinterpret_cast`
  count cannot see: C-style pointer casts **14,405**, `static_cast<T*>` from a `void*` **1,945**,
  offset-accessor calls `At<int>(self, 0x28)` **958**, memcpy of a raw address **256**,
  `const_cast` **191**, `static_cast<uintptr_t>` as an address base **58**, functional **5**,
  explicit `static_cast<T*>(static_cast<void*>(x))` **2**. Bridge markers: **6,849** lines.
- **`static_cast`, PROD: 8,401 occurrences.** Stays: arithmetic 4,468 + integer→uintptr 507,
  hierarchy 666 (616 of them are the factory upcast `static_cast<Object::Base*>(new (p) X())`),
  `T*`→`void*` 746 (placement-new targets, `%p` arguments). Goes (counted by the metric):
  from `void*` 1,945, `void*` chain 2, uintptr address base 58. Unresolved 6, null 1.
  **A `static_cast` can cross unrelated types ONLY through `void*`** (the compiler rejects every
  other pointer `static_cast` between unrelated classes), so "crossing" = those 2,005.
- **Compiler cross-check** (`g++ -fsyntax-only -Wold-style-cast`, ALL 942 TUs, plus 434
  de-`extern "C"` copies): C-style casts, all targets: compiler **19,868**, script **19,909**
  (+0.2%); pointer / reference targets: compiler **11,962**, script **11,975** (+0.1%).
  383 files carry a C-style cast; 373 agree exactly, 10 differ (6 by more than 5%; §4).
- Hand sample: 108 hits read (68 across the counted and uncounted classes + 40 integer-target
  C-style casts): 1 wrong (an accessor false positive, `Readable(slot, 8)`; fixed) → 0 known.

## 2. How the table was built

`castmetric.py DIR|FILE… [--by-file] [--tsv] [--detail] [--static] [--all-cstyle]`
(stdlib Python, no build, ~9 s for the whole tree; deterministic, verified under two
`PYTHONHASHSEED`s; paths are repo-relative whatever the cwd; a DIR that does not exist from the
cwd is taken from the repo root). It prints a per-directory table and one line
`# castmetric total: N`. `--by-file` adds one row per file; `--detail` prints every counted hit
as `file:line:col<TAB>column<TAB>snippet` (what a worker greps for); `--static` prints the
`static_cast` classes per directory.

The type index (known type names, pointer typedefs, the declared kind of every variable / member /
function return, the offset-accessor helpers) is ALWAYS built from the whole tree
(`code runtime standalone dx11 engine tests`), so a file counts the same whether it is measured
alone or with its directory. Names declared in a `.cpp` (a `typedef uint8_t* Elem;`, a local
`At` helper) are local to that file; names from headers are global.

| column | what is counted (per occurrence) |
|---|---|
| `reinterpret` | every `reinterpret_cast` |
| `cstyle_ptr` | a C-style cast `(T)e` whose T is a pointer, reference, function pointer or pointer typedef (`HWND`, `fn_*`, local typedefs) — except `(T*)0`/`NULL`/`nullptr` and `(void*)typedPtr` (the implicit T*→void* conversion spelled out; 414 such, mostly `%p`) — plus `(uintptr_t|intptr_t|…)e` unless e is a known integer, and `(size_t|uint64_t|int64_t|long long)e` when e is a known pointer / `&x` / `this` |
| `functional` | `uintptr_t(p)`, `PtrTypedef(p)`, `size_t(p)` from a pointer (a name that is also a function anywhere is a call, not a cast) |
| `static_void` | `static_cast<T*>(e)`, T not void, e a `void*`: certain when T's pointee is a scalar or T is `U**` (no hierarchy exists), else by e's declared kind (nearest preceding declaration in the file, else the member / return kind across the tree; an ambiguous name declared `void*` anywhere counts as `void*`); `operator new(n)` is `void*` |
| `static_uintptr` | `static_cast<uintptr_t|intptr_t|…>(x)` that is the leading term of a pointer cast's operand (`reinterpret_cast<T*>(static_cast<uintptr_t>(x) + 0x10)`) |
| `static_cross` | `static_cast<T*>(static_cast<void*>(x))` / `((void*)x)` / `(reinterpret_cast<void*>(x))` |
| `const_cast` | every `const_cast` |
| `pun` | `memcpy`/`memmove`/`__builtin_memcpy`/`CopyMemory`/`bit_cast` whose source or destination is a raw address (`p + off`, a cast pointer plus something), `&pointerVar`, or whose size is `sizeof(T*)`/`sizeof(uintptr_t)`; a `union` with a pointer member (0 in the tree) |
| `accessor` | a call with a numeric-literal argument to an OFFSET ACCESSOR: a function whose short body casts `base + off` of its own two parameters, or forwards both to an accessor (`At`, `P64`, `Field`, `Rd`, `ArrayAt`, `I32`, `F`, `SpriteWidth`, … ~70 definitions). `bool`/`void` returning helpers are excluded |
| `total` | the sum of the nine columns: THE METRIC |
| `bridge` | lines of raw source matching `B.3.<x>['s] bridge` (comment text; not in total) |

TSV (`data/F_all_spellings.tsv`, 1,007 rows): `file`, the eleven columns above, `static_cast`
(occurrences in the file) and its classes `s_arith s_conv s_hier s_hier_ref s_to_void
s_void_inner s_void s_cross s_ptr_unknown s_null s_uintptr_arith s_uintptr_addr s_discard
s_unparsed`. Re-run: `python docs/runtime_port/endgame_b/b4_session/scripts/F_all_spellings.py`.

**For a block (NEXT_SESSION §3d "before and after in my files"):**
`python docs/runtime_port/endgame_b/b4_session/castmetric.py <file…> --by-file --tsv` on main and
on the branch, same file list; the TOTAL row is the block's number, and no column may grow.

## 3. Counts

### Per directory (PROD rows sum to the metric)

| dir | files | reinterp | cstyle_ptr | funct | static_void | static_uintptr | static_cross | const_cast | pun | accessor | **total** | total/reinterp | bridge |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| code/Game/Action | 40 | 2417 | 1025 | 0 | 23 | 7 | 0 | 3 | 0 | 198 | **3673** | 1.52 | 338 |
| code/Game/Component | 222 | 8586 | 2141 | 0 | 91 | 31 | 0 | 19 | 3 | 244 | **11115** | 1.29 | 1394 |
| code/Game/Dialog | 76 | 2884 | 5994 | 0 | 72 | 9 | 0 | 53 | 2 | 37 | **9051** | 3.14 | 2065 |
| code/Game/Entity | 5 | 316 | 347 | 0 | 4 | 2 | 0 | 0 | 0 | 19 | **688** | 2.18 | 69 |
| code/Game/Game | 73 | 3060 | 1393 | 0 | 96 | 1 | 0 | 47 | 10 | 161 | **4768** | 1.56 | 1010 |
| code/Game/Goal | 7 | 413 | 75 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | **491** | 1.19 | 67 |
| code/Game/Object | 2 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **10** | 1.00 | 0 |
| code/Game/Player | 32 | 1482 | 737 | 1 | 25 | 0 | 0 | 2 | 4 | 1 | **2252** | 1.52 | 203 |
| code/Game/Tutorial | 26 | 581 | 232 | 0 | 1 | 0 | 0 | 6 | 1 | 0 | **821** | 1.41 | 235 |
| code/Game/Work | 53 | 2417 | 2289 | 0 | 37 | 3 | 0 | 0 | 1 | 0 | **4747** | 1.96 | 324 |
| dx11 | 12 | 135 | 0 | 0 | 94 | 0 | 0 | 0 | 1 | 17 | **247** | 1.83 | 51 |
| engine | 1 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **5** | 1.00 | 0 |
| runtime/funcs | 1 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | **10** | — | 0 |
| runtime/src | 316 | 4874 | 25 | 4 | 1314 | 5 | 2 | 53 | 234 | 281 | **6792** | 1.39 | 970 |
| runtime/types | 25 | 83 | 0 | 0 | 35 | 0 | 0 | 0 | 0 | 0 | **118** | 1.42 | 4 |
| standalone | 48 | 1650 | 147 | 0 | 143 | 0 | 0 | 5 | 0 | 0 | **1945** | 1.18 | 119 |
| **PROD** | 939 | **28913** | **14405** | 5 | **1945** | 58 | 2 | 191 | 256 | **958** | **46733** | 1.62 | 6849 |
| tests | 68 | 556 | 19 | 0 | 49 | 37 | 2 | 12 | 46 | 92 | **813** | 1.46 | 11 |

(`files` = files with any hit or any `static_cast`.) The per-subdirectory table
(`runtime/src/ui`, …) is `castmetric.py code runtime standalone dx11 engine`.

### `static_cast` classified, per directory

| dir | static_cast | arith | uintptr arith | hier (down/up) | hier ref | to void* | **from void*** | **void* chain** | **uintptr addr** | unknown | null |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| code/Game/Action | 633 | 448 | 69 | 49 | 0 | 37 | 23 | 0 | 7 | 0 | 0 |
| code/Game/Component | 1964 | 1013 | 251 | 305 | 0 | 273 | 91 | 0 | 31 | 0 | 0 |
| code/Game/Dialog | 459 | 122 | 21 | 120 | 0 | 114 | 72 | 0 | 9 | 0 | 1 |
| code/Game/Entity | 49 | 31 | 8 | 2 | 0 | 2 | 4 | 0 | 2 | 0 | 0 |
| code/Game/Game | 475 | 281 | 4 | 32 | 0 | 61 | 96 | 0 | 1 | 0 | 0 |
| code/Game/Goal | 54 | 26 | 0 | 11 | 0 | 17 | 0 | 0 | 0 | 0 | 0 |
| code/Game/Player | 273 | 215 | 11 | 18 | 0 | 4 | 25 | 0 | 0 | 0 | 0 |
| code/Game/Tutorial | 167 | 54 | 11 | 39 | 0 | 62 | 1 | 0 | 0 | 0 | 0 |
| code/Game/Work | 565 | 393 | 53 | 50 | 0 | 29 | 37 | 0 | 3 | 0 | 0 |
| dx11 | 259 | 151 | 3 | 1 | 0 | 7 | 94 | 0 | 0 | 3 | 0 |
| runtime/funcs | 10 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 |
| runtime/src | 3117 | 1622 | 8 | 36 | 1 | 124 | 1314 | 2 | 5 | 3 | 0 |
| runtime/types | 81 | 41 | 0 | 0 | 1 | 4 | 35 | 0 | 0 | 0 | 0 |
| standalone | 295 | 71 | 68 | 1 | 0 | 12 | 143 | 0 | 0 | 0 | 0 |
| **PROD** | **8401** | 4468 | 507 | 664 | 2 | 746 | **1945** | **2** | **58** | 6 | 1 |
| tests | 852 | 230 | 3 | 117 | 4 | 406 | 49 | 2 | 37 | 0 | 1 |

(Two `static_cast<void*>` that are the inner half of a chain are in no column; 8,401 includes
them.) Stays: arith, uintptr arith, hier, hier ref, to void*. Goes: the three bold columns. The
48 non-factory `hier` sites are the "downcast after a type check" the rule sheet keeps.

### Top 30 files (PROD; 275 files with ≥ 50 hold 36,740 = 79%; 145 with ≥ 100 hold 27,495)

| file | total | reinterp | cstyle_ptr | static_void | accessor | pun | bridge |
|---|--:|--:|--:|--:|--:|--:|--:|
| `code/Game/Dialog/tradeui.cpp` | 959 | 246 | 701 | 0 | 8 | 0 | 215 |
| `code/Game/Dialog/townhallui.cpp` | 838 | 228 | 601 | 0 | 3 | 0 | 187 |
| `code/Game/Component/citizencomponent.cpp` | 706 | 654 | 49 | 2 | 0 | 1 | 60 |
| `code/Game/Game/starttown.cpp` | 549 | 157 | 379 | 11 | 0 | 0 | 60 |
| `code/Game/Dialog/optionsdialog.cpp` | 547 | 149 | 391 | 4 | 0 | 0 | 143 |
| `code/Game/Dialog/toolbardialog.cpp` | 509 | 124 | 372 | 7 | 3 | 0 | 95 |
| `standalone/tool_harness.cpp` | 471 | 412 | 8 | 51 | 0 | 0 | 34 |
| `code/Game/Dialog/debugui.cpp` | 409 | 76 | 328 | 1 | 0 | 0 | 60 |
| `code/Game/Dialog/saveloaddialog.cpp` | 396 | 149 | 232 | 3 | 1 | 0 | 125 |
| `code/Game/Work/marketwork.cpp` | 373 | 282 | 87 | 4 | 0 | 0 | 18 |
| `code/Game/Entity/citizenbehavior.cpp` | 362 | 211 | 138 | 0 | 11 | 0 | 18 |
| `code/Game/Goal/statvaluedispatch.cpp` | 352 | 315 | 37 | 0 | 0 | 0 | 20 |
| `code/Game/Dialog/professionui.cpp` | 330 | 73 | 256 | 0 | 1 | 0 | 41 |
| `runtime/src/collection/array.cpp` | 329 | 305 | 0 | 20 | 0 | 4 | 0 |
| `code/Game/Action/action_pf_region.cpp` | 296 | 1 | 295 | 0 | 0 | 0 | 7 |
| `code/Game/Game/gameupdateloop_load.cpp` | 294 | 95 | 176 | 14 | 1 | 0 | 41 |
| `code/Game/Action/action_pf_astar.cpp` | 288 | 288 | 0 | 0 | 0 | 0 | 8 |
| `code/Game/Player/keystone_a_removalcalcpath.cpp` | 278 | 263 | 13 | 2 | 0 | 0 | 14 |
| `code/Game/Dialog/workplaceui.cpp` | 273 | 127 | 140 | 0 | 0 | 0 | 42 |
| `code/Game/Game/gameupdateloop_serialize.cpp` | 271 | 148 | 116 | 6 | 0 | 1 | 93 |
| `code/Game/Game/goals_serialize.cpp` | 266 | 263 | 0 | 2 | 0 | 0 | 33 |
| `code/Game/Action/action_pf_avl.cpp` | 262 | 64 | 0 | 0 | 198 | 0 | 0 |
| `code/Game/Action/action_pf_pools.cpp` | 260 | 260 | 0 | 0 | 0 | 0 | 0 |
| `runtime/src/graphics/animationstate_transforms.cpp` | 255 | 224 | 0 | 31 | 0 | 0 | 18 |
| `code/Game/Game/gameupdateloop.cpp` | 253 | 127 | 104 | 18 | 0 | 0 | 95 |
| `code/Game/Work/consumeproduce.cpp` | 249 | 45 | 203 | 1 | 0 | 0 | 27 |
| `code/Game/Dialog/eventnotifydialog.cpp` | 237 | 54 | 176 | 5 | 1 | 0 | 45 |
| `code/Game/Component/herdcomponent.cpp` | 231 | 82 | 148 | 1 | 0 | 0 | 36 |
| `code/Game/Dialog/entityui.cpp` | 223 | 26 | 197 | 0 | 0 | 0 | 21 |
| `code/Game/Work/cleararea.cpp` | 223 | 52 | 171 | 0 | 0 | 0 | 11 |

### The compiler cross-check, in detail

- Flags: the Makefile's `standalone-debug` rule (`-std=c++17 -fno-strict-aliasing
  -fno-lifetime-dse -g -O0 -D_DEBUG`, `SA_INCLUDES`), `-Wall -Wextra` dropped, plus
  `-fsyntax-only -Wold-style-cast`; 2 processes; every output in the F scratch directory; the
  Makefile untouched; nothing written under `standalone/build*`. All 942 TUs (not a sample;
  syntax-only is ~0.2 s a TU), plus a `-MMD -MF` pass for header coverage: every file holding a
  script C-style hit was seen by the compiler.
- **GCC does not emit `-Wold-style-cast` inside an `extern "C"` scope** (the plain pass gave
  18,003). The 434 TUs that hold `extern "C"` were copied to scratch with the linkage specs
  blanked (line numbers kept) and re-compiled (`-iquote` the original dir; 433 clean, 1 with
  four C-linkage conflicts, still counted): +1,865 → 19,868. One hit sits in an `extern "C"`
  block of a header and stays unverified.
- Residual, per file (compiler / script, all C-style): `gameupdateloop.cpp` 132/147 (15 casts
  inside `TRACE(...)` arguments, `#define TRACE(...) do {} while(0)` drops them),
  `optionsdialog.cpp` 480/487 and `toolbardialog.cpp` 406/413 (`#else` arms),
  `buildrequirementui.cpp` 71/75, `buildui.cpp` 134/137 (a multi-line cast attributed to the
  operand's line, plus an `#if` arm), `Input.h` 10/12 and `SliderElement.h` 2/1 (`static_assert`
  expressions), `type_reflection.cpp` 4/6 and `internal_class.cpp` 0/1 (GCC does not warn an
  identity cast, `(size_t)uint64`), `dragtool.cpp` 14/15 (a macro body: the compiler reports the
  expansion, the script the definition). Nothing the compiler counts is missed by the script on a
  whole-file basis except `SliderElement.h` (1).
- The compiler says nothing about the SOURCE type, so it cannot check the "integer target from a
  pointer" half of `cstyle_ptr` (2,887 of 14,405); that half was hand-sampled (40 rows, 0 wrong).

## 4. Blind spots

1. **Operand types are inferred, not compiled.** `static_void` and the integer half of
   `cstyle_ptr` depend on the declared kind of the operand: nearest preceding declaration in the
   file, else the kind the name has across the tree (members, returns); an ambiguous name
   declared `void*` anywhere counts as `void*`. 6 `static_cast<T*>` stay unresolved
   (`static_unknown` in `--detail`; not counted). A `(size_t)x` / `(uint64_t)x` whose x is an
   unknown pointer is NOT counted (2,301 uncounted 64-bit integer C-style casts; sampled 20, all
   arithmetic).
2. **Raw offset reads with no cast at the site**: `self[0x3c]` on a `uint8_t*`, `param_1[3]` on an
   `int64_t*` (≈ 485 code lines subscript with a hex literal, 279 lines `param_N[k]`). They carry
   no spelling to count; they are inventory C's (the subscript on a cast pointer).
3. **Accessor helpers**: detected by the shape of their definition; a call is counted only with a
   literal offset argument (`At<int>(self, off)` with a variable offset is not); an accessor that
   takes the offset from a table is invisible. Read by hand: the ~70 definitions (1 false
   positive, `bool Readable(p, n)`, excluded by return type).
4. **Macros**: a cast inside a macro body counts once, at the definition (2 such macros:
   `DIALOG_IBP()` in `dragtool.cpp`, `RTGC_LAYERS()`); `TRACE(...)` arguments are counted though
   the build drops them.
5. **Every preprocessor arm is counted** (`#if _DEBUG` / `#else`, `BANISHED_UNIT_TEST`); there is
   no `#if 0` in PROD.
6. **Value puns are not counted**: `memcpy(&f, &bits, 4)` float↔int bit copies (legitimate) are
   not; only memcpy of a raw address / a pointer. `std::bit_cast` and unions: 0 in the tree.
7. **The implicit `void*` receiver** (`Body(void* self)` called with a typed pointer) has no cast
   at the call; it shows up only where the body casts back (`static_void`, `reinterpret`, C-style).
8. Tests are their own row; `standalone/build*`, `tests/unit/build/`, `.claude/worktrees/`,
   and any `build`/`build-*`/`build_*` directory are excluded (anchored on a path separator).

## 5. What surprised me

1. **NEXT_SESSION §3b.6: "`-Wold-style-cast` on one build gives the C-style ones exactly" is
   false.** GCC is silent in `extern "C"` scope (434 of 942 TUs; 1,865 casts, 9%), on identity
   casts, and on code the configuration drops (`TRACE` arguments, `#else` arms). It is the
   cross-check, not the metric.
2. **§3a "C-style pointer casts, code/Game ~10,300 (it overcounts)" undercounts.** 10,163 LINES
   hold pointer-target casts (the regex was right in lines), but that is 11,447 occurrences, plus
   2,786 `(uintptr_t)ptr`-style casts the regex never matched: 14,233 in `code/Game`.
3. **Dialog is C-style**: 5,994 C-style vs 2,884 `reinterpret_cast` (total/reinterp 3.14).
   `action_pf_region.cpp` has ONE `reinterpret_cast` and 295 C-style casts: invisible to a
   `reinterpret_cast` count (and to §3a's "141 files with 50 casts or more").
4. **runtime/src spells its reinterpretations `static_cast<T*>(void*)`**: 1,314 (plus 234 memcpy
   of raw addresses, 281 accessor calls): 6,792 real vs the 4,874 `reinterpret_cast`.
5. **§3a "static_cast to a pointer or uintptr_t: 3,822" is mostly not reinterpretation**: 616
   factory upcasts `static_cast<Object::Base*>(new (p) X())`, 746 `T*`→`void*` (placement-new
   targets, `%p`), 507 integer→`uintptr_t` conversions stay; 2,005 go.
6. **The respelling the rule forbids already exists at scale**: 958 accessor calls
   (`At<float>(desc, 0x30)` 230, `P64(n, 0x20)` 130, `Rd` 82, `Field<>` 69, `ArrayAt` 63, …),
   each a raw offset read with no cast at the call site, from ~70 helpers, most of them
   file-local copies in anonymous namespaces (`At` in 13 files, `Field` in 9). And 256 memcpy
   field copies (`std::memcpy(&h, s + 0x12, 2)`, `runtime/src/picking/scene_grid.cpp`).
7. `reinterpret_cast` is 28,913 occurrences on 25,961 code lines today (§3a: 26,488 lines at the
   teardown's close); bridge markers 6,849 lines (§3a: 6,859).

## 6. How the conversion splits, as the metric sees it

The class / file map belongs to A, B and C; the metric adds what each block must measure and
where the invisible casts are.

- **Every block hands in `castmetric.py <its files> --by-file --tsv` on main and on its branch.**
  Acceptance per block: `total` falls and NO column rises (a fall in `reinterpret` paired with a
  rise in `cstyle_ptr`, `static_void`, `accessor` or `pun` is a respelling, §3d).
- **Freeze the baseline first**: commit `castmetric.py` and `data/F_all_spellings.tsv` with W0's
  base; W0 (headers only) must leave `code/`, `runtime/src`, `standalone` rows unchanged.
- Blocks by weight (disjoint files; the classes inside come from A/C):
  | candidate block | files | metric | first? |
  |---|--:|--:|---|
  | Dialog panels (tradeui, townhallui, optionsdialog, toolbardialog, debugui, saveloaddialog, professionui, workplaceui, eventnotifydialog, entityui + the rest of `code/Game/Dialog`) | 76 | 9,051 (66% C-style) | after the UI element headers (`runtime/types/ui`) |
  | `code/Game/Component` (citizencomponent 706 alone; state-machine families) | 222 | 11,115 | CitizenComponent header first (inventory D) |
  | `code/Game/Game` (gameupdateloop_*, starttown, goals_serialize) | 73 | 4,768 | GameUpdateLoop layout first (D): `_singleton + 0x…` reads cross every module |
  | `code/Game/Work` + `Entity` | 58 | 5,435 | |
  | `code/Game/Action` (pathfinding pools/astar/avl: 198 accessor calls in `action_pf_avl.cpp`) | 40 | 3,673 | |
  | `code/Game/Player` + `Tutorial` + `Goal` + `Object` | 67 | 3,574 | |
  | `runtime/src` (static_void 1,314: `void* self` bodies; `collection/array.cpp` 329) | 316 | 6,792 | per namespace, its `runtime/types/<ns>` header with it |
  | `standalone` harnesses (`tool_harness.cpp` 471) | 48 | 1,945 | last: they read game objects through the same offsets |
  | floor: `dx11` 247, allocator, serializer byte streams, `VSlot.h`, hw watch | — | E lists it | stays |
- **Accessor helpers go with their last caller**: a file-local `At`/`Field`/`P64` dies in the block
  that owns its file; a header accessor (`ArrayAt`, `I32`, `U32`, `F32`, `SpriteWidth` in
  `runtime/src/ui/widget_common.h`, `code/Game/Component/coll_bridge.h`) needs its own owner and
  goes last.
- **Behaviour risk the metric exposes**: an integer-target C-style cast (`(int64_t)ptr`,
  `(uint32_t)(intptr_t)data` in `optionsdialog.cpp`) and a `memcpy` field copy both fix a WIDTH;
  turning them into a named field read must keep that width (§3d; `known_port_issues` 5), else it
  is a `behaviour:` commit.
