# Wave 8 — Lane A — the `Engine` loop family

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/engine/loops.cpp`, `runtime/types/engine/UpdateLoop.h`, this
document, and the six source comments the port disproved (§6).

**Result: 12 of 14 ported. One hold-back with a written reason
(`?LoadResources@UpdateLoop@Engine@@QEAAXXZ`), and one name that is DATA and is
deliberately not registered (`?_loader@LoaderLoop@Engine@@1VLoader@2@A`).**
`RtEngineLoopsRegister()` returns **12** and also makes 12 `RegisterRuntimeAddr`
calls. Two real defects were found and fixed at their sources (§6.1, §6.2).

Compiling `runtime/src/engine/loops.cpp` with the project's exact flags
(`-std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type
-include standalone/env_cache.h`) at `-g -O0 -D_DEBUG`, `-O2` and
`-O3 -DNDEBUG` is **warning-clean**. `runtime/types/engine/UpdateLoop.h`
compiles standalone and `static_assert`s **19 field offsets plus 5 sizes**; it
also compiles in one translation unit with the exe tree's `engine/types/`
headers (see §9).
`tools/export_name_audit.py`: **651 distinct `ResolveExport` names, 0 not in
Runtime-x64.dll** (the three phantoms WAVE8.md recorded at the wave boundary
were lanes C and E's and are gone from the tree as of this run — not this
lane's doing, reported as observed). `tools/abi_retslot_audit.py`: **0
suspicious of 48**. `tools/runtime_census.py` sees the registrations:
**Engine 108 in DLL / 29 called / 15 ported** (the 15 is this lane's 12 plus
three of lane B's, which was landing concurrently).

Not run by this lane, by instruction: `make`, the game, `tools/rt_run.sh`,
`sa-run`, `git commit`. Runtime numbers below were written as **predictions**;
where the integrator's run has since measured one, the measurement is added
beside the prediction rather than replacing it (§3, "MEASURED at integration").
Read that section before trusting the digest: 10 of the 12 sites are confirmed
executing with **zero divergences in the family**, one (`IsRunning`) is still
unobserved, and `tools/rt_run.sh`'s digest hides 197 of the run's 249 sites
behind a `tail -80`.

---

## 1. The function table

Retail addresses and ordinals were read out of the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with pefile **before any body was
written** (`reference_resolveexport_phantom_names.md`: a name the DLL does not
contain resolves NULL in silence, the port still registers, and the census still
counts it). **All 14 names, plus the five `??_7` vtable symbols and both dialog
destructors, exist in the shipped table.** No phantoms in this lane.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0UpdateLoop@Engine@@QEAA@XZ` | 0x1800ddd30 | 468 | yes | build | `Engine::UpdateLoop::ctor` |
| `??1UpdateLoop@Engine@@UEAA@XZ` | 0x1800dddb0 | 704 | yes | shadow | `Engine::UpdateLoop::dtor` |
| `?IsRunning@UpdateLoop@Engine@@QEBA_NXZ` | 0x1800dd7b0 | 3160 | yes | **replay** | `Engine::UpdateLoop::IsRunning` |
| `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` | 0x1800dd7d0 | 3210 | **no** | — | — |
| `?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z` | 0x1800dc050 | 3204 | yes | no-oracle marker | `Engine::Loader::Load` |
| `??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z` | 0x1800dc1e0 | 261 | yes | build | `Engine::LoaderLoop::ctor` |
| `?_loader@LoaderLoop@Engine@@1VLoader@2@A` | 0x1801fe7b8 | 4794 | **data — not registered** | — | — |
| `??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z` | 0x1800db3f0 | 153 | yes | build | `Engine::DialogLoopT::ctor` |
| `?Load@DialogLoopT@Engine@@UEAAXXZ` | 0x1800db140 | 3201 | yes | shadow | `Engine::DialogLoopT::Load` |
| `?Startup@DialogLoopT@Engine@@UEAAXXZ` | 0x1800db2f0 | 4093 | yes | shadow | `Engine::DialogLoopT::Startup` |
| `?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z` | 0x1800db270 | 4485 | yes | **replay (arithmetic half)** | `Engine::DialogLoopT::Update[time]` |
| `??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z` | 0x1800db0d0 | 151 | yes | build | `Engine::DialogLoaderLoopT::ctor` |
| `?Load@DialogLoaderLoopT@Engine@@UEAAXXZ` | 0x1800dad50 | 3200 | yes | shadow | `Engine::DialogLoaderLoopT::Load` |
| `?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ` | 0x1800db000 | 4092 | yes | shadow | `Engine::DialogLoaderLoopT::Startup` |

Every port is registered **twice**, by mangled name and by retail address. The
name form is what every live call site uses today; the address form is not
housekeeping here, because **nine of the twelve names have an
`if (!fn) fn = ResolveAddr(Module::Runtime, <addr>)` fallback in
`engine_funcs.cpp`** and that door would otherwise reach a different
implementation for the same live object (the Wave 5 lane D failure mode).

Every `RegisterRuntimeExport` call spells its mangled name as a **string
literal**, never a named constant — `tools/runtime_census.py` cannot see the
latter, and that blind spot has produced a false progress report twice
(`reference_census_cannot_see_nonliteral_registrations.md`). The `k*` constants
at the top of `loops.cpp` exist only for `RT_ORIGINAL_ONCE` lookups inside diff
guards; three that ended up unused were deleted rather than left as decoration.

