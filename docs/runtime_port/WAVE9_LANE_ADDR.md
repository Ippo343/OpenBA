# Wave 9 lane ADDR — the orphan ADDRESS doors

Branch `wave9/lane-addr`, cut from `3c26956`. One commit landed (`bdef5e1`, the
instrument fix). The rest of this document is decode and measurement; see
**HONEST LIMITS** for what was NOT done and why.

## 0. The lane was stopped early by an environment hazard, not by the work

The concurrent lanes share ONE session working-directory pointer. Three times in
~20 minutes another lane's worktree/branch operation moved my HEAD mid-task:

1. My first commit landed on `wave9/lane-uif` (created from `fd30180` and checked
   out into my directory between two of my commands). **Repaired**: commit moved
   to `wave9/lane-addr`, `wave9/lane-uif` reset to `fd30180`, its exact creation
   point. Lane UIF had no commits and lost nothing.
2. My worktree filled with other lanes' uncommitted edits (`context.cpp`,
   `internal.cpp`, `array.cpp`, `rt_math_port.cpp`, `dialog.cpp`,
   `SpriteContext.h`, untracked `wave9_lane_gfxr.cpp`, `external_set.{cpp,h}`).
   All left untouched. **This contaminated my first measurement** — see §1.
3. A second, uniquely-named worktree was taken over by lane GFXR, and my
   `git commit --amend` amended THEIR in-progress merge.
   **Outstanding damage I could not repair**: `wave9/lane-gfxr-work` points at
   `3dc79c4`, carrying MY commit message. Content is intact — `3dc79c4` has a
   byte-identical tree (`92fefc7b`) and identical parents (`ecd0ec9`,`89416c6`)
   to `fd30180`; only the message differs. Fix is
   `git branch -f wave9/lane-gfxr-work fd30180`, which git refused for me
   because lane GFXR has it checked out.

I stopped committing after that. Everything below is read-only analysis, which
cannot corrupt another lane.

## 1. THREE of my nine were not doors at all

The brief handed me nine addresses. Six are real. Three dissolved on measurement.

### 1a. `0x18006f330` — the instrument was manufacturing work

`tools/addr_door_audit.py` had no notion of a comment. It matched
`ResolveAddr`/`RegisterRuntimeAddr` text inside comment blocks and string
literals as if it were live code. `engine/funcs/engine_funcs.cpp:3200` sits
inside a comment block whose entire purpose is to record that the code was
**DELETED**:

```
//     ResolveAddr(Module::Runtime, 0x18006f330)   // "might be in exe"
```

There is no call. Worse, that same comment identifies `0x18006f330` as **0x90
bytes INSIDE the body of an unrelated internal** (`FUN_18006f2a0`, entry
`0x18006f2a0`, body ends `0x18006f3ca`) — so the instrument was pointing a lane
at a mid-function address to "port". Taking it would have jumped into the middle
of another function.

This is `reference_documenting_a_check_can_delete_it.md` **inverted**: there, a
comment deleted a real check from a scanner; here a comment invented a defect.
A scanner reading source as flat text cannot tell prose from code.

Fixed in `bdef5e1` with `strip_comments_and_strings()`, which blanks `//`,
block comments and string/char literals, replacing each character with a space
and preserving every newline, so line numbers and columns are unchanged and the
line-based reporting needed no adjustment. Quote scanning is **bounded to a
single line**: an unmatched quote the scanner does not model (a `1'000` digit
separator, an apostrophe in a macro) would otherwise swallow the rest of the
FILE and silently delete every real door below it — failing open in the
direction that HIDES work, which is the failure being fixed.

Measured on a CLEAN tree. (My first run, on the contaminated tree of §0.2, gave
296/199/1189/650 — those numbers are wrong and are not the ones below.)

|                        | before | after |                              |
|------------------------|-------:|------:|------------------------------|
| by-address call sites  |    295 |   270 | 25 rows were prose           |
| distinct addresses     |    198 |   192 | 6 comment-only               |
| registered addresses   |   1186 |  1186 | **negative control, unmoved**|
| — of which literal     |    647 |   647 | **no registration stripped** |
| UNREGISTERED           |     16 |    15 | set-diff = exactly `0x18006f330` |
| non-literal call sites |     77 |    15 | 62 of 77 were prose          |

