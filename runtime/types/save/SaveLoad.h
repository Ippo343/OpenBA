#pragma once
// ============================================================================
// runtime/types/save/SaveLoad.h — Save::SaveLoad, Save::SaveLoad::SaveInfo,
//                                 Save::SaveLoad::ModInfo, Save::SavedState
// ============================================================================
//
// ENDGAME B.3 step 7, block B2 (Save). The four REAL classes under retail's
// names. Until this block they were `RtSave::` PODs (Wave 5 lane C, Waves
// 13/14/24); every field, every offset and every `static_assert` of those PODs
// is kept below, the PODs BECOME the classes (B6-4.1, one definition per
// layout) and `RtSave::` keeps only aliases (bottom of this file) until the
// last runtime spelling of the old names is gone.
//
// Every offset below was read out of the SHIPPED Runtime-x64.dll in Ghidra
// (image base 0x180000000). The function that pins each field is named next to
// it. The recovered source tree has exactly two Save files, `Save\SaveLoad.cpp`
// and `Save\SavedState.cpp`, which is why the classes share this one header.
//
// SHAPES (B37_RULES 2.4, B7-8):
//   * `Save::SaveLoad`   — NON-POLYMORPHIC ROOT, heap-allocated by the exe
//     (`MemAlloc(0x90,4)` at 0x140097236 / 0x14009b9e0) and built through the
//     `??0SaveLoad` DOOR, on the LOADER thread (GameUpdateLoop's slot-6 Load).
//     Its ctor/dtor stay doors (`RtSave_SaveLoad_Ctor`/`_Dtor`, saveload.cpp),
//     exactly as `UI::UserInterface`'s do, and are NOT declared as C++ special
//     members. Retail's copy ctor (0x180030d50) and `operator=` (0x180030e50)
//     are DEEP (String refcount + the two Arrays): declared, not ported, so no
//     C++ caller can reach a shallow implicit copy. NOT trivially copyable.
//   * `Save::SaveLoad::SaveInfo` / `ModInfo` — VALUE TYPES held in
//     `Collection::Array<T>` (retail NESTS both in SaveLoad: `USaveInfo@12@`,
//     `UModInfo@SaveLoad@Save@@`). Real ctor / copy ctor / dtor, each the
//     transcription of retail's (unexported) body, named per member below.
//   * `Save::SavedState` — held BY VALUE in SaveLoad (+0x28) and heap-allocated
//     by UpdateLoad/UpdateSave/UpdateLoadFromFile, so it gets its real ctors
//     and dtor too; the registered `??0`/`??1` doors delegate to them.
//
// ARGUMENT PASSING (RULE B7-5, read from the retail prologues, B37_SLOTS 8):
//   * `SaveMetaData`'s `System::String` BY VALUE arrives by HIDDEN POINTER and
//     the CALLEE destroys it (0x1800c1ab7..0x1800c1b0b): `System::String*`.
//   * `SetSaveSprite`'s `UI::Sprite` BY VALUE arrives by HIDDEN POINTER
//     (`MOV RAX,[RDX]`): `const UI::Sprite*`.
//   * `Draw`'s `Object::ExternalPtr<Graphics::Material>` BY VALUE arrives as the
//     BARE POINTER in R8 — CLOSED HERE (B37_RULES 13.1): the prologue homes R8
//     at `[RSP0+0x18]` (0x1800c2eb3, i.e. `[RBP+0x2b0]`), and the one reader of
//     that home, 0x1800c3192 `MOV RAX,[RBP+0x2b0]` / 0x1800c3199
//     `MOV [RSI+0x28],RAX`, stores the qword WHOLE into the MaterialInstance.
//     No LEA of the home anywhere in the 724-instruction body and no release on
//     the way out: a register value the callee does not destroy. By value.
//   * `System::Time` arguments here are `const Time&` in retail's mangling.
//
// HIDDEN RETURN SLOTS, all NON-virtual (B37_RULES 2.5): `GenerateSaveName`,
// `GetAutoSaveName` (`System::String`), `SavedState::SetupLoad`/`GetSaveBlock`
// (`IO::Persistence::SaveBlock`). The members return by value; the registered
// doors keep retail's `(this, ret)` shape and placement-new the prvalue (B5-12).
//
// `SaveLoad::_singleton` (0x1801fc6f0) is NOT a class static here: it is read
// through `RT_DATA_SLOT` (saveload_internal.h) and promoted by
// standalone/reg_mint_promoted.cpp; the class-static conversion is the END of
// B.3 (carry-over 5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../collection/Collection.h"
#include "../ui/Sprite.h"   // UI::Sprite, held by value below

