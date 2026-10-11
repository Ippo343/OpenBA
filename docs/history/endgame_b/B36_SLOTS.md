# B36_SLOTS.md — raw appendix for B.3 step 6 (`UI`)

Produced 2026-09-25 by block 0 from the RETAIL `Runtime-x64.dll` (asserted before the read:
15290 functions, image base 0x180000000, `get_current_program_info`) by
`b36_session/ghidra/UiAppendix.java`, run through GhidraMCP. The complete console output,
including the per-vtable referencer lists and the ctor/dtor stamp traces, is checked in beside
it as `b36_session/ui_appendix_raw.txt`; this file is the same data with the two long sections
condensed. Copy from here; do not retype. Rules and verdicts are in `B36_RULES.md`.

How each fact was read (same instruments as `B35_SLOTS.md`):
- slot targets: `read_memory` qwords over `.rdata`, terminating on ANY `??_7`/`??_R` symbol,
  a zero qword, or a non-code target (step 5's reader stopped only on the class set's own
  vtables and overran `StringTable`, `ToolTipDialog` and `Object::Internal` into their
  neighbours on the first run; the fix is in the script);
- `sizeof`: the `MOV EDX,imm` in the class's own slot-0 vector deleting destructor, read as
  DISASSEMBLY;
- parent: the string literal `?_ParentTypeName@X@UI@@CAPEBDXZ` returns, read byte-wise, and
  qualified with `UI::` when bare. ALL 68 classes carry one. `inherited-from P` marks a slot
  whose target is byte-identical to the parent's at the same index;
- `(unexported) FUN_x`: no `?`-mangled symbol at the address (every slot 0);
- `_purecall` is 0x18007ec28;
- section 7 lists every `??_7` LEA inside every `??0`/`??1` of the 68 classes with the
  instruction that follows it (the store). 364 stamps, 364 of them `MOV qword ptr [reg],RAX`
  at offset 0 of `this`: NO SECONDARY VPTR anywhere in `UI`.


@@@@@@ SECTION 1 -- 68 minted-vtable classes (parent = _ParentTypeName literal, namespace-qualified by the rule 'bare means same namespace')



### UI::AnimatedImageDescription  vtable=0x180156060  slots=10  sizeof=0x98  parent=UI::ImageDescription

  [ 0] +0x00 0x1800369e0  (unexported) FUN_1800369e0

  [ 1] +0x08 0x1800366d0  ?TypeName@AnimatedImageDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800366b0  ?Type@AnimatedImageDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800cb6a0  ?GetFileVersion@AnimatedImageDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ImageDescription

  [ 5] +0x28 0x1800cb5c0  ?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180036850  ?Reconstruct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180036880  ?Destruct@AnimatedImageDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800cb480  ?CreateInstance@AnimatedImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180036870  ?Construct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::AnimatedImageElement  vtable=0x180156000  slots=12  sizeof=0x100  parent=UI::ImageElement

  [ 0] +0x00 0x180036db0  (unexported) UI_AnimatedImageElement_VectorDeletingDtor

  [ 1] +0x08 0x180036ab0  ?TypeName@AnimatedImageElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180036a90  ?Type@AnimatedImageElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::ImageElement

  [ 4] +0x20 0x180118650  ?ComputeSize@ImageElement@UI@@UEAAXXZ   inherited-from UI::ImageElement

  [ 5] +0x28 0x1801186f0  ?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z   inherited-from UI::ImageElement

  [ 6] +0x30 0x180118850  ?Draw@ImageElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   inherited-from UI::ImageElement

  [ 7] +0x38 0x1800cb290  ?Update@AnimatedImageElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::ImageElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::ImageElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::ImageElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::ImageElement



### UI::BorderDescription  vtable=0x180156980  slots=10  sizeof=0xb8  parent=UI::ElementDescription

  [ 0] +0x00 0x180040ff0  (unexported) UI_BorderDescription_VectorDeletingDtor

  [ 1] +0x08 0x1800409e0  ?TypeName@BorderDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800409c0  ?Type@BorderDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x18010fe10  ?GetFileVersion@BorderDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x18010fe20  ?Serialize@BorderDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180040b60  ?Reconstruct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180040b90  ?Destruct@BorderDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x18010ff40  ?CreateInstance@BorderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180040b80  ?Construct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::BorderElement  vtable=0x180156920  slots=12  sizeof=0x150  parent=UI::Element

  [ 0] +0x00 0x1800414f0  (unexported) UI_BorderElement_VectorDeletingDtor

  [ 1] +0x08 0x1800410b0  ?TypeName@BorderElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180041090  ?Type@BorderElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1801101d0  ?ComputeSize@BorderElement@UI@@UEAAXXZ

  [ 5] +0x28 0x180110360  ?ComputeLayout@BorderElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1801103e0  ?Draw@BorderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ButtonDescription  vtable=0x180157370  slots=10  sizeof=0xa0  parent=UI::ElementDescription

  [ 0] +0x00 0x1800481a0  (unexported) UI_ButtonDescription_VectorDeletingDtor

  [ 1] +0x08 0x180047e30  ?TypeName@ButtonDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180047e10  ?Type@ButtonDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1801113d0  ?GetFileVersion@ButtonDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1801113e0  ?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180047fb0  ?Reconstruct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180047fe0  ?Destruct@ButtonDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180111510  ?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180047fd0  ?Construct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ButtonElement  vtable=0x180157310  slots=12  sizeof=0xd8  parent=UI::Element

  [ 0] +0x00 0x1800485b0  (unexported) UI_ButtonElement_VectorDeletingDtor

  [ 1] +0x08 0x180048260  ?TypeName@ButtonElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180048240  ?Type@ButtonElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x180110d10  ?ComputeSize@ButtonElement@UI@@UEAAXXZ

  [ 5] +0x28 0x180110e70  ?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x180111020  ?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1801110b0  ?Update@ButtonElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180110cc0  ?Enable@ButtonElement@UI@@UEAAX_N@Z

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::CharacterSet  vtable=0x180156220  slots=9  sizeof=0x28  parent=Object::External

  [ 0] +0x00 0x1800378a0  (unexported) FUN_1800378a0

  [ 1] +0x08 0x180037450  ?TypeName@CharacterSet@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180037430  ?Type@CharacterSet@UI@@UEBAIXZ

  [ 3] +0x18 0x1800cb830  ?GetFileVersion@CharacterSet@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x1800cb7b0  ?Serialize@CharacterSet@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180037650  ?Reconstruct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180037710  ?Destruct@CharacterSet@UI@@UEAAXXZ

  [ 8] +0x40 0x1800376b0  ?Construct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ



### UI::CheckDescription  vtable=0x1801572c0  slots=10  sizeof=0xa8  parent=UI::ButtonDescription

  [ 0] +0x00 0x1800489c0  (unexported) UI_CheckDescription_VectorDeletingDtor

  [ 1] +0x08 0x180048680  ?TypeName@CheckDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180048660  ?Type@CheckDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x180111920  ?GetFileVersion@CheckDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ButtonDescription

  [ 5] +0x28 0x180111930  ?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180048850  ?Reconstruct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x1800488f0  ?Destruct@CheckDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1801119c0  ?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x1800488a0  ?Construct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::CheckElement  vtable=0x180157260  slots=12  sizeof=0xe0  parent=UI::ButtonElement

  [ 0] +0x00 0x180048d80  (unexported) UI_CheckElement_VectorDeletingDtor

  [ 1] +0x08 0x180048a80  ?TypeName@CheckElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180048a60  ?Type@CheckElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::ButtonElement

  [ 4] +0x20 0x180111740  ?ComputeSize@CheckElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1801117b0  ?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x180111800  ?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180111870  ?Update@CheckElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180110cc0  ?Enable@ButtonElement@UI@@UEAAX_N@Z   inherited-from UI::ButtonElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::ButtonElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::ButtonElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::ButtonElement



### UI::ClipRegionDescription  vtable=0x180156350  slots=10  sizeof=0x78  parent=UI::ElementDescription

  [ 0] +0x00 0x180037eb0  (unexported) UI_ClipRegionDescription_VectorDeletingDtor

  [ 1] +0x08 0x180037990  ?TypeName@ClipRegionDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180037970  ?Type@ClipRegionDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800cc0c0  ?GetFileVersion@ClipRegionDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800cc010  ?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180037c30  ?Reconstruct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180037d90  ?Destruct@ClipRegionDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800cbec0  ?CreateInstance@ClipRegionDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180037ce0  ?Construct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ClipRegionElement  vtable=0x1801562f0  slots=12  sizeof=0xd8  parent=UI::Element

  [ 0] +0x00 0x180038370  (unexported) UI_ClipRegionElement_VectorDeletingDtor

  [ 1] +0x08 0x180037f70  ?TypeName@ClipRegionElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180037f50  ?Type@ClipRegionElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800cbc60  ?ComputeSize@ClipRegionElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800cba00  ?ComputeLayout@ClipRegionElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800cb900  ?Draw@ClipRegionElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800cbd70  ?Update@ClipRegionElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1800cbcc0  ?Contains@ClipRegionElement@UI@@UEBA_NHHPEAVUserInterface@2@@Z

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ComboDescription  vtable=0x180159090  slots=10  sizeof=0xb8  parent=UI::ButtonDescription

  [ 0] +0x00 0x180074e70  (unexported) UI_ComboDescription_VectorDeletingDtor

  [ 1] +0x08 0x180074ae0  ?TypeName@ComboDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180074ac0  ?Type@ComboDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1801129b0  ?GetFileVersion@ComboDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ButtonDescription

  [ 5] +0x28 0x1801129c0  ?Serialize@ComboDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180074cc0  ?Reconstruct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180074d70  ?Destruct@ComboDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180112a70  ?CreateInstance@ComboDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180074d20  ?Construct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ComboDialog  vtable=0x1801590e0  slots=10  sizeof=0xd0  parent=UI::DialogInstance

  [ 0] +0x00 0x180074a10  (unexported) FUN_180074a10

  [ 1] +0x08 0x180074760  ?TypeName@ComboDialog@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180074740  ?Type@ComboDialog@UI@@UEBAIXZ

  [ 3] +0x18 0x180114fa0  ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::DialogInstance

  [ 4] +0x20 0x180114fb0  ?OnOpened@DialogInstance@UI@@UEAAXXZ   inherited-from UI::DialogInstance

  [ 5] +0x28 0x180115330  ?OnClosed@DialogInstance@UI@@UEAAXXZ   inherited-from UI::DialogInstance

  [ 6] +0x30 0x180111cb0  ?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ

  [ 7] +0x38 0x180111bd0  ?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z

  [ 8] +0x40 0x180111af0  ?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

  [ 9] +0x48 0x180115470  ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::DialogInstance



### UI::ComboElement  vtable=0x180159030  slots=12  sizeof=0x128  parent=UI::ButtonElement

  [ 0] +0x00 0x180075390  (unexported) UI_ComboElement_VectorDeletingDtor

  [ 1] +0x08 0x180074f30  ?TypeName@ComboElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180074f10  ?Type@ComboElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::ButtonElement

  [ 4] +0x20 0x180110d10  ?ComputeSize@ButtonElement@UI@@UEAAXXZ   inherited-from UI::ButtonElement

  [ 5] +0x28 0x180110e70  ?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z   inherited-from UI::ButtonElement

  [ 6] +0x30 0x180111020  ?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   inherited-from UI::ButtonElement

  [ 7] +0x38 0x1801125d0  ?Update@ComboElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180110cc0  ?Enable@ButtonElement@UI@@UEAAX_N@Z   inherited-from UI::ButtonElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::ButtonElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::ButtonElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::ButtonElement



### UI::ContainerDescription  vtable=0x180156510  slots=10  sizeof=0x90  parent=UI::ElementDescription

  [ 0] +0x00 0x180039c00  (unexported) UI_ContainerDescription_VectorDeletingDtor

  [ 1] +0x08 0x1800398d0  ?TypeName@ContainerDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800398b0  ?Type@ContainerDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800ccfe0  ?GetFileVersion@ContainerDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800cced0  ?Serialize@ContainerDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180039a50  ?Reconstruct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180039a80  ?Destruct@ContainerDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800ccb90  ?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180039a70  ?Construct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ContainerElement  vtable=0x1801564b0  slots=12  sizeof=0xd8  parent=UI::Element

  [ 0] +0x00 0x18003a050  (unexported) UI_ContainerElement_VectorDeletingDtor

  [ 1] +0x08 0x180039cc0  ?TypeName@ContainerElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180039ca0  ?Type@ContainerElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800cc850  ?ComputeSize@ContainerElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800cc620  ?ComputeLayout@ContainerElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800cc590  ?Draw@ContainerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800cc2d0  ?Update@ContainerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::Cursor  vtable=0x180156570  slots=9  sizeof=0x30  parent=Object::External

  [ 0] +0x00 0x18003b3a0  (unexported) FUN_18003b3a0

  [ 1] +0x08 0x18003af80  ?TypeName@Cursor@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003af60  ?Type@Cursor@UI@@UEBAIXZ

  [ 3] +0x18 0x1800cd1b0  ?GetFileVersion@Cursor@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x1800cd110  ?Serialize@Cursor@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18003b180  ?Reconstruct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18003b240  ?Destruct@Cursor@UI@@UEAAXXZ

  [ 8] +0x40 0x18003b1e0  ?Construct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ



### UI::Dialog  vtable=0x1801562a0  slots=9  sizeof=0x88  parent=Object::External

  [ 0] +0x00 0x180038b20  (unexported) FUN_180038b20

  [ 1] +0x08 0x180038440  ?TypeName@Dialog@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180038420  ?Type@Dialog@UI@@UEBAIXZ

  [ 3] +0x18 0x1801130b0  ?GetFileVersion@Dialog@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x1801130c0  ?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800386b0  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x1800387f0  ?Destruct@Dialog@UI@@UEAAXXZ

  [ 8] +0x40 0x180038750  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ



### UI::DialogController  vtable=0x1801565f0  slots=11  sizeof=0x30  parent=Object::Internal

  [ 0] +0x00 0x18003b800  (unexported) FUN_18003b800

  [ 1] +0x08 0x18003b6d0  ?TypeName@DialogController@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003b6b0  ?Type@DialogController@UI@@UEBAIXZ

  [ 3] +0x18 0x1800bfda0  ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z   inherited-from Object::Internal

  [ 4] +0x20 0x1800cd2b0  ?OnCreated@DialogController@UI@@UEAAXXZ

  [ 5] +0x28 0x1800cd2a0  ?OnOpened@DialogController@UI@@UEAAXXZ

  [ 6] +0x30 0x1800cd290  ?OnClosed@DialogController@UI@@UEAAXXZ

  [ 7] +0x38 0x1800cd280  ?OnUpdateLayout@DialogController@UI@@UEAAXXZ

  [ 8] +0x40 0x1800cd270  ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z

  [ 9] +0x48 0x1800cd260  ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

  [10] +0x50 0x1800cd250  ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z



### UI::DialogControllerConfig  vtable=0x180156650  slots=8  sizeof=0x20  parent=Object::External

  [ 0] +0x00 0x18003b600  (unexported) FUN_18003b600

  [ 1] +0x08 0x18003b470  ?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003b450  ?Type@DialogControllerConfig@UI@@UEBAIXZ

  [ 3] +0x18 0x18007ec28  _purecall   inherited-from Object::External

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x18007ec28  _purecall   inherited-from Object::External

  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External

  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External



### UI::DialogInstance  vtable=0x180156d70  slots=10  sizeof=0xb8  parent=Object::Internal

  [ 0] +0x00 0x1800458c0  (unexported) FUN_1800458c0

  [ 1] +0x08 0x180045400  ?TypeName@DialogInstance@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800453e0  ?Type@DialogInstance@UI@@UEBAIXZ

  [ 3] +0x18 0x180114fa0  ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z

  [ 4] +0x20 0x180114fb0  ?OnOpened@DialogInstance@UI@@UEAAXXZ

  [ 5] +0x28 0x180115330  ?OnClosed@DialogInstance@UI@@UEAAXXZ

  [ 6] +0x30 0x1801153a0  ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ

  [ 7] +0x38 0x180115400  ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z

  [ 8] +0x40 0x180114ed0  ?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

  [ 9] +0x48 0x180115470  ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z



### UI::DocumentTextDescription  vtable=0x180156750  slots=10  sizeof=0xe8  parent=UI::ElementDescription

  [ 0] +0x00 0x18003bd90  (unexported) UI_DocumentTextDescription_VectorDeletingDtor

  [ 1] +0x08 0x18003b900  ?TypeName@DocumentTextDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003b8e0  ?Type@DocumentTextDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800cfb20  ?GetFileVersion@DocumentTextDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800cf8a0  ?Serialize@DocumentTextDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18003ba80  ?Reconstruct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18003bab0  ?Destruct@DocumentTextDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800cf7e0  ?CreateInstance@DocumentTextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18003baa0  ?Construct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::DocumentTextElement  vtable=0x1801566f0  slots=12  sizeof=0x1c8  parent=UI::Element

  [ 0] +0x00 0x18003c650  (unexported) UI_DocumentTextElement_VectorDeletingDtor

  [ 1] +0x08 0x18003be50  ?TypeName@DocumentTextElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003be30  ?Type@DocumentTextElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800cdb60  ?ComputeSize@DocumentTextElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800cda00  ?ComputeLayout@DocumentTextElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800cd7a0  ?Draw@DocumentTextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800cd360  ?Update@DocumentTextElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::EditDescription  vtable=0x180156880  slots=10  sizeof=0x88  parent=UI::ElementDescription

  [ 0] +0x00 0x18003fa20  (unexported) UI_EditDescription_VectorDeletingDtor

  [ 1] +0x08 0x18003f6d0  ?TypeName@EditDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003f6b0  ?Type@EditDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d11e0  ?GetFileVersion@EditDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d1110  ?Serialize@EditDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18003f850  ?Reconstruct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18003f880  ?Destruct@EditDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d0f90  ?CreateInstance@EditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18003f870  ?Construct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::EditElement  vtable=0x180156810  slots=13  sizeof=0xf8  parent=UI::Element

  [ 0] +0x00 0x18003fed0  (unexported) UI_EditElement_VectorDeletingDtor

  [ 1] +0x08 0x18003fae0  ?TypeName@EditElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003fac0  ?Type@EditElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d0c10  ?ComputeSize@EditElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d0ab0  ?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d0820  ?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800d0220  ?Update@EditElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element

  [12] +0x60 0x1800d0da0  ?FinalizeText@EditElement@UI@@UEAAXXZ



### UI::Element  vtable=0x180156160  slots=12  sizeof=0xc0  parent=Object::Internal

  [ 0] +0x00 0x180035c10  (unexported) FUN_180035c10

  [ 1] +0x08 0x180035440  ?TypeName@Element@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180035420  ?Type@Element@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z

  [ 4] +0x20 0x18007ec28  _purecall

  [ 5] +0x28 0x1801168d0  ?ComputeLayout@Element@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1801168f0  ?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ



### UI::ElementDescription  vtable=0x1801561c0  slots=9  sizeof=0x68  parent=Object::External

  [ 0] +0x00 0x180035350  (unexported) FUN_180035350

  [ 1] +0x08 0x1800350f0  ?TypeName@ElementDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800350d0  ?Type@ElementDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x180116af0  ?GetFileVersion@ElementDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x180116b00  ?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External

  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External

  [ 8] +0x40 0x18007ec28  _purecall



### UI::Font  vtable=0x180155b50  slots=9  sizeof=0x90  parent=Object::External

  [ 0] +0x00 0x180032130  (unexported) FUN_180032130

  [ 1] +0x08 0x1800319c0  ?TypeName@Font@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800319a0  ?Type@Font@UI@@UEBAIXZ

  [ 3] +0x18 0x1801176b0  ?GetFileVersion@Font@UI@@UEBAHXZ

  [ 4] +0x20 0x1801176c0  ?Compile@Font@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z

  [ 5] +0x28 0x1801176d0  ?Serialize@Font@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180031c40  ?Reconstruct@Font@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180031d80  ?Destruct@Font@UI@@UEAAXXZ

  [ 8] +0x40 0x180031ce0  ?Construct@Font@UI@@UEAAPEAVExternal@Object@@XZ



### UI::GraphDescription  vtable=0x180156a30  slots=10  sizeof=0x88  parent=UI::ElementDescription

  [ 0] +0x00 0x1800403a0  (unexported) UI_GraphDescription_VectorDeletingDtor

  [ 1] +0x08 0x180040090  ?TypeName@GraphDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180040070  ?Type@GraphDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d1b20  ?GetFileVersion@GraphDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d1a30  ?Serialize@GraphDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180040210  ?Reconstruct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180040240  ?Destruct@GraphDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d1920  ?CreateInstance@GraphDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180040230  ?Construct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::GraphElement  vtable=0x1801569d0  slots=12  sizeof=0x100  parent=UI::Element

  [ 0] +0x00 0x1800408b0  (unexported) UI_GraphElement_VectorDeletingDtor

  [ 1] +0x08 0x180040460  ?TypeName@GraphElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180040440  ?Type@GraphElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d1810  ?ComputeSize@GraphElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d16b0  ?ComputeLayout@GraphElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d1360  ?Draw@GraphElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::GroupDescription  vtable=0x180157190  slots=10  sizeof=0x78  parent=UI::ElementDescription

  [ 0] +0x00 0x180046af0  (unexported) FUN_180046af0

  [ 1] +0x08 0x180046660  ?TypeName@GroupDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180046640  ?Type@GroupDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x180117cf0  ?GetFileVersion@GroupDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x180117d00  ?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800468e0  ?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180046a20  ?Destruct@GroupDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180117dc0  ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180046980  ?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::GroupElement  vtable=0x180157130  slots=12  sizeof=0xc0  parent=UI::Element

  [ 0] +0x00 0x180046e10  (unexported) UI_GroupElement_VectorDeletingDtor

  [ 1] +0x08 0x180046bb0  ?TypeName@GroupElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180046b90  ?Type@GroupElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x180117f00  ?ComputeSize@GroupElement@UI@@UEAAXXZ

  [ 5] +0x28 0x180118050  ?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1801181c0  ?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ImageDescription  vtable=0x180156110  slots=10  sizeof=0x80  parent=UI::ElementDescription

  [ 0] +0x00 0x1800361f0  (unexported) FUN_1800361f0

  [ 1] +0x08 0x180035cd0  ?TypeName@ImageDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180035cb0  ?Type@ImageDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1801184a0  ?GetFileVersion@ImageDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1801184b0  ?Serialize@ImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180035f60  ?Reconstruct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x1800360c0  ?Destruct@ImageDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180118910  ?CreateInstance@ImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180036010  ?Construct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ImageElement  vtable=0x1801560b0  slots=12  sizeof=0xe0  parent=UI::Element

  [ 0] +0x00 0x180036600  (unexported) UI_ImageElement_VectorDeletingDtor

  [ 1] +0x08 0x1800362b0  ?TypeName@ImageElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180036290  ?Type@ImageElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x180118650  ?ComputeSize@ImageElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1801186f0  ?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x180118850  ?Draw@ImageElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::LabelDescription  vtable=0x180156b30  slots=10  sizeof=0xb8  parent=UI::TextDescription

  [ 0] +0x00 0x180042730  (unexported) UI_LabelDescription_VectorDeletingDtor

  [ 1] +0x08 0x1800423b0  ?TypeName@LabelDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180042390  ?Type@LabelDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d2950  ?GetFileVersion@LabelDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::TextDescription

  [ 5] +0x28 0x1800d2820  ?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180042530  ?Reconstruct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180042560  ?Destruct@LabelDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d26b0  ?CreateInstance@LabelDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180042550  ?Construct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::LabelElement  vtable=0x180156ad0  slots=12  sizeof=0x130  parent=UI::TextElement

  [ 0] +0x00 0x180042bb0  (unexported) UI_LabelElement_VectorDeletingDtor

  [ 1] +0x08 0x1800427f0  ?TypeName@LabelElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800427d0  ?Type@LabelElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::TextElement

  [ 4] +0x20 0x1800d2490  ?ComputeSize@LabelElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d2330  ?ComputeLayout@LabelElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d1c80  ?Draw@LabelElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::TextElement

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::TextElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::TextElement

  [10] +0x50 0x180042970  ?SetColor@LabelElement@UI@@UEAAXI@Z

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::TextElement



### UI::LayoutDescription  vtable=0x180156cc0  slots=10  sizeof=0x78  parent=UI::ElementDescription

  [ 0] +0x00 0x1800431d0  (unexported) FUN_1800431d0

  [ 1] +0x08 0x180042d40  ?TypeName@LayoutDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180042d20  ?Type@LayoutDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d3a00  ?GetFileVersion@LayoutDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d3940  ?Serialize@LayoutDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180042fc0  ?Reconstruct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180043100  ?Destruct@LayoutDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d33a0  ?CreateInstance@LayoutDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180043060  ?Construct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::LayoutElement  vtable=0x180156c60  slots=12  sizeof=0xd0  parent=UI::Element

  [ 0] +0x00 0x180043570  (unexported) UI_LayoutElement_VectorDeletingDtor

  [ 1] +0x08 0x180043290  ?TypeName@LayoutElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180043270  ?Type@LayoutElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d3100  ?ComputeSize@LayoutElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d2af0  ?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d2a40  ?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ListDescription  vtable=0x180156e20  slots=10  sizeof=0x90  parent=UI::ElementDescription

  [ 0] +0x00 0x180044ed0  (unexported) FUN_180044ed0

  [ 1] +0x08 0x180044ba0  ?TypeName@ListDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180044b80  ?Type@ListDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d4900  ?GetFileVersion@ListDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d4810  ?Serialize@ListDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180044d20  ?Reconstruct@ListDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180044d50  ?Destruct@ListDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d45f0  ?CreateInstance@ListDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180044d40  ?Construct@ListDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ListElement  vtable=0x180156dc0  slots=12  sizeof=0x100  parent=UI::Element

  [ 0] +0x00 0x180045320  (unexported) UI_ListElement_VectorDeletingDtor

  [ 1] +0x08 0x180044f90  ?TypeName@ListElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180044f70  ?Type@ListElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d4380  ?ComputeSize@ListElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d4210  ?ComputeLayout@ListElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d3eb0  ?Draw@ListElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800d3c20  ?Update@ListElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::NumberDescription  vtable=0x180157530  slots=10  sizeof=0x88  parent=UI::ElementDescription

  [ 0] +0x00 0x18004a760  (unexported) FUN_18004a760

  [ 1] +0x08 0x18004a240  ?TypeName@NumberDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004a220  ?Type@NumberDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x180118e20  ?GetFileVersion@NumberDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x180118e30  ?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18004a4c0  ?Reconstruct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18004a610  ?Destruct@NumberDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180118f10  ?CreateInstance@NumberDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18004a570  ?Construct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::NumberEditDescription  vtable=0x180156fe0  slots=10  sizeof=0x88  parent=UI::ElementDescription

  [ 0] +0x00 0x180046190  (unexported) FUN_180046190

  [ 1] +0x08 0x180045e90  ?TypeName@NumberEditDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180045e70  ?Type@NumberEditDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d4fd0  ?GetFileVersion@NumberEditDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d4ee0  ?Serialize@NumberEditDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180046010  ?Reconstruct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180046040  ?Destruct@NumberEditDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d4cc0  ?CreateInstance@NumberEditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180046030  ?Construct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::NumberEditElement  vtable=0x180156f70  slots=13  sizeof=0x100  parent=UI::EditElement

  [ 0] +0x00 0x180046540  (unexported) UI_NumberEditElement_VectorDeletingDtor

  [ 1] +0x08 0x180046250  ?TypeName@NumberEditElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180046230  ?Type@NumberEditElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::EditElement

  [ 4] +0x20 0x1800d0c10  ?ComputeSize@EditElement@UI@@UEAAXXZ   inherited-from UI::EditElement

  [ 5] +0x28 0x1800d0ab0  ?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z   inherited-from UI::EditElement

  [ 6] +0x30 0x1800d0820  ?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   inherited-from UI::EditElement

  [ 7] +0x38 0x1800d0220  ?Update@EditElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z   inherited-from UI::EditElement

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::EditElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::EditElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::EditElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::EditElement

  [12] +0x60 0x1800d4a60  ?FinalizeText@NumberEditElement@UI@@UEAAXXZ



### UI::NumberElement  vtable=0x1801574d0  slots=12  sizeof=0xe8  parent=UI::Element

  [ 0] +0x00 0x18004ab80  (unexported) UI_NumberElement_VectorDeletingDtor

  [ 1] +0x08 0x18004a820  ?TypeName@NumberElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004a800  ?Type@NumberElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x180119000  ?ComputeSize@NumberElement@UI@@UEAAXXZ

  [ 5] +0x28 0x180119080  ?ComputeLayout@NumberElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1801191e0  ?Draw@NumberElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::PageDescription  vtable=0x1801570e0  slots=10  sizeof=0x80  parent=UI::GroupDescription

  [ 0] +0x00 0x1800471a0  (unexported) FUN_1800471a0

  [ 1] +0x08 0x180046ee0  ?TypeName@PageDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180046ec0  ?Type@PageDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d54a0  ?GetFileVersion@PageDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::GroupDescription

  [ 5] +0x28 0x1800d5410  ?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180047060  ?Reconstruct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180047090  ?Destruct@PageDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d52b0  ?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180047080  ?Construct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::PageElement  vtable=0x180157080  slots=12  sizeof=0xc8  parent=UI::GroupElement

  [ 0] +0x00 0x180047540  (unexported) UI_PageElement_VectorDeletingDtor

  [ 1] +0x08 0x180047270  ?TypeName@PageElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180047250  ?Type@PageElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::GroupElement

  [ 4] +0x20 0x180117f00  ?ComputeSize@GroupElement@UI@@UEAAXXZ   inherited-from UI::GroupElement

  [ 5] +0x28 0x180118050  ?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z   inherited-from UI::GroupElement

  [ 6] +0x30 0x1801181c0  ?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   inherited-from UI::GroupElement

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::GroupElement

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::GroupElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::GroupElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::GroupElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::GroupElement



### UI::ProgressDescription  vtable=0x1801591e0  slots=10  sizeof=0xb0  parent=UI::ElementDescription

  [ 0] +0x00 0x180077c80  (unexported) FUN_180077c80

  [ 1] +0x08 0x180077770  ?TypeName@ProgressDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180077750  ?Type@ProgressDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x18011a070  ?GetFileVersion@ProgressDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x180119f60  ?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800778f0  ?Reconstruct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180077920  ?Destruct@ProgressDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x180119d00  ?CreateInstance@ProgressDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180077910  ?Construct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ProgressElement  vtable=0x180159180  slots=12  sizeof=0x130  parent=UI::Element

  [ 0] +0x00 0x180078150  (unexported) UI_ProgressElement_VectorDeletingDtor

  [ 1] +0x08 0x180077d40  ?TypeName@ProgressElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180077d20  ?Type@ProgressElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1801198f0  ?ComputeSize@ProgressElement@UI@@UEAAXXZ

  [ 5] +0x28 0x180119610  ?ComputeLayout@ProgressElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1801192c0  ?Draw@ProgressElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::RadioDescription  vtable=0x180157420  slots=10  sizeof=0x80  parent=UI::GroupDescription

  [ 0] +0x00 0x180047930  (unexported) FUN_180047930

  [ 1] +0x08 0x180047670  ?TypeName@RadioDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180047650  ?Type@RadioDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d5b10  ?GetFileVersion@RadioDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::GroupDescription

  [ 5] +0x28 0x1800d5a80  ?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800477f0  ?Reconstruct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180047820  ?Destruct@RadioDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d58d0  ?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180047810  ?Construct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::RadioElement  vtable=0x1801573c0  slots=12  sizeof=0xd8  parent=UI::GroupElement

  [ 0] +0x00 0x180047d50  (unexported) UI_RadioElement_VectorDeletingDtor

  [ 1] +0x08 0x180047a00  ?TypeName@RadioElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800479e0  ?Type@RadioElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::GroupElement

  [ 4] +0x20 0x180117f00  ?ComputeSize@GroupElement@UI@@UEAAXXZ   inherited-from UI::GroupElement

  [ 5] +0x28 0x180118050  ?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z   inherited-from UI::GroupElement

  [ 6] +0x30 0x1801181c0  ?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   inherited-from UI::GroupElement

  [ 7] +0x38 0x1800d5580  ?Update@RadioElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::GroupElement

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::GroupElement

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::GroupElement

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::GroupElement



### UI::RibbonDescription  vtable=0x180156ed0  slots=10  sizeof=0x80  parent=UI::ElementDescription

  [ 0] +0x00 0x180044620  (unexported) FUN_180044620

  [ 1] +0x08 0x180044130  ?TypeName@RibbonDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180044110  ?Type@RibbonDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d5ef0  ?GetFileVersion@RibbonDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d5f00  ?Serialize@RibbonDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800443c0  ?Reconstruct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180044520  ?Destruct@RibbonDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d6000  ?CreateInstance@RibbonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180044470  ?Construct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::RibbonElement  vtable=0x180156e70  slots=12  sizeof=0xd8  parent=UI::Element

  [ 0] +0x00 0x180044ac0  (unexported) UI_RibbonElement_VectorDeletingDtor

  [ 1] +0x08 0x1800446e0  ?TypeName@RibbonElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800446c0  ?Type@RibbonElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d6190  ?ComputeSize@RibbonElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d6410  ?ComputeLayout@RibbonElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d6740  ?Draw@RibbonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ScrollDescription  vtable=0x180156460  slots=10  sizeof=0xc8  parent=UI::ElementDescription

  [ 0] +0x00 0x18003a790  (unexported) FUN_18003a790

  [ 1] +0x08 0x18003a120  ?TypeName@ScrollDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003a100  ?Type@ScrollDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x18011a930  ?GetFileVersion@ScrollDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x18011a940  ?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18003a2a0  ?Reconstruct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18003a2d0  ?Destruct@ScrollDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x18011aac0  ?CreateInstance@ScrollDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18003a2c0  ?Construct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::ScrollElement  vtable=0x180156400  slots=12  sizeof=0x178  parent=UI::Element

  [ 0] +0x00 0x18003adc0  (unexported) UI_ScrollElement_VectorDeletingDtor

  [ 1] +0x08 0x18003a850  ?TypeName@ScrollElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003a830  ?Type@ScrollElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x18011ad80  ?ComputeSize@ScrollElement@UI@@UEAAXXZ

  [ 5] +0x28 0x18011af20  ?ComputeLayout@ScrollElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x18011b0b0  ?Draw@ScrollElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x18011b620  ?Update@ScrollElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::SliderDescription  vtable=0x1801575e0  slots=10  sizeof=0xb8  parent=UI::ElementDescription

  [ 0] +0x00 0x180049c60  (unexported) FUN_180049c60

  [ 1] +0x08 0x180049750  ?TypeName@SliderDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180049730  ?Type@SliderDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d79f0  ?GetFileVersion@SliderDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d7890  ?Serialize@SliderDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x1800498d0  ?Reconstruct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180049900  ?Destruct@SliderDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d7630  ?CreateInstance@SliderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x1800498f0  ?Construct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::SliderElement  vtable=0x180157580  slots=12  sizeof=0x130  parent=UI::Element

  [ 0] +0x00 0x18004a180  (unexported) UI_SliderElement_VectorDeletingDtor

  [ 1] +0x08 0x180049d20  ?TypeName@SliderElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180049d00  ?Type@SliderElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d7380  ?ComputeSize@SliderElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d6fc0  ?ComputeLayout@SliderElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d6d40  ?Draw@SliderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800d67e0  ?Update@SliderElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::SpinnerDescription  vtable=0x1801576c0  slots=10  sizeof=0x88  parent=UI::ElementDescription

  [ 0] +0x00 0x18004b050  (unexported) FUN_18004b050

  [ 1] +0x08 0x18004ad60  ?TypeName@SpinnerDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004ad40  ?Type@SpinnerDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d8940  ?GetFileVersion@SpinnerDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800d8880  ?Serialize@SpinnerDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18004aee0  ?Reconstruct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18004af10  ?Destruct@SpinnerDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800d86b0  ?CreateInstance@SpinnerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18004af00  ?Construct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::SpinnerElement  vtable=0x180157660  slots=12  sizeof=0xc8  parent=UI::Element

  [ 0] +0x00 0x18004b3b0  (unexported) UI_SpinnerElement_VectorDeletingDtor

  [ 1] +0x08 0x18004b110  ?TypeName@SpinnerElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004b0f0  ?Type@SpinnerElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d83b0  ?ComputeSize@SpinnerElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d80c0  ?ComputeLayout@SpinnerElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d8030  ?Draw@SpinnerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800d7e70  ?Update@SpinnerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::SpriteCapture  vtable=0x1801566a0  slots=9  sizeof=0x58  parent=Object::External

  [ 0] +0x00 0x18003ccf0  (unexported) FUN_18003ccf0

  [ 1] +0x08 0x18003c810  ?TypeName@SpriteCapture@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18003c7f0  ?Type@SpriteCapture@UI@@UEBAIXZ

  [ 3] +0x18 0x1800d9520  ?GetFileVersion@SpriteCapture@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x1800d9530  ?Serialize@SpriteCapture@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18003ca40  ?Reconstruct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18003cb40  ?Destruct@SpriteCapture@UI@@UEAAXXZ

  [ 8] +0x40 0x18003cac0  ?Construct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ



### UI::SpriteSheet  vtable=0x180154590  slots=9  sizeof=0x58  parent=Object::External

  [ 0] +0x00 0x1800037b0  (unexported) FUN_1800037b0

  [ 1] +0x08 0x180002e50  ?TypeName@SpriteSheet@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180002e30  ?Type@SpriteSheet@UI@@UEBAIXZ

  [ 3] +0x18 0x18011d710  ?GetFileVersion@SpriteSheet@UI@@UEBAHXZ

  [ 4] +0x20 0x18011d720  ?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z

  [ 5] +0x28 0x18011d730  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180003080  ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180003180  ?Destruct@SpriteSheet@UI@@UEAAXXZ

  [ 8] +0x40 0x180003100  ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ



### UI::StringTable  vtable=0x180154630  slots=9  sizeof=0x30  parent=Object::External

  [ 0] +0x00 0x180002740  (unexported) FUN_180002740

  [ 1] +0x08 0x180002190  ?TypeName@StringTable@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180002170  ?Type@StringTable@UI@@UEBAIXZ

  [ 3] +0x18 0x18011d930  ?GetFileVersion@StringTable@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External

  [ 5] +0x28 0x18011d940  ?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180002390  ?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180002460  ?Destruct@StringTable@UI@@UEAAXXZ

  [ 8] +0x40 0x180002400  ?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ



### UI::TabDescription  vtable=0x1801577f0  slots=10  sizeof=0xb8  parent=UI::ElementDescription

  [ 0] +0x00 0x18004c640  (unexported) FUN_18004c640

  [ 1] +0x08 0x18004c1f0  ?TypeName@TabDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004c1d0  ?Type@TabDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x1800da8d0  ?GetFileVersion@TabDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x1800da760  ?Serialize@TabDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x18004c370  ?Reconstruct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x18004c3a0  ?Destruct@TabDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x1800da410  ?CreateInstance@TabDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x18004c390  ?Construct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::TabElement  vtable=0x180157790  slots=12  sizeof=0x108  parent=UI::Element

  [ 0] +0x00 0x18004ca90  (unexported) UI_TabElement_VectorDeletingDtor

  [ 1] +0x08 0x18004c700  ?TypeName@TabElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004c6e0  ?Type@TabElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x1800d9ea0  ?ComputeSize@TabElement@UI@@UEAAXXZ

  [ 5] +0x28 0x1800d9b20  ?ComputeLayout@TabElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x1800d98b0  ?Draw@TabElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x1800d9810  ?Update@TabElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::TableDescription  vtable=0x1801592c0  slots=10  sizeof=0x98  parent=UI::ElementDescription

  [ 0] +0x00 0x180079210  (unexported) FUN_180079210

  [ 1] +0x08 0x180078ed0  ?TypeName@TableDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180078eb0  ?Type@TableDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x18011e6e0  ?GetFileVersion@TableDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x18011e590  ?Serialize@TableDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180079050  ?Reconstruct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180079080  ?Destruct@TableDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x18011e350  ?CreateInstance@TableDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180079070  ?Construct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::TableElement  vtable=0x180159260  slots=12  sizeof=0xf0  parent=UI::Element

  [ 0] +0x00 0x180079600  (unexported) UI_TableElement_VectorDeletingDtor

  [ 1] +0x08 0x1800792d0  ?TypeName@TableElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x1800792b0  ?Type@TableElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x18011df60  ?ComputeSize@TableElement@UI@@UEAAXXZ

  [ 5] +0x28 0x18011dd10  ?ComputeLayout@TableElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x18011dc20  ?Draw@TableElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::TextDescription  vtable=0x180156be0  slots=10  sizeof=0x90  parent=UI::ElementDescription

  [ 0] +0x00 0x180041e30  (unexported) FUN_180041e30

  [ 1] +0x08 0x180041ad0  ?TypeName@TextDescription@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180041ab0  ?Type@TextDescription@UI@@UEBAIXZ

  [ 3] +0x18 0x18011ee20  ?GetFileVersion@TextDescription@UI@@UEBAHXZ

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from UI::ElementDescription

  [ 5] +0x28 0x18011ee30  ?Serialize@TextDescription@UI@@UEAA_NAEAVStream@IO@@@Z

  [ 6] +0x30 0x180041c50  ?Reconstruct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ

  [ 7] +0x38 0x180041c80  ?Destruct@TextDescription@UI@@UEAAXXZ

  [ 8] +0x40 0x18011f3d0  ?CreateInstance@TextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

  [ 9] +0x48 0x180041c70  ?Construct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ



### UI::TextElement  vtable=0x180156b80  slots=12  sizeof=0x100  parent=UI::Element

  [ 0] +0x00 0x1800422f0  (unexported) UI_TextElement_VectorDeletingDtor

  [ 1] +0x08 0x180041ef0  ?TypeName@TextElement@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x180041ed0  ?Type@TextElement@UI@@UEBAIXZ

  [ 3] +0x18 0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::Element

  [ 4] +0x20 0x18011f060  ?ComputeSize@TextElement@UI@@UEAAXXZ

  [ 5] +0x28 0x18011f120  ?ComputeLayout@TextElement@UI@@UEAAXHHHH@Z

  [ 6] +0x30 0x18011f280  ?Draw@TextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

  [ 7] +0x38 0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z   inherited-from UI::Element

  [ 8] +0x40 0x180116910  ?Enable@Element@UI@@UEAAX_N@Z   inherited-from UI::Element

  [ 9] +0x48 0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z   inherited-from UI::Element

  [10] +0x50 0x180035620  ?SetColor@Element@UI@@UEAAXI@Z   inherited-from UI::Element

  [11] +0x58 0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ   inherited-from UI::Element



### UI::ToolTipDialog  vtable=0x180157880  slots=10  sizeof=0xc0  parent=UI::DialogInstance

  [ 0] +0x00 0x18004d270  (unexported) FUN_18004d270

  [ 1] +0x08 0x18004cfc0  ?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ

  [ 2] +0x10 0x18004cfa0  ?Type@ToolTipDialog@UI@@UEBAIXZ

  [ 3] +0x18 0x180114fa0  ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::DialogInstance

  [ 4] +0x20 0x180114fb0  ?OnOpened@DialogInstance@UI@@UEAAXXZ   inherited-from UI::DialogInstance

  [ 5] +0x28 0x180115330  ?OnClosed@DialogInstance@UI@@UEAAXXZ   inherited-from UI::DialogInstance

  [ 6] +0x30 0x1801153a0  ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ   inherited-from UI::DialogInstance

  [ 7] +0x38 0x180115400  ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z   inherited-from UI::DialogInstance

  [ 8] +0x40 0x180114ed0  ?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z   inherited-from UI::DialogInstance

  [ 9] +0x48 0x180115470  ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z   inherited-from UI::DialogInstance



@@@@@@ SECTION 2 -- the five Object parent vtables



### Object::Base  vtable=0x180154788  slots=3  sizeof=0x8  parent=ROOT/UNKNOWN (no _ParentTypeName)

  [ 0] +0x00 0x1800065b0  (unexported) FUN_1800065b0

  [ 1] +0x08 0x18007ec28  _purecall

  [ 2] +0x10 0x1800c0380  ?Type@Base@Object@@UEBAIXZ



### Object::External  vtable=0x1801547a0  slots=8  sizeof=0x20  parent=Object::Base

  [ 0] +0x00 0x180006640  (unexported) FUN_180006640

  [ 1] +0x08 0x1800066c0  ?TypeName@External@Object@@UEBAPEBDXZ

  [ 2] +0x10 0x1800066d0  ?Type@External@Object@@UEBAIXZ

  [ 3] +0x18 0x18007ec28  _purecall

  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z

  [ 5] +0x28 0x18007ec28  _purecall

  [ 6] +0x30 0x18007ec28  _purecall

  [ 7] +0x38 0x18007ec28  _purecall



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



@@@@@@ SECTION 3 -- _TypeName / _ParentTypeName literals

count=136

0x1800366a0  ?_ParentTypeName@AnimatedImageDescription@UI@@CAPEBDXZ     -> "ImageDescription"

0x180036a80  ?_ParentTypeName@AnimatedImageElement@UI@@CAPEBDXZ         -> "ImageElement"

0x1800409b0  ?_ParentTypeName@BorderDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x180041080  ?_ParentTypeName@BorderElement@UI@@CAPEBDXZ                -> "Element"

0x180047e00  ?_ParentTypeName@ButtonDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x180048230  ?_ParentTypeName@ButtonElement@UI@@CAPEBDXZ                -> "Element"

0x180037420  ?_ParentTypeName@CharacterSet@UI@@CAPEBDXZ                 -> "Object::External"

0x180048650  ?_ParentTypeName@CheckDescription@UI@@CAPEBDXZ             -> "ButtonDescription"

0x180048a50  ?_ParentTypeName@CheckElement@UI@@CAPEBDXZ                 -> "ButtonElement"

0x180037960  ?_ParentTypeName@ClipRegionDescription@UI@@CAPEBDXZ        -> "ElementDescription"

0x180037f40  ?_ParentTypeName@ClipRegionElement@UI@@CAPEBDXZ            -> "Element"

0x180074ab0  ?_ParentTypeName@ComboDescription@UI@@CAPEBDXZ             -> "ButtonDescription"

0x180074730  ?_ParentTypeName@ComboDialog@UI@@CAPEBDXZ                  -> "DialogInstance"

0x180074f00  ?_ParentTypeName@ComboElement@UI@@CAPEBDXZ                 -> "ButtonElement"

0x1800398a0  ?_ParentTypeName@ContainerDescription@UI@@CAPEBDXZ         -> "ElementDescription"

0x180039c90  ?_ParentTypeName@ContainerElement@UI@@CAPEBDXZ             -> "Element"

0x18003af50  ?_ParentTypeName@Cursor@UI@@CAPEBDXZ                       -> "Object::External"

0x180038410  ?_ParentTypeName@Dialog@UI@@CAPEBDXZ                       -> "Object::External"

0x18003b6a0  ?_ParentTypeName@DialogController@UI@@CAPEBDXZ             -> "Object::Internal"

0x18003b440  ?_ParentTypeName@DialogControllerConfig@UI@@CAPEBDXZ       -> "Object::External"

0x1800453d0  ?_ParentTypeName@DialogInstance@UI@@CAPEBDXZ               -> "Object::Internal"

0x18003b8d0  ?_ParentTypeName@DocumentTextDescription@UI@@CAPEBDXZ      -> "ElementDescription"

0x18003be20  ?_ParentTypeName@DocumentTextElement@UI@@CAPEBDXZ          -> "Element"

0x18003f6a0  ?_ParentTypeName@EditDescription@UI@@CAPEBDXZ              -> "ElementDescription"

0x18003fab0  ?_ParentTypeName@EditElement@UI@@CAPEBDXZ                  -> "Element"

0x180035410  ?_ParentTypeName@Element@UI@@CAPEBDXZ                      -> "Object::Internal"

0x1800350c0  ?_ParentTypeName@ElementDescription@UI@@CAPEBDXZ           -> "Object::External"

0x180031990  ?_ParentTypeName@Font@UI@@CAPEBDXZ                         -> "Object::External"

0x180040060  ?_ParentTypeName@GraphDescription@UI@@CAPEBDXZ             -> "ElementDescription"

0x180040430  ?_ParentTypeName@GraphElement@UI@@CAPEBDXZ                 -> "Element"

0x180046630  ?_ParentTypeName@GroupDescription@UI@@CAPEBDXZ             -> "ElementDescription"

0x180046b80  ?_ParentTypeName@GroupElement@UI@@CAPEBDXZ                 -> "Element"

0x180035ca0  ?_ParentTypeName@ImageDescription@UI@@CAPEBDXZ             -> "ElementDescription"

0x180036280  ?_ParentTypeName@ImageElement@UI@@CAPEBDXZ                 -> "Element"

0x180042380  ?_ParentTypeName@LabelDescription@UI@@CAPEBDXZ             -> "TextDescription"

0x1800427c0  ?_ParentTypeName@LabelElement@UI@@CAPEBDXZ                 -> "TextElement"

0x180042d10  ?_ParentTypeName@LayoutDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x180043260  ?_ParentTypeName@LayoutElement@UI@@CAPEBDXZ                -> "Element"

0x180044b70  ?_ParentTypeName@ListDescription@UI@@CAPEBDXZ              -> "ElementDescription"

0x180044f60  ?_ParentTypeName@ListElement@UI@@CAPEBDXZ                  -> "Element"

0x18004a210  ?_ParentTypeName@NumberDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x180045e60  ?_ParentTypeName@NumberEditDescription@UI@@CAPEBDXZ        -> "ElementDescription"

0x180046220  ?_ParentTypeName@NumberEditElement@UI@@CAPEBDXZ            -> "EditElement"

0x18004a7f0  ?_ParentTypeName@NumberElement@UI@@CAPEBDXZ                -> "Element"

0x180046eb0  ?_ParentTypeName@PageDescription@UI@@CAPEBDXZ              -> "GroupDescription"

0x180047240  ?_ParentTypeName@PageElement@UI@@CAPEBDXZ                  -> "GroupElement"

0x180077740  ?_ParentTypeName@ProgressDescription@UI@@CAPEBDXZ          -> "ElementDescription"

0x180077d10  ?_ParentTypeName@ProgressElement@UI@@CAPEBDXZ              -> "Element"

0x180047640  ?_ParentTypeName@RadioDescription@UI@@CAPEBDXZ             -> "GroupDescription"

0x1800479d0  ?_ParentTypeName@RadioElement@UI@@CAPEBDXZ                 -> "GroupElement"

0x180044100  ?_ParentTypeName@RibbonDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x1800446b0  ?_ParentTypeName@RibbonElement@UI@@CAPEBDXZ                -> "Element"

0x18003a0f0  ?_ParentTypeName@ScrollDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x18003a820  ?_ParentTypeName@ScrollElement@UI@@CAPEBDXZ                -> "Element"

0x180049720  ?_ParentTypeName@SliderDescription@UI@@CAPEBDXZ            -> "ElementDescription"

0x180049cf0  ?_ParentTypeName@SliderElement@UI@@CAPEBDXZ                -> "Element"

0x18004ad30  ?_ParentTypeName@SpinnerDescription@UI@@CAPEBDXZ           -> "ElementDescription"

0x18004b0e0  ?_ParentTypeName@SpinnerElement@UI@@CAPEBDXZ               -> "Element"

0x18003c7e0  ?_ParentTypeName@SpriteCapture@UI@@CAPEBDXZ                -> "Object::External"

0x180002e20  ?_ParentTypeName@SpriteSheet@UI@@CAPEBDXZ                  -> "Object::External"

0x180002160  ?_ParentTypeName@StringTable@UI@@CAPEBDXZ                  -> "Object::External"

0x18004c1c0  ?_ParentTypeName@TabDescription@UI@@CAPEBDXZ               -> "ElementDescription"

0x18004c6d0  ?_ParentTypeName@TabElement@UI@@CAPEBDXZ                   -> "Element"

0x180078ea0  ?_ParentTypeName@TableDescription@UI@@CAPEBDXZ             -> "ElementDescription"

0x1800792a0  ?_ParentTypeName@TableElement@UI@@CAPEBDXZ                 -> "Element"

0x180041aa0  ?_ParentTypeName@TextDescription@UI@@CAPEBDXZ              -> "ElementDescription"

0x180041ec0  ?_ParentTypeName@TextElement@UI@@CAPEBDXZ                  -> "Element"

0x18004cf90  ?_ParentTypeName@ToolTipDialog@UI@@CAPEBDXZ                -> "UI::DialogInstance"

0x180036690  ?_TypeName@AnimatedImageDescription@UI@@CAPEBDXZ           -> "AnimatedImageDescription"

0x180036a70  ?_TypeName@AnimatedImageElement@UI@@CAPEBDXZ               -> "AnimatedImageElement"

0x1800409a0  ?_TypeName@BorderDescription@UI@@CAPEBDXZ                  -> "BorderDescription"

0x180041070  ?_TypeName@BorderElement@UI@@CAPEBDXZ                      -> "BorderElement"

0x180047df0  ?_TypeName@ButtonDescription@UI@@CAPEBDXZ                  -> "ButtonDescription"

0x180048220  ?_TypeName@ButtonElement@UI@@CAPEBDXZ                      -> "ButtonElement"

0x180037410  ?_TypeName@CharacterSet@UI@@CAPEBDXZ                       -> "CharacterSet"

0x180048640  ?_TypeName@CheckDescription@UI@@CAPEBDXZ                   -> "CheckDescription"

0x180048a40  ?_TypeName@CheckElement@UI@@CAPEBDXZ                       -> "CheckElement"

0x180037950  ?_TypeName@ClipRegionDescription@UI@@CAPEBDXZ              -> "ClipRegionDescription"

0x180037f30  ?_TypeName@ClipRegionElement@UI@@CAPEBDXZ                  -> "ClipRegionElement"

0x180074aa0  ?_TypeName@ComboDescription@UI@@CAPEBDXZ                   -> "ComboDescription"

0x180074720  ?_TypeName@ComboDialog@UI@@CAPEBDXZ                        -> "ComboDialog"

0x180074ef0  ?_TypeName@ComboElement@UI@@CAPEBDXZ                       -> "ComboElement"

0x180039890  ?_TypeName@ContainerDescription@UI@@CAPEBDXZ               -> "ContainerDescription"

0x180039c80  ?_TypeName@ContainerElement@UI@@CAPEBDXZ                   -> "ContainerElement"

0x18003af40  ?_TypeName@Cursor@UI@@CAPEBDXZ                             -> "Cursor"

0x180038400  ?_TypeName@Dialog@UI@@CAPEBDXZ                             -> "Dialog"

0x18003b690  ?_TypeName@DialogController@UI@@CAPEBDXZ                   -> "DialogController"

0x18003b430  ?_TypeName@DialogControllerConfig@UI@@CAPEBDXZ             -> "DialogControllerConfig"

0x1800453c0  ?_TypeName@DialogInstance@UI@@CAPEBDXZ                     -> "DialogInstance"

0x18003b8c0  ?_TypeName@DocumentTextDescription@UI@@CAPEBDXZ            -> "DocumentTextDescription"

0x18003be10  ?_TypeName@DocumentTextElement@UI@@CAPEBDXZ                -> "DocumentTextElement"

0x18003f690  ?_TypeName@EditDescription@UI@@CAPEBDXZ                    -> "EditDescription"

0x18003faa0  ?_TypeName@EditElement@UI@@CAPEBDXZ                        -> "EditElement"

0x180035400  ?_TypeName@Element@UI@@CAPEBDXZ                            -> "Element"

0x1800350b0  ?_TypeName@ElementDescription@UI@@CAPEBDXZ                 -> "ElementDescription"

0x180031980  ?_TypeName@Font@UI@@CAPEBDXZ                               -> "Font"

0x180040050  ?_TypeName@GraphDescription@UI@@CAPEBDXZ                   -> "GraphDescription"

0x180040420  ?_TypeName@GraphElement@UI@@CAPEBDXZ                       -> "GraphElement"

0x180046620  ?_TypeName@GroupDescription@UI@@CAPEBDXZ                   -> "GroupDescription"

0x180046b70  ?_TypeName@GroupElement@UI@@CAPEBDXZ                       -> "GroupElement"

0x180035c90  ?_TypeName@ImageDescription@UI@@CAPEBDXZ                   -> "ImageDescription"

0x180036270  ?_TypeName@ImageElement@UI@@CAPEBDXZ                       -> "ImageElement"

0x180042370  ?_TypeName@LabelDescription@UI@@CAPEBDXZ                   -> "LabelDescription"

0x1800427b0  ?_TypeName@LabelElement@UI@@CAPEBDXZ                       -> "LabelElement"

0x180042d00  ?_TypeName@LayoutDescription@UI@@CAPEBDXZ                  -> "LayoutDescription"

0x180043250  ?_TypeName@LayoutElement@UI@@CAPEBDXZ                      -> "LayoutElement"

0x180044b60  ?_TypeName@ListDescription@UI@@CAPEBDXZ                    -> "ListDescription"

0x180044f50  ?_TypeName@ListElement@UI@@CAPEBDXZ                        -> "ListElement"

0x18004a200  ?_TypeName@NumberDescription@UI@@CAPEBDXZ                  -> "NumberDescription"

0x180045e50  ?_TypeName@NumberEditDescription@UI@@CAPEBDXZ              -> "NumberEditDescription"

0x180046210  ?_TypeName@NumberEditElement@UI@@CAPEBDXZ                  -> "NumberEditElement"

0x18004a7e0  ?_TypeName@NumberElement@UI@@CAPEBDXZ                      -> "NumberElement"

0x180046ea0  ?_TypeName@PageDescription@UI@@CAPEBDXZ                    -> "PageDescription"

0x180047230  ?_TypeName@PageElement@UI@@CAPEBDXZ                        -> "PageElement"

0x180077730  ?_TypeName@ProgressDescription@UI@@CAPEBDXZ                -> "ProgressDescription"

0x180077d00  ?_TypeName@ProgressElement@UI@@CAPEBDXZ                    -> "ProgressElement"

0x180047630  ?_TypeName@RadioDescription@UI@@CAPEBDXZ                   -> "RadioDescription"

0x1800479c0  ?_TypeName@RadioElement@UI@@CAPEBDXZ                       -> "RadioElement"

0x1800440f0  ?_TypeName@RibbonDescription@UI@@CAPEBDXZ                  -> "RibbonDescription"

0x1800446a0  ?_TypeName@RibbonElement@UI@@CAPEBDXZ                      -> "RibbonElement"

0x18003a0e0  ?_TypeName@ScrollDescription@UI@@CAPEBDXZ                  -> "ScrollDescription"

0x18003a810  ?_TypeName@ScrollElement@UI@@CAPEBDXZ                      -> "ScrollElement"

0x180049710  ?_TypeName@SliderDescription@UI@@CAPEBDXZ                  -> "SliderDescription"

0x180049ce0  ?_TypeName@SliderElement@UI@@CAPEBDXZ                      -> "SliderElement"

0x18004ad20  ?_TypeName@SpinnerDescription@UI@@CAPEBDXZ                 -> "SpinnerDescription"

0x18004b0d0  ?_TypeName@SpinnerElement@UI@@CAPEBDXZ                     -> "SpinnerElement"

0x18003c7d0  ?_TypeName@SpriteCapture@UI@@CAPEBDXZ                      -> "SpriteCapture"

0x180002e10  ?_TypeName@SpriteSheet@UI@@CAPEBDXZ                        -> "SpriteSheet"

0x180002150  ?_TypeName@StringTable@UI@@CAPEBDXZ                        -> "StringTable"

0x18004c1b0  ?_TypeName@TabDescription@UI@@CAPEBDXZ                     -> "TabDescription"

0x18004c6c0  ?_TypeName@TabElement@UI@@CAPEBDXZ                         -> "TabElement"

0x180078e90  ?_TypeName@TableDescription@UI@@CAPEBDXZ                   -> "TableDescription"

0x180079290  ?_TypeName@TableElement@UI@@CAPEBDXZ                       -> "TableElement"

0x180041a90  ?_TypeName@TextDescription@UI@@CAPEBDXZ                    -> "TextDescription"

0x180041eb0  ?_TypeName@TextElement@UI@@CAPEBDXZ                        -> "TextElement"

0x18004cf80  ?_TypeName@ToolTipDialog@UI@@CAPEBDXZ                      -> "ToolTipDialog"



@@@@@@ SECTION 4 -- hidden-retslot exports (?AV / ?AU); ?AW4 listed separately

retslot count=73

0x180001380  ?GetStringTable@Achievement@1@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ                                UI::VStringTable::?$ExternalPtr  [class by value]

0x180001390  ?GetSpriteSheet@Achievement@1@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ                                UI::VSpriteSheet::?$ExternalPtr  [class by value]

0x180001b90  ?GetStringTable@Goal@Achievement@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ                            UI::VStringTable::?$ExternalPtr  [class by value]

0x180002550  ?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z                                                             System::String  [class by value]

0x1800025b0  ?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z                                                       System::String  [class by value]

0x1800032d0  ?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z                                              System::String  [class by value]

0x180003320  ?GetMaterialInstance@SpriteSheet@UI@@QEBA?AV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@XZ              Graphics::VMaterialInstance::?$ExternalPtr  [class by value]

0x180030ca0  ?GetSpriteSheet@Sprite@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ                                   UI::VSpriteSheet::?$ExternalPtr  [class by value]

0x180031150  ?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@@Collection@@XZ UI::VDialogInstance::V?$InternalPtr::?$LinkedList::Iterator  [struct by value]

0x180031d90  ?GetSpriteSheet@Font@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ                                     UI::VSpriteSheet::?$ExternalPtr  [class by value]

0x180035020  ??BString@UI@@QEBA?AV0System@@XZ                                                                               0System  [class by value]

0x180035080  ?GetStringTable@String@UI@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ                                   UI::VStringTable::?$ExternalPtr  [class by value]

0x180035540  ?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z                                              UI::VElement::?$InternalPtr  [class by value]

0x180035560  ?Child@Element@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z                                              UI::VElement::?$InternalPtr  [class by value]

0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ                                           UI::VElement::?$InternalPtr  [class by value]

0x180035670  ?GetToolTipDialog@Element@UI@@QEBA?AV?$ExternalPtr@VDialog@UI@@@Object@@XZ                                     UI::VDialog::?$ExternalPtr  [class by value]

0x180035680  ?GetToolTip@Element@UI@@QEBA?AVString@2@XZ                                                                     2::String  [class by value]

0x1800356a0  ?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ                                                         System::String  [class by value]

0x1800357d0  ?GetDescription@Element@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@XZ                           UI::VElementDescription::?$ExternalPtr  [class by value]

0x180038860  ?GetBackgroundSprite@Dialog@UI@@QEBA?AVSprite@2@XZ                                                             2::Sprite  [class by value]

0x1800388b0  ?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z                              UI::VElementDescription::?$ExternalPtr  [class by value]

0x1800388d0  ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z                                   Object::VExternal::?$ExternalPtr  [class by value]

0x18003b250  ?GetSpriteSheet@Cursor@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ                                   UI::VSpriteSheet::?$ExternalPtr  [class by value]

0x18003b700  ?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ                               UI::VElement::?$InternalPtr  [class by value]

0x18003c0c0  ?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z                                                     System::String  [class by value]

0x1800420b0  ?GetText@TextElement@UI@@QEBA?AVString@2@XZ                                                                    2::String  [class by value]

0x1800429d0  ?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ                                                                 2::Sprite  [class by value]

0x180045570  ?GetParent@DialogInstance@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@XZ                             UI::VDialogInstance::?$InternalPtr  [class by value]

0x180045580  ?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ                                      UI::VElement::?$InternalPtr  [class by value]

0x180045590  ?GetRoot@DialogInstance@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ                                      UI::VElement::?$InternalPtr  [class by value]

0x180075190  ?GetItemText@ComboElement@UI@@QEBA?AVString@System@@H@Z                                                        System::String  [class by value]

0x1800cb480  ?CreateInstance@AnimatedImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z        UI::VElement::?$InternalPtr  [class by value]

0x1800cbec0  ?CreateInstance@ClipRegionDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z           UI::VElement::?$InternalPtr  [class by value]

0x1800ccb90  ?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z            UI::VElement::?$InternalPtr  [class by value]

0x1800cf7e0  ?CreateInstance@DocumentTextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z         UI::VElement::?$InternalPtr  [class by value]

0x1800d0f90  ?CreateInstance@EditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                 UI::VElement::?$InternalPtr  [class by value]

0x1800d1920  ?CreateInstance@GraphDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x1800d26b0  ?CreateInstance@LabelDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x1800d33a0  ?CreateInstance@LayoutDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x1800d45f0  ?CreateInstance@ListDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                 UI::VElement::?$InternalPtr  [class by value]

0x1800d4cc0  ?CreateInstance@NumberEditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z           UI::VElement::?$InternalPtr  [class by value]

0x1800d52b0  ?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                 UI::VElement::?$InternalPtr  [class by value]

0x1800d58d0  ?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x1800d6000  ?CreateInstance@RibbonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x1800d7630  ?CreateInstance@SliderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x1800d86b0  ?CreateInstance@SpinnerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z              UI::VElement::?$InternalPtr  [class by value]

0x1800da410  ?CreateInstance@TabDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                  UI::VElement::?$InternalPtr  [class by value]

0x18010ff40  ?CreateInstance@BorderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x180111510  ?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x1801119c0  ?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x180112a70  ?CreateInstance@ComboDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x180112c80  ?CreateElements@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ                                       UI::VElement::?$InternalPtr  [class by value]

0x180112ed0  ?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@@Z UI::VElement::?$InternalPtr  [class by value]

0x180113320  ?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z                      UI::VDialogController::?$InternalPtr  [class by value]

0x1801140c0  ?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z                             UI::VElement::?$InternalPtr  [class by value]

0x180114c70  ?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z               UI::VElement::?$InternalPtr  [class by value]

0x180115650  ?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z                      UI::VElement::?$InternalPtr  [class by value]

0x180115760  ?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z                                    UI::VElement::?$InternalPtr  [class by value]

0x180117dc0  ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x1801182a0  ?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ                                                                 2::Sprite  [class by value]

0x180118910  ?CreateInstance@ImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x180118f10  ?CreateInstance@NumberDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x180119d00  ?CreateInstance@ProgressDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z             UI::VElement::?$InternalPtr  [class by value]

0x18011aac0  ?CreateInstance@ScrollDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z               UI::VElement::?$InternalPtr  [class by value]

0x18011ca00  ?GetScreenScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ                                                  Video::Viewport  [class by value]

0x18011ca30  ?GetScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ                                                        Video::Viewport  [class by value]

0x18011d010  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z                                                    Video::Viewport  [class by value]

0x18011d050  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z                                                 Video::Viewport  [class by value]

0x18011e350  ?CreateInstance@TableDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                UI::VElement::?$InternalPtr  [class by value]

0x18011f3d0  ?CreateInstance@TextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z                 UI::VElement::?$InternalPtr  [class by value]

0x18011fb00  ?GetFirstContainedDialog@UserInterface@UI@@QEBA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z              UI::VDialogInstance::?$InternalPtr  [class by value]

0x180122470  ?SetClipRegion@UserInterface@UI@@QEAA?AUClipRegion@12@HHHH@Z                                                   HHHH::12::ClipRegion  [struct by value]

0x1801224d0  ?SetClipRegion@UserInterface@UI@@QEAA?AUClipRegion@12@AEBU312@@Z                                               AEBU312::12::ClipRegion  [struct by value]

-- NOT retslot: enum by value, returned in EAX. count=3

0x18004a9c0  ?GetNumberType@NumberElement@UI@@QEBA?AW4NumberType@NumberDescription@2@XZ                                     2::NumberDescription::NumberType  [enum by value]

0x1801176c0  ?Compile@Font@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                               Object::External::CompileResult  [enum by value]

0x18011d720  ?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                        Object::External::CompileResult  [enum by value]




@@@@@@ SECTION 5 -- functions referencing each UI vtable address (condensed: count + kinds; full lists in ui_appendix_raw.txt)
### UI::AnimatedImageDescription  vtable=0x180156060  refs=7
    (unexported) FUN_180036980  (unexported) FUN_180036980
    (unexported) FUN_1800369e0  (unexported) FUN_1800369e0
    <no fn> 1801b1fe8  <no fn> 1801b1fe8
    export-dir  <no fn> Entry Point
    ctor  ??0AnimatedImageDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0AnimatedImageDescription@UI@@QEAA@XZ
    dtor  ??1AnimatedImageDescription@UI@@UEAA@XZ
### UI::AnimatedImageElement  vtable=0x180156000  refs=7
    (unexported) FUN_180036d40  (unexported) FUN_180036d40
    (unexported) UI_AnimatedImageElement_VectorDeletingDtor  (unexported) UI_AnimatedImageElement_VectorDeletingDtor
    <no fn> 1801b1fec  <no fn> 1801b1fec
    export-dir  <no fn> Entry Point
    ctor  ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0AnimatedImageElement@UI@@QEAA@XZ
    dtor  ??1AnimatedImageElement@UI@@UEAA@XZ
### UI::BorderDescription  vtable=0x180156980  refs=5
    <no fn> 1801b2008  <no fn> 1801b2008
    export-dir  <no fn> Entry Point
    ctor  ??0BorderDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0BorderDescription@UI@@QEAA@XZ
    dtor  ??1BorderDescription@UI@@UEAA@XZ
### UI::BorderElement  vtable=0x180156920  refs=4
    <no fn> 1801b200c  <no fn> 1801b200c
    export-dir  <no fn> Entry Point
    ctor  ??0BorderElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0BorderElement@UI@@QEAA@XZ
### UI::ButtonDescription  vtable=0x180157370  refs=8
    (unexported) FUN_180048120  (unexported) FUN_180048120
    <no fn> 1801b2010  <no fn> 1801b2010
    export-dir  <no fn> Entry Point
    ctor  ??0ButtonDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ButtonDescription@UI@@QEAA@XZ
    dtor  ??1ButtonDescription@UI@@UEAA@XZ
    dtor  ??1CheckDescription@UI@@UEAA@XZ
    dtor  ??1ComboDescription@UI@@UEAA@XZ
### UI::ButtonElement  vtable=0x180157310  refs=19
    (unexported) FUN_180048560  (unexported) FUN_180048560
    (unexported) FUN_180048d20  (unexported) FUN_180048d20
    (unexported) UI_ButtonElement_VectorDeletingDtor  (unexported) UI_ButtonElement_VectorDeletingDtor
    (unexported) UI_CheckElement_VectorDeletingDtor  (unexported) UI_CheckElement_VectorDeletingDtor
    <no fn> 1801b2014  <no fn> 1801b2014
    export-dir  <no fn> Entry Point
    ctor  ??0ButtonElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ButtonElement@UI@@QEAA@XZ
    ctor  ??0CheckElement@UI@@QEAA@XZ
    ctor  ??0ComboElement@UI@@QEAA@XZ
    dtor  ??1ButtonElement@UI@@UEAA@XZ
    dtor  ??1CheckElement@UI@@UEAA@XZ
    dtor  ??1ComboElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ButtonElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@CheckElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateInstance  ?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@ButtonElement@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@CheckElement@UI@@SAPEAVBase@Object@@XZ
