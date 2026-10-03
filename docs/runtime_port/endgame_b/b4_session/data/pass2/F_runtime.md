# F_runtime inventory (runtime/ + engine/): 1,336 casts, 249 files, 487 (file, blocker) rows

Method: every cast was read from `slices/F_runtime.detail.txt` and classified from the snippet, the
`// LEFT:` texts and the declaring headers; ~86 files >= 5 casts were checked at their cast sites.
Counts inside a file are per-site classification, not estimates, except where a file mixes keys on
adjacent lines (ui/*description.cpp). `engine/` has no separate dir: engine casts sit in runtime/src/engine.

## Blockers by casts (key | casts | files | header that changes | the change)
- POD:RtSystem::String/Filename/WideString | 112 | 38 | types/system/String.h, Filename.h, io/FileSystem.h | give the Rt* POD C-doors (CtorCopy, Find, AsUTF8, ConfigMap_Find) real System:: overloads, delete the POD views (H10)
- POD:ExternalPtr-Serialize-thunk | 100 | 35 | types/object/External.h | ExternalPtr<T>::Serialize thunks take `RtObject::External**` + `rt::io::Serializer*`; retype to `External**` + `IO::Serializer&` (ui/*description 51, graphics 18, video 6, achievement 6)
- SIG:Serialize-stream | 85 | 70 | types/object/External.h (slot 5) | `Serialize(void* stream)` -> `Serialize(IO::Stream&)`; one change clears 70 files, every override
- H5:String-members-held-as-raw-char* | 79 | 41 | ui/*Description.h, Element headers, GraphicsMesh/Video/Pathfinding/Picking | members are `char*` / POD storage read as `System::String*`; make them `System::String`
- ERASED:rtsetavl::Set/Node | 74 | 15 | types/collection/Collection.h, src/collection/set_avl.h | typed node per family (FilenameNode, StringPairNode, Resource/RefEntry); core takes void*
- ERASED:rtcoll::ArrayHdr | 72 | 13 | types/collection/Collection.h, array_destroy.cpp | `Array<T>` -> `Array<uint8_t>` core calls and the (stride, offset-table) Body A/B core; a typed body per element
- NOCLASS:AVL node families | 51 | 1 (collection/avl_trees.cpp) | set_avl.h | TypeNode 0x50 / String-pair 0x30 / Filename 0x30 node layouts; fold into the ERASED node-typing task
- SIG:Encoding::MakeReaderWriter | 46 | 12 | types/io/Stream.h | raw vslot (`encVt[slot/8]`) returning `void*`, then viewed as `IO::ReaderWriter*`; declare the Encoding slot with a typed return (collisionmesh, surface, pointlist, pickingmesh, external_*, dialog_vtable ...)
- FIELD:UI::Element::_border | 43 | 25 | types/ui/Element.h:171 (+:533) | `void* _border` -> `Element*` (the unit test ui_element_abi_test.cpp:394 pins void*; update it)
- NOCLASS:GraphicsMesh::TriangleList element (0xb0) | 29 | 1 (array_graphics_mesh.cpp) | types/graphics/GraphicsMesh.h | the 0xb0 element (String, +0x10 float, Array at +0x98, VertexBuffer/IndexBuffer) has no type; header TriangleList differs
- FIELD:RtIO::ModArray::data | 28 | 2 | types/io/Modification.h | ModArray buffer `void*` -> `Mod*` (Mod is POD, H5)
- FIELD:UI::DialogInstance::_ui / UserInterface list element | 27 | 8 | types/ui/DialogInstance.h:230, UserInterface.h:193 | `void* _ui`, node `dialog`, `_dialog` -> `UserInterface*` / `DialogInstance*`
- FIELD:Configuration/Persistence::settings map root | 26 | 7 | types/system/Configuration.h, io/Persistence.h | settings map held as void* root -> `rtsetavl::Set` / `MapNode*`
- SIG:Graphics-void*-doors | 22 | 5 | types/graphics/Mesh.h, Animation.h, Camera.h, Particle.h | C doors with `void* self/vec3/time/list` params
- SIG:Internal-void*-door | 21 | 6 | types/object/Internal.h, Type.h, TypeInfo.h | InternalPtr/factory doors take void*; `Internal**` arrays
- FIELD:Video::Context::void*-members | 21 | 10 | types/video/Context.h | `_renderBuffer`, `_imageBuffers[16]`, `_indexBuffer`, `_pixelConstantStaging` -> typed
- NOCLASS:TypeRecord at AVL node +8 | 18 | 1 (object/type_reflection.cpp) | types/object/TypeInfo.h | node+8 TypeRecord; typed node
- FIELD:Video::*::deviceData/platformData | 17 | 10 | types/video/Device.h, Buffers.h, GraphicsMesh.h Buffer | `void* _deviceData/platformData` -> `*BufferData*`, `DeviceData*`
- SIG:ArraySpec-element-callbacks | 17 | 5 | types/collection/Collection.h | element ctor/copy/dtor callbacks of ArraySpec take `void* dst/src`
- FIELD:RtGraphics::ExternalPtrHandle::p | 16 | 5 | types/graphics/Geometry.h:72 | untyped `void* p` handle -> `T*` (callers in code/Game hold the handle)
- FIELD:UI::Sprite::spriteSheet | 16 | 10 | types/ui/Sprite.h:105, SpriteContext.h:314, Dialog.h:145 | `void* spriteSheet/_currentSheet/_spriteSheet` -> `SpriteSheet*`
- NOCLASS:Factory/InternalFactory/PathStack AVL set nodes | 14 | 1 (factory_dtors.cpp) | types/object/TypeInfo.h | dtors read set nodes by offset
- NOCLASS:cursor/message callback set element | 13 | 3 | src/application/message_callbacks.cpp | AVL element {message, callback} untyped
- FLOOR:string_header 13, FLOOR:byte_io 13, FLOOR:win32 10, FLOOR:alloc_header 7, FLOOR:ptr_int 5, FLOOR:pe_walk 4 = 52 casts to mark `// CAST-OK`
- OTHER:utf8-utf16-transcoder-bytes | 13 | 3 | src/system/{widestring,error}.cpp | retype transcoders over char/wchar_t units
- remaining ~190 casts are single-owner keys of 1-12 casts (see TSV): RtObject POD views in io/object (12 + 11), Encoding codec void* params 12, PathStack element 10, FIELD:GraphicsMesh::_detailLevels 10, Persistence void* self 10, Configuration read by raw offset 10, External ref sets 8 ...

Family totals: FIELD 279, SIG 248, POD 247, NOCLASS 200, ERASED 152, H5 111, FLOOR 52, OTHER 30, FWD 8, CONST 5, LOCAL 4.

## Largest "same-region" files (blocker is a header in runtime/types, no cross-module owner)
LOCAL is nearly empty here: only 4 casts (ui/widget_common.h `B(p, off)` accessors) are a plain
"header already declares the field". The first pass cleared every LOCAL cast in runtime/; what is left
is blocked by a type change. Biggest files and their dominant key:
1. collection/avl_trees.cpp 61 - NOCLASS:AVL node families 51
2. collection/array_destroy.cpp 38 - ERASED:rtcoll::ArrayHdr 33
3. io/modification.cpp 36 - FIELD:RtIO::ModArray::data / H5:Modification arrays
4. collection/array.cpp 33 - 8 distinct NOCLASS element records (Mod, Sampler, VertexProgram Element/Stream)
5. collection/array_graphics_mesh.cpp 33 - NOCLASS TriangleList 29
6. types/collection/Collection.h 27 - ERASED (Set core / Array core)
7. graphics/mesh.cpp 25 - SIG:Graphics-void*-doors + ExternalPtrHandle::p + _detailLevels
8. io/modification_refresh.cpp 24 - ModArray::data + ERASED set
9. io/modification_setupfilesystem.cpp 22 - POD:RtObject External/Factory/FileSystem
10. object/external_load.cpp 22 - SIG:Encoding::MakeReaderWriter + NOCLASS dependency record

## Classes with NO header (a layout must be settled first)
- AVL node families: RtObject TypeNode 0x50 (balance/height int16 at +0x48/+0x4a, header has one int32),
  String-pair node 0x30, Filename node 0x30, Resource/ReferenceEntry nodes, KeyMap binding node (set_avl24),
  cursor/message callback element {message, callback}
- GraphicsMesh TriangleList element (0xb0), RendererDrawGroup items (3x Array at +0x1c0)
- Video::PixelProgram::Sampler, Video::VertexProgram::Element and ::Stream
- Modification file-record {uint32, External*, Filename} (0x18), PackageFile priority view (+0x70)
- Object::External dependency record {String, int32, int32} (0x10), PathStack element {Filename, Filename}
- TypeRecord at node+8, VideoResourceEntry, Object::Factory count field (+8; header declares 8 bytes)

## Where the brief is off
- Casts of the shape `static_cast<IO::Stream*>(streamv)` are the most spread blocker (70 files) but are one
  header change; the brief's `SIG:Serialize-stream` key fits. External**/rt::io::Serializer* casts are
  the sibling change in the same header (External.h); I kept them as one key `POD:ExternalPtr-Serialize-thunk`.
- Most `System::String*` casts in ui/ are H5 (raw char* members), not H10 POD bridges; the rest of the
  region uses Rt* POD C-doors (POD key).
- Encoding reader-writer factory vslot casts (46) fit none of the brief's keys cleanly; filed as SIG.
- Several diagnostics counters `(unsigned long long)` and memcpy puns are counted; they are FLOOR.
- The brief's description says `engine/`; there is no top-level engine dir in this slice, only runtime/src/engine.
