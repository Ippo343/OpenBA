# B35_SLOTS.md — raw appendix for B.3 step 5 (`Video` / `Graphics`)

Produced 2026-09-19 by block 0 from the RETAIL `Runtime-x64.dll` (asserted before the read:
15290 functions, image base 0x180000000) by `b35_session/ghidra/VgAppendix.java`, run through
GhidraMCP. Slot targets are `read_memory` qwords over `.rdata`, terminating on the next `??_7`
symbol / a zero qword / a non-code target; `sizeof` is the `MOV EDX,imm` in the class's own
slot-0 vector deleting destructor, read as disassembly. `inherited-from X` means the slot target
is byte-identical to X's target at the same index. `_purecall` is 0x18007ec28. `(unexported)`
means the address carries no `?`-mangled symbol. `Object::Base` really is a 3-entry array —
`??_7External@Object@@6B@` begins at Base+0x18. Copy from here; do not retype. Rules and
verdicts are in `B35_RULES.md`; anything not listed here was not read from bytes.

PROGRAM Runtime-x64.dll functions=15290 imagebase=180000000

@@@@@@ SECTION 1 -- 39 minted-vtable classes

### Graphics::AmbientLight  vtable=0x180158cd0  slots=11  sizeof=0xd8  parent=Graphics::Light
  [ 0] +0x00 0x180059080  (unexported) FUN_180059080
  [ 1] +0x08 0x1800fa7a0  ?Update@AmbientLight@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x1800fa790  ?Draw@AmbientLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x1800fa890  ?AddDrawItems@AmbientLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Light
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Light
  [ 6] +0x30 0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z   inherited-from Graphics::Light
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Light
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Light
  [ 9] +0x48 0x1800fa880  ?DrawShadowMap@AmbientLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
  [10] +0x50 0x1800fa7b0  ?Draw@AmbientLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z

### Graphics::AnimationEvent  vtable=0x180154bf0  slots=9  sizeof=0x20  parent=Object::External
  [ 0] +0x00 0x180011800  (unexported) FUN_180011800
  [ 1] +0x08 0x1800116f0  ?TypeName@AnimationEvent@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800116d0  ?Type@AnimationEvent@Graphics@@UEBAIXZ
  [ 3] +0x18 0x18007ec28  _purecall   inherited-from Object::External
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x18007ec28  _purecall   inherited-from Object::External
  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External
  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External
  [ 8] +0x40 0x18007ec28  _purecall

### Graphics::AnimationGroup  vtable=0x180154e10  slots=9  sizeof=0x58  parent=Object::External
  [ 0] +0x00 0x1800167f0  (unexported) FUN_1800167f0
  [ 1] +0x08 0x180016310  ?TypeName@AnimationGroup@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800162f0  ?Type@AnimationGroup@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800fb680  ?GetFileVersion@AnimationGroup@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800fb690  ?Compile@AnimationGroup@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1800fb6a0  ?Serialize@AnimationGroup@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180016550  ?Reconstruct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180016650  ?Destruct@AnimationGroup@Graphics@@UEAAXXZ
  [ 8] +0x40 0x1800165d0  ?Construct@AnimationGroup@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::EdgeMesh  vtable=0x180154c70  slots=9  sizeof=0x180  parent=Object::External
  [ 0] +0x00 0x1800134b0  (unexported) FUN_1800134b0
  [ 1] +0x08 0x180012c90  ?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x180012c70  ?Type@EdgeMesh@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800a23d0  ?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800a21c0  ?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x1800a21d0  ?Serialize@EdgeMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180012e10  ?Reconstruct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180012e40  ?Destruct@EdgeMesh@Graphics@@UEAAXXZ
  [ 8] +0x40 0x180012e30  ?Construct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::EdgeMeshInstance  vtable=0x180154d70  slots=9  sizeof=0xd0  parent=Graphics::Geometry
  [ 0] +0x00 0x1800175d0  (unexported) FUN_1800175d0
  [ 1] +0x08 0x1800a39b0  ?Update@EdgeMeshInstance@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x1800a3150  ?Draw@EdgeMeshInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x1800a2fd0  ?AddDrawItems@EdgeMeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Geometry
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Geometry
  [ 6] +0x30 0x1800a3840  ?GetInstanceTransform@EdgeMeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Geometry
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Geometry

### Graphics::FrameCopyImageBuffer  vtable=0x180154f00  slots=12  sizeof=0x88  parent=Graphics::GeneratedImageBuffer
  [ 0] +0x00 0x18001f260  (unexported) FUN_18001f260
  [ 1] +0x08 0x18001efb0  ?TypeName@FrameCopyImageBuffer@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x18001ef90  ?Type@FrameCopyImageBuffer@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800a3c70  ?GetFileVersion@FrameCopyImageBuffer@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Graphics::GeneratedImageBuffer
  [ 5] +0x28 0x1800a3c40  ?Serialize@FrameCopyImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18001f130  ?Reconstruct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18001f160  ?Destruct@FrameCopyImageBuffer@Graphics@@UEAAXXZ
  [ 8] +0x40 0x18001f150  ?Construct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 9] +0x48 0x1800a3b70  ?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@W4Usage@45@W4Format@45@IHHW4FilterMode@45@W4AddressMode@45@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
  [10] +0x50 0x1800a3b20  ?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z
  [11] +0x58 0x1800a3b10  ?Render@FrameCopyImageBuffer@Graphics@@UEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z

### Graphics::GeneratedImageBuffer  vtable=0x180154f60  slots=12  sizeof=0x70  parent=Video::ImageBuffer
  [ 0] +0x00 0x18001eee0  (unexported) FUN_18001eee0
  [ 1] +0x08 0x18001ede0  ?TypeName@GeneratedImageBuffer@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x18001edc0  ?Type@GeneratedImageBuffer@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800a3eb0  ?GetFileVersion@GeneratedImageBuffer@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Video::ImageBuffer
  [ 5] +0x28 0x1800a3ec0  ?Serialize@GeneratedImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18001ea80  ?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ   inherited-from Video::ImageBuffer
  [ 7] +0x38 0x18001eab0  ?Destruct@ImageBuffer@Video@@UEAAXXZ   inherited-from Video::ImageBuffer
  [ 8] +0x40 0x18001eaa0  ?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ   inherited-from Video::ImageBuffer
  [ 9] +0x48 0x1800f2680  ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z   inherited-from Video::ImageBuffer
  [10] +0x50 0x18007ec28  _purecall
  [11] +0x58 0x18007ec28  _purecall

### Graphics::Geometry  vtable=0x180154e60  slots=9  sizeof=0xc0  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180015e90  (unexported) FUN_180015e90
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z
  [ 6] +0x30 0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ

### Graphics::GraphicsMesh  vtable=0x180154cc0  slots=9  sizeof=0x98  parent=Object::CompiledExternal
  [ 0] +0x00 0x180012bd0  (unexported) FUN_180012bd0
  [ 1] +0x08 0x180012390  ?TypeName@GraphicsMesh@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x180012370  ?Type@GraphicsMesh@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800ff3a0  ?GetFileVersion@GraphicsMesh@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::CompiledExternal
  [ 5] +0x28 0x1800ff3b0  ?Serialize@GraphicsMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180012510  ?Reconstruct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180012540  ?Destruct@GraphicsMesh@Graphics@@UEAAXXZ
  [ 8] +0x40 0x180012530  ?Construct@GraphicsMesh@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::Light  vtable=0x180158d30  slots=11  sizeof=0xc0  parent=Graphics::Geometry
  [ 0] +0x00 0x180058dd0  (unexported) FUN_180058dd0
  [ 1] +0x08 0x18007ec28  _purecall   inherited-from Graphics::Geometry
  [ 2] +0x10 0x18007ec28  _purecall   inherited-from Graphics::Geometry
  [ 3] +0x18 0x18007ec28  _purecall   inherited-from Graphics::Geometry
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Geometry
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Geometry
  [ 6] +0x30 0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z   inherited-from Graphics::Geometry
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Geometry
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Geometry
  [ 9] +0x48 0x18007ec28  _purecall
  [10] +0x50 0x18007ec28  _purecall

### Graphics::Material  vtable=0x180154d10  slots=9  sizeof=0x38  parent=Object::External
  [ 0] +0x00 0x180011d20  (unexported) Graphics_Material_DeletingDtor
  [ 1] +0x08 0x1800118d0  ?TypeName@Material@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800118b0  ?Type@Material@Graphics@@UEBAIXZ
  [ 3] +0x18 0x180102a40  ?GetFileVersion@Material@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x180102a50  ?Serialize@Material@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180011ae0  ?Reconstruct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180011bc0  ?Destruct@Material@Graphics@@UEAAXXZ
  [ 8] +0x40 0x180011b50  ?Construct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::MaterialInstance  vtable=0x1801545e0  slots=9  sizeof=0x60  parent=Object::External
  [ 0] +0x00 0x180002d60  (unexported) FUN_180002d60
  [ 1] +0x08 0x180002810  ?TypeName@MaterialInstance@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800027f0  ?Type@MaterialInstance@Graphics@@UEBAIXZ
  [ 3] +0x18 0x180103a60  ?GetFileVersion@MaterialInstance@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x180103a70  ?Serialize@MaterialInstance@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180002a50  ?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180002b50  ?Destruct@MaterialInstance@Graphics@@UEAAXXZ
  [ 8] +0x40 0x180002ad0  ?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::MeshInstance  vtable=0x180154dc0  slots=9  sizeof=0x120  parent=Graphics::Geometry
  [ 0] +0x00 0x180017330  (unexported) FUN_180017330
  [ 1] +0x08 0x180100800  ?Update@MeshInstance@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x180100960  ?Draw@MeshInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x180100f20  ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Geometry
  [ 5] +0x28 0x180101200  ?GetSubTransform@MeshInstance@Graphics@@UEBA?AVTransform@Math@@H@Z
  [ 6] +0x30 0x180101280  ?GetInstanceTransform@MeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z
  [ 7] +0x38 0x180101660  ?SetSharedInstanceData@MeshInstance@Graphics@@UEBAHAEAVContext@Video@@HM@Z
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Geometry

