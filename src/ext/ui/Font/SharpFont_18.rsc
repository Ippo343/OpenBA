// Font/SharpFont_18.rsc -- ext.sharp_ui: the game's Font (24 px) rasterised at 18 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// Font objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/Font.rsc"
{
	int _fontHeight = 18;
	int _sheetWidth = 256; int _sheetHeight = 128;
	String _sheetName = "Build/SharpFont_18Sheet.rsc";
	String _imageName = "Build/uiSharpFont_18Image.png";
	String _materialName = "Font/SharpFont_18.rsc:material";
}

MaterialInstance material : "Font/FontMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFont_18.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontTexture.rsc"
{
	String _imageName = "Build/uiSharpFont_18Image.png";
}
