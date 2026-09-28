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
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/math/Math.h"
#include "../../types/graphics/FrameCopyImageBuffer.h"
#include "../../types/graphics/GeneratedImageBuffer.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Stream.h"
#include "../../types/video/Device.h"       // ENDGAME B.3.5: Video::RenderBuffer
#include "../../types/video/ImageBuffer.h"   // ENDGAME B.3.5: Video::ImageBuffer
#include "../collection/array_internal.h"
#include "../../types/graphics/Camera.h"   // rtgfxd::RtGraphics_Camera_CopyCtor
#include "../../types/graphics/Scene.h"    // rtgfxd::RtGfxd_SceneArraySetSizeFill

// ENDGAME B.3.5 block C — the three real classes. The `RtGraphics::*` PODs are
// no longer used by this file at all; `FrameCopyImageBuffer.h`'s stays for
// `wave23_lane_gfxbody.cpp` (block H1's) and `GeneratedImageBuffer.h`'s two for
// the layout those mirrors embed.
using Graphics::FrameCopyImageBuffer;
using Graphics::GeneratedImageBuffer;
using Graphics::ReflectionImageBuffer;

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

// ---------------------------------------------------------------------------
// The named dependencies, called by name (B3R).
// ---------------------------------------------------------------------------
using SerializeFn = bool (*)(void*, void*);

// `??0ImageBuffer@Video@@QEAA@XZ` 0x1800f2580 — PORTED (video/buffers.cpp).
// Declared returning void because that is how buffers.cpp spells it; the
// callers below therefore never read its return and use the pointer they
// already hold (wave23_lane_gfxbody.cpp's rule, and
// reference_hidden_retslot_wild_write.md's family).
// ENDGAME B.3.9 VS: `::new (p) Video::ImageBuffer()` -- the ??0 door and the minted row are retired.
void ImageBufferCtor(void* p) {
    ::new (p) Video::ImageBuffer();
}
// ENDGAME B.3.5 block C — the `?Serialize@ImageBuffer@Video@@` (0x1800f26f0)
// name door is GONE from this file. All three bodies below reach it as their
// BASE-CLASS arm, which a real class expresses as the qualified non-virtual call
// `this->Video::ImageBuffer::Serialize(stream)`; the member is defined in
// `runtime/src/video/imagebuffer_vtable.cpp`, also this block's, so the call
// cannot land anywhere else. The door itself stays registered there for the raw
// callers outside this block.
//
// `??0ImageBuffer@Video@@` and `??1ImageBuffer@Video@@` KEEP their doors below:
// their bodies are in `runtime/src/video/buffers.cpp`, which is block D2's file,
// so the door is still what decides which body runs.
void* ImageBufferOperatorNew(uint64_t n) {
    return Video::ImageBuffer::operator new(n);    // 0x18001e910
}
void* RenderBufferOperatorNew(uint64_t n) {
    return Video::RenderBuffer::operator new(n);   // 0x18001f2e0
}
// ENDGAME B.3.5 caller batch: `PlatformConstruct`, `PlatformDestruct` and
// `Create` are DEFINED members of the real `Video::RenderBuffer`, so all three
// by-name doors become member calls, and so does the `??3` operator delete (B3R).
void RenderBufferPlatformConstruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformConstruct();   // B.3.5 bridge
}
void RenderBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformDestruct();    // B.3.5 bridge
}
void RenderBufferOperatorDelete(void* p) {
    Video::RenderBuffer::operator delete(p);       // 0x18001f360
}
bool RenderBufferCreate(void* rb, const void* colour, const void* depth) {
    return reinterpret_cast<Video::RenderBuffer*>(rb)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(colour),
        reinterpret_cast<const Video::ImageBuffer*>(depth));          // B.3.5 bridge
}

// ENDGAME B.3.9 block GFX: `FrameCopyCtor` (the ??0 name door), `VtGenerated`
// (the minted table's rebased address) and `DeleteThrough` (a raw slot-0 read
// for objects wearing a minted table) are gone: the three classes are real, and
// Construct / Reconstruct / Destruct spell placement new and `DeletingDtor`.

