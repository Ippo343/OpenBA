# WAVE 27 LANE FRONTIER — the live cutover frontier

Branch `wave27/frontier`, cut from `wave27/base`. Two items, no tooling.

---

## 0. THE ONE THING TO READ IF YOU READ NOTHING ELSE

**Every disassembly this lane took in its first hour came from the WRONG BINARY,
and three separate MCP tools said otherwise.** `list_open_programs` reported
`Runtime-x64.dll` with `"is_current": true` and `"current_program":
"Runtime-x64.dll"`; `get_metadata` reported `Runtime-x64-profile.dll`;
`switch_program("Runtime-x64.dll")` returned `{"success": true, "switched_to":
"Runtime-x64.dll"}` and **changed nothing**. Only passing
`program="Runtime-x64.dll"` explicitly to each call reached retail.

It was caught by an unrelated accident (see §4), not by any check this lane had
planned, and **the profile build's bodies at these addresses happened to be
identical**, so no downstream number was wrong. That is luck, not a control.
Details and the discriminator in §5.

**If you take Ghidra disassembly in Wave 28, pass `program=` on every call.**

---

## 1. WHAT MOVED

Measured with `tools/runtime_census.py` on this branch tip vs `wave27/base`:

```
  wave27/base    1372 called / 1237 ported   90.2%   gap 135
  wave27/frontier 1378 called / 1241 ported  90.1%   gap 137
```

**THE PERCENTAGE WENT DOWN AND THAT IS THE EXPECTED DIRECTION.** `called` grew
by 6 and `ported` by 4, because a faithful port of a body that calls DLL
internals by address ENLARGES the cutover surface
(`reference_porting_can_grow_the_cutover_surface.md`), and one of the doors this
lane opened is deliberately left unported (§3.3). Do not read 90.2 -> 90.1 as a
regression, and do not read it as progress either — score the identities below.

### Ported and registered (8 addresses, 2 of them exported names too)

| address | what | file |
|---|---|---|
| `0x18002c6a0` | `Array<Object::ExternalArrayElement>::SetSize` — **the attempt-29 wall** | `runtime/src/collection/array.cpp` |
| `0x180006ac0` | its element range-destroy (Body B, `kD_Str_10`) | `runtime/src/collection/array_destroy.cpp` |
| `0x180006b80` | `~Array<ExternalArrayElement>` (J2) | same |
| `0x180078c90` | `Array<UI::StringTable::TextEntry>::SetSize` | `array.cpp` |
| `0x180005570` | its element range-destroy (Body B, **new** `kD_Str2_10`) | `array_destroy.cpp` |
| `0x180004a90` | `~Array<TextEntry>` (J2) | same |
| `0x180002740` | `??_7StringTable@UI@@6B@` **slot 0**, the scalar/vector deleting dtor | `runtime/src/ui/stringtable_factory.cpp` |
| `0x18011d940` | `?Serialize@StringTable@UI@@` (**slot 5**) — also by name | same |

`??_7StringTable@UI@@6B@` is therefore **9/9 on the static screen**, up from 7/9.
It is **NOT promoted** — see the residual in §6.1.

---

## 2. ITEM 1 — `Array<ExternalArrayElement>::SetSize`, `0x18002c6a0`

### 2.1 The Wave 26 warning: the CONCLUSION held, the PREMISE did not

WAVE26_CLOSED.md §6 item 1 said, load-bearing:

> the three ported sibling instantiations (`Array<bool>`, `Array<Time>`,
> `Array<InternalPtr>` in `achievement_manager.cpp`) are the **WRONG TEMPLATE**.
> They are POD-element arrays using realloc+memcpy; this element carries a
> refcounted `System::String` … **A port following the sibling recipe compiles,
> runs, and leaks or double-releases a refcount, and no byte oracle on `SetSize`
> itself would catch it.**

**MEASURED verdict: the conclusion is correct and the mechanism it describes is
real — and the reason given for it is wrong in a way that would have cost a lane
a week.** Those three siblings are **not a separate realloc+memcpy template.**
They are three `kSpec_*` rows of the *same* `SetSizeCore` this row now joins
(`runtime/src/collection/array.cpp`, Wave 9 lane Q: one algorithm, 27
registered addresses, parameterised by an `ArraySpec`). The machinery for a
non-trivial element — an `assign` that is a CALL, and a `destroy` that runs on
**both** the realloc arm and the shrink arm — has existed in that file since
Wave 9 and already serves seven rows.

