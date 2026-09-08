# WAVE 9 LANE PKG2 — `IO::PackageFile`

**Status: the attempt-14 wall is CLOSED.** 21 exports ported and registered, all 21
exercised on a DLL-present diff run, **11,747 diffed calls / 0 divergences**.

The lane was given one row — "port `IO::PackageFile` far enough that `LoadPackages` can
construct one". That row is the constructor, and it is done. The rest of the file is the
twenty leaf accessors, which came free once a caller existed, plus the honest list of
what is still missing and why.

---

## 1. What attempt 14 actually died on

The crash reads as `VCALL-of-0` at `RtIo_FileSystem_LoadPackages +0x184` on an object
whose first 0x38 bytes are all zero. **It is not a virtual call.** `RAX=0`,
`RCX=RDX=0x250039802d0` (the fresh block), `RIP=0` — this is
`RT_EXPORT_ONCE(PackageFileCtorFn, "??0PackageFile@IO@@QEAA@XZ")(block)` with a null
function pointer. The object reads as all-zero because nothing had run on it yet: lane
PKG's `RtMemAlloc(0xF0, 8)` returned the block and the constructor never executed.

The crash handler's `VCALL-of-0` label is a heuristic (`this` in RCX, zero at `[this]`),
and here it happens to be misleading in a way worth writing down: **a null call whose
first argument is a fresh heap block looks exactly like a virtual call on an object with
no vtable.** The `[RtResolve] MISS "??0PackageFile@IO@@QEAA@XZ" -> NULL` line immediately
above it in `standalone_error.log` is what actually names the fault, and it is the line
to read first.

---

## 2. What was ported

`runtime/src/io/package_file.cpp`, gated by `BANISHED_RT_PKG2=0`.

| # | Export | Retail | Ord | Size | Diff result |
|---|---|---|---|---|---|
| 1 | `??0PackageFile@IO@@QEAA@XZ` | `0x1800b4440` | 289 | 48 i | calls=5 worked=5 **div=0** |
| 2 | `?Construct@` | `0x1800252f0` | 1834 | 4 i | calls=2 worked=1 **div=0** |
| 3 | `?Type@` | `0x180025130` | 4210 | 2 i | calls=4 **div=0** |
| 4 | `?TypeId@` | `0x180025140` | 4314 | 2 i | calls=4 **div=0** |
| 5 | `?TypeName@` | `0x180025150` | 4417 | 2 i | calls=4 **div=0** |
| 6 | `?GetFileVersion@` | `0x1800b6080` | 2642 | 2 i | calls=4 **div=0** |
| 7 | `?Compile@` | `0x1800b6090` | 1744 | 2 i | calls=4 **div=0** |
| 8 | `?GetName@` | `0x180025310` | 2806 | 13 i | calls=4 **div=0** |
| 9 | `?GetAuthor@` | `0x180025350` | 2456 | 13 i | calls=4 **div=0** |
| 10 | `?GetDescription@` | `0x180025390` | 2566 | 13 i | calls=4 **div=0** |
| 11 | `?GetVersion@` | `0x1800253d0` | 3010 | 2 i | calls=4 **div=0** |
| 12 | `?GetUserVersion@` | `0x1800253e0` | 3003 | 2 i | calls=4 **div=0** |
| 13 | `?GetPlatform@` | `0x1800253f0` | 2848 | 2 i | calls=4 **div=0** |
| 14 | `?GetFileCount@` | `0x180025440` | 2600 | 2 i | calls=4 **div=0** |
| 15 | `?GetFileSize@` | `0x1800254a0` | 2605 | 5 i | calls=**5836** **div=0** |
| 16 | `?GetFileOffset@` | `0x1800254c0` | 2603 | 5 i | calls=**5836** **div=0** |
| 17 | `?GetPriority@` | `0x1800254e0` | 2856 | 2 i | calls=4 **div=0** |
| 18 | `?CanOverride@` | `0x1800254f0` | 1683 | 2 i | calls=4 **div=0** |
| 19 | `?GetCRC@` | `0x180025500` | 2473 | 2 i | calls=4 **div=0** |
| 20 | `?GetIconWidth@` | `0x180025510` | 2699 | 2 i | calls=4 **div=0** |
| 21 | `?GetIconHeight@` | `0x180025520` | 2698 | 2 i | calls=4 **div=0** |

