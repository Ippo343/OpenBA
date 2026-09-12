# WAVE 23 — LANE ENGINE

Branch `wave23/engine`, cut from `wave23/base` at `a60351e3`. Private worktree.
**Every census figure in this document is BRANCH-LOCAL** (rule 13). The integrator
must re-measure on the merged tip; Wave 21 published from lane reports and had to
correct itself, and in Wave 22 the merged System figure matched neither lane.

Subject: all ten Engine gap rows — 3 BODY, 4 VTABLE, 3 DATA.

**Headline.** The three-wave `[NOT RE-DERIVED]` silence on `?PlatformConfigure@Engine@1@`
ends: **the wall is still there, and its mechanism was never written down.** It is
not a missing feature — it is one unported callee on a caller we already ported.
Half of the function is now ported and the wall is gone for the branch every
unattended boot takes; the other half needs Win32 dialog **resources that live
inside the DLL**, which is a named residual, not an oversight. Separately,
`?LoadResources@UpdateLoop@Engine@@` is ported, registered, detoured and **RUNS**
(calls=6) — and **lane ENGINE's own negative control refuted lane ENGINE's own
verification claim about it**, which is §2.4 and is the finding most worth
carrying forward.

Rows moved: **2 of 10** (both BODY). 4 VTABLE are assessed READY-TO-PROMOTE and
deliberately **not** promoted — §3.4 says why that is a decision and not an
omission. 3 DATA are DO-NOT-PROMOTE / BLOCKED, each on its own xrefs.

---

## 0. TASK 0 — grep before porting (rule 2)

`?StrCompare@` read as unported for twenty waves because a correct body was never
registered. Before writing a line, every one of the ten rows was grepped across
`runtime/ code/ engine/ standalone/` for **the plain class/method name**, not only
the mangled symbol (the mangled-only grep is what misses these).

Result: **no hidden body for any of the ten.** `runtime/src/engine/` held
`loops.cpp`, `engine.cpp`, `engine_lifecycle.cpp{,_win}`, `mainloop_run.cpp`,
`mainloop_video.cpp`, `singleton_ctors.cpp{,_win}`, `rt_engine_port.cpp` — and
LoadResources appears in all of three places, every one of them a comment or the
DLL door: `loops.cpp:992` ("Held back"), `rt_engine_port.cpp:92` (prints the
holdback), `engine_funcs.cpp:1705` (`ResolveExport` into a pointer). Which
produced a finding in its own right — see §2.2.

Board re-derived on this tip before starting: **Engine 33 called / 23 ported, gap
10**, matching the kickoff row-for-row. The kickoff was NOT stale for this lane.

---

## 1. HEADLINE — `?PlatformConfigure@Engine@1@`, re-derived after three waves

### 1.1 The wall is still there, and here is what it actually is

Re-derived **statically**, with no DLL-absent boot required, which matters
because a cutover boot is a GATE, not a planner — it can only name what it
reached before faulting.

```
tools/unsatisfied_export_scan.py
  -> 999 distinct names resolved through a door; 54 have neither a port nor a
     mint entry; 50 have at least one site OUTSIDE a diff guard, including:
       ?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z   runtime/src/engine/engine_lifecycle.cpp:710
```

The mechanism, which no previous wave wrote down: **our own ported
`Engine::Engine::Startup` calls it.** `engine_lifecycle.cpp:710` is
`RT_EXPORT_ONCE(PlatformConfigureFn, "?PlatformConfigure@Engine@1@…")(self, true,
self->_platformHandle)`. With the DLL gone that resolves to null, and the EXEC-of-0
lands at `RtEngine_Engine_Startup+0x777` — **inside our code, not the DLL's**. The
wall is one unported callee on a ported caller's only path.

It is also the FIRST such callee in `Engine::Startup`: the other unsatisfied
entries in that file are at lines 842, 846, 937, 949, 955 and 960, all inside
`Shutdown`. Nothing before line 710 is unsatisfied. So the static scan reproduces
the runtime observation exactly, and independently of it.

### 1.2 The function has two halves and only one is portable

Read all 272 instructions (`0x1800a1670`–`0x1800a1ad4`) before scoping.