So: **the danger was real, the remedy was already written, and the warning
pointed at "write a new template" when the correct action was "add one row".**
The three siblings are the wrong ROWS, not the wrong TEMPLATE.

This is the Wave 26 §3.4 shape inverted. A correct prediction with a wrong
mechanism is still much better than no prediction: the *warning* is what made
this lane read all three siblings before writing a line.

### 2.2 The growth policy (WAVE26_CLOSED said what it is NOT)

> "Growth is reportedly not `capacity*=2`" — correct, and it never said what it
> **is**. Read from `0x18002c6ed..0x18002c703`:

```
    capacity = newSize
    if (newSize == oldSize + 1 && oldSize != 0) capacity = newSize * 3
```

(`LEA EAX,[R13+0x1]`, `CMP`, `TEST`, `LEA EAX,[R15+R15*0x2]`.) **This is not a
property of this instantiation.** It is the family rule `SetSizeCore` already
implements for all 27 siblings, verbatim, including the 32-bit wrap. The row
inherits it and adds nothing. Anybody re-deriving "the growth policy" per
address is re-deriving a constant.

### 2.3 The element, and the two helpers

Signature: **TWO arguments.** `MOV dword ptr [RSP+0x10],EDX` at `0x18002c6a0`
homes arg2 and **nothing homes or reads R8**. Registering it behind the
three-argument typedef would dereference a register the caller never set.
(`external_load.cpp:157` calls it with two arguments — agreeing, independently.)

Stride `0x10` (`SHL RCX,0x4` at `0x18002c70a`; `ADD RDI,0x10`; and the
shrink-tail base `ADD R12,R12 ; LEA RDX,[RDI+R12*0x8]`). Align 8 (`MOV EDX,0x8`).

| arm | what retail does | address |
|---|---|---|
| assign | `CALL 0x1800067d0` (`System::Filename` copy ctor) then `MOV RAX,[RBX+8] ; MOV [R14+8],RAX` | `0x18002c773`, `0x18002c779` |
| init | two zero qwords — whole stride covered | `0x18002c79a`, `0x18002c79d` |
| destroy | `CALL 0x180006ac0` on **both** arms | `0x18002c7d4` (realloc, `n=oldSize`), `0x18002c82b` (shrink-tail) |

**`0x1800067d0` is where the sibling warning's mechanism actually bites, and the
trap is one the project has now sprung five times.** That function's LAST TWO
ACTIONS are two calls to `Filename::AdjustDelimiters` (`0x18000682a`,
`0x180006832`). An element assign hand-written as the familiar "null the slot,
adopt, incref" idiom is byte-identical for the first eight bytes and leaves every
dependency PATH with its separators unnormalised — which breaks lookup for any
dependency spelled with forward slashes and breaks **nothing** otherwise. It
passes a green run. `AssignPkgEntry` in `array.cpp` records the fourth occurrence
of exactly this; the comment there was re-read from the callee for this row
rather than inherited.

### 2.4 `0x180006ac0` is Body B, and that is MEASURED

Set beside `0x180005930` (already `B_ENTRY(…, kD_Str_10)`), both from retail:
identical instruction for instruction **and in every resolved absolute operand**
(`0x1801fc718`, `0x18014b390`, `0x18014b388`, `0x1800c6c00`). The two
differences are exactly the pair `array_destroy.cpp` already tabulates between
`0x180026b50` and `0x180005930`, and no caller can observe either:

* `0x180006af2` `NOP dword ptr [RAX+RAX*1]` — alignment padding
* `0x180006b05` a **duplicated** `MOV [RSP+0x50],RBX` into this body's own SEH
  scope slot (`0x180006b00` makes it first, with the same value)

Its spec was read from **this** body (`ADD RBX,0x10` = stride, `MOV RAX,[RBX]` =
slot at +0x00), not from the resemblance: `kD_Str_10`, no new row.

### 2.5 The door comment, closed in place

