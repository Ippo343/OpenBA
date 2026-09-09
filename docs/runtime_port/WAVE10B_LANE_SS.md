# Wave 10B — lane SS

`?Serialize@SpriteSheet@UI@@` (retail `0x18011d730`) ported, and
`??_7SpriteSheet@UI@@6B@` promoted — **the first `??_7` vtable on
`standalone/reg_mint_promoted.cpp`**.

* worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-aabadce277bcf17f8`
* branch `wave10b/lane-ss`, base `wave10/base` @ `453e26d`

---

## 1. The stated blocker was NOT REAL — and it was wrong in two separate ways

`WAVE10_UI_CENSUS.md` §6, `WAVE10_LANE_UI4.md` §6 and the banner of
`runtime/src/ui/spritesheet_vtable.cpp` all said the same thing: slot 5 drives
four calls through **`IO::Encoding`'s vtable at `+0xa0`** and one at `+0x78`,
those offsets differ between retail and the profile build, and the row expires
only when somebody produces "a slot map for retail's `??_7Encoding@IO@@6B@`".

Both halves are false, and the second one is the expensive one.

### 1a. They are NOT Encoding slots

Read the `this` register at each of the five calls in the shipped
disassembly. Every one of them is preceded by

```
MOV RCX,qword ptr [RSP + 0x30]
MOV RAX,qword ptr [RCX]
```

at `0x18011d793`, `0x18011d7a8`, `0x18011d7bd` and `0x18011d7d6`. `RSP+0x30` is
the **second field of the local `IO::Serializer`** — the `IO::ReaderWriter*`
that the encoding's factory slot returned three instructions earlier. The
`IO::Encoding` is touched exactly **once** in the whole body, at `0x18011d755`
(`MOV RCX,[RDX+0x10]`), to pick `+0x18` or `+0x20`.

So `+0x78` and `+0xa0` are **ReaderWriter** slots. The holdback named the wrong
class, and every consequence followed from that.

### 1b. The map it asked for already existed, for the right class

`rt::io::rw_vtbl` in `runtime/types/io/Serializer.h` has carried **all 26 retail
`IO::ReaderWriter` slot offsets since Wave 4**, doubly pinned (each
`IO::Serializer` forwarder's own tail-jump displacement, *and* the contents of
`??_7BinaryReader@IO@@6B@` / `??_7BinaryWriter@IO@@6B@`). Its own banner carries
the standing warning against the profile build's `+0x90/+0xb0/+0xb8`. It names

```
rw_vtbl::kScalarBool = 0x78
rw_vtbl::kScalarInt  = 0xA0
```

**Nothing had to be produced.** The holdback was a misattribution on top of a
search that never reached the file holding the answer — the fourth time this
project has recorded a stale blocker that a five-minute re-derivation dissolves
(`reference_holdback_blockers_are_stale_check_them_first`).

### 1c. I re-derived the two offsets anyway rather than trusting the file

`rw_vtbl` is a claim in a header; a claim is not evidence. Read
`??_7BinaryWriter@IO@@6B@` (retail `0x180155210`) directly and resolve the two
slots:

| slot | entry | Ghidra signature at that address |
|---|---|---|
| `+0x78` (idx 15) | `0x1800a8f30` | `Serialize(BinaryWriter*, bool*, char*)` |
| `+0xa0` (idx 20) | `0x1800a8fd0` | `Serialize(BinaryWriter*, int*, char*)` |

Independent agreement with `rw_vtbl`. A **third**, accidental confirmation fell
out of the port: the per-entry helper `0x180078ad0` walks each `SpriteInfo`
through `+0xa0` for its five ints and `+0x80` for its four floats, and `+0x80`
is `rw_vtbl::kScalarFloat` — a slot the SpriteSheet body itself never uses.

---

## 2. The deliverable that WAS asked for: retail's Encoding vtable, slot by slot

Produced anyway, because the lane brief asked for it and because it is what
settles §1a for the next reader. This is the concrete `IO::Encoding` vtable the
binary-stream family installs — the one `standalone/rt_data_mint_tables.inc`
carries as an **unexported, address-only** row and the shadow report prints as
`BLOCKED (unexported @0x1801554a8) slots= 5 ported= 2 TRAP= 3`.

**Retail `Runtime-x64.dll`, image base `0x180000000`, vtable at `0x1801554a8`,
5 slots. Read out of the shipped bytes 2026-09-09.**

| offset | idx | target | shipped body | meaning |
|---|---|---|---|---|
| `+0x00` | 0 | `0x1800a81e0` | `MOV AL,1 / RET` | constant-`true` predicate. **NOT a destructor** — the usual slot-0 assumption is wrong for this class. |
| `+0x08` | 1 | `0x1800a81d0` | `XOR AL,AL / RET` | constant-`false` predicate |
| `+0x10` | 2 | `0x1800a81c0` | `MOV EAX,2 / RET` | encoding KIND. This binary encoding answers **2**. `runtime/src/achievement/goal_serialize.cpp` and `code/Game/Action/action_base_move.cpp` both branch on this slot returning **3**, which is the *visitor* encoding, not this one. |
| `+0x18` | 3 | `0x1800a8120` | allocates `0x30` under the allocator lock, then `IO::BinaryReader::BinaryReader(this, serializer, version)` | **CreateReader** `(Encoding*, Serializer*, int) -> ReaderWriter*` |
| `+0x20` | 4 | `0x1800a8080` | allocates `0x30` under the allocator lock, then `IO::BinaryWriter::BinaryWriter(this, serializer, version)` | **CreateWriter**, same signature |

`+0x18`/`+0x20` agree with `rt::io::encoding_vtbl::kCreateReader/kCreateWriter`,
and the two factory bodies confirm Wave 4's **arity-3** finding directly: both
take `param_3` and hand it to the codec ctor as the save-format version.

The three trap slots in the shadow report are `+0x00`, `+0x08`, `+0x10` — three
one- or two-instruction constant returns. They are unexported (address-only
doors) and belong to the IO family, not to this lane.

---

## 3. The port

`runtime/src/ui/spritesheet_vtable.cpp`, alongside the other eight slots.
Full instruction-by-instruction transcription is in that file's banner; the
things worth repeating here are the ones that would have been silent defects:

* **The two Strings come off the wire `+0x40` FIRST, then `+0x38`** — the
  opposite of field order. Getting it backwards does not crash; it swaps two
  String records of usually-different length and desyncs everything after.
* **The count is a stack copy**, never the field. `m_spriteCount` is updated by
  `SetSize`, not by the record.
* **`m_sprites` is re-read AFTER `SetSize`**, which reallocates. Hoisting that
  load above the direction branch would hand the per-entry walker a freed block
  on the read path.
* The `Serializer` construction is **inlined** in retail, not a call to
  `??0Serializer@IO@@`, and it moves the stream cursor by 12 bytes — which is
  why the oracle uses synthetic sinks and not the real stream.

### Grepped before writing (address AND mangled name)

`0x18011d730` and `?Serialize@SpriteSheet@UI@@…` appear in `docs/`,
`notes/wp_runs/`, `perf/`, and as a `RT_MINT_NAMED` trap row in
`standalone/rt_data_mint_tables.inc` — **nowhere as a body**. No duplicate
registration.

### Three address doors this port OPENS

A faithful port that calls DLL internals by address grows the cutover surface
(`reference_porting_can_grow_the_cutover_surface`). Stated here rather than left
for a later census:

| address | what | status |
|---|---|---|
| `0x1800c9930` | `?Serialize@String@System@@` | exported **and already ported** (`runtime/src/system/string.cpp` registers both doors) — this door lands on our body |
| `0x180078950` | `Array<SpriteInfo>::SetSize`, stride `0x38` | **unported.** Calls `MemoryAllocator::MemAlloc/Free` → class-B allocator family. Same shape as the 14 `kSpec_*` rows in `runtime/src/collection/array.cpp` and as lane UI4's recorded `0x18004bde0` (stride `0x28`). A one-row follow-on for the collection family. |
| `0x180078ad0` | per-`SpriteInfo` record walker | **unported**, pure. Cheap follow-on. |
| `0x180014520` | `ExternalPtr<Graphics::MaterialInstance>::Serialize` | **deliberately the DLL's.** One of the 25 instantiations lane EXTPTR measured (`WAVE10_LANE_EXTPTR.md:79`); that lane registered 4 and left the Graphics ones alone. Registering it here would be the duplicate-registration trap it refused. |

### A type identified as a side effect

`SpriteSheet+0x30` was `m_field30 // unknown`. It is the argument to
`0x180014520`, so it is an
**`Object::ExternalPtr<Graphics::MaterialInstance>`** — the sheet's material
handle. Recorded in `runtime/types/ui/SpriteSheet.h`; the field is **not**
renamed, because the evidence names the type, not the member.

