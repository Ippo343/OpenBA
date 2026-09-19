// ============================================================================
// runtime/src/graphics/meshinstance_draw.cpp
// ?Draw@MeshInstance@Graphics@@ (0x180100960) and
// ?Draw@EdgeMeshInstance@Graphics@@ (0x1800a3150).  Vtable batch 6.
// ============================================================================
//
// These are slot 2 of `??_7MeshInstance@Graphics@@6B@` and of
// `??_7EdgeMeshInstance@Graphics@@6B@`, the last unported slot of each table,
// and they are the per-item draw submission for every mesh in the game: pose
// the skeleton, hand the bone matrices and the world transform to the
// Video::Context, bind the vertex/index buffers, and issue the draw.
//
// ---------------------------------------------------------------------------
// WHAT THE PACKED `flags` ARGUMENT CARRIES — different in the two bodies
// ---------------------------------------------------------------------------
// The sixth argument (the first `H` after the float) is a BIT-PACKED word and
// the two classes unpack it differently:
//
//   MeshInstance      node    = (flags >> 12) & 0xf      `SAR EAX,0xc; AND EAX,0xf`
//                     sub     =  flags & 0xfff
//   EdgeMeshInstance  edgePass= (flags >> 15) & 1        `SHR R15D,0xf; AND R15B,1`
//                     sub     =  flags & 0x7fff
//
// MeshInstance's shift is an ARITHMETIC right shift (`SAR`) and
// EdgeMeshInstance's is LOGICAL (`SHR`). On the values these actually carry the
// two agree; the transcription keeps each as it is written, because "this is
// never negative" is a claim about the caller.
//
// ---------------------------------------------------------------------------
// THE 0x18-BYTE DRAW RECORD
// ---------------------------------------------------------------------------
// Both bodies index an array of 0x18-byte records — the same stride
// `Graphics::EdgeMesh::SerializeTriangleListArray` walks six dwords of
// (closure batch 2b, runtime/src/graphics/closure_batch2b.cpp). The four fields
// the draw reads are +0x00, +0x04, +0x08, +0x0c, and +0x14 is a SIGNED node
// index whose sign selects which pair of buffers gets bound:
//
//     rec[+0x14] < 0   ->  the "skinned" buffers, stride 0x20 (or 0x10)
//     rec[+0x14] >= 0  ->  the "rigid" buffers,   stride 0x18 (or 0x08)
//
// MeshInstance scales the record index by `sub * 3` and then by 8
// (`LEA R15,[RAX+RAX*2]` then `[R12 + R15*8]`), i.e. `sub * 0x18`. Written that
// way below rather than folded, because the fold is where an off-by-a-factor
// hides.
//
// ---------------------------------------------------------------------------
// HIDDEN RETURN SLOTS
// ---------------------------------------------------------------------------
// Four of the six helpers return a struct by value from a NON-STATIC member, so
// the slot is **RDX** with `this` in RCX, and each returns that slot in RAX
// which the caller feeds straight into the next call. Verified per call site in
// the disassembly, as batch 4 did for `GetSunCamera`:
//     ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z   (this, ret, q, &s)
//     ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ              (this, ret)
//     ?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@HM@Z
//                                                               (this, ret, n, f)
// `?GetBones@MeshInstance@Graphics@@QEBAHMAEBVTransform@Math@@PEAVMatrix@4@@Z`
// returns an `int` and does NOT have one — its float argument rides in XMM1
// (the SECOND register slot), which is the detail Ghidra's C output loses.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/math/Math.h"
// ENDGAME B.3.5 block E: the real `Graphics::MeshInstance` / `EdgeMeshInstance`.
#include "../../types/graphics/Mesh.h"
#include "../../types/video/Context.h"   // ENDGAME B.3.5 caller batch: Video::Context

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// Video::Context offsets these two bodies poke. Same block lights_vtable.cpp
// documents; only the members used here are listed.
//   +0x10  dirty mask      +0x18 / +0x1c  sub-masks
//   +0x48  a bound object          +0xb8 / +0x138 / +0x178  the vertex binding
//   +0x1b8 / +0x1c0         the index binding
//   +0x3f8 .. +0x404  a float4 of tuning constants
//   +0x408 .. +0x414  a float4 whose w is the fade alpha
//   +0x2160 the render-state object; vtable slot 12 (+0x60) issues the draw
// ---------------------------------------------------------------------------
constexpr uint32_t kCtxDirty   = 0x10;
constexpr uint32_t kCtxSub18   = 0x18;
constexpr uint32_t kCtxSub1c   = 0x1c;
constexpr uint32_t kCtxObj48   = 0x48;
constexpr uint32_t kCtxVB      = 0xb8;
constexpr uint32_t kCtxVBFirst = 0x138;
constexpr uint32_t kCtxVBStride= 0x178;
constexpr uint32_t kCtxIB      = 0x1b8;
constexpr uint32_t kCtxIBFirst = 0x1c0;
constexpr uint32_t kCtxTune    = 0x3f8;
constexpr uint32_t kCtxColour  = 0x408;
constexpr uint32_t kCtxState   = 0x2160;

