# ROWS hand-in (PORT GAP hunt wave 2, branch `d4-rows`, base `4042c65b3`, main merged at `46219ba36`)

The open rows of `docs/known_port_issues.md` that GAPHUNT_RULES.md section 2 gives block ROWS. Application-x64.exe
read in Ghidra (the current program, never switched). Runtime-x64.dll read with `objdump` on the PE64. Import slots
named with pefile on the exe's import table.

**Static gate only.** The game was never run from this worktree. Every route below is owed.

## Gate (after `git merge main`, all three targets rebuilt)

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 (nothing added) |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new on every build of the block (debug full + 2 incremental, perf full + 2 incremental) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

## Commits

| commit | title (short) | B11 (perf objects vs `b11base_d4e`) |
|---|---|---|
| `7ee24316f` | behaviour: GameLoadDialogLoopT::Startup's tail = 0x140098fa0 (row 1243) | splash_loop_stubs DIFF 1 (Startup: UIFade x2 -> ApplyKeyMapToUI, SetFullscreenFade x2, SetCursor, EnableCursor; new store +0x13c) IDENTICAL 14 SAME 2 |
| `c06a572ee` | behaviour: RestoreVideoSettings = 0x1400727d0, three tests dropped (row 1244) | optionsdialog DIFF 1 (OnCancel, RestoreVideoSettings inlined into it) IDENTICAL 39 RENAMED-SAME 1 SAME 1 |
| `84e0bd10e` | behaviour: the start town's three agriculture pool loops = 0x1400a3300, four tests dropped (row 1245) | starttown DIFF 1 (StartTown_SpawnEntities) IDENTICAL 43 ONLY-BASE 1 (SeedAgriPool.isra, now inlined) ONLY-TIP 1 (SeedAvailableAgriculture, now out of line) RENAMED-SAME 1 SAME 13 |
| `4459f3197` | behaviour: ResidenceComponent::EnterBuilt = 0x140129de0 whole (row 1260) | residencecomponent DIFF 2 (EnterBuilt + its state thunk: TownRegistry_RegisterResidence + fprintf -> rtcoll::SetSizeCore) IDENTICAL 38 RENAMED-SAME 1 SAME 5; citizen_employ_cascade ONLY-BASE 2 (the free function, its fprintf) and DIFF 8 (only the inlined `s_init` compare's .bss displacement moving) |
| `d39a7f3f5` | behaviour: CountProducedMaterial = 0x140088ef0, `source &&` dropped (row 1140) | action_base_rawmat DIFF 1 (one compare fewer) IDENTICAL 17 |
| `512c89552` | behaviour: AddWorker = 0x140091130, the profession test dropped (row 1141) | citizen_employ_cascade: see below |
| `d000aec8a` | behaviour: RemoveWorker = 0x140091180, the two tests dropped (row 1142) | townhall_helpers IDENTICAL 17 SAME 3 |
| `b0fa840da` | behaviour: Audio::Scene::Update's two table walks = Runtime 0x1800df730 / 0x1800dfc7a (row 1220) | audio_scene_update IDENTICAL 10 RENAMED-IDENTICAL 1 SAME 1 |
| `79658a205` | docs: the status cells, rows 1300 / 1301, the 792 and 1282 comments | entitylist IDENTICAL 33 RENAMED-IDENTICAL 1; audio_sdl IDENTICAL 19 |
| `46219ba36` | merge main (clean) | |
| (this commit) | docs: this hand-in | |

SAME in b11pair means the code changed but calls, stores and compares did not; a dropped `TEST reg,reg; JZ` looks
like that. Two corrections to the commit messages:

- `7ee24316f` says the splash scale is "1.2 at 1080p". It is 900 / 1080 = **0.83**. The scale is 900 / height,
  so it is above 1.0 only below 900 pixels of back-buffer height.
- `512c89552` says AddWorker "loses its profession compare". b11pair does not count a `TEST reg,reg`, so the drop
  shows as nothing in its compare column. citizen_employ_cascade's eight DIFF lines all come from `4459f3197`'s
  .bss shift.

## Rows

| row | verdict | what was done |
|---|---|---|
| 1243 | FIXED (`7ee24316f`, look owed) | Retail's tail after the chained `DialogLoaderLoopT::Startup`, read whole (29 instructions, imports by pefile): `ApplyKeyMapToUI(&_ui)`; `SetFullscreenFade(true, _fadeColor1, true)` then `(false, _fadeColor1, false)`; `SetCursor(container->cursorHourglass)` (+0x38); `EnableCursor(true)`; `_ui._uiScale = (float)(int)(900.0f / height * 100.0f) / 100.0f` (0x140172244 = 900.0f, 0x140172208 = 100.0f, height = `Video::Device` +0x08). The port's `UIFade::EnsureFadeSprite` + `TriggerReveal` in Startup are gone (same two fades, constant black). The unguarded reads hold: GameContainer_Init fills the container in InnerLoaderLoop::Load before the first splash, and the settings block exists before `MainLoop::Run`. |
| 1244 | FIXED (`c06a572ee`, look owed) | Three tests dropped. The CheckPtr at 0x140072800 is a bare RET. ApplyKeyMapToUI's caller comment corrected to retail's four code xrefs. |
| 1245 | FIXED (`84e0bd10e`, look owed) | SeedAgriPool's `!pool \|\| size <= 0 \|\| count <= 0` return, the `rng ?` fallback and the `!tmpl` skip dropped; ResolveTemplate's `!ExternalFactory::_singleton` return dropped (dead: ResolveDescription has already read it). The loops test only the grant count on entry, re-read each pass, as retail. Outside the row, new row 1301. |
| 1260 | FIXED (`4459f3197`, look owed) | EnterBuilt is 0x140129de0 whole. The free function, its exe_bodies.h line, its three tests, its count check and both logs are deleted. |
| 1140 | FIXED (`d39a7f3f5`, look owed) | `source &&` dropped (0x140088f04 reads the slot unguarded). |
| 1141 | FIXED (`512c89552`) | The profession test dropped. Retail has no code caller of 0x140091130; the tree's only caller is the builder-seat harness. The `g_wpcRosterOnly` hold stays: it is the tree-wide seat-path deviation, not this row. |
| 1142 | FIXED (`d000aec8a`, look owed) | Both tests dropped; the laborer is read in one chain. |
| 1220 | FIXED (`b0fa840da`, listen owed) | Both `if (table)` wrappers removed (objdump: 0x1800df730, 0x1800dfc7a, no test). Scene's ctor builds the table. |
| 880 | FIXED earlier (B9 RT2 `21baddeb4`) | The tree already has the typed empty `Array` local with `ArrayAssignCore` inline, and the site's comment matches the -O2 code. Status cell rewritten to lead with the commit. |
| 1163 | FIXED earlier (B9 RT `c2edd2e5d`) | Both calls are `RawArraySetSize`; `RangeResize` is gone. Status cell only. |
| 796 | FIXED earlier (B4 F1 `1c1fb1327`) | Both removes pass the real header. Status cell only. |
| 820 | FIXED earlier (`d068b9ad1`) | `ser.Serialize(_entityFlags, nullptr)`; import 0x140152a50 is `?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z` (unsigned int&), which dispatches SerializeUInt. Status cell only. |
| 810 | FIXED earlier (B6 SAVE `565addbb3`) | The version < 2 arm reads into a local header and frees it; +0x160 untouched. Status cell only. |
| 850 | FIXED earlier (B6 COMP `ec396cd34`) | Unconditional InteractNodePosition into a `Vector3Slot`. Status cell only. |
| 860 | FIXED earlier (B8 RT2 `1df118df7`, row 1160) | +0x00 goes through `SerializeExternal<Graphics::AnimationGroup>`; import 0x140152980 is `?_typeInfo@AnimationGroup@Graphics@@`. Status cell only. |
| 1162 | RULED | Status cell is exactly `RULED (user, 2026-10-10: keep deterministic 0.0f)`. |
| 792 | LEFT | The AuditDyingEntity net exists because the tree drains removals one frame late. Dropping it alone brings back the measured `Graphics::Scene::Clean` fault. It goes with the deferral, which goes when EntityUI holds retail's strong data-source reference and the drain returns to the end of the frame (0x1400c4090's tail). EntityUI is block DRAW's file. The site's comment and the row say so. |
| 1282 | RECORDED (not a defect in our teardown) | Read against SDL 3.4.18's vendored source. The retail quit path (MainMenu Quit, MainLoop::Run returns, Engine::Shutdown, Audio::Device::PlatformShutdown) closes the one logical device. SDL then closes the physical device (`pa_stream_disconnect` + unref, no drain). `SDL_QuitSubSystem(SDL_INIT_AUDIO)` holds the last reference (main inits VIDEO only), so the PulseAudio context disconnects too, all before `SDL_Quit` and the return from main. Nothing of ours stays on the server at a normal exit. The `_exit` paths (crash, hang, fatal error) leave only the socket close. The WSL2 re-test stays the coordinator's. The reading is in audio_sdl.cpp's banner. |
| 1277, 1278, 1280, 1281, 1283, 1284 | RECORDED (not a defect) | Each cell gives its reason. None is a defect that can be fixed statically: an SDL window show state, no portable physical-key query, a faithful heap-order save, three instruments of ours, and a packaging necessity. |