### Graphics::Particle  vtable=0x180155130  slots=9  sizeof=0x48  parent=Object::External
  [ 0] +0x00 0x180020160  (unexported) FUN_180020160
  [ 1] +0x08 0x18001ff30  ?TypeName@Particle@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x18001ff10  ?Type@Particle@Graphics@@UEBAIXZ
  [ 3] +0x18 0x180104080  ?GetFileVersion@Particle@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x180103f00  ?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::External
  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::External
  [ 8] +0x40 0x18007ec28  _purecall

### Graphics::ParticleInstance  vtable=0x1801550e0  slots=10  sizeof=0xd8  parent=Graphics::Geometry
  [ 0] +0x00 0x180020310  (unexported) FUN_180020310
  [ 1] +0x08 0x18007ec28  _purecall   inherited-from Graphics::Geometry
  [ 2] +0x10 0x180104230  ?Draw@ParticleInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x180104700  ?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Geometry
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Geometry
  [ 6] +0x30 0x1801047f0  ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Geometry
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Geometry
  [ 9] +0x48 0x180104a00  ?SetParticle@ParticleInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@@Object@@@Z

### Graphics::ParticleSystem  vtable=0x180155090  slots=10  sizeof=0x108  parent=Graphics::Particle
  [ 0] +0x00 0x180020a70  (unexported) FUN_180020a70
  [ 1] +0x08 0x1800203e0  ?TypeName@ParticleSystem@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800203c0  ?Type@ParticleSystem@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800a4d10  ?GetFileVersion@ParticleSystem@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Graphics::Particle
  [ 5] +0x28 0x1800a4d20  ?Serialize@ParticleSystem@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x180020560  ?Reconstruct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x180020590  ?Destruct@ParticleSystem@Graphics@@UEAAXXZ
  [ 8] +0x40 0x1800a5140  ?CreateSystem@ParticleSystem@Graphics@@UEBAPEAVParticleInstance@2@XZ
  [ 9] +0x48 0x180020580  ?Construct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ

### Graphics::ParticleSystemInstance  vtable=0x180155040  slots=10  sizeof=0xe0  parent=Graphics::ParticleInstance
  [ 0] +0x00 0x180020ce0  (unexported) FUN_180020ce0
  [ 1] +0x08 0x1800a5600  ?Update@ParticleSystemInstance@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x180104230  ?Draw@ParticleInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z   inherited-from Graphics::ParticleInstance
  [ 3] +0x18 0x180104700  ?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z   inherited-from Graphics::ParticleInstance
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::ParticleInstance
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::ParticleInstance
  [ 6] +0x30 0x1801047f0  ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z   inherited-from Graphics::ParticleInstance
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::ParticleInstance
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::ParticleInstance
  [ 9] +0x48 0x1800a6a80  ?SetParticle@ParticleSystemInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@@Object@@@Z

### Graphics::ReflectionImageBuffer  vtable=0x180155190  slots=12  sizeof=0x90  parent=Graphics::GeneratedImageBuffer
  [ 0] +0x00 0x180022600  (unexported) FUN_180022600
  [ 1] +0x08 0x1800221d0  ?TypeName@ReflectionImageBuffer@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x1800221b0  ?Type@ReflectionImageBuffer@Graphics@@UEBAIXZ
  [ 3] +0x18 0x1800a70a0  ?GetFileVersion@ReflectionImageBuffer@Graphics@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Graphics::GeneratedImageBuffer
  [ 5] +0x28 0x1800a6f00  ?Serialize@ReflectionImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x1800223e0  ?Reconstruct@ReflectionImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x1800224c0  ?Destruct@ReflectionImageBuffer@Graphics@@UEAAXXZ
  [ 8] +0x40 0x180022450  ?Construct@ReflectionImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 9] +0x48 0x1800f2680  ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z   inherited-from Graphics::GeneratedImageBuffer
  [10] +0x50 0x1800a6ef0  ?Generate@ReflectionImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z
  [11] +0x58 0x1800a6b80  ?Render@ReflectionImageBuffer@Graphics@@UEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z

### Graphics::SpotLight  vtable=0x180158db0  slots=11  sizeof=0x168  parent=Graphics::Light
  [ 0] +0x00 0x1800642b0  (unexported) FUN_1800642b0
  [ 1] +0x08 0x180105950  ?Update@SpotLight@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x180105940  ?Draw@SpotLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x180105e30  ?AddDrawItems@SpotLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Light
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Light
  [ 6] +0x30 0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z   inherited-from Graphics::Light
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Light
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Light
  [ 9] +0x48 0x180105d00  ?DrawShadowMap@SpotLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
  [10] +0x50 0x180105960  ?Draw@SpotLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z

### Graphics::SunLight  vtable=0x180158e10  slots=11  sizeof=0x108  parent=Graphics::Light
  [ 0] +0x00 0x180064620  (unexported) FUN_180064620
  [ 1] +0x08 0x1801067a0  ?Update@SunLight@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x180106790  ?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x180106f50  ?AddDrawItems@SunLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x1800a4020  ?UpdateAttached@Geometry@Graphics@@UEAAXXZ   inherited-from Graphics::Light
  [ 5] +0x28 0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z   inherited-from Graphics::Light
  [ 6] +0x30 0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z   inherited-from Graphics::Light
  [ 7] +0x38 0x180015590  ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z   inherited-from Graphics::Light
  [ 8] +0x40 0x1800a4420  ?Clean@Geometry@Graphics@@UEAA_NXZ   inherited-from Graphics::Light
  [ 9] +0x48 0x180106bc0  ?DrawShadowMap@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
  [10] +0x50 0x1801067b0  ?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z

### Video::ConstantBufferData  vtable=0x180158b30  slots=4  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058990  (unexported) FUN_180058990
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall

### Video::ContextData  vtable=0x180158b90  slots=20  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058a90  (unexported) FUN_180058a90
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x18007ec28  _purecall
  [ 6] +0x30 0x18007ec28  _purecall
  [ 7] +0x38 0x18007ec28  _purecall
  [ 8] +0x40 0x18007ec28  _purecall
  [ 9] +0x48 0x18007ec28  _purecall
  [10] +0x50 0x18007ec28  _purecall
  [11] +0x58 0x18007ec28  _purecall
  [12] +0x60 0x18007ec28  _purecall
  [13] +0x68 0x18007ec28  _purecall
  [14] +0x70 0x18007ec28  _purecall
  [15] +0x78 0x18007ec28  _purecall
  [16] +0x80 0x18007ec28  _purecall
  [17] +0x88 0x18007ec28  _purecall
  [18] +0x90 0x18007ec28  _purecall
  [19] +0x98 0x18007ec28  _purecall

### Video::DeviceData  vtable=0x180158c30  slots=20  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058b10  (unexported) FUN_180058b10
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x18007ec28  _purecall
  [ 6] +0x30 0x18007ec28  _purecall
  [ 7] +0x38 0x18007ec28  _purecall
  [ 8] +0x40 0x18007ec28  _purecall
  [ 9] +0x48 0x18007ec28  _purecall
  [10] +0x50 0x18007ec28  _purecall
  [11] +0x58 0x18007ec28  _purecall
  [12] +0x60 0x18007ec28  _purecall
  [13] +0x68 0x18007ec28  _purecall
  [14] +0x70 0x18007ec28  _purecall
  [15] +0x78 0x18007ec28  _purecall
  [16] +0x80 0x18007ec28  _purecall
  [17] +0x88 0x18007ec28  _purecall
  [18] +0x90 0x18007ec28  _purecall
  [19] +0x98 0x18007ec28  _purecall

### Video::ImageBuffer  vtable=0x180154fc0  slots=10  sizeof=0x68  parent=Object::CompiledExternal
  [ 0] +0x00 0x18001ed20  (unexported) FUN_18001ed20
  [ 1] +0x08 0x18001e900  ?TypeName@ImageBuffer@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18001e8e0  ?Type@ImageBuffer@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f26e0  ?GetFileVersion@ImageBuffer@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::CompiledExternal
  [ 5] +0x28 0x1800f26f0  ?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18001ea80  ?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18001eab0  ?Destruct@ImageBuffer@Video@@UEAAXXZ
  [ 8] +0x40 0x18001eaa0  ?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ
  [ 9] +0x48 0x1800f2680  ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z

### Video::ImageBufferData  vtable=0x180158b50  slots=7  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058a10  (unexported) FUN_180058a10
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x18007ec28  _purecall
  [ 6] +0x30 0x18007ec28  _purecall

### Video::IndexBufferData  vtable=0x180158af8  slots=7  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058910  (unexported) FUN_180058910
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x18007ec28  _purecall
  [ 6] +0x30 0x18007ec28  _purecall

### Video::PixelProgram  vtable=0x180157ac0  slots=9  sizeof=0x58  parent=Video::ShaderProgram
  [ 0] +0x00 0x18004eb90  (unexported) FUN_18004eb90
  [ 1] +0x08 0x18004e8e0  ?TypeName@PixelProgram@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004e8c0  ?Type@PixelProgram@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f3a80  ?GetFileVersion@PixelProgram@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Video::ShaderProgram
  [ 5] +0x28 0x1800f3a90  ?Serialize@PixelProgram@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18004ea60  ?Reconstruct@PixelProgram@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18004ea90  ?Destruct@PixelProgram@Video@@UEAAXXZ
  [ 8] +0x40 0x18004ea80  ?Construct@PixelProgram@Video@@UEAAPEAVExternal@Object@@XZ

