#pragma once
// ============================================================================
// runtime/types/io/Modification.h — IO::Modification binary layout
// ============================================================================
// Wave 4, lane D modelled only the three fields
// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z (retail 0x1800aea10)
// touches. WAVE 9 LANE MOD completes the record, because the mod-entry
// lifecycle triple (copy ctor 0x1800101e0, operator= 0x180026ef0, dtor
// 0x180011460) walks EVERY member of Mod and a port of them needs the whole
// 0x80 laid out, not just the stride.
//
// Every offset below was read out of one of those three bodies, which is the
// strongest source available for a layout: the copy ctor visits the members in
// declaration order and its element helper tells you the type of each.
//
//   +0x00/08/10  three refcounted System::String   (adopt/release idiom)
//   +0x18..0x2c  six int32 copied field by field
//   +0x30        Collection::Array<int32>          (op= 0x180011110, stride 4)
//   +0x40        Collection::Array<uint8>          (op= 0x180011220, stride 1)
//   +0x50        bool enabled                      (MOVZX/MOV byte)
//   +0x58        System::Filename                  (ctor-copy 0x1800067d0)
//   +0x60        Collection::Set<Filename> root+count  (clone 0x1800104f0)
//   +0x70        Collection::Array<Filename>       (op= 0x1800091f0, stride 8)
//
// The two Modification header flags come from their own accessors, which is
// why they are named rather than guessed:
//   +0x00 _dirty          — ?IsDirty@Modification@IO@@QEBA_NXZ is `MOVZX EAX,
//                           byte ptr [RCX]; RET`.
//   +0x01 _savingSettings — ?EnableSavingSettings@ (0x1800aead0) is the only
//                           writer; the ctor (0x1800b04b0) opens with
//                           `MOV word ptr [RCX],0x101`, i.e. BOTH start at 1.
//                           That is why SaveList/RefreshList take their
//                           registry branch on a normal boot.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>   // B.3.4 PACKAGE+FS: the "IO::Modification has no vptr" pins

#include "../system/Filename.h"

namespace RtIO {

// Collection::Array<T> header, 16 bytes and T-independent (Collection.h in the
// Wave 2 tree static_asserts the same thing). Spelled locally so this header
// stays includable from engine/ without dragging the collection tree in.
struct ModArray {
    void*   data;       // +0x00
    int32_t size;       // +0x08
    int32_t capacity;   // +0x0c
};
static_assert(sizeof(ModArray) == 0x10, "Collection::Array is 16 bytes");

// One node of the Filename-keyed set at Mod+0x60. Allocated at 0x180010570
// (`MOV EDX,0x28` size, `LEA R8D,[RDX-0x20]` align 8) and cloned at 0x1800104f0.
struct ModSetNode {
    RtSystem::Filename key;      // +0x00 — copy-constructed by 0x1800067d0
    ModSetNode*        parent;   // +0x08
    ModSetNode*        left;     // +0x10
    ModSetNode*        right;    // +0x18
    uint16_t           w20;      // +0x20 — copied verbatim by the clone
    uint16_t           w22;      // +0x22 — ditto
    uint32_t           _pad24;   // +0x24
};
static_assert(sizeof(ModSetNode) == 0x28, "node alloc at 0x180010570 asks for 0x28");
static_assert(offsetof(ModSetNode, parent) == 0x08, "0x1800105e8 MOV [RDI+8],RSI");
static_assert(offsetof(ModSetNode, left)   == 0x10, "0x180010535 MOV [RBX+0x10],RAX");
static_assert(offsetof(ModSetNode, right)  == 0x18, "0x18001054e MOV [RBX+0x18],RAX");

struct ModSet {
    ModSetNode* root;    // +0x00
    int32_t     count;   // +0x08
    int32_t     _pad0c;
};
static_assert(sizeof(ModSet) == 0x10, "CopyCtor zeroes [RSI] and [RSI+8] then strides 0x10");

// One entry of Modification's mod array.
struct Mod {
    char*    name;          // +0x00 — refcounted String
    char*    author;        // +0x08 — refcounted String
    char*    description;   // +0x10 — refcounted String

