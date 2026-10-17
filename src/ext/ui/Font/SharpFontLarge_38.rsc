// Font/SharpFontLarge_38.rsc -- ext.sharp_ui: the game's FontLarge (48 px) rasterised at 38 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// FontLarge objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/FontLarge.rsc"
{
	int _fontHeight = 38;
	int _sheetWidth = 512; int _sheetHeight = 256;
	String _sheetName = "Build/SharpFontLarge_38Sheet.rsc";
	String _imageName = "Build/uiSharpFontLarge_38Image.png";
	String _materialName = "Font/SharpFontLarge_38.rsc:material";
}

MaterialInstance material : "Font/FontLargeMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFontLarge_38.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontLargeTexture.rsc"
{
	String _imageName = "Build/uiSharpFontLarge_38Image.png";
}
