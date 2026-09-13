// ============================================================================
// runtime/src/object/type_system.cpp — Wave 3, lane A — Base / Type / BaseTypeInfo / Factory
// ============================================================================
//
// Byte-faithful ports of the six Object reflection exports our tree calls
// (docs/runtime_port/call_surface_20260906.csv). Retail addresses below are in
// Runtime-x64.dll (image base 0x180000000); profile addresses are in
// Runtime-x64-profile.dll, the ToolKit build, whose asserts and source paths
// are what named the fields. Both were read in Ghidra on 2026-09-06.
//
//   ?IsA@Base@Object@@QEBA_NI@Z                       retail 0x1800491c0  profile 0x1800828f0
//   ?IsDerivedFrom@Base@Object@@QEBA_NI@Z             retail 0x1800c0390  profile 0x18008c670
//   ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z
//                                                     retail 0x180006320  profile 0x1800234e0
//   ?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z  retail 0x1800c0ae0  profile 0x18008cde0
//   ?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z  retail 0x1800c0580  profile 0x18008cbe0
//   ?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z retail 0x1800c0c40
//
// Source of truth for the layouts: runtime/types/object/TypeInfo.h. Read it
// before this file.
//
// STATE. Five of the six are read-only walks of the LIVE DLL structures at
// their real addresses; nothing here keeps a replica (rt_object_port.cpp's
// state warning). The sixth, BaseTypeInfo's constructor, MUTATES the DLL
// global `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` (0x1801fc6e0) by
// head-inserting into it, and it runs interleaved with the DLL's own static
// initialisers. It therefore reads and writes that exact global, never a copy.
//
// NO ADDED NULL GUARDS. Retail's lookups deref the found node unconditionally
// and fault when a type id is not registered — CreateObject at 0x1800c0b03
// (`MOV RCX,[0+0x10]`) and IsDerivedFrom at 0x1800c03e0 (`CMP dword [8],EBX`,
// after `ADD RAX,8` on a null). That is load-bearing: it is how a missing
// registration announces itself, and it is what
// standalone/main.cpp's External::Load handler is written against. The ports
// below reproduce it (CLAUDE.md, "Don't 'fix' a faithful crash with a null
// check"). The one consequence to know: the fault now happens in OUR code, at
// an address in reimpl-x64.exe, not at Runtime+0xC03E0.
//
// DIFFABILITY — what each site can and cannot prove is documented at the site.
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../types/object/TypeInfo.h"
#include "../../funcs/rt_port.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, whose <wingdi.h> declares a function called
// `Rectangle` and whose <windef.h> #defines `near`/`far` away. This declaration
// must stay byte-identical to resolve.h's and to the copy in
// runtime/src/collection/array.cpp; the linker checks all three.
//
// Added by Wave 9 lane OBJ2 for Factory::Startup's ONE unexported callee, the
// Collection::Set insert at 0x180030140.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtObject {

// ============================================================
// DLL globals, resolved once by their exported data symbols.
//
// These are addresses of VARIABLES, not of the things they point at:
// `_firstTypeInfo` is a `BaseTypeInfo*` cell, `_singleton` is a `Factory*`
// cell. `_crcTable` is the array itself.
// ============================================================
namespace {

BaseTypeInfo** g_firstTypeInfo = nullptr;   // ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA  0x1801fc6e0
Factory**      g_factorySingleton = nullptr;// ?_singleton@Factory@Object@@0PEAV12@EA           0x1801fc6e8

void ResolveGlobals() {
    if (!g_firstTypeInfo) {
        g_firstTypeInfo = reinterpret_cast<BaseTypeInfo**>(
            ResolveExport("?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA"));
    }
    if (!g_factorySingleton) {
        g_factorySingleton = reinterpret_cast<Factory**>(
            ResolveExport("?_singleton@Factory@Object@@0PEAV12@EA"));
    }
}

// ---- CRC-32, the type-id hash -------------------------------------------
//
// Retail's ?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z (0x1800c0930)
// widens the ASCII name to UTF-16LE on the stack and CRC-32s the resulting
// BYTES, so the id of "Base" is the CRC of "B\0a\0s\0e\0". The table it uses is
// System::CRC::_crcTable (exported at 0x1801f9080). All 256 entries of that
// table were compared against the textbook reflected-CRC-32 table
// (polynomial 0xEDB88320) and are IDENTICAL, so we generate it rather than
// reading the DLL's — PLAN.md §4.1 decision 1 wants ported code to survive
// cutover, and a table we can regenerate does.
//
//     python: t[i] = fold8(i, 0xEDB88320);  compared byte-for-byte with
//     pefile.get_data(0x1f9080, 1024)  ->  identical: True
uint32_t g_crcTable[256];
bool     g_crcTableBuilt = false;

void BuildCrcTable() {
    if (g_crcTableBuilt) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
        g_crcTable[i] = c;
    }
    g_crcTableBuilt = true;
}

