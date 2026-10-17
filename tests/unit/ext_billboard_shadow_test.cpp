// ============================================================================
// tests/unit/ext_billboard_shadow_test.cpp -- ext.billboard_shadow's pure parts
// ============================================================================
// src/render/dx11/billboard_shadow/receiver_rewrite.{h,cpp} (the margin lookup of handins/TS-CORE.md
// "TS-FIX2") and the registry row (src/ext/registry.cpp).
//
// NO RETAIL BYTES: every blob here is SYNTHETIC, assembled token by token in the SHAPE TS-CORE
// documented (a world position built from an instance, a camera-facing offset from cb0[4..6], the
// clip position from cb0[0..3], the shadow output from cb2[10..13]), not any shader's content. A tiny
// interpreter for the instructions used here runs the original and the rewritten program and checks
// the rewrite's MEANING: the clip output is unchanged, and the shadow position is the drawn position
// moved along the light direction to the card centre's depth minus k |C - P| (TS-FIX2).
// ============================================================================
#include "unit.h"

#include "billboard_shadow/edge_dither.h"   // BETTER-TREES: the pixel side
#include "billboard_shadow/receiver_rewrite.h"
#include "foliage_aa/dxbc.h"
#include "ext/registry.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

namespace Op  = Dxbc::Op;
namespace OT  = Dxbc::OperandType;
namespace Enc = Dxbc::Enc;
using Tokens  = std::vector<uint32_t>;

constexpr uint32_t kXYZW = Enc::Swizzle(0, 1, 2, 3);

uint32_t Comp4(uint32_t sel, uint32_t bits, uint32_t type, uint32_t dims) {
    return 2u | (sel << 2) | (bits << 4) | (type << 12) | (dims << 20);
}
void Dst(Tokens& t, uint32_t type, uint32_t reg, uint32_t mask) {
    t.push_back(Comp4(0, mask, type, 1));
    t.push_back(reg);
}
void Swz(Tokens& t, uint32_t type, uint32_t reg, uint32_t swz) {
    t.push_back(Comp4(1, swz, type, 1));
    t.push_back(reg);
}
void Cb(Tokens& t, uint32_t cb, uint32_t row, uint32_t swz = kXYZW) {
    t.push_back(Comp4(1, swz, OT::ConstantBuffer, 2));
    t.push_back(cb);
    t.push_back(row);
}
// cb<cb>[r<reg>.<comp> + row]: dim 1 is "imm32 + relative" (representation 3)
void CbRel(Tokens& t, uint32_t cb, uint32_t row, uint32_t reg, uint32_t comp, uint32_t swz = kXYZW) {
    t.push_back(Comp4(1, swz, OT::ConstantBuffer, 2) | (3u << 25));
    t.push_back(cb);
    t.push_back(row);
    t.push_back(Comp4(2, comp, OT::Temp, 1));
    t.push_back(reg);
}

struct Ins {
    Tokens& t;
    size_t  at;
    explicit Ins(Tokens& tt) : t(tt), at(tt.size()) { t.push_back(0); }
    void End(uint32_t op) { t[at] = Enc::Opcode(op, static_cast<uint32_t>(t.size() - at)); }
};

struct Shape {
    uint32_t cb2Rows   = 14;
    bool     shadowFromCamera = true;    // the shadow output reads the camera-facing position
    bool     noShadow  = false;          // no cb2[10..13] output at all
    bool     flow      = false;          // an if / endif
    bool     relCb0    = false;          // the offset reads cb0[r0.w + 4] (relative)
    bool     relCb3    = false;          // an instance read cb3[r3.w + 0] after the first taint (renamed sub-operand)
    int      filler    = 0;              // extra unrelated instructions (lighting-like)
};

