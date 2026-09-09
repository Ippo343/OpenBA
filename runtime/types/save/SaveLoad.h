#pragma once
// ============================================================================
// runtime/types/save/SaveLoad.h — Save::SaveLoad / Save::SavedState layouts
//                                 (Wave 5, lane C)
// ============================================================================
//
// Every offset below was read out of the SHIPPED Runtime-x64.dll in Ghidra
// (image base 0x180000000, decompiled and disassembled 2026-09-06). The
// function that pins each field is named next to it. Nothing here was taken
// from the profile build: PLAN §6 step 1 says profile NAMES win but profile
// OFFSETS must not, and the two Save classes are small enough that every
// displacement came out of retail bytes directly.
//
// The recovered source tree (docs/runtime_port/original_source_tree.txt) has
// exactly two Save files, `Save\SaveLoad.cpp` and `Save\SavedState.cpp`, which
// is why both classes share this one header.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtSave {

// ============================================================================
// Save::SaveLoad::Operation — the enum SetOperation/GetOperation move.
// ----------------------------------------------------------------------------
// Values read out of ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
// (0x1800c2e40), whose whole body is a dense switch on *(int*)this:
//   1 -> UpdateList  2 -> UpdateLoad  3 -> UpdateLoadFromFile
//   4 -> UpdateSave  5 -> UpdateDelete
// 0 is "no operation" (the ctor's initial value). The NAMES are OURS, inferred
// from the UpdateXxx export each value dispatches to; the DLL exports no
// enumerator symbols and the profile build has no string for them.
// ============================================================================
enum Operation : int32_t {
    kOpNone         = 0,
    kOpList         = 1,
    kOpLoad         = 2,
    kOpLoadFromFile = 3,
    kOpSave         = 4,
    kOpDelete       = 5,
};

// ============================================================================
// UI::Sprite — 0x10 bytes, embedded by value in SaveLoad and in SaveInfo.
// ----------------------------------------------------------------------------
// NOT this lane's class (UI is Wave 7); modelled here only because SaveLoad
// stores one at +0x60 and SaveInfo one at +0x00, and a lane that copies 16
// opaque bytes cannot static_assert anything.
//
// Pinned by ?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z (0x1800c3b90),
// which is four instructions moving [RDX] -> [RCX+0x60] and [RDX+8] ->
// [RCX+0x68], and by ??0SaveLoad (0x1800c3d30), which writes qword 0 at +0x60
// and dword 0xffffffff at +0x68. So the second qword's low dword is a signed
// index whose empty value is -1 — exactly what this project's own note
// reference_unresolved_lookup_leaves_a_valid_looking_zero.md records (index 0
// is a real sprite; empty is -1). The trailing dword is written by neither.
// ============================================================================
struct Sprite {
    void*   sheet;     // +0x00  UI::SpriteSheet* (an ExternalPtr's raw slot)
    int32_t index;     // +0x08  -1 == empty
    int32_t pad0c;     // +0x0c  never written by SetSaveSprite or the ctor
};
static_assert(offsetof(Sprite, index) == 0x08, "Sprite.index @ 0x08");
static_assert(sizeof(Sprite) == 0x10, "sizeof(UI::Sprite) == 0x10");

