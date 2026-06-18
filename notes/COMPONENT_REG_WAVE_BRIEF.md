# Component Registration Wave — Shared Agent Brief

You are one of 4 parallel agents closing the **standalone address-registry gap** for the
`code/Game/Component/` module. Read this whole file before starting.

## Background — what you are fixing

In standalone-EXE mode, *we* are the exe. When our `code/Game/` code wants to call an
exe-internal helper, it does `ResolveAddr(Module::Exe, 0x140xxxxx)` to get a function
pointer, then calls it **guarded**:

```cpp
typedef void (*fn_Foo_t)(void* self, int n);
static fn_Foo_t s_fn_Foo = (fn_Foo_t)ResolveAddr(Module::Exe, 0x140002540);
...
if (s_fn_Foo) s_fn_Foo(self + 0x20, count);   // <-- unregistered => null => SILENTLY SKIPPED
```

If the address is **not registered**, `ResolveAddr` returns `0`, the guard is false, and
the helper is **silently skipped** — causing serialize-stream drift, un-grown arrays, and
subtle corruption. Your job: for each address in your batch, **decompile the exe helper,
reimplement it as clean C++, and register the exe address → your reimpl** so the guarded
call sites actually fire.

The call sites ALREADY EXIST in the component `.cpp` files (typedef + static fn-pointer +
guarded call). You are writing the **callee bodies** + **registration**, not new call sites.

## Per-address workflow

For EACH address in your batch:

1. **Decompile in Ghidra (shared server — see note below).** Tool:
   `mcp__ghidra-mcp__decompile_function(address="0x140xxxxx")`. You must first
   `ToolSearch("select:mcp__ghidra-mcp__decompile_function,mcp__ghidra-mcp__get_function_xrefs,mcp__ghidra-mcp__disassemble_function,mcp__ghidra-mcp__batch_decompile")`
   to load the schemas. Prefer `batch_decompile` to fetch several at once and reduce
   round-trips on the shared server.
2. **Find the existing call site.** `grep` the address in your component `.cpp` to see the
   expected typedef/signature the caller already assumes. Match it EXACTLY (param order,
   return type). If the caller has no typedef yet, derive the signature from the disassembly
   and the Ghidra prototype.
3. **Reimplement faithfully** in the SAME component `.cpp` that references it (e.g. the body
   for an address referenced from `growthcomponent.cpp` goes in `growthcomponent.cpp`).
   Port the decompiled logic literally — do NOT invent or "improve" behavior. Add a
   `// Reimpl of FUN_140xxxxx (<one-line purpose>)` comment.
4. **Use existing engine funcs** for allocator/string/serializer calls. They live in
   `engine/funcs/engine_funcs.h` (already included by these files). Notably:
   - Allocation: `fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, align)` to alloc,
     `fn_MemFreeStatic(ptr)` to free. **There is NO `fn_MemAllocStatic` — do not use it.**
     **GOTCHA: `fn_MemFree` is never resolved (always NULL) — use `fn_MemFreeStatic`.**
   - Check what's already declared before adding anything.
5. **Register** the address in YOUR batch reg file (see below). NEVER edit
   `standalone/address_registry.cpp` — that is the integrator's single-point merge.

## Registration file (yours alone — zero merge conflict)

Create `standalone/reg_component_<BATCH>.cpp` (BATCH = your batch name, e.g. `people`):

```cpp
// Component registration wave — batch <BATCH>
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// forward-declare your reimpl functions (or include nothing if they're file-static —
// in that case expose a small registrar from each component .cpp instead; simplest is to
// make the reimpl functions non-static and extern-declare them here).
extern "C" void RegisterComponentBatch_<BATCH>();   // declared in resolve-side header? NO —
// just define it; the integrator adds the call + an extern decl to address_registry.cpp.

void RegisterComponentBatch_<BATCH>() {
    RegisterExeAddr(0x140002540, reinterpret_cast<uintptr_t>(&YourReimplFn));
    // ... one line per address you completed ...
}
#endif
```

Because your reimpl bodies live inside the component `.cpp` files (often file-static), the
cleanest pattern is: make each reimpl function **non-static**, declare it `extern` at the top
of your reg file, and take its address there. Whatever you choose, your reg file must compile.

## Constraints (critical for clean merge)

- **Touch ONLY**: the component `.cpp` files in your batch + your own `standalone/reg_component_<BATCH>.cpp`.
- **Do NOT edit** `standalone/address_registry.cpp`, `engine/funcs/engine_funcs.h`, or any
  `engine/types/*.h` UNLESS strictly required. If you MUST add a shared decl, keep it minimal
  and **call it out loudly in your final report** so the integrator reconciles it.
- Keep typedefs / static fn-pointers **local to the component `.cpp`** (matches existing style).

## Build verification

Run `mingw32-make standalone-debug` in your worktree. It MUST compile cleanly (ccache makes
this fast). Do NOT run the game — integration/runtime testing is the integrator's step.
Fix all compile errors you introduce.

## Git

You are in an isolated git worktree. At start: `git checkout -b wave/component-<BATCH>`.
Commit all your work there with a clear message. Report the branch name.

## Final report (return as your last message)

- Branch name.
- Table: each address → reimpl function name → component file → 1-line purpose → DONE / SKIPPED(reason).
- Name of your `RegisterComponentBatch_<BATCH>()` function (the integrator wires it in).
- Any shared-header edits you were forced to make (loudly).
- Build result (`standalone-debug` pass/fail).

## Worked example (0x140002540, AmbientArrayResize, 40-byte stride array grow)

Ghidra shows a grow-array-and-copy: reads count at `+8`, capacity at `+0xc`, element stride
`0x28`, allocates `cap*0x28` via `MemAlloc(,8)`, copies old elements, zero-inits new ones
(with `+0x20`=1 default), frees old. Port it 1:1 into `ambientenvironmentdescription.cpp`,
matching the existing `fn_AmbientArrayResize_t` typedef `(void* arrayBase, int newCount)`.
