# Exe close-out — lane C (citizens + notifications)

Targets: `docs/runtime_port/PLAN.md` §1.2 item **2** (CitizenAI Work /
AdultTransition / ElderTransition "unported") and item **6** (achievement award
toast unported).

---

## Item 2 — CitizenAI Work / AdultTransition / ElderTransition — **CLOSED (already ported; the claim was a stale comment)**

PLAN.md §1.2 row 2 cites `code/Game/Component/citizenai_handlers2.cpp:16`. That line is a
**comment**, and it had gone stale:

```
// HANDLERS DELIBERATELY NOT PORTED HERE (blocked on UNregistered deep helpers — see report):
//   0x14010d790  State_Work           : CitizenAI_helper_db20 (0x14010db20) UNREG
//   0x14010cd80  State_AdultTransition: CitizenAI_helper_e370 (0x14008e370) + FUN_14012ac30 UNREG
//   0x14010d2c0  State_ElderTransition: Entity_impl_ec70 (0x14008ec70) + FUN_14012ac30 UNREG
```

All three were ported in the very next wave and the "UNREG" helpers were registered:

| retail addr | our port | registration |
|---|---|---|
| `0x14010d790` State_Work | `code/Game/Component/citizenai_handlers3.cpp:391` | `standalone/reg_citizenai_handlers3.cpp:24` |
| `0x14010cd80` State_AdultTransition | `citizenai_handlers3.cpp:246` | `reg_citizenai_handlers3.cpp:25` |
| `0x14010d2c0` State_ElderTransition | `citizenai_handlers3.cpp:174` | `reg_citizenai_handlers3.cpp:26` |
| `0x14010db20` helper_db20 | `citizenai_findhelpers.cpp` | `reg_citizenai_findhelpers.cpp` |
| `0x14008e370` helper_e370 | `citizenai_findhelpers.cpp` | `reg_citizenai_findhelpers.cpp` |
| `0x14008ec70` Entity_impl_ec70 | `citizenai_findhelpers.cpp` | `reg_citizenai_findhelpers.cpp` |
| `0x14012ac30` ResidenceCheck_2ac30 | `citizenai_findhelpers.cpp` | `reg_citizenai_findhelpers.cpp` |

`RegisterCitizenAIHandlers3()` is called from `standalone/address_registry.cpp:704`, so
`CitizenAIComponent_ValidateAndEnterState` (`0x14010f1f0`, `citizenai_candidatelist.cpp:800`)
resolves a real body for every one of the 15 dispatcher cases. **No handler in the table is
null or shimmed.** Nothing was needed in `reg_closeout_lane_c.cpp` for this item.

Note the Ghidra names are misnomers, recorded at `citizenai_candidatelist.cpp:800`:
`0x14010cd80` is **EAT/GetFood** (states 0 and 0xe) and `0x14010d2c0` is **DRINK/GetWater**
(states 1 and 0xf); only `0x14010d790` (state 5, Work) is named for what it does.

### Evidence

`BANISHED_CITIZENAI_TRACE=1` makes each of the three log its entry
(`[Work] … NavigateTo workplace`, `[AdultTrans] … ENTERED`, `[ElderTrans] … ENTERED`).

- **Autostart new game, seed 12345, 3,600 frames** — *no* handler trace, and the reason is
  visible in the same log rather than in the handlers: `[CandList]` shows the candidate
  lists that `BuildCandidateStateList` actually produced were `{8,3,9,12}` and `{3,12,11}`.
  States 5 / 0 / 1 were never *offered*, because the founding town's 24 citizens are
  children (`[Vitals] … age mean=1.97 max=3.93`) with no workplace and no hunger pressure.
  A run with no instance of the thing under test is a null test — so this run proves
  nothing either way, and is recorded only to explain why the obvious test is not the test.
- **Stress town (`BANISHED_STRESS=1 BANISHED_STRESS_CITIZENS=950`)** — 950 adult citizens,
  every workplace staffed through the real seat cascade. See "Verification run" below.

---

## Item 6 — Achievement award toast — **PORTED**

`0x140096960` — the `void (*)(int)` handler retail installs with
`Achievement::AchievementManager::SetEventHandler`
(`?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z`, import slot
`0x140151f90`) at `GameUpdateLoop::Load` step 30 (call site `0x14009752e`, handler LEA at
`0x140097524`). Runtime calls it with the awarded achievement's **index** the instant an
award is recorded; the whole body pushes one event-log entry — the on-screen announcement.

**Port**: `code/Game/Game/eventlog_achievement.cpp` → `EventLog_OnAchievementAwarded`.
**Registered**: `standalone/reg_closeout_lane_c.cpp` (`RegisterExeAddr(0x140096960, …)`,
self-registering static so `standalone/address_registry.cpp` stays untouched for the
parallel lanes).
**Installed**: `code/Game/Game/gameupdateloop_load.cpp` step 30 — previously a comment
saying "that function is not ported yet, so we leave the handler null".

