#pragma once
// ============================================================================
// runtime/types/object/TypeInfo.h — Object reflection layouts (Wave 3, lane A)
// ============================================================================
//
// Every layout below was read out of Runtime-x64.dll in Ghidra (image base
// 0x180000000, 2026-09-06) and cross-checked against Runtime-x64-profile.dll
// (the ToolKit build), whose asserts name the fields for us. The function that
// pins each offset is named next to it. Nothing here is inferred from a
// consumer's guess; where something IS a guess it says so.
//
// WHAT THIS DESCRIBES
// -------------------
// The engine's reflection system has two halves that are easy to conflate:
//
//   1. `BaseTypeInfo` — one static object per registered class, chained
//      through `_firstTypeInfo`. It is built by static initialisers (the DLL's
//      own, and ours through code/Game/Object/factory_registration.cpp) BEFORE
//      anything can look a type up. It carries the class's names and its two
//      factory functions, and its `typeId` is 0xFFFFFFFF until Startup.
//
//   2. `TypeNode` / `TypeRecord` — the AVL tree `Object::Factory` builds in
//      `Factory::Startup` (0x1800c0630) by walking that chain once, CRC'ing
//      each name into a typeId, and inserting a node per type. Every runtime
//      lookup (CreateObject, GetTypeIdFromName, IsDerivedFrom, GetTypeName,
//      GetTypeIdForFileExtension) walks THIS tree, never the chain.
//
// The tree is `Collection::Set<Pair<uint32_t, TypeRecord>, …>` — exactly the
// shape WAVE2_COLLECTION.md predicted, with the key stored separately from the
// value. The profile build proves it verbatim: Factory::IsDerivedFrom
// (profile 0x18008ce60) asserts
//     "typePtr->_type == type && it->key == type"   Object\Factory.cpp:0x8c
// i.e. the node holds BOTH `it->key` (node+0x00) and `typePtr->_type`
// (node+0x08), and they are required to agree.
//
// NO OBJECT HERE IS EVER ALLOCATED BY US. Wave 3 lane A only reads these
// structures at their live DLL addresses, and only `BaseTypeInfo` is ever
// written — by the ported constructor, into storage the CALLER owns.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtObject {

// ============================================================
// Object::Base — only the vtable pointer is pinned here.
//
// Pinned by Base::IsA (retail 0x1800491c0 / profile 0x1800828f0):
//     (**(code **)(*(longlong *)this + 0x10))()
// so vtable slot 2 (+0x10) is `virtual uint32_t GetTypeId() const`. Every
// concrete implementation of that slot seen in the exe is a two-instruction
// `MOV EAX,[rip+typeIdGlobal]; RET`, which is why calling it twice (as the
// diff harness does) is free of side effects.
//
// The rest of Base's layout belongs to lanes B/C; do not add fields here.
// ============================================================
struct Base {
    void** vtable;   // +0x00
};
static_assert(offsetof(Base, vtable) == 0x00, "Base::vtable");

// ============================================================
// Object::BaseTypeInfo — 0x38 bytes, one static per registered class.
//
// Pinned field-by-field by the constructor
//   ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z
//   retail 0x180006320 / profile 0x1800234e0
// and by the four readers:
//   Factory::Startup            0x1800c0630  reads +0x08 (CRC source), +0x10
//                                            (parent lookup), writes +0x00
//   Factory::CreateObject       0x1800c0ae0  calls +0x20
//   Factory::CreateObject(id,p) 0x1800c0aa0  calls +0x28
//   Factory::GetTypeName        0x1800c0a60  returns +0x08
//   Factory::GetTypeIdForFileExtension 0x1800c04c0  _stricmp's +0x18
// ============================================================
struct BaseTypeInfo {
    // +0x00 The CRC type id. The ctor writes 0xFFFFFFFF here; ONLY
    //       Factory::Startup overwrites it with the real id. Anything that
    //       reads a type id before Startup gets the sentinel — this is the
    //       trap behind the FillComponentTypeIdGlobals_* ordering comments in
    //       code/Game/Game/game_main.cpp.
    uint32_t typeId;
    uint32_t _pad04;          // never written by the ctor; not padding the
                              // compiler inserted, just an unused word.

    // +0x08 The class's own name, e.g. "CitizenComponent". Stored verbatim.
    //       This is the string Factory::Startup CRCs into the type id.
    const char* name;

