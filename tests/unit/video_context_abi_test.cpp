// ============================================================================
// tests/unit/video_context_abi_test.cpp — ENDGAME B.3.5 block D1.
//
// The ABI claims block D1 makes about `Video::Context`, `Video::Viewport` and
// `Video::ConstantBuffer`, and nothing else. Model:
// `tests/unit/video_data_abi_test.cpp` (block A) and `io_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// None of these three classes is polymorphic, so there is no slot order to
// check. What there IS, and what would be silent if it broke:
//
//   1. THE 0x2168-BYTE LAYOUT, unchanged by the conversion. Nine files outside
//      block D1 read `Video::Context`'s fields by name and four more read it by
//      RAW OFFSET (`dx11/src/contextdata.cpp`'s `Ctx::k*`, `runtime/src/
//      graphics/lights_vtable.cpp`'s `kCtxViewport`, `runtime/src/ui/
//      userinterface_draw.cpp`'s `C + 0x408`, and the three raw-offset regions
//      the ported bodies keep). The header asserts every offset at compile time;
//      this file asserts the ones the OTHER files hard-code, so a future field
//      reshuffle inside Context.h cannot pass its own asserts while breaking
//      them.
//   2. TRIVIAL COPYABILITY, which is what makes `memcpy`/`memset` over a
//      Context and a ConstantBuffer defined behaviour. `Context.h` and
//      `ConstantBuffer.h` assert it; asserting it again from a translation unit
//      that includes nothing else is what catches an accidental member with a
//      destructor being added to either class later (RULE B5-4).
//   3. THE SEVEN EMBEDDED ConstantBuffers ARE 0x48 APART AND ARE THE REAL
//      CLASS. That stride IS the evidence for `sizeof(ConstantBuffer)`, so the
//      two facts have to be checked against each other rather than each against
//      itself.
//   4. THE MEMBER-CALL ROUND TRIP. A door delegates with a QUALIFIED
//      non-virtual call on a `Context*` that was `reinterpret_cast` from
//      MemAlloc'd bytes; that only works if the class is standard-layout and
//      `this` is the first byte. Checked by driving a real `Viewport` member and
//      a real `Context` field write through a pointer built the same way the
//      doors build theirs.
//
// WHAT IS NOT HERE, AND WHY. `Video::Context`'s ported members are registered
// doors in `runtime/src/video/`, which stand on the port registry, the address
// registry and (through the backend forwarders) a live `ContextData` vtable, so
// they cannot link into a host test. `Video::Viewport::operator==` /
// `operator!=` CAN — they touch nothing but four ints — so those two are driven
// for real, out of `runtime/src/video/context.cpp`. That file is not on
// UNIT_TREE_SRCS (it pulls in the whole registry), so this file declares the two
// operators' shape and checks it against a local mirror instead; see PART D.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "unit.h"

#include "video/ConstantBuffer.h"
#include "video/Context.h"

