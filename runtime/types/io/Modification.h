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

namespace Object { class External; }   // ReloadEntry::external

namespace RtIO {

// Collection::Array<T> header, 16 bytes and T-independent (Collection.h in the
// Wave 2 tree static_asserts the same thing). Spelled locally so this header
// stays includable from engine/ without dragging the collection tree in.
//
// B.4 P2 R-IO: a TYPED view of the one header (the container policy: typed
// template wrappers over the one erased core). `ModArrayOf<T>` is the same 16
// bytes for every T -- a POD, trivially constructible and destructible, so no
// ctor or dtor runs that did not run before -- and `data` carries the element
// type the owning member's retail operator= / ~Array strides by (Mod's +0x30
// int32, +0x40 uint8, +0x70 Filename; the copy ctor 0x1800101e0 names each).
// `ModArray` is the erased spelling the void* core entry points still take.
template <class T>
struct ModArrayOf {
    T*      data;       // +0x00
    int32_t size;       // +0x08
    int32_t capacity;   // +0x0c
};
using ModArray = ModArrayOf<void>;
static_assert(sizeof(ModArray) == 0x10, "Collection::Array is 16 bytes");
static_assert(offsetof(ModArray, size)     == 0x08, "Array::_size @ +0x08");
static_assert(offsetof(ModArray, capacity) == 0x0c, "Array::_capacity @ +0x0c");
static_assert(sizeof(ModArrayOf<int32_t>) == sizeof(ModArray), "one header for every T");

// One node of the Filename-keyed set at Mod+0x60. Allocated at 0x180010570
// (`MOV EDX,0x28` size, `LEA R8D,[RDX-0x20]` align 8) and cloned at 0x1800104f0.
struct ModSetNode {
    // B.4 Z MIRROR: the real class. The node is raw MemAlloc storage
    // (0x180010570) that is never constructed, copied or destroyed as a
    // ModSetNode: `key` is placement-constructed by the copy ctor and released
    // by the Set<Filename> node frees, so typing it changes no code.
    System::Filename   key;      // +0x00 — copy-constructed by 0x1800067d0
    ModSetNode*        parent;   // +0x08
    ModSetNode*        left;     // +0x10
    ModSetNode*        right;    // +0x18
    // +0x20 / +0x22: the AVL balance factor and height. SIGNED: the retrace
    // (0x1800274c0) tests them with `CMP CX,1 / JLE`, `CMP word [RDX+0x20],-1 /
    // JGE` and `TEST CX,CX / JS`, and AvlUpdate writes +0x20 = rh - lh, +0x22 =
    // max(lh, rh) (0x180027529 / 0x180027525). The clone copies both verbatim.
    int16_t            balance;  // +0x20
    int16_t            height;   // +0x22
    uint32_t           _pad24;   // +0x24
};
static_assert(sizeof(ModSetNode) == 0x28, "node alloc at 0x180010570 asks for 0x28");
static_assert(offsetof(ModSetNode, parent) == 0x08, "0x1800105e8 MOV [RDI+8],RSI");
static_assert(offsetof(ModSetNode, left)   == 0x10, "0x180010535 MOV [RBX+0x10],RAX");
static_assert(offsetof(ModSetNode, right)  == 0x18, "0x18001054e MOV [RBX+0x18],RAX");
static_assert(offsetof(ModSetNode, balance) == 0x20, "0x180027529 MOV [RDX+0x20],CX");
static_assert(offsetof(ModSetNode, height)  == 0x22, "0x180027525 MOV [RDX+0x22],AX");

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

    ModArrayOf<uint32_t> ints;   // +0x30 — Array<uint> (op= 0x180011110, stride 4; filled from ?GetIconData@ `?$Array@I`)
    ModArrayOf<uint8_t> bytes;   // +0x40 — Array<uint8> (op= 0x180011220, stride 1)

    uint8_t  enabled;       // +0x50 — Enable() writes this byte
    uint8_t  _pad51[7];

    RtSystem::Filename file;   // +0x58 — the mod's .pkm path; Find()'s key

    ModSet   set;           // +0x60 — Filename-keyed set
    ModArrayOf<System::Filename> files;     // +0x70 — Array<Filename> (op= 0x1800091f0, stride 8); B.4 Z LAST IO: real Filename elements in the trivial header
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

// The reload queue's element in ?SetupFileSystem@Modification@IO@@ —
// `{uint32 typeId, External*, Filename}`, stride 0x18, built at [RBP-0x71]
// (modification_setupfilesystem.cpp) and copied by the array's assign arm
// 0x180028cd0 (collection/array.cpp kSpec_ModExternal18). THE FOUR BYTES AT
// +0x04 ARE NEVER WRITTEN by anybody (retail's first store is a DWORD and the
// next is at +0x08).
// B.4 Z MIRROR2 (`behaviour:`): `file` is a real System::Filename and the
// entry has retail's stack-pair ctor (0x1800af052..0x1800af0b2: typeId from
// the Type() slot, external, then the Filename copy ctor 0x1800067d0 inlined
// on +0x10, AdjustDelimiters twice; +0x04 left unwritten) and its implicit
// dtor is retail's release of +0x10 right after the push (0x1800af0d5). The
// queue's ELEMENTS are raw storage of the erased core (kSpec_ModExternal18
// copies through the copy ctor and SetupFileSystem's teardown runs
// `file.~Filename()` per element), never built or destroyed as ReloadEntry.
struct ReloadEntry {
    ReloadEntry(uint32_t t, ::Object::External* e, const System::Filename& f)
        : typeId(t), external(e), file(f) {}

