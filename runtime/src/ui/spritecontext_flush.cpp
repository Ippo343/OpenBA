// ============================================================================
// runtime/src/ui/spritecontext_flush.cpp — WAVE 9 LANE FLUSH
//                                          UI::SpriteContext::Flush 0x18011ba40
// ============================================================================
//
// Lane UIF closed three exported `SpriteContext` methods and, in doing so,
// opened exactly one per-frame ADDRESS door: `Flush`. Its write-up
// (docs/runtime_port/WAVE9_LANE_UIF2.md §1, §11.4) says so plainly, and says
// the same object is also why ~11,400 live executions of the class carry no
// differential evidence — `SetScissor`'s changed branch, `AddLine`'s
// sheet-rebind and `FlushDeferred`'s drain all call `Flush`, and no replay
// survives a call that maps the GPU vertex buffer and submits a draw.
//
// This file closes that door. The lane doc is docs/runtime_port/WAVE9_LANE_FLUSH.md.
//
// ---------------------------------------------------------------------------
// WHAT THE FUNCTION IS
// ---------------------------------------------------------------------------
// 591 instructions, retail 0x18011ba40..0x18011c698. It drains `_draws` — one
// batch of same-sheet records — into the Video::Context's dynamic vertex
// buffer, six vertices per sprite (two triangles) or two per line, then clears
// the batch and the sheet cursor. In order:
//
//   1. Ask the video backend for the half-pixel offset:
//        `[[0x1801fe7a8]+0x100]->vt[0x28]()`  (Device::_singleton -> DeviceData,
//        vtable slot 5). ALWAYS called, even when there is nothing to draw —
//        it is the first instruction pair of the function, before the
//        `_currentSheet == 0` test at 0x18011ba73.
//   2. If `_currentSheet` is null: skip to step 8.
//   3. Push `_scissorX..H` into `_context+0x1e0..+0x1ec` if any of the four
//      differs, and OR 0x400 into the context dirty mask.
//   4. `isLine = _draws._size > 0 ? _draws[0].isLine : 0` — the WHOLE batch is
//      typed by its FIRST record.
//   5. `MaterialInstance::Setup(_currentSheet->+0x30, _context, 0)`.
//   6. Walk the batch in chunks of `(isLine ? 2 : 6) << 12` records. Per chunk:
//      `BeginVertices(records*vertsPer, 32)` through the ContextData vtable
//      (+0x2160, slot 0x70); if it FAILS the chunk's records are SKIPPED and
//      `EndVertices` is NOT called (`JMP 0x18011c5e6` at 0x18011bc0f); if it
//      succeeds, every record is expanded and `EndVertices(isLine ? 2 : 0)`
//      (slot 0x78) submits it.
//   7. Each record becomes 6 (or 2) 32-byte vertices memcpy'd one at a time
//      into `_context->_dynamicVertexCursor`, advancing by
//      `_context->_dynamicVertexStride`. Both fields are RE-READ from
//      `_context` before every single vertex; that is kept.
//   8. `Array<SpriteDraw>::SetSize(0)` on `_draws`, then `_currentSheet = 0`.
//
// ---------------------------------------------------------------------------
// THE HALF-PIXEL OFFSET IS 0.0f IN THIS PROCESS, AND THAT IS AN HONEST LIMIT
// ---------------------------------------------------------------------------
// DeviceData vtable slot 5 is `dx11/src/devicedata_stubs.cpp`'s
// `DeviceData_Stub5_ZeroFloat` — the shipped DX11 backend returns 0.0f (a DX9
// backend would return 0.5f). So every `- halfPixel` below subtracts zero on
// every live call, and a defect that dropped the term entirely would NOT be
// caught by a live oracle. The negative control `BANISHED_RT_FLUSH_DEFECT=4`
// exists exactly to show the term is wired: it forces 0.5f into OUR body only,
// and the oracle must diverge. See the lane doc's HONEST LIMITS.
//
// ---------------------------------------------------------------------------
// FLOAT FAITHFULNESS: THE TERMS ARE NOT REORDERED
// ---------------------------------------------------------------------------
// The rotated path builds `T(-centre) * R(angle) * T(+centre)` as three real
// `Math::Transform2`s and two real 2x3 multiplies, with 1.0f/0.0f stored as
// data — retail does not fold the identities, and neither does this port.
// Folding is algebraically identical and NOT bit-identical
// (reference_moved_term_hides_where_terms_cancel.md): `0.0f * -sin` is `-0.0f`,
// and `-0.0f + x` differs from `x` when `x` is `+0.0f`. Every ADDSS in
// `Transform2Mul` and the two point helpers keeps retail's operand order; MULSS
// operands are commutative and are written in whichever order reads best.
//
// The constants are read out of the shipped .rdata, not retyped
// (reference_crt_coefficients_must_be_lifted_not_retyped.md):
//   0x18016167c = 0x3c8efa35  degrees -> radians
//   0x180161694 = 0x3f000000  +0.5f
//   0x180161754 = 0xbf000000  -0.5f
//   0x18016169c = 0x3f800000  +1.0f
//   0x1801617c0 = 0x80000000  the sign flip that makes -sin
//
// `sinf`/`cosf` (0x180127d90 / 0x180126d10) go through `RtLibm::Sin/Cos`,
// which are the PORTED MSVC CRT bodies (Wave 9 lane L) — so this file opens no
// door for them.
//
// ---------------------------------------------------------------------------
// THE ORACLE — AND WHY THIS ONE ACTUALLY WORKS
// ---------------------------------------------------------------------------
// Wave 7 lane F's hold-back, restated by lane UIF §2, was: `Flush` "submits
// into the LIVE Video::Context / GPU vertex buffer ... a scratch copy of the
// SpriteContext object does not isolate a replay", because the copy's
// `_context` still points at the one live context. That is true, and this file
// does not dispute it. It builds a scratch **Context** as well.
//
// The reason a scratch Context is possible at all — and the reason nobody had
// checked — is that the only two things retail's `Flush` does to the context
// that are not plain field writes are:
//
//   * `MaterialInstance::Setup` (0x180102c90), and
//   * the two ContextData virtual calls at +0x2160 slots 0x70 / 0x78.
//
// `Setup` was READ rather than assumed
// (reference_holdback_blockers_are_stale_check_them_first.md). Every one of its
// writes — and every write of `VideoProgram::SetupShaders` (0x1800f4520), the
// one function it calls — is guarded by "if the context's field differs from
// the material's value, store it and set a dirty bit". There is no allocation,
// no refcount, and no backend call anywhere in either body. So `Setup` on a
// BYTE-COPY of the live context, taken before our body ran, writes nothing at
// all: our body already ran the same `Setup` on the original, so every compare
// finds the values equal. That makes the replay observable-free.
//
// The two virtual calls are removed by pointing the scratch context's +0x2160
// at a fake ContextData whose vtable this file owns: slot 0x70 hands back a
// private arena and returns true, slot 0x78 records the primitive type. So the
// shipped body writes its vertices into OUR memory, and our body tees a copy of
// its own vertices into a second arena. The verdict is a byte compare of the
// two arenas plus the Begin/EndVertices argument log plus the four scissor
// ints, the dirty mask, `_draws._size` and `_currentSheet`.
//
// FOUR THINGS THIS ORACLE DELIBERATELY DOES NOT COMPARE:
//   * the whole 0x2168-byte context. Comparing bytes neither side writes is
//     how an oracle reports 100% divergence on unrelated noise
//     (reference_oracle_must_not_compare_unwritten_bytes.md). The context is
//     compared only at +0x10 and +0x1e0..+0x1ec, the only fields `Flush`
//     itself writes.
//   * `_context->_dynamicVertexCursor`, because the two sides run against
//     different buffers by construction. The ADVANCE is compared instead, as
//     the arena byte count.
//   * anything at all when the sheet is null or the batch is empty — there is
//     nothing to compare, and the site declares those calls `worked=0`.
//   * the record buffer. The scratch SpriteContext gets its OWN copy of the
//     records, so the shipped `Array<SpriteDraw>::SetSize(0)` at the end
//     cannot reach the live batch even if lane UIF's §6 proof that it never
//     frees were wrong. The oracle checks that the data pointer survived and
//     reports a HARNESS finding, not a divergence, if it did not.
//
// ---------------------------------------------------------------------------
// THE DOOR ARITHMETIC THIS FILE PRODUCES
// ---------------------------------------------------------------------------
// CLOSES one address door: 0x18011ba40, lane UIF's only opened one.
// OPENS none. Every callee is already satisfied:
//   0x180102c90  MaterialInstance::Setup  — reached BY EXPORTED NAME
//                (`?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z`),
//                which code/Game/Player/tool_base_tilerender.cpp:100 already
//                opens, so the unsatisfied-EXPORT set does not grow either.
//   0x180127d90 / 0x180126d10  sinf/cosf  — RtLibm, ported (lane L).
//   0x1800783d0  Array<SpriteDraw>::SetSize(0) — ported (lane UIF), and this
//                file reaches it through the same literal `ResolveAddr`
//                spelling that file uses, so the audit sees one door, not two.
//   0x18008a290  the DLL's `memcpy` — `std::memcpy`, no door.
//   0x1801fe7a8  Device::_singleton — a DATA address, already minted.
//   the ContextData vtable at +0x2160 — a virtual call, not a door.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_libm.h"
#include "../../funcs/rt_port.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/video/Context.h"

