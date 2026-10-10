# Wave 9 — lane IOA: IO/settings, the Object factory, and the Audio/Input device tick

Scope: 13 named symbols plus 2 ordinal-only rows from cutover boot attempt 06's
61 unresolved symbols. **4 rows closed, 9 declined with per-row evidence, both
ordinals identified.** Nothing was built or run — the integrator holds the game
directory — so every claim below is from the shipped DLL's bytes, its export and
import directories, and the tree's own audit tools.

---

## 1. The two ordinal-only rows

`ord:3214` and `ord:4462` are consecutive `[RtResidue]` lines in every residue
capture and were carried in WAVE9_LANE_B.md §"Unresolved" as *"the 4 ordinals
have no export name and were not identified"*.

**Both have export names, and this tree has been writing them down since before
Wave 9 started.** One `pefile` read of the export directory:

```
3214  ?Lock@Device@Video@@QEAAXXZ
4462  ?Unlock@Device@Video@@QEAAXXZ
```

and `engine/funcs/engine_funcs.h:148` / `:153` already carry exactly those two
comments above the typedefs, as does `dx11/src/buffers.cpp:105`. The residue
line says `ord:` only because the two call sites — `engine_funcs.cpp:1639` and
`dx11/src/buffers.cpp:156` — resolve them with `ResolveOrdinal(3214)` /
`ResolveOrdinal(4462)`, and the residue recorder prints whatever the door was
given. **`ord:` in a residue list means "resolved by ordinal", not "unidentified
symbol"**, and reading it as the latter cost lane B a row it could have closed
by grepping its own tree. (The other two, for the record: `ord:2990` =
`?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z`, `ord:443` =
`??0Thread@System@@QEAA@I@Z`.)

These are Video rows, not this lane's, and both are handed on unported.

---

## 2. Ported — 4 rows

Every mangled name below was checked against the shipped DLL's export directory
with `pefile` **before** a line of body was written, and every body was written
from the DISASSEMBLY, not Ghidra's decompiled C.

| row | retail | ord | new file |
|---|---|---|---|
| `?EnableMouseClipping@Device@Input@@QEAAX_N@Z` | 0x1800E1780 | 2296 | `runtime/src/input/input_win.cpp` |
| `?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z` | 0x1800DE520 | 4481 | `runtime/src/audio/audio_device.cpp` |
| `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` | 0x1800DE6F0 | 2380 | `runtime/src/audio/audio_device.cpp` |
| `?Load@KeyMap@Input@@QEAAXXZ` | 0x1800A7190 | 3203 | `runtime/src/input/keymap_load.cpp` |

Plus one unexported helper, `Input::Device::PlatformUpdate` (0x1800E2130),
registered by address only.

### 2.1 `EnableMouseClipping` — the stated blocker was wrong

WAVE8_LANE_D.md held this back with: *"porting the whole thing needs the same
unresolved internal helper `Update` does."*

`PlatformUpdate` was never disassembled. Its whole body is 0xB8 bytes and it
contains **no DirectInput anything, no COM vtable call, and not one `CALL
<rva>`**. Its four calls are USER32 imports of the DLL, named from its own
import directory: `GetClientRect` (0x18014B558), `ClientToScreen` (0x18014B530,
twice) and `ClipCursor` (0x18014B528). Its one Runtime data read is
`Application::Application::_singleton` (0x1801FC678), **minted since lane C2**,
whose `+0x00` this tree already documents as the HWND
(`runtime/types/video/Device.h:180`).

So `EnableMouseClipping`'s callee set is satisfied and the row closes. Lane D's
hold-back on `Update` itself stands unchanged: three of its four helpers
(`PlatformGetKeyboardState` 0x1800E1C60, `UpdateState` 0x1800E0FC0,
`PlatformGetMouseState` 0x1800E1E10) really are unexported DirectInput. This was
the fourth.

