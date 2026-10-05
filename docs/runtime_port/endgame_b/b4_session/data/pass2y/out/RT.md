# Wave Y inventory: region RT (`runtime/`, `engine/`, `tests/`)

329 cast lines in 112 files, 130 units (`RT.tsv` has one row per line, `RT_units.tsv` one row per unit).
Every retail ctor / dtor address below comes from the `??0` / `??1` exports. Each dtor order was read in
Ghidra (`Runtime-x64.dll`) this pass.

| Kind | Casts | What it is |
|---|---|---|
| CLASS | 89 | 40 classes or element types |
| SIG | 72 | void* / integer declarations |
| POD | 43 | mirrors and bridges; 19 are the `pod_bridge.h` String / Filename bridge |
| ERASED | 39 | rtsetavl / Array core entry points and spec-row element ops |
| RULING | 70 | not work: totals at the end |
| LOCAL | 16 | 3 are stale; the rest are raw String locals or need a ruling |

## Doable classes: one commit each
- **UI::BorderDescription** (1), **UI::ScrollDescription** (2), **UI::TextDescription** (3), **UI::LabelDescription** (1).
  Each has a `String` member or `String[9]`. Retail releases them highest offset first, then the base. A typed member reproduces that order, and the hand-written dtor body becomes empty.
- **UI::ComboElement** (3). Change `_strings` from `Array<void*>` to `Array<System::String>`. Retail 0x180112490 destroys +0x108, +0xf8, +0xe8 in that order. Drop the explicit `RtColl_ArrayDtorElem_180010ab0` call from the dtor body.
- **Audio::Stream** (1). Retail 0x1800a0d90 runs CloseHandle x2 and DeleteCriticalSection, then `~Array<Block>` (+0x30), then ~Thread. The `RtAudio::Stream` mirror stays POD.
- **IO::TextFileStream** (1). Retail 0x1800ba490 frees the +0x70 buffer, then releases +0x60 and +0x58, then ~FileStream.
- **IO::FileSystem** (1). Type `_packfiles` as `Array<PackageFile*>`. Retail 0x1800ac6d0 frees +0x40, +0x30, +0x20, then the three Filenames. The body keeps +0x40 and +0x30, so a member at +0x20 lands in the same place. SortByKey70 (3 casts) follows.
- **Graphics::EdgeMesh::_reserved20**, **Graphics::RendererDrawable::_object** and **IO::Modification::ReloadEntry**: field or element types only.
  `_object` can be `const Geometry*` because Light derives from Geometry. ReloadEntry and **PixelProgram::Sampler** need an element type declared.

## Doable with neighbours (same commit)
- **UI::Dialog** (3). Retail 0x180112e40 destroys +0x78, +0x68, +0x58, +0x48, then `_spriteName` (+0x28, already typed).
  The four arrays and the `_name` of both element types go together. Those element types live beside the `RtUI::Dialog` mirror.
- **UI::DocumentTextElement** (2). Retail 0x1800cf320 destroys +0x198, +0x168, +0x158 (`Array<Link>`), +0x148, +0x130, +0x120, +0x110, +0xe8. That is exact reverse declaration order: the six arrays, `Link::text`, `_sourceText` and `_openLink` go in one commit.
- **UI::DocumentTextDescription** (1). Retail 0x1800cfb30 destroys +0xd0, +0xc0, +0xb0, +0x88 (`Array<String>`), +0x80, +0x70.
- **Erased-array element types**: StringTable::TextEntry, SpriteCapture::CaptureEntry, SpriteSheet entry, MaterialInstance texture / constant, PickingMesh::SubMesh, ShaderProgram::Parameter, VertexLayout::Element.
  Each element destructor releases its Strings in reverse field order, so a typed element matches. The spec-row destroy and assign arms must switch in the same commit, or the names are released twice. ShaderProgram.h:258/261 and video_program_abi_test.cpp:175 assert Parameter trivial.
