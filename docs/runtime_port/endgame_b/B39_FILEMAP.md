# B.3 step 9 — the file map

Companion to `B39_RULES.md` §4. Every source file in scope — each git-tracked `*.cpp *.h *.hpp *.inc *.c`
under `runtime/ standalone/ engine/ code/Game/ dx11/ tests/unit/`, plus the build and tool files the step
touches — is assigned to EXACTLY ONE owner. §1 is the verbatim output of
`b39_session/filemap.py` (its rules are the map; re-run it with `--check` after adding files: it must print
0 unassigned, 0 twice). A block edits ONLY its own files, except (a) the DELETE-ONLY shared files of §2.2, (b)
the foreign sites of §3, and (c) new files it creates in its own directories (its fragment, its unit test).
Anything else it needs is a HAND-IN (§2.3).

Owners: `COORD` (the coordinator, merges only), the Runtime namespace blocks `OBJ SYS IO VID GFX UI-A UI-B UI-D
ENG`, the exe-side blocks `EXE` (EXE-1 and EXE-2 are the same file set, two launches) and `RF` (the reflection
fold), the caller batches `C1..C5`, and `CLOSE` (the closing owner, which may edit ANY file once every block is
merged). A block with two or three launches (W1/W2/W3) keeps its file set in each.

New files, owned by their creator: `tools/rt_mint_retired/<block>.txt` (one fragment per block, lower-case block
name: `obj.txt`, `ui-a.txt`, …; caller batches retire no mint row and have none); `tests/unit/b39_<block>_*_test.cpp`.
`tools/rt_mint_retired/pilot.txt` is COORD's (the pilot's rows; never edited again).

## 1. Per-owner file sets (generated)

<!-- filemap.py: 1875 files in scope, 1875 assigned, 0 unassigned, 0 twice -->

| owner | files |
|---|---:|
| COORD | 27 |
| OBJ | 48 |
| SYS | 85 |
| IO | 58 |
| VID | 67 |
| GFX | 60 |
| UI-A | 64 |
| UI-B | 52 |
| UI-D | 77 |
| ENG | 111 |
| EXE | 298 |
| RF | 11 |
| C1 | 227 |
| C2 | 195 |
| C3 | 98 |
| C4 | 75 |
| C5 | 309 |
| CLOSE | 13 |

### COORD (27 files)

- `/`: Makefile
- `engine/funcs/`: engine_funcs.cpp, engine_funcs.h
- `runtime/funcs/`: rt_port.h, rt_resolve_cache.cpp, rt_sink.h, runtime_funcs.cpp, runtime_funcs.h
- `runtime/types/`: common.h
- `standalone/`: cutover_manifest.cpp, cutover_manifest.inc, options.cpp, options.h, reg_mint_promoted.cpp, resolve_standalone.cpp, rt_data_mint.cpp, rt_data_mint_tables.inc, rt_export_addrs.inc
- `tests/unit/`: collection_link_stubs.cpp, link_stubs.cpp, main.cpp, rt_port_stub.cpp, sources.mk, unit.h
- `tools/`: cutover_manifest.py, dispatch_slot_map.py, rt_mint_gen.py

### OBJ (48 files)

- `runtime/src/object/`: class_operator_delete.cpp, deleting_dtor.cpp, deleting_dtor_internal.h, deleting_dtor_leaves.cpp, external.cpp, external_add.cpp, external_class.cpp, external_class_doors.cpp, external_compiled.cpp, external_compiled.h, external_find.cpp, external_find.h, external_forceunload.cpp, external_list.cpp, external_load.cpp, external_path.h, external_path_stack.cpp, external_path_stack.h, external_path_win.cpp, external_ptr_serialize.cpp, external_reftable.cpp, external_savetext_win.cpp, external_set.cpp, external_set.h, factory_dtors.cpp, factory_file_extension.cpp, internal.cpp, internal_class.cpp, internal_ptr_array_serialize.cpp, internalfactory_persist.cpp, object_typeinfo_accessors.cpp, reference_set.cpp, reference_set.h, rt_factories.h, rt_factories_core.cpp, rt_factories_object.inc, rt_object_port.cpp, type_reflection.cpp, type_system.cpp
- `runtime/types/object/`: External.h, Internal.h, Type.h, TypeInfo.h
- `tests/unit/`: object_external_test.cpp, object_internal_test.cpp, object_ptr_test.cpp, object_typeinfo_test.cpp, object_vtable_abi_test.cpp

### SYS (85 files)

- `runtime/funcs/`: rt_libm.cpp, rt_libm.h
- `runtime/src/collection/`: alloc_door.cpp, array.cpp, array_destroy.cpp, array_destroy_internal.h, array_graphics_anim.cpp, array_graphics_mesh.cpp, array_internal.h, avl_trees.cpp, avl_trees.h, introsort.cpp, introsort_msvc.h, linkedlist.cpp, set_avl.cpp, set_avl.h, set_avl_e24.cpp, set_avl_e24.h, set_avl_pathid.cpp, set_clear.cpp, set_core.cpp
- `runtime/src/crt/`: msvc_libm.cpp, msvc_libm.h, msvc_libm_tables.h, msvc_qsort.cpp, msvc_stricmp.cpp
- `runtime/src/math/`: from_lookat.cpp, rt_math_port.cpp, transform.cpp, transform.h, vector.cpp, vector.h, wave9_lane_gfxr.cpp, wave9_lane_p.cpp
- `runtime/src/system/`: allocator.cpp, allocator_win.cpp, closure_batch2b.cpp, compression.cpp, configuration.cpp, crc.cpp, error.cpp, filename.cpp, pod_bridge.h, rt_system_port.cpp, setupdir_helpers.cpp, setupdir_helpers.h, string.cpp, string.h, thread_win.cpp, timer.cpp, timer.h, value_types.cpp, widestring.cpp, widestring.h
- `runtime/src/utility/`: perlin.cpp, rt_utility_port.cpp
- `runtime/src/volume/`: box.cpp, frustum.cpp, rectangle.cpp, volume_port_init.cpp
- `runtime/types/collection/`: Collection.h
- `runtime/types/math/`: Math.h
- `runtime/types/system/`: Compression.h, Configuration.h, Crc.h, Error.h, Filename.h, MemoryAllocator.h, String.h, Thread.h, Timer.h, Value.h, WideString.h
- `runtime/types/utility/`: PerlinNoise.h
- `runtime/types/volume/`: Volume.h
- `tests/unit/`: alloc_door_test.cpp, array_test.cpp, filename_test.cpp, list_test.cpp, math_test.cpp, set_test.cpp, string_test.cpp, system_abi_stubs.cpp, system_abi_stubs.h, system_abi_test.cpp

### IO (58 files)

- `runtime/src/io/`: binary_writer.cpp, closure_batch2b_win.cpp, compressed_stream_open.cpp, compressed_stream_read.cpp, compressed_stream_release.cpp, compressed_stream_write.cpp, compressedstream.cpp, encoding.cpp, encoding_visitor_text.cpp, file_stream.cpp, file_stream_win.cpp, filesystem.cpp, filesystem_createfile_win.cpp, filesystem_find_win.cpp, filesystem_openfile_win.cpp, filesystem_packages.cpp, filesystem_setupdirs_win.cpp, filesystem_startup.cpp, filesystem_timestamp_win.cpp, filesystem_win.cpp, iobase_vtables.cpp, memory_stream.cpp, modification.cpp, modification_refresh.cpp, modification_setupfilesystem.cpp, modification_sort.cpp, package_file.cpp, package_file_dtor.cpp, package_file_read_win.cpp, package_file_serialize.cpp, persistence.cpp, persistence_registry_win.cpp, persistence_saveinfo_job.cpp, persistence_savelist_job.cpp, persistence_saves.cpp, persistence_saves_win.cpp, persistence_startup.cpp, persistence_worker_win.cpp, rt_factories_io.cpp, rt_factories_io.inc, rt_io_port.cpp, serialize_family.cpp, serializer.cpp, serializer.h, serializer_class.cpp, text_codec.cpp, text_file_stream_win.cpp
- `runtime/types/io/`: FileStream.h, FileSystem.h, Modification.h, PackageFile.h, Persistence.h, Registry.h, Serializer.h, Stream.h
- `tests/unit/`: io_stream_abi_stubs.cpp, io_streams_abi_test.cpp, io_vtable_abi_test.cpp

