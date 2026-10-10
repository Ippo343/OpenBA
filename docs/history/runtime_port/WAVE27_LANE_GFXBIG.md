# WAVE 27 LANE GFXBIG — the three large Graphics BODY rows

Branch `wave27/gfxbig`, cut from `wave27/base` at `c6f30c24`.
Source: `runtime/src/graphics/wave27_lane_gfxbig.cpp`, `runtime/types/graphics/AnimationState.h`.

## 0. HEADLINE

**All three bodies ported and registered.** Three unexported internals they reach
are NOT ported and are called at their retail addresses behind documented doors,
each with a named addressee (§6).

**Two findings outlive the three bodies.**

**§11.1 — this lane grew the Graphics `BODY && ported==0` list from 4 to 11, and
that is a WIN, not a regression.** All seven new rows are direct callees my three
bodies introduced into the `called` column. Do not read the arithmetic as a loss;
the true remaining surface just became visible, and it is now seven *named
exports* instead of an unknown.

**§5 — the instrument findings:**
`ghidra-mcp`'s `disassemble_function` returned **a different function's listing
under the addresses that were asked for**, with no warning, on one of the three
target rows. The lane brief's instruction — "prefer `disassemble_function` over
`decompile_function`" — was correct about the *decompiler* and silent about the
case that actually bit. Everything in this lane was re-taken from `objdump`.

## 1. SCOPE, RE-DERIVED (MEASURED)

`tools/runtime_census.py --unported --csv`, run on this tip, filtered to
`ported==0 && symbol_kind==BODY && namespace==Graphics`:

```
?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z   <- lane GFXMESH, not mine
?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
?Update@AnimationState@Graphics@@QEAA_N...
```

Four rows, not three. The brief named three and the fourth belongs to GFXMESH; I
left it alone. Whole-namespace BODY counts on this tip, from the same run:
`System 7, Graphics 4, IO 4, Achievement 1, Audio 1, Engine 1`.

Addresses and sizes, from `pefile` over the shipped
`C:\GOG Games\Banished\Runtime-x64.dll` (MEASURED, not quoted from any doc):

