# WAVE 18 — lane VDATA: the seven Video DATA rows

Worktree `C:/code/GIT/wt-w18-vdata`, branch `wave18/vdata`, cut from `d12e8be`.

Subject: `??_7ImageBuffer@Video@@6B@` (7 abort-trap slots to close) and the six
`?_typeInfo@…@Video@@` rows CEIL classed *blocked on attendance*, plus the four
Graphics `?_typeInfo@` rows that ride the same consumer.

---

## 0. PRE-REGISTERED PREDICTIONS

**Written and committed BEFORE any code was written and before any run.**
Scored in §6.

### 0.1 The route for the ten `OnVideoError` typeinfos

CEIL §3.4 and the Wave 18 kickoff both say the ten rows are blocked because
"no harness route drives" `Engine::MainLoop::OnVideoError`, and CEIL §5.4
prescribes **synthesising** a call.

**P1 — I predict no synthesis is needed: a real driver already exists.**
`BANISHED_FORCE_DEVICE_LOST=<frame>` (`dx11/src/contextdata.cpp:594-628`,
Wave 9 lane ENGIO) injects one synthetic `DXGI_ERROR_DEVICE_RESET` into a
`Present` result and lets the real teardown run. `WAVE9_LANE_VIDL.md` §4.2
records it firing and reaching the ported body: `Engine::MainLoop::OnVideoError
calls=1`. That is the exact consumer of the ten-element `ids[]`.

Confidence: high. This is `reference_grep_the_harness_before_declaring_no_route.md`
and `reference_holdback_blockers_are_stale_check_them_first.md` for a third time.
The risk is that the driver has rotted since Wave 9 on this tip.

**P2 — attendance under that route.** All ten rows read `post >= 1`, and
`post == 1` exactly, because each `TypeIdX()` in `mainloop_video.cpp` caches in
its own function-local static and `OnVideoError` runs once per injected event.

**P3 — the split between `init` and `post`.** Four rows (the Graphics ones
`reg_unreg_lane3.cpp` resolves eagerly) also carry `init=1`; the six Video rows
read `init=0`. CEIL's phase-boundary correction is what makes this checkable.

**P4 — the control.** `?_singleton@MainLoop@Engine@@1PEAV12@EA` reads `post>0`
(GREEN), as it did on all four of CEIL's routes.

### 0.2 The seven `??_7ImageBuffer@Video@@6B@` trap slots

**P5 — all seven are cheap.** Six of the seven are 1–5 instructions
(`TypeName`, `Type`, `GetFileVersion`, `Reconstruct`, `Destruct`, `Construct`);
only `Serialize` (0x1800f26f0) has a body worth the name, and it is the same
shape as `Achievement::Goal::Serialize`, already ported in
`runtime/src/achievement/goal_serialize.cpp`.

**P6 — closing them makes the row READY with `TRAP=0`,** and
`BANISHED_RT_MINT_SHADOW=1` reports `10 slots / 10 ported / 0 TRAP / 0 purecall`.

**P7 — the row is then PROMOTABLE, and promoting it is behaviour-neutral,**
because every slot holds a faithful port of the body the DLL's own table points
at, and because our ported `ImageBuffer` ctor/dtor already stamp the row through
a `ResolveExport` door (`buffers.cpp:108`), so the stamp follows the promotion.

**P8 — attendance for `??_7ImageBuffer@Video@@6B@` is `post>0` on the ordinary
gameplay route**, because `buffers.cpp:108`'s `ImageBufferVtable()` is on the
ctor path and images are constructed during a normal load.

### 0.3 The census

**P9 — the seven slot bodies do NOT move Video's `called` column.**
`tools/runtime_census.py`'s `mint_symbol_count` deliberately excludes vtable
SLOT TARGET names in the `.inc` from the minted-symbol count, and the current
`--unported` listing for Video contains the vtable row but none of its seven
slot targets. So they are not in the denominator today.

**P10 — Video's numerator moves by 7: +6 typeinfo rows +1 vtable row,**
109 → 116 of 150 = **77.3%**, if P1/P2 and P7 both hold. That is +4.6 points
of the wave's +15.3.

**P11 — the ceiling does NOT move.** 88.0% (132/150) already counts all seven
of my rows as promotable; my lane converts BLOCKED into promoted, which is
numerator work. A ceiling move would require finding a row CEIL called
promotable that is not, or vice versa.

### 0.4 One prediction I expect to be wrong

**P12 — I expect at least one of these twelve to fail,** and the most likely is
P7: a `??_7` row whose only consumers are in-DLL `LEA` stamps is exactly the
shape `reference_a_ported_row_can_be_permanently_unpromotable.md` describes, and
CEIL measured 0 READ / 0 WRITE / 5–118 DATA for all 26 vtable rows including
this one. If our own ctor's `ResolveExport` stamp turns out to be the ONLY door,
the row is promotable but the promotion may be a NO-OP for every object the DLL
constructs itself.

---

*(Sections 1-7 follow after the work. This file was committed with §0 alone.)*