namespace System   { class String; class Time; }
namespace Video    { class Context; }
namespace IO       { class MemoryStream; }
namespace Graphics { class Material; }
namespace RtIo     { struct SaveBlock; }

// The hand-derived Array row for `Array<SaveInfo>` (runtime/src/collection/
// array.cpp; 0x1800332d0 — the three-argument SetSize whose assign is the
// SaveInfo copy ctor 0x180032f70 and whose destroy is ~SaveInfo 0x1800c3f40).
// Declared here so the member below can name its row.
namespace rtcoll {
extern const ArraySpec kSpec_MetaData58;
}

namespace Save {

// ############################################################################
// Save::SavedState — 0x10 bytes. A detached copy of a MemoryStream's bytes.
// ############################################################################
// +0x00  uint32 _id     ?SetId (0x1800c0e90) and the one-argument ctor
//                       (0x1800c0ec0) are the only writers.
// +0x04  uint32 _size   ?Store (0x1800c0da0) writes it from the stream's
//                       GetCurrentPos; ?Retrieve (0x1800c0e50) reads it as both
//                       the Open() length and the memcpy count.
// +0x08  void*  _data   the owned copy. Freed by ~SavedState (0x1800c0ea0), by
//                       Store before it re-allocates, and by Discard.
//
// SIZE: ??0SavedState@Save@@QEAA@XZ (0x1800c0ee0) is exactly two qword zero
// stores, at +0x00 and +0x08; the exe allocates it `MemAlloc(0x10,4)`.
// ############################################################################
class SavedState {
public:
    // ??0SavedState@Save@@QEAA@XZ  0x1800c0ee0 — two qword zero stores.
    SavedState() : _id(0), _size(0), _data(nullptr) {}
    // ??0SavedState@Save@@QEAA@I@Z 0x1800c0ec0 — `MOV [RCX],EDX ; MOV [RCX+4],0 ;
    // MOV [RCX+8],0`. Defined, not registered (no caller in the tree).
    explicit SavedState(uint32_t id) : _id(id), _size(0), _data(nullptr) {}
    // ??1SavedState@Save@@QEAA@XZ  0x1800c0ea0 — `if (_data) MemFree(_data)`;
    // _data is NOT nulled. saveload.cpp.
    ~SavedState();
    // Retail exports no copy constructor, and nothing copies one.
    SavedState(const SavedState&) = delete;
    // ??4SavedState@Save@@QEAAAEAV01@AEBV01@@Z 0x180030bd0 — two qword copies,
    // i.e. the SHALLOW member-wise assign (both objects then name one buffer;
    // retail's). Not registered (no caller).
    SavedState& operator=(const SavedState&) = default;

    // ?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ
    //   0x1800c0c60 — RETSLOT (the door keeps `(this, ret)`). savedstate_setupload.cpp
    RtIo::SaveBlock SetupLoad();
    // ?GetSaveBlock@SavedState@Save@@QEBA?AUSaveBlock@Persistence@IO@@XZ
    //   0x1800c0d50 — RETSLOT: {data = _data, size = _size, id = _id}. Defined,
    //   not registered (no caller; UpdateSave inlines the same three loads).
    RtIo::SaveBlock GetSaveBlock() const;
    // ?Discard@SavedState@Save@@QEAAXXZ 0x1800c0d70 — `if (_data) MemFree(_data);
    //   _size = 0; _data = 0`. Defined, not registered (no caller).
    void Discard();
    // ?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z     0x1800c0da0
    void Store(IO::MemoryStream& stream);
    // ?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z  0x1800c0e50
    void Retrieve(IO::MemoryStream& stream);
    // ?SetId@SavedState@Save@@QEAAXI@Z 0x1800c0e90 — `MOV [RCX],EDX ; RET`.
    //   Defined, not registered (no caller).
    void SetId(uint32_t id) { _id = id; }