// v0 = position, cb0 rows 0..3 worldToProjection, 4..7 worldToCamera, 8 cameraPosition;
// cb2 row 3 nearPlane (the light direction), 10..13 shadowProjection[0]. o0 clip, o1 a texcoord, o3 shadow.
Tokens Program(const Shape& s) {
    Tokens t;
    { Ins i(t); t.push_back(Comp4(1, kXYZW, OT::ConstantBuffer, 2)); t.push_back(0); t.push_back(12);
      i.End(Op::DclConstantBuffer); }
    { Ins i(t); t.push_back(Comp4(1, kXYZW, OT::ConstantBuffer, 2)); t.push_back(2); t.push_back(s.cb2Rows);
      i.End(Op::DclConstantBuffer); }
    { Ins i(t); t.push_back(6); i.End(Op::DclTemps); }
    // 0: mov r0.xyz, v0.xyz                    the card centre (shared)
    { Ins i(t); Dst(t, OT::Temp, 0, 7); Swz(t, OT::Input, 0, kXYZW); i.End(Op::Mov); }
    // 1: mov r3.xyz, v1.xyz                    the world normal N
    { Ins i(t); Dst(t, OT::Temp, 3, 7); Swz(t, OT::Input, 1, kXYZW); i.End(Op::Mov); }
    // 2: mov r5.w, l(0)                        an index register (for the relative variants)
    { Ins i(t); Dst(t, OT::Temp, 5, 8); Enc::ImmScalar(t, 0.0f); i.End(Op::Mov); }
    // 3: mul r1.xyz, r3.yyyy, cb0[5].xyzx      camera-space normal (tainted from here)
    { Ins i(t); Dst(t, OT::Temp, 1, 7); Swz(t, OT::Temp, 3, Enc::Rep(1));
      if (s.relCb0) CbRel(t, 0, 5, 5, 3); else Cb(t, 0, 5); i.End(Op::Mul); }
    // 4: mad r1.xyz, cb0[4].xyzx, r3.xxxx, r1.xyzx
    { Ins i(t); Dst(t, OT::Temp, 1, 7); Cb(t, 0, 4); Swz(t, OT::Temp, 3, Enc::Rep(0)); Swz(t, OT::Temp, 1, kXYZW);
      i.End(Op::Mad); }
    if (s.flow) { Ins i(t); t.push_back(Comp4(2, 0, OT::Temp, 1)); t.push_back(0); i.End(31 /* if */); }
    // 5: mad r1.xyz, r1.xyzx, l(0.5, 0.5, 0.5, 0), cb0[6].xyzx   the card code's 0.5 tilt shape
    { Ins i(t); Dst(t, OT::Temp, 1, 7); Swz(t, OT::Temp, 1, kXYZW); Enc::Imm4(t, 0.5f, 0.5f, 0.5f, 0.0f); Cb(t, 0, 6);
      i.End(Op::Mad); }
    // 6: mul r2.xyz, r1.xyzx, cb0[6].wwww      a scale, so the camera row's w takes part
    { Ins i(t); Dst(t, OT::Temp, 2, 7); Swz(t, OT::Temp, 1, kXYZW); Cb(t, 0, 6, Enc::Rep(3)); i.End(Op::Mul); }
    if (s.flow) { Ins i(t); i.End(21 /* endif */); }
    if (s.relCb3) {
        // 6b: add r2.xyz, r2.xyzx, cb3[r5.w + 0].xyzx  (an untainted relative instance read inside the slice)
        Ins i(t); Dst(t, OT::Temp, 2, 7); Swz(t, OT::Temp, 2, kXYZW); CbRel(t, 3, 0, 5, 3); i.End(Op::Add);
    }
    // 7: add r4.xyz, r0.xyzx, r2.xyzx          the billboarded position P
    { Ins i(t); Dst(t, OT::Temp, 4, 7); Swz(t, OT::Temp, 0, kXYZW); Swz(t, OT::Temp, 2, kXYZW); i.End(Op::Add); }
    // clip: o0 = worldToProjection * (r4, 1)
    { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Swz(t, OT::Temp, 4, Enc::Rep(1)); Cb(t, 0, 1); i.End(Op::Mul); }
    { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Cb(t, 0, 0); Swz(t, OT::Temp, 4, Enc::Rep(0)); Swz(t, OT::Temp, 2, kXYZW);
      i.End(Op::Mad); }
    { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Cb(t, 0, 2); Swz(t, OT::Temp, 4, Enc::Rep(2)); Swz(t, OT::Temp, 2, kXYZW);
      i.End(Op::Mad); }
    { Ins i(t); Dst(t, OT::Output, 0, 0xF); Swz(t, OT::Temp, 2, kXYZW); Cb(t, 0, 3); i.End(Op::Add); }
    // filler: o1 = r4 * 0.5 (+ more), reading the billboarded position
    for (int k = 0; k <= s.filler; ++k) {
        Ins i(t); Dst(t, OT::Temp, 5, 7); Swz(t, OT::Temp, 4, kXYZW); Enc::ImmScalar(t, 0.5f + 0.25f * k); i.End(Op::Mul);
    }
    { Ins i(t); Dst(t, OT::Output, 1, 0xF); Swz(t, OT::Temp, 5, Enc::Swizzle(0, 1, 2, 2)); i.End(Op::Mov); }
    if (!s.noShadow) {
        const uint32_t src = s.shadowFromCamera ? 4 : 1;
        if (!s.shadowFromCamera) {
            // the shadow reads the UNbillboarded position: r1 = v0 (no camera dependence)
            Ins i(t); Dst(t, OT::Temp, 1, 7); Swz(t, OT::Input, 0, kXYZW); i.End(Op::Mov);
        }
        { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Swz(t, OT::Temp, src, Enc::Rep(1)); Cb(t, 2, 11); i.End(Op::Mul); }
        { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Cb(t, 2, 10); Swz(t, OT::Temp, src, Enc::Rep(0)); Swz(t, OT::Temp, 2, kXYZW);
          i.End(Op::Mad); }
        { Ins i(t); Dst(t, OT::Temp, 2, 0xF); Cb(t, 2, 12); Swz(t, OT::Temp, src, Enc::Rep(2)); Swz(t, OT::Temp, 2, kXYZW);
          i.End(Op::Mad); }
        { Ins i(t); Dst(t, OT::Output, 3, 0xF); Swz(t, OT::Temp, 2, kXYZW); Cb(t, 2, 13); i.End(Op::Add); }
    }
    { Ins i(t); i.End(Op::Ret); }
    return t;
}

