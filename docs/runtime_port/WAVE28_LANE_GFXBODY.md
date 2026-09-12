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
  row. So these registrations are LIVE the moment they land — this lane has no
  inert rows, and a zero attendance count from one of them is a finding, not an
  expected null.
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

Seven of the ten bodies are ported and registered. §7 says what did not and why.

| Row | State | Notes |
|---|---|---|
| `Material::GetConstantName` | PORTED, REGISTERED | §1 of the source |
| `Material::GetTextureName` | PORTED, REGISTERED | §1 |
| `MaterialInstance::FindConstant` | PORTED, REGISTERED | §2 |
| `MaterialInstance::FindTexture` | PORTED, REGISTERED | §2 |
| `Animation::ApplyEvents` | PORTED, REGISTERED | §3 |
| `Renderer::BeginDrawGroup` | PORTED, REGISTERED | §4, one address door (R1) |
| `AnimationState::UpdateRootOnly` | PORTED, REGISTERED | §5 |
| `AnimationState::CopyInstancedToUnique` | NOT PORTED | §7 |
| `AnimationState::UpdateNodes` | NOT PORTED | §7 |
| `Scene::AddCulledDrawItems` | NOT PORTED | §7 |

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

### 9.2 NOT MEASURED — UNVERIFIED

**No runtime verification of any of the seven ported bodies was run. The game
was not launched.** That word, plainly: the seven bodies in this lane are
**UNVERIFIED**.

Each has a live route and each is worth a different oracle, so the difference is
recorded rather than flattened:

- `FindConstant` / `FindTexture` / `GetConstantName` / `GetTextureName` run
  inside `MaterialInstance::BuildParameters`, which Wave 27 recorded as having
  **four name doors** and running during world build. A plain `sa-run` exercises
  them heavily; the attendance counters in this file would report it in one run.
- `BeginDrawGroup` runs from `Renderer::Draw`, the every-frame 3D dispatch.
  `tools/trace_oracle/` compares the draw-call/pipeline-state COMMAND STREAM and
  needs no frame determinism — it is the right instrument and it was not run.
  **A pixel diff is not the alternative**: Wave 23 measured two runs at identical
  seed and pin splitting ~98% apart independently of the flag under test.
- `UpdateRootOnly` is reached only on the off-screen cheap path, so `calls=0` on
  an autostart run would be an ordinary result and not evidence of a dark port;
  `g_updateRootLayers` counts the blend arm separately for that reason.
- `ApplyEvents` fires only when an animation event falls inside the frame's time
  window. `g_applyEventsFired` is separate from `g_applyEventsCalls` because
  `calls=N fired=0` and `calls=0` are different findings and an entry counter
  alone cannot distinguish them
  (`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).

When it is run, **read the census `calls=` column and not only
`divergences=`** — a port with zero callers reports clean.

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

## 11. CENSUS DELTA — attributed row by row

Filled in on the closing run; see §12.
