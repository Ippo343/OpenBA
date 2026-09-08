# Wave 9 — lane GFX (Graphics / Renderer / Utility)

Cutover boot attempt 06 (Runtime-x64.dll moved aside) left **61 symbols resolving to
nothing**. Nine of them were handed to this lane. Four are now closed (plus two callees
ported so they close rather than move), five are declined with evidence, and **four of
Wave 6's written holdback reasons turned out to be false or stale** — recorded below in the
same detail as the ports, because the wrong reason is what kept them shut for two waves.

Date: 2026-09-08. Branch: `wave/upgrade-path-complete`.

---

## 0a. Base-commit check — done, and it had already bitten

This worktree was created from **`965361d`**, which predates the entire runtime port: no
`docs/runtime_port/` directory at all, an old `CLAUDE.md`, **591 commits behind**. That was
caught in this lane's first three tool calls (`ls docs/runtime_port/` failed) and fixed with
`git reset --hard wave/upgrade-path-complete` **before any analysis was done**. Nothing in
this write-up was concluded from the stale tree.

The work was then rebased twice more as other lanes merged, finally onto **`e86a5a7`**
(lane ENG's merge), so it sits on top of lanes VID, UIF, IOA and ENG. **No file this lane
touches was modified upstream by any of them** — `git diff --stat` over
`runtime/src/graphics`, `runtime/types/graphics`, `runtime/src/math`, `runtime/src/utility`,
`runtime/types/utility`, `runtime/types/math` and `runtime/types/collection` shows only
this lane's own changes at every base — so each rebase was a clean replay and no conclusion
needed revisiting. **All audit numbers in §7 were re-measured at `e86a5a7`**, not carried
over from an earlier base, so the deltas are this lane's and not lane ENG's.

### The stash incident — reported, and nothing was lost

`git stash push -u`, used once to take a clean "before" measurement, lost this lane's two
new files: **`git stash` is repository-global, not per-worktree**, and with five lanes
running concurrently lane ENG's `stash pop` took this lane's entry into its own tree. It
was detected immediately (`git stash pop` → "No stash entries found") and the two files
were rewritten from source before the coordinator's warning arrived.

Lane ENG also saved copies to
`…/scratchpad/laneGFX_rescued/`. Those were diffed against the rewritten files:
**substantively identical** — the only differences are whitespace on two declarations, one
dropped unused `using`, and three comment lines that the rewrite improved. Nothing was
lost and no conclusion in this document rests on a file that was absent while the tree was
being read (the loss happened *after* all analysis was complete, during measurement).

**Do not use `git stash` in a parallel-agent worktree.** A scratch commit on your own
branch (`git commit -m wip`) does the same job and is not shared mutable state. Every
subsequent measurement in this lane used that instead, plus a copy of both files in the
lane's own scratchpad.

## 0b. The dx11 import check — none of this lane's nine is in that set

Lane VID's finding (one unported Runtime export disables an entire all-or-nothing dx11
import table, and every `ContextData` slot then forwards to a VideoDX11 DLL this project
never loads → EXEC-of-0 on the first draw) is the right thing to check first, since this
lane owns the per-frame path. Ran `docs/runtime_port/wave9_laneVID_dx11scan.py`:

> `dx11 Runtime-import names seen: 70 ; UNSATISFIED (no port, no mint): 21`

**None of this lane's nine targets appears in those 70 names, satisfied or not.** The whole
dx11 import set is `Video::Device` / `Video::Context` / `Video::RenderBuffer` /
`System::WideString` / `System::Error` / `Application` — this lane's targets are all
`Graphics::` and `Utility::`, on Runtime's own side of the Strategy/Bridge seam. So there is
**nothing here to re-scope**: no decline in §6 is silently withholding a table entry, and
the four ports below buy residue rows rather than table gates. The 21 unsatisfied names
remain lane VID's.

Separately, `dx11/` already replaces `VideoDX11-x64.dll` in full, and it implements the
`*Data` **companion** classes. Nothing in this lane was ported twice.

---

## 0c. Verdict table

