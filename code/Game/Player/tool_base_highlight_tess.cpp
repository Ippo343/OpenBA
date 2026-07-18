// code/Game/Player/tool_base_highlight_tess.cpp
// TradeWork_impl_ace40 (0x1400ace40) — the radius / tiled-decal tile tessellation
// builder. Shared by BOTH the PlacementTool radius-highlight overlay (via
// Player_HighlightAdd_55430 -> TradeWork_impl_0d80) AND the building ground-decal
// path (DecalComponent::Draw -> TradeWork_impl_0d80). It builds the vertex + index
// buffers for one tile-renderer slot (gameWorld+0x110, slot = base+0x1f0 + idx*0x60),
// deduping shared corner vertices through an AVL BST keyed by a 0x18-byte descriptor.
//
// Promoted from notes/wave5_drafts/ace40.draft.cpp after end-to-end integration for
// the terrain-decal bring-up (BANISHED_LIVE_DECAL). FAITHFUL 1:1 PORT of the Ghidra
// decompile, cross-checked against disassembly for every float lane, CONCAT pack,
// dropped-arg call, and field offset. Do NOT "improve" — a wrong port here is SILENT
// GEOMETRY CORRUPTION.
//
// STANDALONE-ONLY: like the rest of tool_base_highlight.cpp, this reimpl is reached
// only via RegisterExeAddr. ResolveAddr on a .rdata constant returns NULL in standalone
// (see reference_standalone_rdata_null_fallback), so the .rdata constants below are
// embedded here as their retail values.
//
// Slot layout (lVar16 = param_1+0x1f0 + param_2*0x60):
//   +0x08 vertex-buffer  {void* base, int count, int cap}        (plVar30 == lVar16+8)
//   +0x18 index-buffer   {void* base @+0x18, int count @+0x20, int cap @+0x24}
//   +0x2c minX  +0x30 minY  +0x34 maxX  +0x38 maxY  +0x40 yawDeg(int)
//   +0x3c byte (passed to cc10)  +0x44 char modeFlag (cVar1)
//   +0x48..0x58 float UV params (when modeFlag==0)

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ---- engine .rdata constants (retail values; see header) -------------------------
namespace {
constexpr float    kDAT_140172260 =  4096.0f;    // 0x45800000  (UV tile-size const)
constexpr float    kDAT_140172280 = -1.0f;       // 0xbf800000
constexpr uint32_t kDAT_140172114 = 0x3f000000;  // 0.5f packed bits (uVar8 seed)
constexpr uint32_t kDAT_1401722c0 = 0x80000000;  // XORPS sign-flip mask (the negate)
constexpr uint8_t  kDAT_1401c23cf = 0x2e;        // base index byte (iVar13 seed = 46)
}  // namespace

// ---- ported-alongside AVL helpers (tool_base_highlight.cpp) ----------------------
extern "C" int64_t* TradeWork_impl_1f6b0(int64_t* root, int64_t* outNode, uint32_t* key);
extern "C" int64_t  TradeWork_impl_1fc00(int64_t* root, int64_t key);
extern "C" void     TradeWork_helper_14b00(int64_t ctx, void* node);

namespace {

// ---- already-registered exe sub-callees ------------------------------------------
typedef void (*Cc10_t)(int64_t self, int x, int y, int64_t outDesc, int variant, uint8_t b3c);
typedef int  (*E180_t)(uint64_t a0, char a1, char a2, char a3, char a4,
                       char a5, char a6, char a7, char a8);
typedef void (*Ee40_t)(int64_t* vb, int count);
typedef void (*Pool8d0_t)(int64_t* pool, int newCount, void* rec);

// ---- static MemAlloc/MemFree (slot 0x140152b60/0x140152b68) -----------------------
typedef void* (*MemAllocStatic_t)(uint64_t size, uint32_t align);  // SAPEAXII
typedef void  (*MemFreeStatic_t)(void* ptr);                       // SAXPEAX
MemAllocStatic_t s_memAlloc = nullptr;
MemFreeStatic_t  s_memFree  = nullptr;
bool s_memInit = false;
void EnsureMem() {
    if (s_memInit) return;
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x140152b60));
    if (!s_memAlloc) s_memAlloc = reinterpret_cast<MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));  // (uint64 size, int align)
    s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x140152b68));
    if (!s_memFree) s_memFree = reinterpret_cast<MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_memInit = true;
}
inline void* MemAlloc(uint64_t sz, uint32_t al) { return s_memAlloc ? s_memAlloc(sz, al) : nullptr; }
inline void  MemFree(void* p) { if (s_memFree && p) s_memFree(p); }

