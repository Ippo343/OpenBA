# WAVE 10C — lane TIDENT

**An audit of twelve `?_typeInfo@` rows, not a porting lane.**

Branch `wave10c/lane-tident`, base `wave10/base` @ `e868337`.

Lane DLGV declined to promote `?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A`
and flagged that its reason applied to the **eleven typeinfo rows already promoted
and live default-on** in `standalone/reg_mint_promoted.cpp`:

> promoting it makes `?TypeName@DialogController@` return our node's `name`
> **POINTER** … and the shadow compares **content, not identity**.

That is `reference_diagnostic_field_cannot_express_the_answer.md`: `strs=` is a
`strcmp`, so `strs=0` is a true statement about the characters that says nothing
whatever about the object. The flag was right. This lane answers it.

---

## 1. Verdict

| question | answer |
|---|---|
| Does anything compare `BaseTypeInfo::name` **by pointer**? | **No. Zero consumers, in either shipped binary.** |
| Are the eleven live rows safe as shipped? | **Yes. None is backed out.** |
| Should `?_typeInfo@DialogController@` be promoted? | **No — but NOT for DLGV's reason.** It has no live consumer on any of five routes. |

The pointer-identity question is **retired for all twelve rows**, permanently.

---

## 2. The consumer census, with addresses

Method: Ghidra MCP over the shipped `Runtime-x64.dll` and `Application-x64.exe`.
Two directions — every **static read** of a node's `+0x08`, and every **caller**
of the accessors that hand the pointer out.

### 2.1 Runtime-x64.dll

| finding | number | detail |
|---|---|---|
| `?_typeInfo@` statics | 104 | — |
| static reads of any node's `+0x08` (`name`) | **103**, and **all 103 are a `?TypeName@` accessor** | one per type; nothing else in the DLL reads the field by RIP-relative reference |
| **CALL** sites to any of the 103 `?TypeName@` bodies | **0** | 309 refs total, every one a vtable slot or an export-table entry |
| `?GetTypeName@Factory@Object@@` (0x1800c0a60) | **0 callers** | only `1801b3908 [DATA]` (export table) |

Dynamic readers (walked pointers, invisible to an xref sweep) are the three
already pinned in `runtime/types/object/TypeInfo.h`:

* `Factory::Startup` **0x1800c0630** — `info->typeId = TypeNameCRC(info->name)`. **CONTENT.**
* `BaseTypeInfo::ctor` **0x180006320** — stores it. Not a comparison.
* `Factory::GetTypeIdForFileExtension` **0x1800c04c0** — `_stricmp` on `+0x18`. **CONTENT.**

### 2.2 Application-x64.exe

| finding | number | detail |
|---|---|---|
| imported `?_typeInfo@` slots | 42 | `0x140151330` … `0x140152a80` |
| references to those slots | **525** | |
| …that read the dword at **`+0x00`** (the typeId) | **525** | 445 `MOV EDX,[RAX]`, 23 `MOV ECX,[RAX]`, 19 `MOV EDX,[RCX]`, remainder the same shape |
| …that read **`+0x08`** (the `name`) | **0** | not one, in a 5-instruction window after every reference |
| imported `?TypeName@` thunks | 8 | e.g. `?TypeName@DialogInstance@UI@@` at `0x140152700`, `?TypeName@SpriteSheet@UI@@` at `0x1401519a0` |
| **CALL** sites to those 8 | **0** | every reference is an import-thunk `JMP` or an exe vtable slot |
| `CALL qword ptr [reg+0x8]` (the TypeName slot offset) | 73 | **none is a TypeName dispatch** — every one consumes `EAX` as a signed int (`TEST EAX,EAX / JS`) or stores a pointer; a `const char*` return is never produced |
| imports `?GetTypeName@Factory@` | **no** | it imports `?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z` (`0x1401528c8`, name → CRC, **content**) and `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` (`0x140152a98`, **by id**) |

**Conclusion.** The engine keys everything on the CRC32 of the name (`typeId`).
The name *string* is a debug/reflection courtesy that nothing in either shipped
binary compares, by pointer or otherwise, except by CRC-ing it.

> **Scope, stated plainly.** This census covers static reads and direct calls in
> the two shipped binaries. It does not enumerate hypothetical virtual dispatch
> from a module not examined. The `CALL [reg+0x8]` sweep is what closes the
> virtual route for the exe, and it is clean.

---

## 3. The instrument, and its negative control

`RtDataMintShadowReport`'s typeinfo screen printed `strs=0` and called the row
READY. It now also prints `ptr=`, **computed and scored separately** and never
folded into `ready` (`reference_union_control_hides_a_blind_screen.md`).

