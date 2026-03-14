#pragma once
// UI::SpriteSheet -- binary-compatible struct definition.
// Runtime-x64.dll class, 0x58 bytes.
//
// Inheritance: Object::Base -> Object::External -> UI::SpriteSheet
//
// Decompiled from:
//   Default ctor:            0x18011d600 (??0SpriteSheet@UI@@QEAA@XZ)
//   Copy ctor:               0x1800035d0 (??0SpriteSheet@UI@@QEAA@AEBV01@@Z)
//   Dtor:                    0x18011d670 (??1SpriteSheet@UI@@UEAA@XZ)
//   GetSpriteCount:          0x180003190 (?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ)
//   GetSpriteIndexFromName:  0x1800031f0 (?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z)
//   GetSpriteNameFromIndex:  0x1800032d0 (?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z)
//
// Key behavior:
//   GetSpriteIndexFromName does a LINEAR SCAN over the sprite array comparing
//   System::String contents byte-by-byte (case-sensitive). Returns -1 on failure.
//   Each sprite entry is 0x38 bytes. The name string is at offset 0x00 of each entry.
//
// Sprite entry layout (0x38 bytes, from copy helper @ 0x180004220):
//   +0x00  System::String  name       (refcounted, char*)
//   +0x08  System::String  group      (refcounted, char*)
//   +0x10  int32_t         x          (sprite x in atlas)
//   +0x14  int32_t         y          (sprite y in atlas)
//   +0x18  int32_t         width      (sprite width)
//   +0x1C  int32_t         height     (sprite height)
//   +0x20  int32_t/float   u0/field5a (UV or additional coord)
//   +0x24  int32_t/float   v0/field5b
//   +0x28  int32_t/float   u1/field6a
//   +0x2C  int32_t/float   v1/field6b
//   +0x30  int32_t         field7a    (unknown)
//   +0x34  — padding to 0x38

#include <cstdint>
#include <cstddef>

// ============================================================
// UI::SpriteSheet::SpriteInfo (0x38 bytes per entry)
// Stored in a dynamic array at SpriteSheet::m_sprites (+0x20)
// Copy helper at 0x180004220, element stride 0x38
// ============================================================
struct SpriteInfo {
    char*       name;           // +0x00  System::String data ptr (refcounted, NULL = empty)
    char*       group;          // +0x08  System::String data ptr (refcounted, NULL = empty)
    int32_t     x;              // +0x10  x position in sprite atlas
    int32_t     y;              // +0x14  y position in sprite atlas
    int32_t     width;          // +0x18  sprite width in pixels
    int32_t     height;         // +0x1C  sprite height in pixels
    int32_t     field20;        // +0x20  UV coord or additional data
    int32_t     field24;        // +0x24  UV coord or additional data
    int32_t     field28;        // +0x28  UV coord or additional data
    int32_t     field2C;        // +0x2C  UV coord or additional data
    int32_t     field30;        // +0x30  unknown
    int32_t     _pad34;         // +0x34  padding to 0x38 alignment
};

static_assert(sizeof(SpriteInfo) == 0x38, "SpriteInfo must be 0x38 bytes");
static_assert(offsetof(SpriteInfo, name) == 0x00, "name at +0x00");
static_assert(offsetof(SpriteInfo, group) == 0x08, "group at +0x08");
static_assert(offsetof(SpriteInfo, x) == 0x10, "x at +0x10");
static_assert(offsetof(SpriteInfo, y) == 0x14, "y at +0x14");
static_assert(offsetof(SpriteInfo, width) == 0x18, "width at +0x18");
static_assert(offsetof(SpriteInfo, height) == 0x1C, "height at +0x1C");
static_assert(offsetof(SpriteInfo, field20) == 0x20, "field20 at +0x20");
static_assert(offsetof(SpriteInfo, field24) == 0x24, "field24 at +0x24");
static_assert(offsetof(SpriteInfo, field28) == 0x28, "field28 at +0x28");
static_assert(offsetof(SpriteInfo, field2C) == 0x2C, "field2C at +0x2C");
static_assert(offsetof(SpriteInfo, field30) == 0x30, "field30 at +0x30");

