# Wave 9 — Lane MOD: the `IO::Modification` mods-menu cluster, reached BY ADDRESS

Produced 2026-09-08 on a worktree of `wave/upgrade-path-complete` (base `4ef55b6`).

This lane is the only one in Wave 9 whose section was **not** found by a name-based
instrument. It came out of the address-door census
(`docs/runtime_port/wave9_unregistered_addr_doors.csv`, `ce1d8e9` corrected by `4ef55b6`):
every `ResolveAddr(Module::Runtime, 0x…)` in the tree minus every address anything
registers. 172 call sites, 62 unregistered, **eleven of them the mods cluster**, all
resolved in `engine/funcs/engine_funcs.cpp`.

---

## Verdict line for each of the eleven

| # | address | what it is | verdict |
|---|---------|------------|---------|
| 1 | `0x18000da90` | `?GetCount@Modification@IO@@QEBAHXZ` | **PORTED + BOTH DOORS.** LIVE (4 call sites: `moddialog.cpp` ×2, `mod_dialog_vtables.cpp`, `saveloaddialog.cpp`). Full oracle. |
| 2 | `0x18000da80` | `?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z` | **PORTED + BOTH DOORS.** LIVE (8 call sites across `moddialog`, `moddetailsdialog`, `saveloaddialog`). Full oracle. The `MOVSXD` is load-bearing — see §4. |
| 3 | `0x1800aeab0` | `?Enable@Modification@IO@@QEAAXH_N@Z` | **PORTED + BOTH DOORS.** LIVE (the mod checkbox, `moddialog.cpp:562`). Full BYTE oracle by restore-and-replay. |
| 4 | `0x1800aeb20` | `?Swap@Modification@IO@@QEAAXHH@Z` | **PORTED + BOTH DOORS.** LIVE (3 call sites: move-up, move-down, move-to-top). Shadow oracle by value, 3 independent terms. |
| 5 | `0x1800b05f0` | `?SaveList@Modification@IO@@QEAAXXZ` | **PORTED + BOTH DOORS.** LIVE (`moddialog.cpp:710`, and again by NAME from `game_main.cpp:363`). Restore-and-replay oracle on the `order` column. |
| 6 | `0x1800101e0` | `Mod::Mod(const Mod&)`, unexported | **PORTED + ADDRESS DOOR.** The `fn_ModEntry_CopyCtor` pointer in `engine_funcs.cpp` is **DEAD** (§2), but the address is genuinely live *inside* `Swap` and `Array<Mod>::operator=`, both of which this lane now owns. |
| 7 | `0x180026ef0` | `Mod::operator=`, unexported | **PORTED + ADDRESS DOOR.** Same story as #6. |
| 8 | `0x180011460` | `Mod::~Mod()`, unexported | **PORTED + ADDRESS DOOR.** Same story as #6. |
| 9 | `0x180010110` | `Array<Mod>::operator=`, unexported | **PORTED + ADDRESS DOOR.** The `engine_funcs.cpp` pointer is dead, but `runtime/src/engine/loops.cpp:681` calls this address on **every world load** (the inlined `EnableSavingSettings(false)` in `Loader::Load`). This is the live path that exercises #6–#8. |
| 10 | `0x18002a090` | the string-keyed map insert, unexported | **HELD BACK — and not this lane's.** LIVE, and this lane CORRECTS the tree's existing note about it. See §5. |
| 11 | `0x1800af5f0` | `?RefreshList@Modification@IO@@QEAAXXZ` | **HELD BACK on a NAMED dependency: `IO::PackageFile`,** which is the current cutover wall. See §6. |

**9 ported, 2 closed with a written reason.** Nine more unexported container internals
were ported as dependencies and registered by address (§3).

---

## 1. Measurements

Every number below was produced by running the *same* script on the tree with and without
this lane's changes, so the denominator is comparable.

### The two live runs (DLL present, `BANISHED_RT_DIFF=1`)

**Run 1 — the front end, driven unattended.**
`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1000,1001,3000,2001,4001,101,m6
BANISHED_MODAL_ANSWER=1` — toggle mod 0, toggle mod 1, move 0 down, move 1 up, move 1 to
top, Ok (which is `ModDialog::Apply` → `SaveList`), quit.

