# OPEN DEFECT — `ModDialog_ToggleMod_Impl` null deref on the in-game Mods menu

Found 2026-09-06 during Wave 5, while trying to reach `Save::SavedState::SavedState`.
**Pre-existing; NOT a Wave 5 defect** — see the A/B below. Recorded, not fixed: it is outside the
wave's scope, and CLAUDE.md's rule is to find the upstream null rather than guard the deref.

## Repro

```
BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_BUTTON=7 \
BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_CLICKS=1001,5001,101 \
BANISHED_MODAL_ANSWER=1
```

(`BUTTON=7` is Mods; `MODAL_ANSWER=1` is correct on THIS path — it is the mods dialog, not the
save/load `modsMissing` prompt, where 1 cancels the load. See WAVE5.md.)

## The crash

```
Exception: 0xC0000005 (ACCESS_VIOLATION)   Fault: READ of 0x00000000000000d0
RAX=00000000000000D0
#0 ModDialog_ToggleMod_Impl      code/Game/Dialog/moddialog.cpp:561
#1 ModDialog_Slot8               code/Game/Dialog/moddialog.cpp:801
#2 ModDialog_VOnElementEvent     code/Game/Dialog/mod_dialog_vtables.cpp:189
#3 PauseMenuHarness_Tick         standalone/pausemenu_harness.cpp:168
#4 GameUpdateLoop_Update         code/Game/Game/gameupdateloop.cpp:1071
```

`moddialog.cpp:559-561`:

```cpp
void* mod = fn_Modification_GetMod(modSingleton, index);
bool currentEnabled = *(uint8_t*)((uintptr_t)mod + 0x50) != 0;   // <-- faults
```

`fn_Modification_GetMod` hands back something that cannot be dereferenced for the index the click
carries. The click token used was `1001`; whether that index is valid depends on the installed mod
list, so the first question for whoever picks this up is whether retail bounds-checks the index
before `GetMod` or whether the exe's own caller cannot produce an out-of-range index at all — i.e.
whether the real defect is the missing check or the index that reached it.

## Why it is NOT a Wave 5 defect — the A/B

Re-run byte-identical except `BANISHED_RT_PORT=0`, which disables **every** runtime port from
Waves 1-5:

| Run | Fault | RIP offset | RAX |
|---|---|---|---|
| ports on  | ACCESS_VIOLATION | `reimpl-x64.exe + 0x1437F5` | `0xD0` |
| ports OFF | ACCESS_VIOLATION | `reimpl-x64.exe + 0x1437F5` | `0xD0` |

Identical fault, identical offset, identical register. The runtime port is not involved.

## Consequence for Wave 5

It is the reason `Save::SavedState::SavedState` is registered but never executed: its only two
callers (`moddialog.cpp:722`, `mod_dialog_vtables.cpp:438`) are on the Mods apply/reload path, and
that path dies before reaching them. Closing this defect would also close that gap.