Every name and every ordinal above was printed out of `Runtime-x64.dll`'s own export
directory with `pefile` before a line of the port was written — lane PKG's header records
that its own first draft had seven of eight hand-reconstructed ordinals wrong.
`tools/export_name_audit.py` re-checks the names: **662 distinct names, 1 not in the DLL,
and that one is a pre-existing doc-string false positive in
`runtime/funcs/rt_resolve_cache.cpp:8`, not a row of this lane.**

Every row is registered through **both** doors — `RegisterRuntimeExport` and
`RegisterRuntimeAddr` — because a port reached by address silently uses the DLL and no
run reports it (`reference_bypassed_port_needs_a_source_checker.md`). Unlike the
allocator's four addresses (commit `309c015`), these are registered unconditionally
rather than cutover-only: none of the 21 addresses is patched, detoured or read as
shipped bytes anywhere in the tree, so there is no
`ResolveAddr-consults-the-port-registry-first` hazard to avoid.

---

## 3. The constructor's oracle, and its effectiveness control

`0x1800b4440` is 48 instructions and **calls nothing**. That makes it the rarest thing in
this wave: a byte-faithful port with a real, cheap, non-destructive oracle. Both bodies
run onto private 0xF0 scratch buffers **prefilled with the same byte (0xCD)**, and all
0xF0 bytes are compared.

Prefilling both identically is what makes it sound in both directions:

* a byte neither side writes holds 0xCD on both and compares equal, so the holes cannot
  manufacture a 100%-failure report
  (`reference_oracle_must_not_compare_unwritten_bytes.md`, four false gaps in Wave 6);
* a byte **ours** writes and theirs does not reads 0x00 against 0xCD and **fails** — the
  mirror defect, which a "compare only the fields we modelled" oracle cannot see at all.

Measured:

```
[PKG2] ctor effectiveness: ours wrote 213/240 bytes, theirs wrote 213/240
       (the 27 untouched are +0x78,+0x7c,+0x90,+0xad..af,+0xc4,+0xd4)
[RtDiff] IO::PackageFile::PackageFile   calls=5   worked=5   divergences=0
```

**The 27 untouched bytes were predicted from the disassembly before the run and the run
returned exactly 27** — 4 (+0x78) + 4 (+0x7c) + 8 (+0x90) + 3 (+0xad..af) + 4 (+0xc4) +
4 (+0xd4). Both sides agree on which 213 they write.

`worked=` is **not** hardcoded `true`: it is "our body moved at least one byte off the
prefill". A body that ran and did nothing would report `calls=5 worked=0 divergences=0`,
which is the exact shape of a green line over dead code
(`reference_probe_needs_an_effectiveness_control.md`).

The three vtable stores are part of the compare, so the oracle verifies we wrote the
right table in the right slot. **It cannot verify the MINTED table**, because the mint is
not registered on a DLL-present run. That is §6's problem and it is stated here rather
than implied to be covered.

---

## 4. The sweep probe — how twenty leaves got a caller

Twenty of the twenty-one rows are leaves, and nothing in our tree called any of them. On
their own they would have been the failure this project keeps paying for: registered
ports reporting `divergences=0` over code no run executed
(`reference_uncalled_port_is_not_verified.md`).

The caller already existed and was free. `RtIo_PackageFile_SweepProbe(fs)` runs at the
tail of `RtIo_FileSystem_LoadPackages`, **inside the diff guard** — the one moment in the
process where every mounted package is reachable and fully populated by the DLL's own
`ReadPackfile`. Each port then runs its own ours-vs-theirs comparison; the probe only
supplies inputs.

