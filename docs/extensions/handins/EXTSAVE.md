# EXTSAVE hand-in (2026-10-12)

Block EXTSAVE, base main `e81611a17`. This block builds the save-side machinery for class B / C
extensions. The next block (the class B pathfinding extension) only has to add its row to the
registry (`src/ext/registry.cpp`, with a `formatVersion` >= 1) and its field in `Ext::Extensions`.
Nothing in this block registers a B / C extension in the exe; `unit_b` / `unit_c` are test-only. The
game was not run (agent rule): only `tools/build.sh build` and `unit` were run, plus the static checks.

## 1. What exists

| Commit | What |
|---|---|
| EXTSAVE 1 | the pseudo-mod seam in the engine's mod-list writer, installed at boot |
| EXTSAVE 2 | `src/ext/savetail.{h,cpp}`, the block-1 tail manifest: written on save, read on load, locks the game's B / C set |
| EXTSAVE 3 | `SaveLoadDialog::DoLoad` takes the `ext:` entries out before CheckMod and shows the load notice |
| test | `tests/unit/ext_savetail_test.cpp`, 7 tests (unit 1032 -> 1039, 0 failures) |

### The manifest (`src/ext/savetail.h` has the full contract)
```
u32 magic 0x53545845 ("EXTS")  u32 version 1  u32 count
count x { u32 idLength, id bytes ([a-z0-9_]+, <= 64), i32 formatVersion, u32 payloadLength, payload }
u32 bodyLength  u32 CRC-32(body)          <- an 8-byte trailer, fixed for every version, read from the END
```
The manifest is plain standard C++ (no `IO::Serializer`), so the unit tests link it on their own and
retail's reader is never involved (it stops before the manifest, SAVETAIL §1). `payload` is empty
today. A class C extension can put its own state there later without bumping the chunk version.
`Encode` of an empty set returns **zero bytes, with no header**.

## 2. The seams (file:line at the hand-in)

- **Pseudo-mods, write (engine).** `Save::SaveLoad::_extExtraMods` (`src/engine/save/SaveLoad.h:376`,
  defined null at `saveload_meta.cpp:534`). It is a static function pointer of type
  `int (*)(ExtraModRecord*, int)`. `ExtraModRecord` has five plain fields: path, name, +0x10,
  +0x14, +0x18. In `SaveMetaData` (`saveload_meta.cpp:444-465`) it runs right after the enabled-mod
  loop and before `int32_t count = arr.size;`. When the pointer is non-null it calls the provider,
  first for the count and then for the records, and appends one `ModInfo` per record through the same
  `ModInfoArraySetSize(&arr, arr.size + 1, &tmp)`. Each `ModInfo` gets name, path,
  `versionMajor`(+0x10) = 0, `gameVersion`(+0x14) = 0x010007ff and `versionMinor`(+0x18) =
  formatVersion. `pad1c` is not written, as in retail.
- **Installed once at boot.** `Ext::InitFromSettings` (`src/ext/registry_boot.cpp:94`) sets the pointer
  to a field-by-field adapter (`EngineExtraMods`, `:64`) when `SaveModsProvider()` is non-null, and
  to null otherwise. `static_assert`s at `:56-62` prove that `Ext::PseudoModRecord` and the engine
  record have the same size and offsets. One install is enough: the profile is fixed for the run (the
  settings key or `/profile` is read at boot, and nothing changes it later). A B / C toggle needs no
  re-install, because the provider reports the ACTIVE set and returns 0 records when none is on.
- **Tail, write.** `ExtSave_WriteTail` (`src/game/Game/gameupdateloop_serialize.cpp:727`) is called
  in the SAVE arm between `SerializeAll` and `Close` (`:807`). It writes
  `Ext::SaveTail::EncodeActive()` with `CompressedStream::Write` and returns before any write
  when that is empty.