---

## 2. The vtable question — WAVE8.md §1, answered from the constructors

WAVE8.md §1 requires each lane to say, **from the ctor's first store or from the
export table**, why it does or does not need the detour. For this lane the
answer is Wave 6's reason (`??_7Geometry`), and it was checked in the ctor
rather than inferred from an export existing:

| Class | `??_7` export | address | the ctor's stamp | at |
|---|---|---|---|---|
| `Engine::UpdateLoop` | ord 1519 | 0x180155ba0 | `LEA RAX,[0x180155ba0]` | 0x1800ddd44 |
| `Engine::LoaderLoop` | ord 1462 | 0x180157940 | `LEA RAX,[0x180157940]` | 0x1800dc20c |
| `Engine::DialogLoopT` | ord 1422 | 0x1801579a0 | `LEA RAX,[0x1801579a0]` | 0x1800db414 |
| `Engine::DialogLoaderLoopT` | ord 1421 | 0x1801578d0 | `LEA RAX,[0x1801578d0]` | 0x1800db0fd |
| `Engine::Loader` | ord 1461 | 0x180157a08 | `LEA RAX,[0x180157a08]` | 0x1800dc111 |

Five for five, exact. Each ported constructor **resolves the exported symbol by
name and stamps that**; nothing in this lane mints a table. So an object our
ctor built and one the DLL's ctor built carry the identical vtable pointer.

### And the DLL's own internal callers, MEASURED — not inferred from the vtables

Lane B pointed out that the vtable argument answers the *object* question, not
the *caller* question, and asked for the same `get_xrefs_to` sweep it ran on
`MainLoop`. Run over all twelve ported addresses in `Runtime-x64.dll`, ignoring
export-table and unwind DATA rows:

| Ported function | DLL-internal `CALL`s | which |
|---|---|---|
| `??0UpdateLoop@` | 2 | 0x1800dc206 (inside `LoaderLoop::LoaderLoop`), 0x1800db40e (inside `DialogLoopT::DialogLoopT`) — **both are functions this lane also ports** |
| `??1UpdateLoop@` | 1 real + 6 unwind funclets | 0x180031951, inside `UpdateLoop`'s own scalar-deleting dtor (0x180031900, vtable slot 0). The 0x18013e2xx rows are `Unwind@` cleanup thunks, not ordinary calls |
| `?IsRunning@UpdateLoop@` | **0** | export table only |
| `??0LoaderLoop@` | 1 | 0x1800db0f7, inside `DialogLoaderLoopT::DialogLoaderLoopT` — **also ported here** |
| `?Load@Loader@` | 2 | 0x1800db04c (the tail JMP in `DialogLoaderLoopT::Startup` — **ported here**) and 0x1800dc26b, inside `?Startup@LoaderLoop@` (0x1800dc260), **which is NOT in this lane's scope** |
| the four `Load`/`Startup`/`Update` virtuals | 0 | only their own vtable slots (see below) |
| the two dialog ctors | 0 | — |

**Still no detour, and now for a measured reason.** Every internal caller but
one is itself a function this lane ports, so our body reaches our body. The one
exception is `LoaderLoop::Startup` → `Loader::Load`, and it is theoretical in
our process: every `LoaderLoop` here is built by our tree and given
`g_outerLoaderLoopVtable`, whose Startup calls `fn_Loader_Load` — the DLL's
`LoaderLoop::Startup` never runs. If a future lane ports `LoaderLoop::Startup`
that gap closes; until then it is stated, not papered over.

**One fact this sweep changed.** `~UpdateLoop` is **inlined** into the derived
destructors rather than called: `??1DialogLoopT@Engine@@UEAA@XZ` (0x1800db340)
calls `ExternalFactory::Clean` (0x1800be310) directly and tail-JMPs to
0x180010ab0, with no call to 0x1800dddb0 anywhere in it. So registering
`??1UpdateLoop@` only ever affects callers that invoke it as a standalone
function, which in our process is exactly one — `GameUpdateLoop_Destroy`
(`code/Game/Game/gameupdateloop.cpp:3261`). That is the right path and the only
one; the port is neither dead nor competing with an inlined copy on the same
object.

**A free confirmation of §2's slot table.** The DATA xrefs land inside the
vtables at exactly the offsets the table predicts: `Load@DialogLoopT` at
0x1801579d0 (= 0x1801579a0 + 0x30, slot 6), `Startup@` at 0x1801579a8 (slot 1),
`Update@` at 0x1801579b8 (slot 3), `Load@DialogLoaderLoopT` at 0x180157900
(= 0x1801578d0 + 0x30, slot 6) and `Startup@` at 0x1801578d8 (slot 1). Derived
independently of the qword dump above and agreeing with it.

The port-wide detour count therefore stays at **0** after this lane.

### The slot counts, read out of the binary

Not taken from the base class's count. `reference_vtable_slot_count_from_the_name_string.md`
is the discipline here: a table sized to the base mis-dispatches every derived
virtual, and in the tutorial wave that made slot 8 call the constructor and
auto-pass 61 goals. The region 0x1801578d0 … 0x180157a18 was read as raw
qwords:

