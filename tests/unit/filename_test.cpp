// ============================================================================
// tests/unit/filename_test.cpp — System::Filename standing on its own
// ============================================================================
// AdjustDelimiters, GetPathFilename with and without stripExt, GetPathIdentifer
// and GetPathDirectory, per B31_RULES rule 6. GetPathExtension is NOT covered
// here: its body is still in runtime/src/object/external_path_win.cpp with that
// lane's counters, and the member forwards to it (see the comment on the member).
// ============================================================================
#include "unit.h"
#include "../../runtime/types/system/Filename.h"

using System::Filename;
using System::String;

UNIT_TEST(filename_default_is_null) {
    Filename f;
    CHECK(f._data == nullptr);
    CHECK_EQ(f.GetLength(), 0);
}

UNIT_TEST(filename_adjust_delimiters_rewrites_slashes) {
    Filename f("a/b/c.rsc");
    CHECK_STREQ(f._data, "a\\b\\c.rsc");
    Filename g("already\\done");
    CHECK_STREQ(g._data, "already\\done");
}

UNIT_TEST(filename_adjust_delimiters_collapses_dotdot) {
    // "\.." with a preceding segment is spliced out, head = [0, lastDelim-1].
    Filename f("a\\b\\..\\c");
    CHECK_STREQ(f._data, "a\\c");
    Filename g("a/b/../c/d");
    CHECK_STREQ(g._data, "a\\c\\d");
    // ".." at the very end is NOT spliced: there is nothing after it, so the
    // backslash is treated as an ordinary delimiter.
    Filename h("a\\b\\..");
    CHECK_STREQ(h._data, "a\\b\\..");
}

UNIT_TEST(filename_from_string_shares_and_normalises_in_place) {
    String s("x/y");
    Filename f(s);
    // The buffer is ADOPTED, not copied, and the '/'->'\\' pass writes through
    // it — so the source String's characters change too. Retail's behaviour;
    // several call sites depend on it.
    CHECK(f._data == s._data);
    CHECK_STREQ(s._data, "x\\y");
}

UNIT_TEST(filename_copy_and_assign_share) {
    Filename a("dir\\file.txt");
    Filename b(a);
    CHECK(a._data == b._data);
    Filename c;
    c = a;
    CHECK(c._data == a._data);
    String s("other/path");
    c = s;                                  // the String& overload
    CHECK_STREQ(c._data, "other\\path");
}

UNIT_TEST(filename_move_steals) {
    Filename a("p\\q");
    char* raw = a._data;
    Filename b(static_cast<Filename&&>(a));
    CHECK(b._data == raw);
    CHECK(a._data == nullptr);
}

UNIT_TEST(filename_less_is_case_insensitive) {
    // The whole reason Filename has its own operator<.
    CHECK(!(Filename("Terrain\\Grass.rsc") < Filename("terrain\\grass.rsc")));
    CHECK(!(Filename("terrain\\grass.rsc") < Filename("Terrain\\Grass.rsc")));
    CHECK(Filename("a.rsc") < Filename("B.rsc"));
    Filename nul;
    CHECK(nul < Filename("a"));
    CHECK(!(Filename("a") < nul));
    CHECK(!(nul < nul));
}

UNIT_TEST(filename_get_path_filename) {
    // `stripExt` DOES NOT CONTROL THE STRIPPING. When a '.' is found at index
    // >= 1 the window is shortened to just before it either way — the flag is
    // only read on the NO-DOT branch, where true answers the null Filename and
    // false answers the whole leaf. Both of these are retail's; the parameter
    // is misnamed by its own logic and stays that way.
    Filename f("dir\\sub\\leaf.rsc");
    CHECK_STREQ(f.GetPathFilename(true)._data, "leaf");
    CHECK_STREQ(f.GetPathFilename(false)._data, "leaf");

    Filename bare("dir\\leaf");
    CHECK(bare.GetPathFilename(true)._data == nullptr);
    CHECK_STREQ(bare.GetPathFilename(false)._data, "leaf");

    Filename nodir("leaf.rsc");
    CHECK_STREQ(nodir.GetPathFilename(true)._data, "leaf");

    Filename nul;
    CHECK(nul.GetPathFilename(true)._data == nullptr);
}

UNIT_TEST(filename_get_path_identifer) {
    // Everything after the last ':'.
    CHECK_STREQ(Filename("data\\x.rsc:ident").GetPathIdentifer()._data, "ident");
    // No ':' and no path character -> the whole thing.
    CHECK_STREQ(Filename("bare").GetPathIdentifer()._data, "bare");
    // No ':' but a '.', '/' or '\\' was seen -> nothing: an identifier is a
    // bare name.
    CHECK(Filename("has.dot").GetPathIdentifer()._data == nullptr);
    CHECK(Filename("has\\slash").GetPathIdentifer()._data == nullptr);
    Filename nul;
    CHECK(nul.GetPathIdentifer()._data == nullptr);
}

UNIT_TEST(filename_get_path_directory) {
    // Everything up to and INCLUDING the last delimiter. The trailing '\' is
    // load-bearing: SetupDirectories appends to it with no separator of its own.
    CHECK_STREQ(Filename("C:\\Games\\Banished\\x.txt").GetPathDirectory()._data,
                "C:\\Games\\Banished\\");
    CHECK_STREQ(Filename("a/b/c").GetPathDirectory()._data, "a\\b\\");
    // No delimiter at all -> the NULL Filename, not an empty one.
    CHECK(Filename("nodelim").GetPathDirectory()._data == nullptr);
    Filename nul;
    CHECK(nul.GetPathDirectory()._data == nullptr);
}
