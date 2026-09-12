# WAVE 23 LANE GFXVT — the 8 Graphics VTABLE rows, re-derived

Branch `wave23/gfxvt`, cut from `wave23/base` at `a60351e3`. Private worktree
`.claude/worktrees/agent-aae49b24fc7b9433d`. Closed 2026-09-11.

**All census figures in this document are BRANCH-LOCAL** (kickoff rule 13). The
integrator must re-measure on the merged tip; nothing here was measured anywhere
else.

---

## 0. HEADLINE — readiness and value are ANTI-CORRELATED in this set

**Both COMPLETE rows are ABSTRACT base classes whose table is overwritten one
statement after it is stamped. All three rows whose promotion would actually
change a live dispatch are INCOMPLETE.** The readiness screen ranks this set in
almost exactly the wrong order, and the axis that sorts it correctly — *is the
class abstract?* — falls straight out of a number the backlog already prints and
nobody reads as a verdict: the **purecall count**.

A class with a pure virtual cannot be instantiated. Its vtable is therefore never
a live object's final vptr; it exists only inside the base ctor/dtor window and is
immediately overwritten by the derived ctor. Promoting such a row moves a pointer
that nothing dispatches through.

Second result, and it is a correction to a one-turn-old document:

**`??_7Geometry@Graphics@@6B@` has a SECOND, independent standing blocker that
Wave 22 lane GEOM did not see.** GEOM correctly discharged Wave 21's overrun tag
and wrote that the row "is now eligible to be re-scored and promoted by a future
lane — its stamper census was already green." But thirty lines above the very door
GEOM's evidence came from, `runtime/src/graphics/scene.cpp` carries a written
**VTABLE OWNERSHIP** note (Wave 9 lane GFXD) arguing that this port deliberately
does *not* stamp a table of its own. One of that note's three bullets is now STALE
— GEOM's fix voided it — and **two are still live**. §5.2.

Third result:

**Wave 21's "all eight rows are REQUIRED at cutover, not one of them is optional"
is not per-row derived, and for Geometry the failure mode it names is wrong.**
Wave 21 predicted a NULL vptr and an EXEC-of-0. Since Wave 22's fix, an unpromoted
Geometry row at cutover produces a **silent all-stub billboard vtable** instead —
the copy loop's `(baseVtbl && baseVtbl[i])` guard turns a null base table into
nine `SIGeom_StubRet0`s and renders nothing, with no crash and no log line. §6.

Fourth, answering lane GFXDATA through the integrator:

**Not one of my eight rows is STAMPER-blocked** — 45 stamp sites, 0 unattributed,
every row owns at least one stamper. Every "no" below is "not yet justified", never
"impossible". **But two of GFXDATA's three blocked DATA rows would still measure
attendance 0 after I promoted their tables**, because those tables are the abstract
ones. Its premise is necessary and not sufficient, and acting on it would produce a
zero that indicts a sound row. Only `??_7MaterialInstance@` is the real unblocker,
and it is 4 slot bodies away. §5.3a.

---

## 1. WHAT I DID AND DID NOT DO

**DID:** re-derived all 8 extents from the shipped DLL's raw bytes with my own
decoder plus 6 controls; re-derived readiness and the stamper census from the
tools; cross-checked every `ours/slots` figure against my independent purecall
count; re-checked both of Wave 22 lane GEOM's stated discharges against the
current source; identified the stamper and the resolve door for every row;
verified the purecall stub really terminates; ran both checkers.

**DID NOT — each is a hole, not an omission:**