    int32_t  i18;           // +0x18
    int32_t  i1c;           // +0x1c
    int32_t  order;         // +0x20 — SaveList writes the loop index here
    int32_t  i24;           // +0x24
    int32_t  i28;           // +0x28
    int32_t  i2c;           // +0x2c

    ModArray ints;          // +0x30 — Array<int32>
    ModArray bytes;         // +0x40 — Array<uint8>

    uint8_t  enabled;       // +0x50 — Enable() writes this byte
    uint8_t  _pad51[7];

    RtSystem::Filename file;   // +0x58 — the mod's .pkm path; Find()'s key

    ModSet   set;           // +0x60 — Filename-keyed set
    ModArray files;         // +0x70 — Array<Filename>
};
static_assert(sizeof(Mod) == 0x80,
              "Find() 0x1800aea10: `lVar3 = lVar3 + 0x80` strides the array by this");
static_assert(offsetof(Mod, order)   == 0x20, "SaveList 0x1800b0654");
static_assert(offsetof(Mod, ints)    == 0x30, "CopyCtor 0x180010265 ADD RCX,0x30");
static_assert(offsetof(Mod, bytes)   == 0x40, "CopyCtor 0x180010283 LEA RCX,[RBX+0x40]");
static_assert(offsetof(Mod, enabled) == 0x50, "Enable 0x1800aeabe MOV [RDX+RAX+0x50],R8B");
static_assert(offsetof(Mod, file)    == 0x58,
              "Find() 0x1800aea10: `*(char**)(*(this+8) + 0x58 + lVar3)`");
static_assert(offsetof(Mod, set)     == 0x60, "CopyCtor 0x1800102ae LEA R14,[RDI+0x60]");
static_assert(offsetof(Mod, files)   == 0x70, "CopyCtor 0x1800102e0 LEA RCX,[RBX+0x70]");

struct Modification {
    uint8_t  dirty;           // +0x00 — IsDirty 0x18000daa0
    uint8_t  savingSettings;  // +0x01 — EnableSavingSettings 0x1800aead0
    uint8_t  _pad02[6];
    Mod*     mods;    // +0x08 — Find()'s `*(longlong*)(this + 8)`
    int32_t  count;   // +0x10 — Find()'s `*(int*)(this + 0x10)`
    int32_t  capacity;// +0x14
    Mod*     savedMods;     // +0x18 — the backup EnableSavingSettings(false) makes
    int32_t  savedCount;    // +0x20
    int32_t  savedCapacity; // +0x24
};
static_assert(sizeof(Modification) == 0x28, "ctor 0x1800b04b0 zeroes +0x08..+0x27");
static_assert(offsetof(Modification, mods)  == 0x08, "see field comment");
static_assert(offsetof(Modification, count) == 0x10, "see field comment");
static_assert(offsetof(Modification, savedMods) == 0x18,
              "SaveList 0x1800b0619: ModArrayCopy(this+0x18, this+0x08)");

}  // namespace RtIO

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — the REAL `IO::Modification`.
// ============================================================================
//
// Design: B34_RULES.md §1.5 rule 1 (retail name, retail namespace), §4.3 (the
// ctor/dtor door question), §1.5 rule 3 (the PODs stay), B34_BRIEFS §E.4.
//
// *** IT HAS NO VTABLE, AND IT IS A SINGLETON. ***
// ------------------------------------------------
// There is no `??_7Modification@IO@@6B@`: not in the retail export table, not
// in `standalone/rt_data_mint_tables.inc` (the only Modification row there is
// the SINGLETON POINTER `?_singleton@Modification@IO@@2PEAV12@EA` at
// 0x1801fc6b0), and `deleting_dtor.cpp` carries no row for it. B34_RULES §4.2's
// own table names this as the one exception in the sweep: "Modification's dtor
// 0x1800b0470 is a singleton destructor, not a vtable row". Every exported
// member is `QEAA`/`QEBA`.
//
// *** THE CONSTRUCTOR AND DESTRUCTOR DO NOT CONVERT IN THIS STEP. ***
// -------------------------------------------------------------------
// Not because of a vtable — there is none — but because neither door is in
// PACKAGE+FS's file set: `??0Modification@IO@@QEAA@XZ` is registered by
// `runtime/src/engine/singleton_ctors.cpp:909` and `??1Modification@IO@@QEAA@XZ`
// by `runtime/src/io/iobase_vtables.cpp:579`, both of which stay with the core
// owner. The destructor's ORDER is retail's and is reproduced, not tidied
// (B34_BRIEFS §E.4): it clears the exported singleton — tagged
// `UNPROMOTABLE(HARMFUL)` in `reg_mint_promoted.cpp` — then destroys
// `_savedMods`, then `_mods`. Nothing here changes any of that.
//
// *** `Mod`, `ModSet`, `ModSetNode` AND `ModArray` STAY PODs. ***
// ---------------------------------------------------------------
// `IO::Modification::Mod` is retail's nested struct — `?GetModification@` spells
// it `UMod@12@`, `U` for struct, `12` for `Modification::IO`. It is NOT nested
// here and it is NOT converted, for the plain reason that `RtIO::Mod` and
// `RtIO::Modification` are read outside this block's file set entirely:
// `runtime/src/engine/{loops,engine_platform,singleton_ctors}.cpp` and
// `runtime/src/save/{saveload_meta,saveload_update}.cpp` all include this
// header. B33 §7's rule applies verbatim — the POD stays, trivially
// destructible, until its last user converts, and that is not this step.
//
// That is also the answer to the `RtSystem::Filename` member question for this
// class. The two Filename PODs in this header (`Mod::file` at +0x58 and
// `ModSetNode::key`) belong to those two PODs, not to `Modification` itself,
// which holds no String or Filename member at all: its 0x28 bytes are two
// flags and two `Collection::Array<Mod>` headers. So there is nothing to
// retype here and no double-free to risk.
//
// *** WHAT IS AND IS NOT DECLARED. ***
// ------------------------------------
// Only the members this tree actually ports. `?IsDirty@Modification@IO@@QEBA_NXZ`
// (0x18000daa0) and `?EnableSavingSettings@` (0x1800aead0) are real retail
// members and are deliberately ABSENT: nothing in this tree ports or calls
// them, and an uncalled port is an unverified port. The two unexported sorts
// (0x180028760 / 0x180028fc0, runtime/src/io/modification_sort.cpp) are NOT
// members — their retail signatures carry no `this` — so they stay the free
// functions they are.
// ============================================================================

