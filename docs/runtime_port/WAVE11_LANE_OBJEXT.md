# WAVE 11 lane OBJEXT — Class C, the External / ExternalFactory bodies

**Worktree** `C:/code/GIT/wt-wave11-objext` · **branch** `wave11/objext`, cut from `fa4fa86`.

Subject: the seven `Object::External` / `Object::ExternalFactory` rows of Wave 11's
Class C.

---

## 1. Headline

| | before | after |
|---|---|---|
| `Object` census (`tools/runtime_census.py --namespace Object`) | `191 / 56 called / 32 ported` = **57.1%** | `191 / 57 called / 36 ported` = **63.2%** |
| unported Object rows | 24 | 21 |

**The set-diff, not the count** (WAVE11.md standing rule 7). Four rows LEFT the
unported set and one ENTERED it:

```
LEFT (closed):
  ?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z
  ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
  ?PopPath@ExternalFactory@Object@@QEAAXXZ
  ?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z

ENTERED (new gap row):
  ?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z
```

`24 - 4 + 1 = 21`. The arithmetic is checked, not asserted.

**A LATER READING OF THE SAME NAMESPACE SAYS 38/59 = 64.4%, AND THAT NUMBER IS
NOT THIS LANE'S RESULT.** Re-measured after the probe rewrites, the census reads
`+2 called / +2 ported` against the figures above -- but the UNPORTED SET is
byte-identical, still the same 21 rows. So no row changed state: two rows that
are BOTH called and ported entered the tree's call graph because the PROBE
started calling them, which lifts numerator and denominator together. That is
instrument traffic, not port progress, and I did not pin which two.

The number to trust is the SET-DIFF, which is unaffected. Recorded because a
percentage that moves while nothing changes state is exactly the shape that
reads as progress -- and because this lane's instrument is now large enough to
move the metric it is measured by.

`called` grew by exactly **one**, not three, and the reason is the useful part of
this lane's result — see §4.

---

## 2. Per-row verdict

| row | verdict | oracle |
|---|---|---|
| `?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z` | **PORTED + VERIFIED** | live full shadow against the shipped export (pure function) |
| `?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z` | **PORTED + VERIFIED** | lockstep synthetic-subject probe vs the shipped container bodies |
| `?PopPath@ExternalFactory@Object@@QEAAXXZ` | **PORTED + VERIFIED** | same probe |
| `?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z` | **PORTED, PARTIALLY VERIFIED** | live shadow on the one thing the body itself computes (the compiled path) against retail's *independently compiled* `GetCompiledPath`; the three callees and the mutation are not covered — §3.4 |
| `?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` | **HELD BACK** | §5.1 |
| `?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z` | **HELD BACK** | §5.2 |
| `?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z` | **HELD BACK** | §5.2 |

Four ported, three held back, and the three holdbacks are **two mechanisms, not
three** (§5).

---

## 3. What was ported, and what pinned it

New files, all under `runtime/src/object/`:

* `external_compiled.{h,cpp}` — `GetCompiledPath`, `FindOrLoad`
* `external_path_stack.{h,cpp}` — `PushPath`, `PopPath`, and the four
  `Collection::Array<PathStackEntry>` bodies underneath them
* `external_path_stack_probe.cpp` — the lockstep oracle for the second file

Registered from `rt_object_port.cpp` under `BANISHED_RT_COMPILED` and
`BANISHED_RT_PATHSTACK`, each nested inside `BANISHED_RT_EXTERNAL`, so either
half can be A/B'd without turning the class off.

### 3.1 `GetCompiledPath` — the ABI was the risk, not the body

`SA` means **static**: there is no `this`, and it returns a `System::Filename`
**by value**, so the hidden return slot is the FIRST argument:

```
Filename* RtObject_ExternalFactory_GetCompiledPath(Filename* ret, const Filename* path);
```

Confirmed from the prologue (`MOV RDI,RCX ; MOV RBX,RDX` at 0x1800bec2f, `MOV
RAX,RDI` at 0x1800bee98), not from the demangled name. An `fn_*` typedef that
omits the slot is the shipped defect
`reference_hidden_retslot_wild_write.md` records — a wild write that still
returns the right answer, so it passes every functional check.

The body is one six-term concatenation:

```
result = dataDir + dir + fname + "_" + ident + ".crs"
```

with the two literals read out of the image at `0x180155b04` (`"_"`) and
`0x180155afc` (`".crs"`), then one `AdjustDelimiters`.

