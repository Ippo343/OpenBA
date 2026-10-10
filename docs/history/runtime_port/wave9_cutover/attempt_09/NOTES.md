# Cutover boot attempt 09 — 2026-09-08

Tip with lane ENG round 2 (`cd32b0d`) merged. Two runs: DLL-present (Q1) and
DLL-absent (Q3/Q4).

## Every falsifiable prediction lane ENG made was confirmed — second iteration running

**Q1 — the divide-by-zero regression is gone.** DLL-present, `BANISHED_RT_ENGINELIFE`
**default-on**: `exit=0`, `[Standalone] === GameMain returned 0 ===`, zero-byte
crash log.

**Q4 — the next fault, named in advance and correct in every part:**

```
[RtUnresolved] ?SetupDirectories@FileSystem@IO@@QEAAXXZ  (asked by 00007ff6263f8405)
Fault: EXEC of 0000000000000000
  #1  +0x2D6B3A  ->  RtIo_FileSystem_Startup   +0x55
  #2  +0x2BB67E  ->  RtEngine_Engine_Startup   +0x22c
  #3  +0x19B9CD  ->  Game::RunGameMain         +0x25d
```

It predicted the symbol, the EXEC-of-0, the enclosing function, **and that the
frame would move one level DOWN — which it nominated as the check that the
`FileSystem::Startup` row had landed.** It did. `Engine::Startup` also advanced
within itself, `+0x17a` → `+0x22c`.

Two consecutive iterations now with a predictive model of the boot that holds in
detail. That is what has made each iteration cost one run instead of a bisection.

## My own diagnosis of the regression was CLOSE AND WRONG, and the correction is the finding

I recorded: *"our port replaces the shipped `Engine::Startup` and does not call
`Timer::PlatformStartup`."* **It does call it, and all five of our Timer values
were correct.** `?Startup@Timer@System@@` resolves to lane B3's port, which by
deliberate design **owns its own five statics rather than publishing them into the
exported slot**. So the DLL's `0x1801fe730/738/740` stayed zero and
`MainLoop::Run` — still the shipped body — divided by one of them.

> **A ported function that OWNS storage instead of PUBLISHING it into the exported
> slot is invisible until an UNPORTED CONSUMER reads the copy it never wrote.**

This is a failure shape none of our instruments can see. The mint, the census and
every door audit would call lane B3's `Timer` correct — *because it is*. The
defect exists only in the **seam between a ported writer and an unported reader**,
and it disappears once both sides are ported, which is exactly why it had to
surface now rather than later.

Fixed in `timer.cpp` (`PublishTimerStatics`, five `RT_DATA_SLOT` mirrors), not in
`Engine::Startup`, so every future caller inherits it. A bounded sweep of the same
class across all seven ported callees of `Engine::Startup` found **exactly one**
case.

## Both "(?) no writer found" rows are now closed

`?_systemInitialTick` ← `Timer::PlatformStartup`'s tail `QueryPerformanceCounter`.
`?_errorString` ← `?SetText@Error@System@@` (0x1800c5fa0, ord 3982), which
**APPENDS** — it walks to the NUL and concatenates, which is why a store scan
attributed nothing to it. `game_main.cpp:482` reads that buffer to report why
`Startup` failed, so "leave it zeroed" silences our own diagnostic. Correctly not
ported this round: failure arm only, so by the control-flow-reach rule it buys
nothing this iteration.

## One condition attached to the SetCommandLine row, which matters

`SetCommandLine` closes **conditionally**. Its three callees are unported, but
`main.cpp:1930` passes `""` and retail's own `JLE` at `0x1800c47d7` returns before
the loop. **Pass any argument and the next fault is `?ParseCommand@`.** The loop
body is written out faithfully regardless.

## The wall now

`?SetupDirectories@FileSystem@IO@@QEAAXXZ` (0x1800abd00, ord 4029) — **722
instructions, thirteen distinct callees, twelve unported.** Lane ENG's words:
*"That is a lane, not a tail-end."*
