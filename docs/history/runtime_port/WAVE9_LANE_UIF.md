# Wave 9 lane UIF — the UI frame loop and widgets cluster

**Branch** `wave/upgrade-path-complete` · **Base** `d382e12` · **Date** 2026-09-08

Assigned: the 24 UI rows of cutover boot attempt 06's 61 unresolved symbols.
**Ported 3 of 24, plus two enabling registrations. Declined 21 with evidence.**

Nothing here was built or run — the integrator holds the game directory. Every
file was checked with `g++ -fsyntax-only` under the Makefile's
`standalone-debug` flags. Read the PREDICTION section before merging.

---

## 0. A correction to the brief, and it is the reason the lane found anything

The brief said to watch for "a null STAMPED INTO A LIVE VTABLE" because
derived-dialog vtable builders copy base slots. That shape is real and it is
here — but **not where the brief pointed, and not through `??1`.**

`??_7Element@UI@@6B@` **slot 0 is not the destructor `??1Element`.** It is the
*vector* deleting destructor `??_EElement@UI@@UEAAPEAXI@Z` at `0x180035c10`,
one of the 37 slot-0 bodies Wave 9 lane K ported. Read out of
`standalone/rt_data_mint_tables.inc`, every minted UI vtable's slot 0 is an
`RT_MINT_UNNAMED` row:

| minted vtable | slot 0 (`??_E`) | reaches `??1` at |
|---|---|---|
| `??_7Element@UI@@6B@` | `0x180035c10` | `0x1801161a0` |
| `??_7ElementDescription@UI@@6B@` | `0x180035350` | `0x180116100` |
| `??_7DialogInstance@UI@@6B@` | `0x1800458c0` | `0x180114d40` |