namespace IO {

class Modification {
public:
    // ---- the ported members, each with its retail address -----------------
    // Bodies: runtime/src/io/modification.cpp (the first five),
    // modification_refresh.cpp (RefreshList),
    // modification_setupfilesystem.cpp (SetupFileSystem),
    // filesystem.cpp (Find).

    // ?GetCount@Modification@IO@@QEBAHXZ                       0x18000da90
    int GetCount() const;

    // ?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z     0x18000da80
    // Returns `const Mod&`. `Mod` is still the `RtIO::Mod` POD (see the banner)
    // — a B.3.4 spelling, not a permanent one. The index is SIGN-extended
    // before the `<< 7`: a negative index strides BACKWARDS, and that is
    // load-bearing, not a decompiler artefact.
    const RtIO::Mod& GetModification(int index) const;

    // ?Enable@Modification@IO@@QEAAXH_N@Z                      0x1800aeab0
    void Enable(int index, bool value);

    // ?Swap@Modification@IO@@QEAAXHH@Z                         0x1800aeb20
    void Swap(int a, int b);

    // ?SaveList@Modification@IO@@QEAAXXZ                       0x1800b05f0
    void SaveList();

    // ?RefreshList@Modification@IO@@QEAAXXZ                    0x1800af5f0
    void RefreshList();