// ---- resolved sub-callee fn-ptrs --------------------------------------------------
Cc10_t    fnCc10 = nullptr;
E180_t    fnE180 = nullptr;
Ee40_t    fnEe40 = nullptr;
Pool8d0_t fnPool = nullptr;
bool s_fnInit = false;
void EnsureFns() {
    if (s_fnInit) return;
    fnCc10 = reinterpret_cast<Cc10_t>(ResolveAddr(Module::Exe, 0x1400acc10));
    fnE180 = reinterpret_cast<E180_t>(ResolveAddr(Module::Exe, 0x1400ae180));
    fnEe40 = reinterpret_cast<Ee40_t>(ResolveAddr(Module::Exe, 0x14001ee40));
    fnPool = reinterpret_cast<Pool8d0_t>(ResolveAddr(Module::Exe, 0x14001f8d0));
    s_fnInit = true;
}

// ---- engine Transform2/Vector2 op typedefs (via .rdata dispatch slots) ------------
typedef void* (*Vec2Ctor_t)(void* self);                                   // 0x140151e58
typedef void* (*T2FromScalarVec_t)(void* self, void* scalar, void* vec);   // 0x140152648
typedef void* (*T2FromYaw_t)(void* self, void* scalar, void* vec);         // 0x140152650
// operator*(Transform2): 3-arg member form: RCX=acc(lhs), RDX=outBuf, R8=rhs -> RDX.
typedef void* (*T2MulT2_t)(void* acc, void* outBuf, void* rhs);            // 0x140152640
// operator*(Vector2): RCX=this(Transform2), RDX=out Vector2, R8=in Vector2 -> RDX.
typedef void* (*T2MulVec_t)(void* self, void* outVec, void* inVec);        // 0x140152658

Vec2Ctor_t        opVec2Ctor = nullptr;
T2FromScalarVec_t opT2Scalar = nullptr;
T2FromYaw_t       opT2YawOff = nullptr;
T2MulT2_t         opT2MulT2  = nullptr;
T2MulVec_t        opT2MulVec = nullptr;
bool s_opInit = false;
void EnsureOps() {
    if (s_opInit) return;
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    opVec2Ctor = reinterpret_cast<Vec2Ctor_t>(slot(0x140151e58));
    opT2Scalar = reinterpret_cast<T2FromScalarVec_t>(slot(0x140152648));
    opT2YawOff = reinterpret_cast<T2FromYaw_t>(slot(0x140152650));
    opT2MulT2  = reinterpret_cast<T2MulT2_t>(slot(0x140152640));
    opT2MulVec = reinterpret_cast<T2MulVec_t>(slot(0x140152658));
    s_opInit = true;
}

