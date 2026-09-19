# WAVE 27 LANE GFXMESH — the Graphics mesh vtable family

Branch `wave27/gfxmesh`, cut from `wave27/base` at `c6f30c24`.

Scope as briefed: three tables, ~21 unported slot bodies.
`??_7EdgeMesh@Graphics@@6B@` 0/9, `??_7EdgeMeshInstance@Graphics@@6B@` 4/9,
`??_7MeshInstance@Graphics@@6B@` 2/9. Plus one named obligation: this lane is
the **addressee** of the `RT_UNPROMOTABLE` tag on
`?AddDrawItems@MeshInstance@Graphics@@` at `standalone/reg_mint_promoted.cpp:2257`.

---

## 1. HEADLINE — MEASURED

`tools/runtime_census.py`, run by this lane on `wave27/base` before any edit and
again on the lane tip. Both numbers are from the same tool at the same two tips.

| row | before | after |
|---|---|---|
| `??_7EdgeMesh@Graphics@@6B@` | 0 / 9 | **9 / 9 COMPLETE** |
| `??_7EdgeMeshInstance@Graphics@@6B@` | 4 / 9 | **7 / 9** |
| `??_7MeshInstance@Graphics@@6B@` | 2 / 9 | **5 / 9** |

**13 slot bodies ported. One table reaches COMPLETE. Nothing is promoted.**
Plus a 14th body outside the tables, below — **14 bodies in total.**

The pre-edit run reproduced the brief's three figures exactly, so the brief's
counts were not stale — stated because "re-derive, do not trust" was the
instruction and a re-derivation that *agrees* still has to be reported as having
happened (`reference_a_matching_number_can_match_because_it_is_stale.md`).

### The fourteenth body, and why it is here

`?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@HM@Z`
(0x180016c20, ordinal 2814) is **not** a vtable slot of any of the three tables,
so it is strictly out of scope. It is ported because
`?GetInstanceTransform@EdgeMeshInstance@` **calls** it, and porting that body made
this address newly `called`. **A lane that opens a row and hands it on has moved
work, not done it** — so this closes the one row this lane opened.

### THE TOTALS — measured, with the part I could NOT reconcile said plainly

| | `wave27/base` | lane tip |
|---|---|---|
| called | 1372 | **1377** |
| ported | 1237 | **1238** |
| gap | 135 | **139** |
| BODY rows in the gap | 18 | **18** |
| TOTAL % | 90.2% | **89.9%** |

**The percentage went DOWN while fourteen bodies were added.** That is
`reference_the_ceiling_percentage_is_a_moving_target.md` in its plainest form:
`called` is in both halves of the ratio, and thirteen of the fourteen bodies land
in the **PROMOTION BACKLOG**, a population the census states explicitly is *"NOT
counted above"*. **Score this lane on the three table rows, never on the TOTAL.**

Of the **+5** in `called` I can positively attribute **three**: the
`EdgeMesh::Serialize` helpers in §6, which appear both in
`runtime_census.py --unported` as `addr internal` rows and in this lane's diff —
`0x180014230`, `0x180014520`, `0x180014850`. A fourth is `0x180016c20`, which is
+1 `called` and +1 `ported` and therefore net zero on the gap.

**I did not reconcile the remaining ~1 row, and I am not going to guess at it.**
Saying so is the point: a decomposition that closes by assertion is the defect
`reference_a_matching_number_can_match_because_it_is_stale.md` describes. The
figures above are what the tool printed at the two tips, nothing more.

> **[ADDRESSEE — the ~1 unreconciled `called` row]** the Wave 28 kickoff writer,
> if the figure still matters then. The re-derivation is
> `tools/runtime_census.py --unported` at `wave27/base` and at this tip, diffed
> as SETS rather than compared as counts
> (`reference_diff_the_set_not_the_count.md` — this lane deliberately does not
> pretend to have done that).

### A DEFECT IN THIS LANE'S OWN WORK, CAUGHT BY THE CENSUS AND NOT BY READING