    // ---- the 0x10 bytes (public: the UpdateXxx bodies build their heap
    //      SavedStates field by field, as retail inlines them) --------------
    uint32_t _id;      // +0x00
    uint32_t _size;    // +0x04
    void*    _data;    // +0x08
};
static_assert(offsetof(SavedState, _size) == 0x04, "SavedState._size @ 0x04");
static_assert(offsetof(SavedState, _data) == 0x08, "SavedState._data @ 0x08");
static_assert(sizeof(SavedState)  == 0x10, "sizeof(Save::SavedState) == 0x10");
static_assert(alignof(SavedState) == 8,    "Save::SavedState aligns to 8 (the buffer pointer)");
static_assert(!std::is_polymorphic<SavedState>::value, "no vtable: the ctor's first store is _id");

// ############################################################################
// Save::SaveLoad — 0x90 bytes.
// ############################################################################
// SIZE. ??0SaveLoad@Save@@QEAA@XZ (0x1800c3d30) writes its last qword at +0x88,
// ?Draw@SaveLoad (0x1800c2eb0) writes the capture buffer pointer at +0x88 and
// reads nothing beyond +0x8b, and the exe allocates `MemAlloc(0x90,4)` before
// calling the ctor (0x140097236, 0x14009b9e0). 0x90 it is.
//
// NOT A POLYMORPHIC CLASS. The ctor writes an int at +0x00, not a vtable
// pointer, and the shipped DLL exports no ??_7SaveLoad@Save@@6B@.
//
// FIELDS THE CTOR DOES *NOT* INITIALISE, listed because a reader will assume
// it does: +0x18, +0x3c, +0x40 (all 8 bytes of it), +0x6c, +0x74, +0x78. Read
// straight off the disassembly — the ctor's stores are 0x00, 0x08(q), 0x10(q),
// 0x20(q), 0x28(q), 0x30(q), 0x38(d), 0x48(d), 0x50(q), 0x58(q), 0x60(q),
// 0x68(d), 0x70(q), 0x7c(b), 0x80(q), 0x88(q). In particular **_saveState
// (+0x40) is left as whatever the allocator handed back**, so GetSaveState
// returns garbage until an operation sets it. That is retail behaviour.
// ############################################################################
class SaveLoad {
public:
    // ========================================================================
    // Save::SaveLoad::Operation — the enum SetOperation/GetOperation move
    // (`W4Operation@12@`, nested). Values read out of ?Update@SaveLoad
    // (0x1800c2e40), whose body is a dense switch on *(int*)this:
    //   1 -> UpdateList  2 -> UpdateLoad  3 -> UpdateLoadFromFile
    //   4 -> UpdateSave  5 -> UpdateDelete
    // 0 is "no operation" (the ctor's initial value). The NAMES are OURS,
    // inferred from the UpdateXxx export each value dispatches to.
    // ========================================================================
    enum Operation : int32_t {
        kOpNone         = 0,
        kOpList         = 1,
        kOpLoad         = 2,
        kOpLoadFromFile = 3,
        kOpSave         = 4,
        kOpDelete       = 5,
    };

    // ========================================================================
    // Save::SaveLoad::ModInfo — 0x20 bytes, one per mod recorded in a save file.
    // ------------------------------------------------------------------------
    // The NAME comes from the mangled signature of ?LoadMetaData@SaveLoad@Save@@…,
    // which spells the parameter `Collection::Array<Save::SaveLoad::ModInfo>`.
    //
    // THE 0x20 STRIDE, CLOSED HERE (B37_RULES 13.2) from three retail bodies:
    //   * ?CheckMod (0x1800c1090): `MOVSXD RBP,R8D ; SHL RBP,0x5 ;
    //     ADD RBP,[R9+RAX+0x48]` — modIndex*0x20 into SaveInfo+0x48's buffer;
    //   * the element destroy 0x1800341e0: `ADD RBX,0x20 ; DEC RBP ; JNZ` over
    //     [RBX+8] then [RBX] (two String releases per element);
    //   * the two-argument SetSize 0x180034010: `SHL RCX,0x5 ; MOV EDX,0x8 ;
    //     CALL MemAlloc` (capacity*0x20, align 8), copy loop `ADD RDX,0x20`.
    //
    // FIELDS, pinned by CheckMod:
    //   RBP = modIndex*0x20 + saveInfo->mods._data
    //   R8  = RBP + 8            <- the operand handed to System::operator+
    //   ... compares Modification entry +0x2c against [RBP+0x10]  -> status 4
    //   ... compares Modification entry +0x1c against [RBP+0x18]  -> status 3
    //   ... returns 2 when [RBP+0x14] < 0x1000400
    // +0x00 is the display name ("Debug Menu"); +0x08 is the package path
    // relative to the data directory, and only the second decides CheckMod.
    //
    // THE WIRE FORMAT IS UNSIGNED: ?LoadMetaData (0x1800c1b30) serializes +0x10,
    // +0x14 and +0x18 through ReaderWriter +0x98 (kScalarUInt) — 0x1800c1d2a,
    // 0x1800c1d3f, 0x1800c1d54. Two are typed int32_t because CheckMod compares
    // them SIGNED; a sign test and a wire type are different facts about the
    // same four bytes. +0x1c is written by NO retail body: a byte comparison
    // over a ModInfo must stop at +0x1c.
    // ========================================================================
    struct ModInfo {
        // The init arm of 0x180034010 (0x1800340ff / 0x180034102): the two
        // String qwords zeroed and NOTHING else — +0x10..+0x1f keep what the
        // storage held. This is what `Collection::Array<ModInfo>` grows with.
        ModInfo() : name(nullptr), path(nullptr) {}
        // The assign arm of 0x180034010 / 0x180033080 (0x1800340b8..0x1800340ec):
        // name zeroed-then-adopted with an AddRef, path the same, the three
        // dwords copied, +0x1c NOT copied. saveload.cpp.
        ModInfo(const ModInfo& other);
        // One element of 0x1800341e0: release +0x08 (path) FIRST, then +0x00
        // (name), with the String refcount idiom. saveload.cpp.
        ~ModInfo();
        // Retail never assigns an element: its Array assign (0x180033f30) is
        // clear-then-copy-construct.
        ModInfo& operator=(const ModInfo&) = delete;

