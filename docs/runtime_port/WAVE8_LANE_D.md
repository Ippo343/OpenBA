# Wave 8 — Lane D — `Input::KeyMap` + `Input::Device` + `Input::Configuration`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/input/input.cpp`, `runtime/src/input/rt_input_port.cpp`,
`runtime/types/input/Input.h`, this document.

**Result: 15 of 21 ported, 6 held back with written reasons.**
`RtInputRegister()` (called from `RtInputPortInit()`, kill switch
`BANISHED_RT_INPUT`) returns **15**. `input.cpp` and `rt_input_port.cpp`
compile warning-clean with `g++ -std=c++17 -Wall -Wextra` at `-O0`, `-O2` and
`-O3`; `runtime/types/input/Input.h` compiles standalone and `static_assert`s
every offset it claims. `tools/export_name_audit.py`: **643 distinct
`ResolveExport`/`ResolveExportRaw` names, 0 not in the shipped DLL** (this
lane adds one `ResolveExportRaw` call, already covered). Separately, all
**21** mangled names this lane touches — the 15 ported plus the 6 held back —
were checked against `C:\GOG Games\Banished\Runtime-x64.dll`'s export
directory with `pefile` before this doc was written; **all 21 exist**, so
this lane has zero phantoms (WAVE8.md §4's 3 phantoms are all in lanes C/E).
`tools/abi_retslot_audit.py`: **0 suspicious of 48** (this lane returns no
`String` by value, so no hidden-retslot risk applies to it).

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a
*prediction* of what the integration oracle will report, not a measurement.

---

## 0. Everything Ghidra's decompiled C got wrong for this lane

Per CLAUDE.md and `reference_ghidra_c_output_vs_disassembly.md`, nothing below
is transcribed from `decompile_function` output. Two concrete traps this lane
hit and avoided by reading the disassembly instead:

1. **`KeyMap::GetValue`/`Pressed`'s `_disableKeyboardCount` gate.** The
   decompiled C makes the keyboard-suppression check look like a per-slot
   `continue`. The disassembly (0x1800a7adb/0x1800a7ae0 for `Pressed`,
   equivalently for `GetValue`) shows both branches jump straight to the
   function's single 0/false exit — it is a **whole-function short-circuit**,
   not a "skip this slot, try the next" — see §3.
2. **`RegisterInput`'s node-insert helper's first argument.** The decompiled C
   invents a cast `pKVar5 = (KeyMap *)(ulonglong)bVar1` — a *byte value*
   masquerading as a pointer parameter — which is why this lane did NOT trust
   it and went to the disassembly, where the same register (RCX) turns out to
   hold leftover BST-descent scratch, not `this`, at the call site. That
   ambiguity is the reason `RegisterInput` is held back (§4).

---

## 1. The function table

All addresses and ordinals are from `docs/runtime_port/wave8_lanes.txt`
(generated from the shipped export table before any body was written) and
were independently re-verified by this lane with a second `pefile` pass
(§ above).

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `?ClearAll@KeyMap@Input@@QEAAXXZ` | 0x1800a7d20 | 1706 | yes | shadow | `Input::KeyMap::ClearAll` |
| `?ClearInput@KeyMap@Input@@QEAAXHH@Z` | 0x1800a7d70 | 1709 | yes | shadow | `Input::KeyMap::ClearInput(id,slot)` |
| `?ClearInput@KeyMap@Input@@QEAAXHHH@Z` | 0x1800a7db0 | 1710 | yes | shadow | `Input::KeyMap::ClearInput(id,device,inputId)` |
| `?EnableMouseClipping@Device@Input@@QEAAX_N@Z` | 0x1800e1780 | 2296 | **no** | — | — |
| `?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z` | 0x1800a7970 | 2571 | yes | replay | `Input::KeyMap::GetDeviceForId` |
| `?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z` | 0x1800a7890 | 2700 | yes | replay | `Input::KeyMap::GetIdForDeviceInput` |
| `?GetInputForId@KeyMap@Input@@QEBAHHH@Z` | 0x1800a7930 | 2713 | yes | replay | `Input::KeyMap::GetInputForId` |
| `?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z` | 0x1800e1660 | 3004 | yes | replay | `Input::Device::GetValue` |
| `?GetValue@KeyMap@Input@@QEBAFHH@Z` | 0x1800a7b60 | 3005 | yes | replay | `Input::KeyMap::GetValue` |
| `?Load@Configuration@Input@@QEAAXXZ` | 0x1800e0c80 | 3199 | yes | shadow | `Input::Configuration::Load` |
| `?Load@KeyMap@Input@@QEAAXXZ` | 0x1800a7190 | 3203 | **no** | — | — |
| `?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z` | 0x1800e16c0 | 3431 | yes | replay | `Input::Device::Pressed` |
| `?Pressed@KeyMap@Input@@QEBA_NHH@Z` | 0x1800a7a80 | 3432 | yes | replay | `Input::KeyMap::Pressed` |
| `?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z` | 0x1800a7e70 | 3529 | **no** | — | — |
| `?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z` | 0x1800e1720 | 3545 | yes | replay | `Input::Device::Released` |
| `?ResetState@Device@Input@@QEAAXXZ` | 0x1800e0ef0 | 3579 | yes | shadow | `Input::Device::ResetState` |
| `?Save@Configuration@Input@@QEAAXXZ` | 0x1800e0c00 | 3604 | yes | shadow | `Input::Configuration::Save` |
| `?SetInput@KeyMap@Input@@QEAAXHHHH@Z` | 0x1800a7e20 | 3896 | yes | shadow | `Input::KeyMap::SetInput` |
| `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` | 0x1800e1490 | 4482 | **no** | — | — |
| `?_singleton@Configuration@Input@@0PEAV12@EA` | 0x1801fe768 | 4802 | **no** | — | — |
| `?_singleton@Device@Input@@0PEAV12@EA` | 0x1801fe770 | 4806 | **no** | — | — |

Every ported function is registered **twice**, under its mangled name and its
retail address (`RegisterRuntimeExport` + `RegisterRuntimeAddr`), matching the
Wave 7 lane A precedent — cheap insurance against a live call site that
resolves by address instead of by name (WAVE1 defect 4,
`reference_resolveexport_phantom_names.md`).

---

## 2. The vtable question

Neither `KeyMap`, `Device` nor `Configuration` exports a `??_7` (WAVE8.md §2
table). **Verified, not inferred, from the disassembly of the two functions
this lane could most plausibly expect a vtable dereference in** —
`RegisterInput` (0x1800a7e70, the most complex KeyMap mutator) and
`Device::GetValue` (0x1800e1660, the hottest Device accessor): neither reads
`[this]` as a function-pointer table anywhere in its body; every access is a
direct field read at a fixed offset. No `??0`/`??1` for any of the three
classes was decompiled by this lane (none of the 21 in-scope exports is a
ctor/dtor), so there is no ctor-stamp evidence either way — the verdict rests
entirely on "no method in this lane's 15 dereferences a vtable slot," which is
sufficient for the question this lane needs answered (none of these ports
risk a vtable-ownership split) without claiming the classes are provably
non-polymorphic in general.

---

## 3. What the unattended harness actually exercises

**`BANISHED_AUTOSTART`/`BANISHED_PAUSEMENU_BUTTON` drive the UI through
synthesized `OnCommand`/`OnEvent` calls on dialog vtables — they do NOT go
through `Input::KeyMap` or `Input::Device` at all.** The harness's click
injection (`standalone/autostart.cpp`) calls dialog handlers directly; it
never asks a `KeyMap` binding "is this pressed" and never touches DirectInput.
That means:

* **Exercised by the harness, indirectly, every frame regardless of clicking:**
  nothing in this lane. `Device::Update` is what would normally drive the
  frame's input latch, and it is one of the 6 hold-backs (§4) — so under
  `BANISHED_RT_DIFF=1` this lane's 9 replay-oracle functions
  (`GetDeviceForId`, `GetInputForId`, `GetIdForDeviceInput`, both `GetValue`s,
  both `Pressed`/`Released` pairs) will show **`calls=0`** in an autostart-only
  run unless something else in the tree calls them outside the direct
  keyboard/mouse path.
* **`KeyMap::ClearAll`/`ClearInput`/`SetInput`/`RegisterInput`** are bound
  through the options/keybinding UI (`OptionsDialog`'s input-bind list,
  `engine_funcs.cpp`'s `fn_KeyMap_*` family) — reachable only by a real click
  sequence into the Options dialog, which the harness does not drive today.
* **`Configuration::Load`/`Save`** run once each, at startup and on options
  dialog close respectively — `Load` is very likely to execute once during
  boot regardless of clicking (every prior wave's `Engine::Startup` chain
  reads settings), `Save` only if something actually writes settings.

**This is a real gap this lane is flagging, not hiding**: a broken
`KeyMap`/`Device` port could ship with `calls=0` on every one of this lane's
sites under the existing harness and read as a clean run. WAVE8.md's own
framing predicted this ("a broken Input port may still look like it boots").
The integrator should additionally run a manual/attended session that
actually presses keys and moves the mouse before trusting a `calls=0` result
here as anything other than "the harness didn't ask."

---

## 4. The six hold-backs

### `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` (0x1800e1490)

The single most important function in this lane, and the reason to hold it
back rather than guess. Its body (reversed, not ported):

```
zero a 105-entry State scratch buffer
if (PlatformGetKeyboardState(this, scratch))   // NOT exported, no mangled name
    UpdateState(this, this+0x6C0, scratch, 0x69, &time)   // NOT exported
zero a 13-entry State scratch buffer
if (PlatformGetMouseState(this, scratch))      // NOT exported
    UpdateState(this, this+0x1098, scratch, 0xD, &time)   // NOT exported
PlatformUpdate(this)                            // NOT exported
```

Four internal helpers, **none exported and none named** in either the retail
or profile DLL's string table this lane checked. Porting `Update` would mean:
(a) finding and decompiling all four by address — out of this lane's time
budget; (b) reversing whatever DirectInput8 COM object pointers `Device`
holds past `+0x11D0` (unmapped — see the type header), since
`PlatformGetKeyboardState`/`PlatformGetMouseState` almost certainly read
through them; and (c) accepting that **there is no valid oracle regardless**
(WAVE8.md §5: replaying `Update` drains the DirectInput queue twice, so "call
ours, call theirs, compare" corrupts the very state being compared). A wrong
port here is not a subtle divergence — it is a game that silently stops
responding to the keyboard and mouse, and the harness (§3) would not catch it.
Left on the DLL.

### `?EnableMouseClipping@Device@Input@@QEAAX_N@Z` (0x1800e1780)

One field write (`this[0x11D0] = param`, pinned as `Device::_mouseClipping` in
the type header) followed by a call to the same unexported `PlatformUpdate`
`Update` depends on. Porting the field write alone would silently drop the
Win32 clip-rect side effect the DLL's call actually produces; porting the
whole thing needs the same unresolved internal helper `Update` does. Held
back for the same reason, not a separate one.


**RESOLVED 2026-09-07 during Wave 9 prep — the ambiguity below is settled, and the answer is
that the parameter does not exist.** The hold-back was correct on its own evidence and is now
superseded; the reasoning is left in full because the *method* it warns about still applies.

Lane D asked what `FUN_180023060`'s first argument is. **Reading the callee answers it: RCX is
never read.** The first thing the helper does with RCX is destroy it —
`0x180023097: MOV RCX,RBX`, loading the allocator critical section at `*(0x1801fc718) + 0x640`.
Its only real inputs are `RDX` (a `const Binding*` source) and `R8` (the parent node):

```
180023060  MOV RBP,R8                    ; parent
180023081  MOV RSI,RDX                   ; element source
180023084  MOV RBX,[0x1801fc718]; ADD RBX,0x640
180023097  MOV RCX,RBX                   ; <-- RCX first USED here, as the lock. Never read.
18002309a  CALL [0x18014b390]            ; EnterCriticalSection
1800230a1  MOV EDX,0x38 ; LEA R8D,[RDX-0x30]  ; size 0x38, align 8
1800230aa  MOV RCX,[0x1801fc718]
1800230b1  CALL 0x1800c6e60              ; MemoryAllocator::Allocate
1800230bc  CALL [0x18014b388]            ; LeaveCriticalSection
1800230c2  TEST RDI,RDI ; JZ -> return 0
1800230cb  copy 3 qwords [RSI] -> [RDI]  ; the 0x18-byte Binding
1800230e1  [RDI+0x18] = RBP              ; parent
1800230e7  [RDI+0x20] = 0 ; [RDI+0x28] = 0 ; [RDI+0x30] = 0   ; left, right, balance/height
1800230f2  MOV RAX,RDI                   ; return the node
```

So the signature is `BindingNode* AllocNode(<unused>, const Binding* src, BindingNode* parent)`.

**The call-site evidence agrees, and sharpens one detail lane D got backwards.** The note below
says the register holding the DLL's first argument at `0x1800a7ee6` was last written by the
BST-descent scratch. It was not — *at that site*. `0x1800a7ee6` is reached only when `RBX == 0`
(`0x1800a7e88 JZ`, then `0x1800a7ed9 TEST RBX,RBX / JNZ` diverts every non-empty-tree path away
from it), and the descent loop at `0x1800a7e90` only runs when `RBX != 0`. On the path that
actually reaches `0x1800a7ee6`, **RCX is still `this`, untouched since entry.** The scratch-byte
observation is true of the OTHER two call sites (`0x1800a7f3a`, `0x1800a7f4d`), which are on the
non-empty path — and that is the stronger form of the argument, not the weaker one: **a value
that is `this` at one call site and a zero-extended id byte at the other two cannot be a
parameter the callee reads.** Lane D reached the right verdict from a mis-attributed premise.

**Everything else this lane reversed is confirmed by the allocator's own body**: node size
`0x38`, a `0x18`-byte element copied as three qwords, `parent` at `+0x18`, `left`/`right` at
`+0x20`/`+0x28` and the balance/height dword at `+0x30` — exactly the `BindingNode` and
`Binding` layouts already `static_assert`-pinned in `runtime/types/input/Input.h`.

`RegisterInput` and `Load` are therefore **unblocked for Wave 9 lane A**. What remains is not an
unknown, it is an ORACLE: both mutate the shared AVL tree, so replay would insert twice, and the
port needs a shadow oracle (post-state: the binding is present with the expected fields and the
count moved by exactly one) rather than call-ours-call-theirs.

---
### `?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z` (0x1800a7e70)

The search half (find-by-id, return early if already registered) is identical
to the other 9 KeyMap point functions and posed no problem. The insert half
calls two unexported internal helpers — a node allocator
(`FUN_180023060`) and an AVL rebalance/link step (`FUN_180023170`) — and
**this lane could not pin the allocator's true parameter list with
confidence**. At the empty-tree call site (0x1800a7ee6), the register holding
the DLL's own "first argument" (RCX) was last written by the BST-descent
scratch (`MOVZX ECX, byte [RAX]`, a zero-extended *node id byte*, 0-255) —
not reloaded from `this` anywhere between the descent and the call. Ghidra's
decompiled C papers over this with an invented cast
(`pKVar5 = (KeyMap*)(ulonglong)bVar1`, §0) that is not evidence of anything;
the disassembly is honest that the value in that register is NOT obviously a
`this` pointer. Compare `ClearAll` (§ above), whose two analogous helper
calls both cleanly pass `&this->_root` with no ambiguity — that is why
`ClearAll` is ported and `RegisterInput` is not. Calling an internal allocator
helper with a guessed-wrong first argument risks corrupting the live AVL tree
every other `KeyMap` function in this lane walks; an honest hold-back beats
that (the WAVE2 allocator verdict: an unverified port is not a port, and a
wrong one that corrupts shared state is worse than no port at all).

### `?Load@KeyMap@Input@@QEAAXXZ` (0x1800a7190)

Parses each binding's registry string ("`<device>:<key>`"-shaped) via an
unexported helper (`FUN_18007f034`), does its own `System::String::Format`
allocation pair per binding, and performs a full `Collection::Set<Pair<String,
String>>` lookup into `IO::Persistence`'s in-memory settings map (BST descent
with inlined String comparison, then reference-counted release of two
transient strings). This is the densest of the 21 in every dimension this
lane budgeted for (unexported codec + a second, differently-typed `Set`
instantiation + allocator string traffic) and was not attempted. Left on the
DLL — but see §5, `Configuration::Load` still calls it, unmodified, by
address.

### `?_singleton@Configuration@Input@@0PEAV12@EA` (0x1801fe768) and `?_singleton@Device@Input@@0PEAV12@EA` (0x1801fe770)

Both are exported **DATA** symbols, not functions — there is nothing for
`RegisterRuntimeExport`/`RegisterRuntimeAddr` to redirect, matching every
prior wave's identical note about `_singleton` exports (Wave 4's
`FileSystem::_singleton`, Wave 6/7's equivalents). This lane's own code reads
`Device::_singleton` and `Persistence::_singleton` directly via
`ResolveExportRaw`, exactly like `engine_funcs.cpp`'s existing
`g_pInputDeviceSingleton`/`g_pInputConfigSingleton`.

---

## 5. `Configuration::Load`/`Save` — ported as thin forwards, not reimplementations

Both retail bodies (disassembled, not decompiled) are: call `KeyMap::Load`/
`Save` by a plain near-CALL, then three `IO::Persistence::Get`/`Set(bool&)`
calls for `"Input"`/`{"MouseClipping","Legacy","MouseDisplay"}`, reading/
writing `Configuration+0x08/+0x09/+0x0A` directly as the out-parameter. This
lane's port:

* Calls retail `KeyMap::Load`/`Save` **by address** (`ResolveAddr(Module::
  Runtime, 0x1800a7190)` / `0x1800a7520`) — the DLL's own, unmodified body,
  exactly as retail's own `Configuration::Load`/`Save` do internally. This is
  the same shape Wave 6's `Video::Context` lane used for methods it called
  but did not own: porting the *outer* function does not require
  reimplementing everything it calls.
* Calls this lane's own dependency on the **already-ported, already-diffed**
  Wave 4 `IO::Persistence::GetBool`/`SetBool` (`runtime/src/io/persistence.cpp`)
  directly as C++ functions, rather than re-resolving the DLL's raw export —
  cheaper and it reuses a port this project has already verified rather than
  re-deriving it.

`KeyMap::Save`'s retail address (0x1800a7520) was read directly out of
`Configuration::Save`'s disassembly (`CALL 0x1800a7520`) rather than assumed;
it is not independently in `wave8_lanes.txt` because nothing in our tree calls
it directly — only `Configuration::Save` does, and that call site is the one
this lane owns.

**Oracle**: `RT_DIFF_SHADOW(true, ...)`. Re-calling retail's own
`Configuration::Load`/`Save` to compare would re-run `KeyMap::Load`/`Save` a
second time, each of which mutates a live, ref-counted `String`-keyed map —
the same class of hazard `IO::Persistence::Set` already documents for calling
the DLL twice on shared state. The shadow is honest about proving less than a
full oracle: it confirms the 3 fields were written at the offsets the type
header claims, not that the values are correct (that guarantee comes
transitively from the already-diffed `Persistence::GetBool`/`SetBool` this
lane calls into).

---

## 6. Layout — `runtime/types/input/Input.h`

Full detail is in the header's own comments; summary:

* **`Input::KeyMap`**: `_disableKeyboardCount` (+0x00, name NOT proven — no
  accessor names it, only its observed effect on `GetValue`/`Pressed`, see §3
  of the header), `_root`/`_count` of an embedded `Collection::Set<Binding>`
  at +0x08/+0x10 (Set's own layout: `Collection.h`, "E24 — Input::KeyMap's
  key-set element", confirmed here down to the byte).
* **`Binding`** (the Set's 24-byte element T): `id`(+0x00, unsigned BST key),
  `exclusive`(+0x01), `description`(+0x08, `const char*`), `device[2]`(+0x10),
  `inputId[2]`(+0x12) — every byte pinned from `RegisterInput`'s disassembly
  (0x1800a7e70), the one function in this lane whose stack-blob construction
  spells out the whole element at once.
* **`Input::Device`**: three flat `State[N]` arrays (joystick×4, keyboard,
  mouse) with bases and counts cross-confirmed by BOTH the 3 const accessors'
  dispatch AND `ResetState`'s 3 independent zeroing loops agreeing on every
  base and count — the strongest kind of two-source pin this lane has.
  `_mouseClipping` at +0x11D0 falls out algebraically as "immediately after
  the mouse array," then is independently confirmed by `EnableMouseClipping`.
* **`Input::Configuration`**: `keyMap`(+0x00, pointer), 3 bools at
  +0x08/+0x09/+0x0A — pinned identically by both `Load` and `Save`'s
  disassembly.

Nothing past `Device+0x11D0` is evidenced or claimed.

---

## 7. Comments this lane did NOT find anything to correct

PLAN §6 step 8 requires checking for stale comments this lane's work
disproves. This lane read `engine_funcs.h`'s existing Input comments
(`g_pKeyMapSingleton`/`g_pKeyMapObjectExref`, the various `fn_KeyMap_*`/
`fn_Input_Device_*` typedefs) and found them consistent with what this lane's
disassembly independently derived — in particular the exe-side note that
`g_pKeyMapSingleton` is a *different* "container" object, not `Input::KeyMap`
itself, is correct and this lane's `KeyMap` struct does not contradict it.
No correction filed.

---

## 8. What the integrator must wire

* Add `void RtInputPortInit();` to `runtime/funcs/rt_port.h`'s wave-init
  block (alongside `RtUiPortInit()`), and call it from
  `standalone/resolve_standalone.cpp`'s `RuntimePortInit()`.
* Add `runtime/src/input/input.cpp` and `runtime/src/input/rt_input_port.cpp`
  to the build (mirrors how `runtime/src/video/*.cpp` are already wired).
* This lane's 15 ports do not touch any file another lane owns.
