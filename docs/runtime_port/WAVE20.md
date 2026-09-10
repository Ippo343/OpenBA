# WAVE 20 — the plan as dispatched

**Opened 2026-09-10 by the integrator, cut from `972080e` (= Wave 19 close tip).**
Branch `wave20/base`. Lane branches `wave20/<lane>`, each in a **private worktree**
under `C:/code/GIT/wave20/<lane>`.

Read **[WAVE20_GUIDE.md](WAVE20_GUIDE.md)** first — it is the written handover from
the Wave 19 integrator and this document does not repeat it. Read
**[WAVE19_CLOSED.md](WAVE19_CLOSED.md)** §2, §4, §7.1–§7.6.

---

## 0. Structure of the wave

The guide's §0 is obeyed literally: **Phase 0 fixes the instruments before any lane
is scoped.** Two of the three known-broken instruments would silently mis-scope a
Wave 20 lane, and the census one under-reports DATA work by ~4x. Phase 1 is not
dispatched until Phase 0's numbers are in.

| phase | lane | subject | needs game dir |
|---|---|---|---|
| 0 | **CENSUS** | `runtime_census.py` blind to minted-vtable slot bodies (guide §0a) | no |
| 0 | **TOOLS** | `boot_state.sh` stale-file RUNNING (§0b) + 14 duplicate registrations (§0c) | only if building/running |
| 0 | **EXTENT** | re-derive 31 `adjacent-export` slot counts from the stamping `LEA` (§0d) | no |
| 1 | *(scoped after Phase 0 reports)* | Candidate A: **PROMOTION** — 5 of 62 vtables promoted, 23 complete tables minted and unregistered | yes |

Phase 0 was chosen to be **game-directory-free by construction**, so the three cells
run concurrently with no lock contention. Wave 19 lost 24 minutes to exactly that
contention (§4.3).

---

## 1. Integrator's own verifications, run before relaying anything

The standing rule from Wave 19 §7 is that **a lane's negative claim about the
environment cannot be checked by reading the lane's work — only by running a command
the integrator can always run**, and that relaying one is the single highest-risk
output of this structure. So these were run first, on the wave tip, by me:

### 1.1 `dup_runtime_export.py` — CONFIRMED, exit 3, and the guide's count needs a caveat

Run in the tree being measured, **exit code read directly and not through a pipe**:

```
rc=3
scanned 4596 files under C:\code\GIT\untitled_rev_eng_project
1119 distinct RegisterRuntimeExport names, 7 duplicated
1158 distinct RegisterRuntimeAddr addresses, 7 duplicated
SUBJECT: RegisterRuntimeExport/RegisterRuntimeAddr ONLY. This does NOT cover
         RegisterDispatchSlot (see dispatch_slot_map.py --check) nor RegisterExeAddr.
```

**The guide's "14 duplicate registrations" is `7 names + 7 addresses`, which are
overlapping views of the same defects, not 14 independent sites.** Lane TOOLS is
tasked to triage what is actually there rather than to reach a number.

Note the tool prints its own SUBJECT line and its own denominators — the Wave 19 fix
working as intended, and the reason this check could be quoted at all.

### 1.2 SAVE GAMES EXIST — 13 of them — and NOT where the game directory suggests

Correction C14 was the integrator relaying *"no save games at all"* into a merge
commit when **13 existed**, costing a lane its verification route. Re-run here before
any lane is told anything about the save route:

```
C:/GOG Games/Banished/Save        -> does not exist; 0 *.sav anywhere under the game dir
C:/Users/USPC/Documents/Banished/Save -> 13 *.sav
    Auto.sav  Auto2.sav  Save0.sav .. Save10.sav
```

> **The saves are under `%USERPROFILE%\Documents\Banished\Save`. A check aimed at the
> game directory finds zero and is CORRECT ABOUT THE WRONG DIRECTORY** — the
> `--root`/`--namespace` failure shape of §7.1, in the one place that has already cost
> this project a lane's route once.

This is a live input to the guide's §3 save/load route cell: `?GetSaveFilename@`
builds `<savePath>Save\<name>.sav`, and **`<savePath>` is the Documents path, not the
game path.** Any lane scoped on that cell gets this fact in its brief.

---

## 2. What Phase 1 will be scoped from — NOT yet decided

Deliberately left open. The guide's §5 item 1 is that scoping a DATA lane before §0a
lands under-reports it by ~4x, and its §0d is that no DATA lane may be scoped against
a slot count not re-derived from a ctor's own `LEA`. **Both are Phase 0 outputs.**
Phase 1 is scoped when they land, from measured numbers, and the set-diff of what
entered `called` is reported rather than a percentage delta.

The standing recommendation being carried in is the guide's §2: **Candidate A
(promotion) first, Candidate B (screenshot cutover, 18/25 SHUT) second — A closes 3
of B's 7 open doors — and Candidate C (IO's last 12) only if a lane is free.**

---

## 3. Standing rules issued to every lane at dispatch

Copied into each brief rather than referenced, because a referenced rule is one a
lane can be measured as never having read:

* Private worktree; report path, branch and commit in every update. **Stop editing
  once you report** — a lane that keeps improving after reporting invalidates the
  integrator's build.
* `git commit -F <msgfile>`. **Never** inline shell quoting; backticks have executed
  a snippet inside a commit message here and left a fabricated number.
* **Never read an exit code through a pipe.**
* Run the instrument **in the tree you are measuring**; print the resolved root.
* **Quote every green with its denominator.** Every screen needs a **positive control
  that must read non-zero**, scored on **applicable cases only**.
* An instrument that names its subject only **after** its findings has not named it.
* **Zero free parameters buys unaimability, not soundness.** Remove the parameter;
  failing that, print it beside the verdict.
* **A stated blocker needs its PREMISE as a separate checkable line and an EXPIRY
  CLAUSE, at seal time.** "Needs X" is a TODO, not a blocker. Naming *more* conditions
  makes an impossibility claim **weaker**. Two lanes self-imposed an expiry exactly
  zero times in Wave 19 until it was demanded — so I demand it at dispatch.
* Game directory is **serialised wave-wide** via `tools/game_dir_lock.sh`; never pipe
  `acquire` (a timeout then reads as a grant); **a crashed run is not a finished run** —
  confirm the process EXITED and that the exe and **both** logs are free.
* Don't "fix" a faithful crash with a null check. Find the upstream null.
* **Grep before you port**, in three forms: mangled name, retail VA, and the
  `RtIo_<Class>_<Method>` convention. And **grep `standalone/` and the `BANISHED_*`
  list before writing "no route"** — that rule has paid out four times in two waves.
