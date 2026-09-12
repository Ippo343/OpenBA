# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 evening, tools/cutover_closure, ported set from the live registry)

| | |
|---|---|
| reachable functions from our call surface | 1,752 |
| unported | 298 |
| of which C runtime (our CRT covers them) | 92 |
| **engine functions to port (static lower bound)** | **206** (77,629 bytes) |
| of which unnamed `FUN_` internals | 148 |

Worklist: `docs/runtime_port/closure/WORKLIST.md`. The static closure is a lower bound: the
whole Video shader family below was invisible to it because it is reached only through
vtable slots and factory steps. Expect the gate to keep finding classes the closure cannot.

## Cutover gate

`make cutover` (deploys the debug exe, moves the DLL aside, boots with `BANISHED_RT_NO_DLL=1`,
which now implies `RT_MINT` + `RT_ALLOC`, dumps the live registry, restores the DLL; output
in `docs/runtime_port/cutover/latest/`).

Walls moved today, in order: `ExternalPtr<Material>::Serialize` → `MaterialInstance::Serialize`
passing a lone `Stream*` as the serializer pair (fixed) → `Material::Type` trap (Material
ported whole) → `??0VideoProgram` (ported whole, and the mint now carries every exported
vtable) → `??0VertexLayout` (ported whole) → **current: `??0VertexProgram@Video@@QEAA@XZ`**.

**Next batch, all on the boot path, all in `runtime/src/video/`:** `VertexProgram` and
`PixelProgram` (both derive from `ShaderProgram`, whose `Serialize` + `PlatformSerialize`
carry three device-interface arms and ~8 Array instantiations), then `RenderState`
(with its `Stencil`/`Depth`/`AlphaBlend` sub-serializers). Follow `vertexlayout.cpp`:
one file per class, ctor + all slots + dtor + deleting-dtor row, Array specs in
`collection/array.cpp` / `array_destroy.cpp`, ExternalPtr thunks in
`object/external_ptr_serialize.cpp`.

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (not re-verified after
today's ports; run it before the next commit that touches serialization).
Known broken route: the unattended Mods menu example (`BANISHED_AUTOMENU_BUTTON=7
BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6`) dies with an ACCESS_VIOLATION at
`ModDialog_ToggleMod_Impl+0x43` reading 0xd0. Upstream null, not yet found.

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
- `VertexLayout::Add` / `GetStreamCount` / `GetElement*` are non-virtual and unported.
