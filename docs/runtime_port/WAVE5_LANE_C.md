# Wave 5 — Lane C: `Save::SaveLoad` / `Save::SavedState`

Ground-truthed 2026-09-06 on branch `wave/upgrade-path-complete`. Every address, offset and
export name below was read out of the **shipped** `C:\GOG Games\Banished\Runtime-x64.dll`
(image base `0x180000000`) — decompiled and disassembled in Ghidra, and cross-checked against
the PE export directory with `pefile`. The profile build was **not** used: the two Save classes
are small enough that every displacement came out of retail bytes directly, which sidesteps the
`reference_encoding_vtable_offsets_differ_between_builds.md` trap entirely.

**Result: 15 of the 17 real exports ported and registered; 2 held back with reasons; 1 data
export not portable before cutover; and the two names the lead flagged as suspected phantoms
are confirmed phantoms.**

Census after the lane (`tools/runtime_census.py --namespace Save`):
`Save 37 total / 20 called / 15 ported / 75.0%`.

Files owned by this lane:
* `runtime/src/save/saveload.cpp` — the port, via `RtSaveLoadRegister()`
* `runtime/types/save/SaveLoad.h` — the `static_assert`ed layouts
* this document

---

## 1. The phantom-name verdict — CONFIRMED, and it is a masked defect, not a live one

The lead asked me to adjudicate two names. Both are **phantoms**: they do not appear anywhere in
the shipped DLL's 4,926-entry export directory.