All three `??_E` bodies were disassembled for this lane and the edge confirmed
directly: `LEA R9,[0x1801161a0]` at `0x180035c28` and `CALL 0x1801161a0` at
`0x180035c61`; `LEA R9,[0x180114d40]` at `0x1800458d8` and `CALL 0x180114d40`
at `0x180045911`; `LEA R9,[0x180116100]` at `0x180035377` (this one's scalar
branch is **inlined**, which is lane K's documented "22 of 37" case and exactly
why lane K's port calls `~Class` on both branches).

Lane K reaches `~Class` **by address** through `ResolveAddr`. So with these
`??1` rows unported, lane K's already-ported slot 0 called into the DLL — and
after cutover, into null. **The three destructors are lane K's unfinished tail,
and that, not their size, is what made them this lane's priority.** Wave 7's
hold-back reason (a) for `??1Element`, "nothing in our tree calls it", was a
true statement about our *C++ call graph* and a misleading one about
reachability: the caller is a vtable slot.

---

## 1. A second correction, which was load-bearing for this lane's own ports

`runtime/src/ui/element.cpp` documented two resolution doors and said:

> `ResolveAddr(Module::Runtime, addr)` — **"always the DLL"**.

**That is false, and has been for the whole runtime port.** Settled from the
implementation rather than from either comment —
`standalone/resolve_standalone.cpp:471`:

```cpp
if (mod == Module::Runtime && RtPortEnabled()) {
    auto& reg = RtAddrReg();
    auto it = reg.find(ghidra_addr);
    if (it != reg.end()) { g_rtHits++; return ...it->second; }
```

The port registry is consulted **before** the module handle and before the ASLR
delta. `reference_resolveaddr_consults_port_registry_first.md` says so, and
lane K's `deleting_dtor.cpp` says so in its own words; element.cpp contradicted
both. Corrected in place with the reason spelled out, because believing the old
sentence would make `??1Element` look pointless: it reaches `RemoveChildren`,
`~Array` and `~Internal` through that door **precisely because** the door lands
on the ports. That is what closes the row instead of moving it one frame deeper.

---

## 2. The triage rule, applied

Lane Z's rule, and it decided everything below: *a port CLOSES a cutover row
only if every callee it needs is already satisfied on our side; otherwise it
MOVES the row one frame deeper.* Every one of the 24 was sized in Ghidra with
`disassemble_function(address=…, filter_mnemonics="CALL")` — never
`get_function_metrics` or `get_function_callees`, which silently ignore
`address=` and fall back to a name lookup
(`reference_ghidra_mcp_address_param_silently_ignored.md`; 185 functions in this
DLL are called `Serialize`).

Every callee address was then resolved against the shipped export table and
against our registered port set by a purpose-built script, so "satisfied" is a
query result, not a memory.

**All 24 mangled names were verified against the shipped export directory with
`pefile` BEFORE any body was written** — all 24 present. A name the DLL does not
contain resolves NULL in silence, the port still registers, and the census still
counts it (`reference_resolveexport_phantom_names.md`).

---

## 3. PORTED (3 rows + 2 enabling registrations)

### 3.1 `??1ElementDescription@UI@@UEAA@XZ` — 0x180116100, ord 585
*(`runtime/src/ui/dialog.cpp`)*

The cheapest row in the cluster. Thirteen instructions and a tail jump:
restamp the vtable, release the one `System::String` at `+0x60`, `JMP
~External`. Callees: `MemFree` (ported, lane Q) and
`??1External@Object@@UEAA@XZ` (ported, lane B2). **Two callees, both ours.**

`runtime/types/ui/ElementDescription.h` already stated, from Wave 7's
independent read of the same bytes, that this destructor "releases exactly one
String, the one at +0x60, before chaining". The port agreeing with it is a weak
check but a free one.

`~External` is reached by **name**, not address: `0x1800bc950` is exported, and
`UEAA` is the virtual spelling this destructor actually has — `QEAA` would
resolve null behind the guard
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

### 3.2 `??1Element@UI@@UEAA@XZ` — 0x1801161a0, ord 584
*(`runtime/src/ui/element.cpp`)*

Wave 7's two hold-back reasons were **re-read, not inherited**
(`reference_holdback_blockers_are_stale_check_them_first.md`):

* **(a) "nothing calls it"** — superseded by §0. The caller is a vtable slot.
* **(b) "most destructive function in the class, no oracle"** — *still entirely
  true*, and §5 does not pretend otherwise. What changed is that (a) turned this
  from an optional port into a cutover blocker. The Wave 2 verdict that an
  unverified destroying port is worse than a hold-back assumed a working DLL
  underneath. There is none after cutover; the alternative is a null call.

Callees, each checked individually:

| callee | what | state |
|---|---|---|
| `0x180115570` | `RemoveChildren` | PORTED — lane B3 (**see §3.5**) |
| slot 0 on `_border` | scalar deleting dtor | PORTED — lane K |
| `0x180037240` | `~Array<Element*>` on `+0xa0` | **PORTED BY THIS LANE**, §3.4 |
| `0x1800c7ad0` | `MemFree` | PORTED — lane Q |
| `0x1800bfdb0` | `~Internal` | PORTED — Wave 3 |

**Retail's doubled null test on `_border` is reproduced.** It emits both
`CMP qword ptr [RBX+0xb0],0x0` (0x1801161c5) and `TEST RCX,RCX` (0x1801161d6)
on the same loaded value. Redundant, and kept: tidying a test out of a
destructor is how a difference gets introduced that nothing then looks for.

**The tail call to `~Internal` is not optional.** `0x1800bfdb0` is the
`InternalFactory` swap-remove of
`reference_internalfactory_swapremove_wild_write.md`; an Element freed without
it leaves a stale handle-table entry a later swap-remove writes through.

### 3.3 `?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z` — 0x180040690, ord 1561
*(`runtime/src/ui/widgets_layout.cpp`)*

Wave 7 lane E's hold-back note had the mechanism right; what it lacked was
whether the append helper was ours yet. It is: `0x180023830`
(`Array<uint32>::SetSize(n, &fill)`) is lane Q's, and it is the *same*
instantiation `runtime/src/graphics/scene.cpp` already calls — independent
confirmation of the address. **One callee, and it is ours.**

The `MOVSXD` at `0x1800406a0` is reproduced as a **signed** widening.
Ghidra's decompiled C prints `MOVSXD` as a zero extension and that misreading
caused three Wave 6 defects including a crash
(`reference_ghidra_c_output_vs_disassembly.md`). A negative `graph` therefore
indexes backwards exactly as retail does, and is **not** guarded — retail has no
guard, and inventing one would hide a caller bug rather than fix it.

### 3.4 `0x180037240` — the TWELFTH J1 array destructor
*(`runtime/src/collection/array.cpp`)*

Lane D's survey found eleven addresses in the `Array<T>::~Array` family. There
are twelve. `0x180037240` is `Element::_children`'s, called at `0x1801161f7`;
it was invisible to the survey because nothing in our tree called it until
`??1Element` was ported.

**Same body, at byte level rather than by eye.** `0x180037240` and the
registered `0x180033960` were compared over all 115 bytes: they differ at
exactly eleven offsets (39,40, 60,61, 71,72, 76,77,78, 86,87), every one inside
a RIP-relative displacement or a `CALL rel32`, and every one resolving to the
same four absolute operands lane D's own J1 note pins (`0x1801fc718`,
`0x18014b390`, `0x1800c6c00`, `0x18014b388`). So lane D's "no parameters at
all, not even a stride" conclusion covers it unchanged and one body still serves
them all. Lane D's "eleven" wording was corrected in place rather than left to
rot.

