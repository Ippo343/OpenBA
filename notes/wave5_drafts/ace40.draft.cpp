// notes/wave5_drafts/ace40.draft.cpp
// Player wave round 5 — DRAFT port of FUN_1400ace40 (0x1400ace40).
//
// TradeWork_impl_ace40 — the radius/highlight tile tessellation builder. Builds the
// highlight-tile geometry (vertex + index buffers) for one TradeWork tile renderer
// slot (param_1 + 0x1f0 + param_2*0x60), deduping shared corner vertices through an
// AVL BST keyed by a 0x18-byte tile-corner descriptor.
//
// FAITHFUL 1:1 PORT of the Ghidra decompile, cross-checked against disassembly for
// every float lane, CONCAT pack, dropped-arg call, and field offset. Do NOT
// "improve" — a wrong port here is SILENT GEOMETRY CORRUPTION.
//
// ----------------------------------------------------------------------------------
// DEPENDENCIES (the lead will wire these; this file declares them extern):
//
//  Already-registered exe sub-callees (call directly; resolve via ResolveAddr):
//    0x1400acc10  TradeWork_helper_cc10     (emit 4 corner descriptors for a tile)
//    0x1400ae180  TradeWork_helper_e180     (edge-mask -> texture variant index)
//    0x14001ee40  TradeWork_impl_sub_ee40   (resize the renderer vertex buffer)
//    0x14001f8d0  pool-append helper        (append a 0x14-byte vertex to the pool)
//
//  Ported alongside in code/Game/Player/tool_base_highlight.cpp (extern "C"):
//    TradeWork_impl_1f6b0   (0x14001f6b0  AVL insert; key = 0x18-byte descriptor)
//    TradeWork_impl_1fc00   (0x14001fc00  AVL find;   returns node ptr or 0)
//    TradeWork_helper_14b00 (0x140014b00  recursive BST free)
//
//  Runtime engine ops (resolve at RUNTIME via engine_funcs.h; NO registration):
//    Math::Vector2::Vector2          (slot 0x140151e58)  ctor
//    Math::Transform2::Transform2    (slot 0x140152648)  (self, Scalar*, Vector2*)
//    Math::Transform2::FromYawOffset (slot 0x140152650)  (self, Vector2*)
//    Math::Transform2::operator*     (slot 0x140152640)  (Transform2*) -> Transform2*
//    Math::Transform2::operator*     (slot 0x140152658)  (Vector2*)    -> Vector2*
//    System::MemoryAllocator::MemAlloc/MemFree (static; slots 0x140152b60/0x140152b68)
//
// AVL node layout (SHARED with tool_base_trade.cpp; 0x38 bytes):
//   +0x00 key dwords[0..5] (6 dwords = 0x18-byte descriptor)  +0x18 parent
//   +0x20 left  +0x28 right  +0x30 short balance  +0x32 short height.
//   NOTE: this BST's key is the 0x18-byte tile descriptor (NOT an entity ptr).
//   The node also stores a payload index at +0x14 (read after a find hit).
//   (Node alloc = FUN_14001f850: MemAlloc(0x38,8), copies param_2[0..5], owner@+0x30
//    set elsewhere; +0x14 is the vertex index assigned by the pool.)
//
// Renderer slot layout (lVar16 = param_1+0x1f0 + param_2*0x60):
//   +0x08 vertex-buffer  {void* base, int count, int cap}        (plVar30 == lVar16+8)
//   +0x18 index-buffer   {void* base @+0x18, int count @+0x20, int cap @+0x24}
//   +0x2c minX  +0x30 minY  +0x34 maxX  +0x38 maxY  +0x40 yawDeg(int)
//   +0x3c byte (passed to cc10)  +0x44 char modeFlag (cVar1)
//   +0x48..0x58 float UV params (when modeFlag==0)
// ----------------------------------------------------------------------------------

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ---- engine constants (from .rdata; names match the decompile) -------------------
extern float g_fUVScale;        // g_fUVScale            (0x14017214c per disasm)
extern float DAT_140172260;     //                       (a 1.0-ish UV const)
extern float DAT_140172280;
extern uint32_t DAT_140172114;  // packed float bits (uVar8 seed)
extern uint32_t DAT_1401722c0;  // sign-flip mask (XORPS) for the negate
extern uint8_t  DAT_1401c23cf;  // base index byte (iVar13 seed)

