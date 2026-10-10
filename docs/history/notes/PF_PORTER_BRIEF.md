# Pathfinding/Action closure porter brief

You are porting Banished exe sub-helpers to faithful 1:1 C++ for the standalone reimpl.
Goal: drive the Action registration gap to 0 by reimplementing every function in the
pathfinding / region-graph / road-mesh / action closure. **Faithfulness is everything** —
none of this is runtime-exercised on the autostart path, so a wrong port is *undetectable
silent corruption, strictly worse than leaving it unregistered*. Transcribe the decompile
literally; do NOT "improve", simplify, or guess. When the decompile is ambiguous, check the
disassembly (`disassemble_function`).

## Mechanics
- Ghidra MCP is available (program Application-x64.exe, base 0x140000000). Decompile each
  addr with `decompile_function`. Cross-check intricate index math with `disassemble_function`.
- Each function `0xADDR` becomes a C++ function named `Pf_<Short>_<addr-suffix>` (e.g.
  `Pf_AvlRotateL_e450`) in YOUR assigned file only. Non-static, extern-visible.
- Write a matching registrar: `void Register<Batch>() { RegisterExeAddr(0xADDR, (uintptr_t)&Pf_..._xxxx); ... }`
  in YOUR `standalone/reg_<batch>.cpp` (guard the whole file with `#ifdef STANDALONE_EXE`).
- Do NOT touch any shared file (engine_funcs.h, address_registry.cpp, existing action_base_*.cpp).
  Do NOT build, do NOT commit, do NOT git anything. The lead integrates + builds + verifies.

## Conventions (match existing code/Game/Action/action_base_rawmat.cpp)
- Includes: `#include "engine_funcs.h"`, `#include "resolve.h"`, `#include <cstdint>`.
- Allocation: `fn_MemAlloc(g_pMemAllocSingleton, size, align)` and `fn_MemFreeStatic(ptr)`
  (NOT fn_MemFree — it's always null). Both come from engine_funcs.h.
- IBP/type: `fn_InternalBasePtr_CheckPtr(ibp)`, `fn_InternalBasePtr_Ctor(out, obj)`,
  `fn_IsDerivedFrom(obj, typeId)`. GetTypeId virtual = `(*(uint32_t(**)(void*))(*(uintptr_t*)obj + 0x10))(obj)`.
- Type-id globals (DAT_1401cxxxx) and exe sub-callees: resolve lazily via
  `ResolveAddr(Module::Exe, 0xADDR)` into a static fn-ptr, resolved once. For an exe sub-callee
  that is ANOTHER function in our closure, still call it via `ResolveAddr` (it will be
  registered) — do not duplicate its body.
- Runtime-DLL engine calls (System::/Math::/Graphics::/Picking::/IO::): resolve via
  `ResolveExport("?mangled@name@@...")`. If a needed fn_* already exists in engine_funcs.h, use it.
- Ghidra `g_pGameWorldSingleton + X` means `(uintptr_t)*g_pGameWorldSingleton + X` in our code
  (g_pGameWorldSingleton is `void**`, `*g_pGameWorldSingleton` = the gameworld object pointer).
- `this[0x19]` in Ghidra = byte access at offset 0x19, NOT array index. Empty do/while = decompiler artifact.
- Member functions returning a struct >8 bytes: hidden retval is the SECOND arg (RCX=this, RDX=retval).
- Preserve big stack buffers (e.g. `auStack_10008` = 64KB) — declare the same-size local array; do not shrink.
- Bool returns on functions whose callers test as int: use `int` return with explicit `?1:0`.

## Output (return as final message — data for the lead's verification)
For each addr: one block with `0xADDR name` + a 2-4 line note on what it does + any
faithfulness traps you hit (dropped args, member-ABI, index aliasing, stack buffers) + the
list of sub-callees you call via ResolveAddr/ResolveExport (so the lead can confirm closure).
State explicitly that each is a literal transcription and compiles under `-fsyntax-only`
(run `g++ -std=c++17 -fsyntax-only -I... ` if you can; otherwise eyeball-check syntax).
