// ============================================================================
// runtime/src/graphics/batchc_gfx_leaves.cpp
//   BATCH C — the named Graphics leaves today's vtable batches left on the DLL
// ============================================================================
//
// Ten bodies, every one of them a leaf of the cutover closure and every one of
// them previously reached BY NAME from a ported caller (the "NOT PORTED,
// reached by name" comments in lights_vtable.cpp / graphicsmesh_vtable.cpp).
// Those comments are corrected AT THEIR SOURCE by this batch (PLAN.md §6
// step 8); see the end of this banner for the list.
//
//   retail       ord   export name
//   -----------  ----  ---------------------------------------------------
//   0x180105240  2279  ?DrawItemList@Renderer@Graphics@@QEBAXAEAVContext@Video@@HHH@Z
//   0x1800a48e0  3386  ?PlatformSerialize@GraphicsMesh@Graphics@@QEAAXAEAVSerializer@IO@@@Z
//   0x180106050  2135  ?CreateTargets@SpotLight@Graphics@@AEAAXXZ
//   0x180103c70   643  ??1Particle@Graphics@@UEAA@XZ
//   0x1800a51c0   645  ??1ParticleSystem@Graphics@@UEAA@XZ
//   0x1800160a0    38  ??0Animation@Graphics@@QEAA@AEBV01@@Z   (the COPY ctor)
//   0x1800fb290   525  ??1Animation@Graphics@@QEAA@XZ
//   0x1800a2780   580  ??1EdgeMesh@Graphics@@UEAA@XZ
//   0x180102eb0  2402  ?GenerateTextures@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z
//   0x1800a5520   646  ??1ParticleSystemInstance@Graphics@@UEAA@XZ
//
// Every one read from the shipped retail Runtime-x64.dll's DISASSEMBLY
// (ghidra-mcp disassemble_function, program="Runtime-x64.dll"), never from
// Ghidra's decompiled C. Every export name was read back out of the export
// directory by VA with pefile before this file was written.
//
// ---------------------------------------------------------------------------
// ⚠ ATTENDANCE IS ZERO ON A DLL-PRESENT RUN, AND THAT IS STRUCTURAL
// ---------------------------------------------------------------------------
// MEASURED, 2026-09-12, one `BANISHED_AUTOSTART=1 the diff oracle armed
// BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` run to the in-game world
// and a clean self-quit (exit 0, empty standalone_crash.log, 154 `startTown`
// lines): NONE of the ten sites below entered, so none appears in the diff oracle
// summary at all.
//
// THAT IS NOT "the ports are dead code". It is what the seam does while the DLL
// is loaded. Every caller of `DrawItemList` in this tree is one of the five
// light `Draw` arms in lights_vtable.cpp, and those are VTABLE SLOTS of
// ??_7{Ambient,Spot,Sun}Light@Graphics@@6B@ — rows the mint has not promoted.
// With the DLL present those tables still hold the DLL's own bodies, so the
// DLL's `SunLight::Draw` reaches the DLL's `DrawItemList` by rel32 and never
// consults the port registry. The same holds for `Context::SetLight` (reached
// only from those same arms) and for the four destructors, which the DLL's own
// deleting-dtor slots call internally. The proof that the seam itself is fine
// is in the same log: `Graphics::SunLight::SunLight` and
// `Graphics::ParticleSystemInstance::ParticleSystemInstance` — the NAME-reached
// members of the very same classes — entered 123 times between them.
//
// So these ten are verified by TRANSCRIPTION against the disassembly, and their
// first execution will be at cutover, where the mint's vtable rows are ours.
// A reader who meets a green the diff oracle summary must not read it as coverage of
// this file (reference_uncalled_port_is_not_verified.md).
//
// ---------------------------------------------------------------------------
// FIVE THINGS THAT WOULD BE WRONG IF READ FROM THE DECOMPILER
// ---------------------------------------------------------------------------
// 1. `Renderer::DrawItemList` TAKES FIVE VALUES (this + four ints) and the
//    fifth lives in its own home slot at [RBP+0xc0]. It is the SHADER INDEX:
//    the body indexes `item->material->_programs[arg5]` with it and hands it to
//    `MaterialInstance::Setup` and, as the 7th argument, to the geometry's
//    `Draw`. lights_vtable.cpp's banner already recorded the five-value fact
//    for the three Draw arms that tail-jump here; this body is what those five
//    values are FOR.
// 2. The two 4x4 matrices DrawItemList hands `Context::SetCamera` are BUILT ON
//    THE STACK, not copied: the first is the cull camera's `Transform3`
//    (camera+0x64, twelve floats) EXPANDED to a Matrix with an explicit
//    (0,0,0,1) fourth column written as three zeroed floats and one 1.0f read
//    from 0x18016169c; the second is the camera's projection, sixteen dwords
//    copied verbatim. Getting the expansion wrong silently transposes the view.
// 3. `??1Animation@` and `??1Particle@` release `System::String`s with retail's
//    OWN asymmetry: on the LAST reference the slot is left holding `str-8`
//    (a dangling pointer into the freed block) and only on a SURVIVING
//    reference is it nulled. `~Particle` additionally branches on the ZF of the
//    `ADD qword [slot],-8` itself, so a string whose data pointer was exactly 8
//    is not freed. Both are reproduced; neither is a transcription slip.
// 4. `~EdgeMesh` destroys its eight Video buffer headers in DESCENDING address
//    order and alternates IndexBuffer/VertexBuffer PlatformDestruct — the
//    pairs are (verts, indices) in memory but (indices, verts) in teardown.
// 5. `GraphicsMesh::PlatformSerialize` has TWO device-interface arms over the
//    same stream, the shape runtime/src/video/shaderprogram.cpp spells out:
//    each arm builds its OWN ReaderWriter and, when its interface is not the
//    active one, calls `Skip()` so the stream still advances past that
//    backend's blob. The arms are NOT interchangeable: arm 0 hands the whole
//    triangle array to the shared helper 0x180014850, arm 1/2 emits the same
//    six dwords per element inline. Collapsing them into one loop would emit
//    the right bytes for the wrong backend.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Camera.h"
#include "../../types/graphics/GeneratedImageBuffer.h"  // ENDGAME B.3.5 block GC
#include "../../types/graphics/GraphicsMesh.h"   // ENDGAME B.3.5 block G
#include "../../types/graphics/Light.h"          // Graphics::SpotLight -- block GC
#include "../../types/graphics/Material.h"
#include "../../types/graphics/Mesh.h"
#include "../../types/io/Serializer.h"
#include "../../types/video/Context.h"           // ENDGAME B.3.5 block GC
// ENDGAME B.3.5 caller batch: the cross-class edges this file opens.
#include "../../types/graphics/ParticleInstance.h"  // Graphics::ParticleInstance
#include "../../types/video/Buffers.h"              // Video::{Index,Vertex}Buffer
#include "../../types/video/Device.h"               // Video::RenderBuffer
#include "../../types/collection/Collection.h"      // rtcoll:: the array instantiations
#include "../../types/graphics/Particle.h"          // RtGraphics_Emitter_OperatorDelete
#include "../picking/scene_grid.h"                  // rtpickgrid::TransformArrayAssign / ArrayDtorTransform