**A dead call is faithfully kept.** Both this body and `FindOrLoad` open with
`path.GetPathExtension()` into a stack slot that is never read again — written at
0x1800bec41, released at 0x1800bee6f, untouched in between. Its *value* is dead;
its *allocation traffic* is not. It is transcribed rather than elided.

### 3.2 The path stack — lane RRES's stated blocker, discharged

`runtime/src/object/external_path.h` names this lane's subject as its own
explicit deferral, with the reason:

> `PushPath` … and `PopPath` … are NOT ported. They need `0x18002e220` —
> `Collection::Array<PathStackEntry>::Resize(n, const T&)`, the 0x10-element
> instantiation, which exists nowhere in the tree — plus its element copy-ctor
> and destroy-range.

Four bodies were needed, not one:

| address | body |
|---|---|
| `0x18002e220` | `SetSize(n, const T& fill)` |
| `0x18002e8d0` | elem-destroy(range) — two `Filename`s per element |
| `0x18002ccb0` | `RemoveLast(&out)` |
| `0x18002de30` | `RemoveAt(index)` — release, shift down, `--size` |

**`SetSize` was not written a fourth time.** Wave 9 lane Q already ported that
body once, parameterised by `rtcoll::ArraySpec`, and
`runtime/src/engine/mainloop_video.cpp` already sets the precedent of defining a
spec outside `array.cpp` for another lane's instantiation (`kSpec_VideoResource`).
`kSpec_PathStack` is that same move. **The reuse was checked before it was made,
not after**: `SetSizeCore` was read against 0x18002e220's disassembly arm by arm
— the `cap = n*3` growth rule when `n == old + 1 && old != 0`, the
`MemAlloc(cap*stride, 8)`, the realloc arm's `destroy(old, oldSize)` before
`MemFree`, and the non-realloc shrink arm's
`destroy(old + newSize*stride, oldSize - newSize)`. All four agree. (Port the
CHECK before the code — `reference_a_written_recipe_can_be_wrong.md`.)

The two `Remove*` bodies are **not** `SetSize` and are written out in full. Two
details a paraphrase loses:

* `RemoveAt`'s `--size` sits in **both** arms of its outer `if`, so a removal
  past the end still decrements. Faithful, and load-bearing.
* the shift-down is a placement copy-**construct** into memory the two releases
  immediately above just vacated, not an assignment. Using the release-then-adopt
  idiom there would double-release.

`PushPath` carries a **doubled `AdjustDelimiters`** on the entry's filename
(0x1800bd1cb and 0x1800bd1d4, two separate calls). It is idempotent and so has no
observable effect, but it is what the bytes say and it is kept.
`ExpandRelativePath` carries the same doubled call, which is corroboration that
it is a source-level idiom of retail's and not a one-off.

`PopPath`'s empty-stack behaviour is **faithfully unguarded**: on a zero-deep
stack `idx` is `-1` and retail reads `_data[-1]`. No null check was added
(CLAUDE.md, "Don't 'fix' a faithful crash with a null check").

### 3.3 The write-side pin this port adds

`PushPath` builds its `PathStackEntry` in two adjacent stack slots
(`[RBP-0x18]` directory, `[RBP-0x10]` filename) before handing `&[RBP-0x18]` to
`SetSize`. That is a **write-side** confirmation of the field order
`runtime/types/object/External.h` had only from the two read-side accessors.

### 3.4 `FindOrLoad` — three name doors, zero address doors

Retail's flow, from the listing:

```
0x1800bf339  if (!path->data) return null
0x1800bf345  if (length(path->data) == 0) return null      [data-8, NOT data-4]
0x1800bf359  expanded = ExpandRelativePath(this, path, appendIdent=true)
0x1800bf366  found = Find(expanded)                        *** the PRIVATE Find
0x1800bf376  if (found) { if (this->_loadEnabled) Add(found); return found; }
0x1800bf398  compiled = <the same six-term concatenation as GetCompiledPath>
0x1800bf550  loaded = External::Load(compiled, nullptr)
0x1800bf562  if (loaded) { loaded->_path = expanded; AdjustDelimiters; Add(loaded); }
```

Ghidra's C **drops `ExpandRelativePath`'s third and fourth arguments entirely** —
the listing at 0x1800bf34f-0x1800bf359 shows `R8 = path` and `R9B = 1`, neither of
which appears in the decompiled call. The port was transcribed from the listing
(`reference_ghidra_c_output_vs_disassembly.md`).

