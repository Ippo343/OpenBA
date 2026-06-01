# Foundation D — Component Factory Cascade (standalone) — Agent Brief

## Mission
Give each assigned **component** type a real standalone factory so
`Object::Factory::CreateObject(typeId)` (called by
`ComponentEntity_CtorFromDescription` while instantiating an entity from its
template) returns a valid component instead of NullFactory's null. Each component
becomes **inert but binary-valid** (correct size, base init, shared stub vtable,
real capability flags, zeroed body). Behavior is wired later; this is the
*instantiation* layer.

This is the component analogue of the already-shipped Foundation C description
factories. The pattern + shared infrastructure is DONE and VALIDATED on the
keystone `PopulationComponent` — you just replicate it per type.

## The pattern (copy this exactly)
For each component `XxxComponent`, ADD this block at the END of its existing file
`code/Game/Component/<xxxcomponent>.cpp` (lowercase name; the file already exists):

```cpp
// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(XxxComponent, /*size*/0xNN, /*flags*/0xMM)
REGISTER_TYPE(XxxComponent, "Parent",
              &XxxComponent_Factory, &XxxComponent_CreateInPlace);
#endif
```

- `"Parent"` = the exact parent string from your assignment line
  (`Component`, `ControllerComponent`, or `ResourceProduceComponent`).
- `COMPONENT_INERT_FACTORY` and the shared inert vtable/dtor live in
  `code/Game/Component/component_factory_base.h/.cpp` (already committed). Do NOT
  redefine them. Just `#include` the header (guard against double-include is fine;
  the file may already include other headers — add these two includes inside your
  `#ifdef STANDALONE_EXE` block, near the bottom, and if `factory_registration.h`
  is already included elsewhere in the file that's fine, includes are idempotent).
- Look at `code/Game/Component/populationcomponent.cpp` (the validated keystone)
  for a real example of the exact block.

## Extracting `size` and `flags` (Ghidra, READ-ONLY — no renames/comments)
1. **size** — `decompile_function` the function named `XxxComponent_Factory`.
   It is `MemAlloc(0xNN, 8)` then `XxxComponent_Ctor(p)`. `size = 0xNN`.
   - If the factory does NOT MemAlloc (it tail-calls a `*_Factory_Abstract`,
     asserts, or just returns null), the type is ABSTRACT — **SKIP it**, do NOT
     add a factory, and report it as "abstract/skip". (Concrete leaf components
     should all MemAlloc; abstract bases won't.)

2. **flags** — `disassemble_function` the `XxxComponent_Ctor` (DISASSEMBLY, not
   decompile — these ctors have SEH/jumptables that garble the decompiler).
   The ctor calls its base ctor (a `CALL`), sets the vtable, then does a
   read-modify-write of the capability flags at `[this+0x20]`:
   ```
   MOV  reg,[this+0x20]      ; current (base ctors already OR'd their bits)
   OR   reg,0xKK             ; this type's own capability bits
   MOV  [this+0x20],reg
   ```
   The Component base ctor sets `+0x20 = 0x80`. ControllerComponent and
   ResourceProduceComponent bases OR additional bits before the leaf runs.
   **Compute the final value** = `0x80` OR'd with every `OR reg,IMM` that targets
   `+0x20` along the chain. In practice:
   - Directly `Component`-parented: usually ONE `OR EAX,0xKK` in the leaf ctor →
     `flags = 0x80 | 0xKK`.
   - `ControllerComponent`/`ResourceProduceComponent`-parented: the leaf `CALL`s
     the base ctor (follow it one level if needed) which ORs its bits, then the
     leaf ORs its own. Sum them: `flags = 0x80 | baseIMM | leafIMM`.
   - The flags value is at most a byte or two (e.g. PopulationComponent = `0xCB`).
   - If you genuinely cannot determine it, use `0x80` (safe default — entity
     becomes a passive non-updating member; still spawn-safe) and FLAG it in your
     report so the orchestrator can refine.

   NOTE: exact flags are not safety-critical here (every vtable slot is a stub, so
   any flags value spawns safely) — they only pick which world buckets the entity
   joins. Get them right for fidelity, but don't burn time: `0x80 | leafIMM` is
   almost always correct and always safe.

## Coordination rules (CRITICAL)
- **Only edit the component `.cpp` files in your assignment.** Each is owned by
  exactly one agent — no overlap. ADD your block at the end; do not rewrite
  existing code in the file.
- **Do NOT touch** `factory_registrations_bulk.cpp`, `component_factory_base.*`,
  `address_registry.cpp`, `engine_funcs.*`, the Makefile, or any other shared
  file. The orchestrator removes the matching `REGISTER_NULL_FACTORY` lines
  centrally.
- Ghidra: decompile/disassemble **read-only**. No renames, comments, or struct
  edits (shared DB).
- Syntax-check each file you touch (link errors are expected/fine):
  `g++ -std=c++17 -fsyntax-only -DSTANDALONE_EXE -I. -Ipatches/src -Iengine/types -Iengine/funcs -Icode/Game -Iruntime/types -Iruntime/funcs -Istandalone code/Game/Component/<xxxcomponent>.cpp`
  Fix all SYNTAX/type errors (missing-symbol/link errors are OK).

## Report back (final message): one row per component
`XxxComponent | parent | size(0xNN) | flags(0xMM) | file | status(done/abstract-skip/blocked) | notes`
Plus the exact `REGISTER_NULL_FACTORY(XxxComponent, "...")` line text for each
done type (so the orchestrator can remove it), and anything you skipped + why.