    uint32_t             typeId;    // +0x00
    uint32_t             _hole04;   // +0x04 — NEVER WRITTEN
    ::Object::External*  external;  // +0x08
    System::Filename     file;      // +0x10
};
static_assert(offsetof(ReloadEntry, external) == 0x08, "0x1800af058 MOV [RBP-0x69],R14");
static_assert(offsetof(ReloadEntry, file)     == 0x10, "0x1800af064 MOV [RBP-0x61],R13");
static_assert(sizeof(ReloadEntry) == 0x18, "0x180028d2d stride, cap*3*8");

// (B.4 Z RT2: the RtIO::Modification POD mirror is gone; IO::Modification below
// is the one layout.)

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
// here and it is NOT converted, for the plain reason that `RtIO::Mod` is
// read outside this block's file set entirely:
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
    int Find(const System::Filename& file) const;

    // ---- the 0x28 bytes, at retail's offsets -------------------------------
    // PUBLIC for the same reason `IO::Serializer`'s are: unconverted bodies
    // across `runtime/src/engine` and `runtime/src/save` read them by name, and
    // `offsetof` on them has to be well formed for the pins below.
    uint8_t  _dirty;           // +0x00 — IsDirty 0x18000daa0
    uint8_t  _savingSettings;  // +0x01 — EnableSavingSettings 0x1800aead0
    uint8_t  _pad02[6];
    // B.4 Z RT2: the two Array<Mod>s as the typed 16-byte header (POD, trivially
    // constructible / destructible: the ctor 0x1800b04b0 still zeroes the six
    // fields by name, ??1 0x1800b0470 still releases them through 0x180010b80).
    RtIO::ModArrayOf<RtIO::Mod> _mods;        // +0x08 {data, size +0x10, capacity +0x14}
    RtIO::ModArrayOf<RtIO::Mod> _savedMods;   // +0x18 {data, size +0x20, capacity +0x24}

    // ?_singleton@Modification@IO@@2PEAV12@EA (0x1801fc6b0) -- the class
    // static (ENDGAME B.3.9 IO, B39_RULES D7; retail's `2` = public). Defined
    // in modification.cpp; RtIoPortInit redirects the minted row onto it, so
    // the name door, the address door, the manifest publish and this member
    // are the same eight bytes. Written by the ??0Modification door
    // (engine/singleton_ctors.cpp) and ??1Modification (iobase_vtables.cpp).
    static Modification* _singleton;
};

static_assert(sizeof(Modification) == 0x28, "ctor 0x1800b04b0 zeroes +0x08..+0x27");
static_assert(offsetof(Modification, _mods)      == 0x08, "Find()'s `*(longlong*)(this + 8)`, count at +0x10");
static_assert(offsetof(Modification, _savedMods) == 0x18,
              "SaveList 0x1800b0619: ModArrayCopy(this+0x18, this+0x08)");
// The guard, as an assertion so a future `virtual` fails the build rather than
// the boot: a vptr would push every field eight bytes down under doors that are
// registered BY ADDRESS and under a live singleton five other files read.
static_assert(!std::is_polymorphic<Modification>::value,
              "IO::Modification must never acquire a vtable: retail has none for it");
static_assert(std::is_standard_layout<Modification>::value,
              "the doors reach these fields through a reinterpret_cast");

}  // namespace IO

// ==== B3R W0-P0c: Runtime bodies reached by retail address ====
// ENDGAME B.3R (B3R_RULES R5): each body below is registered at the retail
// address in its trailing comment and read through the registry today. It is
// declared here, copied from its definition (linkage, namespace, types), so the
// reader can call it by name. Nothing calls it through this declaration yet.

namespace RtIo {
// ---- io/iobase_vtables.cpp
extern "C" void RtIo_Modification_Dtor(IO::Modification* self);  // 0x1800b0470
}  // namespace RtIo

namespace RtIO {
// ---- io/modification_sort.cpp
extern "C" void RtIo_Modification_SortMods(Mod* data, int32_t count, void*);  // 0x180028760
// ---- io/modification.cpp
void* RtColl_ArrayFilenameDtor_1800098d0(ModArrayOf<System::Filename>* a);  // 0x1800098d0
ModArrayOf<Mod>* RtColl_ArrayModAssign_180010110(ModArrayOf<Mod>* dst, const ModArrayOf<Mod>* src);  // 0x180010110
void* RtColl_ArrayModDtor_180010b80(ModArrayOf<Mod>* a);  // 0x180010b80
void* RtColl_ArrayU8Assign_180011220(void* dstv, const void* srcv);  // 0x180011220
void* RtIo_Mod_Assign_180026ef0(Mod* dst, const Mod* src);  // 0x180026ef0
void* RtIo_Mod_CopyCtor_1800101e0(Mod* dst, const Mod* src);  // 0x1800101e0
void RtIo_Mod_Dtor_180011460(Mod* self);  // 0x180011460
}  // namespace RtIO
// ==== end B3R W0-P0c ====
