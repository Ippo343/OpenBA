# The DLL-present diff cell for lanes Y and Z — 2026-09-08, integrator

`BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900
BANISHED_PAUSEMENU_QUIT=120`, clean self-quit, exit 0. Exe built 03:56 from the
tip with both lanes merged.

## Headline

```
[RtDiff] ==== 351 sites, 29689833 calls, 0 divergences in 0 sites ====
[RtDiff] ==== did-work: 1045 calls did work across 16 declaring sites;
         335 sites have NOT declared (their coverage is UNKNOWN, not zero) ====
[RtResolve] 0 misses
```

## Lane Y's cell 1 — PASSES, on the exact line it nominated

```
[CheckPtr] export=00007ff6bcea30ea body[0]=0x55 source=our port -> NOP (calls skipped)
```

Lane Y stated the falsifiable form itself: *"it must say `source="our port"` /
`NOP (calls skipped)`. If it says `real call`, my fix is inert and the 60k calls
are back."* It says our port. The detector now recognises our own empty body, so
registering `CheckPtr` is cost-neutral with the DLL present, and the two-wave-old
"registering costs 60k indirect calls/frame" is dead as stated. **The PERF half
of that claim is still argued and not measured** — the A/B lane Y asked for
(three unset-vs-`=0` runs on the debug exe, not the perf exe, which compiles the
sites out) has not been run.

## Lane Z's four `RT_DIFF_WORK` counters — EXERCISED, and `calls=0` is CORRECT here

```
UI::UserInterface::AddDialog      calls=0   worked=6      divergences=0
UI::UserInterface::SetCursor      calls=0   worked=1024   divergences=0
UI::UserInterface::EnableCursor   calls=0   worked=2      divergences=0
```

**`calls=0` on these three is the expected reading, not the alarm.** They are
deliberate NO-ORACLE ports that never call the shipped body, so the *diffed*
call count is zero by construction and `worked=` carries the evidence. This is
the inverse of the standing rule "read the `calls=` column, not just
`divergences=`" — the rule exists because a port with no callers reports clean,
and here the port has callers and the diff column is structurally empty. Reading
`calls=0` as "unexercised" would have been exactly wrong, and it is one grep
away from looking like three dead ports.

## What this cell CANNOT answer — five ports have no diff site at all

`UserInterface::Play`, `Dialog::Persist`, `Dialog::OnSerializeSettings`,
`Dialog::TypeName` and `External::Compile` appear **nowhere** in the 351-site
list. They are registered ports carrying no instrumentation, so this run says
nothing whatever about them — neither that they ran nor that they did not.

Lane Y asked specifically to *"confirm `External::Compile` has non-zero
`calls=`"*. **That confirmation is not available from this cell**, and the
reason is not that the port is dead: there is no site to report. Recorded as
UNKNOWN rather than allowed to read as either outcome, because a
missing row and a zero row look identical to a reader who is scanning.