| `engine_funcs.cpp` line | name our tree asks for | in the DLL? | the real name | retail addr |
|---|---|---|---|---|
| 4523 | `?CheckMod@SaveLoad@Save@@QEAAHHI@Z` | **NO** | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` | `0x1800c1090` (ord 1691) |
| 4531 | `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` | **NO** | `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` | `0x1800c3bb0` (ord 3953) |

Both differ from the real name in the *signature* half of the mangling, which is why they look
plausible: `QEAAHHI` is a non-const member taking `(int, unsigned)`, the real one is `QEBAHHH`,
a **const** member taking `(int, int)`; and `V?$String@D@System@@` is `String` **by value**,
the real one is `AEBVString@System@@`, **by const reference**.

**What each `fn_*` is actually bound to today.** Neither is null, and that is the interesting
part. `ResolveExport` on a name the DLL does not contain returns NULL silently
(`reference_resolveexport_phantom_names.md`), so both `if (!fn_…)` fallbacks fire, and both
fall back to the exe's PE import slot:

```
engine_funcs.cpp:4525   fn_SaveLoad_CheckMod    = *(fn**)ResolveAddr(Exe, 0x140152738)
engine_funcs.cpp:4533   fn_SaveLoad_SetSaveName = *(fn**)ResolveAddr(Exe, 0x140151650)
```

and those two slots are registered in `standalone/address_registry.cpp:1421` and `:1423`
under the **correct** names. `tools/dispatch_slot_map.py` agrees with the binary:

```
0x140152738 -> Runtime-x64.dll ?CheckMod@SaveLoad@Save@@QEBAHHH@Z
0x140151650 -> Runtime-x64.dll ?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z
```

So:

* **This is NOT a live defect.** Both function pointers hold the right function today. Both
  `fn_*_t` typedefs (`engine_funcs.h:4118` and `:4124`) already describe the *real* signature —
  `int(void*, int, int)` and `void(void*, const String*, int)` — not the phantom's, so the call
  sites are correct too. The Wave 4 lane H investigation that leaned on
  `fn_SaveLoad_CheckMod` returning 1 was reading a genuine `CheckMod` result.
* **It IS a masked defect**, on two counts. First, the primary resolution path at
  `engine_funcs.cpp:4523/4531` is dead code that has never once produced a pointer, and the
  comment above each one ("Runtime DLL export (via dispatch table at …)") reads as though the
  export path is the normal one and the slot is the backup. It is the other way round. Second,
  the fallback only works because someone independently registered those two slots with the
  right name; delete either `RegisterDispatchSlot` line — as the unregistered-slots wave was
  routinely doing — and both `fn_*` become NULL with no diagnostic at all, and
  `saveloaddialog.cpp:1114` calls a null pointer on the load path.
* **Init order makes my port the winner**, which is what we want. `standalone/main.cpp` runs
  `RuntimePortInit()` (:1614) → `AddressRegistryInit()` (:1626) → `EngineFuncsInit()` (:1631),
  so `address_registry.cpp:1421`'s `ResolveExport` picks up **this lane's** `CheckMod`, stores
  it in the slot, and `engine_funcs`' fallback reads it back out. Both functions therefore reach
  the port through the existing wiring with no edit to `engine_funcs.cpp` — which is why I have
  not made one. **The fix belongs to whoever owns `engine/funcs/`: replace the two phantom
  strings with the real names and delete the fallbacks.** Listed again in §5.

`tools/export_name_audit.py` reports 17 phantom names across the tree, of which these are two.
The other 15 are outside this lane.

---

## 2. Per-function table

All addresses are retail. "profile addr" is deliberately blank for every row: this lane never
needed the profile build, because every one of these functions is exported from retail under
its own mangled name, so there was nothing to recover from a string anchor. Saying so is more
useful than pasting a hash-match nobody verified.

| # | Export | Retail | What it does | Oracle | Status |
|---|---|---|---|---|---|
| 1 | `??0SaveLoad@Save@@QEAA@XZ` | `0x1800c3d30` | Zeroes the object, `_selectedIndex = -1`, sprite index `-1`, publishes `_singleton = this` | scratch-copy diff, both buffers pre-filled `0xCD`, theirs FIRST | **ported** |
| 2 | `??1SaveLoad@Save@@QEAA@XZ` | `0x1800c3c90` | Clears the singleton, releases two Strings, destroys `Array<SaveInfo>`, frees `+0x30` | none possible | **HELD BACK** |
| 3 | `??0SavedState@Save@@QEAA@XZ` | `0x1800c0ee0` | Two qword zero stores | scratch-copy diff, `0xCD`-filled | **ported** |
| 4 | `??1SavedState@Save@@QEAA@XZ` | `0x1800c0ea0` | `if (_data) locked-free(_data)` | **none** (a second run double-frees) | **ported, unmeasured** |
| 5 | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` | `0x1800c1090` | Matches a save's recorded mod against `IO::Modification` | **full diff** (idempotent) | **ported** |
| 6 | `?Draw@SaveLoad@Save@@…` | `0x1800c2eb0` | Renders the save thumbnail to an offscreen target | none possible | **HELD BACK** |
| 7 | `?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ` | `0x1800c3c80` | `return *(int*)this` | **full diff** | **ported** |
| 8 | `?GetSaveCount@SaveLoad@Save@@QEBAHXZ` | `0x1800c3b80` | `return *(int*)(this+0x58)` | **full diff** | **ported** |
| 9 | `?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z` | `0x1800c3b60` | `_savesData + index*0x58`, unbounded | **full diff** | **ported** |
| 10 | `?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ` | `0x1800c3b50` | `return *(SavedState**)(this+0x40)` | **full diff** | **ported** |
| 11 | `?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` | `0x1800c0e50` | `MemoryStream::Open(size)` + `memcpy` into the stream | **scratch-object diff** | **ported** |
| 12 | `?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z` | `0x1800c3c40` | one qword copy to `+0x10` | scratch-copy diff | **ported** |
| 13 | `?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z` | `0x1800c3c50` | sets op, zeroes `_progress` and `_elapsed`, stores the index | scratch-copy diff | **ported** |
| 14 | `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` | `0x1800c3bb0` | refcounted String assign to `+0x70` + mode to `+0x78` | scratch-copy diff **with refcount compensation** | **ported** |
| 15 | `?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z` | `0x1800c3b90` | 16-byte copy to `+0x60` | scratch-copy diff | **ported** |
| 16 | `?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` | `0x1800c0da0` | detaches a private copy of the stream's bytes | **scratch-object diff** | **ported** |
| 17 | `?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z` | `0x1800c2e40` | accumulates the frame delta, dispatches to 5 `UpdateXxx` | **partial**: shadow on the accumulation, **none on the dispatch** | **ported** |
| — | `?_singleton@SaveLoad@Save@@0PEAV12@EA` | `0x1801fc6f0` | DATA export | n/a | **not portable before cutover** |