The unmoved **647** is the control that matters: had the stripper eaten code,
literal registrations would have fallen. The set-diff of the `--csv` rows is
exactly one line; the other 15 are byte-identical including line numbers.

The 77 → 15 collapse also retires most of a number this tool has been reporting
as a genuine blind spot. Only 15 call sites are really invisible to the literal
scan.

### 1b + 1c. `0x1801fe7c0` / `0x1801fe7e0` — already closed, and the brief's model of them was wrong

The brief said these are "one and two 0x20-strides past the MainLoop singleton
at `0x1801fe750`" and therefore probably new mint rows. **That is wrong**, and
acting on it would have introduced a split-brain bug.

`0x1801fe7b8` is `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (ord 4794) — note
`VLoader@2@A`, a Loader **object BY VALUE**, not a pointer. So `0x1801fe7c0` and
`0x1801fe7e0` are **fields inside that object**, at `+0x08` and `+0x28`. Our
tree already names them that way (`g_pLoaderFlags`, `g_pLoaderCompletionEvent`).

`standalone/rt_data_mint.cpp:598-603` **already registers exactly those two
interior offsets** into the minted `_loader` storage:

```
RegisterRuntimeAddr((uintptr_t)s.dllVa + 0x08, (char*)s.storage + 0x08);
RegisterRuntimeAddr((uintptr_t)s.dllVa + 0x28, (char*)s.storage + 0x28);
```

and `rt_data_mint_tables.inc:1673` mints `_loader` at `0x1801fe7b8` with size
56, which covers both. They read as unregistered only because the registration
goes through a variable (`s.dllVa + 0x08`) — the REG_LIT blind spot the tool's
own docstring already declares.

**Had I followed the brief and minted them as two independent rows**, they would
have received their own storage, `g_pLoaderFlags` would have pointed at memory
unrelated to the Loader the engine actually reads, and the aliasing the mint
carefully preserves would have been broken. This is a case where "registering an
address is not automatically safe" bites in the *opposite* direction from the
usual one: the danger was not a null left deliberately, it was a second copy.

Do not re-farm these two.

## 2. The six real doors

All three export names in the brief were verified by me against the shipped
export directory with `pefile`, and all three are **correct** this time
(ords 3601, 3311, 3310).

| address | identity | status |
|---|---|---|
| `0x1800dd3c0` | `?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` ord 3601 | decoded, NOT ported |
| `0x1800c4930` | `?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z` ord 3311 | decoded, NOT ported |
| `0x1800c4a00` | `?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z` ord 3310 | decoded, NOT ported |
| `0x1800042a0` | `ExternalBasePtr<T>::Serialize` instantiation A | decoded, NOT ported |
| `0x180005dc0` | `ExternalBasePtr<T>::Serialize` instantiation B | decoded, NOT ported |
| `0x180052640` | `Audio` per-element bulk serialize | decoded, NOT ported |

### 2a. `MainLoop::Run` — decoded in full, and it answers the door-growth question

`0x1800dd3c0`..`0x1800dd4ec`, ~60 instructions. RCX = `MainLoop*`,
RDX = `UpdateLoop*`.

```
Run(self, loop):
    t0 = QPC()                                     # import at 0x18014b1a8
    t0 = (t0 - _systemInitialTick) * _ticksPerSecond / _systemTicksPerSecond
    self->currentLoop(+0x20) = loop
    self->startTime(+0x30)   = t0
    loop->vtbl[+0x08]()                            # UpdateLoop::Startup
    h = self->(+0x78)
    Input::Device::Update(  h->(+0x6e0), &self->(+0x48) )   # 0x1800e1490
    Audio::Device::Update(  h->(+0x6d0), &self->(+0x48) )   # 0x1800de520
    Application::PlatformUpdate( h->(+0x690) )              # 0x18009fdf0
    MainLoop::Update( self, &self->(+0x48) )                # 0x1800dd070
    while (self->(+0x19) == 0) {
        self->(+0x60) = now()
        MainLoop::Frame(self, 0)                            # 0x1800dd100
        self->(+0x70) = now()
        self->(+0x58) = self->(+0x70) - self->(+0x60)
    }
    self->currentLoop->vtbl[+0x10]()               # Shutdown
    if (self->currentLoop) self->currentLoop->vtbl[0](1)     # deleting dtor
