# Wave 9, lane Y — the Object/Engine cluster of attempt 04's `[RtUnresolved]` list

Worked 2026-09-08 on `wave/upgrade-path-complete`, base `7bf0059`. Eleven rows,
taken from the 79 distinct `[RtUnresolved]` names the DLL-absent boot printed
(`docs/runtime_port/wave9_cutover/attempt_04/standalone_error.log`; the 79 is
`grep '\[RtUnresolved\]' | awk '{print $2}' | sort -u | wc -l`, and it is 79 in
the per-name section, not the 152 the commit message quotes for the whole run —
that number counts the `[RtUnresolved]` summary lines too).

**This lane did not build and did not run the game**, by instruction. Every
number below comes from the shipped bytes, the audits, the compiler, or a log
another lane produced. The cells it needs are in §7.

---

## 1. Result

| | |
|---|---|
| Ported and registered | **2** — `External::Compile`, `InternalBasePtr::CheckPtr` |
| Armed at cutover, already written and shadow-verified | **2** — `Internal::Internal`, `Internal::~Internal` (plus the two persist-state mutators that ride the same tier) |
| Live defects found and fixed | **3** — a frozen null resolve, a wrong-arity indirect call, a 2,333-site EXEC-of-0 |
| Stated blockers found STALE | **3 of 6** — `CheckPtr`, `ResetPersistState`'s arity, `MainLoop::OnVideoError`/`Restart`'s detour |
| A prior verdict found WRONG about one of its two members | **1** — `FindOrLoad` is *not* blocked on a ported `Collection::Set` |
| Declined, with the reason restated for cutover | **5** — `Clean`, `FindOrLoad`, `SaveText`, `LoadResources`, `OnVideoError`/`OnVideoRestart` |
| `?_singleton@Factory@Object@@` | writer already correct (lane X); the **reader** was the defect |

Census, measured across the change on one tree (`tools/runtime_census.py`):
**Object ported 23 → 25, TOTAL 589 → 591; `called` UNCHANGED at 43 / 846.**
The `called` column is quoted deliberately: WAVE9.md records that a port whose
callees are DLL internals *grows* the cutover surface (`KeyMap::RegisterInput`
went 823/504 → 825/505). Neither of this lane's two ports reaches an unexported
internal, so this one is the good case, and it is measured rather than assumed.

Audits, each with its denominator, run on this tree:

| Audit | Before (`7bf0059`, stashed) | After | Verdict |
|---|---|---|---|
| `rawdoor_audit.py` | 478 sites, **8 UNGUARDED-CALL** | 479 sites, **8 UNGUARDED-CALL** | the +1 site is this lane's one new `RtOriginal`, inside `RT_DIFF_SCALAR`, and it classifies DEFERRED not UNGUARDED. The 8 are the known false positives, individually re-read: `rt_port.h:133` is the `RT_ORIGINAL_ONCE` **macro definition**, and the seven `ui/element.cpp` sites are inside `if (replay.copy)`, where `ReplayGuard`'s ctor returns early unless `rt_diff_on__`. **The gate row in WAVE9.md says 469; this tree reads 478 before I touch it** — the population moved by +9 in commits between that measurement and `7bf0059`, which is why the denominator is quoted on both sides rather than the verdict alone. |
| `port_bypass_audit.py --by-name` | — | ADDRESS door 191 / **0**; NAME door 405 / **0**; cross-check 601 ported names, 394 raw-door statements mention one, **0 the parser did not see**; exit 0 | |
| `rt_data_door_audit.py` | — | **PASS**, exit 0, 112 minted DATA symbols / 171 sites. `RtResolveCached` is in its discovered port-aware wrapper list, so this lane's new `Factory::_singleton` reader is classified, not invisible. The 2 UNCLASSIFIED doors (`audio.cpp:880`, `allocator.cpp:649`) are pre-existing and neither is in a file this lane touched. | |
| `g++ -fsyntax-only` | — | clean on all three touched files, with the project's exact flags, and `engine_funcs.cpp` also compiled with `-DBANISHED_CHECKPTR_STATIC_NOP -DNDEBUG` because this lane changed code inside that `#ifdef` | |