### 3.5 A REAL HOLE, found by the audit and not by reading

`tools/port_bypass_audit.py` flagged the tree's **only** address-door BYPASS the
moment `??1Element` landed:

```
BYPASS  runtime/src/ui/element.cpp:1362
    0x180115570   ?RemoveChildren@Element@UI@@QEAAXXZ
```

**Lane B3 registered `RemoveChildren` under its NAME only.** So
`ResolveAddr(Module::Runtime, 0x180115570)` missed the address registry, fell
through to the DLL — and would have been null after cutover. Nothing exercised
it because `??1Element` is the first by-address caller in the tree.

Closed by adding the missing `RegisterRuntimeAddr(0x180115570, …)`. This is the
Wave-5-lane-D failure mode the "register TWICE, name AND address" convention
exists to prevent, and it says something worth keeping: **the convention is not
decoration, and a lane that skips half of it leaves a hole that only shows up
when someone else uses the other door.** Address-door BYPASS is now 0.

---

## 4. DECLINED (21 rows), with the unsatisfied callee that decides each

Declining is the correct result under the triage rule: each of these would move
its row one frame deeper, and several would move it *several* frames deeper.
`(int)` marks an unexported internal — reachable only by address, with no lane
owning it.

| # | row | unsatisfied callees | depth |
|---|---|---|---|
| 1 | `?Update@UserInterface@UI@@` | `CleanRemovedDialogs` 0x18011f790, `CloseDialog` 0x180120f20, `UpdateEvents` 0x18011fc60, `ComputeLayout@DialogInstance` 0x180113d90, `Update@DialogInstance` 0x180113980, `GetMouseCursorIndex` 0x18011fa40, 0x180033690 (int) | **7** |
| 2 | `?Draw@UserInterface@UI@@` | `Begin@SpriteContext` 0x18011d2b0, `Flush@SpriteContext` 0x18011ba40, `AddSpriteDraw` 0x18011c890, `EndAddingSprites@SpriteCapture` 0x1800d8a60, 0x1800783d0 (int) | **5** |
| 3 | `?ShowDialog@UserInterface@UI@@` | `Frame@MainLoop@Engine` 0x1800dd100, `~UserInterface` 0x180120b90, `EndDragDrop` 0x180120af0, **and `UserInterface::Update` itself** (row 1, +7) | 4 (+7) |
| 4 | `?CloseDialog@UserInterface@UI@@` | `UpdateEvents` 0x18011fc60, `EndDragDrop` 0x180120af0, 0x180033690 (int) | 3 |
| 5 | `?BringDialogToFront@UserInterface@UI@@` | 0x18007a010 (int, list unlink → tail-jumps 0x180009c30), 0x180032750 (int, 24-byte node alloc) | 2 |
| 6 | `??1DialogInstance@UI@@` | 0x1800325f0 (int, list free-all), 0x18002fca0 (int) | 2 (**see below**) |
| 7 | `?OnClosed@DialogInstance@UI@@` | `SaveSettings@DialogInstance` 0x180114240 | 1 |
| 8 | `?SetUpperLeft@DialogInstance@UI@@` | `AdjustUpperLeft@Element` 0x180115840 (×2) | 1 |
| 9 | `?Serialize@ElementDescription@UI@@` | 0x180038f90, 0x180076610, 0x1800042a0 (all int) | 3 |
| 10 | `?CreateInstance@ElementDescription@UI@@` | 0x180009460, 0x180039440, 0x180039360, 0x180039280, 0x180006b80 (all int) | 5 |
| 11 | `?CreateElement@Dialog@UI@@` | *(all five direct callees ARE ported)* — declined on the vcall, see below | — |
| 12 | `?AddLine@SpriteContext@UI@@` | `Flush` 0x18011ba40, `AddSpriteDraw` 0x18011c890 | 2 |
| 13 | `?SetScissor@SpriteContext@…AEBV34@Z` | `Flush` 0x18011ba40, `FlushDeferredFonts` 0x18011c6a0, `AddScissor@SpriteCapture` 0x1800d8ef0 | 3 |
| 14 | `?SetScissor@SpriteContext@…HHHH@Z` | forwards to row 13 — one `CALL 0x18011d050` and nothing else | 1 (+3) |
| 15 | `?AddItem@ComboElement@UI@@` | 0x180034860, 0x18004bfc0, 0x180024fe0 (all int) | 3 |
| 16 | `?SetCurrentItem@ComboElement@UI@@` | `CloseDialog` 0x180120f20 (row 4), `UpdateText@ComboElement` 0x180112180 | 2 (+3) |
| 17 | `?RemoveAllItems@ComboElement@UI@@` | 0x180037310, 0x180024aa0 (both int) | 2 |
| 18 | `?SetParameter@SliderElement@UI@@` | `CacheText@TextElement` 0x18011ec10 | 1 |
| 19 | `?SetProgressTextParameter@ProgressElement@UI@@` | `CacheText@TextElement` 0x18011ec10 | 1 |
| 20 | `?AddGraph@GraphElement@UI@@` | 0x1800417d0 (int) | 1 |
| 21 | `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` | the PRIVATE overload `?FindCheckBoxes@RadioElement@UI@@AEAAX…` 0x1800d5750, which itself needs 0x180048fe0 (int) | 2 |

