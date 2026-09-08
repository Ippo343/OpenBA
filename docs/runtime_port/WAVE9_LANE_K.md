# Wave 9 — Lane K: the unnamed deleting-destructor family

Produced 2026-09-07 on `wave/upgrade-path-complete`, unblocking
[WAVE9_LANE_C2.md](WAVE9_LANE_C2.md). **Nothing was built and the game was not
run** — the integrator owns the exe this round. What was run: `g++
-fsyntax-only` on both new `.cpp` (writes nothing, touches no object directory),
`tools/rt_deleting_dtor_verify.py` (new, this lane), `tools/runtime_census.py`,
`tools/export_name_audit.py`, `tools/abi_retslot_audit.py`,
`tools/resolve_fallback_audit.py`, `tools/port_bypass_audit.py`, and Ghidra MCP.

## Headline

| | |
|---|---|
| slot-0 traps retired | **37 of 37** (not 27, and not 36 — see §1) |
| vtables taken from "one trap" to **COMPLETE** | **12 of 39** — the whole `Video::*Data` family (§1a) |
| distinct bodies | **1** |
| corrections to the assumed MSVC shape | **the shape itself was wrong** — these are `??_E`, the VECTOR deleting destructor, not `??_G` (§2) |
| `operator delete` variants collapsed | 10 addresses → 1 function (§3) |
| rows verified against the DLL's machine code | **37 of 37, 0 problems**, negative control passes |
| new unported Runtime rows created | **14** (§7) |

---

## 1. It is 37 — enumerated from the regenerated tables, not from the CSV

The original brief said 27, from `docs/runtime_port/wave9_data_harvest.csv`
(28 vtables). A later correction from the lead said 36 of 38, 313 slots. Both
are one generation behind. Enumerated from `standalone/rt_data_mint_tables.inc`
as it stands at `ca7ce73`:

```
vtables: 39   slots: 315   RT_MINT_UNNAMED: 37   all at slot index 0: True
vtables with NO unnamed slot 0: ??_7Loader@Engine@@6B@, ??_7Thread@System@@6B@
```

`Thread@System`'s slot 0 is `_purecall` (lane C2 said so); `Loader@Engine` is
new since. **37 is the number, and it is the number this lane registers** — the
two sets are equal, checked mechanically against the `.inc` and against
`deleting_dtor.cpp`:

```
my registrations: 37    unnamed slot-0 set: 37
in the tables but NOT registered: []      registered but NOT in the tables: []
```

Re-run that check after any regeneration of the `.inc`. It is the only thing
standing between a newly minted vtable and a slot-0 trap nobody noticed.

The ten the CSV does not have are exactly the `Video::*Data` family lane B2 read
the shape from, at `0x180058590 + 0x80·k`: `RenderBufferData`,
`VideoProgramData`, `PixelProgramData`, `VertexProgramData`, `VertexLayoutData`,
`VertexBufferData`, `IndexBufferData`, `ConstantBufferData`, `ContextData`,
`DeviceData`. Lane B2's stride observation was right and it is the whole tail of
the family.

**The generated `.inc` is ahead of the CSV.** Anything else using
`wave9_data_harvest.csv` as a worklist is short by ten rows in the same way.

## 1a. The twelve `Video::*Data` vtables are now COMPLETE

The lead's priority call was right, and it was already delivered — all twelve
were in this lane's table from the start (§4). Every one of those vtables is
**one unnamed slot 0 plus nothing but `_purecall`**:

```
??_7ConstantBufferData@Video@@6B@   slots= 4  purecall= 3  unnamed=1  named=0
??_7ContextData@Video@@6B@          slots=20  purecall=19  unnamed=1  named=0
??_7DeviceData@Video@@6B@           slots=20  purecall=19  unnamed=1  named=0
??_7ImageBufferData@Video@@6B@      slots= 7  purecall= 6  unnamed=1  named=0
??_7IndexBufferData@Video@@6B@      slots= 7  purecall= 6  unnamed=1  named=0
??_7PixelProgramData@Video@@6B@     slots= 3  purecall= 2  unnamed=1  named=0
??_7RenderBufferData@Video@@6B@     slots= 4  purecall= 3  unnamed=1  named=0
??_7RenderStateData@Video@@6B@      slots= 3  purecall= 2  unnamed=1  named=0
??_7VertexBufferData@Video@@6B@     slots= 7  purecall= 6  unnamed=1  named=0
??_7VertexLayoutData@Video@@6B@     slots= 3  purecall= 2  unnamed=1  named=0
??_7VertexProgramData@Video@@6B@    slots= 3  purecall= 2  unnamed=1  named=0
??_7VideoProgramData@Video@@6B@     slots= 3  purecall= 2  unnamed=1  named=0
```