### Video::PixelProgramData  vtable=0x180158a78  slots=3  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058710  (unexported) FUN_180058710
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall

### Video::RenderBufferData  vtable=0x180158a28  slots=4  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058590  (unexported) FUN_180058590
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall

### Video::RenderState  vtable=0x180157720  slots=9  sizeof=0x80  parent=Object::External
  [ 0] +0x00 0x18004b9d0  (unexported) FUN_18004b9d0
  [ 1] +0x08 0x18004b570  ?TypeName@RenderState@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004b550  ?Type@RenderState@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f4c80  ?GetFileVersion@RenderState@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x1800f4c90  ?Serialize@RenderState@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18004b6f0  ?Reconstruct@RenderState@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18004b720  ?Destruct@RenderState@Video@@UEAAXXZ
  [ 8] +0x40 0x18004b710  ?Construct@RenderState@Video@@UEAAPEAVExternal@Object@@XZ

### Video::RenderStateData  vtable=0x180158a60  slots=3  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058690  (unexported) FUN_180058690
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall

### Video::ShaderProgram  vtable=0x180157b60  slots=8  sizeof=0x50  parent=Object::CompiledExternal
  [ 0] +0x00 0x18004e360  (unexported) FUN_18004e360
  [ 1] +0x08 0x18004e0c0  ?TypeName@ShaderProgram@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004e0a0  ?Type@ShaderProgram@Video@@UEBAIXZ
  [ 3] +0x18 0x18007ec28  _purecall   inherited-from Object::CompiledExternal
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::CompiledExternal
  [ 5] +0x28 0x1800f5110  ?Serialize@ShaderProgram@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18007ec28  _purecall   inherited-from Object::CompiledExternal
  [ 7] +0x38 0x18007ec28  _purecall   inherited-from Object::CompiledExternal

### Video::VertexBufferData  vtable=0x180158ac0  slots=7  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058890  (unexported) FUN_180058890
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall
  [ 3] +0x18 0x18007ec28  _purecall
  [ 4] +0x20 0x18007ec28  _purecall
  [ 5] +0x28 0x18007ec28  _purecall
  [ 6] +0x30 0x18007ec28  _purecall

### Video::VertexLayout  vtable=0x180157ba0  slots=9  sizeof=0x48  parent=Object::CompiledExternal
  [ 0] +0x00 0x18004e000  (unexported) Video_VertexLayout_DeletingDtor
  [ 1] +0x08 0x18004dcd0  ?TypeName@VertexLayout@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004dcb0  ?Type@VertexLayout@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f5e60  ?GetFileVersion@VertexLayout@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::CompiledExternal
  [ 5] +0x28 0x1800f5e70  ?Serialize@VertexLayout@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18004de50  ?Reconstruct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18004de80  ?Destruct@VertexLayout@Video@@UEAAXXZ
  [ 8] +0x40 0x18004de70  ?Construct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ

### Video::VertexLayoutData  vtable=0x180158aa8  slots=3  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058810  (unexported) FUN_180058810
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall

### Video::VertexProgram  vtable=0x180157b10  slots=9  sizeof=0x58  parent=Video::ShaderProgram
  [ 0] +0x00 0x18004e810  (unexported) FUN_18004e810
  [ 1] +0x08 0x18004e560  ?TypeName@VertexProgram@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004e540  ?Type@VertexProgram@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f6230  ?GetFileVersion@VertexProgram@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Video::ShaderProgram
  [ 5] +0x28 0x1800f6240  ?Serialize@VertexProgram@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18004e6e0  ?Reconstruct@VertexProgram@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18004e710  ?Destruct@VertexProgram@Video@@UEAAXXZ
  [ 8] +0x40 0x18004e700  ?Construct@VertexProgram@Video@@UEAAPEAVExternal@Object@@XZ

### Video::VertexProgramData  vtable=0x180158a90  slots=3  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058790  (unexported) FUN_180058790
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall

### Video::VideoProgram  vtable=0x180157a70  slots=9  sizeof=0x48  parent=Object::External
  [ 0] +0x00 0x18004efb0  (unexported) Video_VideoProgram_DeletingDtor
  [ 1] +0x08 0x18004ec50  ?TypeName@VideoProgram@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x18004ec30  ?Type@VideoProgram@Video@@UEBAIXZ
  [ 3] +0x18 0x1800f46a0  ?GetFileVersion@VideoProgram@Video@@UEBAHXZ
  [ 4] +0x20 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z   inherited-from Object::External
  [ 5] +0x28 0x1800f46b0  ?Serialize@VideoProgram@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x18004edd0  ?Reconstruct@VideoProgram@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x18004ee00  ?Destruct@VideoProgram@Video@@UEAAXXZ
  [ 8] +0x40 0x18004edf0  ?Construct@VideoProgram@Video@@UEAAPEAVExternal@Object@@XZ

### Video::VideoProgramData  vtable=0x180158a48  slots=3  sizeof=0x10  parent=ROOT (no base vtable shares a slot target)
  [ 0] +0x00 0x180058610  (unexported) FUN_180058610
  [ 1] +0x08 0x18007ec28  _purecall
  [ 2] +0x10 0x18007ec28  _purecall

@@@@@@ SECTION 2 -- the five parent vtables

### Object::Base  vtable=0x180154788  slots=3  sizeof=0x8  parent=ROOT (no base vtable shares a slot target)
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

### Object::Internal  vtable=0x180154720  slots=13  sizeof=0x10  parent=Object::Base
  [ 0] +0x00 0x180006440  (unexported) FUN_180006440
  [ 1] +0x08 0x1800064c0  ?TypeName@Internal@Object@@UEBAPEBDXZ
  [ 2] +0x10 0x1800064d0  ?Type@Internal@Object@@UEBAIXZ
  [ 3] +0x18 0x1800bfda0  ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z
  [ 4] +0x20 0x180001680  (unexported) FUN_180001680
  [ 5] +0x28 0x180001040  ?TypeName@Achievement@1@UEBAPEBDXZ
  [ 6] +0x30 0x180001020  ?Type@Achievement@1@UEBAIXZ
  [ 7] +0x38 0x18009e170  ?GetFileVersion@Achievement@1@UEBAHXZ
  [ 8] +0x40 0x1800066e0  ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
  [ 9] +0x48 0x18009e070  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z
  [10] +0x50 0x180001270  ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ
  [11] +0x58 0x180001370  ?Destruct@Achievement@1@UEAAXXZ
  [12] +0x60 0x1800012f0  ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ

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
count=36
0x1800116c0  ?_ParentTypeName@AnimationEvent@Graphics@@CAPEBDXZ         -> "Object::External"
0x1800162e0  ?_ParentTypeName@AnimationGroup@Graphics@@CAPEBDXZ         -> "Object::External"
0x180012c60  ?_ParentTypeName@EdgeMesh@Graphics@@CAPEBDXZ               -> "Object::External"
0x18001ef80  ?_ParentTypeName@FrameCopyImageBuffer@Graphics@@CAPEBDXZ   -> "GeneratedImageBuffer"
0x18001edb0  ?_ParentTypeName@GeneratedImageBuffer@Graphics@@CAPEBDXZ   -> "Video::ImageBuffer"
0x180012360  ?_ParentTypeName@GraphicsMesh@Graphics@@CAPEBDXZ           -> "Object::CompiledExternal"
0x18001e8d0  ?_ParentTypeName@ImageBuffer@Video@@CAPEBDXZ               -> "Object::CompiledExternal"
0x1800118a0  ?_ParentTypeName@Material@Graphics@@CAPEBDXZ               -> "Object::External"
0x1800027e0  ?_ParentTypeName@MaterialInstance@Graphics@@CAPEBDXZ       -> "Object::External"
0x18001ff00  ?_ParentTypeName@Particle@Graphics@@CAPEBDXZ               -> "Object::External"
0x1800203b0  ?_ParentTypeName@ParticleSystem@Graphics@@CAPEBDXZ         -> "Particle"
0x18004e8b0  ?_ParentTypeName@PixelProgram@Video@@CAPEBDXZ              -> "ShaderProgram"
0x1800221a0  ?_ParentTypeName@ReflectionImageBuffer@Graphics@@CAPEBDXZ  -> "GeneratedImageBuffer"
0x18004b540  ?_ParentTypeName@RenderState@Video@@CAPEBDXZ               -> "Object::External"
0x18004e090  ?_ParentTypeName@ShaderProgram@Video@@CAPEBDXZ             -> "Object::CompiledExternal"
0x18004dca0  ?_ParentTypeName@VertexLayout@Video@@CAPEBDXZ              -> "Object::CompiledExternal"
0x18004e530  ?_ParentTypeName@VertexProgram@Video@@CAPEBDXZ             -> "ShaderProgram"
0x18004ec20  ?_ParentTypeName@VideoProgram@Video@@CAPEBDXZ              -> "Object::External"
0x1800116b0  ?_TypeName@AnimationEvent@Graphics@@CAPEBDXZ               -> "AnimationEvent"
0x1800162d0  ?_TypeName@AnimationGroup@Graphics@@CAPEBDXZ               -> "AnimationGroup"
0x180012c50  ?_TypeName@EdgeMesh@Graphics@@CAPEBDXZ                     -> "EdgeMesh"
0x18001ef70  ?_TypeName@FrameCopyImageBuffer@Graphics@@CAPEBDXZ         -> "FrameCopyImageBuffer"
0x18001eda0  ?_TypeName@GeneratedImageBuffer@Graphics@@CAPEBDXZ         -> "GeneratedImageBuffer"
0x180012350  ?_TypeName@GraphicsMesh@Graphics@@CAPEBDXZ                 -> "GraphicsMesh"
0x18001e8c0  ?_TypeName@ImageBuffer@Video@@CAPEBDXZ                     -> "ImageBuffer"
0x180011890  ?_TypeName@Material@Graphics@@CAPEBDXZ                     -> "Material"
0x1800027d0  ?_TypeName@MaterialInstance@Graphics@@CAPEBDXZ             -> "MaterialInstance"
0x18001fef0  ?_TypeName@Particle@Graphics@@CAPEBDXZ                     -> "Particle"
0x1800203a0  ?_TypeName@ParticleSystem@Graphics@@CAPEBDXZ               -> "ParticleSystem"
0x18004e8a0  ?_TypeName@PixelProgram@Video@@CAPEBDXZ                    -> "PixelProgram"
0x180022190  ?_TypeName@ReflectionImageBuffer@Graphics@@CAPEBDXZ        -> "ReflectionImageBuffer"
0x18004b530  ?_TypeName@RenderState@Video@@CAPEBDXZ                     -> "RenderState"
0x18004e080  ?_TypeName@ShaderProgram@Video@@CAPEBDXZ                   -> "ShaderProgram"
0x18004dc90  ?_TypeName@VertexLayout@Video@@CAPEBDXZ                    -> "VertexLayout"
0x18004e520  ?_TypeName@VertexProgram@Video@@CAPEBDXZ                   -> "VertexProgram"
0x18004ec10  ?_TypeName@VideoProgram@Video@@CAPEBDXZ                    -> "VideoProgram"

