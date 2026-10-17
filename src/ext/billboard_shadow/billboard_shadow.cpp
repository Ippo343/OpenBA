// ============================================================================
// src/ext/billboard_shadow/billboard_shadow.cpp -- billboard_shadow.h
// ============================================================================
// NOT RETAIL. ext.billboard_shadow (docs/extensions/handins/TS-CORE.md).
// ============================================================================
#include "ext/billboard_shadow/billboard_shadow.h"

#include "ext/registry.h"

#include "billboard_shadow/billboard_shadow_backend.h"   // src/render/dx11/billboard_shadow: no D3D types

namespace Ext {
namespace BillboardShadow {

void Sync() {
    BbsBackend::Request(Ext::Ext().billboard_shadow);
}

}  // namespace BillboardShadow
}  // namespace Ext