| not checked | why it matters |
|---|---|
| **No game run. None.** `tools/game_dir_lock.sh status` reported `HELD by 'lane-ALLOC' for 1272s` with `reimpl-x64.exe processes=1, last game-dir write 1s ago`. I did not queue behind it. Kickoff §1b says this is an expected outcome; I am saying it plainly rather than calling anything verified. | Every promotion verdict below is derived from bytes and source. **No attendance number in this document was measured this wave.** |
| **I did not re-run the Ghidra xref corroboration.** Wave 21 confirmed `Geometry`'s three stampers against `get_xrefs_to`; I re-derived them only from the objdump writer channel. | Two channels agreeing is stronger than one. I have one channel, freshly run. |
| **I did not check whether our 6 ported Geometry virtual bodies behave correctly under DLL dispatch.** | That is the actual content of the §5.2 verdict. It is why I did not promote, not something the verdict assumes away. |
| **I did not port a single slot body.** 21 unported non-purecall slots remain across the set; that count is unchanged from Wave 21. | This lane is scoped to the promotion question. The bodies are lane GFXBODY's and a future lane's. |
| **I did not classify the other 11 unknown-wrapper identifiers** the census's own screen now prints (§3). | Wave 21 left the same hole. The screen it asked for was built and now names them on every run — that is progress; classifying them is not mine. |
| **`?AddDrawItems@MeshInstance@` / the three census-invisible rows** (`EdgeMesh`, `EdgeMeshInstance`, `MeshInstance`). | Not my rows. Noted in §7 because Wave 21 flagged them as the cheapest promotions in Graphics and nobody has taken them. |

---

