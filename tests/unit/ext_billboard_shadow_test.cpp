// ============================================================================
// tests/unit/ext_billboard_shadow_test.cpp -- ext.billboard_shadow's pure parts
// ============================================================================
// src/render/dx11/billboard_shadow/receiver_rewrite.{h,cpp} (the shadow-output slice clone of
// docs/extensions/TREE_SHADOW.md §4.2) and the registry row (src/ext/registry.cpp).
//
// NO RETAIL BYTES: every blob here is SYNTHETIC, assembled token by token in the SHAPE TS-CORE
// documented (a world position built from an instance, a camera-facing offset from cb0[4..6], the
// clip position from cb0[0..3], the shadow output from cb2[10..13]), not any shader's content. A tiny
// interpreter for the instructions used here runs the original and the rewritten program and checks
// the rewrite's MEANING: the clip output is unchanged, and the shadow position is the drawn position
// moved along the light direction onto the sun-facing twin's plane (TS-FIX).
// ============================================================================
#include "unit.h"

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
// cb2 rows 6..9 textureProjection, 10..13 shadowProjection[0]. o0 clip, o1 a texcoord, o3 shadow.
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
        for (int k = 0; k < 4; ++k) {
            switch (in.opcode) {
            case Op::Mov: res[k] = a[k]; break;
            case Op::Add: res[k] = a[k] + b[k]; break;
            case Op::Mul: res[k] = a[k] * b[k]; break;
            case Op::Mad: res[k] = a[k] * b[k] + d[k]; break;
            case Op::Div: res[k] = a[k] / b[k]; break;
            case 16 /* dp3 */: res[k] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; break;
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
    // cb2[6..8]: an orthonormal sun basis, R / f / U in the .x / .y / .z columns (the card code's
    // reading), from three random angles; cb2[9] a random translation.
    const float a = next() * 3.0f, b = next() * 1.5f, c = next() * 3.0f;
    const float ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b), cc = std::cos(c), sc = std::sin(c);
    const float Rz[3][3] = {{ca, -sa, 0}, {sa, ca, 0}, {0, 0, 1}};
    const float Rx[3][3] = {{1, 0, 0}, {0, cb, -sb}, {0, sb, cb}};
    const float Ry[3][3] = {{cc, 0, sc}, {0, 1, 0}, {-sc, 0, cc}};
    float M[3][3] = {}, Q[3][3] = {};
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) for (int k = 0; k < 3; ++k) M[i][j] += Rz[i][k] * Rx[k][j];
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) for (int k = 0; k < 3; ++k) Q[i][j] += M[i][k] * Ry[k][j];
    for (int k = 0; k < 3; ++k) {
        for (int col = 0; col < 3; ++col) vm.cb[2][6 + k][col] = Q[k][col];   // column col = basis vector col
        vm.cb[2][6 + k][3] = 0.0f;
    }
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
//   P' = P - f * ((P - T) . n) / (f . n),   n = 0.5 (N . R) R + 0.5 (N . U) U - f
// where P is the original's shadow position, T the original's with cb0[4..7] := cb2[6..9] (the
// sun-facing twin), N = v1 and R / f / U the .x / .y / .z columns of cb2[6..8].
void CheckMeaning(const Shape& s, const std::vector<uint8_t>& orig, const std::vector<uint8_t>& xf) {
    for (uint32_t salt = 1; salt <= 6; ++salt) {
        Vm a, b;
        Seed(a, salt);
        b = a;
        CHECK(Run(a, orig));
        CHECK(Run(b, xf));
        CHECK(Near(b.o[0], a.o[0]));
        CHECK(Near(b.o[1], a.o[1]));

        Vm pv, tv, xv;
        Seed(pv, salt);
        IdentityShadow(pv);
        tv = pv;
        xv = pv;
        for (int r = 0; r < 4; ++r) std::memcpy(tv.cb[0][4 + r], tv.cb[2][6 + r], sizeof tv.cb[0][0]);
        CHECK(Run(pv, orig));
        CHECK(Run(tv, orig));
        CHECK(Run(xv, xf));
        float R[3], F[3], U[3], N[3], P[3], T[3];
        for (int k = 0; k < 3; ++k) {
            R[k] = pv.cb[2][6 + k][0]; F[k] = pv.cb[2][6 + k][1]; U[k] = pv.cb[2][6 + k][2];
            N[k] = pv.v[1][k]; P[k] = pv.o[3][k]; T[k] = tv.o[3][k];
        }
        auto dot = [](const float* u, const float* v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
        const float nr = 0.5f * dot(N, R), nu = 0.5f * dot(N, U);
        float n[3], d[3];
        for (int k = 0; k < 3; ++k) { n[k] = nr * R[k] + nu * U[k] - F[k]; d[k] = P[k] - T[k]; }
        const float sc = dot(d, n) / dot(F, n);
        float want[4] = {P[0] - F[0] * sc, P[1] - F[1] * sc, P[2] - F[2] * sc, 1.0f};
        CHECK(Near(xv.o[3], want));
        CHECK(!Near(xv.o[3], pv.o[3]));   // the lookup really moved (in depth)
        // ... along f only: the light-space position (the components across f) is P's
        float dx[3] = {xv.o[3][0] - P[0], xv.o[3][1] - P[1], xv.o[3][2] - P[2]};
        CHECK(std::fabs(dot(dx, R)) < 1e-4f && std::fabs(dot(dx, U)) < 1e-4f);
    }
    (void)s;
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
            CHECK_EQ(m.cbReads, 4);   // rows 5, 4, 6, 6
            CHECK_EQ(m.normalReg, 3u);
            CHECK_EQ(m.posReg, 4u);
            std::vector<uint8_t> out;
            CHECK(BbsRewrite::Transform(m, &out));
            // the copy is a valid container and matches nothing new: re-analysing it still finds the
            // shadow output, now fed from cb2 (it reads no cb0[4..7] in the shadow slice any more)
            const BbsRewrite::Match m2 = Analyse(out);
            CHECK(m2.container && m2.program);
            CHECK(!m2.matched);
            uint32_t temps2 = 0;
            for (const Dxbc::Instr& in : m2.prog.instrs)
                if (in.opcode == Op::DclTemps) temps2 = m2.prog.tokens[in.offset + 1];
            CHECK_EQ(temps2, 15u);   // 2T + 3
            CheckMeaning(s, b, out);
        }
    }
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
        Shape s; s.cb2Rows = 8;   // textureProjection not declared
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched);
    }
    {
        Shape s; s.flow = true;
        const BbsRewrite::Match m = Analyse(Blob(s));
        CHECK(!m.matched);
    }
    {
        Shape s; s.relCb0 = true;   // a relative camera-row read: cannot be redirected
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
    const Ext::Entry* e = Ext::FindEntry("billboard_shadow");
    CHECK(e != nullptr);
    if (e == nullptr) return;
    CHECK(e->tier == Ext::Tier::Extension);
    CHECK(e->cls == Ext::Class::A);
    CHECK(!e->defaultOn);
    CHECK_STREQ(Ext::SettingsKey(*e).c_str(), "ext.billboard_shadow");
    CHECK_STREQ(e->displayName, "Stable tree shading");
}
