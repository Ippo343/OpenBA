# Wave 6, lane C — `Video::ImageBuffer` / `ImageBufferData` / `IndexBuffer` / `VertexBuffer`

Branch `wave/upgrade-path-complete`, 2026-09-07.

Scope: the 22 exports listed under `=== LANE C ===` in `docs/runtime_port/wave6_lanes.txt`.
Files owned and touched by this lane, and no others:

* `runtime/src/video/buffers.cpp` — replaced wholesale (was a `return 0;` stub)
* `runtime/types/video/ImageBuffer.h` — extended (struct already existed from an earlier
  session; field names/semantics for +0x28/+0x2C/+0x38/+0x3C/+0x40 were corrected, see §3)
* `runtime/types/video/Buffers.h` — new: `IndexBuffer`, `VertexBuffer`
* This file.

This lane did not run the game, `make`, or any test harness — per the orchestrator's rules,
only `-fsyntax-only` was used to verify compilation. Every claim below is either **MEASURED**
(read directly out of the shipped `Runtime-x64.dll` — its export table, or a Ghidra
disassembly/decompile) or explicitly marked as a design choice with its reasoning. Live call
counts and divergence numbers are the wave lead's to fill in after integration.

## 0. The shape of this lane, in one paragraph

`IndexBuffer`/`VertexBuffer` carry no vtable of their own: `Create`/`Set` are two- or
three-instruction tail-call trampolines into a platform buffer object at `this+0x08`
(`dx11::IndexBufferData`/`VertexBufferData`, `dx11/types/VideoDX11Data.h` — not this lane's
file). `ImageBuffer::CopyToTexture`/`CopyFromTexture`/`Create` are the same shape, dispatching
through `this+0x60` (`dx11::ImageBufferData`, `dx11/src/imagebufferdata.cpp`). Because VideoDX11
is **already ours** (`dx11/`, loaded in every build — PLAN.md's Wave 6 "why this wave is
different" note), every one of those trampolines reaches a vtable this project already owns on
both sides of the port. That has one direct consequence repeated through §1: for those
functions there is no second implementation on the far side of the DLL export to diverge
from, so a live differential comparison would be comparing dx11's own code against itself
having done the same work twice (and in several cases mutating a GPU resource twice). Those
functions are ported byte-faithfully and registered, but diffed with `RT_DIFF_SHADOW(true, …)`
— present in the summary/census, not double-executing GPU work.

The other twelve functions are the true leaves: nine pure accessors with a full oracle, three
constructors, two destructors, and one held-back function.

## 1. Result table

All addresses/ordinals below are **MEASURED** — read from `Runtime-x64.dll`'s real export
directory with `tools/pe_exports.py`, cross-checked against the ordinal Ghidra reports in each
function's decompile comment (its own PE-loader-derived value, used here as the source of
truth over a hand-written ordinal-parsing script that had an off-by-a-field struct bug on a
first attempt and was discarded).

| # | Export | Retail | Ord | Ported? | Oracle |
|---|---|---|---|---|---|
| 1 | `??0ImageBuffer@Video@@QEAA@XZ` | `0x1800f2580` | 227 | **PORTED** | shadow (fixed constants; see §4) |
| 2 | `??1ImageBuffer@Video@@UEAA@XZ` | `0x1800f2610` | 609 | **PORTED** | post-state only (dtor) |
| 3 | `?Width@ImageBuffer@Video@@QEBAHXZ` | `0x1800f2530` | 4542 | **PORTED** | **FULL** — pure, call both |
| 4 | `?Height@ImageBuffer@Video@@QEBAHXZ` | `0x1800f24e0` | 3052 | **PORTED** | **FULL** |
| 5 | `?GetLevelCount@ImageBuffer@Video@@QEBAHXZ` | `0x1800f2420` | 2753 | **PORTED** | **FULL** |
| 6 | `?IsCompressed@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f2480` | 3101 | **PORTED** | **FULL** |
| 7 | `?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f24c0` | 3109 | **PORTED** | **FULL** |
| 8 | `?IsDynamic@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f24d0` | 3117 | **PORTED** | **FULL** |
| 9 | `?IsGenerated@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f24a0` | 3129 | **PORTED** | **FULL** |
| 10 | `?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f2460` | 3155 | **PORTED** | **FULL** |
| 11 | `?IsRendered@ImageBuffer@Video@@QEBA_NXZ` | `0x1800f2490` | 3156 | **PORTED** | **FULL** |
| 12 | `?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z` | `0x1800f3380` | 1869 | **PORTED** | shadow (trampoline, see §0) |
| 13 | `?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z` | `0x1800f33a0` | 1866 | **PORTED** | shadow (trampoline) |
| 14 | `?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z` | `0x1800f2680` | 1878 | **PORTED** | shadow (field stamp + trampoline) |
| 15 | `??1ImageBufferData@Video@@UEAA@XZ` | `0x1800f8e10` | 610 | **PORTED** | trivial (vptr reset only) |
| 16 | `??0IndexBuffer@Video@@QEAA@XZ` | `0x1800f3490` | 234 | **PORTED** | shadow (fixed constants) |
| 17 | `?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z` | `0x1800f3880` | 1879 | **PORTED** | shadow (trampoline) |
| 18 | `?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z` | `0x1800f38a0` | 3815 | **PORTED** | shadow (trampoline) |
| 19 | `??0VertexBuffer@Video@@QEAA@XZ` | `0x1800f55f0` | 491 | **PORTED** | shadow (fixed constants) |
| 20 | `?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z` | `0x1800f59e0` | 1885 | **PORTED** | shadow (trampoline) |
| 21 | `?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z` | `0x1800f5a00` | 3822 | **PORTED** | shadow (trampoline) |
| 22 | `?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z` | `0x1800f2840` | 3609 | **HELD BACK** | — see §5 |

