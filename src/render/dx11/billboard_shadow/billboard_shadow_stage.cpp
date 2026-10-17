// ============================================================================
// src/render/dx11/billboard_shadow/billboard_shadow_stage.cpp -- billboard_shadow_stage.h,
// billboard_shadow_backend.h
// ============================================================================
// NOT RETAIL (ext.billboard_shadow). Log lines: `[BillboardShadow] ...` on stderr
// (standalone_error.log).
// ============================================================================
#include "billboard_shadow/billboard_shadow_stage.h"

#include "billboard_shadow/billboard_shadow_backend.h"
#include "billboard_shadow/receiver_rewrite.h"

#include "dx11_backend_access.h"
#include "graphics/SunLight.h"   // Graphics::SunLight::_extBillboardShadowLive (the emit's latch)

#include <cstdio>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace VideoDX11 {

std::atomic<BbsStage::Stage*> g_extBillboardShadow{nullptr};

namespace BbsStage {
namespace {

struct Program {
    ID3D11VertexShader* xf[kShaderVariantCount]     = {};
    ID3D11VertexShader* retail[kShaderVariantCount] = {};   // what the copies were made beside
};

struct State {
    std::mutex    mu;
    bool          reqOn  = false;   // the extension's request (main thread)
    ID3D11Device* device = nullptr; // the device the copies belong to
    std::unordered_map<VertexProgramData*, Program> programs;
    // evidence counters (since the last reset)
    int  blobsSeen = 0, blobsTransformed = 0, createFailed = 0;
    long xfBinds = 0, vertexBinds = 0;
    std::set<std::string> logged;   // a blob is logged once per run
};

State& S() {
    // Never destroyed: the process exit must not Release COM objects after the device is gone.
    static State* s = new State();
    return *s;
}

Stage g_stage;

template <class T> void Rel(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

std::string HashName(const uint32_t sum[4]) {
    char b[24];
    std::snprintf(b, sizeof b, "%08x%08x", sum[0], sum[1]);
    return b;
}

void ReleaseAll(State& s, const char* why) {
    if (s.blobsTransformed || s.xfBinds)
        std::fprintf(stderr,
                     "[BillboardShadow] stage reset (%s): %d blobs seen, %d rewritten, %d create failures, "
                     "%ld of %ld vertex binds rewritten\n",
                     why, s.blobsSeen, s.blobsTransformed, s.createFailed, s.xfBinds, s.vertexBinds);
    for (auto& kv : s.programs)
        for (ID3D11VertexShader*& x : kv.second.xf) Rel(x);
    s.programs.clear();
    s.blobsSeen = s.blobsTransformed = s.createFailed = 0;
    s.xfBinds = s.vertexBinds = 0;
}

// This program's copies, made now from its own byte code. Under s.mu.
Program& Build(State& s, VertexProgramData* p) {
    if (s.device != Dx11Device()) {
        if (s.device != nullptr) ReleaseAll(s, "device changed");
        s.device = Dx11Device();
    }
    Program& pr = s.programs[p];
    for (ID3D11VertexShader*& x : pr.xf) Rel(x);
    const ::Video::ShaderProgram* owner = p->_owner;
    const RtByteCodeBlob* blobs = owner->_byteCode.GetData();
    for (int i = 0; i < kShaderVariantCount; ++i) {
        pr.retail[i] = p->variants[i];
        if (i >= owner->_byteCode.GetSize() || p->variants[i] == nullptr) continue;
        const uint8_t* blob = blobs[i].GetData();
        const size_t   size = static_cast<size_t>(blobs[i].GetSize());
        BbsRewrite::Match m;
        BbsRewrite::Analyse(blob, size, &m);
        ++s.blobsSeen;
        const std::string hash = m.container ? HashName(m.cont.checksum) : std::string("(no container)");
        if (!m.matched) {
            // Untouched. Logged once per blob per run when it looked like ours (it reads both windows)
            // or could not be read at all; every other vertex shader is not worth a line.
            if ((m.candidate || !m.program) && s.logged.insert("u" + hash).second)
                std::fprintf(stderr, "[BillboardShadow] untouched %s v%d (%zu B): %s\n", hash.c_str(), i, size,
                             m.why.c_str());
            continue;
        }
        std::vector<uint8_t> out;
        if (!BbsRewrite::Transform(m, &out)) {
            ++s.createFailed;
            std::fprintf(stderr, "[BillboardShadow] rewrite of %s v%d FAILED its self-check: kept retail\n",
                         hash.c_str(), i);
            continue;
        }
        ID3D11VertexShader* vs = nullptr;
        const HRESULT hr = s.device->CreateVertexShader(out.data(), out.size(), nullptr, &vs);
        if (hr != S_OK || vs == nullptr) {
            ++s.createFailed;
            std::fprintf(stderr, "[BillboardShadow] CreateVertexShader of the copy of %s v%d failed hr=0x%08lx: "
                                 "kept retail\n", hash.c_str(), i, static_cast<unsigned long>(hr));
            Rel(vs);
            continue;
        }
        pr.xf[i] = vs;
        ++s.blobsTransformed;
        if (s.logged.insert("t" + hash).second)
            std::fprintf(stderr, "[BillboardShadow] rewrote %s v%d: %zu B -> %zu B (temps %u -> %u, shadow o%u, "
                                 "%d slice instructions cloned from #%d, %d worldToCamera reads -> cb2[6..9], "
                                 "lookup projected along the light from P r%u onto the caster plane (N r%u) at #%d)\n",
                         hash.c_str(), i, size, out.size(), m.temps, m.temps * 2 + 3, m.shadowOut, m.sliceCount,
                         m.first, m.cbReads, m.posReg, m.normalReg, m.insertAt);
    }
    return pr;
}

}  // namespace

void Stage::BindVertex(ID3D11DeviceContext* ctx, VertexProgramData* p, int variant) {
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mu);
    ID3D11VertexShader* const retail = p->variants[variant];
    if (!s.reqOn) {
        // Off: everything released, the seam cleared, retail from this bind on.
        ReleaseAll(s, "off");
        g_extBillboardShadow.store(nullptr, std::memory_order_release);
        std::fprintf(stderr, "[BillboardShadow] stage off: copies released, seam cleared\n");
        ctx->VSSetShader(retail, nullptr, 0);
        return;
    }
    ++s.vertexBinds;
    if (s.device != Dx11Device()) {
        ReleaseAll(s, "device changed");
        s.device = Dx11Device();
    }
    auto it = s.programs.find(p);
    Program* pr = it != s.programs.end() ? &it->second : nullptr;
    if (pr == nullptr || pr->retail[variant] != retail) pr = &Build(s, p);   // first bind, or re-created
    ID3D11VertexShader* const xf = pr->xf[variant];
    // The latch: the light block holds the sun's worldToCamera for this emit (SunLight::DrawLight).
    const bool use = xf != nullptr && ::Graphics::SunLight::_extBillboardShadowLive;
    if (use && ++s.xfBinds == 1) std::fprintf(stderr, "[BillboardShadow] first rewritten bind\n");
    ctx->VSSetShader(use ? xf : retail, nullptr, 0);
}

void Stage::VertexCreated(VertexProgramData* p) {
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mu);
    if (!s.reqOn) return;   // the next bind clears the seam
    Build(s, p);
}

void Stage::VertexDestroyed(VertexProgramData* p) {
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mu);
    auto it = s.programs.find(p);
    if (it == s.programs.end()) return;
    for (ID3D11VertexShader*& x : it->second.xf) Rel(x);
    s.programs.erase(it);
}

}  // namespace BbsStage
}  // namespace VideoDX11

namespace BbsBackend {

void Request(bool on) {
    using namespace VideoDX11;
    BbsStage::State& s = BbsStage::S();
    std::lock_guard<std::mutex> lock(s.mu);
    if (on == s.reqOn) return;
    s.reqOn = on;
    if (on) g_extBillboardShadow.store(&BbsStage::g_stage, std::memory_order_release);
    std::fprintf(stderr, "[BillboardShadow] request %s%s\n", on ? "on: seam set" : "off",
                 on ? "" : (g_extBillboardShadow.load(std::memory_order_acquire) ? ": cleared at the next vertex bind" : ""));
}

}  // namespace BbsBackend
