// ============================================================================
// tests/unit/ext_foliage_aa_test.cpp -- ext.foliage_aa's pure parts
// ============================================================================
// src/render/dx11/foliage_aa/dxbc.{h,cpp} (the DXBC container, its checksum, the
// SM4 token walk) and cutout_rewrite.{h,cpp} (the cutout grammar of FOLIAGE_AA.md
// §6.1 and the emit), plus the registry row / key (src/ext/registry.cpp).
//
// NO RETAIL BYTES: every blob here is SYNTHETIC, assembled token by token below
// in the shape FAA-SPIKE S2 documented (the instruction forms, not any shader's
// content): a ps_4_0 program with the cutout, the two rT variants (the alpha
// register itself, or a separate temp), the optional season `mad`, the four
// DETAIL variants' temp counts, and the near misses the matcher must refuse.
// ============================================================================
#include "unit.h"

#include "foliage_aa/cutout_rewrite.h"
#include "foliage_aa/dxbc.h"
#include "ext/registry.h"

#include <cstring>
#include <vector>

namespace {

namespace Op  = Dxbc::Op;
namespace OT  = Dxbc::OperandType;
namespace Enc = Dxbc::Enc;
using Tokens  = std::vector<uint32_t>;

// ---- operand helpers the encoder does not have (inputs, resources, selects) ----
uint32_t Comp4(uint32_t sel, uint32_t bits, uint32_t type, uint32_t dims) {
    return 2u | (sel << 2) | (bits << 4) | (type << 12) | (dims << 20);
}
void Sel1(Tokens& t, uint32_t type, uint32_t reg, uint32_t comp, uint32_t neg = 0) {
    t.push_back(Comp4(2, comp, type, 1) | (neg ? 0x80000000u : 0u));
    if (neg) t.push_back(1u | (1u << 6));
    t.push_back(reg);
}
void Swz(Tokens& t, uint32_t type, uint32_t reg, uint32_t swz) {
    t.push_back(Comp4(1, swz, type, 1));
    t.push_back(reg);
}
void Dst(Tokens& t, uint32_t type, uint32_t reg, uint32_t mask) {
    t.push_back(Comp4(0, mask, type, 1));
    t.push_back(reg);
}
void SamplerOp(Tokens& t, uint32_t reg) {
    t.push_back(0u | (OT::Sampler << 12) | (1u << 20));
    t.push_back(reg);
}
// -cb0[row].comp
void NegCb(Tokens& t, uint32_t row, uint32_t comp) {
    t.push_back(Comp4(2, comp, OT::ConstantBuffer, 2) | 0x80000000u);
    t.push_back(1u | (1u << 6));
    t.push_back(0);
    t.push_back(row);
}

struct Ins {
    Tokens& t;
    size_t  at;
    Ins(Tokens& tt) : t(tt), at(tt.size()) { t.push_back(0); }
    void End(uint32_t op, uint32_t extra = 0) { t[at] = Enc::Opcode(op, static_cast<uint32_t>(t.size() - at), false, extra); }
};
constexpr uint32_t kTestNonZero = 1u << 18;

struct Shape {
    bool     seasonMad  = false;   // G1: mad r0.w, -cb0[4].z, v2.w, r0.w
    bool     separateRt = false;   // G5 / G6: the test in r1.x instead of over r0.w
    uint32_t temps      = 3;       // 3 / 3 / 4 / 4 over DETAIL0..3
    int      filler     = 0;       // DETAIL variants differ in the lighting length
    // near misses
    bool     noO0w      = false;   // the depth programs: no `mov o0.w, l(0)`
    bool     discardZ   = false;
    bool     flow       = false;   // an if / endif around the lighting
    bool     waterClip  = false;   // lt on an interpolant, not on the add's result
    bool     movBetween = false;   // depth shape: a mov between the add and the lt
    float    cut        = 0.5f;
};

Tokens Program(const Shape& s) {
    Tokens t;
    { Ins i(t); i.End(Op::DclGlobalFlags, 1u << 11); }   // refactoringAllowed
    { Ins i(t); t.push_back(s.temps); i.End(Op::DclTemps); }
    // sample r0.xyzw, v1.xyxx, t0.xyzw, s0
    { Ins i(t); Dst(t, OT::Temp, 0, 0xF); Swz(t, OT::Input, 1, Enc::Swizzle(0, 1, 0, 0));
      Swz(t, OT::Resource, 0, Enc::Swizzle(0, 1, 2, 3)); SamplerOp(t, 0); i.End(Op::Sample); }
    if (s.seasonMad) {
        Ins i(t); Dst(t, OT::Temp, 0, 8); NegCb(t, 4, 2); Sel1(t, OT::Input, 2, 3); Sel1(t, OT::Temp, 0, 3);
        i.End(Op::Mad);
    }
    const uint32_t rT = s.separateRt ? 1 : 0, cT = s.separateRt ? 0 : 3;
    { Ins i(t); Dst(t, OT::Temp, rT, 1u << cT); Sel1(t, OT::Temp, 0, 3); Enc::ImmScalar(t, -s.cut); i.End(Op::Add); }
    if (s.movBetween) { Ins i(t); Dst(t, OT::Output, 0, 0xF); Swz(t, OT::Temp, 0, Enc::Swizzle(0, 1, 2, 3)); i.End(Op::Mov); }
    if (s.waterClip) {
        Ins i(t); Dst(t, OT::Temp, rT, 1u << cT); Sel1(t, OT::Input, 4, 1, 1); Enc::ImmScalar(t, 0.0f); i.End(Op::Lt);
    } else {
        Ins i(t); Dst(t, OT::Temp, rT, 1u << cT); Sel1(t, OT::Temp, rT, cT); Enc::ImmScalar(t, 0.0f); i.End(Op::Lt);
    }
    { Ins i(t); Sel1(t, OT::Temp, rT, cT); i.End(Op::Discard, s.discardZ ? 0u : kTestNonZero); }
    if (s.flow) { Ins i(t); Sel1(t, OT::Temp, 0, 0); i.End(31 /* if */, kTestNonZero); }
    // "lighting": a few movs / muls over r1 / r2 (the DETAIL variants differ in length)
    { Ins i(t); Dst(t, OT::Temp, 1, 7); Swz(t, OT::Temp, 0, Enc::Swizzle(0, 1, 2, 0)); i.End(Op::Mov); }
    for (int k = 0; k < s.filler; ++k) {
        Ins i(t); Dst(t, OT::Temp, 2, 7); Swz(t, OT::Temp, 1, Enc::Swizzle(0, 1, 2, 0));
        Enc::ImmScalar(t, 0.5f + 0.125f * static_cast<float>(k)); i.End(Op::Mul);
    }
    if (s.flow) { Ins i(t); i.End(21 /* endif */); }
    { Ins i(t); Dst(t, OT::Output, 0, 7); Swz(t, OT::Temp, 1, Enc::Swizzle(0, 1, 2, 0)); i.End(Op::Mov); }
    if (!s.noO0w) { Ins i(t); Dst(t, OT::Output, 0, 8); Enc::ImmScalar(t, 0.0f); i.End(Op::Mov); }
    { Ins i(t); i.End(Op::Ret); }
    return t;
}

// A container: a dummy RDEF-like chunk (our own bytes), the SHDR, a dummy OSGN-like chunk.
std::vector<uint8_t> Blob(const Shape& s, uint32_t version = 0x40) {
    Dxbc::Program p;
    p.version = version;
    p.tokens  = Program(s);
    Dxbc::Container c;
    Dxbc::Chunk a;
    a.fourcc = Dxbc::FourCC('R', 'D', 'E', 'F');
    a.data.assign(28, 0x11);
    Dxbc::Chunk sh;
    sh.fourcc = Dxbc::FourCC('S', 'H', 'D', 'R');
    sh.data   = Dxbc::EmitProgram(p);
    Dxbc::Chunk o;
    o.fourcc = Dxbc::FourCC('O', 'S', 'G', 'N');
    o.data.assign(32, 0x22);
    c.chunks = {a, sh, o};
    return Dxbc::Emit(c);
}

FaaRewrite::Match Analyse(const std::vector<uint8_t>& b) {
    FaaRewrite::Match m;
    FaaRewrite::Analyse(b.data(), b.size(), &m);
    return m;
}

int CountOp(const FaaRewrite::Match& m, uint32_t op) {
    int n = 0;
    for (const Dxbc::Instr& in : m.prog.instrs) n += in.opcode == op;
    return n;
}

// The one `mov o0.w, <src>` of a program, or null.
const Dxbc::Instr* MovO0w(const FaaRewrite::Match& m) {
    for (const Dxbc::Instr& in : m.prog.instrs)
        if (in.opcode == Op::Mov && in.operands.size() == 2 && in.operands[0].type == OT::Output &&
            in.operands[0].compBits == 8)
            return &in;
    return nullptr;
}

uint32_t TempsOf(const FaaRewrite::Match& m) {
    for (const Dxbc::Instr& in : m.prog.instrs)
        if (in.opcode == Op::DclTemps) return m.prog.tokens[in.offset + 1];
    return 0;
}

bool ChunkEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, uint32_t fourcc) {
    Dxbc::Container ca, cb;
    std::string err;
    if (!Dxbc::Parse(a.data(), a.size(), &ca, &err) || !Dxbc::Parse(b.data(), b.size(), &cb, &err)) return false;
    const Dxbc::Chunk* x = ca.Find(fourcc);
    const Dxbc::Chunk* y = cb.Find(fourcc);
    return x && y && x->data == y->data;
}

}  // namespace