namespace rtcoll { void MemFree(void* p); }   // runtime/src/collection/array.cpp

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
using RtGraphics::Camera;
using RtGraphics::MaterialInstance;
using RtGraphics::Renderer;
using RtGraphics::RendererDrawGroup;
using RtGraphics::RendererDrawItem;
using RtGraphics::EdgeMesh;

namespace {

// ---------------------------------------------------------------------------
// Retail vtable constants. Resolved through the NAME (all five are exported
// data symbols — checked by VA with pefile), so a minted/promoted row is used
// where one exists and the DLL's own table otherwise.
// ---------------------------------------------------------------------------
// (ENDGAME B.3.9 GFX: `VtParticleInstance` / `VtParticleSystemInstance` are
// gone with the ??1ParticleSystemInstance door that restamped them.)

// ---------------------------------------------------------------------------
// Named dependencies, called by name (B3R): the class members, and the
// unexported template instantiations under their body names (rtcoll::,
// rtpickgrid::).
// ---------------------------------------------------------------------------

// (ENDGAME B.3.9 GFX: `GeometryDtor`, the ??1Geometry door call of the retired
// ??1ParticleSystemInstance door, is gone; the real dtor chains ~Geometry.)

// (ENDGAME B.3.9 GFX: `ExternalDtor`, the ??1External door the retired ??1EdgeMesh
// door tail-called, is gone; the real ~EdgeMesh chains ~External.)

// Video::{Index,Vertex}Buffer::PlatformDestruct — 0x1800f3760 / 0x1800f58c0,
// both PORTED (video/buffers.cpp) and registered by name and address.
// ENDGAME B.3.5 caller batch: both are DEFINED members, so both are member calls.
void IndexBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::IndexBuffer*>(p)->PlatformDestruct();   // B.3.5 bridge
}
void VertexBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::VertexBuffer*>(p)->PlatformDestruct();  // B.3.5 bridge
}

// `??2ImageBuffer@Video@@SAPEAX_K@Z` / `??2RenderBuffer@Video@@SAPEAX_K@Z` —
// the class operator news. CreateTargets INLINES both (Enter(alloc+0x640);
// Allocate(alloc, n, 8); Leave), which is instruction for instruction what
// these two exported statics are; imagebuffer_derived.cpp reaches the identical
// inlining the same way.
void* ImageBufferOperatorNew(uint64_t n) {
    return Video::ImageBuffer::operator new(n);    // 0x18001e910
}
void* RenderBufferOperatorNew(uint64_t n) {
    return Video::RenderBuffer::operator new(n);   // 0x18001f2e0
}
void ImageBufferCtor(void* p) { ::new (p) Video::ImageBuffer(); }   // ENDGAME B.3.9 VS: `::new (p) Video::ImageBuffer()` -- the ??0 door and the minted row are retired
// ENDGAME B.3.5 caller batch: DEFINED members of the real `Video::RenderBuffer`
// (buffers.cpp / device.cpp). The `RbCreateFn` typedef went with its door.
void RenderBufferPlatformConstruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformConstruct();   // B.3.5 bridge
}
bool RenderBufferCreate(void* rb, const void* colour, const void* depth) {
    return reinterpret_cast<Video::RenderBuffer*>(rb)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(colour),
        reinterpret_cast<const Video::ImageBuffer*>(depth));           // B.3.5 bridge
}

// Video::Context setters DrawItemList drives. Both PORTED (video/context.cpp)
// and registered by name and address.
// ENDGAME B.3.5 caller batch: both are DEFINED members. THE DOOR DECIDED THE
// OVERLOAD — `?SetTransform@…QEAAXHAEBVMatrix@Math@@@Z` 0x18001a6b0 is the
// two-argument setter, NOT `SetTransformScale` (0x18001a400).
void ContextSetCamera(void* ctx, const void* view, const void* proj) {
    reinterpret_cast<Video::Context*>(ctx)->SetCamera(
        *reinterpret_cast<const Math::Matrix*>(view),
        *reinterpret_cast<const Math::Matrix*>(proj));                 // B.3.5 bridge
}
void ContextSetTransform(void* ctx, int32_t slot, const void* m) {
    reinterpret_cast<Video::Context*>(ctx)->SetTransform(
        slot, *reinterpret_cast<const Math::Matrix*>(m));              // B.3.5 bridge
}

// `?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z` 0x180102c90 —
// PORTED (wave21_lane_gfxbody.cpp), and a DEFINED member since block GC, so this
// is a member call (ENDGAME B.3.5 caller batch).
void MaterialInstanceSetup(const void* mi, void* ctx, int32_t shader) {
    reinterpret_cast<const Graphics::MaterialInstance*>(mi)
        ->Setup(*reinterpret_cast<Video::Context*>(ctx), shader);       // B.3.5 bridge
}

// The `1.0f` at 0x18016169c, as a BIT PATTERN read out of the image rather
// than converted from decimal (lights_vtable.cpp's rule).
constexpr uint32_t kBits_one = 0x3f800000u;
float AsFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }

// ---------------------------------------------------------------------------
// The System::String release idiom, spelled once. Retail open-codes it at four
// sites in this file.
//
//   if (!s) return;
//   if (--s[-1] != 0) { *slot = nullptr; return; }     // a reference survives
//   *slot = s - 8;                                      // LAST reference:
//   MemFree(*slot);                                     // slot keeps str-8
//
// THE SLOT IS DELIBERATELY LEFT DANGLING ON THE FREE ARM. That is what
// 0x1800fb2c2/0x1800e040e/0x180103e17 do, and vtbatch7b_stream_run_win.cpp's
// ReleaseString already records the same shape. `checkZeroAfterAdjust` is
// ~Particle's extra twist: it branches on the ZF of the `ADD qword [slot],-8`
// and skips the free when the adjusted pointer is zero.
// ---------------------------------------------------------------------------
void ReleaseString(void** slot, bool checkZeroAfterAdjust) {
    uint8_t* s = static_cast<uint8_t*>(*slot);
    if (s == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(s - 4);
    --(*rc);
    if (*rc != 0) { *slot = nullptr; return; }
    uint8_t* blk = s - 8;
    *slot = blk;                                 // ADD qword [slot],-8
    if (checkZeroAfterAdjust && blk == nullptr) return;   // JZ on that ADD's ZF
    rtcoll::MemFree(blk);
}

}  // namespace

