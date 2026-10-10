# CUTOVER PUBLISH MANIFEST -- 46 tagged gap rows

CONDITIONAL 1 | REQUIRED 44 | WITHHELD 1
by namespace: Application 1 | Audio 2 | Graphics 5 | IO 5 | Input 2 | Object 10 | System 1 | UI 2 | Video 18

| # | ns | row | class | scope | minted | doors | cutover | why |
|---|----|-----|-------|-------|--------|-------|---------|-----|
| 1 | Application | `?_singleton@Application@1@0PEAV11@EA` | HARMFUL (DLL-present) / REQUIRED (cutover) | DLL-PRESENT | storage | 11 | **REQUIRED** | tag states the verdict inverts at cutover |
| 2 | Audio | `?_singleton@Configuration@Audio@@0PEAV12@EA` | HARMFUL (DLL-present) / REQUIRED (cutover) | DLL-PRESENT | storage | 3 | **REQUIRED** | verdict is DLL-PRESENT only and 3 by-name door(s) would resolve NULL at cutover |
| 3 | Audio | `?_singleton@Device@Audio@@0PEAV12@EA` | HARMFUL (DLL-present) / REQUIRED (cutover) | DLL-PRESENT | storage | 2 | **REQUIRED** | verdict is DLL-PRESENT only and 2 by-name door(s) would resolve NULL at cutover |
| 4 | Graphics | `?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z` | HARMFUL | DLL-PRESENT | **NO** | 0 | **CONDITIONAL** | HARMFUL, scoped DLL-PRESENT, but the tag never states an inversion -- nobody has adjudicated this row FOR cutover |
| 5 | Graphics | `?_frameIndex@ParticleInstance@Graphics@@1HA` | HARMFUL | DLL-PRESENT | storage | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 6 | Graphics | `?_rng@Particle@Graphics@@1VRandom@System@@A` | HARMFUL | DLL-PRESENT | storage | 2 | **REQUIRED** | tag states the verdict inverts at cutover |
| 7 | Graphics | `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | HARMFUL | DLL-PRESENT | storage | 4 | **REQUIRED** | tag states the verdict inverts at cutover |
| 8 | Graphics | `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 2 | **REQUIRED** | tag states the verdict inverts at cutover |
| 9 | IO | `??_7ReaderWriter@IO@@6B@` | NO-OP | PERMANENT | vtable | 4 | **REQUIRED** | NO-OP is permanent, but 4 by-name door(s) resolve NULL with no DLL loaded |
| 10 | IO | `??_7Stream@IO@@6B@` | NO-OP | DLL-PRESENT | vtable | 10 | **REQUIRED** | verdict is DLL-PRESENT only and 10 by-name door(s) would resolve NULL at cutover |
| 11 | IO | `?_singleton@FileSystem@IO@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 25 | **REQUIRED** | tag states the verdict inverts at cutover |
| 12 | IO | `?_singleton@Modification@IO@@2PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 6 | **REQUIRED** | tag states the verdict inverts at cutover |
| 13 | IO | `?_singleton@Persistence@IO@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 13 | **REQUIRED** | tag states the verdict inverts at cutover |
| 14 | Input | `?_singleton@Configuration@Input@@0PEAV12@EA` | HARMFUL (DLL-present) / REQUIRED (cutover) | DLL-PRESENT | storage | 10 | **REQUIRED** | verdict is DLL-PRESENT only and 10 by-name door(s) would resolve NULL at cutover |
| 15 | Input | `?_singleton@Device@Input@@0PEAV12@EA` | HARMFUL (DLL-present) / REQUIRED (cutover) | DLL-PRESENT | storage | 8 | **REQUIRED** | verdict is DLL-PRESENT only and 8 by-name door(s) would resolve NULL at cutover |
| 16 | Object | `??_7Base@Object@@6B@` | NO-OP | DLL-PRESENT | vtable | 16 | **REQUIRED** | verdict is DLL-PRESENT only and 16 by-name door(s) would resolve NULL at cutover |
| 17 | Object | `??_7CompiledExternal@Object@@6B@` | NO-OP | DLL-PRESENT | vtable | 2 | **REQUIRED** | verdict is DLL-PRESENT only and 2 by-name door(s) would resolve NULL at cutover |
| 18 | Object | `??_7External@Object@@6B@` | NO-OP | DLL-PRESENT | vtable | 15 | **REQUIRED** | verdict is DLL-PRESENT only and 15 by-name door(s) would resolve NULL at cutover |
| 19 | Object | `??_7Internal@Object@@6B@` | NO-OP | DLL-PRESENT | vtable | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 20 | Object | `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 1 | **REQUIRED** | tag states the verdict inverts at cutover |
| 21 | Object | `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 5 | **REQUIRED** | tag states the verdict inverts at cutover |
| 22 | Object | `?_singleton@Factory@Object@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 8 | **REQUIRED** | tag states the verdict inverts at cutover |
| 23 | Object | `?_singleton@InternalFactory@Object@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 24 | Object | `?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A` | NO-OP | DLL-PRESENT | typeinfo | 2 | **REQUIRED** | verdict is DLL-PRESENT only and 2 by-name door(s) would resolve NULL at cutover |
| 25 | Object | `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A` | NO-OP | DLL-PRESENT | typeinfo | 1 | **REQUIRED** | verdict is DLL-PRESENT only and 1 by-name door(s) would resolve NULL at cutover |
| 26 | System | `?_singleton@MemoryAllocator@System@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 8 | **REQUIRED** | tag states the verdict inverts at cutover |
| 27 | UI | `??_7Dialog@UI@@6B@` | HARMFUL | PERMANENT | vtable | 5 | **WITHHELD** | harm is PERMANENT; cutover does not lift it |
| 28 | UI | `??_7DialogController@UI@@6B@` | NO-OP | PERMANENT | vtable | 1 | **REQUIRED** | NO-OP is permanent, but 1 by-name door(s) resolve NULL with no DLL loaded |
| 29 | Video | `??_7ConstantBufferData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 30 | Video | `??_7ContextData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 31 | Video | `??_7DeviceData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 32 | Video | `??_7ImageBufferData@Video@@6B@` | NO-OP | PERMANENT | vtable | 3 | **REQUIRED** | NO-OP is permanent, but 3 by-name door(s) resolve NULL with no DLL loaded |
| 33 | Video | `??_7IndexBufferData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 34 | Video | `??_7PixelProgramData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 35 | Video | `??_7RenderBufferData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 36 | Video | `??_7RenderStateData@Video@@6B@` | NO-OP | PERMANENT | vtable | 3 | **REQUIRED** | NO-OP is permanent, but 3 by-name door(s) resolve NULL with no DLL loaded |
| 37 | Video | `??_7VertexBufferData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 38 | Video | `??_7VertexLayoutData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 39 | Video | `??_7VertexProgramData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 40 | Video | `??_7VideoProgramData@Video@@6B@` | NO-OP | PERMANENT | vtable | 2 | **REQUIRED** | NO-OP is permanent, but 2 by-name door(s) resolve NULL with no DLL loaded |
| 41 | Video | `?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 42 | Video | `?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 43 | Video | `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 4 | **REQUIRED** | tag states the verdict inverts at cutover |
| 44 | Video | `?_firstSurface@ImageBuffer@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 3 | **REQUIRED** | tag states the verdict inverts at cutover |
| 45 | Video | `?_singleton@Configuration@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 14 | **REQUIRED** | tag states the verdict inverts at cutover |
| 46 | Video | `?_singleton@Device@Video@@0PEAV12@EA` | HARMFUL | DLL-PRESENT | storage | 22 | **REQUIRED** | tag states the verdict inverts at cutover |