### VID (67 files)

- `dx11/funcs/`: dx11_seam.cpp, dx11_seam.h
- `dx11/src/`: buffers.cpp, contextdata.cpp, devicedata.cpp, devicedata_stubs.cpp, dx11_backend_access.cpp, dx11_backend_access.h, exports.cpp, imagebufferdata.cpp, renderstatedata.cpp, shaderprograms.cpp, smallclasses.cpp, vertexlayoutdata.cpp
- `dx11/types/`: VideoDX11Data.h
- `runtime/src/video/`: batchc_context_setlight.cpp, buffers.cpp, buffers_win.cpp, closure_batch2b.cpp, companion_data.cpp, constant_buffer.cpp, context.cpp, context_bind.cpp, context_ctor.cpp, context_lifecycle.cpp, device.cpp, device_adjustdisplay_win.cpp, device_enum_types.cpp, device_startup.cpp, device_startup_win.cpp, dynamic_resources.cpp, enumerate_video_modes.cpp, imagebuffer_platform_serialize.cpp, imagebuffer_vtable.cpp, pixelprogram.cpp, renderstate.cpp, rt_factories_video.cpp, rt_factories_video.inc, rt_video_port.cpp, save_image_to_file.cpp, shader_constants.inc, shader_program_names.cpp, shaderprogram.cpp, vertexlayout.cpp, vertexprogram.cpp, video_interfaces_win.cpp, video_mode_sort.cpp, video_program.cpp, video_resources.cpp, video_resources_win.cpp, videoprogram.cpp
- `runtime/types/video/`: Buffers.h, CompanionData.h, ConstantBuffer.h, Context.h, Device.h, ImageBuffer.h, RenderState.h, ShaderProgram.h, VertexLayout.h, VideoProgram.h
- `tests/unit/`: rb_video_abi_test.cpp, video_context_abi_test.cpp, video_data_abi_test.cpp, video_device_abi_test.cpp, video_imagebuffer_abi_test.cpp, video_program_abi_test.cpp

### GFX (60 files)

- `runtime/src/graphics/`: animation.cpp, animationstate.cpp, animationstate_transforms.cpp, batchc_gfx_leaves.cpp, closure_batch2b.cpp, cull.cpp, edgemesh_vtable.cpp, graphics_typeinfo_accessors.cpp, graphicsmesh_vtable.cpp, imagebuffer_derived.cpp, lights_vtable.cpp, material.cpp, mesh.cpp, meshinstance_draw.cpp, renderer.cpp, rt_factories_graphics.cpp, rt_factories_graphics.inc, rt_graphics_port.cpp, scene.cpp, scene_grid.cpp, scene_grid.h, sun_camera.cpp, sunlight.cpp, vtbatch7_animgroup_particlesystem.cpp, wave21_lane_gfxbody.cpp, wave23_lane_gfxbody.cpp, wave24_lane_gfxmat.cpp, wave27_lane_gfxbig.cpp, wave27_lane_gfxpart.cpp, wave28_lane_gfxbody.cpp, wave28_lane_partsys.cpp, wave29_lane_gfxbody.cpp, wave29_lane_slots.cpp, wave9_lane_gfx.cpp, wave9_lane_gfxd.cpp, wave9_lane_gfxd.h
- `runtime/types/graphics/`: Animation.h, AnimationGroup.h, AnimationManager.h, AnimationState.h, Camera.h, FrameCopyImageBuffer.h, GeneratedImageBuffer.h, Geometry.h, GraphicsMesh.h, InterpolatedState.h, Light.h, Material.h, Mesh.h, Particle.h, ParticleInstance.h, Scene.h, SunLight.h
- `tests/unit/`: graphics_animation_abi_test.cpp, graphics_geometry_abi_test.cpp, graphics_interpolatedstate_abi_test.cpp, graphics_material_abi_test.cpp, graphics_particle_abi_test.cpp, graphics_renderer_abi_test.cpp, rb_graphics_abi_test.cpp

### UI-A (64 files)

- `runtime/src/ui/`: animatedimagedescription.cpp, animatedimageelement.cpp, borderdescription.cpp, borderelement.cpp, buttondescription.cpp, buttonelement.cpp, checkdescription.cpp, checkelement.cpp, clipregiondescription.cpp, clipregionelement.cpp, combodescription.cpp, comboelement.cpp, editdescription.cpp, editelement.cpp, groupdescription.cpp, groupelement.cpp, imagedescription.cpp, imageelement.cpp, labeldescription.cpp, labelelement.cpp, numberdescription.cpp, numbereditdescription.cpp, numbereditelement.cpp, numberelement.cpp, pagedescription.cpp, pageelement.cpp, radiodescription.cpp, radioelement.cpp, textdescription.cpp, textelement.cpp, textelement_cachetext.cpp
- `runtime/types/ui/`: AnimatedImageDescription.h, AnimatedImageElement.h, BorderDescription.h, BorderElement.h, ButtonDescription.h, ButtonElement.h, CheckDescription.h, CheckElement.h, ClipRegionDescription.h, ClipRegionElement.h, ComboDescription.h, ComboElement.h, EditDescription.h, EditElement.h, GroupDescription.h, GroupElement.h, ImageDescription.h, ImageElement.h, LabelDescription.h, LabelElement.h, NumberDescription.h, NumberEditDescription.h, NumberEditElement.h, NumberElement.h, PageDescription.h, PageElement.h, RadioDescription.h, RadioElement.h, TextDescription.h, TextElement.h
- `tests/unit/`: ui_leaf_c1_abi_test.cpp, ui_leaf_c2_abi_test.cpp, ui_leaf_c3_abi_test.cpp

### UI-B (52 files)

- `runtime/src/ui/`: containerdescription.cpp, containerelement.cpp, documenttext.cpp, documenttextdescription.cpp, documenttextelement.cpp, graphdescription.cpp, graphelement.cpp, layoutdescription.cpp, layoutelement.cpp, listdescription.cpp, listelement.cpp, progressdescription.cpp, progresselement.cpp, ribbondescription.cpp, ribbonelement.cpp, scrolldescription.cpp, scrollelement.cpp, sliderdescription.cpp, sliderelement.cpp, spinnerdescription.cpp, spinnerelement.cpp, tabdescription.cpp, tabelement.cpp, tabelement_addtab.cpp, tabledescription.cpp, tableelement.cpp
- `runtime/types/ui/`: ContainerDescription.h, ContainerElement.h, DocumentTextDescription.h, DocumentTextElement.h, GraphDescription.h, GraphElement.h, LayoutDescription.h, LayoutElement.h, ListDescription.h, ListElement.h, ProgressDescription.h, ProgressElement.h, RibbonDescription.h, RibbonElement.h, ScrollDescription.h, ScrollElement.h, SliderDescription.h, SliderElement.h, SpinnerDescription.h, SpinnerElement.h, TabDescription.h, TabElement.h, TableDescription.h, TableElement.h
- `tests/unit/`: ui_leaf_c4_abi_test.cpp, ui_leaf_c5_abi_test.cpp

### UI-D (77 files)

