# WAVE 27 LANE IOTEXT — the IO text/package streams, plus `UI::ElementDescription`

Branch `wave27/iotext`, cut from `wave27/base` at `c6f30c24`.

## 0. HEADLINE

**Two of the three tables in scope are now COMPLETE, and four dtor/accessor rows
are ported.** Six bodies written, one instrument crash found by running rather
than reasoning, one stale "already ported" claim in the tree corrected, and every
address and vtable offset this lane carried re-derived through **two mechanisms
that share nothing with Ghidra** after the integrator's two mid-lane warnings.

| Table | before | after | |
|---|---|---|---|
| `??_7ElementDescription@UI@@6B@` | 3 / 6 | **6 / 6** | COMPLETE |
| `??_7PackageFile@IO@@6B@` | 6 / 9 | **9 / 9** | COMPLETE |
| `??_7TextFileStream@IO@@6B@` | 8 / 10 | 8 / 10 | unchanged — see §6 |

`COMPLETE` is the **static** half of readiness only: it says promoting the table
would put no trap in it. It does **not** say the row is promotable — that turns
on owning the function that STAMPS the table, which no source scan can answer
(`reference_a_green_readiness_screen_is_not_a_route.md`). **This lane promoted
nothing and claims nothing about promotion.**

## 1. MEASURED

Every figure in this section was produced by running something. The command and
the tip are named so a reader can re-derive rather than trust.

### 1.1 The board (`tools/runtime_census.py`, run by this lane)

| tip | TOTAL | gap | decomposition |
|---|---|---|---|
| `wave27/base` `c6f30c24` | 1372 called / 1237 ported / 90.2% | 135 | 18 BODY + 2 DTOR + 42 VTABLE + 46 DATA + 27 addr |
| after ElementDescription + PackageFile 6,7 | 1375 / 1239 / 90.1% | 136 | …+1 DATA |
| after `PackageFile::Serialize` | 1377 / 1240 / 90.1% | 137 | …+1 addr |
| **final, this branch** | **1377 / 1242 / 90.2%** | **135** | **18 BODY + 0 DTOR + 42 VTABLE + 47 DATA + 28 addr** |

Per namespace at the end: **IO 234 called / 222 ported / 94.9%** (from 231/217),
**UI 213 / 206 / 96.7%** (from 212/206).

**THE ONE LINE WORTH QUOTING IS NOT THE PERCENTAGE, IT IS THE `DTOR` COLUMN: 2 →
0.** The wave opened with exactly two DTOR gap rows — `??1TextFileStream@IO@@` and
`??1CompressedFileStream@IO@@` — and both were in this lane's scope. Both are
ported. That column is now empty, and it is empty for a reason a percentage cannot
express: those two addresses were the ones `deleting_dtor.cpp` reaches by address,
i.e. the ones that would have made every `delete` of either stream fault at
cutover. The gap total is back to 135 only because this lane also *added* two rows
(§1.1 above) — same number, different composition, and the composition is the
result.

**THE PERCENTAGE WENT DOWN AND THAT IS THE CORRECT BEHAVIOUR.** `called` sits in
both halves of the ratio, so a lane that opens a door moves it
(`reference_the_ceiling_percentage_is_a_moving_target.md`). Two rows entered
`called` unported *because of this lane's work*, and both were declared in
advance at the call site, not discovered afterwards:

* `?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A` — a **DATA** row.
  It is already minted (`rt_data_mint_tables.inc:1989`) but not promoted, so the
  two new accessors resolve it and the census counts it as an unsatisfied call.
  There is no body to write.
* `0x180029bb0` — an **unexported ~680-byte array-of-String serializer**, the one
  genuinely new obligation this lane creates. `PackageFile::Serialize` is its
  first caller in the tree. Pattern:
  `reference_porting_can_grow_the_cutover_surface.md`. See §7 for the addressee.