// ############################################################################
// Graphics::MaterialInstance::GenerateTextures
// ?GenerateTextures@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z
// retail 0x180102eb0, ordinal 2402
// ############################################################################
//
// The render-TO-texture sibling of `RenderTextures` (0x180102df0, ported in
// wave9_lane_gfx.cpp): same walk over `_textures` (0x10 stride, count at
// +0x58), but gated on `hasRenderTexture` (+0x24, not +0x25), selecting
// entries whose kind is 4 (not 5) and dispatching vtable slot +0x50 (not
// +0x58) with (Context&, int).
//
// THE SAME TWO FAITHFUL UGLINESSES AS ITS SIBLING, both kept:
//   * `obj->+0x2c` is dereferenced BEFORE `obj` is null-checked (0x180102eea
//     vs 0x180102f02). A null entry faults in retail and faults here.
//   * the loop bound `*(int*)(this+0x58)` is RE-READ every iteration
//     (0x180102f1c), not hoisted, so a slot-0x50 handler that appends a
//     texture is seen.
//
// ORACLE: none. The dispatch renders into a GPU target; running the shipped
// body afterwards would render a second time. the diff oracle is the effectiveness
// control — almost every call returns at the +0x24 test.
// ############################################################################
// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::MaterialInstance::GenerateTextures`, in this file's existing
// `namespace Graphics` block below. The door keeps its name and both its
// registrations (by name and at 0x180102eb0).
//
// THE DEAD EFFECTIVENESS CONTROL IS GONE, for the same reason its sibling's was
// in wave9_lane_gfx.cpp: `const bool didWork = …` had no reader anywhere in the
// tree. The banner above still calls it "the diff oracle", and that sentence was
// describing a variable nothing printed.
extern "C" void RtGraphics_MaterialInstance_GenerateTextures(const void* selfv,
                                                             void* ctx, int32_t arg) {
    reinterpret_cast<const Graphics::MaterialInstance*>(selfv)      // B.3.5 bridge
        ->Graphics::MaterialInstance::GenerateTextures(
            *reinterpret_cast<Video::Context*>(ctx), arg);          // B.3.5 bridge
}

// ############################################################################
// Graphics::ParticleSystemInstance::~ParticleSystemInstance
// ??1ParticleSystemInstance@Graphics@@UEAA@XZ   retail 0x1800a5520, ordinal 646
// ############################################################################
//
//   this->vtable = ??_7ParticleSystemInstance@
//   if (this->_resource->[0x20] == 0 && this->_f_d8)      // UNGUARDED deref
//       { ArrayDtor(_f_d8); operator delete(_f_d8); }
//   this->vtable = ??_7ParticleInstance@
//   ParticleInstance::RemoveState(this)
//   tail ~Geometry(this)
//
// `this->_resource` (+0xc0) IS DEREFERENCED WITHOUT A NULL TEST (0x1800a5545).
// The ctor initialises it to null, so a ParticleSystemInstance destroyed before
// anything attached a resource faults in retail. Reproduced, not guarded
// (CLAUDE.md: a guard skips the behaviour rather than restoring it).
// ############################################################################
//
// ENDGAME B.3.9 block GFX: the door is RETIRED with the RT_DELDTOR row
// 0x180020ce0 it served; its body is the real protected
// `~ParticleSystemInstance()` below. The two restamps, RemoveState
// (~ParticleInstance) and the ~Geometry tail are the compiler's chain.
Graphics::ParticleSystemInstance::~ParticleSystemInstance() {
    const uint8_t* const resource = reinterpret_cast<const uint8_t*>(_resource);
    if (resource[0x20] == 0) {                          // UNGUARDED, as retail
        void* const owned = _f_d8;
        if (owned != nullptr) {
            // 0x180021490: ~Array<T> for a trivially destructible T — frees
            // [0] and zeroes [0]/[8]. 0x180020600: the matching class
            // operator delete. Both unexported template instantiations,
            // called by their body names.
            rtcoll::RtColl_ArrayDtor_180021490(owned);
            RtGraphics_Emitter_OperatorDelete(owned);
        }
    }
}

// ############################################################################
// ??1ParticleSystem@Graphics@@UEAA@XZ 0x1800a51c0 / ??1Particle@Graphics@@UEAA@XZ
// 0x180103c70 -- the two transcribed DOORS are gone (ENDGAME B.3.9 block GFX).
// Their callers were the retired RT_DELDTOR rows 0x180020a70 / 0x180020160 and
// each other. The bodies live on as `~ParticleSystem` (the owned pool walk,
// vtbatch7_animgroup_particlesystem.cpp) and `~Particle` (`ReleaseFields`,
// animation.cpp); retail's inlined ~External is the compiler's ~External.
// ############################################################################
// ############################################################################
// Graphics::Animation::Animation(const Animation&)
// ??0Animation@Graphics@@QEAA@AEBV01@@Z         retail 0x1800160a0, ordinal 38
// ############################################################################
//
// The COPY constructor, not the default one. Returns `this` in RAX.
//
//   this->[0x00] = 0; this->[0x08] = 0;  ArrayAssign_18001b270(this,   &src)
//   AcquireString(&this->[0x10], src->[0x10])     ; +0 then ++refcount
//   AcquireString(&this->[0x18], src->[0x18])
//   AcquireString(&this->[0x20], src->[0x20])
//   +0x28 int, +0x2c byte, +0x30/+0x34/+0x38 int, +0x3c byte,
//   +0x40/+0x44/+0x48/+0x4c/+0x50 int, +0x58 qword   — all plain copies
//   this->[0x60] = 0; this->[0x68] = 0;  ArrayAssign_18001b360(this+0x60, src+0x60)
//   return this
//
// THE TWO ARRAY MEMBERS ARE ZEROED BEFORE THE ASSIGN, not after: the assign
// helper reads the destination's `_data`/`_size` to decide whether to reuse the
// buffer, so a copy ctor that did not clear them first would free the SOURCE
// object's memory. Both helpers are unexported template instantiations still on
// the DLL (0x18001b270 for the +0x00 element type, 0x18001b360 for the +0x60
// one — the latter is the `TransformArrayAssign` picking/scene_grid.cpp names).
//
// +0x54 IS NOT COPIED. Retail copies +0x50 as a dword and then +0x58 as a
// qword, leaving the four bytes at +0x54 holding whatever the allocator handed
// back. Left alone here for the reason Camera.h's `_pad24` note gives: zeroing
// it would make our copy differ from retail's on a byte retail never writes.
// ############################################################################
extern "C" void* RtGraphics_Animation_CopyCtor(void* selfv, const void* srcv) {
    uint8_t* const self = static_cast<uint8_t*>(selfv);
    const uint8_t* const src = static_cast<const uint8_t*>(srcv);

    *reinterpret_cast<uint64_t*>(self + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x08) = 0;
    rtcoll::RtColl_ArrayAssign_18001b270(self, src);   // 0x18001b270

    // The three refcounted names, ASCENDING — the mirror of the destructor's
    // descending release. Each slot is zeroed first and only then overwritten,
    // exactly as retail does it (`MOV [RBX+off],RSI` with RSI == 0, then the
    // conditional store and `INC dword [RAX-4]`).
    const uint32_t kStrOffsets[3] = { 0x10u, 0x18u, 0x20u };
    for (int k = 0; k < 3; ++k) {
        const uint32_t off = kStrOffsets[k];
        *reinterpret_cast<void**>(self + off) = nullptr;
        uint8_t* s = *reinterpret_cast<uint8_t* const*>(src + off);
        if (s != nullptr) {
            *reinterpret_cast<uint8_t**>(self + off) = s;
            ++(*reinterpret_cast<int32_t*>(s - 4));
        }
    }

    *reinterpret_cast<int32_t*>(self + 0x28) = *reinterpret_cast<const int32_t*>(src + 0x28);
    self[0x2c] = src[0x2c];
    *reinterpret_cast<int32_t*>(self + 0x30) = *reinterpret_cast<const int32_t*>(src + 0x30);
    *reinterpret_cast<int32_t*>(self + 0x34) = *reinterpret_cast<const int32_t*>(src + 0x34);
    *reinterpret_cast<int32_t*>(self + 0x38) = *reinterpret_cast<const int32_t*>(src + 0x38);
    self[0x3c] = src[0x3c];
    *reinterpret_cast<int32_t*>(self + 0x40) = *reinterpret_cast<const int32_t*>(src + 0x40);
    *reinterpret_cast<int32_t*>(self + 0x44) = *reinterpret_cast<const int32_t*>(src + 0x44);
    *reinterpret_cast<int32_t*>(self + 0x48) = *reinterpret_cast<const int32_t*>(src + 0x48);
    *reinterpret_cast<int32_t*>(self + 0x4c) = *reinterpret_cast<const int32_t*>(src + 0x4c);
    *reinterpret_cast<int32_t*>(self + 0x50) = *reinterpret_cast<const int32_t*>(src + 0x50);
    // +0x54 deliberately untouched — see the banner.
    *reinterpret_cast<uint64_t*>(self + 0x58) = *reinterpret_cast<const uint64_t*>(src + 0x58);

    *reinterpret_cast<uint64_t*>(self + 0x60) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x68) = 0;
    rtpickgrid::TransformArrayAssign(self + 0x60, src + 0x60);   // 0x18001b360

    return self;                                          // MOV RAX,RBX
}

