// ============================================================================
// runtime/src/graphics/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x180015b60  ??0Geometry@Graphics@@QEAA@AEBV01@@Z    393 B  (ordinal 204)
//   0x18001be60  Graphics_DrawGroup_CopyCtor             145 B  (unexported)
//   0x180014850  Graphics_EdgeMesh_SerializeTriangles    177 B  (unexported)
//
// All three are pure transcriptions with no new struct layout of their own: the
// Geometry copy constructor writes into the layout runtime/types/graphics/
// Geometry.h already pins (and this file static_asserts against it), and the
// other two walk arrays whose stride is in their own index arithmetic.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Geometry.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ??_7Geometry@Graphics@@6B@, 0x180154e60, ordinal 1439 — the same door
// runtime/src/graphics/scene.cpp:211 opens for the default constructor. Spelled
// again here rather than shared, because scene.cpp's accessor is file-local and
// a second `extern` declaration of a file-local is the kind of link-time
// coincidence this tree has been bitten by.
void* GeometryVtable() {
    static void* vt = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x180154e60));
    return vt;
}

// `Array<Geometry*>::operator=` — retail 0x18001b1a0, the CALL at 0x180015b8b.
// By address, registry-first: batch 2 ported it in
// runtime/src/collection/array_destroy.cpp, so this lands on our body.
void ArrayGeometryPtrAssign(void* dst, const void* src) {
    using Fn = void* (*)(void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001b1a0));
    fn(dst, src);
}

// `Graphics::Camera::Camera(const Camera&)` — retail 0x180017a70, read from
// 0x18001be7e / 0x18001be91 rather than resolved by a mangled name this file
// would have had to guess (reference_resolveexport_phantom_names.md).
void CameraCopyCtor(void* dst, const void* src) {
    using Fn = void* (*)(void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180017a70));
    fn(dst, src);
}

// The per-element copy constructor the eh_vector iterator is handed at
// 0x18001bece: `Collection::Array<T>::Array(const Array&)`, retail 0x18001c620
// (`dst[0]=0; dst[1]=0; operator=(dst, src)` — 0x18001cd30).
void InnerArrayCopyCtor(void* dst, const void* src) {
    using Fn = void* (*)(void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c620));
    fn(dst, src);
}

}  // namespace

// ============================================================================
// 0x180015b60  ??0Geometry@Graphics@@QEAA@AEBV01@@Z
//
// The copy constructor. Stores the BASE vtable (not the source's — retail's
// `LEA RAX,[0x180154e60]`, so copying a derived Geometry through this ctor
// produces a base Geometry, which is what slicing means and is faithful), then:
//
//   +0x00  vtable   = &??_7Geometry@Graphics@@6B@
//   +0x08  qword    copied           (_attachParent)
//   +0x10  dword    copied           (_attachNode)
//   +0x18  qword    ZEROED  }  the Array<Geometry*> header, zeroed and THEN
//   +0x20  qword    ZEROED  }  assigned through 0x18001b1a0 — the same
//                              zero-then-operator= order every nested Array
//                              copy in this DLL uses, because operator= starts
//                              by destroying what the destination holds.
//   +0x28 .. +0x78  21 dwords copied
//   +0x7c  QWORD    copied  (one unaligned 8-byte move, not two dwords —
//                            `MOV RAX,[RDX+0x7c]` at 0x180015bf1)
//   +0x84 .. +0xb8  14 dwords copied
//   +0xbc .. +0xbf  NEVER WRITTEN. sizeof(Geometry) is 0xC0 and the last store
//                   ends at 0xbc; the trailing dword is left holding whatever
//                   the caller's storage held. Recorded, not zeroed.
// ============================================================================
static_assert(sizeof(RtGraphics::Geometry) == 0xC0,
              "the copy ctor's last store is at +0xb8; a different sizeof means "
              "this transcription has the wrong tail");
static_assert(offsetof(RtGraphics::Geometry, _attached) == 0x18,
              "the nested Array<Geometry*> the copy ctor zeroes and assigns");

extern "C" void* RtGraphics_Geometry_CopyCtor(void* self, const void* src) {
    uint8_t* d = static_cast<uint8_t*>(self);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    *reinterpret_cast<void**>(d) = GeometryVtable();
    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
    *reinterpret_cast<uint32_t*>(d + 0x10) = *reinterpret_cast<const uint32_t*>(s + 0x10);

    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x20) = 0;
    ArrayGeometryPtrAssign(d + 0x18, s + 0x18);

    for (uint32_t o = 0x28; o <= 0x78; o += 4)
        *reinterpret_cast<uint32_t*>(d + o) = *reinterpret_cast<const uint32_t*>(s + o);

    // ONE unaligned qword, deliberately: retail moves +0x7c and +0x80 together.
    std::memcpy(d + 0x7c, s + 0x7c, 8);

    for (uint32_t o = 0x84; o <= 0xb8; o += 4)
        *reinterpret_cast<uint32_t*>(d + o) = *reinterpret_cast<const uint32_t*>(s + o);

    return self;   // MOV RAX,RDI — retail returns `this`
}