        const char* name;           // +0x00  System::String — display name
        const char* path;           // +0x08  System::String — path, relative to the data dir
        int32_t     versionMajor;   // +0x10  vs IO::Modification entry +0x2c -> status 4
        uint32_t    gameVersion;    // +0x14  < 0x1000400 -> status 2 (modsOutOfDate)
        int32_t     versionMinor;   // +0x18  vs IO::Modification entry +0x1c -> status 3
        uint32_t    pad1c;          // +0x1c  never written by any retail body
    };

    // ========================================================================
    // Save::SaveLoad::SaveInfo — 0x58 bytes, one per save file on disk.
    // ------------------------------------------------------------------------
    // STRIDE 0x58: ?GetSaveInfo (0x1800c3b60) is `_saves._data + index*0x58`;
    // CheckMod's prologue does `IMUL R9,R9,0x58`; ~SaveLoad's array destructor
    // (0x1800329c0) walks the buffer in 0x58 steps.
    //
    //   +0x00  Sprite    thumbnail (saveloaddialog.cpp:253/258 IsValid + memcpy).
    //   +0x10  String    the description/metadata text. ?UpdateList state 4
    //                    (0x1800c2455) refcount-assigns LoadMetaData's out-param.
    //   +0x18  ImageBuffer*  ?OnVideoRestart (0x1800c0f44) recreates it;
    //                    ~SaveInfo deletes it through vtable slot 0 (EDX=1).
    //   +0x20  int32    thumbnail width  } OnVideoRestart's upload args 3/4
    //   +0x24  int32    thumbnail height } (0x1800c0f92/0f8d); SetThumbnail
    //   +0x28  void*    the thumbnail pixel block (arg 2) } writes all three
    //   +0x30  uint64   sortKey: UpdateList state 1 (0x1800c1ff5) copies the
    //                   first qword of IO::Persistence::SaveGameInfo into it;
    //                   the qsort comparator 0x1800c3db0 compares exactly this
    //                   qword, UNSIGNED, DESCENDING.
    //   +0x38  String    display name (autostart.cpp:411 strcmp's it).
    //   +0x40  int32    the Persistence save index this row came from, or -1
    //                   for the blank "new save" row (UpdateList 0x1800c1fcf /
    //                   0x1800c1fdf: R12D, state 1's loop counter). Every sign
    //                   test in the DLL means "is this a real save on disk";
    //                   ~SaveInfo tears +0x18/+0x28 down only when it is >= 0.
    //   +0x44  int32    LoadMetaData out-param 6 — the save's FORMAT VERSION.
    //   +0x48  Array<ModInfo> — _data pinned by CheckMod ([R9+RAX+0x48]); _size
    //                   by saveloaddialog.cpp:1088 reading the count at +0x50.
    //                   Retail's own type (LoadMetaData's mangled parameter),
    //                   so the default, trait-derived spec: with ModInfo's real
    //                   ctor / copy / dtor that IS kSpec_MetaEntry20's behaviour
    //                   (zero-two-Strings init, AddRef copy, path-then-name
    //                   release).
    // No unmodelled byte left.
    // ========================================================================
    struct SaveInfo {
        // THE STACK TEMPLATE UpdateList state 1 builds and appends
        // (0x1800c1fb0..0x1800c1fdf, the blank row at 0x1800c1f41..0x1800c1f65):
        // eleven stores — q+0x00, d+0x08, q+0x10, q+0x18, q+0x28, q+0x30,
        // q+0x38, q+0x40 (= -1, i.e. field44 too), q+0x48, q+0x50, then d+0x40
        // = index — and NEVER +0x0c, +0x20 or +0x24: those keep what the
        // storage held. (+0x2c IS written: +0x28 is a QWORD store,
        // 0x1800c1fc3 `MOV qword ptr [RBP-0x21],RSI`. The old UpdateList banner
        // listed +0x2c as unwritten; corrected here by block B2.)
        // `index` is -1 for the blank "new save" row and the Persistence loop
        // counter for a real one. Retail has no out-of-line default ctor; this
        // IS the inlined one, so the caller placement-news it into
        // deliberately uninitialised storage.
        explicit SaveInfo(int32_t index)
            : thumbnail(),                 // +0x00 qword 0, +0x08 dword -1
              description(nullptr),        // +0x10
              imageBuffer(nullptr),        // +0x18
              thumbPixels(nullptr),        // +0x28
              sortKey(0),                  // +0x30
              name(nullptr),               // +0x38
              saveIndex(index),            // +0x40
              field44(-1),                 // +0x44
              mods() {}                    // +0x48 qword 0, +0x50 qword 0
        // 0x180032f70 (unexported) — the element assign of 0x1800332d0: every
        // qword/dword verbatim, the two Strings adopted with an AddRef, and the
        // nested Array ZEROED then assigned through 0x180033f30.
        // closure_batch2b.cpp.
        SaveInfo(const SaveInfo& other);
        // 0x1800c3f40 (unexported). saveload.cpp.
        ~SaveInfo();
        SaveInfo& operator=(const SaveInfo&) = delete;   // retail never assigns a row