**Half A** — `configure == false`, plus `configure == true` on the branch that
declines the dialog. Dependencies, all already ported and checked in the census
CSV before a line was written: `?Save@Configuration@Video@@` (`0x1800f6d10`),
`?WriteSettings@Persistence@IO@@` (`0x1800b88e0`), `?Set@String@System@@`,
`?GetCommandLineString@Configuration@System@@` (`0x1800c5300`). **Ported.**

**Half B** — the launcher dialog actually shown. Needs, and pefile against the
shipped export directory confirms every one is **unexported**:

| thing | address | what it is |
|---|---|---|
| dialog template | rsrc `0x66` | the launcher (Options/Video/Play) |
| dialog template | rsrc `0x68` | the options sub-dialog |
| bitmap | rsrc `0x2711` | the launcher art `WM_CTLCOLORDLG` blits |
| `FUN_1800a1ae0` | `0x1800a1ae0` | launcher dialog proc |
| `FUN_1800a1dc0` | `0x1800a1dc0` | options dialog proc |
| `FUN_1800a2020` | `0x1800a2020` | control-region helper |
| `DAT_1801fc680` | `0x1801fc680` | the DLL's own HINSTANCE |
| `DAT_1801fc698/699` | — | the "reset" / "nomods" checkbox bytes |
| `FUN_18000f7d0` | `0x18000f7d0` | recursive clear of Persistence's settings map |

`0x1801fc680`, `…688`, `…698`, `…699` sit **between** two exports
(`?_singleton@Application@1@` at `0x1801fc678` and `?_singleton@FileSystem@IO@@`
at `0x1801fc6a8`) and are not themselves exported. The dialog resources are in
the DLL's `.rsrc`. **Half B is not portable in this lane at any effort level** —
it is a launcher-UI port plus a resource script for the replacement exe.

### 1.3 What landed, and what it does and does not claim

- `runtime/src/engine/engine_platform.cpp` — the body, byte-faithful.
- `runtime/src/engine/engine_platform_win.cpp` — the Win32 seam (PLAN §4.1
  decision 1): `GetAsyncKeyState`, `DialogBoxParamW`, the two checkbox bytes,
  and (for §2) `GetFileAttributesW`.
- Registered under **name and address** (`RegisterRuntimeExport` +
  `RegisterRuntimeAddr(0x1800a1670, …)`), kill switch
  `BANISHED_RT_ENGINEPLATFORM=0`.

**DOES claim:** Half A executes for real; at cutover Half B prints a named
refusal naming the symbol, the missing resources and the reason, instead of
jumping to 0. That is what lets the next cutover attempt name the NEXT wall.

**DOES NOT claim:** that the launcher works without the DLL. An **attended**
cutover run whose arming bit is set gets no launcher and proceeds as if Play were
pressed. That is a real divergence and it is in the source banner, not only here.
An **unattended** run never reaches Half B, because `HarnessGuardUnattendedBoot`
(`standalone/main.cpp:184`) clears the arming bit — which is why the measured
run below shows `dialog-shown=0`.

### 1.4 Measured, DLL-present

```
[RtPort]   Engine::PlatformConfigure: calls=1 deconfigure=0 dialog-skipped=1 dialog-shown=0
```

One call, Half A, game booted to a world and exited 0 with a zero-byte crash log.
`calls=1` is the effectiveness control this body can have: it runs once per
process, so `divergences=0` would say nothing on its own.

### 1.5 Two decompiler traps recorded, because both nearly shipped

1. `param_2` (the `void*`) is **never read** — R8 is untouched for all 272
   instructions. Kept because that is the signature.
2. The "launcher" test and the "nomods"/"reset" tests look identical in Ghidra's
   C and are **different mechanisms**. "launcher" goes through
   `GetCommandLineString` and tests the returned `String`'s data pointer
   (`0x1800a16df`); "nomods"/"reset" go through the unexported map find at
   `0x18002ace0` on `Configuration+0x10` and test **presence**
   (`0x1800a1832`, `0x1800a18f2`).

### 1.6 A documentation defect found on the way

`runtime/types/engine/Engine.h:107` said `0x1800f6d10` was
`Video::Configuration::Shutdown`. pefile says it is
`?Save@Configuration@Video@@QEAAXXZ`. Corrected in that header with the
correction called out inline. It mattered: the wrong name made the launcher
arming bit look like it was written by a teardown, when it is written by an
ordinary save that also runs **mid-boot**, twice, inside PlatformConfigure.

---

## 2. `?LoadResources@UpdateLoop@Engine@@` — Wave 8 lane A's oldest holdback