// ============================================================================
// 0x18001be60  the 0x1f0-byte "draw group" copy constructor
//
// Unexported and unnamed. Its owner is identified from the OTHER end: its
// element destructor, 0x18001c8c0, is array_destroy.cpp's J2b row (three inner
// `Collection::Array<T>` at +0x1c0/+0x1d0/+0x1e0, stride 0x1f0), and the array
// whose elements these are is grown by 0x1800638e0, whose only caller is
// `BeginDrawGroup` (0x180104fd0). So: a per-draw-group record holding TWO
// `Graphics::Camera` (0xD8 each, at +0x08 and +0xE0 — the second offset is what
// pins the Camera size) and three Arrays.
//
//   +0x000  qword  copied verbatim
//   +0x008  Camera copy-constructed  (CALL 0x180017a70)
//   +0x0e0  Camera copy-constructed  (CALL 0x180017a70)
//   +0x1b8  dword  copied
//   +0x1bc  dword  copied
//   +0x1c0  3 x 0x10 copy-constructed ASCENDING through MSVC's
//           `eh vector copy constructor iterator` (0x18007ed60) with element
//           ctor 0x18001c620. The iterator walks FORWARD (unlike the destructor
//           iterator, which walks backward), so the order here is +0x1c0,
//           +0x1d0, +0x1e0 — the reverse of the destroy order.
//   +0x1d0..+0x1ef are inside that range; nothing else in the 0x1f0 bytes is
//   written, so +0x0dc..? etc. are whatever the two Camera ctors leave.
//
// The CRT iterator is not reproduced as a call: three fixed elements with a
// known ctor is the whole of what it does here, and inlining it removes a CRT
// dependency at cutover (array_destroy.cpp's J2b row made the same call for the
// destroy side).
// ============================================================================
extern "C" void* RtGraphics_DrawGroup_CopyCtor(void* self, const void* src) {
    uint8_t* d = static_cast<uint8_t*>(self);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
    CameraCopyCtor(d + 0x008, s + 0x008);
    CameraCopyCtor(d + 0x0e0, s + 0x0e0);
    *reinterpret_cast<uint32_t*>(d + 0x1b8) = *reinterpret_cast<const uint32_t*>(s + 0x1b8);
    *reinterpret_cast<uint32_t*>(d + 0x1bc) = *reinterpret_cast<const uint32_t*>(s + 0x1bc);
    for (int k = 0; k < 3; ++k)                       // ASCENDING
        InnerArrayCopyCtor(d + 0x1c0 + k * 0x10, s + 0x1c0 + k * 0x10);
    return self;
}

// ============================================================================
// 0x180014850  SerializeTriangleListArray(SerializeCtx*, int count, void* data)
//
// Already named and reached by address from
// runtime/src/graphics/edgemesh_vtable.cpp:259, which listed it as inherited
// cutover work. This is that row closed.
//
// Per element (stride 0x18) it makes SIX identical virtual calls through the
// encoding's slot +0xa0, one per dword field, re-loading `ctx->encoding`
// (`[ctx+8]`) before each. All six pass (encoding, &field, 0) — the third
// argument is `XOR R8D,R8D` at every one of the six sites, checked in the
// disassembly rather than taken from Ghidra's C, which prints it on the first
// call and drops it on the other five.
//
// `TEST EDX,EDX / JLE` — a count of zero does nothing at all, including no
// virtual call. `MOV ESI,EDX` then `DEC RSI`: the counter is the zero-extended
// 32-bit argument, so a negative count is impossible rather than huge.
// ============================================================================
namespace {
using EncodeFieldFn = void (*)(void* encoding, void* field, int flags);
}

extern "C" void RtGraphics_EdgeMesh_SerializeTriangleListArray(void* ctx, int count,
                                                               void* data) {
    if (count <= 0) return;                                  // TEST/JLE
    uint8_t* e = static_cast<uint8_t*>(data);
    uint64_t n = static_cast<uint32_t>(count);               // MOV ESI,EDX
    do {
        void* enc = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + 8);
        void** vt = *reinterpret_cast<void***>(enc);
        EncodeFieldFn f = reinterpret_cast<EncodeFieldFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0xa0));
        f(enc, e + 0x00, 0);
        // Retail re-loads `ctx->encoding` and the slot before EVERY call; a
        // hoisted load would be one less indirection than retail performs and
        // would miss an encoding swapped mid-element. Kept literal.
        for (uint32_t o = 0x04; o <= 0x14; o += 4) {
            enc = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + 8);
            vt  = *reinterpret_cast<void***>(enc);
            f   = reinterpret_cast<EncodeFieldFn>(
                      *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0xa0));
            f(enc, e + o, 0);
        }
        e += 0x18;
    } while (--n);
}

// ============================================================================
// Registration — addresses for the two unexported bodies, address AND mangled
// name for the copy constructor (ordinal 204, present in the shipped export
// directory).
// ============================================================================
int RtClosure2bGraphicsRegister() {
    int n = 0;
    RegisterRuntimeExport("??0Geometry@Graphics@@QEAA@AEBV01@@Z",
                          (void*)&RtGraphics_Geometry_CopyCtor);              ++n;
    RegisterRuntimeAddr(0x180015b60, (void*)&RtGraphics_Geometry_CopyCtor);
    RegisterRuntimeAddr(0x18001be60, (void*)&RtGraphics_DrawGroup_CopyCtor);  ++n;
    RegisterRuntimeAddr(0x180014850,
                        (void*)&RtGraphics_EdgeMesh_SerializeTriangleListArray); ++n;
    return n;
}