// Faithful transcription of GetTypeFromNameCRC's body. The 264-byte stack
// buffer is retail's own (132 wide characters); a longer type name overflows
// it in retail exactly as it does here, and no bound check is added because
// adding one would change behaviour retail relies on never needing.
uint32_t TypeNameCRC(const char* name) {
    BuildCrcTable();

    uint8_t wide[264];
    int len = 0;
    for (const char* p = name; *p; ++p) ++len;
    for (int i = 0; i < len; ++i) {
        // (short)param_1[i] in the decompilation: MSVC's `char` is signed, so
        // this sign-extends. Irrelevant for the ASCII type names the engine
        // registers, spelled out so nobody has to rediscover it.
        const int16_t w = static_cast<int16_t>(static_cast<signed char>(name[i]));
        wide[i * 2 + 0] = static_cast<uint8_t>(static_cast<uint16_t>(w) & 0xFFu);
        wide[i * 2 + 1] = static_cast<uint8_t>((static_cast<uint16_t>(w) >> 8) & 0xFFu);
    }

    uint32_t crc = 0xFFFFFFFFu;
    const int nbytes = len * 2;
    for (int i = 0; i < nbytes; ++i)
        crc = g_crcTable[(crc & 0xFFu) ^ wide[i]] ^ (crc >> 8);
    return ~crc;   // len == 0 leaves crc at 0xFFFFFFFF, so the id is 0
}

// The tree walk shared by CreateObject, GetTypeIdFromName, GetTypeName and
// the first half of IsDerivedFrom. Identical instruction sequence in all four
// (e.g. 0x1800c0ae8..0x1800c0b01): descend left when the node's key is
// greater, right when smaller, and yield nullptr on a miss.
TypeNode* FindNode(const Factory* factory, uint32_t typeId) {
    TypeNode* n = factory->root;
    while (n) {
        if (typeId < n->key)      n = n->left;
        else if (typeId > n->key) n = n->right;
        else                      return n;
    }
    return nullptr;
}

// vtable slot 2 (+0x10) — `virtual uint32_t GetTypeId() const`.
inline uint32_t GetTypeIdVirtual(const void* self) {
    void* const* vt = *reinterpret_cast<void* const* const*>(self);
    return reinterpret_cast<uint32_t (*)(const void*)>(vt[2])(self);
}

// ---- Raw ABI typedefs for the originals ----------------------------------
// None of the six returns a class by value, so none has a hidden return slot
// (checked against each mangled name: `_N` bool, `I` unsigned, `PEAVBase` and
// `PEAV12` pointers, `X` void). The BaseTypeInfo ctor DOES return `this` in
// RAX — MSVC always does — even though the mangling says the ctor returns
// void; engine/funcs/engine_funcs.h declares it `void` and that is harmless
// for a caller that ignores RAX, but the port must still produce it.
typedef bool          (*fn_IsA_t)(const void* self, uint32_t typeId);
typedef bool          (*fn_IsDerivedFrom_t)(const void* self, uint32_t typeId);
typedef void*         (*fn_CreateObject_t)(const void* factory, uint32_t typeId);
typedef uint32_t      (*fn_GetTypeIdFromName_t)(const void* factory, const char* name, bool existingOnly);
typedef const char*   (*fn_GetTypeName_t)(const void* factory, uint32_t typeId);
typedef BaseTypeInfo* (*fn_BaseTypeInfoCtor_t)(BaseTypeInfo*, const char*, const char*,
                                               const char*, void*, void*);

const char* const kNameIsA           = "?IsA@Base@Object@@QEBA_NI@Z";
const char* const kNameIsDerivedFrom = "?IsDerivedFrom@Base@Object@@QEBA_NI@Z";
const char* const kNameCreateObject  = "?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z";
const char* const kNameGetTypeIdFromName = "?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z";
const char* const kNameGetTypeName   = "?GetTypeName@Factory@Object@@QEBAPEBDI@Z";
const char* const kNameTypeSerialize = "?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z";
const char* const kNameBaseTypeInfoCtor =
    "??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z";

} // namespace

// ============================================================================
// Wave 10 lane MINTSEL: the ONE exported spelling of the type-id hash.
//
// `TypeNameCRC` above is the faithful transcription of retail's private
// ?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z, and it stays in the
// anonymous namespace where it was written. This thin wrapper exists because
// standalone/rt_data_mint.cpp needs THE SAME hash to stamp a minted
// BaseTypeInfo's `typeId` with the value Factory::Startup would stamp -- and
// the one thing that must NOT happen is a second, independently written copy
// of a CRC that agrees on today's names and diverges on tomorrow's.
//
// Startup's own line is `info->typeId = TypeNameCRC(info->name)`
// (this file, in FactoryStartupRun); a minted node calling this wrapper with
// the same `name` is that line, moved earlier. It is not a new claim about
// retail: it is the existing claim, called from one more place.
// ============================================================================
uint32_t TypeIdFromName(const char* name) { return TypeNameCRC(name); }

// ============================================================================
// ?IsA@Base@Object@@QEBA_NI@Z            retail 0x1800491c0 / profile 0x1800828f0
//
//     bool Base::IsA(unsigned int typeId) const
//     { return typeId == GetTypeId(); }
//
// An EXACT-type test, not a derivation test. Two instructions of substance.
//
// DIFF: full oracle. `GetTypeId` is a virtual, so diffing calls it twice —
// which is free here because every implementation of that slot in this engine
// is `MOV EAX,[rip+global]; RET` (see TypeInfo.h). If a future subclass gives
// slot 2 a side effect, this site becomes unsound; nothing does today.
// ============================================================================
bool RtObject_Base_IsA(const void* self, uint32_t typeId) {
    const bool ours = (typeId == GetTypeIdVirtual(self));
    return ours;
}