- **Tail, read + lock.** `ExtSave_ReadTail` (`:744`) is called in the LOAD arm between `SerializeAll`
  and `Close` (`:802`):
  - When `GetCurrentPos() >= GetSize()` it returns. This is a retail or faithful save, and the lock
    `GameUpdateLoop::Load` took at entry (`gameupdateloop_load.cpp:730`, `Ext::LockForGame(this)`,
    the requested set) stands. This is also the NEW-game path, unchanged.
  - Otherwise it reads the rest and runs `Decode`:
    - `NoManifest` (the bytes do not start with the magic): logged and ignored.
    - `Ok`: `Validate`, then `Ext::LockForGameWithSet(this, ids)` (`src/ext/registry.cpp:217`). The
      game's B / C set becomes exactly the saved one: listed ones on, all other B / C off, whatever
      was requested. Requested values are untouched. `EntryLocked` reads true, so the Extensions
      dialog shows them as "locked for this game". `UnlockForGame` restores the request as before.
- **Load notice.** `SaveLoadDialog::DoLoad` (`src/game/Dialog/saveloaddialog.cpp:897-925`) builds the
  visit order before the loop: real mods first, in save order, and they alone reach `CheckMod`; then
  the `ext:` entries. Each `ext:` entry gets `Ext::PseudoModLoadStatus` (`registry.cpp:292`):
  - 0 when this build has it as B / C, the player requested it, and the saved formatVersion is not
    newer than ours;
  - 1 otherwise, which gives retail's **`modsMissing` modal verbatim** (Yes continues, No aborts,
    @0 name, @1 `ext:<id>`, @2 `"<formatVersion> (1.0.7)"`).

  No new string key was added; `ExtensionsStringTable.rsc` is untouched and `ext_resources.sh` was
  not run. A save with real mods only takes exactly retail's path (same order, same CheckMod calls).

### Bad manifest: fatal, through retail's own error box
An unknown id, a non-B / C id, a formatVersion newer than ours, a duplicate, or a truncated, damaged or
newer-version chunk all lead to the same handling. `[ExtSave] FATAL: ...` goes to stderr, then
`System::Error::SetText(text, true)` + `System::Error::Display()` show the engine's fatal box and
stop the process. Why:
- `SaveRestore` is void, runs on the loader thread inside `GameUpdateLoop::Load`, and the world is
  already deserialized. Retail has no recoverable "bad save" path here: a corrupt block 1 in retail
  goes on with garbage or crashes. The only visible-failure shape retail has for unrecoverable data
  is the Error box (OOM, D3D errors), so I used that.
- A modal through `SaveLoad_ModalShowDialog` is a dialog-thread / nested-MainLoop construct and
  cannot be raised from the loader.
- Continuing would simulate a world saved under rules this build does not have.

The dialog route warns first: an unknown or newer entry already gives status 1 → modsMissing in
DoLoad, so the player is told before the commit. The scenario, tutorial and autoload routes have no
such warning and fail straight at the box.

## 3. Byte identity of the faithful profile