```

The flag at `+0x19` is tested at the top (skipping the loop entirely) and again
at the bottom — a `do{}while` entered conditionally, not a `while`.

**Door growth, measured.** Run's five internal callees:

| callee | identity | ported today? | door if Run is ported |
|---|---|---|---|
| `0x1800de520` | `Audio::Device::Update` | **yes**, `audio_device.cpp:284` (by address) | no |
| `0x18009fdf0` | `Application::PlatformUpdate` | **yes**, `rt_application_port.cpp:37` | no |
| `0x1800e1490` | `?Update@Device@Input@@` ord 4482 | no — lane INPUT's, 61 instrs | **NEW** |
| `0x1800dd070` | `?Update@MainLoop@Engine@@` ord 4496 | no | **NEW** |
| `0x1800dd100` | `?Frame@MainLoop@Engine@@` ord 2377 | no | **NEW** |

So porting `Run` alone closes 1 door and opens 3 — but every one of the three is
**exported**, so they surface as *named* residue in
`unsatisfied_export_scan.py`, not as silent address doors. That is the whole
point of the lane: the exchange converts one **silent** null call into three
**named** ones. It is still a net increase in the counted surface, and the
integrator must expect the number to go UP, not down.

The Timer globals `0x1801fe720/730/738` are already minted
(`rt_data_mint_tables.inc:1698-1701`), and the QPC call at `0x18014b1a8` is the
DLL's own IAT — a port must call `QueryPerformanceCounter` directly rather than
resolve that slot, which will not exist at cutover.

### 2b. `MainLoop::Update` — small, self-contained, ZERO new doors

`0x1800dd070`..`0x1800dd0f7`, ~35 instructions, **all dispatch is virtual**:

```
bool Update(self, const Time& t):
    if (!self->currentLoop->vtbl[+0x18](t))   # UpdateLoop::Update
        if (!self->nextLoop(+0x28)) return false;
    else
        if (!self->nextLoop) return true;
    self->currentLoop->vtbl[+0x10]()          # Shutdown
    if (self->currentLoop) self->currentLoop->vtbl[0](1)   # deleting dtor
    n = self->nextLoop; self->nextLoop = 0; self->currentLoop = n
    n->vtbl[+0x08]()                          # Startup
    self->currentLoop->vtbl[+0x18](t)         # Update
    self->(+0x18) = 1
    return true
```

This is the cheapest item in the lane: it introduces **no new address door at
all** and it removes one of `Run`'s three. Whoever picks up `Run` should take
`Update` in the same commit.

`Frame` (`0x1800dd100`..`0x1800dd3ab`) is 683 bytes / ~180 instructions with six
Win32 imports, an indirect `call [rbx+rax]` dispatch table, and four internal
calls (`0x1800e1490` Input::Device::Update, `0x1800de6f0`
`?FreeChannelPlatform@Device@Audio@@` ord 2380, `0x1800dd070` Update,
`0x1800dcd40` `?Draw@MainLoop@Engine@@` ord 2251). It is a lane of its own, and
`Draw` in its closure is where lane CTX's seven `Video::Context` setters go live.

### 2c. `ParseValue` / `ParseCommand` — fully decoded, both helpers ALREADY ported

Both are `CA` statics returning `String` **by value**: RCX = hidden return slot,
RDX = `int&`, R8 = `const String&`, and RAX returns the retval pointer. Both
existing typedefs in the tree already model the hidden slot correctly
(`engine_funcs.h:323/329` and `configuration.cpp:122`), so there is no
retslot defect to fix — but a new typedef that omits it would be a wild write.

```
String ParseValue(int& i, const String& s):          # 0x1800c4930, 62 instrs
    while (i < s.GetLength() && (s[i]==' ' || s[i]=='\t')) ++i;
    start = i;
    while (i < s.GetLength() && s[i]!=' ' && s[i]!='\t') ++i;
    return s.SubString(start, i-1);                  # INCLUSIVE end

String ParseCommand(int& i, const String& s):        # 0x1800c4a00, 109 instrs
    while (i < s.GetLength() && (s[i]==' '||s[i]=='\t')) ++i;
    while (i < s.GetLength() && s[i] != '/') ++i;
    if (s[i] != '/') { ret.data = nullptr; return ret; }   # empty String
    ++i;
    while (i < s.GetLength() && (s[i]==' '||s[i]=='\t')) ++i;
    start = i;
    while (i < s.GetLength() && s[i]!=' ' && s[i]!=':') ++i;
    end = i;
    if (s[end]==' ' || s[end]==':') i = end+1;
    return s.SubString(start, end-1);