---

## 2. The three defects

### 2.1 `?_singleton@Factory@Object@@0PEAV12@EA` was resolved 80 lines of init too early, and froze on null

`runtime/src/object/external.cpp` resolved the singleton **inside
`RtObjectExternalRegister()`** and cached the answer in a file static.
`RtObjectPortInit()` runs at `standalone/resolve_standalone.cpp:325`;
`RtDataMintInit()` — which registers the minted storage for that symbol — runs
at `:405`. So at cutover the read happened before the thing it was reading
existed, and the cache froze on null for the life of the process.

**It is not an inference. Attempt 04 says it in its own words**, and two
different instruments said it:

```
[RtUnresolved] ?_singleton@Factory@Object@@0PEAV12@EA  (asked by 00007ff71e202371)
[RtResolveEarly]   BYPASS?   caller=00007ff71e202371  "?_singleton@Factory@Object@@0PEAV12@EA"
[RtResolveEarly] 2 of 4 are registered now -- those are the candidates.
```

**The symptom would not have been a crash, which is the part worth carrying.**
`ExternalFactory::FindNext` reads `factoryRoot` and walks Factory's type map to
run retail's inlined `IsDerivedFrom`. A null root makes the walk visit zero
nodes, so **every derived-type query silently finds nothing** — a wrong answer
delivered by healthy-looking code, which is exactly the failure mode this file's
own header warns about two sections further down.

Fixed by making it a lazy accessor over `RtResolveCached`, the tree's
never-cache-a-null helper. Deliberately *not* fixed by moving `RtDataMintInit`
earlier: nothing in a port file may depend on an init ordering it does not own.

A near neighbour was checked and is **not** defective:
`runtime/src/object/type_system.cpp:69` resolves the same symbol but retries on
every call (`if (!g_factorySingleton) …`), so it is self-healing. That asymmetry
is why the attempt-04 line reads `1` asker and not `2`.

**Two neighbours were checked and are NOT defective — recorded because both
look exactly like the one that was.**

* `engine/funcs/engine_funcs.cpp:4184` sets a *different, exe-side*
  `g_pFactorySingleton` (a global, not the file static that was wrong) from the
  exe dispatch slot `0x140152760`. `tools/dispatch_slot_map.py 0x140152760`
  confirms that slot is the PE import of `?_singleton@Factory@Object@@`, and
  `standalone/address_registry.cpp:1544` fills it with
  `ResolveAddr(Module::Runtime, 0x1801fc6e8)` — which consults the **port
  registry first**, and `AddressRegistryInit()` (`main.cpp:1729`) runs *after*
  `RuntimePortInit()` (`:1708`), inside which the mint registers that storage by
  address. So the ordering is right, by luck or by design, and it is written
  down here so nobody has to re-derive it.
* `runtime/src/object/type_system.cpp:69` resolves the same symbol but retries
  (see below).

**The row's stated acceptance criterion was already met by another lane.** The
brief says a minted storage row is only done when its writer writes through the
port-aware door. `??0Factory@Object@@`'s port is
`runtime/src/engine/singleton_ctors.cpp`'s `Ctor_Factory`, which ends
`Publish(SlotFactory(), self)` with `SlotFactory()` being `RT_DATA_SLOT(…)`.
Lane X did that. The **reader** was the open half, and nothing was looking at it.

### 2.2 `InternalFactory::ResetPersistState` calls both encoding-factory slots with the wrong arity

The port passed `(factory, &serializer)`. Retail passes **three** arguments,
identically on both branches, `0x1800bfe68`–`0x1800bfe84`:

```
MOV RCX,[RDX+0x10]     ; arg1 = stream->encoding (the factory)
XOR R8D,R8D            ; arg3 = 0                <-- set BEFORE the branch
MOV RAX,[RCX]
CMP [RDX+0x8],EBX      ; direction == 0 ?
LEA RDX,[RSP+0x28]     ; arg2 = &serializer
JNZ +5
CALL [RAX+0x18]        ; reading -> CreateReader
CALL [RAX+0x20]        ; writing -> CreateWriter
```