// The three tuning constants and the fallback, read out of the image with
// read_memory rather than converted from decimal:
//   0x180161670 = 0x3b000000    0x180161668 = 0x3a000000
//   0x180161674 = 0x3c008081    0x18016175c = 0xbf800000 (= -1.0f)
constexpr uint32_t kTune0 = 0x3b000000u;
constexpr uint32_t kTune1 = 0x3a000000u;
constexpr uint32_t kTune2 = 0x3c008081u;
constexpr uint32_t kNegOne = 0xbf800000u;

float AsFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }

// 48 Matrix (0x40 each) — the bone buffer both bodies zero and fill. The count
// is FIXED at 48 in both: retail computes `ECX = arg8 + 0x2e` (MeshInstance) or
// loads the literal 0x2f (EdgeMeshInstance) and runs the zero loop while the
// counter stays non-negative, which is 48 passes of 0x40 bytes. MeshInstance's
// form looks variable and is not — the block it sits in only runs when arg8 is
// 1, which the branch above it has already established.
constexpr int kBoneMatrices = 48;

// ---------------------------------------------------------------------------
// The six named dependencies. Every one is EXPORTED and every one is already
// PORTED, so RT_EXPORT_ONCE (registry-first) lands on our bodies.
// ---------------------------------------------------------------------------
// ENDGAME B.3.5 caller batch: `GetBonesFn`, `SetBonesFn` and `SetTfsFn` went with
// the three by-name doors below, which are member calls now.

// ENDGAME B.3.1 step 2: both are Math:: members. The `(self, ret, ...)` shape
// stays because the call sites work on raw offsets into MeshInstance's untyped
// storage; those B.3.1 bridges go when THAT class is converted.
void* QuaternionLerp(const void* self, void* ret, const void* other, const float* s) {
    *static_cast<Math::Quaternion*>(ret) =
        static_cast<const Math::Quaternion*>(self)->Lerp(
            *static_cast<const Math::Quaternion*>(other),
            *reinterpret_cast<const Math::Scalar*>(s));
    return ret;
}
void* TransformAsMatrix(const void* self, void* ret) {
    *static_cast<Math::Matrix*>(ret) =
        static_cast<const Math::Transform*>(self)->AsMatrix();
    return ret;
}
// ENDGAME B.3.5 caller batch: all three are DEFINED members today —
// `Graphics::MeshInstance::GetBones` in animationstate_transforms.cpp, and
// `Video::Context::SetBones` / `::SetTransformScale` in video/context.cpp — so
// the three by-name doors become member calls. THE DOOR DECIDED THE OVERLOAD:
// `?SetTransformScale@…AEBVMatrix@Math@@AEBVVector4@4@@Z` 0x18001a400 is the
// four-argument setter, not `SetTransform` (0x18001a6b0).
int32_t MeshInstanceGetBones(const void* self, float t, const void* xform, void* out) {
    return reinterpret_cast<const ::Graphics::MeshInstance*>(self)->GetBones(
        t, *reinterpret_cast<const Math::Transform*>(xform),
        static_cast<Math::Matrix*>(out));                            // B.3.5 bridge
}
void ContextSetBones(void* ctx, int32_t count, const void* matrices) {
    reinterpret_cast<Video::Context*>(ctx)->SetBones(
        count, reinterpret_cast<const Math::Matrix*>(matrices));      // B.3.5 bridge
}
void ContextSetTransformScale(void* ctx, int32_t slot, const void* matrix,
                              const void* scale) {
    reinterpret_cast<Video::Context*>(ctx)->SetTransformScale(
        slot, *reinterpret_cast<const Math::Matrix*>(matrix),
        *reinterpret_cast<const Math::Vector4*>(scale));               // B.3.5 bridge
}
// ENDGAME B.3.5 block E: `MeshInstanceGetNodeToWorld` DELETED. It was a
// `RT_EXPORT_ONCE` door onto `?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AV
// Transform@Math@@HM@Z`, i.e. a runtime-internal caller reaching its own
// neighbour through the port registry. That body is now a member of the real
// `Graphics::MeshInstance` (`runtime/src/graphics/mesh.cpp`), so
// `EdgeMeshInstance::Draw` calls it directly and qualified. Its `NodeToWorldFn`
// typedef went with it.

