// ============================================================================
// tests/unit/filename_test.cpp — System::Filename standing on its own
// ============================================================================
// AdjustDelimiters, GetPathFilename with and without stripExt, GetPathIdentifer
// and GetPathDirectory, per B31_RULES rule 6, and (since 2026-10-17)
// GetPathExtension: its body is src/engine/io/filename_extension.cpp, linked
// here with that lane's counters defined in link_stubs.cpp; the member forwards
// to it (see the comment on the member).
// ============================================================================
#include "unit.h"
#include "system/Filename.h"

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

// CHECK_STREQ on a RETURNED Filename's `_data`: the Filename is held in a local first.
// `CHECK_STREQ(f.Get()._data, ...)` reads the buffer after the temporary that owns it
// has been destroyed (the macro's own `const char* _a = (a);` ends its full
// expression). The Windows heap leaves a freed block's bytes alone, so it passed
// there; glibc's tcache writes its link into the block on free (Phase D.4 POSIX,
// the Linux dry run).
#define CHECK_FN_STREQ(expr, s) do { const Filename _fn = (expr); CHECK_STREQ(_fn._data, s); } while (0)

UNIT_TEST(filename_get_path_filename) {
    // `stripExt` DOES NOT CONTROL THE STRIPPING. When a '.' is found at index
    // >= 1 the window is shortened to just before it either way — the flag is
    // only read on the NO-DOT branch, where true answers the null Filename and
    // false answers the whole leaf. Both of these are retail's; the parameter
    // is misnamed by its own logic and stays that way.
    Filename f("dir\\sub\\leaf.rsc");
    CHECK_FN_STREQ(f.GetPathFilename(true), "leaf");
    CHECK_FN_STREQ(f.GetPathFilename(false), "leaf");

    Filename bare("dir\\leaf");
    CHECK(bare.GetPathFilename(true)._data == nullptr);
    CHECK_FN_STREQ(bare.GetPathFilename(false), "leaf");

    Filename nodir("leaf.rsc");
    CHECK_FN_STREQ(nodir.GetPathFilename(true), "leaf");

    Filename nul;
    CHECK(nul.GetPathFilename(true)._data == nullptr);
}

UNIT_TEST(filename_get_path_identifer) {
    // Everything after the last ':'.
    CHECK_FN_STREQ(Filename("data\\x.rsc:ident").GetPathIdentifer(), "ident");
    // No ':' and no path character -> the whole thing.
    CHECK_FN_STREQ(Filename("bare").GetPathIdentifer(), "bare");
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
    CHECK_FN_STREQ(Filename("C:\\Games\\Banished\\x.txt").GetPathDirectory(),
                "C:\\Games\\Banished\\");
    CHECK_FN_STREQ(Filename("a/b/c").GetPathDirectory(), "a\\b\\");
    // No delimiter at all -> the NULL Filename, not an empty one.
    CHECK(Filename("nodelim").GetPathDirectory()._data == nullptr);
    Filename nul;
    CHECK(nul.GetPathDirectory()._data == nullptr);
}

// ---- GetPathExtension (2026-10-17: the real body, src/engine/io/filename_extension.cpp,
//      linked; before that a null stub stood in and the member was uncovered) ---------------
// Two backward scans: a ':' past index 1 shortens the window to end before it, then the
// last '.' in the window starts the extension. Retail's three edges are kept: a ':' at
// index 0 or 1 does not shorten; the '.' scan covers the directory part, so a dotted
// directory lends its "extension" to a file without one; a trailing '.' returns the NULL
// Filename (data == nullptr), not an empty one.
UNIT_TEST(filename_get_path_extension_cases) {
    CHECK_STREQ(Filename("data/x.rsc").GetPathExtension()._data, "rsc");
    CHECK_STREQ(Filename("data/x.rsc:identifier").GetPathExtension()._data, "rsc");   // the ':' trims
    CHECK_STREQ(Filename("a/b.c/x.tar.gz").GetPathExtension()._data, "gz");           // the LAST dot
    CHECK_STREQ(Filename("x.").GetPathExtension()._data, nullptr);                     // trailing dot: NULL
    CHECK_STREQ(Filename("dir.d/x.").GetPathExtension()._data, nullptr);
    CHECK_STREQ(Filename("noext").GetPathExtension()._data, nullptr);                  // no dot: NULL
    CHECK_STREQ(Filename("dir.d/noext").GetPathExtension()._data, "d\\noext");         // the directory's dot
    CHECK_STREQ(Filename("").GetPathExtension()._data, nullptr);                       // empty (allocated): NULL
    CHECK_STREQ(Filename().GetPathExtension()._data, nullptr);                         // null: NULL
    CHECK_STREQ(Filename(".hidden").GetPathExtension()._data, "hidden");               // dot at 0
    CHECK_STREQ(Filename("a.b.c").GetPathExtension()._data, "c");
    // A ':' at index 0 or 1 does not shorten the window (a drive letter): the dot after it
    // is found, and with none after it, nothing is.
    CHECK_STREQ(Filename("C:/x.sav").GetPathExtension()._data, "sav");
    CHECK_STREQ(Filename(":x.sav").GetPathExtension()._data, "sav");
    CHECK_STREQ(Filename("C:/x").GetPathExtension()._data, nullptr);
    // A ':' past index 1 with no dot before it: NULL; with the dot after the ':' only: NULL
    // too (the window ends before the ':').
    CHECK_STREQ(Filename("ab:x.sav").GetPathExtension()._data, nullptr);
    CHECK_STREQ(Filename("ab.c:x").GetPathExtension()._data, "c");
    // The result is a Filename: its delimiters are normalised like any other.
    CHECK_STREQ(Filename("dir.d/sub/noext").GetPathExtension()._data, "d\\sub\\noext");
}
