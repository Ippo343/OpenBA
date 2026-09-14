// ============================================================================
// runtime/src/video/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x180054080  Video_ShaderProgram_SerializeBlobArray   127 B  (unexported)
//   0x1800f8fb0  Volume_Frustum_SelectBoxSupportVertices  467 B  (unexported)
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"  // ENDGAME B.3.2 step 2: real Array<uint8_t>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// ============================================================================
// 0x180054080 — the blob half of `Video::*Program::PlatformSerialize`.
//
// Its caller 0x180053ea0 is the whole pattern in four lines: serialize the
// outer count, resize the outer `Array<Array<uint8>>` if reading, then hand
// this function (ctx, count, data) to read or write every blob's bytes.
//
// Per element (stride 0x10 — `{uint8* data, int size, int capacity}`, i.e. a
// `Collection::Array<uint8>` header):
//
//   1. copy `element->_size` into a STACK dword and serialize THAT through the
//      encoding's +0xa0 slot with (encoding, &tmp, 0). The stack copy matters:
//      on a read the slot writes the incoming size into `tmp`, NOT into the
//      array header, so the array's own `_size` is untouched until step 2.
//   2. if `*(int*)((char*)ctx->0 + 8) == 0` — the "reading" test, `CMP [RAX+8],0
//      / JNZ` at 0x1800540bf — call 0x180029630(element, tmp), the
//      `Array<uint8>::SetSize` that allocates the buffer.
//   3. serialize the bytes through slot +0x60 with (encoding, tmp,
//      element->_data, 0).
//
// `TEST EDX,EDX / JLE` at entry: a count of zero does nothing. `MOVSXD RSI,EDX`
// — the counter IS sign-extended here (unlike 0x180014850's zero-extension),
// but the JLE above has already excluded negatives.
//
// 0x180029630 is not on this batch's worklist and is reached by ADDRESS, so the
// day it is ported this call follows it (ResolveAddr is registry-first).
// ============================================================================
namespace {
using EncodeFieldFn = void (*)(void* encoding, void* field, int flags);
using EncodeBytesFn = void (*)(void* encoding, int size, void* data, int flags);
}  // namespace

// ENDGAME B.3.2 step 2 — `ArrayU8SetSize` (0x180029630) is GONE: each element
// `e` walked below IS a `Collection::Array<uint8_t>` header (the stride-0x10
// `{data,size,capacity}` triple the banner above names), and `kSpec_Bool`
// (0x180029630's row, array.cpp) is `SpecFor<uint8_t>()` field for field —
// trait-derived, so a direct `.SetSize()` on the reinterpreted element is that
// exact body.

extern "C" void RtVideo_ShaderProgram_SerializeBlobArray(void* ctx, int count,
                                                          void* data) {
    if (count <= 0) return;                                   // TEST EDX,EDX / JLE
    uint8_t* e = static_cast<uint8_t*>(data);
    int64_t n = count;                                        // MOVSXD RSI,EDX
    do {
        int tmp = *reinterpret_cast<const int32_t*>(e + 8);   // MOV EAX,[RBX+8]
        {
            void* enc = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + 8);
            void* vt  = *reinterpret_cast<void**>(enc);
            EncodeFieldFn f = reinterpret_cast<EncodeFieldFn>(
                *reinterpret_cast<void**>(static_cast<uint8_t*>(vt) + 0xa0));
            f(enc, &tmp, 0);
        }
        {
            void* first = *reinterpret_cast<void**>(ctx);      // MOV RAX,[RDI]
            if (*reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(first) + 8) == 0)
                // retail 0x180029630 — kSpec_Bool == SpecFor<uint8_t>().
                reinterpret_cast<Collection::Array<uint8_t>*>(e)->SetSize(tmp);
        }
        {
            void* enc = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + 8);
            void* vt  = *reinterpret_cast<void**>(enc);
            EncodeBytesFn f = reinterpret_cast<EncodeBytesFn>(
                *reinterpret_cast<void**>(static_cast<uint8_t*>(vt) + 0x60));
            f(enc, tmp, *reinterpret_cast<void**>(e), 0);
        }
        e += 0x10;
    } while (--n);
}

// ============================================================================
// 0x1800f8fb0 — the support-vertex selector inside `Volume::Frustum::Contains`
// (its only caller, 0x1800f8e80).
//
// Given a plane normal and an axis-aligned box {min[3], max[3]}, it produces the
// box's two SUPPORT VERTICES along that normal: the corner furthest along -n
// (written to `outNeg`) and the corner furthest along +n (`outPos`). That pair
// is the standard "n-vertex / p-vertex" frustum-culling trick, and it is what
// lets Contains classify a box against a plane with two dot products.
//
// Per component i: `outNeg[i] = (n[i] < 0) ? box.max[i] : box.min[i]` and
// `outPos[i]` is the other one. Retail evaluates the three components in the
// order Z, Y, X (it loads n[0], n[2], n[1] and branches on z first) and then
// stores X, Y, Z; the order is preserved because these are four distinct
// buffers a caller may alias, and an aliasing caller can see it.
//
// COMPARISON IS `< 0.0f` (COMISS against a zeroed register), not a sign-bit
// test. -0.0f therefore takes the `min` branch, and a NaN component takes it
// too (COMISS sets CF on unordered, and the branch is JB). Reproduced with the
// same C comparison, which compiles to the same predicate.
// ============================================================================
extern "C" void RtVolume_Frustum_SelectBoxSupportVertices(const float* n,
                                                           const float* box,
                                                           float* outNeg,
                                                           float* outPos) {
    const float nx = n[0];
    const float nz = n[2];
    const float ny = n[1];

    const float a2 = (nz < 0.0f) ? box[5] : box[2];
    const float a1 = (ny < 0.0f) ? box[4] : box[1];
    const float a0 = (nx < 0.0f) ? box[3] : box[0];
    outNeg[0] = a0;
    outNeg[1] = a1;
    outNeg[2] = a2;

    const float b2 = (nz < 0.0f) ? box[2] : box[5];
    const float b1 = (ny < 0.0f) ? box[1] : box[4];
    const float b0 = (nx < 0.0f) ? box[0] : box[3];
    outPos[0] = b0;
    outPos[1] = b1;
    outPos[2] = b2;
}

int RtClosure2bVideoRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x180054080,
                        (void*)&RtVideo_ShaderProgram_SerializeBlobArray);    ++n;
    RegisterRuntimeAddr(0x1800f8fb0,
                        (void*)&RtVolume_Frustum_SelectBoxSupportVertices);   ++n;
    return n;
}