const uint8_t* VideoDevice() {
    return *RT_DATA_SLOT(const uint8_t* const*, "?_singleton@Device@Video@@0PEAV12@EA");
}
const uint8_t* AnimationManager() {
    return *RT_DATA_SLOT(const uint8_t* const*,
                         "?_singleton@AnimationManager@Graphics@@0PEAV12@EA");
}

// ---------------------------------------------------------------------------
// `Math::Transform` — 0x1c: a Quaternion at +0x00 (four floats) and a Vector3
// at +0x10 (three). Both bodies build one on the stack by LERPing the two
// Transforms of an `InterpolatedState<Transform>`:
//
//     out.rotation    = prev.rotation.Lerp(cur.rotation, t)
//     out.translation = prev.translation + (cur.translation - prev.translation) * t
//
// *** `prev` IS THE SECOND OF THE TWO AND `cur` IS THE FIRST. *** Retail puts
// `state + 0x1c` in RCX (the Lerp's `this`) and `state + 0x00` in R8, and the
// translation arithmetic reads `[state+0x10]` as the SUBTRAHEND and
// `[state+0x2c]` as the base. Swapping them runs every animation backwards.
//
// The three translation terms are computed as `(a - b) * t` then `b + that`,
// each in its own register, exactly as retail schedules them. The build has no
// `-march` beyond the x86-64 baseline, so GCC cannot contract these into FMAs
// and the arithmetic is bit-exact against the shipped MULSS/ADDSS pairs.
// ---------------------------------------------------------------------------
void LerpTransform(const uint8_t* state, float t, float* out) {
    const float* const cur  = reinterpret_cast<const float*>(state);
    const float* const prev = reinterpret_cast<const float*>(state + 0x1c);

    // The scalar is passed BY ADDRESS (`AEBVScalar@2@`); retail spills `t` to a
    // stack slot and hands over its address, which is what this local is.
    const float scalar = t;
    float quat[4];
    QuaternionLerp(state + 0x1c, quat, state, &scalar);

    for (int i = 0; i < 3; ++i) {
        const float d = (cur[4 + i] - prev[4 + i]) * t;
        out[4 + i] = prev[4 + i] + d;
    }
    out[0] = quat[0]; out[1] = quat[1]; out[2] = quat[2]; out[3] = quat[3];
}

