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

### The regression, and the real gap behind it

Wiring the slot turned the whole founding town into bare foundations — measured, then
confirmed visually. Chasing that to its root is where the wave actually paid off.

**What breaks.** Stage 3's `mark >= 0` branch is retail's *"reset this model to
build-controlled"*: remove it from the scene via the model's `+0x38`, stamp
`modelVariant = -1`, so the state-8 ramp can drive its construction frames. Correct for a
site about to be built; catastrophic for one already finished, which never enters state 8
and so never gets a frame back. `StartTown_SpawnStructures` hand-sets the completed
sub-mesh and *then* calls `RegisterEntity` — so `OnAddedPass` wiped it a few hundred
instructions later.

**Two fixes that don't work**, both instructive:

1. *Move the hand-set after `RegisterEntity`.* Fails: `ModelComponent`'s own slot 6
   (`ModelComponent_Draw`, the `Graphics::Scene` add) is itself gated on
   `modelVariant >= 0` (`modelcomponent.cpp:426`), so the model is never added to the
   scene at all. Same black buildings, new cause. Both components are dispatched by the
   *same* `OnAddedPass`: one needs `>= 0`, the other must not see `>= 0`. Over-constrained.
2. *Suppress the reset for already-built sites* (via `DemoGateSkip`). Works, and was
   briefly committed to — but it is a standalone deviation papering over a missing port.

**What retail actually does.** `ComponentEntity_impl_1fc0` — the initial-buildings spawner
our code says it mirrors — brackets every building:

```
1400a2677: XOR R8D,R8D              ; SetModel frame = 0
1400a2682: CALL 0x1401253d0         ; ModelComponent::SetModel(randomVariant, 0)
1400a26b9: CALL 0x1400c48a0         ; RegisterEntity -> OnAddedPass
1400a26c5: AND dword ptr [RAX+0xd0],0xfffffffe
```
preceded by the matching `|= 1`. That bit is the **instant-build flag** — already read
under that name by `buildcomponent_state1.cpp:320` and `state8.cpp:298`, and exposed in
the game's own debug menu as the insta-build setting (user-confirmed). It is also the
first test in BuildComponent's OnAdded, which we had ported and labelled
`// debug-disabled`.

So retail's answer to *"how do you register a pre-built building without the build system
un-framing it"* is: **you tell the build system this one is instant-built, for the
duration of the spawn.** Our spawner set the mesh before registering — correct, matching
retail — but never opened the bracket. Ported; the deviation is gone.

### The thunk/body split

`0x140108d40` is only four instructions:

```
MOV RAX,[0x1401c3dd8]; TEST byte [RAX+0xd0],0x1; JZ 0x140108720; RET
```

The gate is in the **thunk**; the body at `0x140108720` is ungated and has a second
caller — `0x140107bb0`, inside `BuildDescription_helper_7ae0` = **State8Enter**. That is
the `sub_8720` `buildcomponent_state8.cpp` still lists as deferred. Our port had the gate
*inside* the body, which would have no-op'd that second caller, since it runs exactly when
the bit is set. Now split, and both addresses registered.

### The `SetModel` frame argument — identified, deliberately not "fixed"

Retail passes frame **0** (disassembly-verified, not a decompiler artifact); we pass
`subCount-1`. The ramp settles which is which: `State8Enter` resets to frame 0 at
construction start, `State8Update` ramps up, completion snaps to `frameCount-1`. So frame
0 is the *first construction stage* and retail spawns start-town buildings there — relying
on the instant-build path to finish them. That path is `sub_8720`'s second caller, and it
is **not ported**.

Our `subCount-1` is therefore a stand-in for an unported completion path. Changing it to
`0` would faithfully reproduce one instruction and leave every founding building frozen as
a timber frame — strictly worse. Left as-is and documented at the deferral site. **Not
proven:** what triggers the instant completion during spawn (hypothesis: the
StateMachineComponent's own OnAdded entering state 1 inside the same pass, while the bit is
still set — untraced).

### Final A/B

Bridge harness + demo house, after all of the above:

