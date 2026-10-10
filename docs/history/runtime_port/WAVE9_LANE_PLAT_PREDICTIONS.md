# Lane PLAT — predictions recorded BEFORE any run (2026-09-08)

Build identity string: `[LanePLAT] rev=PLAT-r2 registered=11`
  11 = Application::PlatformStartup, WindowProc, Achievement, ExternalFactory,
       Engine, Audio::Device, Input::Device, and the three Input message
       callbacks — plus nothing else. WideString's `PEBD` ctor is counted by the
       System wave's own line, and `Input::Device::PlatformUpdate`'s newly added
       name registration is counted by input_win.cpp's line, so the Input wave's
       printed total also moves by 1.

Tool revision for the residue set-diff: the `BANISHED_RT_STRICT=1` census in
`standalone/resolve_standalone.cpp` (dumped from `standalone/main.cpp:1940`),
UNCHANGED by this lane from base 798bfd3. SAME EXE ON BOTH SIDES — the baseline
is this same binary run with the lane kill switch `BANISHED_RT_PLAT=0`.

## P1 — residue set-diff (baseline `BANISHED_RT_PLAT=0`  minus  mine)

REMOVED, predicted exactly three:
    ?PlatformStartup@Application@1@AEAA_NXZ
    ?PlatformStartup@Device@Audio@@AEAA_NXZ
    ?PlatformStartup@Device@Input@@AEAA_NXZ
All three are resolved by our own ported `Engine::Startup`
(engine_lifecycle.cpp:638, :730, :741) and all three fell through to the DLL
before this lane.

ADDED, predicted small and NAMED IN ADVANCE. My bodies open doors the baseline
run never opens, and each is a possible new residue row if nothing else in the
run had already asked:
    ?PlatformUpdate@Device@Input@@AEAAXXZ   — now registered, so it should NOT
        appear; if it DOES appear on the mine side, the name registration did
        not take.
    ?RegisterMessageCallback@Application@1@…  — ported by lane VID2, expect none.
    ?_singleton@Configuration@Input@@0PEAV12@EA and
    ?_singleton@Device@Input@@0PEAV12@EA and
    ?_singleton@Application@1@0PEAV11@EA  — DATA names. Other live files already
        ask for all three, so expect "already there".
    addr:0x18014bc20 / addr:0x18014be30 — the DIDATAFORMAT probe resolves both
        by address on the mine side ONLY. These ARE expected as two ADDED rows,
        and they are diagnostic scaffolding rather than a port dependency: the
        probe is the only caller and it does not run at cutover.

NOT predicted to move: WindowProc, Achievement, ExternalFactory, Engine
PlatformStartup and the three Input callbacks are NEVER RESOLVED BY ANYTHING
(their xrefs are the export address table alone, and the exe imports none of
them), so they cannot leave a list they were never on. A residue count that
moved by 11 would mean the census counts registrations, not resolutions.

## P2 — diff-harness counters on `BANISHED_RT_DIFF=1`

    Application::Application::PlatformStartup   calls=1 worked=1
    Audio::Device::PlatformStartup              calls=1 worked=1
        (worked=0 is a REAL outcome, not a failure — it means this machine has
         no XAudio2 2.7 redistributable; the printed engine=/voice= flags say so)
    Input::Device::PlatformStartup              calls=1 worked=1
        (worked=0 would mean the Input configuration's +0x09 flag was set and the
         body took the skip-DirectInput path)
    System::WideString::ctor(const char*)       calls=1 worked=1  (chars=8)

`calls=0` beside a non-zero `worked=` means the oracle is aimed at a branch the
body never takes. `calls=0 worked=0` on any of them means registered but never
reached.

## P3 — `BANISHED_PLAT_ORACLE=1`

    [PlatOracle] compared=14 differing=0 nonTrivial>=3
    [PlatProbe]  WindowProc cases=10 differing=0 branches=6
    [PlatOracle] Audio::Device::PlatformStartup ours=… theirs=… (equal)
    [PlatProbe]  DIDATAFORMAT keyboard_mismatch=0 mouse_mismatch=0 fields=1078
                 NEGATIVE-CONTROL(kbd vs mouse) mismatch>0

`fields=1078` is 2*5 header comparisons + (256+11)*4 object comparisons. Any
smaller number means the object walk was cut short by a header mismatch.

## P4 — `BANISHED_PLAT_ORACLE=1 BANISHED_PLAT_NEGCTL=1`

    [PlatOracle] differing=1  (hbrBackground ours=7 theirs=6)
    [PlatProbe]  WindowProc NEGATIVE-CONTROL cases=10 differing=1 branches=6
                 — the VK_F10 case: the wrong body returns DefWindowProcW's
                   value where the shipped one returns 0.

If either of those reports differing=0, the probe measures nothing and every
green number in P3 is worthless.
