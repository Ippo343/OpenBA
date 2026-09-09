# WAVE 10 — LANE UI4 — `UI::SpriteSheet` / `UI::SpriteCapture`

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-af651482ef976f5ab`,
branch `wave10/lane-ui4`, cut from `wave10/base` @ `3318690`.

---

## 1. The three census rows, and what each of them actually was

The lane was handed three rows, all recorded UNREACHABLE with one blocker:
*"dev capture tool; `_spriteCapture` is null in both harnesses"*. Re-testing
that verdict first was the right instruction — **it was wrong about two rows
and about the third for a completely different reason than the one recorded.**

| row | recorded verdict | what it actually was |
|---|---|---|
| `?AddSprite@SpriteCapture@UI@@QEAAXAEBVSprite@2@HHHHMI@Z` | unreachable | **portable and verifiable now** — closed |
| `?AddScissor@SpriteCapture@UI@@QEAAXHHHH_N@Z` | unreachable | **portable and verifiable now** — closed |
| `??_7SpriteSheet@UI@@6B@` | unreachable | **already minted since Wave 9** — a census artefact. The real work behind it was the 8 trap slots INSIDE it, 7 of which are now closed. |

---

## 2. Why "`_spriteCapture` is null" was not a blocker

The recorded blocker asks the wrong question. It records the *state of a
field*, when what decides reachability is **what the guard reads and who is
allowed to write it**.

`SpriteContext::AddSpriteDraw` and `SetScissor` guard their capture branches on
`_spriteCapture != nullptr`. That field is not privileged state — it is a plain
pointer, copied from `UserInterface+0x110`, which
`?BeginSpriteCapture@UserInterface@UI@@QEAAXPEAVSpriteCapture@2@VViewport@Video@@@Z`
(0x18011f4c0) sets in **five stores and nothing else**.

And the exe already contains a caller. `SpriteCaptureUI::OnEvent`
(Application-x64.exe **0x1400792b0**, the handler for the `buttonCapture`
element `SpriteCaptureUI::Serialize` gives id 1000) does exactly this:

```
capture = System::MemoryAllocator::MemAlloc(0x58, 8);
UI::SpriteCapture::SpriteCapture(capture);                  // exported 0x1800d9140
*(void**)capture = &PTR_SpriteCaptureUI_Slot12_14015d550;   // derived vtable
this->pendingExternal = capture;                            // +0x60
UI::UserInterface::BeginSpriteCapture(dialogInstance->ui, capture);
```

Found by `get_xrefs_to` on the exe's own import slots for
`??0SpriteCapture@UI@@QEAA@XZ` (0x140152c10) and `BeginSpriteCapture`
(0x140151d78) — one caller each, the same function.

**Every ingredient is an export.** So the dependency was never unreachable; it
was *unrequested*. That is the distinction
`reference_synthetic_subject_beats_a_fake.md` draws: there is no need to build a
fake `SpriteCapture`, because a **real** one can be constructed and driven, and
a fake would have been the strictly worse instrument.

### What was checked before writing "unreachable" would have been wrong

Per the lane brief, both screens were run first and both came back empty, which
is why the route above had to be found in the binary rather than in the harness:

* `standalone/` grep — `tool_harness.cpp` and every other driver: **no**
  SpriteCapture route exists.
* The full `BANISHED_*` env-var list (≈900 names, enumerated from source):
  `BANISHED_CAPTURE_INGAME` is the closest name and is **unrelated** (it arms a
  RenderDoc frame capture in `gameupdateloop.cpp`). No sprite-capture switch
  exists.

So `reference_grep_the_harness_before_declaring_no_route.md` was honoured and
came back negative — and the route was found one level down instead.

---

## 3. The fact that decides the whole port, and where Ghidra's C lies

Both bodies end in two array appends:

```
CALL 0x18004bfc0   RCX = this+0x40     ; Array<T>::SetSize(n, fill)
CALL 0x18004bde0   RCX = this+0x30
```

`0x18004bfc0` is **already ported** in `runtime/src/collection/array.cpp` as
`J3_ENTRY3(18004bfc0, kSpec_Fill16)` — stride 0x10 — so `this+0x40` is
`Array<UI::Sprite>`.

`0x18004bde0` computes its byte count as `LEA RCX,[RAX+RAX*4]; SHL RCX,3` at
**0x18004be38** — **stride 0x28**.

That single number decides everything. Read as a 4-byte stride (which is what
the shape of Ghidra's decompiled C suggests, since it shows only an `int` being
passed), the 0x28-byte block each body builds on its stack is an **unreferenced
dead local**, and both functions throw away every coordinate, colour and
rotation they were handed. Read as 0x28 it **is the appended element**, and
every field lands somewhere.

`reference_ghidra_c_output_vs_disassembly.md`, fourth instance. The decompiler
was not wrong about any instruction; it simply could not show the stride, and
the stride is the semantics.

### Recovered layout (`runtime/types/ui/SpriteCapture.h`)

`UI::SpriteCapture`, 0x58 bytes, derives `Object::External`:

| off | field | note |
|---|---|---|
| +0x20 | `Array<ExternalPtr<SpriteSheet>> _sheets` | stride 8, de-duplicated |
| +0x30 | `Array<CaptureEntry> _entries` | stride 0x28 |
| +0x40 | `Array<UI::Sprite> _sprites` | stride 0x10 |
| +0x50 | `_width` | `GetWidth` = `MOV EAX,[RCX+0x50]` |
| +0x54 | `_height` | `GetHeight` = `MOV EAX,[RCX+0x54]` |

`CaptureEntry` (0x28): `int sheetIndex; /*pad04, never written*/; String name;
int x1,y1,x2,y2; float rotation; uint color;`

The two streams are **parallel**: one `_sprites` element and one `_entries`
element per recorded thing. A scissor is recorded with
`_sprites[i] = Sprite{null,-1}` and a **negative** `sheetIndex`
(`-1` normal, `-2` full-screen, from `NEG byte / SBB EAX,EAX / DEC EAX`), which
is how a reader tells a scissor from a draw.

### A retail defect that is reproduced, not fixed

`CaptureEntry::name` is a refcounted `System::String`. Both bodies build the
entry holding an **owned** reference, hand it to `SetSize` — which copies
elements bitwise, taking no reference of its own — and then release the local's
reference on the way out. Net refcount change: **zero**. So the stored entry
points at a string nobody holds, and in `AddScissor`'s case (a freshly
allocated `""`) the block is **freed before the function returns**, leaving the
stored pointer dangling.

Nothing in retail reads `Entry::name` back — `Serialize` and `Draw` work from
`_sheets` and `_sprites` — which is why it was never noticed. Per CLAUDE.md, a
guard here would not restore behaviour, it would invent different behaviour and
hide the fact, so the port reproduces it and says so in three places.

---

## 4. What was ported

### `runtime/src/ui/spritecapture.cpp` (new) — the two census rows

* `RtUi_SpriteCapture_AddScissor` — 0x1800d8ef0.
* `RtUi_SpriteCapture_AddSprite` — 0x1800d8b20, including the **inlined**
  `Array<void*>::SetSize` for `_sheets` (the compiler inlined it, so there is
  no callee to delegate to) with its 3x growth heuristic, and the two arms of
  the entry name (`GetSpriteNameFromIndex` vs an open-coded `MemAlloc(9,4)`
  empty string — retail does *not* call `String::Set` on that arm).

Registered under **both doors**, name and address. The address door is not
optional here: `runtime/src/ui/spritecontext.cpp` reaches these two by address
(0x1800d8b20 / 0x1800d8ef0 are its two capture doors), so a name-only
registration would have left the only live caller in the tree walking straight
past the port into the DLL — `reference_bypassed_port_needs_a_source_checker.md`.

### `runtime/src/ui/spritesheet_vtable.cpp` (new) — 7 of the 8 vtable walls

`??_7SpriteSheet@UI@@6B@` was **already minted** by Wave 9 lane C2
(`kVSlots45` in `standalone/rt_data_mint_tables.inc`, 9 slots, registered by
`rt_data_mint.cpp` under both doors). The census cannot see that registration
because it is spelled through a struct field rather than a literal
(`reference_census_cannot_see_nonliteral_registrations.md`), and the mint is
additionally gated behind `BANISHED_RT_MINT=1` until cutover.

The real work was the slot **contents**. A minted slot is a NAMED TRAP until
its body is ported. Measured before this lane: **1 ported, 8 traps** — which is
exactly `WAVE10_PARTITION.md`'s "8 predicted vtable walls" for UI4, confirming
the partition's arithmetic from the other side.

| slot | symbol | status |
|---|---|---|
| 0 | deleting dtor 0x1800037b0 | already ported (Wave 9 lane K) |
| 1 | `?TypeName@` 0x180002e50 | **ported** |
| 2 | `?Type@` 0x180002e30 | **ported** (+ the static `?TypeId@` 0x180002e40) |
| 3 | `?GetFileVersion@` 0x18011d710 | **ported** — `MOV EAX,6; RET` |
| 4 | `?Compile@` 0x18011d720 | **ported** — `MOV EAX,1; RET`, argument ignored |
| 5 | `?Serialize@` 0x18011d730 | **HELD BACK** — see §6 |
| 6 | `?Reconstruct@` 0x180003080 | **ported** |
| 7 | `?Destruct@` 0x180003180 | **ported** |
| 8 | `?Construct@` 0x180003100 | **ported** |

`Construct` has retail's own null test; `Reconstruct` does **not** (0x180003092
dereferences RCX immediately) and the port leaves that deref faithful.

### `runtime/src/ui/sprite.cpp` — one callee this lane opened

Porting `AddSprite` turned `?GetSpriteNameFromIndex@SpriteSheet@UI@@…` from a
symbol nothing referenced into a **new** unported census row — the measured
shape of `reference_porting_can_grow_the_cutover_surface.md`. It is closed
rather than routed around: 18 instructions, no bounds check, hidden return
slot, replay oracle with the refcount bump undone.

---

## 5. Verification

### The oracle: `runtime/src/ui/spritecapture_probe.cpp`

Waiting for a caller buys nothing here — a longer run has the same shape and a
bigger denominator, and the port would report `calls=0` forever
(`reference_uncalled_port_is_not_verified.md`). So the probe drives **our body
and the SHIPPED body on two real, independently-constructed `SpriteCapture`
objects** over the same inputs and compares them field by field.

This is an oracle, not a self-check: **theirs runs for real**. An
`RT_DIFF_SHADOW` site would have taken our own verdict and never consulted the
DLL, and a shared misunderstanding of retail would pass every time —
`reference_shadow_marker_is_not_an_oracle.md`.

The shipped bodies are reached through **`ResolveRuntimeRaw`**, never
`ResolveAddr`: this lane registers both addresses, so `ResolveAddr` would have
handed back our own function and the oracle would have compared ours to ours
and agreed unconditionally
(`reference_resolveaddr_consults_port_registry_first.md`).

Cases: both `fullScreen` arms over a 7-rect sweep including negative, zero and
`INT_MAX` values; `AddSprite` with an invalid index (open-coded `""` path, plus
the first-append arm where the 3x heuristic must NOT fire); a dedup sequence
covering "found at index 0", "found later", and the second-sheet append where
the heuristic DOES fire; and both arms of `GetSpriteNameFromIndex` (a sheet
entry with a name and one without), driven against a real hand-built
`SpriteSheet` — real because that callee is three loads and touches nothing
else.

**Three things are deliberately not compared**, each named at its use site
because each would otherwise fail 100% of cases and read as a catastrophic port
defect (`reference_oracle_must_not_compare_unwritten_bytes.md`):

1. the three array `data` pointers (different heap blocks by construction — the
   CONTENTS are compared instead);
2. `CaptureEntry::pad04` (neither body writes it);
3. `CaptureEntry::name` **only on the cases that allocate a fresh string** —
   the two runs allocate different blocks, and in `AddScissor`'s case the
   stored pointer is already dangling, so dereferencing it to compare contents
   would be a use-after-free the probe must not commit. Those cases compare
   null-ness only. The `GetSpriteNameFromIndex` cases do **not** allocate, so
   they compare the pointer itself — that is the one hard check on `name`.

**Effectiveness control** (`reference_probe_needs_an_effectiveness_control.md`):
the probe reports how many entries and sprites it actually compared, not just
how many cases ran — a green probe whose comparisons look at nothing is worth
nothing.

**Negative control**: `BANISHED_RT_SPRITECAP_NEGCTL=1` corrupts
`CaptureEntry::color` of the last entry our body appended. `color` is in the
compared set, which is the point — a control that corrupted something nothing
compares would indict a sound instrument
(`reference_control_must_pick_a_compared_subject.md`).

**Refcount ledger**: the probe's hand-built name string starts at refcount 1 and
the whole path is net-zero on it; the probe checks the count afterwards rather
than assuming it.

### Running it

```
BANISHED_RT_DIFF=1 BANISHED_RT_SPRITECAP_PROBE=1 ./reimpl-x64.exe
grep SpriteCapProbe standalone_debug.log
```

Kill switches: `BANISHED_RT_UISPRITECAP=0` leaves the whole lane on the DLL;
the probe is additionally gated on the diff harness because it allocates
through the real `MemoryAllocator` (unavoidably — every `AddSprite` path either
calls `String::Set` or open-codes a `MemAlloc(9,4)`).

Full output, both runs, in `docs/runtime_port/wave10_lane_ui4/probe_results.txt`.

**GREEN RUN — `cases=18 failed=0  entries compared=21 sprites compared=21`.**

**NEGATIVE CONTROL — `cases=18 failed=4`, and the reported field is
`entry.color` in all four**, i.e. every one of the four `AddSprite` cases was
caught, on the field the control corrupts. The 14 `AddScissor` cases correctly
stay green (the control only touches what `AddSprite` appended), which is the
control's own specificity check rather than a miss.

### The ledger caught a wrong conclusion — and this is the lane's best finding

The probe's refcount ledger reported **+2** where the first draft of this lane
predicted **0**, and it attributes per implementation: `before=1 afterOURS=2
afterTHEIRS=3`. **Symmetric.** Both implementations behaved identically, so the
port was not at fault — the *theory* was.

That theory was this document's original claim that `SetSize` copies elements
bitwise, so the stored `CaptureEntry::name` is owned by nobody and retail
leaves a dangling pointer. Reading the rest of `0x18004bde0` settles it:

```
18004be97: MOV EAX,[RDX]        18004be99: MOV [RCX],EAX       ; sheetIndex
18004be9b: MOV [RCX+0x8],R12                                   ; name = null
18004be9f: MOV RAX,[RDX+0x8]    18004bea3: TEST RAX,RAX  JZ …
18004bea8: MOV [RCX+0x8],RAX    18004beac: INC dword [RAX-0x4] ; ++refcount
```

The element copier **copy-constructs** the String. The array element owns its
reference and **retail's accounting is exactly right**; the +1 per side is the
probe's own `FreeSubjects()` releasing the raw buffer without running element
destructors, which is now the documented, asserted expectation.

Two things worth carrying forward:

* A **"retail is buggy" conclusion deserves the same scepticism as any other.**
  CLAUDE.md already says not to "fix" a faithful crash with a null check; the
  same caution applies to *documenting* retail as defective. It nearly went
  into a type header as fact.
* The **exact** expectation is what made it visible. Had the ledger been
  written as "non-zero is fine" or omitted as a nicety, the wrong claim would
  have shipped in three comment blocks with nothing to contradict it. The
  assertion is deliberately kept exact (`+2`) rather than loosened.

That listing also gives an **independent second witness for `pad04`**: the
copier writes +0x00, jumps to +0x08, and never touches +0x04..+0x07 — matching
what both producing bodies do, and confirming the one window the oracle
excludes.

---

### `calls=` read honestly, per site

`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md` — `calls=`
counts REPORTING entries, and four different causes produce a zero. From the
final run:

```
UI::SpriteCapture::AddScissor              calls=0  worked=14  divergences=0
UI::SpriteCapture::AddSprite               calls=0  worked=7   divergences=0
UI::SpriteSheet::GetSpriteNameFromIndex()  calls=2  worked=?   divergences=0
UI::SpriteSheet::Type                      calls=1  worked=1   divergences=0
UI::SpriteSheet::TypeId                    calls=1  worked=1   divergences=0
UI::SpriteSheet::TypeName                  calls=1  worked=1   divergences=0
UI::SpriteSheet::GetFileVersion            calls=1  worked=1   divergences=0
UI::SpriteSheet::Compile                   calls=1  worked=1   divergences=0
UI::SpriteSheet::Construct                 calls=3  worked=1   divergences=0
UI::SpriteSheet::Reconstruct               calls=0  worked=1   divergences=0
UI::SpriteSheet::Destruct                  calls=0  worked=1   divergences=0
```

* **`AddScissor` / `AddSprite`: `calls=0` does NOT mean dormant.** `worked=14`
  and `worked=7` say the bodies ran 21 times — which is exactly the probe's 14
  scissor cases plus 7 sprite appends, and exactly the 21 entries the probe
  compared. The zero is because these two bodies contain no in-body
  `RT_DIFF_*` reporting macro at all: their comparison lives in the probe and
  is reported there. **Verified, by the probe, not by this column.**
* **`Reconstruct` / `Destruct`: `calls=0 worked=1`** — driven once each and
  deliberately not compared, because the destructor half runs on the live
  object and cannot be replayed. **Exercised, not verified.** That is the
  honest status and it is not upgraded by the run's silence.
* The remaining seven are real replays against the shipped exports.
* **No LIVE caller anywhere.** Every number above is the probe's. Both capture
  bodies are reachable only via `BeginSpriteCapture`, and the only caller of
  that is `SpriteCaptureUI::OnEvent` — a dev tool with no harness route.

---

## 5a. A process failure in this lane, recorded because burying it would be worse

**I deployed and ran while another lane (`lane-UIF`) held the game-directory
lock.** Reported to `main` so lane-UIF can re-verify anything measured around
23:00.

The mechanism is worth more than the apology. I invoked

```
tools/game_dir_lock.sh acquire lane-UI4 540 2>&1 | tail -3
```

`acquire` is correct — it `return 1`s on timeout (line 130) and prints *"Do NOT
deploy anyway -- that is the clobber this prevents"*. But **a pipeline's exit
status is the LAST command's**, so `| tail -3` replaced the lock's verdict with
`tail`'s unconditional 0. The backgrounded-task notification then said
"completed (exit code 0)" and I acted on that summary instead of reading the
output that said TIMEOUT in plain English.

Two things made it invisible:

* My **first** acquire genuinely succeeded, so the broken guard had already
  "worked" once — the same shape as any instrument whose first agreement is
  taken as evidence it can disagree.
* The script's own documented usage, `acquire lane-FOO 900 || exit 1`, is
  silently defeated by *any* pipe. The header teaches the guard and the guard
  is destroyed by a habit as ordinary as `| tail`.

**Rule for every lane: never pipe `acquire` into `head`/`tail`/`grep`.** Call it
bare, or `set -o pipefail`, or `if ! tools/game_dir_lock.sh acquire …; then
exit 1; fi`. A hardening worth making to the script: have `acquire` write a
sentinel the caller must read, so a discarded exit code cannot look like a
grant.

My own measurements are unaffected — I md5-verified the deployed exe against
my build immediately before each launch, and it was mine both times — but that
is luck about *my* numbers, not a defence of the process.

---

## 6. What is NOT closed, and exactly what would expire it

### `?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z` (0x18011d730)

The last of UI4's 8 vtable walls. **Not** blocked for lack of a caller or a
decompile — the body is 60 instructions and reads clearly: it fetches the
stream's `IO::Encoding` from `stream+0x10`, picks `vt+0x18` or `vt+0x20` by
`stream+0x08`, then drives four calls through the Encoding vtable at **+0xa0**
and one at **+0x78**, serialising the two Strings at +0x38/+0x40 and the
`SpriteInfo` array at +0x20.

**The blocker is those two vtable offsets.**
`reference_encoding_vtable_offsets_differ_between_builds.md` records that
`IO::Encoding`'s slots sit at different offsets in retail than in the profile
DLL (+0x78/+0x98/+0xa0 vs +0x90/+0xb0/+0xb8). Every other Encoding slot in this
tree got its name from the profile build's symbols, and that route cannot name
retail's +0x78 and +0xa0.

**WHAT WOULD EXPIRE IT**: a slot map for **retail's** `??_7Encoding@IO@@6B@`
naming +0x78 and +0xa0. That is an IO-family job with its own evidence trail
(`runtime/src/io/encoding.cpp`), not a SpriteSheet one. Once those two slots are
named, this body is a straightforward transcription with a write-path replay
oracle available (`reference_write_path_serialize_is_replayable.md`).

### `0x18004bde0` — `Array<CaptureEntry>::SetSize`, stride 0x28

This lane's one remaining **address door**. Not exported under any name, and
the same body shape as the fourteen `Array<T>::SetSize` specs already in
`runtime/src/collection/array.cpp`. Adding a `kSpec_*` row for it belongs to the
collection family's lane; it is recorded here so it is not lost.

---

## 7. Measurement

Census before (`tools/runtime_census.py --unported --namespace UI`), the
lane's three rows:

```
export  UI  ??_7SpriteSheet@UI@@6B@                              runtime/src/ui/sprite.cpp
export  UI  ?AddScissor@SpriteCapture@UI@@QEAAXHHHH_N@Z          runtime/src/ui/spritecontext.cpp
export  UI  ?AddSprite@SpriteCapture@UI@@QEAAXAEBVSprite@2@HHHHMI@Z  runtime/src/ui/spritecontext.cpp
```

Both sides measured with the **same tool revision**, from the **same worktree**,
with this lane's four new files physically moved aside for the "before" pass —
the scanners walk `os.walk` relative to the CURRENT DIRECTORY, so leaving them
in place would have measured them in both passes
(`reference_scanner_walks_cwd_not_script.md`).

The **set** difference is reported, not just the count
(`reference_diff_the_set_not_the_count.md` — a count that moves the right
direction by the wrong amount reads as success and hides what opened):

```
UI unported rows: 29 -> 28
DLL-wide:         called 1067 -> 1070   ported 831 -> 834

