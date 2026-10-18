// ============================================================================
// tests/unit/ext_font_raster_test.cpp -- the run-time font rasteriser (KITFREE RSCFONT)
// ============================================================================
// src/ext/resources/font_tables.{h,cpp} (pure: the variant path grammar, the
// names, the measured widths) always; src/platform/win/font_raster_win.cpp
// against the mod kit's compiled variants when they exist
// (build/ext_resources/WinData/Font/, tools/ext_resources.sh; found from a
// worktree at the main checkout) and the face is available -- otherwise the
// comparison SKIPS, it never fails for a missing oracle. The posix twin
// answers "absent", so the comparison skips off Windows.
// ============================================================================
#include "unit.h"
#include "ext/resources/font_raster.h"
#include "ext/resources/font_tables.h"
#include "kit_font_reader.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using namespace Ext::Resources;

bool Exists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
}

// The kit's compiled fonts: the repo root's build/, else the main checkout's
// when this is a worktree under <root>/.claude/worktrees/<name>/.
std::string KitFontDir() {
    const char* candidates[] = { "build/ext_resources/WinData/Font", "../../../build/ext_resources/WinData/Font" };
    for (const char* c : candidates)
        if (Exists(std::string(c) + "/Font_resource.crs")) return c;
    return std::string();
}

}  // namespace

UNIT_TEST(ext_font_variant_path_grammar) {
    VariantPath vp;
    CHECK(ParseVariantPath("Font/SharpFont_32.rsc:resource", vp));
    CHECK(vp.kit == 0 && vp.px == 32 && vp.part == "resource");
    CHECK(ParseVariantPath("Font/SharpFont_32.rsc", vp) && vp.part == "resource");
    CHECK(ParseVariantPath("Font/SharpFontSmall_19.rsc:material", vp));
    CHECK(vp.kit == 1 && vp.px == 19 && vp.part == "material");
    CHECK(ParseVariantPath("Font\\SharpFontLarge_64.rsc:texture", vp));
    CHECK(vp.kit == 3 && vp.px == 64 && vp.part == "texture");
    CHECK(ParseVariantPath("Build/SharpFontMedium_12Sheet.rsc:resource", vp));
    CHECK(vp.kit == 2 && vp.px == 12 && vp.part == "sheet");
    CHECK(!ParseVariantPath("Font/Font.rsc:resource", vp));
    CHECK(!ParseVariantPath("Font/SharpFont_32.rsc:glyphs", vp));
    CHECK(!ParseVariantPath("Font/SharpFontX_32.rsc", vp));
    CHECK(!ParseVariantPath("Build/SharpFont_32.rsc", vp));
    CHECK(!ParseVariantPath("Font/SharpFont_.rsc", vp));
    CHECK(!ParseVariantPath(nullptr, vp));

    const VariantNames n = MakeVariantNames(0, 32);
    CHECK_STREQ(n.file.c_str(), "SharpFont_32");
    CHECK_STREQ(n.fontPath.c_str(), "Font/SharpFont_32.rsc:resource");
    CHECK_STREQ(n.materialPath.c_str(), "Font/SharpFont_32.rsc:material");
    CHECK_STREQ(n.texturePath.c_str(), "Font/SharpFont_32.rsc:texture");
    CHECK_STREQ(n.sheetPath.c_str(), "Build/SharpFont_32Sheet.rsc:resource");
    CHECK_STREQ(n.sheetName.c_str(), "Build/SharpFont_32Sheet.rsc");
    CHECK_STREQ(n.imageName.c_str(), "Build/uiSharpFont_32Image.png");
    CHECK_STREQ(n.baseFontPath.c_str(), "Font/Font.rsc");
    CHECK_STREQ(n.baseMaterialPath.c_str(), "Font/FontMaterial.rsc");
    CHECK_STREQ(MakeVariantNames(1, 19).baseMaterialPath.c_str(), "Font/FontSmallMaterial.rsc");

    CHECK(CodepointFromSpriteName("c65") == 65);
    CHECK(CodepointFromSpriteName("c169") == 169);
    CHECK(CodepointFromSpriteName("c") == -1);
    CHECK(CodepointFromSpriteName("x65") == -1);
    CHECK(CodepointFromSpriteName("c65x") == -1);
    CHECK(CodepointFromSpriteName(nullptr) == -1);

    // The kit's measured widths at 32 px: '.' is (0, 1, 4) -> "..." = 15, ' ' is (0, 5, 1) -> 6.
    const GlyphRow dot{0, 1, 4, 0};
    const GlyphRow dots[3] = { dot, dot, dot };
    CHECK(MeasureRun(dots, 3) == 15);
    const GlyphRow space{0, 5, 1, -1};
    CHECK(MeasureRun(&space, 1) == 6);

    int32_t w = 0, h = 0;
    CHECK(KitSheetSize(32, w, h) && w == 256 && h == 256);    // 64 * 32^2 = 65536
    CHECK(KitSheetSize(9, w, h) && w == 128 && h == 128);
    CHECK(KitSheetSize(64, w, h) && w == 512 && h == 512);
    CHECK(!KitSheetSize(100, w, h));
}

