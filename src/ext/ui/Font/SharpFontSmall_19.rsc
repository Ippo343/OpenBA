// Font/SharpFontSmall_19.rsc -- ext.sharp_ui: the game's FontSmall (14 px) rasterised at 19 px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. Each object derives from the game's own
// FontSmall objects by reference and overrides only the height, the sheet and the names of its outputs.

Font resource : "Font/FontSmall.rsc"
{
	int _fontHeight = 19;
	int _sheetWidth = 256; int _sheetHeight = 128;
	String _sheetName = "Build/SharpFontSmall_19Sheet.rsc";
	String _imageName = "Build/uiSharpFontSmall_19Image.png";
	String _materialName = "Font/SharpFontSmall_19.rsc:material";
}

MaterialInstance material : "Font/FontSmallMaterial.rsc"
{
	Texture _textures [ { String _name = "diffuse"; ImageBuffer _texture = "Font/SharpFontSmall_19.rsc:texture"; } ]
}

ImageBuffer texture : "Font/FontSmallTexture.rsc"
{
	String _imageName = "Build/uiSharpFontSmall_19Image.png";
}