**New field pinned**: `Input::Device::_hasFocus` at +0x11F0, the second half of
`PlatformUpdate`'s guard. Found by a byte-pattern search for `C6 ?? F0 11 00 00
??` over the whole image — exactly four writers and no others, and no
non-immediate writer at all: the ctor (0x1800E1436), `PlatformConstruct`
(0x1800E1832), and the two that name it, `OnLostFocusMessage` (0x1800E17DB, →0)
and `OnFocusMessage` (0x1800E180B, →1), each of which then re-runs
`PlatformUpdate`. Recorded in `runtime/types/input/Input.h`.

### 2.2 The two Audio device rows — an ORACLE verdict had been read as a PORT verdict

WAVE8_5.md §9 and `audio.cpp`'s own header hold these back as *"Device
internals… here the device IS the subject, not an unreachable callee, so no
synthetic Scene helps"* and *"PRIVATE member, COM vtable call at
(**channel)+0x90, critical section at Device+0xA50."*

**Every fact in both paragraphs is true and this lane confirms all of them.**
What they establish is that these two have **no oracle** — which is a different
question from the one a cutover row asks. Sized in Ghidra, the callee sets are:

* `Device::Update` → `FreeChannelPlatform` (this lane) and an indirect `CALL
  RAX` through the stored completion callback. **No Runtime callee at all.**
* `FreeChannelPlatform` → `EnterCriticalSection`/`LeaveCriticalSection`
  (KERNEL32 imports, linked directly) and one virtual call through slot +0x90 of
  the platform voice. **No Runtime callee at all.**

The stored completion callback that WAVE8_5.md correctly identified as the single
root cause of all six Audio hold-backs is a *faithful indirect call*: it is a
reason there is no oracle, and it is not a dependency on the DLL.

**Layout cross-confirmed by a third function.** The channel array's base and
stride are not inferred from `Update`'s loop: `Audio::Device::Stop`
(0x1800DE200) takes an `AudioChannel*` and converts it to an index with `SUB
RDX,RCX ; MOV RAX,0x6666666666666667 ; IMUL RDX ; SAR RDX,4` — division by 0x28
— which independently pins **both** that the array starts at Device+0x00 and
that its stride is 0x28. `Stop` then runs the identical five-instruction reap
epilogue (0x1800DE23F..0x1800DE268) that `Update` runs at
0x1800DE560..0x1800DE57B. The platform-side offsets (+0xA88 gate, +0xAA0 +
idx*0x20 voice array) are cross-confirmed by `Device::StopPlatform`
(0x1800DEA20), which opens with the same gate and indexes the same array,
calling slot +0xA0 where `FreeChannelPlatform` calls +0x90.

**One shipped-DLL hazard reproduced rather than tidied**: `FreeChannelPlatform`
does NOT reuse the "needs the lock" verdict it computes on entry. It re-reads
`channel->source` at 0x1800DE761 and re-tests byte +0x21 of it, so a virtual
call that changed either changes whether the lock is released. Ghidra's C folds
the two reads into one. The port keeps them separate.

### 2.3 `KeyMap::Load` — closed because every callee is *ported*, not merely exported

> **Narrowed after the coordinator's base-commit message (see §8).** The
> sentence "every callee is satisfied" was too strong and is corrected in
> §2.3.1 below. The five callees *retail's body* has are all satisfied, which is
> what makes the row closable; our body reaches one more door than retail's
> does.


Filtered to `CALL`, `Load`'s call list is five distinct targets:

```
0x1800C9730  ?Format@String@System@@SA?AV12@PEBDZZ      PORTED (string.cpp:1118)
0x1800C6C00  ?Free@MemoryAllocator@System@@QEAAXPEAX@Z  PORTED (allocator.cpp)
0x18007F034  XOR EDX,EDX ; LEA R8D,[RDX+0xA] ; JMP …    == strtol(s,NULL,10). CRT.
[0x18014B390] / [0x18014B388]                           Enter/LeaveCriticalSection
```

The `IO::Persistence` lookup is **inlined** in retail — there is no call to
`Map_Find` (0x18002ACE0) — so writing that walk out is both the faithful thing
and the thing that keeps the body off the DLL. Calling `Map_Find` by address,
which `persistence.cpp`'s own `Get`/`Set` do, would have left the row open.

The inlined walk's node layout (key +0x00, value +0x08, left +0x18, right +0x20)
**agrees with `runtime/types/io/Persistence.h`**, which pinned it in Wave 4 lane
C from `Map_Find`/`Map_Insert` — an independent cross-check. The walk is
deliberately kept LOCAL and is not shared with `input.cpp`'s
`Collection::Set<Binding>` helpers, whose nodes put left/right at +0x20/+0x28:
one shared find over two layouts is the exact defect
`reference_one_bst_find_two_node_layouts.md` records.

Two details worth flagging because both are easy to flatten by accident:

* The comparison ends `MOVSX ECX,… ; MOVSX EDX,… ; SUB EDX,ECX` — **signed**
  char subtraction. `std::strcmp` is specified as unsigned. They agree on ASCII
  key names and diverge at the first byte ≥ 0x80, so the signed spelling is
  written out.
* The value sentinel is **three-way**, not two: `-2` (key absent, or
  `Binding::exclusive == 0` → write nothing, keep what `RegisterInput`
  installed), `-1` (stored as explicitly unbound → device = 0xFF, inputId = 0),
  `>= 0` (device = value>>8, inputId = value&0xFF). The sentinel is re-armed at
  the top of every *slot* iteration (0x1800A7200), not once per binding.

The key is `"Input" + (description + suffix)` where suffix is `""` for slot 0 and
`"Alt"` for slot 1 — two nested `String::Format("%s%s", …)` calls, so the
"Input" prefix lands *before* the suffix, not around it. Literals read out of
.rdata at 0x1801551F0/F4/F8 and 0x180155200.

#### 2.3.1 The door our body adds that retail's does not

Our body builds and releases `System::String`s through the tree's ported helpers
rather than through inlined assembly. Both `RtSystem_String_Format` and
`RtSystem::RtString_ReleaseInline` end in `RtMemFree`
(`runtime/src/system/string.cpp`), which resolves with
`RtOriginal<MemFreeFn>("?MemFree@MemoryAllocator@System@@SAXPEAX@Z")`.
**`RtOriginal` is the RAW door** — it calls `ResolveExportRaw`, which skips the
port registry by design. `MemFree` *is* ported (`allocator.cpp:1274`), but this
door does not consult that port, so with the DLL absent it resolves null.

This is a **tree-wide row, not a `KeyMap::Load` one**. Every landed String-using
port on the boot path goes through the same door — including
`IO::Persistence::Get(int&)`/`Get(bool&)`, which call the identical
`RtString_ReleaseInline`. It is `string.cpp`'s deliberate allocator seam
("strings cross the DLL boundary constantly, so both sides must use the same
heap") and belongs to the allocator lane.

So the precise claim is: **`?Load@KeyMap@Input@@QEAAXXZ` stops being a null
symbol**, and the boot can still stop one frame deeper inside our own String
release — at a door it would have reached through `Persistence` anyway. The same
narrowing applies to nothing else in this lane: the other three ported bodies
allocate nothing and touch no `System::String`.

Noted in passing, not this lane's to fix: `port_bypass_audit.py --all` reports
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` as *"not ported; raw == non-raw
today"* while `allocator.cpp:1272` registers it. That is the
gated-registration blind spot `reference_census_counted_gated_and_missed_dx11.md`
records, showing up in a second tool.

