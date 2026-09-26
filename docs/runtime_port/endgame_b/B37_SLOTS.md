# B37_SLOTS.md — raw appendix for B.3 step 7 (`Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`, `Picking`, `Application`)

Produced 2026-09-26 by block 0 from the RETAIL `Runtime-x64.dll` (asserted before each read: 15290 functions,
image base 0x180000000, `get_current_program_info`) and from `Application-x64.exe` (11473 functions, exe mode of
the same script) by `b37_session/ghidra/Step7Appendix.java`, run through GhidraMCP. The complete console output
is `b37_session/step7_appendix_raw.txt`; this file is the same data, condensed where marked, with the corrections
the raw run needs stated inline. Copy from here; do not retype. Rules and verdicts are in `B37_RULES.md`.

How each fact was read (same instruments as `B36_SLOTS.md`, plus four):
- slot targets: qwords over `.rdata`, terminating on ANY `??_7`/`??_R` symbol, a zero qword or a non-code target;
- `sizeof`: the `MOV EDX,imm` in the class's own slot-0 vector deleting destructor, read as DISASSEMBLY. **Three
  rows of the raw run print a false size** because slot 0 of their table is not a deleting dtor: `Audio::Stream`
  (slot 0 is `Run`; the `0x40` is an immediate inside `Run`), `Pathfinding::IPathInfo` and `System::Thread` (slot 0
  is `_purecall`; `0x1` is `_purecall`'s `MOV EDX,0x1`). Their sizes are in §1a from other instruments;
- parent: the string literal `?_ParentTypeName@X@NS@@CAPEBDXZ` returns, read byte-wise. Only EIGHT classes in the
  set carry one (§3); every other polymorphic class here is its own root (`ROOT/UNKNOWN` in the raw file);
- `inherited-from P` marks a slot byte-identical to the parent's at the same index; `same-as P` marks a slot equal
  to one of the five Object parent tables or `System::Thread` at that index when the class has no literal. **A
  `same-as` on a `_purecall` slot means nothing** (every pure slot is 0x18007ec28); only the Loader/Stream slot 1
  (`?OnCompleted@Thread@System@@`) `same-as` is an inheritance fact;
- `_purecall` is 0x18007ec28;
- NEW: each export's signature is Ghidra's MicrosoftDemangler output (§6); every `??0`/`??1` export of every class
  in the set, polymorphic or not, has its first ~40 instructions in the raw file (§7 here: the first-store table);
  every export taking a class BY VALUE has its prologue read (§8); the exe tables are read with import JMP stubs
  (`FF 25 disp32`, undefined in Ghidra) decoded to the IAT slot's import name (§E1–§E3).

## §1 — the 16 exported vtables in the set, plus `System::Thread`

```
### Achievement::Achievement  vtable=0x180154740  slots=9  sizeof=0x58  parent=Object::External
  [ 0] +0x00 0x180001680  (unexported) FUN_180001680
  [ 1] +0x08 0x180001040  ?TypeName@Achievement@1@UEBAPEBDXZ
  [ 2] +0x10 0x180001020  ?Type@Achievement@1@UEBAIXZ
  [ 3] +0x18 0x18009e170  ?GetFileVersion@Achievement@1@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x18009e070  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180001270  ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180001370  ?Destruct@Achievement@1@UEAAXXZ
  [ 8] +0x40 0x1800012f0  ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ
### Achievement::AchievementInstance  vtable=0x180154700  slots=4  sizeof=0x28  parent=Object::Internal
  [ 0] +0x00 0x180001a90  (unexported) FUN_180001a90
  [ 1] +0x08 0x1800017c0  ?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ
  [ 2] +0x10 0x1800017a0  ?Type@AchievementInstance@Achievement@@UEBAIXZ
  [ 3] +0x18 0x18009e4a0  ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
### Achievement::Goal  vtable=0x1801546b0  slots=10  sizeof=0x30  parent=Object::External
  [ 0] +0x00 0x180001d70  (unexported) FUN_180001d70
  [ 1] +0x08 0x180001b60  ?TypeName@Goal@Achievement@@UEBAPEBDXZ
  [ 2] +0x10 0x180001b40  ?Type@Goal@Achievement@@UEBAIXZ
  [ 3] +0x18 0x18009ee10  ?GetFileVersion@Goal@Achievement@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x18009ee20  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External
  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External
  [ 8] +0x40 0x18007ec28  _purecall
  [ 9] +0x48 0x18009eeb0  ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
### Achievement::GoalInstance  vtable=0x180154678  slots=7  sizeof=0x20  parent=Object::Internal
  [ 0] +0x00 0x1800020c0  (unexported) FUN_1800020c0
  [ 1] +0x08 0x180001e60  ?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ
  [ 2] +0x10 0x180001e40  ?Type@GoalInstance@Achievement@@UEBAIXZ
  [ 3] +0x18 0x18009ef70  ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
  [ 4] +0x20 0x18009f000  ?Startup@GoalInstance@Achievement@@UEAAXXZ
  [ 5] +0x28 0x18009f010  ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
  [ 6] +0x30 0x18009f060  ?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ
### Audio::SoundBuffer  vtable=0x1801549e0  slots=9  sizeof=0x80  parent=Object::CompiledExternal
  [ 0] +0x00 0x18000bef0  (unexported) FUN_18000bef0
  [ 1] +0x08 0x18000b8b0  ?TypeName@SoundBuffer@Audio@@UEBAPEBDXZ
  [ 2] +0x10 0x18000b890  ?Type@SoundBuffer@Audio@@UEBAIXZ
  [ 3] +0x18 0x1800e0470  ?GetFileVersion@SoundBuffer@Audio@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::CompiledExternal
  [ 5] +0x28 0x1800e0480  ?Serialize@SoundBuffer@Audio@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18000bb30  ?Reconstruct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18000bc70  ?Destruct@SoundBuffer@Audio@@UEAAXXZ
  [ 8] +0x40 0x18000bbd0  ?Construct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
### Audio::SoundEffect  vtable=0x180157ce0  slots=9  sizeof=0x40  parent=Object::External
  [ 0] +0x00 0x180050820  (unexported) FUN_180050820
  [ 1] +0x08 0x180050340  ?TypeName@SoundEffect@Audio@@UEBAPEBDXZ
  [ 2] +0x10 0x180050320  ?Type@SoundEffect@Audio@@UEBAIXZ
  [ 3] +0x18 0x1800e0990  ?GetFileVersion@SoundEffect@Audio@@UEBAHXZ
  [ 4] +0x20 0x1800e09a0  ?Compile@SoundEffect@Audio@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1800e0aa0  ?Serialize@SoundEffect@Audio@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180050560  ?Reconstruct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180050640  ?Destruct@SoundEffect@Audio@@UEAAXXZ
  [ 8] +0x40 0x1800505d0  ?Construct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ
### Audio::Stream  vtable=0x180154950  slots=2  sizeof=0x40  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x1800a0f60  ?Run@Stream@Audio@@UEAAXXZ
  [ 1] +0x08 0x18000c590  ?OnCompleted@Thread@System@@UEAAXXZ   same-as System::Thread
### Engine::DialogLoaderLoopT  vtable=0x1801578d0  slots=13  sizeof=0x188  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x18004d670  (unexported) FUN_18004d670
  [ 1] +0x08 0x1800db000  ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 2] +0x10 0x1800dafc0  ?Shutdown@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 3] +0x18 0x1800daf20  ?Update@DialogLoaderLoopT@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x1800dae20  ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z
  [ 5] +0x28 0x180031740  ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x1800dad50  ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 7] +0x38 0x180031750  ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x1800dc360  ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x180031770  ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x180031780  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x180031790  ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  [12] +0x60 0x18007ec28  _purecall
### Engine::DialogLoopT  vtable=0x1801579a0  slots=13  sizeof=0x168  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x18004d8b0  (unexported) FUN_18004d8b0
  [ 1] +0x08 0x1800db2f0  ?Startup@DialogLoopT@Engine@@UEAAXXZ
  [ 2] +0x10 0x1800db2c0  ?Shutdown@DialogLoopT@Engine@@UEAAXXZ
  [ 3] +0x18 0x1800db270  ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x1800db210  ?Draw@DialogLoopT@Engine@@UEAAXM@Z
  [ 5] +0x28 0x180031740  ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x1800db140  ?Load@DialogLoopT@Engine@@UEAAXXZ
  [ 7] +0x38 0x180031750  ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x180031760  ?OnVideoError@UpdateLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x180031770  ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x180031780  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x180031790  ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  [12] +0x60 0x18007ec28  _purecall
### Engine::Loader  vtable=0x180157a08  slots=2  sizeof=(no MOV EDX,imm in slot 0)  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x1800dbfc0  ?Run@Loader@Engine@@UEAAXXZ
  [ 1] +0x08 0x18000c590  ?OnCompleted@Thread@System@@UEAAXXZ   same-as System::Thread
### Engine::LoaderLoop  vtable=0x180157940  slots=12  sizeof=0x48  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x18004d4f0  (unexported) FUN_18004d4f0
  [ 1] +0x08 0x1800dc260  ?Startup@LoaderLoop@Engine@@UEAAXXZ
  [ 2] +0x10 0x1800dc280  ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
  [ 3] +0x18 0x1800dc2a0  ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x1800dc310  ?Draw@LoaderLoop@Engine@@UEAAXM@Z
  [ 5] +0x28 0x180031740  ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x1800dc350  ?Load@LoaderLoop@Engine@@UEAAXXZ
  [ 7] +0x38 0x180031750  ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x1800dc360  ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x180031770  ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x180031780  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x180031790  ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
### Engine::UpdateLoop  vtable=0x180155ba0  slots=12  sizeof=0x28  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x180031900  (unexported) FUN_180031900
  [ 1] +0x08 0x18007ec28  _purecall   same-as Object::Base
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall   same-as Object::External
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x180031740  ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x18007ec28  _purecall   same-as Object::External
  [ 7] +0x38 0x180031750  ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x180031760  ?OnVideoError@UpdateLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x180031770  ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x180031780  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x180031790  ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
### Pathfinding::IPathInfo  vtable=0x180158f30  slots=2  sizeof=0x1  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x18007ec28  _purecall   same-as System::Thread
  [ 1] +0x08 0x18007ec28  _purecall   same-as Object::Base
### Pathfinding::PointList  vtable=0x180158f50  slots=9  sizeof=0x50  parent=Object::External
  [ 0] +0x00 0x1800713b0  (unexported) FUN_1800713b0
  [ 1] +0x08 0x180070e40  ?TypeName@PointList@Pathfinding@@UEBAPEBDXZ
  [ 2] +0x10 0x180070e20  ?Type@PointList@Pathfinding@@UEBAIXZ
  [ 3] +0x18 0x18010f880  ?GetFileVersion@PointList@Pathfinding@@UEBAHXZ
  [ 4] +0x20 0x18010f6d0  ?Compile@PointList@Pathfinding@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x18010f6e0  ?Serialize@PointList@Pathfinding@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180071060  ?Reconstruct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180071150  ?Destruct@PointList@Pathfinding@@UEAAXXZ
  [ 8] +0x40 0x1800710e0  ?Construct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
### Picking::PickingMesh  vtable=0x180158fb0  slots=9  sizeof=0x48  parent=Object::External
  [ 0] +0x00 0x180072050  (unexported) FUN_180072050
  [ 1] +0x08 0x180071ab0  ?TypeName@PickingMesh@Picking@@UEBAPEBDXZ
  [ 2] +0x10 0x180071a90  ?Type@PickingMesh@Picking@@UEBAIXZ
  [ 3] +0x18 0x180122940  ?GetFileVersion@PickingMesh@Picking@@UEBAHXZ
  [ 4] +0x20 0x180122950  ?Compile@PickingMesh@Picking@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x180122960  ?Serialize@PickingMesh@Picking@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180071cd0  ?Reconstruct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180071db0  ?Destruct@PickingMesh@Picking@@UEAAXXZ
  [ 8] +0x40 0x180071d40  ?Construct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ
### System::Thread  vtable=0x180155f20  slots=2  sizeof=0x1  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x18007ec28  _purecall
  [ 1] +0x08 0x18000c590  ?OnCompleted@Thread@System@@UEAAXXZ
```

### §1a — sizes the raw §1 prints wrongly, and the unexported table

| class | raw §1 says | retail size | instrument |
|---|---|---|---|
| `Audio::Stream` | 0x40 | **0x88** | `??0Device@Audio@@` (0x1800de3a0) builds the Stream at `Device+0xa00` (`LEA RCX,[RBX+0xa00]; CALL ??0Stream`) and writes the next member at `Device+0xa88`; `??0Stream` (0x1800a0e30) stores up to the byte at +0x80 |
| `Engine::Loader` | (none) | **0x38** | `??0Loader` (0x1800dc0f0) last store `MOV qword [RBX+0x30],0`; the DLL static initializer of `_loader` (0x180148fe0) inlines the same ctor and writes 0x1801fe7e8 = `_loader`+0x30 last |
| `Pathfinding::IPathInfo` | 0x1 | **8** (vptr only) | `??0IPathInfo` (0x18006c970) is `LEA RAX,[vtable]; MOV [RCX],RAX; RET` — no field store |
| `System::Thread` | 0x1 | 0x30 | `runtime/types/system/Thread.h:291` (B34B §2, unchanged) |
| unexported `0x180157c60` (7 slots, `IXAudio2VoiceCallback`) | not in the export list | record 0x20 | `??0Device@Audio@@` stamps it into 64 records at `Device+0xa98` stride 0x20 (`LEA RCX,[0x180157c60]; MOV [RAX],RCX; LEA RAX,[RAX+0x20]`). Slots (bytes read with `read_memory`, Ghidra has no functions there): 0 0x180050200 `RET 0`, 1 0x1800501f0 `RET 0`, **2 0x1800501c0 `MOV RAX,[RCX+0x10]; OR dword [RAX],2; RET` (OnStreamEnd)**, 3 0x180050210 `RET 0`, **4 0x1800501d0 `MOV RCX,[RCX+0x18]; TEST; JZ; JMP [SetEvent]` (OnBufferEnd)**, 5 0x180050220 `RET 0`, 6 0x180050230 `RET 0`. Minted as the unnamed row `rt_data_mint_tables.inc:3888`; bodies in `runtime/src/audio/audio_voice_callback_win.cpp` |

## §2 — the five Object parent vtables

```
### Object::Base  vtable=0x180154788  slots=3  sizeof=0x8  parent=ROOT/UNKNOWN (no _ParentTypeName)
  [ 0] +0x00 0x1800065b0  (unexported) FUN_1800065b0
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x1800c0380  ?Type@Base@Object@@UEBAIXZ
### Object::External  vtable=0x1801547a0  slots=8  sizeof=0x20  parent=Object::Base
  [ 0] +0x00 0x180006640  (unexported) FUN_180006640
  [ 1] +0x08 0x1800066c0  ?TypeName@External@Object@@UEBAPEBDXZ
  [ 2] +0x10 0x1800066d0  ?Type@External@Object@@UEBAIXZ
  [ 3] +0x18 0x18007ec28  _purecall   same-as Object::CompiledExternal
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   same-as Object::CompiledExternal
  [ 5] +0x28 0x18007ec28  _purecall   same-as Object::CompiledExternal
  [ 6] +0x30 0x18007ec28  _purecall   same-as Object::CompiledExternal
  [ 7] +0x38 0x18007ec28  _purecall   same-as Object::CompiledExternal
### Object::CompiledExternal  vtable=0x180154a30  slots=8  sizeof=0x20  parent=Object::External
  [ 0] +0x00 0x18000b7e0  (unexported) FUN_18000b7e0
  [ 1] +0x08 0x18000b6b0  ?TypeName@CompiledExternal@Object@@UEBAPEBDXZ
  [ 2] +0x10 0x18000b690  ?Type@CompiledExternal@Object@@UEBAIXZ
  [ 3] +0x18 0x18007ec28  _purecall   inherited-from Object::External
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x18007ec28  _purecall   inherited-from Object::External
  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External
  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External
### Object::Internal  vtable=0x180154720  slots=4  sizeof=0x10  parent=Object::Base
  [ 0] +0x00 0x180006440  (unexported) FUN_180006440
  [ 1] +0x08 0x1800064c0  ?TypeName@Internal@Object@@UEBAPEBDXZ
  [ 2] +0x10 0x1800064d0  ?Type@Internal@Object@@UEBAIXZ
  [ 3] +0x18 0x1800bfda0  ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z
### Object::ExternalList  vtable=0x180154800  slots=9  sizeof=0x30  parent=Object::External
  [ 0] +0x00 0x180007800  (unexported) FUN_180007800
  [ 1] +0x08 0x180007370  ?TypeName@ExternalList@Object@@UEBAPEBDXZ
  [ 2] +0x10 0x180007350  ?Type@ExternalList@Object@@UEBAIXZ
  [ 3] +0x18 0x1800bfa10  ?GetFileVersion@ExternalList@Object@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x1800bfa20  ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180007570  ?Reconstruct@ExternalList@Object@@UEAAPEAVExternal@2@XZ
  [ 7] +0x38 0x180007640  ?Destruct@ExternalList@Object@@UEAAXXZ
  [ 8] +0x40 0x1800075e0  ?Construct@ExternalList@Object@@UEAAPEAVExternal@2@XZ
```

## §3 — `_TypeName` / `_ParentTypeName` literals (all 16 in the set)

```
count=16
0x180001000  ?_TypeName@Achievement@1@CAPEBDXZ                                -> "Achievement"
0x180001010  ?_ParentTypeName@Achievement@1@CAPEBDXZ                          -> "Object::External"
0x180001780  ?_TypeName@AchievementInstance@Achievement@@CAPEBDXZ             -> "AchievementInstance"
0x180001790  ?_ParentTypeName@AchievementInstance@Achievement@@CAPEBDXZ       -> "Object::Internal"
0x180001b20  ?_TypeName@Goal@Achievement@@CAPEBDXZ                            -> "Goal"
0x180001b30  ?_ParentTypeName@Goal@Achievement@@CAPEBDXZ                      -> "Object::External"
0x180001e20  ?_TypeName@GoalInstance@Achievement@@CAPEBDXZ                    -> "GoalInstance"
0x180001e30  ?_ParentTypeName@GoalInstance@Achievement@@CAPEBDXZ              -> "Object::Internal"
0x18000b870  ?_TypeName@SoundBuffer@Audio@@CAPEBDXZ                           -> "SoundBuffer"
0x18000b880  ?_ParentTypeName@SoundBuffer@Audio@@CAPEBDXZ                     -> "Object::CompiledExternal"
0x180050300  ?_TypeName@SoundEffect@Audio@@CAPEBDXZ                           -> "SoundEffect"
0x180050310  ?_ParentTypeName@SoundEffect@Audio@@CAPEBDXZ                     -> "Object::External"
0x180070e00  ?_TypeName@PointList@Pathfinding@@CAPEBDXZ                       -> "PointList"
0x180070e10  ?_ParentTypeName@PointList@Pathfinding@@CAPEBDXZ                 -> "Object::External"
0x180071a70  ?_TypeName@PickingMesh@Picking@@CAPEBDXZ                         -> "PickingMesh"
0x180071a80  ?_ParentTypeName@PickingMesh@Picking@@CAPEBDXZ                   -> "Object::External"
```

No `Engine`, `Save`, `Input`, `Picking::Scene`, `Pathfinding::Scene`/`IPathInfo`, `Audio::{Stream,Scene,Device,Configuration}`
or `Application` class has a type-name literal or a `?_typeInfo@` record: they are not `Object::Base` classes.

## §4 — hidden-retslot exports (`?AV`/`?AU` return), and the `?AW4` enums

Correction to the raw column: `?AABB@PickingMesh@Picking@@QEBA?AVBox@Volume@@H@Z` returns `Volume::Box` by value (the
raw parser matched `?AABB`). `1::VAchievement` = `Achievement::Achievement` (back-reference).

**The list below is INCOMPLETE by six**: the script anchors on the first `@@`, and the six retslot exports of
`Achievement::Achievement` spell their class `@Achievement@1@` (no `@@` before the return type). They are, from §6:
`?GetStringTable@Achievement@1@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ` 0x180001380,
`?GetSpriteSheet@Achievement@1@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ` 0x180001390,
`?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ` 0x1800013a0,
`?GetTitle@Achievement@1@QEBA?AVString@System@@XZ` 0x1800013e0,
`?GetDescription@Achievement@1@QEBA?AVString@System@@XZ` 0x180001420,
`?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@XZ` 0x18009df80.
All non-virtual (`QEBA`). Total: 31 retslot exports, 1 virtual.

```
retslot count=25
0x180001710  non-virtual ?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z                    Achievement::VGoalInstance::?$InternalPtr  [class by value]
0x180001950  non-virtual ?GetAchievement@AchievementInstance@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@XZ               1::VAchievement::?$ExternalPtr  [class by value]
0x180001b90  non-virtual ?GetStringTable@Goal@Achievement@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ                            UI::VStringTable::?$ExternalPtr  [class by value]
0x180001ba0  non-virtual ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ                                                            System::String  [class by value]
0x180001fe0  non-virtual ?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ                         Achievement::VGoal::?$ExternalPtr  [class by value]
0x1800314f0  non-virtual ?GetDeltaTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ                                                          System::Time  [class by value]
0x180031500  non-virtual ?GetFrameTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ                                                          System::Time  [class by value]
0x180031510  non-virtual ?GetFrameStartTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ                                                     System::Time  [class by value]
0x180031520  non-virtual ?GetFrameUpdateTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ                                                    System::Time  [class by value]
0x1800506a0  non-virtual ?GetMaxLength@SoundEffect@Audio@@QEBA?AVTime@System@@XZ                                                        System::Time  [class by value]
0x1800711b0  non-virtual ?GetMeshName@PointList@Pathfinding@@QEBA?AVString@System@@XZ                                                   System::String  [class by value]
0x180071dd0  non-virtual ?AABB@PickingMesh@Picking@@QEBA?AVBox@Volume@@H@Z                                                              Picking::PickingMesh::BB  [enum by value]
0x180072200  non-virtual ?GetMesh@Scene@Picking@@QEBA?AV?$ExternalPtr@VPickingMesh@Picking@@@Object@@H@Z                                Picking::VPickingMesh::?$ExternalPtr  [class by value]
0x180072220  non-virtual ?GetAABB@Scene@Picking@@QEBA?AVBox@Volume@@H@Z                                                                 Volume::Box  [class by value]
0x180072250  non-virtual ?GetLocalAABB@Scene@Picking@@QEBA?AVBox@Volume@@H@Z                                                            Volume::Box  [class by value]
0x180072280  non-virtual ?GetTransform@Scene@Picking@@QEBA?AVTransform@Math@@HH@Z                                                       Math::Transform  [class by value]
0x18009e8a0  non-virtual ?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z               1::VAchievement::?$ExternalPtr  [class by value]
0x18009e8f0  non-virtual ?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z Achievement::VAchievementInstance::?$InternalPtr  [class by value]
0x18009eeb0  VIRTUAL ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ                                                      Object::Type  [class by value]
0x1800c0c60  non-virtual ?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ                                                IO::Persistence::SaveBlock  [struct by value]
0x1800c0d50  non-virtual ?GetSaveBlock@SavedState@Save@@QEBA?AUSaveBlock@Persistence@IO@@XZ                                             IO::Persistence::SaveBlock  [struct by value]
0x1800c1250  non-virtual ?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ                                                      System::String  [class by value]
0x1800c1440  non-virtual ?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ                                                       System::String  [class by value]
0x1800e08c0  non-virtual ?GetSoundBuffer@SoundEffect@Audio@@QEBA?AV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@HAEAVRandom@System@@AEAM11@Z Audio::VSoundBuffer::?$ExternalPtr  [class by value]
0x1800e0e20  non-virtual ?PressedTime@Device@Input@@QEBA?AVTime@System@@W4InputDevice@12@HH@Z                                           System::Time  [class by value]
-- NOT retslot: enum by value, returned in EAX. count=5
0x18009f060  VIRTUAL ?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ                                          2::Goal::GoalState  [enum by value]
0x1800c3c80  non-virtual ?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ                                                           12::Operation  [enum by value]
0x1800e09a0  VIRTUAL ?Compile@SoundEffect@Audio@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                     Object::External::CompileResult  [enum by value]
0x18010f6d0  VIRTUAL ?Compile@PointList@Pathfinding@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                 Object::External::CompileResult  [enum by value]
0x180122950  VIRTUAL ?Compile@PickingMesh@Picking@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                   Object::External::CompileResult  [enum by value]
```

Prologues read (all MSVC `this` in RCX, retslot in RDX): `?GetInstanceType@Goal@Achievement@@` 0x18009eeb0
`MOV EAX,[0x1801f9940] (GoalInstance::_typeInfo); MOV [RDX],EAX; MOV RAX,RDX; RET`; `?GetDeltaTime@MainLoop@Engine@@`
0x1800314f0 `MOV RAX,[RCX+0x48]; MOV [RDX],RAX; MOV RAX,RDX; RET`; `?GetGoal@GoalList@Achievement@@` 0x180001710
`MOV RAX,[RCX]; MOVSXD RCX,R8D; MOV RCX,[RAX+RCX*8]; MOV RAX,RDX; MOV [RDX],RCX; RET` (index in R8).

## §5 — who references each vtable (stampers)

```
### Achievement::Achievement  vtable=0x180154740  refs=9
    <no fn> 1801b1fdc
    <no fn> Entry Point
    ??0Achievement@0@QEAA@AEBV00@@Z @0x180001470
    ??0Achievement@0@QEAA@XZ @0x18009e240
    ??1Achievement@0@UEAA@XZ @0x18009e180
    ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ @0x1800012f0
    ?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z @0x1800011f0
    ?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ @0x180001130
    ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ @0x180001270
### Achievement::AchievementInstance  vtable=0x180154700  refs=9
    (unexported) FUN_180001a30 @0x180001a30
    (unexported) FUN_180001a90 @0x180001a90
    <no fn> 1801b1fe0
    <no fn> Entry Point
    ??0AchievementInstance@Achievement@@QEAA@AEBV01@@Z @0x180001970
    ??0AchievementInstance@Achievement@@QEAA@XZ @0x18009e3b0
    ??1AchievementInstance@Achievement@@UEAA@XZ @0x18009e400
    ?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@XZ @0x18009df80
    ?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ @0x1800018b0
### Achievement::Goal  vtable=0x1801546b0  refs=7
    (unexported) FUN_180001cf0 @0x180001cf0
    (unexported) FUN_180001d70 @0x180001d70
    <no fn> 1801b20c4
    <no fn> Entry Point
    ??0Goal@Achievement@@QEAA@AEBV01@@Z @0x180001be0
    ??0Goal@Achievement@@QEAA@XZ @0x18009ed50
    ??1Goal@Achievement@@UEAA@XZ @0x18009edb0
### Achievement::GoalInstance  vtable=0x180154678  refs=8
    (unexported) FUN_180002070 @0x180002070
    (unexported) FUN_1800020c0 @0x1800020c0
    <no fn> 1801b20c8
    <no fn> Entry Point
    ??0GoalInstance@Achievement@@QEAA@AEBV01@@Z @0x180001ff0
    ??0GoalInstance@Achievement@@QEAA@XZ @0x18009ef00
    ??1GoalInstance@Achievement@@UEAA@XZ @0x18009ef40
    ?CreateNewObject@GoalInstance@Achievement@@SAPEAVBase@Object@@XZ @0x180001f50
### Audio::SoundBuffer  vtable=0x1801549e0  refs=9
    <no fn> 1801b21ac
    <no fn> Entry Point
    ??0SoundBuffer@Audio@@QEAA@AEBV01@@Z @0x18000bd00
    ??0SoundBuffer@Audio@@QEAA@XZ @0x1800e0320
    ??1SoundBuffer@Audio@@UEAA@XZ @0x1800e03b0
    ?Construct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ @0x18000bbd0
    ?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z @0x18000ba90
    ?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ @0x18000b9a0
    ?Reconstruct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ @0x18000bb30
### Audio::SoundEffect  vtable=0x180157ce0  refs=11
    (unexported) FUN_1800507c0 @0x1800507c0
    (unexported) FUN_180050820 @0x180050820
    <no fn> 1801b21b0
    <no fn> Entry Point
    ??0SoundEffect@Audio@@QEAA@AEBV01@@Z @0x1800506c0
    ??0SoundEffect@Audio@@QEAA@XZ @0x1800e0810
    ??1SoundEffect@Audio@@UEAA@XZ @0x1800e0880
    ?Construct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ @0x1800505d0
    ?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z @0x1800504f0
    ?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ @0x180050430
    ?Reconstruct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ @0x180050560
### Audio::Stream  vtable=0x180154950  refs=4
    <no fn> 1801b21c8
    <no fn> Entry Point
    ??0Stream@Audio@@QEAA@XZ @0x1800a0e30
    ??1Stream@Audio@@QEAA@XZ @0x1800a0d90
### Engine::DialogLoaderLoopT  vtable=0x1801578d0  refs=6
    (unexported) FUN_18004d5f0 @0x18004d5f0
    <no fn> 1801b2078
    <no fn> Entry Point
    ??0DialogLoaderLoopT@Engine@@QEAA@AEBV01@@Z @0x18004d570
    ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z @0x1800db0d0
    ??1DialogLoaderLoopT@Engine@@UEAA@XZ @0x1800db060
### Engine::DialogLoopT  vtable=0x1801579a0  refs=5
    <no fn> 1801b207c
    <no fn> Entry Point
    ??0DialogLoopT@Engine@@QEAA@AEBV01@@Z @0x18004d7d0
    ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z @0x1800db3f0
    ??1DialogLoopT@Engine@@UEAA@XZ @0x1800db340
### Engine::Loader  vtable=0x180157a08  refs=4
    (unexported) FUN_180148fe0 @0x180148fe0
    <no fn> 1801b2118
    <no fn> Entry Point
    ??0Loader@Engine@@QEAA@XZ @0x1800dc0f0
### Engine::LoaderLoop  vtable=0x180157940  refs=5
    <no fn> 1801b211c
    <no fn> Entry Point
    ??0LoaderLoop@Engine@@QEAA@AEBV01@@Z @0x18004d3a0
    ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z @0x1800dc1e0
    ??1LoaderLoop@Engine@@UEAA@XZ @0x1800dc170
### Engine::UpdateLoop  vtable=0x180155ba0  refs=10
    (unexported) FUN_180031880 @0x180031880
    <no fn> 1801b2200
    <no fn> Entry Point
    ??0DialogLoopT@Engine@@QEAA@AEBV01@@Z @0x18004d7d0
    ??0LoaderLoop@Engine@@QEAA@AEBV01@@Z @0x18004d3a0
    ??0UpdateLoop@Engine@@QEAA@AEBV01@@Z @0x1800317d0
    ??0UpdateLoop@Engine@@QEAA@XZ @0x1800ddd30
    ??1DialogLoopT@Engine@@UEAA@XZ @0x1800db340
    ??1LoaderLoop@Engine@@UEAA@XZ @0x1800dc170
    ??1UpdateLoop@Engine@@UEAA@XZ @0x1800dddb0
### Pathfinding::IPathInfo  vtable=0x180158f30  refs=4
    <no fn> 1801b20e0
    <no fn> Entry Point
    ??0IPathInfo@Pathfinding@@QEAA@AEBV01@@Z @0x18006c980
    ??0IPathInfo@Pathfinding@@QEAA@XZ @0x18006c970
### Pathfinding::PointList  vtable=0x180158f50  refs=9
    <no fn> 1801b2168
    <no fn> Entry Point
    ??0PointList@Pathfinding@@QEAA@AEBV01@@Z @0x1800711f0
    ??0PointList@Pathfinding@@QEAA@XZ @0x18010f980
    ??1PointList@Pathfinding@@UEAA@XZ @0x18010f8e0
    ?Construct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ @0x1800710e0
    ?CreateInPlaceObject@PointList@Pathfinding@@SAPEAVBase@Object@@PEAX@Z @0x180070ff0
    ?CreateNewObject@PointList@Pathfinding@@SAPEAVBase@Object@@XZ @0x180070f30
    ?Reconstruct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ @0x180071060
### Picking::PickingMesh  vtable=0x180158fb0  refs=9
    <no fn> 1801b215c
    <no fn> Entry Point
    ??0PickingMesh@Picking@@QEAA@AEBV01@@Z @0x180071ea0
    ??0PickingMesh@Picking@@QEAA@XZ @0x180122830
    ??1PickingMesh@Picking@@UEAA@XZ @0x1801228a0
    ?Construct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ @0x180071d40
    ?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z @0x180071c60
    ?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ @0x180071ba0
    ?Reconstruct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ @0x180071cd0
### System::Thread  vtable=0x180155f20  refs=8
    (unexported) FUN_1800b7740 @0x1800b7740
    (unexported) FUN_18014a580 @0x18014a580
    <no fn> 1801b21f8
    <no fn> Entry Point
    ??0Thread@System@@QEAA@I@Z @0x1800ca4e0
    ??1Loader@Engine@@QEAA@XZ @0x18004d330
    ??1Stream@Audio@@QEAA@XZ @0x1800a0d90
    ??1Thread@System@@QEAA@XZ @0x1800ca590
```

`FUN_180148fe0` (Loader) is the DLL static initializer of `?_loader@LoaderLoop@Engine@@` (0x1801fe7b8): `??0Thread`(0x40000),
stamp `??_7Loader`, `_loader+0x30 = 0`, `atexit(FUN_18014a580)`. `??1Loader`, `??1Stream`, `??1Thread` are `QEAA`
(non-virtual) and each re-stamps `??_7Thread@System@@` before tearing the thread down.

## §6 — every export in the set, by class (30 classes, 618 exports), with the demangled signature

```
classes=30
##### Achievement::Achievement   exports=28 vtable=0x180154740
    0x180001000  ?_TypeName@Achievement@1@CAPEBDXZ
                    private static char const * __cdecl Achievement::Achievement::_TypeName(void)
    0x180001010  ?_ParentTypeName@Achievement@1@CAPEBDXZ
                    private static char const * __cdecl Achievement::Achievement::_ParentTypeName(void)
    0x180001020  ?Type@Achievement@1@UEBAIXZ
                    public virtual unsigned int __thiscall Achievement::Achievement::Type(void) const
    0x180001030  ?TypeId@Achievement@1@SAIXZ
                    public static unsigned int __cdecl Achievement::Achievement::TypeId(void)
    0x180001040  ?TypeName@Achievement@1@UEBAPEBDXZ
                    public virtual char const * __thiscall Achievement::Achievement::TypeName(void) const
    0x180001050  ??2Achievement@0@SAPEAX_K@Z
                    public static void * __cdecl Achievement::Achievement::operator new(unsigned __int64)
    0x1800010c0  ??2Achievement@0@SAPEAX_KPEAX@Z
                    public static void * __cdecl Achievement::Achievement::operator new(unsigned __int64,void *)
    0x1800010d0  ??3Achievement@0@SAXPEAX@Z
                    public static void __cdecl Achievement::Achievement::operator delete(void *)
    0x180001130  ?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Achievement::Achievement::CreateNewObject(void)
    0x1800011f0  ?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Achievement::Achievement::CreateInPlaceObject(void *)
    0x180001270  ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Achievement::Achievement::Reconstruct(void)
    0x1800012f0  ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Achievement::Achievement::Construct(void)
    0x180001370  ?Destruct@Achievement@1@UEAAXXZ
                    public virtual void __thiscall Achievement::Achievement::Destruct(void)
    0x180001380  ?GetStringTable@Achievement@1@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ
                    public class Object::ExternalPtr<class UI::StringTable> __thiscall Achievement::Achievement::GetStringTable(void) const
    0x180001390  ?GetSpriteSheet@Achievement@1@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ
                    public class Object::ExternalPtr<class UI::SpriteSheet> __thiscall Achievement::Achievement::GetSpriteSheet(void) const
    0x1800013a0  ?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Achievement::Achievement::GetSpriteName(void) const
    0x1800013e0  ?GetTitle@Achievement@1@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Achievement::Achievement::GetTitle(void) const
    0x180001420  ?GetDescription@Achievement@1@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Achievement::Achievement::GetDescription(void) const
    0x180001460  ?GetGoalList@Achievement@1@QEBAAEBV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@XZ
                    public class Collection::Array<class Object::ExternalPtr<class Achievement::Goal> > const & __thiscall Achievement::Achievement::GetGoalList(void) const
    0x180001470  ??0Achievement@0@QEAA@AEBV00@@Z
                    public __thiscall Achievement::Achievement::Achievement(class Achievement::Achievement const &)
    0x180001550  ??4Achievement@0@QEAAAEAV00@AEBV00@@Z
                    public class Achievement::Achievement & __thiscall Achievement::Achievement::operator=(class Achievement::Achievement const &)
    0x18009df80  ?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@XZ
                    public class Object::InternalPtr<class Achievement::AchievementInstance> __thiscall Achievement::Achievement::CreateInstance(void) const
    0x18009e070  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Achievement::Achievement::Serialize(class IO::Stream &)
    0x18009e170  ?GetFileVersion@Achievement@1@UEBAHXZ
                    public virtual int __thiscall Achievement::Achievement::GetFileVersion(void) const
    0x18009e180  ??1Achievement@0@UEAA@XZ
                    public virtual __thiscall Achievement::Achievement::~Achievement(void)
    0x18009e240  ??0Achievement@0@QEAA@XZ
                    public __thiscall Achievement::Achievement::Achievement(void)
    0x180154740  ??_7Achievement@0@6B@
                    const Achievement::Achievement::`vftable'
    0x1801f9898  ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Achievement::Achievement::_typeInfo
##### Achievement::AchievementInstance   exports=22 vtable=0x180154700
    0x180001780  ?_TypeName@AchievementInstance@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::AchievementInstance::_TypeName(void)
    0x180001790  ?_ParentTypeName@AchievementInstance@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::AchievementInstance::_ParentTypeName(void)
    0x1800017a0  ?Type@AchievementInstance@Achievement@@UEBAIXZ
                    public virtual unsigned int __thiscall Achievement::AchievementInstance::Type(void) const
    0x1800017b0  ?TypeId@AchievementInstance@Achievement@@SAIXZ
                    public static unsigned int __cdecl Achievement::AchievementInstance::TypeId(void)
    0x1800017c0  ?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ
                    public virtual char const * __thiscall Achievement::AchievementInstance::TypeName(void) const
    0x1800017d0  ??2AchievementInstance@Achievement@@SAPEAX_K@Z
                    public static void * __cdecl Achievement::AchievementInstance::operator new(unsigned __int64)
    0x180001840  ??2AchievementInstance@Achievement@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Achievement::AchievementInstance::operator new(unsigned __int64,void *)
    0x180001850  ??3AchievementInstance@Achievement@@SAXPEAX@Z
                    public static void __cdecl Achievement::AchievementInstance::operator delete(void *)
    0x1800018b0  ?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Achievement::AchievementInstance::CreateNewObject(void)
    0x180001950  ?GetAchievement@AchievementInstance@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@XZ
                    public class Object::ExternalPtr<class Achievement::Achievement> __thiscall Achievement::AchievementInstance::GetAchievement(void) const
    0x180001960  ?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ
                    public class Achievement::GoalList const & __thiscall Achievement::AchievementInstance::GetGoalList(void) const
    0x180001970  ??0AchievementInstance@Achievement@@QEAA@AEBV01@@Z
                    public __thiscall Achievement::AchievementInstance::AchievementInstance(class Achievement::AchievementInstance const &)
    0x1800019f0  ??4AchievementInstance@Achievement@@QEAAAEAV01@AEBV01@@Z
                    public class Achievement::AchievementInstance & __thiscall Achievement::AchievementInstance::operator=(class Achievement::AchievementInstance const &)
    0x18009e2b0  ?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::AchievementInstance::HasFailed(void) const
    0x18009e2f0  ?IsMet@AchievementInstance@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::AchievementInstance::IsMet(void) const
    0x18009e340  ?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Achievement::AchievementInstance::Update(class System::Time const &)
    0x18009e3b0  ??0AchievementInstance@Achievement@@QEAA@XZ
                    public __thiscall Achievement::AchievementInstance::AchievementInstance(void)
    0x18009e400  ??1AchievementInstance@Achievement@@UEAA@XZ
                    public virtual __thiscall Achievement::AchievementInstance::~AchievementInstance(void)
    0x18009e440  ?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z
                    public void __thiscall Achievement::AchievementInstance::SetAchievement(class Object::ExternalPtr<class Achievement::Achievement>)
    0x18009e4a0  ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
                    public virtual void __thiscall Achievement::AchievementInstance::Persist(class IO::Stream &)
    0x180154700  ??_7AchievementInstance@Achievement@@6B@
                    const Achievement::AchievementInstance::`vftable'
    0x1801f98d0  ?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Achievement::AchievementInstance::_typeInfo
##### Achievement::AchievementManager   exports=24 (no vtable export)
    0x1800079b0  ?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ
                    public int __thiscall Achievement::AchievementManager::GetAchievementCount(void) const
    0x1800079c0  ?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z
                    public bool __thiscall Achievement::AchievementManager::IsAchievementAwarded(int) const
    0x1800079d0  ?Get@AchievementManager@Achievement@@SAPEAV12@XZ
                    public static class Achievement::AchievementManager * __cdecl Achievement::AchievementManager::Get(void)
    0x1800079e0  ??0AchievementManager@Achievement@@QEAA@AEBV01@@Z
                    public __thiscall Achievement::AchievementManager::AchievementManager(class Achievement::AchievementManager const &)
    0x180007a70  ??4AchievementManager@Achievement@@QEAAAEAV01@AEBV01@@Z
                    public class Achievement::AchievementManager & __thiscall Achievement::AchievementManager::operator=(class Achievement::AchievementManager const &)
    0x18009e5c0  ?Reset@AchievementManager@Achievement@@QEAAXXZ
                    public void __thiscall Achievement::AchievementManager::Reset(void)
    0x18009e6b0  ?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z
                    public void __thiscall Achievement::AchievementManager::SetEventHandler(void (__cdecl *)(int))
    0x18009e6c0  ?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Achievement::AchievementManager::Update(class System::Time const &)
    0x18009e860  ?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z
                    public void __thiscall Achievement::AchievementManager::SetAchievements(class Object::ExternalPtr<class Object::ExternalList>)
    0x18009e8a0  ?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z
                    public class Object::ExternalPtr<class Achievement::Achievement> __thiscall Achievement::AchievementManager::GetAchievement(int) const
    0x18009e8f0  ?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z
                    public class Object::InternalPtr<class Achievement::AchievementInstance> __thiscall Achievement::AchievementManager::GetAchievementInstance(int) const
    0x18009e910  ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z
                    public void __thiscall Achievement::AchievementManager::Persist(class IO::Stream &)
    0x18009e9d0  ?RemoveAll@AchievementManager@Achievement@@QEAAXXZ
                    public void __thiscall Achievement::AchievementManager::RemoveAll(void)
    0x18009ea90  ?Shutdown@AchievementManager@Achievement@@QEAAXXZ
                    public void __thiscall Achievement::AchievementManager::Shutdown(void)
    0x18009eaa0  ?Startup@AchievementManager@Achievement@@QEAA_NXZ
                    public bool __thiscall Achievement::AchievementManager::Startup(void)
    0x18009eab0  ??1AchievementManager@Achievement@@QEAA@XZ
                    public __thiscall Achievement::AchievementManager::~AchievementManager(void)
    0x18009eb00  ??0AchievementManager@Achievement@@QEAA@XZ
                    public __thiscall Achievement::AchievementManager::AchievementManager(void)
    0x18009eb50  ?PlatformConstruct@AchievementManager@Achievement@@AEAAXXZ
                    private void __thiscall Achievement::AchievementManager::PlatformConstruct(void)
    0x18009eb60  ?PlatformDestruct@AchievementManager@Achievement@@AEAAXXZ
                    private void __thiscall Achievement::AchievementManager::PlatformDestruct(void)
    0x18009eb70  ?PlatformStartup@AchievementManager@Achievement@@AEAA_NXZ
                    private bool __thiscall Achievement::AchievementManager::PlatformStartup(void)
    0x18009eb80  ?PlatformShutdown@AchievementManager@Achievement@@AEAAXXZ
                    private void __thiscall Achievement::AchievementManager::PlatformShutdown(void)
    0x18009eb90  ?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ
                    public void __thiscall Achievement::AchievementManager::ReadAchievements(void)
    0x18009eca0  ?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z
                    public bool __thiscall Achievement::AchievementManager::SetAchievementMet(int)
    0x1801fc670  ?_singleton@AchievementManager@Achievement@@0PEAV12@EA
                    private static class Achievement::AchievementManager * Achievement::AchievementManager::_singleton
##### Achievement::Goal   exports=18 vtable=0x1801546b0
    0x180001b20  ?_TypeName@Goal@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::Goal::_TypeName(void)
    0x180001b30  ?_ParentTypeName@Goal@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::Goal::_ParentTypeName(void)
    0x180001b40  ?Type@Goal@Achievement@@UEBAIXZ
                    public virtual unsigned int __thiscall Achievement::Goal::Type(void) const
    0x180001b50  ?TypeId@Goal@Achievement@@SAIXZ
                    public static unsigned int __cdecl Achievement::Goal::TypeId(void)
    0x180001b60  ?TypeName@Goal@Achievement@@UEBAPEBDXZ
                    public virtual char const * __thiscall Achievement::Goal::TypeName(void) const
    0x180001b70  ?CreateNewObject@Goal@Achievement@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Achievement::Goal::CreateNewObject(void)
    0x180001b80  ?CreateInPlaceObject@Goal@Achievement@@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Achievement::Goal::CreateInPlaceObject(void *)
    0x180001b90  ?GetStringTable@Goal@Achievement@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ
                    public class Object::ExternalPtr<class UI::StringTable> __thiscall Achievement::Goal::GetStringTable(void) const
    0x180001ba0  ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Achievement::Goal::GetText(void) const
    0x180001be0  ??0Goal@Achievement@@QEAA@AEBV01@@Z
                    public __thiscall Achievement::Goal::Goal(class Achievement::Goal const &)
    0x180001c80  ??4Goal@Achievement@@QEAAAEAV01@AEBV01@@Z
                    public class Achievement::Goal & __thiscall Achievement::Goal::operator=(class Achievement::Goal const &)
    0x18009ed50  ??0Goal@Achievement@@QEAA@XZ
                    public __thiscall Achievement::Goal::Goal(void)
    0x18009edb0  ??1Goal@Achievement@@UEAA@XZ
                    public virtual __thiscall Achievement::Goal::~Goal(void)
    0x18009ee10  ?GetFileVersion@Goal@Achievement@@UEBAHXZ
                    public virtual int __thiscall Achievement::Goal::GetFileVersion(void) const
    0x18009ee20  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Achievement::Goal::Serialize(class IO::Stream &)
    0x18009eeb0  ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
                    public virtual class Object::Type __thiscall Achievement::Goal::GetInstanceType(void) const
    0x1801546b0  ??_7Goal@Achievement@@6B@
                    const Achievement::Goal::`vftable'
    0x1801f9908  ?_typeInfo@Goal@Achievement@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Achievement::Goal::_typeInfo
##### Achievement::GoalInstance   exports=23 vtable=0x180154678
    0x180001e20  ?_TypeName@GoalInstance@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::GoalInstance::_TypeName(void)
    0x180001e30  ?_ParentTypeName@GoalInstance@Achievement@@CAPEBDXZ
                    private static char const * __cdecl Achievement::GoalInstance::_ParentTypeName(void)
    0x180001e40  ?Type@GoalInstance@Achievement@@UEBAIXZ
                    public virtual unsigned int __thiscall Achievement::GoalInstance::Type(void) const
    0x180001e50  ?TypeId@GoalInstance@Achievement@@SAIXZ
                    public static unsigned int __cdecl Achievement::GoalInstance::TypeId(void)
    0x180001e60  ?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ
                    public virtual char const * __thiscall Achievement::GoalInstance::TypeName(void) const
    0x180001e70  ??2GoalInstance@Achievement@@SAPEAX_K@Z
                    public static void * __cdecl Achievement::GoalInstance::operator new(unsigned __int64)
    0x180001ee0  ??2GoalInstance@Achievement@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Achievement::GoalInstance::operator new(unsigned __int64,void *)
    0x180001ef0  ??3GoalInstance@Achievement@@SAXPEAX@Z
                    public static void __cdecl Achievement::GoalInstance::operator delete(void *)
    0x180001f50  ?CreateNewObject@GoalInstance@Achievement@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Achievement::GoalInstance::CreateNewObject(void)
    0x180001fe0  ?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ
                    public class Object::ExternalPtr<class Achievement::Goal> __thiscall Achievement::GoalInstance::GetGoal(void) const
    0x180001ff0  ??0GoalInstance@Achievement@@QEAA@AEBV01@@Z
                    public __thiscall Achievement::GoalInstance::GoalInstance(class Achievement::GoalInstance const &)
    0x180002050  ??4GoalInstance@Achievement@@QEAAAEAV01@AEBV01@@Z
                    public class Achievement::GoalInstance & __thiscall Achievement::GoalInstance::operator=(class Achievement::GoalInstance const &)
    0x18009eec0  ?HasFailed@GoalInstance@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::GoalInstance::HasFailed(void) const
    0x18009eed0  ?IsMet@GoalInstance@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::GoalInstance::IsMet(void) const
    0x18009eee0  ?SetGoal@GoalInstance@Achievement@@QEAAXV?$ExternalPtr@VGoal@Achievement@@@Object@@@Z
                    public void __thiscall Achievement::GoalInstance::SetGoal(class Object::ExternalPtr<class Achievement::Goal>)
    0x18009ef00  ??0GoalInstance@Achievement@@QEAA@XZ
                    public __thiscall Achievement::GoalInstance::GoalInstance(void)
    0x18009ef40  ??1GoalInstance@Achievement@@UEAA@XZ
                    public virtual __thiscall Achievement::GoalInstance::~GoalInstance(void)
    0x18009ef70  ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
                    public virtual void __thiscall Achievement::GoalInstance::Persist(class IO::Stream &)
    0x18009f000  ?Startup@GoalInstance@Achievement@@UEAAXXZ
                    public virtual void __thiscall Achievement::GoalInstance::Startup(void)
    0x18009f010  ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
                    public virtual void __thiscall Achievement::GoalInstance::Update(class System::Time const &,class Achievement::GoalList const &)
    0x18009f060  ?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ
                    public virtual enum Achievement::Goal::GoalState __thiscall Achievement::GoalInstance::CheckGoalState(void) const
    0x180154678  ??_7GoalInstance@Achievement@@6B@
                    const Achievement::GoalInstance::`vftable'
    0x1801f9940  ?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Achievement::GoalInstance::_typeInfo
##### Achievement::GoalList   exports=12 (no vtable export)
    0x180001700  ?GetGoalCount@GoalList@Achievement@@QEBAHXZ
                    public int __thiscall Achievement::GoalList::GetGoalCount(void) const
    0x180001710  ?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z
                    public class Object::InternalPtr<class Achievement::GoalInstance> __thiscall Achievement::GoalList::GetGoal(int) const
    0x180001730  ??0GoalList@Achievement@@QEAA@AEBV01@@Z
                    public __thiscall Achievement::GoalList::GoalList(class Achievement::GoalList const &)
    0x180001760  ??4GoalList@Achievement@@QEAAAEAV01@AEBV01@@Z
                    public class Achievement::GoalList & __thiscall Achievement::GoalList::operator=(class Achievement::GoalList const &)
    0x18009f070  ?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z
                    public bool __thiscall Achievement::GoalList::AreGoalsMetExcept(class Object::InternalPtr<class Achievement::GoalInstance>) const
    0x18009f0b0  ??0GoalList@Achievement@@QEAA@XZ
                    public __thiscall Achievement::GoalList::GoalList(void)
    0x18009f0e0  ?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z
                    public void __thiscall Achievement::GoalList::Initialize(class Collection::Array<class Object::ExternalPtr<class Achievement::Goal> >)
    0x18009f270  ?Update@GoalList@Achievement@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Achievement::GoalList::Update(class System::Time const &)
    0x18009f2e0  ?AreGoalsMet@GoalList@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::GoalList::AreGoalsMet(void) const
    0x18009f330  ?AnyGoalFailed@GoalList@Achievement@@QEBA_NXZ
                    public bool __thiscall Achievement::GoalList::AnyGoalFailed(void) const
    0x18009f370  ?Serialize@GoalList@Achievement@@QEAAXAEAVSerializer@IO@@@Z
                    public void __thiscall Achievement::GoalList::Serialize(class IO::Serializer &)
    0x18009f420  ??1GoalList@Achievement@@QEAA@XZ
                    public __thiscall Achievement::GoalList::~GoalList(void)
##### Application::Application   exports=27 (no vtable export)
    0x18000a0c0  ?IsCursorEnabled@Application@1@QEAA_NXZ
                    public bool __thiscall Application::Application::IsCursorEnabled(void)
    0x18000a0d0  ??0Application@0@QEAA@AEBV00@@Z
                    public __thiscall Application::Application::Application(class Application::Application const &)
    0x18000a160  ??4Application@0@QEAAAEAV00@AEBV00@@Z
                    public class Application::Application & __thiscall Application::Application::operator=(class Application::Application const &)
    0x18000a2f0  ?Get@Application@1@SAPEAV11@XZ
                    public static class Application::Application * __cdecl Application::Application::Get(void)
    0x18009f4d0  ?Quit@Application@1@QEAAXXZ
                    public void __thiscall Application::Application::Quit(void)
    0x18009f4e0  ?IsPaused@Application@1@QEAA_NXZ
                    public bool __thiscall Application::Application::IsPaused(void)
    0x18009f4f0  ?Update@Application@1@QEAA_NXZ
                    public bool __thiscall Application::Application::Update(void)
    0x18009f500  ?Shutdown@Application@1@QEAAXXZ
                    public void __thiscall Application::Application::Shutdown(void)
    0x18009f510  ?Startup@Application@1@QEAA_NXZ
                    public bool __thiscall Application::Application::Startup(void)
    0x18009f520  ??1Application@0@QEAA@XZ
                    public __thiscall Application::Application::~Application(void)
    0x18009f590  ??0Application@0@QEAA@PEAX@Z
                    public __thiscall Application::Application::Application(void *)
    0x18009f5e0  ?LaunchProcess@Application@1@QEAA_NPEBD_N@Z
                    public bool __thiscall Application::Application::LaunchProcess(char const *,bool)
    0x18009f800  ?EnableCursor@Application@1@QEAAX_N@Z
                    public void __thiscall Application::Application::EnableCursor(bool)
    0x18009f850  ?SleepThread@Application@1@QEAAXH@Z
                    public void __thiscall Application::Application::SleepThread(int)
    0x18009f920  ?YieldToThreads@Application@1@QEAAXXZ
                    public void __thiscall Application::Application::YieldToThreads(void)
    0x18009f930  ?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z
                    private static __int64 __cdecl Application::Application::WindowProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)
    0x18009fa20  ?UnregisterMessageCallback@Application@1@QEAAXI@Z
                    public void __thiscall Application::Application::UnregisterMessageCallback(unsigned int)
    0x18009fa60  ?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z
                    public void __thiscall Application::Application::RegisterMessageCallback(unsigned int,int (__cdecl *)(struct HWND__ *,unsigned int,unsigned __int64,__int64))
    0x18009fb90  ?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ
                    public struct HINSTANCE__ * __thiscall Application::Application::GetInstance(void) const
    0x18009fba0  ?GetWindow@Application@1@QEBAPEAUHWND__@@XZ
                    public struct HWND__ * __thiscall Application::Application::GetWindow(void) const
    0x18009fbe0  ?PlatformConstruct@Application@1@AEAAXPEAX@Z
                    private void __thiscall Application::Application::PlatformConstruct(void *)
    0x18009fc00  ?PlatformDestruct@Application@1@AEAAXXZ
                    private void __thiscall Application::Application::PlatformDestruct(void)
    0x18009fc10  ?PlatformStartup@Application@1@AEAA_NXZ
                    private bool __thiscall Application::Application::PlatformStartup(void)
    0x18009fd70  ?PlatformShutdown@Application@1@AEAAXXZ
                    private void __thiscall Application::Application::PlatformShutdown(void)
    0x18009fdf0  ?PlatformUpdate@Application@1@AEAA_NXZ
                    private bool __thiscall Application::Application::PlatformUpdate(void)
    0x18009fe80  ?PlatformIsPaused@Application@1@AEAA_NXZ
                    private bool __thiscall Application::Application::PlatformIsPaused(void)
    0x1801fc678  ?_singleton@Application@1@0PEAV11@EA
                    private static class Application::Application * Application::Application::_singleton
##### Audio::Configuration   exports=16 (no vtable export)
    0x18000d470  ??4Configuration@Audio@@QEAAAEAV01@AEBV01@@Z
                    public class Audio::Configuration & __thiscall Audio::Configuration::operator=(class Audio::Configuration const &)
    0x18000d490  ?Get@Configuration@Audio@@SAPEAV12@XZ
                    public static class Audio::Configuration * __cdecl Audio::Configuration::Get(void)
    0x18000d4a0  ?GetUIVolume@Configuration@Audio@@QEBAHXZ
                    public int __thiscall Audio::Configuration::GetUIVolume(void) const
    0x18000d4b0  ?GetMusicVolume@Configuration@Audio@@QEBAHXZ
                    public int __thiscall Audio::Configuration::GetMusicVolume(void) const
    0x18000d4c0  ?GetAmbientVolume@Configuration@Audio@@QEBAHXZ
                    public int __thiscall Audio::Configuration::GetAmbientVolume(void) const
    0x18000d4d0  ?GetEffectVolume@Configuration@Audio@@QEBAHXZ
                    public int __thiscall Audio::Configuration::GetEffectVolume(void) const
    0x18000d4e0  ?SetUIVolume@Configuration@Audio@@QEAAXH@Z
                    public void __thiscall Audio::Configuration::SetUIVolume(int)
    0x18000d4f0  ?SetMusicVolume@Configuration@Audio@@QEAAXH@Z
                    public void __thiscall Audio::Configuration::SetMusicVolume(int)
    0x18000d500  ?SetAmbientVolume@Configuration@Audio@@QEAAXH@Z
                    public void __thiscall Audio::Configuration::SetAmbientVolume(int)
    0x18000d510  ?SetEffectVolume@Configuration@Audio@@QEAAXH@Z
                    public void __thiscall Audio::Configuration::SetEffectVolume(int)
    0x1800dde10  ?Save@Configuration@Audio@@QEAAXXZ
                    public void __thiscall Audio::Configuration::Save(void)
    0x1800ddea0  ?Shutdown@Configuration@Audio@@QEAAXXZ
                    public void __thiscall Audio::Configuration::Shutdown(void)
    0x1800ddeb0  ??0Configuration@Audio@@QEAA@XZ
                    public __thiscall Audio::Configuration::Configuration(void)
    0x1800ddee0  ?Startup@Configuration@Audio@@QEAA_NXZ
                    public bool __thiscall Audio::Configuration::Startup(void)
    0x1800de0d0  ??1Configuration@Audio@@QEAA@XZ
                    public __thiscall Audio::Configuration::~Configuration(void)
    0x1801fe758  ?_singleton@Configuration@Audio@@0PEAV12@EA
                    private static class Audio::Configuration * Audio::Configuration::_singleton
##### Audio::Device   exports=23 (no vtable export)
    0x18000c710  ?GetChannel@Device@Audio@@QEAAPEAUAudioChannel@12@H@Z
                    public struct Audio::Device::AudioChannel * __thiscall Audio::Device::GetChannel(int)
    0x18000c720  ?GetPlatformChannel@Device@Audio@@QEAAPEAUPlatformChannel@12@H@Z
                    public struct Audio::Device::PlatformChannel * __thiscall Audio::Device::GetPlatformChannel(int)
    0x18000c740  ?Get@Device@Audio@@SAPEAV12@XZ
                    public static class Audio::Device * __cdecl Audio::Device::Get(void)
    0x1800de0e0  ?OnSoundBufferDeleted@Device@Audio@@QEAAXPEBVSoundBuffer@2@@Z
                    public void __thiscall Audio::Device::OnSoundBufferDeleted(class Audio::SoundBuffer const *)
    0x1800de180  ?FindFreeChannel@Device@Audio@@AEBAHXZ
                    private int __thiscall Audio::Device::FindFreeChannel(void) const
    0x1800de1b0  ?SetChannelVolume@Device@Audio@@QEAAXPEAUAudioChannel@12@M@Z
                    public void __thiscall Audio::Device::SetChannelVolume(struct Audio::Device::AudioChannel *,float)
    0x1800de200  ?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z
                    public void __thiscall Audio::Device::Stop(struct Audio::Device::AudioChannel *)
    0x1800de280  ?Play@Device@Audio@@QEAAPEAUAudioChannel@12@V?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMMP6AXPEAU312@PEAXI@Z3I@Z
                    public struct Audio::Device::AudioChannel * __thiscall Audio::Device::Play(class Object::ExternalPtr<class Audio::SoundBuffer>,class System::Time const &,float,float,float,void (__cdecl *)(struct Audio::Device::AudioChannel *,void *,unsigned int),void *,unsigned int)
    0x1800de350  ?FreeChannel@Device@Audio@@AEAAXH@Z
                    private void __thiscall Audio::Device::FreeChannel(int)
    0x1800de3a0  ??0Device@Audio@@QEAA@XZ
                    public __thiscall Audio::Device::Device(void)
    0x1800de480  ?Startup@Device@Audio@@QEAA_NXZ
                    public bool __thiscall Audio::Device::Startup(void)
    0x1800de490  ?Shutdown@Device@Audio@@QEAAXXZ
                    public void __thiscall Audio::Device::Shutdown(void)
    0x1800de4f0  ??1Device@Audio@@QEAA@XZ
                    public __thiscall Audio::Device::~Device(void)
    0x1800de520  ?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Audio::Device::Update(class System::Time const &)
    0x1800de5b0  ?PlatformConstruct@Device@Audio@@AEAAXXZ
                    private void __thiscall Audio::Device::PlatformConstruct(void)
    0x1800de5f0  ?PlatformDestruct@Device@Audio@@AEAAXXZ
                    private void __thiscall Audio::Device::PlatformDestruct(void)
    0x1800de600  ?PlatformStartup@Device@Audio@@AEAA_NXZ
                    private bool __thiscall Audio::Device::PlatformStartup(void)
    0x1800de690  ?PlatformShutdown@Device@Audio@@AEAAXXZ
                    private void __thiscall Audio::Device::PlatformShutdown(void)
    0x1800de6f0  ?FreeChannelPlatform@Device@Audio@@AEAAXH@Z
                    private void __thiscall Audio::Device::FreeChannelPlatform(int)
    0x1800de7a0  ?PlayPlatform@Device@Audio@@AEAA_NHV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMM@Z
                    private bool __thiscall Audio::Device::PlayPlatform(int,class Object::ExternalPtr<class Audio::SoundBuffer>,class System::Time const &,float,float,float)
    0x1800dea20  ?StopPlatform@Device@Audio@@AEAAXH@Z
                    private void __thiscall Audio::Device::StopPlatform(int)
    0x1800deab0  ?SetVolumePlatform@Device@Audio@@AEAAXHM@Z
                    private void __thiscall Audio::Device::SetVolumePlatform(int,float)
    0x1801fe760  ?_singleton@Device@Audio@@0PEAV12@EA
                    private static class Audio::Device * Audio::Device::_singleton
##### Audio::Scene   exports=33 (no vtable export)
    0x1800508b0  ??2Scene@Audio@@SAPEAX_K@Z
                    public static void * __cdecl Audio::Scene::operator new(unsigned __int64)
    0x180050920  ??2Scene@Audio@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Audio::Scene::operator new(unsigned __int64,void *)
    0x180050930  ??3Scene@Audio@@SAXPEAX@Z
                    public static void __cdecl Audio::Scene::operator delete(void *)
    0x180050990  ?IsPaused@Scene@Audio@@QEBA_NH@Z
                    public bool __thiscall Audio::Scene::IsPaused(int) const
    0x180050a50  ??0Scene@Audio@@QEAA@AEBV01@@Z
                    public __thiscall Audio::Scene::Scene(class Audio::Scene const &)
    0x180050ad0  ??4Scene@Audio@@QEAAAEAV01@AEBV01@@Z
                    public class Audio::Scene & __thiscall Audio::Scene::operator=(class Audio::Scene const &)
    0x1800deae0  ?FindFree3DEffect@Scene@Audio@@AEBAHV?$ExternalPtr@VSoundEffect@Audio@@@Object@@AEBVTime@System@@AEBVVector3@Math@@@Z
                    private int __thiscall Audio::Scene::FindFree3DEffect(class Object::ExternalPtr<class Audio::SoundEffect>,class System::Time const &,class Math::Vector3 const &) const
    0x1800debc0  ?FindFreeEffect@Scene@Audio@@AEBAHXZ
                    private int __thiscall Audio::Scene::FindFreeEffect(void) const
    0x1800dec00  ?GetEmitterCount@Scene@Audio@@QEBAHI@Z
                    public int __thiscall Audio::Scene::GetEmitterCount(unsigned int) const
    0x1800dec40  ?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z
                    public void __thiscall Audio::Scene::MoveEmitter(unsigned int,class Math::Vector3 const &,class Math::Vector3 const &)
    0x1800decf0  ?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z
                    public void __thiscall Audio::Scene::RemoveEmitter(unsigned int,class Math::Vector3 const &)
    0x1800ded60  ?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z
                    public void __thiscall Audio::Scene::SetEmitterVolume(unsigned int,class Math::Vector3 const &,float)
    0x1800dedf0  ?AddEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z
                    public void __thiscall Audio::Scene::AddEmitter(unsigned int,class Math::Vector3 const &,float)
    0x1800dee70  ?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z
                    public unsigned int __thiscall Audio::Scene::FindEffect3D(class Object::ExternalPtr<class Audio::SoundEffect>)
    0x1800deeb0  ?PlayEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@AEBVVector3@Math@@@Z
                    public unsigned int __thiscall Audio::Scene::PlayEffect3D(class Object::ExternalPtr<class Audio::SoundEffect>,enum Audio::Scene::SoundType,class System::Time const &,class Math::Vector3 const &)
    0x1800def60  ?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z
                    public unsigned int __thiscall Audio::Scene::PlayEffect(class Object::ExternalPtr<class Audio::SoundEffect>,enum Audio::Scene::SoundType,class System::Time const &)
    0x1800df000  ?SetEffectVolume3D@Scene@Audio@@QEAAXIM@Z
                    public void __thiscall Audio::Scene::SetEffectVolume3D(unsigned int,float)
    0x1800df040  ?SetEffectVolume@Scene@Audio@@QEAAXIM@Z
                    public void __thiscall Audio::Scene::SetEffectVolume(unsigned int,float)
    0x1800df070  ?Pause@Scene@Audio@@QEAAXH_N@Z
                    public void __thiscall Audio::Scene::Pause(int,bool)
    0x1800df150  ?StopAll@Scene@Audio@@QEAAXXZ
                    public void __thiscall Audio::Scene::StopAll(void)
    0x1800df370  ?StopEffect3D@Scene@Audio@@QEAAXI@Z
                    public void __thiscall Audio::Scene::StopEffect3D(unsigned int)
    0x1800df470  ?UnpauseEffect@Scene@Audio@@QEAAXI@Z
                    public void __thiscall Audio::Scene::UnpauseEffect(unsigned int)
    0x1800df4a0  ?PauseEffect@Scene@Audio@@QEAAXI@Z
                    public void __thiscall Audio::Scene::PauseEffect(unsigned int)
    0x1800df550  ?StopEffect@Scene@Audio@@QEAAXI@Z
                    public void __thiscall Audio::Scene::StopEffect(unsigned int)
    0x1800df680  ?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z
                    public bool __thiscall Audio::Scene::IsEffectPlaying(unsigned int) const
    0x1800df6c0  ?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z
                    public bool __thiscall Audio::Scene::Update(class System::Time,class System::Time,class System::Random &,class Math::Vector3 const &,float)
    0x1800dfde0  ?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@AEBVVector3@Math@@M@Z
                    private float __thiscall Audio::Scene::GetAttenuatedVolume(struct Audio::Scene::AttenutedEffect const &,class Math::Vector3 const &,float) const
    0x1800dfed0  ?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z
                    private float __thiscall Audio::Scene::GetVolumeForPosition(class Math::Vector3 const &,float,float,class Math::Vector3 const &) const
    0x1800dff70  ?SoundEnded@Scene@Audio@@QEAAXPEAUAudioChannel@Device@2@I@Z
                    public void __thiscall Audio::Scene::SoundEnded(struct Audio::Device::AudioChannel *,unsigned int)
    0x1800dffe0  ?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z
                    public static void __cdecl Audio::Scene::OnSoundEnded(struct Audio::Device::AudioChannel *,void *,unsigned int)
    0x1800e0050  ?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z
                    public void __thiscall Audio::Scene::Serialize(class IO::Serializer &)
    0x1800e0120  ??1Scene@Audio@@QEAA@XZ
                    public __thiscall Audio::Scene::~Scene(void)
    0x1800e0160  ??0Scene@Audio@@QEAA@XZ
                    public __thiscall Audio::Scene::Scene(void)
##### Audio::SoundBuffer   exports=33 vtable=0x1801549e0
    0x18000b870  ?_TypeName@SoundBuffer@Audio@@CAPEBDXZ
                    private static char const * __cdecl Audio::SoundBuffer::_TypeName(void)
    0x18000b880  ?_ParentTypeName@SoundBuffer@Audio@@CAPEBDXZ
                    private static char const * __cdecl Audio::SoundBuffer::_ParentTypeName(void)
    0x18000b890  ?Type@SoundBuffer@Audio@@UEBAIXZ
                    public virtual unsigned int __thiscall Audio::SoundBuffer::Type(void) const
    0x18000b8a0  ?TypeId@SoundBuffer@Audio@@SAIXZ
                    public static unsigned int __cdecl Audio::SoundBuffer::TypeId(void)
    0x18000b8b0  ?TypeName@SoundBuffer@Audio@@UEBAPEBDXZ
                    public virtual char const * __thiscall Audio::SoundBuffer::TypeName(void) const
    0x18000b8c0  ??2SoundBuffer@Audio@@SAPEAX_K@Z
                    public static void * __cdecl Audio::SoundBuffer::operator new(unsigned __int64)
    0x18000b930  ??2SoundBuffer@Audio@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Audio::SoundBuffer::operator new(unsigned __int64,void *)
    0x18000b940  ??3SoundBuffer@Audio@@SAXPEAX@Z
                    public static void __cdecl Audio::SoundBuffer::operator delete(void *)
    0x18000b9a0  ?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Audio::SoundBuffer::CreateNewObject(void)
    0x18000ba90  ?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Audio::SoundBuffer::CreateInPlaceObject(void *)
    0x18000bb30  ?Reconstruct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Audio::SoundBuffer::Reconstruct(void)
    0x18000bbd0  ?Construct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Audio::SoundBuffer::Construct(void)
    0x18000bc70  ?Destruct@SoundBuffer@Audio@@UEAAXXZ
                    public virtual void __thiscall Audio::SoundBuffer::Destruct(void)
    0x18000bc80  ?IsLooping@SoundBuffer@Audio@@QEBA_NXZ
                    public bool __thiscall Audio::SoundBuffer::IsLooping(void) const
    0x18000bc90  ?IsStreamed@SoundBuffer@Audio@@QEBA_NXZ
                    public bool __thiscall Audio::SoundBuffer::IsStreamed(void) const
    0x18000bca0  ?GetStreamPath@SoundBuffer@Audio@@QEBAAEBVString@System@@XZ
                    public class System::String const & __thiscall Audio::SoundBuffer::GetStreamPath(void) const
    0x18000bcb0  ?Format@SoundBuffer@Audio@@QEBAPEBUtWAVEFORMATEX@@XZ
                    public struct tWAVEFORMATEX const * __thiscall Audio::SoundBuffer::Format(void) const
    0x18000bcc0  ?GetLength@SoundBuffer@Audio@@QEBAHXZ
                    public int __thiscall Audio::SoundBuffer::GetLength(void) const
    0x18000bcd0  ?GetData@SoundBuffer@Audio@@QEBAPEBXXZ
                    public void const * __thiscall Audio::SoundBuffer::GetData(void) const
    0x18000bce0  ?GetWMAData@SoundBuffer@Audio@@QEBAPEBXXZ
                    public void const * __thiscall Audio::SoundBuffer::GetWMAData(void) const
    0x18000bcf0  ?GetWMALength@SoundBuffer@Audio@@QEBAHXZ
                    public int __thiscall Audio::SoundBuffer::GetWMALength(void) const
    0x18000bd00  ??0SoundBuffer@Audio@@QEAA@AEBV01@@Z
                    public __thiscall Audio::SoundBuffer::SoundBuffer(class Audio::SoundBuffer const &)
    0x18000bdc0  ??4SoundBuffer@Audio@@QEAAAEAV01@AEBV01@@Z
                    public class Audio::SoundBuffer & __thiscall Audio::SoundBuffer::operator=(class Audio::SoundBuffer const &)
    0x1800e0320  ??0SoundBuffer@Audio@@QEAA@XZ
                    public __thiscall Audio::SoundBuffer::SoundBuffer(void)
    0x1800e03b0  ??1SoundBuffer@Audio@@UEAA@XZ
                    public virtual __thiscall Audio::SoundBuffer::~SoundBuffer(void)
    0x1800e0470  ?GetFileVersion@SoundBuffer@Audio@@UEBAHXZ
                    public virtual int __thiscall Audio::SoundBuffer::GetFileVersion(void) const
    0x1800e0480  ?Serialize@SoundBuffer@Audio@@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Audio::SoundBuffer::Serialize(class IO::Stream &)
    0x1800e0550  ?PlatformConstruct@SoundBuffer@Audio@@AEAAXXZ
                    private void __thiscall Audio::SoundBuffer::PlatformConstruct(void)
    0x1800e0580  ?PlatformDestruct@SoundBuffer@Audio@@AEAAXXZ
                    private void __thiscall Audio::SoundBuffer::PlatformDestruct(void)
    0x1800e05c0  ?PlatformSerialize@SoundBuffer@Audio@@AEAAXAEAVSerializer@IO@@@Z
                    private void __thiscall Audio::SoundBuffer::PlatformSerialize(class IO::Serializer &)
    0x1800e06c0  ?GetLengthSeconds@SoundBuffer@Audio@@QEBAMXZ
                    public float __thiscall Audio::SoundBuffer::GetLengthSeconds(void) const
    0x1801549e0  ??_7SoundBuffer@Audio@@6B@
                    const Audio::SoundBuffer::`vftable'
    0x1801fa4d8  ?_typeInfo@SoundBuffer@Audio@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Audio::SoundBuffer::_typeInfo
##### Audio::SoundEffect   exports=29 vtable=0x180157ce0
    0x180050300  ?_TypeName@SoundEffect@Audio@@CAPEBDXZ
                    private static char const * __cdecl Audio::SoundEffect::_TypeName(void)
    0x180050310  ?_ParentTypeName@SoundEffect@Audio@@CAPEBDXZ
                    private static char const * __cdecl Audio::SoundEffect::_ParentTypeName(void)
    0x180050320  ?Type@SoundEffect@Audio@@UEBAIXZ
                    public virtual unsigned int __thiscall Audio::SoundEffect::Type(void) const
    0x180050330  ?TypeId@SoundEffect@Audio@@SAIXZ
                    public static unsigned int __cdecl Audio::SoundEffect::TypeId(void)
    0x180050340  ?TypeName@SoundEffect@Audio@@UEBAPEBDXZ
                    public virtual char const * __thiscall Audio::SoundEffect::TypeName(void) const
    0x180050350  ??2SoundEffect@Audio@@SAPEAX_K@Z
                    public static void * __cdecl Audio::SoundEffect::operator new(unsigned __int64)
    0x1800503c0  ??2SoundEffect@Audio@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Audio::SoundEffect::operator new(unsigned __int64,void *)
    0x1800503d0  ??3SoundEffect@Audio@@SAXPEAX@Z
                    public static void __cdecl Audio::SoundEffect::operator delete(void *)
    0x180050430  ?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Audio::SoundEffect::CreateNewObject(void)
    0x1800504f0  ?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Audio::SoundEffect::CreateInPlaceObject(void *)
    0x180050560  ?Reconstruct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Audio::SoundEffect::Reconstruct(void)
    0x1800505d0  ?Construct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Audio::SoundEffect::Construct(void)
    0x180050640  ?Destruct@SoundEffect@Audio@@UEAAXXZ
                    public virtual void __thiscall Audio::SoundEffect::Destruct(void)
    0x180050650  ?GetSoundCount@SoundEffect@Audio@@QEBAHXZ
                    public int __thiscall Audio::SoundEffect::GetSoundCount(void) const
    0x180050660  ?GetStartTime@SoundEffect@Audio@@QEBAMH@Z
                    public float __thiscall Audio::SoundEffect::GetStartTime(int) const
    0x180050680  ?GetRadius@SoundEffect@Audio@@QEBAMXZ
                    public float __thiscall Audio::SoundEffect::GetRadius(void) const
    0x180050690  ?GetInnerRadius@SoundEffect@Audio@@QEBAMXZ
                    public float __thiscall Audio::SoundEffect::GetInnerRadius(void) const
    0x1800506a0  ?GetMaxLength@SoundEffect@Audio@@QEBA?AVTime@System@@XZ
                    public class System::Time __thiscall Audio::SoundEffect::GetMaxLength(void) const
    0x1800506b0  ?IsLooping@SoundEffect@Audio@@QEBA_NXZ
                    public bool __thiscall Audio::SoundEffect::IsLooping(void) const
    0x1800506c0  ??0SoundEffect@Audio@@QEAA@AEBV01@@Z
                    public __thiscall Audio::SoundEffect::SoundEffect(class Audio::SoundEffect const &)
    0x180050770  ??4SoundEffect@Audio@@QEAAAEAV01@AEBV01@@Z
                    public class Audio::SoundEffect & __thiscall Audio::SoundEffect::operator=(class Audio::SoundEffect const &)
    0x1800e0810  ??0SoundEffect@Audio@@QEAA@XZ
                    public __thiscall Audio::SoundEffect::SoundEffect(void)
    0x1800e0880  ??1SoundEffect@Audio@@UEAA@XZ
                    public virtual __thiscall Audio::SoundEffect::~SoundEffect(void)
    0x1800e08c0  ?GetSoundBuffer@SoundEffect@Audio@@QEBA?AV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@HAEAVRandom@System@@AEAM11@Z
                    public class Object::ExternalPtr<class Audio::SoundBuffer> __thiscall Audio::SoundEffect::GetSoundBuffer(int,class System::Random &,float &,float &,float &) const
    0x1800e0990  ?GetFileVersion@SoundEffect@Audio@@UEBAHXZ
                    public virtual int __thiscall Audio::SoundEffect::GetFileVersion(void) const
    0x1800e09a0  ?Compile@SoundEffect@Audio@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
                    public virtual enum Object::External::CompileResult __thiscall Audio::SoundEffect::Compile(class System::Filename const &)
    0x1800e0aa0  ?Serialize@SoundEffect@Audio@@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Audio::SoundEffect::Serialize(class IO::Stream &)
    0x180157ce0  ??_7SoundEffect@Audio@@6B@
                    const Audio::SoundEffect::`vftable'
    0x1801fa510  ?_typeInfo@SoundEffect@Audio@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Audio::SoundEffect::_typeInfo
##### Audio::Stream   exports=10 vtable=0x180154950
    0x18000b650  ?Lock@Stream@Audio@@QEAAXXZ
                    public void __thiscall Audio::Stream::Lock(void)
    0x18000b660  ?Unlock@Stream@Audio@@QEAAXXZ
                    public void __thiscall Audio::Stream::Unlock(void)
    0x1800a0d10  ?Play@Stream@Audio@@QEAAXHH@Z
                    public void __thiscall Audio::Stream::Play(int,int)
    0x1800a0d90  ??1Stream@Audio@@QEAA@XZ
                    public __thiscall Audio::Stream::~Stream(void)
    0x1800a0e30  ??0Stream@Audio@@QEAA@XZ
                    public __thiscall Audio::Stream::Stream(void)
    0x1800a0ed0  ?PlatformConstruct@Stream@Audio@@AEAAXXZ
                    private void __thiscall Audio::Stream::PlatformConstruct(void)
    0x1800a0f10  ?PlatformDestruct@Stream@Audio@@AEAAXXZ
                    private void __thiscall Audio::Stream::PlatformDestruct(void)
    0x1800a0f40  ?Stop@Stream@Audio@@QEAAXXZ
                    public void __thiscall Audio::Stream::Stop(void)
    0x1800a0f60  ?Run@Stream@Audio@@UEAAXXZ
                    public virtual void __thiscall Audio::Stream::Run(void)
    0x180154950  ??_7Stream@Audio@@6B@
                    const Audio::Stream::`vftable'
##### Engine::DialogLoaderLoopT   exports=9 vtable=0x1801578d0
    0x18004d570  ??0DialogLoaderLoopT@Engine@@QEAA@AEBV01@@Z
                    public __thiscall Engine::DialogLoaderLoopT::DialogLoaderLoopT(class Engine::DialogLoaderLoopT const &)
    0x1800dad50  ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoaderLoopT::Load(void)
    0x1800dae20  ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z
                    public virtual void __thiscall Engine::DialogLoaderLoopT::Draw(float)
    0x1800daf20  ?Update@DialogLoaderLoopT@Engine@@UEAA_NAEBVTime@System@@@Z
                    public virtual bool __thiscall Engine::DialogLoaderLoopT::Update(class System::Time const &)
    0x1800dafc0  ?Shutdown@DialogLoaderLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoaderLoopT::Shutdown(void)
    0x1800db000  ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoaderLoopT::Startup(void)
    0x1800db060  ??1DialogLoaderLoopT@Engine@@UEAA@XZ
                    public virtual __thiscall Engine::DialogLoaderLoopT::~DialogLoaderLoopT(void)
    0x1800db0d0  ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
                    public __thiscall Engine::DialogLoaderLoopT::DialogLoaderLoopT(class System::String const &,class Engine::UpdateLoop *,class System::Time const &)
    0x1801578d0  ??_7DialogLoaderLoopT@Engine@@6B@
                    const Engine::DialogLoaderLoopT::`vftable'
##### Engine::DialogLoopT   exports=10 vtable=0x1801579a0
    0x18004d7c0  ?GetUserInterface@DialogLoopT@Engine@@QEAAAEAVUserInterface@UI@@XZ
                    public class UI::UserInterface & __thiscall Engine::DialogLoopT::GetUserInterface(void)
    0x18004d7d0  ??0DialogLoopT@Engine@@QEAA@AEBV01@@Z
                    public __thiscall Engine::DialogLoopT::DialogLoopT(class Engine::DialogLoopT const &)
    0x1800db140  ?Load@DialogLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoopT::Load(void)
    0x1800db210  ?Draw@DialogLoopT@Engine@@UEAAXM@Z
                    public virtual void __thiscall Engine::DialogLoopT::Draw(float)
    0x1800db270  ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z
                    public virtual bool __thiscall Engine::DialogLoopT::Update(class System::Time const &)
    0x1800db2c0  ?Shutdown@DialogLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoopT::Shutdown(void)
    0x1800db2f0  ?Startup@DialogLoopT@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::DialogLoopT::Startup(void)
    0x1800db340  ??1DialogLoopT@Engine@@UEAA@XZ
                    public virtual __thiscall Engine::DialogLoopT::~DialogLoopT(void)
    0x1800db3f0  ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z
                    public __thiscall Engine::DialogLoopT::DialogLoopT(class System::String const &)
    0x1801579a0  ??_7DialogLoopT@Engine@@6B@
                    const Engine::DialogLoopT::`vftable'
##### Engine::Engine   exports=30 (no vtable export)
    0x18000c7c0  ?Get@Engine@1@SAPEAV11@XZ
                    public static class Engine::Engine * __cdecl Engine::Engine::Get(void)
    0x18000c7d0  ??4Engine@0@QEAAAEAV00@AEBV00@@Z
                    public class Engine::Engine & __thiscall Engine::Engine::operator=(class Engine::Engine const &)
    0x18000c8b0  ?GetMemoryAllocator@Engine@1@QEAAPEAVMemoryAllocator@System@@XZ
                    public class System::MemoryAllocator * __thiscall Engine::Engine::GetMemoryAllocator(void)
    0x18000c8c0  ?GetFileSystem@Engine@1@QEAAPEAVFileSystem@IO@@XZ
                    public class IO::FileSystem * __thiscall Engine::Engine::GetFileSystem(void)
    0x18000c8d0  ?GetPersistence@Engine@1@QEAAPEAVPersistence@IO@@XZ
                    public class IO::Persistence * __thiscall Engine::Engine::GetPersistence(void)
    0x18000c8e0  ?GetModification@Engine@1@QEAAPEAVModification@IO@@XZ
                    public class IO::Modification * __thiscall Engine::Engine::GetModification(void)
    0x18000c8f0  ?GetConfiguration@Engine@1@QEAAPEAVConfiguration@System@@XZ
                    public class System::Configuration * __thiscall Engine::Engine::GetConfiguration(void)
    0x18000c900  ?GetApplication@Engine@1@QEAAPEAVApplication@2@XZ
                    public class Application::Application * __thiscall Engine::Engine::GetApplication(void)
    0x18000c910  ?GetInternalFactory@Engine@1@QEAAPEAVInternalFactory@Object@@XZ
                    public class Object::InternalFactory * __thiscall Engine::Engine::GetInternalFactory(void)
    0x18000c920  ?GetFactory@Engine@1@QEAAPEAVFactory@Object@@XZ
                    public class Object::Factory * __thiscall Engine::Engine::GetFactory(void)
    0x18000c930  ?GetExternalFactory@Engine@1@QEAAPEAVExternalFactory@Object@@XZ
                    public class Object::ExternalFactory * __thiscall Engine::Engine::GetExternalFactory(void)
    0x18000c940  ?GetVideoConfiguration@Engine@1@QEAAPEAVConfiguration@Video@@XZ
                    public class Video::Configuration * __thiscall Engine::Engine::GetVideoConfiguration(void)
    0x18000c950  ?GetVideoDevice@Engine@1@QEAAPEAVDevice@Video@@XZ
                    public class Video::Device * __thiscall Engine::Engine::GetVideoDevice(void)
    0x18000c960  ?GetAudioConfiguration@Engine@1@QEAAPEAVConfiguration@Audio@@XZ
                    public class Audio::Configuration * __thiscall Engine::Engine::GetAudioConfiguration(void)
    0x18000c970  ?GetAudioDevice@Engine@1@QEAAPEAVDevice@Audio@@XZ
                    public class Audio::Device * __thiscall Engine::Engine::GetAudioDevice(void)
    0x18000c980  ?GetInputConfiguration@Engine@1@QEAAPEAVConfiguration@Input@@XZ
                    public class Input::Configuration * __thiscall Engine::Engine::GetInputConfiguration(void)
    0x18000c990  ?GetInputDevice@Engine@1@QEAAPEAVDevice@Input@@XZ
                    public class Input::Device * __thiscall Engine::Engine::GetInputDevice(void)
    0x18000c9a0  ?GetAchievementManager@Engine@1@QEAAPEAVAchievementManager@Achievement@@XZ
                    public class Achievement::AchievementManager * __thiscall Engine::Engine::GetAchievementManager(void)
    0x1800a1670  ?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z
                    public bool __thiscall Engine::Engine::PlatformConfigure(bool,void *)
    0x1800a20e0  ?PlatformUpdate@Engine@1@QEAAXXZ
                    public void __thiscall Engine::Engine::PlatformUpdate(void)
    0x1800a20f0  ?PlatformShutdown@Engine@1@QEAAXXZ
                    public void __thiscall Engine::Engine::PlatformShutdown(void)
    0x1800a2100  ?PlatformStartup@Engine@1@QEAA_NXZ
                    public bool __thiscall Engine::Engine::PlatformStartup(void)
    0x1800db460  ?NotifyEndLoad@Engine@1@QEAAXXZ
                    public void __thiscall Engine::Engine::NotifyEndLoad(void)
    0x1800db480  ?NotifyBeginLoad@Engine@1@QEAAXXZ
                    public void __thiscall Engine::Engine::NotifyBeginLoad(void)
    0x1800db4a0  ?Update@Engine@1@QEAA_NAEBVTime@System@@@Z
                    public bool __thiscall Engine::Engine::Update(class System::Time const &)
    0x1800db4f0  ?Shutdown@Engine@1@QEAAXXZ
                    public void __thiscall Engine::Engine::Shutdown(void)
    0x1800db8a0  ?Startup@Engine@1@QEAAIPEBDII_N@Z
                    public unsigned int __thiscall Engine::Engine::Startup(char const *,unsigned int,unsigned int,bool)
    0x1800dbec0  ??1Engine@0@QEAA@XZ
                    public __thiscall Engine::Engine::~Engine(void)
    0x1800dbf00  ??0Engine@0@QEAA@PEAX@Z
                    public __thiscall Engine::Engine::Engine(void *)
    0x1801fe748  ?_singleton@Engine@1@0PEAV11@EA
                    private static class Engine::Engine * Engine::Engine::_singleton
##### Engine::Loader   exports=5 vtable=0x180157a08
    0x18004d330  ??1Loader@Engine@@QEAA@XZ
                    public __thiscall Engine::Loader::~Loader(void)
    0x1800dbfc0  ?Run@Loader@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::Loader::Run(void)
    0x1800dc050  ?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z
                    public void __thiscall Engine::Loader::Load(class Engine::UpdateLoop *)
    0x1800dc0f0  ??0Loader@Engine@@QEAA@XZ
                    public __thiscall Engine::Loader::Loader(void)
    0x180157a08  ??_7Loader@Engine@@6B@
                    const Engine::Loader::`vftable'
##### Engine::LoaderLoop   exports=14 vtable=0x180157940
    0x18004d3a0  ??0LoaderLoop@Engine@@QEAA@AEBV01@@Z
                    public __thiscall Engine::LoaderLoop::LoaderLoop(class Engine::LoaderLoop const &)
    0x18004d430  ??4LoaderLoop@Engine@@QEAAAEAV01@AEBV01@@Z
                    public class Engine::LoaderLoop & __thiscall Engine::LoaderLoop::operator=(class Engine::LoaderLoop const &)
    0x1800dc130  ?Exit@LoaderLoop@Engine@@QEAAXXZ
                    public void __thiscall Engine::LoaderLoop::Exit(void)
    0x1800dc150  ?IsFinished@LoaderLoop@Engine@@QEBA_NXZ
                    public bool __thiscall Engine::LoaderLoop::IsFinished(void) const
    0x1800dc170  ??1LoaderLoop@Engine@@UEAA@XZ
                    public virtual __thiscall Engine::LoaderLoop::~LoaderLoop(void)
    0x1800dc1e0  ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z
                    public __thiscall Engine::LoaderLoop::LoaderLoop(class Engine::UpdateLoop *,class System::Time const &)
    0x1800dc260  ?Startup@LoaderLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::LoaderLoop::Startup(void)
    0x1800dc280  ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::LoaderLoop::Shutdown(void)
    0x1800dc2a0  ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
                    public virtual bool __thiscall Engine::LoaderLoop::Update(class System::Time const &)
    0x1800dc310  ?Draw@LoaderLoop@Engine@@UEAAXM@Z
                    public virtual void __thiscall Engine::LoaderLoop::Draw(float)
    0x1800dc350  ?Load@LoaderLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::LoaderLoop::Load(void)
    0x1800dc360  ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::LoaderLoop::OnVideoError(void)
    0x180157940  ??_7LoaderLoop@Engine@@6B@
                    const Engine::LoaderLoop::`vftable'
    0x1801fe7b8  ?_loader@LoaderLoop@Engine@@1VLoader@2@A
                    protected static class Engine::Loader Engine::LoaderLoop::_loader
##### Engine::MainLoop   exports=32 (no vtable export)
    0x1800313d0  ??2MainLoop@Engine@@SAPEAX_K@Z
                    public static void * __cdecl Engine::MainLoop::operator new(unsigned __int64)
    0x180031440  ??2MainLoop@Engine@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Engine::MainLoop::operator new(unsigned __int64,void *)
    0x180031450  ??3MainLoop@Engine@@SAXPEAX@Z
                    public static void __cdecl Engine::MainLoop::operator delete(void *)
    0x1800314b0  ?IsRunning@MainLoop@Engine@@QEBA_NPEBVUpdateLoop@2@@Z
                    public bool __thiscall Engine::MainLoop::IsRunning(class Engine::UpdateLoop const *) const
    0x1800314d0  ?GetContext@MainLoop@Engine@@QEAAAEAVContext@Video@@XZ
                    public class Video::Context & __thiscall Engine::MainLoop::GetContext(void)
    0x1800314e0  ?GetCurrentLoop@MainLoop@Engine@@QEAAPEAVUpdateLoop@2@XZ
                    public class Engine::UpdateLoop * __thiscall Engine::MainLoop::GetCurrentLoop(void)
    0x1800314f0  ?GetDeltaTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ
                    public class System::Time __thiscall Engine::MainLoop::GetDeltaTime(void) const
    0x180031500  ?GetFrameTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ
                    public class System::Time __thiscall Engine::MainLoop::GetFrameTime(void) const
    0x180031510  ?GetFrameStartTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ
                    public class System::Time __thiscall Engine::MainLoop::GetFrameStartTime(void) const
    0x180031520  ?GetFrameUpdateTime@MainLoop@Engine@@QEBA?AVTime@System@@XZ
                    public class System::Time __thiscall Engine::MainLoop::GetFrameUpdateTime(void) const
    0x180031530  ?MarkFrameUpdateTime@MainLoop@Engine@@QEAAXXZ
                    public void __thiscall Engine::MainLoop::MarkFrameUpdateTime(void)
    0x180031570  ?Get@MainLoop@Engine@@SAPEAV12@XZ
                    public static class Engine::MainLoop * __cdecl Engine::MainLoop::Get(void)
    0x180031580  ??0MainLoop@Engine@@QEAA@AEBV01@@Z
                    public __thiscall Engine::MainLoop::MainLoop(class Engine::MainLoop const &)
    0x180031680  ??4MainLoop@Engine@@QEAAAEAV01@AEBV01@@Z
                    public class Engine::MainLoop & __thiscall Engine::MainLoop::operator=(class Engine::MainLoop const &)
    0x1800dc3b0  ?OnVideoRestart@MainLoop@Engine@@QEAAXXZ
                    public void __thiscall Engine::MainLoop::OnVideoRestart(void)
    0x1800dc5c0  ?OnVideoRestart_@MainLoop@Engine@@SAXXZ
                    public static void __cdecl Engine::MainLoop::OnVideoRestart_(void)
    0x1800dc5e0  ?OnVideoError@MainLoop@Engine@@QEAAXXZ
                    public void __thiscall Engine::MainLoop::OnVideoError(void)
    0x1800dcd20  ?OnVideoError_@MainLoop@Engine@@SAXXZ
                    public static void __cdecl Engine::MainLoop::OnVideoError_(void)
    0x1800dcd40  ?Draw@MainLoop@Engine@@QEAAXM@Z
                    public void __thiscall Engine::MainLoop::Draw(float)
    0x1800dceb0  ?RemovePostUpdate@MainLoop@Engine@@QEAAXP6AXAEBVTime@System@@PEAX@Z1@Z
                    public void __thiscall Engine::MainLoop::RemovePostUpdate(void (__cdecl *)(class System::Time const &,void *),void *)
    0x1800dcf60  ?AddPostUpdate@MainLoop@Engine@@QEAAXP6AXAEBVTime@System@@PEAX@Z1@Z
                    public void __thiscall Engine::MainLoop::AddPostUpdate(void (__cdecl *)(class System::Time const &,void *),void *)
    0x1800dcf90  ?RemovePostDraw@MainLoop@Engine@@QEAAXP6AXAEAVContext@Video@@PEAX@Z1@Z
                    public void __thiscall Engine::MainLoop::RemovePostDraw(void (__cdecl *)(class Video::Context &,void *),void *)
    0x1800dd040  ?AddPostDraw@MainLoop@Engine@@QEAAXP6AXAEAVContext@Video@@PEAX@Z1@Z
                    public void __thiscall Engine::MainLoop::AddPostDraw(void (__cdecl *)(class Video::Context &,void *),void *)
    0x1800dd070  ?Update@MainLoop@Engine@@QEAA_NAEBVTime@System@@@Z
                    public bool __thiscall Engine::MainLoop::Update(class System::Time const &)
    0x1800dd100  ?Frame@MainLoop@Engine@@QEAA_N_N@Z
                    public bool __thiscall Engine::MainLoop::Frame(bool)
    0x1800dd3c0  ?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z
                    public void __thiscall Engine::MainLoop::Run(class Engine::UpdateLoop *)
    0x1800dd500  ?CreateVideoContext@MainLoop@Engine@@AEAAXXZ
                    private void __thiscall Engine::MainLoop::CreateVideoContext(void)
    0x1800dd5e0  ?ReleaseVideoContext@MainLoop@Engine@@AEAAXXZ
                    private void __thiscall Engine::MainLoop::ReleaseVideoContext(void)
    0x1800dd640  ??1MainLoop@Engine@@QEAA@XZ
                    public __thiscall Engine::MainLoop::~MainLoop(void)
    0x1800dd6b0  ??0MainLoop@Engine@@QEAA@PEAV11@@Z
                    public __thiscall Engine::MainLoop::MainLoop(class Engine::Engine *)
    0x1800dd7a0  ?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z
                    public void __thiscall Engine::MainLoop::SetNextLoop(class Engine::UpdateLoop *)
    0x1801fe750  ?_singleton@MainLoop@Engine@@1PEAV12@EA
                    protected static class Engine::MainLoop * Engine::MainLoop::_singleton
##### Engine::UpdateLoop   exports=16 vtable=0x180155ba0
    0x180031740  ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::UpdateLoop::DrawFailed(void)
    0x180031750  ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
                    public virtual void __thiscall Engine::UpdateLoop::SaveRestore(bool,class Save::SavedState *)
    0x180031760  ?OnVideoError@UpdateLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::UpdateLoop::OnVideoError(void)
    0x180031770  ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
                    public virtual void __thiscall Engine::UpdateLoop::OnVideoRestart(void)
    0x180031780  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
                    public virtual char const * __thiscall Engine::UpdateLoop::GetResourceFilename(void)
    0x180031790  ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
                    public virtual bool __thiscall Engine::UpdateLoop::AllowsModChanges(void)
    0x1800317a0  ?SetLoadProgress@UpdateLoop@Engine@@QEAAXH@Z
                    public void __thiscall Engine::UpdateLoop::SetLoadProgress(int)
    0x1800317b0  ?GetLoadProgress@UpdateLoop@Engine@@QEBAHXZ
                    public int __thiscall Engine::UpdateLoop::GetLoadProgress(void) const
    0x1800317c0  ?GetLoadBit@UpdateLoop@Engine@@QEBAHXZ
                    public int __thiscall Engine::UpdateLoop::GetLoadBit(void) const
    0x1800317d0  ??0UpdateLoop@Engine@@QEAA@AEBV01@@Z
                    public __thiscall Engine::UpdateLoop::UpdateLoop(class Engine::UpdateLoop const &)
    0x180031840  ??4UpdateLoop@Engine@@QEAAAEAV01@AEBV01@@Z
                    public class Engine::UpdateLoop & __thiscall Engine::UpdateLoop::operator=(class Engine::UpdateLoop const &)
    0x1800dd7b0  ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ
                    public bool __thiscall Engine::UpdateLoop::IsRunning(void) const
    0x1800dd7d0  ?LoadResources@UpdateLoop@Engine@@QEAAXXZ
                    public void __thiscall Engine::UpdateLoop::LoadResources(void)
    0x1800ddd30  ??0UpdateLoop@Engine@@QEAA@XZ
                    public __thiscall Engine::UpdateLoop::UpdateLoop(void)
    0x1800dddb0  ??1UpdateLoop@Engine@@UEAA@XZ
                    public virtual __thiscall Engine::UpdateLoop::~UpdateLoop(void)
    0x180155ba0  ??_7UpdateLoop@Engine@@6B@
                    const Engine::UpdateLoop::`vftable'
##### Input::Configuration   exports=16 (no vtable export)
    0x18000d800  ?GetKeyMap@Configuration@Input@@QEAAPEAVKeyMap@2@XZ
                    public class Input::KeyMap * __thiscall Input::Configuration::GetKeyMap(void)
    0x18000d810  ?GetMouseClipping@Configuration@Input@@QEBA_NXZ
                    public bool __thiscall Input::Configuration::GetMouseClipping(void) const
    0x18000d820  ?IsLegacyEnabled@Configuration@Input@@QEBA_NXZ
                    public bool __thiscall Input::Configuration::IsLegacyEnabled(void) const
    0x18000d830  ?GetMouseDisplay@Configuration@Input@@QEBA_NXZ
                    public bool __thiscall Input::Configuration::GetMouseDisplay(void) const
    0x18000d840  ?SetMouseClipping@Configuration@Input@@QEAAX_N@Z
                    public void __thiscall Input::Configuration::SetMouseClipping(bool)
    0x18000d850  ?SetLegacy@Configuration@Input@@QEAAX_N@Z
                    public void __thiscall Input::Configuration::SetLegacy(bool)
    0x18000d860  ?SetMouseDisplay@Configuration@Input@@QEAAX_N@Z
                    public void __thiscall Input::Configuration::SetMouseDisplay(bool)
    0x18000d870  ??4Configuration@Input@@QEAAAEAV01@AEBV01@@Z
                    public class Input::Configuration & __thiscall Input::Configuration::operator=(class Input::Configuration const &)
    0x18000d890  ?Get@Configuration@Input@@SAPEAV12@XZ
                    public static class Input::Configuration * __cdecl Input::Configuration::Get(void)
    0x1800e0c00  ?Save@Configuration@Input@@QEAAXXZ
                    public void __thiscall Input::Configuration::Save(void)
    0x1800e0c80  ?Load@Configuration@Input@@QEAAXXZ
                    public void __thiscall Input::Configuration::Load(void)
    0x1800e0d00  ?Shutdown@Configuration@Input@@QEAAXXZ
                    public void __thiscall Input::Configuration::Shutdown(void)
    0x1800e0d30  ?Startup@Configuration@Input@@QEAA_NXZ
                    public bool __thiscall Input::Configuration::Startup(void)
    0x1800e0de0  ??0Configuration@Input@@QEAA@XZ
                    public __thiscall Input::Configuration::Configuration(void)
    0x1800e0e10  ??1Configuration@Input@@QEAA@XZ
                    public __thiscall Input::Configuration::~Configuration(void)
    0x1801fe768  ?_singleton@Configuration@Input@@0PEAV12@EA
                    private static class Input::Configuration * Input::Configuration::_singleton
##### Input::Device   exports=31 (no vtable export)
    0x180022ba0  ??4Device@Input@@QEAAAEAV01@AEBV01@@Z
                    public class Input::Device & __thiscall Input::Device::operator=(class Input::Device const &)
    0x180022c40  ?Get@Device@Input@@SAPEAV12@XZ
                    public static class Input::Device * __cdecl Input::Device::Get(void)
    0x1800e0e20  ?PressedTime@Device@Input@@QEBA?AVTime@System@@W4InputDevice@12@HH@Z
                    public class System::Time __thiscall Input::Device::PressedTime(enum Input::Device::InputDevice,int,int) const
    0x1800e0ea0  ?GetState@Device@Input@@AEBAPEBUState@12@W4InputDevice@12@HAEAH@Z
                    private struct Input::Device::State const * __thiscall Input::Device::GetState(enum Input::Device::InputDevice,int,int &) const
    0x1800e0ef0  ?ResetState@Device@Input@@QEAAXXZ
                    public void __thiscall Input::Device::ResetState(void)
    0x1800e0fc0  ?UpdateState@Device@Input@@AEAAXPEAUState@12@0HAEBVTime@System@@@Z
                    private void __thiscall Input::Device::UpdateState(struct Input::Device::State *,struct Input::Device::State *,int,class System::Time const &)
    0x1800e13b0  ??0Device@Input@@QEAA@XZ
                    public __thiscall Input::Device::Device(void)
    0x1800e1460  ?Startup@Device@Input@@QEAA_NXZ
                    public bool __thiscall Input::Device::Startup(void)
    0x1800e1470  ?Shutdown@Device@Input@@QEAAXXZ
                    public void __thiscall Input::Device::Shutdown(void)
    0x1800e1480  ??1Device@Input@@QEAA@XZ
                    public __thiscall Input::Device::~Device(void)
    0x1800e1490  ?Update@Device@Input@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Input::Device::Update(class System::Time const &)
    0x1800e15a0  ?PressedOrRepeated@Device@Input@@QEBA_NW4InputDevice@12@HH@Z
                    public bool __thiscall Input::Device::PressedOrRepeated(enum Input::Device::InputDevice,int,int) const
    0x1800e1600  ?GetNormalizedValue@Device@Input@@QEBAMW4InputDevice@12@HH@Z
                    public float __thiscall Input::Device::GetNormalizedValue(enum Input::Device::InputDevice,int,int) const
    0x1800e1660  ?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z
                    public short __thiscall Input::Device::GetValue(enum Input::Device::InputDevice,int,int) const
    0x1800e16c0  ?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z
                    public bool __thiscall Input::Device::Pressed(enum Input::Device::InputDevice,int,int) const
    0x1800e1720  ?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z
                    public bool __thiscall Input::Device::Released(enum Input::Device::InputDevice,int,int) const
    0x1800e1780  ?EnableMouseClipping@Device@Input@@QEAAX_N@Z
                    public void __thiscall Input::Device::EnableMouseClipping(bool)
    0x1800e17a0  ?OnMouseWheelMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z
                    private static int __cdecl Input::Device::OnMouseWheelMessage(struct HWND__ *,unsigned int,unsigned __int64,__int64)
    0x1800e17d0  ?OnLostFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z
                    private static int __cdecl Input::Device::OnLostFocusMessage(struct HWND__ *,unsigned int,unsigned __int64,__int64)
    0x1800e1800  ?OnFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z
                    private static int __cdecl Input::Device::OnFocusMessage(struct HWND__ *,unsigned int,unsigned __int64,__int64)
    0x1800e1830  ?PlatformConstruct@Device@Input@@AEAAXXZ
                    private void __thiscall Input::Device::PlatformConstruct(void)
    0x1800e1870  ?PlatformDestruct@Device@Input@@AEAAXXZ
                    private void __thiscall Input::Device::PlatformDestruct(void)
    0x1800e1880  ?PlatformStartup@Device@Input@@AEAA_NXZ
                    private bool __thiscall Input::Device::PlatformStartup(void)
    0x1800e1b50  ?PlatformShutdown@Device@Input@@AEAAXXZ
                    private void __thiscall Input::Device::PlatformShutdown(void)
    0x1800e1c60  ?PlatformGetKeyboardState@Device@Input@@AEAA_NPEAUState@12@@Z
                    private bool __thiscall Input::Device::PlatformGetKeyboardState(struct Input::Device::State *)
    0x1800e1e10  ?PlatformGetMouseState@Device@Input@@AEAA_NPEAUState@12@@Z
                    private bool __thiscall Input::Device::PlatformGetMouseState(struct Input::Device::State *)
    0x1800e2120  ?PlatformGetGamepadState@Device@Input@@AEAA_NHPEAUState@12@@Z
                    private bool __thiscall Input::Device::PlatformGetGamepadState(int,struct Input::Device::State *)
    0x1800e2130  ?PlatformUpdate@Device@Input@@AEAAXXZ
                    private void __thiscall Input::Device::PlatformUpdate(void)
    0x1801f9480  ?_lkeyMap@Device@Input@@0PAEA
                    private static unsigned char * Input::Device::_lkeyMap
    0x1801f94f0  ?_keyMap@Device@Input@@0PAEA
                    private static unsigned char * Input::Device::_keyMap
    0x1801fe770  ?_singleton@Device@Input@@0PEAV12@EA
                    private static class Input::Device * Input::Device::_singleton
##### Input::KeyMap   exports=20 (no vtable export)
    0x180022970  ??1KeyMap@Input@@QEAA@XZ
                    public __thiscall Input::KeyMap::~KeyMap(void)
    0x1800229d0  ??0KeyMap@Input@@QEAA@AEBV01@@Z
                    public __thiscall Input::KeyMap::KeyMap(class Input::KeyMap const &)
    0x180022a40  ??4KeyMap@Input@@QEAAAEAV01@AEBV01@@Z
                    public class Input::KeyMap & __thiscall Input::KeyMap::operator=(class Input::KeyMap const &)
    0x1800a7190  ?Load@KeyMap@Input@@QEAAXXZ
                    public void __thiscall Input::KeyMap::Load(void)
    0x1800a7520  ?Save@KeyMap@Input@@QEAAXXZ
                    public void __thiscall Input::KeyMap::Save(void)
    0x1800a76c0  ?GetIdList@KeyMap@Input@@QEBAXAEAV?$Array@H@Collection@@@Z
                    public void __thiscall Input::KeyMap::GetIdList(class Collection::Array<int> &) const
    0x1800a7890  ?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z
                    public int __thiscall Input::KeyMap::GetIdForDeviceInput(int,int) const
    0x1800a7930  ?GetInputForId@KeyMap@Input@@QEBAHHH@Z
                    public int __thiscall Input::KeyMap::GetInputForId(int,int) const
    0x1800a7970  ?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z
                    public int __thiscall Input::KeyMap::GetDeviceForId(int,int) const
    0x1800a79b0  ?Released@KeyMap@Input@@QEBA_NHH@Z
                    public bool __thiscall Input::KeyMap::Released(int,int) const
    0x1800a7a80  ?Pressed@KeyMap@Input@@QEBA_NHH@Z
                    public bool __thiscall Input::KeyMap::Pressed(int,int) const
    0x1800a7b60  ?GetValue@KeyMap@Input@@QEBAFHH@Z
                    public short __thiscall Input::KeyMap::GetValue(int,int) const
    0x1800a7c50  ?GetNormalizedValue@KeyMap@Input@@QEBAMHH@Z
                    public float __thiscall Input::KeyMap::GetNormalizedValue(int,int) const
    0x1800a7d20  ?ClearAll@KeyMap@Input@@QEAAXXZ
                    public void __thiscall Input::KeyMap::ClearAll(void)
    0x1800a7d70  ?ClearInput@KeyMap@Input@@QEAAXHH@Z
                    public void __thiscall Input::KeyMap::ClearInput(int,int)
    0x1800a7db0  ?ClearInput@KeyMap@Input@@QEAAXHHH@Z
                    public void __thiscall Input::KeyMap::ClearInput(int,int,int)
    0x1800a7e20  ?SetInput@KeyMap@Input@@QEAAXHHHH@Z
                    public void __thiscall Input::KeyMap::SetInput(int,int,int,int)
    0x1800a7e70  ?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z
                    public void __thiscall Input::KeyMap::RegisterInput(int,int,int,char const *,bool)
    0x1800a7f90  ?Suspend@KeyMap@Input@@QEAAH_N@Z
                    public int __thiscall Input::KeyMap::Suspend(bool)
    0x1800a7fb0  ??0KeyMap@Input@@QEAA@XZ
                    public __thiscall Input::KeyMap::KeyMap(void)
##### Pathfinding::IPathInfo   exports=4 vtable=0x180158f30
    0x18006c970  ??0IPathInfo@Pathfinding@@QEAA@XZ
                    public __thiscall Pathfinding::IPathInfo::IPathInfo(void)
    0x18006c980  ??0IPathInfo@Pathfinding@@QEAA@AEBV01@@Z
                    public __thiscall Pathfinding::IPathInfo::IPathInfo(class Pathfinding::IPathInfo const &)
    0x18006c990  ??4IPathInfo@Pathfinding@@QEAAAEAV01@AEBV01@@Z
                    public class Pathfinding::IPathInfo & __thiscall Pathfinding::IPathInfo::operator=(class Pathfinding::IPathInfo const &)
    0x180158f30  ??_7IPathInfo@Pathfinding@@6B@
                    const Pathfinding::IPathInfo::`vftable'
##### Pathfinding::PointList   exports=25 vtable=0x180158f50
    0x180070e00  ?_TypeName@PointList@Pathfinding@@CAPEBDXZ
                    private static char const * __cdecl Pathfinding::PointList::_TypeName(void)
    0x180070e10  ?_ParentTypeName@PointList@Pathfinding@@CAPEBDXZ
                    private static char const * __cdecl Pathfinding::PointList::_ParentTypeName(void)
    0x180070e20  ?Type@PointList@Pathfinding@@UEBAIXZ
                    public virtual unsigned int __thiscall Pathfinding::PointList::Type(void) const
    0x180070e30  ?TypeId@PointList@Pathfinding@@SAIXZ
                    public static unsigned int __cdecl Pathfinding::PointList::TypeId(void)
    0x180070e40  ?TypeName@PointList@Pathfinding@@UEBAPEBDXZ
                    public virtual char const * __thiscall Pathfinding::PointList::TypeName(void) const
    0x180070e50  ??2PointList@Pathfinding@@SAPEAX_K@Z
                    public static void * __cdecl Pathfinding::PointList::operator new(unsigned __int64)
    0x180070ec0  ??2PointList@Pathfinding@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Pathfinding::PointList::operator new(unsigned __int64,void *)
    0x180070ed0  ??3PointList@Pathfinding@@SAXPEAX@Z
                    public static void __cdecl Pathfinding::PointList::operator delete(void *)
    0x180070f30  ?CreateNewObject@PointList@Pathfinding@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Pathfinding::PointList::CreateNewObject(void)
    0x180070ff0  ?CreateInPlaceObject@PointList@Pathfinding@@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Pathfinding::PointList::CreateInPlaceObject(void *)
    0x180071060  ?Reconstruct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Pathfinding::PointList::Reconstruct(void)
    0x1800710e0  ?Construct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Pathfinding::PointList::Construct(void)
    0x180071150  ?Destruct@PointList@Pathfinding@@UEAAXXZ
                    public virtual void __thiscall Pathfinding::PointList::Destruct(void)
    0x180071190  ?Points@PointList@Pathfinding@@QEBAAEBV?$Array@UPoint@PointList@Pathfinding@@@Collection@@XZ
                    public class Collection::Array<struct Pathfinding::PointList::Point> const & __thiscall Pathfinding::PointList::Points(void) const
    0x1800711a0  ?PathPoints@PointList@Pathfinding@@QEBAAEBV?$Array@UPoint@PointList@Pathfinding@@@Collection@@XZ
                    public class Collection::Array<struct Pathfinding::PointList::Point> const & __thiscall Pathfinding::PointList::PathPoints(void) const
    0x1800711b0  ?GetMeshName@PointList@Pathfinding@@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Pathfinding::PointList::GetMeshName(void) const
    0x1800711f0  ??0PointList@Pathfinding@@QEAA@AEBV01@@Z
                    public __thiscall Pathfinding::PointList::PointList(class Pathfinding::PointList const &)
    0x1800712c0  ??4PointList@Pathfinding@@QEAAAEAV01@AEBV01@@Z
                    public class Pathfinding::PointList & __thiscall Pathfinding::PointList::operator=(class Pathfinding::PointList const &)
    0x18010f6d0  ?Compile@PointList@Pathfinding@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
                    public virtual enum Object::External::CompileResult __thiscall Pathfinding::PointList::Compile(class System::Filename const &)
    0x18010f6e0  ?Serialize@PointList@Pathfinding@@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Pathfinding::PointList::Serialize(class IO::Stream &)
    0x18010f880  ?GetFileVersion@PointList@Pathfinding@@UEBAHXZ
                    public virtual int __thiscall Pathfinding::PointList::GetFileVersion(void) const
    0x18010f8e0  ??1PointList@Pathfinding@@UEAA@XZ
                    public virtual __thiscall Pathfinding::PointList::~PointList(void)
    0x18010f980  ??0PointList@Pathfinding@@QEAA@XZ
                    public __thiscall Pathfinding::PointList::PointList(void)
    0x180158f50  ??_7PointList@Pathfinding@@6B@
                    const Pathfinding::PointList::`vftable'
    0x1801fa858  ?_typeInfo@PointList@Pathfinding@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Pathfinding::PointList::_typeInfo
##### Pathfinding::Scene   exports=19 (no vtable export)
    0x18006ca50  ?GetNodes@Scene@Pathfinding@@QEBAAEBV?$HashGrid@UNode@Scene@Pathfinding@@@Utility@@XZ
                    public class Utility::HashGrid<struct Pathfinding::Scene::Node> const & __thiscall Pathfinding::Scene::GetNodes(void) const
    0x18006ca60  ?GetLinks@Scene@Pathfinding@@QEBAAEBV?$Array@ULink@Scene@Pathfinding@@@Collection@@XZ
                    public class Collection::Array<struct Pathfinding::Scene::Link> const & __thiscall Pathfinding::Scene::GetLinks(void) const
    0x18006ca70  ??0Scene@Pathfinding@@QEAA@AEBV01@@Z
                    public __thiscall Pathfinding::Scene::Scene(class Pathfinding::Scene const &)
    0x18006cb00  ??4Scene@Pathfinding@@QEAAAEAV01@AEBV01@@Z
                    public class Pathfinding::Scene & __thiscall Pathfinding::Scene::operator=(class Pathfinding::Scene const &)
    0x18010cb70  ?GeneratePath@Scene@Pathfinding@@AEBA_NHHIAEAV?$Array@H@Collection@@@Z
                    private bool __thiscall Pathfinding::Scene::GeneratePath(int,int,unsigned int,class Collection::Array<int> &) const
    0x18010dcf0  ?GeneratePath@Scene@Pathfinding@@QEBA_NAEBVVector2@Math@@0MIAEAV?$Array@VVector2@Math@@@Collection@@@Z
                    public bool __thiscall Pathfinding::Scene::GeneratePath(class Math::Vector2 const &,class Math::Vector2 const &,float,unsigned int,class Collection::Array<class Math::Vector2> &) const
    0x18010e140  ?SmoothPath@Scene@Pathfinding@@QEAAXAEBV?$Array@VVector2@Math@@@Collection@@AEAV34@M@Z
                    public void __thiscall Pathfinding::Scene::SmoothPath(class Collection::Array<class Math::Vector2> const &,class Collection::Array<class Math::Vector2> &,float)
    0x18010e6f0  ?IsPassable@Scene@Pathfinding@@AEBA_NAEBVVector2@Math@@0@Z
                    private bool __thiscall Pathfinding::Scene::IsPassable(class Math::Vector2 const &,class Math::Vector2 const &) const
    0x18010e720  ?FindClosestNode@Scene@Pathfinding@@AEBAHAEBVVector2@Math@@@Z
                    private int __thiscall Pathfinding::Scene::FindClosestNode(class Math::Vector2 const &) const
    0x18010e980  ?BuildNode@Scene@Pathfinding@@AEAAXH@Z
                    private void __thiscall Pathfinding::Scene::BuildNode(int)
    0x18010ece0  ?Build@Scene@Pathfinding@@QEAAXXZ
                    public void __thiscall Pathfinding::Scene::Build(void)
    0x18010ecf0  ?AddLink@Scene@Pathfinding@@AEAAXHHM@Z
                    private void __thiscall Pathfinding::Scene::AddLink(int,int,float)
    0x18010eea0  ?HasLink@Scene@Pathfinding@@AEBA_NHH@Z
                    private bool __thiscall Pathfinding::Scene::HasLink(int,int) const
    0x18010ef00  ?RemoveLinks@Scene@Pathfinding@@AEAAXH@Z
                    private void __thiscall Pathfinding::Scene::RemoveLinks(int)
    0x18010efd0  ?RemoveLinkFrom@Scene@Pathfinding@@AEAAXHH@Z
                    private void __thiscall Pathfinding::Scene::RemoveLinkFrom(int,int)
    0x18010f0f0  ?RemoveNode@Scene@Pathfinding@@QEAAXAEBVVector2@Math@@@Z
                    public void __thiscall Pathfinding::Scene::RemoveNode(class Math::Vector2 const &)
    0x18010f4e0  ?AddNode@Scene@Pathfinding@@QEAAXAEBVVector2@Math@@II@Z
                    public void __thiscall Pathfinding::Scene::AddNode(class Math::Vector2 const &,unsigned int,unsigned int)
    0x18010f5f0  ??1Scene@Pathfinding@@QEAA@XZ
                    public __thiscall Pathfinding::Scene::~Scene(void)
    0x18010f660  ??0Scene@Pathfinding@@QEAA@PEBVIPathInfo@1@M@Z
                    public __thiscall Pathfinding::Scene::Scene(class Pathfinding::IPathInfo const *,float)
##### Picking::PickingMesh   exports=29 vtable=0x180158fb0
    0x180071a70  ?_TypeName@PickingMesh@Picking@@CAPEBDXZ
                    private static char const * __cdecl Picking::PickingMesh::_TypeName(void)
    0x180071a80  ?_ParentTypeName@PickingMesh@Picking@@CAPEBDXZ
                    private static char const * __cdecl Picking::PickingMesh::_ParentTypeName(void)
    0x180071a90  ?Type@PickingMesh@Picking@@UEBAIXZ
                    public virtual unsigned int __thiscall Picking::PickingMesh::Type(void) const
    0x180071aa0  ?TypeId@PickingMesh@Picking@@SAIXZ
                    public static unsigned int __cdecl Picking::PickingMesh::TypeId(void)
    0x180071ab0  ?TypeName@PickingMesh@Picking@@UEBAPEBDXZ
                    public virtual char const * __thiscall Picking::PickingMesh::TypeName(void) const
    0x180071ac0  ??2PickingMesh@Picking@@SAPEAX_K@Z
                    public static void * __cdecl Picking::PickingMesh::operator new(unsigned __int64)
    0x180071b30  ??2PickingMesh@Picking@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Picking::PickingMesh::operator new(unsigned __int64,void *)
    0x180071b40  ??3PickingMesh@Picking@@SAXPEAX@Z
                    public static void __cdecl Picking::PickingMesh::operator delete(void *)
    0x180071ba0  ?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ
                    public static class Object::Base * __cdecl Picking::PickingMesh::CreateNewObject(void)
    0x180071c60  ?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z
                    public static class Object::Base * __cdecl Picking::PickingMesh::CreateInPlaceObject(void *)
    0x180071cd0  ?Reconstruct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Picking::PickingMesh::Reconstruct(void)
    0x180071d40  ?Construct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ
                    public virtual class Object::External * __thiscall Picking::PickingMesh::Construct(void)
    0x180071db0  ?Destruct@PickingMesh@Picking@@UEAAXXZ
                    public virtual void __thiscall Picking::PickingMesh::Destruct(void)
    0x180071dc0  ?GetSubMeshCount@PickingMesh@Picking@@QEBAHXZ
                    public int __thiscall Picking::PickingMesh::GetSubMeshCount(void) const
    0x180071dd0  ?AABB@PickingMesh@Picking@@QEBA?AVBox@Volume@@H@Z
                    public class Volume::Box __thiscall Picking::PickingMesh::AABB(int) const
    0x180071e10  ?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z
                    public class Collection::Array<class Math::Vector3> const & __thiscall Picking::PickingMesh::Vertices(int) const
    0x180071e30  ?Normals@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z
                    public class Collection::Array<class Math::Vector3> const & __thiscall Picking::PickingMesh::Normals(int) const
    0x180071e50  ?Triangles@PickingMesh@Picking@@QEBAAEBV?$Array@UTriangle@PickingMesh@Picking@@@Collection@@H@Z
                    public class Collection::Array<struct Picking::PickingMesh::Triangle> const & __thiscall Picking::PickingMesh::Triangles(int) const
    0x180071e70  ?TriangleCount@PickingMesh@Picking@@QEBAHH@Z
                    public int __thiscall Picking::PickingMesh::TriangleCount(int) const
    0x180071e90  ?GetNode@PickingMesh@Picking@@QEBAAEBUSubMesh@12@H@Z
                    public struct Picking::PickingMesh::SubMesh const & __thiscall Picking::PickingMesh::GetNode(int) const
    0x180071ea0  ??0PickingMesh@Picking@@QEAA@AEBV01@@Z
                    public __thiscall Picking::PickingMesh::PickingMesh(class Picking::PickingMesh const &)
    0x180071f60  ??4PickingMesh@Picking@@QEAAAEAV01@AEBV01@@Z
                    public class Picking::PickingMesh & __thiscall Picking::PickingMesh::operator=(class Picking::PickingMesh const &)
    0x180122830  ??0PickingMesh@Picking@@QEAA@XZ
                    public __thiscall Picking::PickingMesh::PickingMesh(void)
    0x1801228a0  ??1PickingMesh@Picking@@UEAA@XZ
                    public virtual __thiscall Picking::PickingMesh::~PickingMesh(void)
    0x180122940  ?GetFileVersion@PickingMesh@Picking@@UEBAHXZ
                    public virtual int __thiscall Picking::PickingMesh::GetFileVersion(void) const
    0x180122950  ?Compile@PickingMesh@Picking@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
                    public virtual enum Object::External::CompileResult __thiscall Picking::PickingMesh::Compile(class System::Filename const &)
    0x180122960  ?Serialize@PickingMesh@Picking@@UEAA_NAEAVStream@IO@@@Z
                    public virtual bool __thiscall Picking::PickingMesh::Serialize(class IO::Stream &)
    0x180158fb0  ??_7PickingMesh@Picking@@6B@
                    const Picking::PickingMesh::`vftable'
    0x1801faf20  ?_typeInfo@PickingMesh@Picking@@0VBaseTypeInfo@Object@@A
                    private static class Object::BaseTypeInfo Picking::PickingMesh::_typeInfo
##### Picking::Scene   exports=22 (no vtable export)
    0x1800720d0  ??2Scene@Picking@@SAPEAX_K@Z
                    public static void * __cdecl Picking::Scene::operator new(unsigned __int64)
    0x180072140  ??2Scene@Picking@@SAPEAX_KPEAX@Z
                    public static void * __cdecl Picking::Scene::operator new(unsigned __int64,void *)
    0x180072150  ??3Scene@Picking@@SAXPEAX@Z
                    public static void __cdecl Picking::Scene::operator delete(void *)
    0x1800721b0  ?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z
                    public void __thiscall Picking::Scene::SetTransform(unsigned int,class Math::Transform const &)
    0x180072200  ?GetMesh@Scene@Picking@@QEBA?AV?$ExternalPtr@VPickingMesh@Picking@@@Object@@H@Z
                    public class Object::ExternalPtr<class Picking::PickingMesh> __thiscall Picking::Scene::GetMesh(int) const
    0x180072220  ?GetAABB@Scene@Picking@@QEBA?AVBox@Volume@@H@Z
                    public class Volume::Box __thiscall Picking::Scene::GetAABB(int) const
    0x180072250  ?GetLocalAABB@Scene@Picking@@QEBA?AVBox@Volume@@H@Z
                    public class Volume::Box __thiscall Picking::Scene::GetLocalAABB(int) const
    0x180072280  ?GetTransform@Scene@Picking@@QEBA?AVTransform@Math@@HH@Z
                    public class Math::Transform __thiscall Picking::Scene::GetTransform(int,int) const
    0x1800722d0  ?GetUserData@Scene@Picking@@QEBAPEAXI@Z
                    public void * __thiscall Picking::Scene::GetUserData(unsigned int) const
    0x1800722e0  ?GetUserValue@Scene@Picking@@QEBAHI@Z
                    public int __thiscall Picking::Scene::GetUserValue(unsigned int) const
    0x1800722f0  ??0Scene@Picking@@QEAA@AEBV01@@Z
                    public __thiscall Picking::Scene::Scene(class Picking::Scene const &)
    0x180072320  ??4Scene@Picking@@QEAAAEAV01@AEBV01@@Z
                    public class Picking::Scene & __thiscall Picking::Scene::operator=(class Picking::Scene const &)
    0x180122a80  ?ClipTriangle@Scene@Picking@@AEBAHHPEAVVector3@Math@@0AEBVPlane@Volume@@@Z
                    private int __thiscall Picking::Scene::ClipTriangle(int,class Math::Vector3 *,class Math::Vector3 *,class Volume::Plane const &) const
    0x180122ef0  ?ClipTriangle@Scene@Picking@@AEBAXAEBVVector3@Math@@00000PEBVPlane@Volume@@HAEBVTransform@4@AEAV?$Array@UClipInfo@Scene@Picking@@@Collection@@AEAV?$Array@VVector3@Math@@@9@4@Z
                    private void __thiscall Picking::Scene::ClipTriangle(class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &,class Volume::Plane const *,int,class Math::Transform const &,class Collection::Array<struct Picking::Scene::ClipInfo> &,class Collection::Array<class Math::Vector3> &,class Collection::Array<class Math::Vector3> &) const
    0x180123c50  ?Clip@Scene@Picking@@QEAAXAEBVTransform@Math@@AEBVFrustum@Volume@@HAEAV?$Array@UClipInfo@Scene@Picking@@@Collection@@AEAV?$Array@VVector3@Math@@@8@3@Z
                    public void __thiscall Picking::Scene::Clip(class Math::Transform const &,class Volume::Frustum const &,int,class Collection::Array<struct Picking::Scene::ClipInfo> &,class Collection::Array<class Math::Vector3> &,class Collection::Array<class Math::Vector3> &)
    0x1801246c0  ?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z
                    public int __thiscall Picking::Scene::Intersect(class Math::Vector3 const &,class Math::Vector3 const &,class Math::Scalar const &,class Math::Vector3 &,class Math::Vector3 &) const
    0x180125240  ?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z
                    public void __thiscall Picking::Scene::SetMeshUserData(unsigned int,void *,int,int,void (__cdecl *)(void *,int,int,class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &))
    0x180125290  ?RemoveMesh@Scene@Picking@@QEAAXI@Z
                    public void __thiscall Picking::Scene::RemoveMesh(unsigned int)
    0x1801252e0  ?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z
                    public unsigned int __thiscall Picking::Scene::AddMesh(class Object::ExternalPtr<class Picking::PickingMesh>,class Math::Transform const &,void *,int,int,void (__cdecl *)(void *,int,int,class Math::Vector3 const &,class Math::Vector3 const &,class Math::Vector3 const &))
    0x1801256a0  ?Update@Scene@Picking@@QEAAXAEBVTime@System@@_NMMMAEBVFrustum@Volume@@AEBVTransform@Math@@H@Z
                    public void __thiscall Picking::Scene::Update(class System::Time const &,bool,float,float,float,class Volume::Frustum const &,class Math::Transform const &,int)
    0x180125890  ??1Scene@Picking@@QEAA@XZ
                    public __thiscall Picking::Scene::~Scene(void)
    0x1801258e0  ??0Scene@Picking@@QEAA@XZ
                    public __thiscall Picking::Scene::Scene(void)
##### Save::SaveLoad   exports=28 (no vtable export)
    0x180030d40  ?Get@SaveLoad@Save@@SAPEAV12@XZ
                    public static class Save::SaveLoad * __cdecl Save::SaveLoad::Get(void)
    0x180030d50  ??0SaveLoad@Save@@QEAA@AEBV01@@Z
                    public __thiscall Save::SaveLoad::SaveLoad(class Save::SaveLoad const &)
    0x180030e50  ??4SaveLoad@Save@@QEAAAEAV01@AEBV01@@Z
                    public class Save::SaveLoad & __thiscall Save::SaveLoad::operator=(class Save::SaveLoad const &)
    0x1800c0f10  ?OnVideoRestart@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::OnVideoRestart(void)
    0x1800c1000  ?OnVideoError@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::OnVideoError(void)
    0x1800c1090  ?CheckMod@SaveLoad@Save@@QEBAHHH@Z
                    public int __thiscall Save::SaveLoad::CheckMod(int,int) const
    0x1800c1250  ?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Save::SaveLoad::GenerateSaveName(void) const
    0x1800c1440  ?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ
                    public class System::String __thiscall Save::SaveLoad::GetAutoSaveName(void) const
    0x1800c1490  ?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z
                    public void __thiscall Save::SaveLoad::SaveMetaData(void *,int,int,class System::String,int)
    0x1800c1b30  ?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z
                    public void __thiscall Save::SaveLoad::LoadMetaData(class System::String &,void * *,int &,int &,int &,class Collection::Array<struct Save::SaveLoad::ModInfo> &)
    0x1800c1ea0  ?UpdateList@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::UpdateList(void)
    0x1800c2560  ?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::UpdateLoadFromFile(void)
    0x1800c2690  ?UpdateLoad@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::UpdateLoad(void)
    0x1800c2a80  ?UpdateSave@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::UpdateSave(void)
    0x1800c2d70  ?UpdateDelete@SaveLoad@Save@@QEAAXXZ
                    public void __thiscall Save::SaveLoad::UpdateDelete(void)
    0x1800c2e40  ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Save::SaveLoad::Update(class System::Time const &)
    0x1800c2eb0  ?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z
                    public void __thiscall Save::SaveLoad::Draw(class Video::Context &,class Object::ExternalPtr<class Graphics::Material>)
    0x1800c3b50  ?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ
                    public class Save::SavedState * __thiscall Save::SaveLoad::GetSaveState(void)
    0x1800c3b60  ?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z
                    public struct Save::SaveLoad::SaveInfo const & __thiscall Save::SaveLoad::GetSaveInfo(int) const
    0x1800c3b80  ?GetSaveCount@SaveLoad@Save@@QEBAHXZ
                    public int __thiscall Save::SaveLoad::GetSaveCount(void) const
    0x1800c3b90  ?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z
                    public void __thiscall Save::SaveLoad::SetSaveSprite(class UI::Sprite)
    0x1800c3bb0  ?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z
                    public void __thiscall Save::SaveLoad::SetSaveName(class System::String const &,int)
    0x1800c3c40  ?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
                    public void __thiscall Save::SaveLoad::SetMinOperationTime(class System::Time const &)
    0x1800c3c50  ?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z
                    public void __thiscall Save::SaveLoad::SetOperation(enum Save::SaveLoad::Operation,int)
    0x1800c3c80  ?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ
                    public enum Save::SaveLoad::Operation __thiscall Save::SaveLoad::GetOperation(void) const
    0x1800c3c90  ??1SaveLoad@Save@@QEAA@XZ
                    public __thiscall Save::SaveLoad::~SaveLoad(void)
    0x1800c3d30  ??0SaveLoad@Save@@QEAA@XZ
                    public __thiscall Save::SaveLoad::SaveLoad(void)
    0x1801fc6f0  ?_singleton@SaveLoad@Save@@0PEAV12@EA
                    private static class Save::SaveLoad * Save::SaveLoad::_singleton
##### Save::SavedState   exports=10 (no vtable export)
    0x180030bd0  ??4SavedState@Save@@QEAAAEAV01@AEBV01@@Z
                    public class Save::SavedState & __thiscall Save::SavedState::operator=(class Save::SavedState const &)
    0x1800c0c60  ?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ
                    public struct IO::Persistence::SaveBlock __thiscall Save::SavedState::SetupLoad(void)
    0x1800c0d50  ?GetSaveBlock@SavedState@Save@@QEBA?AUSaveBlock@Persistence@IO@@XZ
                    public struct IO::Persistence::SaveBlock __thiscall Save::SavedState::GetSaveBlock(void) const
    0x1800c0d70  ?Discard@SavedState@Save@@QEAAXXZ
                    public void __thiscall Save::SavedState::Discard(void)
    0x1800c0da0  ?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z
                    public void __thiscall Save::SavedState::Store(class IO::MemoryStream &)
    0x1800c0e50  ?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z
                    public void __thiscall Save::SavedState::Retrieve(class IO::MemoryStream &)
    0x1800c0e90  ?SetId@SavedState@Save@@QEAAXI@Z
                    public void __thiscall Save::SavedState::SetId(unsigned int)
    0x1800c0ea0  ??1SavedState@Save@@QEAA@XZ
                    public __thiscall Save::SavedState::~SavedState(void)
    0x1800c0ec0  ??0SavedState@Save@@QEAA@I@Z
                    public __thiscall Save::SavedState::SavedState(unsigned int)
    0x1800c0ee0  ??0SavedState@Save@@QEAA@XZ
                    public __thiscall Save::SavedState::SavedState(void)
```

## §7 — the first stores of every constructor (condensed; full heads in the raw file, SECTION 7)

The polymorphism test the brief asks for: what the ctor writes at `this+0` first, and whether any `??_7` is stamped.

| class | ctor (address) | first store(s) at +0 / vtable stamp chain | last store / size evidence | verdict |
|---|---|---|---|---|
| `Engine::MainLoop` | `??0MainLoop@Engine@@QEAA@PEAV11@@Z` 0x1800dd6b0 | `XOR EAX,EAX` / `MOV [RCX],RAX` (Array data ptr); no `??_7` | qword +0x98; exe `MemAlloc(0xa0,8)` at 0x140041f77 | NOT polymorphic, 0xa0 (the tree header is right) |
| `Engine::Engine` | `??0Engine@0@QEAA@PEAX@Z` 0x1800dbf00 | `CALL ??0MemoryAllocator@System@@` on `this` (allocator by value at +0) | word +0x6f0; exe holds it on GameMain's stack (`LEA RCX,[RSP+0x60]`) | NOT polymorphic; size 0x6f8 stays INFERRED |
| `Application::Application` | `??0Application@0@QEAA@PEAX@Z` 0x18009f590 | `MOV [RCX],R8` (=0); `_singleton = this` | byte +0x28; `Engine::Startup` `MemAlloc(0x30)` at 0x1800dbbb3 (ctor inlined there) | NOT polymorphic, 0x30; the ctor IS exported (Application.h:13-15 says otherwise) |
| `Achievement::Achievement` | `??0Achievement@0@QEAA@XZ` 0x18009e240 | `??_7Base@Object` then `??_7External@Object` then `??_7Achievement@0@6B@` (0x180154740) | qword +0x50; slot-0 `MOV EDX,0x58` | POLYMORPHIC `Object::External` subclass, 9 slots, 0x58 |
| `Achievement::AchievementInstance` | 0x18009e3b0 | `CALL ??0Internal@Object` then `??_7AchievementInstance` | qword +0x20; `MOV EDX,0x28` | `Object::Internal` subclass, 0x28; GoalList by value at +0x10 |
| `Achievement::Goal` | 0x18009ed50 | Base, External, then `??_7Goal` | qword +0x28; `MOV EDX,0x30` | `Object::External` subclass, ABSTRACT (slots 6, 7, 8 pure) |
| `Achievement::GoalInstance` | 0x18009ef00 | `??0Internal` then `??_7GoalInstance` | qword +0x18; `MOV EDX,0x20` | `Object::Internal` subclass, 0x20 |
| `Achievement::GoalList` | `??0GoalList@Achievement@@QEAA@XZ` 0x18009f0b0 | `XOR EAX,EAX` / `MOV [RCX],RAX` / `MOV [RCX+8],RAX`; no `??_7` | qword +0x08 | NOT polymorphic, 0x10 (one `Array<InternalPtr<GoalInstance>>`); dtor `QEAA` 0x18009f420 walks the array calling each element's slot 0 with EDX=1 |
| `Achievement::AchievementManager` | 0x18009eb00 | zeroes +0x00..+0x38; `_singleton = this` | dword +0x40; `Engine::Startup` `MemAlloc(0x48)` at 0x1800dbe42 (ctor inlined) | NOT polymorphic, 0x48 |
| `Audio::Configuration` | 0x1800ddeb0 | `MOV dword [RCX],0x64` | dword +0xc; `MemAlloc(0x10)` at 0x1800dbd00 | NOT polymorphic, 0x10 |
| `Audio::Device` | 0x1800de3a0 | 64 x 0x28 AudioChannel loop from +0; `??0Stream` at +0xa00; 64 x 0x20 records at +0xa98 stamped with 0x180157c60 | `MemAlloc(0x1298)` at 0x1800dbdd4 | NOT polymorphic, 0x1298; holds a POLYMORPHIC `Audio::Stream` by value at +0xa00 and 64 records of an unexported polymorphic class |
| `Audio::Scene` | 0x1800e0160 | `MOV [RCX],RSI` (=0, a `Time`); `+0x8 = 1` | byte +0x34; exe `MemAlloc(0x38,8)` at 0x140097065 | NOT polymorphic, 0x38; the exe also holds one BY VALUE at MenuDialogLoopT+0xb30 |
| `Audio::SoundBuffer` | 0x1800e0320 | Base, External, CompiledExternal, then `??_7SoundBuffer` | `MOV EDX,0x80` | `Object::CompiledExternal` subclass, 0x80 |
| `Audio::SoundEffect` | 0x1800e0810 | Base, External, then `??_7SoundEffect` | `MOV EDX,0x40` | `Object::External` subclass, 0x40 |
| `Audio::Stream` | `??0Stream@Audio@@QEAA@XZ` 0x1800a0e30 | `CALL ??0Thread@System@@QEAA@I@Z` (0x10000), then `??_7Stream` | byte +0x80 (0x88, see §1a) | `System::Thread` subclass (Run overridden); CRITICAL_SECTION raw at +0x50 (`InitializeCriticalSection` + spin 5000, not the door) |
| `Engine::UpdateLoop` | 0x1800ddd30 | `??_7UpdateLoop` | dword +0x24; `MOV EDX,0x28` | root, ABSTRACT (slots 1-4 and 6 pure) |
| `Engine::LoaderLoop` | 0x1800dc1e0 | `CALL ??0UpdateLoop` then `??_7LoaderLoop` | byte +0x40; `MOV EDX,0x48` | UpdateLoop subclass, CONCRETE (no pure slot) |
| `Engine::DialogLoopT` | 0x1800db3f0 | `CALL ??0UpdateLoop` then `??_7DialogLoopT`; `??0UserInterface` at +0x40 | `MOV EDX,0x168` | UpdateLoop subclass, ABSTRACT (slot 12 pure) |
| `Engine::DialogLoaderLoopT` | 0x1800db0d0 | `CALL ??0LoaderLoop` then `??_7DialogLoaderLoopT`; `??0UserInterface` at +0x60 | `MOV EDX,0x188` | LoaderLoop subclass, ABSTRACT (slot 12 pure) |
| `Engine::Loader` | 0x1800dc0f0 | `CALL ??0Thread` (0x40000), then `??_7Loader` | qword +0x30 (0x38) | `System::Thread` subclass; the only instance is the static `_loader` |
| `Input::Configuration` | 0x1800e0de0 | `MOV qword [RCX],0` (KeyMap*) | byte +0xa; `MemAlloc(0x10)` at 0x1800dbd51 | NOT polymorphic, 0x10; the KeyMap is allocated in `Startup` 0x1800e0d30 (`MemAlloc(0x18,4)`, ctor inlined) and held BY POINTER |
| `Input::Device` | 0x1800e13b0 | three `State` loops (0x48, 0x69, 0xd entries of 0x18) | dword +0x11fc; `MemAlloc(0x1200)` at 0x1800dbe0b | NOT polymorphic, 0x1200 |
| `Input::KeyMap` | 0x1800a7fb0 | `MOV dword [RCX],0`; Set at +0x8 | dword +0x10; `MemAlloc(0x18,4)` | NOT polymorphic, 0x18 |
| `Pathfinding::IPathInfo` | 0x18006c970 | `??_7IPathInfo` only | none | pure interface: 2 slots, BOTH `_purecall`, no dtor slot; 8 bytes |
| `Pathfinding::PointList` | 0x18010f980 | Base, External, then `??_7PointList` | qword +0x48; `MOV EDX,0x50` | `Object::External` subclass, 0x50 |
| `Pathfinding::Scene` | 0x18010f660 | `MOV [RCX],RDX` (`const IPathInfo*`) | float +0x8c | NOT polymorphic, at least 0x90; zero callers in the DLL, not imported by the exe |
| `Picking::PickingMesh` | 0x180122830 | Base, External, then `??_7PickingMesh` | qword +0x40; `MOV EDX,0x48` | `Object::External` subclass, 0x48 |
| `Picking::Scene` | 0x1801258e0 | `CALL Picking_Scene_InitGrid` (unexported) | exe `MemAlloc(0x60,8)` at 0x1400970b7 | NOT polymorphic, 0x60; the exe also holds one BY VALUE at +0x8 of the object 0x14009ee70 builds |
| `Save::SaveLoad` | 0x1800c3d30 | `MOV dword [RCX],0`; `_singleton = this` | qword +0x88; exe `MemAlloc(0x90,4)` at 0x140097236 and 0x14009b9e0 | NOT polymorphic, 0x90; `Array<SaveInfo>` by value at +0x50 (`GetSaveInfo` 0x1800c3b60 `IMUL RAX,RAX,0x58; ADD RAX,[RCX+0x50]`; `GetSaveCount` reads +0x58) |
| `Save::SavedState` | 0x1800c0ee0 | `MOV [RCX],RAX` (=0) | qword +0x8; exe `MemAlloc(0x10,4)` | NOT polymorphic, 0x10 |

## §8 — exports taking a class BY VALUE: the prologue verdict per member (B6-11)

| member | argument | prologue evidence | arrives as |
|---|---|---|---|
| `AchievementInstance::SetAchievement` 0x18009e440 | `ExternalPtr<Achievement>` | `MOV [RCX+0x20],RDX`; `ADD RDX,0x48` (the Achievement's goal array) | bare register |
| `AchievementManager::SetAchievements` 0x18009e860 | `ExternalPtr<ExternalList>` | `MOV [RCX],RDX`; `MOV EDX,[RDX+0x28]` | bare register |
| `GoalInstance::SetGoal` 0x18009eee0 | `ExternalPtr<Goal>` | `MOV [RCX+0x18],RDX` | bare register |
| `GoalList::AreGoalsMetExcept` 0x18009f070 | `InternalPtr<GoalInstance>` | `MOV RDX,[RDX]` | HIDDEN POINTER |
| `GoalList::Initialize` 0x18009f0e0 | `Collection::Array<ExternalPtr<Goal>>` | `MOV EDX,[RDX+0x8]` (the count, through RDX) | HIDDEN POINTER |
| `Audio::Device::Play` 0x1800de280 | `ExternalPtr<SoundBuffer>` | RDX homed; `MOV R8,RDX` into `PlayPlatform`; the homed value stored raw at channel+0x8 | bare register |
| `Audio::Device::PlayPlatform` 0x1800de7a0 | `ExternalPtr<SoundBuffer>` (arg 2, R8) | receives `Play`'s RDX value in R8 | bare register (from the caller's shape) |
| `Audio::Scene::FindFree3DEffect` 0x1800deae0 | `ExternalPtr<SoundEffect>` | `MOV RAX,[RDX+0x28]` (a SoundEffect field) | bare register |
| `Audio::Scene::FindEffect3D` 0x1800dee70 | `ExternalPtr<SoundEffect>` | `CMP [RAX],RDX` | bare register |
| `Audio::Scene::PlayEffect3D` 0x1800deeb0 | `ExternalPtr<SoundEffect>` | `TEST RDX,RDX` | bare register |
| `Audio::Scene::PlayEffect` 0x1800def60 | `ExternalPtr<SoundEffect>` | `TEST RDX,RDX` | bare register |
| `Audio::Scene::Update` 0x1800df6c0 | `System::Time` x2 (RDX, R8) | `MOV R15,[this]`... `MOV [R15],R8` (the second Time's ticks stored raw) | register (8-byte value) |
| `Picking::Scene::AddMesh` 0x1801252e0 | `ExternalPtr<PickingMesh>` | RDX homed at `[RBP+0x48]`; `MOV RAX,[RBP+0x48]; MOV EDX,[RAX+0x40]` | bare register |
| `Save::SaveLoad::SaveMetaData` 0x1800c1490 | `System::String` (arg 4, stack) | `MOV R15,[RBP+0x80]` (the stack arg) used as a POINTER, and freed through it at 0x1800c1ab7..0x1800c1b0b | HIDDEN POINTER (callee-destroyed) |
| `Save::SaveLoad::Draw` 0x1800c2eb0 | `ExternalPtr<Graphics::Material>` (R8) | R8 homed at `[RSP0+0x18]`; its first use is not in the first 60 instructions | NOT VERIFIED |
| `Save::SaveLoad::SetSaveSprite` 0x1800c3b90 | `UI::Sprite` | `MOV RAX,[RDX]; MOV RAX,[RDX+0x8]` | HIDDEN POINTER |

## §9 — slot-0 bodies (size immediate and the dtor each calls; condensed from raw SECTION 9)

```
### Achievement::Achievement  slot0=0x180001680
    0x18000169f  MOV EDX,0x58
    0x1800016a9  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x1800016b8  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Achievement::AchievementInstance  slot0=0x180001a90
    0x180001ac2  MOV EDX,0x28
    0x180001ac7  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180001ad6  CALL 0x180006a10                                  => (unexported) FUN_180006a10
    0x180001aef  CALL 0x18009f420                                  => ??1GoalList@Achievement@@QEAA@XZ
    0x180001af8  CALL 0x1800bfdb0                                  => ??1Internal@Object@@UEAA@XZ
    0x180001b06  CALL 0x180001850                                  => ??3AchievementInstance@Achievement@@SAXPEAX@Z
### Achievement::Goal  slot0=0x180001d70
    0x180001da2  MOV EDX,0x30
    0x180001da7  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180001db6  CALL 0x180006a10                                  => (unexported) FUN_180006a10
    0x180001de6  CALL 0x1800c7ad0                                  => ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
    0x180001df8  CALL 0x1800bc950                                  => ??1External@Object@@UEAA@XZ
    0x180001e06  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Achievement::GoalInstance  slot0=0x1800020c0
    0x1800020f2  MOV EDX,0x20
    0x1800020f7  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180002106  CALL 0x180006a10                                  => (unexported) FUN_180006a10
    0x18000211b  CALL 0x1800bfdb0                                  => ??1Internal@Object@@UEAA@XZ
    0x180002129  CALL 0x180001ef0                                  => ??3GoalInstance@Achievement@@SAXPEAX@Z
### Audio::SoundBuffer  slot0=0x18000bef0
    0x18000bf0f  MOV EDX,0x80
    0x18000bf19  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x18000bf28  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Audio::SoundEffect  slot0=0x180050820
    0x180050852  MOV EDX,0x40
    0x180050857  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180050866  CALL 0x180006a10                                  => (unexported) FUN_180006a10
    0x18005087f  CALL 0x1800516c0                                  => (unexported) Collection_Array_DtorElem_1800516c0
    0x180050888  CALL 0x1800bc950                                  => ??1External@Object@@UEAA@XZ
    0x180050896  CALL 0x1800503d0                                  => ??3SoundEffect@Audio@@SAXPEAX@Z
### Audio::Stream  slot0=0x1800a0f60
### Engine::DialogLoaderLoopT  slot0=0x18004d670
    0x18004d68f  MOV EDX,0x188
    0x18004d699  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x18004d6a8  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Engine::DialogLoopT  slot0=0x18004d8b0
    0x18004d8cf  MOV EDX,0x168
    0x18004d8d9  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x18004d8e8  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Engine::Loader  slot0=0x1800dbfc0
    0x1800dbff3  CALL qword ptr [RAX + 0x20]
    0x1800dc007  CALL 0x1800dd7d0                                  => ?LoadResources@UpdateLoop@Engine@@QEAAXXZ
    0x1800dc013  CALL qword ptr [RAX + 0x30]
### Engine::LoaderLoop  slot0=0x18004d4f0
    0x18004d50f  MOV EDX,0x48
    0x18004d519  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x18004d528  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Engine::UpdateLoop  slot0=0x180031900
    0x18003191f  MOV EDX,0x28
    0x180031929  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180031938  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Pathfinding::IPathInfo  slot0=0x18007ec28
    0x18007ec33  CALL qword ptr [0x18014b438]                      => PTR_DecodePointer_18014b438 (.rdata)
    0x18007ec3e  CALL RAX
    0x18007ec45  CALL 0x1800805a8                                  => (unexported) _NMSG_WRITE
    0x18007ec4a  MOV EDX,0x1
    0x18007ec51  CALL 0x18008084c                                  => (unexported) _set_abort_behavior
    0x18007ec56  CALL 0x180080868                                  => (unexported) FUN_180080868
### Pathfinding::PointList  slot0=0x1800713b0
    0x1800713cf  MOV EDX,0x50
    0x1800713d9  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x1800713e8  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### Picking::PickingMesh  slot0=0x180072050
    0x18007206f  MOV EDX,0x48
    0x180072079  CALL 0x18007ec98                                  => ??_M@YAXPEAX_KHP6AX0@Z@Z
    0x180072088  CALL 0x180006a10                                  => (unexported) FUN_180006a10
### System::Thread  slot0=0x18007ec28
    0x18007ec33  CALL qword ptr [0x18014b438]                      => PTR_DecodePointer_18014b438 (.rdata)
    0x18007ec3e  CALL RAX
    0x18007ec45  CALL 0x1800805a8                                  => (unexported) _NMSG_WRITE
    0x18007ec4a  MOV EDX,0x1
    0x18007ec51  CALL 0x18008084c                                  => (unexported) _set_abort_behavior
    0x18007ec56  CALL 0x180080868                                  => (unexported) FUN_180080868
```

## §E1 — the five exe loop-subclass vtables (Application-x64.exe)

`STUB JMP [iat] -> X` = an undefined `FF 25` import stub in the exe's stub run at 0x14003a8xx, so the slot INHERITS
the Runtime base body `X`. `EXE ...` = an exe body (Ghidra auto-names such as `ModDialog_impl_6010` are NOT attested).

```
### GameUpdateLoop (brief: 22 slots?)  vtable=0x14015e2f0  slots=12  (exe bodies=11, Runtime thunks=1)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140019dd0  EXE FUN_140019dd0
  [ 1] +0x08 0x140097c00  EXE FUN_140097c00
  [ 2] +0x10 0x1400977c0  EXE CitizenComponent_impl_77c0
  [ 3] +0x18 0x140096380  EXE FUN_140096380
  [ 4] +0x20 0x140096010  EXE ModDialog_impl_6010
  [ 5] +0x28 0x140096000  EXE RET 0 (empty body, no Ghidra function)
  [ 6] +0x30 0x140096b00  EXE MainMenuDialog_impl_6b00
  [ 7] +0x38 0x1400959c0  EXE TradeComponent_impl_59c0
  [ 8] +0x40 0x140095970  EXE FUN_140095970
  [ 9] +0x48 0x140095930  EXE FUN_140095930
  [10] +0x50 0x140019d40  EXE code, no Ghidra function
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (6):
### MenuDialogLoopT (MenuDLT)  vtable=0x14015ed00  slots=13  (exe bodies=11, Runtime thunks=2)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x14001a2e0  EXE FUN_14001a2e0
  [ 1] +0x08 0x14009b930  EXE OptionsDialog_impl_b930
  [ 2] +0x10 0x14009b280  EXE FUN_14009b280
  [ 3] +0x18 0x14009b6e0  EXE MainMenuDialog_impl_b6e0
  [ 4] +0x20 0x14009b440  EXE FUN_14009b440
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14009a930  EXE MenuScene_impl_a930
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14009a8d0  EXE code, no Ghidra function
  [ 9] +0x48 0x14009a8b0  EXE code, no Ghidra function
  [10] +0x50 0x14001a2d0  EXE code, no Ghidra function
  [11] +0x58 0x14001a2c0  EXE code, no Ghidra function
  [12] +0x60 0x14009ba30  EXE MainMenuDialog_Ctor
  referenced by (2):
### splash DialogLoaderLoopT subclass  vtable=0x140158800  slots=13  (exe bodies=5, Runtime thunks=8)  stop: zero qword
  [ 0] +0x00 0x140001010  EXE FUN_140001010
  [ 1] +0x08 0x140098fa0  EXE OptionsDialog_impl_8fa0
  [ 2] +0x10 0x140098f90  EXE RET 0 (empty body, no Ghidra function)
  [ 3] +0x18 0x140098da0  EXE FUN_140098da0
  [ 4] +0x20 0x14003a8ee  STUB JMP [0x140152b90] -> ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14003a8e8  STUB JMP [0x140152b98] -> ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x14003a8fa  STUB JMP [0x140152b28] -> ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  [12] +0x60 0x140099070  EXE LoaderDialog_Ctor3
  referenced by (16):
### LoaderLoop subclass 'inner'  vtable=0x140158870  slots=12  (exe bodies=3, Runtime thunks=9)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140001150  EXE FUN_140001150
  [ 1] +0x08 0x14003a8e2  STUB JMP [0x140152bb0] -> ?Startup@LoaderLoop@Engine@@UEAAXXZ
  [ 2] +0x10 0x14003a8dc  STUB JMP [0x140152bb8] -> ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
  [ 3] +0x18 0x14003a8d6  STUB JMP [0x140152bc0] -> ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x14003a8d0  STUB JMP [0x140152bc8] -> ?Draw@LoaderLoop@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x140001120  EXE FUN_140001120
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x140001110  EXE code, no Ghidra function
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (2):
### LoaderLoop subclass 'outer'  vtable=0x1401588d0  slots=12  (exe bodies=1, Runtime thunks=11)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140001190  EXE FUN_140001190
  [ 1] +0x08 0x14003a8e2  STUB JMP [0x140152bb0] -> ?Startup@LoaderLoop@Engine@@UEAAXXZ
  [ 2] +0x10 0x14003a8dc  STUB JMP [0x140152bb8] -> ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
  [ 3] +0x18 0x14003a8d6  STUB JMP [0x140152bc0] -> ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x14003a8d0  STUB JMP [0x140152bc8] -> ?Draw@LoaderLoop@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14003a8ca  STUB JMP [0x140152bd0] -> ?Load@LoaderLoop@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x14003a8fa  STUB JMP [0x140152b28] -> ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (14):
```

## §E3 — tables stamped right after a step-7 base ctor call (the exe subclass census)

```
### after ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z in FUN_1400b4540  vtable=0x140158800  slots=13  (exe bodies=5, Runtime thunks=8)  stop: zero qword
  [ 0] +0x00 0x140001010  EXE FUN_140001010
  [ 1] +0x08 0x140098fa0  EXE OptionsDialog_impl_8fa0
  [ 2] +0x10 0x140098f90  EXE RET 0 (empty body, no Ghidra function)
  [ 3] +0x18 0x140098da0  EXE FUN_140098da0
  [ 4] +0x20 0x14003a8ee  STUB JMP [0x140152b90] -> ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14003a8e8  STUB JMP [0x140152b98] -> ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x14003a8fa  STUB JMP [0x140152b28] -> ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  [12] +0x60 0x140099070  EXE LoaderDialog_Ctor3
  referenced by (16):
### after ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z in GameMain  vtable=0x140158870  slots=12  (exe bodies=3, Runtime thunks=9)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140001150  EXE FUN_140001150
  [ 1] +0x08 0x14003a8e2  STUB JMP [0x140152bb0] -> ?Startup@LoaderLoop@Engine@@UEAAXXZ
  [ 2] +0x10 0x14003a8dc  STUB JMP [0x140152bb8] -> ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
  [ 3] +0x18 0x14003a8d6  STUB JMP [0x140152bc0] -> ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x14003a8d0  STUB JMP [0x140152bc8] -> ?Draw@LoaderLoop@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x140001120  EXE FUN_140001120
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x140001110  EXE code, no Ghidra function
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (2):
### after ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z in FUN_1400b4540  vtable=0x1401588d0  slots=12  (exe bodies=1, Runtime thunks=11)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140001190  EXE FUN_140001190
  [ 1] +0x08 0x14003a8e2  STUB JMP [0x140152bb0] -> ?Startup@LoaderLoop@Engine@@UEAAXXZ
  [ 2] +0x10 0x14003a8dc  STUB JMP [0x140152bb8] -> ?Shutdown@LoaderLoop@Engine@@UEAAXXZ
  [ 3] +0x18 0x14003a8d6  STUB JMP [0x140152bc0] -> ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z
  [ 4] +0x20 0x14003a8d0  STUB JMP [0x140152bc8] -> ?Draw@LoaderLoop@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14003a8ca  STUB JMP [0x140152bd0] -> ?Load@LoaderLoop@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14003a8c4  STUB JMP [0x140152bd8] -> ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ
  [ 9] +0x48 0x14003a900  STUB JMP [0x140152b20] -> ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ
  [10] +0x50 0x14003a8fa  STUB JMP [0x140152b28] -> ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (14):
### after ??0UpdateLoop@Engine@@QEAA@XZ in FUN_140098060  vtable=0x14015e2f0  slots=12  (exe bodies=11, Runtime thunks=1)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140019dd0  EXE FUN_140019dd0
  [ 1] +0x08 0x140097c00  EXE FUN_140097c00
  [ 2] +0x10 0x1400977c0  EXE CitizenComponent_impl_77c0
  [ 3] +0x18 0x140096380  EXE FUN_140096380
  [ 4] +0x20 0x140096010  EXE ModDialog_impl_6010
  [ 5] +0x28 0x140096000  EXE RET 0 (empty body, no Ghidra function)
  [ 6] +0x30 0x140096b00  EXE MainMenuDialog_impl_6b00
  [ 7] +0x38 0x1400959c0  EXE TradeComponent_impl_59c0
  [ 8] +0x40 0x140095970  EXE FUN_140095970
  [ 9] +0x48 0x140095930  EXE FUN_140095930
  [10] +0x50 0x140019d40  EXE code, no Ghidra function
  [11] +0x58 0x14003a8f4  STUB JMP [0x140152b30] -> ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
  referenced by (6):
### after ??0AchievementInstance@Achievement@@QEAA@XZ in MainMenuDialog_impl_6b00  vtable=0x14015e350  slots=4  (exe bodies=1, Runtime thunks=3)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140019d90  EXE FUN_140019d90
  [ 1] +0x08 0x14003aaf2  STUB JMP [0x140152200] -> ?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ
  [ 2] +0x10 0x14003aaf8  STUB JMP [0x140152208] -> ?Type@AchievementInstance@Achievement@@UEBAIXZ
  [ 3] +0x18 0x14003aada  STUB JMP [0x1401521d0] -> ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
  referenced by (2):
### after ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z in FUN_14009bb00  vtable=0x14015ed00  slots=13  (exe bodies=11, Runtime thunks=2)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x14001a2e0  EXE FUN_14001a2e0
  [ 1] +0x08 0x14009b930  EXE OptionsDialog_impl_b930
  [ 2] +0x10 0x14009b280  EXE FUN_14009b280
  [ 3] +0x18 0x14009b6e0  EXE MainMenuDialog_impl_b6e0
  [ 4] +0x20 0x14009b440  EXE FUN_14009b440
  [ 5] +0x28 0x14003a90c  STUB JMP [0x140152b10] -> ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ
  [ 6] +0x30 0x14009a930  EXE MenuScene_impl_a930
  [ 7] +0x38 0x14003a906  STUB JMP [0x140152b18] -> ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z
  [ 8] +0x40 0x14009a8d0  EXE code, no Ghidra function
  [ 9] +0x48 0x14009a8b0  EXE code, no Ghidra function
  [10] +0x50 0x14001a2d0  EXE code, no Ghidra function
  [11] +0x58 0x14001a2c0  EXE code, no Ghidra function
  [12] +0x60 0x14009ba30  EXE MainMenuDialog_Ctor
  referenced by (2):
### after ??0Achievement@0@QEAA@XZ in Scenario_Factory  vtable=0x14015f020  slots=9  (exe bodies=8, Runtime thunks=1)  stop: zero qword
  [ 0] +0x00 0x14001eb40  EXE Scenario_ScalarDtor
  [ 1] +0x08 0x14001e9e0  EXE Scenario_Slot1
  [ 2] +0x10 0x14001e9d0  EXE Scenario_Slot2
  [ 3] +0x18 0x1400a9460  EXE Scenario_Shutdown
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400a9370  EXE Scenario_Slot5
  [ 6] +0x30 0x14001ead0  EXE Scenario_Draw
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x14001ea70  EXE Scenario_Cleanup
  referenced by (12):
### after ??0Goal@Achievement@@QEAA@XZ in BuildingGoal_Factory  vtable=0x140160300  slots=11  (exe bodies=9, Runtime thunks=2)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140025e80  EXE BuildingGoal_ScalarDtor
  [ 1] +0x08 0x140025d70  EXE BuildingGoal_Slot1
  [ 2] +0x10 0x140025d60  EXE BuildingGoal_Slot2
  [ 3] +0x18 0x1400d7790  EXE BuildingGoal_Shutdown
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400d76a0  EXE BuildingGoal_Slot5
  [ 6] +0x30 0x140025e30  EXE BuildingGoal_Draw
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x1400d7580  EXE BuildingGoal_OnRemove
  [ 9] +0x48 0x14003ab5e  THUNK -> ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
  [10] +0x50 0x140025de0  EXE BuildingGoal_Cleanup
  referenced by (12):
### after ??0Goal@Achievement@@QEAA@XZ in PopulationGoal_Factory  vtable=0x140160370  slots=11  (exe bodies=9, Runtime thunks=2)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026000  EXE PopulationGoal_ScalarDtor
  [ 1] +0x08 0x140025ef0  EXE PopulationGoal_Slot1
  [ 2] +0x10 0x140025ee0  EXE PopulationGoal_Slot2
  [ 3] +0x18 0x1400d78e0  EXE PopulationGoal_Shutdown
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400d7870  EXE PopulationGoal_Slot5
  [ 6] +0x30 0x140025fb0  EXE PopulationGoal_Draw
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x1400d7820  EXE PopulationGoal_OnRemove
  [ 9] +0x48 0x14003ab5e  THUNK -> ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
  [10] +0x50 0x140025f60  EXE PopulationGoal_Cleanup
  referenced by (12):
### after ??0Goal@Achievement@@QEAA@XZ in FinishTutorialGoal_Factory  vtable=0x140160420  slots=11  (exe bodies=10, Runtime thunks=1)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026260  EXE FinishTutorialGoal_ScalarDtor
  [ 1] +0x08 0x1400260e0  EXE FinishTutorialGoal_Slot1
  [ 2] +0x10 0x1400260d0  EXE FinishTutorialGoal_Slot2
  [ 3] +0x18 0x1400d79d0  EXE FinishTutorialGoal_Shutdown
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400d7970  EXE FinishTutorialGoal_Slot5
  [ 6] +0x30 0x1400261a0  EXE FinishTutorialGoal_Draw
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x1400d7960  EXE FinishTutorialGoal_OnRemove
  [ 9] +0x48 0x1400261f0  EXE FinishTutorialGoal_Update
  [10] +0x50 0x140026150  EXE FinishTutorialGoal_Cleanup
  referenced by (12):
### after ??0GoalInstance@Achievement@@QEAA@XZ in FinishTutorialInstance_Factory  vtable=0x140160478  slots=7  (exe bodies=7, Runtime thunks=0)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026200  EXE FinishTutorialGoal_Slot11
  [ 1] +0x08 0x140026060  EXE FinishTutorialGoal_Slot12
  [ 2] +0x10 0x140026050  EXE FinishTutorialGoal_Slot13
  [ 3] +0x18 0x1400d7bf0  EXE FinishTutorialGoal_Slot14
  [ 4] +0x20 0x1400d7a90  EXE FinishTutorialGoal_Slot15
  [ 5] +0x28 0x1400d7a50  EXE FinishTutorialInstance_Slot5
  [ 6] +0x30 0x1400d7bb0  EXE FinishTutorialInstance_Draw
  referenced by (8):
### after ??0Goal@Achievement@@QEAA@XZ in StatGoal_Ctor  vtable=0x1401604c0  slots=11  (exe bodies=9, Runtime thunks=2)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026540  EXE FinishTutorialInstance_vslot9
  [ 1] +0x08 0x1400264c0  EXE code, no Ghidra function
  [ 2] +0x10 0x1400264b0  EXE code, no Ghidra function
  [ 3] +0x18 0x1400daa10  EXE code, no Ghidra function
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400da8b0  EXE FinishTutorialInstance_vslot14
  [ 6] +0x30 0x140026520  EXE FinishTutorialInstance_vslot15
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x1400da720  EXE FinishTutorialInstance_vslot17
  [ 9] +0x48 0x14003ab5e  THUNK -> ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
  [10] +0x50 0x140026510  EXE code, no Ghidra function
  referenced by (6):
### after ??0Goal@Achievement@@QEAA@XZ in TimeGoal_Factory  vtable=0x140160540  slots=11  (exe bodies=10, Runtime thunks=1)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026850  EXE TimeGoal_ScalarDtor
  [ 1] +0x08 0x1400266e0  EXE TimeGoal_Slot1
  [ 2] +0x10 0x1400266d0  EXE TimeGoal_Slot2
  [ 3] +0x18 0x1400daba0  EXE TimeGoal_Shutdown
  [ 4] +0x20 0x14003a912  THUNK -> ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1400dab20  EXE TimeGoal_Slot5
  [ 6] +0x30 0x1400267a0  EXE TimeGoal_Draw
  [ 7] +0x38 0x140001640  EXE CitizenComponent_GetDescription
  [ 8] +0x40 0x1400dab10  EXE TimeGoal_OnRemove
  [ 9] +0x48 0x1400267f0  EXE TimeGoal_Update
  [10] +0x50 0x140026750  EXE TimeGoal_Cleanup
  referenced by (12):
### after ??0GoalInstance@Achievement@@QEAA@XZ in TimeGoalInstance_Factory  vtable=0x140160598  slots=7  (exe bodies=7, Runtime thunks=0)  stop: next address is referenced from code (another table)
  [ 0] +0x00 0x140026800  EXE TimeGoal_Slot11
  [ 1] +0x08 0x140026660  EXE TimeGoal_Slot12
  [ 2] +0x10 0x140026650  EXE TimeGoal_Slot13
  [ 3] +0x18 0x1400dad30  EXE TimeGoal_Slot14
  [ 4] +0x20 0x1400daca0  EXE TimeGoal_Slot15
  [ 5] +0x28 0x1400dac30  EXE TimeGoalInstance_Slot5
  [ 6] +0x30 0x1400dacb0  EXE TimeGoalInstance_Draw
  referenced by (8):
```

## §E2 — exe call sites of step-7 ctor/dtor imports (counts; the instruction windows are in the raw file)

```
  4  ??0Achievement@0@QEAA@XZ
  1  ??0AchievementInstance@Achievement@@QEAA@XZ
  8  ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
  1  ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z
  1  ??0Engine@0@QEAA@PEAX@Z
 17  ??0Goal@Achievement@@QEAA@XZ
  4  ??0GoalInstance@Achievement@@QEAA@XZ
  8  ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z
  1  ??0MainLoop@Engine@@QEAA@PEAV11@@Z
  2  ??0SaveLoad@Save@@QEAA@XZ
  2  ??0SavedState@Save@@QEAA@XZ
  2  ??0Scene@Audio@@QEAA@XZ
  2  ??0Scene@Picking@@QEAA@XZ
  1  ??0UpdateLoop@Engine@@QEAA@XZ
  6  ??1Achievement@0@UEAA@XZ
  1  ??1AchievementInstance@Achievement@@UEAA@XZ
 10  ??1DialogLoaderLoopT@Engine@@UEAA@XZ
  3  ??1DialogLoopT@Engine@@UEAA@XZ
  2  ??1Engine@0@QEAA@XZ
 25  ??1Goal@Achievement@@UEAA@XZ
 10  ??1GoalInstance@Achievement@@UEAA@XZ
  4  ??1LoaderLoop@Engine@@UEAA@XZ
  1  ??1MainLoop@Engine@@QEAA@XZ
  7  ??1SaveLoad@Save@@QEAA@XZ
  1  ??1SavedState@Save@@QEAA@XZ
  4  ??1Scene@Audio@@QEAA@XZ
  4  ??1Scene@Picking@@QEAA@XZ
  5  ??1UpdateLoop@Engine@@UEAA@XZ
```

Allocation sizes read in those windows: `MainLoop` `MemAlloc(0xa0,8)` (GameMain 0x140041f77); `SaveLoad` `MemAlloc(0x90,4)`
(0x140097236, 0x14009b9e0); `SavedState` `MemAlloc(0x10,4)`; `Audio::Scene` `MemAlloc(0x38,8)` (0x14009706a) and BY VALUE
at `MenuDialogLoopT+0xb30` (0x14009bb34); `Picking::Scene` `MemAlloc(0x60,8)` (0x140097093) and BY VALUE at `+0x8` of
the object 0x14009ee70 builds; `Engine` on `GameMain`'s stack at `[RSP+0x60]`.

## §E4 — the exe's Runtime imports in the eight namespaces

From the PE import table (`pefile`): Engine 42, Save 18, Achievement 42, Audio 21, Input 21, **Pathfinding 1**,
Picking 11, Application 4. The DATA imports among them: `?_singleton@MainLoop@Engine@@` (IAT 0x140151440),
`?_singleton@SaveLoad@Save@@` (0x140151610), `?_singleton@AchievementManager@Achievement@@` (0x1401514f0),
`?_typeInfo@AchievementInstance@Achievement@@` (0x140151f50), `?_typeInfo@Achievement@1@` (0x140151f68),
`?_singleton@Configuration@Audio@@` (0x140151b78), `?_typeInfo@SoundEffect@Audio@@` (0x140152a80),
`?_singleton@Configuration@Input@@` (0x140151738), `?_singleton@Device@Input@@` (0x140151b80),
**`?_typeInfo@PointList@Pathfinding@@` (0x140151448), the ONLY Pathfinding import**, `?_typeInfo@PickingMesh@Picking@@`
(0x140151468), `?_singleton@Application@1@` (0x140152368). The Ghidra census below covers FUNCTION imports only.

The function imports and the exe functions reaching each (direct `CALL [IAT]` and the undefined JMP stubs; `Unwind@`
rows are Ghidra's exception-unwind pseudo-functions):

```
??0Achievement@0@QEAA@XZ                                                                             sites=4 fns=4  [Scenario_Cleanup, Scenario_CopyCtor, Scenario_Draw, Scenario_Factory]
??0AchievementInstance@Achievement@@QEAA@XZ                                                          sites=1 fns=1  [MainMenuDialog_impl_6b00]
??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z                  sites=8 fns=7  [FUN_1400b4540, FUN_1400b4790, FUN_1400b49f0, FinishTutorialInstance_helper_0d70, GameMain, SaveLoadDialog_OnCreate, TutorialUI_helper_34e0]
??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z                                                    sites=1 fns=1  [FUN_14009bb00]
??0Engine@0@QEAA@PEAX@Z                                                                              sites=1 fns=1  [GameMain]
??0Goal@Achievement@@QEAA@XZ                                                                         sites=17 fns=17  [BuildingGoal_Cleanup, BuildingGoal_CopyCtor, BuildingGoal_Draw, BuildingGoal_Factory, FinishTutorialGoal_Cleanup, FinishTutorialGoal_CopyCtor, FinishTutorialGoal_Draw, FinishTutorialGoal_Factory, PopulationGoal_Cleanup, PopulationGoal_CopyCtor] ...
??0GoalInstance@Achievement@@QEAA@XZ                                                                 sites=4 fns=4  [FinishTutorialInstance_CopyCtor, FinishTutorialInstance_Factory, TimeGoalInstance_CopyCtor, TimeGoalInstance_Factory]
??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z                                      sites=8 fns=7  [FUN_1400b4540, FUN_1400b4790, FUN_1400b49f0, FinishTutorialInstance_helper_0d70, GameMain, SaveLoadDialog_OnCreate, TutorialUI_helper_34e0]
??0MainLoop@Engine@@QEAA@PEAV11@@Z                                                                   sites=1 fns=1  [GameMain]
??0SaveLoad@Save@@QEAA@XZ                                                                            sites=2 fns=2  [MainMenuDialog_impl_6b00, OptionsDialog_impl_b930]
??0SavedState@Save@@QEAA@XZ                                                                          sites=2 fns=2  [FUN_14006e9b0, MessageDialog_Ctor]
??0Scene@Audio@@QEAA@XZ                                                                              sites=2 fns=2  [FUN_14009bb00, MainMenuDialog_impl_6b00]
??0Scene@Picking@@QEAA@XZ                                                                            sites=2 fns=2  [FUN_14009ee70, MainMenuDialog_impl_6b00]
??0UpdateLoop@Engine@@QEAA@XZ                                                                        sites=1 fns=1  [FUN_140098060]
??1Achievement@0@UEAA@XZ                                                                             sites=6 fns=6  [Scenario_Ctor, Scenario_ScalarDtor, Unwind@140140d6e, Unwind@140140da3, Unwind@140140de0, Unwind@140140e2c]
??1AchievementInstance@Achievement@@UEAA@XZ                                                          sites=1 fns=1  [FUN_140019d90]
??1DialogLoaderLoopT@Engine@@UEAA@XZ                                                                 sites=10 fns=10  [FUN_140001010, Unwind@14013836a, Unwind@1401383ae, Unwind@140138430, Unwind@140141b1b, Unwind@140141b9b, Unwind@140141c1e, Unwind@14014205f, Unwind@140145075, Unwind@140145676]
??1DialogLoopT@Engine@@UEAA@XZ                                                                       sites=3 fns=3  [FUN_14001a310, Unwind@14013f92a, Unwind@14013f990]
??1Engine@0@QEAA@XZ                                                                                  sites=2 fns=2  [GameMain, Unwind@140138300]
??1Goal@Achievement@@UEAA@XZ                                                                         sites=25 fns=25  [BuildingGoal_Ctor, BuildingGoal_ScalarDtor, FUN_1400daa20, FinishTutorialGoal_Ctor, FinishTutorialGoal_ScalarDtor, FinishTutorialInstance_vslot9, PopulationGoal_Ctor, PopulationGoal_ScalarDtor, TimeGoal_Ctor, TimeGoal_ScalarDtor] ...
??1GoalInstance@Achievement@@UEAA@XZ                                                                 sites=10 fns=10  [FinishTutorialGoal_Slot11, FinishTutorialInstance_Ctor, TimeGoalInstance_Ctor, TimeGoal_Slot11, Unwind@140144222, Unwind@140144230, Unwind@140144438, Unwind@140144446, Unwind@140144460, Unwind@14014447c]
??1LoaderLoop@Engine@@UEAA@XZ                                                                        sites=4 fns=4  [FUN_140001150, FUN_140001190, Unwind@1401383c8, Unwind@140138420]
??1MainLoop@Engine@@QEAA@XZ                                                                          sites=1 fns=1  [GameMain]
??1SaveLoad@Save@@QEAA@XZ                                                                            sites=7 fns=6  [CitizenComponent_impl_77c0, FUN_140097f50, FUN_14009b220, FUN_14009b280, FUN_1400b4790, TutorialUI_helper_34e0]
??1SavedState@Save@@QEAA@XZ                                                                          sites=1 fns=1  [MainMenuDialog_impl_6b00]
??1Scene@Audio@@QEAA@XZ                                                                              sites=4 fns=4  [CitizenComponent_impl_77c0, FUN_14001a310, Unwind@14013f938, Unwind@14013f99e]
??1Scene@Picking@@QEAA@XZ                                                                            sites=4 fns=4  [CitizenComponent_impl_77c0, TradeComponent_impl_ed60, Unwind@14013fa4c, Unwind@14013faaf]
??1UpdateLoop@Engine@@UEAA@XZ                                                                        sites=5 fns=5  [FUN_140019dd0, FUN_140098010, Unwind@14013f586, Unwind@14013f5a6, Unwind@14013f5d0]
??3AchievementInstance@Achievement@@SAXPEAX@Z                                                        sites=1 fns=1  [Unwind@14013f50e]
??3MainLoop@Engine@@SAXPEAX@Z                                                                        sites=1 fns=1  [Unwind@14013830e]
??3Scene@Audio@@SAXPEAX@Z                                                                            sites=1 fns=1  [Unwind@14013f40c]
??3Scene@Picking@@SAXPEAX@Z                                                                          sites=1 fns=1  [Unwind@14013f41a]
?AddEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z                                                 sites=2 fns=2  [FUN_140042c60, FUN_140042e10]
?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z sites=3 fns=3  [Entity_impl_8f50, FUN_14009cc30, FloorComponent_sub_f390]
?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ                                                        sites=4 fns=4  [<data 0x140158858>, <data 0x1401588c8>, <data 0x140158928>, <data 0x14015e348>]
?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z sites=1 fns=1  [TimeGoalInstance_Slot5]
?CheckMod@SaveLoad@Save@@QEBAHHH@Z                                                                   sites=1 fns=1  [MessageDialog_CopyCtor]
?ClearAll@KeyMap@Input@@QEAAXXZ                                                                      sites=1 fns=1  [FUN_1400992f0]
?ClearInput@KeyMap@Input@@QEAAXHH@Z                                                                  sites=1 fns=1  [FUN_1400729a0]
?ClearInput@KeyMap@Input@@QEAAXHHH@Z                                                                 sites=1 fns=1  [FUN_1400729a0]
?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z                                                             sites=1 fns=1  [<data 0x140158820>]
?Draw@LoaderLoop@Engine@@UEAAXM@Z                                                                    sites=2 fns=2  [<data 0x140158890>, <data 0x1401588f0>]
?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z          sites=1 fns=1  [ModDialog_impl_6010]
?DrawFailed@UpdateLoop@Engine@@UEAAXXZ                                                               sites=4 fns=4  [<data 0x140158828>, <data 0x140158898>, <data 0x1401588f8>, <data 0x14015ed28>]
?EnableMouseClipping@Device@Input@@QEAAX_N@Z                                                         sites=3 fns=3  [FUN_140072740, OptionsDialog_helper_2840, OptionsDialog_helper_sub_2360]
?Exit@LoaderLoop@Engine@@QEAAXXZ                                                                     sites=1 fns=1  [FUN_140098da0]
?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z                       sites=2 fns=2  [FUN_140042c60, FUN_140042e10]
?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z     sites=2 fns=2  [AchievementDialog_Serialize, operator_impl_6960]
?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ                                         sites=1 fns=1  [AchievementDialog_Serialize]
?GetDescription@Achievement@1@QEBA?AVString@System@@XZ                                               sites=2 fns=2  [AchievementDialog_Serialize, ScenarioDialog_Serialize]
?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z                                                              sites=5 fns=4  [FUN_140073580, HotKeyToolTipUI_Serialize, TutorialImageHotKey_Update, TutorialImageInputKey_Update]
?GetEmitterCount@Scene@Audio@@QEBAHI@Z                                                               sites=5 fns=5  [AmbientEnvironmentComponent_Update_Impl_sub_2a10, CitizenComponent_helper_a550, FUN_140042c00, FUN_140042d70, FireComponent_Extinguish]
?GetFileVersion@Achievement@1@UEBAHXZ                                                                sites=1 fns=1  [Scenario_Shutdown]
?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z          sites=2 fns=2  [GoalDialog_Draw, GoalDialog_Serialize]
?GetGoalCount@GoalList@Achievement@@QEBAHXZ                                                          sites=1 fns=1  [GoalDialog_Serialize]
?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z                                                         sites=1 fns=1  [FUN_1400729a0]
?GetInputForId@KeyMap@Input@@QEBAHHH@Z                                                               sites=5 fns=4  [FUN_140073580, HotKeyToolTipUI_Serialize, TutorialImageHotKey_Update, TutorialImageInputKey_Update]
?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ                                            sites=3 fns=3  [<data 0x140160348>, <data 0x1401603b8>, <data 0x140160508>]
?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ                                                 sites=10 fns=6  [AutoSaveDialog_Draw, FUN_140077cb0, FinishTutorialInstance_helper_0d70, SaveLoadDialog_Draw, SaveLoadDialog_OnCreate, operator_impl_dec0]
?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ                                                   sites=2 fns=2  [<data 0x140158850>, <data 0x140158920>]
?GetSaveCount@SaveLoad@Save@@QEBAHXZ                                                                 sites=1 fns=1  [SaveLoadDialog_OnCreate]
?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z                                                  sites=7 fns=4  [MessageDialog_CopyCtor, MessageDialog_Ctor4, SaveLoadDialog_Draw, operator_impl_6ce0]
?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ                                                 sites=3 fns=3  [FUN_140077cb0, FinishTutorialInstance_helper_0d70, SaveLoadDialog_OnCreate]
?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ                                                sites=2 fns=2  [AchievementDialog_Serialize, ScenarioDialog_Serialize]
?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ                                                  sites=1 fns=1  [GoalDialog_Serialize]
?GetTitle@Achievement@1@QEBA?AVString@System@@XZ                                                     sites=5 fns=5  [AchievementDialog_Serialize, GoalDialog_Serialize, ScenarioDialog_Serialize, operator_impl_6830, operator_impl_6960]
?GetUserData@Scene@Picking@@QEBAPEAXI@Z                                                              sites=2 fns=2  [EditTool_Draw, PathTool_Draw]
?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z                                                   sites=6 fns=1  [SpriteCaptureUI_OnRemove]
?GetValue@KeyMap@Input@@QEBAFHH@Z                                                                    sites=23 fns=6  [BridgeTool_Draw, EntityUI_Slot8_PerFrameInput, FUN_1401005a0, MainMenuDialog_helper_eec0, RoadTool_Draw, ToolUI_OnRemove]
?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ                                                 sites=1 fns=1  [FUN_140096380]
?HasFailed@GoalInstance@Achievement@@QEBA_NXZ                                                        sites=1 fns=1  [GoalDialog_Draw]
?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z                            sites=3 fns=3  [CharacterComponent_impl_f3e0, EditTool_Draw, PathTool_Draw]
?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z                                      sites=1 fns=1  [AchievementDialog_Draw]
?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z                                                              sites=1 fns=1  [FUN_1400a40e0]
?IsFinished@LoaderLoop@Engine@@QEBA_NXZ                                                              sites=1 fns=1  [FUN_140098da0]
?IsMet@AchievementInstance@Achievement@@QEBA_NXZ                                                     sites=3 fns=2  [FUN_140096380, operator_impl_6830]
?IsMet@GoalInstance@Achievement@@QEBA_NXZ                                                            sites=1 fns=1  [GoalDialog_Draw]
?IsPaused@Application@1@QEAA_NXZ                                                                     sites=2 fns=2  [MainMenuDialog_impl_26a0, MainMenuDialog_impl_b6e0]
?IsRunning@UpdateLoop@Engine@@QEBA_NXZ                                                               sites=1 fns=1  [PopulationGoal_OnRemove]
?Load@Configuration@Input@@QEAAXXZ                                                                   sites=2 fns=2  [FUN_140072740, OptionsDialog_helper_2840]
?Load@DialogLoaderLoopT@Engine@@UEAAXXZ                                                              sites=1 fns=1  [<data 0x140158830>]
?Load@DialogLoopT@Engine@@UEAAXXZ                                                                    sites=1 fns=1  [MenuScene_impl_a930]
?Load@KeyMap@Input@@QEAAXXZ                                                                          sites=1 fns=1  [FUN_1400992f0]
?Load@LoaderLoop@Engine@@UEAAXXZ                                                                     sites=1 fns=1  [<data 0x140158900>]
?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z                                                sites=4 fns=4  [CitizenComponent_helper_a710, ComponentEntity_impl_02f0, DebugUI_OnDrawComboChanged, FUN_140042960]
?OnVideoError@LoaderLoop@Engine@@UEAAXXZ                                                             sites=3 fns=3  [<data 0x140158840>, <data 0x1401588b0>, <data 0x140158910>]
?OnVideoError@MainLoop@Engine@@QEAAXXZ                                                               sites=1 fns=1  [OptionsDialog_helper_sub_1e10]
?OnVideoRestart@MainLoop@Engine@@QEAAXXZ                                                             sites=1 fns=1  [OptionsDialog_helper_sub_1e10]
?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ                                                           sites=3 fns=3  [<data 0x140158848>, <data 0x1401588b8>, <data 0x140158918>]
?Pause@Scene@Audio@@QEAAXH_N@Z                                                                       sites=10 fns=4  [MainMenuDialog_Serialize, MainMenuDialog_Slot5, MainMenuDialog_impl_6b00, MainMenuDialog_impl_b6e0]
?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z                                     sites=1 fns=1  [<data 0x14015e368>]
?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z                                      sites=1 fns=1  [TradeComponent_impl_5a90]
?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z                                            sites=2 fns=2  [FinishTutorialGoal_Slot14, TimeGoal_Slot14]
?PlayEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@AEBVVector3@Math@@@Z sites=3 fns=3  [FUN_140042c60, FUN_140042e10, FUN_140087e40]
?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z sites=6 fns=6  [EventNotifyDialog_OnCreate, FUN_1400a40e0, FUN_1400ffbf0, FUN_1401000f0, FUN_140100140, OptionsDialog_impl_b930]
?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z                                                   sites=1 fns=1  [SpriteCaptureUI_OnRemove]
?Pressed@KeyMap@Input@@QEBA_NHH@Z                                                                    sites=21 fns=10  [BridgeTool_Draw, EditTool_Draw, EntityUI_Slot8_PerFrameInput, FUN_140096380, FollowDialog_OnCreate, MainMenuDialog_helper_eec0, MainMenuDialog_helper_f6e0, PathTool_Draw, PlacementTool_Draw, TimeGoalInstance_Slot13]
?Quit@Application@1@QEAAXXZ                                                                          sites=1 fns=1  [MainMenuDialog_OnRemove]
?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z                                                        sites=50 fns=1  [FUN_1400992f0]
?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z                                                  sites=2 fns=1  [OptionInputBindDialog_OnCreate]
?Released@KeyMap@Input@@QEBA_NHH@Z                                                                   sites=2 fns=1  [BridgeTool_Draw]
?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z                                               sites=5 fns=5  [AmbientEnvironmentComponent_Update_Impl_sub_2a10, CitizenComponent_helper_a550, FUN_140042c00, FUN_140042d70, FireComponent_Extinguish]
?RemoveMesh@Scene@Picking@@QEAAXI@Z                                                                  sites=2 fns=2  [FUN_14009cba0, PickingComponent_OnCreate]
?Reset@AchievementManager@Achievement@@QEAAXXZ                                                       sites=1 fns=1  [MainMenuDialog_impl_6b00]
?ResetState@Device@Input@@QEAAXXZ                                                                    sites=1 fns=1  [FUN_1400729a0]
?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z                                              sites=1 fns=1  [TradeComponent_impl_59c0]
?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z                                                       sites=1 fns=1  [GameMain]
?Save@Configuration@Audio@@QEAAXXZ                                                                   sites=2 fns=2  [OptionsDialog_helper_28e0, OptionsDialog_helper_2930]
?Save@Configuration@Input@@QEAAXXZ                                                                   sites=1 fns=1  [OptionsDialog_helper_sub_2360]
?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z                                       sites=4 fns=4  [<data 0x140158838>, <data 0x1401588a8>, <data 0x140158908>, <data 0x14015ed38>]
?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z                                                     sites=1 fns=1  [Scenario_Slot5]
?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z                                                 sites=5 fns=5  [BuildingGoal_Slot5, FinishTutorialGoal_Slot5, FinishTutorialInstance_vslot14, PopulationGoal_Slot5, TimeGoal_Slot5]
?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z                                                   sites=1 fns=1  [TradeComponent_impl_5a90]
?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z      sites=1 fns=1  [MainMenuDialog_impl_6b00]
?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z sites=1 fns=1  [GameContainer_Init_99db0]
?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z                                           sites=7 fns=7  [AmbientEnvironmentComponent_Update_Impl_sub_2a10, CitizenComponent_helper_a710, ComponentEntity_impl_42c0, DebugUI_OnDrawComboChanged, ResidenceDescription_helper_a350_sub_9970, TornadoComponent_OnRemove, Weather_ResetTime]
?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z                                      sites=2 fns=2  [CitizenComponent_impl_77c0, MainMenuDialog_impl_6b00]
?SetInput@KeyMap@Input@@QEAAXHHHH@Z                                                                  sites=1 fns=1  [FUN_1400729a0]
?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z                          sites=2 fns=2  [BridgeTool_Slot13, FencedTool_helper_90d0]
?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z                                         sites=4 fns=4  [MessageDialog_CopyCtor, MessageDialog_Ctor4, SaveLoadDialog_Serialize, operator_impl_dec0]
?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z                                               sites=2 fns=2  [FUN_140096380, MainMenuDialog_impl_b6e0]
?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z                                                 sites=8 fns=8  [FUN_140077cb0, FinishTutorialInstance_helper_0d70, MessageDialog_CopyCtor, MessageDialog_Ctor3, MessageDialog_Ctor4, SaveLoadDialog_OnCreate, SaveLoadDialog_Serialize, operator_impl_dec0]
?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z                                              sites=4 fns=4  [FUN_140077cb0, FinishTutorialInstance_helper_0d70, MessageDialog_Ctor4, operator_impl_dec0]
?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z                                                    sites=2 fns=2  [SaveLoadDialog_Serialize, operator_impl_dec0]
?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z                                            sites=1 fns=1  [CharacterComponent_OnAttach]
?Shutdown@DialogLoopT@Engine@@UEAAXXZ                                                                sites=1 fns=1  [FUN_14009b280]
?Shutdown@Engine@1@QEAAXXZ                                                                           sites=2 fns=1  [GameMain]
?Shutdown@LoaderLoop@Engine@@UEAAXXZ                                                                 sites=2 fns=2  [<data 0x140158880>, <data 0x1401588e0>]
?Startup@Configuration@Audio@@QEAA_NXZ                                                               sites=2 fns=2  [FUN_1400727a0, OptionsDialog_helper_2840]
?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ                                                           sites=1 fns=1  [OptionsDialog_impl_8fa0]
?Startup@DialogLoopT@Engine@@UEAAXXZ                                                                 sites=1 fns=1  [OptionsDialog_impl_b930]
?Startup@Engine@1@QEAAIPEBDII_N@Z                                                                    sites=1 fns=1  [GameMain]
?Startup@LoaderLoop@Engine@@UEAAXXZ                                                                  sites=2 fns=2  [<data 0x140158878>, <data 0x1401588d8>]
?StopAll@Scene@Audio@@QEAAXXZ                                                                        sites=2 fns=2  [CitizenComponent_impl_77c0, FUN_14009b280]
?StopEffect3D@Scene@Audio@@QEAAXI@Z                                                                  sites=5 fns=5  [AmbientEnvironmentComponent_Update_Impl_sub_2a10, CitizenComponent_helper_a550, FUN_140042c00, FUN_140042d70, FireComponent_Extinguish]
?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z                                                 sites=1 fns=1  [TradeComponent_impl_59c0]
?Type@AchievementInstance@Achievement@@UEBAIXZ                                                       sites=1 fns=1  [<data 0x14015e360>]
?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ                                                sites=1 fns=1  [<data 0x14015e358>]
?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z                                    sites=1 fns=1  [FUN_140096380]
?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z                                     sites=1 fns=1  [FUN_140096380]
?Update@DialogLoaderLoopT@Engine@@UEAA_NAEBVTime@System@@@Z                                          sites=1 fns=1  [FUN_140098da0]
?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z                                                sites=1 fns=1  [MainMenuDialog_impl_b6e0]
?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z                            sites=1 fns=1  [TimeGoalInstance_Slot5]
?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z                                                 sites=2 fns=2  [<data 0x140158888>, <data 0x1401588e8>]
?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z                                                      sites=4 fns=4  [FUN_140077cb0, FUN_140096380, FinishTutorialInstance_helper_0d70, MainMenuDialog_impl_b6e0]
?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z                         sites=2 fns=2  [FUN_140096380, MainMenuDialog_impl_b6e0]
?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z                       sites=6 fns=2  [QuarryComponent_sub_49c0, TerraformComponent_Slot12_sub_6b50]
?YieldToThreads@Application@1@QEAAXXZ                                                                sites=3 fns=3  [FUN_1400a6690, PathTool_helper_d770, RoadTool_helper_6e30]
per-namespace distinct imports: {Achievement=39, Application=3, Audio=19, Engine=41, Input=19, Picking=10, Save=17}
per-namespace imports with >=1 site: {Achievement=39, Application=3, Audio=19, Engine=41, Input=19, Picking=10, Save=17}
=== SCRIPT COMPLETED SUCCESSFULLY ===
```

## §T — tree side: the hex-literal sweep (B36_RULES 14.1 shape)

Every `0x18xxxxxxx` literal in `code/ engine/ standalone/ runtime/ dx11/ tests/` (build dirs, `rt_export_addrs.inc` and
`rt_data_mint_tables.inc` excluded; `//` comments stripped) matched against `standalone/rt_export_addrs.inc`, and every
`0x14xxxxxxx` literal matched against the exe's PE import table, restricted to exports whose outermost qualifier is one of
the eight namespaces (back-references resolved). Each hit classified by its line: a registration row
(`RegisterRuntimeAddr`/`RegisterDispatchSlot`/`RegisterGoalSlot`/...), an `RT_DELDTOR` row, a BY-ADDRESS RESOLVE
(`ResolveAddr`/`RtAddr`/`resolveIAT`/`RSlot`/`ResolveSlot`), or a named constant. The by-address resolves are the
caller sites (spelling 3 / spelling 10 / spelling 11). The dx11 Application rows do NOT appear: 0x180013290.. are
VideoDX11-x64.dll's own IAT slots (see B37_RULES §4).

```
Engine: RT_DELDTOR row 4, by-address resolve 7, named constant 67, registration 52
    code/Game/Game/game_main.cpp:184  0x1800dd6b0  ??0MainLoop@Engine@@QEAA@PEAV11@@Z
    code/Game/Game/game_main.cpp:186  0x1800dd640  ??1MainLoop@Engine@@QEAA@XZ
    code/Game/Game/game_main.cpp:188  0x1800dd3c0  ?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z
    engine/funcs/engine_funcs.cpp:1262  0x140151440  ?_singleton@MainLoop@Engine@@1PEAV12@EA
    engine/funcs/engine_funcs.cpp:2404  0x1801fe7b8  ?_loader@LoaderLoop@Engine@@1VLoader@2@A
    engine/funcs/engine_funcs.cpp:2843  0x140152850  ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ
    runtime/src/engine/mainloop_video.cpp:717  0x1800dd500  ?CreateVideoContext@MainLoop@Engine@@AEAAXXZ
Save: by-address resolve 5, registration 21
    engine/funcs/engine_funcs.cpp:1341  0x140151610  ?_singleton@SaveLoad@Save@@0PEAV12@EA
    engine/funcs/engine_funcs.cpp:1364  0x140151658  ?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z
    engine/funcs/engine_funcs.cpp:1419  0x140151d20  ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
    engine/funcs/engine_funcs.cpp:2679  0x140152738  ?CheckMod@SaveLoad@Save@@QEBAHHH@Z
    engine/funcs/engine_funcs.cpp:2692  0x140151650  ?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z
Achievement: RT_DELDTOR row 4, by-address resolve 17, comment/string/other 1, named constant 7, registration 50
    code/Game/Game/gameupdateloop.cpp:536  0x140152058  ?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z
    code/Game/Game/gameupdateloop.cpp:541  0x140152048  ?IsMet@AchievementInstance@Achievement@@QEBA_NXZ
    code/Game/Game/gameupdateloop.cpp:546  0x140152060  ?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ
    code/Game/Game/scenario.cpp:75  0x18009e240  ??0Achievement@0@QEAA@XZ
    code/Game/Game/scenario.cpp:77  0x18009e180  ??1Achievement@0@UEAA@XZ
    code/Game/Game/scenario.cpp:79  0x18009e170  ?GetFileVersion@Achievement@1@UEBAHXZ
    code/Game/Game/scenario.cpp:81  0x18009e070  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z
    code/Game/Goal/statgoal.cpp:66  0x140152840  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
    code/Game/Goal/timegoal.cpp:34  0x140152840  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
    code/Game/Goal/timegoalinstance.cpp:58  0x140152868  ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
    code/Game/Goal/timegoalinstance.cpp:64  0x140152878  ?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z
    code/Game/Goal/timegoalinstance.cpp:67  0x140152880  ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
    engine/funcs/engine_funcs.cpp:1459  0x1401514f0  ?_singleton@AchievementManager@Achievement@@0PEAV12@EA
    engine/funcs/engine_funcs.cpp:1533  0x140151598  ?GetTitle@Achievement@1@QEBA?AVString@System@@XZ
    engine/funcs/engine_funcs.cpp:1539  0x140151590  ?GetDescription@Achievement@1@QEBA?AVString@System@@XZ
    engine/funcs/engine_funcs.cpp:1545  0x1401515a0  ?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ
    engine/funcs/engine_funcs.cpp:2827  0x140152840  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
Audio: RT_DELDTOR row 2, by-address resolve 11, comment/string/other 1, registration 14
    code/Game/Component/citizen_assignworkplace.cpp:133  0x140152a00  ?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z
    code/Game/Component/citizen_assignworkplace.cpp:134  0x140152a08  ?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z
    code/Game/Component/citizen_assignworkplace.cpp:135  0x140152a10  ?StopEffect3D@Scene@Audio@@QEAAXI@Z
    code/Game/Component/citizen_assignworkplace.cpp:136  0x140152a18  ?GetEmitterCount@Scene@Audio@@QEBAHI@Z
    code/Game/Component/citizen_assignworkplace.cpp:137  0x140152a20  ?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z
    code/Game/Tutorial/finishtutorialinstance.cpp:72  0x140151c08  ?Save@Configuration@Audio@@QEAAXXZ
    engine/funcs/engine_funcs.cpp:1403  0x140152030  ?Pause@Scene@Audio@@QEAAXH_N@Z
    engine/funcs/engine_funcs.cpp:1697  0x140151b78  ?_singleton@Configuration@Audio@@0PEAV12@EA
    engine/funcs/engine_funcs.cpp:1708  0x1801fe758  ?_singleton@Configuration@Audio@@0PEAV12@EA
    runtime/src/audio/audio_stream_win.cpp:296  0x180154950  ??_7Stream@Audio@@6B@
    runtime/src/engine/mainloop_run.cpp:282  0x1800de520  ?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z
Input: by-address resolve 8, comment/string/other 2, registration 32
    code/Game/Player/edittool.cpp:212  0x140151758  ?Pressed@KeyMap@Input@@QEBA_NHH@Z
    code/Game/Player/pathtool.cpp:117  0x140151758  ?Pressed@KeyMap@Input@@QEBA_NHH@Z
    code/Game/Player/placementtool.cpp:146  0x140151758  ?Pressed@KeyMap@Input@@QEBA_NHH@Z
    engine/funcs/engine_funcs.cpp:1259  0x140151738  ?_singleton@Configuration@Input@@0PEAV12@EA
    engine/funcs/engine_funcs.cpp:1386  0x140151b80  ?_singleton@Device@Input@@0PEAV12@EA
    engine/funcs/engine_funcs.cpp:1470  0x140151758  ?Pressed@KeyMap@Input@@QEBA_NHH@Z
    engine/funcs/engine_funcs.cpp:1856  0x1401528f0  ?Released@KeyMap@Input@@QEBA_NHH@Z
    runtime/src/input/input.cpp:559  0x1800a7520  ?Save@KeyMap@Input@@QEAAXXZ
Pathfinding: RT_DELDTOR row 1
Picking: RT_DELDTOR row 1, by-address resolve 15, comment/string/other 2, registration 14
    code/Game/Component/citizenai_executor.cpp:369  0x1401529a8  ?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z
    code/Game/Component/citizenai_executor.cpp:372  0x1801252e0  ?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z
    code/Game/Component/floorcomponent.cpp:249  0x1801252e0  ?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z
    code/Game/Component/floorcomponent.cpp:254  0x180125290  ?RemoveMesh@Scene@Picking@@QEAAXI@Z
    code/Game/Component/pickingcomponent.cpp:312  0x180125290  ?RemoveMesh@Scene@Picking@@QEAAXI@Z
    code/Game/Component/pickingcomponent.cpp:314  0x140152438  ?RemoveMesh@Scene@Picking@@QEAAXI@Z
    code/Game/Component/quarrycomponent_deform.cpp:93  0x140151478  ?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z
    code/Game/Component/terraformcomponent.cpp:307  0x140151478  ?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z
    code/Game/Game/mapquery.cpp:154  0x140152428  ?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z
    code/Game/Player/bridgetool_calcpath.cpp:226  0x1401529a0  ?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z
    code/Game/Player/edittool.cpp:213  0x140152428  ?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z
    code/Game/Player/edittool.cpp:214  0x140152890  ?GetUserData@Scene@Picking@@QEBAPEAXI@Z
    code/Game/Player/fencedtool_calcpath.cpp:92  0x1401529a0  ?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z
    code/Game/Player/pathtool.cpp:109  0x140152428  ?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z
    code/Game/Player/pathtool.cpp:113  0x140152890  ?GetUserData@Scene@Picking@@QEBAPEAXI@Z
Application: by-address resolve 3, registration 26
    engine/funcs/engine_funcs.cpp:2411  0x140152368  ?_singleton@Application@1@0PEAV11@EA
    standalone/address_registry.cpp:1544  0x1801fc678  ?_singleton@Application@1@0PEAV11@EA
    runtime/src/engine/mainloop_run.cpp:284  0x18009fdf0  ?PlatformUpdate@Application@1@AEAA_NXZ
```
