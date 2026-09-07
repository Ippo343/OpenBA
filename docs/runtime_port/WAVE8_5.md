# Wave 8.5 — Fix

Landed 2026-09-07 on `wave/upgrade-path-complete`. A wave in its own right, sitting between
Wave 8 (Top) and Wave 9 (Cutover): **it ports nothing new for its own sake, and instead repairs
what the previous eight waves left wrong, unverified, or unverifiable.** Wave 9 merges after it.

It exists because scoping Wave 9 (see [WAVE9.md](WAVE9.md)) could not proceed honestly on the
numbers and holdbacks as they stood. The census was wrong in both directions, four functions were
held back on blockers that no longer existed, and the largest inherited item was 32 functions with
32 loose reasons and no per-function decision.

## Result

**Census: 772 called / 507 ported (65.7%) → 827 / 507 (61.3%) → 830 / 510 (61.4%).** The first
move was not the port changing at all: the denominator grew because the census could not see
`dx11/`'s door, and the numerator's basis changed because it was counting gated-off registrations
as live. The second move is the close-out sitting's five real ports (§13).

**The wave was written in two sittings and §1–§12 are the first one, left exactly as filed.** The
close-out (§13–§16) works §12's four pickups and, in the course of it, corrects three conclusions
§1–§12 reached — including this wave's own headline advice about the Audio lane. Read them in
order; the corrections need something to correct.

| | First sitting | Close-out | Total |
|---|---|---|---|
| Ports landed and verified | 3 | 5 | **8** |
| Held-back functions verified without porting | 6 | — | 6 |
| Tools built | 3 | 1 (the Audio device recorder) | 4 |
| Census defects fixed | 3 | — | 3 |
| Stated blockers dissolved | 5 | 6 + 4 Serialize write paths | **15** |
| Comments/types corrected at source | 4 files | 6 | 10 |
| Own claims corrected | 5 | 4 | **9** |

Whole-tree at close: **253–269 diff sites, 25.6M–36.2M calls per run, divergences only in the two
known Wave 1 sin/cos sites.** `tools/export_name_audit.py` 0 phantoms of 652.
`tools/dispatch_slot_map.py --check` 0 mismatches, 0 shadowed. `Save9.sav` loads and simulates
with the new save-path port live. Zero-byte crash log on every run.

**WAVE CLOSED 2026-09-07.** §12's four pickups are all worked; what remains in Audio is
`Scene::Update`, `Device::Update` and `Device::FreeChannelPlatform`, each with a per-function
reason, and the next steps everywhere else are Wave 9 lane work with named prerequisites.

## 1. The census was wrong in BOTH directions

This is the finding the rest of the wave rests on, because every progress number this project has
published came from a tool that had not itself been checked against the process.

`tools/runtime_census.py` answers "how much is ported" **from source**. Nothing had ever answered
the question the cutover gate actually asks — **"does anything still ASK the DLL for something at
runtime"** — so `BANISHED_RT_STRICT` was built to answer it (§3), and comparing the two found
three defects:

1. **UNDERCOUNT, +51 called.** `dx11/` reaches Runtime through its OWN resolver,
   `Dx11ResolveRuntimeImport(mangled, iat)`, passing the name from a table literal rather than at
   the call site. `RE_EXPORT` saw none of it. **Our VideoDX11 replacement — the one subsystem that
   already survives its DLL being absent — had its entire Runtime import surface missing from
   `called`.** Video went 69 → 112. All 60 dx11-only names checked against the real export
   directory: 0 phantoms.
2. **OVERCOUNT, −6 ported.** A `RegisterRuntimeExport` inside an `if (<flag>)` block that is OFF
   by default was counted as a live port; `GATED_OFF_FILES` only knew whole FILES. All six are
   WAVE3.md's NO-GO handle-table tier, and the residue census caught four of them **arriving at
   the DLL** while the census called them ported. **Object went 28 → 22 — which is what WAVE3.md
   said all along.** The wave doc was right and the tool was wrong, for a full wave. Fixed with
   `GATED_OFF_BLOCKS` + brace-matched `gated_block_spans()`.
3. **18 registrations passing a `const char* const kX = "?Mangled…"` name** were warned about
   instead of counted. Naming the mangling once and using it at both the `RT_ORIGINAL_ONCE` site
   and the registration is GOOD practice, so the census now resolves same-file constants.
   UI 98 → 101.

The orphan list ("Ports with no caller in our tree") now also says that all of them have written
verdicts, so a NEW name appearing there reads as untriaged rather than as more of the same. (The
close-out added two — see §2's table — and the census text was updated with them rather than
left to be absorbed into "more of the same", which is the whole point of the line.)
## 2. CLOSED — the ports with no caller in our tree (8, and 10 after §14)

`tools/runtime_census.py` has ended with this list since Wave 1, and
`reference_uncalled_port_is_not_verified.md` exists because a wrong
`Quaternion::operator*` hid in it for a whole wave. Each now has a verdict.

| Port | Verdict |
|---|---|
| `??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z` | **Body exercised.** Three direct C++ calls from `Quaternion::Rotate` (x2) and `Transform::operator*` — only the export registration is idle. This is the one that was WRONG in z, and those callers are what caught it. |
| `??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z` | **Body exercised** from `RtMath_Transform_FromDirection` (`transform.cpp:770`). |
| `?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z` | **Body exercised** from `transform.cpp:798`; Wave 1 found a reassociated float sum in it through that path. |
| `?Serialize@Serializer@IO@@QEAAXHPEACPEBD@Z` (schar[]) | **Structurally verified, deliberately dead.** |
| `?Serialize@Serializer@IO@@QEAAXHPEADPEBD@Z` (char[]) | Ditto. |
| `?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z` (short[]) | Ditto. |
| `?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z` | **NOW VERIFIED** — see the probe below. |
| `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` | **NOW VERIFIED** — see the disassembly comparison below. |
| `?PauseEffect@Scene@Audio@@QEAAXI@Z` | *(added by the close-out, §14.)* **Body exercised** — `Scene::Pause` calls it exactly as retail does (retail does not inline it), and `ProbeAudioScene` sweeps it directly. Only the export registration is idle. |
| `?StopEffect@Scene@Audio@@QEAAXI@Z` | *(added by the close-out, §14.)* **Body exercised** from `Scene::StopEffect3D`, same as above. |