- `runtime/src/ui/`: characterset.cpp, combodialog.cpp, combodialog_ctor.cpp, cursor.cpp, description_common.h, description_dtors.cpp, dialog.cpp, dialog_controller_base.cpp, dialog_create.cpp, dialog_ctor_dtor.cpp, dialog_factory.cpp, dialog_settings.cpp, dialog_vtable.cpp, dialogcontrollerconfig.cpp, element.cpp, element_bodies_b5.cpp, element_common.h, element_sprite_debug.cpp, element_sprite_debug.h, element_vtable.cpp, elementdescription_serialize.cpp, factory_ui1.cpp, factory_ui1_tables.inc, font.cpp, rt_ui_port.cpp, sprite.cpp, sprite_debug.cpp, sprite_debug.h, spritecapture.cpp, spritecapture_class.cpp, spritecontext.cpp, spritecontext_add.cpp, spritecontext_flush.cpp, spritesheet_new.cpp, spritesheet_vtable.cpp, stringtable_factory.cpp, stringtable_lifecycle.cpp, tooltipdialog.cpp, ui_factory_wave28.cpp, ui_operator_delete.cpp, userinterface.cpp, userinterface_close.cpp, userinterface_dragstart.cpp, userinterface_draw.cpp, userinterface_frame.cpp, userinterface_hover.cpp, userinterface_showdialog.cpp, userinterface_standalone.cpp, widget_common.h, widgets_input.cpp, widgets_layout.cpp
- `runtime/types/ui/`: CharacterSet.h, ComboDialog.h, Cursor.h, DescriptionsBatch1.h, Dialog.h, DialogController.h, DialogControllerConfig.h, DialogInstance.h, Element.h, ElementDescription.h, Font.h, Sprite.h, SpriteCapture.h, SpriteContext.h, SpriteSheet.h, String.h, StringTable.h, ToolTipDialog.h, UserInterface.h
- `tests/unit/`: rb_ui_abi_test.cpp, ui_dialog_abi_test.cpp, ui_element_abi_test.cpp, ui_resources_abi_test.cpp, ui_spritecontext_abi_test.cpp, ui_userinterface_abi_test.cpp, ui_value_types_abi_test.cpp

### ENG (111 files)

- `runtime/src/achievement/`: achievement_manager.cpp, achievement_serialize.cpp, achievement_state.cpp, achievement_typeinfo_accessors.cpp, achievement_vtable.cpp, batchc_goallist_dtor.cpp, goal.cpp, goal_list.cpp, goal_serialize.cpp, rt_achievement_port.cpp, rt_factories_achievement.cpp, rt_factories_achievement.inc
- `runtime/src/application/`: application.cpp, application_accessors.cpp, application_lifecycle.cpp, application_thunks.cpp, application_win.cpp, message_callbacks.cpp, platform_startup_win.cpp, rt_application_port.cpp
- `runtime/src/audio/`: audio.cpp, audio_device.cpp, audio_effectslot_serialize.cpp, audio_platform_startup_win.cpp, audio_play_platform.cpp, audio_scene_update.cpp, audio_stream_play_win.cpp, audio_stream_win.cpp, audio_voice_callback_win.cpp, batchc_audio_leaves.cpp, rt_audio_port.cpp, rt_factories_audio.cpp, rt_factories_audio.inc, vtbatch7_soundbuffer_soundeffect.cpp, vtbatch7b_stream_run_win.cpp, wave29_lane_avbody.cpp
- `runtime/src/collision2d/`: collisionmesh.cpp, rt_factories_collision2d.cpp, rt_factories_collision2d.inc, surface.cpp
- `runtime/src/engine/`: closure_batch2b.cpp, engine.cpp, engine_lifecycle.cpp, engine_lifecycle_win.cpp, engine_platform.cpp, engine_platform_win.cpp, loops.cpp, mainloop_frame.cpp, mainloop_internal.h, mainloop_run.cpp, mainloop_video.cpp, rt_engine_port.cpp, singleton_ctors.cpp, singleton_ctors_win.cpp, updateloop_loadresources.cpp
- `runtime/src/input/`: input.cpp, input_config_startup.cpp, input_device_update.cpp, input_platform_shutdown_win.cpp, input_platform_startup_win.cpp, input_win.cpp, keymap_load.cpp, keymap_save.cpp, rt_input_port.cpp
- `runtime/src/pathfinding/`: pointlist.cpp, rt_factories_pathfinding.cpp, rt_factories_pathfinding.inc
- `runtime/src/picking/`: closure_batch2b.cpp, pickingmesh.cpp, rt_picking_port.cpp, scene.cpp, scene_grid.cpp, scene_grid.h
- `runtime/src/save/`: closure_batch2b.cpp, rt_save_port.cpp, savedstate_setupload.cpp, saveload.cpp, saveload_draw.cpp, saveload_internal.h, saveload_loadmeta.cpp, saveload_meta.cpp, saveload_saveinfo.cpp, saveload_update.cpp
- `runtime/types/achievement/`: AchievementManager.h, Goal.h
- `runtime/types/application/`: Application.h
- `runtime/types/audio/`: Audio.h, Device.h, SoundBuffer.h, SoundEffect.h, Stream.h
- `runtime/types/engine/`: Engine.h, MainLoop.h, UpdateLoop.h
- `runtime/types/input/`: Device.h, Input.h
- `runtime/types/pathfinding/`: IPathInfo.h, PointList.h, Scene.h
- `runtime/types/picking/`: PickingMesh.h, Scene.h
- `runtime/types/save/`: SaveLoad.h
- `tests/unit/`: achievement_goal_abi_test.cpp, application_abi_test.cpp, audio_abi_test.cpp, engine_loop_abi_test.cpp, input_abi_test.cpp, pathfinding_picking_abi_test.cpp, rb_achievement_abi_test.cpp, rb_engine_abi_test.cpp, save_abi_test.cpp

### EXE (298 files)