```
READY  ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A  id=0x1821d6a5 idcmp=yes strs=0 ptr=2 fac=2traps  [PROMOTED]
[RtMintShadow]   ptr= (identity, scored SEPARATELY from strs=): 207 of 312 string
                 fields hold a DIFFERENT POINTER than the DLL's node; 11 PROMOTED
                 row(s) hand out at least one
```

### 3.1 Why the control had to run the *other* way

`bti->name = t.name` is a string literal in our own image, so `ptr=` is non-zero
for essentially every row **by construction**. A screen that says "differs" for
every input cannot be distinguished from a screen stuck on "yes". So the control
drives it toward *identical*: `BANISHED_RT_MINT_PTRALIAS=<n>` re-points the first
`n` rows' `name` at the DLL's own pointer, leaving the characters untouched.

### 3.2 Each screen scored separately — the point of the exercise

2026-09-09, debug build, DLL present, autostart + `BANISHED_AUTOQUIT=300`, under
`tools/game_dir_lock.sh`. Logs: `lane_tident_logs/{control,ptralias}/`.

| screen | control | `PTRALIAS=5` | verdict |
|---|---|---|---|
| `strs=` (content, pre-existing) | 624 comparisons, **0 MISMATCH** | 624 comparisons, **0 MISMATCH** | **did not move — blind to identity, exactly as charged** |
| `ptr=` (identity, new) | **207** / 312 | **202** / 312 | **moved by exactly 5** |

Per row, on the two aliased rows that print:

```
control   READY ?_typeInfo@AnimatedImageDescription@UI@@…  strs=0 ptr=2
ptralias  READY ?_typeInfo@AnimatedImageDescription@UI@@…  strs=0 ptr=1
```

`5 of 5 requested row(s) aliased`, and the control announces itself when it does
**not** fire (`CONTROL DID NOT FIRE; the ptr= count below is NOT evidence either
way`) — so a run where `DllExport` returned null cannot be scored as the
screen's failure.

Both polarities are therefore demonstrated, on separate scores:
a row whose content matches and whose pointer differs → `ptr` catches it;
a row whose pointer is made identical → `ptr` drops. `strs=` sat at 0 through
both, which is the indictment, not a pass.

---

## 4. The finding nobody had named: a live pointer divergence

The tree **already contained** a pointer-identity screen for this question — three
of them — and they were never read as such. `?TypeName@` ports compare the
returned `const char*` **as a pointer** (`RT_DIFF_SCALAR(static_cast<const void*>…)`).

| body | row promoted? | calls | divergences |
|---|---|---|---|
| `IO::PackageFile::TypeName` | no | 4 | 0 |
| `UI::SpriteSheet::TypeName` | **yes** | 1 | **1** |
| `UI::Element::TypeName` | yes | **0 (never dispatched)** | — |

```
[RtDiff] DIVERGENCE UI::SpriteSheet::TypeName  call #1
         ours=00007ff60aa4612c   theirs=00007ffcab5e4578
```

`ours` is in our image, `theirs` in the DLL. **This has been live and unexplained
since the SpriteSheet row was promoted.**

### 4.1 The effectiveness control for that claim

Logs: `lane_tident_logs/{promoted_on,promoted_off}/`.

| oracle | promoted ON | promoted OFF |
|---|---|---|
| `UI::SpriteSheet::TypeName` | calls=1 **divergences=1** | calls=1 **divergences=0** |
| `IO::PackageFile::TypeName` (unpromoted, same code shape) | calls=4 div=0 | calls=4 div=0 |