| # | Symbol | Retail | Verdict |
|---|---|---|---|
| 1 | `??0PerlinNoise@Utility@@QEAA@MMHH@Z` | `0x180108c70` | **PORTED — closes the row** |
| 2 | `?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z` | `0x18001aff0` | **PORTED — closes the row** |
| 2b | `?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z` | `0x18001d690` | **PORTED** — #2's only callee; not on the 61 list, ported so #2 closes instead of moving |
| 3 | `?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z` | `0x1801057b0` | **PORTED — closes the row** (one stated caveat, §5) |
| 3b | `?RenderTextures@MaterialInstance@Graphics@@QEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z` | `0x180102df0` | **PORTED** — #3's only callee; same reason as 2b |
| 4 | `?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z` | `0x180104bb0` | **DECLINED** — §6.1 |
| 5 | `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | **DECLINED** — §6.2 |
| 6 | `?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z` | `0x1800fc240` | **DECLINED** — §6.3 |
| 7 | `?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z` | `0x1800ff680` | **DECLINED** — §6.4 |
| 8 | `??0SunLight@Graphics@@QEAA@XZ` | `0x1801081f0` | **DECLINED** — §6.5 |
| 9 | `?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z` | `0x180107f10` | **DECLINED** — §6.5 |

Every one of the eleven mangled names above was verified against the shipped DLL's export
table with `pefile` **before** a body was written. All eleven are present. (The one that
would have bitten: `?Lerp@Transform@Math@@…` is not resolved by any file in this tree, so a
typo in it would have registered silently and never fired —
`reference_resolveexport_phantom_names.md`.)

---

## 1. `Utility::PerlinNoise::PerlinNoise(float,float,int,int)` — the body was already
   written, and unregistered

Wave 8 lane E **wrote this ctor** into `runtime/src/utility/perlin.cpp` and deliberately
left it `[[maybe_unused]] static`, on the reasoning that "a caller that needs a NEW
PerlinNoise is out of this lane's scope". Cutover attempt 06 disagrees:
`engine/funcs/engine_funcs.cpp:3493` resolves it as `fn_PerlinNoise_Ctor` and
`docs/runtime_port/wave9_residue_tutorial3.txt` caught it requested live. Unregistered, it
is a null function pointer waiting for the terrain generator.

This is another instance of `reference_holdback_blockers_are_stale_check_them_first.md`:
the fact needed to close the row was **already in the tree**. Cost to close: two lines of
registration.

The body is unchanged and was re-verified against the retail bytes. The whole function:

```
180108c70: MOV   EAX,[RSP+0x28]      ; 4th argument, out of its home slot
180108c74: MOVSS [RCX],XMM1          ; +0x00 = 1st (float)
180108c78: MOVSS [RCX+0x4],XMM2      ; +0x04 = 2nd (float)
180108c7d: MOV   [RCX+0xc],EAX       ; +0x0c = 4th (int)
180108c80: MOV   RAX,RCX             ; return `this`
180108c83: MOV   [RCX+0x8],R9D       ; +0x08 = 3rd (int)
180108c87: RET
```

**Bit-exactness:** not applicable — there is no arithmetic. Four stores of four argument
registers.

**Oracle:** none, and none is possible or useful. A scratch replay would compare four
stores of the same four registers against four stores of the same four registers: true by
construction (`reference_recheck_must_match_its_own_proof.md`). The verification is the
listing above, and it is **total** — a field this port missed would be a field that is not
in the disassembly. What is *not* verified is the field **names**, which remain Wave 8's
usage inference; a wrong name changes no emitted byte.

---

## 2. `InterpolatedState<Transform>::Lerp` — and a Wave 1 argument reading that was wrong

### 2.1 What Wave 1 said, and what the bytes say

`runtime/src/math/transform.cpp` has carried this since Wave 1:

> only ONE Transform operand (at +0x1c) plus a bare Scalar (at +0x00) reach
> `Transform::Lerp`, with no second Transform argument visible at this call site … Neither
> `Transform::Lerp`'s own address nor `InterpolatedState<T>`'s layout is pinned …

The whole function is eleven instructions:

```
18001aff0: PUSH  RBX
18001aff2: SUB   RSP,0x20
18001aff6: MOV   R9,R8      ; R9  = the Scalar& argument (t)
18001aff9: MOV   R8,RCX     ; R8  = this + 0x00     <-- THE SECOND TRANSFORM
18001affc: ADD   RCX,0x1c   ; RCX = this + 0x1c
18001b000: MOV   RBX,RDX    ; RDX = the hidden return slot
18001b003: CALL  0x18001d690
18001b008: MOV   RAX,RBX
…          RET
```

The second Transform is `MOV R8,RCX` — `this` itself. And `Transform::Lerp`'s address is
the CALL target; `pefile` confirms `?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z`
is exported at `0x18001d690`, ordinal 3193. Wave 1's layout worry was legitimate at the
time (Wave 6 lane E pinned `InterpolatedState<Transform>` at 0x38 afterwards); the
**argument** reading was not, and it is the half that stood unchallenged for eight waves.
The note in `transform.cpp` is corrected in place rather than deleted.

### 2.2 A second open question closed: which half is CURRENT

Wave 6 lane E §4.2 pinned the element at 0x38 (two adjacent 0x1c `Math::Transform`s) and
said plainly:

> **Which half is "current" is NOT pinned by this lane** … `InterpolatedState<T>::Lerp` is
> the function that settles it … nobody should read `_state[0]` as "current" on this
> lane's authority.

It settles it. `Transform::Lerp(from = this+0x1c, ret, to = this+0x00, t)` and
`Transform::Lerp` computes `from + t*(to - from)` (§2.3). Interpolation therefore runs
**from +0x1c to +0x00**, so **+0x00 is CURRENT and +0x1c is PREVIOUS** — the ordinary
render-interpolation arrangement, and consistent with `GetNodeToWorld` reading the first
one. `runtime/types/graphics/Mesh.h` now says so with the derivation attached.

### 2.3 `Math::Transform::Lerp` — the bit-exactness argument

Translation, from `0x18001d696..0x18001d6fa`, term for term:

| lane | retail | result |
|---|---|---|
| x | `XMM2 = to.x - a.x` ; `XMM1 = t` ; `MULSS XMM1,XMM2` ; `XMM8 = a.x` ; `ADDSS XMM8,XMM1` | `a.x + t*(to.x - a.x)` |
| y | `XMM4 = to.y - a.y` ; `MULSS XMM4,XMM0` ; `XMM7 = a.y` ; `ADDSS XMM7,XMM4` | `a.y + (to.y - a.y)*t` |
| z | `XMM3 = to.z - a.z` ; `MULSS XMM3,XMM0` ; `XMM6 = a.z` ; `ADDSS XMM6,XMM3` | `a.z + (to.z - a.z)*t` |

Two things are load-bearing and are preserved literally:

* **It is `a + t*(b - a)`, not `a*(1-t) + b*t`.** Those are algebraically identical and
  bit-different, and they differ exactly where the terms nearly cancel — the failure mode
  `reference_moved_term_hides_where_terms_cancel.md` documents at one divergence in 23,654
  calls. The port does not "simplify".
* The x lane multiplies `t*(b-a)` and the y/z lanes `(b-a)*t`. IEEE multiplication **is**
  commutative to the bit, so this asymmetry is preserved only so a future reader diffing
  against the listing sees the same shape; it cannot change a value.

Rotation is `CALL 0x18001d500` with RCX unchanged — and `Transform::rotation` is at +0x00,
so `this` doubles as `&this->rotation`. That is `Quaternion::Lerp`, **already ported by
Wave 9 lane P**, and it is called as a direct C++ call, not through `ResolveAddr`: an
address door here would walk straight past lane P's name registration into the DLL
(`reference_bypassed_port_needs_a_source_checker.md`).

**Hidden return slot:** both functions return `Math::Transform` **by value**, so RDX is the
return slot and the real arguments start at R8. The existing typedef
`fn_InterpolatedState_Transform_Lerp_t` at `engine/funcs/engine_funcs.h:6262` already
declares it correctly (`void* self, void* retval, void* scalar`) — checked, because a
typedef that omits the slot is a wild write that still returns the right answer
(`reference_hidden_retslot_wild_write.md`). `tools/abi_retslot_audit.py` reports **0
suspicious of 48** after this lane.

### 2.4 Oracle

Full byte compare of the 0x1c-byte result against the shipped body, on both functions.

* **Inputs are snapshotted before a single byte of `ret` is written.** `ret` is a hidden
  return slot the caller chose and is allowed to alias `self` or `other`; retail tolerates
  that because it reads every input into a register first, and so does this port. The
  *oracle* cannot: it runs the shipped body **after** ours has written. Without the
  snapshot the original is handed input we just destroyed and "diverges" on nearly every
  call — the artefact that nearly got a correct `Transform::operator*` deregistered
  (`reference_oracle_must_snapshot_aliased_inputs.md`).
* **No unwritten-byte gap.** `sizeof(Transform) == 0x1c` is exactly quaternion 0x10 +
  Vector3 0xc with no padding, and both sides write all of it. So a full-width compare has
  nothing of the `0xCD`-survives-in-theirs kind to trip on
  (`reference_oracle_must_not_compare_unwritten_bytes.md`).
* Expected tolerance: **0.0f — bit equality**, not a tolerance. `Quaternion::Lerp` calls
  `RtLibm::Sqrt`, which Wave 9 lane L proved bit-exact against the DLL's own CRT over 1.49B
  comparisons. There is no `pow`/`cos`-class residual here.

Live call site: `code/Game/Game/geometry_draw.cpp:367`, once per interpolated drawable per
frame — so this is a high-volume oracle, not a token one.

---

## 3. `Renderer::AddDrawItem` — Wave 6's reason (2) was checkable and false

Lane F held it back for two reasons. Quoting `WAVE6_LANE_F.md` §2:

> 1. **No safe oracle.** …
> 2. **It writes an element type this lane did not pin.** … the array helper it hands that
>    to has a stride this lane did not read out. Guessing it would put a wrong layout in a
>    header, which is worse than an absent one.

**Reason (2) is false, and it was falsifiable then.** The helper is `0x180063680`. It is
not a shared generic; it is `Collection::Array<T>::Resize` instantiated for **this** element
type, and its stride is the literal in its own body:

```
1800636d3: IMUL RCX,RCX,0x38            ; the byte size — the stride, in one instruction
180063715..180063747: seven qword MOVs  ; the element copy, 0x38 bytes
```

Nothing had to be guessed. `RendererDrawItem` is now written out in
`runtime/types/graphics/Camera.h` in place of lane F's forward declaration, with every
field pinned by the store `AddDrawItem` makes into the stack record it hands the helper:

| Off | Field | Store |
|---|---|---|
| +0x00 | `const void* geometry` | `[RSP+0x30] = RSI` (2nd argument) |
| +0x08 | `const void* material` | `[RSP+0x38] = R8` = `MaterialInstance+0x28` |
| +0x10 | `const void* instance` | `[RSP+0x40] = RDI` (3rd argument) |
| +0x18 | `uint16 flags` | `[RSP+0x48] = AX` |
| +0x1a | `uint16 arg5` | `[RSP+0x4a]` (5th argument) |
| +0x1c | `float sortKey` | `[RSP+0x4c] = XMM0` (6th argument) |
| +0x20 | `int32 arg8` | `[RSP+0x50]` (8th argument) |
| +0x24 | **`_pad24` — NEVER WRITTEN** | — |
| +0x28 | `const void* userData` | `[RSP+0x58]` (9th argument) |
| +0x30 | `uint32 arg10` | `[RSP+0x60]` (10th argument) |
| +0x34 | **`_pad34` — NEVER WRITTEN** | — |

The two gaps are real and are reproduced, not tidied: the helper copies all seven qwords
regardless, so **retail stores uninitialised stack bytes into the array at +0x24 and +0x34
on every single append**. The port's `RendererDrawItem rec;` is deliberately not
value-initialised for that reason, and the one comparison this lane does make skips those
two spans (`reference_oracle_must_not_compare_unwritten_bytes.md` — four such gaps in
Wave 6 alone read as catastrophic port defects).

### 3.1 Argument slots, counted against the mangled name

`…QEAAX PEBVGeometry PEBVMaterialInstance G G M M H PEBX I @Z` → return `X` (void, **no
hidden return slot**), then `Geometry*`, `MaterialInstance*`, `unsigned short`, `unsigned
short`, `float`, `float`, `int`, `const void*`, `unsigned`. Four in registers, six on the
stack. Prologue is `PUSH RDI; SUB RSP,0x70` = 0x78, so the 5th argument's home slot (0x28)
lands at `RSP+0xa0` — and the body reads exactly `0xa0/0xa8/0xb0/0xb8/0xc0/0xc8`. Ten
arguments predicted, ten arguments read. The existing typedef at
`engine/funcs/engine_funcs.h:6217` already matches, field for field.

### 3.2 The three silent drops — an independent confirmation, not a new finding

Reading the bytes reproduces lane F's decompiled list unchanged:

* **(a)** `BT EAX,R11D` on the draw group's pass mask `+0x1bc`, bit = `MaterialInstance+0x20`.
  It is a **32-bit** `BT`, so the index is taken modulo 32; the port writes
  `(mask >> (pass & 31)) & 1`.
* **(b)** the group's shader index `+0x1b8` is negative, or `>= Material+0x30`.
* **(c)** the selected `VideoProgram` (`Material+0x28` indexed by that shader) has a null
  `+0x20`, or a null `+0x28`/`+0x30`, or either of those has `+0x48 <= 0`.

Two readings of the same bytes by different lanes agreeing is worth more than either alone.
The claim at `standalone/scene_watch.cpp:593` and `scene_watch.h:71` that `AddDrawItem`
silently drops items stands — still no correction needed there.

### 3.3 The float compare

`0x18010584d`: `UCOMISS XMM0,[0x18016169c]` (= 1.0f), `JP`/`JNZ` → `EAX = 0x8000` unless
the 7th argument is **exactly** 1.0f. A NaN takes the not-equal path, and C++ `!=` on a
NaN does the same, so `(arg7 == 1.0f) ? 0 : 0x8000` is faithful including for NaN. The
subsequent `OR AX,R9W` is a **16-bit** OR and is written as one.

This is the only float the function touches. There is no arithmetic, so there is no
association order to preserve — the bit-exactness risk here is zero.

### 3.4 The array helper's growth rule

Transcribed exactly; it is **not** doubling:

* `newCount == oldCount` → return, no work at all;
* `newCount <= capacity` → no reallocation, and only slots `[oldCount, newCount)` are filled;
* `newCount > capacity` → `capacity = newCount`, **except** that a one-element growth from a
  non-empty array sets `capacity = newCount*3` (`0x1800636be`; the first append onto an
  empty array therefore allocates 1, not 3);
* the old buffer is freed only if it actually changed and was non-null.

The `EAX = (oldData == this->_data) ? oldCount : 0` at `0x1800636e4` is what lets one loop
serve both cases. Allocation goes through `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
and `?MemFree@…` via `RT_EXPORT_ONCE` — **not** `RtOriginal`, which is `GetProcAddress` and
would also bypass our own ported allocator
(`reference_rtoriginal_is_getprocaddress.md`).