`runtime/src/object/external_load.cpp:147` predicted this failure, naming its
SYMBOL, its CONDITION and its CONSEQUENCE at the call site. It has been updated
**in place** (not in a doc) to say the prediction came true, what supplied the
body, and **that the door is still a door**: it resolves through the port
registry, so it is only as good as three registrations that live behind
`BANISHED_RT_COLLECTION` / `BANISHED_RT_ARRAY_DESTROY`. With either gate off at
cutover, that call site is an EXEC-of-0 again **at the same instruction**, and
the crash log will be indistinguishable from attempt 29's.

---

## 3. ITEM 2 — `??_7StringTable@UI@@6B@` from 7/9 to 9/9

### 3.1 The handover's slot-0 address was RIGHT and Ghidra disagreed with it

WAVE26_CLOSED.md §5 names slot 0 as `0x180002740`. Ghidra's analysis of retail
places that address **mid-instruction** inside a neighbouring function — it is
not even an instruction boundary there. That reads exactly like a stale address
in a handover, and acting on that reading would have been the wrong move.

Settled by going to the **binary itself**: `pefile`, retail
`Runtime-x64.dll`, RVA `0x154630` (the export address of
`??_7StringTable@UI@@6B@`, ordinal 1507):

```
+0x00 0x180002740   +0x08 0x180002190   +0x10 0x180002170
+0x18 0x18011d930   +0x20 0x1800066e0   +0x28 0x18011d940
+0x30 0x180002390   +0x38 0x180002460   +0x40 0x180002400
```

which is `kVSlots52` in `standalone/rt_data_mint_tables.inc`, entry for entry.
**The mint is right, the handover is right, Ghidra's function boundaries are
wrong at this address.** Recorded because the next reader will hit the same
disagreement and the cheap instinct is to distrust the handover.

### 3.2 Slot 0 — the scalar/vector deleting destructor

Transcribed from `0x180002740..0x1800027cd`. `flag` is EDX; bit 1 = array block,
bit 0 = also free the storage.

The **scalar arm is `??1StringTable@UI@@UEAA@XZ` (`0x18011d8f0`) INLINED** — the
two were diffed instruction by instruction, not assumed equal from length: the
same `LEA [0x180154630] / MOV [RCX],RAX / ADD RCX,0x20 / CALL 0x180004a90`
sequence, where the exported dtor then tail-JMPs to `~External` (`0x1800bc950`)
and slot 0 CALLs it and falls through to the free. **The port calls the exported
dtor** rather than re-transcribing its three steps, so this slot follows a future
promotion of `??1StringTable@UI@@` instead of freezing today's body — the same
reasoning FACTORY used for `?Destruct@`.

`0x180002220` (`??3StringTable@UI@@SAXPEAX@Z`) is a null-guarded `MemFree`; it is
**exported**, so it is reached by NAME. `SAXPEAX` is an access specifier, not
evidence of non-export (`reference_aeaa_is_private_not_unexported.md`).

Slot 0 itself is **address-only**, and that was CHECKED rather than assumed: the
retail export directory's 30 `StringTable@UI` symbols (dumped with `pefile`)
contain no `??_G` or `??_E` entry for this class.

### 3.3 The one door this lane opened and did NOT close

```
SYMBOL:      0x18007ec98 — MSVC's `eh vector destructor iterator`
CONDITION:   nothing in this tree registers it. Grepped across runtime/ code/
             standalone/ engine/ dx11/: it appears only in COMMENTS
             (array_destroy.cpp, io/binary_writer.cpp).
CONSEQUENCE: at cutover, any call reaching slot 0's VECTOR arm is an EXEC-of-0
             at RtUiStringTable_ScalarDelete — the same failure attempt 29 took
             one level up.
```

It is an address door rather than a hand-rolled loop because the iteration ORDER
and the EH semantics are the entire content of that helper and **this lane did
not read it**. The arm is *believed* unreachable — `UI::StringTable` is created
only through `?CreateNewObject@`, a single-object `MemAlloc(0x30, 8)` with no
count header — but "believed unreachable" is why there is a **counter** and a
printed warning rather than an assertion: `StringTableSlotsReport()` prints
`THE VECTOR ARM RAN` and names the door if `g_slot0Vector != 0`.

