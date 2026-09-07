# Wave 7 — Lane C — `UI::UserInterface`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/userinterface.cpp`, `runtime/types/ui/UserInterface.h`, this
document, plus the three name corrections in `engine/funcs/engine_funcs.{h,cpp}`
that this lane's export query disproved.

**Result: 8 of 19 distinct functions ported, 11 held back with written reasons.**
`RtUiManagerRegister()` returns **8**. `-fsyntax-only` with the build's exact
command line is **warning-clean** in all three configurations
(`-Wall -Wextra`, `-O3 -DNDEBUG`, `-O0 -g -D_DEBUG`).
`tools/export_name_audit.py` went from 8 phantoms to **5, with none in this lane**.
`tools/runtime_census.py` reports **UI 1625 / 143 called / 8 ported**.

The lane's scope list is 22 rows, but three of them are phantom spellings of
rows already in the list (§4), so there are **19 distinct functions** and 19 is
the achievable denominator.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a *prediction of
what the oracle will report*, not a measurement — see §7.

---

## 1. The function table

Retail addresses and ordinals were read out of the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with pefile **before any body was
written** (`reference_resolveexport_phantom_names.md`).

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0UserInterface@UI@@QEAA@XZ` | 0x180120de0 | 471 | **yes** | twin | `UI::UserInterface::UserInterface` |
| `??1UserInterface@UI@@QEAA@XZ` | 0x180120b90 | 705 | no | — | — |
| `?AddDialog@…QEAAXV?$InternalPtr@VDialogInstance…` | 0x180120ee0 | 1546 | no | — | — |
| `?BringDialogToFront@…QEAAXAEAV…` | **not exported** | — | n/a | — | phantom of the row below |
| `?BringDialogToFront@…QEAAXV…` | 0x180120440 | 1672 | no | — | — |
| `?CloseDialog@…QEAAXAEAV…` | **not exported** | — | n/a | — | phantom of the row below |
| `?CloseDialog@…QEAAXV…` | 0x180120f20 | 1725 | no | — | — |
| `?Draw@…QEAAXAEAVContext@Video@@@Z` | 0x180121890 | 2272 | no | — | — |
| `?EnableCursor@…QEAAX_N@Z` | 0x180122580 | 2293 | no | — | — |
| `?EndSpriteCapture@…QEAAXXZ` | 0x18011f4a0 | 2316 | **yes** | replay | `UI::UserInterface::EndSpriteCapture` |
| `?GetAddDialogCount@…QEBAHXZ` | 0x180031160 | 2441 | **yes** | replay | `UI::UserInterface::GetAddDialogCount` |
| `?GetFirstContainedDialog@…QEAA…` | **not exported** | — | n/a | — | phantom of the row below |
| `?GetFirstContainedDialog@…QEBA…HH@Z` | 0x18011fb00 | 2669 | **yes** | replay | `UI::UserInterface::GetFirstContainedDialog` |
| `?GetFirstDialog@…QEAA?AUIterator@…` | 0x180031150 | 2670 | **yes** | replay | `UI::UserInterface::GetFirstDialog` |
| `?PausesGame@…QEBA_NXZ` | 0x18011fa10 | 3317 | **yes** | replay | `UI::UserInterface::PausesGame` |
| `?Play@…QEAAXH@Z` | 0x1801223d0 | 3418 | no | — | — |
| `?SaveAllSettings@…QEAAXXZ` | 0x1801204b0 | 3608 | no | — | — |
| `?SetCursor@…QEAAXV?$ExternalPtr@VCursor@UI@@…` | 0x1801225f0 | 3865 | no | — | — |
| `?SetFullscreenFade@…QEAAX_NI0@Z` | 0x18011f500 | 3887 | **yes** | replay | `UI::UserInterface::SetFullscreenFade` |
| `?SetFullscreenFadeSprite@…QEAAXVSprite@2@@Z` | 0x18011f560 | 3888 | **yes** | replay | `UI::UserInterface::SetFullscreenFadeSprite` |
| `?ShowDialog@…QEAAHV?$InternalPtr@VDialogInstance…` | 0x18011fd10 | 4034 | no | — | — |
| `?Update@…QEAAHAEBVTime@System@@@Z` | 0x1801213c0 | 4511 | no | — | — |

Every one of the 8 is registered **twice**, under its mangled name and under
its retail address. That is not defensive housekeeping in this class — see §4,
point (b).

---

## 2. The vtable verdict — `UserInterface` is NOT polymorphic

WAVE7.md fact 2 asked for this to be **verified from the ctor's first store**,
not inferred from the absent `??_7UserInterface@UI@@6B@` export. Verified:

```
180120de0  MOV qword ptr [RSP+0x8],RCX
180120de5  SUB RSP,0x18
180120df1  XOR EAX,EAX
180120df3  MOV qword ptr [RCX],RAX        ; _dialogs._head = 0   <-- first store
180120df6  MOV qword ptr [RCX+0x8],RAX    ; _dialogs._tail = 0
…
180120ed2  MOV RAX,RCX                    ; returns `this`
```

The first store is the dialog list's head pointer. No vtable is stamped, and
every remaining store in the 42-store body is a zero or a literal constant.
Corroborating: of the 52 `UserInterface` symbols in the export table, **none is
`UEAA`** — they are all `QEAA` (public non-const), `QEBA` (public const),
`AEAA` (private, `CleanRemovedDialogs`) or `CA` (static, the two
`*StandAloneUI` callbacks). There is no virtual member to have a vtable for.

So this is the `Video::Context` shape from Wave 6 lane A, and PLAN §4.3 risk 3
(vtable ownership) does not arise: our port and the DLL's own code operate on
one live object with no dispatch ambiguity, as long as the layout is faithful.

---

## 3. The detour verdict — not needed here, and this lane did not walk up to it

PLAN §4.2's closing note says the first wave that ports a **stateful** class the
DLL drives internally will be the detour mechanism's first real user, and the
detour count is still 0 across all 4,632 functions. WAVE7.md named this lane the
most likely candidate.

`UserInterface` genuinely is that class. `Engine::DialogLoaderLoopT` (0x1800db128)
and `DialogLoopT` (0x1800db43f) construct one; `ShowDialog` (0x18011fd89)
constructs a nested one; `Engine::MainLoop` pumps `Update` and `Draw` on it every
frame. Registering a port redirects our callers only, so for anything ported
there really are two implementations on one object.

**No detour is installed, and none is needed for what this lane ported** — for
four reasons, each checked:

1. The class is not polymorphic (§2).
2. All eight ported bodies are **leaves**: five are pure reads, three write only
   scalar fields of `this`. None allocates, frees, links, unlinks, calls another
   ported body, or touches a global.
3. Everything that reshapes the object — the dtor, `AddDialog`, `CloseDialog`,
   `BringDialogToFront`, `Update`, `Draw`, `ShowDialog` — is held back, so the
   DLL stays the sole owner of the object's structure.
4. The replay oracle asserts on every diffed call that our body leaves the whole
   0x128-byte object byte-identical to what the DLL's body would have left.

**State the limit honestly:** this is *not* a finding that `UserInterface` needs
no detour in general. The day someone ports `Update` or `AddDialog`, the DLL's
`MainLoop` will still be calling the DLL's `Update` on an object our mutator
reshaped, and that is exactly the question the detour exists to answer. This
lane deliberately stopped short of that line rather than installing an untested
mechanism to get past it.

---

## 4. The three phantom names — resolved, and they are NOT dead code

WAVE7.md assigned three of Wave 7's six phantom mangled names to this lane and
expected the Wave 6 `Geometry::Attach`/`Detach` shape: a silent NULL behind an
`if (ptr)` guard, i.e. a live functional gap where the behaviour has simply
never happened. **That is not what these are, and the difference matters.**

| Phantom we resolved | Falls back to | Which is |
|---|---|---|
| `?CloseDialog@…QEAAXAEAV…` | `ResolveAddr(Runtime, 0x180120F20)` | `?CloseDialog@…QEAAXV…`, ord 1725 |
| `?BringDialogToFront@…QEAAXAEAV…` | `ResolveAddr(Runtime, 0x180120440)` | `?BringDialogToFront@…QEAAXV…`, ord 1672 |
| `?GetFirstContainedDialog@…QEAA…` | `ResolveAddr(Runtime, 0x18011FB00)` | `?GetFirstContainedDialog@…QEBA…`, ord 2669 |

All three call sites (`engine_funcs.cpp:1603`, `:3252`, `:2905`) have an
explicit address fallback, and all three fall back onto **the right function**.
The ABI is identical in every case:

* `AEAV?$InternalPtr@…` (by reference) and `V?$InternalPtr@…` (by value) are the
  **same machine calling sequence**. `InternalPtr<T>` is an 8-byte class with a
  non-trivial copy constructor, so MSVC x64 passes it **by address** in both
  spellings — which the decompilations confirm directly: every callee
  dereferences RDX (`AddDialog` does `*(void**)param_2`, `CloseDialog` does
  `lVar2 = *param_2`). The only real difference is whose copy the callee gets,
  and none of these three callees *writes through* the slot, so handing over the
  live pointer instead of retail's temporary is behaviourally identical.
* `QEAA` versus `QEBA` is const-ness, which has no ABI at all.

**So the answer to "what has therefore never happened" is: nothing.** No
player-visible gap. These are naming defects, not functional ones, and the
difference from Wave 6's `Geometry::Attach` is precisely that `ResolveExport`
had no address fallback there and does here.

Two consequences that do matter, and both are why this was still worth fixing at
the source rather than noting in a doc:

**(a) It makes the export-name audit honest.** A phantom masked by an address
fallback is the case `reference_resolveexport_phantom_names.md` warns is
invisible: nothing is null, nothing crashes, and the wrong name sits in the tree
being believed. `tools/export_name_audit.py` now reports 5 phantoms instead of
8, with none in this lane.

**(b) It changes what a PORT of one of these must do.** Suppose someone ports
`CloseDialog` and registers it under the DLL's real (by-value) name only. The
call site asks for the phantom by-reference name, `ResolveExport` returns NULL
as it always did, the address fallback finds the DLL, and **the port is dead at
its only call site while reporting `calls=0 divergences=0`** — which reads as a
pass (`reference_uncalled_port_is_not_verified.md`; a wrong `Quaternion::operator*`
survived all of Wave 1 that way). Registering under **both** the name and the
retail address, which this file does for all eight ports, is what closes that
hole, and `ResolveAddr(Module::Runtime, …)` consults the runtime address
registry first (`standalone/resolve_standalone.cpp:325`) so the address door
works.

Fixed at the source, per PLAN §6 step 8: `engine_funcs.cpp` now resolves the
names the DLL actually exports (address fallbacks kept), and the three
`fn_UI_*` comments in `engine_funcs.h` now say by-value / const and record what
they used to claim.

Side effect worth flagging to the wave lead: `tools/runtime_census.py`'s UI
`called` count moved from **147 to 143**. Collapsing three phantom/twin pairs
into three single rows accounts for 3 of that; the fourth is presumably lane D's
`?SetText@EditElement@…` fix, which had already left the audit when this lane
checked. **WAVE7.md's scope table and denominator want updating.**

---

## 5. Layout recovered — `runtime/types/ui/UserInterface.h`

`sizeof(UI::UserInterface) == 0x128`, alignment 4. **Proven, not inferred**: it
is the literal size and alignment argument in `ShowDialog`'s
`System::MemoryAllocator::Allocate(_singleton, 0x128, 4)` (0x18011fd10) — the
nested modal UI it builds for itself. Independently, our own tree already had
the same pair: `code/Game/Game/gameupdateloop_load.cpp:1065` does
`AllocSub(0x128, 4)` before calling the ctor. The ctor's last store lands at
+0x120..+0x127, one qword short of the end.

Every offset the lane uses is `static_assert`ed. Ten have a one-line exported
accessor, which is the strongest kind of evidence because it gives the offset
*and* the field's real name:

| off | field | proven by | retail |
|---|---|---|---|
| 0x000 | `_dialogs` (`LinkedList<InternalPtr<DialogInstance>>`) | `CleanRemovedDialogs` unlink | 0x18011f790 |
| 0x000 | `_dialogs._head` | `GetFirstDialog` = `*(this+0)` | 0x180031150 |
| 0x008 | `_dialogs._tail` | `GetFirstContainedDialog` walks back from it | 0x18011fb00 |
| 0x010 | `_dialogs._count` | `++`/`--` around every link edit | 0x18011f790 |
| 0x018 | `_removedDialogs` (`Array`) | dtor + `CleanRemovedDialogs` iterate with count at +0x20 | 0x180120b90 |
| 0x028 | `_addDialogs` (`Array`) | `AddDialog` pushes to +0x28 | 0x180120ee0 |
| 0x030 | `_addDialogs._size` | `GetAddDialogCount` = `*(this+0x30)` | 0x180031160 |
| 0x038 | `_toolTipOwner` | `BeginToolTip` stores the owning dialog | 0x180120860 |
| 0x040 | `_toolTipDialog` | `BeginToolTip` stores the new `ToolTipDialog` | 0x180120860 |
| 0x048 | `_events` (`Array<Event>`, stride 0x30) | `CleanRemovedDialogs` compacts it at 0x30 | 0x18011f790 |
| 0x058 / 0x05c | `_mouseDownX` / `_mouseDownY` | `Update`, on button-down | 0x1801213c0 |
| 0x060 | `_dragDropDialog` | `Update` / `CloseDialog` end-drag | 0x1801213c0 |
| 0x068 | `_dragDropElement` | `Update` destroys it via vslot 0 | 0x1801213c0 |
| 0x070 | `_dragDropId` | `DragDropId` = `*(this+0x70)` | 0x180031040 |
| 0x078 | `_dragDropData` | `DragDropData` = `*(this+0x78)` | 0x180031030 |
| 0x090 | `_audioScene` | `SetAudioScene` | 0x180031050 |
| 0x098 | `_sounds[7]` | `SetSound` writes `+0x98 + i*8` | 0x1800310d0 |
| 0x0d0 | `_cursor` | `SetCursor` | 0x1801225f0 |
| 0x0d8 | `_cursorEnabled` | `EnableCursor` | 0x180122580 |
| 0x0d9 | `_mouseCaptured` | `WasMouseCaptured` = `this[0xd9]` | 0x180031060 |
| 0x0dc | `_uiScale` | `SetUIScale` = `scale / 100.0f` | 0x180031070 |
| 0x0e0 | `_fadeSprite` (0x10) | `SetFullscreenFadeSprite` copies both qwords | 0x18011f560 |
| 0x0f0 | `_fadeVelocity` | `SetFullscreenFade`; `IsFading` is `+0xf0 != 0.0f` | 0x18011f500 / 0x180031010 |
| 0x0f4 | `_fadeAmount` | `SetFullscreenFade` immediate branch | 0x18011f500 |
| 0x0f8 | `_fadeColor` | `SetFullscreenFade` 2nd arg | 0x18011f500 |
| 0x0fc | `_clipRegion` (l,t,r,b) | `GetClipRegion` = `(this+0xfc)` | 0x180031000 |
| 0x110 | `_spriteCapture` | `EndSpriteCapture` = 0; `Draw` passes it on | 0x18011f4a0 |
| 0x118 | `_captureViewport` (0x10) | `Draw` passes `(Viewport*)(this+0x118)` | 0x180121890 |

**Independent cross-check from the exe side**, written months before this lane
existed: `code/Game/Dialog/optionsdialog.cpp`'s `ApplyKeyMapToUI_Impl` (exe
0x140099260) writes UI +0xA8/+0xB0/+0xB8/+0xC0/+0xC8 as five sound effects and
UI +0xDC as `settings[0] / 100.0f`. Those are sound slots 2..6 of the
`_sounds[7]` array at +0x98, and `_uiScale`. Both agree with the Runtime-side
accessors, from a completely separate source.

Two containers, not hand-rolled triples (PLAN §6 step 3): `Collection::LinkedList`
and `Collection::Array` from `runtime/types/collection/Collection.h`, with
`sizeof(T) == 8` so the node offsets (`prev` at +0x08, `next` at +0x10) fall out
of `align8(sizeof T) + 0x10` (PLAN §4.3 risk 1) — which is also exactly how Wave
2 pinned `LinkedList` in the first place, using this very class.

`Event` (0x30) and `_captureViewport` (0x10) have **proven sizes and
deliberately opaque interiors**: nothing this lane ports reads a field of
either, and inventing a layout we have not proven is how a shared struct
corrupts silently.

### The 25 bytes the ctor does not write

`0x014..0x017`, `0x074..0x077`, `0x089..0x08f`, `0x0da..0x0db`, `0x0ec..0x0ef`,
`0x10c..0x10f`. Two of those gaps exist because retail's store is *narrower than
the field group*: the store at +0xd8 is a **WORD** (it clears `_cursorEnabled`
and `_mouseCaptured` together and stops), and `_clipRegion`'s four ints are
covered by two **QWORD** stores at +0xfc and +0x104 which stop at +0x10c. The
port reproduces the store **widths**, not just the values, because the ctor's
oracle compares the bytes in between.

---

## 6. Two oracles, argued per function

WAVE7.md §5: "lanes must argue their oracle per function", and replay is invalid
for anything that mutates the dialog list, draws, updates, or shows a modal.

### (a) Replay — the seven non-constructors

Snapshot the live 0x128-byte object into a private scratch copy **before** our
body runs; run our body on the real object; run the DLL's body on the copy;
`memcmp` all 0x128 bytes and compare the return value. `RtDiffReportBytes` names
the first differing offset and §5's table turns that offset into a field name.

Sound for these seven and no others, because each is a **leaf that touches only
`this`**:

| function | what it reads/writes | why replay is valid |
|---|---|---|
| `GetAddDialogCount` | reads `+0x30` | pure |
| `GetFirstDialog` | reads `+0x00`, writes the caller's retval slot | pure; the retval slot is a private local under diff |
| `PausesGame` | walks the list, reads dialog `+0x20` | pure; no write on either side |
| `GetFirstContainedDialog` | walks the list, reads dialog `+0x20`/`+0x50`, element bounds or a virtual hit test | pure — with one caveat, below |
| `EndSpriteCapture` | writes `+0x110` | writes one scalar of `this` only |
| `SetFullscreenFade` | writes `+0xf0`/`+0xf4`/`+0xf8` | ditto |
| `SetFullscreenFadeSprite` | writes `+0xe0..+0xef` | ditto |

For the four read-only ones the snapshot does double duty: after both have run,
the copy also proves the DLL's body wrote nothing either.

Unwritten padding is **not** a hazard here — both objects start from the same
live snapshot, so bytes neither implementation touches are equal by
construction. (`reference_oracle_must_not_compare_unwritten_bytes.md`: an oracle
that memcmps bytes neither side writes fails 100% of calls and reads like a
catastrophic port defect.)

**The one caveat, stated plainly.** `GetFirstContainedDialog`'s custom hit-test
path calls an `Element` virtual at slot `+0x48` with `(element, x, y, ui)`.
Under a diff that virtual is invoked a **second** time, on the same element with
the same arguments. It answers "do you contain this point" and is `const` in
every widget that overrides it, so the second call is a pure re-evaluation — but
it is a virtual into another lane's code, and if a widget ever makes it stateful
this site is where that surfaces. The alternative was no oracle at all for the
custom-hit-test half of the function.

One shared file-scope scratch buffer rather than a 296-byte stack frame per
call. Safe because the UI is driven from one thread (`Engine::MainLoop`'s) and
all eight ported bodies are leaves, so a replay can never nest. Written down in
the source so the next reader knows what to fix if either premise changes.

### (b) Twin — the constructor

A constructor cannot be replayed against a snapshot of its own object: there is
no "before" state to copy, and running the DLL's ctor over the live object after
ours would overwrite our work with an identical result and prove nothing about
the 25 bytes neither one writes.

So: pre-fill **two** scratch buffers with the **same** poison pattern (0xA5),
run our ctor on one and the DLL's on the other, and compare all 0x128 bytes.
The poison makes the six unwritten ranges compare equal by construction, and
makes any byte one implementation writes and the other does not fail loudly
instead of silently matching a zero. It also catches a store of the wrong
**width** — the +0xd8 WORD and the two ClipRegion QWORDs — which a
zero-initialised comparison would not.

The live object is constructed by our body exactly once, as normal; the twin
comparison happens beside it and touches nothing the caller owns. The port
additionally asserts that the DLL's ctor returns `this` in RAX, because a member
that forgets RAX is invisible to a bytes-only compare and
`code/Game/Game/gameupdateloop_load.cpp:91` types the pointer as returning
`void*`.

---

## 7. The eleven hold-backs

Ordered by how close each came to being ported. A hold-back with a reason is a
better outcome than an unverified registration (WAVE2 allocator verdict: an
unmeasured port is not a port).

### `?ShowDialog@` 0x18011fd10 — a nested `MainLoop::Frame` loop

The wave brief said not to port this blind. Reading it fully is what the
hold-back is based on, not the brief:

`ShowDialog` allocates a **second** `UserInterface` (this is the
`Allocate(_singleton, 0x128, 4)` that pins our `sizeof`), copies the audio scene
and all seven sound slots into it, moves the dialog onto it (setting
`dialog+0x10` to the nested UI and OR-ing flag bit 5 so the outer UI's cleanup
will not release it), pushes it onto the nested `_addDialogs`, calls `Update`
once, registers `DrawStandAloneUI` and `UpdateStandAloneUI` on
`Engine::MainLoop`'s two callback arrays (growing each with the `*3` policy),
and then

```c
do { … walk the nested UI's list for our dialog … }
while (!Engine::MainLoop::Frame(Engine::MainLoop::_singleton, true));
```

pumps whole frames until the dialog leaves the list. It then removes both
callbacks by compacting the arrays in place, destroys and frees the nested UI,
and returns `*(int*)(dialog + 0x24)` — the dialog's result code.

Porting it means owning `MainLoop::Frame` re-entrancy, two callback arrays'
growth and compaction, and the lifetime of a second `UserInterface` — **with no
oracle of any kind**, because running it twice pumps two frame loops. Held back.

**One correction to `WAVE4_LANE_H.md` §2**, now that a port has read the body.
That document says ShowDialog "takes its argument by value and writes through
the slot it is given". The by-value half is right and the caller-side fix built
on it is right and should not change. The "writes through the slot" half is not:
the body only **reads** `*param_2` (four times) and never stores to it. What
retail's caller at exe 0x1400b61e0 is really honouring is MSVC's by-value
convention, in which the **caller** owns and destroys the copy.

### `??1UserInterface@` (dtor) 0x180120b90

Unlinks and frees every list node under the allocator's `CRITICAL_SECTION`,
runs each dialog's deleting destructor through vtable slot 0, and frees the
three arrays. Three separate disqualifications: `EnterCriticalSection` cannot
appear in a non-`_win` file (PLAN §4.1 decision 1); `MemFree` is Wave 2's
allocator, which shipped **opt-in with a NO-GO verdict**; and a destructor runs
once, so there is nothing to compare.

### `?AddDialog@` 0x180120ee0

Four instructions of our own plus `DynArrayPush`, and tempting because it is
tiny. Not replayable: the scratch copy shares the live `_addDialogs._data`
pointer, so the DLL's replay would either push the same dialog a **second** time
into live memory or realloc and free the buffer the live object still points at.
It also calls the dialog's vtable slot `+0x20` (an add-ref), which would fire
twice. A shadow oracle over the post-state is possible and is the obvious next
step for whoever wants it; this lane did not write one it could not run.

### `?CloseDialog@` 0x180120f20

The largest non-`Update` body in the class: recursive over the dialog's own
child list, allocates, frees under the allocator lock, unlinks nodes from
**another object's** list, queues drag-drop end events, and calls
`UpdateEvents`. Not replayable several times over.

### `?BringDialogToFront@` 0x180120440

Removes the node and re-appends it at `_tail`, allocating a fresh node. A replay
moves it twice and leaks a second node.

### `?Draw@` 0x180121890 and `?Update@` 0x1801213c0

The two halves of the class's frame. `Draw` builds a `Graphics::Camera` and a
`SpriteContext`, submits every dialog's geometry plus the fade and the mouse
cursor, and frees four buffers; a replay draws the frame twice. `Update` drains
`_addDialogs` into the list (allocating a node each), frees the array, advances
the fade, calls `DialogInstance::Update` on every dialog, consumes input from
`Input::Device::_singleton`, and ends drag-drop; it is not replayable and not
even idempotent.

### `?SaveAllSettings@` 0x1801204b0

Builds an `IO::MemoryStream` on the stack, walks every dialog with a settings
block, serialises through virtuals, and frees. Allocates, frees, and writes
persisted state.

### `?Play@` 0x1801223d0

Small, pure-looking, and **held back because a replay plays the sound twice,
audibly**. It scans the audio scene's 256-slot event table for a free entry and
writes the sound pointer, a state of 1, a timestamp and a packed
`index*2 | serial<<9`. A shadow oracle (predict the slot, let theirs write it)
is the right approach and needs an `Audio::Scene` layout this lane does not own.

### `?EnableCursor@` 0x180122580 and `?SetCursor@` 0x1801225f0

Both fully reversed, both held back because **the oracle cannot see the risky
half**.

`EnableCursor` writes `_cursorEnabled`, then — unless
`Input::Configuration::_singleton[10]` forces it off — writes
`Application::_singleton[0x10]` and walks a keyed handler tree to fire one
indirect callback with id 0x20. A replay here is *worse than useless*: our body
would have already set the Application flag, so the DLL's replay takes the
early-out and never exercises the tree walk at all. The object compare would
pass while the only thing that can go wrong went untested — which is exactly the
shape `reference_uncalled_port_is_not_verified.md` warns reads as a pass.

`SetCursor` writes `_cursor` and then calls `Video::Device::_singleton` →
`+0x100` → vtable `+0x40` with the cursor sheet's image and hotspot. A replay
issues that backend call a second time per diffed call. Setting the same cursor
twice is very likely idempotent — but "very likely" is not an oracle, and a
graphics-backend call is not the place to find out.

---

## 8. Notable faithfulness details

Things a "tidied" port would silently drop, each preserved and commented in the
source:

- **`SetFullscreenFade`'s third argument is not "reverse".** It is *immediate*:
  when set, the function writes the fade **amount** (+0xf4) directly and leaves
  `_fadeVelocity` alone, so `IsFading()` stays false and `Update` never
  animates. When clear, it writes the **velocity** (+0xf0, ±1.0f) and lets
  `Update` integrate. Two different fields, two different behaviours, one
  boolean.
- **`SetFullscreenFade` writes the colour in both branches, at different points**
  — first in the immediate branch, last in the animated one — and the animated
  branch has the store duplicated once per sub-branch in the disassembly
  (0x18011f534 and 0x18011f546).
- **`SetFullscreenFadeSprite` copies all 16 bytes of the `Sprite`, padding
  included.** Assigning the two named fields would leave +0xec holding whatever
  was there, and the byte compare would trip on padding the DLL propagated and
  we did not.
- **`GetFirstContainedDialog` walks BACKWARD, from `_tail` through `prev`**, so
  the front-most dialog wins the hit test; `PausesGame` and `Draw` walk
  **forward** from `_head`. Getting the direction wrong would silently pick the
  wrong dialog under overlapping windows.
- **Its rectangle test is inclusive on all four sides**, and it pairs +0x68 with
  +0x70 for x and +0x6c with +0x74 for y — left/right are *not* adjacent to
  top/bottom in the `Element` header.
- **`EndSpriteCapture` does not touch the capture viewport at +0x118** and does
  not tell the `SpriteCapture` object anything; `Draw`'s
  `if (capture) EndAddingSprites` is what closes a capture out.
- **The ctor's `_fadeSpriteIndex = -1`** is the engine's empty-sprite value.
  Index 0 is a real sprite
  (`reference_unresolved_lookup_leaves_a_valid_looking_zero.md`).
- **The ctor returns `this` in RAX**, and the twin oracle checks it.

---

## 9. Stale claims this lane disproved

Fixed at their source by this lane (PLAN §6 step 8):

1. **`engine_funcs.cpp:1603 / :2905 / :3252`** — three `ResolveExport` names the
   DLL does not contain. Now the exported spellings; address fallbacks kept.
2. **`engine_funcs.h`** — the `fn_UI_CloseDialog`, `fn_UI_BringDialogToFront`
   and `fn_UI_GetFirstContainedDialog` comments said by-reference /
   non-const. Now corrected, each keeping one line recording the old claim so a
   reader who meets it elsewhere recognises it.

For the wave lead or the owning lane to apply, since this lane may not edit
those files:

3. **`docs/runtime_port/WAVE7.md`** — the phantom table's three lane-C rows
   should say "benign: masked by an address fallback, ABI-identical twin", not
   the `Geometry::Attach` shape. And the scope table's `called` figure of 147
   is now **143**, three of that from this lane's name fixes.
4. **`docs/runtime_port/WAVE4_LANE_H.md` §2** — "ShowDialog … writes through the
   slot it is given". It does not; it only reads the slot. See §7.
5. **`runtime/types/collection/Collection.h`**, `LinkedList::_pad14` comment —
   "present so `sizeof == 0x10`". The struct is head(8) + tail(8) + count(4) +
   pad(4) = **0x18**, which the same comment block states correctly two
   paragraphs earlier and which this lane's `static_assert` now pins. A typo,
   but in the one file PLAN §4.3 ranks the biggest layout risk in the port.
6. **`engine_funcs.h`**, the `fn_ApplyKeyMapToUI` comment "copies +0x58..+0x78,
   +0xA8/+0xB0 to UI" — those are the **KeyMap's** source offsets; the
   destinations are UI +0xA8..+0xC8 and the fade sprite. Ambiguous rather than
   wrong, but it reads as a claim about UI offsets that would contradict §5.

---

## 10. What this lane could NOT verify

Stated plainly, because a prediction is not a measurement:

1. **No run happened.** Per the lane brief, this lane did not build, deploy or
   run anything. `RtUiManagerRegister()` returning 8, the census row, the
   export audit and the three `-fsyntax-only` passes are source-level facts;
   **zero diffed calls have executed.** Until the wave lead runs
   `BANISHED_RT_DIFF=1`, every oracle row in §1 is "expected clean", not
   "clean". The kill switch is `BANISHED_RT_UIMANAGER=0`.
2. **The ctor's twin oracle has never poisoned a buffer.** The 0xA5 fill, the
   double invocation and the RAX check are written against the disassembly, not
   observed. If the DLL's ctor were to read a field before writing it — it does
   not; all 42 stores are constants — the poison would surface that as a
   divergence rather than hide it, which is the right failure direction but is
   still an untested path.
3. **`GetFirstContainedDialog`'s custom hit-test virtual is called twice under a
   diff** (§6a). Believed harmless; not proven.
4. **The `Element` bounds offsets (+0x68/+0x6c/+0x70/+0x74) and the
   `DialogInstance` offsets (+0x10, +0x20, +0x50) are read raw**, from this
   lane's own disassembly of 0x18011fb00 and 0x18011fa10, not from lanes A and
   B's headers. They were then **cross-checked against those headers as they
   stood at the end of this lane's work, and they agree exactly**:
   `runtime/types/ui/Element.h:185-188` names +0x68/+0x6c/+0x70/+0x74
   `_layoutLeft`/`_layoutTop`/`_layoutRight`/`_layoutBottom` (from
   `?GetLayoutLeft@` 0x180035580 and its three siblings), and
   `runtime/types/ui/DialogInstance.h` names +0x10 `_ui`, +0x20 `_flags`,
   +0x50 `_root`. Two independent derivations, same answer. What is *not*
   verified is that those headers are final — both lanes were still working
   when this was checked.
5. **The `Event` (0x30) and `_captureViewport` (0x10) interiors are unmodelled.**
   Their sizes are proven; a divergence landing inside one will report an offset
   that does not resolve to a field name, and the next reader will have to
   reverse `UpdateEvents` (0x18011fc60) or `SpriteContext::Begin`.
6. **Nothing here was measured for performance.** The replay and twin oracles
   are inside `if (rt_diff_on__)` and cost nothing in a normal run; five of the
   eight ports are one or two instructions of real work. The PLAN 5% gate is
   deferred (PLAN §5) and this lane did not touch it.
7. **`?Play@`, `?EnableCursor@` and `?SetCursor@` are reversed but unported.**
   The reversed bodies are described in §7 rather than committed, deliberately:
   a body nobody can run is a comment, and a comment is where this project keeps
   losing time.

---

## 11. Ghidra

Everything in this lane was read with `program="Runtime-x64.dll"` on every call.
`switch_program` was never invoked. No function was renamed, so no other lane's
view of the shared project changed.