```

Two details a port must not smooth over:

* The `s[i] != '/'` test and the `s[end]` test read **without a bounds check**.
  This is faithful and safe: `System::String` is NUL-terminated, so `s[len]`
  is a legal read of `'\0'`, which is neither `'/'` nor `' '` nor `':'`. Do not
  "fix" it with a guard — the guard would change the result.
* The empty return writes `ret.data = nullptr` **directly** (`MOV [R14],RBP`
  with RBP=0). It does not construct an empty String. A port that calls a
  String ctor here allocates a block retail never allocates.

Both callees are **exported and already ported**:
`?GetLength@String@System@@QEBAHXZ` (`0x1800063c0`, ord 2750) and
`?SubString@String@System@@QEBA?AV12@HH@Z` (`0x1800c9230`, ord 4124, registered
by name AND address at `string.cpp:1143`/`1183`). **So porting these two closes
2 address doors and opens ZERO.** They are the best value in the lane.

They also close two *name* doors: `runtime/src/system/configuration.cpp:245,256`
reaches both through `RT_EXPORT_ONCE(ParseFn, ...)`, and that file's registration
block lists them as DELIBERATELY ABSENT. Porting them lets
`Configuration::SetCommandLine` handle a non-empty command line, which per that
file's own banner is currently the next fault at cutover for anyone who launches
`reimpl-x64.exe` with an argument.

**Reachability caveat, stated because it decides the verification plan.** The two
*address* doors at `engine_funcs.cpp:1727/1729` assign `fn_ParseCommand` /
`fn_ParseValue`, and those two pointers are **never invoked anywhere in the
tree** — the only other references are two `printf`s at `engine_funcs.cpp:3786-87`.
So these two address doors are dead diagnostics; the live reach is the
`RT_EXPORT_ONCE` name door in `configuration.cpp`. A port here would land at
`calls=0` on every current boot (no harness passes a command line — every knob
is an env var), so it **must** be driven by a probe that feeds a synthetic
command line, or it verifies nothing
(`reference_uncalled_port_is_not_verified.md`).

### 2d. The two `ExternalBasePtr<T>::Serialize` instantiations

`0x1800042a0` and `0x180005dc0` are **the same function body** — I diffed them
instruction by instruction — differing only in one global:

* `0x1800042a0` type-id at `0x1801fae08`
* `0x180005dc0` type-id at `0x1801f9908`

confirming `goal_serialize.cpp`'s banner assumption that these are two template
instantiations. Shape (RCX = `ExternalBasePtr*`, RDX = `Serializer*`):

```
mode = ser->stream(+0x10)->vtbl[+0x10]()
if (mode == 3) {                                  # the load/relink path
    if (!self->ptr) return;
    Filename p = External::GetPath(self->ptr);    # 0x180006250 ord 2835
    Base* o = ExternalFactory::FindOrLoad(*(void**)0x1801fc6c8, &p);   # 0x1800bf320 ord 2359
    if (o) o->IsDerivedFrom(TYPEID);              # 0x1800c0390 ord 3110
    ... refcount decrement at [rax-0x4] on the temporary ...
} else { ... write path at +0x9e ... }
```

All three callees are **exported** (2835, 2359, 3110). The two type-id globals
are `.data` and belong to the mint, not to a body. This is tractable work; I ran
out of lane before the write path and the temporary's lifetime were decoded, and
I will not guess at a `Serialize` — see LIMITS.

### 2e. `0x180052640` — the audio per-element bulk serialize

Fully decoded, `0x180052640`..`0x1800526f8`. Signature matches the typedef
`audio.cpp` already uses: `void (Serializer* self, int32_t count, void* elems)`,
`count` in EDX, `elems` in R8. `self+0x8` is the ReaderWriter. Stride **0x40**:

```
if (count <= 0) return;
for (e = elems; count--; e += 0x40) {
    rw->vtbl[+0x28](rw, 3, e+0x00, 0)
    rw->vtbl[+0x98](rw, e+0x0c, 0)
    call 0x180051790(e+0x10, self)
    rw->vtbl[+0x90](rw, e+0x18, 0)
    rw->vtbl[+0x98](rw, e+0x24, 0)
    rw->vtbl[+0x80](rw, e+0x28, 0)
    call 0x180051e00(e+0x30, self)
}
```

**Two warnings for whoever ports it.**

1. The vtable offsets used here (`+0x80/0x90/0x98`) are the **retail** IO
   ReaderWriter offsets. `reference_encoding_vtable_offsets_differ_between_builds.md`
   records that these differ between retail and the profile DLL
   (+0x78/0x98/0xa0 vs +0x90/0xb0/0xb8). Read them from **retail**; the profile
   build's NAMES win but its OFFSETS must not.
2. `0x180051790` and `0x180051e00` are **NOT EXPORTED** and not ported, so this
   one closes 1 door and opens **2**. (`0x180051390`, the array resize beside it
   in `audio.cpp:762`, is already registered — it is not on the watch list.)

## 3. HONEST LIMITS

* **Nothing in §2 was ported.** One commit landed, `bdef5e1`, and it is the
  instrument fix only. Six address doors remain open exactly as I found them.
  The reason is the environment hazard in §0, not a technical blocker: after my
  `--amend` damaged another lane's branch I judged that continuing to write and
  commit code was more likely to destroy other lanes' work than to close a door.
  That is a judgement call and the lead may disagree.
* **The decodes in §2 are read-only analysis and have NOT been executed,
  diffed, or oracle-verified.** They are disassembly reads. Every one of them is
  a hypothesis until a diff oracle with a non-zero `calls=` count says otherwise.
  Do not treat the pseudocode as verified behaviour.
* **`0x1800042a0`/`0x180005dc0`: the write path (mode != 3) and the temporary
  `Filename`'s lifetime/refcount handling are NOT decoded.** I have the read
  path and the identity. That is not enough to port a `Serialize`, and I am not
  going to guess at one.
* **My first measurement was contaminated** by other lanes' uncommitted edits
  and reported 296/199/1189/650. I re-ran on a clean tree and the corrected
  numbers are in §1a. The conclusion and both controls were unchanged, but the
  figures I originally committed were wrong and were amended.
* **The `--check`-style audits the brief asked for
  (`port_bypass_audit.py`, `rawdoor_audit.py`, `detour_target_audit.py`,
  `abi_retslot_audit.py`, `runtime_census.py`) were NOT run.** My change touches
  one Python file and adds no C++, no registration and no detour, so its effect
  on every one of those denominators is structurally zero — but I did not run
  them, and "structurally zero" is an argument, not a measurement. `git status`
  in a clean checkout of `wave9/lane-addr` shows one file changed.
* **No build and no game run.** I never acquired the game-dir lock because I
  never needed it. `abi_retslot_audit.py` in particular is unrun, so my claim in
  §2c that the two existing Parse typedefs already model the hidden return slot
  correctly rests on reading them, not on the audit.
* **Outstanding damage to another lane**: `wave9/lane-gfxr-work` still carries my
  commit message on `3dc79c4`. Tree and parents are identical to `fd30180`, so
  no content is lost, but someone with that branch un-checked-out must run
  `git branch -f wave9/lane-gfxr-work fd30180`.

## 4. What I DECLINED, with the measured blocker

| item | measured blocker |
|---|---|
| Porting `MainLoop::Run`+`Update` | Not a technical blocker — decoded and tractable. Declined on the §0 hazard after damaging another lane's branch. `Frame` (683 bytes, 4 internal callees, the `Draw` subtree) genuinely is a separate lane. |
| Porting `ParseValue`/`ParseCommand` | Same. Technically the cheapest item in the lane (zero new doors, both helpers already ported); blocked only by §0. |
| Porting `0x1800042a0`/`0x180005dc0` | **Real**: the write path (mode != 3) and the temporary's refcount/lifetime are undecoded. Porting a `Serialize` on a half-read body is how you get a save-file corruption that no oracle catches on the read path. |
| Porting `0x180052640` | **Real, and it is a scoping fact**: closes 1 door, opens 2 (`0x180051790`, `0x180051e00`, neither exported nor ported). It should be taken together with those two, by the audio lane, not alone. |
| Running the five pre-report audits | Declined: they walk the tree, and the tree I am standing in is being written by other lanes (§0.2), so any number I produced would be theirs, not mine. My first contaminated measurement is the evidence that this is a real effect and not a hypothetical. |
