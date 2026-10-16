# DEVLOSTMEM -- the post-device-loss memory climb, known_port_issues row 1329 (hand-in, 2026-10-16)

- **Branch:** `worktree-agent-a8d982b3db8c5fa9d`, on main `dd1e71cb2`. Nothing is merged.
- **Verdict:** **not a port defect, and not the device-lost route's.** The climb is the NVIDIA user-mode
  driver (`nvwgf2umx.dll`) allocating its own internal objects in the process heap while it draws.
  It happens without any device loss. There is **no `fix:` commit**: nothing in our tree allocates the
  memory, so there is nothing to fix at the root on our side.
- **Exe:** the debug exe (`tools/build.sh build debug --jobs 2`) with temporary stderr instruments,
  all removed. Every run held the lock `devlostmem`, had the deployed exe checked with
  `assert-held`, and had no `*.pkm` mounted. Exception: `before4` ran another lane's exe and is
  left out of the evidence below.
- **settings.cfg:** the run script backed the file up before each run and restored it byte-identical
  afterwards (md5 `f7fad13a…` for the early runs). Between my runs another party changed
  `VideoSettings=true` to `false` (md5 `6392ed7f…`), and I left that change as I found it.
- **Route:** AOTUNE §2.3's `devlost_off`: `BANISHED_AUTOSTART=1 BANISHED_FORCE_DEVICE_LOST=12000
  BANISHED_PAUSEMENU=40000`, `ext.ambient_occlusion=0`, the user's profile (2560x1377, 4x MSAA,
  every other row on). Most runs added `BANISHED_SEED=823186202`. Controls drop the loss.
- **Sampler:** private bytes, working set and the four `\GPU Process Memory(pid_*)\*` counters every
  10 s for 4-5 min (scratch `build/dlm/sample.ps1`; the per-run logs are in the ignored `build/dlm/runs/`).

## 1. Reproduced, and the control climbs too

"Climb" runs from the first sample after the trip (about 35 s) to the end.

| Run | Loss | Seed | Private after the trip -> end | Notes |
|---|---|---|---|---|
| `before1` | yes | random | 1301 -> 1621 MB (**+320**) | `BANISHED_ALLOC_STATS=1` |
| `before2` | yes | random | 1085 -> 1125 (+40) | another world |
| `before3` | yes | 823186202 | 1272 -> 1548 (**+276**) | |
| `control3` | **no** | 823186202 | 887 -> 1084 (**+197**) | one step of +177 at 90-113 s |
| `before5` | yes | 823186202 | 1085 -> 1271 (**+186**) | |
| `control4`, `control5` | no | 823186202 | 721 -> 718 (flat) | ~4000 fps |
| `before6`, `before7`, `cnt_devlost`, `fp_devlost` | yes | 823186202 | +0..+20 (flat) | |
| `pan_control`, `pan_devlost` | no / yes | 823186202 | +21 / +70 | a scripted camera pan |

1. **It is not the device-lost route's.** `control3` has no loss and climbs +197 MB in the same steps.
2. **Whether a run climbs does not depend on what we control.** With the same exe, seed and
   settings, 4 runs climbed and 7 stayed flat. Most flat runs drew at about 4000 fps against about
   1000 fps for the climbing ones, so what the driver ends up drawing differs between runs. My
   guess is the window's visibility or focus, or the mouse, but I did not pin this down.
3. **GPU dedicated memory barely moves** (+3..+16 MB) while private bytes climb.

## 2. Where the bytes go

In-process report every 5 s (temporary). It covers four things: `K32GetProcessMemoryInfo`, the
engine arena's committed bytes (`MemoryAllocator::ranges[].committed`), `HeapSummary` over
`GetProcessHeaps()`, and a `VirtualQuery` walk.

- **The engine arena is flat.** In `before1` (`ALLOC_STATS`) the live bytes read 158 MB for the whole
  window, and in `before5` the arena read 162 MB committed.
- **The process heap carries the climb.** In `before5`, heap 0 (the process heap) went
  115 -> 284 MB while private bytes went 1085 -> 1271 MB.
- **We create no GPU objects after the trip.** Every `CreateBuffer` / `CreateTexture2D` was counted:
  2330 buffers and 819 textures, all at boot and the trip, 0 afterwards. No `Create` landed over a
  live buffer.
- **No resource is used across devices.** A `GetDevice` check on every buffer / texture bind,
  `Map` and `UpdateSubresource` found 0 objects of device 0 used on device 1.
- **SORTMEMO is not it** (6 entries, at most 1 MB).
- **The growing blocks belong to the driver.** A `HeapWalk` histogram of busy blocks shows what
  grows: 5944-, 1680- and 1552-byte blocks in equal counts (440 -> 3596 in `pan_devlost`), plus
  1560, 1048 and 272. Each block's second qword points into `nvwgf2umx.dll+0x100140` (a vtable).
  The 5944-byte blocks also carry a 64-bit hash at +0x18 (for example `9824218006268a23`): they
  are the NVIDIA D3D11 user-mode driver's internal, hash-keyed objects. Most likely they are its
  pipeline / state-combination cache, which grows as new draw-state combinations are first drawn.

**Cause:** the NVIDIA D3D11 user-mode driver allocating its internal per-device state objects on
the process heap as new draw states are first drawn.

- **Where the device loss comes in:** a loss adds only the trip step. The new device starts with an
  empty cache and refills it, while device 0, whose refcount after the trip reads **8** (retail
  nulls `d3dDevice` before Shutdown, DEVLOST.md §5), keeps its own.
- **What our code does:** no port code allocates this memory or holds it.
- **Its rhythm** is the rhythm at which new combinations come into view, so it is not periodic.

## 3. Why DEVLOST1 read flat

DEVLOST1 (2026-10-11) sampled the **working set** ("~315 MB"), not private bytes, on a smaller
profile. Private bytes sit about 590 MB above the working set here (`before5`: 1271 against 685 MB).
When the driver grows, the working set grows too (`before1`: 690 -> 969 MB), so the counter alone
does not explain the flat reading. What does is §1: the climb is absent in about two runs out of
three even on the same profile, and a smaller window draws less new state. One flat 220 s window is
no evidence that the code changed. Nothing later in the tree exposed it.

## 4. Before / after

There is no fix, so there is no "after". On the same route and sampler, the numbers are §1's: a climb
of +186..+320 MB in the runs where the driver draws new state, and +0..+40 MB in the runs where it
does not, with the same exe. GPU dedicated memory reads 721..762 MB after the trip in every run.

## 5. Ruled out, with the evidence

| Candidate | Evidence |
|---|---|
| The minimap / terrain re-upload after `Recreate` (AOTUNE's candidate) | 0 `CreateTexture2D` / `CreateBuffer` after the trip; `Update` is `UpdateSubresource` / `Map` on existing resources |
| A device-0 resource bound or updated on device 1 | `GetDevice` check on every bind, `Map` and `UpdateSubresource`: 0 hits |
| Engine heap (MemAlloc) growth or fragmentation | arena live and committed flat after the trip |
| SORTMEMO / ext caches in the CRT heap | memo 6 entries, at most 1 MB; the growing heap blocks carry nvwgf2umx pointers, not ours |
| Device-lost specific | `control3` (no loss) climbs +197 MB |

## 6. Left open

- **Retail is not measured.** It uses the same driver, so the same growth is expected. Retail's
  device-0 retention (DEVLOST.md §5) is unchanged.
- **The bound of the driver cache is not measured.** No run here went past 5 min.