---

## 3. Declined — 9 rows, with what each is actually blocked on

Callee sets were produced by `disassemble_function(address=…, filter_mnemonics="CALL")`
and every target resolved against the export directory with `pefile`.
**"Exported" and "satisfied on our side" are different properties and the table
keeps them apart** — a port that calls an exported-but-unported symbol by name
moves its row one frame deeper instead of closing it
(`reference_converting_a_door_is_not_satisfying_it.md`).

| row | retail | size | blocker, verified this lane |
|---|---|---|---|
| `FindOrLoad@ExternalFactory` | 0x1800BF320 | 0x39E | **13 callees, ALL EXPORTED — zero unexported.** But 6 are UNPORTED: `Add`, `Load@External`, `ExpandRelativePath`, `GetPathExtension`, `GetPathDirectory`, `operator+`. Porting it MOVES the row, it does not close it. |
| `Clean@ExternalFactory` | 0x1800BE310 | 0x250 | Holds. 0x18002DFF0 and 0x18002CF60 are genuinely unexported (the AVL walk/erase). Its third non-CRT callee, 0x18007EEFC, is `_stricmp`. |
| `LoadResources@UpdateLoop` | 0x1800DD7D0 | 0x54F | Downstream of `FindOrLoad`, which it calls three times. Its own only unexported callee is 0x18008A290 = `memcpy`. Same lane, after. |
| `SaveText@External` | 0x1800BB9D0 | 0x253 | Two unexported callees. One, 0x180006A10, is the **unfolded second copy of `MemFree`** `compressedstream.cpp:313` already proved byte-identical to the exported one — so effectively **one** real blocker, 0x18000C180. Closest of the remaining Object rows. |
| `ReadSettings@Persistence` | 0x1800B84B0 | 0x420 | `Map_Insert` (0x18002A090) and 0x18000F7D0, both unexported. 0x180095060 is `__chkstk`. |
| `WriteSettings@Persistence` | 0x1800B88E0 | 0x815 | `Map_Find` (0x18002ACE0), unexported. Largest body in the cluster. |
| `~SaveLoad@Save` | 0x1800C3C90 | 0x93 | Small, but calls 0x1800C3F40 — an unexported per-element `SavedState` destructor that is **not** the exported `??1SavedState` (0x1800C0EA0; the two bodies differ), already noted at `saveload.cpp:33`. |
| `Update@Device@Input` | 0x1800E1490 | 0x105 | Holds, minus one. Three unexported DirectInput helpers remain (0x1800E1C60, 0x1800E0FC0 ~0x3E7 bytes, 0x1800E1E10). The fourth, `PlatformUpdate`, is ported by this lane. |
| `Update@Scene@Audio` | 0x1800DF6C0 | 0x717 | Holds. One unexported callee (0x180050D30) **and** the `System::Random` blocker WAVE8_5.md states: it calls `Random::NextGroup` (0x1800C7DD0), so a replay draws twice by construction. |