### Four of these are worth a sentence each

**Row 6, `??1DialogInstance`, is the one genuinely close call and I declined
it.** Its two blockers are small (a linked-list free-all whose only real callee
is the ported `Free`, and a 24-byte-node helper). What stopped it is not depth:
the body **open-codes the `InternalFactory` swap-remove inline** at
`0x180114e71` — it reads the factory singleton at `0x1801fc6d8` and rewrites the
handle table itself, rather than delegating — and it stamps two further base
vtables (`0x180154720`, `0x180154788`) mid-destruction. That is
`reference_internalfactory_swapremove_wild_write.md`'s exact failure family
written out longhand, in a function with **no oracle**, and it is the one row
here where getting it wrong corrupts the handle table rather than crashing.
It wants its own lane with the mint's `_typeInfo` question (which lane Z already
filed as open) settled first.

**Row 11, `?CreateElement@Dialog`, is the near-miss.** Every one of its five
direct callees is ported (`GetPath`, `GetPathIdentifer`, `MemFree` ×3). It is
declined on the single `CALL qword ptr [RAX+0x40]` at `0x18011305d` — a virtual
dispatch on an `ElementDescription`-family vtable whose slot 8 I did not
identify, and `??_7ElementDescription@UI@@6B@` has only 9 slots with three of
them `RT_MINT_PURECALL`. Declining an unidentified vtable slot is cheaper than
guessing one. **This is the highest-value row left in the cluster** and it is
one disassembly away from a verdict.

**Rows 18 and 19 share a single blocker.** `CacheText@TextElement`
(`0x18011ec10`) is the only thing standing between both of them and admission,
and its own callees are three-quarters ported (`??BString@UI@@`,
`FormatParamList`, `MemFree` — only `SetParameterCount@TextElement`
`0x18011e8e0` is not). **Porting `CacheText` would close two rows at once**, and
it is the best next move in this cluster after row 11. Note that lane M's
Finding 1 was about `CacheText`'s refcounted string swap, so it must be a
no-oracle port and must NOT be prefix-replayed.

**Row 14 is the clearest illustration of the triage rule.** The `HHHH` overload
of `SetScissor` is a single forwarding `CALL` to the `AEBV34` overload. Porting
it is trivial and would have closed nothing at all — it would have relocated one
null by one instruction.

---

## 5. What I could not verify — stated plainly

* **The three ported bodies have NO ORACLE and I did not manufacture one.** For
  the two destructors a replay is not a comparison but a second destruction: it
  would destroy every child again through slot 0, decrement `String` refcounts
  our body may already have taken to zero and freed, free the child array twice,
  and run the handle swap-remove twice on a handle that no longer exists. For
  `AddGraphPoint` a replay appends the point a *second* time and reallocates.
  All three ship as **no-oracle ports** in the disposition lane M established
  and lane Z reused, with **no call into the shipped body at all**.
* **`RT_DIFF_WORK` is a liveness instrument, not a correctness one.** It reports
  how many entered calls actually released or appended something. Be aware that
  these three sites will print **`calls=0`**: `calls` is bumped only by
  `RtDiffReport`, and there is no comparison to report. **`worked=` is the whole
  signal.** A reader who checks `calls=` here and sees 0 will wrongly conclude
  the ports are dead.
* **Nothing was built or run.** `g++ -fsyntax-only` proves the four touched
  files compile; it proves nothing about behaviour. No `BANISHED_RT_DIFF=1` run,
  no cutover attempt, no `worked=` figure exists yet.
* **The `0x180037240` byte-identity argument is static.** It is strong (115
  bytes, eleven differences, all displacements resolving to the same four
  targets) but no run has yet executed our body at that address.
