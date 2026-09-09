# Wave 10B lane ELEM — `??_7Element@UI@@6B@`, all eight trap slots, and the promotion

> **CLOSED 2026-09-09.** The row moved from
> `slots=12 ported= 3 TRAP= 8 purecall= 1` (baseline `7358732`) to
> `READY … slots=12 ported=11 TRAP= 0 purecall= 1  [PROMOTED]`, and it is the
> **first `??_7` vtable on `standalone/reg_mint_promoted.cpp`'s default-on list**.
> Two rows were promoted in total; four entered the census as new callees.

* Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a94a877cc2463fef2`
* Branch `wave10b/lane-elem`, based on `wave10/base` @ `453e26d`
* Code: `runtime/src/ui/element_vtable.cpp` (new), behind `BANISHED_RT_ELEMVTABLE`
* Evidence: `docs/runtime_port/wave10b/elem_evidence.txt` (extracted from four locked runs)

---

## 1. Per-slot table

Slots 0, 5 and 6 were already ours (wave 9 lane K; wave 7 lane A in `element.cpp`).
Slot 4 is `_purecall` in `Element` itself. The eight below are this lane's.

| slot | export | retail | ord | size | oracle | verdict |
|-----:|--------|--------|----:|-----:|--------|---------|
| 1 | `?TypeName@Element@UI@` | `0x180035440` | 4385 | 8 | replay | **ported, NOT exercised** (calls=0) |
| 2 | `?Type@Element@UI@` | `0x180035420` | 4178 | 7 | replay | **VERIFIED** — `calls=1 worked=1 divergences=0` |
| 3 | `?Persist@Element@UI@` | `0x180116870` | 3323 | 0x4b | pin | **ported, NOT differentially verified** |
| 7 | `?Update@Element@UI@` | `0x180116270` | 4489 | 0x5ec | pin | **ported, NOT differentially verified** |
| 8 | `?Enable@Element@UI@` | `0x180116910` | 2288 | 0xc8 | scratch (child array excluded) | **ported, NOT exercised** (calls=0) |
| 9 | `?Contains@Element@UI@` | `0x1801169e0` | 1862 | 0x101 | replay | **ported, NOT exercised** (calls=0) |
| 10 | `?SetColor@Element@UI@` | `0x180035620` | 3847 | 4 | scratch | **ported, NOT exercised** (calls=0) |
| 11 | `?GetBorder@Element@UI@` | `0x180035630` | 2468 | 0xe | replay | **ported, NOT exercised** (calls=0) |

All eight are registered under **both** doors (name and address), each mangled
name spelled as a literal at the registration so `runtime_census.py` can see it.

### What the four oracle words mean, and what each does not cover

* **replay** — the body is a pure read (or writes only a caller-owned return
  slot); the shipped export is re-run on the same live object and compared.
  Free of side effects; full coverage when it fires.
* **scratch** — the body mutates `this`; ours runs on the live object, the
  shipped export runs on a private 0xC0-byte copy of the pre-state, all 0xC0
  bytes compared. For `Enable` the copy additionally has its **child array
  emptied**, because otherwise the shipped body would recurse into the real
  children through their real vtables and run a second `Enable` on live objects
  whose derived overrides are not known to be idempotent. So `Enable`'s oracle
  covers the **flag arithmetic and not the six-instruction child loop**.
* **pin** — an FNV-1a-64 over the whole shipped body **in the loaded image**
  against the hash of the bytes the port was transcribed from. It answers *"is
  the code I read the code that runs?"* and nothing else. It is a pre-flight on
  the transcription, **not** a comparison against retail's behaviour. All eight
  ranges were checked against the base-relocation directory first — none
  contains a relocation, so the loaded bytes are the file bytes and the
  constants are stable under ASLR.
* **NOT exercised** — the body is live in the process (registered, and reachable
  through the now-promoted vtable) but no route in this lane's runs entered it,
  so its oracle reports `calls=0`. Stated as a coverage hole, not as a pass.

`Persist` and `Update` are `pin` because they **cannot be replayed**: `Persist`
opens and closes an encoding scope (a second run writes a second record on the
write path) and `Update` pushes UI events, moves tooltips and can call
`CloseDialog`. There is no post-state to compare that is not itself the side
effect.

---

## 2. The `Update` A/B

`?Update@Element@UI@` is the hot one — the DLL's own `UserInterface::Update`
reaches this slot for every element of every open dialog, every frame — so it
was A/B'd explicitly rather than merged with the rest.

**The A/B is the promotion itself**, because arming the vtable is the only thing
that makes any of these eight slots reachable. Control and armed differed in
exactly one variable (`BANISHED_RT_MINT_ROWS`); both had `BANISHED_RT_DIFF=1`.

| | R3 control | R4 armed |
|---|---|---|
| `[Shutdown] done` | yes | yes |
| `standalone_crash.log` | 0 bytes | 0 bytes |
| `[RtResolve]` misses | 0 | 0 |
| harness/MapProbe lines | 21 | 21 |
| `UI::Element::ctor` | `calls=1 divergences=0` | `calls=1 divergences=1` |
| `UI::Element::Type` | *(site absent)* | `calls=1 worked=1 divergences=0` |
| minted slot 0 at teardown | *(DLL's dtor)* | `RemoveChildren calls=1`, `Array<8>::SetSize calls=1`, `~Array calls=1` |
| divergence lines | 12 | 13 |

**The one extra divergence is the promotion.** It is
`UI::Element::ctor call #1 ours=+0x0 <exe .data> theirs=+0x0 <Runtime .rdata>`
— the vtable pointer, which the two ctors are *required* to write differently
once the row is promoted. The other twelve lines are byte-identical between the
runs and are the tree's existing named negative controls. `element.cpp`'s ctor
oracle now excludes `+0x00` with that argument written out, and checks the field
with its own explicitly-labelled self-check instead
(`_vtable != null && _vtable == ElementVTable()`), which still catches the two
ways that field actually breaks.