### 2.1 ERRCFG's claim, RE-DERIVED not inherited (rules 1 and 3)

Wave 22 lane ERRCFG said this body plus an eight-byte accessor are the COMPLETE
unported reader set of `?_singleton@Configuration@System@@0PEAV12@EA`. Re-derived
from primary evidence: symbol address found independently
(`0x1801fc6f8`, RVA `0x1fc6f8`, the only export name at that RVA), all **8**
Ghidra xrefs walked, READ split from WRITE **by reading the instruction** rather
than trusting Ghidra's tag.

| site | function | R/W | ported? |
|---|---|---|---|
| `0x1800abc8b` | `FileSystem::Startup` | READ | ported + registered |
| `0x1800abd2e` | `FileSystem::SetupDirectories` | READ | ported + registered |
| `0x1800ddbe0` | `UpdateLoop::LoadResources` | READ | **was** unported |
| `0x18000da70` | `Configuration::Get` | READ | unported, 8 bytes, uncalled |
| `0x1800c51e0` | `Configuration::Configuration` | WRITE | ported |
| `0x1800db954` | `Engine::Startup` | WRITE | ported |

The remaining 2 xrefs are the EAT slot at `0x1801b5550` and the export-directory
entry — neither touches the storage. A raw-file scan of the DLL closed the gap
ERRCFG had left open: the 4-byte RVA appears **once** (the EAT entry) and the
8-byte VA **zero** times, so no vtable, jump table or data literal holds this
static's address and there is no indirect reader the operand-level xrefs could
have missed.

**VERDICT: ERRCFG's claim is TRUE as written.** Not one of the four corrections
Wave 22 taught us to expect was needed here.

### 2.2 A second finding that fell out of Task 0

`fn_UpdateLoop_LoadResources` (`engine_funcs.cpp:1705`) is **resolved and called
from nowhere**. The only three occurrences in the entire tree are the typedef,
the resolve, and a `printf` of its value. Our side has **zero callers.**

### 2.3 What porting it DOES and DOES NOT discharge — route-scoped (rule 5)

- **At CUTOVER**: no DLL code exists, so once `?Get@Configuration@System@@` is
  also ported (8 bytes, uncalled, near-free) the reader set is entirely ours and
  the row becomes promotable.
- **DLL-PRESENT**: the DLL's own `Configuration::Get` at `0x18000da70` still
  reads `0x1801fc6f8` through a RIP-relative operand baked into `.text`, and any
  in-DLL rel32 caller reaches that, not us. Our port cannot intercept an in-DLL
  call. **Promoting the row today still strands those readers.**

So lane ENGINE hands the System owner a **CONDITIONAL**, not a green light. The
kickoff's framing — "porting it discharges a named blocker" — is right for
cutover and wrong for today, and the difference is not cosmetic.

### 2.4 THE FINDING WORTH MORE THAN THE ROW: registration was INERT, and then my own verification claim was false

**First measurement, registered but not detoured:**

```
[RtPort]   Engine::UpdateLoop::LoadResources: calls=0 ...
```

`calls=0` on a body that unquestionably executes on every game load. Diagnosed
rather than asserted: our tree has zero callers (§2.2), and the one live caller
is the DLL's own `?Run@Loader@Engine@@` (`0x1800dbfc0`) on the **loader thread**,
reaching LoadResources by an **in-DLL rel32 CALL** that never consults the export
table. `?Run@Loader@Engine@@` is not merely unported — it is not in the census
`called` set at all, and its vptr comes from `??_7Loader@Engine@@6B@` stamped
into the DLL's `_loader` static by the DLL's own CRT initialiser. Neither is a
door. **This is "we intercept only at doors", stated about a BODY rather than a
vtable row**, and the census cannot tell the two situations apart.

So `standalone/loadresources_detour.cpp` was added — a 13-byte splice on
`0x1800dd7d0`, opt-in via `BANISHED_RT_ENGINELOADRES_DETOUR=1`. (13, not the
installer's default 12: the prologue bytes were read out of the shipped DLL with
pefile and start `40 55` — a REX-prefixed `PUSH RBP` — so the boundaries are 11
and 13, and a 12-byte splice would land mid-instruction. `runtime_detour.h` says
plainly that the byte guard is SILENT on boundary errors.)

**Second measurement, detour armed:**

