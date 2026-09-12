# WAVE 28 LANE GFXBODY — the ten remaining named Graphics BODY rows

Branch `wave28/gfxbody`, cut from `wave28/base` at `5d2594de`.
Source: `runtime/src/graphics/wave28_lane_gfxbody.cpp`.
Written as the lane ran, not at the end.

## 0. SCOPE AND WHERE IT CAME FROM

All ten rows were handed to this lane by `WAVE27_LANE_GFXBIG.md` §11.1, which is
worth restating because the arithmetic looks wrong in the handover: lane GFXBIG
GREW the Graphics `BODY && ported==0` list from four rows to eleven, and that
was a WIN. Every new row was a direct callee its three bodies introduced into
the `called` column. They were always in the DLL and always unported; only
their VISIBILITY changed.

Ten of those eleven are mine. `?AddDrawItems@MeshInstance@Graphics@@` is lane
SLOTS'; particles are lane PARTSYS'; the factory family is FACTUI1/2/REST'.

## 1. ADDRESSES AND EXTENTS — MEASURED

`pefile` over the export directory of the shipped
`C:\GOG Games\Banished\Runtime-x64.dll`, read before a line was written.

| Export | Retail | Ord | Bytes | .pdata frags |
|---|---|---|---|---|
| `?GetConstantName@Material@Graphics@@` | `0x180102830` | 2506 | 72 | 1 |
| `?GetTextureName@Material@Graphics@@` | `0x180102900` | 2970 | 72 | 1 |
| `?FindConstant@MaterialInstance@Graphics@@` | `0x180102f40` | 2343 | 211 | 1 |
| `?FindTexture@MaterialInstance@Graphics@@` | `0x180103020` | 2361 | 211 | 1 |
| `?ApplyEvents@Animation@Graphics@@` | `0x1800fa9a0` | 1605 | 242 | **3** |
| `?BeginDrawGroup@Renderer@Graphics@@` | `0x180104fd0` | 1640 | 314 | 1 |
| `?UpdateRootOnly@AnimationState@Graphics@@` | `0x1800fc870` | 4525 | 934 | **3** |
| `?CopyInstancedToUnique@AnimationState@Graphics@@` | `0x1800fd4c0` | 1868 | 1127 | **3** |
| `?UpdateNodes@AnimationState@Graphics@@` | `0x1800fcc20` | 4522 | 2191 | **5** |
| `?AddCulledDrawItems@Scene@Graphics@@` | `0x1801017a0` | 1543 | 2950 | 1 |

**No extent above is a single `.pdata` row.** Four of the ten are assembled from
chained fragments and the first fragment understates them badly — `UpdateNodes`
reports **126** bytes for a **2191**-byte body, a 17x understatement, and
understating makes work look cheap
(`reference_pdata_is_a_lower_bound_on_function_extent.md`). The extents were
taken by walking `.pdata` forward while the successor carries
`UNW_FLAG_CHAININFO` (0x4) **and** its chained-parent record points back into
this body — the second half matters, because mere address contiguity is
satisfied by the next unrelated function.

**Every one of the ten is a real named export.** `AEAA`/`AEBA` in these names
means PRIVATE, not unexported (`reference_aeaa_is_private_not_unexported.md`),
so none of them needs an address door.

## 2. GREP-BEFORE-WRITE — MEASURED, and what it found

All ten names and all ten addresses were grepped across
`code/ engine/ runtime/ standalone/ dx11/` before anything was written
(standing warning 8). **No body existed for any of them.** What did exist:

- **Ten `RT_EXPORT_ONCE` name doors inside `wave27_lane_gfxbig.cpp`**, one per
  row.
  > **CORRECTED BY §9.3, AND THE CORRECTION IS THE POINT.** This paragraph
  > originally read "So these registrations are LIVE the moment they land — this
  > lane has no inert rows." **The live run measured two of eight live and six at
  > attendance zero.** A door is a RESOLUTION SITE; it is not a call. The
  > inference was wrong in the same way Wave 27's was about `BuildParameters`,
  > and it is left here struck through rather than deleted, because the deleted
  > version is the one the next lane would have re-derived.