The first draft registered the fourteenth body as
`?GetNodeToWorld@MeshInstance@Graphics@@`**`UEBA`**`?AVTransform@Math@@HM@Z`, by
analogy with the four virtual slot bodies beside it. **That overload is a public
NON-VIRTUAL const member: `QEBA`.** `UEBA` is a name the retail DLL does not
export, so `RegisterRuntimeExport` would have registered a symbol nothing can ever
ask for — the body written and unreachable by name, while every door in the file
went on resolving the DLL's copy
(`reference_resolveexport_phantom_names.md`, and the `QEAA`/`UEAA` form of the
same trap in `reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

**Reading the code did not catch it. The tool did**, in a specific and reusable
way: `runtime_census.py --unported --namespace Graphics` kept listing the symbol
as *unported* **after** the registration had been added. That is what a phantom
name looks like from the outside, and it is worth keeping as a detector:
**a registration that does not move the census is a registration of a name that
does not exist.** With the spelling corrected, `ported` moved 1237 → 1238 and the
BODY gap 19 → 18.

A second, independent half of the same fix: every `RegisterRuntimeExport` this
lane added is now spelled **on one line**, because `runtime_census.py` cannot see
a mangled name split across a C++ string-literal concatenation
(`reference_census_three_blind_spots.md`), and a registration it cannot see reads
as an absent port
(`reference_half_registered_port_reads_as_absent.md`). Four of this lane's
registrations had been split for column width.

---

## 2. WHAT IS MEASURED vs WHAT IS INFERRED

**MEASURED**

- The three census rows above, before and after, same tool, same tips.
- `mingw32-make standalone-debug` exits 0 on the lane tip.
- `tools/addr_door_audit.py` reports UNREGISTERED **21 → 25** and a new
  concentration of **4 in `runtime/src/graphics/edgemesh_vtable.cpp`**. That rise
  is the audit becoming ABLE TO SEE, not doors being added — see §8.2, and note
  that ONE of the four is not this lane's at all.
- Every one of the 21 addresses in scope, plus every `~Class` and every helper,
  resolved in the RETAIL export directory **by VA** with `pefile` against
  `C:\GOG Games\Banished\Runtime-x64.dll` — not by name pattern, not through
  Ghidra, not against the profile build.
- `tools/rt_deleting_dtor_verify.py` re-derives the size and `~Class` constants
  of all three new deleting-destructor rows from the retail PE image and agrees
  with all six. (It also prints BAD on two rows for an unrelated reason — §8.)
- The retail/profile discriminator `disassemble_function(0x180002190)` returns
  the 2-instruction RETAIL body, both with `program=` explicit and with it
  omitted; four offset-carrying bodies were then re-taken with `program=`
  explicit and diffed against what had already been derived — all identical; then
  the two most constant-dense of them re-derived a THIRD time with `objdump`
  against the retail file on disk, which is the first genuinely independent
  witness of the three (§8.5).

**INFERRED, and labelled as such**

- That `??_7EdgeMesh@Graphics@@6B@` is *promotable*. It is COMPLETE, which is the
  static half only. See §5 — this lane did NOT establish who stamps it.
- That the thirteen bodies are CORRECT. Twelve carry a diff screen and one
  (`EdgeMesh::Serialize`) deliberately carries none. **None of the thirteen has
  been observed executing.** See §4.
- That `EdgeMesh::Compile` returning 1 means "success". The constant is
  transcribed; its meaning in `Object::External::CompileResult` is not decoded
  and is not needed.

---

## 3. PER-SLOT TABLE

`??_7EdgeMesh@Graphics@@6B@` — DLL 0x180154c70

| slot | symbol | VA | state |
|---|---|---|---|
| 0 | (unnamed vector deleting dtor) | 0x1800134b0 | **ported** — `deleting_dtor.cpp` row 44 |
| 1 | `?TypeName@EdgeMesh@` | 0x180012c90 | **ported** |
| 2 | `?Type@EdgeMesh@` | 0x180012c70 | **ported** |
| 3 | `?GetFileVersion@EdgeMesh@` | 0x1800a23d0 | **ported** |
| 4 | `?Compile@EdgeMesh@` | 0x1800a21c0 | **ported** |
| 5 | `?Serialize@EdgeMesh@` | 0x1800a21d0 | **ported**, no oracle (§4) |
| 6 | `?Reconstruct@EdgeMesh@` | 0x180012e10 | **ported** |
| 7 | `?Destruct@EdgeMesh@` | 0x180012e40 | **ported** |
| 8 | `?Construct@EdgeMesh@` | 0x180012e30 | **ported** |

→ **9/9 COMPLETE.** Not promoted (§5).

`??_7EdgeMeshInstance@Graphics@@6B@` — DLL 0x180154d70

| slot | symbol | VA | state |
|---|---|---|---|
| 0 | (unnamed vector deleting dtor) | 0x1800175d0 | **ported** — row 45 |
| 1 | `?Update@EdgeMeshInstance@` | 0x1800a39b0 | **ported**, no oracle (§4) |
| 2 | `?Draw@EdgeMeshInstance@` | 0x1800a3150 | **BLOCKED** (§7) |
| 3 | `?AddDrawItems@EdgeMeshInstance@` | 0x1800a2fd0 | **BLOCKED** (§7) |
| 4 | `?UpdateAttached@Geometry@` | 0x1800a4020 | already ported (scene.cpp) |
| 5 | `?GetSubTransform@Geometry@` | 0x1800a43c0 | already ported (scene.cpp) |
| 6 | `?GetInstanceTransform@EdgeMeshInstance@` | 0x1800a3840 | **ported** |
| 7 | `?SetSharedInstanceData@Geometry@` | 0x180015590 | already ported (scene.cpp) |
| 8 | `?Clean@Geometry@` | 0x1800a4420 | already ported (scene.cpp) |

→ **7/9.**

`??_7MeshInstance@Graphics@@6B@` — DLL 0x180154dc0

| slot | symbol | VA | state |
|---|---|---|---|
| 0 | (unnamed vector deleting dtor) | 0x180017330 | **ported** — row 46 |
| 1 | `?Update@MeshInstance@` | 0x180100800 | **BLOCKED** (§7) |
| 2 | `?Draw@MeshInstance@` | 0x180100960 | **BLOCKED** (§7) |
| 3 | `?AddDrawItems@MeshInstance@` | 0x180100f20 | **BLOCKED** (§7) — the tagged row |
| 4 | `?UpdateAttached@Geometry@` | 0x1800a4020 | already ported (scene.cpp) |
| 5 | `?GetSubTransform@MeshInstance@` | 0x180101200 | **ported** |
| 6 | `?GetInstanceTransform@MeshInstance@` | 0x180101280 | **ported** |
| — | `?GetNodeToWorld@MeshInstance@…HM@Z` 0x180016c20 | *not a slot* | **ported** — closes the row this lane opened |
| 7 | `?SetSharedInstanceData@MeshInstance@` | 0x180101660 | **BLOCKED** (§7) |
| 8 | `?Clean@Geometry@` | 0x1800a4420 | already ported (scene.cpp) |

→ **5/9.**

**Note on the brief's "~21".** The real figure was 21 and it decomposes as
9 + 5 + 7, because `EdgeMeshInstance` and `MeshInstance` share four
`Geometry` slots that `scene.cpp` had ported waves ago — which is exactly where
`EdgeMeshInstance`'s starting 4/9 came from. A lane that read "4/9" as "four
EdgeMeshInstance bodies exist" would have gone looking for four files that do not
exist.

---

## 4. THE ONE FINDING THAT OUTLIVES THE LANE

### `MeshInstance::GetInstanceTransform` returns a Matrix whose fourth column is not part of the matrix

`?GetInstanceTransform@MeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z`
(0x180101280) ends with **sixteen separate `MOVSS` stores** into the return slot,
and only **twelve** of them come from the `Math::Matrix` that
`Transform::AsMatrix` just produced:

```
    ret+0x00,+0x04,+0x08   <- matrix+0x00,+0x04,+0x08
    ret+0x10,+0x14,+0x18   <- matrix+0x10,+0x14,+0x18
    ret+0x20,+0x24,+0x28   <- matrix+0x20,+0x24,+0x28
    ret+0x30,+0x34,+0x38   <- matrix+0x30,+0x34,+0x38
    ret+0x0c               <- this+0x10c     MeshInstance::_color[0]
    ret+0x1c               <- this+0x110     MeshInstance::_color[1]
    ret+0x2c               <- this+0x114     MeshInstance::_color[2]
    ret+0x3c               <- this+0x118     MeshInstance::_color[3]
```

`AsMatrix` writes 0,0,0,1 into `m[3]/m[7]/m[11]/m[15]`. Those four values are
**discarded** and the instance's RGBA tint is packed into that column instead —
per-instance colour reaching the vertex shader inside the transform buffer.

**Why this is worth a section.** The obvious port is `AsMatrix(&xf, ret)` — one
call, 64 bytes, done. That port passes **every geometric check there is**: every
position, every rotation, every bounds test, every screenshot of geometry. It
renders the whole world untinted and nothing says so. The four colour stores are
unconditional — they run on all three branch arms — so no input finds the bug
either.

The four `_color` floats sit at exactly the offsets
`runtime/types/graphics/Mesh.h` already pinned from the constructor's four
`0x3f800000` stores, years of waves before this body was read. That is the
independent witness; the reading is not resting on this lane's decode alone.

**And the sibling override does NOT do it.**
`?GetInstanceTransform@EdgeMeshInstance@Graphics@@` (0x1800a3840) — the same slot
index, one class over, ported in the same file — writes the matrix `AsMatrix`
produced, untouched. Two overrides of one slot that differ only in their last
four stores. Carrying either body's ending over to the other would be silent,
plausible and wrong.

### Three smaller ones, recorded so they are not re-derived

- **The two `GetInstanceTransform` overrides pack their `int` argument
  differently.** `MeshInstance` uses **12 + 4** (`AND 0xfff` / `SAR 12` /
  `AND 0xf`); `EdgeMeshInstance` uses **15 + 1** (`AND 0x7fff` / `SHR 15`).
  Same slot, same signature, different encodings. Both were decoded separately
  and neither was assumed from the other.
- **`MeshInstance`'s packing is `MOVSXD` + `SAR`, not a zero-extend and a
  logical shift** — the trap `reference_ghidra_c_output_vs_disassembly.md`
  names, and one this very file had already been bitten by once (the pool-id
  shift in `RtInstancedPoolSlot`). Here the following `AND 0xf` masks the result
  back into range, so the two spellings agree **for every input**. That is worse
  than a difference, not better: no test can distinguish them, so the next
  reader has to re-derive it from the bytes. Written down for that reason.
- **`EdgeMeshInstance::Update` never reads its `Time&`.** Not an inference —
  RDX is overwritten by the function's first instruction. It copies the followed
  `MeshInstance`'s *current* `Geometry::_transform` (+0x84, 0x1c bytes, not the
  interpolated pair) onto itself through `Geometry::SetTransform`, which is how
  the selection outline tracks the thing it outlines and how its world box gets
  marked dirty.

### The oracle refusals, each with its reason

Twelve of the thirteen bodies carry a diff screen. Two do not, and both refusals
are structural, not convenience:

- **`EdgeMesh::Serialize`** — a differential run would consume or emit the record
  **twice** and desync the stream, and a stream desync surfaces later on an
  innocent record. This is the identical refusal `mesh.cpp`'s HELD BACK note
  item 2 makes for `?Serialize@MeshInstance@`, and Wave 5 made for four
  functions. What makes porting it safe anyway is narrow and stated at the body:
  it performs **no arithmetic of its own**. Every line is a transcription of a
  call — same helper, same arguments, same order, same retail vtable offsets.
  The format lives entirely in four helpers, and those are **not** ported.
- **`EdgeMeshInstance::Update`** — `SetTransform` **writes**: it rolls
  `_transform` into `_prevTransform` and raises two dirty bits. A replay would
  roll the pair twice and hand the renderer a previous transform one frame too
  new.

### `calls=0` is expected for all thirteen, and it is NOT verification

Two independent reasons, and they cover the whole set:

1. All thirteen are **virtual slots the DLL dispatches out of its own table**.
   The class-split banner at the top of `mesh.cpp` has said since Wave 6 that
   objects built by these constructors carry the DLL's vtable, so registering
   the export redirects nothing that runs today. The value of a registration is
   that the row becomes PORTED — which is what an unpromoted table's missing
   slots *are* — and that the body exists when the table is promoted or the DLL
   is gone.
2. For the EdgeMesh eight there is a second, independent reason:
   `standalone/address_registry.cpp:584` gates the selection-outline chain —
   the only consumer of `EdgeMesh` at all — default-OFF on
   `BANISHED_SELECTION_OUTLINE`.

`reference_uncalled_port_is_not_verified.md` applies to every one of them. A run
with `BANISHED_SELECTION_OUTLINE=1` and a building selected would exercise the
EdgeMesh half; this lane did not take one (§9).

---

## 5. PROMOTION VERDICTS — and why COMPLETE bought none of them

`??_7EdgeMesh@Graphics@@6B@` is **COMPLETE and NOT PROMOTED**.

The brief was explicit that COMPLETE is the static half of readiness and that
promoting turns on **owning the function that stamps the table**. This lane can
say precisely who stamps it, and the answer is why it stopped:

`??0EdgeMesh@Graphics@@QEAA@XZ` (0x1800a2830) stamps `0x180154c70` at
`0x1800a287c`, as a **RIP-relative `LEA` + `MOV [RCX],RAX`**. That constructor IS
ported (`mesh.cpp`, `RtGraphics_EdgeMesh_Ctor`) and OUR copy stamps our minted
table — but the DLL's copy still exists and still stamps the DLL's, and
**an in-DLL RIP-relative `LEA` is not a door**
(`reference_a_ported_row_can_be_permanently_unpromotable.md`). Whether every
live `EdgeMesh` comes through our ctor is a question about *call sites*, and
answering it needs an attendance measurement on a route that is gated off by
default (§4). **No source scan answers it and this lane did not run that route.**

So: **the static half is done and the dynamic half is untested.** Recording it as
"COMPLETE, promotion not established" rather than promoting on the green screen
is the whole content of `reference_a_green_readiness_screen_is_not_a_route.md`.

> **[ADDRESSEE — promoting `??_7EdgeMesh@Graphics@@6B@`]** the next lane scoped
> to Graphics promotion, or the Wave 28 kickoff writer if no lane is. The work
> is ONE measurement, not a port: take a run with
> `BANISHED_SELECTION_OUTLINE=1` and a building selected, and establish whether
> every live `EdgeMesh` is constructed through our registered
> `??0EdgeMesh@Graphics@@QEAA@XZ`. If it is, the row is promotable on the
> DLL-present route; if some are built by the DLL's own copy, it is a SPLIT and
> promoting stamps our table into only half the objects.

`??_7EdgeMeshInstance@` (7/9) and `??_7MeshInstance@` (5/9) are not COMPLETE, so
the question does not arise for them yet.

---

## 6. WHAT THIS LANE MADE NEWLY `called` — the inherited cutover work

`reference_porting_can_grow_the_cutover_surface.md`. Named here so no future
census has to rediscover them. **None is claimed as ported by this lane.**

| symbol / address | reached from | door |
|---|---|---|
| ~~`?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@HM@Z` 0x180016c20 ord 2814~~ | `EdgeMeshInstance::GetInstanceTransform` | **CLOSED — ported by this lane** |
| `??1EdgeMesh@Graphics@@UEAA@XZ` 0x1800a2780 ord 580 | `deleting_dtor.cpp` row 44 | ADDRESS |
| `??1EdgeMeshInstance@Graphics@@UEAA@XZ` 0x1800174d0 ord 581 | `deleting_dtor.cpp` row 45 | ADDRESS |
| (unexported) 0x180014230 `SerializeExternalPtr<Base>` | `EdgeMesh::Serialize` | ADDRESS |
| (unexported) 0x180014520 `SerializeExternalPtr<MaterialInstance>` | `EdgeMesh::Serialize` | ADDRESS |
| (unexported) 0x180014850 `SerializeTriangleListArray` | `EdgeMesh::Serialize` | ADDRESS |

0x1800136f0 (`Array<TriangleList>::SetSize`) is also reached, but it was already
reached by `mesh.cpp:201` and is deliberately resolved the same way so there is
ONE place to repoint.

**0x180016c20 is a different function from 0x180016a40.** Both are exported,
both are `?GetNodeToWorld@MeshInstance@Graphics@@`, and the two mangled names
differ only in a trailing `M`. Exactly the pair
`reference_resolveexport_phantom_names.md` warns about, so the VA was confirmed
against the export directory rather than the name being assumed to exist.

> **CORRECTION (ENDGAME B.3.5 closing pass, b35-close, 2026-09-19).** This
> paragraph used to end "— the `(int)` overload is ported, the `(int,float)`
> overload is not", which CONTRADICTED the paragraph immediately below it in this
> same section ("0x180016c20 IS CLOSED — this lane ported it") and the table row
> above it (struck through, "CLOSED — ported by this lane"). The "is not" half was
> stale the day it was written. **BOTH OVERLOADS ARE PORTED**, and both are in
> `runtime/src/graphics/mesh.cpp`: the `(int)` overload at :531 (retail
> 0x180016a40, ordinal 2813) and the `(int, float)` interpolated overload at :1663
> (retail 0x180016c20, ordinal 2814), each with its own registered door. The rest
> of the paragraph — the VA-versus-name warning, which is the part worth keeping —
> stands unchanged.

**0x180016c20 IS CLOSED — this lane ported it** rather than handing it on, and
that is why the BODY gap reads 18 at the tip instead of 19. Instruction for
instruction it is the already-ported `(int)` overload with every pose read
replaced by a LERP of a Transform pair by `t`: same two arms, same `_instanced`
byte, same pool indirection, same identity seeding, same two-call composition. It
reuses `RtInstancedPoolSlot`, so the MOVSXD / SAR / second-deref corrections
`WAVE6_LANE_E.md` §6.7-6.8 paid for keep exactly one owner. Its whole constant set
was confirmed with `objdump` against the retail file (§8.5): the
`cmpb $0x0,0xd8(%rcx)` instanced test, `movslq %r8d,%rsi`, both
`call 0x18001d500`, the `movslq 0xe8(%rdi)` / `imul $0x38` /
`movslq -0x2c(%rcx,%rax,1)` pool-id chain, `add 0x38(%rax,%rdx,8),%r8`,
`movq $0x3f800000,0xc(%rbx)`, `call 0x18001e140`, `call 0x18001e280`, and the
simple arm's `add 0xf0(%rcx),%r8` then `call 0x18001d690`.

**The five other rows in the table above are NOT closed.**

> **[ADDRESSEE — the three unexported `EdgeMesh::Serialize` helpers
> 0x180014230, 0x180014520, 0x180014850]** the lane that next ports
> `Object::ExternalPtr` serialization, or the cutover lane that finds them in the
> `[RtRawResidue]` list — whichever comes first. They are template instantiations
> with no export entry, so they need address doors. The first two are the SAME
> body differing only in the `?_typeInfo@` they type-check against
> (`GraphicsMesh` 0x1801fa708 vs `MaterialInstance` 0x1801fa778), which is why
> retail emits two and why they must not be collapsed into one.

> **[ADDRESSEE — `??1EdgeMesh@` 0x1800a2780 and `??1EdgeMeshInstance@`
> 0x1800174d0]** the next lane scoped to Graphics destructors. Both are exported
> (`UEAA`, ordinals 580 and 581), so both are plain NAME ports, and both are
> reached today only by this lane's two deleting-destructor rows.

---

## 7. THE `AddDrawItems` TAG — what this lane did, and what it deliberately did not

The tag at `standalone/reg_mint_promoted.cpp:2257`:

> `?AddDrawItems@MeshInstance@Graphics@@…` — registering it is harmful while the
> DLL supplies vtable[3]; **voids when `scene_watch.cpp:401` stops using this
> address as an identity token** `[SCOPE: DLL-PRESENT]`
> `[ADDRESSEE: the lane that next needs AddDrawItems registered — it must give
> scene_watch a different identity token first]`

### The tag was RE-VERIFIED before it was acted on. It was still true.

`standalone/scene_watch.cpp` `IsMeshInstance` did resolve `0x180100f20` through
`ResolveAddr(Module::Runtime, …)` and compare it against `vtable[3]` of a live
object. `ResolveAddr` **consults the port registry first**, so the moment any
lane registered a port of `AddDrawItems` the compare would fail for every
DLL-constructed object and `BANISHED_LOD_CENSUS` / `BANISHED_FORCE_LOD0` would
silently observe an **empty scene** — a wrong answer with no error.

This mattered to check. Three of Wave 20's cheap-wins items were stale on
contact, and a tag naming a condition that is already false is a documented
failure mode (`reference_a_matching_number_can_match_because_it_is_stale.md`,
`reference_four_tag_defects_all_well_formed.md`). This one was not stale.

### DONE: the prescribed fix, in the prescribed order, FIRST.

`IsMeshInstance` no longer asks `ResolveAddr`. It now matches `vtable[3]` against
**either** of two tokens:

- `ResolveRuntimeRaw(0x180100f20)` — the **registry-free** door. This is what the
  DLL's own constructor actually stamped, so it is correct *by construction* for
  every DLL-built object, and **nothing any lane registers can move it**.
  `ResolveAddr` could never answer this question: hiding the DLL's address is the
  port registry's job, and here the DLL's address is the subject.
- `RtPortLookupAddr(0x180100f20)` — our port, if one is ever registered. Objects
  constructed after a promotion of `??_7MeshInstance@` would carry OUR slot, and
  a test that knew only the DLL's would miss that population.

Two tokens cover both populations. One covers whichever half you happened to
think of — and the version this replaced covered the wrong one.

The grep the fix rests on, re-run on the lane tip:
`grep -rn 0x180100f20 runtime/ code/ standalone/ engine/ dx11/` returns the one
live consumer (`scene_watch.cpp`), `rt_data_mint_tables.inc` (which **stamps**
the address, not compares it), and comments. **No second identity-token consumer
exists.**

**The HARMFUL verdict is therefore RETIRED.** The retirement is written **beside
the tag**, not only here — `reg_mint_promoted.cpp` now carries the "THE CONDITION
ABOVE IS NOW FALSE" block at the row, and `mesh.cpp`'s HELD BACK note item 1(b)
is marked dead at its own site. A correction has to live on the surface the next
person reaches (§4 #9); a lane doc has the weakness it describes.

### DECLINED, explicitly: this lane did NOT register `AddDrawItems`.

The brief asked me to say so rather than silently register and void the tag's
premise. **The body is not ported.** Removing a blocker is not doing the work,
and registering a symbol with no body behind it is the exact shape of
`reference_half_registered_port_reads_as_absent.md` run in reverse.

What is left: `?AddDrawItems@MeshInstance@` is 0xd0 bytes of LOD selection — a
squared-distance compare against the **union** of `_worldBounds` and
`_prevWorldBounds`, walking `_detailLevels` at stride 0xb0 — followed by one
`?AddDrawItem@Renderer@` call per `TriangleList`, and that call has a
nine-argument frame including a packed `short` built from
`(level << 12) | listIndex` and a `_sortBias` sum. `mesh.cpp`'s reason **(a)**
also still stands unchanged and is untouched by anything above: it is a virtual
the DLL dispatches, so a port of it has **zero callers** until
`??_7MeshInstance@` is promoted or the DLL is gone.

> **[ADDRESSEE — porting `?AddDrawItems@MeshInstance@Graphics@@`]** the next lane
> scoped to it. It may now register the symbol **with no `scene_watch` change of
> its own** — that debt is paid. It must still re-run the grep above at that
> time and confirm no new identity-token consumer of `0x180100f20` has appeared.

### The other four BLOCKED slots, with real reasons

| slot | why not this lane |
|---|---|
| `?Update@MeshInstance@` 0x180100800 | Lazily initialises a 48-entry × 0x1c **static table** at 0x1801ff310 behind a flag at 0x1801ff30c, then calls `AnimationState::Update` (0x1800fdb20) with a **nine-argument** frame. The static is the hazard, not the arithmetic: our copy and the DLL's are two different arrays, and initialising ours while the DLL reads its own is a split brain that no diff screen in this tree would see. Needs its own decision about minting that symbol. |
| `?Draw@MeshInstance@` 0x180100960, `?Draw@EdgeMeshInstance@` 0x1800a3150 | Both bind and issue through the live `Video::Context`. A replay oracle re-issues draw calls; `docs/runtime_port/WAVE23_CLOSED.md` §3 records that a pixel diff between two runs measures nothing, so the only instrument that could judge these is `tools/trace_oracle/`, and standing up a trace comparison is a lane of its own. |
| `?AddDrawItems@EdgeMeshInstance@` 0x1800a2fd0 | Same dependency on `Renderer::AddDrawItem`'s nine-argument frame as its `MeshInstance` sibling. Porting one without the other would leave the two overrides of one slot derived from different readings of that call — which §4 shows is precisely how this family goes wrong. |
| `?SetSharedInstanceData@MeshInstance@` 0x180101660 | Writes instance data into the live `Video::Context`'s constant buffer. Same instrument problem as `Draw`. |

---

## 8. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Per WAVE27_KICKOFF §4 #1. Three, none fixed, all recorded with file and line.

### 8.1 `tools/rt_deleting_dtor_verify.py:260-273` — inflates, 2 false positives

Prints `BAD` for two of this lane's three **correct** new rows:

```
BAD 0x1800175d0 ... scalar branch neither calls 0x1800174d0 nor restamps a vtable
BAD 0x180017330 ... scalar branch neither calls 0x180017060 nor restamps a vtable
```

The checker knows exactly **two** shapes for the scalar branch — K1, it CALLs
`~Class`; K2/K3, it inlines a `~Class` that *begins with a vtable restamp*, which
the tool matches on — and its third arm is a bare `else` that reports everything
else as a problem. These two rows are a **third** shape: the scalar branch inlines
a `~Class` whose own body contains **no restamp at all**, so there is nothing to
match and the tool cannot distinguish an inlined-correct row from a wrong one.
It announces a verdict without being able to examine its subject
(`reference_a_screen_that_cannot_name_its_subject.md`) and it **inflates**: two
false positives, no false negatives.

Both were hand-verified instruction by instruction (`0x1800174d0` is 0x1a bytes
of `JMP 0x1800a41c0` and the scalar branch is `CALL 0x1800a41c0`; `0x180017060`
is four calls on +0xf0, +0xd8+8, +0xc8 and `this`, and the scalar branch is the
same four on the same four in the same order). The correction is written
**beside the rows** in `deleting_dtor.cpp`, where the next reader meets it before
the tool's verdict.

> **[ADDRESSEE]** whoever scopes a Wave 28 instruments lane. The fix is a K4 arm
> accepting a scalar branch whose call set equals the call set of the named
> `~Class`.

### 8.2 `tools/addr_door_audit.py:47` — blind to an address passed as a parameter

Harvests by-address doors with
`ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)`, so an address handed
to a wrapper as a **parameter** is invisible: the door exists and no audit can
see it. This lane's first draft of `edgemesh_vtable.cpp` used a shared
`RtAddrOnce(va, &cache)` template and its four doors — three of them **unported
DLL internals that SHOULD appear in the UNREGISTERED list** — did not appear
anywhere in the audit's output.

**Not fixed in the tool. Worked around in the source**: the file now spells the
literal at each `ResolveAddr` call, one accessor per helper, with a comment saying
why that is not a style choice. This is the fifth recorded instance of
`reference_literal_grep_blind_to_variables.md`.

**AND THE RESPELLING FOUND SOMETHING THAT WAS NOT THIS LANE'S.** After it, the
audit reports UNREGISTERED 21 → 25 and names
`runtime/src/graphics/edgemesh_vtable.cpp` with **four**. Three are this lane's
genuinely-unported `EdgeMesh::Serialize` helpers and belong there. **The fourth is
`0x1800136f0`, `Array<GraphicsMesh::TriangleList>::SetSize` — an unregistered
by-address door that `mesh.cpp:201` has held since WAVE 6**, spelled
`RtAddr<ArrayResizeFn>(0x1800136f0)` through that file's own wrapper template and
therefore invisible to this audit for the entire time it has existed. Nothing was
wrong with the door; what was wrong is that no audit could count it, which is the
whole hazard the reference describes. It surfaced only because a second file
reached the same address through the spelling the tool can read.

> **[ADDRESSEE — `mesh.cpp`'s wrapper-hidden address doors]** whoever scopes a
> Wave 28 instruments lane, alongside §8.1. `mesh.cpp` reaches at least seven
> `Collection::Array` instantiations through `RtAddr<Fn>(literal)`
> (`mesh.cpp:183-201`) and every one of them is invisible to
> `addr_door_audit.py`. The cheap half is respelling them; the half that matters
> is deciding whether the audit should learn to follow a one-line wrapper, because
> this house style is not unique to `mesh.cpp`.

### 8.3 `get_function_metrics` ignores `address=`

Five calls with `address=` returned `Function not found: 0x…` for functions that
`disassemble_function` renders at those exact addresses — it silently falls back
to a NAME lookup. Already recorded as
`reference_ghidra_mcp_address_param_silently_ignored.md`; confirmed live on this
tip, not fixed, worked around by using `disassemble_function` to size bodies.

### 8.4 The retail/profile check, run mid-lane on the integrator's alert

`disassemble_function(0x180002190)` returned the 2-instruction **RETAIL** body
both with `program="Runtime-x64.dll"` and with the parameter omitted, so this
lane's default program state was serving retail throughout. Not treated as
sufficient: the four most offset-sensitive bodies were re-taken with `program=`
explicit and diffed against what had already been derived — `EdgeMesh::Serialize`
(which alone carries five Encoding/factory vtable offsets: +0x18, +0x20, +0xa0,
+0x28, +0x30), both `GetInstanceTransform` overrides, and the two branch-sense
bodies. **All identical. No re-derivation debt.**

Two structural facts made this cheap and are worth carrying forward: every export
VA in this lane was confirmed with `pefile` against the retail file on disk —
never through Ghidra — and the three deleting-destructor rows' constants are
independently re-derived from the retail PE image by
`tools/rt_deleting_dtor_verify.py`.

### 8.5 `disassemble_function` can serve a DIFFERENT function's listing — and the §8.4 diff did not cover it

Reported by lane GFXBIG mid-wave: two overlapping bodies at `0x1800fdb20`, and the
tool returned the wrong one, **internally consistent and relabelled with the
requested address.** This defeats §8.4's diff, and the reason is worth stating
plainly because it is a reasoning error, not a tooling one:

> **§8.4's re-take was Ghidra against Ghidra. That is ONE source consulted twice,
> not two sources.** It could only ever have caught program-selection drift
> *between* the two calls. Against a body that is consistently wrong it agrees
> with itself, which is what `reference_two_checks_one_hidden_premise.md`
> describes. `program="Runtime-x64.dll"` does not help either — you can be
> reading the correct binary and still be handed the wrong function out of it.

**A REAL SECOND WITNESS WAS THEN TAKEN**, with `objdump` reading the retail file
on disk by explicit path (true VA addressing, so the address typed is the address
disassembled — no program selection and no symbol lookup involved):

```
objdump -d --start-address=<va> --stop-address=<va2> "C:/GOG Games/Banished/Runtime-x64.dll"
```

The discriminator reproduced first (`0x180002190` → `mov 0x1f8c79(%rip),%rax ; ret`),
then the two bodies where an invisible mixup would have been most costly:

- **`EdgeMesh::Serialize` 0x1800a21d0 — the dense one, and the whole point of the
  exercise.** Every constant the port depends on reproduced **byte for byte**:
  the factory's `call *0x18(%rax)` / `call *0x20(%rax)` pair; `call *0xa0(%rax)`
  twice; `call *0x30(%rax)` eight times; `call *0x28(%rax)` twice; `call *(%rax)`
  once; the helper targets `0x180014230`, `0x180014520` ×4, `0x1800136f0` ×2,
  `0x180014850` ×2; the `ExternalPtr` slots `0x20/0x28/0x30/0x38/0x40`; the size
  reads `0x150` and `0x160`; the eight buffer slots in retail's order
  `0x70, 0x50, 0xb0, 0x90, 0xf0, 0xd0, 0x130, 0x110` — index before vertex in
  each pair; and both bounds runs `0x168` and `0x174`.
- **`MeshInstance::GetInstanceTransform` 0x180101280 — §4's finding.** The sixteen
  `movss` stores into `%rdi` are there, and so are the four reads that make the
  finding: `movss 0x10c(%rbx),%xmm3 → 0xc(%rdi)`,
  `0x110 → %xmm7 → 0x1c(%rdi)`, `0x114 → %xmm11 → 0x2c(%rdi)`,
  `0x118 → %xmm15 → 0x3c(%rdi)`. **The colour column is confirmed by a second,
  independent disassembler.** §4 is not resting on one tool.

**Not fixed** (§4 #1). The generalisable rule, which this lane's `pefile` use had
already demonstrated from the other side: **a constant that can be read from the
PE should be read from the PE.** `objdump` is the cheapest way to do that for
machine code, and it is what a dense body's offsets should be checked against
regardless of whether any Ghidra defect is currently suspected.

---

## 9. WHAT THIS LANE DID NOT DO

- **Did not launch the game.** No run, no `game_dir_lock.sh` acquisition, no
  attendance measurement. Every claim here is static or build-time. In
  particular: the thirteen bodies have never been observed executing, and §5's
  promotion question is unanswered *because* it needs a run.
- **Did not promote any vtable row.**
- **Did not register `?AddDrawItems@MeshInstance@`** (§7, declined explicitly).
- **Did not touch another lane's tables.** `Particle*`, `Light`, `SunLight`,
  `Renderer::Draw`, `MaterialInstance::BuildParameters`,
  `AnimationState::Update`, the Engine loops, Achievement, IO and
  `ElementDescription` are untouched. The one shared file edited outside the
  three tables is `runtime/src/object/deleting_dtor.cpp`, appended to (never
  inserted into — `kProbeSubjects` is a list of table indices and a mid-table
  insert silently re-points all twelve `Video::*Data` probe subjects).
- **Did not fix any tool** (§8).

## 10. KILL SWITCHES

| var | turns off |
|---|---|
| `BANISHED_RT_GFXEDGEVT=0` | the eight `EdgeMesh` vtable slot bodies |
| `BANISHED_RT_GFXMESHVT=0` | the four instance-table slot bodies |
| `BANISHED_RT_DELDTOR=0` | all 47 deleting-destructor rows, this lane's three included |

Both new switches are separate from `BANISHED_RT_GFXMESH` on purpose: that one
gates twenty pre-existing non-virtual bodies, and an A/B that cannot separate
this lane's thirteen from those twenty is not an A/B.

## 11. FILES

| file | change |
|---|---|
| `runtime/src/graphics/edgemesh_vtable.cpp` | **new** — EdgeMesh slots 1..8 |
| `runtime/src/graphics/mesh.cpp` | Wave 27 section: 4 instance-table bodies + `RtGraphicsMeshVtableRegister`; address door for `~MeshInstance`; HELD BACK item 1(b) marked dead |
| `runtime/src/object/deleting_dtor.cpp` | 3 rows, 3 entry points, 3 registrations, the §8.1 correction |
| `runtime/src/object/deleting_dtor_internal.h` | `kSpecCount` 44 → 47 |
| `runtime/src/graphics/rt_graphics_port.cpp` | two new gated register calls + identity lines |
| `standalone/scene_watch.cpp` | `IsMeshInstance` rebuilt on two registry-independent tokens |
| `standalone/reg_mint_promoted.cpp` | the `AddDrawItems` tag retired in place, with the remaining work re-addressed |