```
[LoadResDetour] armed on 0x1800dd7d0
[RtPort]   Engine::UpdateLoop::LoadResources: calls=6 ownList=3 modsEnabled=0 ...
world built, 24 citizens through frame 480, exit 0, zero-byte crash log
```

And this banner then claimed: *"because boot depends on the load, a green boot
with the detour on IS a real functional test."*

**THE NEGATIVE CONTROL REFUTED THAT (rule 9).** `BANISHED_LOADRES_NEGCONTROL=1`
forces the root-resource block to be skipped — corrupting exactly the value the
port computes and stores into `UpdateLoop+0x08`:

| run | calls | ownList | outcome |
|---|---|---|---|
| detour on, control off | 6 | **3** | world built, 24 citizens, exit 0 |
| detour on, control **ON** | 6 | **0** | world built, 24 citizens, exit 0 |

**NOT CAUGHT.** And this is not the mirror trap — the control corrupts something
the port genuinely computes, so the instrument is sound and the *probe* is blind.
The error was reading `loader_loop_stubs.cpp`'s "skipping it crashes boot 6/6" as
coverage of this subject: that sentence is about skipping
`Engine::Loader::Load` **entirely**, a far bigger deletion than nulling three
resource lists.

**What the detoured green boot DOES establish:** the body runs six times on the
real loader thread, does not crash, does not corrupt the heap or the `String`
refcounts badly enough to fail a full world build, and returns.
**What it does NOT establish:** that its outputs equal retail's. **There is no
oracle for this body and this lane did not build one.** The control is kept in
the source so the next lane re-runs it instead of re-inventing it.

The detour stays **opt-in**, and now for a second reason: not only is boot
load-bearing on this path, the port's outputs are unverified.

---

## 3. THE FOUR VTABLES

Extent, readiness and promotability answered **separately** (rule 4) and
route-scoped (rule 5). Extent re-derived by dumping each table with an 18-slot
**overshoot**, so the terminator was read rather than assumed.

### 3.1 Extent — and the kickoff row list is WRONG for two of them

| table | DLL addr | mint claims | **actual** | terminator read |
|---|---|---|---|---|
| `??_7DialogLoaderLoopT@Engine@@6B@` | `0x1801578d0` | 13 | **13** | zero qword at `0x180157938` |
| `??_7LoaderLoop@Engine@@6B@` | `0x180157940` | 12 | **12** | `0x1801579a0` = next table, **no pad** |
| `??_7DialogLoopT@Engine@@6B@` | `0x1801579a0` | 13 | **13** | `0x180157a08` = `??_7Loader@Engine@@6B@` |
| `??_7UpdateLoop@Engine@@6B@` | `0x180155ba0` | 12 | **12** | `0x180155c00` = `"0x%08X\0"` bytes |

All four mint rows are **exact**. But `??_7LoaderLoop`'s justification string is
`adjacent-export:??_7DialogLoopT@Engine@@6B@`, which is an **upper bound read as
exact** — and there is **zero padding** between those two tables, so a swallowed
unexported table would have been invisible to that proof. It is nevertheless 12,
by an independent argument: its base `??_7UpdateLoop` is 12 by a **non-adjacency**
proof (the string tail), and `LoaderLoop` slots 0–11 are `UpdateLoop`'s twelve
roles in order — five purecalls overridden (`0x1800dc260/280/2a0/310/350`) and
five pointers inherited **identically** (`0x180031740/50/70/80/90`). LoaderLoop
introduces no new virtual, so there is nowhere for a hidden table to begin. The
asymmetry that makes the hypothesis testable: a zero pad **does** exist before
LoaderLoop and **does not** before DialogLoopT — this region is not uniformly
padded, so the adjacency is real rather than an artefact.

### 3.2 Readiness (denominator = all slots, purecall included)

| table | slots | ported | TRAP | purecall |
|---|---|---|---|---|
| `??_7UpdateLoop@` | 12 | 1 | 6 | 5 |
| `??_7LoaderLoop@` | 12 | 1 | 11 | 0 |
| `??_7DialogLoopT@` | 13 | 4 | 8 | 1 |
| `??_7DialogLoaderLoopT@` | 13 | 3 | 9 | 1 |