Body, disasm-verified against `0x140096960`..`0x140096aea` (97 instructions):

```
eventLog = *(g_pGameWorldSingleton) + 0x138
ExternalPtr<Achievement> ap; AchievementManager::GetAchievement(**0x1401514f0, &ap, id)
System::String key;          Achievement::GetTitle(ap, &key)
UI::String ui(*(ap+0x20) /*ExternalPtr<StringTable>*/, key)   [0x140151300 — the NAME-lookup ctor]
~key
System::String text = (System::String)ui                      [0x140151308]
InternalBasePtr nullEntity(nullptr)
<AddEventWithName body inlined>: colour 0xFFEA7517, type 0x19, severity 4,
    tile = TileIndex(nullEntity) = -1, icon 9, popup -1,
    FormatIndexed(events[0x19], {0}=season, {1}=year, {2}=text),
    EventArray_Grow(eventLog+0x18, count+1, entry)            [0x140019b70]
```

Every derived constant is exactly what `EventLog_AddEventCore` already computes for
`(type 0x19, severity 4, null entity)` — `SeverityColor(4)=0xFFEA7517`, `IconIndex(0x19)=9`,
`PopupIndex(0x19)=-1`, and `0x19` is outside the `0xb/0xc/0x1e/0x1f` set so there is no
speed-drop tail, matching retail's tail-free body. So the port routes through
`EventLog_AddEventWithName` (the ported `0x140095750`) rather than transcribing a fourth
copy of the append — the eventlog.cpp family note records that copies of this body have
already drifted once.

Faithfulness notes:
- The `UI::String` temporary is **not** destructed, in retail either (it holds an
  ExternalPtr + an index, no owned heap String). Kept.
- `**g_ppAchievementManager` (import slot `0x1401514f0` =
  `?_singleton@AchievementManager@Achievement@@0PEAV12@EA`) is dereferenced unguarded in
  retail. The one guard added returns early only if the slot never resolved at all —
  we cannot reach the function except from Runtime's own handler call.
- The only other deviation: if `UI::String`'s ctor / `operator System::String` fail to
  resolve, the toast announces the raw title *key* instead of being dropped. Stated in
  the source.

Trace: `BANISHED_ACH_TOAST_TRACE=1` (deliberately **not** `BANISHED_ACH_TRACE` — that is
already the achievement-census interval in `standalone/achievement_probe.cpp`).

### Why a plain autostart run cannot show the toast

An award only fires the handler when an instance becomes **newly** met. Awards persist in
`HKCU\Software\Shining Rock Software LLC\Banished`, and `AchievementManager::Reset()` (which
`BANISHED_ACH_RESET=1` calls) clears instance state but **not** the persisted awarded bits —
in the seed-12345 autostart run the census sat at `awarded 5/36` for 3,600 frames with every
`met=0`, so nothing could fire. The population achievements are the reachable ones:
`Settlement` ≥ 300, `Village`, `Town` ≥ 900 (`stat=5`), and `Town` is `awarded=0`. Hence the
verification run below uses `BANISHED_STRESS_CITIZENS=950`.

---

## Verification run

`reimpl-x64.exe` (debug, built from this branch) in `C:/GOG Games/Banished` under the
shared-directory lock:

```
BANISHED_STRESS=1 BANISHED_STRESS_CITIZENS=950 BANISHED_STRESS_FRAMES=2500 \
BANISHED_STRESS_SPEED=10 BANISHED_CITIZENAI_TRACE=1 BANISHED_ACH_TOAST_TRACE=1 \
BANISHED_EVENT_TRACE=1 BANISHED_ACH_TRACE=600 ./reimpl-x64.exe
```

Step 30 install, first run (seed 12345 autostart), `standalone_error.log:148`:

```
[GUL::Load] step30 AchievementManager::SetEventHandler (mgr=0000019e800da5d0 handler=EventLog_OnAchievementAwarded)
```

### Item 6 — toast, verified end to end

```
standalone_error.log:150  [GUL::Load] step30 AchievementManager::SetEventHandler
                          (mgr=000002669af2a5d0 handler=EventLog_OnAchievementAwarded)
standalone_error.log:5163 [AchToast] handler ENTER id=2
[retail data elided]
standalone_error.log:5165 [EventLog] AddEventWithName type=25 sev=4 icon=9 popup=-1 tile=-1 (count now 2)
                                     [retail data elided]
                                     [retail data elided]
                                     [retail data elided]
                                     [retail data elided]
```

`type=25` (0x19), `sev=4`, `icon=9`, `popup=-1`, `tile=-1` — every constant retail bakes in.
The `@2` slot of the real `events` StringTable template is filled with the achievement's
looked-up title, so the `UI::String` name-lookup ctor + `operator System::String` chain works;
`(count now 2)` is the event-log array before the append, i.e. the entry landed.