**ADDRESSEE:** the lane that ports MSVC's `eh_vector` helpers, **or** whichever
cutover attempt first reports an EXEC-of-0 at `RtUiStringTable_ScalarDelete`.
The comment sits at the call site in `stringtable_factory.cpp`, in the §4 item 10
shape.

### 3.4 Slot 5 — `?Serialize@StringTable@UI@@`, and the three rows it dragged in

`Serialize` is the same 16-byte `IO::Serializer` facade
`runtime/src/object/external_load.cpp` documents at length (a `{Stream*, impl}`
pair on the stack, handed to the `IO::Encoding`'s factory with only the stream
filled in; `+0x18` reading, `+0x20` writing). **That file's account was used to
SHAPE this one; every offset below was re-read from `0x18011d940`'s own
listing.**

Two things that are easy to get wrong and were read, not assumed:

* the loop bound is the **LOCAL** count at `[RSP+0x50]` (`MOV EAX,[RSP+0x50]` at
  `0x18011d9c0`), not `self->size` re-read;
* the element cursor is **latched before the loop** (`MOV RBX,[RBX+0x20]` at
  `0x18011d9bc`), so a reallocation inside the loop would not be seen. Retail's
  own choice, transcribed.

The return is **unconditionally true** — there is no failure path. Recorded
because "a `Serialize` returning `bool`" invites a guess.

On the READ path it calls `0x180078c90`,
`Array<UI::StringTable::TextEntry>::SetSize` — **which was unported**. Porting
`Serialize` without it would have converted a 7/9 vtable gap into a fresh
EXEC-of-0 one level deeper, i.e. reproduced attempt 29 exactly. So this lane
ported that row too, and the row it needs, and the `~Array` above it:

* `kSpec_StringPair10` (`0x180078c90`) — stride `0x10`, align 8, two-argument,
  `InitZero10`, assign = the refcount-adopt idiom run TWICE (INLINED in retail,
  `0x180078d33..0x180078d55`), destroy on both arms.
* `kD_Str2_10` (`0x180005570`) — **the first two-slot Body-B row at stride
  0x10.** Every previous Body-B row at that stride (`kD_Str_10`: `0x180005930`,
  `0x180026b50`, and now `0x180006ac0`) releases ONE slot at +0x00; this one
  releases TWO. **Sharing `kD_Str_10` by stride would have leaked every
  TextEntry's `text` String, silently, with a clean build** — the fifth time this
  family has offered a wrong answer by resemblance. Read from its own body:
  `0x1800055a7` releases +0x08 FIRST, `0x180005600` releases +0x00 second, `ADD
  RBX,0x10` is the stride. The DESCENDING order is enforced by a `static_assert`,
  as `kD_Str2_38` already does.
* `J2_ENTRY(180004a90, …, kD_Str2_10)` — `~Array<TextEntry>`, what
  `??1StringTable@UI@@` calls on `this+0x20`. Its **CALL operand** is what picks
  the spec, which is the Wave 25 lesson three J2 rows in that file already carry:
  an address-normalised hash erases exactly that operand.

**Note the order difference and that it is not a contradiction:** the assign
walks +0x00 then +0x08, the destroy walks +0x08 then +0x00. Each was read from
its own body.

### 3.5 Three specs that differ in ONE property

`kSpec_ExtStr10at08`, `kSpec_ExtArrayElem` and `kSpec_StringPair10` are all
stride `0x10`, align 8, `InitZero10`, non-trivial, with a destroy on both arms —
every property a reader would use to conclude they are one row. They differ only
in **which slots carry a refcounted String**: `+0x08` only, `+0x00` only, and
**both**. Each wrong pairing leaks or double-releases exactly one refcount, and
no byte oracle on `SetSize` would see it. This is the concrete form of the Wave
26 warning and it is written beside all three rows.

---

## 4. A SECOND LIVE COPY, LEFT DELIBERATELY

`runtime/src/object/external.cpp`'s `RtObject_External_Dtor` hand-inlines Body B
over `_refs`. Retail's `~External` (`0x1800bc950`) does **not** inline it — it
`CALL`s `0x180006b80` at `0x1800bc974` — so as of this commit there are two live
copies of that logic, one of them behind a newly registered address. That is the
shadowed-body shape `array_destroy.cpp` records against lane UIW.