### The two held-back functions, in full

**`?Draw@SaveLoad@Save@@…` (`0x1800c2eb0`).** ~400 decompiled lines. It `operator new`s and
hand-constructs a `Graphics::FrameCopyImageBuffer` (0x88 bytes), a `Video::ImageBuffer` (0x68),
a `Video::RenderBuffer` (0x30), a `Graphics::MaterialInstance` (0x60) and a `UI::SpriteSheet`
(0x58), stamping `Object::Base`, `Object::External`, `Graphics::MaterialInstance` and
`UI::SpriteSheet` vtables in sequence, then builds a `Graphics::Camera` from a hand-filled
frustum and drives a `UI::SpriteContext`. Those five classes are Waves 6 and 7; porting Draw
means porting their constructors byte-faithfully first, which is a wave, not a function. It
also has **no oracle**: it renders into the live `Video::Context`, changes its viewport and
scissor state, `MemAlloc`s a `w*h*4` readback buffer into `+0x88`, and destroys its render
target — running both implementations would draw twice and leak a render target per call.
Note it is a no-op unless `+0x7c` is set, so it costs one predicted-not-taken branch today.

**`??1SaveLoad@Save@@QEAA@XZ` (`0x1800c3c90`).** The destructor walks the `Array<SaveInfo>` at
`+0x50` through `FUN_1800329c0`, which calls the **un-exported** per-element destructor
`FUN_1800c3f40` once per 0x58-byte element and then locked-frees the buffer. Three reasons to
leave it: no oracle is possible (it frees); the object is a process-lifetime singleton, so this
is unlikely ever to run; and porting it means calling an un-named `FUN_` by address on the heap
path, where a mistake is silent corruption rather than a crash. The decompiled body is recorded
as a comment in `saveload.cpp` so the next reader does not redo the work.

---

## 3. The oracle shapes, and why each one is what it is

The lead's brief made the point that decides this lane: `Store`/`Retrieve` move a cursor through
a `MemoryStream`, so running both implementations would double-consume it — the Wave 4 failure
mode. Three shapes came out of thinking about that first.

**Full diff** — both run, results compared. Used only where the function is read-only or
provably idempotent. `CheckMod` qualifies despite allocating: it builds three temporary
`System::String`s and releases all three before returning, and mutates nothing else, so the
second call sees byte-identical inputs.

**Scratch-copy diff** — the function writes only into `*this` and allocates nothing, so two
private 0x90-byte copies of the live object are made, ours runs on one and theirs on the other,
the two are compared byte for byte, and *only then* is ours applied to the real object. Neither
scratch run touches shared state. This is a genuine differential oracle, not a self-check.

For the two constructors the scratch buffers are pre-filled with `0xCD` rather than copied from
the live object, deliberately: `??0SaveLoad` leaves seven fields untouched (`+0x18`, `+0x3c`,
all eight bytes of `+0x40`, `+0x6c`, `+0x74`, `+0x78`), and a zero-filled comparison would
agree on a missing store into an already-zero field. Starting from `0xCD` makes the oracle prove
what the ctor writes *and* what it deliberately does not.

`??0SaveLoad`'s two runs are also **ordered**: theirs first, ours second, because the last thing
the constructor does is publish `_singleton = this`. Running theirs second would leave the DLL
global pointing at a stack buffer about to go out of scope.

