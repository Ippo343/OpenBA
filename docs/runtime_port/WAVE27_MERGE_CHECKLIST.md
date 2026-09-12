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

## BOARD: PRESENT BY IDENTITY, NOT PERCENTAGE

Four lanes have now independently moved the percentage **down while doing real
work**, because a faithful by-address port grows `called`:

| lane | board after its own work |
|---|---|
| base | 1372/1237 90.2% |
| FRONTIER | 1378/1241 90.1% |
| GFXPART | 1376/1237 89.9% (`ported` cannot move — promotion-backlog rows) |
| IOTEXT | 1377/1242 90.2%, `DTOR 2 -> 0` |
| GFXBIG | Graphics `BODY` gap 4 -> 11 (seven newly *visible* named exports) |

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

## NAMED FOR WAVE 28, NOT DONE HERE

- `?Update@ParticleSystemInstance@` (`0x1800a5600`, 0x1479 bytes — the whole
  particle sim, 12 inlined RNG draws). Deserves a GFXBIG-shaped lane of its own.
- `SunLight`'s five remaining bodies; `TextFileStream` slots 2 and 8.
- `?SetupFileSystem@Modification@IO@@` — **writable but not verifiable**: the only
  unattended mod route is the broken `ModDialog_ToggleMod_Impl+0x43` one. Pair it
  with whoever fixes that route; doctrine says find the upstream null.
- `Particle::Serialize`'s write-path `RtSaveTrace` oracle — available, not built.
- `?_typeInfo@ElementDescription@UI@@` is minted but not promoted; promoting it
  converts two by-construction ports into measured ones.
- Every ledger entry.
