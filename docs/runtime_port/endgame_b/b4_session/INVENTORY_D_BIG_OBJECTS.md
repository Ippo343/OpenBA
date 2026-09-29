# Inventory D — the big objects (B.4 block 0, NEXT_SESSION §3b.3)

Owner D `big_objects`. Read-only; nothing in product code was changed. Table:
`data/D_big_objects.tsv` (262 rows), retail side hand-kept in `data/D_retail_layouts.tsv`
(146 rows), script `scripts/D_big_objects.py`.

## 1. The answer

| object | retail size | fields (retail) | tree lines reading it raw | files | modules | header today |
|---|---:|---:|---:|---:|---:|---|
| `GameUpdateLoop` | 0x160 | 42 + base | **808** (+29 already by name) | 236 | 10 | COMPLETE: every byte declared, 39 offsets asserted |
| … one level deeper (pointee fields) | — | — | 435 | — | — | the pointees are `void*` |
| `CitizenComponent` | 0x290 | 50 + base | **354** (its own bodies only) | 9 | 1 (+≈7 unmeasured) | all bytes declared, WRONG types for 17 fields, 19 asserts |
| settings block `g_gameSettings` | 0x34 | 16 | 63 (+2 via a byte helper) | 17 | 4 | none (`int32_t*`) |
| Game container `g_gameContainer` | 0xE0 | 26 | 34 | 14 | 5 | none (`void*`) |

- **GameUpdateLoop is not a header problem.** All 808 raw reads land on fields the header already
  declares and names (0 reads past 0x160 after alias clean-up; 3 read fields lack an assert:
  +0x48, +0xA8, +0xB0). The casts survive because the 33 pointer members are `void*`: turning
  `*(void**)(gw + 0xD8)` into `gw->m_random` still needs `reinterpret_cast<System::Random*>`. The work
  is **typing the members**, which needs the POINTEE classes (CitizenBehavior 139 deeper reads,
  Map/"Game" 78, EntityList 56, ProfessionList 48, PlayerToolManager 36, PlayerController 23 …).
- **CitizenComponent**: full retail layout recovered (ctor, dtor, serializer, Update, RemoveChild,
  the happiness update, AssignHousing). 5 bytes of the header are misdeclared (see §5).
- **Settings block**: every field named from retail's own `IO::Persistence` keys ("Game", "UIScale",
  … "LastDisaster") in `0x140098b60`. **Game container**: every field named from the resource path
  its init `0x140099db0` loads. Both are fully recovered at high confidence.
- **No width disagreement changes behaviour**: 20 reads use another width than retail's type, all
  bit copies of a float through `uint32_t`, zeroing two adjacent floats with one `int64_t` store
  (retail does the same at +0x44), or an `int32` read truncated to `uint8_t`.

## 2. How the table was built

`python docs/runtime_port/endgame_b/b4_session/scripts/D_big_objects.py [--reads=PATH]` from the
repo root; deterministic (two runs, same md5); 3 s. It walks PROD (`code runtime standalone dx11
engine`), strips comments with `b3r_session/codegrep.py`'s `strip()` and blanks string literals.

Receivers (per file, brace-scoped):
- roots `GameUpdateLoop::_singleton`, `g_gameSettings`, `g_gameContainer`;
- file-local helpers whose body is `return <cast>(root);` (`GW()`, `GameWorld()`, `World()` …);
- aliases `T x = <casts>(receiver);` or `x = c ? receiver : 0;` (a call such as `TownMgr(gw)` is
  NOT an alias — that rule removed ≈115 false lines);
- CitizenComponent: the first parameter (named `self`/`selfV`/`param_1`/`citizen`) of a definition
  of `CitizenComponent_*`, `CitizenComp_*` in `code/Game/Component/citizen_*|citizencomponent*`,
  `this` of `CitizenComponent::` members, the `At(this, off)` helper; three bodies whose receiver
  is another class are excluded by hand (`NOT_CITIZEN_RECEIVER`, §5).
- field aliases of GameUpdateLoop (`int* mapGrid = *(int**)(gw + 0xF8);`) and `*(T*)(*(X**)(gw +
  0xF0) + 0x170)` feed the `GameUpdateLoop.<field>->` rows (pointee reads).