@@@@@@ SECTION 4 -- hidden-retslot exports (?AV / ?AU); ?AW4 listed separately
retslot count=60
0x180003320  ?GetMaterialInstance@SpriteSheet@UI@@QEBA?AV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@XZ              Graphics::VMaterialInstance::?$ExternalPtr  [class by value]
0x18000d3e0  ?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z                                                        H::12::DisplayMode  [class by value]
0x180012730  ?GetMeshName@GraphicsMesh@Graphics@@QEBA?AVString@System@@XZ                                                   System::String  [class by value]
0x180012770  ?GetSubMeshName@GraphicsMesh@Graphics@@QEBA?AVString@System@@XZ                                                System::String  [class by value]
0x1800154a0  ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z                                              Math::Matrix  [class by value]
0x1800157c0  ?LocalAABB@Geometry@Graphics@@QEBA?AVBox@Volume@@XZ                                                            Volume::Box  [class by value]
0x180015800  ?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ                                                     Math::Transform  [class by value]
0x180015f10  ?GetName@Animation@Graphics@@QEBA?AVString@System@@XZ                                                          System::String  [class by value]
0x180015f50  ?GetMeshName@Animation@Graphics@@QEBA?AVString@System@@XZ                                                      System::String  [class by value]
0x180015f90  ?GetSourceName@Animation@Graphics@@QEBA?AVString@System@@XZ                                                    System::String  [class by value]
0x180016000  ?GetLength@Animation@Graphics@@QEBA?AVTime@System@@XZ                                                          System::Time  [class by value]
0x180016a20  ?GetMesh@MeshInstance@Graphics@@QEBA?AV?$ExternalPtr@VGraphicsMesh@Graphics@@@Object@@XZ                       Graphics::VGraphicsMesh::?$ExternalPtr  [class by value]
0x180016a40  ?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z                                              Math::Transform  [class by value]
0x180016c20  ?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@HM@Z                                             Math::Transform  [class by value]
0x180016ef0  ?RootAnimationOffset@MeshInstance@Graphics@@QEBA?AVVector3@Math@@XZ                                            Math::Vector3  [class by value]
0x180016fd0  ?GetVisualScale@MeshInstance@Graphics@@QEBA?AVVector4@Math@@XZ                                                 Math::Vector4  [class by value]
0x180017010  ?GetMaterialInstance@MeshInstance@Graphics@@QEBA?AV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@H@Z      Graphics::VMaterialInstance::?$ExternalPtr  [class by value]
0x1800174c0  ?GetMesh@EdgeMeshInstance@Graphics@@QEBA?AV?$ExternalPtr@VEdgeMesh@Graphics@@@Object@@XZ                       Graphics::VEdgeMesh::?$ExternalPtr  [class by value]
0x180017e10  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@M@Z                                                 Math::Transform3  [class by value]
0x180017e70  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@XZ                                                  Math::Transform3  [class by value]
0x180017ed0  ?GetProjection@Camera@Graphics@@QEBA?AVMatrix@Math@@XZ                                                         Math::Matrix  [class by value]
0x18001a0c0  ?GetViewport@Context@Video@@QEBA?AVViewport@2@XZ                                                               2::Viewport  [class by value]
0x18001afc0  ??B?$InterpolatedStateT@VBox@Volume@@@Graphics@@QEBA?AVBox@Volume@@XZ                                          Volume::Box  [class by value]
0x18001aff0  ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z                   Math::Transform  [class by value]
0x18001b020  ??B?$InterpolatedStateT@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@XZ                                  Math::Transform  [class by value]
0x18001b110  ?Current@?$InterpolatedStateT@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@XZ                            Math::Transform  [class by value]
0x1800201e0  ?GetParticle@ParticleInstance@Graphics@@QEBA?AV?$ExternalPtr@VParticle@Graphics@@@Object@@XZ                   Graphics::VParticle::?$ExternalPtr  [class by value]
0x180021740  ??B?$InterpolatedStateT@VVector3@Math@@@Graphics@@QEBA?AVVector3@Math@@XZ                                      Math::Vector3  [class by value]
0x18004e420  ?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z                                                 System::String  [class by value]
0x18004e4b0  ?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z                                                System::String  [class by value]
0x18004ee10  ?GetVertexLayout@VideoProgram@Video@@QEBA?AV?$ExternalPtr@VVertexLayout@Video@@@Object@@XZ                     Video::VVertexLayout::?$ExternalPtr  [class by value]
0x18004ee20  ?GetVertexProgram@VideoProgram@Video@@QEBA?AV?$ExternalPtr@VVertexProgram@Video@@@Object@@XZ                   Video::VVertexProgram::?$ExternalPtr  [class by value]
0x18004ee30  ?GetPixelProgram@VideoProgram@Video@@QEBA?AV?$ExternalPtr@VPixelProgram@Video@@@Object@@XZ                     Video::VPixelProgram::?$ExternalPtr  [class by value]
0x18004ee40  ?GetRenderState@VideoProgram@Video@@QEBA?AV?$ExternalPtr@VRenderState@Video@@@Object@@XZ                       Video::VRenderState::?$ExternalPtr  [class by value]
0x18005a730  ?GetRootAnimationOffset@AnimationManager@Graphics@@QEBA?AVVector3@Math@@H@Z                                    Math::Vector3  [class by value]
0x18005dad0  ?Lerp@?$InterpolatedState@VBox@Volume@@@Graphics@@QEBA?AVBox@Volume@@AEBVScalar@Math@@@Z                       Volume::Box  [class by value]
0x180062400  ?Lerp@?$InterpolatedState@VVector3@Math@@@Graphics@@QEBA?AVVector3@Math@@AEBVScalar@4@@Z                       Math::Vector3  [class by value]
0x1800a3840  ?GetInstanceTransform@EdgeMeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z                                      Math::Matrix  [class by value]
0x1800a43c0  ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z                                                 Math::Transform  [class by value]
0x1800f7b20  ?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z                                                    System::String  [class by value]
0x1800faaa0  ?GetRootPoseAtTime@Animation@Graphics@@QEBA?AVTransform@Math@@AEBVTime@System@@0@Z                             Math::Transform  [class by value]
0x1800fb4a0  ?GetBlendTime@AnimationGroup@Graphics@@QEBA?AVTime@System@@HH@Z                                                System::Time  [class by value]
0x1800fb4f0  ?GetDefaultBlendTime@AnimationGroup@Graphics@@QEBA?AVTime@System@@XZ                                           System::Time  [class by value]
0x1800fb870  ?GetRootPose@AnimationManager@Graphics@@QEBA?AVTransform@Math@@H@Z                                             Math::Transform  [class by value]
0x1800fe320  ?InstancedRootAnimationOffset@AnimationState@Graphics@@QEBA?AVVector3@Math@@XZ                                 Math::Vector3  [class by value]
0x1800fe3e0  ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ                                                         Volume::Frustum  [class by value]
0x180101200  ?GetSubTransform@MeshInstance@Graphics@@UEBA?AVTransform@Math@@H@Z                                             Math::Transform  [class by value]
0x180101280  ?GetInstanceTransform@MeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z                                          Math::Matrix  [class by value]
0x180102830  ?GetConstantName@Material@Graphics@@QEBA?AVString@System@@HH@Z                                                 System::String  [class by value]
0x180102900  ?GetTextureName@Material@Graphics@@QEBA?AVString@System@@HH@Z                                                  System::String  [class by value]
0x180102980  ?GetRenderState@Material@Graphics@@QEBA?AV?$ExternalPtr@VRenderState@Video@@@Object@@H@Z                       Video::VRenderState::?$ExternalPtr  [class by value]
0x180103100  ?GetTexture@MaterialInstance@Graphics@@QEBA?AV?$ExternalPtr@VImageBuffer@Video@@@Object@@H@Z                   Video::VImageBuffer::?$ExternalPtr  [class by value]
0x180103120  ?GetTexture@MaterialInstance@Graphics@@QEBA?AV?$ExternalPtr@VImageBuffer@Video@@@Object@@AEBVString@System@@@Z Video::VImageBuffer::?$ExternalPtr  [class by value]
0x1801047f0  ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z                                      Math::Matrix  [class by value]
0x180105fb0  ?GetSpotCamera@SpotLight@Graphics@@QEBA?AVCamera@2@AEAVRenderer@2@@Z                                           2::AEAVRenderer::2::Camera  [class by value]
0x180107010  ?GetSunCamera@SunLight@Graphics@@QEBA?AVCamera@2@AEAVRenderer@2@@Z                                             2::AEAVRenderer::2::Camera  [class by value]
0x18011ca00  ?GetScreenScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ                                                  Video::Viewport  [class by value]
0x18011ca30  ?GetScissor@SpriteContext@UI@@QEBA?AVViewport@Video@@XZ                                                        Video::Viewport  [class by value]
0x18011d010  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z                                                    Video::Viewport  [class by value]
0x18011d050  ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z                                                 Video::Viewport  [class by value]
-- NOT retslot: enum by value, returned in EAX. count=3
0x18001eb50  ?GetFormat@ImageBuffer@Video@@QEBA?AW4Format@12@XZ                                                             12::Format  [enum by value]
0x1800a21c0  ?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z                     Object::External::CompileResult  [enum by value]
0x1800fb690  ?Compile@AnimationGroup@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z               Object::External::CompileResult  [enum by value]

