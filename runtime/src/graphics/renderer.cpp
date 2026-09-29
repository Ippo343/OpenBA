// ============================================================================
// runtime/src/graphics/renderer.cpp — Wave 6 lane F
//   Graphics::Camera + Graphics::Renderer + Graphics::SunLight + Graphics::Material*
// ============================================================================
//
// Twelve of the twenty exports this lane owns (docs/runtime_port/wave6_lanes.txt,
// `=== LANE F ===`). Layouts and the evidence for every offset are in
// runtime/types/graphics/Camera.h and runtime/types/graphics/Material.h; the
// per-function write-up, the oracle arguments, the eight hold-backs and the
// MaterialInstance vtable-stamping argument are in
// docs/runtime_port/WAVE6_LANE_F.md.
//
//   | Export                              | Retail      | Ord  | Oracle        |
//   |-------------------------------------|-------------|------|---------------|
//   | ??0Camera@Graphics@@...             | 0x1800fe960 |   72 | FULL (scratch)|
//   | ?SetFrustum@Camera@Graphics@@...    | 0x1800fe5c0 | 3885 | FULL (scratch)|
//   | ?SetCameraToWorld@Camera@Graphics...| 0x1800feb00 | 3838 | FULL (scratch)|
//   | ?SnapCameraToWorld@Camera@Graphics..| 0x1800fe560 | 4075 | FULL (scratch)|
//   | ?GetFrustum@Camera@Graphics@@...    | 0x1800fe3e0 | 2680 | FULL (retslot)|
//   | ??0Renderer@Graphics@@QEAA@XZ       | 0x180105180 |  354 | FULL (scratch)|
//   | ??1Renderer@Graphics@@QEAA@XZ       | 0x1800180a0 |  663 | post-state    |
//   | ?GetCullCamera@Renderer@Graphics@@..| 0x180017f90 | 2521 | FULL          |
//   | ?GetShader@Renderer@Graphics@@QEBAHXZ| 0x180018010| 2897 | FULL          |
//   | ??0MaterialInstance@Graphics@@QEAA@XZ| 0x180103980|  267 | FULL (scratch)|
//   | ?SetMaterial@MaterialInstance@...   | 0x180103340 | 3906 | FULL (scratch)|
//   | ?AddTexture@MaterialInstance@...    | 0x180103350 | 1590 | post-state    |
//
// All twenty lane-F mangled names — the eight held back included — were checked
// against the export directory of the shipped C:\GOG Games\Banished\Runtime-x64.dll
// with pefile BEFORE any body was written, and all twenty are present. The
// addresses and ordinals above are that query's output. This step is not
// optional: a name the DLL does not contain resolves to NULL in silence, the
// port still registers, and the census still counts it
// (reference_resolveexport_phantom_names.md).
//
// EVERY PORT IS REGISTERED TWICE — under its mangled name AND under its retail
// address — because this lane's call sites already use both doors:
// engine/funcs/engine_funcs.cpp:2856 resolves `Camera::SetCameraToWorld` by
// ADDRESS (`ResolveAddr(Module::Runtime, 0x1800feb00)`) while every other lane-F
// consumer resolves by name. A name-only registration would have left that one
// call site pointed at the DLL while its siblings came here — two
// implementations taking turns on one live Camera, which is the exact shape
// Wave 5 lane D had to reason about for Picking::Scene.
//
// WHAT STAYS IN THE DLL, AND WHY
// ------------------------------
// EIGHT of the lane's twenty rows are NOT ported. In short (full reasons in the
// lane doc): `Renderer::Draw` is the entire frame and reaches into
// `Video::Context`'s private fields and `Geometry`'s virtuals, both owned by
// other lanes running concurrently; `Renderer::AddDrawItem` and
// `MaterialInstance::Setup`/`BuildParameters` mutate or read layouts this lane
// did not pin and admit no safe oracle; `SunLight`'s ctor and `Set` create and
// destroy GPU render targets; and the two `?_typeInfo@...` rows are exported
// DATA symbols, not functions — there is nothing to register before cutover.
//
// IS THE CLASS SPLIT?  Camera: no — every Camera method that exists is here
// (the class exports exactly these five symbols). Renderer and MaterialInstance:
// YES, deliberately, and it is safe for the same reason PLAN §4.1 decision 5
// gives — every ported body is byte-faithful on a layout both implementations
// share, and no ported body allocates, frees or stamps anything the unported
// half does not also produce. A Renderer built by this file is byte-identical to
// one built by the DLL, so the DLL's own `Draw` operates on it correctly, and
// vice versa. Nothing here changes an allocation size, a vtable, or a field the
// other half reads.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <new>          // the retslot doors placement-new the member's prvalue

