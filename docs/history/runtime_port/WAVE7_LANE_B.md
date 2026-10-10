# Wave 7 — Lane B — `UI::DialogInstance` / `Dialog` / `DialogController` / `ElementDescription`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/dialog.cpp`, `runtime/types/ui/DialogInstance.h`,
`runtime/types/ui/Dialog.h`, `runtime/types/ui/ElementDescription.h`, this document.

**Result: 14 of 28 ported, 10 held back with written reasons, 4 are not portable functions**
(two exported DATA symbols, and the two phantom names of §5, which are not in the DLL at all).
`RtUiDialogRegister()` returns **14**. `-fsyntax-only` with the wave's exact command line, at
both `-O0 -g -D_DEBUG` and `-O3 -DNDEBUG`, is **warning-clean**.
`tools/export_name_audit.py` went from **5 phantoms to 3** — both of this lane's are gone, and
the 3 that remain (`?CheckMod@SaveLoad@`, `?SetSaveName@SaveLoad@`, `?PlayEffect@Scene@Audio@`)
belong to Wave 5 and to Audio, not here.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`, `sa-run`,
`git commit`. **Every runtime number below is a prediction of what the oracle will report,
not a measurement** — see §7.

---

## 1. The function table

Retail addresses and ordinals were read out of the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with a pefile reader **before any body was written**
(`reference_resolveexport_phantom_names.md`). 26 of the 28 names exist; the two that do not are
§5.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@…@Z` | 0x180114870 | 147 | **no** | — | — |
| `??1DialogInstance@UI@@UEAA@XZ` | 0x180114d40 | 574 | **no** | — | — |
| `??_7DialogInstance@UI@@6B@` | 0x180156d70 | 1420 | n/a (data) | — | — |
| `?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z` | **not an export** | — | n/a | — | — |
| `?AddController@…@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z` | 0x180113470 | 1541 | **no** | — | — |
| `?AddController@…@V?$InternalPtr@VElement@UI@@@4@@Z` | 0x1801133b0 | 1542 | **no** | — | — |
| `?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z` | 0x180114c70 | 2346 | yes | replay | `UI::DialogInstance::FindElement` |
| `?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z` | 0x1801140c0 | 2348 | yes | replay | `UI::DialogInstance::FindElementType` |
| `?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z` | 0x180113320 | 2513 | yes | replay | `UI::DialogInstance::GetController` |
| `?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ` | 0x180045580 | 2875 | yes | replay | `UI::DialogInstance::GetRoot` |
| `?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@…@HHHHHPEAX@Z` | 0x180114ed0 | 3274 | yes | shape | `UI::DialogInstance::OnElementEvent[shape]` |
| `?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z` | 0x180115400 | 3286 | yes | shape | `UI::DialogInstance::OnUpdate[shape]` |
| `?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ` | 0x1801153a0 | 3289 | yes | shape | `UI::DialogInstance::OnUpdateLayout[shape]` |
| `?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z` | 0x180114690 | 3882 | yes | replay | `UI::DialogInstance::SetFlagsOnOff` |
| `?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z` | 0x180114a60 | 3997 | **no** | — | — |
| `?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z` | 0x1801137c0 | 4081 | **no** | — | — |
| `?StopTimer@DialogInstance@UI@@QEAAXH@Z` | 0x180113730 | 4111 | yes | replay+array | `UI::DialogInstance::StopTimer` |
| `?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@…@Z` | 0x180112ed0 | 1888 | **no** | — | — |
| `?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ` | **not an export** | — | n/a | — | — |
| `?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z` | 0x1800388b0 | 2956 | yes | replay | `UI::Dialog::GetTemplate` |
| `?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A` | 0x1801faa88 | 4847 | n/a (data) | — | — |
| `??1DialogController@UI@@UEAA@XZ` | 0x1800cd2f0 | 572 | yes | shape | `UI::DialogController::~DialogController[shape]` |
| `?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ` | 0x18003b700 | 2508 | yes | replay | `UI::DialogController::GetContainer` |
| `??0ElementDescription@UI@@QEAA@XZ` | 0x180115a40 | 173 | yes | replay | `UI::ElementDescription::ctor` |
| `??1ElementDescription@UI@@UEAA@XZ` | 0x180116100 | 585 | **no** | — | — |
| `?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | 0x180115ad0 | 2004 | **no** | — | — |
| `?GetFileVersion@ElementDescription@UI@@UEBAHXZ` | 0x180116af0 | 2624 | yes | replay | `UI::ElementDescription::GetFileVersion` |
| `?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z` | 0x180116b00 | 3687 | **no** | — | — |

Every one of the 14 is registered **twice**, under its mangled name and under its retail
address. The address form costs nothing and closes the Wave-5-lane-D failure mode where one live
object ends up split between two implementations because a later call site used the other door.

### The two "n/a" rows are data, not functions

`??_7DialogInstance@UI@@6B@` and `?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A` are exported
DATA symbols. There is nothing to port: our tree consumes the DLL's own vtable and the DLL's own
`BaseTypeInfo`, and the *right* thing for a partial port is exactly that
(`??_7Geometry@Graphics@@6B@`, Wave 6). They are counted in the 28 because `wave7_lanes.txt`
counts them; they are not counted in the 14.

---

## 2. The ten hold-backs

Grouped by the reason, because the reasons repeat. (14 ported + 10 held back + 4 non-functions
= 28.)

### 2a. Eight allocate or free — split ownership, and no oracle can be built

`??0DialogInstance` · `??1DialogInstance` · both `?AddController@` overloads · `?StartTimer@` ·
`?CreateElement@Dialog@` · `?CreateInstance@ElementDescription@` · `??1ElementDescription@`

* **`??0DialogInstance` (0x180114870)** takes the allocator's critical section
  (`MemoryAllocator::_singleton + 0x640`) to build an 0x18-byte `LinkedList` node, links itself
  into its parent's child list, and then runs the whole `AddController` loop over the Dialog's
  controller-config table. Three separate disqualifications: `EnterCriticalSection` is Win32 and
  cannot appear in a non-`_win` file (PLAN §4.1 decision 1); the node it allocates is freed by
  `??1DialogInstance`, which stays in the DLL; and `MemFree` is Wave 2's allocator port, which
  shipped **opt-in with a NO-GO verdict** (`WAVE2_ALLOCATOR.md`).
* **`??1DialogInstance` (0x180114d40)** is worse. It frees both arrays under the same critical
  section and then **inlines `Object::Internal::~Internal`** — the handle-table swap-remove:
  `InternalFactory::_singleton`'s last entry is moved down over this object's slot, the moved
  object's index is rewritten, and the array is shrunk. That is the family
  `reference_internalfactory_swapremove_wild_write.md` and
  `reference_phantom_dtor_mangling_qeaa_vs_ueaa.md` were both written about. Wave 3 ported the
  handle-table mutators and shipped them **opt-in with a NO-GO**. A dialog lane has no business
  being the thing that finally arms them.
* **Both `?AddController@` overloads.** The four-argument one (0x1801133b0) walks
  `Object::Factory::_singleton`'s AVL tree for the type id, calls the factory's vtable +0x20 to
  CONSTRUCT a controller, and pushes it into `_controllers` through the `Array<T>` grow helper
  (`FUN_180076370`, an unnamed DLL internal). The six-argument one (0x180113470) additionally
  builds a stack `ExternalPtr` with its own destructor chain, resolves the insertion Element by
  name, calls the description's vtable +0x40 to build an element, and ORs `0x8000000` up the
  parent chain. Both allocate objects that the DLL's `??1DialogInstance` later frees. Nothing to
  compare, only two heaps' worth of bookkeeping to get wrong.
* **`?StartTimer@` (0x1801137c0)** is short but ends in the same `Array<T>` grow helper
  (`FUN_180076250`) and so allocates. Worth recording while it is fresh, because it looks like a
  bug and is not: the loop that finds an EXISTING timer with the same id rewrites that timer's
  two `Time` fields and then **falls through and appends a second entry anyway**. Retail really
  does accumulate duplicate ids. Do not "fix" that when it lands.
* **`?CreateElement@Dialog@` (0x180112ed0)** and **`?CreateInstance@ElementDescription@`
  (0x180115ad0)** both construct an Element through an `Object::External` vtable slot +0x40 and
  both manipulate `System::String` refcounts (increment, then the decrement-and-`MemFree` idiom)
  on the way. `CreateInstance` also writes eleven fields of a live `UI::Element` — lane A's
  header — and, decisively, would need to run twice to be replayed,
  producing two elements.
* **`??1ElementDescription@` (0x180116100)** releases the `String` at +0x60 and chains to
  `Object::External::~External`. A replay would run the release twice: the refcount would reach
  zero on the first pass and the second would `MemFree` a pointer already freed. There is no
  version of this that is safe to diff.

### 2b. One is not replayable and reaches deep into another lane's class

**`?SetUpperLeft@` (0x180114a60)** is fully reversed but held back for two reasons.

The decisive one is that **it is not replayable**: it calls `Element::AdjustUpperLeft` on every
child of the root and on the root's +0xb0 element, so a second pass would shift the whole
Element tree twice. There is no scratch copy to run the original against, because the state it
mutates is a tree of objects the DialogInstance only points at.

The second is scope. It reads eleven `UI::Element` fields (+0x1c, +0x24, +0x2c, +0x34, +0x64,
+0x68..+0x74, +0x90, +0x98, +0xa0, +0xa8, +0xb0). Lane A owns `runtime/types/ui/Element.h`, and
it had not landed when this lane did its recon — it has since appeared in the tree, but by then
the replay argument had already settled the question, and hand-rolling a competing Element
layout in the interval is exactly what WAVE7.md's cross-lane contract forbids. When it is
landed, it should be landed against lane A's header and with a shadow oracle, not a replay.

It is worth landing later, and the two facts a future porter will want are: it clamps the
requested position against `Video::Device::_singleton`'s +0x04/+0x08 (screen width/height)
scaled by `UserInterface+0xdc` (the UI scale factor), and when `Element+0x34` has bit 0x10 or
0x20 set it walks to the ROOT-most element through +0x98 and re-derives the extent from *that*
element's own UserInterface, not from `this`.

### 2c. One is registered on the exe side too, and has no cheap oracle

**`?Serialize@ElementDescription@` (0x180116b00).** Two independent reasons, either sufficient.

1. **No oracle.** It constructs an `IO::Serializer` through the stream's ReaderWriter factory
   (vtable +0x18 on read, +0x20 on write), then reads or writes fourteen fields and four
   sub-objects. Wave 5 held four ports back for exactly this: **constructing an `IO::Serializer`
   emits a 12-byte stream header**, so running it twice desynchronises the stream. It cannot be
   replayed, and a shadow replica would have to model the whole `IO::Serializer` state machine.
2. **Blast radius.** `standalone/address_registry.cpp` binds exe PE-import slot `0x1401518c8` to
   this export BY NAME, and `RuntimePortInit()` runs before `AddressRegistryInit()`
   (`standalone/main.cpp:1614` / `:1626`), so registering the name would re-point the exe's own
   `.rdata` slot at our body. That puts an unverifiable port on the UIDescription parse path of
   every `.rsc` in the game — the single worst neighbourhood in this codebase for it
   (`reference_createplaced_placebitarray_zeroed_by_reparse.md`, and commit `a12037d`, where a
   `Serialize` that re-ran on a second `FindOrLoad` and pre-zeroed its pools produced an empty
   world).

An honest hold-back beats an unverifiable registration (WAVE2 allocator verdict: an unmeasured
port is not a port). For whoever lands it, the body is straightforward and the retail Encoding
vtable offsets it uses are `+0x98` (uint) and `+0xa0` (int) — which independently confirm
`runtime/types/object/Internal.h`'s `kEncSerializeUint` / `kEncSerializeInt`, and which are NOT
the numbers the profile build uses (`reference_encoding_vtable_offsets_differ_between_builds.md`).
Field order: ints at +0x20, +0x24, +0x28, +0x2c, +0x30, +0x34; uints at +0x40, +0x44, +0x3c;
int at +0x38; then four sub-serializers at +0x48, +0x50, +0x58 and `String::Serialize` on +0x60.
It returns `true` unconditionally.

### What is NOT a hold-back reason here

None of the ten is held back for "we ran out of time". Each row above names a mechanism.
Conversely, nothing was registered merely because it was easy: `?GetFileVersion@` is a two-
instruction `return 4` and it is registered because it is fully oracled AND it has live callers
(exe dispatch slot `0x1401518c0`), not because it was cheap.

---

## 3. Layouts recovered

Three headers, `static_assert` on every offset, and every offset cited to the function it came
from. Verified numerically as well as by assertion — a throwaway program printing `offsetof`
for all of them reproduced the retail displacements exactly.

### `runtime/types/ui/DialogInstance.h`

`sizeof` **is not claimed**. `UI::UserInterface::BeginToolTip` (0x180120860) does
`ToolTipDialog::operator new(0xc0)` and then runs the base ctor, so 0xc0 is an upper bound on
the base; the last field the base ctor writes is the int32 at +0xb0. The struct is declared at
0xb8 (the ctor-initialised extent, rounded) and says so out loud rather than pretending 0xc0 is
`sizeof(DialogInstance)`.

Highlights (full table in the header):

| off | field | proven by |
|---|---|---|
| 0x00 | `Object::Internal` | ctor calls `Internal::Internal` then stamps `??_7DialogInstance` |
| 0x20 | `_flags` | `?SetFlagsOnOff@` is `(f\|on) & ~off`, seeded from `Dialog+0x40` |
| 0x38 | `_children` `LinkedList<InternalPtr<DialogInstance>>` | ctor's node insert |
| 0x50 | `_root` | `?GetRoot@` is `MOV RAX,[RCX+0x50]` |
| 0x68 | `_timers` `Array<DialogTimer>` (stride 0x18) | `?StartTimer@` / `?StopTimer@` |
| 0x78 | `_controllers` `Array<DialogController*>` | five methods iterate exactly this pair |
| 0x88/0x8c | `_upperLeftX/Y` | `?SetUpperLeft@` delta base |

**The child list is a PREPEND and the naive reading gets it backwards.** The ctor's insert is
equally consistent with `{tail@0x38, head@0x40}` + append and with `{head@0x38, tail@0x40}` +
prepend. The tie-break is `?GetFirstDialog@UserInterface@UI@@…` (0x180031150), which is literally
`MOV RAX,[RCX]; MOV [RDX],RAX; RET` — the FIRST element lives at the list's +0x00. So +0x38 is
the head, node+0x08 is `prev` and node+0x10 is `next`. **This confirms
`runtime/types/collection/Collection.h`'s existing `LinkedList` layout, including its otherwise-
unpinned `_pad14`**, which turns out to be the +0x4c hole the DialogInstance ctor never writes.
Recorded because a first pass through the ctor alone produced the opposite (wrong) answer.

**The base ctor leaves four holes uninitialised** — +0x4c..0x4f, +0x64..0x67, +0x99..0x9f,
+0xb4 up. They are named in the header because any future oracle that builds a DialogInstance
from FILL rather than from a copy must skip them
(`reference_oracle_must_not_compare_unwritten_bytes.md`). This lane's replay is not affected;
see §4.

`UI::DialogController` lives in the same header (it has no independent existence — it is
created, owned, iterated and destroyed by `DialogInstance`, and every field of it was pinned
from a `DialogInstance` method). Its `sizeof` is deliberately not asserted: every real
controller is a derived class the factory allocates, and nothing here allocates one.

### `runtime/types/ui/ElementDescription.h`

`sizeof == 0x68`, proven: the ctor's last store is a qword at +0x60 and the dtor touches nothing
above. Base chain `Object::Base -> Object::External -> UI::ElementDescription`, read from the
ctor's three successive vtable stores, and **all three vtables are exported**
(0x180154788 / 0x1801547a0 / 0x1801561c0) — so the ported ctor stamps the DLL's own tables.

Three functions pin the fourteen members and they agree field for field: the ctor gives the
offsets, `?Serialize@` gives the TYPES (the Encoding vtable slot names them outright), and
`?CreateInstance@` gives the MEANINGS (what each one is copied into on the live Element).

Two retail behaviours recorded because they read like slips and are not: `_flags` (+0x44) is
zero after construction and `CreateInstance` **ORs** it into the element rather than assigning,
so a description can never clear a flag the element already has; and `_id` (+0x3c) is seeded to
`-1`, so **0 is a real id, not "unset"**
(`reference_unresolved_lookup_leaves_a_valid_looking_zero.md`).

### `runtime/types/ui/Dialog.h`

Deliberately partial, and it says so. This lane ports one Dialog method, so the header records
the ten offsets four decompiled bodies actually pin and writes "unknown" everywhere else rather
than inventing names. **No `sizeof` assert** — nothing here allocates a Dialog and nothing reads
past +0x7f, so any number would be a guess dressed as a fact. The +0x00..+0x1f base is left
opaque: the pattern matches `Object::External` exactly (0x20 bytes, first Dialog-owned field at
+0x20) but no body in this lane touches it, so the header says "very likely" and not "is".

Two element strides ARE pinned and are worth having: `_controllerConfigs` (+0x48) is
`{uint32 typeId, ExternalPtr config, ExternalPtr elementDesc, String name}` at stride **0x20**,
from the ctor's loop; `_namedTemplates` (+0x58) is `{External*, String}` at stride **0x10**,
from `?CreateElement@`'s linear scan.

---

## 4. The oracle, argued per function

WAVE7.md fact 5 is right that almost nothing here is pure, so each of the fourteen gets its
argument. Three oracle shapes are used.

### (a) Replay — nine functions

Snapshot the live object into a file-scope scratch copy BEFORE our body runs, run our body on
the real object, then run the DLL's body on the scratch and compare.

* **Six const accessors** (`GetRoot`, `GetController`, `FindElement`, `FindElementType`,
  `GetTemplate`, `GetContainer`) — the comparison is the returned pointer, which is the entire
  output of the function. This is a FULL oracle, not a spot check: these functions have exactly
  one observable effect.
  `FindElement`/`FindElementType` are forwarders into `Element::FindElement` /
  `Element::FindElementType`; running theirs calls that search a second time, which is harmless
  because the search mutates nothing.
* **`GetFileVersion`** — scalar comparison of the constant. Trivial, and it is registered
  because it has a live exe caller, not because it was easy.
* **`SetFlagsOnOff`** — full-object byte comparison over the ctor-initialised prefix (0xb4).
  Catches a wrong offset, a wrong mask order, and a stray write anywhere else in the object.
* **`??0ElementDescription`** — full-object byte comparison over all 0x68 bytes. Legitimate
  because the ctor writes **every** byte of the object, so nothing being compared is unwritten.

**Why the padding holes do not poison the replay.** The scratch is a `memcpy` of the live
object (for the ctor, of the target's pre-state), so both sides start from identical bytes and
the four uninitialised holes hold the same values on each side. The trap
`reference_oracle_must_not_compare_unwritten_bytes.md` describes — 0xCD surviving in THEIRS and
reading as a catastrophic port defect — bites only when one side starts from fill. It does not
arise here, and the code says so where it matters.

### (b) Replay with a redirected array — `StopTimer` only

`StopTimer` mutates a heap buffer the object merely points at, so a naive replay would shift the
SAME buffer twice and the second pass would report agreement with a corrupted array. The scratch
object's `_timers._data` is repointed at a private copy of the timer entries first, and the
comparison is over a fingerprint: the object prefix with that one pointer blanked (we changed it
on purpose — comparing it would report a divergence the oracle itself introduced), followed by
the live timer entries.

**One honest limitation, stated so a low `calls=` is not misread.** The private copy is a fixed
8 KB buffer, which holds 341 timers. If a dialog ever exceeds that, the call is **not diffed at
all** — deliberately, rather than reported as a divergence, because "the oracle's buffer was too
small" is a fact about the oracle and not about the port. A real dialog runs a handful of timers,
so this is not expected to fire, but `calls=` for `UI::DialogInstance::StopTimer` is a lower
bound on invocations, not an invocation count.

### (c) Shape, decoded from the shipped bytes at registration — four functions

`OnUpdateLayout`, `OnUpdate`, `OnElementEvent` and `??1DialogController` cannot be replayed:
running a dispatch loop twice would tick every controller twice, and running the dtor twice
would swap-remove the object from the handle table twice. So the whole failure surface is
checked structurally instead, at registration time, the way Wave 6 lane A's `VerifyThunk`
checked a tail-jump's slot.

The three loops compile to the same five-instruction core, and there are exactly three things a
transcription of one can get wrong:

```
39 99 <disp32>     CMP  dword ptr [RCX+disp32],EBX     ; the COUNT field   (0x80)
48 8b 46 <disp8>   MOV  RAX,qword ptr [RSI+disp8]      ; the ARRAY field   (0x78)
ff 50 <disp8>      CALL qword ptr [RAX+disp8]          ; the VTABLE SLOT   (0x38/0x40/0x48)
```

Get the array or count offset wrong and the loop walks the timer array instead; get the slot
wrong and every controller has a different virtual called with this method's arguments — which
is the `reference_serialize_lazy_resolve_and_wrong_slot.md` failure. `DecodeDispatchLoop` reads
all three displacements out of the shipped instruction stream and `VerifyDispatchLoop` reports
them through an ordinary diff site. That is complete coverage of a body whose only content is
those three numbers plus a loop.

`??1DialogController` (0x1800cd2f0) is two instructions of substance —
`LEA RAX,[rip+…]; MOV [RCX],RAX` then `JMP 0x1800bfdb0` — and both addresses it names are
exported, so `VerifyDtorThunk` decodes the RIP-relative displacement and the JMP target and
compares them against `??_7DialogController@UI@@6B@` and `??1Internal@Object@@UEAA@XZ`. The port
itself does not reimplement the handle-table swap-remove: it calls the same exported
`~Internal` retail's JMP lands in, through the seam, so if Wave 3's `~Internal` port is ever
armed we follow it exactly as a cutover build would.

**Read a shape row correctly in `RtDiffSummary()`.** A shape check runs ONCE, at registration,
so it shows as `calls=1`. That is not "barely exercised" — it is one check that covers the whole
body, and unlike a per-call oracle it also fires on a run where the function is never called at
all, which is the opposite of the
`reference_uncalled_port_is_not_verified.md` failure mode.

### What the oracle does NOT cover

* Nothing proves that our `OnElementEvent` reproduces retail's **argument marshalling** — only
  that it calls the right slot on the right controllers. That marshalling is the one subtle part
  of the function and it is written up in the source: the `InternalPtr<Element>` first parameter
  arrives by address, and retail re-reads `*RDX` into a FRESH stack slot on every iteration and
  hands the controller the address of that copy, never the caller's. Collapsing that into
  "pass the caller's pointer through" would be a real behavioural change, so it is reproduced
  literally.
* The three virtuals are `UEAA`. Registering them redirects our direct `fn_DialogInstance_*`
  callers; every dispatch through the DLL's own `??_7DialogInstance@UI@@6B@` still lands in the
  DLL. That is by design (no detour — see §6) and it means the ported bodies see less traffic
  than the function's true call count.

---

## 5. The two phantom names — both were live defects, neither was port work

### `?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z`

**Verdict: a mis-mangling of an overload we already resolve correctly, whose null fallback bound
the pointer to a completely different function.**

It is not in the export table. The signature it spells — `(uint, int, int, External const*,
String const&)` — is the six-argument overload at **0x180113470 / ordinal 1541**, whose real
signature is `(uint, Object::Base*, ExternalPtr<DialogControllerConfig>,
ExternalPtr<ElementDescription>, System::String const&)`. Arguments 2 and 3 were typed as plain
`int`s: the exact `reference_hidden_retval_and_int_pointer_args.md` failure, where a pointer
parameter typed `int` puts a 32-bit half in R8D/R9D.

That overload is **already** resolved correctly a few lines below as
`fn_DialogInstance_AddController6` (through hard-coded Runtime RVA `0x180113470`), so the
phantom was a duplicate as well as a wrong name.

The part that makes it a defect rather than a curiosity: `ResolveExport` returned null, so the
`if (!fn_DialogInstance_AddController)` fallback fired **on every run**, and that fallback reads
exe dispatch slot `0x140151a60` — which `tools/dispatch_slot_map.py` says is
`?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z`. So a pointer named
`AddController` has been silently bound to `Element::AddChild` for as long as it has existed.
Calling it would have passed a controller type id where `AddChild` expects an
`InternalPtr<Element>`.

It never fired only because **the pointer has zero callers tree-wide.** A grep across every
`.cpp` and `.h` in the repository finds it in exactly three places: its own declaration, its own
definition, and a comment.

That comment is the third finding here. `standalone/reg_unreg_lane2.cpp:140` flagged the
0x140151a60 naming mismatch — correctly — and then reassured the reader that it was
*"currently harmless because `fn_DialogInstance_AddController`'s own ResolveExport already
succeeds so that fallback branch never fires."* **The mismatch was real; the reassurance was
exactly backwards.** The ResolveExport never succeeded. This is the `reference_uncalled_port_is
_not_verified` shape applied to a comment: a plausible-sounding "so it can't matter" that nobody
checked.

**Fixed at the source** (PLAN §6 step 8): the typedef, the `extern`, the definition and the
resolve are all deleted from `engine/funcs/engine_funcs.{h,cpp}` — deleted rather than repaired,
because the correctly-named overload is already there and a second pointer to it is exactly the
vestigial `fn_*` the code-fn wave deleted ten of. The removal notes in both files state what the
symbol was believed to be, so the next reader recognises the old claim if they meet it
elsewhere. The `reg_unreg_lane2.cpp` comment is corrected in place, keeping the old claim and
saying why it was wrong.

### `?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ`

**Verdict: an invented class name AND a missing parameter. The real function exists and is a
one-liner.**

Neither this name nor **any** symbol containing "DialogReference" is in the 4,926-entry export
table — the class name was invented. The real function is

```
?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z
    Runtime 0x1800388d0, ordinal 2861
    MOV RAX,[RCX+0x78] / MOVSXD RCX,R8D / MOV RCX,[RAX+RCX*8] / MOV RAX,RDX / MOV [RDX],RCX / RET
