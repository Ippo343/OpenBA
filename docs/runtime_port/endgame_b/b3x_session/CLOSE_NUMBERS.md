# B3X (normal-virtual cutover) — close numbers

Step start `deaf604c` (B.3.9 closed) → close `0419d0ce` + the close documents. 2026-09-27 / 28.

## 1. The step

| | start | close |
|---|---:|---:|
| product classes with `DeletingDtor(unsigned)` at slot 0 | 461 | **12** (the `Video::*Data` classes, exempt by decision X1) |
| product classes with `virtual ~T()` | 0 (2 public non-virtual leaves) | **449** (`Object::Base` 414, `Graphics::Geometry` 13, `IO::Stream` 9, `Engine::UpdateLoop` 9, `IO::ReaderWriter` 4) |
| product `DeletingDtor(0)` calls | 367 on `this` + 24 on stack objects | **0** (345 `this->~T()`, 24 qualified `p->X::~X()`) |
| product `DeletingDtor(1)` / `Object::Destroy(` calls | 24 + 2 by name, ~126 raw slot-0 deletes | **8** (`*Data` receivers) / **0**; 118 + 2 `delete p` |
| dead vector arms (`flags & 2`) | 127 | 0 |
| class-scope `operator delete` DEFINED for a polymorphic class | 0 (41 declared, undefined) | **46** (5 roots in `runtime/src/object/root_operator_delete.cpp` + 41 in-class) |
| deleting destructors that reach the global `operator delete` | — | **0 of 426** (symbol check on both builds; `Dx9Placeholder` is the one legitimate global user) |
| slot-index reads on a flipping class, product | ~950 (B1: MEMBER 701 + DELETE 111 + DESTRUCT 6 on all classes; B2: 129 + 15) | **0** (W0_AUDIT.md: 1,114 remaining sites = exempt 397, not a vtable 533, by name 140, slot from the member 25, whole-vptr / dump 19) |
| unit tests | 914 | **919**, 0 failures |
| per-file PMF decoders in tests | 33 copies, 9 names | 0 (`tests/unit/slot_check.h`) |

`git diff --shortstat deaf604c 0419d0ce -- code runtime standalone engine dx11 tests`:
959 files changed, 8,880 insertions, 13,167 deletions. 44 commits on main, 10 gated merges.

## 2. Headers corrected from retail (each in the block that found it)

| member | was | is | evidence (Application-x64.exe) |
|---|---|---|---|
| `Action::Enter` | `Enter()` | `Enter(void* actionStack, void* time)` | 0x1400c4fc4 `MOV RDX,RSI`, 0x1400c4fbe `MOV R8,RDI`, `CALL [RAX+0x30]` 0x1400c4fc7 |
| `Work::Validate` | `Validate(void* citizen)` | `Validate()` | RCX only at 0x1400f7bc3, 0x1400f86ad, 0x1400f86f4, 0x1400f82b1, 0x1400f99a4 |
| `Work::Execute` | three arguments | `Execute(citizen, context)` | 0x1400f7be9, 0x1400f86d3 load RDX and R8, never R9 |
| `Work::Slot12` | `Slot12()` | `Slot12(int version)` | 0x1400f933b `MOV EDX,EAX` |

Argument lists that now pass what retail passes (the raw calls left a register unset): `OnEvent(0)`
(0x140108a2e `XOR EDX,EDX`), the `ActionFlush` precheck receives the stack (0x1400c4be0 `MOV RDX,RBX`),
two `Type()` calls receive `this`, ResidenceUI's "go to" `Enable(true)` (0x1400b54f6 `MOV DL,1`).
`WorkGrid_TryAssignWorkplace_f8620` returns zero upper bits (all five callers read AL). None showed in the
gate: resave 5/5 and the invariants held on each of those merges.

## 3. Found on the way

- **`partc.py` was a lower bound**: 190 of 1,205 runtime rows, 58 of 204 game rows, a tenth of the tests.
  The script now carries the missed shapes; four more are listed in `W0_AUDIT.md` §6.
- **NEXT_SESSION §3a's exempt list was partly wrong**: "UI::Font's factory-flag slot" is
  `IO::Encoding::IsBinary`; `edgemesh_vtable.cpp:452` was a ReaderWriter delete; the "state-machine handler
  tables" are raw data with no class and no caller. Two exempt roots it did not name:
  `Audio::Device::PlatformChannel`, `Pathfinding::IPathInfo`.
