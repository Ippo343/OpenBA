# The normal-virtual cutover (ENDGAME B.1 decision 2): the rule sheet

Written 2026-09-28 by the coordinator from the five block-0 inventories in `b3x_session/`
(`INVENTORY_A_CLASSES`, `_B1_SLOTREADS_RUNTIME`, `_B2_SLOTREADS_GAME`, `_D_DTOR_CALLS`, `_E_TESTS`) and
`b3x_session/classes.py`. Earlier rule sheets still bind (B34B … B39); a later rule wins.

**The step.** Every class whose retail slot 0 is a deleting destructor declares `virtual ~T()` instead of
`virtual T* DeletingDtor(unsigned)`. GCC spends TWO slots on it (D1 complete, D0 deleting), so retail slot
N >= 1 becomes compiler slot N + 1. Nothing may read such a table by index when that happens.

## 1. Measured (block 0)

| | |
|---|---|
| classes in a `DeletingDtor` hierarchy | 640 = 461 product + 179 tests |
| product roots | 17: `Object::Base` 414, `Graphics::Geometry` 13, `IO::Stream` 9, `Engine::UpdateLoop` 9, `IO::ReaderWriter` 4, twelve `Video::*Data` 1 each |
| product slot-0 bodies | STD 323, VEC (dead vector arm + STD) 127, OTHER 5 (Game loops: STD + logging), PURE 3, INHERITED 3 |
| free function of every body | ONE target, `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (5 spellings) |
| global `operator delete` | libstdc++'s (`free()`), replaced nowhere |
| in-class `operator delete` declared, defined nowhere | 41 classes (+24 inherit one) |
| protected destructors | 447 of 461 |
| callers able to pass `flags & 2` | 0 (product); 41 test sites |
| slot-index rows, runtime/standalone/engine/dx11 | 1,205: MEMBER 701, DELETE 111, DESTRUCT 6, EXEMPT-TABLE 212, TABLE-BUILDER 67, DIAGNOSTIC 11, not a vtable 97 |
| slot-index rows, code/Game | 204: MEMBER 129, DELETE 15, EXEMPT-TABLE 49, DIAGNOSTIC 11 |
| tests | 1,192 PMF slot checks in 33 files (128 take `&X::DeletingDtor`), ~400 raw-slot helper calls, 41 vector-arm calls, 198 + 97 trait asserts that invert |

`partc.py` as written at the B.3.9 close saw 190 of the 1,205 runtime rows and 58 of the 204 game rows.
NEXT_SESSION §3a's numbers are a lower bound, not the size of the step.

## 2. Decisions

### X1 — The exempt list (derived; nothing else is exempt)
A class is exempt when retail has NO destructor in slot 0:
- `System::Thread` (slot 0 `Run`) and `Engine::Loader`, `Audio::Stream`, `IO::PersistenceWorker`, `PathfinderThread`
- `IO::Encoding` (slot 0 `IsBinary`) and `BinaryEncoding`, `TextEncoding`, `VisitorEncoding`
- `ControllerComponent::Controller` (slot 0 `FindState`), `StateMachineComponent::RootController`,
  `ControllerLive::TableController`
- `Audio::Device::PlatformChannel`, `Pathfinding::IPathInfo`
- COM interfaces (not ours)

Exempt BY DECISION (retail slot 0 IS a destructor, the class stays on `DeletingDtor`):
- the twelve `Video::*Data` classes. Each is its own root with no C++ subclass; every live object wears
  dx11's hand table built in RETAIL order. Re-indexing the seam is 84 → 96 slots in one commit with no
  compiler check and no behaviour gained. They flip when dx11's types become real subclasses (carry-over 6).
  Each gets `static_assert(!std::has_virtual_destructor<X>::value)` with this reason.

Corrections to NEXT_SESSION §3a: "UI::Font's factory-flag slot" is `IO::Encoding::IsBinary` (`UI::Font`
flips); `edgemesh_vtable.cpp:452` is a ReaderWriter DELETE; the state-machine "handler tables" are raw
`void**` data with no class (and `statemachine.cpp`'s readers have no callers).

`IO::ReaderWriter` FLIPS (539 rows converted by name first). It is not held.

### X2 — Two waves
- **W0, by name first** (behaviour-neutral, parallel, each merge gated): every slot-index read on a class
  that flips becomes a by-name call while the class is still on `DeletingDtor`. After W0 the only index
  reads left are on exempt tables, and the flip is a rename the compiler checks.
- **W1, the flip** (one owner, a script, all flipping roots and the tests in one branch).

### X3 — W0 conversions
| KIND | becomes |
|---|---|
| DELETE `vt[0](p, 1)` | `static_cast/reinterpret_cast<Root-or-class*>(p)->DeletingDtor(1)` — the cast target is the object's OWN hierarchy (`menudialogloopt_stubs.cpp:447` casts Geometry objects to `Object::Base`: fix) |
| DESTRUCT `vt[0](p, 0)` | `p->DeletingDtor(0)` |
| MEMBER `vt[N](p, …)` | `p->Member(…)`; a `void*` receiver takes `reinterpret_cast<Class*>(p)  // B.3.x bridge` |
| null-slot guards `if (vt && vt[N])` | deleted with the conversion (a compiler table has no null slot); a null OBJECT check that retail has stays |
| DIAGNOSTIC / TABLE-BUILDER on a flipping class | no literal index: compare whole vptrs, or take the slot from the member (`Object::CompilerSlotOf(&T::M)` / `SlotAddressOf`, `runtime/types/object/VSlot.h` — exists, do not edit) |
| EXEMPT-TABLE, not a vtable, dx11 seam | untouched |