### 3.5 `MaterialInstance::RenderTextures` — ported so #3 closes rather than moves

Lane Z's triage rule: a port closes a row only if every callee it needs is already
satisfied on our side. `AddDrawItem`'s tail calls `RenderTextures` (`0x180102df0`,
exported, ordinal 3570) whenever `MaterialInstance+0x25 != 0`. Reaching that through the
DLL would leave a null waiting at cutover
(`reference_converting_a_door_is_not_satisfying_it.md`), so it is ported too — 45
instructions, and its own callees are satisfied:

* `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` — **ported** (`runtime/src/object/type_system.cpp`);
* the type id at `MOV EDX,[0x1801f9a20]` — that address is not an anonymous global:
  `pefile` says `0x1801f9a20` **is** the export
  `?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A` (ordinal 4864), and
  the first dword of an `Object::BaseTypeInfo` is its type id, the same shape
  `runtime/src/ui/element.cpp` already reads for `ClipRegionElement` and `ScrollElement`.
  At rest in the image it is `0xffffffff`; the reflection registry fills it in at startup.
  (See §5 — this is the one caveat.)

Two faithful uglinesses kept: `obj->+0x2c` is dereferenced **before** `obj` is null-checked
(`0x180102e49` vs `0x180102e61`), and the loop bound `*(int*)(this+0x58)` is **re-read every
iteration** (`0x180102e83`), not hoisted. Guarding the first would not restore retail's
behaviour, it would skip it; hoisting the second would change behaviour if a slot-0x58
handler ever appends a texture.

