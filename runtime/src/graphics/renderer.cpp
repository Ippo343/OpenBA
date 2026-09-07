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

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Camera.h"
#include "../../types/graphics/Material.h"

using RtGraphics::Camera;
using RtGraphics::MaterialInstance;
using RtGraphics::MaterialTexture;
using RtGraphics::Renderer;
using RtGraphics::RendererDrawGroup;

// ----------------------------------------------------------------------------
// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — the same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally. resolve.h drags in <windows.h>, whose
// <wingdi.h> declares a FUNCTION called `Rectangle` (so `Volume::Rectangle`
// would stop naming a type) and whose <windef.h> still #defines the Win16 `near`
// and `far` keywords away (so a frustum's near/far plane distances would not
// parse). PLAN §4.1 decision 1 requires runtime/src/ to stay platform-neutral
// outside a `_win` twin, and this file has no Win32 in it.
//
// These declarations must stay byte-identical to resolve.h's; the linker checks
// them against each other, and standalone/resolve_standalone.cpp includes both.
// The precedent is runtime/src/picking/scene.cpp:88 and
// runtime/src/achievement/achievement_manager.cpp:71.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

namespace {

// ============================================================================
// Mangled names for the diff-oracle calls. The registrations at the bottom of
// this file deliberately repeat each name as its own literal rather than using
// these constants: tools/runtime_census.py matches `RegisterRuntimeExport("<lit>"`
// and cannot see through a NAMED CONSTANT, which is exactly how Wave 3's Object
// count stood at 21/41 when it was really 27/41
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
const char* const kCameraCtor =
    "??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z";
const char* const kSetFrustum        = "?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z";
const char* const kSetCameraToWorld  = "?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z";
const char* const kSnapCameraToWorld = "?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z";
const char* const kGetFrustum        = "?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ";
const char* const kRendererCtor      = "??0Renderer@Graphics@@QEAA@XZ";
const char* const kGetCullCamera     = "?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ";
const char* const kGetShader         = "?GetShader@Renderer@Graphics@@QEBAHXZ";
const char* const kMaterialInstCtor  = "??0MaterialInstance@Graphics@@QEAA@XZ";
const char* const kSetMaterial =
    "?SetMaterial@MaterialInstance@Graphics@@QEAAXV?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z";

// ============================================================================
// The DLL symbols the ported bodies call.
//
// Resolved through ResolveExport / ResolveAddr — which consult the runtime
// registry FIRST — and cached in a function-local static, never through
// RtOriginal outside a diff guard. RtOriginal is GetProcAddress: an export-table
// binary search by string taken under the loader lock on every call. Two of them
// sitting in the body of System::Time cost Wave 2 five points of sim time and
// were invisible to both profilers (reference_rtoriginal_is_getprocaddress.md).
// ============================================================================

// Volume::Frustum::Projection(bool) -> Math::Matrix BY VALUE.
// Retail 0x1800f9dc0, ordinal 3435. Hidden return slot in RDX after `this`.
// NOT ported by Wave 1 (Volume ported only the 12 functions our tree calls, and
// no exe call site asks for Projection), so today this always lands in the DLL.
Matrix* RtFrustumProjection(const Frustum* self, Matrix* ret, bool zeroToOneDepth) {
    using Fn = Matrix* (*)(const Frustum*, Matrix*, bool);
    static Fn fn = reinterpret_cast<Fn>(
        ResolveExport("?Projection@Frustum@Volume@@QEBA?AVMatrix@Math@@_N@Z"));
    return fn(self, ret, zeroToOneDepth);
}

// Math::Matrix::operator*(const Matrix&) -> Matrix BY VALUE.
// Retail 0x18001d980, ordinal 1318. PORTED IN WAVE 1
// (runtime/src/math/transform.cpp:977), so this resolve lands in our own code —
// which is what we want: it was proved bit-exact over millions of diffed calls,
// and routing through the registry keeps one implementation of the multiply in
// the process rather than two.
Matrix* RtMatrixMul(const Matrix* lhs, Matrix* ret, const Matrix* rhs) {
    using Fn = Matrix* (*)(const Matrix*, Matrix*, const Matrix*);
    static Fn fn = reinterpret_cast<Fn>(
        ResolveExport("??DMatrix@Math@@QEBA?AV01@AEBV01@@Z"));
    return fn(lhs, ret, rhs);
}

// System::MemoryAllocator::MemFree(void*) — the STATIC one-argument overload,
// which is what retail's inlined `System::String::~String` calls. Retail
// 0x1800c7ad0, ordinal 3228.
void RtMemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    fn(p);
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

// Array<T>::~Array for a trivially destructible T: Free(_data) under the
// allocator lock, then zero the {_data, _size, _capacity} triple. Retail
// 0x18001c9e0 — the Renderer dtor's first callee, on `_drawables`.
void RtArrayDtorTrivial(void* arrayTriple) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c9e0));
    fn(arrayTriple);
}

