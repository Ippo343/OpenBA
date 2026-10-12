# EXTNOTICE -- the load notice says "extension" (hand-in)

The user's ask: "Can we make the load notice say 'extension' if its an extension issue?" Before this
block, a save carrying an `ext:<id>` pseudo-mod that this build could not load as it stands got
retail's `Dialog/SaveLoad.rsc:modsMissing` verbatim ("The following mod can not be found or cannot be
loaded", EXTSAVE.md section 2 and its wording doubt). Now each such entry gets one of three notices
of our own. Each says extension and says what Yes will do.

| commit | what |
|---|---|
| 1 | `src/ext/ui/Dialog/Extensions.rsc` + `ExtensionsStringTable.rsc`: three notice dialogs, three strings |
| 2 | `src/ext/load_notice.{h,cpp}` (the reason), `SaveLoadDialog::DoLoad` uses it, `tests/unit/ext_load_notice_test.cpp` (3 tests; unit 1055 / 0) |
| 3 | this hand-in |

## 1. The dialogs and the strings

Each dialog copies `modsMissing` (kit `resource/Dialog/SaveLoad.rsc:394`): `Dialog x : "MessageBox.rsc"`,
root `MessageBox.rsc:main`, the `text` element overridden, and `buttonOk` / `buttonCancel` hidden, so
Yes / No remain. The text element inherits `MessageBox.rsc:text` (word wrap 400, FontMedium) with our
string table. `Extensions.rsc:327-381`:

| dialog | text key | when |
|---|---|---|
| `extensionNotInBuild` | `ExtensionNotInBuild` | the id is unknown to this build, or it names a fix / a class A extension |
| `extensionNewer` | `ExtensionNewer` | this build has it, but a newer format version of it wrote the save |
| `extensionTurnedOff` | `ExtensionTurnedOff` | this build has it, the version is readable, and the player has not turned it on |

The strings are in `ExtensionsStringTable.rsc:37-41`. They follow ModMissing's layout: `@0 [@1]`, then
"Saved with extension version @2", then the consequence, then "Do you still want to load the save game?".
- not in build: "...cannot be played here: loading it will stop the game with an error."
- newer: "This version of the game cannot read it: loading this save will stop the game with an error."
- turned off: "A save keeps the extensions it was made with, so it will be turned on for this game."

The first two are true because the block-1 manifest fails the load through the fatal box on an
unknown or newer id (`gameupdateloop_serialize.cpp:723-738`). The third is true because the manifest
locks the save's B / C set on (EXTSAVE section 2).

**No script change.** `tools/ext_resources.sh` builds one /build per top-level object, and a
named object is built only when something references it. So the dialog's `resource` in
`Extensions.rsc:27-35` now has an `External _references` list naming the three notices. That is how
retail's `SaveLoad.rsc` resource references `modsMissing` and the others. The existing
`/build Dialog/Extensions.rsc` therefore compiles them to `Dialog/Extensions_extension*.crs`, and the
deploy glob `Dialog/Extensions*.crs` already copies them loose.

## 2. DoLoad (`src/game/Dialog/saveloaddialog.cpp`)

- `:86` includes `ext/load_notice.h`.
- `:917-935` handles an `ext:` entry whose `PseudoModLoadStatus` is 1: `Ext::PseudoModNoticeFor(path,
  formatVersion)` picks the reason. If that ever returned None, it falls back to NotInBuild, so a
  status-1 entry is never skipped. The stdout line now names the dialog.
- `:967-969` sets `warnPath`: for an `ext:` entry it is the notice's dialog instead of
  `SaveLoad.rsc:modsMissing`.
- `:1015-1017` sets @2 for an `ext:` entry to the format version alone (`"%i"`). Its +0x14 is the
  pseudo-mod constant 0x010007ff, not a game version the player saved with, so `"1 (1.0.7)"` meant
  nothing. @0 (the saved display name) and @1 (`ext:<id>`) are unchanged.
- Everything else is retail's path. The modal is `SaveLoad_ModalShowDialog`, and any result other than 2
  aborts. Real mods take the same order, the same CheckMod and the same retail dialogs.

`Ext::PseudoModNoticeFor` is in `src/ext/load_notice.cpp`, because `registry.*` belongs to PATHWORKERS.
It applies `PseudoModLoadStatus`'s checks in the same order. The test `ext_load_notice_matches_load_status`
checks that None holds exactly where that function returns 0, over every on/off combination of
`unit_b` / `unit_c`, five paths and versions 0-4.

## 3. What the coordinator must do

1. Merge, then run `tools/build.sh deploy`, which runs `tools/ext_resources.sh`. Check that its log
   lists `Dialog/Extensions_extensionNotInBuild.crs`, `_extensionNewer.crs` and `_extensionTurnedOff.crs`
   among the deployed files, with 0 problem lines for `/build Dialog/Extensions.rsc`.
2. Turn on `ext.fast_paths`, start a game and save it. Turn `ext.fast_paths` off in Extensions, then
   Load that save from the menu. Expect the "turned off" notice naming Faster pathfinding `[ext:fast_paths]`,
   version 1. Answer No and the load is aborted. Answer Yes and it loads, with `[ExtSave] manifest: ... locked` in standalone_error.log
   and the row shown as locked. standalone_debug.log shows `[ExtSave] DoLoad: ... -> Dialog/Extensions.rsc:extensionTurnedOff`.
3. Optional: the not-in-build notice needs a save whose `ext:` id this build lacks, for example one
   whose `fast_paths` id was hand-edited. On Yes, that load ends at the fatal box, as before this block.

## 4. Doubts

- **Unexercised.** The `.rsc` was not compiled here, because only the coordinator runs the kit. If the
  compiler does not follow `External _references` from a `Dialog resource` the way retail's
  `SaveLoad.rsc` relies on, the three `.crs` files will be missing. The fallback is three TARGETS lines,
  `"Dialog/Extensions.rsc:extensionNotInBuild"` and the other two, in `tools/ext_resources.sh`.
- **The references load with the Extensions dialog.** Opening Extensions now also loads the three small
  notice dialogs, through FindOrLoad of the references. They are harmless, but they are loaded.
- **Yes on a fatal notice.** For "not in build" and "newer", Yes still leads to the fatal box. The text
  says so. Offering only No (or Ok = abort) would be kinder, but it changes the Yes/No contract DoLoad
  keeps with retail, and the brief said to keep it. It is the user's call.
- **"Turn it on in Extensions first."** The brief suggested telling the player this. The notice
  doesn't, because loading turns the extension on for that game anyway (the manifest lock), so turning
  it on first changes only what new games get.
- **Retail Banished** still shows its own `modsMissing` for these saves. It never sees our dialogs.