// ---- the container + checksum ------------------------------------------------

UNIT_TEST(ext_faa_container_roundtrip_and_checksum) {
    // Both of the checksum's final-block layouts: (size - 20) % 64 below and at / above 56.
    int below = 0, above = 0;
    for (int filler = 0; filler < 24; ++filler) {
        Shape s;
        s.filler = filler;
        const std::vector<uint8_t> b = Blob(s);
        ((b.size() - 20) % 64 >= 56 ? above : below)++;
        const FaaRewrite::Match m = Analyse(b);
        CHECK(m.container && m.program);
        CHECK(m.identity);   // parse -> re-emit is byte-identical
        CHECK(m.checksum);   // our checksum verifies our own emit
        uint32_t sum[4];
        Dxbc::Checksum(b.data(), b.size(), sum);
        CHECK(std::memcmp(sum, b.data() + 4, 16) == 0);
        // a flipped bit anywhere past the checksum is caught
        std::vector<uint8_t> bad = b;
        bad[bad.size() - 5] ^= 0x10;
        Dxbc::Checksum(bad.data(), bad.size(), sum);
        CHECK(std::memcmp(sum, bad.data() + 4, 16) != 0);
        const FaaRewrite::Match mb = Analyse(bad);
        CHECK(!mb.checksum && !mb.colour);
    }
    CHECK(below > 0 && above > 0);
    // a frozen value: the checksum of one synthetic container (regression guard for the MD5 variant)
    {
        Shape s;
        const std::vector<uint8_t> b = Blob(s);
        uint32_t sum[4];
        Dxbc::Checksum(b.data(), b.size(), sum);
        // frozen 2026-10-16 (FAA-CORE); the algorithm itself was proven on 148/148 installed blobs (FAA-SPIKE S3)
        CHECK_EQ(b.size(), static_cast<size_t>(316));
        CHECK_EQ(sum[0], 0x1f38e537u);
        CHECK_EQ(sum[1], 0x048539b6u);
        CHECK_EQ(sum[2], 0x60a69380u);
        CHECK_EQ(sum[3], 0xb00741a1u);
    }
    // not a container
    std::vector<uint8_t> junk(64, 0x5a);
    const FaaRewrite::Match mj = Analyse(junk);
    CHECK(!mj.container && !mj.colour);
}