// Array<DrawGroup>::~Array. Retail 0x18001c8c0: for each of `_size` elements it
// runs an EH vector destructor over `element + 0x1c0`, count 3, stride 0x10 —
// the group's three per-pass item arrays — then frees the buffer and zeroes the
// triple. The stride 0x1f0 in its loop is what pins sizeof(DrawGroup).
void RtArrayDtorDrawGroups(void* arrayTriple) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c8c0));
    fn(arrayTriple);
}

// Array<SceneEntry>::~Array. Retail 0x18001c7e0: element-destroy helper over the
// whole buffer (0x18001ce90), then free, then zero the triple.
void RtArrayDtorSceneEntries(void* arrayTriple) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c7e0));
    fn(arrayTriple);
}

// Array<{String, ExternalPtr}>::SetSize(n, fillValue). Retail 0x180061480: grows
// (capacity*3 on a by-one append from non-empty), copy-CONSTRUCTS the tail from
// `fillValue` — bumping the String refcount at `*element - 4` — releases the old
// buffer's elements, and frees it. The 0x10 element stride in its loop is what
// pins sizeof(MaterialTexture).
void RtArraySetSizeTexturePairs(void* arrayTriple, uint32_t newSize, const MaterialTexture* fill) {
    using Fn = void (*)(void*, uint32_t, const MaterialTexture*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180061480));
    fn(arrayTriple, newSize, fill);
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
    static void** singletonSlot =
        reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fe7a8));
    void* device = *singletonSlot;
    void* iface = *reinterpret_cast<void**>(static_cast<char*>(device) + 0x100);
    void** vtable = *reinterpret_cast<void***>(iface);
    using Fn = bool (*)(void*);
    return reinterpret_cast<Fn>(vtable[7])(iface);
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
// Graphics::Camera
// ============================================================================

// ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z — retail 0x1800fe5c0
//
// Copies the frustum in, then rebuilds the projection matrix from it.
//
// FAITHFULNESS DETAIL THAT LOOKS LIKE A TYPO AND IS NOT: retail copies the
// frustum as 24 DWORDs (+0x00..+0x5f) plus a single BYTE at +0x60 — see the
// MOVZX/MOV byte pair at 0x1800fe6ec — so the three alignment pad bytes at
// +0x61..+0x63 of the DESTINATION are left as they were. The Camera ctor and
// GetFrustum, by contrast, move +0x60 as a full dword and so do copy the pad.
// A blanket `memcpy(..., sizeof(Frustum))` here would be a real deviation, and
// the scratch-copy oracle below would report it as a divergence in the pad
// bytes, which is precisely the kind of "harmless" difference that turns into a
// mystery three waves later.
extern "C" void RtGraphics_Camera_SetFrustum(Camera* self, const Frustum* frustum) {
    RT_DIFF_BEGIN("Graphics::Camera::SetFrustum");
    Camera before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(Camera));

    std::memcpy(&self->frustum, frustum, 0x60);
    self->frustum.isOrthographic = frustum->isOrthographic;

    const bool zeroToOne = RtVideoDeviceUsesZeroToOneDepth();
    if (self->swapZUpToYUp) {
        Matrix projection;
        RtFrustumProjection(&self->frustum, &projection, zeroToOne);
        RtMatrixMul(&kZUpToYUp, &self->projection, &projection);
    } else {
        RtFrustumProjection(&self->frustum, &self->projection, zeroToOne);
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Camera*, const Frustum*);
        RtOriginal<Fn>(kSetFrustum)(&before, frustum);
        RT_DIFF_BYTES(self, &before, sizeof(Camera));
    }
}

// ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z
// — retail 0x1800fe960. Returns `this` in RAX like every MSVC constructor.
//
// Note the ordering retail chose and this reproduces: the projection matrix is
// zeroed and the flag stored BEFORE SetFrustum runs, so SetFrustum's branch on
// +0xd4 reads the value just written and its output overwrites the zeros.
extern "C" Camera* RtGraphics_Camera_Ctor(Camera* self, const Frustum* frustum,
                                          const Transform3* cameraToWorld, bool swapZUpToYUp) {
    RT_DIFF_BEGIN("Graphics::Camera::Camera");
    Camera before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(Camera));

    // 25 dwords: the ctor DOES move +0x60 as a dword, so the frustum's pad
    // bytes come across too. (SetFrustum, above, does not.)
    std::memcpy(&self->frustum, frustum, sizeof(Frustum));
    std::memcpy(&self->cameraToWorld, cameraToWorld, sizeof(Transform3));
    std::memset(&self->projection, 0, sizeof(Matrix));
    self->swapZUpToYUp = swapZUpToYUp ? 1 : 0;
    RtGraphics_Camera_SetFrustum(self, frustum);

    if (rt_diff_on__) {
        using Fn = Camera* (*)(Camera*, const Frustum*, const Transform3*, bool);
        RtOriginal<Fn>(kCameraCtor)(&before, frustum, cameraToWorld, swapZUpToYUp);
        RT_DIFF_BYTES(self, &before, sizeof(Camera));
    }
    return self;
}

// ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z
// — retail 0x1800feb00. Twelve dwords into +0x64.
extern "C" void RtGraphics_Camera_SetCameraToWorld(Camera* self, const Transform3* cameraToWorld) {
    RT_DIFF_BEGIN("Graphics::Camera::SetCameraToWorld");
    Camera before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(Camera));

    std::memcpy(&self->cameraToWorld, cameraToWorld, sizeof(Transform3));

    if (rt_diff_on__) {
        using Fn = void (*)(Camera*, const Transform3*);
        RtOriginal<Fn>(kSetCameraToWorld)(&before, cameraToWorld);
        RT_DIFF_BYTES(self, &before, sizeof(Camera));
    }
}

// ?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z
// — retail 0x1800fe560.
//
// FINDING, recorded because the name promises otherwise: this is
// INSTRUCTION-FOR-INSTRUCTION the same function as SetCameraToWorld above —
// twelve dwords into +0x64 and return. In a class with interpolated state a
// "Snap" would also overwrite the PREVIOUS transform so the next Lerp does not
// smear; `Graphics::Camera` has no previous-transform field for it to write, so
// the distinction the two names draw does not exist in the shipped binary. They
// are kept as two ports rather than one alias so that the diff oracle checks
// each against its OWN export, and so a future divergence between them (a
// different build, a modded DLL) would show up instead of being assumed away.
extern "C" void RtGraphics_Camera_SnapCameraToWorld(Camera* self, const Transform3* cameraToWorld) {
    RT_DIFF_BEGIN("Graphics::Camera::SnapCameraToWorld");
    Camera before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(Camera));

    std::memcpy(&self->cameraToWorld, cameraToWorld, sizeof(Transform3));

    if (rt_diff_on__) {
        using Fn = void (*)(Camera*, const Transform3*);
        RtOriginal<Fn>(kSnapCameraToWorld)(&before, cameraToWorld);
        RT_DIFF_BYTES(self, &before, sizeof(Camera));
    }
}

// ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ — retail 0x1800fe3e0.
//
// HIDDEN RETURN SLOT. `Volume::Frustum` has a user-declared constructor, so MSVC
// returns it by memory: RCX = this, RDX = the caller's slot, RAX = that same
// pointer back. Declaring this without the slot would make it a wild write that
// still returns the right answer, with RAX pointing at the memory it just
// corrupted (reference_hidden_retslot_wild_write.md). The two exe call sites
// already spell the slot out (code/Game/Component/frustumoutlineelement.cpp:97,
// code/Game/Game/weather_particle_draw.cpp:85), so this signature matches them.
//
// Retail moves 25 dwords, so the frustum's three pad bytes travel too.
extern "C" Frustum* RtGraphics_Camera_GetFrustum(const Camera* self, Frustum* ret) {
    RT_DIFF_BEGIN("Graphics::Camera::GetFrustum");
    std::memcpy(ret, &self->frustum, sizeof(Frustum));

    if (rt_diff_on__) {
        Frustum theirs;
        using Fn = Frustum* (*)(const Camera*, Frustum*);
        RtOriginal<Fn>(kGetFrustum)(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Frustum));
    }
    return ret;
}

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
extern "C" Renderer* RtGraphics_Renderer_Ctor(Renderer* self) {
    RT_DIFF_BEGIN("Graphics::Renderer::Renderer");
    Renderer before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(Renderer));

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

    if (rt_diff_on__) {
        using Fn = Renderer* (*)(Renderer*);
        RtOriginal<Fn>(kRendererCtor)(&before);
        RT_DIFF_BYTES(self, &before, sizeof(Renderer));
    }
    return self;
}