The three `Serializer` overloads are the char/schar/short array members of an 18-member
forwarder family. `RtIoSerializerRegister` verifies every one of the 18 **structurally at
registration**: it decodes the DLL's own 3-instruction tail-jump and asserts the ReaderWriter
vtable slot our port uses is the slot the DLL jumps to. The other 15 have a `ResolveExport`
site; these three do not, so they never run. Deleting 3 of 18 to satisfy a census line would
make the family asymmetric and buy nothing — they are kept, and the verification they DO have is
the structural check, not a `calls=` number.

**`standalone/rt_orphan_probe.cpp` (new)** is for the remainder: a port no harness path reaches,
no sibling calls, and no structural check covers. It calls our registered port and the shipped
DLL side by side over inputs chosen to cover every branch. It guards against the vacuous pass —
if `ResolveExport` and `ResolveExportRaw` return the same pointer then the port is not
registered and the probe would be comparing the DLL against itself, which it reports as a
failure rather than a pass.

`Math::Vector2::Normalize(Scalar&)`: **10 cases, 10 pass, 0 fail** — zero vector (the one
branch), unit axes, exact-length-5, denormals, overflow-to-infinity, mixed magnitudes and
`-0.0`. Output buffers are poisoned with `0xCD` on both sides first, so a field neither side
writes cannot read as agreement (`reference_oracle_must_not_compare_unwritten_bytes.md`).

`Graphics::EdgeMeshInstance::EdgeMeshInstance(const&)` cannot be probed: constructing a valid
source in isolation and destroying the copy would run `Geometry::~Geometry` over an attach array
shared with the source and clear live siblings' parent links. It is closed the other way, by
**static comparison against the retail disassembly** at `0x1800174f0`, which is five statements:

| Retail | Port |
|---|---|
| `CALL 0x180015b60` | `RtGeometryCopyCtor()(self, other)` |
| `LEA RAX,[0x180154d70]; MOV [RDI],RAX` | `emi->_base._vtable = RtEdgeMeshInstanceVtable()` |
| `MOV RAX,[RBX+0xc0]; MOV [RDI+0xc0],RAX` | `emi->_mesh = src->_mesh` |
| `MOV RAX,[RBX+0xc8]; MOV [RDI+0xc8],RAX` | `emi->_followed = src->_followed` |
| `MOV RAX,RDI; RET` | `return self` |

All three bindings confirmed against the shipped export directory with `pefile`:
`??0Geometry@Graphics@@QEAA@AEBV01@@Z` **is** `0x180015b60`,
`??_7EdgeMeshInstance@Graphics@@6B@` **is** `0x180154d70`, and `_mesh` / `_followed` are
`static_assert`-pinned at `+0xc0` / `+0xc8`, 8 bytes each. Instruction for instruction.

## 3. The three tools this wave added

**`BANISHED_RT_STRICT` — the residue census** (`standalone/resolve_standalone.cpp`). Records
every `ResolveExport` / `ResolveOrdinal` / `ResolveAddr(Module::Runtime)` that misses the port
registry and falls through to the shipped DLL, with the caller's return address. Log-only by
design: a large share of the misses are exported DATA, and poisoning those corrupts the process
long before the call that would name the culprit. **An empty `[RtResidue]` list across the
harness matrix is Wave 9's real cutover gate.** Full write-up in [WAVE9.md](WAVE9.md).

**`standalone/rt_orphan_probe.cpp` — verify ports no harness path reaches.** Calls our
registered port and the shipped body side by side over inputs chosen to cover every branch.
It refuses the vacuous pass: if `ResolveExport` and `ResolveExportRaw` return the same pointer,
the port is not registered and the probe would be comparing the DLL with itself, which it
reports as a FAILURE. Closed §2, §4 and §5 — **1,914 comparisons, 0 mismatches.**

**`runtime/src/io/save_trace.{h,cpp}` — lane S, the save-stream A/B oracle**, with a prefix
stop. See §8.
## 4. CLOSED — `Input::Device`, and WAVE8.md's premise about it was half wrong

WAVE8.md said the whole of `Input::Device` "**needs an ATTENDED run with real keyboard and mouse
input** before it counts as exercised — the harness injects dialog clicks by calling handlers
directly and never passes through the input layer."

**The second half is true and the first half does not follow.** `GetValue`, `Pressed` and
`Released` never touch the hardware: each computes `DeviceArrayBase(this, device, deviceCtx) +
inputIdx` and returns one field of the `State` record it lands on. Lane D gave all three a FULL
REPLAY oracle for exactly that reason. What they need is not a human at a keyboard — it is a
**caller**. Whether a key was down changes the value both sides read; it does not change whether
they read the same address, and the address arithmetic is the entire port.

A probe also covers it *better* than a person could: an attended run reaches only the triples
the player happens to press, while `RtOrphanProbeInGame` sweeps every valid one.

**570 comparisons, 0 mismatches** — joystick 216 (4 contexts x 18 inputs x 3 functions),
keyboard 315 (105 x 3), mouse 39 (13 x 3), against the live `Input::Device::_singleton` on a
real gameplay frame. It runs once, from `Camera_UpdateMovement`, which is on the input path and
only ticks with a live world: the boot probe cannot reach the singleton (`Engine::Startup`
builds it later) and the exit dumps cannot either (`Engine::Shutdown` destroys it first).