| Export | Retail | Ord | Bytes |
|---|---|---|---|
| `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | 1678 | 1421 |
| `?Draw@Renderer@Graphics@@…` | `0x180104bb0` | 2258 | 920 |
| `?Update@AnimationState@Graphics@@…` | `0x1800fdb20` | 4474 | 1062 |

## 2. GREP-BEFORE-WRITE (MEASURED) — and what it found

Warning #3. All three were grepped across `code/ engine/ runtime/ standalone/
dx11/` before a line was written. **No body existed for any of the three.** What
did exist, and mattered:

- `runtime/src/graphics/wave9_lane_gfxd.cpp` — ported `Renderer::Reset` and
  `Renderer::AddScene`, i.e. **two of Draw's five callees were already mine to
  call by name.** Its banner also records that four of the five are exports.
- `runtime/src/graphics/wave23_lane_gfxbody.cpp` — ported
  `AnimationManager::Update`, **the only caller of `AnimationState::Update`**, and
  had already read the nine-argument call site from the disassembly. That gave me
  an independent second derivation of my argument mapping (§4).
- `runtime/types/graphics/Material.h` (Wave 6 lane F) — **already pinned from
  `BuildParameters` itself.** Every field it names beside that function held
  against the disassembly. No correction needed.
- `runtime/types/graphics/Camera.h` (Wave 6 lane F) — `Renderer`,
  `RendererSceneEntry`, `RendererDrawGroup`, `RendererDrawable`,
  `RendererDrawItem`, **all pinned from `Renderer::Draw` itself**, header says so
  explicitly. All held. No correction needed.

So two of the three bodies needed **no new type work at all**. That is the
measured payoff of grepping first, and it is worth recording in the positive
direction: the rule usually gets cited after it catches a duplicate port, and
here it saved the layout phase of two functions.

Only `AnimationState::Update` needed a new header.

## 3. THE GFXR LESSON RE-RUN (MEASURED)

`wave9_lane_gfxd.cpp` insists that a source grep records an unnamed internal
where the export table has a name. Every callee of all three bodies went through
`pefile` first. **Of sixteen distinct callees, twelve are ordinary named
exports.** Full table in the source banner.

Four of those names **decided an argument mapping the prologue could not** — e.g.
`?UpdateNodes@…QEAAXMAEBV?$Array@…` spells a **float first**, which is what
identifies `MOVAPS XMM1,XMM6` as argument 2 rather than a spill. Two more
(`Material::GetTextureName` / `GetConstantName`) spell `?AVString@System@@` as a
**by-value return**, i.e. a hidden return slot in RDX — the exact
`reference_hidden_retslot_wild_write.md` trap the brief flagged, answered from the
export table with no reversing.

## 4. THE NINE-ARGUMENT SIGNATURE, SETTLED THREE WAYS (MEASURED)

The brief said arguments past the fourth are on the stack and Ghidra will get
some wrong, and to verify against call sites. I used three derivations:

1. **The exported mangled name** spells all eight parameters and their types.
2. **The caller** (`AnimationManager::Update`, ported in Wave 23) passes them in
   exactly that order.
3. **Home-slot arithmetic of this body, computed and checked.** Four pushes
   (`0x20`) plus `SUB RSP,0x98` puts the caller's frame base at `RSP+0xB8`, so
   `[RSP+0xD8]`=arg4, `[RSP+0xE0]`=arg5, `[RSP+0xE8]`=arg6, `[RSP+0xF0]`=arg7,
   `[RSP+0xF8]`=arg8, `[RSP+0x100]`=arg9. **Every one of those six is loaded at
   exactly the offset the arithmetic predicts**, and `[RSP+0xF0]` is the pointer
   whose three floats are zeroed in the prologue — which is what the mangled name
   calls `AEAVVector3@8@`.

**No hidden return slot.** The return is `_N` in AL (`MOV AL,0x1` / `XOR AL,AL`),
so the four register arguments are not shifted. Checked, absent.

**A runtime global that looks like a float constant.** `MULSD XMM0,[0x1801fe728]`
reads like a literal. It is **`System::Timer::_secondsPerTick`**: RVA `0x1fe728`
is offset `0x8728` in `.data`, whose `SizeOfRawData` is `0x5400` — the
uninitialised tail. There are no bytes in the file to lift. A port that had baked
in "the constant" would have baked in **zero**, and every tick→seconds conversion
in the body would have collapsed. Found by trying to read the bytes and getting
none; the tree already resolves this global at
`engine/funcs/engine_funcs.cpp:1546`. The genuinely-static `1.0f` two lines away
(`0x18016169c`, in `.rdata`) *was* read as bytes: `00 00 80 3f`.

## 5. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Warning #1. Recorded, not fixed.

### 5.1 `ghidra-mcp disassemble_function` silently returns the wrong function

**`tools/ghidra_mcp/bridge_mcp_ghidra.py`** — the `disassemble_function` endpoint
(server side; the bridge only forwards `address`).

`disassemble_function(address="0x1800fdb20")` returned **126 instructions
spanning `0x1800fd9a0`..`0x1800fdb58`** — an `Array<T>::SetSize` of stride
`0x18`, a different function — while printing them under the addresses that were
asked for. It reported the instruction at `0x1800fdb20` as `LEA RAX,[RBX + -0x8]`.

The bytes in the shipped file at `0x1800fdb20` are

```
4c 89 4c 24 20   48 89 54 24 10   53 55 56 41 55   48 81 ec 98 00 00 00
MOV [RSP+0x20],R9 / MOV [RSP+0x10],RDX / PUSH RBX / PUSH RBP / PUSH RSI / PUSH R13 / SUB RSP,0x98
```

a prologue, which is what an export entry has to be.

**Cause**, visible from Ghidra itself: it holds **two overlapping function
bodies** here — `get_function_by_address("0x1800fd9a0")` returns
`FUN_1800fd9a0` with body `1800fd9a0 - 1800fdb58`, and
`get_function_by_address("0x1800fdb20")` returns `Update` with body
`1800fdb20 - 1800fdf45`. The endpoint resolves an address to the first body that
contains it.

**Why this is worse than the known `get_function_metrics` defect**
(`reference_ghidra_mcp_address_param_silently_ignored.md`): that one ignores
`address=` and falls back to a name, which at least produces *a* coherent
function. This one produces a coherent function **and relabels its instructions
with the addresses you asked about**, so the output passes every internal
consistency check a reader can apply. Nothing in it is wrong-looking. I only
caught it because `?Update@AnimationState@` is supposed to have a nine-argument
prologue and the listing began with `MOV [RSP+0x18],R8` on a *three*-argument
shape — a suspicion available only because the mangled name had already been read.

**The generalisable form:** *a screen that relabels borrowed data with the
identifier you supplied cannot be caught by reading its output* — the
identifier you asked about is present, so the only available check is against a
source outside the instrument. `get_function_by_address` on the *neighbouring*
address is that check and it costs one call.

**Detection cost, for whoever fixes it:** the endpoint has the entry address in
hand. `entry != requested` is a one-line warning.

### 5.2 The profile/retail program-confusion defect — CHECKED, LANE IS CLEAN

Mid-lane the integrator relayed lane FRONTIER's finding that GhidraMCP may silently
serve `Runtime-x64-profile.dll` while `list_open_programs` reports retail as
current, and that `switch_program` can return `{"success": true}` having changed
nothing. Recorded here, not fixed (warning #1).

**MEASURED on this lane, with the one-call discriminator
`disassemble_function(0x180002190, program="Runtime-x64.dll")`:**

```
180002190: MOV RAX,qword ptr [0x1801fae10]
180002197: RET
```

Two instructions — the **RETAIL** shape. Not the ~40-instruction profile shape.

**Why this lane was largely insulated, stated as a fact about method and not as
luck:** after the §5.1 defect I had already stopped taking disassembly from
Ghidra entirely. All three bodies' listings, every address, every ordinal, every
extent cross-check and every data byte came from `objdump` and `pefile` run
directly against `C:\GOG Games\Banished\Runtime-x64.dll` **by explicit path**.
A tool that cannot choose the file cannot serve the wrong one.

**ONE BODY WAS EXPOSED, AND IT WAS RE-TAKEN AND DIFFED RATHER THAN ASSUMED
SAFE.** `BuildParameters` was the first body I read, and it was read through
`disassemble_function` *before* §5.1 was discovered and *without* an explicit
`program=`. FRONTIER recorded its five bodies being identical across builds as
**luck, not a control**, and that is the right reading — so I did not repeat it.
I re-dumped `0x1801033e0`..`0x18010396d` from the retail file and compared every
load-bearing instruction:

- entry `MOV [RSP+8],RCX`; the null-material `JE 0x18010395a`; the
  `JLE 0x18010390c`; the texture skip flag `CMP BYTE PTR [RDX+R12*1+0xc],0x0`
- all four helper calls at their expected targets (`0x180060d30`, `0x180102900`,
  `0x180103020`, `0x180102830`, `0x180102f40`)
- **`MOV R8D,0x4` at BOTH `0x1801035c4` and `0x1801037ec`** — the align-4 finding
  in §7 that `kSpec_Binding8` exists for, confirmed on retail
- the whole latch block `0x18010390c`..`0x180103958` byte for byte, including the
  `+0x24` clear / `+0x25` *no*-clear asymmetry and the re-read of `[RCX]` between
  the two comparisons

**No divergence, and — the check that mattered most for a hot path — no call
target anywhere in the body that is not already accounted for.** There are no
profiling-hook calls. The integrator's warning that a profile-derived
`Renderer::Draw` could contain hooks that do not exist in retail is exactly right
as a risk; `Renderer::Draw` was never read from Ghidra at all, and
`BuildParameters`, which was, is hook-free on retail.

**Also recorded, not fixed** (relayed from FRONTIER, not independently measured
here — flagged as inherited): `list_segments` reports section bounds that
contradict the retail PE headers, placing the `.rdata` vtable `0x180154630`
inside `.text`. This lane took no section-bounds answer from Ghidra; the one
section question it had (is `0x1801fe728` in `.data`'s raw range or its
uninitialised tail?) was answered from the PE headers with `pefile` — §4.

### 5.3 `runtime_census.py --csv` header does not match its documented field name

`tools/runtime_census.py` — `--csv` writes the columns
`kind,key,namespace,ported,detoured,callers,symbol_kind`. The brief (and,
reasonably, a reader) expects a `symbol` column; the symbol is in `key`. Cost me
one failed filter. Not fixed.

## 6. RESIDUALS — three address doors, one addressee

Each is commented at its call site in the WAVE27_KICKOFF §4 #10 shape
(SYMBOL / CONDITION / CONSEQUENCE). None is a vtable slot; none belongs to
another lane's table. All three were confirmed absent from the export directory
**with pefile**, not inferred from a comment
(`reference_aeaa_is_private_not_unexported.md`).

**ADDRESSEE FOR ALL THREE: the Wave 28 Graphics lane.** Not "the next lane that
touches this" — that clause waits forever
(`reference_expiry_clause_needs_an_addressee.md`).

| # | Symbol | Bytes | Condition | Consequence at cutover |
|---|---|---|---|---|
| R1 | `0x180062d60` draw-item sort | 2311 | any pass list with ≥2 items — essentially every frame | draw items go out **UNSORTED**: transparency and depth artefacts, **not a crash**. Blocks `Renderer::Draw` running DLL-free. |
| R2 | `0x18005aa30` `Array<Node>::SetSize` | 403 | a fading layer expires **and** its manager group refcount hits 0 | the group's node buffer **leaks**; header keeps a stale pointer. No crash, invisible in a short run. |
| R3 | `0x18005b1e0` `Array<AnimationState>::SetSize` | 382 | same arm as R2 | same as R2 for the state buffer. |

R2 and R3 are **plain `SetSizeCore` instantiations with a non-trivial element**,
so the cheap route is two more `ArraySpec` rows in the style of §7, not two
hand-written bodies. R1 is a real 2.3KB sort and is its own piece of work.

**R1 is the one that gates anything.** R2/R3 leak on a rare arm; R1 changes what
is on screen on every frame.

## 7. THE ONE UNEXPORTED BODY THIS LANE DID PORT

`0x180060d30`, `Collection::Array<MaterialInstance::MaterialParameterSet>::SetSize`
(420 bytes, not exported). `BuildParameters` cannot run without it, and it is a
plain `rtcoll::SetSizeCore` instantiation, so it is a new `ArraySpec`
(`kSpec_MatPass`) rather than a new loop — the reason `array.cpp` gives for
refusing private copies of `SetSizeCore`.

Its three element helpers were all read in full: `0x180005800` turned out to be
**instruction-for-instruction lane Q's existing `ArrayDtorImpl`**, so it is
called rather than rewritten.

**A trap caught here, worth the sentence:** the append `BuildParameters` inlines
for the binding arrays is element stride 8 — which *looks* like the existing
`kSpec_Fill8` — but the allocation is `MOV R8D,0x4`, **align 4, not 8**.
`kSpec_Fill8` is align 8. Reusing the spec whose name and stride matched would
have changed the allocator call. The thing that has to match is the immediate in
the instruction, which is `array.cpp`'s own stated rule; I checked it rather than
inheriting by resemblance. `kSpec_Binding8` is a separate row for that one reason.

`kSpec_MatPass` has `byteCopy=false` and `seedZero` over the whole element,
because both sub-array `_data` pointers are handed to `MemFree` — a probe seeding
non-zero there would hand the allocator wild pointers. **No probe drives
kSpec_MatPass today**, stated so a correctly-filled field is not read as evidence
that one does.

## 8. PER-BODY STATUS

### 8.1 `?BuildParameters@MaterialInstance@Graphics@@` — PORTED, REGISTERED

- **Ported**: yes, in full. No address door of its own beyond §7.
- **Registered**: `RegisterRuntimeExport` by name.
- **Live**: **four name doors already in the tree** — `engine_funcs.cpp:3492`,
  `standalone/reg_unreg_lane3.cpp:154` (exe dispatch slot),
  `runtime/src/save/saveload_draw.cpp:596`, `wave24_lane_gfxmat.cpp:656`. This row
  goes live immediately; it is not one of the inert ones.
- **Two retail asymmetries transcribed rather than tidied**, both of which a
  cleanup would silently "fix":
  - `+0x24 hasRenderTexture` is **cleared** before the latch loop and recomputed;
    `+0x25 hasRenderedTexture` is **not** — it is loaded and OR'd into. So a
    second `BuildParameters` on the same instance can only ever *add* to `+0x25`.
  - the null-material early return skips the latch loop **as well as** the pass
    loop, so it does not clear `+0x24` either.
- **One instruction deliberately not reproduced, and named**: retail tests
  `data - 8 == 0` before freeing a released `String` (the ZF of `SUB RDI,0x8`),
  true only if `data == 8`. The shared `RtString_ReleaseInline` relies on the null
  test inside `0x1800c7ad0` instead. Recorded rather than quietly called identical.

### 8.2 `?Draw@Renderer@Graphics@@` — PORTED, REGISTERED, one door (R1)

- **Ported**: yes. Declined by lanes GFX, F, GFX2, GFXR (Wave 6/9) and by every
  lane since; the reason it is tractable now is that Wave 9 GFXD ported `Reset`
  and `AddScene` and Wave 6 lane F pinned all five structs **from this body**.
- **Registered**: by name. `code/Game/Game/gameupdateloop.cpp:628` and
  `engine_funcs.cpp:3615` both resolve it — **this is the every-frame 3D render
  call**, so it is live on the first frame.
- **Structure, stated because it is the thing a reader gets wrong:** two passes,
  and **the second runs BACKWARD** (`DEC R13 / JNS`). Render-to-texture entries
  are produced before the entry that samples them, so a forward rewrite would
  sample a stale target. Transcribed as a decrementing signed counter.
- **Four kinds of virtual call, none of them a door and none of them a table this
  lane owns**: slot 3 on scene drawables (`AddDrawItems` — lane GFXMESH's row,
  left entirely alone), slot 4 on `Context::_contextData` (Clear), slots 9 and 10
  on renderer drawables.
- **R1** (the sort) is its only unported dependency.
- **A retail quirk preserved**: `Renderer::field_3c` is read in pass 1 **before
  anything writes it** on the first frame — `Camera.h` already documents that the
  ctor never writes it. The port does not zero it.

### 8.3 `?Update@AnimationState@Graphics@@` — PORTED, REGISTERED, two doors (R2/R3)

- **Ported**: yes. New header `runtime/types/graphics/AnimationState.h`
  (`AnimationState` 0x48, `AnimationLayer` 0x38, `AnimationClipPrefix`,
  `InterpolatedTransform` 0x38), every field carrying the instruction address
  that pins it and every unpinned field left as an explicit named hole.
- **Registered**: by name. Reached from `wave23_lane_gfxbody.cpp:555` inside the
  ported `AnimationManager::Update`, so it is live on every sim iteration with an
  animated mesh registered.
- **MEASURED vs INHERITED is separated inside the header**, not just here: the
  three fields `Update` reads (`+0x00`, `+0x08`, `+0x10`) are measured with
  addresses; the 0x48 **stride** and the meaning of `+0x18`/`+0x28`/`+0x38` are
  **inherited** from Wave 23's read of the caller, and the header says so and
  names the file. `Update` never touches those three, so this lane cannot claim to
  have re-derived them.
- **`layerCapacity` (+0x14) has no reader in this body** and is named by position
  in the Array triple, not by measurement. Said in the header too.
- **Float association order preserved exactly** at the one place it matters: the
  loop-restart offset is `((num / den) * rate) * loopScale` — three separate
  instructions (`DIVSS`, `MULSS`, `MULSS`) in that order. Re-associating is
  algebraically identical and numerically different.
- **`loopFrame == 0` short-circuits the whole fraction**, which is *not* the same
  as computing it and getting zero: with `frameLast == frameFirst` the division
  would be 0/0. Retail's branch is kept.
- **A non-terminating loop is reproduced, deliberately.** The loop wrap is
  retail's `do { t -= duration; } while (t >= duration)`, not a modulo. For
  `duration == 0` that does not terminate — **in retail too**. Per the standing
  rule, a zero-duration looping clip is a data defect retail would also hang on,
  and a guard would hide it rather than restore behaviour.
- **A no-op transcribed as a no-op, and named:** at `0x1800fdc50..0x1800fdc7c`
  retail loads seven dwords and stores them back to the **same seven addresses**
  (MSVC's rendering of `a = a` on an inlined `Transform` assignment) before doing
  the real copy. Omitted, with the reason in the source.
- **Integer moves of float storage** on the dt==0 snapshot, for Wave 23's reason:
  bit-exact vs merely value-exact, and the two differ on a signalling NaN and on
  negative zero.

## 9. VERIFICATION — WHAT I ACTUALLY RAN, AND WHAT I DID NOT

Stated in the two halves warning #6 demands.

### 9.1 MEASURED

- **`mingw32-make standalone-debug` exits 0.** (§10.)
- **Export directory, `pefile`**: all three target names present at the addresses
  and ordinals in §1; all sixteen callees classified exported / not-exported;
  all three residual addresses confirmed **absent** from the directory.
- **PE `.pdata`**: extents of all three bodies and of the three residuals; the
  five-fragment chain on `AnimationState::Update`.
- **Raw file bytes**: `0x18016169c` == `00 00 80 3f` (1.0f). `0x1801fe728`,
  `0x1801fe7b0`, `0x1801fc718` proven to lie **beyond `.data`'s `SizeOfRawData`**
  — i.e. runtime-initialised, no bytes to lift.
- **Cross-check of the instrument defect** from two sides:
  `get_function_by_address` on `0x1800fdb20` and on `0x1800fd9a0` return
  overlapping bodies; the raw bytes decide which is the entry.
- **Grep of the whole tree** for all three bodies and for every callee address,
  before writing — results in §2.
- **`0x180060d30` occurs nowhere** in `code/ engine/ runtime/ standalone/`, so its
  `RegisterRuntimeAddr` is neither a shadowed address nor an identity token
  (the `?AddDrawItems@MeshInstance@` hazard checked for and absent).

### 9.2 NOT MEASURED — and why, per body

**No runtime verification of any of the three bodies was run. I did not launch
the game.** Stating that plainly rather than describing what was available.

The reasons differ per body and the difference matters:

- **`Renderer::Draw`** — the instrument the brief names, `tools/trace_oracle/`,
  compares the draw-call/pipeline-state command stream and is exactly right for
  this body. **It is the correct next step and I did not run it.** The brief also
  says only three of its six screens are verdict-bearing, and its README's
  measured noise floor has to be read before quoting any of them. A pixel diff is
  *not* the alternative: Wave 23 measured two runs at identical seed and pin
  splitting ~98% apart **independently of the flag under test**, so a pixel diff
  between two runs measures nothing.
- **`BuildParameters`** — has four name doors and runs during world build, so a
  plain `sa-run` would exercise it heavily; the useful oracle is a shadow
  comparison of the `{sourceIndex, shaderSlot}` pairs against the DLL's, which is
  buildable but was not built here. **Note the standing trap before someone
  does**: ~250 `RT_DIFF_SHADOW` sites are **self-checks, not oracles** — they take
  the port's own verdict and never consult the DLL
  (`reference_shadow_marker_is_not_an_oracle.md`). A real oracle here has to call
  `0x1801033e0` **by raw address** on a snapshot and compare, and the snapshot must
  copy the `MaterialInstance` *and* its `_parameters` buffers, because the body
  mutates them in place.
- **`AnimationState::Update`** — the DLL's `AnimationManager::Update` is already
  our body, so the call site is ours and a shadow is straightforward. Its
  interesting arm (layer expiry, R2/R3) is **rare**, which is why
  `g_animStateExpired` counts it separately from the door:
  `calls=N expired=0` is a different finding from `calls=0`, and an entry counter
  alone cannot distinguish them
  (`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).