The three unported callees (`Add`, `External::Load`, and the private `Find`) are
reached by **mangled name** through `RT_EXPORT_ONCE`, never by address. That
matters: a by-address call silently keeps using the DLL even after someone ports
the target, and is invisible to the scanners
(`reference_bypassed_port_needs_a_source_checker.md`). Each of these three
follows a later port automatically.

---

## 4. The finding worth carrying out of this lane

**`called` grew by one, not three, because one of the three callees was already
ours.**

`FindOrLoad` calls `0x1800be0e0` =
`?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z` — note
`AEAA`, **private**, non-const, returning a *mutable* `External*`. It is **not**
`0x1800bdeb0`, the public `QEBA…PEBVExternal` that `external.cpp` ports. Both are
exported: `AEAA` means private, not unexported, and this DLL exports its private
members (`reference_aeaa_is_private_not_unexported.md`). Calling the ported public
one here would have been a different function with a different signature.

Reaching for the correct private symbol turned out to hit an existing port:
`runtime/src/object/external_find.cpp` (Wave 9 lane RRES follow-on) already
registers exactly that name. So the census `called` set gained only `?Add@`.

Two lessons in one row:

1. "Grep for the port before writing it" pays **in both directions** — it stops
   you re-porting something, and it tells you a callee you were about to count as
   a new DLL dependency is already yours.
2. A lane that reads `AEAA` as "unexported, must reach by address" would have
   opened an address door **onto our own already-ported body**, which is the
   port-bypassing-a-port defect `tools/port_bypass_audit.py` exists to catch.

---

## 5. The three holdbacks — two mechanisms

### 5.1 `SerializeReferenceTable` — the *other* AVL instantiation

Mechanism: the body clears and then rebuilds `ExternalFactory::_references`,
which is `Collection::Set<ReferenceEntry>` — the **0x10-byte-element**
instantiation, node layout parent `+0x10` / left `+0x18` / right `+0x20`,
`sizeof(Node) == 0x30`. It needs two bodies our tree does not have:

* `0x18002cb90` — `Set<ReferenceEntry>::Insert(key, Filename)`
* `0x18002ebf0` — the destroy-whole-tree the function opens with

`external_set.cpp` ports the **0x18-byte-element** family
(`ResourceEntry`, node `0x38`) and cannot serve this one: MSVC emitted a
completely separate copy of find/insert/rebalance/erase because the node's
pointer tail offsets are a function of `sizeof(T)`
(`reference_one_bst_find_two_node_layouts.md`, restated in `External.h`).

**This is a scopeable follow-on, not a wall.** The remaining work is exactly what
Wave 9 lane RES did once already, at the other size: port the `ReferenceEntry`
AVL family, then this body is ordinary. Everything else it needs is present —
`IO::Serializer` is fully ported (`runtime/src/io/serializer.h`), and the
`rw_vtbl` slot constants name `0x98` = `kScalarUInt` and `0xA0` = `kScalarInt`,
which are the two slots this body drives.

**The brief said to build the replay oracle rather than record it unverifiable,
and that instruction is correct and is not what is blocking this row.** A
write-path `Serialize` *is* replayable and that is how it should be verified when
it is written; the row is held back because the tree cannot express the body at
all, not because no oracle exists for it.

### 5.2 `Load@External`, `SaveText@External` — and `Add`, the row this lane opened

**One root cause for all three.** Each stack-constructs a *concrete* `IO` stream
object **by value** and then drives it:

| row | the object it constructs on the stack |
|---|---|
| `External::Load` | `IO::CompressedFileStream` over an `IO::BinaryFileStream` |
| `External::SaveText` | `IO::TextFileStream` |
| `ExternalFactory::Add` (the new gap row) | `IO::VisitorStream` |

`runtime/types/io/Stream.h` pins `IO::Stream`, `IO::MemoryStream`,
`IO::CompressedStream` and `IO::CompressedMemoryStream`. It pins **none of these
three**, and their constructors and destructors are not ported either. A
by-value stack construction cannot be substituted through a door — the caller's
frame has to be the right size and the vtable stamps have to land at the right
offsets — so this is not a case where an unported callee can be reached by name.