        // 0x1800c3de0 (unexported) — allocate + Create a Video::ImageBuffer,
        // upload `pixels` under the device lock, store pixels/w/h.
        // saveload_saveinfo.cpp.
        void SetThumbnail(void* pixels, int32_t width, int32_t height);
        // 0x1800c3db0 (unexported, 22 bytes) — the qsort comparator UpdateList
        // hands retail's qsort as DATA. saveload_saveinfo.cpp.
        static int Compare(const void* a, const void* b);

        UI::Sprite  thumbnail;       // +0x00
        const char* description;     // +0x10  System::String  (UpdateList state 4)
        void*       imageBuffer;     // +0x18  Video::ImageBuffer*  (OnVideoRestart)
        int32_t     thumbWidth;      // +0x20  upload arg 3
        int32_t     thumbHeight;     // +0x24  upload arg 4
        void*       thumbPixels;     // +0x28  upload arg 2
        uint64_t    sortKey;         // +0x30  SaveGameInfo+0x00; qsort key, DESCENDING
        const char* name;            // +0x38  System::String
        int32_t     saveIndex;       // +0x40  Persistence save index; -1 == the blank row
        int32_t     field44;         // +0x44  LoadMetaData out-param 6 (format version)
        Collection::Array<ModInfo> mods;   // +0x48  {_data, _size, _capacity}
    };

    // The retail type of the +0x50 array: `Array<SaveInfo>` driven by the
    // hand-derived 0x1800332d0 row (its assign is the SaveInfo copy ctor and
    // its destroy ~SaveInfo, both reached through their registered address
    // doors). Retail has no two-argument SetSize for this element and no init
    // arm, so the trait-derived default (which would default-construct a grown
    // tail) is not the instantiation.
    using SaveInfoArray = Collection::Array<SaveInfo, Collection::DefaultAllocator,
                                            Collection::ExplicitSpec<&rtcoll::kSpec_MetaData58>>;