No row of the block's list is left `OPEN`. Rows 861, 890, 1291-1293, 1295 were not touched (not this block's).

## Rows added

- **1300** (OPEN): `OptionsDialog::ApplyGameSettings` calls `ApplyKeyMapToUI`. Retail's 0x140072420 does not (no call
  to 0x140099260 in its body; the four code xrefs are listed in the row). On a healthy run the values written are
  the ones the UI already holds.
- **1301** (OPEN): the start town's pool entries are resolved in place, where retail resolves a stack copy and never
  writes the pool. `StartTown_SeedAvailableAgriculture` keeps its loop / town manager / map tests and three
  `[AgriSeed]` prints.

## Save format and world build

- **Resave golden: should NOT move.** Rows 820, 810 and 860 touch serialization but were fixed before this block;
  nothing in their serializers changed here.
- **1260:** the residence list gets the same entity pointer appended at the same moment. The town manager's +0x20
  list is the only state, and it is built the same way.
- **1245:** the same `Rand` calls in the same order on a scenario whose pools are non-empty, so the same picks. The
  new-game world, the herd and the agriculture list should be identical (`worldcmp.sh`: no difference expected). A
  scenario with an EMPTY pool and a non-zero count would now call `Rand(0)` as retail does, instead of skipping;
  none of the shipped start conditions is like that (Easy.rsc pool sizes 7 / 8 / 8 / 3).
