# Wave 9 — lane INPUT: `Input::Device`, `Input::KeyMap` and the `Set<E24>` AVL family

Branch `lane-INPUT`, based on `wave/upgrade-path-complete` @ `ba7bc60`
(*Merge lane PKG3 and record attempt 17*).

**Landed: 7 bodies, 5 address doors closed, 0 new doors opened, 18,306,086 diffed
calls with 0 lane divergences, plus a defect found and fixed in a LANDED oracle
from Wave 8.5.**
**Declined: `?Update@Device@Input@@` and its three remaining helpers, with a
measured blocker.**

---

## 1. The door count, with a set-diff and an unmoved denominator

```
tools/addr_door_audit.py            BEFORE          AFTER
by-address call sites                  284            287   (+3, all to already-registered addresses)
distinct addresses                     201            201   <- the denominator did not move
registered addresses                  1125           1132
UNREGISTERED                            42             37
```

Set-diff of the unregistered list:

```
removed: 0x180023060  0x180023130  0x180023170  0x180053440  0x1800a7520
added  : (none)
```

Exactly the five rows this lane was given, and **nothing new**. The `+3` call
sites are this lane's own calls into `MemAlloc` (0x1800c7a50), the allocator-lock
free wrapper (0x180009c30) and the guarded object free (0x180006a10) — all three
already registered by other lanes, which is why `distinct addresses` is unmoved.

`tools/port_bypass_audit.py`: **0 BYPASS** (was 1 mid-lane — this lane's own, see
§5). `tools/rawdoor_audit.py`: 573 sites, **0 UNGUARDED-CALL**.
`tools/export_name_audit.py`: 667 names, 1 not in the DLL and it is
`rt_resolve_cache.cpp`'s pre-existing `?_singleton@...` placeholder, not a lane
row. `tools/runtime_census.py` Input row: 21/28 → **22/28 (78.6%)** — only
`?Save@` is exported, so five of this lane's seven bodies are invisible to that
census by construction (the `RegisterRuntimeAddr`-only blind spot
`reference_census_cannot_see_the_macro_doors.md` records).

---

## 2. The closure, counted BEFORE any body was written

Lane PICK's rule: porting the visible rows of a census can trade N doors for more
than N. Counted with `disassemble_function(filter_mnemonics=…)` on each body
before writing a line:

| # | Body | Retail | Instrs | Calls out | Door before |
|---|---|---|---|---|---|
| 1 | `Set<E24>` node alloc | `0x180023060` | 42 | `0x1800c6e60` Allocate (REGISTERED) + Enter/LeaveCriticalSection | YES (input.cpp:245) |
| 2 | `Set<E24>` destroy subtree | `0x180023130` | 18 | itself + `0x180009c30` (REGISTERED) | YES (input.cpp:137) |
| 3 | `Set<E24>` retrace | `0x180023170` | 61 | `0x180023570`, `0x1800236a0` | YES (input.cpp:247) |
| 4 | `Set<E24>` rotate left | `0x180023570` | 90 | **LEAF** (only RET) | no — pulled in by #3 |
| 5 | `Set<E24>` rotate right | `0x1800236a0` | 74 | **LEAF** (only RET) | no — pulled in by #3 |
| 6 | `?Save@KeyMap@Input@@QEAAXXZ` | `0x1800a7520` ord 3607 | 79 | `String::Format`, `Persistence::Set(int)`, `MemoryAllocator::Free` — all PORTED | YES (input.cpp:615) |
| 7 | KeyMap **deleting** destructor | `0x180053440` | 26 | #2, `0x180009c30`, `0x180006a10` — all ours after this lane | YES (engine_lifecycle.cpp:858, input_config_startup.cpp:180) |

**5 doors in, 7 bodies out, 0 doors added.** The three visible AVL rows cost two
extra bodies and both are leaves — the closure is CLOSED.

### The finding that made this cheap: there are TWO AVL families, not one

`runtime/src/collection/set_avl.cpp` (lane VID2) already ports the same four
algorithms — at four **different** addresses, for a **different** node layout.
PLAN §4.3 risk 1 says why: the element sits first in the node, so `sizeof(T)`
moves the whole header and MSVC emits a separate copy per instantiation.

| field | E16 (`set_avl.h`, lane VID2) | E24 (`set_avl_e24.h`, this lane) |
|---|---|---|
| element | +0x00, 0x10 bytes | +0x00, 0x18 bytes |
| parent / left / right | +0x10 / +0x18 / +0x20 | +0x18 / +0x20 / +0x28 |
| balance / height | +0x28 / +0x2a | +0x30 / +0x32 |
| `sizeof(Node)` | 0x30 | 0x38 |
| alloc / retrace / rotL / rotR | a5b0 / adc0 / b040 / b170 | 23060 / 23170 / 23570 / 236a0 |
| destroy subtree / erase | — / `0x18000aba0` | `0x180023130` / no door |