The port's own comment called this a stated deviation and said the two slots
"should be disassembled and the exact arity written down" if the function were
ever taken live. That sentence **is WAVE3_LANE_B.md's gate step 2**, and it is
now closed.

It was not academic. The third argument is the serializer **version**, and
**the tree had already proved the callee reads it** — Wave 4's `IO::Serializer`
port disassembled the same two slots out of the out-of-line
`??0Serializer@IO@@QEAA@AEAVStream@1@H@Z` (`0x1800b9960`) and wrote in
`runtime/src/io/serializer.cpp`:

> The `int version` argument is NOT ignored (the claim this file used to make):
> R8D is live all the way to the CALL … BinaryWriter stores it and writes it to
> the stream. **That pins the arity of both factory slots at 3**, which the Wave
> 4 brief listed as open.

So a two-argument call hands `BinaryWriter` whatever the caller left in R8 and
writes it into the save stream. **Four lanes running, four for four: the
holdback's stated blocker was a fact the tree already had, written down in a
different file.** The mitigation is the same one PLAN §6 step 8 prescribes —
the fact is now in the function that needed it, with the instruction evidence.

### 2.3 `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` unregistered is 2,333 null calls at cutover

Counted, not estimated, over `code/ engine/ standalone/ runtime/ dx11/`:

* **3,475** `fn_InternalBasePtr_CheckPtr(` call sites across **269** files;
* **1,142** of them carry the `if (fn_InternalBasePtr_CheckPtr)` null guard —
  the macro is function-like, so the bare name in the `if` reads the variable;
* **2,333** do not. Those expand to `engine_funcs.h`'s
  `if (!g_checkPtrIsNop) fn_InternalBasePtr_CheckPtr(p);`

With no DLL and no port, `ResolveExport` returns null, the byte sniff in
`engine_funcs.cpp` has no bytes to look at, `g_checkPtrIsNop` goes **false**, and
all 2,333 call a null pointer. EXEC-of-0 in the hottest code in the game.
Attempt 04 never saw it because it returned out of `GameMain` before any entity
existed.

**The stated blocker was real and was about the wrong object.** The reason not
to register, written on the function since Wave 3, was that registering makes
the sniff see a GCC prologue instead of `C2 00 00`, flip the flag, and pay ~60k
indirect calls a frame for a no-op. That is a true statement about the
**detector**, and it had been read for two waves as a fact about the **port**.

Fixed where it was wrong. The detector now asks the registry first:

```cpp
const bool isOurPort = b && RuntimeExportIsRegistered("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ");
const bool doesNothing = isRet || isOurPort;
```

Our port's body is empty *by construction* — we wrote it — so this is a proof,
not a heuristic. With that in place, registering is behaviour- and
cost-neutral while the DLL is present (the flag stays true either way), and in
the perf build the sites are compiled out regardless. `CheckPtr` is therefore
**registered unconditionally**; `BANISHED_RT_INTERNAL_CHECKPTR` survives only as
a log assertion.

Two things this deliberately does **not** claim. It is a **registered** symbol,
not an **exercised** one — in the perf build it has no callers at all, which is
`reference_uncalled_port_is_not_verified.md` and is written into the source
comment rather than left for a census reader to trip over. And the perf claim is
**argued, not measured**: the argument is that the flag's value is unchanged, so
no call site changes behaviour. §7 asks for the A/B that settles it.

The resolve also now prints `*** UNRESOLVED ***` instead of quietly degrading to
`real call` on a pointer that is not there.

---

## 3. `Internal::Internal` / `~Internal` — ARMED at cutover, and why that is not the NO-GO being waived

Both bodies have existed since Wave 3 behind `BANISHED_RT_INTERNAL_TABLE`, with
a written NO-GO. They are on attempt 04's unresolved list, and they are not
optional: `Object::Internal` is the base of every object in the engine.

