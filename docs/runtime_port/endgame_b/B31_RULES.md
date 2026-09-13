# ENDGAME B.3.1 — value types: rules for the batch (coordinator, 2026-09-13)

Scope: ENDGAME.md §B.3 step 1 — `System::String`, `System::Filename`, the `Math` value
types, and (after a design read) the smart pointers. Read ENDGAME.md §B.1 decisions 1, 5, 6
and §B.2 before starting. The gate is §B.1.8.

## Measured start (main 1d97579c)

| vocabulary | code | engine | runtime | standalone | dx11 |
|---|---|---|---|---|---|
| `RtSystem_String_*` direct calls | 0 | 0 | 252 | 68 | 0 |
| `"…String@System@@…"` name lookups (`RT_EXPORT_ONCE` etc.) | 9 | 65 | 432 | 375 | 9 |
| `fn_String_*` pointer calls (engine_funcs.h) | 969 | 143 | 2 | 22 | 0 |
| `fn_Filename_*` pointer calls | 310 | 85 | 1 | 26 | 0 |
| `RtSystem_Filename_*` direct calls | — | — | ~240 | — | — |
| `"…@Math@@…"` name lookups | 173 | 60 | 383 | 43 | 0 |
| `RtMath_*` direct calls | 0 | 0 | 198 | 0 | 0 |
| `fn_Vector*/Quaternion*/Transform*` pointer calls | 187 | 141 | 4 | 2 | 0 |
| `reinterpret_cast` (the B.4 metric) | 23,657 | 962 | 6,826 | 3,066 | 145 |

Registry rows by name (`RegisterRuntimeExport`): String 161, Filename 92, Math 384. Three
PODs describe the same 8 bytes today: `RtSystem::String` (runtime/types/system/String.h),
`RtSystem::Filename`, and the exe-side `SystemString` (engine/types/SystemString.h). Two
PODs describe each Math type: `RtMath::*` (runtime/types/math/Math.h) and `Vector2/3/4`
in engine/types/common.h.

## The one fact that changes the rules since the port

**There is no cross-compiler ABI any more.** Runtime-x64.dll is gone; every caller and
callee is compiled by the same GCC. The only binary facts that still bind are the OBJECT
LAYOUT (every `static_assert(offsetof/sizeof)` stays) and vtable slot order (decision 2,
not touched by B.3.1). Hidden return slots, `(ret, self, …)` argument shapes and the
`fn_*` typedefs were the DLL boundary's shape; they are now just the C-body shape and go.
Retail addresses stay as comments on every body: they are the provenance (decision 5).

## Two steps, not one

**Step 1 — the core (one owner per type family, serial inside the family).** Introduce the
real class under its RETAIL name (`System::String`, `System::Filename`, `Math::Vector3`,
…) with the bodies the C functions already have, and keep EVERY existing vocabulary
working through thin shims so that no caller has to move in the same commit:

- `RtSystem_String_Set(RtSystem::String* self, const char*)` becomes a one-line shim that
  calls `System::String::Set` on the same bytes. The old POD name stays declared.
- The `RegisterRuntimeExport/Addr` rows now register the shims. Not deleted in step 1.
- `fn_String_*` / `fn_Filename_*` / `fn_Vector*` pointers in engine_funcs.{h,cpp}: untouched
  in step 1 (they resolve to the shims through the registry exactly as before).

**Why a NEW name and not the old POD made real:** ported bodies hold
`RtSystem::String tmp; RtSystem_String_Ctor(&tmp); … RtSystem_String_Dtor(&tmp);`. Give
that POD a real destructor and the compiler runs `~String` after the manual one — and
retail's release idiom leaves `data = block` DANGLING on the free branch (String.h header
comment), so that is a double free on every such local. The old PODs stay trivially
destructible until their last user is converted, then they are deleted.

**Step 2 — the callers (parallel, by directory).** Each batch owns a disjoint file set and
converts its call sites to the class: locals and temporaries become RAII objects and the
manual Ctor/Dtor/Release pairs around them are deleted (early returns and `goto cleanup`
paths included — check each one); `RT_EXPORT_ONCE(fn, "?M@String@System@@…")(self, …)`
becomes `str.M(…)`; `fn_String_X(&s, …)` becomes a member call. When a shim, a `fn_*`
pointer, or a `RegisterRuntimeExport` row has zero callers left in the whole tree, the
batch that removes the last caller deletes it (grep the tree first, and grep the retail
ADDRESS too: a `RegisterRuntimeAddr` row may be reached by address from a minted data
table — `tools/rt_mint_gen.py` inputs and `ResolveAddr(Module::Runtime, 0x18…)` sites).