* **Row 11's vtable slot 8 is unidentified**, which is why it was declined
  rather than ported — recorded as an open question, not a closed one.

---

## 6. Audit numbers, before → after, with denominators

| audit | before | after |
|---|---|---|
| `rawdoor_audit.py` | **481** sites, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **481** sites, **0** UNGUARDED-CALL, 34 DEFERRED-CALL |
| `port_bypass_audit.py --by-name` (name door) | **408** sites; BYPASS=2 | **408** sites; BYPASS=**2** |
| `port_bypass_audit.py` (address door) | ADDR-REGISTERED=31, **BYPASS=1** | ADDR-REGISTERED=**32**, **BYPASS=0** |
| by-name cross-check | 608 ported names / 396 statements / **0** unseen | **611** ported names / 397 statements / **0** unseen |
| `rt_data_door_audit.py` | 3 RT_RAW_OK, **3** unclassified | 3 RT_RAW_OK, **3** unclassified (unchanged) |
| `runtime_census.py` UI | 1625 total / 146 called / **107** ported / 73.3% | 1625 total / 146 called / **110** ported / **75.3%** |
| `runtime_census.py` TOTAL | 4632 total / 846 called / **595** ported / 70.3% | 4632 total / **847** called / **599** ported / **70.7%** |

The two remaining name-door BYPASSes are pre-existing and are not this lane's:
both are in `RuntimeDllPresent` (`runtime/src/system/allocator.cpp:1157-1158`).
The three unclassified data doors are also pre-existing and unchanged.

`called` went 846 → 847 while `ported` went +4. That is expected and is
`reference_porting_can_grow_the_cutover_surface.md`: `??1Element` calls DLL
internals by address, so porting it *adds* to the called surface even as it
closes rows. **`called` is not a denominator counting down to zero.**

---

## 7. A falsifiable prediction for the next DLL-less boot

Stated so it can be *wrong*, not so it can be agreed with.

1. **The 61-symbol residue drops by exactly 3**, to 58 — `??1Element@UI@@`,
   `??1ElementDescription@UI@@` and `?AddGraphPoint@GraphElement@UI@@` will not
   appear. The other 21 UI rows **will still be there**; I did not port them and
   claim no effect on them.
2. **No new residue row appears from these three ports.** Every callee was
   checked to be satisfied, so none of the three should introduce a name or
   address the cutover cannot resolve. *If a new row appears attributable to
   them, the §2 triage was wrong and this is the falsifier.*
3. **The boot will NOT get further on account of this lane.** Attempt 06 died
   somewhere; unless it died in one of these three, the distance travelled is
   unchanged. Destructors run at teardown, and `AddGraphPoint` needs a graph
   widget on screen. **A boot that stops in the same place is consistent with
   this lane having worked**, and it would be wrong to read it as failure.
4. **Where the effect should actually show is teardown, not boot** — the first
   `delete` of a `UI::Element` or `ElementDescription` through minted slot 0.
   Before this lane that was an EXEC-of-0 inside lane K's ported deleting
   destructor. If a cutover run reaches teardown and *still* dies there with a
   null call, my §0 reading of the `??_E` → `??1` edge is wrong.
5. **`RemoveChildren` should stop appearing in any by-address residue** (§3.5).
   Its nine name-door call sites were never affected.

**The cheapest way to test 1–2 before a cutover attempt** is a normal
`BANISHED_RT_DIFF=1` run with the DLL present: the three sites should report
non-zero `worked=` and `calls=0`. If `worked=0` on all three, the ports are
registered but dead and prediction 1 is worthless — a port with no traffic is
not verified (`reference_uncalled_port_is_not_verified.md`).

---

## 8. Next moves in this cluster, in the order I would take them

1. **`?CreateElement@Dialog`** — identify vtable slot 8 (`CALL [RAX+0x40]` at
   `0x18011305d`). Every other callee is already ours. One disassembly from a
   verdict, and it is the highest-value row left.
2. **`CacheText@TextElement` (0x18011ec10)** — closes rows 18 and 19 together.
   Must be a no-oracle port; lane M's Finding 1 is about this exact function.
3. **`??1DialogInstance`** — its own lane, with the inline `InternalFactory`
   swap-remove and lane Z's open `_typeInfo` mint question settled first.
4. The `SpriteContext` internals (`Flush` 0x18011ba40, `AddSpriteDraw`
   0x18011c890) gate rows 2, 12, 13 and 14 — **four rows behind two functions**,
   the best leverage in the cluster after item 2.