- **Follow the OFFSET, not the helper's name** (`external_load.cpp:198/201`, `stringtable_factory.cpp:185`:
  `+0xa0` is `SerializeInt`, `+0x98` is `SerializeUInt`).
- **An argument list that disagrees with the header** (B2 "Obstacles", B1 §1) is settled from retail's
  disassembly before converting; when the header is wrong the header is corrected in the same commit,
  with the address. When it cannot be settled the site is LEFT and listed in the hand-in — never guessed.
- A receiver whose class is NOT VERIFIED (16 Work rows) is proven from retail or left and listed.
- No behaviour change is expected from any W0 block. A site that must change behaviour is not converted
  in the block; it is listed.
- Helpers defined in a block's files but used outside them keep their signature until the close.

### X4 — The flip (W1)
```cpp
class Base {
public:
    virtual ~Base();                                  // retail [0] 0x…: D1 + D0 under GCC
    static void operator delete(void* p);             // MemoryAllocator::MemFree (unit build: free)
};
class Leaf : public Base {
public:
    ~Leaf() override;                                 // the old protected ~Leaf() body, unchanged
};
```
- Destructors become PUBLIC and virtual; the body is the old non-virtual `~T()` body. The `DeletingDtor`
  declaration and definition are deleted, vector arm included (0 callers, INVENTORY_D Part 2).
- ONE defined `operator delete` per flipping root (`Object::Base`, `Graphics::Geometry`, `IO::Stream`,
  `IO::ReaderWriter`, `Engine::UpdateLoop`), in a registry-free TU that the unit build links
  (`tests/unit/sources.mk`), with the `BANISHED_UNIT_TEST` split `internal_class.cpp:88-95` has.
- The 41 declared-but-undefined in-class `operator delete`s are DEFINED (retail has a `??3` per class;
  all 88 bodies are MemFree) — a declaration left undefined is a link error, one deleted loses retail's
  member. In-class `operator new` declarations get the same treatment if D0/new-expressions reference them.
- PURE roots: `virtual ~T() = 0;` plus the out-of-class body.
- The five Game loops: the logging line moves to the top of `~T()`; `GameUpdateLoop::EnsureInit()` stays
  the first statement of `~GameUpdateLoop()`.

| call today | becomes |
|---|---|
| `p->DeletingDtor(1)`, `Object::Destroy(p)` | `delete p` (`Object::Destroy` stays as `inline void Destroy(Base* p) { delete p; }` for its callers) |
| `DeletingDtor(0)` on `this` (`Destruct` / `Reconstruct`) | `this->~T()` UNQUALIFIED — it must dispatch (inherited `Destruct` bodies) |
| `DeletingDtor(0)` on a stack / embedded / placement object of exact type T | `p->T::~T()` QUALIFIED; never `delete` |
| unqualified `p->~X()` where retail calls `??1X` directly (`modelcomponent.cpp:242`, `storagedisplaycomponent.cpp:316/323`) | `p->X::~X()` |
| anything on a `Video::*Data` or a Thread-family object | unchanged |

### X5 — Tests
- W0 block T0 lands `tests/unit/slot_check.h` (`CompilerSlot`, `RetailSlot<T>(n)`, `CHECK_SLOT`,
  `CHECK_DTOR_SLOT`) and the `SlotAt` translation as a no-op refactor: tests keep RETAIL slot numbers and
  the type decides the shift (`std::has_virtual_destructor<T>`). Constants are never bumped by 8.
- At W1 the tests are rewritten, not deleted: traits invert, recording stand-ins record in `~Probe()`,
  the free is proven by a recording class `operator delete`, the six stack probes no longer call slot 0
  raw (double destruction), `_ZTV[2](p, 1)` becomes `[3]`, `object_vtable_abi_test.cpp` becomes the test
  of the NEW rule with the retired one-slot shape kept as the control (INVENTORY_E §3.2).
- The 41 vector-arm tests: the arm no longer exists. Each file keeps one line naming the retail arm it
  used to exercise; where `T` is default-constructible a shared `CheckArrayDelete<T>()` replaces it.

