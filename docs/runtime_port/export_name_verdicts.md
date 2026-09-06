# `ResolveExport` names that are not in Runtime-x64.dll — verdicts

Produced by `tools/ghidra_mcp/.venv/Scripts/python.exe tools/export_name_audit.py`.
Read [WAVE2.md](WAVE2.md) finding 4 for why this matters: a name the DLL does not
export resolves to **null**, the call site's `ResolveAddr` fallback keeps the pointer
valid, and nothing looks wrong — until someone ports that function, at which point
`RegisterRuntimeExport` (which is keyed on the NAME) never reaches the call site and
the port is inert with a green `calls=0` in the diff summary.

**Status 2026-09-06: 619 distinct names, 20 unknown.** Every one has an identifiable
real counterpart in the export table; none is a mystery. They are listed here with a
verdict so no wave rediscovers them, and each is left for **the wave that owns the
class**, because fixing one changes behaviour (null → resolved) and needs a run to
verify. Doing them cold, in a batch, with no owning wave to test them, is how a latent
name error becomes a live regression.

## Fixed already

| Name we used | The real export | When | Why fixed early |
|---|---|---|---|
| `?AdjustDelimiters@Filename@System@@QEAAXXZ` | `…@AEAAXXZ` (private) | Wave 2 | Lane A was porting Filename; the port would have been inert |
| `?GetPathIdentifer@…@QEBA?AVString@2@XZ` | `…?AV12@XZ` (returns Filename) | Wave 2 | same |
| `?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z` | `…@SAPEAX_KH@Z` | Wave 2 | phantom; 6 sites used it as their only fallback |
| `?CheckPtr@InternalBasePtr@Object@@QEAAXXZ` | `…@IEBAXXZ` (protected, const) | Wave 2 | the ONLY Object-namespace hit; clears Wave 3's exposure. Export address `0x1800c0290` is exactly the fallback already in use, so zero behaviour change |
| `?Update@UserInterface@UI@@QEAAXAEBVTime@System@@@Z` | `…@QEAAHAEBVTime@System@@@Z` — **returns int** | Wave 2 | not just a lookup string: the shared typedef said `void` and `gameupdateloop.cpp` had a local return-int cast working around it. Declaration corrected, workaround retired |

## Open, by owning wave

Twenty remain. None is in System or IO.

| Wave | Count | Namespace |
|---|---|---|
| 7 (UI) | 8 | UI |
| 5 (Achievement/Save/Picking) | 8 | Achievement 5, Save 2, Picking 1 |
| 6 (Graphics) | 3 | Graphics |
| 8 (Audio) | 1 | Audio |

### Category A — wrong access or const code (Q / U / I, and QEAA vs QEBA)

MSVC encodes access and constness in the name. A public non-const guess is wrong
whenever the real member is virtual (`U`), protected (`I`), or const (`QEBA`).

| Ours | Real |
|---|---|
| `??1Goal@Achievement@@QEAA@XZ` | `??1Goal@Achievement@@UEAA@XZ` (virtual dtor) |
| `??1GoalInstance@Achievement@@QEAA@XZ` | `??1GoalInstance@Achievement@@UEAA@XZ` |
| `?Persist@GoalInstance@Achievement@@QEAAX…` | `…@UEAAX…` |
| `?CheckMod@SaveLoad@Save@@QEAAHHI@Z` | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` (const, and `H` not `I`) |
| `?GetFirstContainedDialog@UserInterface@UI@@QEAA…` | `…@QEBA…` (const) |
| `?PlayEffect@Scene@Audio@@QEAI…` | `…@QEAA…` (`QEAI` is not a valid code) |

### Category B — reference vs pointer vs value. **Check the typedef, not just the name.**

These are the ones worth care: `AEAV` (reference), `PEAV` (pointer) and `V` (by value)
are different ABIs, so a wrong name here often travels with a wrong mental model of
what the call site passes. The name is only a lookup string, so a mismatch does not
prove the typedef is wrong — but it is the cue to check.

| Ours | Real |
|---|---|
| `?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z` | `…XPEAV12@H@Z` (pointer, not reference) |
| `?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z` | `…XPEAV12@@Z` |
| `?BringDialogToFront@UserInterface@UI@@QEAAXAEAV?$InternalPtr@…` | `…XV?$InternalPtr@…` (by VALUE) |
| `?CloseDialog@UserInterface@UI@@QEAAXAEAV?$InternalPtr@…` | `…XV?$InternalPtr@…` (by VALUE) |
| `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` | `…XAEBVString@System@@H@Z` (const ref, and plain `String` not `String<D>`) |
| `?SetText@EditElement@UI@@QEAAXVString@2@@Z` | `…XAEBVString@System@@@Z` |

### Category C — truncated names

Cut off mid-mangling, almost certainly a copy/paste truncation. Harmless today (the
fallback carries them) and trivially fixable by pasting the full export.

`?SetAchievement@AchievementInstance@Achievement@@QEAAX` ·
`?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@` (two variants, one in
`engine_funcs.cpp`, one in `address_registry.cpp`) ·
`?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@` ·
`?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@W4Usage@45@`

### Category D — malformed, and one genuinely different signature

* `?~Goal@Achievement@@QEAA@XZ` — `?~Name` is not MSVC mangling at all. A destructor is
  `??1`. The real export is `??1Goal@Achievement@@UEAA@XZ`, so this is a duplicate of
  the Category A row, hand-written rather than copied.
* `?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z`
  — the real `AddController` takes `(unsigned, Base*, ExternalPtr<DialogControllerCon…>)`.
  This is not a spelling slip; the parameter list we believe in does not match the DLL's.
  **Do not just swap the string** — reconcile the call site first. Wave 7.

## How to work one

1. `tools/ghidra_mcp/.venv/Scripts/python.exe tools/export_name_audit.py` to confirm it
   is still unknown.
2. Find the real export (`pefile` over the DLL, filter on class+method).
3. Check the `fn_*` typedef against the real signature — Category B especially, and
   remember a by-value class return means a hidden return slot as an ARGUMENT
   (WAVE1.md's ABI rule).
4. Fix the name AND the typedef AND the comment (PLAN.md §6 step 8).
5. Run. A name going from null to resolved is a behaviour change: some nulls are
   load-bearing, because a dispatch-or-fallback site deliberately wants the fallback
   (`docs/waves/unregistered_slots_wave.md`). Confirm the site still does what it did.