    // ---- the 28 exports, in B37_SLOTS section 6 order -----------------------
    // ?Get@SaveLoad@Save@@SAPEAV12@XZ  0x180030d40 — `MOV RAX,[_singleton]`.
    //   saveload.cpp
    static SaveLoad* Get();
    // ??0SaveLoad@Save@@QEAA@AEBV01@@Z  0x180030d50 — DEEP copy (String AddRef
    // + both Arrays). NOT PORTED, no caller; declared so no C++ code reaches a
    // shallow implicit copy. Deliberately undefined.
    SaveLoad(const SaveLoad& other);
    // ??4SaveLoad@Save@@QEAAAEAV01@AEBV01@@Z  0x180030e50 — NOT PORTED, no caller.
    SaveLoad& operator=(const SaveLoad& other);
    // ?OnVideoRestart@SaveLoad@Save@@QEAAXXZ  0x1800c0f10 — saveload_update.cpp
    void OnVideoRestart();
    // ?OnVideoError@SaveLoad@Save@@QEAAXXZ    0x1800c1000 — defined, NOT
    // registered: no caller (retail's own MainLoop::OnVideoError INLINES this
    // walk, mainloop_video.cpp step 5). saveload_update.cpp.
    void OnVideoError();
    // ?CheckMod@SaveLoad@Save@@QEBAHHH@Z  0x1800c1090 — saveload.cpp
    int CheckMod(int saveIndex, int modIndex) const;
    // ?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ  0x1800c1250 — RETSLOT
    System::String GenerateSaveName() const;
    // ?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ   0x1800c1440 — RETSLOT
    System::String GetAutoSaveName() const;
    // ?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z  0x1800c1490
    //   `name` is the by-value String, by HIDDEN POINTER, destroyed by the
    //   callee. saveload_meta.cpp
    void SaveMetaData(void* pixels, int width, int height, System::String* name, int nameMode);
    // ?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z
    //   0x1800c1b30 — saveload_loadmeta.cpp
    void LoadMetaData(System::String& description, void** pixels, int& width, int& height,
                      int& version, Collection::Array<ModInfo>& mods);
    // ?UpdateList / ?UpdateLoadFromFile / ?UpdateLoad / ?UpdateSave / ?UpdateDelete
    //   0x1800c1ea0 / 0x1800c2560 / 0x1800c2690 / 0x1800c2a80 / 0x1800c2d70
    //   saveload_update.cpp
    void UpdateList();
    void UpdateLoadFromFile();
    void UpdateLoad();
    void UpdateSave();
    void UpdateDelete();
    // ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z  0x1800c2e40 — saveload.cpp
    void Update(const System::Time& delta);
    // ?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z
    //   0x1800c2eb0 — the ExternalPtr is a bare register value (banner).
    //   saveload_draw.cpp
    void Draw(Video::Context& context, Object::ExternalPtr<Graphics::Material> material);
    // ?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ  0x1800c3b50
    SavedState* GetSaveState() { return _saveState; }
    // ?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z   0x1800c3b60 — a
    //   REFERENCE return (`AEBU`), no retslot. NO bounds check: MOVSXD + IMUL.
    const SaveInfo& GetSaveInfo(int index) const {
        return *reinterpret_cast<const SaveInfo*>(
            reinterpret_cast<const uint8_t*>(_saves._data) +
            static_cast<int64_t>(index) * static_cast<int64_t>(sizeof(SaveInfo)));
    }
    // ?GetSaveCount@SaveLoad@Save@@QEBAHXZ  0x1800c3b80 — `MOV EAX,[RCX+0x58]`.
    int GetSaveCount() const { return _saves._size; }
    // ?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z  0x1800c3b90 — the
    //   Sprite by HIDDEN POINTER: `MOV RAX,[RDX] ; MOV [RCX+0x60],RAX ;
    //   MOV RAX,[RDX+8] ; MOV [RCX+0x68],RAX` (both qwords, pad0C included,
    //   which Sprite's trivial copy reproduces).
    void SetSaveSprite(const UI::Sprite* sprite) { _saveSprite = *sprite; }
    // ?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z  0x1800c3bb0
    void SetSaveName(const System::String& name, int mode);
    // ?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z  0x1800c3c40
    void SetMinOperationTime(const System::Time& time);
    // ?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z  0x1800c3c50
    void SetOperation(Operation op, int index);
    // ?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ  0x1800c3c80 — an
    //   enum in EAX (`MOV EAX,[RCX] ; RET`), not a retslot.
    Operation GetOperation() const { return _operation; }
    // ??1SaveLoad@Save@@QEAA@XZ  0x1800c3c90 — DOOR (RtSave_SaveLoad_Dtor), not
    //   declared: nothing destroys a SaveLoad in C++.
    // ??0SaveLoad@Save@@QEAA@XZ  0x1800c3d30 — DOOR (RtSave_SaveLoad_Ctor), not
    //   declared (see the banner).
    // ?_singleton@SaveLoad@Save@@0PEAV12@EA  0x1801fc6f0 — RT_DATA_SLOT (banner).