std::vector<uint8_t> Blob(const Shape& s, uint32_t version = 0x10040) {   // vs_4_0
    Dxbc::Program p;
    p.version = version;
    p.tokens  = Program(s);
    Dxbc::Container c;
    Dxbc::Chunk a;
    a.fourcc = Dxbc::FourCC('I', 'S', 'G', 'N');
    a.data.assign(24, 0x33);
    Dxbc::Chunk sh;
    sh.fourcc = Dxbc::FourCC('S', 'H', 'D', 'R');
    sh.data   = Dxbc::EmitProgram(p);
    c.chunks = {a, sh};
    return Dxbc::Emit(c);
}

BbsRewrite::Match Analyse(const std::vector<uint8_t>& b) {
    BbsRewrite::Match m;
    BbsRewrite::Analyse(b.data(), b.size(), &m);
    return m;
}

// ---- a tiny SM4 interpreter: mov / add / mul / mad / div / dp3 over temps, inputs, cbuffers, immediates ----
struct Vm {
    float r[16][4] = {};
    float v[4][4]  = {};
    float o[8][4]  = {};
    float cb[4][16][4] = {};
};

bool Fetch(const Vm& vm, const Dxbc::Program& p, const Dxbc::Operand& o, float out[4]) {
    float raw[4];
    switch (o.type) {
    case OT::Temp: std::memcpy(raw, vm.r[o.index[0]], sizeof raw); break;
    case OT::Input: std::memcpy(raw, vm.v[o.index[0]], sizeof raw); break;
    case OT::ConstantBuffer: {
        uint32_t row = o.index[1];
        if (o.relative) {
            // the sub-operand follows the two index dwords: token, reg (select-1)
            const uint32_t* t = p.tokens.data() + o.offset;
            const uint32_t sub = t[3], reg = t[4];
            row += static_cast<uint32_t>(vm.r[reg][(sub >> 4) & 3]);
        }
        std::memcpy(raw, vm.cb[o.index[0]][row], sizeof raw);
        break;
    }
    case OT::Imm32:
        for (int c = 0; c < 4; ++c) std::memcpy(&raw[c], &o.imm[o.numComp == 1 ? 0 : c], 4);
        break;
    case OT::Resource: case OT::Sampler: case 13 /* null */:
        for (int c = 0; c < 4; ++c) raw[c] = 0.0f;   // the pixel stand-ins never read these
        break;
    default: return false;
    }
    for (int c = 0; c < 4; ++c) out[c] = raw[o.numComp == 1 ? 0 : o.Swz(c)];
    if (o.modifier & 1) for (int c = 0; c < 4; ++c) out[c] = -out[c];
    return true;
}