// ---------------------------------------------------------------------------
// The instanced-pose pool slot, retail 0x180100b73 .. 0x180100bbd. IDENTICAL to
// the helper runtime/src/graphics/mesh.cpp:562 carries for
// `GetNodeToWorld`/`GetInstanceTransform`; spelled again here because that one
// is file-local and a second `extern` declaration of a file-local is the
// link-time coincidence this tree has been bitten by (closure_batch2b.cpp says
// the same about `GeometryVtable`).
//
//     poolId = *(int32*)(layers._data + layers._size * 0x38 - 0x2c);  // MOVSXD
//     hi = (uint8)(poolId >> 8);   lo = (uint8)poolId;
//     bank  = *(void**)(*(void**)AnimationManager + 0x20 + hi * 0x30);
//     poses = *(void**)(bank + lo * 0x48 + 0x38);
//
// `*(void**)AnimationManager` is the SECOND dereference the wave-6 lane paid
// for: the exported symbol is a pointer to the manager and the manager's first
// member is the pool base.
// ---------------------------------------------------------------------------
const uint8_t* InstancedPoses(const uint8_t* self) {
    const uint8_t* const layers = *reinterpret_cast<const uint8_t* const*>(self + 0xe0);
    const int32_t size = *reinterpret_cast<const int32_t*>(self + 0xe8);
    const int64_t poolId = *reinterpret_cast<const int32_t*>(
        layers + static_cast<int64_t>(size) * 0x38 - 0x2c);          // MOVSXD

    const uint64_t hi = static_cast<uint8_t>(poolId >> 8);
    const uint64_t lo = static_cast<uint8_t>(poolId);

    const uint8_t* const base = *reinterpret_cast<const uint8_t* const*>(AnimationManager());
    const uint8_t* const bank =
        *reinterpret_cast<const uint8_t* const*>(base + 0x20 + hi * 0x30);
    return *reinterpret_cast<const uint8_t* const*>(bank + lo * 0x48 + 0x38);
}

// `if (ctx->[off] != {a,b,c,d}) { store; ctx->dirty |= 0x1000; }` over four
// floats. The compare is UCOMISS + JP + JNZ per term in retail — an ORDERED
// compare, so a NaN in either operand takes the "differs" arm. `!=` on floats
// in C++ is the same unordered-aware test for this purpose (NaN != x is true),
// so the branch is reproduced, not approximated.
void SetFloat4(void* ctx, uint32_t off, float a, float b, float c, float d) {
    float* const p = reinterpret_cast<float*>(static_cast<uint8_t*>(ctx) + off);
    if (p[0] == a && p[1] == b && p[2] == c && p[3] == d) return;
    p[0] = a; p[1] = b; p[2] = c; p[3] = d;
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + kCtxDirty) |= 0x1000u;
}

// The vertex-buffer binding triple, which both bodies write twice over.
void BindVertexBuffer(void* ctx, const void* buffer, int32_t first, int32_t stride) {
    uint8_t* const c = static_cast<uint8_t*>(ctx);
    if (*reinterpret_cast<const void* const*>(c + kCtxVB) == buffer &&
        *reinterpret_cast<const int32_t*>(c + kCtxVBFirst) == first &&
        *reinterpret_cast<const int32_t*>(c + kCtxVBStride) == stride)
        return;
    *reinterpret_cast<uint32_t*>(c + kCtxDirty) |= 8u;
    *reinterpret_cast<uint32_t*>(c + kCtxSub1c) |= 1u;
    *reinterpret_cast<const void**>(c + kCtxVB)      = buffer;
    *reinterpret_cast<int32_t*>(c + kCtxVBFirst)     = first;
    *reinterpret_cast<int32_t*>(c + kCtxVBStride)    = stride;
}

void BindIndexBuffer(void* ctx, const void* buffer, int32_t first) {
    uint8_t* const c = static_cast<uint8_t*>(ctx);
    if (*reinterpret_cast<const void* const*>(c + kCtxIB) == buffer &&
        *reinterpret_cast<const int32_t*>(c + kCtxIBFirst) == first)
        return;
    *reinterpret_cast<uint32_t*>(c + kCtxDirty) |= 0x10u;
    *reinterpret_cast<int32_t*>(c + kCtxIBFirst) = first;
    *reinterpret_cast<const void**>(c + kCtxIB)  = buffer;
}

// The draw itself: `obj = ctx->[0x2160]; obj->vtable[12](obj, 0, rec[0], rec[4],
// rec[8], rec[0xc], last)`. SEVEN values — two of them on the stack — and
// Ghidra's C output prints four.
void IssueDraw(void* ctx, const uint8_t* rec, int32_t last) {
    void* const obj = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + kCtxState);
    void** const vt = *reinterpret_cast<void***>(obj);
    using Fn = void (*)(void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
    reinterpret_cast<Fn>(vt[12])(obj, 0,
                                 *reinterpret_cast<const int32_t*>(rec + 0x00),
                                 *reinterpret_cast<const int32_t*>(rec + 0x04),
                                 *reinterpret_cast<const int32_t*>(rec + 0x08),
                                 *reinterpret_cast<const int32_t*>(rec + 0x0c),
                                 last);
}

}  // namespace