// ---------------------------------------------------------------------------
// ENDGAME B.3.5 block C — `MakeCodec`, `DestroyCodec` and `EmitFloatArray` are
// GONE, and with them this file's third copy of the encoding-factory prologue
// and its three `rt::io::*_vtbl` byte offsets (`kCreateReader`/`kCreateWriter`,
// `kDtor`, `kArrayFloat` +0x28).
//
// The REAL `IO::Serializer` (B.3.4 block 3) is that prologue: its constructor
// takes the reading or the writing factory slot off the stream's encoding and
// stores the result, and its destructor is `if (rw) rw->slot0(rw, 1)`. The third
// factory argument is the save-format VERSION and it is ZERO — retail zeroes R8D
// before the `CMP [RDX+8],R8D` and never reloads it (0x1800a6f3d / 0x1800a6f4d in
// `ReflectionImageBuffer::Serialize`) — so the local is `IO::Serializer ser(s, 0)`
// and its SCOPE is what places both stream operations at retail's positions.
// `EmitFloatArray` is `ser.Serialize(4, &_planeX, nullptr)`, the non-virtual
// forwarder that reaches +0x28.
// ---------------------------------------------------------------------------

}  // namespace

// ############################################################################
// THE MEMBERS — `Graphics::GeneratedImageBuffer` and
// `Graphics::FrameCopyImageBuffer`. ENDGAME B.3.5 block C.
//
// This block is at `namespace Graphics` scope and comes AFTER the anonymous
// namespace closes, per B34_BRIEFS A.2: a member of `Graphics::X` cannot be
// defined inside a sibling namespace, and the anonymous-namespace helpers are
// still reachable here by unqualified name because they are members of the
// global namespace's declarative region.
// ############################################################################
namespace Graphics {

// slot 2 — `MOV EAX,dword ptr [0x1801f9a20]` (= ?_typeInfo@… + 0x00)
// ENDGAME B.3.9 TI-REST: Type / TypeId / TypeName are DECLARE_TYPE's bodies over the class's own
// record (graphics_type_records.cpp).

// slot 1 — `MOV RAX,qword ptr [0x1801f9a28]` (= ?_typeInfo@… + 0x08)

// slot 3 — `MOV EAX,0x7 ; RET`. A LITERAL, not a derivation: the on-disk format
// version of this class. It happens to equal Video::ImageBuffer's 7 and that is
// a coincidence of the two classes' histories, not a rule — FrameCopy's and
// Reflection's are both 0x1007. Spelled as its own constant for that reason.
int32_t GeneratedImageBuffer::GetFileVersion() const { return 7; }

// RULE B5-15 — COPIED from B35_SLOTS section 3, and this class is the reason the
// rule is stated the way it is: the two literals for ONE class disagree about
// namespacing.
//   0x18001eda0  ?_TypeName@       -> "GeneratedImageBuffer"   (no namespace)
//   0x18001edb0  ?_ParentTypeName@ -> "Video::ImageBuffer"     (namespaced)
// (Both are DEFINE_TYPE's, graphics_type_records.cpp.)

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
//
// `CALL 0x1800f26f0` is the BASE-CLASS arm, which a real class spells as a
// qualified non-virtual call. It must stay qualified: an unqualified
// `Serialize(stream)` would be a recursive virtual call into this very slot.
// ============================================================================
bool GeneratedImageBuffer::Serialize(void* stream) {
    Video::ImageBuffer::Serialize(stream);
    return true;
}

// ############################################################################
// Graphics::FrameCopyImageBuffer — the eight slots wave23 did not port
// ############################################################################

// slot 2 — `MOV EAX,dword ptr [0x1801f99e8]`
// ENDGAME B.3.9 TI-REST: Type / TypeId / TypeName are DECLARE_TYPE's bodies over the class's own
// record (graphics_type_records.cpp).
// slot 1 — `MOV RAX,qword ptr [0x1801f99f0]`
// slot 3 — `MOV EAX,0x1007 ; RET`
int32_t FrameCopyImageBuffer::GetFileVersion() const { return 0x1007; }

// RULE B5-15, COPIED: 0x18001ef70 -> "FrameCopyImageBuffer",
// 0x18001ef80 -> "GeneratedImageBuffer" (NO namespace).
// (Both are DEFINE_TYPE's, graphics_type_records.cpp.)

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
bool FrameCopyImageBuffer::Serialize(void* stream) {
    _kind = 4;
    Video::ImageBuffer::Serialize(stream);
    _kind = 4;
    return true;                                  // MOV AL,1; no failure path
}

// slot 7 — ?Destruct@ ... retail 0x18001f160.  `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`
void FrameCopyImageBuffer::Destruct() {
    this->~FrameCopyImageBuffer();                             // flags 0 = destroy, DO NOT free
}

// slot 8 — ?Construct@ ... retail 0x18001f150.
// `TEST RCX,RCX; JNZ 0x1800a3d10; XOR EAX,EAX; RET` — MSVC's placement-new
// codegen: placement new of the class. (ENDGAME B.3.9 GFX: the null-`this`
// arm sat in the retired slot door; a virtual is never entered with null.)
Object::External* FrameCopyImageBuffer::Construct() {
    return ::new (this) FrameCopyImageBuffer();
}

// slot 6 — ?Reconstruct@ ... retail 0x18001f130. Destroy in place through slot
// 0 with flags 0, then tail-jump the constructor onto the SAME storage: the
// device-lost recovery primitive.
Object::External* FrameCopyImageBuffer::Reconstruct() {
    this->~FrameCopyImageBuffer();
    return ::new (this) FrameCopyImageBuffer();
}

// ?CreateNewObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@XZ
// 0x18001f0a0 -- `??2FrameCopyImageBuffer` (MemAlloc 0x88, align 8), retail's
// null branch, then the ctor. Replaces the FACTREST row (B39_RULES D2).
Object::Base* FrameCopyImageBuffer::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FrameCopyImageBuffer), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) FrameCopyImageBuffer();
}
// ?CreateInPlaceObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z
// 0x18001f120
Object::Base* FrameCopyImageBuffer::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) FrameCopyImageBuffer();
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
void FrameCopyImageBuffer::Render(void* renderer, const void* scene,
                                  const void* camera, int32_t generation) const {
    (void)renderer; (void)scene; (void)camera; (void)generation;
}

}  // namespace Graphics

