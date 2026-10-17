// Font/SharpFontMedium_23.rsc -- ext.sharp_ui: the game's FontMedium (18 px) rasterised at 23 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// FontMedium objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/FontMedium.rsc"
{
	int _fontHeight = 23;
	int _sheetWidth = 256; int _sheetHeight = 256;
	String _sheetName = "Build/SharpFontMedium_23Sheet.rsc";
	String _imageName = "Build/uiSharpFontMedium_23Image.png";
	String _materialName = "Font/SharpFontMedium_23.rsc:material";
}

MaterialInstance material : "Font/FontMediumMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFontMedium_23.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontMediumTexture.rsc"
{
	String _imageName = "Build/uiSharpFontMedium_23Image.png";
}