| Class | table | slots | ends |
|---|---|---|---|
| `DialogLoaderLoopT` | 0x1801578d0 | **13** | one qword of zero padding at 0x180157938 |
| `LoaderLoop` | 0x180157940 | **12** | no padding |
| `DialogLoopT` | 0x1801579a0 | **13** | — |
| `Loader` | 0x180157a08 | **2** | `Run` 0x1800dbfc0, then 0x18000c590 |

Slot 12 (`+0x60`) in both dialog tables is `_purecall` (0x18007ec28) — that is
`CreateDialog`, and the exe's concrete subclasses supply it. Our own
`g_menuDialogLoopVtable` is already 13 slots, which now has a second,
independent confirmation.

Two small facts worth keeping: `DialogLoaderLoopT` slot 8 is
`LoaderLoop::OnVideoError` (0x1800dc360) while `DialogLoopT` slot 8 is the
`UpdateLoop` base's (0x180031760) — the tables are not uniform, so a stub that
assumes one shape for both is wrong; and `LoaderLoop` adds no virtual of its
own beyond overriding six of the base's.

---

## 3. Is the port actually REACHED? (the `calls=0` trap)

`reference_uncalled_port_is_not_verified.md` — a port with no callers reports
`calls=0 divergences=0` forever, which reads as a pass, and a wrong
`Quaternion::operator*` survived all of Wave 1 that way. **Three of this lane's
functions are `UEAA` — virtuals — and registering an export does not rewrite the
DLL's own vtables.** They are still reached, and here is the chain, checked
rather than assumed:

* Every `DialogLoopT` / `DialogLoaderLoopT` in our process is built by our tree
  and then has its vtable **overwritten** with an exe-subclass table we own
  (`g_menuDialogLoopVtable`, `g_splashDialogLoopVtable`,
  `g_outerLoaderLoopVtable`).
* Those stubs call the base through `fn_DialogLoopT_Startup` /
  `_Load` / `_Update` and `fn_DialogLoaderLoopT_Startup` / `_Load`, which
  `engine_funcs.cpp` resolves with `ResolveExport`.
* `RuntimePortInit()` runs at `standalone/main.cpp:1614`, `AddressRegistryInit`
  at 1626 and `EngineFuncsInit` at 1631 — **the port registry is populated
  before either caches a pointer.**

Live call sites, one per function, so nothing here is speculative:

| Function | reached from |
|---|---|
| `DialogLoopT::Startup` | `standalone/menudialogloopt_stubs.cpp:321` |
| `DialogLoopT::Update` | `standalone/menudialogloopt_stubs.cpp:528` |
| `DialogLoopT::Load` | `standalone/menudialogloopt_stubs.cpp:645` |
| `DialogLoaderLoopT::Startup` | `standalone/splash_loop_stubs.cpp:194` |
| `DialogLoaderLoopT::Load` | `standalone/splash_loop_stubs.cpp:173`, `:558`, `code/Game/Game/game_main.cpp:776`, `:780` |
| `UpdateLoop::IsRunning` | exe dispatch slot 0x140152850, registered in `standalone/reg_unreg_lane3.cpp:136` with `ResolveExport`, called from `code/Game/Goal/populationgoal.cpp:141` |
| `UpdateLoop::~UpdateLoop` | `code/Game/Game/gameupdateloop.cpp:3261` |
| `Loader::Load` | `standalone/loader_loop_stubs.cpp:133`, `:244`, and our own `DialogLoaderLoopT::Startup` |
| the four ctors | `mainmenudialog.cpp`, `newgamedialog.cpp`, `saveloaddialog.cpp`, `game_main.cpp`, `worldsetup_reload.cpp`, `tutorialui.cpp`, `tutorial_next_scenario.cpp` |

**Prediction, to be checked at integration:** all twelve sites report a non-zero
`calls=` on an autostart run. If any of them reports `calls=0`, that is the
finding, not a pass.

### MEASURED at integration — 10 of 12 confirmed executing, 1 still open

Read out of `C:/GOG Games/Banished/standalone_error.log` after the integrator's
run (this lane ran nothing). A `[RtDiff] site[N] = <name>` announcement is
emitted on a site's **first call**, so an announcement is proof of execution:

| Site | status |
|---|---|
| `UpdateLoop::ctor`, `LoaderLoop::ctor`, `Loader::Load`, `DialogLoopT::ctor` / `Load` / `Startup` / `Update[time]`, `DialogLoaderLoopT::ctor` / `Load` / `Startup` | **announced — executing** |
| `UpdateLoop::dtor` | **calls=1, divergences=0** |
| `UpdateLoop::IsRunning` | **NOT YET OBSERVED — open** |

**Zero divergences name any Engine loop-family site.** The only two functions
diverging in the whole run are `Math::Transform::FromYawPitchOffset` and
`Math::Vector3::FromYawPitch` — the known Wave 1 one-ULP residual family
(last-bit, `e2` vs `e3`), which WAVE8.md predicted the short control would not
reach and a longer run would. Not this lane's.

