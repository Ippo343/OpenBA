// ============================================================================
// tests/unit/graphics_renderer_abi_test.cpp — ENDGAME B.3 step 5, block F
//   `Graphics::Camera`, `Graphics::Renderer` and `Graphics::Scene` as real
//   C++ classes, plus the four element types of Renderer's three arrays
//
// The sibling of `graphics_geometry_abi_test.cpp` (block E) and
// `io_vtable_abi_test.cpp`, for the seven classes block F converts:
//
//   Graphics::Camera             NON-POLYMORPHIC root, 0xd8   (Camera.h)
//   Graphics::RendererDrawable   0x10                         (Camera.h)
//   Graphics::RendererDrawItem   0x38                         (Camera.h)
//   Graphics::RendererDrawGroup  0x1f0                        (Camera.h)
//   Graphics::RendererSceneEntry 0x110, retail `Renderer::SceneGroup` (Camera.h)
//   Graphics::Renderer           NON-POLYMORPHIC root, 0x50   (Camera.h)
//   Graphics::Scene              NON-POLYMORPHIC root, 0x78   (Scene.h)
//   Graphics::SceneNode          0x18, retail `Scene::Node`    (Scene.h)
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// None of these seven has a vtable, so there is no slot order to pin and PART B
// of the Geometry test has no analogue here. What binds instead is LAYOUT and
// two ABI properties that a later edit could silently break:
//
// PART A — LAYOUT. Every size, alignment and field offset the two headers claim,
//   measured here as well as `static_assert`ed there, so a header edit that moves
//   a field fails a TEST and not only a build somebody may be tempted to "fix".
//   It also pins the three EMBEDDINGS that fix `sizeof(Camera)` independently of
//   Camera's own field list — `RendererDrawGroup`'s two adjacent Cameras and
//   `RendererSceneEntry`'s one — and the eight POD cross-asserts that keep the
//   `RtGraphics` mirrors from drifting.
//
// PART B — TRIVIAL COPYABILITY (RULE B5-4), IN BOTH DIRECTIONS. `Camera` is a
//   by-value member of two `Collection::Array<T>` element types, so it must stay
//   trivially copyable; `RendererDrawGroup` and `RendererSceneEntry` must NOT be,
//   because each embeds a `Collection::Array<T>` whose destructor is what
//   `rtcoll::SpecFor<T>`'s destroy hook (and retail's 0x18001c8c0 / 0x18001ce90)
//   runs. Both halves are asserted, because getting either wrong changes which
//   body `array.cpp` runs on a resize.
//
// PART C — THE HIDDEN RETURN SLOT ON A NON-VIRTUAL BY-VALUE RETURN. `Camera` has
//   four of them (`GetFrustum`, `GetProjection`, `GetCameraToWorld` ×2) and
//   `SunLight::GetSunCamera` returns a whole `Camera` by value. Block E measured
//   that GCC's ms_abi passes the hidden return pointer BEFORE `this`, which is
//   why a VIRTUAL by-value return must spell an out-parameter in retail order
//   (Geometry.h's B5-12 block). PART C shows the other half of that finding: for
//   a NON-VIRTUAL member the same swap is invisible, because both ends of the
//   call are this same GCC — and that the DOOR shape the tree uses,
//   `(this, ret)` with `RAX = ret`, reproduces retail's ABI exactly when the door
//   placement-news the member's prvalue. A `void` door would be the 2026-09-13
//   perf-only crash (`1a4200c3`) again.
//
// PART D — THE QUALIFIED CALL. Doors delegate with
//   `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`. On a NON-POLYMORPHIC
//   class the qualification cannot change dispatch, and PART D says so as a
//   measurement rather than as an assumption: a hand-built table stamped over the
//   object is not consulted by either spelling, so no door in block F's set can
//   re-enter itself the way B34_BRIEFS §A.1 warns about for a polymorphic one.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC, and nothing about the ported bodies. NO TREE SOURCE IS
// LINKED: every body of these seven classes lives in `runtime/src/graphics/
// {renderer,scene,cull,sun_camera,wave9_lane_gfxd,wave29_lane_gfxbody,
// closure_batch2b}.cpp`, which stand on `RegisterRuntimeExport`, `ResolveAddr`,
// `RT_EXPORT_ONCE` and `RT_DATA_SLOT` and cannot link into a host test (the same
// reason `external_class_doors.cpp` is kept out — tests/unit/sources.mk). The
// binding binary facts here are OBJECT LAYOUT and the retail accessor bodies the
// offsets were read out of; the arithmetic in `IsMirrored`, `GetSunCamera` and
// `Scene::Clean` is verified by the real run.
// `Math::Transform3` / `Math::Matrix` / `::Frustum` ARE the real classes
// (`runtime/src/math/` is on the UNIT_TREE_SRCS list), so PART C's retslot values
// are real ones.
// ============================================================================
#include "unit.h"

