// Dialog/ExtensionsSprites.rsc -- the Extensions sprites (NOT RETAIL): the menu button's icon and
// the per-tier row glyphs. Programmer art (src/ext/ui/Dialog/Sprite/Extensions/*.png, 16 x 16 like
// the Mods button's "Mod" sprite, Dialog/Sprite/ButtonIcons/Scenario.png); the user authors the real
// art later by replacing the PNGs. The kit's sprite-sheet form (Dialog/SpriteSheet.rsc,
// example/building/UI/ApiarySpriteSheet.rsc): the compiler packs the sources into _imageName.

SpriteSheet resource
{
	String _materialName = "ExtensionsSpritesMaterial.rsc";
	String _imageName = "Build/ExtensionsSprites.png";
	int _imageWidth = 64;
	int _imageHeight = 64;
	bool _padForFiltering = true;

	Sprite _sprites
	[
		{ String _name = "ExtensionsIcon";		String _source = "Sprite/Extensions/ExtensionsIcon.png"; }
		{ String _name = "ExtensionsTierFix";	String _source = "Sprite/Extensions/TierFix.png"; }
		{ String _name = "ExtensionsTierA";		String _source = "Sprite/Extensions/TierA.png"; }
		{ String _name = "ExtensionsTierB";		String _source = "Sprite/Extensions/TierB.png"; }
		{ String _name = "ExtensionsTierC";		String _source = "Sprite/Extensions/TierC.png"; }
	]
}