    // ---- the 0x90 bytes (public: the bodies in six files, and the exe's
    //      raw-offset readers, reach them) ------------------------------------
    Operation   _operation;        // +0x00  GetOperation / SetOperation
    uint32_t    pad04;             // +0x04
    int64_t     _elapsed;          // +0x08  Time ticks; Update adds the frame delta, SetOperation zeroes it
    int64_t     _minOperationTime; // +0x10  Time; SetMinOperationTime copies 8 bytes here
    uint32_t    _progress;         // +0x18  zeroed by SetOperation; NOT zeroed by the ctor
    uint32_t    pad1c;             // +0x1c
    const char* _string20;         // +0x20  System::String; ~SaveLoad releases it with the refcount idiom
    // An entire Save::SavedState embedded BY VALUE (Wave 13's correction of the
    // old `field28`/`_buffer30`): ?UpdateList state 3 (0x1800c2455) calls
    // ?SetupLoad@SavedState with `this + 0x28`; ?UpdateSave (0x1800c2c60) builds
    // SaveBlock entry 0 out of +0x30/+0x2c/+0x28; ~SaveLoad frees +0x30 through
    // the same locked MemFree ~SavedState uses. The save path serialises into
    // TWO SavedStates, one owned inline and one on the heap (+0x40).
    SavedState  _embeddedState;    // +0x28 .. +0x37
    uint32_t    field38;           // +0x38  UpdateList's metadata cursor
    uint32_t    pad3c;             // +0x3c  NOT written by the ctor
    SavedState* _saveState;        // +0x40  GetSaveState. NOT written by the ctor
    int32_t     _selectedIndex;    // +0x48  SetOperation's second argument; ctor -1
    uint32_t    pad4c;             // +0x4c
    SaveInfoArray _saves;          // +0x50  {_data, _size, _capacity}; GetSaveCount is _size
    UI::Sprite  _saveSprite;       // +0x60  SetSaveSprite; ctor {0, -1}
    const char* _saveName;         // +0x70  System::String; SetSaveName refcount-assigns it
    int32_t     _saveNameMode;     // +0x78  SetSaveName's second argument
    uint32_t    _captureFlags;     // +0x7c  ONLY the low byte is written (ctor 0; Draw tests it, then clears it)
    int32_t     _captureWidth;     // +0x80  Draw reads it as the capture width
    int32_t     _captureHeight;    // +0x84  Draw reads it as the capture height
    void*       _capturePixels;    // +0x88  Draw MemAllocs w*h*4 and stores it here
};

// ---- ModInfo ----------------------------------------------------------------
static_assert(offsetof(SaveLoad::ModInfo, name)         == 0x00, "ModInfo.name @ 0x00");
static_assert(offsetof(SaveLoad::ModInfo, path)         == 0x08, "ModInfo.path @ 0x08");
static_assert(offsetof(SaveLoad::ModInfo, versionMajor) == 0x10, "ModInfo.versionMajor @ 0x10");
static_assert(offsetof(SaveLoad::ModInfo, gameVersion)  == 0x14, "ModInfo.gameVersion @ 0x14");
static_assert(offsetof(SaveLoad::ModInfo, versionMinor) == 0x18, "ModInfo.versionMinor @ 0x18");
static_assert(offsetof(SaveLoad::ModInfo, pad1c)        == 0x1c, "ModInfo.pad1c @ 0x1c");
static_assert(sizeof(SaveLoad::ModInfo)  == 0x20, "sizeof(Save::SaveLoad::ModInfo) == 0x20 (SHL 5, three bodies)");
static_assert(alignof(SaveLoad::ModInfo) == 8,    "ModInfo aligns to 8 (0x180034010's MemAlloc(n*0x20, 8))");
static_assert(!std::is_trivially_copyable<SaveLoad::ModInfo>::value,
              "RULE B7-8: ModInfo's copy AddRefs two Strings; it is not trivially copyable");

// ---- SaveInfo ---------------------------------------------------------------
static_assert(offsetof(SaveLoad::SaveInfo, thumbnail)   == 0x00, "SaveInfo.thumbnail @ 0x00");
static_assert(offsetof(SaveLoad::SaveInfo, description) == 0x10, "SaveInfo.description @ 0x10");
static_assert(offsetof(SaveLoad::SaveInfo, imageBuffer) == 0x18, "SaveInfo.imageBuffer @ 0x18");
static_assert(offsetof(SaveLoad::SaveInfo, thumbWidth)  == 0x20, "SaveInfo.thumbWidth @ 0x20");
static_assert(offsetof(SaveLoad::SaveInfo, thumbHeight) == 0x24, "SaveInfo.thumbHeight @ 0x24");
static_assert(offsetof(SaveLoad::SaveInfo, thumbPixels) == 0x28, "SaveInfo.thumbPixels @ 0x28");
static_assert(offsetof(SaveLoad::SaveInfo, sortKey)     == 0x30, "SaveInfo.sortKey @ 0x30");
static_assert(offsetof(SaveLoad::SaveInfo, name)        == 0x38, "SaveInfo.name @ 0x38");
static_assert(offsetof(SaveLoad::SaveInfo, saveIndex)   == 0x40, "SaveInfo.saveIndex @ 0x40");
static_assert(offsetof(SaveLoad::SaveInfo, field44)     == 0x44, "SaveInfo.field44 @ 0x44");
static_assert(offsetof(SaveLoad::SaveInfo, mods)        == 0x48, "SaveInfo.mods @ 0x48");
static_assert(offsetof(SaveLoad::SaveInfo, mods) + offsetof(Collection::Array<SaveLoad::ModInfo>, _size) == 0x50,
              "SaveInfo.mods._size @ 0x50 (saveloaddialog.cpp:1088)");
static_assert(sizeof(SaveLoad::SaveInfo)  == 0x58, "sizeof(Save::SaveLoad::SaveInfo) == 0x58 (IMUL 0x58)");
static_assert(alignof(SaveLoad::SaveInfo) == 8,    "SaveInfo aligns to 8 (kSpec_MetaData58 align 8)");
static_assert(!std::is_trivially_copyable<SaveLoad::SaveInfo>::value,
              "RULE B7-8: SaveInfo's copy (0x180032f70) AddRefs two Strings and deep-copies its Array");

// ---- SaveLoad ---------------------------------------------------------------
static_assert(offsetof(SaveLoad, _operation)        == 0x00, "SaveLoad._operation @ 0x00");
static_assert(offsetof(SaveLoad, _elapsed)          == 0x08, "SaveLoad._elapsed @ 0x08");
static_assert(offsetof(SaveLoad, _minOperationTime) == 0x10, "SaveLoad._minOperationTime @ 0x10");
static_assert(offsetof(SaveLoad, _progress)         == 0x18, "SaveLoad._progress @ 0x18");
static_assert(offsetof(SaveLoad, _string20)         == 0x20, "SaveLoad._string20 @ 0x20");
static_assert(offsetof(SaveLoad, _embeddedState)    == 0x28, "SaveLoad._embeddedState @ 0x28");
static_assert(offsetof(SaveLoad, _embeddedState) + offsetof(SavedState, _data) == 0x30,
              "the embedded SavedState puts its _data at SaveLoad+0x30 — the old _buffer30");
static_assert(offsetof(SaveLoad, field38)           == 0x38, "SaveLoad.field38 @ 0x38");
static_assert(offsetof(SaveLoad, _saveState)        == 0x40, "SaveLoad._saveState @ 0x40");
static_assert(offsetof(SaveLoad, _selectedIndex)    == 0x48, "SaveLoad._selectedIndex @ 0x48");
static_assert(offsetof(SaveLoad, _saves)            == 0x50, "SaveLoad._saves @ 0x50");
static_assert(offsetof(SaveLoad, _saves) + offsetof(SaveLoad::SaveInfoArray, _size) == 0x58,
              "GetSaveCount reads +0x58");
static_assert(offsetof(SaveLoad, _saveSprite)       == 0x60, "SaveLoad._saveSprite @ 0x60");
static_assert(offsetof(SaveLoad, _saveName)         == 0x70, "SaveLoad._saveName @ 0x70");
static_assert(offsetof(SaveLoad, _saveNameMode)     == 0x78, "SaveLoad._saveNameMode @ 0x78");
static_assert(offsetof(SaveLoad, _captureFlags)     == 0x7c, "SaveLoad._captureFlags @ 0x7c");
static_assert(offsetof(SaveLoad, _captureWidth)     == 0x80, "SaveLoad._captureWidth @ 0x80");
static_assert(offsetof(SaveLoad, _captureHeight)    == 0x84, "SaveLoad._captureHeight @ 0x84");
static_assert(offsetof(SaveLoad, _capturePixels)    == 0x88, "SaveLoad._capturePixels @ 0x88");
static_assert(sizeof(SaveLoad)  == 0x90, "sizeof(Save::SaveLoad) == 0x90");
static_assert(alignof(SaveLoad) == 8,    "Save::SaveLoad aligns to 8 (pointer members)");
static_assert(!std::is_polymorphic<SaveLoad>::value, "no vtable: the ctor's first store is _operation");
static_assert(!std::is_trivially_copyable<SaveLoad>::value,
              "RULE B7-8: retail's copy (0x180030d50) is deep; SaveLoad is not trivially copyable");

}  // namespace Save