// One compiled font against the rasteriser: same codepoints, glyph count,
// line height, baseline, ellipsis and space widths, every glyph row
// (preAdvance, postKern, postAdvance, offsetY). The kit's rule (Ghidra,
// UI::Font::Compile in Runtime-x64-profile.dll, RSCFONT.md "The kit's rule"):
// the row is the GetCharABCWidthsW triple (A, C, B) plus
// gmptGlyphOrigin.y - gmBlackBoxY, so a variant the kit compiled on this box
// matches EXACTLY (`exact`; otherwise each field within one pixel). `px` 0 =
// the compiled font's own height (the base fonts).
static void CompareVariant(const std::string& dir, const std::string& kitName, int32_t px, bool exact,
                           int32_t& rows, int32_t& rowsOff) {
    const std::string crs = dir + "/" + kitName + "_resource.crs";
    if (!Exists(crs)) return;
    KitFont::Font oracle;
    CHECK(KitFont::Load(crs.c_str(), kGameFace, oracle));
    if (oracle.glyphs.empty()) return;
    const std::vector<int32_t> cps = KitFont::GlyphCodepoints(oracle);
    if (px <= 0) px = oracle.height;
    RasterFont ours;
    std::string why;
    CHECK(RasterizeFont(kGameFace, px, cps.data(), static_cast<int32_t>(cps.size()), ours, &why));
    if (ours.glyphs.size() != cps.size()) { std::printf("  %s: rasterised %zu of %zu\n", kitName.c_str(), ours.glyphs.size(), cps.size()); return; }
    CHECK_STREQ(ours.face.c_str(), kGameFace);
    CHECK_EQ(ours.height, oracle.height);
    CHECK_EQ(ours.height, px);
    CHECK_EQ(ours.baseLine, oracle.baseLine);
    CHECK_EQ(static_cast<int32_t>(ours.glyphs.size()), static_cast<int32_t>(oracle.glyphs.size()));
    GlyphRow dot{0, 0, 0, 0}, sp{0, 0, 0, 0};
    for (size_t i = 0; i < cps.size(); ++i) {
        const KitFont::GlyphRow& k = oracle.glyphs[i];
        const RasterGlyph& g = ours.glyphs[i];
        CHECK_EQ(g.codepoint, cps[i]);
        CHECK_EQ(static_cast<int32_t>(g.alpha.size()), g.width * g.height);
        const bool same = g.preAdvance == k.preAdvance && g.postKern == k.postKern &&
                          g.postAdvance == k.postAdvance && g.offsetY == k.offsetY;
        ++rows;
        if (!same) {
            ++rowsOff;
            std::printf("  %s U+%04X: kit (%d %d %d %d) ours (%d %d %d %d)\n", kitName.c_str(), cps[i],
                        k.preAdvance, k.postKern, k.postAdvance, k.offsetY,
                        g.preAdvance, g.postKern, g.postAdvance, g.offsetY);
        }
        if (exact) {
            CHECK(same);
        } else {
            CHECK(std::abs(g.preAdvance - k.preAdvance) <= 1);
            CHECK(std::abs(g.postAdvance - k.postAdvance) <= 1);
            CHECK(std::abs(g.postKern - k.postKern) <= 1);
            CHECK(std::abs(g.offsetY - k.offsetY) <= 1);
        }
        if (cps[i] == '.') dot = GlyphRow{g.preAdvance, g.postKern, g.postAdvance, g.offsetY};
        if (cps[i] == ' ') sp = GlyphRow{g.preAdvance, g.postKern, g.postAdvance, g.offsetY};
    }
    const GlyphRow dots[3] = { dot, dot, dot };
    CHECK_EQ(MeasureRun(dots, 3), oracle.ellipsisWidth);
    CHECK_EQ(MeasureRun(&sp, 1), oracle.spaceWidth);
}

UNIT_TEST(ext_font_raster_matches_kit_variant) {
    const std::string dir = KitFontDir();
    if (dir.empty()) UNIT_SKIP("no kit-compiled fonts under build/ext_resources/WinData/Font (tools/ext_resources.sh)");
    std::string why;
    if (!FaceAvailable(kGameFace, &why)) UNIT_SKIP("typeface FrancophilSans not available");
    // Every variant tools/ext_fonts.py has the kit compile: Sharp<Kit>_<px>.
    static const char* const kKits[] = { "SharpFont", "SharpFontSmall", "SharpFontMedium", "SharpFontLarge" };
    int32_t rows = 0, rowsOff = 0, variants = 0;
    for (const char* kit : kKits) {
        for (int32_t px = 4; px <= 128; ++px) {
            const std::string name = std::string(kit) + "_" + std::to_string(px);
            if (!Exists(dir + "/" + name + "_resource.crs")) continue;
            ++variants;
            CompareVariant(dir, name, px, true, rows, rowsOff);
        }
    }
    std::printf("  font raster vs kit: %d variants, %d rows, %d differ\n", variants, rows, rowsOff);
    CHECK(variants > 0);
    CHECK_EQ(rowsOff, 0);
    // The game's base fonts as they sit in the kit's output (Font is recompiled
    // by tools/ext_resources.sh on every run; FontSmall / FontMedium as found).
    int32_t baseRows = 0, baseOff = 0;
    for (const char* base : { "Font", "FontSmall", "FontMedium", "FontLarge" })
        CompareVariant(dir, base, 0, true, baseRows, baseOff);
    std::printf("  font raster vs the base fonts: %d rows, %d differ\n", baseRows, baseOff);
    CHECK_EQ(baseOff, 0);
}

UNIT_TEST(ext_font_raster_rejects_absent_face) {
    RasterFont out;
    std::string why;
    // A face no machine has: never substituted (GDI would hand back another
    // face; the name check refuses it).
    const int32_t cps[] = { 'A' };
    CHECK(!RasterizeFont("NoSuchFace_OpenBA_Test", 24, cps, 1, out, &why));
    CHECK(out.glyphs.empty());
    CHECK(!why.empty());
    CHECK(!RasterizeFont(kGameFace, 1, cps, 1, out, &why));   // height out of range
}
