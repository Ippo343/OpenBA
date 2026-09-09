#pragma once
// UI::SpriteSheet -- binary-compatible struct definition.
// Runtime-x64.dll class, 0x58 bytes.
//
// Inheritance: Object::Base -> Object::External -> UI::SpriteSheet
//
// Decompiled from (Wave 7 lane F, 2026-09-07, Runtime-x64.dll, image base
// 0x180000000):
//   Default ctor:            0x18011d600 (??0SpriteSheet@UI@@QEAA@XZ)
//   Copy ctor:               0x1800035d0 (??0SpriteSheet@UI@@QEAA@AEBV01@@Z)  -- not read this wave
//   Dtor:                    0x18011d670 (??1SpriteSheet@UI@@UEAA@XZ)         -- not read this wave
//   GetSpriteCount:          0x180003190 (?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ)
//   GetSpriteIndexFromName:  0x1800031f0 (?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z)
//   GetUVs:                  0x180003330 (?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z)
//   AddSpriteInfo:           0x180003390 (?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z)
//   SetSpriteInfo:           0x180003460 (?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z)
//   GetSpriteNameFromIndex:  0x1800032d0 (?GetSpriteNameFromIndex@SpriteSheet@UI@@QEBA?AVString@System@@H@Z)  -- not read this wave
//
// VTABLE: `??_7SpriteSheet@UI@@6B@` IS EXPORTED (retail 0x180154590, per
// PLAN.md §2.2 item D / §4.3 risk 3 -- "147 ??_7 symbols are exported", of
// which SpriteSheet is one of the two the PLAN calls out by name). The ctor
// below stamps this symbol; a port does not need to mint or clone a vtable.
//
// *** CORRECTION (Wave 7 lane F, 2026-09-07): the SpriteInfo layout below was
// WRONG in two ways in the version this replaces. It said width/height sat at
// +0x18/+0x1C and that +0x20..+0x2C were "UV coords". Both claims came from
// guessing a conventional struct shape rather than reading a write. The real
// offsets, recovered from three independent bodies that all agree (evidence
// table below `SpriteInfo`): width is +0x1C, height is +0x20, the four UVs
// are +0x24/+0x28/+0x2C/+0x30, and there is a previously-unnoticed always-zero
// int32 at +0x18 that no read site in our call surface ever touches. Kept
// here as a comment, per PLAN §6 step 8, so the old (wrong) claim is
// recognisable if it is met again elsewhere (it is not met anywhere else in
// the tree today -- this header was the only place it was written down).
//
// Key behavior:
//   GetSpriteIndexFromName does a LINEAR SCAN over the sprite array comparing
//   System::String contents byte-by-byte (case-sensitive). Returns -1 on failure.
//   Each sprite entry is 0x38 bytes. The name string is at offset 0x00 of each entry.

#include <cstdint>
#include <cstddef>