A read is `receiver ) + N`, `receiver[N]` or `&receiver[N]`; the offset is scaled by the receiver's
declared type (`int32_t*` ×4, `int64_t*` ×8, byte pointers ×1, a byte / integer receiver cast ×1).
The width is the pointer type of the cast that dereferences the sum (`addr:T` = the member's
address is taken as `T*`, `addr` = an untyped address). Each read is mapped to the retail field that
CONTAINS it (`i32@+0xA4` = a read inside the 0x10-byte array at +0x98).

Columns: `object offset size type proposed_name evidence readers_in_tree confidence` (the asked
eight) then `tree_lines tree_widths header_decl header_asserted width_check modules`.
`width_check`: `ok`, `DIFFERS: <widths>`, `READS PADDING`, `CHECK` (past sizeof).

Hand check: sample 1 (45 random reads) found 3 misattributed rows — `CitizenComp_FUN_14012c3f0`
(receiver = StatusIconComponent, typed `int64_t*`: `param_1 + 0x1e` is byte 0xF0),
`CitizenComp_AnyCitizenSick` (receiver = CitizenBehavior), and a `char r = HandleLifeEvent(self…)`
alias; all three fixed in the script. Sample 2 (45 new random reads, seed 11): **0 errors** in
object, offset or width. A third pass read every `DIFFERS` / `PADDING` row (20 lines): 1 more
misattribution fixed (`CitizenComp_helper_a4f0` = retail `ResidenceComponent_IsHouseholdFull`
0x14012a4f0), the rest confirmed.

## 3. Counts

GameUpdateLoop, lines per field (top): m_game +0xF8 189 · m_citizenList +0xF0 151 · m_random
+0xD8 57 · m_professionList +0xB0 45 · m_entityList +0xE8 40 · m_entityUI +0x120 39 · m_tutorial
+0x118 31 · m_graphicsScene +0x60 29 · m_dayCounter +0x128 20 · m_worldGeometry +0x110 20 ·
m_userInterface +0x98 19 · m_weather +0x130 17 · m_gameFlags +0xD0 14 · m_playerController 12 ·
m_eventNotifier 11 · m_audioScene 10 (exact per-row numbers in the TSV). Via: root 271, alias 477,
helper 59. By module: Component 293 · Dialog 177 · Player 99 · Work 74 · Action 61 · standalone 43 ·
Tutorial 37 · Entity 13 · Goal 8 · Game 2 (the loop's own bodies use `m_` names).

Pointee reads (435 lines): m_citizenList-> 139 (offsets +0x170 24, +0xA8 13, +0xA0 13, +0x100 10 …
up to +0x1B0, i.e. CitizenBehavior 0x1C0) · m_game-> 78 (+0x80 54: the tile array) · m_entityList->
56 · m_professionList-> 48 (+0x30 21, +0x20 10, +0x28 8) · m_entityUI-> 36 · m_playerController-> 23 ·
m_weather-> 15 · m_worldGeometry-> 14 · m_dayCounter-> 10.

CitizenComponent, lines per field (top): description +0x288 22 · spouse +0x250 19 · age +0x3C 14 ·
hunger +0x44 14 · character +0x278 14 · consumedTypes array +0x98 14 · disease +0x68 13 · base
+0x10/+0x18 13 · dirtyFlags +0x88 13 · workplace +0x238 12 · home +0x248 12 · school +0x208 12 ·
_parent +0x258 12 · _child +0x260 12 · profession +0x1F0 12 · timer_70 12. Files:
citizencomponent.cpp 193, citizencomponent_serialize.cpp 48, citizen_updatehealth.cpp 40,
citizencomponent_lifecycle.cpp 25, citizen_needsupdate.cpp 22, citizen_onremove.cpp 11,
citizen_lifeevent_handler.cpp 9, citizen_lifeevent.cpp 5, citizen_assignworkplace.cpp 4.

Settings: Dialog 48 (optionsdialog, newgamedialog, statusbarui, autosavedialog) · Component 11 ·
Game 3 · Player 1. Most read: statusIconEnable +0x0C 9, autoSlow +0x17 6. Container: Dialog 14 ·
Game 10 · standalone 5 · Player 3 · Tutorial 2; 9 of 26 fields are never read raw.

## 4. Blind spots

- **CitizenComponent read from outside its own bodies.** The method sees only receivers that are
  the first parameter of a citizen-named body. Other variables that hold a citizen
  (`partnerComp`, `other`, `cand` in citizencomponent.cpp; `ai` in citizenui.cpp; citizenai_*,
  citizenbehavior.cpp, statvaluedispatch.cpp, townhall*, professionui, starttown, pathtool) are not
  followed. Upper bound: 625 code lines in `code/Game` carry an offset typical of the citizen tail
  (+0x1E4…+0x288 and the IBP block); ≈400 of them are outside the nine files, not all citizens
  (CitizenAIComponent and CitizenBehavior share some offsets). Inventory C (`offsets`) owns them;
  what D adds is the retail layout they must be checked against. In citizencomponent.cpp itself,
  654 casts vs 191 citizen-read lines: the rest are CitizenDescription (`desc +`, ≈49 lines),
  workplace / storage / residence components, IBP bridges.
- **GameUpdateLoop passed as a parameter** (`TownMgr(gw)`, `void* gw` arguments) is not followed
  into the callee; aliases re-assigned mid-scope keep the first meaning; expressions split over
  more than two lines lose their cast (then the row says `addr`).
- **Pointee rows are a lower bound**: one alias level only; `m_game->` etc. are not attributed to
  the pointee's own fields (that class's header owner does it).