**`SetSaveName` needed one extra idea.** The naive scratch diff is *unsafe* here: both runs
manipulate the refcounts of the same two shared `String`s, so the old string is decremented
twice, and if its count was 1 the first decrement frees it and the second writes through freed
memory. That is Wave 4 lane H's defect in a new costume. The fix is to make the extra pair of
operations provably harmless: **add 2 to the old string's refcount before the two scratch runs**
(so the two decrements can never reach zero and never free) and **subtract 2 from the new
string's refcount after** (undoing the two increments) — conditional on the assignment actually
happening, because retail's short path (`old == new`) does no refcount work at all. The live
object is then updated by one ordinary run. Net effect: exactly one assignment, nothing freed
early, nothing leaked.

**Scratch-object diff** — for `Store` and `Retrieve`, theirs runs against a private replica of
the *output* object, and the buffer it produces is compared against ours and then freed:

* `Store`: theirs gets a zeroed `SavedState`, so its `if (_data) MemFree(_data)` is skipped and
  it allocates its own buffer. Both runs only **read** the stream, so nothing is double-consumed.
  We compare `_size` and `memcmp` the payload, then `MemFree` theirs.
* `Retrieve`: the stream's 0x30 object bytes are captured *before* ours mutates it, theirs runs
  against that copy, and we compare `_direction` / `_ownsData` / `_cursor` / `_size` and the
  payload before freeing theirs' buffer. It has to be done this way rather than by running
  theirs a second time on the live stream, because `MemoryStream::Open` (`0x1800ae670`,
  decompiled) does **not** free the buffer the stream already holds — a second run would leak
  one buffer per call. That leak is retail's own on the first run and is reproduced faithfully.

**`Update` is the one partial oracle, and I am not going to dress it up.** The time
accumulation is checked in shadow mode against an independent replica. The **dispatch is checked
by nothing**, because running it twice would perform the save/load/delete file work twice. What
backs it is that all five `UpdateXxx` targets stay on the DLL and the branch is five integer
comparisons transcribed from a decompile — plus end-to-end evidence that a save still saves and
a load still loads. If that is not good enough for the wave, `Update` is the one function to pull.

