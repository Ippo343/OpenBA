# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12, tools/cutover_closure, Ghidra on Runtime-x64.dll)

| | |
|---|---|
| reachable functions from our call surface | 1,751 |
| unported | 488 |
| of which C runtime (our CRT covers them) | 90 |
| **engine functions to port** | **398** (125,978 bytes) |
| of which unnamed `FUN_` internals | 272 (80,760 bytes) |
| over 1 KB | 16 |

Worklist: `docs/runtime_port/closure/WORKLIST.md`. Biggest single items: `SunLight::GetSunCamera`
(3.8 KB), `Scene::AddCulledDrawItems` (3.0 KB), `Modification::SetupFileSystem` (2.6 KB),
`FUN_180094218`, `FUN_180062d60`, `AnimationState::UpdateNodes`.

## Cutover gate

`make cutover` (deploys debug exe, moves the DLL aside, boots with `BANISHED_RT_NO_DLL=1`,
restores the DLL; output in `docs/runtime_port/cutover/latest/`).

Last result (attempt 33, 2026-09-12, before the reorientation): EXEC-of-0 at
`RtGraphics_MaterialInstance_Serialize+0x83` = `ExternalPtr<Graphics::Material>::Serialize`
(`0x180061100`), one of 25 `ExternalPtr<T>::Serialize` instantiations, 6 ported.

## DLL-present build

`make sa-run` boots to the in-game world and the pause-menu Quit path works.
Known broken route: the unattended Mods menu example (`BANISHED_AUTOMENU_BUTTON=7
BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6`) dies with an ACCESS_VIOLATION at
`ModDialog_ToggleMod_Impl+0x43` reading 0xd0. Upstream null, not yet found.

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) written, armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
