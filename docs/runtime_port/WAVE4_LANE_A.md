# Wave 4, Lane A — IO::Serializer

Following [PLAN.md](PLAN.md) §4.2 and §6, and modelled on
[WAVE3_LANE_A.md](WAVE3_LANE_A.md). Lane A owns the `IO::Serializer` surface our tree reaches,
plus the layouts of the four classes it touches.

Files owned and written by this lane:

| File | What |
|---|---|
| `runtime/types/io/Serializer.h` | new — `Serializer`, `Stream` (base), `Encoding`, `ReaderWriter`, `BinaryCodec`, the retail vtable offset tables |
| `runtime/src/io/serializer.{h,cpp}` | the 22 ports + `RtIoSerializerRegister()`; rewritten from the 2025-era file that was here |
| `docs/runtime_port/WAVE4_LANE_A.md` | this file |

No file outside that set was edited. Everything this lane found that needs changing elsewhere is
in [§8](#8-comments-to-correct-for-the-orchestrator).

---

## 1. Result

| | |
|---|---|
| Ported | **22** (the 19 the brief scoped + `Serialize(n,char*)`, `Serialize(n,signed char*)`, and `Serialize(n,short*)` which the brief's list omitted) |
| Held back | 6 `IO::Serializer` exports our tree does not reach — see [§6](#6-held-back-and-why) |
| Registration count | `RtIoSerializerRegister()` returns **22** |
| Census, IO namespace | `IO 30/41 called = 73.2%`, up from 11/41 before this lane (the other 19 are lanes B/C/D's). TOTAL `205/879 = 23.3%` |
| Diff sites added | **22** — 2 full oracles, 1 real shadow, 18 slot-proved partials, 1 with no oracle at all |
| Defects found | **4**, three of them pre-existing and live in the tree today ([§7](#7-defects)) |
| Measured in a run | **nothing** |

**Nothing in this lane has executed.** No build was run and the game was not launched — the
orchestrator owns both. Every claim below is "this is what the shipped DLL's bytes say and this
is what the port does", never "this was observed agreeing". WAVE2 finding 5 stands: a
registration is not a port until it runs.

Two things were run, both read-only and both against the shipped binary rather than our exe:
`tools/runtime_census.py` and `tools/export_name_audit.py`, plus a `pefile` read of
`C:\GOG Games\Banished\Runtime-x64.dll`.

---

## 2. Export names — all 22 verified, none phantom, none folded

`reference_resolveexport_phantom_names.md`: a `ResolveExport` name the DLL does not contain
returns NULL silently, the `ResolveAddr` fallback hides it, and the port then registers under a
name nothing resolves — census counts it, diff says `calls=0`.

**The 19 names in this lane's brief are covered centrally by
[`wave4_export_names.txt`](wave4_export_names.txt)** — 39 Wave-4 IO symbols checked against the
shipped `Runtime-x64.dll` export directory with `pefile`, 39 ok / 0 missing, address and ordinal
per name. This lane checked the same 19 independently before writing any body and got byte-identical
addresses and ordinals; cite the central file, not this paragraph.

Three names this lane ports are **not** in that file, because the brief's list omitted them (they
have no `ResolveExport` site — they are reached only through exe dispatch slots, see §5). They were
verified the same way:

```
OK  0x180034fd0 ord=3744  ?Serialize@Serializer@IO@@QEAAXHPEADPEBD@Z    (n, char*)
OK  0x18007c3f0 ord=3743  ?Serialize@Serializer@IO@@QEAAXHPEACPEBD@Z    (n, signed char*)
OK  0x18007c400 ord=3746  ?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z    (n, short*)
```

`tools/export_name_audit.py` reports 20 phantom names project-wide; **none of them is an
`IO::Serializer` name.**

### No ICF/COMDAT folding — 22 names, 22 distinct bodies

The scattered addresses (`0x1800061d0/e0/f0` sit together, everything else is spread across the
image) look like folding but are not. Checked over the whole export directory: **all 22 addresses
are distinct, and not one of them is exported under more than one name.** So "22 ported" means 22
bodies, and no body has to land under a second name. This matches the orchestrator's finding for
the wave's other 38 addresses and extends it to this lane's three extras.

### Hidden return slots — checked per name, none present

None of the 22 returns a class by value: the manglings end `X` (void), `H` (int),
`AEBVEncoding@2@` (a plain reference, i.e. a pointer), or are ctor/dtor. So none carries the RDX
hidden return slot that made `fn_Math_Transform3_Identity` a wild write
(`reference_hidden_retslot_wild_write.md`). `GetEncoding` in particular returns `const Encoding&`
— a pointer in RAX, **not** a hidden slot; the `?A` that marks a by-value class return is absent.

One ABI detail that is live: the **ctor returns `this` in RAX** (`MOV RAX,RBX` at `0x1800b998b`
and `0x1800b999b`), as every MSVC ctor does. `engine/funcs/engine_funcs.h:2562` types it
`void (*)(void*, void*, int)`, which is harmless for a caller that ignores RAX; the port returns
`this` anyway. See §8 for the comment fix.

---

## 3. The layouts, and what pins each offset

Full commentary with the instruction for every field is in `runtime/types/io/Serializer.h`.
Everything below was read out of **retail** `Runtime-x64.dll`. The profile build was not used for
a single number — see §4.

### `IO::Serializer` — 0x10 bytes

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `Stream* stream` | ctor `MOV [RCX],RDX` (`0x1800b9966`); `?GetStream@Serializer@IO@@` (`0x18000a0a0`) is `MOV RAX,[RCX]; RET` |
| 0x08 | `ReaderWriter* readerWriter` | ctor `MOV [RBX+0x08],RAX`; every forwarder opens `MOV RCX,[RCX+0x08]` |

Wave 3 lane A's pinning is re-confirmed, including its naming: **`+0x08` is a `ReaderWriter*`, not
an `Encoding*`.**

### `IO::Stream` (base sub-object only — the concrete family is lane B)

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | vtable | base ctor `0x1800b99d0` stamps `??_7Stream@IO@@6B@` (`0x1801549a0`) |
| 0x08 | `int32 direction` | base ctor zeroes it; `?GetDirection@Serializer@IO@@` (`0x180006210`) is `MOV RAX,[RCX]; MOV EAX,[RAX+8]; RET`; the Serializer ctor branches on it |
| 0x10 | `Encoding* encoding` | base ctor nulls it; `?SetEncoding@Stream@IO@@` (`0x1800b99f0`) writes it, `?GetEncoding@Stream@IO@@` (`0x1800b9a60`) reads it |

`direction == 0` is **reading**: that is the branch the Serializer ctor takes to the
`CreateReader` slot, and the reader is the side that calls `Stream::Read`.

### `IO::ReaderWriter` — the class that was conflated with `Encoding`

`engine/types/io_serializer.h` calls both `serializer+0x08` and `stream+0x10` an `Encoding*` and
describes the second as "the encoding factory". They are two different classes and the export
table says so:

* **`IO::Encoding`** is `stream+0x10`. It is the class named by the mangling
  `?GetEncoding@Serializer@IO@@QEAAAEBV**Encoding**@2@XZ` and installed by
  `?SetEncoding@Stream@IO@@QEAAXPEAVEncoding@2@@Z`. It is the encoding *policy* object; its
  vtable slot 3 (`+0x18`) makes a reader and slot 4 (`+0x20`) makes a writer. It has **no**
  non-virtual exported members at all, which is why a search for `Encoding@IO` in the export
  table comes back with only those three `Get/SetEncoding` rows.
* **`IO::ReaderWriter`** is `serializer+0x08`, vtable `??_7ReaderWriter@IO@@6B@` = `0x1801553b0`,
  ctor `??0ReaderWriter@IO@@QEAA@AEAVSerializer@1@_N@Z`. Concrete subclasses `IO::BinaryReader`
  (`0x1801552e0`) and `IO::BinaryWriter` (`0x180155210`). This is the object every forwarder
  tail-jumps through.

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | vtable | `BinaryReader::BinaryReader` stamps `0x1801553b0` then its own |
| 0x08 | `bool allowCustomSerialize` | ctor `MOV byte [RCX+0x08],0`; `?AllowCustomSerialize@ReaderWriter@IO@@` (`0x1800b9100`) |
| 0x10 | `Serializer* serializer` | ctor `MOV [RCX+0x10],RDX` |
| 0x18 | `Stream* stream` | ctor `MOV RAX,[RDX]; MOV [RCX+0x18],RAX` — i.e. `serializer->stream` |

`BinaryReader`/`BinaryWriter` extend it to 0x30 bytes (the factory asks the allocator for exactly
`0x30`): `+0x20`, `+0x24`, and **`+0x28` = the version**, which is what
`?GetVersion@BinaryReader@IO@@UEBAHXZ` (`0x180023930`, `MOV EAX,[RCX+0x28]; RET`) returns.

### The `ReaderWriter` vtable — DOUBLY pinned, retail only

Two independent sources, agreeing on every entry:

1. each `IO::Serializer` forwarder's own tail-jump displacement, decoded from the shipped DLL's
   bytes;
2. the contents of `??_7BinaryReader@IO@@6B@` and `??_7BinaryWriter@IO@@6B@`, where the slot at
   that offset resolves to the exported `?Serialize@Binary{Reader,Writer}@IO@@` whose *mangled
   parameter type matches the forwarder that jumps there*.

| Off | Signature | Off | Signature |
|---|---|---|---|
| 0x00 | scalar-deleting dtor | 0x70 | `(char&, name)` |
| 0x08 | `GetVersion()` | 0x78 | `(bool&, name)` |
| 0x10 | `Skip()` | 0x80 | `(float&, name)` |
| 0x18 | `(n, char*, name)` | 0x88 | `(uint64&, name)` |
| 0x20 | `(n, bool*, name)` | 0x90 | `(int64&, name)` |
| 0x28 | `(n, float*, name)` | 0x98 | `(uint&, name)` |
| 0x30 | `(n, uint64*, name)` | 0xA0 | `(int&, name)` |
| 0x38 | `(n, int64*, name)` | 0xA8 | `(ushort&, name)` |
| 0x40 | `(n, uint*, name)` | 0xB0 | `(short&, name)` |
| 0x48 | `(n, int*, name)` | 0xB8 | `(uchar&, name)` |
| 0x50 | `(n, ushort*, name)` | 0xC0 | `(signed char&, name)` |
| 0x58 | `(n, short*, name)` | 0xC8 | `SerializeBytes(size, void*, name)` |
| 0x60 | `(n, uchar*, name)` | | |
| 0x68 | `(n, signed char*, name)` | | |

`BinaryWriter`'s vtable is 26 slots (`0x180155210 + 0xd0 == 0x1801552e0`, where `BinaryReader`'s
begins); `BinaryReader`'s is 27.

---

## 4. The three warnings, and what happened to each

### Warning 1 — the profile build's vtable offsets

`reference_encoding_vtable_offsets_differ_between_builds.md`: bool `+0x78` / uint `+0x98` / int
`+0xa0` in retail versus `+0x90` / `+0xb0` / `+0xb8` in `Runtime-x64-profile.dll`. A body
transcribed from a profile decompile writes the right values through the wrong slots — it does not
crash, it desyncs the save stream, and the damage surfaces later on an innocent record.

**The profile DLL was not opened for this lane at all.** Every offset came from retail bytes, and
the retail vtable dump above confirms them a second time.

That is not enough on its own, because it is a static claim about a binary a user could replace.
So the port also **checks itself against the shipped DLL at run time, on every run, not only under
`BANISHED_RT_DIFF`.** Every forwarder in the DLL is exactly three instructions:

```
48 8B 49 08           MOV RCX, [RCX+0x08]      ; rw = self->readerWriter
48 8B 01              MOV RAX, [RCX]           ; rw->vtable
48 FF 60 <disp8>      JMP  [RAX + disp]        ; tail-call, arguments pass through
  or 48 FF A0 <disp32>
```

so the DLL states its own slot number in its own bytes. `RtIoSerializerRegister()` decodes the
displacement out of all 18 tail-jump forwarders and compares it with the constant this port
compiles in, and byte-compares the three non-tail-jump bodies (ctor, dtor, `GetEncoding`) against
the retail prologues. **If any check fails, nothing is registered and the whole family stays on
the DLL** — a refusal, not a warning, because a wrong slot is silent. This is the strongest
available answer to warning 1 and it costs one pass over ~200 bytes at init.

### Warning 2 — phantom `ResolveExport` names

Handled in §2. All 22 verified present before a body was written.

### Warning 3 — read `calls=`, not `divergences=`

§5 states per function how our tree reaches it. Every one of the 22 has a live consumer.

---

## 5. Registration, and the two-mechanism question the brief asked about

### The decision: **the runtime-export registry is the mechanism; the 14 exe dispatch slots stay, bound to the same function pointers.**

They are not two redirections of one class. They are one implementation with two entry points:

* `RegisterRuntimeExport(<mangled>, &RtSerializer_X)` flips every `ResolveExport` site — the
  `fn_*` table in `engine/funcs/engine_funcs.cpp` (lines 3533, 3535, 3537, 3539, 3541, 4922, 4998,
  5005) and the `RegisterDispatchSlot(slot, ResolveExport(...))` bindings in
  `standalone/address_registry.cpp` (`0x140152ad8` ctor, `0x140152ad0` dtor, `0x140151458`
  `(n,bool*)`, `0x140151ea8` `(n,float*)`, `0x140152468` `(short&)`).
* `RegisterDispatchSlot(slot, &RtSerializer_X)` binds the 14 slots that are written as a direct
  function-pointer registration rather than through `ResolveExport`. Those slots are real PE
  imports and the exe's retail-shaped code reads `*(fn**)slot` directly — it never consults
  `ResolveExport`. **Deleting those 14 lines would leave the slots null and EXEC-of-0 at the
  first save**, because PLAN §1.1's housekeeping already removed the earlier shadowed bindings
  that used to back them. So they stay.

Ordering makes this consistent with no work: `RuntimePortInit()` (`standalone/main.cpp:1614`) runs
before `AddressRegistryInit()` (`:1626`) and `EngineFuncsInit()` (`:1631`), so the five
`ResolveExport`-bound slots and the whole `fn_*` table pick up the ports too.

### `BANISHED_REIMPL_SERIALIZER` is retired, and why the kill switch had to move into the body

The 2025-era gate defaulted **off**, so those 14 slots have been running the DLL all along. It is
replaced by `BANISHED_RT_IO` / `BANISHED_RT_SERIALIZER`, which default **on**.

The kill switch is tested **inside each body**, not expressed as "do not register". It has to be:
`AddressRegistryInit()` binds those 14 slots to these function pointers unconditionally, so
skipping registration would kill only the `ResolveExport` half and leave the slot half running the
port with no way to turn it off. A single `g_portOn` bool — set true by
`RtIoSerializerRegister()` and only after every verification passes — kills both halves together.
With `BANISHED_RT_SERIALIZER=0` every one of the 22 tail-calls the shipped DLL through
`RT_ORIGINAL_ONCE`, which is exactly the old default behaviour.

`g_portOn` is all-or-nothing on purpose. These 22 share one stream; a mixed state where some
records go through our slot table and some through the DLL's is the half-ported-class hazard
PLAN §4.1 decision 5 names.

### Registered BY NAME only

`call_surface_20260906.csv` lists all of them as `ResolveExport` rows and not one as a
`ResolveAddr(Module::Runtime, …)` row, so there is no address site to protect. (Wave 3 lane A
registered addresses as well because `engine_funcs.cpp` had real `ResolveAddr` fallbacks for its
six; this lane has none.)

**The registrations are written as 22 literal calls rather than a loop over the verification
table, and that is load-bearing.** `tools/runtime_census.py` scans the source for
`RegisterRuntimeExport("<literal>"` — written as a table-driven loop, this whole lane reported as
**3** ported functions instead of 22, and the wave would have looked half-landed. That was caught
by running the census, which is exactly what
`reference_uncalled_port_is_not_verified.md` says to do.

### Who calls each, and by which mechanism

No site should report `calls=0`.

| Function | Reached through |
|---|---|
| ctor / dtor | `g_fn_Serializer_Ctor` / `_Dtor` (`engine_funcs.cpp:3533,3535`) **and** slots `0x140152ad8` / `0x140152ad0` |
| `GetVersion` | `g_fn_Serializer_GetVersion` (`:3541`) and slot `0x140152a48` |
| `GetEncoding` | `fn_Serializer_GetEncoding` (`:4998`) and `standalone/reg_unreg_lane1.cpp:148` |
| `Serialize(int&)` | `engine_funcs.cpp:3537`, slot `0x140152ac8` — the hottest in the lane |
| `Serialize(bool&)` | `:3539`, slot `0x140152a58` |
| `Serialize(float&)` | `:4922`, slot `0x140152a60` |
| `Serialize(uint&)` | `:5005`, slot `0x140152a50`, `code/Game/Game/namedatabase.cpp` |
| `Serialize(uchar&)` | slot `0x140152410`, `code/Game/Action/completeroad.cpp`, `namedatabase.cpp` |
| `Serialize(signed char&)` | `code/Game/Game/gamegrid_serialize.cpp`; slot `0x140152418` **is bound to the wrong wrapper today** — defect 1 |
| `Serialize(short&)` | slot `0x140152468` (via `ResolveExport`) |
| `Serialize(ushort&)` | `gamegrid_serialize.cpp` |
| `Serialize(n, int*/ushort*/uchar*)` | `namedatabase.cpp`, slots `0x140151278` / `0x140152488` / `0x140152400` |
| `Serialize(n, uint*/bool*)` | `code/Game/Game/gameupdateloop_serialize.cpp`, slot `0x140151458` |
| `Serialize(n, float*)` | slot `0x140151ea8` |
| `Serialize(n, short*)` | slot `0x1401523f8` |
| `Serialize(n, char*)` | slot `0x140152490` — **wrong wrapper today**, defect 2 |
| `Serialize(n, signed char*)` | slot `0x140152478` — **wrong wrapper today**, defect 3 |
| `SerializeBytes` | `namedatabase.cpp`, slots `0x140152618` / `0x140152478`(sic — defect 3) |

**A census blind spot worth knowing:** `Serialize(n,char*)`, `Serialize(n,signed char*)` and
`Serialize(n,short*)` show up in `runtime_census.py --unported` under "Ports with no caller in our
tree". They *do* have callers — through exe dispatch slots bound by direct function pointer, which
the census's `ResolveExport`-literal scan cannot see. Do not delete them as vestigial.

### No detour is installed

As in Waves 1–3. The DLL's own internal calls to these 22 still land in the DLL, and that is safe
here for the same reason it was safe for Wave 3 lane A: these ports keep no replica. They read the
live `Serializer`/`Stream`/`ReaderWriter` the DLL built, at their real addresses, and dispatch
through the DLL's own vtable. A `Serializer` is interchangeable whichever side constructed it.

---

## 6. What each diff site can and cannot prove

This is the part worth reading closely. **These functions mutate a stream**, so the naive "call
ours, call theirs, compare" is not merely weak here — it would write the record twice and desync
the file. Only two of the 22 have a full oracle, and saying which is which is the whole point.

| Site | Kind | Proves | Does not prove |
|---|---|---|---|
| `GetEncoding` | **full** `RT_DIFF_SCALAR` | everything | — |
| `GetVersion` | **full** `RT_DIFF_SCALAR`, one caveat | everything | that the dispatched virtual is side-effect free for a subclass that does not exist today |
| `Serializer(Stream&,int)` | **real shadow** | the stream store, that the factory ran, that argument 2 was `this`, that the direction branch picked the right factory slot, and (save path) that argument 3 reached the codec | the 12 bytes the codec ctor moves |
| 18 × `Serialize…` / `SerializeBytes` | **partial, slot-proved** | *at registration:* our slot constant equals the DLL's own tail-jump displacement — a complete proof of the only thing the body decides. *Per call:* that the `ReaderWriter` and its slot are live | nothing about the bytes written, per call |
| `~Serializer` | **none** | only that it executed | anything |

### `GetEncoding` — full, free

Retail `0x180006290` is `MOV RAX,[RCX]; MOV RAX,[RAX+0x10]; RET`. Two loads, no dispatch, no side
effect. Running the original as an oracle costs a call and proves the whole function. Note what it
returns: the **Stream's `IO::Encoding`**, not the Serializer's `ReaderWriter`.

### `GetVersion` — full, with the caveat named

Retail `0x1800070b0` tail-jumps `ReaderWriter` vtable `+0x08`. Diffing calls that virtual twice.
That is safe **here and only here** because both implementations of the slot are a single load:
`?GetVersion@BinaryReader@IO@@UEBAHXZ` (`0x180023930`) and
`?GetVersion@BinaryWriter@IO@@UEBAHXZ` (`0x180023ab0`) are both `MOV EAX,[RCX+0x28]; RET`. If a
third `ReaderWriter` subclass ever gives that slot a side effect, this site becomes unsound. This
is the same argument Wave 3 lane A made for `Base::GetTypeId`, and it needs re-making, not
assuming, per class.

### The ctor — a real shadow, and why there is no runnable oracle

The brief allowed "state plainly that it is unverified". It does not have to be entirely. But it
absolutely cannot run the original, and the reason is concrete rather than precautionary:

> **Constructing a `Serializer` moves the stream cursor by 12 bytes.**
> `BinaryReader::BinaryReader` (`0x1800a8340`) calls `stream->vtable[+0x18]` (Read) three times
> with `count=4` and `stream->vtable[+0x28]` once. `BinaryWriter::BinaryWriter` (`0x1800a8a30`)
> calls `stream->vtable[+0x10]` (Write) three times with `count=4`. `BinaryReader::~BinaryReader`
> (`0x1800a82f0`) reads a further 4 bytes.

Running the DLL's ctor a second time on the same stream would consume or emit a **second 12-byte
header** — `reference_slot3_wrapper_must_resolve_first.md`, in the direction that corrupts every
following record. Constructing it on a scratch stream would prove nothing about this one.

So the shadow checks the post-state, and every clause is a real claim about the port:

1. `self->stream == stream` — the store happened;
2. `self->readerWriter != nullptr` — the factory ran;
3. `rw->serializer == self && rw->stream == stream` — **argument 2 was `this`.** This is the
   arity/argument-order proof: had we passed the factory or the stream in RDX, both would be
   wrong;
4. `rw->vtable == ??_7BinaryReader` when `direction == 0`, `??_7BinaryWriter` otherwise — the
   direction branch chose the right factory slot. Both vtables are resolved by their exported
   `??_7` names, not by address;
5. on the **writing path only**, `rw[0x28] == version` — argument 3 reached the codec. On the
   reading path `BinaryReader` immediately overwrites `+0x28` with a dword read from the stream,
   so there is nothing to compare.

**Caveat, so a divergence is read correctly:** clause 4 assumes the Stream carries the standard
binary `IO::Encoding`. A stream with a custom encoding would legitimately produce some other
`ReaderWriter` subclass and trip it with nothing wrong. The log line prints the actual and
expected vtable pointers so the two cases are distinguishable.

### The dtor — no oracle exists

Retail `0x1800b99b0` tail-jumps `ReaderWriter` vtable `+0x00` with `EDX = 1` — the scalar-deleting
destructor with "also free the memory" set. So it **deletes** the `ReaderWriter`, and
`BinaryReader::~BinaryReader` reads 4 more bytes on its way out. Running the original would
double-free and double-read. The site is registered so the run reports an honest `calls=` count,
and its name in the summary says `[not diffable]`. **Read its `divergences=0` as "it executed",
not as "it agreed."**

(Retail does not null `self->readerWriter` afterwards. Kept.)

### The 18 forwarders — where the real proof lives, and where it does not

The per-call site does **not** call the original, for the same stream-mutation reason. It reports:

* the **registration-time proof** that our slot constant equals the shipped DLL's own tail-jump
  displacement (§4). This is the strong claim and it is *complete for the slot choice* — nothing
  else in a three-instruction forwarder is a decision. But it is established **once, at init**, not
  per call;
* a per-call invariant registration cannot see: that `self->readerWriter` and the slot in its
  vtable are both non-null. Weak, but real — a null `readerWriter` means a `Serializer` used
  before construction.

**Read `divergences=0` on these 18 as "it executed and its ReaderWriter was live", not as "the
bytes agreed with the DLL".** What proves the bytes is the save/load pass: a wrong slot consumes
or emits the wrong number of bytes and the damage shows up later, on an innocent-looking record.

Three honest oracles and eighteen documented partials, as the brief asked for, rather than
twenty-two claimed ones.

---

## 7. Held back, and why

| Export | Retail | Slot | Why held back |
|---|---|---|---|
| `?Skip@Serializer@IO@@QEAAXXZ` | `0x1800070a0` | rw `+0x10` | no call site in the tree (no `ResolveExport`, no dispatch slot) |
| `?GetStream@Serializer@IO@@QEAAAEAVStream@2@XZ` | `0x18000a0a0` | — | same |
| `?GetDirection@Serializer@IO@@QEAA?AW4Direction@Stream@2@XZ` | `0x180006210` | — | same |
| `?Serialize@Serializer@IO@@QEAAXAEADPEBD@Z` (`char&`) | `0x18007c3e0` | rw `+0x70` | same |
| `Serialize(int64&/uint64&)` and their array forms | `0x180035000` etc. | rw `+0x88/+0x90/+0x30/+0x38` | same |

All five would be two lines each — the same three-instruction shape with a different constant. They
are held back because "called" is the honest denominator (PLAN §4.1 decision 1) and an uncalled
port is an unverified port (`reference_uncalled_port_is_not_verified.md`, WAVE1 defect 4). If a
later wave gives one a caller, add it *and* its `kForwarders` verification row together.

`IO::Encoding`, `IO::ReaderWriter`, `IO::BinaryReader` and `IO::BinaryWriter` are **modelled but
not ported.** This lane reads their layouts and dispatches through their vtables; it does not own
them. Porting `BinaryReader`/`BinaryWriter` is the natural next slice of Wave 4 and it is where the
actual stream bytes are decided.

---

## 8. Defects

| # | Defect | Class |
|---|---|---|
| 1 | Exe dispatch slot `0x140152418` is a PE import of `Serialize(signed char&)` (rw `+0xC0`) but is bound to `RtSerializer_Serialize_UChar` (rw `+0xB8`) | **PRE-EXISTING**, live. Wire-identical under the binary encoding — both move one byte — so it has never desynced a save, but it dispatches the wrong virtual and would diverge under any other encoding |
| 2 | Slot `0x140152490` is a PE import of `Serialize(n, char*)` (rw `+0x18`) but is bound to `RtSerializer_Serialize_UCharArr` (rw `+0x60`) | **PRE-EXISTING**, live. Same analysis |
| 3 | Slot `0x140152478` is a PE import of `Serialize(n, signed char*)` (rw `+0x68`) but is bound to `RtSerializer_SerializeBytes` (rw `+0xC8`) | **PRE-EXISTING**, live. Same analysis, and the worst of the three: `SerializeBytes` is one N-byte call, the schar array is an N-iteration per-element loop |
| 4 | The old `serializer.cpp` resolved its DLL fallback with `ResolveExport(...)`. That was correct only while nothing was registered under those names; the moment this lane registers, `ResolveExport` returns **the same function**, and the fallback calls itself | **INTRODUCED-BY-THE-WAVE if unfixed**; found and fixed in this lane's own file before it could run. Every fallback now uses `RT_ORIGINAL_ONCE` (`ResolveExportRaw`, cached — bare `RtOriginal` is `GetProcAddress` and cost Wave 2 five points of sim time) |

Defects 1–3 were all found the same way: running `tools/dispatch_slot_map.py <addr>` on all 14
registered slots and comparing the PE import name with the wrapper bound to it. The exact same
class of error is already documented in an existing comment at
`standalone/address_registry.cpp:1634-1637`, for `0x140152400` — which is correct today. Nobody
re-ran the check on the neighbours.

**No port bugs are claimed, because nothing has run.** Waves 1, 2 and 3 all found their port bugs
in the diff run, not in review. Expect the same here.

---

## 9. Comments to correct — for the orchestrator

PLAN §6 step 8 requires stale claims to be fixed at their source. These are outside this lane's
file ownership, so they are listed with exact replacement text rather than edited.

### 9.1 `standalone/address_registry.cpp` — three wrong bindings (defects 1–3)

The three correct wrappers now exist in `runtime/src/io/serializer.h`
(`RtSerializer_Serialize_SChar`, `_CharArr`, `_SCharArr`).

**Line 1627**, replace:
```cpp
    RegisterDispatchSlot(0x140152418, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UChar));  // Serialize(uchar&) (NameDB alias)
```
with:
```cpp
    // 0x140152418 is a PE import of Serialize(SIGNED CHAR&) -- rw vtable +0xC0,
    // not the uchar& +0xB8 this used to bind (tools/dispatch_slot_map.py).
    // Wire-identical under the binary encoding, so it never desynced a save; it
    // did dispatch the wrong Encoding virtual. Wave 4 lane A.
    RegisterDispatchSlot(0x140152418, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_SChar));  // Serialize(signed char&)
```

**Line 1631**, replace:
```cpp
    RegisterDispatchSlot(0x140152490, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UCharArr));  // Serialize(cnt,uchar*) (NameDB bulk)
```
with:
```cpp
    // 0x140152490 is a PE import of Serialize(n, CHAR*) -- rw vtable +0x18, not
    // the uchar* +0x60 this used to bind. Same analysis as 0x140152418 above.
    RegisterDispatchSlot(0x140152490, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_CharArr));   // Serialize(cnt,char*)
```

**Line 1640**, replace:
```cpp
    RegisterDispatchSlot(0x140152478, reinterpret_cast<uintptr_t>(&RtSerializer_SerializeBytes));      // SerializeBytes (NameDB alias)
```
with:
```cpp
    // 0x140152478 is a PE import of Serialize(n, SIGNED CHAR*) -- rw vtable
    // +0x68, an N-iteration per-element loop -- not SerializeBytes' +0xC8 one
    // N-byte call. This is the same mistake the comment above 0x140152400
    // already documents; the neighbours were never re-checked. Wave 4 lane A.
    RegisterDispatchSlot(0x140152478, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_SCharArr));  // Serialize(cnt,signed char*)
```

**Lines 1612–1622**, the block comment. Replace the `BANISHED_REIMPL_SERIALIZER` paragraph with:
```cpp
    // ----------------------------------------------------------------
    // IO::Serializer ports (runtime/src/io/serializer.cpp) -- Wave 4 lane A
    // ----------------------------------------------------------------
    // These slots are real PE imports whose consumers read *(fn**)slot
    // directly and never consult ResolveExport, so they must be bound here.
    // They point at the SAME functions RtIoSerializerRegister() registers by
    // mangled name -- one implementation, two entry points, not two
    // redirections. Kill switch: BANISHED_RT_SERIALIZER=0 (or
    // BANISHED_RT_IO=0) makes every one of them tail-call the shipped DLL.
    // The old BANISHED_REIMPL_SERIALIZER gate is retired.
```

### 9.2 `engine/types/io_serializer.h` — the conflated class and the wrong "version IGNORED"

This header is still the one `code/Game/**` comments point at, so it should be corrected rather
than deleted. Three claims are wrong:

* **`Serializer+0x08` is typed `Encoding*`.** It is an `IO::ReaderWriter*`. `IO::Encoding` is a
  different class, living at `Stream+0x10`. Suggested replacement for the `struct Serializer`
  comment block and the `Encoding`/`enc_vtbl` naming: mirror
  `runtime/types/io/Serializer.h`, or simply add above `namespace enc_vtbl`:
  ```cpp
  // NAME CORRECTION (Wave 4 lane A): the object at Serializer+0x08 is an
  // IO::ReaderWriter (vtable ??_7ReaderWriter@IO@@6B@ = 0x1801553b0; concrete
  // IO::BinaryReader / IO::BinaryWriter). IO::Encoding is a DIFFERENT class,
  // the policy object at Stream+0x10 that ?SetEncoding@Stream@IO@@ installs
  // and whose vtable slots +0x18/+0x20 CREATE those ReaderWriters. The offsets
  // in this table are right; the class name on them was not.
  // Authoritative layouts: runtime/types/io/Serializer.h
  ```
* **"the `version` ctor arg is IGNORED"** (the comment above `struct Serializer`). Replace with:
  ```cpp
  // Ctor (Runtime 0x1800b9960) -- the `version` arg is NOT ignored (this
  // comment said it was for a year). R8D is never touched by the body, so it
  // is still live at the factory CALL, which is
  // ??0Binary{Reader,Writer}@IO@@QEAA@AEAVSerializer@1@H@Z -- (Serializer&,
  // int). BinaryWriter stores it at +0x28 and WRITES it to the stream, so it
  // is the save-format version stamped into the file. Both factory slots take
  // three arguments: (factory, Serializer*, version).
  ```
* the decode-path comment says the factory slot is called `factoryVtbl[0x18/8](factory)` — one
  argument. It is three.

The `enc_vtbl` **offsets** in that file are all correct; only the class name and the ctor
description are wrong.

### 9.3 `engine/funcs/engine_funcs.h:2562` — the ctor typedef

```cpp
typedef void (*g_fn_Serializer_Ctor_t)(void* self, void* stream, int version);
```
The ctor returns `this` in RAX. This is *harmless* (no hidden return slot is involved — a ctor's
`this` return is not the `?A` by-value case that made `fn_Math_Transform3_Identity` a wild write),
but it is worth a note so nobody reads "void" as "the version is discarded":

```cpp
// Returns `this` in RAX like every MSVC ctor; typed void here because no
// caller uses it. NOTE: `version` is NOT ignored -- it reaches the
// BinaryReader/BinaryWriter ctor in R8 and is stamped into the stream.
// Wave 4 lane A / runtime/types/io/Serializer.h.
typedef void (*g_fn_Serializer_Ctor_t)(void* self, void* stream, int version);
```

### 9.4 `PLAN.md` §2.1 — the precedent paragraph is now history

It currently says the Serializer reimpl "checks `BANISHED_REIMPL_SERIALIZER` once; unset it
tail-calls the resolved original… **This only redirects our callers.**" Suggested replacement:

```
`runtime/src/io/serializer.cpp` reimplements the `IO::Serializer` forwarders. It was the one
runtime reimpl predating the port; Wave 4 lane A re-derived it from the shipped DLL and brought
it into the wave seam. `BANISHED_REIMPL_SERIALIZER` is retired in favour of
`BANISHED_RT_IO`/`BANISHED_RT_SERIALIZER`, and it is now registered by mangled name as well as
into the exe dispatch slots. It still only redirects our callers: no detour is installed, so the
DLL's own internal calls to `Serializer::Serialize` still go to the DLL. That is fine for a
forwarder that keeps no state.
```

### 9.5 `PLAN.md` §1.1 — the shadowed-slot note

"14 shadowed addresses (all `IO::Serializer` methods registered twice…)" is already marked as
fixed by the 2026-09-06 housekeeping in §1.4. Worth one added sentence there: **three of the 14
surviving bindings named the wrong overload** (defects 1–3 above); the removal fixed the
duplication but not the mis-binding underneath it.

### 9.6 Ghidra renames (PLAN §6 step 2) — NOT done, and why

Same reason as WAVE3_LANE_A §7: `rename_function_by_address` / `set_plate_comment` take no
`program=` argument and act on the current program, and `switch_program` is forbidden with four
lanes sharing the server. The retail functions in this lane's scope are already correctly named
and demangled in the Ghidra database because they are exports. The two factory helpers are not:
`FUN_1800a8120` is `IO::BinaryEncoding::CreateReader` (or whatever the concrete `Encoding`
subclass is called — the class name is not recoverable from the export table) and `FUN_1800a8080`
is its writer twin. They belong to whoever ports `IO::Encoding`.

---

## 10. What the integration run must look for

1. **`[RtIO/Serializer] 22 exports registered, all 22 retail bodies verified`** on stdout. If it
   instead prints `VERIFY FAILED`, the shipped DLL is not the one this port was written against
   and the family has correctly stayed on the DLL — read the printed slot numbers before touching
   anything.
2. **`calls=` on all 22 sites, before `divergences=`.** All 22 have live consumers (§5).
   `IO::Serializer::Serialize(int&)` should be the largest count in the lane by a wide margin. A
   `calls=0` there means the registration is not reaching `engine_funcs.cpp:3537` and the lane is
   inert.
3. **`IO::Serializer::Serializer(Stream&,int)` divergences must be 0**, and this is the one site
   in the lane whose `divergences` figure carries real weight — it is the shadow. A divergence
   prints the actual and expected `ReaderWriter` vtable; if the actual is neither
   `??_7BinaryReader` nor `??_7BinaryWriter`, that is a custom encoding and not a port bug (§6).
4. **`divergences=0` on the 18 forwarders proves only that they executed.** What proves them is
   the save/load pass. Run a save **and** load it back: a wrong slot consumes or emits the wrong
   byte count and the damage lands on a later, innocent record
   (`reference_slot3_wrapper_must_resolve_first.md`).
5. **`Save9.sav`.** PLAN §5 records that the `BANISHED_AUTOLOAD` harness has been broken since
   before Wave 2 and that Wave 4 should fix it first. This lane did not — it lives in
   `standalone/autostart.cpp` and `MenuDLT`, which lane A does not own. It is still the right
   thing to fix before this wave's boundary is called green, because everything in this lane lives
   on the save/load path.
6. **Perf.** These 18 forwarders are three instructions in retail and are on the save/load path,
   not the sim tick, so the 5% `sim ms/iteration` gate should be unaffected. `sa-stress` writes no
   saves. If the number moves, suspect the `g_portOn` test only after checking everything else:
   it is a static bool in L1.
7. **An access violation at a fault address of `0x00` or `0x08` inside `reimpl-x64.exe`** during
   serialization is the faithful missing-`ReaderWriter` fault, not a port bug — retail derefs
   `self->readerWriter` and its vtable unguarded and so does the port. Symbolize with `nm` against
   `standalone/build/reimpl-x64.exe`.

---

## 11. Findings that change other lanes' or other waves' assumptions

* **Constructing an `IO::Serializer` moves the stream cursor 12 bytes** (read on load, written on
  save), and destroying it moves 4 more. Any lane that constructs a scratch `Serializer` to probe
  something is corrupting the stream. This also means the *number of `Serializer` objects* a
  record path creates is part of the wire format.
* **`IO::Encoding` and `IO::ReaderWriter` are different classes.** Lane B owns `Stream`, whose
  `+0x10` is the `Encoding`; whoever ports the codecs owns `ReaderWriter`. The Serializer is just
  the thing that pairs them.
* **A `Stream`'s vtable slots, pinned incidentally by the codec ctors:** `+0x10` is Write, `+0x18`
  is Read (both `(this, count, void*)`), `+0x28` returns a position-like dword. Lane B should
  check these against its own derivation rather than adopt them — they were read off the codec
  call sites, not off `IO::Stream` itself.
* **`Object::Type::Serialize`'s `[not diffable]` site from Wave 3 lane A is now explained**, and
  its transcription confirmed: `Serializer+0x08` really is the `ReaderWriter*` and `+0x98` really
  is `Serialize(uint&)` on that vtable.
* **A three-instruction forwarder states its own vtable slot in its own bytes**, which makes
  "did I pick the right slot?" a decidable question at run time rather than a review question.
  The same trick applies to every other forwarder family in the DLL —
  `Object::Type::Serialize`, `IO::Persistence`'s getters, and most of `UI::Element` — and it is
  the only defence there is against the profile-build-offset trap, since that failure mode is
  silent by construction. `DecodeForwarderSlot()` in `runtime/src/io/serializer.cpp` is 15 lines
  and worth lifting into `runtime/funcs/` when a second lane needs it.
* **`tools/runtime_census.py` cannot see a table-driven `RegisterRuntimeExport` loop, and cannot
  see a caller that reaches a function through a directly-bound exe dispatch slot.** The first
  under-reports ported; the second over-reports "no caller in our tree". Both bit this lane.
