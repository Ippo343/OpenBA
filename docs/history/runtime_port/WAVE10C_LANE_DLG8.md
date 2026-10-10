# Wave 10C lane DLG8 — `??_7Dialog@UI@@6B@`, the last blocked UI vtable

**Branch** `wave10c/lane-dlg8`, based on `wave10/base` @ `e868337`.
**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-ae97121fb0fd23555`.

> ## VERDICT: eight traps closed, `READY … TRAP= 0` — and **NOT PROMOTED**.
> Gate (a) passes. Gate (b) is refused on measurement, not on caution: across
> three A/B routes the seven bodies were entered **zero** times, while the
> effectiveness control fired in all three. The row is **live for the stamp and
> dead for the dispatch**, and that is a sharper statement than lane DLGV could
> make about `??_7DialogController` — see §5.

The worktree this lane was handed was seeded from `965361d`, an unrelated old
commit; `git merge-base --is-ancestor e868337 HEAD` said NOT_ANCESTOR before a
line was written, and the branch was cut from `e868337` explicitly. The check in
the brief earned its place.

---

## 1. The board, measured

| | before | after |
|---|---|---|
| `??_7Dialog@UI@@6B@` shadow row | `slots= 9 ported= 1 TRAP= 8` | `slots= 9 ported= 9 TRAP= 0 purecall= 0` |
| UI census (called / ported) | 179 / 170 | 180 / 171 |
| deleting-dtor family rows | 37 | 38 |

---

## 2. Per slot

| slot | DLL | what the body is | how it differs from its SpriteSheet analogue | how verified |
|---|---|---|---|---|
| 0 | `0x180038b20` | MSVC `??_E` **vector** deleting destructor. K1 shape: `LEA R9,[0x180112e40]` (`~Dialog`, exported, and PORTED by lane DLG2) and `MOV EDX,0x88` (`sizeof`). | Same family body; only the two constants differ (SpriteSheet: `0x18011d670` / `0x58`). | **Not in this lane's file at all** — added as row 38 of `runtime/src/object/deleting_dtor.cpp`, so `tools/rt_deleting_dtor_verify.py` re-derives both constants from the shipped DLL. It does: `OK 0x180038b20 size=0x88 dtor=0x180112e40 UI::Dialog`. Its negative control still reports exactly 1 problem, so the green line discriminates. |
| 1 | `0x180038440` | `MOV RAX,[0x1801faa90] / RET` — `_typeInfo + 8`. | **Same shape.** | `RT_DIFF_SCALAR` against the shipped export. |
| 2 | `0x180038420` | `MOV EAX,[0x1801faa88] / RET`. | **Same shape.** | `RT_DIFF_SCALAR`. Its static twin `?TypeId@Dialog@UI@@SAIXZ` (`0x180038430`, not a vtable slot) is the same two instructions and was closed with it, as lane SS did for SpriteSheet. |
| 3 | `0x1801130b0` | `MOV EAX,6 / RET`. | Same shape, and by coincidence the same constant — two independent resource formats that both reached version 6. Not a reason to share a body. | `RT_DIFF_SCALAR`. |
| 4 | `0x1800066e0` | `?Compile@External@Object@@` | — | Already ours (`runtime/src/object/external.cpp`). Untouched. |
| 5 | `0x1801130c0` | `?Serialize@` — 4 arrays (3 walked element-by-element with a different element shape each), 5 scalars, 3 `ExternalPtr<T>`, 1 `String`. **~150 instructions.** | **Not remotely the same body.** SpriteSheet's is 2 strings, 3 scalars, 1 array, 1 ExternalPtr, and reaches 3 internals by address where this reaches **9**. Transcribed from scratch. | `RtSaveTrace::Compare`, write direction, twin synthetic sinks, **no `stopAfter`** — every store into the object is a `SetSize` and all four are behind retail's own `direction == 0` test, so this body writes nothing on the write path. **The probe did not fire: nothing called it (§5).** |
| 6 | `0x1800386b0` | Destruct-then-Construct in place. **No null test** (`0x1800386c2` derefs RCX immediately) — left faithful. | Same control flow; the Construct half differs as slot 8 does. | Not replayed (the destructor half cannot run twice). Its Construct half is covered by slot 8's byte oracle. |
| 7 | `0x1800387f0` | `MOV RAX,[RCX] / XOR EDX,EDX / JMP [RAX]` — tail jump into the object's OWN slot 0 with flag 0. | **Instruction-identical.** | Not replayed (double-destroy). |
| 8 | `0x180038750` | `if (this) { <ctor inlined>; return this; } else return nullptr;`, writing **0x88** bytes. | **DIFFERS, three ways — see §3.** | Poisoned private scratch + `RT_DIFF_BYTES` over **two** windows (`[0,0x44)` and `[0x48,0x88)`), plus a `RT_DIFF_SHADOW` on "returns this". **calls=0: no caller (§5).** |

---

## 3. The recipe was checked before it was followed, and it broke in four places

The brief named `??_7SpriteSheet@UI@@6B@` as the same slot shape. It is — same
nine slots, same order, same `Object::External` idiom. Every Dialog body and
every SpriteSheet analogue was nonetheless disassembled before anything was
reused, because lane UI3's array-dtor recipe was wrong last wave in exactly this
way (`reference_a_written_recipe_can_be_wrong`). Four of seven differ, and three
of the four differences would have been **silent**:

1. **`+0x30` is `MOV dword 0xFFFFFFFF`, not a zero-fill.** SpriteSheet's
   `ConstructInPlace` is all zeros. Copying it would leave every `UI::Dialog`
   with `_backgroundColor == 0` — transparent black instead of opaque white —
   and nothing would crash, on every dialog in the game.
2. **`+0x40` is stored as a DWORD and `+0x44` is never written by either side.**
   A qword zero-fill writes four bytes retail leaves alone; an oracle that
   compares them reports allocator residue as a divergence
   (`reference_oracle_must_not_compare_unwritten_bytes`). Hence two windows.
3. **The written extent is the whole class**, not SpriteSheet's `0x51` bytes
   ending in a single BYTE store.
4. **`Serialize` is a different function**, not a variation.

---

## 4. What this lane pinned about `UI::Dialog`, each with its own witness

`runtime/types/ui/Dialog.h` gains four field names and a `sizeof` assert the
header had explicitly refused.

| off | was | now | witness |
|---|---|---|---|
| `+0x30` | `_unknown30[8]` | `uint32_t _backgroundColor` | `?GetBackgroundColor@Dialog@UI@@QEBAIXZ` `0x1800388a0` = `MOV EAX,[RCX+0x30]; RET`; Serialize records it through `kScalarUInt`, agreeing with the `I` in the mangling |
| `+0x34` | (same field) | `int32_t _drawLevel` | `?GetDrawLevel@Dialog@UI@@QEBAHXZ` `0x180038910` = `MOV EAX,[RCX+0x34]; RET`; Serialize records it through `kScalarInt`, agreeing with the `H` |
| `+0x70` | `_unknown70[8]`, "presumed size/capacity, unproven" | `int32_t _templateCount` | Serialize `0x1801131c2` reads it as the bound it records and hands to `+0x68`'s `SetSize` |
| `+0x80` | `_unknown80[8]`, "**ctor zeroes it; never read**" | `int32_t _referenceCount` | Serialize `0x180113293`. **It is read.** This makes `+0x78/+0x80` the same `{data,count}` pair as the other three arrays |

### The `sizeof` assert, and why the header's refusal was right until now

`Dialog.h` carried an explicit refusal to assert a size, and the refusal was
sound *for its evidence*: lane DLG2's witness was the default ctor's last store
at `+0x80`, and a constructor's **written extent is a LOWER BOUND**, never the
size — trailing members a ctor leaves alone are invisible to it. A written
extent belongs to a WRITER (`reference_never_written_needs_by_whom`).

The new witness is a **different kind of thing**, not more of the same. Slot 0
passes MSVC's `eh vector destructor iterator` the stride it must walk a
`Dialog[]` with:

```
180038b3f   MOV EDX,0x88
```

That is `sizeof` by definition — what the compiler itself believes the distance
between two adjacent `Dialog` objects is. It agrees with DLG2's lower bound
exactly, which is the informative part: **there are no trailing members.**
`tools/rt_deleting_dtor_verify.py` re-derives it from the DLL independently of
the comment.

---

## 5. Attendance, the A/B, and why the answer is NO

**A green readiness screen is not a route**, and this lane was told so. It is
also not the whole story here, because unlike lane DLGV's row this one has a
control that *does* fire.

Every run: debug build, DLL present, under `tools/game_dir_lock.sh run lane-DLG8
1800 --snapshot`, deployed md5 quoted on both sides of every pair
(`466682a690e2da0690b2be2bd86a4339`). Logs in `lane_dlg8_logs/`.

| run | env | `[DlgVtbl] ATTENDANCE` | `[DLG2 diff]` | shutdown / crash log |
|---|---|---|---|---|
| `shadow2` | `AUTOSTART` + `RT_MINT_SHADOW=1` | **none entered** | — | `[Shutdown] done`, 0 bytes |
| `ab_ctrl` | `AUTOSTART` + `RT_DIFF=1` | **none entered** | `divergences=0` | `[Shutdown] done`, 0 bytes |
| `ab_cand` | ... + `RT_MINT_ROWS=??_7Dialog@UI@@6B@` | **none entered** | **`divergences=3`** | `[Shutdown] done`, 0 bytes |
| `mods_cand` | Mods route + `RT_DIFF` + `SAVE_TRACE` + row armed | **none entered** | `divergences=3` | `[Shutdown] done`, 0 bytes |

### The effectiveness control fires — so the zero is not the instrument's fault

Arming the row moves lane DLG2's independent ctor differential `0 -> 3`. Our
ported `??0Dialog` stamps the MINTED table where retail's stamps the DLL's, so
`+0x00` differs. The row therefore genuinely reaches live objects:
`DialogTempCtor` (`runtime/src/ui/dialog.cpp:2075`) stamps
`ResolveExport("??_7Dialog@UI@@6B@")` on the element path of **every** dialog the
game builds, and lane DLG2 measured that path firing in every run.

### And nothing dispatches it

The temp Dialog is destroyed by a **direct call** to our ported `??1Dialog`,
never through slot 0, and no DLL body makes a virtual call on it. With the DLL
present, retail-constructed Dialogs carry retail's table (a `LEA` inside the DLL
that `RegisterRuntimeAddr` cannot redirect —
`reference_registering_an_address_converts_a_selfcheck_into_an_oracle` names the
same limit for rel32).

**Live for the stamp, dead for the dispatch.** Promoting would put a different
pointer in every Dialog's `+0x00`, buy nothing measurable, and leave DLG2's
oracle carrying three permanent divergences. Refused.

### A second instrument agrees, by a different mechanism

Of the eight names registered, exactly **one** entered the census
(`?Serialize@Dialog@UI@@`, `ported=1`) — because `PortIsLive()` reaches it
through `ResolveExport(kSerializeMangled)`, which `RE_EXPORT_NONLITERAL` sees.
The other seven are reached only through `RT_ORIGINAL_ONCE`, a **diff-only door
that disappears at cutover**, which the called-scan correctly does not count. So
the census's "+1, not +8" is not a blind spot here: it is the same fact the
attendance counter reports, arrived at independently.

---

## 6. Census SET diff — measured on this tip, both directions

Measured by running `tools/runtime_census.py --csv` twice from this worktree
root on tip `<see final report>`, once with `runtime/src/ui/dialog_vtable.cpp`
present and once with it held out, and diffing the ROWS.

**UI called-but-not-ported list: 9 rows before, 9 rows after, `comm` empty in
BOTH directions.** No UI row left; no UI row entered. `??_7Dialog@UI@@6B@` is
still listed, and will be until it is promoted — a mint is not a promotion, and
neither is porting its slots.

**ENTERED the whole-census row set (7):**

| row | ns | ported |
|---|---|---|
| `?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z` | UI | **1** |
| `0x180075d20` `Array<DialogNamedTemplate>::SetSize` (stride 0x10) | internal | 0 |
| `0x180075b20` `Array<DialogControllerConfigEntry>::SetSize` (stride 0x20) | internal | 0 |
| `0x1800492d0` `Array<ExternalPtr<ElementDescription>>::SetSize` (stride 0x08) | internal | 0 |
| `0x18002f070` `Array<ExternalPtr<External>>::SetSize` (stride 0x08) | internal | 0 |
| `0x1800493d0` `_templates` element walker | internal | 0 |
| `0x18002f170` `_references` element walker | internal | 0 |

**LEFT: none.**

The six unported entrants are `?Serialize@`'s address doors and are the
**cutover surface growing by six** (`reference_porting_can_grow_the_cutover_surface`),
stated here rather than left for a later census. They reconcile exactly with the
nine doors the source banner names: the other three (`0x180038f90`,
`0x180004590`, `0x1800756d0`, the three `ExternalPtr<T>::Serialize`
instantiations from lane EXTPTR's table of 25) plus `0x1800c9930`
(`?Serialize@String@System@@`, exported and already ported) were **already**
called by other files, so they were already rows. The four `SetSize`
instantiations belong to the class-B allocator family and are not this lane's
rows — the same call lane SS made for `0x180078950`.

Namespace counts: UI `179/170 -> 180/171` (95.0% both sides); TOTAL
`1094/875 -> 1101/876`, i.e. **80.0% -> 79.6%**. The percentage went DOWN and
that is the port working, for the reason Wave 10B's close already records: read
the set, never the ratio (`reference_diff_the_set_not_the_count`).

---

## 7. Two things found on the way that are not this lane's row

* **`0x1800384d0` is EXPORTED**, as `??3Dialog@UI@@SAXPEAX@Z` — a class-scoped
  `operator delete`. `deleting_dtor.cpp`'s banner lists ten `operator delete`
  addresses and calls them all "per-translation-unit copies"; this one has a
  name in the export directory. Disassembled and byte-identical to the other ten
  and to `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`, so nothing about the port
  changes — but "unexported internal" was the wrong description of the set, and
  that is exactly the class of error
  `reference_grep_cannot_tell_internal_from_unported_export` is about. All nine
  of `?Serialize@`'s callees were run through the export table for this reason;
  all nine really are absent from it.

* **`BANISHED_RT_MINT=1` on its own is fatal today, and that is expected.** The
  first shadow run armed the whole mint (56 vtables, **156 unported traps**) and
  died in `MemAlloc` inside `RtEngine_Engine_Startup` with
  `[REDZONE-DUMP] no allocator singleton`. Nothing to do with this lane; the
  shadow SCREEN needs only `BANISHED_RT_MINT_SHADOW=1`, and every run after that
  used it. Recorded because "the shadow run crashed" is a sentence a later lane
  will otherwise have to re-derive.

* `[DLG2 diff]`'s hole-screen accounting reads `excluded=12` in the control and
  `excluded=0` in the candidate. Noticed, not chased: it is lane DLG2's
  instrument and the change is consistent with its accounting stopping at the
  first divergence.

---

## 8. Files

| file | change |
|---|---|
| `runtime/src/ui/dialog_vtable.cpp` | **new** — seven slot bodies, attendance counters, both registration doors |
| `runtime/src/ui/rt_ui_port.cpp` | `BANISHED_RT_DLGVTABLE` lane hook |
| `runtime/src/object/deleting_dtor.cpp` | row 38 (`UI::Dialog`), thunk, registration, count |
| `runtime/src/object/deleting_dtor_internal.h` | `kSpecCount` 37 -> 38 |
| `tools/rt_deleting_dtor_verify.py` | expects 38 rows |
| `runtime/types/ui/Dialog.h` | four fields named, four asserts, the `sizeof` assert |
| `standalone/reg_mint_promoted.cpp` | the Dialog paragraph replaced with the measured NOT-PROMOTED verdict |
| `lane_dlg8_run.sh` | the lock-friendly run driver these measurements used |
