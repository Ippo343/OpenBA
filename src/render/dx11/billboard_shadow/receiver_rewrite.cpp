// ============================================================================
// src/render/dx11/billboard_shadow/receiver_rewrite.cpp -- receiver_rewrite.h
// ============================================================================
// NOT RETAIL (ext.billboard_shadow). Standard C++ only: the unit tests link it.
// ============================================================================
#include "billboard_shadow/receiver_rewrite.h"

#include <cstdio>
#include <cstring>

namespace BbsRewrite {
namespace {

namespace Op = Dxbc::Op;
namespace OT = Dxbc::OperandType;

constexpr uint32_t kOperandNull = 13;   // D3D10_SB_OPERAND_TYPE_NULL

// D3D10_SB_OPCODE_TYPE values this file needs beyond Dxbc::Op.
enum : uint32_t {
    kBreak = 2, kBreakC = 3, kCall = 4, kCallC = 5, kCase = 6, kContinue = 7, kContinueC = 8, kCut = 9,
    kDefault = 10, kDp3 = 16, kDp4 = 17, kElse = 18, kEmit = 19, kEmitThenCut = 20, kEndIf = 21,
    kEndLoop = 22, kEndSwitch = 23, kIf = 31, kIMul = 38, kLabel = 44, kLd = 45, kLoop = 48,
    kRetC = 63, kSwitch = 76, kSinCos = 77, kUDiv = 78, kUMul = 81, kDclIndexableTemp = 105,
};

bool IsFlowControl(uint32_t op) {
    switch (op) {
    case kBreak: case kBreakC: case kCall: case kCallC: case kCase: case kContinue: case kContinueC:
    case kCut: case kDefault: case Op::Discard: case kElse: case kEmit: case kEmitThenCut: case kEndIf:
    case kEndLoop: case kEndSwitch: case kIf: case kLabel: case kLoop: case kRetC: case kSwitch:
        return true;
    default:
        return false;
    }
}

int DstCount(const Dxbc::Instr& in) {
    if (in.operands.empty()) return 0;
    switch (in.opcode) {
    case kIMul: case kSinCos: case kUDiv: case kUMul: return 2;
    default: return 1;
    }
}

// How many source components a source operand contributes, by opcode: -1 = per destination
// component (the default), N = the first N swizzle slots (dot products), 4 = all four (fetches).
int SourceSpan(uint32_t op) {
    switch (op) {
    case Op::Dp2: return 2;
    case kDp3: return 3;
    case kDp4: return 4;
    case kLd: case Op::ResInfo: case Op::Sample: case Op::SampleC: case Op::SampleCLz: case Op::SampleL:
    case Op::SampleD: case Op::SampleB:
        return 4;
    default:
        return -1;
    }
}

// One temp read: register and component bits.
struct Read {
    uint32_t reg;
    uint32_t mask;
};

// The temps an operand's RELATIVE indices read (cb3[r0.w + 1] reads r0.w), recursively.
// Returns the operand's length in dwords (0 = malformed).
uint32_t RelativeReads(const uint32_t* t, uint32_t avail, std::vector<Read>* reads, bool* cb0Relative) {
    if (avail == 0) return 0;
    uint32_t n = 0;
    const uint32_t tok = t[n++];
    const uint32_t type = (tok >> 12) & 0xFF;
    const uint32_t dims = (tok >> 20) & 3;
    const uint32_t numComp = tok & 3;
    if (tok & 0x80000000u) {
        uint32_t ext;
        do {
            if (n >= avail) return 0;
            ext = t[n++];
        } while (ext & 0x80000000u);
    }
    if (type == OT::Imm32 || type == OT::Imm64) {
        const uint32_t comps = numComp == 1 ? 1 : (numComp == 2 ? 4 : 0);
        const uint32_t len = comps * (type == OT::Imm64 ? 2 : 1);
        return n + len <= avail ? n + len : 0;
    }
    uint32_t idx0 = 0;
    for (uint32_t d = 0; d < dims; ++d) {
        const uint32_t rep = (tok >> (22 + 3 * d)) & 7;
        if (rep == 0 || rep == 3) {
            if (n >= avail) return 0;
            if (d == 0) idx0 = t[n];
            ++n;
        } else if (rep == 1 || rep == 4) {
            n += 2;
            if (n > avail) return 0;
        }
        if (rep >= 2) {
            if (type == OT::ConstantBuffer && d == 1 && idx0 == kGlobalCb && cb0Relative) *cb0Relative = true;
            // The index register: a temp (or input) with one selected component.
            const uint32_t sub = t[n];
            const uint32_t subType = (sub >> 12) & 0xFF;
            const uint32_t subSel  = (sub >> 2) & 3;
            const uint32_t subBits = (sub >> 4) & 0xFF;
            const uint32_t len = RelativeReads(t + n, avail - n, reads, cb0Relative);
            if (len == 0) return 0;
            if (subType == OT::Temp && reads) {
                uint32_t mask = 0;
                if (subSel == 2) mask = 1u << (subBits & 3);
                else if (subSel == 1) for (int c = 0; c < 4; ++c) mask |= 1u << ((subBits >> (2 * c)) & 3);
                else mask = subBits & 0xF;
                // the sub-operand's register index is its first index dword
                uint32_t k = 1;
                if (sub & 0x80000000u) {
                    while (t[n + k] & 0x80000000u) ++k;
                    ++k;
                }
                reads->push_back({t[n + k], mask});
            }
            n += len;
        }
    }
    return n;
}

// Rename temps (+shift) and redirect cb0[4..7] -> cb2[6..9] inside one operand, recursively.
// Returns its length (0 = malformed).
uint32_t PatchOperand(uint32_t* t, uint32_t avail, uint32_t shift, int* cbSwaps) {
    if (avail == 0) return 0;
    uint32_t n = 0;
    const uint32_t tok = t[n++];
    const uint32_t type = (tok >> 12) & 0xFF;
    const uint32_t dims = (tok >> 20) & 3;
    const uint32_t numComp = tok & 3;
    if (tok & 0x80000000u) {
        uint32_t ext;
        do {
            if (n >= avail) return 0;
            ext = t[n++];
        } while (ext & 0x80000000u);
    }
    if (type == OT::Imm32 || type == OT::Imm64) {
        const uint32_t comps = numComp == 1 ? 1 : (numComp == 2 ? 4 : 0);
        const uint32_t len = comps * (type == OT::Imm64 ? 2 : 1);
        return n + len <= avail ? n + len : 0;
    }
    int  immPos[3] = {-1, -1, -1};
    bool rel[3]    = {false, false, false};
    for (uint32_t d = 0; d < dims; ++d) {
        const uint32_t rep = (tok >> (22 + 3 * d)) & 7;
        if (rep == 0 || rep == 3) {
            if (n >= avail) return 0;
            immPos[d] = static_cast<int>(n);
            ++n;
        } else if (rep == 1 || rep == 4) {
            n += 2;
            if (n > avail) return 0;
        }
        if (rep >= 2) {
            rel[d] = true;
            const uint32_t len = PatchOperand(t + n, avail - n, shift, cbSwaps);
            if (len == 0) return 0;
            n += len;
        }
    }
    if (type == OT::Temp && dims == 1 && immPos[0] >= 0 && !rel[0]) t[immPos[0]] += shift;
    if (type == OT::ConstantBuffer && dims == 2 && immPos[0] >= 0 && immPos[1] >= 0 && !rel[0] && !rel[1] &&
        t[immPos[0]] == kGlobalCb && t[immPos[1]] >= kWorldToCamera && t[immPos[1]] < kWorldToCamera + 4) {
        t[immPos[0]] = kLightCb;
        t[immPos[1]] = t[immPos[1]] - kWorldToCamera + kTextureProj;
        if (cbSwaps) ++*cbSwaps;
    }
    return n;
}

// Per instruction: what it writes and reads, in temps and the two windows.
struct Info {
    std::vector<Read> dstTemps;
    uint32_t          outReg  = ~0u;   // an output destination (o#), if any
    uint32_t          outMask = 0;
    std::vector<Read> reads;
    bool              readsW2C   = false;   // cb0[4..7]
    bool              readsShadow = false;  // cb2[10..13]
    bool              readsCarrier = false; // cb2[6..9]: only a copy this rewriter made reads them
    bool              cb0Relative = false;
};

bool Describe(const Dxbc::Program& p, const Dxbc::Instr& in, Info* info, std::string* why) {
    const int nd = DstCount(in);
    uint32_t dstUnion = 0;
    for (int i = 0; i < nd && i < static_cast<int>(in.operands.size()); ++i) {
        const Dxbc::Operand& o = in.operands[static_cast<size_t>(i)];
        if (o.type == kOperandNull) continue;
        const uint32_t mask = o.numComp == 2 && o.selMode == 0 ? (o.compBits & 0xF) : 0xF;
        dstUnion |= mask;
        if (o.type == OT::Temp) {
            if (o.relative) { *why = "relative temp destination"; return false; }
            info->dstTemps.push_back({o.index[0], mask});
        } else if (o.type == OT::Output) {
            if (o.relative) { *why = "relative output destination"; return false; }
            info->outReg  = o.index[0];
            info->outMask = mask;
        } else {
            *why = "unexpected destination type";
            return false;
        }
    }
    if (dstUnion == 0) dstUnion = 0xF;
    const int span = SourceSpan(in.opcode);
    for (size_t i = static_cast<size_t>(nd); i < in.operands.size(); ++i) {
        const Dxbc::Operand& o = in.operands[i];
        // relative sub-operands first (their temps are reads too)
        const uint32_t len = RelativeReads(p.tokens.data() + o.offset, o.length, &info->reads, &info->cb0Relative);
        if (len != o.length) { *why = "operand re-walk mismatch"; return false; }
        if (o.type == OT::ConstantBuffer && o.indexDim == 2) {
            if (o.index[0] == kGlobalCb && o.index[1] >= kWorldToCamera && o.index[1] < kWorldToCamera + 4)
                info->readsW2C = true;
            if (o.index[0] == kLightCb && o.index[1] >= kShadowProj0 && o.index[1] < kShadowProj0 + 4)
                info->readsShadow = true;
            if (o.index[0] == kLightCb && o.index[1] >= kTextureProj && o.index[1] < kTextureProj + 4)
                info->readsCarrier = true;
        }
        if (o.type != OT::Temp) continue;
        uint32_t mask = 0;
        if (o.numComp != 2) {
            mask = 0xF;
        } else if (o.selMode == 2) {
            mask = 1u << (o.compBits & 3);
        } else if (o.selMode == 0) {
            mask = o.compBits & 0xF;
        } else if (span < 0) {
            for (int c = 0; c < 4; ++c)
                if (dstUnion & (1u << c)) mask |= 1u << o.Swz(c);
        } else {
            for (int c = 0; c < span; ++c) mask |= 1u << o.Swz(c);
        }
        info->reads.push_back({o.index[0], mask});
    }
    return true;
}

bool Refuse(Match* m, const std::string& why) {
    m->matched = false;
    m->why = why;
    return false;
}

}  // namespace

void Analyse(const uint8_t* blob, size_t size, Match* m) {
    *m = Match();
    std::string err;
    if (!Dxbc::Parse(blob, size, &m->cont, &err)) { Refuse(m, "container: " + err); return; }
    m->container = true;
    const Dxbc::Chunk* shdr = m->cont.Find(Dxbc::FourCC('S', 'H', 'D', 'R'));
    if (shdr == nullptr) shdr = m->cont.Find(Dxbc::FourCC('S', 'H', 'E', 'X'));
    if (shdr == nullptr) { Refuse(m, "no SHDR / SHEX chunk"); m->container = false; return; }
    // The container must round-trip byte-identical (same chunk order, same checksum) before it is touched.
    {
        const std::vector<uint8_t> again = Dxbc::Emit(m->cont);
        if (again.size() != size || std::memcmp(again.data(), blob, size) != 0) {
            Refuse(m, "container does not round-trip");
            return;
        }
    }
    if (!Dxbc::ParseProgram(*shdr, &m->prog, &err)) { Refuse(m, "program: " + err); return; }
    m->program = true;
    if (m->prog.ProgramType() != 1) { Refuse(m, "not a vertex shader"); return; }
    m->vertex = true;
    if (m->prog.Major() != 4) { Refuse(m, "not shader model 4"); return; }

    const Dxbc::Program& p = m->prog;
    const size_t n = p.instrs.size();
    uint32_t cb0Rows = 0;
    std::vector<Info> info(n);
    bool anyW2C = false, anyShadow = false;
    int  lastReal = -1;
    for (size_t i = 0; i < n; ++i) {
        const Dxbc::Instr& in = p.instrs[i];
        if (in.isDecl) {
            if (in.opcode == Op::DclTemps && in.length >= 2) m->temps = p.tokens[in.offset + 1];
            if (in.opcode == kDclIndexableTemp) { Refuse(m, "indexable temps"); return; }
            if (in.opcode == Op::DclConstantBuffer && in.length >= 4) {
                const uint32_t idx0 = p.tokens[in.offset + 2], idx1 = p.tokens[in.offset + 3];
                if (idx0 == kLightCb) m->cb2Rows = idx1;
                if (idx0 == kGlobalCb) cb0Rows = idx1;
            }
            continue;
        }
        if (IsFlowControl(in.opcode)) { Refuse(m, "flow control"); return; }
        if (in.token & 0x80000000u) { Refuse(m, "extended opcode token"); return; }
        std::string why;
        if (!Describe(p, in, &info[i], &why)) { Refuse(m, why); return; }
        if (info[i].readsCarrier) { Refuse(m, "already reads lc.textureProjection (a rewritten copy)"); return; }
        anyW2C    = anyW2C || info[i].readsW2C;
        anyShadow = anyShadow || info[i].readsShadow;
        lastReal  = static_cast<int>(i);
    }
    m->candidate = anyW2C && anyShadow;
    if (!m->candidate) { Refuse(m, "does not read both gc.worldToCamera and lc.shadowProjection[0]"); return; }
    if (lastReal < 0 || p.instrs[static_cast<size_t>(lastReal)].opcode != Op::Ret) {
        Refuse(m, "ret is not the last instruction");
        return;
    }
    for (int i = 0; i < lastReal; ++i)
        if (!p.instrs[static_cast<size_t>(i)].isDecl && p.instrs[static_cast<size_t>(i)].opcode == Op::Ret) {
            Refuse(m, "more than one ret");
            return;
        }
    if (m->temps == 0 || m->temps > 2048) { Refuse(m, "dcl_temps missing or too large"); return; }
    for (size_t i = 0; i < n; ++i) {
        for (const Read& r : info[i].reads)
            if (r.reg >= m->temps) { Refuse(m, "temp index past dcl_temps"); return; }
        for (const Read& d : info[i].dstTemps)
            if (d.reg >= m->temps) { Refuse(m, "temp index past dcl_temps"); return; }
    }
    if (m->cb2Rows < kTextureProj + 4) { Refuse(m, "cb2 declared too small for textureProjection"); return; }
    if (cb0Rows < kWorldToCamera + 4) { Refuse(m, "cb0 declared too small"); return; }

    // The shadow output: the one output written by instructions that read cb2[10..13].
    bool haveOut = false;
    for (size_t i = 0; i < n; ++i) {
        if (!info[i].readsShadow || info[i].outReg == ~0u) continue;
        if (haveOut && info[i].outReg != m->shadowOut) { Refuse(m, "two shadow outputs"); return; }
        m->shadowOut = info[i].outReg;
        haveOut = true;
    }
    if (!haveOut) { Refuse(m, "no output written from lc.shadowProjection[0]"); return; }

    // Backward slice of the shadow output, component by component (straight-line code: one reaching
    // definition per component).
    const uint32_t T = m->temps;
    std::vector<uint32_t> needed(T, 0);
    uint32_t neededOut = 0xF;
    m->inSlice.assign(n, 0);
    m->writesOut.assign(n, 0);
    for (size_t k = n; k-- > 0;) {
        const Dxbc::Instr& in = p.instrs[k];
        if (in.isDecl) continue;
        const Info& f = info[k];
        bool mark = false;
        if (f.outReg == m->shadowOut && (f.outMask & neededOut)) {
            mark = true;
            m->writesOut[k] = 1;
        }
        for (const Read& d : f.dstTemps) {
            if (d.reg >= T) { Refuse(m, "temp index past dcl_temps"); return; }
            if (needed[d.reg] & d.mask) mark = true;
        }
        if (!mark) continue;
        m->inSlice[k] = 1;
        if (f.outReg != ~0u && f.outReg != m->shadowOut) { Refuse(m, "a slice instruction writes another output"); return; }
        if (f.cb0Relative) { Refuse(m, "relative cb0 read in the slice"); return; }
        if (m->writesOut[k]) neededOut &= ~f.outMask;
        for (const Read& d : f.dstTemps) needed[d.reg] &= ~d.mask;
        for (const Read& r : f.reads) {
            if (r.reg >= T) { Refuse(m, "temp index past dcl_temps"); return; }
            needed[r.reg] |= r.mask;
        }
    }

    // Taint forward (instruction granularity): what depends on cb0[4..7].
    std::vector<uint32_t> taint(T, 0);
    bool sliceReadsW2C = false;
    for (size_t i = 0; i < n; ++i) {
        if (p.instrs[i].isDecl) continue;
        const Info& f = info[i];
        bool t = f.readsW2C;
        for (const Read& r : f.reads)
            if (taint[r.reg] & r.mask) t = true;
        for (const Read& d : f.dstTemps) taint[d.reg] = t ? (taint[d.reg] | d.mask) : (taint[d.reg] & ~d.mask);
        if (m->inSlice[i] && f.readsW2C) sliceReadsW2C = true;
        if (m->inSlice[i] && t && m->first < 0) m->first = static_cast<int>(i);
    }
    if (!sliceReadsW2C || m->first < 0) { Refuse(m, "the shadow output does not depend on gc.worldToCamera"); return; }

    // TS-FIX: the projection's inputs (receiver_rewrite.h "THE PROJECTION").
    // N: the first camera-dependent instruction must be `mul rX, rN.<rep>, cb0[4..6]`.
    {
        const Dxbc::Instr& in = p.instrs[static_cast<size_t>(m->first)];
        bool ok = in.opcode == Op::Mul && in.operands.size() == 3;
        int  nTemp = -1;
        if (ok) {
            for (int k = 1; k <= 2; ++k) {
                const Dxbc::Operand& o = in.operands[static_cast<size_t>(k)];
                if (o.type == OT::Temp && !o.relative) nTemp = static_cast<int>(o.index[0]);
                else if (!(o.type == OT::ConstantBuffer && o.index[0] == kGlobalCb && o.index[1] >= kWorldToCamera &&
                           o.index[1] < kWorldToCamera + 3)) ok = false;
            }
        }
        if (!ok || nTemp < 0) { Refuse(m, "first camera-dependent instruction is not rX = rN * camera row"); return; }
        m->normalReg = static_cast<uint32_t>(nTemp);
    }
    // P and the insertion point: the slice instructions that read cb2[10..12] all read one position
    // temp (with a replicated swizzle); the first of them (or of any cb2[10..13] reader) is where the
    // projection goes.
    {
        int posTemp = -1;
        for (size_t i = 0; i < n; ++i) {
            if (!m->inSlice[i] || !info[i].readsShadow) continue;
            if (m->insertAt < 0) m->insertAt = static_cast<int>(i);
            const Dxbc::Instr& in = p.instrs[i];
            int row = -1;
            for (const Dxbc::Operand& o : in.operands)
                if (o.type == OT::ConstantBuffer && o.index[0] == kLightCb) row = static_cast<int>(o.index[1]);
            if (row < static_cast<int>(kShadowProj0) || row > static_cast<int>(kShadowProj0) + 2) continue;
            int found = -1;
            for (size_t k = 1; k < in.operands.size(); ++k) {
                const Dxbc::Operand& o = in.operands[k];
                if (o.type == OT::Temp && o.selMode == 1 && o.Swz(0) == o.Swz(1) && o.Swz(1) == o.Swz(2) &&
                    o.Swz(2) == o.Swz(3) && o.Swz(0) == static_cast<uint32_t>(row - static_cast<int>(kShadowProj0)))
                    found = static_cast<int>(o.index[0]);
            }
            if (found < 0 || (posTemp >= 0 && found != posTemp)) { Refuse(m, "shadow-matrix instructions: no single position temp"); return; }
            posTemp = found;
        }
        if (posTemp < 0 || m->insertAt <= m->first) { Refuse(m, "no position temp feeding lc.shadowProjection[0]"); return; }
        m->posReg = static_cast<uint32_t>(posTemp);
    }
    // N's xyz must still hold the normal at the insertion point (originals only write r[0..T)).
    for (int i = m->first; i < m->insertAt; ++i)
        for (const Read& d : info[static_cast<size_t>(i)].dstTemps)
            if (d.reg == m->normalReg && (d.mask & 7)) { Refuse(m, "the normal temp is overwritten before the shadow lookup"); return; }
    // The card code's 0.5 tilt (`mad ..., l(0.5, 0.5, 0.5, x), ...`) must be in the slice: the plane
    // normal above assumes it.
    {
        bool half = false;
        for (size_t i = static_cast<size_t>(m->first); i < n && !half; ++i) {
            if (!m->inSlice[i] || p.instrs[i].opcode != Op::Mad) continue;
            for (const Dxbc::Operand& o : p.instrs[i].operands)
                if (o.type == OT::Imm32 && o.numComp == 2 && o.ImmF(0) == 0.5f && o.ImmF(1) == 0.5f && o.ImmF(2) == 0.5f)
                    half = true;
        }
        if (!half) { Refuse(m, "no 0.5 tilt in the card code"); return; }
    }
    for (size_t i = static_cast<size_t>(m->first); i < n; ++i) {
        if (!m->inSlice[i]) continue;
        ++m->sliceCount;
        if (info[i].readsW2C) ++m->cbReads;
    }
    m->matched = true;
    m->why.clear();
}

namespace {

// cb<cb>[row] with a replicated component.
void CbRep(std::vector<uint32_t>& t, uint32_t cb, uint32_t row, uint32_t comp) {
    t.push_back(2u | (1u << 2) | ((Dxbc::Enc::Rep(comp) & 0xFF) << 4) | (OT::ConstantBuffer << 12) | (2u << 20));
    t.push_back(cb);
    t.push_back(row);
}

// The TS-FIX projection block (receiver_rewrite.h "THE PROJECTION"): rTw.xyz = P' from rP (P), rTw (T),
// rN (N) and the sun basis in cb2[6..8]. A, B, C are three scratch temps. 26 instructions.
void EmitProjection(std::vector<uint32_t>& t, uint32_t P, uint32_t Tw, uint32_t N, uint32_t A, uint32_t B,
                    uint32_t C) {
    namespace E = Dxbc::Enc;
    const uint32_t XYZ = E::Swizzle(0, 1, 2, 0);
    // A.xyz = R, B.xyz = U, C.xyz = f: the .x / .z / .y columns of cb2[6..8]
    const uint32_t col[3] = {0, 2, 1};
    const uint32_t reg[3] = {A, B, C};
    for (int v = 0; v < 3; ++v)
        for (uint32_t k = 0; k < 3; ++k) {
            t.push_back(E::Opcode(Op::Mov, 6));
            E::TempDst(t, reg[v], 1u << k);
            CbRep(t, kLightCb, kTextureProj + k, col[v]);
        }
    auto op3 = [&](uint32_t opc, uint32_t dst, uint32_t mask, auto srcA, auto srcB) {
        const size_t at = t.size();
        t.push_back(0);
        E::TempDst(t, dst, mask);
        srcA();
        srcB();
        t[at] = E::Opcode(opc, static_cast<uint32_t>(t.size() - at));
    };
    // A.w = 0.5 * (N . R), B.w = 0.5 * (N . U)
    op3(kDp3, A, 8, [&] { E::TempSrc(t, N, XYZ); }, [&] { E::TempSrc(t, A, XYZ); });
    op3(kDp3, B, 8, [&] { E::TempSrc(t, N, XYZ); }, [&] { E::TempSrc(t, B, XYZ); });
    op3(Op::Mul, A, 8, [&] { E::TempSrc(t, A, E::Rep(3)); }, [&] { E::ImmScalar(t, 0.5f); });
    op3(Op::Mul, B, 8, [&] { E::TempSrc(t, B, E::Rep(3)); }, [&] { E::ImmScalar(t, 0.5f); });
    // A.xyz = n = R * A.w + U * B.w - f
    op3(Op::Mul, A, 7, [&] { E::TempSrc(t, A, XYZ); }, [&] { E::TempSrc(t, A, E::Rep(3)); });
    {
        const size_t at = t.size();
        t.push_back(0);
        E::TempDst(t, A, 7);
        E::TempSrc(t, B, XYZ);
        E::TempSrc(t, B, E::Rep(3));
        E::TempSrc(t, A, XYZ);
        t[at] = E::Opcode(Op::Mad, static_cast<uint32_t>(t.size() - at));
    }
    op3(Op::Add, A, 7, [&] { E::TempSrc(t, A, XYZ); }, [&] { E::TempSrc(t, C, XYZ, 1); });
    // B.xyz = P - T; B.w = (P - T) . n; C.w = f . n; B.w /= C.w
    op3(Op::Add, B, 7, [&] { E::TempSrc(t, P, XYZ); }, [&] { E::TempSrc(t, Tw, XYZ, 1); });
    op3(kDp3, B, 8, [&] { E::TempSrc(t, B, XYZ); }, [&] { E::TempSrc(t, A, XYZ); });
    op3(kDp3, C, 8, [&] { E::TempSrc(t, C, XYZ); }, [&] { E::TempSrc(t, A, XYZ); });
    op3(Op::Div, B, 8, [&] { E::TempSrc(t, B, E::Rep(3)); }, [&] { E::TempSrc(t, C, E::Rep(3)); });
    // Tw.xyz = P - f * B.w
    {
        const size_t at = t.size();
        t.push_back(0);
        E::TempDst(t, Tw, 7);
        E::TempSrc(t, C, XYZ, 1);
        E::TempSrc(t, B, E::Rep(3));
        E::TempSrc(t, P, XYZ);
        t[at] = E::Opcode(Op::Mad, static_cast<uint32_t>(t.size() - at));
    }
}

constexpr int kProjectionInstrs = 9 + 4 + 3 + 4 + 1;   // movs, dp/scale, n, d / dots / div, the mad

}  // namespace

bool Transform(const Match& m, std::vector<uint8_t>* out) {
    if (!m.matched) return false;
    const Dxbc::Program& p = m.prog;
    const uint32_t T = m.temps;
    std::vector<uint32_t> t;
    t.reserve(p.tokens.size() * 2 + T * 5);
    int cbSwaps = 0;
    for (size_t i = 0; i < p.instrs.size(); ++i) {
        const Dxbc::Instr& in = p.instrs[i];
        const uint32_t* src = p.tokens.data() + in.offset;
        if (in.isDecl) {
            if (in.opcode == Op::DclTemps) {
                t.push_back(src[0]);
                t.push_back(T * 2 + 3);   // the second set + the projection's three scratch temps
            } else {
                t.insert(t.end(), src, src + in.length);
            }
            continue;
        }
        if (static_cast<int>(i) == m.first) {
            // seed the second temp set from the first: every slice value computed so far is shared
            for (uint32_t r = 0; r < T; ++r) {
                t.push_back(Dxbc::Enc::Opcode(Op::Mov, 5));
                Dxbc::Enc::TempDst(t, r + T, 0xF);
                Dxbc::Enc::TempSrc(t, r, Dxbc::Enc::Swizzle(0, 1, 2, 3));
            }
        }
        if (static_cast<int>(i) == m.insertAt)
            EmitProjection(t, m.posReg, m.posReg + T, m.normalReg, 2 * T, 2 * T + 1, 2 * T + 2);
        const bool clone = m.inSlice[i] && static_cast<int>(i) >= m.first;
        if (clone) {
            const size_t at = t.size();
            t.insert(t.end(), src, src + in.length);
            uint32_t k = 1;   // no extended opcode tokens (Analyse refused them)
            while (k < in.length) {
                const uint32_t len = PatchOperand(t.data() + at + k, in.length - k, T, &cbSwaps);
                if (len == 0) return false;
                k += len;
            }
        }
        if (!(clone && m.writesOut[i])) t.insert(t.end(), src, src + in.length);   // the original
    }
    if (cbSwaps != m.cbReads) return false;

    Dxbc::Program np;
    np.version = p.version;
    np.tokens  = std::move(t);
    Dxbc::Container c = m.cont;
    Dxbc::Chunk* shdr = c.Find(Dxbc::FourCC('S', 'H', 'D', 'R'));
    if (shdr == nullptr) shdr = c.Find(Dxbc::FourCC('S', 'H', 'E', 'X'));
    if (shdr == nullptr) return false;
    shdr->data = Dxbc::EmitProgram(np);
    std::vector<uint8_t> blob = Dxbc::Emit(c);

    // Self-check: the copy parses, its checksum verifies, and it has exactly the expected shape.
    Dxbc::Container rc;
    std::string err;
    if (!Dxbc::Parse(blob.data(), blob.size(), &rc, &err)) return false;
    uint32_t sum[4];
    Dxbc::Checksum(blob.data(), blob.size(), sum);
    if (std::memcmp(sum, rc.checksum, 16) != 0) return false;
    const Dxbc::Chunk* rs = rc.Find(shdr->fourcc);
    Dxbc::Program rp;
    if (rs == nullptr || !Dxbc::ParseProgram(*rs, &rp, &err)) return false;
    size_t replaced = 0;
    for (uint8_t w : m.writesOut) replaced += w;
    if (rp.instrs.size() != p.instrs.size() + T + static_cast<size_t>(m.sliceCount) - replaced + kProjectionInstrs)
        return false;
    *out = std::move(blob);
    return true;
}

}  // namespace BbsRewrite