#include "../../types/graphics/Camera.h"
#include "../../types/graphics/Material.h"
#include "../../types/video/Device.h"   // ENDGAME B.3.9 close: Video::Device::_singleton
#include "../../types/video/CompanionData.h"   // B.4: Video::DeviceData::Slot7
#include "../collection/array_internal.h"
#include "../../types/collection/Collection.h"   // rtcoll:: the array instantiations
#include "../../types/volume/Volume.h"           // RtVolume_Frustum_Projection
#include "../../types/system/MemoryAllocator.h"

using RtGraphics::Camera;
using RtGraphics::MaterialInstance;
using RtGraphics::MaterialTexture;
using RtGraphics::Renderer;
using RtGraphics::RendererDrawGroup;

namespace {

// ============================================================================
// The Runtime bodies the ported bodies call, by name (B3R).
// ============================================================================

// Volume::Frustum::Projection(bool) -> Math::Matrix BY VALUE.
// Retail 0x1800f9dc0, ordinal 3435. Hidden return slot in RDX after `this`.
// PORTED by Wave 9 lane P (runtime/src/volume/frustum.cpp), so this resolve now
// lands in our own code and this call site is the ONLY thing that exercises it.
// Wave 1 skipped it — "Volume ported only the 12 functions our tree calls, and
// no exe call site asks for Projection" — which was true and is what this
// comment said until 2026-09-07. B3R: RtVolume_Frustum_Projection by name.
Matrix* RtFrustumProjection(const Frustum* self, Matrix* ret, bool zeroToOneDepth) {
    return RtVolume_Frustum_Projection(self, ret, zeroToOneDepth);
}

// Math::Matrix::operator*(const Matrix&) -> Matrix BY VALUE.
// Retail 0x18001d980, ordinal 1318. PORTED IN WAVE 1
// (runtime/src/math/transform.cpp:977), so this resolve lands in our own code —
// which is what we want: it was proved bit-exact over millions of diffed calls,
// and routing through the registry keeps one implementation of the multiply in
// the process rather than two.
// ENDGAME B.3.1 step 2: `Math::Matrix::operator*`, called as the operator it
// is; the hidden return slot the shape carries is the caller's storage.
Matrix* RtMatrixMul(const Matrix* lhs, Matrix* ret, const Matrix* rhs) {
    *ret = *lhs * *rhs;
    return ret;
}

// System::MemoryAllocator::MemFree(void*) — the STATIC one-argument overload,
// which is what retail's inlined `System::String::~String` calls. Retail
// 0x1800c7ad0, ordinal 3228.
void RtMemFree(void* p) {
    System::MemoryAllocator::MemFree(p);
}

// ---- DLL internals, called at their retail addresses -----------------------
// These are `FUN_`-named COMDATs: one array helper per element type, emitted by
// MSVC per template instantiation, with no export and no name to resolve. Wave 5
// lane D established the pattern — port the exported bodies faithfully and call
// the unexported internals exactly where retail calls them.
//
// Note that all four of them allocate or free through
// System::MemoryAllocator under that allocator's own critical section, which is
// Win32 and therefore cannot appear in this file at all (PLAN §4.1 decision 1).
// That is a second, independent reason to leave them where they are.

// ENDGAME B.3.2 step 2 (callers) — the three forwarders that used to sit here
// (0x18001c9e0 on `drawables`, 0x18001c8c0 `DtorEhVec` on `drawGroups`, 0x18001c7e0
// `DtorElem` on `scenes`) are GONE. `Renderer::drawables/drawGroups/scenes` are
// already real `Collection::Array<T>` members (runtime/types/graphics/Camera.h),
// and all three rows are TRAIT-EQUIVALENT once the element type is the real
// struct, not a hand-derived spec:
//
//   0x18001c9e0  Array<RendererDrawable>::~Array — RendererDrawable is a plain
//                POD (Camera.h), trivially destructible: SpecFor<RendererDrawable>()
//                has destroy == nullptr, so this is the J1 body exactly.
//   0x18001c8c0  Array<RendererDrawGroup>::~Array — RendererDrawGroup carries
//                `Collection::Array<RendererDrawItem> items[3]` as a REAL member
//                (Camera.h), so RendererDrawGroup's own (implicit) destructor
//                already destroys items[2], items[1], items[0] in that order —
//                array-of-class-type members are destroyed in DESCENDING index
//                order, which is exactly the "walks backward" eh_vector
//                destructor iteration closure_batch2b.cpp's copy-ctor comment
//                contrasts itself against. SpecFor<RendererDrawGroup>() therefore
//                calls that same destructor per outer element — trait-derived,
//                not hand-derived.
//   0x18001c7e0  Array<RendererSceneEntry>::~Array — RendererSceneEntry carries
//                `Collection::Array<int32_t> drawableIds` as a real member, same
//                argument: its own destructor already frees that inner array.
//
// The three registered rows stay in array.cpp / array_destroy.cpp: `code/` still
// reaches them by address.

// WAVE 9 LANE Q: 0x180061480 is registered (kSpec_StringExt), one of the three
// NON-TRIVIAL element types in that lane -- the `INC dword ptr [*element - 4]`
// refcount bump described below is reproduced literally in the port, and the old
// buffer's element release still goes to retail's 0x180005930 by address.
// Disarm on its own with RT_ARRAY_NONTRIVIAL=0.
//
// Array<{String, ExternalPtr}>::SetSize(n, fillValue). Retail 0x180061480: grows
// (capacity*3 on a by-one append from non-empty), copy-CONSTRUCTS the tail from
// `fillValue` — bumping the String refcount at `*element - 4` — releases the old
// buffer's elements, and frees it. The 0x10 element stride in its loop is what
// pins sizeof(MaterialTexture).
//
// ENDGAME B.3.2 step 2 (callers) — the ResolveAddr forwarder that used to sit
// here is GONE, but this row is NOT trait-equivalent (kSpec_StringExt is one of
// the twelve hand-derived-only rows Collection.h's Array banner lists: a
// byteCopy assign that ALSO carries a destroy, which C++ traits cannot express).
// `MaterialInstance::textures` in runtime/types/graphics/Material.h is declared
// `Collection::Array<MaterialTexture>` with the DEFAULT (trait-derived) spec —
// that field's own SetSize/SetSizeFill would therefore run the WRONG body (a
// bitwise copy with no refcount bump). Retyping the field to
// `ExplicitSpec<&kSpec_StringExt>` is the correct long-term fix but touches a
// shared runtime/types header this batch does not own; bridging at the call
// site instead, exactly as B32_RULES prescribes for a hand-derived row.
// B.4: takes the texture array itself (the one caller hands `&_textures`).
void RtArraySetSizeTexturePairs(Collection::Array<MaterialTexture>* textures, uint32_t newSize,
                                const MaterialTexture* fill) {
    rtcoll::AsArray<&rtcoll::kSpec_StringExt>(textures)
        ->SetSizeFill(static_cast<int>(newSize), fill);
}

// ============================================================================
// Video::Device::_singleton -> +0x100 -> vtable slot 7.
//
// SetFrustum asks the video backend one yes/no question and passes the answer
// straight to Frustum::Projection: whether clip-space depth runs 0..1 (D3D) or
// -1..1 (GL). `Device::_singleton` is the exported DATA symbol
// ?_singleton@Device@Video@@0PEAV12@EA at 0x1801fe7a8; +0x100 is the active
// GraphicsInterface's backend object, and 0x38/8 == slot 7 of its vtable.
//
// In THIS process the answer is a compile-time constant and we know it: the
// backend is our own dx11/, whose DeviceData slot 7 is the shipped
// `mov al,1; ret` (dx11/src/devicedata_stubs.cpp:11) — always true. The port
// still makes the indirect call rather than folding the constant, because
// folding it would bake the DX11 backend into a Runtime file that PLAN §4.1
// decision 1 wants portable to a second backend.
bool RtVideoDeviceUsesZeroToOneDepth() {
    // ENDGAME B.3.9 close (D7): the class static, read fresh (no cached door).
    // B.4: `Device::deviceData` (+0x100) and `DeviceData::Slot7`, by name. LEFT:
    // the field is `void*` in Device.h, viewed once as the companion it is.
    return static_cast<::Video::DeviceData*>(::Video::Device::_singleton->deviceData)->Slot7();
}

// ============================================================================
// The basis swap SetFrustum post-multiplies the projection by when Camera+0xd4
// is set. Built inline on the stack at 0x1800fe5f0..0x1800fe6a3 out of three
// 1.0f stores and one -1.0f loaded from DAT_18016175c (verified by reading the
// bytes out of the shipped DLL: 00 00 80 bf). Math::Matrix is row-major with the
// translation in m[12..14] (runtime/types/math/Math.h), so as a row-vector
// transform this maps (x, y, z, w) -> (x, z, -y, w): a Z-up world into Y-up clip
// space.
// ============================================================================
const Matrix kZUpToYUp = {{
    1.0f, 0.0f,  0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, 0.0f,
    0.0f, 1.0f,  0.0f, 0.0f,
    0.0f, 0.0f,  0.0f, 1.0f,
}};

}  // namespace