// ---- ported-alongside AVL helpers (tool_base_highlight.cpp) ----------------------
extern "C" int64_t* TradeWork_impl_1f6b0(int64_t* root, int64_t* outNode, uint32_t* key);
extern "C" int64_t  TradeWork_impl_1fc00(int64_t* root, int64_t key);
extern "C" void     TradeWork_helper_14b00(void* root, void* node);

// ---- already-registered exe sub-callees ------------------------------------------
typedef void (*Cc10_t)(int64_t self, int x, int y, int64_t outDesc, int variant, uint8_t b3c);
typedef int  (*E180_t)(uint64_t a0, char a1, char a2, char a3, char a4,
                       char a5, char a6, char a7, char a8);
typedef void (*Ee40_t)(int64_t* vb, int count);
typedef void (*Pool8d0_t)(int64_t* pool, int newCount, void* rec);

// ---- static MemAlloc/MemFree (slot 0x140152b60/0x140152b68; copied EXACTLY from
// code/Game/Player/tool_base_trade.cpp EnsureMem) ---------------------------------
namespace {

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
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
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

// ---- engine Transform2/Vector2 op typedefs (resolve via engine_funcs.h slots) -----
// These match the .rdata dispatch-slot ABI used elsewhere in code/Game.
typedef void* (*Vec2Ctor_t)(void* self);                                   // 0x140151e58
typedef void* (*T2FromScalarVec_t)(void* self, void* scalar, void* vec);   // 0x140152648
typedef void* (*T2FromYaw_t)(void* self, void* scalar, void* vec);         // 0x140152650 (see NOTE)
// operator*(Transform2): 3-arg member form verified from disasm 0x1400ad0dc..0x1400ad127:
//   RCX = accumulator (this/lhs), RDX = out-buffer, R8 = rhs operand. Returns RDX.
typedef void* (*T2MulT2_t)(void* acc, void* outBuf, void* rhs);            // 0x140152640
// operator*(Vector2): 3-arg form, disasm 0x1400ad141: RCX=this(Transform2),
//   RDX=out Vector2, R8=in Vector2(packed corner). Returns RDX.
typedef void* (*T2MulVec_t)(void* self, void* outVec, void* inVec);        // 0x140152658

Vec2Ctor_t        opVec2Ctor   = nullptr;
T2FromScalarVec_t opT2Scalar   = nullptr;
T2FromYaw_t       opT2YawOff    = nullptr;
T2MulT2_t         opT2MulT2     = nullptr;
T2MulVec_t        opT2MulVec    = nullptr;
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

// ---------------------------------------------------------------------------------
// AppendIndex — the inlined "append one short index to the growable index buffer at
// renderer+0x18/+0x20/+0x24" block, which the original emits verbatim at every one
// of the 6 vertices per triangle pair. 1:1 with the disasm at e.g. 0x1400ad47c.
//   buffers: base @ lVar16+0x18, count @ lVar16+0x20, cap @ lVar16+0x24.
//   grow rule: newCount = count+1; if newCount>cap then cap=newCount, and if the old
//   count!=0 cap=newCount*3; realloc cap*2 bytes (shorts). Copy-forward old shorts,
//   fill new slot(s) with `idx`. Free old buffer if reallocated.
// (orig: inlined; see disasm 0x1400ad47c..0x1400ad516 etc.)
// ---------------------------------------------------------------------------------
inline void AppendIndex(int64_t lVar16, uint16_t idx) {
    int  iVar13 = *reinterpret_cast<int*>(lVar16 + 0x20);   // old count
    int  iVar25 = iVar13 + 1;                               // new count
    if (iVar25 == iVar13) return;                           // (never, but faithful)
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

// ---------------------------------------------------------------------------------
// EmitVertex — the find/insert/pool-append pattern that resolves ONE corner
// descriptor to a vertex index, then appends that index to the index buffer.
//   - find the 0x18-byte descriptor `desc` in the BST (root @ poolRoot/local_2c0);
//   - if found: idx = node[+0x14];
//   - if not found: idx = current pool count (*poolCount); pack desc[0..5] into a
//     0x14-byte vertex record, insert into BST (1f6b0) and append to the pool (1f8d0,
//     newCount = *poolCount + 1); the pool-append bumps *poolCount.
//   - then AppendIndex(idx).
// `desc` points at the 6-dword descriptor (key bytes 0..0x17 + vertex payload).
// The packed record copies: rec[0..7]=desc[0..7], rec[8..15]=desc[8..15],
//   rec[16..19] low = desc[16..19], rec[20..23] = idx (placed at +0x14 of the
//   vertex record, the +0x14 payload slot). Mirrors the CONCAT44 packing.
// outNode is a throwaway buffer for 1f6b0's returned node ptr (write-only).
//
// `pool` points at the contiguous 0x10-byte vertex pool {base@+0, count@+8, cap@+0xc}
// (the &local_2d0 frame block). 1f8d0 writes the new count into pool.count, so reading
// pool.count after the call yields the freshly-incremented value (== local_2c8).
// ---------------------------------------------------------------------------------
inline void EmitVertex(int64_t lVar16, int64_t* poolRoot, int64_t* pool,
                       uint32_t* desc, int64_t* outNode) {
    int64_t node = TradeWork_impl_1fc00(poolRoot, (int64_t)desc);   // orig: 0x14001fc00
    uint16_t idx;
    if (node == 0) {
        int curCount = *reinterpret_cast<int*>((char*)pool + 8);    // (int)local_2c8 (pool.count)
        // Pack the 0x14-byte vertex record (local_2e8 / uStack_2e0 / local_2d8):
        //   local_2e8       = desc[0..1]    (8 bytes)
        //   uStack_2e0      = desc[2..3]    (8 bytes)
        //   local_2d8 low4  = desc[4]       (4 bytes), high4 = (int)local_2c8 (the index)
        // i.e. rec[0..15] = desc[0..15], rec[16..19] = desc[16..19], rec[20..23] = idx.
        uint32_t rec[6];
        rec[0] = desc[0]; rec[1] = desc[1]; rec[2] = desc[2];
        rec[3] = desc[3]; rec[4] = desc[4];
        rec[5] = (uint32_t)curCount;          // CONCAT44((int)local_2c8, local_268) high lane
        idx = (uint16_t)curCount;
        TradeWork_impl_1f6b0(poolRoot, outNode, desc);                 // orig: 0x14001f6b0
        fnPool(pool, curCount + 1, rec);                               // orig: 0x14001f8d0
        // 1f8d0 stored (curCount+1) into pool.count (visible to the next EmitVertex
        // and to the finalize copy via local_2c8).
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

    // lVar16 = renderer slot for this tile group.
    int64_t lVar16 = (int64_t)param_2 * 0x60 + *reinterpret_cast<int64_t*>(param_1 + 0x1f0);
    int iVar24 = (*reinterpret_cast<int*>(lVar16 + 0x34) - *reinterpret_cast<int*>(lVar16 + 0x2c)) + 1; // width
    int iVar28 = (*reinterpret_cast<int*>(lVar16 + 0x38) - *reinterpret_cast<int*>(lVar16 + 0x30)) + 1; // height

    // --- four scratch Vector2 (local_1d8..local_1c0 region); ctor x4 (loop runs 4x) ---
    // (orig: 0x140151e58 in the do/while; the loop body runs for iVar18 = 3,2,1,0.)
    uint8_t cornerBuf[0x20]; // local_1d8 region (4 * Vector2)  -- holds the 4 corners
    {
        Vector2* p = reinterpret_cast<Vector2*>(cornerBuf);
        for (int i = 3; i >= 0; --i) { if (opVec2Ctor) opVec2Ctor(p); p = (Vector2*)((char*)p + 8); }
    }

    float fVar3 = g_fUVScale;
    char  cVar1 = *reinterpret_cast<char*>(lVar16 + 0x44);   // modeFlag

    // --- build the two source/dest UV+transform Scalar/Vector2 pairs --------------
    // Mirrors the XMM lane assignment at 0x1400acefd (cVar1==0) / 0x1400acf55 (else).
    // The packed locals feed Transform2::Transform2 (local_138) and the final mul.
    uint32_t uVar8 = 0;
    float fVar33 = 0.0f;
    float fVar31, fVar32, fVar34, fVar35, fVar36;
    float local_288, fStack_284;
    float local_290_hi; // local_290._4_4_

    if (cVar1 == '\0') {
        float fVar_div = *reinterpret_cast<float*>(lVar16 + 0x48);          // XMM0
        local_288 = 0.0f;
        fVar33 = *reinterpret_cast<float*>(lVar16 + 0x4c) / fVar_div;       // u0
        fVar31 = (*reinterpret_cast<float*>(lVar16 + 0x54) - *reinterpret_cast<float*>(lVar16 + 0x4c)) / fVar_div; // du
        fVar32 = (*reinterpret_cast<float*>(lVar16 + 0x58) - *reinterpret_cast<float*>(lVar16 + 0x50)) / fVar_div; // dv
        fVar34 = *reinterpret_cast<float*>(lVar16 + 0x50) / fVar_div;       // v0
        fVar35 = DAT_140172260;
        fStack_284 = g_fUVScale;
        local_290_hi = DAT_140172280;
        fVar36 = g_fUVScale;
        uVar8 = DAT_140172114;
    } else {
        fVar36 = (float)iVar24;
        local_290_hi = (float)iVar28;
        local_288 = (float)*reinterpret_cast<int*>(lVar16 + 0x2c);
        fStack_284 = (float)*reinterpret_cast<int*>(lVar16 + 0x30);
        fVar31 = g_fUVScale;
        fVar32 = g_fUVScale;
        fVar34 = fVar33;     // 0
        fVar35 = g_fUVScale;
    }

    // --- assemble the Scalar/Vector2 stack temps (1:1 with the MOV block) ----------
    // local_2a0 (Scalar a) = 0 ; local_29c = (float)yawDeg(+0x40)
    int   local_2a0 = 0;
    float local_29c = (float)*reinterpret_cast<int*>(lVar16 + 0x40);
    int   local_2b0_0 = 0;     // Scalar b

    // local_208/local_200 = (fVar35, fVar35) packed as the FromScalar Vector2 [+0x18 grp]
    uint64_t local_208 = (uint64_t)(uint32_t&)fVar35;
    uint64_t local_200 = (uint64_t)(uint32_t&)fVar35 << 0x20;
    uint64_t local_1f8 = 0;

    // local_280/local_220/local_218/local_210 group (the source-UV Vector2 for local_138)
    uint64_t local_280 = (uint64_t)(uint32_t&)fVar31;
    uint64_t local_220 = local_280;
    uint64_t local_218 = (uint64_t)(uint32_t&)fVar32 << 0x20;
    uint64_t local_210 = ((uint64_t)(uint32_t&)fVar34 << 32) | (uint32_t&)fVar33; // CONCAT44(fVar34,fVar33)
    uint64_t local_228 = ((uint64_t)uVar8 << 32) | uVar8;                          // CONCAT44(uVar8,uVar8)

    float local_298[2] = {0.0f, 0.0f}; // the working Vector2 fed to operator*(Vector2)

    // local_2a8 = CONCAT44(uVar8 ^ mask, uVar8 ^ mask) (the negated offset Vector2)
    uint32_t negSeed = uVar8 ^ DAT_1401722c0;
    uint64_t local_2a8 = ((uint64_t)negSeed << 32) | negSeed;

    // local_290 lo = 0 ; local_280 = fVar36 ; local_2e8/uStack_2e0/local_2d8 group:
    uint32_t local_290_lo = 0;
    local_280 = (uint64_t)(uint32_t&)fVar36;
    uint64_t local_2e8 = local_280;
    uint64_t uStack_2e0 = (uint64_t)(uint32_t&)local_290_hi << 0x20;
    uint64_t local_2d8 = ((uint64_t)(uint32_t&)fStack_284 << 32) | (uint32_t&)local_288;

    // --- the Transform2 chain (orig slots 0x140152648/650/640) --------------------
    // Mirrors the DISASM at 0x1400ad097..0x1400ad127 (Ghidra's decompile of this region
    // is unreliable — it lost the 3-arg operator* out-buffer threading). Each Transform2
    // local is 0x18 bytes. The chain composes:
    //   T_138 = Transform2(scalar=local_2a0, vec=local_228)          [RDI]
    //   T_168 = FromYawOffset(scalar=local_29c, vec=local_2a0..)     [RBX]   (3-arg form)
    //   T_108 = Transform2(scalar=local_2b0_0, vec=local_2a8)        [RAX0]
    //   acc = (T_2e8group) * T_108  -> T_160
    //   acc = acc * T_168 -> T_148 ; acc = acc * T_138 -> T_118 ;
    //   acc = acc * (RBP+0x18 grp) -> T_e8 ; acc = acc * (RBP+0x30 grp) -> T_48 == T_1f0
    // The FINAL out-buffer is the one read by the corner muls below; in the disasm that
    // is RBP+0x48 (the last operator* RDX). We name it T_1f0 for the corner step.
    uint8_t T_138[0x18], T_168[0x18], T_108[0x18];
    uint8_t T_160[0x18], T_148[0x18], T_118[0x18], T_e8[0x18], T_1f0[0x18];
    // The two trailing rhs operands (RBP+0x18 = the (local_2e8/2e0/2d8) src-UV group,
    // RBP+0x30 = the (local_30/38/40) dst group) — passed by address.
    void* rhs18 = &local_2e8;   // RBP+0x18 group (src-UV transform inputs)
    void* rhs30 = &local_2d8;   // RBP+0x30 group (dst transform inputs)   [SEE UNCERTAINTY]

    void* r138 = nullptr; void* r168 = nullptr; void* r108 = nullptr;
    if (opT2Scalar) r138 = opT2Scalar(T_138, &local_2a0, &local_228);          // orig:0x140152648
    if (opT2YawOff) r168 = opT2YawOff(T_168, &local_29c, &local_2a0);          // orig:0x140152650 (3-arg)
    if (opT2Scalar) r108 = opT2Scalar(T_108, &local_2b0_0, &local_2a8);        // orig:0x140152648

    void* acc = T_1f0;
    if (opT2MulT2) {
        // operator*(acc=RCX, outBuf=RDX, rhs=R8) -> returns outBuf.
        acc = opT2MulT2((void*)&local_2e8, T_160, r108);   // orig:0x140152640
        acc = opT2MulT2(acc, T_148, r168);                 // orig:0x140152640
        acc = opT2MulT2(acc, T_118, r138);                 // orig:0x140152640
        acc = opT2MulT2(acc, T_e8,  rhs18);                // orig:0x140152640
        acc = opT2MulT2(acc, T_1f0, rhs30);                // orig:0x140152640 -> final transform
    }
    (void)rhs30; // (rhs30/rhs18 addresses are best-effort; see UNCERTAINTY notes)

    // --- compute the 4 unit-quad corners (local_1d8..local_1c0) -------------------
    // operator*(Vector2) : (T_1f0, &local_2a8 [packed corner], &local_298) -> Vector2*
    // orig: 0x140152658. The packed corner cycles {0,0},{1,0},{1,1},{0,1}.
    uint64_t* corner = reinterpret_cast<uint64_t*>(cornerBuf);
    auto mulVec = [&](uint64_t packedCorner) -> uint64_t {
        local_2a8 = packedCorner;
        // The 3rd arg (R8 = &local_2a8 packed corner), 2nd (RDX = &local_298 working
        // Vector2), 1st (RCX = T_1f0). Returns ptr; we read *ret (8 bytes).
        if (!opT2MulVec) return 0;
        uint64_t* r = reinterpret_cast<uint64_t*>(opT2MulVec(T_1f0, local_298, &local_2a8));
        return *r;
    };
    corner[0] = mulVec(0x0);                  // {0,0}  -> local_1d8
    corner[1] = mulVec(0x3f800000ULL);        // {1,0}  -> local_1d0
    corner[2] = mulVec(0x3f8000003f800000ULL);// {1,1}  -> local_1c8
    corner[3] = mulVec(0x3f80000000000000ULL);// {0,1}  -> local_1c0

    // --- init the dedup pool + BST -------------------------------------------------
    // pool = contiguous {base@+0 (local_2d0), count@+8 (local_2c8 low), cap@+0xc} 0x10B.
    // (1f8d0 reads pool[0]=base, pool[1] low=count, pool[1]+0xc=cap.)
    struct VPool { int64_t base; int32_t count; int32_t cap; } pool = {0, 0, 0};
    // BST root holder: {root@+0 (local_2c0), nodeCount@+8 (local_2b8)}.
    struct BstRoot { int64_t root; int32_t count; } bst = {0, 0};
    int64_t* poolRootHolder = reinterpret_cast<int64_t*>(&bst);  // local_2c0 == &bst.root

    int iVar13 = (int)DAT_1401c23cf;                                 // base index byte
    fVar33 = (float)((*reinterpret_cast<int*>(lVar16 + 0x34) + *reinterpret_cast<int*>(lVar16 + 0x2c)) >> 1); // centerX
    int iVar25 = (*reinterpret_cast<int*>(lVar16 + 0x38) + *reinterpret_cast<int*>(lVar16 + 0x30)) >> 1;       // centerY
    int64_t* plVar30 = reinterpret_cast<int64_t*>(lVar16 + 8); // renderer vertex buffer
    local_2b0_0 = iVar13;
    local_2a0   = iVar25;
    local_29c   = fVar33;
    local_228   = (uint64_t)plVar30;

    // --- free + reset the renderer's vertex and index buffers ---------------------
    MemFree(reinterpret_cast<void*>(*plVar30));                       // orig: 0x140152b68
    *plVar30 = 0;
    *reinterpret_cast<int64_t*>(lVar16 + 0x10) = 0;                   // count/cap
    void** ppvVar9 = *reinterpret_cast<void***>(lVar16 + 0x18);
    MemFree(ppvVar9);                                                 // orig: 0x140152b68
    *reinterpret_cast<int64_t*>(lVar16 + 0x18) = 0;
    *reinterpret_cast<int64_t*>(lVar16 + 0x20) = 0;

    int64_t outNode;  // throwaway for 1f6b0 return slot

    // === main tile loop (y = minY..maxY, x = minX..maxX) ==========================
    int iVar18 = *reinterpret_cast<int*>(lVar16 + 0x30);   // y
    if (iVar18 <= *reinterpret_cast<int*>(lVar16 + 0x38)) {
        do {
            int iVar22 = *reinterpret_cast<int*>(lVar16 + 0x2c);  // x
            int iVar11 = *reinterpret_cast<int*>(lVar16 + 0x34);  // maxX
            if (iVar22 <= iVar11) {
                do {
                    bool doTile;
                    if (param_3 == '\0') {
                        doTile = true; // fall to LAB_1400ad2ac
                    } else {
                        // radius test: dist^2 in [param_4^2, param_5^2]
                        uint32_t dist = (uint32_t)((iVar18 - iVar25) * (iVar18 - iVar25) +
                                                   (iVar22 - (int)fVar33) * (iVar22 - (int)fVar33));
                        doTile = (param_4 * param_4 <= (int)dist) && ((int)dist <= param_5 * param_5);
                    }

                    if (doTile) {
                        // --- LAB_1400ad2ac: edge-mask -> variant index (only if cVar1) ---
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
                            // orig: 0x1400ae180 — 9 args (verified vs disasm 0x1400ad31c..0x1400ad355):
                            //  a0 = CONCAT71(ppvVar9>>8, bVar2)  [low byte used]
                            //  a1 = (iVar25b <= iVar13b)  a2 = (iVar14 <= iVar11)
                            //  a3 = (lVar16+0x30 <= iVar18-1)  a4 = bVar2
                            //  a5 = cVar12  a6 = cVar17  a7 = cVar23  a8 = cVar26
                            iVar13 = fnE180(
                                (uint64_t)((uintptr_t)ppvVar9 & ~0xffULL) | (uint8_t)bVar2,
                                (char)(iVar25b <= iVar13b), (char)(iVar14 <= iVar11),
                                (char)(*reinterpret_cast<int*>(lVar16 + 0x30) <= iVar18 - 1),
                                (char)bVar2, cVar12, cVar17, cVar23, cVar26);
                            local_2b0_0 = iVar13;
                        }

                        // --- cc10: emit the 4 corner descriptors into local_278 group ----
                        // orig: 0x1400acc10 — (param_1, x, y, &local_278, variant, [+0x3c]).
                        // Output buffer is the 0x60-ish descriptor block starting at
                        // local_278 (= &cornerDesc below). cc10 fills 4 corners * 0x14.
                        uint8_t descBlock[0x60]; // local_278 .. local_22c region
                        fnCc10(param_1, iVar22, iVar18, (int64_t)descBlock, iVar13,
                               *reinterpret_cast<uint8_t*>(lVar16 + 0x3c));

                        // --- per-tile UV: 4 corners, write short pos pair into descBlock -
                        //  for k in {0,1,2,3}:
                        //   u = ((xOff[k] - minX) + x) * (fVar3 / width)     [low lane]
                        //   v = ((yOff[k] - minY) + y) * (fVar3 / height)    [high lane]
                        //   pos = local_1f0 * Vector2(u,v); store (short)(int)pos.x at
                        //   descBlock+0x8 and (short)(int)pos.y at descBlock+0xA, stride 0x14.
                        //   (RDI starts at &uStack_270+2 == descBlock+0xA; writes [-2]=+0x8,[0]=+0xA.)
                        {
                            // Offset tables read from &local_208 (u-lane) and &local_220 (v-lane).
                            // local_208=0x100000000, local_200=1 -> xOff = {0,1,1,0}
                            // local_220=0,           local_218=0x100000001 -> yOff = {0,0,1,1}
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
                                local_2a8 = ((uint64_t)(uint32_t&)v << 32) | (uint32_t&)u; // CONCAT44(v,u)
                                if (opT2MulVec) opT2MulVec(T_1f0, local_298, &local_2a8);
                                // CVTTSS2SI (truncate toward zero):
                                puVar19[-1] = (int16_t)(int)local_298[0];  // descBlock+0x8
                                puVar19[0]  = (int16_t)(int)local_298[1];  // descBlock+0xA
                                puVar19 += 10; // +0x14 bytes
                            }
                        }

                        // --- triangle winding: parity of (x+y) picks two windings --------
                        // The 4 corner descriptors live at descBlock offsets:
                        //   c0 = descBlock+0x00 (local_278)  c1 = descBlock+0x14 (local_264)
                        //   c2 = descBlock+0x28 (local_250)  c3 = descBlock+0x3c (local_23c)
                        // (each 0x14 bytes; verified from the CONCAT44 packs that read
                        //  local_278/local_264/local_250/local_23c.)
                        uint32_t* c0 = reinterpret_cast<uint32_t*>(descBlock + 0x00);
                        uint32_t* c1 = reinterpret_cast<uint32_t*>(descBlock + 0x14);
                        uint32_t* c2 = reinterpret_cast<uint32_t*>(descBlock + 0x28);
                        uint32_t* c3 = reinterpret_cast<uint32_t*>(descBlock + 0x3c);

                        // EmitVertex reads/writes pool.count directly (1f8d0 updates it).
                        auto emit = [&](uint32_t* desc) {
                            EmitVertex(lVar16, poolRootHolder, reinterpret_cast<int64_t*>(&pool),
                                       desc, &outNode);
                        };

                        if (((iVar22 + iVar18) & 1U) == 0) {
                            // parity 0 : tri(c0,c3,c1)  tri(c1,c3,c2)
                            //  order from decompile: local_278, local_23c, local_264,
                            //                        local_264, local_23c, local_250
                            emit(c0); // local_278
                            emit(c3); // local_23c
                            emit(c1); // local_264
                            emit(c1); // local_264
                            emit(c3); // local_23c
                            emit(c2); // local_250
                        } else {
                            // parity 1 : tri(c0,c2,c1)  tri(c1,c2,c0,...)
                            //  order from decompile: local_278, local_250, local_264,
                            //                        local_278, local_23c, local_264
                            // (the 2nd triangle re-reads local_278/local_23c/local_264 —
                            //  see disasm 0x1400ada1c..0x1400ae047 block.)
                            emit(c0); // local_278
                            emit(c2); // local_250
                            emit(c1); // local_264
                            emit(c0); // local_278
                            emit(c3); // local_23c
                            emit(c1); // local_264
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

    // === finalize: copy pooled vertices into the renderer vertex buffer ===========
    // orig: 0x14001ee40 — resize renderer vertex buffer (plVar30 == lVar16+8) to count.
    fnEe40(plVar30, pool.count);   // (int)local_2c8
    // Copy pool.count records of 0x14 bytes (8+8+4) from pool.base -> plVar30[0] base.
    // (disasm 0x1400ae0b0: RDX=[plVar30], RCX=[RSP+0x68]=pool.base.)
    int     total   = pool.count;
    int64_t vbBase  = *plVar30;
    int64_t srcBase = pool.base;
    for (int64_t off = 0, i = 0; i < total; ++i, off += 0x14) {
        *reinterpret_cast<int64_t*>(off + vbBase)        = *reinterpret_cast<int64_t*>(off + srcBase);
        *reinterpret_cast<int64_t*>(off + 8 + vbBase)    = *reinterpret_cast<int64_t*>(off + 8 + srcBase);
        *reinterpret_cast<int32_t*>(off + 0x10 + vbBase) = *reinterpret_cast<int32_t*>(off + 0x10 + srcBase);
    }

    // === free the BST + vertex pool ===============================================
    // pvVar7 = local_2c0 (BST root). Free both subtrees then the root node.
    if (bst.root != 0) {
        TradeWork_helper_14b00(&bst.root, *reinterpret_cast<void**>(bst.root + 0x20)); // orig:0x140014b00
        TradeWork_helper_14b00(&bst.root, *reinterpret_cast<void**>(bst.root + 0x28)); // orig:0x140014b00
        MemFree(reinterpret_cast<void*>(bst.root));                                    // orig:0x140152b68
    }
    bst.root  = 0;
    bst.count = 0;   // local_2b8 = 0
    MemFree(reinterpret_cast<void*>(pool.base));                                       // orig:0x140152b68

    // silence unused (faithful locals)
    (void)local_208; (void)local_200; (void)local_1f8; (void)local_280; (void)local_220;
    (void)local_218; (void)local_210; (void)local_290_lo; (void)local_2e8; (void)uStack_2e0;
    (void)local_2d8; (void)iVar11; (void)corner;
}