UNIT_TEST(ext_faa_checksum_covers_bytes_20_on) {
    // DXBC's checksum covers bytes [20, size): the magic and the checksum itself are outside it.
    // An 84-byte input = exactly one full 64-byte block, then the length block.
    std::vector<uint8_t> z(84, 0);
    uint32_t a[4], b[4];
    Dxbc::Checksum(z.data(), z.size(), a);
    z[20] = 1;
    Dxbc::Checksum(z.data(), z.size(), b);
    CHECK(std::memcmp(a, b, 16) != 0);
    z[20] = 0;
    z[0]  = 0xff;   // bytes 0..19 are not covered
    Dxbc::Checksum(z.data(), z.size(), b);
    CHECK(std::memcmp(a, b, 16) == 0);
}

// ---- the grammar ---------------------------------------------------------------

UNIT_TEST(ext_faa_grammar_matches_both_rt_variants_and_the_season_mad) {
    for (int g = 0; g < 4; ++g) {
        Shape s;
        s.seasonMad  = g == 0 || g == 2;
        s.separateRt = g >= 2;
        const FaaRewrite::Match m = Analyse(Blob(s));
        CHECK(m.colour);
        CHECK_STREQ(m.why.c_str(), "colour");
        CHECK_EQ(m.iMad >= 0, s.seasonMad);
        CHECK_EQ(m.cut, 0.5f);
        CHECK_EQ(m.alpha.index[0], 0u);
        CHECK_EQ(m.alphaComp, 3u);   // r0.w
        CHECK_EQ(m.temps, 3u);
        CHECK_EQ(m.iLt, m.iDiscard - 1);
        CHECK_EQ(m.iAdd, m.iDiscard - 2);
        CHECK(m.iSample >= 0 && m.iSample < m.iAdd);
    }
}

