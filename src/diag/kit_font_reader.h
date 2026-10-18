#pragma once
// ============================================================================
// src/diag/kit_font_reader.h -- read a kit-compiled UI::Font (.crs) without the engine
// ============================================================================
// NOT RETAIL, diagnostics only (KITFREE RSCFONT): the cross-check oracle for
// the run-time rasteriser is the mod kit's own Font/Sharp<Kit>_<px>_resource.crs
// (tools/ext_resources.sh builds them under build/ext_resources/WinData/).
// This header decodes one on its own: the IO::CompressedStream container
// (tools/ext_font_check.py has the format: u32 blockCount, blockCount x {u32
// offset, u32 uncompressed << 16 | compressed}, LZF blocks) and the binary
// Font record from the face name on (UI::Font::Serialize's order, font.cpp:
// face, height, sheet name, sheet w/h, material name, image name, three
// external refs, the int32 codepoint table, the glyph rows, baseline,
// ellipsis and space widths). Header-only, standard C++, used by the unit
// test and the font_dump harness. NEVER commit a .crs (KITFREE.md §1).
// ============================================================================

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace KitFont {

struct GlyphRow { int32_t preAdvance, postKern, postAdvance, offsetY; };

struct Font {
    std::string face, sheetName, materialName, imageName;
    std::string charsetRef, sheetRef, materialRef;
    int32_t height = 0, sheetWidth = 0, sheetHeight = 0;
    std::vector<int32_t> charToGlyph;   // -1 = unmapped
    std::vector<GlyphRow> glyphs;
    int32_t baseLine = 0, ellipsisWidth = 0, spaceWidth = 0;
};

inline bool ReadFile(const char* path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    const int64_t n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    const bool ok = n >= 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

// LZF (System::Compression::Decompress's format): literal runs (c < 32: c+1
// bytes) and back-references (len = c >> 5 (+ next byte when 7) + 2, offset
// = ((c & 31) << 8 | next) + 1).
inline bool Lzf(const uint8_t* src, size_t n, size_t expect, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(expect);
    size_t i = 0;
    while (i < n) {
        const unsigned c = src[i++];
        if (c < 32) {
            if (i + c + 1 > n) return false;
            out.insert(out.end(), src + i, src + i + c + 1);
            i += c + 1;
        } else {
            unsigned len = c >> 5;
            if (len == 7) { if (i >= n) return false; len += src[i++]; }
            if (i >= n) return false;
            const size_t off = ((c & 31u) << 8 | src[i++]) + 1;
            if (off > out.size()) return false;
            for (unsigned k = 0; k < len + 2; ++k) out.push_back(out[out.size() - off]);
        }
    }
    return out.size() == expect;
}

inline bool DecodeCrs(const std::vector<uint8_t>& d, std::vector<uint8_t>& out) {
    out.clear();
    if (d.size() < 4) return false;
    uint32_t count; std::memcpy(&count, d.data(), 4);
    if (d.size() < 4 + 8ull * count) return false;
    std::vector<uint8_t> block;
    for (uint32_t b = 0; b < count; ++b) {
        uint32_t offset, packed;
        std::memcpy(&offset, d.data() + 4 + 8 * b, 4);
        std::memcpy(&packed, d.data() + 8 + 8 * b, 4);
        const size_t comp = packed & 0xffffu;
        size_t uncomp = packed >> 16;
        if (uncomp == 0) uncomp = 0x10000;
        if (offset + comp > d.size()) return false;
        if (comp == uncomp) out.insert(out.end(), d.begin() + offset, d.begin() + offset + comp);
        else {
            if (!Lzf(d.data() + offset, comp, uncomp, block)) return false;
            out.insert(out.end(), block.begin(), block.end());
        }
    }
    return true;
}

struct Cursor {
    const std::vector<uint8_t>& d; size_t p; bool ok = true;
    int32_t Int() { int32_t v = 0; if (p + 4 > d.size()) { ok = false; return 0; } std::memcpy(&v, d.data() + p, 4); p += 4; return v; }
    std::string Str() {
        uint32_t n = static_cast<uint32_t>(Int());
        if (!ok || n == 0) return std::string();
        n &= 0x7fffffffu;
        if (n == 0 || p + n > d.size()) { ok = false; return std::string(); }
        std::string s(reinterpret_cast<const char*>(d.data() + p), n - 1);
        p += n;
        return s;
    }
    std::string Ref() { if (p >= d.size()) { ok = false; return std::string(); } const bool has = d[p++] != 0; return has ? Str() : std::string(); }
};

// Reads the Font record from `blob` (a decoded .crs) whose face is `face`.
inline bool Parse(const std::vector<uint8_t>& blob, const char* face, Font& out) {
    const std::string needle = std::string(face) + '\0';
    const auto it = std::search(blob.begin(), blob.end(), needle.begin(), needle.end());
    if (it == blob.end() || it - blob.begin() < 4) return false;
    Cursor c{blob, static_cast<size_t>(it - blob.begin()) - 4};
    out.face = c.Str();
    out.height = c.Int();
    out.sheetName = c.Str();
    out.sheetWidth = c.Int();
    out.sheetHeight = c.Int();
    out.materialName = c.Str();
    out.imageName = c.Str();
    out.charsetRef = c.Ref();
    out.sheetRef = c.Ref();
    out.materialRef = c.Ref();
    const int32_t n = c.Int();
    if (!c.ok || n < 0 || n > 0x110000) return false;
    out.charToGlyph.resize(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) out.charToGlyph[static_cast<size_t>(i)] = c.Int();
    const int32_t g = c.Int();
    if (!c.ok || g < 0 || g > 100000) return false;
    out.glyphs.resize(static_cast<size_t>(g));
    for (int32_t i = 0; i < g; ++i) {
        GlyphRow& r = out.glyphs[static_cast<size_t>(i)];
        r.preAdvance = c.Int(); r.postKern = c.Int(); r.postAdvance = c.Int(); r.offsetY = c.Int();
    }
    out.baseLine = c.Int();
    out.ellipsisWidth = c.Int();
    out.spaceWidth = c.Int();
    return c.ok;
}

inline bool Load(const char* crsPath, const char* face, Font& out) {
    std::vector<uint8_t> raw, blob;
    return ReadFile(crsPath, raw) && DecodeCrs(raw, blob) && Parse(blob, face, out);
}

// The codepoint of glyph i (the lowest codepoint mapping to it), -1 when none.
inline std::vector<int32_t> GlyphCodepoints(const Font& f) {
    std::vector<int32_t> cps(f.glyphs.size(), -1);
    for (size_t cp = 0; cp < f.charToGlyph.size(); ++cp) {
        const int32_t g = f.charToGlyph[cp];
        if (g >= 0 && static_cast<size_t>(g) < cps.size() && cps[static_cast<size_t>(g)] < 0) cps[static_cast<size_t>(g)] = static_cast<int32_t>(cp);
    }
    return cps;
}

}  // namespace KitFont