// ############################################################################
// Graphics::Animation::~Animation
// ??1Animation@Graphics@@QEAA@XZ                retail 0x1800fb290, ordinal 525
// ############################################################################
//
//   ArrayDtor_18001c100(this + 0x60)        ; the Transform array
//   ReleaseString(&this->[0x20])            ; the three names, in DESCENDING
//   ReleaseString(&this->[0x18])            ; offset order — the mirror of the
//   ReleaseString(&this->[0x10])            ; copy ctor's ascending acquire
//   tail ArrayDtor_18001c020(this)          ; the +0x00 array
//
// NO VTABLE: `??1Animation@...QEAA@XZ` is a Q, not a U, and the body stamps
// nothing. Graphics::Animation is plain data, like AnimationManager.
// ############################################################################
extern "C" void RtGraphics_Animation_Dtor(void* selfv) {
    uint8_t* const self = static_cast<uint8_t*>(selfv);

    rtpickgrid::ArrayDtorTransform(self + 0x60);          // 0x18001c100

    ReleaseString(reinterpret_cast<void**>(self + 0x20), /*checkZeroAfterAdjust=*/false);
    ReleaseString(reinterpret_cast<void**>(self + 0x18), /*checkZeroAfterAdjust=*/false);
    ReleaseString(reinterpret_cast<void**>(self + 0x10), /*checkZeroAfterAdjust=*/false);

    rtcoll::RtColl_ArrayDtor_18001c020(self);             // JMP 0x18001c020

}

// ############################################################################
// Graphics::EdgeMesh::~EdgeMesh
// ??1EdgeMesh@Graphics@@UEAA@XZ                 retail 0x1800a2780, ordinal 580
// ############################################################################
//
//   this->vtable = ??_7EdgeMesh@
//   ArrayDtor_180013e70(&_edgeLists)      +0x158
//   ArrayDtor_180013e70(&_triangleLists)  +0x148
//   IndexBuffer::PlatformDestruct (+0x128)   VertexBuffer::PlatformDestruct (+0x108)
//   IndexBuffer::PlatformDestruct (+0x0e8)   VertexBuffer::PlatformDestruct (+0x0c8)
//   IndexBuffer::PlatformDestruct (+0x0a8)   VertexBuffer::PlatformDestruct (+0x088)
//   IndexBuffer::PlatformDestruct (+0x068)   VertexBuffer::PlatformDestruct (+0x048)
//   tail ~External(this)
//
// DESCENDING ADDRESS ORDER, INDICES BEFORE VERTS within each pair — the exact
// reverse of the constructor's build order (Mesh.h's EdgeMesh banner).
// ############################################################################
//
// ENDGAME B.3.9 block GFX: the door is RETIRED with the RT_DELDTOR row
// 0x1800134b0 it served; its body is the real protected `~EdgeMesh()`. The
// restamp and the tail ~External are the compiler's chain.
Graphics::EdgeMesh::~EdgeMesh() {
    RtGraphics::EdgeMesh* const self =
        reinterpret_cast<RtGraphics::EdgeMesh*>(this);                 // B.3.9 bridge (the POD view)

    // 0x180013e70 — Array<T>::~Array for the two list arrays. An unexported
    // template instantiation, called by its body name.
    rtcoll::RtColl_ArrayDtor_180013e70(&self->_edgeLists);
    rtcoll::RtColl_ArrayDtor_180013e70(&self->_triangleLists);

    IndexBufferPlatformDestruct(&self->_extraIndices);
    VertexBufferPlatformDestruct(&self->_extraVerts);
    IndexBufferPlatformDestruct(&self->_capIndices);
    VertexBufferPlatformDestruct(&self->_capVerts);
    IndexBufferPlatformDestruct(&self->_edgeIndices);
    VertexBufferPlatformDestruct(&self->_edgeVerts);
    IndexBufferPlatformDestruct(&self->_triIndices);
    VertexBufferPlatformDestruct(&self->_triVerts);
}