21 of 22 ported. `RtVideoBuffersRegister()` returns **21**, each registered twice (mangled name
and retail address, matching the WAVE5_LANE_D pattern of "registered under both, which is what
makes the port live for address-only call sites too" — this lane's own call-site list in
`wave6_lanes.txt` shows every site as `export`-kind, so the address registration is
defense-in-depth rather than closing a known gap, but it costs nothing and the project's own
census tooling has twice been shown to miss address-only call sites — see
`reference_census_cannot_see_nonliteral_registrations.md` and
`reference_census_three_blind_spots.md` — so it is done unconditionally).

## 2. Export-name verification — MEASURED

All 22 names were resolved against the real export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with a small script wrapping `tools/pe_exports.py`'s
`exports()` before any body was written (a first attempt inverted the `(rva, name)` pairs into
a `{rva: name}` dict instead of `{name: rva}` and reported all 22 as missing — caught and fixed
before use, not a finding about the DLL). All 22 are present; none required a
`ResolveAddr(Module::Runtime, …)` fallback.

## 3. Struct layouts recovered

### 3.1 `Video::ImageBuffer` (0x68 bytes) — STALE COMMENT CORRECTED

The struct already existed in `runtime/types/video/ImageBuffer.h` from earlier work, with
+0x28, +0x2C, +0x38, +0x3C and +0x40 marked "unknown". This lane's decompiles resolve all
five, from two independent angles that agree:

* The ctor (`0x1800f2580`) disassembly: `MOV dword[+0x28],0 ; MOV qword[+0x2C],2` (one dword
  store, one qword store spanning +0x2C/+0x30 — so +0x2C and +0x30 are set together but are
  two separate 4-byte fields, `2` and `0`), then `MOV qword[+0x34],0 ; MOV qword[+0x3C],0 ;
  MOV dword[+0x44],1`.
