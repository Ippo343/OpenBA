// ============================================================================
// runtime/src/graphics/imagebuffer_derived.cpp
// The three Graphics classes derived from Video::ImageBuffer, vtable batch 6.
// ============================================================================
//
//   ??_7GeneratedImageBuffer@Graphics@@6B@   0x180154f60   5 slots + ctor/dtor
//   ??_7FrameCopyImageBuffer@Graphics@@6B@   0x180154f00   9 slots
//   ??_7ReflectionImageBuffer@Graphics@@6B@  0x180155190  10 slots + ctor/dtor
//
// WHAT IS *NOT* HERE, deliberately. `Video::ImageBuffer`'s own ten slots are
// runtime/src/video/imagebuffer_vtable.cpp's (Wave 18 lane VDATA), and
// `FrameCopyImageBuffer`'s ctor / dtor / Create / Generate are
// runtime/src/graphics/wave23_lane_gfxbody.cpp's. Both are REUSED through the
// registry door, never restated: every base-class arm below goes out through
// `RT_EXPORT_ONCE` on the mangled name, which is registry-first, so it lands on
// our port where one exists and on the DLL where one does not.
//
// THE FAMILY'S SHAPE, and why three classes share one file. Slots 6/7/8
// (Reconstruct / Destruct / Construct) are the `Object::External` device-lost
// protocol and are *structurally identical* in all three, differing only in
// which constructor they reach:
//
//   Destruct     MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]      ; slot 0, flags=0
//   Construct    TEST RCX,RCX; JNZ <ctor>; XOR EAX,EAX; RET ; placement new
//   Reconstruct  slot 0 with flags=0, then tail-jump <ctor>
//
// The `XOR EDX,EDX` in Destruct and Reconstruct is the argument Ghidra's C
// output DROPS (reference_ghidra_c_output_vs_disassembly.md): flags=0 means
// "destroy, do NOT free". Passing 1 would free an object the caller still owns.
// Every body below was read from the listing at its retail VA, not from the
// decompiler.
//
// GeneratedImageBuffer does NOT have slots 6/7/8 of its own — it inherits
// Video::ImageBuffer's, which is why its row in the batch is five slots and not
// eight. Its `??_7` slots 6/7/8 hold 0x18001ea80 / 0x18001eab0 / 0x18001eaa0,
// i.e. the base class's bodies, already ported and already registered.
//
// This file also carries ONE non-slot helper, `Math::Transform3::operator*`
// (0x180022680), because `ReflectionImageBuffer::Render` cannot run without it
// and nothing in the tree ported it. See its own banner.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/math/Math.h"
#include "../../types/graphics/FrameCopyImageBuffer.h"
#include "../../types/graphics/GeneratedImageBuffer.h"
#include "../../types/io/Serializer.h"
#include "../collection/array_internal.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
using RtGraphics::GeneratedImageBuffer;
using RtGraphics::ReflectionImageBuffer;