// ############################################################################
// ENDGAME B.3.9 block GFX: the registered doors of the two classes above are
// RETIRED. `??0GeneratedImageBuffer` (0x1800a3e70) / `??1GeneratedImageBuffer`
// (0x1800a3e40) stamped the MINTED `??_7GeneratedImageBuffer`; the real
// `GeneratedImageBuffer()` / `~GeneratedImageBuffer()` (GeneratedImageBuffer.h)
// are the only ones now, and nothing but the retired RT_DELDTOR rows and the
// FrameCopy ctor door reached them. The four GeneratedImageBuffer and eight
// FrameCopyImageBuffer slot doors had one reader each, the minted tables
// `??_7GeneratedImageBuffer@` / `??_7FrameCopyImageBuffer@` (retired).
// ############################################################################

namespace Graphics {
// ?CreateNewObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@XZ      0x18001edf0
// ?CreateInPlaceObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z 0x18001ee00
// Retail's ABSTRACT pair: both bodies are `XOR EAX,EAX ; RET` (read at both
// VAs). Replaces the FACTREST row (B39_RULES D2).
Object::Base* GeneratedImageBuffer::CreateNewObject() { return nullptr; }
Object::Base* GeneratedImageBuffer::CreateInPlaceObject(void* place) { (void)place; return nullptr; }
}  // namespace Graphics
// ############################################################################
// Graphics::ReflectionImageBuffer
// ############################################################################

// ============================================================================
// ENDGAME B.3.9 block GFX: `ReflectionCtorBody` (the ctor open-coded with two
// MINTED vptr stamps) and the ??0 / ??1 doors are gone. The ctor is the real
// `ReflectionImageBuffer()` (GeneratedImageBuffer.h), the stamps are the
// compiler's, and the ??1 door's body is the member below.
// ============================================================================
namespace Graphics {

// ??1ReflectionImageBuffer@Graphics@@UEAA@XZ         retail 0x1800a70b0
//
//     LEA RAX,[0x180155190] ; MOV [RCX],RAX                         ; chain
//     RCX = this->_depth  ; if (RCX) { MOV EDX,1; CALL [[RCX]] }   ; slot 0
//     RDI = this->_target ; if (RDI) { PlatformDestruct; ??3RenderBuffer }
//     LEA RAX,[0x180154f60] ; MOV [RBX],RAX ; JMP 0x1800f2610       ; chain
//
// TWO DIFFERENT DESTROY IDIOMS, and swapping them would leak or double-free:
// the ImageBuffer at +0x70 has a virtual deleting destructor and is destroyed
// through its own slot 0 with flags = 1 (destroy AND free); the RenderBuffer at
// +0x78 has none, so it is destroyed the long way. The plane is not touched.
// The two restamps and the tail jump to ~ImageBuffer (via the empty
// ~GeneratedImageBuffer) are the compiler's destructor chain.
ReflectionImageBuffer::~ReflectionImageBuffer() {
    if (_depth != nullptr) delete _depth;   // retail: slot 0, EDX = 1: free it too
    if (_target != nullptr) {
        RenderBufferPlatformDestruct(_target);
        RenderBufferOperatorDelete(_target);
    }
}

// ?CreateNewObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@XZ
// 0x1800222c0 -- `??2ReflectionImageBuffer` (MemAlloc 0x90, align 8), retail's
// null branch, the inlined ctor. Replaces the FACTREST row (B39_RULES D2).
Object::Base* ReflectionImageBuffer::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ReflectionImageBuffer), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ReflectionImageBuffer();
}
// ?CreateInPlaceObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z
// 0x180022370
Object::Base* ReflectionImageBuffer::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ReflectionImageBuffer();
}