// ============================================================================
// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z  retail 0x1800c0390 / profile 0x18008c670
//
// Retail is `Factory::IsDerivedFrom(_singleton, this->GetTypeId(), typeId)`
// inlined (the profile build shows the call unwrapped, plus two asserts that
// retail compiles out). The walk:
//
//     if (GetTypeId() == typeId) return true;
//     record = FindNode(_singleton, GetTypeId())->value;   // faults on a miss
//     while (record->typeId != typeId) {
//         record = record->parent;
//         if (!record) return false;
//     }
//     return true;
//
// The base chain is the `parent` links Factory::Startup filled in its second
// pass, NOT the `_firstTypeInfo` list.
//
// ORDER MATTERS AND IS PRESERVED: retail loads `_singleton` into RDI BEFORE
// making the virtual GetTypeId call (0x1800c039a-0x1800c03a6). Harmless today,
// kept because it is what the disassembly says.
//
// This is by far the hottest function in lane A — several hundred call sites
// in code/Game/Action alone go through `fn_IsDerivedFrom`.
//
// DIFF: full oracle. Read-only on both sides.
// ============================================================================
bool RtObject_Base_IsDerivedFrom(const void* self, uint32_t typeId) {
    ResolveGlobals();

    const Factory* factory = *g_factorySingleton;
    const uint32_t ownId   = GetTypeIdVirtual(self);

    bool ours;
    if (ownId == typeId) {
        ours = true;
    } else {
        // Retail's `ADD RAX,0x8` happens on the found pointer WHETHER OR NOT
        // it is null, so a missing registration reads address 0x8 and faults.
        // Reproduced exactly; see the header comment.
        TypeNode* n = FindNode(factory, ownId);
        const TypeRecord* rec =
            reinterpret_cast<const TypeRecord*>(reinterpret_cast<const char*>(n) + 8);
        for (;;) {
            if (rec->typeId == typeId) { ours = true; break; }
            rec = rec->parent;
            if (!rec) { ours = false; break; }
        }
    }

    return ours;
}

// ============================================================================
// ?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z
//                                        retail 0x1800c0580 / profile 0x18008cbe0
//
//     uint32_t Factory::GetTypeIdFromName(const char* name, bool existingOnly) const
//
// CRC the name, then confirm the id is in the tree; return the id if it is and
// 0 if it is not.
//
// `existingOnly` IS UNUSED IN RETAIL. The retail disassembly never touches R8
// (0x1800c0580..0x1800c05b6). The profile build shows why: it guards an assert,
//     "!existingOnly || IsTypeValid(type)"  "Type is not registered!"
//     Object\Factory.cpp:0x59
// which retail compiles out. Every one of our call sites passes `true`. The
// parameter is accepted and ignored, deliberately, and this comment is the
// reason it is not "cleaned up".
//
// DIFF: full oracle.
// ============================================================================
uint32_t RtObject_Factory_GetTypeIdFromName(const void* factory, const char* name,
                                            bool existingOnly) {
    (void)existingOnly;   // retail-unused; see above

    const uint32_t crc = TypeNameCRC(name);
    const uint32_t ours =
        FindNode(static_cast<const Factory*>(factory), crc) ? crc : 0u;

    return ours;
}

// ============================================================================
// ?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z   retail 0x1800c0930, ord 2989
//
//     unsigned int Factory::GetTypeFromNameCRC(char const* name) const
//
// BATCH 5 (2026-09-12). *** THE BODY WAS ALREADY HERE AND WAS NOT REACHABLE. ***
// `TypeNameCRC` above is this function, transcribed by Wave 3 lane A as a
// private helper for GetTypeIdFromName — but no entry point carried the retail
// ABI and nothing registered the address, so `0x1800c0930` stayed on the
// closure worklist as an unported engine function while its algorithm sat
// twenty lines up. This adds the ABI shim and the two doors; it does NOT add a
// second copy of the algorithm, which would be a place for the two to drift.
//
// `this` IS READ BY NOTHING. Retail's RCX is dead from the first instruction
// (the body opens `LEA RAX,[RSP+0x...]` and never touches RCX again); the CRC
// depends only on the name. The parameter is kept because the mangling is a
// const member and the caller passes it.
//
// `AEBA` is PRIVATE, and private is not unexported: this DLL exports its
// private members and the name above is ordinal 2989 in the retail export
// directory, verified with pefile before this was written
// (reference_aeaa_is_private_not_unexported.md). So the NAME door is real and
// is registered alongside the address door.
//
// NO DIFF SITE. A `the diff oracle` here would compare `TypeNameCRC` against
// the shipped 0x1800c0930 — but `RtObject_Factory_GetTypeIdFromName`'s own
// site already compares a value DERIVED from exactly this computation against
// the shipped GetTypeIdFromName on every call, which is a strictly stronger
// check on the same arithmetic and has run clean since Wave 3. Adding a second
// site on the same bytes would inflate `calls=` without adding coverage.
// ============================================================================
uint32_t RtObject_Factory_GetTypeFromNameCRC(const void* /*factory*/, const char* name) {
    return TypeNameCRC(name);
}