// Declared here rather than pulled in from standalone/resolve.h: that header
// drags in <windows.h>, and PLAN §4.1 requires runtime/src/ to stay
// platform-neutral. Must sit at FILE scope and stay byte-identical to
// resolve.h's declaration; the linker checks.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// The BaseTypeInfo nodes slots 1 and 2 read.
// ---------------------------------------------------------------------------
// Same two fields, same reasoning, as graphics_typeinfo_accessors.cpp:
//     +0x00 uint32_t typeId   (MOV EAX,[node])
//     +0x08 const char* name  (MOV RAX,[node+8])
// Reached through RT_DATA_SLOT and NOT by address, because all four of these
// `?_typeInfo@` rows are MINTED and PROMOTED (standalone/reg_mint_promoted.cpp
// :936-992). By address we would read the DLL's copy while the rest of the tree
// reads the minted one — the split-brain shape mainloop_video.cpp:303 warns of.
const uint8_t* TypeInfoGenerated() {
    return RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* TypeInfoFrameCopy() {
    return RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@FrameCopyImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* TypeInfoReflection() {
    return RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@ReflectionImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A");
}

uint32_t ReadTypeId(const uint8_t* node) {
    uint32_t v = 0; std::memcpy(&v, node, sizeof v); return v;
}
const char* ReadTypeName(const uint8_t* node) {
    const char* v = nullptr; std::memcpy(&v, node + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// The named dependencies. RT_EXPORT_ONCE, not the raw DLL door: these calls
// ARE the work, so they must reach our ports where ports exist.
// ---------------------------------------------------------------------------
using VoidThisFn = void  (*)(void*);
using PtrThisFn  = void* (*)(void*);
using NewFn      = void* (*)(uint64_t);
using SerializeFn = bool (*)(void*, Stream*);
using RbCreateFn = bool  (*)(void*, const void*, const void*);

// `??0ImageBuffer@Video@@QEAA@XZ` 0x1800f2580 — PORTED (video/buffers.cpp).
// Declared returning void because that is how buffers.cpp spells it; the
// callers below therefore never read its return and use the pointer they
// already hold (wave23_lane_gfxbody.cpp's rule, and
// reference_hidden_retslot_wild_write.md's family).
void ImageBufferCtor(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??0ImageBuffer@Video@@QEAA@XZ")(p);
}
void ImageBufferDtor(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??1ImageBuffer@Video@@UEAA@XZ")(p);
}
bool ImageBufferSerialize(void* self, Stream* s) {
    return RT_EXPORT_ONCE(SerializeFn,
        "?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z")(self, s);
}
void* ImageBufferOperatorNew(uint64_t n) {
    return RT_EXPORT_ONCE(NewFn, "??2ImageBuffer@Video@@SAPEAX_K@Z")(n);
}
void* RenderBufferOperatorNew(uint64_t n) {
    return RT_EXPORT_ONCE(NewFn, "??2RenderBuffer@Video@@SAPEAX_K@Z")(n);
}
void RenderBufferPlatformConstruct(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ")(p);
}
void RenderBufferPlatformDestruct(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ")(p);
}
void RenderBufferOperatorDelete(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??3RenderBuffer@Video@@SAXPEAX@Z")(p);
}
bool RenderBufferCreate(void* rb, const void* colour, const void* depth) {
    return RT_EXPORT_ONCE(RbCreateFn,
        "?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z")(rb, colour, depth);
}

// `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` 0x1800a3d10 — PORTED
// (wave23_lane_gfxbody.cpp, registered BY NAME ONLY; there is no address door
// for it anywhere in the tree, which is why this is a name call and not a
// ResolveAddr one).
void* FrameCopyCtor(void* p) {
    return RT_EXPORT_ONCE(PtrThisFn, "??0FrameCopyImageBuffer@Graphics@@QEAA@XZ")(p);
}

// The three vtables these bodies stamp, rebased. `ResolveAddr` on a DATA
// address is the rebase plus a registry lookup, so if a later change MINTS one
// of these rows the stamp follows the minted table — which is the right answer
// for a promoted row and the reason this is not a raw `base + 0x...`.
void* VtGenerated() {
    static void* v = nullptr;
    if (!v) v = reinterpret_cast<void*>(
        ResolveAddr(Module::Runtime, RtGraphics::kVtGeneratedImageBuffer));
    return v;
}
void* VtReflection() {
    static void* v = nullptr;
    if (!v) v = reinterpret_cast<void*>(
        ResolveAddr(Module::Runtime, RtGraphics::kVtReflectionImageBuffer));
    return v;
}

// ---------------------------------------------------------------------------
// The Serializer prologue/epilogue, identical to imagebuffer_vtable.cpp's
// `MakeCodec` / `DestroyCodec`, which in turn are identical to
// achievement/goal_serialize.cpp's. Four lines each; not shared through a
// header because both existing copies keep them file-local and a header per
// free function is a header per function (external_set.cpp's rule).
//
// The third argument of the factory is the save-format VERSION and it is ZERO:
// retail zeroes R8D before the `CMP [RDX+8],R8D` and never reloads it
// (0x1800a6f3d / 0x1800a6f4d in ReflectionImageBuffer::Serialize).
// ---------------------------------------------------------------------------
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

// `+0x28` is `kArrayFloat`, `(n, float*, name)` — runtime/types/io/Serializer.h,
// RETAIL offsets. `EDX = 4` and `R8 = this+0x80` at 0x1800a6f6c/0x1800a6f62.
void EmitFloatArray(ReaderWriter* rw, int n, float* p) {
    using Fn = void (*)(ReaderWriter*, int, float*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8])(rw, n, p, nullptr);
}

// The object's OWN vtable slot 0, the vector deleting destructor, with the
// flags byte spelled out at each call site. Read through the object rather than
// through a resolved symbol because a derived class must reach ITS OWN slot 0.
void DeleteThrough(void* obj, unsigned flags) {
    void** vt = *reinterpret_cast<void***>(obj);
    reinterpret_cast<void* (*)(void*, unsigned)>(vt[0])(obj, flags);
}

}  // namespace

// ############################################################################
// Graphics::GeneratedImageBuffer
// ############################################################################

// ============================================================================
// ??0GeneratedImageBuffer@Graphics@@QEAA@XZ          retail 0x1800a3e70
//
//     CALL 0x1800f2580              ; ??0ImageBuffer@Video@@
//     LEA RAX,[0x180154f60]         ; ??_7GeneratedImageBuffer@Graphics@@6B@
//     MOV [RBX],RAX
//     MOV dword [RBX+0x68],0
//     MOV RAX,RBX                   ; returns `this`
// ============================================================================
extern "C" void* RtGraphics_GeneratedImageBuffer_Ctor(void* self) {
    GeneratedImageBuffer* const p = static_cast<GeneratedImageBuffer*>(self);
    ImageBufferCtor(p);
    p->base.vtable = VtGenerated();
    p->_generation = 0;
    return self;
}

// ============================================================================
// ??1GeneratedImageBuffer@Graphics@@UEAA@XZ          retail 0x1800a3e40
//
//     LEA RAX,[0x180154f60] ; MOV [RCX],RAX ; JMP 0x1800f2610
//
// Restamp and hand back to the base. Nothing of its own to release: +0x68 is a
// plain int.
// ============================================================================
extern "C" void RtGraphics_GeneratedImageBuffer_Dtor(void* self) {
    GeneratedImageBuffer* const p = static_cast<GeneratedImageBuffer*>(self);
    p->base.vtable = VtGenerated();
    ImageBufferDtor(p);                          // tail JMP ??1ImageBuffer
}

// slot 2 — `MOV EAX,dword ptr [0x1801f9a20]` (= ?_typeInfo@… + 0x00)
extern "C" uint32_t RtGraphics_GeneratedImageBuffer_Type(const void* self) {
    (void)self;                                  // retail never touches RCX
    return ReadTypeId(TypeInfoGenerated());
}

// slot 1 — `MOV RAX,qword ptr [0x1801f9a28]` (= ?_typeInfo@… + 0x08)
extern "C" const char* RtGraphics_GeneratedImageBuffer_TypeName(const void* self) {
    (void)self;
    return ReadTypeName(TypeInfoGenerated());
}

// slot 3 — `MOV EAX,0x7 ; RET`. A LITERAL, not a derivation: the on-disk format
// version of this class. It happens to equal Video::ImageBuffer's 7 and that is
// a coincidence of the two classes' histories, not a rule — FrameCopy's and
// Reflection's are both 0x1007. Spelled as its own constant for that reason.
extern "C" int32_t RtGraphics_GeneratedImageBuffer_GetFileVersion(const void* self) {
    (void)self;
    return 7;
}

// ============================================================================
// slot 5 — ?Serialize@GeneratedImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
//                                                    retail 0x1800a3ec0
//
//     SUB RSP,0x28 ; CALL 0x1800f26f0 ; MOV AL,1 ; ADD RSP,0x28 ; RET
//
// The whole body: forward to the base and return true unconditionally. RCX and
// RDX are untouched, so `this` and the stream arrive at the base call exactly
// as they were passed. There is NO failure path and the base's return value is
// DISCARDED — `MOV AL,1` overwrites whatever it returned. Transcribed as
// written; returning the base's result instead would be a different function.
// ============================================================================
extern "C" bool RtGraphics_GeneratedImageBuffer_Serialize(void* self, Stream* stream) {
    ImageBufferSerialize(self, stream);
    return true;
}

// ############################################################################
// Graphics::FrameCopyImageBuffer — the nine slots wave23 did not port
// ############################################################################

// slot 2 — `MOV EAX,dword ptr [0x1801f99e8]`
extern "C" uint32_t RtGraphics_FrameCopyImageBuffer_Type(const void* self) {
    (void)self;
    return ReadTypeId(TypeInfoFrameCopy());
}

// slot 1 — `MOV RAX,qword ptr [0x1801f99f0]`
extern "C" const char* RtGraphics_FrameCopyImageBuffer_TypeName(const void* self) {
    (void)self;
    return ReadTypeName(TypeInfoFrameCopy());
}

// slot 3 — `MOV EAX,0x1007 ; RET`
extern "C" int32_t RtGraphics_FrameCopyImageBuffer_GetFileVersion(const void* self) {
    (void)self;
    return 0x1007;
}

// ============================================================================
// slot 5 — ?Serialize@FrameCopyImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
//                                                    retail 0x1800a3c40
//
//     MOV dword [RCX+0x2c],4        ; kind = 4  BEFORE the base call
//     CALL 0x1800f26f0              ; ImageBuffer::Serialize(this, stream)
//     MOV dword [RBX+0x2c],4        ; kind = 4  AGAIN, after it
//     MOV AL,1
//
// *** BOTH STORES ARE REAL AND BOTH MATTER. *** `kind` is +0x2c, which the base
// Serialize EMITS (imagebuffer_vtable.cpp's field list, the fourth emission) —
// so the first store decides what goes into the FILE, and the second undoes
// whatever the READING direction just loaded over it. Dropping either one
// changes a different thing: the first changes the bytes written, the second
// leaves a loaded object with the file's `kind` instead of 4.
// Video::ImageBuffer::IsGenerated() is `kind == 4`, which is what 4 means here.
// ============================================================================
extern "C" bool RtGraphics_FrameCopyImageBuffer_Serialize(void* self, Stream* stream) {
    RtGraphics::FrameCopyImageBuffer* const p =
        static_cast<RtGraphics::FrameCopyImageBuffer*>(self);
    p->base.kind = 4;
    ImageBufferSerialize(self, stream);
    p->base.kind = 4;
    return true;                                  // MOV AL,1; no failure path
}

// slot 7 — ?Destruct@ ... retail 0x18001f160.  `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`
extern "C" void RtGraphics_FrameCopyImageBuffer_Destruct(void* self) {
    DeleteThrough(self, 0u);                      // flags 0 = destroy, DO NOT free
}

// slot 8 — ?Construct@ ... retail 0x18001f150.
// `TEST RCX,RCX; JNZ 0x1800a3d10; XOR EAX,EAX; RET` — MSVC's placement-new
// codegen. The null test looks unreachable for a virtual and is transcribed
// anyway: "obviously unreachable" is not a licence to drop a branch.
extern "C" void* RtGraphics_FrameCopyImageBuffer_Construct(void* self) {
    if (self == nullptr) return nullptr;
    FrameCopyCtor(self);
    return self;                                  // the ctor's `MOV RAX,RSI`
}

// slot 6 — ?Reconstruct@ ... retail 0x18001f130. Destroy in place through slot
// 0 with flags 0, then tail-jump the constructor onto the SAME storage: the
// device-lost recovery primitive.
extern "C" void* RtGraphics_FrameCopyImageBuffer_Reconstruct(void* self) {
    DeleteThrough(self, 0u);
    FrameCopyCtor(self);
    return self;
}

// ============================================================================
// slot 11 — ?Render@FrameCopyImageBuffer@Graphics@@UEBAX…  retail 0x1800a3b10
//
//     RET
//
// One byte. This class copies the frame in `Generate`, which the owner calls
// directly; it contributes nothing to the render graph, so its Render arm is
// empty in the shipped build. An empty retail body is ported as an empty body
// (reference_retail_ships_empty_bodies.md) — a slot left as an abort trap is
// not the same thing, because the trap fires where retail returns.
// ============================================================================
extern "C" void RtGraphics_FrameCopyImageBuffer_Render(const void* self, void* renderer,
                                                       const void* scene, const void* camera,
                                                       int generation) {
    (void)self; (void)renderer; (void)scene; (void)camera; (void)generation;
}

// ############################################################################
// Graphics::ReflectionImageBuffer
// ############################################################################

namespace {

// The inlined constructor body, spelled once because FOUR retail bodies carry
// it verbatim: the constructor itself (0x1800a7146..0x1800a717f), `Reconstruct`
// (0x180022401..0x18002243a) and `Construct` (0x18002246c..0x1800224a5) all
// open-code exactly these stores in exactly this order, and the two vptr
// stamps are part of it.
//
// THE FIRST VPTR STORE IS OVERWRITTEN SEVEN INSTRUCTIONS LATER with no virtual
// dispatch in between, so it is observationally dead. Kept, for the reason
// wave23_lane_gfxbody.cpp gives about the same idiom: it is what the bytes do,
// and a later change that inserts anything between the two wants retail's own
// ordering rather than this file's opinion of it.
void ReflectionCtorBody(ReflectionImageBuffer* p) {
    ImageBufferCtor(p);                 // CALL 0x1800f2580
    p->base.vtable = VtGenerated();     // LEA 0x180154f60
    p->_generation = 0;
    p->base.vtable = VtReflection();    // LEA 0x180155190
    p->_depth  = nullptr;
    p->_target = nullptr;
    // ONE eight-byte store covers x and y, another covers z and d:
    //   MOV qword [RBX+0x80],RCX             (RCX == 0)
    //   MOV qword [RBX+0x88],0x3f800000
    p->_planeX = 0.0f;
    p->_planeY = 0.0f;
    p->_planeZ = 1.0f;                  // low half of the imm64
    p->_planeD = 0.0f;                  // high half — the sign-extended zero
}

}  // namespace

// ??0ReflectionImageBuffer@Graphics@@QEAA@XZ         retail 0x1800a7130
extern "C" void* RtGraphics_ReflectionImageBuffer_Ctor(void* self) {
    ReflectionCtorBody(static_cast<ReflectionImageBuffer*>(self));
    return self;                                   // MOV RAX,RBX
}

// ============================================================================
// ??1ReflectionImageBuffer@Graphics@@UEAA@XZ         retail 0x1800a70b0
//
//     LEA RAX,[0x180155190] ; MOV [RCX],RAX
//     RCX = this->_depth  ; if (RCX) { MOV EDX,1; CALL [[RCX]] }   ; slot 0
//     RDI = this->_target ; if (RDI) { PlatformDestruct; ??3RenderBuffer }
//     LEA RAX,[0x180154f60] ; MOV [RBX],RAX
//     JMP 0x1800f2610                                             ; ~ImageBuffer
//
// TWO DIFFERENT DESTROY IDIOMS, and swapping them would leak or double-free:
// the ImageBuffer at +0x70 has a virtual deleting destructor and is destroyed
// through its own slot 0 with flags = 1 (destroy AND free); the RenderBuffer at
// +0x78 has none, so it is destroyed the long way. FrameCopyImageBuffer's
// destructor makes the same distinction for the same reason.
//
// The plane at +0x80..+0x8c is NOT touched — it is four floats.
// ============================================================================
extern "C" void RtGraphics_ReflectionImageBuffer_Dtor(void* self) {
    ReflectionImageBuffer* const p = static_cast<ReflectionImageBuffer*>(self);

    p->base.vtable = VtReflection();
    if (p->_depth != nullptr) DeleteThrough(p->_depth, 1u);   // EDX = 1: free it too
    if (p->_target != nullptr) {
        RenderBufferPlatformDestruct(p->_target);
        RenderBufferOperatorDelete(p->_target);
    }
    p->base.vtable = VtGenerated();
    ImageBufferDtor(p);                            // tail JMP ??1ImageBuffer
}

// slot 2 — `MOV EAX,dword ptr [0x1801f9a90]`
extern "C" uint32_t RtGraphics_ReflectionImageBuffer_Type(const void* self) {
    (void)self;
    return ReadTypeId(TypeInfoReflection());
}

// slot 1 — `MOV RAX,qword ptr [0x1801f9a98]`
extern "C" const char* RtGraphics_ReflectionImageBuffer_TypeName(const void* self) {
    (void)self;
    return ReadTypeName(TypeInfoReflection());
}

// slot 3 — `MOV EAX,0x1007 ; RET`
extern "C" int32_t RtGraphics_ReflectionImageBuffer_GetFileVersion(const void* self) {
    (void)self;
    return 0x1007;
}

// slot 7 — retail 0x1800224c0, byte-identical to FrameCopy's.
extern "C" void RtGraphics_ReflectionImageBuffer_Destruct(void* self) {
    DeleteThrough(self, 0u);
}

// slot 8 — retail 0x180022450. The ctor is INLINED here rather than tail-called
// (unlike FrameCopy's, which jumps to 0x1800a3d10), which is why this goes
// through `ReflectionCtorBody` and not through a name door.
extern "C" void* RtGraphics_ReflectionImageBuffer_Construct(void* self) {
    if (self == nullptr) return nullptr;           // TEST RCX,RCX / XOR EAX,EAX
    ReflectionCtorBody(static_cast<ReflectionImageBuffer*>(self));
    return self;
}

// slot 6 — retail 0x1800223e0. Slot 0 with flags 0, then the inlined ctor.
extern "C" void* RtGraphics_ReflectionImageBuffer_Reconstruct(void* self) {
    DeleteThrough(self, 0u);
    ReflectionCtorBody(static_cast<ReflectionImageBuffer*>(self));
    return self;
}

// ============================================================================
// slot 10 — ?Generate@ReflectionImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z
//                                                    retail 0x1800a6ef0
//
//     RET
//
// Empty in the shipped build: a reflection image is produced by `Render`
// feeding a draw group, not by a blit, so there is nothing for the
// GeneratedImageBuffer "generate" arm to do. Ported as empty
// (reference_retail_ships_empty_bodies.md).
// ============================================================================
extern "C" void RtGraphics_ReflectionImageBuffer_Generate(const void* self, void* context,
                                                          int generation) {
    (void)self; (void)context; (void)generation;
}

// ============================================================================
// slot 5 — ?Serialize@ReflectionImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
//                                                    retail 0x1800a6f00
//
// Transcribed call by call from 0x1800a6f21 .. 0x1800a707e.
//
//   0x1800a6f21  this->kind = 5            ; BEFORE the base call, so 5 is what
//                                          ; the base Serialize EMITS
//   0x1800a6f28  ImageBuffer::Serialize(this, stream)
//   0x1800a6f32  Serializer ser{stream, nullptr}; rw = encoding factory
//   0x1800a6f70  rw->vtable[+0x28](rw, 4, &this->_planeX, nullptr)   ; kArrayFloat
//   0x1800a6f73  this->kind = 5            ; again, over whatever was loaded
//   0x1800a6f7a  if (stream->direction != 0) goto teardown
//                ... the LOAD-ONLY rebuild, below ...
//   0x1800a7068  if (rw) rw->vtable[0](rw, 1)
//   0x1800a707c  return true
//
// *** THE REBUILD ARM RUNS ONLY WHEN READING. *** `CMP dword [RBX+8],0 / JNZ`
// at 0x1800a6f7a tests IO::Stream::direction, and 0 is READING
// (runtime/types/io/Serializer.h: kDirectionReading). So saving a reflection
// image does not touch the GPU; loading one throws away the old depth target
// and render target and builds new ones at the size just read out of the file.
// That is the whole reason this class has a Serialize of its own.
//
// THE UNGUARDED DEREFS ARE FAITHFUL. If `??2ImageBuffer` returns null, retail
// walks into `MOV R10,[RCX]` at 0x1800a6fc2 and dies. Per CLAUDE.md that is
// reproduced, not guarded: the deref is load-bearing.
// ============================================================================
extern "C" bool RtGraphics_ReflectionImageBuffer_Serialize(void* self, Stream* stream) {
    ReflectionImageBuffer* const p = static_cast<ReflectionImageBuffer*>(self);

    p->base.kind = 5;
    ImageBufferSerialize(self, stream);

    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);
    EmitFloatArray(rw, 4, &p->_planeX);            // the plane: n, nx, ny, nz, d
    p->base.kind = 5;

    if (stream->direction == rt::io::kDirectionReading) {
        // --- the depth target -------------------------------------------------
        if (p->_depth != nullptr) DeleteThrough(p->_depth, 1u);

        void* const img = ImageBufferOperatorNew(0x68);
        if (img != nullptr) ImageBufferCtor(img);
        p->_depth = img;                           // never the ctor's return

        // `CALL qword ptr [R10+0x48]` = slot 9, Video::ImageBuffer::Create. The
        // eleven arguments in retail's own order, read from 0x1800a6fc5 ..
        // 0x1800a7005 — NOT assumed from the field order, which they do not
        // follow (the same trap wave23 documented on FrameCopy::Create):
        //   ImageType 0, Usage 1, Format 7,
        //   flags = this->flags, width = this->width, height = this->height,
        //   FilterMode 0, AddressMode 1, 1, 1, levels = nullptr.
        // Usage 1 is what makes this member a DEPTH buffer
        // (Video::ImageBuffer::IsDepthBuffer() is `kind == 1`).
        {
            void** const vt = *reinterpret_cast<void***>(img);   // faithful: unguarded
            using CreateFn = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t,
                                      int32_t, int32_t, int32_t,
                                      int32_t, int32_t, int32_t, void*);
            reinterpret_cast<CreateFn>(vt[9])(
                img, 0, 1, 7,
                p->base.flags, p->base.width, p->base.height,
                0, 1, 1, 1, nullptr);
        }

        // --- the render target ------------------------------------------------
        if (p->_target != nullptr) {
            RenderBufferPlatformDestruct(p->_target);
            RenderBufferOperatorDelete(p->_target);
        }
        void* const rb = RenderBufferOperatorNew(0x30);
        if (rb != nullptr) {
            reinterpret_cast<uint64_t*>(rb)[0] = 0;
            reinterpret_cast<uint64_t*>(rb)[1] = 0;
            reinterpret_cast<uint64_t*>(rb)[2] = 0;
            RenderBufferPlatformConstruct(rb);
        }
        p->_target = rb;

        // `MOV R8,[RDI+0x70]; MOV RDX,RDI; MOV RCX,RBX` — colour is THIS image,
        // depth is the buffer just built. The argument order is
        // Create(this, colour, depth), so the reflection renders INTO us.
        RenderBufferCreate(rb, p, p->_depth);
    }

    DestroyCodec(rw);
    return true;                                   // MOV AL,1
}