// The shipped headers. The tests measure THESE, not local copies, so a failure
// means the header and the ABI disagree.
#include "runtime/types/graphics/Camera.h"
#include "runtime/types/graphics/Scene.h"

#include <cstdint>
#include <cstddef>
#include <new>
#include <type_traits>

namespace {

// Byte offset of a member measured from two live addresses, for the cases where
// `offsetof` would be awkward. Every class here is standard-layout, so both
// spellings are available and the two are cross-checked in PART A.
template <typename Obj, typename Field>
uintptr_t FieldOffset(const Obj* obj, const Field* field) {
    return reinterpret_cast<uintptr_t>(field) - reinterpret_cast<uintptr_t>(obj);
}

// Raw storage for a class whose default constructor is deliberately not
// accessible (`Graphics::Camera`, RULE B5-4) or whose real constructor is a
// registered DOOR (`Graphics::Renderer`, `Graphics::Scene`). Nothing here
// CONSTRUCTS one: these tests read layout, and a `reinterpret_cast` over aligned
// bytes is exactly what every minted row and every hand-inlined caller does.
template <typename T>
struct RawOf {
    alignas(T) unsigned char bytes[sizeof(T)];
    RawOf() { for (unsigned char& b : bytes) b = 0; }
    T*       ptr()       { return reinterpret_cast<T*>(bytes); }
    const T* ptr() const { return reinterpret_cast<const T*>(bytes); }
};

}  // namespace

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================

UNIT_TEST(graphics_renderer_family_sizes_match_retail) {
    // Camera: 0xd8, pinned three ways over and above its own field list — see
    // graphics_camera_is_embedded_three_times below.
    CHECK_EQ(sizeof(Graphics::Camera), size_t(0xd8));
    CHECK_EQ(alignof(Graphics::Camera), size_t(4));   // widest member is a float

    // Renderer: 0x50. `engine/funcs/engine_funcs.h:3081` says the same
    // independently ("Object size 0x50, alignment 4").
    CHECK_EQ(sizeof(Graphics::Renderer), size_t(0x50));

    // The three element strides, each read out of its own array helper's
    // `IMUL`: 0x10 (0x180062c40), 0x38 (0x180063680) and 0x1f0 — the last
    // confirmed three independent ways in wave29_lane_gfxbody.cpp's banner
    // (`IMUL R8,R8,0x1f0`, `IMUL RCX,RCX,0x1f` + `SHL RCX,4`, and
    // `Array<RendererDrawGroup>::SetSize`).
    CHECK_EQ(sizeof(Graphics::RendererDrawable), size_t(0x10));
    CHECK_EQ(sizeof(Graphics::RendererDrawItem), size_t(0x38));
    CHECK_EQ(sizeof(Graphics::RendererDrawGroup), size_t(0x1f0));

    // 0x110, from `?GetSceneGroup@Renderer@Graphics@@QEBAAEBUSceneGroup@12@H@Z`
    // (0x180018090): `MOVSXD RAX,EDX; IMUL RAX,RAX,0x110; ADD RAX,[RCX]`.
    CHECK_EQ(sizeof(Graphics::RendererSceneEntry), size_t(0x110));

    // Scene: 0x78, and its grid node: 0x18.
    CHECK_EQ(sizeof(Graphics::Scene), size_t(0x78));
    CHECK_EQ(sizeof(Graphics::SceneNode), size_t(0x18));
}