// ============================================================================
// ?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z
//                                        retail 0x1800c0ae0 / profile 0x18008cde0
//
//     Base* Factory::CreateObject(unsigned int typeId) const
//     { return FindNode(typeId)->value.info->factoryFn(); }
//
// A tail-jump through BaseTypeInfo+0x20. The sibling overload CreateObject(id,
// void*) jumps through +0x28 instead; it is not in lane A's scope (our tree
// does not call it) and is NOT ported.
//
// DIFF: PARTIAL, and this is the honest limit of it. Running the original as
// an oracle would construct a SECOND object and leak it, so the site does not.
// What it does instead is a real independent check of the only part we
// reimplement — the tree walk — by asking the DLL's own
// `Factory::GetTypeName(id)` (0x1800c0a60), which returns
// `FindNode(id)->value.info->name` from its own walk, and comparing that
// pointer with the one our walk produced. If our walk lands on a different
// node, the two names differ. It proves nothing about the factory dispatch,
// which is one indirect call through a pointer we did not compute.
// ============================================================================
void* RtObject_Factory_CreateObject(const void* factory, uint32_t typeId) {

    TypeNode* n = FindNode(static_cast<const Factory*>(factory), typeId);
    // Faithful: retail's `MOV RCX,[RAX+0x10]` at 0x1800c0b03 runs on a null
    // RAX and faults. No guard.
    const TypeRecord* rec =
        reinterpret_cast<const TypeRecord*>(reinterpret_cast<const char*>(n) + 8);
    BaseTypeInfo* info = rec->info;


    return reinterpret_cast<void* (*)()>(info->factoryFn)();
}

// ============================================================================
// ?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z      retail 0x1800c0c40
//
//     void Type::Serialize(IO::Serializer& s)
//     { s._readerWriter->vtable[0x98](s._readerWriter, this, 0); }
//
// Six instructions, all dispatch. `Object::Type` is a 4-byte wrapper around a
// type id (`??BType@Object@@QEBAIXZ` is its `operator unsigned int`), and this
// serializes that id as a plain uint through the ReaderWriter's +0x98 slot.
//
// Serializer+0x08 is the `IO::ReaderWriter*`; pinned by the profile build's
// Achievement::Serialize (0x180022880), where the local at Serializer+8 is the
// object every `AllowCustomSerialize` / slot-0xf8 / slot-0x100 call is made on.
// Serializer+0x00 is the `IO::Stream*`. Those two offsets are all this needs;
// IO::Serializer itself is Wave 4 and is NOT modelled here.
//
// NOT A FORWARDER. This is a real transcription — the object it dispatches on
// is the DLL's ReaderWriter carrying the DLL's vtable, exactly as it is when
// retail runs, so the virtual lands in the DLL either way. That is correct
// until Wave 4 ports ReaderWriter, at which point nothing here changes.
//
// DIFF: NONE IS POSSIBLE. The call mutates the stream; running it twice writes
// the id twice and desyncs every subsequent record — the exact failure mode of
// reference_slot3_wrapper_must_resolve_first.md. The site below registers so
// the run reports a real `calls=` count, and its verdict is the only invariant
// available without a stream: that the ReaderWriter and its vtable are
// non-null. Read it as "this port executed N times", NOT as "this port agrees
// with the DLL N times".
// ============================================================================
void RtObject_Type_Serialize(void* self, void* serializer) {

    void*  rw = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void** vt = *reinterpret_cast<void***>(rw);


    reinterpret_cast<void (*)(void*, void*, int)>(vt[0x98 / 8])(rw, self, 0);
}

// ============================================================================
// ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z
//                                        retail 0x180006320 / profile 0x1800234e0
//
//     BaseTypeInfo::BaseTypeInfo(const char* name, const char* parentName,
//                                const char* fileExtension,
//                                Base* (*factory)(), Base* (*inPlace)(void*))
//
// Stores the five arguments, stamps typeId = 0xFFFFFFFF, head-inserts into
// `_firstTypeInfo`, then rewrites `parentName` to point past the LAST ':' it
// contains (so "Object::Base" becomes "Base"). Only the parent is stripped.
//
// Retail's backward scan, transcribed exactly (0x180006381-0x18000639b):
// it starts at the NUL terminator, walks down while `q > p`, and when the loop
// falls out at `q == p` it tests that byte once more before giving up. An
// empty parent string therefore still gets its single (NUL) byte tested.
//
// THE ONE MUTATING PORT IN THIS LANE. It writes a DLL global, and it runs
// interleaved with the DLL's own static initialisers (which have already run
// by the time RuntimePortInit registers us, so the chain we insert into is
// already populated) and with ours, from
// code/Game/Object/factory_registration.cpp's InvokeAllRegistrations.
//
// DIFF: REAL, but it needs the list put back. Running the original as-is would
// link a second node. Instead the site runs OURS for real, then rewinds
// `_firstTypeInfo` to the value it had on entry, runs the ORIGINAL into a
// private scratch BaseTypeInfo, restores `_firstTypeInfo` to what our port
// left, and compares all seven fields. The DLL's ctor touches nothing else —
// the disassembly at 0x180006320 is 21 instructions and the only global it
// reads or writes is `_firstTypeInfo` — so the rewind is complete, not
// approximate. This runs only under the old DLL-era switch and only during static
// registration, which is single-threaded.
// ============================================================================
BaseTypeInfo* RtObject_BaseTypeInfo_Ctor(BaseTypeInfo* self,
                                         const char* name,
                                         const char* parentName,
                                         const char* fileExtension,
                                         void* factoryFn,
                                         void* inPlaceFactoryFn) {
    ResolveGlobals();

    BaseTypeInfo* const prevHead = *g_firstTypeInfo;

    self->name             = name;
    self->parentName       = parentName;
    self->factoryFn        = factoryFn;
    self->typeId           = 0xFFFFFFFFu;
    self->fileExtension    = fileExtension;
    self->inPlaceFactoryFn = inPlaceFactoryFn;
    self->next             = prevHead;
    *g_firstTypeInfo       = self;

    if (self->parentName) {
        const char* p = self->parentName;
        int len = 0;
        for (const char* s = p; *s; ++s) ++len;
        const char* q = p + len;
        while (q > p) {
            if (*q == ':') { ++q; goto stored; }
            --q;
        }
        if (*q == ':') ++q;
    stored:
        self->parentName = q;
    }


    return self;   // MSVC returns `this` from every ctor; so does retail (RAX=R10)
}

