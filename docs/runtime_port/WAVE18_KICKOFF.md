# WAVE 18 — KICKOFF: drive Video to its measured ceiling

**Written 2026-09-10 by the advisor at `dacc015`. Dispatched.**
Read `WAVE18_VIDEO.md` first (the plan) and `WAVE17_LANE_CEIL.md` §3 (the
denominator). This document does one thing the plan does not: it **names all 23
rows** and partitions them into lanes.

## 0. A scope correction, stated up front

`WAVE18_VIDEO.md` scopes Wave 18 as a **cutover** wave — six ctor/dtor bodies,
aimed at the boot wall — and says explicitly *"do not scope this wave against
Video's percentage."*

**This wave is scoped against Video's percentage anyway, deliberately.** The
objective is: **Video 72.7% → 88.0%, its measured ceiling.**

That is not a contradiction of the plan, it is a **superset of it**. The six
blocker bodies are 6 of the 23 rows, and they stay lane 1 with first priority
precisely because they are also the boot wall. The plan's warning was written
before lane CEIL had measured anything; CEIL then showed Video's real gap is 23
rows and not 41, which makes "drive it to ceiling" a bounded, scopeable target
rather than the open-ended census chase the plan was right to refuse.

**What the plan's warning still buys, and it is kept:** *do not follow this wave
with more Video.* At ceiling, Video is finished. IO (gap 34) and Graphics (gap
29) are the larger real work and are the next wave's subject, not Video's
remainder.

## 1. The 23 rows, named

`tools/runtime_census.py --namespace Video --unported` on `dacc015`:
41 unported, 18 tagged UNPROMOTABLE by CEIL (12 NO-OP + 6 HARMFUL), **23 left.**
Do not touch the 18 — four of Video's six HARMFUL rows are *deliberately* on the
DLL and promoting one introduces a defect (CEIL §3.2).

### Tranche A — ctor/dtor (10)