## 2. RE-DERIVATION OF THE BOARD

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --namespace Graphics --unported
```

```
Graphics   total 599   called 100   ported 75   covered 75.0%   ceiling 95.0%
gap 25 |  BODY 8  CTOR 1  DTOR 1  VTABLE 8  DATA 7
```

**The kickoff re-derives exactly** — 25 rows, 8 of them VTABLE, and all eight are
the eight I was dealt. No staleness found in my inputs at the board level.

Note the ceiling is **95.0%, not 96.0%**: Wave 21's Geometry tag moved it and Wave
22's deletion of that tag did not move it back, because Graphics still carries four
other `HARMFUL` rows. Score the identity, not the percentage.

---

## 3. EXTENT — measured from raw bytes, and I say which bound bit

`tools/rt_mint_gen.py` records a proof string per row. I did not take it. I read
`C:\GOG Games\Banished\Runtime-x64.dll` directly with `pefile`, decoding qwords
forward from each exported table start and classifying each by section.

**The two bounds are not interchangeable and the kickoff's warning bites on
exactly two of my eight rows:**

* **byte-terminator** — the pointer run ends at a qword that is *not* a `.text`
  pointer (a zero, or ASCII string content). This bound is EXACT in both
  directions: a vtable slot is always a code pointer, so the row cannot be longer;
  and every qword before it is one, so it cannot be shorter.
* **adjacent-export only** — the pointer run does **not** stop. It continues
  straight into the next exported table. The only thing ending the row is the next
  export's address. That is an **UPPER bound** (`reference_adjacent_export_is_an_upper_bound`).

| row | DLL VA | extent | bound that bit | the terminating qword I read |
|---|---|---|---|---|
| `??_7MaterialInstance@Graphics@@6B@` | 0x1801545e0 | **9** | byte-terminator | `[9]` = `0x0000000000000000` |
| `??_7AnimationEvent@Graphics@@6B@` | 0x180154bf0 | **9** | byte-terminator | `[9]` = `0x6c6169726574614d` = `"Material"` |
| `??_7Geometry@Graphics@@6B@` | 0x180154e60 | **9** | byte-terminator | `[9]` = `0x6675426567616d49` = `"ImageBuf"` |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 0x180155040 | **10** | **adjacent-export ONLY** | none — run continues; `[10]` = 0x180155090 = `??_7ParticleSystem@Graphics@@6B@` |
| `??_7ParticleInstance@Graphics@@6B@` | 0x1801550e0 | **10** | **adjacent-export ONLY** | none — run continues; `[10]` = 0x180155130 = `??_7Particle@Graphics@@6B@` |
| `??_7Particle@Graphics@@6B@` | 0x180155130 | **9** | byte-terminator | `[9]` = `0x697463656c666552` = `"Reflecti"` |
| `??_7Light@Graphics@@6B@` | 0x180158d30 | **11** | byte-terminator | `[11..14]` = four zero qwords, then `0x1` |
| `??_7SunLight@Graphics@@6B@` | 0x180158e10 | **11** | byte-terminator | `[11]` = `0x0065636166727553` = `"Surface\0"` |

**All eight agree with `rt_data_mint_tables.inc` and with Wave 21.** No extent
correction is needed. But the *confidence* is not uniform and Wave 21's table did
not separate it this way: **six rows are exact in both directions from the bytes
alone; two (`ParticleInstance`, `ParticleSystemInstance`) rest entirely on the
adjacent-export upper bound** and would silently over-size if an unexported table
began inside the run. Nothing I can read refutes that; the writer channel
(`mint_extent_audit.py`, Wave 21) cleared both, and I did not re-run it.

### 3.1 Controls on my decoder (kickoff rule 9)

A screen that only ever says "yes" is not a screen. Mine was handed cases it
should reject:

```
POSITIVE  Geometry vtable start        0x180154e60   extent=9   expected=9   OK
POSITIVE  Light vtable start           0x180158d30   extent=11  expected=11  OK
NEGATIVE  "ImageBuf" string            0x180154ea8   extent=0   expected=0   OK
NEGATIVE  zero pad after MaterialInst  0x180154628   extent=0   expected=0   OK
NEGATIVE  Light tail zero run          0x180158d88   extent=0   expected=0   OK
SHIFTED   Geometry + 1 slot            0x180154e68   extent=8   expected=8   OK
```

**What the controls caught:** the decoder can say "not a table" (3/3 negatives)
and it notices a one-slot shift.

**What they did NOT catch, and it is a real limit:** the SHIFTED case returns
`8` — a perfectly plausible-looking vtable extent — with no complaint. **This
instrument measures extent FROM A GIVEN START; it cannot find or validate a
start.** Every start above came from the DLL export table, not from the bytes.
A row whose start we had wrong would be reported confidently and wrongly.

---

## 4. READINESS, AND A CROSS-CHECK THE BACKLOG DOES NOT PRINT

`ours / slots` below is the PROMOTION BACKLOG's figure, which counts
**non-purecall** slots only. `purecall` is **my own count**, from the bytes:
occurrences of `0x18007ec28` in each table.

**`extent − purecall` must equal the backlog's `slots`. It does, 8 times out of
8.** That is an independent arithmetic check on both channels at once — the
backlog's denominator and my byte read corroborate each other.

| row | extent | purecall | slots (= extent − purecall) | backlog says | ours | complete? | **abstract?** |
|---|---|---|---|---|---|---|---|
| `AnimationEvent` | 9 | 5 | 4 | 4 ✔ | 4 | **COMPLETE** | **YES** |
| `Geometry` | 9 | 3 | 6 | 6 ✔ | 6 | **COMPLETE** | **YES** |
| `Light` | 11 | 5 | 6 | 6 ✔ | 5 | no | **YES** |
| `MaterialInstance` | 9 | 0 | 9 | 9 ✔ | 5 | no | no (concrete) |
| `Particle` | 9 | 3 | 6 | 6 ✔ | 5 | no | **YES** |
| `ParticleInstance` | 10 | 1 | 9 | 9 ✔ | 5 | no | **YES** |
| `ParticleSystemInstance` | 10 | 0 | 10 | 10 ✔ | 5 | no | no (concrete) |
| `SunLight` | 11 | 0 | 11 | 11 ✔ | 5 | no | no (concrete) |

Unported non-purecall slots across the set: **21**, unchanged from Wave 21's close.

### 4.1 The purecall stub really is one — I did not take the mint's word

`0x18007ec28`, read from the shipped DLL:

```
48 83 ec 28        sub    rsp,0x28
48 8b 0d b5 cd 17  mov    rcx,[rip+0x17cdb5]     ; the purecall handler
ff 15 ff c7 0c 00  call   [rip+0xcc7ff]
48 85 c0 / 74 02   test   rax,rax ; jz +2
ff d0              call   rax
b9 19 00 00 00     mov    ecx,0x19               ; _RT_PUREVIRTUAL
e8 ...             call   <abort path>
```

That is MSVC's `_purecall`: it **terminates**. The mint fills each such slot with
a named abort stub in our image, which is behaviourally faithful and needs no DLL
at cutover. **Purecall slots are not a promotion blocker on any route**, and the
classification the whole §4 table rests on is measured, not inherited.

### 4.2 Why "abstract" is the load-bearing column

A `__purecall` slot exists only for a pure virtual, and a class with a pure
virtual **cannot be instantiated**. So for the five rows marked YES, no live
object ever carries that table as its final vptr: MSVC stamps the base table in
the base ctor and the derived ctor overwrites it in the next statement (and
symmetrically on the way out). Confirmed in our own tree, not just asserted:

* `code/Game/Component/animaudioevent.cpp:58-62` — `baseCtor(mem)` (our ported
  `??0AnimationEvent@`), then `*(void**)mem = ResolveAddr(Module::Exe, 0x14015e0c0)`
  on the **very next line**. The dtor does the same in reverse.
* `runtime/src/graphics/scene.cpp:621` / `:714` — our ported `??0Geometry@` /
  `??1Geometry@` store the base vptr and **dispatch no virtual on `self`**. I read
  both bodies. So even the narrow base-ctor/base-dtor window is inert here.

The three `no` rows are the opposite: `MaterialInstance`, `ParticleSystemInstance`
and `SunLight` have zero pure virtuals, our tree constructs them
(`renderer.cpp:561`, `saveload_draw.cpp:373`, `animation.cpp:168`,
`sunlight.cpp:179`), and their table **is** a live object's final vptr.

A neat structural corroboration fell out: at each base/derived pair the derived
table fills exactly the base's pure slot. `ParticleInstance[1]` is purecall;
`ParticleSystemInstance[1]` is `0x1800a5600`. `Light[1]` is purecall;
`SunLight[1]` is `0x1801067a0`. Two independent readings of the same hierarchy
agreeing.

---

## 5. PROMOTABILITY — the stamper, and the route

### 5.1 Stamper census, re-run

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_stamper_census.py --rows <my 8>
  POSITIVE CONTROL ??0BinaryFileStream@IO@@QEAA@XZ   REGISTERED
  NEGATIVE CONTROL (synthetic symbol)                not-registered OK
  LIMIT: `ours-capable` is an UPPER BOUND -- a registration is a DOOR, not a
         guarantee every caller uses it.
```