// ============================================================================
// RtSave — the transition aliases. The PODs of Waves 5-24 BECAME the classes
// above (one definition per layout); these names stay until the last runtime
// spelling of them converts.
// ============================================================================
namespace RtSave {
using SaveLoad   = Save::SaveLoad;
using SavedState = Save::SavedState;
using SaveInfo   = Save::SaveLoad::SaveInfo;
using ModInfo    = Save::SaveLoad::ModInfo;
using Operation  = Save::SaveLoad::Operation;
constexpr Operation kOpNone         = Save::SaveLoad::kOpNone;
constexpr Operation kOpList         = Save::SaveLoad::kOpList;
constexpr Operation kOpLoad         = Save::SaveLoad::kOpLoad;
constexpr Operation kOpLoadFromFile = Save::SaveLoad::kOpLoadFromFile;
constexpr Operation kOpSave         = Save::SaveLoad::kOpSave;
constexpr Operation kOpDelete       = Save::SaveLoad::kOpDelete;
// ENDGAME B.3.6 block D (RULE B6-4.2): ONE definition of UI::Sprite.
using Sprite = UI::Sprite;
static_assert(offsetof(Sprite, spriteIndex) == 0x08, "Sprite.index @ 0x08");
static_assert(sizeof(Sprite) == 0x10, "sizeof(UI::Sprite) == 0x10");
}  // namespace RtSave