- `runtime/types/graphics/Material.h` (Wave 6 lane F) and
  `runtime/types/graphics/Camera.h` (Wave 6 lane F) — both already pinned from
  the neighbouring bodies. Every field they name held against the disassembly.
- `runtime/types/graphics/AnimationState.h` (Wave 27 lane GFXBIG) — see §4, the
  strongest single finding of this lane.
- `standalone/scene_watch.cpp:267` and `scene_watch.h:47` quote
  `AddCulledDrawItems`' retail address **0x1801017a0 in prose only**, not as a
  live identity token. Checked explicitly, because in this same namespace
  `?AddDrawItems@MeshInstance@` IS compared as an identity token and registering
  it would break the comparison.

## 3. WHAT LANDED

Seven of the ten scoped bodies are ported and registered, plus one row this lane
itself opened (§6). §7 says what did not land and why.

| Row | State | Live attendance (§9.2) | Notes |
|---|---|---|---|
| `Material::GetConstantName` | PORTED, REGISTERED | 0 — UNVERIFIED | §1 of the source |
| `Material::GetTextureName` | PORTED, REGISTERED | 0 — UNVERIFIED | §1 |
| `MaterialInstance::FindConstant` | PORTED, REGISTERED | 0 — UNVERIFIED | §2 |
| `MaterialInstance::FindTexture` | PORTED, REGISTERED | 0 — UNVERIFIED | §2 |
| `Animation::ApplyEvents` | PORTED, REGISTERED | 0 — UNVERIFIED | §3 |
| `Renderer::BeginDrawGroup` | PORTED, REGISTERED | **1,600,000** | §4, one address door (R1) |
| `AnimationState::UpdateRootOnly` | PORTED, REGISTERED | 0 — UNVERIFIED | §5 |
| `Camera::operator=` | PORTED, REGISTERED | **3,300,000** | §6 — **not one of the ten**; this lane opened the row and closed it |
| `AnimationState::CopyInstancedToUnique` | NOT PORTED | — | §7 |
| `AnimationState::UpdateNodes` | NOT PORTED | — | §7 |
| `Scene::AddCulledDrawItems` | NOT PORTED | — | §7 |

**PORTED + REGISTERED IS NOT VERIFIED**, and the third column is here so the two
cannot be read as one. Six of the eight carry a zero.

Kill switch: **`BANISHED_RT_GFXBODY28=0`**. Its own variable, not shared with
`BANISHED_RT_GFXBIG`, and the reason is an A/B that would otherwise be
impossible: with GFXBIG on and this off, lane GFXBIG's three bodies run and
reach the **DLL** for exactly these callees — which separates "the big body is
wrong" from "one of its callees is". Sharing a variable destroys that.

## 4. THE STRONGEST FINDING: `Graphics::Animation` IS THE CLIP RECORD, PROVED FROM THE OTHER SIDE

Wave 27 lane GFXBIG built `AnimationClipPrefix` (0x70 stride) **entirely from
`AnimationState::Update`'s reads of `group->clips[i]`** — a reader's view, from
outside the class. It named +0x08 `eventCount`, +0x30 `frameFirst`, +0x34
`frameLast`, +0x48 `loopScale`, +0x50 `frameRate`, +0x58 `duration`, and left
+0x00 as `_head_00[8]` with "no reader in Update".

`?ApplyEvents@Animation@Graphics@@` is a **member of that class**, and its
`this` reads **all six of those offsets in exactly those roles**:

```
1800fa9bb  MOV RBX,[RCX+0x58]     duration   (int64 ticks)
1800fa9d6  CMP [RCX+0x8],EBP      eventCount (loop bound)
1800fa9f3  MOVD XMM1,[R15+0x50]   frameRate  (an INT, CVTDQ2PS'd)
1800faa00  SUB EAX,[R15+0x30]     frameFirst
1800faa08  MOV EAX,[R15+0x34]     frameLast
1800faa22  MULSS XMM2,[R15+0x48]  loopScale
```

and it computes `((frame-first)/(last-first) * frameRate) * loopScale` — the
**same three-instruction association order** GFXBIG transcribed for
`AnimationState::Update`'s loop-restart offset.

