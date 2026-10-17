#pragma once
// ============================================================================
// src/render/dx11/billboard_shadow/billboard_shadow_stage.h -- ext.billboard_shadow's D3D side: the
// rewritten billboard vertex shaders (docs/extensions/TREE_SHADOW.md §4.2, handins/TS-CORE.md)
// ============================================================================
// NOT RETAIL.
//
// THE SEAM (DESIGN.md §3 rule 5): `VideoDX11::g_extBillboardShadow`, null by default. With the row
// off it is null for the whole run and the three retail bodies that test it pay one load + branch:
//   VertexProgramData::Bind            per vertex-program bind (the stage binds instead)
//   VertexProgramData::PlatformCreate  per program load (make the copies when on)
//   ~VertexProgramData                 (forget what the stage holds)
//
// The stage binds a program's rewritten variant only while
// `Graphics::SunLight::_extBillboardShadowLive` is true: SunLight::DrawLight sets it in the same
// emit (same thread) right after it wrote the sun camera's worldToCamera into the light block's
// textureProjection rows, the constants the rewritten shader reads. So a rewritten shader never runs
// over the zero rows retail leaves there (a toggle mid-frame, a frame with no sun light).
//
// Set by BbsBackend::Request (the extension, main thread) when the row goes on; cleared by the stage
// itself on the binding thread at the first bind after an off request, once everything is released.
// All state is under one mutex: programs are created on loader threads, bound on the main or the
// render thread. A device change (device lost) releases every copy; they are re-made lazily.
// ============================================================================
#include <atomic>

struct ID3D11DeviceContext;

namespace VideoDX11 {

class VertexProgramData;

namespace BbsStage { class Stage; }

// THE SEAM. Null = faithful.
extern std::atomic<BbsStage::Stage*> g_extBillboardShadow;

namespace BbsStage {

class Stage {
public:
    // VertexProgramData::Bind with the seam set. `variant` is retail's post-fallback index.
    void BindVertex(ID3D11DeviceContext* ctx, VertexProgramData* p, int variant);
    // VertexProgramData::PlatformCreate succeeded: (re)make this program's copies now.
    void VertexCreated(VertexProgramData* p);
    void VertexDestroyed(VertexProgramData* p);
};

}  // namespace BbsStage
}  // namespace VideoDX11