namespace {

// ---------------------------------------------------------------------------
// PART A — the offsets OTHER files hard-code.
//
// Each row is a raw offset that some file outside runtime/types/video/ reaches
// `Video::Context` by, with that file named. Context.h asserts the same offsets
// from the inside; the point of repeating them here is that these are the ones
// whose breakage would be silent — a wrong field would be read, not a compile
// error.
// ---------------------------------------------------------------------------
UNIT_TEST(video_context_raw_offsets_other_files_use) {
    using Video::Context;

    // dx11/src/contextdata.cpp's `Ctx::` block, recovered from the DX11 side
    // before the Runtime layout existed.
    CHECK_EQ(static_cast<int>(offsetof(Context, _renderBuffer)),        0x0028);
    CHECK_EQ(static_cast<int>(offsetof(Context, _viewport)),            0x01d0);
    CHECK_EQ(static_cast<int>(offsetof(Context, _instanceCount)),       0x2110);
    CHECK_EQ(static_cast<int>(offsetof(Context, _defaultRenderBuffer)), 0x2138);
    CHECK_EQ(static_cast<int>(offsetof(Context, _trianglesSubmitted)),  0x2150);
    CHECK_EQ(static_cast<int>(offsetof(Context, _dynamicVertexCursor)), 0x20e0);
    CHECK_EQ(static_cast<int>(offsetof(Context, _dynamicVertexStride)), 0x20e8);
    CHECK_EQ(static_cast<int>(offsetof(Context, _contextData)),         0x2160);

    // runtime/src/graphics/lights_vtable.cpp: `kCtxViewport = 0x1d0`, and it
    // writes the four dwords plus dirty bit 0x200 by hand.
    CHECK_EQ(static_cast<int>(offsetof(Context, _viewport)), 0x1d0);
    CHECK_EQ(static_cast<int>(offsetof(Context, _dirty)),    0x010);

    // runtime/src/ui/userinterface_draw.cpp: `C + 0x408` .. `C + 0x414` is
    // `_color`, forced to opaque white one dword at a time.
    CHECK_EQ(static_cast<int>(offsetof(Context, _color)), 0x408);

    // runtime/src/engine/loops.cpp writes `_time` and `_unknown03b8` as four
    // floats each; runtime/src/save/saveload_draw.cpp and
    // runtime/src/graphics/wave27_lane_gfxbig.cpp read `_renderBuffer` and
    // `_viewport`.
    CHECK_EQ(static_cast<int>(offsetof(Context, _time)),         0x3c8);
    CHECK_EQ(static_cast<int>(offsetof(Context, _unknown03b8)),  0x3b8);

    // The three raw-offset regions the ported bodies keep, and the reason each
    // one is raw. `_videoProgram` is the first qword of the staging array;
    // `?Reset@` memsets +0x208 and +0x248 from inside it; `?SetLight@`
    // publishes into +0x248..+0x287 and reads/writes +0x1dd8..+0x1e37.
    CHECK_EQ(static_cast<int>(offsetof(Context, _pixelConstantStaging)), 0x200);
    CHECK_EQ(static_cast<int>(sizeof(Context::_pixelConstantStaging)),   0x098);
    CHECK_EQ(static_cast<int>(offsetof(Context, _lightStaging)),         0x1dd8);
    CHECK_EQ(static_cast<int>(sizeof(Context::_lightStaging)),           0x1a0);

    // And the two that the QWORD clear in `?BindResources@` covers together.
    CHECK_EQ(static_cast<int>(offsetof(Context, _transformCount)), 0x20);
    CHECK_EQ(static_cast<int>(offsetof(Context, _unknown0024)),    0x24);

    CHECK_EQ(static_cast<int>(sizeof(Context)), 0x2168);
}

// ---------------------------------------------------------------------------
// PART B — RULE B5-4's triviality, from a translation unit that includes
// nothing but the two headers.
// ---------------------------------------------------------------------------
UNIT_TEST(video_context_and_members_stay_trivially_copyable) {
    CHECK(std::is_trivially_copyable<Video::Context>::value);
    CHECK(std::is_trivially_copyable<Video::Viewport>::value);
    CHECK(std::is_trivially_copyable<Video::ConstantBuffer>::value);

    // Standard layout is what every `offsetof` above and every
    // `reinterpret_cast` onto MemAlloc'd bytes depends on.
    CHECK(std::is_standard_layout<Video::Context>::value);
    CHECK(std::is_standard_layout<Video::Viewport>::value);
    CHECK(std::is_standard_layout<Video::ConstantBuffer>::value);

    // `Viewport` must ALSO stay trivially default-constructible: `Context` holds
    // two by value and has no real constructor, and retail's own
    // `??0Viewport@Video@@QEAA@XZ` zeroes — so neither a user-provided body nor
    // `= default` is available and the class must remain an aggregate.
    CHECK(std::is_trivially_default_constructible<Video::Viewport>::value);

    // `Context` is NOT trivially default-constructible, and that is correct
    // rather than a slip: its `Math::Matrix` and `Math::Vector4` members carry
    // retail's zeroing default constructors. Nothing default-constructs a
    // Context — `??0Context@Video@@` takes a `Device*` and is a registered door.
    CHECK(!(std::is_trivially_default_constructible<Video::Context>::value));

    // No vtable, on any of the three (`?GetFrameIndex@` and friends are
    // non-virtual; the ctor's first store is `_device`, not a vptr).
    CHECK(!(std::is_polymorphic<Video::Context>::value));
    CHECK(!(std::is_polymorphic<Video::Viewport>::value));
    CHECK(!(std::is_polymorphic<Video::ConstantBuffer>::value));
}

// ---------------------------------------------------------------------------
// PART C — the seven embedded ConstantBuffers, against the stride that IS the
// evidence for their size.
// ---------------------------------------------------------------------------
UNIT_TEST(video_constantbuffer_stride_inside_context) {
    using Video::Context;

    CHECK_EQ(static_cast<int>(sizeof(Video::ConstantBuffer)), 0x48);

    // `?AssignConstants@Context@Video@@` (0x1800ef3d0) walks exactly these seven
    // offsets, and `?Get*ConstantBuffer@` returns each one by name.
    CHECK_EQ(static_cast<int>(offsetof(Context, _materialConstants)),  0x0298);
    CHECK_EQ(static_cast<int>(offsetof(Context, _pixelConstants)),     0x02e0);
    CHECK_EQ(static_cast<int>(offsetof(Context, _globalConstants)),    0x1f78);
    CHECK_EQ(static_cast<int>(offsetof(Context, _instanceConstants)),  0x1fc0);
    CHECK_EQ(static_cast<int>(offsetof(Context, _boneConstants)),      0x2008);
    CHECK_EQ(static_cast<int>(offsetof(Context, _transformConstants)), 0x2050);
    CHECK_EQ(static_cast<int>(offsetof(Context, _lightConstants)),     0x2098);

    // The pair and the five-run, each spaced by exactly one object.
    CHECK_EQ(static_cast<int>(offsetof(Context, _pixelConstants) -
                               offsetof(Context, _materialConstants)),
              static_cast<int>(sizeof(Video::ConstantBuffer)));
    const size_t chain[5] = {
        offsetof(Context, _globalConstants),  offsetof(Context, _instanceConstants),
        offsetof(Context, _boneConstants),    offsetof(Context, _transformConstants),
        offsetof(Context, _lightConstants),
    };
    for (int i = 1; i < 5; ++i)
        CHECK_EQ(static_cast<int>(chain[i] - chain[i - 1]),
                  static_cast<int>(sizeof(Video::ConstantBuffer)));

    // The interior block D1 closed out of retail's one-line accessors. These are
    // the offsets `dx11/src/buffers.cpp`'s `ConstantBufferOwner` — the fourth
    // view of this class, and the only one that named them — reads from the
    // other side of the seam.
    using Video::ConstantBuffer;
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _prefix)),        0x00);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _typeName)),      0x08);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _ordinalIndex)),  0x10);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _uploadIndex)),   0x14);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _bindToVertex)),  0x18);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _banked)),        0x19);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _registerCount)), 0x1c);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _bankStride)),    0x20);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _constantData)),  0x28);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _constantCount)), 0x30);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _constantCapacity)), 0x34);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _device)),        0x38);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer, _platformData)),  0x40);

    // `ConstantInfo`'s 0x18 stride, which `?GetConstant@` indexes by.
    CHECK_EQ(static_cast<int>(sizeof(ConstantBuffer::ConstantInfo)), 0x18);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer::ConstantInfo, size)),   0x10);
    CHECK_EQ(static_cast<int>(offsetof(ConstantBuffer::ConstantInfo, offset)), 0x14);
}