- **1243, 1244, 1140-1142, 1220:** UI, audio or dropped tests that never fire on a healthy run. No saved state.
- **Log lines gone:** `[TownRegistry] RegisterResidence ...`, `[Residence] state-2 enter FIRED ...`, and the
  splash Startup's `[UIFade::EnsureFadeSprite]` / `[UIFade::TriggerReveal]` lines. No tool in `tools/`, `perf/` or
  the phase_d scripts greps them.

## Routes for the coordinator (merged exe)

1. **Splash (row 1243).** `sa-run-menu`. Watch the ShiningRock and Loading splashes. The cursor over them is the
   hourglass sprite (before: the default arrow or none). Hovering or clicking a splash button, if any, makes the UI
   sound. The splash text and logo are drawn at the height-based scale, 0.83 at 1080p (before: the settings scale,
   1.0 by default), so the splash layout changes size at 1080p (which way was not checked statically). The fade-in
   from black still plays.
2. **Options Cancel (row 1244).** In the main menu or in game, open Options, change a video and a game setting,
   press Cancel, reopen Options. Expect the old values and the last tab as before.
3. **New game (rows 1245, 1260).** Start a new game. Open the town hall's agriculture tab: the starting crops,
   trees and livestock are bright as before. The starting herd is in its pasture. The town hall's house count
   equals the starting houses. Build one more house and watch the count rise when it finishes.
4. **Production (row 1140).** Woodcutter or gatherer dropping piles: stock counts rise as before.
5. **Profession spinner (row 1142).** Town hall, professions: press minus on a profession with workers. A worker
   becomes a laborer as before. Plus (row 1141) is reached only through the builder-seat harness.
6. **Audio (row 1220).** Any game with sounds playing for a few minutes (fire, crowd, chopping). Sounds play and
   stop as before; no sound sticks.
7. **Linux exit (row 1282).** The WSL2 re-test: a long new-game route, then a menu boot at once. If it still stalls
   in `pa_threaded_mainloop_wait`, the cause is server-side, since our teardown disconnects cleanly.

## Needs in other blocks' files

- **DRAW (`Dialog/entityui*.cpp`):** row 792's net and the one-frame drain deferral (entitylist.cpp) both go only
  after EntityUI holds retail's strong data-source reference. Then `EntityList::Update`'s Step 0 moves back to
  0x1400c4090's tail, `AuditDyingEntity` and the two dying-entity helpers go, in one `behaviour:` commit.
- **Seat paths (`workplacecomponent.cpp` and the seven files that hold `g_wpcRosterOnly`):** the flag is a tree-wide
  deviation; AddWorker keeps it until WorkplaceComponent's Attach / Detach run on real scene objects.
- **`splash_loop_stubs.cpp`, Update (not in row 1243):** `GameLoadDialogLoopT::Update` still calls
  `UIFade::EnsureFadeSprite` + `TriggerFadeIn` and logs `UIFade::LogState` from Draw. Retail's 0x140098da0 should be
  read against them.
- **`goals_serialize.cpp`:** WorkScheduler::Serialize's stage 3 still has `if (GameUpdateLoop::_singleton !=
  nullptr)` (outside row 796).

Shared files appended or edited: `docs/known_port_issues.md` (status cells of 24 rows, rows 1300 / 1301 appended
after 1295); `src/game/Component/exe_bodies.h` (one declaration deleted, the folded free function).
