# Integrator queue — code-fn wave

Edits requested by lanes that touch SHARED files. Applied in one batch, with one
build, after all six lanes report. Nothing here is applied yet.

## Lane 3 — delete 8 vestigial `fn_*` pointers (VERIFIED by integrator)

Independently confirmed before queueing: each identifier has exactly 3 source
mentions (defn / resolve / decl) and **no dereference anywhere**, and each of the
7 dead addresses has **exactly one** `ResolveAddr` site — the dead pointer
itself. So deleting them removes 7 census rows and changes zero instructions.

| identifier | addr | defn | resolve | decl |
|---|---|---|---|---|
| `fn_SelectResourceUI_GetResourceCount` | 0x140078a40 | cpp:409 | cpp:2623-2624 | h:1232-1234 |
| `fn_TownHallUI_InitProfessions` | 0x140080160 | cpp:716 | cpp:3554-3555 | h:2551 |
| `fn_TownHallUI_InitProduction` | 0x14007fc30 | cpp:717 | cpp:3556-3557 | h:2552 |
| `fn_TownHallUI_InitInventory` | 0x14007f660 | cpp:718 | cpp:3558-3559 | h:2553 |
| `fn_TownHallUI_InitAgriculture` | 0x14007ebf0 | cpp:719 | cpp:3561-3562 | h:2554 |
| `fn_TradeUI_RefreshDisplay` | 0x1400846b0 | cpp:765 | cpp:3651-3652 | h:2679-2682 |
| `fn_CitizenList_ShuffleEntities` | 0x140089470 | cpp:745 | cpp:3606-3607 | h:2613-2616 |
| `fn_CitizenBehavior_ShuffleSchools` | 0x1400896a0 | cpp:1122 | cpp:4847-4848 | h:5295-5297 |

All in `engine/funcs/engine_funcs.{cpp,h}`. **Delete bottom-up per file or match
on the identifier** — the line numbers are declaration-order sensitive.

### Do NOT touch
- `s_optimize89470` (`code/Game/Game/town_workplace_registry.cpp:56/64/238`) —
  the LIVE, deliberately-gated consumer of 0x140089470. Verified independent of
  the dead `fn_CitizenList_ShuffleEntities`; deleting that pointer cannot disable
  the gated feature.
- `engine_funcs.h:2535-2538` — the four `void TownHallUI_Init*(TownHallUI*)`
  prototypes for the REAL ports in `townhallui.cpp`. These stay.

### Note on overriding a stated intent
`engine_funcs.h:2679-2680` says the TradeUI pointer is "kept for reference /
fallback only". There is no fallback code — the comment is stale in exactly the
way the two corrected earlier in this branch were. Deleting deliberately, not
silently.

### Expected census effect
7 rows disappear. **0x140089470 will REMAIN** in the census whenever
`BANISHED_LIVE_SEAT_SHUFFLE` is off, because `s_optimize89470` still resolves it.
That is correct — it is a genuine deliberate-leave and should stay visible.