UNIT_TEST(graphics_renderer_family_matches_the_rt_pods) {
    // B4b-1 keeps the `RtGraphics` PODs alive because they have live users in
    // eight files outside block F's set. These are the asserts that stop the two
    // descriptions drifting; the headers carry them too, and a TEST failure is
    // harder to "fix" past than a build failure.
    CHECK_EQ(sizeof(Graphics::Camera),             sizeof(RtGraphics::Camera));
    CHECK_EQ(sizeof(Graphics::Renderer),           sizeof(RtGraphics::Renderer));
    CHECK_EQ(sizeof(Graphics::RendererDrawable),   sizeof(RtGraphics::RendererDrawable));
    CHECK_EQ(sizeof(Graphics::RendererDrawItem),   sizeof(RtGraphics::RendererDrawItem));
    CHECK_EQ(sizeof(Graphics::RendererDrawGroup),  sizeof(RtGraphics::RendererDrawGroup));
    CHECK_EQ(sizeof(Graphics::RendererSceneEntry), sizeof(RtGraphics::RendererSceneEntry));
    CHECK_EQ(sizeof(Graphics::Scene),              sizeof(RtGraphics::Scene));
    CHECK_EQ(sizeof(Graphics::SceneNode),          sizeof(RtGraphics::SceneNode));

    CHECK_EQ(offsetof(Graphics::Camera, _cameraToWorld),
             offsetof(RtGraphics::Camera, cameraToWorld));
    CHECK_EQ(offsetof(Graphics::Camera, _projection),
             offsetof(RtGraphics::Camera, projection));
    CHECK_EQ(offsetof(Graphics::Camera, _zUp),
             offsetof(RtGraphics::Camera, swapZUpToYUp));
    CHECK_EQ(offsetof(Graphics::Renderer, _drawGroupsUsed),
             offsetof(RtGraphics::Renderer, field_38));
    CHECK_EQ(offsetof(Graphics::Renderer, _shaderDetail),
             offsetof(RtGraphics::Renderer, field_3c));
    CHECK_EQ(offsetof(Graphics::Renderer, _frameIndex),
             offsetof(RtGraphics::Renderer, contextSerial));
    CHECK_EQ(offsetof(Graphics::Renderer, _detailId),
             offsetof(RtGraphics::Renderer, field_48));
    CHECK_EQ(offsetof(Graphics::RendererDrawGroup, _materialTypeFlags),
             offsetof(RtGraphics::RendererDrawGroup, passMask));
    CHECK_EQ(offsetof(Graphics::Scene, _globals),
             offsetof(RtGraphics::Scene, _globals));
}

UNIT_TEST(graphics_camera_field_offsets) {
    RawOf<Graphics::Camera> raw;
    const Graphics::Camera* c = raw.ptr();

    // Each offset names the retail body that pins it.
    CHECK_EQ(FieldOffset(c, &c->_frustum),       uintptr_t(0x00));  // GetFrustum 0x1800fe3e0
    CHECK_EQ(FieldOffset(c, &c->_cameraToWorld), uintptr_t(0x64));  // 0x180017e70
    CHECK_EQ(FieldOffset(c, &c->_projection),    uintptr_t(0x94));  // 0x180017ed0
    CHECK_EQ(FieldOffset(c, &c->_zUp),           uintptr_t(0xd4));  // SetZUp 0x1800fe550
    CHECK_EQ(FieldOffset(c, &c->_pad_d5),        uintptr_t(0xd5));  // never written

    // `offsetof` and the live-address form must agree — they do only if the class
    // is standard-layout, which is the property that lets every minted row and
    // every `(*(void***)p)[N]` caller keep working.
    CHECK_EQ(offsetof(Graphics::Camera, _cameraToWorld), size_t(0x64));
    CHECK(std::is_standard_layout_v<Graphics::Camera>);

    // The embedded value types are the REAL Math / Volume classes, and their own
    // sizes are what make the three offsets above add up.
    CHECK_EQ(sizeof(::Frustum), size_t(0x64));
    CHECK_EQ(sizeof(Math::Transform3), size_t(0x30));
    CHECK_EQ(sizeof(Math::Matrix), size_t(0x40));
    CHECK_EQ(size_t(0x94) + sizeof(Math::Matrix), size_t(0xd4));
}

