// Dialog/AoOptions.rsc -- the Options dialog's fifth tab "Enhanced" (NOT RETAIL; block AOMENU,
// docs/extensions/AO.md §6.3). Added at run time by UI::TabElement::AddTab from
// OptionsDialog::OnOpened (src/ext/ambient_occlusion/options_tab.cpp). Every name here is NEW:
// a loose .crs never overrides a name the game's packages hold (IO::FileSystem::OpenFile looks
// in the packfile index first, handins/AOSPIKE.md S5). The look inherits the kit's own
// Options.rsc / SharedElements.rsc objects (resolved from data*.pkg at compile and run time).

StringTable aoStrings
{
	Entry _strings
	[
		{ String _name = "AoTab";			String _text = "Enhanced"; }
		{ String _name = "AoQuality";		String _text = "Ambient occlusion"; }
		{ String _name = "AoOpacity";		String _text = "AO strength"; }
	]
}

// The combo's items, in Params::quality order (0 = off, 1 Low, 2 Medium, 3 High).
StringTable aoQualityLevels
{
	Entry _strings
	[
		{ String _name = "Level0";		String _text = "Off";		}
		{ String _name = "Level1";		String _text = "Low";		}
		{ String _name = "Level2";		String _text = "Medium";	}
		{ String _name = "Level3";		String _text = "High";		}
	]
}

ButtonDescription buttonEnhanced : "SharedElements.rsc:raisedButton"
{
	Alignment _alignment = MidCenter;
	ElementDescription _content = "labelEnhanced";
}

// Options.rsc:labelOptions's look, with our string table and our icon (ExtensionsSprites.rsc).
LabelDescription labelEnhanced
{
	Alignment _alignment = MidCenter;

	int _bottomPad = 5;
	int _leftPad = 5;
	int _rightPad = 5;
	int _topPad = 5;

	int _minWidth = 40;
	int _minHeight = 40;

	Font _font = "Font\FontSmall.rsc";
	StringTable _stringTable = "AoOptions.rsc:aoStrings";
	SpriteSheet _spriteSheet = "ExtensionsSprites.rsc";

	int _spacing = 2;
	LabelPosition _labelPosition = TextBottom;
	Alignment _textAlignment = MidCenter;

	String _text = "AoTab";
	String _spriteName = "OptionsEnhanced";
}

// The page: Options.rsc:gameOptionsTable's shape, two rows.
TableDescription enhancedOptions
{
	int _leftPad = 6;
	int _topPad = 4;
	int _rightPad = 2;
	int _bottomPad = 4;

	Alignment _alignment = TopLeft;

	int _width = 2;
	int _height = 2;

	TableItem _items
	[
		{ int _x = 0; int _y = 0; ElementDescription _element = "textAoQuality"; }
		{ int _x = 0; int _y = 1; ElementDescription _element = "textAoOpacity"; }

		{ int _x = 1; int _y = 0; ElementDescription _element = "comboAoQuality"; }
		{ int _x = 1; int _y = 1; ElementDescription _element = "sliderAoOpacity"; }
	]
}

TextDescription textAoOption
{
	Font _font = "Font\FontMedium.rsc";
	StringTable _stringTable = "AoOptions.rsc:aoStrings";
	int _minWidth = 200;
	Color _color = 0xFFC0C0C0;
}

TextDescription textAoQuality : "textAoOption" { String _text = "AoQuality"; }
TextDescription textAoOpacity : "textAoOption" { String _text = "AoOpacity"; }

ComboDescription comboAoQuality : "SharedElements.rsc:textCombo"
{
	int _minWidth = 180;
	int _bottomPad = 3;
	StringTable _initialData = "AoOptions.rsc:aoQualityLevels";
}

SliderDescription sliderAoOpacity : "SharedElements.rsc:slider"
{
	int _bottomPad = 2;
	int _minWidth = 180;
	ElementDescription _number = "Options.rsc:number";
}