- `code/Game/Action/`: Action.h, action_base_complete.cpp, showtool.cpp
- `code/Game/Component/`: CitizenComponent.h, Component.h, ModelComponent.h, citizenai_actionctors.cpp, citizenai_handlers.cpp, clearareacomponent_states.cpp, component.cpp, controller_state_table.h, modelcomponent.cpp, storagedisplaycomponent.cpp
- `code/Game/Dialog/`: edittool_select.cpp, mainmenudialog.cpp, mapui.cpp, moddialog.cpp, newgamedialog.cpp, noiseui.cpp, saveloaddialog.cpp, thumb_vtables.cpp, toolbardialog.cpp
- `code/Game/Game/`: GameLoadDialogLoopT.h, GameLoaderLoop.h, GameUpdateLoop.h, InnerLoaderLoop.h, MenuDialogLoopT.h, game_leaf_helpers.cpp, gameupdateloop.cpp, gameupdateloop_serialize.cpp, keystone_a_gamedtor.cpp, menudialogloopt.cpp, minimap_paint.cpp, minimap_upload.cpp, pathfinder_seed.cpp, serialize_image_upload.cpp, w2_takescreenshot.cpp
- `code/Game/Player/`: fencedtool.cpp, placementtool.cpp, tool_base_render.cpp
- `code/Game/Tutorial/`: tutorial_next_scenario.cpp, tutorialui.cpp
- `code/Game/Work/`: consumeproduce.cpp, work_act_clear.cpp, work_act_hunt.cpp, work_act_path.cpp, work_act_perform.cpp, work_base_actionctor.cpp, work_base_reimpl.cpp
- `engine/types/`: CitizenBehavior.h, ClimateGenerator.h, Component.h, Configuration.h, DialogLoaderLoopT.h, DiseaseComponent.h, EntityList.h, FireComponent.h, GameUpdateLoop.h, LoaderLoop.h, MainLoop.h, MapGenerator.h, MenuScene.h, Modification.h, Profession.h, ProfessionList.h, RawMaterialDescription.h, TerrainGenerator.h, Toolbar.h, TornadoComponent.h, UpdateLoop.h, Weather.h, common.h, io_serializer.h
- `engine/types/ui/`: DialogController.h, DialogInstance.h, Dialogs.h, Panels.h
- `standalone/`: ach_clock.cpp, address_registry.cpp, address_registry.h, alloc_cutover_arm.cpp, alloc_stats.cpp, autostart.cpp, autostart.h, boot_state_watchdog.cpp, boot_state_watchdog.h, boot_window.cpp, boot_window.h, builder_seat_harness.cpp, chunked_scene_loader.cpp, chunked_scene_loader.h, citizen_vitals.cpp, citizen_vitals.h, cropfield_force_start.cpp, death_harness.cpp, disaster_harness.cpp, engine_loader_gate.cpp, engine_loader_gate.h, entity_comp_validator.cpp, finish_builds_harness.cpp, food_panel_harness.cpp, food_placement_harness.cpp, food_production_diag.cpp, food_stock_census.cpp, food_workers_harness.cpp, frame_limiter.cpp, frame_limiter.h, hw_watch.cpp, hw_watch.h, loader_loop_stubs.cpp, log_strip.h, market_overstock_harness.cpp, menudialogloopt_stubs.cpp, modal_driver.cpp, modal_driver.h, mouse_inject.cpp, mouse_inject.h, orchard_cut_harness.cpp, orchard_diag.cpp, orchard_force_start.cpp, orchard_growth_harness.cpp, orchard_workers_harness.cpp, panel_hold_harness.cpp, panel_staffing_driver.cpp, pasture_harness.cpp, pausemenu_harness.cpp, perf_experimental.cpp, perf_experimental.h, perf_terrain.cpp, perf_terrain.h, redzone_alloc.cpp, redzone_alloc.h, reg_action_child_ctors.cpp, reg_action_child_ctors2.cpp, reg_action_cnr.cpp, reg_action_complete.cpp, reg_action_hunt.cpp, reg_action_move.cpp, reg_action_rawmat.cpp, reg_animal_serialize.cpp, reg_animal_update.cpp, reg_autosave.cpp, reg_boat_ctrl.cpp, reg_build_actions.cpp, reg_charmanager.cpp, reg_citizen_25.cpp, reg_citizen_25_tick.cpp, reg_citizen_assignworkplace.cpp, reg_citizen_lifeevent_handler.cpp, reg_citizen_needsupdate.cpp, reg_citizen_updatehealth.cpp, reg_citizenai_actionctors.cpp, reg_citizenai_candidatelist.cpp, reg_citizenai_executor.cpp, reg_citizenai_findhelpers.cpp, reg_citizenai_findtarget.cpp, reg_citizenai_gather.cpp, reg_citizenai_growup.cpp, reg_citizenai_handlers.cpp, reg_citizenai_handlers2.cpp, reg_citizenai_handlers3.cpp, reg_citizenai_housing_idle.cpp, reg_citizenai_leisure.cpp, reg_citizenbehavior_tick.cpp, reg_closeout_lane_c.cpp, reg_closeout_lane_d.cpp, reg_code_lane2.cpp, reg_code_lane4.cpp, reg_code_lane5.cpp, reg_component_animal.cpp, reg_component_cropdisease.cpp, reg_component_economy.cpp, reg_component_fruit.cpp, reg_component_gap_A.cpp, reg_component_gap_B.cpp, reg_component_gap_C.cpp, reg_component_gap_D.cpp, reg_component_nature.cpp, reg_component_people.cpp, reg_component_spawn.cpp, reg_component_tombstone.cpp, reg_component_visuals.cpp, reg_controlui.cpp, reg_destroy_sm.cpp, reg_destroy_workers.cpp, reg_dialog.cpp, reg_edittool_select.cpp, reg_entity.cpp, reg_eventlog_citizen.cpp, reg_fire.cpp, reg_game_leaf.cpp, reg_gamesettings.cpp, reg_goal.cpp, reg_herd_live.cpp, reg_immigration.cpp, reg_keystone_a_gamedtor.cpp, reg_keystone_a_map.cpp, reg_keystone_a_removal.cpp, reg_keystone_a_restree.cpp, reg_keystone_a_zone.cpp, reg_marketwork_bst.cpp, reg_mods.cpp, reg_namedb.cpp, reg_orchard_growth.cpp, reg_pasture_live.cpp, reg_pf_astar.cpp, reg_pf_avl.cpp, reg_pf_ctors.cpp, reg_pf_geom.cpp, reg_pf_pools.cpp, reg_pf_region.cpp, reg_pf_road.cpp, reg_pf_roots_drop.cpp, reg_pf_roots_misc.cpp, reg_pf_roots_road.cpp, reg_pf_roots_route.cpp, reg_player_bridge.cpp, reg_player_createbar.cpp, reg_player_drag.cpp, reg_player_edit.cpp, reg_player_highlight.cpp, reg_player_path.cpp, reg_player_place.cpp, reg_player_ray.cpp, reg_player_render.cpp, reg_player_screenrect.cpp, reg_player_tile.cpp, reg_player_tilerender.cpp, reg_player_toolui.cpp, reg_player_trade.cpp, reg_rdata_float_consts.cpp, reg_region_graph.cpp, reg_region_paircache.cpp, reg_toolbar_utility.cpp, reg_tornado.cpp, reg_town_services.cpp, reg_town_storage_seed.cpp, reg_town_workplace_registry.cpp, reg_town_workplace_registry_reader.cpp, reg_townhallui.cpp, reg_trade_ctrl.cpp, reg_tradeui_checkout.cpp, reg_tradeui_leaves.cpp, reg_tutorial_base.cpp, reg_tutorial_wave.cpp, reg_unreg_lane1.cpp, reg_unreg_lane2.cpp, reg_unreg_lane3.cpp, reg_w2_lane3.cpp, reg_w2_screenshot.cpp, reg_work_accessor.cpp, reg_work_act.cpp, reg_work_actionctor.cpp, reg_work_base.cpp, reg_work_bst.cpp, reg_work_cellbst.cpp, reg_work_grid.cpp, reg_work_impl2.cpp, reg_work_leaf.cpp, reg_work_tail_fire.cpp, reg_work_tail_serialize.cpp, reg_work_tail_trade.cpp, reg_work_walkpos.cpp, renderdoc_integration.cpp, renderdoc_integration.h, res_load_lock.cpp, res_load_lock.h, resolve.h, rt_data_mint.h, sampler.cpp, sampler.h, saveload_harness.cpp, scene_watch.cpp, scene_watch.h, splash_loop_stubs.cpp, stall_debug.cpp, stall_debug.h, stress_harness.cpp, stress_harness.h, tool_harness.cpp, tool_harness.h, trade_buy_harness.cpp, video_interface.cpp, video_interface.h
- `tests/unit/`: act_abi_test.cpp, cit_abi_test.cpp, dlg_abi_test.cpp, exe_class_abi_test.cpp, goal_abi_test.cpp, k1_abi_test.cpp, loop_abi_test.cpp, mia_abi_test.cpp, mib_abi_test.cpp, pnl_abi_test.cpp, smoke_test.cpp, tool_abi_test.cpp, vis_abi_test.cpp, wrk_abi_test.cpp

### RF (11 files)

- `code/Game/Game/`: game_main.cpp
- `code/Game/Goal/`: statgoal.cpp
- `code/Game/Object/`: exe_type.h, factory_registration.cpp, factory_registration.h, factory_registrations_bulk.cpp, internal_factory_audit.cpp, internal_factory_audit.h
- `standalone/`: dlgctrl_route.cpp, factory_enum_stub.cpp, main.cpp

### C1 (227 files)