bool Run(Vm& vm, const std::vector<uint8_t>& blob) {
    Dxbc::Container c;
    std::string err;
    if (!Dxbc::Parse(blob.data(), blob.size(), &c, &err)) return false;
    Dxbc::Program p;
    if (!Dxbc::ParseProgram(*c.Find(Dxbc::FourCC('S', 'H', 'D', 'R')), &p, &err)) return false;
    for (const Dxbc::Instr& in : p.instrs) {
        if (in.isDecl) continue;
        if (in.opcode == Op::Ret) break;
        float a[4], b[4], d[4], res[4];
        const size_t ns = in.operands.size() - 1;
        if (ns >= 1 && !Fetch(vm, p, in.operands[1], a)) return false;
        if (ns >= 2 && !Fetch(vm, p, in.operands[2], b)) return false;
        if (ns >= 3 && !Fetch(vm, p, in.operands[3], d)) return false;
        if (in.opcode == Op::SampleCLz && ns == 4 && !Fetch(vm, p, in.operands[4], d)) return false;   // the compare value
        for (int k = 0; k < 4; ++k) {
            switch (in.opcode) {
            case Op::Mov: res[k] = a[k]; break;
            case Op::Add: res[k] = a[k] + b[k]; break;
            case Op::Mul: res[k] = a[k] * b[k]; break;
            case Op::Mad: res[k] = a[k] * b[k] + d[k]; break;
            case Op::Div: res[k] = a[k] / b[k]; break;
            case Op::Max: res[k] = a[k] > b[k] ? a[k] : b[k]; break;
            case Op::Dp2: res[k] = a[0] * b[0] + a[1] * b[1]; break;
            case 16 /* dp3 */: res[k] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; break;
            case 75 /* sqrt */: res[k] = std::sqrt(a[k]); break;
            case 26 /* frc */: res[k] = a[k] - std::floor(a[k]); break;
            case 65 /* round_ni */: res[k] = std::floor(a[k]); break;
            case 77 /* sincos: dst0 = sin(src), the second (null) destination unwritten */: res[k] = std::sin(b[k]); break;
            // the pixel stand-ins (ext_bbs_dither_*): `sample` yields its coordinates, `sample_c_lz` its
            // compare value, so the outputs expose what the dither fed them
            case Op::Sample: res[k] = a[k]; break;
            case Op::SampleCLz: res[k] = d[k]; break;
            default: return false;
            }
        }
        const Dxbc::Operand& o = in.operands[0];
        float* dst = o.type == OT::Temp ? vm.r[o.index[0]] : (o.type == OT::Output ? vm.o[o.index[0]] : nullptr);
        if (dst == nullptr) return false;
        for (int k = 0; k < 4; ++k)
            if (o.compBits & (1u << k)) dst[k] = res[k];
    }
    return true;
}

void Seed(Vm& vm, uint32_t salt) {
    uint32_t x = 0x9e3779b9u ^ salt;
    auto next = [&]() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return static_cast<float>(x % 2000) / 1000.0f - 1.0f; };
    for (auto& row : vm.cb) for (auto& reg : row) for (float& f : reg) f = next();
    for (float& f : vm.v[0]) f = next();
    for (float& f : vm.v[1]) f = next();
    // cb2[3].xyz: the light direction, unit length (SunLight::DrawLight hands Context::SetLight a
    // normalised direction).
    float* L = vm.cb[2][3];
    const float len = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
    if (len < 0.1f) { L[0] = 0.0f; L[1] = -1.0f; L[2] = 0.0f; }
    else for (int k = 0; k < 3; ++k) L[k] /= len;
    for (int r = 0; r < 4; ++r) vm.cb[3][r][0] = vm.cb[3][r][0];   // cb3: instance rows (any)
}

bool Near(const float a[4], const float b[4], float tol = 1e-4f) {
    for (int c = 0; c < 4; ++c)
        if (std::fabs(a[c] - b[c]) > tol) return false;
    return true;
}

void IdentityShadow(Vm& vm) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) vm.cb[2][10 + r][c] = r == c ? 1.0f : 0.0f;
}