UNIT_TEST(ext_faa_grammar_refuses_the_near_misses) {
    struct Case { const char* name; Shape s; uint32_t version; };
    std::vector<Case> cases;
    { Shape s; s.noO0w = true;      cases.push_back({"depth: no mov o0.w", s, 0x40}); }
    { Shape s; s.discardZ = true;   cases.push_back({"discard_z", s, 0x40}); }
    { Shape s; s.flow = true;       cases.push_back({"flow control", s, 0x40}); }
    { Shape s; s.waterClip = true;  cases.push_back({"water clip", s, 0x40}); }
    { Shape s; s.movBetween = true; cases.push_back({"mov between add and lt", s, 0x40}); }
    { Shape s; s.cut = 1.5f;        cases.push_back({"cut outside (0,1)", s, 0x40}); }
    { Shape s;                      cases.push_back({"vertex program", s, 0x10040u}); }
    { Shape s;                      cases.push_back({"ps_5_0", s, 0x50}); }
    for (const Case& c : cases) {
        const std::vector<uint8_t> b = Blob(c.s, c.version);
        const std::vector<uint8_t> keep = b;
        const FaaRewrite::Match m = Analyse(b);
        if (m.colour) std::printf("  [faa] near miss '%s' matched\n", c.name);
        CHECK(!m.colour);
        CHECK(m.identity);   // readable, only not ours
        std::vector<uint8_t> out;
        for (int mode = 1; mode <= 2; ++mode)
            for (int msaa = 0; msaa <= 1; ++msaa)
                CHECK(!FaaRewrite::Transform(m, FaaRewrite::OptionsFor(mode, 0.25f, msaa != 0), &out));
        CHECK(b == keep);   // the blob handed to CreatePixelShader stays byte-identical
    }
}

// ---- the emit --------------------------------------------------------------------