| row | resolve door in our tree | stamp sites | ours-capable | retail | unattributed |
|---|---|---|---|---|---|
| `AnimationEvent` | **ADDR** `animation.cpp:156` | 5 | 3 | 2 | 0 |
| `Geometry` | **ADDR** `scene.cpp:176` | 3 | 2 | 1 | 0 |
| `Light` | **NAME** `sunlight.cpp:175` | 9 | 1 | 8 | 0 |
| `MaterialInstance` | **NAME** `renderer.cpp:561`, `saveload_draw.cpp:373` | 10 | 3 | 7 | 0 |
| `Particle` | **ADDR** `animation.cpp:160` | 4 | 1 | 3 | 0 |
| `ParticleInstance` | **ADDR** `animation.cpp:164` | 8 | 3 | 5 | 0 |
| `ParticleSystemInstance` | **ADDR** `animation.cpp:168` | 3 | 1 | 2 | 0 |
| `SunLight` | **NAME** `sunlight.cpp:179` | 3 | 2 | 1 | 0 |

**45 sites, 16 ours-capable, 29 retail, 0 unattributed — character for character
what Wave 21 measured.** The door split (5 ADDR / 3 NAME) also re-derives exactly;
I re-grepped it rather than copying the table, and it matters: a name-only
promotion of the five ADDR rows would be invisible to their own call sites, so any
promotion here must register **both** name and address.

**Nothing in this set is unstampable.** Every row has at least one stamper we own
and no site is unattributed. So kickoff rule 4's permanent-unpromotability shape
(`??_7DialogController@UI@@`) does **not** occur here. That is a green light on
the *stamper* question and, as §0 says, it is not the question that decides these
rows.

### 5.2 `??_7Geometry@Graphics@@6B@` — the discharge is REAL, and there is a SECOND blocker

**Wave 22 lane GEOM's fix landed and I verified it, not re-read it:**

* `code/Game/Component/statusicon_geometry.cpp:392` reads `if (i < 9)`. ✔
* `standalone/rt_data_mint_tables.inc:1568` declares the row **9 slots**, and its
  storage `kVTable28` is `static void* [9]`. ✔ Exact fit — the 24-byte overrun
  Wave 21 tagged `HARMFUL` is structurally impossible now.
* The `RT_UNPROMOTABLE` line is gone from `standalone/reg_mint_promoted.cpp` and
  the census no longer prints a tag against this row. ✔

**Wave 21's tag is correctly dead. I did not re-raise it.**

