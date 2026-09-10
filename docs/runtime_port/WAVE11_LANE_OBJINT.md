# Wave 11 lane OBJINT — the InternalFactory persist-state pair, and the creation-path probe

**Worktree** `C:/code/GIT/wt-wave11-objint` · **branch** `wave11/objint` · cut from `fa4fa86`.

Two deliverables, and they point in opposite directions. The two rows were **not
unported** — they had bodies since Wave 3 and the work was verifying them, which
this lane did with a new oracle. The probe's answer is **negative and it
contradicts its own brief**: porting a factory does not reach the creation path,
and that is a fact about all 202, established completely rather than
sampled.

---

## 1. The two rows

### 1.1 They were already written, and the census was right anyway

Doctrine 1 says grep before writing. Both bodies were already in
`runtime/src/object/internal.cpp` — `ClearPersistState` at line 575,
`ResetPersistState` at line 629 — **and both were already registered**, under
`if (table)`. `tools/runtime_census.py` counted them as unported for a reason it
prints itself:

```
40 registration(s) are present in the source but NOT counted above:
  runtime/src/object/internal.cpp   8  (opt-in; inside if(table) / if(shadow) / if(checkPtr))
  A gated registration is not a port. Arm it, measure it, then move it.
```

So the lane's job was never authoring. It was **arm it, measure it, then move
it** — and the reason nobody had is recorded in `WAVE3.md` as defect E: *the two
persist-state shadow sites have never executed.* That stood for eight waves.

### 1.2 Defect E is closed

`BANISHED_AUTOLOAD=1 BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1`, save
`Auto`, clean self-quit (`rc=0`):

```
[RtDiff]   Object::InternalFactory::ResetPersistState [shadow] calls=1  divergences=0
[RtDiff]   Object::InternalFactory::ClearPersistState [shadow] calls=1  divergences=0
```

`calls=1` each, and `calls=1` is the honest number: these run **once per
save/load transaction**, and the run performed one load. Reporting it as a
strong reading would be the `calls=` mistake in a new costume.

### 1.3 Why the shadow was not enough — and what was built instead

Each shadow has a **structural blind spot** that no number of calls closes,
because a shadow lets the DLL do the real work and then checks a prediction:

| shadow | what it verifies | what it is blind to |
|---|---|---|
| `ClearPersistState_Shadow` | that the post-state is `data=null size=0 cap=0` | **our body's stores never run.** A port with the `MemFree` deleted has an identical post-state and passes on every call — a silent leak under a green verdict |
| `ResetPersistState_Shadow` | array sizes, predicted capacities, the `-1` fill | **the three stream calls.** The encoding-factory slot (`+0x18` read vs `+0x20` write), the int slot (retail `0xa0`; the profile build's `0xb8` writes the right value through the wrong slot and desyncs every save), the destroy through slot 0 |

New file: **`runtime/src/object/internalfactory_persist_probe.cpp`**
(`BANISHED_RT_PERSIST_AB=1`, `=2` adds the negative controls). A
synthetic-subject A/B on a private `InternalFactory`, never reachable from the
game, run at the rows' own call site in `gameupdateloop_serialize.cpp` — not at
registration time, because it allocates and the DLL's allocator is not started
during `RtObjectPortInit`.

Two things make it an oracle rather than a self-check:

* the "theirs" side goes through **`RtOriginal`, the raw door**. `ResolveAddr(Module::Runtime, …)`
  consults the port registry first and would have handed back our own prologue
  once the tier is armed — retail compared with itself, which left a Wave 5
  probe dark for five waves.
* `ClearPersistState` is compared **through the allocator's own counters**
  (`MemoryAllocator+0x630 allocatedBytes`, `+0x638 allocCount`). That is the
  only screen in existence that can see the free.

Both legs are restored to a pristine template on entry, so the second leg does
not start where the first finished. Without that the growth arm would be
exercised once and the comparison would be green for a reason unrelated to the
port.

### 1.4 Readings

```
[PersistAB] ClearPersistState: fields ours{size=0 cap=0} theirs{size=0 cap=0}
            | allocator delta ours{blocks=-1 bytes=-72} theirs{blocks=-1 bytes=-72} | IDENTICAL
[PersistAB] ResetPersistState: emissions IDENTICAL | post-state
            ours{pSize=7 pCap=7 oSize=7 oCap=7 fill-1=1}
            theirs{pSize=7 pCap=7 oSize=7 oCap=7 fill-1=1} | IDENTICAL
[RtSaveTrace] pass Object::InternalFactory::ResetPersistState: 3 emissions identical
```

The three emissions are the encoding-factory create, the int count, and the
destroy — the whole stream contribution of the body.

**Negative controls, scored per screen, never as a union:**

| control | defect injected | caught by | result |
|---|---|---|---|
| 1 | `ClearPersistState` with the `MemFree` removed | allocator counters **only** — every field check passes | **CAUGHT** (`ours{blocks=0 bytes=0}` vs `theirs{blocks=-1 bytes=-72}`) |
| 3 | `ResetPersistState` filling `0` instead of `-1` | post-state fill check | **CAUGHT** (`fill-1=0` vs `1`) |

There is deliberately **no control 2**. A control for the encoding-factory slot
choice would have to be injected inside the port body, and this probe does not
edit the subject it measures. That screen is stated as unexercised-by-control
rather than folded into a union score.

**Still not covered, said here rather than implied:** the encoding factory's
third argument (retail's `XOR R8D,R8D` at `0x1800bfe6c`, the serializer
version). `save_trace.cpp`'s `EncThunk` records an Encoding call's *slot* and
not its arguments, and adding them would compare a stack address that
legitimately differs between the two sinks. That argument is pinned twice from
the bytes instead — Wave 9 lane Y read the `XOR` directly, Wave 4 independently
pinned both factory slots at arity 3. Verified by a different method, not
unverified. The read direction is likewise out of scope; the sink drives
`direction = 1` only, which is `save_trace.h`'s own documented limit.

### 1.5 Verdicts — and they differ

| row | verdict | census |
|---|---|---|
| `?ClearPersistState@InternalFactory@Object@@QEAAXXZ` | **PORTED, VERIFIED, UNGATED — default-on** | enters `ported` |
| `?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` | **PORTED, VERIFIED, STAYS GATED** | stays out |

**`ClearPersistState` left the tier** on Wave 9's own principle —
*gated-because-unverified is not the same as gated-because-dangerous.* The
tier's hazard is two writers on the handle table. **This body never touches
`_objects`.** It frees `_persistIds._data` and zeroes three fields, and
`_persistIds` is written by exactly two functions in the engine, both of them
this pair. There is no second writer to race. It was in the tier by association.

Liveness confirmed, because a verified body nothing calls is dead code that
reads like a landed port:

```
[RtPort] FIRST LIVE CALL into our ?ClearPersistState@InternalFactory@Object@@
         (default-on since Wave 11 lane OBJINT)
```

40-second `BANISHED_AUTOLOAD` run, `rc=0`, empty crash log.

**`ResetPersistState` stayed**, and **not because it is unverified** — its
oracle is as green as the other's. It stays for one line the other does not
have:

```c
ArrayPtrReserve(&self->objects, count);
```

`_objects` **is** the handle table, and on the load leg `count` comes off the
stream and can exceed `_size`, so that call can realloc an array the DLL's own
`Internal` ctors and dtors are also writing. Reserve never moves an index or a
`_factoryIndex`, and the DLL re-reads `_data` through the singleton every time,
so the safety argument is a good one — **but it is an argument**, and the tier
exists because this project has been wrong about a two-writer array before with
no proximate symptom. Its state is now **VERIFIED-BUT-UNARMED**, which is a
different thing from the UNVERIFIED it was in this morning, and what it owes is
a live tier-2 load run, not another oracle.

Three stale statements in `internal.cpp` were corrected rather than left to
mislead: the tier banner, the arming block's "the two persist-state shadow sites
have NEVER EXECUTED", and the `TABLE mode ARMED` printf's "STILL UNEXERCISED".

### 1.6 Object census, before → after

```
                total  called  ported  covered
before (fa4fa86)  191      56      32    57.1%
after             191      56      33    58.9%
```

Set-diff of the `--unported` list, one line, nothing entered:

```
-  export   Object   ?ClearPersistState@InternalFactory@Object@@QEAAXXZ
```

TOTAL 914 → 915, 80.0% → 80.1%.

---

## 2. The probe — and it says the published expiry is wrong

### 2.1 What was asked

`WAVE10E_LANE_FINAL.md` left `??_7Dialog@UI@@6B@` unpromotable with a
**mechanical expiry**, repeated verbatim by `WAVE11_KICKOFF.md` and again by
this lane's brief:

> The row becomes promotable and required the moment `?CreateNewObject@Dialog@UI@@`
> (`0x180038530`) or `?CreateInPlaceObject@Dialog@UI@@` (`0x180038610`) is ported
> **and registered**. From that point every resource-loaded `UI::Dialog` carries
> whatever `ResolveExport("??_7Dialog@UI@@6B@")` returns.

That is what was tested. **It does not hold.**

### 2.2 The static fact, established completely rather than sampled

Both factories have **zero code cross-references** in `Runtime-x64.dll`. Their
only referents are the `?_typeInfo@Dialog@UI@@` node's `factoryFn` (`+0x20`,
`0x1801faaa8`) and `inPlaceFactoryFn` (`+0x28`, `0x1801faab0`), plus `.pdata`,
relocations and the export table.

