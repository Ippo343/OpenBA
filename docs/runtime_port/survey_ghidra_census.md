# Runtime-x64.dll census

Ghidra was reachable, so all numbers below come from live queries against the `Runtime-x64.dll`
program. I passed the program explicitly rather than calling `switch_program`, to avoid disturbing
the other agent's session. Nothing was renamed or edited.

Sources: three `run_script_inline` Ghidra scripts (namespace aggregation, cross-namespace call-edge
matrix, naming coverage) plus two `pefile` scripts in the scratchpad (`exp.py`, `exp2.py`).

## 0. Profile-build DLL status

**`Runtime-x64-profile.dll` is NOT imported into the Ghidra project.** `list_project_files` on
project `Banished_RE` returns exactly six programs: `Application-x64.exe` (v54),
`Application-x64-profile.exe` (v5), `Runtime-x64.dll` (v8), `Tools-x64.exe` (v2),
`VideoDX11-x64.dll` (v2), `VideoDX9-x64.dll` (v2). This matches the `project.prp` listing.

The file does exist on disk, at
`C:\code\GIT\untitled_rev_eng_project\ToolKit\bin\x64\Runtime-x64-profile.dll`, and it is a
substantially richer binary than the retail one:

| | retail `Runtime-x64.dll` | `Runtime-x64-profile.dll` |
|---|---|---|
| file size | 2,790,888 | 3,188,224 |
| `.text` | 1,349,120 | 1,669,714 |
| exports | 4,926 | 5,182 |

It also carries a CodeView debug directory naming `C:\Projects\Buildy\bin\x64.Profile\Runtime-x64-profile.pdb`.
Importing it is cheap and would add 256 exported names and profiling instrumentation the retail
build strips. Worth doing before a serious port.

## 1. Namespace census

Totals from the Ghidra function manager: **15,109 functions iterated** (program metadata reports
15,290, the difference being external/thunk stubs), **1,209,608 bytes of function bodies**,
**4,633 functions are export entry points**, **2,467 still carry default `FUN_` names**.

The exported-symbol count from `pefile` is **4,926 named exports**, and grouping them by defining
scope agrees with Ghidra to within a few symbols.

| Namespace | Classes | Functions | Exported | Body bytes |
|---|---|---|---|---|
| UI | 73 | 1,625 | 1,625 | 193,399 |
| Graphics | 40 | 599 | 599 | 93,712 |
| Video | 35 | 573 | 573 | 54,751 |
| IO | 21 | 407 | 407 | 73,426 |
| System | 18 | 262 | 262 | 28,289 |
| Math | 10 | 228 | 228 | 21,503 |
| Object | 13 | 191 | 191 | 23,318 |
| Audio | 6 | 137 | 137 | 15,651 |
| Achievement | 6 | 118 | 118 | 8,078 |
| Engine | 7 | 108 | 108 | 14,817 |
| Collision2D | 3 | 76 | 76 | 18,051 |
| Volume | 5 | 64 | 64 | 8,589 |
| Input | 3 | 63 | 63 | 9,129 |
| Picking | 2 | 49 | 49 | 14,039 |
| Pathfinding | 3 | 45 | 45 | 13,086 |
| Save | 2 | 37 | 37 | 12,723 |
| Application | 1 | 26 | 26 | 2,450 |
| Utility | 2 | 16 | 16 | 5,029 |
| Debug | 4 | 8 | 8 | 28 |

Two non-engine buckets absorb the rest: **2,467 unnamed `FUN_` functions holding 407,017 bytes**
(a third of all code, and the largest single block in the binary), and **7,761 globally-named
functions holding 170,729 bytes** that are almost entirely C runtime routines auto-identified by
Ghidra (`qsort`, `_stricmp`, `_initterm`, `__C_specific_handler`). `UnDecorator` and `DName`,
70 functions and 17,973 bytes, are the MSVC name-demangler linked into the DLL, not engine code.

Every namespaced function is essentially exported: 4,740 namespaced versus 4,632 exported.
**The internal, non-exported engine code is the unnamed `FUN_` pool**, which is where the real
reimplementation risk sits.

Largest classes by function count: `Video::Context` 85, `UI::Element` 69, `Video::Device` 67,
`UI::DialogInstance` 62, `Video::ImageBuffer` 56, `IO::FileSystem` 54, `UI::UserInterface` 52,
`System::String` 48, `IO::PackageFile` 45, `Video::Configuration` 45, `Math::Vector3` 42,
`Graphics::GraphicsMesh` 42, `Graphics::MeshInstance` 41.

By bytes rather than count the picture shifts: `IO::PackageFile` 25,143, `Video::Context` 21,153,
`Collision2D::Scene` 14,743, `IO::FileSystem` 14,562, `UI::UserInterface` 12,833,
`Save::SaveLoad` 12,152, `Object::ExternalFactory` 10,952, `Graphics::MeshInstance` 10,430,
`UI::DocumentTextElement` 10,287. These are the dense, algorithm-heavy classes.

## 2. Layering

I built a full cross-namespace call-edge matrix rather than sampling. It is nearly acyclic and
gives a clean port order.

**Tier 0, true leaves.** `Math` calls nothing outside itself except internal helpers. `Volume`
calls only `Math` (1 edge). `Debug` calls nothing. `Utility` touches only `System` (4). These have
no engine dependencies at all.

**Tier 1, foundation.** `System` calls only KERNEL32 and CRT helpers, no other engine namespace.
It is the floor everything stands on.

**Tier 2.** `IO` depends on `System` (238) plus KERNEL32 and ADVAPI32. `Object` depends on
`System` (103) and `IO` (35).