**But `runtime/src/graphics/scene.cpp:76-99` carries a separate written
verdict that GEOM did not cite, from Wave 9 lane GFXD, under the heading
`VTABLE OWNERSHIP — READ THIS BEFORE ASSUMING A VIRTUAL IS OURS`.** Its three
bullets, re-scored today:

1. *"Our five ported virtual bodies are reached ONLY through the direct
   `fn_Geometry_*` pointers"* — **STILL TRUE, and it is the cost.** Those six
   bodies (`Clean`, `UpdateAttached`, `GetSubTransform`, `GetInstanceTransform`,
   `SetSharedInstanceData`, plus the deleting dtor) have **never been reached by a
   virtual dispatch**. Promoting the row is precisely the act of handing them to
   the DLL's own dispatchers — `Scene`, `Renderer`, the cull walk — for every base
   Geometry our ctor builds. That is the promotion's *value* and its *risk*, and
   they are the same event.
2. *"Every VIRTUAL dispatch still lands in the DLL ... that is exactly retail's
   behaviour and is why porting the ctor without a vtable of our own is the safe
   choice here (PLAN §4.3 risk 3)."* — **STILL TRUE.** This is a standing decision
   of record against promoting this row while the DLL is present.
3. *"Keeping the DLL vtable is also what keeps `statusicon_geometry.cpp:335`
   working: it builds its billboard vtable by COPYING slots out of whatever the
   Geometry ctor stamped. Stamping our own table would silently change every slot
   it copies."* — **STALE.** GEOM's fix makes source and destination both exactly
   nine. The copy is now width-correct against either table. **This bullet should
   be struck when someone edits that comment.**

One further cost nobody has priced: our ported Geometry ctor and dtor both carry
`RT_DIFF` oracles that compare `self->_vtable` against `RtGeometryVtable()`
(`scene.cpp:624`, `:717`, `:728`). Those are self-checks, not oracles
(`reference_shadow_marker_is_not_an_oracle`) and would stay green by construction.
The byte-diff sites are the real cost: a promoted vtable makes **+0x00 diverge
permanently** on every Geometry comparison — `reg_mint_promoted.cpp` already
documents that as expected for any promoted vtable, but here it lands on Wave 9
lane GFXD's oracle surface, one of the most-exercised in the tree, and it would
mask a genuine vptr defect there from then on.

**VERDICT, route-scoped:**

* **DLL-present — DO NOT PROMOTE.** Not harmful the way Wave 21's tag meant
  (that mechanism is gone); **unjustifiable without a run**. It converts six
  never-virtually-dispatched bodies into DLL-dispatched ones and permanently
  dirties an oracle, in exchange for nothing retail's own table is not already
  doing. I could not get a run (§1), so I did not take the bet.
* **At cutover — REQUIRED**, and see §6 for what actually breaks.

**I did not add an `RT_UNPROMOTABLE` tag.** Wave 21's expiry condition fired and
GEOM paid it off; re-tagging the row for a different reason would re-open a debt
the project just closed, and "not yet justified" is not the same claim as
"unpromotable". The right instrument is this document plus a named addressee (§7).

### 5.3 `??_7AnimationEvent@Graphics@@6B@` — COMPLETE, stampable, and a NO-OP

4/4, 5 purecall slots that abort faithfully, 3 ours-capable stampers, an ADDR door
we own, no unported slot, no trap. Every readiness gate is green and the stamper
question is green. **And promoting it would change nothing observable.**

`AnimAudioEvent_CtorImpl` calls our ported base ctor and overwrites the vptr with
the exe's own 11-slot table on the next line; `AnimAudioEvent_Dtor` re-stamps the
exe table *before* calling the base dtor. Our ported base ctor and dtor dispatch no
virtual on `self`. The class is abstract, so no other object can carry this table
as its final vptr. **The window in which the minted table would be live is one
statement wide and contains no dispatch.**

That is the `reference_uncalled_port_is_not_verified` shape, and
`reg_mint_promoted.cpp`'s own standing rule for the 94 unpromoted `?_typeInfo@`
rows says the same thing: *promoting them would be a port with no caller.*

Promotion is also not free here, for the same reason as Geometry: `animation.cpp:249`
runs retail's ctor into a scratch and byte-compares. A promoted table makes `+0x00`
diverge permanently in **this row's own oracle** — a dirtied instrument bought with
zero dispatch.

