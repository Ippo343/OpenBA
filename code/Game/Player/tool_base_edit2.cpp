// code/Game/Player/tool_base_edit2.cpp
// Player registration wave, round 2c — batch "edit2" (edittool.cpp deep tier).
//
// Target addrs (both resolved from code/Game/Player/edittool.cpp):
//   0x1400fcc90  Entity_impl_cc90  — fallback positioning when entity rect fails
//   0x140100380  Entity_impl_0380  — entity screen bounding-rectangle
//
// VERDICT (2026-06-19): BOTH DEFERRED. No bodies are provided and nothing is
// registered (see standalone/reg_player_edit2.cpp, an empty registrar). Each
// pulls in an unregistered, hard-to-port closure; a half-null / wrong-ABI port of
// a function that is DORMANT on the autostart path is undetectable silent
// corruption — strictly worse than the safe no-op of leaving the call site skipped.
//
// The closure analysis below is the verified starting point for the dedicated
// sub-wave that will land these (it needs the EdgeMesh tessellation chain and the
// Math dispatch-slot family registered first).
//
// ============================================================================
// 0x1400fcc90  Entity_impl_cc90  — DEFER (render-mesh closure)
// ============================================================================
// void Entity_impl_cc90(longlong self)
//   -> Entity_impl_cb40(self)                                  [0x1400fcb40]
//   -> UI::DialogInstance::GetController / UI::UserInterface::CloseDialog  (Runtime exports, OK)
//   -> fn_InternalBasePtr_Ctor / fn_InternalBasePtr_CheckPtr_slot / fn_IsDerivedFrom (engine_funcs, OK)
//   -> reads global DWORD_1401c5788 (type-id, read via ResolveAddr — OK)
//   -> reads g_pGameWorldSingleton+0x98 (UserInterface) (OK)
//
//   Entity_impl_cb40 (0x1400fcb40):
//     -> FUN_140004740                         [0x140004740  REGISTERED]
//     -> thunk_helper_d250_sub_7080            [0x140007080  UNREG — leaf, portable:
//                                               FindComponent loop, type DAT_1401c4410,
//                                               IsDerivedFrom + CheckPtr only]
//     -> Entity_impl_c4a0                       [0x14004c4a0  UNREG — RENDER MUTATION]
//
//   Entity_impl_c4a0 (0x14004c4a0) is the blocker. It rebuilds an
//   EdgeMeshInstance and (re)adds it to the Scene:
//     -> Graphics::Scene::RemoveObject / AddObject                 (Runtime exports)
//     -> Graphics::EdgeMeshInstance::SetMesh / Geometry::SetTransform (Runtime exports)
//     -> System::Time::Time                                        (Runtime export)
//     -> FUN_1400c3470                          [0x1400c3470  REGISTERED]
//     -> FUN_140004680 / Entity_ClearPathQueue  [0x140004680 / 0x140003f60  REGISTERED]
//     -> thunk_helper_d250_sub_7080             [0x140007080  UNREG]
//     -> FUN_140004740                          [0x140004740  REGISTERED]
//     -> TradeWork_impl_b930                    [0x14004b930  UNREG — ~250-line
//                                                EdgeMesh tessellation/vertex-buffer
//                                                builder; grows MemAlloc/MemFree arrays,
//                                                calls TradeWork_impl_b440 (UNREG),
//                                                FUN_1400b10e0 (UNREG), and the full
//                                                Graphics::EdgeMesh::* + Volume::Box::*
//                                                export set]
//
//   => DEFER. Registering cc90 would fire the c4a0 render rebuild whose b930
//      tessellation closure (b440/b10e0) is unregistered → builds a half-populated
//      EdgeMesh against the live Scene. Must be its own careful render sub-wave;
//      register bottom-up: b440, b10e0, 7080 -> b930 -> c4a0 -> cb40 -> cc90.
//
// ============================================================================
// 0x140100380  Entity_impl_0380  — DEFER (15 unregistered Math dispatch slots)
// ============================================================================
// void Entity_impl_0380(longlong self, InternalBasePtr* entityIBP, undefined8* rectOut)
//   Computes an entity's screen-space bounding rectangle by transforming its
//   world bounding-box corners and projecting them through the camera.
//
//   exe sub-callees:
//     -> Entity_ClearPathQueue / FUN_140004680  [0x140003f60 / 0x140004680  REGISTERED]
//     -> CharacterComponent_impl_ec70           [0x1400fec70  UNREG — portable leaf:
//                                                builds 4 footprint corners; its only
//                                                exe sub-callee is CharacterComponent_impl_f3e0
//                                                = 0x14009f3e0, REGISTERED]
//     -> FUN_1400fe960                          [0x1400fe960  UNREG:
//                                                -> FUN_140098210 (0x140098210, UNREG —
//                                                   camera world->screen projection:
//                                                   Transform3/Matrix/Vector4 math)
//                                                -> Math::Vector2::Min/Max/operator*= (slots)
//                                                -> g_pGameSettingsPtr / DAT_140172208 consts]
//
//   The Math operations in 0380 + fe960 + 098210 are ALL invoked through exe IAT
//   dispatch slots (0x14015xxxx), and NONE of these 15 are registered:
//     0380:    0x1401528a8 0x140152a78 0x140152ab0(Vector3::Vector3 ctor, x6)
//              0x140152950 0x140152900 0x140151470
//              (Volume::Rectangle::Rectangle, Math::Vector3::Vector3, Math::Vector3::Min,
//               Volume::Box::GetCorners, Math::Transform::operator*)
//     fe960:   0x140152780(Math::Vector2::Max) 0x140152788(Math::Vector2::Min)
//              0x140151e70 (Math::Vector2::operator*=)  + FUN_140098210
//     098210:  0x140151f00(Math::Vector4::Vector4) 0x140151868 0x1401522d0
//              0x1401520a0 0x1401522c8 0x1401522c0
//              (Math::Transform3::Transform3/Inverse/AsMatrix, Math::Matrix::operator*,
//               Math::Matrix::operator*(Vector4))
//
//   => DEFER. The exe-code closure is portable, but the value-returning Math ABI
//      surface is 15 unregistered dispatch slots (RCX=this, RDX=&retbuf for the
//      >8-byte returns). A wrong ABI on any one = silent corruption on a dormant
//      path the A/B test cannot catch. This needs its own Math-slot sub-wave:
//      first RegisterDispatchSlot each slot -> its Runtime export (or add fn_*
//      exports + rewrite the bodies to call them), then port ec70 (trivial),
//      098210, fe960, and finally 0380.
//
// No code is emitted in this file — it is documentation only for the deferred set.