**Two bodies reached from opposite directions agreeing on six offsets and one
float association order is a far stronger statement than either alone**, and it
is the reason this lane reuses `AnimationClipPrefix` instead of minting a
second struct beside it. The one field it ADDS is +0x00: the **event array
base** (`MOV RDX,[R15]`), walked with stride 0x10 against the +0x08 count. So
+0x00/+0x08 are a `Collection::Array` data/size pair and GFXBIG's "no reader"
hole is now filled by a reader in a different function.

Recorded in the positive direction on purpose: the sign of such an agreement
carries no information until somebody runs the check, and this one was run.

## 5. THE `COUNT THE STORES` WARNING, MET IN ITS EXACT SHAPE

Kickoff warning 3 said a mismatch between the writes a body performs and the
writes its named helper accounts for is THE signal, and that it is visible in
disassembly and invisible in decompiled C. `UpdateRootOnly` is that case:

- The blend calls `?Lerp@Quaternion@Math@@` (0x18001d500, ord 3191) and copies
  **exactly four dwords** out of its result (0x1800fcb7e..0x1800fcbb0, into
  `[RSP+0x30..0x3c]`).
- The accumulator is a 0x1c-byte `Math::Transform` — **seven** floats. The other
  three, the translation at `[RSP+0x40/0x44/0x48]`, come from XMM7/XMM8/XMM9,
  which were lerped **BY HAND** three instructions earlier as
  `cur + (accum - cur) * t`.

A port that handed the whole Transform to `Lerp` and copied the whole thing back
passes every quaternion check and silently substitutes the DLL's translation for
the hand-computed one. Found by counting MOVSS stores, exactly as prescribed.

A second instance in the same body, and this one is an ARGUMENT rather than a
store count: **the blend weight comes from the NEXT layer.**
`CVTSI2SD XMM0,[RAX+RBX+0x28]` at 0x1800fcb08 uses `RBX` where every other read
in the iteration uses `RBX-0x38`, and `RBX` is `(i+1)*0x38`. So the weight is
`layers[i+1].fadeRemaining / layers[i+1].fadeLength` — the fade of the layer
being blended FROM. Reading `RBX` as "this layer" is a one-line error that
produces a working cross-fade with the wrong layer's timing.

And both halves of that weight are **narrowed to float before the divide**
(`CVTPD2PS` twice, then `DIVSS`). Dividing in double and narrowing once is a
different number.

## 6. LIFTED CONSTANTS (warning 4)

Nothing was retyped from memory.

- `0x18016169c` = `00 00 80 3f` = `1.0f`, the identity quaternion's w. Read out
  of the file with `pefile`.
- `0x1801617c0` = `00 00 00 80` x4 = the XORPS sign-bit mask (a negation, not a
  value). Used by `CopyInstancedToUnique`; `runtime/src/graphics/mesh.cpp:789`
  already reads it the same way.
- `0x1801fe728` — `System::Timer::_secondsPerTick`. **NOT a constant.** RVA
  `0x1fe728` lies beyond `.data`'s `SizeOfRawData`, i.e. in the uninitialised
  tail, so there are **no bytes in the file to lift** and a port that baked in
  "the constant" would have baked in ZERO. Resolved through the tree's existing
  `g_Timer_secondsPerTick` (`engine/funcs/engine_funcs.cpp:1546`).
- `0x1801fe7b0` — `?_singleton@AnimationManager@Graphics@@`, a DATA export
  (ord 4799), reached through `RT_DATA_SLOT` by name, not by address.

## 7. WHAT DID NOT LAND — and the honest reason for each

**These three are NOT ported. Saying so plainly rather than describing what was
available.**

### 7.1 `?AddCulledDrawItems@Scene@Graphics@@` (2950 bytes) — NOT PORTED

The largest single body in the Graphics gap and a frustum-cull + draw-item
emit over the scene quadtree. Out of budget for this lane. It is the one row
here that changes what is on screen every frame, so it should lead the next
Graphics lane's scope, not trail it.

