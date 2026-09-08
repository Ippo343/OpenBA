# Wave 9 lane UIC — `UI::UserInterface` / `DialogInstance` / `Dialog` / `ElementDescription`

**Branch** `worktree-agent-ab90a41f9fb5f334a` · **Base** `29cfaa2` · **Date** 2026-09-08

Assigned: the eleven rows of cutover boot **attempt 14**'s 43 unresolved symbols that
belong to those four classes (`docs/runtime_port/wave9_attempt14_unresolved.txt`).

**Ported 3 of the 11, plus 6 enabling bodies. Declined 8 with the unsatisfied callee
that decides each.** Built, deployed and run under the diff oracle with the DLL
present: **one port has a real oracle — 14 comparisons at 0 divergences, with a negative
control that takes it to 13 of 14 on demand; three more are live but have no oracle by
construction; four are registered and NEVER CALLED, and are therefore NOT VERIFIED.**
The numbers and what they do and do not show are §6.

Everything here follows directly from [WAVE9_LANE_UIF.md](WAVE9_LANE_UIF.md), which
sized all eleven of these rows in Ghidra and declined all eleven with a depth table.
That table was the work order; this lane is its §8 "next moves", executed.

---

## 1. What was ported

| # | row | retail | ord | oracle | live? |
|---|---|---|---|---|---|
| 1 | `?CreateElement@Dialog@UI@@…` | 0x180112ed0 | 1888 | replay | **NO — 0 calls** |
| 2 | `?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z` | 0x180114a60 | 3997 | none (by construction) | yes, `worked=1` |
| 3 | `?BringDialogToFront@UserInterface@UI@@…` | 0x180120440 | 1672 | none (by construction) | **NO — 0 calls** |
| E1 | `?AdjustUpperLeft@Element@UI@@QEAAXHH@Z` | 0x180115840 | 1596 | **invertibility + 14-case sweep, negative control 13/14** | yes, **calls=14, div=0** |
| E2 | `Collection::LinkedList::Remove` | 0x18007a010 | — | none | **NO — 0 calls** |
| E3 | `Collection::LinkedList::NewNode` | 0x180032750 | — | none | **NO — 0 calls** |
| E4 | allocator-lock `Free` wrapper | 0x180009c30 | — | delegation | yes, `worked=4..8` |
| E5 | `Array<T>::~Array` — **13th address** | 0x180009460 | — | byte identity | no caller yet |
| E6 | `Array<T>::~Array` — **14th address** | 0x180039440 | — | byte identity | no caller yet |

Rows 1-3 are cutover rows. E1-E6 are enabling work: without them rows 1-3 would only
have MOVED their row one frame deeper, which is lane Z's triage rule and the reason
lane UIF declined them in the first place.

All three exported names were verified present in the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with a `pefile`-style reader (`tools/uic_probe.py`)
**before any body was written** — a name the DLL does not contain resolves NULL in
silence, the port still registers, and the census still counts it
(`reference_resolveexport_phantom_names.md`). E2/E3/E4 are **not exported at all**, so
they are registered by ADDRESS only and there is nothing there for
`tools/export_name_audit.py` to check.

Files touched: `runtime/src/ui/element.cpp`, `runtime/src/ui/dialog.cpp`,
`runtime/src/ui/userinterface.cpp`, `runtime/src/collection/array.cpp`, and one new file
`runtime/src/collection/linkedlist.cpp`.

---

## 2. The two rows lane UIF left "one disassembly from a verdict"

### 2.1 `?CreateElement@Dialog@` — slot 8 identified, out of a table

Lane UIF declined this row on a single `CALL qword ptr [RAX+0x40]` at `0x18011305d`:
*"a virtual dispatch on an `ElementDescription`-family vtable whose slot 8 I did not
identify"*. It is identified, and not by guessing from a name:
`standalone/rt_data_mint_tables.inc`'s `??_7ElementDescription@UI@@6B@` (`kVSlots20`,
9 slots, minted from the DLL's own vtable) has **index 8 — byte offset 0x40 —
as `RT_MINT_PURECALL` at `0x18007ec28`**.

So slot 8 is **pure virtual on the base** and is only ever reached on a concrete
derived description's vtable. That settles the row in the direction of admission for a
reason worth stating explicitly: **porting this body introduces no new cutover symbol.**
Retail's own body dispatches through the same derived slot. Whether that slot is
satisfied is the description class's problem, and it is exactly as much of a problem
with the port as without it.

