# SAVETAIL -- save-compat verification for DESIGN.md §2 / §5

Block SAVETAIL, 2026-10-11, base `d5bc23b57`. Verification only: no game code changed, the game was
not run. Retail evidence comes from Ghidra (Application-x64.exe) and from `objdump -d` of the shipped
`Runtime-x64.dll`; our side is cited as file:line.

## Verdict (one line each)

1. **Trailing bytes: VERIFIED, and the design must change where the chunk goes.** Bytes appended to the
   `.sav` FILE are not ignored: retail's GET-SAVE-INFO job requires the exact file size (0x1800b67ed
   `cmp 0x48(%rsp),%edx ; jne 0x1800b691b` -> status 3). Bytes appended INSIDE block 1's compressed
   stream, after `SerializeAll` returns, ARE ignored by retail's reader. That is where the class-C chunk goes.
2. **Mod list in the save: VERIFIED** (writer 0x1800c1490, reader 0x1800c1b30). The path compared is
   ModInfo+0x08 (0x1800c10dc `lea 0x8(%rbp),%r8`). `ext:<id>` can never match a real `.pkm` on
   Windows, and on Linux it cannot match as long as `<id>` never ends in `.pkm`. AdjustDelimiters
   does not change a colon. Correction: ModInfo+0x10 is the package **CRC**, not a version.
3. **Modal path: VERIFIED (0x1400b61e0).** modsMissing does not abort the load. It is a Yes/No modal:
   Yes (2) continues and No (3) aborts. The pseudo-mod idea works in RETAIL, with one modal for each
   `ext:` entry.
4. **Our loader: seams named below.** The resave gate (default mode) byte-compares the WHOLE
   decompressed block-1 image against the golden, so a tail chunk would show up there. It does not look
   at block 0 (the mod list). At `d5bc23b57` no extension code exists, so the faithful profile writes
   neither the chunk nor a pseudo-mod. That is VERIFIED only because there is no such code yet. It is
   also a rule the implementation must keep.
5. **"Different mod set" notices: VERIFIED.** There are two. On load it is `SaveLoadDialog::DoLoad`
   0x1400b61e0 (`Dialog/SaveLoad.rsc:modsMissing|modsOutOfDate|modsVersion|modsChanged`). When mods
   are toggled during a game it is `ModDialog::Apply` 0x14006ea70 (`Dialog/Reload.rsc:game|menu`).

---

## 1. Trailing bytes -- VERIFIED: not at the file tail; inside block 1's tail instead

### The file format (Persistence worker, Runtime)
SAVE job 0x1800b6e00 (`src/engine/io/persistence_worker.cpp:556`) writes:

```
u32 fileHeader   = version(16) | blockCount(16)     version 2, blockCount 2
u32 magic        = 0x80808080
u32 crc          CRC-32 over the descriptors, then each block's payload (0x8000-byte chunks)
blockCount x {u32 id, u32 offset, u32 size}         (BlockDesc, stride 0xc)
block 0 payload  = SaveLoad::_embeddedState         the metadata record (SaveMetaData)
block 1 payload  = the heap SavedState              the game state (GameUpdateLoop::SaveRestore)
```
`UpdateSave` builds exactly two blocks (`saveload_update.cpp:981`, retail 0x1800c2c60..0x1800c2c88).

### The CRC and the size check cover the whole file
GET-SAVE-INFO job 0x1800b65b0 (`src/engine/io/persistence_saveinfo_job.cpp`):
- the header's block count must equal the requested count, 2 (`BeginGetSaveGameInfo(row.name, 2)`,
  `saveload_update.cpp:658` / `:1544`). A third SaveBlock therefore fails with status 3.
- **file size must equal `12 + count*0xc + sum(desc.size)` exactly** (`:248`). Retail bytes:
  ```
  1800b67e1  add (%rax),%edx          ; sum desc[i].size
  1800b67ed  cmp 0x48(%rsp),%edx      ; == FileInfo.size ?
  1800b67f1  jne 0x1800b691b
  1800b691b  movl $0x3,0x38(%r15)     ; status = 3 (not a save of this shape)
  ```