// Same forward declarations, same spelling, as spritecontext.cpp — the linker
// checks them. See that file's header for why resolve.h is not included.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

using UI::SpriteContext;
using UI::SpriteDraw;

// ---------------------------------------------------------------------------
// The constants, lifted from the shipped .rdata rather than retyped.
// ---------------------------------------------------------------------------
inline float FromBits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, sizeof(f));
    return f;
}
const float kDegToRad = FromBits(0x3c8efa35);   // 0x18016167c
const float kHalf     = FromBits(0x3f000000);   // 0x180161694
const float kNegHalf  = FromBits(0xbf000000);   // 0x180161754
const float kOne      = FromBits(0x3f800000);   // 0x18016169c
const float kZero     = FromBits(0x00000000);

// ---------------------------------------------------------------------------
// The vertex retail builds on its stack: 32 bytes, four of them per record.
//
// The layout is read off the stores, not guessed. Retail keeps four slots at
// RSP+0x28 (A), RSP+0x48 (B), RBP-0x70 (C) and RBP-0x50 (D) — with RBP = RSP +
// 0x100 for this frame (`LEA RBP,[RSP-0x1d8]` then `SUB RSP,0x2d8`) — and the
// field offsets within a slot are identical in all four:
//
//   +0x00 x    +0x04 y    +0x08 z (always 0)   +0x0c color (raw ARGB dword)
//   +0x10 u    +0x14 v    (the sprite's sheet UV, from sprite+0x24..+0x30)
//   +0x18 cu   +0x1c cv   (the CORNER uv: A(0,0) B(0,1) C(1,1) D(1,0))
//
// `color` is moved through an integer stack slot and reloaded with MOVSS
// (0x18011c472/0x18011c476) — a bit reinterpret, not a conversion, so it is a
// uint32_t here.
// ---------------------------------------------------------------------------
struct FlushVertex {
    float    x, y, z;
    uint32_t color;
    float    u, v;
    float    cu, cv;
};
static_assert(sizeof(FlushVertex) == 0x20, "the dynamic vertex is 32 bytes");
static_assert(offsetof(FlushVertex, color) == 0x0c, "color at +0x0c");
static_assert(offsetof(FlushVertex, u)     == 0x10, "sheet u at +0x10");
static_assert(offsetof(FlushVertex, cu)    == 0x18, "corner u at +0x18");

// ---------------------------------------------------------------------------
// Math::Transform2 — a 2x2 linear part plus a translation, row-vector
// convention. Retail builds three of these on the stack at RBP+0x50 (T1),
// RBP+0x30 (R) and RBP+0x70 (T2) and multiplies them pairwise.
// ---------------------------------------------------------------------------
struct Transform2 {
    float m00, m01;
    float m10, m11;
    float tx,  ty;
};

// r = a * b. THE ADDITION ORDER IS RETAIL'S, term for term:
//
//   0x18011c03e  MULSS XMM0, a.m10*b.m00 ; 0x18011c04d MULSS XMM1, a.m11*b.m10
//   0x18011c057  ADDSS XMM1,XMM0                      -> r.m10
//   0x18011c003  XMM0 = b.m00*a.tx ; XMM1 = b.m10*a.ty
//   0x18011c011  ADDSS XMM1,XMM0 ; 0x18011c018 ADDSS XMM1,b.tx  -> r.tx
//
// i.e. every element is `(a[i][1] * b[1][j]) + (a[i][0] * b[0][j])`, the ROW-1
// term FIRST, and the translation adds `b.t` last. The second multiply
// (0x18011c100 onwards) has the identical shape, which is why one helper
// serves both.
inline Transform2 Transform2Mul(const Transform2& a, const Transform2& b) {
    Transform2 r;
    r.m00 = (a.m01 * b.m10) + (a.m00 * b.m00);
    r.m01 = (a.m01 * b.m11) + (a.m00 * b.m01);
    r.m10 = (a.m11 * b.m10) + (a.m10 * b.m00);
    r.m11 = (a.m11 * b.m11) + (a.m10 * b.m01);
    r.tx  = ((b.m10 * a.ty) + (b.m00 * a.tx)) + b.tx;
    r.ty  = ((b.m11 * a.ty) + (b.m01 * a.tx)) + b.ty;
    return r;
}

// One point through the transform, same ADDSS order (0x18011c216..0x18011c2ae):
// the y term first, then the x term, then the translation.
inline float TransformX(const Transform2& m, float x, float y) {
    return ((m.m10 * y) + (m.m00 * x)) + m.tx;
}
inline float TransformY(const Transform2& m, float x, float y) {
    return ((m.m11 * y) + (m.m01 * x)) + m.ty;
}

// ---------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------
// `?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z` 0x180102c90.
// An EXPORT door the tree already opens (tool_base_tilerender.cpp:100), so
// naming it here adds nothing to the unsatisfied-export set. RT_EXPORT_ONCE and
// not RT_ORIGINAL_ONCE: this call is part of the WORK, so it must pick up a
// future port of `Setup` rather than being frozen onto the shipped body.
typedef void (*MaterialSetupFn)(void* material, void* context, int shader);
const char* const kMaterialSetup =
    "?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z";

// `Collection::Array<UI::SpriteDraw>::SetSize(0)` 0x1800783d0, ported and
// registered in runtime/src/collection/array.cpp. Reached through the same
// literal `ResolveAddr(Module::Runtime, 0x1800783d0)` spelling spritecontext.cpp
// uses, on purpose: `tools/addr_door_audit.py` matches a LITERAL and nothing
// else (reference_literal_grep_blind_to_variables.md), and two spellings of one
// door must still read as one door.
typedef void (*ArrayClearFn)(void* array);
inline ArrayClearFn TheArrayClear() {
    static ArrayClearFn f = nullptr;
    if (!f) f = reinterpret_cast<ArrayClearFn>(ResolveAddr(Module::Runtime, 0x1800783d0));
    return f;
}

// ---------------------------------------------------------------------------
// Field access on the two objects this file does not own a header for.
// ---------------------------------------------------------------------------
template <typename T>
inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

enum : size_t {
    // Video::Context (runtime/types/video/Context.h names all five).
    kCtxDirtyMask     = 0x0010,
    kCtxScissor       = 0x01e0,   // four ints, x/y/w/h
    kCtxVertexCursor  = 0x20e0,
    kCtxVertexStride  = 0x20e8,
    kCtxContextData   = 0x2160,
    // UI::SpriteSheet — only two fields are read by Flush.
    kSheetSprites     = 0x0020,   // the sprite array; stride 0x38
    kSheetMaterial    = 0x0030,   // Graphics::MaterialInstance*
    // The UI::Sprite entry inside that array.
    kSpriteUv         = 0x0024,   // u0, v0, u1, v1 at +0x24/+0x28/+0x2c/+0x30
    kSpriteStride     = 0x0038,
};

// ContextData vtable byte offsets. Both are pinned twice over: once as the
// offset the exported one-line thunk jumps through, and once by dx11/'s own
// slot table (dx11/src/contextdata.cpp:37-38).
enum : size_t {
    kCdBeginVertices = 0x70,   // bool (ContextData*, int vertexCount, int stride)
    kCdEndVertices   = 0x78,   // void (ContextData*, int primitiveType)
};
typedef bool (*BeginVerticesFn)(void*, int32_t, int32_t);
typedef void (*EndVerticesFn)(void*, int32_t);

// The dirty bit `Flush` ORs in when the scissor changed (0x18011badf).
const uint32_t kDirtyScissor = 0x400;

// ---------------------------------------------------------------------------
// The half-pixel offset. `Video::Device::_singleton` (0x1801fe7a8) -> +0x100
// (the DeviceData) -> vtable slot 5.
//
// The SLOT ADDRESS is cached (it is a fixed .data location); the POINTER it
// holds is re-read on every call, which is what retail does
// (`MOV RAX,[0x1801fe7a8]` inside the body). Never cache a null: the singleton
// is null while the address registry is being built, and freezing that would
// break the process for good (rt_port.h's contract; lane UIF's probe fault at
// Runtime+0x11BA66 is the same hole seen from the other side).
// ---------------------------------------------------------------------------
inline void* VideoDeviceData() {
    static void** slot = nullptr;
    if (!slot) slot = reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fe7a8));
    if (!slot) return nullptr;
    void* device = *slot;
    if (!device) return nullptr;
    return Field<void*>(device, 0x100);
}