LEFT the unported set (closed by this lane):
  ?AddScissor@SpriteCapture@UI@@QEAAXHHHH_N@Z
  ?AddSprite@SpriteCapture@UI@@QEAAXAEBVSprite@2@HHHHMI@Z

ENTERED the unported set (opened by this lane):
  ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A
```

**Two of the three assigned rows are closed.** The count moved by only 1
because the lane opened one row, and the set diff is what names it.

### The row that entered, and why it is not a debt

`?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A` is newly *referenced*
because `?Type@` and `?TypeName@` read it — those two bodies are literally
`MOV EAX,[0x1801fadd0]` and `MOV RAX,[0x1801fadd8]`, so porting them cannot
avoid naming it.

It is **already minted**, exactly like `??_7SpriteSheet@UI@@6B@`:
`standalone/rt_data_mint_tables.inc` line 2030 carries the row (with both
`CreateNewObject`/`CreateInPlaceObject` factory hooks), and
`standalone/rt_data_mint.cpp:582` registers it under both doors. It is
invisible to the census for the same two reasons the vtable is — the
registration is spelled through a struct field rather than a literal, and the
mint is gated behind `BANISHED_RT_MINT=1`.

So the honest net is: **2 rows genuinely closed, 1 row that is a third instance
of the same census blind spot, and 7 of 8 real vtable walls filled.** Recording
it as "−2 +1" without that decomposition would misstate all three.

### Third row: `??_7SpriteSheet@UI@@6B@` — stays listed, is not open work

It appears in BOTH sets, unchanged. That is correct and expected: the data was
minted in Wave 9 and this lane changed nothing about the row itself. What the
lane changed is what the minted table *contains* — measured directly against
`rt_data_mint_tables.inc` and the port registry:

```
before:  slots=9  ported=1  TRAP=8
after:   slots=9  ported=8  TRAP=1      (the remaining trap is ?Serialize@)
```

That measurement is the one that matters for cutover, and it is not something
`runtime_census.py` can report.