Deliberately NOT probed, each for a stated reason: `Device::ResetState` **writes** (it memsets
the three State ranges) and its oracle is a self-check rather than a replay, so a probe adds
nothing and would zero live input mid-frame; `KeyMap::SetInput` / `ClearInput` mutate the
binding tree and would rewrite the player's key bindings. `KeyMap::GetDeviceForId` /
`GetInputForId` / `GetIdForDeviceInput` are const readers and are probeable in principle, but
only against a populated binding tree — **they are what actually remains of the attended-run
item**, and it is now three functions rather than "the whole of `Input::Device` and most of
`KeyMap`".

## 5. CLOSED — `Input::KeyMap`'s three const readers

Closed the same way and in the same run as `Input::Device`, once the point was
generalised: they need a **populated** binding tree, and a live gameplay frame has one — the
player's real key configuration, reached through the same `g_pKeyMapObjectExref` triple-deref
`Camera_UpdateMovement` already uses. There was never anything attended about it.

Swept exhaustively, not sampled — `id` is narrowed to a `uint8_t` by the port and by retail, so
0..255 **is** the whole domain:

| Reader | Domain | Comparisons | Result |
|---|---|---|---|
| `GetDeviceForId` | id 0..255 x slot 0..1 | 512 | 0 mismatches |
| `GetInputForId` | id 0..255 x slot 0..1 | 512 | 0 mismatches |
| `GetIdForDeviceInput` | device 0..2 x input 0..104 | 315 | 0 mismatches |

The third matters most. It is the only member of the family that is **not** a lookup — it is an
in-order walk of the whole tree checking both slots of every binding, so it is the only place a
port could get `Leftmost`/`Successor` wrong, and a lookup-shaped test would never touch it.

**With this, WAVE8.md's attended-run item is closed in full**: 1,909 comparisons across
`Input::Device` and `Input::KeyMap`, 0 mismatches, no human at a keyboard.

## 6. CLOSED (as a decision) — the Wave 1 1-ULP residual

**The decision was already made and written**, in `runtime/funcs/rt_libm.h`: our own libm is the
default and `BANISHED_RT_LIBM=dll` is a diagnostic only, because "a port that cannot survive the
DLL's removal is not a port". Nothing about cutover changes that; the diagnostic simply stops
working, exactly as its own comment says.

What was genuinely open is the risk that same header flags: *"a last-bit difference in a routine
that feeds world generation is not automatically harmless, because generation is
seed-deterministic and small differences compound."* That had never been measured. It now has
been — three `sa-stress` runs at `--seed 12345 --frames 1200`, all on one build:

| Column | mingw #1 | DLL CRT | mingw #2 (control) |
|---|---|---|---|
| entities @600 | 13577 | 13577 | 13577 |
| entities @1200 | 13637 | 13706 | **13739** |
| citizens / homed / worked | 802 / 801 / 215 | 802 / 801 / 215 | 802 / 801 / 215 |
| age_mean, hunger_mean | 2.25, 5.5 | 2.25, 5.5 | 2.25, 5.5 |

**The control moves further than the change does.** Two runs of the *same* libm differ by 102
entities at frame 1200; our libm and the DLL's differ by 69. Every column that is deterministic
at all is identical across all three.

Stated at the strength the evidence supports: **the 1-ULP residual is below this harness's own
run-to-run noise floor** over 1200 frames and ~13,600 entities. That is a bound, not a proof of
bit-identity — the stress town steps in real time at 10x and is not frame-deterministic, so it
*cannot* resolve a 1-ULP effect. A bit-exact answer needs a deterministic world-gen fingerprint
that nothing in the tree currently emits; that is the follow-up if lane B ever wants one.

**The trap this nearly fell into is worth recording**, because the first attempt compared two
`standalone_debug.log`s and found exactly one differing line — the menu camera Transform — which
read like a clean libm signal. It is not: the control shows that same line differing between two
runs of the *same* libm. And those logs contained no world-generation output at all, so the
comparison was close to vacuous. Both errors are
`reference_one_perf_run_cannot_resolve_the_gate.md` in a new costume: check what your comparison
actually contains, and never attribute a difference without a control.


## 7. The holdbacks: 32 loose reasons reduced to a per-function disposition

Opening lane A on its two most tractable rows produced a better result than either port would
have: **the critical path is one missing tool, shared by four rows.**

`Audio::Scene::Serialize` turned out to be 40 instructions whose stated blocker was already
answered (above), and whose two unexported callees are ordinary `Collection` internals a faithful
port may call by address exactly as `ClearAll` and `RegisterInput` already do —
`0x180051390` is `Collection::Array<Emitter>::Resize` (0x40-byte elements, 3x growth heuristic,
allocator-locked free of the old buffer) and `0x180052640` is the bulk element serialize. Nothing
there is a wall.

**The wall is the oracle, and it is the same wall four times over.** Everything on the save path
shares one property: *replay is impossible*. Calling ours and then theirs either consumes the
stream twice or writes it twice, and — the specific trap — a second call often **succeeds
differently and silently**, which makes the oracle vacuous while it reports green. That is the
`RegisterInput` lesson (a second insert of the same id returns early) applied to a whole family.

Rows that are blocked on exactly this and nothing else:

| Row | Function | Why replay fails |
|---|---|---|
| 8 | `Audio::Scene::Serialize` `0x1800e0050` | consumes/emits the stream |
| 25 | `ExternalFactory::SerializeReferenceTable` `0x1800bcb60` | same, plus it rebuilds a `Set` |
| 26 | `External::SaveText` `0x1800bb9d0` | same, via `TextFileStream` |
| (Wave 3, opt-in tier) | `InternalBasePtr::Serialize` | WAVE3.md: "has **no oracle at all** and lands in the save/load path" |

**So lane A's first deliverable should be a tool, not a port: a save-stream A/B harness.** Write
the same object through *two* independent `IO::MemoryStream`s — one driven by our port, one by
the shipped body — and `memcmp` the bytes. The pieces already exist: `??0MemoryStream@IO@@QEAA@XZ`
(`0x1800ae710`) is already constructed by the Wave 4 `CompressedMemoryStream` port, and Wave 4
landed 22 `Serializer` functions with 0 divergences, so the `Serializer`/`Encoding` pair over a
scratch stream is assemblable from parts that are already ours.