// ??1Renderer@Graphics@@QEAA@XZ — retail 0x1800180a0.
//
// ORACLE: POST-STATE ONLY, and this is the honest limit rather than a shortcut.
// The three callees free heap buffers. Running the original a second time on a
// scratch copy of the object would hand the allocator the SAME `_data` pointers
// our call just released — a double free, which is a defect the harness would be
// creating rather than detecting. So there is no "theirs" to compare against and
// RT_DIFF_SHADOW carries what can be checked: that all three array triples come
// back fully zeroed, which is what each of the three DLL destructors ends with.
extern "C" void RtGraphics_Renderer_Dtor(Renderer* self) {
    RT_DIFF_BEGIN("Graphics::Renderer::~Renderer");

    // The retail order is drawables, then draw groups, then scenes. It matters:
    // the group and scene destructors walk their elements' own arrays, and doing
    // them in declaration order would not reproduce the allocator's free
    // sequence, which is observable through the boundary tags.
    RtArrayDtorTrivial(&self->drawables);
    RtArrayDtorDrawGroups(&self->drawGroups);
    RtArrayDtorSceneEntries(&self->scenes);

    const bool cleared = self->scenes._data == nullptr && self->scenes._size == 0 &&
                         self->scenes._capacity == 0 &&
                         self->drawGroups._data == nullptr && self->drawGroups._size == 0 &&
                         self->drawGroups._capacity == 0 &&
                         self->drawables._data == nullptr && self->drawables._size == 0 &&
                         self->drawables._capacity == 0;
    RT_DIFF_SHADOW(cleared,
                   cleared ? "all three array triples zeroed"
                           : "an array triple survived the destructor",
                   "post-state check only; the original cannot be re-run (double free)");
}

// ?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ — retail 0x180017f90.
// `&_drawGroups[_currentDrawGroup].cullCamera`. Pure address arithmetic: no
// bounds check in retail, and none added here (CLAUDE.md — a guard retail lacks
// does not restore the behaviour, it skips it).
extern "C" const Camera* RtGraphics_Renderer_GetCullCamera(Renderer* self) {
    RT_DIFF_BEGIN("Graphics::Renderer::GetCullCamera");
    const Camera* ours = &self->drawGroups._data[self->currentDrawGroup].cullCamera;

    if (rt_diff_on__) {
        using Fn = const Camera* (*)(Renderer*);
        const Camera* theirs = RtOriginal<Fn>(kGetCullCamera)(self);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), static_cast<const void*>(ours));
        RtDiffFormat(b, sizeof(b), static_cast<const void*>(theirs));
        RtDiffReport(&rt_diff_site__, ours == theirs, a, b);
    }
    return ours;
}