// ---------------------------------------------------------------------------
// PART D — `Video::Viewport`'s two comparison operators, and the door's
// `reinterpret_cast`-onto-raw-bytes round trip.
//
// The real operators live in `runtime/src/video/context.cpp`, which cannot link
// here (port registry + address registry). What is under test is the SHAPE the
// doors rely on, so the mirror below is declared exactly as the real class is
// and the assertions run against both: the mirror's behaviour, and the real
// class's layout and triviality.
// ---------------------------------------------------------------------------
struct ViewportMirror {
    int32_t x, y, width, height;

    // The bodies read out of 0x180018240 and 0x180018270 this pass: TWO separate
    // four-compare bodies, not one plus a negation.
    bool operator==(const ViewportMirror& o) const {
        if (x != o.x) return false;
        if (y != o.y) return false;
        if (width != o.width) return false;
        if (height != o.height) return false;
        return true;
    }
    bool operator!=(const ViewportMirror& o) const {
        if (x != o.x) return true;
        if (y != o.y) return true;
        if (width != o.width) return true;
        if (height != o.height) return true;
        return false;
    }
};

UNIT_TEST(video_viewport_shape_and_comparison) {
    // Same layout as the real class, field for field.
    CHECK_EQ(static_cast<int>(sizeof(ViewportMirror)), static_cast<int>(sizeof(Video::Viewport)));
    CHECK_EQ(static_cast<int>(offsetof(Video::Viewport, x)),      0x0);
    CHECK_EQ(static_cast<int>(offsetof(Video::Viewport, y)),      0x4);
    CHECK_EQ(static_cast<int>(offsetof(Video::Viewport, width)),  0x8);
    CHECK_EQ(static_cast<int>(offsetof(Video::Viewport, height)), 0xc);

    // The four-argument constructor (0x180018220) stores x/y/width/height from
    // EDX/R8D/R9D/[RSP+0x28] — i.e. the third argument is the WIDTH. A reader
    // who assumed (x, y, right, bottom) would put the width where the height is,
    // so the aggregate order is asserted through a brace-init.
    const Video::Viewport vp{1, 2, 640, 480};
    CHECK_EQ(vp.x, 1);
    CHECK_EQ(vp.y, 2);
    CHECK_EQ(vp.width, 640);
    CHECK_EQ(vp.height, 480);

    const ViewportMirror a{1, 2, 640, 480};
    const ViewportMirror b{1, 2, 640, 480};
    ViewportMirror c{1, 2, 640, 481};
    CHECK(a == b);
    CHECK(!(a != b));
    CHECK(!(a == c));
    CHECK(a != c);
    // Each field on its own, because a body that compares only three of four
    // passes every all-equal case.
    c = a; c.x += 1;      CHECK(a != c);
    c = a; c.y += 1;      CHECK(a != c);
    c = a; c.width += 1;  CHECK(a != c);
    c = a; c.height += 1; CHECK(a != c);
}