Final numbers to be re-derived by the integrator at merge; do not quote the
table above as a post-merge board.

### 1.2 Oracles that RAN, with their verdicts

Run: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_RT_SAVE_TRACE=1`,
game-dir lock held by IOTEXT throughout, deployed exe md5-verified as this
lane's build by `tools/game_dir_lock.sh assert-held` before every run.

| subject | oracle | verdict |
|---|---|---|
| `?Destruct@PackageFile@IO@@` | ours vs `ResolveExportRaw`, two freshly-constructed scratch blocks, memcmp 240 B | `[PKG2] Destruct oracle: MATCH (240 bytes compared)` |
| `?Reconstruct@PackageFile@IO@@` | same, plus the return value | `MATCH (240 bytes compared), return-is-this ours=1 theirs=1` |
| `?Serialize@PackageFile@IO@@` | `RtSaveTrace::Compare`, writing direction, full replay, real mounted packages | `[RtSaveTrace] pass IO::PackageFile::Serialize: 21 emissions identical` ×4 |
| the new `UI::ElementDescription` deleting-dtor row | `tools/rt_deleting_dtor_verify.py`, which re-derives size and `~Class` from the retail PE image | `OK 0x180035350 size=0x68 dtor=0x180116100 UI::ElementDescription`; 45 rows, **problems: 0** |

The `RtSaveTrace` line is guarded by `PortIsLive`, which fails loudly if the
registry does not hand our body back for the name — so a green run cannot be the
vacuous `ours == theirs` pass (`reference_uncalled_port_is_not_verified.md`).

The memcmp oracles compare the WHOLE 240-byte object, which is only sound
because both blocks are `memset` to the same pattern first. The constructor does
**not** write all 240 bytes: the same probe measures
`ours wrote 213/240, theirs wrote 213/240 (the 27 untouched are
+0x78,+0x7c,+0x90,+0xad..af,+0xc4,+0xd4)`. Without the memset this would have
compared 27 bytes neither side wrote and failed on stack garbage
(`reference_oracle_must_not_compare_unwritten_bytes.md`). The first draft of the
comment named only `+0x78/+0x7C` from the header's prose; it now quotes the
measured set.

### 1.3 The build

`mingw32-make standalone-debug` exits 0 at every commit. Final boot: `RUNNING`
per `tools/boot_state.sh`, zero-byte `standalone_crash.log`.

## 2. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Warning #1 binds: recorded, not repaired.

### 2.1 `runtime/src/io/file_stream_probe.cpp` — **ALREADY FIXED, re-confirmed**

The brief and `WAVE26_CLOSED.md` §2 item 6 say two of three vptr checks here are
structurally false (subject built by retail's ctor, compared against
`RT_DATA_SLOT`, which is registry-first). **That is no longer true at
`wave27/base`.** All three now go through `RtResolveExportForProbe`
(→ `ResolveExportRaw`), at `file_stream_probe.cpp:389`, `:390` and `:492`, with
the full reasoning in the comment at `:370-388`. `git log` attributes the fix to
`d668f883` "Wave 26 lane PROMOIO" — the same wave that recorded the defect
committed the fix after recording it. **Nothing to do; I did not touch the file.**

This matters as a process fact, not just a code fact: I re-checked the stated
blocker before acting on it, and it had expired
(`reference_holdback_blockers_are_stale_check_them_first.md`). A lane that had
trusted the brief would have "fixed" a file that was already correct.

### 2.2 `tools/rt_deleting_dtor_verify.py`

The integrator reports its scalar-branch check knows only two shapes with a bare
`else`, so it prints BAD for correct rows (inflating; 2 false positives, 0 false
negatives, found by GFXMESH on Graphics rows). **It printed OK for my row, so the
defect did not bite here** — and an inflating defect cannot manufacture a false
OK. Not fixed. Not needed.

### 2.3 GhidraMCP, two defects, both routed around — see §3

## 3. THE GHIDRA DEFECTS, AND THE TWO NON-GHIDRA WITNESSES I BUILT

The integrator warned mid-lane of (a) a session-scoped program-selection defect
serving the profile DLL while `list_open_programs` claims retail, and (b)
`disassemble_function` relabelling one body's listing with another body's
address. My namespace is the one place the two builds are **measured** to
disagree (IO Encoding / ReaderWriter vtable offsets, retail `+0x78/98/a0` vs
profile `+0x90/b0/b8`), so a contaminated read here produces a body that
compiles, registers, dispatches and calls the wrong function.

**Every Ghidra call in this lane passed `program="Runtime-x64.dll"` explicitly
from the first call onward** — before the warning arrived, because
`list_open_programs` had shown `Runtime-x64-profile.dll` as current and I did not
want to rely on `switch_program`.

**Discriminator, run both ways:** `disassemble_function(0x180002190)` returns the
retail two-instruction form (`MOV RAX,[0x1801fae10] ; RET`) with `program=`
explicit **and** with it omitted. No re-derivation debt.

That is one mechanism, and one mechanism is one witness. Two more were built:

**Witness 2 — `pefile` on the retail file on disk** (`C:\GOG Games\Banished\
Runtime-x64.dll`), reading the export directory and the `.rdata` vtables
directly. Every export VA I carried and every slot of all three tables agreed
**exactly**:

```
??_7PackageFile@IO@@6B@        0x180155700  [0..8] = 1800259d0 180025150 180025130
                                                    1800b6080 1800b6090 1800b60a0
                                                    1800252d0 180025300 1800252f0
