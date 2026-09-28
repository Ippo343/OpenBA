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
#include "../../types/io/Serializer.h"   // IO::ReaderWriter (B3X W0)

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// `Array<Geometry*>::operator=` — retail 0x18001b1a0, the CALL at 0x180015b8b.
// By address, registry-first: batch 2 ported it in
// runtime/src/collection/array_destroy.cpp, so this lands on our body.
//
// ENDGAME B.3.2 step 2 (callers) — NOT converted to `dst->_attached =
// src->_attached`. `Geometry::_attached` is a real `Collection::Array<
// Geometry*>`, but the class's `operator=` is presently `=default` (a
// SHALLOW bitwise copy — Collection.h's Array banner: the deep
// `ArrayAssignCore` this address runs becomes `operator=` only once the last
// bitwise copier converts). Rewriting this call as a member assignment would
// silently swap the deep, allocator-owning assign for a shallow one.
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
//
// ENDGAME B.3.2 step 2 (callers) — NOT converted. Same reason as
// `ArrayGeometryPtrAssign` above: `Collection::Array<T>`'s copy CONSTRUCTOR is
// also `=default` (shallow) today, so there is no member spelling yet that
// reproduces this deep zero-then-assign. `T` here is `RendererDrawItem`
// (Camera.h, 0x38 bytes — array_destroy.cpp's "Array<blob0x38>" names the
// element WIDTH, not an anonymous type: this is
// `Collection::Array<RendererDrawItem>::Array(const Array&)`, the copy ctor
// of `RendererDrawGroup::items[i]`), a real, named C++ type — the missing
// piece is only the class's copy constructor, not the element type.
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

// ENDGAME B.3.9 GFX: this is the MEMBER now. The vptr store (retail's
// `LEA RAX,[0x180154e60]`) is the compiler's; the ??0 copy door
// (`RtGraphics_Geometry_CopyCtor`, which stamped the minted table), its
// registrations (name + 0x180015b60) and `GeometryVtable()` are retired.
// `_attached` is default-initialised first (Array()'s three zero stores), which
// the body then repeats before the deep assign, so the final bytes are retail's.
namespace Graphics {
Geometry::Geometry(const Geometry& src) {
    uint8_t* d = reinterpret_cast<uint8_t*>(this);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(&src);

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
}
}  // namespace Graphics

// ############################################################################
// ENDGAME B.3 step 5 (block E) — the real `Graphics::Geometry`'s C++ surface
// ############################################################################
//
// `runtime/types/graphics/Geometry.h` now declares the real class. This file is
// the Geometry file in block E's file set, so it carries the two things the
// class needs in order to LINK:
//
//   1. the copy constructor RULE B5-10 requires be explicit (below), and
//   2. definitions for Geometry's five NON-PURE virtual slots (4..8).
//
// (2) is needed because a constructor definition on a polymorphic class emits
// the vptr store, which makes GCC emit `vtable for Graphics::Geometry` in this
// translation unit, which needs an address for every non-pure slot. The pure
// ones (0, 1, 2, 3) resolve to `__cxa_pure_virtual`, exactly as retail's resolve
// to `_purecall`. AS OF BLOCK F the five non-pure slots are DEFINED IN
// `scene.cpp`, so the vtable emitted here references them across translation
// units — which the linker resolves, and which is the normal arrangement for a
// class whose key function is not in the same file as its constructor.
//
// ============================================================================
// ENDGAME B.3.5 BLOCK F — FIVE OF THE SEVEN INVERTED DELEGATIONS ARE GONE
// ============================================================================
// Block E wrote SEVEN `Graphics::Geometry` definitions here — the copy
// constructor, the destructor and the five non-pure slots — and every one of them
// delegated INTO a door, the one place in block E where the direction was
// inverted. The reason was that all seven bodies lived in
// `runtime/src/graphics/scene.cpp`, which block E could not touch because it also
// holds `Graphics::Scene`, BLOCK F's class.
//
// Block F owns `scene.cpp`. The FIVE SLOT BODIES (`UpdateAttached` 4,
// `GetSubTransform` 5, `GetInstanceTransform` 6, `SetSharedInstanceData` 7,
// `Clean` 8) have MOVED ONTO THE MEMBERS THERE, their doors are one-line
// qualified calls, and their definitions here are deleted. Block E's prediction
// held exactly: no change to `Geometry.h` was needed.
//
// *** THE OTHER TWO CANNOT BE FLIPPED, AND THE REASONS ARE STRUCTURAL. *** They
// stay here, inverted, and this is the record of why rather than a TODO:
//
//  1. `??0Geometry@Graphics@@QEAA@AEBV01@@Z` (0x180015b60), the copy constructor.
//     `Graphics::Geometry` is ABSTRACT — slots 0, 1, 2 and 3 are pure
//     (B35_RULES §2.2) — so the door, which is handed RAW MEMORY by
//     `Collection::Array` copies and by the four POD mirrors, cannot
//     placement-construct one. There is no C++ expression that runs a
//     constructor of an abstract class on a buffer. The registered body and the
//     member therefore live in this same file, which is at least one file rather
//     than two.
//  2. `??1Geometry@Graphics@@UEAA@XZ` (0x1800a41c0), the destructor. It is
//     PROTECTED (ENDGAME §B.1 decision 2 makes a base's destructor non-virtual
//     and protected), so the door — a free function — cannot call it; and its
//     body's first act is to re-stamp `??_7Geometry@Graphics@@6B@` through a
//     `_vtable` MEMBER, which only the POD has. `scene.cpp` records both reasons
//     at the door.
//
// Both are in block F's report as debt block F could not discharge either, with
// the structural reason attached, so a later block does not re-attempt them.
//
// No `reinterpret_cast` here crosses a layout boundary: `RtGraphics::Geometry`
// and `Graphics::Geometry` are asserted field for field and size for size in
// Geometry.h, which is what licenses the cast (B33_RULES' minted-versus-GCC
// argument, one level down).
// ############################################################################