### 7.2 `?UpdateNodes@AnimationState@Graphics@@` (2191 bytes) — NOT PORTED
### 7.3 `?CopyInstancedToUnique@AnimationState@Graphics@@` (1127 bytes) — NOT PORTED

Both are dominated by a **fully inlined `Math::Transform` composition** —
`CopyInstancedToUnique`'s middle block (0x1800fd630..0x1800fd83a) is ~180
instructions of interleaved MULSS/SUBSS/ADDSS with no call to a named helper, a
quaternion product plus a rotate-vector with the operand order scrambled by the
scheduler. Porting it correctly means transcribing every term in its own order;
porting it by recognising the algorithm is precisely what warning 4 forbids, and
an RNG-shaped hazard applies here too — a wrong sign on one term of a quaternion
product produces plausible-looking animation.

**What IS measured about them and should not be re-derived:**

- `CopyInstancedToUnique` takes a FIFTH argument on the stack
  (`MOVSXD RCX,[RSP+0x150]` at 0x1800fd4d1) — the `H` in its mangled name — and
  that argument is a **packed handle**, decoded `(h>>8)&0xff` / `h&0xff` exactly
  as `UpdateRootOnly` decodes `layer.handle`.
- The `AnimationManager` group table shape is confirmed by BOTH bodies: group
  stride **0x30** (`LEA RDX,[RAX+RAX*2] / ADD RDX,RDX` then `*8`), entry stride
  **0x48** (`LEA R8,[RAX+RAX*8]` then `*8`), reached through
  `manager->_groupData` at `group + 0x20`. `UpdateRootOnly` reads a `Vector3` at
  entry `+0x18`; `CopyInstancedToUnique` reads a pointer at entry `+0x28`.
- `CopyInstancedToUnique` calls `0x18001e140` and `0x18001e280` and walks its
  output with stride **0x38**, its input with stride **0x1c**, and the node
  array with stride **0x48**. Both callees must be named from the export
  directory before anything is written — this lane did not need them and did
  not look them up, which is stated so the next lane does not inherit a gap as
  a fact.

## 8. RESIDUALS — each with a named addressee

**ADDRESSEE FOR ALL OF THESE: the Wave 29 Graphics lane.** Not "the next lane
that touches this" — that clause satisfies a checklist and then waits forever
(`reference_expiry_clause_needs_an_addressee.md`).

| # | Item | Condition | Consequence at cutover |
|---|---|---|---|
| R1 | `0x1800638e0` — `Array<RendererDrawGroup>::SetSize`, 404 bytes, **confirmed absent from the export directory with pefile**. Called at its retail address from `BeginDrawGroup`'s append path. | Taken only the FIRST time a given draw-group index is used; every later frame reuses the slot. | With the DLL gone it resolves to nothing and the first frame's draw group is never appended. **Not a silent artefact — nothing renders.** |
| R2 | `Scene::AddCulledDrawItems` unported (§7.1) | every frame | already a `called` row; the cull runs in the DLL |
| R3 | `AnimationState::UpdateNodes` unported (§7.2) | every animated on-screen mesh | as R2 |
| R4 | `AnimationState::CopyInstancedToUnique` unported (§7.3) | a state with exactly one handled layer | as R2 |

R1 is expressible as an `ArraySpec` over `rtcoll::SetSizeCore` (stride 0x1f0,
align 8) — **but its element assign (`0x18001be60`) and element destroy (the CRT
`eh_vector_destructor_iterator` at `0x18007ec98` over `+0x1c0`) are THEMSELVES
unexported**, so that route opens THREE address doors where the direct call
opens one. That trade is the reason for the choice and is stated so the next
lane does not redo the analysis.

## 9. VERIFICATION — MEASURED vs INFERRED, separated

### 9.1 MEASURED

- `mingw32-make standalone-debug` — **exit 0**, `standalone/build/reimpl-x64.exe`
  32M, no warnings from this lane's files.
- Export directory (`pefile`): all ten target names present at the addresses and
  ordinals in §1; every callee classified exported / not-exported;
  `0x1800638e0`, `0x18001be60`, `0x18007ec98`, `0x180062950` and `0x18001b960`
  all confirmed **absent** from the directory.