// ============================================================================
// Save::SaveLoad::ModInfo — 0x20 bytes, one per mod recorded in a save file.
// ----------------------------------------------------------------------------
// The element type of the array SaveInfo carries; the NAME comes from the
// mangled signature of ?LoadMetaData@SaveLoad@Save@@…, which spells the
// parameter `Collection::Array<Save::SaveLoad::ModInfo>` in full.
//
// The stride 0x20 and four fields are pinned by
// ?CheckMod@SaveLoad@Save@@QEBAHHH@Z (0x1800c1090), disassembled:
//   RBP = modIndex*0x20 + saveInfo->_mods._data       (SHL RBP,0x5)
//   R8  = RBP + 8            <- the operand handed to System::operator+
//   ... compares Modification entry +0x2c against [RBP+0x10]  -> status 4
//   ... compares Modification entry +0x1c against [RBP+0x18]  -> status 3
//   ... returns 2 when [RBP+0x14] < 0x1000400
//
// *** A CORRECTION THIS LANE MAKES *** `code/Game/Dialog/saveloaddialog.cpp`
// (1119, 1131) reads the mod name at ModInfo+0x00 and its comment describes
// +0x00 as "String at stride 0x20". True, but incomplete in a way that
// matters: CheckMod does NOT compare +0x00. It concatenates the data directory
// with the String at **+0x08** and matches THAT against IO::Modification's
// per-entry path String at +0x58. +0x00 is the display name ("Debug Menu");
// +0x08 is the package path relative to the data directory, and only the
// second one decides CheckMod's answer.
// ============================================================================
struct ModInfo {
    const char* name;           // +0x00  System::String — display name
    const char* path;           // +0x08  System::String — path, relative to the data dir
    int32_t     versionMajor;   // +0x10  vs IO::Modification entry +0x2c -> status 4
    uint32_t    gameVersion;    // +0x14  < 0x1000400 -> status 2 (modsOutOfDate)
    int32_t     versionMinor;   // +0x18  vs IO::Modification entry +0x1c -> status 3
    uint32_t    pad1c;          // +0x1c  not read by CheckMod
};
static_assert(offsetof(ModInfo, path)         == 0x08, "ModInfo.path @ 0x08");
static_assert(offsetof(ModInfo, versionMajor) == 0x10, "ModInfo.versionMajor @ 0x10");
static_assert(offsetof(ModInfo, gameVersion)  == 0x14, "ModInfo.gameVersion @ 0x14");
static_assert(offsetof(ModInfo, versionMinor) == 0x18, "ModInfo.versionMinor @ 0x18");
static_assert(sizeof(ModInfo) == 0x20, "sizeof(Save::SaveLoad::ModInfo) == 0x20");