// ============================================================================
// ?Startup@Factory@Object@@QEAA_NXZ      retail 0x1800c0630  ord 4096
//                                        (Wave 9 lane OBJ2, 2026-09-08)
//
// STEP 6 OF Engine::Startup's QUEUE, and the reflection system's own bring-up:
// it is the ONLY code that overwrites `BaseTypeInfo::typeId`'s 0xFFFFFFFF ctor
// sentinel, and the only code that builds the AVL tree every later lookup
// walks (CreateObject, GetTypeIdFromName, IsDerivedFrom, GetTypeName,
// GetTypeIdForFileExtension). Called once per process from
// runtime/src/engine/engine_lifecycle.cpp:612.
//
// TWO PASSES over 212 instructions:
//
//   1. Walk `_firstTypeInfo` (0x1801fc6e0). For each entry, CRC-32 its `name`
//      into `typeId`, then insert {typeId, info, nullptr, {}} into the tree
//      keyed by typeId. The CRC is the SAME algorithm as
//      Factory::GetTypeFromNameCRC (0x1800c0930) — retail has it twice, once
//      inline here and once out of line there — so this reuses TypeNameCRC
//      above rather than transcribing it a second time.
//
//   2. In-order walk of the tree just built. For each node whose `info` has a
//      `parentName`, CRC that name, look the parent's node up, store
//      `&parentNode->value` as this record's `parent`, and push
//      `&thisNode->value` onto the parent's `children` array.
//
// Pass 2's tail is `Collection::Array<TypeRecord*>::SetSize(n+1, fill)`
// INLINED — it is instruction-for-instruction the J3 body in
// runtime/src/collection/array.cpp (the exact-equal no-op, the
// `capacity = n*3` rule that fires only on a growth after the first, the
// `if (dst == 0) continue`, the copy-then-free). It is transcribed here rather
// than routed through lane Q's `SetSizeCore` because retail inlined it: this
// call site has no `ArraySpec`, no registered address, and a stride and align
// baked in as immediates.
//
// TWO UNGUARDED DEREFS THAT ARE LOAD-BEARING. Both are kept (CLAUDE.md, "Don't
// 'fix' a faithful crash with a null check"):
//
//   * When a `parentName` names a type that is NOT registered, retail's tree
//     search leaves RCX == 0 and `LEA RBP,[RCX+8]` (0x1800c079d) produces
//     `(TypeRecord*)8`, which the very next instruction dereferences at
//     +0x20 — a read of address 0x28. That fault IS how a missing parent
//     registration announces itself, and at cutover it is a fault we want,
//     in our own code, with a symbol name on it.
//   * `info->name` is dereferenced by the CRC with no null test.
//
// RETURNS `true` UNCONDITIONALLY — `MOV AL,1` at 0x1800c0907 is the only exit.
// The `if (!Startup(factory)) { g_startupFailAt = 5; return 0; }` at
// engine_lifecycle.cpp:612 therefore has a branch retail can never take.
//
// DEPENDENCY THIS PORT DID NOT CLOSE — ***CLOSED 2026-09-08 BY WAVE 9 LANE
// COLL.*** The Set insert at 0x180030140 and its whole chain are now ported in
// runtime/src/collection/avl_trees.cpp and registered by address, so this body
// reaches OUR insert through the same port-aware door with no edit here and
// THIS ROW NOW CLOSES AT CUTOVER. The diagnostic below is kept: it is the
// early-warning for a registration that failed to arm, not dead code.
//
// The chain, as this file predicted it, and what it actually turned out to be
// (kept side by side because the prediction was good and the two corrections
// are worth having):
//     0x180030140  Set::Insert wrapper (builds the Pair, calls the core,
//                  destroys the temp Array)      ~25 instrs
//     0x180030380  insert core: BST descend + node alloc + retrace   ~50
//     0x180008440  node allocate (0x50 bytes, align 8) + Pair ctor   ~25
//     0x1800087c0  Pair placement-construct
//     0x180030540  AVL retrace, calling the two rotations
//     0x1800307b0 / 0x1800308e0  the two rotations
//     0x180009350 / 0x1800093e0  Array copy-ctor / dtor for the temp
// Nothing else in this file needs them; they are a Collection::Set lane.
// ============================================================================

