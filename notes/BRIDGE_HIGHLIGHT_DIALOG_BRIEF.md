# The two manual-play bridge opens: the drag preview and the click crash

Session 2026-07-26, on `wave/upgrade-path-complete`, following `40165be`. The two items
that only hand-play had found (task #23):

* **A.** No red/green validity box while dragging a bridge.
* **B.** Clicking a placed bridge crashes the next frame.

Both are closed. **Neither had the cause the previous brief predicted.**

## The predicted cause was wrong, and why

`40165be` proposed that A and B were unregistered type-id globals — `0x1401c4410`
HighlightComponent, `0x1401c4448` HighlightDescription, `0x1401c7fc8` UIComponent — the
same silent-null class as the `0x1401c7ac0` fix in that wave. They are indeed
unregistered. They are also **not the cause of either bug**: every reader of all three
already carries a `GetTypeIdByName` fallback.

```cpp
uint32_t ResolveTypeId(uintptr_t rdataAddr, const char* name, uint32_t* cache) {
    if (*cache) return *cache;
    uint32_t* p = (uint32_t*)ResolveAddr(Module::Exe, rdataAddr);
    if (p && *p) { *cache = *p; return *cache; }
    *cache = ::Object::GetTypeIdByName(name);          // <- the fallback
    return *cache;
}
```

`tool_base_render.cpp` (`ResolveTid`), `edittool_select.cpp` (`ResolveTypeId`) and
`pathtool_setselected.cpp` (a hand-rolled static) all do this. They were left
unregistered deliberately and each site compensated locally.

**The lesson**: `NOT IMPLEMENTED` on a type-id address proves the address is
unregistered. It does not prove the *reader* is broken. Check the reader before
promoting a log line to a diagnosis. Registering them centrally is still worth doing as
cleanup — it would retire three local workarounds — but it is a separate change with its
own un-dormanting risk, and it is NOT what fixed anything here.

## A — `BridgeTool` vtable slot 8 was `Tool_Ret0`

The red/green box is `BridgeTool::Draw`, retail `0x1400fadc0`, and it was never ported:

```cpp
(void*)&Tool_Ret0,   // +0x40 Draw — retail 0x1400fadc0 (bridge preview, NOT ported)
```

The comment had been sitting in `bridgetool.cpp` the whole time. Nothing was broken —
the preview simply did not exist. `CalcCost` returning `valid=1` every drag frame was
never evidence about rendering, because nothing consumed it.

Ghidra mislabels `0x1400fadc0` as `BridgeTool_OnRemove`. The body is unmistakably a
draw: it builds a `0x14`-stride tile array and hands it to `0x1401008c0`
(`fn_TileRender`), exactly as the already-ported `RoadTool::Draw` (`0x140105190`) does.
The two colours are literals — `0xffff0000` red, `0xff00ff00` green.

Ported in `code/Game/Player/bridgetool_draw.cpp`. Shape:

* **body cells** — every cell of the bounding rect between drag start and snapped end.
  Green when the path validates AND the terrain bit is in `maskBody | maskBodyExtra`
  AND the cell is unoccupied. `+0x28` (`maskBodyExtra`) is a description field
  `ValidatePath` does not read; only the preview ORs it in.
* **single-cell hover** — any `maskApproach` (bank) terrain reads green, so the player
  can see where a bridge may start.
* **clearance ring** — only during a real drag, `neighborRadius` cells each side,
  perpendicular to the run. `WoodBridge` has `radius=0`, so this is dormant for the
  stock bridge; it exists for descriptions that set one.

Two things worth carrying:

1. **The colour variable persists across the clearance ring.** Retail reuses one
   register for the body cell and its whole ring, so a red body cell forces its ring
   red. Ported faithfully, with a comment, because it looks like a bug and isn't ours.
2. **Argument slots were read from the disassembly, not the decompiler** — this project
   has been burned twice by decompiled argument order (`FromYawPitchOffset`'s pitch, the
   element-vs-byte offsets). Verified: `0x1400fb27b` loads `RDX=context`, `R8=&tiles`,
   `R9=&empty` and never loads `RCX` at all (a leftover register; the road port passes
   `nullptr` for it too).

**One deliberate deviation, flagged:** retail indexes the terrain array with no bounds
test, so a span within `radius` of the map edge reads past the end of the array every
frame of the drag. The port range-checks `idx` — the same guard `ValidatePath`'s port
already applies.

Verified in-game (`BANISHED_BRIDGE_DRAG_FRAMES=45`, new):

```
[Bridge] Draw(drag):  start=32396 end=34956 rect=(140,126)..(140,136) valid=1 radius=0
                      tiles=11 green=11 red=0
[Bridge] Draw(hover): ... valid=0 radius=0 tiles=1
```

11 tiles for an 11-cell vertical span, all green against `valid=1`; the invalid hover
renders its single tile red.

## B — a selection outline whose source MeshInstance has no node array

The crash log's frames symbolize (Runtime base `0x180000000`) to something quite
different from "the UI draw chain":

```
#0  Math::Transform::Lerp                     18001d690
#1  Graphics::MeshInstance::GetNodeToWorld    180016c20
#2  Graphics::EdgeMeshInstance::Draw          1800a3150
#3  Graphics::Renderer::DrawItemList          180105240
#4  Graphics::Renderer::Draw                  180104bb0
#5  GameUpdateLoop_Draw +0x35b
```

That is the **selection outline**, drawn from the world scene — the `HighlightComponent`
`EdgeMeshInstance` at `+0x28`. Which meant the first job was noticing that
`reg_player_render.cpp`'s header saying the outline chain is **DEFAULT OFF** is stale:
`BANISHED_SELECTION_OUTLINE` has been in `kDefaultOnFlags` since it graduated. That
comment is now corrected — it briefly ruled out the correct answer.

The mechanism, from the disassembly of `EdgeMeshInstance::Draw`:

```
1800a33c7  MOV RCX,[RSI + 0xc8]     ; the SOURCE MeshInstance
1800a33d9  JZ  0x1800a3469          ; null -> skip, draw without node follow
1800a33e4  CALL GetNodeToWorld      ; else: source->nodes[idx]
```

`+0xc8` (the third `SetMesh` argument — the mangled name ends `PEAVMeshInstance@2@@Z`)
**is** null-checked. Its **node array** at `MeshInstance+0xf0` is not.
`GetNodeToWorld` computes `nodeIdx * 0x38 + *(this + 0xf0)` and passes it straight to
`Transform::Lerp`, which reads `[R8+0x10]`. With a null node array and node 0 that is a
READ of `0x10` — and the recorded registers say exactly that:

```
RCX=000000000000001C   (= pSVar11 + 0x1c, so pSVar11 == 0)
R8 =0000000000000000   (= pSVar11)
Fault: READ of 0000000000000010
```

`Impl_c4a0`'s PREBUILT branch passes that source as `ModelComponent+0x28`. Retail can
rely on "a ModelComponent's MeshInstance always has its mesh"; standalone cannot,
because `ModelComponent` is built by an inert factory whose on-added never ran — the
same family as `PickingComponent::OnAdded` (task #22).

**Fix** (`tool_base_render.cpp`): if the source instance's `+0xf0` is null, pass
`nullptr` instead. That takes retail's own no-source branch — the outline still draws,
it just does not follow node transforms.

### Measured

New harness switch `BANISHED_BRIDGE_SELECT=N` fires a **real full click** (info dialog +
outline) on the placed master N frames after the commit, and again after completion.
With the instrumentation in place:

```
foundation select : 5 of 5 segments -> "PREBUILT source MeshInstance ... has NO node
                                        array (+0xf0 null)"
completed select  : 0 of 5           (variant flips (0,0) -> (0,2); the finished
                                      model's mesh loads with nodes)
```

Five `EdgeMeshInstance`s were sitting in the live `Graphics::Scene` in exactly the shape
that faults. A **bridge foundation** is what the player clicked.

### What is NOT proven

**The headless harness never reproduced the crash itself**, across every variant tried —
foundation select, completed select, 301 frames of settle. The five fatal objects were
in the scene and simply were never *drawn*: the harness has no camera control and the
bridge sits ~11 tiles from town, outside the view frustum. `DrawItemList` only walks
culled-in items, so `EdgeMeshInstance::Draw` never ran on them.

So the evidence is: the exact fatal state is real, observed, and now guarded — but the
guard has not been shown to fix the user's crash by reproducing and then not
reproducing it. **The manual test should be re-run** (place a bridge by hand, click the
foundation while looking at it) to confirm. `BANISHED_OUTLINE_TRACE=1` will print the
guard line if it fires.

This is the same blind spot `40165be` recorded, one level down: the harness now drives
the click, but still cannot drive the *camera*, and the fault is in rendering. A repro
that exercises a code path is not a repro that exercises its consequences.

## Harness additions

`BANISHED_TOOL_HARNESS=bridge` gained two switches, both opt-in:

| env | effect |
|---|---|
| `BANISHED_BRIDGE_DRAG_FRAMES=N` | hold the tool in the dragging state over the chosen span for N frames before committing, so slot 8 renders the multi-cell preview instead of only the single-cell hover |
| `BANISHED_BRIDGE_SELECT=N` | N frames after the commit, swap back to the EditTool and select the master — a full click (info dialog + outline); repeats once the bridge completes |

`BANISHED_OUTLINE_TRACE=1` shows which branch `c4a0` takes per segment.

## Regression

* Bridge harness, full scenario: drag preview → commit → `[Bridge] COMPLETE` at frame
  ~447 → both selects survive → `survived 301 frames of Draw`. `standalone_crash.log`
  0 bytes.
* Default flags, no harness, ~2 500 frames: no crash, normal logs.

## Open

* **Confirm B by hand** — see "What is NOT proven" above.
* **The three type-id globals** (`0x1401c4410`, `0x1401c4448`, `0x1401c7fc8`) are still
  unregistered. Registering them centrally would retire three local
  `GetTypeIdByName` workarounds, but it un-dormants nothing that is currently broken and
  carries the shared-global risk `0x1401c7ac0` demonstrated. Cleanup, not a fix.
* ~~**`ModelComponent`'s on-added is a stub** — that is *why* a foundation's MeshInstance
  has no node array.~~ **WRONG — measured and corrected 2026-07-26, see below.**

* ~~**`BuildComponent::Draw`'s draw-tick is never dispatched** — *that* is why a foundation's
  MeshInstance has no node array.~~ **HALF WRONG — verification pass 2026-07-27, see
  "The `BuildComponent` slot-6 verification pass" below. The dispatch really was missing,
  but it is not a draw-tick, it is not at `+0x40`, and wiring it does NOT give a
  foundation a node array.** Measured with `BANISHED_MODEL_TRACE=1`:

  | claim | measured |
  |---|---|
  | `ComponentEntity_CopyCtor` skips `StartupComponents` | **false** — `0x1400c3720` is `CtorFromDescription`; its tail calls it (`componententity.cpp:663`) |
  | `ModelComponent::Startup` is unwired | **false** — `g_ModelComponent_RealVtable[4]`; 96/96 world models got a mesh + nodes through it |
  | a bridge segment's Startup was skipped | **false** — segment ends `comp(0,-1)`, mirroring its `ModelDescription(+0x30=0, +0x34=-1)`; the factory zero-inits to `(0,0)`, so `-1` proves Startup ran |
  | `BuildComponent::Draw` (`0x140108d40`) runs | **false** — **0 dispatches** in a full harness run |

  `modelVariant = -1` is not "no model", it is retail's "a `BuildComponent` picks the
  frame" — and retail's `Startup` skips `SetMesh` for it too. The frame-picker is
  `BuildComponent::Draw`, whose Stage 2 walks the `MultiObjectComponent` segment array
  (`+0x48`/`+0x50`) that `CalcPath` fills. It is ported (`BuildComponent_DrawReimpl`) and
  registered, but nothing dispatches vtable `+0x40`, so it never runs.
  `BC8_EnsureMeshList` is the hand-rolled workaround for exactly this and says so in its
  own comment — but it only fires from the state-8 path, which a bridge never reaches
  (`delivered=0 required=24` forever).

  Wiring the draw-tick un-dormants Stages 1–4 for **every** build site (the `+0x4c`
  multiplier write, the Stage 3 `+0x60`/`+0x38` dispatches per collected node), so it
  wants its own wave behind an opt-in, not a drive-by. The `tool_base_render.cpp` guard
  stays correct meanwhile.

  **Trap paid for:** the stale comment "ModelComponent is built by an inert factory whose
  on-added never ran" (in `tool_base_render.cpp` and in this file) sent the next session
  at an already-wired slot. Corrected in both. Same lesson as the stale "default off"
  below, one level down: *before "wire slot X", count live dispatches — being registered
  and being ported are not being called.*

## The `BuildComponent` slot-6 verification pass (2026-07-27)

The item above was taken as a work order — "wire vtable `+0x40` behind an opt-in and
verify". Both halves of the premise turned out to be wrong, in opposite directions.

### The slot is `+0x30` (index 6), and it is `OnAdded`, not `Draw`

`0x140108d40` has exactly **one** xref: `0x140162290`. That is
`vtable_BuildComponent (0x140162260) + 0x30` — **index 6**. Every neighbour confirms the
base: `+0x18` = `0x140108d80` (Shutdown), `+0x38` = `0x140108cb0`, `+0x78` =
`0x140108710` (MsgReceive) — each already named that way in `buildcomponent.cpp`.

`+0x40` (index 8) is `0x140118a70`, a **shared empty `return;` stub**. There was never
anything there to wire.

Index 6 is this project's long-established **OnAdded** slot — `ComponentEntity_OnAddedPass`
(`componententity.cpp:701`) dispatches it at `EntityList_RegisterEntity` time for
components whose `+0x20` bit 0 is set (BuildComponent's flags are `0x83`, so it
qualifies). So it is a **one-shot at registration**, not a per-frame tick. Ghidra's
`BuildComponent_Draw` is the same slot-6 mislabel already recorded for
`InteractComponent` and `CitizenComponent`.

The stale claim came from `BC8_EnsureMeshList`'s own comment ("runs every frame as the
component's draw-tick (vtable +0x40)"), which this brief then repeated. Both corrected.

### Wired it anyway — `BANISHED_LIVE_BUILD_ONADDED`, opt-in

`BuildComponent_LiveVtable[6]` → `BuildComponent_DrawReimpl`, installed from
`BuildComponent_InstallMsgSlot` so it survives `InitInertTyped`'s per-instance re-stub.
Deliberately **not** in `kDefaultOnFlags`. A/B, bridge harness + demo house:

| | control (flag off) | flag on |
|---|---|---|
| invocations | 1 | **9** |
| via slot 6 | 0 | **9** |
| via `BC8_EnsureMeshList` fallback | 1 | **0** |
| bridge master mesh list | never filled | **0→3** (all 3 segments) |
| bridge `+0x4c` multiplier | 1 (clamp default) | **6** (real `MultiObjectComponent+0x58`) |
| `standalone_crash.log` | 0 bytes | 0 bytes |

It works, and it works *better than expected on the one point I had flagged as risky*:
the ordering caveat is empirically fine. The bridge master's OnAdded fires at line 690,
**after** all three segments are created (684–688) and appended to the
`MultiObjectComponent` — so Stage 2 collects all of them. The lazy fallback drops to
zero calls, confirming the two paths are cleanly idempotent.

### But it does NOT fix the node array — the guard stays load-bearing

This was the whole reason the item existed, and it is disproven. After the bridge
master's OnAdded ran to completion over all three collected segments:

```
[ModelTrace] OnAdded EXIT #9 meshCount 0->3 storeCount=3 mult(+0x4c)=6
[ModelTrace]   mesh[0] model=...5a08 variant(0,-1) nodes@+0xf0=0000000000000000
[ModelTrace]   mesh[1] model=...63d8 variant(0,-1) nodes@+0xf0=0000000000000000
[ModelTrace]   mesh[2] model=...f8c0 variant(0,-1) nodes@+0xf0=0000000000000000
```

Still null. Which the disassembly says it must be: **Stage 3 never calls `SetMesh`.** It
hides the model (`+0x38`), stamps `modelVariant = -1`, and re-adds draw children
(`+0x60`). It is the frame *un*-setter — the "this model is now build-controlled"
transition. The frame is set later, by `State8Enter`/`State8Update` via
`BC8_SetMeshFrame`. A bridge never reaches state 8 (`delivered=0 required=24`), so its
segments keep an empty `MeshInstance` no matter what slot 6 does.

**`tool_base_render.cpp`'s null-`+0xf0` guard remains correct and load-bearing, with the
flag on or off.** Its comment is corrected again: the cause is not the missing dispatch,
it is that a foundation legitimately has no frame mesh until the state-8 ramp.

### Measured regression — why this stays opt-in

The 8 founding StartTown buildings take Stage 3's `mark >= 0` branch. Before / after, same
`ModelComponent`:

```
Startup : self=...de0cf0 modelVariant=0  mesh=...557730 nodes=...de1508
OnAdded : mesh[0] model=...de0cf0 variant(0,-1)         nodes=...de1508
```

Completed buildings get `modelVariant` stamped to `-1` and their draw entry cycled — i.e.
put into the un-framed build-controlled state — and nothing restores a frame, because they
are already finished and never enter state 8. The node array survives (Stage 3 does not
clear the mesh), so this is a display-state change, not a corruption; the visual result
was not confirmed headless, but "un-framed" is what `BC8_SetMeshFrame` treats as hidden.

That is a real behaviour change to the founding town on a flag that fixes nothing
currently broken. **Not promoted.**

### What this bought

Not the bug it was aimed at. What it did buy: the mesh list is now filled at the faithful
time by the faithful path for every site, the real `+0x4c` multiplier is read instead of
clamped, and the hand-rolled `BC8_EnsureMeshList` workaround is provably redundant when
the flag is on (0 fallback calls). Those are the reasons to keep it and finish it later —
after the state-3/completed-site path stops stamping finished buildings.

### Traps this pass paid for

* **Verify the slot index before writing the port, not after.** One `get_xrefs_to` would
  have caught `+0x40` vs `+0x30` at any point in the previous two sessions. The cost was
  a code comment that read as measured fact and got copied twice.
* **"It is never dispatched" and "dispatching it fixes X" are two separate claims.** The
  first was true here; the second was false. Un-dormanting a function proves what the
  function does — it is not evidence for the diagnosis that motivated it.
* **Instrument the consequence, not just the call.** Counting dispatches showed the wiring
  worked. Only probing `+0xf0` on the collected models showed it did not matter.
* Pre-existing and untouched: `[PickupStoredRawMaterial] ERROR: some function pointers
  not resolved!` (`typeId7158=0`) floods the default run's log.

## Traps this wave paid for

* **A stale "default off" comment is worse than no comment.** It ruled out the correct
  subsystem for the first half of the session. When a flag graduates into
  `kDefaultOnFlags`, fix its registrar's header in the same commit.
* **A `NOT IMPLEMENTED` line names an unregistered address, not a broken caller.** Three
  of them were red herrings because every reader already had a fallback.
* **Symbolize the whole stack before naming the subsystem.** "Runtime UI draw chain" was
  a guess; five minutes of `get_function_by_address` turned it into
  `EdgeMeshInstance::Draw` and the bug fell out immediately.
* **Retail's null checks tell you what retail's invariants are.** `EdgeMeshInstance::Draw`
  checks `+0xc8` and not `+0xf0`: that is retail saying "a source instance always has
  nodes". Every such asymmetry is a place where an inert-factory standalone will diverge.