That harness converts four "no oracle possible" rows into ordinary ports, and it would
retroactively strengthen Wave 4's own `Serializer` family, which today is verified by call-level
diffing rather than by byte-comparing what it actually wrote.

#### And "no oracle possible" is WRONG for the write path — which is the half that desyncs

Wave 3 wrote off `InternalBasePtr::Serialize` as having "no oracle at all", Wave 5 and Wave 8
used the same reasoning, and this document repeated it above. It is only true of the READ path.

**On the WRITE path a `Serialize` is a pure read of the object.** It walks the object and emits
bytes; it does not mutate what it is serializing. So calling ours and then theirs is a legitimate
replay **provided the two calls emit into different sinks** — the thing that made replay
impossible was never the function, it was that both calls shared one stream.

That gives lane S a much cheaper shape than a full `MemoryStream` pair. The comparison that
actually matters is not the bytes on disk but **the sequence of typed emissions**, because a
desync IS "a different sequence of typed writes". So the sink can be a synthetic double: a stub
`Stream` whose `+0x08` says writing and whose `+0x10` points at a stub `Encoding` whose vtable
slots record `(slot, size, value)` into a vector instead of touching a buffer. Run the port
against one recorder and the shipped body against another, then compare the two traces.

Everything needed to shape those doubles is already measured: `Serializer+0x00` is the `Stream*`,
`Stream+0x08` is the mode (non-zero = writing), `Stream+0x10` is the `Encoding*`, the
`+0x18`/`+0x20` pair returns the field object, and retail's field slots are bool `+0x78` /
uint `+0x98` / int `+0xa0`.

**The read path stays hard** — it consumes the stream and mutates the object — but it is also the
half that a write-path oracle indirectly protects: if our writer emits the same sequence retail's
does, a retail reader will accept our save, which is the property that actually matters.
**This is the single highest-leverage item left in the wave**, and it is why the remaining
holdbacks should not be attacked in the order they appear in the table.

## 8. Lane S, built — and the harness found its own bugs the way it was designed to

`runtime/src/io/save_trace.{h,cpp}`, gated on `BANISHED_RT_SAVE_TRACE=1`.

Two design choices are worth carrying to the remaining three rows:

**1. Compare the emission SEQUENCE, not the buffer.** A desync is precisely "a different sequence
of typed writes", so the sink does not need a real `MemoryStream` or a codec at all — a stub
`Stream` whose direction says writing, pointing at a stub `Encoding` whose factory returns a stub
`ReaderWriter` that records `(slot, args, value)`. That collapsed the estimated work from "build a
MemoryStream pair" to one file, and it makes divergences *legible*: the report names the emission
index, the slot, and both values, instead of "the buffers differ at byte 4118".

**2. Bind every slot, not the known ones.** The harness's first run crashed `EXEC-of-0` on the
stub `Encoding` vtable. Retail reaches an `Encoding` slot that `Scene::Serialize`'s own
disassembly never names, because the bulk element pass (`0x180052640`) derives its **own**
reader/writer from the `Stream` rather than reusing the caller's. A null slot faults and tells you
nothing; a bound one records and names itself. **The unknown slots are exactly the ones worth
binding** — that is the whole reason the harness could diagnose itself in one run instead of
needing a debugger.

The first subject came out clean at **32,772 emissions identical** — the full 4,096-entry effect
array plus the header and the five paused flags — and a separate gameplay run showed no whole-tree
regression (251 sites, 28,412,621 calls, 99 divergences, all in the two known Wave 1 sin/cos
sites).

#### CORRECTION: lane S unblocks ONE of the four rows, not four

This document claimed lane S "unblocks four rows at once". **Checking the other three
per-function shows it does not, and the reason is precise enough to be useful.**

Lane S rests on one property: *on the write path a Serialize is a pure read of the object.* That
is what makes two calls into two sinks a legitimate replay. **Three of the four target rows break
that property** — each does real work on the write path beyond emitting:

| Row | What it mutates on the WRITE path |
|---|---|
| `Audio::Scene::Serialize` | **nothing** — the resize is guarded to the reading side (`0x1800e00c0`'s JNZ). This is why it worked. |
| `ExternalFactory::SerializeReferenceTable` | walks the reference set writing an id into every node (`MOV [RBX+0x14],EAX` inside the `0x1800bcdf4` branch) |
| `InternalBasePtr::Serialize` | ends in `PersistObject` — mutates both the pointer and the global factory. **CLOSED by the prefix oracle 2026-09-07**: all four emissions precede the mutation, so `stopAfter=4` is not a partial view — it IS the whole emission sequence. 4 identical (non-null) + 1 identical (null, full compare); `BANISHED_AUTOLOAD=Save9` loaded and simulated 322,398 draw calls with the port live on the LOAD path. **Ungated**, having sat in the handle-table NO-GO tier without touching the handle table. |
| `External::SaveText` | needs `IO::TextFileStream` before the question even arises |

Running ours-then-theirs on any of those three would perform the mutation **twice**, on the one
live tree or the one live factory. That is the same shared-state trap the family was held back
for, wearing a different hat — and `RegisterInput` already showed what it looks like when a second
call succeeds *differently* rather than failing.

**The PREFIX oracle is now BUILT** (`Compare(..., stopAfter)`, 2026-09-07). The emission sequence is still
fully determined by the object at entry, so what these three need is a way to compare the
emissions *up to the first mutation* — either by stopping the body at a chosen emission index, or
by snapshotting and restoring the small amount of state each one touches (`InternalBasePtr` is
eight bytes; the reference-set ids are one dword per node). Lane S already records the sequence;
what it lacks is a stop.

It was proved on a subject already known clean — `Audio::Scene::Serialize`, which had just
passed the full compare at 32,772 identical emissions, so a truncation to 8 *had* to pass. It
failed twice, and both failures were the harness, not the port:

1. the two recorders were **locals written after `setjmp` and read after `longjmp`** —
   indeterminate, the one real rule `setjmp` imposes;
2. **`longjmp` on Windows x64 UNWINDS**, running the SEH cleanup of every frame it jumps over.
   Retail's `Serialize` bodies carry that cleanup (`MOV [RSP+0x20],-0x2` is the state marker) and
   it calls the `ReaderWriter`'s deleting destructor — **which emits**. Our own port is a plain
   C++ frame with nothing to unwind, so the sides were asymmetric by construction and the harness
   reported a divergence it had caused itself. Fixed by clearing the recorder before jumping.

**A prefix oracle that manufactures a divergence is worse than none, and only a known-good
subject can tell the two apart** — the same shape as the Wave 6 oracle that compared bytes
neither side wrote and read as a catastrophic port defect. What remains for the three rows is
now a reachable subject for each, not a missing tool.
**Recorded as a correction rather than fixed silently, because it is the fourth time this session
that a group-level claim of mine failed per-function checking** — after the `ExternalFactory`
four, `KeyMap::Load`, and `SerializeReferenceTable` as "the cheapest row". The per-function checks
have been right every time; the summaries over them have been wrong every time. That is a
measured property of this material, not bad luck: **these functions differ from each other in
exactly the details that decide whether a technique applies.**

## 9. The six Audio holdbacks share ONE root cause, and it is not what the lane doc says

WAVE8_LANE_C listed six reasons for six functions — "no oracle (plays sound)", "no oracle (stops
real audio)", "COM vtable calls", "audible side effect", "needs `SoundBuffer`/`SoundEffect`
internals". Reading three of them directly (`StopAll` `0x1800df150`, `Pause` `0x1800df070`, and
`FreeChannelPlatform` `0x1800de6f0` transitively) shows they converge on one thing, and it is
sharper and more actionable than any of those phrasings:

**Every path ends in a stored completion callback fired through a function pointer.**

* `StopAll` — walks 256 channels × 8 handles; per channel it `SetEvent`s the device wait handle,
  makes the COM `Stop` call at `Device+0xaa0`, calls `FreeChannelPlatform`, and then
  `CALL RAX` on the callback stored in the channel record.
* `Pause` → `PauseEffect` `0x1800df4a0` → `0x1800de200`, whose three calls are `0x1800dea20`,
  `FreeChannelPlatform` `0x1800de6f0`, and again `CALL RAX` — the same stored callback.

**Why that specific fact decides the oracle.** Every other technique this wave has used would
otherwise apply: the emissions have no stream, so lane S is irrelevant; the state is bounded (the
channel array is 256 × 0x70 = 28 KB plus `Scene::paused[5]`), so **snapshot/restore would work** —
except that **a callback which has already run into game state cannot be un-run.** Replaying
either function fires every live effect's completion handler twice. That is not an audible
artefact, it is arbitrary game-side reentrancy.

It also explains a detail lane C noticed but did not connect: `Pause`'s first line is
`if (paused[soundType] == paused) return;`, so a naive ours-then-theirs would have the second call
**early-return** and report green having compared nothing — the exact `RegisterInput` trap, in a
class where the first call has already done the damage.

**So the Audio lane needs a Device-level fake, not a better diff.** Interpose on the channel
records so the completion callbacks and the COM `Stop` land in a recorder instead of the real
device, then both sides can run. That is the same shape as lane S — replace the sink, not the
subject — one level lower in the stack. Sizing it is the Audio lane's first task, and it is a
better first task than porting `SoundBuffer`/`SoundEffect` (71 exports), because **without it
those 71 ports would still have no oracle.**

**And it narrows the stated dependency.** Lane C said `Scene::Update` "needs
`SoundBuffer`/`SoundEffect` internals this lane does not own". A faithful port may CALL those
classes by address — that is what `ClearAll`, `RegisterInput` and `Scene::Serialize` all do with
`Collection` internals. What the wave actually needs from them is **layout**, not ports. The
irreducible blocker is the callback, and `Scene::Update`'s own extra one: it consumes
`System::Random` state, so even a perfect device fake leaves it non-replayable without also
snapshotting the RNG.

## 10. The pattern, three for three: a known fact that did not reach the next wave

Three separate holdbacks and one wrong finding, all the same shape — **the information existed in
this repository and the person who needed it did not have it.**

1. **`BANISHED_AUTOQUIT` quits at the main menu.** WAVE3.md's "Notes for later waves" says so, and
   records that it had already produced one green-over-unexercised report. Six waves later it
   produced a wrong path-independence finding in a new tool.
2. **`KeyMap::RegisterInput`'s disputed parameter.** Lane D could not pin `FUN_180023060`'s first
   argument from the call sites. The callee answers it in nine instructions: RCX is never read.
   The function is now ported, 50 calls, 0 divergences.
3. **`Audio::Scene::Serialize`'s `IO::Encoding` offsets.** Lane C held it back because *that lane*
   had not re-derived retail's offsets. Wave 4 had already written them into
   `runtime/src/io/rt_io_port.cpp` — bool +0x78 / uint +0x98 / int +0xa0 — where they have sat
   ever since.

**Each holdback was correct on the evidence its lane had.** The failure is not judgement, it is
transport: a lane doc records what a lane learned, and the next lane reads its own brief. Two of
the three facts above were in a *wave doc* and did not travel; the one that did travel was in a
**source comment at the point of use** (`rt_io_port.cpp`), and even that one was not consulted by
a lane in a different namespace.

**This is the argument for lane 0 of Wave 9 being a reading pass, not just a residue matrix.**
Before porting anything, re-check each holdback's stated blocker against what the tree now knows —
because the measured hit rate on that question in this session was **three for three**.

## 11. What this wave deliberately did NOT do