// AppendIndex — inlined "append one short index to the growable index buffer at
// slot+0x18/+0x20/+0x24" block (orig emits it verbatim per triangle vertex).
inline void AppendIndex(int64_t lVar16, uint16_t idx) {
    int  iVar13 = *reinterpret_cast<int*>(lVar16 + 0x20);   // old count
    int  iVar25 = iVar13 + 1;                               // new count
    if (iVar25 == iVar13) return;
    void* pvVar7 = *reinterpret_cast<void**>(lVar16 + 0x18); // old base
    *reinterpret_cast<int*>(lVar16 + 0x20) = iVar25;
    if (*reinterpret_cast<int*>(lVar16 + 0x24) < iVar25) {
        *reinterpret_cast<int*>(lVar16 + 0x24) = iVar25;
        if (iVar13 != 0) *reinterpret_cast<int*>(lVar16 + 0x24) = iVar25 * 3;
        void* pvVar6 = MemAlloc((int64_t)*reinterpret_cast<int*>(lVar16 + 0x24) * 2, 2);
        *reinterpret_cast<void**>(lVar16 + 0x18) = pvVar6;
    }
    int iVar11 = 0;
    if (pvVar7 == *reinterpret_cast<void**>(lVar16 + 0x18)) iVar11 = iVar13;
    for (int64_t lv = iVar11; lv < iVar25; ++lv) {
        uint16_t* puVar19 =
            reinterpret_cast<uint16_t*>(*reinterpret_cast<int64_t*>(lVar16 + 0x18) + lv * 2);
        if (puVar19 != nullptr) {
            uint16_t uVar27 = (lv < iVar13)
                ? *reinterpret_cast<uint16_t*>((int64_t)pvVar7 + lv * 2)
                : idx;
            *puVar19 = uVar27;
        }
    }
    if (pvVar7 != *reinterpret_cast<void**>(lVar16 + 0x18)) MemFree(pvVar7);
}

// EmitVertex — resolve ONE corner descriptor to a vertex index (find/insert/append),
// then append that index to the index buffer.
inline void EmitVertex(int64_t lVar16, int64_t* poolRoot, int64_t* pool,
                       uint32_t* desc, int64_t* outNode) {
    int64_t node = TradeWork_impl_1fc00(poolRoot, (int64_t)desc);   // 0x14001fc00
    uint16_t idx;
    if (node == 0) {
        int curCount = *reinterpret_cast<int*>((char*)pool + 8);    // pool.count
        uint32_t rec[6];
        rec[0] = desc[0]; rec[1] = desc[1]; rec[2] = desc[2];
        rec[3] = desc[3]; rec[4] = desc[4];
        rec[5] = (uint32_t)curCount;          // CONCAT44((int)local_2c8, local_268)
        idx = (uint16_t)curCount;
        // CRITICAL argument split (matches ace40 disasm exactly): the BST insert stores
        // `rec` — the 6-int key whose +0x14 slot is the vertex INDEX — so that a later
        // dedup HIT reads back the correct index via node+0x14. The vertex POOL stores the
        // raw `desc` (position + UVs). Passing these swapped makes node+0x14 hold garbage
        // descriptor bytes; every shared-corner dedup hit then returns a wild index and the
        // triangle stretches off toward the bogus vertex (the "sliver to map center" bug).
        TradeWork_impl_1f6b0(poolRoot, outNode, rec);                  // 0x14001f6b0 (key = rec)
        if (fnPool) fnPool(pool, curCount + 1, desc);                  // 0x14001f8d0 (vertex = desc)
    } else {
        idx = *reinterpret_cast<uint16_t*>(node + 0x14);
    }
    AppendIndex(lVar16, idx);
}

} // namespace

