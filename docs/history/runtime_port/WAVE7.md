# Wave 7 — UI

In progress on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2 wave 7.

## Scope

`tools/runtime_census.py` (2026-09-07, before any Wave 7 registration):

| Namespace | in DLL | called by our tree | ported |
|---|---|---|---|
| UI | 1625 | 147 | 0 |

The `called` figure moved to **143** once lane C collapsed three phantom/twin pairs onto one
name each; later lanes moved it again. Re-run the census for the current number rather than
quoting this table.

The wave's denominator is the **147 functions our tree actually calls**, not the 1,625 the
namespace contains — the same rule every wave since 1 has used. UI is the largest single
namespace in the DLL (1,625 fns / 193 KB) and the largest `called` count of any wave so far,
but the called surface is 29 classes, and 22 of those are leaf widgets with a handful of
accessors each. Per-function lists with retail addresses and call sites: `wave7_lanes.txt`.

## Why this wave is different from 1–6

**1. It is the first wave whose classes form a deep inheritance hierarchy.** Every `*Element`
derives from `UI::Element`, which is polymorphic and whose layout every derived class's field
offsets are measured from. PLAN §4.3 risk 3 ("port classes whole") is live in a way it was not
in Wave 6, where `Video::Context` had no vtable at all.

**2. The vtable-ownership risk is nevertheless SMALLER than the PLAN row implies, and this was
measured before the wave opened, not assumed.** All 68 UI vtables are exported from the shipped
DLL, including `??_7Element@UI@@6B@`, `??_7ElementDescription@UI@@6B@`, `??_7DialogInstance@UI@@6B@`
and one per widget class. This is the `??_7Geometry@Graphics@@6B@` lever from Wave 6: a ported
ctor stamps the DLL's own vtable byte for byte instead of minting one, so a half-ported class
does not produce two vtables in play. `??_7UserInterface@UI@@6B@` is NOT exported — consistent
with `UserInterface` being non-polymorphic like `Video::Context`; lane C must verify that from
the ctor's first store rather than infer it from an absent export.

**3. Six of the 147 mangled names our tree resolves DO NOT EXIST in the shipped export table.**
Found by a pefile query over `wave7_lanes.txt` before any body was written
(`reference_resolveexport_phantom_names.md`). Each is assigned to the lane that owns the class:

| Phantom name | Lane | Verdict |
|---|---|---|
| `?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z` | B | TBD |
| `?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ` | B | TBD |
| `?BringDialogToFront@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z` | C | **benign — address fallback** |
| `?CloseDialog@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z` | C | **benign — address fallback** |
| `?GetFirstContainedDialog@UserInterface@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z` | C | **benign — address fallback** |
| `?SetText@EditElement@UI@@QEAAXVString@2@@Z` | D | TBD |

**CORRECTION, entered by lane C with evidence: this row's original premise was WRONG for all
three of its phantoms, and the error was mine.** This section used to assert that all six were
"live functional gaps" of the Wave 6 `Geometry::Attach`/`Detach` shape — silently dead call sites.
That inference does not hold. WAVE6.md's finding was that **`ResolveExport` has no address
fallback**, which is true and unchanged; but the *call sites* in `engine_funcs.cpp` have their own
`if (!fn) fn = ResolveAddr(Module::Runtime, <addr>)` fallback, which is a different mechanism, and
for all three lane-C names that fallback lands on the correct function
(`CloseDialog` → 0x180120F20, `BringDialogToFront` → 0x180120440,
`GetFirstContainedDialog` → 0x18011FB00). `InternalPtr<T>` has a non-trivial copy ctor, so MSVC x64
passes it **by address** in both the `AEAV` and by-value spellings, and `QEAA`/`QEBA` has no ABI at
all. **So the answer to "what has therefore never happened" is: nothing.** Nothing player-visible,
nothing silently dead. A phantom name is a real defect, but it is not automatically the Wave 6
shape — check for a fallback at the call site before claiming a functional gap.