Re-derived by joining `rt_data_mint_tables.inc` against a literal registration
scan, then compared against `perf/mintsel/control/shadow_report.txt` **only
afterwards** — they match, which says the instrument is live rather than that the
number was copied. Note the near-miss: the six `0x180031740..90` addresses DO
appear in `runtime/types/engine/UpdateLoop.h`, as a **comment**, which a
grep-for-the-address readiness check would have scored as registered. They are
traps.

### 3.3 Promotability — a green screen is not a route, and here it is barely even a change

**DLL-present: SAFE and COMPLETELY INERT.** The decisive fact is not the stamper
count; it is that **none of these four tables is ever the live vptr of any object
at dispatch time.** All **7** construction sites in our tree call the ported base
ctor and then immediately overwrite `*(void**)obj` with an exe-side subclass
table we own — `newgamedialog.cpp:864,908,926`, `saveloaddialog.cpp:823,868,893`,
`mainmenudialog.cpp:250,268`, `game_main.cpp:753,846`. The three base ctor bodies
(`loops.cpp:404-465`) dispatch **no virtual on `self`** between our stamp and the
overwrite. So promotion redirects a store whose value is discarded microseconds
later; the 6/11/8/9 traps are unreachable **by construction**, and the retail
stampers keep stamping the DLL table into objects nobody dispatches through.
Census credit only — **it is not a route**, and its safety rests on an
overwrite-after-ctor convention that **nothing enforces**: an eighth construction
site that forgets it makes LoaderLoop's first `Startup` tick an abort.

**At CUTOVER: MANDATORY, opposite sign.** `ResolveExport("??_7UpdateLoop@…")` has
no export table to hit, so `VTableUpdateLoop()` (`loops.cpp:302-316`) returns
**null** and our ctors stamp a null vptr — an EXEC-of-0 with no name on it. A
per-slot trap is strictly better, and the overwrite convention still neutralises
the traps. All retail stampers cease to exist, so ownership goes to 100%.

### 3.4 …and they are NOT promoted. That is a decision.

Promotion touches `standalone/reg_mint_promoted.cpp`, which lanes **GFXVT,
VIDVT, IOPORT and OBJ** are all editing this wave. Weigh that against what
promotion buys DLL-present — by the analysis above, **nothing behavioural** — and
against the fact that this lane's own §2.4 has just demonstrated how an "inert,
obviously safe" claim survives until a control is pointed at it. I released the
game-directory lock and cannot verify a promotion with a run.

So: four rows assessed **READY-TO-PROMOTE**, evidence above, **promoted by no
one in this lane**. Addressee below.

---

## 4. THE THREE DATA ROWS — decided on their own xrefs (rule 6)

### 4.1 `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (`0x1801fe7b8`) — BLOCKED

WRITE: `0x180149006` (`FUN_180148fe0`, the CRT **dynamic initializer**),
`0x18014a594` (its atexit dtor). READ (LEA, address-of): `0x1800dc264`
(`LoaderLoop::Startup`), `0x1800db040` (`DialogLoaderLoopT::Startup`).

`runtime/src/engine/README.md` says the row is deliberately unregistered because
"the loader thread waits on ITS events". **Verified, and the mechanism is more
specific than the README's wording.** `Loader::Load` (`0x1800dc050`) stores the
loop at `this+0x30`, signals **`this+0x28`**, and tail-jumps on **`this+0x18`** —
those handles are **per-object fields, not file-scope statics**, so two `Loader`
objects means the thread blocks on one object's handle forever while `Load`
signals the other's. Refuting the README would have required the handles to be
globals; they are not.

DLL-present: promoting publishes our zeroed 0x38 bytes (null vptr, no thread,
null handles) **and** strands `?Startup@LoaderLoop@` — which is TRAP slot 1 of
the LoaderLoop row, i.e. unported, and reads `0x1801fe7b8` as a baked
RIP-relative LEA. Cutover: `FUN_180148fe0` does not exist, so the storage must be
minted **and constructed**, and nothing in our tree does that.

**BLOCKED-ON-a-ported-static-Loader-constructor** (`FUN_180148fe0`'s `Thread`
ctor + `??_7Loader` stamp, plus `?Startup@LoaderLoop@` `0x1800dc260` and
`?Run@Loader@Engine@@` `0x1800dbfc0`).

### 4.2 `?_singleton@Engine@1@0PEAV11@EA` (`0x1801fe748`) — DO NOT PROMOTE

