# ENDGAME B.3.4a closing 2b/2c — the guard-and-pointer sweep: handover spec

Written by `b34-closing` after commits 1/4 (`6564144b`) and 2a/4 (`67035026`).
Every number below was MEASURED on the tree at `67035026` by the scripts in this
directory, not carried over from a brief. Re-measure before you start: `git merge
main` first, then re-run §5's scripts — commit 2a already moved some of these to zero.

**What 2a already did, so you do not redo it.** 2a retired the 18 zero-call doors
whose *entire* remaining occurrence set was inside `engine/funcs/engine_funcs.{h,cpp}`
— no guard, no caller, no cache anywhere else. What is left is exactly the doors that
still have a READINESS GUARD somewhere in `code/Game`, plus the ~369 file-local door
caches. **Guard and pointer must die in the same change**: removing the pointer and
keeping the guard does not compile, and keeping the pointer only to feed the guard is
dead weight. That is the whole reason this is one job and not two.

---

## 0. How the work is split

Four agents, by directory, each owning its own files:

| agent | files | local-cache files | local-cache names |
|---|---|---|---|
| **A** | `code/Game/Component/` | 57 | 190 |
| **B** | `code/Game/Game/` | 11 | 36 |
| **C** | `code/Game/Action/` + `code/Game/Work/` | 31 | 112 |
| **D** | `code/Game/{Tutorial,Goal,Entity,Player,Dialog,Object}/` and any other `code/Game` dir | 8 | 17 |