// The rewrite means: o0 / o1 as the original, and with an identity shadow matrix the shadow output is
//   P' = P + (basis dot(C - P, L) - (margin + jitter (2 frc(C.K) - 1)) |C - P|) L
// where P is the original's shadow position, C = v0 (the card centre) and L = cb2[3].xyz.
void CheckMeaning(const std::vector<uint8_t>& orig, const std::vector<uint8_t>& xf) {
    for (uint32_t salt = 1; salt <= 6; ++salt) {
        Vm a, b;
        Seed(a, salt);
        b = a;
        CHECK(Run(a, orig));
        CHECK(Run(b, xf));
        CHECK(Near(b.o[0], a.o[0]));
        CHECK(Near(b.o[1], a.o[1]));

        Vm pv, xv;
        Seed(pv, salt);
        IdentityShadow(pv);
        xv = pv;
        CHECK(Run(pv, orig));
        CHECK(Run(xv, xf));
        float C[3], P[3], L[3], d[3];
        for (int k = 0; k < 3; ++k) {
            C[k] = pv.v[0][k]; P[k] = pv.o[3][k]; L[k] = pv.cb[2][3][k]; d[k] = C[k] - P[k];
        }
        auto dot = [](const float* u, const float* v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
        // the hash, in the interpreter's own float order (dp3 = x*x + y*y + z*z, then frc)
        const float hd = C[0] * BbsRewrite::kHashX + C[1] * BbsRewrite::kHashY + C[2] * BbsRewrite::kHashZ;
        const float h = hd - std::floor(hd);
        const BbsRewrite::Params k = BbsRewrite::kDefaultParams;
        const float e = h * (2.0f * k.jitter) + (k.margin - k.jitter);
        const float r = std::sqrt(dot(d, d));
        const float s = dot(d, L) * k.basis - e * r;
        // BETTER-TREES: o3.w = -(r * |cb2[12].xyz|) = -r under the identity shadow matrix (the extent)
        float want[4] = {P[0] + s * L[0], P[1] + s * L[1], P[2] + s * L[2], -r};
        CHECK(Near(xv.o[3], want, 2e-3f));
        if (std::fabs(s) > 1e-3f) CHECK(!Near(xv.o[3], pv.o[3]));   // the lookup really moved (in depth)
        // its depth is P's moved by s along L ...
        float e3[3] = {xv.o[3][0] - P[0], xv.o[3][1] - P[1], xv.o[3][2] - P[2]};
        CHECK(std::fabs(dot(e3, L) - s) < 2e-3f);
        // ... and across L it is P's (the same texel)
        float dx[3] = {xv.o[3][0] - P[0], xv.o[3][1] - P[1], xv.o[3][2] - P[2]};
        float across[3];
        const float along = dot(dx, L);
        for (int k = 0; k < 3; ++k) across[k] = dx[k] - along * L[k];
        CHECK(std::sqrt(dot(across, across)) < 1e-4f);
    }
}

}  // namespace

UNIT_TEST(ext_bbs_match_and_meaning) {
    for (int filler = 0; filler < 3; ++filler) {
        for (int rel = 0; rel < 2; ++rel) {
            Shape s;
            s.filler = filler;
            s.relCb3 = rel != 0;
            const std::vector<uint8_t> b = Blob(s);
            const BbsRewrite::Match m = Analyse(b);
            CHECK(m.container && m.program && m.vertex && m.candidate);
            CHECK(m.matched);
            CHECK_STREQ(m.why.c_str(), "");
            CHECK_EQ(m.temps, 6u);
            CHECK_EQ(m.shadowOut, 3u);
            CHECK_EQ(m.posReg, 4u);
            CHECK_EQ(m.centerOperand, 1);   // add r4, r0 (the centre), r2 (the camera-facing offset)
            std::vector<uint8_t> out;
            CHECK(BbsRewrite::Transform(m, &out));
            // the copy is a valid container and is not rewritten twice
            const BbsRewrite::Match m2 = Analyse(out);
            CHECK(m2.container && m2.program);
            CHECK(!m2.matched);
            uint32_t temps2 = 0;
            for (const Dxbc::Instr& in : m2.prog.instrs)
                if (in.opcode == Op::DclTemps) temps2 = m2.prog.tokens[in.offset + 1];
            CHECK_EQ(temps2, 9u);   // T + 3
            CheckMeaning(b, out);
        }
    }
}

// ---- the pixel side: edge_dither (BETTER-TREES) ------------------------------------------------