- **Dynamic offsets**: `SettingByte(int)` (camera_input.cpp, called with 0x14) and
  `SettingsByte(int)` (weather_particles.cpp, called with 0x16) — read by hand, not in the table.
- **Names**: CitizenComponent's serializer passes `nullptr` names, so its field names come from
  behaviour, the profile build's assertions (`_parent`, `_prevSibling`, `_nextSibling`,
  `workPlaceComponent`) and the dialogs; 11 fields stay `field_XX`/`timer_XX` at low confidence.
  The two containers' CLASS names are not settled (§5.8); the profile exe
  (`Application-x64-profile.exe`, off-limits to block 0) passes the header path to MemAlloc and
  would settle both in one read of its GameMain.

## 5. What surprised me (code or comment vs retail / §3a)

1. §3a "≈880 lines" is 808 raw + 29 named; the header they need is already complete — the
   problem is 30 `void*` members.
2. `CitizenComponent.h` banner: the dtor "InternalBasePtr dtor at +0x28" — retail runs
   `System::String::~String(+0x28)` (dtor 0x1401146f0; Serialize writes a String first).
   `citizencomponent_serialize.cpp` banner says +0xB0 is a MeshInstance; the CODE is right (a
   plain uint through Encoding slot 0x98); the MeshInstance is at +0xB8.
3. Header misdeclarations: `_pad240[8]` hides the serialized int32 **workplace slot** at +0x240;
   `_pad84` is an int32 retail reads (0x140112ae0 compares it with desc+0xB0); `data1D8` and
   `data1E0` are `uint64_t` but each holds two int32 (+0x1DC tool scene id, not serialized;
   +0x1E4 **clothing level**) — reads at +0x1DC/+0x1E4 land inside a declared field; +0x3C…+0x50
   and +0x80 are floats declared `uint32_t`; +0x28 is a String declared `uint64_t`.
4. Tree comments disagree about +0x250/+0x258 (spouse / partner / parent / guardian /
   reservation / "current-workplace link"). Retail `RemoveChild` 0x140112040: **+0x258 is
   `_parent`**, +0x260 the parent's first child (ring head), +0x268 `_nextSibling`, +0x270
   `_prevSibling`; +0x250 is the spouse.
5. +0x1E8 is the **clothing wear timer** (Update: `< 0` resets clothing +0x1E4 to 0); the tree
   comments it "education countdown".
6. `citizenui.cpp` calls the component it reads "CitizenAI" and says "+0x208 workplace": it is the
   CitizenComponent (+0x288 description, +0x3C age) and +0x208 is the school
   (IBP<AttendanceComponent>); the workplace is +0x238.
7. Ghidra labels mislead (known_port_issues 6 applies to retail labels too): `0x140112ae0
   CitizenComponent_ConsumeFuel` is the **happiness update** (+0x80, clamped to desc+0xA4);
   `0x140114900 CitizenComponent_OnCreate` serializes **CitizenDescription** fields;
   `0x14012a4f0` = `ResidenceComponent_IsHouseholdFull`, ported as `CitizenComp_helper_a4f0`;
   `CitizenComp_FUN_14012c3f0` is StatusIconComponent's SetFlag; `CitizenComp_AnyCitizenSick`'s
   receiver is CitizenBehavior.