- **Graphics::GraphicsMesh** (4). Retail 0x1800ff270 destroys +0x70, +0x60 (`_nodes`), +0x50, +0x20 (`_name`). `_name`, `_nodes` (as `Array<Node>`, with Node::_name), DetailLevel::_name and the array_graphics_anim Node rows go together.
- **Graphics::AnimationGroup** (3). Retail destroys +0x48, +0x38 (Tags), +0x20 (`Array<Animation>`). ~Animation 0x1800fb290 destroys +0x60, +0x20, +0x18, +0x10, +0x00, which is reverse declaration order. AnimationGroup.h:240's trivial assert on Tag flips.
- **UI::ElementDescription::_textName** (2) touches every derived description dtor. **UI::DialogInstance::_root** needs `InternalPtr<Element>` with no destructor: retail deletes `_root` in the dtor BODY, first.
- **UI::SpriteContext** local in `UserInterface::Draw` (1). The line tag is stale. Retail Draw 0x180121890 runs an inlined ~SpriteContext at its end, four array frees from 0x1801222e0. Its field-by-field build still needs comparing with the ctor 0x18011d1e0.
- **Video::Context** (6). Retype `_renderBuffer`, `_imageBuffers` and `_unknown20f0`. The readers are in code/ (geometry_draw.cpp, weather_particle_draw.cpp, statusicon_geometry.cpp, minimap_paint.cpp) and in the dx11 RenderBufferPrefix view.
- **POD:RtIo::SettingsMap** (1) waits on `Configuration::_map` becoming a Set member. Persistence already moved to OwnedSettingsMap in X7-RT.

## Blocked
- **IO::Modification::Mod** (7, plus 5 SIG casts on its Array cores), **ExternalFactory::PathStackEntry** (5) and **ReferenceEntry** (2) are blocked. The ledger's wave X round 1 finding still holds: these are elements of erased containers with hand-written lifecycles.
- **System::WideString slots** (7) are blocked by ledger rows 580-582: four sites free the WideString before the Strings built from it.
- **Graphics::MeshInstance::_animation** (4) is blocked: the tree's `Graphics::AnimationState` is 0x48 bytes, but retail embeds a 0x18-byte one at +0xd8. AnimationState must be redefined first.
- **AnimationManager::Instance** local (1) waits on GraphicsMesh `_nodes`.
- **System::Filename IS-a String** (3) is a design ruling: a base class would run String's ctor and dtor.
- **POD:RtSystem::String/Filename bridge** (19) stays while RtSystem::String is named in 50 files and RtSystem::Filename in 36.

## Stale (LOCAL, convert by name)
- dialog_create.cpp:328: `UI::Dialog::_spriteName` is already a `System::String`.
- widgets_layout.cpp:92-93: `RtStringAssign`'s only caller passes `TextElement::_params`, which has been `Array<System::String>` since X7-RT.
- editelement.cpp:87/110: `EditElement::_text` and `_filter` are typed. The helpers now serve raw `char*` locals only, so the ledger's noiseui.cpp:154 finding is resolved.

## Hidden units
`description_common.h` StringSerialize and GetSpriteIndexFromName take 2 casts. They stand for the String members of the Edit, Graph, Image, Layout (entry), List, Progress, Slider and Tab descriptions, which no line in this region counts.

## RULING totals (count only)
| Class | Casts |
|---|---|
| macro (array_destroy.cpp / array.cpp erased-entry macros) | 16 |
| erased_self (array_destroy self 8, Collection.h `this` 5) | 13 |
| alloc_shape: RtMemAlloc 8, class operator new 6, InternalMemAlloc 5, Device / Emitter / sunlight OperatorNew / TypeChildAlloc 1 each | 23 |
| vptr_probe (thread_win.cpp slot calls) | 4 |
| callback_ctx | 3 |
| const_find | 3 |
| h8 | 3 |
| retail_diff:371 (UserInterface null Encoding guard) | 2 |
| const_slot (const method writes: UEBA Render / Generate, Achievement::CreateInstance) | 3 |

`const_slot` is not one of the brief's keys; the nearest is const_find. Two more LOCALs need a ruling rather than work: the `g_identityTable` raw storage and the unbounded `_items[pass]` index.