### 3.6 Oracle for `AddDrawItem`: there is none, and that is stated rather than faked

The function's entire effect is an append into a **live** array the helper may reallocate
and free. A scratch replay would share `_data` with the real group, so "theirs" would
release memory the live `Renderer` still points at; a deep copy would need the group's
other 0x1c0 bytes, which this lane does not own. Lane F's reason (1) stands and this lane
does not pretend otherwise.

What **is** instrumented:

* `RT_DIFF_WORK` on every path — how many calls got past all three guards to the append.
  A site that always dropped would otherwise report `calls=` looking healthy while doing
  nothing (`reference_uncalled_port_is_not_verified.md`,
  `reference_probe_needs_an_effectiveness_control.md`).
* `RT_DIFF_SHADOW` checking that the element the **array helper** landed in memory is
  byte-identical to the record handed to it, over the 0x34 written bytes in two spans
  (skipping +0x24 and +0x34). **This tests the helper, not the port's agreement with
  retail**, and the source comment says so in those words. It is the one piece of new
  transcription with a mechanical answer; it is not a substitute for a diff, and reading it
  as one would be exactly the "true by construction" error in
  `reference_recheck_must_match_its_own_proof.md`.

---

## 4. Registration

All four Graphics ports and the PerlinNoise ctor are registered **twice** — by mangled name
and by retail address — so a consumer that resolves either way lands here. The Wave 6 lane
F precedent is explicit about why (`engine/funcs/engine_funcs.cpp:2856` resolves one Camera
method by address while its siblings resolve by name; a name-only registration would have
left that call site on the DLL with two implementations taking turns on one live object).