### UI::CharacterSet  vtable=0x180156220  refs=11
    (unexported) FUN_180037820  (unexported) FUN_180037820
    (unexported) FUN_1800378a0  (unexported) FUN_1800378a0
    <no fn> 1801b2018  <no fn> 1801b2018
    export-dir  <no fn> Entry Point
    ctor  ??0CharacterSet@UI@@QEAA@AEBV01@@Z
    ctor  ??0CharacterSet@UI@@QEAA@XZ
    dtor  ??1CharacterSet@UI@@UEAA@XZ
    ?Construct  ?Construct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@CharacterSet@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@CharacterSet@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ
### UI::CheckDescription  vtable=0x1801572c0  refs=9
    <no fn> 1801b201c  <no fn> 1801b201c
    export-dir  <no fn> Entry Point
    ctor  ??0CheckDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0CheckDescription@UI@@QEAA@XZ
    dtor  ??1CheckDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@CheckDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@CheckDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::CheckElement  vtable=0x180157260  refs=10
    (unexported) FUN_180048d20  (unexported) FUN_180048d20
    (unexported) UI_CheckElement_VectorDeletingDtor  (unexported) UI_CheckElement_VectorDeletingDtor
    <no fn> 1801b2020  <no fn> 1801b2020
    export-dir  <no fn> Entry Point
    ctor  ??0CheckElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0CheckElement@UI@@QEAA@XZ
    dtor  ??1CheckElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@CheckElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@CheckElement@UI@@SAPEAVBase@Object@@XZ
### UI::ClipRegionDescription  vtable=0x180156350  refs=10
    (unexported) FUN_180037e30  (unexported) FUN_180037e30
    <no fn> 1801b2024  <no fn> 1801b2024
    export-dir  <no fn> Entry Point
    ctor  ??0ClipRegionDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ClipRegionDescription@UI@@QEAA@XZ
    dtor  ??1ClipRegionDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::ClipRegionElement  vtable=0x1801562f0  refs=11
    (unexported) FUN_180038320  (unexported) FUN_180038320
    (unexported) UI_ClipRegionElement_VectorDeletingDtor  (unexported) UI_ClipRegionElement_VectorDeletingDtor
    <no fn> 1801b2028  <no fn> 1801b2028
    export-dir  <no fn> Entry Point
    ctor  ??0ClipRegionElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ClipRegionElement@UI@@QEAA@XZ
    dtor  ??1ClipRegionElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ClipRegionElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@ClipRegionDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateInstance  ?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@ClipRegionElement@UI@@SAPEAVBase@Object@@XZ
### UI::ComboDescription  vtable=0x180159090  refs=9
    <no fn> 1801b2030  <no fn> 1801b2030
    export-dir  <no fn> Entry Point
    ctor  ??0ComboDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ComboDescription@UI@@QEAA@XZ
    dtor  ??1ComboDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ComboDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ComboDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::ComboDialog  vtable=0x1801590e0  refs=10
    (unexported) FUN_1800749c0  (unexported) FUN_1800749c0
    (unexported) FUN_180074a10  (unexported) FUN_180074a10
    <no fn> 1801b2034  <no fn> 1801b2034
    export-dir  <no fn> Entry Point
    ctor  ??0ComboDialog@UI@@QEAA@AEBV01@@Z
    ctor  ??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDia
    ctor  ??0ComboDialog@UI@@QEAA@XZ
    dtor  ??1ComboDialog@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ComboDialog@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ComboDialog@UI@@SAPEAVBase@Object@@XZ
### UI::ComboElement  vtable=0x180159030  refs=5
    <no fn> 1801b2038  <no fn> 1801b2038
    export-dir  <no fn> Entry Point
    ctor  ??0ComboElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ComboElement@UI@@QEAA@XZ
    dtor  ??1ComboElement@UI@@UEAA@XZ
### UI::ContainerDescription  vtable=0x180156510  refs=6
    (unexported) FUN_180039b80  (unexported) FUN_180039b80
    <no fn> 1801b2054  <no fn> 1801b2054
    export-dir  <no fn> Entry Point
    ctor  ??0ContainerDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ContainerDescription@UI@@QEAA@XZ
    dtor  ??1ContainerDescription@UI@@UEAA@XZ
### UI::ContainerElement  vtable=0x1801564b0  refs=10
    (unexported) FUN_18003a000  (unexported) FUN_18003a000
    (unexported) UI_ContainerElement_VectorDeletingDtor  (unexported) UI_ContainerElement_VectorDeletingDtor
    <no fn> 1801b2058  <no fn> 1801b2058
    export-dir  <no fn> Entry Point
    ctor  ??0ContainerElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ContainerElement@UI@@QEAA@XZ
    dtor  ??1ContainerElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ContainerElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@ContainerElement@UI@@SAPEAVBase@Object@@XZ
### UI::Cursor  vtable=0x180156570  refs=11
    (unexported) FUN_18003b350  (unexported) FUN_18003b350
    (unexported) FUN_18003b3a0  (unexported) FUN_18003b3a0
    <no fn> 1801b2060  <no fn> 1801b2060
    export-dir  <no fn> Entry Point
    ctor  ??0Cursor@UI@@QEAA@AEBV01@@Z
    ctor  ??0Cursor@UI@@QEAA@XZ
    dtor  ??1Cursor@UI@@UEAA@XZ
    ?Construct  ?Construct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@Cursor@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@Cursor@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ
### UI::Dialog  vtable=0x1801562a0  refs=13
    <no fn> 1801b2068  <no fn> 1801b2068
    export-dir  <no fn> Entry Point
    ctor  ??0Dialog@UI@@QEAA@AEBV01@@Z
    ctor  ??0Dialog@UI@@QEAA@XZ
    dtor  ??1Dialog@UI@@UEAA@XZ
    ?AddController  ?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@
    ?AddTab  ?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription
    ?Construct  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z
    ?CreateNewObject  ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
    ?Update  ?Update@ComboElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI
### UI::DialogController  vtable=0x1801565f0  refs=7
    (unexported) FUN_18003b7b0  (unexported) FUN_18003b7b0
    (unexported) FUN_18003b800  (unexported) FUN_18003b800
    <no fn> 1801b206c  <no fn> 1801b206c
    export-dir  <no fn> Entry Point
    ctor  ??0DialogController@UI@@QEAA@AEBV01@@Z
    ctor  ??0DialogController@UI@@QEAA@XZ
    dtor  ??1DialogController@UI@@UEAA@XZ
### UI::DialogControllerConfig  vtable=0x180156650  refs=7
    (unexported) FUN_18003b5b0  (unexported) FUN_18003b5b0
    (unexported) FUN_18003b600  (unexported) FUN_18003b600
    <no fn> 1801b2070  <no fn> 1801b2070
    export-dir  <no fn> Entry Point
    ctor  ??0DialogControllerConfig@UI@@QEAA@AEBV01@@Z
    ctor  ??0DialogControllerConfig@UI@@QEAA@XZ
    dtor  ??1DialogControllerConfig@UI@@UEAA@XZ
### UI::DialogInstance  vtable=0x180156d70  refs=7
    <no fn> 1801b2074  <no fn> 1801b2074
    export-dir  <no fn> Entry Point
    ctor  ??0DialogInstance@UI@@QEAA@AEBV01@@Z
    ctor  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Obje
    ctor  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@V
    ctor  ??0DialogInstance@UI@@QEAA@XZ
    dtor  ??1DialogInstance@UI@@UEAA@XZ
### UI::DocumentTextDescription  vtable=0x180156750  refs=5
    <no fn> 1801b2080  <no fn> 1801b2080
    export-dir  <no fn> Entry Point
    ctor  ??0DocumentTextDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0DocumentTextDescription@UI@@QEAA@XZ
    dtor  ??1DocumentTextDescription@UI@@UEAA@XZ
### UI::DocumentTextElement  vtable=0x1801566f0  refs=5
    <no fn> 1801b2084  <no fn> 1801b2084
    export-dir  <no fn> Entry Point
    ctor  ??0DocumentTextElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0DocumentTextElement@UI@@QEAA@XZ
    dtor  ??1DocumentTextElement@UI@@UEAA@XZ
### UI::EditDescription  vtable=0x180156880  refs=5
    <no fn> 1801b2090  <no fn> 1801b2090
    export-dir  <no fn> Entry Point
    ctor  ??0EditDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0EditDescription@UI@@QEAA@XZ
    dtor  ??1EditDescription@UI@@UEAA@XZ
### UI::EditElement  vtable=0x180156810  refs=5
    <no fn> 1801b2094  <no fn> 1801b2094
    export-dir  <no fn> Entry Point
    ctor  ??0EditElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0EditElement@UI@@QEAA@XZ
    dtor  ??1EditElement@UI@@UEAA@XZ
### UI::Element  vtable=0x180156160  refs=5
    <no fn> 1801b2098  <no fn> 1801b2098
    export-dir  <no fn> Entry Point
    ctor  ??0Element@UI@@QEAA@AEBV01@@Z
    ctor  ??0Element@UI@@QEAA@XZ
    dtor  ??1Element@UI@@UEAA@XZ
### UI::ElementDescription  vtable=0x1801561c0  refs=84
    (unexported) FUN_1800352d0  (unexported) FUN_1800352d0
    (unexported) FUN_180035350  (unexported) FUN_180035350
    (unexported) FUN_180037e30  (unexported) FUN_180037e30
    (unexported) FUN_180039b80  (unexported) FUN_180039b80
    (unexported) FUN_180046110  (unexported) FUN_180046110
    (unexported) FUN_180048120  (unexported) FUN_180048120
    (unexported) FUN_18004a6e0  (unexported) FUN_18004a6e0
    (unexported) FUN_18004afd0  (unexported) FUN_18004afd0
    <no fn> 1801b209c  <no fn> 1801b209c
    export-dir  <no fn> Entry Point
    ctor  ??0AnimatedImageDescription@UI@@QEAA@XZ
    ctor  ??0BorderDescription@UI@@QEAA@XZ
    ctor  ??0ButtonDescription@UI@@QEAA@XZ
    ctor  ??0ClipRegionDescription@UI@@QEAA@XZ
    ctor  ??0ContainerDescription@UI@@QEAA@XZ
    ctor  ??0DocumentTextDescription@UI@@QEAA@XZ
    ctor  ??0EditDescription@UI@@QEAA@XZ
    ctor  ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ElementDescription@UI@@QEAA@XZ
    ctor  ??0GraphDescription@UI@@QEAA@XZ
    ctor  ??0GroupDescription@UI@@QEAA@XZ
    ctor  ??0ImageDescription@UI@@QEAA@XZ
    ctor  ??0LayoutDescription@UI@@QEAA@XZ
    ctor  ??0ListDescription@UI@@QEAA@XZ
    ctor  ??0NumberDescription@UI@@QEAA@XZ
    ctor  ??0NumberEditDescription@UI@@QEAA@XZ
    ctor  ??0PageDescription@UI@@QEAA@XZ
    ctor  ??0ProgressDescription@UI@@QEAA@XZ
    ctor  ??0RadioDescription@UI@@QEAA@XZ
    ctor  ??0RibbonDescription@UI@@QEAA@XZ
    ctor  ??0ScrollDescription@UI@@QEAA@XZ
    ctor  ??0SliderDescription@UI@@QEAA@XZ
    ctor  ??0SpinnerDescription@UI@@QEAA@XZ
    ctor  ??0TabDescription@UI@@QEAA@XZ
    ctor  ??0TableDescription@UI@@QEAA@XZ
    ctor  ??0TextDescription@UI@@QEAA@XZ
    dtor  ??1BorderDescription@UI@@UEAA@XZ
    dtor  ??1ButtonDescription@UI@@UEAA@XZ
    dtor  ??1CheckDescription@UI@@UEAA@XZ
    dtor  ??1ClipRegionDescription@UI@@UEAA@XZ
    dtor  ??1ComboDescription@UI@@UEAA@XZ
    dtor  ??1ContainerDescription@UI@@UEAA@XZ
    dtor  ??1DocumentTextDescription@UI@@UEAA@XZ
    dtor  ??1EditDescription@UI@@UEAA@XZ
    dtor  ??1ElementDescription@UI@@UEAA@XZ
    dtor  ??1GraphDescription@UI@@UEAA@XZ
    dtor  ??1GroupDescription@UI@@UEAA@XZ
    dtor  ??1ImageDescription@UI@@UEAA@XZ
    dtor  ??1LayoutDescription@UI@@UEAA@XZ
    dtor  ??1ListDescription@UI@@UEAA@XZ
    dtor  ??1NumberDescription@UI@@UEAA@XZ
    dtor  ??1NumberEditDescription@UI@@UEAA@XZ
    dtor  ??1ProgressDescription@UI@@UEAA@XZ
    dtor  ??1RibbonDescription@UI@@UEAA@XZ
    dtor  ??1ScrollDescription@UI@@UEAA@XZ
    dtor  ??1SliderDescription@UI@@UEAA@XZ
    dtor  ??1SpinnerDescription@UI@@UEAA@XZ
    dtor  ??1TabDescription@UI@@UEAA@XZ
    dtor  ??1TableDescription@UI@@UEAA@XZ
    dtor  ??1TextDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Construct  ?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Construct  ?Construct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Construct  ?Construct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Construct  ?Construct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Construct  ?Construct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@GroupDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@LayoutDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@GroupDescription@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@LayoutDescription@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Reconstruct  ?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Reconstruct  ?Reconstruct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Reconstruct  ?Reconstruct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Reconstruct  ?Reconstruct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?Reconstruct  ?Reconstruct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::Font  vtable=0x180155b50  refs=9
    <no fn> 1801b20b4  <no fn> 1801b20b4
    export-dir  <no fn> Entry Point
    ctor  ??0Font@UI@@QEAA@AEBV01@@Z
    ctor  ??0Font@UI@@QEAA@XZ
    dtor  ??1Font@UI@@UEAA@XZ
    ?Construct  ?Construct@Font@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@Font@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@Font@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@Font@UI@@UEAAPEAVExternal@Object@@XZ
### UI::GraphDescription  vtable=0x180156a30  refs=5
    <no fn> 1801b20cc  <no fn> 1801b20cc
    export-dir  <no fn> Entry Point
    ctor  ??0GraphDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0GraphDescription@UI@@QEAA@XZ
    dtor  ??1GraphDescription@UI@@UEAA@XZ
### UI::GraphElement  vtable=0x1801569d0  refs=7
    (unexported) UI_GraphElement_VectorDeletingDtor  (unexported) UI_GraphElement_VectorDeletingDtor
    <no fn> 1801b20d0  <no fn> 1801b20d0
    export-dir  <no fn> Entry Point
    ctor  ??0GraphElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0GraphElement@UI@@QEAA@XZ
    dtor  ??1GraphElement@UI@@UEAA@XZ
    ??_GCMFCShadowWnd  ??_GCMFCShadowWnd@@EEAAPEAXI@Z
### UI::GroupDescription  vtable=0x180157190  refs=13
    <no fn> 1801b20d8  <no fn> 1801b20d8
    export-dir  <no fn> Entry Point
    ctor  ??0GroupDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0GroupDescription@UI@@QEAA@XZ
    ctor  ??0PageDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0PageDescription@UI@@QEAA@XZ
    ctor  ??0RadioDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0RadioDescription@UI@@QEAA@XZ
    dtor  ??1GroupDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@GroupDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@GroupDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::GroupElement  vtable=0x180157130  refs=26
    (unexported) FUN_180046dc0  (unexported) FUN_180046dc0
    (unexported) FUN_1800474e0  (unexported) FUN_1800474e0
    (unexported) FUN_180047ce0  (unexported) FUN_180047ce0
    (unexported) UI_GroupElement_VectorDeletingDtor  (unexported) UI_GroupElement_VectorDeletingDtor
    (unexported) UI_PageElement_VectorDeletingDtor  (unexported) UI_PageElement_VectorDeletingDtor
    (unexported) UI_RadioElement_VectorDeletingDtor  (unexported) UI_RadioElement_VectorDeletingDtor
    <no fn> 1801b20dc  <no fn> 1801b20dc
    export-dir  <no fn> Entry Point
    ctor  ??0GroupElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0GroupElement@UI@@QEAA@XZ
    ctor  ??0PageElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0PageElement@UI@@QEAA@XZ
    ctor  ??0RadioElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0RadioElement@UI@@QEAA@XZ
    dtor  ??1GroupElement@UI@@UEAA@XZ
    dtor  ??1PageElement@UI@@UEAA@XZ
    dtor  ??1RadioElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@GroupElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@PageElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@RadioElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateInstance  ?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateInstance  ?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@GroupElement@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@PageElement@UI@@SAPEAVBase@Object@@XZ
    ?CreateNewObject  ?CreateNewObject@RadioElement@UI@@SAPEAVBase@Object@@XZ
### UI::ImageDescription  vtable=0x180156110  refs=11
    <no fn> 1801b20ec  <no fn> 1801b20ec
    export-dir  <no fn> Entry Point
    ctor  ??0AnimatedImageDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0AnimatedImageDescription@UI@@QEAA@XZ
    ctor  ??0ImageDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ImageDescription@UI@@QEAA@XZ
    dtor  ??1ImageDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::ImageElement  vtable=0x1801560b0  refs=14
    (unexported) FUN_1800365b0  (unexported) FUN_1800365b0
    (unexported) FUN_180036d40  (unexported) FUN_180036d40
    (unexported) UI_AnimatedImageElement_VectorDeletingDtor  (unexported) UI_AnimatedImageElement_VectorDeletingDtor
    (unexported) UI_ImageElement_VectorDeletingDtor  (unexported) UI_ImageElement_VectorDeletingDtor
    <no fn> 1801b20f0  <no fn> 1801b20f0
    export-dir  <no fn> Entry Point
    ctor  ??0AnimatedImageElement@UI@@QEAA@XZ
    ctor  ??0ImageElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ImageElement@UI@@QEAA@XZ
    dtor  ??1AnimatedImageElement@UI@@UEAA@XZ
    dtor  ??1ImageElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@ImageElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@ImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@ImageElement@UI@@SAPEAVBase@Object@@XZ
### UI::LabelDescription  vtable=0x180156b30  refs=5
    <no fn> 1801b20fc  <no fn> 1801b20fc
    export-dir  <no fn> Entry Point
    ctor  ??0LabelDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0LabelDescription@UI@@QEAA@XZ
    dtor  ??1LabelDescription@UI@@UEAA@XZ
### UI::LabelElement  vtable=0x180156ad0  refs=7
    (unexported) FUN_180042b60  (unexported) FUN_180042b60
    (unexported) UI_LabelElement_VectorDeletingDtor  (unexported) UI_LabelElement_VectorDeletingDtor
    <no fn> 1801b2100  <no fn> 1801b2100
    export-dir  <no fn> Entry Point
    ctor  ??0LabelElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0LabelElement@UI@@QEAA@XZ
    dtor  ??1LabelElement@UI@@UEAA@XZ
### UI::LayoutDescription  vtable=0x180156cc0  refs=9
    <no fn> 1801b2104  <no fn> 1801b2104
    export-dir  <no fn> Entry Point
    ctor  ??0LayoutDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0LayoutDescription@UI@@QEAA@XZ
    dtor  ??1LayoutDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@LayoutDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@LayoutDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::LayoutElement  vtable=0x180156c60  refs=10
    (unexported) UI_LayoutElement_VectorDeletingDtor  (unexported) UI_LayoutElement_VectorDeletingDtor
    <no fn> 1801b2108  <no fn> 1801b2108
    export-dir  <no fn> Entry Point
    ctor  ??0LayoutElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0LayoutElement@UI@@QEAA@XZ
    dtor  ??1LayoutElement@UI@@UEAA@XZ
    ??_GCMFCShadowWnd  ??_GCMFCShadowWnd@@EEAAPEAXI@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@LayoutElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@LayoutDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@LayoutElement@UI@@SAPEAVBase@Object@@XZ
### UI::ListDescription  vtable=0x180156e20  refs=5
    <no fn> 1801b2110  <no fn> 1801b2110
    export-dir  <no fn> Entry Point
    ctor  ??0ListDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ListDescription@UI@@QEAA@XZ
    dtor  ??1ListDescription@UI@@UEAA@XZ
### UI::ListElement  vtable=0x180156dc0  refs=7
    (unexported) UI_ListElement_VectorDeletingDtor  (unexported) UI_ListElement_VectorDeletingDtor
    <no fn> 1801b2114  <no fn> 1801b2114
    export-dir  <no fn> Entry Point
    ctor  ??0ListElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ListElement@UI@@QEAA@XZ
    dtor  ??1ListElement@UI@@UEAA@XZ
    ??_GCMFCShadowWnd  ??_GCMFCShadowWnd@@EEAAPEAXI@Z
### UI::NumberDescription  vtable=0x180157530  refs=10
    (unexported) FUN_18004a6e0  (unexported) FUN_18004a6e0
    <no fn> 1801b2130  <no fn> 1801b2130
    export-dir  <no fn> Entry Point
    ctor  ??0NumberDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0NumberDescription@UI@@QEAA@XZ
    dtor  ??1NumberDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::NumberEditDescription  vtable=0x180156fe0  refs=6
    (unexported) FUN_180046110  (unexported) FUN_180046110
    <no fn> 1801b2134  <no fn> 1801b2134
    export-dir  <no fn> Entry Point
    ctor  ??0NumberEditDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0NumberEditDescription@UI@@QEAA@XZ
    dtor  ??1NumberEditDescription@UI@@UEAA@XZ
### UI::NumberEditElement  vtable=0x180156f70  refs=10
    (unexported) FUN_1800464f0  (unexported) FUN_1800464f0
    (unexported) UI_NumberEditElement_VectorDeletingDtor  (unexported) UI_NumberEditElement_VectorDeletingDtor
    <no fn> 1801b2138  <no fn> 1801b2138
    export-dir  <no fn> Entry Point
    ctor  ??0NumberEditElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0NumberEditElement@UI@@QEAA@XZ
    dtor  ??1NumberEditElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@NumberEditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ
### UI::NumberElement  vtable=0x1801574d0  refs=5
    <no fn> 1801b213c  <no fn> 1801b213c
    export-dir  <no fn> Entry Point
    ctor  ??0NumberElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0NumberElement@UI@@QEAA@XZ
    dtor  ??1NumberElement@UI@@UEAA@XZ
### UI::PageDescription  vtable=0x1801570e0  refs=7
    (unexported) FUN_180047150  (unexported) FUN_180047150
    (unexported) FUN_1800471a0  (unexported) FUN_1800471a0
    <no fn> 1801b2144  <no fn> 1801b2144
    export-dir  <no fn> Entry Point
    ctor  ??0PageDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0PageDescription@UI@@QEAA@XZ
    dtor  ??1PageDescription@UI@@UEAA@XZ
### UI::PageElement  vtable=0x180157080  refs=10
    (unexported) FUN_1800474e0  (unexported) FUN_1800474e0
    (unexported) UI_PageElement_VectorDeletingDtor  (unexported) UI_PageElement_VectorDeletingDtor
    <no fn> 1801b2148  <no fn> 1801b2148
    export-dir  <no fn> Entry Point
    ctor  ??0PageElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0PageElement@UI@@QEAA@XZ
    dtor  ??1PageElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@PageElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@PageElement@UI@@SAPEAVBase@Object@@XZ
### UI::ProgressDescription  vtable=0x1801591e0  refs=5
    <no fn> 1801b216c  <no fn> 1801b216c
    export-dir  <no fn> Entry Point
    ctor  ??0ProgressDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ProgressDescription@UI@@QEAA@XZ
    dtor  ??1ProgressDescription@UI@@UEAA@XZ
### UI::ProgressElement  vtable=0x180159180  refs=4
    <no fn> 1801b2170  <no fn> 1801b2170
    export-dir  <no fn> Entry Point
    ctor  ??0ProgressElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ProgressElement@UI@@QEAA@XZ
### UI::RadioDescription  vtable=0x180157420  refs=7
    (unexported) FUN_1800478e0  (unexported) FUN_1800478e0
    (unexported) FUN_180047930  (unexported) FUN_180047930
    <no fn> 1801b2174  <no fn> 1801b2174
    export-dir  <no fn> Entry Point
    ctor  ??0RadioDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0RadioDescription@UI@@QEAA@XZ
    dtor  ??1RadioDescription@UI@@UEAA@XZ
### UI::RadioElement  vtable=0x1801573c0  refs=10
    (unexported) FUN_180047ce0  (unexported) FUN_180047ce0
    (unexported) UI_RadioElement_VectorDeletingDtor  (unexported) UI_RadioElement_VectorDeletingDtor
    <no fn> 1801b2178  <no fn> 1801b2178
    export-dir  <no fn> Entry Point
    ctor  ??0RadioElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0RadioElement@UI@@QEAA@XZ
    dtor  ??1RadioElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@RadioElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@RadioElement@UI@@SAPEAVBase@Object@@XZ
### UI::RibbonDescription  vtable=0x180156ed0  refs=9
    <no fn> 1801b2190  <no fn> 1801b2190
    export-dir  <no fn> Entry Point
    ctor  ??0RibbonDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0RibbonDescription@UI@@QEAA@XZ
    dtor  ??1RibbonDescription@UI@@UEAA@XZ
    ?Construct  ?Construct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ
### UI::RibbonElement  vtable=0x180156e70  refs=10
    (unexported) UI_RibbonElement_VectorDeletingDtor  (unexported) UI_RibbonElement_VectorDeletingDtor
    <no fn> 1801b2194  <no fn> 1801b2194
    export-dir  <no fn> Entry Point
    ctor  ??0RibbonElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0RibbonElement@UI@@QEAA@XZ
    dtor  ??1RibbonElement@UI@@UEAA@XZ
    ??_GCMFCShadowWnd  ??_GCMFCShadowWnd@@EEAAPEAXI@Z
    ?CreateInPlaceObject  ?CreateInPlaceObject@RibbonElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@RibbonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@RibbonElement@UI@@SAPEAVBase@Object@@XZ
### UI::ScrollDescription  vtable=0x180156460  refs=5
    <no fn> 1801b2198  <no fn> 1801b2198
    export-dir  <no fn> Entry Point
    ctor  ??0ScrollDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0ScrollDescription@UI@@QEAA@XZ
    dtor  ??1ScrollDescription@UI@@UEAA@XZ
### UI::ScrollElement  vtable=0x180156400  refs=7
    (unexported) FUN_18003ad70  (unexported) FUN_18003ad70
    (unexported) UI_ScrollElement_VectorDeletingDtor  (unexported) UI_ScrollElement_VectorDeletingDtor
    <no fn> 1801b219c  <no fn> 1801b219c
    export-dir  <no fn> Entry Point
    ctor  ??0ScrollElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0ScrollElement@UI@@QEAA@XZ
    dtor  ??1ScrollElement@UI@@UEAA@XZ
### UI::SliderDescription  vtable=0x1801575e0  refs=5
    <no fn> 1801b21a4  <no fn> 1801b21a4
    export-dir  <no fn> Entry Point
    ctor  ??0SliderDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0SliderDescription@UI@@QEAA@XZ
    dtor  ??1SliderDescription@UI@@UEAA@XZ
### UI::SliderElement  vtable=0x180157580  refs=4
    <no fn> 1801b21a8  <no fn> 1801b21a8
    export-dir  <no fn> Entry Point
    ctor  ??0SliderElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0SliderElement@UI@@QEAA@XZ
### UI::SpinnerDescription  vtable=0x1801576c0  refs=6
    (unexported) FUN_18004afd0  (unexported) FUN_18004afd0
    <no fn> 1801b21b4  <no fn> 1801b21b4
    export-dir  <no fn> Entry Point
    ctor  ??0SpinnerDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0SpinnerDescription@UI@@QEAA@XZ
    dtor  ??1SpinnerDescription@UI@@UEAA@XZ
### UI::SpinnerElement  vtable=0x180157660  refs=10
    (unexported) FUN_18004b360  (unexported) FUN_18004b360
    (unexported) UI_SpinnerElement_VectorDeletingDtor  (unexported) UI_SpinnerElement_VectorDeletingDtor
    <no fn> 1801b21b8  <no fn> 1801b21b8
    export-dir  <no fn> Entry Point
    ctor  ??0SpinnerElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0SpinnerElement@UI@@QEAA@XZ
    dtor  ??1SpinnerElement@UI@@UEAA@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@SpinnerElement@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateInstance  ?CreateInstance@SpinnerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
    ?CreateNewObject  ?CreateNewObject@SpinnerElement@UI@@SAPEAVBase@Object@@XZ
### UI::SpriteCapture  vtable=0x1801566a0  refs=11
    (unexported) FUN_18003cc80  (unexported) FUN_18003cc80
    (unexported) FUN_18003ccf0  (unexported) FUN_18003ccf0
    <no fn> 1801b21c0  <no fn> 1801b21c0
    export-dir  <no fn> Entry Point
    ctor  ??0SpriteCapture@UI@@QEAA@AEBV01@@Z
    ctor  ??0SpriteCapture@UI@@QEAA@XZ
    dtor  ??1SpriteCapture@UI@@UEAA@XZ
    ?Construct  ?Construct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@SpriteCapture@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@SpriteCapture@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ
### UI::SpriteSheet  vtable=0x180154590  refs=10
    <no fn> 1801b21c4  <no fn> 1801b21c4
    export-dir  <no fn> Entry Point
    ctor  ??0SpriteSheet@UI@@QEAA@AEBV01@@Z
    ctor  ??0SpriteSheet@UI@@QEAA@XZ
    dtor  ??1SpriteSheet@UI@@UEAA@XZ
    ?Construct  ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ
    ?Draw  ?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z
    ?Reconstruct  ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ
### UI::StringTable  vtable=0x180154630  refs=11
    (unexported) FUN_1800026e0  (unexported) FUN_1800026e0
    (unexported) FUN_180002740  (unexported) FUN_180002740
    <no fn> 1801b21d0  <no fn> 1801b21d0
    export-dir  <no fn> Entry Point
    ctor  ??0StringTable@UI@@QEAA@AEBV01@@Z
    ctor  ??0StringTable@UI@@QEAA@XZ
    dtor  ??1StringTable@UI@@UEAA@XZ
    ?Construct  ?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ
    ?CreateInPlaceObject  ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
    ?Reconstruct  ?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ
### UI::TabDescription  vtable=0x1801577f0  refs=5
    <no fn> 1801b21dc  <no fn> 1801b21dc
    export-dir  <no fn> Entry Point
    ctor  ??0TabDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0TabDescription@UI@@QEAA@XZ
    dtor  ??1TabDescription@UI@@UEAA@XZ
### UI::TabElement  vtable=0x180157790  refs=7
    (unexported) FUN_18004ca40  (unexported) FUN_18004ca40
    (unexported) UI_TabElement_VectorDeletingDtor  (unexported) UI_TabElement_VectorDeletingDtor
    <no fn> 1801b21e0  <no fn> 1801b21e0
    export-dir  <no fn> Entry Point
    ctor  ??0TabElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0TabElement@UI@@QEAA@XZ
    dtor  ??1TabElement@UI@@UEAA@XZ
### UI::TableDescription  vtable=0x1801592c0  refs=5
    <no fn> 1801b21e4  <no fn> 1801b21e4
    export-dir  <no fn> Entry Point
    ctor  ??0TableDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0TableDescription@UI@@QEAA@XZ
    dtor  ??1TableDescription@UI@@UEAA@XZ
### UI::TableElement  vtable=0x180159260  refs=7
    (unexported) FUN_180079590  (unexported) FUN_180079590
    (unexported) UI_TableElement_VectorDeletingDtor  (unexported) UI_TableElement_VectorDeletingDtor
    <no fn> 1801b21e8  <no fn> 1801b21e8
    export-dir  <no fn> Entry Point
    ctor  ??0TableElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0TableElement@UI@@QEAA@XZ
    dtor  ??1TableElement@UI@@UEAA@XZ
### UI::TextDescription  vtable=0x180156be0  refs=5
    <no fn> 1801b21ec  <no fn> 1801b21ec
    export-dir  <no fn> Entry Point
    ctor  ??0TextDescription@UI@@QEAA@AEBV01@@Z
    ctor  ??0TextDescription@UI@@QEAA@XZ
    dtor  ??1TextDescription@UI@@UEAA@XZ
### UI::TextElement  vtable=0x180156b80  refs=5
    <no fn> 1801b21f0  <no fn> 1801b21f0
    export-dir  <no fn> Entry Point
    ctor  ??0TextElement@UI@@QEAA@AEBV01@@Z
    ctor  ??0TextElement@UI@@QEAA@XZ
    dtor  ??1TextElement@UI@@UEAA@XZ
### UI::ToolTipDialog  vtable=0x180157880  refs=11
    (unexported) FUN_18004d220  (unexported) FUN_18004d220
    (unexported) FUN_18004d270  (unexported) FUN_18004d270
    <no fn> 1801b21fc  <no fn> 1801b21fc
    export-dir  <no fn> Entry Point
    ctor  ??0ToolTipDialog@UI@@QEAA@AEBV01@@Z
    ctor  ??0ToolTipDialog@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Objec
    ctor  ??0ToolTipDialog@UI@@QEAA@XZ
    dtor  ??1ToolTipDialog@UI@@UEAA@XZ
    ?BeginToolTip  ?BeginToolTip@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VEle
    ?CreateInPlaceObject  ?CreateInPlaceObject@ToolTipDialog@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject  ?CreateNewObject@ToolTipDialog@UI@@SAPEAVBase@Object@@XZ

@@@@@@ SECTION 6 -- every UI export, by class (nested classes bucket as Outer::Inner)
(section 7 follows section 6 in the raw file; here it is condensed at the end)
classes=75



