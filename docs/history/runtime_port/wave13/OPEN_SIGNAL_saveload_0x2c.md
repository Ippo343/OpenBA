# OPEN: nine unpredicted bytes at `SaveLoad+0x2c`, UpdateList state 4

**Status: CLOSED 2026-09-09 by Wave 14 lane SETUPLOAD — see the section at the
bottom. The writer is `?LoadMetaData@SaveLoad@Save@@` (0x1800c1b30), which is
deterministic and single-threaded; the worker-thread hypothesis below is
FALSIFIED.** Everything from here to that section is preserved as written,
because the reasoning that produced the wrong candidate is worth reading beside
the measurement that killed it.

Raised by Wave 13 lane UPDATELIST; this note adds one candidate
writer and, more importantly, the test that would kill it.

## The signal

Every UpdateList round reports ~9 unpredicted bytes at `SaveLoad+0x2c` during
**state 4**. That region is the embedded `Save::SavedState`'s `_size` (+0x2c,
4 bytes) and `_data` (+0x30, 8 bytes) — see the embedded-SavedState note in
`runtime/types/save/SaveLoad.h`.

Lane UPDATELIST attributed it to state 3 twice and was wrong twice. What
exposed the second wrong guess was that `excluded=` did not move across a build
that added `Exclude()`s — a quantity that did not vary with the thing it
supposedly measured, which is the shape
`reference_the_gate_was_never_a_control.md` is about. They then put the STATE
into the `Watch` label and it named state 4 in one run.

They deliberately did **not** add an `Exclude`, reasoning that an `Exclude` is a
CLAIM that the body knows what writes a region and chose not to model it.
**That call was correct and stands.**

## Candidate writer (INTEGRATOR'S HYPOTHESIS — NOT ESTABLISHED)

The **save/load worker thread**, not any code in `UpdateList` at all.

State 3 hands the embedded SavedState to `SetupLoad`, which builds a
`SaveBlock`, and passes that block to `Persistence::BeginLoad`. The worker then
fills the block's buffer asynchronously. `_size` and `_data` are exactly the two
fields a loader would write, and it would write them *while state 4 is running*,
which is when the signal appears. Lane UPDATELIST's own finding 3 — that a
worker-thread race is structural in states 3/4, because both arms branch on
`persistence[0x48]`, which the worker owns — is independent support for a worker
being active in precisely this window.

Byte count is consistent but NOT confirming: 4 bytes of `_size` plus the 5
low-order bytes of an 8-byte heap pointer that differs from its predecessor
gives 9, and heap pointers in one process routinely share their top 3 bytes.

## THE FALSIFIER — run this before believing any of the above

**The count being CONSTANT at 9 across every run is evidence AGAINST a
thread race**, and it is the reason this note does not close the question. A
genuine cross-thread write observed at an unsynchronised moment should vary run
to run; a constant count smells like a deterministic single-threaded writer.

So the discriminating test is:

1. Log the unpredicted byte OFFSETS and VALUES, not just the count, across
   >=10 rounds. A worker write should show `_data` taking different pointer
   values per round; a deterministic writer should not.
2. Check whether the signal appears in rounds where `BeginLoad` was never
   issued. If it does, the worker is exonerated outright.
3. Hardware write watchpoint on `SaveLoad+0x2c` (`standalone/hw_watch.cpp`
   already exists) and read the faulting thread id and RIP. This answers it
   directly and should be preferred over any amount of reasoning.

Step 3 makes steps 1 and 2 unnecessary. It is listed last only because it costs
a debugger session.

## If it IS the worker

Then the correct fix is **not** silence and **not** a bare `Exclude`. It is an
`Exclude` whose reason names the worker thread, because a region written by
another thread is permanently outside what a single-threaded predictor can
cover — and that reason would then be a claim the body can actually back.

---

# CLOSED 2026-09-09 by Wave 14 lane SETUPLOAD — it is NOT the worker thread

**The writer is `?LoadMetaData@SaveLoad@Save@@` (retail 0x1800c1b30), which
state 4 calls at 0x1800c22c6. It is deterministic and single-threaded.**

LoadMetaData only READS the embedded SavedState on the way in (0x1800c1b88
`MOV EDX,[R14+0x2c]`, 0x1800c1b99 `MOV RDX,[R14+0x30]` — it opens a
MemoryStream over the loaded bytes). It DESTROYS it on the way out:

```
1800c1d98  MOV RCX,[R14+0x30]        ; this->_embeddedState._data
1800c1d9c  TEST RCX,RCX / JZ
1800c1da1  CALL 0x1800c7ad0          ; MemFree
1800c1da6  MOV qword ptr [R14+0x30],0x0
1800c1dae  MOV dword ptr [R14+0x2c],0x0
```

Our UpdateList body SUPPRESSES the LoadMetaData call in predict mode, so those
two stores are never predicted while the DLL performs them for real. That is
the whole signal.

## The confirming measurement (not the argument)

`BANISHED_RT_SETUPLOAD_WATCH=1` (runtime/src/save/savedstate_setupload.cpp)
polls the SaveLoad singleton's embedded SavedState every frame. Under
`BANISHED_RT_PREDICT=1`, over ~40 rounds:

```
frame 303  _progress=3  _size=0      _data=0000000000000000
frame 304  _progress=4  _size=85070  _data=0000015e044ef760   <- state 3 ran SetupLoad
frame 305  _progress=2  _size=0      _data=0000000000000000   <- state 4 ran LoadMetaData
frame 306  _progress=3  _size=0      _data=0000000000000000
frame 307  _progress=4  _size=85315  _data=0000015e044ef760
frame 308  _progress=2  _size=0      _data=0000000000000000
```

Every state-4 round leaves BOTH fields **zero**. A worker filling the block
would leave a live pointer there, so this is the discriminating observation the
note asked for (its step 1: offsets AND values, over more than ten rounds).

## The "constant 9" that made the note doubt a race — arithmetic, exactly

Nine is not a constant of nature; it is what these particular values give, and
it explains why the count looked steady:

* `_size` 85070 = `0x00014C4E` -> `4E 4C 01 00`. The high byte was already 0,
  so **3** bytes change on the way to zero.
* `_data` `0x0000015E044EF760` -> `60 F7 4E 04 5E 01 00 00`. The top 2 bytes
  were already 0, so **6** bytes change.
* 3 + 6 = **9**.

Both terms are stable in one process (a save's meta block is ~80 KB, and the
allocator hands back addresses in one arena), which is exactly why it reads as
constant. The reporter also prints only the first few occurrences, so the
"always 9" impression came from a handful of lines, not from the site total.

## What to do about it

An `Exclude` is now a claim the body CAN back: the region is written by
LoadMetaData, a callee `RtSink::Call` suppresses, which is the documented
reason `Exclude` exists ("declare the footprint of a suppressed callee").
**This lane did not add it** — it is UpdateList's call to make, in UpdateList's
file, and this lane's remit was SetupLoad. Wave 13's refusal to add a BARE
`Exclude` still stands; what has changed is that a REASONED one is now
available.

The worker-thread race in states 3/4 that lane UPDATELIST found (both arms
branch on `persistence[0x48]`) is a SEPARATE, still-real finding. Nothing here
touches it.
