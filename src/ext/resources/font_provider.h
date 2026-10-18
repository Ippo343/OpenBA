#pragma once
// ============================================================================
// src/ext/resources/font_provider.h -- ext.sharp_ui's font variants, provided (KITFREE RSCFONT)
// ============================================================================
// NOT RETAIL. Registers the provider for "Font/SharpFont" (the UI::Font, its
// material and its texture) and "Build/SharpFont" (the glyph sheet) with
// Ext::Resources (Resources.h). Called once from the ext boot
// (registry_boot.cpp's Ext::InitFromSettings), after the registry resolved.
// font_provider.cpp has the build; font_raster.h the rasteriser;
// font_sheet.h the sheet builder it calls.
// ============================================================================

namespace Ext {
namespace Resources {

void InitFonts();

}  // namespace Resources
}  // namespace Ext