##### UI::AnimatedImageDescription   exports=22 vtable=0x180156060

    0x180036890  ??0AnimatedImageDescription@UI@@QEAA@AEBV01@@Z

    0x1800cb6f0  ??0AnimatedImageDescription@UI@@QEAA@XZ

    0x1800cb6b0  ??1AnimatedImageDescription@UI@@UEAA@XZ

    0x1800366e0  ??2AnimatedImageDescription@UI@@SAPEAX_K@Z

    0x180036750  ??2AnimatedImageDescription@UI@@SAPEAX_KPEAX@Z

    0x180036760  ??3AnimatedImageDescription@UI@@SAXPEAX@Z

    0x180036930  ??4AnimatedImageDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156060  ??_7AnimatedImageDescription@UI@@6B@

    0x180036870  ?Construct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180036840  ?CreateInPlaceObject@AnimatedImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800cb480  ?CreateInstance@AnimatedImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x1800367c0  ?CreateNewObject@AnimatedImageDescription@UI@@SAPEAVBase@Object@@XZ

    0x180036880  ?Destruct@AnimatedImageDescription@UI@@UEAAXXZ

    0x1800cb6a0  ?GetFileVersion@AnimatedImageDescription@UI@@UEBAHXZ

    0x180036850  ?Reconstruct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cb5c0  ?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800366b0  ?Type@AnimatedImageDescription@UI@@UEBAIXZ

    0x1800366c0  ?TypeId@AnimatedImageDescription@UI@@SAIXZ

    0x1800366d0  ?TypeName@AnimatedImageDescription@UI@@UEBAPEBDXZ

    0x1800366a0  ?_ParentTypeName@AnimatedImageDescription@UI@@CAPEBDXZ

    0x180036690  ?_TypeName@AnimatedImageDescription@UI@@CAPEBDXZ

    0x1801f9c88  ?_typeInfo@AnimatedImageDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::AnimatedImageElement   exports=17 vtable=0x180156000

    0x180036c30  ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z

    0x1800cb3f0  ??0AnimatedImageElement@UI@@QEAA@XZ

    0x1800cb3a0  ??1AnimatedImageElement@UI@@UEAA@XZ

    0x180036ac0  ??2AnimatedImageElement@UI@@SAPEAX_K@Z

    0x180036b30  ??2AnimatedImageElement@UI@@SAPEAX_KPEAX@Z

    0x180036b40  ??3AnimatedImageElement@UI@@SAXPEAX@Z

    0x180036cb0  ??4AnimatedImageElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156000  ??_7AnimatedImageElement@UI@@6B@

    0x180036c20  ?CreateInPlaceObject@AnimatedImageElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180036ba0  ?CreateNewObject@AnimatedImageElement@UI@@SAPEAVBase@Object@@XZ

    0x180036a90  ?Type@AnimatedImageElement@UI@@UEBAIXZ

    0x180036aa0  ?TypeId@AnimatedImageElement@UI@@SAIXZ

    0x180036ab0  ?TypeName@AnimatedImageElement@UI@@UEBAPEBDXZ

    0x1800cb290  ?Update@AnimatedImageElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180036a80  ?_ParentTypeName@AnimatedImageElement@UI@@CAPEBDXZ

    0x180036a70  ?_TypeName@AnimatedImageElement@UI@@CAPEBDXZ

    0x1801f9cc0  ?_typeInfo@AnimatedImageElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::BorderDescription   exports=22 vtable=0x180156980

    0x180040ba0  ??0BorderDescription@UI@@QEAA@AEBV01@@Z

    0x18010fa50  ??0BorderDescription@UI@@QEAA@XZ

    0x18010fb20  ??1BorderDescription@UI@@UEAA@XZ

    0x1800409f0  ??2BorderDescription@UI@@SAPEAX_K@Z

    0x180040a60  ??2BorderDescription@UI@@SAPEAX_KPEAX@Z

    0x180040a70  ??3BorderDescription@UI@@SAXPEAX@Z

    0x180040ce0  ??4BorderDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156980  ??_7BorderDescription@UI@@6B@

    0x180040b80  ?Construct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180040b50  ?CreateInPlaceObject@BorderDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18010ff40  ?CreateInstance@BorderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180040ad0  ?CreateNewObject@BorderDescription@UI@@SAPEAVBase@Object@@XZ

    0x180040b90  ?Destruct@BorderDescription@UI@@UEAAXXZ

    0x18010fe10  ?GetFileVersion@BorderDescription@UI@@UEBAHXZ

    0x180040b60  ?Reconstruct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18010fe20  ?Serialize@BorderDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800409c0  ?Type@BorderDescription@UI@@UEBAIXZ

    0x1800409d0  ?TypeId@BorderDescription@UI@@SAIXZ

    0x1800409e0  ?TypeName@BorderDescription@UI@@UEBAPEBDXZ

    0x1800409b0  ?_ParentTypeName@BorderDescription@UI@@CAPEBDXZ

    0x1800409a0  ?_TypeName@BorderDescription@UI@@CAPEBDXZ

    0x1801fa890  ?_typeInfo@BorderDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::BorderElement   exports=19 vtable=0x180156920

    0x180041250  ??0BorderElement@UI@@QEAA@AEBV01@@Z

    0x18010fd40  ??0BorderElement@UI@@QEAA@XZ

    0x180041230  ??1BorderElement@UI@@UEAA@XZ

    0x1800410c0  ??2BorderElement@UI@@SAPEAX_K@Z

    0x180041130  ??2BorderElement@UI@@SAPEAX_KPEAX@Z

    0x180041140  ??3BorderElement@UI@@SAXPEAX@Z

    0x180041390  ??4BorderElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156920  ??_7BorderElement@UI@@6B@

    0x180110360  ?ComputeLayout@BorderElement@UI@@UEAAXHHHH@Z

    0x1801101d0  ?ComputeSize@BorderElement@UI@@UEAAXXZ

    0x180041220  ?CreateInPlaceObject@BorderElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800411a0  ?CreateNewObject@BorderElement@UI@@SAPEAVBase@Object@@XZ

    0x1801103e0  ?Draw@BorderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180041090  ?Type@BorderElement@UI@@UEBAIXZ

    0x1800410a0  ?TypeId@BorderElement@UI@@SAIXZ

    0x1800410b0  ?TypeName@BorderElement@UI@@UEBAPEBDXZ

    0x180041080  ?_ParentTypeName@BorderElement@UI@@CAPEBDXZ

    0x180041070  ?_TypeName@BorderElement@UI@@CAPEBDXZ

    0x1801fa8c8  ?_typeInfo@BorderElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ButtonDescription   exports=23 vtable=0x180157370

    0x180047ff0  ??0ButtonDescription@UI@@QEAA@AEBV01@@Z

    0x180110b10  ??0ButtonDescription@UI@@QEAA@XZ

    0x180110bd0  ??1ButtonDescription@UI@@UEAA@XZ

    0x180047e40  ??2ButtonDescription@UI@@SAPEAX_K@Z

    0x180047eb0  ??2ButtonDescription@UI@@SAPEAX_KPEAX@Z

    0x180047ec0  ??3ButtonDescription@UI@@SAXPEAX@Z

    0x180048090  ??4ButtonDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157370  ??_7ButtonDescription@UI@@6B@

    0x180047fd0  ?Construct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180047fa0  ?CreateInPlaceObject@ButtonDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1801108d0  ?CreateInstance@ButtonDescription@UI@@QEBAXV?$InternalPtr@VButtonElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180111510  ?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180047f20  ?CreateNewObject@ButtonDescription@UI@@SAPEAVBase@Object@@XZ

    0x180047fe0  ?Destruct@ButtonDescription@UI@@UEAAXXZ

    0x1801113d0  ?GetFileVersion@ButtonDescription@UI@@UEBAHXZ

    0x180047fb0  ?Reconstruct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1801113e0  ?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180047e10  ?Type@ButtonDescription@UI@@UEBAIXZ

    0x180047e20  ?TypeId@ButtonDescription@UI@@SAIXZ

    0x180047e30  ?TypeName@ButtonDescription@UI@@UEBAPEBDXZ

    0x180047e00  ?_ParentTypeName@ButtonDescription@UI@@CAPEBDXZ

    0x180047df0  ?_TypeName@ButtonDescription@UI@@CAPEBDXZ

    0x1801fa900  ?_typeInfo@ButtonDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ButtonElement   exports=24 vtable=0x180157310

    0x180048460  ??0ButtonElement@UI@@QEAA@AEBV01@@Z

    0x180110c40  ??0ButtonElement@UI@@QEAA@XZ

    0x180110c90  ??1ButtonElement@UI@@UEAA@XZ

    0x180048270  ??2ButtonElement@UI@@SAPEAX_K@Z

    0x1800482e0  ??2ButtonElement@UI@@SAPEAX_KPEAX@Z

    0x1800482f0  ??3ButtonElement@UI@@SAXPEAX@Z

    0x1800484f0  ??4ButtonElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157310  ??_7ButtonElement@UI@@6B@

    0x180110e70  ?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z

    0x180110d10  ?ComputeSize@ButtonElement@UI@@UEAAXXZ

    0x1800483f0  ?CreateInPlaceObject@ButtonElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180048350  ?CreateNewObject@ButtonElement@UI@@SAPEAVBase@Object@@XZ

    0x180111020  ?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180110cc0  ?Enable@ButtonElement@UI@@UEAAX_N@Z

    0x180048450  ?GetKeyId@ButtonElement@UI@@QEBAHXZ

    0x180048440  ?SetKeyId@ButtonElement@UI@@QEAAXH@Z

    0x180048240  ?Type@ButtonElement@UI@@UEBAIXZ

    0x180048250  ?TypeId@ButtonElement@UI@@SAIXZ

    0x180048260  ?TypeName@ButtonElement@UI@@UEBAPEBDXZ

    0x1801110b0  ?Update@ButtonElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x1801108a0  ?WasKeyPressed@ButtonElement@UI@@IEBA_NAEBUUpdateState@Element@2@@Z

    0x180048230  ?_ParentTypeName@ButtonElement@UI@@CAPEBDXZ

    0x180048220  ?_TypeName@ButtonElement@UI@@CAPEBDXZ

    0x1801fa938  ?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::CharacterSet   exports=21 vtable=0x180156220

    0x180037720  ??0CharacterSet@UI@@QEAA@AEBV01@@Z

    0x1800cb8a0  ??0CharacterSet@UI@@QEAA@XZ

    0x1800cb840  ??1CharacterSet@UI@@UEAA@XZ

    0x180037460  ??2CharacterSet@UI@@SAPEAX_K@Z

    0x1800374d0  ??2CharacterSet@UI@@SAPEAX_KPEAX@Z

    0x1800374e0  ??3CharacterSet@UI@@SAXPEAX@Z

    0x1800377b0  ??4CharacterSet@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156220  ??_7CharacterSet@UI@@6B@

    0x1800376b0  ?Construct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800375f0  ?CreateInPlaceObject@CharacterSet@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180037540  ?CreateNewObject@CharacterSet@UI@@SAPEAVBase@Object@@XZ

    0x180037710  ?Destruct@CharacterSet@UI@@UEAAXXZ

    0x1800cb830  ?GetFileVersion@CharacterSet@UI@@UEBAHXZ

    0x180037650  ?Reconstruct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cb7b0  ?Serialize@CharacterSet@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180037430  ?Type@CharacterSet@UI@@UEBAIXZ

    0x180037440  ?TypeId@CharacterSet@UI@@SAIXZ

    0x180037450  ?TypeName@CharacterSet@UI@@UEBAPEBDXZ

    0x180037420  ?_ParentTypeName@CharacterSet@UI@@CAPEBDXZ

    0x180037410  ?_TypeName@CharacterSet@UI@@CAPEBDXZ

    0x1801f9cf8  ?_typeInfo@CharacterSet@UI@@0VBaseTypeInfo@Object@@A



##### UI::CheckDescription   exports=22 vtable=0x1801572c0

    0x180048900  ??0CheckDescription@UI@@QEAA@AEBV01@@Z

    0x1801115e0  ??0CheckDescription@UI@@QEAA@XZ

    0x180111620  ??1CheckDescription@UI@@UEAA@XZ

    0x180048690  ??2CheckDescription@UI@@SAPEAX_K@Z

    0x180048700  ??2CheckDescription@UI@@SAPEAX_KPEAX@Z

    0x180048710  ??3CheckDescription@UI@@SAXPEAX@Z

    0x180048950  ??4CheckDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801572c0  ??_7CheckDescription@UI@@6B@

    0x1800488a0  ?Construct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180048800  ?CreateInPlaceObject@CheckDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1801119c0  ?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180048770  ?CreateNewObject@CheckDescription@UI@@SAPEAVBase@Object@@XZ

    0x1800488f0  ?Destruct@CheckDescription@UI@@UEAAXXZ

    0x180111920  ?GetFileVersion@CheckDescription@UI@@UEBAHXZ

    0x180048850  ?Reconstruct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180111930  ?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180048660  ?Type@CheckDescription@UI@@UEBAIXZ

    0x180048670  ?TypeId@CheckDescription@UI@@SAIXZ

    0x180048680  ?TypeName@CheckDescription@UI@@UEBAPEBDXZ

    0x180048650  ?_ParentTypeName@CheckDescription@UI@@CAPEBDXZ

    0x180048640  ?_TypeName@CheckDescription@UI@@CAPEBDXZ

    0x1801fa970  ?_typeInfo@CheckDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::CheckElement   exports=22 vtable=0x180157260

    0x180048ca0  ??0CheckElement@UI@@QEAA@AEBV01@@Z

    0x1801116a0  ??0CheckElement@UI@@QEAA@XZ

    0x180111700  ??1CheckElement@UI@@UEAA@XZ

    0x180048a90  ??2CheckElement@UI@@SAPEAX_K@Z

    0x180048b00  ??2CheckElement@UI@@SAPEAX_KPEAX@Z

    0x180048b10  ??3CheckElement@UI@@SAXPEAX@Z

    0x180048cf0  ??4CheckElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157260  ??_7CheckElement@UI@@6B@

    0x1801117b0  ?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z

    0x180111740  ?ComputeSize@CheckElement@UI@@UEAAXXZ

    0x180048c20  ?CreateInPlaceObject@CheckElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180048b70  ?CreateNewObject@CheckElement@UI@@SAPEAVBase@Object@@XZ

    0x180111800  ?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180048c90  ?EnableCheck@CheckElement@UI@@QEAAX_N@Z

    0x180048c80  ?IsChecked@CheckElement@UI@@QEBA_NXZ

    0x180048a60  ?Type@CheckElement@UI@@UEBAIXZ

    0x180048a70  ?TypeId@CheckElement@UI@@SAIXZ

    0x180048a80  ?TypeName@CheckElement@UI@@UEBAPEBDXZ

    0x180111870  ?Update@CheckElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180048a50  ?_ParentTypeName@CheckElement@UI@@CAPEBDXZ

    0x180048a40  ?_TypeName@CheckElement@UI@@CAPEBDXZ

    0x1801fa9a8  ?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ClipRegionDescription   exports=22 vtable=0x180156350

    0x180037da0  ??0ClipRegionDescription@UI@@QEAA@AEBV01@@Z

    0x1800cc140  ??0ClipRegionDescription@UI@@QEAA@XZ

    0x1800cc0d0  ??1ClipRegionDescription@UI@@UEAA@XZ

    0x1800379a0  ??2ClipRegionDescription@UI@@SAPEAX_K@Z

    0x180037a10  ??2ClipRegionDescription@UI@@SAPEAX_KPEAX@Z

    0x180037a20  ??3ClipRegionDescription@UI@@SAXPEAX@Z

    0x180037df0  ??4ClipRegionDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156350  ??_7ClipRegionDescription@UI@@6B@

    0x180037ce0  ?Construct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180037b80  ?CreateInPlaceObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800cbec0  ?CreateInstance@ClipRegionDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180037a80  ?CreateNewObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@XZ

    0x180037d90  ?Destruct@ClipRegionDescription@UI@@UEAAXXZ

    0x1800cc0c0  ?GetFileVersion@ClipRegionDescription@UI@@UEBAHXZ

    0x180037c30  ?Reconstruct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cc010  ?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180037970  ?Type@ClipRegionDescription@UI@@UEBAIXZ

    0x180037980  ?TypeId@ClipRegionDescription@UI@@SAIXZ

    0x180037990  ?TypeName@ClipRegionDescription@UI@@UEBAPEBDXZ

    0x180037960  ?_ParentTypeName@ClipRegionDescription@UI@@CAPEBDXZ

    0x180037950  ?_TypeName@ClipRegionDescription@UI@@CAPEBDXZ

    0x1801f9d30  ?_typeInfo@ClipRegionDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ClipRegionElement   exports=26 vtable=0x1801562f0

    0x180038220  ??0ClipRegionElement@UI@@QEAA@AEBV01@@Z

    0x1800cbe60  ??0ClipRegionElement@UI@@QEAA@XZ

    0x1800cbe30  ??1ClipRegionElement@UI@@UEAA@XZ

    0x180037f80  ??2ClipRegionElement@UI@@SAPEAX_K@Z

    0x180037ff0  ??2ClipRegionElement@UI@@SAPEAX_KPEAX@Z

    0x180038000  ??3ClipRegionElement@UI@@SAXPEAX@Z

    0x1800382b0  ??4ClipRegionElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801562f0  ??_7ClipRegionElement@UI@@6B@

    0x1800cba00  ?ComputeLayout@ClipRegionElement@UI@@UEAAXHHHH@Z

    0x1800cbc60  ?ComputeSize@ClipRegionElement@UI@@UEAAXXZ

    0x1800cbcc0  ?Contains@ClipRegionElement@UI@@UEBA_NHHPEAVUserInterface@2@@Z

    0x180038110  ?CreateInPlaceObject@ClipRegionElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180038060  ?CreateNewObject@ClipRegionElement@UI@@SAPEAVBase@Object@@XZ

    0x1800cb900  ?Draw@ClipRegionElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800381e0  ?GetContentSize@ClipRegionElement@UI@@QEAAXAEAH0@Z

    0x180038180  ?GetHeight@ClipRegionElement@UI@@QEBAHXZ

    0x180038170  ?GetWidth@ClipRegionElement@UI@@QEBAHXZ

    0x1800381d0  ?SetOffset@ClipRegionElement@UI@@QEAAXHH@Z

    0x180038190  ?SetSize@ClipRegionElement@UI@@QEAAXHH@Z

    0x180037f50  ?Type@ClipRegionElement@UI@@UEBAIXZ

    0x180037f60  ?TypeId@ClipRegionElement@UI@@SAIXZ

    0x180037f70  ?TypeName@ClipRegionElement@UI@@UEBAPEBDXZ

    0x1800cbd70  ?Update@ClipRegionElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180037f40  ?_ParentTypeName@ClipRegionElement@UI@@CAPEBDXZ

    0x180037f30  ?_TypeName@ClipRegionElement@UI@@CAPEBDXZ

    0x1801f9d68  ?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ComboDescription   exports=22 vtable=0x180159090

    0x180074d80  ??0ComboDescription@UI@@QEAA@AEBV01@@Z

    0x180112320  ??0ComboDescription@UI@@QEAA@XZ

    0x180112370  ??1ComboDescription@UI@@UEAA@XZ

    0x180074af0  ??2ComboDescription@UI@@SAPEAX_K@Z

    0x180074b60  ??2ComboDescription@UI@@SAPEAX_KPEAX@Z

    0x180074b70  ??3ComboDescription@UI@@SAXPEAX@Z

    0x180074df0  ??4ComboDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180159090  ??_7ComboDescription@UI@@6B@

    0x180074d20  ?Construct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180074c70  ?CreateInPlaceObject@ComboDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180112a70  ?CreateInstance@ComboDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180074bd0  ?CreateNewObject@ComboDescription@UI@@SAPEAVBase@Object@@XZ

    0x180074d70  ?Destruct@ComboDescription@UI@@UEAAXXZ

    0x1801129b0  ?GetFileVersion@ComboDescription@UI@@UEBAHXZ

    0x180074cc0  ?Reconstruct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1801129c0  ?Serialize@ComboDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180074ac0  ?Type@ComboDescription@UI@@UEBAIXZ

    0x180074ad0  ?TypeId@ComboDescription@UI@@SAIXZ

    0x180074ae0  ?TypeName@ComboDescription@UI@@UEBAPEBDXZ

    0x180074ab0  ?_ParentTypeName@ComboDescription@UI@@CAPEBDXZ

    0x180074aa0  ?_TypeName@ComboDescription@UI@@CAPEBDXZ

    0x1801faa18  ?_typeInfo@ComboDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ComboDialog   exports=19 vtable=0x1801590e0

    0x180074940  ??0ComboDialog@UI@@QEAA@AEBV01@@Z

    0x180111d00  ??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@V?$InternalPtr@VComboElement@UI@@@4@@Z

    0x180112130  ??0ComboDialog@UI@@QEAA@XZ

    0x180111cd0  ??1ComboDialog@UI@@UEAA@XZ

    0x180074770  ??2ComboDialog@UI@@SAPEAX_K@Z

    0x1800747e0  ??2ComboDialog@UI@@SAPEAX_KPEAX@Z

    0x1800747f0  ??3ComboDialog@UI@@SAXPEAX@Z

    0x1801590e0  ??_7ComboDialog@UI@@6B@

    0x1800748f0  ?CreateInPlaceObject@ComboDialog@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180074850  ?CreateNewObject@ComboDialog@UI@@SAPEAVBase@Object@@XZ

    0x180111af0  ?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

    0x180111bd0  ?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z

    0x180111cb0  ?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ

    0x180074740  ?Type@ComboDialog@UI@@UEBAIXZ

    0x180074750  ?TypeId@ComboDialog@UI@@SAIXZ

    0x180074760  ?TypeName@ComboDialog@UI@@UEBAPEBDXZ

    0x180074730  ?_ParentTypeName@ComboDialog@UI@@CAPEBDXZ

    0x180074720  ?_TypeName@ComboDialog@UI@@CAPEBDXZ

    0x1801fa9e0  ?_typeInfo@ComboDialog@UI@@0VBaseTypeInfo@Object@@A



##### UI::ComboElement   exports=26 vtable=0x180159030

    0x1800751e0  ??0ComboElement@UI@@QEAA@AEBV01@@Z

    0x1801123f0  ??0ComboElement@UI@@QEAA@XZ

    0x180112490  ??1ComboElement@UI@@UEAA@XZ

    0x180074f40  ??2ComboElement@UI@@SAPEAX_K@Z

    0x180074fb0  ??2ComboElement@UI@@SAPEAX_KPEAX@Z

    0x180074fc0  ??3ComboElement@UI@@SAXPEAX@Z

    0x1800752c0  ??4ComboElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180159030  ??_7ComboElement@UI@@6B@

    0x1800750b0  ?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z

    0x1800750a0  ?CreateInPlaceObject@ComboElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180075020  ?CreateNewObject@ComboElement@UI@@SAPEAVBase@Object@@XZ

    0x180112520  ?DropDownClosed@ComboElement@UI@@QEAAXXZ

    0x180075160  ?GetCurrentItem@ComboElement@UI@@QEBAHXZ

    0x180075180  ?GetItemCount@ComboElement@UI@@QEBAHXZ

    0x180075170  ?GetItemData@ComboElement@UI@@QEBAPEAXH@Z

    0x180075190  ?GetItemText@ComboElement@UI@@QEBA?AVString@System@@H@Z

    0x180075120  ?RemoveAllItems@ComboElement@UI@@QEAAXXZ

    0x180112540  ?SetCurrentItem@ComboElement@UI@@QEAAXH@Z

    0x180074f10  ?Type@ComboElement@UI@@UEBAIXZ

    0x180074f20  ?TypeId@ComboElement@UI@@SAIXZ

    0x180074f30  ?TypeName@ComboElement@UI@@UEBAPEBDXZ

    0x1801125d0  ?Update@ComboElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180112180  ?UpdateText@ComboElement@UI@@AEAAXXZ

    0x180074f00  ?_ParentTypeName@ComboElement@UI@@CAPEBDXZ

    0x180074ef0  ?_TypeName@ComboElement@UI@@CAPEBDXZ

    0x1801faa50  ?_typeInfo@ComboElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ContainerDescription   exports=22 vtable=0x180156510

    0x180039a90  ??0ContainerDescription@UI@@QEAA@AEBV01@@Z

    0x1800cd060  ??0ContainerDescription@UI@@QEAA@XZ

    0x1800ccff0  ??1ContainerDescription@UI@@UEAA@XZ

    0x1800398e0  ??2ContainerDescription@UI@@SAPEAX_K@Z

    0x180039950  ??2ContainerDescription@UI@@SAPEAX_KPEAX@Z

    0x180039960  ??3ContainerDescription@UI@@SAXPEAX@Z

    0x180039b10  ??4ContainerDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156510  ??_7ContainerDescription@UI@@6B@

    0x180039a70  ?Construct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180039a40  ?CreateInPlaceObject@ContainerDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800ccb90  ?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x1800399c0  ?CreateNewObject@ContainerDescription@UI@@SAPEAVBase@Object@@XZ

    0x180039a80  ?Destruct@ContainerDescription@UI@@UEAAXXZ

    0x1800ccfe0  ?GetFileVersion@ContainerDescription@UI@@UEBAHXZ

    0x180039a50  ?Reconstruct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cced0  ?Serialize@ContainerDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800398b0  ?Type@ContainerDescription@UI@@UEBAIXZ

    0x1800398c0  ?TypeId@ContainerDescription@UI@@SAIXZ

    0x1800398d0  ?TypeName@ContainerDescription@UI@@UEBAPEBDXZ

    0x1800398a0  ?_ParentTypeName@ContainerDescription@UI@@CAPEBDXZ

    0x180039890  ?_TypeName@ContainerDescription@UI@@CAPEBDXZ

    0x1801f9da0  ?_typeInfo@ContainerDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ContainerElement   exports=24 vtable=0x1801564b0

    0x180039f00  ??0ContainerElement@UI@@QEAA@AEBV01@@Z

    0x1800ccb40  ??0ContainerElement@UI@@QEAA@XZ

    0x1800ccb10  ??1ContainerElement@UI@@UEAA@XZ

    0x180039cd0  ??2ContainerElement@UI@@SAPEAX_K@Z

    0x180039d40  ??2ContainerElement@UI@@SAPEAX_KPEAX@Z

    0x180039d50  ??3ContainerElement@UI@@SAXPEAX@Z

    0x180039f90  ??4ContainerElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801564b0  ??_7ContainerElement@UI@@6B@

    0x1800cc620  ?ComputeLayout@ContainerElement@UI@@UEAAXHHHH@Z

    0x1800cc850  ?ComputeSize@ContainerElement@UI@@UEAAXXZ

    0x180039e50  ?CreateInPlaceObject@ContainerElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180039db0  ?CreateNewObject@ContainerElement@UI@@SAPEAVBase@Object@@XZ

    0x1800cc590  ?Draw@ContainerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180039ef0  ?GetHeight@ContainerElement@UI@@QEBAHXZ

    0x180039ee0  ?GetWidth@ContainerElement@UI@@QEBAHXZ

    0x1800cc1e0  ?SetOffsets@ContainerElement@UI@@QEAAXHH@Z

    0x180039eb0  ?SetSize@ContainerElement@UI@@QEAAXHH@Z

    0x180039ca0  ?Type@ContainerElement@UI@@UEBAIXZ

    0x180039cb0  ?TypeId@ContainerElement@UI@@SAIXZ

    0x180039cc0  ?TypeName@ContainerElement@UI@@UEBAPEBDXZ

    0x1800cc2d0  ?Update@ContainerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180039c90  ?_ParentTypeName@ContainerElement@UI@@CAPEBDXZ

    0x180039c80  ?_TypeName@ContainerElement@UI@@CAPEBDXZ

    0x1801f9dd8  ?_typeInfo@ContainerElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::Cursor   exports=24 vtable=0x180156570

    0x18003b280  ??0Cursor@UI@@QEAA@AEBV01@@Z

    0x1800cd1f0  ??0Cursor@UI@@QEAA@XZ

    0x1800cd1c0  ??1Cursor@UI@@UEAA@XZ

    0x18003af90  ??2Cursor@UI@@SAPEAX_K@Z

    0x18003b000  ??2Cursor@UI@@SAPEAX_KPEAX@Z

    0x18003b010  ??3Cursor@UI@@SAXPEAX@Z

    0x18003b310  ??4Cursor@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156570  ??_7Cursor@UI@@6B@

    0x18003b1e0  ?Construct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ

    0x18003b120  ?CreateInPlaceObject@Cursor@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003b070  ?CreateNewObject@Cursor@UI@@SAPEAVBase@Object@@XZ

    0x18003b240  ?Destruct@Cursor@UI@@UEAAXXZ

    0x1800cd1b0  ?GetFileVersion@Cursor@UI@@UEBAHXZ

    0x18003b260  ?GetHotSpotX@Cursor@UI@@QEBAHXZ

    0x18003b270  ?GetHotSpotY@Cursor@UI@@QEBAHXZ

    0x18003b250  ?GetSpriteSheet@Cursor@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ

    0x18003b180  ?Reconstruct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cd110  ?Serialize@Cursor@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18003af60  ?Type@Cursor@UI@@UEBAIXZ

    0x18003af70  ?TypeId@Cursor@UI@@SAIXZ

    0x18003af80  ?TypeName@Cursor@UI@@UEBAPEBDXZ

    0x18003af50  ?_ParentTypeName@Cursor@UI@@CAPEBDXZ

    0x18003af40  ?_TypeName@Cursor@UI@@CAPEBDXZ

    0x1801f9e10  ?_typeInfo@Cursor@UI@@0VBaseTypeInfo@Object@@A



##### UI::Dialog   exports=36 vtable=0x1801562a0

    0x180038920  ??0Dialog@UI@@QEAA@AEBV01@@Z

    0x180112db0  ??0Dialog@UI@@QEAA@XZ

    0x180112e40  ??1Dialog@UI@@UEAA@XZ

    0x180038450  ??2Dialog@UI@@SAPEAX_K@Z

    0x1800384c0  ??2Dialog@UI@@SAPEAX_KPEAX@Z

    0x1800384d0  ??3Dialog@UI@@SAXPEAX@Z

    0x180038a30  ??4Dialog@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801562a0  ??_7Dialog@UI@@6B@

    0x180038750  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ

    0x180112ed0  ?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@@Z

    0x180112c80  ?CreateElements@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ

    0x180038610  ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180038530  ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ

    0x1800387f0  ?Destruct@Dialog@UI@@UEAAXXZ

    0x1800388a0  ?GetBackgroundColor@Dialog@UI@@QEBAIXZ

    0x180038860  ?GetBackgroundSprite@Dialog@UI@@QEBA?AVSprite@2@XZ

    0x180038900  ?GetController@Dialog@UI@@QEBAAEBUElementController@12@H@Z

    0x1800388f0  ?GetControllerCount@Dialog@UI@@QEBAHXZ

    0x180038910  ?GetDrawLevel@Dialog@UI@@QEBAHXZ

    0x1801130b0  ?GetFileVersion@Dialog@UI@@UEBAHXZ

    0x180038850  ?GetFlags@Dialog@UI@@QEBAIXZ

    0x1800388d0  ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z

    0x1800388b0  ?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z

    0x180038840  ?IsFirstDraw@Dialog@UI@@QEBA_NXZ

    0x180038800  ?IsModal@Dialog@UI@@QEBA_NXZ

    0x180038820  ?NoMouseCapture@Dialog@UI@@QEBA_NXZ

    0x180038810  ?PausesGame@Dialog@UI@@QEBA_NXZ

    0x180038830  ?PerElementCapture@Dialog@UI@@QEBA_NXZ

    0x1800386b0  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ

    0x1801130c0  ?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180038420  ?Type@Dialog@UI@@UEBAIXZ

    0x180038430  ?TypeId@Dialog@UI@@SAIXZ

    0x180038440  ?TypeName@Dialog@UI@@UEBAPEBDXZ

    0x180038410  ?_ParentTypeName@Dialog@UI@@CAPEBDXZ

    0x180038400  ?_TypeName@Dialog@UI@@CAPEBDXZ

    0x1801faa88  ?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A



##### UI::DialogController   exports=22 vtable=0x1801565f0

    0x18003b710  ??0DialogController@UI@@QEAA@AEBV01@@Z

    0x1800cd320  ??0DialogController@UI@@QEAA@XZ

    0x1800cd2f0  ??1DialogController@UI@@UEAA@XZ

    0x18003b780  ??4DialogController@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801565f0  ??_7DialogController@UI@@6B@

    0x18003b6f0  ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003b6e0  ?CreateNewObject@DialogController@UI@@SAPEAVBase@Object@@XZ

    0x18003b700  ?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ

    0x1800cd290  ?OnClosed@DialogController@UI@@UEAAXXZ

    0x1800cd2b0  ?OnCreated@DialogController@UI@@UEAAXXZ

    0x1800cd260  ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

    0x1800cd2a0  ?OnOpened@DialogController@UI@@UEAAXXZ

    0x1800cd250  ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z

    0x1800cd270  ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z

    0x1800cd280  ?OnUpdateLayout@DialogController@UI@@UEAAXXZ

    0x1800cd2c0  ?Setup@DialogController@UI@@QEAAXPEAVBase@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$InternalPtr@VElement@UI@@@4@@Z

    0x18003b6b0  ?Type@DialogController@UI@@UEBAIXZ

    0x18003b6c0  ?TypeId@DialogController@UI@@SAIXZ

    0x18003b6d0  ?TypeName@DialogController@UI@@UEBAPEBDXZ

    0x18003b6a0  ?_ParentTypeName@DialogController@UI@@CAPEBDXZ

    0x18003b690  ?_TypeName@DialogController@UI@@CAPEBDXZ

    0x1801f9e48  ?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A



##### UI::DialogControllerConfig   exports=13 vtable=0x180156650

    0x18003b520  ??0DialogControllerConfig@UI@@QEAA@AEBV01@@Z

    0x18003b4a0  ??0DialogControllerConfig@UI@@QEAA@XZ

    0x18003b4f0  ??1DialogControllerConfig@UI@@UEAA@XZ

    0x18003b590  ??4DialogControllerConfig@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156650  ??_7DialogControllerConfig@UI@@6B@

    0x18003b490  ?CreateInPlaceObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003b480  ?CreateNewObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@XZ

    0x18003b450  ?Type@DialogControllerConfig@UI@@UEBAIXZ

    0x18003b460  ?TypeId@DialogControllerConfig@UI@@SAIXZ

    0x18003b470  ?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ

    0x18003b440  ?_ParentTypeName@DialogControllerConfig@UI@@CAPEBDXZ

    0x18003b430  ?_TypeName@DialogControllerConfig@UI@@CAPEBDXZ

    0x1801f9e80  ?_typeInfo@DialogControllerConfig@UI@@0VBaseTypeInfo@Object@@A



