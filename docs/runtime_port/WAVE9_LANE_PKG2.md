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

---

# Iteration 2 — `ReadPackfile` decomposed, and its bottom rung ported

Full dependency reading and sub-lane order: **[WAVE9_READPACKFILE_PLAN.md](WAVE9_READPACKFILE_PLAN.md)**.

## 12. The verdict on `ReadPackfile`

**It should not be ported as one body, and not because of its size — it should not be
ported first at all.** Six of its callees are unported and one of them decides every byte
of every package the game loads.

`0x1800b46b0` measures **1,864 instructions**, the largest single body of the wave. But it
is not 1,864 instructions of novel logic: `?Read@CompressedStream@IO@@` is 51 instructions
of `SetWindow` + bounds + `memcpy`, and **MSVC inlined it at ~25 sites**. That is where the
byte count went. The distinct logic is a header/directory parser standing on an
`IO::CompressedStream` read path nothing in this tree has ported.

Two findings worth having regardless of who takes the rest:

* **`0x18008a290` is the CRT `memcpy`** — jump table under 16 bytes, `REP MOVSB` under the
  ERMSB bit at `0x1801fc65c`, `MOVNTI` above 4 KB. A pure byte copy, so `std::memcpy` is
  byte-faithful. **Not** a lane-L-shaped CRT port; do not spend a lane on it.
* **`Compression::Decompress` is a bespoke LZ77/LZSS codec — no zlib, no LZ4, no library.**
  Stated loudly because "the package format is compressed" invites the opposite assumption
  and would send a lane hunting for a library that is not there.

## 13. Rung 1 ported: `?Decompress@Compression@System@@SAHPEBXIPEAXI@Z`

`0x1800c4660`, ord 2150, 72 instructions. `runtime/src/system/compression.cpp`,
`BANISHED_RT_COMPRESSION=0`.

```
[RtDiff] System::Compression::Decompress   calls=177  worked=137  divergences=0
[PKG2/Compression] probe: 177 cases, round-trip ok=137 bad=0 (compressor declined 0)
[PKG2/Compression] branch coverage: literal=39879 matchShort=55927 matchExtended=9211
                   | failLiteral=3 failMatchDst=5 failMatchSrc=32 -- ALL SIX BRANCHES COVERED
```

**105,017 control bytes decoded, 0 divergences, every branch entered.**

Four things in the codec that are load-bearing and easy to get wrong:

1. **The match copy must be byte-at-a-time.** `mp` and `op` overlap whenever the distance
   is less than the length, and each byte is read *after* the previous was written — the
   RLE idiom every LZ77 depends on. A `memcpy`/`memmove` here compiles, runs, and differs
   for exactly the inputs that compress best.
2. Total copied is `2 + len` (two unrolled bytes, then `do { } while (--len)`), which is
   why the bound is `op + 2 + len` and the minimum match is 3.
3. **The loop is a do-while: `srcLen == 0` still reads one control byte.** Retail has no
   test before the first iteration. Left unguarded and faithful.
4. `mp < dst` is an **unsigned** compare (`JC`, not `JL`) — `mp` is computed by subtraction
   and can wrap below the buffer.

**Why this rung went first: it has the best oracle in the stack.**
`?Compress@Compression@System@@` (`0x1800c4230`, ord 1749) is exported, so the probe
compresses a real buffer with the DLL's own encoder and decompresses it with both
implementations. Three independent checks — ours == theirs on the return value and every
byte; **ours round-trips to the original input**, which does not involve the DLL's decoder
at all, so a shared misreading of the format cannot pass it; and the three failure exits
agree. The encoder is deliberately **not** ported: porting the thing that generates your
test inputs is how a shared misreading passes its own test.

`worked=137` against `calls=177` is the effectiveness control working: the 40 non-working
cases are the deliberately malformed and tight-`dstCap` inputs, which return 0 by design.
A codec probe is exactly where a uniform population hides — feed it garbage and nearly
every case returns 0 from the same bounds check, and thousands of agreeing zeroes read as
thorough. The three failure buckets are small (3/5/32) but non-zero **because they were
driven on purpose**, which is the difference between a covered branch and a lucky one.

## 14. A defect the run found that reasoning did not

The first draft called the probe from `RtSystemPortInit()`. It crashed:

```
Fault: WRITE of 0000000000000648      RCX=0000000000000640
#1  Runtime-x64.dll + 0xC41D8         (inside ??0Compression@System@@)
[REDZONE-DUMP] no allocator singleton
```

**`RtSystemPortInit` runs before `System::MemoryAllocator` has published its singleton.**
The probe builds a `System::Compression` through the DLL's ctor; that ctor takes the
allocator lock at `[0x1801fc718] + 0x640`; with the singleton still null that is a write
to `0x648`. The top frame is inside ntdll and **nothing in the crash names the probe** —
the only clue is the `no allocator singleton` line the dump prints anyway.

The rule, for the next lane that adds one: **a probe that allocates cannot run from a
`*PortInit`. Registration is fine there; execution is not.** The probe now runs from
`LoadPackages`' tail, a point the heap is provably live because the surrounding body
allocated a 0xF0 `PackageFile` per package to get there.

## 15. Contention, again — and a tool for it