namespace {

constexpr uint32_t kDclInputPs = 98, kSampleCLz = 71;

// A synthetic ps_4_0 in the SHAPE of a billboard colour program: a diffuse sample from v1.xy, a
// shadow compare from v3.z (two taps, one through a temp offset), the compare value written out.
// `shape` 0 = that; 1 = no sample_c (a depth variant); 2 = the compare value comes through a temp
// only; 3 = the texcoord sample reads a temp (no input uv); 4 = flow control.
Tokens DitherProgram(int shape) {
    Tokens t;
    auto dclInput = [&](uint32_t reg, uint32_t mask) {
        Ins i(t);
        t.push_back(Comp4(0, mask, OT::Input, 1));
        t.push_back(reg);
        t[i.at] = Enc::Opcode(kDclInputPs, static_cast<uint32_t>(t.size() - i.at)) | (2u << 11);   // linear
    };
    dclInput(1, 0xF);
    dclInput(3, 0x7);
    { Ins i(t); t.push_back(3); i.End(Op::DclTemps); }
    if (shape == 4) { Ins i(t); t.push_back(Comp4(2, 0, OT::Temp, 1)); t.push_back(0); i.End(31 /* if */); }
    // 0: sample r0.xyzw, v1.xyxx, t0, s0      (3: from r2 instead of v1)
    { Ins i(t); Dst(t, OT::Temp, 0, 0xF);
      if (shape == 3) Swz(t, OT::Temp, 2, Enc::Swizzle(0, 1, 0, 0)); else Swz(t, OT::Input, 1, Enc::Swizzle(0, 1, 0, 0));
      Swz(t, OT::Resource, 0, kXYZW); t.push_back(Comp4(0, 0, OT::Sampler, 1) & ~3u); t.push_back(0); i.End(Op::Sample); }
    if (shape == 4) { Ins i(t); i.End(21 /* endif */); }
    if (shape != 1) {
        // 1: mov r1.xyz, v3.xyzx             2: sample_c_lz r1.w, r1.xyxx, t5, s5, r1.z   (a tap through a temp)
        { Ins i(t); Dst(t, OT::Temp, 1, 7); Swz(t, OT::Input, 3, Enc::Swizzle(0, 1, 2, 0)); i.End(Op::Mov); }
        { Ins i(t); Dst(t, OT::Temp, 1, 8); Swz(t, OT::Temp, 1, Enc::Swizzle(0, 1, 0, 0)); Swz(t, OT::Resource, 5, Enc::Rep(0));
          t.push_back(Comp4(0, 0, OT::Sampler, 1) & ~3u); t.push_back(5);
          t.push_back(Comp4(2, 2, OT::Temp, 1)); t.push_back(1); i.End(kSampleCLz); }
        if (shape != 2) {
            // 3: sample_c_lz r0.w, v3.xyxx, t5, s5, v3.z   (the direct tap)
            Ins i(t); Dst(t, OT::Temp, 0, 8); Swz(t, OT::Input, 3, Enc::Swizzle(0, 1, 0, 0)); Swz(t, OT::Resource, 5, Enc::Rep(0));
            t.push_back(Comp4(0, 0, OT::Sampler, 1) & ~3u); t.push_back(5);
            t.push_back(Comp4(2, 2, OT::Input, 1)); t.push_back(3); i.End(kSampleCLz);
        } else {
            Ins i(t); Dst(t, OT::Temp, 0, 8); Swz(t, OT::Temp, 1, Enc::Rep(3)); i.End(Op::Mov);
        }
        // 4: mov o0.xyzw, r0.wwww   (the direct tap's compare value)   5: mov o1.xyzw, r1.wwww (the temp tap's)
        { Ins i(t); Dst(t, OT::Output, 0, 0xF); Swz(t, OT::Temp, 0, Enc::Rep(3)); i.End(Op::Mov); }
        { Ins i(t); Dst(t, OT::Output, 1, 0xF); Swz(t, OT::Temp, 1, Enc::Rep(3)); i.End(Op::Mov); }
    } else {
        Ins i(t); Dst(t, OT::Output, 0, 0xF); Swz(t, OT::Temp, 0, kXYZW); i.End(Op::Mov);
    }
    { Ins i(t); i.End(Op::Ret); }
    return t;
}

// ISGN: two TEXCOORD elements (v1 xyzw read xyzw, v3 xyzw read xyz), the real layout.
std::vector<uint8_t> Isgn() {
    std::vector<uint8_t> d;
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) d.push_back(static_cast<uint8_t>(v >> (8 * i))); };
    w32(2); w32(8);
    const uint32_t name = 8 + 2 * 24;
    w32(name); w32(1); w32(0); w32(3); w32(1); d.push_back(0xF); d.push_back(0xF); d.push_back(0); d.push_back(0);
    w32(name); w32(3); w32(0); w32(3); w32(3); d.push_back(0xF); d.push_back(0x7); d.push_back(0); d.push_back(0);
    for (char c : std::string("TEXCOORD")) d.push_back(static_cast<uint8_t>(c));
    d.push_back(0);
    while (d.size() % 4) d.push_back(0);
    return d;
}