* **No new porting for its own sake.** The three ports landed because closing their stated
  blocker made them cheap, not because a count needed moving.
* **`Audio::Scene::Pause` was written and then REVERTED.** Its unpause direction is genuinely
  A/B-able (it calls nothing), but the probe that would have proved it did not land, and a
  registered-but-unverified port on a live path is exactly what
  `reference_uncalled_port_is_not_verified.md` exists to prevent. It is a good first row for
  the next sitting: the port is ~25 lines and the oracle is a synthetic Scene with a crafted
  256-entry channel array.
* **The allocator was not armed**, `SoundBuffer`/`SoundEffect` (71 exports) and
  `IO::TextFileStream` were not ported, and no DirectInput oracle was built. Those are Wave 9
  lane work with named prerequisites, now written down per function.

## 12. Picking this up next session

> **ALL FOUR OF THESE ARE DONE — see §13 onward.** Left as written because three of them turned
> out differently than this list expected, and the list is the evidence for that.

In the order that pays best, each unblocked by this wave rather than blocked on it:

1. **`Audio::Scene::Pause` unpause path** — smallest complete unit, oracle already designed.
2. **The Audio device-level fake** (§9) — one tool that unblocks all six Audio holdbacks;
   build it BEFORE `SoundBuffer`/`SoundEffect`, or those 71 ports still have no oracle.
3. **`ExternalFactory::SerializeReferenceTable`** via lane S's prefix stop — it mutates within
   ~2 emissions, so measure what the prefix actually covers before committing to it.
4. **Re-check every remaining holdback's stated blocker first** (§10). Hit rate this wave: 3/3.

## 13. THE CLOSE-OUT SITTING — §12's four pickups, worked

Wave 8.5 landed at commit `77b50cc` with §12 listing four pickups "in the order that pays best".
All four are now done, and three of them changed what the wave itself had concluded. This section
is the second sitting; everything above it is the first, left as written so the corrections have
something to correct.

| §12 item | Outcome |
|---|---|
| 1. `Audio::Scene::Pause` unpause path | **PORTED — and so were four more, because item 2's premise was wrong.** |
| 2. The Audio device-level fake | **NOT NEEDED for 3 of the 6, and 15 lines for the 4th.** See §14. |
| 3. `SerializeReferenceTable` via the prefix stop | **MEASURED: the prefix covers 0.012%. Use the FULL compare — it is already safe.** See §15. |
| 4. Re-check every remaining holdback's stated blocker | **6 dissolved, 17 stand, 2 moot.** See §16. |

Close-out numbers, all on one build (`standalone-og`, 19:09:54):

| | |
|---|---|
| Census | 827 called / 507 ported → **830 / 510 (61.4%)** |
| Ports landed | 5 (`Pause`, `PauseEffect`, `StopEffect`, `StopEffect3D`, `StopAll`) |
| Probe | **520 comparisons, 0 mismatches, 3,469 `Device::Stop` calls matched pairwise** |
| Live diff, autostart (probe off) | 253 sites, 25,639,889 calls, 110 divergences — the two known Wave 1 sin/cos sites only |
| Live diff, autostart (probe on) | 264 sites, 25,773,904 calls, 97 divergences — same two sites |
| Live diff, `Save9` load | 269 sites, 36,241,696 calls, 45 divergences — same two sites |
| `tools/export_name_audit.py` | 0 phantoms of 652 |
| `tools/dispatch_slot_map.py --check` | 0 mismatches, 0 shadowed |
| Crash log | zero bytes on every run |
| Own claims corrected | 4 (see §14, and they are the interesting part) |

Live `calls=` for the five new ports, and an honest note about where they come from:

| Port | autostart, probe off | autostart, probe on | `Save9` load | divergences |
|---|---|---|---|---|
| `Scene::Pause` | 3 | 163 | 3 | 0 |
| `Scene::PauseEffect` | — | 2,264 | — | 0 |
| `Scene::StopEffect` | — | 236 | — | 0 |
| `Scene::StopEffect3D` | — | 96 | — | 0 |
| `Scene::StopAll` | 1 | 9 | 1 | 0 |

**The probe-on column is mostly the probe's own calls**, and the dashes are the honest part: in a
660-frame autostart with the probe off, `PauseEffect`, `StopEffect` and `StopEffect3D` are never
entered at all — nothing in that window drives an ambient or fire emitter's handle count to zero,
which is the only live path to `StopEffect3D` (`ambientenvironmentcomponent.cpp:170`,
`firecomponent_ignite.cpp:373`). For those three the probe is not a supplement to a live oracle, it
IS the oracle, which is exactly what §2 built it for and why the negative control in §14 is not
optional.

## 14. The Audio device fake was the wrong tool, and the probe's control proved it twice

### The finding: the Device hangs off a SCENE field

§9 concluded, from three of the six Audio holdbacks, that "every path ends in a stored completion
callback fired through a function pointer", and therefore "the Audio lane needs a Device-level
fake, not a better diff." **The root cause is right. The conclusion does not follow**, and reading
the five retail bodies *per function* rather than as a family says why in one line:

> the Device is reachable only through a non-null `Channel::platformChannel[b]` — and that is a
> field of the **Scene**, not of the Device.

| Function | The guard that puts the Device out of reach |
|---|---|
| `PauseEffect` `0x1800df4a0` | `TEST RDX,RDX / JZ` at `0x1800df4f8`, before `CALL 0x1800de200` |
| `StopEffect` `0x1800df550` | `TEST RDX,RDX / JZ` at `0x1800df5ab`, before the same call |
| `StopAll` `0x1800df150` | `TEST RCX,RCX / JZ` at `0x1800df1c8`, before the INLINED body |
| `Pause` `0x1800df070` | reaches the Device only through `PauseEffect` |
| `StopEffect3D` `0x1800df370` | reaches it only through `StopEffect` |