`RtObjectInternalRegister()` now arms the tier when **`BANISHED_RT_NO_DLL`** is
set as well as when the explicit flag is. The runbook already sets that variable
for the controlled cutover boot, so no runbook change is needed.

**The NO-GO's premise expires; it is not overruled.** WAVE3_LANE_B.md held the
tier back because "our code now drives the LIVE handle table" *while the DLL's
own `Internal` ctors drive it too* — two writers into one array, and a wrong
`_factoryIndex` is the swap-remove wild write with no proximate symptom. With
Runtime-x64.dll absent there is no second writer, and there is also no DLL body
to fall through to. The hazard and the alternative disappear together.

The tier's own three-step gate, checked rather than asserted:

1. **Shadow green with `calls=` in the thousands — MET, and met in Wave 3.**
   WAVE3.md: `Internal::Internal` / `~Internal` **93,387 calls each, 0
   divergences, ctor and dtor counts identical**. This was true when the NO-GO
   was written; the tier stayed unarmed because step 2 was open.
2. **The encoding-factory arity — CLOSED by this lane** (§2.2).
3. The wave-boundary run. That is a cell (§7), not a code change.

**What is still owed, stated rather than implied by the arming.** The two
persist-state shadow sites have **never executed** — WAVE3.md defect E, they
live only on the save/load path, and Wave 3 could not drive it.
`BANISHED_AUTOLOAD` was repaired in Wave 4, so that is now a runnable cell and
§7 asks for it. Arming the tier does not turn `calls=0` into evidence.

**A dependency the arming exposed, and it had to be fixed with it.** The tier's
`MemAlloc`/`MemFree` resolved through `RtOriginal<>` only. That is correct while
the DLL is present and is deliberate — the handle table's buffers cross the DLL
boundary in both directions, so an armed allocator port must not split the one
heap that owns them. But `RtOriginal` **is** `GetProcAddress`, so with the DLL
absent it returns null and the next line calls it: every tier-2 allocation would
have died on the first `Internal`. The premise dies with the DLL too — no DLL
means no DLL-side `Internal` ctors and exactly one heap — so the raw door now
falls through to the port-aware one when it has nothing to offer.

That is **not** the `ResolveExport → ResolveAddr(Module::Runtime, addr)` idiom
lane F deleted 166 of. That one falls back to an address in a module that will
not be mapped. This falls the other way, from the DLL to our own registered
port, which at cutover is the only thing that can answer.

---

## 4. `External::Compile` — ported

`?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z`,
retail `0x1800066e0`, ordinal 1742. The entire body:

```
1800066e0  MOV EAX,0x1
1800066e5  RET
```

It is `External`'s virtual **default**: the base class cannot compile anything,
so it answers `CompileResult(1)` and every resource type that can compile
overrides the slot. `path` is genuinely dead — RDX is never read.

Two rules checked rather than assumed. `?A` in the mangling means the enum is
returned **by value**, but a `W4` enum is 4 bytes and comes back in EAX, so there
is **no hidden return slot** and the argument count matches the mangled name with
nothing shifted (`reference_hidden_retslot_wild_write.md`). And the oracle is a
real one: the function is pure — no stores, no calls, no read of `this` — so
`RT_DIFF_SCALAR` against the shipped body is an exact comparison, not a shape
check, and replaying it is free.

**It fills a hole in a minted vtable.** `standalone/rt_data_mint_tables.inc`
carries `??_7External@Object@@6B@` (`0x1801547a0`, 8 slots) with `Compile` at
**slot 4** as `RT_MINT_NAMED` — i.e. a named TRAP today. Registering the port
fills it from the port registry at runtime.

**A cheap follow-on this lane did not take, flagged rather than done.** Slots 1
and 2 of the same table are `?TypeName@External@Object@@UEBAPEBDXZ`
(`0x1800066c0`) and `?Type@External@Object@@UEBAIXZ` (`0x1800066d0`), and they
are two instructions each:

```
1800066c0  MOV RAX,[0x1801f9b40]   ; _typeInfo + 8   -> the name
1800066d0  MOV EAX,[0x1801f9b38]   ; _typeInfo + 0   -> the id
```

