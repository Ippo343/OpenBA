# WAVE 9 LANE FS — `IO::FileSystem::SetupDirectories`, the 722-instruction wall

**Base:** `12901b6` (attempt 09's notes). **Lane commit:** see §9.
**Kill switch:** `BANISHED_RT_FSDIRS=0` removes the ENTIRE lane — both halves,
all twelve rows — in one variable.

---

## 0. The stale-tree check, first, because five lanes before me needed it

My worktree was created at `965361d`, which predates the whole runtime port:
no `docs/runtime_port/` at all. Caught on the first command and
`git reset --hard`'d onto `wave/upgrade-path-complete` (`12901b6`) before any
analysis. **Six for six.** Whoever creates these worktrees is branching from a
fixed old commit; that is a harness defect, not six coincidences.

---

## 1. What the wall actually was

Lane ENG left the boot faulting at

```
[RtUnresolved] ?SetupDirectories@FileSystem@IO@@QEAAXXZ
Fault: EXEC of 0000000000000000
  #1  RtIo_FileSystem_Startup  +0x55
```

and sized it correctly: **722 instructions, thirteen distinct callees.** Its
list of twelve unported addresses was right in substance; measured again here
the callee set resolves to nineteen distinct call targets, of which nine were
unported exports, one an unexported internal (memset, `0x1800881b0`), one an
unexported internal that IS ported by address (`0x18002ace0`, the config map's
BST find — `runtime/src/system/configuration.cpp`), and the rest already ported
String/Filename/allocator rows.

The body is not complicated, it is just long: MSVC inlined `FileSystem::Exists`
and `String::operator=` (seven times) and emitted the same
`-pathres`/`-pathdat`/`-pathpkg` override three times verbatim. Semantically:

```
Filename root   = GetRootDirectory();                  // "<install>\"
Configuration*  cfg = *0x1801fc6f8;
String   cmdPath = root + "cmd.txt";
if (Exists(Filename(cmdPath))) {                        // a RESPONSE FILE
    FileInfo fi = OpenFile(cmdPath);
    if (fi.handle != -1) {
        buf = MemAlloc(fi.size + 1); memset(buf,0,...);
        ReadFile(fi, fi.size, buf); CloseFile(fi);
        Configuration::SetCommandLine(cfg, String(buf));
    }
}
String sub = "";  if (cfg has "bin")  sub = cfg["bin"];
String platform = GetPlatformName(cfg->platform);       // "Win"
String build    = GetBuildName(cfg->build);             // "Data"
this->sourceDirectory = Format("%s%s%s\\",   root, sub, "..\\resource");
this->dataDirectory   = Format("%s%s%s%s\\", root, sub, platform, build);
this->packDirectory   = this->dataDirectory;
if (cfg has "pathres") sourceDirectory = Format("%s%s%s\\", root, sub, cfg["pathres"]);
if (cfg has "pathdat") dataDirectory   = Format("%s%s%s\\", root, sub, cfg["pathdat"]);
if (cfg has "pathpkg") packDirectory   = Format("%s%s%s\\", root, sub, cfg["pathpkg"]);
```

Every literal was read with `read_memory`, not guessed: `0x180155698`
`"cmd.txt"`, `0x1801551f0` `""`, `0x1801556a0` `"bin"`, `0x1801556a8`
`"..\resource"`, `0x1801556b8` `"%s%s%s\"`, `0x1801556c0` `"%s%s%s%s\"`,
`0x1801556d0/d8/e0` `"pathres"/"pathdat"/"pathpkg"`, `0x180155cc4` `"Win"`,
`0x180155cd0/d8` `"Data"/"Eval"`.

**The reconstruction is confirmed against the disk, not just the decompile:**
`Format("%s%s%s%s\\", "<install>\", "", "Win", "Data")` is
`"<install>\WinData\"`, and `C:\GOG Games\Banished\WinData` exists. A
reconstruction that got the argument order or the trailing `\` wrong would not
have landed on a real directory.

---

## 2. Ordering by control-flow reach — and what that pruned

Lane ENG's method (enumerate the callees, check each against every
`RegisterRuntimeExport` literal in the tree, then name which one control flow
hits FIRST) is what made this lane tractable rather than a nineteen-function
slog. Two gate reads pruned five functions off the critical path:

### 2.1 `ExistsInPackfile` is NOT REACHED — and the constructor proves it

`FileSystem::Exists` is inlined here and its first arm is
`if (this->+0x18) { if (ExistsInPackfile(...)) return true; }`. The
constructor `??0FileSystem@IO@@QEAA@XZ` (0x1800ac7c0) writes

```
MOV word ptr [RCX+0x18],0x100        ; +0x18 = 0x00, +0x19 = 0x01
```

so the packfile gate is **zero** and the platform gate is **one**. The only
writer of +0x18 on this path is `FileSystem::Startup`, which sets it *after*
`SetupDirectories` returns (`runtime/src/io/filesystem_startup.cpp`, the line
right below the call). So the live arm is `GetFileAttributesW`, and
`?ExistsInPackfile@` — an export lane ENG's list had as a required callee —
is dead code here. Declined, with the instruction that makes it dead quoted in
the source.

### 2.2 `OpenFile` / `ReadFile` / `CloseFile` / memset are NOT REACHED

All four are inside `if (Exists("<root>cmd.txt"))`. There is no `cmd.txt` in
the install. Declined. The calls are still written out faithfully through
`RT_EXPORT_ONCE`, so if a `cmd.txt` ever appears the boot dies with those
names on `[RtUnresolved]` rather than silently.

### 2.3 `GetCommandLineString` is NOT REACHED — **conditionally**

Four call sites, every one behind a `Map_Find` on
`Configuration::_map`. Lane ENG established that `SetCommandLine` is handed
`""` by `standalone/main.cpp:1930` and returns before its parse loop, so the
map is empty and all four finds miss. **This inherits lane ENG's condition
exactly: pass any command line and `?GetCommandLineString@Configuration@`
becomes the next unresolved symbol.** Stated here so the next iteration is
not surprised by it.

---

## 3. Ported — twelve exports

| Export | Retail | Ord | Note |
|---|---|---|---|
| `?SetupDirectories@FileSystem@IO@@QEAAXXZ` | 0x1800abd00 | 4029 | the wall |
| `?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ` | 0x1800ad650 | 2878 | `GetModuleFileNameW` |
| `?GetPathDirectory@Filename@System@@QEBA?AV12@XZ` | 0x1800c6880 | 2836 | |
| `?GetPlatformName@Configuration@System@@…W4Platform@12@@Z` | 0x1800c5210 | 2850 | |
| `?GetBuildName@Configuration@System@@…W4Build@12@@Z` | 0x1800c5280 | 2470 | |
| `?SetLength@WideString@System@@AEAAXH@Z` | 0x1800ca9b0 | 3902 | **AEAA**, private |
| `?Set@WideString@System@@QEAAXPEB_W@Z` | 0x1800caf40 | 3823 | |
| `?AsUTF8@WideString@System@@QEBA?AVString@2@XZ` | 0x1800cacd0 | 1625 | |
| `??0WideString@System@@QEAA@AEBVString@1@@Z` | 0x1800cab50 | 513 | |
| `??0WideString@System@@QEAA@PEB_W@Z` | 0x18000a6e0 | 515 | dx11, §5 |
| `??1WideString@System@@QEAA@XZ` | 0x18000a730 | 716 | dx11, §5 |
| `?StrLength@WideString@System@@SAHPEB_W@Z` | 0x1800cab20 | 4121 | dx11, §5 |

Every mangled name was checked against Runtime-x64.dll's export directory with
`pefile` **before** a body was written. `SetLength` is the trap: it is
`AEAA` (private), and the `QEAA` spelling is not an export and would have
resolved null forever — the same shape as Wave 2's
`?AdjustDelimiters@Filename@System@@QEAAXXZ`.

Files: `runtime/src/system/widestring.{cpp,h}`,
`runtime/src/system/setupdir_helpers.{cpp,h}`,
`runtime/src/io/filesystem_setupdirs_win.cpp`.
Shared-file edits: `rt_system_port.cpp`, `rt_io_port.cpp` (one switch each) and
`runtime/types/io/FileSystem.h` (`_unknown10[8]` → the named `packDirectory`).

**Oracles.** Nine of the twelve carry a real replaying value oracle
(`GetRootDirectory`, `GetPathDirectory`, `GetPlatformName`, `GetBuildName`,
`WideString::Set` / `AsUTF8` / ctor(String)) — all are pure or replay onto a
scratch object, and all compare by TEXT because both sides allocate their own
buffer and could never share a pointer. `WideString::Set` snapshots `self->data`
BEFORE mutating and seeds the scratch with it: replaying an out-param function
on input our own write has destroyed is how a correct port reads as a 10%
divergence (`reference_oracle_must_snapshot_aliased_inputs.md`).

`SetupDirectories`' oracle replays the shipped body onto a **scratch
FileSystem** (retail reads only +0x18/+0x19 and writes only +0x00/+0x08/+0x10,
so a scratch object is a complete stand-in) and is **skipped unless the DLL's
OWN `Configuration::_singleton` is populated**, checked through
`ResolveExportRaw`. That is a correctness condition, not caution: retail
dereferences `[0x1801fc6f8]` directly, and once a mint points our
`RT_DATA_SLOT` at OUR storage the DLL's copy stays null. Replaying then would
fault *inside the oracle* and read as a port defect.

---

## 4. Two stale facts found, one of them a live hole

### 4.1 "WideString is not called by our tree" — **false since Wave 4**

PLAN §5 recorded `System::WideString` as uncalled and Wave 2 left it unported
on that basis. `runtime/src/io/filesystem_win.cpp` (Wave 4 lane D,
`FileSystem::GetSaveDirectory`) has called `?Set@WideString@` and
`?AsUTF8@WideString@` through `RT_EXPORT_ONCE` **since the day it landed, with
nothing behind either door** — two null calls at cutover on the save path,
which no instrument in this project reports because a `RT_EXPORT_ONCE` to an
unported symbol is indistinguishable from one to a ported one until it runs.

This lane closes both, and `filesystem_win.cpp` needs **no edit at all**,
because it already asks through the port-aware door. That is the door
conversion work of lane V2 paying off exactly as designed.

### 4.2 `tools/rt_mint_gen.py` currently REFUSES TO RUN — **not my change**

Lane ENG's attempt-08 prediction Q5 listed four mint rows still needed,
including `?_singleton@Configuration@System@@0PEAV12@EA` (ord 4803,
0x1801fc6f8) which `SetupDirectories` reads. The generator picks new symbols up
from a source scan, and two files now spell that name as a literal, so a
regeneration *would* add it. It cannot:

```
REFUSING TO GENERATE - verification failed:
  ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA: exported DATA in .data with no known size
  ?_hideDisplay@Error@System@@0_NA:                   exported DATA in .data with no known size
  ?_systemSecondsPerTick@Timer@System@@0NA:           exported DATA in .data with no known size
```

**All three sizes are readable straight off their own mangled names** —
`_N` = bool = 1, `N` = double = 8, `PEAV12` = pointer = 8 — so this is three
`EXTRA_SIZES` lines away from unblocked. It is not this lane's file to edit
mid-flight (the mint is lane C2's, and regenerating also pulls in ten new
BaseTypeInfo closure rows), but **no mint row can land for anyone until
somebody adds those three.** That blocks all four of lane ENG's Q5 rows, not
just mine.

**This lane does not need it.** With `cfg == nullptr` the port still produces
the correct three paths: `GetPlatformName` and `GetBuildName` **never
dereference `this`** (checked in the disassembly — RCX is not read in either
body), so they still answer "Win" and "Data" for the zero enums, and the four
`Map_Find` calls miss, which is the answer they give on an empty command line
anyway. The `if (cfg)` guards are documented as our-vs-retail difference, per
CLAUDE.md's rule, and the row closes either way.

---

## 5. Three more rows closed, found by reading the census AFTER

`runtime_census.py` names `dx11/src/devicedata.cpp` as the caller of FOUR
`WideString` exports. `AsUTF8` closed as a side effect of `GetRootDirectory`;
the other three had `ported=0` and are on `DeviceData::EnumerateInterfaces`,
which turns each adapter's UTF-16 `Description` into a `String` at **Video
startup on every boot** — a few frames behind this wall. Closing one wall and
leaving three null calls just past it would have cost an iteration, so they are
ported here: `??0WideString@…@PEB_W@Z` (eight instructions: zero and call
`Set`), `??1WideString` (the release idiom verbatim) and `?StrLength@` (wcslen,
unguarded, as retail's `CMP word ptr [RCX],AX` is).

The class is now closed against its census caller list: every WideString export
any file in this tree names is ported. The other 21 are named by nothing.

---

## 6. The one thing that is NOT transcribed from retail

The UTF-8↔UTF-16 conversions go through two unexported CRT-style dispatchers,
`0x1800c9fc0` (decode) and `0x1800c9b60` (encode). `0x1800c9b60` is six
instructions and a jump table at `0x1800c9d40`, so the encoding ids are
readable off the table — **2 = UTF-8, 4 = UTF-16LE** — but Ghidra cannot
recover the table's bodies as functions. That is the SAME WALL, ON THE SAME
PAIR, that Wave 2 lane A hit porting `String::Serialize`, and it made the same
disclosure there (`Utf16DecodeOne` / `Utf8EncodeOne` in `string.cpp`).

So the four codec helpers in `widestring.cpp` are written **from the
specification**, and that is stated in the file header rather than buried. What
was checked in the disassembly rather than assumed: table entry 4
(`0x1800c9c58`) rejects a lone surrogate (`LEA EAX,[R9-0xD800] ; CMP EAX,0x7FF ;
JBE <error>`) and returns 4 for a supplementary codepoint after
`SUB R9D,0x10000`. On the only caller chain that exists here the input is the
process's own module path — ASCII on every machine this has run on — so every
codepoint takes the one-byte and two-byte branches, and the oracles compare
against the shipped bodies on exactly that data.

---

## 7. Audits — both denominators, base `12901b6`

| audit | BEFORE | AFTER (at the 9-export commit) |
|---|---|---|
| `rawdoor_audit.py` | **483** sites, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **492**, **0**, 34 |
| `port_bypass_audit.py --by-name`, ADDRESS door | **200** sites, 0 reach a ported fn; ADDR-REG=33 DETOUR=3 NO-EXPORT=87 UNPORTED=77 | **201**, **0**; 33 / 3 / **88** / 77 |
| … NAME door | **410** raw-door sites, **0** bypass; OK-ORACLE=379 OK-DATA=3 | **419**, **0**; OK-ORACLE=**387** OK-DATA=**4** |
| … BY-NAME cross-check | **643** ported names, 399 statements, **0** missed | **652**, **407**, **0** |
| `rt_data_door_audit.py` | PASS; 3 `RT_RAW_OK`; 10 UNCLASSIFIED | PASS; 3; 10 — *unchanged* |
| `runtime_census.py` TOTAL | 4632 / **849** called / **615** ported / 72.4 % | 4632 / **850** / **616** / **72.5 %** |
| … ports with no caller | **71** exports, **69** addrs | **79**, **77** |

Every move accounted for, none waved away:

* **+9 raw-door / +9 NAME-door sites.** Exactly my nine oracle statements:
  eight `RtOriginal<>` replays (OK-ORACLE 379→387) plus one
  `ResolveExportRaw` (OK-DATA 3→4) — the guard that decides whether
  `SetupDirectories`' replay is safe. `0 UNGUARDED-CALL` and `0 bypass` hold.
* **+1 ADDRESS-door site, NO-EXPORT 87→88.** My one
  `ResolveAddr(Module::Runtime, 0x18002ace0)` in `ConfigHasKey`. It buckets
  NO-EXPORT because that address has no name in either build; it IS ported, by
  address, in `configuration.cpp`, which is why `UNPORTED` stays at 77.
* **+9 BY-NAME ported names but only +8 statements.** Nine registrations; the
  ninth raw-door statement names a DATA symbol, not a ported function.
* **census `called` 849→850, and the +1 is not one of my ports.** It is
  `?_singleton@Configuration@System@@0PEAV12@EA`, `ported=0`, caller
  `filesystem_setupdirs_win.cpp`. The census could not see it before because
  lane ENG reaches it through `RT_DATA_SLOT`, a macro the scanner cannot see
  through; my `ResolveExportRaw` literal is the first spelling it can read. So
  **the census now names §4.2's missing mint row as an unported dependency** —
  a side effect worth more than the number itself.
* **census `ported` +1 only, for nine registrations, and orphans +8.** Same
  fact from both sides. `ported` is a subset of `called`, and eight of my nine
  are reached either by a DIRECT C++ CALL from a sibling in this lane (not a
  resolve door at all) or through `RT_EXPORT_ONCE` from lane ENG's
  `FileSystem::Startup` — §7.2's documented blind spot. The one that counts is
  `?AsUTF8@WideString@`, because `dx11/src/devicedata.cpp` names it as a
  literal. **All eight "orphans" run on every boot.** The three §5 rows land
  the same way and are the reason to re-read this table after the second
  commit rather than trusting the numbers above for it.
* **`rt_data_door_audit` completely unchanged**, including UNCLASSIFIED at 10.
  My `ResolveExportRaw` of `Configuration::_singleton` is not flagged because
  that symbol is not MINTED, and the audit's rule is "no *minted* DATA symbol
  read through a raw door". If §4.2's mint row ever lands, that line becomes a
  legitimate `RT_RAW_OK` exemption (it is oracle-only and dies with the
  oracle) and should be marked as one.

---

## 8. No new link library

`GetModuleFileNameW` and `GetFileAttributesW` are both **KERNEL32**, which
every link already pulls in. `SA_LIBS` is untouched. Verified the way lane
ENG's `CoInitializeEx` should have been: not with `-fsyntax-only`, but with a
real `-c` compile using the Makefile's own `standalone-debug` flags
(`-std=c++17 -Wall -Wextra -g -O0 -D_DEBUG` and the full `-I` set) on all five
new/edited sources plus the files they touch — all clean, no warnings — and
then an `nm` cross-check that every `RtSystem::`/`RtIo::` symbol the three new
objects import is defined by an object in the set. Zero unresolved.

I did not build or run the game; that is the integrator's.

---

## 9. Falsifiable prediction for attempt 10

**P1 — `?SetupDirectories@FileSystem@IO@@QEAAXXZ` LEAVES the `[RtUnresolved]`
list**, and so do `?GetRootDirectory@FileSystem@IO@@`, the four WideString
rows dx11 asks for, `?GetPathDirectory@Filename@System@@`,
`?GetPlatformName@Configuration@System@@` and
`?GetBuildName@Configuration@System@@`.

**P2 — the next fault is `[RtUnresolved] ?LoadPackages@FileSystem@IO@@QEAAXXZ`
(0x1800ab460, ord 3209), EXEC of 0, and the stack frame DOES NOT MOVE.** It
symbolises inside **`RtIo_FileSystem_Startup`** again — the same enclosing
function as attempt 09 — because `LoadPackages` is the LAST call in that body
and `SetupDirectories` was the first. The offset moves *up within the frame*,
from `+0x55` to something larger; the depth stays at #1
`RtIo_FileSystem_Startup` / #2 `RtEngine_Engine_Startup` / #3
`Game::RunGameMain`.

This deliberately differs from the last two iterations, where the frame moved
one level DOWN. **A frame that moves down instead falsifies this**: it would
mean `SetupDirectories` itself faulted internally, and the symbol named would
say which of §2's declined callees was reached after all — which is exactly
the information the next iteration wants.

**P3 — two new `[RtDiff]` sites appear DLL-present**
(`IO::FileSystem::SetupDirectories` and `IO::FileSystem::GetRootDirectory`),
each `calls=1 divergences=0`, plus `System::WideString::Set` and
`::AsUTF8` at `calls=1` and `System::Filename::GetPathDirectory` at
`calls=1`. `SetupDirectories`' shadow prints
`src=…\..\resource\ | dat=…\WinData\ | pkg=…\WinData\`. If `dat` is not
`…\WinData\`, the Configuration enums at +0x20/+0x28 are not zero and §1's
"Win"/"Data" reading is wrong.

**P4 — `?_singleton@Configuration@System@@0PEAV12@EA` shows on
`[RtUnresolved]` exactly once with no crash**, and `cfg` is null inside
`SetupDirectories`. That is HARMLESS here (§4.2) and the three paths still come
out right — but it means the oracle in P3 will take its no-replay branch under
`BANISHED_RT_NO_DLL`, printing `no replay: the DLL's Configuration::_singleton
is null`. Seeing that string is the check that the guard did what it says.

**P5 — this lane adds NO new link error.** If the integrator's link fails, it
is not KERNEL32.

---

## 10. What I could not verify

No run of any kind. The two things most worth watching:

1. The four **spec-written codec helpers** (§6). They are exercised on ASCII
   only, and the oracles will say so — but a `calls=` of 0 on
   `System::WideString::AsUTF8` would mean `GetRootDirectory` never ran, not
   that the codec is fine (`reference_uncalled_port_is_not_verified.md`).
2. `SetupDirectories`' **scratch-FileSystem oracle**. It allocates a 0x48-byte
   stack object and lets the shipped body write three Strings into it, which
   this lane then releases. If that shows up as a leak or a double free with
   the DLL present, the fault is the oracle, not the port — set
   `BANISHED_RT_DIFF` off and the port is unaffected.