* **DLL-present — SAFE but a NO-OP.** I did not promote it. A future lane that
  wants the row for tidiness may; it should expect attendance 0 and say so.
* **At cutover — SAFE, and NOT provably required.** §6.

### 5.3a STAMPER-BLOCKED vs MERELY-UNPROVEN — the distinction the integrator asked for

Lane GFXDATA has three READY DATA rows (`?_typeInfo@AnimationEvent@`,
`?_typeInfo@MaterialInstance@`, `?_typeInfo@Particle@Graphics@@`) that it could not
promote for want of attendance, and it recorded that their only ported consumers —
Wave 21's `Type`/`TypeName` accessor bodies — are *"reachable only once
`??_7AnimationEvent@`, `??_7MaterialInstance@` and `??_7Particle@Graphics@@6B@` are
promoted."* The answer, per table:

| table | stamper-BLOCKED? | actually blocked by | would promoting it produce attendance on GFXDATA's row? |
|---|---|---|---|
| `??_7AnimationEvent@` | **NO** — 3 of 5 stampers ours, 0 unattributed | nothing; it is COMPLETE and promotable today | **NO.** Abstract base; table overwritten one statement later; no dispatch in the window. |
| `??_7Particle@` | **NO** — 1 of 4 stampers ours, 0 unattributed | 1 unported slot (5/6) | **NO**, even after that slot lands. Abstract base, same mechanism. |
| `??_7MaterialInstance@` | **NO** — 3 of 10 stampers ours, 0 unattributed | 4 unported slots (5/9) | **YES** — concrete, live final vptr, our tree constructs it. This is the one. |

**None of the three is stamper-blocked. Not one row in my whole set is.** So the
honest answer to the integrator's question is "merely unproven" in all three cases —
but that is *not* the same as "promote them and the attendance appears," and GFXDATA's
stated premise is **necessary but not sufficient** for two of its three rows.

**This matters more than a refusal would.** Promote `??_7AnimationEvent@` on GFXDATA's
reasoning and the follow-up boot measures attendance **0** on
`?_typeInfo@AnimationEvent@` — which reads exactly like "the DATA row is unreached,
leave it unpromoted," when the true cause is that the accessor is in a table nothing
dispatches. That is `reference_a_green_readiness_screen_is_not_a_route` and
`reference_calls_zero_can_mean_always_took_the_uncovered_branch` arriving together,
and it would indict a sound DATA row.

**`??_7MaterialInstance@` is the row that would genuinely unblock GFXDATA**, and its
price is 4 slot bodies (`5/9` today), not a promotion decision. It is also the dearest
stamper split among the concrete rows (7 retail). **→ named in §7 residual 5.**

I also record GFXDATA's writer/reader finding as consistent with mine from the other
side: it found that owning a *writer* did not flip a HARMFUL verdict because unported
*readers* govern. My rows are the dual case — owning every *stamper* does not make a
promotion worth anything if nothing *dispatches* the table. Both are the same error:
scoring a row on the half of the route you happen to own.

### 5.4 The other six

None is COMPLETE, so none is promotable on any route today: each would put a named
abort trap in a live table. Route-scoped, and ordered by what actually matters
rather than by slot count:

| row | DLL-present | at cutover | promotion price (retail stamps) |
|---|---|---|---|
| `MaterialInstance` (concrete) | blocked, 4 slots short | **required** — live final vptr | **7** — dearest in the set, incl. the whole `Create*Object` factory group |
| `ParticleSystemInstance` (concrete) | blocked, 5 slots short | **required** — live final vptr | 2 |
| `SunLight` (concrete) | blocked, 6 slots short | **required** — live final vptr | 1 (the copy ctor) — **cheapest concrete row in the set** |
| `Light` (abstract) | blocked, 1 slot short | required only via its concrete subclasses | **8** — dearest stamper split anywhere here |
| `Particle` (abstract) | blocked, 1 slot short | see §6 | 3 |
| `ParticleInstance` (abstract) | blocked, 4 slots short | see §6 | 5 |