- `code/Game/Component/`: AbsorbResourceComponent.h, AbsorbResourceDescription.h, AmbientEmitterComponent.h, AmbientEmitterDescription.h, AmbientEnvironmentComponent.h, AmbientEnvironmentDescription.h, AnimAudioEvent.h, AnimalComponent.h, AnimalDescription.h, AttendanceComponent.h, AttendanceDescription.h, AutoPickupComponent.h, AutoPickupDescription.h, BoatComponent.h, BoatDescription.h, BuildComponent.h, BuildDescription.h, CaptureBoxDescription.h, CaptureBoxElement.h, CemeteryComponent.h, CemeteryDescription.h, CharacterComponent.h, CharacterDescription.h, CitizenAIComponent.h, CitizenAIDescription.h, CitizenDescription.h, ClearAreaComponent.h, ClearAreaDescription.h, ClearResourceComponent.h, ClearResourceDescription.h, ComponentDescription.h, ConsumeProduceComponent.h, ConsumeProduceDescription.h, ControllerComponent.h, CreateBridgeDescription.h, CreateDragDescription.h, CreateFencedDescription.h, CreatePathDescription.h, CreatePlacedDescription.h, CreateRemovalDescription.h, CreateRoadDescription.h, CropFieldComponent.h, CropFieldDescription.h, DecalComponent.h, DecalDescription.h, Description.h, DestroyComponent.h, DestroyDescription.h, DiseaseComponent.h, DiseaseDescription.h, EnvironmentDescription.h, FireComponent.h, FireDescription.h, FishingComponent.h, FishingDescription.h, FloorComponent.h, FloorDescription.h, ForesterComponent.h, ForesterDescription.h, FruitComponent.h, FruitDescription.h, FrustumOutlineDescription.h, FrustumOutlineElement.h, GrowthComponent.h, GrowthDescription.h, HappinessComponent.h, HappinessDescription.h, HealerComponent.h, HealerDescription.h, HerbalistComponent.h, HerbalistDescription.h, HerdComponent.h, HerdDescription.h, HighlightComponent.h, HighlightDescription.h, HuntComponent.h, HuntDescription.h, absorbresourcecomponent.cpp, absorbresourcedescription.cpp, ambient_externalptr_serialize.cpp, ambientemittercomponent.cpp, ambientemittercomponent_funcs.h, ambientemitterdescription.cpp, ambientenvironmentcomponent.cpp, ambientenvironmentcomponent_funcs.h, ambientenvironmentcomponent_serialize.cpp, ambientenvironmentdescription.cpp, animalcomponent.cpp, animalcomponent_funcs.h, animaldescription.cpp, animalwave_serialize.cpp, animaudioevent.cpp, attendancecomponent.cpp, attendancecomponent_funcs.h, attendancecomponent_states.cpp, attendancedescription.cpp, autopickupcomponent.cpp, autopickupdescription.cpp, boatcomponent.cpp, boatcomponent_route.cpp, boatcomponent_states.cpp, boatdescription.cpp, buildcomponent.cpp, buildcomponent_funcs.h, buildcomponent_state1.cpp, buildcomponent_state8.cpp, buildcomponent_sub8630.cpp, builddescription.cpp, captureboxdescription.cpp, captureboxelement.cpp, cemeterycomponent.cpp, cemeterycomponent_funcs.h, cemeterydescription.cpp, character_trig.cpp, charactercomponent.cpp, charactercomponent_funcs.h, charactercomponent_serialize.cpp, characterdescription.cpp, charmanager.cpp, citizen_assign_citizens.cpp, citizen_assignworkplace.cpp, citizen_employ_cascade.cpp, citizen_lifeevent.cpp, citizen_lifeevent_handler.cpp, citizen_needsupdate.cpp, citizen_onremove.cpp, citizen_reassign_workers.cpp, citizen_updatehealth.cpp, citizenai_candidatelist.cpp, citizenai_executor.cpp, citizenai_findhelpers.cpp, citizenai_findtarget.cpp, citizenai_gather.cpp, citizenai_growup.cpp, citizenai_handlers2.cpp, citizenai_handlers3.cpp, citizenai_housing_idle.cpp, citizenai_leisure.cpp, citizenai_work_scheduler.cpp, citizenaicomponent.cpp, citizenaicomponent_funcs.h, citizenaicomponent_serialize.cpp, citizenaidescription.cpp, citizencomponent.cpp, citizencomponent_funcs.h, citizencomponent_lifecycle.cpp, citizencomponent_serialize.cpp, citizendescription.cpp, clearareacomponent.cpp, clearareacomponent_funcs.h, clearareadescription.cpp, clearresourcecomponent.cpp, clearresourcecomponent_states.cpp, clearresourcedescription.cpp, coll_bridge.h, componentdescription.cpp, consumeproduce_recipe_dump.cpp, consumeproducecomponent.cpp, consumeproducecomponent_funcs.h, consumeproducedescription.cpp, createbridgedescription.cpp, createdragdescription.cpp, createfenceddescription.cpp, createpathdescription.cpp, createplaceddescription.cpp, createremovaldescription.cpp, createroaddescription.cpp, cropfieldcomponent.cpp, cropfieldcomponent_funcs.h, cropfieldcomponent_states.cpp, cropfielddescription.cpp, decalcomponent.cpp, decalcomponent_funcs.h, decaldescription.cpp, description.cpp, destroycomponent.cpp, destroycomponent_state6.cpp, destroycomponent_states.cpp, destroycomponent_workerrelease.cpp, destroydescription.cpp, diseasecomponent.cpp, diseasecomponent_funcs.h, diseasedescription.cpp, entity_finders_disaster.cpp, environmentdescription.cpp, firecomponent.cpp, firecomponent_funcs.h, firecomponent_ignite.cpp, firecomponent_smstate.cpp, firedescription.cpp, fishingcomponent.cpp, fishingcomponent_funcs.h, fishingcomponent_produce.cpp, fishingdescription.cpp, floorcomponent.cpp, floorcomponent_funcs.h, floordescription.cpp, forestercomponent.cpp, forestercomponent_funcs.h, foresterdescription.cpp, fruitcomponent.cpp, fruitcomponent_funcs.h, fruitcomponent_serialize.cpp, fruitdescription.cpp, frustumoutlinedescription.cpp, frustumoutlineelement.cpp, growthcomponent.cpp, growthcomponent_funcs.h, growthcomponent_serialize.cpp, growthdescription.cpp, happinesscomponent.cpp, happinesscomponent_funcs.h, happinessdescription.cpp, healercomponent.cpp, healercomponent_funcs.h, healerdescription.cpp, herbalistcomponent.cpp, herbalistcomponent_funcs.h, herbalistdescription.cpp, herdcomponent.cpp, herdcomponent_funcs.h, herddescription.cpp, highlightcomponent.cpp, highlightdescription.cpp, huntcomponent.cpp, huntcomponent_funcs.h, huntdescription.cpp

### C2 (195 files)