inline float DeviceHalfPixel(void* deviceData) {
    void** vt = *reinterpret_cast<void***>(deviceData);
    typedef float (*Fn)(void*);
    return reinterpret_cast<Fn>(vt[5])(deviceData);
}

// ---------------------------------------------------------------------------
// THE TEE AND THE INJECTED DEFECT — both live only while the oracle runs.
// ---------------------------------------------------------------------------
// `g_tee` is non-null only inside the oracle's own arm, so a shipping run pays
// one predictable-not-taken branch per vertex and nothing else.
struct Arena {
    uint8_t* base;
    size_t   used;
    size_t   cap;
    Arena() : base(nullptr), used(0), cap(0) {}
};

// The Begin/EndVertices argument log — recorded on BOTH sides and compared.
struct CallLog {
    static const int kMax = 16;
    int      n;
    int      overflow;
    int32_t  beginCount[kMax];
    int32_t  beginStride[kMax];
    int8_t   beginOk[kMax];
    int32_t  endPrim[kMax];         // -1 == EndVertices was not called
    CallLog() : n(0), overflow(0) {
        std::memset(beginCount, 0, sizeof(beginCount));
        std::memset(beginStride, 0, sizeof(beginStride));
        std::memset(beginOk, 0, sizeof(beginOk));
        std::memset(endPrim, 0, sizeof(endPrim));
    }
    void Begin(int32_t c, int32_t s, bool ok) {
        if (n >= kMax) { ++overflow; return; }
        beginCount[n]  = c;
        beginStride[n] = s;
        beginOk[n]     = ok ? 1 : 0;
        endPrim[n]     = -1;
        ++n;
    }
    void End(int32_t prim) {
        if (n <= 0 || n > kMax) return;
        endPrim[n - 1] = prim;
    }
};

Arena*   g_tee        = nullptr;   // our body's vertex tee
CallLog* g_log        = nullptr;   // our body's Begin/EndVertices log
uint32_t g_dirtyAtEnd = 0;         // live dirty mask sampled before our EndVertices
int      g_defect     = 0;         // BANISHED_RT_FLUSH_DEFECT, negative controls
bool     g_defectRead = false;

int FlushDefect() {
    if (!g_defectRead) {
        g_defectRead = true;
        const char* e = std::getenv("BANISHED_RT_FLUSH_DEFECT");
        g_defect = (e && *e) ? std::atoi(e) : 0;
    }
    return g_defect;
}

// One vertex into the live buffer, plus the tee when the oracle is watching.
// `_context` is re-read for EVERY vertex and so are both fields, exactly as
// retail does (six `MOV RBX,[R13+0x30]` in the shared tail, 0x18011c46e..).
inline void Emit(SpriteContext* self, const FlushVertex& v) {
    void*         ctx    = self->_context;
    uint8_t*      cursor = Field<uint8_t*>(ctx, kCtxVertexCursor);
    const int32_t stride = Field<int32_t>(ctx, kCtxVertexStride);
    std::memcpy(cursor, &v, static_cast<size_t>(stride));
    Field<uint8_t*>(ctx, kCtxVertexCursor) = cursor + stride;

    if (g_tee) {
        Arena* a = g_tee;
        if (a->used + static_cast<size_t>(stride) <= a->cap)
            std::memcpy(a->base + a->used, &v, static_cast<size_t>(stride));
        a->used += static_cast<size_t>(stride);
    }
}

// ---------------------------------------------------------------------------
// THE ORACLE'S FAKE ContextData
// ---------------------------------------------------------------------------
// One object, one vtable, both owned by this file. Only two of its slots are
// ever reached, and the object is only ever seen by the SHIPPED body running
// against the scratch context — nothing else can find it.
struct FakeContextData {
    void*    vtable;
    void*    context;    // the scratch Video::Context this fake belongs to
    Arena*   arena;
    CallLog* log;
    int      chunk;      // how many BeginVertices calls have been served
    uint32_t failMask;   // bit k set => chunk k's BeginVertices returns FALSE
};

bool FakeBeginVertices(void* selfRaw, int32_t vertexCount, int32_t stride) {
    FakeContextData* self = static_cast<FakeContextData*>(selfRaw);
    const int k = self->chunk++;
    if (k < 32 && (self->failMask & (1u << k)) != 0) {
        // The scripted MAP FAILURE. Retail's response to a false here is the
        // one branch of Flush that is otherwise unreachable in a normal run:
        // the chunk's records are skipped and EndVertices is NOT called
        // (0x18011bc0d / 0x18011bc0f). Nothing is published into the Context,
        // exactly as a real BeginVertices that bailed on capacity does.
        if (self->log) self->log->Begin(vertexCount, stride, false);
        return false;
    }
    // Real BeginVertices PUBLISHES the stride and the mapped cursor into the
    // Context, and the shipped Flush reads BOTH back out of the Context — so
    // the fake must publish them too. A fake that owns state instead of
    // publishing it is invisible to its caller
    // (reference_port_must_reproduce_side_effects.md, which applies to a fake
    // exactly as it applies to a port).
    Field<int32_t>(self->context, kCtxVertexStride)  = stride;
    Field<uint8_t*>(self->context, kCtxVertexCursor) = self->arena->base + self->arena->used;
    if (self->log) self->log->Begin(vertexCount, stride, true);
    return true;
}

void FakeEndVertices(void* selfRaw, int32_t primitiveType) {
    FakeContextData* self = static_cast<FakeContextData*>(selfRaw);
    // The arena cursor moved as the shipped body memcpy'd; recover the advance
    // from the Context, which is the only place it was recorded.
    uint8_t* cursor = Field<uint8_t*>(self->context, kCtxVertexCursor);
    if (cursor >= self->arena->base &&
        static_cast<size_t>(cursor - self->arena->base) <= self->arena->cap)
        self->arena->used = static_cast<size_t>(cursor - self->arena->base);
    if (self->log) self->log->End(primitiveType);
}

// Slot 0x70 is index 14 and 0x78 is index 15; the table covers both and every
// other entry is null, which is safe because nothing else is reached — if that
// turned out to be false the process would fault on a null call immediately
// rather than running on wrong data.
void* g_fakeVtable[16] = {};