// ?GetShader@Renderer@Graphics@@QEBAHXZ — retail 0x180018010.
// `_drawGroups[_currentDrawGroup].shader`.
extern "C" int RtGraphics_Renderer_GetShader(const Renderer* self) {
    RT_DIFF_BEGIN("Graphics::Renderer::GetShader");
    const int ours = self->drawGroups._data[self->currentDrawGroup].shader;

    if (rt_diff_on__) {
        using Fn = int (*)(const Renderer*);
        RT_DIFF_SCALAR(ours, RtOriginal<Fn>(kGetShader)(self));
    }
    return ours;
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
//      BANISHED_RT_GFXRENDER=0 run and a default run produce the same object.
extern "C" MaterialInstance* RtGraphics_MaterialInstance_Ctor(MaterialInstance* self) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::MaterialInstance");
    MaterialInstance before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(MaterialInstance));

    static void* const vtBase = ResolveExport("??_7Base@Object@@6B@");
    static void* const vtExternal = ResolveExport("??_7External@Object@@6B@");
    static void* const vtMaterialInstance = ResolveExport("??_7MaterialInstance@Graphics@@6B@");

    self->vptr = vtBase;
    self->vptr = vtExternal;
    self->external_08 = nullptr;
    self->external_10 = nullptr;
    self->external_18 = nullptr;
    self->vptr = vtMaterialInstance;
    self->shaderClass = 0;
    self->hasRenderTexture = 0;
    self->hasRenderedTexture = 0;
    self->material = nullptr;
    self->parameters._data = nullptr;
    self->parameters._size = 0;
    self->parameters._capacity = 0;
    self->constants._data = nullptr;
    self->constants._size = 0;
    self->constants._capacity = 0;
    self->textures._data = nullptr;
    self->textures._size = 0;
    self->textures._capacity = 0;

    if (rt_diff_on__) {
        using Fn = MaterialInstance* (*)(MaterialInstance*);
        RtOriginal<Fn>(kMaterialInstCtor)(&before);
        RT_DIFF_BYTES(self, &before, sizeof(MaterialInstance));
    }
    return self;
}

// ?SetMaterial@MaterialInstance@Graphics@@QEAAXV?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z
// — retail 0x180103340. One store.
//
// `Object::ExternalPtr<T>` is EIGHT BYTES and is passed BY VALUE, so it arrives
// whole in RDX; there is no reference to dereference. Retail does not touch a
// refcount here — the caller's ExternalPtr and the member end up sharing one,
// which is why BuildParameters can read `_material` back without owning it.
extern "C" void RtGraphics_MaterialInstance_SetMaterial(MaterialInstance* self, void* material) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::SetMaterial");
    MaterialInstance before;
    if (rt_diff_on__) std::memcpy(&before, self, sizeof(MaterialInstance));

    self->material = material;

    if (rt_diff_on__) {
        using Fn = void (*)(MaterialInstance*, void*);
        RtOriginal<Fn>(kSetMaterial)(&before, material);
        RT_DIFF_BYTES(self, &before, sizeof(MaterialInstance));
    }
}

// ?AddTexture@MaterialInstance@Graphics@@QEAAXAEBVString@System@@V?$ExternalPtr@VImageBuffer@Video@@@Object@@@Z
// — retail 0x180103350.
//
// Appends one {name, image} pair to `_textures`. Retail's body is a
// `System::String` copy on the stack (refcount++ at `data - 4`), the array grow
// helper — which takes its own reference when it copy-constructs the tail — and
// then the local's destructor (refcount--, and MemFree(data - 8) at zero). Net
// effect on the string: +1 reference, held by the array. The header convention
// is WAVE2's: `data - 8` is the length, `data - 4` the refcount, and the
// allocator's own boundary tag is a further 8 bytes back
// (docs/runtime_port/WAVE2_ALLOCATOR.md).
//
// ORACLE: POST-STATE ONLY. A scratch-copy replay is not merely unhelpful here,
// it is destructive: the copy would share `_textures._data` with the live object,
// and the grow helper frees the old buffer on reallocation — so "theirs" would
// release memory the real MaterialInstance still points at. RT_DIFF_SHADOW
// therefore checks what our own call can prove: the size went up by exactly one
// and the new tail element holds the two values we were handed.
extern "C" void RtGraphics_MaterialInstance_AddTexture(MaterialInstance* self,
                                                       const void* name, void* image) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::AddTexture");
    const int32_t sizeBefore = self->textures._size;

    MaterialTexture local;
    local.name = nullptr;
    char* data = *static_cast<char* const*>(name);   // System::String is one pointer
    if (data) {
        ++*reinterpret_cast<int32_t*>(data - 4);
        local.name = data;
    }
    local.image = image;

    RtArraySetSizeTexturePairs(&self->textures,
                               static_cast<uint32_t>(self->textures._size + 1), &local);

    if (local.name) {
        int32_t* refCount = reinterpret_cast<int32_t*>(local.name - 4);
        if (--*refCount == 0) RtMemFree(local.name - 8);
    }

    if (rt_diff_on__) {
        const bool ok = self->textures._size == sizeBefore + 1 &&
                        self->textures._data != nullptr &&
                        self->textures._data[sizeBefore].name == data &&
                        self->textures._data[sizeBefore].image == image;
        RT_DIFF_SHADOW(ok,
                       ok ? "tail element holds the name and image passed in"
                          : "append did not land at _size-1",
                       "post-state check only; the original cannot be re-run "
                       "(it would free the live element buffer)");
    }
}

