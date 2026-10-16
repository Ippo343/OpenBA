#pragma once
// ============================================================================
// src/render/dx11/billboard_shadow/billboard_shadow_backend.h -- ext.billboard_shadow's render side,
// as the extension sees it (docs/extensions/TREE_SHADOW.md, handins/TS-CORE.md)
// ============================================================================
// NOT RETAIL. No D3D types: src/ext/billboard_shadow/billboard_shadow.cpp includes it (the
// foliage_aa_backend.h precedent). Everything D3D is billboard_shadow_stage.{h,cpp}.
// ============================================================================

namespace BbsBackend {

// The extension's one call (Ext::BillboardShadow::Sync, main thread). Idempotent.
//   on   points the seam (VideoDX11::g_extBillboardShadow) at the stage now; the rewritten vertex
//        shaders are made per program at load (PlatformCreate) or lazily at the first bind.
//   off  the binding thread's next vertex-program bind releases every rewritten shader and nulls
//        the seam: from then on the faithful path runs again (one null test per bind).
void Request(bool on);

}  // namespace BbsBackend