| | control (flag off) | flag on |
|---|---|---|
| invocations | 1 | 2 |
| via slot 6 | 0 | **1** |
| via `BC8_EnsureMeshList` | 1 | 1 |
| start-town buildings reset | — | **0** (bracket suppresses all 8) |
| bridge master mesh list | never filled | **0→5** (all 5 segments) |
| bridge `+0x4c` multiplier | 1 (clamp default) | **13** (real, = the 13-cell span) |
| `standalone_crash.log` | 0 bytes | 0 bytes |

The bridge master is the one slot-6 dispatch; the demo house still comes through the lazy
fallback (it reaches state 8, where `EnsureMeshList` fires first). All 5 collected segments
remain `+0xf0` null, so the `tool_base_render.cpp` guard is still load-bearing — unchanged.

### Still opt-in, and why

The regression that held it back is fixed at the source. What is *not* soaked is the
consequence of finally reading a real `+0x4c`: the bridge's build target becomes
`matComp[0x30] * 13` instead of `* 1`. That is almost certainly right — a 13-cell bridge
should cost 13 segments of labour — but no build has yet completed with a real multiplier
(bridges stall at `delivered=0 required=24` for unrelated reasons). Per project policy a
feature graduates once *verified*, not once *plausible*, so `BANISHED_LIVE_BUILD_ONADDED`
stays out of `kDefaultOnFlags` until a site completes end-to-end with a multiplier > 1.

The instant-build bracket in `StartTown_SpawnStructures` is **not** flag-gated — it is
retail behaviour that was simply missing, and the control run above confirms it is neutral
with the flag off.

### Traps this pass paid for

* **Verify the slot index before writing the port, not after.** One `get_xrefs_to` would
  have caught `+0x40` vs `+0x30` at any point in the previous two sessions. The cost was
  a code comment that read as measured fact and got copied twice.
* **"It is never dispatched" and "dispatching it fixes X" are two separate claims.** The
  first was true here; the second was false. Un-dormanting a function proves what the
  function does — it is not evidence for the diagnosis that motivated it.
* **Instrument the consequence, not just the call.** Counting dispatches showed the wiring
  worked. Only probing `+0xf0` on the collected models showed it did not matter.
* **A third stale label, same session.** `// debug-disabled` on the instant-build bit —
  after `+0x40` and after `"default off"`. Two *other* files already had the right name for
  that exact bit. Grep the codebase for an offset before naming it from one call site.
* **When a deviation looks necessary, look one level up first.** The `DemoGateSkip` guard
  worked and would have shipped. The question "is this a gap in how we build the town at
  start?" is what turned a permanent deviation into a four-line port of retail's own
  bracket.
* **A 4-instruction function is a gate, not an implementation.** Ghidra inlines the tail
  jump, so the decompiler shows the whole body under the thunk's name and the gate looks
  like the body's first statement. Check the function's byte length before assuming its
  decompilation is its scope.
* Pre-existing and untouched: `[PickupStoredRawMaterial] ERROR: some function pointers
  not resolved!` (`typeId7158=0`) floods the default run's log.

## The insta-build path, end to end (2026-07-27)

Three opens from the pass above, all closed: what triggers the instant completion during
spawn (was "hypothesis, untraced"), `sub_8720`'s instant branch (was "unported"), and the
`BANISHED_LIVE_BUILD_ONADDED` graduation (was "plausible, not verified").

### The path

Retail's insta-build is not a special case in the spawner — it is the ordinary build state
machine, run to completion inside `RegisterEntity`:

```
StartTown / ComponentEntity_impl_1fc0, per building:
  gw+0xd0 |= 1                          ; instant-build bit (also the debug menu setting)
  ModelComponent::SetModel(variant, 0)  ; construction frame 0, NOT the finished mesh
  EntityList_RegisterEntity --> ComponentEntity_OnAddedPass
        StateMachineComponent slot 6 (+0x30, 0x14012b660) = StateMachine::Tick(sm, Time(0), 0, 0)
          |  Tick's outer loop runs *while a pending state is set*, so the whole chain
          |  below happens inside this one call:
          |- state 1 enter (0x140108290): instant bit set => skip the gather-work spawn,
          |                 request 1 -> 8
          |- state 8 enter (0x140107ae0) INSTANT branch:
          |       sub_8720 (= BuildComponent's OnAdded BODY) fills the mesh list + stamps
          |       each model build-controlled (variant -1), then
          |       +0x48 (delivered) = matComp[0x30] * +0x4c (multiplier), then
          |       every mesh -> its FINAL frame (frameCount-1), then request 8 -> 2
          \- state 2 entered => the 8->2 action (Finalize) + State8OneShot run: site dequeued
                   from townMgr+0xd0, WorkPlaceComponent state-2 enter, storage
                   reconfigured as OUTPUT — and every *other* controller's state-2 enter
                   fires too (ResidenceComponent's household registry, etc.)
  gw+0xd0 &= ~1
```

