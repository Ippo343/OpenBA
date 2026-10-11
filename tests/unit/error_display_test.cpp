// ============================================================================
// tests/unit/error_display_test.cpp -- System::Error::Display's box, captured
// ============================================================================
// Phase D.4 SDL step W6. Error::Display (src/engine/system/error.cpp) builds retail's
// two clamped UTF-16 buffers and hands them to Platform::Window::ShowErrorMessage, the
// SDL message box in the exe. `make unit` links error.cpp (the allocator's OOM path
// calls it) but not SDL (D4_RULES C7), so this file is the unit build's definition of
// that primitive: it records the call instead of showing a box, which lets the tests
// check what retail would have shown.
// ============================================================================
#include "unit.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "platform_window.h"
#include "system/Error.h"

namespace {

int         g_calls = 0;
std::string g_text;      // ASCII-narrowed copies of the two UTF-16 buffers
std::string g_caption;

std::string Narrow(const uint16_t* s) {
    std::string out;
    for (size_t i = 0; s[i] != 0; ++i) out += (s[i] < 0x80) ? static_cast<char>(s[i]) : '?';
    return out;
}

void Reset() {
    g_calls = 0;
    g_text.clear();
    g_caption.clear();
    std::memset(System::Error::_errorString, 0, sizeof(System::Error::_errorString));
    System::Error::_hideDisplay = false;
    System::Error::_stopApplication = false;
}

}  // namespace

namespace Platform {
namespace Window {
void ShowErrorMessage(const uint16_t* textUtf16, const uint16_t* captionUtf16) {
    ++g_calls;
    g_text = Narrow(textUtf16);
    g_caption = Narrow(captionUtf16);
}
}  // namespace Window
}  // namespace Platform

UNIT_TEST(error_display_shows_retail_text_and_caption) {
    Reset();
    System::Error::SetText("VideoDevice: test failure.", false);
    System::Error::Display();
    CHECK_EQ(g_calls, 1);
    CHECK_STREQ(g_caption.c_str(), "Banished");
    CHECK_STREQ(g_text.c_str(),
                "A fatal error has occurred and the application cannot continue.\n"
                "VideoDevice: test failure.\n");
    Reset();
}

UNIT_TEST(error_display_hidden_shows_nothing) {
    Reset();
    System::Error::SetText("hidden", false);
    System::Error::_hideDisplay = true;
    System::Error::Display();
    CHECK_EQ(g_calls, 0);
    Reset();
}

// Retail clamps the text at 0x800 BYTES of UTF-16 output (0x400 code units), half the
// buffer, and leaves the terminator to the up-front zeroing when the bound is hit.
UNIT_TEST(error_display_clamps_at_retail_bound) {
    Reset();
    std::string longText(3000, 'x');
    System::Error::SetText(longText.c_str(), false);
    System::Error::Display();
    CHECK_EQ(g_calls, 1);
    CHECK_EQ(g_text.size(), size_t(0x400));
    Reset();
}