UNIT_TEST(graphics_renderer_field_offsets) {
    RawOf<Graphics::Renderer> raw;
    const Graphics::Renderer* r = raw.ptr();

    CHECK_EQ(FieldOffset(r, &r->_sceneGroups),       uintptr_t(0x00));  // GetSceneGroup 0x180018090
    CHECK_EQ(FieldOffset(r, &r->_drawGroups),        uintptr_t(0x10));  // GetCamera 0x180017f70
    CHECK_EQ(FieldOffset(r, &r->_drawables),         uintptr_t(0x20));
    CHECK_EQ(FieldOffset(r, &r->_currentSceneGroup), uintptr_t(0x30));  // 0x180018030
    CHECK_EQ(FieldOffset(r, &r->_currentDrawGroup),  uintptr_t(0x34));  // BeginDrawGroup 0x180104ffd
    CHECK_EQ(FieldOffset(r, &r->_drawGroupsUsed),    uintptr_t(0x38));  // BeginDrawGroup 0x18010509e
    CHECK_EQ(FieldOffset(r, &r->_shaderDetail),      uintptr_t(0x3c));  // SetShaderDetail 0x180018050
    CHECK_EQ(FieldOffset(r, &r->_alphaT),            uintptr_t(0x40));  // GetAlphaT 0x180018040
    CHECK_EQ(FieldOffset(r, &r->_frameIndex),        uintptr_t(0x44));  // GetFrameIndex 0x180018080
    CHECK_EQ(FieldOffset(r, &r->_detailId),          uintptr_t(0x48));  // GetDetailId 0x180018070
    CHECK_EQ(FieldOffset(r, &r->_pad_4c),            uintptr_t(0x4c));  // RULE B5-13, never written

    CHECK(std::is_standard_layout_v<Graphics::Renderer>);

    // Retail's own names for the three element types, reachable through the
    // member aliases so the nesting `?GetSceneGroup@` implies is not lost.
    CHECK(sizeof(Graphics::Renderer::SceneGroup) == sizeof(Graphics::RendererSceneEntry));
    CHECK(sizeof(Graphics::Renderer::DrawGroup)  == sizeof(Graphics::RendererDrawGroup));
    CHECK(sizeof(Graphics::Renderer::DrawItem)   == sizeof(Graphics::RendererDrawItem));
    CHECK(sizeof(Graphics::Renderer::Drawable)   == sizeof(Graphics::RendererDrawable));
}

UNIT_TEST(graphics_camera_is_embedded_three_times) {
    // THIS IS THE TEST THAT PINS sizeof(Camera) WITHOUT LOOKING AT CAMERA.
    //
    // A draw group holds TWO adjacent Cameras — `BeginDrawGroup` (0x180104fd0)
    // runs `Camera::operator=` (0x180017c40) on group+0x08 and again on
    // group+0xe0, `?GetCamera@Renderer@…@H@Z` (0x180017fb0) returns group+0x08 as
    // a `const Camera&` and `?GetCullCamera@…@H@Z` (0x180017fd0) returns
    // group+0xe0 — and the next field after the second one is `_shader` at
    // +0x1b8, which `?GetShader@Renderer@` (0x180018010) reads.
    RawOf<Graphics::RendererDrawGroup> rawGroup;
    const Graphics::RendererDrawGroup* g = rawGroup.ptr();
    CHECK_EQ(FieldOffset(g, &g->_renderBuffer),     uintptr_t(0x000));
    CHECK_EQ(FieldOffset(g, &g->_camera),           uintptr_t(0x008));
    CHECK_EQ(FieldOffset(g, &g->_cullCamera),       uintptr_t(0x0e0));
    CHECK_EQ(FieldOffset(g, &g->_shader),           uintptr_t(0x1b8));
    CHECK_EQ(FieldOffset(g, &g->_materialTypeFlags), uintptr_t(0x1bc));
    CHECK_EQ(FieldOffset(g, &g->_items),            uintptr_t(0x1c0));
    // 0x08 + sizeof(Camera) == 0xe0, and 0xe0 + sizeof(Camera) == 0x1b8.
    CHECK_EQ(size_t(0x0e0) - size_t(0x008), sizeof(Graphics::Camera));
    CHECK_EQ(size_t(0x1b8) - size_t(0x0e0), sizeof(Graphics::Camera));
    // The three per-pass lists fill the 0x1f0 exactly: 0x1c0 + 3*0x10.
    CHECK_EQ(size_t(0x1c0) + 3 * sizeof(Collection::Array<Graphics::RendererDrawItem>),
             sizeof(Graphics::RendererDrawGroup));

    // And a scene group holds one, at +0x10, with its next field at +0xe8 —
    // Camera.h's original derivation from `Renderer::Draw`.
    RawOf<Graphics::RendererSceneEntry> rawEntry;
    const Graphics::RendererSceneEntry* e = rawEntry.ptr();
    CHECK_EQ(FieldOffset(e, &e->_scene),             uintptr_t(0x000));
    CHECK_EQ(FieldOffset(e, &e->_renderBuffer),      uintptr_t(0x008));
    CHECK_EQ(FieldOffset(e, &e->_camera),            uintptr_t(0x010));
    CHECK_EQ(FieldOffset(e, &e->_shaderDetail),      uintptr_t(0x0e8));
    CHECK_EQ(FieldOffset(e, &e->_shader),            uintptr_t(0x0ec));
    CHECK_EQ(FieldOffset(e, &e->_materialTypeFlags), uintptr_t(0x0f0));
    CHECK_EQ(FieldOffset(e, &e->_drawGroupIndex),    uintptr_t(0x0f4));
    CHECK_EQ(FieldOffset(e, &e->_detailId),          uintptr_t(0x0f8));
    CHECK_EQ(FieldOffset(e, &e->_pad_fc),            uintptr_t(0x0fc));
    CHECK_EQ(FieldOffset(e, &e->_drawableIds),       uintptr_t(0x100));
    CHECK_EQ(size_t(0x010) + sizeof(Graphics::Camera), size_t(0x0e8));
}

