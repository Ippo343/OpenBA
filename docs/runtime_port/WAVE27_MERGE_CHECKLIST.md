# WAVE 27 — INTEGRATOR MERGE CHECKLIST

Residuals the lanes addressed to **the integrator, at merge**. Kept separately
from the lane docs because a residual with an addressee only fires if the
addressee keeps a list. Built as reports arrive, not at merge time.

## MUST DO AT MERGE

- [ ] **`python tools/cutover_manifest.py --inc`** — IOTEXT's two new by-name doors
      moved the door count, so `cutover_manifest.inc` prints a cosmetic `GATE WARN`.
      Row set and dispositions still match and the build does NOT fail. IOTEXT
      deliberately did **not** refresh it from a lane branch, which is correct:
      Wave 26 defect #4 was a STALE `.inc` committed from one lane's view of the
      world. Regenerate once, on the merged tip, after the last lane lands.
- [ ] **Re-derive the board on the FINAL merged tip**, after the `.inc`
      regeneration — Wave 25's rule, which Wave 26's own close obeyed. Do not sum
      the lanes' individual figures.
- [ ] **`rt_mint_gen.py --check`** and **`runtime_census.py --selftest-all`**.
- [ ] **Cutover attempt 30.**
- [ ] **`??_7UpdateLoop@Engine@@6B@` IS PROMOTED by LOOPS** — the one promotion
      claimed in this wave, and it was the wave's one explicitly open question.
      LOOPS settled it and recorded two corrections to the holdback note, including
      that its literal test **cannot distinguish a ctor from a dtor**, making it
      unsatisfiable for any class with a virtual dtor. Re-read that before trusting
      any similarly-worded holdback elsewhere.
- [ ] **LOOPS reversed itself on three promotions and shipped them OPT-IN** —
      promoting the three derived tables routed ZERO bodies and cost three red
      oracles. Confirm they are still opt-in on the merged tip; do not "tidy" them
      to default-on.
- [ ] **Watch two ACHIEVE probes on the merge run** — every oracle it wired is
      UNEXERCISED, because lane LOOPS held the game-dir lock for ACHIEVE's whole
      600s window (verified: legitimately held, heartbeat current — ACHIEVE
      misattributed it to the integrator, but handled it correctly by killing its
      own blocking acquire rather than letting a grant go unnoticed):
      - `Achievement::AchievementManager::ctor` — full byte oracle
      - `Achievement::GoalList::Initialize` — shadow; **`firstWrongGoal >= 0`
        means the ownership MOVE is transposed**
- [ ] **ELEVEN of LOOPS' twelve new bodies have NO oracle** — byte-derived and
      registered, never executed. LOOPS says in as many words: *"ported + registered
      should not be read as verified."* The same is true of most of GFXMESH's 14,
      GFXPART's 8 and ACHIEVE's 16. **The wave ported ~72 bodies and verified a
      minority of them by execution.** Say so in the close; it is the single most
      misleading thing available to omit.
- [ ] **Confirm `?_singleton@AchievementManager@Achievement@@` is still NOT
      promoted.** ACHIEVE retired HALF the blocker (the ctor is ported and
      registered with a full-byte oracle); the other writer is a store inside
      `??0DialogLoopT@Engine@@`, which is lane LOOPS's table. **The tag STANDS and
      the two-owners-one-object hazard is live.** ACHIEVE wrote the correction
      beside the tag in `reg_mint_promoted.cpp` and messaged LOOPS directly.

## BOARD: PRESENT BY IDENTITY, NOT PERCENTAGE

Four lanes have now independently moved the percentage **down while doing real
work**, because a faithful by-address port grows `called`:

| lane | board after its own work |
|---|---|
| base | 1372/1237 90.2% |
| **LOOPS** | **1392/1262 90.7%, gap 130 — Engine 77.8% -> 94.1% AT CEILING** |
| FRONTIER | 1378/1241 90.1% |
| GFXPART | 1376/1237 89.9% (`ported` cannot move — promotion-backlog rows) |
| IOTEXT | 1377/1242 90.2%, `DTOR 2 -> 0` |
| GFXBIG | Graphics `BODY` gap 4 -> 11 (seven newly *visible* named exports) |

**LOOPS is the one lane whose percentage went UP, and it is not the lane that did
the most work** — it is the lane whose bodies were mostly already-`called` vtable
slots rather than new by-address doors. That is the whole argument for scoring
identities: the sign of the percentage tracks *what kind* of row a lane touched,
not how much it achieved.

**A reader who diffs the merge on the percentage will conclude the wave went
backwards.** The close must lead with identities: tables reaching COMPLETE, the
`DTOR` column emptied, the attempt-29 wall down. See
`reference_the_ceiling_percentage_is_a_moving_target.md`.

## PROMOTION: NOT ONE LANE CLAIMED IT, AND THAT IS THE RIGHT ANSWER

FRONTIER, GFXPART and GFXMESH each reached COMPLETE tables and each **declined to
promote**, because none established ownership of the stamping function. That is
KICKOFF §1 applied: COMPLETE is the static half of readiness, and a green
readiness screen is not a route.