### X6 — The measure
`b3x_session/partc.py` gains the seven shapes of INVENTORY_B1 §6.2 and B2's missed shapes (block R-IO
hands the regexes in; the coordinator edits the script). Its numbers only fall.

## 3. Standing rules
- Retail wins. A behaviour change is its own titled commit gated with resave x5. This step should have none.
- Never "fix" a faithful crash with a null check; never `getenv`; no new flag or instrument; no
  `Register*()` inside a `printf`; generated `.inc` files are regenerated, never hand-edited.
- Never `find /`; never `git stash`; greps exclude `standalone/build*/`, `tests/unit/build/`,
  `.claude/worktrees/`.
- Blocks own FILES. An agent edits only its block's files, builds at `-j3`, never runs the game and never
  touches `C:/GOG Games/Banished`. The coordinator alone merges and gates.

## 4. Blocks

### W0 (worktrees, disjoint file sets)
| block | files | rows (approx.) |
|---|---|---:|
| T0 | `tests/` | 1,192 PMF + ~20 helpers |
| R-UI | `runtime/src/ui/` | 331 |
| R-GFX | `runtime/src/graphics/`, `runtime/src/video/`, `runtime/types/video/`, `runtime/types/graphics/` | 310 |
| R-IO | `runtime/src/io/`, `object/`, `system/`, `save/`, `math/`, `collection/`, `runtime/types/io/`, `runtime/types/object/` | 235 |
| R-REST | `runtime/src/` audio, achievement, collision2d, engine, input, pathfinding, picking; `engine/`; `standalone/`; `dx11/` (2 MEMBER rows only) | 250 |
| G-A | `code/Game/Component/`, `Entity/`, `Action/` | 94 |
| G-B | every other `code/Game/` directory | 110 |

### W1
One owner: `b3x_session/flip.py` (driven by `classes.py`), the root `operator delete` TU, the product
hand cases, then the tests. One branch, one merge.

### Close
`partc.py` and `acceptance.sh` at target, STATUS, CLOSE_NUMBERS, NEXT_SESSION, `stepclose.sh`.

## 5. The gate (coordinator, every merge)
`gate.sh` unchanged (debug + release + perf build, new-warning set, unit, new game + pause-menu quit,
tutorial, Load Game, `newgame_perf`, resave x5, typetable == baseline, `[RtResolve] 0 misses`, 0 TRAP,
0 GATE WARN, `calls=5683` / `5773 openFileFail=12`, dispatch 0, mint UP TO DATE 0/0/2); `uiroutes.sh` and
`toolroutes.sh` on UI / tool merges; `devlost.sh` on Video / Graphics / dx11 merges (a crash check only —
device-lost is deferred).

## 6. Acceptance
0 `DeletingDtor` declarations outside X1's list; 0 `DeletingDtor(0)` / `(1)` calls outside it;
`Object::Destroy` is `delete`; every slot-index row of the inventories converted or on an exempt table;
the tests pin the new order; the gate passes; `stepclose.sh`.

## 7. The step as run (2026-09-27 / 28)

Block 0: five read-only inventories in parallel → this sheet → **W0**: seven blocks on worktrees (T0, R-UI,
R-GFX, R-IO, R-REST, G-A, G-B), each merged and gated → an independent AUDIT of what was left → **W1**: one
owner, `flip.py` + the hand cases, gated ALONE on a detached checkout before the tests existed → a second
owner rewrote the tests on top of that branch → one merge → the close. 10 gated merges, 0 problems, no
re-run, no re-cut baseline (`b3x_session/GATE_TABLE.md`, `CLOSE_NUMBERS.md`).

## 8. Corrections to this sheet
- X1: the state-machine "handler tables" have no class; nothing to exempt.
- X3: two reads could not become calls by name and take the slot from the member instead
  (`dialog.cpp:1646` `Contains`, `wave28_lane_gfxbody.cpp:1174` `AnimationEvent::Construct`): their
  headers disagree with retail, and the fix is a behaviour / signature change of its own (NEXT_SESSION §0).
- X4: the body of `IO::ReaderWriter`'s pure destructor is out of line (a link collision, CLOSE_NUMBERS §3);
  `Graphics::Light`, `Object::ExtensionCompiler`, `Object::ExternalCompiler` declare no destructor of their
  own slot 0 and keep their protected ones; in-class `operator new` declarations nothing references stay
  undefined.
- X5: no vector-arm test was retired — 41 became `delete[]` of `new T[n]` (braced initialisers where the
  class has no default constructor).
- §4: `runtime/types/engine` and the other per-namespace type headers belonged to the block of their
  namespace's `runtime/src` directory.

## 9. Deliberately left
The twelve `Video::*Data` classes and the dx11 seam (carry-over 6); the two header / argument divergences
of §8; `Video::DeviceData::Slot4`'s missing bool; dead `statemachine.cpp` helpers and `factory_vtbl`; four
shapes `partc.py` does not see. All listed in `NEXT_SESSION.md` §0 and §2.