* `Video::ImageBuffer::Create` (`0x1800f2680`, this lane's #14) stamps its ten parameters
  directly onto these offsets: `*(ImageType*)(this+0x28)=param1`, `*(Usage*)(this+0x2C)=param2`,
  `*(Format*)(this+0x30)=param3`, `*(uint*)(this+0x34)=param4`, `*(int*)(this+0x20)=param5`
  (width), `*(int*)(this+0x24)=param6` (height), `*(FilterMode*)(this+0x44)=param7`,
  `*(AddressMode*)(this+0x38..0x40)=param8..10`.
* `IsRenderBuffer`/`IsDepthBuffer`/`IsDynamic`/`IsGenerated`/`IsRendered` (this lane's #7-11)
  all read `*(int*)(this+0x2C)` against 0/1/3/4/5 and the mask `0xFFFFFFFA` — five data points
  independently confirming +0x2C is a `Usage`-shaped enum whose values classify the buffer.
* `dx11/src/imagebufferdata.cpp` (Lane L3, an earlier unrelated session) independently recovered
  the **same** offsets from the DX11 side of the same fields, under the names `dimension` and
  `kind` — this lane's `+0x28`/`+0x2C` naming was chosen to match that file rather than invent
  new terms for the same bytes.

Renamed in the header: `field28`→`dimension`, `field2C`→`kind`, `field38`→`addressU`,
`field3C`→`addressV`, `field40`→`addressW`, `field44`→`filter`. All five `static_assert`s were
kept at the same offsets, so no downstream file changes.

**`Width()`/`Height()` were also wrong**, not just under-documented. The header's old inline
`return width;` / `return height;` only reproduced the field read, but the actual exports
(`0x1800f2530`/`0x1800f24e0`) special-case two flag bits that override the stored size
entirely (bit 8 → `Device::_singleton`'s live width/height; bit 9 → half of it) and a third
(bit 6) that right-shifts the stored value by a device-configured downsample amount read from
`Configuration::_singleton+0x2C`. Nothing in this tree ever called the struct's own inline
methods (grep across `code/`, `engine/`, `standalone/`, `dx11/`, `runtime/` for `.Width()` /
`->Width()` found zero hits — every real caller goes through `fn_ImageBuffer_Width`, resolved
by mangled name via `ResolveExport`), so this was latent rather than a live bug; it is fixed in
the header now that the type is authoritative, and the real logic is in
`Video_ImageBuffer_Width`/`Height` in `buffers.cpp`.

### 3.2 `Video::ImageBufferData` (0x10 bytes) — new, minimal

Runtime's own abstract base class (distinct from `dx11::ImageBufferData` in
`dx11/types/VideoDX11Data.h`, which derives from it). This lane only owns its destructor
(`0x1800f8e10`), which is trivial — `*this->vtable = &Video::ImageBufferData::_vftable_;
return;` — so the struct only needs the two fields that decompile proves exist: the vtable
pointer and the owning-`ImageBuffer*` back-pointer at +0x08 (visible in the 1-arg ctor at
`0x1800f8c90`, which is **not** in this lane's scope and was read only to confirm the layout).

### 3.3 `Video::IndexBuffer` / `Video::VertexBuffer` (0x20 bytes each, identical)

No vtable. Ctor disassembly: `MOV dword[+0],0 ; MOV byte[+4],0 ; CALL PlatformConstruct`,
byte-identical between the two classes (`0x1800f3490` vs `0x1800f55f0` — same instruction
bytes, different `CALL` target). `Create`/`Set` are 3-instruction trampolines through `this+8`
(`MOV RCX,[RCX+8] ; MOV RAX,[RCX] ; JMP [RAX+off]`), `Create` at vtable+0x08, `Set` at
vtable+0x10, again byte-identical between the two classes. `GetSize()` (`0x180011e00` /
`0x180011db0`, **not** in this lane's scope — read only to confirm the field) is `return
*(int*)this;`, confirming +0x00 is the buffer's element count/size. `PlatformConstruct` links
the object into an intrusive doubly-linked list rooted at an exported `_firstBuffer` static
per class, guarded by `Device::_singleton`'s critical section — fields +0x10/+0x18 exist in the
struct purely to keep the byte layout (and therefore `this+8`'s offset) correct; this lane does
not touch or reproduce the list itself, since `PlatformConstruct`/`PlatformDestruct` are out of
scope (see §4).

## 4. Design choice: constructors and "out of scope" helper calls

Three functions this lane owns (`ImageBuffer`, `IndexBuffer`, `VertexBuffer` constructors) call
a `PlatformConstruct` that is **not** in this lane's 22-function scope — it is exported, but it
belongs to `Video::Device`'s buffer-factory machinery (lane B) and, for `IndexBuffer`/
`VertexBuffer`, to a global live-buffer registry this lane does not want to duplicate or
fork. Rather than reimplement it (which would either require depending on lane B's unowned
internals mid-wave or hand-rolling a second copy of Device's factory dispatch), these bodies
resolve the retail export once (`RT_ORIGINAL_ONCE`) and call through it, exactly reproducing
what retail's own constructor does. `ImageBuffer::~ImageBuffer` and the destructor pair for
`IndexBuffer`/`VertexBuffer` (the latter not in this lane's list at all — only the ctors and
`Create`/`Set` are) follow the same pattern for `PlatformDestruct` and
`Object::External::~External`.

This is also why the constructors are diffed with `RT_DIFF_SHADOW` rather than by constructing
a second scratch object with the real ctor and comparing: `PlatformConstruct` has a **global**
side effect (the `_firstBuffer` linked list, guarded by `Device::_singleton`'s critical
section). Running it a second time on a throwaway scratch object for comparison purposes would
splice that scratch object into the *same* live registry every real buffer uses, and this lane
has no matching `PlatformDestruct` call to unsplice it afterward without doing the destructor's
work twice as well. Since every field this lane's own ctor code sets is a fixed constant (no
input parameters at all), `RT_DIFF_SHADOW` just asserts those constants are what was written —
which is what a scratch-object comparison would have shown anyway, without the side effect.

## 5. Held back: `SaveImageToFile`

**Not ported.** `?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z`,
`0x1800f2840`, ordinal 3609. One caller in this tree: `code/Game/Game/w2_takescreenshot.cpp`
(the screenshot path — `fn_ImageBuffer_*` in `engine/funcs/engine_funcs.h` never resolves this
export; the screenshot capture pipeline calls a different, already-ported path and only reaches
this specific export for the final file write).

Reasons:

1. **File I/O side effect.** It writes a `.bmp` to disk. None of this lane's diff patterns
   (full pure-call, idempotent-replay, or the trampoline shadow used elsewhere in this file)
   apply to a filesystem write — there is no private-scratch variant of "write a file," and
   calling both implementations back-to-back would write the screenshot twice.
2. **Out of this lane's owned surface.** The decompile (146 lines, not reproduced in full here)
   builds paths via `System::Filename::GetPathDirectory`/`GetPathFilename`/
   `AdjustDelimiters` and `System::operator+` on refcounted `String`s, opens the file through
   `IO::FileSystem::CreatePath` and `IO::BinaryFileStream`/`IO::FileStream::Create`, and
   hand-assembles a 24-bit BMP header (`0x4d42` = "BM", 54-byte header, row stride
   `(width+1)*3 & ~3`) before writing rows back-to-front through raw `WriteFile` calls under a
   critical section taken on `System::MemoryAllocator::_singleton+0x640`. That is Wave 4 (IO)
   and `System::String` territory — this lane owns `ImageBuffer`/`IndexBuffer`/`VertexBuffer`,
   not `Filename`, `String`, or `IO::FileStream`. Porting it here would mean either reaching
   into those subsystems' unowned internals mid-wave or hand-duplicating them a second time in
   this file, both against the lane-ownership rule this wave runs under.
3. **Low traffic, low risk left on the DLL.** It runs once per user-initiated screenshot, never
   in a per-frame or per-draw path, so leaving it unrouted costs nothing measurable.

## 6. Stale comments elsewhere disproved by this work

None found outside this lane's own two files (§3.1 already documents and corrects the one in
`runtime/types/video/ImageBuffer.h`, which this lane owns). `dx11/src/imagebufferdata.cpp`'s
field-offset documentation for `Video::ImageBuffer` (+0x20 through +0x44) was checked against
every decompile in this lane and found to already be correct — no correction needed there.

## 7. What could not be verified

* The exact enumerator values/names for `ImageType`, `Usage`, `Format`, `FilterMode`,
  `AddressMode` (Video::ImageBuffer::Create's five enum parameters) are not recoverable from
  this DLL — no exported type-info or debug strings name them. `buffers.cpp` passes them as
  plain `int32_t` (ABI-identical to a 4-byte MSVC `enum` parameter), matching the numeric
  literals already used in `dx11/src/imagebufferdata.cpp`'s field-map comment rather than
  inventing enumerator names.
* `+0x50`/`+0x58` in `ImageBuffer` remain unnamed (`field50`/`field58`) — the copy ctor
  (`0x18001eb80`) touches them but that function is not in this lane's scope, and no export
  this lane owns reads or writes them, so there is no decompile evidence in-scope to name them
  from.
* This lane's ports were never run — no live call counts, no `BANISHED_RT_DIFF=1` run, no
  divergence numbers. That is the wave lead's integration step.

## 8. Build verification

`g++ -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -include
standalone/env_cache.h -I. -Iengine/types -Iengine/funcs -Icode/Game -Iruntime/types
-Iruntime/funcs -Idx11/types -Idx11/funcs -Istandalone -fsyntax-only runtime/src/video/buffers.cpp`
— **clean, zero warnings.** `runtime/types/video/ImageBuffer.h` and
`runtime/types/video/Buffers.h` were also independently syntax-checked standalone.

`RtVideoBuffersRegister()` returns **21**.
