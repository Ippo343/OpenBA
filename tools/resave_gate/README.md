# resave gate — the serializer regression check (2026-09-13)

```
tools/resave_gate/resave_gate.sh Gate                # verdict: PASS / DIFF vs golden / NEW-GOLDEN
tools/resave_gate/resave_gate.sh Gate <outdir> <secs>
GATE_MODE=file tools/resave_gate/resave_gate.sh Gate  # smoke test of the on-disk save path only
```

Run it under `tools/game_dir_lock.sh`. It needs `Documents/Banished/Save/Gate.sav` (the
golden was cut against md5 `68f99e06e6cdce6c17a8fb06e867b2c8`, a copy of the 2026-09-12
autosave; copy any save to that name and delete the golden to start over). Exit 0 is PASS.

## What it measures

The DLL-absent exe loads the save unattended (`BANISHED_AUTOLOAD`), and in the
LOAD-FROM-SAVE branch of `GameUpdateLoop::Load`, right after `SaveRestore(load)` and before
the first sim tick, `GameUpdateLoop_SaveRoundtripCheck` (code/Game/Game/gameupdateloop_serialize.cpp)
calls `SaveRestore(save)` into a fresh `SavedState`, decompresses both images through the
same `CompressedMemoryStream` path the load uses, writes them next to the logs
(`standalone_roundtrip_{loaded,resaved}.bin`, ~21 MB each) and prints one line:

```
[Roundtrip] RESULT=DIFF raw loaded=21074851 resaved=21075114 compressed ... firstDiff=4 diffBytes=... tail=263
```

**That line is informational.** Loaded-vs-resaved is DIFF by the design of retail's format:

- `ExternalFactory::SerializeReferenceTable` numbers every RESIDENT resource 1..N in sorted
  path order and every `ExternalPtr` in the payload is written as that number. The
  resident set depends on the process that writes the save: our load route leaves the
  loading-screen image (`Dialog\Loading.rsc:gameLoadN` + its three sprite resources) and
  the menu scene (`Game\MenuScene\SceneN.rsc`) resident, the process that wrote `Gate.sav`
  did not. Five extra rows shift every id after them, which is the ~15 MB of "differences".
- `InternalFactory::ResetPersistState` writes the internal object table's size, a
  process-history value (123800 in the input, 123514 in ours).
- `N` in both names above is a random pick (`Rand(10)` for the scene, `Rand(12)` for the
  loading image). `BANISHED_MENU_SCENE=<n>` pins both; the gate sets it to 0.

**The verdict is against the golden**: `docs/runtime_port/resave_gate/golden/<save>.resaved.bin`,
the resave of a known-good build for this save and route. With the two picks pinned the
resave is byte-identical run to run (measured: runs 13/14 and the golden run; unpinned runs
differed by exactly the digits of the picks). A Serialize body that changes what it writes
shows up as `DIFF vs golden` with the first offset. When the change is intended, delete the
golden and run twice. The `.bin` files are git-ignored; the golden's md5 is committed.

## What it found on its first run

- The DLL-absent save died in `CompressedStream::SetWindow`: the write-side GROW path
  (extend the header/block arrays on the first write into each new block) had been left
  delegated to the DLL as a "named cutover blocker". Ported 2026-09-13
  (runtime/src/io/compressed_stream_read.cpp). Every save takes it; the cutover gate never
  saved, so it never showed.
- Intermittent (2026-09-13, seen once in five full runs on the same exe): `DIFF vs golden`
  with exactly ONE differing byte, the reference-table `count` at +0x18 one higher (5689 vs
  5688) and no extra row, sizes identical. The next four runs of the identical command
  passed. So the count field can include one resource the row loop does not write, and
  which resource is resident is not fully deterministic even with the picks pinned. A
  one-byte diff at +0x18 is this; re-run before reading it as a serializer change.
- Intermittent, not fixed, both pre-existing: (1) one run in ~12 crashed in
  `ExternalFactory::Add`'s AVL rotate during the loader, with a `[GUL::Load] redundant
  dispatch` line just before it (two loader dispatchers racing; see the guard comment in
  gameupdateloop_load.cpp); (2) one run hung after the pause-menu Quit had returned, in
  teardown. Re-run on either; a second failure is a finding.

## Mapping an offset to a field

The payload has no field names (BinaryWriter clobbers them). Records are
`[AAAAAAAA][len][version]...[BBBBBBBB]`, the reference table is the first record
(`count` at +0x18, then `{u32 id, u32 len|0x80000000, path}` rows), and the loop's write
order is the body of `GameUpdateLoop_Serialize_95a90`. To localise a DIFF, add a
`SerMark`-style position print around the suspect Serialize call, run once, remove it.