UNIT_TEST(graphics_renderer_draw_item_and_drawable_pads) {
    // The two gaps RULE B5-13 requires stay. `AddDrawItem` (0x1801057b0) never
    // writes `_pad24` or `_pad34`; `AddLight` (0x1801051d0) writes twelve of
    // `RendererDrawable`'s sixteen bytes and leaves `_pad0c`. The array fill
    // helper copies the whole element regardless, so retail stores uninitialised
    // stack there on every append and NO oracle may compare those bytes.
    RawOf<Graphics::RendererDrawItem> rawItem;
    const Graphics::RendererDrawItem* it = rawItem.ptr();
    CHECK_EQ(FieldOffset(it, &it->_geometry), uintptr_t(0x00));
    CHECK_EQ(FieldOffset(it, &it->_material), uintptr_t(0x08));
    CHECK_EQ(FieldOffset(it, &it->_instance), uintptr_t(0x10));
    CHECK_EQ(FieldOffset(it, &it->_flags),    uintptr_t(0x18));
    CHECK_EQ(FieldOffset(it, &it->_arg5),     uintptr_t(0x1a));
    CHECK_EQ(FieldOffset(it, &it->_sortKey),  uintptr_t(0x1c));
    CHECK_EQ(FieldOffset(it, &it->_arg8),     uintptr_t(0x20));
    CHECK_EQ(FieldOffset(it, &it->_pad24),    uintptr_t(0x24));
    CHECK_EQ(FieldOffset(it, &it->_userData), uintptr_t(0x28));
    CHECK_EQ(FieldOffset(it, &it->_arg10),    uintptr_t(0x30));
    CHECK_EQ(FieldOffset(it, &it->_pad34),    uintptr_t(0x34));

    RawOf<Graphics::RendererDrawable> rawD;
    const Graphics::RendererDrawable* d = rawD.ptr();
    CHECK_EQ(FieldOffset(d, &d->_object), uintptr_t(0x00));
    CHECK_EQ(FieldOffset(d, &d->_value),  uintptr_t(0x08));
    CHECK_EQ(FieldOffset(d, &d->_pad0c),  uintptr_t(0x0c));
}

