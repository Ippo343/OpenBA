# Wave 8.5 — Fix

Landed 2026-09-07 on `wave/upgrade-path-complete`. A wave in its own right, sitting between
Wave 8 (Top) and Wave 9 (Cutover): **it ports nothing new for its own sake, and instead repairs
what the previous eight waves left wrong, unverified, or unverifiable.** Wave 9 merges after it.

It exists because scoping Wave 9 (see [WAVE9.md](WAVE9.md)) could not proceed honestly on the
numbers and holdbacks as they stood. The census was wrong in both directions, four functions were
held back on blockers that no longer existed, and the largest inherited item was 32 functions with
32 loose reasons and no per-function decision.

## Result

**Census: 772 called / 507 ported (65.7%) → 827 / 507 (61.3%).** Both figures moved, and neither
move was the port changing:
the denominator grew because the census could not see `dx11/`'s door, and the numerator's basis
changed because it was counting gated-off registrations as live.

| | |
|---|---|
| Ports landed and verified | 3 |
| Held-back functions verified without porting | 6 |
| Tools built | 3 (`BANISHED_RT_STRICT`, `rt_orphan_probe`, lane S + prefix stop) |
| Census defects fixed | 3 |
| Stated blockers dissolved | 5 |
| Comments/types corrected at source | 4 files |
| Own claims corrected mid-wave | 5 |

Whole-tree at close: **248–251 diff sites, ~28.4M calls, divergences only in the two known
Wave 1 sin/cos sites.** `tools/export_name_audit.py` 0 phantoms of 651.
`tools/dispatch_slot_map.py --check` 0 mismatches. `Save9.sav` loads and simulates 322,398 draw
calls with the new save-path port live. Zero-byte crash log on every run.

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

The orphan list ("Ports with no caller in our tree") now also says that all 8 have written
verdicts, so a NINTH name appearing there reads as untriaged rather than as more of the same.
## 2. CLOSED — the 8 ports with no caller in our tree

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

In the order that pays best, each unblocked by this wave rather than blocked on it:

1. **`Audio::Scene::Pause` unpause path** — smallest complete unit, oracle already designed.
2. **The Audio device-level fake** (§9) — one tool that unblocks all six Audio holdbacks;
   build it BEFORE `SoundBuffer`/`SoundEffect`, or those 71 ports still have no oracle.
3. **`ExternalFactory::SerializeReferenceTable`** via lane S's prefix stop — it mutates within
   ~2 emissions, so measure what the prefix actually covers before committing to it.
4. **Re-check every remaining holdback's stated blocker first** (§10). Hit rate this wave: 3/3.
