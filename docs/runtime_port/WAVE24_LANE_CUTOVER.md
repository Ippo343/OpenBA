# WAVE 24 LANE CUTOVER — attempt 25, and the next wall has a name

Lane branch `wave24/cutover`, cut from `wave24/base` @ `f5d70de2`.
Game-directory lock held as `wave24-cutover` for 3 minutes (21:08–21:11), acquired
bare, released; `Runtime-x64.dll` restored and re-verified present (2762752 bytes)
after **both** runs.

**Rows promoted: zero.** That was the expected shape (kickoff §2). The deliverable is
the wall, and it is named with byte-level evidence plus a same-tip A/B control.

---

## 0. HEADLINE

**The next cutover wall is `??0Stream@Audio@@QEAA@XZ`** (Audio::Stream default ctor,
Runtime `0x1800a0e30`, ordinal 410), called from **our own ported
`??0Device@Audio@@QEAA@XZ`** — `(anonymous namespace)::Ctor_AudioDevice`,
`runtime/src/engine/singleton_ctors.cpp:742` — reached from
`RtEngine_Engine_Startup+0x82a`.

```
Fault: EXEC of 0000000000000000
  #1  reimpl-x64.exe + 0x2E811B   ->  Ctor_AudioDevice(void*) + 0x136
  #2  reimpl-x64.exe + 0x2E05BC   ->  RtEngine_Engine_Startup + 0x82a
  #3  reimpl-x64.exe + 0x1A795D   ->  Game::RunGameMain + 0x25d
```
```
[RtUnresolved] ??0Stream@Audio@@QEAA@XZ  (asked by 00007ff6e25484d7)
[RtResolve] MISS "??0Stream@Audio@@QEAA@XZ" -> NULL. ... If this is cutover,
            this symbol still needs minting.
```

That symbol is **already on Wave 24's board**: it is lane **SMALLBODY**'s first row
(kickoff §2, `Audio ??0Stream@Audio@@QEAA@XZ (ctor)`). The cutover boot and the body
census independently selected the same function this wave. I did not port it — it is
another lane's row and porting it here would collide.

**The wall moved, and the move is measured, not inherited.** Wave 23 lane ENGINE
predicted `RtEngine_Engine_Startup+0x777` statically and never ran a boot. This lane
ran the boot **and** its control, on one exe, and got `+0x777` with ENGINE's port
disarmed and `+0x82a` with it armed.

---

## 1. WHAT I MEASURED, AND THE COMMANDS THAT RE-DERIVE IT

### 1.1 The run

New script, committed: `tools/cutover_attempt25.sh` — `WAVE9_CUTOVER_BOOT_RUNBOOK.md`
verbatim, with the DLL restore as a `trap … EXIT INT TERM` and a post-restore presence
assertion.

```
mingw32-make standalone-debug
tools/game_dir_lock.sh acquire wave24-cutover 900        # BARE, never piped
tools/cutover_attempt25.sh docs/runtime_port/wave24_cutover/attempt25 120
tools/game_dir_lock.sh release wave24-cutover
```

Exe under test, **identical in both runs** (printed by the script, not assumed):

```
32690559 bytes  2026-09-11 21:07:02  md5 cc8dc7c1803232c9ccea432734d2aa22
```

Env (runbook §2 exactly): `BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1
BANISHED_RT_MINT_CTOR=1 BANISHED_RT_ALLOC=1 BANISHED_RT_STRICT=1
BANISHED_RT_MINT_SHADOW=1 BANISHED_AUTOSTART=1`. No `BANISHED_RT_DIFF`.

### 1.2 The run was valid — three independent checks, none of them my own say-so

| check | line in the run's own log |
|---|---|
| the DLL really was absent | `[RtMintShadow] ==== phase=init  DLL=ABSENT (post-cutover; oracle gone) ====` |
| the mint really registered | `[RtMint] registered 201 by name, 207 by address` |
| the allocator really was ours | `[RtAlloc] ATTEST: ARMED (cutover, DLL absent) (exports=27)` |

Also `[RtMint] minted 62 vtables (578 slots: 326 ported, 109 unported-trap,
143 purecall-trap), 104 BaseTypeInfo, 39 storage symbols`, and
`[RtMint] constructed 3 of the 3 statics the DLL's CRT builds at load`.
**No `[RtMint] FATAL` trap fired at any point in either run.**

### 1.3 Symbolisation, and the one hand-check

`nm -C --defined-only standalone/build/reimpl-x64.exe`, image base `0x140000000`.
Then, because instrument rule 1 says to hand-check a row **while the tool is agreeing
with me**, I disassembled the frame-#1 return address instead of trusting the symbol:

```
objdump -d --start-address=0x1402e80f0 --stop-address=0x1402e8130 \
        standalone/build/reimpl-x64.exe
  1402e8106:  call  <Ctor_AudioDevice::{lambda}>      ; the RT_EXPORT_ONCE resolve
  1402e810f:  add   $0xa00,%rdx                       ; self + 0xa00
  1402e8119:  call  *%rax                             ; RAX = 0 in the crash dump
  1402e811b:                                          ; <- frame #1 return address
```

`self + 0xa00` is literally `singleton_ctors.cpp:742`
(`RT_EXPORT_ONCE(StreamCtorFn, "??0Stream@Audio@@QEAA@XZ")(… + 0xa00)`). The
attribution is byte-exact, not a symbol-table nearest-match. (AT&T syntax throughout —
kickoff §3 item 6.)

### 1.4 The two crash-log fields I did NOT use

- **`asked-by` names the RESOLVER.** Confirmed here rather than assumed:
  `00007ff6e25484d7` symbolises to `RtResolveCached(char const*, void**) + 0x34`.
  Every conclusion above comes from the stack trace, not from this field.
- **`VCALL-of-0`** printed `this=…6C0 vtable=0` with eight zero words. The real call is
  a plain indirect `call *%rax` through a resolved export pointer, not a virtual call.
  The heuristic is wrong here in exactly the documented way, and nothing above rests on it.

---

## 2. MY PROBE SHAPE (§1c), AND ITS CONTROL

**Probe shape: none of the five.** This lane's instrument is the **controlled cutover
boot** of `WAVE9_CUTOVER_BOOT_RUNBOOK.md` — a live gate, not an oracle. The five shapes
in §1c all compare our body against retail's; at cutover **retail is not loaded**, so
every one of them is structurally unavailable (the shadow check says so itself:
`The DLL is not loaded, so there is nothing to compare against`). The deliverable is
not "is body X right" but "what does the process ask for and not get, first".

**Control — a same-tip A/B, not a cross-wave comparison** (kickoff §3 item 8).
The subject my probe compares is *the fault site*. So I corrupted exactly that: a second
run, same exe, same md5, same env **plus `BANISHED_RT_ENGINEPLATFORM=0`**, lane ENGINE's
own kill switch (`runtime/src/engine/rt_engine_port.cpp:108`), which un-registers the
Wave 23 `PlatformConfigure` port.

| | armed (attempt 25) | control (`BANISHED_RT_ENGINEPLATFORM=0`) |
|---|---|---|
| fault site | `RtEngine_Engine_Startup + 0x82a` | `RtEngine_Engine_Startup + 0x777` |
| last unresolved name | `??0Stream@Audio@@QEAA@XZ` | `?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z` |
| DX11 device created | yes (`D3D11CreateDevice` +80.58ms, swapchain, `fallbacks to shipped code: 0`) | never reached |
| frames in trace | 9 | 8 |

**Effectiveness counter: 1 of 1 control case changed the outcome** — and it changed
*both* observables (site and symbol), in the direction predicted. `+0x777` is the exact
offset Wave 23 derived statically without a run, so the control also independently
confirms that static derivation.

This is what converts "the wall moved" from a cross-wave comparison (which rule 8 bars)
into a one-tip, one-binary measurement.

**What the control does NOT establish:** it shows ENGINE's port is *sufficient* to move
the wall from `+0x777` to `+0x82a`. It says nothing about whether any of the other
things between those offsets (`Video::Device::PlatformStartup`, `Audio::Configuration`)
would independently have blocked.

---

## 3. DISTANCE TRAVELLED — the runbook's progress metric

Runbook ladder: `ResolveInit` → `RuntimePortInit` → `[RtMint] registered` →
`Engine::Startup` → `[BootState] RUNNING` → `[AutoStart] menu settled` →
`[AutoStart] sequence fired` → first frame → gameplay.

| attempt | reached | died at |
|---|---|---|
| 23 (Wave 10 open) | — | `DeviceData::EnumerateInterfaces` |
| 24 (Wave 16 bootgate) | — | `DeviceData::EnumerateInterfaces`, unchanged |
| **25 (this lane, control)** | `Engine::Startup` | `Startup+0x777`, `PlatformConfigure` |
| **25 (this lane, armed)** | `Engine::Startup`, **Video PlatformStartup complete** | `Startup+0x82a`, `??0Stream@Audio@@` |

**The attempt-23/24 wall is gone.** The DX11 seam table in the armed run prints all 20
`DeviceData` slots as `OURS` and ends `[Dx11Seam] fallbacks to shipped code: 0
(expected 0)`. The run reaches `D3D11CreateDevice`, builds the swapchain and the
colour/depth buffers, sizes the window, and only then dies in audio.