// ############################################################################
// Graphics::SpotLight::CreateTargets
// ?CreateTargets@SpotLight@Graphics@@AEAAXXZ    retail 0x180106050, ordinal 2135
// ############################################################################
//
// Builds the shadow-map trio lights_vtable.cpp's SpotLight banner describes:
// +0x148 the colour image, +0x150 the depth image, +0x158 the render target.
//
//   p = ImageBuffer::operator new(0x68); this->_shadowMap = p ? (ImageBuffer(p), p) : 0
//   this->_shadowMap->vt[9](this->_shadowMap, 0, 0, 5, 0x40, 0x400, 0x400, 1,1,1,1, null)
//   p = ImageBuffer::operator new(0x68); this->_shadowDepth = p ? (ImageBuffer(p), p) : 0
//   this->_shadowDepth->vt[9](..., 0, 1, 7, 0x40, 0x400, 0x400, 1,1,1,1, null)
//   p = RenderBuffer::operator new(0x30)
//   if (p) { p[0]=p[1]=p[2]=0; RenderBuffer::PlatformConstruct(p); }
//   this->_shadowTarget = p
//   tail RenderBuffer::Create(p, this->_shadowMap, this->_shadowDepth)
//
// The eleven Create arguments are in RETAIL'S OWN ORDER, read from the stack
// stores at 0x1801060d5..0x180106112 — NOT assumed from the field order, which
// they do not follow (the trap imagebuffer_derived.cpp documents for
// ReflectionImageBuffer::Serialize). `usage` 1 + `format` 7 on the second call
// is what makes it the DEPTH buffer; 0x400 x 0x400 is the 1024² shadow map.
//
// THE CTOR'S RETURN VALUE IS NOT USED. Retail stores the ctor's RAX
// (`MOV RCX,RAX ; MOV [RSI+0x148],RCX`), and the ctor returns `this`, so the
// stored pointer is the allocation either way — spelled with the allocation
// here for the reason wave23_lane_gfxbody.cpp gives.
//
// AN UNGUARDED DEREF, KEPT: when `operator new` returns null the field is set
// to null and `MOV RAX,[RCX]` at 0x1801060d2 immediately reads through it.
// Retail faults there; so does this.
// ############################################################################
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::SpotLight::CreateTargets` (block E's class; this file is not block
// E's, which is why Light.h says "its body stays where it is"). The door keeps its
// name and both its registrations.
extern "C" void RtGraphics_SpotLight_CreateTargets(void* selfv) {
    reinterpret_cast<Graphics::SpotLight*>(selfv)                        // B.3.5 bridge
        ->Graphics::SpotLight::CreateTargets();
}

// ############################################################################
// Graphics::GraphicsMesh::PlatformSerialize
// ?PlatformSerialize@GraphicsMesh@Graphics@@QEAAXAEAVSerializer@IO@@@Z
// retail 0x1800a48e0, ordinal 3386
// ############################################################################
//
// TWO device-interface arms over one stream, the shape
// runtime/src/video/shaderprogram.cpp spells out. Arm 0 runs when
// `Video::Device::_singleton->activeInterface == 0`; arm 1 runs when it is 1 or
// 2. Each builds its OWN ReaderWriter; the arm whose interface is not active
// calls `Skip()` so the stream still advances past that backend's blob.
//
// The detail level is the 0xb0-stride record Mesh.h's GraphicsMeshView pins
// (data at mesh+0x50, count at mesh+0x58). Within it:
//   +0x20 / +0x40 / +0x60 / +0x80  four Video buffer objects, each serialized
//                                  through ITS OWN vtable slot +0x30, and in
//                                  the order 0x40, 0x20, 0x80, 0x60 — NOT
//                                  ascending, and not a transcription slip
//                                  (the same crossed order EdgeMesh::SetMaterials
//                                  has, Mesh.h's note).
//   +0x98 / +0xa0                  Collection::Array<Triangle>, 0x18 stride
//   +0xa8                          a trailing int
//
// THE VERSION GATE READS THE **OUTER** READER/WRITER, not the arm's own
// (0x1800a4961: `MOV RCX,[R15+8]`). When it says >= 1 each arm emits one extra
// int whose value is DISCARDED on read — a local initialised to zero.
// ############################################################################
namespace {

constexpr uint32_t kLvlStride   = 0xb0;
constexpr uint32_t kLvlBuf20    = 0x20;
constexpr uint32_t kLvlBuf40    = 0x40;
constexpr uint32_t kLvlBuf60    = 0x60;
constexpr uint32_t kLvlBuf80    = 0x80;
constexpr uint32_t kLvlTriData  = 0x98;
constexpr uint32_t kLvlTriCount = 0xa0;
constexpr uint32_t kLvlTail     = 0xa8;
static_assert(kLvlTail + 4 <= kLvlStride, "detail-level fields must fit the 0xb0 stride");
static_assert(kLvlTriCount == kLvlTriData + 8,
              "the triangle Array's _size must follow its _data, Collection::Array shape");

using BufSerFn   = void (*)(void*, Serializer*);

// B3X W0: the ReaderWriter members, by name on the real class (the
// `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
// interchangeable in Serializer.h). `name` is nullptr, as retail passes.
IO::ReaderWriter* AsRw(ReaderWriter* rw) {
    return reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}
void RwInt(ReaderWriter* rw, void* p) {
    AsRw(rw)->SerializeInt(*static_cast<int32_t*>(p), nullptr);
}
int RwVersion(ReaderWriter* rw) {
    return AsRw(rw)->GetVersion();
}
void DeleteRw(ReaderWriter* rw) {
    if (rw == nullptr) return;
    delete AsRw(rw);
}
bool IsReading(const Stream* s) { return s->direction == rt::io::kDirectionReading; }

ReaderWriter* MakeReaderWriter(Serializer* ser) {
    Stream* stream = ser->stream;
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = IsReading(stream) ? rt::io::encoding_vtbl::kCreateReader
                                       : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

// `?_singleton@Device@Video@@0PEAV12@EA` @ 0x1801fe7a8; Device+0xF8 is
// `activeInterface` (runtime/types/video/Device.h). ENDGAME B.3.9 SS1
// (B39_RULES D7): the class static now -- the null test this used to carry
// was, by its own comment, justified only because RT_DATA_SLOT could hand
// back an unfilled mint slot ("retail's own static could not be null"); that
// reason is gone, so the guard goes with it and retail's unguarded deref is
// restored.
int32_t ActiveInterface() {
    void* device = ::Video::Device::_singleton;
    return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0xF8);
}

// ENDGAME B.3.5 block G: it now takes the OBJECT the caller loaded instead of a
// (base, byte offset) pair, because `GraphicsMesh::DetailLevel` names all four
// `_deviceData` qwords. The dereference is still unguarded, which is faithful.
void SerializeBufferSlot(void* obj, Serializer* sub) {
    void** vt = *reinterpret_cast<void***>(obj);            // faithful: unguarded
    reinterpret_cast<BufSerFn>(vt[0x30 / 8])(obj, sub);
}

}  // namespace

// ############################################################################
// ENDGAME B.3.5 step 5, block G - the body MOVED onto
// `Graphics::GraphicsMesh::PlatformSerialize`, and every offset it walked by
// hand is now a named field of `GraphicsMesh` / `GraphicsMesh::DetailLevel`
// (`runtime/types/graphics/GraphicsMesh.h`, authored by this block). The eleven
// `kLvl*` constants this file derived independently AGREE with that header field
// for field - stride 0xb0, the Array<TriangleList> at +0x98/+0xa0, the four
// buffer `_deviceData` qwords at +0x20/+0x40/+0x60/+0x80 and the tail int at
// +0xa8 - which is the third independent reading of the 0xb0 record.
//
// *** RULE B5-11 LIVES ON THIS FUNCTION. *** Its registration was once stripped
// from the PERF build because the `RtGraphicsBatchCLeavesRegister()` call that
// made it sat inside a `printf` argument, whose whole expression
// `standalone/log_strip.h` compiles to `((void)0)`; boot then died on a
// VCALL-of-0 (defect 3, fixed `1a4200c3`). Nothing below introduces such a call,
// and the perf build is a gate item for this commit.
// ############################################################################
namespace Graphics {

// ---------------------------------------------------------------------------
// ?GenerateTextures@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z
// retail 0x180102eb0 — ENDGAME B.3.5 block GC. The evidence, the two faithful
// uglinesses and the oracle note are in the door's banner above.
//
// TWO RAW BINDINGS BECAME TYPED ONES (item 6), exactly as in the sibling
// `RenderTextures`: the `textures._data + i*0x10` then `+0x08` walk is
// `_textures._data[i].image`, and the hand-inlined `(*(void***)obj)[0x50/8]` is
// slot TEN of `Graphics::GeneratedImageBuffer` — `Generate(context, generation)`
// — still dispatched VIRTUALLY, so an object wearing the minted table is reached
// the same way it was. 0x50/8 == 10 is checked against
// GeneratedImageBuffer.h's slot list.
//
// NOTE THE ASYMMETRY WITH ITS SIBLING, WHICH IS RETAIL'S: `RenderTextures` tests
// `_textures._size <= 0` before the loop and this one does NOT — it goes
// straight from the `+0x24` test into the loop, whose own bound then ends it.
// ---------------------------------------------------------------------------
void MaterialInstance::GenerateTextures(Video::Context& context, int32_t arg) const {
    if (_hasRenderTexture == 0) {
        return;
    }

    for (int32_t i = 0; i < _textures._size; ++i) {
        void* obj = _textures._data[i].image;

        if (*reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(obj) + 0x2c) != 4)
            continue;
        if (obj == nullptr)                      // retail's order: after the deref
            continue;

        // Slot 10 (0x50/8) — a virtual call on the real class.
        reinterpret_cast<const GeneratedImageBuffer*>(obj)           // B.3.5 bridge
            ->Generate(&context, arg);
    }
}