**It is left as is, and the reason is not taste:** the registered body sits
behind `BANISHED_RT_COLLECTION` + `BANISHED_RT_ARRAY_DESTROY`, so routing this
dtor through the registry would make it an EXEC-of-0 at cutover whenever either
gate is off — strictly worse than a duplicate. The two are behaviourally
identical (Body B, stride `0x10`, slot at +0x00). A comment at that function says
so, and names its **ADDRESSEE: whoever arms the collection gates
unconditionally.**

---

## 5. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Per WAVE27_KICKOFF.md §4 item 1. None of these was touched.

### 5.1 GhidraMCP: "current program" is a lie, three ways

* `tools/ghidra_mcp/bridge_mcp_ghidra.py` — `list_open_programs` reports
  `"current_program": "Runtime-x64.dll"` and `"is_current": true` on that entry,
  while `get_metadata` (same session, no calls between) reports
  `Runtime-x64-profile.dll`. **Two tools, one field, opposite answers.**
* `switch_program("Runtime-x64.dll")` returns `{"success": true, "switched_to":
  "Runtime-x64.dll"}` and **subsequent `get_metadata` and `disassemble_function`
  still serve the profile DLL.** A success return that changed nothing.
* Passing `program="Runtime-x64.dll"` explicitly to each call **works**.

This is the same family as
`reference_ghidra_mcp_address_param_silently_ignored.md` (a parameter accepted
and silently ignored) — here it is a parameter's DEFAULT that is silently wrong,
which is worse, because omitting a parameter does not feel like a decision.

**HOW IT WAS CAUGHT, honestly: by accident.** Not by any planned check. The
profile DLL's `.text` extends past `0x180154630`, so a memory read of the
StringTable vtable returned *code* where the PE has *pointers*. Chasing that
inconsistency produced the `get_metadata` call that exposed it. Had the address
been anywhere else, this lane would have shipped profile-derived numbers.

**The discriminator, for whoever fixes it:** `disassemble_function(0x180002190)`
is a 2-instruction accessor (`MOV RAX,[0x1801fae10] ; RET` —
`?TypeName@StringTable@UI@@`) in retail, and a 40-instruction comparison
function in the profile build. One call tells you which binary you are reading.

**Mitigating fact, measured not assumed:** all five bodies this lane derived
before noticing (`0x18002c6a0`, `0x1800067d0`, `0x180006ac0`, `0x180005930`,
`0x180006b80`) were re-taken with `program="Runtime-x64.dll"` and are
**identical, instruction for instruction and in every resolved operand**, to the
profile listings. No number in §2 is profile-derived. **That is luck.**

### 5.2 `list_segments` contradicts the PE

