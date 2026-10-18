#pragma once
// ============================================================================
// src/ext/resources/ext_sprites.h -- the Extensions sprite sheet provider
// (docs/extensions/KITFREE.md §3, block RSCSPRITE). NOT RETAIL.
// ============================================================================
// Provides `Dialog/ExtensionsSprites.rsc`, `Dialog/ExtensionsSpritesMaterial.rsc`
// and `Dialog/ExtensionsSpritesTexture.rsc` (each ":resource") from the baked
// bitmaps of ext_sprites_data.h through the sheet builder, so the Extensions
// button icon, the tier glyphs and the per-entry icons need no loose
// `Dialog/ExtensionsSprites*.crs` in the game directory.

namespace Ext {
namespace Resources {

// Registers the provider (prefix "Dialog/ExtensionsSprites"). Called once from
// the ext boot (registry_boot.cpp, after Install()).
void InitSprites();

}  // namespace Resources
}  // namespace Ext