**`~SavedState` has no oracle at all** and is registered anyway: nine unambiguous instructions
(`if (p) locked-free(p)`, which is literally `MemFree`'s body), measured only by `calls=`.

---

## 4. Facts this lane established, corrected, or could not confirm

**Established**

* `sizeof(Save::SaveLoad) == 0x90`, corroborated two ways: the ctor's last store is at `+0x88`,
  and `standalone/menudialogloopt_stubs.cpp:265` has been `MemAlloc(0x90)`ing this object for
  months. `sizeof(SavedState) == 0x10`, `sizeof(SaveInfo) == 0x58`, `sizeof(ModInfo) == 0x20`.
* **`SaveLoad` is not a polymorphic class.** The ctor writes an int at `+0x00`, not a vptr, and
  the DLL exports no `??_7SaveLoad@Save@@6B@`. PLAN §4.3 risk 3 (vtable ownership) simply does
  not apply to this lane — worth recording, because the risk register implies it applies to
  every class.
* **`??0SaveLoad` does not initialise `_saveState` (`+0x40`).** `GetSaveState` therefore returns
  whatever the allocator left there until an operation sets it. Reproduced faithfully; a reader
  who assumes the ctor zeroes the whole object will be wrong.
* `SetOperation` zeroes `_progress` (`+0x18`); the constructor does **not**.
* The `Operation` values, from `Update`'s switch: 1 List, 2 Load, 3 LoadFromFile, 4 Save,
  5 Delete, 0 none. The DLL exports no enumerator symbols, so those names are ours.

**Corrected**

* **`ModInfo+0x00` is the display name; `ModInfo+0x08` is the path, and `CheckMod` compares
  `+0x08`.** `code/Game/Dialog/saveloaddialog.cpp:1119` and `:1131` read the name at `+0x00` and
  the comment there describes `+0x00` as "String at stride 0x20". That is right for printing but
  it invites the wrong conclusion about what `CheckMod` matches. The instruction is
  `LEA R8,[RBP+0x8]` immediately before the `System::operator+` call: retail concatenates the
  data directory with the string at `+0x08` and matches *that* against `IO::Modification`'s
  per-entry path String at `+0x58`.
* **WAVE4_LANE_H.md §5a's description of `CheckMod` is right in outcome and vague in one place
  that matters.** It says the function builds "`GetDataDirectory()` + *the save's recorded mod
  path string*" without naming the field. It is `+0x08`, per the above. Its status codes
  (1 missing / 4 changed / 3 version / 2 out-of-date / 0 fine) and its search shape
  (count at `+0x10`, base at `+0x8`, stride `0x80`, String at `+0x58`, length pre-check then
  `_stricmp`) all reproduce exactly and are confirmed here.

**Not confirmed / not modelled, stated rather than guessed**

* `SaveInfo+0x10..0x37` and `+0x40..0x47` are **not** modelled. They are written by
  `?UpdateList@SaveLoad` (`0x1800c1ea0`) and `?LoadMetaData@SaveLoad` (`0x1800c1b30`), neither of
  which this lane ports or needed to read.
* `IO::Persistence+0x18` (bit 0) and `+0x48` are the two fields `Update`'s gate reads. This lane
  does **not** name them: `runtime/types/io/Persistence.h` (Wave 4 lane C) explicitly models
  `+0x08..+0x8f` as "NOT modelled", and inventing two field names in another lane's header is
  exactly what PLAN §6 step 8 exists to prevent. They are read as raw displacements with a
  comment saying so.
* `Sprite+0x0c` is written by nothing this lane saw.

**Nothing in PLAN.md, CLAUDE.md or the memory notes was disproved by this lane.**
Two things were *confirmed* that had only been claimed:
`reference_resolveexport_phantom_names.md`'s mechanism (two more instances, §1), and
`reference_unresolved_lookup_leaves_a_valid_looking_zero.md`'s "sprite index 0 is real, empty is
-1" (the SaveLoad ctor writes `-1` into the sprite index, which is the writer's side of it).

---

## 5. Comments to correct (PLAN §6 step 8) — all outside this lane's files

I own three files and have edited only those, so these are reported, not fixed.

| file:line | what it says | what is true |
|---|---|---|
| `engine/funcs/engine_funcs.cpp:4521-4529` | `// Save::SaveLoad::CheckMod — Runtime DLL export (via dispatch table at 0x140152738)` over `ResolveExport("?CheckMod@SaveLoad@Save@@QEAAHHI@Z")` with a slot fallback | The export name is a **phantom**; that `ResolveExport` has never returned non-NULL. The fallback is the *only* path. Real name: `?CheckMod@SaveLoad@Save@@QEBAHHH@Z`. Replace the string and delete the fallback. |
| `engine/funcs/engine_funcs.cpp:4530-4535` | same shape for `SetSaveName` with `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` | Also a phantom. Real name: `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z`. Note the typedef at `engine_funcs.h:4124` is already correct (`const String*`, not by value) — only the resolve string is wrong. |
| `code/Game/Dialog/saveloaddialog.cpp:1130-1132` | `// saveInfo+0x48 = base of mod name array; each entry is 0x20 bytes; entry+0 = String` | Correct as far as it goes, but `CheckMod` matches `entry+0x08` (the path), not `entry+0x00` (the display name). Exact replacement wording below. |
| `docs/runtime_port/WAVE4_LANE_H.md:305` | "builds `GetDataDirectory() + <the save's recorded mod path string>`" | Right, and now specific: that string is `ModInfo+0x08`. Exact replacement wording below. |

### Exact wording for those two edits

The team lead owns both files. Proposed text, ready to paste.

**`code/Game/Dialog/saveloaddialog.cpp`, the comment at 1130-1132.** Replace:

```cpp
        // Get mod name string pointer: saveInfo->modList[i] (stride 0x20 from base at +0x48)
        // saveInfo+0x48 = base of mod name array; each entry is 0x20 bytes; entry+0 = String
```

with:

```cpp
        // saveInfo+0x48 = base of the save's ModInfo array; each entry is 0x20 bytes.
        // entry+0x00 is the mod's DISPLAY NAME ("Debug Menu") and is what the
        // warning dialog shows. It is NOT what CheckMod compares: CheckMod
        // (retail 0x1800c1090) concatenates FileSystem::GetDataDirectory() with
        // the String at entry+**0x08** — the package path relative to the data
        // directory — and matches that against IO::Modification's per-entry path
        // String at +0x58. The proof is one instruction, `LEA R8,[RBP+0x8]`
        // immediately before the System::operator+ call, where RBP is
        // modIndex*0x20 + saveInfo->modsData. So a save can name a mod whose
        // display name you recognise and still report missing, because the two
        // strings are different fields. Layout: runtime/types/save/SaveLoad.h.
```

**`docs/runtime_port/WAVE4_LANE_H.md`, §5a "The function that does the comparing", step 1.**
Replace:

> 1. builds `IO::FileSystem::GetDataDirectory() + <the save's recorded mod path string>` and runs
>    `System::Filename::AdjustDelimiters` on it;

with:

> 1. builds `IO::FileSystem::GetDataDirectory() + <the save's ModInfo+0x08 string>` and runs
>    `System::Filename::AdjustDelimiters` on it. **The field is `ModInfo+0x08`, the package path
>    relative to the data directory — not `+0x00`, which is the display name the `DoLoad` trace
>    below prints.** The proof is `LEA R8,[RBP+0x8]` immediately before the `System::operator+`
>    call, with `RBP = modIndex*0x20 + saveInfo->modsData` (Wave 5 lane C, which ported the
>    function; layout in `runtime/types/save/SaveLoad.h`);

Nothing else in that section changes: the status codes (1/4/3/2/0), the stride `0x80`, the
count at `+0x10`, the base at `+0x8` and the entry String at `+0x58` all reproduce exactly and
are confirmed by this lane's port.

---

## 6. Verification — what I did and what I did not

**Did:**
* `mingw32-make standalone-debug` — clean, from a state where the whole tree rebuilt.
  Two rounds: the first caught a real layout error in my own header (an extra `pad44` after
  `_saveState`, which shifted everything from `+0x48` on by 4 bytes). The `static_assert`s in
  `runtime/types/save/SaveLoad.h` caught it at compile time, which is what they are for.
* `tools/runtime_census.py --namespace Save` → **Save 20 called / 15 ported / 75.0%**, and the
  15 registrations are all spelled as single one-line string literals, so the census can see
  every one (the WAVE3.md 21-vs-27 undercount cannot recur here).
* `tools/abi_retslot_audit.py` → **0 suspicious of 49 by-value-returning exports**. Note the
  tool excludes enum returns, so `GetOperation` was checked by hand instead: the whole retail
  function is `MOV EAX,[RCX] ; RET`, `this` arrives in RCX and RDX is untouched, so a 4-byte
  enum comes back in EAX with **no** hidden return slot.
* `tools/export_name_audit.py` → 17 phantoms tree-wide, two of them mine, adjudicated in §1.
  Every one of my 15 registration names was independently checked against the shipped export
  directory with `pefile` before it was written.
* Read every retail body at both the decompiler and the instruction level. `SetSaveSprite`,
  `??0SaveLoad`, `Retrieve` and `CheckMod` were disassembled specifically to settle ABI
  questions the decompiler papers over.

**Did NOT** run the game myself (as instructed — the orchestrator runs the integration passes).

### Live results — the team lead's integration pass, 2026-09-06

Autostart + pause-menu quit with `BANISHED_RT_DIFF=1` and a full world teardown:
**91 sites, 24,050,239 calls, 99 divergences — all 99 confined to the two pre-existing Wave 1
sin/cos sites. No new divergent site anywhere.** Clean exit, empty crash log.

This lane's rows from that run:

| Function | `calls=` | `divergences=` |
|---|---|---|
| `Save::SaveLoad::SaveLoad` | 2 | 0 |
| `Save::SaveLoad::Update [shadow]` | 1,039 | 0 |

So the blanket "registered but never executed" caveat is **partly** retired — for two functions.
Read that table the way this project's own scar tissue says to read it
(`reference_uncalled_port_is_not_verified.md`): **the other 13 registered functions still show
nothing from this run**, and the two that do show something are the two the menu path happens to
touch. In particular the run drives no save/load operation, so `_operation` is `0` for all 1,039
`Update` calls and the switch never dispatched — see the recommendation below. Runs #1 and #2
are still the ones that matter.

What the run does settle: `??0SaveLoad`'s ordered scratch-copy diff (theirs first, because the
ctor publishes `_singleton = this` and ours must be the last writer) survives a real run twice
over, with the singleton left pointing at the live object both times.