// ============================================================
// UI::SpriteSheet::SpriteInfo (0x38 bytes per entry)
// Stored in a dynamic array at SpriteSheet::m_sprites (+0x20) -- a
// Collection::Array<SpriteInfo> (runtime/types/collection/Collection.h):
// {data, size, capacity}, growth via System::MemoryAllocator under its own
// critical section, all confirmed by FUN_180004e30 (SpriteSheet's array-grow
// body, byte-identical in shape to Collection.h's documented growth policy)
// called from AddSpriteInfo.
//
// EVIDENCE for +0x10 (x), +0x14 (y), +0x18 (reserved), +0x1C (width),
// +0x20 (height): AddSpriteInfo (0x180003390) builds a 0x38-byte temporary on
// its stack before handing it to the array-grow helper. Reading the temporary
// field by field (`local_30`/`iStack_2c` is one CONCAT44 write, `local_28` is
// `(uint)param_5 << 0x20`, `local_20` is `(ulonglong)(uint)param_6`):
//     temp+0x10 = param_3   (low dword of the CONCAT44 -> +0x10, not +0x18)
//     temp+0x14 = param_4   (high dword of the CONCAT44)
//     temp+0x18 = 0         (low dword of the "<<0x20" qword -- ALWAYS ZERO)
//     temp+0x1C = param_5   (high dword of the "<<0x20" qword)
//     temp+0x20 = param_6   (low dword of the plain zero-extend)
//     temp+0x24 = 0         (high dword of the plain zero-extend)
// AddSpriteInfo's mangled name is `(String,String,int,int,int,int)`, so in
// call order that is `AddSpriteInfo(name, group, x=param_3, y=param_4,
// width=param_5, height=param_6)`.
//
// INDEPENDENT CONFIRMATION for width/height, and the discriminator against
// the old +0x18/+0x1C guess: `Sprite::Width` (0x180030bf0) reads
// `entry + 0x1C` and `Sprite::Height` (0x180030c10) reads `entry + 0x20` --
// see Sprite.h. Both are one-line accessors with no other field in play, so
// there is no ambiguity about which field each name belongs to.
//
// EVIDENCE for +0x24/+0x28/+0x2C/+0x30 (u0, v0, u1, v1): `SetSpriteInfo`
// (0x180003460, mangled `(int,int,int,float,float,float,float)`) writes
// param_4..param_7 to exactly those four offsets, and `GetUVs` (0x180003330,
// mangled `(int, float&, float&, float&, float&)`) reads them back in the
// same order into its four out-parameters. SetSpriteInfo also writes
// param_2/param_3 (width/height) to +0x1C/+0x20, confirming those offsets a
// second, independent way.
// ============================================================
struct SpriteInfo {
    char*       name;           // +0x00  System::String data ptr (refcounted, NULL = empty)
    char*       group;          // +0x08  System::String data ptr (refcounted, NULL = empty)
    int32_t     x;              // +0x10  x position in sprite atlas
    int32_t     y;              // +0x14  y position in sprite atlas
    int32_t     _reserved18;    // +0x18  ALWAYS ZERO from AddSpriteInfo; no read site
                                //        in our call surface ever touches it. Not
                                //        "width" -- that claim was this header's bug.
    int32_t     width;          // +0x1C  CORRECTED (was +0x18). Sprite::Width, AddSpriteInfo, SetSpriteInfo.
    int32_t     height;         // +0x20  CORRECTED (was +0x1C). Sprite::Height, AddSpriteInfo, SetSpriteInfo.
    float       u0;             // +0x24  CORRECTED (was "field20", guessed at +0x20). SetSpriteInfo/GetUVs.
    float       v0;             // +0x28  CORRECTED (was "field24"). SetSpriteInfo/GetUVs.
    float       u1;             // +0x2C  CORRECTED (was "field28"). SetSpriteInfo/GetUVs.
    float       v1;             // +0x30  CORRECTED (was "field2C"). SetSpriteInfo/GetUVs.
    int32_t     _pad34;         // +0x34  CORRECTED (Wave 7 integration, 2026-09-07): NEVER
                                //        WRITTEN, by either implementation. The earlier
                                //        comment here said AddSpriteInfo "zeroes it" -- wrong,
                                //        and the exact same class of mistake this field's own
                                //        offset correction (above) already fixed once. Proven
                                //        by the DISASSEMBLY (not just the decompile) of the
                                //        element copy-construct helper both AddSpriteInfo and
                                //        this port's internal-helper call go through
                                //        (FUN_180004220, retail 0x180004220): its last store is
                                //        `MOV dword ptr [RCX+0x30],EAX` (copying `v1`) then RET
                                //        -- +0x34..+0x37 is never touched, on the source OR the
                                //        destination side, by any caller. A `newEntry._pad34 = 0`
                                //        in the port's C++ has no effect on the shipped entry,
                                //        because the helper never reads or copies that field.
                                //        See kSpriteInfoWrittenBytes below and
                                //        docs/runtime_port/WAVE7_LANE_F.md §9.
};

// Bytes a diff oracle may compare between two SpriteInfo entries -- see the
// _pad34 correction above. Comparing the full 0x38 bytes of two entries that
// live in UNRELATED allocations (this port's real array vs. a diff oracle's
// scratch replica) reports a divergence on whatever garbage each allocation
// happened to contain at +0x34, on every single call -- exactly the trap
// reference_oracle_must_not_compare_unwritten_bytes.md describes, and exactly
// what caught this (Wave 7 integration run, 100% divergence on
// UI::SpriteSheet::AddSpriteInfo() until this constant was introduced).
inline constexpr size_t kSpriteInfoWrittenBytes = 0x34;

static_assert(sizeof(SpriteInfo) == 0x38, "SpriteInfo must be 0x38 bytes");
static_assert(offsetof(SpriteInfo, name) == 0x00, "name at +0x00");
static_assert(offsetof(SpriteInfo, group) == 0x08, "group at +0x08");
static_assert(offsetof(SpriteInfo, x) == 0x10, "x at +0x10");
static_assert(offsetof(SpriteInfo, y) == 0x14, "y at +0x14");
static_assert(offsetof(SpriteInfo, _reserved18) == 0x18, "_reserved18 at +0x18");
static_assert(offsetof(SpriteInfo, width) == 0x1C, "width at +0x1C");
static_assert(offsetof(SpriteInfo, height) == 0x20, "height at +0x20");
static_assert(offsetof(SpriteInfo, u0) == 0x24, "u0 at +0x24");
static_assert(offsetof(SpriteInfo, v0) == 0x28, "v0 at +0x28");
static_assert(offsetof(SpriteInfo, u1) == 0x2C, "u1 at +0x2C");
static_assert(offsetof(SpriteInfo, v1) == 0x30, "v1 at +0x30");