// ============================================================================
// Graphics::Camera — THE DOORS. Every body below MOVED onto a member of the real
// `Graphics::Camera` (runtime/types/graphics/Camera.h); see the block at the
// bottom of this file. Each door keeps its name, its registrations and retail's
// ABI, and delegates with a QUALIFIED non-virtual call.
// ============================================================================

// ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z — retail 0x1800fe5c0
extern "C" void RtGraphics_Camera_SetFrustum(Camera* self, const Frustum* frustum) {
    reinterpret_cast<::Graphics::Camera*>(self)                       // B.3.5 bridge
        ->::Graphics::Camera::SetFrustum(*frustum);
}

// ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z
// — retail 0x1800fe960. Returns `this` in RAX like every MSVC constructor.
//
// THIS STAYS A CONSTRUCTOR DOOR, not a real C++ constructor (B4b-2, and
// B35_RULES §11 names `fn_Graphics_Camera_Ctor` in its leave list). Its four
// call sites in `code/Game` and `engine/funcs` keep working unchanged.
//
// Note the ordering retail chose and this reproduces: the projection matrix is
// zeroed and the flag stored BEFORE SetFrustum runs, so SetFrustum's branch on
// +0xd4 reads the value just written and its output overwrites the zeros.
extern "C" Camera* RtGraphics_Camera_Ctor(Camera* self, const Frustum* frustum,
                                          const Transform3* cameraToWorld, bool swapZUpToYUp) {
    // 25 dwords: the ctor DOES move +0x60 as a dword, so the frustum's pad
    // bytes come across too. (SetFrustum does not — see the member below.)
    std::memcpy(&self->frustum, frustum, sizeof(Frustum));
    std::memcpy(&self->cameraToWorld, cameraToWorld, sizeof(Transform3));
    // Was `memset(&self->projection, 0, sizeof(Matrix))`. ENDGAME B.3.1 made
    // Math::Matrix a real class whose default ctor (retail 0x18001d770) zeroes
    // all sixteen cells, so value-initialising it is the same sixteen stores
    // and does not trip -Wclass-memaccess.
    self->projection = Matrix();
    self->swapZUpToYUp = swapZUpToYUp ? 1 : 0;
    RtGraphics_Camera_SetFrustum(self, frustum);

    return self;
}