```
[PKG2] sweep: 4 packages, 5836 entries; per-entry accessors got 5836 calls each.
       Distinct values across packages: version=1 platform=1 priority=>=2
```

**And the hole that leaves, stated rather than left for a reader to find.** The
per-package population is 4 and it is partly UNIFORM: `version` and `platform` returned
the SAME value on all four packages. Four identical comparisons are not a verification
(`reference_uniform_population_is_not_a_verification.md` — a shared BST find passed four
waves of oracles because 13 of 14 values were the same value). What actually pins those
two rows is the disassembly: each is a single `MOV EAX,[RCX+off] ; RET`, and the offset
is the whole function. `priority` returned **>=2 distinct values**, so that row is
genuinely exercised — and it is the one that matters, since `+0x70` is the packfile
sort's entire comparator.

The two per-entry accessors are the strong rows: **5,836 distinct entries each**, and a
wrong stride or a wrong `+0x8`/`+0xc` would disagree on all but the zeroth.

`?Construct@`'s two branches are driven explicitly (a fresh scratch block and `nullptr`),
because no caller in this tree reaches the factory. `worked=1` of `calls=2` is correct
and deliberate: the null branch does no work by definition, and counting it as covered
would be dishonest.

---

## 5. Accounting

### `tools/unsatisfied_export_scan.py`, before and after

| | before | after |
|---|---|---|
| distinct names resolved through a door | 789 | 798 |
| have neither a port nor a mint entry | 149 | **148** |
| ...of those, at least one site outside a diff guard | 146 | **145** |

**Set diff of the outside-a-diff-guard list (the load-bearing measurement):**

```
LEFT the list:    ??0PackageFile@IO@@QEAA@XZ
JOINED the list:  (nothing)
```

Exactly one name left, nothing was added. Files:
`docs/runtime_port/wave9_lane_pkg2_unsatisfied_{before,after}.txt`.

### The denominator MOVED, +9, and here is the exact reason

