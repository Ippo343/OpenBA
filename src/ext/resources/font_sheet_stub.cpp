// ============================================================================
// src/ext/resources/font_sheet_stub.cpp -- no sheet builder linked (KITFREE RSCFONT)
// ============================================================================
// NOT RETAIL. font_sheet.h has the contract. This file goes when block
// RSCSPRITE's sheet_builder lands: the coordinator replaces it with a
// BuildFontSheet over sheet_builder.h. Until then a provided variant has no
// sheet, the provider returns null and ext.sharp_ui's retail(...) fallback
// stands -- nothing is half-built.
// ============================================================================
#include "ext/resources/font_sheet.h"

#include <cstdio>

namespace Ext {
namespace Resources {

bool BuildFontSheet(const FontSheetInput& in, FontSheetOutput& out) {
    out = FontSheetOutput();
    std::fprintf(stderr, "[ExtRes] font sheet %s: no sheet builder linked (RSCSPRITE's sheet_builder), %d glyphs not built\n",
                 in.sheetPath != nullptr ? in.sheetPath : "?", in.count);
    return false;
}

}  // namespace Resources
}  // namespace Ext
