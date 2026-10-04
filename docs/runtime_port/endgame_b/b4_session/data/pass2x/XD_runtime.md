# Wave X inventory — XD_runtime (runtime/ + engine/)

749 casts on 704 lines in 191 files, every cast classified (`XD_runtime.tsv`, one row per line).
By class: POD 258, ERASED 122, SIG 98, H5 94, LOCAL 54, FIELD 54, OTHER 39, FLOOR 12, H8 10, CONST 5, SLOT 3.
`XD_runtime_families.tsv` has one row per SIG / SLOT / ERASED / POD family (65 rows).
Caller scans (`all_caller_files`) cover code runtime standalone dx11 engine tests, comments and strings stripped.
Where the typed signature is a proposal it comes from the body, not from SIG_WORK / SIG_ACT (neither names a runtime function).

## Families by casts (SIG / SLOT / ERASED / POD only; H5, FIELD, LOCAL follow)
1. POD:ExternalPtr-thunk 103 (48 files) — `RtObject_ExternalPtr(Array)Serialize_<T>` / `ExternalPtrSerializeBody` taking `RtObject::External**` + `rt::io::Serializer*`; ~40 thunks, ~100 code/Game callers. One declaration family, the biggest by far.
2. POD:RtObject-mirror 43 (68 files) — `RtObject::External/Factory/ExternalFactory/Internal/BaseTypeInfo` beside the `Object::` classes.
3. POD:RtSystem::Filename 34 (46 files), POD:RtSystem::String 24 (57), POD:pod_bridge-Sys/SysFile 20 (32): retire together (the bridges die with the mirrors).
4. POD:RtIo-views 23 (81); POD:RtGraphics-views 9 (50); POD:RtIO::Modification-storage 2.
5. ERASED:ArraySpec element ops 25 (17 files): graphics/engine/save element copy / init / destroy callbacks with `void* dst, const void* src`.
6. ERASED:RtColl_Array{Dtor,DtorElem,Assign,CopyCtor}_*(void*) 20 + RtColl_Array{I32,U8,Filename,Mod}* 12 + Rt_ArrayDtor/Assign (graphics-anim) 10 + SetSize/SetSizeFill/Reserve 4: the `void*` doors of Collection.h:1756+ (about 100 entry points).
7. ERASED:rtsetavl::Node-element-read 15, rtcoll::ArrayHdr-core 13 (Collection.h's own `reinterpret_cast<ArrayHdr*>(this)`), ElemDestroy callbacks 11, rtsetavl24 bridge 5, PortedRetrace8 3.
8. SIG (each small): rtavl::StringMapInsert 7, Debug_*Sprite* 7, rtpathid::SubtreeFree 4 + Insert 3, rtsetclear::Clear 4, FileSystem::CreatePath/ExistsInPackfile 4, SetLight 4, Persistence forwarders 4, Serialize(void*) family 4 + Vector 3 + Transform / Audio::Scene / String 1 each, FilenameSetInsert 3, SortByKey70 3, CullStack 3, EdgeVertex_Ctor 3, AdjustDelimiters 3, Factory dtors 3.
9. SLOT: ImageBuffer::Render(void* renderer, scene, camera) 3 (GeneratedImageBuffer.h:192, 11 files). DialogInstance::Update(..., uint8_t*) is a non-virtual overload, not a slot (key `SIG:`).
Not in the families table: H5 94 (74 are String / Array members in ui, video, graphics and picking; the rest Mod / Persistence / FileStream / MeshInstance::_animation), FIELD 54 (`ExternalPtrHandle::p` 17, `Context::_pixelConstantStaging` 5, `MainLoopCallback::_func` 2, `BaseTypeInfo::factoryFn` 3, `ExternalArrayElement::opaque/owned` 6), OTHER 39 (20 are `static_cast<T*>(RtMemAlloc / operator new)` results), FLOOR 12 (vslot 4, alloc_header 2, ptr_int 2, dx11_seam 2, byte_io 1, elem_stride 1).

## LOCAL: nothing outside the file blocks these (54 casts)
- array_graphics_mesh.cpp, 33: the "0xb0 TriangleList" element IS `GraphicsMesh::DetailLevel` (GraphicsMesh.h:311, with embedded `Video::VertexBuffer/IndexBuffer`); `TriangleList` is a different 0x18 struct. Whole file can be typed.
- array.cpp / array_destroy.cpp: `VertexProgram::Stream` 7, `AnimationGroup::Tag` 5, `VertexProgram::Element` 4, `MaterialConstant` 1; avl_trees.cpp `TypeNode::balance/height` 4 casts on 2 lines.

## LEFT comments the code contradicts
- array.cpp:3148-3174 and array_destroy.cpp:1392-1403: "VertexProgram::Element / Stream has no type in any header" — types exist (video/ShaderProgram.h:342 and :349, with static_asserts).
- array.cpp:2135-2141: "AnimationGroup::_tags is raw Array<uint8_t>, no element type" — `Collection::Array<Tag> _tags` and `Tag` exist (graphics/AnimationGroup.h).
- array.cpp:1487: MaterialConstant `+0x00 String` — `MaterialConstant::name` is `RtSystem::String` (graphics/Material.h:87).
- array_graphics_mesh.cpp (all 33): "the 0xb0 TriangleList element has no type" — `GraphicsMesh::DetailLevel`.
- avl_trees.cpp:296/299: "TypeNode balance/height are one int32 in the header" — `int16_t balance`, `int16_t height` at +0x48/+0x4a (object/TypeInfo.h:209).
- persistence.cpp:112-124 "B4 FORWARD: 1/2 outside callers": `RtIo_Persistence_GetInt` and `GetBool` have ZERO callers (GetBool only an unused `extern` at input.cpp:40). SetInt has 1 (keymap_save.cpp:211), SetBool 3 (input.cpp:435-437).
- cull.cpp:227/265 "CullStack door takes void* (outside callers)": no file outside cull.cpp names `RtGraphics_Scene_CullStack*`; the two doors are LOCAL-typable now.
- Still true: array.cpp:852 `ExternalArrayElement::owned` is `char*` (FIELD); array.cpp:1086 `PackageFileEntry::name` is `RtSystem::String` copied with the Filename ctor (POD:RtSystem::Filename).
- Still true: `PixelProgram::Sampler` has no type in any header (array.cpp:3122, 1 row, OTHER).

## Deletable now (pure forwarder, zero callers)
- `RtIo_Persistence_GetInt` (persistence.cpp:111) and `RtIo_Persistence_GetBool` (persistence.cpp:115) + the dead `extern` at input.cpp:40: forward to `IO::Persistence::Get`, no call site.

## Notes for the planner
- `SIG:Serialize(void*)` per-class rows carry a SUPERSET caller list (any `x.Serialize(ser|serializer|outer|stream…)` line, ~134 files): receiver types are not visible to grep. Narrow by compiling after retyping.
- The ExternalPtr thunks + `RtObject-mirror` + `RtSystem::Filename/String` + `pod_bridge` + `RtIo-views` families overlap on the same files (external_*.cpp, ui/*description.cpp); give them one owner.
- `ERASED:RtColl_ArraySetSize*` lists 64 caller files, all under runtime/ (none in code/Game): the owner is the runtime tree alone.
