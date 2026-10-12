// ============================================================================
// src/ext/sharp_ui/sharp_ui.h -- ext.sharp_ui (class A, default OFF)
// ============================================================================
// NOT RETAIL. "Sharp UI scaling": draw the interface crisp at high UI scales.
// While the extension is on:
//   1. the EFFECTIVE UI scale is an integer device-pixel ratio (1x, 2x, ...)
//      chosen from the Options slider and the back buffer (sharp_math.h: the
//      rule and the fit floor); the slider's own value, retail's setting in
//      settings.cfg, is never changed;
//   2. every axis-aligned UI quad's corners land on whole device pixels
//      (UI::SpriteContext::_extSharp, the engine end in ui/SharpDraw.h);
//   3. at a ratio >= 2 the UI sheets (sprites and font glyphs) are point-
//      sampled, batch by batch: a flush whose quads are all pixel-exact
//      (each texel a whole number of device pixels, or a long fill) binds a
//      point sampler (Video::ImageBufferData::_extPointSampling, the DX11
//      Bind); one with a picture stretched off-integer keeps the linear one.
// Class A: no serialized state, no sim effect, live. Toggling the extension or
// moving the slider re-derives _uiScale on the next update, and retail's
// per-frame UserInterface::Update relayouts every dialog at it, the way the
// slider's own apply does.
//
// The seams (each `if (Ext::Ext().sharp_ui ...) <ours> else <retail's line>`):
//   ApplyKeyMapToUI 0x140099260 / ApplyGameSettings 0x140072420 (optionsdialog
//     .cpp): the derivation -- `_uiScale = EffectiveUIScale(slider)`
//   GameUpdateLoop::Update step 9 / MenuDialogLoopT::Update: `SyncScale(ui)`
//     before the UI update (the live toggle, a resolution change)
//   GameUpdateLoop::Draw step 12 / MenuDialogLoopT::Draw: `Draw(ui, ctx)`
//     around retail's UserInterface::Draw, which runs unchanged inside it
// TRACE (stderr, standalone_error.log), one line per change:
//   [SharpUI] on scale=<_uiScale> ratio=<1/scale> integer=<0|1> snap=<0|1>
//             point=<0|1> slider=<%> wanted=<k>x fit=<k>x effective=<k>x
//             clamped=<0|1> bb=<WxH> layout=<WxH>
//   [SharpUI] off scale=<retail's slider/100> (retail's derivation restored)
//   [SharpUI] overflow dialog=<resource> rect=<l,t,r,b> layout=<WxH>
//             (an open dialog that does not fit the layout space; once per
//             dialog per scale: the evidence for the fit floor)
// ============================================================================
#pragma once

namespace UI { class UserInterface; }
namespace Video { class Context; }

namespace SharpUI {

// The derivation seam: the _uiScale for the slider's `percent` on the current
// back buffer (sharp_math.h ChooseScale). Call only when Ext::Ext().sharp_ui.
float EffectiveUIScale(int percent);

// True after SyncScale has set a UI's scale and before it handed it back:
// the update seam calls SyncScale while this is true so that switching the
// extension off restores retail's derivation once.
bool OwnsScale();

// Once per update, before UserInterface::Update. On: _uiScale = the effective
// scale (traced when it changes). Off while owning `ui`: retail's
// slider / 100 restored, once.
void SyncScale(UI::UserInterface& ui);

// Once per frame, in place of `ui.Draw(context)`: sets the engine-side snap
// pointer (with the point permission at an integer ratio >= 2), runs retail's
// UserInterface::Draw, clears it and the point-sampling flag (forcing the
// texture slots to rebind). Call only when Ext::Ext().sharp_ui.
void Draw(UI::UserInterface& ui, Video::Context& context);

}  // namespace SharpUI