##### UI::DialogInstance   exports=64 vtable=0x180156d70

    0x180045630  ??0DialogInstance@UI@@QEAA@AEBV01@@Z

    0x180114870  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z

    0x1801146b0  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@PEAVBase@4@PEAXH@Z

    0x180114ca0  ??0DialogInstance@UI@@QEAA@XZ

    0x180114d40  ??1DialogInstance@UI@@UEAA@XZ

    0x180045410  ??2DialogInstance@UI@@SAPEAX_K@Z

    0x180045480  ??2DialogInstance@UI@@SAPEAX_KPEAX@Z

    0x180045490  ??3DialogInstance@UI@@SAXPEAX@Z

    0x180045790  ??4DialogInstance@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156d70  ??_7DialogInstance@UI@@6B@

    0x180113470  ?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z

    0x1801133b0  ?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$InternalPtr@VElement@UI@@@4@@Z

    0x180114180  ?CloseChildren@DialogInstance@UI@@QEAAXXZ

    0x180113d90  ?ComputeLayout@DialogInstance@UI@@QEAAXHHHH@Z

    0x180114820  ?Contains@DialogInstance@UI@@QEBA_NHH@Z

    0x1800454f0  ?CreateNewObject@DialogInstance@UI@@SAPEAVBase@Object@@XZ

    0x180114110  ?DeleteOnClose@DialogInstance@UI@@QEBA_NXZ

    0x1801141c0  ?Detach@DialogInstance@UI@@QEAAXXZ

    0x180113840  ?Draw@DialogInstance@UI@@QEBAXAEAVSpriteContext@2@@Z

    0x180113880  ?DrawBackground@DialogInstance@UI@@QEBAXAEAVSpriteContext@2@HHHH@Z

    0x180113d10  ?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z

    0x180114c70  ?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z

    0x1801140c0  ?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z

    0x180113320  ?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z

    0x180045620  ?GetDialog@DialogInstance@UI@@QEBAPEBVDialog@2@XZ

    0x1801140f0  ?GetDrawLevel@DialogInstance@UI@@QEBAHXZ

    0x1800455f0  ?GetLeftCorner@DialogInstance@UI@@QEBAHXZ

    0x180113fe0  ?GetMouseCursorIndex@DialogInstance@UI@@QEBAHHH@Z

    0x180114010  ?GetMouseCursorIndex@DialogInstance@UI@@QEBAHV?$InternalPtr@VElement@UI@@@Object@@HH@Z

    0x180045570  ?GetParent@DialogInstance@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@XZ

    0x1800455c0  ?GetResult@DialogInstance@UI@@QEBAHXZ

    0x180045580  ?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ

    0x180045590  ?GetRoot@DialogInstance@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ

    0x180045600  ?GetTopCorner@DialogInstance@UI@@QEBAHXZ

    0x1800455e0  ?GetUserInterface@DialogInstance@UI@@QEAAPEAVUserInterface@2@XZ

    0x180045610  ?GetUserObject@DialogInstance@UI@@QEBAPEAXXZ

    0x180114130  ?IsFirstDraw@DialogInstance@UI@@QEBA_NXZ

    0x1800455a0  ?IsHovering@DialogInstance@UI@@QEBA_NXZ

    0x180114170  ?IsModal@DialogInstance@UI@@QEBA_NXZ

    0x180114450  ?LoadSettings@DialogInstance@UI@@QEAAXXZ

    0x180114150  ?NoMouseCapture@DialogInstance@UI@@QEBA_NXZ

    0x180115330  ?OnClosed@DialogInstance@UI@@UEAAXXZ

    0x180114ed0  ?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z

    0x180114fb0  ?OnOpened@DialogInstance@UI@@UEAAXXZ

    0x180115470  ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z

    0x180115400  ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z

    0x1801153a0  ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ

    0x180114160  ?PausesGame@DialogInstance@UI@@QEBA_NXZ

    0x180114140  ?PerElementCapture@DialogInstance@UI@@QEBA_NXZ

    0x180114fa0  ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z

    0x180114240  ?SaveSettings@DialogInstance@UI@@QEAAXXZ

    0x1800455d0  ?SetDialogResult@DialogInstance@UI@@QEAAXH@Z

    0x180114690  ?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z

    0x180114a60  ?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z

    0x1800455b0  ?SetUserInterface@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z

    0x1801137c0  ?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z

    0x180113730  ?StopTimer@DialogInstance@UI@@QEAAXH@Z

    0x1800453e0  ?Type@DialogInstance@UI@@UEBAIXZ

    0x1800453f0  ?TypeId@DialogInstance@UI@@SAIXZ

    0x180045400  ?TypeName@DialogInstance@UI@@UEBAPEBDXZ

    0x180113980  ?Update@DialogInstance@UI@@QEAAXAEBVTime@System@@PEAVUserInterface@2@AEAUUpdateState@Element@2@@Z

    0x1800453d0  ?_ParentTypeName@DialogInstance@UI@@CAPEBDXZ

    0x1800453c0  ?_TypeName@DialogInstance@UI@@CAPEBDXZ

    0x1801faac0  ?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A



##### UI::DocumentTextDescription   exports=23 vtable=0x180156750

    0x18003bac0  ??0DocumentTextDescription@UI@@QEAA@AEBV01@@Z

    0x1800cfc30  ??0DocumentTextDescription@UI@@QEAA@XZ

    0x1800cfb30  ??1DocumentTextDescription@UI@@UEAA@XZ

    0x18003b910  ??2DocumentTextDescription@UI@@SAPEAX_K@Z

    0x18003b980  ??2DocumentTextDescription@UI@@SAPEAX_KPEAX@Z

    0x18003b990  ??3DocumentTextDescription@UI@@SAXPEAX@Z

    0x18003bc00  ??4DocumentTextDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156750  ??_7DocumentTextDescription@UI@@6B@

    0x18003baa0  ?Construct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18003ba70  ?CreateInPlaceObject@DocumentTextDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800cf520  ?CreateInstance@DocumentTextDescription@UI@@QEBAXV?$InternalPtr@VDocumentTextElement@UI@@@Object@@PEBVDialog@2@@Z

    0x1800cf7e0  ?CreateInstance@DocumentTextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18003b9f0  ?CreateNewObject@DocumentTextDescription@UI@@SAPEAVBase@Object@@XZ

    0x18003bab0  ?Destruct@DocumentTextDescription@UI@@UEAAXXZ

    0x1800cfb20  ?GetFileVersion@DocumentTextDescription@UI@@UEBAHXZ

    0x18003ba80  ?Reconstruct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800cf8a0  ?Serialize@DocumentTextDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18003b8e0  ?Type@DocumentTextDescription@UI@@UEBAIXZ

    0x18003b8f0  ?TypeId@DocumentTextDescription@UI@@SAIXZ

    0x18003b900  ?TypeName@DocumentTextDescription@UI@@UEBAPEBDXZ

    0x18003b8d0  ?_ParentTypeName@DocumentTextDescription@UI@@CAPEBDXZ

    0x18003b8c0  ?_TypeName@DocumentTextDescription@UI@@CAPEBDXZ

    0x1801f9eb8  ?_typeInfo@DocumentTextDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::DocumentTextElement   exports=31 vtable=0x1801566f0

    0x18003c140  ??0DocumentTextElement@UI@@QEAA@AEBV01@@Z

    0x1800cf410  ??0DocumentTextElement@UI@@QEAA@XZ

    0x1800cf320  ??1DocumentTextElement@UI@@UEAA@XZ

    0x18003be60  ??2DocumentTextElement@UI@@SAPEAX_K@Z

    0x18003bed0  ??2DocumentTextElement@UI@@SAPEAX_KPEAX@Z

    0x18003bee0  ??3DocumentTextElement@UI@@SAXPEAX@Z

    0x18003c3d0  ??4DocumentTextElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801566f0  ??_7DocumentTextElement@UI@@6B@

    0x1800cdeb0  ?BeginLink@DocumentTextElement@UI@@AEAAXAEBVString@System@@@Z

    0x1800cda00  ?ComputeLayout@DocumentTextElement@UI@@UEAAXHHHH@Z

    0x1800cdb60  ?ComputeSize@DocumentTextElement@UI@@UEAAXXZ

    0x18003bfc0  ?CreateInPlaceObject@DocumentTextElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003bf40  ?CreateNewObject@DocumentTextElement@UI@@SAPEAVBase@Object@@XZ

    0x1800cd7a0  ?Draw@DocumentTextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800cde00  ?EndLink@DocumentTextElement@UI@@AEAAXXZ

    0x1800cddb0  ?EndRow@DocumentTextElement@UI@@AEAAXXZ

    0x1800cefd0  ?FindNextWhiteSpace@DocumentTextElement@UI@@AEAAHXZ

    0x1800cf060  ?GenerateItems@DocumentTextElement@UI@@AEAAXXZ

    0x18003c0c0  ?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z

    0x1800cdf40  ?GetNextItem@DocumentTextElement@UI@@AEAA_NAEAUDocumentItem@12@@Z

    0x1800cf030  ?IsWhiteSpace@DocumentTextElement@UI@@AEBA_ND@Z

    0x1800cdbc0  ?LayoutRow@DocumentTextElement@UI@@AEAAHXZ

    0x18003c010  ?SetText@DocumentTextElement@UI@@QEAAXAEBVString@System@@@Z

    0x18003bfd0  ?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z

    0x18003be30  ?Type@DocumentTextElement@UI@@UEBAIXZ

    0x18003be40  ?TypeId@DocumentTextElement@UI@@SAIXZ

    0x18003be50  ?TypeName@DocumentTextElement@UI@@UEBAPEBDXZ

    0x1800cd360  ?Update@DocumentTextElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x18003be20  ?_ParentTypeName@DocumentTextElement@UI@@CAPEBDXZ

    0x18003be10  ?_TypeName@DocumentTextElement@UI@@CAPEBDXZ

    0x1801f9ef0  ?_typeInfo@DocumentTextElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::EditDescription   exports=22 vtable=0x180156880

    0x18003f890  ??0EditDescription@UI@@QEAA@AEBV01@@Z

    0x1800d12c0  ??0EditDescription@UI@@QEAA@XZ

    0x1800d11f0  ??1EditDescription@UI@@UEAA@XZ

    0x18003f6e0  ??2EditDescription@UI@@SAPEAX_K@Z

    0x18003f750  ??2EditDescription@UI@@SAPEAX_KPEAX@Z

    0x18003f760  ??3EditDescription@UI@@SAXPEAX@Z

    0x18003f920  ??4EditDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156880  ??_7EditDescription@UI@@6B@

    0x18003f870  ?Construct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18003f840  ?CreateInPlaceObject@EditDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d0f90  ?CreateInstance@EditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18003f7c0  ?CreateNewObject@EditDescription@UI@@SAPEAVBase@Object@@XZ

    0x18003f880  ?Destruct@EditDescription@UI@@UEAAXXZ

    0x1800d11e0  ?GetFileVersion@EditDescription@UI@@UEBAHXZ

    0x18003f850  ?Reconstruct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d1110  ?Serialize@EditDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18003f6b0  ?Type@EditDescription@UI@@UEBAIXZ

    0x18003f6c0  ?TypeId@EditDescription@UI@@SAIXZ

    0x18003f6d0  ?TypeName@EditDescription@UI@@UEBAPEBDXZ

    0x18003f6a0  ?_ParentTypeName@EditDescription@UI@@CAPEBDXZ

    0x18003f690  ?_TypeName@EditDescription@UI@@CAPEBDXZ

    0x1801f9f28  ?_typeInfo@EditDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::EditElement   exports=26 vtable=0x180156810

    0x18003fc90  ??0EditElement@UI@@QEAA@AEBV01@@Z

    0x1800d0f10  ??0EditElement@UI@@QEAA@XZ

    0x1800d0e50  ??1EditElement@UI@@UEAA@XZ

    0x18003faf0  ??2EditElement@UI@@SAPEAX_K@Z

    0x18003fb60  ??2EditElement@UI@@SAPEAX_KPEAX@Z

    0x18003fb70  ??3EditElement@UI@@SAXPEAX@Z

    0x18003fd70  ??4EditElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156810  ??_7EditElement@UI@@6B@

    0x1800d0ab0  ?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z

    0x1800d0c10  ?ComputeSize@EditElement@UI@@UEAAXXZ

    0x18003fc50  ?CreateInPlaceObject@EditElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003fbd0  ?CreateNewObject@EditElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d0820  ?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800d0da0  ?FinalizeText@EditElement@UI@@UEAAXXZ

    0x18003fc70  ?GetMaxLength@EditElement@UI@@QEBAHXZ

    0x18003fc60  ?GetText@EditElement@UI@@QEBAAEBVString@System@@XZ

    0x18003fc80  ?IsActive@EditElement@UI@@QEBA_NXZ

    0x1800cfd40  ?SelectCharacter@EditElement@UI@@AEBADH@Z

    0x1800d0db0  ?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z

    0x18003fac0  ?Type@EditElement@UI@@UEBAIXZ

    0x18003fad0  ?TypeId@EditElement@UI@@SAIXZ

    0x18003fae0  ?TypeName@EditElement@UI@@UEBAPEBDXZ

    0x1800d0220  ?Update@EditElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x18003fab0  ?_ParentTypeName@EditElement@UI@@CAPEBDXZ

    0x18003faa0  ?_TypeName@EditElement@UI@@CAPEBDXZ

    0x1801f9f60  ?_typeInfo@EditElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::Element   exports=71 vtable=0x180156160

    0x1800358c0  ??0Element@UI@@QEAA@AEBV01@@Z

    0x180115e50  ??0Element@UI@@QEAA@XZ

    0x1801161a0  ??1Element@UI@@UEAA@XZ

    0x180035a50  ??4Element@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156160  ??_7Element@UI@@6B@

    0x180115df0  ?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x180115840  ?AdjustUpperLeft@Element@UI@@QEAAXHH@Z

    0x180035540  ?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z

    0x180035560  ?Child@Element@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z

    0x180035530  ?ChildCount@Element@UI@@QEBAHXZ

    0x180035490  ?ComputeColor@Element@UI@@QEBAIIAEAE@Z

    0x1801168d0  ?ComputeLayout@Element@UI@@UEAAXHHHH@Z

    0x1801169e0  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z

    0x180035450  ?CreateNewObject@Element@UI@@SAPEAVBase@Object@@XZ

    0x1801168f0  ?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180116910  ?Enable@Element@UI@@UEAAX_N@Z

    0x180115650  ?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z

    0x180115760  ?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z

    0x180115ff0  ?GetAlignedPosition@Element@UI@@QEAAXHHHHHHAEAH000_N1@Z

    0x1801158e0  ?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z

    0x180035630  ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ

    0x180035610  ?GetBottomPad@Element@UI@@QEBAHXZ

    0x1800357d0  ?GetDescription@Element@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@XZ

    0x1800354d0  ?GetElementFlags@Element@UI@@QEBAIXZ

    0x180115830  ?GetElementId@Element@UI@@QEBAHXZ

    0x1800355b0  ?GetLayoutBottom@Element@UI@@QEBAHXZ

    0x1800355d0  ?GetLayoutHeight@Element@UI@@QEBAHXZ

    0x180035580  ?GetLayoutLeft@Element@UI@@QEBAHXZ

    0x180035590  ?GetLayoutRight@Element@UI@@QEBAHXZ

    0x1800355a0  ?GetLayoutTop@Element@UI@@QEBAHXZ

    0x1800355c0  ?GetLayoutWidth@Element@UI@@QEBAHXZ

    0x1800355e0  ?GetLeftPad@Element@UI@@QEBAHXZ

    0x180115740  ?GetMouseCursorIndex@Element@UI@@QEBAHXZ

    0x180115f10  ?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z

    0x180035600  ?GetRightPad@Element@UI@@QEBAHXZ

    0x180035680  ?GetToolTip@Element@UI@@QEBA?AVString@2@XZ

    0x180035670  ?GetToolTipDialog@Element@UI@@QEBA?AV?$ExternalPtr@VDialog@UI@@@Object@@XZ

    0x1800356a0  ?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ

    0x1800355f0  ?GetTopPad@Element@UI@@QEBAHXZ

    0x180115550  ?GetUserData@Element@UI@@QEAAPEAVBase@Object@@XZ

    0x180035830  ?Hide@Element@UI@@QEAAX_N@Z

    0x180035520  ?IsDisabled@Element@UI@@QEBA_NXZ

    0x180035500  ?IsEnabled@Element@UI@@QEBA_NXZ

    0x180035510  ?IsSystemDisabled@Element@UI@@QEBA_NXZ

    0x180035640  ?IsToolTipValid@Element@UI@@QEBA_NXZ

    0x1800354e0  ?IsVisible@Element@UI@@QEBA_NXZ

    0x180035460  ?NeedsSizeCalc@Element@UI@@QEBA_NXZ

    0x180116870  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z

    0x180115570  ?RemoveChildren@Element@UI@@QEAAXXZ

    0x180115750  ?SetAlignment@Element@UI@@QEAAXW4Alignment@ElementDescription@2@@Z

    0x180035620  ?SetColor@Element@UI@@UEAAXI@Z

    0x180115630  ?SetDialogParent@Element@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x180115820  ?SetElementId@Element@UI@@QEAAXH@Z

    0x180116160  ?SetFlagsOnOff@Element@UI@@QEAAXII@Z

    0x1801154a0  ?SetMinSize@Element@UI@@QEAAXHH@Z

    0x1801154f0  ?SetName@Element@UI@@QEAAXAEBVString@System@@@Z

    0x180035470  ?SetNeedsSizeCalc@Element@UI@@QEAAXXZ

    0x180115480  ?SetPadding@Element@UI@@QEAAXHHHH@Z

    0x1801154d0  ?SetParent@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x180035740  ?SetToolTip@Element@UI@@QEAAXVString@2@@Z

    0x1800356e0  ?SetToolTipString@Element@UI@@QEAAXAEBVString@System@@@Z

    0x180115560  ?SetUserData@Element@UI@@QEAAXPEAVBase@Object@@@Z

    0x1800357e0  ?Show@Element@UI@@QEAAX_N@Z

    0x180035420  ?Type@Element@UI@@UEBAIXZ

    0x180035430  ?TypeId@Element@UI@@SAIXZ

    0x180035440  ?TypeName@Element@UI@@UEBAPEBDXZ

    0x180116270  ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z

    0x1800354f0  ?WasActivated@Element@UI@@QEBA_NXZ

    0x180035410  ?_ParentTypeName@Element@UI@@CAPEBDXZ

    0x180035400  ?_TypeName@Element@UI@@CAPEBDXZ

    0x1801fab30  ?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A



##### UI::Element::UpdateState   exports=2 (no vtable export)

    0x180115600  ??0UpdateState@Element@UI@@QEAA@XZ

    0x180035880  ??4UpdateState@Element@UI@@QEAAAEAU012@AEBU012@@Z



##### UI::ElementDescription   exports=16 vtable=0x1801561c0

    0x180035120  ??0ElementDescription@UI@@QEAA@AEBV01@@Z

    0x180115a40  ??0ElementDescription@UI@@QEAA@XZ

    0x180116100  ??1ElementDescription@UI@@UEAA@XZ

    0x180035210  ??4ElementDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801561c0  ??_7ElementDescription@UI@@6B@

    0x180035110  ?CreateInPlaceObject@ElementDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180115ad0  ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x180035100  ?CreateNewObject@ElementDescription@UI@@SAPEAVBase@Object@@XZ

    0x180116af0  ?GetFileVersion@ElementDescription@UI@@UEBAHXZ

    0x180116b00  ?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800350d0  ?Type@ElementDescription@UI@@UEBAIXZ

    0x1800350e0  ?TypeId@ElementDescription@UI@@SAIXZ

    0x1800350f0  ?TypeName@ElementDescription@UI@@UEBAPEBDXZ

    0x1800350c0  ?_ParentTypeName@ElementDescription@UI@@CAPEBDXZ

    0x1800350b0  ?_TypeName@ElementDescription@UI@@CAPEBDXZ

    0x1801faaf8  ?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::Font   exports=31 vtable=0x180155b50

    0x180031e10  ??0Font@UI@@QEAA@AEBV01@@Z

    0x180117510  ??0Font@UI@@QEAA@XZ

    0x1801175a0  ??1Font@UI@@UEAA@XZ

    0x1800319d0  ??2Font@UI@@SAPEAX_K@Z

    0x180031a40  ??2Font@UI@@SAPEAX_KPEAX@Z

    0x180031a50  ??3Font@UI@@SAXPEAX@Z

    0x180031f60  ??4Font@UI@@QEAAAEAV01@AEBV01@@Z

    0x180155b50  ??_7Font@UI@@6B@

    0x1801176c0  ?Compile@Font@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z

    0x180116ee0  ?ComputeCharacterPosition@Font@UI@@QEBAHAEBVString@System@@H@Z

    0x180116c70  ?ComputeLineLayout@Font@UI@@QEBAHPEBDHHHHHPEAULayout@12@@Z

    0x180116fd0  ?ComputeStringLayout@Font@UI@@QEBAXAEBVString@System@@_NHHW4Justify@12@AEAH3PEAULayout@12@@Z

    0x180031ce0  ?Construct@Font@UI@@UEAAPEAVExternal@Object@@XZ

    0x180031ba0  ?CreateInPlaceObject@Font@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180031ab0  ?CreateNewObject@Font@UI@@SAPEAVBase@Object@@XZ

    0x180031d80  ?Destruct@Font@UI@@UEAAXXZ

    0x180031df0  ?GetBaseLine@Font@UI@@QEBAHXZ

    0x1801176b0  ?GetFileVersion@Font@UI@@UEBAHXZ

    0x180031de0  ?GetHeight@Font@UI@@QEBAHXZ

    0x180031dd0  ?GetKern@Font@UI@@QEBAAEBUKerning@12@H@Z

    0x180031e00  ?GetSpaceWidth@Font@UI@@QEBAHXZ

    0x180031da0  ?GetSpriteIndex@Font@UI@@QEBAHH@Z

    0x180031d90  ?GetSpriteSheet@Font@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ

    0x180031c40  ?Reconstruct@Font@UI@@UEAAPEAVExternal@Object@@XZ

    0x1801176d0  ?Serialize@Font@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800319a0  ?Type@Font@UI@@UEBAIXZ

    0x1800319b0  ?TypeId@Font@UI@@SAIXZ

    0x1800319c0  ?TypeName@Font@UI@@UEBAPEBDXZ

    0x180031990  ?_ParentTypeName@Font@UI@@CAPEBDXZ

    0x180031980  ?_TypeName@Font@UI@@CAPEBDXZ

    0x1801fab68  ?_typeInfo@Font@UI@@0VBaseTypeInfo@Object@@A



