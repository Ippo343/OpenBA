# OPEN: nine unpredicted bytes at `SaveLoad+0x2c`, UpdateList state 4

**Status: UNEXPLAINED. Do not close on the hypothesis below without running the
falsifier.** Raised by Wave 13 lane UPDATELIST; this note adds one candidate
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