UNIT_TEST(graphics_scene_field_offsets) {
    RawOf<Graphics::Scene> raw;
    const Graphics::Scene* s = raw.ptr();

    CHECK_EQ(FieldOffset(s, &s->_cellSize),     uintptr_t(0x00));
    CHECK_EQ(FieldOffset(s, &s->_halfCellX),    uintptr_t(0x04));
    CHECK_EQ(FieldOffset(s, &s->_halfCellY),    uintptr_t(0x08));
    CHECK_EQ(FieldOffset(s, &s->_levelCount),   uintptr_t(0x0c));
    CHECK_EQ(FieldOffset(s, &s->_nodes),        uintptr_t(0x10));
    CHECK_EQ(FieldOffset(s, &s->_freeIds),      uintptr_t(0x20));
    CHECK_EQ(FieldOffset(s, &s->_cellHeads),    uintptr_t(0x30));
    CHECK_EQ(FieldOffset(s, &s->_levelOffsets), uintptr_t(0x40));
    CHECK_EQ(FieldOffset(s, &s->_cellCounts),   uintptr_t(0x50));
    CHECK_EQ(FieldOffset(s, &s->_globals),      uintptr_t(0x60));
    CHECK(std::is_standard_layout_v<Graphics::Scene>);

    // `Scene::Node`, reachable under retail's own nested spelling.
    CHECK(sizeof(Graphics::Scene::Node) == sizeof(Graphics::SceneNode));
    RawOf<Graphics::SceneNode> rawNode;
    const Graphics::SceneNode* n = rawNode.ptr();
    CHECK_EQ(FieldOffset(n, &n->_object), uintptr_t(0x00));
    CHECK_EQ(FieldOffset(n, &n->_next),   uintptr_t(0x08));
    CHECK_EQ(FieldOffset(n, &n->_prev),   uintptr_t(0x0c));
    CHECK_EQ(FieldOffset(n, &n->_cell),   uintptr_t(0x10));
    CHECK_EQ(FieldOffset(n, &n->_pad14),  uintptr_t(0x14));   // retail stores garbage

    // The grid's two big arrays are both 0x5555 entries, from the ctor's
    // `while (lVar1 < 0x15554)` byte bound.
    CHECK_EQ(RtGraphics::kSceneCellCount, 0x5555);
}

// ===========================================================================
// PART B — TRIVIAL COPYABILITY, RULE B5-4, IN BOTH DIRECTIONS
// ===========================================================================

UNIT_TEST(graphics_camera_stays_trivially_copyable) {
    // The half that must be TRUE. `Camera` is a by-value member of two array
    // element types and retail moves it as a plain 0xd8 blob, so the class must
    // carry no copy, assign or destroy step of its own. Its defaulted copy is
    // also what makes the three-byte deviation Camera.h records the ONLY
    // divergence from retail's partial copy bodies.
    CHECK(std::is_trivially_copyable_v<Graphics::Camera>);
    CHECK(std::is_trivially_destructible_v<Graphics::Camera>);
    CHECK(std::is_trivially_copy_constructible_v<Graphics::Camera>);
    CHECK(std::is_trivially_copy_assignable_v<Graphics::Camera>);

    // The two plain PODs among the element types: `SpecFor<T>` derives
    // `byteCopy = true` for both, which is what `Renderer::AddLight`'s
    // `_drawables.SetSize(n, rec)` and the draw-item lists rely on.
    CHECK(std::is_trivially_copyable_v<Graphics::RendererDrawable>);
    CHECK(std::is_trivially_copyable_v<Graphics::RendererDrawItem>);

    // `Camera`'s default constructor is `protected` (B4b-3) so only the two
    // holders can make one, and `??0Camera@Graphics@@QEAA@XZ` stays a door.
    CHECK(!std::is_default_constructible_v<Graphics::Camera>);
    // ...and the holders CAN, which is what lets them hold it by value.
    CHECK(std::is_default_constructible_v<Graphics::RendererDrawGroup>);
    CHECK(std::is_default_constructible_v<Graphics::RendererSceneEntry>);
}

UNIT_TEST(graphics_renderer_element_types_keep_their_destructors) {
    // The half that must be FALSE, and it was never true — as PODs or as
    // classes. Each element type embeds a `Collection::Array<T>`, whose
    // destructor is user-declared and whose `operator=` is retail's DEEP
    // `ArrayAssignCore`. `rtcoll::SpecFor<T>` therefore derives
    // `byteCopy = false` and a real `destroy` hook for both — which is exactly
    // what renderer.cpp's `~Renderer` comment ("RendererDrawGroup's own
    // destructor already destroys items[2], items[1], items[0] in that order")
    // and wave9_lane_gfxd.cpp's `kSpec_SceneEntry` stand on. Flip either and the
    // array resize stops freeing the inner buffers.
    CHECK(!std::is_trivially_destructible_v<Graphics::RendererDrawGroup>);
    CHECK(!std::is_trivially_destructible_v<Graphics::RendererSceneEntry>);
    CHECK(!std::is_trivially_destructible_v<Graphics::Renderer>);
    CHECK(!std::is_trivially_destructible_v<Graphics::Scene>);

    // Array-of-class members are destroyed in DESCENDING index order, which is
    // the "walks backward" iteration retail's 0x18001c8c0 performs. Measured
    // rather than asserted, on a stand-in with the same shape.
    struct Order {
        static int& Seq() { static int n = 0; return n; }
        static int (&Log())[3] { static int log[3] = {-1, -1, -1}; return log; }
        int index = 0;
        ~Order() { Log()[Seq()++] = index; }
    };
    Order::Seq() = 0;
    {
        Order items[3];
        items[0].index = 0; items[1].index = 1; items[2].index = 2;
    }
    CHECK_EQ(Order::Log()[0], 2);
    CHECK_EQ(Order::Log()[1], 1);
    CHECK_EQ(Order::Log()[2], 0);
}