??_7ElementDescription@UI@@6B@ 0x1801561c0  [0..8] = 180035350 1800350f0 1800350d0
                                                    180116af0 1800066e0 180116b00
                                                    18007ec28 ×3 (purecall)
??_7TextFileStream@IO@@6B@     0x180155980  [0..9] = 18002ba90 1800aab50 1800ba9f0
                                                    1800ba900 1800babd0 1800bac30
                                                    1800ba8e0 1800aac70 1800ba640
                                                    1800ba940
```

and the two ElementDescription accessors' raw bytes decode to the BaseTypeInfo
this lane claims: `48 8b 05 09 5a 1c 00 c3` at `0x1800350f0` →
`0x1800350f7 + 0x1c5a09 = 0x1801FAB00` = `_typeInfo + 8` (the `name`);
`8b 05 22 5a 1c 00 c3` at `0x1800350d0` → `0x1801FAAF8` = `_typeInfo + 0` (the
`typeId`). `?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A` is
exported at exactly `0x1801faaf8`.

The PE headers also settle the section question the integrator raised about
`list_segments`: `.text 0x180001000..0x18014a600`, `.rdata
0x18014b000..0x1801f5c00`. All three vtables and `0x180154630` are in `.rdata`.
I never called `list_segments`.

**Witness 3 — `objdump` by true VA on the same retail file.** This is the one
that matters most, because it is the check the integrator asked for by name:

```
1800b60de: ff 50 18          call *0x18(%rax)   Encoding::CreateReader
1800b60e3: ff 50 20          call *0x20(%rax)   Encoding::CreateWriter
1800b6139: ff 90 a0 00 00 00 call *0xa0(%rax)   rw kScalarInt   <- RETAIL, not +0xb8
1800b61e4: ff 50 78          call *0x78(%rax)   rw kScalarBool  <- RETAIL, not +0x90
1800b6259: ff 50 40          call *0x40(%rax)   rw kArrayUInt
```

**At the raw-byte level, every ReaderWriter slot this lane carries is the RETAIL
offset.** That converts "I hope I read the right binary" into a measurement, and
it is the one thing that could have silently ruined this lane. `objdump` also
reproduced `~TextFileStream` (0x1800ba490), `~CompressedFileStream`
(0x1800a9330), `?Reconstruct@` and `?Destruct@` instruction for instruction
against my transcriptions.

Three mechanisms, agreeing. **INFERRED, and said so:** I did not re-take the
whole of every body under all three — I took the *constants* (VAs, slot offsets,
sizes) under all three and the *control flow* under two.

## 4. WHAT WAS PORTED

### 4.1 `??_7ElementDescription@UI@@6B@` 3/6 → 6/6

| slot | symbol | where | verified how |
|---|---|---|---|
| 0 `+0x00` | `??_EElementDescription@UI@@` `0x180035350` | `runtime/src/object/deleting_dtor.cpp` spec 44 + `RegisterRuntimeAddr` | `tools/rt_deleting_dtor_verify.py` OK against the retail PE; `size=0x68` agrees with the `MOV EDX,0x68` in the thunk, with the ctor's last store at `+0x60`, and with `ElementDescription.h`'s `static_assert` — three independent pins |
| 1 `+0x08` | `?TypeName@ElementDescription@UI@@UEBAPEBDXZ` `0x1800350f0` | `runtime/src/ui/dialog.cpp`, name **and** address | by construction from the decoded RIP-relative operand (§3); no oracle — see below |
| 2 `+0x10` | `?Type@ElementDescription@UI@@UEBAIXZ` `0x1800350d0` | same | same |

**Slots 1 and 2 have NO ORACLE, and the reason is not "no route" — it is that an
oracle would be VACUOUS.** `?_typeInfo@ElementDescription@UI@@` is minted but
**not** promoted (`reg_mint_promoted.cpp` carries no row for it), so with the DLL
present `RT_DATA_SLOT` falls through to the DLL's own static. Our side and
retail's side would read the same object through the same door and a comparison
would be retail against itself
(`reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md`).
Said at the call site, not hidden. What *is* checked is the mint shadow's
field-by-field comparison of every minted BaseTypeInfo against the loaded DLL's
nodes, and that check becomes load-bearing the day this row is promoted.

**A STALE CLAIM IN THE TREE, CORRECTED IN PLACE.** `dialog.cpp:1471` had said
since Wave 9 that `0x180035350` was "one of the 37 Wave 9 lane K ported". **It
was not.** At `wave27/base`, grepping `0x180035350` across `runtime/`,
`standalone/` and `code/` found it in exactly two places: that comment, and
`rt_data_mint_tables.inc:1016` — which is the MINT, i.e. the *trap*. Never in a
registration. The sentence read as a completion
(`reference_a_prescription_read_as_a_completion.md`) and cost the table three
waves at 3/6. The paragraph now carries a ⚠ CORRECTION naming what was false and
where the row actually lives, on the surface the next reader reaches
(warning #9).

### 4.2 `??_7PackageFile@IO@@6B@` 6/9 → 9/9

| slot | symbol | file | verified how |
|---|---|---|---|
| 5 `+0x28` | `?Serialize@PackageFile@IO@@` `0x1800b60a0` | **new** `runtime/src/io/package_file_serialize.cpp` | `RtSaveTrace::Compare`, 21 emissions identical, ×4 real packages |
| 6 `+0x30` | `?Reconstruct@PackageFile@IO@@` `0x1800252d0` | `package_file.cpp` | 240-byte memcmp vs retail, MATCH |
| 7 `+0x38` | `?Destruct@PackageFile@IO@@` `0x180025300` | `package_file.cpp` | 240-byte memcmp vs retail, MATCH |

Slots 6 and 7 are 10 and 3 instructions. Both dispatch **through the object's own
vtable slot 0** with `flags = 0` (destruct in place, do not free), and the port
keeps that dispatch indirect: short-cutting to `RtDelDtor_PackageFile` would read
correct today, be wrong for anything derived, and hide which table is live.
`Reconstruct` then tail-jumps the ported constructor at `0x1800b4440`.

Slot 5 is the file format. Its record sequence — five Strings, two ints, three
counted arrays (two of String, one of uint), a bool, two ints — is transcribed in
full in the new file's banner with the retail address of every call, because the
ORDER *is* the specification. Three facts worth surfacing here:

* `packfileId` (+0xA8), `version` (+0x68) and `platform` (+0x74) are **NOT
  serialized**. Absences are as load-bearing as presences.
* the three array counts go through a **stack local**, not the field: retail
  serializes the local and lets `SetSize` write the count back. Serializing
  `&this->count` in place would work on write and leave a count disagreeing with
  the allocation on read.
* the `SetSize` calls are behind retail's own **reading-only** guard, which is
  exactly what makes the write-path replay oracle sound.

### 4.3 Two destructors — and why, against the census's own tag

| symbol | file | verified how |
|---|---|---|
| `??1TextFileStream@IO@@UEAA@XZ` `0x1800ba490` | `file_stream.cpp` | no oracle (see below); `objdump` transcription; all callees ported |
| `??1CompressedFileStream@IO@@UEAA@XZ` `0x1800a9330` | `file_stream.cpp` | same |

Both `UEAA`, the **virtual** spelling. `QEAA` is a phantom that resolves NULL for
ever (`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