// ============================================================================
// ?Draw@MeshInstance@Graphics@@UEBAX…                retail 0x180100960
// ============================================================================
// Arguments, from the mangled name and confirmed against the prologue:
//   RCX this, RDX Context&, R8 Camera&, R9 ExternalPtr<MaterialInstance>,
//   [+0x28] float t, [+0x30] int flags, [+0x38] int mode, [+0x40] int pass.
//
// *** THE CAMERA AND THE MATERIAL ARE NEVER READ. *** R8 and R9 are overwritten
// in the first fifteen instructions and nothing reloads them from the home
// slots. Transcribed as unused rather than "obviously used somewhere".
//
// THE POSE BLOCK IS BEHIND `pass == 1` (`CMP R13D,1 / JNZ`). For any other pass
// the body skips straight to the colour/tuning state and the draw, so the bone
// matrices and the world transform are submitted once per object and not once
// per pass.
//
// The buffer triple comes off `this->_geometry->[0x50]` (`[this+0xc0]` then
// `+0x50`), re-read from the object EVERY time — four separate reloads in
// retail, kept literal.
// ============================================================================
// ENDGAME B.3.5 block E: BODY MOVED onto the member at the bottom of this file.
// The door keeps its name, its address and its registration and delegates with a
// QUALIFIED non-virtual call — the qualification is load-bearing, because a live
// MeshInstance wears the MINTED table whose slot 2 resolves back to this door.
extern "C" void RtGraphics_MeshInstance_Draw(const void* self, void* ctx,
                                             const void* camera, void* material,
                                             float t, int32_t flags,
                                             int32_t mode, int32_t pass) {
    RtGraphics::ExternalPtr mat;
    mat.p = material;
    reinterpret_cast<const ::Graphics::MeshInstance*>(self)
        ->::Graphics::MeshInstance::Draw(
              *reinterpret_cast<Video::Context*>(ctx),
              *reinterpret_cast<const ::Graphics::Camera*>(camera),
              mat, t, flags, mode, pass);
}