// ===========================================================================
// PART C — THE HIDDEN RETURN SLOT ON A NON-VIRTUAL BY-VALUE RETURN
// ===========================================================================

namespace {

// A stand-in for `Graphics::Camera`'s four by-value getters and for
// `SunLight::GetSunCamera`: a NON-VIRTUAL member returning a value type larger
// than any register class, so GCC must use a hidden return slot for it too.
struct Big {
    uint32_t w[8];
};

struct Holder {
    Big value;
    // NON-VIRTUAL, exactly like `Camera::GetFrustum` / `GetProjection` /
    // `GetCameraToWorld` and `SunLight::GetSunCamera`.
    Big Get() const { return value; }
};

// The DOOR shape the tree uses for all of them: `this` first, the caller's
// storage second, and the storage returned again in RAX. `1a4200c3` was a `void`
// port of this shape, which at -O2 left RAX holding garbage.
Big* DoorGet(const void* self, Big* ret) {
    new (ret) Big(reinterpret_cast<const Holder*>(self)->Holder::Get());
    return ret;
}

}  // namespace

UNIT_TEST(graphics_nonvirtual_byvalue_return_is_safe_through_a_retail_shaped_door) {
    Holder h;
    for (int i = 0; i < 8; ++i) h.value.w[i] = 0xA0000000u + uint32_t(i);

    // Retail's caller shape: allocate the slot, pass (this, slot), read RAX.
    Big slot;
    for (uint32_t& x : slot.w) x = 0xDEADBEEFu;
    Big* returned = DoorGet(&h, &slot);

    // RAX carried the slot back — the half `1a4200c3` lost.
    CHECK(returned == &slot);
    // ...and the door wrote the CALLER'S storage, not the object. This is the
    // other half of block E's finding: for a NON-VIRTUAL member the GCC-versus-
    // MSVC argument order is invisible, because both ends of the member call are
    // this same GCC and only the DOOR's own signature is an ABI contract.
    for (int i = 0; i < 8; ++i) CHECK_EQ(slot.w[i], 0xA0000000u + uint32_t(i));
    for (int i = 0; i < 8; ++i) CHECK_EQ(h.value.w[i], 0xA0000000u + uint32_t(i));
}

