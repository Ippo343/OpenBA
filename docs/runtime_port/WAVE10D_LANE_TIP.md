# Wave 10D — lane TIP: `UI::ToolTipDialog`

**Branch** `wave10d/lane-tip`, cut from `wave10/base` @ `190e91c`
(`git merge-base --is-ancestor 190e91c HEAD` → yes, checked before any work).
**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a38d18aaffbf7cc30`.

Scope was the four `ToolTipDialog` census rows — the only rows in UI missing a
port at all. It grew by two on the wave owner's instruction (vtable slots 1 and
2, plus slot 0 as a deleting-dtor family row) once the reason was established:
**minting is not porting**, and a regenerated mint row with three trapped slots
would not finish UI.

---

## 1. The rows

| row | retail VA | size | port | entries this wave | verdict |
|---|---|---|---|---|---|
| `??2ToolTipDialog@UI@@SAPEAX_K@Z` | `0x18004cfd0` | 19 instr (0x63 B) | `RtUi_ToolTipDialog_OperatorNew` | **0** | ported, NOT verified — no route (MOUSE) |
| `?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ` | `0x1800356a0` | 13 instr (0x38 B) | `RtUi_Element_GetToolTipString` | **0** | ported, NOT verified — no route (MOUSE) |
| `?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z` | `0x1800dab00` | 61 instr (0x140 B) | `RtUi_ToolTipDialog_SetText` | **0** | ported, NOT verified — no route (MOUSE) |
| `??_7ToolTipDialog@UI@@6B@` slot 1 `?TypeName@` | `0x18004cfc0` | 2 instr | `RtUi_ToolTipDialog_TypeName` | **0** | ported, NOT verified — no route (MOUSE) |
| `??_7ToolTipDialog@UI@@6B@` slot 2 `?Type@` | `0x18004cfa0` | 2 instr | `RtUi_ToolTipDialog_Type` | **0** | ported, NOT verified — no route (MOUSE) |
| `??_7ToolTipDialog@UI@@6B@` slot 0 (vector deleting dtor) | `0x18004d270` | 24 instr | `deleting_dtor.cpp` row 39 | n/a (family body) | ported, verifier green |
| `??_7ToolTipDialog@UI@@6B@` (the DATA row itself) | `0x180157880` | 10 slots | **not a mint row yet** | — | blocked on the wave-close regeneration + a promotion |

All five function bodies live in `runtime/src/ui/tooltipdialog.cpp` behind
`BANISHED_RT_UITOOLTIPDLG`. The dtor row is in
`runtime/src/object/deleting_dtor.cpp` behind `BANISHED_RT_DELDTOR`.

**Absence established first.** Every one of the five addresses and five mangled
names was grepped across `code/`, `runtime/`, `engine/` and `standalone/` before
a line was written. The only hits were CONSUMERS — `RT_EXPORT_ONCE` call sites
in `runtime/src/ui/userinterface_hover.cpp` — plus one comment. Nothing was
re-ported.

---

## 2. The vtable: slots COMPARED, not assumed

`??_7ToolTipDialog@UI@@6B@` at `0x180157880`, ten slots. The extent is pinned by
the adjacent export: `0x180157880 + 10*8 == 0x1801578d0 ==
??_7DialogLoaderLoopT@Engine@@6B@`.

| slot | ToolTipDialog | DialogInstance (`0x180156d70`) | same? |
|---|---|---|---|
| 0 | `0x18004d270` | `0x1800458c0` | **no — override** |
| 1 | `0x18004cfc0` TypeName | `0x180045400` | **no — override** |
| 2 | `0x18004cfa0` Type | `0x1800453e0` | **no — override** |
| 3 | `0x180114fa0` Persist | `0x180114fa0` | yes |
| 4 | `0x180114fb0` OnOpened | `0x180114fb0` | yes |
| 5 | `0x180115330` OnClosed | `0x180115330` | yes |
| 6 | `0x1801153a0` OnUpdateLayout | `0x1801153a0` | yes |
| 7 | `0x180115400` OnUpdate | `0x180115400` | yes |
| 8 | `0x180114ed0` OnElementEvent | `0x180114ed0` | yes |
| 9 | `0x180115470` OnSerializeSettings | `0x180115470` | yes |

Both tables were read out of the DLL's `.rdata` by file offset, not inferred.
Slots 3..9 are byte-identical to `??_7DialogInstance@UI@@6B@`'s own targets, and
that vtable is already 10/10 and PROMOTED — so those seven cost this lane
nothing. **ToolTipDialog overrides exactly slots 0, 1 and 2, and this lane ports
all three.** (Lane VTFID is concurrently testing the general "derived vtables
copy the base entry" hypothesis; this table is one instance of it, measured
rather than assumed.)

---

## 3. What was found in each body

**`??2ToolTipDialog` (operator new).** Instruction for instruction
`?MemAlloc@MemoryAllocator@System@@` (`0x1800c7a50`) with the alignment argument
frozen to the immediate `MOV R8D,0x8`. Both were disassembled side by side —
`runtime/src/video/context_ctor.cpp` records the same equivalence and found a
sibling in the family using align **4**, which is why resemblance is not
accepted here. The critical section is inside `MemAlloc`, so it is not
reproduced at this level.

**`?GetToolTipString@Element@`.** `ret->data = 0; p = self[+0x58]; if (p) {
ret->data = p; ++p[-4]; } return ret.` The `++p[-4]` is a REFCOUNT ACQUIRE — it
hands the caller an owned reference, which is why `BeginToolTip` releases both of
its two calls. `+0x58` is `Element::_toolTipString`
(`runtime/types/ui/Element.h:251`). The `MOV dword ptr [RSP],0/1` pair is MSVC's
SEH state variable, not a program value, and is not reproduced.

**`?SetText@ToolTipDialog@`.** Six steps, all read from the listing:

1. build a local `System::String` from the literal at `0x180157868`, verified
   from the DLL image to be `textToolTip\0` (the vtable this lane also ports
   begins in the very next qword);
2. `MOV RBX,[RBX+0x50]` — the DialogInstance root, **unguarded**, and left
   unguarded per CLAUDE.md's rule;
3. `FindElement`'s head test INLINED onto the root — empty query never matches,
   lengths compared before `StrCompare`, a null name of matching length counts
   as a match;
4. the child scan, which calls the real ported `Element::FindElement`
   (`0x180115650`) per non-null child, bound read once, signed;
5. release the local with retail's inlined idiom **including its asymmetry**
   (freeing branch leaves `data - 8`, sharing branch nulls);
6. the `SetParameter(0, text)` tail — signed `> 0` guard on the parameter array
   size at `+0xd0`, the do-while dirty walk ORing `0x8000000` into `_flags`
   (`+0x34`) up the `_parent` chain (`+0x98`), `String::operator=` into
   `paramArray[0]` (`+0xc8`, no index arithmetic at all in retail), `CacheText`.

**`?TypeName@` / `?Type@`.** Two instructions each, off
`?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A` (`0x1801fa4a0`): `MOV
RAX,[+0x08]` (the `name` pointer) and `MOV EAX,[+0x00]` (a **DWORD**, the
typeId). The `dialog_controller_base.cpp` shape exactly.

**Slot 0.** A VECTOR deleting destructor, not the scalar `??_G` shape — the
distinction lane K's family banner spells out and that would have been a wild
write if got wrong. Its two constants are `LEA R9,[0x1800dac70]` and `MOV
EDX,0xc0`, and `0x1800dac70` (`??1ToolTipDialog@UI@@UEAA@XZ`) disassembles to a
vtable restamp plus a tail call to `~DialogInstance`, which is exactly what the
deleting dtor's scalar branch inlines. `0xc0` is the same `0xc0` `BeginToolTip`
passes to `operator new` — the two ends agree.

### No oracle, and why that is the right call

None of these five gets an RtDiff oracle. `SetText` is the shape
`widgets_layout.cpp` already wrote the argument out for on `SetParameter`:
running retail afterwards is **vacuous** (its own equality check inside
`String::operator=` sees the slot already holding `text` and no-ops), and
restoring the slot first to make it non-vacuous is a use-after-free risk on a
shared refcounted `String`. `GetToolTipString` mutates a refcount, so a
re-run doubles it. The instrument here is the attendance counter.

---

## 4. Attendance — entries, not comparisons

`runtime/src/ui/tooltipdialog.cpp` prints one unconditional line at exit:

```
[RtPort] UI::ToolTipDialog ATTENDANCE: none of the five bodies was entered in this run.
```

That is the measured result of the full autostart boot (debug build, DLL
present, `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60`,
under `tools/game_dir_lock.sh`, clean self-exit, `standalone_crash.log` zero
bytes). **All five bodies: entries = 0.**

This is NOT `calls=0` from a diff site, which has five recorded causes that
invert the conclusion. It is an unconditional entry counter, so `0` means
exactly one thing: registered and never reached.

**Why zero, and it is not a defect.** All five sit downstream of
`UserInterface::BeginToolTip`, which is mouse-driven, and an unattended run has
no mouse. Lane UIF6 measured the same zero on `BeginToolTip`, `EndHover` and
`EndDragDrop` last wave for the same reason. `standalone/` was grepped for an
existing route (`tooltip`, `hover`) before writing this — there is none, which
is exactly why Wave 10D has a lane MOUSE building one. **This lane did not build
a second injector.**

Verdict, stated plainly: **ported, NOT verified. Verified once MOUSE's injector
lands** — at which point the counters to read are `operatorNew` (expect one
entry per tooltip shown), `GetToolTipString` (expect **two** per tooltip, retail
calls it twice), `SetText` (one), and within `SetText`, `found` > 0 whenever the
tooltip template contains a `textToolTip` element. `TypeName`/`Type` move only
if something reflects on the object.

Each counter ships with a named effectiveness control beside it (`nonNull`,
`rootHit`, `found`, `wrote`, `bytes`) so a counter that moves without the body
doing anything cannot read as success.

---

## 5. The gate was tested before it was trusted

The wave's rule is: does the quantity the gate measures actually VARY with the
thing being gated? Measured, both halves of an A/B on the same exe:

| | `BANISHED_RT_UITOOLTIPDLG` unset | `=0` |
|---|---|---|
| `[RtPort] UI:` | **205** exports | **200** exports |
| `runtime registry:` | **953** exports, **959** addrs | **948** exports, **954** addrs |

Exactly 5 and 5 — the five rows, both doors each. The gate is a real control,
not a constant.

The same test was applied to the **verifier** change. `tools/rt_deleting_dtor_verify.py`
carried a typed `!= 38`; it now reads `kSpecCount` out of
`deleting_dtor_internal.h`, which makes it a genuine three-way cross-check
(table ↔ constant ↔ verifier) instead of a fourth place to remember. Controls:

* unmodified: `problems: 0`, 39 rows, K1 16 / K2K3 23;
* `--neg` (the built-in perturbation): `problems: 1`, caught, `NEGATIVE CONTROL PASSED`;
* `kSpecCount` temporarily set to 40: `*** kSpecCount says 40 rows,
  deleting_dtor.cpp has 39`, `problems: 1`. Restored.

The third is the one that matters: the new check fires on the thing it exists to
catch, so it is not decorative.

---

## 6. Census SET DIFF

Measured with `tools/runtime_census.py` from this worktree's root, twice on this
branch: **before** at `190e91c` with no edits, **after** with all edits in and
the debug build green.

```
UNPORTED total  before = 223   after = 221
```

**LEFT the unported set (3), all UI:**

```
??2ToolTipDialog@UI@@SAPEAX_K@Z
?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ
?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z
```

**ENTERED the unported set (1), UI:**

```
?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A   [runtime/src/ui/tooltipdialog.cpp]
```

That entrant is the honest price of resolving by name rather than opening an
address door: `TypeName` and `Type` read that node, so the tree now asks for it.
It is ALREADY a minted row (`standalone/rt_data_mint_tables.inc:2091`,
`0x1801fa4a0`), so at cutover it exists — it is in the census because a minted
row leaves the census only when PROMOTED. Its promotion warrant is §7.

UI namespace: **7 → 5** called-but-not-ported.

**A row the census cannot see, stated because it is real.**
`??1ToolTipDialog@UI@@UEAA@XZ` (`0x1800dac70`) is newly reached, by ADDRESS,
from the deleting-dtor row. `runtime_census.py` buckets address-called functions
as `internal` and does not list it, so the diff above **understates** the
cutover surface by one. It is an exported symbol and it is not ported; it will
need a body (or a lane B registration) before cutover. Naming it here is the
whole point — an instrument's blind spot does not stop being a fact.

---

## 7. Two promotion warrants, written ready-to-apply

Neither is applied on this branch: the mint table must be regenerated first, and
the wave owner does that once at wave close.

### 7a. `??_7ToolTipDialog@UI@@6B@` → `standalone/reg_mint_promoted.cpp`

```cpp
    st = RtMintPromotionStorage("??_7ToolTipDialog@UI@@6B@", 0x180157880ull);
    RegisterRuntimeExport("??_7ToolTipDialog@UI@@6B@", st);
    RegisterRuntimeAddr(0x180157880ull, st); ++n;