Rather than trust a Ghidra sample, a linear byte scan of all **1,349,120 bytes**
of `.text` looked for every encoding that could name a factory address —
`E8`/`E9 rel32`, any REX `LEA reg,[rip+d32]`, and `MOVABS reg,imm64` — against
all **202** `?Create*Object@` exports. The scan checks every byte offset, so it
is a *superset* scan: it can produce false positives, never false negatives, and
a zero from it is conclusive for those encodings.

```
factories                                                     202
code references to ANY of them, all encodings, whole .text      0
factories with >= 1 qword reference in .data                  202   (206 refs total)
```

**Scope of that zero, bounded precisely** (asked by the integrator, and worth
answering with a second scan rather than a caveat):

* **Both images, not one.** The `1,349,120` bytes above are `Runtime-x64.dll`'s
  `.text`. The same scan over `Application-x64.exe`'s `.text` (`1,372,672`
  bytes) also returns **zero**. So the claim is complete across both shipped
  code sections, not bounded to the DLL.
* **The zero includes the two doored exports.** `?CreateInPlaceObject@DialogController@UI@@`
  and `?CreateInPlaceObject@Internal@Object@@` were inside the covered set and
  are likewise unreferenced from either `.text`. Their door is the exe's
  **import slot**, which is an indirect call through the IAT and by construction
  contains no direct reference to the DLL address — which is exactly why a
  direct-encoding scan cannot see it, and why the import table had to be read
  separately to find them.

**The `.data` pointer is the `BaseTypeInfo` factory slot — measured, not
inferred.** For all **98** classes exporting both factories, the qword holding
`?CreateNewObject@` sits **exactly 8 bytes before** the one holding
`?CreateInPlaceObject@`, i.e. `BaseTypeInfo+0x20` (`factoryFn`) and `+0x28`
(`inPlaceFactoryFn`); node start is `ref − 0x20`. Zero exceptions. The
206-vs-202 discrepancy resolves cleanly too: **201 factories have exactly one
`.data` reference, and `?CreateInPlaceObject@Internal@Object@@` has five** —
it is shared by five typeinfo nodes, the classes that define no in-place factory
of their own and inherit `Internal`'s.

**So the whole chain resolves the way the integrator guessed.** To intercept any
factory you must own the **minted typeinfo row**, because the pointer lives in
that row. This is not a factory-porting problem at all — it is lane OBJDATA's
subject. Porting the 202 bodies is neither necessary nor sufficient for
interception; owning the rows is both.

**The only route into any of the 202 is an indirect call through a function
pointer in the DLL's `.data`,** written there by the DLL's static initialiser.
That is not a door. Not the import table, not the port registry, not a detour —
the same class as an in-DLL `rel32`, and `RegisterRuntimeExport` cannot redirect
it any more than it can redirect one.

The generic reader is code our tree already owns —
`?CreateObject@Factory@Object@@` (`runtime/src/object/type_system.cpp:352`) ends
in `info->factoryFn()`, and `?AddController@DialogInstance@UI@@`
(`runtime/src/ui/dialog_create.cpp:551`) does the same by hand. Those *are*
doors, but only for whatever `BaseTypeInfo*` they hold, and that node comes from
the AVL `Factory::Startup` builds by walking `?_firstTypeInfo@BaseTypeInfo@Object@@`,
which `type_system.cpp:79` resolves **by name** — the DLL's chain head. Every
node in that tree is a DLL node carrying the DLL's raw factory address.

So three conditions must hold, and the published expiry names only the first:

1. our body exists and is registered — **this lane did that**;
2. something calls through a `BaseTypeInfo` whose `factoryFn` is *our* pointer —
   needs the **minted** `?_typeInfo@Dialog@UI@@` row, not the DLL's node;