### ABI traps checked, one by one

* `SetSaveSprite(UI::Sprite)` — a 16-byte class **by value**, so the MS x64 ABI passes it by
  **hidden pointer**. Confirmed from the bytes (`MOV RAX,[RDX]` — RDX is dereferenced). The
  parameter is declared `const Sprite*`. Declaring it by value would have made GCC read the
  caller's 16 bytes out of registers and corrupt the sprite.
* `SetSaveName(const String&, int)` — by **const reference**, one pointer, no hidden slot. (The
  phantom name claimed by-value; the real export does not.)
* `GetOperation()` — enum by value, 4 bytes, **no hidden slot** (see above).
* `GetSaveInfo(int)` — returns `SaveInfo const&` (`AEBU`), a **reference**, so RAX and no hidden
  slot. `?AU` would have been by value and would have needed one.
* Argument counts were taken from the **mangled names**, not from Ghidra's guesses, per
  `reference_hidden_retval_and_int_pointer_args.md`.

### The runs that would exercise this lane, and what a PASS looks like

All of these need `BANISHED_RT_DIFF=1` and a clean self-quit, because `RtDiffSummary()` prints
from `WinMain` rather than `atexit`:
`BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=6`.

1. **The load path — the important one.**
   `BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1` + the self-quit knobs.
   **Do NOT set `BANISHED_MODAL_ANSWER` on this run.** Wave 4 lane H made the unattended default
   answer **2** (confirm) for the save/load modals; CLAUDE.md's mods-menu example passes `1`,
   which is a *different dialog*, and setting `1` here cancels the load and reproduces lane H's
   stall exactly. The team lead hit this on 2026-09-06. Leave it unset and the harness answers
   correctly on its own.
   Exercises `??0SaveLoad` (the menu `MenuDLT::Stub_Startup` builds one), `Update` (every frame
   from the menu loop), `SetOperation`, `GetOperation`, `GetSaveCount`, `GetSaveInfo`,
   `SetSaveName`, and — because this machine's saves all name the uninstalled `"Debug Menu"` mod
   — **`CheckMod`, which should return 1** and drive the `modsMissing` path Wave 4 lane H made
   unattended. `Store`/`Retrieve`/`GetSaveState` ride the `SavedState` handoff in
   `gameupdateloop_serialize.cpp:179/181`.
   **PASS**: `[RtDiff]` summary shows every `Save::…` site with a **non-zero `calls=`** and
   `divergences=0`; the world builds and reaches gameplay; `standalone_crash.log` empty.
   The only expected residual divergences anywhere are the known Wave 1 1-ULP trig sites
   (`Vector3::FromYawPitch`, `Transform::FromYawPitchOffset`, `Vector3::Heading`).