// ??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z — retail 0x180017c40.
//
// ALREADY PORTED, and NOT IN BLOCK F'S FILE SET: the body is
// `RtGraphics_Camera_Assign` in `runtime/src/graphics/wave28_lane_gfxbody.cpp`,
// which ported it because `BeginDrawGroup`'s reuse path is the tree's first
// caller of it. Block F drafted a second copy of it here and DELETED it on the
// link error, which is the duplicate-registration hazard
// reference_grep_for_the_port_before_writing_it.md describes — an address grep
// found only COMMENTS on 0x180017c40 because that file registers by NAME only
// (its banner says "NO RegisterRuntimeAddr IN THIS FILE").
//
// Its body must stay where it is for the reason Camera.h's three-byte-deviation
// banner gives: retail's `operator=` copies dwords +0x00..+0x5f, the BYTE at
// +0x60, dwords +0x64..+0xd0 and the BYTE at +0xd4, leaving +0x61..+0x63 AND
// +0xd5..+0xd7 untouched, and `Graphics::Camera::operator=` is `= default` — a
// full 0xd8 memberwise copy — precisely so the class stays trivially copyable. A
// defaulted assignment cannot express a partial copy, so the faithful body stays
// in that door and every assignment the live engine makes goes through it.
// wave28_lane_gfxbody.cpp reached the SAME conclusion about the ctor/assign
// asymmetry independently; block F's reading of the disassembly agrees with it
// instruction for instruction.

// ============================================================================
// Graphics::Renderer
// ============================================================================

// ??0Renderer@Graphics@@QEAA@XZ — retail 0x180105180. Returns `this`.
//
// FAITHFULNESS DETAIL: retail leaves +0x3c and +0x4c UNWRITTEN (see the header's
// note on Renderer). This port leaves them unwritten too. Zeroing them would be
// "tidier" and wrong: `Draw` assigns +0x3c from a scene entry before reading it,
// so retail's behaviour does not depend on the value — but our object would stop
// being byte-identical to the DLL's, and the scratch oracle below would report
// a divergence on whatever the two buffers happened to contain.
// THIS STAYS A CONSTRUCTOR DOOR (B4b-2; B35_RULES §11's leave list names
// `fn_Graphics_Renderer_Ctor`), and it operates on the `RtGraphics` POD on
// purpose: a real `Graphics::Renderer` constructor with a member-initialiser
// list is exactly what RULE B5-13 forbids, because the list would have to
// mention `_shaderDetail` (+0x3c) and `_pad_4c` (+0x4c) or leave a reader
// wondering why it does not.
extern "C" Renderer* RtGraphics_Renderer_Ctor(Renderer* self) {
    self->scenes._data        = nullptr;
    self->scenes._size        = 0;
    self->scenes._capacity    = 0;
    self->drawGroups._data    = nullptr;
    self->drawGroups._size    = 0;
    self->drawGroups._capacity = 0;
    self->drawables._data     = nullptr;
    self->drawables._size     = 0;
    self->drawables._capacity = 0;
    self->currentScene        = -1;
    self->currentDrawGroup    = -1;
    self->field_38            = 0;
    self->frameAlpha          = 0.0f;
    self->contextSerial       = -1;
    self->field_48            = 0;

    return self;
}