Each agent removes, **in its own files only**:
- the file-local door caches (static pointer + its `typedef` + its resolver statement
  + the resolver's `s_*_resolved` latch if that latch has no other reader);
- the readiness guards on those local caches **and** on the guard-bearing GLOBALS of §1.

**Nobody touches `engine/funcs/engine_funcs.{h,cpp}` in A–D.** The globals'
declarations, definitions, bindings and `_DEBUG` prints are deleted **LAST**, by one
agent, after all four have landed — because until the last guard is gone the extern
still has a reader, and the build proves it. Call that pass **2d**.

`engine/` shows 1 file / 14 local names in §5's listing: that is `engine_funcs.cpp`
itself and it belongs to 2d, not to A–D.

---

## 1. The guard-bearing globals — exact list, binding, and occurrences per directory

All CALL counts are **0**; what remains is guards, plus the two odd readers in §2.7.
Counts are non-comment, non-string occurrences.

| door | bound at | binding | Component | Game | Action+Work | rest | engine |
|---|---|---|---|---|---|---|---|
| `g_fn_Serializer_Ctor` | engine_funcs.cpp:2790 | `ResolveExport("??0Serializer@IO@@QEAA@AEAVStream@1@H@Z")` | 89 | 18 | 21 | 32 | 3 |
| `g_fn_Serializer_Dtor` | :2792 | `ResolveExport("??1Serializer@IO@@QEAA@XZ")` | 89 | 18 | 21 | 32 | 3 |
| `g_fn_Serialize_Int` | :2794 | `ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z")` | 62 | 21 | 29 | 20 | 3 |
| `g_fn_Serialize_Bool` | :2796 | `ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z")` | 32 | 11 | 19 | 6 | 3 |
| `g_fn_Serializer_GetVersion` | :2798 | `ResolveExport("?GetVersion@Serializer@IO@@QEBAHXZ")` | 12 | 5 | 7 | 3 | 3 |
| `fn_Serializer_Serialize_Float` | :3983 | `ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z")` | 29 | 14 | 0 | 0 | 4 |
| `fn_Serializer_Serialize_Uint` | :4112/:4114 | slot `0x140152a50`, else mangled | 5 | 7 | 0 | 0 | 5 |
| `fn_Serializer_GetEncoding` | :4105/:4107 | slot, else `?GetEncoding@Serializer@IO@@…` | 2 | 4 | 1 | 0 | 5 |
| `fn_Serializer_SerializeBulk` | :3576 | `*(T*)ResolveAddr(Module::Exe, 0x140152400)` | 0 | 1 | 0 | 0 | 3 |
| `fn_InternalBasePtr_Serialize` | :3808 | `ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z")` | 14 | 6 | 17 | 2 | 4 |
| `fn_Ser_Slot_SerByte_410` | :4174 | slot `0x140152410` | 0 | 1 | 0 | 0 | 4 |
| `fn_Ser_Slot_SerSizedRaw_3F8` | :4177 | slot `0x1401523f8` | 0 | 1 | 0 | 0 | 4 |
| `fn_Ser_Slot_SerSizedRaw_400` | :4179 | slot `0x140152400` | 0 | 1 | 0 | 0 | 4 |
| `fn_Persistence_GetSavePath` | :2017 | `ResolveExport("?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ")` | 0 | 0 | 0 | 2 | 5 |
| `fn_Persistence_WriteSettings` | :3503 | `ResolveExport("?WriteSettings@Persistence@IO@@QEBAXXZ")` | 0 | 0 | 0 | 1 | 3 |
| `fn_Persistence_ReadSettings` | :3505 | `ResolveExport("?ReadSettings@Persistence@IO@@QEAAXXZ")` | 0 | 0 | 0 | 3 | 3 |
| `fn_FileSystem_GetDataDirectory` | :2023 | `ResolveExport("?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")` | 0 | 0 | 0 | 2 | 5 |
| `fn_Modification_GetCount` | :2711 | `ResolveAddr(Module::Runtime, 0x18000da90)` | 0 | 0 | 0 | 1 | 3 |
| `fn_Modification_GetMod` | :2713 | `ResolveAddr(Module::Runtime, …)` | 0 | 0 | 0 | 1 | 3 |
| `SerializerEncodingVCall` | engine_funcs.h:3915 | an `inline` helper, not a pointer | 0 | 0 | 0 | 0 | 1 |

The `engine` column is always the declaration + definition + binding (+ `_DEBUG`
print) — i.e. 2d's share. `SerializerEncodingVCall` has no caller at all and is
deleted whole by 2d, together with the "same fn, different usage" comment on it.

**Also for 2d, not for A–D:** the `Harvest`/`Plant` alias lines
(`s_fn_Serializer_SerializeBoolArray = <the int door>` + typedef + its "same fn,
different usage" comment) in `harvestarea.cpp` / `harvestorchard.cpp` — but see
§4.6: `plantarea.cpp` / `plantorchard.cpp` still CALL theirs and are being fixed
separately in commit 3. Do not touch those two files.

---

## 2. The guard shapes, with a rule for each

Measured over `code/Game`, excluding `engine/funcs/` (473 occurrences, all shapes):

| count | shape | rule |
|---|---|---|
| 213 | plain `if (p) <member call>;` | delete the `if`, keep the body, unindent |
| 78 | negated chain `if (!p \|\| !q \|\| …)` | drop only the door conjuncts; if ALL conjuncts were doors, the whole `if` + its body goes |
| 66 | "other" — read by hand | §2.7 |
| 45 | early return `if (!p) return;` | as the negated chain; if the condition empties, delete the statement |
| 42 | conjunct in a multi-line `ok = a && b && …` chain | §2.5 |
| 19 | `if (p && <other>)` | keep `<other>`: `if (version > 0 && g_fn_Serialize_Bool)` becomes `if (version > 0)` |
| 6 | ternary with a FALLBACK VALUE | §2.4 |
| 4 | `else if (…)` guard | §2.6 |

### 2.1 Plain `if (p) …`
```cpp
if (g_fn_Serializer_Dtor) reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();
```
The guard tests a POINTER the member call no longer uses. Delete the `if`, keep the
call. The body is often a block — unindent it and delete the braces, or keep the
braces if that keeps the diff readable; either is fine, but do not leave a bare
`if (true)`.

### 2.2 Negated chain / early return
```cpp
if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
    !fn_ExternalPtr_Serialize2) {
    return;
}
```
`fn_ExternalPtr_Serialize2` is an **ExternalPtr exe door — it is NOT yours** (§4.2),
so this one becomes `if (!fn_ExternalPtr_Serialize2) return;`. Only when every
conjunct is a door of §1 does the whole statement go.
**Check the line AFTER every deletion**: B.3.3 learned that removing a statement
inside a multi-line `#define` eats the previous line's `\`. The same applies to a
multi-line condition — deleting the last conjunct can leave a dangling `||`.

### 2.3 `if (p && <other>)`
Keep the other conjunct, verbatim. `if (g_fn_Serializer_GetVersion &&
ser->GetVersion() > 0)` becomes `if (ser->GetVersion() > 0)`.

### 2.4 Ternary with a FALLBACK VALUE — 6 sites, list every one in the commit
```cpp
int version = g_fn_Serializer_GetVersion ? ser->GetVersion() : 4;
```
The coordinator's rule: the pointer is unconditionally bound in the standalone
(§3), so the fallback arm is dead and goes — `int version = ser->GetVersion();`.
**But name every such site in the commit message**, because the fallback is a
DIFFERENT VALUE, not a no-op, and a reviewer must be able to see which constant
stopped being reachable. The six, measured:

- `code/Game/Game/gamegrid_serialize.cpp:235` fallback `4`
- `code/Game/Game/gamegrid_serialize.cpp:301` fallback `4`
- `code/Game/Game/goals_serialize.cpp:652` fallback `0`
- `code/Game/Game/goals_serialize.cpp:684` fallback `0`
- `code/Game/Work/work_base_reimpl.cpp:301` fallback `0`
- `code/Game/Work/work_base_reimpl.cpp:306` fallback `0` (inside `if ((… ? … : 0) > 2)`)

### 2.5 Readiness chains `ok = a && b && c;`
```cpp
bool ok = s_pGameState && s_fn_CompleteRoadTile && s_fn_Serialize_UChar &&
          g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
          g_fn_Serialize_Bool && g_fn_Serialize_Int;
```
Drop the door conjuncts, keep the rest, keep the `ok` variable and every reader of
it. If ALL conjuncts were doors, `ok` becomes `true` — then delete `ok`, its
`s_*_resolved` latch and every `if (!ok) return;` that tested it, **in the same
change**, and say in the message that the function is now unconditional.
Watch the line continuations: these chains span 2–4 lines.

### 2.6 `else if` guards
```cpp
} else if (fn_Serializer_SerializeBulk && heightBuf && byteCount > 0) {
```
Keep the non-door conjuncts. An `else if` whose condition empties entirely becomes a
plain `else` — do **not** delete the arm, its body is behaviour.
`code/Game/Dialog/scenariodialog.cpp:406-407` is a different animal: a chain that
builds a diagnostic string naming the first missing primitive. Those arms describe a
failure that can no longer happen; delete the two door arms and leave the rest of
the chain intact.

### 2.7 Guards with an ELSE ARM THAT HAS BEHAVIOUR, and the two odd readers
An `if`/`else` where the else arm prints a warning, writes a stream-equivalent
fallback, or picks a different door is **not a readiness guard** — decide per site
and SAY WHAT YOU DID in the commit. The disposition commit 1 used, and the reason,
is the precedent: when the else arm exists only to report or to compensate for "the
door did not resolve", it goes with the door, because the state it described cannot
occur once the member is a real C++ member. When the else arm is retail behaviour,
it stays.

Two readers in the "other" bucket are NOT guards and will break the build if you
forget them:

1. **`code/Game/Action/performwork.cpp:231-234`** — a `_DEBUG` name→pointer
   diagnostic table, `{ "Serializer_Ctor", (const void*)g_fn_Serializer_Ctor }, …`,
   followed by `if (!d.p) fprintf(stderr, "[PerformWork] UNRESOLVED: %s\n", …)`.
   Delete the four door rows; keep the table and its other rows.
2. **`code/Game/Action/consumerawmaterial.cpp:85`** — a conjunct that ends a
   multi-line chain on its own line (`g_fn_Serializer_GetVersion;`). Deleting it
   leaves the previous line ending in `&&`. This is exactly the §2.2 trap.

### 2.8 Guards inside macros and lambdas
A guard inside a lambda body follows the same rules. A guard inside a multi-line
`#define` does not: deleting a line there can eat the previous line's `\`, so if you
find one, edit it by hand and re-read the whole macro afterwards. (None was found in
the shape census, but the census only classifies lines it can see as statements.)

---

## 3. How to PROVE a pointer is unconditionally bound before deleting its guard

You are not allowed to delete a readiness guard because "it always passes in
practice". The proof has three parts and all three must hold for the door in hand:

1. **The door has ZERO CALLs.** Not zero greps — zero CALLs by §5's binding-aware
   sweep. Comments, strings, typedefs and the pointer's own definition are not
   callers; a call inside a `printf` argument IS a caller.
2. **The target is a real C++ member now.** The guard protected a *call through a
   resolved pointer*. Once the call is `ser->Serialize(x, name)` on a real
   `IO::Serializer`, there is no pointer in the expression at all: the guard is
   testing something the call no longer reads. This is the actual argument — it does
   not depend on the bind succeeding, which is why it is the one to lean on.
3. **The bind cannot fail silently in a way that matters.** For the record, and for
   any site where you are tempted to keep a guard: `standalone/address_registry.cpp`
   binds the fourteen `0x140152xxx` Serializer rows (B34_RULES §6.5(g)) and
   `ResolveAddr` consults the port registry FIRST, so in the standalone these
   resolve to the ported bodies. The gate additionally requires `[RtResolve] 0
   misses` on every run. But note the failure mode that DOES exist: a MISSPELLED
   mangled name makes `ResolveExport` return null silently — that is how
   `completeroad.cpp`'s `s_fn_Serialize_UChar` was permanently 0 (`"AEAuchar"` is
   not a valid mangled fragment) and the file's own comment records it. That is an
   argument for deleting these doors, not for keeping their guards.

If a site fails (1) or (2), **leave it and list it**. A shorter honest commit beats
a risky one.

---

## 4. What must NOT be touched

1. **`RegisterDispatchSlot` / `standalone/address_registry.cpp` / `standalone/reg_*.cpp`
   rows.** The `0x140152xxx` addresses are PE import slots that unported,
   retail-shaped exe code still reads. They do not retire when a caller converts.
   `python tools/dispatch_slot_map.py --check` must stay at **0 mismatches / 109
   ResolveExport registrations**.
2. **The twelve ExternalPtr exe doors and their caches.** `fn_ExternalPtr_Serialize`,
   `_Serialize2`, `_Serialize3`, `_SerializeTutorial`, `_BuildSerialize`,
   `fn_SerializeExternalPtr`, `fn_ExternalRefArray_Serialize`,
   `fn_ExternalPtr_Serialize_GraphicsMesh`, `_Weather`, … and the 125 sites through
   them are exe-phase work. `fn_ExternalPtr_Serialize3` is the last reader of exe
   `0x140007140` and explicitly STAYS.
   **The misnamed ones are the trap**: `s_fn_IBP_Serialize` in
   `code/Game/Work/cleararea.cpp` and `code/Game/Component/ambientemittercomponent.cpp`
   binds the ExternalPtr EXE doors `0x140002340` / `0x1400019a0`, not
   `InternalBasePtr::Serialize`. **A static's NAME and its COMMENT prove nothing** —
   `s_fn_Serialize_Char` binds the uchar door; `s_fn_SerSChar` in
   `gamegrid_serialize.cpp` is commented uchar `0x140152418` and binds the schar
   export. Resolve by BINDING, every time.
3. **Non-Serializer caches that share a resolver block or a scratch local with a
   Serializer one.** `moveto.cpp` rebinds one `uintptr_t iat` **ten times**; a
   whole-file closure over it mis-binds `s_fn_IsDerivedFrom` as a Serializer door.
   Resolve **positionally**: the nearest preceding assignment wins, and a
   reassignment from an unrecognised source KILLS the binding. §5's scripts already
   do this — a naive closure reports 136 "door calls" on the same tree where the
   positional walk reports 5.
4. **`code/Game/Work/work_base_walkpos.cpp:86`** — a record COUNT, not a stream.
5. **`RtSystem_Time_Serialize`** (`+0x90` int64) — that is step 4b.
6. **`code/Game/Work/plantarea.cpp` and `plantorchard.cpp`** — `b34-closing` is
   fixing the aliased bool-array door there in commit 3 (retail-backed). Leave both
   files alone entirely; their caches retire in that commit or after it.
7. **`code/Game/Game/w2_takescreenshot.cpp`** — commit 4 takes it whole
   (`GetSaveDirectory` + `Exists` + the `s_ok` chain). Leave it alone.
8. **`dx11/src/buffers.cpp:271`** and the nineteen minted vtable rows / fourteen
   deleting-dtor registrations (B34_RULES §4.2: **none retires in this step**).
9. **`.inc` files are regenerated, never hand-edited.** `tools/rt_mint_gen.py --check`
   rewrites `standalone/rt_export_addrs.inc` spuriously every run — `git checkout --
   standalone/rt_export_addrs.inc` afterwards, every time.

---

## 5. The scripts, and how to use them

All in this directory; run them **from this directory**, they hard-code the worktree
root in `sweep.py:ROOT` — change that one line to your own worktree.

| script | what it answers |
|---|---|
| `sweep.py` | remaining door CALL sites, by binding, tree-wide. Pass A = named globals; pass B = file-local/scratch bindings resolved POSITIONALLY. This is the "am I done" check: it must print no new rows. |
| `zero.py` | every door pointer at ZERO calls, global and file-local, with per-file call counts. Your work list. |
| `occ.py [names…]` | every non-comment, non-string OCCURRENCE of a name, with file:line and the source line. Use this, not grep, to find the guards — and to prove a door is gone. |
| `bydir.py` | the §0/§1 tables: per-directory occurrence counts and per-directory local-cache file lists. Re-run to re-measure. |
| `shapes.py` | the §2 guard-shape census with six examples per shape. |

Pre-generated output is already on disk next to the scripts: `bydir.txt` carries the
**per-file local-cache list for every bucket** — file path, every door-cache name in
it and that name's call count — which is the per-agent work list; `shapes.txt` is the
shape census; `zero.txt` and `sweep_final.txt` are the state at `67035026`. Treat all
four as a starting point to RE-MEASURE, not as the answer: they predate your merge.
| `warn_ab.sh` | the warning A/B, **both flag sets** (§6). |

`sweep.py`'s lexer strips comments and string literals while preserving line numbers.
Its pass-B binding rules cover the six spellings that each cost a caller batch a miss:
`ResolveAddr(Module::Exe, <slot>)` on one line with the variable; the helpers
`RSlot()` / `ResolveSlotPtr<>()` / `ResolveSlot(...)` / `loadExref()` /
`ResolveFn<T>("mangled")`; `ResolveExport("…@Serializer@IO@@…")` by mangled name with
strings intact; the two-statement indirection through a bare `uintptr_t` scratch
local; plain aliases `s_fn_X = g_fn_Y;`; and globals bound in `engine_funcs.cpp` and
called from a game directory. **Spelling 5 (the plain alias) is the one that bit
batch D** — it left four calls in `cleararea.cpp` through `s_fn_Serialize_Bool`,
invisible to any grep for `g_fn_Serialize_Bool`.

### The removed-line scan — do this on your own final diff, not on your notes
`git diff -U0 | grep "^-"`, then classify **every** removed non-blank line as exactly
one of:
- a guard condition or conjunct,
- a pointer declaration / definition / `extern`,
- a `typedef`,
- a resolver statement (`ResolveAddr` / `ResolveExport` / `ResolveSlot` / the `sp =` read),
- a `_DEBUG` value print,
- a comment about one of the above.

Anything that does not fit one of those six is a behaviour change and must be called
out in the message or reverted. Commit 2a's scan classified all 111 removed lines
this way; do the same and quote the number.

Watch for **orphans the deletion exposes**: 2a deleted
`if (sp && *sp) fn_Ser_Slot_SerUShortArr_488 = …;` and had to delete the preceding
`sp = ResolveAddr(Module::Exe, 0x140152488);` too, because nothing read `sp` any
more. Same for `s_*_resolved` latches and for `ok` variables that become `true`.

---

## 6. Verification recipe, per commit

```
mingw32-make standalone-debug    # exit 0
mingw32-make standalone-perf     # exit 0 — THE REAL NDEBUG BUILD
mingw32-make unit                # exit 0
python tools/dispatch_slot_map.py --check       # 0 mismatches, 109 registrations
python tools/cutover_manifest.py --check        # 59 rows: 58 REQUIRED + 1 WITHHELD
python tools/rt_mint_gen.py --check             # UP TO DATE; then restore rt_export_addrs.inc
python tools/runtime_census.py                  # quote the IO row
bash warn_ab.sh [base-ref]                      # zero NEW warnings, BOTH flag sets
```

`standalone` and `standalone-debug` share `standalone/build/obj`, so running one
after the other compiles nothing — **`standalone-perf` is the real NDEBUG build**
and it has its own object dir.

**The perf-flag A/B is not optional, and here is the bug it catches.** Commit 1
hoisted a `GetCount()` call out of a `printf` argument list into a local (correctly —
the perf build strips printf arguments, so a call left inside one disappears). The
perf build then warned `unused variable 'modCount'`, because the *read* of the local
is what the stripping deleted. A debug-only A/B is blind to this whole class. The fix
is `[[maybe_unused]]` plus a comment saying the attribute is load-bearing — keep the
CALL, let the READ go. `warn_ab.sh` runs:

- debug: `-g -O0 -D_DEBUG`
- perf: `-O2 -g -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=1 -DBANISHED_STRIP_LOGS -include standalone/log_strip.h`

against `git show <base>:<file>` and the working tree, same flags both sides, and
prints any NEW warning. Every deleted `_DEBUG` print and every guard whose body was a
log is a candidate for a new unused-variable warning in the perf build.

**Do not run the game and do not take the game-dir lock.** Report your hash to the
coordinator; the coordinator gates the merged tree (3 routes + resave ×3 + live type
table) and runs `git diff --stat` against your declared file set before merging.

---

## 7. Order, and the one hard sequencing constraint

1. **A, B, C, D in parallel** — disjoint file sets, no shared file. Each lands its own
   commit and reports its hash.
2. **2d last, alone**: `engine/funcs/engine_funcs.{h,cpp}` — the §1 globals'
   `extern`, `typedef`, `= nullptr` definition, binding statement and `_DEBUG` print;
   the `SerializerEncodingVCall` inline helper and its comment; the
   `harvestarea.cpp` / `harvestorchard.cpp` `s_fn_Serializer_SerializeBoolArray`
   alias + typedef + "same fn, different usage" comment.

2d cannot run earlier: while any guard still reads the extern, deleting it does not
compile — which is the build proving the invariant for you, and is why the globals
are deliberately left standing through A–D rather than "cleaned up as we go".

After 2d, `zero.py` should report no global door at zero calls that still exists, and
`sweep.py` should still print only the rows `b34-closing` left deliberately (see the
commit-1 message's residual table).