bool LogsEqual(const CallLog& a, const CallLog& b) {
    if (a.n != b.n || a.overflow != b.overflow) return false;
    for (int i = 0; i < a.n && i < CallLog::kMax; ++i) {
        if (a.beginCount[i]  != b.beginCount[i])  return false;
        if (a.beginStride[i] != b.beginStride[i]) return false;
        if (a.beginOk[i]     != b.beginOk[i])     return false;
        if (a.endPrim[i]     != b.endPrim[i])     return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The body, factored out so the entry point can wrap it with the oracle.
// `defect` is 0 on every shipping call.
// ---------------------------------------------------------------------------
void FlushBody(SpriteContext* self, float halfPixel, int defect,
               int* outRecords, int* outLines, int* outRotated, int* outMapFail) {
    if (self->_currentSheet == nullptr) {
        TheArrayClear()(&self->_draws);
        self->_currentSheet = nullptr;
        return;
    }

    void* ctx = self->_context;

    // --- the scissor push, 0x18011bab3..0x18011bb08 -------------------------
    // Four independent compares that all fall through to the SAME store block:
    // one difference rewrites all four fields and sets the bit.
    int32_t* ctxScissor = &Field<int32_t>(ctx, kCtxScissor);
    if (ctxScissor[0] != self->_scissorX || ctxScissor[1] != self->_scissorY ||
        ctxScissor[2] != self->_scissorW || ctxScissor[3] != self->_scissorH) {
        Field<uint32_t>(ctx, kCtxDirtyMask) |= kDirtyScissor;
        ctxScissor[0] = self->_scissorX;
        ctxScissor[1] = self->_scissorY;
        ctxScissor[2] = self->_scissorW;
        ctxScissor[3] = self->_scissorH;
    }

    // --- the batch type, 0x18011bb0e..0x18011bb20 ---------------------------
    // The FIRST record types the whole batch. An empty batch is `isLine = 0`.
    int32_t count = self->_draws._size;
    const uint8_t isLine = (count > 0) ? self->_draws._data[0].isLine : 0;

    // --- 0x18011bb37 -------------------------------------------------------
    RT_EXPORT_ONCE(MaterialSetupFn, kMaterialSetup)(
        Field<void*>(self->_currentSheet, kSheetMaterial), ctx, 0);

    // --- the chunked walk, 0x18011bb3c..0x18011c5ef ------------------------
    const int32_t vertsPer = isLine ? 2 : 6;
    const int32_t chunkCap = vertsPer << 12;      // `SHL EAX,0xc` at 0x18011bb57
    int32_t       base     = 0;                   // read cursor into _draws

    while (count > 0) {
        const int32_t n = (count < chunkCap) ? count : chunkCap;
        count -= n;

        void*  cd   = Field<void*>(self->_context, kCtxContextData);
        void** cdvt = *reinterpret_cast<void***>(cd);
        const bool ok = reinterpret_cast<BeginVerticesFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(cdvt) + kCdBeginVertices))(
                cd, n * vertsPer, 0x20);
        if (g_log) g_log->Begin(n * vertsPer, 0x20, ok);

        if (!ok) {
            // 0x18011bc0d/0x18011bc0f: the chunk's records are SKIPPED and
            // EndVertices is NOT called. Faithful, including the skip.
            base += n;
            if (outMapFail) ++*outMapFail;
            continue;
        }

        for (int32_t i = 0; i < n; ++i) {
            // Both are re-read every iteration (0x18011bc40 / 0x18011bc44).
            const SpriteDraw* d = &self->_draws._data[base + i];
            void* sheet = self->_currentSheet;

            // --- the sheet UV, 0x18011bc4b..0x18011bc89 --------------------
            // `MOVSXD RAX,[rec+8]` then `TEST EAX,EAX / JNS`: a SIGNED test on
            // the 32-bit index; a negative index yields four zeros, not a read.
            float u0 = kZero, v0 = kZero, u1 = kZero, v1 = kZero;
            const int32_t idx = d->spriteIndex;
            if (idx >= 0) {
                const uint8_t* sprites = Field<uint8_t*>(sheet, kSheetSprites);
                const float* uv = reinterpret_cast<const float*>(
                    sprites + static_cast<int64_t>(idx) * kSpriteStride + kSpriteUv);
                u0 = uv[0]; v0 = uv[1]; u1 = uv[2]; v1 = uv[3];
            }

            const uint32_t color = d->color;
            if (outRecords) ++*outRecords;

            if (d->isLine != 0) {
                // --- the LINE path, 0x18011bcb3..0x18011bd9c ---------------
                // NOTE: no half-pixel subtraction here. The line path converts
                // the four ints and uses them raw (0x18011bceb/0x18011bcee is
                // CVTDQ2PS with no SUBSS). That asymmetry is retail's.
                if (outLines) ++*outLines;
                FlushVertex a, b;
                a.x = static_cast<float>(d->x1); a.y = static_cast<float>(d->y1);
                a.z = kZero; a.color = color;
                a.u = u0; a.v = v0; a.cu = kZero; a.cv = kZero;
                b.x = static_cast<float>(d->x2); b.y = static_cast<float>(d->y2);
                b.z = kZero; b.color = color;
                b.u = u1; b.v = v1; b.cu = kOne;  b.cv = kOne;
                Emit(self, a);
                Emit(self, b);
                continue;
            }

            // --- the SPRITE path: four corners, then six vertices ----------
            float ax, ay, bx, by, cx, cy, dx, dy;
            if (d->rotation == 0.0f) {
                // 0x18011bdb7..0x18011bea3. `UCOMISS / JP / JNZ` sends a NaN
                // rotation to the ROTATED path, and `rot == 0.0f` is false for
                // a NaN in C++ too, so no special case is needed.
                ax = static_cast<float>(d->x1) - halfPixel;
                ay = static_cast<float>(d->y1) - halfPixel;
                by = static_cast<float>(d->y2) - halfPixel;
                cx = static_cast<float>(d->x2) - halfPixel;
                bx = ax;  cy = by;  dx = cx;  dy = ay;
            } else {
                // 0x18011bea8..0x18011c46e. T(-centre) * R(angle) * T(+centre),
                // built as three real Transform2s. Not folded — see the header.
                if (outRotated) ++*outRotated;
                const float ang = d->rotation * kDegToRad;

                // `LEA EAX,[R14+RDI]` — a 32-bit INT sum, then CVTDQ2PS.
                const float sumX = static_cast<float>(d->x2 + d->x1);
                const float sumY = static_cast<float>(d->y2 + d->y1);

                // sinf FIRST (0x18011bf89), then cosf (0x18011bf94).
                const float s = RtLibm::Sin(ang);
                const float c = RtLibm::Cos(ang);
                // `XORPS XMM1,[0x1801617c0]` — a SIGN FLIP, so sin == +0.0f
                // becomes -0.0f, which unary minus reproduces exactly.
                const float ns = -s;

                const Transform2 t1 = { kOne, kZero, kZero, kOne,
                                        sumX * kNegHalf, sumY * kNegHalf };
                Transform2       rot = { c, s, ns, c, kZero, kZero };
                const Transform2 t2 = { kOne, kZero, kZero, kOne,
                                        sumX * kHalf, sumY * kHalf };
                if (defect == 3) { rot.m01 = ns; rot.m10 = s; }

                const Transform2 m1 = Transform2Mul(t1, rot);
                const Transform2 m  = Transform2Mul(m1, t2);

                // Each coordinate is converted ONCE (0x18011c18e / 0x18011c1bc
                // / 0x18011c245 / 0x18011c2a6) and reused across the corners.
                const float fx1 = static_cast<float>(d->x1);
                const float fy1 = static_cast<float>(d->y1);
                const float fx2 = static_cast<float>(d->x2);
                const float fy2 = static_cast<float>(d->y2);

                ax = TransformX(m, fx1, fy1) - halfPixel;
                ay = TransformY(m, fx1, fy1) - halfPixel;
                bx = TransformX(m, fx1, fy2) - halfPixel;
                by = TransformY(m, fx1, fy2) - halfPixel;
                cx = TransformX(m, fx2, fy2) - halfPixel;
                cy = TransformY(m, fx2, fy2) - halfPixel;
                dx = TransformX(m, fx2, fy1) - halfPixel;
                dy = TransformY(m, fx2, fy1) - halfPixel;
            }

            // The four corners and their two UV pairs. The CORNER uv is what
            // distinguishes them beyond position: A(0,0) B(0,1) C(1,1) D(1,0).
            FlushVertex A, B, C, D;
            A.x = ax; A.y = ay; A.z = kZero; A.color = color;
            A.u = u0; A.v = v0; A.cu = kZero; A.cv = kZero;
            B.x = bx; B.y = by; B.z = kZero; B.color = color;
            B.u = u0; B.v = v1; B.cu = kZero; B.cv = kOne;
            C.x = cx; C.y = cy; C.z = kZero; C.color = color;
            C.u = u1; C.v = v1; C.cu = kOne;  C.cv = kOne;
            D.x = dx; D.y = dy; D.z = kZero; D.color = color;
            D.u = u1; D.v = v0; D.cu = kOne;  D.cv = kZero;

            if (defect == 2) B.cv = kZero;

            // 0x18011c4ae..0x18011c58d: A, D, C, A, C, B — two triangles,
            // (TL,TR,BR) and (TL,BR,BL).
            if (defect == 1) {
                Emit(self, A); Emit(self, C); Emit(self, D);
                Emit(self, A); Emit(self, D); Emit(self, B);
            } else {
                Emit(self, A); Emit(self, D); Emit(self, C);
                Emit(self, A); Emit(self, C); Emit(self, B);
            }
        }
        base += n;

        // 0x18011c5c5..0x18011c5e3. `_context` and `+0x2160` are re-read.
        void*  cd2   = Field<void*>(self->_context, kCtxContextData);
        void** cdvt2 = *reinterpret_cast<void***>(cd2);
        const int32_t prim = isLine ? 2 : 0;
        // The oracle's fair comparison point for the dirty mask: the live
        // backend's EndVertices DRAWS and consumes the bits, the scratch
        // context's fake one never does, so sample before the call.
        if (g_tee) g_dirtyAtEnd = Field<uint32_t>(self->_context, kCtxDirtyMask);
        reinterpret_cast<EndVerticesFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(cdvt2) + kCdEndVertices))(
                cd2, prim);
        if (g_log) g_log->End(prim);
    }

    // --- 0x18011c674..0x18011c685 ------------------------------------------
    TheArrayClear()(&self->_draws);
    self->_currentSheet = nullptr;
}

// ---- effectiveness counters, printed by the report hook --------------------
uint64_t g_calls = 0, g_oracled = 0, g_skipNoSheet = 0, g_skipEmpty = 0;
uint64_t g_skipNoDevice = 0, g_skipNoRaw = 0, g_skipArena = 0;
uint64_t g_records = 0, g_lines = 0, g_rotated = 0, g_mapFail = 0, g_multiChunk = 0;
uint64_t g_harnessFindings = 0;

}  // namespace