namespace Graphics {

// ============================================================================
// void Graphics::MeshInstance::Draw(Video::Context&, const Camera&,
//                                   ExternalPtr<MaterialInstance>, float,
//                                   int, int, int) const           [slot 2]
//                                                   retail 0x180100960
// The banner above the door is the evidence for every offset below; it is not
// repeated. `Video::Context` and `Graphics::Camera` are other blocks' classes and
// are taken by REFERENCE under their real names; the offsets are read through the
// one bridge cast at the top of the body, which is where they were read before.
// ============================================================================
void MeshInstance::Draw(Video::Context& context, const Camera& camera,
                        ExternalPtrHandle material, float t, int32_t flags,
                        int32_t mode, int32_t pass) const {
    (void)camera; (void)material;                  // R8/R9, never reloaded
    void* const ctx = reinterpret_cast<void*>(&context);            // B.3.5 bridge
    const void* const self = this;

    const uint8_t* const d = reinterpret_cast<const uint8_t*>(this);

    const int32_t node = (flags >> 12) & 0xf;      // SAR 0xc / AND 0xf
    const int64_t nodeOff = static_cast<int64_t>(node) * 0xb0;
    const int64_t recOff  = static_cast<int64_t>(flags & 0xfff) * 0x18;

    const uint8_t* const lists =
        *reinterpret_cast<const uint8_t* const*>(
            *reinterpret_cast<const uint8_t* const*>(d + 0xc0) + 0x50);
    const uint8_t* const rec =
        *reinterpret_cast<const uint8_t* const*>(lists + nodeOff + 0x98) + recOff;

    if (pass == 1) {
        const int32_t boneNode = *reinterpret_cast<const int32_t*>(rec + 0x14);

        float world[7];                            // Math::Transform, 0x1c

        if (boneNode < 0) {
            // --- the SKINNED arm: pose 48 bones from this object's own state
            alignas(16) float bones[kBoneMatrices * 16];
            std::memset(bones, 0, sizeof bones);   // 48 x 0x40, see kBoneMatrices

            LerpTransform(d + 0x84, t, world);

            // GetBones is handed the FIRST of the two Transforms verbatim (the
            // seven dwords at +0x84), NOT the lerped one — retail copies
            // `[RBX+0x00 .. RBX+0x18]` into the argument buffer at 0x180100b12.
            float bindPose[7];
            std::memcpy(bindPose, d + 0x84, sizeof bindPose);

            const int32_t count = MeshInstanceGetBones(self, t, bindPose, bones);
            ContextSetBones(ctx, count, bones);
        } else {
            // --- the RIGID arm: one node transform out of the pose array
            const uint8_t* base;
            if (d[0xd8] != 0) base = InstancedPoses(d);                 // instanced
            else              base = *reinterpret_cast<const uint8_t* const*>(d + 0xf0);
            LerpTransform(base + static_cast<int64_t>(boneNode) * 0x38, t, world);
        }

        float matrix[16];
        void* const m = TransformAsMatrix(world, matrix);
        ContextSetTransformScale(ctx, 0, m, d + 0x10c);
    }

    // ---- the fade colour ----------------------------------------------------
    // `alpha = t * this->[0x7c] + (1 - t) * this->[0x80]`, in that order:
    // XMM9 = t * [0x7c] is computed FIRST and the (1-t) term is added to it.
    const float alpha = (t * *reinterpret_cast<const float*>(d + 0x7c)) +
                        ((1.0f - t) * *reinterpret_cast<const float*>(d + 0x80));
    SetFloat4(ctx, kCtxColour, 1.0f, 1.0f, 1.0f, alpha);

    // ---- the material object, bound only when `mode != 1` -------------------
    if (mode != 1) {
        void* const obj = *reinterpret_cast<void* const*>(lists + nodeOff + 0x08);
        void** const slot = reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + kCtxObj48);
        if (obj != nullptr && obj != *slot) {
            uint8_t* const c = static_cast<uint8_t*>(ctx);
            *reinterpret_cast<uint32_t*>(c + kCtxDirty) |= 2u;
            *reinterpret_cast<uint32_t*>(c + kCtxSub18) |= 8u;
            *slot = obj;
        }
    }

    SetFloat4(ctx, kCtxTune, AsFloat(kTune0), AsFloat(kTune1), AsFloat(kTune2),
              AsFloat(kNegOne));

    // ---- the buffer bindings ------------------------------------------------
    // `this->_geometry->[0x50]` is RE-READ before each of the two, which is why
    // it is loaded twice below rather than reused from `lists`.
    {
        const uint8_t* const l1 = *reinterpret_cast<const uint8_t* const*>(
            *reinterpret_cast<const uint8_t* const*>(d + 0xc0) + 0x50);
        const bool skinned = *reinterpret_cast<const int32_t*>(rec + 0x14) < 0;
        BindVertexBuffer(ctx, l1 + (skinned ? 0x58 : 0x18) + nodeOff, 0,
                         skinned ? 0x20 : 0x18);

        const uint8_t* const l2 = *reinterpret_cast<const uint8_t* const*>(
            *reinterpret_cast<const uint8_t* const*>(d + 0xc0) + 0x50);
        BindIndexBuffer(ctx, l2 + (skinned ? 0x78 : 0x38) + nodeOff, 0);
    }

    IssueDraw(ctx, rec, pass);
}

}  // namespace Graphics