So a **synthetic Scene** whose 256 channels have null platform channels removes the device by
construction, and both implementations can be stepped side by side over the whole state space —
no interposer, no recorder, no fake. `StopAll` even loads the Device singleton into RBX at
`0x1800df192` *before* the null test; it is never dereferenced, so the singleton may legitimately
be null there.

That closed **three of the six holdbacks** (`Pause`, `StopEffect3D`, `StopAll` — all three in the
`--unported` CALLED set) plus the two siblings they call. The other three are untouched by it and
for a stated reason: `Device::Update` and `FreeChannelPlatform` are device internals, where the
device is the *subject* rather than an unreachable callee, and `Scene::Update` consumes
`System::Random` state, so removing the device still leaves it non-replayable.

`Scene::Update`'s stated dependency also narrows the way §9 predicted: it does NOT need
`SoundBuffer`/`SoundEffect` **ported** (Wave 8 said it did). It needs their **layout**. A faithful
body may call them by address exactly as this family calls `Device::Stop`.

**This is the fifth time in this wave that a group-level claim over these functions failed
per-function checking** — after the `ExternalFactory` four, `KeyMap::Load`, `SerializeReferenceTable`
as "the cheapest row", and §8's own "lane S unblocks four rows". The per-function checks keep being
right and the summaries over them keep being wrong. At five for five it is no longer worth calling
a pattern; it is the material.

### The device fake, once it was actually needed, was fifteen lines

Phase 1 of the probe (null channels) verified all five bodies' selection-and-reset logic. It also
reported **`PauseEffect`: 64 comparisons, 0 mismatches** — and that was worthless, because with
null platform channels `PauseEffect` *cannot do anything*. Its entire body is inside the branch
the probe had removed.

The probe caught that itself, because it carries a third scene: an untouched control `C`, compared
against `A` after every case purely to answer **"did this case change anything at all?"** The line
`WARNING Audio::Scene::PauseEffect: 0 of its cases changed the scene — agreement here is VACUOUS`
is the whole reason phase 2 exists. This is
`reference_uncalled_port_is_not_verified.md` one level down: not a port with no caller, but a
caller that reaches no code.

Phase 2 gives the channels **sentinel** platform-channel pointers and detours
`?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` to a recorder, so both sides run the release
loop and the **device call sequence** is compared alongside the state. That is the "device-level
fake" §12 item 2 asked for, and it turned out to be one detour and a two-field struct rather than
a Device model — because the only thing the Scene-level family ever asks of the Device is
`Stop(channel)`. Anything arriving while the probe is not running is passed through to the real
body, so installing it cannot swallow a genuine stop.

`StopAll` is deliberately EXCLUDED from phase 2, and the exclusion is not a shortcut: retail's
`StopAll` does not *call* `Device::Stop`, it **inlines** it (`0x1800df1eb`..`0x1800df289`), so the
detour would catch our call and miss theirs and the two sides would be asymmetric by construction
— the same shape as §8's longjmp-unwind trap, where the harness manufactured its own divergence.
`StopAll` keeps null channels and phase 1's verdict.

**520 comparisons (264 phase 1, 256 phase 2), 0 mismatches, 3,469 `Device::Stop` calls matched
pairwise, and every one of the five functions has effective cases** (Pause 80, PauseEffect 30,
StopEffect 60, StopEffect3D 38, StopAll 8).

### The negative control found a defect in the probe, not in the port

A probe that cannot fail proves nothing, so two defects were injected into the port and the probe
re-run:

1. `channelFlags[b] &= 0xfd` → `&= 0xfe` — **caught, 46 failures.**
2. `StopAll` working on channel `i` instead of the tag-derived `idx` — **NOT CAUGHT.**

The second is the point. The seeder built its "aliased" channels by *synthesising* a tag with this
channel's own generation and another channel's index bits — which can never pass the
`0xfffffe00` guard, so the aliasing cases never fired and the one branch that distinguishes `i`
from `idx` was untested behind 264 green comparisons. Fixed by **copying** another channel's tag
verbatim, and the mutation is now caught (374 differing fields on the first case). Both mutations
were then reverted and the clean run reproduced.

### Two shadow predicates and one comment were wrong about RETAIL

The live-path oracle for these five is a `RT_DIFF_SHADOW`: our body runs for real, and the DLL's
own read-only observer is asked whether the resulting state is what it should be. The first
version of three of them reported divergences, and **every one was the predicate, not the port**
(`reference_shadow_predicate_must_name_its_failing_term.md`, in its exact form):

| Predicate as first written | Divergences | Why it was wrong about retail |
|---|---|---|
| `StopAll`: "no channel still has a tag" | 8 of 9 | A channel whose tag names a DIFFERENT channel keeps its own tag while the other one is reset. Rewritten to pre-compute the obligation set *before* the call using the DLL's own `IsEffectPlaying`, then check it afterwards. |
| `StopEffect`: "the handle no longer validates" | 2 of 236 | False for a handle whose generation bits are all zero — it validates against a zeroed tag in retail too. Rewritten as the exact post-condition, "the record was zeroed". |
| `StopEffect3D`: "on reject, `GetEmitterCount` equals the slot's size" | 56 of 96 | The DLL's `0xffffe000` mask **is** bits 13..31, i.e. the generation and nothing else — the same predicate retail spells as `(idTag>>13) != (id>>13)`. A source comment claiming the two "differ by one bit" was wrong, and the predicate was built on it. Rewritten as `GetEmitterCount == 0`, which makes the reject path a full replay of the guard against the DLL. |

All three now read 0 divergences over 163 / 236 / 96 live calls.

### `calls=0` did not mean what the whole tree reads it as

`Pause` first shipped with the faithful early-out (`if (paused[type] == paused) return;`) ahead of
its shadow. It then reported **`calls=0` on a fresh autostart AND on a `Save9` load** — while
`site[189] = Audio::Scene::Pause` sat in the site list, which only happens if the body was
ENTERED. Every one of the six live call sites asks the scene for a state it is already in.