// =================================================================================
// TradeWork_impl_ace40 — 0x1400ace40
// =================================================================================
extern "C" void TradeWork_impl_ace40(int64_t param_1, int param_2, char param_3,
                                     int param_4, int param_5) {
    EnsureMem(); EnsureFns(); EnsureOps();

    const float fUV = (g_fUVScale ? *g_fUVScale : 1.0f);   // g_fUVScale (1.0f in retail)

    int64_t lVar16 = (int64_t)param_2 * 0x60 + *reinterpret_cast<int64_t*>(param_1 + 0x1f0);
    int iVar24 = (*reinterpret_cast<int*>(lVar16 + 0x34) - *reinterpret_cast<int*>(lVar16 + 0x2c)) + 1; // width
    int iVar28 = (*reinterpret_cast<int*>(lVar16 + 0x38) - *reinterpret_cast<int*>(lVar16 + 0x30)) + 1; // height

    // four scratch Vector2 (local_1d8..local_1c0); ctor x4.
    uint8_t cornerBuf[0x20];
    {
        uint8_t* p = cornerBuf;
        for (int i = 3; i >= 0; --i) { if (opVec2Ctor) opVec2Ctor(p); p += 8; }
    }

    float fVar3 = fUV;
    char  cVar1 = *reinterpret_cast<char*>(lVar16 + 0x44);   // modeFlag

    uint32_t uVar8 = 0;
    float fVar33 = 0.0f;
    float fVar31, fVar32, fVar34, fVar35, fVar36;
    float local_288, fStack_284;
    float local_290_hi;

    if (cVar1 == '\0') {
        float fVar_div = *reinterpret_cast<float*>(lVar16 + 0x48);
        local_288 = 0.0f;
        fVar33 = *reinterpret_cast<float*>(lVar16 + 0x4c) / fVar_div;
        fVar31 = (*reinterpret_cast<float*>(lVar16 + 0x54) - *reinterpret_cast<float*>(lVar16 + 0x4c)) / fVar_div;
        fVar32 = (*reinterpret_cast<float*>(lVar16 + 0x58) - *reinterpret_cast<float*>(lVar16 + 0x50)) / fVar_div;
        fVar34 = *reinterpret_cast<float*>(lVar16 + 0x50) / fVar_div;
        fVar35 = kDAT_140172260;
        fStack_284 = fUV;
        local_290_hi = kDAT_140172280;
        fVar36 = fUV;
        uVar8 = kDAT_140172114;
    } else {
        fVar36 = (float)iVar24;
        local_290_hi = (float)iVar28;
        local_288 = (float)*reinterpret_cast<int*>(lVar16 + 0x2c);
        fStack_284 = (float)*reinterpret_cast<int*>(lVar16 + 0x30);
        fVar31 = fUV;
        fVar32 = fUV;
        fVar34 = fVar33;     // 0
        fVar35 = fUV;
    }

    int   local_2a0 = 0;
    float local_29c = (float)*reinterpret_cast<int*>(lVar16 + 0x40);
    int   local_2b0_0 = 0;

    uint64_t local_208 = (uint64_t)(uint32_t&)fVar35;
    uint64_t local_200 = (uint64_t)(uint32_t&)fVar35 << 0x20;
    uint64_t local_1f8 = 0;

    uint64_t local_280 = (uint64_t)(uint32_t&)fVar31;
    uint64_t local_220 = local_280;
    uint64_t local_218 = (uint64_t)(uint32_t&)fVar32 << 0x20;
    uint64_t local_210 = ((uint64_t)(uint32_t&)fVar34 << 32) | (uint32_t&)fVar33;
    uint64_t local_228 = ((uint64_t)uVar8 << 32) | uVar8;

    float local_298[2] = {0.0f, 0.0f};

    uint32_t negSeed = uVar8 ^ kDAT_1401722c0;
    uint64_t local_2a8 = ((uint64_t)negSeed << 32) | negSeed;

    uint32_t local_290_lo = 0;
    local_280 = (uint64_t)(uint32_t&)fVar36;
    uint64_t local_2e8 = local_280;
    uint64_t uStack_2e0 = (uint64_t)(uint32_t&)local_290_hi << 0x20;
    uint64_t local_2d8 = ((uint64_t)(uint32_t&)fStack_284 << 32) | (uint32_t&)local_288;

    // Transform2 chain (orig slots 0x140152648/650/640). Each Transform2 is 0x18 bytes
    // (6 floats: [m00, m01, m10, m11, tx, ty]).
    uint8_t T_138[0x18], T_168[0x18], T_108[0x18];
    uint8_t T_160[0x18], T_148[0x18], T_118[0x18], T_e8[0x18], T_1f0[0x18];

    // The three "raw data" operands of the operator* chain. CRITICAL: these MUST be
    // contiguous 0x18-byte Transform2 buffers. The prior port built them from separate,
    // non-adjacent stack locals (local_2e8/uStack_2e0/local_2d8, local_220/218/210,
    // local_208/200/1f8) and — worse — passed only &local_2e8/&local_2d8 into the chain,
    // DROPPING the uv-rect and the 4096 scale entirely. Result: T_1f0 had neither the
    // uv-rect mapping nor the ×4096 texel scale, so every per-tile UV came out as the raw
    // 0..1 value and truncated to int16 (0, 0/1) -> the decal sampled texel (0,0) (the empty
    // corner of the dirt-splat atlas) -> transparent black -> invisible. Rebuilt as real
    // contiguous buffers and wired EXACTLY per the ace40 disassembly at 0x1400ad0d0..0ad127:
    //   T_1f0 = ((((flip * r108) * r168) * r138) * uvRect) * scale
    // Layout confirmed from the disasm store pattern (e.g. RBP+0x18 = [fVar31,0,0,fVar32,
    // fVar33,fVar34] = uvRect; RBP+0x30 = [fVar35,0,0,fVar35,0,0] = scale; RSP+0x50 = flip).
    float TF_flip[6]   = { fVar36, 0.0f, 0.0f, local_290_hi, local_288, fStack_284 };
    float TF_uvRect[6] = { fVar31, 0.0f, 0.0f, fVar32,       fVar33,    fVar34    };
    float TF_scale[6]  = { fVar35, 0.0f, 0.0f, fVar35,       0.0f,      0.0f      };

    // FromYawOffset (0x140152650) reads an 8-byte (Vector2) OFFSET from R8. The disasm
    // passes a dedicated zeroed slot ([RBP-0x60] = MOV qword,0 → {0,0}); we must give it a
    // real 8-byte zero Vector2 — NOT &local_2a0 (a lone int), whose second float lane reads a
    // garbage adjacent stack local. That lane held -1.0, injecting a translate(0,-1) that
    // shifted every decal's diffuse V down by one uv-cell height (0.15625): the barn survived
    // (v0=0.844 → 0.688, still positive) but top-row cells (v0=0) folded NEGATIVE → sampled
    // off-atlas → transparent → houses/stockpiles invisible. Verified via [ace40::op] trace:
    // with {0,0} r168 offset, decal V now maps positive/in-atlas (barn f5=3456=0.84375·4096).
    float yawOffset[2] = { 0.0f, 0.0f };   // disasm [RBP-0x60] clean {0,0}
    void* r138 = nullptr; void* r168 = nullptr; void* r108 = nullptr;
    if (opT2Scalar) r138 = opT2Scalar(T_138, &local_2a0, &local_228);          // 0x140152648 (ctor1)
    if (opT2YawOff) r168 = opT2YawOff(T_168, &local_29c, yawOffset);           // 0x140152650 (yawOff)
    if (opT2Scalar) r108 = opT2Scalar(T_108, &local_2b0_0, &local_2a8);        // 0x140152648 (ctor3)

    if (opT2MulT2) {
        void* acc;
        acc = opT2MulT2(TF_flip, T_160, r108);   // 0x140152640  flip   * ctor3(r108)
        acc = opT2MulT2(acc, T_148, r168);        //              (..)   * yawOff(r168)
        acc = opT2MulT2(acc, T_118, r138);        //              (..)   * ctor1(r138)
        acc = opT2MulT2(acc, T_e8,  TF_uvRect);   //              (..)   * uvRect
        acc = opT2MulT2(acc, T_1f0, TF_scale);    //              (..)   * scale  -> T_1f0
        (void)acc;
    }

    // ── One-shot operand dump (BANISHED_DECAL_TRACE) — pin the V-negation bug ────────
    // The retail decal's diffuse V is POSITIVE (+0.156) but ours emits NEGATIVE int16 V
    // (-640). operator*(Vector2/Transform2) + the chain are verified faithful, so a single
    // operand float must differ. Dump flip/uvRect/scale + the composed T_1f0 (all as the
    // [f0,f1,f2,f3,f4,f5] = {basisX,basisY,trans} layout) for the first few tiled decals
    // (cVar1==0). Expected for a variant-2 house: flip=[1,0,0,-1,0,1], uvRect≈[0.125,0,0,
    // 0.156,0.25,0]; T_1f0*corner must give POSITIVE V. Whichever is off is the bug.
    {
        static int s_opDump = -1;
        if (s_opDump < 0) { const char* e = getenv("BANISHED_DECAL_TRACE");
                            s_opDump = (e && *e && e[0] != '0') ? 0 : 999; }
        if (s_opDump < 6 && cVar1 == '\0') {
            ++s_opDump;
            const float* t1f0 = reinterpret_cast<const float*>(T_1f0);
            const float* p108 = reinterpret_cast<const float*>(T_108);   // ctor3 (−0.5 seed)
            const float* p168 = reinterpret_cast<const float*>(T_168);   // yawOff (rotation)
            const float* p138 = reinterpret_cast<const float*>(T_138);   // ctor1 (+0.5 seed)
            const float div = *reinterpret_cast<float*>(lVar16 + 0x48);
            fprintf(stderr,
                "[ace40::op] desc=%p div=%.1f uvRaw=(%.1f,%.1f,%.1f,%.1f) fUV=%.3f yawDeg=%.2f\n"
                "[ace40::op]   TF_flip  =[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]\n"
                "[ace40::op]   TF_uvRect=[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]\n"
                "[ace40::op]   r108(-.5)=[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]\n"
                "[ace40::op]   r168(yaw)=[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]\n"
                "[ace40::op]   r138(+.5)=[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]\n"
                "[ace40::op]   T_1f0    =[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]  (out.y=x*f1+y*f3+f5)\n",
                reinterpret_cast<void*>(lVar16), div,
                *reinterpret_cast<float*>(lVar16 + 0x4c), *reinterpret_cast<float*>(lVar16 + 0x50),
                *reinterpret_cast<float*>(lVar16 + 0x54), *reinterpret_cast<float*>(lVar16 + 0x58),
                fUV, local_29c,
                TF_flip[0], TF_flip[1], TF_flip[2], TF_flip[3], TF_flip[4], TF_flip[5],
                TF_uvRect[0], TF_uvRect[1], TF_uvRect[2], TF_uvRect[3], TF_uvRect[4], TF_uvRect[5],
                p108[0], p108[1], p108[2], p108[3], p108[4], p108[5],
                p168[0], p168[1], p168[2], p168[3], p168[4], p168[5],
                p138[0], p138[1], p138[2], p138[3], p138[4], p138[5],
                t1f0[0], t1f0[1], t1f0[2], t1f0[3], t1f0[4], t1f0[5]);
            fflush(stderr);
        }
    }

    // 4 unit-quad corners via operator*(Vector2). Packed corner cycles {0,0},{1,0},{1,1},{0,1}.
    uint64_t* corner = reinterpret_cast<uint64_t*>(cornerBuf);
    auto mulVec = [&](uint64_t packedCorner) -> uint64_t {
        local_2a8 = packedCorner;
        if (!opT2MulVec) return 0;
        uint64_t* r = reinterpret_cast<uint64_t*>(opT2MulVec(T_1f0, local_298, &local_2a8));
        return *r;
    };
    corner[0] = mulVec(0x0);                   // {0,0}
    corner[1] = mulVec(0x3f800000ULL);         // {1,0}
    corner[2] = mulVec(0x3f8000003f800000ULL); // {1,1}
    corner[3] = mulVec(0x3f80000000000000ULL); // {0,1}

    // dedup pool + BST.
    struct VPool { int64_t base; int32_t count; int32_t cap; } pool = {0, 0, 0};
    struct BstRoot { int64_t root; int32_t count; } bst = {0, 0};
    int64_t* poolRootHolder = reinterpret_cast<int64_t*>(&bst);

    int iVar13 = (int)kDAT_1401c23cf;
    fVar33 = (float)((*reinterpret_cast<int*>(lVar16 + 0x34) + *reinterpret_cast<int*>(lVar16 + 0x2c)) >> 1);
    int iVar25 = (*reinterpret_cast<int*>(lVar16 + 0x38) + *reinterpret_cast<int*>(lVar16 + 0x30)) >> 1;
    int64_t* plVar30 = reinterpret_cast<int64_t*>(lVar16 + 8);
    local_2b0_0 = iVar13;
    local_2a0   = iVar25;
    local_29c   = fVar33;
    local_228   = (uint64_t)plVar30;

    // free + reset the slot's vertex and index buffers.
    MemFree(reinterpret_cast<void*>(*plVar30));
    *plVar30 = 0;
    *reinterpret_cast<int64_t*>(lVar16 + 0x10) = 0;
    void** ppvVar9 = *reinterpret_cast<void***>(lVar16 + 0x18);
    MemFree(ppvVar9);
    *reinterpret_cast<int64_t*>(lVar16 + 0x18) = 0;
    *reinterpret_cast<int64_t*>(lVar16 + 0x20) = 0;

    int64_t outNode;

    // main tile loop (y = minY..maxY, x = minX..maxX).
    int iVar18 = *reinterpret_cast<int*>(lVar16 + 0x30);
    if (iVar18 <= *reinterpret_cast<int*>(lVar16 + 0x38)) {
        do {
            int iVar22 = *reinterpret_cast<int*>(lVar16 + 0x2c);
            int iVar11 = *reinterpret_cast<int*>(lVar16 + 0x34);
            if (iVar22 <= iVar11) {
                do {
                    bool doTile;
                    if (param_3 == '\0') {
                        doTile = true;
                    } else {
                        uint32_t dist = (uint32_t)((iVar18 - iVar25) * (iVar18 - iVar25) +
                                                   (iVar22 - (int)fVar33) * (iVar22 - (int)fVar33));
                        doTile = (param_4 * param_4 <= (int)dist) && ((int)dist <= param_5 * param_5);
                    }

                    if (doTile) {
                        if (cVar1 != '\0') {
                            int iVar25b = *reinterpret_cast<int*>(lVar16 + 0x2c);
                            int iVar13b = iVar22 - 1;
                            int iVar14  = iVar22 + 1;
                            char cVar26, cVar23, cVar17, cVar12;
                            cVar26 = ((iVar13b < iVar25b) && (*reinterpret_cast<int*>(lVar16 + 0x38) < iVar18 + 1)) ? '\0' : '\x01';
                            cVar23 = ((iVar11   < iVar14 ) && (*reinterpret_cast<int*>(lVar16 + 0x38) < iVar18 + 1)) ? '\0' : '\x01';
                            cVar17 = ((iVar13b < iVar25b) && (iVar18 - 1 < *reinterpret_cast<int*>(lVar16 + 0x30))) ? '\0' : '\x01';
                            cVar12 = ((iVar11   < iVar14 ) && (iVar18 - 1 < *reinterpret_cast<int*>(lVar16 + 0x30))) ? '\0' : '\x01';
                            bool bVar2 = iVar18 + 1 <= *reinterpret_cast<int*>(lVar16 + 0x38);
                            if (fnE180) iVar13 = fnE180(
                                (uint64_t)((uintptr_t)ppvVar9 & ~0xffULL) | (uint8_t)bVar2,
                                (char)(iVar25b <= iVar13b), (char)(iVar14 <= iVar11),
                                (char)(*reinterpret_cast<int*>(lVar16 + 0x30) <= iVar18 - 1),
                                (char)bVar2, cVar12, cVar17, cVar23, cVar26);
                            local_2b0_0 = iVar13;
                        }

                        // cc10: emit the 4 corner descriptors into descBlock.
                        uint8_t descBlock[0x60];
                        if (fnCc10) fnCc10(param_1, iVar22, iVar18, (int64_t)descBlock, iVar13,
                                           *reinterpret_cast<uint8_t*>(lVar16 + 0x3c));

                        // per-tile UV: 4 corners, write short pos pair into descBlock.
                        {
                            int32_t xOff[4]; int32_t yOff[4];
                            *reinterpret_cast<uint64_t*>(&xOff[0]) = 0x100000000ULL; // {0,1}
                            *reinterpret_cast<uint64_t*>(&xOff[2]) = 0x1ULL;          // {1,0}
                            *reinterpret_cast<uint64_t*>(&yOff[0]) = 0x0ULL;          // {0,0}
                            *reinterpret_cast<uint64_t*>(&yOff[2]) = 0x100000001ULL;  // {1,1}
                            int16_t* puVar19 = reinterpret_cast<int16_t*>(descBlock + 0xA);
                            for (int64_t k = 0; k < 4; ++k) {
                                float u = (float)((xOff[k] - *reinterpret_cast<int*>(lVar16 + 0x2c)) + iVar22)
                                          * (fVar3 / (float)iVar24);
                                float v = (float)((yOff[k] - *reinterpret_cast<int*>(lVar16 + 0x30)) + iVar18)
                                          * (fVar3 / (float)iVar28);
                                local_2a8 = ((uint64_t)(uint32_t&)v << 32) | (uint32_t&)u;
                                if (opT2MulVec) opT2MulVec(T_1f0, local_298, &local_2a8);
                                puVar19[-1] = (int16_t)(int)local_298[0];
                                puVar19[0]  = (int16_t)(int)local_298[1];
                                puVar19 += 10;
                            }
                        }

                        uint32_t* c0 = reinterpret_cast<uint32_t*>(descBlock + 0x00);
                        uint32_t* c1 = reinterpret_cast<uint32_t*>(descBlock + 0x14);
                        uint32_t* c2 = reinterpret_cast<uint32_t*>(descBlock + 0x28);
                        uint32_t* c3 = reinterpret_cast<uint32_t*>(descBlock + 0x3c);

                        auto emit = [&](uint32_t* desc) {
                            EmitVertex(lVar16, poolRootHolder, reinterpret_cast<int64_t*>(&pool),
                                       desc, &outNode);
                        };

                        if (((iVar22 + iVar18) & 1U) == 0) {
                            emit(c0); emit(c3); emit(c1);
                            emit(c1); emit(c3); emit(c2);
                        } else {
                            // odd-parity tile: triangles (c0,c2,c1) and (c0,c3,c2) — the
                            // last vertex is c2 (descBlock+0x28), NOT c1. Matches the ace40
                            // disasm else-branch find order 278,250,264,278,23c,250.
                            emit(c0); emit(c2); emit(c1);
                            emit(c0); emit(c3); emit(c2);
                        }
                    } // doTile

                    iVar22 = iVar22 + 1;
                    iVar11 = *reinterpret_cast<int*>(lVar16 + 0x34);
                } while (iVar22 <= iVar11);
            }
            iVar18 = iVar18 + 1;
            plVar30 = reinterpret_cast<int64_t*>(local_228);
        } while (iVar18 <= *reinterpret_cast<int*>(lVar16 + 0x38));
    }

    // finalize: copy pooled vertices into the slot's vertex buffer.
    if (fnEe40) fnEe40(plVar30, pool.count);
    int     total   = pool.count;
    int64_t vbBase  = *plVar30;
    int64_t srcBase = pool.base;
    for (int64_t off = 0, i = 0; i < total; ++i, off += 0x14) {
        *reinterpret_cast<int64_t*>(off + vbBase)        = *reinterpret_cast<int64_t*>(off + srcBase);
        *reinterpret_cast<int64_t*>(off + 8 + vbBase)    = *reinterpret_cast<int64_t*>(off + 8 + srcBase);
        *reinterpret_cast<int32_t*>(off + 0x10 + vbBase) = *reinterpret_cast<int32_t*>(off + 0x10 + srcBase);
    }

    // free the BST + vertex pool.
    if (bst.root != 0) {
        TradeWork_helper_14b00(reinterpret_cast<int64_t>(&bst.root), *reinterpret_cast<void**>(bst.root + 0x20));
        TradeWork_helper_14b00(reinterpret_cast<int64_t>(&bst.root), *reinterpret_cast<void**>(bst.root + 0x28));
        MemFree(reinterpret_cast<void*>(bst.root));
    }
    bst.root  = 0;
    bst.count = 0;
    MemFree(reinterpret_cast<void*>(pool.base));

    (void)local_208; (void)local_200; (void)local_1f8; (void)local_280; (void)local_220;
    (void)local_218; (void)local_210; (void)local_290_lo; (void)local_2e8; (void)uStack_2e0;
    (void)local_2d8; (void)corner; (void)r138; (void)r168; (void)r108;
}