- **Block 0 (mod list).**
  - Under `profile=faithful` (the gate's `/profile:faithful`), `SaveModsProvider()` returns null, so
    `_extExtraMods` is null and SaveMetaData runs one null test and nothing else.
  - In today's exe, no B / C entry is registered under any profile, so the pointer is null there too.
  - Once one is registered, under per-entry / `all`, the provider returns 0 records while none is
    active, so nothing is appended.
- **Block 1 (game state).**
  - Under faithful every extension resolves off, so `ActiveRecords()` is empty and `EncodeActive()`
    returns an empty vector. `ExtSave_WriteTail` returns before any `Write`: zero bytes and no header.
  - On load, Gate.sav has no tail (`pos == size`), so `ExtSave_ReadTail` returns before reading.
- **Gates that prove it.**
  - `tools/resave_gate/resave_gate.sh` byte-compares the whole decompressed block 1 against the
    golden (`971aaec2cf95`); it must stay 5/5 identical.
  - The new-game gate (`tools/build.sh run`, `[ExtLoad] calls=5694`) and `unit` (1039 / 0 here). The
    unit test `ext_savetail_zero_bytes_when_empty` pins "faithful → zero bytes + no provider".
- **`RoundtripDecompress`** (`gameupdateloop_serialize.cpp:832`) still reads the whole image
  (`GetSize()` bytes), manifest included. Under `/profile:all` with a B / C extension active, a save
  WITHOUT a manifest (Gate.sav) resaves WITH one, so the resave gate is an **expected DIFF** in that
  profile. A save that already carries the manifest round-trips identical.
- **Static checks run here:**
  - `castmetric` total 2 (unchanged; the decoder copies the id byte by byte, with no cast);
  - `abi_audit` OK;
  - `acceptance.sh` 0 off target;
  - no new warnings in the touched files (the two `saveloaddialog.cpp` warnings at :239 / :1054 are
    pre-existing).

## 4. What the coordinator must verify at the game gate

1. **The normal faithful gate** (`tools/build.sh run`, resave x5):
   - invariants unchanged, resave == golden;
   - **no `[ExtSave]` line** in `standalone_error.log`. A `not a B / C manifest -- ignored` line on
     Gate.sav would mean retail's reader leaves bytes after the outer Serializer, against SAVETAIL §1.
     That is harmless (ignored), but record it.
2. **A save and load in the menu** under the default profile: the save's mod list is unchanged
   (DoLoad prints `save-mod-count` and no `[ExtSave] DoLoad` lines).
3. **The live exercise is the first real B extension** (the next block). `unit_b` is test-only and not
   in the exe, so nothing in this block can be clicked through. When that row lands:
   - save under `/profile:all` and check for `[ExtSave] wrote the B / C manifest: N bytes`;
   - reload it and check for `[ExtSave] manifest: 1 B / C extension(s) locked ... <id>(v1)`, with the
     Extensions dialog showing the row locked;
   - reload it under the default profile with the extension NOT requested: expect a modsMissing modal
     for `ext:<id>`; on Yes the game still runs with the extension ON (locked from the manifest);
   - load the same `.sav` in retail Banished: expect one modsMissing modal per `ext:` entry, Yes loads
     (SAVETAIL §3);
   - the resave gate under `/profile:all` from Gate.sav: an expected DIFF (above).

## 5. Doubts

- **No manifest means the requested set.** As briefed, a save without a tail keeps the lock Load took
  from the request. So a player can turn a B extension on for an existing retail or faithful save. The
  stricter reading of "locked for the game's lifetime" would lock an EMPTY B / C set for a save with
  no manifest. That is a one-line change in `ExtSave_ReadTail` (`LockForGameWithSet(owner, {})` on
  the `pos >= size` arm). It is the user's call; for a class C extension it means its state starts
  fresh on an old save.
- **Lock timing on a load.** `Load` locks the requested set at entry (`gameupdateloop_load.cpp:730`),
  and the manifest replaces it only at step 31-LOAD (`SaveRestore`), after Load's earlier steps. On a
  load, a B extension that picks its body BEFORE SaveRestore would see the requested set, not the
  saved one. The pathfinding block must pick at or after SaveRestore, or read `Ext()` where it
  dispatches (hoisted above loops, hook rule 4).
- **The notice wording.** For an extension that exists but is not requested, modsMissing says "can
  not be found or cannot be loaded", yet on Yes the game runs WITH it (from the manifest). A new
  `ExtensionsStringTable.rsc` key ("This save uses the extension @0, which will be on for this game")
  would read better. I left retail's text as briefed.
- **Fatal after a Yes.** An unknown extension shows modsMissing; if the player answers Yes, the load
  then hits the fatal box and the process exits. Refusing at DoLoad would be kinder, but it would
  change retail's Yes/No semantics, and it would not cover the scenario, tutorial and autoload routes.
- **The gate profile does not veto a manifest.** Under `/profile:faithful` a save that carries a
  manifest still locks its B / C set on. The gate never loads such a save, but a faithful-forced run
  loading a player's `/profile:all` save would not be faithful.
- **Unexercised at runtime.** `CompressedStream::Write` after `SerializeAll` and `Read` of the
  remainder before `Close` are retail's own stream members, called the way the Serializer calls them,
  but no run has passed through either arm with a non-empty manifest (see §4.3).
