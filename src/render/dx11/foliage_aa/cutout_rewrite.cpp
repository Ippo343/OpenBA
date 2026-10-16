// src/render/dx11/foliage_aa/cutout_rewrite.cpp -- see cutout_rewrite.h (FAA-SPIKE).
#include "foliage_aa/cutout_rewrite.h"

#include <cstdio>
#include <cstring>

namespace FoliageAA {

using Dxbc::Instr;
using Dxbc::Operand;
using Dxbc::Program;
namespace Op = Dxbc::Op;
namespace OT = Dxbc::OperandType;
namespace Enc = Dxbc::Enc;

namespace {
// ---- the pattern -------------------------------------------------------------


bool IsSample(uint32_t op) {
    return op == Op::Sample || op == Op::SampleB || op == Op::SampleL || op == Op::SampleD;
}
bool IsFlowControl(uint32_t op) {
    // break breakc call callc case continue continuec default else endif endloop endswitch if
    // loop retc switch
    return op == 2 || op == 3 || op == 4 || op == 5 || op == 6 || op == 7 || op == 8 || op == 10 ||
           op == 18 || op == 21 || op == 22 || op == 23 || op == 31 || op == 48 || op == 63 || op == 76;
}
uint32_t ReadComp(const Operand& o) {   // the one component a scalar source reads
    if (o.selMode == 2) return o.compBits & 3;
    return o.Swz(0);
}
bool IsImm(const Operand& o, float v) {
    if (o.type != OT::Imm32) return false;
    if (o.numComp == 1) return o.ImmF(0) == v;
    return false;
}
bool IsScalarTempDst(const Operand& o, uint32_t reg, uint32_t comp) {
    return o.type == OT::Temp && o.index[0] == reg && o.selMode == 0 && o.compBits == (1u << comp);
}
// For a scalar-destination instruction (mask one bit d), the component src reads for it.
uint32_t SrcCompFor(const Operand& dst, const Operand& src) {
    uint32_t d = 0;
    while (d < 4 && !(dst.compBits & (1u << d))) ++d;
    if (src.selMode == 2) return src.compBits & 3;
    return src.Swz(static_cast<int>(d));
}

}  // namespace

void Analyse(const uint8_t* blob, size_t size, Match* m) {
    if (!Dxbc::Parse(blob, size, &m->cont, &m->err)) return;
    m->container = true;
    uint32_t sum[4];
    Dxbc::Checksum(blob, size, sum);
    m->checksum = std::memcmp(sum, m->cont.checksum, 16) == 0;
    Dxbc::Chunk* shdr = m->cont.Find(Dxbc::FourCC('S', 'H', 'D', 'R'));
    if (!shdr) shdr = m->cont.Find(Dxbc::FourCC('S', 'H', 'E', 'X'));
    if (!shdr) { m->err = "no SHDR"; return; }
    if (!Dxbc::ParseProgram(*shdr, &m->prog, &m->err)) return;
    m->program = true;
    // identity: SHDR re-emitted from the parsed tokens, the container re-emitted (fresh offsets,
    // sizes and checksum) -- must equal the input byte for byte
    {
        Dxbc::Container c = m->cont;
        c.Find(shdr->fourcc)->data = Dxbc::EmitProgram(m->prog);
        const std::vector<uint8_t> out = Dxbc::Emit(c);
        m->identity = out.size() == size && std::memcmp(out.data(), blob, size) == 0;
    }
    const std::vector<Instr>& in = m->prog.instrs;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i].opcode == Op::Discard) { ++m->discards; m->iDiscard = static_cast<int>(i); }
        if (IsFlowControl(in[i].opcode)) ++m->flowControl;
        if (in[i].opcode == Op::Ret) { ++m->rets; m->iRet = static_cast<int>(i); }
        if (in[i].opcode == Op::DclTemps) {
            m->iTemps = static_cast<int>(i);
            m->temps  = m->prog.tokens[in[i].offset + 1];
        }
        if (in[i].opcode == Op::Mov && in[i].operands.size() == 2 && in[i].operands[0].type == OT::Output &&
            in[i].operands[0].index[0] == 0 && in[i].operands[0].compBits == 8 && IsImm(in[i].operands[1], 0.0f)) {
            ++m->o0wCount;
            m->iO0w = static_cast<int>(i);
        }
    }
    if (m->discards == 0) { m->why = "no discard"; return; }
    if (m->discards > 1) { m->why = "more than one discard"; return; }
    const int d = m->iDiscard;
    const Instr& disc = in[d];
    if (!disc.TestNonZero()) { m->why = "discard_z"; return; }
    if (disc.operands.size() != 1 || disc.operands[0].type != OT::Temp) { m->why = "discard operand"; return; }
    const uint32_t rT = disc.operands[0].index[0];
    const uint32_t cT = ReadComp(disc.operands[0]);
    if (d < 2) { m->why = "discard too early"; return; }
    const Instr& lt = in[d - 1];
    if (lt.opcode != Op::Lt || lt.operands.size() != 3 || !IsScalarTempDst(lt.operands[0], rT, cT) ||
        lt.operands[1].type != OT::Temp || lt.operands[1].index[0] != rT || SrcCompFor(lt.operands[0], lt.operands[1]) != cT ||
        !IsImm(lt.operands[2], 0.0f)) {
        m->why = "no `lt rT, rT, l(0)` before the discard: " + Dxbc::Describe(m->prog, lt);
        return;
    }
    const Instr& add = in[d - 2];
    if (add.opcode != Op::Add || add.operands.size() != 3 || !IsScalarTempDst(add.operands[0], rT, cT)) {
        m->why = "no `add rT, a, l(-cut)` before the lt: " + Dxbc::Describe(m->prog, add);
        return;
    }
    int immAt = -1;
    for (int k = 1; k <= 2; ++k)
        if (add.operands[k].type == OT::Imm32 && add.operands[k].numComp == 1) immAt = k;
    const int srcAt = immAt == 1 ? 2 : 1;
    if (immAt < 0 || add.operands[srcAt].type != OT::Temp || add.operands[srcAt].modifier != 0) {
        m->why = "add operands: " + Dxbc::Describe(m->prog, add);
        return;
    }
    m->cut       = -add.operands[immAt].ImmF(0);
    m->alpha     = add.operands[srcAt];
    m->alphaComp = SrcCompFor(add.operands[0], add.operands[srcAt]);
    m->iLt = d - 1;
    m->iAdd = d - 2;
    // the writer chain of rA.c: an optional season `mad`, then a sample
    const uint32_t rA = m->alpha.index[0];
    for (int i = d - 3; i >= 0; --i) {
        const Instr& w = in[i];
        if (w.isDecl || w.operands.empty()) continue;
        const Operand& dst = w.operands[0];
        if (dst.type != OT::Temp || dst.index[0] != rA || dst.selMode != 0 || !(dst.compBits & (1u << m->alphaComp)))
            continue;
        if (w.opcode == Op::Mad && m->iMad < 0) { m->iMad = i; continue; }
        if (IsSample(w.opcode)) { m->iSample = i; m->sampleOp = w.opcode; break; }
        m->why = "alpha written by: " + Dxbc::Describe(m->prog, w);
        return;
    }
    if (m->iSample < 0) { m->why = "no sample writes the alpha"; return; }
    if (m->iTemps < 0) { m->why = "no dcl_temps"; return; }
    if (m->rets != 1 || m->iRet != static_cast<int>(in.size()) - 1) { m->why = "ret not single / last"; return; }
    m->pattern = true;
    m->colour  = m->o0wCount == 1 && m->flowControl == 0;
    m->why     = m->colour ? "colour" : (m->o0wCount == 0 ? "no `mov o0.w, l(0)` (depth/other)" : "o0.w or flow");
}