| # | symbol | note |
|---|---|---|
| 1 | `??0GraphicsInterface@Device@Video@@QEAA@XZ` | **boot wall** |
| 2 | `??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z` | **boot wall** |
| 3 | `??1GraphicsInterface@Device@Video@@QEAA@XZ` | **boot wall** |
| 4 | `??0Adapter@Device@Video@@QEAA@XZ` | **boot wall** |
| 5 | `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | **boot wall** |
| 6 | `??1Adapter@Device@Video@@QEAA@XZ` | **boot wall** |
| 7 | `??0RenderBuffer@Video@@QEAA@XZ` | |
| 8 | `??1RenderBuffer@Video@@QEAA@XZ` | |
| 9 | `??1Configuration@Video@@QEAA@XZ` | |
| 10 | `??1Device@Video@@QEAA@XZ` | |

### Tranche B — CODE (6)

| symbol |
|---|
| `?AdjustDisplay@Device@Video@@QEAAXXZ` |
| `?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z` |
| `?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z` |
| `?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z` |
| `?Recreate@ImageBuffer@Video@@QEAAXXZ` |
| `?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z` |

### Tranche C — DATA (7)

| symbol | CEIL verdict |
|---|---|
| `??_7ImageBuffer@Video@@6B@` | BLOCKED — 10 slots / 3 ported / **7 TRAP** |
| `?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A` | BLOCKED on attendance |
| `?_typeInfo@PixelProgram@Video@@…` | " |
| `?_typeInfo@RenderState@Video@@…` | " |
| `?_typeInfo@VertexLayout@Video@@…` | " |
| `?_typeInfo@VertexProgram@Video@@…` | " |
| `?_typeInfo@VideoProgram@Video@@…` | " |

The six typeinfos have **exactly one consumer in the tree**: the ten-element
`ids[]` at `runtime/src/engine/mainloop_video.cpp:487-499` inside
`RtEngine_MainLoop_OnVideoError` — device-lost recovery, which no harness route
drives. CEIL §5.4: one synthesised `OnVideoError` call decides all ten (six
Video, four Graphics) in a single run. Readiness and value evidence for all ten
is already green; **the factory-trap hazard does not apply** (CEIL §2.6, and it
is structural, so it exonerates every `?_typeInfo@` row).

**10 + 6 + 7 = 23.** Arithmetic checked; it reproduces CEIL's headline exactly.

## 2. Lanes

Three port lanes in **private worktrees, already created** — plus GATE, which
runs after GFXIF merges.

| lane | worktree | branch | rows |
|---|---|---|---|
| **GFXIF** | `C:/code/GIT/wt-w18-gfxif` | `wave18/gfxif` | 9 — the 6 blockers + AdjustDisplay + FindOrAddMode + GetDisplayMode |
| **VRES** | `C:/code/GIT/wt-w18-vres` | `wave18/vres` | 7 — RenderBuffer ctor/dtor/Bind, ImageBuffer Recreate/SaveImageToFile, `??1Device`, `??1Configuration` |
| **VDATA** | `C:/code/GIT/wt-w18-vdata` | `wave18/vdata` | 7 — the `??_7ImageBuffer` traps + the 6 OnVideoError typeinfos |
| **GATE** | (later) | — | 0 rows; re-run the cutover boot gate, **set diff** vs attempt 24's 17 |

GFXIF's nine are one cohesive cluster: Adapter/GraphicsInterface are the
enumeration value types, and AdjustDisplay/FindOrAddMode/GetDisplayMode are the
`Device` methods that build and read them. Splitting them would put two lanes in
the same three files.

**GATE does not run until GFXIF is merged.** Its job is to say whether the wall
moved and to where — not to port. It must carry attempt 24's own warning: a
cutover boot is a **GATE, not a PLANNER**; its residue is a lower bound on the
remaining work, never the work list.

## 3. The shared-resource constraint

Source is isolated by worktree. **The game directory is not.** One
`C:/GOG Games/Banished`, one `reimpl-x64.exe`, one set of logs.

* `tools/game_dir_lock.sh acquire <lane>` — **never piped.** A pipeline exits
  with the *last* command's status, so piping the acquire into `tail` makes a
  TIMEOUT read as a GRANT. One lane deployed over another mid-run that way.
* Check its **own** exit status, and gate every deploy on
  `assert-held <lane>`.
* **Never background a blocking acquire** — the lock is held from the moment it
  is granted, not when you notice; that cost 19 idle minutes of a wave's
  critical path.
* Hold it for a **run batch**, not a single run, and release promptly.

Three lanes contending for one lock is the real throughput ceiling of this wave.
Do the reading, the disassembly and the porting first; batch the runs.

## 4. Doctrine — restated because each item has cost this project a wave

1. **Prose is a hypothesis; a grep is the answer.** Before writing a body, grep
   the mangled name AND the retail address across `code/ engine/ runtime/
   standalone/ dx11/`. Three times in one wave a lane ported what the tree
   already had. A duplicate registration is last-binding-wins, so it silently
   makes one body dead code.
2. **A body ported but never registered is indistinguishable from an absent one**
   to the census. Grep BOTH the address and the mangled name, by symbol not by
   class string.
3. **A census `unported` row has four meanings** — absent / gated / unregistered
   / storage-never-written. They imply completely different work. Decide which
   before planning.
4. **`AEAA` means PRIVATE, not unexported.** This DLL exports its private
   members. An address door justified by the word "unexported" in a comment is
   usually a plain name door. The export table settles it in seconds.
5. **Register by name AND address.** 13 live sites reach Runtime symbols through
   `ResolveAddr(Module::Runtime, …)`, which is registry-first —
   `ResolveAddr(Module::Runtime, addr)` is NOT "the DLL".
6. **`ResolveExport` falls through to the raw impl for an unported symbol,
   silently**, and a name the DLL does not contain returns NULL silently.
7. **Read the disassembly, not Ghidra's C.** Ghidra's decompiled C drops
   dereferences, prints MOVSXD as a zero extension, and drops whole arguments at
   a mistyped vtable call — four defect classes, two crashes, in one lane.
8. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
9. **An instrument outside its subject's live window prints a plausible ZERO,
   not an error.** Every screen needs a positive control that MUST read
   non-zero, verified to print before any zero is trusted.
10. **A control must be scored separately** with its own catch rate, must
    produce a comparable divergence rather than UB, and must catch **via the
    mechanism it names**. A control scored on the union of two screens hides a
    blind one.
11. **`calls=0` has four causes and they invert the conclusion.** Read the
    oracle's `calls=` column, not just `divergences=`; and `calls=0` with
    `worked>0` means the body ran every time and always took the branch the
    oracle does not cover.
12. **`BANISHED_AUTOQUIT` overrides `BANISHED_AUTOSTART` and `BANISHED_AUTOLOAD`**
    — it replaces them, it does not compose. `[AutoStart]` lines go to **stderr**,
    not the debug log.
13. **The mods-menu route is broken on this tip** (`ModDialog_ToggleMod_Impl+0x43`,
    read of null+0xd0). Do not plan around it. Evidence:
    `docs/runtime_port/wave17_modsroute_crash.txt`.
14. Build **both** `standalone-debug` and `standalone-perf`;
    `make check-log-strip` fails the stripped builds on qualified `std::printf`.
15. **Commit early and often.** A machine restart destroyed one lane's entire
    uncommitted session last series.
16. **No heredocs** for file content or commit messages — a heredoc silently
    rewrites `\b` to a 0x08 byte, splits string literals on `\n`, and runs
    backtick command substitution. Use `Write` + `git commit -F`.
17. **Report your worktree PATH and branch name** in every status update, and
    **stop editing once you report** — a lane that keeps improving after it
    reports invalidates the integrator's measurement.

## 5. What "done" looks like

* Census reads **Video 88.0%, AT CEILING** with no row left untriaged.
* Every ported row has a **verdict with evidence**, not a registration. An
  UNTRIAGED caller-less port is not evidence of anything; it is work nobody has
  done.
* A row a lane cannot promote gets a **written verdict with a class (NO-OP /
  HARMFUL / BLOCKED) and an expiry condition** — and **BLOCKED is not a
  ceiling.** CEIL predicted 40 NO-OPs and measured 12 because it asked whether
  promotion would *matter* before asking whether it was *possible*.
* GATE reports the new wall by **set diff**, and scopes the next wave with three
  separate bounded screens (predicted walls = upper bound, residue = lower
  bound, census gap = lossy both ways).