**`SunLight` is the row a body-porting lane should aim at**: concrete (so promoting
it will actually be worth something), only 1 retail stamper, and 6 slots to source.
`Light` is the trap — 1 slot from COMPLETE and therefore the tempting target, but
abstract *and* carrying 8 retail stamps, so the finished promotion buys the least
of any row here.

---

## 6. CUTOVER — Wave 21's blanket claim, re-derived per row

Wave 21 §5.0 wrote: *"All eight rows are REQUIRED at cutover. Not one of them is
optional,"* from one general argument — unpromoted row → `ResolveAddr` returns 0
for an unmapped module → NULL vptr → *"the first virtual call is an EXEC-of-0."*

That argument never asks **who dispatches through that vptr** (kickoff rule 6, in
its vtable form). Asked per row, it does not hold uniformly:

* **The three CONCRETE rows** (`MaterialInstance`, `ParticleSystemInstance`,
  `SunLight`) — **REQUIRED, and Wave 21's mechanism is exactly right.** These
  tables are live final vptrs. A null there is an EXEC-of-0 on the first virtual.
* **`AnimationEvent`, `Particle`, `ParticleInstance`, `Light` (abstract)** —
  **NOT proven required.** The null is written by the base ctor and overwritten by
  the derived ctor before anything can dispatch it, and our base ctor/dtor bodies
  contain no self-dispatch. Promote them anyway for cheap insurance if you like —
  but the EXEC-of-0 claim is not the reason, and a cutover lane that budgets these
  four as blockers is budgeting work it may not need.
* **`Geometry` (abstract) — REQUIRED, but NOT for Wave 21's reason, and the
  failure is SILENT.** Nothing dispatches the base table; but
  `statusicon_geometry.cpp` **copies** it. With the row unpromoted at cutover,
  `baseVtbl` is null, the guard `(baseVtbl && baseVtbl[i])` is false for every
  index, and all nine slots become `SIGeom_StubRet0`. The billboard geometry is
  then fully constructed, fully dispatchable, and draws nothing. **No crash, no
  EXEC-of-0, no log line** — the loudest failure Wave 21 predicted has become the
  quietest one there is, and it became that way *because* Wave 22's fix replaced
  a garbage-pointer install with a well-behaved stub. A correct fix moved the
  failure from loud to silent; that is worth knowing before the cutover boot is
  read as a planner (`reference_the_boot_is_a_gate_not_a_planner`).

---

## 7. WHAT I PROMOTED: NOTHING — and the residuals, each with a NAME

**Rows promoted: 0. Tags added: 0. Source files changed: 0.** This document is the
only file this lane adds.

Wave 22 lanes TIMER and ERRCFG reached zero promotions on this same reasoning and
the kickoff records that as a correct outcome (rule 6). Mine is narrower: one row
is a measured no-op, one is a bet I could not price without a run, and six are
blocked on slot bodies that are not this lane's scope.

