# DEVLOST hand-in (branch `d5-devlost`, base `a3b293c0a`)

The device-lost pair `GameUpdateLoop::OnVideoError` / `OnVideoRestart` (retail 0x140095970 /
0x140095930, NEXT_SESSION section 2.4, known_port_issues row 890), plus the menu loop's twin
pair found on the way. Application-x64.exe, and one Runtime member (`ImageBuffer::Release`).

**Static gate only.** The game was never run from this worktree. The forced device-lost route
is owed (section 5).

## 1. What retail does, call by call

All five bodies were read as disassembly in Ghidra (Application-x64.exe); the Runtime
member with objdump on `Runtime-x64.dll`. Import slots are named from Ghidra's decompile.
Ghidra now names them `GameUpdateLoop_OnVideoError`, `GameUpdateLoop_OnVideoRestart`,
`WorldGeometry_OnVideoError`, `WorldGeometry_OnVideoRestart` and `Minimap_OnVideoRestart`
(program saved).

### [8] 0x140095970 `GameUpdateLoop::OnVideoError`

| address | retail | port |
|---|---|---|
| 0x140095979 | `RCX = [this+0x110]`; `TEST`; `JZ` | `if (m_worldGeometry)` |
| 0x140095985 | `CALL 0x1400b0be0` | `m_worldGeometry->OnVideoError()` |
| 0x14009598a | `RBX = [this+0x148]`; `TEST`; `JZ` ret | `if (Minimap* minimap = m_imageCapture)` |
| 0x14009599a | `CALL [0x1401524a0]` `ImageBuffer::Release([RBX+0x10])` | `minimap->overlayImage->Release()` |
| 0x1400959a9 | tail `JMP [0x1401524a0]` `ImageBuffer::Release([RBX+0x08])` | `minimap->terrainImage->Release()` |

### [9] 0x140095930 `GameUpdateLoop::OnVideoRestart`

| address | retail | port |
|---|---|---|
| 0x140095939 | `RCX = [this+0x110]`; `TEST`; `JZ` | `if (m_worldGeometry)` |
| 0x140095945 | `CALL 0x1400b0c50` | `m_worldGeometry->OnVideoRestart()` |
| 0x14009594a | `RCX = [this+0x148]`; `TEST`; `JZ` ret | `if (m_imageCapture)` |
| 0x14009595b | tail `JMP 0x1400a59a0` | `m_imageCapture->OnVideoRestart()` |

### 0x1400b0be0 `WorldGeometry::OnVideoError` (new member, geometry_draw.cpp)

1. `VertexBuffer::PlatformDestruct(this+0x100)` (import 0x140152670)
2. `IndexBuffer::PlatformDestruct(this+0x120)` (import 0x140152678)
3. `VertexBuffer::PlatformDestruct(this+0x160)`
4. `IndexBuffer::PlatformDestruct(this+0x180)`
5. `if ([this+0x148]) ([this+0x148])->vtbl[0](1)`: `delete _heightImage`. **Retail does not
   null +0x148.** It dangles until step 6 of the restart stores the new image over it. Nothing
   reads it in between: `Engine::MainLoop::OnVideoError` destroys the Context right after the
   loop's slot [8], and no frame is drawn before slot [9]. Kept as retail, with a comment.

### 0x1400b0c50 `WorldGeometry::OnVideoRestart` (new member, geometry_draw.cpp)

1. to 4. `PlatformConstruct` on +0x100, +0x120, +0x160, +0x180 in that order (imports
   0x140152680 vertex, 0x140152688 index): new companions on the new device, relinked into the
   engine lists.
5. `0x1400ac1e0` `BuildTerrainMesh()` (ported): Creates +0x100, fills and Sets every chunk,
   builds +0x120.
6. `0x1400ac780` `UploadHeightImage()` (ported): MemAlloc(0x68) a new `ImageBuffer`, stores
   it at +0x148, Create, Lock / CopyToTexture from +0x140 / Unlock.
7. `IndexBuffer::Create(this+0x180, [this+0x1b8]*2, false, [this+0x1b0])` (import 0x140152660)
8. tail `VertexBuffer::Create(this+0x160, [this+0x1a8]<<4, false, [this+0x1a0])` (import 0x140152668)