3. the chain the creator walks contains that minted node — blocked by the
   `_firstTypeInfo` split brain (`WAVE11.md`, class A).

### 2.3 Measured, not argued

New file `runtime/src/ui/dialog_factory.cpp` ports both factories faithfully and
registers each under **both** the mangled name and the address
(`BANISHED_RT_DLGFACTORY=1`; `=2` adds an effectiveness control). Two
independent counters, each with a working positive control:

| run | route | `[DlgFactory]` attendance | `[DlgTrip]` |
|---|---|---|---|
| armed | `AUTOTUTORIAL` + `DLG_VPTR_TRIP=2` + `RT_DLGFACTORY=2` | `CreateNewObject=0 CreateInPlaceObject=1 control=1` | `stamped=3 control=1 totalDispatch=1 foreignDispatch=0` |
| control | same route, factory **unarmed** | (not registered) | `stamped=2 control=1 totalDispatch=1 foreignDispatch=0` |

Both instruments' positive controls fired and were caught
(`[DlgFactory] CONTROL FIRED`, `[DlgTrip] POSITIVE CONTROL … CAUGHT (the
instrument discriminates)`), so neither zero is the zero of a counter that
cannot move.

Read the attendance line exactly: **the only entry into either factory in the
whole run was the probe's own control.** The `stamped` delta between the two
halves is `3 − 2 = 1`, which is that same control object and nothing else. The
game created its `UI::Dialog` objects through retail's factory in both halves,
identically. Registering the mangled name and the address changed nothing.

A bare `AUTOSTART` run reads `stamped=1` (the control alone) — the game stamps
**zero** on that route. That is not a defect in this measurement; it reproduces
`WAVE10E_LANE_FINAL.md` §4(b)'s own correction, which records bare `AUTOSTART`
at 0. The tutorial route was chosen because it is one the same lane measured as
non-zero.

### 2.4 Verdict on `??_7Dialog@UI@@6B@`

**STILL UNPROMOTABLE, and the published expiry condition is retired as
insufficient.** Porting and registering the factory is necessary and is now
done; it is not sufficient, and no amount of registering will make it
sufficient. The next observable to watch is not "is the factory ported" but
**"does the creator walk a chain containing the minted `?_typeInfo@Dialog@UI@@`
row"** — i.e. Wave 11 class A's split brain, lane OBJDATA's subject, not this
one's.

`??_7DialogController@UI@@6B@` was **not** promoted and was not investigated for
promotion. Its verdict is permanent and independent of creation.

### 2.5 What this says about the other 201 — bounded

Stated as three claims at three different strengths:

* **Complete and mechanical (all 202, both images).** No code in
  `Runtime-x64.dll` **or `Application-x64.exe`** names any of the 202 factory
  addresses in any direct encoding. Every one is reachable only through a
  `.data` function pointer, and that pointer is a `BaseTypeInfo` `factoryFn` /
  `inPlaceFactoryFn` slot (§2.2). Measured over both whole `.text` sections,
  not sampled.
* **Complete, and it is the one exception (2 of 202).** The exe's import table
  names exactly two factories —
  `?CreateInPlaceObject@DialogController@UI@@` (slot `0x140151688`) and
  `?CreateInPlaceObject@Internal@Object@@` (slot `0x140152a88`). **Those two do
  have a door.** They are the only members of the 202 for which "port and
  register it" is by itself a route. `?CreateInPlaceObject@Dialog@UI@@` is not
  among them, which is why this probe reads zero.
* **Measured on one subject only (1 of 202).** That registering a factory
  changes nothing observable was measured for `UI::Dialog` and for no other
  class. The mechanism generalises to the other 199 doorless factories by the
  first claim; the *reading* does not, and is not offered as one.

On shape: the two-line `new T()` picture is **half right**. Both bodies are
short, but neither is a `new T()` — each is the class constructor **fully
inlined**, three vtable stamps and roughly twenty zero-stores; the `CreateNew`
half additionally inlines `System::MemAlloc(0x88, 8)` with the allocator lock
taken by hand. This port calls the already-verified out-of-line
`??0Dialog@UI@@` instead of re-transcribing the inlined stores, which is the
move UI3 made in the opposite direction, and says so at the site.

### 2.6 Which wall is it — "not called", or "called but stamps by LEA"?

The integrator is right that these are different walls with different expiries,
and for `UI::Dialog` the answer is **both at once, on different sides of the
boundary**, which is why the published expiry looked plausible:

* **Our factory is not called.** `[DlgFactory] CreateNewObject=0
  CreateInPlaceObject=1(control)`. Nothing in the game enters our body, because
  the DLL reaches its own through a `.data` pointer.
* **Retail's factory *is* called, and it stamps by in-DLL `LEA`.** Both shipped
  bodies carry the stamp inline —
  `0x18003859d LEA RAX,[0x1801562a0]; MOV [RDI],RAX` in `CreateNewObject` and
  `0x180038650` in `CreateInPlaceObject`, `0x1801562a0` being
  `??_7Dialog@UI@@6B@`. The `[DlgTrip] stamped=2` in the *control* half is the
  count of dialogs **our** `??0Dialog@UI@@` stamped; every other `UI::Dialog` in
  the run was stamped by that `LEA`.

So porting the factory clears the second wall for the objects our factory
builds — and never gets to build any, because of the first. Clearing one wall
without the other buys nothing, which is exactly what the two attendance
readings measured.

This is a second, independent instance of lane OBJDATA's finding that
`Object::InternalFactory` is constructed every boot by an in-DLL `rel32` from
`Engine::Startup`. Same mechanism, different opcode (`rel32` there, a `.data`
indirect here), same conclusion; the two lanes reached it by different routes
and instruments.

### 2.7 The split-vptr question, answered explicitly

The integrator asks whether promoting `??_7Dialog@UI@@6B@` would have the
split-vptr shape OBJLIFE found for `??_7Internal@Object@`. **It would, and this
lane's own measurements show the split already exists — it is merely harmless
today.**

Two constructors stamp `UI::Dialog` in a live run with the DLL present:

| constructor | how it gets the pointer | measured |
|---|---|---|
| retail's `CreateNewObject`/`CreateInPlaceObject@Dialog@UI@@` | in-DLL RIP-relative `LEA [0x1801562a0]` — unredirectable | every resource-loaded dialog |
| our `??0Dialog@UI@@` (`dialog_ctor_dtor.cpp:242`, default-on since Wave 10 lane DLG2) | `ResolveExport("??_7Dialog@UI@@6B@")` — **consults the port registry first** | `[DlgTrip] stamped=2` on the tutorial route |

Both stamp the **same** value today, because `ResolveExport` falls through to the
DLL for an unpromoted row. **Promoting the minted row is precisely the act that
makes them differ**: our ctor would stamp the minted table while retail's
factory keeps stamping `0x1801562a0`, giving two vtables on one class with the
DLL present — the identical hazard OBJLIFE names, reached by an identical
mechanism.

So `??_7Dialog@UI@@6B@` fails the amended promotion rule
(`WAVE11.md` @ `1aed90e`: attendance is not sufficient, raw-pinning is a
co-condition) on **two** independent grounds — no attendance, and an
unpinnable competing stamp. It is not a row waiting on a route.

### 2.8 Two live risks, both answered

* **The 104 abort traps: NONE become reachable.** The
  `RtMintTiTrapStub_*` stubs are only ever loaded into a *minted* `BaseTypeInfo`
  row, and a minted row is only installed under `BANISHED_RT_MINT` /
  `reg_mint_promoted.cpp`. This lane registers two function symbols, mints no
  data, promotes no row, and does not touch `_firstTypeInfo`. Row 24's own
  traps (`RtMintTiTrapStub_24_0/_24_1`,
  `rt_data_mint_tables.inc:1816-1817`) are the only ones adjacent to this
  subject and arming the file makes them neither more nor less reachable. No
  trap fired in any run.
* **The allocator gate is on this path but is not a blocker.** `CreateNewObject`
  allocates: retail inlines `System::MemAlloc(0x88, 8)` (`0x1800c7a50` — lock
  `_singleton@MemoryAllocator + 0x640`, `?Allocate@` `0x1800c6e60`, unlock). The
  port reaches it through `ResolveAddr(Module::Runtime, 0x1800c7a50)`, which
  consults the port registry first, so `BANISHED_RT_ALLOC=1` makes our allocator
  answer and unset makes the DLL's; one heap either way.
  **`BANISHED_RT_ALLOC` was left unset in every run of this lane**, so the probe
  measures one change and not two. `CreateInPlaceObject` allocates nothing.

---

### 2.9 The static-destructor exposure, checked rather than assumed

