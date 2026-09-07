# Wave 4, Lane C — IO::Persistence

Following [PLAN.md](PLAN.md) §4.2 and §6, and the shape set by
[WAVE3_LANE_A.md](WAVE3_LANE_A.md). Lane C owns the 7 `IO::Persistence` exports our
tree calls, plus their layout.

Files owned and written by this lane:

| File | What |
|---|---|
| `runtime/types/io/Persistence.h` | new — `Persistence`, `SettingsMapNode` (partial layout) |
| `runtime/src/io/persistence.cpp` | the 5 ports + `RtIoPersistenceRegister()` |
| `docs/runtime_port/WAVE4_LANE_C.md` | this file |

No files outside that set were edited. See §6 for two stale comments this lane found
in `engine_funcs.h` but does not own.

---

## 1. Result

| | |
|---|---|
| Ported | **5 of 7** — `Get(int&)`, `Get(bool&)`, `Set(int)`, `Set(bool)`, `GetSavePath` |
| Held back | **2 of 7** — `ReadSettings`, `WriteSettings` (registry mutation, see §5) |
| Registration count | `RtIoPersistenceRegister()` returns **5** |
| Diff sites added | **5** — 3 full oracles (`Get(int&)`, `Get(bool&)`, `GetSavePath`), 2 shadow-via-readback (`Set(int)`, `Set(bool)`) |
| Measured in a run | **nothing yet** — same standing warning as WAVE3_LANE_A.md: every claim below is static analysis plus a clean `-fsyntax-only` compile and a clean `tools/abi_retslot_audit.py` (0 suspicious of 49 exports) run, not an observed agreement |

**Nothing in this lane has executed.** Read every claim as "this is what the DLL does
and this is what the port does," not "this was observed agreeing."

---

## 2. The seven functions, and which five are ported

All addresses read live in Ghidra 2026-09-06. Retail = `Runtime-x64.dll` (base
`0x180000000`).

| Export | Retail | Ported? | Shape |
|---|---|---|---|
| `?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z` | `0x1800b81e0` | yes | `Map_Find` + `strtol` |
| `?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z` | `0x1800b8290` | yes | `Map_Find` + `strcmp` vs `"true"` |
| `?Set@Persistence@IO@@QEAAXPEBD0H@Z` | `0x1800b80c0` | yes | `Format` + `Map_Insert` + refcount swap |
| `?Set@Persistence@IO@@QEAAXPEBD0_N@Z` | `0x1800b8380` | yes | same, value = `"true"`/`"false"` |
| `?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ` | `0x180007ad0` | yes | 4-instruction field read + AddRef |
| `?ReadSettings@Persistence@IO@@QEAAXXZ` | `0x1800b84b0` | **no** | full registry enumeration into the map |
| `?WriteSettings@Persistence@IO@@QEBAXXZ` | `0x1800b88e0` | **no** | full registry enumeration + stale-key deletion |

### Export names — verified, none phantom

