# Wave 1 — Math + Volume

Landed 2026-09-06 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.
Wave 1 is the first wave that ports actual Runtime functions, and its real job is to
prove the Wave 0 mechanisms on production code before anything stateful depends on them.

## Result

| | |
|---|---|
| Ported | **70 functions** — Math 58 of the 59 our tree calls, Volume 12 of 12 |
| Census | `Math 98.3%`, `Volume 100%` of *called* functions (`tools/runtime_census.py`) |
| Diff oracle, DLL libm | **5,983,638 calls, 0 divergences across 32 sites** |
| Diff oracle, our libm | 90 divergences in 2 sites, every one 1 ULP, all sin/cos-fed |
| Defects found | 5 real ones — 2 of them PRE-EXISTING and unrelated to the ports |

Deferred, deliberately: `Graphics::InterpolatedState<Math::Transform>::Lerp`
(`0x18001aff0`). It is bucketed with Math only because of its template argument;
nothing decompiled in this wave pins `InterpolatedState<T>`'s layout, and guessing it
is precisely the trap PLAN.md §4.3 risk 4 warns about. It belongs to Wave 6 with the
rest of Graphics.

## What landed

| Piece | Files |
|---|---|
| Math layouts | `runtime/types/math/Math.h` — Scalar, Quaternion, Matrix, Transform, Transform2, Transform3; every offset `static_assert`ed, each pinned by a named decompiled function |
| Volume layouts | `runtime/types/volume/Volume.h` — Box (0x18), Rectangle (0x10), Frustum (0x64) |
| Ports | `runtime/src/math/vector.cpp`, `runtime/src/math/transform.cpp`, `runtime/src/volume/{box,rectangle,frustum}.cpp` |
| Registration | `runtime/src/math/rt_math_port.cpp` (`BANISHED_RT_MATH`), `runtime/src/volume/volume_port_init.cpp` (`BANISHED_RT_VOLUME`) |
| Wave scaffolding | `runtime/funcs/rt_port.h` — per-wave kill switch, `RtOriginal<>()` |
| Transcendental seam | `runtime/funcs/rt_libm.{h,cpp}` — `BANISHED_RT_LIBM=dll` |
| Run harness | `tools/rt_run.sh` |
| ABI invariant check | `tools/abi_retslot_audit.py` |

## The ABI rule this wave is built on

Every Math and Volume class has a user-declared constructor, so MSVC returns it BY
MEMORY: the caller passes a hidden return-slot pointer and the callee returns that
same pointer in RAX. The slot is an ARGUMENT, and it is the SECOND register for a
member (`RCX=this, RDX=ret, R8=arg`) and the FIRST for a static (`RCX=ret,
RDX=arg1`) — not the last, which is the natural thing to assume from the simple
`self, retval` cases. Ports are therefore plain free functions with the slot spelled
out, never C++ methods returning by value.

## Environment variables added

| Variable | Effect |
|---|---|
| `BANISHED_RT_MATH=0` | Leave the whole Math namespace on the DLL |
| `BANISHED_RT_VOLUME=0` | Leave the whole Volume namespace on the DLL |
| `BANISHED_RT_LIBM=dll` | Route Sqrt/Sin/Cos/Atan2 through Runtime-x64.dll's own CRT. **Diagnostic only** — it does not survive cutover. Its purpose is to answer "is this divergence just libm rounding?" in one run instead of by argument. |

## The five defects

### 1. `fn_Math_Transform3_Identity` was a wild write (PRE-EXISTING, exe-side)

`engine/funcs/engine_funcs.h` declared it `void*(*)()` — no hidden return slot — and
the comment claimed it "returns a pointer to a static identity transform". Both were
wrong. `?Identity@Transform3@Math@@SA?AV12@XZ` returns Transform3 by value and writes
0x30 bytes through RCX. Called with no argument it wrote an identity matrix over
whatever the previous call had left in RCX, on every menu scene load, at two call
sites (`menudialogloopt.cpp`, `menuscene.cpp`).

It hid for months because it is **self-consistent**: RAX comes back pointing at the
same address that was just corrupted, the caller hands it to `Graphics::Camera::Ctor`,
and the camera reads a perfectly valid identity matrix back out. Nothing looks wrong
until the 48 bytes land on something live.

What exposed it: the diff harness's own `RtDiffSite` statics became the victim, so a
site's `name` field became `0x3F800000` (1.0f) and faulted when printed.
`tools/abi_retslot_audit.py` now checks this invariant across every by-value-returning
export our tree resolves — 49 of them, all clean after the fix.

### 2. `Transform::FromDirection` reassociated a float sum (Wave 1 port bug)

Retail (`0x180021d70`) normalizes the up vector with `upY² + upX² + upZ²`; the port
summed `upZ² + upX² + upY²`. Float addition is not associative, so this diverged on
7 of 980 calls by roughly 20 ULP, and through `FromLookAt` as well.

This is the divergence that justifies the whole oracle. It is far too rare and too
small to find by playing the game, and it was separated from the surrounding 1-ULP
libm noise by exactly one experiment: it *persisted* under `BANISHED_RT_LIBM=dll`
while the sin/cos sites went silent.

### 3. Four exports resolved under names that do not exist (PRE-EXISTING, exe-side)

