// Font/SharpFontMedium_12.rsc -- ext.sharp_ui: the game's FontMedium (18 px) rasterised at 12 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// FontMedium objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/FontMedium.rsc"
{
	int _fontHeight = 12;
	int _sheetWidth = 128; int _sheetHeight = 128;
	String _sheetName = "Build/SharpFontMedium_12Sheet.rsc";
	String _imageName = "Build/uiSharpFontMedium_12Image.png";
	String _materialName = "Font/SharpFontMedium_12.rsc:material";
}

MaterialInstance material : "Font/FontMediumMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFontMedium_12.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontMediumTexture.rsc"
{
	String _imageName = "Build/uiSharpFontMedium_12Image.png";
}