Its five direct callees are all ours: `?GetPath@External@Object@@` (0x180006250,
ported, registered under both doors at `runtime/src/object/external.cpp:739/763`),
`?GetPathIdentifer@Filename@System@@` (0x1800c6610, `runtime/src/system/filename.cpp:450/477`),
and `MemFree` (0x1800c7ad0, three call sites, lane E).

**The inline strcmp is transcribed, not summarised**, and that is the one part of this
body worth being nervous about. `reference_uniform_population_is_not_a_verification.md`
is about precisely this construct: a ported inline `strcmp` that returned 0 for every
distinct pair sharing a first byte survived four waves of oracles because thirteen of
the fourteen values in its population were the same value. Retail's loop
(0x180112fe8..0x180113021) is guarded by a length compare on `String`'s length dword at
`[p-8]` and forms its result as `(signed char)ours[i] - (signed char)theirs[i]`; it is
written out instruction for instruction rather than replaced by `std::strcmp`.

**And this port's population is currently ZERO.** See §6.3 — that is the honest state
of this row, and it is the single biggest gap in the lane.

### 2.2 `?SetUpperLeft@DialogInstance@` — and the enabling port under it

Lane UIF's row 8: blocked, depth 1, on `?AdjustUpperLeft@Element@UI@@` (0x180115840).
That function turns out to be the cheapest thing in the cluster — **thirty-two
instructions and not one call other than the recursion into itself.** Porting it took
the row.

Three details in `SetUpperLeft` that a tidier rewrite gets wrong, all reproduced:

* **The virtual at `0x180114ae8` is Element slot 4 = `ComputeSize`**, which
  `runtime/types/ui/Element.h` already names and which `kVSlots19` confirms is
  `RT_MINT_PURECALL` on the base — the same "pure, so no new symbol" situation as §2.1.
* **The clamp order.** `x` is floored at 0 FIRST (`CMOVS` at 0x180114b8a) and the
  overflow test `w + x > screenW` then uses the ALREADY-FLOORED `x` (`LEA` at
  0x180114b90). A symmetric two-sided clamp gives a different answer for any dialog
  wider than the screen.
* **The "fill screen" branch (`_flags & 0x30`) reaches the UI scale through a DIFFERENT
  ROUTE** from the top of the function: it walks the `_parent` chain (+0x98) to the root
  and reads *that* element's `_dialogParent` (+0x90) `->[+0x10][+0xdc]`, rather than
  `self->_ui->[+0xdc]`. Normally the same float. Collapsing the two would be an
  assumption, so both are written as retail writes them.

`runtime/types/ui/Element.h` independently documents the same `root->_dialogParent
[+0x10][+0xdc]` idiom from `?GetPaddedSize@`'s side. That is corroboration of the
offset, not the evidence for it.

---

## 3. `?BringDialogToFront@` and the `Collection::LinkedList` helpers

Lane UIF's row 5: depth 2, on `0x18007a010` (list unlink, "tail-jumps 0x180009c30") and
`0x180032750` (24-byte node alloc). Both are ported here, and both turned out to be
smaller than their depth suggested.

**`0x180032750` never reads RCX**, which matters because `BringDialogToFront` calls it
*without setting RCX at all* — RCX arrives holding whatever the preceding
`CALL 0x18007a010` left there. That is settled by reading the callee
(`reference_read_the_callee_before_holding_back_on_an_arg.md`): the only mention of RCX
in the whole body is `XOR ECX,ECX`, which destroys it before any use. A port that read
a first parameter there would be reading garbage and would look correct for exactly as
long as the garbage happened to be harmless.

**`0x180009c30` IS `MemFree` with an ignored leading argument**, and that is an
instruction-level claim. Both bodies are
`if (p) { Enter(*_singleton + 0x640); Free(_singleton, p); Leave(...); }`; the only
differences are which register carries the pointer (RCX at 0x1800c7ad0, RDX here) and
one extra `MOV [RSP+0x8],RCX` that homes the first argument and never reads it back. It
is implemented as exactly that, so ONE body serves the pair rather than a second copy
that can drift. Likewise `0x180032750`'s allocation is `MemAlloc(0x18, 8)` spelled
inline, so it goes through the one ported allocator entry point — which matters at
cutover, because `0x1800c7a50`/`0x1800c7ad0`/`0x1800c6e60`/`0x1800c6c00` are the four
addresses `RegisterAllocatorAddresses()` arms (`309c015`), and reaching the heap any
other way from here would re-open the wall attempt 13 closed.