- PE `.pdata` with chain-flag validation: the fragment counts in §1.
- Raw file bytes: `0x18016169c` == `00 00 80 3f`; `0x1801617c0` == four copies
  of `00 00 00 80`.
- Whole-tree grep for all ten names and all ten addresses before writing (§2).

### 9.2 THE LIVE RUN — MEASURED

`mingw32-make sa-deploy` then `BANISHED_AUTOSTART=1 reimpl-x64.exe`, under
`tools/game_dir_lock.sh` held as `gfxbody` for the whole run and with
`assert-held` verifying the deployed md5 was **this lane's** build
(`50f6f970a5bfdc61d865212bb5d61621`) — because a clobbered run does not announce
itself. Autostart reached the world build and ran to **frame 7200**
(`[EntityCensus] frame=7200 entities=22328`, 19 citizens).

```
[GFXBODY28] Renderer::BeginDrawGroup calls=1600000
[GFXBODY28] Camera::operator=        calls=3300000
[GFXBIG27]  Renderer::Draw           calls= 800000     (lane GFXBIG's, for scale)
[GFXBIG27]  AnimationState::Update   calls= 200000     (lane GFXBIG's)
```

**TWO OF THE EIGHT BODIES ARE MASSIVELY LIVE AND SIX PRINTED NOTHING AT ALL.**

**The 2:1 ratio is a structural check, not a coincidence.** `Camera::operator=`
is called **exactly twice** per `BeginDrawGroup` — and only on the REUSE path;
the APPEND path uses the copy CTOR instead. 3,300,000 / 1,600,000 = 2.06, i.e.
the reuse path is taken essentially every time after warm-up. That is
independent confirmation of the two-path structure read out of the disassembly
in §4 of the source, from a direction the disassembly could not supply.

### 9.3 NOT MEASURED — UNVERIFIED, and a Wave 27 claim this run CORRECTS

**Six of the eight ported bodies printed NO attendance line at all** and are
therefore **UNVERIFIED** in that word:

| Row | attendance in a 7200-frame autostart run |
|---|---|
| `Renderer::BeginDrawGroup` | **1,600,000** — live |
| `Camera::operator=` | **3,300,000** — live |
| `Material::GetConstantName` | **0** — UNVERIFIED |
| `Material::GetTextureName` | **0** — UNVERIFIED |
| `MaterialInstance::FindConstant` | **0** — UNVERIFIED |
| `MaterialInstance::FindTexture` | **0** — UNVERIFIED |
| `Animation::ApplyEvents` | **0** — UNVERIFIED |
| `AnimationState::UpdateRootOnly` | **0** — UNVERIFIED |

**THE FOUR MATERIAL ZEROS ARE NOT THIS LANE'S DEFECT, AND THE SAME RUN PROVES
IT.** All four are reached only from `MaterialInstance::BuildParameters`, and
**`BuildParameters` itself has attendance 0 in this run** — `grep -c
BuildParameters standalone_debug.log` returns **0** while
`[GFXBIG27] Renderer::Draw` and `[GFXBIG27] AnimationState::Update` both print,
so lane GFXBIG is armed and its third body simply never runs.

That **corrects a Wave 27 claim with a measurement.**
`WAVE27_LANE_GFXBIG.md` §8.1 says of `BuildParameters`: *"four name doors
already in the tree … This row goes live immediately; it is not one of the inert
ones"*, and `rt_graphics_port.cpp:129` states that for lane GFXBIG *"a zero
attendance count from this lane means something is wrong, not that the rows are
unreachable."* **Measured on this tip: `BuildParameters` attendance is zero in a
full autostart world-build run.** The inference was that four name doors implies
live; the observation is that they are not reached on this route. Per warning 13
the LICENCE is what needs repairing, not the sentence: **a door count is a count
of RESOLUTION SITES, not of CALLS**, and it cannot establish attendance. The
same faulty step also produced this lane's own expectation (§2) that its
registrations are "LIVE the moment they land" — true for two of eight, false for
six, and it was the same reasoning.