**Do not treat the pile of COMPLETE tables as a pile of cheap promotions at
merge.** Every stamper ratio quoted in Wave 26 came from a tool later found
DEFECTIVE in the DEFLATING direction, and its fix landed late. A promotion wave is
a Wave 28 lane with its own measurement, not an integration step.

## OPEN CUTOVER OBLIGATIONS OPENED BY THIS WAVE

Each is commented at its call site in SYMBOL/CONDITION/CONSEQUENCE form, per
KICKOFF §4 #10. None is a merge blocker; all are Wave 28 inputs.

| address | what | opened by | gates |
|---|---|---|---|
| `0x180062d60` | 2311-byte draw-item sort, hit ~every frame | GFXBIG | `Renderer::Draw` DLL-free; items go out UNSORTED at cutover (artefacts, not a crash) |
| `0x18005aa30`, `0x18005b1e0` | ~400 b each; two more `ArraySpec` rows, not two bodies | GFXBIG | a buffer leak on a rare layer-expiry arm |
| `0x180029bb0` | ~680-byte array-of-String serializer, unexported | IOTEXT | IO cutover |
| `0x18007ec98` | MSVC `eh_vector_destructor_iterator` | FRONTIER | counted at runtime, addressee named |
| `0x1800136f0` | **unregistered door at `mesh.cpp:201` SINCE WAVE 6** | GFXMESH (found via ledger #8) | pre-existing, NOT opened by this wave — 21 waves invisible |
| — | 5 new Runtime symbols referenced by ACHIEVE's 16 bodies | ACHIEVE | `called` +5, `ported` +2; attributed, not estimated |

## NAMED FOR WAVE 28, NOT DONE HERE

- `?Update@ParticleSystemInstance@` (`0x1800a5600`, 0x1479 bytes — the whole
  particle sim, 12 inlined RNG draws). Deserves a GFXBIG-shaped lane of its own.
- `SunLight`'s five remaining bodies; `TextFileStream` slots 2 and 8.
- `?SetupFileSystem@Modification@IO@@` — **writable but not verifiable**: the only
  unattended mod route is the broken `ModDialog_ToggleMod_Impl+0x43` one. Pair it
  with whoever fixes that route; doctrine says find the upstream null.
- `Particle::Serialize`'s write-path `RtSaveTrace` oracle — available, not built.
- **Audio: neither item ported** (`?Run@Stream@Audio@@` ~1800 b worker-thread file
  IO; `?Update@Scene@Audio@@` ~1816 b). ACHIEVE re-checked both stated blockers and
  reports them **out of budget, NOT newly blocked** — and that Wave 24 lane
  SMALLBODY had already re-checked `Scene::Update`'s blocker as "NOT A WALL". The
  brief's stored-callback hint described neither. Clean scope for a Wave 28 lane.
- **`AchievementInstance::Persist` prefix oracle uses `stopAfter=1`, a FLOOR the
  lane chose, not the index `save_trace.h` asks for.** It could not derive the
  index and **declined to guess**. Tighten it in Wave 28. The body is the family's
  outlier anyway: two nested codecs, outer version **1** where every sibling
  passes 0, plus a real `GetVersion()==0` early return.
- `?_typeInfo@ElementDescription@UI@@` is minted but not promoted; promoting it
  converts two by-construction ports into measured ones.
- Every ledger entry — **#8 first** (the only DEFLATING one) and **#11 next**
  (wrong bytes, self-corroborating, and it bears on trust in lane outputs).
- **`?_loader@LoaderLoop@Engine@@` was NOT retired, and the reason matters:** LOOPS
  satisfied the written expiry condition and **the row is still unpromotable — the
  condition named the wrong fact.** It and `??_7Loader@Engine@@` are now
  route-scoped refusals retiring only at cutover, addressed to the cutover lane.
  This is the fourth time this project has found an expiry clause that was well
  formed and wrong; see `reference_four_tag_defects_all_well_formed.md`.

## ONE TAG WAS RETIRED THIS WAVE, CORRECTLY, AND IT NEEDS RE-READING AT MERGE

GFXMESH was the named addressee of the `?AddDrawItems@MeshInstance@Graphics@@`
HARMFUL tag. It did the right things in the right order:

1. **Re-verified the tag was still true** before acting (it was).
2. Did the **prescribed fix first** — `scene_watch.cpp`'s `IsMeshInstance` now
   matches `vtable[3]` against `ResolveRuntimeRaw` (registry-free, unmovable) OR
   `RtPortLookupAddr`. **Two tokens**, so it is correct before *and* after any
   future promotion.
3. **Rewrote the expiry clause itself**, not merely a comment beside it, because
   the census echoes that clause verbatim — KICKOFF §4 #9, applied to the surface
   that actually gets read.
4. **Declined to register `AddDrawItems`**: the body is not ported, and *removing
   a blocker is not doing the work.* Addressee named for whoever ports it.

**At merge, confirm the census's echoed clause matches the rewritten tag** — the
one thing that could go wrong here is the doc and the echo drifting apart, which
is the failure the rewrite was meant to prevent.