`[BootState] RUNNING` was never reached — `standalone_boot_state.txt` reads
`STATE=BOOTING pid=30168 uptimeSec=0`. Boot dies ~0.5s in, so `tools/boot_state.sh
--wait` was never needed; the process was already gone.

---

## 4. THE CARRY-IN LISTS — all four satisfied, none of them the wall

The kickoff handed me four inversion items. **`BANISHED_RT_MINT=1` registers all rows
in one switch** (`rt_data_mint.cpp:937`), so every one of them was live in both runs.
None produced a trap and none is the wall:

| carry-in | status in attempt 25 |
|---|---|
| the **ten Object rows** (`WAVE23_LANE_OBJ.md` §6) | inside the 201/207; no `[RtMint] FATAL`, no split-brain symptom before the fault |
| **six Video rows**, `HARMFUL` → `REQUIRED` | inside the same 201/207; the run completed `Video::Device::PlatformStartup` and full DX11 device creation, which is the route those rows serve |
| **`??_7Thread@System@@6B@`** | minted (`rt_data_mint_tables.inc:1593`, 2 slots) and registered at cutover as predicted |
| **`?_singleton@MemoryAllocator@System@@`** | registered; **`engine_funcs.cpp:1629` did not fault**, and `[RtAlloc] ATTEST: ARMED (cutover, DLL absent)` plus a successful `D3D11CreateDevice` is the positive evidence that the allocator ran on our storage |

**Finding: the "one residual cutover dependency" named in
`singleton_ctors.cpp:719-722` is STALE and discharged.** That comment says the
per-record vtable at `0x180157c60` "is not an export and is not in lane C2's mint table,
so it is reached by address and will be null at cutover." It **is** in the mint table
today, by address: `rt_data_mint_tables.inc:1542`,
`{ nullptr, 0x180157c60ull, 7, … }`. The run walked all 64 records that stamp it and
faulted three lines further down, on something else. This is another instance of
kickoff §5 — a discharge that did not propagate to the comment that states the blocker.

---

## 5. THE FULL CUTOVER RESIDUE — and it is a LOWER BOUND

A cutover boot is a **GATE, not a PLANNER**. Everything below is only what the process
asked for before it faulted, 0.5s in. **Do not read it as remaining work.**

**Name doors that missed (5 — `BANISHED_RT_STRICT`):**

| symbol | asked by (symbolised) | called? |
|---|---|---|
| `?BuildParameters@MaterialInstance@Graphics@@` | `RegisterUnregLane3()+0x1a3` | no — init-time pointer fetch |
| `?Serialize@MeshInstance@Graphics@@` | `RegisterDispatchSlots()+0x91f` | no — init-time |
| `?Update@Scene@Audio@@` | `EngineFuncsInit()+0x179f` | no — init-time |
| `?Draw@Renderer@Graphics@@` | `EngineFuncsInit()+0x4682` | no — init-time |
| **`??0Stream@Audio@@QEAA@XZ`** | `RtResolveCached+0x34` | **YES — this is the wall** |

Four of the five are *lookups at registration time* that stored a null and returned;
only the fifth was an in-flight call. That distinction is not in the log's own wording
and it matters: a naive reading of the residue names four Graphics/Audio rows that this
run never executed. All four are already on the Wave 24 board (GFXMAT and SMALLBODY).

**Address doors that missed (5):**

```
[AllocLock] could not resolve Allocate (0x1800c6e60)      } expected: no DLL, so no
[AllocLock] could not resolve Free    (0x1800c6c00)       } DLL-internal callers to redirect
[FNHook] could not resolve Video::Device::InitializeInterfaces
[FNHook] could not resolve ExternalFactory::FindOrLoad
[FNHook] could not resolve External::Load
```

The first two are benign and the code says so:
`[RtAllocSwap] cutover arm: no detour installed (no DLL, so no DLL-internal callers to
redirect)`. `Video::Device::InitializeInterfaces` printed a loud `*** DETOUR FAILED ***`
warning that at cutover *"something MUST call `VideoInterfaceFillDx11Slot()`"* — and
**something did**: `[VideoInterface] DX11 slot pre-filled on Device … Runtime will not
LoadLibrary VideoDX11-x64.dll`, twelve lines later. That warning is a false alarm at
cutover and should be re-worded; it is the one line in this run most likely to be
mis-read as a wall by the next attempt.

The two `Object` doors (`FindOrLoad`, `External::Load`) were left null
(`[ResLoad] serialized: FindOrLoad=0 External::Load=0`) and were never called before the
fault. They are live cutover work, not discharged — but this run cannot say when they
would bite.