`reference_resolveexport_phantom_names.md`: a `ResolveExport` name the DLL does not
contain returns NULL silently. All seven were checked against the shipped DLL's export
directory with `pefile` **before** any code was written, independently confirmed by the
orchestrator's wave-wide sweep of all 39 IO symbols
(`docs/runtime_port/wave4_export_names.txt` — cite that file rather than re-deriving
this table; reproduced here only for this lane's own record):

```
OK  0x1800b81e0 ord=2421  ?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z
OK  0x1800b8290 ord=2423  ?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z
OK  0x1800b80c0 ord=3817  ?Set@Persistence@IO@@QEAAXPEBD0H@Z
OK  0x1800b8380 ord=3818  ?Set@Persistence@IO@@QEAAXPEBD0_N@Z
OK  0x180007ad0 ord=2890  ?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ
OK  0x1800b84b0 ord=3462  ?ReadSettings@Persistence@IO@@QEAAXXZ
OK  0x1800b88e0 ord=4561  ?WriteSettings@Persistence@IO@@QEBAXXZ
```

`GetSavePath`'s retail address also matches the fallback our own tree already used at
`engine_funcs.cpp:2591` (`ResolveAddr(Module::Runtime, 0x180007ad0)`), which is
independent corroboration that this address and this mangled name name the same function.

`tools/export_name_audit.py` reports no mismatches for this lane's names.

**On the address gap (raised and then retracted by the orchestrator):** `GetSavePath`
at `0x180007ad0` sits far from the other six (`0x1800b80c0`-`0x1800b88e0`, Persistence's
own code), in the same low band as `FileSystem::GetDataDirectory`/`GetSourceDirectory`
(`0x180007bd0`/`0x180007b90`) — raising the question of whether it is folded (one body,
several exported names) with something else. Decompiled on its own terms (§3, and the
full body in §2 of the earlier draft of this doc): it is a self-contained 4-instruction
function (`*retval = 0; if (this->_savePath) { *retval = it; ++refcount; }`) with no
resemblance to `GetDataDirectory`'s body beyond both being trivial field-copy getters —
not folded with it or with anything else. The orchestrator independently confirmed this
by checking the whole export directory for addresses exported under more than one name
(0 of 38 Wave-4 addresses this wave touches are shared): the low-address placement is
linker/COMDAT ordering, not ICF. `tools/abi_retslot_audit.py` (0 suspicious of 49
by-value-returning exports) is the check that matters for this function's real risk —
the hidden RDX return slot, not its address neighborhood.

### Hidden return slots — one, and it was already handled correctly upstream

`GetSavePath` returns `System::String` **by value** (`?AV` in the mangling — a hidden
RDX return-slot pointer, returned again in RAX). `tools/abi_retslot_audit.py` run over
the whole ported surface (49 by-value-returning exports) reports **0 suspicious**, so no
`fn_*` typedef anywhere is missing its slot for this or any other export. The other four
manglings end `_N` (bool) or `X` (void) — no hidden slot.

**Pre-existing stale comment found on this exact function**, not introduced by this
lane — see §6.

---

## 3. The layout, and the evidence for it

Full commentary is in `runtime/types/io/Persistence.h`; both pinned offsets carry a
`static_assert` (one of them) and a cited instruction.

| Off | Field | Pinned by |
|---|---|---|
| `+0x00` | root of an in-memory `Set<Pair<String,String>>` | `Persistence::Get`/`Set` pass `this` itself (not `this+N`) as the map argument to `Map_Find`/`Map_Insert`, both of which read/write `*(void**)self` |
| `+0x90` | `System::String _savePath` | `GetSavePath` (`0x180007ad0`): `if (*(int64*)(this+0x90) != 0) { copy, ++refcount; }` — **independently corroborated** by `code/Game/Dialog/scenariodialog.cpp:502` and `code/Game/Tutorial/tutorial_next_scenario.cpp:294,311`, which write that same field directly (`fn_String_Assign((void*)(*(uintptr_t*)g_pPersistenceSingleton + 0x90), …)`), bypassing `Set` entirely |

Offsets `+0x08`..`+0x8F` are **explicitly not modeled** — nothing this lane ported
touches them, and `ReadSettings`/`WriteSettings` build their own `Registry` helper on
the stack rather than reading it from `this`, so nothing in this lane's decompiles
evidences what lives there. A future lane extending this struct must repin it, not
assume the gap is empty.

### The settings map's node, and why Map_Find/Map_Insert are called by address

`Map_Find` (`0x18002ace0`) and `Map_Insert` (`0x18002a090`) are **not Persistence-specific**
— they are two of the whole DLL's shared `Set<Pair<String,String>>` internals, folded
into one instantiation by MSVC because both key and value are 8-byte `String` handles
regardless of the conceptual map (`Map_Find` has 36 xrefs, `Map_Insert` has 8, across
classes far outside `IO`). This is `Collection::Set`, a class no lane has ported yet
(PLAN.md §3.3, §4.3 risk 1). Reimplementing AVL find/insert/rebalance by hand for this
lane alone would be out of scope for a 7-function IO lane and would repeat exactly the
"one BST body cannot serve two node layouts" trap `WAVE3_LANE_A.md` and
`reference_one_bst_find_two_node_layouts.md` both warn about. Instead, this lane calls
both **by address**, exactly as retail's own `Persistence::Get`/`Set` do — PLAN §6 step 3
("resolve deps"), not step 4 ("port"). If a later wave ports `Collection::Set` and
registers these two addresses, `ResolveAddr` is registry-aware, so this file's calls
transparently pick up that port for free.

Decompiling `Map_Find`/`Map_Insert` did yield one measurement worth recording: the node
is `{ String key @+0x00; String value @+0x08; }`, then (unread by this lane, taken on
faith from the BST descent in `Map_Find`/`WriteSettings`) parent/left/right pointers —
matching WAVE3_LANE_A.md's general formula `parent @ align8(sizeof K + sizeof V)`,
i.e. `+0x10/+0x18/+0x20` for two 8-byte `String`s. `runtime/types/io/Persistence.h`'s
`SettingsMapNode` models only the prefix this lane actually reads/writes.

### The key encoding — bare concatenation, no separator

All four `Get`/`Set` retail bodies build the map key as
`String::Format(&key, "%s%s", section, key)` — literal concatenation, no separator. So
`Get("Game", "StatusIconEnable", …)` looks up the in-memory key `"GameStatusIconEnable"`.
This matches, independently, the live registry value name this project's own memory
note (`project_debugmenu_combos_and_statusicon_setting.md`) found on the test machine:
`HKCU\Software\Shining Rock Software LLC\Banished\GameStatusIconEnable`, a **string**
`"true"`/`"false"`. `ReadSettings`/`WriteSettings` (not ported) are what makes that
registry value and this in-memory key the same key: `ReadSettings` populates the map
directly from `RegEnumValueW` value names, `WriteSettings` writes the map's keys back
out the same way — this lane's `Get`/`Set` only ever touch the map, never the registry
API, which is exactly why they can be ported without touching Win32 at all.

### The bool encoding — exact string compare against `"true"`

`Get(bool&)`'s retail body (`0x1800b8290`) is an inlined `strcmp`: walk both strings
byte-for-byte until a mismatch or a NUL, then diff the two bytes at the stopping
position — functionally `strcmp(value, "true") == 0`, case-sensitive, and `"false"` (or
anything else) reads as `false`. `Set(bool)` writes exactly one of the two literals
`"true"`/`"false"` via `Format("%s", …)`. `RetailValueIsTrue()` in `persistence.cpp`
implements this.

### The int encoding — decimal via CRT `strtol`, not an engine call

`Get(int&)`'s retail body tail-jumps `FUN_18007f034`, which is itself a 3-instruction
tail-jump straight to CRT `strtol(s, 0, 10)` — not an engine function at all. The port
calls `std::strtol` directly rather than resolving anything from the DLL for this piece.
`Set(int)` writes the decimal form via `Format("%i", value)`.

---

## 4. What each diff site can and cannot prove

| Site | Kind | Proves | Does not prove |
|---|---|---|---|
| `Get(int&)` | **full** `RT_DIFF_SCALAR` | found-flag AND value, packed into one scalar, one call to the DLL's own `Get(int&)` | — |
| `Get(bool&)` | **full** `RT_DIFF_SCALAR` | found-flag AND value, packed the same way | — |
| `GetSavePath` | **full**, `RT_DIFF_SHADOW`-shaped | both sides really run; string CONTENTS compared (not pointer identity), the temp the oracle call allocates is released | — |
| `Set(int)` | **partial** (readback shadow) | our write installed a value the DLL's OWN already-proven `Get(int&)` can read back correctly | the exact byte sequence written into the map node (refcount bookkeeping is exercised but not independently verified byte-for-byte) |
| `Set(bool)` | **partial** (readback shadow) | same, via `Get(bool&)` | same caveat |

### Why `Get`/`GetSavePath` can be full oracles and `Set` cannot

`Get`/`GetSavePath` are read-only apart from a transient `String::Format`/`MemFree` pair
that touches no persistent state — the brief's WARNING 3 does not apply, so "call ours,
call theirs, compare" is exactly the Wave 1/3 shape. `Set` **mutates the shared live
map** both implementations would touch; calling the DLL's real `Set` a second time would
insert the value twice (WARNING 3, `RT_DIFF_SHADOW` is "the tool Wave 3 used for exactly
this"). Instead of a fabricated "shadow" that predicts state without checking it, this
lane's shadow makes a REAL, side-effect-free call into the DLL's own `Get` — a function
this same file already proves is a full oracle — to verify the map now holds a
retrievable, correct value. That is a real correctness check, not a formality: it would
catch a wrong key encoding, a wrong `Map_Insert` argument order, or a refcount bug that
corrupted the stored value, without ever calling the DLL's `Set`.

### `GetSavePath`'s comparison intentionally is not `RT_DIFF_SCALAR`

Two `String`s can hold equal *content* at different addresses (each side got its own
ref-counted copy), so pointer equality is the wrong test and `RT_DIFF_SCALAR`'s
`operator==` would use it. The site uses `RT_DIFF_SHADOW` for its free-form description
strings even though — unlike the canonical "no call to the original" `RT_DIFF_SHADOW`
use — it DOES call the original for real; `WAVE3_LANE_A.md`'s `BaseTypeInfo::BaseTypeInfo`
site set this precedent ("field-by-field, never `memcmp`") for the same reason.

---

## 5. Why `ReadSettings`/`WriteSettings` are held back

Both are enormous relative to the other five (`ReadSettings` alone: `RegEnumValueW` over
every registry value, `WideString`↔UTF8 conversion per entry, and a `Map_Insert` per
value; `WriteSettings` additionally walks the in-memory tree in sorted order, tracks
which registry values no longer exist in the map using a growable array it reallocates
inline, and calls `RegDeleteKeyW` for each one). Three reasons, together:

1. **No safe oracle exists.** Both mutate the real `HKCU\...\Banished` registry key.
   Running "ours" and "theirs" back to back would enumerate, write and (for
   `WriteSettings`) *delete* registry values twice — the same class of problem as `Set`,
   except unbounded in scope (`Set` touches one key; `WriteSettings` touches all of them,
   including ones nothing here inserted this session) and, unlike `Set`'s in-memory map,
   not confined to process memory that resets on the next run. A readback-shadow
   equivalent to `Set`'s would still have to perform the real deletions to be meaningful.
2. **The internal logic is not fully recovered.** `WriteSettings`'s stale-key deletion
   walks a `DynArray`-style buffer it grows with its own bespoke policy
   (`capacity = needed` first growth, `*3` after — the same policy WAVE3_LANE_A.md
   measured for `Factory::Startup`'s child array, but here driving `RegDeleteKeyW`
   arguments rather than a read-only tree) and this lane did not verify every branch of
   it byte-for-byte. Porting it "byte-faithful first, readable second" without full
   confidence in every branch is exactly the risk PLAN §4.3 risk 5 gestures at, applied
   to something that deletes user data instead of crashing a process.
3. **The cost of being wrong is qualitatively different.** Every other function this
   wave and the three before it ported fails, at worst, by crashing `reimpl-x64.exe` —
   annoying, loud, and bounded to a debug session. A wrong `WriteSettings` can silently
   delete or corrupt entries in the player's real Windows registry the moment
   `BANISHED_RT_PERSISTENCE=1` is set and the game exits normally. CLAUDE.md's own
   testing rules for this lane explicitly forbade writing to the registry during
   development for the same reason.

Both are left on the DLL, called exactly as before through
`fn_Persistence_ReadSettings`/`fn_Persistence_WriteSettings` in
`code/Game/Dialog/optionsdialog.cpp` (Options Apply/Cancel, on-open) and
`code/Game/Game/game_main.cpp` (boot/shutdown, via `GameSettings_Load`/`_Save` which are
a *different*, already-ported layer — see §2 and §7). This is safe precisely because
this lane's five ports never own the map's storage; they mutate the DLL's own live
singleton in place, the same "one heap, one metadata set" argument PLAN.md §4.3 risk 2
made for the allocator. `ReadSettings` populating the map and this lane's `Get`
reading it back are the same map either way.

---

## 6. The singleton

`?_singleton@Persistence@IO@@0PEAV12@EA`, retail `0x1801fc6b8`, ordinal 4816 — **an
exported data symbol**, confirmed with `pefile` against the shipped DLL:

```
0x1801fc6b8  ord=4816  ?_singleton@Persistence@IO@@0PEAV12@EA
```

This closes one of the two open questions `WAVE3_LANE_A.md` §10 left for this lane
("Lanes B and C should check whether the singletons they need ... are likewise
exported"). It also confirms `PLAN.md` §2.3's row for this address (already correct
there) and extends the running tally of exported-but-undocumented globals: while
checking, this lane also found `?_singleton@InternalFactory@Object@@0PEAV12@EA`
(`0x1801fc6d8`) and `?_singleton@ExternalFactory@Object@@0PEAV12@EA` (`0x1801fc6c8`)
sitting immediately adjacent in the export table — both exported too, answering the
other half of WAVE3_LANE_A.md §10's open question, for whichever lane owns those
classes. `engine/funcs/engine_funcs.cpp:1788-1789` already resolves the Persistence
singleton via `ResolveAddr(Module::Runtime, 0x1801fc6b8)` rather than by name; this
lane did not change that (both resolve to the same address; the export merely means a
by-name resolve was also available and was not needed).

This lane's ports are called with `self` = whatever the caller already resolved through
`g_pPersistenceSingleton` — they never read or cache the singleton themselves.

---

## 7. Comments to correct (PLAN §6 step 8) — outside this lane's ownership

Two edits, both minimal, both in `engine/funcs/engine_funcs.h`, which the brief
restricts to the orchestrator (shared with concurrently-writing lanes):

1. **`engine_funcs.h:1242-1246`** — the comment and mangled name are stale in exactly
   the way `engine_funcs.cpp:2575-2578`'s own neighboring note already diagnoses for a
   sibling function (`FileSystem::GetDataDirectory`): it claims `GetSavePath` returns a
   `Filename` and is non-const.

   Current:
   ```cpp
   // IO::Persistence::GetSavePath(this, retval) -> Filename (non-trivial return ABI)
   // ?GetSavePath@Persistence@IO@@QEAA?AVFilename@System@@XZ
   // MSVC x64 member: RCX=this, RDX=hidden_retval (Filename buffer)
   typedef void* (*fn_Persistence_GetSavePath_t)(void* self, void* retval);
   ```
   Replace with:
   ```cpp
   // IO::Persistence::GetSavePath(this, retval) -> System::String (non-trivial return ABI)
   // ?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ
   // MSVC x64 member, const: RCX=this, RDX=hidden_retval (String buffer). Was
   // misdocumented as returning Filename via a non-const QEAA mangling that is not
   // in the DLL's export table; engine_funcs.cpp:2583-2591 already resolves and
   // falls back to the CORRECT name/address (retail 0x180007ad0) — only this
   // comment and the two identifiers in it were stale. See
   // docs/runtime_port/WAVE4_LANE_C.md §2-3.
   typedef void* (*fn_Persistence_GetSavePath_t)(void* self, void* retval);
   ```
   (The typedef itself was already correct — `void*` return, `(self, retval)` args —
   nothing there needs to change, only the two comment lines and would-be mangled-name
   string above it.)

2. **`engine_funcs.cpp:2575-2578`** (the block comment right above the working code) is
   *already correct* and should NOT be touched — it is the note that this lane's
   finding confirms, not contradicts. Flagging it here only so the orchestrator does not
   mistake it for the same defect as (1).

Neither of these is a functional bug: the code at `engine_funcs.cpp:2583-2591` already
resolves the right name with the right fallback address. Only the doc comment at the
typedef site (1) was wrong, and it was wrong in the specific way PLAN §6 step 8 warns
about — "a comment that was right about the mechanism but wrong about" a detail,
outliving the fix placed a few hundred lines away from it.

---

## 8. Faithfulness decisions

* **No null guards added.** `Map_Find`/`Map_Insert` are called and dereferenced exactly
  as retail does; an unregistered/empty map produces the same `nullptr` retail's own
  `Get` would see (returns "not found"), which is retail's actual designed behavior for
  a missing key, not a fault path.
* **One documented deviation** (`RetailValueIsTrue`, `persistence.cpp`): retail's
  `Get(bool&)` computes an out-of-bounds read (`value[-1]`) if a stored value's
  `String::data` is `nullptr` — reachable by no known write path, since `Set(bool)`
  always formats a real (if zero-length) buffer. CLAUDE.md's "don't fix a faithful
  crash" rule targets retail *relying* on a value always being present; this is the
  opposite — an OOB read retail does not rely on, that nothing can currently trigger.
  The port treats a null value as `false` rather than reproducing undefined behaviour
  for a state nothing can create. Flagged loudly in the source in case a future
  `ReadSettings` port (or any other writer of the map) is ever found to produce a
  null-data value — at that point this decision needs revisiting, not silently trusting.
* **`Persistence::Set`'s asymmetric release-then-adopt is transcribed exactly**, not
  simplified: on a brand-new map key, `Map_Insert`'s own internal refcounting already
  leaves the node holding our formatted value (the `node->value.data != valStr.data`
  check is false, and the branch that would double-free or double-adopt is skipped); on
  an existing key, `Map_Insert` leaves the old value in place and `Persistence::Set`
  itself performs the release/adopt. Collapsing this into an unconditional overwrite
  would double-free the old value's refcount on the new-key path.
* **`String::Format` is resolved from the DLL directly (`ResolveAddr`, not
  `ResolveExport`)**, deliberately, even though Wave 2 (lane String) already ported it.
  This keeps the lane correct standing alone against the DLL rather than silently
  depending on a sibling lane's port being enabled; the two are meant to agree, so this
  costs nothing in practice, but it means `BANISHED_RT_SYSTEM=0` cannot ever change this
  lane's behavior.

---

## 9. Registration

`RtIoPersistenceRegister()` registers each of the five ported functions **by name and by
address**. The address registrations matter for the same reason `WAVE3_LANE_A.md` §6
gives: `engine_funcs.cpp:1791-1802` already has an `if (!fn) ResolveAddr(Module::Runtime,
0x1800b8380 / 0x1800b80c0)` fallback for `Set(bool)`/`Set(int)`, and `:2585-2591` has one
for `GetSavePath` (`0x140151d38` exe slot, then `0x180007ad0` Runtime address) —
registering only the export names would leave any site whose name resolve silently
failed on the DLL, exactly WAVE2 finding 4 / WAVE3_LANE_A.md §6's warning.

No detour is installed. `ReadSettings` (not ported) still populates the map the DLL's
own internal code, and this lane's ports, both read; `WriteSettings` (not ported) still
flushes it. That is safe under the same "one heap, one metadata set" argument as the
allocator (PLAN.md §4.3 risk 2): this lane's ports never own the map's storage, only
manipulate the live DLL singleton's map in place.

Kill switches: `BANISHED_RT_IO=0` (whole namespace, other lanes' too),
`BANISHED_RT_PERSISTENCE=0` (this lane only). Both already existed in `rt_io_port.cpp`
(not edited by this lane).

---

## 10. What the integration run must look for

1. **`calls=` before `divergences=`**, per lane convention. Expected non-zero call
   counts on a normal `sa-run` autostart session:
   - `Get(int&)`: **10** at boot (`GameSettings_Load`, `code/Game/Game/game_main.cpp:669`)
   - `Get(bool&)`: **6** at boot, same call
   - `Set(int)`/`Set(bool)`: **10**/**6** only if the session reaches a clean shutdown
     (`GameSettings_Save`, `game_main.cpp:882`) or opens Options and hits Apply
     (`optionsdialog.cpp:1244`) — a session killed mid-run (or via a crash) will show
     `calls=0` here and that is NOT evidence of a broken registration, unlike the other
     three sites.
   - `GetSavePath`: reached from `code/Game/Dialog/scenariodialog.cpp:498` and
     `code/Game/Tutorial/tutorial_next_scenario.cpp:289` — only on the saved-scenario /
     tutorial-resume branch, so a plain `sa-run` with `BANISHED_AUTOSTART=1` (new game,
     not a scenario resume) may legitimately show `calls=0` here too. Use
     `BANISHED_AUTOTUTORIAL` to exercise it (this is the exact branch
     `engine_funcs.cpp:2575-2582`'s comment says "every tutorial takes").
2. **`Set(int)`/`Set(bool)`'s shadow verdict (`equal=`) is the whole proof for those two
   sites** — there is no `divergences=` against the DLL's own `Set`, by design (§4). A
   `false` verdict here means the map write is wrong; read it as seriously as a
   `RT_DIFF_SCALAR` divergence anywhere else in the project.
3. **`StatusIconEnable` end to end.** This project has a standing, previously-shipped
   defect in exactly this area (`project_debugmenu_combos_and_statusicon_setting.md`):
   reading the setting through the wrong overload silently hid every status icon with
   nothing logged. `GameSettings_Load`'s own diagnostic print
   (`code/Game/Game/game_settings_persist.cpp:115-119`) already reports the byte read at
   `settings+0xC`; confirm it still reports the correct value with
   `BANISHED_RT_PERSISTENCE=1` and that status icons still render in-game. A silent
   regression here would look identical to the original bug.
4. **A crash inside `reimpl-x64.exe` (not `Runtime-x64.dll`) touching `Map_Find`/
   `Map_Insert`'s resolved addresses** would mean the `ResolveAddr(Module::Runtime, …)`
   calls in `persistence.cpp` landed on the wrong bytes — symbolize with `nm`/`addr2line`
   against `Runtime-x64.dll` at the ASLR-adjusted address, not the exe.
5. **Perf.** All five ports are cold-path (boot/shutdown/options/scenario-resume), not
   per-frame, so this lane is not expected to move the PLAN §5 5% gate at all — a
   measurable delta here would itself be a finding worth reporting, not expected noise.