@@@@@@ SECTION 5 -- exported members of the non-polymorphic classes

##### Video::Context   exports=85
    0x180018560  ??0Context@Video@@QEAA@AEBV01@@Z
    0x1800f1280  ??0Context@Video@@QEAA@PEAVDevice@1@@Z
    0x1800f1820  ??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z
    0x1800f16e0  ??1Context@Video@@QEAA@XZ
    0x180018480  ??2Context@Video@@SAPEAX_K@Z
    0x1800184f0  ??2Context@Video@@SAPEAX_KPEAX@Z
    0x180018500  ??3Context@Video@@SAXPEAX@Z
    0x180018f90  ??4Context@Video@@QEAAAEAV01@AEBV01@@Z
    0x18001adc0  ?AddIndex@Context@Video@@QEAAXG@Z
    0x18001ade0  ?AddIndices@Context@Video@@QEAAXHPEBG@Z
    0x18001ad40  ?AddVertex@Context@Video@@QEAAXPEBX@Z
    0x18001ad70  ?AddVertices@Context@Video@@QEAAXHPEBX@Z
    0x1800ef3d0  ?AssignConstants@Context@Video@@AEAAXXZ
    0x1800f2340  ?BeginCommands@Context@Video@@QEAA_NXZ
    0x1800f2170  ?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z
    0x1800f2130  ?BeginInstancedIndexedVertices@Context@Video@@QEAA_NHHHH@Z
    0x1800f21b0  ?BeginVertices@Context@Video@@QEAA_NHH@Z
    0x1800f22e0  ?BindIndexNone@Context@Video@@AEAAXXZ
    0x1800f2280  ?BindRenderBuffer@Context@Video@@AEAAXXZ
    0x1800ef060  ?BindResources@Context@Video@@AEAAXXZ
    0x1800f2320  ?BindScissor@Context@Video@@AEAAXXZ
    0x1800f2260  ?BindTextureNone@Context@Video@@AEAAXH@Z
    0x1800ef330  ?BindTextures@Context@Video@@AEAAXXZ
    0x1800f22a0  ?BindTransforms@Context@Video@@AEAAXXZ
    0x1800f22c0  ?BindVertexNone@Context@Video@@AEAAXH@Z
    0x1800f2300  ?BindViewport@Context@Video@@AEAAXXZ
    0x1800f23f0  ?Clear@Context@Video@@QEAAXIIMH@Z
    0x1800f2090  ?ClearResources@Context@Video@@QEAAXXZ
    0x1800f2410  ?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z
    0x1800f2360  ?EndCommands@Context@Video@@QEAAXXZ
    0x1800f2150  ?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
    0x1800f2110  ?EndInstancedIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
    0x1800f2190  ?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
    0x1800f2380  ?Finish@Context@Video@@QEAAXXZ
    0x180019eb0  ?ForceRenderStateDirty@Context@Video@@QEAAXI@Z
    0x18001aec0  ?GetBoneConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x1800f1090  ?GetConstantBufferForName@Context@Video@@QEBAPEBVConstantBuffer@2@PEBD@Z
    0x18001ae40  ?GetDefaultRenderBuffer@Context@Video@@QEAAPEAVRenderBuffer@2@XZ
    0x18001ae80  ?GetDrawsSubmitted@Context@Video@@QEBAHXZ
    0x18001ae50  ?GetFrameIndex@Context@Video@@QEBAHXZ
    0x18001aea0  ?GetGlobalConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x18001aed0  ?GetInstanceConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x18001aee0  ?GetLightConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x18001aef0  ?GetMaterialConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x1800f21d0  ?GetMaxDynamicInstances@Context@Video@@QEBAHHHH@Z
    0x18001ae90  ?GetObjectsSubmitted@Context@Video@@QEBAHXZ
    0x18001af00  ?GetPixelConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x180019ec0  ?GetRenderBuffer@Context@Video@@QEBAPEBVRenderBuffer@2@XZ
    0x18001ae30  ?GetShaderDetail@Context@Video@@QEBAHXZ
    0x18001aeb0  ?GetTransformConstantBuffer@Context@Video@@QEBAAEBVConstantBuffer@2@XZ
    0x18001ae60  ?GetTrianglesSubmitted@Context@Video@@QEBAHXZ
    0x180019f80  ?GetVertexBuffer@Context@Video@@QEBAPEBVVertexBuffer@2@H@Z
    0x180019fa0  ?GetVertexBufferOffset@Context@Video@@QEBAHH@Z
    0x180019f90  ?GetVertexBufferStride@Context@Video@@QEBAHH@Z
    0x18001ae70  ?GetVerticesSubmitted@Context@Video@@QEBAHXZ
    0x18001a0c0  ?GetViewport@Context@Video@@QEBA?AVViewport@2@XZ
    0x1800f21f0  ?PlatformConstruct@Context@Video@@AEAAXXZ
    0x1800f2240  ?PlatformDestruct@Context@Video@@AEAAXXZ
    0x1800f1e50  ?Reset@Context@Video@@QEAAXXZ
    0x18001a780  ?SetBones@Context@Video@@QEAAXHPEBVMatrix@Math@@@Z
    0x18001a150  ?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z
    0x18001a8e0  ?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z
    0x1800ef3c0  ?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z
    0x18001a360  ?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z
    0x180019ed0  ?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z
    0x180019fb0  ?SetIndexBuffer@Context@Video@@QEAAXPEBVIndexBuffer@2@H@Z
    0x18001a880  ?SetInverseScale@Context@Video@@QEAAXMMMM@Z
    0x18001a950  ?SetLight@Context@Video@@QEAAXAEBVVector3@Math@@0AEBVVector4@4@1MMH@Z
    0x18001acb0  ?SetMaterialParameter@Context@Video@@QEAAXHAEBVVector4@Math@@@Z
    0x18001a020  ?SetPixelProgram@Context@Video@@QEAAXPEBVPixelProgram@2@@Z
    0x1800f2100  ?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z
    0x180019fe0  ?SetRenderState@Context@Video@@QEAAXPEBVRenderState@2@@Z
    0x18001a0f0  ?SetScissor@Context@Video@@QEAAXAEBVViewport@2@@Z
    0x18001ae20  ?SetShaderDetail@Context@Video@@QEAAXH@Z
    0x18001ac20  ?SetShadowProjection@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z
    0x18001ab80  ?SetTextureProjection@Context@Video@@QEAAXAEBVMatrix@Math@@@Z
    0x18001a330  ?SetTime@Context@Video@@QEAAXM@Z
    0x18001a6b0  ?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z
    0x18001a400  ?SetTransformScale@Context@Video@@QEAAXHAEBVMatrix@Math@@AEBVVector4@4@@Z
    0x18001a2e0  ?SetUserPosition@Context@Video@@QEAAXAEBVVector4@Math@@@Z
    0x180019f20  ?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z
    0x180019f00  ?SetVertexLayout@Context@Video@@QEAAXPEBVVertexLayout@2@@Z
    0x18001a000  ?SetVertexProgram@Context@Video@@QEAAXPEBVVertexProgram@2@@Z
    0x18001a040  ?SetVideoProgram@Context@Video@@QEAAXPEBVVideoProgram@2@@Z
    0x18001a060  ?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z

