// Dialog/ExtensionsStringTable.rsc -- the Extensions dialog's and menu button's texts (NOT RETAIL).
// Our own table, so the game's Dialog/StringTable.rsc is never edited (the kit's README, section 5:
// "Take care with resources that might be shared"). Compiled by tools/ext_resources.sh.
// The rows' names and descriptions come from the registry (src/ext/registry.cpp) through the
// general table's "Parameter" string (@0); everything fixed is here.

StringTable resource
{
	Entry _strings
	[
		// the menu button (main menu and pause menu)
		{ String _name = "Extensions";				String _text = "Extensions"; }
		{ String _name = "ExtensionsTip";			String _text = "Turn fixes for the game's known bugs and new features on or off."; }

		// the dialog's chrome
		{ String _name = "ExtensionsTitle";			String _text = "Fixes and Extensions"; }
		{ String _name = "ExtensionsProfile";		String _text = "(profile: @0)"; }
		{ String _name = "ExtensionsTab";			String _text = "Fixes and extensions"; }
		{ String _name = "ExtensionsHeaderName";	String _text = "Name"; }
		{ String _name = "ExtensionsHeaderDesc";	String _text = "Description"; }
		{ String _name = "ExtensionsHeaderOn";		String _text = "On"; }
		{ String _name = "ExtensionsToggleTip";		String _text = "Turn this fix or extension on or off. Press OK to apply."; }

		// a row's kind: the tier and the class letter (docs/extensions/DESIGN.md section 2)
		{ String _name = "ExtensionsKindFix";		String _text = "Fix: a retail bug corrected"; }
		{ String _name = "ExtensionsKindA";			String _text = "Extension, class A: toggles live"; }
		{ String _name = "ExtensionsKindB";			String _text = "Extension, class B: changes play, locked per game"; }
		{ String _name = "ExtensionsKindC";			String _text = "Extension, class C: extends the save, locked per game"; }
		{ String _name = "ExtensionsKindOther";		String _text = "Extension"; }

		// a row's lock state
		{ String _name = "ExtensionsStateLive";		String _text = "live"; }
		{ String _name = "ExtensionsStateLocked";	String _text = "locked for this game"; }
		{ String _name = "ExtensionsStateLockedOn";	String _text = "locked for this game (on from the next game)"; }
		{ String _name = "ExtensionsStateLockedOff";	String _text = "locked for this game (off from the next game)"; }
	]
}