That is why `Add` entered the gap when `FindOrLoad` landed, and it is not a
surprise this lane could have avoided: `FindOrLoad` calls `Add` unconditionally
on both of its branches, so porting `FindOrLoad` at all means naming `Add`. The
choice was between one loud, named, scanner-visible door and leaving the whole
row on the DLL. `runtime/src/engine/mainloop_video.cpp`'s banner takes the same
trade explicitly and gives the same reason ("trading one silent dependency for
three loud ones is the direction PLAN §5 wants").

**Recommended next lane: the concrete `IO` file-stream family.** Three of the
four rows above fall out of it, plus whatever else in the tree is stuck behind
`TextFileStream` / `BinaryFileStream` / `VisitorStream`. Scoping it as "port
`External::Load`" would be scoping the symptom
(`reference_audio_holdbacks_share_a_callback_root_cause.md` is the same shape:
six holdbacks, one root cause).

**One row this port does close from that family's debt.**
`runtime/src/engine/mainloop_video.cpp`'s banner records that porting
`OnVideoRestart` deliberately raised the unsatisfied-export count 117 → 119 by
opening `+Load@External`, `+GetCompiledPath@ExternalFactory` and
`+OnVideoRestart@SaveLoad`. `GetCompiledPath` is now satisfied.

---

## 6. Oracles, and what each can and cannot say

### 6.1 `GetCompiledPath` — a full live shadow, because it is pure

It reads a const buffer and one singleton's const `dataDirectory`, allocates its
own result, and mutates nothing, so ours and theirs can both be run on every
live call. The two results are written to two different fresh slots and cannot
alias (`reference_oracle_must_snapshot_aliased_inputs.md`). Compared as **text**,
because the two sides allocate their own buffers and a pointer comparison would
fail 100% of calls.

### 6.2 `PushPath` / `PopPath` — lockstep synthetic subject

Both are destructive mutators on one shared stack. Running theirs after ours
pushes the same entry twice and, on the pop side, pops an element the first call
already freed — a live shadow is not weak here, it is **wrong**. So the subject is
a private pair of `Collection::Array<PathStackEntry>` headers driven by one
script, ours through the ported bodies and theirs through the shipped ones reached
with `ResolveRuntimeRaw`.

`ResolveRuntimeRaw`, **not** `ResolveAddr(Module::Runtime, …)`: the latter
consults the port registry first and would hand back our own body, making the
probe compare ours with ours
(`reference_resolveaddr_consults_port_registry_first.md`). These four bodies have
no export in either build, so `ResolveExportRaw` cannot reach them at all.

The script is chosen to drive every arm at least once:

* push #1 — capacity 0 → 1, the `oldSize == 0` arm (no `*3`)
* push #2 — the `n == old+1 && old != 0` arm, capacity 2 → 6
* pushes #3/#4 — inside capacity, no realloc, no destroy
* `RemoveAt(0)` on a 4-deep stack — the shift-down loop, three elements moved
* pops to empty — the shrink arm with `realloced == false`

The comparison is on **shape** (size *and capacity* — the `*3` rule is the arm a
paraphrase is likeliest to get wrong and is invisible to a contents-only check)
and on the **text** of both halves of every live element.

### 6.3 `FindOrLoad` — what it is and is not held to

**Not covered:** both branches mutate. The hit branch conditionally re-`Add`s and
the miss branch allocates an `External` and inserts it. Running retail's
`FindOrLoad` after ours would insert the same key a second time and retail's
second insert **returns early**, which makes the comparison vacuously green —
the exact trap `external_set.h` records for `KeyMap::RegisterInput`.

**Covered:** the compiled path the miss branch builds is the whole of that
branch's own work, and retail has a **separately compiled body that builds the
same string from the same input** — `GetCompiledPath`. Shadowing against it is
therefore a real comparison and not a self-check. This is why the two functions
share a source file.

### 6.4 Effectiveness controls

Both files carry branch counters (`RtObject::g_cc`, `RtObject::g_psc`) so a green
run can be told from an unexercised one, and negative controls that force a
single-token defect:

* `BANISHED_RT_COMPILED_NEGCTL=1..5` — wrong join character, wrong extension,
  `dir`/`fname` swapped, `GetPathFilename(strip=false)`, extension substituted
  for identifier
* `BANISHED_RT_PATHSTACK_NEGCTL=1..4` — wrong join character, entry halves
  stored in the wrong order, `RemoveAt` skips the shift-down, `SetSize`'s fill
  ignored

Each control is scored **separately** with the mechanism of its miss stated. A
score on the union of two screens hides that one of them caught 1 of 3
(`reference_union_control_hides_a_blind_screen.md`).

---

## 7. Run results — MEASURED

Exe under test `e2b6cd3913fe29c17bee3cf981aba32b`, deployed under a held
`tools/game_dir_lock.sh` and md5-verified against the build before any run
(`tools/objext_run.sh` refuses to measure otherwise). Logs in
`lane_objext_logs/`.

### 7.1 Stability

| run | ports | probe | crash log |
|---|---|---|---|
| A | ON | off | **empty** |
| C | ON | on  | **empty** |
| B | OFF (kill switch) | off | **empty** |

Run A exists specifically so that "the port is stable and my instrument was
not" is a measurement rather than an argument — see §10.

### 7.2 `GetCompiledPath` — the input sweep

14 inputs (13 synthetic + the live path the self-test rides in on), each run
ours-vs-theirs against the shipped export, every control scored separately:

```
SWEEP ctl=0 compared=14 mismatches=0   PASS              disagree 0
SWEEP ctl=1 compared=14 mismatches=14  control CAUGHT    disagree 0
SWEEP ctl=2 compared=14 mismatches=14  control CAUGHT    disagree 0
SWEEP ctl=3 compared=14 mismatches=8   control CAUGHT    disagree 0
SWEEP ctl=4 compared=14 mismatches=4   control CAUGHT    disagree 0
SWEEP ctl=5 compared=14 mismatches=12  control CAUGHT    disagree 0
```

`disagree 0` on every control is the strong result here, and it is a claim
about the MODEL, not the port: the predicted mask is computed by the three
predicates in §11 and reproduces all 14 cells of all five controls. See §10.4
for why that number is worth more than the `mismatches=0` beside it.

### 7.3 `PushPath` / `PopPath` — live singleton, and the container bodies

```
PART2 ctl=0 compared=1 mismatches=0 att=0  PASS
PART2 ctl=1 compared=1 mismatches=1 att=1  control CAUGHT
PART2 ctl=2 compared=1 mismatches=1 att=1  control CAUGHT
PART2 offset cross-check (+0x30/+0x38 via retail's own accessors): 3 checked, 0 disagreed

PART1 ctl=0 compared=8 mismatches=0 att=0  PASS
PART1 ctl=2 compared=8 mismatches=8 att=6  control CAUGHT
PART1 ctl=3 compared=8 mismatches=4 att=1  control CAUGHT
PART1 ctl=4 compared=8 mismatches=8 att=1  control CAUGHT
```

`att=` is the control's attendance at the perturbation site it NAMES (§10.3).
Every active control has a non-zero one, so none of these catches is banked on
an unrelated mechanism. `ctl=0` has `att=0` correctly — it perturbs nothing.

The **offset cross-check is the answer to the singleton hazard**: the element
`PushPath` pushed onto the live factory was read back both directly through our
`_pathStack` offsets and through retail's own `GetPathStackDirectory` /
`GetPathStackFilename`, and the two agreed on every trial. That is the only
independent evidence available that our `static_assert`s describe the object
retail actually built, and it is why part 2 does not use a fake factory.

### 7.4 What is NOT covered

* `FindOrLoad` has no whole-body oracle — §6.3, and that is a property of the
  function, not a gap in effort.
* `PART1 ctl=1` does not exist: control 1 perturbs `PushPath`, which part 1
  does not drive. `PART2 ctl=3/ctl=4` likewise do not exist. The control table
  in the probe states which screen each control can reach, and that table is
  itself a claim that was WRONG once — see §10.3.

---

## 8. Anything that contradicts the brief

1. **"`runtime/src/object/` … part of this subject may already be ported and
   merely unregistered."** Checked by mangled name **and** by address across
   `runtime/`, `code/`, `engine/` and `standalone/`. **None of the seven rows had
   a body anywhere in the tree.** The five existing `external_*.cpp` files are
   the resource *tree*, the *path readers*, the private `Find` cache lookup, the
   `ExternalPtr` serializer and the `Set` family — no overlap. The
   already-ported thing this lane did find was a **callee**, not a subject row
   (§4).

2. **"`?GetCompiledPath@` and `?Load@` are `SA` — statics … Get the ABI right for
   the two by-value-return rows."** `GetCompiledPath` returns by value and has a
   hidden slot; **`Load@External` does not.** It returns `PEAV12@` — a raw
   `External*` in RAX — so it has no hidden slot, and its two arguments are
   `RCX = const Filename&`, `RDX = External*`. Treating it as a by-value return
   would have inserted a phantom first parameter. (`SaveText` returns `_N`, a
   bool: also no slot.) The rule the brief was reaching for is right; the row it
   was applied to is not.

3. **"Before writing 'unverifiable, no route', grep `standalone/` and the
   `BANISHED_*` list."** Done, and it did not change any verdict here — the three
   holdbacks are blocked on *expressing the body*, not on reaching it. No route
   exists to a function the tree cannot compile.

---

## 9. Two cross-lane questions, answered for this lane

### 9.1 Was any of the seven already ported behind a gate?

**No.** Three of this wave's row assignments turned out to be already ported and
merely gated behind an env var (`??0Internal` / `??1Internal` and both of
OBJINT's `InternalFactory` rows, inside an `if (table)` block in
`runtime/src/object/internal.cpp` since Wave 3), so the question is a real one.

Swept both ways — every mangled-name fragment **and** every retail address —
across `runtime/`, `standalone/`, `engine/` and `code/`, filtered to lines that
register or gate (`Register*`, `RtOptIn(`, `if (`), excluding this lane's own
two new sources:

```
PushPath@ExternalFactory     none      1800bd0e0   none
PopPath@ExternalFactory      none      1800bd040   none
FindOrLoad@ExternalFactory   none      1800bf320   none
GetCompiledPath@Externa…     none      1800bec10   none
SerializeReferenceTable@…    none      1800bcb60   none
SaveText@External            none      1800bb9d0   none
Load@External@Object         none      1800bc620   none
```

The address half matters as much as the name half: a registration written
through a *named constant* rather than a string literal is invisible to
`runtime_census.py` (`reference_census_cannot_see_nonliteral_registrations.md`),
but it still has to name the address somewhere. Both columns are empty.

So all seven rows were genuine unported work. Four are now ports; three are
holdbacks. **No row in this lane has an "arming decision" disposition.**

The one thing this lane *did* find already ported was a **callee**, not a
subject row — see §4.

### 9.2 Do any of the ported bodies stamp or read a vptr?

**No — not one of the four, and this is what makes these rows independent of the
two-vtables-on-one-class hazard** that binds `??_7External@Object@@6B@` and
`??_7CompiledExternal@Object@@6B@` in this same wave.

Checked from the listings rather than asserted: **every `CALL` in all four
bodies is a direct `CALL 0x…`. There is not one `CALL qword ptr [reg+disp]` in
any of them** — no virtual dispatch at all, so there is no slot for a wrong
vtable to be read out of.

| body | vptr traffic |
|---|---|
| `GetCompiledPath` | none — string work on `Filename`, which has no vtable |
| `PushPath` / `PopPath` | none — `Filename` and the array header only |
| the four `Array<PathStackEntry>` bodies | none — `Filename` copy-ctor and release |
| `FindOrLoad` | **touches an `External` but never its vptr** |

`FindOrLoad` is the only one that handles a polymorphic object at all. It
receives an `External*` from `External::Load` — retail-constructed, therefore
carrying retail's vtable — and writes exactly one field, `_path` at **+0x08**.
It never writes +0x00 and never dispatches through it.

Two consequences worth stating rather than leaving implicit:

* Promoting `??_7External@Object@@6B@` or `??_7CompiledExternal@Object@@6B@`
  cannot change the behaviour of anything this lane ported. These four rows are
  not a reason to promote and not a reason not to.
* The virtual dispatch retail *does* perform on this path (`Add` and
  `External::Load` both drive slot **+0x28**, `Serialize`) happens **inside
  bodies we did not port** and which we reach by name. Those run the DLL's code
  on DLL-constructed objects, which is self-consistent: there is no mixed pair
  in which one half is ours and the other retail's. Should a later lane port
  `Add` or `Load`, that is where the vtable question becomes live — not here.

---

## 10. Four instrument defects, and the one cause underneath them

Both were written by this lane, both were found by this lane, and both would
have produced a **green line with no comparison behind it**. Recorded here
because the failure mode is the same one other lanes of this wave hit
independently, which makes it the reusable part.

### 10.1 A probe scheduled off a body that is never called

The probe originally self-scheduled from the first call into `PushPath` — the
obvious choice, and the one `array_internal.h` recommends in general ("the first
call into ANY registered entry point runs it").

**`PushPath` is never called on a normal run.** Lane RRES had already measured
it and written the number down: `stack empty/nonempty = 22583/0` — 22,583 live
`ExpandRelativePath` calls in a menu-to-gameplay run, every one against an EMPTY
stack. Nothing pushes.

The registration line would have printed `probe=ARMED`. The probe would never
have run. There would have been no `[PathStack] PROBE` line in the log at all —
and an absent line is exactly what a reader skims past, because a *failing*
probe prints and a *skipped* probe prints, but an *unreached* one is silent.

The gate now hangs off `FindOrLoad`, which is what drives those 22,583 calls,
and which additionally holds the live `ExternalFactory` the probe's part 2
needs. **The fix and the requirement came from the same fact.**

Generalisation: *scheduling an instrument off a body is a claim that the body
runs.* That claim is measurable and was already measured here, in another lane's
counters, by someone who had no idea it would be load-bearing for this one.

### 10.2 A negative control that would have crashed instead of diverging

Control 4 was originally "SetSize ignores the fill" — pass `nullptr` where
retail passes the entry. It reads like the sharpest possible perturbation.

With `fill == nullptr` and `spec->init == nullptr`, retail's else-arm is empty
and **the new tail is left UNINITIALISED**. The probe's comparator reads
`element.directory.data` as a `char*` and prints it. On a fresh `MemAlloc` block
that is a garbage pointer, so the control would have taken the process down
inside the probe rather than reporting `control CAUGHT`.

A crash is not a caught control. It is an unusable instrument that also destroys
the run carrying the other three.

Control 4 is now "grow by 2 instead of 1", which keeps every element initialised
(`SetSizeCore` assigns the fill to every element in `[oldSize, newSize)`) and
diverges in the **shape**, which is compared. The genuinely-uninitialised arm is
therefore **not exercised by this lane**, and that is stated rather than papered
over: reading it at all is a wild dereference, so it cannot be compared, only
reasoned about.

Generalisation: *a negative control has to survive being wrong.* It must produce
an observable, comparable divergence — not undefined behaviour. "Did this
perturbation change a value the comparator reads?" is the question; "is this
perturbation a real defect?" is not sufficient.

### 10.3 A control that CAUGHT — for a mechanism that could not apply

`ctl=4` meant "grow by two". Part 2 drives `PushPath`, which always grows by
exactly one, so the control **cannot reach part 2 at all**. It nonetheless
reported `control CAUGHT` there.

It caught because a **stale second implementation** of the same control was
still live in `external_path_stack.cpp:375`, passing `fill = nullptr` — the
control's ORIGINAL meaning, retired in the probe and in two comment blocks and
left behind in the body. So the element `PushPath` pushed was uninitialised and
the readback differed for a reason unrelated to the control's stated meaning.
That same stale line took the run down in `ucrtbase` a moment later, during
part 1's `ctl=4`.

**The green cell was withdrawn.** Both it and the probe's control table — which
claimed `ctl 4 -> PART 1 and PART 2` — were wrong, and the table's "which
screen can this control reach" column is itself a claim that has to be checked.

> **A control that catches must be shown to catch VIA THE MECHANISM IT NAMES,
> not merely to catch.** A control passing for the wrong reason is worse than
> one failing, because it reads as coverage.

This is now mechanical rather than a matter of noticing. Each control
increments an **attendance counter at the perturbation site it names**, and a
catch with `att=0` prints as `CAUGHT BUT NOT VIA ITS OWN MECHANISM (attendance
0) -- NOT BANKED` instead of being credited. On the validation run every active
control had a non-zero attendance (§7.3).

### 10.4 A prior that contradicted its own derivation

Covered in §12. Its fix — making the prior an executable model rather than a
hand-written string — is the only one of these four that could have been caught
**without a run**, and the `disagree` column it produces is worth more than the
`mismatches` column beside it: `mismatches=0` says the port matches retail on
these inputs, while `disagree=0` says the MODEL of why each input is or is not
sensitive is also right. The second is the one that would notice if the sweep
were quietly testing nothing.

### 10.5 The pattern

**Three control defects in ONE instrument is an architecture signal, not three
slips.** The common cause is that **a control's meaning was expressible in more
than one place** — the probe, two comment blocks, and the body at
`external_path_stack.cpp:375` — so it could be changed in three and left stale
in the fourth, silently. More care does not fix that; structure does. Both
fixes are now in:

* each control is **one parameter consumed by the single implementation** of the
  operation it perturbs, so there is no second path to drift;
* each control **attends at the site it names**, so a stale implementation
  fails loudly (`att=0`, NOT BANKED) instead of catching for the wrong reason.

The wider shape, which §6.2's `ResolveRuntimeRaw`-not-`ResolveAddr` check shares:
**an instrument can be wrong in a way that makes it silent or vacuous rather
than wrong-looking.** Each defect has a one-line test:

| defect | the test |
|---|---|
| probe on an uncalled body | does the body it hangs off run? *count it, don't assume* |
| control that dereferences uninitialised memory | does the perturbed value get **read** by the comparator, and is it **valid**? |
| control that catches for the wrong reason | did the branch the control NAMES actually execute? |
| prior that contradicts its derivation | derive the prior, never transcribe it |
| oracle resolving through the port registry | does "theirs" reach the DLL, or has it come back to us? |

All five are answerable before a run. Four of them were not, here, and each cost
a launch of a contended game directory.

---

## 11. Two facts about callees, and the rule that produced both

### 11.1 `GetPathFilename`'s flag does not mean "strip the extension"

`?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z` (0x1800c6740). The backward
scan for `'.'` runs unconditionally and `length = dotIndex - 1` strips the
extension **whether or not the flag is set**. The flag is read in exactly one
place:

```c
if (dotIndex < 1) {              // no '.' at all, or one at index 0
    if (stripExt) { *retval = 0; return retval; }   // EMPTY
}                                // else: keep the whole leaf
```

So it means **"yield an EMPTY filename when the path has no extension"**. For
any path that has one, the flag is dead.

**How it was found, and why the route matters.** `ctl=4` caught on 1 of 6
inputs where the then-current model predicted 4-5. The available dispositions
were to caveat the ratio as thin coverage, or to enlarge the input set until it
went away. Both would have been wrong, and the second **provably** so: the
enlargement actually attempted first added five extension-BEARING paths, and
`ctl=4` can only bite on extension-less ones, so it would have moved nothing
while looking like diligence. That is the concrete argument against "enlarge
until the anomaly resolves" as a method.

The fact is confirmed by **two independent routes**, which is a better warrant
than either alone:

1. this lane's derivation from the disassembly, tested by the requirement that
   ONE fact explain all five of the first sweep's ratios exactly — 6/6, 6/6,
   4/6, 1/6, 5/6 — which it does;
2. `runtime/src/system/filename.cpp:282-288`, the tree's own port of that
   function, which had already written it down.

**The systemic finding is worth more than either.** This tree re-declares
callees locally by house style, to avoid dragging in headers. **A local
re-declaration carries the signature and drops every caveat attached to the real
port.** Here it did not merely fail to help: `external_path_win.cpp` declared
the parameter `bool stripExtension`, a confident-sounding wrong name two call
sites from code whose behaviour depends on the caveat being false.

> **Before deriving a callee's semantics from disassembly, grep for OUR port of
> that callee. A local re-declaration is a signature, not a source of truth.**

### 11.2 `GetPathIdentifer` returns the WHOLE path for a bare name

Found by applying that rule immediately to the other callees this lane uses —
`runtime/src/system/filename.cpp:366-370`:

> Returns everything after the last `':'`. If there is no `':'` at all the whole
> path comes back UNLESS a `'.'`, `'/'` or `'\'` was seen on the way, in which
> case the result is empty.

I had assumed "no `':'` means empty". True for every input in the set except
`"Solo"` — no `':'`, no `'.'`, no `'/'` — whose identifier is the whole string.

This was **sealed as a prediction before the run**
(`WAVE11_LANE_OBJEXT_SEALED_PREDICTION.md`, committed `f25669e`): the claim was
that the instrument would flag exactly one disagreement, at `ctl=5` index 10,
and none elsewhere. It flagged exactly that.

## 12. The unsealed disagreement, and what it cost

The same run also flagged `ctl=3` index 13, which was **not** sealed.

`"I meant to write X"` after seeing X is unfalsifiable, so the claim needs
evidence that predates the run. It has some: the comment beside that mask
enumerates its insensitive set as `(1,8 empty dir; 4,11,12 empty fname; 10
both)` — **six** indices — while the mask carries **seven** dots. The mask
contradicted its own stated derivation before anything ran, and index 13 is the
extra dot.

Verdict: **model right, transcription wrong.** Not upgraded to a held
prediction.

The fix is not more care. The prior is now an **executable model** (§11's three
predicates) rather than six hand-written strings, so a transcription slip is not
expressible, and the model computes the live path's prediction too instead of
relying on a guess about what game paths look like. On the validation run it
reproduced all 14 cells of all five controls — `disagree 0` throughout,
including index 13 and index 10, the two cells the hand-written masks got wrong.

That is the only instrument defect in this lane that could have been caught
**without a run**, and it is now caught without one by construction.
