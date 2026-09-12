# CUTOVER ATTEMPT 26 — the wall moved, and the manifest worked

Run by the integrator on the merged Wave 25 tip, after all eight lanes.
`tools/cutover_attempt26.sh`, exe md5 `6007cf98576b57a65f070ed8c6b095e0`.
Runtime-x64.dll moved aside, **restored and verified present (2,762,752 bytes)** by the
script's unconditional EXIT trap.

## 1. THE WALL, NAMED

```
[RtMint] FATAL: minted BaseTypeInfo::factoryFn is not ported
[RtMint]   vtable : ?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A
[RtMint]   slot   : 0
[RtMint]   target : ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
```

**This is not an EXEC-of-0.** It is the mint's own per-slot trap firing and **naming itself** —
the thing those traps were built for. `standalone_crash.log` is 0 bytes because nothing
crashed: the process failed fast, deliberately, with the symbol printed.

**The wall MOVED, and by a long way.** Attempt 25 died in audio at
`??0Stream@Audio@@QEAA@XZ`, called from our own `Audio::Device` ctor. Attempt 26 shows
**zero references to `Stream@Audio` in the error log** — Wave 24 lane SMALLBODY's port of that
ctor (and its conversion of retail's RIP-relative LEA stamp for `??_7Stream@Audio@@6B@` into a
real door) carried the boot past it.

Boot now reaches, in order: Engine::Startup complete · MainLoop constructed · Game ctor done
(`t=569ms`) · KeyMap singleton · InputConfig → KeyMap · **50 default input bindings populated
and Load applied** · `StringTable typeId = 3224705508` → trap.

That is deep into game initialisation, well past the video and audio subsystems that stopped
attempts 23, 24 and 25.

## 2. THE MANIFEST WORKED, FIRST TIME, WITH NO HAND-EDITING

Lane MANIFEST's generator ran at cutover and **detected DLL absence itself** — no new env var,
exactly as designed:

```
[CutoverManifest] ==== DLL ABSENT (cutover) ==== 46 tagged gap rows
[CutoverManifest] published 44, withheld 1, conditional 1, unminted 0, failed 0 (of 46)
```

Those are **precisely the numbers MANIFEST projected while it was still writing the thing**,
and they held after lane DATA added 14 more tagged rows *after* MANIFEST had finished
measuring — which is what the generator (rather than a list) was for.

It also pre-empted its own worst case, in the log, before the fault:

```
[CutoverManifest] WITHHELD  UI  ??_7Dialog@UI@@6B@
[CutoverManifest]   ^ NOT published, and 5 by-name door(s) in this tree WILL resolve NULL.
                     If attempt 26 faults reaching this symbol, that is this line,
                     not a new discovery.
```

It did not fault there. But a wall that announces itself *in advance* and is then ruled out is
worth more than one diagnosed afterwards.

## 3. WHAT THIS DOES NOT ESTABLISH

- **The wall after this one is NOT named.** A cutover boot is a GATE, not a planner — it can
  only name what it reached before faulting. Expect attempt 27 to find a new name, not a list.
- **One run.** No control run was taken (attempt 25's lane took one; this did not).
- **`BANISHED_RT_MAINLOOPFRAME=0` was set**, on lane BODY's advice, so its newly default-on
  `?Frame@MainLoop@Engine@@` could not confound a new failure. **Attempt 27 should re-run
  without it** — the wall may differ, and that difference is information.
- Nothing here says the 44 published rows are *correct*, only that publishing them did not
  fault. No differential oracle exists at cutover.
- `[VideoInterface] *** DETOUR FAILED ***` did **not** appear as a false alarm this run; lane
  SWEEP's fix made the check ask the registry.

## 4. NEXT

`?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ` is a factory body, slot 0 of
`?_typeInfo@StringTable@UI@@`. It is one function. The trap is the mint's, so the class was
minted before this method was ported — the ordinary shape, and the ordinary fix.

**Do not read "one function" as "one function to cutover."** That is exactly the reasoning the
gate cannot support.