`?IsRunning@` is the one still open, and it is the one that matters most: it is
the **only function in this lane with a full replay oracle**, so it is where a
divergence would be most informative. It reaches us through exe dispatch slot
0x140152850 from `code/Game/Goal/populationgoal.cpp:141`, so it needs a run that
actually evaluates population goals. It should be exercised, not assumed.

**A trap in the measurement itself, worth more than the result.**
`tools/rt_run.sh:70` ends its extraction with `| tail -80`. The run registered
**249** diff sites; the digest in `wave8_integration_autostart.txt` contains
**52**. Eleven of this lane's twelve were absent from the digest purely because
they scrolled off — which reads *exactly* like the `calls=0` dead-port failure
this section was written to guard against, and was nearly filed as one. Raised
with the integrator. **Read the full `standalone_error.log`, not the digest,
before concluding any port is unreached.**

---

## 4. The layouts — `runtime/types/engine/UpdateLoop.h`

Five structs, 19 asserted offsets, five asserted sizes. Where an exported
one-line accessor pins a field it is cited, because that gives the offset AND
the engine's own name for it.

```
Engine::UpdateLoop            0x28
  Engine::LoaderLoop          0x48
    Engine::DialogLoaderLoopT 0x188
  Engine::DialogLoopT         0x168
System::Thread                0x30   (Wave 2, runtime/types/system/Thread.h)
  Engine::Loader              0x38
```

`DialogLoopT::_ui` at +0x40 is not deduced — it is the return value of
`?GetUserInterface@DialogLoopT@Engine@@QEAAAEAVUserInterface@UI@@XZ`
(0x18004d7c0), whose whole body is `LEA RAX,[RCX+0x40] ; RET`. The UI is
declared as lane-F's `UI::UserInterface` (0x128 bytes) rather than as opaque
bytes, so the two layouts are checked against each other by the compiler; the
`DialogLoaderLoopT` position is independently confirmed by `Startup`, which
addresses the `_addDialogs` array as `this + 0x88` = 0x60 + the UI's own 0x28.

### `UpdateLoop::_loadBit` is a rented slot, not a flag word

The whole of `UpdateLoop::UpdateLoop` past the field zeroing is a bit
allocation: it scans `ExternalFactory + 0x04` with `BT` for the lowest **clear**
bit, sets it, and stores that bit's **index** at +0x20 (or −1 if all 32 are
taken). `~UpdateLoop` publishes the index as `ExternalFactory::_cleanBitIndex`,
calls `Clean()`, and only then clears the bit. **The order is load bearing** —
`Clean` evicts every resource whose usage mask reaches zero once this bit is
cleared from it, so clearing the mask first would sweep for the wrong bit.

Consequence worth recording: **the engine supports at most 32 simultaneously
live `UpdateLoop`s.** The 33rd gets `_loadBit == −1` and silently owns no
resource-lifetime slot.

---

## 5. Four oracles, argued per function

WAVE8.md §5 is explicit that almost nothing in this wave is pure and that
replay is invalid for anything with a side effect. Each function got its own
argument rather than a default.

### (a) Replay — `IsRunning`, and half of `Update`

`?IsRunning@` is the only genuinely const, side-effect-free function in the
lane: `this != null && this == MainLoop::_singleton->[+0x20]`. Full replay
against the DLL on every call, `RT_DIFF_SCALAR`.

Note it tests its own `this` for null. That guard is **faithful** — retail
really does `TEST RCX,RCX / JZ` — so CLAUDE.md's "don't add a null check" rule
cuts the other way here: removing it would diverge.

`?Update@DialogLoopT@` splits cleanly into a pure half and an impure one, and
the pure half is fully replayed. The body rounds the incoming `Time` **through a
float**:

```
CVTSI2SD xmm0,[rdx] ; MULSD xmm0,[_secondsPerTick] ; CVTPD2PS xmm0,xmm0
CVTPS2PD xmm1,xmm0  ; DIVSD xmm1,[_secondsPerTick] ; CVTTSD2SI rax,xmm1
```

which is exactly `Timer::TicksToSeconds` (0x1800ca820) followed by
`Timer::SecondsToTicks` (0x1800ca800) — verified instruction for instruction
against both exported bodies. The port calls those two, so it is bit-exact
without duplicating `Timer::_secondsPerTick` (a QPC-derived runtime global);
this is the decision Wave 2 already took for `System::Time`'s own ctor. The
oracle recomputes the composition through the DLL and compares the tick count,
which catches a dropped narrowing or a reversed order — **the narrowing is
lossy and matters for any tick count over 2²⁴**, so folding it away would be a
real divergence, not a tidy-up. The `UI::UserInterface::Update` call is not
replayed and obviously cannot be.

### (b) Build — the four constructors

A ctor cannot be replayed onto the object it just built, and not onto a plain
scratch copy either: every one of these takes a slot out of ExternalFactory's
32-bit mask, and the two dialog ctors take a reference on the caller's `String`.

So this is Wave 7's **poison twin**: two scratch buffers filled with the same
`0xCD`, ours on one and the DLL's on the other, all N bytes compared. The poison
makes every byte **neither** writes compare equal by construction, so no
hand-written mask of unwritten ranges is needed at all — and that hand-written
mask is precisely the mistake that read as a catastrophic port defect and cost a
day in Wave 6 (`reference_oracle_must_not_compare_unwritten_bytes.md`). It
matters here: `LoaderLoop` leaves +0x41…+0x47 untouched and `UserInterface`
leaves six ranges untouched, and none of them has to be enumerated.

