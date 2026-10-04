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
// The slot is a GeometryBatch (Game/WorldGeometryElements.h, the 0x60-byte decal
// element at WorldGeometry +0x1F0; batch = decals.data + param_2):
//   vertices (+0x08, 0x14-byte GeometryBatchVertex)   indices (+0x18, uint16)
//   minX / minY / maxX / maxY (+0x2c..+0x38)   field_0x40 yawDeg (int)
//   field_0x3C byte (passed to cc10)   field_0x44 modeFlag (cVar1)
//   field_0x48[0..4] float UV params (when modeFlag==0)

#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "math/Math.h"
#include "system/MemoryAllocator.h"
#include "Action/exe_bodies.h"
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Player/exe_bodies.h"
#include "Game/WorldGeometryElements.h"   // GeometryBatch / GeometryBatchVertex (0x14001fa80 / 0x14001ee40)

// ---- engine .rdata constants (retail values; see header) -------------------------
namespace {
constexpr float    kDAT_140172260 =  4096.0f;    // 0x45800000  (UV tile-size const)
constexpr float    kDAT_140172280 = -1.0f;       // 0xbf800000
constexpr uint32_t kDAT_140172114 = 0x3f000000;  // 0.5f packed bits (uVar8 seed)
constexpr uint32_t kDAT_1401722c0 = 0x80000000;  // XORPS sign-flip mask (the negate)
constexpr uint8_t  kDAT_1401c23cf = 0x2e;        // base index byte (iVar13 seed = 46)
}  // namespace

// ---- the vertex-dedup set (tool_base_highlight.cpp, Player/exe_bodies.h) -----------
//   0x14001f6b0 DecalVertexSetInsert, 0x14001fc00 DecalVertexSetFind,
//   0x140014b00 Avl38FreeSubtree<DecalVertexNode>.

