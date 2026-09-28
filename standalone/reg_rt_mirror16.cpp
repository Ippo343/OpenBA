// The name<->address mirror's last live rows (B3R W0-SM MIRROR, 2026-09-28).
//
// Until this commit every RegisterRuntimeExport / RegisterRuntimeAddr call was
// mirrored into the opposite door through standalone/rt_export_addrs.inc (the
// DLL's export directory). The mirror is deleted. Seven addresses were served
// ONLY by it and have a reader bound to them (a literal at a door, by binding):
// they are registered here, explicitly, with the body their name already holds.
// (The file keeps the name the rule sheet gave it; INVENTORY_C counted 16 rows by
// literal, which included two keys that are registered explicitly under a split
// string literal and eight whose only literal is a named constant nothing reads.
// docs/runtime_port/endgame_b/b3r_session/scripts/mirror_proof.py is the proof.)
//
// Each address takes the value its name already resolves to, which is exactly
// what the mirror copied. Every name below is registered once, with one body,
// by the *PortInit named in its comment, so this runs after all of them: from
// RuntimePortInit(), after the last *PortInit() and before the registry count is
// printed. W2-RT deletes this file when the readers are direct calls.
#include "resolve.h"

void RtMirror16Register() {
    // UI::StringTable::GetIndexFromName -- RtUi_StringTable_GetIndexFromName
    // (runtime/src/ui/sprite.cpp); read by documenttextdescription.cpp.
    RegisterRuntimeAddr(0x180002470,
        RtPortLookupExport("?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z"));
    // UI::SpriteSheet::GetSpriteIndexFromName -- RtUi_SpriteSheet_GetSpriteIndexFromName
    // (runtime/src/ui/sprite.cpp); read by documenttext.cpp.
    RegisterRuntimeAddr(0x1800031f0,
        RtPortLookupExport("?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"));
    // UI::String::operator System::String -- RtUi_UIString_ToSystemString
    // (runtime/src/ui/sprite.cpp); read by combodescription.cpp, documenttext.cpp.
    RegisterRuntimeAddr(0x180035020,
        RtPortLookupExport("??BString@UI@@QEBA?AV0System@@XZ"));
    // UI::NumberEditElement::SetValue -- NumberEditElement_SetValue
    // (runtime/src/ui/widgets_input.cpp, file-static); read by spinnerelement.cpp.
    RegisterRuntimeAddr(0x1800d4bb0,
        RtPortLookupExport("?SetValue@NumberEditElement@UI@@QEAAXH@Z"));
    // Graphics::AnimationState::Update -- RtGraphics_AnimationState_Update
    // (runtime/src/graphics/wave27_lane_gfxbig.cpp); read by animationstate_transforms.cpp.
    RegisterRuntimeAddr(0x1800fdb20,
        RtPortLookupExport("?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@"
                           "AEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@"
                           "AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@"
                           "PEAV78@AEAVVector3@8@_N6@Z"));
    // UI::SpriteContext::AddLine -- RtUi_SpriteContext_AddLine
    // (runtime/src/ui/spritecontext.cpp); read by graphelement.cpp.
    RegisterRuntimeAddr(0x18011cbe0,
        RtPortLookupExport("?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z"));
    // UI::SpriteContext::SetScissor -- RtUi_SpriteContext_SetScissor_Viewport
    // (runtime/src/ui/spritecontext.cpp); read by clipregionelement.cpp.
    RegisterRuntimeAddr(0x18011d050,
        RtPortLookupExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z"));
}