// ============================================================================
// THE DRIVEN PROBE — BANISHED_RT_FLUSH_PROBE=1
// ============================================================================
// WHY IT EXISTS, AND THE MEASUREMENT THAT FORCED IT.
//
// The live oracle above is real and it is ZERO-EFFECTIVENESS on the driver the
// brief named. Measured, not assumed — the minimap cell
// (`BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=utility BANISHED_RT_DIFF=1`)
// reports:
//
//     UI::SpriteContext::Flush   calls=0  worked=0
//     [FlushOracle] calls=1340 oracled=0 | records=0
//     [FlushOracle] not oracled: noSheet=670 emptyBatch=670
//
// `Flush` is entered 1,340 times and the VERTEX LOOP RUNS ZERO TIMES. Half the
// calls arrive with `_currentSheet` null and half with an empty batch, because
// the only bodies that reach OUR `Flush` are the three lane UIF ported —
// `SetScissor`, `AddLine`, `FlushDeferred` — and everything the DLL's own
// `UserInterface::Draw` batches goes to the DLL's `Flush` through a direct
// CALL that no registry can redirect. So a live `divergences=0` from this port
// would be `0 of 0`: the exact shape of a port that reports clean because
// nothing called it (reference_uncalled_port_is_not_verified.md).
//
// The probe drives the body instead. Both sides run against SYNTHETIC objects
// — a scratch Context per side with the fake backend, a synthetic SpriteSheet,
// a synthetic sprite table, a synthetic batch — so neither reaches the GPU and
// the live frame is untouched. The one thing borrowed from the live object is
// the real `MaterialInstance*`, because `Setup` must be given a real material
// to walk; it writes nothing, since both scratch contexts are byte copies of a
// live context that has already had that same `Setup` applied to it.
//
// It runs ONCE, on the first `Flush` call that finds the device up and a real
// context and sheet — which is the earliest moment the shipped body can be
// called at all. Lane UIF's probe faulted at Runtime+0x11BA66 for running
// before that moment; this one is placed where the condition is already known
// to hold rather than tripwired after the fact.
// ============================================================================

