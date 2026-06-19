# Player Registration Wave — Agent Brief (2026-06-19)

Goal: drive the **Player** standalone-registration gap (31 exe-helper addrs) to 0.
Read `notes/STANDALONE_REG_WAVES_PLAN.md` (SAFETY RULES + closure-safety) and
`notes/COMPONENT_REG_WAVE_BRIEF.md` first. You are ONE agent owning a DISJOINT set
of files; other agents own the rest. Do NOT touch files outside your set.

## What these addrs are

`code/Game/Player/<tool>.cpp` call exe sub-helpers via `ResolveAddr(Module::Exe, 0xADDR)`
and cast the result to a function pointer (e.g. `g_fn_PlayTickSound`). In standalone
mode an UNregistered addr resolves to 0 → the call is silently skipped. We provide a
faithful clean reimpl body and register `addr -> &body` so the call fires.

## Your deliverables (TEXT report + 2 NEW files — do NOT build/commit/sub-fork)

1. `code/Game/Player/tool_base_<batch>.cpp` — NEW file holding your reimpl bodies
   (pattern: `code/Game/Work/work_base_accessor.cpp`). Guard `#ifdef STANDALONE_EXE`
   is NOT needed for the bodies if they compile in both modes; follow work_base_accessor
   (it `#include "engine_funcs.h"` + `"resolve.h"`, bodies unconditional). Bodies must
   NOT depend on hooks.h.
2. `standalone/reg_player_<batch>.cpp` — NEW file with `extern` decls + one
   `RegisterPlayer<Batch>Batch()` calling `RegisterExeAddr(addr, (uintptr_t)&Body)`.
   Wrap in `#ifdef STANDALONE_EXE` (pattern: `standalone/reg_work_accessor.cpp`).
3. A text report listing: per addr — VERDICT (REGISTER / DEFER / LEVEL-1-FIX),
   the closure table, and for DEFER the exact unregistered sub-callee(s).
   Quote the `RegisterExeAddr(...)` lines you added.

`<batch>` = use your assigned name (e.g. `drag`, `place`, `edit`, `path`).

## Per-address procedure (do ALL of this before registering)

1. `get_function_by_address(0xADDR)` — Entry MUST == ADDR. If ADDR is mid-function
   (Entry != ADDR), it is a **Level-1 caller bug**: do NOT register. Decompile the
   ORIGINAL Player method that the code/Game reimpl mirrors, find the TRUE callee at
   that site, report the corrected addr. (Don't edit the caller — just report it.)
2. `decompile_function(0xADDR)`. Then decompile EVERY exe sub-callee it calls
   (any `FUN_140xxxxxx` / named func whose addr is 0x140001000–0x14014ffff),
   RECURSIVELY, until you know the whole closure.
3. Classify each callee/symbol:
   - **exe code addr** (0x140001xxx–0x14014Fxxx): must be ALREADY registered, OR a
     gap-addr you register in THIS batch (bottom-up — leaves first). Check registered
     set (command below).
   - **dispatch slot** (0x14015xxxx): must be registered (`RegisterDispatchSlot`).
     If not, and the slot is needed, DEFER (or register the slot if its target is a
     pure Runtime export — see reg_action_move for the pattern).
   - **Runtime export** (`System::`/`UI::`/`Audio::`/`Object::`/`Math::`/`Graphics::`
     names, or `fn_*` from engine_funcs.h): resolves at runtime, NO registration
     needed. Confirm the `fn_*` exists in `engine/funcs/engine_funcs.h`; if a needed
     export has no `fn_` yet, you may add one (mangled-name resolved) — but report it.
   - **global** (`g_p...`, `DAT_...`, `DWORD_...`): confirm available. Singletons
     `g_pGameWorldSingleton`/`g_pKeyMapSingleton`/`g_pMemAllocSingleton` exist in
     engine_funcs.h. Type-ID storage globals (e.g. `DWORD_1401c7f58`, addr
     0x1401c7xxx in .rdata) are read via `ResolveAddr(Module::Exe, 0x1401c7xxx)`
     (see work_base_accessor.cpp `EnsureTypeIds`). `DAT_140172xxx` float consts:
     hardcode the literal value (read it from Ghidra `read_memory`/the disasm).
4. **Register ONLY if the ENTIRE closure is satisfied.** A half-null reimpl (some
   sub-callee resolves to 0) is SILENT CORRUPTION — strictly WORSE than the safe
   no-op skip. When in doubt, DEFER with the reason.

## Faithfulness rules (Ghidra-verify your own ports)

- Port 1:1; do not "improve". Match struct offsets exactly.
- Allocator: `fn_MemAlloc(g_pMemAllocSingleton, size, align)` / `fn_MemFreeStatic(ptr)`.
  There is NO `fn_MemAllocStatic`.
- Watch dropped args (Ghidra drops R8/RDX on some calls — check disasm with
  `disassemble_function` if a call's arg count looks off, esp. SetMesh/Format/ctors).
- Member-fn struct-return ABI: RCX=this, RDX=hidden retval, R8+=args.
- Contiguous Vector3 locals: declare `float v[3]` not 3 scalars (avoids -Wmaybe-uninit
  reorder bugs).
- Bool-return action quirk does NOT apply here (these aren't action executors), but
  still match the original return type.

## Registered-set / gap commands (run in repo root, git-bash)

```bash
# everything already registered:
grep -rhoiE "Register(ExeAddr|DispatchSlot)\(\s*0x1[0-9a-f]+" code engine standalone \
  | grep -oiE "0x1[0-9a-f]+" | tr 'A-F' 'a-f' | sort -u
# is 0xADDR registered?  (empty output = NOT registered)
grep -rhoiE "Register(ExeAddr|DispatchSlot)\(\s*0xADDR" code engine standalone
```

## Hard DON'Ts

- Do NOT edit `standalone/address_registry.cpp` (the lead wires your
  `RegisterPlayer<Batch>Batch()` in). Do NOT edit existing `code/Game/Player/*.cpp`
  (except to ADD nothing — bodies go in your NEW file).
- Do NOT build the whole tree, do NOT commit, do NOT sub-fork agents.
- Cap your Ghidra use — it's a single shared server. Be efficient.