Grepped first, per this wave's rule: before this lane the only textual hits for
the five E24 addresses anywhere in `code/`, `engine/`, `runtime/`, `standalone/`
and `dx11/` were input.cpp's two `ResolveAddr` call sites and one comment in
`runtime/types/input/Input.h`; `0x180023570` and `0x1800236a0` appeared nowhere
at all. **No duplicate `RegisterRuntimeAddr` was created.** The two files were
transcribed independently from their own disassembly and then diffed against each
other: they agree in structure and differ in exactly the offsets above. Sharing
one walk across the two layouts is the defect
`reference_one_bst_find_two_node_layouts.md` exists to prevent, so they are
separate types with separate `static_assert`s.

---

## 3. What was verified, with call counts

Final run (`tools/rt_run.sh laneINPUT4`, `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1
BANISHED_RT_SETAVL24_PROBE=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60`),
under `tools/game_dir_lock.sh acquire lane-INPUT`, exited `rc=0` after 50s, empty
crash log. **Gated on this lane's own identity line, read out of the run's own
log** — not the on-disk md5, which says which exe is on disk and not which one
ran:

```
[RtPort] Input: 26 exports registered (lane INPUT: +2 KeyMap doors, +5 Set<E24> AVL addresses)
```

Tree-wide: `==== 486 sites, 18,306,086 calls, 1 divergences in 1 sites ====`. The
one divergence is **not this lane's**: it is `Object::Factory::Startup [probe,
not diffable]`, the pre-existing self-check WAVE9.md lists as instrument #6.

### 3.1 The live sites

```
Input::KeyMap::RegisterInput   calls=100    divergences=0     <- drives AllocNode + Retrace + both rotations
Input::KeyMap::ClearAll        calls=2      divergences=0     <- drives DestroySubtree
Input::KeyMap::Save            calls=1      worked=1  divergences=0
Input::KeyMap::~KeyMap (deleting, 0x180053440)  calls=1  worked=1  divergences=0
Input::KeyMap::Pressed         calls=10954  divergences=0
Input::KeyMap::GetValue        calls=9000   divergences=0
```

`calls=` read, not just `divergences=` — an uncalled port reports clean. Every one
of the seven bodies executed.

### 3.2 The lockstep AVL probe (`set_avl_e24_probe.cpp`)

Two private `Input::KeyMap`s, the same 45-step script, ours through the ported
bodies (reached because our `RegisterInput` resolves the addresses through
`ResolveAddr`, which consults the port registry first) and theirs through the
shipped `?RegisterInput@`/`?ClearAll@` via **`ResolveExportRaw`** — the raw door,
because `ResolveExport` would have handed back our own port and compared ours to
ours. Compared **after every step**, because AVL defects cancel: a rotation wrong
in one direction and wrong again in the other leaves the same key set and often
the same final shape.

```
[SetAvl24] steps=45 mismatches=0 parent-linkage-bad=0
[SetAvl24] effectiveness: depthAfter20Ascending=5 (a spine would be 20) rootChanged=6
           idsAbove0x7F=5 finalCount=44 dupCount=44
[SetAvl24] destroy: nodes=44 visited=43 expected=43 -> OK (rootLeft=25 rootRight=18)
[SetAvl24] dtor(0x180053440): ran=yes bindings=6 -> OK
[SetAvl24] negative-controls: shape-sensitivity=CAUGHT
           visit-counter-can-see-a-dropped-subtree=CAUGHT
```

Every counter is an **effectiveness control measured INSIDE the subject**, not a
proxy observed from outside it:

* `depthAfter20Ascending=5` against a spine of 20 is the retrace's control. It
  cannot be satisfied by noise, and if it ever reads 20 the probe says in as many
  words that no rotation ran and the sweep verified only the descent.
* `rootChanged=6` says the tree was restructured six times.
* `idsAbove0x7F=5` — the key is a BYTE compared UNSIGNED (`CMP DL,CL ; JC`). With
  only small ids a signed compare orders identically and the defect is invisible;
  this is the same shape as the ported inline `strcmp` that survived four waves on
  a uniform population.
* the zig-zag block is the only path through the pre-rotation predicates at
  `0x1800231f0` / `0x180023224`; a purely ascending population never enters them,
  so a port that dropped the double rotation entirely would have passed.
* `dupCount == finalCount` at step 45 says the duplicate insert returned without
  incrementing.

The fingerprint carries **shape** — the side of the parent, the balance and the
height, with `.` for a null child — and parent linkage is checked separately,
because a walk over `left`/`right` cannot see a wrong `parent`, and a wrong
parent is what breaks the NEXT retrace rather than this one.

### 3.3 `KeyMap::Save` — our writer, their reader, their writer, their reader

Save is a **pure read of the KeyMap** (it mutates only Persistence's settings
map) and its writes are idempotent and value-identical, which is what Wave 8.5
measured for `SerializeReferenceTable` and used to license a full replay. So:
ours writes and records → the shipped `?Get@Persistence@IO@@` reads back → the
shipped `?Save@` runs on the same object → read back again → all three must agree
per key. **Theirs runs last, so the map ends in exactly the state a retail Save
would have left.**

```
[KeyMapSave] oracle: keys=90 readable=90 diffs=0 vs-recorded=0 overflow=0
             neg-control=CAUGHT -> PASS