// ??1Renderer@Graphics@@QEAA@XZ — retail 0x1800180a0.
//
// ORACLE: POST-STATE ONLY, and this is the honest limit rather than a shortcut.
// The three callees free heap buffers. Running the original a second time on a
// scratch copy of the object would hand the allocator the SAME `_data` pointers
// our call just released — a double free, which is a defect the harness would be
// creating rather than detecting. So there is no "theirs" to compare against and
// the diff oracle carries what can be checked: that all three array triples come
// back fully zeroed, which is what each of the three DLL destructors ends with.
extern "C" void RtGraphics_Renderer_Dtor(Renderer* self) {

    // The retail order is drawables, then draw groups, then scenes. It matters:
    // the group and scene destructors walk their elements' own arrays, and doing
    // them in declaration order would not reproduce the allocator's free
    // sequence, which is observable through the boundary tags.
    self->drawables.~Array();    // retail 0x18001c9e0
    self->drawGroups.~Array();   // retail 0x18001c8c0
    self->scenes.~Array();       // retail 0x18001c7e0

    const bool cleared = self->scenes._data == nullptr && self->scenes._size == 0 &&
                         self->scenes._capacity == 0 &&
                         self->drawGroups._data == nullptr && self->drawGroups._size == 0 &&
                         self->drawGroups._capacity == 0 &&
                         self->drawables._data == nullptr && self->drawables._size == 0 &&
                         self->drawables._capacity == 0;
}

// ============================================================================
// Graphics::MaterialInstance
// ============================================================================

// ??0MaterialInstance@Graphics@@QEAA@XZ — retail 0x180103980. Returns `this`.
//
// VTABLE STAMPING — the per-class argument PLAN §2.2 D demands, made for THIS
// class rather than inherited from Thread's.
//
// Retail stamps three vtables in sequence — Object::Base, then Object::External,
// then MaterialInstance — which is just MSVC emitting each base's constructor
// inline, each re-seating the vptr as it goes. All three are exported DATA
// symbols, verified against the shipped DLL:
//     ??_7Base@Object@@6B@              0x180154788  ord 1389
//     ??_7External@Object@@6B@          0x1801547a0  ord 1432
//     ??_7MaterialInstance@Graphics@@6B@ 0x1801545e0  ord 1464
// so the port resolves each BY NAME and writes the DLL's own array. It does not
// build a vtable of its own and does not copy slots out of a live object.
//
// Why that is SAFE here, specifically:
//   1. Every stamp is the address of a table the DLL itself owns and would have
//      written. An object built by this ctor is byte-identical to one built by
//      the DLL's, so any virtual call — from the DLL's Renderer::Draw, from
//      MaterialInstance::RenderTextures, from Object::External's own machinery —
//      dispatches to exactly the same code it would have.
//   2. MaterialInstance is CONCRETE and, in Graphics, LEAF: slot 0 of
//      0x1801545e0 is a real scalar-deleting destructor, not `_purecall`, and no
//      other exported ??_7 in the DLL names a class derived from it. So there is
//      no derived ctor whose later stamp we could be racing, which is the
//      failure mode a half-ported class hits.
//   3. Our own two subclass-like users are unaffected because they stamp AFTER
//      construction, not during it. code/Game/Dialog/thumb_vtables.cpp:66-82
//      copies slots 1..8 out of whatever the ctor installed and plants its own
//      slot 0 so the exe controls the free; that mirroring reads the same table
//      whether this port or the DLL put it there. (Worth stating plainly: that
//      file is NOT a workaround for a missing vtable symbol — it wants a
//      DIFFERENT slot 0. PLAN §2.2 D lists MaterialInstance under "the mirroring
//      workaround was never necessary"; for thumb_vtables that reading is wrong,
//      and the correction is in WAVE6_LANE_F.md.)
//      code/Game/Component/statusicon_geometry.cpp:328 builds a Geometry vtable,
//      not a MaterialInstance one, and is not affected by this port at all.
//   4. The ctor allocates nothing and installs nothing global, so a
//      RT_GFXRENDER=0 run and a default run produce the same object.
//
// ENDGAME B.3.9 block GFX: the door that transcribed this is RETIRED. The three
// stamps are the compiler's chain of the real `Graphics::MaterialInstance()`
// (Material.h), whose body is the field stores; every builder -- the factory
// pair, Construct / Reconstruct, SaveLoad::Draw and the four exe dialogs --
// spells `::new (p) Graphics::MaterialInstance()`.

// ============================================================================
// Graphics::MaterialInstance::~MaterialInstance — 0x180103a00 (ENDGAME B.3.8
// block RB). Retail: restamp, then three retail `~Array` bodies in this order
//     0x180004d60 (+0x50 _textures), 0x180004c80 (+0x40 _constants),
//     0x180004b70 (+0x30 _parameters)
// then ~External. The restamp and ~External are the compiler's chain; the
// three calls are the body, reached BY ADDRESS (each resolves to its ported
// J-row, which zeroes the header, so the compiler's implicit member ~Arrays
// after the body are no-ops). ENDGAME B.3.9 GFX: the `ReleaseFields` split and
// the ??1 door in deleting_dtor_leaves.cpp are gone.
// ============================================================================
namespace Graphics {
MaterialInstance::~MaterialInstance() {
    rtcoll::RtColl_ArrayDtorElem_180004d60(&_textures);     // +0x50
    rtcoll::RtColl_ArrayDtorElem_180004c80(&_constants);    // +0x40
    rtcoll::RtColl_ArrayDtorElem_180004b70(&_parameters);   // +0x30
}
}  // namespace Graphics