Two reasons the fix still mattered, the second load-bearing:
  * The phantom audit is honest again — `tools/export_name_audit.py` is down from 9 to 3, and none
    of the remainder is a Wave 7 name.
  * **A port registered under the DLL's real name ONLY would have been dead at its only call
    site while reporting `calls=0 divergences=0`** — the call site would still be going through
    the address fallback to the DLL. That is `reference_uncalled_port_is_not_verified.md` with a
    clean-looking oracle on top. Every Wave 7 port of a name in this table must register under
    **both** the name and the address.

Lanes B and D still owe their three verdicts, and must apply the same test rather than assuming
either answer.

**4. `UI::UserInterface::ShowDialog` is a nested `MainLoop::Frame` loop** (WAVE4_LANE_H). It is
the reason `BANISHED_MODAL_ANSWER` exists. Lane C owns it and must not port it blind.

**5. Almost nothing here is a pure function.** The Wave 1/2 replay oracle (call ours, call
theirs, compare) is invalid for anything that mutates an Element tree, the dialog list, or the
sprite context. Lanes must argue their oracle per function — replay for the const accessors
(`GetValue`, `ChildCount`, `Width`, `GetText`, …), byte-comparison of the whole object for
mutators that write fields of an object somebody else owns, and a written NO-ORACLE holdback for
anything that allocates, frees, or reshapes a tree. An unverified port is not a port (WAVE2
allocator verdict).

## Lanes

| Lane | Scope | Fns | File | Kill switch |
|---|---|---|---|---|
| A | `UI::Element` + `ContainerElement` | 23 | `runtime/src/ui/element.cpp` | `BANISHED_RT_UIELEMENT` |
| B | `DialogInstance` + `Dialog` + `DialogController` + `ElementDescription` | 28 | `runtime/src/ui/dialog.cpp` | `BANISHED_RT_UIDIALOG` |
| C | `UserInterface` | 22 | `runtime/src/ui/userinterface.cpp` | `BANISHED_RT_UIMANAGER` |
| D | Input/value widgets (Spinner, Combo, NumberEdit, Number, Edit, Slider, Radio, Progress) | 27 | `runtime/src/ui/widgets_input.cpp` | `BANISHED_RT_UIWIDGETSIN` |
| E | Layout/content widgets (Table, Image, Label, Text, DocumentText, Tab, Page, Graph) | 24 | `runtime/src/ui/widgets_layout.cpp` | `BANISHED_RT_UIWIDGETSLAY` |
| F | `Sprite` + `SpriteSheet` + `SpriteContext` + `StringTable` + `UI::String` | 23 | `runtime/src/ui/sprite.cpp` | `BANISHED_RT_UISPRITE` |

Namespace-level switch: `BANISHED_RT_UI=0`. Registration aggregator:
`runtime/src/ui/rt_ui_port.cpp` (`RtUiPortInit`), mirroring `runtime/src/video/rt_video_port.cpp`.

### The cross-lane layout contract

Lanes D and E measure their field offsets from `sizeof(UI::Element)`, and lane B's
`ElementDescription::CreateInstance` builds the objects D/E port methods on. **Lane A owns
`runtime/types/ui/Element.h` and publishes it as its FIRST deliverable**, before writing any
body; D and E do their recon (export verification, decompiles, function tables) against the
profile build while they wait, and must not hand-roll a competing Element layout. Lane F owns
`runtime/types/ui/Sprite.h` and `SpriteSheet.h`, which already exist in partial form and which
A and E both read.

Nobody edits a header another lane owns. If a lane needs a field added to someone else's header,
it says so in its doc and asks.

## Pre-wave control (2026-09-07, scaffolding only, 0 Wave 7 ports)

`tools/rt_run.sh wave7-control -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900
BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6`

**180 sites, 25,240,587 calls, 80 divergences in 2 sites.** Clean exit (rc=0, 35s), empty
`standalone_crash.log`. Both divergent sites are exactly the pre-existing Wave 1 1-ULP sin/cos
family and nothing else — `Math::Transform::FromYawPitchOffset` (64 of 39,328 calls) and
`Math::Vector3::FromYawPitch` (16 of 3,694 calls). `[RtPort] UI: 0 exports registered` confirms
zero Wave 7 ports are live. Full site table: `wave7_control_autostart.txt`.

A second control on the save-load path, same exe:

`tools/rt_run.sh wave7-control-save9 -- BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1
BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6`

**200 sites, 49,886,841 calls, 47 divergences in 2 sites** — again only the Wave 1 family:
`Math::Transform::FromYawPitchOffset` (41 of 320,172) and `Math::Vector3::FromYawPitch` (6 of
2,760). No `Math::Vector3::Heading` divergence this run (WAVE5/WAVE6 controls saw 1 there;
this run saw 0 — noted, not chased, since it is a pre-existing residual either way). The load
actually ran: `[AutoStart] AUTOLOAD sequence fired`, all five `IO::CompressedMemoryStream`
sites show `calls=1`, `[MenuDLT::PhaseB] MenuScene+0x38=15533 elements` processed, and 7
`[PauseMenu]` lines confirm the pause-menu-driven quit fired. Clean exit, `standalone_crash.log`
0 bytes. Full site table: `wave7_control_save9.txt`.

Note per WAVE5.md/WAVE6.md: **do not pass `BANISHED_MODAL_ANSWER` on the AUTOLOAD path** — it
overrides the `modsMissing` confirm and the load silently never happens. Not passed here.

**Any NEW divergent site after Wave 7 lands is a Wave 7 defect.**

## Standing cross-lane hazards

**1. Register phantom-adjacent ports under BOTH the name and the address.** See §3. A port
registered under the DLL's real name only stays dead at a call site that is going through an
`if (!fn) ResolveAddr(Module::Runtime, …)` fallback — while its oracle reports
`calls=0 divergences=0`. That is `reference_uncalled_port_is_not_verified.md` with a clean-looking
oracle on top, and it would survive a wave undetected.

**2. `Element` vtable slot +0x48 (`Contains`, the hit test) — RAISED AND CLOSED IN-WAVE.**
Lane C's `UserInterface::GetFirstContainedDialog` port calls that virtual, so under a replay
oracle it is invoked TWICE per diffed call. Lane C recorded that as believed-harmless-not-proven
rather than claiming otherwise, and flagged the real danger as one of **attribution**: if any
widget overrode +0x48 with a mutating body, the site that diverges would be in lane C's file,
which would have been correct all along.

**Both widget lanes have now answered, by reading the shipped vtables rather than inferring:
no class overrides it.** All sixteen classes across lanes D and E — `Spinner`, `Combo`,
`NumberEdit`, `Number`, `Edit`, `Slider`, `Radio`, `Progress`, `Table`, `Image`, `Label`, `Text`,
`DocumentText`, `Tab`, `Page`, `Graph` — decode slot 9 (byte 0x48) of their exported
`??_7<Class>@UI@@6B@` to the identical address **0x1801169e0**, which is `Element::Contains`
itself. Slots 4–8 (`ComputeSize`/`ComputeLayout`/`Draw`/`Update`/`Enable`) do vary per class, so
the tables were genuinely read, not assumed uniform. Per-class tables are in WAVE7_LANE_D.md §9
and WAVE7_LANE_E.md §6a.

The hazard is therefore closed **for this wave's scope**, on evidence. It is not closed in
general: a later wave that ports a widget outside these sixteen, or that lands a `Contains`
override, re-opens it. Anyone who meets a `GetFirstContainedDialog` divergence should re-run this
check before debugging lane C.

Incidental, from the same read and worth having: `LabelElement` overrides slot +0x50 (`SetColor`),
and `DocumentText`/`Tab`/`Graph`/`Table` override slot +0x38 (`Update`) with their own bodies.
None is in Wave 7's scope.

**3. `ResolveAddr(Module::Runtime, addr)` DOES NOT MEAN "the DLL" — it consults the port
registry FIRST** (`standalone/resolve_standalone.cpp:325`, verified this wave). So any code that
reads the shipped instruction bytes at an address it also passes to `RegisterRuntimeAddr` gets
**its own function back and decodes its own prologue**. Only `RtOriginal` / `ResolveExportRaw`
(GetProcAddress) is guaranteed to reach the DLL.