---

## 4. What was measured

Four boots in three lock sessions, debug build, DLL present,
`BANISHED_AUTOQUIT=300`, every one under
`tools/game_dir_lock.sh run lane-SS 1800 --snapshot …`. Logs read from the
snapshots (`lane_ss_logs/`, `lane_ss_logs2/`, `lane_ss_logs3/`), never from the
game dir. Exe md5-checked against the build immediately before each session.

### Gate (a) — the shadow readiness screen

```
baseline (docs/runtime_port/wave10b/baseline_shadow_7358732.txt:458)
  BLOCKED  ??_7SpriteSheet@UI@@6B@   slots= 9 ported= 8 TRAP= 1 purecall= 0
             TRAP slot  5  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z

this lane (lane_ss_logs3/shadow_debug.log:477 and :1725, BOTH passes)
  READY    ??_7SpriteSheet@UI@@6B@   slots= 9 ported= 9 TRAP= 0 purecall= 0  [PROMOTED]
```

The screen moved for a reason and is not stuck at READY.

### Gate (b) — the boot A/B

`BANISHED_RT_MINT_ROWS=??_7SpriteSheet@UI@@6B@` vs the same boot without it
(both with the 10 pre-existing typeinfo promotions on, so the row is the only
variable):

* both exit `0`, both `standalone_crash.log` are 0 bytes;
* both end on the identical two closing lines;
* `Dialog/SpriteSheet.rsc` loads in both, and both resolve
  `Sprite("SaveLoadEmpty")` to index **211**;