Kill switch: **`BANISHED_RT_GFX9=0`** leaves all four Graphics-file ports on the DLL
(`runtime/src/graphics/rt_graphics_port.cpp`). The PerlinNoise ctor rides
`BANISHED_RT_UTILITY` with its sibling `Generate`. Both are default-ON, like every other
wave's switch — a default-off registration would be counted as ported by the census while
never running (`reference_census_counted_gated_and_missed_dx11.md`).

`Math::Transform::Lerp` is registered from a **Graphics** file. That is deliberate and
stated: nothing in this tree resolves it by name, it exists here only as
`InterpolatedState<Transform>::Lerp`'s callee, and coupling it to this lane's switch keeps
caller and callee turning on and off together.

### Two new "ports with no caller" — written verdicts, as the census demands

`tools/runtime_census.py` flags a registered port that nothing in the tree resolves, and
says "a NEW name appearing here has NOT been triaged". Two appear:

* **`0x18001d690` / `?Lerp@Transform@Math@@…`** — exercised by a **direct C++ call** from
  `RtGraphics_InterpolatedStateTransform_Lerp`, exactly as retail's `0x18001aff0` calls
  `0x18001d690`. Fires once per interpolated drawable per frame via
  `geometry_draw.cpp:367`. Same category as the two `Quaternion` members in
  `WAVE8_5.md` §2.
* **`0x180102df0` / `?RenderTextures@MaterialInstance@Graphics@@…`** — exercised by a
  direct C++ call from `RtGraphics_Renderer_AddDrawItem` when `MaterialInstance+0x25 != 0`,
  exactly as retail's `AddDrawItem` calls it. It may legitimately never fire in a given run
  (no render-to-texture material bound in that scene); that is precisely what the
  `RT_DIFF_WORK` effectiveness control on the site measures, and a `work=0` reading there
  is a fact about the scene, not about the port.

---

## 5. The one caveat on `AddDrawItem`, stated plainly

`?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A` is an exported **data**
symbol and is **not in the mint** (`grep GeneratedImageBuffer runtime/ standalone/` finds
nothing outside this lane). Consequences, exactly:

* **While the DLL is loaded** (today, and every diff run): `ResolveExport` finds the DLL's
  own object, the type id is the real one, and `RenderTextures` behaves as retail does.
* **At cutover**: the resolve returns null, `GeneratedImageBufferTypeId()` returns
  `0xffffffff`, `IsDerivedFrom(obj, 0xffffffff)` is false for everything, and the
  render-to-texture pre-pass **silently does nothing**. No crash — a degraded sub-feature.

That is a strict improvement on the status quo, where `AddDrawItem` itself is null at
cutover and the process dies on the first drawable. But it is a real gap and it belongs to
the mint lane, not here: **`?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A`
needs minting before cutover is called clean.** It is the same class of symbol lane F
marked "n/a — they become ours at cutover, when the type registry itself is built by our
code".

Note also that resolving it grew the census's Graphics `called` column by one (87 → 88):
porting can **grow** the cutover surface, and it did
(`reference_porting_can_grow_the_cutover_surface.md`); `called` is not a denominator
counting down to zero.

---

## 6. Declined, with evidence

The triage rule throughout: **a port closes a cutover row only if every callee it needs is
already satisfied on our side; otherwise it moves the row one frame deeper.** "Exported" is
not "satisfied" — an exported symbol nobody has ported is still null once the DLL is gone
(`reference_converting_a_door_is_not_satisfying_it.md`).

### 6.1 `Renderer::Draw` (`0x180104bb0`, 219 instructions)

Lane F declined it and this lane agrees, but **for a different reason**, because lane F's
reason has partly expired. Lane F wrote that `Video::Context` "is lane A's class and is
being ported concurrently; pinning its layout from here would duplicate that work". Lane A
landed: `runtime/types/video/Context.h` now pins `sizeof(Video::Context) == 0x2168`, and
`Camera.h` already pins `RendererSceneEntry` (0x110), `RendererDrawGroup` (0x1f0) and
`Renderer` (0x50). **The layout objection is gone.**

What remains is callee arithmetic, and it is decisive. `Draw` calls five functions this
lane would have to port as well for the row to close, none of them ported today:

| Callee | Retail | Size | Exported? |
|---|---|---|---|
| `Renderer::Reset` | `0x180105110` | 0x64 | — |
| `Renderer::AddScene` | `0x180105700` | 0xaa | — |
| `Renderer::BeginDrawGroup` | `0x180104fd0` | 0x139 | — |
| `Scene::AddCulledDrawItems` | `0x1801017a0` | — | private (`AEBA`) |
| `FUN_180062d60` (the per-pass sort) | `0x180062d60` | **0x2306** | **no** |

That last one is 2,310 bytes of unexported sort. Porting `Draw` without it moves the row
from `Draw` to `FUN_180062d60` — a *worse* row, because an unexported internal has no name
for the residue census to report. This is a lane of its own (`Draw` + those five, with the
`Context` layout it can now rely on), and it should be taken as one. It is also the reason
this lane does **not** claim the per-frame path is closed: see §8 point 4.