WRITE (2): `??0Engine@0@` `0x1800dbf9d`, `~Engine` `0x1800dbed4` — **we own
both**. READ (3 functions): `?PlatformConfigure@Engine@1@` (twice, `0x1800a1875`
and `0x1800a18ac`), `?Run@Loader@Engine@@` `0x1800dbfcf`, `?Get@Engine@1@`
`0x18000c7c0`.

**This lane changed that count.** `PlatformConfigure` is now ported and reads the
slot through `RT_DATA_SLOT`, so the unported reader set drops **3 → 2**
(`?Get@Engine@1@`, `?Run@Loader@Engine@@`). Owning every writer is exactly what
makes promotion fatal here: our ported ctor would write *our* storage and
`0x1801fe748` would stay null for anything still on the DLL's side.

**DO NOT PROMOTE** (DLL-present). At cutover: required, but
**BLOCKED-ON `?Get@Engine@1@` + `?Run@Loader@Engine@@`.**

### 4.3 `?_singleton@MainLoop@Engine@@1PEAV12@EA` (`0x1801fe750`) — DO NOT PROMOTE

WRITE: **exactly one**, `0x1800dd755` in the MainLoop ctor, which **is ported**
(`engine.cpp:702`). READ: **18 instructions across 15 DLL functions**, almost all
unported — `DialogLoaderLoopT::Draw`/`Update`, `DialogLoopT::Draw`,
`LoaderLoop::Update`/`Draw`/`Exit`, `MainLoop::OnVideoRestart`/`Draw`/`Get`,
`UI::ShowDialog` ×5, `UpdateLoop::IsRunning`, `?UpdateSave@`, `?Finish@`, and the
two static video thunks. Unpromoted, our single ported writer stores into the
DLL's real slot and all 18 stay correct **by construction**; promoted, the writer
freezes our copy and every one reads null. `?Run@MainLoop@` does not touch the
slot itself, but it still calls the DLL's unported `MainLoop::Frame`, whose
neighbours are those readers — so a promoted row breaks the loop `Run` is
driving, one frame in.

**Highest fan-in of the seven. DO NOT PROMOTE.**

### 4.4 A live defect found in an existing comment — for the integrator

`runtime/src/engine/engine.cpp:173-182` states the protection explicitly: *"If
the registry ever held our own storage for `MainLoop::_singleton`, a ctor that
wrote there would leave the DLL's internal readers looking at a null singleton"*
— and concludes that data globals therefore go through `ResolveExportRaw`, which
is deliberately **not** registry-aware.

**The code does not do that.** `engine.cpp:227-230`, `loops.cpp`'s
`MainLoopSingleton()`, `mainloop_video.cpp:348` and
`userinterface_showdialog.cpp:191` all reach it through `RT_DATA_SLOT` /
`ResolveExport`, which `rt_port.h:192` → `RtResolveCached` makes
**registry-aware**. Only `standalone/rt_orphan_probe.cpp:2511` uses
`ResolveExportRaw`. So the stated guard against exactly this promotion **is not
implemented**, and promoting the row would additionally split our own tree.

This is the same shape as `reference_a_prescription_read_as_a_completion.md`: a
comment describing the fix reads as proof the fix landed. Not repaired by this
lane — the row is DO-NOT-PROMOTE anyway, so repairing it changes no behaviour
today, and it is a `MainLoop` ownership question, not an Engine-rows question.

---

## 5. DENOMINATOR, AND WHAT THIS LANE DID **NOT** CHECK

**Census, BRANCH-LOCAL**, `tools/runtime_census.py` on `wave23/engine`:

| | before (`a60351e3`) | after |
|---|---|---|
| Engine called / ported | 33 / 23 | **33 / 25** |
| TOTAL called / ported | 1314 / 1137 | 1314 / **1139** |
| TOTAL % | 86.5% | **86.7%** |
| gap rows that are a function | 42 of 177 | 40 of 175 |

Checkers: `dispatch_slot_map.py --check` **0 mismatches, 0 double-registrations**.
`export_name_audit.py` — 3 phantoms, **all three pre-existing** and all three in
files this lane never touched (`reg_mint_promoted.cpp` ×2,
`rt_resolve_cache.cpp` ×1); none of the 12 mangled names this lane added is
flagged. Debug build clean.

**NOT checked, stated rather than discovered later:**

1. **No oracle for either body.** Neither has a replaying diff harness. §2.4 is
   explicit that the detoured boot is a liveness and non-crash test only.