##### Video::Device   exports=95
    0x18000cc70  ??0Adapter@Device@Video@@QEAA@AEBU012@@Z
    0x1800f7100  ??0Adapter@Device@Video@@QEAA@XZ
    0x18000ced0  ??0Device@Video@@QEAA@AEBV01@@Z
    0x1800f7160  ??0Device@Video@@QEAA@W4DeviceType@01@@Z
    0x18000cba0  ??0DisplayMode@Device@Video@@QEAA@AEBV012@@Z
    0x18000d300  ??0DisplayMode@Device@Video@@QEAA@HH@Z
    0x18000ce10  ??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z
    0x18000cd90  ??0GraphicsInterface@Device@Video@@QEAA@XZ
    0x18000d270  ??0RefreshRate@Device@Video@@QEAA@HH@Z
    0x18000cc10  ??1Adapter@Device@Video@@QEAA@XZ
    0x1800f7290  ??1Device@Video@@QEAA@XZ
    0x18000cb80  ??1DisplayMode@Device@Video@@QEAA@XZ
    0x18000cdc0  ??1GraphicsInterface@Device@Video@@QEAA@XZ
    0x18000c9b0  ??2Device@Video@@SAPEAX_K@Z
    0x18000ca20  ??2Device@Video@@SAPEAX_KPEAX@Z
    0x18000ca30  ??3Device@Video@@SAXPEAX@Z
    0x18000cd00  ??4Adapter@Device@Video@@QEAAAEAU012@AEBU012@@Z
    0x18000d0b0  ??4Device@Video@@QEAAAEAV01@AEBV01@@Z
    0x18000cbe0  ??4DisplayMode@Device@Video@@QEAAAEAV012@AEBV012@@Z
    0x18000ce60  ??4GraphicsInterface@Device@Video@@QEAAAEAU012@AEBU012@@Z
    0x18000cb70  ??4RefreshRate@Device@Video@@QEAAAEAV012@AEBV012@@Z
    0x18000d350  ??8DisplayMode@Device@Video@@QEBA_NAEBV012@@Z
    0x18000d2b0  ??8RefreshRate@Device@Video@@QEBA_NAEBV012@@Z
    0x18000d330  ??MDisplayMode@Device@Video@@QEBA_NAEBV012@@Z
    0x18000d280  ??MRefreshRate@Device@Video@@QEBA_NAEBV012@@Z
    0x1800f7e20  ?AdjustDisplay@Device@Video@@QEAAXXZ
    0x1800f8110  ?ApplyChangedSettings@Device@Video@@QEAA_NXZ
    0x1800f7cf0  ?CompareDisplayMode@Device@Video@@CAHPEBX0@Z
    0x1800f85f0  ?CreateConstantBufferData@Device@Video@@QEAAPEAVConstantBufferData@2@PEAVConstantBuffer@2@@Z
    0x1800f85d0  ?CreateContextData@Device@Video@@QEAAPEAVContextData@2@PEAVContext@2@@Z
    0x1800f85b0  ?CreateImageBufferData@Device@Video@@QEAAPEAVImageBufferData@2@PEAVImageBuffer@2@@Z
    0x1800f8590  ?CreateIndexBufferData@Device@Video@@QEAAPEAVIndexBufferData@2@PEAVIndexBuffer@2@@Z
    0x1800f8570  ?CreatePixelProgramData@Device@Video@@QEAAPEAVPixelProgramData@2@PEAVPixelProgram@2@@Z
    0x1800f8550  ?CreateRenderBufferData@Device@Video@@QEAAPEAVRenderBufferData@2@PEAVRenderBuffer@2@@Z
    0x1800f8510  ?CreateRenderStateData@Device@Video@@QEAAPEAVRenderStateData@2@PEAVRenderState@2@@Z
    0x1800f84f0  ?CreateVertexBufferData@Device@Video@@QEAAPEAVVertexBufferData@2@PEAVVertexBuffer@2@@Z
    0x1800f84d0  ?CreateVertexLayoutData@Device@Video@@QEAAPEAVVertexLayoutData@2@PEAVVertexLayout@2@@Z
    0x1800f84b0  ?CreateVertexProgramData@Device@Video@@QEAAPEAVVertexProgramData@2@PEAVVertexProgram@2@@Z
    0x1800f7930  ?CreateVideoInterface@Device@Video@@QEAA_NH@Z
    0x1800f8530  ?CreateVideoProgramData@Device@Video@@QEAAPEAVVideoProgramData@2@PEAVVideoProgram@2@@Z
    0x1800f8610  ?EnumerateVideoModes@Device@Video@@QEAA_NXZ
    0x1800f7310  ?FindDisplayMode@Adapter@Device@Video@@QEBAHAEBVDisplayMode@23@@Z
    0x1800f7020  ?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z
    0x1800f6f90  ?FindOrAddRefresh@DisplayMode@Device@Video@@QEAAXAEBVRefreshRate@23@@Z
    0x1800f6fe0  ?FindRefresh@DisplayMode@Device@Video@@QEBAHAEBVRefreshRate@23@@Z
    0x18000d260  ?Get@Device@Video@@SAPEAV12@XZ
    0x1800f81c0  ?GetCenterPixelOffset@Device@Video@@QEBAMXZ
    0x18000d440  ?GetCurrentDisplayMode@Device@Video@@QEBAHXZ
    0x1800f7b20  ?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z
    0x18000cb20  ?GetData@Device@Video@@QEAAPEAVDeviceData@2@XZ
    0x1800f7600  ?GetDefaultRefreshRate@Device@Video@@QEAAHAEBVDisplayMode@12@H@Z
    0x1800f7540  ?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z
    0x18000d2e0  ?GetDenominator@RefreshRate@Device@Video@@QEBAHXZ
    0x18000d3e0  ?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z
    0x18000d3d0  ?GetDisplayModeCount@Device@Video@@QEBAHXZ
    0x18000d3a0  ?GetHeight@Device@Video@@QEBAHXZ
    0x18000d380  ?GetHeight@DisplayMode@Device@Video@@QEBAHXZ
    0x18000d2f0  ?GetIntRefresh@RefreshRate@Device@Video@@QEBAHXZ
    0x18000d3c0  ?GetInterface@Device@Video@@QEBAAEBUGraphicsInterface@12@H@Z
    0x18000d3b0  ?GetInterfaceCount@Device@Video@@QEBAHXZ
    0x18000cb30  ?GetInterfaceIndex@Device@Video@@QEBAHXZ
    0x18000cb60  ?GetInterfaceName@Device@Video@@QEBAPEBDH@Z
    0x18000cb10  ?GetMaxAnisotropy@Device@Video@@QEBAHXZ
    0x18000d450  ?GetMultisampleModeCount@Device@Video@@QEBAHXZ
    0x18000d2d0  ?GetNumerator@RefreshRate@Device@Video@@QEBAHXZ
    0x18000d390  ?GetWidth@Device@Video@@QEBAHXZ
    0x18000d370  ?GetWidth@DisplayMode@Device@Video@@QEBAHXZ
    0x18000caf0  ?GetWindow@Device@Video@@QEAAPEAUHWND__@@XZ
    0x1800f79f0  ?InitializeInterfaces@Device@Video@@QEAAXXZ
    0x18000cb40  ?IsInterfaceAvailable@Device@Video@@QEBA_NH@Z
    0x18000d460  ?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z
    0x18000ca90  ?Lock@Device@Video@@QEAAXXZ
    0x1800f7640  ?LogDisplay@Device@Video@@QEBAXXZ
    0x1800f7d90  ?MoveWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z
    0x18000cb00  ?NeedsRestart@Device@Video@@QEAA_NXZ
    0x1800f8810  ?NotifyBackgroundLoad@Device@Video@@QEAAX_N@Z
    0x1800f81e0  ?PlatformConstruct@Device@Video@@AEAAXXZ
    0x1800f8220  ?PlatformDestruct@Device@Video@@AEAAXXZ
    0x1800f83f0  ?PlatformShutdown@Device@Video@@AEAAXXZ
    0x1800f8260  ?PlatformStartup@Device@Video@@AEAA_NXZ
    0x1800f80b0  ?RecreateDynamic@Device@Video@@QEAAXXZ
    0x1800f80e0  ?ReleaseDynamic@Device@Video@@QEAAXXZ
    0x1800f87b0  ?ReleaseVideoModes@Device@Video@@QEAAXXZ
    0x1800f81a0  ?RequiresColorTarget@Device@Video@@QEBA_NXZ
    0x1800f7350  ?SetCursor@Device@Video@@QEAAXV?$ExternalPtr@VImageBuffer@Video@@@Object@@HH@Z
    0x1800f7150  ?SetVideoErrorFuncs@Device@Video@@QEAAXP6AXXZ0@Z
    0x1800f7280  ?Shutdown@Device@Video@@QEAAXXZ
    0x1800f7d20  ?SizeWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z
    0x1800f7270  ?Startup@Device@Video@@QEAA_NXZ
    0x18000cac0  ?Unlock@Device@Video@@QEAAXXZ
    0x1800f8180  ?UsesZeroToOneClipSpace@Device@Video@@QEBA_NXZ
    0x1800f7370  ?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z
    0x1800f8830  ?ValidateConfiguration@Device@Video@@QEAA_NXZ
    0x18000cae0  ?VerifyLock@Device@Video@@QEAAXXZ
    0x1801fe7a8  ?_singleton@Device@Video@@0PEAV12@EA

##### Video::Configuration   exports=46
    0x1800f6f10  ??0Configuration@Video@@QEAA@XZ
    0x1800f6f80  ??1Configuration@Video@@QEAA@XZ
    0x18000d520  ??4Configuration@Video@@QEAAAEAV01@AEBV01@@Z
    0x18000d590  ?Get@Configuration@Video@@SAPEAV12@XZ
    0x18000d5c0  ?GetAdapter@Configuration@Video@@QEBAHXZ
    0x18000d5e0  ?GetHeight@Configuration@Video@@QEBAHXZ
    0x18000d5b0  ?GetInterface@Configuration@Video@@QEBAHXZ
    0x18000d630  ?GetMultisample@Configuration@Video@@QEBAHXZ
    0x18000d6b0  ?GetReflectionDetail@Configuration@Video@@QEBAHXZ
    0x18000d660  ?GetRefreshDenominator@Configuration@Video@@QEBAHXZ
    0x18000d650  ?GetRefreshNumerator@Configuration@Video@@QEBAHXZ
    0x18000d6a0  ?GetShaderDetail@Configuration@Video@@QEBAHXZ
    0x18000d690  ?GetShadowMapScalar@Configuration@Video@@QEBAHXZ
    0x18000d5a0  ?GetShowSettings@Configuration@Video@@QEBA_NXZ
    0x18000d680  ?GetTextureFilter@Configuration@Video@@QEBAHXZ
    0x18000d5d0  ?GetWidth@Configuration@Video@@QEBAHXZ
    0x18000d620  ?GetWindowHeight@Configuration@Video@@QEBAHXZ
    0x18000d610  ?GetWindowWidth@Configuration@Video@@QEBAHXZ
    0x18000d5f0  ?GetWindowX@Configuration@Video@@QEBAHXZ
    0x18000d600  ?GetWindowY@Configuration@Video@@QEBAHXZ
    0x18000d6c0  ?HideWindow@Configuration@Video@@QEBA_NXZ
    0x18000d640  ?IsFullscreen@Configuration@Video@@QEBA_NXZ
    0x18000d670  ?IsVSync@Configuration@Video@@QEBA_NXZ
    0x1800f6d10  ?Save@Configuration@Video@@QEAAXXZ
    0x18000d6f0  ?SetAdapter@Configuration@Video@@QEAAXH@Z
    0x18000d770  ?SetFullscreen@Configuration@Video@@QEAAX_N@Z
    0x18000d710  ?SetHeight@Configuration@Video@@QEAAXH@Z
    0x18000d7f0  ?SetHideWindow@Configuration@Video@@QEAAX_N@Z
    0x18000d6e0  ?SetInterface@Configuration@Video@@QEAAXH@Z
    0x18000d760  ?SetMultisample@Configuration@Video@@QEAAXH@Z
    0x18000d7e0  ?SetReflectionDetail@Configuration@Video@@QEAAXH@Z
    0x18000d790  ?SetRefreshDenominator@Configuration@Video@@QEAAXH@Z
    0x18000d780  ?SetRefreshNumerator@Configuration@Video@@QEAAXH@Z
    0x18000d7d0  ?SetShaderDetail@Configuration@Video@@QEAAXH@Z
    0x18000d7c0  ?SetShadowMapScalar@Configuration@Video@@QEAAXH@Z
    0x18000d6d0  ?SetShowSettings@Configuration@Video@@QEAAX_N@Z
    0x18000d7b0  ?SetTextureFilter@Configuration@Video@@QEAAXH@Z
    0x18000d7a0  ?SetVSync@Configuration@Video@@QEAAX_N@Z
    0x18000d700  ?SetWidth@Configuration@Video@@QEAAXH@Z
    0x18000d750  ?SetWindowHeight@Configuration@Video@@QEAAXH@Z
    0x18000d740  ?SetWindowWidth@Configuration@Video@@QEAAXH@Z
    0x18000d720  ?SetWindowX@Configuration@Video@@QEAAXH@Z
    0x18000d730  ?SetWindowY@Configuration@Video@@QEAAXH@Z
    0x1800f6670  ?Shutdown@Configuration@Video@@QEAAXXZ
    0x1800f6680  ?Startup@Configuration@Video@@QEAA_NXZ
    0x1801fe7a0  ?_singleton@Configuration@Video@@0PEAV12@EA