- then the CRC is recomputed over the descriptors and each block (`:287`, 0x1800b68f3). A mismatch also
  gives status 3.

What status 3 does in retail: `UpdateList` states 3/4 gate the metadata load on `status == 1`
(`saveload_update.cpp:1552`, `:1572`). A file with bytes appended after its last block is listed with an
empty description, a black thumbnail and **an empty mod list**. Its pseudo-mod entries are then never
read, so the modsMissing notice never fires. (`UpdateLoad` state 1 does not test the status and the
descriptors were already read, so the load itself would still go through.) **A chunk at the file tail
defeats the mechanism.**

### Inside block 1, after the last Serialize: ignored by retail's reader
Retail `GameUpdateLoop::SaveRestore` 0x1400959c0 (Ghidra):
```
load:  SavedState::Retrieve(state, &cms.memoryStream); CompressedMemoryStream::Open(&cms);
       <0x140095a90 master serializer>(this, &cms);   CompressedMemoryStream::Close(&cms);
save:  CompressedMemoryStream::Create(&cms); <0x140095a90>(this, &cms);
       CompressedMemoryStream::Close(&cms);  SavedState::Store(state, &cms.memoryStream);
```
- `Open`'s result is discarded. `Open` (0x1800aa560, `compressed_stream_open.cpp:99`) checks only that
  the window table adds up to the compressed image length. Bytes written before `Close` are committed
  into that same table, so the check stays consistent.
- `CompressedMemoryStream::Close` (0x1800a9620, `compressedstream.cpp:482`) runs `Commit` only when
  the direction is 1 (writing). The read arm compares no position and reads no trailer.
- The outermost `IO::Serializer` in 0x140095a90 has already consumed its own 4-byte trailer
  (`~BinaryReader` 0x1800a82f0 reads exactly 4 bytes from the current position, and `Skip` is never
  called), so nothing in retail reads past it.
- After `SaveRestore`, `SetupLoad` sizes the buffer from the descriptor (`savedstate_setupload.cpp`),
  and the descriptor size plus the CRC are computed by the SAVE job over whatever block 1 holds. So a
  chunk inside block 1 is covered by both, automatically.

**Where the class-C chunk goes:** in `GameUpdateLoop::SaveRestore`'s save arm, between
`SerializeAll(*cms)` and `cms->Close()` (`src/game/Game/gameupdateloop_serialize.cpp:723-724`). It
should be written through its own `IO::Serializer` so it gets a 0xAAAAAAAA/length/version header and
its own trailer. On load our build reads it in the load arm between `SerializeAll` and `Close`
(`:719-720`) when `GetCurrentPos() < GetSize()`. Retail stops at `:720`'s equivalent and never sees it.
No sidecar file is needed.

## 2. The mod list in the save -- VERIFIED

**Writer:** `SaveLoad::SaveMetaData` 0x1800c1490 (`src/engine/save/saveload_meta.cpp`), block 0. It
uses an `IO::Serializer` at version 3 (`MOV R8D,0x3` 0x1800c1570) over a `CompressedMemoryStream`
with 2 windows:
```
String name; int32 width; int32 height; bytes w*h*4 (BGRA); int32 nameMode;
int32 count;                                 (only ENABLED mods, Mod+0x50 != 0)
count x { String name     <- Mod+0x00
          String path     <- Mod+0x58 + len(GetDataDirectory())   (prefix stripped, :426)
          u32 +0x10       <- Mod+0x2c    0x1800c16ed
          u32 +0x14       <- Mod+0x18    0x1800c16f4
          u32 +0x18       <- Mod+0x1c }  0x1800c16fb
```
The three dwords go through ReaderWriter +0x98 (SerializeUInt), at 0x1800c17ea / 0x1800c17ff /
0x1800c1814. The two Strings go through `String::Serialize` 0x1800c9930 (0x1800c17c9 / 0x1800c17d7).