2. **No cutover boot.** The wall re-derivation in §1.1 is entirely static. I did
   not run with the DLL absent, so I cannot say what the NEXT wall is — only that
   this one's Half A no longer resolves to null. Someone must run attempt 25.
3. **Half B is unexercised.** `dialog-shown=0` in every run. The launcher path —
   both its DLL-present delegation and its cutover refusal — has never executed.
4. **`?Get@Configuration@System@@`** (`0x18000da70`, 8 bytes) is the other half
   of §2.3 and I did **not** port it. It is trivial and out of my row list.
5. **Out-of-DLL importers** of `?_singleton@Configuration@System@@` — I did not
   run a pefile import scan of the exe or the DX11/DX9 DLLs for that DATA export.
   ERRCFG claims to have; I did not reproduce it.
6. **Route question on already-ported readers**: `filesystem_setupdirs_win.cpp:503`
   holds a `ResolveExportRaw("?_singleton@Configuration@System@@…") // RT_RAW_OK`
   which I did not audit. It does not change the unported-reader set, but a
   promoter should look at it.
7. **Vtable comparisons**: the stamper census finds vptr **stores** only. An
   in-DLL comparison of a vptr against one of these four table addresses would
   be invisible to it. I also verified our 7 construction sites, not the DLL's.
8. **No perf measurement.** The detour adds a call on the loader thread; I did
   not run `sa-stress`, and one run could not resolve it anyway.

---

## 6. RESIDUALS — every one with a NAMED addressee (rule 11)

| # | residual | addressee |
|---|---|---|
| R1 | **The launcher UI.** Half B of PlatformConfigure: port `0x1800a1ae0`, `0x1800a1dc0`, `0x1800a2020`, the four unexported statics, and `0x18000f7d0`; **and** add dialog templates `0x66`/`0x68` and bitmap `0x2711` to the replacement exe's own resource script. This is a LANE, not a task. | **Wave 24 integrator** — scope it as lane LAUNCHER |
| R2 | **Cutover boot attempt 25.** §1.1 is static. Run DLL-absent and record the next wall; that is the only way to learn whether Half A was the last thing between Startup and the next one. | **Wave 23 integrator**, at merge |
| R3 | **Promote the 4 Engine vtables.** Evidence in §3; the only missing step is one green boot after promotion, which I could not take (§3.4). Inert DLL-present, mandatory at cutover. | **Wave 23 integrator**, after GFXVT/VIDVT/IOPORT/OBJ have landed their `reg_mint_promoted.cpp` edits |
| R4 | **Port `?Get@Configuration@System@@`** (`0x18000da70`, 8 bytes, uncalled). With it, the cutover half of the `?_singleton@Configuration@System@@` blocker is fully discharged (§2.3). | **lane ALLOC / the System namespace owner** |
| R5 | **An oracle for `LoadResources`.** The detour gives it a live caller; nothing gives it a comparison. The control in §2.4 is kept in-source for whoever builds one. | **Wave 24 integrator** — pair with R1 or R2 |
| R6 | **`engine.cpp:173-182` describes a guard that is not implemented** (§4.4). Either implement it or delete the paragraph; leaving it is how a prescription gets read as a completion. | **Wave 23 integrator**, hand to whoever owns `MainLoop` next |

---

## 7. FILES

New:
- `runtime/src/engine/engine_platform.cpp`
- `runtime/src/engine/engine_platform_win.cpp`
- `runtime/src/engine/updateloop_loadresources.cpp`
- `standalone/loadresources_detour.cpp`

Modified:
- `runtime/src/engine/rt_engine_port.cpp` — two new switches
  (`BANISHED_RT_ENGINEPLATFORM`, `BANISHED_RT_ENGINELOADRES`) and their lines
- `runtime/types/engine/Engine.h` — the `0x1800f6d10` correction (§1.6)
- `standalone/main.cpp` — two summary calls beside `RtEngineLifecycleSummary()`

New env vars: `BANISHED_RT_ENGINEPLATFORM=0`, `BANISHED_RT_ENGINELOADRES=0`,
`BANISHED_RT_ENGINELOADRES_DETOUR=1` (opt-in),
`BANISHED_LOADRES_NEGCONTROL=1` (the §2.4 control).

Game-directory lock: acquired **bare** as `ENGINE`, `assert-held` before each
deploy, four runs, **released**.
