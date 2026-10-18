// ============================================================================
// src/ext/resources/font_sheet.cpp -- the font provider's sheet over the
// sprite sheet builder (KITFREE: RSCFONT -> RSCSPRITE). NOT RETAIL.
// ============================================================================
// font_sheet.h has the contract. The glyph bitmaps are coverage only; the
// kit's font sheets are white texels with the coverage as alpha, so each glyph
// is expanded to RGBA here and handed to BuildSheetObjects with the kit's own
// sprite form: name "c<codepoint>", group "Build\<file>c<codepoint>.png" (what
// the kit writes for a glyph it rasterised into Build/), padded for filtering.
// The material derives from the game's Font/<Kit>Material.rsc the way the
// kit's does: the same Graphics::Material, one "diffuse" texture (ours), no
// constants -- FontMaterial.rsc has nothing else to inherit.
// The sheet grows to the next kit size when the glyphs do not fit the
// suggested one (tools/ext_fonts.py's ladder).
// ============================================================================
#include "ext/resources/font_sheet.h"
#include "ext/resources/sheet_builder.h"
#include "ext/resources/Resources.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "graphics/Material.h"
#include "object/External.h"
#include "system/Filename.h"
#include "ui/SpriteSheet.h"
#include "video/ImageBuffer.h"

namespace Ext {
namespace Resources {

namespace {

// "Build/SharpFont_32Sheet.rsc:resource" -> "SharpFont_32"
std::string FileStem(const char* sheetPath) {
    std::string s = sheetPath != nullptr ? sheetPath : "";
    const size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s.erase(0, slash + 1);
    const size_t sheet = s.find("Sheet.rsc");
    if (sheet != std::string::npos) s.erase(sheet);
    return s;
}

const int kSizes[][2] = {{128, 128}, {256, 128}, {256, 256}, {512, 256}, {512, 512}, {1024, 512}, {1024, 1024}};

}  // namespace

bool BuildFontSheet(const FontSheetInput& in, FontSheetOutput& out) {
    out = FontSheetOutput();
    Object::ExternalFactory* const factory = Object::ExternalFactory::_singleton;
    if (factory == nullptr || in.count <= 0 || in.sheetPath == nullptr) return false;

    // The base material instance: its Graphics::Material is the one the kit's
    // font material names (Material/UI/uiMaterial.rsc).
    Graphics::MaterialInstance* const baseInst =
        factory->FindOrLoadAs<Graphics::MaterialInstance>(System::Filename(in.baseMaterialPath));
    if (!Object::IsDerivedFromNullSafe(baseInst, Graphics::MaterialInstance::TypeId())) {
        std::fprintf(stderr, "[ExtRes] font sheet %s: base material %s did not load\n", in.sheetPath,
                     in.baseMaterialPath != nullptr ? in.baseMaterialPath : "?");
        return false;
    }
    Graphics::Material* const baseMaterial = baseInst->_material.Get();
    if (baseMaterial == nullptr) {
        std::fprintf(stderr, "[ExtRes] font sheet %s: %s has no Material\n", in.sheetPath, in.baseMaterialPath);
        return false;
    }
    const System::Filename baseMaterialFile = baseMaterial->GetPath();

    // The bitmaps as the builder wants them: white, alpha = coverage.
    const std::string stem = FileStem(in.sheetPath);
    std::vector<std::vector<uint8_t>> rgba(static_cast<size_t>(in.count));
    std::vector<std::string> groups(static_cast<size_t>(in.count));
    std::vector<SheetBitmap> bitmaps(static_cast<size_t>(in.count));
    for (int32_t i = 0; i < in.count; ++i) {
        const size_t k = static_cast<size_t>(i);
        const int w = in.widths[i], h = in.heights[i];
        std::vector<uint8_t>& px = rgba[k];
        px.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
        const uint8_t* a = in.bitmaps[i];
        for (size_t t = 0; t < static_cast<size_t>(w) * static_cast<size_t>(h); ++t) {
            px[t * 4 + 0] = 255; px[t * 4 + 1] = 255; px[t * 4 + 2] = 255; px[t * 4 + 3] = a[t];
        }
        groups[k] = "Build\\" + stem + in.names[i] + ".png";
        bitmaps[k] = SheetBitmap{in.names[i], groups[k].c_str(), w, h, px.data()};
    }

    const std::string imageName = "Build/ui" + stem + "Image.png";
    const std::string materialName = "Font/" + stem + ".rsc:material";
    SheetSpec spec;
    spec.padForFiltering = true;
    spec.materialName = materialName.c_str();
    spec.imageName = imageName.c_str();

    // The kit's size first, then up the ladder until the glyphs fit.
    size_t first = 0;
    for (size_t s = 0; s < sizeof kSizes / sizeof kSizes[0]; ++s)
        if (kSizes[s][0] == in.sheetWidth && kSizes[s][1] == in.sheetHeight) { first = s; break; }
    SheetObjects built;
    for (size_t s = first; s < sizeof kSizes / sizeof kSizes[0] && built.sheet == nullptr; ++s) {
        spec.sheetWidth = kSizes[s][0];
        spec.sheetHeight = kSizes[s][1];
        SheetLayout probe;
        if (!PackSheet(spec, bitmaps.data(), in.count, probe)) continue;
        built = BuildSheetObjects(spec, bitmaps.data(), in.count, baseMaterialFile._data, "diffuse");
    }
    if (built.sheet == nullptr || built.material == nullptr || built.texture == nullptr) {
        std::fprintf(stderr, "[ExtRes] font sheet %s: %d glyphs not built\n", in.sheetPath, in.count);
        DestroySheetObjects(built);
        return false;
    }

    // Registered in dependency order: texture, material, sheet.
    SetPath(built.texture, in.texturePath);
    Publish(built.texture);
    SetPath(built.material, in.materialPath);
    Publish(built.material);
    SetPath(built.sheet, in.sheetPath);
    Publish(built.sheet);

    out.sheet = built.sheet;
    out.material = built.material;
    out.texture = built.texture;
    out.sheetWidth = spec.sheetWidth;
    out.sheetHeight = spec.sheetHeight;
    return true;
}

}  // namespace Resources
}  // namespace Ext