**One addition this lane makes to the pattern.** The mask slot each run takes is
**undone between the two runs**, so both runs see an identical factory mask and
allocate the *same* bit index — which means `_loadBit` is compared like every
other field instead of being excluded for a bookkeeping reason. Nothing is
excluded from these four comparisons.

The undo is `mask &= ~(1 << bit)` — the last step of `~UpdateLoop`, and
**deliberately not the whole destructor**. `~UpdateLoop` also calls
`ExternalFactory::Clean()`, a real global sweep of unreferenced resources; a
scratch object that never loaded anything has nothing for Clean to collect, so
running it would be a behaviour change the oracle imposed on the boot path, not
an undo. The two dialog runs each release their one string reference afterwards,
with the destructor's own idiom. That is the complete undo: the embedded
`UI::UserInterface` ctor (0x180120de0) is 40 field stores and nothing else — no
allocation, no global registration — which was checked, not assumed.

Coverage: **every field store in all four ctors**, including the vtable pointer,
the `_autoAdvance = 1` literal, the `Time(0.0f)` conversion, and the acquired
string pointer.

### (c) Shadow — the destructor and the two `Load`s and two `Startup`s

Five functions where running the original beside ours would do the work twice
for real:

* `~UpdateLoop` would hand the same slot back twice and run `Clean()` an extra
  time, evicting live resources.
* both `Load`s call `ExternalFactory::FindOrLoad`, which on a cold cache reads
  the dialog off disk and on a warm one re-tags the usage mask.
* both `Startup`s dispatch slot 12, which **constructs a live
  `UI::DialogInstance`** with a handle-table entry, and then queues it for
  display. A replay builds a second dialog and shows two.

Each gets `RT_DIFF_SHADOW` with a verdict over what the body was supposed to
achieve. The destructor's predicate is three named terms —
`vtableRestamped`, `bitReleased`, `cleanBitPublished` — and the message prints
all three with the bit index, because
`reference_shadow_predicate_must_name_its_failing_term.md`: a predicate whose
message does not name every term it tests reports "something differed" and sends
the next reader looking in the wrong place.

**Stated plainly rather than dressed up:** the two `Load` shadows and the two
`Startup` shadows are weak. `Load`'s predicate is `true` — it records that the
site was reached and how many times, and says in its own message that there is
no oracle. `Startup`'s only checks that the field it wrote holds the dialog it
was handed. Neither would catch a wrong type-id filter or a wrong array append.
What actually protects those four bodies is that each is under fifteen
instructions, every call in them is an exported function verified elsewhere, and
the two `Load`s are byte-identical to each other apart from the +0x20 base
offset — which is why they are **written out twice rather than shared**: retail
emitted two bodies, and a shared helper would hide a divergence between them.

### (d) No oracle, said so — `Loader::Load`

Marked with `RT_DIFF_SHADOW(true, "no oracle: mounts the mod filesystem and
wakes the loader thread", …)`. This function switches the mod filesystem over
and then **starts the background load**; replaying it would remount the packages
and start a second load of the same `UpdateLoop`. The marker exists so the site
appears in the summary with a call count — a function that is executed but
unoracled is a different thing from one that is never executed, and the summary
should be able to tell them apart.

What it does, so a reviewer can check the order without re-deriving it:

```
this->_loop = loop
allows = loop->vtable[11]()                      # AllowsModChanges, base returns false
if (Modification->_savingEnabled != allows):
    if (allows): _savingEnabled = 1; RefreshList(); _dirty = 1
    else:        _savingEnabled = 0; Array<Mod>::operator=(&_savedMods, &_mods)
ExternalFactory->_loadEnabled = true
Modification::SetupFileSystem()
ExternalFactory->_loadEnabled = false
Thread::Start(this)                              # ResetEvent(hFinished); SetEvent(hStart)
```

The last line is a finding in its own right: retail **inlines**
`System::Thread::Start`, and the two Win32 calls are instruction-for-instruction
`?Start@Thread@System@@QEAAXXZ` (0x1800ca610), which Wave 2 already ported. So
`loops.cpp` calls `RtSystem_Thread_Start` and keeps `<windows.h>` out of the
file — PLAN §4.1 decision 1 holds for this lane with no `_win` twin needed.

---

## 6. Comments this port disproved, fixed at their sources (PLAN §6 step 8)

### 6.1 A wrong fallback address — `engine/funcs/engine_funcs.cpp`

```c
fn_UpdateLoop_Ctor = ResolveExport("??0UpdateLoop@Engine@@QEAA@XZ");
if (!fn_UpdateLoop_Ctor)
    fn_UpdateLoop_Ctor = ResolveAddr(Module::Runtime, 0x1800dcc00);   // WRONG
```

`??0UpdateLoop@Engine@@QEAA@XZ` is at **0x1800ddd30**. 0x1800dcc00 is not a
function entry at all: it is an address **inside the body of
`Engine::MainLoop::OnVideoError`** (0x1800dc5e0 … 0x1800dcd12). Latent, because
the `ResolveExport` above has always succeeded — but a fallback is only ever
taken when something is already wrong, which is exactly where a wrong address
hides. Fixed to 0x1800ddd30, with the old value and why it was wrong kept in the
comment. The other nine loop-family fallbacks in that file were checked against
the export directory and are all correct.