- **A by-name delete through the wrong root** (`standalone/menudialogloopt_stubs.cpp:447`: Geometry objects
  cast to `Object::Base`) worked only while both roots had slot 0 = delete. Fixed in W0 R-REST; the audit
  checked all 58 cast receivers and found no other.
- **Five index reads in no inventory** (`external.cpp:407`, `sunlight.cpp:197/272`,
  `wave28_lane_gfxbody.cpp:1174`, `dialog.cpp:1646`) — found by the independent audit, not by the blocks.
- **The global `operator delete` is the C runtime's**: without a class-scope one per root, `delete p` would
  have handed allocator blocks to `free()`. 41 in-class declarations had no definition anywhere.
- **`IO::ReaderWriter`'s pure destructor body cannot be inline**: as the first symbol of `main.o` it
  collided with MinGW's `.weak.__cxa_pure_virtual` alias at link. It lives in `serializer_class.cpp`.
- **Hand tables handed to by-name product calls** (four test fixtures) were in retail order and failed only
  at run time; they are built through `RetailSlot<T>` now.
- **`UI::DialogInstance::Update`'s `Contains` call passes `_dialog` where retail passes `_ui`** — a
  pre-existing divergence, left for its own behaviour commit (NEXT_SESSION §0.1).

## 4. Metrics that chain to the next step

`reinterpret_cast` lines (`git grep`, tracked `*.cpp *.h *.inc`): **29,870 → 29,159** (−711).
`// B.3.x bridge` lines: 6,116 → 6,397 (+281: W0's by-name calls on `void*` receivers are bridge casts).
PORT GAP lines 165 → 149; STANDALONE DEVIATION 49 → 49.
Registry (unchanged by this step, measured for the next): `rt_export_addrs.inc` 4,926 rows;
`RegisterRuntimeExport` 1,257, `RegisterRuntimeAddr` 1,452, `RegisterExeAddr` 696 code lines;
`address_registry.cpp` 1,243 lines; 127 `reg_*.cpp`; `ResolveAddr(Exe` 2,185, `ResolveAddr(Runtime` 275,
`ResolveExport` 204, `RT_EXPORT_ONCE` 242; `engine_funcs` 228 externs (3,035 + 1,651 lines);
`X_ResolveFuncs` 36 / 154; mint 0 / 0 / 2.

## 5. The step close (`stepclose.sh` on `0419d0ce`, 2026-09-28)

| | |
|---|---|
| Mods route with `.pkm` | exit 0, crash log 0, `[RtResolve] 0 misses` |
| Quit / Options / Achievements | exit 0, 0 TRAP |
| GoalDialog from a tutorial pause menu | opens (1 `OpenGoal` line), killed at the timeout as designed |
| tool routes placement / road / removal | exit 0, crash log 0 |
| in-game save (resave file mode) | `SAVE OPERATION COMPLETE`, crash log 0 (exit 1 = the expected whole-file DIFF of this mode) |
| `sa-stress` | exit 0, 6,000 frames: sim 0.17 ms/iteration, update 1.73 ms/frame (p95 2.25), draw 1.09 ms, 804 fps (baseline 0.19 / 1.92 / 2.40 / 1.38) — recorded, not chased |
| true release compile from a clean obj dir | exit 0, 1,094 files, exe 7,616,981 bytes; then `make clean` |
| clean rebuild debug / perf / unit | exit 0 each; 0 new warnings; unit 919 / 0 |
| gate on the clean-built exe | 0 problems: new game, tutorial, Load Game, `newgame_perf`, resave 3/3, typetable == baseline |
| `NOT IMPLEMENTED` stub set | 0 new, 0 gone vs the baseline |
| tutorial census | `[TownCensus] LOAD exit` citizen=16 herd=16 |
| forced device loss (rows 9 and 11 of `GATE_TABLE.md`) | INJECT=1, COMPLETE >= 95, crash log 0, killed at the 600 s limit |

Nothing owed from the close.

## 6. The acceptance

`bash docs/runtime_port/endgame_b/b3x_session/acceptance.sh` — output in `ACCEPTANCE.txt`, every line at
target (X1 12 / X1b 0 / X2 0 / X3 8 / X4 0 / X5 1 / X6 0 / X7 0 / X8 12 / X10 0 / X11 5).