Filling slot 0 therefore leaves nothing unfilled: **lane C2's "0 of 28 COMPLETE"
should become 12 of 39.** All twelve also have their `~Class` already ported by
lane B2, so both halves are ours.

That is a prediction to check on the run, not an assertion: `[RtMintShadow]
complete vtables` should read **12 of 39**. If it still reads 0, the mint is not
seeing these registrations, and that is a finding about init order rather than
about this table.

## 1b. Slot 0 is `??_E` — and it is neither `??1` nor `??_G`. How to tell.

Three different functions, and the project has now been bitten by the
distinction twice. Stated explicitly because the lead asked for it — and the
answer corrects the request:

| | what it is | how you recognise it | exported |
|---|---|---|---|
| `??1<Class>@@UEAA@XZ` | the **real** destructor | restamps the vtable, destroys members, **no flags argument** | **yes** — lane B2 ported twelve |
| `??_G<Class>@@UEAAPEAXI@Z` | **scalar** deleting destructor | `~Class(this); if (flags & 1) operator delete(this); return this;` — **one** flag bit, no cookie, no iterator | n/a |
| `??_E<Class>@@UEAAPEAXI@Z` | **vector** deleting destructor | **`TEST DL,0x2`**, `MOV R8D,[RCX-8]` (array cookie), `CALL 0x18007ec98`, frees and returns `this-8` | no |

**All 37 are the third.** The tell the lead already had in hand is decisive:
`TEST DL,0x2` **does not occur in `??_G` at all** — a scalar deleting destructor
has no array form and nothing to test bit 1 for. Lane C2 reading `test dl,2` in
four of these bodies is not a curiosity to reconcile with the canonical `??_G`
form; it *is* the proof that they are not `??_G`.

**The flags question has a both/and answer, not an either/or.** The lead asked
which bit decides the free. Both bits are live, for different things:

* **bit 1 (`0x2`) selects the array form** — read the cookie at `this-8`, sweep
  `n` elements, and express both the free and the return value against the
  BLOCK rather than against `this`.
* **bit 0 (`0x1`) is the free**, tested separately inside *both* branches
  (`TEST SIL,0x1` / `TEST DL,0x1`).

So `flags==2` destroys an array without freeing it and `flags==1` frees a single
object: four combinations, four distinct behaviours, and the probe drives all
four (§6.1).

**Lane B2's twelve `??1` ports moving lane C2's filled count by exactly zero is
correct arithmetic, not a measurement error.** The `??1` is the *callee*; slot 0
holds the `??_E` *thunk*. This lane registers the thunks, and because lane B2
already owns the callees, `ResolveAddr` reaches our code on both sides for all
twelve (§4).


## 2. The correction: `??_E`, not `??_G`

The brief asked for the shape to be verified rather than assumed. It does not
hold. Every one of the 37 is MSVC's **vector** deleting destructor:

```
??_E<Class>@@UEAAPEAXI@Z(this, flags):
    if (flags & 2) {                                  // delete[] arrived here
        int n = *(const int32_t*)((char*)this - 8);   // the array cookie
        `eh vector destructor iterator`(this, sizeof(Class), n, ~Class);
        if (flags & 1) operator delete((char*)this - 8);
        return (char*)this - 8;                        // ... and the BLOCK, not this
    }
    ~Class(this);
    if (flags & 1) operator delete(this);
    return this;
```

Read at `0x180001680`:

```
18000168f: TEST DL,0x2
180001692: JZ   0x1800016d1          <- the scalar half the brief described
180001694: MOV  R8D,dword ptr [RCX + -0x8]      count, from the array cookie
180001698: LEA  R9,[0x18009e180]                ~Achievement
18000169f: MOV  EDX,0x58                        sizeof(Achievement)
1800016a9: CALL 0x18007ec98                     eh vector destructor iterator
1800016ae: TEST SIL,0x1
1800016b4: LEA  RCX,[RBX + -0x8]                free the BLOCK
1800016b8: CALL 0x180006a10
1800016bd: LEA  RAX,[RBX + -0x8]                and RETURN the block
```

**Why this matters more than a shape quibble.** Porting the assumed `??_G` would
have produced a function that, on any `delete[]` of a Runtime object:

* destroys **one** element instead of `n`;
* hands `operator delete` a pointer **eight bytes past** the block — in this
  allocator that is a write through the wrong boundary tag, which is
  `reference_load_wild_write_high_dword_7fc.md` and
  `reference_internalfactory_swapremove_wild_write.md` verbatim; and
* returns the wrong pointer to the caller.

And it would have looked **completely correct in every scalar `delete`**, which
is the overwhelming majority of calls. A latent, allocator-corrupting defect
behind a green probe is the exact failure shape this project has paid for twice.