// ############################################################################
// ENDGAME B.3 step 5 (block F) — THE BODIES ABOVE, MOVED ONTO THE MEMBERS
// ############################################################################
//
// `Graphics::Camera` and `Graphics::Renderer` are real C++ classes as of block F
// (`runtime/types/graphics/Camera.h`). Nineteen bodies move here: nine of
// `Camera`'s thirteen exports and ten of `Renderer`'s twenty-five, of which
// eleven `Renderer` accessors plus `Camera::GetCameraToWorld` ×2,
// `GetProjection`, `IsMirrored` and `SetZUp` are NEW PORTS read out of the
// retail DLL by block F. Every registered door above kept its name and its
// address and became a one-line delegation with a QUALIFIED non-virtual call.
//
// The qualification is not decoration even on a NON-POLYMORPHIC class: it is
// what B34_BRIEFS §A.1 requires uniformly, and here it also documents that the
// door is calling THE MEMBER and not re-entering itself through the registry.
//
// WHAT DID **NOT** MOVE, and why:
//
//  * `??0Camera(const Frustum&, const Transform3&, bool)` (0x1800fe960) and
//    `??0Renderer()` / `??1Renderer()` (0x180105180 / 0x1800180a0). ENDGAME §B.1
//    decision 2, B4b-2 and B35_RULES §11's leave list keep all three as DOORS:
//    `fn_Graphics_Camera_Ctor` and `fn_Graphics_Renderer_Ctor` are named there.
//    The Renderer constructor additionally MUST NOT become a member-initialiser
//    list, because RULE B5-13 requires +0x3c and +0x4c stay unwritten.
//  * `??4Camera@` (0x180017c40). Its retail body is a PARTIAL copy that leaves
//    +0x61..+0x63 and +0xd5..+0xd7 alone, and `Graphics::Camera::operator=` is
//    `= default` (Camera.h's three-byte-deviation banner) precisely so the class
//    stays trivially copyable. A defaulted assignment cannot express a partial
//    copy, so the faithful body stays in the door — which is where every copy
//    the live engine makes goes.
//  * The three `MaterialInstance` doors at the bottom of this file.
//    `Graphics::MaterialInstance` is BLOCK G's class, converted concurrently.
//    UNTOUCHED, and listed in block F's report.
//
// NAME-LOOKUP HYGIENE (B34B_RULES §7.2): inside `namespace Graphics` below, a
// bare `Camera` or `Renderer` binds the REAL CLASS, not the `RtGraphics` POD the
// `using` declarations at the top of this file introduce at global scope. The
// file-local helpers (`RtFrustumProjection`, `RtMatrixMul`,
// `RtVideoDeviceUsesZeroToOneDepth`, `kZUpToYUp`) are reached by unqualified
// lookup falling through to the global unnamed namespace, which is the
// arrangement B34_BRIEFS §A.2 records as the one that works.
//
// RULE B5-7: nothing below adds a static initialiser, a lazy singleton or a
// non-atomic first-call cache. `Camera` and `Renderer` are constructed on the
// `Engine::Loader` thread while the main thread is inside `Draw`. The one cache
// in this file, `RtVideoDeviceUsesZeroToOneDepth`'s `static void** singletonSlot`,
// predates block F, is a pointer to an exported DATA cell rather than a value,
// and is left exactly as it was.
// ############################################################################