The lane brief asked for an unmoved denominator. It moved, so here is the arithmetic
rather than a hand-wave. Of the 21 export names this lane registers, **20 were not
previously reached through any door anywhere in the tree** (checked by grepping each name
across `runtime/ code/ engine/ standalone/ dx11/` excluding this lane's own file); only
`??0PackageFile@` was. Of those 20, **11 are invisible to the scan** because its `DOOR`
regex needs the mangled literal adjacent to the door token, and this file reaches 11 of
them through two indirections:

* 8 through the `RT_PKG2_SCALAR_GETTER` macro, where the door is `RtOriginal<ORACLE_FN>(MANGLED)`
  in the macro *definition* and the literal appears only at the *call* site;
* 3 through the shared `ReplayStringGetter(self, ret, "?GetName@…")` helper, where the
  literal is a plain function argument.

20 − 11 = **9**, which is the measured move exactly. This is the same family as
`reference_census_cannot_see_nonliteral_registrations.md` and
`reference_census_cannot_see_the_macro_doors.md`, and it is flagged here rather than
quietly benefited from. **It hides nothing unsatisfied in this lane** — all 11 are
registered by `RtIoPackageFileRegister()`, and all 11 are oracle-only doors inside a diff
guard, compiled out of the cutover path. It would matter if a future lane used the same
shape for a LIVE `RT_EXPORT_ONCE` door, and that is the reason to record it.

### `tools/runtime_census.py` (post-`29cfaa2` numbers)

| | before | after |
|---|---|---|
| called | 958 | 960 |
| ported | 665 | 666 |
| coverage | 69.4% | 69.4% |

Measured by stashing this lane's changes, running the census, and restoring — not
inferred. **Coverage does not move, and that is the correct result, not a disappointment.**
20 of the 21 ports are functions the tree did not previously reach at all, so they enter
`called` and `ported` together and cancel; only `??0PackageFile@` was already `called`,
which is the +1. The +2 on `called` is the two new DATA names this file reads
(`??_7PackageFile@IO@@6B@`, `?_typeInfo@PackageFile@IO@@`). Same shape as
`reference_porting_can_grow_the_cutover_surface.md`, now measured a fourth time in Wave 9.

### Other audits

* `tools/port_bypass_audit.py` — **0 bypasses on both doors.** 209 address sites, 0 reach
  a function ported by name; 440 raw-door sites, 0 are a ported function on a live path.
* `tools/rt_data_door_audit.py` — **PASS**, no minted DATA symbol read through a raw door.
* `tools/export_name_audit.py` — 1 miss, pre-existing, not this lane's.

### The one divergence in the run is NOT this lane's

```
[RtDiff] DIVERGENCE Object::Factory::Startup [probe, not diffable] call #1
         ours=chain=416 nodes=415 badCrc=0   theirs=chain=416 nodes=416 badCrc=0
```

An A/B control settled it: the same run with `BANISHED_RT_PKG2=0` (`IO: 56 exports
registered` instead of 77) produces the **byte-identical** line, `chain=416 nodes=415`.
Pre-existing. Logs: `wave9_lane_pkg2/pkg2off_error.log`.

Worth noting for whoever owns it: `chain==nodes+1` means one `BaseTypeInfo` in the
reflection chain did not get its own node in the factory tree, i.e. two entries share a
name. It is not the mint — the mint keeps its BaseTypeInfo objects on a **private** chain
head (`g_mintTypeInfoHead`, `standalone/rt_data_mint.cpp:273`), and the proof is in the
number itself: this lane took the mint from 45 to 46 BaseTypeInfo rows and `chain` stayed
416 in both runs.

---

## 6. The mint row, and its independent confirmation

`tools/rt_mint_gen.py` was re-run as part of this commit. It drives its own set off
`tools/runtime_census.py`, so this file's `RT_DATA_SLOT` reads are what pull the rows in:

```
Counts: 49 vtables, 45 BaseTypeInfo  ->  50 vtables, 46 BaseTypeInfo
+ "??_7PackageFile@IO@@6B@"
+ "?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A"
```

Lane PKG §7 predicted **9 slots** at `0x180155700` by reading the bytes. The generator,
which decides slot counts independently (adjacency proof or a tail probe), emitted
**9 slots, `[tail-not-a-pointer]`**, and named 8 of them; slot 0 is `0x1800259d0`,
`RT_MINT_UNNAMED` — the deleting destructor, exactly as predicted. Two independent
mechanisms agree.

This lane fills mint slots **1, 2, 3, 4 and 8** (`TypeName`, `Type`, `GetFileVersion`,
`Compile`, `Construct`). Slots **0, 5, 6, 7** (deleting destructor, `Serialize`,
`Reconstruct`, `Destruct`) remain named traps, for the reasons in §7.

The constructor's two *dead* vtable stores (`??_7Base@Object@@6B@`,
`??_7External@Object@@6B@`) are transcribed **through the port-aware data door**, not as
literal VAs. They are provably dead — nothing reads `[this+0]` between them and the
surviving store, and there is no call in the window — but at cutover a literal
`0x180154788` would be a pointer into a module that is not loaded, written into every
package object in the process.

---

## 7. What was NOT ported, and the honest reason for each