UNIT_TEST(graphics_camera_byvalue_getters_copy_exactly_what_retail_copies) {
    // WHAT THIS CAN AND CANNOT DO. The four members themselves are in
    // `renderer.cpp`, which stands on `RegisterRuntimeExport`, `ResolveExport` and
    // `RT_DATA_SLOT` and is NOT on the UNIT_TREE_SRCS list, so calling
    // `Graphics::Camera::GetFrustum()` here is an undefined reference at link.
    // Defining marker bodies for them in this file — block H1's arrangement for
    // its two polymorphic classes — would be a SECOND definition of four functions
    // that already have real ones, so it is not done either.
    //
    // What is testable, and what actually binds, is that each getter's SOURCE
    // member is a whole value type whose C++ copy moves EXACTLY the dwords retail
    // moves. Retail's four bodies are dword-for-dword copies out of +0x00, +0x64
    // and +0x94, so the claim is a claim about the three member TYPES:
    //
    //   GetFrustum            25 dwords out of +0x00  == sizeof(::Frustum)
    //   GetCameraToWorld ×2   12 dwords out of +0x64  == sizeof(Math::Transform3)
    //   GetProjection         16 dwords out of +0x94  == sizeof(Math::Matrix)
    //
    // `Math::Transform3` and `Math::Matrix` ARE real linkable classes here
    // (`runtime/src/math/` is on the list), so these copies are real ones.
    CHECK_EQ(sizeof(::Frustum) / 4, size_t(25));
    CHECK_EQ(sizeof(Math::Transform3) / 4, size_t(12));
    CHECK_EQ(sizeof(Math::Matrix) / 4, size_t(16));

    RawOf<Graphics::Camera> raw;
    Graphics::Camera* cam = raw.ptr();
    for (int p = 0; p < 6; ++p) {
        cam->_frustum.planes[p] = Math::Vector4();
        cam->_frustum.planes[p].x = float(p) + 0.5f;
    }
    cam->_frustum.isOrthographic = true;
    cam->_cameraToWorld = Math::Transform3::Identity();
    cam->_projection = Math::Matrix();
    cam->_projection.m[5] = 7.25f;
    cam->_zUp = 1;

    // The copy each member's `return _x;` performs, spelled here directly.
    ::Frustum fslot = cam->_frustum;
    for (int p = 0; p < 6; ++p) CHECK_EQ(fslot.planes[p].x, float(p) + 0.5f);
    CHECK(fslot.isOrthographic);

    Math::Matrix mslot = cam->_projection;
    CHECK_EQ(mslot.m[5], 7.25f);

    Math::Transform3 tslot = cam->_cameraToWorld;
    CHECK_EQ(tslot.axisX.x, 1.0f);
    CHECK_EQ(tslot.axisY.y, 1.0f);
    CHECK_EQ(tslot.axisZ.z, 1.0f);

    // And the field the two `GetCameraToWorld` overloads share is ONE field, which
    // is the whole content of Camera.h's finding that retail's `float` overload
    // (0x180017e10) is instruction for instruction the no-argument one
    // (0x180017e70): there is no second transform for a lerp factor to interpolate
    // from. If a later edit ever adds one, this offset moves and PART A fails.
    CHECK_EQ(offsetof(Graphics::Camera, _projection) - offsetof(Graphics::Camera, _cameraToWorld),
             sizeof(Math::Transform3));
}

// ===========================================================================
// PART D — THE QUALIFIED CALL ON A NON-POLYMORPHIC CLASS
// ===========================================================================

UNIT_TEST(graphics_renderer_family_has_no_vtable_to_dispatch_through) {
    // B34_BRIEFS §A.1 makes every door's delegation a QUALIFIED non-virtual call
    // because a live POLYMORPHIC instance in this build wears a MINTED table
    // whose slots resolve back to the doors themselves, so an unqualified virtual
    // call inside a door is infinite recursion. None of block F's classes is
    // polymorphic — B35_RULES §2.1: no `??_7Camera@Graphics@@6B@`, no
    // `??_7Renderer@`, no `??_7Scene@`, no `_typeInfo`, and every export
    // `QEAA`/`QEBA`/`AEBA` — so for THIS block the qualification cannot change
    // dispatch and no door can re-enter itself. Stated as a measurement:
    CHECK(!std::is_polymorphic_v<Graphics::Camera>);
    CHECK(!std::is_polymorphic_v<Graphics::Renderer>);
    CHECK(!std::is_polymorphic_v<Graphics::Scene>);
    CHECK(!std::is_polymorphic_v<Graphics::RendererDrawGroup>);
    CHECK(!std::is_polymorphic_v<Graphics::RendererSceneEntry>);

    // The consequence that matters to a caller batch: the FIRST eight bytes of
    // each of these objects are DATA, not a vptr. `Renderer+0x00` is
    // `_sceneGroups._data` (which `?GetSceneGroup@` adds the index to) and
    // `Scene+0x00` is `_cellSize`, a float — so a `(*(void***)p)[N]` call on one
    // of these receivers would dispatch through a heap pointer or through
    // 2048.0f. That is why block F converted every such site in its file set to a
    // direct member call and why none was left as a bridge.
    RawOf<Graphics::Renderer> rawR;
    const Graphics::Renderer* r = rawR.ptr();
    CHECK_EQ(FieldOffset(r, &r->_sceneGroups._data), uintptr_t(0x00));
    RawOf<Graphics::Scene> rawS;
    const Graphics::Scene* s = rawS.ptr();
    CHECK_EQ(FieldOffset(s, &s->_cellSize), uintptr_t(0x00));
}