UNIT_TEST(ext_faa_emit_coverage_all_detail_variants) {
    const uint32_t temps[4] = {3, 3, 4, 4};
    for (int g = 0; g < 2; ++g) {
        for (int v = 0; v < 4; ++v) {
            Shape s;
            s.separateRt = g == 1;
            s.seasonMad  = g == 0;
            s.temps      = temps[v];
            s.filler     = v * 3;
            const std::vector<uint8_t> in = Blob(s);
            const FaaRewrite::Match m = Analyse(in);
            CHECK(m.colour);
            std::vector<uint8_t> out;
            CHECK(FaaRewrite::Transform(m, FaaRewrite::OptionsFor(1, 0.25f, true), &out));
            const FaaRewrite::Match t = Analyse(out);
            CHECK(t.program && t.identity && t.checksum);
            CHECK_EQ(t.discards, 0);                   // dropped
            CHECK_EQ(TempsOf(t), s.temps + 1);         // P only
            CHECK_EQ(static_cast<int>(t.prog.instrs.size()),
                     static_cast<int>(m.prog.instrs.size()) - 1 + 1 + 7);   // - discard + save + sharpen
            const Dxbc::Instr* o = MovO0w(t);
            CHECK(o != nullptr && o->operands[1].type == OT::Temp && o->operands[1].index[0] == s.temps);
            // the alpha is saved to P.x right before the retail add, from rA.c (r0.w)
            const Dxbc::Instr& save = t.prog.instrs[static_cast<size_t>(m.iAdd)];
            CHECK_EQ(save.opcode, static_cast<uint32_t>(Op::Mov));
            CHECK(save.operands[0].index[0] == s.temps && save.operands[0].compBits == 1);
            CHECK(save.operands[1].index[0] == 0 && save.operands[1].Swz(0) == 3);
            // the sharpen ends with add_sat P.x, P.x, l(0.5)
            const Dxbc::Instr& last = t.prog.instrs[static_cast<size_t>(m.iDiscard) + 1 + 6];
            CHECK(last.opcode == Op::Add && last.Saturate() && last.operands[2].ImmF(0) == 0.5f);
            CHECK_EQ(CountOp(t, Op::DerivRtx), 1);
            CHECK_EQ(CountOp(t, Op::DerivRty), 1);
            CHECK_EQ(CountOp(t, Op::ResInfo), 0);      // no fill in mode 1
            // the other chunks are untouched
            CHECK(ChunkEqual(in, out, Dxbc::FourCC('R', 'D', 'E', 'F')));
            CHECK(ChunkEqual(in, out, Dxbc::FourCC('O', 'S', 'G', 'N')));
            // everything before the add is retail's, token for token (the season mad included)
            const uint32_t addAt = m.prog.instrs[static_cast<size_t>(m.iAdd)].offset;
            for (uint32_t k = 0; k < addAt; ++k)
                if (k != m.prog.instrs[static_cast<size_t>(m.iTemps)].offset + 1)   // dcl_temps' count
                    CHECK_EQ(t.prog.tokens[k], m.prog.tokens[k]);
        }
    }
}