Neither has an oracle, and the reason is the one `~Element` and
`~ElementDescription` already state: a replay would decrement each refcount a
second time and, where our body took one to zero, free a block twice, Commit
twice and close a closed handle. `RT_DIFF_WORK` records whether a call released
anything; `calls=` stays 0 because there is no comparison to report. Liveness,
not correctness — stated plainly rather than dressed up.

Every callee of both is PORTED, checked by grepping for each
`RegisterRuntimeAddr` rather than assumed: `~FileStream` `0x1800aaab0`,
`MemFree` `0x1800c7ad0`, `CompressedStream::Commit` `0x1800a9ee0`,
`FileSystem::CloseFile` `0x1800accc0`, `~CompressedStream` `0x1800aa650`. **Both
rows close; neither moves the frontier one frame deeper.**

## 5. THE FIVE INSTRUMENT-SOURCED ROWS — WHICH WAY I WENT, AND WHY

The census prints an `INSTRUMENT-SOURCED` block for rows whose *every* caller is
a probe/selftest/oracle file, with the note that a probe's resolve is a real
dependency but **not** a port obligation. Five of my rows were in it. **I split
them, and the split is not arbitrary — it turns on a defect in what that tag can
see.**

**PORTED — the two destructors (§4.3).** The tag is right about what it measured
and wrong as a scoping signal here, because it buckets a row by its **export-name
callers** and these two have a caller that is not a name at all:
`runtime/src/object/deleting_dtor.cpp`'s rows 40 and 41 reach `0x1800a9330` and
`0x1800ba490` **by ADDRESS** through `ResolveAddr`. Row 41's own comment says in
as many words that this made the address "newly `called`". With these bodies
absent, that edge went to the DLL before cutover and to **NULL** after — so every
`delete` of a TextFileStream or a CompressedFileStream faults. That is a real
cutover dependency, not board movement.

