# WAVE 11 follow-up 5 — lane TINFO: the last three Object rows

Worktree `C:/code/GIT/wt-w11f-tinfo`, branch `wave11f/tinfo`, cut from `9bb2dfa`.
Every number below was measured on this branch, debug build, DLL present, under
`tools/game_dir_lock.sh acquire tinfo` (held for the whole run batch, released
before the write-up; `assert-held tinfo` gated every deploy).

## 0. Verdict table

| row | verdict | the measurement behind it |
|---|---|---|
| `??_7Internal@Object@@6B@` | **READY-BUT-UNPROMOTABLE (NO-OP)** — tagged | the amendment's gates (a) and (b) are **mutually exclusive**; and `entries=0` even in the half where 124,395 objects really carried the minted table (§2) |
| `?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A` | **READY-BUT-UNPROMOTABLE (NO-OP)** — tagged | factory trap unreachable: 0 of 102,454 dispatches through a minted node, subject type created 0 times; and its only consumers never run (§3) |
| `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A` | **READY-BUT-UNPROMOTABLE (NO-OP)** — tagged | same two measurements, same mechanism (§3) |

**Zero rows promoted, and Object is now AT CEILING.** The ceiling moved
**88.5% → 83.6%** because these three rows are *proved unpromotable*, not
unfinished — the same correction the UI namespace made at its close. The
census reads `Object 191 61 51 83.6% 83.6% AT CEILING`, and the set-diff is
**zero entered, zero left** (§5).

> **The brief asked whether the ceiling is reachable by this lane alone. It is
> not, and the reason is not porting work: all three rows fail for ONE
> mechanism.** Their only consumers are vtable slots that nothing in this tree
> can dispatch, because **we intercept only at doors and an in-DLL vtable stamp
> is not a door** (`reference_a_ported_row_can_be_permanently_unpromotable.md`).
> Porting the two `CreateNewObject`/`CreateInPlaceObject` factories would not
> have changed a single one of these verdicts — see §3.

## 1. The safety question the brief said dominates the lane — answered, and the brief was wrong about the count