##### Video::RenderBuffer   exports=22
    0x1800f3ee0  ??0RenderBuffer@Video@@QEAA@XZ
    0x1800f4020  ??1RenderBuffer@Video@@QEAA@XZ
    0x18001f2e0  ??2RenderBuffer@Video@@SAPEAX_K@Z
    0x18001f350  ??2RenderBuffer@Video@@SAPEAX_KPEAX@Z
    0x18001f360  ??3RenderBuffer@Video@@SAXPEAX@Z
    0x18001f3d0  ??4RenderBuffer@Video@@QEAAAEAV01@AEBV01@@Z
    0x1800f4300  ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z
    0x18001f430  ?ColorBuffer@RenderBuffer@Video@@QEBAPEBVImageBuffer@2@XZ
    0x1800f3f10  ?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z
    0x18001f440  ?DepthBuffer@RenderBuffer@Video@@QEBAPEBVImageBuffer@2@XZ
    0x18001f3c0  ?GetData@RenderBuffer@Video@@QEBAPEBVRenderBufferData@2@XZ
    0x18001f420  ?Height@RenderBuffer@Video@@QEBAHXZ
    0x1800f4320  ?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ
    0x1800f44f0  ?PlatformCreate@RenderBuffer@Video@@AEAA_NXZ
    0x1800f4400  ?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ
    0x1800f4170  ?Recreate@RenderBuffer@Video@@QEAAXXZ
    0x1800f4030  ?RecreateDynamic@RenderBuffer@Video@@SAXXZ
    0x1800f42d0  ?Release@RenderBuffer@Video@@QEAAXXZ
    0x1800f40c0  ?ReleaseDynamic@RenderBuffer@Video@@SAXXZ
    0x1800f4510  ?Resolve@RenderBuffer@Video@@QEBAXAEAVContext@2@PEAV12@_N@Z
    0x18001f410  ?Width@RenderBuffer@Video@@QEBAHXZ
    0x1801fe790  ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA

##### Video::IndexBuffer   exports=15
    0x1800f3490  ??0IndexBuffer@Video@@QEAA@XZ
    0x1800f3480  ??1IndexBuffer@Video@@QEAA@XZ
    0x180011e10  ??4IndexBuffer@Video@@QEAAAEAV01@AEBV01@@Z
    0x1800f3860  ?Bind@IndexBuffer@Video@@QEBAXAEAVContext@2@@Z
    0x1800f3880  ?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z
    0x180011e00  ?GetSize@IndexBuffer@Video@@QEBAHXZ
    0x1800f34e0  ?Lock@IndexBuffer@Video@@QEAAPEAXHAEAH@Z
    0x1800f3680  ?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ
    0x1800f3760  ?PlatformDestruct@IndexBuffer@Video@@QEAAXXZ
    0x1800f3500  ?RecreateDynamic@IndexBuffer@Video@@SAXXZ
    0x1800f35d0  ?ReleaseDynamic@IndexBuffer@Video@@SAXXZ
    0x1800f38c0  ?Serialize@IndexBuffer@Video@@QEAAXAEAVSerializer@IO@@@Z
    0x1800f38a0  ?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z
    0x1800f34c0  ?Unlock@IndexBuffer@Video@@QEAAXXZ
    0x1801fe788  ?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA

##### Video::VertexBuffer   exports=16
    0x1800f55f0  ??0VertexBuffer@Video@@QEAA@XZ
    0x1800f55e0  ??1VertexBuffer@Video@@QEAA@XZ
    0x180011dd0  ??4VertexBuffer@Video@@QEAAAEAV01@AEBV01@@Z
    0x1800f59c0  ?Bind@VertexBuffer@Video@@QEBAXAEAVContext@2@HII@Z
    0x1800f59e0  ?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z
    0x180011dc0  ?GetData@VertexBuffer@Video@@QEBAPEBVVertexBufferData@2@XZ
    0x180011db0  ?GetSize@VertexBuffer@Video@@QEBAHXZ
    0x1800f5640  ?Lock@VertexBuffer@Video@@QEAAPEAXHAEAH@Z
    0x1800f57e0  ?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ
    0x1800f58c0  ?PlatformDestruct@VertexBuffer@Video@@QEAAXXZ
    0x1800f5660  ?RecreateDynamic@VertexBuffer@Video@@SAXXZ
    0x1800f5730  ?ReleaseDynamic@VertexBuffer@Video@@SAXXZ
    0x1800f5a20  ?Serialize@VertexBuffer@Video@@QEAAXAEAVSerializer@IO@@@Z
    0x1800f5a00  ?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z
    0x1800f5620  ?Unlock@VertexBuffer@Video@@QEAAXXZ
    0x1801fe798  ?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA

##### Video::ConstantBuffer   exports=18
    0x180018300  ??0ConstantBuffer@Video@@QEAA@AEBV01@@Z
    0x1800eedf0  ??0ConstantBuffer@Video@@QEAA@PEAVDevice@1@@Z
    0x1800eeee0  ??1ConstantBuffer@Video@@QEAA@XZ
    0x1800183a0  ??4ConstantBuffer@Video@@QEAAAEAV01@AEBV01@@Z
    0x1800eec40  ?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z
    0x1800eed10  ?BeginConstants@ConstantBuffer@Video@@QEAAXAEBVString@System@@0HH_N1H@Z
    0x1800eef70  ?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z
    0x1800eebf0  ?EndConstants@ConstantBuffer@Video@@QEAAXH@Z
    0x1800182b0  ?GetConstant@ConstantBuffer@Video@@QEBAAEBUConstantInfo@12@H@Z
    0x1800182a0  ?GetConstantCount@ConstantBuffer@Video@@QEBAHXZ
    0x1800eef90  ?GetNextUploadIndex@ConstantBuffer@Video@@QEBAHXZ
    0x1800182d0  ?GetOrdinalIndex@ConstantBuffer@Video@@QEBAHXZ
    0x1800182e0  ?GetPrefix@ConstantBuffer@Video@@QEBAAEBVString@System@@XZ
    0x1800182f0  ?GetTypeName@ConstantBuffer@Video@@QEBAAEBVString@System@@XZ
    0x1800182c0  ?GetUploadIndex@ConstantBuffer@Video@@QEBAHXZ
    0x1800eefb0  ?PlatformConstruct@ConstantBuffer@Video@@AEAAXXZ
    0x1800ef010  ?PlatformCreate@ConstantBuffer@Video@@AEAA_NXZ
    0x1800eeff0  ?PlatformDestruct@ConstantBuffer@Video@@AEAAXXZ

##### Video::Viewport   exports=5
    0x180018220  ??0Viewport@Video@@QEAA@HHHH@Z
    0x180018210  ??0Viewport@Video@@QEAA@XZ
    0x1800181f0  ??4Viewport@Video@@QEAAAEAV01@AEBV01@@Z
    0x180018240  ??8Viewport@Video@@QEBA_NAEBV01@@Z
    0x180018270  ??9Viewport@Video@@QEBA_NAEBV01@@Z

##### Graphics::Camera   exports=13
    0x180017a70  ??0Camera@Graphics@@QEAA@AEBV01@@Z
    0x1800fe960  ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z
    0x1800fe7f0  ??0Camera@Graphics@@QEAA@XZ
    0x180017c40  ??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x180017e10  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@M@Z
    0x180017e70  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@XZ
    0x1800fe3e0  ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ
    0x180017ed0  ?GetProjection@Camera@Graphics@@QEBA?AVMatrix@Math@@XZ
    0x1800fe480  ?IsMirrored@Camera@Graphics@@QEBA_NXZ
    0x1800feb00  ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z
    0x1800fe5c0  ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z
    0x1800fe550  ?SetZUp@Camera@Graphics@@QEAAX_N@Z
    0x1800fe560  ?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z