**NOT PORTED — the three `TextFileStream` accessors**
(`?GetObjectName@`, `?GetObjectType@`, `?GetObjectVersion@`). They are `QEBA`
(non-virtual), they are **not** vtable slots, and their only caller in the tree
really is `file_stream_probe.cpp`. They are also two instructions each — a `LEA`
and a `RET` — so porting them would have cost fifteen minutes and moved three
census rows. **That is precisely why I did not.** This project's own record is
that the item a handover calls "safest" is often safest only because it is
pointless (`reference_cheap_wins_list_is_the_stalest_section.md`), and a lane
that spends its budget on rows which satisfy nothing has moved a percentage, not
the port. They remain available to any later lane that wants them; they cost
nothing to find, and nothing depends on them.

## 6. `??_7TextFileStream@IO@@6B@` STAYS AT 8/10 — the two slots, unchanged

Both were already deferred by Wave 20 lane SAVETEXT with reasons I re-read and
**agree with**; nothing about them has expired, and I am not re-deferring them
with a new excuse:

| slot | symbol | size | why not |
|---|---|---|---|
| 2 `+0x10` | `?Write@TextFileStream@IO@@` `0x1800ba9f0` | 462 B | branches on `_requestedFormat != _activeEncoding` into the transcoder at `0x1800c9d60`, which has its own five-way chain |
| 8 `+0x40` | `?Open@TextFileStream@IO@@(Filename, Text::Format)` `0x1800ba640` | 666 B | the largest body in the table, and the one that INSTALLS the `buffer` that four of the five ported bodies branch on |