**Addressee: the Wave 29 Graphics lane** — either find the route that drives
`BuildParameters` (a material/shader reload, a mods remount, or the tool-side
`Tools-x64` path) or record it as genuinely unreached in ordinary play. Until
then all five rows are unexercised, not wrong.

The other two zeros are **expected and were predicted before the run**:

- `UpdateRootOnly` is the OFF-SCREEN cheap path. `AnimationState::Update`
  (200,000 calls) is the on-screen one, so a zero here means every animated mesh
  stayed in frustum, not that the port is dark.
- `ApplyEvents` fires only when an animation event falls inside a frame's time
  window; this town's clips evidently carry none. `g_applyEventsFired` is
  separate from `g_applyEventsCalls` precisely because `calls=N fired=0` and
  `calls=0` are different findings
  (`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).

Not run, and named rather than described as unavailable:
**`tools/trace_oracle/` was NOT run on `BeginDrawGroup`.** It compares the
draw-call/pipeline-state COMMAND STREAM and needs no frame determinism, which
makes it the right instrument for the one body here that can change a pixel. **A
pixel diff is not the alternative**: Wave 23 measured two runs at identical seed
and pin splitting ~98% apart independently of the flag under test.

`g_beginGroupAppends` — the count of APPEND-path entries — was **not read**; the
summary function in this file is not wired to any caller (see §10.2). The 2:1
ratio is the evidence that appends are rare, not a direct count.

### 9.4 RE-VERIFICATION AFTER THE SHARED-SCRATCHPAD WARNING — MEASURED

Mid-lane the integrator relayed that the session scratchpad is shared between all
seven lanes and that a sibling lane had analysis files with generic basenames
(`exp.py`, `pdata.py`, `exports.py`…) silently overwritten. Three of this lane's
helpers had exactly that shape, and one of them sat in the **shared temp root**,
not even the session scratchpad.

**Every byte, address and extent in this document was re-derived** by a single
lane-private script, `<scratchpad>/gfxbody/gfxbody_reverify.py`, which re-reads
the DLL from scratch and consults no intermediate file from the first pass.

**All eleven addresses, all eleven ordinals, all eleven extents, all four
fragment counts, all fourteen callee classifications and both raw constants
reproduced EXACTLY.** Nothing in §1, §6 or §8 changes.

Two things the re-run added that the first pass had wrong or missing:

1. **`0x1801fe728` IS A NAMED EXPORT** — `?_secondsPerTick@Timer@System@@0NA`,
   ordinal **4797**. `WAVE27_LANE_GFXBIG.md` §4 describes it only as a runtime
   global the tree resolves **by address** at
   `engine/funcs/engine_funcs.cpp:1546`. Everything GFXBIG says about it is true
   (it is in `.data`'s uninitialised tail — the re-run confirms `.data`'s raw
   data ends at `0x1801fb400`, below `0x1801fe728`, while its virtual end is
   `0x180200998`), but the conclusion "reach it by address" was drawn without
   noticing it has a NAME. **`0NA` means private, not unexported**
   (`reference_aeaa_is_private_not_unexported.md`) — that rule applied to a DATA
   symbol, where it is easier to miss. Residual R5.
2. **A blind spot in the re-verify script, caught by the script's own control.**
   Its store-scanner matched `MOV [RCX+disp8],EAX` (`89 41 xx`) and
   `MOV [RCX+disp32],EAX` (`89 81 …`) but **not** the no-displacement form
   `MOV [RCX],EAX` (`89 01`), so it reported `+0x00..+0x03` as UNWRITTEN by
   **both** Camera bodies — which is false, and the port writes it. It was caught
   only because the same script ran over the copy CTOR as a control and produced
   the same impossible hole in both. **A scanner that reports the same wrong
   answer about two subjects looks consistent**; running the control is what made
   the consistency suspicious. Corrected by hand against the objdump listing; the
   dword counts then reconcile (52 matched + 1 unmatched = 53 for the ctor,
   51 + 1 = 52 for `operator=`), which is what §6 of the source states.

**And the analogy finding of §6 is now MECHANICAL rather than eyeballed.** The
script computes the written-byte SET of both bodies and prints the difference:

```
Camera::operator=  UNWRITTEN: 0x61 0x62 0x63   0xd5 0xd6 0xd7
??0Camera copyctor UNWRITTEN:                  0xd5 0xd6 0xd7
DIFFERENCE ctor-writes-but-assign-does-not: 0x61 0x62 0x63
```

Exactly the three bytes, derived from the instruction stream rather than read off
a listing by eye.

## 10. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Warning 1. Recorded, not fixed.

### 10.1 A backgrounded `mingw32-make` wrote a ZERO-BYTE output file and still reported exit 0

The harness's background-task capture (`.../tasks/<id>.output`) was **0 bytes**
at completion for a `mingw32-make standalone-debug` that the completion
notification reported as `exit code 0`. The `echo "EXIT=$?"` at the end of the
same command line also produced nothing. So the only evidence the build
succeeded was the notification's own exit code — the output the command actually
produced was lost.

**Why this is the dangerous shape rather than a nuisance:** a build whose output
is empty and whose exit code is 0 is INDISTINGUISHABLE from a build that
compiled nothing. The lane re-ran the build in the foreground with an explicit
`> /tmp/b28.log 2>&1` and got the full log, so nothing was concluded from the
empty file — but a lane that reads "exit 0, no errors in the output" from that
file has verified nothing at all.

Not fixed, per warning 1. No project file/line is given because the defect is in
the agent harness's background-task capture, not in a file this repo owns.

### 10.2 Every wave lane's `*Summary()` is written and NOTHING CALLS IT

`runtime/src/graphics/wave28_lane_gfxbody.cpp` (`RtGraphicsWave28GfxbodySummary`,
near the end of the file) and `runtime/src/graphics/wave27_lane_gfxbig.cpp:1128`
(`RtGraphicsWave27GfxbigSummary`) both define a one-line end-of-run attendance
summary. **`grep -rn RtGraphicsWave27GfxbigSummary` outside its own file returns
NOTHING** — no caller anywhere in `code/`, `engine/`, `runtime/` or
`standalone/`.

So the only attendance a run actually emits is the milestone ladder inside
`Attend()` (1, 10, 1000, then every 100,000). **That is why §9.2 can report
`BeginDrawGroup` at 1,600,000 and §9.3 cannot report `g_beginGroupAppends` at
all**: a counter only the unwired summary prints is invisible, and a BRANCH
counter that never prints cannot do the job it was added for — distinguishing
`calls=N branch=0` from `calls=0`, which is the whole point of having one
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`). Four of
this lane's six branch counters are in that state.