// ============================================================
// UI::SpriteSheet (0x58 bytes)
// ============================================================
//
// GetSpriteIndexFromName algorithm (0x1800031f0):
//   Linear scan: for i = 0..m_spriteCount-1:
//     entry = m_sprites[i]  (stride 0x38, name at entry+0x00)
//     Compare lengths first (length at charPtr[-8])
//     If lengths equal, compare chars byte-by-byte (case-sensitive)
//     If match, return i
//   Return -1 on not found
//
// GetSpriteNameFromIndex (0x1800032d0):
//   Returns m_sprites[index].name as System::String (with refcount bump)
//   No bounds checking observed
//
struct SpriteSheet {
    // --- Object::External base (0x08 bytes visible) ---
    void*       vtable;             // +0x00  vtable pointer (set through Base->External->SpriteSheet chain)
    char*       m_filename;         // +0x08  System::Filename data ptr (sheet resource path)

    // --- External/Container fields ---
    void*       m_field10;          // +0x10  container/resource ptr (zeroed, then copied via FUN_180006890)
    void*       m_field18;          // +0x18  container/resource ptr (part of +0x10 pair)

    // --- SpriteSheet-specific fields ---
    SpriteInfo* m_sprites;          // +0x20  dynamic array of SpriteInfo entries (0x38 bytes each)
    int32_t     m_spriteCount;      // +0x28  number of sprites in the array
    int32_t     m_spriteCapacity;   // +0x2C  allocated capacity of sprite array

    uint64_t    m_field30;          // +0x30  unknown (copied in copy ctor)

    char*       m_textureName;      // +0x38  System::String data ptr (refcounted, texture name?)
    char*       m_groupName;        // +0x40  System::String data ptr (refcounted, group/atlas name?)

    int32_t     m_field48;          // +0x48  unknown int
    int32_t     m_field4C;          // +0x4C  unknown int

    uint8_t     m_field50;          // +0x50  unknown byte (single byte copy in copy ctor)
    uint8_t     _pad51[7];          // +0x51  padding to 8-byte alignment (0x58 total)

    // --- Inline accessors matching original methods ---
    int GetSpriteCount() const { return m_spriteCount; }

    // Linear scan lookup. Returns index (0-based) or -1 if not found.
    // Case-sensitive, compares string lengths first then char-by-char.
    // Original: 0x1800031f0
    // NOTE: param is System::String const& — the char* is at *(char**)&stringObj
    //       Length is at charPtr[-8], refcount at charPtr[-4]
};

static_assert(sizeof(SpriteSheet) == 0x58, "SpriteSheet must be 0x58 bytes");
static_assert(offsetof(SpriteSheet, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(SpriteSheet, m_filename) == 0x08, "m_filename at +0x08");
static_assert(offsetof(SpriteSheet, m_field10) == 0x10, "m_field10 at +0x10");
static_assert(offsetof(SpriteSheet, m_field18) == 0x18, "m_field18 at +0x18");
static_assert(offsetof(SpriteSheet, m_sprites) == 0x20, "m_sprites at +0x20");
static_assert(offsetof(SpriteSheet, m_spriteCount) == 0x28, "m_spriteCount at +0x28");
static_assert(offsetof(SpriteSheet, m_spriteCapacity) == 0x2C, "m_spriteCapacity at +0x2C");
static_assert(offsetof(SpriteSheet, m_field30) == 0x30, "m_field30 at +0x30");
static_assert(offsetof(SpriteSheet, m_textureName) == 0x38, "m_textureName at +0x38");
static_assert(offsetof(SpriteSheet, m_groupName) == 0x40, "m_groupName at +0x40");
static_assert(offsetof(SpriteSheet, m_field48) == 0x48, "m_field48 at +0x48");
static_assert(offsetof(SpriteSheet, m_field4C) == 0x4C, "m_field4C at +0x4C");
static_assert(offsetof(SpriteSheet, m_field50) == 0x50, "m_field50 at +0x50");