## ROWS WITH NO MINTED STORAGE (the blanket switch cannot publish these)
- `?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z` (Graphics) -- CONDITIONAL

## DOORS PER ROW
- `?_singleton@Application@1@0PEAV11@EA`: dx11/src/devicedata.cpp, runtime/src/application/application_accessors.cpp, runtime/src/application/platform_startup_win.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/input/input_device_update.cpp, runtime/src/input/input_platform_shutdown_win.cpp, runtime/src/input/input_platform_startup_win.cpp, runtime/src/input/input_win.cpp, runtime/src/ui/userinterface.cpp, runtime/src/video/device_startup.cpp, standalone/rt_orphan_probe.cpp
- `?_singleton@Configuration@Audio@@0PEAV12@EA`: runtime/src/audio/audio.cpp, runtime/src/engine/engine_platform.cpp, runtime/src/engine/singleton_ctors.cpp
- `?_singleton@Device@Audio@@0PEAV12@EA`: runtime/src/audio/audio.cpp, runtime/src/engine/singleton_ctors.cpp
- `?_frameIndex@ParticleInstance@Graphics@@1HA`: runtime/src/graphics/animation.cpp, standalone/reg_unreg_lane3.cpp, standalone/wave17_door_attend.cpp
- `?_rng@Particle@Graphics@@1VRandom@System@@A`: code/Game/Game/tornado.cpp, standalone/wave17_door_attend.cpp
- `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA`: code/Game/Component/citizenai_executor.cpp, standalone/address_registry.cpp, standalone/perf_experimental.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@AnimationManager@Graphics@@0PEAV12@EA`: runtime/src/graphics/mesh.cpp, standalone/wave17_door_attend.cpp
- `??_7ReaderWriter@IO@@6B@`: runtime/src/io/binary_writer.cpp, runtime/src/io/encoding.cpp, runtime/src/io/iobase_vtables.cpp, standalone/wave17_door_attend.cpp
- `??_7Stream@IO@@6B@`: runtime/src/io/compressedstream.cpp, runtime/src/io/file_stream.cpp, runtime/src/io/file_stream_cutover_probe.cpp, runtime/src/io/iobase_vtables.cpp, runtime/src/object/external_add.cpp, runtime/src/object/external_load.cpp, runtime/src/object/external_savetext_win.cpp, runtime/src/save/saveload_meta.cpp, runtime/src/ui/userinterface.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@FileSystem@IO@@0PEAV12@EA`: code/Game/Game/gameupdateloop.cpp, code/Game/Game/w2_takescreenshot.cpp, engine/funcs/engine_funcs.cpp, runtime/src/achievement/achievement_manager.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/engine/updateloop_loadresources.cpp, runtime/src/io/compressed_stream_write.cpp, runtime/src/io/file_stream.cpp, runtime/src/io/file_stream_cutover_probe.cpp, runtime/src/io/file_stream_slots_probe.cpp, runtime/src/io/filesystem_packages.cpp, runtime/src/io/modification_refresh.cpp, runtime/src/io/package_file_read_win.cpp, runtime/src/io/persistence_startup.cpp, runtime/src/io/text_file_stream_probe.cpp, runtime/src/object/external_compiled.cpp, runtime/src/object/external_load.cpp, runtime/src/object/external_path_win.cpp, runtime/src/object/external_savetext_win.cpp, runtime/src/save/saveload_internal.h, runtime/src/video/save_image_to_file.cpp, runtime/src/video/video_resources_probe.cpp, standalone/achievement_probe.cpp, standalone/reg_unreg_lane1.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@Modification@IO@@2PEAV12@EA`: runtime/src/engine/loops.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/engine/updateloop_loadresources.cpp, runtime/src/io/iobase_vtables.cpp, runtime/src/save/saveload_internal.h, standalone/wave17_door_attend.cpp
- `?_singleton@Persistence@IO@@0PEAV12@EA`: runtime/src/achievement/achievement_state.cpp, runtime/src/audio/audio.cpp, runtime/src/engine/engine_platform.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/input/input.cpp, runtime/src/input/input_config_startup.cpp, runtime/src/input/keymap_load.cpp, runtime/src/input/keymap_save.cpp, runtime/src/io/modification.cpp, runtime/src/io/modification_refresh.cpp, runtime/src/save/saveload_internal.h, runtime/src/video/device.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@Configuration@Input@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp, runtime/src/engine/engine_platform.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/input/input.cpp, runtime/src/input/input_device_update.cpp, runtime/src/input/input_device_update_probe.cpp, runtime/src/input/input_platform_startup_win.cpp, runtime/src/ui/userinterface.cpp, runtime/src/ui/userinterface_draw.cpp, runtime/src/ui/userinterface_frame.cpp
- `?_singleton@Device@Input@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/input/input.cpp, runtime/src/input/input_platform_startup_win.cpp, runtime/src/ui/userinterface_draw.cpp, runtime/src/ui/userinterface_frame.cpp, standalone/mouse_inject.cpp, standalone/rt_orphan_probe.cpp
- `??_7Base@Object@@6B@`: runtime/src/achievement/achievement_manager.cpp, runtime/src/achievement/goal.cpp, runtime/src/graphics/mesh.cpp, runtime/src/graphics/renderer.cpp, runtime/src/graphics/wave24_lane_gfxmat.cpp, runtime/src/io/package_file.cpp, runtime/src/object/external.cpp, runtime/src/object/internal.cpp, runtime/src/object/object_typeinfo_accessors.cpp, runtime/src/save/saveload_draw.cpp, runtime/src/ui/dialog.cpp, runtime/src/ui/dialog_ctor_dtor.cpp, runtime/src/ui/dialog_vtable.cpp, runtime/src/ui/sprite.cpp, runtime/src/ui/spritesheet_vtable.cpp, runtime/types/ui/ElementDescription.h
- `??_7CompiledExternal@Object@@6B@`: runtime/src/object/object_typeinfo_accessors.cpp, runtime/src/video/buffers.cpp
- `??_7External@Object@@6B@`: runtime/src/achievement/achievement_manager.cpp, runtime/src/achievement/goal.cpp, runtime/src/graphics/mesh.cpp, runtime/src/graphics/renderer.cpp, runtime/src/graphics/wave24_lane_gfxmat.cpp, runtime/src/io/package_file.cpp, runtime/src/object/external.cpp, runtime/src/object/object_typeinfo_accessors.cpp, runtime/src/save/saveload_draw.cpp, runtime/src/ui/dialog.cpp, runtime/src/ui/dialog_ctor_dtor.cpp, runtime/src/ui/dialog_vtable.cpp, runtime/src/ui/sprite.cpp, runtime/src/ui/spritesheet_vtable.cpp, runtime/types/ui/ElementDescription.h
- `??_7Internal@Object@@6B@`: runtime/src/object/internal.cpp, runtime/src/object/object_typeinfo_accessors.cpp, runtime/src/ui/dialog.cpp
- `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA`: runtime/src/object/type_system.cpp
- `?_singleton@ExternalFactory@Object@@0PEAV12@EA`: runtime/src/engine/loops.cpp, runtime/src/engine/mainloop_video.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/engine/updateloop_loadresources.cpp, runtime/src/object/external_ptr_serialize.cpp
- `?_singleton@Factory@Object@@0PEAV12@EA`: runtime/src/engine/mainloop_video.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/object/external.cpp, runtime/src/object/external_load.cpp, runtime/src/object/external_savetext_probe.cpp, runtime/src/object/external_savetext_win.cpp, runtime/src/object/type_system.cpp, runtime/src/ui/dialog_create.cpp
- `?_singleton@InternalFactory@Object@@0PEAV12@EA`: code/Game/Game/gameupdateloop_serialize.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/object/internal.cpp
- `?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A`: runtime/src/object/factory_dispatch_probe.cpp, runtime/src/object/object_typeinfo_accessors.cpp
- `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A`: runtime/src/object/object_typeinfo_accessors.cpp
- `?_singleton@MemoryAllocator@System@@0PEAV12@EA`: runtime/funcs/rt_port.h, runtime/src/application/application_lifecycle_probe.cpp, runtime/src/collection/set_clear_probe.cpp, runtime/src/engine/engine_lifecycle.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/object/internalfactory_persist_probe.cpp, runtime/src/system/allocator.cpp, runtime/src/ui/spritesheet_new.cpp
- `??_7Dialog@UI@@6B@`: runtime/src/ui/dialog.cpp, runtime/src/ui/dialog_ctor_dtor.cpp, runtime/src/ui/dialog_factory.cpp, runtime/src/ui/dialog_vptr_trip.cpp, runtime/src/ui/dialog_vtable.cpp
- `??_7DialogController@UI@@6B@`: runtime/src/ui/dialog.cpp
- `??_7ConstantBufferData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7ContextData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7DeviceData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7ImageBufferData@Video@@6B@`: runtime/src/video/buffers.cpp, runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7IndexBufferData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7PixelProgramData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7RenderBufferData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7RenderStateData@Video@@6B@`: runtime/src/video/companion_data.cpp, runtime/src/video/device.cpp, standalone/wave17_door_attend.cpp
- `??_7VertexBufferData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7VertexLayoutData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7VertexProgramData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `??_7VideoProgramData@Video@@6B@`: runtime/src/video/companion_data.cpp, standalone/wave17_door_attend.cpp
- `?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp, runtime/src/video/dynamic_resources.cpp, standalone/wave17_door_attend.cpp
- `?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp, runtime/src/video/dynamic_resources.cpp, standalone/wave17_door_attend.cpp
- `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA`: runtime/src/graphics/sunlight.cpp, runtime/src/video/buffers.cpp, runtime/src/video/dynamic_resources.cpp, standalone/wave17_door_attend.cpp
- `?_firstSurface@ImageBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp, runtime/src/video/dynamic_resources.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@Configuration@Video@@0PEAV12@EA`: dx11/src/contextdata.cpp, dx11/src/devicedata.cpp, dx11/src/imagebufferdata.cpp, runtime/src/engine/engine_platform.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/save/saveload_draw.cpp, runtime/src/video/buffers.cpp, runtime/src/video/device.cpp, runtime/src/video/device_adjustdisplay_win.cpp, runtime/src/video/device_startup.cpp, runtime/src/video/device_startup_probe.cpp, runtime/src/video/device_startup_win.cpp, runtime/src/video/video_resources.cpp, standalone/wave17_door_attend.cpp
- `?_singleton@Device@Video@@0PEAV12@EA`: dx11/src/devicedata.cpp, dx11/src/imagebufferdata.cpp, runtime/src/engine/engine.cpp, runtime/src/engine/engine_platform.cpp, runtime/src/engine/singleton_ctors.cpp, runtime/src/graphics/sunlight.cpp, runtime/src/save/saveload_draw.cpp, runtime/src/save/saveload_update.cpp, runtime/src/ui/dialog.cpp, runtime/src/ui/element.cpp, runtime/src/ui/userinterface.cpp, runtime/src/ui/userinterface_draw.cpp, runtime/src/ui/userinterface_frame.cpp, runtime/src/video/buffers.cpp, runtime/src/video/context_ctor.cpp, runtime/src/video/device.cpp, runtime/src/video/device_startup_win.cpp, runtime/src/video/dynamic_resources.cpp, runtime/src/video/video_resources.cpp, runtime/src/video/video_resources_probe.cpp, standalone/address_registry.cpp, standalone/wave17_door_attend.cpp

## BLINDNESS THIS TOOL DECLARES
- The door scan matches the symbol as a STRING LITERAL in runtime, code, standalone, engine, dx11. A door spelled through a variable or a named constant is invisible to it, and it cannot tell you so (`reference_literal_grep_blind_to_variables.md`). A door count of 0 is therefore a LOWER bound and never a proof of absence.
- The disposition is derived from tags other lanes wrote. This tool re-derives the DERIVATION, not the underlying measurement.
- Population: the tagged rows the census reports in the gap. A tag on a row that is not called, or already promoted, is out of scope by construction.