##### UI::GraphDescription   exports=22 vtable=0x180156a30

    0x180040250  ??0GraphDescription@UI@@QEAA@AEBV01@@Z

    0x1800d1bd0  ??0GraphDescription@UI@@QEAA@XZ

    0x1800d1b30  ??1GraphDescription@UI@@UEAA@XZ

    0x1800400a0  ??2GraphDescription@UI@@SAPEAX_K@Z

    0x180040110  ??2GraphDescription@UI@@SAPEAX_KPEAX@Z

    0x180040120  ??3GraphDescription@UI@@SAXPEAX@Z

    0x1800402d0  ??4GraphDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156a30  ??_7GraphDescription@UI@@6B@

    0x180040230  ?Construct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180040200  ?CreateInPlaceObject@GraphDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d1920  ?CreateInstance@GraphDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180040180  ?CreateNewObject@GraphDescription@UI@@SAPEAVBase@Object@@XZ

    0x180040240  ?Destruct@GraphDescription@UI@@UEAAXXZ

    0x1800d1b20  ?GetFileVersion@GraphDescription@UI@@UEBAHXZ

    0x180040210  ?Reconstruct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d1a30  ?Serialize@GraphDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180040070  ?Type@GraphDescription@UI@@UEBAIXZ

    0x180040080  ?TypeId@GraphDescription@UI@@SAIXZ

    0x180040090  ?TypeName@GraphDescription@UI@@UEBAPEBDXZ

    0x180040060  ?_ParentTypeName@GraphDescription@UI@@CAPEBDXZ

    0x180040050  ?_TypeName@GraphDescription@UI@@CAPEBDXZ

    0x1801f9f98  ?_typeInfo@GraphDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::GraphElement   exports=25 vtable=0x1801569d0

    0x1800406d0  ??0GraphElement@UI@@QEAA@AEBV01@@Z

    0x1800d1880  ??0GraphElement@UI@@QEAA@XZ

    0x1800d1840  ??1GraphElement@UI@@UEAA@XZ

    0x180040470  ??2GraphElement@UI@@SAPEAX_K@Z

    0x1800404e0  ??2GraphElement@UI@@SAPEAX_KPEAX@Z

    0x1800404f0  ??3GraphElement@UI@@SAXPEAX@Z

    0x1800407a0  ??4GraphElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801569d0  ??_7GraphElement@UI@@6B@

    0x180040620  ?AddGraph@GraphElement@UI@@QEAAXI@Z

    0x180040690  ?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z

    0x1800d16b0  ?ComputeLayout@GraphElement@UI@@UEAAXHHHH@Z

    0x1800d1810  ?ComputeSize@GraphElement@UI@@UEAAXXZ

    0x1800405d0  ?CreateInPlaceObject@GraphElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180040550  ?CreateNewObject@GraphElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d1360  ?Draw@GraphElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180040610  ?RemoveGraphs@GraphElement@UI@@QEAAXXZ

    0x1800405f0  ?SetXDivisions@GraphElement@UI@@QEAAXH@Z

    0x180040600  ?SetYDivisions@GraphElement@UI@@QEAAXH@Z

    0x1800405e0  ?SetYRange@GraphElement@UI@@QEAAXM@Z

    0x180040440  ?Type@GraphElement@UI@@UEBAIXZ

    0x180040450  ?TypeId@GraphElement@UI@@SAIXZ

    0x180040460  ?TypeName@GraphElement@UI@@UEBAPEBDXZ

    0x180040430  ?_ParentTypeName@GraphElement@UI@@CAPEBDXZ

    0x180040420  ?_TypeName@GraphElement@UI@@CAPEBDXZ

    0x1801f9fd0  ?_typeInfo@GraphElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::GroupDescription   exports=22 vtable=0x180157190

    0x180046a30  ??0GroupDescription@UI@@QEAA@AEBV01@@Z

    0x180117b60  ??0GroupDescription@UI@@QEAA@XZ

    0x180117c00  ??1GroupDescription@UI@@UEAA@XZ

    0x180046670  ??2GroupDescription@UI@@SAPEAX_K@Z

    0x1800466e0  ??2GroupDescription@UI@@SAPEAX_KPEAX@Z

    0x1800466f0  ??3GroupDescription@UI@@SAXPEAX@Z

    0x180046a90  ??4GroupDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157190  ??_7GroupDescription@UI@@6B@

    0x180046980  ?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180046840  ?CreateInPlaceObject@GroupDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180117dc0  ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180046750  ?CreateNewObject@GroupDescription@UI@@SAPEAVBase@Object@@XZ

    0x180046a20  ?Destruct@GroupDescription@UI@@UEAAXXZ

    0x180117cf0  ?GetFileVersion@GroupDescription@UI@@UEBAHXZ

    0x1800468e0  ?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180117d00  ?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180046640  ?Type@GroupDescription@UI@@UEBAIXZ

    0x180046650  ?TypeId@GroupDescription@UI@@SAIXZ

    0x180046660  ?TypeName@GroupDescription@UI@@UEBAPEBDXZ

    0x180046630  ?_ParentTypeName@GroupDescription@UI@@CAPEBDXZ

    0x180046620  ?_TypeName@GroupDescription@UI@@CAPEBDXZ

    0x1801faba0  ?_typeInfo@GroupDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::GroupElement   exports=19 vtable=0x180157130

    0x180046d70  ??0GroupElement@UI@@QEAA@AEBV01@@Z

    0x180117c80  ??0GroupElement@UI@@QEAA@XZ

    0x180117cc0  ??1GroupElement@UI@@UEAA@XZ

    0x180046bc0  ??2GroupElement@UI@@SAPEAX_K@Z

    0x180046c30  ??2GroupElement@UI@@SAPEAX_KPEAX@Z

    0x180046c40  ??3GroupElement@UI@@SAXPEAX@Z

    0x180046da0  ??4GroupElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157130  ??_7GroupElement@UI@@6B@

    0x180118050  ?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z

    0x180117f00  ?ComputeSize@GroupElement@UI@@UEAAXXZ

    0x180046d30  ?CreateInPlaceObject@GroupElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180046ca0  ?CreateNewObject@GroupElement@UI@@SAPEAVBase@Object@@XZ

    0x1801181c0  ?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180046b90  ?Type@GroupElement@UI@@UEBAIXZ

    0x180046ba0  ?TypeId@GroupElement@UI@@SAIXZ

    0x180046bb0  ?TypeName@GroupElement@UI@@UEBAPEBDXZ

    0x180046b80  ?_ParentTypeName@GroupElement@UI@@CAPEBDXZ

    0x180046b70  ?_TypeName@GroupElement@UI@@CAPEBDXZ

    0x1801fabd8  ?_typeInfo@GroupElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ImageDescription   exports=23 vtable=0x180156110

    0x1800360d0  ??0ImageDescription@UI@@QEAA@AEBV01@@Z

    0x1801182d0  ??0ImageDescription@UI@@QEAA@XZ

    0x180118370  ??1ImageDescription@UI@@UEAA@XZ

    0x180035ce0  ??2ImageDescription@UI@@SAPEAX_K@Z

    0x180035d50  ??2ImageDescription@UI@@SAPEAX_KPEAX@Z

    0x180035d60  ??3ImageDescription@UI@@SAXPEAX@Z

    0x180036140  ??4ImageDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156110  ??_7ImageDescription@UI@@6B@

    0x180036010  ?Construct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180035eb0  ?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180118570  ?CreateInstance@ImageDescription@UI@@QEBAXV?$InternalPtr@VImageElement@UI@@@Object@@@Z

    0x180118910  ?CreateInstance@ImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180035dc0  ?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ

    0x1800360c0  ?Destruct@ImageDescription@UI@@UEAAXXZ

    0x1801184a0  ?GetFileVersion@ImageDescription@UI@@UEBAHXZ

    0x180035f60  ?Reconstruct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1801184b0  ?Serialize@ImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180035cb0  ?Type@ImageDescription@UI@@UEBAIXZ

    0x180035cc0  ?TypeId@ImageDescription@UI@@SAIXZ

    0x180035cd0  ?TypeName@ImageDescription@UI@@UEBAPEBDXZ

    0x180035ca0  ?_ParentTypeName@ImageDescription@UI@@CAPEBDXZ

    0x180035c90  ?_TypeName@ImageDescription@UI@@CAPEBDXZ

    0x1801fac10  ?_typeInfo@ImageDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ImageElement   exports=23 vtable=0x1801560b0

    0x1800364c0  ??0ImageElement@UI@@QEAA@AEBV01@@Z

    0x180118410  ??0ImageElement@UI@@QEAA@XZ

    0x180118470  ??1ImageElement@UI@@UEAA@XZ

    0x1800362c0  ??2ImageElement@UI@@SAPEAX_K@Z

    0x180036330  ??2ImageElement@UI@@SAPEAX_KPEAX@Z

    0x180036340  ??3ImageElement@UI@@SAXPEAX@Z

    0x180036540  ??4ImageElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801560b0  ??_7ImageElement@UI@@6B@

    0x1801186f0  ?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z

    0x180118650  ?ComputeSize@ImageElement@UI@@UEAAXXZ

    0x180036450  ?CreateInPlaceObject@ImageElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800363a0  ?CreateNewObject@ImageElement@UI@@SAPEAVBase@Object@@XZ

    0x180118850  ?Draw@ImageElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1801182a0  ?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ

    0x1801182c0  ?SetRotation@ImageElement@UI@@QEAAXM@Z

    0x180118600  ?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z

    0x180118260  ?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z

    0x180036290  ?Type@ImageElement@UI@@UEBAIXZ

    0x1800362a0  ?TypeId@ImageElement@UI@@SAIXZ

    0x1800362b0  ?TypeName@ImageElement@UI@@UEBAPEBDXZ

    0x180036280  ?_ParentTypeName@ImageElement@UI@@CAPEBDXZ

    0x180036270  ?_TypeName@ImageElement@UI@@CAPEBDXZ

    0x1801fac48  ?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::LabelDescription   exports=22 vtable=0x180156b30

    0x180042570  ??0LabelDescription@UI@@QEAA@AEBV01@@Z

    0x1800d29d0  ??0LabelDescription@UI@@QEAA@XZ

    0x1800d2960  ??1LabelDescription@UI@@UEAA@XZ

    0x1800423c0  ??2LabelDescription@UI@@SAPEAX_K@Z

    0x180042430  ??2LabelDescription@UI@@SAPEAX_KPEAX@Z

    0x180042440  ??3LabelDescription@UI@@SAXPEAX@Z

    0x180042630  ??4LabelDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156b30  ??_7LabelDescription@UI@@6B@

    0x180042550  ?Construct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180042520  ?CreateInPlaceObject@LabelDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d26b0  ?CreateInstance@LabelDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x1800424a0  ?CreateNewObject@LabelDescription@UI@@SAPEAVBase@Object@@XZ

    0x180042560  ?Destruct@LabelDescription@UI@@UEAAXXZ

    0x1800d2950  ?GetFileVersion@LabelDescription@UI@@UEBAHXZ

    0x180042530  ?Reconstruct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d2820  ?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180042390  ?Type@LabelDescription@UI@@UEBAIXZ

    0x1800423a0  ?TypeId@LabelDescription@UI@@SAIXZ

    0x1800423b0  ?TypeName@LabelDescription@UI@@UEBAPEBDXZ

    0x180042380  ?_ParentTypeName@LabelDescription@UI@@CAPEBDXZ

    0x180042370  ?_TypeName@LabelDescription@UI@@CAPEBDXZ

    0x1801fa008  ?_typeInfo@LabelDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::LabelElement   exports=23 vtable=0x180156ad0

    0x180042a00  ??0LabelElement@UI@@QEAA@AEBV01@@Z

    0x1800d2620  ??0LabelElement@UI@@QEAA@XZ

    0x1800d25f0  ??1LabelElement@UI@@UEAA@XZ

    0x180042800  ??2LabelElement@UI@@SAPEAX_K@Z

    0x180042870  ??2LabelElement@UI@@SAPEAX_KPEAX@Z

    0x180042880  ??3LabelElement@UI@@SAXPEAX@Z

    0x180042ac0  ??4LabelElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156ad0  ??_7LabelElement@UI@@6B@

    0x1800d2330  ?ComputeLayout@LabelElement@UI@@UEAAXHHHH@Z

    0x1800d2490  ?ComputeSize@LabelElement@UI@@UEAAXXZ

    0x180042960  ?CreateInPlaceObject@LabelElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800428e0  ?CreateNewObject@LabelElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d1c80  ?Draw@LabelElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800429d0  ?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ

    0x180042970  ?SetColor@LabelElement@UI@@UEAAXI@Z

    0x180042980  ?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z

    0x1800429f0  ?SetSpriteColor@LabelElement@UI@@QEAAXI@Z

    0x1800427d0  ?Type@LabelElement@UI@@UEBAIXZ

    0x1800427e0  ?TypeId@LabelElement@UI@@SAIXZ

    0x1800427f0  ?TypeName@LabelElement@UI@@UEBAPEBDXZ

    0x1800427c0  ?_ParentTypeName@LabelElement@UI@@CAPEBDXZ

    0x1800427b0  ?_TypeName@LabelElement@UI@@CAPEBDXZ

    0x1801fa040  ?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::LayoutDescription   exports=22 vtable=0x180156cc0

    0x180043110  ??0LayoutDescription@UI@@QEAA@AEBV01@@Z

    0x1800d3a90  ??0LayoutDescription@UI@@QEAA@XZ

    0x1800d3a10  ??1LayoutDescription@UI@@UEAA@XZ

    0x180042d50  ??2LayoutDescription@UI@@SAPEAX_K@Z

    0x180042dc0  ??2LayoutDescription@UI@@SAPEAX_KPEAX@Z

    0x180042dd0  ??3LayoutDescription@UI@@SAXPEAX@Z

    0x180043170  ??4LayoutDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156cc0  ??_7LayoutDescription@UI@@6B@

    0x180043060  ?Construct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180042f20  ?CreateInPlaceObject@LayoutDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d33a0  ?CreateInstance@LayoutDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180042e30  ?CreateNewObject@LayoutDescription@UI@@SAPEAVBase@Object@@XZ

    0x180043100  ?Destruct@LayoutDescription@UI@@UEAAXXZ

    0x1800d3a00  ?GetFileVersion@LayoutDescription@UI@@UEBAHXZ

    0x180042fc0  ?Reconstruct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d3940  ?Serialize@LayoutDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180042d20  ?Type@LayoutDescription@UI@@UEBAIXZ

    0x180042d30  ?TypeId@LayoutDescription@UI@@SAIXZ

    0x180042d40  ?TypeName@LayoutDescription@UI@@UEBAPEBDXZ

    0x180042d10  ?_ParentTypeName@LayoutDescription@UI@@CAPEBDXZ

    0x180042d00  ?_TypeName@LayoutDescription@UI@@CAPEBDXZ

    0x1801fa078  ?_typeInfo@LayoutDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::LayoutElement   exports=22 vtable=0x180156c60

    0x180043470  ??0LayoutElement@UI@@QEAA@AEBV01@@Z

    0x1800d32e0  ??0LayoutElement@UI@@QEAA@XZ

    0x1800d32a0  ??1LayoutElement@UI@@UEAA@XZ

    0x1800432a0  ??2LayoutElement@UI@@SAPEAX_K@Z

    0x180043310  ??2LayoutElement@UI@@SAPEAX_KPEAX@Z

    0x180043320  ??3LayoutElement@UI@@SAXPEAX@Z

    0x1800434d0  ??4LayoutElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156c60  ??_7LayoutElement@UI@@6B@

    0x1800d2af0  ?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z

    0x1800d3100  ?ComputeSize@LayoutElement@UI@@UEAAXXZ

    0x180043420  ?CreateInPlaceObject@LayoutElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180043380  ?CreateNewObject@LayoutElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d2a40  ?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800d2dc0  ?GetChildLayout@LayoutElement@UI@@AEAAXHHHHHH_N@Z

    0x1800d3090  ?GetChildLayoutX@LayoutElement@UI@@AEBAHW4LayoutHorizontal@LayoutDescription@2@HHHH@Z

    0x1800d3020  ?GetChildLayoutY@LayoutElement@UI@@AEBAHW4LayoutVertical@LayoutDescription@2@HHHH@Z

    0x180043270  ?Type@LayoutElement@UI@@UEBAIXZ

    0x180043280  ?TypeId@LayoutElement@UI@@SAIXZ

    0x180043290  ?TypeName@LayoutElement@UI@@UEBAPEBDXZ

    0x180043260  ?_ParentTypeName@LayoutElement@UI@@CAPEBDXZ

    0x180043250  ?_TypeName@LayoutElement@UI@@CAPEBDXZ

    0x1801fa0b0  ?_typeInfo@LayoutElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ListDescription   exports=22 vtable=0x180156e20

    0x180044d60  ??0ListDescription@UI@@QEAA@AEBV01@@Z

    0x1800d49b0  ??0ListDescription@UI@@QEAA@XZ

    0x1800d4910  ??1ListDescription@UI@@UEAA@XZ

    0x180044bb0  ??2ListDescription@UI@@SAPEAX_K@Z

    0x180044c20  ??2ListDescription@UI@@SAPEAX_KPEAX@Z

    0x180044c30  ??3ListDescription@UI@@SAXPEAX@Z

    0x180044df0  ??4ListDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156e20  ??_7ListDescription@UI@@6B@

    0x180044d40  ?Construct@ListDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180044d10  ?CreateInPlaceObject@ListDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d45f0  ?CreateInstance@ListDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180044c90  ?CreateNewObject@ListDescription@UI@@SAPEAVBase@Object@@XZ

    0x180044d50  ?Destruct@ListDescription@UI@@UEAAXXZ

    0x1800d4900  ?GetFileVersion@ListDescription@UI@@UEBAHXZ

    0x180044d20  ?Reconstruct@ListDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d4810  ?Serialize@ListDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180044b80  ?Type@ListDescription@UI@@UEBAIXZ

    0x180044b90  ?TypeId@ListDescription@UI@@SAIXZ

    0x180044ba0  ?TypeName@ListDescription@UI@@UEBAPEBDXZ

    0x180044b70  ?_ParentTypeName@ListDescription@UI@@CAPEBDXZ

    0x180044b60  ?_TypeName@ListDescription@UI@@CAPEBDXZ

    0x1801fa0e8  ?_typeInfo@ListDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ListElement   exports=28 vtable=0x180156dc0

    0x180045140  ??0ListElement@UI@@QEAA@AEBV01@@Z

    0x1800d4570  ??0ListElement@UI@@QEAA@XZ

    0x1800d4530  ??1ListElement@UI@@UEAA@XZ

    0x180044fa0  ??2ListElement@UI@@SAPEAX_K@Z

    0x180045010  ??2ListElement@UI@@SAPEAX_KPEAX@Z

    0x180045020  ??3ListElement@UI@@SAXPEAX@Z

    0x180045220  ??4ListElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156dc0  ??_7ListElement@UI@@6B@

    0x1800d44b0  ?AddItem@ListElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@PEAX@Z

    0x1800d4210  ?ComputeLayout@ListElement@UI@@UEAAXHHHH@Z

    0x1800d4380  ?ComputeSize@ListElement@UI@@UEAAXXZ

    0x180045100  ?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180045080  ?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d3eb0  ?Draw@ListElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800d3f70  ?DrawHighlight@ListElement@UI@@QEBAXAEAVSpriteContext@2@HI@Z

    0x1800d4120  ?GetCellLayout@ListElement@UI@@AEBA_NHAEAH000@Z

    0x180045130  ?GetCurrentSelection@ListElement@UI@@QEBAHXZ

    0x1800d43c0  ?GetHeight@ListElement@UI@@QEBAHXZ

    0x1800d4400  ?GetWidth@ListElement@UI@@QEBAHXZ

    0x180045110  ?SetCurrentSelection@ListElement@UI@@QEAAXH@Z

    0x1800d4440  ?SetSize@ListElement@UI@@QEAAXHH@Z

    0x180044f70  ?Type@ListElement@UI@@UEBAIXZ

    0x180044f80  ?TypeId@ListElement@UI@@SAIXZ

    0x180044f90  ?TypeName@ListElement@UI@@UEBAPEBDXZ

    0x1800d3c20  ?Update@ListElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x180044f60  ?_ParentTypeName@ListElement@UI@@CAPEBDXZ

    0x180044f50  ?_TypeName@ListElement@UI@@CAPEBDXZ

    0x1801fa120  ?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::NumberDescription   exports=22 vtable=0x180157530

    0x18004a620  ??0NumberDescription@UI@@QEAA@AEBV01@@Z

    0x180118b60  ??0NumberDescription@UI@@QEAA@XZ

    0x180118c00  ??1NumberDescription@UI@@UEAA@XZ

    0x18004a250  ??2NumberDescription@UI@@SAPEAX_K@Z

    0x18004a2c0  ??2NumberDescription@UI@@SAPEAX_KPEAX@Z

    0x18004a2d0  ??3NumberDescription@UI@@SAXPEAX@Z

    0x18004a690  ??4NumberDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157530  ??_7NumberDescription@UI@@6B@

    0x18004a570  ?Construct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18004a420  ?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180118f10  ?CreateInstance@NumberDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18004a330  ?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ

    0x18004a610  ?Destruct@NumberDescription@UI@@UEAAXXZ

    0x180118e20  ?GetFileVersion@NumberDescription@UI@@UEBAHXZ

    0x18004a4c0  ?Reconstruct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180118e30  ?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18004a220  ?Type@NumberDescription@UI@@UEBAIXZ

    0x18004a230  ?TypeId@NumberDescription@UI@@SAIXZ

    0x18004a240  ?TypeName@NumberDescription@UI@@UEBAPEBDXZ

    0x18004a210  ?_ParentTypeName@NumberDescription@UI@@CAPEBDXZ

    0x18004a200  ?_TypeName@NumberDescription@UI@@CAPEBDXZ

    0x1801fac80  ?_typeInfo@NumberDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::NumberEditDescription   exports=22 vtable=0x180156fe0

    0x180046050  ??0NumberEditDescription@UI@@QEAA@AEBV01@@Z

    0x1800d5050  ??0NumberEditDescription@UI@@QEAA@XZ

    0x1800d4fe0  ??1NumberEditDescription@UI@@UEAA@XZ

    0x180045ea0  ??2NumberEditDescription@UI@@SAPEAX_K@Z

    0x180045f10  ??2NumberEditDescription@UI@@SAPEAX_KPEAX@Z

    0x180045f20  ??3NumberEditDescription@UI@@SAXPEAX@Z

    0x1800460c0  ??4NumberEditDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156fe0  ??_7NumberEditDescription@UI@@6B@

    0x180046030  ?Construct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180046000  ?CreateInPlaceObject@NumberEditDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d4cc0  ?CreateInstance@NumberEditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180045f80  ?CreateNewObject@NumberEditDescription@UI@@SAPEAVBase@Object@@XZ

    0x180046040  ?Destruct@NumberEditDescription@UI@@UEAAXXZ

    0x1800d4fd0  ?GetFileVersion@NumberEditDescription@UI@@UEBAHXZ

    0x180046010  ?Reconstruct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d4ee0  ?Serialize@NumberEditDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180045e70  ?Type@NumberEditDescription@UI@@UEBAIXZ

    0x180045e80  ?TypeId@NumberEditDescription@UI@@SAIXZ

    0x180045e90  ?TypeName@NumberEditDescription@UI@@UEBAPEBDXZ

    0x180045e60  ?_ParentTypeName@NumberEditDescription@UI@@CAPEBDXZ

    0x180045e50  ?_TypeName@NumberEditDescription@UI@@CAPEBDXZ

    0x1801fa158  ?_typeInfo@NumberEditDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::NumberEditElement   exports=22 vtable=0x180156f70

    0x180046450  ??0NumberEditElement@UI@@QEAA@AEBV01@@Z

    0x1800d4c70  ??0NumberEditElement@UI@@QEAA@XZ

    0x1800d4c40  ??1NumberEditElement@UI@@UEAA@XZ

    0x180046260  ??2NumberEditElement@UI@@SAPEAX_K@Z

    0x1800462d0  ??2NumberEditElement@UI@@SAPEAX_KPEAX@Z

    0x1800462e0  ??3NumberEditElement@UI@@SAXPEAX@Z

    0x1800464b0  ??4NumberEditElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156f70  ??_7NumberEditElement@UI@@6B@

    0x1800463e0  ?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180046340  ?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d4a60  ?FinalizeText@NumberEditElement@UI@@UEAAXXZ

    0x180046430  ?GetMaxValue@NumberEditElement@UI@@QEBAHXZ

    0x180046440  ?GetMinValue@NumberEditElement@UI@@QEBAHXZ

    0x1800d4b50  ?GetValue@NumberEditElement@UI@@QEBAHXZ

    0x1800d4b70  ?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z

    0x1800d4bb0  ?SetValue@NumberEditElement@UI@@QEAAXH@Z

    0x180046230  ?Type@NumberEditElement@UI@@UEBAIXZ

    0x180046240  ?TypeId@NumberEditElement@UI@@SAIXZ

    0x180046250  ?TypeName@NumberEditElement@UI@@UEBAPEBDXZ

    0x180046220  ?_ParentTypeName@NumberEditElement@UI@@CAPEBDXZ

    0x180046210  ?_TypeName@NumberEditElement@UI@@CAPEBDXZ

    0x1801fa190  ?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::NumberElement   exports=25 vtable=0x1801574d0

    0x18004a9d0  ??0NumberElement@UI@@QEAA@AEBV01@@Z

    0x180118c70  ??0NumberElement@UI@@QEAA@XZ

    0x180118ce0  ??1NumberElement@UI@@UEAA@XZ

    0x18004a830  ??2NumberElement@UI@@SAPEAX_K@Z

    0x18004a8a0  ??2NumberElement@UI@@SAPEAX_KPEAX@Z

    0x18004a8b0  ??3NumberElement@UI@@SAXPEAX@Z

    0x18004aa80  ??4NumberElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801574d0  ??_7NumberElement@UI@@6B@

    0x1801189f0  ?CacheText@NumberElement@UI@@AEAAXXZ

    0x180119080  ?ComputeLayout@NumberElement@UI@@UEAAXHHHH@Z

    0x180119000  ?ComputeSize@NumberElement@UI@@UEAAXXZ

    0x18004a990  ?CreateInPlaceObject@NumberElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18004a910  ?CreateNewObject@NumberElement@UI@@SAPEAVBase@Object@@XZ

    0x1801191e0  ?Draw@NumberElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x18004a9b0  ?GetFloatValue@NumberElement@UI@@QEBAMXZ

    0x18004a9a0  ?GetIntValue@NumberElement@UI@@QEBAHXZ

    0x18004a9c0  ?GetNumberType@NumberElement@UI@@QEBA?AW4NumberType@NumberDescription@2@XZ

    0x180118d50  ?SetValue@NumberElement@UI@@QEAAXH@Z

    0x180118db0  ?SetValue@NumberElement@UI@@QEAAXM@Z

    0x18004a800  ?Type@NumberElement@UI@@UEBAIXZ

    0x18004a810  ?TypeId@NumberElement@UI@@SAIXZ

    0x18004a820  ?TypeName@NumberElement@UI@@UEBAPEBDXZ

    0x18004a7f0  ?_ParentTypeName@NumberElement@UI@@CAPEBDXZ

    0x18004a7e0  ?_TypeName@NumberElement@UI@@CAPEBDXZ

    0x1801facb8  ?_typeInfo@NumberElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::PageDescription   exports=22 vtable=0x1801570e0

    0x1800470a0  ??0PageDescription@UI@@QEAA@AEBV01@@Z

    0x1800d54e0  ??0PageDescription@UI@@QEAA@XZ

    0x1800d54b0  ??1PageDescription@UI@@UEAA@XZ

    0x180046ef0  ??2PageDescription@UI@@SAPEAX_K@Z

    0x180046f60  ??2PageDescription@UI@@SAPEAX_KPEAX@Z

    0x180046f70  ??3PageDescription@UI@@SAXPEAX@Z

    0x180047110  ??4PageDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801570e0  ??_7PageDescription@UI@@6B@

    0x180047080  ?Construct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180047050  ?CreateInPlaceObject@PageDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d52b0  ?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180046fd0  ?CreateNewObject@PageDescription@UI@@SAPEAVBase@Object@@XZ

    0x180047090  ?Destruct@PageDescription@UI@@UEAAXXZ

    0x1800d54a0  ?GetFileVersion@PageDescription@UI@@UEBAHXZ

    0x180047060  ?Reconstruct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d5410  ?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180046ec0  ?Type@PageDescription@UI@@UEBAIXZ

    0x180046ed0  ?TypeId@PageDescription@UI@@SAIXZ

    0x180046ee0  ?TypeName@PageDescription@UI@@UEBAPEBDXZ

    0x180046eb0  ?_ParentTypeName@PageDescription@UI@@CAPEBDXZ

    0x180046ea0  ?_TypeName@PageDescription@UI@@CAPEBDXZ

    0x1801fa1c8  ?_typeInfo@PageDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::PageElement   exports=18 vtable=0x180157080

    0x180047450  ??0PageElement@UI@@QEAA@AEBV01@@Z

    0x1800d5260  ??0PageElement@UI@@QEAA@XZ

    0x1800d5220  ??1PageElement@UI@@UEAA@XZ

    0x180047280  ??2PageElement@UI@@SAPEAX_K@Z

    0x1800472f0  ??2PageElement@UI@@SAPEAX_KPEAX@Z

    0x180047300  ??3PageElement@UI@@SAXPEAX@Z

    0x1800474b0  ??4PageElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157080  ??_7PageElement@UI@@6B@

    0x180047400  ?CreateInPlaceObject@PageElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180047360  ?CreateNewObject@PageElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d5100  ?GetActivePage@PageElement@UI@@QEBAHXZ

    0x1800d5110  ?SetActivePage@PageElement@UI@@QEAAXH@Z

    0x180047250  ?Type@PageElement@UI@@UEBAIXZ

    0x180047260  ?TypeId@PageElement@UI@@SAIXZ

    0x180047270  ?TypeName@PageElement@UI@@UEBAPEBDXZ

    0x180047240  ?_ParentTypeName@PageElement@UI@@CAPEBDXZ

    0x180047230  ?_TypeName@PageElement@UI@@CAPEBDXZ

    0x1801fa200  ?_typeInfo@PageElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ProgressDescription   exports=22 vtable=0x1801591e0

    0x180077930  ??0ProgressDescription@UI@@QEAA@AEBV01@@Z

    0x18011a210  ??0ProgressDescription@UI@@QEAA@XZ

    0x18011a080  ??1ProgressDescription@UI@@UEAA@XZ

    0x180077780  ??2ProgressDescription@UI@@SAPEAX_K@Z

    0x1800777f0  ??2ProgressDescription@UI@@SAPEAX_KPEAX@Z

    0x180077800  ??3ProgressDescription@UI@@SAXPEAX@Z

    0x180077a30  ??4ProgressDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801591e0  ??_7ProgressDescription@UI@@6B@

    0x180077910  ?Construct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800778e0  ?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180119d00  ?CreateInstance@ProgressDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180077860  ?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ

    0x180077920  ?Destruct@ProgressDescription@UI@@UEAAXXZ

    0x18011a070  ?GetFileVersion@ProgressDescription@UI@@UEBAHXZ

    0x1800778f0  ?Reconstruct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180119f60  ?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180077750  ?Type@ProgressDescription@UI@@UEBAIXZ

    0x180077760  ?TypeId@ProgressDescription@UI@@SAIXZ

    0x180077770  ?TypeName@ProgressDescription@UI@@UEBAPEBDXZ

    0x180077740  ?_ParentTypeName@ProgressDescription@UI@@CAPEBDXZ

    0x180077730  ?_TypeName@ProgressDescription@UI@@CAPEBDXZ

    0x1801facf0  ?_typeInfo@ProgressDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ProgressElement   exports=24 vtable=0x180159180

    0x180077f10  ??0ProgressElement@UI@@QEAA@AEBV01@@Z

    0x180119c40  ??0ProgressElement@UI@@QEAA@XZ

    0x180077ef0  ??1ProgressElement@UI@@UEAA@XZ

    0x180077d50  ??2ProgressElement@UI@@SAPEAX_K@Z

    0x180077dc0  ??2ProgressElement@UI@@SAPEAX_KPEAX@Z

    0x180077dd0  ??3ProgressElement@UI@@SAXPEAX@Z

    0x180078020  ??4ProgressElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180159180  ??_7ProgressElement@UI@@6B@

    0x180119610  ?ComputeLayout@ProgressElement@UI@@UEAAXHHHH@Z

    0x1801198f0  ?ComputeSize@ProgressElement@UI@@UEAAXXZ

    0x180077eb0  ?CreateInPlaceObject@ProgressElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180077e30  ?CreateNewObject@ProgressElement@UI@@SAPEAVBase@Object@@XZ

    0x1801192c0  ?Draw@ProgressElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180077ee0  ?SetBackgroundColor@ProgressElement@UI@@QEAAXI@Z

    0x180077ed0  ?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z

    0x180077ec0  ?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z

    0x180119bd0  ?SetProgressText@ProgressElement@UI@@QEAAXVString@2@@Z

    0x180119b30  ?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z

    0x180077d20  ?Type@ProgressElement@UI@@UEBAIXZ

    0x180077d30  ?TypeId@ProgressElement@UI@@SAIXZ

    0x180077d40  ?TypeName@ProgressElement@UI@@UEBAPEBDXZ

    0x180077d10  ?_ParentTypeName@ProgressElement@UI@@CAPEBDXZ

    0x180077d00  ?_TypeName@ProgressElement@UI@@CAPEBDXZ

    0x1801fad28  ?_typeInfo@ProgressElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::RadioDescription   exports=22 vtable=0x180157420

    0x180047830  ??0RadioDescription@UI@@QEAA@AEBV01@@Z

    0x1800d5b50  ??0RadioDescription@UI@@QEAA@XZ

    0x1800d5b20  ??1RadioDescription@UI@@UEAA@XZ

    0x180047680  ??2RadioDescription@UI@@SAPEAX_K@Z

    0x1800476f0  ??2RadioDescription@UI@@SAPEAX_KPEAX@Z

    0x180047700  ??3RadioDescription@UI@@SAXPEAX@Z

    0x1800478a0  ??4RadioDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157420  ??_7RadioDescription@UI@@6B@

    0x180047810  ?Construct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800477e0  ?CreateInPlaceObject@RadioDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d58d0  ?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180047760  ?CreateNewObject@RadioDescription@UI@@SAPEAVBase@Object@@XZ

    0x180047820  ?Destruct@RadioDescription@UI@@UEAAXXZ

    0x1800d5b10  ?GetFileVersion@RadioDescription@UI@@UEBAHXZ

    0x1800477f0  ?Reconstruct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d5a80  ?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180047650  ?Type@RadioDescription@UI@@UEBAIXZ

    0x180047660  ?TypeId@RadioDescription@UI@@SAIXZ

    0x180047670  ?TypeName@RadioDescription@UI@@UEBAPEBDXZ

    0x180047640  ?_ParentTypeName@RadioDescription@UI@@CAPEBDXZ

    0x180047630  ?_TypeName@RadioDescription@UI@@CAPEBDXZ

    0x1801fa238  ?_typeInfo@RadioDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::RadioElement   exports=21 vtable=0x1801573c0

    0x180047c20  ??0RadioElement@UI@@QEAA@AEBV01@@Z

    0x1800d5870  ??0RadioElement@UI@@QEAA@XZ

    0x1800d5820  ??1RadioElement@UI@@UEAA@XZ

    0x180047a10  ??2RadioElement@UI@@SAPEAX_K@Z

    0x180047a80  ??2RadioElement@UI@@SAPEAX_KPEAX@Z

    0x180047a90  ??3RadioElement@UI@@SAXPEAX@Z

    0x180047c90  ??4RadioElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801573c0  ??_7RadioElement@UI@@6B@

    0x180047ba0  ?CreateInPlaceObject@RadioElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180047af0  ?CreateNewObject@RadioElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d5750  ?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x180047c00  ?FindCheckBoxes@RadioElement@UI@@QEAAXXZ

    0x1800d56e0  ?GetActiveRadio@RadioElement@UI@@QEBAHXZ

    0x1800d56f0  ?SetActiveRadio@RadioElement@UI@@QEAAXH@Z

    0x1800479e0  ?Type@RadioElement@UI@@UEBAIXZ

    0x1800479f0  ?TypeId@RadioElement@UI@@SAIXZ

    0x180047a00  ?TypeName@RadioElement@UI@@UEBAPEBDXZ

    0x1800d5580  ?Update@RadioElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x1800479d0  ?_ParentTypeName@RadioElement@UI@@CAPEBDXZ

    0x1800479c0  ?_TypeName@RadioElement@UI@@CAPEBDXZ

    0x1801fa270  ?_typeInfo@RadioElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::RibbonDescription   exports=22 vtable=0x180156ed0

    0x180044530  ??0RibbonDescription@UI@@QEAA@AEBV01@@Z

    0x1800d5bf0  ??0RibbonDescription@UI@@QEAA@XZ

    0x1800d5c90  ??1RibbonDescription@UI@@UEAA@XZ

    0x180044140  ??2RibbonDescription@UI@@SAPEAX_K@Z

    0x1800441b0  ??2RibbonDescription@UI@@SAPEAX_KPEAX@Z

    0x1800441c0  ??3RibbonDescription@UI@@SAXPEAX@Z

    0x1800445a0  ??4RibbonDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156ed0  ??_7RibbonDescription@UI@@6B@

    0x180044470  ?Construct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180044310  ?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d6000  ?CreateInstance@RibbonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180044220  ?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ

    0x180044520  ?Destruct@RibbonDescription@UI@@UEAAXXZ

    0x1800d5ef0  ?GetFileVersion@RibbonDescription@UI@@UEBAHXZ

    0x1800443c0  ?Reconstruct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d5f00  ?Serialize@RibbonDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180044110  ?Type@RibbonDescription@UI@@UEBAIXZ

    0x180044120  ?TypeId@RibbonDescription@UI@@SAIXZ

    0x180044130  ?TypeName@RibbonDescription@UI@@UEBAPEBDXZ

    0x180044100  ?_ParentTypeName@RibbonDescription@UI@@CAPEBDXZ

    0x1800440f0  ?_TypeName@RibbonDescription@UI@@CAPEBDXZ

    0x1801fa2a8  ?_typeInfo@RibbonDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::RibbonElement   exports=22 vtable=0x180156e70

    0x180044980  ??0RibbonElement@UI@@QEAA@AEBV01@@Z

    0x1800d5d10  ??0RibbonElement@UI@@QEAA@XZ

    0x1800d5d70  ??1RibbonElement@UI@@UEAA@XZ

    0x1800446f0  ??2RibbonElement@UI@@SAPEAX_K@Z

    0x180044760  ??2RibbonElement@UI@@SAPEAX_KPEAX@Z

    0x180044770  ??3RibbonElement@UI@@SAXPEAX@Z

    0x180044a00  ??4RibbonElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156e70  ??_7RibbonElement@UI@@6B@

    0x1800448e0  ?AddItem@RibbonElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x1800d6410  ?ComputeLayout@RibbonElement@UI@@UEAAXHHHH@Z

    0x1800d6190  ?ComputeSize@RibbonElement@UI@@UEAAXXZ

    0x180044880  ?CreateInPlaceObject@RibbonElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800447d0  ?CreateNewObject@RibbonElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d6740  ?Draw@RibbonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800d5db0  ?GetCellLayout@RibbonElement@UI@@QEBAXHAEAH000@Z

    0x180044970  ?IsVertical@RibbonElement@UI@@QEBA_NXZ

    0x1800446c0  ?Type@RibbonElement@UI@@UEBAIXZ

    0x1800446d0  ?TypeId@RibbonElement@UI@@SAIXZ

    0x1800446e0  ?TypeName@RibbonElement@UI@@UEBAPEBDXZ

    0x1800446b0  ?_ParentTypeName@RibbonElement@UI@@CAPEBDXZ

    0x1800446a0  ?_TypeName@RibbonElement@UI@@CAPEBDXZ

    0x1801fa2e0  ?_typeInfo@RibbonElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ScrollDescription   exports=22 vtable=0x180156460

    0x18003a2e0  ??0ScrollDescription@UI@@QEAA@AEBV01@@Z

    0x18011a4e0  ??0ScrollDescription@UI@@QEAA@XZ

    0x18011a5d0  ??1ScrollDescription@UI@@UEAA@XZ

    0x18003a130  ??2ScrollDescription@UI@@SAPEAX_K@Z

    0x18003a1a0  ??2ScrollDescription@UI@@SAPEAX_KPEAX@Z

    0x18003a1b0  ??3ScrollDescription@UI@@SAXPEAX@Z

    0x18003a450  ??4ScrollDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156460  ??_7ScrollDescription@UI@@6B@

    0x18003a2c0  ?Construct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18003a290  ?CreateInPlaceObject@ScrollDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18011aac0  ?CreateInstance@ScrollDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18003a210  ?CreateNewObject@ScrollDescription@UI@@SAPEAVBase@Object@@XZ

    0x18003a2d0  ?Destruct@ScrollDescription@UI@@UEAAXXZ

    0x18011a930  ?GetFileVersion@ScrollDescription@UI@@UEBAHXZ

    0x18003a2a0  ?Reconstruct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18011a940  ?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18003a100  ?Type@ScrollDescription@UI@@UEBAIXZ

    0x18003a110  ?TypeId@ScrollDescription@UI@@SAIXZ

    0x18003a120  ?TypeName@ScrollDescription@UI@@UEBAPEBDXZ

    0x18003a0f0  ?_ParentTypeName@ScrollDescription@UI@@CAPEBDXZ

    0x18003a0e0  ?_TypeName@ScrollDescription@UI@@CAPEBDXZ

    0x1801fad60  ?_typeInfo@ScrollDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::ScrollElement   exports=25 vtable=0x180156400

    0x18003aa30  ??0ScrollElement@UI@@QEAA@AEBV01@@Z

    0x18011a7f0  ??0ScrollElement@UI@@QEAA@XZ

    0x18011a900  ??1ScrollElement@UI@@UEAA@XZ

    0x18003a860  ??2ScrollElement@UI@@SAPEAX_K@Z

    0x18003a8d0  ??2ScrollElement@UI@@SAPEAX_KPEAX@Z

    0x18003a8e0  ??3ScrollElement@UI@@SAXPEAX@Z

    0x18003abe0  ??4ScrollElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156400  ??_7ScrollElement@UI@@6B@

    0x18011af20  ?ComputeLayout@ScrollElement@UI@@UEAAXHHHH@Z

    0x18011ad80  ?ComputeSize@ScrollElement@UI@@UEAAXXZ

    0x18003a9c0  ?CreateInPlaceObject@ScrollElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003a940  ?CreateNewObject@ScrollElement@UI@@SAPEAVBase@Object@@XZ

    0x18011b0b0  ?Draw@ScrollElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x18011a2d0  ?GetBarLayout@ScrollElement@UI@@AEBAXAEAH000@Z

    0x18003aa20  ?GetCurrentValue@ScrollElement@UI@@QEBAMXZ

    0x18003aa10  ?SetCurrentValue@ScrollElement@UI@@QEAAXM@Z

    0x18003a9f0  ?SetDisplayPercent@ScrollElement@UI@@QEAAXM@Z

    0x18003a9d0  ?SetMaxValue@ScrollElement@UI@@QEAAXM@Z

    0x18003a830  ?Type@ScrollElement@UI@@UEBAIXZ

    0x18003a840  ?TypeId@ScrollElement@UI@@SAIXZ

    0x18003a850  ?TypeName@ScrollElement@UI@@UEBAPEBDXZ

    0x18011b620  ?Update@ScrollElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x18003a820  ?_ParentTypeName@ScrollElement@UI@@CAPEBDXZ

    0x18003a810  ?_TypeName@ScrollElement@UI@@CAPEBDXZ

    0x1801fad98  ?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::SliderDescription   exports=22 vtable=0x1801575e0

    0x180049910  ??0SliderDescription@UI@@QEAA@AEBV01@@Z

    0x1800d7b60  ??0SliderDescription@UI@@QEAA@XZ

    0x1800d7a00  ??1SliderDescription@UI@@UEAA@XZ

    0x180049760  ??2SliderDescription@UI@@SAPEAX_K@Z

    0x1800497d0  ??2SliderDescription@UI@@SAPEAX_KPEAX@Z

    0x1800497e0  ??3SliderDescription@UI@@SAXPEAX@Z

    0x180049a30  ??4SliderDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801575e0  ??_7SliderDescription@UI@@6B@

    0x1800498f0  ?Construct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800498c0  ?CreateInPlaceObject@SliderDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d7630  ?CreateInstance@SliderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180049840  ?CreateNewObject@SliderDescription@UI@@SAPEAVBase@Object@@XZ

    0x180049900  ?Destruct@SliderDescription@UI@@UEAAXXZ

    0x1800d79f0  ?GetFileVersion@SliderDescription@UI@@UEBAHXZ

    0x1800498d0  ?Reconstruct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d7890  ?Serialize@SliderDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180049730  ?Type@SliderDescription@UI@@UEBAIXZ

    0x180049740  ?TypeId@SliderDescription@UI@@SAIXZ

    0x180049750  ?TypeName@SliderDescription@UI@@UEBAPEBDXZ

    0x180049720  ?_ParentTypeName@SliderDescription@UI@@CAPEBDXZ

    0x180049710  ?_TypeName@SliderDescription@UI@@CAPEBDXZ

    0x1801fa318  ?_typeInfo@SliderDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::SliderElement   exports=27 vtable=0x180157580

    0x180049f00  ??0SliderElement@UI@@QEAA@AEBV01@@Z

    0x1800d7570  ??0SliderElement@UI@@QEAA@XZ

    0x180049ee0  ??1SliderElement@UI@@UEAA@XZ

    0x180049d30  ??2SliderElement@UI@@SAPEAX_K@Z

    0x180049da0  ??2SliderElement@UI@@SAPEAX_KPEAX@Z

    0x180049db0  ??3SliderElement@UI@@SAXPEAX@Z

    0x18004a030  ??4SliderElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157580  ??_7SliderElement@UI@@6B@

    0x1800d6fc0  ?ComputeLayout@SliderElement@UI@@UEAAXHHHH@Z

    0x1800d7380  ?ComputeSize@SliderElement@UI@@UEAAXXZ

    0x180049e90  ?CreateInPlaceObject@SliderElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180049e10  ?CreateNewObject@SliderElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d6d40  ?Draw@SliderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180049ed0  ?GetCurrentValue@SliderElement@UI@@QEBAMXZ

    0x1800d7270  ?GetSliderBounds@SliderElement@UI@@AEBAXAEAH000@Z

    0x180049ec0  ?SetCurrentValue@SliderElement@UI@@QEAAXM@Z

    0x180049eb0  ?SetMaximumValue@SliderElement@UI@@QEAAXM@Z

    0x180049ea0  ?SetMinimumValue@SliderElement@UI@@QEAAXM@Z

    0x1800d6ca0  ?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z

    0x180049d00  ?Type@SliderElement@UI@@UEBAIXZ

    0x180049d10  ?TypeId@SliderElement@UI@@SAIXZ

    0x180049d20  ?TypeName@SliderElement@UI@@UEBAPEBDXZ

    0x1800d67e0  ?Update@SliderElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x1800d6c20  ?UpdateNumber@SliderElement@UI@@AEAAXXZ

    0x180049cf0  ?_ParentTypeName@SliderElement@UI@@CAPEBDXZ

    0x180049ce0  ?_TypeName@SliderElement@UI@@CAPEBDXZ

    0x1801fa350  ?_typeInfo@SliderElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::SpinnerDescription   exports=22 vtable=0x1801576c0

    0x18004af20  ??0SpinnerDescription@UI@@QEAA@AEBV01@@Z

    0x1800d89c0  ??0SpinnerDescription@UI@@QEAA@XZ

    0x1800d8950  ??1SpinnerDescription@UI@@UEAA@XZ

    0x18004ad70  ??2SpinnerDescription@UI@@SAPEAX_K@Z

    0x18004ade0  ??2SpinnerDescription@UI@@SAPEAX_KPEAX@Z

    0x18004adf0  ??3SpinnerDescription@UI@@SAXPEAX@Z

    0x18004af80  ??4SpinnerDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801576c0  ??_7SpinnerDescription@UI@@6B@

    0x18004af00  ?Construct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18004aed0  ?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800d86b0  ?CreateInstance@SpinnerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18004ae50  ?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ

    0x18004af10  ?Destruct@SpinnerDescription@UI@@UEAAXXZ

    0x1800d8940  ?GetFileVersion@SpinnerDescription@UI@@UEBAHXZ

    0x18004aee0  ?Reconstruct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d8880  ?Serialize@SpinnerDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18004ad40  ?Type@SpinnerDescription@UI@@UEBAIXZ

    0x18004ad50  ?TypeId@SpinnerDescription@UI@@SAIXZ

    0x18004ad60  ?TypeName@SpinnerDescription@UI@@UEBAPEBDXZ

    0x18004ad30  ?_ParentTypeName@SpinnerDescription@UI@@CAPEBDXZ

    0x18004ad20  ?_TypeName@SpinnerDescription@UI@@CAPEBDXZ

    0x1801fa388  ?_typeInfo@SpinnerDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::SpinnerElement   exports=28 vtable=0x180157660

    0x18004b2e0  ??0SpinnerElement@UI@@QEAA@AEBV01@@Z

    0x1800d8670  ??0SpinnerElement@UI@@QEAA@XZ

    0x1800d8640  ??1SpinnerElement@UI@@UEAA@XZ

    0x18004b120  ??2SpinnerElement@UI@@SAPEAX_K@Z

    0x18004b190  ??2SpinnerElement@UI@@SAPEAX_KPEAX@Z

    0x18004b1a0  ??3SpinnerElement@UI@@SAXPEAX@Z

    0x18004b330  ??4SpinnerElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157660  ??_7SpinnerElement@UI@@6B@

    0x1800d80c0  ?ComputeLayout@SpinnerElement@UI@@UEAAXHHHH@Z

    0x1800d83b0  ?ComputeSize@SpinnerElement@UI@@UEAAXXZ

    0x18004b290  ?CreateInPlaceObject@SpinnerElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18004b200  ?CreateNewObject@SpinnerElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d8030  ?Draw@SpinnerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800d7cb0  ?GetMaxValue@SpinnerElement@UI@@QEBAHXZ

    0x1800d7c70  ?GetMinValue@SpinnerElement@UI@@QEBAHXZ

    0x1800d7cf0  ?GetValue@SpinnerElement@UI@@QEBAHXZ

    0x1800d7c30  ?IsActive@SpinnerElement@UI@@QEBA_NXZ

    0x1800d7e10  ?SetElementId@SpinnerElement@UI@@QEAAXH@Z

    0x1800d7d30  ?SetIncrement@SpinnerElement@UI@@QEAAXH@Z

    0x1800d7d40  ?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z

    0x1800d7dc0  ?SetValue@SpinnerElement@UI@@QEAAXH@Z

    0x18004b0f0  ?Type@SpinnerElement@UI@@UEBAIXZ

    0x18004b100  ?TypeId@SpinnerElement@UI@@SAIXZ

    0x18004b110  ?TypeName@SpinnerElement@UI@@UEBAPEBDXZ

    0x1800d7e70  ?Update@SpinnerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x18004b0e0  ?_ParentTypeName@SpinnerElement@UI@@CAPEBDXZ

    0x18004b0d0  ?_TypeName@SpinnerElement@UI@@CAPEBDXZ

    0x1801fa3c0  ?_typeInfo@SpinnerElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::Sprite   exports=12 (no vtable export)

    0x18011b9c0  ??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z

    0x18011ba30  ??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z

    0x18011ba10  ??0Sprite@UI@@QEAA@XZ

    0x180030cb0  ??4Sprite@UI@@QEAAAEAV01@AEBV01@@Z

    0x180030c60  ??8Sprite@UI@@QEBA_NAEBV01@@Z

    0x180030c80  ??9Sprite@UI@@QEBA_NAEBV01@@Z

    0x180030ca0  ?GetSpriteSheet@Sprite@UI@@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ

    0x180030c10  ?Height@Sprite@UI@@QEBAHXZ

    0x180030c30  ?Index@Sprite@UI@@QEBAHXZ

    0x180030c40  ?IsValid@Sprite@UI@@QEBA_NXZ

    0x18011b980  ?Serialize@Sprite@UI@@QEAAXAEAVSerializer@IO@@@Z

    0x180030bf0  ?Width@Sprite@UI@@QEBAHXZ



