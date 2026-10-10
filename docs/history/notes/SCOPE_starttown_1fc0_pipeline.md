# Scope: faithful port of the StartTown building-assembly pipeline (`1fc0`)

**Date:** 2026-07-07 · **Branch:** wave/phase3-keystone-d
**Motivation:** recurring "ModelComponent field is 0/wrong" bugs (transform +0x158,
variant +0x164, modelVariant +0x166). ModelComponent's *own* methods are faithfully
ported already — the gap is the **spawn/assembly pipeline that populates those fields**,
which our `StartTown_SpawnStructures` only partially (~70%) hand-ports.

---

## The target

| fn | addr | role |
|----|------|------|
| `ComponentEntity_impl_1fc0` | `0x1400a1fc0` | **building assembly** — per-part: CopyCtor child entity, set MapComponent footprint (+0x28..+0x30), set ModelComponent transform (+0x148) + **random variant** (`FUN_1401253d0`), register. 240 instr, single function. |
| `FUN_1400c2ea0` | `0x1400c2ea0` | **deferred world-activation pass** — fires `vtable+0x38` (OnCreate) on every component with flag bit 1, sets the `0x400` "activated" sentinel. |

Driver `FUN_1400a3300` (StartTown) calls `1fc0` as pass 3 of ~7:
`a2720`(spawn tile) → `002080`(world pos) → **`1fc0`(structures)** → `a12b0`(nature) →
`0dd0`(citizens) → `09e0`/`02f0`(simple pools) → destroy/remove passes.
Our `starttown.cpp` already mirrors this ordering.

---

## Dependency readiness — GOOD

Every function `1fc0` calls is **already registered/available in standalone**. No new
function ports required; this is an assembly-only port.

| callee | addr | SA status |
|--------|------|-----------|
| tile→world pos | `0x14009f3e0` | ✅ reg_action_child_ctors |
| ComponentEntity CopyCtor | `0x1400c3720` | ✅ address_registry (`fn_EntityFactory_CreateEntity`) |
| EntityList::RegisterEntity | `0x1400c48a0` | ✅ address_registry |
| SetVariant (`1253d0`) | `0x1401253d0` | ✅ reg_action_cnr |
| String normalize | `0x14001b1d0` | ✅ engine_funcs |
| activation pass (`c2ea0`) | `0x1400c2ea0` | ✅ reg_action_hunt |
| FromYawPitchOffset / Geometry::SetTransform / MemAlloc / Rand / IsDerivedFrom | dispatch slots | ✅ all registered |

---

## The 4 type-id `.rdata` constants `1fc0` matches on

`1fc0` finds components by CRC type-id read from raw `.rdata`. These are **NULL in
standalone** unless explicitly seeded (`.data` filled by `RegisterType` in retail):

| const | type | SA status |
|-------|------|-----------|
| `DWORD_1401c7468` | CreatePlacedDescription (footprint) | our port uses `GetTypeIdByName` ✅ |
| `DWORD_1401c79e0` | MapComponent | effectively works (MapComponent find works) |
| `DWORD_1401c7a50` | **ModelComponent** | ❌ **NULL** — this is exactly today's decal bug (`FindComponent(ModelComponent)` in `0x140003f60` returns null → decal read variant 0). Worked around locally in `decalcomponent.cpp`. |
| `DAT_1401c7a88` | MultiObjectDescription | used by `ModelComponent_Serialize` |

**Recommendation:** seed all 4 centrally (`RegisterExeAddr(addr,&storage)` +
`GetTypeIdByName` fill after `Engine::Startup`, per the `reg_component_spawn.cpp`
pattern). This **retroactively fixes the decal's FindComponent** (removing the local
`decalcomponent.cpp` workaround) and any other `0x140003f60`/`0x140004680` caller.

---

## The landmine: the activation pass (`c2ea0`)

Faithful `c2ea0` fires `vtable+0x38` (OnCreate) on **every** bit-1 component. Our
current `StartTown_RunOnCreatePass` deliberately fires it on **ResidenceComponent only**,
because (per the existing comment at `starttown.cpp:442`) firing `ModelComponent::OnCreate`
**removes the mesh from the scene / resets modelId → the building goes invisible.**

Root cause of the tension — **inverted ordering vs retail:**
- retail: `1fc0` sets variant/transform → `RegisterEntity` → **deferred activation fires OnCreate** → *later* a render pass fires OnAdded/Draw (scene-add).
- SA: our `RegisterEntity` path fires **OnAdded (scene-add) inline**, so OnCreate would run *after* the mesh is already in the scene and tear it back out.

Resolving this (so the full faithful activation pass is safe) is the **risky ~40%** of
the work. It requires untangling the OnAdded-vs-OnCreate ordering, not just transcribing
`1fc0`.

---

## Effort / risk

| item | size | risk | value |
|------|------|------|-------|
| Central type-id seeding (4 consts) | S (~1h) | low | fixes decal find + all future finds; removes local workaround |
| Faithful `1fc0` transform/variant/footprint block (replace partial `SpawnStructures`) | M (~2–4h) | low (all deps ready) | kills transform/variant gaps at the source |
| Activation-pass ordering + full faithful `c2ea0` | M–L | **high** | removes the surgical Residence-only hack; fully faithful lifecycle |

**Recommended two-phase plan:**
- **Phase 1 (low risk, high value):** central type-id seeding + finish the faithful
  `1fc0` transform/variant/footprint block. Keep the surgical Residence-only activation.
- **Phase 2 (needs care):** resolve OnAdded/OnCreate ordering, then enable full `c2ea0`.

---

## Caveat — this does NOT fix the decal visual

The residual decal rendering bug (tessellator int16 **V still folds negative** even with
the correct atlas cell now selected) is in the `ace40` render layer, **orthogonal** to
the spawn pipeline. The full `1fc0` port corrects the *data* (variant selection) — already
achieved via the local decal fix — but the decal will still need a separate RenderDoc pass
on the `ace40` V-transform / sampler address mode regardless.