**Question 1 (does `External::Load`'s type lookup ever select these types?) and
question 2 (is there any other `factoryFn` caller?) turned out to be one
question, and the brief's premise for question 2 is false.**

The brief said *"There was none before follow-up 2; verify rather than assume."*
A grep for `factoryFn` / `inPlaceFactoryFn` finds **three** dispatch sites in
this tree, and two of them **predate follow-up 2**:

```
runtime/src/object/external_load.cpp:351   Object::External::Load                (follow-up 2)
runtime/src/object/type_system.cpp:352     Object::Factory::CreateObject         (Wave 9 lane A)
runtime/src/ui/dialog_create.cpp:551       UI::DialogInstance::AddControllerTab  (Wave 10)
```

All three are instrumented by the new
`runtime/src/object/factory_dispatch_probe.{h,cpp}`, which records every
dispatch and classifies the `BaseTypeInfo*` as MINTED (inside
`kTypeInfoStorage[104]`), DLL, or OTHER (our image — the 312 exe types
registered by `code/Game/Object/factory_registration.cpp`). Autostart →
gameplay → pause-menu quit, `rc=0`, empty crash log:

```
[FactoryDispatch] Object::External::Load                dispatches=2847    minted=0 dll=828   other=2019 null=0
[FactoryDispatch] Object::Factory::CreateObject         dispatches=100067  minted=0 dll=0     other=100067 null=0
[FactoryDispatch] UI::DialogInstance::AddControllerTab  dispatches=0  -- SITE DID NOT RUN on this route; its minted=0 is NOT evidence
[FactoryDispatch] CONTROL C1 (predicate) minted-node=TRUE dll-node=FALSE  -> GREEN
[FactoryDispatch] CONTROL C2 (liveness) total dispatches=102914 -> GREEN
[FactoryDispatch] VERDICT: 0 of 102914 dispatches went through a MINTED BaseTypeInfo
[FactoryDispatch] SUBJECT type "CompiledExternal" created through a factory pointer: 0 time(s)
[FactoryDispatch] SUBJECT type "Internal"         created through a factory pointer: 0 time(s)
```

A `BANISHED_AUTOLOAD` run with **all three subject rows armed** gives the same
answer on a completely different mix (`Load` 2849, `CreateObject` **3** — the
world comes out of the save rather than being built): `0 of 2852 minted`,
both subjects `0`.

**Three controls, scored separately, and one of them is why the name list can
be trusted:**

* **C1 — the predicate can say YES and NO.** `RtMintIsMintedTypeInfo` is asked
  about `&kTypeInfoStorage[20]` (must be TRUE) and about the DLL's own node for
  the same row (must be FALSE). GREEN. Without it, `minted=0` cannot be told
  from a predicate stuck on `false`.
* **C2 — liveness, per site.** A site with `dispatches=0` says so in those
  words: `AddControllerTab` never ran on either route, and its `minted=0` is
  reported as **not evidence** rather than folded into a clean-looking total.
* **The SUBJECT counters are cap-immune.** The probe's first run filled a
  96-entry name table and dropped 102,486 dispatches, which left *the one
  question the probe exists to answer* unanswerable from that list — a dropped
  name reads exactly like a name that never appeared. The cap is now 512 (163
  distinct names actually appear) **and** the two subject types are counted by a
  dedicated pair of counters that never consults the table.

**Why the answer is structural, not luck.** All three sites reach `info` the
same way: walk `?_singleton@Factory@Object@@`'s AVL tree by typeId and read
`TypeNode+0x10`. None resolves a `?_typeInfo@` symbol by name. The minted nodes
hang off `g_mintTypeInfoHead`, a private chain no `Factory::Startup` walks
(`standalone/rt_data_mint.cpp`), so a promotion cannot put one in front of a
dispatch. The mint's own banner already asserted this
(*"reachable only through a Factory tree our minted node is not in"*); this lane
turned the assertion into a measurement, which is what follow-up 2's binding
rule asked for.

**`tiFactoriesTrapped`, as instructed:** both rows are `fac=2traps` on the
readiness screen, and that is now printed for armed and promoted rows too (§4).
The trap count is **not** a blocker for these two rows, because the count
describes a slot nothing reaches — but it is exactly why neither row should be
promoted on a readiness screen alone.

## 2. `??_7Internal@Object@@6B@` — the amendment's two gates cannot both hold

The advisor's amendment (WAVE11.md, `1aed90e`) is binding:

> may be promoted only when BOTH hold: (a) `[ObjTI] ATTENDANCE … entries>0`, and
> (b) `VtblInternal()`/`VtblBase()` pin to the RAW DLL symbol while the DLL is
> present. **Attendance alone is NOT sufficient.**

### 2.1 Condition (b) is implemented, and the hazard it prevents was live

`runtime/src/object/internal.cpp` now pins: `VtblInternal()` / `VtblBase()` take
the DLL's own table while the DLL is present and fall back to the registry only
when it is absent (at cutover `ResolveExportRaw` returns null and the minted
table is the only one there is, so the same code is correct on both sides).
Kill switch `BANISHED_RT_INTERNAL_VTBL_RAW=0` restores the old registry-first
behaviour so the hazard is reproducible with one variable.

A new always-on exit screen makes the condition checkable from a log, and it is
a **gate rather than a tally by this project's own test — the quantity VARIES
with the thing it gates.** Both halves have the minted row armed
(`BANISHED_RT_MINT_ROWS=??_7Internal@Object@@6B@`), TABLE tier armed by default:

| | `BANISHED_RT_INTERNAL_VTBL_RAW=0` | default (pin armed) |
|---|---|---|
| `[RtInternalVtbl] ??_7Internal` | `stamped=…fc0 dll=…720` → **MINTED (SPLIT)** | `stamped=…720 dll=…720` → **DLL, no split** |
| `[RtInternalVtbl] ??_7Base` | DLL (Base is unregistered) | DLL |
| `[RtInternalTable] ctorRuns` | **124,395** | 124,680 |
| rc / crash log | 0 / empty | 0 / empty |

So the hazard was **live, not theoretical**: 124,395 objects a boot carrying a
different vtable from the ones the DLL built, on one class — PLAN.md §4.3
risk 3 verbatim. It is now closed by construction.

### 2.2 And that is exactly what makes condition (a) unreachable

The only thing that could ever make `[ObjTI] ATTENDANCE ??_7Internal` move on a
DLL-present run is **our own ctor stamping the minted table** — a virtual call
through the DLL's table at `0x180154720` goes to the DLL's own accessor address
and never consults our registry. **Gate (b) is precisely the change that stops
our ctor doing that.** The two conditions are therefore mutually exclusive:
satisfy (b) and (a) can never fire; satisfy (a) and (b) is violated.

This is not a reason to skip (b). The measurement settles the row independently:

```
[ObjTI] ATTENDANCE ??_7Internal@Object@@6B@   entries=0
```

in **both** halves of the A/B above — *including the half where 124,395 live
objects really did carry the minted table*. Nothing dispatches slots 2 or 3 of
that table on any route this project can drive, split or not.

**The zero is the instrument's answer, not its silence.** The ObjTI positive
control was armed (`BANISHED_OBJTI_PROBE=1`) on a run with all three rows armed:

```
[ObjTI] CONTROL screen0(registry-live) = GREEN  (promoted ??_7Element@UI@@6B@, doors differ)
[ObjTI] CONTROL Base             row=unregistered(DLL)      screen1=GREEN d_entries=1  screen2=GREEN d_dll=1
[ObjTI] CONTROL CompiledExternal row=unregistered(DLL)      screen1=GREEN d_entries=2  screen2=GREEN d_dll=2
[ObjTI] CONTROL External         row=unregistered(DLL)      screen1=GREEN d_entries=2  screen2=GREEN d_dll=2
[ObjTI] CONTROL Internal         row=REGISTERED(minted)     screen1=GREEN d_entries=2  screen2=GREEN d_mint=2
```

Screen 2 on the armed row reads `d_mint=2`, i.e. the classifier *does*
distinguish the minted table from the DLL's on the very row whose real
attendance is zero.

**Verdict: tagged `RT_UNPROMOTABLE … NO-OP`, expiry "NONE while the DLL is
present".** This reverses lane OBJDATA's deliberate decision to leave the row
untagged, and it does so on a measurement OBJDATA could not make (the TABLE tier
was not armed then, so its `entries=0` was consistent with "our ctor never ran";
now `ctorRuns=124,395` and the answer is still zero).

## 3. The two `?_typeInfo@` rows — safe, and still routeless

Both are **READY** on the exit-phase shadow — `id` equal to the loaded DLL's,
`strs=0`. (At `phase=init` both read `idcmp=NO`: the DLL still holds the ctor
sentinel before its own `Factory::Startup`, exactly as OBJDATA §5 recorded.
Reading the init snapshot as the verdict would have blocked both rows for
nothing.)

Their factory slots are abort traps, and §1 shows the traps are unreachable.
That clears follow-up 2's blocker. What does **not** clear is the route:

* **Consumers, by grep on the mangled name:** the ONLY readers of either symbol
  in the tree are `CompiledExternalTypeInfo()` / `InternalTypeInfo()` in
  `runtime/src/object/object_typeinfo_accessors.cpp`. (The third hit,
  `factory_dispatch_probe.cpp`, is this lane's C1 control reading the DLL's node
  raw — not a consumer.)
* **Consumers, from the shipped binary:** neither symbol is among the exe's 42
  imported `?_typeInfo@` slots (`pefile` over `Application-x64.exe`'s Runtime
  import table). `?_typeInfo@External@Object@@` and
  `?_typeInfo@ExternalList@Object@@` **are** — which is precisely the consumer
  that carried OBJDATA's two promotions and that these two rows do not have.
* **Those accessors are reached only through the four `??_7…@Object@@6B@`
  vtables**, and all four are now proved routeless: three by C++ (abstract,
  OBJDATA §2), the fourth by §2 above.
* `[ObjTI] ATTENDANCE` reads `entries=0` for both rows on every route driven —
  autostart, `BANISHED_AUTOLOAD`, and runs with the rows armed.

So promoting either would be **safe and would change nothing**, which is the
NO-OP class exactly. Both tagged.

**Porting the factories would not have helped.** The brief allowed porting
`CreateNewObject`/`CreateInPlaceObject` for these two classes "if you find it
necessary and cheap". It is neither: the measurement says the traps are never
reached, so removing them changes no verdict and no count. This is lane OBJINT's
conclusion arriving from the other side — *owning the row is what matters, and
the row is owned; what is missing is a consumer, and no amount of factory
porting creates one.*

## 4. Three instrument defects found and fixed (two of them in existing screens)

1. **A candidate row armed for measurement printed NO LINE AT ALL.** The
   readiness report prints a `?_typeInfo@` row only when it is BLOCKED or a UI
   row, so the moment a candidate went READY the run stopped saying anything
   about the thing it was launched to measure. Both screens now print PROMOTED
   **and** ARMED rows, tagged `[ARMED for measurement]`.
   *And the first version of that fix was wrong in the way this project keeps
   getting caught by*: the bookkeeping went into `RtMintRegisterRow`, which
   `BANISHED_RT_MINT_ROWS` **does not call** — it calls `RegisterOneMintRow`
   directly. The first armed run printed nothing and the tag looked broken. The
   bookkeeping now lives at the one choke point both callers go through.
2. **The probe's own name table manufactured a missing answer** — see §1's
   cap-immunity note. Fixed before any verdict was drawn from it.
3. **`RtObjectInternalVtblPin` first resolved through a loop variable**, which
   `tools/port_bypass_audit.py` files as `UNRESOLVED-NAME` (its literal-grep
   blindness, announced by the tool but easy to leave behind as residue). Both
   symbols are now spelled as literals at every call. Audit deltas versus an
   untouched main checkout: `BYPASS 25 → 25`, `UNRESOLVED-NAME 3 → 3`,
   `OK-DATA 15 → 19` (this lane's four raw/port reads, correctly classified).

## 5. Census — pre-registered, then measured

**Pre-registered before running:** *zero* `called` growth. The probe names
`?_typeInfo@CompiledExternal@Object@@…` as a literal, but that symbol was
already in the `called` set via the accessors, and `factory_dispatch_probe.cpp`
introduces no other Runtime symbol.

```
                total  called  ported  detour  covered  ceiling
Object  before    191      61      51       2    83.6%    88.5%
Object  after     191      61      51       2    83.6%    83.6%  AT CEILING
TOTAL   before   4632    1164     938       9    80.6%
TOTAL   after    4632    1164     938       9    80.6%
```

**Set-diff: ZERO rows entered the unported set, ZERO left it.** The prediction
held. The only number that moved is the **ceiling**, and it moved *down*,
because three rows changed from "not yet promoted" to "proved unpromotable".
Arithmetic check, because a count that moves the right direction by the wrong
amount reads as success: 10 rows called-but-not-ported, of which **10 are now
tagged** (7 from OBJDATA + 3 here) ⇒ ceiling = ported/called = 51/61 = 83.6%,
and 51 + 10 = 61 = called. It balances.

Both build configurations are clean at the tip (`standalone-debug`,
`standalone-perf`, and `make check-log-strip` exits 0).

## 6. What contradicts the brief

1. **"Is there any other caller of `factoryFn` in our tree? There was none
   before follow-up 2."** There were **two**, both older than follow-up 2
   (`Factory::CreateObject`, Wave 9; `DialogInstance::AddControllerTab`,
   Wave 10). The abort traps have been one promotion away from reachable for
   longer than follow-up 2's note implies — the note's *conclusion* is right and
   its *history* is not.
2. **"Condition (b) is now the actionable work, and it is squarely in scope."**
   True, and doing it is what proves condition (a) can never be met. The
   amendment reads as a pair of conditions to be satisfied in sequence; they
   cannot both be true on a DLL-present run. The amendment is not wrong about
   safety — (b) really does close a live 124k-object split — but its (a) is not
   a gate a future lane can pass, and it should be read as *retired by (b)*
   rather than *still owed*.
3. **"The three rows between them are all promotions, so the ceiling is
   reachable by this lane alone."** The ceiling is not reachable by promotion;
   it was the ceiling that was wrong. Object is at its true ceiling now.
4. **Scope note the brief does not mention.** The amendment names
   `VtblInternal()`/`VtblBase()`, and those are the two this class uses — but
   they are **not** the only registry-first stampers of `??_7Base@Object@@6B@`.
   Twelve other files reach that symbol through `ResolveExport` /
   `RT_DATA_SLOT` / `RtVtable`:
   `achievement_manager.cpp`, `goal.cpp`, `mesh.cpp`, `renderer.cpp`,
   `package_file.cpp`, `external.cpp`, `saveload_draw.cpp`, `dialog.cpp`,
   `dialog_ctor_dtor.cpp`, `dialog_vtable.cpp`, `sprite.cpp`,
   `spritesheet_vtable.cpp`. `??_7Base@Object@@6B@` is tagged
   UNPROMOTABLE(NO-OP) so none of them is live today — but anyone who ever
   revisits that tag has twelve sites to pin, not one.
