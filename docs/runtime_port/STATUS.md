# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12, late night, tools/cutover_closure, roots = census + every minted vtable slot)

| | |
|---|---|
| reachable functions from our call surface | 2,870 |
| unported | 273 |
| of which C runtime (our CRT covers them) | 128 |
| **engine functions to port (static lower bound)** | **145** (34,803 bytes) |
| minted vtables complete | **150 of 150** |

Worklist: `docs/runtime_port/closure/WORKLIST.md`. The roots now include every non-purecall
slot of every minted vtable (`build_roots.py` reads `rt_data_mint_tables.inc`), which is what
turned this morning's "0 to port" into 856 and let the vtable wave run as disjoint class
batches instead of one gate wall per boot. The 145 left are the callees those slot bodies
reach by address: ~30 `??3` operator-delete rows (one 93-byte body, register it N times),
the 748/818-byte `ExternalPtr<T>::Serialize` / `ExternalPtrArray` instantiations still
unported (`0x180076e90` is the current gate wall), the Graphics leftovers batch 6 left on
the DLL by name (`Renderer::DrawItemList` 0x180105240, `GraphicsMesh::PlatformSerialize`
0x1800a48e0, `SpotLight::CreateTargets`, `Context::SetLight`, `~Particle` /
`~ParticleSystem` / `~Animation`), Video's `PlatformSerialize`, Audio's `~SoundBuffer` /
`OnSoundBufferDeleted`, and a few Achievement / System / Object leaves.

## Cutover gate

`make cutover` (deploys the debug exe, moves the DLL aside, boots with `BANISHED_RT_NO_DLL=1`,
which implies `RT_MINT` + `RT_ALLOC`, dumps the live registry, restores the DLL; output in
`docs/runtime_port/cutover/latest/`). Run it under `tools/game_dir_lock.sh` when agents share
the game directory.

Walls passed 2026-09-12, in order: `ExternalPtr<Material>::Serialize` → `MaterialInstance::Serialize`
serializer pair → `Material::Type` → `??0VideoProgram` → `??0VertexLayout` → `??0VertexProgram`
(ShaderProgram / VertexProgram / PixelProgram / RenderState) → `MaterialInstance::Serialize+0xfe`
→ minted `?_typeInfo@Dialog@UI@@` factoryFn → `GroupDescription` → `RibbonDescription` →
`ImageDescription` → `AnimatedImageDescription` → `TextDescription` (the vtable wave: 53 UI
Description/Element classes, Font/CharacterSet/Cursor/SpriteCapture, 9 Graphics buffer/light/mesh
classes, ParticleSystem, AnimationGroup, 3 Audio, 4 spatial, 6 Object/IO classes) →
**current: `UI::Font::Serialize` (font.cpp:381) calls the field-`+0x28` serializer
`0x180076e90` by address; it is unported, so `ResolveAddr` returns null and the call is
EXEC-of-0.** It is one of the 748-byte `ExternalPtr<T>::Serialize` instantiations
(`object/external_ptr_serialize.cpp` is the template; identify T from the `__typeInfo` it
checks). No mint trap fires any more: every minted slot has a body.

**Next batch is the worklist as it stands (145 rows, no vtable work):** port the
`ExternalPtr` family rows first (each is the same thunk shape), then the Graphics leftovers,
then the operator-delete rows in one file. Regenerate after; the gate should then reach
whatever the closure still cannot see (callbacks stored in data).

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (re-verified 2026-09-12
late night on the merged tree after the vtable wave, 973 UI exports registered: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
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
- Ports reached only through the DLL's own vtables (every `UI::*Description` / `*Element`
  body, the ShaderProgram family, Audio::Stream::Run) show no oracle attendance DLL-present;
  only the gate exercises them. The DLL-present run proves link/register/boot, nothing more.
- `deleting_dtor.cpp` row indices are table positions (135 rows now). Every parallel batch
  collided on them; append after the last row and re-count before registering.
- `tools/rt_deleting_dtor_verify.py` flags the two `Object::*Compiler` rows BAD (their dtors
  restamp two vtables; the reader knows one) and eight rows UNRECOGNISED (bare tail-jump
  dtors). None is a port defect; each is explained at its row.
- Two mesh-code lines reached the DLL through `RT_ORIGINAL_ONCE`, the raw door that skips
  the registry, and silently bypassed two ported bodies until 2026-09-12. A bypassed port
  never runs, so the oracle cannot see it. Grep for that macro before trusting attendance.