So the answer to "what triggers the instant completion" is `StateMachine::Tick`'s
pending-state loop: a state's enter handler that requests a transition is serviced by the
*same* Tick, so 1 -> 8 -> 2 completes before `RegisterEntity` returns. The earlier
hypothesis ("the StateMachineComponent's own OnAdded entering state 1 inside the same
pass") had the entry point right and did not know the loop was what carried it to the end.

`SetModel`'s frame argument is settled by the same finding: retail's **0** is correct
*because* this path exists. Our `subCount-1` was the stand-in for it.

### What was missing in standalone — three things, not one

1. `sub_8720`'s caller: State8Enter's INSTANT branch (unported, traced only).
2. State1Enter's `else` leg: it traced "state-8 transition DEFERRED" instead of requesting.
3. **The SM component itself**: slot 6 was never wired *and* our controller-linking ran
   lazily on the first `Update` — a frame after the spawner clears the bit. So even with
   (1) and (2) ported, nothing would have run inside the bracket. Retail links in slot 4
   (`StateMachineComponent::Startup` 0x14012b6a0, dispatched by `StartupComponents` before
   `RegisterEntity`); we now do the same, and slot 6 runs the Tick.

All three ported, behind `BANISHED_LIVE_SM_ONADDED` (opt-in) plus a new
`COMPONENT_LIVE_FACTORY_SOA` macro (Startup + Update under one env var, OnAdded under its
own).

### Measured, flag on, `BANISHED_SEED=12345`

| founding structure | result |
|---|---|
| State1Enter reached | 7 / 7 (the 8th has no BuildComponent) |
| `State8Enter INSTANT` completions | 7 / 7 |
| mesh frame | `variant=N frame -1 -> 2` — sub_8720 un-frames, the instant branch snaps to the final sub-mesh (= the exact value the old stand-in hand-set) |
| `delivered(+0x48)` | 60 / 30 — identical to what the hand-stamp computed |
| SM current state `+0x50` | **2** for all 7, `pending(+0x52)` back to 0xff |
| completion bookkeeping | `State8OneShot` + `State8Finalize` run for founding buildings for the first time: dequeue from townMgr+0xd0, WPC state-2 enter, storage reconfigured OUTPUT (`accept=0x60 cap=1000` per house — the same numbers the fuel-haul session derived by hand; `accept=0x16220 cap=6000` for the hall) |
| residence state-2 registrations | 6, same as control |
| crash log | 0 bytes, both runs |

Retired by it: the `subCount-1` model frame, the `+0x52 = 2` stamp, and the
`+0x48`/`+0x4c` build-bookkeeping stamp — **for the 7 structures the real path reaches**.

### The one entity it does not reach, and why the stand-in stays

Start-town entry `[1]` (4x6 footprint, no model group, no `BuildComponent` — the stockpile)
has nothing to drive its state machine, and its own first controller is inert so the linker
cannot read a real default either. It lands at `+0x50 = 0`, which `EntityUI`'s page router
(`pageIdx = pending < 0 ? current : pending`, `entityui.cpp:588`) reads as the
under-construction page.

So the stamps are not deleted — they became a **post-`RegisterEntity` fallback** that fires
only when the real chain did not land on state 2, and logs either way. The measurement is
the point: exactly one fallback line, for exactly that entity.

⚠ `+0x50` is the CURRENT state and `+0x52` the PENDING one (block pointer at `+0x48`; the
`sm` pointer `StateMachine::Tick` receives IS `SMC+0x48`, so its `sm+0x08..0x0b` are
`SMC+0x50..0x53`). The first version of the fallback probe tested `+0x52` and reported
`-1` for every building that had *succeeded* — the stand-in wrote the pending byte so the
first tick would ENTER state 2, and after a real entry pending is 0xff again.

### `BANISHED_LIVE_BUILD_ONADDED` — graduated, and the blocker was mis-attributed

The stated blocker was: "the flag makes the bridge's build target `matComp[0x30] * 13`
instead of `* 1`, and no build has completed with a multiplier > 1."

That premise is wrong. `State1Enter` reads `+0x4c` from the **same** MultiObjectComponent
finder (`0x140004740`) and runs *after* registration, so it sets the multiplier for every
site that enters state 1 whether slot 6 ran or not. Measured with the flag **off**:

```
[BuildTrace] HaveAllMaterials ... delivered=0 required=60 (costQty=4 mult=15) SHORT   # run A
[BuildTrace] HaveAllMaterials ... (costQty=4 mult=21)                                # run B, different span
```

The multiplier was already real on the default path. The flag is target-neutral, so there
was never an unverified target riding on it. What it does change is verified: the mesh list
is filled at the faithful time (0 lazy fallbacks), and Stage 3 un-frames a site's model at
register time — whose one regression (the founding town) is fixed at the source by the
instant-build bracket, and whose remaining visible effect (a placed foundation carries no
building mesh until construction starts) is what retail does.

Positive evidence with it on: the organic demo-house build still completes end to end
(30/30 credits -> 8->2 -> OneShot + Finalize), 0-byte crash log. **Promoted to
`kDefaultOnFlags`.**

### Still open

* `BANISHED_LIVE_SM_ONADDED` stays opt-in. It is verified on the spawn path above, but it
  changes controller-link *timing for every entity* and newly runs `State8Finalize`
  (storage reconfigure) on all founding buildings — economy surface that wants a real soak.
  **The soak gate itself is broken, and that is the finding**: the standing town-health
  recipe goes EXTINCT by frame 9000 with *both* flags explicitly off.

  | frame | flags off (control) | both flags on |
  |---|---|---|
  | 3000 | 27 citizens | 27 |
  | 6000 | 18 | 13 |
  | 9000 | **none — extinct** | **none — extinct** |

  Cause, identical in both runs and unrelated to either flag:
  `CLAMPED food seed 6000 -> 0 units (cap=1000, fuel reserves 1600, keeping 100 free)`,
  then `seeded food into 7/7 houses (8 units each)`. `BANISHED_SEED_FUEL`'s 400 units/house
  is 1600 volume against a real house capacity of 1000 (`StorageDescription+0x24`), so the
  house is already over cap before any food goes in and the food seed clamps to ~nothing.
  Hunger climbs 7.5 → 11.4 (death at 25) and the town starves. Until the fuel stand-in is
  sized to the real capacity (or the woodcutter exists), this recipe cannot clear ANY flag —
  the 13-vs-18 gap at frame 6000 is a difference between two dying towns, not a signal. The
  earlier note "loses ~40% of the town by frame 6000, pre-existing" was the same thing seen
  before it finished.
* A completed build with a multiplier > 1 is still not demonstrated *by labour*. Not a
  blocker for the flag (see above), but bridges still stall with `delivered=0` — 4 log units
  x a 15-21 cell span is 60-84 units that never arrive. That is the haul-to-a-bridge-site
  question, untouched here.

### Traps this pass paid for

* **A field can be the pending state, not the state.** The fallback probe "confirmed"
  failure for seven buildings that had all succeeded. Read the consumer (`entityui.cpp`
  already had `pending < 0 ? current : pending`) before asserting what a byte means.
* **"The flag changes X" needs the write ordering checked.** Two writers of `+0x4c` exist;
  the later one runs on the default path, so the earlier one can never be the source of a
  behaviour difference. One grep for the offset shows both writers.
* **A "one-line port" can be three ports.** Both branch handlers were pointless without the
  component wiring that lets them run at spawn time. Ask what *dispatches* the code before
  calling it the last piece.

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