std::vector<uint8_t> DitherBlob(int shape, bool realIsgn = true) {
    Dxbc::Program p;
    p.version = 0x40;   // ps_4_0
    p.tokens  = DitherProgram(shape);
    Dxbc::Container c;
    Dxbc::Chunk a;
    a.fourcc = Dxbc::FourCC('I', 'S', 'G', 'N');
    if (realIsgn) a.data = Isgn(); else a.data.assign(24, 0x33);
    Dxbc::Chunk sh;
    sh.fourcc = Dxbc::FourCC('S', 'H', 'D', 'R');
    sh.data   = Dxbc::EmitProgram(p);
    c.chunks = {a, sh};
    return Dxbc::Emit(c);
}

uint8_t IsgnReadMask(const std::vector<uint8_t>& blob, uint32_t reg) {
    Dxbc::Container c;
    std::string err;
    if (!Dxbc::Parse(blob.data(), blob.size(), &c, &err)) return 0xEE;
    const Dxbc::Chunk* isgn = c.Find(Dxbc::FourCC('I', 'S', 'G', 'N'));
    if (isgn == nullptr) return 0xEE;
    uint32_t count;
    std::memcpy(&count, isgn->data.data(), 4);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = isgn->data.data() + 8 + 24 * i;
        uint32_t r;
        std::memcpy(&r, e + 16, 4);
        if (r == reg) return e[21];
    }
    return 0xEE;
}

}  // namespace

UNIT_TEST(ext_bbs_dither_match_and_meaning) {
    const std::vector<uint8_t> b = DitherBlob(0);
    BbsDither::Match m;
    BbsDither::Analyse(b.data(), b.size(), &m);
    CHECK(m.container && m.program && m.pixel && m.candidate);
    CHECK(m.matched);
    CHECK_STREQ(m.why.c_str(), "");
    CHECK_EQ(m.temps, 3u);
    CHECK_EQ(m.shadowIn, 3u);
    CHECK_EQ(m.shadowComp, 2u);
    CHECK_EQ(m.uvIn, 1u);
    BbsRewrite::Params k = BbsRewrite::kDefaultParams;
    k.noise = 0.6f;
    k.noiseScale = 32.0f;
    std::vector<uint8_t> out;
    CHECK(BbsDither::Transform(m, &out, k));
    // the copy: a valid container, v3 declared and signed xyzw, temps + 2, not rewritten twice
    BbsDither::Match m2;
    BbsDither::Analyse(out.data(), out.size(), &m2);
    CHECK(m2.container && m2.program && m2.candidate);
    CHECK(!m2.matched);
    CHECK_EQ(IsgnReadMask(out, 3), 0xF);
    CHECK_EQ(IsgnReadMask(out, 1), 0xF);
    uint32_t temps2 = 0, v3mask = 0;
    for (const Dxbc::Instr& in : m2.prog.instrs) {
        if (in.opcode == Op::DclTemps) temps2 = m2.prog.tokens[in.offset + 1];
        if (in.opcode == kDclInputPs && m2.prog.tokens[in.offset + 2] == 3) v3mask = (m2.prog.tokens[in.offset + 1] >> 4) & 0xF;
    }
    CHECK_EQ(temps2, 5u);
    CHECK_EQ(v3mask, 0xFu);
    // meaning: both taps see z + (h - 0.5) * noise * max(-w, 0); w = +1 (a retail VS) leaves z alone
    for (uint32_t salt = 1; salt <= 6; ++salt) {
        Vm a, c;
        Seed(a, salt);
        a.v[3][0] = 0.25f * salt; a.v[3][1] = 0.5f - 0.1f * salt; a.v[3][2] = 0.3f + 0.1f * salt;   // the shadow interpolant
        a.v[3][3] = salt % 2 ? -0.37f : 1.0f;   // the extent (negative) or retail's w
        c = a;
        CHECK(Run(a, b));
        CHECK(Run(c, out));
        const float u = std::floor(a.v[1][0] * k.noiseScale), v = std::floor(a.v[1][1] * k.noiseScale);
        const float hd = std::sin(u * BbsDither::kHashU + v * BbsDither::kHashV) * BbsDither::kHashMul;
        const float h = hd - std::floor(hd);
        const float amp = a.v[3][3] < 0.0f ? -a.v[3][3] : 0.0f;
        const float want = a.v[3][2] + (h - 0.5f) * amp * k.noise;
        CHECK(std::fabs(c.o[0][0] - want) < 2e-3f);
        CHECK(std::fabs(c.o[1][0] - want) < 2e-3f);   // the tap through the temp copy sees the same z'
        CHECK(std::fabs(a.o[0][0] - a.v[3][2]) < 1e-6f);
        if (amp == 0.0f) CHECK(std::fabs(c.o[0][0] - a.v[3][2]) < 1e-6f);
        else CHECK(std::fabs(c.o[0][0] - a.v[3][2]) > 1e-4f || std::fabs(h - 0.5f) < 1e-3f);
    }
}