Found and fixed inside Wave 7 lane A, whose thunk decoder hit it. This is not a UI-specific
hazard — it applies to every future wave that decodes shipped bytes, installs a detour, or builds
a trampoline, which is exactly what PLAN §4.1 decision 3 asks the first detour user to do. A
sweep of all six lane files this wave found **no other exposure**: the addresses the lanes call
through `ResolveAddr(Module::Runtime, …)` (unexported DLL internals 0x180004e30, 0x18002fea0,
0x18003ffe0, 0x1800416d0, 0x1800764c0, 0x180076900, 0x1800c7ad0, 0x1801189f0, 0x1801fa190) are
disjoint from the 61 addresses the lanes register. That disjointness is a fact about today's
tree, not a property anything enforces — re-check it whenever a lane adds a registration.

**4. "Never written" is meaningless without "…by WHOM". A written-extent constant is a property
of a particular WRITER, not of a type.**

The concrete facts: `Element::Element` never writes +0x54, +0x60, +0x64, +0x7C or +0x8C
(+0x60/+0x64 are `_layoutWidth`/`_layoutHeight`, filled by virtual slot 4 `ComputeSize`), and
`UI::String`'s three constructors never write its `pad0C`, which is why
`runtime/types/ui/String.h` exports `kUIStringWrittenBytes = 0x0C` against a `sizeof` of 0x10.
No byte-comparison against a *freshly constructed* object may include those ranges —
`reference_oracle_must_not_compare_unwritten_bytes.md`, the Wave 6 mistake that read as a
catastrophic port defect and cost a day.

**But the constant does NOT mean "these bytes are never written by anything", and applying it
uniformly is itself a defect.** This wave produced both halves of the lesson within an hour:

  * Lane E compared all 0x10 bytes of a `UI::Sprite` whose ctors write only 0x0C — the bound was
    too WIDE, and the comparison was not one it was entitled to make.
  * Lane A was then advised (by me) to adopt the same constant, and correctly declined. Its one
    `UI::String` use is a COPY, not a comparison, and retail's `?SetToolTip@` (0x180035740)
    copies sixteen bytes — `MOV RAX,[RDX+0x8]` / `MOV [RCX+0x50],RAX` is a full qword at +0x50,
    which writes +0x54, the very byte the constructors leave alone. Copying 0x0C would have
    DROPPED a write retail performs, and lane A's own replay oracle would then have reported a
    real divergence at +0x54. The bound would have been too NARROW.

So the same field is correctly excluded from one oracle and correctly included in another, in
the same lane, on the same type. Lane A's ctor BUILD oracle excludes +0x54 (neither ctor writes
it); its `SetToolTip` replay includes it (both implementations write it). There is a
"DO NOT fix this to `kUIStringWrittenBytes`" block at that call site carrying the two
instructions, so a future uniform sweep does not silently blind the one oracle whose function is
unusual for writing that field.

**The rule: before excluding a byte range from a comparison, name the writer you are excluding it
for.** A constant named for a type invites exactly the uniform application that breaks this.

**RESOLVED at the instruction level, and the wave lead's objection to lane E was WRONG on this
point.** Lane E checked all five of its `Sprite`/`UI::String`-shaped comparisons — `ImageElement`
`GetSprite` (0x1801182a0) / `SetSprite` (0x180118600), `LabelElement` `GetSprite` (0x1800429d0) /
`SetSprite` (0x180042980), `DocumentTextElement::SetText[UI::String]` (0x18003bfd0). Every one is
two unconditional 8-byte `MOV` pairs, never a narrower or conditional variant, and **none of the
five is a constructor** — they are all copies of an existing 16-byte value. So 16 is the width
retail itself moves, `kSpriteWrittenBytes` does not apply to any of them, and lane E's original
code needed no change.