**When the verification does run, read `calls=` and not only `divergences=`**, and
snapshot aliased inputs — a correct port once read as a 10% divergence because a
call site passed one buffer as both self and return.

## 10. BUILD

`mingw32-make standalone-debug` — see §11 for the recorded result.

Kill switch: **`BANISHED_RT_GFXBIG=0`** leaves all three on the DLL.
`rt_graphics_port.cpp` explains why it is its own variable and not shared: it is
the switch in that file with the widest blast radius, because `Renderer::Draw` is
the entire 3D frame.

## 11. RESULT OF THE BUILD (MEASURED)

`mingw32-make standalone-debug` — **exit 0**, `standalone/build/reimpl-x64.exe`
32M. One compile error on the first attempt (`Video::` vs the header's actual
`RtVideo::` namespace), fixed; no warnings from this lane's two files in the
final build.

Census re-run on the built tip:

```
Graphics            599      123       97        1   78.9%   95.9%
TOTAL              4632     1385     1240        9   89.5%
```

**READ THAT AGAINST THE KICKOFF'S NUMBERS WITH CARE, NOT AS A DELTA.** The
kickoff's board is `1372 called / 1237 ported / 90.2%`; the run above prints
`1385 / 1240 / 89.5%` on a different column set. **Do not subtract them.** Three
things move at once here and only one of them is my three bodies:

1. my three rows enter `ported`;
2. my `RegisterRuntimeAddr(0x180060d30)` and the doors in §6 change what is
   *called*, and porting can GROW the called column
   (`reference_porting_can_grow_the_cutover_surface.md`);
3. six other lanes are landing on the same base.

A ceiling percentage has `called` in **both** halves, so any lane opening a door
moves it (`reference_the_ceiling_percentage_is_a_moving_target.md`). **The
integrator should re-derive the board on the merge, not add my three to the
kickoff figure.** The only claim I make about the board is the identity one:
**three named BODY rows that were `ported==0` are now ported and registered, and
one unexported Array instantiation with them.**

### 11.1 THIS LANE GREW THE GRAPHICS BODY GAP FROM 4 TO 11. THAT IS NOT A REGRESSION.

Identity check on the built tip — the three target rows now read `ported=1`:

```
1 BODY ?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
1 BODY ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
1 BODY ?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@UNode@…
```

And the Graphics `BODY && ported==0` list, which was **four** rows at §1, is now
**eleven**:

```
?AddCulledDrawItems@Scene@Graphics@@         <- called by my Draw
?AddDrawItems@MeshInstance@Graphics@@        <- lane GFXMESH's, unchanged
?ApplyEvents@Animation@Graphics@@            <- called by my AnimationState::Update
?BeginDrawGroup@Renderer@Graphics@@          <- called by my Draw
?CopyInstancedToUnique@AnimationState@       <- called by my AnimationState::Update
?FindConstant@MaterialInstance@Graphics@@    <- called by my BuildParameters
?FindTexture@MaterialInstance@Graphics@@     <- called by my BuildParameters
?GetConstantName@Material@Graphics@@         <- called by my BuildParameters
?GetTextureName@Material@Graphics@@          <- called by my BuildParameters
?UpdateNodes@AnimationState@Graphics@@       <- called by my AnimationState::Update
?UpdateRootOnly@AnimationState@Graphics@@    <- called by my AnimationState::Update
```

**Every one of the seven new rows is a callee my three bodies introduced.** They
were always in the DLL and always unported; they were simply not in the `called`
column because nothing in our tree named them. Porting a body that calls them put
them there — `reference_porting_can_grow_the_cutover_surface.md`, which this lane
was told about in the abstract and has now produced a large concrete instance of.

**Stated loudly because the arithmetic looks like a loss.** A reader diffing the
Graphics BODY gap across this merge sees 4 → 11 and concludes the lane went
backwards. The honest reading is the opposite: **three bodies were ported and the
true remaining surface became VISIBLE.** The seven rows were unported before this
lane and are unported after it; only their visibility changed.

It also makes the next Graphics lane's scope concrete and cheap to state: seven
named exports, all of them direct callees of bodies that now exist in our tree,
plus the three unexported residuals in §6. That is a far better handover than "the
Graphics BODY gap is 4".
