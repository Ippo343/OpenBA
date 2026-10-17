// Font/SharpFontLarge_46.rsc -- ext.sharp_ui: the game's FontLarge (48 px) rasterised at 46 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// FontLarge objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/FontLarge.rsc"
{
	int _fontHeight = 46;
	int _sheetWidth = 512; int _sheetHeight = 512;
	String _sheetName = "Build/SharpFontLarge_46Sheet.rsc";
	String _imageName = "Build/uiSharpFontLarge_46Image.png";
	String _materialName = "Font/SharpFontLarge_46.rsc:material";
}

MaterialInstance material : "Font/FontLargeMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFontLarge_46.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontLargeTexture.rsc"
{
	String _imageName = "Build/uiSharpFontLarge_46Image.png";
}