Runtime's award path is `AchievementManager::Update` (Runtime `0x18009e6c0`), which fires the
handler only on a **new** award:

```
if (((*(char*)(idx + *(longlong*)(this+8)) == '\0') && SetAchievementMet(this, idx)) &&
    (*(code**)(this + 0x28) != 0))
   (**(code**)(this + 0x28))(*(int*)(this + 0x40));
```

`SetAchievementMet` (Runtime `0x18009eca0`) writes `Persistence::Set("Ach", "<idx>", true)` →
`HKCU\Software\Shining Rock Software LLC\Banished\Ach<idx>`, and `ReadAchievements` reloads it
on the next launch — so an achievement only ever toasts **once per machine** until that value
is deleted. `AchievementManager::Reset()` (`0x18009e5c0`) rebuilds the instances but does **not**
clear the awarded array, so `BANISHED_ACH_RESET=1` does not re-arm a toast. Neither `Reset` nor
`SetAchievements` (`0x18009e860`) clears the handler at `+0x28`, so a single step-30 install
holds for the life of the manager. To re-test: delete `Ach<idx>` from that registry key first
(this wave deleted and restored `Ach2`, leaving the key as it was found: Ach0/1/4/20/30).

### Item 2 — handlers observed entering

Same run, `BANISHED_CITIZENAI_TRACE=1` (each trace point is capped at 6 lines):

```
[ElderTrans] self=000002669deaf6a0 ENTERED entity=000002669d85ec18      (x6)
[AdultTrans] …                                                          (x12, both trace points)
[Work]       …                                                          (0)
```

`0x14010cd80` (Eat / "AdultTransition") and `0x14010d2c0` (Drink / "ElderTransition") enter
in-game through the ported bodies. **`0x14010d790` (Work, state 5) was not observed entering,
and the candidate-list census says why it cannot be**:

```
[CandCensus] --- after 20000 calls ---
[CandCensus]   EatOther      ( 0) offered=844   top=844    by prio: p6=844
[CandCensus]   DrinkOther    ( 1) offered=855   top=855    by prio: p6=855
[CandCensus]   Education     ( 3) offered=20000 top=15243  by prio: p3=20000
[CandCensus]   Gather        ( 6) offered=2598  top=2435   by prio: p4=2598
[CandCensus]   SeekHappiness ( 8) offered=646   top=623    by prio: p5=646
[CandCensus]   Idle          ( 9) offered=10084 top=0
[CandCensus]   FindWork      (10) offered=33    top=0
[CandCensus]   Wander        (11) offered=9883  top=0
[CandCensus]   Leisure       (12) offered=20000 top=0
[CandCensus]   LongTermPlan  (13) offered=118   top=0
```

State 5 is **absent from the table — offered 0 times in 20,000 candidate-list builds.**
`BuildCandidateStateList` appends Work in exactly two places
(`citizenai_candidatelist.cpp:465` early "already working" branch, `:536` the housed branch),
and both are gated on `CitizenComponent+0x68` (the housing/attendance link). The per-citizen
gate line shows it is zero for the stress town:

```
[CandCensus] citizen=0000029dc5d8fb30 branch=2 home(+0x238)=29dc4756280 housed(+0x68)=0
             elder(+0x208)=0 age=1.33/2.00 happy=0 hunger=1.62/18.00 warmth=0.00/15.00
             | list: Gather(6)/p4 Education(3)/p3 Leisure(12)/p0 Wander(11)/p0
```

So the Work handler is ported, registered and reachable from the dispatcher, but nothing in
this harness can reach it: the blocker is `+0x68` never being linked, which lives in the
housing / attendance path, not in the handler or in this lane. **Not claimed as observed.**

---

## Files changed by this lane

| File | Change |
|---|---|
| `code/Game/Game/eventlog_achievement.cpp` | NEW — port of `0x140096960` |
| `standalone/reg_closeout_lane_c.cpp` | NEW — `RegisterExeAddr(0x140096960, …)`, self-registering |
| `code/Game/Game/gameupdateloop_load.cpp` | step 30 now installs the handler (was a "not ported yet" comment) |
| `code/Game/Component/citizenai_handlers2.cpp` | comment only — corrected the stale "NOT PORTED" list that PLAN.md §1.2 row 2 was citing |
| `docs/waves/exe_closeout/lane_c.md` | this file |

`tools/dispatch_slot_map.py --check`: 0 name mismatches (unchanged — this lane added no
dispatch-slot registrations).

## Still open (out of lane)

- `CitizenComponent+0x68` is 0 for every citizen in the `BANISHED_STRESS` town, so candidate
  state 5 (Work) is never offered and the Work handler cannot be exercised there. Whoever owns
  housing/attendance should re-check the census table above with a town where citizens are
  actually housed.