* the normalised log diff is the two expected `[RtMint] ROW ON …` lines plus
  splash-frame-count and camera-animation jitter. Nothing else.

Repeated on the DEFAULT-ON configuration after the row was added to
`reg_mint_promoted.cpp` (`promoted` vs `BANISHED_RT_MINT_PROMOTED=0`): same
result, `[RtMint] promoted 11 minted DATA row(s)`.

### The effectiveness control — because "it still boots" is also what a NO-OP reports

A promotion that did nothing would pass gate (b) exactly as cleanly
(`reference_probe_needs_an_effectiveness_control`). With `BANISHED_RT_DIFF=1`
added to both halves, the promoted half **and only** the promoted half shows:

```
[RtDiff] DIVERGENCE UI::SpriteSheet::Construct call #2
           ours=+0x0: 60 bf 0e 5b f7 7f 00 00     <- an address in OUR image
           theirs=+0x0: 90 45 5e ab fc 7f 00 00   <- DLL 0x180154590 + ASLR
[RtDiff]   UI::SpriteSheet::Construct   calls=3  worked=1  divergences=1
[RtDiff]   UI::SpriteSheet              calls=2  worked=?  divergences=0
```

* The `Construct` divergence is the **eight bytes at `+0x00`** and nothing else:
  our `Construct` now stamps the minted table. That IS the promotion.
* `UI::SpriteSheet` is the **deleting destructor**
  (`runtime/src/object/deleting_dtor.cpp:281`). It has **no site at all** in the
  control run and `calls=2 divergences=0` in the promoted one — i.e. real
  dispatch went through slot 0 of the minted table and came back clean.

Whole-run diverging-site **set** diff (not the count):

```
promoted: UI3 pin negative control, DLG2 pin negative control,
          Object::Factory::Startup [probe, not diffable],
          DialogInstance::StartTimer [negative control],
          UI::SpriteSheet::TypeName, UI::SpriteSheet::Construct
killed:   the same four controls only
```