```
[RtDiff]   IO::Modification::GetCount           calls=8    divergences=0
[RtDiff]   IO::Modification::GetModification    calls=17   divergences=0
[RtDiff]   IO::Modification::Enable             calls=2    divergences=0
[RtDiff]   IO::Modification::Swap               calls=3    divergences=0
[RtDiff]   IO::Modification::SaveList           calls=2    divergences=0
standalone_crash.log: 0 bytes
```

**Read the `calls=` column, not just `divergences=`** — a port with no callers reports clean
(`reference_uncalled_port_is_not_verified.md`). Every one of the five ran, and the counts
are their own effectiveness control: `Enable calls=2` is exactly the two checkbox clicks,
`Swap calls=3` exactly the move-down, move-up and move-to-top clicks. Nothing here reported
green by not executing.

**Run 2 — autostart to gameplay, same two mods mounted.** World built, ran 721 frames,
clean staged shutdown, `standalone_crash.log` 0 bytes. `Engine::Loader::Load calls=6,
divergences=0` — that is the body which toggles `_savingEnabled` and therefore drives this
lane's `Array<Mod>::operator=` and, through it, the `Mod` copy constructor over both real
mods. See §7 for exactly how much that does and does not prove.

### THE POPULATION — why a two-mod list is the minimum, not a nicety

The mods menu had **two mods mounted: `betterFields.pkm` and `DebugMenu.pkm`**
(`[ModDialog] opened + AddDialog'd (dlg=… mods=2)`). They were copied in from
`WinData/_mods_off/` for the two runs and **moved back immediately afterwards**, and the
integrator warned the other lanes that the package set was temporarily different so nobody
would attribute a difference to their own port. `WinData/*.pkm` is back to zero.

This is not housekeeping. **With `_mods_off` as found, `Modification::count` is 0 and every
one of `GetModification`, `Enable`, `Swap` and `SaveList` would have reported
`divergences=0` while doing nothing at all** — `SaveList`'s loop would not execute,
`Swap`'s three clicks would index nothing, and the lane would have shipped on a green line
that meant "never ran". That is the trap
`reference_uniform_population_is_not_a_verification.md` records, where a ported inline
`strcmp` survived four waves of oracles because 13 of the 14 values in the population were
the same value.

The threshold is specific to what is being verified. `Swap`'s entire post-condition is that
**two records exchanged contents**, which is unstateable at n=0 and *vacuous at n=1*: with
one mod, move-up, move-down and move-to-top all early-out on retail's own `a == b` test and
`calls=3, divergences=0` would still be printed. Two mods with different names, different
`.pkm` paths and different enabled bytes is the smallest population in which the swap can be
wrong and be *seen* to be wrong.

### Which exe actually ran — the acceptance test is the log, not the md5

`C:/GOG Games/Banished` is shared and six other lanes were deploying into it. **Three
consecutive attempts were clobbered in the seconds between the `cp` and the launch**, and
the first of them was nearly banked: the run completed, exited 0, and produced a full
`[RtDiff]` summary with no divergences — from *another lane's binary*. The on-disk md5 does
not settle it either, because a lane can overwrite the file the instant our process exits;
the post-run md5 differed from ours on a run that really was ours.

The test that works is the **run's own log**: `[RtPort]   IO::Modification (lane MOD): 5
exports + 14 addresses`, checked 25 seconds after launch, with a kill-and-retry if it is
absent. Both runs quoted above passed it. Stated because
`reference_lane_edits_after_report_invalidate_the_measurement.md` is about the same class of
error and this is a sharper instance of it: a clobbered deploy does not produce an error, it
produces a *clean measurement of someone else's code*.

### Address-door census — the instrument that created this lane

```
                    before (4ef55b6)   after
called                    172            172     <- DENOMINATOR UNMOVED
literal-registered        508            528
mint-registered           504            504
UNREGISTERED               62             52
```

Set-diff, exactly as lane FIND's standard requires (unmoved total **plus** the set):

```
REMOVED: 0x180009c30 0x18000da80 0x18000da90 0x180010110 0x1800101e0
         0x180011460 0x180026ef0 0x1800aeab0 0x1800aeb20 0x1800b05f0
ADDED:   (none)
```

Ten removed, zero added. Nine are from the lane's eleven; the tenth, `0x180009c30`
(the set-node free), was an unregistered door in a *different* file that this lane's
dependency closure closed for free.

The concentration table moves `22 → 13` for `engine/funcs/engine_funcs.cpp`, which is the
per-file form of the same nine.

### `tools/runtime_census.py` — post-`29cfaa2` numbers

```
              before          after
