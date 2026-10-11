# Phase D.4 wave 4 block GAPS: rows 1273 and 1275

Branch `d4-gaps`, base `a125c96bb`. Main had not moved past the base, so no merge was needed.
The block has two `behaviour:` commits plus this hand-in and the two row edits.

Every commit was gated statically only. **The game was never run.** The coordinator's routes are in
section 3.

## 1. Commits

| commit | title | unit |
|---|---|---|
| `9ba9fede4` | behaviour: GameMain's Startup failure shows the fatal box, shuts down, returns -1 (row 1273) | 990 / 0 |
| `2b69c19e5` | behaviour: MainMenuDialog OnOpened / OnClosed are retail's full bodies (row 1275) | 990 / 0 |

Each commit was gated on its own tree, with these results:
- `standalone-debug`, `standalone-perf` and `unit` at `-j2` all exited 0.
- There were 0 new warnings against the main checkout's `runs/warn_baseline_{debug,perf}.txt`, using gate.sh's `norm()`.
- castmetric is 2 and `abi_audit` is OK.
- `acceptance.sh` reports 0 lines off target. R14 to R16 were skipped because they need a step-close log.

### 1273: `Game::RunGameMain`'s Startup-failure arm

This is retail GameMain at 0x140041ef0, decompiled and disassembled in Ghidra:

```
140041f4f CALL [0x140152c08]   ; Engine::Engine::Startup(&engine, cmdLine, 0x200, 0, false)
140041f55 TEST EAX,EAX
140041f57 JNZ  0x140041f72
140041f59 CALL [0x140152b58]   ; System::Error::Display()
140041f5f LEA  RCX,[RSP+0x60]
140041f64 CALL [0x140152c18]   ; Engine::Engine::Shutdown(&engine)
140041f6a OR   EDI,0xffffffff  ; return -1
140041f6d JMP  0x1400422e4     ; ~Engine, return
```

- **Before.** The arm logged `_errorString`, showed no box, ran no shutdown and returned 3.
- **Now.** After the same log, the arm calls `System::Error::Display()`, which is the SDL box since SDL W6.
  It then calls `ShutdownEngineStaged(eng)` and returns -1.
- **Why the staged shutdown.** It is the tree's stand-in for Engine::Shutdown, and the success arm already
  uses it. The ported `Engine::Engine::Shutdown` (engine_lifecycle.cpp) has never been called.
  - Every step of the staged shutdown tests its subsystem pointer, so a half-built engine is safe.
  - Its step 1 stores VideoSettings = true, so the launcher dialog comes back next boot, as in retail.
- **B11 for `game_main.o`.**
  - DIFF 1: `RunGameMain`. The `$0x3f` compares and the store of 3 are gone; the Display and shutdown calls are new.
  - IDENTICAL 8.
  - ONLY-TIP 1: `ShutdownEngineStaged` is now emitted out of line because it has two callers.

The harness consequences, from reading the consumers:

| what | result |
|---|---|
| The box on an unattended boot | It blocks until clicked. The watchdog sees a `#32770` window and publishes BLOCKED_DIALOG, but does not terminate on it. |
| `tools/boot_state.sh --wait` | It exits 2 (BLOCKED_DIALOG). |
| `BANISHED_MODAL_ANSWER` | It does NOT answer this box. It bypasses `UI::ShowDialog` only. The brief's assumption did not hold, and no new instrument was added (DIRECTION section 2). |
| The process exit code | 0xFFFFFFFF, which bash reads as 255. `main.cpp` returns RunGameMain's value. |
| `gate.sh` | Any non-zero exit is "not clean", and a live route is killed at its timeout. Nothing misreads 255. |
| `_stopApplication` | `FileSystem::GetSaveDirectory` arms it with true, so that failure exits -1 from inside Display, before the shutdown. Retail's CRT `exit(-1)` does the same. The Video::Device sites pass false. |

### 1275: `MainMenuDialog::OnOpened` and `OnClosed`

Retail 0x1400b3cd0 (slot 4) and 0x1400b3c60 (slot 5) are quoted in the banners above the two members
and in the commit message.