namespace Graphics {

// ---------------------------------------------------------------------------
// ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z   retail 0x1800fe5c0
//
// Copies the frustum in, then rebuilds the projection matrix from it.
//
// FAITHFULNESS DETAIL THAT LOOKS LIKE A TYPO AND IS NOT: retail copies the
// frustum as 24 DWORDs (+0x00..+0x5f) plus a single BYTE at +0x60 — see the
// MOVZX/MOV byte pair at 0x1800fe6ec — so the three alignment pad bytes at
// +0x61..+0x63 of the DESTINATION are left as they were. The Camera constructor
// and `GetFrustum`, by contrast, move +0x60 as a full dword and so do copy the
// pad. A blanket `memcpy(..., sizeof(Frustum))` here would be a real deviation.
// ---------------------------------------------------------------------------
void Camera::SetFrustum(const ::Frustum& frustum) {
    // The `void*` cast is deliberate and is NOT a behaviour change: ENDGAME
    // B.3.1 gave Math::Vector4 retail's zeroing default ctor, which makes
    // `Frustum` non-trivially-default-constructible, and -Wclass-memaccess then
    // objects that this DELIBERATELY partial copy (0x60 of 0x64 bytes) leaves
    // four bytes unchanged. Copying through void* says "yes, on purpose".
    std::memcpy(static_cast<void*>(&_frustum), &frustum, 0x60);
    _frustum.isOrthographic = frustum.isOrthographic;

    const bool zeroToOne = RtVideoDeviceUsesZeroToOneDepth();
    if (_zUp) {
        Matrix projection;
        RtFrustumProjection(&_frustum, &projection, zeroToOne);
        RtMatrixMul(&kZUpToYUp, &_projection, &projection);
    } else {
        RtFrustumProjection(&_frustum, &_projection, zeroToOne);
    }
}

// ---------------------------------------------------------------------------
// ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z   0x1800feb00
// Twelve dwords into +0x64. `Transform3::operator=` is `= default`, i.e. the
// same 0x30-byte field copy retail's `memcpy` is.
// ---------------------------------------------------------------------------
void Camera::SetCameraToWorld(const Math::Transform3& cameraToWorld) {
    _cameraToWorld = cameraToWorld;
}

// ---------------------------------------------------------------------------
// ?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z  0x1800fe560
//
// FINDING, recorded because the name promises otherwise: this is
// INSTRUCTION-FOR-INSTRUCTION `SetCameraToWorld` above — twelve dwords into
// +0x64 and return. In a class with interpolated state a "Snap" would also
// overwrite the PREVIOUS transform so the next Lerp does not smear;
// `Graphics::Camera` has no previous-transform field for it to write, so the
// distinction the two names draw does not exist in the shipped binary. Kept as
// two members rather than one alias so each door checks against its OWN export
// and a future divergence (a different build, a modded DLL) would show up
// instead of being assumed away. `Camera::GetCameraToWorld`'s two overloads are
// the same finding a second time (Camera.h's banner).
// ---------------------------------------------------------------------------
void Camera::SnapCameraToWorld(const Math::Transform3& cameraToWorld) {
    _cameraToWorld = cameraToWorld;
}

// ---------------------------------------------------------------------------
// ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ   retail 0x1800fe3e0
// Twenty-five dwords out of +0x00 — the pad travels. NON-VIRTUAL, so the C++
// by-value return is right; the door carries retail's hidden slot.
// ---------------------------------------------------------------------------
::Frustum Camera::GetFrustum() const { return _frustum; }

// ---------------------------------------------------------------------------
// ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@XZ  0x180017e70
// ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@M@Z 0x180017e10
//
// Both bodies are twelve `MOV EAX,[RCX+0x64+4i] / MOV [RDX+4i],EAX` pairs and
// `MOV RAX,RDX`. THE FLOAT NEVER APPEARS IN THE SECOND BODY — see
// `SnapCameraToWorld`'s finding note above, and Camera.h's banner.
// ---------------------------------------------------------------------------
Math::Transform3 Camera::GetCameraToWorld() const { return _cameraToWorld; }

Math::Transform3 Camera::GetCameraToWorld(float t) const {
    (void)t;   // retail's 0x180017e10 ignores it — not an omission
    return _cameraToWorld;
}

// ---------------------------------------------------------------------------
// ?GetProjection@Camera@Graphics@@QEBA?AVMatrix@Math@@XZ   retail 0x180017ed0
// Sixteen dwords out of +0x94.
// ---------------------------------------------------------------------------
Math::Matrix Camera::GetProjection() const { return _projection; }

// ---------------------------------------------------------------------------
// ?SetZUp@Camera@Graphics@@QEAAX_N@Z   retail 0x1800fe550
// `MOV byte ptr [RCX+0xd4],DL; RET`.
// ---------------------------------------------------------------------------
void Camera::SetZUp(bool zUp) { _zUp = zUp ? 1 : 0; }

// ---------------------------------------------------------------------------
// ?IsMirrored@Camera@Graphics@@QEBA_NXZ   retail 0x1800fe480
//
// The SIGN of the camera-to-world basis determinant, computed as
// `dot(axisZ, axisX x axisY)` and answered as `0.0f > det` (`COMISS XMM0,XMM11`
// with XMM0 zeroed by `XORPS`, then `SETA AL`) — so a NaN determinant answers
// FALSE, which is retail's behaviour and is preserved by the ordinary spelling.
//
// TRANSCRIBED IN RETAIL'S ASSOCIATION, term by term, because a re-grouped float
// sum flips exactly at the sign change this function tests
// (reference_moved_term_hides_where_terms_cancel.md). Read off the SSE schedule
// at 0x1800fe484..0x1800fe53e: the three cross terms first, then
// `(axisZ.y*cy + axisZ.x*cx) + axisZ.z*cz` in that bracketing — `axisZ.y` FIRST,
// which is not the order a reader would write.
//
// Retail also spills XMM7..XMM11 over a 0x58-byte frame; that is register
// allocation, not object state, and is not reproduced.
// ---------------------------------------------------------------------------
bool Camera::IsMirrored() const {
    const Math::Vector3& ax = _cameraToWorld.axisX;
    const Math::Vector3& ay = _cameraToWorld.axisY;
    const Math::Vector3& az = _cameraToWorld.axisZ;

    const float cx = ax.z * ay.y - ax.y * ay.z;
    const float cy = ax.x * ay.z - ay.x * ax.z;
    const float cz = ay.x * ax.y - ax.x * ay.y;

    const float det = (az.y * cy + az.x * cx) + az.z * cz;
    return 0.0f > det;
}

// ---------------------------------------------------------------------------
// Graphics::Renderer — the thirteen accessors.
//
// The five indexed ones are pure address arithmetic with no bounds check, in
// retail and here. `_drawGroups[i]` and `_sceneGroups[i]` are
// `Collection::Array<T>::operator[]`, which is `_data[index]` — the same
// `IMUL / ADD [RCX+0x10]` pair retail emits, now named.
// ---------------------------------------------------------------------------
float Renderer::GetAlphaT() const { return _alphaT; }

const Camera& Renderer::GetCamera() const {
    return _drawGroups[_currentDrawGroup]._camera;              // group + 0x08
}
const Camera& Renderer::GetCamera(int group) const {
    return _drawGroups[group]._camera;
}
const Camera& Renderer::GetCullCamera() const {
    return _drawGroups[_currentDrawGroup]._cullCamera;          // group + 0xe0
}
const Camera& Renderer::GetCullCamera(int group) const {
    return _drawGroups[group]._cullCamera;
}

int      Renderer::GetCurrentSceneGroup() const { return _currentSceneGroup; }
int      Renderer::GetDetailId() const          { return _detailId; }
int      Renderer::GetFrameIndex() const        { return _frameIndex; }
int      Renderer::GetShaderDetail() const      { return _shaderDetail; }
void     Renderer::SetShaderDetail(int detail)  { _shaderDetail = detail; }

uint32_t Renderer::GetMaterialTypeFlags() const {
    return _drawGroups[_currentDrawGroup]._materialTypeFlags;   // group + 0x1bc
}
int Renderer::GetShader() const {
    return _drawGroups[_currentDrawGroup]._shader;              // group + 0x1b8
}
const RendererSceneEntry& Renderer::GetSceneGroup(int index) const {
    return _sceneGroups[index];                                 // _data + i*0x110
}

// ============================================================================
// ENDGAME B.3.5 block GC — the two `Graphics::MaterialInstance` bodies this file
// still owned. Block F left all three MaterialInstance doors alone because the
// class was block G's and was converting concurrently; block G then ran out of
// budget before reaching them.
//
// THE CTOR DOOR STAYS A DOOR and is NOT one of the two. `??0MaterialInstance`
// (0x180103980) stamps three MINTED vtables in sequence, which is right for every
// object that reaches it and is exactly what B4b-2 and B35_RULES section 11 keep
// as a door; a member constructor would emit GCC's table instead. Its banner
// above is the per-class vtable-stamping argument and is unchanged.
// ============================================================================

// ?SetMaterial@MaterialInstance@Graphics@@QEAAXV?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z
// retail 0x180103340 — ONE STORE, and no refcount traffic: the caller's handle
// and the member end up sharing one reference, which is why `BuildParameters`
// can read `_material` back without owning it. The door's banner has the rest.
void MaterialInstance::SetMaterial(Object::ExternalPtr<Material> material) {
    _material = material;
}

// ?AddTexture@MaterialInstance@Graphics@@QEAAXAEBVString@System@@V?$ExternalPtr@VImageBuffer@Video@@@Object@@@Z
// retail 0x180103350 — see the door's banner for the refcount accounting and for
// why the array helper is bridged rather than the field retyped.
//
// `_textures` is spelled through `RtArraySetSizeTexturePairs`, which takes the
// POD: the element spec here is `kSpec_StringExt`, one of the twelve
// hand-derived-only rows, so the field's own trait-derived `SetSizeFill` would
// run a bitwise copy with no refcount bump. That bridge is the reason this body
// cannot simply say `_textures.SetSize(…)`, and it is stated at the helper.
void MaterialInstance::AddTexture(const RtSystem::String& name,
                                  Object::ExternalPtr<Video::ImageBuffer> image) {
    MaterialTexture local;
    local.name = nullptr;
    char* data = name.data;                          // System::String is one pointer
    if (data) {
        ++*reinterpret_cast<int32_t*>(data - 4);
        local.name = data;
    }
    local.image = image.Get();

    RtArraySetSizeTexturePairs(&_textures,
                               static_cast<uint32_t>(_textures.GetSize() + 1),
                               &local);

    if (local.name) {
        int32_t* refCount = reinterpret_cast<int32_t*>(local.name - 4);
        if (--*refCount == 0) RtMemFree(local.name - 8);
    }
}

}  // namespace Graphics