`calls=1` on **both** sides, so what moved is the **value**, not the coverage —
this is not a `calls=0` artefact
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`). PackageFile
is the built-in negative control that rules out "the oracle is broken".

Given §2, the divergence is **expected and harmless**: same characters, different
object, and nothing compares the object. It is the string twin of the
"`Construct` divergence is the vtable identity" paragraph already in
`reg_mint_promoted.cpp`. It is now documented beside the body, and the comparison
is **deliberately left as a pointer** — relaxing it to `strcmp` would delete the
one screen in the tree that can still see identity change.

`UI::Element::TypeName` reports nothing at all (only its `[bytes]` pin does), i.e.
`calls=0`: it will report the same divergence the first time an Element is
dispatched through slot 1. That is now written above the body so it is met as a
documented expectation rather than a mystery.

---

## 5. `?_typeInfo@DialogController@` — NOT promoted, for a different reason

Gate (a) passes: `READY … id=0xf2c192b8 idcmp=yes strs=0 ptr=2 fac=2traps`.
DLGV's stated blocker is retired by §2.

**Gate (b) fails.** The row's only consumer in the tree is `TypeIdOfStatic(…)` in
`runtime/src/ui/dialog_create.cpp:399`, reached from
`RtUiDialogInstance_AddControllerTab` (retail `0x1801133b0`). That helper now
carries a `[TidStatic]` attendance line printing the **node pointer** and the id.

Five routes, both halves of the armed/unarmed A/B, all exit 0 with 0-byte crash
logs — and **zero `[TidStatic]` lines on every one**:

| route | `[TidStatic]` lines |
|---|---|
| autostart + quit, row unarmed | 0 |
| autostart + quit, `BANISHED_RT_MINT_ROWS=?_typeInfo@DialogController@…` (`ROW ON` confirmed in the log) | 0 |
| `BANISHED_TOOL_HARNESS=housepanel` | 0 |
| `BANISHED_TOOL_HARNESS=pinpanel` | 0 |
| `BANISHED_TOOL_HARNESS=tradepanel` | 0 |

Same root as DLGV's `[DlgCtrl] ATTENDANCE: none of the nine bodies was entered`:
nothing drivable constructs a controller through the runtime base path. Arming
the row is measurably a no-op, and an A/B in which nothing is entered cannot
distinguish a safe promotion from an unexercised one — this file's own
"WHY NOT ALL 104" rule (`reference_uncalled_port_is_not_verified.md`).

**It expires the moment `[TidStatic]` prints on any route.** Note for whoever
gets there: the **id is identical either way** (`idcmp=yes`), so the observable
is the **node pointer on that line** moving out of the DLL's data into our image
— which is why the line prints the pointer and not just the value.

---

## 6. What moved in the shadow report format

Three other lanes read this output this wave:

1. Per-typeinfo readiness lines gained **one column**, `ptr=<n>`, between
   `strs=` and `fac=`. Existing `strs=` / `idcmp=` / `READY|BLOCKED` /
   `[PROMOTED]` fields are **unchanged in name, meaning and position**.
2. **Two new summary lines** after `typeinfo rows: N READY, M BLOCKED`, both
   prefixed `[RtMintShadow]   ptr= (identity…` / `[RtMintShadow]   ^ EXPECTED…`.
3. `BANISHED_RT_MINT_PTRALIAS=<n>` (default 0/off) adds `[RtMint] PTRALIAS …`
   lines. Off by default — no existing run's output changes because of it.

Nothing was renamed or removed. A grep for `strs=` still matches; a grep for the
whole readiness line as a fixed-field pattern will need the extra column.

---

## 7. Files

| file | what |
|---|---|
| `standalone/rt_data_mint.cpp` | the `ptr=` screen, the two summary lines, and `BANISHED_RT_MINT_PTRALIAS` with its did-not-fire announcement |
| `standalone/reg_mint_promoted.cpp` | the retired question in the header; the `?_typeInfo@DialogController@` entry under NOT PROMOTED |
| `runtime/src/ui/spritesheet_vtable.cpp` | the ⚠ block explaining the permanent divergence, with its A/B |
| `runtime/src/ui/element_vtable.cpp` | two corrections to the "pointer identity fails silently and late" paragraph |
| `runtime/src/ui/dialog_create.cpp` | the `[TidStatic]` attendance line on `TypeIdOfStatic` |
| `lane_tident_runs*.sh` | the four run scripts, each under `game_dir_lock.sh` |
| `lane_tident_logs/` | seven run snapshots |

Debug and **release** (`make clean` first, then `mingw32-make standalone`) both
build clean.

---

## 8. Census set-diff

`runtime_census.py --namespace UI` on the lane tip reads **179 called / 170
ported = 95.0%**, and that is the `e868337` baseline unchanged — this lane
promoted nothing and ported no new body.

**Verified rather than asserted.** `git diff e868337 -U0` over the whole lane
adds or removes **no** `RegisterRuntimeExport` / `RegisterRuntimeAddr` /
`RegisterExeAddr` / `RegisterDispatchSlot` / `RT_DATA_SLOT` /
`RtMintPromotionStorage` line. The only `ResolveExport` line in the diff is the
pre-existing one in `TypeIdOfStatic`, changed solely by adding a brace for the
new `[TidStatic]` printf. Both halves of the census — `called` and `ported` —
are therefore unchanged **by construction**, and the **set-diff is empty**: no
row entered and none left.

`?_typeInfo@DialogController@UI@@…` stays in the
called-but-not-ported set, where lane DLGV's `RT_DATA_SLOT` literal made it
visible. Per DLGV, its appearance is **not** new surface and **not** a
regression: `dialog_create.cpp` has resolved it by name since Wave 10 lane UI3
through a variable the census's `called` scan cannot see
(`reference_census_cannot_see_nonliteral_registrations.md`).