8. Two different objects are called "Game": `GameUpdateLoop::m_game` (+0xF8, 0xF8 bytes, ctor
   0x14009ee70, used as the map/tile grid — profile has `Game\Map.cpp`) and the 0xE0 container
   (`game.h`'s `Game_Ctor`, "KeyMap"). The profile's header list has `Globals.h` and
   `GameConfiguration.h` without other owners: the likely retail names of the container and the
   settings block (medium; unconfirmed).
9. The tree's settings init (game_main.cpp) writes int32 at +0x0C and +0x18 where retail writes a
   byte (pads become 0, retail leaves MemAlloc garbage); its comment places the +0x15..+0x17 bools
   under `settings[3]` (they are in `settings[5]`). The retail key is spelled `ShowCelcius`.
10. §3a "CitizenComponent (654 casts in one file)": the count is right (654 `reinterpret_cast` in
    citizencomponent.cpp) but only 191 of its lines read the citizen; a CitizenComponent header
    alone removes well under half of that file's casts.

## 6. Proposal: blocks, order, what would change behaviour

Order (each header first, alone, W0 = declarations + `static_assert` only):

1. **W0-CIT — `CitizenComponent.h` + `CitizenDescription.h`** (one block: the bodies read both;
   `desc +` is ≈49 lines in citizencomponent.cpp alone). Rewrite the field list from
   `D_retail_layouts.tsv` (50 fields; keep `meshInstance[0x120]` raw), assert every field (today
   19 of 50). Read from ≥7 modules, so it goes first. Files: 2 headers. Behaviour: typing +0x28
   as `System::String` or +0x98 as an Array with a destructor makes `~CitizenComponent` run
   `~String` / MemFree — that is retail's dtor but a **`behaviour:`** commit (the tree ctor memsets,
   B38 D1/D2 PORT GAP). IBP / ExternalPtr members have defaulted dtors: neutral.
   W1-CIT then owns the 9 files (354 lines) plus the citizen reads C lists elsewhere.
2. **W0-SET / W0-CON** (small, parallel with 1): new `GameSettings` struct (16 fields, 0x34,
   suggested header `code/Game/Game/GameConfiguration.h`) and `GameContainer` struct (26 fields,
   0xE0) in `game.h`; `g_gameSettings` / `g_gameContainer` become typed pointers. 2 headers +
   game_main.cpp / game.cpp. Readers (17 + 14 files) convert inside whichever W1 block owns them.
   Only the `(uint8_t)gs[6]`-style int reads (3 lines) need `bool` reads; value-identical.
3. **Pointee headers** (owned by their class blocks, before GameUpdateLoop's members can be
   typed): CitizenBehavior (0x1C0), Map/"Game" (0xF8), EntityList (0xE8), ProfessionList,
   PlayerToolManager (0xB0), PlayerController (0x10C), Weather (0x78), DayCounter (0x18),
   EventNotifier, Tutorial, ImageCapture; engine ones exist (Graphics::Scene, Audio::Scene,
   System::Random, UI::UserInterface).
4. **W0-GUL — `GameUpdateLoop.h`** (1 file): add the 3 missing asserts; retype each `void*` member
   to its class pointer as soon as that class has a header (forward declarations suffice;
   layout-neutral, provable by the existing asserts).
5. **W1-GUL sweep** — by MODULE, not by class (808 lines in 236 files: the rename is one token per
   line): Component 293, Dialog 177, Player 99, Work 74, Action 61, standalone 43, Tutorial 37,
   Entity+Goal+Game 23. These file sets overlap the class blocks, so the GUL rename should be done
   by whichever block owns each file, after step 4, with the rule "`gw + 0xNN` → `gw->m_name`".

Widths retail contradicts: none that change behaviour (§1). A W1 conversion must keep the one real
two-field store (`*(int64_t*)(p + 0x44) = 0` zeroes hunger AND coldness, as retail does) as two
named stores in the same order, and the 11 float-through-`uint32_t` bit copies as float
assignments (identical bits; NaN payloads are copied either way). Receivers that are not the class
the body name says: `CitizenComp_FUN_14012c3f0` (StatusIconComponent), `CitizenComp_AnyCitizenSick`
(CitizenBehavior), `CitizenComp_helper_a4f0` (ResidenceComponent), `CitizenComponent_helper_a550 /
_a710` (StorageComponent) — they move to those classes, not to CitizenComponent.