2. **The save path.** `BANISHED_AUTOSTART=1 BANISHED_SAVE_TEST=<frame> BANISHED_RT_DIFF=1`
   (`standalone/saveload_harness.cpp`) — this is the only run that drives
   `SetOperation(kOpSave)` → `UpdateSave` and therefore the `Store` side of `SavedState`.
   **PASS**: a save file appears, `Save::SavedState::Store` shows non-zero `calls=` and
   `divergences=0`, `Save::SaveLoad::SetOperation` and `…::Update [shadow]` likewise.
3. **The A/B control.** The same load run with `BANISHED_RT_SAVELOAD=0`. If run 1 misbehaves,
   this says in one run whether it is this lane.
4. **`sa-stress`** adds nothing here — the stress town never opens the save menu — so the perf
   question for this lane is whether the always-on `Update` dispatcher costs anything. It is one
   `switch` plus two loads per frame, and both the Persistence singleton and the five `UpdateXxx`
   targets are `RT_ORIGINAL_ONCE`-cached, so there is **no `GetProcAddress` on any per-frame
   path** (`reference_rtoriginal_is_getprocaddress.md`).

### `Update`: keep it registered — with a stated gate

The team lead asked me directly whether `Update` should stay registered, given that its 1,039
clean calls prove the time accumulation and not the switch. **Recommendation: KEEP, gated on
runs #1 and #2 passing.** The reasoning, so the choice can be re-examined rather than trusted:

* **The 1,039 calls prove less than they look like.** All of them ran with `_operation == 0`,
  so they fell through the switch to `default`. They are evidence about the accumulation and
  about the Persistence gate not faulting, and about nothing else. Saying "1,039 calls, 0
  divergences" without that sentence would be exactly the rounding-off this project keeps
  paying for.
* **But the dispatch is not untestable — it is untested by the run that has happened.** A load
  that completes proves case 2 (`UpdateLoad`) and case 1 (`UpdateList`, which populates the save
  list the dialog shows) dispatched correctly; a save file appearing proves case 4
  (`UpdateSave`). Runs #1 and #2 test the switch end to end, they are already on the list, and
  the lead has said they are coming. This is a gap in coverage with a scheduled closing date,
  not a permanent blind spot.
* **The failure mode is loud, not silent** — which is the opposite of the usual argument for
  pulling a Save-namespace port. A wrong branch either dispatches nothing (the save/load spinner
  never finishes: immediately visible, and the exact symptom lane H spent a day on, so it is the
  best-recognised failure signature in the project) or dispatches the wrong `UpdateXxx` (a load
  that deletes, which run #1 would surface at once). Neither is the quiet save-desync this lane
  was warned about, because `Update` moves no bytes and touches no stream.
* **The surface is small and its two fields are independently proven.** The body reads exactly
  two members: `_operation` at `+0x00`, whose offset is proven by `GetOperation`'s and
  `SetOperation`'s full/scratch diffs, and `_elapsed` at `+0x08`, proven by the shadow check.
  All five dispatch targets stay on the DLL. There is no arithmetic, no allocation, no layout
  risk left unproven.

**The gate, concretely: if run #1 or run #2 fails, pull `Update` first** — comment out its one
`RegisterRuntimeExport` line, which reverts the whole dispatcher to the DLL and leaves the other
14 ports untouched. If both pass, `Update` has a genuine end-to-end oracle and this note can be
closed. I would not keep it registered if it shipped without those two runs.

### Honest residual risk

The other one I would look at:

* **`SetSaveName`'s diff-mode compensation** — the refcount pre-bump/post-fix is correct by
  construction, but it is the only place in the lane where *diff mode itself* touches shared
  state. If a divergence hunt ever turns strange around save names, run with `BANISHED_RT_DIFF`
  unset first: the compensation is inside `if (rt_diff_on__)` and does not exist in a normal run.