```

Printed **unconditionally**, not through `RT_DIFF_SHADOW` alone: that macro emits
its strings only on a mismatch, so on a passing run the numbers that make the pass
meaningful are invisible (WAVE9.md instrument #7 — *if a control is only visible
when the test fails, it is not a control*). The negative control writes a scratch
key and requires the read-back to distinguish its value from that value plus one.

**What this oracle cannot see, stated rather than implied:** an EXTRA key ours
emits that theirs does not — both read-backs would agree on it, because both are
reading what our own writer put there. Catching that needs the shipped
`Persistence::Set` detoured, and it is reached by a NEAR CALL inside the DLL that
our registry cannot intercept.

### 3.4 The deleting destructor had NO caller, and that was fixed rather than reported

Its two live sites are `Engine::Shutdown` (`calls=0` on the autostart path by
construction — lane ENG says so) and `Configuration::Startup`'s re-entry arm,
which no boot takes. Left alone it would have been an **unexercised port
reporting `calls=0`**, which is not verification
(`reference_uncalled_port_is_not_verified.md`). The probe therefore drives it on a
heap-allocated synthetic KeyMap — heap, because the body frees its own subject —
and checks that it returns `this` and that the destroy walk reached `count-1`
nodes. There is still **no ours-vs-theirs comparison** for it and there cannot be:
both implementations free the tree, so nothing survives to compare, and the
shipped body's frees go through a near call. Said in the source, in the probe's
output line, and here.

---

## 4. A DEFECT FOUND IN A LANDED ORACLE — and it took two attempts to state correctly

The probe's first run reported **2 divergences in 94 `RegisterInput` calls**. The
port was right both times. **`RegisterInput`'s oracle — landed by Wave 8.5, and
the row WAVE9.md lists as "PORTED AND VERIFIED, 50 calls, 0 divergences" — was
wrong.**

It asserts `GetIdForDeviceInput(device, inputId) == the id we just inserted`. But
that function is an **in-order walk returning the FIRST match**, so the assertion
only holds when the `(device, inputId)` pair is unique. Two actions bound to one
physical key is a legitimate KeyMap state; the probe's high-bit group happens to
map `0x7F`/`0xFF` onto `(0,15)` and `0x80`/`0xC0`/`0xF0` onto `(0,0)`.

The fix went in twice, which is the part worth recording:

1. **First attempt — expect the pre-insert first match.** Capture
   `preFound = theirFind(...)` before mutating and expect that when it is >= 0.
   Divergences 2 → 1. Still wrong: inserting `0x7F` when `0xFF` already owns the
   pair makes `0x7F` the new first match, because it sorts EARLIER in the walk.
2. **Correct:** `expectFound = (preFound >= 0 && (uint8_t)preFound < key)
   ? preFound : key`. Divergences 1 → **0**.

Two lessons, both of which this wave has paid for before:

* **A partially-corrected oracle still reports a divergence, and it is tempting to
  read the remaining one as the port.** The intermediate state — "1 divergence,
  and I just fixed the oracle" — is exactly where a correct port gets
  deregistered (`reference_oracle_must_snapshot_aliased_inputs.md`).
* **The population that found it is kept, not removed.** Deleting the duplicate
  pairs would have turned the run green and left the oracle wrong for every future
  lane. `set_avl_e24_probe.cpp` says so at the population, and `input.cpp` carries
  the correction at the line that was wrong — PLAN §6 step 8, fixed where it is
  written and not only in a wave doc.

Also corrected at source: `input.cpp` said `RegisterInput` is **ord 3712**. Read
back out of the export directory with pefile, it is **ord 3529**; 3712 is not this
symbol.

---

## 5. One self-inflicted finding worth carrying

The Save oracle's `TheirGetInt` helper resolved `?Get@Persistence@IO@@` with
`ResolveExportRaw` — correct for an oracle, but `port_bypass_audit.py` flagged it
**BYPASS**, because the helper itself had no diff guard and the audit classifies
per function. The right answer was not to rename the function past the checker but
to put the guard where it belongs: `if (!RtDiffEnabled()) return false;` at the
top, so the helper is genuinely inert whenever the oracle is off. Audit back to
**0 BYPASS**.

---

## 6. DECLINED: `?Update@Device@Input@@` and its three remaining bodies

Not ported. The blocker is **not** the one WAVE9.md row 16 states ("four
unexported, unnamed internal helpers plus DirectInput8 COM pointers past
`Device+0x11D0`, unmapped") and it is not lane SYS2's sizing either. Measured
here:

| body | retail | instrs | calls out | status |
|---|---|---|---|---|
| `Update` | `0x1800e1490` | 61 | the four below | not ported |
| `PollKeyboard` | `0x1800e1c60` | 98 | USER32 `GetForegroundWindow`, `GetAsyncKeyState`; COM `[RAX+0x48]`, `[RAX+0x38]` | not ported |
| `PollMouse` | `0x1800e1e10` | 192 | USER32 `GetForegroundWindow`, `GetCursorPos`, `ScreenToClient`, `GetAsyncKeyState`, `GetSystemMetrics`; COM `[RAX+0x48]`, `[RAX+0x38]` | not ported |
| `DispatchEvents` | `0x1800e0fc0` | 234 | **ZERO CALLS — a pure leaf** | not ported |
| `PlatformUpdate` | `0x1800e2130` | 41 | — | **already ported**, lane IOA (`input_win.cpp`) |

Three facts that sharpen the triage for whoever takes it:

1. **The COM surface is exactly two `IDirectInputDevice8` vtable slots**, `+0x38`
   (`Acquire`) and `+0x48` (`GetDeviceState`), and they are the same two in both
   pollers. Everything else the pollers call is USER32, resolved by name.
   "DirectInput8 COM pointers past `Device+0x11D0` (unmapped)" overstates it: the
   unmapped region holds the device pointer, and only two methods are used.
2. **`DispatchEvents` is a pure leaf with no calls at all** — a 4x-unrolled loop
   over `State` records turning a raw axis word into `value` (a float divide by a
   constant at `0x180161738`), `pressed`, `released` and a hold timer (the two
   doubles at `0x1801616b8`/`0x1801616c0`). It is the biggest of the four and the
   only one that is trivially oracle-able: it is a pure function of (buffer, count,
   time) and replays on a synthetic `Device` copy with a byte compare over the
   0x11D0 state region.
3. **The blocker that stands is the ORACLE for the two pollers, not their size.**
   `Update` drains the DirectInput queue, so ours-then-theirs reads a queue our own
   pass already emptied — the second call sees a different world. That needs a
   record-and-replay harness around `GetDeviceState`, which is one detour on a COM
   slot (the shape `ProbeAudioScene` already uses for `Device::Stop`), not a
   person at a keyboard. **Do not accept "needs an attended run" for this**: Wave
   8.5 closed that claim for `Input::Device`'s readers with 1,914 comparisons and
   no keyboard, and nothing in these four bodies changes it.

**Why this lane did not do it anyway.** Porting `Update` alone produces a shell
calling three unregistered addresses — SYS2's point, and still true. Porting all
four means landing two bodies (the pollers) with no oracle in the one wave where
`PLAN` says *nothing may be ported that was not diff-verified before the DLL
goes*. `DispatchEvents` alone is verifiable but closes no door: nothing in our
tree calls `0x1800e0fc0` today, so it would register with `calls=0` outside its own
probe. The honest shape is a lane: **the `GetDeviceState` recorder first, then all
four bodies together.**

Also still open and unchanged by this lane: `?Load@KeyMap@Input@@` is PORTED (lane
IOA) but `Input::Device::EnableMouseClipping` / `PlatformUpdate` are the only
Win32 half done; `?Suspend@KeyMap@Input@@`, `?GetIdList@`, `?GetNormalizedValue@`
and the two `??0KeyMap`/`??1KeyMap`/`??4KeyMap` exports are untouched and have no
door.

---

## 7. Files

| File | What |
|---|---|
| `runtime/src/collection/set_avl_e24.h` | the E24 node/set layout, with the E16↔E24 table and `static_assert` on every offset |
| `runtime/src/collection/set_avl_e24.cpp` | the five AVL bodies + `RtCollectionSetAvlE24Register()` |
| `runtime/src/collection/set_avl_e24_probe.cpp` | the lockstep probe (`BANISHED_RT_SETAVL24_PROBE=1`) |
| `runtime/src/input/keymap_save.cpp` | `KeyMap::Save` + the deleting destructor + the read-back oracle |
| `runtime/src/input/input.cpp` | the `RegisterInput` oracle fix, the probe's self-schedule hook, the ordinal correction |
| `runtime/src/input/rt_input_port.cpp` | registration + the lane identity line |