Lane C2's independent read of four bodies — "identical apart from one
RIP-relative `LEA` displacement and the `CALL` target" — holds **within the
`Video::*Data` subfamily**, and only there: those twelve all have
`sizeof(Class) == 0x10`, so the `MOV EDX,imm32` does not vary between them.
Across all 37 it varies from `0x8` to `0x188`, and **22 of the 37 have no `CALL`
in the scalar branch at all** — they inline the destructor instead. Which is why
"four of thirty-six, keep verifying per address" was the right caution, and why
the verification here is mechanical rather than a sample:
`tools/rt_deleting_dtor_verify.py` re-derives all 37 rows from the DLL's machine
code (§6.2).

Two further sub-shape facts, both checked per address rather than assumed:

* **The arity does not vary.** This is where lane Q's `SetSize` correction came
  from (eight two-argument instantiations behind a three-argument typedef), so
  it was checked at all 37: every one reads `RCX` and `EDX` and nothing else.
  `(void* this, unsigned flags)`, uniformly.
* **The three prologue variants are register-allocation noise** — with and
  without the MSVC unwind marker `MOV [RSP+0x20],-2`, and the two orders of
  `LEA R9` / `MOV R8D`. Neither affects the object. (The verifier had to learn
  this the hard way; see §6.)

## 3. Cluster analysis: ONE body, and `operator delete` is not a parameter

All 37 disassembled. They are one compiler-generated body parameterized by
**exactly two constants**: `~Class` and `sizeof(Class)`.

The obvious third candidate parameter is `operator delete`, and it dissolves.
Ten distinct addresses appear across the 37 — `0x180006a10`, `0x1800010d0`,
`0x180001850`, `0x180001ef0`, `0x1800028a0`, `0x180002ee0`, `0x18001e990`,
`0x180020b70`, `0x180045490`, and `0x1800c7ad0` reached from inside two of the
inlined destructors. All ten were disassembled. **All ten are the same
function**, instruction for instruction:

```
if (p) { EnterCriticalSection(&_singleton->cs);
         Free(_singleton, p);
         LeaveCriticalSection(&_singleton->cs); }
```