// slot 2 — `MOV EAX,dword ptr [0x1801f9a90]`
// ENDGAME B.3.9 TI-REST: Type / TypeId / TypeName are DECLARE_TYPE's bodies over the class's own
// record (graphics_type_records.cpp).
// slot 1 — `MOV RAX,qword ptr [0x1801f9a98]`
// slot 3 — `MOV EAX,0x1007 ; RET`
int32_t ReflectionImageBuffer::GetFileVersion() const { return 0x1007; }

// RULE B5-15, COPIED: 0x180022190 -> "ReflectionImageBuffer",
// 0x1800221a0 -> "GeneratedImageBuffer" (NO namespace).
// (Both are DEFINE_TYPE's, graphics_type_records.cpp.)

// slot 7 — retail 0x1800224c0, byte-identical to FrameCopy's: slot 0, flags 0.
void ReflectionImageBuffer::Destruct() { this->~ReflectionImageBuffer(); }

// slot 8 — retail 0x180022450. The ctor is INLINED here (unlike FrameCopy's,
// which jumps to 0x1800a3d10): placement new of the class. (ENDGAME B.3.9 GFX:
// the `TEST RCX,RCX / XOR EAX,EAX` arm sat in the retired door.)
Object::External* ReflectionImageBuffer::Construct() {
    return ::new (this) ReflectionImageBuffer();
}

// slot 6 — retail 0x1800223e0. Slot 0 with flags 0, then the inlined ctor.
Object::External* ReflectionImageBuffer::Reconstruct() {
    this->~ReflectionImageBuffer();
    return ::new (this) ReflectionImageBuffer();
}

}  // namespace Graphics

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
namespace Graphics {
void ReflectionImageBuffer::Generate(void* context, int32_t generation) const {
    (void)context; (void)generation;
}
}  // namespace Graphics

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
namespace Graphics {

bool ReflectionImageBuffer::Serialize(void* stream) {
    // B.3.5 bridge: the slot's parameter is `IO::Stream&` in retail, but
    // `Object::External::Serialize` is declared `bool(void*)` across the whole
    // External family and that signature is not this block's to change.
    IO::Stream& s = *reinterpret_cast<IO::Stream*>(stream);

    _kind = 5;
    Video::ImageBuffer::Serialize(stream);         // the BASE-CLASS arm, qualified

    {
        IO::Serializer ser(s, 0);
        ser.Serialize(4, &_planeX, nullptr);       // the plane: n, nx, ny, nz, d
        _kind = 5;

        if (s.GetDirection() == IO::kReading) {
            // --- the depth target ---------------------------------------------
            if (_depth != nullptr) delete _depth;

            void* const img = ImageBufferOperatorNew(0x68);
            if (img != nullptr) ImageBufferCtor(img);
            // B.3.5 bridge: `??2ImageBuffer` hands back raw storage and
            // `??0ImageBuffer` is still a door in block D2's `buffers.cpp`, so the
            // object is typed only here. Never the ctor's return value — see the
            // `ImageBufferCtor` banner.
            _depth = reinterpret_cast<Video::ImageBuffer*>(img);

            // `CALL qword ptr [R10+0x48]` = slot 9, Video::ImageBuffer::Create.
            // With the real class that is `_depth->Create(...)`, a virtual call at
            // index 9 through the object's own (minted) vptr — the same
            // instruction, with the index checked by the compiler. The deref is
            // UNGUARDED in retail and stays unguarded here: if `??2ImageBuffer`
            // returned null, retail walks into `MOV R10,[RCX]` at 0x1800a6fc2 and
            // dies, and per CLAUDE.md that is reproduced, not guarded.
            //
            // The eleven arguments in retail's own order, read from 0x1800a6fc5 ..
            // 0x1800a7005 — NOT assumed from the field order, which they do not
            // follow (the same trap wave23 documented on FrameCopy::Create):
            //   ImageType 0, Usage 1, Format 7,
            //   flags = this->flags, width = this->width, height = this->height,
            //   FilterMode 0, AddressMode 1, 1, 1, levels = nullptr.
            // Usage 1 is what makes this member a DEPTH buffer
            // (Video::ImageBuffer::IsDepthBuffer() is `kind == 1`).
            _depth->Create(0, 1, 7, _flags, _width, _height, 0, 1, 1, 1, nullptr);

            // --- the render target --------------------------------------------
            if (_target != nullptr) {
                RenderBufferPlatformDestruct(_target);
                RenderBufferOperatorDelete(_target);
            }
            void* const rb = RenderBufferOperatorNew(0x30);
            if (rb != nullptr) {
                reinterpret_cast<uint64_t*>(rb)[0] = 0;
                reinterpret_cast<uint64_t*>(rb)[1] = 0;
                reinterpret_cast<uint64_t*>(rb)[2] = 0;
                RenderBufferPlatformConstruct(rb);
            }
            _target = rb;

            // `MOV R8,[RDI+0x70]; MOV RDX,RDI; MOV RCX,RBX` — colour is THIS
            // image, depth is the buffer just built. The argument order is
            // Create(this, colour, depth), so the reflection renders INTO us.
            RenderBufferCreate(rb, this, _depth);
        }
    }   // ~Serializer: `if (rw) rw->slot0(rw, 1)` at 0x1800a7068
    return true;                                   // MOV AL,1
}

}  // namespace Graphics

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