    // +0x10 The parent class's name, e.g. "Component". Stored with any
    //       namespace prefix REMOVED: the ctor scans backwards from the end
    //       for ':' and keeps everything after it, so "Object::Base" is
    //       stored as "Base". Only this field is stripped — `name` is not,
    //       which is why Startup's parent lookup (CRC of the stripped parent)
    //       can match a child's unstripped `name`.
    const char* parentName;

    // +0x18 The file extension this type loads from, or nullptr.
    //       ***THIS IS NOT AN "abstract" FLAG OR NAME.*** It is compared with
    //       _stricmp against a caller-supplied extension in
    //       Factory::GetTypeIdForFileExtension (0x1800c04c0). Every
    //       Component/Description registration in the exe passes nullptr here
    //       (see CitizenComponent_RegisterType, exe 0x14014e7b5); the types
    //       that pass a real string are the Externals loaded from disk.
    //       Our own tree called this field `abstract` in three places until
    //       Wave 3 lane A; see WAVE3_LANE_A.md.
    const char* fileExtension;

    // +0x20 `Base* (*)()` — allocates and constructs an instance.
    //       Factory::CreateObject(id) tail-jumps here.
    void* factoryFn;

    // +0x28 `Base* (*)(void* storage)` — placement-constructs into `storage`.
    //       Factory::CreateObject(id, void*) tail-jumps here.
    void* inPlaceFactoryFn;

    // +0x30 Intrusive singly-linked list. The ctor head-inserts:
    //       this->next = _firstTypeInfo; _firstTypeInfo = this;
    BaseTypeInfo* next;
};
static_assert(offsetof(BaseTypeInfo, typeId)           == 0x00, "BaseTypeInfo::typeId");
static_assert(offsetof(BaseTypeInfo, name)             == 0x08, "BaseTypeInfo::name");
static_assert(offsetof(BaseTypeInfo, parentName)       == 0x10, "BaseTypeInfo::parentName");
static_assert(offsetof(BaseTypeInfo, fileExtension)    == 0x18, "BaseTypeInfo::fileExtension");
static_assert(offsetof(BaseTypeInfo, factoryFn)        == 0x20, "BaseTypeInfo::factoryFn");
static_assert(offsetof(BaseTypeInfo, inPlaceFactoryFn) == 0x28, "BaseTypeInfo::inPlaceFactoryFn");
static_assert(offsetof(BaseTypeInfo, next)             == 0x30, "BaseTypeInfo::next");
static_assert(sizeof(BaseTypeInfo)                     == 0x38, "sizeof(BaseTypeInfo)");

// ============================================================
// Object::Factory's per-type record — the VALUE half of a tree node, 0x28
// bytes. The profile build calls its first field `_type`.
//
// Built on the stack by Factory::Startup (0x1800c0630) as
//     { typeId, info, nullptr, {nullptr, 0, 0} }
// and copied into the node by the Set insert helper (retail 0x180030140,
// which copies +0x00, +0x08, +0x10 and then copy-constructs the Array at
// +0x18). Startup's second pass fills `parent` and pushes each child into its
// parent's `children` Array.
//
// `children` is `Collection::Array<TypeRecord*>` — the 16-byte
// {_data, _size:i32, _capacity:i32} triple from WAVE2_COLLECTION.md, and the
// growth code in Startup (capacity*3 on any growth after the first) is
// visible at 0x1800c07d0. Each stored element is a `TypeRecord*`, i.e. a
// pointer to another node's +0x08 — NOT a node pointer.
// ============================================================
struct TypeRecord {
    uint32_t    typeId;        // +0x00  profile: `_type`
    uint32_t    _pad04;
    BaseTypeInfo* info;        // +0x08  the static this record was built from
    TypeRecord* parent;        // +0x10  base class's record, or nullptr at a root
    TypeRecord** children;     // +0x18  Array<TypeRecord*>::_data
    int32_t     childCount;    // +0x20  Array<TypeRecord*>::_size
    int32_t     childCapacity; // +0x24  Array<TypeRecord*>::_capacity
};
static_assert(offsetof(TypeRecord, typeId)        == 0x00, "TypeRecord::typeId");
static_assert(offsetof(TypeRecord, info)          == 0x08, "TypeRecord::info");
static_assert(offsetof(TypeRecord, parent)        == 0x10, "TypeRecord::parent");
static_assert(offsetof(TypeRecord, children)      == 0x18, "TypeRecord::children");
static_assert(offsetof(TypeRecord, childCount)    == 0x20, "TypeRecord::childCount");
static_assert(offsetof(TypeRecord, childCapacity) == 0x24, "TypeRecord::childCapacity");
static_assert(sizeof(TypeRecord)                  == 0x28, "sizeof(TypeRecord)");