- `code/Game/Component/`: ImmigrationComponent.h, ImmigrationDescription.h, InteractComponent.h, InteractDescription.h, LivestockComponent.h, LivestockDescription.h, MapComponent.h, MapDescription.h, MarketComponent.h, MarketDescription.h, MarketLocationComponent.h, MarketLocationDescription.h, MineComponent.h, MineDescription.h, ModelDescription.h, MultiObjectComponent.h, MultiObjectDescription.h, NaturalResourceComponent.h, NaturalResourceDescription.h, NomadComponent.h, NomadDescription.h, OrchardComponent.h, OrchardDescription.h, ParticleComponent.h, ParticleDescription.h, PastureComponent.h, PastureDescription.h, PickingComponent.h, PickingDescription.h, PopulationComponent.h, PopulationDescription.h, QuarryComponent.h, QuarryDescription.h, RadiusDescription.h, RawMaterialComponent.h, RawMaterialDescription.h, ResidenceComponent.h, ResidenceDescription.h, ResourceProduceComponent.h, SelectDescription.h, SpawnComponent.h, SpawnDescription.h, StateMachineComponent.h, StateMachineDescription.h, StatusIconComponent.h, StatusIconDescription.h, StorageComponent.h, StorageDescription.h, StorageDisplayComponent.h, StorageDisplayDescription.h, StorageLocationComponent.h, StorageLocationDescription.h, TerraformComponent.h, TerraformDescription.h, TombstoneComponent.h, TombstoneDescription.h, ToolbarDescription.h, TornadoComponent.h, TornadoDescription.h, TrackerComponent.h, TrackerDescription.h, TradeComponent.h, TradeDescription.h, UIComponent.h, UIDescription.h, ValueDescription.h, WellComponent.h, WellDescription.h, WildAnimalComponent.h, WildAnimalDescription.h, WorkComponent.h, WorkDescription.h, WorkPlaceComponent.h, WorkPlaceDescription.h, ZoneComponent.h, ZoneDescription.h, immigrationcomponent.cpp, immigrationcomponent_states.cpp, immigrationdescription.cpp, interactcomponent.cpp, interactcomponent_funcs.h, interactdescription.cpp, keystone_a_map.cpp, keystone_a_restree.cpp, livestockcomponent.cpp, livestockcomponent_funcs.h, livestockdescription.cpp, mapcomponent.cpp, mapcomponent_funcs.h, mapdescription.cpp, marketcomponent.cpp, marketdescription.cpp, marketlocationcomponent.cpp, marketlocationdescription.cpp, minecomponent.cpp, minedescription.cpp, modelcomponent_funcs.h, modeldescription.cpp, multiobjectcomponent.cpp, multiobjectcomponent_funcs.h, multiobjectdescription.cpp, naturalresourcecomponent.cpp, naturalresourcecomponent_funcs.h, naturalresourcedescription.cpp, nomadcomponent.cpp, nomadcomponent_funcs.h, nomadcomponent_states.cpp, nomaddescription.cpp, orchardcomponent.cpp, orchardcomponent_funcs.h, orchardcomponent_states.cpp, orcharddescription.cpp, particlecomponent.cpp, particlecomponent_funcs.h, particledescription.cpp, pasturecomponent.cpp, pasturecomponent_funcs.h, pasturecomponent_spawn.cpp, pasturecomponent_states.cpp, pasturedescription.cpp, pickingcomponent.cpp, pickingcomponent_funcs.h, pickingdescription.cpp, populationcomponent.cpp, populationcomponent_funcs.h, populationdescription.cpp, quarrycomponent.cpp, quarrycomponent_deform.cpp, quarrydescription.cpp, radiusdescription.cpp, rawmaterialcomponent.cpp, rawmaterialcomponent_funcs.h, rawmaterialdescription.cpp, residence_heattick.cpp, residencecomponent.cpp, residencecomponent_funcs.h, residencedescription.cpp, resourceproduce_creatework.cpp, resourceproducecomponent_states.cpp, selectdescription.cpp, spawncomponent.cpp, spawncomponent_funcs.h, spawndescription.cpp, statemachinecomponent.cpp, statemachinecomponent_funcs.h, statemachinedescription.cpp, statusicon_geometry.cpp, statusiconcomponent.cpp, statusiconcomponent_funcs.h, statusicondescription.cpp, storagecomponent.cpp, storagecomponent_funcs.h, storagedescription.cpp, storagedisplaycomponent_funcs.h, storagedisplaydescription.cpp, storagelocationcomponent.cpp, storagelocationdescription.cpp, terraformcomponent.cpp, terraformcomponent_funcs.h, terraformdescription.cpp, tombstonecomponent.cpp, tombstonecomponent_funcs.h, tombstonedescription.cpp, toolbardescription.cpp, tornadocomponent.cpp, tornadocomponent_funcs.h, tornadodescription.cpp, trackercomponent.cpp, trackerdescription.cpp, tradecomponent.cpp, tradecomponent_dock.cpp, tradecomponent_funcs.h, tradecomponent_livestock.cpp, tradecomponent_serialize.cpp, tradecomponent_states.cpp, tradedescription.cpp, uicomponent.cpp, uicomponent_serialize.cpp, uidescription.cpp, valuedescription.cpp, wellcomponent.cpp, wellcomponent_funcs.h, welldescription.cpp, wildanimalcomponent.cpp, wildanimalcomponent_funcs.h, wildanimaldescription.cpp, workcomponent.cpp, workcomponent_funcs.h, workdescription.cpp, workplacecomponent.cpp, workplacecomponent_funcs.h, workplacedescription.cpp, zonecomponent.cpp, zonecomponent_funcs.h, zonedescription.cpp

### C3 (98 files)

- `code/Game/Work/`: AutoPickupWork.h, BuildRoad.h, BuildStructure.h, ClearArea.h, ClearStorage.h, ConsumeProduce.h, FireWork.h, FishingWork.h, ForesterWork.h, GatherRawMaterials.h, HarvestArea.h, HarvestOrchard.h, HerbalistWork.h, HuntWork.h, LocationWork.h, MarketWork.h, PastureWork.h, Pickup.h, PlantArea.h, PlantOrchard.h, TendArea.h, TradeWork.h, Work.h, autopickupwork.cpp, build_actions.cpp, buildroad.cpp, buildroad_funcs.h, buildstructure.cpp, buildstructure_funcs.h, cleararea.cpp, cleararea_funcs.h, clearstorage.cpp, clearstorage_funcs.h, consumeproduce_funcs.h, firework.cpp, firework_funcs.h, firework_search.cpp, fishingwork.cpp, fishingwork_funcs.h, foresterwork.cpp, foresterwork_funcs.h, gatherrawmaterials.cpp, gatherrawmaterials_ctor.cpp, gatherrawmaterials_funcs.h, harvestarea.cpp, harvestarea_funcs.h, harvestorchard.cpp, harvestorchard_ctor.cpp, harvestorchard_funcs.h, herbalistwork.cpp, herbalistwork_funcs.h, huntwork.cpp, huntwork_funcs.h, locationwork.cpp, locationwork_funcs.h, marketwork.cpp, marketwork_bst_a9f0.cpp, marketwork_bst_a9f0.h, marketwork_bst_aaa0.cpp, marketwork_bst_aaa0.h, marketwork_bst_afb0.cpp, marketwork_bst_afb0.h, marketwork_bst_b290.cpp, marketwork_bst_b290.h, marketwork_bst_b680.cpp, marketwork_bst_b680.h, marketwork_bst_ba70.cpp, marketwork_bst_ba70.h, marketwork_funcs.h, pasturework.cpp, pasturework_funcs.h, pickup.cpp, pickup_funcs.h, pickup_gate.cpp, plantarea.cpp, plantarea_funcs.h, plantorchard.cpp, plantorchard_ctor.cpp, plantorchard_funcs.h, tendarea.cpp, tendarea_ctor.cpp, tendarea_funcs.h, tradework.cpp, tradework_funcs.h, tradework_serialize.cpp, work_act_consume.cpp, work_act_rawmat.cpp, work_base_accessor.cpp, work_base_bst.cpp, work_base_cellbst.cpp, work_base_grid.cpp, work_base_impl2.cpp, work_base_standalone.cpp, work_base_walkpos.cpp, work_leaf_clear.cpp, work_leaf_gather.cpp, work_leaf_hunt.cpp, work_leaf_market.cpp

### C4 (75 files)

- `code/Game/Action/`: AnimateNaturalResource.h, CompleteRoad.h, CompleteTask.h, ConsumeRawMaterial.h, CreateNaturalResource.h, CreateRawMaterial.h, DropOffRawMaterial.h, HuntAnimal.h, MoveTo.h, PerformWork.h, PickupRawMaterial.h, PickupStoredRawMaterial.h, PlayAnimation.h, RemoveAnimal.h, RemoveNaturalResource.h, SendComponentMessage.h, ShowTool.h, action.cpp, action_base_cnr.cpp, action_base_hunt.cpp, action_base_move.cpp, action_base_movedeps.cpp, action_base_rawmat.cpp, action_child_ctors.cpp, action_child_ctors2.cpp, action_pf_astar.cpp, action_pf_avl.cpp, action_pf_ctors.cpp, action_pf_geom.cpp, action_pf_pools.cpp, action_pf_region.cpp, action_pf_road.cpp, action_pf_roots_drop.cpp, action_pf_roots_misc.cpp, action_pf_roots_road.cpp, action_pf_roots_route.cpp, animatenaturalresource.cpp, completeroad.cpp, completetask.cpp, consumerawmaterial.cpp, createnaturalresource.cpp, createrawmaterial.cpp, dropoffrawmaterial.cpp, huntanimal.cpp, keystone_a_zone.cpp, moveto.cpp, performwork.cpp, pickuprawmaterial.cpp, pickupstoredrawmaterial.cpp, playanimation.cpp, region_graph_build.cpp, region_paircache.cpp, removeanimal.cpp, removenaturalresource.cpp, sendcomponentmessage.cpp
- `code/Game/Entity/`: ComponentEntity.h, citizenbehavior.cpp, citizenbehavior_funcs.h, componententity.cpp, componententity_funcs.h, entitylist.cpp, entitylist_funcs.h
- `code/Game/Goal/`: BuildingGoal.h, PopulationGoal.h, StatGoal.h, TimeGoal.h, TimeGoalInstance.h, buildinggoal.cpp, goal_exe_helpers.cpp, goal_exe_helpers.h, populationgoal.cpp, statvaluedispatch.cpp, statvaluedispatch.h, timegoal.cpp, timegoalinstance.cpp