- **The change.** The members now ARE the full bodies. These are deleted:
  - the free `MainMenuDialog_Slot4` / `_Slot5` copies;
  - their declarations in `ui_dialogs.h`;
  - the `CreateBuildIdChildDialog` helper;
  - the now-unused static `FindElement` helper.
- **Divergences found and fixed.**
  1. `textBuildNotice` is kept only if it IsDerivedFrom `?_typeInfo@TextElement@UI@@`. Import slot 0x140151508
     was read from the IAT name; Ghidra calls it `fn_LabelElement_SetSprite`. Both old copies skipped this check.
  2. The BuildId child is built unguarded, as in retail. The old helper skipped it on a null resource or on null
     singletons. The one guard kept is `_ui == nullptr`. Only `MainMenuDialog_FromMenuDLT`'s bare-bones fallback,
     a standalone-only path, can produce it, and the comment says so.
  3. Resume's `_keyIndex = 8` (the SDL-C Esc fix) stays in retail's place.
- **What already matched.** The id order, the three `Scene::Pause` calls through the singleton's +0x78, the
  scenario index at +0x15c and flag bit 10 at game+0xd0.
- **Callees ported.** None were needed. Every callee is an existing ported body: `Audio::Scene::Pause`
  (0x1800df070), `ButtonElement::Enable` (0x180110cc0, virtual slot 8), `Element::SetFlagsOnOff`, the
  DialogInstance ctor, `AddDialog` and `TextElement::SetParameter`.
- **Consumers read.**
  - `toolbardialog.cpp` builds the pause menu with `game` set. The front-end menu has a null `game`.
  - OnClosed is reached only through `UserInterface::CloseDialog`. That is the Resume button or Esc.
  - Quit and "Menu" do not close the dialog, and no UI teardown calls OnClosed, so OnClosed never runs after
    `GameUpdateLoop::_singleton` is cleared. This is the same as retail.
- **B11.**
  - `mainmenudialog.o`: DIFF 2 (OnOpened and OnClosed), IDENTICAL 33, ONLY-BASE 4 (the deleted functions).
    SAME 1 (`ReturnToMenu`) and RENAMED-SAME 1 (the static initialiser) have the same calls, stores and
    compares; only position-dependent bytes moved.
  - The other 12 TUs that include the two headers are all IDENTICAL or RENAMED-IDENTICAL.

## 2. Rows

- **1273** is FIXED (`9ba9fede4`). The row records the consequences above.
- **1275** is FIXED (`2b69c19e5`).
- No new rows were added.

## 3. Routes the coordinator owes

1. **The pause menu, free game.**
   - Esc opens it. Music, ambience and effects pause, and stderr shows `[MainMenu] OnOpened ... game=<non-null>`.
   - The Goal button is hidden.
   - Esc or Resume closes it, and the sound resumes.
2. **A scenario game.** The Map button is hidden instead of Goal.
3. **The front end.** The BuildId text still shows; look for the `[BuildId] rsc=... textBuildNotice=...` line.
4. **The gate's newgame, tutorial and loadgame routes.** The pause-menu quit still exits clean.
5. **The Startup failure, by hand.**
   - Force a Video::Device::PlatformStartup failure, for example a renderer in settings.cfg that cannot start.
     Restore it afterwards.
   - Check that the SDL box shows the "Video::Device..." text and that `boot_state.sh` reports BLOCKED_DIALOG.
   - After the click, check for the `[Shutdown] partial` lines, exit 255, and VideoSettings = true.

## 4. Open items

- **Stale exit-3 text.** These still say "GameMain returns 3 when Engine::Startup fails", and neither file is
  this block's:
  - the `src/diag/win/boot_state_watchdog.cpp` banner comment;
  - the `triage-run` skill's exit-code table.
- **An unattended failed boot now waits on the box.** The watchdog reports it but never kills it. Two options
  are the coordinator's call:
  - retail's own switch, `System::Error::_hideDisplay`, set on unattended boots in `main.cpp`;
  - leaving it as BLOCKED_DIALOG.
- **The real `Engine::Engine::Shutdown` has never been called.** Both GameMain arms run the staged stand-in.