There is **no `[RtResidue]` exit list** in either run: the process crashed, so the
at-exit dump never ran. The five `[RtUnresolved]` lines above are the whole name-side
record.

---

## 6. WHAT THIS LANE DID **NOT** ESTABLISH

1. **I did not name the wall after this one.** That needs `??0Stream@Audio@@` ported and
   another boot. A gate cannot plan, and the two adjacent runs in this lane are the
   proof: the armed run's next wall was not derivable from the control's.
2. **I did not port anything.** Zero rows promoted, by choice — the wall row belongs to
   lane SMALLBODY and the cross-lane collision is worse than the delay.
3. **I did not verify the carry-in rows are CORRECT**, only that they registered, ran,
   and did not trap before the fault. "Reached `D3D11CreateDevice`" is evidence the
   Video rows are on a working path; it is not a differential verdict, and no
   differential verdict is available at cutover (§2).
4. **I did not measure the census.** No figure in this doc is a board figure.
5. **I did not establish that `??0Stream@Audio@@` is the *only* thing standing between
   `+0x82a` and the next stage.** It is the first, which is all a gate reports.
6. **Attended-boot Half B is untested.** Both runs set `BANISHED_AUTOSTART`, so
   `HarnessGuardUnattendedBoot` cleared the arming bit and `PlatformConfigure` took
   Half A. Lane ENGINE's named-refusal path for Half B **did not execute in either run**
   and remains unexercised at cutover.
7. **One boot per configuration.** Both results are single runs. They are stable in the
   sense that the two runs differed only in the flag under test and differed exactly
   where predicted, but neither was repeated.

---

## 7. RESIDUALS — each with SCOPE and ADDRESSEE

I touched no `RT_UNPROMOTABLE` tag, so there is no tag scope/addressee to declare.
These are handover items.

1. **`??0Stream@Audio@@QEAA@XZ` (`0x1800a0e30`, ord 410) is the cutover wall.**
   [SCOPE: CUTOVER] [ADDRESSEE: **Wave 24 lane SMALLBODY**, which already owns this row]
   Two dependency facts, re-derived here from the shipped export directory with
   `pefile`, not quoted:
   - `??0Thread@System@@QEAA@I@Z` (`0x1800ca4e0`, ord 443) — the base ctor Stream calls —
     **is already ported and registered** (`runtime/src/system/thread_win.cpp:457`). Not
     a blocker.
   - `??_7Stream@Audio@@6B@` (`0x180154950`) — the vtable Stream stamps — **is NOT in
     `rt_data_mint_tables.inc`** (`grep -n "Stream@Audio@@6B\|0x180154950"` returns
     nothing). At cutover that stamp writes a null vptr. Porting the ctor without
     minting this table moves the fault rather than removing it.
2. **The `[VideoInterface] *** DETOUR FAILED ***` banner is a false alarm at cutover**
   and the slot is filled twelve lines later.
   [SCOPE: CUTOVER] [ADDRESSEE: the **Wave 24 integrator**] — re-word it so attempt 26
   does not spend a session on it.
3. **`singleton_ctors.cpp:719-722` states a blocker that is discharged** (§4).
   [SCOPE: ALL ROUTES] [ADDRESSEE: the **Wave 24 integrator**] — one comment edit; I did
   not make it because that file is lane SMALLBODY's working surface this wave.
4. **`ExternalFactory::FindOrLoad` / `External::Load` are null at cutover** and were not
   reached before the fault. [SCOPE: CUTOVER] [ADDRESSEE: **whoever runs attempt 26**] —
   expect them after the audio wall, do not pre-port on this evidence alone.
5. **Lane ENGINE's Half-B named refusal has never executed.** [SCOPE: CUTOVER, ATTENDED]
   [ADDRESSEE: **whoever runs attempt 26**] — an attempt with the harness env vars unset
   and the arming bit intact is the only run that exercises it.

---

## 8. FILES

```
tools/cutover_attempt25.sh                                  new — the runbook, scripted
docs/runtime_port/WAVE24_LANE_CUTOVER.md                    this doc
docs/runtime_port/wave24_cutover/attempt25/                 armed run
    standalone_{crash,error,debug}.txt, standalone_boot_state.txt
docs/runtime_port/wave24_cutover/attempt25_control/         BANISHED_RT_ENGINEPLATFORM=0
    standalone_{crash,error,debug}.txt, standalone_boot_state.txt
```

The four captured logs are renamed `.log` -> `.txt` on the way in: `.gitignore:37`
excludes `*.log`, which silently dropped Wave 12's run evidence and had to be fixed in
a follow-up commit. The script writes `.log`; the rename is deliberate.

No source file was modified by this lane.