### 6.2 `ExternalFactory + 0x04` is not padding — `runtime/types/object/External.h`

Wave 3 declared it `_pad04`, "zeroed with +0x00 as one qword". The zeroing is
true; the conclusion is not. It is the **allocation mask for the 32 clean-bit
slots**, scanned and set by `Engine::UpdateLoop::UpdateLoop` and cleared by
`~UpdateLoop`. Wave 3 saw no writer because both writers live in `Engine`, a
namespace that was five waves away. Renamed to `_cleanBitMask` with the two
functions that own it named in the comment; no code read the old name (the other
`_pad04`s in `TypeInfo.h` are a different struct and are untouched), and
`external.cpp`, `type_system.cpp` and `filesystem.cpp` still compile.

This is the shape `reference_never_written_needs_by_whom.md` warns about, seen
from the other side: "no writer" is only ever true **relative to the code you
have looked at**.

### 6.3 `LoaderLoop + 0x40` is not `m_finished` — `engine/types/LoaderLoop.h`

The comment said "set to 1 by loading thread on completion". It is not written
by the loading thread or by anything else: the **constructor** sets it to 1 and
nothing in the family writes it again. `LoaderLoop::Update` (0x1800dc2a0) only
**reads** it, as the first of three terms; `LoaderLoop::IsFinished`
(0x1800dc150) does not read it at all. The completion signal both actually
consult is **bit 0 of `Engine::Loader::_loader`'s `System::Thread::runFlags`**
(0x1801fe7c0).

Renamed `m_autoAdvance` in `LoaderLoop.h` and `DialogLoaderLoopT.h`, with the
old name and the correction recorded. The three sites in `code/Game/` that write
0 there (`saveloaddialog.cpp:882`, `tutorialui.cpp:122`, `game_main.cpp:742`)
were **doing the right thing for a mis-stated reason** — zeroing it suppresses
the inner loop's own transition so the outer `LoaderLoop` drives it — so only
their comments changed. `DialogLoopT.h`'s cross-reference was updated too.

### 6.4 Two mis-named Loader globals — `engine/funcs/engine_funcs.h`

`g_pLoaderFlags` (0x1801fe7c0) and `g_pLoaderCompletionEvent` (0x1801fe7e0) are
described as `_loader.m_flags` and `_loader.m_completionEvent`, as though Loader
owned them. `Engine::Loader` derives from `System::Thread` (0x30 bytes,
`runtime/types/system/Thread.h`) and adds exactly one field, `_loop` at +0x30;
so those two are `Thread::runFlags` (+0x08) and `Thread::hFinished` (+0x28) —
the base's fields. Both comments now say so, and note that the ResetEvent /
SetEvent pair at the end of `Loader::Load` **is** `Thread::Start`, inlined.

### 6.5 Confirmed, not changed

`engine/types/UpdateLoop.h`'s vtable address (0x180155BA0), its 12-slot count
and its slot names; `engine/types/DialogLoopT.h`'s 0x1801579A0 and 0x168;
`engine/types/LoaderLoop.h`'s 0x180157940 and 0x48; `DialogLoaderLoopT`'s 0x188
and every offset in it. Those were all right, and saying so is worth as much as
the corrections — the next reader should not have to re-derive them.

---

## 7. The two names not registered, and why

### `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` (0x1800dd7d0, ord 3210) — HOLD-BACK

Fully reversed, deliberately not landed. Three reasons, the third decisive.

1. **It is ~250 instructions, and there is no oracle for any of it.** It walks
   `IO::Modification`'s mod array (stride 0x80, gated on the `_enabled` byte at
   +0x50), builds a per-mod path with `String::Format` against two format
   strings at 0x180157be8 / 0x180157bf8, calls `ExternalFactory::FindOrLoad`
   once per enabled mod, probes the filesystem with
   `FileSystem::ExistsInPackfile` and — on the miss branch — a raw
   `GetFileAttributesW` through a `WideString`, then builds a command-line
   string under the allocator's own critical section and stores one pointer at
   `this->_resourceList`. Replaying it re-loads every mod's resource list;
   a shadow can only observe the single pointer it ends up writing, which is
   true whether or not the twelve steps that produced it were right.
2. **Two of its helpers are not exported** — the string-copy at 0x18008a290 and
   the inlined `strlen` loops — so a port would reach them by raw address, which
   is fine in isolation but adds unverified surface to a function that already
   has none.
3. **It is on the boot path and its failure mode is silent.** A wrong branch
   here loads the wrong mod list, and the symptom surfaces later as missing or
   stale content, not as a crash. WAVE8.md is explicit that a hold-back is a
   good outcome and a wrong port that reports clean is the bad one; that is the
   whole of the argument.