**`Update` itself was not entered in any run.** Its `calls=0` is honest: the one
Element constructed on this route is a `FrustumOutlineElement` that
`code/Game/Component/frustumoutlineelement.cpp` builds a **patched copy** of the
base table for, and it was not driven through a UI update with the mouse over
it. So `Update` is `ported, not differentially verified` twice over — no replay
oracle *and* no entry. That is the largest single thing this lane did not close.

### The route matters more than the run count (a trap this lane nearly fell into)

The first A/B was menu-only, and it was a **no-op**: the two logs were identical
apart from two `[RtMint] ROW ON` lines. Reading that as "safe, promote it" would
have promoted a table nothing reads — `reference_uncalled_port_is_not_verified`
in vtable form. The reason is structural: the DLL's derived element constructors
call `??0Element` by **rel32**, which no registration can redirect, so
DLL-constructed elements never see our table. The only readers are the two
**exe-side** subclasses (`FrustumOutlineElement`, `CaptureBoxElement`) that go
through the exe's import slot `0x1401518d8` → our port → `ElementVTable()`.
`BANISHED_TOOL_HARNESS=utility BANISHED_UTILITY_TEMPLATE=Template\UtilityMap.rsc`
is the route that builds one, and it was found by grepping `standalone/` rather
than by concluding "no route exists"
(`reference_grep_the_harness_before_writing_unverifiable`).

---

## 3. A retail dead branch, found and kept

Five flag updates across `Enable` and `Update` end with the same shape:

```
MOV  R8D,EAX          ; old flags
BTR  R8D,0x17         ; clear bit 23   (or BTS 0x11 / BTR 0x11 / BTS 0x14)
MOV  [RCX+0x34],R8D
XOR  R8D,EAX          ; what changed
TEST R8B,0x1          ; <-- bit ZERO
JZ   ...              ; skip the ancestor needs-size-calc walk
```

The XOR can only hold the one bit the `BTS`/`BTR` just touched — 23, 17 or 20,
never bit 0 — so **the ancestor dirty walk behind that test is unreachable in
the shipped build**. Read out of the loaded image (`0x18011693b: 41 f6 c0 01`),
not out of Ghidra's C. Ghidra's own decompiler agrees on one of the five copies,
emitting `WARNING: Removing unreachable block (ram,0x000180116390)` for
`Update`'s set-bit-17 arm while keeping the identical clear-bit-17 arm.

It is almost certainly a source bug — bit 0 **is** `kElementFlagHidden`, and
`Hide` / `Show` / `SetFlagsOnOff` do the same sequence with bit 0 and *do* reach
the walk — but the shipped behaviour is what a port owes. The condition is
written literally so it is evaluated and provably false, rather than deleted.

Consequence worth knowing: in retail, an element whose enabled-state or
hover-state changes is **not** marked needs-size-calc.

---

## 4. Also recovered: the interior of `UI::Event`