```

— it returns `ExternalPtr<Object::External>`, and **it takes an int index** that the typedef
`fn_Dialog_GetReference_t(void* self, void* retval)` omitted entirely. It sits 0x20 bytes after
`?GetTemplate@` and is its exact twin against `Dialog+0x78` instead of `Dialog+0x68`. Comparing
the two overloads, as the brief suggested, is what found it.

Unlike the `AddController` phantom this one has **no fallback at all**, so `fn_Dialog_GetReference`
has been NULL since it was written, on every run, with nothing to hide it. It too has zero
callers, so nothing has ever noticed — the Wave 6 `Geometry::Attach`/`Detach` shape exactly,
except that here there is not even a guarded call site: there is no call site.

**Fixed at the source**, the same way and for the same reason: deleted, with a note recording
both the wrong name and the right one, index and all, so whoever needs it declares the real
signature rather than resurrecting this.

### Effect

`tools/export_name_audit.py`: **5 phantoms before, 3 after.** The 3 remaining are
`?CheckMod@SaveLoad@Save@@QEAAHHI@Z`, `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z`
and `?PlayEffect@Scene@Audio@@QEAI…` — Wave 5 and Audio, not this lane.

---

## 6. Other corrections made, and two findings recorded rather than acted on

### Corrected at their source

| Where | Was | Is |
|---|---|---|
| `engine_funcs.h` `Toolbar_CenterOnEntity_Impl` | the `GetController` filter was a literal `0` | **a live, player-visible bug — see §6a below.** Now the EntityUI type id, resolved before the call, as retail does |
| `engine_funcs.h` `fn_DialogInstance_GetController_t` | third parameter documented and named `index` | it is a **type id**. Retail 0x180113320 walks the whole array and compares the argument against each controller's own type id (vtable slot +0x10, the `Object::Internal` Type slot). Calling it with `0` asks for "the controller whose type id is 0", not "the first controller". |
| `engine_funcs.{h,cpp}` | `fn_Dialog_GetReference`, `fn_DialogInstance_AddController` | deleted, with the §5 write-ups at the declaration sites |
| `standalone/reg_unreg_lane2.cpp:140` | "harmless because that ResolveExport already succeeds" | the ResolveExport never succeeded; the fallback always fired |

### 6a. "Go to entity" never followed the entity — the earlier fix was only half of it

**A port's first job is to make you re-read the call sites, and this is what that produced.**
`fn_DialogInstance_GetController`'s third argument is a type-id FILTER, and
`Toolbar_CenterOnEntity_Impl` (`engine_funcs.h`, the reimplementation of exe `FUN_140103040`)
passed a literal `0`. Nothing has type id 0, so `GetController` returned null for every dialog,
`controllerRaw` was null, the `if (controllerRaw != 0 && tidEntityUI)` guard below could never
pass, `panelController` stayed null, and `entity+0xd8` — the follow flag — was never set.

That is **verbatim the symptom the comment ten lines below it claimed to have cured**: "the
camera centred on the entity but never FOLLOWED it, for every caller (go to entity in
workplace / profession / residence / townhall / statusbar / toolbar / citizen panels)". An
earlier session corrected the DOWNSTREAM `IsDerivedFrom` type id (from `g_TypeId_PageToggleUI`
to the EntityUI id) and left the UPSTREAM filter at 0, so the bug survived its own fix note.
That is this project's most expensive failure mode — a wrong comment is worse than none,
because it is believed — and it is what PLAN §6 step 8 exists for.

**Verified against retail before changing anything, and deliberately NOT inferred from the
surrounding comment, which was the thing under suspicion.** Ghidra's decompiled C of
`FUN_140103040` prints the call as `GetController(dialog, (uint)&local_res10)` — it drops the
third argument entirely and mislabels the hidden retval slot as a `uint`
(`reference_ghidra_c_output_vs_disassembly.md`, a third instance in this one lane). The
disassembly is unambiguous:

```
14010307a  MOV  R8D,dword ptr [0x1401c4f00]   ; arg 3 = the EntityUI type-id FILTER
140103081  MOV  RCX,qword ptr [RBX]           ; this = DialogInstance*
140103084  LEA  RDX,[RBP + 0x6f]              ; hidden retval slot
140103088  CALL qword ptr [0x140151768]       ; UI::DialogInstance::GetController
...
1401030a6  MOV  EDX,dword ptr [0x1401c4f00]   ; and AGAIN, for IsDerivedFrom
1401030af  CALL qword ptr [0x140152a98]       ; fn_IsDerivedFrom
```

Three independent confirmations that this is the right reading:

1. `tools/dispatch_slot_map.py 0x140151768` resolves that slot to
   `?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z` —
   the very export this lane ported. The trailing `I` in the mangled name IS that uint32.
2. `get_xrefs_to 0x1401c4f00` shows `RegisterType_EntityUI` as its only DATA writer, so the
   value really is the EntityUI type id.
3. The **load-it-twice idiom is universal**: every sibling call site in the exe reads
   `0x1401c4f00` twice, once for the filter and once for `IsDerivedFrom` —
   `EditTool_SelectEntity` (0x1400fd434/0x1400fd460), `StatusIconUI_OnCreate`
   (0x14007b14b/0x14007b17c), `EntityUI_impl_1ee0`, `EditTool_Startup_PositionInfoDialog`,
   `TradeWork_impl_3480`. Our tree already gets this right at **sixteen** call sites (and
   `code/Game/Component/nomadcomponent.cpp:364` documents the exact `MOV R8D,[0x1401c4f00]`
   instruction next to its own call). `engine_funcs.h` was the **only** site in the whole tree
   still passing a literal 0.

The fix hoists the existing `ResolveExeTypeId(0x1401c4f00, "EntityUI", …)` above the
`GetController` call — the ordering was the trap, since `tidEntityUI` was resolved *after* it —
and passes it as the filter. The comment block is rewritten to say what is true now and keeps
one line recording that the earlier fix was **incomplete**, so the next reader who meets the old
story recognises it. `-fsyntax-only` clean; not run, per §7.

There is now a general reference for this: `reference_getcontroller_third_arg_is_a_type_filter`.

`engine_funcs.h` line 2547's older comment on `fn_DialogInstance_OnUpdateLayout` says it
"iterates child elements at +0x78 (count at +0x80)". The offsets are right; the wording is
slightly off — +0x78/+0x80 is the **controller** array, not children (children are the
`LinkedList` at +0x38). Left alone deliberately: it is another lane's neighbourhood in a file
several agents are editing this session, the offsets it names are correct, and the authoritative
statement is now in `runtime/types/ui/DialogInstance.h`. Flagged here rather than edited.

### 6b. `Collection.h`'s `LinkedList` is CONFIRMED — the layout was not changed

Stated loudly because the file was edited elsewhere this session (lane C found and fixed a
self-contradiction in its `_pad14` comment, which said `sizeof == 0x10` where the same block
correctly said 0x18). **This lane touched none of it, and independently confirms the layout it
already had.** Two things it confirms:

* `{_head @+0x00, _tail @+0x08, _count @+0x10}` with `Node {element, prev, next}` — exactly as
  written.
* The otherwise-unpinned `_pad14` is real, and this lane can say what it IS: it is the
  `+0x4c..0x4f` hole that the `DialogInstance` constructor never initialises. An independent
  second witness for a field that previously had none.

**The reason to record this rather than just agree quietly**: a first pass through the
`DialogInstance` ctor's node insert produced the OPPOSITE reading — `{_tail, _head}` with an
append — and it is perfectly self-consistent. A lane that had stopped there would have filed a
confident "correction" against a header that was already right, and the next reader would have
had two contradicting sources. What settles it is a second witness outside the ctor:
`?GetFirstDialog@UserInterface@UI@@…` (0x180031150) is literally `MOV RAX,[RCX]` — the FIRST
element lives at the list's `+0x00`, so `+0x00` is the head and the insert is a **prepend**.
One ambiguous body is not evidence of a layout; two bodies that can only agree one way are.

### 6c. Retail `StartTimer` really does append a duplicate — do NOT "fix" this when it lands

`?StartTimer@` (0x1801137c0) is held back (§2a, it allocates), but this belongs where the next
porter will read it. Its body walks `_timers` looking for an entry whose id matches, and when it
finds one it rewrites that entry's two `Time` fields — **and then falls through and appends a
second entry with the same id anyway.** There is no `return` and no `else`. Retail accumulates
duplicate timer ids, and `?StopTimer@` removes only the first match it finds.

It reads exactly like a missing early-out, and it is not one: it is the shipped behaviour, and
anything downstream that has ever worked has worked with duplicates present. Port it as written
(CLAUDE.md: retail's oddities are load-bearing) and, if it ever needs changing, change it as a
deliberate deviation with a note — not as a tidy-up during transcription.

### No detour is installed, deliberately

Same argument as Wave 6 lane A. A detour would route the DLL's own UI loop into this port, which
is a much larger blast radius for nothing the safety argument does not already give: nothing
ported here allocates, frees or reshapes a tree; the one ported function that writes vtable
pointers resolves the DLL's own exported tables rather than minting one; and the replay oracle
proves per call that our body leaves the object byte-identical. The census `detour` column stays
at 0, and PLAN §4.2's note that the mechanism remains unexercised in anger still stands after
this lane.

---

## 7. What this lane did NOT run

By instruction, because five other lane agents and one control-run agent are working
concurrently and the control agent owns the build:

* **No `make`, no `sa-run`, no `tools/rt_run.sh`, no launch of the game, no `git commit`.**
* **Therefore no diff-oracle numbers.** Every "the oracle will report" statement above is a
  prediction. Nothing in this document claims a measured call count, a measured divergence
  count, or a measured performance figure, because this lane measured none of them.
* No `tools/runtime_census.py` run (it reports against the tree, but the honest place to read
  the lane's registered count is the control agent's post-wave census, not a mid-wave one).

What this lane DID run:

* `pefile` over the shipped export directory, before writing any body — the source of every
  address and ordinal in §1.
* `-fsyntax-only` with the build's exact command line, at `-O0 -g -D_DEBUG` and at
  `-O3 -DNDEBUG`, on `runtime/src/ui/dialog.cpp`, `engine/funcs/engine_funcs.cpp`,
  `standalone/reg_unreg_lane2.cpp` and `runtime/src/ui/rt_ui_port.cpp`. All warning-clean.
* A throwaway program that prints `offsetof` for every field of the three new headers, to check
  the `static_assert`s against the retail displacements numerically rather than only by
  assertion.
* `tools/export_name_audit.py`, before (5) and after (3).
* Ghidra decompiles AND disassembly of all 26 real functions. The disassembly was not optional:
  `?StopTimer@`'s `MOVSXD R10,[RCX+0x70]` and its `ADD RCX,RDX / JZ` guard against the SUM both
  vanish in the decompiled C, and `?GetTemplate@`'s `MOVSXD` is printed as a zero extension —
  `reference_ghidra_c_output_vs_disassembly.md`, three times over in one lane.

### The first thing to check when the wave's diff run lands

If any of the four `[shape]` sites reports a divergence, it is a decoder problem before it is a
port problem: the byte scanner looks for its three patterns **in order** within a 0xC0-byte
window, and a differently-scheduled prologue would break that before anything about the loop
was wrong. Read the reported "theirs" string — it prints the displacements it actually decoded,
so a garbage triple means the scan missed and a plausible-but-different triple means the port is
wrong.