namespace {

// ---- exe sub-callees, called by name (Action/exe_bodies.h) -------------------------
//   0x1400acc10 Pf_RoadQuad_cc10, 0x1400ae180 Pf_TileShapeLut_ae180,
//   0x14001ee40 Pf_VectorResize14_ee40, 0x14001f8d0 Pf_VectorResize14Fill_f8d0.
// Static MemAlloc/MemFree (slots 0x140152b60/0x140152b68) are
// System::MemoryAllocator::MemAlloc / MemFree.
//
// ---- engine Transform2/Vector2 ops (.rdata dispatch slots), called as the members of
// Math::Transform2 / Math::Vector2 exactly as their C doors (RtMath_*) did:
//   0x140151e58 Vector2::Vector2()            -> *p = Math::Vector2()
//   0x140152648 Transform2(Scalar&, Vector2&)  -> *self = Math::Transform2(s, v)
//   0x140152650 Transform2::FromYawOffset      -> *ret = Transform2::FromYawOffset(s, v)
//   0x140152640 Transform2::operator*(T2)      -> *ret = lhs * rhs   (RCX=lhs, RDX=ret, R8=rhs)
//   0x140152658 Transform2::operator*(Vector2) -> *ret = self * v    (RCX=self, RDX=ret, R8=v)
inline void* T2Ctor(void* self, const void* s, const void* v) {   // B.3.x bridge
    *static_cast<Math::Transform2*>(self) = Math::Transform2(
        *static_cast<const Math::Scalar*>(s), *static_cast<const Math::Vector2*>(v));
    return self;
}
inline void* T2FromYawOffset(void* ret, const void* s, const void* v) {   // B.3.x bridge
    *static_cast<Math::Transform2*>(ret) = Math::Transform2::FromYawOffset(
        *static_cast<const Math::Scalar*>(s), *static_cast<const Math::Vector2*>(v));
    return ret;
}
inline void* T2MulT2(const void* lhs, void* ret, const void* rhs) {   // B.3.x bridge
    *static_cast<Math::Transform2*>(ret) =
        *static_cast<const Math::Transform2*>(lhs) * *static_cast<const Math::Transform2*>(rhs);
    return ret;
}
inline void* T2MulVec(const void* self, void* ret, const void* v) {   // B.3.x bridge
    *static_cast<Math::Vector2*>(ret) =
        *static_cast<const Math::Transform2*>(self) * *static_cast<const Math::Vector2*>(v);
    return ret;
}

// The two growable lists inside a highlight slot (GeometryBatch::vertices / indices):
//   +0x08 {data, size, cap}  GeometryBatchVertex records, 0x14 bytes each (the pool ->
//                            buffer copy at the end of TessellateSlot moves them 8+8+4);
//   +0x18 {data, size, cap}  uint16 triangle indices.
// ENDGAME B.3.2 step 2: AppendIndex WAS a verbatim copy of rtcoll::SetSizeCore for
// the +0x18 list (exact-equal early return; size stored before the capacity test;
// `oldCount != 0 ? n*3 : n` with newCount == oldCount + 1 by construction, i.e. the
// reference rule; MemAlloc(cap * 2, align 2) through the static slot 0x140152b60 —
// the same locking static rtcoll::MemAlloc is; copy the prefix only when the block
// moved; the new slot takes the fill; the COMPUTED element address null-tested;
// free the old block when it moved). The trait-derived Array<uint16_t> row
// (stride 2, align 2, byteCopy, init == nullptr, destroy == nullptr) is that
// exactly. The vertex list is only ever destroyed here (its grow is 0x14001ee40,
// outside this file); its element is the trivially destructible GeometryBatchVertex,
// so the dtor is MemFree + zero regardless.
using TessVertexArray = Collection::Array<GeometryBatchVertex>;
using TessIndexArray  = Collection::Array<uint16_t>;

// AppendIndex — inlined "append one short index to the growable index buffer at
// slot+0x18/+0x20/+0x24" block (orig emits it verbatim per triangle vertex).
inline void AppendIndex(GeometryBatch* batch, uint16_t idx) {
    TessIndexArray* a = reinterpret_cast<TessIndexArray*>(&batch->indices);   // LEFT: GeometryBatch::indices is a RawArray, not a Collection::Array (Game/WorldGeometryElements.h, not PLAYER's)
    a->SetSize(a->_size + 1, idx);
}

// EmitVertex — resolve ONE corner descriptor to a vertex index (find/insert/append),
// then append that index to the index buffer.
inline void EmitVertex(GeometryBatch* batch, AvlTreeHead<DecalVertexNode>* set,
                       RawArray<GeometryBatchVertex>* pool,
                       uint32_t* desc, DecalVertexNode** outNode) {
    // LEFT: the corner records are raw dwords of this body's 0x60-byte stack block
    // (cc10 writes them through an int64_t); the set reads their first 0x14 bytes as
    // the key.
    DecalVertexNode* node = DecalVertexSetFind(set, reinterpret_cast<const DecalVertexKey*>(desc));   // 0x14001fc00
    uint16_t idx;
    if (node == nullptr) {
        int curCount = pool->size;
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
        DecalVertexSetInsert(set, outNode, reinterpret_cast<const DecalVertexKey*>(rec));   // 0x14001f6b0 (key = rec) LEFT: as above
        Pf_VectorResize14Fill_f8d0(reinterpret_cast<int64_t*>(pool), curCount + 1, reinterpret_cast<uint64_t*>(desc));   // 0x14001f8d0 (vertex = desc) LEFT: SIG Pf_VectorResize14Fill_f8d0
    } else {
        idx = (uint16_t)node->key.index;   // the low word (retail reads a word at node+0x14)
    }
    AppendIndex(batch, idx);
}

} // namespace