Wave 3 lane C left them alone because `?_typeInfo@External@Object@@` was
"deliberately left on the DLL". **That reason is stale**: lane C2 mints it
(`rt_data_mint_tables.inc:1378`, `0x1801f9b38`). Porting them would take
`??_7External` from 3 unfilled named slots to 0. It belongs to whoever owns the
reflection reads, because the read must go through the port-aware door or it
will read the DLL's copy and freeze — the defect in §2.1, one class up.

---

## 5. What was declined, and on what evidence

### 5.1 `ExternalFactory::Clean` — DECLINED, the Wave 3 reason still holds exactly

`0x1800be310` calls `0x18002dff0` and `0x18002cf60`, both **unexported
`Collection::Set` internals** — the erase and rebalance for the 0x18-wide
element. There is no name to register and no exported entry point to call, so at
cutover a ported `Clean` needs a ported `Set<0x18>` erase, which nothing has.
`Clean` is also the only thing that bumps `ExternalFactory::_generation`, so
getting it wrong silently invalidates or fails to invalidate every
`ExternalRef` cache in the exe.

### 5.2 `ExternalFactory::FindOrLoad` — DECLINED, but the STATED REASON IS WRONG and is corrected here

WAVE3_LANE_C.md §5 and WAVE9.md's holdback table both say `FindOrLoad` and
`Clean` together "rewrite the shared resource AVL tree", and that porting them
means "porting `Collection::Set` insert, erase and rebalance for the 0x18-wide
element". **That is true of `Clean` and false of `FindOrLoad`.** Its call list
(`0x1800bf320`, filtered to CALL) contains no `Set` internal at all:

| Callee | What |
|---|---|
| `0x1800bd360` | `?ExpandRelativePath@…` — **exported** |
| `0x1800be0e0` | the path lookup |
| `0x1800be570` ×2 | `?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z` — **exported** |
| `0x1800bc620` | `?Load@External@Object@@SAPEAV12@…` — **exported** |
| `0x1800c64f0/6610/6740/6880/6970`, `0x1800c9440/92d0`, `0x1800c7ad0` ×12 | `System::String` plumbing and `MemFree` |

**The AVL insert lives behind `Add`, which is an exported symbol a port can
call by name through the port-aware door.** So `FindOrLoad` decomposes: it is
string plumbing plus three exported calls, and it does not need a ported `Set`.

This is the same shape as the correction WAVE9.md's own holdback table records
about its first draft — a group summary that was accurate about the group and
wrong about one of its two members, catchable only by reading each one.

It is still declined **by this lane**, for a reason that is now the honest one
rather than the inherited one: it is ~30 call sites of reference-counted
`String` traffic on the boot path, this lane cannot run anything, and the
failure mode is silent (a wrong path loads the wrong resource). It wants a lane
with the game directory, and it should be sized as "string plumbing", not as
"port the AVL tree".

Note also that the "no oracle, both sides mutate the one tree" objection is a
**DLL-present** objection. At cutover there is no second implementation to
double-mutate with — which means the oracle has to be recorded *before* lane G
or not at all, exactly as WAVE9.md §2 says.

### 5.3 `External::SaveText` — DECLINED, blocker CONFIRMED still live

`0x1800bb9d0` drives the object's `+0x28 Serialize(IO::Stream&)` slot with an
**`IO::TextFileStream`**, and WAVE4.md is explicit that `TextFileStream` (with
`FileStream`, `BinaryFileStream`, `VisitorStream` and the codec) was **not** in
Wave 4. It also writes a BOM through `WriteFile` directly, so under PLAN §4.1
decision 1 the Win32 half belongs in a `_win.cpp`. Re-checked, not inherited;
the blocker is real and unchanged. Needs `IO::TextFileStream` first.

### 5.4 `UpdateLoop::LoadResources` — DECLINED, and it is DOWNSTREAM of 5.2