**Residuals, with named addressees (kickoff rule 11 — no conditions, no "the next
lane that…"):**

1. **`??_7Geometry@Graphics@@6B@` promotion decision → the WAVE 24 KICKOFF WRITER,
   to deal to a lane that can hold `tools/game_dir_lock.sh`.** Everything static is
   done and green: extent 9 exact, 6/6 COMPLETE, 2 of 3 stampers ours, 0
   unattributed, Wave 21's overrun blocker genuinely discharged. What is missing is
   one A/B run — promoted vs `BANISHED_RT_MINT_PROMOTED=0` — with `BANISHED_RT_DIFF=1`,
   checking that the six Geometry virtuals dispatched from the DLL agree, and
   accepting the permanent `+0x00` divergence as the promotion's signature rather
   than a regression. **Do not promote it on the strength of §5.1's green stamper
   census; that is the exact mistake §0 records.**

2. **`runtime/src/graphics/scene.cpp:94-99`, bullet 3 of the VTABLE OWNERSHIP note,
   is STALE → lane GFXBODY of Wave 23**, which owns `runtime/src/graphics/`. It
   claims stamping our own table "would silently change every slot
   `statusicon_geometry.cpp` copies"; since Wave 22 both sides are exactly nine and
   the copy is width-correct. Strike that bullet; **keep bullets 1 and 2**, which
   are the live argument. I did not edit the file myself — it is GFXBODY's this
   wave and a comment edit from a second lane is a merge conflict for no gain.

3. **Wave 21's "all eight REQUIRED at cutover" needs replacing with §6's per-row
   split → the integrator of Wave 23**, for `WAVE23_CLOSED.md`. Three rows
   required with Wave 21's stated mechanism, one required with a different and
   *silent* mechanism, four not proven required at all. A cutover plan built on the
   blanket claim over-budgets four rows and mis-describes the fifth's symptom.

4. **GFXDATA's three READY `?_typeInfo@` rows are NOT stamper-blocked, and two of
   them will NOT gain attendance from a promotion of my tables → the WAVE 23
   INTEGRATOR**, to correct GFXDATA's stated premise before it is dealt onward.
   `AnimationEvent` and `Particle` are abstract bases; their `Type`/`TypeName`
   accessors sit in tables nothing dispatches, so a promotion followed by an
   attendance boot would report **0** and would be misread as evidence against a
   sound DATA row. Full table in §5.3a.

5. **`?_typeInfo@MaterialInstance@Graphics@@` is unblockable today and its price is
   4 slot bodies → lane GFXBODY of Wave 23**, which owns the Graphics function rows.
   `??_7MaterialInstance@` is the only one of my eight that is concrete, live, and
   consumed by GFXDATA's rows; at `5/9` it needs four more non-purecall slots before
   it can be promoted, after which an attendance boot would give GFXDATA a real
   measurement. If GFXBODY does not take it, **the WAVE 24 KICKOFF WRITER**.

6. **`??_7EdgeMeshInstance@` and `??_7MeshInstance@` are still the cheapest
   promotions in Graphics** — 2 stampers each, both ours, zero retail — and both are
   still invisible to the census's `called` column (they reach the DLL only through
   `RtVtable`, which the census's new unknown-wrapper screen now names on every run
   but still does not count). Flagged twice now, unclaimed twice. **→ the WAVE 24
   KICKOFF WRITER**; they are not in any Wave 23 lane's row list.

---

## 8. CHECKERS — before and after are IDENTICAL, because I changed no code

```
tools/unpromotable_slot_check.py
  CHECK 1: 20 of 20 scoreable tags fit, 0 STALE
  CHECK 2: 20 numeric tags -- 1 declared, 19 grandfathered, 0 UNDECLARED, 0 malformed
  CHECK 3: 41 tags with an expiry -- 1 scoped, 1 addressed, 40 grandfathered,
           0 MISSING, 0 malformed
  CONTROLS: 1pos/1neg/2pos/2neg/3pos/3neg all GREEN
  FREEZE  : 20 grandfathered pairs, 19 matched by a live tag
            UNMATCHED: ??_7ReaderWriter@IO@@6B@   <- PRE-EXISTING, IO's, not mine;
                       present identically before and after. Not "fixed" (kickoff).

tools/dispatch_slot_map.py --check
  checked 110 ResolveExport registrations, 0 name mismatch(es);
  0 address(es) registered more than once
```

Both green. **The tag count is 20, down from Wave 21's 21 — that is Wave 22 lane
GEOM's deletion, already merged, not this lane.**

---

## 9. DENOMINATOR

* **8 rows** — the eight `??_7…@Graphics@@6B@` VTABLE rows in the Wave 23 board's
  Graphics gap of 25. Not the 11 rows Wave 21 reported on (it added `EdgeMesh`,
  `EdgeMeshInstance`, `MeshInstance` as context; they are not in my brief and I
  re-measured none of them).
* **Extent**: 8 of 8 measured myself from `Runtime-x64.dll` bytes; 6 exact in both
  directions, 2 upper-bound-only.
* **Readiness**: 8 of 8 from the census backlog, each cross-checked against my own
  purecall count. 8 of 8 agree.
* **Stampers**: 45 sites over the 8 rows, from `mint_stamper_census.py`, one
  channel, controls green, `ours-capable` explicitly an upper bound.
* **Abstract/concrete**: derived from the purecall count in the bytes, corroborated
  for two of the five abstract rows by reading our own ctor/dtor bodies and one exe
  subclass ctor. **The other three abstract rows (`Light`, `Particle`,
  `ParticleInstance`) were NOT corroborated that way** — for them "abstract" rests
  on the purecall count alone, which is sound but is one channel.
* **Runtime attendance**: **zero measurements. No run was obtained.**