// ============================================================================
// ?Draw@EdgeMeshInstance@Graphics@@UEBAX…            retail 0x1800a3150
// ============================================================================
// The edge-mesh twin. Four differences from the body above, every one of them
// something a "same but for edges" assumption would get wrong:
//
//   1. The flags word is split at BIT 15, not bit 12, and the high bit selects
//      between TWO record arrays — `_geometry->[0x158]` when set and
//      `_geometry->[0x148]` when clear — rather than indexing one of them.
//   2. The pose source is a SEPARATE MeshInstance at `this->[0xc8]`, not this
//      object. When it is non-null and the record's node index is >= 0, the
//      transform comes from `MeshInstance::GetNodeToWorld(node, t)` rather than
//      from a lerp; when it is NULL the object falls back to its own
//      InterpolatedState at +0x84 AND uses a UNIT scale vector instead of the
//      mesh's — `(1, 1, 1, 0)`, four immediates at 0x1800a350a.
//   3. The tuning float4's w is `1.0f / (float)Video::Device::_singleton->[4]`
//      — a reciprocal of the device WIDTH, computed with CVTDQ2PS + DIVSS — not
//      MeshInstance's -1.0f constant. That is the edge-thickness-in-pixels term.
//   4. The colour float4 is a flat `(1, 1, 1, 1)`: there is no fade alpha, so
//      neither `this->[0x7c]` nor `this->[0x80]` is read, and the last argument
//      of the draw call is the LITERAL 1 rather than the pass number.
//
// The bone buffer's 48 matrices and the 0x18 record stride are the same.
// ============================================================================
// ENDGAME B.3.5 block E: BODY MOVED onto the member below; the door delegates.
extern "C" void RtGraphics_EdgeMeshInstance_Draw(const void* self, void* ctx,
                                                 const void* camera, void* material,
                                                 float t, int32_t flags,
                                                 int32_t mode, int32_t pass) {
    RtGraphics::ExternalPtr mat;
    mat.p = material;
    reinterpret_cast<const ::Graphics::EdgeMeshInstance*>(self)
        ->::Graphics::EdgeMeshInstance::Draw(
              *reinterpret_cast<Video::Context*>(ctx),
              *reinterpret_cast<const ::Graphics::Camera*>(camera),
              mat, t, flags, mode, pass);
}