void GraphicsMesh::PlatformSerialize(IO::Serializer& serializer) {
    // The ported bodies in this file speak the `rt::io::Serializer` POD; the
    // member takes retail's own `IO::Serializer&` so the header names the retail
    // signature. The two describe the same bytes.
    Serializer* const outer = reinterpret_cast<Serializer*>(&serializer);  // B.3.5 bridge

    // `if (!stream->encoding->vtable[0](encoding)) return;` — the encoding
    // policy object's "does this stream carry platform data at all" slot.
    {
        void** encVt = *reinterpret_cast<void***>(outer->stream->encoding);
        using CarriesPlatformDataFn = bool (*)(void*);
        if (!reinterpret_cast<CarriesPlatformDataFn>(encVt[0])(outer->stream->encoding)) {
            return;
        }
    }

    const int32_t levelCount = _detailLevels._size;                  // +0x58

    for (int arm = 0; arm < 2; ++arm) {
        Serializer sub = { outer->stream, nullptr };
        ReaderWriter* rw = MakeReaderWriter(&sub);

        const int32_t iface = ActiveInterface();
        const bool active = (arm == 0) ? (iface == 0) : (iface == 1 || iface == 2);
        if (!active) {
            AsRw(rw)->Skip();
            DeleteRw(rw);
            continue;
        }

        // The gate reads the OUTER reader/writer — see the banner.
        if (RwVersion(outer->readerWriter) >= 1) {
            int32_t discarded = 0;
            RwInt(rw, &discarded);
        }

        for (int32_t i = 0; i < levelCount; ++i) {
            // `this->[0x50]` is RE-READ every iteration (0x1800a49a0 /
            // 0x1800a4b30), so a slot-0x30 handler that reallocates the level
            // array is seen. Not hoisted.
            DetailLevel* const level =
                reinterpret_cast<DetailLevel*>(_detailLevels._data) + i;

            int32_t n = level->_triangleLists._size;                  // +0xa0
            RwInt(rw, &n);
            if (IsReading(sub.stream))
                rtcoll::RtColl_ArraySetSize_1800136f0(&level->_triangleLists, n);  // +0x98

            void* triData = level->_triangleLists._data;
            if (arm == 0) {
                // 0x180014850 SerializeTriangleListArray(ctx, count, data) —
                // PORTED (closure_batch2b.cpp) and registered by address.
                RtGraphics_EdgeMesh_SerializeTriangleListArray(&sub, n, triData);   // 0x180014850
            } else {
                // The same six dwords per element, emitted inline by retail on
                // this arm (0x1800a4b90..0x1800a4c0e, stride 0x18).
                uint8_t* e = static_cast<uint8_t*>(triData);
                for (int32_t j = 0; j < n; ++j, e += 0x18) {
                    RwInt(rw, e + 0x00);
                    RwInt(rw, e + 0x04);
                    RwInt(rw, e + 0x08);
                    RwInt(rw, e + 0x0c);
                    RwInt(rw, e + 0x10);
                    RwInt(rw, e + 0x14);
                }
            }

            // The four buffers, INDEX BEFORE VERTEX in each pair, retail's
            // order (+0x40, +0x20, +0x80, +0x60 = each header's +0x08).
            DetailLevel* const lvl2 =
                reinterpret_cast<DetailLevel*>(_detailLevels._data) + i;
            SerializeBufferSlot(lvl2->_indexBuffer._deviceData,      &sub);
            SerializeBufferSlot(lvl2->_vertexBuffer._deviceData,     &sub);
            SerializeBufferSlot(lvl2->_skinIndexBuffer._deviceData,  &sub);
            SerializeBufferSlot(lvl2->_skinVertexBuffer._deviceData, &sub);

            DetailLevel* const lvl3 =
                reinterpret_cast<DetailLevel*>(_detailLevels._data) + i;
            RwInt(rw, &lvl3->_maxInstances);                          // +0xa8
        }

        DeleteRw(rw);
    }
}

// ---------------------------------------------------------------------------
// ?CreateTargets@SpotLight@Graphics@@AEAAXXZ  0x180106050 -- block GC.
// `AEAA` is PRIVATE and this DLL exports its private members; the member is
// PUBLIC in Light.h for the reason block G recorded on MaterialInstance's two
// private finders (an `extern "C"` door cannot be a friend).
//
// The four SpotLight target pointers are still reached at their raw offsets
// through one bridge local: Light.h names them, but every store here is an
// inlined `operator new` plus an unguarded vtable slot-9 `Create`, and the
// arithmetic is what the banner above documents.
// ---------------------------------------------------------------------------
void SpotLight::CreateTargets() {
    uint8_t* const self = reinterpret_cast<uint8_t*>(this);   // B.3.5 bridge

    // ---- +0x148, the shadow colour image ----------------------------------
    {
        void* p = ImageBufferOperatorNew(0x68);
        if (p != nullptr) ImageBufferCtor(p);
        *reinterpret_cast<void**>(self + 0x148) = p;
        // Slot 9 `Create`, by name (B3X W0); faithful: unguarded.
        Video::ImageBuffer* const img =
            *reinterpret_cast<Video::ImageBuffer**>(self + 0x148);   // B.3.x bridge
        img->Create(0, 0, 5,
                    0x40u, 0x400, 0x400,
                    1, 1, 1, 1, nullptr);
    }

    // ---- +0x150, the shadow depth image ------------------------------------
    {
        void* p = ImageBufferOperatorNew(0x68);
        if (p != nullptr) ImageBufferCtor(p);
        *reinterpret_cast<void**>(self + 0x150) = p;
        Video::ImageBuffer* const img =
            *reinterpret_cast<Video::ImageBuffer**>(self + 0x150);   // B.3.x bridge
        img->Create(0, 1, 7,
                    0x40u, 0x400, 0x400,
                    1, 1, 1, 1, nullptr);
    }

    // ---- +0x158, the render target -----------------------------------------
    void* rb = RenderBufferOperatorNew(0x30);
    if (rb != nullptr) {
        reinterpret_cast<uint64_t*>(rb)[0] = 0;
        reinterpret_cast<uint64_t*>(rb)[1] = 0;
        reinterpret_cast<uint64_t*>(rb)[2] = 0;
        RenderBufferPlatformConstruct(rb);
    }
    *reinterpret_cast<void**>(self + 0x158) = rb;

    RenderBufferCreate(rb,
                       *reinterpret_cast<void**>(self + 0x148),
                       *reinterpret_cast<void**>(self + 0x150));   // JMP 0x1800f3f10
}