IO           85 / 55  64.7%   85 / 60  70.6%
internal     81 / 41  50.6%   81 / 46  56.8%
TOTAL       958 / 665 69.4%  958 / 675 70.5%
```

`called` is **unmoved in every row** (958, 85, 81) and `ported` is +10. This lane did not
grow the cutover surface, which is not automatic — a faithful port that calls DLL internals
by address normally does (`reference_porting_can_grow_the_cutover_surface.md`). It did not
here because the internals it reaches were *already* in the tree's `called` set or are
reached only by direct C++ call from this file.

**Be precise about the +10 vs the 14 registrations.** Five are the named exports; five are
internals the census already counted as `called`; the remaining nine registered addresses
(`0x180010b80`, `0x180011110/1a0`, `0x180011220/2a0`, `0x1800091f0`, `0x1800098d0`,
`0x180010570`, `0x180009c30`) are reached from this file by ordinary C++ call and would
work at cutover with no registration at all. They are registered anyway so a DLL-internal
caller reached through a future detour, or a later lane that finds the address, lands on
the same body rather than on a second copy.

### The other audits

```
tools/abi_retslot_audit.py     0 suspicious of 48 by-value-returning exports
tools/export_name_audit.py     662 names, 1 not in the DLL -- the pre-existing
                               "?_singleton@..." placeholder in a COMMENT in
                               rt_resolve_cache.cpp:8. 0 real phantoms.
tools/port_bypass_audit.py     ADDRESS door: 211 sites, 0 bypasses
                               NAME door:    433 sites, 0 bypasses
                               ADDR-REGISTERED 38 (was 24)