`runtime/types/ui/UserInterface.h` deliberately leaves the 0x30-byte `Event`
opaque and records only what lane UI1's two push sites showed. `Element::Update`
writes **eleven** records at nine distinct offsets and its frame slots line up
with UI1's, which gives the whole layout:

```
+0x00 dialog    +0x08 element    +0x10 eventId   +0x14 elementId
+0x18 mouseX    +0x1c mouseY     +0x20 dragId    +0x24 (never written)
+0x28 dragData
```

UI1's records are the same struct with `element=0` and `elementId=-1`. Evidenced
at the struct in `element_vtable.cpp`; the header is left alone rather than
edited from another lane's branch.

The event ids are **not** sequential and their order is **not** the state order
(`state+0x01 → 2, +0x02 → 4, +0x03 → 3, +0x04 → 5, +0x05 → 7, +0x06 → 6`).
Transcribed one at a time; `id = 2 + i` would silently swap two of them.

---

## 5. The census set-diff

Board at `453e26d`, `tools/runtime_census.py --unported --namespace UI` = **6 rows**.
After this lane = **9 rows**.

**LEFT (1)**

* `??_7Element@UI@@6B@` — promoted.

**ENTERED (4)** — all four are callees of the newly ported bodies, i.e. functions
retail itself calls from `Element::Update` / `Element::Contains`. This is
`reference_porting_can_grow_the_cutover_surface`: a faithful port grows the
`called` column, and that is the correct outcome, not a regression.

* `?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z` (from `Contains`)
* `?BeginToolTip@UserInterface@UI@@…` (from `Update`)
* `?EndHover@DialogInstance@UI@@…` (from `Update`)
* `?EndDragDrop@UserInterface@UI@@QEAAXXZ` (from `Update`)

`?Play@UserInterface@`, `?CloseDialog@UserInterface@` and
`?SetUpperLeft@DialogInstance@` are also called by `Update` and did **not**
enter — they are already ported by sibling lanes and are reached through them.

**ENTERED THEN LEFT AGAIN (1)**

* `?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A` — entered because `Type` and
  `TypeName` read it, and was then **promoted** as the eleventh `?_typeInfo@`
  row. It qualified on `reg_mint_promoted.cpp`'s own stated rule — the ten
  before it were promoted because they had live consumers, and this one now
  does. Measured end to end rather than by analogy: across the A/B,
  `standalone/reg_element_typeids.cpp` publishes into exe slot `0x140151660`
  `typeInfo=00007ffcab1cab30 id=2370443687` (control, DLL storage) →
  `typeInfo=00007ff7ac54d9d8 id=2370443687` (armed, our minted storage). The
  pointer moves, the id — the one field anybody reads — is byte-identical, and
  with the row armed `UI::Element::Type calls=1 worked=1 divergences=0`: our
  body read our storage and still matched the shipped export reading the DLL's.

Aggregate line, measured on this branch:
`UI  total 1625  called 175  ported 166  detour 5  94.9%`.
Do not diff that percentage against the wave-10 close (`170/164/1 = 96.5%`)
without re-measuring the base — `453e26d` is several merges past `730c544` and
the `called` and `detour` columns moved for reasons that are not this lane's.
**The row-level set-diff above is the claim; the percentage is not.**

---

## 6. Stated plainly: what is ported and not verified

1. **`?Update@Element@UI@`** — no replay oracle is possible, and no run entered
   it. Byte pin green, `calls=0`. The most valuable follow-up in this lane's
   scope is a harness route that puts the mouse over a live element built
   through our ctor and drives `DialogInstance::Update` across it.
2. **`?Persist@Element@UI@`** — no replay oracle possible (encoding scope);
   byte pin green; `calls=0`.
3. **`?TypeName@`, `?Enable@`, `?Contains@`, `?SetColor@`, `?GetBorder@`** — full
   oracles are written and would fire, but nothing entered them
   (`calls=0`). They are transcriptions plus a byte pin today.
4. **`?Enable@`'s child recursion** — deliberately outside its oracle's window,
   for the reason given in §1.
5. **The promotion's coverage** — only slots 0 and 2 were dispatched in the
   verifying run. A READY row's claim is "no abort trap in this table", which is
   what the shadow measures and the boot confirms; it is **not** a claim that
   all eleven bodies are differentially verified.

## 7. Builds

`mingw32-make clean && mingw32-make standalone` and
`mingw32-make clean && mingw32-make standalone-debug` both complete with exit 0
on this branch (the first `make standalone` after a debug build reports
"Nothing to be done" and proves nothing — the two targets share one object
directory, so a release check has to follow a `clean`).