Steps 7 and 8 are the same two calls the world build ends with (water_contour_mesh.cpp:777).

### 0x1400a59a0 `Minimap::OnVideoRestart` (new member, minimap_upload.cpp)

1. `ImageBuffer::Recreate([this+0x10])` (import 0x1401524a8), then `Recreate([this+0x08])`
2. `Device::Lock`; `CopyToTexture([this+0x08], [this+0x18], [this+4], [this+4])`; `Unlock`
3. `Device::Lock`; `CopyToTexture([this+0x10], [this+0x28], [this+4], [this+4])`; tail `Unlock`

### Runtime 0x1800f33c0 `Video::ImageBuffer::Release` (video_resources.cpp, was declared, not ported)

`if (_data) _data->vtbl[0](1); _data = nullptr;`. The companion is guarded and the field is
always nulled; `this` is dereferenced unguarded. Spelled `delete companion`, as
`ImageBuffer::ReleaseDynamic` spells the same slot-0 call.

### The menu loop's twin pair (second commit)

The callers of 0x1400b0be0 / 0x1400b0c50 include two tail-jump stubs that are
`MenuDialogLoopT` slots [8] 0x14009a8d0 and [9] 0x14009a8b0:
`MOV RCX,[RCX+0xbc0]; TEST RCX,RCX; JNZ 0x1400b0be0 (resp. 0x1400b0c50); RET`. The tree's
bodies printed a trace line and did nothing else. They now make the call under retail's null
test. The trace lines stay, because DEVLOST_FINDING greps for them. `MenuDialogLoopT::_sceneGeometry`
(+0xbc0) is retyped from `Graphics::Geometry*` to `WorldGeometry*`. It is a copy of
`MenuScene::geometry`, which is already `WorldGeometry*`. The layout is unchanged and its
`static_assert` holds.

## 2. Offsets used

Every offset is an existing named field with a `static_assert`. No field was added.

| offset | field | header |
|---|---|---|
| GameUpdateLoop+0x110 | `m_worldGeometry` | GameUpdateLoop.h |
| GameUpdateLoop+0x148 | `m_imageCapture` (Minimap*) | GameUpdateLoop.h |
| WorldGeometry+0x100 / +0x120 | `_vertexBuffer` / `_indexBuffer` | WorldGeometry.h |
| WorldGeometry+0x148 | `_heightImage` | WorldGeometry.h |
| WorldGeometry+0x160 / +0x180 | `_waterVertexBuffer` / `_waterIndexBuffer` | WorldGeometry.h |
| WorldGeometry+0x1a0 / +0x1a8 | `_contourVertices.data` / `.size` | WorldGeometry.h, RawArray |
| WorldGeometry+0x1b0 / +0x1b8 | `_contourIndices.data` / `.size` | WorldGeometry.h, RawArray |
| Minimap+0x04 | `size` | Minimap.h |
| Minimap+0x08 / +0x10 | `terrainImage` / `overlayImage` | Minimap.h |
| Minimap+0x18 / +0x28 | `terrainPixels._data` / `overlayPixels._data` | Minimap.h |
| ImageBuffer+0x60 | `_data` | ImageBuffer.h |
| MenuDialogLoopT+0xbc0 | `_sceneGeometry` | MenuDialogLoopT.h |

## 3. What stays a PORT GAP

Nothing. Every callee these bodies reach was already ported or is ported here. `grep -rn
"PORT GAP" src` now finds one line, the comment that says "no PORT GAP".

No guard was added anywhere. Every null test in the new code is retail's. Retail
dereferences these unguarded, and so does the port:

- **The minimap images.** `overlayImage` and `terrainImage` are used without a test in
  slot [8] and in 0x1400a59a0. In a running game both exist, because the first in-game Draw
  builds them (gameupdateloop.cpp:1309).
- **The injection must come after the first in-game frame.** A device loss while
  `GameUpdateLoop` is current but before its first Draw would fault on a null image, in
  retail too. The forced route injects at Present #12000, well past that.