// ============================================================================
// Save::SaveLoad::SaveInfo — 0x58 bytes, one per save file on disk.
// ----------------------------------------------------------------------------
// STRIDE 0x58 is pinned three ways: ?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z
// (0x1800c3b60) is `return this->_saves._data + index * 0x58`; CheckMod's
// prologue does the same IMUL R9,R9,0x58; and ~SaveLoad's array destructor
// (FUN_1800329c0) walks the buffer in 0x58 steps.
//
// THREE FIELDS ARE PINNED, THE REST IS NOT, and this header says so rather
// than inventing names:
//   +0x00  Sprite    thumbnail. `code/Game/Dialog/saveloaddialog.cpp:253/258`
//                    calls Sprite::IsValid(saveInfo) and memcpy's 16 bytes
//                    from saveInfo+0, i.e. the Sprite is the FIRST member.
//   +0x38  String    display name. `standalone/autostart.cpp:411` strcmp's
//                    *(const char**)(info+0x38) against the requested save
//                    name and picks the matching row; that harness works.
//   +0x48  Array<ModInfo> {_data, _size:i32, _capacity:i32}. _data pinned by
//                    CheckMod ([R9+RAX+0x48]); _size by saveloaddialog.cpp:1088
//                    reading the mod count at +0x50.
//
// WAVE 13 CLOSES MOST OF THAT HOLE. The note here used to end: "+0x10..0x37
// and +0x40..0x47 are NOT modelled. They are written by ?UpdateList
// (0x1800c1ea0) and ?LoadMetaData (0x1800c1b30), neither of which this lane
// ports or needed to read." Wave 13 ports UpdateList and OnVideoRestart, so
// five of those bytes are now pinned BY A READER, which is a stronger pin than
// a writer gives — a writer only shows the width, a reader shows the use:
//
//   +0x10  String   the description/metadata text. ?UpdateList state 4
//                   (0x1800c2455) refcount-assigns LoadMetaData's out-param
//                   into it, and the failure arm assigns the empty string.
//   +0x18  ImageBuffer*  ?OnVideoRestart (0x1800c0f44) loads it and calls
//                   ?Recreate@ImageBuffer@Video@@ on it.
//   +0x20  int32    thumbnail width  } OnVideoRestart passes them as the 3rd
//   +0x24  int32    thumbnail height } and 4th arguments of the upload call
//                   (MOV R8D,[+0x20]; MOV R9D,[+0x24] at 0x1800c0f92/0f8d).
//   +0x28  void*    the thumbnail pixel block (MOV RDX,[+0x28]).
//   +0x40  int32    the "metadata loaded" sentinel. NEGATIVE means not loaded:
//                   OnVideoRestart skips such a row (JL at 0x1800c0f53),
//                   UpdateList state 2 scans FORWARD past them looking for the
//                   next row to load, and UpdateSave treats a negative one as
//                   "this slot has no name yet" and generates one.
//   +0x44  int32    written by UpdateList state 4 from LoadMetaData's sixth
//                   out-param, and zeroed on the failure arm.
//
// +0x2c..+0x37 remain unmodelled: no body this project has ported either reads
// or writes them. Naming them from a guess is exactly the kind of comment
// PLAN §6 step 8 exists to prevent.
// ============================================================================
struct SaveInfo {
    Sprite      thumbnail;       // +0x00
    const char* description;     // +0x10  System::String  (UpdateList state 4)
    void*       imageBuffer;     // +0x18  Video::ImageBuffer*  (OnVideoRestart)
    int32_t     thumbWidth;      // +0x20  upload arg 3
    int32_t     thumbHeight;     // +0x24  upload arg 4
    void*       thumbPixels;     // +0x28  upload arg 2
    uint8_t     unknown30[0x08]; // +0x30 .. +0x37  NOT modelled — no ported body touches it
    const char* name;            // +0x38  System::String
    int32_t     metaLoaded;      // +0x40  < 0 == metadata not loaded yet
    int32_t     field44;         // +0x44  LoadMetaData out-param 6
    ModInfo*    modsData;        // +0x48  Collection::Array<ModInfo>::_data
    int32_t     modsSize;        // +0x50  ::_size
    int32_t     modsCapacity;    // +0x54  ::_capacity
};
static_assert(offsetof(SaveInfo, description) == 0x10, "SaveInfo.description @ 0x10");
static_assert(offsetof(SaveInfo, imageBuffer) == 0x18, "SaveInfo.imageBuffer @ 0x18");
static_assert(offsetof(SaveInfo, thumbWidth)  == 0x20, "SaveInfo.thumbWidth @ 0x20");
static_assert(offsetof(SaveInfo, thumbHeight) == 0x24, "SaveInfo.thumbHeight @ 0x24");
static_assert(offsetof(SaveInfo, thumbPixels) == 0x28, "SaveInfo.thumbPixels @ 0x28");
static_assert(offsetof(SaveInfo, metaLoaded)  == 0x40, "SaveInfo.metaLoaded @ 0x40");
static_assert(offsetof(SaveInfo, field44)     == 0x44, "SaveInfo.field44 @ 0x44");
static_assert(offsetof(SaveInfo, name)         == 0x38, "SaveInfo.name @ 0x38");
static_assert(offsetof(SaveInfo, modsData)     == 0x48, "SaveInfo.modsData @ 0x48");
static_assert(offsetof(SaveInfo, modsSize)     == 0x50, "SaveInfo.modsSize @ 0x50");
static_assert(offsetof(SaveInfo, modsCapacity) == 0x54, "SaveInfo.modsCapacity @ 0x54");
static_assert(sizeof(SaveInfo) == 0x58, "sizeof(Save::SaveLoad::SaveInfo) == 0x58");