**Reader:** `SaveLoad::LoadMetaData` 0x1800c1b30 (`saveload_loadmeta.cpp:322-349`). The mod block is
read only when the Serializer version is > 2 (0x1800c1c9a `cmpl $0x2,(%r15)`). It reads the version a
second time and the count (+0xa0 twice), then `SetSize` (0x180034010), then for each entry
`call 0x1800c9930` x2 and `call *0x98` x3 (0x1800c1d09..0x1800c1d54). The order is the same as the
writer's. Its only caller is `UpdateList` state 4 (`saveload_update.cpp`), which stores the result in
`SaveInfo+0x48`.

**String encoding** (`String::Serialize` 0x1800c9930, `src/engine/system/string.cpp:891`): `u32 n`
= byte length INCLUDING the NUL, with bit 31 set as the UTF-8 flag (`n | 0x80000000`), then n raw
bytes. A null String writes `0x80000000` and no bytes. A clear bit 31 on read means legacy UTF-16
(`n` u16 units).

**The three dwords, named by their source** (PackageFile accessors, `modification_refresh.cpp:511-515`):
- +0x10 = `Mod+0x2c` = **`PackageFile::GetCRC`** (0x180025500). `SaveLoad.h:241` calls it
  `versionMajor`, which is wrong. CheckMod status 4, "modsChanged", is a CRC mismatch, and the
  `ModChanged` text agrees ("different from when the game was saved ... without changing the version
  number").
- +0x14 = `Mod+0x18` = `PackageFile::GetVersion` (0x1800253d0), the game/toolkit version the `.pkm` was
  built with, packed `major<<24|minor<<16|patch<<8|suffix`. CheckMod returns 2 (modsOutOfDate) when it
  is `< 0x1000400`, i.e. older than 1.0.4. The constant is an immediate in CheckMod. The running game
  is `0x010007ff` (`Configuration::CurrentVersion` 0x1800c4f40 -> "1.0.7").
- +0x18 = `Mod+0x1c` = `PackageFile::GetUserVersion` (0x1800253e0), the mod author's version. Status 3
  is a mismatch.

**The path CheckMod compares** (0x1800c1090, `saveload.cpp:688-760`), from the bytes:
```
1800c10c1  shl $0x5,%rbp                 ; modIndex*0x20
1800c10d6  call 0x180007bd0              ; GetDataDirectory
1800c10dc  lea 0x8(%rbp),%r8             ; ModInfo+0x08  (the PATH, not the name)
1800c10e8  call 0x1800c9440              ; operator+
1800c110b  call 0x1800c6970              ; AdjustDelimiters
1800c114a  call 0x18007eefc              ; _stricmp vs Mod+0x58 (after a length compare)
```
The order of checks is: not found -> 1, before any dword is read (`saveload.cpp:751`). So for a
pseudo-mod the three dwords never affect the status code. They only change what the modal displays (§3).

**`ext:<id>` can never be matched:**
- `AdjustDelimiters` (0x1800c6970, `src/engine/system/filename.cpp:203`) changes only `/` -> `\` and
  collapses `\..`. A colon passes through unchanged. The compared string is
  `<dataDir>ext:<id>`, case-insensitively.
- `Mod+0x58` is `packDir + FindFirstFileW name` for the pattern `*.pkm`
  (`modification_refresh.cpp:575-600`, `filesystem_find.cpp` FindFilesImpl `*dir + utf8`).
  - **Windows:** a colon appears in such a path only as the drive letter at index 1. `:` is illegal in
    an NTFS file name, and FindFirstFileW never returns stream names. A colon after the dataDir
    prefix is impossible, so there is **no match**.
  - **Linux (our port):** `:` is a legal file-name character. A match needs a file named exactly
    `ext:<id>` that FindFiles also returns for `*.pkm`. **Rule: `<id>` must be `[a-z0-9_-]+` with no
    `.`.** It then never ends in `.pkm`, and no match is possible.

## 3. The modal path -- VERIFIED: modsMissing lets the load proceed in retail

Retail `SaveLoadDialog::DoLoad` 0x1400b61e0 (Ghidra; ported at `src/game/Dialog/saveloaddialog.cpp:849`):
- `confirmLoad` first, only when a game is running.
- For each `i < SaveInfo(sel).mods._size` it calls `st = CheckMod(sel, i)`. When st != 0 it loads
  `Dialog/SaveLoad.rsc:` `modsOutOfDate` (2) / `modsVersion` (3) / `modsChanged` (4) /
  **`modsMissing`** (any other value, i.e. 1), builds a MessageDialog (vtable 0x14015c480), and sets
  TextElement parameters @0 = ModInfo.name, @1 = ModInfo.path, and
  @2 = `Format("%i (%s)", ModInfo+0x18, GetVersionString(ModInfo+0x14))`. It then calls `ShowDialog`.
  **`if (result != 2) goto end` (abort); otherwise the next mod.** After the loop it sets `+0xba = 1`
  and runs `SetOperation(2, sel)`, which starts the load.
- The resource (`ToolKit/resource/Dialog/SaveLoad.rsc:394`): `Dialog modsMissing : "MessageBox.rsc"`
  hides `buttonOk` and `buttonCancel`, which leaves **buttonYes / buttonNo**. Its text is
  `StringTable.rsc:mainMenu` key **`ModMissing`**: "The following mod can not be found or cannot be
  loaded. @0 [@1] Saved with version @2 ... Do you still want to load the save game?"
- The button ids come from MessageDialog::OnOpened 0x14006d140: Cancel=0, Ok=1, **Yes=2**, No=3.
  OnElementEvent 0x14006d070 dispatches id 2 to vtable +0x60 = 0x14006cfb0, which stores
  `movl $0x2,0x24(%rcx)`. (Vtable 0x14015c480 slot 12 read from memory = 0x14006cfb0.)

**So in retail, a save with `ext:` entries loads after the player answers Yes once per entry.** The
data in block 1's tail is never read (§1). The player sees `@1` = `ext:<id>` and `@2` =
`"<+0x18> (<GetVersionString(+0x14)>)"`. **Field recommendation** (this replaces DESIGN §2's
"versionMajor = format version"): +0x18 = the extension's format version (the number that gets
displayed), +0x14 = `0x010007ff` (displays "1.0.7"; anything >= 0x1000400 is fine), +0x10 = 0 (the
CRC slot, never compared for a missing mod). One modal per extension is retail's cost. N enabled B/C
extensions mean N Yes clicks in retail.

## 4. Our loader -- the seams

**Write, pseudo-mod entries:** `Save::SaveLoad::SaveMetaData`, `src/engine/save/saveload_meta.cpp`,
right after the enabled-mod loop (`:439`) and before `int32_t count = arr.size;` (`:449`): append one
`ModInfo` for each enabled B/C extension through the same `ModInfoArraySetSize(&arr, arr.size+1,
&tmp)`. That is the only writer of `Array<ModInfo>`. The body is in `src/engine/`, and the extension
registry should not be included from there. The smallest seam is a hook such as
`std::span<const ModInfo> (*)()` that the game side registers; it stays null under the faithful profile.

**Write, class-C chunk:** `GameUpdateLoop::SaveRestore` save arm,
`src/game/Game/gameupdateloop_serialize.cpp:723-724` (between `SerializeAll` and `Close`).

**Recognise on load / lock the set:**
- *Before the commit (notice):* `SaveLoadDialog::DoLoad`, `saveloaddialog.cpp:885`. Split out the
  entries whose `path` starts with `ext:` before the CheckMod loop, show our own notice for those
  (§5), and run CheckMod only on the real entries. Our dialog stays retail-identical for real mods.
- *At the commit (lock):* the authoritative place is the SaveRestore **load** arm
  (`gameupdateloop_serialize.cpp:719-720`): read the tail chunk when `GetCurrentPos() < GetSize()`.
  Reason: `UpdateLoad` (op 2, `saveload_update.cpp:686`) sees the save's mod list, but
  `UpdateLoadFromFile` (op 3: scenarios `scenariodialog.cpp:329`, the tutorial
  `tutorial_next_scenario.cpp:214`, the autoload harness) never loads block 0's mod list. A lock that
  reads only the pseudo-mods would miss those routes. **Recommendation:** every enabled B/C
  extension, class B included, writes a small manifest record (id + format version) into the
  block-1 tail. The tail is invisible to retail, so class B stays format-neutral for retail. Our loader
  locks the set from that manifest. The pseudo-mod list is only the retail-facing notice.
- `UpdateLoad` state 1 (`:686`) re-enables the installed mods that match the save's paths. An `ext:`
  entry matches nothing, so nothing happens there. No change is needed.

**Resave gate:** `tools/resave_gate/resave_gate.sh` (default `GATE_MODE=roundtrip`) does
`cmp -s golden standalone_roundtrip_resaved.bin` (`:76`), a **whole-image byte compare** of the
DECOMPRESSED block 1. `RoundtripDecompress` reads `GetSize()` bytes (`gameupdateloop_serialize.cpp:749`).
A tail chunk would make that DIFF. Block 0 (the pseudo-mods) is not part of the compare.
`GATE_MODE=file` compares the whole `.sav`, but it is an expected-DIFF smoke test. **The faithful
profile writes neither:** at `d5bc23b57` no extension code exists (grep for `ext:` / extension in
`src/engine/save`, `gameupdateloop_serialize.cpp`: nothing), so this holds today by absence. The
implementation must gate both write seams on "enabled B/C set is empty", and must write zero bytes
(no empty chunk header) in that case.

## 5. The notices the player already knows -- VERIFIED

- **Loading a save made with a different mod set:** `SaveLoadDialog::DoLoad` 0x1400b61e0 (§3),
  `Dialog/SaveLoad.rsc:modsMissing` / `modsOutOfDate` / `modsVersion` / `modsChanged`, string keys
  `ModMissing` / `ModOutOfDate` / `ModVersion` / `ModChanged` in `StringTable.rsc:mainMenu`
  (lines 183-186). Our port is `saveloaddialog.cpp:849`, and the modal goes through
  `SaveLoad_ModalShowDialog` (unattended answer = 2).
- **Toggling mods during a game:** `ModDialog::Apply` 0x14006ea70 (Ghidra; `src/game/Dialog/moddialog.cpp`
  `Apply`). It runs when `Modification::_dirty` is set, loads `Dialog/Reload.rsc:game` (text key
  `ReloadGame`: "Changes to the loaded mods require the game to be reloaded ... may have undesirable
  effects ... Do you want to apply changes and reload now?") or `:menu` (`ReloadMenu`), and on
  result 1 or 2 calls `SaveList` + reload.

For the Extensions dialog: reuse **`Reload.rsc:game`** (the same MessageDialog + Yes/No) when the
player toggles a B/C extension during a game. On load, show a notice built like `modsMissing`
(MessageBox.rsc with Yes/No, @0 name, @1 `ext:<id>`, @2 version) for each `ext:` entry whose extension
is missing or disabled. Its wording needs a new string key, because `ModMissing` says "mod". Without a
new key it can reuse `modsMissing` verbatim.

## Not verified / open
- No game run: the Yes-continues path in retail is proved from the bytes plus the shipped ToolKit
  resource, not by clicking it in retail.
- Whether `*.pkm` on Windows can match an 8.3 short name (`foo.pkmx` -> `FOO~1.PKM`) does not matter
  here: the colon argument holds whatever FindFiles returns.