##### Graphics::Scene   exports=15
    0x18005e290  ??0Scene@Graphics@@QEAA@AEBV01@@Z
    0x180102730  ??0Scene@Graphics@@QEAA@XZ
    0x1801026c0  ??1Scene@Graphics@@QEAA@XZ
    0x18005e120  ??2Scene@Graphics@@SAPEAX_K@Z
    0x18005e190  ??2Scene@Graphics@@SAPEAX_KPEAX@Z
    0x18005e1a0  ??3Scene@Graphics@@SAXPEAX@Z
    0x18005e2e0  ??4Scene@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x1801017a0  ?AddCulledDrawItems@Scene@Graphics@@AEBAXAEAVRenderer@2@@Z
    0x1801023b0  ?AddDrawItems@Scene@Graphics@@QEBAXAEAVRenderer@2@@Z
    0x180102330  ?AddGlobalDrawItems@Scene@Graphics@@AEBAXAEAVRenderer@2@@Z
    0x180102540  ?AddGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z
    0x180102670  ?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z
    0x1801023f0  ?Clean@Scene@Graphics@@QEAAXXZ
    0x1801024c0  ?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z
    0x180102600  ?RemoveObject@Scene@Graphics@@QEAAXI@Z

##### Graphics::Renderer   exports=25
    0x1800180e0  ??0Renderer@Graphics@@QEAA@AEBV01@@Z
    0x180105180  ??0Renderer@Graphics@@QEAA@XZ
    0x1800180a0  ??1Renderer@Graphics@@QEAA@XZ
    0x180018180  ??4Renderer@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x1801057b0  ?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z
    0x1801051d0  ?AddLight@Renderer@Graphics@@QEAAXPEBVLight@2@@Z
    0x180105700  ?AddScene@Renderer@Graphics@@QEAAXPEBVScene@2@PEBVRenderBuffer@Video@@AEBVCamera@2@HHHI@Z
    0x180104fd0  ?BeginDrawGroup@Renderer@Graphics@@QEAAHPEBVRenderBuffer@Video@@AEBVCamera@2@1HI@Z
    0x180104bb0  ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
    0x180105240  ?DrawItemList@Renderer@Graphics@@QEBAXAEAVContext@Video@@HHH@Z
    0x180104f50  ?EndDrawGroup@Renderer@Graphics@@QEAAXH@Z
    0x180018040  ?GetAlphaT@Renderer@Graphics@@QEBAMXZ
    0x180017fb0  ?GetCamera@Renderer@Graphics@@QEAAAEBVCamera@2@H@Z
    0x180017f70  ?GetCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ
    0x180017fd0  ?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@H@Z
    0x180017f90  ?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ
    0x180018030  ?GetCurrentSceneGroup@Renderer@Graphics@@QEBAHXZ
    0x180018070  ?GetDetailId@Renderer@Graphics@@QEBAHXZ
    0x180018080  ?GetFrameIndex@Renderer@Graphics@@QEBAHXZ
    0x180017ff0  ?GetMaterialTypeFlags@Renderer@Graphics@@QEBAIXZ
    0x180018090  ?GetSceneGroup@Renderer@Graphics@@QEBAAEBUSceneGroup@12@H@Z
    0x180018010  ?GetShader@Renderer@Graphics@@QEBAHXZ
    0x180018060  ?GetShaderDetail@Renderer@Graphics@@QEBAHXZ
    0x180105110  ?Reset@Renderer@Graphics@@QEAAXXZ
    0x180018050  ?SetShaderDetail@Renderer@Graphics@@QEAAXH@Z

##### Graphics::Animation   exports=22
    0x1800160a0  ??0Animation@Graphics@@QEAA@AEBV01@@Z
    0x1800fb1d0  ??0Animation@Graphics@@QEAA@XZ
    0x1800fb290  ??1Animation@Graphics@@QEAA@XZ
    0x180016190  ??4Animation@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x1800fa9a0  ?ApplyEvents@Animation@Graphics@@QEBAXAEBVTransform@Math@@AEBVTime@System@@1@Z
    0x180015fe0  ?GetEndTime@Animation@Graphics@@QEBAHXZ
    0x180016090  ?GetEvent@Animation@Graphics@@QEBAAEBUEvent@12@H@Z
    0x180016080  ?GetEventCount@Animation@Graphics@@QEBAHXZ
    0x180016030  ?GetInstanceCount@Animation@Graphics@@QEBAHXZ
    0x180016000  ?GetLength@Animation@Graphics@@QEBA?AVTime@System@@XZ
    0x180016040  ?GetLoopPoint@Animation@Graphics@@QEBAMXZ
    0x180015f50  ?GetMeshName@Animation@Graphics@@QEBA?AVString@System@@XZ
    0x180015f10  ?GetName@Animation@Graphics@@QEBA?AVString@System@@XZ
    0x1800faca0  ?GetPoseAtTime@Animation@Graphics@@QEBAXAEBVTime@System@@0PEAVTransform@Math@@H@Z
    0x180015ff0  ?GetRate@Animation@Graphics@@QEBAMXZ
    0x1800faaa0  ?GetRootPoseAtTime@Animation@Graphics@@QEBA?AVTransform@Math@@AEBVTime@System@@0@Z
    0x180015f90  ?GetSourceName@Animation@Graphics@@QEBA?AVString@System@@XZ
    0x180015fd0  ?GetStartTime@Animation@Graphics@@QEBAHXZ
    0x180016020  ?IsInstanced@Animation@Graphics@@QEBA_NXZ
    0x180016010  ?IsLooping@Animation@Graphics@@QEBA_NXZ
    0x1800fb080  ?Serialize@Animation@Graphics@@QEAAXAEAVSerializer@IO@@@Z
    0x1800fafa0  ?Set@Animation@Graphics@@QEAAXHHPEBVTransform@Math@@@Z

##### Graphics::AnimationManager   exports=19
    0x18005a780  ??0AnimationManager@Graphics@@QEAA@AEBV01@@Z
    0x1800fc430  ??0AnimationManager@Graphics@@QEAA@XZ
    0x1800fc400  ??1AnimationManager@Graphics@@QEAA@XZ
    0x18005a5b0  ??2AnimationManager@Graphics@@SAPEAX_K@Z
    0x18005a620  ??2AnimationManager@Graphics@@SAPEAX_KPEAX@Z
    0x18005a630  ??3AnimationManager@Graphics@@SAXPEAX@Z
    0x18005a870  ??4AnimationManager@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x1800fb970  ?FreeInstanceId@AnimationManager@Graphics@@QEAAXH@Z
    0x18005a690  ?Get@AnimationManager@Graphics@@SAPEAV12@XZ
    0x1800fb9d0  ?GetInstanceId@AnimationManager@Graphics@@QEAAHV?$ExternalPtr@VAnimationGroup@Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@@Z
    0x18005a6d0  ?GetPose@AnimationManager@Graphics@@QEBAPEBVTransform@Math@@H@Z
    0x1800fb8e0  ?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z
    0x18005a730  ?GetRootAnimationOffset@AnimationManager@Graphics@@QEBA?AVVector3@Math@@H@Z
    0x1800fb870  ?GetRootPose@AnimationManager@Graphics@@QEBA?AVTransform@Math@@H@Z
    0x18005a700  ?GetTransforms@AnimationManager@Graphics@@QEAAPEAV?$InterpolatedState@VTransform@Math@@@2@H@Z
    0x18005a6a0  ?GetTransforms@AnimationManager@Graphics@@QEBAPEBV?$InterpolatedState@VTransform@Math@@@2@H@Z
    0x1800fc190  ?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z
    0x1800fc240  ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z
    0x1801fe7b0  ?_singleton@AnimationManager@Graphics@@0PEAV12@EA

##### Graphics::AnimationState   exports=21
    0x1800168d0  ??0AnimationState@Graphics@@QEAA@AEBV01@@Z
    0x1800fdaf0  ??0AnimationState@Graphics@@QEAA@XZ
    0x1800fe380  ??1AnimationState@Graphics@@QEAA@XZ
    0x180016910  ??4AnimationState@Graphics@@QEAAAEAV01@AEBV01@@Z
    0x1800fd4c0  ?CopyInstancedToUnique@AnimationState@Graphics@@AEBAXAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@H@Z
    0x1800fe220  ?GetAnimationElapsed@AnimationState@Graphics@@QEBAMXZ
    0x1800fda90  ?GetCurrentAnimationIndex@AnimationState@Graphics@@QEBAHXZ
    0x1800fda70  ?GetCurrentAnimationTimeScale@AnimationState@Graphics@@QEBAMXZ
    0x1800168b0  ?GetInstanceId@AnimationState@Graphics@@QEBAHXZ
    0x1800fe2d0  ?GetInstanceTransforms@AnimationState@Graphics@@QEBAPEBV?$InterpolatedState@VTransform@Math@@@2@XZ
    0x1800fe320  ?InstancedRootAnimationOffset@AnimationState@Graphics@@QEBA?AVVector3@Math@@XZ
    0x1800fdab0  ?IsAnimating@AnimationState@Graphics@@QEBA_NXZ
    0x1800fda20  ?IsCurrentAnimationLooping@AnimationState@Graphics@@QEBA_NXZ
    0x1800168a0  ?IsInstanced@AnimationState@Graphics@@QEBA_NXZ
    0x1800fdf50  ?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup@Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTime@System@@2M_N@Z
    0x1800fd930  ?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ
    0x1800fe250  ?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z
    0x1800fda50  ?SetCurrentAnimationTimeScale@AnimationState@Graphics@@QEAAXM@Z
    0x1800fdb20  ?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@PEAV78@AEAVVector3@8@_N6@Z
    0x1800fcc20  ?UpdateNodes@AnimationState@Graphics@@AEAAXMAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@PEAV56@AEAVVector3@6@@Z
    0x1800fc870  ?UpdateRootOnly@AnimationState@Graphics@@AEAAXMAEAVVector3@Math@@@Z