UNIT_TEST(ext_bbs_dither_refusals) {
    { BbsDither::Match m; const std::vector<uint8_t> b = DitherBlob(1); BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(!m.matched && !m.candidate); }                      // no sample_c at all (a depth variant)
    { BbsDither::Match m; const std::vector<uint8_t> b = DitherBlob(2); BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(m.candidate && !m.matched); }                       // no direct input compare
    { BbsDither::Match m; const std::vector<uint8_t> b = DitherBlob(3); BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(m.candidate && !m.matched); }                       // no input texcoord
    { BbsDither::Match m; const std::vector<uint8_t> b = DitherBlob(4); BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(!m.matched); }                                      // flow control
    { BbsDither::Match m; const std::vector<uint8_t> b = DitherBlob(0, false); BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(m.matched);                                         // the match does not read the ISGN ...
      std::vector<uint8_t> out;
      CHECK(!BbsDither::Transform(m, &out, BbsRewrite::kDefaultParams)); }   // ... the transform refuses a bad one
    { Shape s; const std::vector<uint8_t> b = Blob(s);          // the vertex program is not a pixel shader
      BbsDither::Match m; BbsDither::Analyse(b.data(), b.size(), &m);
      CHECK(!m.matched && !m.pixel); }
}

UNIT_TEST(ext_bbs_refusals) {
    {
        Shape s; s.noShadow = true;
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched && !m.candidate);
    }
    {
        Shape s; s.shadowFromCamera = false;   // the shadow does not depend on the camera rows
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(m.candidate && !m.matched);
    }
    {
        Shape s; s.cb2Rows = 8;   // shadowProjection[0] not declared
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched);
    }
    {
        Shape s; s.flow = true;
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched);
    }
    {
        Shape s; s.relCb0 = true;   // a relative gc read: which rows it reads is unknown
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched);
    }
    {
        Shape s;
        const BbsRewrite::Match m = Analyse(Blob(s, 0x40));   // a pixel shader with the same body
        CHECK(!m.matched && !m.vertex);
    }
    {
        Shape s;
        std::vector<uint8_t> b = Blob(s);
        b[b.size() - 9] ^= 0x01;   // checksum no longer valid: the round trip refuses it
        const BbsRewrite::Match m = Analyse(b);
        CHECK(!m.matched);
    }
}

UNIT_TEST(ext_bbs_registry_row) {
    const Ext::Entry* e = Ext::FindEntry("better_trees");
    CHECK(e != nullptr);
    if (e == nullptr) return;
    CHECK(e->tier == Ext::Tier::Extension);
    CHECK(e->cls == Ext::Class::A);
    CHECK(!e->defaultOn);
    CHECK_STREQ(Ext::SettingsKey(*e).c_str(), "ext.better_trees");
    CHECK_STREQ(e->displayName, "Better trees");
}