**Tier 3, self-contained algorithm packages.** `Collision2D`, `Picking`, and `Pathfinding` each
depend only on `System` (11, 11, 12) and a trace of `Object`; `Picking` adds `Math` (6) and
`Volume` (4). They are independently portable and each is worth roughly 13,000 to 18,000 bytes.

**Tier 4, subsystems.** `Video` sits on `System` (72) and `Object` (17). `Graphics` sits on
`Video` (73), `System` (75), `Math` (45), `Object` (19). `Audio` and `Achievement` sit on `System`
and `Object`. `Input` sits on `System` (8) and `IO` (3), and reaches USER32 and DINPUT8 directly.
`UI` sits on `System` (401) and `Object` (79) with very heavy KERNEL32 traffic (472).

**Tier 5, top.** `Save` reaches down into `System` (38), `IO` (25), `Video` (10), `UI` (6),
`Graphics` (5). `Engine` is the only genuine integrator, calling `System` 35, `Video` 22,
`Object` 17, `IO` 14, `UI` 12, `Input` 9, `Audio` 9, `Application` 6, `Save` 1, `Achievement` 1.
`Application` is a thin 26-function USER32 shim that `Input` and `Engine` both call, so it is
better read as a platform-window leaf than as a top layer.

The only near-cycle is `Application` sitting below `Input` while being named like a top layer.
Port it early with `System`.

## 3. Foundation types

I measured inbound call edges, which is a better priority signal than function count.

| Class | Functions | Bytes | Inbound caller edges |
|---|---|---|---|
| `System::String` | 48 | 6,333 | 2,388 |
| `System::MemoryAllocator` | 30 | 4,169 | 1,778 |
| `System::Filename` | 18 | 2,737 | 986 |
| `Object::External` | 23 | 4,397 | 624 |
| `Object::Base` | 10 | 387 | 363 |
| `Object::ExternalFactory` | 37 | 10,952 | 144 |
| `System::WideString` | 29 | 2,946 | 108 |
| `IO::FileSystem` | 54 | 14,562 | 107 |
| `IO::Serializer` | 30 | 414 | 89 |
| `IO::Stream` | 8 | 204 | 56 |
| `Object::Internal` | 13 | 858 | 54 |

`System::String` and `System::MemoryAllocator` together account for over 4,100 call edges. They are
the unambiguous first port target, and they are small: 78 functions, 10,502 bytes combined.

**The containers are not exported at all.** `pefile` finds zero symbols whose defining scope is
`Collection`, yet `Collection` appears constantly inside other mangled names:
`?$Array@VString@System@@@Collection@@`, `?$Set@VFilename@System@@UDefaultAllocator@Collection@@`,
`?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@`. There is
also a `DefaultAllocator` policy type in that namespace. The count in `decomp/runtime.md` of
"Collection, 41 symbols" does not correspond to exported functions.

This is the single most important finding for planning. `Collection::Array`, `Set`, `LinkedList`,
and `Map` are **header-only templates inlined into every consumer**. They have no callable ABI
surface, their layouts must be recovered from field access in consumer code, and getting them wrong
silently corrupts every class that embeds one. The out-of-line remnants are visible only as three
hand-named helpers already in the Ghidra database: `Map_Insert`, `Map_Find`, and `DynArrayPush`.

`Object::Type`, `Object::BaseTypeInfo`, `Object::Factory`, `Object::InternalFactory`, and
`Object::ExternalFactory` form the reflection and handle-table layer, 80 functions and 14,684
bytes. `Object::Type` shows zero inbound direct calls because it is reached through the exported
per-class `TypeId` and `TypeName` accessors that every class carries.

Natural first-port set, roughly 34,000 bytes: `System::String`, `System::MemoryAllocator`,
`System::Filename`, `System::WideString`, `System::Time`, `System::Color`, `System::Random`,
`System::CRC`, the whole of `Math` and `Volume`, and the `Collection` templates recovered from
usage.

## 4. Size

| Section | Range | Bytes |
|---|---|---|
| `.text` | 0x180001000 - 0x18014a5ff | 1,349,120 |
| `.rdata` | 0x18014b000 - 0x1801f5bff | 699,392 |
| `.data` | 0x1801f6000 - 0x180200997 | 43,416 |
| `.pdata` | 0x180201000 - 0x180212bff | 72,704 |
| `.rsrc` | 0x180213000 - 0x1802a63ff | 603,136 |
| `.reloc` | 0x1802a7000 - 0x1802aadff | 15,872 |

Function bodies cover 1,209,608 of the 1,349,120 `.text` bytes, or 90 percent; the remainder is
padding and unclaimed data. Subtracting the CRT, the demangler, and the `FUN_` pool, the
**identified engine namespaces total about 610,000 bytes**, with UI alone at 193,399, or 32 percent
of that.

## 5. Existing Ghidra annotation coverage

Of 15,109 functions: 2,467 keep default `FUN_` names, 141 are auto-generated `thunk_` names, 4,740
sit in a demangled namespace, and 7,761 are globally named. Sampling the global-named set shows it
is overwhelmingly Ghidra's own C-runtime signature matches, not human work. Project-authored names
in the Runtime database number roughly a dozen: `ListCleanup`, `ModArrayCopy`, `ModEntry_CopyCtor`,
`ModEntry_Dtor`, `ModEntry_Assign`, `Map_Insert`, `Map_Find`, `PostDrawPush`, `PostUpdatePush`,
`DynArrayPush`.

There are 5,098 functions carrying a plate comment, but these are dominated by auto-generated thunk
and demangler annotations rather than analysis notes.

**Practical reading: Runtime-x64.dll is essentially un-annotated.** Its 4,633 exported functions
carry meaningful names for free from the MSVC mangling, which is why the namespace census looks
rich. The 2,467 unnamed internal functions holding a third of all code have had almost no
attention, and that pool is where the reimplementation work actually lives.