WAVE8_LANE_A.md's three reasons were re-read. Two survive intact (~250
instructions with no oracle; a silent failure mode on the boot path). The third
is now a **dependency edge worth naming**: `LoadResources` calls
`ExternalFactory::FindOrLoad` once per enabled mod. So `FindOrLoad` is the
keystone of this cluster, and `LoadResources` should be scheduled **after** it,
by the same lane, or it will re-derive the same string plumbing.

WAVE8_LANE_A.md's own advice stands and is repeated because it is easy to lose:
start from `Engine::Loader::Run` (`0x1800dbfc0`), the only caller that matters,
and expect `Modification::SetupFileSystem` alongside.

### 5.5 `MainLoop::OnVideoError` / `OnVideoRestart` — the DETOUR blocker is STALE, and what replaces it is worse

WAVE8_LANE_B.md holds both back because each has exactly one internal caller — a
static thunk the `MainLoop` ctor installs into `Video::Device::_singleton` at
`+0x50`/`+0x58` — so "whoever ports those two must install the detour."

**At cutover there is no DLL, therefore no DLL-internal caller and no thunk to
detour. The blocker as stated cannot exist.** WAVE9.md already says the detour
question "stops being a category" at cutover; this is a concrete instance.

**What replaces it is a live cutover defect that no gate row covers.** Lane B's
ported `MainLoop` ctor still installs *the DLL's own* thunk addresses, by
address, `runtime/src/engine/engine.cpp:359-368`:

```cpp
*(void**)(device + DeviceErrorFuncSlot)   = (void*)ResolveAddr(Module::Runtime, MainLoopAddr::OnVideoError_static);
*(void**)(device + DeviceRestartFuncSlot) = (void*)ResolveAddr(Module::Runtime, MainLoopAddr::OnVideoRestart_static);
```

With the DLL absent both resolve **null**, and the ctor writes two nulls into the
live `Video::Device`. The first device-lost or device-restore event is then an
EXEC-of-0 with no symbol to name it. The port's own comment says the DLL's
thunks are kept "on purpose … both of which are held back", which is correct
today and becomes a null store at cutover.

So the disposition changes from WAVE9.md's **STUB** to a three-part item, and it
is not this lane's to land alone because it edits lane B's ctor:

1. port `OnVideoError` (`0x1800dc5e0`) and `OnVideoRestart` (`0x1800dc3b0`);
2. write **our own** two static thunks, since the retail ones are unexported
   internals at `0x1800dcd20` / `0x1800dc5c0`;
3. change the ported ctor to install ours when the DLL is absent.

A stub satisfies (1) and still leaves (2) and (3) as null stores, so **"stub it"
is not sufficient here** — that is the correction this section exists to make.