### The stale-blocker scoreboard

Re-checking every stated blocker, as instructed, changed **three** verdicts:

1. **`EnableMouseClipping` — WRONG.** The helper it shares with `Update` has no
   DirectInput dependency at all. §2.1.
2. **The two Audio device rows — MISCLASSIFIED.** Every fact was right; an
   oracle verdict was being applied to a port question. §2.2.
3. **The two ordinals — STALE.** Named in this tree's own headers all along. §1.

And it **confirmed** the brief's finding 2 while sharpening it: `FindOrLoad` is
not blocked on a ported `Collection::Set`, its AVL insert really is behind the
exported `ExternalFactory::Add`, and the decline is about size — but the *right*
reason to decline it is not size either. It is that `Add` and five siblings are
**exported and unported**, so a port would relocate the null rather than remove
it.

**One brief premise did NOT survive.** Finding 1 says *"ReadSettings/WriteSettings
touch the `IO::Encoding` vtable"* and warns that retail's offsets (+0x78/0x98/0xA0)
differ from the profile build's. That warning is real and is recorded elsewhere in
the tree — but **neither function touches `IO::Encoding` at all.** Their full call
lists (§3) contain no vtable call of any kind; they build a stack `IO::Registry`
and talk to ADVAPI32 (`RegEnumValueW`, `RegQueryValueExW`, `RegSetValueExW`,
`RegDeleteKeyW`, `RegCloseKey`). Their real blocker is the unexported
`Map_Find`/`Map_Insert` pair. The Encoding warning appears to have been attached
to these two rows by association with `IO::Persistence`.

---

## 4. Audits — before and after, with denominators

| audit | before | after |
|---|---|---|
| `rawdoor_audit.py` | 481 sites, **0 UNGUARDED-CALL**, 34 DEFERRED-CALL | 481 sites, **0 UNGUARDED-CALL**, 34 DEFERRED-CALL |
| `port_bypass_audit.py --by-name` (ADDRESS door) | 192 sites; ADDR-REGISTERED=31 DETOURED=3 NO-EXPORT=81 UNPORTED=77 | 192 sites; ADDR-REGISTERED=**32** DETOURED=3 NO-EXPORT=81 UNPORTED=**76** |
| `port_bypass_audit.py --by-name` (NAME door) | 407 sites, **BYPASS=2** | 407 sites, **BYPASS=2** |
| by-name cross-check | 608 ported names, 396 statements, 0 missed | **612** ported names, 396 statements, 0 missed |
| `rt_data_door_audit.py` | PASS; 3 RT_RAW_OK, **3 unclassified** | PASS; 3 RT_RAW_OK, **4 unclassified** |
| `runtime_census.py` | 846 called / **595** ported / 70.3% | 846 called / **599** ported / **70.8%** |
| `unsatisfied_export_scan.py` | 717 names through a door, **136** unsatisfied (**133** outside a diff guard) | 717 names, **132** unsatisfied (**129** outside a diff guard) |
| census — Input | 22 called / 16 ported / 72.7% | 22 called / **18** / **81.8%** |
| census — Audio | 21 called / 17 ported / 81.0% | 21 called / **19** / **90.5%** |