| Symbol | Retail | Why not |
|---|---|---|
| `?ReadPackfile@` | `0x1800b46b0` | **6,597 bytes.** The `.pkg` format parser. THE NEXT WALL, and a lane of its own. |
| `??1PackageFile@` | `0x1800b4540` | Not hard — **blocked on five unported callees**: `0x18000c180` (`~FileInfo` at +0xc0), `0x1800269a0` (`~Array` at +0xb0), `0x1800112a0` (+0x98), `0x1800111a0` (+0x80), `0x180010ab0` (+0x30 and +0x20). The tail `JMP 0x1800bc950` (`~External`) IS ported. A "port" today would be five null calls at cutover — strictly worse than one named MISS. |
| slot 0, `FUN_1800259d0` | `0x1800259d0` | The MSVC deleting-destructor thunk over `~PackageFile`. Lane K's family. Same blocker. |
| `?Reconstruct@` / `?Destruct@` | `0x1800252d0` / `0x180025300` | **Two instructions each, and BOTH drive vtable slot 0** — i.e. the destructor above. Porting them would put a faithful-looking body over a call that cannot work, and neither has any oracle: running one destroys the object being compared. **This row corrects this lane's own first draft**, which grouped them with `Construct` because all three are 2–4 instructions. `Construct` is portable because its only callee is the constructor this lane ports; the other two are not, because their only callee is the destructor it does not. Cost was never the discriminator. |
| `?Serialize@` | `0x1800b60a0` | ~100 instructions dispatching through the `IO::Encoding` vtable at +0x18/+0x20/+0x40/+0x78/+0xa0. Save path — this project's most-cited failure mode — and the retail-vs-profile Encoding offset trap applies. A real oracle exists (`reference_write_path_serialize_is_replayable.md`) but needs a `Stream` to drive it. **Deferred, not dismissed.** |
| `?GetFileInfo@` | `0x180025400` | Needs the `FileInfo` copy ctor `0x18000c110`. |
| `?GetIconData@` / `?GetPreviewData@` | `0x180025530` / `0x180025580` | Return `Collection::Array` copies; that machinery is lane Q/D2's. |
| copy ctor, `operator=`, `WritePackfile`, `ExtractPackFile`, `BuildPackageFile`, `AreResourcesEqual`, `BuildFileSet`, `ClosePackFile` | — | The **ToolKit half** of the class: building and writing packages. Nothing on the game's boot or run path reaches any of them; they are the resource compiler's side. Listed so the next lane does not re-derive it. |

---

## 8. The four unregistered address doors in `filesystem_packages.cpp`

Asked for by the integrator (`docs/runtime_port/wave9_unregistered_addr_doors.csv`). Read
at the call site, with a boot-path verdict each.

| Address | What it is | Reached on the boot path? |
|---|---|---|
| `0x18007eefc` | **A FUNCTION, not data.** The CRT's `_stricmp`: `CMP dword ptr [0x1801fba28],0` (the locale flag), then null-checks both pointers and tail-`JMP`s to `0x18007eec0` (`__ascii_stricmp`); the non-default-locale path is `0x18007ef44`. Disassembled to settle the integrator's "ends in `fc`, check whether it is data" question — it is a valid call target. | **YES.** `PathStrICmp` → `IndexFind` → LOOP 3, once per file per package: **5,836 calls on the run measured above.** |
| `0x1800264a0` | The packfile introsort. | **YES.** Guarded only by `packfileCount > 1`; a stock install mounts 4. |
| `0x180025a80` | The path→`{packIndex,fileIndex}` BST insert. | **YES.** Once per first-seen path in LOOP 3. |
| `0x180008340` | The recursive index-node free. | **NO on the first boot.** `ClosePackages` guards it on `packfileIndexRoot != nullptr`, and `LoadPackages` calls `ClosePackages` *first*, on a `FileSystem` that has no index yet. It IS reached on the **second** `LoadPackages` — the mods-menu remount — and on teardown. |

**The ordering that matters.** All three boot-path doors sit *after* `ReadPackfile` in
program order: LOOP 1 is `alloc → ctor → ReadPackfile → append`, and only then the sort
and LOOP 3. So they are rungs **behind** the next wall, not competing with it. Predicted
sequence from here:

```
??0PackageFile   (CLOSED by this lane)
  -> ?ReadPackfile@                 6,597 bytes           <- NEXT
    -> FUN_1800264a0  (sort)        110 instrs
      -> FUN_180025a80 (BST insert) + FUN_18007eefc (_stricmp)
        -> FUN_180008340 (tree free), only on the second LoadPackages
```

Two notes for whoever takes them, both from the integrator and both worth keeping:

* `0x180025a80` and `0x180008340` are **Collection internals** — an AVL/BST insert with a
  rebalance, and a recursive free. A rebalance written blind silently corrupts a tree and
  a green oracle would not necessarily catch it. Give them lane Q's care.