namespace {

// Number of synthetic sprites in the fake sheet's table.
const int kProbeSprites = 8;

uint8_t g_probeSheet[0x40];
uint8_t g_probeSprites[kProbeSprites * 0x38];

bool     g_probeRan     = false;
int      g_probeCases   = 0;
int      g_probeDiv     = 0;
int      g_probeRecords = 0;
int      g_probeLines   = 0;
int      g_probeRotated = 0;
int      g_probeChunks  = 0;
int      g_probeMapFail = 0;
int      g_probeZeroUv  = 0;

// One side's run against its own scratch context and its own record copy.
struct SideResult {
    Arena       arena;      // its own ctor zeroes it
    CallLog     log;        // its own ctor zeroes it
    uint8_t*    ctx;
    SpriteDraw* recs;
    int32_t     sizeAfter;
    void*       sheetAfter;
    SideResult() : ctx(nullptr), recs(nullptr), sizeAfter(0), sheetAfter(nullptr) {}
};

void ProbeFreeSide(SideResult& r) {
    std::free(r.arena.base);
    std::free(r.ctx);
    std::free(r.recs);
}

// `useOurs == false` drives the SHIPPED body at 0x18011ba40 through the raw
// door — never through ResolveAddr, which now returns OUR body and would make
// the comparison true by construction
// (reference_recheck_must_match_its_own_proof.md).
bool ProbeRunSide(SideResult& r, bool useOurs, const SpriteContext& proto,
                  const uint8_t* ctxTemplate, const SpriteDraw* recs, int n,
                  uint32_t failMask, float halfPixel, int defect,
                  void (*theirsFn)(void*)) {
    const size_t need = static_cast<size_t>(n) * 6u * 32u + 64u;
    r.arena.base = static_cast<uint8_t*>(std::malloc(need));
    r.ctx        = static_cast<uint8_t*>(std::malloc(RtVideo::kContextSize));
    r.recs       = static_cast<SpriteDraw*>(
        std::malloc(sizeof(SpriteDraw) * static_cast<size_t>(n)));
    if (!r.arena.base || !r.ctx || !r.recs) return false;
    r.arena.used = 0;
    r.arena.cap  = need;
    std::memset(r.arena.base, 0xCD, need);
    std::memcpy(r.ctx, ctxTemplate, RtVideo::kContextSize);
    std::memcpy(r.recs, recs, sizeof(SpriteDraw) * static_cast<size_t>(n));

    FakeContextData fake;
    g_fakeVtable[kCdBeginVertices / 8] = reinterpret_cast<void*>(&FakeBeginVertices);
    g_fakeVtable[kCdEndVertices   / 8] = reinterpret_cast<void*>(&FakeEndVertices);
    fake.vtable   = g_fakeVtable;
    fake.context  = r.ctx;
    fake.arena    = &r.arena;
    fake.log      = &r.log;
    fake.chunk    = 0;
    fake.failMask = failMask;

    Field<void*>(r.ctx, kCtxContextData)     = &fake;
    Field<uint8_t*>(r.ctx, kCtxVertexCursor) = r.arena.base;

    SpriteContext scratch     = proto;
    scratch._context          = r.ctx;
    scratch._currentSheet     = g_probeSheet;
    scratch._draws._data      = r.recs;
    scratch._draws._size      = n;
    scratch._draws._capacity  = n;

    if (useOurs) {
        // NO TEE AND NO BODY-SIDE LOG HERE. Both sides run through the FAKE
        // backend in the probe, and the fake already records the arena advance
        // and the Begin/EndVertices arguments — so `g_tee`/`g_log` would count
        // OUR side twice and nobody's twice on theirs. The first run of this
        // probe did exactly that and reported six divergences whose whole
        // content was "ours 2 chunks, theirs 1"; the vertex BYTES were
        // byte-identical in all six. That is the instrument measuring itself
        // (reference_recheck_must_match_its_own_proof.md, one level down), and
        // it is recorded here rather than quietly deleted.
        //
        // The LIVE oracle in the entry point is the other case: there our body
        // runs against the REAL ContextData, which logs nothing, so it sets
        // both globals and the fake serves only the shipped side.
        FlushBody(&scratch, halfPixel, defect, nullptr, nullptr, nullptr, nullptr);
    } else {
        theirsFn(&scratch);
    }
    r.sizeAfter  = scratch._draws._size;
    r.sheetAfter = scratch._currentSheet;
    // If the shipped SetSize(0) had freed, this would not still be our buffer.
    if (scratch._draws._data != r.recs) {
        ++g_harnessFindings;
        std::fprintf(stderr, "[FlushProbe] HARNESS: SetSize(0) moved the record "
                             "pointer; lane UIF2 section 6's no-free proof is wrong.\n");
    }
    return true;
}

void ProbeCase(const char* name, const SpriteContext& proto, const uint8_t* ctxTemplate,
               const SpriteDraw* recs, int n, uint32_t failMask,
               float halfPixel, int defect, void (*theirsFn)(void*)) {
    SideResult a, b;
    if (!ProbeRunSide(a, true,  proto, ctxTemplate, recs, n, failMask,
                      halfPixel, defect, theirsFn) ||
        !ProbeRunSide(b, false, proto, ctxTemplate, recs, n, failMask,
                      halfPixel, 0, theirsFn)) {
        std::fprintf(stderr, "[FlushProbe] case '%s' SKIPPED: out of memory\n", name);
        ProbeFreeSide(a);
        ProbeFreeSide(b);
        return;
    }

    ++g_probeCases;
    g_probeRecords += n;
    for (int i = 0; i < n; ++i) {
        if (recs[i].isLine)          ++g_probeLines;
        else if (recs[i].rotation != 0.0f) ++g_probeRotated;
        if (recs[i].spriteIndex < 0) ++g_probeZeroUv;
    }
    if (a.log.n > 1) ++g_probeChunks;
    for (int i = 0; i < a.log.n && i < CallLog::kMax; ++i)
        if (!a.log.beginOk[i]) ++g_probeMapFail;

    bool        equal = true;
    const char* why   = "match";
    if (a.arena.used != b.arena.used)   { equal = false; why = "vertex BYTE COUNT"; }
    else if (std::memcmp(a.arena.base, b.arena.base, a.arena.used) != 0) {
        equal = false; why = "vertex BYTES";
    } else if (!LogsEqual(a.log, b.log)) {
        equal = false; why = "Begin/EndVertices ARGUMENTS";
    } else if (std::memcmp(&Field<int32_t>(a.ctx, kCtxScissor),
                           &Field<int32_t>(b.ctx, kCtxScissor), 16) != 0) {
        equal = false; why = "context SCISSOR";
    } else if (Field<uint32_t>(a.ctx, kCtxDirtyMask) !=
               Field<uint32_t>(b.ctx, kCtxDirtyMask)) {
        equal = false; why = "context DIRTY MASK";
    } else if (a.sizeAfter != b.sizeAfter || a.sheetAfter != b.sheetAfter) {
        equal = false; why = "_draws._size / _currentSheet";
    }

    if (!equal) {
        ++g_probeDiv;
        std::fprintf(stderr, "[FlushProbe] *** DIVERGENCE in case '%s': %s "
                             "(ours %d bytes / %d chunks, theirs %d bytes / %d chunks)\n",
                     name, why, (int)a.arena.used, a.log.n,
                     (int)b.arena.used, b.log.n);
        const size_t nv = (a.arena.used < b.arena.used ? a.arena.used : b.arena.used) / 32u;
        for (size_t i = 0; i < nv; ++i) {
            if (std::memcmp(a.arena.base + i * 32u, b.arena.base + i * 32u, 32u) != 0) {
                const FlushVertex* p = reinterpret_cast<const FlushVertex*>(a.arena.base + i * 32u);
                const FlushVertex* q = reinterpret_cast<const FlushVertex*>(b.arena.base + i * 32u);
                std::fprintf(stderr,
                    "[FlushProbe]   vertex %d: ours (%.9g,%.9g,%.9g) c=%08x uv=(%.9g,%.9g) "
                    "cuv=(%.9g,%.9g) | theirs (%.9g,%.9g,%.9g) c=%08x uv=(%.9g,%.9g) "
                    "cuv=(%.9g,%.9g)\n", (int)i,
                    (double)p->x, (double)p->y, (double)p->z, (unsigned)p->color,
                    (double)p->u, (double)p->v, (double)p->cu, (double)p->cv,
                    (double)q->x, (double)q->y, (double)q->z, (unsigned)q->color,
                    (double)q->u, (double)q->v, (double)q->cu, (double)q->cv);
                break;
            }
        }
    }

    ProbeFreeSide(a);
    ProbeFreeSide(b);
}

void FlushProbe(SpriteContext* live, float halfPixel, int defect) {
    typedef void (*FlushFn)(void*);
    FlushFn theirsFn = reinterpret_cast<FlushFn>(ResolveRuntimeRaw(0x18011ba40));
    if (!theirsFn) {
        std::fprintf(stderr, "[FlushProbe] SKIPPED: no raw door at 0x18011ba40 "
                             "(the DLL is not mapped).\n");
        return;
    }

    // The synthetic sheet: a real MaterialInstance (borrowed, read-only) and
    // our own sprite table. `Setup` needs the material; nothing needs the rest.
    std::memset(g_probeSheet, 0, sizeof(g_probeSheet));
    std::memset(g_probeSprites, 0, sizeof(g_probeSprites));
    Field<void*>(g_probeSheet, kSheetSprites)  = g_probeSprites;
    Field<void*>(g_probeSheet, kSheetMaterial) =
        Field<void*>(live->_currentSheet, kSheetMaterial);
    for (int i = 0; i < kProbeSprites; ++i) {
        float* uv = reinterpret_cast<float*>(g_probeSprites + i * kSpriteStride + kSpriteUv);
        uv[0] = 0.0f + 0.125f * static_cast<float>(i);          // u0
        uv[1] = 1.0f - 0.0625f * static_cast<float>(i);         // v0
        uv[2] = 0.125f * static_cast<float>(i) + 0.09375f;      // u1
        uv[3] = -0.5f + 0.25f * static_cast<float>(i);          // v1 (negative on purpose)
    }

    uint8_t* ctxTemplate = static_cast<uint8_t*>(std::malloc(RtVideo::kContextSize));
    if (!ctxTemplate) return;
    std::memcpy(ctxTemplate, live->_context, RtVideo::kContextSize);

    SpriteContext proto = *live;

    // ---- case 1: axis-aligned sprites, including NEGATIVE indices ----------
    // 512 of them, not a handful: this is the six-vertex fast path and the
    // whole of the sheet-UV / corner-UV assignment, and a dozen records would
    // be a shape check rather than a measurement.
    {
        const int n = 512;
        SpriteDraw* r = static_cast<SpriteDraw*>(
            std::malloc(sizeof(SpriteDraw) * static_cast<size_t>(n)));
        if (r) {
            std::memset(r, 0, sizeof(SpriteDraw) * static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                r[i].sheet       = g_probeSheet;
                // Every 37th record carries a negative index, which is retail's
                // "no sprite" and yields four zero UVs instead of a table read.
                r[i].spriteIndex = (i % 37 == 3) ? -(1 + (i % 5)) : (i % kProbeSprites);
                r[i].x1 = (i * 7919) % 4001 - 2000;
                r[i].y1 = (i * 6317) % 3001 - 1500;
                r[i].x2 = r[i].x1 + 1 + (i % 91);
                r[i].y2 = r[i].y1 - (i % 53);     // inverted rects on purpose
                r[i].rotation = 0.0f;
                r[i].color = 0xff102030u * static_cast<uint32_t>(i + 1) ^ 0x5a5a5a5au;
                r[i].isLine = 0;
            }
            ProbeCase("axis-aligned sprites (512, + negative indices)", proto,
                      ctxTemplate, r, n, 0, halfPixel, defect, theirsFn);
            std::free(r);
        }
    }

    // ---- case 2: ROTATED sprites, the transform chain ----------------------
    // 721 records sweeping -360..+360 degrees in half-degree steps. This is the
    // part of the port most likely to be subtly wrong — two 2x3 multiplies, a
    // sin/cos pair, and eight point transforms per record, every one of them a
    // float sum whose TERM ORDER had to be copied rather than re-derived
    // (reference_moved_term_hides_where_terms_cancel.md). A seven-angle sample
    // could not distinguish a correct chain from one that happens to agree at
    // the cardinal angles.
    {
        const int n = 721;
        SpriteDraw* r = static_cast<SpriteDraw*>(
            std::malloc(sizeof(SpriteDraw) * static_cast<size_t>(n)));
        if (r) {
            std::memset(r, 0, sizeof(SpriteDraw) * static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                r[i].sheet       = g_probeSheet;
                r[i].spriteIndex = (i % 29 == 5) ? -1 : (i % kProbeSprites);
                r[i].x1 = -40 + (i % 211) * 11;
                r[i].y1 = 17 - (i % 173) * 3;
                r[i].x2 = r[i].x1 + 64 + (i % 17);
                r[i].y2 = r[i].y1 + 48 - (i % 13);
                // -360.0 .. +360.0 in 0.5 steps. NOT a cardinal-angle sample.
                r[i].rotation = -360.0f + 0.5f * static_cast<float>(i);
                r[i].color = 0x80ffee00u ^ (static_cast<uint32_t>(i) << 8);
                r[i].isLine = 0;
            }
            ProbeCase("rotated sprites (721, -360..+360 by 0.5)", proto, ctxTemplate,
                      r, n, 0, halfPixel, defect, theirsFn);
            std::free(r);
        }
    }

    // ---- case 3: LINES — the two-vertex path and primitive type 2 ----------
    {
        SpriteDraw r[4];
        std::memset(r, 0, sizeof(r));
        for (int i = 0; i < 4; ++i) {
            r[i].sheet       = g_probeSheet;
            r[i].spriteIndex = (i == 1) ? -1 : (i % kProbeSprites);
            r[i].x1 = i * 5;         r[i].y1 = -i * 7;
            r[i].x2 = 300 - i * 9;   r[i].y2 = 200 + i;
            r[i].rotation = 0.0f;
            r[i].color = 0xffffffffu - static_cast<uint32_t>(i);
            r[i].isLine = 1;
        }
        ProbeCase("lines", proto, ctxTemplate, r, 4, 0, halfPixel, defect, theirsFn);
    }

    // ---- case 4: the MAP-FAILURE branch ------------------------------------
    // BeginVertices returns false for chunk 0. Retail skips the whole chunk and
    // does NOT call EndVertices — the one branch of the walk that a successful
    // run can never reach.
    {
        SpriteDraw r[3];
        std::memset(r, 0, sizeof(r));
        for (int i = 0; i < 3; ++i) {
            r[i].sheet       = g_probeSheet;
            r[i].spriteIndex = i;
            r[i].x1 = i; r[i].y1 = i; r[i].x2 = i + 10; r[i].y2 = i + 10;
            r[i].color = 0xdeadbeefu;
        }
        ProbeCase("BeginVertices FAILS", proto, ctxTemplate, r, 3, 0x1u,
                  halfPixel, defect, theirsFn);
    }

    // ---- case 5: MULTI-CHUNK -----------------------------------------------
    // The chunk cap is `vertsPer << 12`, so a LINE batch splits at 8,192
    // records — 8,193 lines costs two chunks and 524 KB of arena, where the
    // sprite equivalent (24,577) would cost 4.7 MB.
    {
        const int n = (2 << 12) + 1;
        SpriteDraw* r = static_cast<SpriteDraw*>(
            std::malloc(sizeof(SpriteDraw) * static_cast<size_t>(n)));
        if (r) {
            std::memset(r, 0, sizeof(SpriteDraw) * static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                r[i].sheet       = g_probeSheet;
                r[i].spriteIndex = i % kProbeSprites;
                r[i].x1 = i & 0x3ff;      r[i].y1 = (i >> 3) & 0x1ff;
                r[i].x2 = r[i].x1 + 4;    r[i].y2 = r[i].y1 + 1;
                r[i].color = 0x11223344u + static_cast<uint32_t>(i);
                r[i].isLine = 1;
            }
            ProbeCase("multi-chunk line batch (8193)", proto, ctxTemplate, r, n, 0,
                      halfPixel, defect, theirsFn);
            std::free(r);
        }
    }

    // ---- case 6: the SCISSOR PUSH, forced to differ ------------------------
    // Both sides start from the same context template, so the push either fires
    // on both or neither. Forcing the proto's scissor away from the template's
    // makes it fire, which is what puts +0x1e0..+0x1ec and the 0x400 dirty bit
    // under comparison instead of leaving them equal by never being written.
    {
        SpriteContext p2 = proto;
        p2._scissorX = Field<int32_t>(ctxTemplate, kCtxScissor + 0) + 17;
        p2._scissorY = Field<int32_t>(ctxTemplate, kCtxScissor + 4) - 3;
        p2._scissorW = Field<int32_t>(ctxTemplate, kCtxScissor + 8) + 1;
        p2._scissorH = Field<int32_t>(ctxTemplate, kCtxScissor + 12) + 2;
        SpriteDraw r[2];
        std::memset(r, 0, sizeof(r));
        for (int i = 0; i < 2; ++i) {
            r[i].sheet = g_probeSheet;
            r[i].spriteIndex = i;
            r[i].x1 = -100 * i; r[i].y1 = 50; r[i].x2 = 20; r[i].y2 = 90;
            r[i].rotation = (i == 1) ? 12.5f : 0.0f;
            r[i].color = 0x01020304u;
        }
        ProbeCase("scissor push fires", p2, ctxTemplate, r, 2, 0,
                  halfPixel, defect, theirsFn);
    }

    std::free(ctxTemplate);

    std::fprintf(stderr,
        "[FlushProbe] %d cases, %d divergences vs the shipped body.\n"
        "[FlushProbe] effectiveness: records=%d (lines=%d rotated=%d zeroUv=%d) "
        "multiChunkCases=%d mapFailChunks=%d | defect=%d\n",
        g_probeCases, g_probeDiv, g_probeRecords, g_probeLines, g_probeRotated,
        g_probeZeroUv, g_probeChunks, g_probeMapFail, defect);
    if (defect != 0 && g_probeDiv == 0)
        std::fprintf(stderr, "[FlushProbe] *** NEGATIVE CONTROL %d DID NOT FIRE — the "
                             "probe is not measuring what it claims. ***\n", defect);
}

bool ProbeWanted() {
    static int want = -1;
    if (want < 0) {
        const char* e = std::getenv("BANISHED_RT_FLUSH_PROBE");
        want = (e && *e && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return want != 0;
}

}  // namespace

// ============================================================================
// 0x18011ba40  private: void UI::SpriteContext::Flush()
// ============================================================================
void RtUi_SpriteContext_Flush(void* selfRaw) {
    RT_DIFF_BEGIN("UI::SpriteContext::Flush");
    SpriteContext* self = static_cast<SpriteContext*>(selfRaw);

    // 0x18011ba53..0x18011ba70, BEFORE the `_currentSheet` test. Retail derefs
    // the device global UNGUARDED and so does this port: the call below faults
    // exactly where retail's does if the singleton is not up yet. The null is
    // READ here only so the ORACLE can decline to run — see `wantOracle`. That
    // is not a null check standing in for a fix (CLAUDE.md); the deref stays
    // unguarded, and lane UIF's probe fault at Runtime+0x11BA66 is what a
    // caller that arrives too early looks like, on either side.
    void* const deviceData = VideoDeviceData();
    float       halfPixel  = DeviceHalfPixel(deviceData);

    ++g_calls;
    const int defect = FlushDefect();

    // NEGATIVE CONTROL 4. The shipped DX11 backend returns 0.0f here on every
    // call, so `- halfPixel` is an identity on the live path and a port that
    // dropped the term entirely would still compare equal. Forcing 0.5f into
    // OUR side only is what proves the term is wired; the oracle must diverge
    // (reference_probe_needs_an_effectiveness_control.md).
    if (defect == 4) halfPixel = kHalf;

    // The driven probe, once, at the first call that finds the device up and a
    // real context and sheet — see its banner for why the live oracle alone is
    // not evidence. It runs entirely on synthetic objects and returns before
    // this call's own work begins. It is placed AFTER the control-4 override so
    // that the control reaches the probe, which is the only place the vertex
    // loop actually runs; the first version of this patch put it before, and
    // the control reported "DID NOT FIRE" — the effectiveness control catching
    // the effectiveness control's own wiring.
    if (!g_probeRan && ProbeWanted() && deviceData &&
        self->_context && self->_currentSheet) {
        g_probeRan = true;
        FlushProbe(self, halfPixel, defect);
    }

    // ---- decide, BEFORE anything mutates, whether the oracle can run -------
    const int32_t recordCount = self->_draws._size;
    const bool    haveSheet   = (self->_currentSheet != nullptr);
    const bool    didWork     = haveSheet && recordCount > 0;
    bool          wantOracle  = rt_diff_on__ && didWork;

    // A call with no sheet, or with an empty batch, does no vertex work at all.
    // It is DECLARED as such rather than counted as coverage — a large `calls=`
    // is not coverage (rt_diff.h's did-work counter, and lane UIF §11.3, which
    // mis-read exactly this column in its own port).
    RT_DIFF_WORK(didWork);
    if (rt_diff_on__ && !haveSheet)            ++g_skipNoSheet;
    else if (rt_diff_on__ && recordCount == 0) ++g_skipEmpty;

    uint8_t*      ctxCopy = nullptr;
    SpriteDraw*   recCopy = nullptr;
    Arena         ours, theirs;
    CallLog       oursLog, theirsLog;
    SpriteContext scratch;
    int32_t       ctxScissorBefore[4] = { 0, 0, 0, 0 };
    uint32_t      ctxDirtyBefore = 0;

    typedef void (*FlushFn)(void*);
    static FlushFn theirsFn = nullptr;
    static bool    rawTried = false;
    if (wantOracle) {
        if (!rawTried) {
            rawTried = true;
            theirsFn = reinterpret_cast<FlushFn>(ResolveRuntimeRaw(0x18011ba40));
        }
        if (!theirsFn)   { wantOracle = false; ++g_skipNoRaw; }
        if (!deviceData) { wantOracle = false; ++g_skipNoDevice; }
    }

    if (wantOracle) {
        // Six vertices per record is the worst case, 32 bytes each. One arena
        // per side. A batch big enough to overflow the cap is not oracled, and
        // the skip is COUNTED rather than silently truncating a comparison.
        const size_t need = static_cast<size_t>(recordCount) * 6u * 32u;
        const size_t kArenaCap = 4u * 1024u * 1024u;
        if (need == 0 || need > kArenaCap) {
            wantOracle = false;
            ++g_skipArena;
        } else {
            ours.base   = static_cast<uint8_t*>(std::malloc(need));
            theirs.base = static_cast<uint8_t*>(std::malloc(need));
            ctxCopy     = static_cast<uint8_t*>(std::malloc(RtVideo::kContextSize));
            recCopy     = static_cast<SpriteDraw*>(
                std::malloc(sizeof(SpriteDraw) * static_cast<size_t>(recordCount)));
            if (!ours.base || !theirs.base || !ctxCopy || !recCopy) {
                wantOracle = false;
                ++g_skipArena;
            } else {
                ours.cap = theirs.cap = need;
                std::memset(ours.base,   0xCD, need);
                std::memset(theirs.base, 0xCD, need);
                // The context copy is taken NOW, before our body's Setup and
                // scissor push touch the live one — that is what makes the
                // shipped body's own Setup a no-op against it.
                std::memcpy(ctxCopy, self->_context, RtVideo::kContextSize);
                std::memcpy(recCopy, self->_draws._data,
                            sizeof(SpriteDraw) * static_cast<size_t>(recordCount));
                std::memcpy(ctxScissorBefore,
                            &Field<int32_t>(self->_context, kCtxScissor),
                            sizeof(ctxScissorBefore));
                ctxDirtyBefore = Field<uint32_t>(self->_context, kCtxDirtyMask);
                scratch = *self;
            }
        }
    }

    // ---- ours, for real ----------------------------------------------------
    int records = 0, lines = 0, rotated = 0, mapFail = 0;
    g_dirtyAtEnd = 0;
    if (wantOracle) { g_tee = &ours; g_log = &oursLog; }
    FlushBody(self, halfPixel, defect, &records, &lines, &rotated, &mapFail);
    g_tee = nullptr;
    g_log = nullptr;

    g_records += static_cast<uint64_t>(records);
    g_lines   += static_cast<uint64_t>(lines);
    g_rotated += static_cast<uint64_t>(rotated);
    g_mapFail += static_cast<uint64_t>(mapFail);
    if (oursLog.n > 1) ++g_multiChunk;

    if (!wantOracle) {
        std::free(ours.base);
        std::free(theirs.base);
        std::free(ctxCopy);
        std::free(recCopy);
        return;
    }

    // ---- theirs, against a scratch context that cannot reach the GPU -------
    FakeContextData fake;
    g_fakeVtable[kCdBeginVertices / 8] = reinterpret_cast<void*>(&FakeBeginVertices);
    g_fakeVtable[kCdEndVertices   / 8] = reinterpret_cast<void*>(&FakeEndVertices);
    fake.vtable  = g_fakeVtable;
    fake.context = ctxCopy;
    fake.arena    = &theirs;
    fake.log      = &theirsLog;
    fake.chunk    = 0;
    fake.failMask = 0;

    // Restore the copy's `Flush`-visible fields to their pre-call values and
    // swap in the fake backend. Everything else in the copy is the live
    // context's state as of before our body ran, which is what makes Setup and
    // SetupShaders find their values already equal and write nothing.
    std::memcpy(&Field<int32_t>(ctxCopy, kCtxScissor), ctxScissorBefore,
                sizeof(ctxScissorBefore));
    Field<uint32_t>(ctxCopy, kCtxDirtyMask)    = ctxDirtyBefore;
    Field<void*>(ctxCopy, kCtxContextData)     = &fake;
    Field<uint8_t*>(ctxCopy, kCtxVertexCursor) = theirs.base;

    scratch._context     = ctxCopy;
    scratch._draws._data = recCopy;

    theirsFn(&scratch);
    ++g_oracled;

    // ---- the verdict -------------------------------------------------------
    // A HARNESS finding, not a divergence: lane UIF §6 proves the shipped
    // `SetSize(0)` never frees, and this is the check that would notice if that
    // proof were wrong. It is reported separately so it can never be read as a
    // port defect.
    if (scratch._draws._data != recCopy) {
        ++g_harnessFindings;
        std::fprintf(stderr,
                     "[FlushOracle] HARNESS: the shipped Array<SpriteDraw>::SetSize(0) "
                     "MOVED the scratch data pointer (%p -> %p). Lane UIF2 section 6's "
                     "no-free proof is wrong and this oracle is unsound until that is "
                     "resolved.\n",
                     static_cast<void*>(recCopy),
                     static_cast<void*>(scratch._draws._data));
    }

    bool        equal = true;
    const char* why   = "vertices+calls+state";

    if (ours.used != theirs.used)  { equal = false; why = "vertex BYTE COUNT differs"; }
    else if (ours.used > ours.cap) { equal = false; why = "vertex count OVERFLOWED the arena"; }
    else if (std::memcmp(ours.base, theirs.base, ours.used) != 0) {
        equal = false; why = "vertex BYTES differ";
    } else if (!LogsEqual(oursLog, theirsLog)) {
        equal = false; why = "Begin/EndVertices ARGUMENTS differ";
    } else if (std::memcmp(&Field<int32_t>(self->_context, kCtxScissor),
                           &Field<int32_t>(ctxCopy, kCtxScissor), 16) != 0) {
        equal = false; why = "context SCISSOR differs";
    } else if (g_dirtyAtEnd != Field<uint32_t>(ctxCopy, kCtxDirtyMask)) {
        // `g_dirtyAtEnd`, not the live mask NOW: the live EndVertices drew and
        // cleared the bits (ours read 0 against theirs 0x109a2 on 61,994 of
        // 68,377 calls, 2026-09-12); the fake backend never draws.
        // Name the two masks and the pre-call one: a verdict that only says
        // "differs" cannot say which body set which bit.
        static char maskWhy[112];
        std::snprintf(maskWhy, sizeof(maskWhy),
                      "context DIRTY MASK differs (before=%08x ours=%08x theirs=%08x)",
                      (unsigned)ctxDirtyBefore,
                      (unsigned)g_dirtyAtEnd,
                      (unsigned)Field<uint32_t>(ctxCopy, kCtxDirtyMask));
        equal = false; why = maskWhy;
    } else if (self->_draws._size != scratch._draws._size) {
        equal = false; why = "_draws._size differs";
    } else if (self->_currentSheet != scratch._currentSheet) {
        equal = false; why = "_currentSheet differs";
    }

    if (!equal && ours.used == theirs.used && ours.used <= ours.cap) {
        // Name the first differing vertex rather than dumping bytes: a verdict
        // that cannot say WHICH term failed is not actionable
        // (reference_shadow_predicate_must_name_its_failing_term.md).
        const size_t nv = ours.used / 32u;
        for (size_t i = 0; i < nv; ++i) {
            if (std::memcmp(ours.base + i * 32u, theirs.base + i * 32u, 32u) != 0) {
                const FlushVertex* a =
                    reinterpret_cast<const FlushVertex*>(ours.base + i * 32u);
                const FlushVertex* b =
                    reinterpret_cast<const FlushVertex*>(theirs.base + i * 32u);
                std::fprintf(stderr,
                    "[FlushOracle] vertex %d of %d differs: "
                    "ours xy=(%.9g,%.9g) z=%.9g c=%08x uv=(%.9g,%.9g) cuv=(%.9g,%.9g) | "
                    "theirs xy=(%.9g,%.9g) z=%.9g c=%08x uv=(%.9g,%.9g) cuv=(%.9g,%.9g)\n",
                    (int)i, (int)nv,
                    (double)a->x, (double)a->y, (double)a->z, (unsigned)a->color,
                    (double)a->u, (double)a->v, (double)a->cu, (double)a->cv,
                    (double)b->x, (double)b->y, (double)b->z, (unsigned)b->color,
                    (double)b->u, (double)b->v, (double)b->cu, (double)b->cv);
                break;
            }
        }
    }

    {
        char oursDesc[96], theirsDesc[144];
        std::snprintf(oursDesc, sizeof(oursDesc), "%d vertex bytes, %d chunk(s)",
                      (int)ours.used, oursLog.n);
        std::snprintf(theirsDesc, sizeof(theirsDesc), "%d vertex bytes, %d chunk(s) [%s]",
                      (int)theirs.used, theirsLog.n, equal ? "match" : why);
        RT_DIFF_SHADOW(equal, oursDesc, theirsDesc);
    }

    std::free(ours.base);
    std::free(theirs.base);
    std::free(ctxCopy);
    std::free(recCopy);
}

// ============================================================================
// The effectiveness report. Printed alongside the RtDiff summary rather than
// inside it, because a `divergences=0` with no idea WHICH branches ran is the
// exact shape lane UIF's own §11.3 mis-read
// (reference_probe_needs_an_effectiveness_control.md).
// ============================================================================
void RtUiSpriteContextFlushReport() {
    if (!RtDiffEnabled()) return;
    std::fprintf(stderr,
        "[FlushOracle] calls=%llu oracled=%llu | records=%llu (lines=%llu rotated=%llu) "
        "chunks>1=%llu mapFail=%llu\n"
        "[FlushOracle] not oracled: noSheet=%llu emptyBatch=%llu noDevice=%llu "
        "noRawDoor=%llu arena=%llu | harnessFindings=%llu | defect=%d\n",
        (unsigned long long)g_calls, (unsigned long long)g_oracled,
        (unsigned long long)g_records, (unsigned long long)g_lines,
        (unsigned long long)g_rotated, (unsigned long long)g_multiChunk,
        (unsigned long long)g_mapFail,
        (unsigned long long)g_skipNoSheet, (unsigned long long)g_skipEmpty,
        (unsigned long long)g_skipNoDevice, (unsigned long long)g_skipNoRaw,
        (unsigned long long)g_skipArena, (unsigned long long)g_harnessFindings,
        FlushDefect());
    if (ProbeWanted())
        std::fprintf(stderr, "[FlushOracle] probe: ran=%d cases=%d divergences=%d\n",
                     g_probeRan ? 1 : 0, g_probeCases, g_probeDiv);
}

// ============================================================================
int RtUiSpriteContextFlushRegister() {
    // *** THIS CHANGES BEHAVIOUR AT EXISTING CALL SITES. ***
    // spritecontext.cpp's `TheFlush()` resolves 0x18011ba40 through
    // `ResolveAddr`, which consults the port registry FIRST
    // (reference_resolveaddr_consults_port_registry_first.md). From here on
    // `AddLine`, both `SetScissor`s and `FlushDeferred` reach OUR body. That is
    // the point of the lane: it is lane UIF's one opened address door closing.
    RegisterRuntimeAddr(0x18011ba40, (void*)&RtUi_SpriteContext_Flush);
    return 1;
}
