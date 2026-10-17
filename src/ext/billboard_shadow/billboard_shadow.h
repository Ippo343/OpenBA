// ============================================================================
// src/ext/billboard_shadow/billboard_shadow.h -- ext.billboard_shadow (class A, default OFF)
// ============================================================================
// NOT RETAIL. docs/extensions/TREE_SHADOW.md (the diagnosis and the design), handins/TS-CORE.md.
//
// "Stable tree shading". The deciduous canopies (oak, birch, the fruit and nut trees) and the
// blueberry bushes are camera-facing leaf cards. Retail casts their shadow from the cards turned to
// face the SUN (correct, view-independent) but looks the shadow up from the cards as drawn, facing
// the CAMERA: each card shadows half of itself and the canopy shows a straight light / dark line
// that turns as the camera orbits. With the row on, the billboard colour vertex shader (rewritten at
// load from the user's own installed bytecode, src/render/dx11/billboard_shadow/) looks the shadow up
// from the card rebuilt sun-facing, where its caster really was. The caster, the clip position, the
// lighting and the ground shadow are retail's.
//
// Two seams, both null by default and under the faithful profile:
//   VideoDX11::g_extBillboardShadow    the D3D stage (BbsBackend::Request, from Sync)
//   Graphics::SunLight::_extBillboardShadow  the sun's worldToCamera into the light block's unused
//                                      textureProjection rows (registry_boot.cpp installs it)
//
//   Sync()   boot (Ext::InitFromSettings) and every toggle (Ext::SetAndRecord): hands the row's
//            live value to the render side.
// ============================================================================
#pragma once

namespace Ext {
namespace BillboardShadow {

void Sync();

}  // namespace BillboardShadow
}  // namespace Ext