`list_segments` reports `.text: 180001000 - 180198bff` and `.rdata: 180199000 -
1802517ff`. The retail PE's own headers say `.text` RVA `0x1000` size
`0x1495f1` (ends `0x18014a5f1`) and `.rdata` RVA `0x14b000` size `0xaaa7f`. On
the reported bounds, `0x180154630` — an `.rdata` vtable — is "in `.text`". This
is downstream of §5.1 (the figures are the profile DLL's) but it is worth its own
line, because a section-bounds answer is exactly the kind of thing a reader takes
as ground truth without a second source.

---

## 6. RESIDUALS, EACH WITH A NAMED ADDRESSEE

### 6.1 `??_7StringTable@UI@@6B@` is 9/9 and NOT PROMOTED

This lane did **not** establish that it owns the function that STAMPS the table,
and per WAVE27_KICKOFF.md §1 ("COMPLETE is the STATIC half of readiness only… a
green readiness screen is not a route") that is what promotion turns on. The row
is left **COMPLETE-but-unpromoted**, as the brief instructed.

What is known: `runtime/src/ui/stringtable_factory.cpp`'s `ConstructBody` stamps
the vptr through `RT_DATA_SLOT("??_7StringTable@UI@@6B@")`, and all three of the
ctor / `CreateNewObject` / `CreateInPlaceObject` entry points that call it are
registered by FACTORY. What is NOT known is whether those are the ONLY stampers
in the DLL. **`tools/mint_stamper_census.py` is the tool that answers this, and
WAVE26_CLOSED.md §5 says every stamper ratio it printed in Wave 26 came from a
DEFECTIVE version — verdicts survive only where `unattributed` is 0, a per-row
check.** This lane did not run it, because running it is not the same as trusting
it and re-deriving that belongs with whoever owns the tool.

**ADDRESSEE: the Wave 28 promotion lane (whichever lane inherits
`reg_mint_promoted.cpp`).** **CONDITION:** a `mint_stamper_census.py` run on the
post-fix tool whose `unattributed` count for row 52 is 0. If every stamper is
ours, promote; the slots are all there.

### 6.2 No probe drives `kSpec_ExtArrayElem` or `kSpec_StringPair10`

`array_probe.cpp` drives the other rows against the SHIPPED body through
`SetSizeCore` on synthetic subjects; both new rows are declared in
`array_internal.h` specifically so it can, and **neither has a case.** The live
checks in `SetSizeCore` are weak by construction and the `byteCopy` gate turns
the memcmp one off for both (correctly — neither assign is a pure byte copy), so
what a run gives today is `calls=` and nothing more.

Concretely unverified: that the assign's refcount arithmetic is right. This is
**exactly** the hole WAVE26_CLOSED.md §6 item 1 warned about, and porting the
row did not close it — the warning was about the ORACLE, and only the BODY
landed.

**ADDRESSEE: the Wave 28 lane that owns `runtime/src/collection/`.**
**CONDITION:** two `array_probe.cpp` cases, one per row, seeded per each row's
`seedZero` window (`0x00..0x08` and `0x00..0x10`) — the seeding is the whole
point, since all-zero elements are the one seed under which a refcount bug is
invisible.

### 6.3 Attendance is unmeasured for everything this lane registered

`ArraySetSize` in `external_load.cpp` is a live path (resource loading), so
`0x18002c6a0` should have real attendance — but this lane's smoke run is a
functional check, not an attendance measurement, and the `RtDiff` sites are off
unless `BANISHED_RT_DIFF` is armed. Slot 0 and `Serialize` are expected to read
**zero** with the DLL present, for the reason FACTORY's banner gives (the DLL
reaches its own vtable by in-module RIP-relative stamp, which is not a door) —
and a zero there must not be read as "dead".

**ADDRESSEE: the integrator, at cutover attempt 30.** The counters and the
`[RtPort] UI::StringTable slot ATTENDANCE` line exist so that run can answer it.

### 6.4 The `0x18007ec98` door — see §3.3, addressee named there.

---

## 7. GATES RUN

```
mingw32-make standalone-debug          exit 0   (twice: after item 1, after item 2)
tools/game_dir_lock.sh acquire         ACQUIRED by FRONTIER (not piped, not backgrounded)
tools/game_dir_lock.sh assert-held     verified, deployed exe == this build
```

`assert-held` **refused first**, correctly: the deployed exe was another lane's.
Deploy, re-assert, then launch — that is the sequence, and the refusal is the
tool working.

---

## 8. WILL ATTEMPT 30 CLEAR THIS WALL?

**It will clear `ArraySetSize+0x4d`. It will almost certainly report a new
name.** Stated as a prediction, with its reasoning, so it can be scored:

* The wall itself is gone in the only sense a cutover gate measures: the address
  now resolves to a registered body, and so do both of its callees.
* `Object::External::Load` is a **full deserialisation driver** whose own banner
  lists nine steps, and attempt 29 died at step 7. Steps 8 and 9 — the object's
  own `Serialize` at vtable `+0x28`, and the three-level inlined destructor
  chain — are past the point any attempt has reached. Each is a fresh population
  of doors.
* **A cutover boot is a GATE, not a PLANNER**
  (`reference_the_boot_is_a_gate_not_a_planner.md`). Attempts 28 and 29 each
  named exactly ONE symbol. There is no reason to expect attempt 30 to name
  zero, and if it names one, that is the gate working, not this lane failing.

The honest summary: **one wall down, the corridor behind it unmeasured.**