The cost is stated rather than hidden: `fn_UpdateLoop_LoadResources` keeps going
to the DLL at its one call site, and `Loader::Run` (the DLL's own thread body)
calls it internally anyway, where registering would not have reached it. A
future lane that wants it should start from `Engine::Loader::Run` (0x1800dbfc0),
which is the only caller that matters, and should expect to port
`Modification::SetupFileSystem` alongside it.

### `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (0x1801fe7b8, ord 4794) — DATA, not registered

This is not a function. It is the DLL's **one static `Engine::Loader`
instance**, 0x38 bytes, constructed during the DLL's own startup: a
`System::Thread` (four Win32 HANDLEs, a real OS thread already created and
parked on `hStart`) plus `_loop` at +0x30.

Registering it — `RegisterRuntimeAddr(0x1801fe7b8, &ourLoader)` — would hand
every caller a **different object from the one the DLL's loader thread was
created on and waits on**. The thread would sleep on one pair of events forever
while `Loader::Load` signalled another, and the loading screen would never
advance. There is nothing to "port" about a static object; what a port can own
is the code that touches it, and `Loader::Load` above does.

So `loops.cpp` **resolves** the symbol by name (in `DialogLoaderLoopT::Startup`,
mirroring retail's `LEA RCX,[0x1801fe7b8]` — note it is a LEA, not a load:
`_loader` *is* the object, not a pointer to one) and never registers it. That is
the finished state, not an omission. `engine_funcs.cpp` already resolves the
same address as `g_pLoader` and is unaffected.

---

## 8. What the integrator must do

1. **Create `runtime/src/engine/rt_engine_port.cpp`** (this lane does not own it,
   by instruction) with the `RtVideoPortInit` shape:
   ```c
   extern int RtEngineLoopsRegister();   // runtime/src/engine/loops.cpp  (lane A)
   extern int RtEngineCoreRegister();    // runtime/src/engine/engine.cpp (lane B)

   void RtEnginePortInit() {
       if (!RtWaveEnabled("BANISHED_RT_ENGINE")) { … return; }
       int n = 0;
       if (RtWaveEnabled("BANISHED_RT_ENGINELOOP")) n += RtEngineLoopsRegister();
       if (RtWaveEnabled("BANISHED_RT_ENGINECORE")) n += RtEngineCoreRegister();
       printf("[RtPort] Engine: %d exports registered\n", n);
   }
   ```
   Lane A's entry point is exactly `int RtEngineLoopsRegister();`, non-static,
   returning 12.
2. **Declare `void RtEnginePortInit();` in `runtime/funcs/rt_port.h`** and call
   it from `RuntimePortInit()` in `standalone/resolve_standalone.cpp`. As of this
   writing rt_port.h has the Wave 8 block with `RtAudioPortInit`,
   `RtApplicationPortInit` and `RtUtilityPortInit` but **not yet
   `RtEnginePortInit`**.
3. **No Makefile change is needed.** `RUNTIME_SRC_SRCS` is
   `$(wildcard runtime/src/*/*.cpp)` and the per-directory rule is generated from
   it, so `runtime/src/engine/loops.cpp` is picked up automatically.
4. **Take lane A and lane B's headers together.** `runtime/src/engine/loops.cpp`
   now `#include`s `runtime/types/engine/MainLoop.h` (lane B) for
   `MainLoop::_currentLoop`. Both headers are in `namespace RtEngine`; neither
   compiles the other's job.
5. **Order matters and is already right** — `RuntimePortInit()` at
   `standalone/main.cpp:1614` runs before `AddressRegistryInit` and
   `EngineFuncsInit`. If that ever moves, every function in §3 goes dead
   silently.
6. **On the diff run, read the `calls=` column, not only `divergences=`** (§3).
   Twelve sites should show non-zero calls on `BANISHED_AUTOSTART=1`.
7. **Kill switch**: `BANISHED_RT_ENGINELOOP=0` for this lane,
   `BANISHED_RT_ENGINE=0` for the namespace.

### What another lane must know

* **Lane B**: this lane reads `MainLoop::_singleton` (`?_singleton@MainLoop@Engine@@1PEAV12@EA`,
  0x1801fe750) and exactly one field of `MainLoop`, **+0x20 = the current
  loop**, by displacement rather than through `MainLoop.h`, which lane B owns.
  Two more offsets this lane established that lane B will want:
  **MainLoop +0x28 is the PENDING next loop** (`LoaderLoop::Exit` 0x1800dc130
  and `LoaderLoop::Update` 0x1800dc2a0 both write `_singleton[+0x28]`), and
  `?_singleton@MainLoop@Engine@@1PEAV12@EA` holds a *pointer to* a MainLoop,
  where `?_loader@LoaderLoop@Engine@@1VLoader@2@A` holds the object itself.
  Lane B's `MainLoop.h` has since landed and names them `_currentLoop` and
  `_nextLoop` at exactly those offsets — derived independently, agreeing.
* **RESOLVED — the engine family is now `namespace RtEngine` throughout.** Lane
  B's headers originally declared `MainLoopAddr`, `struct MainLoop`, `struct
  Engine` and a forward `struct UpdateLoop` at **global scope**, which meant (a)
  their `MainLoop::_currentLoop` pointed at a different, incomplete type from
  this lane's `RtEngine::UpdateLoop` — a mismatch that produced **no error and
  no warning**, only an inability to assign — and (b) a hard clash with the
  hook-era `engine/types/MainLoop.h`, reproduced as
  `runtime/types/engine/MainLoop.h:121: error: redefinition of 'constexpr const
  uintptr_t MainLoopAddr::Singleton'` (previously at `engine/types/MainLoop.h:44`;
  `-Iengine/types` precedes `-Iruntime/types`). Lane B has moved both headers
  into `RtEngine`. **`loops.cpp` now includes `runtime/types/engine/MainLoop.h`
  and reads `MainLoopSingleton()->_currentLoop`** instead of a hand-written
  `+ 0x20`, so the two lanes' independent derivations of that offset are checked
  by the compiler rather than by two comments agreeing. This is a build
  dependency on lane B's header — the integrator must take both or neither.