// ============================================================================
// Save::SavedState — 0x10 bytes. A detached copy of a MemoryStream's bytes.
// ----------------------------------------------------------------------------
// +0x00  uint32 _id     ?SetId@SavedState@Save@@QEAAXI@Z (0x1800c0e90) and the
//                       one-argument ctor ??0SavedState@Save@@QEAA@I@Z
//                       (0x1800c0ec0) are the only writers. This lane ports
//                       neither; the field has to exist for the layout.
// +0x04  uint32 _size   ?Store@ (0x1800c0da0) writes it from the stream's
//                       GetCurrentPos; ?Retrieve@ (0x1800c0e50) reads it as
//                       both the Open() length and the memcpy count.
// +0x08  void*  _data   the owned copy. Freed by ~SavedState (0x1800c0ea0) and
//                       by Store before it re-allocates.
//
// SIZE: ??0SavedState@Save@@QEAA@XZ (0x1800c0ee0) is exactly two qword zero
// stores, at +0x00 and +0x08, and nothing in the class touches +0x10 or beyond.
// ============================================================================
struct SavedState {
    uint32_t _id;      // +0x00
    uint32_t _size;    // +0x04
    void*    _data;    // +0x08
};
static_assert(offsetof(SavedState, _size) == 0x04, "SavedState._size @ 0x04");
static_assert(offsetof(SavedState, _data) == 0x08, "SavedState._data @ 0x08");
static_assert(sizeof(SavedState) == 0x10, "sizeof(Save::SavedState) == 0x10");

// ============================================================================
// Save::SaveLoad — 0x90 bytes.
// ----------------------------------------------------------------------------
// SIZE. ??0SaveLoad@Save@@QEAA@XZ (0x1800c3d30) writes its last qword at +0x88,
// and ?Draw@SaveLoad (0x1800c2eb0) writes the capture buffer pointer at +0x88
// and reads nothing beyond +0x8b. Independently, this project's own
// `standalone/menudialogloopt_stubs.cpp:265` MemAllocs **0x90** for the menu's
// SaveLoad before calling the retail ctor, and that object has been live for
// months. 0x90 it is.
//
// NOT A POLYMORPHIC CLASS. The ctor writes an int at +0x00, not a vtable
// pointer, and the shipped DLL exports no ??_7SaveLoad@Save@@6B@ (checked
// against the export table: 147 ??_7 symbols, none of them Save's). So the
// vtable-ownership hazard of PLAN §4.3 risk 3 simply does not apply here.
//
// FIELDS THE CTOR DOES *NOT* INITIALISE, listed because a reader will assume
// it does: +0x18, +0x3c, +0x40 (all 8 bytes of it), +0x6c, +0x74, +0x78. Read straight off
// the disassembly — the ctor's stores are 0x00, 0x08(q), 0x10(q), 0x20(q),
// 0x28(q), 0x30(q), 0x38(d), 0x48(d), 0x50(q), 0x58(q), 0x60(q), 0x68(d),
// 0x70(q), 0x7c(b), 0x80(q), 0x88(q). In particular **_saveState (+0x40) is
// left as whatever the allocator handed back**, so ?GetSaveState@SaveLoad
// returns garbage until an operation sets it. That is retail behaviour and
// this port reproduces it exactly.
// ============================================================================
struct SaveLoad {
    Operation   _operation;        // +0x00  GetOperation / SetOperation
    uint32_t    pad04;             // +0x04
    int64_t     _elapsed;          // +0x08  Time ticks; Update adds the frame delta, SetOperation zeroes it
    int64_t     _minOperationTime; // +0x10  Time; SetMinOperationTime copies 8 bytes here
    uint32_t    _progress;         // +0x18  zeroed by SetOperation; NOT zeroed by the ctor
    uint32_t    pad1c;             // +0x1c
    const char* _string20;         // +0x20  System::String; ~SaveLoad releases it with the refcount idiom
    // WAVE 13 CORRECTION. These ten bytes used to be modelled as `uint64_t
    // field28` plus `void* _buffer30`, which is the right WIDTH and the wrong
    // THING. They are an entire Save::SavedState embedded by value, and three
    // independent readings say so:
    //
    //   * ?UpdateList@SaveLoad state 3 (0x1800c2455) calls
    //     ?SetupLoad@SavedState@Save@@ with `this + 0x28` as the SavedState.
    //   * ?UpdateSave@SaveLoad (0x1800c2c60) builds SaveBlock entry 0 out of
    //     it with exactly the three loads it uses for the HEAP SavedState at
    //     +0x40 — qword at +0x30, dword at +0x2c, dword at +0x28 — which lines
    //     up field for field with SavedState{_id@+0, _size@+4, _data@+8}.
    //   * ~SaveLoad frees +0x30 through the same locked MemFree that
    //     ~SavedState uses on its own _data.
    //
    // The old names were not WRONG about any byte; they just could not express
    // that the save path serialises into TWO SavedStates, one owned inline and
    // one on the heap, and that the pair is what a SaveBlock array is built
    // from. `field28` in particular read as padding.
    SavedState  _embeddedState;    // +0x28 .. +0x37
    uint32_t    field38;           // +0x38
    uint32_t    pad3c;             // +0x3c  NOT written by the ctor
    void*       _saveState;        // +0x40  SavedState*; GetSaveState. NOT written by the ctor
    int32_t     _selectedIndex;    // +0x48  SetOperation's second argument; ctor -1
    uint32_t    pad4c;             // +0x4c
    SaveInfo*   _savesData;        // +0x50  Collection::Array<SaveInfo>::_data
    int32_t     _savesSize;        // +0x58  ::_size — GetSaveCount returns exactly this
    int32_t     _savesCapacity;    // +0x5c  ::_capacity
    Sprite      _saveSprite;       // +0x60  SetSaveSprite; ctor {0, -1}
    const char* _saveName;         // +0x70  System::String; SetSaveName refcount-assigns it
    int32_t     _saveNameMode;     // +0x78  SetSaveName's second argument
    uint32_t    _captureFlags;     // +0x7c  ONLY the low byte is written (ctor 0; Draw tests it, then clears it)
    int32_t     _captureWidth;     // +0x80  Draw reads it as the capture width
    int32_t     _captureHeight;    // +0x84  Draw reads it as the capture height
    void*       _capturePixels;    // +0x88  Draw MemAllocs w*h*4 and stores it here
};
static_assert(offsetof(SaveLoad, _elapsed)          == 0x08, "SaveLoad._elapsed @ 0x08");
static_assert(offsetof(SaveLoad, _minOperationTime) == 0x10, "SaveLoad._minOperationTime @ 0x10");
static_assert(offsetof(SaveLoad, _progress)         == 0x18, "SaveLoad._progress @ 0x18");
static_assert(offsetof(SaveLoad, _string20)         == 0x20, "SaveLoad._string20 @ 0x20");
static_assert(offsetof(SaveLoad, _embeddedState)    == 0x28, "SaveLoad._embeddedState @ 0x28");
static_assert(offsetof(SaveLoad, _embeddedState._data) == 0x30,
              "the embedded SavedState puts its _data at SaveLoad+0x30 — the old _buffer30");