Sizing, from the bytes: `OnVideoRestart` is ~100 instructions — it walks
`_videoResources` at stride 0x18, looks each entry's type up in **`Factory`'s
type map (`0x1801fc6e8`, this lane's own §2.1 row)**, dispatches typeinfo
`vtbl+0x28`, does refcounted `String` traffic, calls `External::Load`
(`0x1800bc620`, exported) per entry, then `Array::SetSize(0)` and frees the
buffer under the allocator's critical section. `OnVideoError` is its mirror and
builds the list. Neither needs an unexported `Set`.

They remain unverifiable without forcing a device-lost event; §7 asks for that
cell, and WAVE9.md already says "if a lane does port them, the device-lost path
has to be forced, or the port is unverifiable."

---

## 6. Files touched

| Path | What |
|---|---|
| `runtime/src/object/external.cpp` | §2.1 fix (lazy `RtResolveCached` accessor), §4 `External::Compile` port + both registrations |
| `runtime/src/object/internal.cpp` | §2.2 arity fix, §2.3 `CheckPtr` default-on, §3 `BANISHED_RT_NO_DLL` arming + the `MemAlloc`/`MemFree` cutover door, and two stale comments corrected (the tier-2 header block, and `InternalBasePtr::Serialize`'s "registered only under TABLE=1 and UNVERIFIED", which lane S falsified in both halves) |
| `engine/funcs/engine_funcs.cpp` | §2.3 registry-aware `g_checkPtrIsNop` + the `*** UNRESOLVED ***` line |
| `docs/runtime_port/WAVE9_LANE_Y.md` | this |

Nothing else. `standalone/rt_data_mint*.*` and the three shared audit tools were
not touched.

---

## 7. Cells this lane needs

Ordered by what they unblock. Every one needs the DLL **present** except the
last.

1. **`BANISHED_RT_DIFF=1` on any cell that reaches gameplay**, reading the
   `[CheckPtr]` line first. It must now say `source="our port"` and
   `NOP (calls skipped)`. If it says `real call`, the registry check in
   `engine_funcs.cpp` is not seeing the registration and §2.3's fix is inert —
   and the 60k calls a frame are back, which is the thing the old comment feared.
   Also confirms `Object::External::Compile` has non-zero `calls=`; if it is 0,
   say so rather than reading zero divergences as a pass
   (`reference_uncalled_port_is_not_verified.md`).
2. **Three `sa-stress` runs with `BANISHED_CHECKPTR_NOP` unset vs `=0`**, on one
   binary. This is the A/B that turns §2.3's perf claim from argued into
   measured. One run cannot resolve it
   (`reference_one_perf_run_cannot_resolve_the_gate.md`), and the perf build
   compiles the sites out, so this must be the **debug or release** exe, not
   `sa-stress`'s perf exe — or the control is vacuous.
3. **`BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1`.**
   This is the cell Wave 3 could not run. It is the only thing that exercises
   `ClearPersistState` and `ResetPersistState`, and it is where §2.2's arity fix
   either shows up as a stream that still loads or does not. **Read `calls=` on
   both shadow sites** — Wave 3's defect E is precisely that they read green at
   zero. Note SHADOW and the table tier are mutually exclusive by design, so this
   cell must NOT set `BANISHED_RT_INTERNAL_TABLE` or `BANISHED_RT_NO_DLL`.
4. **`BANISHED_RT_INTERNAL_TABLE=1` across the four PLAN §5 wave-boundary
   gates** (tutorials 1–4, the achievement census, `Save9.sav`, `sim
   ms/iteration` within 5%). This is WAVE3_LANE_B.md gate step 3, the last one
   open, and it is worth running **with the DLL present** even though the tier
   only has to work without it — a divergence here is diagnosable and the same
   divergence at cutover is not.
5. **A forced device-lost event** (alt-tab a fullscreen exclusive swapchain, or
   a driver reset), for whoever takes §5.5. Without it that port is
   unverifiable, and WAVE8_LANE_B.md already said so.
6. **The next controlled cutover boot** (attempt 05+), with the runbook's env
   unchanged. Expected effects of this lane, stated as predictions so they can
   fail: `?_singleton@Factory@Object@@`, `?CheckPtr@InternalBasePtr@Object@@`,
   `?Compile@External@Object@@`, `??0Internal@Object@@` and `??1Internal@Object@@`
   all leave `[RtUnresolved]`; the `[RtResolveEarly]` BYPASS? line for
   `?_singleton@Factory@Object@@` disappears while the `??_7Thread@System@@6B@`
   one remains (it is not this lane's); `[RtPort] Object::Internal` reports the
   table tier ARMED by `BANISHED_RT_NO_DLL`; and the distance travelled should
   not *regress* past `=== ENTERING GameMain ===`. **A shorter run is a result,
   not a failure** — the tier arming means our code now drives the handle table
   for the first time.

---

## 8. What this lane could not answer

* **Whether the table tier is right.** It is shadow-verified for the ctor/dtor
  at 93,387 calls each and unexercised for the two persist mutators. Arming is a
  disposition, not a measurement, and cell 3 and 4 are what turn it into one.
* **Whether `?_singleton@Factory@Object@@` had ever produced a wrong answer with
  the DLL present.** It cannot have: with the DLL loaded the early resolve
  succeeds and the cached pointer is correct. The defect is cutover-only, which
  is why eight waves of green oracle runs are consistent with it.
* **The order in which the remaining rows are hit.** That is what the controlled
  boot exists to produce and no analysis on this side of the cutover can.