The unsatisfied count falls by exactly 4 and the four names that disappear are
this lane's four rows. `called` is unmoved at 846, as expected: nothing here
reaches a DLL internal by address (`reference_porting_can_grow_the_cutover_surface.md`
is about the case where it does).

### The one number that got worse, and why it is a tool defect

`rt_data_door_audit.py`'s UNCLASSIFIED list goes **3 → 4**. The new row is
`runtime/src/input/input_win.cpp:127`, the `RT_DATA_SLOT(void**,
"?_singleton@Application@1@0PEAV11@EA")` read of a **minted** symbol — the same
construct, on the same class of symbol, as the pre-existing
`runtime/src/audio/audio.cpp:880` row this tool has flagged since Wave 8.5.

It is not a raw door. The classifier's `CALL_BEFORE` regex requires the mangled
literal to be the **first** argument of the door call, and `RT_DATA_SLOT(T,
"name")` puts it second, so every such read falls through to "(not a call)" and
is then kept unless the symbol happens to be seen as a first argument somewhere
else in the tree. (That is why `keymap_load.cpp`'s identical
`RT_DATA_SLOT(void**, "?_singleton@Persistence@IO@@0PEAV12@EA")` is *not*
listed: the Persistence singleton is explained by another site, the Application
one is not.) The fix belongs in the tool, in `CALL_BEFORE` / the pass-2 pattern.

**No `RT_RAW_OK` marker was added to silence it.** That marker means
"oracle-only read that dies with the oracle", which would be false here.

---

## 5. Oracles — what each ported row has, and what it does not

None of the four is call-level replayable, and this lane did not manufacture an
oracle for any of them. `RT_DIFF_WORK` carries an **effectiveness control** at
every site instead, because `calls=` counts calls ENTERED and all four are
entered constantly while doing nothing:

| row | replay is | work verdict |
|---|---|---|
| `EnableMouseClipping` / `PlatformUpdate` | a second `ClipCursor` — global desktop state, a repeat not a comparison | the flag actually changed / the clipping branch was taken |
| `Device::Update` | a second firing of every stored completion callback | at least one channel had the reap bit set |
| `FreeChannelPlatform` | releasing an already-released COM voice | the platform was up (`Device+0xA88 != 0`) |
| `KeyMap::Load` | idempotent — theirs would overwrite ours and **always agree**: a control that cannot fail | at least one binding slot was written |

**A run whose work column is zero has not verified these bodies.** For
`KeyMap::Load` that is the common case: on a machine with no saved key bindings
every lookup misses, every slot keeps its `-2` sentinel, and the body writes
nothing while `calls=` still reads 1.

**The one oracle that IS possible, written down rather than half-built.**
`KeyMap::Load`'s subject *is* restorable — 2+2 bytes per binding node and
nothing else in the object is touched — so the real oracle is: snapshot every
`(device[slot], inputId[slot])` pair, run ours, restore, run the DLL's, compare.
This lane did not build it because it could not run the game to prove the
restore is exact, and an unproven restore harness is precisely the failure mode
`reference_longjmp_unwind_runs_retail_cleanup.md` records — a harness that
manufactures its own divergence. It is the next step for whoever holds the game
directory.

---

## 6. Falsifiable prediction for the next DLL-less boot

**Attempt 07 will not stop on any of these four symbols**, and it will stop
*later than* attempt 06 only if attempt 06 stopped on one of them.

More precisely, and each of these is falsifiable on its own:

1. `?EnableMouseClipping@Device@Input@@QEAAX_N@Z`,
   `?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z`,
   `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` and
   `?Load@KeyMap@Input@@QEAAXXZ` will be **absent** from the residue list and
   from any null-call crash. (Refuted if any appears.)