namespace Graphics {

void EdgeMeshInstance::Draw(Video::Context& context, const Camera& camera,
                            ExternalPtrHandle material, float t, int32_t flags,
                            int32_t mode, int32_t pass) const {
    (void)camera; (void)material; (void)mode; (void)pass;
    void* const ctx = reinterpret_cast<void*>(&context);            // B.3.5 bridge

    const uint8_t* const d = reinterpret_cast<const uint8_t*>(this);

    const bool edgePass = ((static_cast<uint32_t>(flags) >> 15) & 1u) != 0;   // SHR/AND
    const int64_t recOff = static_cast<int64_t>(flags & 0x7fff) * 0x18;

    const uint8_t* const geometry = *reinterpret_cast<const uint8_t* const*>(d + 0xc0);
    const uint8_t* const array =
        *reinterpret_cast<const uint8_t* const*>(geometry + (edgePass ? 0x158 : 0x148));
    const uint8_t* const rec = array + recOff;
    const int32_t boneNode = *reinterpret_cast<const int32_t*>(rec + 0x14);

    const void* const poseSource = *reinterpret_cast<const void* const*>(d + 0xc8);

    float world[7];                                 // Math::Transform, 0x1c
    float scale[4];                                 // the Vector4 handed to the ctx

    if (boneNode < 0) {
        // --- SKINNED: pose 48 bones through the OTHER MeshInstance -----------
        alignas(16) float bones[kBoneMatrices * 16];
        std::memset(bones, 0, sizeof bones);

        LerpTransform(d + 0x84, t, world);

        float bindPose[7];
        std::memcpy(bindPose, d + 0x84, sizeof bindPose);
        const int32_t count = MeshInstanceGetBones(poseSource, t, bindPose, bones);
        ContextSetBones(ctx, count, bones);

        std::memcpy(scale, *reinterpret_cast<const uint8_t* const*>(d + 0xc8) + 0x10c,
                    sizeof scale);
    } else if (poseSource != nullptr) {
        // --- RIGID, driven by the mesh instance ------------------------------
        // ENDGAME B.3.5 block E: this was `RT_EXPORT_ONCE(…"?GetNodeToWorld@
        // MeshInstance@Graphics@@QEBA?AVTransform@Math@@HM@Z")` — a runtime-
        // internal caller reaching its own neighbour through the port registry.
        // The body is now a member of the real class (`mesh.cpp`), so this is a
        // direct qualified member call and the resolve is gone.
        const Math::Transform got =
            _followed->MeshInstance::GetNodeToWorld(boneNode, t);
        std::memcpy(world, &got, sizeof world);
        std::memcpy(scale, static_cast<const uint8_t*>(poseSource) + 0x10c, sizeof scale);
    } else {
        // --- RIGID, standalone: lerp our own state and use a UNIT scale ------
        LerpTransform(d + 0x84, t, world);
        scale[0] = 1.0f; scale[1] = 1.0f; scale[2] = 1.0f; scale[3] = 0.0f;
    }

    float matrix[16];
    void* const m = TransformAsMatrix(world, matrix);
    ContextSetTransformScale(ctx, 0, m, scale);

    SetFloat4(ctx, kCtxColour, 1.0f, 1.0f, 1.0f, 1.0f);

    // `CVTDQ2PS` on the device width, then `1.0f / that`. A signed int-to-float
    // conversion, then a single DIVSS — not a multiply by a precomputed
    // reciprocal, which would round differently.
    const float deviceWidth =
        static_cast<float>(*reinterpret_cast<const int32_t*>(VideoDevice() + 0x04));
    SetFloat4(ctx, kCtxTune, AsFloat(kTune0), AsFloat(kTune1), AsFloat(kTune2),
              1.0f / deviceWidth);

    // ---- the buffer bindings ------------------------------------------------
    // Four (offset, stride) pairs, chosen by `edgePass` and the record's sign.
    // The base is `this->_geometry`, RE-READ from the object for each of the two
    // bindings exactly as retail reloads it.
    {
        const uint8_t* const g1 = *reinterpret_cast<const uint8_t* const*>(d + 0xc0);
        const bool skinned = boneNode < 0;
        const uint32_t vbOff    = edgePass ? (skinned ? 0x108u : 0x0c8u)
                                           : (skinned ? 0x088u : 0x048u);
        const int32_t  vbStride = edgePass ? (skinned ? 0x20   : 0x10)
                                           : (skinned ? 0x10   : 0x08);
        BindVertexBuffer(ctx, g1 + vbOff, 0, vbStride);

        const uint8_t* const g2 = *reinterpret_cast<const uint8_t* const*>(d + 0xc0);
        const uint32_t ibOff = edgePass ? (skinned ? 0x128u : 0x0e8u)
                                        : (skinned ? 0x0a8u : 0x068u);
        BindIndexBuffer(ctx, g2 + ibOff, 0);
    }

    IssueDraw(ctx, rec, 1);                         // the LITERAL 1, not `pass`
}

}  // namespace Graphics

// ============================================================================
// Registration — name AND address for both.
//
// *** THE ADDRESS DOOR NEEDED A CHECK BEFORE IT WAS WRITTEN. *** This namespace
// contains one row where a RegisterRuntimeAddr is ACTIVE HARM:
// `standalone/scene_watch.cpp` compares `?AddDrawItems@MeshInstance@`'s retail
// address against a live vtable slot as an IDENTITY TOKEN, so registering that
// address makes the comparison lie (wave21_lane_gfxbody.cpp:311). Both
// addresses below were grepped across code/, engine/, runtime/, standalone/ and
// dx11/ before this block was written: 0x180100960 and 0x1800a3150 appear only
// in the generated mint table and in this batch's own comments, so neither is
// an identity token and both doors are safe.
// ============================================================================
int RtGraphicsMeshInstanceDrawRegister() {
    int n = 0;

    RegisterRuntimeExport("?Draw@MeshInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z",
                          (void*)&RtGraphics_MeshInstance_Draw); ++n;
    RegisterRuntimeAddr(0x180100960ull, (void*)&RtGraphics_MeshInstance_Draw);

    RegisterRuntimeExport("?Draw@EdgeMeshInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z",
                          (void*)&RtGraphics_EdgeMeshInstance_Draw); ++n;
    RegisterRuntimeAddr(0x1800a3150ull, (void*)&RtGraphics_EdgeMeshInstance_Draw);

    return n;
}