### C5 (309 files)

- `code/Game/Dialog/`: AchievementDialog.h, AttendanceUI.h, AutoSaveDialog.h, BuildRequirementUI.h, BuildUI.h, CemeteryUI.h, CitizenUI.h, ConsumeProduceUI.h, ControlUI.h, ControlUIConfig.h, CropUI.h, DebugUI.h, DestroyUI.h, EntityUI.h, EventLogUI.h, EventNotifyDialog.h, EventPopupDialog.h, FollowDialog.h, GoalDialog.h, HelpUI.h, HotKeyToolTipUI.h, ImmigrationUI.h, LoaderDialog.h, MainMenuDialog.h, MapParamsDialog.h, MapUI.h, MessageDialog.h, ModBrowseUI.h, ModDetailsDialog.h, ModDialog.h, ModUploadDialog.h, NewGameDialog.h, NoiseUI.h, OptionInputBindDialog.h, OptionsDialog.h, OrchardUI.h, PageToggleUI.h, PastureUI.h, ProductionUI.h, ProfessionUI.h, RadiusUI.h, ResidenceUI.h, ResourceLimitUI.h, ResourceLimitUIConfig.h, SaveLoadDialog.h, ScenarioDialog.h, SelectResourceUI.h, SpriteCaptureUI.h, StatusBarUI.h, StatusIconUI.h, StatusIconUIConfig.h, StorageCapacityUI.h, StorageUI.h, TitleDescToolTipUI.h, ToolUI.h, ToolbarDialog.h, TownHallUI.h, TradeUI.h, UpgradeUI.h, UpgradeUIConfig.h, WorkPlaceUI.h, achievementdialog.cpp, attendanceui.cpp, autosavedialog.cpp, buildrequirementui.cpp, buildui.cpp, cemeteryui.cpp, citizen_icon_helpers.h, citizenui.cpp, consumeproduceui.cpp, controlui.cpp, controluiconfig.cpp, cropui.cpp, cropui_promote.cpp, debugui.cpp, destroyui.cpp, dialog_exe_helpers.cpp, dialog_exe_helpers.h, entityui.cpp, eventlogui.cpp, eventnotifydialog.cpp, eventpopupdialog.cpp, followdialog.cpp, goaldialog.cpp, helpui.cpp, hotkeytooltipui.cpp, immigrationui.cpp, loaderdialog.cpp, mapparamsdialog.cpp, messagedialog.cpp, mod_dialog_vtables.cpp, modbrowseui.cpp, moddetailsdialog.cpp, moduploaddialog.cpp, optioninputbinddialog.cpp, optionsdialog.cpp, orchardui.cpp, pagetoggleui.cpp, pastureui.cpp, productionui.cpp, professionui.cpp, radiusui.cpp, radiusui_promote.cpp, residenceui.cpp, resourcelimitui.cpp, resourcelimituiconfig.cpp, scenariodialog.cpp, selectresourceui.cpp, spritecaptureui.cpp, statusbarui.cpp, statusiconui.cpp, statusiconuiconfig.cpp, storagecapacityui.cpp, storageui.cpp, titledesctooltipui.cpp, toolbar_utility_helpers.cpp, toolbar_vtable_standalone.cpp, toolui.cpp, townhall_helpers.cpp, townhallui.cpp, tradeui.cpp, tradeui_buyentries.cpp, tradeui_checkout.cpp, tradeui_leaves.cpp, tradeui_price.cpp, tradeui_sort.cpp, ui_dialog_stub_factories.cpp, ui_dialogs.h, ui_panels.h, unreg_lane2_mapparamsdialog_ctor.cpp, upgradeui.cpp, upgradeuiconfig.cpp, workplaceui.cpp
- `code/Game/Game/`: ClimateGenerator.h, Disease.h, MapGenerator.h, MenuScene.h, MusicList.h, NameDatabase.h, PathfinderThread.h, Profession.h, ProfessionList.h, Scenario.h, TerrainGenerator.h, Toolbar.h, Tornado.h, Weather.h, WeatherParticleGeometry.h, WorldGeometry.h, builder_pool_driver.cpp, camera_input.cpp, charmanager_seed.cpp, citizen_names.cpp, citizenlist_serialize.cpp, climategenerator.cpp, construction_force_harness.cpp, disease.cpp, eventlog.cpp, eventlog_achievement.cpp, force_work.cpp, force_work.h, forester_force_harness.cpp, founding_spawn.cpp, game.cpp, game.h, game_main.h, game_settings_persist.cpp, gamegrid_serialize.cpp, gameupdateloop_load.cpp, gameupdateloop_teardown.cpp, geometry_draw.cpp, geometry_serialize.cpp, goals_serialize.cpp, inputbindings.cpp, inputbindings.h, mapgenerator.cpp, mapquery.cpp, maptilegrid.cpp, menudialogloopt_funcs.h, menuscene.cpp, musiclist.cpp, namedatabase.cpp, profession.cpp, professionlist.cpp, region_seed.cpp, scenario.cpp, serialize_arrays.cpp, serialize_externals.cpp, serialize_externals.h, serialize_externals_array.cpp, serialize_externals_typed.cpp, serialize_heightmap_mesh.cpp, serialize_resize_helpers.cpp, serialize_struct60b.cpp, serialize_struct60b_serialize.cpp, starttown.cpp, statemachine.cpp, statemachine_funcs.h, terraingenerator.cpp, toolbar.cpp, tornado.cpp, town_storage_seed.cpp, town_workplace_registry.cpp, uifade.cpp, uifade.h, unreg_lane5_musicplayer.cpp, water_contour_mesh.cpp, weather.cpp, weather_particle_draw.cpp, weather_particles.cpp, weather_sim.cpp, weather_sun.cpp, worldgen.cpp, worldgen_f390_coastline.cpp, worldgen_f390_erosion.cpp, worldgen_f390_ranges.cpp, worldgen_f390_rivers.cpp, worldgen_f390_shims.cpp, worldsetup_reload.cpp
- `code/Game/Player/`: BridgeTool.h, DragPlacementTool.h, EditTool.h, FencedTool.h, PathTool.h, PlacementTool.h, RemovalTool.h, RoadTool.h, RotationSetting.h, Tool.h, ToolSetting.h, bridgetool.cpp, bridgetool_calcpath.cpp, bridgetool_draw.cpp, bridgetool_validate.cpp, dragplacementtool.cpp, dragtool.cpp, dragtool.h, edittool.cpp, edittool_funcs.h, fencedtool_calcpath.cpp, keystone_a_removalcalcpath.cpp, pathtool.cpp, pathtool_setselected.cpp, removaltool.cpp, roadtool.cpp, roadtool_calcpath.cpp, roadtool_funcs.h, rotationsetting.cpp, tool.cpp, tool_activation_standalone.cpp, tool_base_createbar.cpp, tool_base_drag.cpp, tool_base_edit.cpp, tool_base_edit2.cpp, tool_base_highlight.cpp, tool_base_highlight_tess.cpp, tool_base_path.cpp, tool_base_place.cpp, tool_base_ray.cpp, tool_base_screenrect.cpp, tool_base_tile.cpp, tool_base_tilerender.cpp, tool_base_toolui.cpp, tool_base_trade.cpp
- `code/Game/Tutorial/`: FinishTutorialGoal.h, FinishTutorialInstance.h, Tutorial.h, TutorialGoal.h, TutorialGoalButton.h, TutorialGoalCount.h, TutorialGoalDialog.h, TutorialGoalGameSpeed.h, TutorialHighlightDialog.h, TutorialImage.h, TutorialImageHotKey.h, TutorialImageInputKey.h, TutorialImageToolbarIcon.h, TutorialInstance.h, TutorialSetup.h, TutorialSetupCamera.h, TutorialSetupDialog.h, TutorialSetupGameSpeed.h, TutorialSetupRestrictPlayer.h, TutorialStep.h, TutorialUI.h, find_ui_element_by_path.cpp, finishtutorialgoal.cpp, finishtutorialinstance.cpp, tutorial.cpp, tutorial_base.cpp, tutorial_dialog_vtables.cpp, tutorial_wave.cpp, tutorialgoal.cpp, tutorialgoalbutton.cpp, tutorialgoalcount.cpp, tutorialgoaldialog.cpp, tutorialgoalgamespeed.cpp, tutorialhighlightdialog.cpp, tutorialimage.cpp, tutorialimagehotkey.cpp, tutorialimageinputkey.cpp, tutorialimagetoolbaricon.cpp, tutorialinstance.cpp, tutorialsetup.cpp, tutorialsetupcamera.cpp, tutorialsetupdialog.cpp, tutorialsetupgamespeed.cpp, tutorialsetuprestrictplayer.cpp, tutorialstep.cpp