// ---------------------------------------------------------------------------
// ?DrawItemList@Renderer@Graphics@@QEBAXAEAVContext@Video@@HHH@Z 0x180105240
// ENDGAME B.3.5 block GC. The five-value argument list, the two stack-built 4x4
// matrices and the instancing batch are documented in the banner above.//
// THE POD SPELLING OF THE *ELEMENT* RECORDS IS KEPT, DELIBERATELY. `this` is the
// real `Graphics::Renderer`; the draw-group / scene-entry / draw-item records it
// indexes are still spelled `RtGraphics::*` through one bridge local, because
// those PODs have live users in files outside this block and B4b-1 keeps a POD
// until its LAST user converts. Camera.h cross-asserts every one of their offsets
// against the real classes', so the two descriptions cannot drift, and renaming
// ~200 field accesses in a body this size is a second change, not this one.
// ---------------------------------------------------------------------------
void Renderer::DrawItemList(Video::Context& context, int groupIndex, int listIndex,
                            int shaderIndex) const {
    const RtGraphics::Renderer* const self =
        reinterpret_cast<const RtGraphics::Renderer*>(this);   // B.3.5 bridge
    void* const ctx = &context;
    uint8_t* const groupBase = reinterpret_cast<uint8_t*>(self->drawGroups._data);
    const int64_t groupOffset = static_cast<int64_t>(groupIndex) * sizeof(RtGraphics::RendererDrawGroup);
    const RtGraphics::Camera* const cam =
        reinterpret_cast<const RtGraphics::Camera*>(groupBase + groupOffset +
                                        offsetof(RtGraphics::RendererDrawGroup, cullCamera));

    // ---- the two matrices, built on the stack ------------------------------
    // view = the cull camera's Transform3 expanded to a 4x4 with an explicit
    // (0,0,0,1) fourth column: three zeroed floats and the 1.0f at 0x18016169c.
    float view[16];
    {
        const float* t = reinterpret_cast<const float*>(cam) + (0x64 / 4);
        view[0] = t[0];  view[1] = t[1];  view[2]  = t[2];  view[3]  = 0.0f;
        view[4] = t[3];  view[5] = t[4];  view[6]  = t[5];  view[7]  = 0.0f;
        view[8] = t[6];  view[9] = t[7];  view[10] = t[8];  view[11] = 0.0f;
        view[12] = t[9]; view[13] = t[10]; view[14] = t[11];
        view[15] = AsFloat(kBits_one);
    }
    // proj = the camera's projection matrix, sixteen dwords copied verbatim.
    uint32_t proj[16];
    std::memcpy(proj, reinterpret_cast<const uint8_t*>(cam) + 0x94, sizeof proj);

    ContextSetCamera(ctx, view, proj);

    // ---- the item list ------------------------------------------------------
    const Collection::Array<RtGraphics::RendererDrawItem>* const items =
        reinterpret_cast<const Collection::Array<RtGraphics::RendererDrawItem>*>(
            groupBase + groupOffset + offsetof(RtGraphics::RendererDrawGroup, items) +
            static_cast<int64_t>(listIndex) * 0x10);

    const int32_t count = items->_size;
    if (count <= 0) {
        return;
    }

    int32_t emitted = 0;
    int32_t drawn = 0;
    int64_t i = 0;
    while (i < count) {
        const uint8_t* const itemBase = reinterpret_cast<const uint8_t*>(items->_data);
        int64_t itemOff = i * 0x38;
        const RtGraphics::RendererDrawItem* const item =
            reinterpret_cast<const RtGraphics::RendererDrawItem*>(itemBase + itemOff);

        int32_t step = 1;
        int32_t batchCount = 1;

        // `item->material` is dereferenced BEFORE the shader-index tests
        // (0x1801054f5 vs 0x1801054f9). Faithful.
        const uint8_t* const material = static_cast<const uint8_t*>(item->material);
        bool draw = false;

        if (shaderIndex >= 0 &&
            shaderIndex < *reinterpret_cast<const int32_t*>(material + 0x30)) {
            const uint8_t* const* const programs =
                *reinterpret_cast<const uint8_t* const* const*>(material + 0x28);
            const uint8_t* const prog = programs[shaderIndex];

            const uint8_t* const sub28 = *reinterpret_cast<const uint8_t* const*>(prog + 0x28);
            const uint8_t* const sub30 = *reinterpret_cast<const uint8_t* const*>(prog + 0x30);
            if (*reinterpret_cast<const void* const*>(prog + 0x20) != nullptr &&
                sub28 != nullptr && *reinterpret_cast<const int32_t*>(sub28 + 0x48) > 0 &&
                sub30 != nullptr && *reinterpret_cast<const int32_t*>(sub30 + 0x48) > 0) {
                draw = true;
            }
        }

        if (draw) {
            // shader 0 only, and only when the instance has a render texture:
            // `CMP byte [instance+0x24], R15B` with R15B known zero.
            if (shaderIndex == 0 &&
                *(static_cast<const uint8_t*>(item->instance) + 0x24) != 0) {
                RtGraphics_MaterialInstance_GenerateTextures(item->instance, ctx,
                                                             self->contextSerial);
            }
            MaterialInstanceSetup(item->instance, ctx, shaderIndex);

            if (item->arg8 > 1) {
                // ---- the instancing batch ---------------------------------
                // Geometry slot 7 `SetSharedInstanceData`, by name (B3X W0).
                const Geometry* const geom0 =
                    static_cast<const Geometry*>(item->geometry);   // B.3.x bridge
                int32_t slotIndex = geom0->SetSharedInstanceData(
                    context, item->arg5, self->frameAlpha);
                const int32_t base = emitted - slotIndex;

                batchCount = 0;
                for (;;) {
                    const uint8_t* const cur =
                        reinterpret_cast<const uint8_t*>(items->_data) + itemOff;
                    ++batchCount;
                    itemOff += 0x38;

                    // Geometry slot 6 `GetInstanceTransform`, by name (B3X W0).
                    // Its hidden return slot is `view` -- the stack matrix is
                    // the storage, exactly as the raw call passed it.
                    const Geometry* const g =
                        *reinterpret_cast<const Geometry* const*>(cur);   // B.3.x bridge
                    const uint16_t sub = *reinterpret_cast<const uint16_t*>(cur + 0x1a);
                    const void* m = g->GetInstanceTransform(
                        reinterpret_cast<Math::Matrix*>(view), sub, self->frameAlpha);
                    ContextSetTransform(ctx, slotIndex, m);

                    ++slotIndex;
                    if (base + slotIndex >= count) break;
                    if (batchCount >= item->arg8) break;

                    const uint8_t* const nxt =
                        reinterpret_cast<const uint8_t*>(items->_data) + itemOff;
                    if (*reinterpret_cast<const void* const*>(nxt + 0x10) != item->instance) break;
                    if (*reinterpret_cast<const uint16_t*>(nxt + 0x1a) != item->arg5) break;
                    if (*reinterpret_cast<const void* const*>(nxt + 0x28) != item->userData) break;
                    if (*reinterpret_cast<const uint32_t*>(nxt + 0x30) != item->arg10) break;
                }
                // ESI and R15 are the SAME count in retail (both bumped once
                // per consumed item at 0x1801055c4/0x1801055c6), and both are
                // what the outer counters advance by at 0x1801056b4/0x1801056bd.
                step = batchCount;
            } else {
                // 0x18010569c: a non-instanced item is one item, one instance.
                batchCount = 1;
                step = 1;
            }

            // Geometry slot 2 `Draw`, by name (B3X W0). The material handle is
            // the 8-byte `ExternalPtr` passed by value in its register, as the
            // raw call passed the pointer.
            const Geometry* const geom =
                static_cast<const Geometry*>(item->geometry);   // B.3.x bridge
            geom->Draw(
                context,
                *reinterpret_cast<const ::Graphics::Camera*>(                // B.3.x bridge
                    groupBase + groupOffset + offsetof(RtGraphics::RendererDrawGroup, cullCamera)),
                ExternalPtrHandle{ const_cast<void*>(item->instance) },
                self->frameAlpha,
                static_cast<int32_t>(item->arg5),
                shaderIndex,
                batchCount);
            ++drawn;
        }

        emitted += step;
        i += step;
    }
}

}  // namespace Graphics