**Members of not-yet-converted structs keep their POD type** (`RtSystem::String path;`
inside `ExternalFactory`, `SystemString` inside a component). They change when THAT class
is converted (§B.2); until then the containing code reaches them through the shims or
through `reinterpret_cast<System::String&>(field)` at the use site — the latter is a
transitional cast that the containing class's conversion removes, and it is preferable to
leaving the whole body in C shape.

## Class design rules (all families)

1. Namespace and class name are retail's (`System::String`, not `RtSystem::String`;
   `Math::Vector3`, not `RtMath::Vector3`). Method names are retail's export names
   (`Set`, `SubString`, `GetLength`, `operator+`, `operator+=`, `operator==`, `operator<`,
   `Serialize`, `Format`, `FormatIndexed`, `ToFloat`, `ToInteger`, …). Static functions
   stay static. Private members (`SetLength`, `Release`, `AEAA`-mangled) are private.
2. Layout is byte-identical: same fields, same order, `static_assert(sizeof)` and every
   existing `static_assert(offsetof)` kept. `String` is `{ char* _data; }`, nothing else.
3. Semantics are byte-faithful to the ported body, including retail's release asymmetry
   (String.h) — the C body moves, it is not rewritten. The manual AddRef/Release sequences
   in CALLERS were retail's compiler emitting RAII; those disappear in step 2. The bodies
   of the class itself keep them.
4. Copy ctor, copy assignment, destructor are the retail exports. Add a move ctor/assign
   only if it is a pure pointer steal with no observable difference (String: yes,
   `_data` swap; retail had none, C++03).
5. Default ctors: read retail's. A `= default` is allowed only when retail's ctor is a
   no-op (then value-init still zeroes, which is a strict superset of retail; default-init
   is identical). A user-provided no-op `T() {}` is FORBIDDEN: it silently turns
   `Foo f = {};` into "vectors uninitialised" in every struct that contains the type.
6. The class is usable standalone in a host unit test (`make unit`, tests/unit/). Add
   tests for every public method with the cases the C bodies' comments document
   (String: empty string is null, refcount born at 1, SubString bounds, Format with the
   `%` forms retail supports, ToFloat/ToInteger edge cases; Filename: AdjustDelimiters,
   GetPathFilename with/without stripExt, GetPathIdentifer, GetPathDirectory/Extension).
   Filename's allocator dependency is `runtime/src/system/allocator*.cpp` — ours, link it
   (or a test-only allocator behind `BANISHED_UNIT_TEST` if the real one drags in the
   registry; say which in the report).
7. A converted file has no `reinterpret_cast` left except the four legitimate kinds
   (ENDGAME §B.4). Count before/after per file in the report.
8. `-fno-strict-aliasing` is unconditional; do not rely on it for new code.
9. No new `BANISHED_*` flag, no new instrument, no `getenv` (ENDGAME C is closed).

## Hygiene (project memory; every point has cost a run before)

1. Edit in place; never rewrite a file with Write. Never drop an existing function.
2. Build: `mingw32-make standalone-debug -j8 > build.log 2>&1; echo $?` — the exit code,
   not a pipe's. Fix every error and every new warning. Also `mingw32-make unit`.
3. One real run: `tools/game_dir_lock.sh acquire <batch> 900` (shared by several agents;
   wait for it), then deploy: `cp standalone/build/reimpl-x64.exe "/c/GOG Games/Banished/"`,
   `rm -f "/c/GOG Games/Banished/standalone_{debug,crash,error,hang}.log"`, then from that
   directory `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
   ./reimpl-x64.exe; echo exit=$?`, in the FOREGROUND with a 600000 ms Bash timeout
   (never run_in_background; never wait on a notification; if it times out re-invoke
   and check `tools/boot_state.sh`). Pass = exit 0, no standalone_crash.log, no
   standalone_hang.log, `[AutoStart]` lines in standalone_error.log. Release the lock in
   the same command line as the run (`; tools/game_dir_lock.sh release <batch>`).
   If it dies, read standalone_crash.log FIRST and symbolize with
   `addr2line -f -C -e standalone/build/reimpl-x64.exe`. Exit 4 + standalone_hang.log
   means the exe froze: read the hang log.
4. Before the final build: merge `main` into your branch, keep both sides on conflicts,
   rebuild, rerun step 3.
5. Commit on your worktree branch (message: `ENDGAME B.3.1 <family> <step>: ...`). Do not
   push. Do not run the resave gate (coordinator only, on every merge).

## Report (short)

Class(es) introduced and where; methods count vs C bodies count (every C body must have
a home — list any that did not); shims left and their caller counts per directory;
`reinterpret_cast` count in the files you own before/after; unit test count and result;
build exit code; run exit code + crash/hang log state; commit hash; anything left and why.