* **Lane B**: the six `0x18013e2xx` rows in a `get_xrefs_to` on
  `??1UpdateLoop@` are `Unwind@` cleanup funclets, not ordinary callers — worth
  filtering those out of the `MainLoop` sweep too.
* **Lane B**: `Engine::MainLoop::OnVideoError` really does span
  0x1800dc5e0 … 0x1800dcd12 (that is how §6.1's defect was identified), which
  makes it one of the largest bodies in the namespace.
* **Any lane touching `Object::ExternalFactory`**: +0x04 is now
  `_cleanBitMask`, not `_pad04` (§6.2).
* **Any lane touching `System::Thread` subclasses**: `Engine::Loader` is one,
  and the inlined `ResetEvent(hFinished) / SetEvent(hStart)` pair is
  `Thread::Start`. Recognising that idiom keeps `<windows.h>` out of a
  platform-neutral file.


---

## 9. One collision closed on this lane's side

Lane B found that a TU including this lane's `runtime/types/engine/UpdateLoop.h`
together with anything from `engine/types/` died with `redefinition of 'struct
Time'`: **`struct Time` is defined at global scope in BOTH
`engine/types/common.h:12` and `runtime/types/system/Value.h:65`** (Wave 2), and
this header was including Value.h.

The duplicate itself is pre-existing and is not this lane's to fix — it belongs
to whoever owns `Value.h`, and it is still real:

```
$ printf '#include "common.h"
#include "system/Value.h"
int main(){}
' > t.cpp
$ g++ -std=c++17 -Iengine/types -Iruntime/types -c t.cpp
runtime/types/system/Value.h:65:8: error: redefinition of 'struct Time'
```

But this header did not need it. `Time` appears in `UpdateLoop.h` **only inside
comments**, and `LoaderLoop::_delayTicks` is declared as the `int64_t` it
actually is. The include was dead weight that made this header a *carrier* for
someone else's duplicate. It is gone, with a comment saying why it must not come
back; `loops.cpp`, which genuinely uses `Time`, includes `Value.h` itself.

Verified after the change: `engine/types/common.h` + `engine/types/Component.h` +
`runtime/types/engine/UpdateLoop.h` in one TU compiles clean.

**Two live vectors remain, and they are NOT fixed the same way.** This lane's
fix worked because the include was gratuitous. That is not true elsewhere:

```
$ printf '#include "common.h"
#include "object/Internal.h"
int main(){}
' > v.cpp
$ g++ -std=c++17 -Iengine/types -Iruntime/types -c v.cpp
runtime/types/object/../system/Value.h:65:8: error: redefinition of 'struct Time'
```

`runtime/types/object/Internal.h:37` includes `Value.h` for `::Random`, which it
genuinely embeds (0x9c4 bytes), and `runtime/types/achievement/AchievementManager.h:28`
inherits the problem through it. **Deleting those includes is not an option** —
the fix there is to namespace `Value.h` (or the exe tree's `common.h` `Time`),
which is a change to files no Wave 8 lane owns. So "closed" in this section means
closed *as a property of this lane's header*, not closed in general.

**How wide it is, measured — not "plausibly".** Lane A first guessed that lanes C
and E were probably exposed; lane B swept it and the guess was wrong, so here is
the count instead. The `Time`-*defining* side is wider than "the exe tree": five
runtime headers pull `engine/types/common.h` in for `Vector2/3/4` —
`math/Math.h:30`, `volume/Volume.h:23`, `graphics/Geometry.h:47`,
`graphics/Mesh.h:25`, `picking/Scene.h:29` — so the clash needs no exe-tree file
at all:

```
$ printf '#include "math/Math.h"
#include "object/Internal.h"
int main(){}
' > w.cpp
$ g++ -std=c++17 -I. -Iruntime/types -c w.cpp
runtime/types/object/../system/Value.h:65:8: error: redefinition of 'struct Time'
```

But a `-fsyntax-only` sweep of **every** `runtime/src/*/*.cpp` finds **0** files
that reach both sides today (verified independently by lanes A and B). So the
honest statement is neither "latent, no trigger" nor "lanes C and E hit it":
**reachable through two ordinary headers, hit by nothing today, and the next
person to add a routine include to the wrong file gets a confusing error in code
they did not touch.** One grep at integration, not a blocker, and the fix belongs
to whoever owns `Value.h`.

The general point is worth keeping, because Wave 8 is the first wave where the
runtime tree and the exe tree meet in one translation unit: **a header that
includes something it only mentions in comments is a collision waiting for the
first consumer that spans both trees** — and where the include is real, the
duplicate type underneath it has to be fixed instead.