// =================================================================================
// TradeWork_impl_ace40 — 0x1400ace40
// =================================================================================
extern "C" void TradeWork_impl_ace40(int64_t param_1, int param_2, char param_3,
                                     int param_4, int param_5) {
    const float fUV = kRdata_14017214c;   // g_fUVScale (1.0f in retail)

    GeometryBatch* batch = *reinterpret_cast<GeometryBatch**>(param_1 + 0x1f0) + param_2;   // LEFT: WorldGeometry::_arrays (GAME-2, H5): the decals' data pointer
    int iVar24 = (batch->maxX - batch->minX) + 1; // width
    int iVar28 = (batch->maxY - batch->minY) + 1; // height

    // four scratch Vector2 (local_1d8..local_1c0); ctor x4.
    uint8_t cornerBuf[0x20];
    {
        uint8_t* p = cornerBuf;
        for (int i = 3; i >= 0; --i) { *reinterpret_cast<Math::Vector2*>(p) = Math::Vector2(); p += 8; }
    }

    float fVar3 = fUV;
    char  cVar1 = batch->field_0x44;   // modeFlag

    uint32_t uVar8 = 0;
    float fVar33 = 0.0f;
    float fVar31, fVar32, fVar34, fVar35, fVar36;
    float local_288, fStack_284;
    float local_290_hi;

    if (cVar1 == '\0') {
        float fVar_div = batch->field_0x48[0];
        local_288 = 0.0f;
        fVar33 = batch->field_0x48[1] / fVar_div;
        fVar31 = (batch->field_0x48[3] - batch->field_0x48[1]) / fVar_div;
        fVar32 = (batch->field_0x48[4] - batch->field_0x48[2]) / fVar_div;
        fVar34 = batch->field_0x48[2] / fVar_div;
        fVar35 = kDAT_140172260;
        fStack_284 = fUV;
        local_290_hi = kDAT_140172280;
        fVar36 = fUV;
        uVar8 = kDAT_140172114;
    } else {
        fVar36 = (float)iVar24;
        local_290_hi = (float)iVar28;
        local_288 = (float)batch->minX;
        fStack_284 = (float)batch->minY;
        fVar31 = fUV;
        fVar32 = fUV;
        fVar34 = fVar33;     // 0
        fVar35 = fUV;
    }

    int   local_2a0 = 0;
    float local_29c = (float)batch->field_0x40;
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
    r138 = T2Ctor(T_138, &local_2a0, &local_228);                             // 0x140152648 (ctor1)
    r168 = T2FromYawOffset(T_168, &local_29c, yawOffset);                     // 0x140152650 (yawOff)
    r108 = T2Ctor(T_108, &local_2b0_0, &local_2a8);                           // 0x140152648 (ctor3)

    {
        void* acc;
        acc = T2MulT2(TF_flip, T_160, r108);   // 0x140152640  flip   * ctor3(r108)
        acc = T2MulT2(acc, T_148, r168);        //              (..)   * yawOff(r168)
        acc = T2MulT2(acc, T_118, r138);        //              (..)   * ctor1(r138)
        acc = T2MulT2(acc, T_e8,  TF_uvRect);   //              (..)   * uvRect
        acc = T2MulT2(acc, T_1f0, TF_scale);    //              (..)   * scale  -> T_1f0
        (void)acc;
    }


    // 4 unit-quad corners via operator*(Vector2). Packed corner cycles {0,0},{1,0},{1,1},{0,1}.
    uint64_t* corner = reinterpret_cast<uint64_t*>(cornerBuf);
    auto mulVec = [&](uint64_t packedCorner) -> uint64_t {
        local_2a8 = packedCorner;
        uint64_t* r = reinterpret_cast<uint64_t*>(T2MulVec(T_1f0, local_298, &local_2a8));
        return *r;
    };
    corner[0] = mulVec(0x0);                   // {0,0}
    corner[1] = mulVec(0x3f800000ULL);         // {1,0}
    corner[2] = mulVec(0x3f8000003f800000ULL); // {1,1}
    corner[3] = mulVec(0x3f80000000000000ULL); // {0,1}

    // dedup pool + BST.
    RawArray<GeometryBatchVertex> pool = {nullptr, 0, 0};
    AvlTreeHead<DecalVertexNode> bst = {nullptr, 0, {}};

    int iVar13 = (int)kDAT_1401c23cf;
    fVar33 = (float)((batch->maxX + batch->minX) >> 1);
    int iVar25 = (batch->maxY + batch->minY) >> 1;
    int64_t* plVar30 = reinterpret_cast<int64_t*>(&batch->vertices);   // LEFT: SIG Pf_VectorResize14_ee40 takes int64_t*
    local_2b0_0 = iVar13;
    local_2a0   = iVar25;
    local_29c   = fVar33;
    // (retail spills plVar30 to the local_228 slot here and reloads it at the end of
    // each row; the round trip computes nothing and is not modelled.)

    // free + reset the slot's vertex and index buffers.
    // ENDGAME B.3.2 step 2: both WERE inlined Collection::Array<T>::~Array bodies
    // for a trivially destructible T (MemFree the buffer — null-guarded in both —
    // then zero _data and the _size/_capacity qword). Now the dtor itself, in place.
    // ppvVar9 keeps the pre-free index pointer because the fnE180 call below still
    // feeds its upper bytes through a register-passthrough byte argument.
    uint16_t* ppvVar9 = batch->indices.data;
    reinterpret_cast<TessVertexArray*>(&batch->vertices)->~Array();   // LEFT: GeometryBatch's lists are RawArray, not Collection::Array (Game/WorldGeometryElements.h)
    reinterpret_cast<TessIndexArray*>(&batch->indices)->~Array();     // LEFT: as above

    DecalVertexNode* outNode;

    // main tile loop (y = minY..maxY, x = minX..maxX).
    int iVar18 = batch->minY;
    if (iVar18 <= batch->maxY) {
        do {
            int iVar22 = batch->minX;
            int iVar11 = batch->maxX;
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
                            int iVar25b = batch->minX;
                            int iVar13b = iVar22 - 1;
                            int iVar14  = iVar22 + 1;
                            char cVar26, cVar23, cVar17, cVar12;
                            cVar26 = ((iVar13b < iVar25b) && (batch->maxY < iVar18 + 1)) ? '\0' : '\x01';
                            cVar23 = ((iVar11   < iVar14 ) && (batch->maxY < iVar18 + 1)) ? '\0' : '\x01';
                            cVar17 = ((iVar13b < iVar25b) && (iVar18 - 1 < batch->minY)) ? '\0' : '\x01';
                            cVar12 = ((iVar11   < iVar14 ) && (iVar18 - 1 < batch->minY)) ? '\0' : '\x01';
                            bool bVar2 = iVar18 + 1 <= batch->maxY;
                            iVar13 = Pf_TileShapeLut_ae180(
                                reinterpret_cast<void*>((uint64_t)((uintptr_t)ppvVar9 & ~0xffULL) | (uint8_t)bVar2),
                                (char)(iVar25b <= iVar13b), (char)(iVar14 <= iVar11),
                                (char)(batch->minY <= iVar18 - 1),
                                (char)bVar2, cVar12, cVar17, cVar23, cVar26);
                            local_2b0_0 = iVar13;
                        }

                        // cc10: emit the 4 corner descriptors into descBlock.
                        uint8_t descBlock[0x60];
                        Pf_RoadQuad_cc10(param_1, iVar22, iVar18, (int64_t)descBlock, iVar13,
                                         batch->field_0x3C);

                        // per-tile UV: 4 corners, write short pos pair into descBlock.
                        {
                            int32_t xOff[4]; int32_t yOff[4];
                            *reinterpret_cast<uint64_t*>(&xOff[0]) = 0x100000000ULL; // {0,1}
                            *reinterpret_cast<uint64_t*>(&xOff[2]) = 0x1ULL;          // {1,0}
                            *reinterpret_cast<uint64_t*>(&yOff[0]) = 0x0ULL;          // {0,0}
                            *reinterpret_cast<uint64_t*>(&yOff[2]) = 0x100000001ULL;  // {1,1}
                            int16_t* puVar19 = reinterpret_cast<int16_t*>(descBlock + 0xA);
                            for (int64_t k = 0; k < 4; ++k) {
                                float u = (float)((xOff[k] - batch->minX) + iVar22)
                                          * (fVar3 / (float)iVar24);
                                float v = (float)((yOff[k] - batch->minY) + iVar18)
                                          * (fVar3 / (float)iVar28);
                                local_2a8 = ((uint64_t)(uint32_t&)v << 32) | (uint32_t&)u;
                                T2MulVec(T_1f0, local_298, &local_2a8);
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
                            EmitVertex(batch, &bst, &pool, desc, &outNode);
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
                    iVar11 = batch->maxX;
                } while (iVar22 <= iVar11);
            }
            iVar18 = iVar18 + 1;
        } while (iVar18 <= batch->maxY);
    }

    // finalize: copy pooled vertices into the slot's vertex buffer.
    Pf_VectorResize14_ee40(plVar30, pool.size);
    int     total   = pool.size;
    int64_t vbBase  = *plVar30;
    int64_t srcBase = reinterpret_cast<int64_t>(pool.data);   // LEFT: the 8+8+4 record copy below is retail's (0x1400ace40); a GeometryBatchVertex assignment would change its store widths
    for (int64_t off = 0, i = 0; i < total; ++i, off += 0x14) {
        *reinterpret_cast<int64_t*>(off + vbBase)        = *reinterpret_cast<int64_t*>(off + srcBase);
        *reinterpret_cast<int64_t*>(off + 8 + vbBase)    = *reinterpret_cast<int64_t*>(off + 8 + srcBase);
        *reinterpret_cast<int32_t*>(off + 0x10 + vbBase) = *reinterpret_cast<int32_t*>(off + 0x10 + srcBase);
    }

    // free the BST + vertex pool.
    if (bst.root != nullptr) {
        Avl38FreeSubtree(&bst.root, bst.root->left);    // 0x140014b00
        Avl38FreeSubtree(&bst.root, bst.root->right);   // 0x140014b00
        System::MemoryAllocator::MemFree(bst.root);   // bst.root != 0 here
    }
    bst.root  = nullptr;
    bst.count = 0;
    if (pool.data) System::MemoryAllocator::MemFree(pool.data);

    (void)local_208; (void)local_200; (void)local_1f8; (void)local_280; (void)local_220;
    (void)local_218; (void)local_210; (void)local_290_lo; (void)local_2e8; (void)uStack_2e0;
    (void)local_2d8; (void)corner; (void)r138; (void)r168; (void)r108;
}