// ============================================================================
// slot 11 — ?Render@ReflectionImageBuffer@Graphics@@UEBAXAEAVRenderer@2@
//                   AEBVScene@2@AEBVCamera@2@H@Z      retail 0x1800a6b80
// ============================================================================
// The one body in this file that does real work, and the reason the class
// exists: mirror the camera through the reflection plane and queue a whole
// extra scene pass into the renderer that draws into this image.
//
// ARGUMENTS, from the mangled name and confirmed against the prologue:
//   RCX this, RDX Renderer&, R8 Scene&, R9 Camera&, [stack] int generation.
// The generation is the GeneratedImageBuffer latch: `CMP [RCX+0x68],ESI / JZ`
// at 0x1800a6bf8 skips the ENTIRE body when it already matches, and the last
// thing the body does is store it (0x1800a6e91). One pass per frame per image.
//
// ---------------------------------------------------------------------------
// THE REFLECTION MATRIX, 0x1800a6c01 .. 0x1800a6d22
// ---------------------------------------------------------------------------
// With n = (_planeX, _planeY, _planeZ) and d = _planeD, retail builds the
// Householder transform `I - 2 n nT` with a `2 d n` translation row, as twelve
// MULSS/SUBSS on three named constants:
//     0x18016169c = 1.0f      0x1801616d8 = 2.0f      0x180161768 = -2.0f
// (all three read out of the image with read_memory, not guessed).
//
// The twelve stores at 0x1800a6cde .. 0x1800a6d22 are, in address order:
//     [ 1-2nx^2   -2nx*ny   -2nx*nz ]
//     [ -2ny*nx    1-2ny^2  -2ny*nz ]
//     [ -2nz*nx   -2nz*ny    1-2nz^2]
//     [ 2d*nx      2d*ny     2d*nz  ]
//
// *** THE OFF-DIAGONAL TERMS ARE COMPUTED TWICE, WITH DIFFERENT OPERAND ORDER.
// *** Retail does NOT reuse `-2nx*ny` for the [1][0] slot: it computes
// `(-2*ny)*nx` there and `(-2*nx)*ny` at [0][1]. A single float product is
// exactly commutative, so the two are the same number and the duplication
// costs nothing — but it is transcribed as written rather than folded, because
// the folding is an argument about IEEE semantics and the transcription is not.
//
// The DIAGONAL is `1 - (n*2)*n`, i.e. the 2.0f multiply happens BEFORE the
// second factor (`MULSS XMM0,XMM10 ; MULSS XMM0,XMM8`), not after. `1 - 2*(n*n)`
// rounds differently and is not what the bytes do.
//
// The three 2d*n terms are `n*(d*2)`: XMM0 = d*2.0f is computed ONCE at
// 0x1800a6c12 and each component is multiplied BY it (`MULSS XMM1,XMM0` with
// the component in XMM1), so the component is the left operand.
//
// ---------------------------------------------------------------------------
// THE DRAW GROUP, 0x1800a6e09 .. 0x1800a6e7f
// ---------------------------------------------------------------------------
// A 0x110-byte record built on the stack and handed to
// `Array<T>::SetSize(renderer, size + 1, &group)` at 0x1800637b0 — the renderer
// object's FIRST member is that Array header, which is why RCX is the renderer
// itself and not renderer+something. Its stride is pinned two independent ways:
// this record's own extent, and `AmbientLight::AddDrawItems`' `IMUL RDX,RDX,0x110`
// (0x1800fa8b0), which indexes the same array. It is NOT the 0x1f0 draw-group
// record `Graphics::Renderer::BeginDrawGroup` grows — that is the array at
// renderer+0x10, and the two must not be confused.
//
//   +0x000  const Scene*             the scene argument, verbatim
//   +0x008  Video::RenderBuffer*     this->_target
//   +0x010  Graphics::Camera (0xd8)  copy-constructed from the MIRRORED camera
//   +0x0e8  int 0
//   +0x0ec  int 2
//   +0x0f0  int 0x8001
//   +0x0f4  int -1
//   +0x0f8  int   Video::Configuration::_singleton->[0x34]
//   +0x100  Collection::Array<T>     zeroed; destroyed again immediately below
//   +0x108  (the rest of that header)
//
// +0x0fc is NEVER WRITTEN. Recorded, not zeroed.
//
// `Array<T>::SetSize` COPIES the record in, so the local's own Array member is
// then destroyed with `~Array` (0x18001c750) before the frame is left — that is
// the trailing call at 0x1800a6e8c, and dropping it leaks one Array per pass.
// ============================================================================
namespace {

// `??0Camera@Graphics@@QEAA@AEBV01@@Z` 0x180017a70 — PORTED (renderer.cpp
// registers the name). Reached by NAME; the address door would work too, but
// the name is the one renderer.cpp guarantees.
void CameraCopyCtor(void* dst, const void* src) {
    using Fn = void* (*)(void*, const void*);
    RT_EXPORT_ONCE(Fn, "??0Camera@Graphics@@QEAA@AEBV01@@Z")(dst, src);
}

// `Collection::Array<T>::SetSize(n, const T&)` 0x1800637b0 and
// `Collection::Array<T>::~Array` 0x18001c750 — both UNEXPORTED template
// instantiations (checked against the shipped export directory), so both are
// address doors. `ResolveAddr(Module::Runtime, ...)` is registry-first, and
// both are already ported, so these land on our bodies.
void GroupArraySetSize(void* arrayHdr, int n, const void* fill) {
    using Fn = void (*)(void*, int, const void*);
    static Fn f = nullptr;
    if (!f) f = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800637b0));
    f(arrayHdr, n, fill);
}
// ENDGAME B.3.2 step 2 (callers) — `ArrayDtor`'s ResolveAddr forwarder is
// GONE. 0x18001c750 is array.cpp's `J1_ENTRY`, `ArrayDtorSited(a) { a->
// ~Array(); }` on a plain `ArrayHdr* = Collection::Array<uint8_t>*` (the
// DEFAULT trait-derived spec, not an ExplicitSpec row) — so the call site
// below now destroys the local group record's own trailing Array header
// exactly the same way, in place.

