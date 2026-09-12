# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 night, tools/cutover_closure, ported set from the live registry)

| | |
|---|---|
| reachable functions from our call surface | 1,756 |
| unported | 128 |
| of which C runtime (our CRT covers them) | 128 |
| **engine functions to port (static lower bound)** | **0** (was 206 this morning) |

Worklist: `docs/runtime_port/closure/WORKLIST.md` (empty). The static closure is closed:
every function the DLL's call graph reaches from our call surface is either ported or CRT.
What remains is exactly what the closure cannot see: classes reached only through minted
vtable slots and factory steps. The mint reports **68 of 150 vtables complete**; the boot
walks the `UI::*Description` / `*Element` family in resource order, about one class per
gate run (each is a whole-class port: ctor, every slot, dtor, deleting-dtor row).

## Cutover gate

`make cutover` (deploys the debug exe, moves the DLL aside, boots with `BANISHED_RT_NO_DLL=1`,
which implies `RT_MINT` + `RT_ALLOC`, dumps the live registry, restores the DLL; output in
`docs/runtime_port/cutover/latest/`). Run it under `tools/game_dir_lock.sh` when agents share
the game directory.

Walls passed today, in order: `ExternalPtr<Material>::Serialize` → `MaterialInstance::Serialize`
serializer pair → `Material::Type` → `??0VideoProgram` → `??0VertexLayout` → `??0VertexProgram`
(ShaderProgram / VertexProgram / PixelProgram / RenderState ported whole) →
`MaterialInstance::Serialize+0xfe` (Array rows) → minted `?_typeInfo@Dialog@UI@@` factoryFn
(`CreateNewObject@Dialog` was behind a default-OFF flag; the only such gate in the tree) →
`GroupDescription` → `RibbonDescription` → `ImageDescription` → `AnimatedImageDescription` →
**current: `??_7TextDescription@UI@@6B@` slot 2, `?Type@TextDescription@UI@@UEBAIXZ`.**

**Next, with the disassembly already read (see the commit for `animatedimagedescription.cpp`):**
`UI::TextDescription`, sizeof 0x90, ctor OUT-OF-LINE at 0x18011ea20 (not table-driven in
`factory_ui1.cpp`, so it must be ported), dtor 0x18011eae0, Serialize 0x18011ee30 (passes
`1`, not `0`, as the encoding factory's third argument; ends in a real version/IsBinary
conditional), CreateInstance 0x18011f3d0 (needs `??0TextElement@UI@@QEAA@XZ` 0x18011eb80 and
the applier 0x18011ef70), GetFileVersion 0x18011ee20 returns 0x304, Type/TypeName
0x180041ab0/0x180041ad0, Reconstruct 0x180041c50, Destruct 0x180041c80, deleting dtor
0x180041e30. Its `+0x68` is `ExternalPtr<UI::Font>::Serialize` 0x18003f1e0, one `EXTPTR_THUNK`
row in `object/external_ptr_serialize.cpp`. Template: `groupdescription.cpp` /
`ribbondescription.cpp`; `imagedescription.cpp` for a base+subclass pair.

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (re-verified 2026-09-12
night on the merged tree: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`, exit 0, empty crash log, the ten divergent sites are the
pre-existing negative controls and vtable-identity artefacts).
Known broken route: the unattended Mods menu example (`BANISHED_AUTOMENU_BUTTON=7
BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6`) dies with an ACCESS_VIOLATION at
`ModDialog_ToggleMod_Impl+0x43` reading 0xd0. Upstream null, not yet found.

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
- `VertexLayout::Add` / `GetStreamCount` / `GetElement*` are non-virtual and unported.
- Ports reached only through the DLL's own vtables (the `UI::*Description` family, the
  ShaderProgram family) show no oracle attendance DLL-present; only the gate exercises them.
- Two mesh-code lines reached the DLL through `RT_ORIGINAL_ONCE`, the raw door that skips
  the registry, and silently bypassed two ported bodies until 2026-09-12. A bypassed port
  never runs, so the oracle cannot see it. Grep for that macro before trusting attendance.