```

**Warrant.**

* *Why it will exist.* `tools/rt_mint_gen.py`'s source-scan rule matches the
  literal `"??_7ToolTipDialog@UI@@6B@"` at `userinterface_hover.cpp:410`. The
  generated table is STALE (it predates Wave 10C), not the rule wrong. Confirmed
  by reading the generator, not assumed.
* *Why it will be `TRAP=0`.* All ten slots are ours the moment this lane lands:
  slot 0 → `deleting_dtor.cpp` row 39; slots 1, 2 → this lane; slots 3..9 →
  `??_7DialogInstance@UI@@6B@`'s own targets, compared by address (§2), and that
  vtable is already 10/10 and PROMOTED. **If the regenerated row does NOT read
  `TRAP=0`, do not promote it — a trap that appears is a slot this table did not
  predict, and the table above is the thing to re-derive.**
* *Why a green screen is not enough, and what closes it.* `READY / TRAP=0` says
  the table is safe to stamp, not that anything dispatches it — two UI vtables
  are green-and-unpromoted right now for exactly that reason. The route is lane
  MOUSE's hover injector, because `BeginToolTip` is what stamps this vtable onto
  a freshly allocated object (`userinterface_hover.cpp:410`). **The counter that
  proves the route is `operatorNew` in the `[RtPort] UI::ToolTipDialog
  ATTENDANCE` line**: it cannot be non-zero unless a ToolTipDialog was actually
  built, and a ToolTipDialog is not built without this vtable being stamped.
* *Pointer identity.* Retired for this family in Wave 10C lane TIDENT and not
  re-asked per row.

### 7b. `?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A`

```cpp
    st = RtMintPromotionStorage("?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A", 0x1801fa4a0ull);
    RegisterRuntimeExport("?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa4a0ull, st); ++n;
```

**Warrant — MEASURED on this branch**, `BANISHED_RT_MINT_SHADOW=1`, debug build,
DLL present, full autostart boot under the lock:

```
[RtMintShadow] BLOCKED ?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A  id=0x175c6265 idcmp=NO  strs=0 ptr=2 fac=2traps
[RtMintShadow] READY   ?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A  id=0x175c6265 idcmp=yes strs=0 ptr=2 fac=2traps
```

The BLOCKED line is the `phase=init` report, before the DLL's own
`Factory::Startup` has run, and it is `NOT compared` rather than passed — an
early report cannot read as a pass. By the READY report the minted typeId is
compared against the DLL's own node and agrees (`idcmp=yes`), and the name
strings agree (`strs=0`).

The decisive comparison is that this row is **character-for-character the same
shape as three rows that are already promoted and live default-on**:

```
BLOCKED/READY ?_typeInfo@Dialog@UI@@…       id=0x4fcc106b idcmp=NO/yes strs=0 ptr=2 fac=2traps  [PROMOTED]
BLOCKED/READY ?_typeInfo@Element@UI@@…      id=0x8d4a19a7 idcmp=NO/yes strs=0 ptr=2 fac=2traps  [PROMOTED]
BLOCKED/READY ?_typeInfo@SpriteSheet@UI@@…  id=0x1821d6a5 idcmp=NO/yes strs=0 ptr=2 fac=2traps  [PROMOTED]
```

Same `ptr=2`, same `fac=2traps` (nothing constructs a ToolTipDialog through the
factory, so a trapped factory function is never called — `reg_mint_promoted.cpp`
states that for the whole family). Nothing distinguishes this row from three
that have shipped promoted for a wave.

---

## 8. Files touched

| file | change |
|---|---|
| `runtime/src/ui/tooltipdialog.cpp` | **new** — five bodies, attendance counters, `BANISHED_RT_UITOOLTIPDLG` |
| `runtime/src/ui/rt_ui_port.cpp` | `extern` + call `RtUiToolTipDialogRegister()` |
| `runtime/src/object/deleting_dtor.cpp` | row 39 + `RtDelDtor_ToolTipDialog` + registration; the `printf` count now comes from `kSpecCount` |
| `runtime/src/object/deleting_dtor_internal.h` | `kSpecCount` 38 → 39 |
| `tools/rt_deleting_dtor_verify.py` | expected row count now read from `kSpecCount` instead of typed |
| `docs/runtime_port/WAVE10D_LANE_TIP.md` | this file |

Not touched, deliberately: `standalone/rt_data_mint_tables.inc` (generated — the
wave owner regenerates at close) and `standalone/reg_mint_promoted.cpp` (the two
warrants above are applied after that regeneration).

## 9. Build

* `mingw32-make standalone-debug -j8` — clean, **zero warnings** on the touched
  translation units (one `-Wcomment` from an ASCII-art backslash was found and
  fixed).
* Release: `make clean` then `make standalone` — a `make standalone` straight
  after a debug build reports "Nothing to be done" and proves nothing, so the
  clean was done first. Note `log_strip.h` `#define`s `printf`; this file uses
  unqualified `printf`, never `std::printf`, which is the form that survives the
  release build.