```

All nine mangled names this lane spells were checked against `Runtime-x64.dll`'s export
directory with `pefile` **before a line was written**, not after:

```
OK  ?GetCount@Modification@IO@@QEBAHXZ
OK  ?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z
OK  ?Enable@Modification@IO@@QEAAXH_N@Z
OK  ?Swap@Modification@IO@@QEAAXHH@Z
OK  ?SaveList@Modification@IO@@QEAAXXZ
OK  ?_singleton@Persistence@IO@@0PEAV12@EA
OK  ?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z
OK  ?Set@Persistence@IO@@QEAAXPEBD0_N@Z
OK  ?Set@Persistence@IO@@QEAAXPEBD0H@Z
```

---

## 2. FIVE OF THE ELEVEN POINTERS ARE DEAD — the `fn_MemFree` shape, again

The brief warned to check for the never-assigned-pointer class before assuming a null is a
cutover problem. The mods-menu closeout found `fn_MemFree` was never assigned
project-wide. **The same shape is here in a mirrored form, five times over.**

`fn_ModEntry_CopyCtor`, `fn_ModEntry_Assign`, `fn_ModEntry_Dtor`, `fn_Map_Insert` and
`fn_ModArrayCopy` are **resolved and never called**. Grepping each identifier across
`code/`, `engine/`, `runtime/`, `standalone/` and `dx11/` returns, for every one of them,
exactly three lines: the definition at `engine_funcs.cpp:139-162`, the `ResolveAddr` at
`engine_funcs.cpp:1713-1747`, and a `printf` of the pointer value in the diagnostics dump
at `engine_funcs.cpp:3781-3793`. There is no fourth line. Nothing calls them.

That materially changes the triage:

* **As `fn_*` pointers, all five are inert at cutover.** A null stored in a pointer nothing
  reads is not a defect, and "register it so the pointer is non-null" would have been
  cargo-cult work.
* **But two of the five ADDRESSES are live through a different door.** `0x180010110` is
  called from `runtime/src/engine/loops.cpp:681` on every world load, and `0x18002a090`
  from `runtime/src/io/persistence.cpp:59` and
  `runtime/src/io/persistence_registry_win.cpp:266`. The dead `fn_*` pointer and the live
  address are the same row of the CSV, because the census keys on the address.

**The rule this is worth carrying:** the address-door census counts *doors*, not *calls*.
A row can be simultaneously a dead pointer and a live dependency, and only grepping the
identifier — not the address — tells the two apart. Doing only the address grep would have
made this lane report five urgent ports; doing only the identifier grep would have made it
report five dead rows and miss `Array<Mod>::operator=`, which runs on every load.

---

## 3. What was ported, and the layout it rests on

`runtime/src/io/modification.cpp` (new), `runtime/types/io/Modification.h` (completed),
one line in `runtime/src/io/rt_io_port.cpp`.

The Wave 4 header modelled three fields of `Mod` — enough for `Find`, which only strides
the array. The lifecycle triple walks **every** member, so the header is now complete, and
every offset in it was read out of the copy constructor, which visits the members in
declaration order and whose element helper names the type of each:

```
+0x00/08/10  three refcounted System::String   (adopt/release idiom)
+0x18..0x2c  six int32, copied field by field
+0x30        Collection::Array<int32>          (op= 0x180011110, stride 4, align 4)
+0x40        Collection::Array<uint8>          (op= 0x180011220, stride 1, align 1)
+0x50        bool enabled
+0x58        System::Filename                  (ctor-copy 0x1800067d0)
+0x60        Collection::Set<Filename> {root, count}   (clone 0x1800104f0)
+0x70        Collection::Array<Filename>       (op= 0x1800091f0, stride 8, align 8)
```

The two `Modification` header flags are named from their own accessors rather than guessed:
`+0x00 _dirty` because `?IsDirty@` (0x18000daa0) is `MOVZX EAX, byte ptr [RCX]; RET`, and
`+0x01 _savingSettings` because `?EnableSavingSettings@` (0x1800aead0) is its only writer.
**The constructor opens `MOV word ptr [RCX], 0x101`** — both flags start at 1 — which is
why `SaveList` and `RefreshList` take their registry branch on a normal boot and not the
array-backup branch. That one instruction is what makes `SaveList`'s ported body the live
one rather than dead code.

The nine internals ported as dependencies, all `RegisterRuntimeAddr` only because neither
shipped build gives them a name:

```
0x180010b80  Array<Mod>::~Array           0x180010110  Array<Mod>::operator=
0x1800111a0  Array<int32>::~Array         0x180011110  Array<int32>::operator=
0x1800112a0  Array<uint8>::~Array         0x180011220  Array<uint8>::operator=
0x1800098d0  Array<Filename>::~Array      0x1800091f0  Array<Filename>::operator=
0x180010570  Set node alloc+init          0x180009c30  Set node free
0x1800104f0  Set clone-subtree            0x180010470  Set destroy-subtree
```

Each was grepped for an existing `RegisterRuntimeAddr(0x…` across `runtime/` before being
claimed, so no other lane's registration is shadowed.

### The one deliberate deviation from the instruction stream

Retail inlines the allocator lock at every allocation and free in this cluster
(`MOV RBX,[0x1801fc718]; ADD RBX,0x640; EnterCriticalSection; Allocate/Free; Leave`).
That is byte-for-byte what the composed statics `0x1800c7a50` / `0x1800c7ad0` do, and
attempt 14's `RegisterAllocatorAddresses()` registers both, so this file calls those rather
than re-inlining the critical section eleven times. Same lock, same singleton, same order;
only the inlining differs. Eleven hand-copied lock/unlock pairs is eleven chances to get
the pairing wrong for zero behavioural gain.

Both allocator pointers are cached with the never-cache-a-null contract (cache a hit, retry
a miss), for the reason attempt 14 records: a caller that runs before the allocator arms
would otherwise freeze a null in for the life of the process.

---

## 4. Three things a careful reimplementation would have got wrong

**`GetModification`'s `MOVSXD`.** `MOVSXD RAX,EDX; SHL RAX,7; ADD RAX,[RCX+8]` sign-extends
the index, so a negative index strides *backwards* off the array.
`reference_ghidra_c_output_vs_disassembly.md` records that Ghidra's C prints `MOVSXD` as a
zero extension — which would turn index `-1` into an 8-GB forward offset. The port casts to
`int64_t` before the shift and says why.

**`Mod::operator=`'s String assign is NOT `RtString_AdoptInline`.** Retail stores the source
pointer **unconditionally** and only then tests it (`MOV [RBX],RAX; TEST RAX,RAX; JZ; INC`).
`RtString_AdoptInline` stores only when the source is non-null. The difference matters
because the release that runs first leaves `data == d - 8` on its freeing branch: reusing
the shared helper would have left a **dangling allocation-block base** in the field whenever
the source string was null, instead of the null retail writes. This lane wrote the sequence
out rather than reusing the helper, and the comment above it says which helper was rejected
and why — the same class of near-miss `reference_never_written_needs_by_whom.md` is about.

**The copy loop's guard is on `data + i*stride`, not on `data`.**
`MOV R9,[RBX]; ADD R9,RDX; JZ skip` tests the *element address*, and the loop re-reads
`dst->size` on every iteration. Transcribed as written: it is the only thing standing
between a failed `MemAlloc` and a write through null, and simplifying it to `if (data)`
hoisted out of the loop would be a real behaviour change on an exhausted heap.

`RtColl_SetCloneSubtree_1800104f0` writes `[RAX+0x22]` immediately after the allocation
with **no null check**, exactly as retail does. Left unguarded on this project's rule: a
faithful crash names an upstream problem (here, an exhausted heap), and a guard would
silently produce a half-built tree instead of a loud fault.

---

## 5. `0x18002a090` — held back, with a CORRECTION to the tree's existing note

`runtime/src/system/configuration.cpp:315` already recorded a disposition for this address:

> `(unexported) the map insert 0x18002a090 -> 0x18002a170 -> node alloc 0x18000f970,`
> `rebalance 0x18002a310 : a red-black insert, a Collection lane's work, unreachable`
> `on our boots.`

The *ownership* half of that is right and this lane agrees with it. `0x18002a090` is a thin
wrapper — it addrefs the key and value into a stack `Pair<String,String>`, calls
`0x18002a170` (117 instructions: the real insert), then releases both. Writing a red-black
rebalance from a paginated disassembly, with no populated tree to test it against, is
exactly the silent-corruption case
`reference_uniform_population_is_not_a_verification.md` is about. It should go with
`Map_Find` (`0x18002ace0`), which the same file already owns.

**"unreachable on our boots" is wrong about the address, though it is right about that
file's call site.** `configuration.cpp`'s insert runs only on a non-empty command line,
which no automated run in this project has. But the *same address* is reached from
`runtime/src/io/persistence_registry_win.cpp:266`, inside `ReadSettings`, which every boot
runs, and from `runtime/src/io/persistence.cpp:59`. So the door is live even though the
call site that documented it is not.

This is the `reference_never_written_needs_by_whom.md` shape moved one level up: a
reachability claim belongs to a **call site**, not to an address, and a note attached to
the address inherits the wrong scope. Whoever picks this row up should not read
"unreachable" and defer it.

---

## 6. `0x1800af5f0` `RefreshList` — held back on `IO::PackageFile`

~450 instructions. It stack-constructs an `IO::PackageFile` (vtable `0x180155700`), calls
its open (`0x1800b46b0`), five accessors (`0x180025310/50/90`, `0x180025450/530/580`) and
close (`0x1800b4540`); then for each package it formats `"ModEnabled_%s"` / `"ModIndex_%s"`,
looks each up in `Persistence`'s map through `0x18002ace0`, parses the value with
`0x18007f034`, builds a `Set<Filename>` per mod, and finally runs an inline insertion sort
over the array.

`IO::PackageFile` **is the current cutover wall.** WAVE9.md attempt 14 ends with
`[RtResolve] MISS "??0PackageFile@IO@@QEAA@XZ" -> NULL` and a VCALL-of-0 on the resulting
all-zero object at `LoadPackages +0x184`. Porting `RefreshList` before `PackageFile` lands
would produce a body that cannot be exercised and cannot be diffed — a `calls=0` green line
over unverified code, which is the failure
`reference_uncalled_port_is_not_verified.md` exists to prevent.

This is a blocker on a **named dependency another lane already owns**, which is the one
blocker shape `reference_holdback_blockers_are_stale_check_them_first.md` says to state
plainly. It was re-checked rather than inherited: the wall is in attempt 14, dated today.

Note the asymmetry that makes the holdback safe: `RefreshList` and `SaveList` are a
read/write pair over the same registry keys, but `SaveList` reaches `Persistence` only
through two *exported* setters and `Filename::GetPathFilename`, all three already ported.
It has no `PackageFile` dependency at all. Porting one without the other is not
half-porting a class in the sense PLAN.md §4.1.5 warns about — they share no state beyond
the array they both walk.

---

## 7. The oracles, and what each one can actually fail on

| function | oracle | why that one |
|---|---|---|
| `GetCount`, `GetModification` | **full diff** (`RT_DIFF_SCALAR`) | const reads producing independent results |
| `Enable` | **full byte diff by restore-and-replay** | writes exactly two bytes and allocates nothing, so ours can run, be photographed, be *restored*, and theirs replayed over the same pre-image. `RT_DIFF_BYTES` over the whole 0x28 header + 0x80 record — so a right value at a wrong offset fails |
| `Swap` | **shadow, 3 named terms** | cannot be replayed and its bytes cannot be restored: retail's swap deep-copies both Arrays and *clones* the Set into new heap nodes, so putting the old bytes back would leak them |
| `SaveList` | **restore-and-replay on the `order` column** | its only object mutation (`mods[i].order = i`) is idempotent and allocation-free, and its `Persistence` writes are idempotent, so theirs can be replayed and the column compared |
| the lifecycle triple + the nine internals | **not diffed, and not diffable** | each mutates a live object through the shared heap; running the original as an oracle double-frees at refcount 1. Verified *indirectly* through `Swap`'s shadow — `Swap` is nothing but three calls into the triple |

`Swap`'s shadow deserves the detail, because a shadow that re-reads what the port just
wrote is tautological and this one is not. It photographs both records **before** the swap
and then asserts three independent things about the result:

* `shared` — the refcounted members (`name`, `author`, `description`, `file.data`) must have
  exchanged **pointers exactly**, because retail's assign shares rather than duplicates them;
* `scalar` — the six ints and the enabled byte must have exchanged **values**;
* `deep` — the two Arrays, the Filename array and the Set must have exchanged **counts**,
  since their buffers are new.

Each term can fail alone and the message names which did, per
`reference_shadow_predicate_must_name_its_failing_term.md`. A wrong offset fails `shared` or
`scalar`; a skipped member fails whichever group it is in; a deep copy that lost elements
fails `deep`. Measured: **3 calls, all three terms green on all three, 0 divergences**, on a
two-mod population where the two records differ in every group.

### What the lifecycle triple's verification IS and IS NOT

Honest statement, because this is the weakest link in the lane and it should not be
overclaimed.

**It is:** `Swap` is *nothing but* three calls into the triple — one copy construct, two
assignments, one destruct — and its shadow examined the result of all three, three times,
against a pre-image, and passed on all three terms. A wrong offset, a skipped member or a
lossy deep copy inside the triple surfaces there. That is a real statement about the triple,
not a proxy for one.

**It is not:** `Array<Mod>::operator=` (`0x180010110`) has **no direct oracle and no
per-call counter**. What run 2 establishes is bounded: `Engine::Loader::Load` ran 6 times
with `_savingEnabled` toggling, which is the only caller of that address in our tree
(`loops.cpp:681`), and the process built a world, ran 721 frames over two mounted mods and
shut down cleanly with `SaveList` writing correct values — so the deep copy did not corrupt
the heap and did not lose the mod list. That is a smoke test with a real population, not a
differential result, and the lane does not claim otherwise. Instrumenting it would want a
counter at `loops.cpp:681` and a post-condition comparing `savedMods` to `mods` by value —
one afternoon of work for whoever next touches that file.

---

## 8. Kill switch

`BANISHED_RT_MOD=0` disables the whole lane — all five exports and all fourteen addresses
stay on the DLL — which is the A/B control for any attempt-ladder rung this lane is
suspected of moving. It sits under `BANISHED_RT_IO`, so `BANISHED_RT_IO=0` also disables it.

The registration is **not** cutover-gated, unlike `RegisterAllocatorAddresses()`. That gate
exists because `standalone/allocator_lock.cpp` patches the allocator *at its address* and
`ResolveAddr` consults the port registry FIRST
(`reference_resolveaddr_consults_port_registry_first.md`), so registering early would hand
the patcher our own prologue. Nothing in `standalone/` patches any address in this cluster —
each of the twenty was grepped — so registering unconditionally is correct here, and it has
the advantage of putting these bodies under the diff oracle on a DLL-present run, which is
the only way they get verified at all.

---

## 9. Files

| File | What |
|---|---|
| `runtime/src/io/modification.cpp` | new — the 5 exports, the lifecycle triple, the 9 container internals, `RtIoModificationRegister()`, the oracles |
| `runtime/types/io/Modification.h` | completed — the full `Mod` 0x80 layout, `ModSetNode`, `ModSet`, `ModArray`, and both `Modification` flags named from their accessors |
| `runtime/src/io/rt_io_port.cpp` | one `extern` + one gated call under `BANISHED_RT_MOD` |

No `Makefile` change: `runtime/src/io/*.cpp` is globbed.