    // ?SetupFileSystem@Modification@IO@@QEAAXXZ                0x1800aebb0
    void SetupFileSystem();

    // ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z      0x1800aea10
    // Declared with the `RtSystem::Filename` POD for its parameter because that
    // is what the door's signature carries today; `System::Filename` is a real
    // class already, so this is a spelling the caller batches collapse, not a
    // layout question (the two are layout-identical).
    int Find(const RtSystem::Filename& file) const;

    // ---- the 0x28 bytes, at retail's offsets -------------------------------
    // PUBLIC for the same reason `IO::Serializer`'s are: unconverted bodies
    // across `runtime/src/engine` and `runtime/src/save` reach them at these
    // offsets through the `RtIO::Modification` POD, and `offsetof` on them has
    // to be well formed for the pins below.
    uint8_t  _dirty;           // +0x00 — IsDirty 0x18000daa0
    uint8_t  _savingSettings;  // +0x01 — EnableSavingSettings 0x1800aead0
    uint8_t  _pad02[6];
    RtIO::Mod* _mods;          // +0x08
    int32_t  _count;           // +0x10
    int32_t  _capacity;        // +0x14
    RtIO::Mod* _savedMods;     // +0x18
    int32_t  _savedCount;      // +0x20
    int32_t  _savedCapacity;   // +0x24
};

static_assert(sizeof(Modification) == 0x28, "ctor 0x1800b04b0 zeroes +0x08..+0x27");
static_assert(sizeof(Modification) == sizeof(RtIO::Modification),
              "IO::Modification and the RtIO::Modification POD stay interchangeable");
static_assert(offsetof(Modification, _mods)      == 0x08, "see the POD's field comment");
static_assert(offsetof(Modification, _count)     == 0x10, "see the POD's field comment");
static_assert(offsetof(Modification, _capacity)  == 0x14, "the Array capacity");
static_assert(offsetof(Modification, _savedMods) == 0x18,
              "SaveList 0x1800b0619: ModArrayCopy(this+0x18, this+0x08)");
static_assert(offsetof(Modification, _savedCount) == 0x20, "the saved Array size");
static_assert(offsetof(Modification, _dirty)          == offsetof(RtIO::Modification, dirty),
              "alias dirty");
static_assert(offsetof(Modification, _savingSettings) == offsetof(RtIO::Modification, savingSettings),
              "alias savingSettings");
static_assert(offsetof(Modification, _mods)      == offsetof(RtIO::Modification, mods),      "alias mods");
static_assert(offsetof(Modification, _count)     == offsetof(RtIO::Modification, count),     "alias count");
static_assert(offsetof(Modification, _capacity)  == offsetof(RtIO::Modification, capacity),  "alias capacity");
static_assert(offsetof(Modification, _savedMods) == offsetof(RtIO::Modification, savedMods), "alias savedMods");
static_assert(offsetof(Modification, _savedCount) == offsetof(RtIO::Modification, savedCount),
              "alias savedCount");
static_assert(offsetof(Modification, _savedCapacity) == offsetof(RtIO::Modification, savedCapacity),
              "alias savedCapacity");
// The guard, as an assertion so a future `virtual` fails the build rather than
// the boot: a vptr would push every field eight bytes down under doors that are
// registered BY ADDRESS and under a live singleton five other files read.
static_assert(!std::is_polymorphic<Modification>::value,
              "IO::Modification must never acquire a vtable: retail has none for it");
static_assert(std::is_standard_layout<Modification>::value,
              "the doors reach these fields through a reinterpret_cast");

}  // namespace IO