* `0x18007eefc` must **not** be shared with `runtime/src/system/configuration.cpp`'s
  comparator. `__ascii_stricmp` lowers only `'A'`–`'Z'` and compares **unsigned**
  (`MOVZX`); the config map inlines a **signed**-char `strcmp`. They disagree on any byte
  `>= 0x80`, and a package path can contain one. A single shared `TreeStrCmp` would be
  wrong for one of the two.

---

## 9. A header correction this lane made

`runtime/types/io/FileSystem.h` modelled `+0x70` as `uint64_t priority`, because the
constructor's store there is one qword. It is **two dwords**: `?GetPriority@` reads the
low half and `?GetPlatform@` (`0x1800253f0`) reads `[RCX+0x74]`, the high half. The single
qword store seeds `priority=0x2710` **and** `platform=0`, so the qword reading was never
wrong about the bytes — only about what they mean. Nothing in the tree read the field as a
qword (grepped before the change) and no offset or `static_assert` moved.

Eight new fields and one new struct were added with a `static_assert` each, every one
pinned by the single instruction that reads it: `name` +0x40, `author` +0x48,
`description` +0x50, `version` +0x68, `userVersion` +0x6C, `platform` +0x74, `iconWidth`
+0x78, `iconHeight` +0x7C, and `PackageFileEntry {name; offset +0x8; size +0xC}` with
stride 16 proved three independent ways (`GetFileName`'s `SHL RDX,0x4`, `GetFileOffset`'s
`[RAX + R8*8 + 0x8]` after `ADD R8,R8`, and `GetFileSize`'s `+0xc`).

Note the corroboration that fell out of it: `?GetIconWidth@`/`?GetIconHeight@` read
`+0x78`/`+0x7c`, **the two fields the constructor does not initialise**. The written-extent
analysis and the accessor set agree independently.

Also recorded: `+0xA8` is **one** field with two exported readers — `?GetCRC@` and
`FileSystem::GetPackfileId` (`0x1800ab050`). The package CRC *is* the packfile id.

---

## 10. Measurement hygiene

Six lanes deploy to one game directory. The first attempt at this measurement **was
corrupted by exactly that**: `rt_run.sh` reported `IO: 77 exports registered` (mine), and
by the time the logs were copied out a concurrent lane had replaced them with a run
showing `IO: 56` and `Build: 09:16:49`. The numbers in this document come from a rerun in
which the deploy, the run and the log copy are a **single Bash invocation** that first
waits for any running `reimpl-x64.exe` to exit. Logs are committed under
`docs/runtime_port/wave9_lane_pkg2/`.

One caveat on the identity check, so nobody re-derives it: the `[Standalone] Build:` line
comes from `__DATE__`/`__TIME__` in a **ccache-reused** `main.cpp` object and is stale by
design. It is not evidence of a stale exe. The reliable identity marker is
`[RtPort] IO: 77 exports registered` (56 + 21), which only this lane's source produces.

---

## 11. What the next rung is

**`?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z`, `0x1800b46b0`, ord 3461,
6,597 bytes** — the `.pkg` format parser. It is the next instruction `LoadPackages`
cannot get past, and it is a lane, not a tail-end.

Prediction for attempt 15: the frame **stays** in `RtIo_FileSystem_LoadPackages` and the
fault moves forward from the constructor call to the `ReadPackfile` call, with
`[RtResolve] MISS "?ReadPackfile@..."` above it. Falsifiers:

* a fault still naming `??0PackageFile@` means the registration did not take — check that
  `[RtPort] IO: N exports registered` reads 77, not 56;
* a fault *inside* our own constructor body means a mint row is missing and
  `RT_DATA_SLOT` stored a null at `[this+0]`;
* a fault naming `?CloseFile@` or `FUN_180008340` would mean the boot got **past**
  `ReadPackfile`, which would be good news and would mean §8's ordering is wrong.