namespace Graphics {

// RULE B5-10 — `??0Geometry@Graphics@@QEAA@AEBV01@@Z`, 0x180015b60: declared
// explicitly in Geometry.h and NEVER `= default` (the deep Array assign above
// is the reason). Its body is the member defined above (ENDGAME B.3.9 GFX).

// Slots 4, 5, 6, 7 and 8 — `UpdateAttached`, `GetSubTransform`,
// `GetInstanceTransform`, `SetSharedInstanceData` and `Clean` — USED TO BE
// DEFINED HERE as reverse delegations. ENDGAME B.3.5 block F moved their bodies
// onto the members in `runtime/src/graphics/scene.cpp` and turned the five doors
// there into one-line qualified calls, so the definitions are gone from this file
// and the vtable emitted below references them across translation units. See this
// file's block-F banner above.

// `??1Geometry@Graphics@@UEAA@XZ`, 0x1800a41c0. NON-VIRTUAL (ENDGAME decision
// 2) and `protected`, so it is not in the vtable and the linker only needs it
// once a derived destructor is defined. It is defined here anyway, as the same
// reverse delegation, because it is the one member of this class that a derived
// class's destructor CANNOT avoid — and a missing definition would surface as an
// undefined reference in whichever later block defines one, a long way from the
// cause.
// ENDGAME B.3.9 GFX: `~Geometry()` is the member in scene.cpp; its door is retired.

}  // namespace Graphics

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
// `ctx` is the `IO::Serializer`; `[ctx+8]` is its ReaderWriter and +0xa0 is
// `SerializeInt` (Serializer.h). B3X W0: by name on the real class.
IO::ReaderWriter* CtxRw(void* ctx) {
    return *reinterpret_cast<IO::ReaderWriter**>(static_cast<uint8_t*>(ctx) + 8);  // B.3.x bridge
}
}

extern "C" void RtGraphics_EdgeMesh_SerializeTriangleListArray(void* ctx, int count,
                                                               void* data) {
    if (count <= 0) return;                                  // TEST/JLE
    uint8_t* e = static_cast<uint8_t*>(data);
    uint64_t n = static_cast<uint32_t>(count);               // MOV ESI,EDX
    do {
        CtxRw(ctx)->SerializeInt(*reinterpret_cast<int32_t*>(e + 0x00), nullptr);
        // Retail re-loads `ctx->readerWriter` and the slot before EVERY call; a
        // hoisted load would be one less indirection than retail performs and
        // would miss a reader/writer swapped mid-element. Kept literal: the
        // receiver is re-read here and the virtual call re-reads the vptr.
        for (uint32_t o = 0x04; o <= 0x14; o += 4) {
            CtxRw(ctx)->SerializeInt(*reinterpret_cast<int32_t*>(e + o), nullptr);
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
    // (??0Geometry copy, 0x180015b60: retired with its door, ENDGAME B.3.9 GFX.)
    RegisterRuntimeAddr(0x18001be60, (void*)&RtGraphics_DrawGroup_CopyCtor);  ++n;
    RegisterRuntimeAddr(0x180014850,
                        (void*)&RtGraphics_EdgeMesh_SerializeTriangleListArray); ++n;
    return n;
}
