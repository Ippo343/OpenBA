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