This lane added its own summary in the same shape as the existing one rather
than wiring either, per warning 1.

Addressee: the Wave 29 Graphics lane, or whoever next needs a branch count.

### 10.3 The build gate prints a WARN and an OK about the same artefact

`mingw32-make standalone-debug` on this tip emits, two lines apart:

```
GATE WARN: standalone\cutover_manifest.inc is stale in COSMETIC fields only
           (a by-name door count moved -- some lane added or removed a door).
cutover manifest gate: OK (60 rows; row set and every disposition match ...)
```

Both are correct, and the pair is legible only because the WARN explains itself
at length. Recorded because **this lane caused it** — it added six by-name doors
and removed none. Per Wave 26 defect 4 the manifest is regenerated on the MERGED
tip, not per lane, so this lane deliberately did **not** run
`python tools/cutover_manifest.py --inc`. The integrator should expect to.

## 11. CENSUS DELTA — ATTRIBUTED ROW BY ROW

Re-derived on the built tip. **NOTE FOR ANYONE REPEATING THIS: the kickoff's
census command does not work in a lane worktree.** `tools/ghidra_mcp/.venv/` is
not checked in — only `requirements.txt` is. The invocation that actually runs
points at the MAIN checkout:

```
C:\code\GIT\untitled_rev_eng_project\tools\ghidra_mcp\.venv\Scripts\python.exe \
    tools/runtime_census.py --namespace Graphics
```

A worktree-relative invocation fails, and **an empty result read as a clean gate
is exactly this project's recurring defect** — this lane used the absolute path
throughout and got real output, stated so the gate is not taken on trust.