// `??0Camera@Graphics@@QEAA@AEBV01@@Z` 0x180017a70 — its ported body,
// rtgfxd::RtGraphics_Camera_CopyCtor (wave9_lane_gfxd.cpp), by name.
void CameraCopyCtor(void* dst, const void* src) {
    rtgfxd::RtGraphics_Camera_CopyCtor(static_cast<RtGraphics::Camera*>(dst),
                                       static_cast<const RtGraphics::Camera*>(src));
}

// `Collection::Array<T>::SetSize(n, const T&)` 0x1800637b0 and
// `Collection::Array<T>::~Array` 0x18001c750 — both UNEXPORTED template
// instantiations (checked against the shipped export directory); both are
// ported and the first is called by its body name.
void GroupArraySetSize(void* arrayHdr, int n, const void* fill) {
    rtgfxd::RtGfxd_SceneArraySetSizeFill(arrayHdr, n, fill);
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
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    return reinterpret_cast<const uint8_t*>(::Video::Configuration::_singleton);
}

constexpr uint32_t kCameraSize = 0xd8;   // group +0x010 .. +0x0e7
constexpr uint32_t kGroupSize  = 0x110;  // the Array stride, two ways (see above)

}  // namespace

namespace Graphics {

void ReflectionImageBuffer::Render(void* renderer, const void* scene,
                                   const void* camera, int32_t generation) const {
    // `?Render@` is `UEBA` — CONST — and the body's last act WRITES `_generation`
    // (0x1800a6e91). Retail's own signature and retail's own body disagree, which
    // MSVC source can only have expressed with a cast too. The const-ness is
    // transcribed because it is what the slot's mangled name says, and the write
    // is spelled with the cast at the one site rather than by making the field
    // `mutable`, which would hide the disagreement in the declaration.
    ReflectionImageBuffer* const p = const_cast<ReflectionImageBuffer*>(this);

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

}  // namespace Graphics

// ############################################################################
// ENDGAME B.3.9 block GFX: the nine `Graphics::ReflectionImageBuffer` slot doors
// (Type, TypeName, GetFileVersion, Destruct, Construct, Reconstruct, Generate,
// Serialize, Render) are gone -- their only reader was the minted
// `??_7ReflectionImageBuffer@Graphics@@6B@` (retired). Every live reflection
// image wears the compiler's table.
// ############################################################################

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

    // ---- Graphics::GeneratedImageBuffer / FrameCopyImageBuffer -------------
    // ENDGAME B.3.9 block GFX: the ctor / dtor / slot doors are retired (see the
    // banner above the Reflection section). The factory pairs (B39_RULES D2) are
    // the classes' own statics, by name AND address; the minted `?_typeInfo@`
    // records resolve them.

    // ---- Graphics::ReflectionImageBuffer -----------------------------------
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@ReflectionImageBuffer` record resolves them.

    return n;
}