### CLOSE (13 files)

- `standalone/`: reg_citizenai_typeids.cpp, reg_cleararea_typeids.cpp, reg_economy_typeids.cpp, reg_element_typeids.cpp, reg_food_typeid.cpp, reg_growth_typeid.cpp, reg_market_typeid.cpp, reg_orchard_typeid.cpp, reg_pipeline_typeids.cpp, reg_tracker_typeid.cpp, reg_tradeui_typeids.cpp, reg_tutorial_typeid.cpp, reg_unreg_lane4.cpp

## 2. The shared files

### 2.1 Coordinator-owned (a block never edits them; it hands in)

| file | what a block hands in | when COORD applies it |
|---|---|---|
| `standalone/rt_data_mint_tables.inc` | nothing — REGENERATED (`rt_mint_gen.py`) from the fragments; on a merge conflict take either side and regenerate | every merge |
| `standalone/rt_export_addrs.inc` | nothing (DLL-derived; `git checkout` it if the generator only re-ended its lines) | never changes in step 9 |
| `tools/rt_mint_gen.py` | the `KNOWN_CTORS` / `EXTRA_INIT` / `EXTRA_VTABLES` / `EXTRA_STORAGE_BY_ADDR` entries of a row the block retired | in the merge of that commit |
| `standalone/rt_data_mint.cpp` | W0: the redirect (D7). Later: "delete `ConstructLoader` / `ConstructParticleRng` / the `_loader` interior doors" with the row that owned it | in that merge |
| `standalone/cutover_manifest.{cpp,inc}`, `tools/cutover_manifest.py`, `standalone/reg_mint_promoted.cpp` | nothing: `cutover_manifest.py --inc` regenerates the `.inc` at merge (a REQUIRED row disappears when the census stops seeing a door) | every merge; the machinery goes in CLOSE when the manifest is empty |
| `engine/funcs/engine_funcs.{h,cpp}` | DELETE-ONLY for the block's own zero-use globals (§2.2); any other change is a hand-in line `engine_funcs: <global> -> <replacement>` | §2.2 applies directly |
| `standalone/resolve_standalone.cpp`, `runtime/funcs/rt_port.h` (`RT_DATA_SLOT`), `runtime/funcs/rt_resolve_cache.cpp`, `runtime/funcs/runtime_funcs.*`, `runtime/types/common.h` | a change request with the reason | the macro goes when its last use does (CLOSE) |
| `Makefile`, `tests/unit/sources.mk`, `tests/unit/{main.cpp,unit.h,link_stubs.cpp,rt_port_stub.cpp,collection_link_stubs.cpp}` | nothing normally (`UNIT_SRCS` is a wildcard); a link stub a test needs | on request |
| `standalone/options.{h,cpp}` | `Opt().factoryEnum` deletion (W0 does it) | W0 |

### 2.2 DELETE-ONLY shared files (B9-12)

These files hold ROWS of many classes. Any block may DELETE the rows of ITS OWN classes in them — nothing
else: no reformatting, no reordering, no edits to a neighbour. The coordinator resolves a merge conflict by
keeping every side's deletions (the pilot made the `RT_DELDTOR` table position-free for exactly this).

| file (owner of everything else) | a block deletes |
|---|---|
| `runtime/src/object/deleting_dtor.cpp` (OBJ) | its classes' `RT_DELDTOR(...)` lines, `RtDelDtor_X` wrappers, `RegisterRuntimeAddr(0xDDVA, …)` lines (B9-2) |
| `runtime/src/object/deleting_dtor_leaves.cpp` (OBJ) | the `??1` doors of its classes and their registration lines; fix the `return N` count |
| `runtime/src/object/rt_factories_core.cpp`, `rt_factories.h`, `runtime/src/object/README`-style lists (OBJ) | the call and declaration of its namespace's `rt_factories_<ns>` pair when it deletes that pair (its own file) |
| `runtime/src/ui/factory_ui1.cpp`, `factory_ui1_tables.inc`, `ui_factory_wave28.cpp` (UI-D) | the rows / thunks of its classes (UI-A, UI-B) |
| `engine/funcs/engine_funcs.{h,cpp}` (COORD) | a global that has ZERO uses after its commit, by binding (the `EngineFuncsDump` `printf` is not a use): typedef + extern + definition + initialiser + dump line, in the same commit (pilot c2 did this for `fn_Graphics_SunLight_Ctor`) |
| `standalone/address_registry.cpp`, `standalone/reg_*.cpp` (EXE / CLOSE) | a `RegisterExeAddr` / `Register*Slot` row whose every reader the block's commit removed |

The `rt_factories_<ns>.{cpp,inc}` pairs themselves belong to the namespace block (they are in its §1 list).

### 2.3 Hand-in format (every block's report ends with it)

One fenced block per kind, one line per item, so the coordinator can apply them mechanically:

```
RETIRED <fragment file> <n rows>                       # the fragment lines are in the commit; COORD regenerates
GEN-ENTRY <rt_mint_gen.py table> <key>                 # an EXTRA_* / KNOWN_CTORS entry to delete
MINT-CPP <function or line> <reason>                   # rt_data_mint.cpp hunk
ENGINE-FUNCS <global> zero-use | -> <replacement>      # if not already deleted under 2.2
EXE-ROW <file>:<line> <0xaddr> reader-less after <commit>
TYPEID-DEAD <cell 0x1401c...> <class> last reader removed in <commit>       # caller batches -> CLOSE
STORAGE-LAST <row> readers left: <file:line ...>       # for CLOSE
FOREIGN <file>:<line> <what>                            # a section-3 edit you made
BEHAVIOUR <commit> <one line>                           # every titled behaviour commit, for resave x5
BOOTLINE <log line> <old> -> <new>                      # every boot-line change the coordinator should expect
```

## 3. Foreign sites (B9-13)

A construction or storage site of a block's class that sits in another block's file. The class's owner edits
that ONE line (and reports it as `FOREIGN`); the file's owner does not touch it in the same wave.

| site | class (owner) | what |
|---|---|---|
| `runtime/src/ui/userinterface.cpp:1490` (UI-D file) | IO::Encoding table, unexported row 0 (IO) | the address stamp → the new class's ctor |
| `runtime/src/engine/singleton_ctors.cpp:444` (ENG file) | PersistenceWorker, unexported row 1 (IO) | same |
| `runtime/src/engine/singleton_ctors.cpp` `Publish` lines of each singleton (ENG file) | the singleton's class (OBJ, SYS, IO, VID, ENG) | the store into the storage cell → the class static (D7) |
| `runtime/src/engine/engine_lifecycle.cpp` singleton stores (`:342/373/439/469/691`, ENG file) | System::Configuration, Error (SYS); AchievementManager, Engine (ENG) | same |
| `runtime/src/engine/engine_platform.cpp` `=nullptr` resets (ENG file) | Video / Input / Audio Configuration | same |
| `standalone/rt_data_mint.cpp` `ConstructLoader` / `ConstructParticleRng` (COORD) | `_loader` (ENG), `_rng` (GFX) | hand-in `MINT-CPP` — COORD applies it in the same merge |
| `code/Game/Game/menudialogloopt.cpp` (EXE) | MeshInstance, ParticleSystemInstance (GFX) | EXE converts (it owns the file) once GFX's ctor exists — not a foreign site |

Any other cross-block need is a hand-in, never an edit.