// ============================================================================
// Registration
// ============================================================================
// Both doors for every port: the mangled name AND the retail address. See the
// file header for why — engine/funcs/engine_funcs.cpp:2856 resolves one of these
// twelve by address and the rest by name, and a name-only registration would
// have split one live Camera between two implementations.
//
// Names are spelled as LITERALS here, not through the k* constants above, so
// tools/runtime_census.py can see them.
// ============================================================================
int RtGraphicsRendererRegister() {
    int count = 0;

    // --- Graphics::Camera (all five of the class's exports) -----------------
    RegisterRuntimeExport("??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z",
                          (void*)&RtGraphics_Camera_Ctor); ++count;
    RegisterRuntimeAddr(0x1800fe960, (void*)&RtGraphics_Camera_Ctor);

    RegisterRuntimeExport("?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z",
                          (void*)&RtGraphics_Camera_SetFrustum); ++count;
    RegisterRuntimeAddr(0x1800fe5c0, (void*)&RtGraphics_Camera_SetFrustum);

    RegisterRuntimeExport("?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z",
                          (void*)&RtGraphics_Camera_SetCameraToWorld); ++count;
    RegisterRuntimeAddr(0x1800feb00, (void*)&RtGraphics_Camera_SetCameraToWorld);

    RegisterRuntimeExport("?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z",
                          (void*)&RtGraphics_Camera_SnapCameraToWorld); ++count;
    RegisterRuntimeAddr(0x1800fe560, (void*)&RtGraphics_Camera_SnapCameraToWorld);

    RegisterRuntimeExport("?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ",
                          (void*)&RtGraphics_Camera_GetFrustum); ++count;
    RegisterRuntimeAddr(0x1800fe3e0, (void*)&RtGraphics_Camera_GetFrustum);

    // --- Graphics::Renderer (4 of the 6 rows this lane owns) ----------------
    RegisterRuntimeExport("??0Renderer@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_Renderer_Ctor); ++count;
    RegisterRuntimeAddr(0x180105180, (void*)&RtGraphics_Renderer_Ctor);

    RegisterRuntimeExport("??1Renderer@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_Renderer_Dtor); ++count;
    RegisterRuntimeAddr(0x1800180a0, (void*)&RtGraphics_Renderer_Dtor);

    RegisterRuntimeExport("?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ",
                          (void*)&RtGraphics_Renderer_GetCullCamera); ++count;
    RegisterRuntimeAddr(0x180017f90, (void*)&RtGraphics_Renderer_GetCullCamera);

    RegisterRuntimeExport("?GetShader@Renderer@Graphics@@QEBAHXZ",
                          (void*)&RtGraphics_Renderer_GetShader); ++count;
    RegisterRuntimeAddr(0x180018010, (void*)&RtGraphics_Renderer_GetShader);

    // --- Graphics::MaterialInstance (3 of the 5 rows this lane owns) --------
    RegisterRuntimeExport("??0MaterialInstance@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_MaterialInstance_Ctor); ++count;
    RegisterRuntimeAddr(0x180103980, (void*)&RtGraphics_MaterialInstance_Ctor);

    RegisterRuntimeExport("?SetMaterial@MaterialInstance@Graphics@@QEAAXV?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z",
                          (void*)&RtGraphics_MaterialInstance_SetMaterial); ++count;
    RegisterRuntimeAddr(0x180103340, (void*)&RtGraphics_MaterialInstance_SetMaterial);

    RegisterRuntimeExport("?AddTexture@MaterialInstance@Graphics@@QEAAXAEBVString@System@@V?$ExternalPtr@VImageBuffer@Video@@@Object@@@Z",
                          (void*)&RtGraphics_MaterialInstance_AddTexture); ++count;
    RegisterRuntimeAddr(0x180103350, (void*)&RtGraphics_MaterialInstance_AddTexture);

    return count;
}