Both objections to that lane are therefore settled, and they settled differently: the
**vacuous-mutator-oracle** objection was correct and produced a real rework; the
**Sprite-width** objection was not, and the right answer was found by reading instructions rather
than by applying either constant. Lane E also recorded that its *original* justification ("it
just copies raw bytes, so it does not need the type to be right") reached the correct answer for
the wrong reason — the real reason is a proven instruction-level copy width that happens to equal
`sizeof` here. That distinction is the whole of this hazard: a right answer from wrong reasoning
survives only until the next case, where lane A's `SetToolTip` shows it does not.

**5. Constructing a widget into a scratch buffer is not a free oracle.** `Element::Element` runs
`Object::Internal::Internal` (0x1800bfd10), which registers the object in the global handle table
(`InternalFactory::_singleton`, 0x1801fc6d8). That is a real, leaking mutation of shared state.

**6. `RuntimePortInit()` runs BEFORE `EngineFuncsInit()`** (`standalone/main.cpp:1614` then
`:1631`, verified by lane D), and `ResolveExport` consults the Wave 7 registry before
`GetProcAddress`. That ordering is what makes the seam work for the cached `fn_*` pointers in
`engine_funcs.cpp`: they resolve straight through to a registered port with no extra wiring.
Reverse the order and every `fn_*` would cache the DLL's address before any port registered, and
the whole registry would silently do nothing for that entire family of call sites. Nothing
enforces the order — it is two adjacent calls in one function.

## Result — CLOSED, verified clean

Source work closed 2026-09-07. **101 functions ported** of the 147 the wave opened against
(the called denominator moved to 149 as lanes resolved phantom/twin pairs; re-run the census
rather than quoting a fixed number). Per lane, reconciled three ways — static `++count`
increments, distinct registered functions, and each lane's own enumeration:

| Lane | Scope | Ported | Held back |
|---|---|---|---|
| A | `Element` + `ContainerElement` | 21 / 23 | 2 |
| B | `DialogInstance`/`Dialog`/`DialogController`/`ElementDescription` | 14 / 28 | 10 (+4 not portable) |
| C | `UserInterface` | 8 / 19 | 11 |
| D | Input/value widgets | 18 / 27 | 9 |
| E | Layout/content widgets | 20 / 24 | 4 |
| F | Sprite + text primitives | 20 / 23 | 3 |
| **Total** | | **101** | |

The live process prints `[RtPort] UI: 101 exports registered`. `tools/runtime_census.py` reports
**98 ported** — both are correct and they measure different things: the census's denominator is
the *called* surface, so registrations outside it (lane A's static `GetAlignedPosition`
delegation target, DATA symbols resolved by name) do not appear. Lane D's summary initially said
17 where its table, its code and its own enumeration all said 18; corrected.

### Integration runs (2026-09-07, exe stamp 15:01:20, full rebuild)

| Run | Sites | Diffed calls | Divergences | New sites |
|---|---|---|---|---|
| autostart (integration) | 223 | 26,011,765 | 106 in 2 sites | **none** |
| `Save9` (integration) | 241 | 50,972,828 | 83 in 4 sites | **2 — see below** |
| autostart (**final**) | 223 | 26,280,535 | 91 in 2 sites | **none** |
| `Save9` (**final**) | 241 | 50,848,191 | 44 in 2 sites | **none** |

**FINAL: 0 Wave 7 divergences across 77,128,726 diffed calls in the two closing runs.** Every
remaining divergence in either run is the pre-existing Wave 1 1-ULP sin/cos family
(`Transform::FromYawPitchOffset`, `Vector3::FromYawPitch`) and nothing else. Both closing runs:
`rc=0`, clean exit, `standalone_crash.log` **0 bytes**, world built, and on the save path the
load verified to have executed rather than inferred. `[RtPort] UI: 101 exports registered` in
every log.

The two `SpriteSheet` sites came back **`calls=12, divergences=0`** — not `calls=0`. That
distinction was specified in advance: a site that stops executing would be a regression wearing a
fix's clothing, and the fix is only proven by the compare still running and now passing.

Both runs: `rc=0`, clean exit, `standalone_crash.log` **0 bytes**, world built
(`PhaseB 15533 elements`), and on the save path the load verified to have actually executed
rather than inferred from a clean exit. The autostart run is **clean** — its only divergences are
the pre-existing Wave 1 1-ULP sin/cos family (`Transform::FromYawPitchOffset` 90/44,812,
`Vector3::FromYawPitch` 16/3,694).

**The integration run found TWO NEW DIVERGENT SITES on the save path — Wave 7 defects by this
document's own rule. Both are now closed; the history is kept because the reasoning is the
transferable part:**

```
UI::SpriteSheet::SpriteSheet()      calls=12   divergences=12
UI::SpriteSheet::AddSpriteInfo()    calls=12   divergences=12
```

Save-path only; neither executes on autostart. The rest of lane F is clean on the same path —
`GetSpriteCount`, `SetSpriteInfo` and `GetUVs` run from hundreds to 1.15M calls with 0
divergences — so this is not "SpriteSheet is broken", it is the ctor at +0x51 and
`AddSpriteInfo` at +0x34/+0x35.

**ROOT-CAUSED AND FIXED (lane F), verified independently by the wave lead in Ghidra before being
accepted. Both were the ORACLE comparing bytes NEITHER implementation writes. Neither was a port
defect, and no ported body changed.**

  * `SpriteSheet::SpriteSheet` (0x18011d600) ends
    `MOV qword [RCX+0x48],RDX` / `MOV byte ptr [RCX+0x50],DL` / `MOV RAX,RCX` / `RET`.
    The final write is a single **byte** at +0x50, so the written extent is **0x51** against a
    `sizeof` of 0x58; +0x51..+0x57 is never touched by the ctor. The oracle compared the full
    `sizeof`, so it was reading 7 bytes of 0xCD poison on the scratch side against allocator
    residue on the live side.
  * The shared `SpriteInfo` copy-construct helper (0x180004220) that BOTH implementations go
    through does two refcounted qword copies at +0x00/+0x08, then nine dword copies +0x10 through
    +0x30, then `RET`. Its last write ends at **0x34**, so `_pad34` (+0x34..+0x37) is never read
    from the source nor written to the destination on either path. The oracle compared
    `sizeof(SpriteInfo)` (0x38) across two *unrelated* allocations, whose leftover bytes had no
    reason to agree.

Fix: `kSpriteSheetWrittenBytes = 0x51` and `kSpriteInfoWrittenBytes = 0x34` added to
`runtime/types/ui/SpriteSheet.h` with the disassembly as evidence, and the two `RT_DIFF_BYTES`
extents narrowed to them. `SetSpriteInfo`'s full-`sizeof` compare needed no change and had
already reported 0 divergences on the same run — its scratch snapshot copies the real entry's
bytes, tail included, before mutating, so the untouched tail is byte-identical by construction.

**Two hypotheses were considered and DISCARDED on evidence, which is the part worth keeping:**

  1. *A real infidelity where our ctor writes bytes retail leaves alone.* The identical
     `theirs = cd cd cd…` evidence is equally consistent with this, and it is the more serious
     reading. Excluded by reading the ctor's final store, not by trusting the poison pattern.
  2. *Array growth policy — a capacity field of 32 vs 16.* The `0x20`-against-`0x10` on two
     consecutive calls was an attractive story (the wave lead floated it) and would have
     implicated lane F's delegation to the DLL's own append helper. Excluded by showing the copy
     helper cannot touch those bytes on either path: the pattern is unrelated heap residue in a
     scratch allocation. Coincidence, not signal.

This also corrected a stale claim in `SpriteSheet.h` itself, which said `AddSpriteInfo` "zeroes"
`_pad34`. It does not and never did — lane F's own `newEntry._pad34 = 0` has no effect, because
the copy helper never copies that field.

**Narrowing a compare window until a divergence disappears is not an acceptable resolution** —
it was the required standard here, and it was met: each narrowing is justified by the writer's
final store, read out of the shipped binary and checked twice, by the lane and by the lead.

### Coverage

No `calls=0` sites at all in the save9 run — everything that registered a diff site executed.
That retires the "registered but never exercised" worry for the wave, which neither Wave 5 nor
Wave 6 could claim at their boundaries.

**One exception, recorded as unverified**: `UI::EditElement::SetText` has **zero calls in both
runs**. It is lane D's phantom-name fix target — the case where the old fallback resolved to
`UI::TextElement::SetText`, a different class entirely — and its only live caller is
`SpriteCaptureUI::Serialize`, a dev tool neither harness path touches. The fix is landed,
registered and completely unexercised (`reference_uncalled_port_is_not_verified.md`).