**None of these objects is reached by the engine's dynamic walkers.** The minimap images and
the height image are usage 2, and the four buffers are created non-dynamic.
`ReleaseDynamic` / `RecreateDynamic` skip them. This pair is their only device-lost path,
which is why the no-op left them on the dead device.

## 4. Static checks

| check | result |
|---|---|
| `tools/build.sh build debug --jobs 2` | EXIT 0 (full build of a fresh worktree, then the incremental for commit 2) |
| new warnings | 0. No warning names a touched file; the 109 in the full log are pre-existing (array.cpp, array_destroy.cpp, alloc_stats.cpp, ...) |
| `tools/build.sh unit debug --jobs 2` | 1002 tests, 0 failures |
| `python tools/abi_audit.py` | OK, exit 0 |
| `python tools/castmetric.py src` | total 2 |
| `bash tools/acceptance/acceptance.sh` | 0 line(s) off target |

The perf flavour was not built. The gate's `-O2` build is the coordinator's.

## 5. What the coordinator should look at on the forced route

The route is `BANISHED_FORCE_DEVICE_LOST=12000` with `devlost.sh`, `DLMAX=5`.

**Expected lines per round trip.** These come from the ported helpers, in
`standalone_debug.log` (stdout), between `[DeviceLost] hr=... -> calling` and
`recreate OK` in `standalone_error.log`:

```
[sub_c1e0] entry self=<wg> gridSize=<N> chunkSize=<c> ... createOk=1
[sub_c1e0] chunk(0,0) scratch v0: ... Set->1 ...
[sub_c780] post-Create img=<new> createOk=1 width=<N>
[sub_c780] CopyToTexture(img=<new>, bytes=<...>, w=<N>, h=<N>)
```

- `<wg>` must equal the world build's `[sub_c1e0] entry self=` value.
- `img=` must differ from the world build's value, because it is a new allocation.
- No `[MenuDLT::OnVideo*]` line is expected in-game. It appears only for a loss on the menu.

**What "fixed" looks like, if H1 of DEVLOST_FINDING is right.** The injected trip
(`hr=0x887a0007`) is followed by no `hr=0x887a0005` at all:

```
INJECT=1, COMPLETE=1
```

With that, the run reaches its normal end without `DLMAX` killing it. The town keeps drawing
terrain, water and the minimap after the trip. Look at a screenshot after the trip: terrain
and water present, and the minimap not black.

**Working set.** Expect one step at the injected trip and then flat. The injection leaves
device 0 healthy, and retail's `d3dDevice = nullptr` before Shutdown means it is never
Released. So device 0 and everything destroyed on it stay alive once. That is a harness
consequence, kept on purpose. What must be gone is the ~100 MB climb per trip. If the
removals stop, there are no further trips to climb.

**If `0x887a0005` trips still recur:**

- **H1 is not the whole cause.** The pair is ported exactly, so another game-owned GPU object
  is still crossing devices. Candidates are objects our port creates non-dynamic where retail
  makes them dynamic or an External.
- **Settling it** needs the D3D11 debug layer run that DEVLOST_FINDING section 2 describes.
- **Memory.** Watch whether the per-trip growth fell even though the trips continue. If it
  did, the stranded terrain was suspect 3 of section 3.

**Crash signatures to recognise:**

- A fault in `ImageBuffer::Release` or `Minimap::OnVideoRestart` on a null `this` means the
  loss came before the first in-game Draw. That is retail's window, not a port defect.
- A fault reading `_heightImage` between the two slots means something drew between them.
  That would contradict the engine order recorded above.

## 6. Commits

| commit | title |
|---|---|
| `c88a2ede2` | behaviour: GameUpdateLoop::OnVideoError / OnVideoRestart = retail 0x140095970 / 0x140095930 (row 890) |
| `154ba9fad` | behaviour: MenuDialogLoopT::OnVideoError / OnVideoRestart = retail 0x14009a8d0 / 0x14009a8b0 |

STATUS.md, NEXT_SESSION.md and row 890 are not edited here. The PORT GAP count drops from
3 to 1 and row 890's state waits on the route. Both are the coordinator's to record.