These are **size judgements, not blockers**, and they have no premise that can
expire — which is exactly what `text_file_stream_win.cpp`'s DEFERRED block
already says. Slot count re-confirmed at 10 from the retail `.rdata` with
`pefile` (§3), independently of the note in `rt_data_mint_tables.inc` that argues
the table is 10 and not 20.

## 7. RESIDUALS, EACH WITH AN ADDRESSEE

Warning #8: a condition without an addressee waits forever.

1. **`0x180029bb0` — unexported array-of-String serializer, ~680 B, NOT ported.**
   This lane's one new cutover obligation. It is a UTF-8/UTF-16 transcoder (the
   write path is `kScalarUInt(len | 0x80000000)` then `kArrayChar(len, data)` per
   element; the read path allocates and converts), so it is a body, not a stub.
   **ADDRESSEE: whichever Wave 28 lane owns IO's remaining address doors.** The
   door is commented in SYMBOL/CONDITION/CONSEQUENCE form at its helper in
   `package_file_serialize.cpp`.
2. **`?SetupFileSystem@Modification@IO@@QEAAXXZ` — in my brief, NOT attempted.**
   ~1500 bytes, mounts enabled mod packages into `FileSystem`; called by
   `runtime/src/engine/loops.cpp:212` and currently disabled there. Both
   singletons it needs (`?_singleton@Modification@IO@@`,
   `?_singleton@FileSystem@IO@@`) are tagged UNPROMOTABLE, and
   `_singleton@FileSystem` is route-scoped: **harmful with the DLL present,
   required at cutover** (`reference_unpromotable_is_route_scoped_not_permanent.md`).
   The only unattended verification route documented for the mod path —
   `BANISHED_AUTOMENU_BUTTON=7 …` — is **broken** with an ACCESS_VIOLATION in
   `ModDialog_ToggleMod_Impl+0x43`, so this body could be written but not
   verified. **ADDRESSEE: a Wave 28 lane paired with whoever fixes that route**
   (project doctrine: find the upstream null, do not guard the deref). Note also
   the recorded behaviour that a save made with mods mounted stalls or crashes on
   load once the `.pkm` are moved aside — not a defect a mods lane introduced.
3. **`??_7TextFileStream@IO@@6B@` slots 2 and 8** (§6). **ADDRESSEE: the Wave 28
   lane that takes IO's remaining bodies**, together with item 1 — the three are
   the same neighbourhood.
4. **`?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A` is minted but
   not promoted.** Promoting it is what makes the §4.1 oracle non-vacuous, and it
   would turn two by-construction ports into measured ones. **ADDRESSEE: the next
   lane that runs the mint promotion filter** — it is a one-row addition to
   `reg_mint_promoted.cpp`, and this lane deliberately did not touch that file
   because promotion is not a slot-porting decision.

## 8. THE LANE'S OWN MISTAKE, RECORDED BECAUSE IT GENERALISES

The first version of the `Serialize` oracle put the probe block **inside** the
body — the shape `goal_serialize.cpp` and `elementdescription_serialize.cpp` use
— and had the sweep probe drive it with
`RtIo_PackageFile_Serialize(pkg, nullptr)`, on the argument that `Compare`
substitutes its own sink so the null Stream is never dereferenced. **It died with
an ACCESS_VIOLATION, READ of `0x10`, in `MakeCodec+0x2b`.**