`?Length@Vector2@Math@@QEBAMXZ` and `?LengthSquared@Vector2@Math@@QEBAMXZ` assumed a
float return; the real exports return `Scalar` through a hidden slot.
`?Normalize@Vector2@Math@@QEAAAEAV12@XZ` assumes an in-place overload the DLL does not
have at all. `?operator==@Vector3@Math@@...` is a mangling MSVC never emits
(`??8Vector3@Math@@QEBA_NAEBV01@@Z` is the real one). All four had resolved to null
forever, so `worldgen_f390_coastline.cpp` and `worldgen_f390_ranges.cpp` silently ran
inline fallbacks and two `Vector3::operator==` call sites always took their guard
branch. All four now resolve to the real exports.

**Consequence worth knowing:** the ranges fallback computed `v /= len` where retail
computes `v * (1/len)`. Those are not bit-identical, so map generation for a given
seed shifts very slightly. That is a move toward faithfulness, not away, but any
seed-pinned baseline recorded before this commit is no longer directly comparable.

### 4. `Quaternion::operator*` had the wrong term in z (Wave 1 port bug, latent)

Retail (`0x18001e280`) computes the z row as
`((a.z*b.w + a.y*b.x) - b.y*a.x) + b.z*a.w`. The port's final term was
`b.z * a.y` — it used the quaternion's y where retail uses w, so every
quaternion product was wrong in z.

It survived the entire wave undetected because **nothing called it**.
`Quaternion::operator*` and `Quaternion::Rotate` had zero calls in every diff
run; the only Wave 1 path into them appeared when
`Transform::operator*(Vector3)` was added at the end (see defect 5), at which
point they diverged on 82% and 100% of calls respectively. A port with no
callers is not a verified port, however green the summary looks — check the
`calls=` column, not just `divergences=`.

### 5. Two assigned functions were reported as ported but were absent

`Transform::operator*(const Transform&)` (`0x18001e3c0`) and
`Transform::operator*(const Vector3&)` (`0x18001e200`) were reported complete but
had been conflated with their `Transform2`/`Transform3` namesakes and never
written. `tools/runtime_census.py` caught the gap; the lane's own summary did not.

Both had to be transcribed from the DISASSEMBLY rather than the decompilation:
Ghidra cannot track R8 across a call that itself has a hidden return slot, so it
drops the third argument of the inner `Quaternion::Rotate` and
`Quaternion::operator*` calls, and the decompiled body reads as though `this` is
unused. The listing shows `other` supplying the rotation on both inner calls and
`this` supplying the operands:

    ret.translation = other.rotation.Rotate(this->translation) + other.translation
    ret.rotation    = other.rotation * this->rotation

## A harness lesson: the oracle must not corrupt its own experiment

Adding `Transform::operator*(Vector3)` made the diff report ~11,000 divergences
that were not real. `ret` legitimately aliases `v` at most call sites
(`vec = transform * vec;`), and retail is safe under that aliasing because it
consumes `v` inside `Rotate` before storing anything — and so are we. The DIFF is
not: it runs the original AFTER ours has written `ret`, so the original was being
handed an input we had just overwritten.

The tell was that "theirs" was CONSTANT across calls while "ours" varied. A real
port bug produces two values that both track the inputs. Any diff wrapper for a
function whose output can alias its input must snapshot the inputs first; both
`operator*` overloads now do.

## Harness fixes made along the way

* **The diff summary used to crash.** It dereferenced `site->name` unconditionally, so
  one corrupted static turned the entire report into an ACCESS_VIOLATION — destroying
  the very output the run existed to produce. `RtDiffRegister` now logs
  `site[N] @addr = name` as sites appear, and the summary validates a site before
  printing it, reporting the index instead of faulting.
* **The Frustum comparison read padding.** `sizeof(Frustum)` is 0x64 but only 0x61
  bytes are defined; retail never writes the three tail pad bytes and neither do we.
  Diffing all 0x64 compared stack garbage against a zeroed scratch struct and
  manufactured a divergence whenever the stack happened to be dirty. Now only the
  defined prefix is compared. A harness that cries wolf is worse than no harness.
* **`<windows.h>` was leaking into `runtime/src/`** via `rt_port.h` →
  `standalone/resolve.h`, which PLAN.md §4.1 forbids. It broke exactly the things you
  would expect and nothing else: `<wingdi.h>` declares a function named `Rectangle`,
  so `Volume::Rectangle` would not name a type, and `<windef.h>` still `#define`s the
  Win16 `near`/`far` keywords away, so a frustum could not have a near plane.
  `rt_port.h` now declares the four seam entry points itself.

## Notes for later waves

* **No detours were installed.** Registration redirects OUR callers only; the DLL's
  own internal calls to `Math::` still land in the DLL. That is harmless while the DLL
  is loaded and these classes are pure, but it means the diff figures above cover our
  call sites, not the DLL's. Wave 2's allocator is the first place where this stops
  being acceptable.
* **Two sites still diverge by 1 ULP** under our own libm (`Vector3::FromYawPitch`,
  `Transform::FromYawPitchOffset`). Both are sin/cos fed and both go to zero under
  `BANISHED_RT_LIBM=dll`. What to do about MinGW-vs-MSVC transcendental rounding is a
  cutover question, not a Wave 1 one — but it is now a measured quantity rather than
  a worry.
* **Trust the census over a lane's self-report.** Two assigned functions
  (`Transform::operator*(Transform)`, `Transform::operator*(Vector3)`) were reported
  as ported but were absent — conflated with their `Transform2`/`Transform3`
  namesakes. `tools/runtime_census.py` caught it; the summary did not.