// ---- the emit ----------------------------------------------------------------
// Build the transformed SHDR for kMode 2..6. Returns false when the mode does not apply.
bool Transform(const Match& m, int mode, float fillK, bool fillAniso, std::vector<uint8_t>* out) {
    const Program& p = m.prog;
    const std::vector<Instr>& in = p.instrs;
    const uint32_t N = m.temps, M = m.temps + 1, P = m.temps + 2;   // our temps, never retail's
    const Instr& smp = in[m.iSample];
    const Operand& uv  = smp.operands[1];
    const Operand& res = smp.operands[2];
    const Operand& spl = smp.operands[3];
    const uint32_t sx = uv.Swz(0), sy = uv.Swz(1);
    const bool fill = mode == 4 || mode == 6;
    const bool sharpen = mode == 3 || mode == 4;
    const bool dropDiscard = mode == 2 || mode == 3 || mode == 4;

    auto emitLod = [&](std::vector<uint32_t>& t, uint32_t dstReg, bool aniso) {
        // M.zw = texture size; N = (ddx(uv), ddy(uv)) * size; dstReg.x = lod
        t.push_back(Enc::Opcode(Op::ResInfo, 0));
        size_t at = t.size() - 1;
        Enc::TempDst(t, M, 4 | 8);
        Enc::ImmScalar(t, 0.0f);
        Enc::CopyWithSwizzle(t, p, res, Enc::Swizzle(0, 1, 0, 1));
        t[at] = Enc::Opcode(Op::ResInfo, static_cast<uint32_t>(t.size() - at));
        at = t.size();
        t.push_back(0);
        Enc::TempDst(t, N, 1 | 2);
        Enc::CopyWithSwizzle(t, p, uv, Enc::Swizzle(sx, sy, sx, sy));
        t[at] = Enc::Opcode(Op::DerivRtx, static_cast<uint32_t>(t.size() - at));
        at = t.size();
        t.push_back(0);
        Enc::TempDst(t, N, 4 | 8);
        Enc::CopyWithSwizzle(t, p, uv, Enc::Swizzle(sx, sy, sx, sy));
        t[at] = Enc::Opcode(Op::DerivRty, static_cast<uint32_t>(t.size() - at));
        auto op3 = [&](uint32_t op, uint32_t dreg, uint32_t dmask, uint32_t ra, uint32_t sa, uint32_t rb, uint32_t sb) {
            const size_t a = t.size();
            t.push_back(0);
            Enc::TempDst(t, dreg, dmask);
            Enc::TempSrc(t, ra, sa);
            Enc::TempSrc(t, rb, sb);
            t[a] = Enc::Opcode(op, static_cast<uint32_t>(t.size() - a));
        };
        auto opImm = [&](uint32_t op, uint32_t dreg, uint32_t dmask, uint32_t ra, uint32_t sa, float v) {
            const size_t a = t.size();
            t.push_back(0);
            Enc::TempDst(t, dreg, dmask);
            Enc::TempSrc(t, ra, sa);
            Enc::ImmScalar(t, v);
            t[a] = Enc::Opcode(op, static_cast<uint32_t>(t.size() - a));
        };
        op3(Op::Mul, N, 0xF, N, Enc::Swizzle(0, 1, 2, 3), M, Enc::Swizzle(2, 3, 2, 3));
        op3(Op::Dp2, M, 1, N, Enc::Swizzle(0, 1, 0, 0), N, Enc::Swizzle(0, 1, 0, 0));   // |ddx|^2
        op3(Op::Dp2, M, 2, N, Enc::Swizzle(2, 3, 2, 2), N, Enc::Swizzle(2, 3, 2, 2));   // |ddy|^2
        if (aniso) {
            op3(Op::Max, M, 4, M, Enc::Rep(0), M, Enc::Rep(1));          // Pmax^2
            op3(Op::Min, M, 8, M, Enc::Rep(0), M, Enc::Rep(1));          // Pmin^2
            opImm(Op::Mul, M, 4, M, Enc::Rep(2), 1.0f / 256.0f);         // (Pmax/16)^2
            op3(Op::Max, M, 4, M, Enc::Rep(2), M, Enc::Rep(3));
        } else {
            op3(Op::Max, M, 4, M, Enc::Rep(0), M, Enc::Rep(1));
        }
        {   // log M.z
            const size_t a = t.size();
            t.push_back(0);
            Enc::TempDst(t, M, 4);
            Enc::TempSrc(t, M, Enc::Rep(2));
            t[a] = Enc::Opcode(Op::Log, static_cast<uint32_t>(t.size() - a));
        }
        opImm(Op::Mul, dstReg, 1, M, Enc::Rep(2), 0.5f);   // dstReg.x = lod
    };

    std::vector<uint32_t> t;
    t.reserve(p.tokens.size() + 128);
    for (size_t i = 0; i < in.size(); ++i) {
        const Instr& x = in[i];
        const auto copy = [&]() { t.insert(t.end(), p.tokens.begin() + x.offset, p.tokens.begin() + x.offset + x.length); };
        if (static_cast<int>(i) == m.iTemps) {
            t.push_back(p.tokens[x.offset]);
            t.push_back(m.temps + 3);
            continue;
        }
        if (static_cast<int>(i) == m.iAdd && (fill)) {
            // the fill block, then the retail add on the filled alpha (N.x)
            emitLod(t, P, fillAniso);
            size_t a = t.size();
            t.push_back(0);
            Enc::TempDst(t, P, 1);
            Enc::TempSrc(t, P, Enc::Rep(0));
            Enc::ImmScalar(t, 0.0f);
            t[a] = Enc::Opcode(Op::Max, static_cast<uint32_t>(t.size() - a));
            a = t.size();
            t.push_back(0);
            Enc::TempDst(t, P, 1);
            Enc::TempSrc(t, P, Enc::Rep(0));
            Enc::ImmScalar(t, fillK);
            Enc::ImmScalar(t, 1.0f);
            t[a] = Enc::Opcode(Op::Mad, static_cast<uint32_t>(t.size() - a));
            a = t.size();
            t.push_back(0);
            Enc::TempDst(t, P, 1);
            Enc::CopyWithSwizzle(t, p, m.alpha, Enc::Rep(m.alphaComp));
            Enc::TempSrc(t, P, Enc::Rep(0));
            t[a] = Enc::Opcode(Op::Mul, static_cast<uint32_t>(t.size() - a));   // P.x = filled alpha
            // the retail add, reading P.x instead of rA.c
            a = t.size();
            t.push_back(x.token & ~(0x7Fu << 24));
            Enc::Copy(t, p, x.operands[0]);
            for (int k = 1; k <= 2; ++k) {
                if (x.operands[k].type == OT::Imm32) Enc::Copy(t, p, x.operands[k]);
                else Enc::TempSrc(t, P, Enc::Rep(0));
            }
            t[a] = (x.token & ~(0x7Fu << 24)) | (static_cast<uint32_t>(t.size() - a) << 24);
            continue;
        }
        if (static_cast<int>(i) == m.iAdd && sharpen) {
            // save the alpha: the retail add usually writes its result over the alpha register
            const size_t a = t.size();
            t.push_back(0);
            Enc::TempDst(t, P, 1);
            Enc::CopyWithSwizzle(t, p, m.alpha, Enc::Rep(m.alphaComp));
            t[a] = Enc::Opcode(Op::Mov, static_cast<uint32_t>(t.size() - a));
            copy();
            continue;
        }
        if (static_cast<int>(i) == m.iDiscard) {
            if (!dropDiscard) copy();
            if (sharpen) {
                // P.x = alpha (filled or not, saved before the retail add overwrote it);
                // sharpened: saturate((a - cut) / max(fwidth(a), 1e-4) + 0.5)
                auto un = [&](uint32_t op, uint32_t dmask) {
                    const size_t a = t.size();
                    t.push_back(0);
                    Enc::TempDst(t, P, dmask);
                    Enc::TempSrc(t, P, Enc::Rep(0));
                    t[a] = Enc::Opcode(op, static_cast<uint32_t>(t.size() - a));
                };
                un(Op::DerivRtx, 2);
                un(Op::DerivRty, 4);
                size_t a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 2);
                Enc::TempSrc(t, P, Enc::Rep(1), 2);
                Enc::TempSrc(t, P, Enc::Rep(2), 2);
                t[a] = Enc::Opcode(Op::Add, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 2);
                Enc::TempSrc(t, P, Enc::Rep(1));
                Enc::ImmScalar(t, 0.0001f);
                t[a] = Enc::Opcode(Op::Max, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 1);
                Enc::TempSrc(t, P, Enc::Rep(0));
                Enc::ImmScalar(t, -m.cut);
                t[a] = Enc::Opcode(Op::Add, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 1);
                Enc::TempSrc(t, P, Enc::Rep(0));
                Enc::TempSrc(t, P, Enc::Rep(1));
                t[a] = Enc::Opcode(Op::Div, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 1);
                Enc::TempSrc(t, P, Enc::Rep(0));
                Enc::ImmScalar(t, 0.5f);
                t[a] = Enc::Opcode(Op::Add, static_cast<uint32_t>(t.size() - a), true);
            }
            if (mode == 5) {
                // P.x = iso lod, P.z = aniso lod, P.y = hardware lod (SM4.1 `lod`, unclamped .y)
                emitLod(t, P, false);
                const size_t a0 = t.size();
                emitLod(t, M, true);   // M.x = aniso (M.zw reused inside; done before the next block)
                (void)a0;
                size_t a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 4);
                Enc::TempSrc(t, M, Enc::Rep(0));
                t[a] = Enc::Opcode(Op::Mov, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, N, 1 | 2);
                Enc::CopyWithSwizzle(t, p, uv, Enc::Swizzle(sx, sy, sx, sy));
                Enc::Copy(t, p, res);
                Enc::Copy(t, p, spl);
                t[a] = Enc::Opcode(Op::Lod, static_cast<uint32_t>(t.size() - a));
                a = t.size();
                t.push_back(0);
                Enc::TempDst(t, P, 2);
                Enc::TempSrc(t, N, Enc::Rep(1));
                t[a] = Enc::Opcode(Op::Mov, static_cast<uint32_t>(t.size() - a));
            }
            continue;
        }
        if (static_cast<int>(i) == m.iO0w && sharpen) {
            const size_t a = t.size();
            t.push_back(0);
            Enc::OutputDst(t, 0, 8);
            Enc::TempSrc(t, P, Enc::Rep(0));
            t[a] = Enc::Opcode(Op::Mov, static_cast<uint32_t>(t.size() - a));
            continue;
        }
        if (static_cast<int>(i) == m.iRet && mode == 5) {
            const size_t a = t.size();
            t.push_back(0);
            Enc::OutputDst(t, 0, 7);
            Enc::TempSrc(t, P, Enc::Swizzle(0, 1, 2, 0));
            Enc::ImmScalar(t, 0.125f);
            t[a] = Enc::Opcode(Op::Mul, static_cast<uint32_t>(t.size() - a), true);
        }
        copy();
    }
    Program q = p;
    q.tokens = std::move(t);
    if (mode == 5) q.version = (q.version & ~0xFFu) | 0x41u;   // ps_4_1 for `lod`
    Dxbc::Container c = m.cont;
    Dxbc::Chunk* shdr = c.Find(Dxbc::FourCC('S', 'H', 'D', 'R'));
    shdr->data = Dxbc::EmitProgram(q);
    if (mode == 5) {
        Dxbc::Chunk* rdef = c.Find(Dxbc::FourCC('R', 'D', 'E', 'F'));
        if (rdef && rdef->data.size() >= 20) {
            uint32_t target;
            std::memcpy(&target, rdef->data.data() + 16, 4);
            if (target == 0xFFFF0400u) { target = 0xFFFF0401u; std::memcpy(rdef->data.data() + 16, &target, 4); }
        }
    }
    *out = Dxbc::Emit(c);
    // round-trip the result through our own parser: a malformed emit fails here, not in the driver
    Match chk;
    Analyse(out->data(), out->size(), &chk);
    if (!chk.program || !chk.identity || !chk.checksum) {
        std::fprintf(stderr, "[FoliageAA] transform self-check FAILED (%s)\n", chk.err.c_str());
        return false;
    }
    return true;
}


}  // namespace FoliageAA