namespace {

// ---- The allocator boundary ----------------------------------------------
//
// Retail INLINES `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50)
// and `?MemFree@...` (0x1800c7ad0) here rather than calling them:
// 0x1800c07e5..0x1800c0826 is Enter(&_singleton->cs) / Allocate(_singleton,
// bytes, 8) / Leave, which is 0x1800c7a50's whole body with align=8, and
// 0x1800c0873..0x1800c08a6 is 0x1800c7ad0's body with retail's `if (p)` hoisted
// to 0x1800c086e. So calling the two wrappers is the same instruction sequence,
// and it goes through the door lane E's allocator port will own instead of
// pinning 0x1800c6e60 / 0x1800c6c00 / the +0x640 lock offset in a third place.
typedef void* (*FactoryMemAllocFn)(uint64_t, int);
typedef void  (*FactoryMemFreeFn)(void*);

void* FactoryMemAlloc(uint64_t bytes, int align) {
    return RT_EXPORT_ONCE(FactoryMemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(bytes, align);
}
void FactoryMemFree(void* p) {
    // The `if (p)` lives INSIDE 0x1800c7ad0 (`TEST RCX,RCX; JZ ret` is its
    // first instruction). The one call site below still tests, because retail
    // does too, at 0x1800c086e.
    RT_EXPORT_ONCE(FactoryMemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// ---- MEASURED CORRECTIONS to two layouts TypeInfo.h records as guesses -----
//
// `Object::Factory` is 0x10 bytes, not 8. `??0Factory@Object@@QEAA@XZ`
// (0x1800c09d0) is: `[RCX]=0; [RCX+8]=0(dword); _singleton=RCX; return RCX`,
// and the Set insert core INCs that dword at 0x1800303ac / 0x18003043c on
// every successful insert. It is the Set's element count. TypeInfo.h's
// `struct Factory { TypeNode* root; }` is CORRECT AS A VIEW — every lookup
// only needs +0x00 — but it is 8 bytes, so it must never be used as STORAGE.
// The probe below allocates one, hence this declaration.
struct FactorySet {
    TypeNode* root;    // +0x00
    uint32_t  count;   // +0x08
    uint32_t  _pad0c;  // +0x0c  zeroed by the ctor's qword store? no: the ctor
                       //        writes a DWORD at +8 only. Never read.
};
static_assert(sizeof(FactorySet) == 0x10, "Object::Factory is MemAlloc(0x10,4)");

// `TypeNode` is 0x50 bytes and its +0x48 is TWO 16-bit fields, not one int32.
// TypeInfo.h marks both facts "GUESS, NOT MEASURED"; here is the measurement:
//   0x180008440  MOV EDX,0x50 / LEA R8D,[RDX-0x48]  -> Allocate(0x50, align 8)
//   0x180030540  MOVZX ...,word [RAX+0x4a] / INC    -> +0x4a is a HEIGHT
//                MOV word [RDX+0x48],CX             -> +0x48 is a SIGNED
//                CMP word [RDX+0x48],-1 / JGE       -> ...balance factor
// Nothing in this file allocates or rebalances a node, so nothing here relies
// on them; they are recorded because the Set lane above will.

// ---- Tree walking ---------------------------------------------------------

TypeNode* LeftMost(TypeNode* n) {
    // Retail 0x1800c0747: `while ((RAX = node->left)) node = RAX;`
    if (!n) return nullptr;
    while (n->left) n = n->left;
    return n;
}

// Retail's in-order successor, 0x1800c08ac..0x1800c08f6, transcribed branch for
// branch. The `!p` arm reads `node->parent` a second time to produce its null;
// that is retail's 0x1800c08f2 shared exit, not a redundancy to tidy away.
TypeNode* Successor(TypeNode* node) {
    if (TypeNode* r = node->right)
        return LeftMost(r);

    TypeNode* p = node->parent;
    if (!p)                    return node->parent;   // nullptr, via 0x1800c08f2
    if (p->left == node)       return p;              // via 0x1800c08d2
    while (p && p->right == node) { node = p; p = p->parent; }
    return node->parent;
}

// ---- The body, factored so the probe can drive it on a synthetic Factory ---
//
// `self` is a FactorySet, never the 8-byte view. The real entry point passes
// the engine's own factory; the probe passes storage it owns.
void FactoryStartupRun(FactorySet* self) {
    // ---- Pass 1: CRC every name, insert a record per type -----------------
    //
    // `_firstTypeInfo` is read ONCE into RBX at 0x1800c065b and then followed
    // through `info->next`; the loop does not re-read the global, so a
    // BaseTypeInfo constructed DURING Startup would not be seen. Nothing
    // constructs one during Startup, but the port matches the read anyway.
    // The one unexported callee. Resolved through the PORT-AWARE address door,
    // so the day a Collection::Set lane registers 0x180030140 this body picks
    // the port up with no edit. A NULL IS NOT CACHED (rt_resolve_cache.cpp's
    // contract): freezing one here would mean a Set port registered later
    // could never be reached, and nothing would say which symbol was missing.
    typedef TypeNode** (*SetInsertFn)(FactorySet*, TypeNode**,
                                      const uint32_t*, const TypeRecord*);
    static SetInsertFn setInsert = nullptr;
    if (!setInsert)
        setInsert = reinterpret_cast<SetInsertFn>(
            ResolveAddr(Module::Runtime, 0x180030140));
    if (!setInsert) {
        // At cutover this is where the row that is NOT closed announces
        // itself, by name, instead of as EXEC-of-0 with no context.
        printf("[RtPort] Object::Factory::Startup: Collection::Set insert "
                    "0x180030140 is UNPORTED and the DLL is absent -- the type "
                    "tree cannot be built\n");
        return;
    }

    for (BaseTypeInfo* info = *g_firstTypeInfo; info; info = info->next) {
        info->typeId = TypeNameCRC(info->name);

        TypeRecord rec;
        // Retail stamps 0xFFFFFFFF into rec.typeId at 0x1800c06e3 and then
        // overwrites it with info->typeId at 0x1800c0706 — the sentinel is
        // dead. `_pad04` is written by NEITHER retail nor the insert wrapper
        // (which copies +0x00 as a dword, then +0x08, +0x10, then
        // copy-constructs the Array at +0x18), so it is unobservable; it is
        // zeroed here only so this struct has no indeterminate member.
        rec.typeId        = info->typeId;
        rec._pad04        = 0;
        rec.info          = info;
        rec.parent        = nullptr;
        rec.children      = nullptr;
        rec.childCount    = 0;
        rec.childCapacity = 0;

        // R8 and R9 are the SAME address at 0x1800c070f/0x1800c070a: the key
        // IS the record's first field. Passed as two arguments because that is
        // what the callee's signature is.
        TypeNode* it = nullptr;
        setInsert(self, &it, &rec.typeId, &rec);
    }


    // ---- Pass 2: link parents, fill children ------------------------------
    for (TypeNode* node = LeftMost(self->root); node; node = Successor(node)) {
        BaseTypeInfo* const info = node->value.info;
        const char* const parentName = info->parentName;
        if (!parentName) continue;                    // 0x1800c076b

        const uint32_t parentId = TypeNameCRC(parentName);

        // 0x1800c0779..0x1800c079a, the same descend as FindNode above,
        // written out because retail wrote it out.
        TypeNode* p = self->root;
        while (p) {
            if (parentId < p->key)      p = p->left;
            else if (p->key < parentId) p = p->right;
            else                        break;
        }

        // FAITHFUL: `LEA RBP,[RCX+8]` on a null RCX. See the banner — a
        // parentName naming an unregistered type yields (TypeRecord*)8 and
        // faults on the next line, exactly as retail does. uintptr_t
        // arithmetic rather than `(char*)p + 8` only because null pointer
        // arithmetic is UB in C++ and this must be the LEA, not whatever the
        // optimiser concludes from a UB premise.
        // NOT A GUARD (lane TYPE, 2026-09-08). The deref below is unchanged
        // and still faults; this only puts a NAME on the fault before it
        // happens. "READ of 0x0000000000000028 in FactoryStartupRun" with no
        // name is the entire cost of the faithful LEA, and one printf buys it
        // back without altering a single subsequent instruction's inputs.
        if (!p) {
            printf("[RtPort] Factory::Startup: type \"%s\" (id 0x%08X) names "
                        "parent \"%s\" (id 0x%08X), which is NOT in the type "
                        "chain. The faithful LEA below yields (TypeRecord*)8 "
                        "and the next read faults at 0x28 -- as retail does.\n",
                        info->name ? info->name : "(null)", node->value.typeId,
                        parentName, parentId);
            std::fflush(stdout);
        }

        TypeRecord* const parentRec =
            reinterpret_cast<TypeRecord*>(reinterpret_cast<uintptr_t>(p) + 8);
        node->value.parent = parentRec;

        // ---- Array<TypeRecord*>::SetSize(count + 1, &node->value) ---------
        const int64_t oldSize = parentRec->childCount;             // MOVSXD
        const int32_t newSize = static_cast<int32_t>(oldSize) + 1;
        if (newSize == static_cast<int32_t>(oldSize)) continue;    // 0x1800c07b0

        TypeRecord** const old = parentRec->children;
        parentRec->childCount = newSize;
        if (newSize > parentRec->childCapacity) {                  // SIGNED
            parentRec->childCapacity = newSize;
            // The growth rule: triple ONLY when this is a one-element growth
            // of a non-empty array, i.e. never on the first push. A type with
            // two or more children is what exercises it.
            if (newSize == static_cast<int32_t>(oldSize) + 1 && oldSize != 0)
                parentRec->childCapacity = newSize * 3;            // 32-bit LEA
            parentRec->children = static_cast<TypeRecord**>(FactoryMemAlloc(
                static_cast<uint64_t>(
                    static_cast<int64_t>(parentRec->childCapacity)) * 8, 8));
        }

        // 0x1800c082c: start at oldSize when the buffer did NOT move (append
        // only), at 0 when it did (copy then append).
        for (int64_t i = (old == parentRec->children) ? oldSize : 0;
             i < newSize; ++i) {
            TypeRecord** const dst = parentRec->children + i;
            if (!dst) continue;      // retail's TEST RDX,RDX at 0x1800c0849
            *dst = (i < oldSize) ? old[i] : &node->value;
        }

        if (old != parentRec->children && old)                     // 0x1800c086e
            FactoryMemFree(old);
    }
}

}  // namespace

// ============================================================================
// The registered entry point.
//
// DIFF: SHADOW ONLY, and deliberately so. `calls=` will read 1. The real
// verification is the probe above; the shadow here is a structural invariant
// that can be checked without a second implementation — every entry in the
// chain got a typeId that is the CRC of its own name, and the tree has as many
// nodes as the chain had entries. Read the site as "this port executed and its
// own postcondition holds", NOT as "this port agrees with the DLL".
// ============================================================================
bool RtObject_Factory_Startup(void* factory) {
    ResolveGlobals();

    FactoryStartupRun(static_cast<FactorySet*>(factory));


    return true;   // MOV AL,1 at 0x1800c0907 is Startup's only exit
}

} // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
// Both by NAME and by ADDRESS. Half the exe's Object call sites resolve by
// name with an `if (!fn) ResolveAddr(Module::Runtime, addr)` fallback
// (engine/funcs/engine_funcs.cpp:1730, :2697), and ResolveAddr consults its own
// registry — registering only the names would leave any site whose name lookup
// failed silently on the DLL (WAVE2.md finding 4).
//
// All six mangled names were verified present in the shipped DLL with pefile
// before a line of this file was written, so none of them is a
// reference_resolveexport_phantom_names.md case:
//     ?IsA@Base@Object@@QEBA_NI@Z                        ord 3089
//     ?IsDerivedFrom@Base@Object@@QEBA_NI@Z              ord 3110
//     ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z  ord 52
//     ?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z   ord 2128
//     ?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z   ord 2991
//     ?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z ord 3772
// ============================================================================
int RtObjectTypeSystemRegister() {
    using namespace RtObject;
    int n = 0;

    // LITERALS, not the kName* constants above (which are still used for the
    // the raw DLL door lookups). tools/runtime_census.py matches
    // `RegisterRuntimeExport("<literal>"` and cannot see through a named
    // constant, so registering through the constants made these six ports
    // INVISIBLE to the census. Found in Wave 4 when lane D repeated the
    // pattern; the census now warns on it. These six happened to still be
    // counted, because they are ALSO registered by address below and the
    // census takes either -- but that was luck, not design, and a port
    // registered by name only would have vanished silently.
    RegisterRuntimeExport("?IsA@Base@Object@@QEBA_NI@Z",
                          (void*)&RtObject_Base_IsA); ++n;
    RegisterRuntimeExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z",
                          (void*)&RtObject_Base_IsDerivedFrom); ++n;
    RegisterRuntimeExport("?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z",
                          (void*)&RtObject_Factory_GetTypeIdFromName); ++n;
    RegisterRuntimeExport("?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z",
                          (void*)&RtObject_Factory_CreateObject); ++n;
    RegisterRuntimeExport("?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtObject_Type_Serialize); ++n;
    RegisterRuntimeExport("??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z",
                          (void*)&RtObject_BaseTypeInfo_Ctor); ++n;

    // Wave 9 lane OBJ2. `?Startup@Factory@Object@@QEAA_NXZ` (0x1800c0630, ord
    // 4096) verified present in the shipped DLL's export table with pefile on
    // 2026-09-08 before a line of it was written.
    //
    RegisterRuntimeExport("?Startup@Factory@Object@@QEAA_NXZ",
                          (void*)&RtObject_Factory_Startup); ++n;
    RegisterRuntimeAddr(0x1800c0630, (void*)&RtObject_Factory_Startup);

    RegisterRuntimeAddr(0x1800491c0, (void*)&RtObject_Base_IsA);
    RegisterRuntimeAddr(0x1800c0390, (void*)&RtObject_Base_IsDerivedFrom);
    RegisterRuntimeAddr(0x1800c0580, (void*)&RtObject_Factory_GetTypeIdFromName);
    RegisterRuntimeAddr(0x1800c0ae0, (void*)&RtObject_Factory_CreateObject);
    RegisterRuntimeAddr(0x1800c0c40, (void*)&RtObject_Type_Serialize);
    RegisterRuntimeAddr(0x180006320, (void*)&RtObject_BaseTypeInfo_Ctor);

    // Ordinals: engine/funcs/engine_funcs.cpp:4552 falls back to
    // the ordinal door(2991) for GetTypeIdFromName and :4566 to
    // the ordinal door(3089) for IsA when the name lookup fails. Both names DO
    // resolve, so these are belt-and-braces — but an unregistered ordinal
    // fallback is exactly how a port goes silently inert.
    RegisterRuntimeOrdinal(3089, (void*)&RtObject_Base_IsA);
    RegisterRuntimeOrdinal(2991, (void*)&RtObject_Factory_GetTypeIdFromName);

    // BATCH 5 (2026-09-12): the private CRC helper. Both doors — the name is a
    // real export (ordinal 2989) despite the `AEBA` private mangling.
    RegisterRuntimeExport("?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z",
                          (void*)&RtObject_Factory_GetTypeFromNameCRC); ++n;
    RegisterRuntimeAddr(0x1800c0930, (void*)&RtObject_Factory_GetTypeFromNameCRC);

    return n;
}