// ---------------------------------------------------------------------------
// PART E — the door's cast. Every registered door in block D1 does exactly
// this: take bytes that came from `System::MemoryAllocator`, call
// `reinterpret_cast<Video::Context*>` on them, and make a QUALIFIED non-virtual
// member call. That is only well defined because the class is standard-layout
// with `this` at the first byte and no vptr; if a vtable pointer were ever added
// every field would move by 8 and every one of the raw offsets in PART A would
// read the wrong thing.
// ---------------------------------------------------------------------------
UNIT_TEST(video_context_reinterpret_cast_from_raw_bytes) {
    alignas(8) static uint8_t storage[sizeof(Video::Context)];
    std::memset(storage, 0, sizeof(storage));

    Video::Context* ctx = reinterpret_cast<Video::Context*>(storage);

    // `this` is the first byte: no vptr, no base subobject.
    CHECK(static_cast<const void*>(ctx) == static_cast<const void*>(storage));

    // A field write lands where the raw-offset readers expect it.
    ctx->_dirty = 0x1fe00u;
    CHECK_EQ(static_cast<int>(*reinterpret_cast<const uint32_t*>(storage + 0x10)),
              static_cast<int>(0x1fe00u));

    ctx->_viewport.width  = 1280;
    ctx->_viewport.height = 720;
    CHECK_EQ(static_cast<int>(*reinterpret_cast<const int32_t*>(storage + 0x1d8)), 1280);
    CHECK_EQ(static_cast<int>(*reinterpret_cast<const int32_t*>(storage + 0x1dc)), 720);

    // An embedded ConstantBuffer's field lands at the offset dx11 reads.
    ctx->_globalConstants._uploadIndex = 7;
    CHECK_EQ(static_cast<int>(*reinterpret_cast<const int32_t*>(storage + 0x1f78 + 0x14)), 7);

    // And the whole object copies as bytes, which is what `is_trivially_copyable`
    // buys and what context_bind.cpp's probe relies on.
    alignas(8) static uint8_t copy[sizeof(Video::Context)];
    std::memcpy(copy, storage, sizeof(storage));
    CHECK_EQ(std::memcmp(copy, storage, sizeof(storage)), 0);
}

}  // namespace