which is the per-translation-unit copy of `operator delete`, and is byte-for-byte
`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (`0x1800c7ad0`) — the export lane Q
already reaches. So the port calls `rtcoll::MemFree`, reusing lane Q's single
boundary rather than re-deriving it, and **does not take the heap**:
`BANISHED_RT_ALLOC` is lane E's and carries a standing NO-GO. Lane Q's LANE E
DEPENDENCY note covers this file too.

### The one thing that does vary: how the scalar branch reaches `~Class`

| | count | shape |
|---|--:|---|
| **K1** | **15** | `CALL <the same address the array branch puts in R9>` |
| **K2/K3** | **22** | the destructor is **INLINED** into the scalar branch |

All 22 inlined bodies were checked against the destructor their own array branch
names, and in all 22 the inlined code **is** that destructor's body. Worked
example — `Object::CompiledExternal`, `0x18000b7e0`:

```
scalar branch @0x18000b831:   LEA RAX,[0x180154a30]; MOV [RCX],RAX; CALL 0x1800bc950
~CompiledExternal @0x1800bb980: LEA RAX,[0x180154a30]; MOV [RCX],RAX; JMP  0x1800bc950
```

Same for the harder ones: `~Goal` (`0x18009edb0`) is a vtable restamp plus the
`String` release at `+0x28`, and `0x180001d70`'s scalar branch is that release
inlined, `DEC [RAX-4]` for `DEC [RAX-4]`; `~MaterialInstance` (`0x180103a00`)
destroys `+0x50`, `+0x40`, `+0x30` then tail-jumps its base, and `0x180002d60`
inlines exactly those three calls in that order.

**So the port calls `~Class` in both branches and does not reproduce the
inlining.** That is a deliberate departure from byte-faithfulness and it is the
only one in the file: what retail computes is identical; what it costs is one
extra call in 22 of the 37.

### `eh vector destructor iterator` (`0x18007ec98`)

32 instructions, reproduced rather than called so the array branch adds no CRT
dependency that must survive cutover:

```
MOVSXD RDI,R8D        ; count is SIGNED
RBX = base + count*size
DEC EDI / JS out      ; a count of 0 or less destroys NOTHING
SUB RBX,RSI / CALL R14 ; ... and the walk is DOWNWARD
```

Not reproduced: its exception bookkeeping (the `[RSP+0x20]` state that lets a
throwing element destructor continue the sweep). No destructor in this family
throws — all 37 were read and every one ends in a vtable store, a member destroy
or a free — so "unreachable" here is a claim about these 37 callees, which were
read, not about callers.

**Descending order is preserved and a comparator cannot see it.** The elements
do not alias, so any order leaves the same bytes. It is preserved because it
reaches the allocator. Lane D2 had to make the identical statement about J2b;
said again here so nobody reads the probe's green line as proof of the order.

## 4. Per-address parameterization

`ported` = whether `~Class` is already registered by another lane today, in
which case `ResolveAddr` hands back our code with no change here. **Every row in
this table was re-derived from the DLL's machine code by
`tools/rt_deleting_dtor_verify.py`, not transcribed.**

| deleting dtor | class | `sizeof` | `~Class` | scalar | `~Class` ported |
|---|---|--:|---|---|:-:|
| `0x180001680` | `Achievement::Achievement` | `0x58` | `0x18009e180` | K1 | yes |
| `0x180001a90` | `Achievement::AchievementInstance` | `0x28` | `0x18009e400` | inline | **no** |
| `0x180001d70` | `Achievement::Goal` | `0x30` | `0x18009edb0` | inline | yes |
| `0x1800020c0` | `Achievement::GoalInstance` | `0x20` | `0x18009ef40` | inline | yes |
| `0x180002d60` | `Graphics::MaterialInstance` | `0x60` | `0x180103a00` | inline | **no** |
| `0x1800037b0` | `UI::SpriteSheet` | `0x58` | `0x18011d670` | K1 | **no** |
| `0x180006440` | `Object::Internal` | `0x10` | `0x1800bfdb0` | K1 | yes |
| `0x1800065b0` | `Object::Base` | `0x08` | `0x1800c0360` | inline | **no** · probe subject |
| `0x180006640` | `Object::External` | `0x20` | `0x1800bc950` | K1 | yes |
| `0x18000b7e0` | `Object::CompiledExternal` | `0x20` | `0x1800bb980` | inline | **no** |
| `0x18000c610` | `IO::Stream` | `0x18` | `0x1800b9a40` | inline | **no** |
| `0x180011800` | `Graphics::AnimationEvent` | `0x20` | `0x1800a2120` | inline | yes |
| `0x180015e90` | `Graphics::Geometry` | `0xc0` | `0x1800a41c0` | K1 | yes |
| `0x18001ed20` | `Video::ImageBuffer` | `0x68` | `0x1800f2610` | K1 | yes |
| `0x180020160` | `Graphics::Particle` | `0x48` | `0x180103c70` | K1 | **no** |
| `0x180020310` | `Graphics::ParticleInstance` | `0xd8` | `0x1801041f0` | inline | **no** |
| `0x180020ce0` | `Graphics::ParticleSystemInstance` | `0xe0` | `0x1800a5520` | K1 | **no** |
| `0x1800244a0` | `IO::CompressedMemoryStream` | `0x98` | `0x1800a9560` | K1 | yes |
| `0x180031900` | `Engine::UpdateLoop` | `0x28` | `0x1800dddb0` | K1 | yes |
| `0x180035c10` | `UI::Element` | `0xc0` | `0x1801161a0` | K1 | **no** |
| `0x18003b800` | `UI::DialogController` | `0x30` | `0x1800cd2f0` | inline | yes |
| `0x1800458c0` | `UI::DialogInstance` | `0xb8` | `0x180114d40` | K1 | **no** |
| `0x18004d4f0` | `Engine::LoaderLoop` | `0x48` | `0x1800dc170` | K1 | **no** |
| `0x18004d670` | `Engine::DialogLoaderLoopT` | `0x188` | `0x1800db060` | K1 | **no** |
| `0x18004d8b0` | `Engine::DialogLoopT` | `0x168` | `0x1800db340` | K1 | **no** |
| `0x180058590` | `Video::RenderBufferData` | `0x10` | `0x1800f8db0` | inline | yes · probe subject |
| `0x180058610` | `Video::VideoProgramData` | `0x10` | `0x1800f8d90` | inline | yes · probe subject |
| `0x180058690` | `Video::RenderStateData` | `0x10` | `0x1800f8d70` | inline | yes · probe subject |
| `0x180058710` | `Video::PixelProgramData` | `0x10` | `0x1800f8dd0` | inline | yes · probe subject |
| `0x180058790` | `Video::VertexProgramData` | `0x10` | `0x1800f8d10` | inline | yes · probe subject |
| `0x180058810` | `Video::VertexLayoutData` | `0x10` | `0x1800f8d30` | inline | yes · probe subject |
| `0x180058890` | `Video::VertexBufferData` | `0x10` | `0x1800f8d50` | inline | yes · probe subject |
| `0x180058910` | `Video::IndexBufferData` | `0x10` | `0x1800f8df0` | inline | yes · probe subject |
| `0x180058990` | `Video::ConstantBufferData` | `0x10` | `0x1800f8e50` | inline | yes · probe subject |
| `0x180058a10` | `Video::ImageBufferData` | `0x10` | `0x1800f8e10` | inline | yes · probe subject |
| `0x180058a90` | `Video::ContextData` | `0x10` | `0x1800f8e30` | inline | yes · probe subject |
| `0x180058b10` | `Video::DeviceData` | `0x10` | `0x1800f8cf0` | inline | yes · probe subject |

**23 ported, 14 not.** The 14 are §7.

### The mangling hazard, and why it does not apply

`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md` is about a `??1` spelled
`QEAA` when it is `UEAA`, resolving NULL silently behind a guard. **All 37 are
registered by ADDRESS**, because all 37 are unnamed in the export directory —
which is exactly why lane C2 could only trap them. There is no name to get
wrong. `tools/export_name_audit.py` has nothing to check here, and that absence
is the finding: it would apply the moment someone tried to register these by
name, and the spelling is `??_E<Class>@@UEAAPEAXI@Z` — not `??_G`, not `??1`,
and the DLL exports none of the three.

Audit results after this lane, all unchanged from before it:

```
export_name_audit.py        662 names, 1 not in the DLL  (the `?Mangled@...` doc example in resolve.h, pre-existing)
abi_retslot_audit.py        0 suspicious of 48
resolve_fallback_audit.py   0 fallbacks
port_bypass_audit.py        ADDRESS door 0 bypasses; NAME door 0 bypasses
```

### `~Internal` is in this set

`0x180006440` calls `0x1800bfdb0`, which is the swap-remove through the handle
table that `reference_internalfactory_swapremove_wild_write.md` is about. This
lane does not change that in either direction — it calls `~Internal` exactly
where retail calls it, on both branches — but it is now on the path, so a defect
here would present as that failure family. Recorded because the brief asked.

## 5. Files

| File | What |
|---|---|
| `runtime/src/object/deleting_dtor.cpp` | the body, the 37-row table, the 37 entry points, the 37 registrations, `RtObjectDeletingDtorPortInit()` |
| `runtime/src/object/deleting_dtor_internal.h` | what the port and the probe must agree on |
| `runtime/src/object/deleting_dtor_probe.cpp` | the oracle's body half: synthetic subjects, three controls |
| `tools/rt_deleting_dtor_verify.py` | the oracle's row half: re-derives all 37 rows from the DLL |
| `tools/gen_deleting_dtor.py`, `tools/emit_deleting_dtor_cpp.py` | emit the repetitive halves of the `.cpp` once, so the rows / entry points / registrations cannot drift apart |
| `runtime/funcs/rt_port.h`, `standalone/resolve_standalone.cpp` | `RtObjectDeletingDtorPortInit()` declared and called |

No `Makefile` change: `runtime/src/*/*.cpp` is globbed and the per-directory
compile rule is generated from what is on disk. `runtime/src/object/` already
exists, deliberately chosen over a new directory for that reason.

**Init order.** `RtObjectDeletingDtorPortInit()` runs **after** every other
`*_PortInit()` (including lane Q's and lane D2's) and **before**
`RtDataMintInit()`. After, because it resolves each `~Class` through the
port-aware `ResolveAddr` and its probe runs at the end of it, so a destructor
registered by a later init would be invisible to the probe. Before, because the
mint is what consumes these registrations.

**Kill switches.** `BANISHED_RT_DELDTOR=0` leaves all 37 on the DLL.
`BANISHED_RT_DELDTOR_PROBE=1|neg|iso|all` drives the probe; off by default.

## 6. The oracle — two halves, because one cannot cover both risks

A deleting destructor cannot be replayed at a live call site: ours then theirs
destroys twice and frees twice, and run on the post-state the second run takes
the torn-down path, **compares nothing and reports green**. Lane B2 hit exactly
that with `~DisplayMode` — the `Scene::Pause` early-out shape. The brief's
warning that "a destructor probe that reports 37 green no-ops is worse than
none" is the design constraint, so the risk is split in two.

### 6.1 The BODY — `deleting_dtor_probe.cpp`

Synthetic subjects (`reference_synthetic_subject_beats_a_fake.md`, lane Q's
design): two private, identical objects in **file-scope** storage, ours run on
one and the **shipped body** on the other, bytes compared. The shipped body is
reached through `ResolveRuntimeRaw`, never `ResolveAddr` — this lane registers
all 37 addresses, so `ResolveAddr` would compare ours to ours and always agree
(`reference_resolveaddr_consults_port_registry_first.md`).

**13 of 37 subjects, and why that is not the coverage hole it reads as.** A
subject can only be fabricated if `~Class` is safe on fabricated bytes. It is
for 13 — `Object::Base` and the twelve `Video::*Data`, whose destructor is a
pure vtable restamp (`LEA RAX,[vt]; MOV [RCX],RAX; RET`). The other 24 destroy
members, release `String` references, or swap-remove a handle-table entry;
running one on a synthetic object would fabricate a crash, not a measurement.
Since **all 37 share one body**, 13 subjects verify the body. What they cannot
verify is the other 24 **rows** — which is §6.2.

**6 cases × 13 subjects = 78 comparisons**, of which 52 run today:

| case | flags | cookie | what it pins |
|---|--:|--:|---|
| `scalar-destroy` | 0 | — | the scalar branch; return offset 0 |
| `array-destroy-1` | 2 | 1 | the array branch at n=1; return offset −8 |
| `array-destroy-3` | 2 | 3 | the stride and extent |
| `noop-array-0` | 2 | 0 | the `DEC EDI; JS out` edge — **counted as ineffective on purpose** |
| `scalar-delete` | 1 | — | opt-in (`=all`); needs the Runtime heap |
| `array-delete-3` | 3 | 3 | opt-in (`=all`) |

**Compared:** the whole subject block — the cookie plus three elements' storage,
pre-filled with a fixed sentinel — plus the returned pointer as an offset from
`this`. Comparing the extent *past* the destroyed elements is deliberate and is
**not** `reference_oracle_must_not_compare_unwritten_bytes.md`: there, the
unwritten bytes held allocator fill that differed run to run; here the probe owns
the storage and puts the same sentinel in both, so "neither side wrote here"
passes for a reason, and an over-long walk is caught by it.

**Not compared:** whether `operator delete` was called and with what. The
comparator cannot see a free. That half is covered by pairing a run with
`BANISHED_REDZONE=watch` — the same thing lane Q had to say about `~Array`.

**Three controls.**

* **Effectiveness.** Every case counts, **from the bytes**, how many element
  slots stopped holding the sentinel. `did-work` and the element total are
  printed; zero prints `*** EFFECTIVENESS CONTROL FAILED ... This run proves
  NOTHING ***` in four lines of asterisks. Expected: **39 of 52 did-work**
  (three effective cases × 13 subjects), 156 elements destroyed. The 13
  `noop-array-0` cases are ineffective by design and counting them would inflate
  the number that exists to catch inflation
  (`reference_probe_needs_an_effectiveness_control.md`).
* **Negative** (`=neg`). A deliberately wrong envelope through the same
  comparator: destroys `count − 1` elements and returns `this` instead of
  `this − 8` on the array path — **precisely the two defects the assumed `??_G`
  shape would have introduced**. It must be flagged on all three array cases of
  every subject: **39 of 39**. Fewer prints `*** NEGATIVE CONTROL FAILED ... the
  green result above is worthless ***`.
* **Isolation** (`=iso`), a discriminator rather than a control. 12 of the 13
  subjects have a `~Class` that is **already our port** (lane B2's), so a red
  comparison could belong to that lane. `=iso` re-runs every case with the
  **shipped** `~Class` forced into our body. Red normally + green under `iso` =
  a destructor-port defect, not an envelope defect. It is here so a lane-boundary
  bisection costs one run instead of an escalation.

**Scheduling, and why it differs from lane Q's.** The byte-comparing cases run
from `RtObjectDeletingDtorPortInit()`. Lane Q's probe self-schedules on the first
call into a registered entry point because the Runtime allocator is
*constructed but not started* at init; this probe may run at init because it
**allocates nothing**, and it *must*, because nothing calls a deleting
destructor before cutover so that first call never arrives. The two allocating
cases self-schedule and are opt-in, and **today they will not run at all**.

**The probe registers nothing with `rt_diff.cpp`.** It drives
`DeletingDtorCoreWith` directly and the diff reporting lives in the wrapper the
entry points call, so probe traffic cannot inflate `calls=`. Lane Q's first draft
passed stack-local `RtDiffSite`s and the summary reported 144 of them as "a stray
write hit the harness's own statics"; the rule (*a `RtDiffSite` must have static
storage duration*) is restated in both new files, and every `Site` here is
file-scope.

### 6.2 The 37 ROWS — `tools/rt_deleting_dtor_verify.py`

The realistic per-address failure is a **transcription error in one row**, and a
runtime probe on 13 subjects cannot see it. So the verifier re-reads all 37
bodies out of the shipped DLL, extracts `~Class` and `sizeof(Class)` from the
machine code, and checks them against the table in `deleting_dtor.cpp` — which
it parses out of the source. It also checks, per address, that the count comes
from `[this−8]`, that the iterator called is `0x18007ec98`, that the free target
has the `operator delete` shape, that the free is conditional on bit 0, that the
block pointer is what is freed and returned, and that the scalar branch reaches
the same destructor (by call, or by restamping the vtable that destructor
restamps).

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_deleting_dtor_verify.py
    rows in deleting_dtor.cpp: 37
    OK  ... × 37
    call-the-dtor (K1): 15    restamp-inline (K2/K3): 22
    problems: 0                                            exit 0

tools/... tools/rt_deleting_dtor_verify.py --neg
    NEGATIVE CONTROL: 0x180058b10's element size perturbed 0x10 -> 0x18
    BAD 0x180058b10 ... elemSize: DLL says 0x10, table says 0x18
    problems: 1
    NEGATIVE CONTROL PASSED: the perturbed row was caught.  exit 0
```

**No build and no game run needed.** Both are already green as of this lane.

#### Two defects the verifier had, kept because they generalise

1. **A raw byte compare of the ten `operator delete` copies failed all 37 rows.**
   They *are* the same function — but they differ in the RIP-relative
   displacements of `MOV RBX,[_singleton]` and the two IAT calls, because they
   sit at different addresses. This is exactly why lane D had to hash
   *mnemonics* rather than bytes, and the checker walked straight into it while
   checking a family that had been clustered by mnemonic hash. The fix resolves
   each RIP operand to its absolute target and compares those.
2. **A bare opcode scan invented a second `MOV EDX,imm32`.** `CALL 0x18007ec98`
   from `0x18004d8b0` encodes as `E8 BA 13 03 00`, and the `BA` inside that
   displacement read as an instruction. Every field the verifier extracts is now
   **anchored to a preceding instruction** rather than found by scanning for an
   opcode byte. *Bytes are not instructions until something says where they
   start* — the same lesson as
   `reference_ghidra_c_output_vs_disassembly.md`, one level down.

Both are corrected in the tool and commented there.

## 7. Coverage gaps, stated plainly

### 7.1 Expect `calls=0` on all 37, and it is not a defect

**Nothing in our tree calls a deleting destructor today.** They live in the DLL's
own vtables and the DLL dispatches through them directly, which never consults
the port registry. So the `[RtDiff]` `calls=` column will read 0 for all 37 until
lane C2's mint is armed and our vtables are the only ones there are.

`reference_uncalled_port_is_not_verified.md` would normally stop a lane here. It
does not, because the probe does not depend on a live caller — but the same fact
has a visible consequence in the census (§7.3) and it is the reason the two
allocating probe cases cannot run today.

### 7.2 The 14 destructors this lane makes newly `called` — lane B's worklist

`reference_porting_can_grow_the_cutover_surface.md`. Each becomes ours with **no
source change here** the moment lane B registers it:

| `~Class` | class | note |
|---|---|---|
| `0x1800c0360` | `Object::Base` | **3 instructions** — `LEA RAX,[0x180154788]; MOV [RCX],RAX; RET` |
| `0x1800bb980` | `Object::CompiledExternal` | restamp `0x180154a30` + tail-jump `~External`, which is **already ported** |
| `0x1800b9a40` | `IO::Stream` | restamp `0x1801549a0` + `if (p = this[0x10]) MemFree(p)` |
| `0x18009e400` | `Achievement::AchievementInstance` | restamp + member dtor at `+0x10` + tail-jump `~Internal` |
| `0x180103a00` | `Graphics::MaterialInstance` | restamp + three member dtors + tail-jump `~External` |
| `0x18011d670` | `UI::SpriteSheet` | not read past its entry |
| `0x180103c70` | `Graphics::Particle` | not read past its entry |
| `0x1801041f0` | `Graphics::ParticleInstance` | restamp + `0x1801040b0` + tail-jump `~Geometry` (ported) |
| `0x1800a5520` | `Graphics::ParticleSystemInstance` | not read past its entry |
| `0x1801161a0` | `UI::Element` | not read past its entry |
| `0x180114d40` | `UI::DialogInstance` | not read past its entry |
| `0x1800dc170` | `Engine::LoaderLoop` | not read past its entry |
| `0x1800db060` | `Engine::DialogLoaderLoopT` | not read past its entry |
| `0x1800db340` | `Engine::DialogLoopT` | not read past its entry |

The **first three are one line each** and were deliberately **not** taken: lanes
are writing concurrently this round and a duplicate registration would shadow
lane B's (`tools/dispatch_slot_map.py --check`'s SHADOWED shape, one door over).
They are the cheapest three rows on lane B's list.

**These 14 are invisible to `tools/runtime_census.py`.** The port reaches them
through `ResolveAddr(Module::Runtime, kSpecs[i].dtorVa)` — a table field, not a
literal — and the census's `called` scan matches literal
`ResolveAddr(Module::Runtime, 0x…)` only. Same mechanism as
`reference_census_cannot_see_nonliteral_registrations.md`, in its `called` form.
The 14 addresses are listed above so the next census does not have to rediscover
them. The 37 **registrations** are all literals and are seen.

### 7.3 The census moves by +1, and 36 new orphans appear

```
before lane K:  internal 70 called, 38 ported    TOTAL 843 called, 585 ported  69.4%
after  lane K:  internal 70 called, 39 ported    TOTAL 843 called, 586 ported  69.5%
```

That is **correct, not a bug in this lane or in the census.** The census's ported
column iterates over *called* addresses, and 36 of the 37 have no caller in our
tree (§7.1). They now appear in the "ports with no caller" list as
`addr 0x180001680` … `addr 0x180058b10`. Only `0x1800244a0` moved the counter,
because `runtime/src/io/compressedstream.cpp` already resolves it by literal.

**Written verdict for those 36**, per the convention the census's own text asks
for (*"a NEW name appearing here has NOT been triaged"*):

> All 36 are the unnamed deleting destructors of Runtime vtables, registered by
> lane K. They have no caller in our tree **by construction**: the DLL's own
> vtables dispatch slot 0 directly and never consult the port registry. They
> acquire callers at cutover, when lane C2's minted vtables are armed and hold
> these registrations in slot 0. Their bodies are verified by
> `runtime/src/object/deleting_dtor_probe.cpp` (13 subjects, one shared body,
> two controls) and their 37 rows by
> `tools/rt_deleting_dtor_verify.py` (0 problems, negative control passes) —
> neither of which needs a caller. This is the cutover-readiness case, not a
> vestigial resolve.

### 7.4 What the probe does NOT cover

1. **The 24 non-restamp destructors as subjects.** Not fabricable; covered as
   *rows* by the verifier, not as *runs*. The BODY they share is verified.
2. **The free.** `operator delete` is invisible to the comparator (§6.1).
   `BANISHED_REDZONE=watch` is the cover, and only on a run where something
   actually calls one — which today is no run.
3. **The two allocating cases** (`scalar-delete`, `array-delete-3`) will not run
   before cutover, because they self-schedule on the first call into a
   registered entry point and there is none. The probe says so in its summary
   rather than leaving a silent zero.
4. **Destruction order.** Descending, preserved, and unobservable to any
   comparator (§3).
5. **The exception path** through `eh vector destructor iterator`. Not
   reproduced; justified in §3 by having read all 37 callees.

## 8. What the integrator needs to run

**R1 is the one that verifies this lane, and it needs no game run.**

| # | Command | Expected |
|---|---|---|
| **K0** | `tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_deleting_dtor_verify.py` | `rows: 37`, 37 × `OK`, `K1: 15  K2/K3: 22`, `problems: 0`, exit 0 |
| **K0n** | same, `--neg` | `problems: 1`, `NEGATIVE CONTROL PASSED`, exit 0. **Run this first** — a verifier nobody has shown a wrong row to is not a verifier. |
| **K1** | `BANISHED_RT_DELDTOR_PROBE=neg BANISHED_AUTOQUIT=1 make sa-run`, then `grep RtDelDtorProbe "C:/GOG Games/Banished/standalone_error.log"` | `static: 52 comparisons, 0 MISMATCH, 39 did-work (65 elements destroyed)`; `26 allocating cases NOT RUN`; `negative control: 39 wrong-body cases, 39 flagged`. **If the negative control does not fire, stop — nothing else here means anything.** |
| **K2** | `BANISHED_RT_DELDTOR_PROBE=iso` on the same run | `isolation: 52 cases re-run with the SHIPPED ~Class, 0 disagreed`. Only needed if K1 is red; it says whether the defect is this envelope or lane B2's destructor ports. |
| **K3** | `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 make sa-run` | **`calls=0` on all 37 is the EXPECTED result** (§7.1). A non-zero `calls=` would be news and worth reading — it would mean something in our tree reaches a deleting destructor before cutover, which this lane's analysis says nothing does. |
| **K4** | K1 with `BANISHED_RT_DELDTOR=0` | The A/B. `[RtDelDtor] DISABLED …`, no probe lines, and whatever K1 showed must disappear. |
| **K5** | `tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py` | `internal 70/39`, `TOTAL 843/586`, and **36 new `addr` lines in "ports with no caller"** — §7.3 is their written verdict. |

`BANISHED_AUTOQUIT=1` quits at the **main menu**
(`reference_rt_strict_residue_census.md`), which is fine for K1: the probe runs
at `RuntimePortInit`, long before the menu, and needs no gameplay.

**Do not set `BANISHED_RT_MINT=1`.** It is lane C2's cutover flag; this lane
fills 37 of its slots but does not arm it.

### Bisection

`BANISHED_RT_DELDTOR=0` first (all 37 off). If a failure survives that, it is not
this lane. There is no finer switch because there is only one body.