// ============================================================
// A node of Factory's AVL tree: Set<Pair<uint32_t, TypeRecord>, …>::Node.
//
// MEASURED offsets, each from a specific instruction:
//   +0x00 key    — `MOV ECX,[RAX]` then CMP in every lookup
//                  (CreateObject 0x1800c0ae8, GetTypeIdFromName 0x1800c0596,
//                   IsDerivedFrom 0x1800c03b8)
//   +0x08 value  — `ADD RAX,0x8` in IsDerivedFrom (0x1800c03d5) before the
//                  base-chain walk; `[RAX+0x10]` = value.info in CreateObject
//   +0x30 parent — Startup's and GetTypeIdForFileExtension's in-order
//                  successor walk (`puVar+0xc`)
//   +0x38 left   — taken when key > wanted
//   +0x40 right  — taken when key < wanted
//
// MEASURED 2026-09-08 by Wave 9 lane COLL, which DOES insert, rebalance and
// free these nodes. Both facts lane A recorded as guesses are now pinned by an
// instruction, and one of them was WRONG AS SPELLED:
//
//   sizeof == 0x50   0x180008440 `MOV EDX,0x50 / LEA R8D,[RDX-0x48]`
//                    -> Allocate(0x50, align 8)
//   +0x48 is TWO 16-BIT FIELDS, not one int32:
//                    0x180030540 `CMP word ptr [RDX+0x48],-1 / JGE`  -> a
//                        SIGNED 16-bit BALANCE at +0x48
//                    0x180030540 `MOVZX ...,word ptr [RAX+0x4a] / INC` -> a
//                        16-bit HEIGHT at +0x4a
//                    0x1800087c0 `MOV dword ptr [RDI+0x48],ESI` zeroes BOTH in
//                        one dword store, which is why an int32 view of +0x48
//                        looked right for as long as nobody rebalanced.
//
// The declaration below keeps `int32_t balance` so every existing reader still
// compiles; runtime/src/collection/avl_trees.cpp reads the two words through
// Balance50()/Height50() at the raw offsets. The old spelling is left visible
// here rather than silently replaced, so a reader who meets it elsewhere
// recognises it. It is now safe to allocate one of these from this declaration
// (sizeof is asserted against 0x50 in avl_trees.cpp).
// ============================================================
struct TypeNode {
    uint32_t    key;      // +0x00  duplicate of value.typeId; asserted equal
                          //        by the profile build
    uint32_t    _pad04;
    TypeRecord  value;    // +0x08 .. +0x2f
    TypeNode*   parent;   // +0x30
    TypeNode*   left;     // +0x38
    TypeNode*   right;    // +0x40
    int32_t     balance;  // +0x48  GUESS — see above
    int32_t     _pad4c;
};
static_assert(offsetof(TypeNode, key)    == 0x00, "TypeNode::key");
static_assert(offsetof(TypeNode, value)  == 0x08, "TypeNode::value");
static_assert(offsetof(TypeNode, parent) == 0x30, "TypeNode::parent");
static_assert(offsetof(TypeNode, left)   == 0x38, "TypeNode::left");
static_assert(offsetof(TypeNode, right)  == 0x40, "TypeNode::right");

// ============================================================
// Object::Factory — only the tree root is pinned.
//
// Every lookup starts with `MOV RAX,[RCX]`, so the Set's root pointer is at
// +0x00. The Set's element count and allocator, and everything else Factory
// owns (the compiler list behind LoadCompilers/UnloadCompilers), are NOT
// modelled: nothing in lane A's scope touches them.
//
// The singleton lives at Runtime 0x1801fc6e8 and IS an exported data symbol,
// `?_singleton@Factory@Object@@0PEAV12@EA`. PLAN.md §2.3 lists that address as
// "Factory singleton (LoadDialogResource)" among globals that "have no
// export"; both halves of that are wrong.
// ============================================================
struct Factory {
    TypeNode* root;   // +0x00
};
static_assert(offsetof(Factory, root) == 0x00, "Factory::root");

} // namespace RtObject