// `?_singleton@Configuration@Video@@0PEAV12@EA` — 0x1801fe7a0. A pointer TO the
// Configuration, so the read is a double indirection, which is what
// `MOV RAX,[0x1801fe7a0]; MOV EBX,[RAX+0x34]` does. By NAME: the row is an
// exported private static and a name door is the one that follows a promotion
// (reference_aeaa_is_private_not_unexported.md).
const uint8_t* VideoConfiguration() {
    return *RT_DATA_SLOT(const uint8_t* const*,
                         "?_singleton@Configuration@Video@@0PEAV12@EA");
}

constexpr uint32_t kCameraSize = 0xd8;   // group +0x010 .. +0x0e7
constexpr uint32_t kGroupSize  = 0x110;  // the Array stride, two ways (see above)

}  // namespace

extern "C" void RtGraphics_ReflectionImageBuffer_Render(void* self, void* renderer,
                                                        const void* scene,
                                                        const void* camera,
                                                        int generation) {
    ReflectionImageBuffer* const p = static_cast<ReflectionImageBuffer*>(self);
    if (p->_generation == generation) return;      // CMP [RCX+0x68],ESI / JZ

    // ---- the reflection transform (three rows + translation) ----------------
    const float nx = p->_planeX, ny = p->_planeY, nz = p->_planeZ, d = p->_planeD;
    const float two = 2.0f, negTwo = -2.0f, one = 1.0f;

    const float d2 = d * two;                       // MULSS XMM0,XMM10

    Math::Transform3 reflect;
    reflect.axisX       = { one - ((nx * two) * nx), (nx * negTwo) * ny, (nx * negTwo) * nz };
    reflect.axisY       = { (ny * negTwo) * nx, one - ((ny * two) * ny), (ny * negTwo) * nz };
    reflect.axisZ       = { (nz * negTwo) * nx, (nz * negTwo) * ny, one - ((nz * two) * nz) };
    reflect.translation = { nx * d2, ny * d2, nz * d2 };

    // ---- mirror the camera --------------------------------------------------
    // The camera's world transform is twelve floats at +0x64 (0x1800a6d27 ..
    // 0x1800a6d96 copy exactly +0x64 .. +0x90 out, one dword at a time, into a
    // stack copy — retail does NOT pass &camera->transform directly, and the
    // copy is reproduced because `operator*` takes `this` by pointer and would
    // otherwise alias the live camera).
    Math::Transform3 cameraToWorld;
    std::memcpy(&cameraToWorld,
                static_cast<const uint8_t*>(camera) + 0x64, sizeof cameraToWorld);

    const Math::Transform3 mirrored = cameraToWorld * reflect;

    alignas(8) uint8_t mirrorCam[kCameraSize];
    CameraCopyCtor(mirrorCam, camera);
    std::memcpy(mirrorCam + 0x64, &mirrored, sizeof mirrored);

    // ---- the draw group -----------------------------------------------------
    alignas(8) uint8_t group[kGroupSize];
    *reinterpret_cast<const void**>(group + 0x000) = scene;
    *reinterpret_cast<void**>(group + 0x008)       = p->_target;
    CameraCopyCtor(group + 0x010, mirrorCam);
    *reinterpret_cast<int32_t*>(group + 0x0e8)  = 0;
    *reinterpret_cast<int32_t*>(group + 0x0ec)  = 2;
    *reinterpret_cast<int32_t*>(group + 0x0f0)  = 0x8001;
    *reinterpret_cast<int32_t*>(group + 0x0f4)  = -1;
    *reinterpret_cast<int32_t*>(group + 0x0f8)  =
        *reinterpret_cast<const int32_t*>(VideoConfiguration() + 0x34);
    // +0x0fc: never written by retail. Left as it is.
    *reinterpret_cast<uint64_t*>(group + 0x100) = 0;
    *reinterpret_cast<uint64_t*>(group + 0x108) = 0;

    const int32_t count = *reinterpret_cast<const int32_t*>(
                              static_cast<const uint8_t*>(renderer) + 8);
    GroupArraySetSize(renderer, count + 1, group);

    // retail 0x18001c750 — the temporary's own Array<uint8_t> header, freed.
    reinterpret_cast<rtcoll::ArrayHdr*>(group + 0x100)->~Array();
    p->_generation = generation;                    // MOV [RDI+0x68],ESI
}