Two more measurements were lost to the shared game directory: one where a concurrent lane
replaced `reimpl-x64.exe` between the copy and the launch (the run reported `IO: 56`,
another lane's count), and one where the logs were overwritten after the run.

`tools/pkg2_run.sh` now does deploy + launch + log copy in one process with an identity
check on **both** ends: the md5 is re-checked after the copy and before the launch, and the
run is **discarded and retried** unless `[RtPort] IO: N exports registered` matches the
expected count. It also records the **package set** next to the binary identity, because
for this lane the input set is as much part of the measurement as the exe — a `.pkm`
appearing in `WinData` changes what `LoadPackages` mounts even when the binary is right.
Expected counts after this iteration: **IO = 77, System = 84.**

## 16. The `.pkm` question, answered

Asked whether two `.pkm` copied into `WinData/` would give `ReadPackfile` coverage the
4-package baseline misses. **Not as framed, and the correction matters:**

* A `.pkm` is **not a different code path**. `LoadPackages`' two loops call the *same*
  `ReadPackfile`; delta-packing is a property of the content, not a second parser.
* **Presence in `WinData/` alone changes nothing.** `FindFiles`' pattern is the literal
  `*.pkg` (five bytes at `0x1801556f0`), so LOOP 1 never matches a `.pkm`. One reaches
  `ReadPackfile` only through LOOP 2, from `userPackFilenames` (+0x30), which the mods
  system populates from the registry. Measured: the baseline reported exactly 4 packages
  while both `.pkm` were sitting in `WinData`.

What *would* help at rung 4 is a **population** argument, and a good one: the accessor
sweep measured `?GetVersion@` and `?GetPlatform@` returning **one distinct value across all
four base packages**, and four identical values is not a verification. A mod package should
carry a different version, a non-default `+0x70` priority and non-empty
name/author/description — exactly the uniform fields. That needs the mods **enabled**, not
merely present.

## 15b. Reproduced under the game lock, and one finding the second run added

Re-run after merging the integration tip, this time under `tools/game_dir_lock.sh`
(it waited 90s for `lane-PICK`, then held the lock across deploy, run **and** the log
copy). Identity verified from the log, not the disk.

```
[RtPort]   IO::PackageFile (lane PKG2): 21 exports registered
[RtDiff] System::Compression::Decompress  calls=177  worked=137  divergences=0
[PKG2/Compression] probe: 177 cases, round-trip ok=137 bad=0 (compressor declined 0)
[PKG2/Compression] branch coverage: literal=39880 matchShort=55926 matchExtended=9211
                   | failLiteral=3 failMatchDst=5 failMatchSrc=32 -- ALL SIX BRANCHES COVERED
[PKG2/Compression] read-cursor exits (ours only, NOT compared -- sp is a local):
                   exact=137 overshoot=0
```

**The identity gate had to change, and the reason is worth keeping.** The first version
matched `[RtPort] IO: 77 exports registered`. Lane MOD then landed `IO::Modification` on
the same `RtIoPortInit`, the IO total moved, and that gate would have **failed a perfectly
good run**. A shared total changes whenever any other lane lands in the same namespace, so
it is the wrong thing to key an identity check on. The gate now keys on a lane-owned
banner that changes only when this lane's own file changes, and `tools/pkg2_run.sh`
**derives the expected number from the source** rather than carrying a literal forward.

**`literal` moved 39879 → 39880 and `matchShort` 55927 → 55926 between the two runs.** Not
noise to wave away: the corpus is sliding windows over the *loaded* Runtime image, which
is ASLR-relocated, so relocated pointer bytes differ run to run. The corpus is
deliberately not bit-identical across runs, which adds variety rather than costing
determinism — and `divergences=0` reproduced across both.

### The read-cursor instrumentation, and the honest reading of it

Added in response to lane PICK's finding that an oracle observing its result only through
a lossy transform is blind to whatever the transform discards. The decompressor's
analogue is comparing output bytes while ignoring **where the decode left the stream**.

For *this* function the hazard does not apply, and the reason is specific rather than
reassuring: `sp` is a **local**. `Decompress` is `static`, owns no object, and its only
observable outputs are the returned length and the destination bytes — both compared, and
the destination compared over the **whole `dstCap`** rather than the prefix the return
value claims, so a body returning the right length while writing a different amount fails.
There is no cursor field, no checksum, no end-of-block state to miss.

It is counted anyway, and **the count says the overshoot exit is UNCOVERED**: `exact=137,
overshoot=0`. By this lane's own rule a zero bucket is reported as uncovered rather than
averaged into a pass. The explanation is benign and checkable — `Compress` emits whole
tokens, so a well-formed stream always lands the cursor exactly on `srcEnd`, and the 40
malformed cases all returned 0 before reaching the exit at all. So the overshoot landing
is reachable only from a malformed stream that nonetheless decodes to completion, which
this corpus does not contain. It is not a distinct code path (same loop test, different
landing), so this is a gap in the *report*, not in the verification — but "the corpus
never exercised it" and "it cannot happen" are different claims and only one of them is
supported.

**At rung 2 this stops being academic.** `CompressedStream` carries `position` (+0x1c),
`activeWindow` (+0x20) and per-window use counts; there, comparing returned bytes alone
*is* the lossy transform, and the oracle must compare the object's state too. Lane PICK's
second lesson — compare **in lockstep**, step by step, rather than running each subject to
completion and comparing once, because a defect that cancels out compares equal at the end
while every intermediate state differed — applies to rung 2 for the same reason: a
`CompressedStream` decodes block by block and carries state between blocks. Both are
recorded in [WAVE9_READPACKFILE_PLAN.md](WAVE9_READPACKFILE_PLAN.md) as requirements on
rung 2's oracle rather than as advice.

Neither lesson changes rung 1's verdict: a single `Decompress` call is one indivisible
operation with no state carried across calls, and the round-trip check is an independent
second opinion that a compensating defect would also have to satisfy.

## 16b. Accounting for iteration 2 — including a +3 that is mine

| `unsatisfied_export_scan.py` | it.1 after | it.2 after |
|---|---|---|
| distinct names resolved through a door | 798 | 802 |
| neither a port nor a mint entry | 148 | 151 |
| ...at least one site outside a diff guard | 145 | **148** |

**The +3 is this iteration's own probe, and it is named rather than buried.** Set diff:

```
JOINED: ??0Compression@System@@QEAA@XZ
        ??1Compression@System@@QEAA@XZ
        ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z
LEFT:   (nothing — ?Decompress@ was never on the list, having had no door before)
```

All three are reached **only** from `RtSystem_CompressionProbe()`, whose first statement is
`if (!RtDiffEnabled()) return;`. They are unreachable in any run that is not the diff
harness, and unreachable at cutover where the harness is compiled out. **The scan cannot
see a function-level early return as a guard** — it recognises the structural
`rt_diff_on__` form only — so it lists them as live.

That is the second blind spot this lane has hit in the same tool (iteration 1's was the
literal-adjacency regex). The sites are marked `RT_RAW_OK`, which is the convention
`tools/rt_data_door_audit.py` already honours for exactly this case ("oracle-only reads
that die with the oracle"); `unsatisfied_export_scan.py` does not honour it yet. **Flagged
for the tool's owner rather than worked around** — and the marker means a future reader
greps one string instead of re-deriving the argument.

**Those three rows are a delta measured on this branch before merging the integration
tip**, which is the only way to isolate this lane's own contribution. After merging
`7421209` the absolute numbers are different (831 doors / 148 unsatisfied / 145 outside a
guard) because four other lanes landed in between — that movement is theirs, not this
lane's, and quoting the post-merge figure as "my after" would silently credit or blame
this lane for it.

Also unchanged and clean: `port_bypass_audit.py` **0 bypasses** on both doors,
`export_name_audit.py` 1 miss (the pre-existing doc-string false positive), and the run
carries **no crash** and no new divergence — the only one is `Object::Factory::Startup`,
which iteration 1's A/B proved pre-existing and which the integrator has since fixed at
`da5093e`, after this branch's base.

## 17. What is still open

* **Rungs 2–4** — the `CompressedStream` read path (~544 instructions, layout already fully
  modelled in `runtime/types/io/Stream.h` by Wave 4), `FileSystem::OpenFile`/`ReadFile`,
  then `ReadPackfile` itself. Each has an oracle named in the plan. Rung 2 needs one
  **built** (a dual-stream probe over the same `.pkg`), because `RegisterRuntimeExport`
  does not redirect the DLL's own internal `CALL`s — there is no free caller until rung 3.
* **`ReadPackfile` now has an oracle it did not have before this lane**: it is replayable
  onto a *second, freshly constructed* `PackageFile`, because iteration 1 ported and proved
  the constructor byte-exact.
* **Which normal-play action triggers the second `LoadPackages`** (and so `0x180008340`,
  the tree free) is still unconfirmed. A mods reload is the candidate; it was not verified.

---

# Iteration 3 — rung 2: the `CompressedStream` read path

## 18. Ported

`runtime/src/io/compressed_stream_read.cpp`, `BANISHED_RT_CSREAD=0`.

| Export | Retail | Ord | Instrs |
|---|---|---|---|
| `?Read@CompressedStream@IO@@UEAA_NHPEAX@Z` | `0x1800aa780` | 3449 | 51 |
| `?SetWindow@CompressedStream@IO@@AEAA_NH@Z` | `0x1800a9aa0` | 4016 | 296 |
| `?GetAvailableWindow@CompressedStream@IO@@AEAAHXZ` | `0x1800a9980` | 2458 | 79 |
| `?DecompressFromSource@CompressedStream@IO@@AEAA_NH@Z` | `0x1800a96b0` | 2152 | 52 |

```
[RtDiff] IO::CompressedStream::ReadPath [lockstep probe] calls=1 worked=1 divergences=0
[CSRead] lockstep probe: image=132813 bytes; cfg w=1 75 reads, w=4 75 reads,
         w=4+revisit 75 reads (591474 bytes total) -- IDENTICAL (bytes, ORIGINAL
         content, position, activeWindow, every window record and page)
[CSRead] SetWindow branches: fastPath=221 scanHit=3 readMiss=19 inMemory=0
         growDelegated=0 -- the three READ-PATH branches are all exercised
```

**225 reads, 591,474 bytes, three configurations, 0 divergences.**

### The stream format, recovered in full

`?Open@CompressedStream@IO@@` spells it out and validates it by exact sum:

```
[uint32 blockCount][blockCount x Header{srcOffset, compSize:u16, uncompSize:u16}][blocks]
```

A block is a fixed 32 KB of *decompressed* data, so `blockIndex = offset >> 15`
throughout, and **`compSize == 0` means STORED, not empty**. Two new record types with a
`static_assert` per offset are in `runtime/types/io/Stream.h`.

### Three quirks a tidied-up port would "fix" and break

1. **`SetWindow`'s scan tests the ACTIVE window's `start`, not the candidate's.**
   `TEST R8D,R8D ; JS` sits inside the loop but R8D is loaded once *before* it and never
   rewritten. When the active window is free, the scan skips every window and falls
   through to eviction even if a loaded window covers `pos`. It reads exactly like a
   copy-paste bug; "correcting" it changes which window is evicted and therefore the LRU
   for the rest of the stream.
2. **`GetAvailableWindow` keeps the LAST free window**, not the first — `CMOVL` with no
   break.
3. **Its LRU tie-break keeps the EARLIER index** (`JLE`, strictly-greater replaces). Every
   window is 0 on a fresh stream, so this decides the very first eviction.

## 19. The oracle: lockstep, state, and three configurations

Both of lane PICK's requirements were live and both are met.

* **State, not just bytes.** After *every* read the probe compares the returned bool, the
  bytes, `position`, `activeWindow`, and for every window its `useCount`, `start`, `end`
  and the **full 32 KB page** of any loaded window. A wrong block index leaves every
  counter agreeing and the page different.
* **Lockstep.** One read on each stream, compare immediately.
* **Right, not merely equal.** Each read is also compared against the ORIGINAL content —
  two identically wrong readers would agree with each other.

Isolation is structural: our `Read` calls our `SetWindow` by direct C++ call, the DLL's
calls its own by a direct relative `CALL` no registration can redirect.

### The effectiveness control was WRONG TWICE, and both times it said PASS

This is the part worth keeping.

1. **v1 counted changes of `activeWindow`.** `?Open@CompressedMemoryStream@IO@@` hardcodes
   **one** window, so `activeWindow` is 0 forever — it reported "1 window switch, LRU
   UNVERIFIED" on a run that in fact evicted on every block. The control fired, but for
   the wrong reason.
2. **v2 counted "reads that did not refill" and reported LRU EXERCISED.** That number is
   dominated by `SetWindow`'s **fast path** and says nothing about the scan. It was a
   false pass.
3. **v3 counts the branches INSIDE `SetWindow`.** Only a counter in the body can say which
   branch ran: `fastPath=221 scanHit=3 readMiss=19`.

And v3 showed the corpus was inadequate: **sequential reading never returns to a resident
block, so `scanHit` was 0 with any window count.** A third configuration was added — 4
windows plus a schedule that rewinds the position by two blocks — and `scanHit` went to 3.
Without it, three of the four bodies would have been "verified" by a green run that never
entered their interesting branch.

`inMemory=0 growDelegated=0` are write-side branches; a read-only probe leaving them at 0
is expected and is stated as such rather than counted as coverage.

## 20. What is NOT ported, and one named cutover blocker

`SetWindow`'s miss path forks on `Stream::direction` (+0x08). The **read** fork
(`direction == 0`) and the **in-memory** fork are ported. The third —
`direction != 0` with `blockIndex >= headerSize`, ~200 instructions of two inlined
`Collection::Array` growth loops that extend a stream being written — is **not**.

It is guarded, delegated to the shipped body, **counted, and announced once**. A partial
port that silently did nothing there would corrupt a save.

**At cutover that delegation is a null call. This is a named cutover blocker**, not an
oversight: `[CSRead] GROW path delegated` in a log is the tell that a run took it. It read
0 on every run here, which is expected for a read path and is not evidence it is dead —
saving is what reaches it.

**A SECOND blocker, which porting this rung CREATED rather than found.**
`GetAvailableWindow` flushes a dirty window before evicting it — `if (start >= 0 &&
direction == 1) CompressWindow(...)` — and `?CompressWindow@CompressedStream@IO@@`
(`0x1800a9810`) is unported. Before this lane nothing in the tree reached that name at
all; `unsatisfied_export_scan.py` now lists it as a **live** door, correctly: it is in no
diff guard, and every save that evicts a dirty window takes it. That is
`reference_porting_can_grow_the_cutover_surface.md` for the fifth time this wave, and it
is recorded here rather than left for the next census to find.

So the **write** side of this class now owes three things at cutover — the grow path,
`CompressWindow`, and whatever those pull in. The **read** side, which is the row this
lane was asked for, owes nothing.

Also declined: `?Open@CompressedStream@IO@@` (needs `SetWindowCount` and
`Array<Header>::SetSize`, is once-per-stream, and **this lane uses it to build its own
test subjects — it must not also be its author**), `?CompressWindow@`, `?Write@`,
`?Commit@`, `?PreCommit@`, `?Close@`.

## 21. Rung 1 now has real callers

`DecompressFromSource` calls `?Decompress@Compression@System@@` directly, so rung 1's
codec moved from probe corpora to actual stream blocks in the same run:

```
System::Compression::Decompress  calls=177 -> 196   worked=137 -> 156   divergences=0
```

Nineteen of those are real 32 KB blocks decoded through the ported stream stack. That is
the plan's bottom-up argument paying off rather than being asserted.

## 22. Three defects the runs found that reading did not

1. **`?Create@CompressedMemoryStream@IO@@` does not give its MemoryStream any storage.**
   Writing works until the first window fills; the flush then copies into a stream with no
   buffer — `REP MOVSB` with `RDI = 0` inside the DLL's memcpy, two calls downstream of the
   mistake, with nothing in the dump naming the probe.
2. **`?Open@MemoryStream@IO@@QEAA_NH@Z` is the wrong overload** — it sets
   `direction = 0` (reading). The writer needs `?Create@MemoryStream@IO@@QEAA_NH@Z`, which
   is the same body but for `MOV dword ptr [RBP+0x8],1`. Two exports, one letter apart in
   intent, and the compiler cannot tell them apart.
3. **The revisit schedule I added deadlocked the probe.** Rewinding `total` along with the
   position made progress net-negative (five reads advance ~13 KB, each revisit jumps back
   64 KB), so the loop never terminated and the harness killed the run at its timeout.
   `total` is now the read *budget* and only `position` rewinds.

## 23. `port_bypass_audit.py` caught a real bypass in this lane

It flagged the probe calling `??1CompressedMemoryStream@IO@@` through a raw door — **a
function Wave 4 lane B ported**. The `RT_RAW_OK` marker on those lines was borrowed from
`rt_data_door_audit.py`'s convention, which this audit does not share, and an exemption
would have been the wrong answer anyway: the *subject factory* deliberately uses the DLL
(a subject built by the code under test is worthless), but *teardown* has no such reason.
Routing it through `RT_EXPORT_ONCE` removed the bypass and exercises Wave 4's dtor for free.

Three more sites were flagged because their `RtDiffEnabled()` guard lived in the **caller**,
which the audit's per-site analysis cannot see. Rather than ask for an allowlist entry, each
helper now carries its own local guard — which also removes an invariant a future caller
could break silently. **`port_bypass_audit.py`: 0 bypasses on both doors.**

## 24. A reporting note worth carrying

`IO::CompressedStream::Read` reports `calls=0 worked=225`. **`calls=0` here means "this
site performs no self-comparison", not "this function never ran"** — `Read` and
`SetWindow` are verified by the lockstep probe, not by a comparison of their own. Reading
that column as execution coverage would be exactly backwards.

## 25. The next rung

**Rung 3: `?OpenFile@FileSystem@IO@@` (264 instrs) + `?ReadFile@` (90).** Win32, so a
`*_win.cpp`; lane FS already models `FileInfo` and already calls both through
`RT_EXPORT_ONCE`, so their signatures are settled. Then rung 4, `ReadPackfile` itself,
which is now a transcription onto two verified layers.

---

# Rung 3 — `FileSystem::OpenFile` / `ReadFile`

## 26. Ported (rung 3)

`runtime/src/io/filesystem_openfile_win.cpp`, own kill switch `BANISHED_RT_OPENFILE`.
It is a `_win.cpp` because `PlatformOpenFile` is `CreateFileW` / `GetFileSize` /
`SetFilePointer` / `::ReadFile` — PLAN §4.1 keeps `<windows.h>` out of the
portable half.

| symbol | addr | size | door |
|---|---|---|---|
| `?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z` | `0x1800ac880` | 264 | name + addr |
| `?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z` | `0x1800ace00` | 90 | name + addr |
| `?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z` | `0x1800adce0` | — | name + addr |
| `?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@FileSystem@2@XZ` | `0x180025400` | — | name + addr |
| `FileInfo::FileInfo(const FileInfo&)` | `0x18000c110` | — | addr only (unexported) |

Rung 3 is the layer that makes `ReadPackfile` reachable at all: once packages are
mounted, `OpenFile` is how **every** resource in the game is named, and `ReadFile`
is the recursion that turns a packed handle back into a read of the containing
`.pkg`. It has the widest blast radius of anything this lane has landed, which is
why it gets its own switch the integrator can remove in isolation.

### The two transcriptions worth naming

**`ReadFile`'s discriminator is UNSIGNED.** Retail tests
`(uint32_t)fi->handle > 0xFFFFFFFDu`, which is `handle == -1 || handle == -2` —
"no file" or "loose file" — in one compare. Read as a signed test it inverts.

**The pack index is the TOP BYTE, not a shift.** `handle` is
`(packIndex << 24) | fileIndex`; retail recovers the index with a byte load of
`((uint8_t*)&fi->handle)[3]`, not `>> 24`, so it is inherently unsigned and the
port spells it the same way.

## 27. A disagreement with lane FS's `FileInfo`, and three proofs

Lane FS's `FileInfo` (previously duplicated at namespace scope in
`filesystem_setupdirs_win.cpp`) declares `+0x08` as a `uint64_t size`. **That is
wrong**, and it makes `+0x0C` — the read cursor — invisible. The corrected layout
now lives once, in `runtime/types/io/FileSystem.h`:

```
+0x00 int32 handle       -1 none, -2 loose, else (packIndex<<24)|fileIndex
+0x04 int32 _unk04       never initialised by OpenFile
+0x08 int32 size
+0x0C int32 cursor       READ POSITION, advanced by ReadFile
+0x10 int32 baseOffset
+0x14 int32 _pad14       not copied by the copy ctor
+0x18 int64 winHandle
+0x20 String _name0
+0x28 String _name1
```

Three independent proofs, all from disassembly:

1. `ReadFile` **adds to `[rcx+0x0C]` and stores back a 32-bit value** — a field
   inside a 64-bit `size` cannot be separately incremented.
2. `PlatformOpenFile` stores `GetFileSize`'s 32-bit `EAX` into `+0x08` with a
   `MOV DWORD`, and separately zeroes `+0x0C`.
3. The **copy ctor at `0x18000c110` copies `+0x14` nowhere** — it moves
   `+0x00/+0x04`, `+0x08/+0x0C`, `+0x10` and `+0x18` and skips `+0x14`. A
   `uint64_t` at `+0x08` cannot produce that pattern.

**Nothing broke from the old layout only because its single reader casts to
`uint32_t` on a little-endian machine**, which happens to read the right half.
Any code that had written a size, or read the cursor, would have been wrong.
This is `reference_never_written_needs_by_whom` in a new dress: the field was
"never observed wrong" by a reader that could not observe it.

## 28. The oracle (rung 3)

`runtime/src/io/filesystem_openfile_probe.cpp`, run from the tail of
`LoadPackages` — the earliest point where an `OpenFile` of a **packed** name can
succeed at all, because the packfile index is what the loops above just built.

Lockstep, per lane PICK's rule: ours and theirs are opened side by side and
compared **after every single read**, not once at the end.

Compared: `handle`, `size`, `cursor`, `baseOffset`, handle **validity**, and
every byte read.

**Deliberately NOT compared, and why** (`reference_oracle_must_not_compare_unwritten_bytes`):
- `winHandle` — two opens of one file get two different `HANDLE`s *by
  construction*. Comparing them would fail 100% of loose opens and say nothing.
  Handle **validity** is compared instead, which is the property that matters.
- `+0x04` — retail's own uninitialised stack, copied out by `OpenFile`. Comparing
  it would be comparing bytes neither side wrote.

**Effectiveness control**: the run must produce a PACKED open (the BST hit plus
the `ReadFile` recursion), a LOOSE open (`CreateFileW`), **and** a MISS (the `-1`
return). Any one at zero makes the verdict `no divergence, but a PATH READS ZERO`,
not a pass. This lane got an effectiveness control wrong twice in rung 2, both
times while printing PASS; this one counts the three *outcomes* of the function,
which is the thing that cannot be satisfied by an early return.

### Result (run `of1`, identity gate OK: `21 / rung3 4`)

```
[OpenFile] lockstep probe: 8 opens (packed=5 loose=2 miss=1), 24 reads, 71471 bytes
           -- identical on handle, size, cursor, baseOffset, handle validity and every byte read
[RtDiff] ==== 313 sites, 1281624 calls, 1 divergences in 1 sites ====
```

All three control paths non-zero. The single divergence is
`Object::Factory::Startup [probe, not diffable] chain=416 nodes=415`, already
proved **pre-existing** by this lane's `BANISHED_RT_PKG2=0` A/B in §15b — it is
lane SYS2's, not rung 3's.

## 29. Audits

- `export_name_audit.py` — 663 names, 1 phantom, and it is the pre-existing
  `?_singleton@…` in `runtime/funcs/rt_resolve_cache.cpp`. None of rung 3's four
  names is phantom.
- `port_bypass_audit.py` — **0 bypasses on both doors.** The probe's raw doors
  are classified `OK-PROBE` / `OK-ORACLE` and carry a *local* `RtDiffEnabled()`
  guard, because that checker's guard analysis is per-site.
- `unsatisfied_export_scan.py` set-diff, **137 → 135**:
  - REMOVED: `?OpenFile@FileSystem@IO@@…`, `?ReadFile@FileSystem@IO@@…`
  - ADDED: **none.**

  `PlatformOpenFile` and `GetFileInfo` were not in the residue list — nothing
  outside a diff guard was asking for them — so porting them removes no name but
  costs none either.

## 30. What rung 3 adds to the cutover bill — one address, not two

The name residue scan is name-only and cannot see address doors. Rung 3 opens two,
and only one of them is new:

| addr | what | status |
|---|---|---|
| `0x18007eefc` | the CRT `_stricmp` | **already a shared blocker** — `filesystem_packages.cpp`, `filesystem_find_win.cpp` and `saveload.cpp` all name it. Rung 3 adds a caller, not a blocker. |
| `0x18000c200` | `FileInfo::operator=` | **NEW cutover blocker.** Unexported, unported; at cutover `ResolveAddr` returns null and this is a null call. |

Deliberately not ported, and listed so it is not mistaken for an oversight:
`?CloseFile@FileSystem@IO@@` (`0x1800accc0`, ord 1726) — the teardown half. The
probe closes through the shipped one on both sides, so it is compared on neither;
it belongs to whoever takes the teardown, not to rung 3.

Running total of this lane's named cutover blockers: `SetWindow`'s GROW path and
`?CompressWindow@` (rung 2), plus `0x18000c200` (rung 3).

## 31. Next predicted fault site

With rung 3 in, the DLL-less boot should get **past** `LoadPackages` and its
`OpenFile` of the first packed resource. The next thing to fail is, in order of
likelihood:

1. **`0x18000c200` `FileInfo::operator=`** — a null call, from *inside* our own
   `OpenFile`. Cheap to close and it is rung 4's first job.
2. **`?ReadPackfile@`** itself (`0x1800b46b0`) — rung 4. Everything below it is
   now ported and verified, so it is a transcription onto three known-good
   layers rather than an excavation.
3. **`?CloseFile@FileSystem@IO@@`** — whoever opens must close; the first
   teardown after a successful open will find it.

---

# Rung 4a — `FileInfo::operator=`, and what `ReadPackfile` actually needs

## 32. `FileInfo::operator=` is ported — rung 3's one blocker is closed

`0x18000c200`, 61 instructions, unexported, so it gets the address door only
(`RegisterRuntimeAddr(0x18000c200, …)`). `addr_door_audit.py` moved **44 → 43
UNREGISTERED**, which is the blocker closing and nothing else moving.

It also supplies a **fourth independent proof** of rung 3's `FileInfo`
correction: the moves are `+0x00`, `+0x04`, `+0x08`, `+0x0C`, `+0x10` and the
QWORD at `+0x18`, and `+0x14` is skipped — the same shape as the copy ctor. Two
separate DWORD moves at `+0x08` and `+0x0C` cannot come from a `uint64_t` at
`+0x08`.

### Three details a paraphrase gets wrong

1. **The self-assignment short-circuit skips the retain as well as the release.**
   `CMP RAX,[RDX+0x20]; JZ` jumps past both. Release-then-retain on a shared
   buffer drops the count to zero and frees a string both sides still hold.
2. **The adopt is unconditional; only the increment is guarded.** Retail stores
   the source pointer whatever it is, and only then tests it for the `INC`.
   This is why `RtString_AdoptInline` is **not** reused here — it skips the
   store when the source is null, which would leave the destination holding the
   dangling `data - 8` that the freeing branch of the release deliberately
   leaves behind.
3. **`AdjustDelimiters` runs on both paths.** `LEA RCX,[RSI+0x20]; CALL` sits at
   the merge point the short-circuit jumps to, so it runs even when nothing was
   copied.

`AdjustDelimiters` is called **directly** (lane String's port), not through a
door: it is already ported, so a door here would only be a way to reach the
shipped copy by accident.

## 33. The oracle, and why rung 3's corpus was not enough

The only `operator=` call site rung 3 exercises is `PlatformOpenFile`'s "reset
to blank", where the destination's `Filename`s are **always already null**. That
site alone leaves the release path, the short-circuit and the AddRef entirely
unentered — while reporting green. Precisely the effectiveness failure this lane
hit twice in rung 2. So rung 4 **drives** the branches instead of waiting for
them:

| case | setup | branch proved |
|---|---|---|
| 1 | distinct buffers, dst refcount 2 | release takes the **sharing** branch |
| 2 | distinct buffers, dst refcount 1 | release takes the **freeing** branch |
| 3 | **same buffer** both sides | the short-circuit (a naive port frees a live string here) |
| 4 | **null source** | the store is unconditional (`AdoptInline` would skip it) |

Compared as **state, not just result**: all six scalars including `cursor`,
`+0x14`, buffer **identity** (adopted, never copied), and — the half pointer
equality cannot see — the **refcount** of every buffer involved. A missing
AddRef leaves the pointers identical and only the refcount wrong.

Ours and theirs get two **independent but identical** pairs, because `operator=`
destroys its destination (`reference_oracle_must_snapshot_aliased_inputs`); one
shared destination would have ours overwrite what theirs was about to read.

The probe **skips and says so** when `0x18000c200` resolves null, rather than
reporting a pass over zero comparisons — that is the state it will be in after
cutover.

### Result (run `as1`, identity gate OK)

```
[Assign] operator= probe: 4 cases (freeing=1 sharing=1 shortCircuit=1 nullSrc=1)
         -- identical on all 6 scalars, +0x14, buffer identity and refcount
[OpenFile] lockstep probe: 8 opens (packed=5 loose=2 miss=1), 24 reads, 71471 bytes -- identical
[RtDiff] ==== 315 sites, 1275525 calls, 1 divergences in 1 sites ====
```

All four branches non-zero. The one divergence is the pre-existing
`Object::Factory::Startup chain=416`. Audits: `port_bypass_audit` 0 on both
doors; `export_name_audit` 1 phantom, the pre-existing `?_singleton@`.

## 34. `ReadPackfile` — it IS a transcription, and it is still too big for one rung

I said in the rung-3 report that `ReadPackfile` would be a transcription onto
verified layers. **That holds, and I can now show it rather than assert it.**
Its complete call set, mapped against the tree:

| callee | what | status |
|---|---|---|
| `0x1800a9aa0` | `CompressedStream::SetWindow` | **PORTED — rung 2** |
| `0x1800aa780` | `CompressedStream::Read` | **PORTED — rung 2** |
| `0x1800ac880` | `FileSystem::OpenFile` | **PORTED — rung 3** |
| `0x1800ace00` | `FileSystem::ReadFile` | **PORTED — rung 3** |
| `0x18000c200` | `FileInfo::operator=` | **PORTED — rung 4a** |
| `0x1800c7a50` / `0x1800c7ad0` | `MemAlloc` / `MemFree` | PORTED — lane E |
| `0x1800c8220` | `String::SetLength` | PORTED — lane String |
| `0x1800c6970` | `Filename::AdjustDelimiters` | PORTED — lane String |
| `0x1800c9fc0` / `0x1800c9b60` | UTF decode / encode | known, `string.cpp` |
| `0x18008a290` | CRT `memcpy` | `std::memcpy` is byte-faithful |
| `0x180095060` | `__chkstk` (the `_alloca(0x8020)`) | known — rung 2 |
| `0x1800aa3d0` / `0x1800aa560` | `CompressedStream` ctor / `Open` | **CORRECTED in §48** — the ctor is already lane W's; and this row wrongly implied a stack object separate from the MemoryStream below. They are ONE `CompressedMemoryStream`. |
| `0x1800ae710` | `MemoryStream` ctor | **CORRECTED in §48** — already lane W's, and it constructs the `+0x68` half of the SAME object as the row above. |
| `0x1800a9560` | `~CompressedMemoryStream` (NOT `~CompressedStream` — **corrected in §48**) | **already ported** |
| `0x1800a9ee0` | `?Commit@CompressedStream@IO@@` (88 insns) | **unported, and already LIVE** — see §49 |
| `0x180029510` / `0x180029630` | `Array<T>::Resize` variants (67 / 65) | **unported** |
| `0x1800298f0` | `PackageFile` entry insert (114) | **unported** |
| `[RAX+0x38]` @ `0x1800b6042` | `MemoryStream::Close`, slot 7 | **PORTED, rung 4b-ii** — a bare `RET` |
| `[0x18014b0f0]` @ `0x1800b47e3` | `KERNEL32!SetFilePointer` | identified, rung 4b-i |

**The repeated `SetWindow` + `memcpy` pair appears ~25 times** — that is exactly
the inlined `CompressedStream::Read` I predicted in the decomposition, and both
halves of it are already ported. So there is no excavation left in the *hot*
part of the function.

**But the arithmetic is against doing it in one rung**: 1,864 instructions of
`ReadPackfile` itself, plus ~370 instructions of new helper ports (the two
`Array::Resize`, the entry insert, `~CompressedStream`, `0x1800a9ee0`), plus a
vtable slot and an import still to pin. Taking the coordinator at their word
that *"the decomposition has earned the right to grow another rung"*, I am
splitting rather than pushing through:

* **Rung 4b — the substrate.** The five unported helpers above, plus pinning
  slot 7 and the import. Each is small and independently oracle-able; the two
  `Array::Resize` bodies are almost certainly lane Q's `Collection::Array`
  already and may cost nothing but a registration.
* **Rung 4c — `ReadPackfile` itself**, in the three pieces the plan already
  named: the file-header read, the metadata block (~25 inlined `Read`s plus the
  `SetLength` traffic), and the entry directory.

Landing 4a now is what matters for the ladder, because it is the piece that was
blocking the layer already landed: without it, our own `OpenFile` calls a null
at cutover.

## 35. Next predicted fault site

Rung 4a does **not** move the ladder on its own, and I do not want that
overstated — `LoadPackages` will still stop at the `ReadPackfile` call. What 4a
changes is that the layer beneath it is now null-free: rungs 2, 3 and 4a
together mean every callee `ReadPackfile` makes on its hot path resolves to one
of our own bodies.

Predicted order from here:

1. **`ReadPackfile` (`0x1800b46b0`)** — unchanged, and now the *only* thing
   between `LoadPackages` and a mounted package.
2. **`0x1800aa560` `CompressedStream::Open`** — the first call `ReadPackfile`
   makes that is still unported, so rung 4b's first target.
3. **`?CloseFile@FileSystem@IO@@`** — still deliberately out of scope, still the
   first teardown after a successful open.

---

# Rung 4b (part 1) — the two excavation risks are gone, and neither was a blocker

## 36. Both risks resolved, and one of them the tree already knew

The integrator flagged the unpinned vtable slot and the unidentified import as
the two things most likely to turn 4b into an excavation. Neither did.

**The import `[0x18014b0f0]` is `KERNEL32!SetFilePointer`** — read out of the PE
import table with `pefile`, not guessed. Nothing to port; it is the seek before
the header read.

**The virtual call `CALL [RAX+0x38]` at `0x1800b6042` is `MemoryStream`'s slot 7,
`0x1800ae9e0`, which is a bare `RET`.** So the call is a provable no-op at this
site — but it is still transcribed as a virtual call, because that is what
retail does and the object's dynamic type is only *provably* `MemoryStream`
here.

**How the slot was pinned, and the trap in the middle of it.** The receiver is
the stack object at `[RBP-0x58]`, constructed by `0x1800ae710`
(`MemoryStream::MemoryStream`). That constructor stores **two** vtables into the
same slot:

```
1800ae72d: LEA RAX,[0x1801549a0]   ; the BASE (Stream) vtable
1800ae734: MOV qword ptr [RCX],RAX
...
1800ae740: LEA RAX,[0x1801555b0]   ; the DERIVED (MemoryStream) vtable
1800ae747: MOV qword ptr [RCX],RAX ; overwrites it
```

Slot 7 of the **base** vtable `0x1801549a0` is **null**; slot 7 of the derived
`0x1801555b0` is `0x1800ae9e0`. Stopping at the first `LEA` — the natural thing
to do, since it is the one that looks like "the vtable store" — yields
"slot 7 is a null call", which is wrong and is exactly the shape of this
project's three `Slot6`/`Slot7` mix-ups. The base vtable also has slots 2..6 all
equal to one address, which reads like a plausible stub table and makes the
wrong answer look self-consistent.

**And then: `runtime/types/io/Stream.h:110` already says
`[7] 0x1800ae9e0 Close   <-- +0x38`.** I derived it independently from the
binary and only then found it written down. The independent derivation is worth
keeping as corroboration, but the fact stands: this was another stale blocker
that the tree could already answer
(`reference_holdback_blockers_are_stale_check_them_first`). I should have grepped
`Stream.h` before opening the PE.

## 37. The two `Array::SetSize` bodies — a port, not just a registration

The integrator's guess was half right. They *are* members of lane Q's family, but
**neither is byte-identical to any member already registered**, so each needed
its own entry rather than a bare `RegisterRuntimeAddr`. Both specs are read out
of the bodies:

| addr | evidence in the body | spec |
|---|---|---|
| `0x180029510` | `SHL RCX,0x2`, `MOV EDX,0x4`, `MOV EAX,[RSI+RCX*4]; MOV [RDX],EAX`; the copy loop `JGE`s past the tail | `kSpec_I32` (tail **uninitialised**) |
| `0x180029630` | **no shift at all** (element size 1), `MOV EDX,0x1`, `MOVZX EAX,byte [RCX+RDI]; MOV [RDX],AL`; tail likewise skipped | `kSpec_Bool` (tail **uninitialised**) |

Byte-similarity agrees (first difference at 93 and 89 — a shared ~90-byte
prologue, then the element op) but that is the **corroboration, not the
evidence**: the family is large and near-identical by construction, so "looks
like member X" is precisely the reasoning that would pick the wrong spec.

That brings lane Q's two-argument family to **nine**.

## 38. These two registrations are NOT verified, and I will not claim they are

Run `arr1` is clean — `315 sites, 1170772 calls, 1 divergence` (the pre-existing
`Object::Factory::Startup`), rung 3's and rung 4a's probes still green — **but
neither new site appears in the diff table at all, because neither was called.**

They are reachable only from `ReadPackfile`, which is still the shipped body, and
the DLL reaches its own copies by direct relative `CALL`; our registration
redirects only *our* callers. So this is exactly
`reference_uncalled_port_is_not_verified`: `divergences=0` here is a statement
about zero calls.

They will first execute when rung 4c's `ReadPackfile` calls them, and that is the
run that verifies them. Recorded as an open item rather than a landed one.

## 39. `CompressedStream::Open` has its own substrate — 4b is not one layer

`0x1800aa560` disassembles to something short and readable:

```
this->_source(+0x28) = src;  this->+0x8 = 0;
0x1800aa020(this, windowCount);              // window array init
len = src->vtbl[6]/*GetSize*/(src);
if ((unsigned)len < 4) return false;
src->vtbl[3]/*Read*/(src, 4, &blockCount);
if (blockCount * 8 > len) return false;
Array<Header>::SetSize(this+0x40, blockCount);          // 0x180024d70
src->vtbl[3](src, this->+0x48 * 8, this->+0x40);        // the block directory
total = 4 + blockCount*8;
for each 8-byte entry: w = word[0]; if (!w) w = word[2]; total += w;
return total == len;                                     // a consistency check
```

But it stands on **two more unported bodies** — `0x1800aa020` (window array
init) and `0x180024d70` (`Array<Header>::SetSize`) — plus five `MemoryStream`
virtuals (`0x1800ae8f0` Write, `0x1800ae950` Read, `0x1800ae9b0` SetCurrentPos,
`0x1800ae9c0` GetCurrentPos, `0x1800ae9d0` GetSize). `compressed_stream_read.cpp:472`
already names the first two as `Open`'s dependencies and notes they are not on
the read path, which is why rung 2 correctly left them alone.

So 4b is itself two layers, and I am naming that rather than discovering it
mid-port:

* **4b-i (this commit)**: the two risks resolved, the two `Array::SetSize`
  instantiations.
* **4b-ii**: `MemoryStream` — its ctor, its five virtuals and slot 7 — then
  `0x1800aa020`, `0x180024d70`, and only then `CompressedStream::Open`, its ctor,
  `~CompressedStream` and `0x1800a9ee0`.

`MemoryStream` first is not the order the call graph suggests, but it is the
order the *oracle* needs: `Open` cannot be diffed at all until the stream it
reads through is ours on both sides.

## 40. Next predicted fault site

Unchanged and unmoved, deliberately: `LoadPackages` still stops at the
`ReadPackfile` call. Within 4b, the next thing to touch is `MemoryStream`'s
ctor + five virtuals, because every other item in the rung needs them to be
diffable.

---

# Rung 4b-ii (part 1) — `IO::MemoryStream` is complete

## 41. Sequencing by what the oracle needs, not by the call graph

Stated explicitly, since the integrator asked for it to be on the record.

`CompressedStream::Open` reads its header and block directory **through** a
Stream, and in `ReadPackfile` that Stream is a `MemoryStream` over the
freshly-read file header. An oracle that runs ours and theirs side by side would
therefore have one of them advancing a cursor the other cannot see — so **`Open`
is not diffable until the stream it reads through is ours on both sides.** The
call graph says `Open` first; the oracle says `MemoryStream` first, and the
oracle wins. That is the same rule the whole bottom-up decomposition rests on:
a rung is ready when it can be *verified*, not when its callee list is short.

## 42. Ported (4b-ii): six virtual bodies

`runtime/src/io/memory_stream.cpp`, switch `BANISHED_RT_MEMSTREAM`.

| symbol | addr | note |
|---|---|---|
| `?Read@MemoryStream@IO@@UEAA_NHPEAX@Z` | `0x1800ae950` | |
| `?Write@MemoryStream@IO@@UEAA_NHPEBX@Z` | `0x1800ae8f0` | |
| `?SetCurrentPos@MemoryStream@IO@@UEAA_NH@Z` | `0x1800ae9b0` | 3 instructions |
| `?GetCurrentPos@MemoryStream@IO@@UEBAIXZ` | `0x1800ae9c0` | `UEBAI` → **unsigned** |
| `?GetSize@MemoryStream@IO@@UEBAIXZ` | `0x1800ae9d0` | `UEBAI` → **unsigned** |
| `?Close@MemoryStream@IO@@UEAAXXZ` | `0x1800ae9e0` | slot 7, a **bare RET** |

All six names verified against the shipped export table first. The layout needed
no derivation at all — `Stream.h` already carried it, disassembly-pinned.

**Three transcriptions that a cleanup would break:**

1. **The bound check in `Read`/`Write` is 32-bit and signed.** Retail sign-extends
   cursor and count to 64 bits (`MOVSXD`) and then *narrows again* for the test:
   `LEA EAX,[RCX+RDI*1]` is a 32-bit add and `CMP EAX,[RBX+0x28]; JG` a 32-bit
   signed compare. Writing it in 64-bit changes behaviour for a sum that
   overflows int32 — retail's **wraps** and then compares small. The offset
   arithmetic that follows really is 64-bit, so the two widths are transcribed
   separately rather than unified.
2. **`SetCurrentPos` has no bounds check at all.** A seek past the end succeeds;
   the following `Read` is what fails. A port that "helpfully" validated would
   move which of the two calls reports the error.
3. **`Write` does not grow the buffer.** Past `_size` it fails rather than
   reallocating.

**No inline replay in any of the six.** `RT_DIFF_SCALAR` runs the shipped body a
second time on the same object — exactly wrong for four of these, because they
mutate `_cursor`: a replay would advance it twice and manufacture a divergence
in a correct port. The bodies only declare that they did work; verification
lives in the probe.

## 43. The oracle

`memory_stream_probe.cpp`. Each side gets its **own** stream over its **own**
copy of the same buffer, stepped in lockstep with the full state compared after
**every** call. Compared: return value, `_cursor`, `_size`, and the bytes — and
for `Write` the **destination buffer**, because a `Write` that returns true and
copies nothing is otherwise indistinguishable from a correct one.

The stream is built by hand rather than through the constructor: the six bodies
touch only `_data`/`_cursor`/`_size`, and building it by hand keeps the probe
from depending on a constructor this rung does not own.

The corpus **straddles the 32-bit signed compare** the port refuses to clean up:
chunks summing to exactly `_size` (must succeed), one byte more (must fail), and
a seek past the end followed by a read (seek succeeds, read fails).

**Effectiveness control counts outcomes, not calls.** Each bulk body has two —
the copy and the refusal — and a corpus that only ever succeeds leaves the bound
check unentered while reporting green.

### Result (run `ms1`, identity gate OK)

```
[MemStream] lockstep probe: 16 calls (copied=12 refused=3 seeks=2 getters=2)
            -- identical on return, _cursor, _size and every byte
[RtDiff] ==== 321 sites, 1171789 calls, 1 divergences in 1 sites ====
```

All four outcomes non-zero. The one divergence is the pre-existing
`Object::Factory::Startup`. Audits: `port_bypass_audit` **0 on both doors**;
`export_name_audit` 1 phantom, the pre-existing `?_singleton@`.

## 44. The correction: the ctor and dtor were ALREADY PORTED, by lane W

I had written, and nearly committed, that `MemoryStream`'s constructor and
destructor were "4b-ii's next step". **They are lane W's, and they already
exist**:

* `0x1800ae710` `??0MemoryStream@IO@@QEAA@XZ` → `RtIo_MemoryStream_Ctor`
* `0x1800ae890` `??1MemoryStream@IO@@UEAA@XZ` → `RtIo_MemoryStream_Dtor`

both in `runtime/src/io/compressedstream.cpp`.

**What caught it was the run's own diff table**, which listed four
`IO::MemoryStream::~MemoryStream` sites that this file never declared. Not
review, not planning — the instrument.

So: **with the six bodies above, `IO::MemoryStream` is complete across the two
lanes.** That is a better outcome than the plan predicted, and it shrinks 4b-ii.

This is the *same* transport failure as 4b-i's `Stream.h` finding, twice in one
rung. The rule earned there needs a sibling:

> Grep the types header before opening the PE — **and grep for the port before
> planning to write it.**

## 45. One blocker that is lane W's, not this rung's

Lane W's ctor allocates an `IO::Encoding` carrying vtable `0x1801554a8`, whose
five slots are all unexported `.text` internals that mint as **named traps**.
Construction no longer calls through null, but the first *virtual* call on that
Encoding still traps at cutover. **Nothing in the six bodies above touches the
Encoding**, so this rung neither fixes nor worsens it — recorded so the
completeness claim in §44 is not read as more than it is.

## 46. What is left in 4b, and the next predicted fault site

Remaining, in oracle order: `0x1800aa020` (window array init) and `0x180024d70`
(`Array<Header>::SetSize`), then `CompressedStream::Open` `0x1800aa560`, its
ctor `0x1800aa3d0` (also lane W's — **check before writing**),
`~CompressedStream` `0x1800a9560` and `0x1800a9ee0`.

Predicted fault site is unchanged and deliberately so: `LoadPackages` still
stops at the `ReadPackfile` call, and nothing in 4b moves it. Within 4b the next
touch is `0x180024d70`, because `Open` cannot be diffed without it either.

**Still open from 4b-i**: the two `Array::SetSize` instantiations
(`0x180029510`, `0x180029630`) remain **registered but never called**, and are
verified by the run in which 4c first reaches them — not before.

---

# Rung 4b-ii (part 2) — a structural correction to my own §34, and 4b's real remainder

## 47. The rule caught three more before they cost anything

Applied to all six remaining 4b items **before** touching any of them, per the
rule 4b-i and 4b-ii part 1 earned. Three of the six were already ported:

| addr | what | status |
|---|---|---|
| `0x1800aa3d0` | `??0CompressedStream` | **already ported** — lane W, `compressedstream.cpp:957` |
| `0x1800a9560` | `~CompressedMemoryStream` | **already ported** — `compressedstream.cpp:932` |
| `0x1800ae710` | `??0MemoryStream` | **already ported** — lane W (found in part 1) |

Nothing was written for any of them. That is the rule working *before* the cost,
rather than the instrument catching it after.

## 48. THE CORRECTION: `ReadPackfile` builds ONE `CompressedMemoryStream`, not two objects

My §34 callee map listed `0x1800aa3d0` (`CompressedStream` ctor) and
`0x1800ae710` (`MemoryStream` ctor) as constructing **two separate stack
objects**. **That is wrong, and it changes 4c's shape.**

What made me look: the tree labels `0x1800a9560` as `~CompressedMemoryStream`
(sizeof `0x98`), but `ReadPackfile` calls it at `0x1800b6054` on `[RSP+0x40]` —
the object it built with the *`CompressedStream`* ctor, which is `0x68` bytes.
Calling a `0x98`-byte destructor on a `0x68`-byte object would be a defect in
either retail or the tree's label, so one of my readings had to be wrong.

**Mine was.** The disassembly settles it two ways:

1. `~CompressedMemoryStream` at `0x1800a9560` works on `RBX + 0x68` — it calls
   slot 7 there, stamps `0x1801555b0` (the `MemoryStream` vtable), frees `_data`
   if `_ownsData`, stamps `0x1801549a0` (the `Stream` vtable), frees the
   Encoding, and then **tail-jumps to `0x1800aa650`**, which is the real
   `~CompressedStream`. That is precisely a `CompressedMemoryStream`:
   `CompressedStream` at `+0x00`, `MemoryStream` at `+0x68`.
2. The frame arithmetic proves the two stack slots are that one object.
   `ReadPackfile`'s prologue is `LEA RBP,[RSP-0x1860]` **before**
   `SUB RSP,0x1960`, so **`RBP = RSP + 0x100`**, and therefore
   **`RBP - 0x58` == `RSP + 0xA8` == `(RSP + 0x40) + 0x68`.**

MSVC simply addressed the two halves of one object through two different base
registers, and I read them as two objects. Consequences, all of which make 4c
*simpler*:

* The destructor `ReadPackfile` calls is the **already-ported**
  `~CompressedMemoryStream`. `~CompressedStream` (`0x1800aa650`) is reached only
  through its tail-jump, never directly from `ReadPackfile`.
* The virtual `CALL [RAX+0x38]` at `0x1800b6042` is `MemoryStream::Close` on
  **this object's own `MemoryStream` half** — the bare `RET` ported in part 1.
* `CALL 0x1800aa560` at `0x1800b4864` is
  `cms.CompressedStream::Open(&cms.memoryStream, n)` — the same shape
  `compressedstream.cpp:382` already describes for
  `?Open@CompressedMemoryStream@IO@@`.

**The lesson is not "read more carefully".** Two stack slots addressed off
different base registers look like two objects *by construction*, and nothing in
the call list contradicts it. What contradicted it was a **type-size
disagreement** — a `0x98` destructor on a `0x68` object — surfaced by the tree's
own label. Cross-checking a callee's label against the size of what it is called
on is a cheap, mechanical check that this lane will now do on every by-address
call to a destructor.

## 49. 4b's real remainder — five items, and one is ALREADY a live blocker

| addr | what | insns | note |
|---|---|---|---|
| `0x1800aa020` | `CompressedStream::SetWindowCount` | 52 | unported |
| `0x180024d70` | `Array<Header>::SetSize` | — | unported; another member of lane Q's family |
| `0x1800aa560` | `CompressedStream::Open` | ~50 | unported; needs the two above |
| `0x1800a9ee0` | `?Commit@CompressedStream@IO@@` | 88 | **unported AND ALREADY LIVE** |
| `0x1800aa650` | `~CompressedStream` | — | unported; reached by the ported CMS dtor's tail-jump |

**`Commit` is not a new 4b cost — it is an existing cutover blocker this rung
merely found.** Lane W's *ported* `~CompressedMemoryStream` reaches it through
`RT_EXPORT_ONCE(..., "?Commit@CompressedStream@IO@@QEAAXXZ")` at
`compressedstream.cpp:327` and `:429`, and there is no port behind that name. So
a ported body already calls a name that resolves to the shipped DLL today and to
nothing at cutover — `reference_converting_a_door_is_not_satisfying_it`, exactly.
`~CompressedStream` at `0x1800aa650` is in the same position via the tail-jump.
Both are named here so they are attributed to the right lane rather than
appearing as 4b overhead.

## 50. Next predicted fault site

Unchanged: `LoadPackages` still stops at the `ReadPackfile` call, and nothing in
4b moves it. Within 4b the next touch is `0x180024d70` then `0x1800aa020`,
because `Open` cannot be diffed without both.

**Still open, still not counted**: 4b-i's two `Array::SetSize` instantiations
(`0x180029510`, `0x180029630`) remain **registered but never called** — to be
closed explicitly in the run where 4c first reaches them.

---

# Rung 4b-ii (part 3) — `Array<Header>::SetSize`, and an alignment that similarity got wrong

## 51. `0x180024d70` needed a NEW spec, and the difference is one field

`Array<CompressedStream::Header>::SetSize` — the block directory `Open`
allocates. It joins lane Q's two-argument family as its **tenth** member, but
**not** on an existing spec.

Byte-similarity puts it closest to `kSpec_U64Zero` (first difference at 88 of
352), and `kSpec_U64Zero` agrees on **element size 8**, on **`Assign8`** and on
**`InitZero8`**. It disagrees on exactly one field:

| | element size | **alignment** | assign | tail |
|---|---|---|---|---|
| `0x180008ff0` (`kSpec_U64Zero`) | 8 (`SHL RCX,3`) | **8** (`MOV EDX,0x8`) | `Assign8` | zeroed |
| `0x180024d70` (this one) | 8 (`SHL RCX,3`) | **4** (`MOV EDX,0x4`) | `Assign8` | zeroed (`MOV [RDX],R10`, R10==0) |

Both `MOV EDX` values were read out of the disassembly of both functions, not
inferred. Reusing `kSpec_U64Zero` because it "looks like" this one would have
passed **a wrong alignment to `MemAlloc` on every packfile block directory** —
and it would have passed every functional test, because over-alignment is
invisible until something depends on the allocator's boundary tags.

This is the second time in two rungs that this family has tried to give a wrong
answer by resemblance, and the second time the *only* thing separating the right
spec from the plausible one was a single immediate. `kSpec_U64ZeroA4` is
therefore named for what distinguishes it, not for its element type.

Run `ar2`: `321 sites, 1189763 calls, 1 divergence` (the pre-existing
`Object::Factory::Startup`); rungs 3, 4a and 4b-ii probes all still green.

**Like its two siblings from 4b-i, this registration is NOT yet verified** — it
is reachable only from `Open`, which is still the shipped body. Three
`Array::SetSize` instantiations are now registered-but-uncalled and all three
close in the run where 4c first reaches them.

## 52. `SetWindowCount` reads clean, and reveals one more layer

`0x1800aa020`, 52 instructions:

```
Array<Window>::SetSize(this+0x30, n);              // 0x180024c10
for (i = 0; i < n; ++i) {
    EnterCriticalSection(allocator + 0x640);
    buf = MemoryAllocator::Alloc(allocator, 0x8000, 4);   // 0x1800c6e60
    LeaveCriticalSection(allocator + 0x640);
    windows[i]._buffer(+0x10) = buf;                       // stride 0x18
    memset(buf, 0, 0x8000);                                // 0x1800881b0
}
this->_activeWindow(+0x20) = GetAvailableWindow(this);     // 0x1800a9980 — PORTED, rung 2
```

`0x8000` is `kCompressedStreamWindowSize` and `0x18` the `Window` stride, both
already in `Stream.h` — the body agrees with the header on both, which is a
useful independent check on rung 2's layout.

But it needs **`0x180024c10`, an ELEVENTH `Array::SetSize`** —
`Array<Window>`, and its shape is already read: `LEA RCX,[RAX+RAX*2]; SHL RCX,3`
is element size **0x18**, `MOV EDX,0x8` is align **8**, and the copy is three
qword moves. Its tail behaviour is the one field still to read.

## 53. Where I stopped, and why

The chain `Open → SetWindowCount → Array<Window>::SetSize` revealed its next
layer *while I was reading it*, which is the third time in this rung that a
"remaining item" has turned out to have its own substrate. That is not a
surprise to be pushed through; it is the decomposition telling me where the next
boundary is.

Landed and verified here: `kSpec_U64ZeroA4` and the `0x180024d70`
instantiation, on a clean run.

Not started, and deliberately: `SetWindowCount`, `Open`, `Commit` and
`~CompressedStream`. `Open` cannot be diffed until `SetWindowCount` is ours, and
`SetWindowCount` cannot be ported until `0x180024c10` is — so writing any of the
three now would mean writing them without an oracle, which is the one thing this
lane has consistently refused to do.

**The next unit of work is exact and small**: read `0x180024c10`'s tail
behaviour, add its spec and entry, port `SetWindowCount` (52 instructions),
then `Open` (~50) with a lockstep probe over a real packfile header — at which
point `Commit` and `~CompressedStream`, lane W's two rows, close in the same
file.

## 54. Next predicted fault site

Unchanged: `LoadPackages` still stops at the `ReadPackfile` call. Nothing in 4b
moves it, and attempt 17 is correctly still held for 4c.

---

# Rung 4b-ii (part 4) — `Array<Window>` lands, and the chain BOTTOMS OUT

## 55. `0x180024c10` — `Array<CompressedStream::Window>::SetSize`

Element stride **0x18** (`LEA RCX,[RAX+RAX*2]; SHL RCX,3`), align **8**
(`MOV EDX,0x8`), copy is three plain qword moves. Unlike `kSpec_U64ZeroA4` there
was no lookalike to be tempted by: it differs from every existing spec on both
the element size *and* the tail init.

**Its tail init leaves four bytes unwritten, and that matters for any future
oracle.** The `else` arm is exactly three stores:

```
MOV dword ptr [RCX],R11D       -> [d+0x00] = 0    (4 bytes)
MOV qword ptr [RCX+0x4],-0x1   -> [d+0x04] = -1   (8 bytes, UNALIGNED, +0x04..+0x0b)
MOV qword ptr [RCX+0x10],R11   -> [d+0x10] = 0    (8 bytes)
```

so **`+0x0c..+0x0f` is never written** even though the stride is `0x18`. Any
oracle over a grown `Array<Window>` must exclude those four bytes or it compares
memory neither side wrote — `reference_oracle_must_not_compare_unwritten_bytes`,
recorded now rather than discovered later as a 100%-failure diff. The unaligned
8-byte `-1` is retail's own, and is reproduced rather than split into two
aligned stores.

Run `aw1`: `321 sites, 1286502 calls, 1 divergence` (the pre-existing one).

**The registered-but-uncalled count is now FOUR**, not three: `0x180029510`,
`0x180029630`, `0x180024d70`, `0x180024c10`. All four are reachable only from
`SetWindowCount` / `Open` / `ReadPackfile`, all still shipped bodies. They close
together in the run where 4c first reaches them — and if any one of them is
*still* not reached then, that is a finding about the corpus, not an oversight,
and I will say so.

## 56. THE DEPTH ANSWER: two more rungs, not ten

The integrator asked for a remaining-depth estimate if the chain revealed a
fourth layer. **It revealed one, and then it stopped** — every leaf now
terminates in something already ported, in lane Q's framework, or in the CRT.
The full closure, checked by grep and disassembly rather than estimated:

| callee | reached from | status |
|---|---|---|
| `0x1800881b0` memset | `SetWindowCount` | **not a port** — already `std::memset` (`filesystem_setupdirs_win.cpp:381`) |
| `0x180024c10` `Array<Window>::SetSize` | `SetWindowCount` | **PORTED, this commit** |
| `0x1800a9980` `GetAvailableWindow` | `SetWindowCount` | **ported, rung 2** |
| `0x180024d70` `Array<Header>::SetSize` | `Open` | **ported, part 3** |
| `0x180024aa0` Array dtor | `~CompressedStream` | **already ported, lane Q** |
| `0x1800a9810` `CompressWindow` | `Commit` | **already a KNOWN blocker** — named by this lane's own rung 2, write path only |
| `0x1800aa100` `Release` | `Commit`, `~CompressedStream` | unported, small |
| `0x1800249c0`, `0x1800248e0` | `~CompressedStream` | unported Array dtors — lane Q `J1_ENTRY` family, one line each if they match |

**Nothing below this line is new depth.** So the remaining work is:

* **Rung 4b-iii — seven bodies, one rung.** `SetWindowCount` (52),
  `Open` (~50), `Release`, two `J1_ENTRY` array-dtor lines, `Commit` (88) and
  `~CompressedStream`. The last two are lane W's rows, closed by this lane.
* **Rung 4c — `ReadPackfile` itself** (1,864 instructions), which after 4b-iii
  is a transcription with every callee ported.

**Two more rungs.** My confidence is high on 4b-iii being one rung — every item
is sized and its dependencies are closed — and moderate on 4c being one, because
1,864 instructions may want splitting into the three pieces the plan already
names. It is **not** three-to-ten more rungs, and I would not put a second lane
on this chain: 4b-iii is a single file's worth of work whose pieces share one
oracle, and splitting it across lanes would cost more in coordination than it
saves.

The honest caveat: my depth estimate was implicitly "one more layer" three times
this rung and was wrong each time. What is different now is that the estimate is
no longer an extrapolation — it is an enumerated closure, every leaf of which I
have either grepped to an existing port or disassembled. That is the same
distinction as similarity-versus-evidence in §51.

## 57. Next predicted fault site

Unchanged: `LoadPackages` stops at the `ReadPackfile` call. Nothing in 4b moves
it; attempt 17 remains correctly held for 4c.

---

# Rung 4b-iii (part 1) — lane W's two rows close, and they are LIVE

## 58. `Release` and `~CompressedStream` — ported, and actually executing

`runtime/src/io/compressed_stream_release.cpp`, switch `BANISHED_RT_CSRELEASE`.
Both mangled names verified against the shipped export table first.

| symbol | addr |
|---|---|
| `?Release@CompressedStream@IO@@QEAAXXZ` | `0x1800aa100` |
| `??1CompressedStream@IO@@UEAA@XZ` | `0x1800aa650` |

Plus two `J1_ENTRY` array-dtor lines in lane Q's family (`0x1800249c0`,
`0x1800248e0` — the `+0x40` and `+0x30` members `~CompressedStream` destroys).
Neither is byte-identical to the already-ported `0x180024aa0` it calls at `+0x50`
(first difference at byte 39 of 256 for both), so they get their own entries
rather than a shared registration.

**These are lane W's rows, closed by this lane**, and billed that way.

### They ran, and the count proves the path

Run `rel1`: `326 sites, 1188115 calls, 1 divergence` (the pre-existing
`Object::Factory::Startup`), clean shutdown, empty crash log.

```
IO::CompressedStream::~CompressedStream            calls=0   worked=6
IO::CompressedStream::Release                      calls=0   worked=6
IO::CompressedMemoryStream::~CompressedMemoryStream calls=6   worked=?   <-- lane W's
```

**6 against 6.** Lane W's ported `~CompressedMemoryStream` tail-jumps into
`~CompressedStream`, and the counts match exactly — so this is not a
registered-but-uncalled port, it is a live one on the real teardown path, and
the live blocker is genuinely closed rather than merely filled in.

(Reading the columns: `calls` counts replay-diffed entries, `worked` counts
`RT_DIFF_WORK`. A body with no inline replay reports `calls=0 worked=N`, which
is what all six MemoryStream bodies do too. `worked=0` would have been the
alarming reading, not `calls=0`.)

## 59. What these two are NOT verified by, stated plainly

**There is no differential oracle on either.** They ran six times without
crashing, the allocator raised nothing, and the process shut down clean — but
nothing compared them against the shipped bodies.

The reason is structural, not laziness: both **free memory**. Replaying the
shipped body over the same object double-frees it, and there is no snapshot that
makes that safe. This is the same wall the copy ctor and `operator=` hit in rung
4a, where the answer was a constructed probe — and here that probe needs two
independently built `CompressedStream`s, which needs `SetWindowCount`, which is
not ported yet.

**So `Release`'s oracle becomes possible in the same rung that ports
`SetWindowCount`, and I will build it there rather than declare these two done.**
Recorded as executed-but-undiffed, which is a weaker claim than every other port
this lane has landed and should not be counted with them.

## 60. A third confirmation of §48, for free

`~CompressedStream` stamps vtable **`0x180155640`**, not the `0x180155560` that
`ReadPackfile` stamps onto its stack object. Had those been one object, one of
the two stamps would have to be wrong. `0x180155560` is
`CompressedMemoryStream`'s table and `0x180155640` is `CompressedStream`'s —
independent of both the destructor-size argument and the frame arithmetic.

`Release`'s field accesses are likewise an independent check on `Stream.h`'s
`CompressedStream` layout: it walks `+0x50/+0x58`, `+0x30/+0x38` (stride `0x18`,
buffer at `+0x10`), `+0x40/+0x48`, `+0x60`, and resets `+0x1c`, `+0x20`, `+0x28`.
Every one agrees with a header that was pinned from the **constructor** — two
different functions, same answer.

## 61. `Commit` is NOT a two-body job — it opens onto `CompressWindow`

Prioritising `Commit` and `~CompressedStream` as the live pair was right, but
they are not symmetrical. `~CompressedStream` closed. **`Commit` did not, and
the reason is a dependency I had not surfaced:**

`?Commit@CompressedStream@IO@@` (`0x1800a9ee0`) calls
**`?CompressWindow@CompressedStream@IO@@` (`0x1800a9810`) at `0x1800a9f11`** —
which is unported, and is **this lane's own rung-2 named blocker**, declined
there as write-path-only.

So porting `Commit` today would move the null one frame deeper, not remove it:
a ported `Commit` calling an unported `CompressWindow` is exactly
`reference_converting_a_door_is_not_satisfying_it`. `Commit` is a **three**-body
job — `Commit` + `CompressWindow` + `Commit`'s own oracle — and `CompressWindow`
is a compressor, not a mover of fields.

I have not started it, and I would rather flag that than land a `Commit` that
looks closed. It is also **not on the `ReadPackfile` read path**: `ReadPackfile`
never calls `Commit`, so this does not block 4c.

## 62. Remaining, and the next fault site

Left in 4b-iii: `SetWindowCount` (52, decoded), `Open` (~50, decoded), and the
`Release` oracle that becomes possible with them. Deferred with a stated reason:
`Commit` + `CompressWindow`, which are write-path and do not block 4c.

**Registered-but-uncalled is still four** (`0x180029510`, `0x180029630`,
`0x180024d70`, `0x180024c10`) — the two `J1_ENTRY` dtors added here are NOT
added to that list, because `~CompressedStream` ran six times and calls all
three of its array dtors unconditionally, so they executed.

Next predicted fault site unchanged: `LoadPackages` stops at the `ReadPackfile`
call.

---

# Rung 4b-iii (part 2) — 4b CLOSES: `SetWindowCount`, `Open`, and the oracle that was promised

## 63. Ported

`runtime/src/io/compressed_stream_open.cpp`, switch `BANISHED_RT_CSOPEN`.

| symbol | addr |
|---|---|
| `?SetWindowCount@CompressedStream@IO@@QEAAXH@Z` | `0x1800aa020` |
| `?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z` | `0x1800aa560` |

**A phantom name was caught before it shipped.** I wrote
`?SetWindowCount@CompressedStream@IO@@**AEAA**XH@Z` — the PRIVATE spelling, from
the assumption that a helper like this would be private. The export table says
**`QEAA`**, public. A name the DLL does not contain resolves NULL silently and
forever behind the `ResolveAddr` fallback, and the port would have registered,
been counted, and never run. Caught because the names are checked *before* the
line is believed, not after.

### Transcriptions worth naming

**`Open`'s two bound tests have different signedness, and both are load-bearing:**

* `CMP RBP,0x4; JNC` (`1800aa595`) is **unsigned** — a negative `GetSize`,
  sign-extended, becomes huge and *passes*.
* `LEA EAX,[RDX*0x8]; CMP EAX,EBP; JG` (`1800aa5cb`) is **signed and 32-bit** —
  a `blockCount` big enough to overflow `blockCount * 8` wraps negative and also
  *passes*.

Both are spelled as retail spells them. "Fixing" either would reject inputs
retail accepts, in the one function whose entire job is deciding whether a
packfile header is well-formed.

**`SetWindowCount` re-reads `windowData` from the object twice per iteration**,
and reloads the buffer pointer from the window it just stored it into, rather
than reusing registers. Reproduced; observationally identical here, but caching
it would change what the code does.

Its `0x8000` and `0x18` agree with `Stream.h` — a genuine independent check,
since that header was pinned from the **constructor** and this is a different
function.

## 64. The oracle arrived with the port, exactly as promised

`compressed_stream_open_probe.cpp`. Two independently built `CompressedStream`s,
driven in lockstep through three phases with state compared before the next runs:
`SetWindowCount` (window array, header, `activeWindow`), `Open` (return,
`headerSize`, source cursor, directory bytes), then **`Release`** — the phase
that could not exist in part 1.

```
[CSOpen] lockstep probe: windows=3 opens=4 (accepted=1 rejected=3) releases=6
         -- identical on the window array, the block directory and all nine
            fields Release resets
[RtDiff] ==== 333 sites, 1191480 calls, 1 divergences in 1 sites ====
```

The single divergence is the pre-existing `Object::Factory::Startup`.
`port_bypass_audit` 0 on both doors; `export_name_audit` 1 phantom, the
pre-existing `?_singleton@`.

**`Release` and `~CompressedStream` are no longer executed-but-undiffed.** That
category was opened in part 1 with a stated route out, and this is that route
arriving. All nine fields `Release` resets are now compared against the shipped
body, including `activeWindow` returning to **-1** rather than 0.

The `+0x0c..+0x0f` exclusion is applied, and the doc says **why it is not a
tolerance**: it is derived from the tail init at `0x180024c10`, which writes
`+0x00`, an unaligned qword at `+0x04..+0x0b`, and `+0x10`, and never those four
bytes. If retail ever wrote them the exclusion would be a defect. A tolerance
that began life as a failing comparison is indistinguishable later from one that
was reasoned, and only one of the two is sound.

## 65. The probe was wrong first, and the way it was wrong is worth keeping

The first run reported `DIVERGED on 'header directory bytes'`. **It was the
probe, not the port.** Case 1 declared 64 blocks — a 512-byte directory — in a
512-byte image buffer, so `Open` read the last entry off the end of the array.
Two separate stack images have different neighbours, so the comparison failed on
bytes **neither side owned**.

That is `reference_oracle_must_not_compare_unwritten_bytes` in a form I had not
seen: not comparing bytes nobody *wrote*, but comparing bytes read from past the
end of the input. And it was camouflaged — it surfaced as `rejected=1`, which
reads like a reject path being properly exercised.

The rebuilt corpus sizes the image to the largest directory any case declares,
and makes each rejection genuine and non-overrunning:

| case | shape | rejected by |
|---|---|---|
| 0 | well-formed | — (accepted) |
| 1 | `blockCount*8 > len`, image small | the **second** test, before any read |
| 2 | `len < 4` | the **first** test |
| 3 | valid shape, one size field corrupted | the **sum loop** — the only case that exercises per-entry accumulation on a reject |

`accepted=1 rejected=3` with all four paths distinct.

## 66. 4b is closed. What is left, and where the ladder stands

**Closed in 4b**: `MemoryStream`'s six virtuals; `Array` instantiations
`0x180029510`, `0x180029630`, `0x180024d70`, `0x180024c10` and dtors
`0x1800249c0`, `0x1800248e0`; `Release`; `~CompressedStream`; `SetWindowCount`;
`Open`. Plus the two risks (`SetFilePointer`, slot 7) resolved as non-blockers.

**Deferred with a stated reason**: `Commit` + `CompressWindow` — write-path, not
on `ReadPackfile`'s read path, and porting `Commit` alone would only move the
null a frame deeper.

**Registered-but-uncalled is still four** (`0x180029510`, `0x180029630`,
`0x180024d70`, `0x180024c10`). Note `0x180024d70` and `0x180024c10` were
*exercised by the probe* this run — but through our own `Open`, not from
`ReadPackfile`, so I am NOT closing them: the loop closes when the real caller
reaches them, and I will report any that stay unreached as a corpus finding.

**Next is 4c — `ReadPackfile` itself**, and every callee it has is now ported.
Predicted fault site is unchanged right up until it lands: `LoadPackages` stops
at the `ReadPackfile` call. I will take the three-way split if the transcription
wants it and say so rather than force one commit.

---

# Rung 4c (part 1) — `ReadPackfile` decoded, and it is smaller than 1,864 instructions

## 67. Why this is a decode commit and not the port

`ReadPackfile` is all-or-nothing: it cannot be half-registered, so unlike every
previous rung there is no way to land part of it and measure. That makes the
transcription the single riskiest edit this lane will make, and it makes the
*decode* worth committing on its own — it is what de-risks the transcription,
and it contains findings that stand whether or not the port follows immediately.

I am taking the split the integrator authorised, and saying where: **4c-i is the
structure, 4c-ii is the transcription.** What follows is a complete control-flow
decode with every callee resolved.

## 68. The 25 inlined `Read`s collapse to 25 CALLS — this is the finding

The repeated `SetWindow` + `memcpy` pair I identified in §34 is confirmed as the
inlined body of `?Read@CompressedStream@IO@@`, and the inlined loop is exactly
the one rung 2 ported:

```
loop:
  if (!SetWindow(cs, position)) break;            // 0x1800a9aa0 — rung 2
  Window* w   = &cs->windowData[cs->activeWindow];
  int take    = count;
  if (cs->position + count > w->end) take = w->end - cs->position;
  memcpy(dst, w->buffer + (cs->position - w->start), take);
  dst += take;  cs->position += take;  count -= take;
  if (count > 0) goto loop;
```

**MSVC inlined it; the port does not have to.** Each of the ~25 sites becomes one
call to `RtIo_CompressedStream_Read`. That is not a paraphrase — it is the same
body, reached by a call instead of a copy — and it removes ~1,200 of the 1,864
instructions from the transcription along with 25 chances to mistype an offset.

**Two things make that substitution safe, and both were checked:**

* The inlined copies **discard the return value**. On `SetWindow` failure retail
  breaks out and leaves the rest of the destination unwritten, then carries on;
  ignoring `Read`'s `bool` reproduces exactly that.
* The stack offsets prove the object identity: `[RSP+0x5c]`, `[RSP+0x60]` and
  `[RSP+0x70]` against a `CompressedStream` at `[RSP+0x40]` are `+0x1c`
  `position`, `+0x20` `activeWindow` and `+0x30` `windowData`. A **sixth**
  independent agreement with `Stream.h`.

## 69. A fifth confirmation of `FileInfo+0x0C`, from a seek

At `0x1800b47b1` the header word is read, its top bit is extracted and cleared
(`BT`/`BTR EDX,0x1f`), and the remainder is stored to **`[RDI+0x0c]`** — then,
**only when the handle is `-1` or `-2`** (`CMP [RDI],-0x3; JBE` skips otherwise,
i.e. only the loose-file case), `SetFilePointer(winHandle, thatSameValue, NULL,
FILE_BEGIN)`.

So retail **seeks by writing the field directly** and mirrors it into the OS
handle only when there is one. That field can only be the read cursor, and it is
at `+0x0C`. Lane FS's `uint64_t size` at `+0x08` would have swallowed it.

Also confirmed here: `this->_fileInfo` is at **`PackageFile+0xC0`**, the same
offset lane PKG's `ClosePackages` hands to `CloseFile`.

## 70. `Commit` is PROVABLY dead on the read path

§61 argued from the call graph that `Commit` is write-path-only. The guard says
so outright:

```
1800b6023: CMP dword ptr [RBP + -0x78],0
1800b6027: JLE skip
1800b6029: CMP dword ptr [RSP + 0x48],1      ; cs->direction (+0x08)
1800b602e: JNZ skip
1800b6030: LEA RCX,[RSP + 0x40]
1800b6035: CALL 0x1800a9ee0                  ; Commit
```

`direction == 1` is the write direction, and `?Open@CompressedStream@IO@@` — the
body this lane ported in 4b-iii — **sets `direction = 0`**. On the read path this
call is unreachable. The deferral of `Commit` + `CompressWindow` is now proved
rather than argued.

## 71. The rest of the structure

```
save fs->[0x19]; fs->[0x19] = 1              // a re-entrancy flag, restored at 1800b4792
FileInfo tmp = OpenFile(fs, filename);        // rung 3
this->_fileInfo(+0xC0) = tmp;                 // rung 4a — FileInfo::operator=
release tmp's two Filenames
restore fs->[0x19]
if (this->_fileInfo.handle == -1) return false;

ReadFile(fs, &_fileInfo, 4, &hdr);            // rung 3
compressed = (hdr >> 31) & 1;  hdr &= 0x7fffffff;
_fileInfo.cursor = hdr;                       // the seek, §69
if (handle is -1 or -2) SetFilePointer(winHandle, hdr, NULL, FILE_BEGIN);

ReadFile(fs, &_fileInfo, 4, &blobLen);
blob = MemAlloc(blobLen, 4);                  // lane E
ReadFile(fs, &_fileInfo, blobLen, blob);

CompressedMemoryStream cms;                   // ONE object, §48
  CompressedStream::CompressedStream(&cms);   // lane W
  cms.vptr = 0x180155560;
  MemoryStream::MemoryStream(&cms.memoryStream);   // lane W
  cms.memoryStream = { data: blob, size: blobLen, cursor: 0 };
  CompressedStream::Open(&cms, &cms.memoryStream, 1);   // 4b-iii

Read(&cms, 4, &version);                      // rung 2
if (compressed) { ~20 more Reads: the metadata block }   // SKIPPED when the
                                                          // header sign bit is 0
  ... two Array::SetSize (0x180029510 int32, 0x180029630 bool)  // 4b-i
  ... five {Read len; Read bytes; decode; SetLength; memcpy; MemFree} groups
      for name / author / description / two more strings
Array<int32>::SetSize (0x180029510)           // 4b-i
entry loop: Read fields; build a Filename; AdjustDelimiters x3;
            PackageFile entry insert (0x1800298f0)
if (dirCount > 0 && cs->direction == 1) Commit(&cms);   // DEAD, §70
cms.memoryStream.Close();                     // slot 7, a bare RET — 4b-ii
MemFree(blob);
~CompressedMemoryStream(&cms);                // lane W
return true;
```

**Every callee is ported.** The one body still unported anywhere in this listing
is `0x1800298f0`, the entry insert (114 instructions) — which §34 listed and
which I had not re-checked since. It is 4c-ii's only new port.

## 72. A branch the 4c corpus must cover

The metadata block is **conditional on the header's sign bit** (`CMP R15D,0x1;
JL 0x1800b5b25` at `0x1800b48f0`). An uncompressed packfile skips ~1,000
instructions of it entirely. So a 4c oracle that only ever sees the shipped
`.pkg` files may exercise one side of that branch and report green — the same
effectiveness trap this lane has now hit three times. The corpus must establish
which way the retail packfiles go and, if they are all one way, say so rather
than imply coverage.

## 73. Registered-but-uncalled: the four are all reachable from here

All four appear in §71's listing — `0x180029510` twice, `0x180029630` once,
`0x180024d70` and `0x180024c10` via `Open`. So the loop should close when 4c-ii
lands. If any is still unreached, §72's branch is the first thing to suspect:
`0x180029630` in particular sits inside the conditional metadata block.

## 74. Next predicted fault site

Unchanged until 4c-ii lands: `LoadPackages` stops at the `ReadPackfile` call.
After it, the first thing that can fail is `0x1800298f0`, the entry insert — the
only unported body left in the listing.

---

# Rung 4c-ii (part 2) — the complete destination map, and retail over-reads its own stack on every packfile string

## 75. Every `Read` destination, resolved

`R13` is `this` (the `PackageFile`). Each inlined `Read` is preceded by
`MOV ESI,<count>` and `LEA R14,<dest>`, so the sequence reduces to a table —
the transcription's working sheet:

| site | count | destination |
|---|---|---|
| `1800b486e` | 4 | `[RBP-0x10]` — the version word |
| `1800b48ff` | 4 | `[RBP+0x18b0]` — first field of the conditional block |
| `1800b497e` .. `1800b4cc0` | 4 each | `this+0x68, +0x6c, +0x70, +0x74, +0xac, +0xa8, +0x78, +0x7c` |
| `1800b4d30` | — | `Array<int32>::SetSize(this+0x80, n)` then a **real `CALL Read`** of `n*4` |
| `1800b4d71` | 4 | `this+0x90` |
| `1800b4dea` | — | `Array<bool>::SetSize(this+0x98, n)` then a **real `CALL Read`** |
| `1800b4e2e` | 4 | `[RBP+0x18b8]` — a string LENGTH |
| `1800b4eb3` | len | `[RBP+0x860]` — the string BYTES |

then four more length/bytes pairs in the same shape, and the entry loop.

The two `SetSize` sites use a **real `CALL 0x1800aa780`** rather than an inlined
copy — retail inlined `Read` 25 times and left 2 as calls to the same body. A
small extra confirmation that substituting a call for the inlined form (§68) is
legitimate rather than convenient.

## 76. `len` bytes are read and `2 * len` are decoded — and that is retail's, not a misreading

Every string group does this:

```
Read(&cms, 4, &len);
Read(&cms, len, buf);            // len BYTES into buf at [RBP+0x860]
end = buf + 2*len;               // LEA EAX,[R15+R15*1] — TWICE what was read
```

I stopped on this rather than guess, and then resolved it by reading the
decoder. `0x1800c9fc0` is a five-way jump table on its 5th argument; entry **1**
is nine instructions:

```
MOVZX EAX,[RDX]  ;  MOVZX ECX,[RDX+1]  ;  SHL ECX,8  ;  OR ECX,EAX
MOV EAX,2        ;  MOV [R9],ECX       ;  RET
```

**UTF-16LE, two bytes per codepoint, `end` is accepted and completely ignored,
returns 2.** So the loop `while (src < end) src += decode(...)` runs exactly
`len` iterations and consumes `2 * len` bytes — **`len` bytes more than were
ever read into the buffer.**

**This is a real over-read in the shipped DLL**, and the port reproduces it. The
project's standing rule applies exactly: retail is not wrong to be fixed, it is
behaviour to be reproduced. Reading `2*len` instead, or clamping `end` to
`buf + len`, would both be "corrections" that change what the function returns.

What stops it being catastrophic is downstream, and is worth knowing:

* the re-encode loop **caps the output at `0x800` bytes** (`CMP ESI,0x800; JG`),
* the result is **NUL-terminated and then `strlen`-ed** (`1800b4fce`..`1800b4feb`)
  before `String::SetLength`,

so the decoded string ends at the first zero codepoint. The real characters come
first; the excess is stale stack that almost always begins with a zero. "Almost
always" is doing real work in that sentence — it is retail's own fragility, now
documented rather than inherited silently.

### The consequence for 4c's oracle, which is the important part

Ours and theirs will decode from **two different stack buffers with different
stale contents.** Any packfile string whose real data does not terminate within
the `len` bytes actually read can therefore differ between the two sides
**legitimately** — retail's own output is stack-dependent there.

So the 4c oracle must compare the decoded strings **only up to the terminator**,
and must say that the region past it is excluded because retail reads
uninitialised memory there. That is the same class of exclusion as rung 3's
`winHandle` and 4b-iii's `+0x0c..+0x0f`, and had I not read the decoder I would
have compared it, seen an intermittent mismatch, and had every reason to blame
the port.

## 77. Everything else about the transcription is ready

* Every callee is ported (4c-ii part 1 closed the last one).
* The `Read` collapse is proved legitimate two ways (§68 and §75).
* Every destination is mapped (§75).
* The string groups' full shape is now understood end to end (§76).
* `Commit` is proved dead (§70).
* The conditional metadata branch is identified as the false-green risk (§72).

What remains is writing it: the five string groups and the entry-directory loop,
against the map above.

## 78. Registered-but-uncalled: FIVE

`0x180029510`, `0x180029630`, `0x180024d70`, `0x180024c10`, `0x1800298f0`.
All reachable only from `ReadPackfile` or `Open`; all close, or become a corpus
finding, in the run where the transcription first executes. `0x180029630`
remains the first suspect if the conditional block is one-sided — §75 places it
inside that block.

## 79. Next predicted fault site

Unchanged: `LoadPackages` stops at the `ReadPackfile` call, and will until the
transcription lands.

---

# Rung 4c-ii (part 3) — there are TWO string paths, not one, and a second false-green branch

## 80. `this+0x40 / +0x48 / +0x50` are written TWICE, by two different code paths

While mapping the string groups I found six of them, and the first three and the
last three write **the same three fields**:

| group | temp buffers | target |
|---|---|---|
| 1 | `[RBP+0x18b8]` len, `[RBP+0x860]` bytes | `this+0x40` |
| 2 | same | `this+0x48` |
| 3 | same | `this+0x50` |
| 4 | `[RBP+0x18a0]` len, `[RBP+0x60]` bytes | `this+0x40` |
| 5 | same | `this+0x48` |
| 6 | same | `this+0x50` |

Six groups, **three strings.** Groups 1-3 end with `JMP 0x1800b5b6c`
(`1800b558e`/`1800b559a`), jumping clean over groups 4-6, whose entry point is
`0x1800b559f`. It is an if/else, not a sequence — and had I transcribed it as a
sequence, groups 1-3 would have been dead stores overwritten by 4-6.

## 81. The selector is a FORMAT VERSION, and it is the second branch that can fake a green

```
1800b48ff  Read(&cms, 4, &version);        // into [RBP+0x18b0]
...
1800b4e0e  MOV R15D,[RBP+0x18b0]
1800b4e15  CMP R15D,0x2
1800b4e19  JG  0x1800b559f                 // version >  2  -> PATH B
                                           // version <= 2  -> PATH A
```

* **PATH A (version <= 2)** — read `len`, read `len` bytes, **decode UTF-16LE and
  re-encode to UTF-8** through the `[RBP+0x860]` buffer. This is the path with the
  over-read documented in §76.
* **PATH B (version > 2)** — read `len`, read the bytes **straight into
  `[RBP+0x60]`**, no transcode at all.

So `ReadPackfile` has **two** conditionals that a corpus of shipped `.pkg` files
probably cannot cover on both sides:

1. the compressed flag (§72), which gates the whole metadata block, and
2. **this version check**, which gates *which* of two string implementations runs.

The version one is the more likely to be one-sided: every shipped `.pkg` was
produced by the same `Tools-x64.exe`, so they will almost certainly all carry the
same format version and all take the same arm. **A green 4c run will therefore
probably exercise one string path and not the other, and must say so rather than
imply it covered both.**

That is the integrator's standing item answered before the fact: the coverage gap
is now identified, and the honest report after the transcription will name which
arm the real files take and state the other as untested — rather than a green run
being read as covering a path it never entered.

## 82. Why this mattered before writing rather than after

Nothing here required a run to discover, and none of it would have shown up as a
failure. Transcribed as a sequence, groups 1-3 become dead stores and the port
still *works* on every version-3+ packfile — which is to say, on all of them. It
would have been correct on all available data and wrong in the source, and the
only symptom would have appeared on an old packfile nobody has.

That is the same shape as the `AdjustDelimiters` near-miss (§74) and the
alignment near-miss (§51): a difference that is invisible to every test the
corpus can run.

## 83. State of the transcription

Now fully mapped:

* every callee ported;
* the `Read` collapse proved legitimate twice;
* every `Read` destination resolved (§75);
* the UTF-16 path understood end to end, including retail's stack over-read and
  what it means for the oracle (§76);
* **both string paths and their selector identified (§80-81)**;
* `Commit` proved dead (§70).

Remaining to read: the entry-directory loop (`0x1800b5b6c`..`0x1800b601d`).
Then it is writing, with both arms transcribed.

## 84. Registered-but-uncalled: still FIVE, and §81 sharpens the prediction

`0x180029510`, `0x180029630`, `0x180024d70`, `0x180024c10`, `0x1800298f0`.
`0x180029630` (`Array<bool>::SetSize`, `this+0x98`) sits **before** the version
branch but **inside** the compressed-flag block, so it is reached iff the
metadata block runs at all. If it comes back unreached, the compressed flag — not
the version — is the one that was one-sided.

## 85. Next predicted fault site

Unchanged: `LoadPackages` stops at the `ReadPackfile` call.