```
namespace         total   called   ported   detour  called covered  ceiling
Graphics            599      128      106        1   82.8%   96.1%
TOTAL              4632     1435     1285        9   89.5%

  namespace       gap |  BODY  CTOR  DTOR OPNEWDEL OPERATOR VTABLE  DATA  UNKNOWN
  Graphics         22 |     6     0     0        0        0      8     8        0
```

Kickoff baseline for Graphics was **126 called / 98 ported / 77.8%**.

**PORTED 98 -> 106 = +8, and every one is a row this lane wrote:**

| # | Row | note |
|---|---|---|
| 1 | `?GetConstantName@Material@Graphics@@` | in scope |
| 2 | `?GetTextureName@Material@Graphics@@` | in scope |
| 3 | `?FindConstant@MaterialInstance@Graphics@@` | in scope |
| 4 | `?FindTexture@MaterialInstance@Graphics@@` | in scope |
| 5 | `?ApplyEvents@Animation@Graphics@@` | in scope |
| 6 | `?BeginDrawGroup@Renderer@Graphics@@` | in scope |
| 7 | `?UpdateRootOnly@AnimationState@Graphics@@` | in scope |
| 8 | `??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z` | **NOT in the brief's ten** — see below |

**CALLED 126 -> 128 = +2, and both are callees this lane's own ports
introduced** (`reference_porting_can_grow_the_cutover_surface.md`):

| Row | introduced by | kind | state |
|---|---|---|---|
| `?GetRootPoseAtTime@Animation@Graphics@@` (ord 2880, 495 bytes, 3 frags) | §5 `UpdateRootOnly` | BODY | unported — residual R6 |
| `??4Camera@Graphics@@` (ord 1060, 460 bytes) | §4 `BeginDrawGroup` | OPERATOR | **ported in the same lane**, row 8 above |

`??0Camera@Graphics@@QEAA@AEBV01@@Z` and
`?_singleton@AnimationManager@Graphics@@` were **already** in the `called`
column (`wave9_lane_gfxd.cpp` and `animation.cpp` respectively), so neither
moved it. `?GetConstantName@ShaderProgram@Video@@` and
`?GetTextureName@ShaderProgram@Video@@` are **Video**, not Graphics, so they do
not appear in this namespace's delta at all — they may move Video's `called` by
up to 2 and the integrator should not read that as unexplained.

**The gap decomposition changes SHAPE, not just size.** Graphics' OPERATOR
column went `1 -> 0` (row 8 closed the only one) while its BODY column stayed at
**6** — because the three rows this lane did not land are still there and
`?GetRootPoseAtTime@` replaced the one the ports removed. A reader diffing only
the gap total sees `23 -> 22` and misses both movements.

**The identity claim, and it is the only claim made about the board:** seven
named Graphics BODY rows that were `ported==0` are now ported and registered;
one Graphics OPERATOR row **this lane itself opened** is ported and registered
with them; one new BODY row (`GetRootPoseAtTime`) is open and named.

**Do not add these to the kickoff's TOTAL.** Six other lanes are landing on the
same base, and `ceiling` has `called` in both halves so any lane opening a door
moves it (`reference_the_ceiling_percentage_is_a_moving_target.md`). The
integrator should re-derive on the merge.

## 12. RESIDUALS ADDED AFTER §8

| # | Item | Addressee |
|---|---|---|
| R5 | `0x1801fe728` is the named export `?_secondsPerTick@Timer@System@@0NA` (ord 4797), but `engine/funcs/engine_funcs.cpp:1546` reaches it **by address**. A name door is available and nobody has taken it. | Wave 29 Graphics lane, or whoever next audits address doors |
| R6 | `?GetRootPoseAtTime@Animation@Graphics@@` (0x1800faaa0, ord 2880, 495 bytes across 3 chained fragments) entered `called` because of §5 and is unported. | Wave 29 Graphics lane |
| R7 | `MaterialInstance::BuildParameters` has attendance **0** in a full autostart world-build run, contradicting Wave 27's "goes live immediately" (§9.3). Five ported rows hang off that route. | Wave 29 Graphics lane |