UNIT_TEST(ext_faa_emit_fill_with_and_without_coverage) {
    Shape s;
    s.seasonMad = true;
    const FaaRewrite::Match m = Analyse(Blob(s));
    CHECK(m.colour);
    // mode 2, MSAA on: fill + coverage
    {
        std::vector<uint8_t> out;
        CHECK(FaaRewrite::Transform(m, FaaRewrite::OptionsFor(2, 0.25f, true), &out));
        const FaaRewrite::Match t = Analyse(out);
        CHECK(t.program && t.identity && t.checksum);
        CHECK_EQ(t.discards, 0);
        CHECK_EQ(TempsOf(t), s.temps + 3);
        CHECK_EQ(CountOp(t, Op::ResInfo), 1);
        CHECK_EQ(CountOp(t, Op::Log), 1);
        CHECK_EQ(CountOp(t, Op::DerivRtx), 2);     // the uv derivatives + fwidth
        // k is an immediate: the mad P.x, P.x, l(k), l(1)
        bool kFound = false;
        for (const Dxbc::Instr& in : t.prog.instrs)
            if (in.opcode == Op::Mad && in.operands.size() == 4 && in.operands[2].type == OT::Imm32 &&
                in.operands[2].ImmF(0) == 0.25f && in.operands[3].ImmF(0) == 1.0f)
                kFound = true;
        CHECK(kFound);
        // the season mad survives, before the fill reads the alpha
        CHECK_EQ(CountOp(t, Op::Mad), 2);
        const Dxbc::Instr* o = MovO0w(t);
        CHECK(o != nullptr && o->operands[1].type == OT::Temp);
        // a different k re-emits a different blob (the shaders are re-created on a k change)
        std::vector<uint8_t> out2;
        CHECK(FaaRewrite::Transform(m, FaaRewrite::OptionsFor(2, 0.5f, true), &out2));
        CHECK(out2.size() == out.size() && out2 != out);
    }
    // mode 2, MSAA off: the discard stays, o0.w stays 0, only the fill
    {
        std::vector<uint8_t> out;
        CHECK(FaaRewrite::Transform(m, FaaRewrite::OptionsFor(2, 0.25f, false), &out));
        const FaaRewrite::Match t = Analyse(out);
        CHECK(t.program && t.identity && t.checksum);
        CHECK_EQ(t.discards, 1);
        CHECK_EQ(t.o0wCount, 1);                   // still `mov o0.w, l(0)`
        CHECK_EQ(CountOp(t, Op::ResInfo), 1);
        CHECK_EQ(CountOp(t, Op::DerivRtx), 1);     // no fwidth
        // the retail add now reads the filled alpha (P.x), not r0.w
        const Dxbc::Instr& add = t.prog.instrs[static_cast<size_t>(t.iDiscard) - 2];
        CHECK_EQ(add.opcode, static_cast<uint32_t>(Op::Add));
        CHECK(add.operands[1].type == OT::Temp && add.operands[1].index[0] == s.temps);
    }
    // mode 1, MSAA off: nothing to emit (retail)
    {
        std::vector<uint8_t> out;
        CHECK(!FaaRewrite::Transform(m, FaaRewrite::OptionsFor(1, 0.25f, false), &out));
    }
    // OptionsFor
    CHECK(FaaRewrite::OptionsFor(1, 0.25f, true).coverage && FaaRewrite::OptionsFor(1, 0.25f, true).fillK == 0.0f);
    CHECK(FaaRewrite::OptionsFor(2, 0.25f, true).fillK == 0.25f);
    CHECK(!FaaRewrite::OptionsFor(2, 0.25f, false).coverage);
    CHECK(!FaaRewrite::OptionsFor(0, 0.25f, true).coverage);
}

// ---- the registry row and key -----------------------------------------------------

UNIT_TEST(ext_faa_registry_row_and_mode_key) {
    const Ext::Entry* e = Ext::FindEntry("foliage_aa");
    CHECK(e != nullptr);
    CHECK(e->tier == Ext::Tier::Extension && e->cls == Ext::Class::A && !e->defaultOn && e->formatVersion == 0);
    CHECK_STREQ(Ext::SettingsKey(*e).c_str(), "ext.foliage_aa");
    const Ext::ValueKey* k = Ext::FindValueKey("foliage_aa_mode");
    CHECK(k != nullptr);
    CHECK_STREQ(k->owner, "foliage_aa");
    CHECK_EQ(k->minValue, 1);
    CHECK_EQ(k->maxValue, 2);
    CHECK_EQ(k->defaultValue, 2);
    CHECK_STREQ(Ext::SettingsKey(*k).c_str(), "ext.foliage_aa_mode");
    bool ok = false;
    CHECK_EQ(Ext::ParseValue(*k, "1", ok), 1); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(*k, "2", ok), 2); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(*k, "0", ok), 1); CHECK(ok);    // clamped up
    CHECK_EQ(Ext::ParseValue(*k, "9", ok), 2); CHECK(ok);    // clamped down
    CHECK_EQ(Ext::ParseValue(*k, "smooth", ok), 2); CHECK(!ok);   // malformed -> the default
    const Ext::State boot;
    CHECK_EQ(boot.val.foliage_aa_mode, 2);
    CHECK(!boot.ext.foliage_aa);
}