// ============================================================================
// Registration — BY NAME and BY ADDRESS for every body.
//
// A body PORTED but never REGISTERED is indistinguishable from an ABSENT one,
// and the mint fills a slot by NAME for RT_MINT_NAMED rows and by ADDRESS
// otherwise, so each row gets whichever door its slot uses and the other one
// too. Each mangled name is its own STRING LITERAL on one line so
// tools/runtime_census.py can see it — it cannot follow a named constant.
//
// The three slot-0 deleting destructors of these classes are NOT here: they are
// table rows in runtime/src/object/deleting_dtor.cpp, where the whole family
// lives as one body.
// ============================================================================
int RtGraphicsImageBufferDerivedRegister() {
    int n = 0;

    // ---- Graphics::GeneratedImageBuffer ------------------------------------
    RegisterRuntimeExport("??0GeneratedImageBuffer@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_GeneratedImageBuffer_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a3e70ull, (void*)&RtGraphics_GeneratedImageBuffer_Ctor);

    RegisterRuntimeExport("??1GeneratedImageBuffer@Graphics@@UEAA@XZ",
                          (void*)&RtGraphics_GeneratedImageBuffer_Dtor); ++n;
    RegisterRuntimeAddr(0x1800a3e40ull, (void*)&RtGraphics_GeneratedImageBuffer_Dtor);

    RegisterRuntimeExport("?Type@GeneratedImageBuffer@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_GeneratedImageBuffer_Type); ++n;
    RegisterRuntimeAddr(0x18001edc0ull, (void*)&RtGraphics_GeneratedImageBuffer_Type);

    RegisterRuntimeExport("?TypeName@GeneratedImageBuffer@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_GeneratedImageBuffer_TypeName); ++n;
    RegisterRuntimeAddr(0x18001ede0ull, (void*)&RtGraphics_GeneratedImageBuffer_TypeName);

    RegisterRuntimeExport("?GetFileVersion@GeneratedImageBuffer@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_GeneratedImageBuffer_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800a3eb0ull,
                        (void*)&RtGraphics_GeneratedImageBuffer_GetFileVersion);

    RegisterRuntimeExport("?Serialize@GeneratedImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_GeneratedImageBuffer_Serialize); ++n;
    RegisterRuntimeAddr(0x1800a3ec0ull, (void*)&RtGraphics_GeneratedImageBuffer_Serialize);

    // ---- Graphics::FrameCopyImageBuffer ------------------------------------
    RegisterRuntimeExport("?Type@FrameCopyImageBuffer@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Type); ++n;
    RegisterRuntimeAddr(0x18001ef90ull, (void*)&RtGraphics_FrameCopyImageBuffer_Type);

    RegisterRuntimeExport("?TypeName@FrameCopyImageBuffer@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_TypeName); ++n;
    RegisterRuntimeAddr(0x18001efb0ull, (void*)&RtGraphics_FrameCopyImageBuffer_TypeName);

    RegisterRuntimeExport("?GetFileVersion@FrameCopyImageBuffer@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800a3c70ull,
                        (void*)&RtGraphics_FrameCopyImageBuffer_GetFileVersion);

    RegisterRuntimeExport("?Serialize@FrameCopyImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Serialize); ++n;
    RegisterRuntimeAddr(0x1800a3c40ull, (void*)&RtGraphics_FrameCopyImageBuffer_Serialize);

    RegisterRuntimeExport("?Reconstruct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Reconstruct); ++n;
    RegisterRuntimeAddr(0x18001f130ull, (void*)&RtGraphics_FrameCopyImageBuffer_Reconstruct);

    RegisterRuntimeExport("?Destruct@FrameCopyImageBuffer@Graphics@@UEAAXXZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Destruct); ++n;
    RegisterRuntimeAddr(0x18001f160ull, (void*)&RtGraphics_FrameCopyImageBuffer_Destruct);

    RegisterRuntimeExport("?Construct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Construct); ++n;
    RegisterRuntimeAddr(0x18001f150ull, (void*)&RtGraphics_FrameCopyImageBuffer_Construct);

    RegisterRuntimeExport("?Render@FrameCopyImageBuffer@Graphics@@UEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Render); ++n;
    RegisterRuntimeAddr(0x1800a3b10ull, (void*)&RtGraphics_FrameCopyImageBuffer_Render);

    // ---- Graphics::ReflectionImageBuffer -----------------------------------
    RegisterRuntimeExport("??0ReflectionImageBuffer@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a7130ull, (void*)&RtGraphics_ReflectionImageBuffer_Ctor);

    RegisterRuntimeExport("??1ReflectionImageBuffer@Graphics@@UEAA@XZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Dtor); ++n;
    RegisterRuntimeAddr(0x1800a70b0ull, (void*)&RtGraphics_ReflectionImageBuffer_Dtor);

    RegisterRuntimeExport("?Type@ReflectionImageBuffer@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Type); ++n;
    RegisterRuntimeAddr(0x1800221b0ull, (void*)&RtGraphics_ReflectionImageBuffer_Type);

    RegisterRuntimeExport("?TypeName@ReflectionImageBuffer@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_TypeName); ++n;
    RegisterRuntimeAddr(0x1800221d0ull, (void*)&RtGraphics_ReflectionImageBuffer_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ReflectionImageBuffer@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800a70a0ull,
                        (void*)&RtGraphics_ReflectionImageBuffer_GetFileVersion);

    RegisterRuntimeExport("?Serialize@ReflectionImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_ReflectionImageBuffer_Serialize); ++n;
    RegisterRuntimeAddr(0x1800a6f00ull, (void*)&RtGraphics_ReflectionImageBuffer_Serialize);

    RegisterRuntimeExport("?Reconstruct@ReflectionImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Reconstruct); ++n;
    RegisterRuntimeAddr(0x1800223e0ull, (void*)&RtGraphics_ReflectionImageBuffer_Reconstruct);

    RegisterRuntimeExport("?Destruct@ReflectionImageBuffer@Graphics@@UEAAXXZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Destruct); ++n;
    RegisterRuntimeAddr(0x1800224c0ull, (void*)&RtGraphics_ReflectionImageBuffer_Destruct);

    RegisterRuntimeExport("?Construct@ReflectionImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_ReflectionImageBuffer_Construct); ++n;
    RegisterRuntimeAddr(0x180022450ull, (void*)&RtGraphics_ReflectionImageBuffer_Construct);

    RegisterRuntimeExport("?Generate@ReflectionImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z",
                          (void*)&RtGraphics_ReflectionImageBuffer_Generate); ++n;
    RegisterRuntimeAddr(0x1800a6ef0ull, (void*)&RtGraphics_ReflectionImageBuffer_Generate);

    RegisterRuntimeExport("?Render@ReflectionImageBuffer@Graphics@@UEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z",
                          (void*)&RtGraphics_ReflectionImageBuffer_Render); ++n;
    RegisterRuntimeAddr(0x1800a6b80ull, (void*)&RtGraphics_ReflectionImageBuffer_Render);


    return n;
}
