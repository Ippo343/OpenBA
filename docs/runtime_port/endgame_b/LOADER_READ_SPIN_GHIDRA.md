# Ghidra findings for LOADER_READ_SPIN.md §8 — does retail lock the load path?

Program verified: `Runtime-x64.dll`, `function_count 15290`, image base `0x180000000`.
Verified 3 times across this session (start, mid-session before Q1/Q2, and again at the
end before writing this file); `switch_program` was never called. All addresses below are
disassembly-derived (`disassemble_function`), not decompile-derived, except where a
decompile is quoted for shape only and cross-checked against the disassembly.

---

## 1. Global lock-caller table

Retail links five lock-shaped primitives (mapped by tracing the IAT thunk targets used
inside `System::CriticalSection`'s own methods, then confirming by the caller shape):

| primitive | thunk addr | resolves to | evidence |
|---|---|---|---|
| Enter | `[0x18014b390]` | `EnterCriticalSection` | called by `_lock` (CRT, `[INDIRECTION]`), `MemAlloc`, `MemFree`; used by `System::CriticalSection::Lock` (`0x18000ca90`, ADD RCX,0x60 then this call) |
| Leave | `[0x18014b388]` | `LeaveCriticalSection` | called by `_unlock` (CRT, `[INDIRECTION]`), `MemFree`; used by `System::CriticalSection::Unlock` (`0x18000cac0`, tail-JMP) |
| Init | `[0x18014b380]` | `InitializeCriticalSection` | called only from `CriticalSection` ctor (`0x18000c5c0`), `Stream` ctor, `MemoryAllocator` ctor, `Device` ctor |
| SetSpinCount | `[0x18014b208]` | `SetCriticalSectionSpinCount` | called from the same 4 ctors, always with EDX=0x1388 (5000) |
| Delete | `[0x18014b370]` | `DeleteCriticalSection` | called from `~CriticalSection`, `~Stream`, `~MemoryAllocator`, `_mtdeletelocks`×2, `_ioterm`, `~Device`, `_fcloseall`, `~Engine` |

`get_xrefs_to` on the Enter and Leave thunks each returned the same ~180-function list
(both queried in full, no pagination cutoff seen — the two lists are symmetric, as
expected for balanced Enter/Leave). Every single caller is one of: `MemAlloc`/`MemFree`
(the allocator singleton's heap CS at `_singleton+0x640`, confirmed in `MemFree`'s
disassembly: `RBX = [0x1801fc718]+0x640` then `CALL [0x18014b390]` … `CALL [0x18014b388]`),
a `Collection_Array_*`/`operator_delete`/`~X` destructor releasing a refcounted buffer
through that same heap CS, `Release()` (refcounting), or a `Graphics_*`/`Video_*`/`Particle`
resource teardown. **Zero** of the ~180 callers are named anything in `IO::`, `Object::`,
or `FileSystem`/`PackageFile`/`ExternalFactory`/`External`. `System::CriticalSection::Lock`
and `::Unlock` (the exported wrapper methods) have **zero direct callers by name anywhere
in the DLL** — `get_xrefs_to` on both returns only DATA/vtable references and the Entry
Point stub; every real lock in retail is inlined `EnterCriticalSection`/`LeaveCriticalSection`,
not a call through the wrapper. `StackCriticalSection`'s ctor is likewise inlined at every
use site (3 xrefs, all DATA); its dtor's 1094 "callers" are all `Unwind@...` SEH-cleanup
trampolines, not normal-path call sites — this RAII type is used, but not visibly, so its
usage is already covered by the Enter/Leave caller list above (same thunks).

**No SRW lock or mutex import exists in this DLL.** `list_imports` (full table, 189
entries, `CreateDirectoryW`…`MiniDumpWriteDump`) contains `InitializeCriticalSection`,
`InitializeCriticalSectionAndSpinCount`, `SetCriticalSectionSpinCount`,
`EnterCriticalSection`, `LeaveCriticalSection`, `DeleteCriticalSection` — no
`AcquireSRWLockExclusive`, no `CreateMutexW`, no `TryEnterCriticalSection`. (Querying
`get_function_xrefs` by these import *names* returned "No references found" — the
importer stores them as external-location thunks, not named functions; the thunk-address
method above is what actually works and is what produced the table.)

**Conclusion of §1: the only lock retail ever takes on this class of path is the global
allocator heap lock, taken inside `MemAlloc`/`MemFree` themselves — never inside
`FileSystem`, `PackageFile`, or `ExternalFactory` code, and never via the
`CriticalSection`/`StackCriticalSection` wrapper types outside of the ~180-function heap/
refcount/teardown list above.**

---

## Q1. `ReadFile@FileSystem@IO@@` @ `0x1800ace00`

Full disassembly read (69 instructions) and full callee list
(`get_function_callees`): `GetFileInfo @ 0x180025400`, `MemFree @ 0x1800c7ad0`,
`ReadFile@EXTERNAL` (the Win32 import), `ReadFile @ 0x1800ace00` (itself, recursive),
`SetFilePointer@EXTERNAL`. **No `CriticalSection`, no `EnterCriticalSection`/
`LeaveCriticalSection`, anywhere in the function — not just in the
`SetFilePointer`→recursive-call span, in the whole body.**

Span in question, verbatim:
```
1800ace54: XOR R9D,R9D
1800ace57: XOR R8D,R8D
1800ace5a: MOV EDX,EBX
1800ace5c: MOV RCX,qword ptr [RSP + 0x50]
1800ace61: CALL qword ptr [0x18014b0f0]      ; SetFilePointer
1800ace67: CMP EAX,EBX
1800ace69: JNZ 0x1800ace8f
1800ace6b: MOV R9,RBP
1800ace6e: MOV R8D,ESI
1800ace71: LEA RDX,[RSP + 0x38]
1800ace76: MOV RCX,qword ptr [RSP + 0x90]
1800ace7e: CALL 0x1800ace00                  ; recursive ReadFile (loose arm, ::ReadFile)
```
No instruction between `0x1800ace61` and `0x1800ace7e` touches the CS thunks or a
`CriticalSection` field. The two `CALL 0x1800c7ad0` (`MemFree`) sites at `0x1800acec1`/
`0x1800acefb` are *after* the read, releasing the two `Filename` copies made by
`GetFileInfo` at entry (`0x1800ace3d`) — `MemFree` takes the heap lock internally (§1),
but the `DEC dword ptr [RAX-4]` refcount decrement immediately before each `MemFree` call
is unlocked, exactly matching S3 in the source document.

**Verdict: NO.** `ReadFile` takes no lock and touches no `CriticalSection` field, anywhere.

---

## Q2. `OpenFile@FileSystem@IO@@` @ `0x1800ac880`

Full disassembly (139 instructions) and full callee list: `AdjustDelimiters @ 0x1800c6970`,
`Filename @ 0x1800067d0`, `MemFree @ 0x1800c7ad0`, `PlatformOpenFile @ 0x1800adce0`,
`Set @ 0x1800c95d0`, `_stricmp @ 0x18007eefc`. **No CS callee.**

The BST walk at `0x1800ac938`–`0x1800ac98c` (`packfileIndexRoot` descent) is a plain
`_stricmp`-driven binary search — no lock instruction anywhere in the loop:
```
1800ac938: MOV RBX,qword ptr [RSI + 0x40]     ; root
1800ac93f: JZ 0x1800ac98e
1800ac941: MOV RAX,qword ptr [RBP + 0x30]
1800ac945: MOV RDX,qword ptr [RBX]
...
1800ac955: CALL 0x18007eefc                    ; _stricmp
...
1800ac989: TEST RBX,RBX
1800ac98c: JNZ 0x1800ac945                     ; loop
```

**Verdict: NO.** No lock, no CS-field touch, anywhere in `OpenFile`, including the BST walk.

---

## Q3. Does `IO::FileSystem` contain a `System::CriticalSection` member?

Disassembled both `FileSystem` ctor candidates. `0x1800ac7c0` is the one referenced by
`OpenFile`'s peers (address range next to `ReadFile`/`OpenFile`); it stores its `this`
into the singleton global `[0x1801fc6a8]` at the end — this is the singleton
`IO::FileSystem` ctor. Full disassembly (17 instructions after prologue): every store is
a plain zero-init (`MOV qword ptr [RCX+n], 0`) up through offset `+0x48`, no
`InitializeCriticalSection`/`InitializeCriticalSectionAndSpinCount` call, no reference to
the CS thunks from §1.

Also disassembled `SetupFileSystem` (`0x1800aebb0`, retail's `Startup`, 313 instructions).
Every CS-thunk call inside it (over a dozen, e.g. `0x1800aec6f`–`0x1800aec8c`,
`0x1800aecf0`–`0x1800aed20`, …) is the *same* `[0x1801fc718]+0x640` heap-allocator-lock
pattern seen in `MemFree` — releasing `Filename`/array buffers built during startup, not a
`FileSystem`-owned CS. No `InitializeCriticalSection` call appears in `Startup` at all.

**Verdict: NO CS member.** `IO::FileSystem` holds no `CriticalSection`, in the ctor or in
`Startup`; every lock used during startup is the global heap lock, taken and released the
same way every other buffer-release site in the DLL does it.

---

## Q4. `ExternalFactory::FindOrLoad` / `::Add` / `::Find`; does `ExternalFactory` have a CS member?

- **`FindOrLoad` @ `0x1800bf320`** (confirmed matching address; xref_count 100 — by far
  the hottest of the group, consistent with "every resource request"). Full callee list:
  `Add @ 0x1800be570`, `AdjustDelimiters`, `ExpandRelativePath @ 0x1800bd360`,
  `Find @ 0x1800be0e0`, `GetDataDirectory`, `GetPathDirectory/Extension/Filename/Identifer`,
  `Load @ 0x1800bc620`, `MemFree`, `operator+`×2. **No CS callee.**
- **`Add` @ `0x1800be570`**: full disassembly (159 instructions). Only non-string callee
  is `FUN_18002bcf0` (a `SetLength`-style buffer-resize helper, itself only touching the
  heap lock via `MemFree`'s pattern for the *old* buffer, not the tree). The tree
  insertion itself is `CALL 0x18002ca00`, called with no lock held before or after. **No
  lock around the AVL insert/rotate — confirms the S4 hazard in the source document is
  real: `Add` mutates the tree completely unguarded.**
- **`Find` @ `0x1800be0e0`** (the one called by `FindOrLoad`): single callee `_stricmp`.
  **No lock.**
- **`Object::External::Load`**: identified by address+signature at `0x1800bc620`
  (`get_function_by_address` → `External * __cdecl Load(Filename*, External*)`, body
  `0x1800bc620`–`0x1800bc93b`). Full disassembly filtered to `CALL` (30 call sites): all
  direct calls (`0x1800aa3d0`, `0x1800a8200`, `0x1800aa560`, `0x1800c0390`, `0x18002c6a0`,
  `0x1800c9930`, `0x1800a9ee0`×2, `0x1800accc0`×2, `0x1800aaab0`, `0x1800aa100`,
  `0x180024aa0`, `0x1800249c0`, `0x1800248e0`, `0x180006a10`) or vtable-indirect
  (`[RAX+0x38]`, `[RAX+0x18]`, `[RAX+0x20]`, `[RAX+0xa0]`, `[RAX+0x98]`, `[RAX+0x28]`,
  `[RAX]`×2). **None is a CS thunk from §1.** (Caveat, stated per the task rules: a
  vtable-indirect call's target is not resolved here, so a lock *inside* one of those
  polymorphic callees — e.g. a serializer or compressor's own vtable slot — is not ruled
  out by this reading alone; only `Load`'s own body is clear.)
- **`Object::ExternalFactory` ctor** @ `0x1800be7e0`: full disassembly (17 instructions
  after prologue), same shape as the `FileSystem` ctor — plain zero-init through offset
  `+0x43`, storing `this` into singleton global `[0x1801fc6c8]`. **No
  `InitializeCriticalSection` call, no CS member.**

**Verdict: NO lock in `FindOrLoad`, `Add`, or `Find`; NO CS member on `ExternalFactory`.**
This directly corroborates the source document's S4 finding (unlocked AVL race) from the
disassembly side, independent of the runtime instrumentation.

---

## Q6. `PackageFile::GetFileInfo` @ `0x180025400`

Decompile (for shape) + disassembly agree: hidden return slot in RDX (confirmed —
`MOV qword ptr [RSP+0x10],RDX` at entry, `MOV RAX,RBX` / return of the dest pointer at
exit), source `this+0xC0`, single call to `FUN_18000c110`:
```c
undefined4 * FUN_18000c110(undefined4 *param_1,undefined4 *param_2)
{
  *param_1 = *param_2;            // 5 plain field copies (20 bytes) + 1 qword (8 bytes)
  ... (fields 1..4, then a qword at +0x30)
  System::Filename::Filename((Filename*)(param_1+8), (Filename*)(param_2+8));   // AddRef #1
  System::Filename::Filename((Filename*)(param_1+10), (Filename*)(param_2+10)); // AddRef #2
  return param_1;
}
```
**Verdict: CONFIRMED.** `GetFileInfo` copies the whole `FileInfo` by value via a hidden
retslot, with exactly two `Filename` copy-ctors (two AddRefs) per call — this is retail's
own shape, on every packed read, not a port artifact.

---

## Q7. `ExternalFactory::PushPath` / `PopPath` / `ExpandRelativePath`; per-object or per-thread stack?

- **`PushPath`** @ `0x1800bd0e0` (full disassembly, 132 instructions): builds a path
  string via `GetPathExtension`/`GetPathFilename`/`operator+` helpers, then
  `LEA RCX,[RSI+0x30]` / `CALL 0x18002e220` — a vector/array push onto a member at
  **`this+0x30`** (`RSI` = `this`, the `ExternalFactory` passed by the caller in Q4's
  `FindOrLoad`/`Add` disassembly, which also uses `RSI+0x30` as the tree-adjacent member
  region). No CS-thunk call anywhere in the function.
- **`PopPath`** @ `0x1800bd040` (full disassembly, 40 instructions): `ADD RCX,0x30` then
  `CALL 0x18002ccb0` (a pop/shrink helper) on the same `this+0x30` member, plus two
  `Filename` releases. No CS-thunk call.
- **`ExpandRelativePath`** @ `0x1800bd360` (full disassembly, 611 instructions — the
  largest function read this session): every `CALL` target is a path/string helper
  (`GetPathExtension/Identifer/Filename/Directory`, `GetDataDirectory @ 0x180007bd0`,
  `PlatformIsAbsolute @ 0x1800ab180`, `0x1800cab50`, an import at `[0x18014b0d0]`,
  `0x1800bcfd0`, `operator+`, `Filename` ctors) or `MemFree`. **No CS-thunk call.**
- **No TLS access anywhere in any of the three functions** — no `gs:[...]` segment-prefixed
  instruction, no call to `TlsGetValue`/`TlsAlloc`/`TlsSetValue` (these imports exist in
  the DLL per `list_imports`, but none of their xrefs land in these three functions;
  not separately re-verified by name-xref since that lookup mode returned empty for
  Win32-import names throughout this session — see the §1 caveat — but exhaustive
  disassembly of all three functions shows no `gs:` prefix byte at all).

**Verdict: NO lock in any of the three; the path stack lives at a fixed offset (`+0x30`)
on the singleton `ExternalFactory` object itself — it is per-object, not per-thread.**
This directly corroborates the source document's S6 concern: retail's path stack is a
single shared, unlocked, unTLS'd stack, exactly as the document guessed.

---

## Q8. `PackageFile` ctor @ `0x1800b4440` — one `FileInfo`/HANDLE, or two?

Full disassembly (46 instructions): sets vtable pointers (`0x180154788` then immediately
overwritten `0x1801547a0` — two-stage vtable set, typical of a most-derived-class ctor
calling into a base ctor first), zeroes fields through `+0xb8`, sets `+0x70` to `0x2710`
(10000, likely a default block/window size constant), sets `+0xac` to `1` (a bool flag),
then at `+0xc0` begins **exactly one** `FileInfo` block: `dword[+0xc0]=-1` (handle/type
sentinel), qword `+0xc8`=0, dword `+0xd0`=0, two zeroed pointer members at `+0xe0`/`+0xe8`
(the two `Filename` slots matching Q6's shape), and `qword[+0xd8]=-1`. **No second block
of this shape appears anywhere in the 0xC0-byte layout before or after this one** — the
ctor writes fields only up to roughly `+0xf0` and returns.

**Verdict: exactly one `FileInfo` / one implied `HANDLE` slot.** No second handle, no
per-thread handle, anywhere in the constructor's field layout.

---

## Q9 (bonus). Thread count / `Engine::Loader`

Found exactly **one** `Thread::Start` in the whole DLL (`search_functions_enhanced` on
`^Start$`, total 1) at `0x1800ca610`. Its `get_xrefs_to` returns only two DATA references
and the Entry Point stub — **`Start` is called exclusively through the `Thread` base
vtable**, never by direct/named call, so counting "how many threads retail spawns" this
way would require resolving every vtable-indirect call site whose target class derives
from `Thread` and enumerating each `Thread`-derived ctor's vtable-store instruction — that
is not a cheap lookup and was not attempted (correctly out of budget per the task's "bonus,
only if cheap" framing).

Disassembled `Engine::Loader`'s ctor at `0x1800dc0f0` (matched via `search_functions_enhanced`
name pattern `Loader`, xref 4): it calls `0x1800ca4e0` with `EDX=0x40000` (a 256KB stack
size, consistent with a `Thread` base-ctor call), sets its own vtable to `0x180157a08`,
and zeroes one member at `+0x30`. This constructs **one** `Loader` object; it does not by
itself prove whether the object's thread is ever started more than once, or whether the
exe (not this DLL) also dispatches loads from elsewhere.

**Verdict: UNKNOWN** whether retail ever runs more than one loader thread or dispatches
`Load` from a second call site — the vtable indirection on `Thread::Start` makes this
determination non-trivial, and Q5 (the EXE-side half of this question) is explicitly out
of scope for this Ghidra program per the task brief.

---

## What this means

No function on the resource-load path — `FileSystem::ReadFile`, `FileSystem::OpenFile`,
`ExternalFactory::FindOrLoad`/`Add`/`Find`, `External::Load`'s own body, `PushPath`/
`PopPath`/`ExpandRelativePath` — takes a lock or touches a `CriticalSection` field,
anywhere, in retail. **YES/NO answer: NO function on this path locks anything.** The
*only* lock retail ever takes near this code is the global allocator heap lock inside
`MemAlloc`/`MemFree` themselves (taken ~180 places across the whole DLL, none of them
`IO::`/`Object::` load-path code), which protects heap metadata, not resource state.
Neither `IO::FileSystem` nor `Object::ExternalFactory` owns a `CriticalSection` member.
The shared `_pathStack` (S6) is confirmed to live on the singleton `ExternalFactory`
object at a fixed offset, unguarded and not TLS-based — a second concurrent loader would
scramble it exactly as the source document's §5a speculates. Q9 (thread count) is the one
UNKNOWN: `Thread::Start` is only ever called through a vtable, so it could not be answered
without resolving indirect call targets, which was out of the cheap-lookup budget for this
task. Conclusion for the coordinator: retail's correctness on this path structurally
depends on having exactly one loader thread — there is no lock anywhere to fall back on —
so the fix belongs in `standalone/` (single-owner loader), not as a new lock in `runtime/`.

---

## Coordinator verification (2026-09-19)

Q1, the decisive question, was re-checked independently by the B.3.4 step coordinator against
retail `Runtime-x64.dll` (program re-asserted: function_count 15290), with
`disassemble_function(0x1800ace00, filter CALL,JMP)`. The COMPLETE list of call targets of
`?ReadFile@FileSystem@IO@@` is: `0x180025400` (`PackageFile::GetFileInfo`) at 0x1800ace3d; the
import at `[0x18014b0f0]` (SetFilePointer) at 0x1800ace61; `0x1800ace00` (itself, the recursive
packed read) at 0x1800ace7e; `0x1800c7ad0` (static `MemFree`, twice - the two `Filename`
releases) at 0x1800acec1 / 0x1800acefb; the import at `[0x18014b110]` (ReadFile) at 0x1800acf2a.
There is no critical-section thunk and no other callee. **Verified: retail's packed read takes no
lock; its correctness depends on a single loader.** Q2-Q9 above were NOT re-checked by the
coordinator. Q5 (does the EXE ever dispatch a load off the loader thread) is still open: it needs
`Application-x64.exe` as the Ghidra program and waits until the agents pinned to the retail DLL
have finished.