2. `EnableMouseClipping` will fault on the `*(HWND*)app` deref at
   `input_win.cpp` **if and only if** our `Application` was constructed without
   its window handle at +0x00. The deref is unguarded because retail's is
   (CLAUDE.md's rule); if it faults, the upstream fact to fix is the missing
   HWND, not this line.
3. `?Update@Device@Input@@…` will still be a null call, and the boot will reach
   it *before* it reaches `EnableMouseClipping`, because `Update` tail-calls
   `PlatformUpdate` every frame while `EnableMouseClipping` runs only on a
   settings change. **`Update` is the higher-value next pickup in this cluster**,
   and it now needs three helpers rather than four.
4. `?Load@KeyMap@Input@@QEAAXXZ` will not appear as a null symbol, **but the
   boot may still stop inside our String release** at
   `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` reached through
   `string.cpp`'s raw `RtOriginal` door (§2.3.1). If it does, the crash will
   name `RtMemFree`, not `KeyMap::Load`, and the fix belongs in `string.cpp`'s
   allocator seam. It will happen through `IO::Persistence::Get` first if
   anything reads a setting before the key map loads.
5. `?FindOrLoad@ExternalFactory@Object@@…` will still be a null call, and
   porting it alone will NOT remove that — it will replace one null with up to
   six (`Add`, `Load@External`, `ExpandRelativePath`, `GetPathExtension`,
   `GetPathDirectory`, `operator+`). Refuted if a `FindOrLoad` port alone
   advances the boot.

---

## 7. What this lane could NOT verify

* **Nothing was compiled into the exe and nothing was run.** Verification is
  `g++ -fsyntax-only` with the Makefile's `standalone-debug` flags on all six
  touched files (all clean), plus an object-level symbol check that
  `keymap_load.cpp`'s undefined `_ZN8RtSystem22RtSystem_String_FormatEPNS_6StringEPKcz`
  is exactly the symbol `string.cpp` defines. **No ported body here has ever
  executed.**
* **No `calls=` number exists for any of the four.** Every "did work" claim in
  §5 is a description of instrumentation that has not yet produced a number.
* `Binding::exclusive == 0` skipping a slot entirely (§2.3) is what the branch
  at 0x1800A7206 does; **why** the engine would decline to restore a
  non-exclusive binding from the registry is not explained by anything this lane
  read.
* The `Audio::Device` platform voice's vtable slot +0x90 is called faithfully;
  **what it is** (Release? Destroy?) is not established — only that
  `StopPlatform` calls +0xA0 on the same object and `FreeChannelPlatform` calls
  +0x90 and then nulls the slot.
* Offsets +0x04, +0x1C of `AudioChannel` and everything in `Audio::Device`
  outside {channels, +0xA50, +0xA88, +0xAA0+idx*0x20} are unmodelled gaps, not
  padding that has been proven empty.

---

## 8. Base-commit check (coordinator's message, after the first report)

**Checked, and this lane hit the same trap the other two did.** The worktree was
created at `965361d` — three waves stale, no `docs/runtime_port/` directory at
all and an old `CLAUDE.md`. It was caught in this lane's first four tool calls
(`ls docs/runtime_port/` failed) and `git reset --hard wave/upgrade-path-complete`
was run **before any analysis**, landing on `d382e12`. Every fact in this
document was read from that tree. `git merge-base --is-ancestor d382e12 HEAD`
confirms it; the lane commit sits directly on `d382e12`.

The three landed findings, checked against this lane's work:

1. **Allocator arms with the DLL absent** — no target here was held back on
   allocator grounds, so nothing changes. It does bear on §2.3.1: the remaining
   String-release exposure is `string.cpp`'s **raw door**, which skips the port
   registry, so an armed allocator does not by itself close it.
2. **`ResolveAddr(Module::Runtime, …)` consults the port registry FIRST** —
   this lane wrote no comment claiming otherwise and calls `ResolveAddr` zero
   times. The one comment that could have been *read* that way
   (`input_win.cpp`) has been rewritten to state the mechanism explicitly: the
   address door reaches the DLL exactly when the address is UNREGISTERED.
3. **Register by name AND address** — all four rows are registered both ways
   (`input_win.cpp:180/182`, `keymap_load.cpp:279/280`,
   `audio_device.cpp:282/284` and `:286/288`). `Input::Device::PlatformUpdate`
   is address-only because it has **no mangled export in either the retail or
   the profile build** — verified against the export directory with `pefile`,
   not assumed. That is the correct handling of a genuinely unexported helper,
   not lane B3's defect.

**What the re-check changed in this lane's own conclusions**: §2.3's claim that
"every callee is satisfied" for `KeyMap::Load` was too strong. Corrected in
§2.3.1 and in prediction 4 of §6. The row still closes as a symbol; the
narrowing is about what the boot can still stop on one frame deeper. Nothing in
§1, §2.1, §2.2, §3 or §4 changed — those rest on DLL bytes and on audit runs
made against `d382e12`.

Also noted: `docs/runtime_port/WAVE9_LANE_Z.md` does not exist. This lane used
lane Z's triage rule as stated in its own brief and did not go looking for the
file, so nothing here depended on its absence.