##### UI::SpriteCapture   exports=28 vtable=0x1801566a0

    0x18003cb50  ??0SpriteCapture@UI@@QEAA@AEBV01@@Z

    0x1800d9140  ??0SpriteCapture@UI@@QEAA@XZ

    0x1800d91b0  ??1SpriteCapture@UI@@UEAA@XZ

    0x18003c820  ??2SpriteCapture@UI@@SAPEAX_K@Z

    0x18003c890  ??2SpriteCapture@UI@@SAPEAX_KPEAX@Z

    0x18003c8a0  ??3SpriteCapture@UI@@SAXPEAX@Z

    0x18003cc20  ??4SpriteCapture@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801566a0  ??_7SpriteCapture@UI@@6B@

    0x1800d8ef0  ?AddScissor@SpriteCapture@UI@@QEAAXHHHH_N@Z

    0x1800d8b20  ?AddSprite@SpriteCapture@UI@@QEAAXAEBVSprite@2@HHHHMI@Z

    0x1800d9010  ?BeginAddingSprites@SpriteCapture@UI@@QEAAXXZ

    0x18003cac0  ?Construct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ

    0x18003c9c0  ?CreateInPlaceObject@SpriteCapture@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18003c900  ?CreateNewObject@SpriteCapture@UI@@SAPEAVBase@Object@@XZ

    0x18003cb40  ?Destruct@SpriteCapture@UI@@UEAAXXZ

    0x1800d9230  ?Draw@SpriteCapture@UI@@QEBAXAEAVSpriteContext@2@HH@Z

    0x1800d8a60  ?EndAddingSprites@SpriteCapture@UI@@QEAAXXZ

    0x1800d9520  ?GetFileVersion@SpriteCapture@UI@@UEBAHXZ

    0x1800d9210  ?GetHeight@SpriteCapture@UI@@QEBAHXZ

    0x1800d9220  ?GetWidth@SpriteCapture@UI@@QEBAHXZ

    0x18003ca40  ?Reconstruct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800d9530  ?Serialize@SpriteCapture@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18003c7f0  ?Type@SpriteCapture@UI@@UEBAIXZ

    0x18003c800  ?TypeId@SpriteCapture@UI@@SAIXZ

    0x18003c810  ?TypeName@SpriteCapture@UI@@UEBAPEBDXZ

    0x18003c7e0  ?_ParentTypeName@SpriteCapture@UI@@CAPEBDXZ

    0x18003c7d0  ?_TypeName@SpriteCapture@UI@@CAPEBDXZ

    0x1801fa3f8  ?_typeInfo@SpriteCapture@UI@@0VBaseTypeInfo@Object@@A



##### UI::SpriteContext   exports=23 (no vtable export)

    0x18011d1e0  ??0SpriteContext@UI@@QEAA@AEAVContext@Video@@@Z

    0x1800321c0  ??0SpriteContext@UI@@QEAA@AEBV01@@Z

    0x18011d250  ??1SpriteContext@UI@@QEAA@XZ

    0x18011ccb0  ?AddFont@SpriteContext@UI@@QEAAXV?$ExternalPtr@VFont@UI@@@Object@@AEBVString@System@@_NHW4Justify@Font@2@HHHI@Z

    0x18011cbe0  ?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z

    0x18011d3c0  ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z

    0x18011cb10  ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHIM@Z

    0x18011cf00  ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHI@Z

    0x18011c780  ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHIM@Z

    0x18011c890  ?AddSpriteDraw@SpriteContext@UI@@AEAAXAEBUSpriteDraw@12@@Z

    0x18011d2b0  ?Begin@SpriteContext@UI@@QEAAXMPEAVSpriteCapture@2@PEAVViewport@Video@@@Z

    0x18011c9d0  ?BeginDeferredFonts@SpriteContext@UI@@QEAAXXZ

    0x18011d490  ?End@SpriteContext@UI@@QEAAXXZ

    0x18011c9e0  ?EndDeferredFonts@SpriteContext@UI@@QEAAXXZ

    0x18011ba40  ?Flush@SpriteContext@UI@@AEAAXXZ

    0x18011c6a0  ?FlushDeferredFonts@SpriteContext@UI@@AEAAXXZ

    0x18011ca30  ?GetScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ

    0x18011ca00  ?GetScreenScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ

    0x1800321b0  ?GetVideoContext@SpriteContext@UI@@QEAAAEAVContext@Video@@XZ

    0x18011d050  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z

    0x18011d010  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z

    0x18011caa0  ?SetScreenScissor@SpriteContext@UI@@QEAAXAEBVViewport@Video@@@Z

    0x18011c740  ?SetSpriteSheet@SpriteContext@UI@@AEAAXV?$ExternalPtr@VSpriteSheet@UI@@@Object@@@Z



##### UI::SpriteSheet   exports=34 vtable=0x180154590

    0x1800035d0  ??0SpriteSheet@UI@@QEAA@AEBV01@@Z

    0x18011d600  ??0SpriteSheet@UI@@QEAA@XZ

    0x18011d670  ??1SpriteSheet@UI@@UEAA@XZ

    0x180002e60  ??2SpriteSheet@UI@@SAPEAX_K@Z

    0x180002ed0  ??2SpriteSheet@UI@@SAPEAX_KPEAX@Z

    0x180002ee0  ??3SpriteSheet@UI@@SAXPEAX@Z

    0x1800036b0  ??4SpriteSheet@UI@@QEAAAEAV01@AEBV01@@Z

    0x180154590  ??_7SpriteSheet@UI@@6B@

    0x180003390  ?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z

    0x18011d720  ?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z

    0x180003100  ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ

    0x180003000  ?CreateInPlaceObject@SpriteSheet@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180002f40  ?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ

    0x180003180  ?Destruct@SpriteSheet@UI@@UEAAXXZ

    0x18011d710  ?GetFileVersion@SpriteSheet@UI@@UEBAHXZ

    0x180003320  ?GetMaterialInstance@SpriteSheet@UI@@QEBA?AV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@XZ

    0x180003190  ?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ

    0x1800031f0  ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z

    0x1800032d0  ?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z

    0x180003330  ?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z

    0x1800031c0  ?Height@SpriteSheet@UI@@QEBAHH@Z

    0x1800031e0  ?IsValid@SpriteSheet@UI@@QEBA_NH@Z

    0x180003080  ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ

    0x18011d730  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z

    0x1800034f0  ?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z

    0x1800034a0  ?SetMaterial@SpriteSheet@UI@@QEAAXV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Z

    0x180003460  ?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z

    0x180002e30  ?Type@SpriteSheet@UI@@UEBAIXZ

    0x180002e40  ?TypeId@SpriteSheet@UI@@SAIXZ

    0x180002e50  ?TypeName@SpriteSheet@UI@@UEBAPEBDXZ

    0x1800031a0  ?Width@SpriteSheet@UI@@QEBAHH@Z

    0x180002e20  ?_ParentTypeName@SpriteSheet@UI@@CAPEBDXZ

    0x180002e10  ?_TypeName@SpriteSheet@UI@@CAPEBDXZ

    0x1801fadd0  ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A



##### UI::String   exports=8 (no vtable export)

    0x18011da60  ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z

    0x18011da20  ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z

    0x18011da40  ??0String@UI@@QEAA@XZ

    0x180035090  ??4String@UI@@QEAAAEAV01@AEBV01@@Z

    0x180035020  ??BString@UI@@QEBA?AV0System@@XZ

    0x180035080  ?GetStringTable@String@UI@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ

    0x180035010  ?Index@String@UI@@QEBAHXZ

    0x180035060  ?IsValid@String@UI@@QEBA_NXZ



##### UI::StringTable   exports=25 vtable=0x180154630

    0x180002620  ??0StringTable@UI@@QEAA@AEBV01@@Z

    0x18011d890  ??0StringTable@UI@@QEAA@XZ

    0x18011d8f0  ??1StringTable@UI@@UEAA@XZ

    0x1800021a0  ??2StringTable@UI@@SAPEAX_K@Z

    0x180002210  ??2StringTable@UI@@SAPEAX_KPEAX@Z

    0x180002220  ??3StringTable@UI@@SAXPEAX@Z

    0x1800026b0  ??4StringTable@UI@@QEAAAEAV01@AEBV01@@Z

    0x180154630  ??_7StringTable@UI@@6B@

    0x180002400  ?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ

    0x180002330  ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180002280  ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ

    0x180002460  ?Destruct@StringTable@UI@@UEAAXXZ

    0x180002610  ?GetCount@StringTable@UI@@QEBAHXZ

    0x18011d930  ?GetFileVersion@StringTable@UI@@UEBAHXZ

    0x180002470  ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z

    0x1800025b0  ?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z

    0x180002550  ?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z

    0x180002390  ?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ

    0x18011d940  ?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180002170  ?Type@StringTable@UI@@UEBAIXZ

    0x180002180  ?TypeId@StringTable@UI@@SAIXZ

    0x180002190  ?TypeName@StringTable@UI@@UEBAPEBDXZ

    0x180002160  ?_ParentTypeName@StringTable@UI@@CAPEBDXZ

    0x180002150  ?_TypeName@StringTable@UI@@CAPEBDXZ

    0x1801fae08  ?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A



##### UI::TabDescription   exports=22 vtable=0x1801577f0

    0x18004c3b0  ??0TabDescription@UI@@QEAA@AEBV01@@Z

    0x1800daa00  ??0TabDescription@UI@@QEAA@XZ

    0x1800da8e0  ??1TabDescription@UI@@UEAA@XZ

    0x18004c200  ??2TabDescription@UI@@SAPEAX_K@Z

    0x18004c270  ??2TabDescription@UI@@SAPEAX_KPEAX@Z

    0x18004c280  ??3TabDescription@UI@@SAXPEAX@Z

    0x18004c4a0  ??4TabDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801577f0  ??_7TabDescription@UI@@6B@

    0x18004c390  ?Construct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18004c360  ?CreateInPlaceObject@TabDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800da410  ?CreateInstance@TabDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18004c2e0  ?CreateNewObject@TabDescription@UI@@SAPEAVBase@Object@@XZ

    0x18004c3a0  ?Destruct@TabDescription@UI@@UEAAXXZ

    0x1800da8d0  ?GetFileVersion@TabDescription@UI@@UEBAHXZ

    0x18004c370  ?Reconstruct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x1800da760  ?Serialize@TabDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x18004c1d0  ?Type@TabDescription@UI@@UEBAIXZ

    0x18004c1e0  ?TypeId@TabDescription@UI@@SAIXZ

    0x18004c1f0  ?TypeName@TabDescription@UI@@UEBAPEBDXZ

    0x18004c1c0  ?_ParentTypeName@TabDescription@UI@@CAPEBDXZ

    0x18004c1b0  ?_TypeName@TabDescription@UI@@CAPEBDXZ

    0x1801fa430  ?_typeInfo@TabDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::TabElement   exports=26 vtable=0x180157790

    0x18004c8b0  ??0TabElement@UI@@QEAA@AEBV01@@Z

    0x1800da390  ??0TabElement@UI@@QEAA@XZ

    0x1800da360  ??1TabElement@UI@@UEAA@XZ

    0x18004c710  ??2TabElement@UI@@SAPEAX_K@Z

    0x18004c780  ??2TabElement@UI@@SAPEAX_KPEAX@Z

    0x18004c790  ??3TabElement@UI@@SAXPEAX@Z

    0x18004c980  ??4TabElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157790  ??_7TabElement@UI@@6B@

    0x1800da130  ?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@0@Z

    0x1800d9b20  ?ComputeLayout@TabElement@UI@@UEAAXHHHH@Z

    0x1800d9ea0  ?ComputeSize@TabElement@UI@@UEAAXXZ

    0x18004c870  ?CreateInPlaceObject@TabElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18004c7f0  ?CreateNewObject@TabElement@UI@@SAPEAVBase@Object@@XZ

    0x1800d98b0  ?Draw@TabElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x18004c880  ?GetActiveTab@TabElement@UI@@QEBAHXZ

    0x1800da120  ?GetButtonIndex@TabElement@UI@@AEBAHH@Z

    0x1800da110  ?GetContainerIndex@TabElement@UI@@AEBAHH@Z

    0x18004c890  ?SetActiveTab@TabElement@UI@@QEAAXH@Z

    0x18004c6e0  ?Type@TabElement@UI@@UEBAIXZ

    0x18004c6f0  ?TypeId@TabElement@UI@@SAIXZ

    0x18004c700  ?TypeName@TabElement@UI@@UEBAPEBDXZ

    0x1800d9810  ?Update@TabElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z

    0x1800d9710  ?UpdateContainers@TabElement@UI@@AEAAXXZ

    0x18004c6d0  ?_ParentTypeName@TabElement@UI@@CAPEBDXZ

    0x18004c6c0  ?_TypeName@TabElement@UI@@CAPEBDXZ

    0x1801fa468  ?_typeInfo@TabElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::TableDescription   exports=22 vtable=0x1801592c0

    0x180079090  ??0TableDescription@UI@@QEAA@AEBV01@@Z

    0x18011e7a0  ??0TableDescription@UI@@QEAA@XZ

    0x18011e6f0  ??1TableDescription@UI@@UEAA@XZ

    0x180078ee0  ??2TableDescription@UI@@SAPEAX_K@Z

    0x180078f50  ??2TableDescription@UI@@SAPEAX_KPEAX@Z

    0x180078f60  ??3TableDescription@UI@@SAXPEAX@Z

    0x180079130  ??4TableDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x1801592c0  ??_7TableDescription@UI@@6B@

    0x180079070  ?Construct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180079040  ?CreateInPlaceObject@TableDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18011e350  ?CreateInstance@TableDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180078fc0  ?CreateNewObject@TableDescription@UI@@SAPEAVBase@Object@@XZ

    0x180079080  ?Destruct@TableDescription@UI@@UEAAXXZ

    0x18011e6e0  ?GetFileVersion@TableDescription@UI@@UEBAHXZ

    0x180079050  ?Reconstruct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18011e590  ?Serialize@TableDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180078eb0  ?Type@TableDescription@UI@@UEBAIXZ

    0x180078ec0  ?TypeId@TableDescription@UI@@SAIXZ

    0x180078ed0  ?TypeName@TableDescription@UI@@UEBAPEBDXZ

    0x180078ea0  ?_ParentTypeName@TableDescription@UI@@CAPEBDXZ

    0x180078e90  ?_TypeName@TableDescription@UI@@CAPEBDXZ

    0x1801fae40  ?_typeInfo@TableDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::TableElement   exports=26 vtable=0x180159260

    0x180079470  ??0TableElement@UI@@QEAA@AEBV01@@Z

    0x18011e2f0  ??0TableElement@UI@@QEAA@XZ

    0x18011e2a0  ??1TableElement@UI@@UEAA@XZ

    0x1800792e0  ??2TableElement@UI@@SAPEAX_K@Z

    0x180079350  ??2TableElement@UI@@SAPEAX_KPEAX@Z

    0x180079360  ??3TableElement@UI@@SAXPEAX@Z

    0x180079520  ??4TableElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180159260  ??_7TableElement@UI@@6B@

    0x18011dd10  ?ComputeLayout@TableElement@UI@@UEAAXHHHH@Z

    0x18011df60  ?ComputeSize@TableElement@UI@@UEAAXXZ

    0x180079440  ?CreateInPlaceObject@TableElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x1800793c0  ?CreateNewObject@TableElement@UI@@SAPEAVBase@Object@@XZ

    0x18011dc20  ?Draw@TableElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x180079460  ?GetColumnCount@TableElement@UI@@QEBAHXZ

    0x18011e1a0  ?GetColumnWidth@TableElement@UI@@QEAAHH@Z

    0x180079450  ?GetRowCount@TableElement@UI@@QEBAHXZ

    0x18011e090  ?GetRowHeight@TableElement@UI@@QEAAHH@Z

    0x18011daa0  ?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@H@Z

    0x18011db50  ?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@HH@Z

    0x18011db90  ?SetSize@TableElement@UI@@QEAAXHH@Z

    0x1800792b0  ?Type@TableElement@UI@@UEBAIXZ

    0x1800792c0  ?TypeId@TableElement@UI@@SAIXZ

    0x1800792d0  ?TypeName@TableElement@UI@@UEBAPEBDXZ

    0x1800792a0  ?_ParentTypeName@TableElement@UI@@CAPEBDXZ

    0x180079290  ?_TypeName@TableElement@UI@@CAPEBDXZ

    0x1801fae78  ?_typeInfo@TableElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::TextDescription   exports=23 vtable=0x180156be0

    0x180041c90  ??0TextDescription@UI@@QEAA@AEBV01@@Z

    0x18011ea20  ??0TextDescription@UI@@QEAA@XZ

    0x18011eae0  ??1TextDescription@UI@@UEAA@XZ

    0x180041ae0  ??2TextDescription@UI@@SAPEAX_K@Z

    0x180041b50  ??2TextDescription@UI@@SAPEAX_KPEAX@Z

    0x180041b60  ??3TextDescription@UI@@SAXPEAX@Z

    0x180041d40  ??4TextDescription@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156be0  ??_7TextDescription@UI@@6B@

    0x180041c70  ?Construct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x180041c40  ?CreateInPlaceObject@TextDescription@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18011ef70  ?CreateInstance@TextDescription@UI@@QEBAXV?$InternalPtr@VTextElement@UI@@@Object@@PEBVDialog@2@@Z

    0x18011f3d0  ?CreateInstance@TextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z

    0x180041bc0  ?CreateNewObject@TextDescription@UI@@SAPEAVBase@Object@@XZ

    0x180041c80  ?Destruct@TextDescription@UI@@UEAAXXZ

    0x18011ee20  ?GetFileVersion@TextDescription@UI@@UEBAHXZ

    0x180041c50  ?Reconstruct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ

    0x18011ee30  ?Serialize@TextDescription@UI@@UEAA_NAEAVStream@IO@@@Z

    0x180041ab0  ?Type@TextDescription@UI@@UEBAIXZ

    0x180041ac0  ?TypeId@TextDescription@UI@@SAIXZ

    0x180041ad0  ?TypeName@TextDescription@UI@@UEBAPEBDXZ

    0x180041aa0  ?_ParentTypeName@TextDescription@UI@@CAPEBDXZ

    0x180041a90  ?_TypeName@TextDescription@UI@@CAPEBDXZ

    0x1801faeb0  ?_typeInfo@TextDescription@UI@@0VBaseTypeInfo@Object@@A



##### UI::TextElement   exports=24 vtable=0x180156b80

    0x1800420d0  ??0TextElement@UI@@QEAA@AEBV01@@Z

    0x18011eb80  ??0TextElement@UI@@QEAA@XZ

    0x18011eda0  ??1TextElement@UI@@UEAA@XZ

    0x180041f00  ??2TextElement@UI@@SAPEAX_K@Z

    0x180041f70  ??2TextElement@UI@@SAPEAX_KPEAX@Z

    0x180041f80  ??3TextElement@UI@@SAXPEAX@Z

    0x1800421c0  ??4TextElement@UI@@QEAAAEAV01@AEBV01@@Z

    0x180156b80  ??_7TextElement@UI@@6B@

    0x18011ec10  ?CacheText@TextElement@UI@@AEAAXXZ

    0x18011f120  ?ComputeLayout@TextElement@UI@@UEAAXHHHH@Z

    0x18011f060  ?ComputeSize@TextElement@UI@@UEAAXXZ

    0x180042060  ?CreateInPlaceObject@TextElement@UI@@SAPEAVBase@Object@@PEAX@Z

    0x180041fe0  ?CreateNewObject@TextElement@UI@@SAPEAVBase@Object@@XZ

    0x18011f280  ?Draw@TextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z

    0x1800420b0  ?GetText@TextElement@UI@@QEBA?AVString@2@XZ

    0x18011e9b0  ?SetParameter@TextElement@UI@@QEAAXHAEBVString@System@@@Z

    0x18011e8e0  ?SetParameterCount@TextElement@UI@@AEAAXXZ

    0x180042070  ?SetText@TextElement@UI@@QEAAXVString@2@@Z

    0x180041ed0  ?Type@TextElement@UI@@UEBAIXZ

    0x180041ee0  ?TypeId@TextElement@UI@@SAIXZ

    0x180041ef0  ?TypeName@TextElement@UI@@UEBAPEBDXZ

    0x180041ec0  ?_ParentTypeName@TextElement@UI@@CAPEBDXZ

    0x180041eb0  ?_TypeName@TextElement@UI@@CAPEBDXZ

    0x1801faee8  ?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A



##### UI::ToolTipDialog   exports=19 vtable=0x180157880

    0x18004d180  ??0ToolTipDialog@UI@@QEAA@AEBV01@@Z

    0x1800daca0  ??0ToolTipDialog@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@5@@Z

    0x1800dad00  ??0ToolTipDialog@UI@@QEAA@XZ

    0x1800dac70  ??1ToolTipDialog@UI@@UEAA@XZ

    0x18004cfd0  ??2ToolTipDialog@UI@@SAPEAX_K@Z

    0x18004d040  ??2ToolTipDialog@UI@@SAPEAX_KPEAX@Z

    0x18004d050  ??3ToolTipDialog@UI@@SAXPEAX@Z

    0x18004d1e0  ??4ToolTipDialog@UI@@QEAAAEAV01@AEBV01@@Z

    0x180157880  ??_7ToolTipDialog@UI@@6B@

    0x18004d140  ?CreateInPlaceObject@ToolTipDialog@UI@@SAPEAVBase@Object@@PEAX@Z

    0x18004d0b0  ?CreateNewObject@ToolTipDialog@UI@@SAPEAVBase@Object@@XZ

    0x1800dac50  ?SetPosition@ToolTipDialog@UI@@QEAAXHH@Z

    0x1800dab00  ?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z

    0x18004cfa0  ?Type@ToolTipDialog@UI@@UEBAIXZ

    0x18004cfb0  ?TypeId@ToolTipDialog@UI@@SAIXZ

    0x18004cfc0  ?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ

    0x18004cf90  ?_ParentTypeName@ToolTipDialog@UI@@CAPEBDXZ

    0x18004cf80  ?_TypeName@ToolTipDialog@UI@@CAPEBDXZ

    0x1801fa4a0  ?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A



##### UI::UserInterface   exports=52 (no vtable export)

    0x180031170  ??0UserInterface@UI@@QEAA@AEBV01@@Z

    0x180120de0  ??0UserInterface@UI@@QEAA@XZ

    0x180120b90  ??1UserInterface@UI@@QEAA@XZ

    0x180120ee0  ?AddDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x18011f5f0  ?BeginDragDrop@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@4@HHIPEAX@Z

    0x18011f4c0  ?BeginSpriteCapture@UserInterface@UI@@QEAAXPEAVSpriteCapture@2@VViewport@Video@@@Z

    0x180120860  ?BeginToolTip@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@4@HH@Z

    0x180120440  ?BringDialogToFront@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x18011f580  ?CancelDragDrop@UserInterface@UI@@QEAAXXZ

    0x18011f790  ?CleanRemovedDialogs@UserInterface@UI@@AEAAXXZ

    0x180120f20  ?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x18011fbb0  ?DialogContains@UserInterface@UI@@QEBA_NHH@Z

    0x180031030  ?DragDropData@UserInterface@UI@@QEBAPEAXXZ

    0x180031040  ?DragDropId@UserInterface@UI@@QEBAIXZ

    0x180121890  ?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z

    0x18011f780  ?DrawDragDrop@UserInterface@UI@@QEBAXAEAVSpriteContext@2@@Z

    0x18011f6f0  ?DrawFade@UserInterface@UI@@QEBAXAEAVSpriteContext@2@HHHH@Z

    0x18011f640  ?DrawMouse@UserInterface@UI@@QEBAXAEAVSpriteContext@2@@Z

    0x1801203f0  ?DrawStandAloneUI@UserInterface@UI@@CAXAEAVContext@Video@@PEAX@Z

    0x180122580  ?EnableCursor@UserInterface@UI@@QEAAX_N@Z

    0x180120af0  ?EndDragDrop@UserInterface@UI@@QEAAXXZ

    0x18011f4a0  ?EndSpriteCapture@UserInterface@UI@@QEAAXXZ

    0x180120aa0  ?EndToolTip@UserInterface@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z

    0x180031160  ?GetAddDialogCount@UserInterface@UI@@QEBAHXZ

    0x180031000  ?GetClipRegion@UserInterface@UI@@QEBAAEBUClipRegion@12@XZ

    0x18011fb00  ?GetFirstContainedDialog@UserInterface@UI@@QEBA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z

    0x180031150  ?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@@Collection@@XZ

    0x18011fa40  ?GetMouseCursorIndex@UserInterface@UI@@QEBAHHH@Z

    0x180120410  ?IsDialogOpen@UserInterface@UI@@QEBA_NV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x180031010  ?IsFading@UserInterface@UI@@QEBA_NXZ

    0x180120830  ?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z

    0x18011fa10  ?PausesGame@UserInterface@UI@@QEBA_NXZ

    0x1801223d0  ?Play@UserInterface@UI@@QEAAXH@Z

    0x1800310e0  ?PostEvent@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@4@HHHHHPEAX@Z

    0x1801204b0  ?SaveAllSettings@UserInterface@UI@@QEAAXXZ

    0x180031050  ?SetAudioScene@UserInterface@UI@@QEAAXPEAVScene@Audio@@@Z

    0x1801224d0  ?SetClipRegion@UserInterface@UI@@QEAA?AUClipRegion@12@AEBU312@@Z

    0x180122470  ?SetClipRegion@UserInterface@UI@@QEAA?AUClipRegion@12@HHHH@Z

    0x1801225f0  ?SetCursor@UserInterface@UI@@QEAAXV?$ExternalPtr@VCursor@UI@@@Object@@@Z

    0x18011f500  ?SetFullscreenFade@UserInterface@UI@@QEAAX_NI0@Z

    0x18011f560  ?SetFullscreenFadeSprite@UserInterface@UI@@QEAAXVSprite@2@@Z

    0x1800310d0  ?SetSound@UserInterface@UI@@QEAAXHV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z

    0x180031070  ?SetUIScale@UserInterface@UI@@QEAAXH@Z

    0x18011fd10  ?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z

    0x180031090  ?UIScaled@UserInterface@UI@@QEBAHH@Z

    0x1800310b0  ?UIUnscaled@UserInterface@UI@@QEBAHH@Z

    0x1801213c0  ?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z

    0x18011fc60  ?UpdateEvents@UserInterface@UI@@QEAAXXZ

    0x18011f990  ?UpdateFade@UserInterface@UI@@QEAAXAEBVTime@System@@@Z

    0x180120310  ?UpdateStandAloneUI@UserInterface@UI@@CAXAEBVTime@System@@PEAX@Z

    0x180120a70  ?UpdateToolTip@UserInterface@UI@@QEAAXHH@Z

    0x180031060  ?WasMouseCaptured@UserInterface@UI@@QEBA_NXZ



##### UI::VSpriteSheet::QEBA?AV?$ExternalPtr::1::Achievement   exports=1 (no vtable export)

    0x180001390  ?GetSpriteSheet@Achievement@1@QEBA?AV?$ExternalPtr@VSpriteSheet@UI@@@Object@@XZ



##### UI::VStringTable::QEBA?AV?$ExternalPtr::1::Achievement   exports=1 (no vtable export)

    0x180001380  ?GetStringTable@Achievement@1@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ



=== SCRIPT COMPLETED SUCCESSFULLY ===

@@@@@@ SECTION 7 -- vtable stamps in every ??0/??1 (condensed). Every LEA of a ??_7 is followed by a zero-offset store; only the chain is listed here.
### UI::AnimatedImageDescription  ??0AnimatedImageDescription@UI@@QEAA@AEBV01@@Z @0x180036890
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7AnimatedImageDescription@UI@@6B@
    stamp ??_7AnimatedImageDescription@UI@@6B@
### UI::AnimatedImageDescription  ??1AnimatedImageDescription@UI@@UEAA@XZ @0x1800cb6b0
    stamp ??_7AnimatedImageDescription@UI@@6B@
    stamp ??_7AnimatedImageDescription@UI@@6B@
### UI::AnimatedImageDescription  ??0AnimatedImageDescription@UI@@QEAA@XZ @0x1800cb6f0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7AnimatedImageDescription@UI@@6B@
    stamp ??_7AnimatedImageDescription@UI@@6B@
### UI::AnimatedImageElement  ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z @0x180036c30
    CALL ??0ImageElement@UI@@QEAA@AEBV01@@Z
    stamp ??_7AnimatedImageElement@UI@@6B@
    stamp ??_7AnimatedImageElement@UI@@6B@
### UI::AnimatedImageElement  ??1AnimatedImageElement@UI@@UEAA@XZ @0x1800cb3a0
    stamp ??_7AnimatedImageElement@UI@@6B@
    stamp ??_7AnimatedImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
### UI::AnimatedImageElement  ??0AnimatedImageElement@UI@@QEAA@XZ @0x1800cb3f0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7AnimatedImageElement@UI@@6B@
    stamp ??_7AnimatedImageElement@UI@@6B@
### UI::BorderDescription  ??0BorderDescription@UI@@QEAA@AEBV01@@Z @0x180040ba0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7BorderDescription@UI@@6B@
    stamp ??_7BorderDescription@UI@@6B@
### UI::BorderDescription  ??0BorderDescription@UI@@QEAA@XZ @0x18010fa50
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7BorderDescription@UI@@6B@
    stamp ??_7BorderDescription@UI@@6B@
### UI::BorderDescription  ??1BorderDescription@UI@@UEAA@XZ @0x18010fb20
    stamp ??_7BorderDescription@UI@@6B@
    stamp ??_7BorderDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::BorderElement  ??1BorderElement@UI@@UEAA@XZ @0x180041230
### UI::BorderElement  ??0BorderElement@UI@@QEAA@AEBV01@@Z @0x180041250
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7BorderElement@UI@@6B@
    stamp ??_7BorderElement@UI@@6B@
### UI::BorderElement  ??0BorderElement@UI@@QEAA@XZ @0x18010fd40
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7BorderElement@UI@@6B@
    stamp ??_7BorderElement@UI@@6B@
### UI::ButtonDescription  ??0ButtonDescription@UI@@QEAA@AEBV01@@Z @0x180047ff0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
### UI::ButtonDescription  ??0ButtonDescription@UI@@QEAA@XZ @0x180110b10
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
### UI::ButtonDescription  ??1ButtonDescription@UI@@UEAA@XZ @0x180110bd0
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ButtonElement  ??0ButtonElement@UI@@QEAA@AEBV01@@Z @0x180048460
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
### UI::ButtonElement  ??0ButtonElement@UI@@QEAA@XZ @0x180110c40
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
### UI::ButtonElement  ??1ButtonElement@UI@@UEAA@XZ @0x180110c90
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
### UI::CharacterSet  ??0CharacterSet@UI@@QEAA@AEBV01@@Z @0x180037720
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7CharacterSet@UI@@6B@
    stamp ??_7CharacterSet@UI@@6B@
### UI::CharacterSet  ??1CharacterSet@UI@@UEAA@XZ @0x1800cb840
    stamp ??_7CharacterSet@UI@@6B@
    stamp ??_7CharacterSet@UI@@6B@
### UI::CharacterSet  ??0CharacterSet@UI@@QEAA@XZ @0x1800cb8a0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7CharacterSet@UI@@6B@
    stamp ??_7CharacterSet@UI@@6B@
### UI::CheckDescription  ??0CheckDescription@UI@@QEAA@AEBV01@@Z @0x180048900
    CALL ??0ButtonDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7CheckDescription@UI@@6B@
    stamp ??_7CheckDescription@UI@@6B@
### UI::CheckDescription  ??0CheckDescription@UI@@QEAA@XZ @0x1801115e0
    CALL ??0ButtonDescription@UI@@QEAA@XZ
    stamp ??_7CheckDescription@UI@@6B@
    stamp ??_7CheckDescription@UI@@6B@
### UI::CheckDescription  ??1CheckDescription@UI@@UEAA@XZ @0x180111620
    stamp ??_7CheckDescription@UI@@6B@
    stamp ??_7CheckDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::CheckElement  ??0CheckElement@UI@@QEAA@AEBV01@@Z @0x180048ca0
    CALL ??0ButtonElement@UI@@QEAA@AEBV01@@Z
    stamp ??_7CheckElement@UI@@6B@
    stamp ??_7CheckElement@UI@@6B@
### UI::CheckElement  ??0CheckElement@UI@@QEAA@XZ @0x1801116a0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7CheckElement@UI@@6B@
    stamp ??_7CheckElement@UI@@6B@
### UI::CheckElement  ??1CheckElement@UI@@UEAA@XZ @0x180111700
    stamp ??_7CheckElement@UI@@6B@
    stamp ??_7CheckElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
### UI::ClipRegionDescription  ??0ClipRegionDescription@UI@@QEAA@AEBV01@@Z @0x180037da0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ClipRegionDescription@UI@@6B@
    stamp ??_7ClipRegionDescription@UI@@6B@
### UI::ClipRegionDescription  ??1ClipRegionDescription@UI@@UEAA@XZ @0x1800cc0d0
    stamp ??_7ClipRegionDescription@UI@@6B@
    stamp ??_7ClipRegionDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ClipRegionDescription  ??0ClipRegionDescription@UI@@QEAA@XZ @0x1800cc140
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ClipRegionDescription@UI@@6B@
    stamp ??_7ClipRegionDescription@UI@@6B@
### UI::ClipRegionElement  ??0ClipRegionElement@UI@@QEAA@AEBV01@@Z @0x180038220
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ClipRegionElement@UI@@6B@
    stamp ??_7ClipRegionElement@UI@@6B@
### UI::ClipRegionElement  ??1ClipRegionElement@UI@@UEAA@XZ @0x1800cbe30
    stamp ??_7ClipRegionElement@UI@@6B@
    stamp ??_7ClipRegionElement@UI@@6B@