// ============================================================
// UI::SpriteSheet (0x58 bytes)
// ============================================================
//
// GetSpriteIndexFromName algorithm (0x1800031f0), identical in shape to
// StringTable::GetIndexFromName (see StringTable.h) except for the 0x38 vs
// 0x10 entry stride:
//   Linear scan: for i = 0..m_spriteCount-1:
//     entry = m_sprites[i]  (stride 0x38, name at entry+0x00)
//     Compare lengths first (length at charPtr[-8], RtSystem::StringLength)
//     If lengths equal, compare chars byte-by-byte (case-sensitive)
//     If match, return i
//   Return -1 on not found
//
// GetSpriteNameFromIndex (0x1800032d0, NOT in Wave 7 lane F's scope):
//   Returns m_sprites[index].name as System::String (with refcount bump)
//   No bounds checking observed
//
// Object::External's own three fields (vtable, filename, +0x10, +0x18) are
// NOT re-derived this wave; they are carried over from the pre-existing
// header, which cites SpriteSheet's ctor for them and that citation still
// holds (the ctor zeroes +0x08/+0x10/+0x18 before stamping SpriteSheet's own
// vtable, exactly as this file's ctor evidence below shows).
struct SpriteSheet {
    // --- Object::External base (0x08 bytes visible) ---
    void*       vtable;             // +0x00  ??_7SpriteSheet@UI@@6B@ (0x180154590, EXPORTED)
    char*       m_filename;         // +0x08  System::Filename data ptr (sheet resource path)

    // --- External/Container fields ---
    void*       m_field10;          // +0x10  container/resource ptr (zeroed, then copied via FUN_180006890)
    void*       m_field18;          // +0x18  container/resource ptr (part of +0x10 pair)

    // --- SpriteSheet-specific fields ---
    SpriteInfo* m_sprites;          // +0x20  Collection::Array<SpriteInfo>._data
    int32_t     m_spriteCount;      // +0x28  Collection::Array<SpriteInfo>._size
    int32_t     m_spriteCapacity;   // +0x2C  Collection::Array<SpriteInfo>._capacity

    uint64_t    m_field30;          // +0x30  Object::ExternalPtr<Graphics::MaterialInstance>
                                    //        IDENTIFIED Wave 10B lane SS: Serialize
                                    //        (0x18011d730) hands &this+0x30 to
                                    //        `CALL 0x180014520`, which is one of the
                                    //        twenty-five ExternalPtr<T>::Serialize
                                    //        instantiations lane EXTPTR measured and
                                    //        tabulated — WAVE10_LANE_EXTPTR.md:79 names
                                    //        that copy's type id as
                                    //        Graphics::MaterialInstance. So this is the
                                    //        sheet's material handle, not an anonymous
                                    //        qword. Left as uint64_t rather than renamed:
                                    //        nothing in the tree reads it yet, and a
                                    //        rename would be a change with no evidence
                                    //        behind its NAME beyond the type id.

    char*       m_textureName;      // +0x38  System::String data ptr (refcounted, texture name?)
    char*       m_groupName;        // +0x40  System::String data ptr (refcounted, group/atlas name?)

    int32_t     m_field48;          // +0x48  unknown int
    int32_t     m_field4C;          // +0x4C  unknown int

    uint8_t     m_field50;          // +0x50  unknown byte (single byte copy in copy ctor)
    uint8_t     _pad51[7];          // +0x51  padding to 8-byte alignment (0x58 total).
                                    //        CONFIRMED never written by the ctor (Wave 7
                                    //        integration, 2026-09-07) -- its disassembly ends
                                    //        `MOV byte ptr [RCX+0x50],DL` / `RET`, retail
                                    //        0x18011d65d. See kSpriteSheetWrittenBytes below.
};

// Bytes a diff oracle may compare between two freshly-constructed SpriteSheet
// objects. The ctor writes through and including +0x50 (0x51 bytes total) and
// never touches +0x51..+0x57 -- comparing the full 0x58-byte sizeof() reports
// a divergence on whatever the allocator happened to leave there, which is
// unrelated between a real object and a diff oracle's scratch buffer. Same
// class of mistake as kSpriteInfoWrittenBytes above; caught the same way
// (Wave 7 integration run, 100% divergence on UI::SpriteSheet::SpriteSheet()
// until this constant was introduced -- theirs was uniformly 0xCD poison at
// +0x51, proving retail's ctor never writes there, while ours held real
// uninitialized heap bytes).
inline constexpr size_t kSpriteSheetWrittenBytes = 0x51;

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