A shadow placed after an early return makes `calls=` mean *"did work"*, when
`reference_uncalled_port_is_not_verified.md` and every reader of that column take it to mean
*"was entered"* — a port that is called constantly and always early-returns reads exactly like a
dead port. The guard paths now report too, and on the `Save9` load `Pause` reads `calls=3,
divergences=0`, all three of them early-outs. `PauseEffect`, `StopEffect` and `StopEffect3D` got
the same treatment, and in their case the guard replay is the STRONGEST check available: the DLL's
`IsEffectPlaying` computes the identical predicate, so reporting it is asking retail whether our
guard agreed with retail's, on real handles on a real scene.

### One thing this deliberately did NOT do

`Audio::Scene::Update` (`0x1800df6c0`) is still held back, and the synthetic scene does not reach
it. Removing the device leaves the RNG: a replay that draws twice from `System::Random` diverges
by construction. Snapshot/restore of the Random state is a real and bounded next step, and it is
now the ONLY thing between this technique and the last called Audio function.

## 15. §12 item 3, measured: the prefix stop is the wrong tool here too

Full write-up in [WAVE8_5_PREFIX_MEASUREMENT.md](WAVE8_5_PREFIX_MEASUREMENT.md). Static analysis of
`?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` (`0x1800bcb60`..`0x1800bcf38`):

* **Q1. Emissions before the first mutation: 2** — before `MOV [RBX+0x14],EAX`, the one §8 names.
  Before the mutation §8 did NOT name — a full recursive teardown of the `_references` tree at
  `0x1800bcb80`, eight instructions in — **zero**. No emission index can get in front of it.
* **Q2. Total write-path emissions are `3 + 3N`**, N = `_resources._count`. The one empirical N in
  the tree is `[PreloadRefs] preloaded 5687/5688` on the `Save9` load, so ≈ 17,067 emissions.
  **Prefix coverage ≈ 0.012%**, and it is the wrong 0.012%: it verifies the factory choice and the
  entry count, and touches none of the walk, the id assignment, the id/path pairing, the string
  encoding or the terminator — i.e. nothing that can desync a save. `save_trace.h`'s own warning
  applies: a partial oracle that reads like a full one is worse than none.
* **Q3. Neither prefix nor snapshot is needed** — the write path's mutations are **idempotent and
  value-identical**. The teardown's second call reads a null root and returns; the per-node
  `referenceId = id` writes the same 1..N over a tree neither call modifies; the `GetPath`
  refcounts are balanced within each iteration. So the FULL compare (`stopAfter = 0`) is safe.

**This is a third distinct shape in one family** — `Audio::Scene::Serialize` mutates nothing,
`InternalBasePtr::Serialize` mutates irreversibly and needed the stop, this one mutates
reversibly-by-repetition — which is §8's per-function point confirmed a fourth time.

Two traps are recorded for whoever ports it, both silent: the second argument is a `Stream&`, not
a `Serializer&` (so lane S's `Compare` needs a one-line wrapper), and the teardown emits nothing,
so a port that forgets it passes a clean emission compare and needs an explicit call-site
assertion instead.

## 16. §12 item 4, done: 6 blockers dissolved, 17 stand, 2 moot

Full table in [WAVE8_5_HOLDBACK_RECHECK.md](WAVE8_5_HOLDBACK_RECHECK.md). Every dissolved item is a
**transport failure** in §10's sense — the answering fact already existed in the tree, and none of
the six needed new Ghidra work; they were found by grepping for the name the blocker itself
already stated.

The two that pay best:

1. **`Input::KeyMap::Load` was blocked on three things and is blocked on one.** (a) The unexported
   registry-string parser `FUN_18007f034` "in retail is a bare tail-jump to CRT `strtol(s,0,10)`"
   — written in `runtime/src/io/persistence.cpp:127` AND `WAVE4_LANE_C.md:167`, three waves before
   the holdback was recorded. (b) `System::String::Format` has been ported since Wave 2
   (`runtime/src/system/string.cpp:609`). (c) The `IO::Persistence` `Set<Pair<String,String>>`
   lookup genuinely remains.
2. **`UI::NumberEditElement::SetValue`'s "calling-convention risk" is gone.** The risk was calling
   the DLL's *exported variadic* `String::Format` across two toolchains' varargs ABI. Our tree has
   had a native, non-forwarding `RtSystem_String_Format` since Wave 2 — an ordinary same-toolchain
   call with none of that exposure.

And a structural one worth more than either: **lane S dissolves "no oracle possible" for four more
`Serialize`/`Persist` functions on their WRITE direction** — `AchievementManager::Persist`,
`Achievement::Serialize`, `Graphics::MeshInstance::Serialize`, and (flagged as needing the
per-function check, NOT assumed) `Goal::Serialize` / `GoalInstance::Persist`. Wave 5 held four of
these back as a GROUP for "all four construct an `IO::Serializer`, no oracle possible"; §8 has
already shown once what a group claim over Serialize functions is worth, so the recheck explicitly
refuses to close the two it did not read.

Three holdbacks are recorded as **framing stale, disposition unchanged** — `Save::SaveLoad::Draw`,
`Video::Context` ctor/dtor and `Graphics::FrameCopyImageBuffer` all cite "that's Wave 6/7/8's
class" as a reason, and those waves are closed, which reads as if the dependency resolved. It did
not: the specific classes named are still individually unported. The load-bearing reasons (a live
GPU context, zero callers) are independent and stand.

**All three comments were CORRECTED AT SOURCE** (`saveload.cpp`, `context.cpp`, `animation.cpp`)
rather than only noted here, because that is the whole lesson of §10: a fact recorded in a wave
doc does not reach the lane that needs it. **A blocker phrased as "waiting on another wave"
silently becomes a lie the day that wave lands** — it is the one form of holdback that rots
without anyone touching it, and there is no tool that flags it. Each now leads with the reason
that does not expire and keeps the wave-ownership note only as a correction of itself.