### UI::ClipRegionElement  ??0ClipRegionElement@UI@@QEAA@XZ @0x1800cbe60
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ClipRegionElement@UI@@6B@
    stamp ??_7ClipRegionElement@UI@@6B@
### UI::ComboDescription  ??0ComboDescription@UI@@QEAA@AEBV01@@Z @0x180074d80
    CALL ??0ButtonDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ComboDescription@UI@@6B@
    stamp ??_7ComboDescription@UI@@6B@
### UI::ComboDescription  ??0ComboDescription@UI@@QEAA@XZ @0x180112320
    CALL ??0ButtonDescription@UI@@QEAA@XZ
    stamp ??_7ComboDescription@UI@@6B@
    stamp ??_7ComboDescription@UI@@6B@
### UI::ComboDescription  ??1ComboDescription@UI@@UEAA@XZ @0x180112370
    stamp ??_7ComboDescription@UI@@6B@
    stamp ??_7ComboDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ButtonDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ComboDialog  ??0ComboDialog@UI@@QEAA@AEBV01@@Z @0x180074940
    CALL ??0DialogInstance@UI@@QEAA@AEBV01@@Z
    stamp ??_7ComboDialog@UI@@6B@
    stamp ??_7ComboDialog@UI@@6B@
### UI::ComboDialog  ??1ComboDialog@UI@@UEAA@XZ @0x180111cd0
    stamp ??_7ComboDialog@UI@@6B@
    stamp ??_7ComboDialog@UI@@6B@
### UI::ComboDialog  ??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@V?$InternalPtr@VComboElement@UI@@@4@@Z @0x180111d00
    CALL ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@PEAVBase@4@PEAXH@Z
    stamp ??_7ComboDialog@UI@@6B@
    stamp ??_7ComboDialog@UI@@6B@
### UI::ComboDialog  ??0ComboDialog@UI@@QEAA@XZ @0x180112130
    CALL ??0DialogInstance@UI@@QEAA@XZ
    stamp ??_7ComboDialog@UI@@6B@
    stamp ??_7ComboDialog@UI@@6B@
### UI::ComboElement  ??0ComboElement@UI@@QEAA@AEBV01@@Z @0x1800751e0
    CALL ??0ButtonElement@UI@@QEAA@AEBV01@@Z
    stamp ??_7ComboElement@UI@@6B@
    stamp ??_7ComboElement@UI@@6B@
### UI::ComboElement  ??0ComboElement@UI@@QEAA@XZ @0x1801123f0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ComboElement@UI@@6B@
    stamp ??_7ComboElement@UI@@6B@
### UI::ComboElement  ??1ComboElement@UI@@UEAA@XZ @0x180112490
    stamp ??_7ComboElement@UI@@6B@
    stamp ??_7ComboElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
    stamp ??_7ButtonElement@UI@@6B@
### UI::ContainerDescription  ??0ContainerDescription@UI@@QEAA@AEBV01@@Z @0x180039a90
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ContainerDescription@UI@@6B@
    stamp ??_7ContainerDescription@UI@@6B@
### UI::ContainerDescription  ??1ContainerDescription@UI@@UEAA@XZ @0x1800ccff0
    stamp ??_7ContainerDescription@UI@@6B@
    stamp ??_7ContainerDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ContainerDescription  ??0ContainerDescription@UI@@QEAA@XZ @0x1800cd060
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ContainerDescription@UI@@6B@
    stamp ??_7ContainerDescription@UI@@6B@
### UI::ContainerElement  ??0ContainerElement@UI@@QEAA@AEBV01@@Z @0x180039f00
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ContainerElement@UI@@6B@
    stamp ??_7ContainerElement@UI@@6B@
### UI::ContainerElement  ??1ContainerElement@UI@@UEAA@XZ @0x1800ccb10
    stamp ??_7ContainerElement@UI@@6B@
    stamp ??_7ContainerElement@UI@@6B@
### UI::ContainerElement  ??0ContainerElement@UI@@QEAA@XZ @0x1800ccb40
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ContainerElement@UI@@6B@
    stamp ??_7ContainerElement@UI@@6B@
### UI::Cursor  ??0Cursor@UI@@QEAA@AEBV01@@Z @0x18003b280
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7Cursor@UI@@6B@
    stamp ??_7Cursor@UI@@6B@
### UI::Cursor  ??1Cursor@UI@@UEAA@XZ @0x1800cd1c0
    stamp ??_7Cursor@UI@@6B@
    stamp ??_7Cursor@UI@@6B@
### UI::Cursor  ??0Cursor@UI@@QEAA@XZ @0x1800cd1f0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7Cursor@UI@@6B@
    stamp ??_7Cursor@UI@@6B@
### UI::Dialog  ??0Dialog@UI@@QEAA@AEBV01@@Z @0x180038920
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7Dialog@UI@@6B@
    stamp ??_7Dialog@UI@@6B@
### UI::Dialog  ??0Dialog@UI@@QEAA@XZ @0x180112db0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7Dialog@UI@@6B@
    stamp ??_7Dialog@UI@@6B@
### UI::Dialog  ??1Dialog@UI@@UEAA@XZ @0x180112e40
    stamp ??_7Dialog@UI@@6B@
    stamp ??_7Dialog@UI@@6B@
### UI::DialogController  ??0DialogController@UI@@QEAA@AEBV01@@Z @0x18003b710
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7DialogController@UI@@6B@
    stamp ??_7DialogController@UI@@6B@
### UI::DialogController  ??1DialogController@UI@@UEAA@XZ @0x1800cd2f0
    stamp ??_7DialogController@UI@@6B@
    stamp ??_7DialogController@UI@@6B@
### UI::DialogController  ??0DialogController@UI@@QEAA@XZ @0x1800cd320
    CALL ??0Internal@Object@@QEAA@XZ
    stamp ??_7DialogController@UI@@6B@
    stamp ??_7DialogController@UI@@6B@
### UI::DialogControllerConfig  ??0DialogControllerConfig@UI@@QEAA@XZ @0x18003b4a0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7DialogControllerConfig@UI@@6B@
    stamp ??_7DialogControllerConfig@UI@@6B@
### UI::DialogControllerConfig  ??1DialogControllerConfig@UI@@UEAA@XZ @0x18003b4f0
    stamp ??_7DialogControllerConfig@UI@@6B@
    stamp ??_7DialogControllerConfig@UI@@6B@
### UI::DialogControllerConfig  ??0DialogControllerConfig@UI@@QEAA@AEBV01@@Z @0x18003b520
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7DialogControllerConfig@UI@@6B@
    stamp ??_7DialogControllerConfig@UI@@6B@
### UI::DialogInstance  ??0DialogInstance@UI@@QEAA@AEBV01@@Z @0x180045630
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7DialogInstance@UI@@6B@
### UI::DialogInstance  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@V?$InternalPtr@VElement@UI@@@Object@@V?$InternalPtr@VDialogInstance@UI@@@4@PEAVBase@4@PEAXH@Z @0x1801146b0
    CALL ??0Internal@Object@@QEAA@XZ
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7DialogInstance@UI@@6B@
### UI::DialogInstance  ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z @0x180114870
    CALL ??0Internal@Object@@QEAA@XZ
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7DialogInstance@UI@@6B@
### UI::DialogInstance  ??0DialogInstance@UI@@QEAA@XZ @0x180114ca0
    CALL ??0Internal@Object@@QEAA@XZ
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7DialogInstance@UI@@6B@
### UI::DialogInstance  ??1DialogInstance@UI@@UEAA@XZ @0x180114d40
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7DialogInstance@UI@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
### UI::DocumentTextDescription  ??0DocumentTextDescription@UI@@QEAA@AEBV01@@Z @0x18003bac0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7DocumentTextDescription@UI@@6B@
    stamp ??_7DocumentTextDescription@UI@@6B@
### UI::DocumentTextDescription  ??1DocumentTextDescription@UI@@UEAA@XZ @0x1800cfb30
    stamp ??_7DocumentTextDescription@UI@@6B@
    stamp ??_7DocumentTextDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::DocumentTextDescription  ??0DocumentTextDescription@UI@@QEAA@XZ @0x1800cfc30
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7DocumentTextDescription@UI@@6B@
    stamp ??_7DocumentTextDescription@UI@@6B@
### UI::DocumentTextElement  ??0DocumentTextElement@UI@@QEAA@AEBV01@@Z @0x18003c140
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7DocumentTextElement@UI@@6B@
    stamp ??_7DocumentTextElement@UI@@6B@
### UI::DocumentTextElement  ??1DocumentTextElement@UI@@UEAA@XZ @0x1800cf320
    stamp ??_7DocumentTextElement@UI@@6B@
    stamp ??_7DocumentTextElement@UI@@6B@
### UI::DocumentTextElement  ??0DocumentTextElement@UI@@QEAA@XZ @0x1800cf410
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7DocumentTextElement@UI@@6B@
    stamp ??_7DocumentTextElement@UI@@6B@
### UI::EditDescription  ??0EditDescription@UI@@QEAA@AEBV01@@Z @0x18003f890
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7EditDescription@UI@@6B@
    stamp ??_7EditDescription@UI@@6B@
### UI::EditDescription  ??1EditDescription@UI@@UEAA@XZ @0x1800d11f0
    stamp ??_7EditDescription@UI@@6B@
    stamp ??_7EditDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::EditDescription  ??0EditDescription@UI@@QEAA@XZ @0x1800d12c0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7EditDescription@UI@@6B@
    stamp ??_7EditDescription@UI@@6B@
### UI::EditElement  ??0EditElement@UI@@QEAA@AEBV01@@Z @0x18003fc90
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7EditElement@UI@@6B@
    stamp ??_7EditElement@UI@@6B@
### UI::EditElement  ??1EditElement@UI@@UEAA@XZ @0x1800d0e50
    stamp ??_7EditElement@UI@@6B@
    stamp ??_7EditElement@UI@@6B@
### UI::EditElement  ??0EditElement@UI@@QEAA@XZ @0x1800d0f10
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7EditElement@UI@@6B@
    stamp ??_7EditElement@UI@@6B@
### UI::Element  ??0Element@UI@@QEAA@AEBV01@@Z @0x1800358c0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Internal@Object@@6B@
    stamp ??_7Element@UI@@6B@
    stamp ??_7Element@UI@@6B@
### UI::Element  ??0Element@UI@@QEAA@XZ @0x180115e50
    CALL ??0Internal@Object@@QEAA@XZ
    stamp ??_7Element@UI@@6B@
    stamp ??_7Element@UI@@6B@
### UI::Element  ??1Element@UI@@UEAA@XZ @0x1801161a0
    stamp ??_7Element@UI@@6B@
    stamp ??_7Element@UI@@6B@
### UI::ElementDescription  ??0ElementDescription@UI@@QEAA@AEBV01@@Z @0x180035120
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ElementDescription  ??0ElementDescription@UI@@QEAA@XZ @0x180115a40
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ElementDescription  ??1ElementDescription@UI@@UEAA@XZ @0x180116100
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::Font  ??0Font@UI@@QEAA@AEBV01@@Z @0x180031e10
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7Font@UI@@6B@
    stamp ??_7Font@UI@@6B@
### UI::Font  ??0Font@UI@@QEAA@XZ @0x180117510
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7Font@UI@@6B@
    stamp ??_7Font@UI@@6B@
### UI::Font  ??1Font@UI@@UEAA@XZ @0x1801175a0
    stamp ??_7Font@UI@@6B@
    stamp ??_7Font@UI@@6B@
### UI::GraphDescription  ??0GraphDescription@UI@@QEAA@AEBV01@@Z @0x180040250
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7GraphDescription@UI@@6B@
    stamp ??_7GraphDescription@UI@@6B@
### UI::GraphDescription  ??1GraphDescription@UI@@UEAA@XZ @0x1800d1b30
    stamp ??_7GraphDescription@UI@@6B@
    stamp ??_7GraphDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::GraphDescription  ??0GraphDescription@UI@@QEAA@XZ @0x1800d1bd0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7GraphDescription@UI@@6B@
    stamp ??_7GraphDescription@UI@@6B@
### UI::GraphElement  ??0GraphElement@UI@@QEAA@AEBV01@@Z @0x1800406d0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7GraphElement@UI@@6B@
    stamp ??_7GraphElement@UI@@6B@
### UI::GraphElement  ??1GraphElement@UI@@UEAA@XZ @0x1800d1840
    stamp ??_7GraphElement@UI@@6B@
    stamp ??_7GraphElement@UI@@6B@
### UI::GraphElement  ??0GraphElement@UI@@QEAA@XZ @0x1800d1880
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7GraphElement@UI@@6B@
    stamp ??_7GraphElement@UI@@6B@
### UI::GroupDescription  ??0GroupDescription@UI@@QEAA@AEBV01@@Z @0x180046a30
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
### UI::GroupDescription  ??0GroupDescription@UI@@QEAA@XZ @0x180117b60
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
### UI::GroupDescription  ??1GroupDescription@UI@@UEAA@XZ @0x180117c00
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::GroupElement  ??0GroupElement@UI@@QEAA@AEBV01@@Z @0x180046d70
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
### UI::GroupElement  ??0GroupElement@UI@@QEAA@XZ @0x180117c80
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
### UI::GroupElement  ??1GroupElement@UI@@UEAA@XZ @0x180117cc0
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
### UI::ImageDescription  ??0ImageDescription@UI@@QEAA@AEBV01@@Z @0x1800360d0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
### UI::ImageDescription  ??0ImageDescription@UI@@QEAA@XZ @0x1801182d0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
### UI::ImageDescription  ??1ImageDescription@UI@@UEAA@XZ @0x180118370
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ImageDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ImageElement  ??0ImageElement@UI@@QEAA@AEBV01@@Z @0x1800364c0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
### UI::ImageElement  ??0ImageElement@UI@@QEAA@XZ @0x180118410
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
### UI::ImageElement  ??1ImageElement@UI@@UEAA@XZ @0x180118470
    stamp ??_7ImageElement@UI@@6B@
    stamp ??_7ImageElement@UI@@6B@
### UI::LabelDescription  ??0LabelDescription@UI@@QEAA@AEBV01@@Z @0x180042570
    CALL ??0TextDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7LabelDescription@UI@@6B@
    stamp ??_7LabelDescription@UI@@6B@
### UI::LabelDescription  ??1LabelDescription@UI@@UEAA@XZ @0x1800d2960
    stamp ??_7LabelDescription@UI@@6B@
    stamp ??_7LabelDescription@UI@@6B@
### UI::LabelDescription  ??0LabelDescription@UI@@QEAA@XZ @0x1800d29d0
    CALL ??0TextDescription@UI@@QEAA@XZ
    stamp ??_7LabelDescription@UI@@6B@
    stamp ??_7LabelDescription@UI@@6B@
### UI::LabelElement  ??0LabelElement@UI@@QEAA@AEBV01@@Z @0x180042a00
    CALL ??0TextElement@UI@@QEAA@AEBV01@@Z
    stamp ??_7LabelElement@UI@@6B@
    stamp ??_7LabelElement@UI@@6B@
### UI::LabelElement  ??1LabelElement@UI@@UEAA@XZ @0x1800d25f0
    stamp ??_7LabelElement@UI@@6B@
    stamp ??_7LabelElement@UI@@6B@
### UI::LabelElement  ??0LabelElement@UI@@QEAA@XZ @0x1800d2620
    CALL ??0TextElement@UI@@QEAA@XZ
    stamp ??_7LabelElement@UI@@6B@
    stamp ??_7LabelElement@UI@@6B@
### UI::LayoutDescription  ??0LayoutDescription@UI@@QEAA@AEBV01@@Z @0x180043110
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7LayoutDescription@UI@@6B@
    stamp ??_7LayoutDescription@UI@@6B@
### UI::LayoutDescription  ??1LayoutDescription@UI@@UEAA@XZ @0x1800d3a10
    stamp ??_7LayoutDescription@UI@@6B@
    stamp ??_7LayoutDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::LayoutDescription  ??0LayoutDescription@UI@@QEAA@XZ @0x1800d3a90
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7LayoutDescription@UI@@6B@
    stamp ??_7LayoutDescription@UI@@6B@
### UI::LayoutElement  ??0LayoutElement@UI@@QEAA@AEBV01@@Z @0x180043470
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7LayoutElement@UI@@6B@
    stamp ??_7LayoutElement@UI@@6B@
### UI::LayoutElement  ??1LayoutElement@UI@@UEAA@XZ @0x1800d32a0
    stamp ??_7LayoutElement@UI@@6B@
    stamp ??_7LayoutElement@UI@@6B@
### UI::LayoutElement  ??0LayoutElement@UI@@QEAA@XZ @0x1800d32e0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7LayoutElement@UI@@6B@
    stamp ??_7LayoutElement@UI@@6B@
### UI::ListDescription  ??0ListDescription@UI@@QEAA@AEBV01@@Z @0x180044d60
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ListDescription@UI@@6B@
    stamp ??_7ListDescription@UI@@6B@
### UI::ListDescription  ??1ListDescription@UI@@UEAA@XZ @0x1800d4910
    stamp ??_7ListDescription@UI@@6B@
    stamp ??_7ListDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ListDescription  ??0ListDescription@UI@@QEAA@XZ @0x1800d49b0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ListDescription@UI@@6B@
    stamp ??_7ListDescription@UI@@6B@
### UI::ListElement  ??0ListElement@UI@@QEAA@AEBV01@@Z @0x180045140
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ListElement@UI@@6B@
    stamp ??_7ListElement@UI@@6B@
### UI::ListElement  ??1ListElement@UI@@UEAA@XZ @0x1800d4530
    stamp ??_7ListElement@UI@@6B@
    stamp ??_7ListElement@UI@@6B@
### UI::ListElement  ??0ListElement@UI@@QEAA@XZ @0x1800d4570
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ListElement@UI@@6B@
    stamp ??_7ListElement@UI@@6B@
### UI::NumberDescription  ??0NumberDescription@UI@@QEAA@AEBV01@@Z @0x18004a620
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7NumberDescription@UI@@6B@
    stamp ??_7NumberDescription@UI@@6B@
### UI::NumberDescription  ??0NumberDescription@UI@@QEAA@XZ @0x180118b60
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7NumberDescription@UI@@6B@
    stamp ??_7NumberDescription@UI@@6B@
### UI::NumberDescription  ??1NumberDescription@UI@@UEAA@XZ @0x180118c00
    stamp ??_7NumberDescription@UI@@6B@
    stamp ??_7NumberDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::NumberEditDescription  ??0NumberEditDescription@UI@@QEAA@AEBV01@@Z @0x180046050
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7NumberEditDescription@UI@@6B@
    stamp ??_7NumberEditDescription@UI@@6B@
### UI::NumberEditDescription  ??1NumberEditDescription@UI@@UEAA@XZ @0x1800d4fe0
    stamp ??_7NumberEditDescription@UI@@6B@
    stamp ??_7NumberEditDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::NumberEditDescription  ??0NumberEditDescription@UI@@QEAA@XZ @0x1800d5050
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7NumberEditDescription@UI@@6B@
    stamp ??_7NumberEditDescription@UI@@6B@
### UI::NumberEditElement  ??0NumberEditElement@UI@@QEAA@AEBV01@@Z @0x180046450
    CALL ??0EditElement@UI@@QEAA@AEBV01@@Z
    stamp ??_7NumberEditElement@UI@@6B@
    stamp ??_7NumberEditElement@UI@@6B@
### UI::NumberEditElement  ??1NumberEditElement@UI@@UEAA@XZ @0x1800d4c40
    stamp ??_7NumberEditElement@UI@@6B@
    stamp ??_7NumberEditElement@UI@@6B@
### UI::NumberEditElement  ??0NumberEditElement@UI@@QEAA@XZ @0x1800d4c70
    CALL ??0EditElement@UI@@QEAA@XZ
    stamp ??_7NumberEditElement@UI@@6B@
    stamp ??_7NumberEditElement@UI@@6B@
### UI::NumberElement  ??0NumberElement@UI@@QEAA@AEBV01@@Z @0x18004a9d0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7NumberElement@UI@@6B@
    stamp ??_7NumberElement@UI@@6B@
### UI::NumberElement  ??0NumberElement@UI@@QEAA@XZ @0x180118c70
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7NumberElement@UI@@6B@
    stamp ??_7NumberElement@UI@@6B@
### UI::NumberElement  ??1NumberElement@UI@@UEAA@XZ @0x180118ce0
    stamp ??_7NumberElement@UI@@6B@
    stamp ??_7NumberElement@UI@@6B@
### UI::PageDescription  ??0PageDescription@UI@@QEAA@AEBV01@@Z @0x1800470a0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7PageDescription@UI@@6B@
    stamp ??_7PageDescription@UI@@6B@
### UI::PageDescription  ??1PageDescription@UI@@UEAA@XZ @0x1800d54b0
    stamp ??_7PageDescription@UI@@6B@
    stamp ??_7PageDescription@UI@@6B@
### UI::PageDescription  ??0PageDescription@UI@@QEAA@XZ @0x1800d54e0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7PageDescription@UI@@6B@
    stamp ??_7PageDescription@UI@@6B@
### UI::PageElement  ??0PageElement@UI@@QEAA@AEBV01@@Z @0x180047450
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7PageElement@UI@@6B@
    stamp ??_7PageElement@UI@@6B@
### UI::PageElement  ??1PageElement@UI@@UEAA@XZ @0x1800d5220
    stamp ??_7PageElement@UI@@6B@
    stamp ??_7PageElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
### UI::PageElement  ??0PageElement@UI@@QEAA@XZ @0x1800d5260
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7PageElement@UI@@6B@
    stamp ??_7PageElement@UI@@6B@
### UI::ProgressDescription  ??0ProgressDescription@UI@@QEAA@AEBV01@@Z @0x180077930
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ProgressDescription@UI@@6B@
    stamp ??_7ProgressDescription@UI@@6B@
### UI::ProgressDescription  ??1ProgressDescription@UI@@UEAA@XZ @0x18011a080
    stamp ??_7ProgressDescription@UI@@6B@
    stamp ??_7ProgressDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ProgressDescription  ??0ProgressDescription@UI@@QEAA@XZ @0x18011a210
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ProgressDescription@UI@@6B@
    stamp ??_7ProgressDescription@UI@@6B@
### UI::ProgressElement  ??1ProgressElement@UI@@UEAA@XZ @0x180077ef0
### UI::ProgressElement  ??0ProgressElement@UI@@QEAA@AEBV01@@Z @0x180077f10
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ProgressElement@UI@@6B@
    stamp ??_7ProgressElement@UI@@6B@
### UI::ProgressElement  ??0ProgressElement@UI@@QEAA@XZ @0x180119c40
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ProgressElement@UI@@6B@
    stamp ??_7ProgressElement@UI@@6B@
### UI::RadioDescription  ??0RadioDescription@UI@@QEAA@AEBV01@@Z @0x180047830
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7RadioDescription@UI@@6B@
    stamp ??_7RadioDescription@UI@@6B@
### UI::RadioDescription  ??1RadioDescription@UI@@UEAA@XZ @0x1800d5b20
    stamp ??_7RadioDescription@UI@@6B@
    stamp ??_7RadioDescription@UI@@6B@
### UI::RadioDescription  ??0RadioDescription@UI@@QEAA@XZ @0x1800d5b50
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7GroupDescription@UI@@6B@
    stamp ??_7RadioDescription@UI@@6B@
    stamp ??_7RadioDescription@UI@@6B@
### UI::RadioElement  ??0RadioElement@UI@@QEAA@AEBV01@@Z @0x180047c20
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7RadioElement@UI@@6B@
    stamp ??_7RadioElement@UI@@6B@
### UI::RadioElement  ??1RadioElement@UI@@UEAA@XZ @0x1800d5820
    stamp ??_7RadioElement@UI@@6B@
    stamp ??_7RadioElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
### UI::RadioElement  ??0RadioElement@UI@@QEAA@XZ @0x1800d5870
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7GroupElement@UI@@6B@
    stamp ??_7RadioElement@UI@@6B@
    stamp ??_7RadioElement@UI@@6B@
### UI::RibbonDescription  ??0RibbonDescription@UI@@QEAA@AEBV01@@Z @0x180044530
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7RibbonDescription@UI@@6B@
    stamp ??_7RibbonDescription@UI@@6B@
### UI::RibbonDescription  ??0RibbonDescription@UI@@QEAA@XZ @0x1800d5bf0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7RibbonDescription@UI@@6B@
    stamp ??_7RibbonDescription@UI@@6B@
### UI::RibbonDescription  ??1RibbonDescription@UI@@UEAA@XZ @0x1800d5c90
    stamp ??_7RibbonDescription@UI@@6B@
    stamp ??_7RibbonDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::RibbonElement  ??0RibbonElement@UI@@QEAA@AEBV01@@Z @0x180044980
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7RibbonElement@UI@@6B@
    stamp ??_7RibbonElement@UI@@6B@
### UI::RibbonElement  ??0RibbonElement@UI@@QEAA@XZ @0x1800d5d10
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7RibbonElement@UI@@6B@
    stamp ??_7RibbonElement@UI@@6B@
### UI::RibbonElement  ??1RibbonElement@UI@@UEAA@XZ @0x1800d5d70
    stamp ??_7RibbonElement@UI@@6B@
    stamp ??_7RibbonElement@UI@@6B@
### UI::ScrollDescription  ??0ScrollDescription@UI@@QEAA@AEBV01@@Z @0x18003a2e0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7ScrollDescription@UI@@6B@
    stamp ??_7ScrollDescription@UI@@6B@
### UI::ScrollDescription  ??0ScrollDescription@UI@@QEAA@XZ @0x18011a4e0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ScrollDescription@UI@@6B@
    stamp ??_7ScrollDescription@UI@@6B@
### UI::ScrollDescription  ??1ScrollDescription@UI@@UEAA@XZ @0x18011a5d0
    stamp ??_7ScrollDescription@UI@@6B@
    stamp ??_7ScrollDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::ScrollElement  ??0ScrollElement@UI@@QEAA@AEBV01@@Z @0x18003aa30
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7ScrollElement@UI@@6B@
    stamp ??_7ScrollElement@UI@@6B@
### UI::ScrollElement  ??0ScrollElement@UI@@QEAA@XZ @0x18011a7f0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7ScrollElement@UI@@6B@
    stamp ??_7ScrollElement@UI@@6B@
### UI::ScrollElement  ??1ScrollElement@UI@@UEAA@XZ @0x18011a900
    stamp ??_7ScrollElement@UI@@6B@
    stamp ??_7ScrollElement@UI@@6B@
### UI::SliderDescription  ??0SliderDescription@UI@@QEAA@AEBV01@@Z @0x180049910
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7SliderDescription@UI@@6B@
    stamp ??_7SliderDescription@UI@@6B@
### UI::SliderDescription  ??1SliderDescription@UI@@UEAA@XZ @0x1800d7a00
    stamp ??_7SliderDescription@UI@@6B@
    stamp ??_7SliderDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::SliderDescription  ??0SliderDescription@UI@@QEAA@XZ @0x1800d7b60
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7SliderDescription@UI@@6B@
    stamp ??_7SliderDescription@UI@@6B@
### UI::SliderElement  ??1SliderElement@UI@@UEAA@XZ @0x180049ee0
### UI::SliderElement  ??0SliderElement@UI@@QEAA@AEBV01@@Z @0x180049f00
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7SliderElement@UI@@6B@
    stamp ??_7SliderElement@UI@@6B@
### UI::SliderElement  ??0SliderElement@UI@@QEAA@XZ @0x1800d7570
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7SliderElement@UI@@6B@
    stamp ??_7SliderElement@UI@@6B@
### UI::SpinnerDescription  ??0SpinnerDescription@UI@@QEAA@AEBV01@@Z @0x18004af20
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7SpinnerDescription@UI@@6B@
    stamp ??_7SpinnerDescription@UI@@6B@
### UI::SpinnerDescription  ??1SpinnerDescription@UI@@UEAA@XZ @0x1800d8950
    stamp ??_7SpinnerDescription@UI@@6B@
    stamp ??_7SpinnerDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::SpinnerDescription  ??0SpinnerDescription@UI@@QEAA@XZ @0x1800d89c0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7SpinnerDescription@UI@@6B@
    stamp ??_7SpinnerDescription@UI@@6B@
### UI::SpinnerElement  ??0SpinnerElement@UI@@QEAA@AEBV01@@Z @0x18004b2e0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7SpinnerElement@UI@@6B@
    stamp ??_7SpinnerElement@UI@@6B@
### UI::SpinnerElement  ??1SpinnerElement@UI@@UEAA@XZ @0x1800d8640
    stamp ??_7SpinnerElement@UI@@6B@
    stamp ??_7SpinnerElement@UI@@6B@
### UI::SpinnerElement  ??0SpinnerElement@UI@@QEAA@XZ @0x1800d8670
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7SpinnerElement@UI@@6B@
    stamp ??_7SpinnerElement@UI@@6B@
### UI::SpriteCapture  ??0SpriteCapture@UI@@QEAA@AEBV01@@Z @0x18003cb50
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7SpriteCapture@UI@@6B@
    stamp ??_7SpriteCapture@UI@@6B@
### UI::SpriteCapture  ??0SpriteCapture@UI@@QEAA@XZ @0x1800d9140
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7SpriteCapture@UI@@6B@
    stamp ??_7SpriteCapture@UI@@6B@
### UI::SpriteCapture  ??1SpriteCapture@UI@@UEAA@XZ @0x1800d91b0
    stamp ??_7SpriteCapture@UI@@6B@
    stamp ??_7SpriteCapture@UI@@6B@
### UI::SpriteSheet  ??0SpriteSheet@UI@@QEAA@AEBV01@@Z @0x1800035d0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7SpriteSheet@UI@@6B@
    stamp ??_7SpriteSheet@UI@@6B@
### UI::SpriteSheet  ??0SpriteSheet@UI@@QEAA@XZ @0x18011d600
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7SpriteSheet@UI@@6B@
    stamp ??_7SpriteSheet@UI@@6B@
### UI::SpriteSheet  ??1SpriteSheet@UI@@UEAA@XZ @0x18011d670
    stamp ??_7SpriteSheet@UI@@6B@
    stamp ??_7SpriteSheet@UI@@6B@
### UI::StringTable  ??0StringTable@UI@@QEAA@AEBV01@@Z @0x180002620
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    CALL ??0Filename@System@@QEAA@AEBV01@@Z
    stamp ??_7StringTable@UI@@6B@
    stamp ??_7StringTable@UI@@6B@
### UI::StringTable  ??0StringTable@UI@@QEAA@XZ @0x18011d890
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7StringTable@UI@@6B@
    stamp ??_7StringTable@UI@@6B@
### UI::StringTable  ??1StringTable@UI@@UEAA@XZ @0x18011d8f0
    stamp ??_7StringTable@UI@@6B@
    stamp ??_7StringTable@UI@@6B@
### UI::TabDescription  ??0TabDescription@UI@@QEAA@AEBV01@@Z @0x18004c3b0
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7TabDescription@UI@@6B@
    stamp ??_7TabDescription@UI@@6B@
### UI::TabDescription  ??1TabDescription@UI@@UEAA@XZ @0x1800da8e0
    stamp ??_7TabDescription@UI@@6B@
    stamp ??_7TabDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::TabDescription  ??0TabDescription@UI@@QEAA@XZ @0x1800daa00
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7TabDescription@UI@@6B@
    stamp ??_7TabDescription@UI@@6B@
### UI::TabElement  ??0TabElement@UI@@QEAA@AEBV01@@Z @0x18004c8b0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7TabElement@UI@@6B@
    stamp ??_7TabElement@UI@@6B@
### UI::TabElement  ??1TabElement@UI@@UEAA@XZ @0x1800da360
    stamp ??_7TabElement@UI@@6B@
    stamp ??_7TabElement@UI@@6B@
### UI::TabElement  ??0TabElement@UI@@QEAA@XZ @0x1800da390
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7TabElement@UI@@6B@
    stamp ??_7TabElement@UI@@6B@
### UI::TableDescription  ??0TableDescription@UI@@QEAA@AEBV01@@Z @0x180079090
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7TableDescription@UI@@6B@
    stamp ??_7TableDescription@UI@@6B@
### UI::TableDescription  ??1TableDescription@UI@@UEAA@XZ @0x18011e6f0
    stamp ??_7TableDescription@UI@@6B@
    stamp ??_7TableDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::TableDescription  ??0TableDescription@UI@@QEAA@XZ @0x18011e7a0
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7TableDescription@UI@@6B@
    stamp ??_7TableDescription@UI@@6B@
### UI::TableElement  ??0TableElement@UI@@QEAA@AEBV01@@Z @0x180079470
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7TableElement@UI@@6B@
    stamp ??_7TableElement@UI@@6B@
### UI::TableElement  ??1TableElement@UI@@UEAA@XZ @0x18011e2a0
    stamp ??_7TableElement@UI@@6B@
    stamp ??_7TableElement@UI@@6B@
### UI::TableElement  ??0TableElement@UI@@QEAA@XZ @0x18011e2f0
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7TableElement@UI@@6B@
    stamp ??_7TableElement@UI@@6B@
### UI::TextDescription  ??0TextDescription@UI@@QEAA@AEBV01@@Z @0x180041c90
    CALL ??0ElementDescription@UI@@QEAA@AEBV01@@Z
    stamp ??_7TextDescription@UI@@6B@
    stamp ??_7TextDescription@UI@@6B@
### UI::TextDescription  ??0TextDescription@UI@@QEAA@XZ @0x18011ea20
    stamp ??_7Base@Object@@6B@
    stamp ??_7Base@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7External@Object@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7TextDescription@UI@@6B@
    stamp ??_7TextDescription@UI@@6B@
### UI::TextDescription  ??1TextDescription@UI@@UEAA@XZ @0x18011eae0
    stamp ??_7TextDescription@UI@@6B@
    stamp ??_7TextDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
    stamp ??_7ElementDescription@UI@@6B@
### UI::TextElement  ??0TextElement@UI@@QEAA@AEBV01@@Z @0x1800420d0
    CALL ??0Element@UI@@QEAA@AEBV01@@Z
    stamp ??_7TextElement@UI@@6B@
    stamp ??_7TextElement@UI@@6B@
### UI::TextElement  ??0TextElement@UI@@QEAA@XZ @0x18011eb80
    CALL ??0Element@UI@@QEAA@XZ
    stamp ??_7TextElement@UI@@6B@
    stamp ??_7TextElement@UI@@6B@
### UI::TextElement  ??1TextElement@UI@@UEAA@XZ @0x18011eda0
    stamp ??_7TextElement@UI@@6B@
    stamp ??_7TextElement@UI@@6B@
### UI::ToolTipDialog  ??0ToolTipDialog@UI@@QEAA@AEBV01@@Z @0x18004d180
    CALL ??0DialogInstance@UI@@QEAA@AEBV01@@Z
    stamp ??_7ToolTipDialog@UI@@6B@
    stamp ??_7ToolTipDialog@UI@@6B@
### UI::ToolTipDialog  ??1ToolTipDialog@UI@@UEAA@XZ @0x1800dac70
    stamp ??_7ToolTipDialog@UI@@6B@
    stamp ??_7ToolTipDialog@UI@@6B@
### UI::ToolTipDialog  ??0ToolTipDialog@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@5@@Z @0x1800daca0
    CALL ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z
    stamp ??_7ToolTipDialog@UI@@6B@
    stamp ??_7ToolTipDialog@UI@@6B@
### UI::ToolTipDialog  ??0ToolTipDialog@UI@@QEAA@XZ @0x1800dad00
    CALL ??0DialogInstance@UI@@QEAA@XZ
    stamp ??_7ToolTipDialog@UI@@6B@
    stamp ??_7ToolTipDialog@UI@@6B@