### 3.1 A hole this closed that was not in my brief

`0x180009c30` was an **unregistered address door with two existing readers**:
`runtime/src/graphics/scene.cpp:239` (`RtFreeLocked`) and
`runtime/src/input/input.cpp:139` (`RtInput_KeyMap_ClearAll`'s `freeNode`). Both reach
it through `ResolveAddr(Module::Runtime, 0x180009c30)`, which before this lane fell
through to the DLL and after cutover would have been a null call inside
`Input::KeyMap::ClearAll`.

It is now registered, and it is the ONLY one of my nine bodies with a healthy live
population in every cell (`worked=4..8`). It has its own kill switch
(`BANISHED_RT_LINKEDLIST=0`) separate from `BANISHED_RT_COLLECTION`, precisely because
registering it changes behaviour in the graphics and input paths, which are not this
lane's.

---

## 4. The thirteenth and fourteenth `Array<T>::~Array`

The integrator's unregistered-address-door census asked for a verdict on four by-address
doors, one of which was `?AddController@DialogInstance@` (0x180113470). Reading it
produced a finding that belongs to lane Q rather than to me:

**Two of the five "unexported internals" `WAVE9_LANE_UIF.md` row 10 declined
`?CreateInstance@ElementDescription@` on are not opaque internals. They are
`Array<T>::~Array`.** `0x180009460` and `0x180039440` are the J1 body, byte for byte.

Lane D's survey found eleven; lane UIF found a twelfth and wrote "there are twelve".
There are at least fourteen, and lane UIF's own mechanism explains why: **a J1 address is
invisible to a caller-driven survey until something in our tree calls it.** These two
surfaced from the opposite direction — from reading a caller nobody has ported.

`tools/uic_bytediff.py` compares all 115 bytes against the registered `0x180033960` and
resolves every differing byte back to the absolute operand it encodes:

| candidate | bytes | differ | outside a displacement | operands |
|---|---|---|---|---|
| `0x180009460` | 115 | 10 (at 40,41 61,62 72,73 77,78 87,88) | **0** | all 5 identical |
| `0x180039440` | 115 | 11 (at 39,40 60,61 71,72 76,77,78 86,87) | **0** | all 5 identical |

`0x180039440`'s diff-set is byte-for-byte the one lane UIF reports for `0x180037240`,
which is corroboration that the reference and the method are both unchanged.

### 4.1 The measurement nearly went the other way, and that is worth recording

The FIRST version of `tools/uic_bytediff.py` carried a **hardcoded offset table** for the
five relocatable operands. One entry was one byte off, so it decoded the wrong four
bytes and printed

```
*** MISMATCH *** RIP-rel CALL [EnterCriticalSection] -> 0x1917d29b4
```

— a confident, specific, wrong refutation, **for the reference body, which is registered
and known good.** Both candidates were declared "NOT THE SAME BODY". Locating the fields
by instruction pattern instead produced the table above.
`reference_recheck_must_match_its_own_proof.md` is about a check whose own construction
makes its verdict meaningless; this is the sibling case, a check whose own *decode* is
wrong and which then reads as a finding about the subject. A tool that cannot decode its
own reference correctly must fail loudly on the reference, not report on the candidates.

The other two internals in that group (`0x180039360`, `0x180039280`) are the **J2**
variant — the same free preceded by a per-element destroy helper (`0x180039640` /
`0x180039580`), which are themselves unported. They are NOT taken here.

---

## 5. Verdict on the integrator's four by-address doors

Asked for explicitly, so answered explicitly. **All four ARE exported** (checked with
`tools/uic_probe.py` against the shipped export table), and **none of the four is ported
by this lane.**

| address | export | verdict |
|---|---|---|
| `0x180114870` | `??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@…@Object@@PEAVBase@5@PEAXH@Z` | **NOT TAKEN.** See below. |
| `0x180113470` | `?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z` | **NOT TAKEN** — 13 calls, 5 internals |
| `0x1801133b0` | `?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@…@4@V?$InternalPtr@VElement@UI@@@4@@Z` | **NOT TAKEN** — tail of 0x180113470 |
| `0x1800da130` | `?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@0@Z` | **NOT TAKEN** — `TabElement` is lane E's widget file, not this cluster |

**The integrator's reading of the ctor is right and I am not taking it anyway.** It is a
genuine hole with the exact shape of the current wall: a `DialogInstance` built through a
null `fn_DialogInstance_Ctor` is an all-zero object whose first virtual call executes 0,
which is `??0PackageFile@IO@@` at `LoadPackages` in different clothes. But it is not a
one-function job. `0x180114870` has 7 calls:

```
0x1800bfd10  Object::Internal::Internal          PORTED (Wave 3)
CALL [RAX+0x40]  root description slot 8          pure virtual — free, see §2.1
0x1800031f0  ?GetSpriteIndexFromName@SpriteSheet@ exported, UNPORTED
Enter/0x1800c6e60/Leave                           = MemAlloc, armed at cutover
0x180113470  ?AddController@ (6-arg)              exported, UNPORTED  <-- the real cost
```

and `0x180113470` in turn needs `0x1800c98a0 StrCompare` (exported, and ALREADY an
unsatisfied door from `runtime/src/ui/element.cpp:213`), `0x180006b80` (unexported, its
own lock+Free), the two **J2** array destructors of §4, and `0x1801133b0`. That is a
four-to-six function sub-lane, not a slot in this one. Sized rather than waved at, so
whoever takes it starts from this list.

**One thing NOT to do with these four**, because it looks like progress and is not:
converting the `engine_funcs.cpp` call sites from `ResolveAddr` to `ResolveExport` closes
nothing. `ResolveExport` falls through to the raw implementation for an unported symbol,
so a door conversion on a symbol with no port and no mint entry changes only which
residue list it prints in — still a null call at cutover
(`reference_converting_a_door_is_not_satisfying_it.md`; 14 of 36 such conversions were in
that state and "0 UNGUARDED-CALL" did not mean otherwise).

---

## 6. Verification — what was measured, and what was not

### 6.1 Static accounting: the unmoved denominator plus a set-diff

`tools/unsatisfied_export_scan.py`, before and after, on the same tree:

|  | before | after |
|---|---|---|
| distinct names resolved through a door | **789** | **789** |
| of those, neither a port nor a mint entry | 149 | **146** |
| of those, at least one site outside a diff guard | 146 | **143** |

Set-diff of the two lists — **exactly three closed, zero new**:

```
CLOSED  ?BringDialogToFront@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@
CLOSED  ?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@
CLOSED  ?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z
NEW     (none)
```

The denominator not moving is the point: it says the three rows were closed rather than
relocated. `?AdjustUpperLeft@` never appears because nothing in our tree resolved it by
name before; the three LinkedList helpers are unexported and invisible to this scan by
construction. Both facts are stated so the "+3" is not read as the whole effect.

### 6.2 The other audits

| audit | result |
|---|---|
| `port_bypass_audit.py` (address door) | 214 sites, **BYPASS=0**, ADDR-REGISTERED=34 |
| `port_bypass_audit.py --by-name` | 430 sites, **BYPASS=0**; 674 ported names / 418 statements / **0 unseen** |
| `rawdoor_audit.py` | 503 sites, **0 UNGUARDED-CALL**, 37 DEFERRED-CALL |
| `rawdoor_audit.py` **after merging the integration tip** | 521 sites, **1 UNGUARDED-CALL**, 38 DEFERRED — and it is NOT mine: `runtime/src/io/package_file.cpp:426`, which arrived with lane PKG2's `271ebb5`. Flagged rather than left for the integrator to find. |
| `abi_retslot_audit.py` | **0 suspicious** of 48 by-value-returning exports |
| `export_name_audit.py` | 662 names, 1 not in the DLL — `"?_singleton@..."`, a doc-comment placeholder in `runtime/funcs/rt_resolve_cache.cpp:8`, pre-existing and not a real name |
| `runtime_census.py` UI | 151 called / 110 ported / 72.8% → **152 / 114 / 75.0%** |
| `runtime_census.py` TOTAL | 958 called / 665 ported / 69.4% → **961 / 672 / 69.9%** |

Census numbers are **post-`29cfaa2`** (the fifth-blind-spot fix), taken before and after
on the same tree with the same script. `called` grew 958 → 961 while `ported` grew +7:
that is expected and is `reference_porting_can_grow_the_cutover_surface.md` — my bodies
reach `0x180115840`, `0x18007a010` and `0x180032750` by address, so porting *adds* to the
called surface even as it closes rows. **`called` is not a denominator counting down.**

### 6.3 The live diff run — and the four ports it did NOT verify

Five cells, DLL present, `BANISHED_RT_DIFF=1`, each on an exe whose size was verified to
match my build immediately before launch (six lanes share one game directory;
`tools/uic_run.sh` closes the deploy/launch race that cost two earlier attempts).

Final cell (`uicE`, `BANISHED_AUTOTUTORIAL=1 BANISHED_PAUSEMENU=6000
BANISHED_PAUSEMENU_QUIT=600`, 115 s, 411 diff sites, clean exit, no crash log):

```
[RtDiff]   UI::Element::AdjustUpperLeft                     calls=14   worked=1   divergences=0
[RtDiff]   UI::DialogInstance::SetUpperLeft                 calls=0    worked=1   divergences=0
[RtDiff]   Collection::FreeLocked(0x180009c30)              calls=0    worked=4   divergences=0
```

and **that is the whole list.** `?CreateElement@Dialog@`,
`?BringDialogToFront@UserInterface@`, `LinkedList::Remove` and `LinkedList::NewNode` do
not appear at all — `RT_DIFF_BEGIN` registers a site on first entry, so an absent site
means **zero calls**. Four of my nine bodies are registered and were never executed.
They are **NOT VERIFIED** (`reference_uncalled_port_is_not_verified.md`), and no amount
of green in the rest of the table changes that.

Why, concretely — their callers are narrow, and this is routing information rather than
an excuse:

* `?CreateElement@Dialog@` — four call sites, all in `code/Game/Tutorial/`
  (`tutorialimage.cpp:196`, `tutorialimagehotkey.cpp:224`, `tutorialimageinputkey.cpp:212`,
  `tutorialimagetoolbaricon.cpp:240`), reached through dispatch slot `0x1401528c0`. The
  tutorial DID load in cells C-E (`[AutoStart] AUTOTUTORIAL sequence fired`) but no image
  step ran inside the window.
* `?BringDialogToFront@` — ONE caller in our tree, `code/Game/Dialog/toolui.cpp:540`.
  A tool UI, not the front-end. Nothing in the autostart or tutorial harness opens it.
* `LinkedList::Remove` / `NewNode` — reachable only through `BringDialogToFront`.

### 6.4 `?AdjustUpperLeft@` — the one row with a real oracle

A destructor-shaped function has no replay, but this one is **exactly invertible**, which
buys a genuinely two-sided check:

```
snapshot the subtree
OURS(dx, dy)
THEIRS(-dx, -dy)         <- the shipped body, undoing it
compare against the snapshot          <- the verification
THEIRS(dx, dy)           <- apply for real, so the CALLER gets retail's result
```

This is strictly stronger than a field-by-field replay of one node: a node ours **fails
to visit** is left at -(dx,dy) by the undo, a node ours **visits twice** is left at
+(dx,dy), and a swapped dx/dy shows on any case with dx != dy. It checks the visit
**multiplicity over the whole subtree**, which is the part of this function that can
actually be wrong.

**One live call was not a verification.** Cells B, C and D each produced exactly ONE
call — a 115-second tutorial run yields one dialog move. One comparison is the population
`reference_uniform_population_is_not_a_verification.md` warns about, and the fix is not a
longer run, it is a CALLER (`reference_const_reader_needs_a_caller_not_a_human.md`). So
each live call now carries a **sweep of 14 (dx,dy) cases** — both signs on each axis, one
axis at a time (which is what catches a swapped delta), asymmetric pairs, a large pair,
`(0,0)`, and the caller's own delta last. Every case is self-cancelling, so the sweep
costs the game state nothing.

Two instrument details that were got wrong first and are worth carrying:

* **One `RtDiffReport` per case, not one for the sweep.** The first version aggregated,
  and printed `calls=1` for fourteen comparisons — which makes a sweep that ran
  indistinguishable from a sweep that was skipped. `calls=14` is now the literal number
  of invertibility checks performed.
* **An effectiveness control.** `worked=` counts cases that moved something, so a
  population of `(0,0)`s cannot read as a pass.

**Result: `calls=14  divergences=0`, on a real live element subtree of 80 nodes.**

### 6.4.1 The negative control — and the hole it found IN THE ORACLE

`BANISHED_RT_ADJUSTUL_NEGCTL=<mode>` injects a defect of a shape a real port would
plausibly have: `noborder` (skip the border chain — the single most likely mistake,
because the border is a LOOP at the bottom of the body rather than an obvious recursive
call), `nochild`, `swap`, `once`.

**The first control run came back RED, and with the WRONG NUMBER, which is the more
useful of the two facts.** `noborder` should have failed all thirteen non-zero cases.
It failed exactly **ten**:

```
[RtDiff] DIVERGENCE UI::Element::AdjustUpperLeft  call #2
         ours=ours(1,0) then theirs(-1,0) over 80 nodes: 3 off
[RtDiff]   UI::Element::AdjustUpperLeft   calls=14  worked=1  divergences=10
```

The three non-zero cases that passed were `(0,-1)`, `(-1,-1)` and `(-1,1)` — **the exact
inverses of cases that ran earlier in the sweep.** A case that leaves residue leaves it
for the NEXT case, whose comparison is against the same original snapshot, so a later
case can CANCEL an earlier case's error and report clean. That is a defect inside the
ORACLE, not inside the port: the sweep claimed its cases were independent and they were
not.

Fixed by re-snapshotting before each case and writing the snapshot back after it
(`AdjustRestore`), which costs n integer stores. Re-run under the game-directory lock,
both cells in one acquisition, each gated on an identity line only this build emits:

| cell | env | result |
|---|---|---|
| `uicNEG2` | `BANISHED_RT_ADJUSTUL_NEGCTL=noborder` | `calls=14 worked=1 **divergences=13**` |
| `uicOK2` | (none) | `calls=14 worked=1 **divergences=0**` |

**13 of 14, and the one that passes is `(0,0)`** — vacuous by design and labelled as
such. The oracle is now shown capable of failing, on demand, on a defect of the right
shape, and the clean result is a measurement rather than a claim.

Worth stating as a rule rather than an anecdote: **the negative control's VALUE was not
that it went red. It was that it went red by the wrong amount.** A control that had
simply reported "divergences > 0, good" would have licensed a sweep with an unsound
cancellation hole in it. Counting what the control SHOULD have caught, and noticing it
caught three fewer, is what found the bug.

### 6.4.2 The identity gate

Six lanes share one game directory and an advisory lock only binds lanes that call it.
An on-disk md5 says which exe is on disk when you look, not which one ran. The gate used
here is a line **only this build can emit**: the diff site `UI::Element::AdjustUpperLeft`
does not exist in any other lane's binary, so its presence in a run's OWN log proves the
binary. `tools/uic_locked_run.sh` discards a run whose log lacks it rather than
interpreting it. This matters most for the negative control: **had it come back green,
the first hypothesis would have to be "I measured someone else's binary", not "my
control is broken"** — and without an identity line those are indistinguishable.

### 6.5 What I could not verify — stated plainly

* **`?CreateElement@`'s replay oracle has never executed** (§6.3). Its inline strcmp is
  the single riskiest construct in the lane and it is currently unexercised.
  Mounted `.pkm` mods give the named-template scan a population of distinct names for
  the first time and are the cheapest way to fix this — requested from the integrator.
* **`?SetUpperLeft@` has no oracle and cannot get one.** Its own guard is `delta != 0`,
  so a second call with the same (x,y) finds delta == 0 and does nothing: a replay would
  compare "the dialog is where we put it" against itself and report green for a port that
  put it in the wrong place. Same false-pass shape as `Audio::Scene::Pause`'s early
  return. What carries verification for it is the enabling port underneath it — every
  translate it performs goes through `?AdjustUpperLeft@`, which IS checked.
* **`?BringDialogToFront@` has no oracle for the same reason** — its first test is
  "already frontmost", so a replay finds the dialog at the tail and does nothing.
* **The two new J1 addresses have no caller.** Their evidence is byte identity (§4), not
  execution, and that is stated at the registration site too.
* **Nothing here was run with the DLL absent.** Six lanes share one game directory and
  the integrator owns the cutover attempts; the ⛔ in my brief was respected.

### 6.6 A live divergence that is NOT mine — and MY FIRST READING OF IT WAS WRONG

Every cell shows exactly one non-zero divergence, lane OBJ2's:

```
[RtDiff] DIVERGENCE Object::Factory::Startup [probe, not diffable]  call #1
         ours=chain=416 nodes=415 badCrc=0   theirs=chain=416 nodes=416 badCrc=0
```

I reported this to the integrator as "retail's tree has 416 nodes and ours loses an
insert". **That is wrong, and the way it is wrong is worth more than the observation.**

**The `theirs=` side of that line is not a measurement of retail.** The site runs no DLL
body at all — it is a self-check on our own result — and the second string is built as
`snprintf(b, "chain=%d nodes=%d badCrc=0", chain, chain)`. The `416` on the "theirs" side
is just `chain` printed twice: **the line's own assumption restated in the column where a
reader expects independent evidence.** The correct reading is only that our tree has 415
nodes for a 416-entry chain, and the report says nothing whatever about retail.

That is `reference_recheck_must_match_its_own_proof.md` — an expected side that is true
by construction — and it is the SAME CLASS as two other things this lane hit:

* my own sweep reporting fourteen comparisons as `calls=1` (§6.4), and
* `RT_DIFF_SHADOW` emitting its strings only on a mismatch, so a population description
  carried inside it is invisible on exactly the runs where someone would want it (§2.1).

All three are one rule: **if a control is only visible when the test fails, or its
expected side is computed from its actual side, it is not a control.** The integrator has
since fixed the string to read `EXPECTED (no DLL run): nodes==chain==N` so it cannot be
misread again, and added a key-collision reporter that distinguishes a CRC collision from
a lost insert — the underlying 415-vs-416 question is real and still open.

Recorded here with my wrong reading intact rather than quietly corrected, because the
instrument earned the misreading and the next person will make it too.

---

## 7. DECLINED (8 rows), with the unsatisfied callee that decides each

Lane UIF's depth table, re-checked at `29cfaa2` rather than inherited
(`reference_holdback_blockers_are_stale_check_them_first.md`). `(int)` marks an
unexported internal with no lane.

| row | unsatisfied callees | depth |
|---|---|---|
| `?Update@UserInterface@UI@@` | `CleanRemovedDialogs` 0x18011f790, `CloseDialog` 0x180120f20, `UpdateEvents` 0x18011fc60, `ComputeLayout@DialogInstance` 0x180113d90, `Update@DialogInstance` 0x180113980, `GetMouseCursorIndex` 0x18011fa40, 0x180033690 (int) | **7** |
| `?Draw@UserInterface@UI@@` | `Begin@SpriteContext` 0x18011d2b0, `Flush@SpriteContext` 0x18011ba40, `AddSpriteDraw` 0x18011c890, `EndAddingSprites@SpriteCapture` 0x1800d8a60, 0x1800783d0 (int) | **5** |
| `?ShowDialog@UserInterface@UI@@` | `Frame@MainLoop@Engine` 0x1800dd100, `~UserInterface` 0x180120b90, `EndDragDrop` 0x180120af0, **and `UserInterface::Update` itself** | 4 (+7) |
| `?CloseDialog@UserInterface@UI@@` | `UpdateEvents` 0x18011fc60, `EndDragDrop` 0x180120af0, 0x180033690 (int) | 3 |
| `??1DialogInstance@UI@@` | 0x1800325f0 (int), 0x18002fca0 (int) — **and see below** | 2 |
| `?OnClosed@DialogInstance@UI@@` | `SaveSettings@DialogInstance` 0x180114240 — **and see below** | 1 |
| `?Serialize@ElementDescription@UI@@` | 0x180038f90, 0x180076610, 0x1800042a0 (all int) — **and see below** | 3 |
| `?CreateInstance@ElementDescription@UI@@` | 0x180009460 ✔, 0x180039440 ✔, 0x180039360 (int, J2), 0x180039280 (int, J2), 0x180006b80 (int) | 5 → **3** |

### Four of these are worth more than a row

**`?OnClosed@DialogInstance@` is depth 1 and I still declined it — the depth number is
misleading here.** Its one blocker, `?SaveSettings@DialogInstance@` (0x180114240), is not
a small helper. It is ~100 instructions that build a `MemoryStream` (0x1800ae710),
dispatch through the **`IO::Encoding` vtable at +0x18/+0x20** and a serializer's
+0x98/+0xa0, drive every controller's vtable slots 2/10, and end by open-coding an
allocator Free under the `0x1801fc718 + 0x640` critical section. That is the
save-path hazard family filed under WAVE9.md rows 8 and 25, in a function with no
oracle, and `reference_encoding_vtable_offsets_differ_between_builds.md` is a live trap
in it. A silent save desync is this project's most-cited failure mode. It wants a lane.

**`?Serialize@ElementDescription@` is NOT the replayable write-path Serialize.**
`reference_write_path_serialize_is_replayable.md` says a Serialize on the WRITE path is a
pure read of the object and replays into two independent sinks. That argument does not
transfer here: this one is reached on the **READ** path, loading an `ElementDescription`
out of a `.rsc` resource stream, so it *writes* the object and *consumes* the stream. A
replay reads a second record from a stream that has moved. Twelve of its seventeen calls
are `IO::Encoding` vtable slots (+0x18/+0x20/+0x98/+0xa0) and three are unexported
internals. Declined on both counts, and the "no oracle possible" claim is qualified: on
the write path it would be replayable, and no write path reaches it.

**`?CreateInstance@ElementDescription@` moved from depth 5 to depth 3** as a side effect
of §4 — two of its five blockers are now ported. The three left are the two **J2** array
destructors (which need their per-element destroy helpers `0x180039640` / `0x180039580`)
and `0x180006b80`. It is the **best next move in this cluster**, and porting the J2 pair
would close it and unblock `?AddController@` at the same time.

**`??1DialogInstance@` stays declined for the reason lane UIF gave, which I re-read and
agree with.** Its two named blockers are small, but the body **open-codes the
`InternalFactory` swap-remove inline** at 0x180114e71 — reading the factory singleton at
`0x1801fc6d8` and rewriting the handle table itself — and stamps two further base vtables
mid-destruction. `reference_internalfactory_swapremove_wild_write.md` is that exact
failure family, in a function with no oracle, and it is the one row here where being
wrong corrupts the handle table rather than crashing.

Note for whoever takes the dtor: `??1DialogInstance@UI@@` is spelled **`UEAA`**, the
virtual form, and on the `??_7DialogInstance@UI@@6B@` mint **slot 0 is the `??_E` scalar
deleting destructor at 0x1800458c0, not the `??1` at 0x180114d40** — lane UIF confirmed
that edge directly (`LEA R9,[0x180114d40]` at 0x1800458d8, `CALL 0x180114d40` at
0x180045911). `QEAA` would resolve null behind the guard
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

---

## 8. A falsifiable prediction for the next DLL-less boot

Stated so it can be wrong.

1. **The 43-symbol residue drops by exactly 3**, to 40 — `?CreateElement@Dialog@UI@@`,
   `?SetUpperLeft@DialogInstance@UI@@` and `?BringDialogToFront@UserInterface@UI@@` will
   not appear. **The other eight UI rows WILL still be there**; I did not port them and
   claim no effect on them.
2. **No new residue row appears attributable to these ports.** Every callee was checked
   satisfied, and the two virtual dispatches (`ElementDescription` slot 8,
   `Element` slot 4) are pure-on-the-base and therefore not symbols of mine. *If a new
   row appears from these, §2's triage was wrong and this is the falsifier.*
3. **The boot will NOT get further on account of this lane.** Attempt 14 dies in
   `IO::PackageFile`, long before any dialog exists. A boot that stops in exactly the
   same place is consistent with this lane having worked.
4. **Where the effect should show is `Input::KeyMap::ClearAll`** (§3.1), not in the
   dialog code at all — that was an EXEC-of-0 waiting to happen at teardown and is the
   one thing here on a path the boot actually reaches.
5. **`?CreateElement@` may still be dead at cutover** exactly as it is under the DLL. A
   symbol that resolves is not a symbol that runs, and I have no evidence it runs.

## 9. Next moves in this cluster, in the order I would take them

1. **Get `?CreateElement@` a population** — mounted mods, or a tutorial cell that
   actually reaches an image step. Its strcmp is the riskiest code I wrote.
2. **The J2 array-destructor pair** (`0x180039360`, `0x180039280`, plus
   `0x180039640` / `0x180039580`) — closes `?CreateInstance@ElementDescription@` AND
   unblocks `?AddController@DialogInstance@`, which is the gate on the ctor the
   integrator flagged. Best leverage left.
3. **`SaveSettings@DialogInstance` (0x180114240)** as its own lane, with the Encoding
   offsets from `runtime/src/io/rt_io_port.cpp`'s warning block in hand. Closes
   `?OnClosed@`.
4. **The `SpriteContext` internals** (`Flush` 0x18011ba40, `AddSpriteDraw` 0x18011c890) —
   lane UIF's count still stands: four rows behind two functions, including my
   `?Draw@UserInterface@`.
