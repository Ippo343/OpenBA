// ============================================================================
// tests/unit/open_url_test.cpp -- what Platform::Process::Open hands SDL_OpenURL
// ============================================================================
// Phase D.4 SDL step W7. Platform::Process::ToOpenUrl (src/platform/common/open_url.cpp)
// turns the target of Application::LaunchProcess's "open" arm (retail: ShellExecuteW
// "open") into a URL: URLs pass through, absolute paths become file URLs, relative
// paths pass through. No SDL (D4_RULES C7).
// ============================================================================
#include "unit.h"

#include <string>

#include "open_url.h"

using Platform::Process::ToOpenUrl;

UNIT_TEST(open_url_urls_pass_through) {
    CHECK(ToOpenUrl("https://www.shiningrocksoftware.com/") ==
          "https://www.shiningrocksoftware.com/");
    CHECK(ToOpenUrl("mailto:a@b.c") == "mailto:a@b.c");
    CHECK(ToOpenUrl("file:///C:/x") == "file:///C:/x");
    CHECK(ToOpenUrl("steam://run/242920") == "steam://run/242920");
}

UNIT_TEST(open_url_absolute_paths_become_file_urls) {
    CHECK(ToOpenUrl("C:\\Games\\Banished\\readme.txt") ==
          "file:///C:/Games/Banished/readme.txt");
    CHECK(ToOpenUrl("C:/My Documents/a#b?.txt") == "file:///C:/My%20Documents/a%23b%3F.txt");
    CHECK(ToOpenUrl("d:\\100%\\") == "file:///d:/100%25/");
    CHECK(ToOpenUrl("\\\\server\\share\\x") == "file://server/share/x");
    CHECK(ToOpenUrl("/home/u/x y") == "file:///home/u/x%20y");
    // UTF-8 bytes are kept (SDL converts the URL to UTF-16 for ShellExecuteW).
    CHECK(ToOpenUrl("C:\\caf\xC3\xA9") == "file:///C:/caf\xC3\xA9");
}

UNIT_TEST(open_url_relative_and_drive_letter_edge_cases) {
    // A drive letter is not a scheme; a bare "C:" with no separator is relative to that
    // drive's current folder and passes through, as does a relative path.
    CHECK(ToOpenUrl("C:") == "C:");
    CHECK(ToOpenUrl("C:x.txt") == "C:x.txt");
    CHECK(ToOpenUrl("readme.txt") == "readme.txt");
    CHECK(ToOpenUrl("Data\\x") == "Data\\x");
    CHECK(ToOpenUrl("") == "");
}