`TypeName`'s divergence is the **pre-existing** `?_typeInfo@SpriteSheet@UI@@`
promotion (lane MINTSEL), not this row: it is present in a run with the vtable
row OFF and the typeinfo rows ON (`lane_ss_logs/diffctrl_error.log:770`), and
absent only when `BANISHED_RT_MINT_PROMOTED=0` turns all eleven off. Both are
identity differences by construction and are documented next to the row so a
later reader does not read the red line as a regression.

`[RtSaveTrace] ==== 18 compared, 0 diverged ====` in both halves; no crash logs.

---

## 5. What is NOT verified, stated plainly

**`?Serialize@SpriteSheet@UI@@` is PORTED, NOT VERIFIED.**

Its write-path oracle (`RtSaveTrace::Compare`, wired in the body exactly as
`elementdescription_serialize.cpp` wires its own) reported **no entry at all**
across all four boots — including the two with `BANISHED_RT_SAVE_TRACE=1` and
the row promoted. That is the `calls=0` column, and `calls=0` is not a pass
(`reference_uncalled_port_is_not_verified`).

The cause is identified rather than left as one of the four ambiguous ones: the
sheet that *is* deserialised at boot (`Dialog/SpriteSheet.rsc`) is built by the
DLL's own `ExternalFactory`, so it carries the DLL's vtable and dispatches
slot 5 off that table. The objects our `Construct` builds under the promotion
(three of them) are never serialised. There is no harness route in the tree
today that drives a SpriteSheet through a Serialize on an object our ctor
built — I looked for one (`grep` over `standalone/`, `tool_harness.cpp`, and
the `BANISHED_*` list) before writing this, per
`reference_grep_the_harness_before_writing_unverifiable`.

**Promoting is still the right call**, and the reason is not "it looks fine":
`TRAP= 0` means the slot can no longer `abort()` if it *is* reached, the other
eight slots ARE exercised and clean, and leaving the row unpromoted would not
have verified slot 5 either — it would only have kept the eight measured slots
unreachable as well.

Also not verified, and not claimed: the three unported callees above run as the
DLL's bodies. Nothing about them is asserted by this lane beyond their
addresses and shapes.

---

## 6. Census — the SET diff

`tools/runtime_census.py --unported --namespace UI`, both sides run with the
**same tool revision from this worktree**, the "before" taken by checking out
`453e26d`'s copy of the three changed files (the scanners walk `os.walk`
relative to the CWD — `reference_scanner_walks_cwd_not_script`).

```
UI unported rows: 6 -> 5

LEFT the unported set (closed by this lane):
  ??_7SpriteSheet@UI@@6B@

ENTERED the unported set:
  (nothing)
```

Remaining five, for the other Wave 10B lanes:

```
??_7Dialog@UI@@6B@
??_7DialogController@UI@@6B@
??_7DialogInstance@UI@@6B@
??_7Element@UI@@6B@
?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
```

Both build targets verified from a clean tree: `mingw32-make standalone-debug`
and `mingw32-make standalone` (release, the one a lane has broken before).

---

## 7. Follow-on work found, not done

1. **Three documents still carry the false blocker** —
   `docs/runtime_port/WAVE10_UI_CENSUS.md` §6 and
   `docs/runtime_port/WAVE10_LANE_UI4.md` §6 both still say "the blocker is
   those two vtable offsets … needs a slot map for retail's
   `??_7Encoding@IO@@6B@`". They are other lanes' documents and were not edited
   here; the correction is recorded in `spritesheet_vtable.cpp`'s banner and in
   §1 above. **The integrator should strike those paragraphs.**
2. `0x180078950` (`Array<SpriteInfo>::SetSize`, stride `0x38`) and
   `0x18004bde0` (stride `0x28`, lane UI4's) are two one-row additions for the
   collection family's `kSpec_*` table.
3. `0x180078ad0`, the per-`SpriteInfo` walker, is a small pure body with a
   ready-made oracle route (it is entered whenever slot 5 is).
4. The three unexported `IO::Encoding` slots (`+0x00`, `+0x08`, `+0x10` in the
   table in §2) are three constant-return bodies away from that vtable row
   going `READY`.
5. A harness route that drives a Serialize on an OUR-constructed SpriteSheet
   would convert §5 from "ported, not verified" into a measurement. It does not
   exist today.