// The registered door: name, address and retail argument order unchanged.
extern "C" void RtGraphics_GraphicsMesh_PlatformSerialize(void* selfv, void* serv) {
    reinterpret_cast<Graphics::GraphicsMesh*>(selfv)                 // B.3.5 bridge
        ->Graphics::GraphicsMesh::PlatformSerialize(
            *reinterpret_cast<IO::Serializer*>(serv));               // B.3.5 bridge
}

// ############################################################################
// Graphics::Renderer::DrawItemList
// ?DrawItemList@Renderer@Graphics@@QEBAXAEAVContext@Video@@HHH@Z
// retail 0x180105240, ordinal 2279
// ############################################################################
//
// FIVE VALUES: (this, Context&, int group, int list, int shader). The fifth
// travels in its own home slot at [RBP+0xc0] and is the SHADER INDEX — see
// point 1 of this file's banner, and lights_vtable.cpp's own note about the
// three Draw arms that write their arg5 home slot before tail-jumping here.
//
// SHAPE
//   group = this->_drawGroups._data + group*0x1f0
//   SetCamera(ctx, Matrix(group->cullCamera.cameraToWorld), group->cullCamera.projection)
//   items = group->items[list]                  ; group + 0x1c0 + list*0x10
//   for (i = 0; i < items._size; ) {
//       item = items._data + i*0x38
//       step = 1
//       if (shader >= 0 && shader < item->material->_programCount) {
//           prog = item->material->_programs[shader]
//           if (prog->[0x20] && prog->[0x28] && prog->[0x28]->[0x48] > 0
//                            && prog->[0x30] && prog->[0x30]->[0x48] > 0) {
//               if (shader == 0 && item->instance->hasRenderTexture)
//                   GenerateTextures(item->instance, ctx, this->contextSerial)
//               MaterialInstance::Setup(item->instance, ctx, shader)
//               if (item->arg8 > 1) { ...the instancing batch, below... }
//               item->geometry->vt[2](geometry, ctx, &group->cullCamera,
//                                     item->instance, this->frameAlpha,
//                                     item->arg5, shader, batchCount)
//           }
//       }
//       emitted += step; i += step
//   }
//
// THE FIVE SILENT DROPS are AddDrawItem's own list (wave9_lane_gfx.cpp) read
// from the other end: a negative or out-of-range shader index, and a selected
// VideoProgram with a null +0x20, a null-or-empty +0x28, or a null-or-empty
// +0x30. On every one of them `step` stays 1 and nothing is drawn.
//
// THE INSTANCING BATCH (`item->arg8 > 1`) is the part no decompile shows
// honestly. It calls the geometry's slot 7 to reserve N instance transform
// slots, then walks forward over CONSECUTIVE items that agree on all four of
// (instance, arg5, userData, arg10), asking each one's slot 6 for a Matrix and
// pushing it with Context::SetTransform. It stops on the first mismatch, on
// `item->arg8` items, or when the running index would pass the list's size.
// `step` is then the number of items consumed, so the outer loop skips the
// whole batch and the single Draw below carries them all.
//
// ORACLE: NONE, and that is stated rather than faked. Every arm of this body
// ends in a real GPU draw through the geometry's vtable; running the shipped
// copy afterwards would submit every draw a second time. the diff oracle records
// how many calls reached a Draw at all, which is the number that matters — a
// green `calls=` column on a body that dropped every item would mean nothing.
// ############################################################################
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::Renderer::DrawItemList` (block F's class, not block F's file). The
// door keeps its name and both its registrations.
extern "C" void RtGraphics_Renderer_DrawItemList(const void* selfv, void* ctx,
                                                 int32_t groupIndex, int32_t listIndex,
                                                 int32_t shaderIndex) {
    reinterpret_cast<const Graphics::Renderer*>(selfv)                   // B.3.5 bridge
        ->Graphics::Renderer::DrawItemList(
            *reinterpret_cast<Video::Context*>(ctx),                     // B.3.5 bridge
            groupIndex, listIndex, shaderIndex);
}

// ============================================================================
// Registration — BOTH DOORS for every one of the ten, unconditionally
// (DIRECTION.md §2.3).
//
// THE ADDRESS DOORS WERE CHECKED BEFORE THEY WERE WRITTEN. Every one of the ten
// addresses was grepped across code/, engine/, runtime/, standalone/ and dx11/.
// None is used as an IDENTITY TOKEN the way standalone/scene_watch.cpp compares
// `?AddDrawItems@MeshInstance@`'s address against a live vtable slot
// (wave21_lane_gfxbody.cpp:311), so registering all ten is safe. Three of them
// (0x180103c70, 0x1800a51c0, 0x1800a5520, 0x1800a2780) appear as RT_DELDTOR
// rows in runtime/src/object/deleting_dtor.cpp, which reaches them through
// ResolveAddr — registry-first — so those rows now land on the bodies above,
// which is the intent.
// ============================================================================
extern "C" int RtGraphicsBatchCLeavesRegister() {
    int n = 0;

    // (ENDGAME B.3.9 GFX: the ??1ParticleSystemInstance door is retired.)

    RegisterRuntimeExport("??0Animation@Graphics@@QEAA@AEBV01@@Z",
                          (void*)&RtGraphics_Animation_CopyCtor); ++n;
    RegisterRuntimeAddr(0x1800160a0ull, (void*)&RtGraphics_Animation_CopyCtor);

    RegisterRuntimeExport("??1Animation@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_Animation_Dtor); ++n;
    RegisterRuntimeAddr(0x1800fb290ull, (void*)&RtGraphics_Animation_Dtor);

    // (ENDGAME B.3.9 GFX: the ??1EdgeMesh door is retired.)

    return n;
}