static_assert(offsetof(SaveLoad, _saveState)        == 0x40, "SaveLoad._saveState @ 0x40");
static_assert(offsetof(SaveLoad, _selectedIndex)    == 0x48, "SaveLoad._selectedIndex @ 0x48");
static_assert(offsetof(SaveLoad, _savesData)        == 0x50, "SaveLoad._savesData @ 0x50");
static_assert(offsetof(SaveLoad, _savesSize)        == 0x58, "SaveLoad._savesSize @ 0x58");
static_assert(offsetof(SaveLoad, _saveSprite)       == 0x60, "SaveLoad._saveSprite @ 0x60");
static_assert(offsetof(SaveLoad, _saveName)         == 0x70, "SaveLoad._saveName @ 0x70");
static_assert(offsetof(SaveLoad, _saveNameMode)     == 0x78, "SaveLoad._saveNameMode @ 0x78");
static_assert(offsetof(SaveLoad, _captureFlags)     == 0x7c, "SaveLoad._captureFlags @ 0x7c");
static_assert(offsetof(SaveLoad, _captureWidth)     == 0x80, "SaveLoad._captureWidth @ 0x80");
static_assert(offsetof(SaveLoad, _capturePixels)    == 0x88, "SaveLoad._capturePixels @ 0x88");
static_assert(sizeof(SaveLoad) == 0x90, "sizeof(Save::SaveLoad) == 0x90");

}  // namespace RtSave
