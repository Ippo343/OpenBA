# Lane GFX's verification cell — 2026-09-08, integrator

`BANISHED_RT_DIFF=1` gameplay cell on the fully merged tip (all five lanes).
**Run with `BANISHED_RT_ENGINELIFE=0`**, because with lane ENG's port default-on
this cell now crashes — see the regression section below. Isolating the
regression is what let lane GFX be checked at all today.

```
[RtDiff] ==== 359 sites, 31031532 calls, 0 divergences in 0 sites ====
```

## Lane GFX's predictions, scored

It stated its own failure condition, which is what made scoring possible:
*"with the DLL present, both Lerps should show `divergences=0, calls=` in the
millions; **`calls=0` on either is a failure, not a pass**."*

| row | calls | worked | divergences |
|---|---|---|---|
| `Graphics::InterpolatedState<Transform>::Lerp` | 200,876 | — | 0 |
| `Math::Transform::Lerp` | 200,876 | — | 0 |
| `Math::Quaternion::Lerp` | 200,876 | — | 0 |
| `Graphics::Renderer::AddDrawItem` | 200,876 | 200,876 | 0 |
| `Utility::PerlinNoise::Generate` | 2,220,721 | — | 0 |
| `Graphics::MaterialInstance::RenderTextures` | 0 | 15,515 | 0 |

**PASS on the binding criterion.** Both `Lerp` rows are exercised and bit-exact,
so the moved-term class of defect that this wave found in `Picking::Scene::
Intersect` is not present here. "Millions" overshot — 200,876 — but the lane
bound itself to `calls=0`, not to a magnitude, and it is the binding form that
counts.

**REFUTED sub-prediction, recorded rather than rounded off.** It predicted
`AddDrawItem` would show *"`work=` non-zero but **strictly less than** `calls=`"*.
Observed: **`worked=200876`, exactly equal to `calls=200876`**. Every call did
work. The port is not implicated — `divergences=0` over 200,876 diffed calls —
but the lane's model of the function has an early-out in it that the function
does not take, at least on this path. Whoever next touches `AddDrawItem` should
not inherit the belief that it skips work.

`RenderTextures` shows the now-familiar `calls=0 / worked=15,515` shape: a
no-oracle port, structurally empty diff column, evidence in `worked=`.

## The regression this cell also found — attributed by A/B, not by inference

With lane ENG's port default-on, the DLL-present cell dies:

```
Exception: 0xC0000094 (INT_DIVIDE_BY_ZERO)
Module: Runtime-x64.dll + 0xDD3F9        ->  MainLoop::Run (0x1800dd3c0) +0x39
```

```
1800dd3e8: SUB  RAX,[0x1801fe738]     ; Timer::_systemInitialTick
1800dd3ef: IMUL RAX,[0x1801fe720]     ; Timer::_ticksPerSecond  (1000000)
1800dd3f9: IDIV qword ptr [0x1801fe730] ; Timer::_systemTicksPerSecond (QPF) <- ZERO
```

`BANISHED_RT_ENGINELIFE=0` → exit 0, `GameMain returned 0`. Default-on → the
divide. One run each; the kill switch is what made it a one-run diagnosis.

Retail's `Engine::Startup` calls `Timer::PlatformStartup`, which writes
`_systemTicksPerSecond` and `_systemInitialTick`. **Our port replaces the shipped
function and does not**, so both stay at the mint's zero.

**Two things follow that are larger than the regression.**

1. **This answers a "no writer found" row.** WAVE9.md's storage-writer table
   classifies exactly two symbols as *"(?) no writer found at all"* —
   `?_errorString` and `?_systemInitialTick`. The writer is `Timer::
   PlatformStartup`, reached from `Engine::Startup`. Nothing could have found it
   until something ported `Engine::Startup` and then divided by the result.
2. **It is not a DLL-present-only problem.** At cutover the same `IDIV` runs
   against our own minted storage, zero for the same reason, so the boot divides
   by zero the moment it reaches the main loop.

The general rule, which is the reason this is written down: **a port that
replaces a shipped function must reproduce its SIDE EFFECTS, not just its return
value — including side effects nobody has attributed to it.** A global written by
nobody is invisible until something divides by it.