The integrator warns that OBJDATA's first `[ObjTI] ATTENDANCE` report printed
from a static destructor, where the port registry is already torn down, and read
`unregistered(DLL)` for every row — a false negative made entirely of timing.

`[DlgFactory]` **does** print from a static destructor, and so does `[DlgTrip]`.
It does not have that exposure, and the reason is what the counter is made of:
both are plain `uint64_t` globals in their own translation unit, incremented at
**entry to the body during the run**. Neither consults the port registry, at
report time or ever. Teardown order can change nothing about a number already
counted. OBJDATA's report was falsifiable by teardown because it *asked the
registry a question at print time*; these do not.

And that is argued from construction, so it is backed by the reading the
integrator asks for — **a positive control that must read nonzero, in both
instruments, in the same run**:

```
[DlgFactory] CONTROL FIRED (attendance moved 0 -> 1, returned the buffer)
[DlgTrip] POSITIVE CONTROL: fired=1 observed-in-slot3=1 -- CAUGHT (the instrument discriminates)
```

A counter that has only ever printed zero is indistinguishable from one wired
wrong. Neither of these has only ever printed zero.

## 3. Contradictions to the brief, collected

1. **"Two rows to port."** Both had bodies and registrations since Wave 3. The
   work was verification and gating. A gated registration reads as absent to the
   census, correctly, and that is a different task from authoring.
2. **"`??_7Dialog`'s expiry is mechanical and already written down."** It is
   written down and it is **wrong** — necessary, not sufficient. §2.2, §2.3.
3. **"Is the two-line `new T()` shape typical?"** Neither factory is a `new T()`.
   §2.5.
4. **"Zero of the 202 are reachable."** Two are: they sit in the exe's import
   table. Neither is Dialog's. §2.5.
5. **`BANISHED_AUTOQUIT` was not used** — it overrides `AUTOSTART`'s New Game
   path and yields a menu-only run that exits 0 and looks healthy.
   `BANISHED_PAUSEMENU` + `PAUSEMENU_QUIT` was used instead, which is what made
   the `calls=` summary land at all: the first Part-1 run was killed by its
   timeout and printed no summary, which would have read as `calls=0`.

## 3a. LANE-BOUNDARY COLLISION — `internal.cpp`, for the integrator

The lane-boundary message ("OBJLIFE owns `internal.cpp` edits; do not add
instruments there") arrived **after** this lane had already committed to that
file. Declaring it rather than leaving it to a merge:

| edit | what it is | can it move? |
|---|---|---|
| `ClearPersistState` registration moved out of `if (table)` into an `if (!shadow)` block | **the lane's verdict.** Cannot move — it is the deliverable | no |
| one first-call `printf` inside `RtObject_InternalFactory_ClearPersistState` | an instrument, and exactly what the boundary reserves to OBJLIFE | **yes — drop it if OBJLIFE's version conflicts.** It only proves the ungated body is not dead code, and §1.5 records that reading |
| three stale statements corrected (tier banner, the arming block's "NEVER EXECUTED", the `TABLE mode ARMED` printf) | comments only | yes, re-apply on OBJLIFE's text |

The registration edit and OBJLIFE's `??0Internal`/`??1Internal` work touch the
same `if (table)` block but different lines, and OBJLIFE's rows stay inside it
while `ClearPersistState` leaves it. If the merge is textual rather than
semantic, **the thing to preserve is that `ClearPersistState` is registered
exactly once and outside the tier** — a duplicate registration is
last-binding-wins and would silently make one body dead code.

## 4. Files

| file | change |
|---|---|
| `runtime/src/object/internalfactory_persist_probe.cpp` | **new** — the synthetic-subject A/B and its controls |
| `runtime/src/object/internal.cpp` | `ClearPersistState` ungated; liveness line; three stale statements corrected; `ResetPersistState`'s own reason for staying recorded |
| `runtime/src/ui/dialog_factory.cpp` | **new** — both factories, attendance counter, effectiveness control |
| `runtime/src/ui/rt_ui_port.cpp` | registers the above (`BANISHED_RT_DLGFACTORY`, opt-in) |
| `code/Game/Game/gameupdateloop_serialize.cpp` | one gated call firing the A/B at the rows' own call site |

## 5. Environment variables added

| var | effect |
|---|---|
| `BANISHED_RT_PERSIST_AB` | `1` run the persist-state A/B once; `2` run the negative controls first |
| `BANISHED_RT_DLGFACTORY` | `1` register both `UI::Dialog` factories; `2` also fire the effectiveness control |