### 6.2 `MaterialInstance::BuildParameters` (`0x1801033e0`, 361 instructions)

The largest function in the class, and lane F's reasons hold unchanged: it reads
`Graphics::Material`'s internals four levels deep (`material+0x28 → [i] → +0x30 →
+0x20/+0x30`), calls `Material::GetTextureName`/`GetConstantName` which **return
`System::String` by value** (hidden return slot plus a refcounted temporary destroyed every
iteration — `reference_hidden_retval_and_int_pointer_args.md`), calls the private
`FindTexture`/`FindConstant`, and bulk-mutates `_parameters` with no replayable oracle.
`Graphics::Material` is a class no lane has laid out because no ported body needed it; this
one would need it in full. **It belongs to a lane paired with `Graphics::Material`**, which
is what lane F already said.

### 6.3 `AnimationManager::Update` (`0x1800fc240`, 90 instructions)

Lane G held this back on two grounds, and **both are now known to be wrong on the facts**:

* "calls a 9-argument **unexported** `AnimationState::Update`" — it **is** exported:
  `?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@PEAV78@AEAVVector3@8@_N6@Z`,
  ordinal 4474, `0x1800fdb20`.
* "this lane read the calling convention of from the decompile only, not from
  disassembly" — the disassembly gives it unambiguously, and it matches the mangled name
  slot for slot: `RCX` = the `AnimationState*`, `RDX` = the `Time&` (spilled to its home
  slot at `RSP+0xe8` and restored every iteration), `R8` = `&group.nodes` (`R15+RSI`),
  `R9` = an **identity `Transform` built on the stack** at `RSP+0x50` (`{0,0,0,1,0,0,0}`),
  then `[RSP+0x20] = state->+0x38`, `[RSP+0x28] = state->+0x28`, `[RSP+0x30] = state+0x18`,
  `[RSP+0x38] = the bool argument`, `[RSP+0x40] = 0`. The outer walk is groups of stride
  0x30 (`_data` at +0x00, count at +0x08, state array at +0x20, state count at +0x28) with
  states of stride 0x48. After each call it re-stamps `*(Transform*)(state->+0x28)` with an
  identity rotation and the translation at `state+0x18`.

So the stated blocker is stale — and the row still does not close, for the reason the
triage rule gives: `AnimationState::Update` is **266 instructions and unported**. Porting
the manager alone converts one null at cutover into a different null one frame deeper, and
this is the hottest skeleton path in the game (every character, every frame,
`gameupdateloop.cpp:1293`, already behind the default-on `ANIM_BATCH_ALL` batching in
`standalone/perf_experimental.cpp`). **Declined — but the next lane should start from the
argument map above, not from "the convention is unknown".**

### 6.4 `MeshInstance::Serialize` (`0x1800ff680`, 20 instructions)

Short, and genuinely tempting: on the WRITE path a `Serialize` is a pure read of the object
and replays into two independent sinks
(`reference_write_path_serialize_is_replayable.md`). Declined anyway, on callees:

```
1800ff697: CALL 0x1800fe250   ; serialize the mesh handle at this+0xd8
1800ff6b5: CALL [RAX+0xa0]    ; Encoding vtable slot +0xa0 — an int, default 0
1800ff6cf: CALL 0x18005cc60   ; Array::SetCount(this+0xc8, count)  — READ path only
1800ff6e2: CALL 0x18005d4a0   ; serialize `count` elements of this+0xc8
```

Three unported helpers, two of them the array-serialisation machinery, on the **save/load
stream**. Wave 5 held back every serializer-constructing function project-wide for exactly
this: a serializer that consumes the wrong byte count does not fail where it is wrong, it
desyncs the stream and surfaces later on an unrelated record
(`reference_slot3_wrapper_must_resolve_first.md`). This lane cannot build or run, so it
cannot even take a save/load parity pass — porting it blind onto the save format is the one
place where being wrong costs the player their town. **Declined, and it should go to a lane
that can run a save/load round trip.**

Also worth noting for that lane: the `Encoding` vtable slot here is read at **+0xa0 in
retail**, and retail's offsets — not the profile build's — are the ones that must be used
(`reference_encoding_vtable_offsets_differ_between_builds.md`).

### 6.5 `SunLight::SunLight` (`0x1801081f0`) and `SunLight::Set` (`0x180107f10`)

Lane F declined both on three grounds. **Two of the three have expired**, and this lane
records that rather than repeating them:

* "`Video::RenderBuffer` and `Video::ImageBuffer` are lanes B and C, being ported
  concurrently" — they landed. `??0ImageBuffer@Video@@QEAA@XZ` (0x1800f2580),
  `?Create@RenderBuffer@Video@@…` (0x1800f3f10), the ImageBuffer platform hooks and the
  allocator are all **ported** today; lane VID has since added `RenderBuffer::Recreate` and
  the `_firstRenderBuffer` list walkers on top.
* "`SunLight` derives from `Graphics::Geometry` — lane D — porting a derived ctor while its
  base is being ported in another file" — lane D landed too;
  `??0Geometry@Graphics@@QEAA@XZ` (0x1800a4270) is registered by address at
  `runtime/src/graphics/scene.cpp:1721`, and both vtables the ctor stamps are exported
  (`??_7Light@Graphics@@6B@` ord 1458, `??_7SunLight@Graphics@@6B@` ord 1508).

Two new findings, which is why the pair is still declined:

1. **`SunLight::Set` does no float arithmetic at all.** The brief flagged it as a
   bit-exactness risk alongside `Lerp`; it is not. `0x180107f2b..0x180107f8d` is eleven
   plain `MOV EAX,[src] / MOV [this+off],EAX` dword copies — direction `Vector3` to
   +0xc0..+0xc8, sun colour `Vector4` to +0xcc..+0xd8, ambient `Vector4` to +0xdc..+0xe8.
   There is no association order to get wrong. (The layout matches lane F's recovered note
   exactly.) Worth recording because a future lane should not budget bit-exactness work
   for it.
2. **The real blocker is two unported functions, and it is the ctor that needs them.**
   `?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ` (`0x1800f4320`, ord 3341) and
   `?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ` (`0x1800f4400`, ord 3369) are exported
   and **unported** — `runtime/src/video/buffers.cpp` ports the `ImageBuffer`,
   `IndexBuffer` and `VertexBuffer` platform hooks but not the `RenderBuffer` pair, and
   re-checked against lane VID's landed work at `5f85cc2` they are still absent. The ctor's
   GPU-build block is **not** a rare path: it runs whenever the device reports the
   capability, which is the normal case. Both also mutate the global intrusive list at
   `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA` (0x1801fe790, exported data — the
   same symbol lane VID now reads at `runtime/src/video/dynamic_resources.cpp:168`) under
   the Device's critical section at `Device+0x60` with the recursion bookkeeping at
   `+0x88`/`+0x8c`.

And lane F's reason (1) is untouched and remains sufficient on its own: **no oracle exists
and constructing one is not free.** Running the shipped ctor a second time allocates two
more 2048×2048 render targets and leaks them, or frees the ones the live object holds.

Recovered for whoever takes the pair (all fresh from the disassembly, agreeing with lane F):
`sizeof(SunLight) == 0x108`; direction `Vector3` +0xc0, sun colour `Vector4` +0xcc, ambient
`Vector4` +0xdc, depth `ImageBuffer*` +0xf0, colour `ImageBuffer*` +0xf8, `RenderBuffer*`
+0x100; the ctor sets `Geometry+0x30 = 0x8000`, forces the bounds at +0x34..+0x48 to
±1.0e6 (`DAT_180161778`/`DAT_18016173c`) and ORs the dirty bit `Geometry+0x2c |= 2`; both
image buffers are created 0x800×0x800 through vtable slot +0x48 with `(0, 0, 5, …)` for
depth and `(0, 1, 7, …)` for colour; `RenderBuffer::Create` (0x1800f3f10) is then called as
`Create(rb, this->+0xf0, this->+0xf8)`. `operator new` here is `0x18001e910`/`0x18001f2e0`
(**identical bodies**: lock `_singleton+0x640`, `Allocate(size, 8)`, unlock) and `operator
delete` is `0x18001f360` — i.e. `RT_EXPORT_ONCE` on `?MemAlloc@…`/`?MemFree@…` reproduces
both exactly.

`SunLight::Set`'s live path is also worth handing over: with the capability present and
`+0xf0` non-null (the state the ctor leaves), **both** rebuild branches are skipped and the
function is *only* the eleven dword copies. A future lane can therefore oracle the live path
cheaply and reason about the rebuild path separately — but it must port the two
`RenderBuffer` platform hooks first, or it only moves the row.

**Recommended order for a follow-up lane:** `RenderBuffer::PlatformConstruct` /
`PlatformDestruct` first (they close two rows of their own and unblock lane VID's
neighbourhood), then the `SunLight` pair, which then closes without moving anything.

---

## 7. Audit numbers, before and after, with denominators

Both columns measured on this lane's **final base, `e86a5a7`** (lane ENG's merge) — the
"before" was re-measured there rather than carried over from an earlier base, precisely so
the delta is this lane's and not lane VID's, IOA's or ENG's.

| Audit | Before (`e86a5a7`) | After |
|---|---|---|
| `tools/rawdoor_audit.py` | **481** sites scanned, **0 UNGUARDED-CALL**, 34 DEFERRED-CALL | **483** sites, **0 UNGUARDED-CALL**, 34 DEFERRED-CALL |
| `tools/port_bypass_audit.py --by-name` — ADDRESS door | **198** sites, **0** reach an already-ported function; ADDR-REGISTERED 33, DETOURED 3, NO-EXPORT 85, UNPORTED 77 | **198** sites, **0**; the same four counts, unchanged |
| … NAME door | **408** raw-door sites, **0 BYPASS** on a live path | **410** sites, **0 BYPASS** |
| … by-name cross-check | **636** ported names / **397** raw-door statements / **0** the parser missed | **641** names / **399** statements / **0** missed |
| `tools/rt_data_door_audit.py` | **PASS** — no minted DATA symbol through a raw door; 3 RT_RAW_OK exemptions; **5** UNCLASSIFIED | **PASS**; 3 exemptions; **5** UNCLASSIFIED — the same five |
| `tools/runtime_census.py` | **848 called / 611 ported / 72.1%** (Graphics 87/59, Math 63/60, Utility 2/1) | **849 called / 614 ported / 72.3%** (Graphics **88/60**, Math 63/**61**, Utility 2/**2**) |
| `tools/export_name_audit.py` | **659** distinct names, **1** not in the DLL | **660** names, **1** not in the DLL (the same pre-existing one, `rt_resolve_cache.cpp:8` — the tool's own placeholder) |
| `tools/abi_retslot_audit.py` | **0 suspicious of 48** by-value-returning exports | **0 suspicious of 48** |
| `docs/runtime_port/wave9_laneVID_dx11scan.py` | 70 names seen, **21 UNSAT** | 70 seen, **21 UNSAT** — unchanged; none of this lane's targets is in that set (§0b) |

Reading the census honestly: the `ported` column moved **+3**, not +5 (611 → 614), because
`Transform::Lerp` and `RenderTextures` were not in the `called` denominator to begin with —
nothing in this tree resolved them. They are still real ports; they are the two entries in
§4's "no caller" table. And `Graphics.called` moved **87 → 88** because this lane now
resolves one more DATA symbol (§5).

The `rt_data_door_audit` UNCLASSIFIED count came back to 5 only after a deliberate change:
the type-id read was first written with the `RT_DATA_SLOT` macro, which that tool cannot
see through (it matches the identifier immediately before the mangled literal), and it
filed as a sixth UNCLASSIFIED door. It is now spelled with a literal `ResolveExport` and an
explicit cache-the-hit / retry-the-miss static — identical semantics, visible door. The
comment at the site says why, so nobody "tidies" it back into the macro.

---

## 8. A falsifiable prediction for the next DLL-less boot

Concretely, for cutover attempt 07:

1. **Three of the 61 null symbols will be gone from the unsatisfied list**:
   `??0PerlinNoise@Utility@@QEAA@MMHH@Z`,
   `?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@…`, and
   `?AddDrawItem@Renderer@Graphics@@…`. Neither `?Lerp@Transform@Math@@…` nor
   `?RenderTextures@…` should appear as a **new** entry — if either does, this lane's
   registration is not being reached.
2. **Six of this lane's nine will still be there**, unchanged:
   `?Draw@Renderer@Graphics@@…`, `?BuildParameters@MaterialInstance@Graphics@@…`,
   `?Update@AnimationManager@Graphics@@…`, `?Serialize@MeshInstance@Graphics@@…`,
   `??0SunLight@Graphics@@QEAA@XZ`, `?Set@SunLight@Graphics@@…`.
3. **One new unsatisfied DATA symbol should appear**:
   `?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A`. If it does *not*
   appear, the residue census is not watching DATA resolves and §5's gap will be invisible
   rather than absent — which is a finding about the census, not a pass.
4. `?Draw@Renderer@Graphics@@…` is still null, so **the boot will still die at the first
   frame**. The per-frame path is not closed by this lane, only its per-drawable half is.
   Anyone reading "AddDrawItem ported" as "rendering works at cutover" is reading it wrong.
   The 21 dx11 import names (§0b) are a separate and independent way the first draw dies.

And with the DLL still present, on an ordinary diff run
(`BANISHED_RT_DIFF=1`, `mingw32-make sa-run`):

5. `Math::Transform::Lerp` and `Graphics::InterpolatedState<Transform>::Lerp` should report
   **`divergences=0` with `calls=` in the millions** — one per interpolated drawable per
   frame via `geometry_draw.cpp:367`. **A `calls=0` on either is a failure of this lane,
   not a pass**: it would mean the registration is not being reached
   (`reference_uncalled_port_is_not_verified.md`).
6. `Graphics::Renderer::AddDrawItem` should report a large `calls=` with `work=` a
   **strictly smaller but non-zero** number (the three silent drops are real and fire).
   `work=0` means the guards are being read wrong — most likely the `BT` mask index — and
   the screen will be empty, which is the loud version of the same signal.
7. `Graphics::MaterialInstance::RenderTextures` may legitimately report `work=0` in a scene
   with no render-to-texture material. That is a fact about the scene. It is **not**
   evidence the port is correct.

---

## 9. What this lane could not verify

* **No build of the full exe and no run of any kind.** The integrator holds the game
  directory. Verification was `g++ -fsyntax-only` **and** a full `-O2 -c` compile with the
  Makefile's exact standalone flags, on `wave9_lane_gfx.cpp`, `perlin.cpp`,
  `transform.cpp`, `rt_graphics_port.cpp`, `mesh.cpp` and `renderer.cpp` (every source in
  the tree that includes the two headers this lane changed) — all clean, on the rebased
  base. Every oracle described above is code that has compiled and never executed.
* **`AddDrawItem` has no differential oracle at all** (§3.6). Its correctness rests
  entirely on the disassembly transcription. It is on the hot per-frame path, and a wrong
  guard would show up as models not drawing rather than as a crash. `BANISHED_RT_GFX9=0`
  isolates all four Graphics ports in one A/B run if the integrator sees anything odd — use
  it before bisecting.
* **The `_pad24`/`_pad34` claim is a claim about what retail does NOT write.** It is read
  from the absence of stores in `0x1801057b0`'s listing, which is a complete listing, but
  "never written" is only meaningful with a "by whom"
  (`reference_never_written_needs_by_whom.md`): it means *by `AddDrawItem`*. If some other
  writer touches a draw item in place, those two spans may hold meaning this lane has not
  looked for.
* **`GeneratedImageBuffer`'s type id at cutover** (§5) — the degradation is reasoned, not
  observed.
* **`Renderer::Draw`'s callee sizes** are byte ranges from Ghidra's function boundaries, not
  instruction counts; the argument in §6.1 does not depend on the exact figure, only on
  `FUN_180062d60` being an unexported function of that order of magnitude.
* **Field names**, throughout: `PerlinNoise`'s four, and `RendererDrawItem`'s `arg5` /
  `arg8` / `arg10` / `userData`. Offsets are pinned by stores; the names are usage
  inference and are labelled as such rather than dressed up.