My first reading of the crash was "the probe budget is 16 and the seventeenth
package fell through". That is a plausible mechanism that fits the evidence and
**it is not the measured one.** Symbolising the stack
(`reimpl-x64.exe+0x3375B9 ← RtIo_PackageFile_Serialize+0xff ←
RtIo_PackageFile_SweepProbe+0x31b`) shows the real cause: **the probe block does
not RETURN.** After `Compare` comes back, control falls into the real body, which
reads `stream->encoding` at `+0x10` off the null — on *every* call including the
first. The single `[RtSaveTrace] pass` line in that run is the probe succeeding
immediately before the body it tapped killed the process.

Two things generalise:

* **The in-body probe shape is right for a body with real callers and wrong for a
  body without any.** For those two files the probe is a tap on real traffic; here
  the driver *is* the traffic, and a driver that must pass a null `this`-argument
  to reach a probe is a driver calling the wrong function. The fix was to give the
  instrument its own door (`RtIo_PackageFile_SerializeProbe`), **not** a null
  guard in the body — a guard there would be guarding retail's own unguarded
  deref for an instrument's convenience.
* **A plausible cause that fits the evidence is not a measured cause.** Only the
  symbolisation separated "budget exhausted on call 17" from "falls through on
  call 1". Both comments that carried the wrong story have been corrected in
  place.

## 9. FILES TOUCHED

| file | what |
|---|---|
| `runtime/src/io/package_file_serialize.cpp` | **new** — `?Serialize@PackageFile@IO@@`, its oracle door, its registration |
| `runtime/src/io/package_file.cpp` | `?Reconstruct@`, `?Destruct@`, the two memcmp oracles, the Serialize probe driver, registrations |
| `runtime/src/io/file_stream.cpp` | `~TextFileStream`, `~CompressedFileStream`, registrations |
| `runtime/src/io/rt_io_port.cpp` | wires `RtIoPackageFileSerializeRegister` behind `BANISHED_RT_PKGSER` |
| `runtime/src/ui/dialog.cpp` | `?Type@`/`?TypeName@ElementDescription@`, the typeInfo door, registrations, **the ⚠ correction at `:1471`** |
| `runtime/src/object/deleting_dtor.cpp` | spec row 44 `UI::ElementDescription`, its body and registration; three stale `(NOT ported)` comments corrected |
| `runtime/src/object/deleting_dtor_internal.h` | `kSpecCount` 44 → 45 |
| `docs/runtime_port/WAVE27_LANE_IOTEXT.md` | this file |

No tool was written, fixed or extended. `file_stream_probe.cpp` was not touched.

## 10. GATES

* `mingw32-make standalone-debug` → exit 0.
* Game dir lock acquired non-piped, non-backgrounded, as `IOTEXT`; released at
  the end of the lane. `assert-held` with the exe path run before every launch,
  so no measurement was taken against another lane's binary.
* `tools/rt_deleting_dtor_verify.py` → 45 rows, problems: 0.
* `tools/boot_state.sh` → RUNNING; `standalone_crash.log` zero bytes.
* `standalone/cutover_manifest.inc` now reports `GATE WARN: stale in COSMETIC
  fields only (a by-name door count moved)` — that moved count is this lane's two
  new by-name registrations. The gate itself says `OK (60 rows; row set and every
  disposition match a fresh derivation)` and explicitly does **not** fail the
  build. Deliberately NOT refreshed here: the `.inc` is a generated artefact the
  integrator derives at merge, and refreshing it from one lane's branch would put
  a mid-wave snapshot in the file the next lane's build compares against.
  **ADDRESSEE: the Wave 27 integrator** — `python tools/cutover_manifest.py --inc`
  once the lanes are merged.
