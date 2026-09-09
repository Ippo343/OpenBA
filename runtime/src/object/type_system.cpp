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
    RT_DIFF_BEGIN("Object::Base::IsA");
    const bool ours = (typeId == GetTypeIdVirtual(self));
    RT_DIFF_SCALAR(ours, RtOriginal<fn_IsA_t>(kNameIsA)(self, typeId));
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
    RT_DIFF_BEGIN("Object::Base::IsDerivedFrom");
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

    RT_DIFF_SCALAR(ours, RtOriginal<fn_IsDerivedFrom_t>(kNameIsDerivedFrom)(self, typeId));
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
    RT_DIFF_BEGIN("Object::Factory::GetTypeIdFromName");
    (void)existingOnly;   // retail-unused; see above

    const uint32_t crc = TypeNameCRC(name);
    const uint32_t ours =
        FindNode(static_cast<const Factory*>(factory), crc) ? crc : 0u;

    RT_DIFF_SCALAR(ours,
        RtOriginal<fn_GetTypeIdFromName_t>(kNameGetTypeIdFromName)(factory, name, existingOnly));
    return ours;
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
    RT_DIFF_BEGIN("Object::Factory::CreateObject");

    TypeNode* n = FindNode(static_cast<const Factory*>(factory), typeId);
    // Faithful: retail's `MOV RCX,[RAX+0x10]` at 0x1800c0b03 runs on a null
    // RAX and faults. No guard.
    const TypeRecord* rec =
        reinterpret_cast<const TypeRecord*>(reinterpret_cast<const char*>(n) + 8);
    BaseTypeInfo* info = rec->info;

    if (rt_diff_on__) {
        const char* theirs = RtOriginal<fn_GetTypeName_t>(kNameGetTypeName)(factory, typeId);
        const char* oursName = info ? info->name : nullptr;
        char a[64], b[64];
        std::snprintf(a, sizeof(a), "name=%p", static_cast<const void*>(oursName));
        std::snprintf(b, sizeof(b), "name=%p", static_cast<const void*>(theirs));
        RtDiffReport(&rt_diff_site__, oursName == theirs, a, b);
    }

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
    RT_DIFF_BEGIN("Object::Type::Serialize [not diffable]");

    void*  rw = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void** vt = *reinterpret_cast<void***>(rw);

    RT_DIFF_SHADOW(rw != nullptr && vt != nullptr && vt[0x98 / 8] != nullptr,
                   "rw+vtable+slot0x98 non-null", "(no oracle: mutates the stream)");

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
// approximate. This runs only under BANISHED_RT_DIFF=1 and only during static
// registration, which is single-threaded.
// ============================================================================
BaseTypeInfo* RtObject_BaseTypeInfo_Ctor(BaseTypeInfo* self,
                                         const char* name,
                                         const char* parentName,
                                         const char* fileExtension,
                                         void* factoryFn,
                                         void* inPlaceFactoryFn) {
    RT_DIFF_BEGIN("Object::BaseTypeInfo::BaseTypeInfo");
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

    if (rt_diff_on__) {
        BaseTypeInfo scratch;
        std::memset(&scratch, 0, sizeof(scratch));

        BaseTypeInfo* const ourHead = *g_firstTypeInfo;   // == self
        *g_firstTypeInfo = prevHead;                      // rewind
        RtOriginal<fn_BaseTypeInfoCtor_t>(kNameBaseTypeInfoCtor)(
            &scratch, name, parentName, fileExtension, factoryFn, inPlaceFactoryFn);
        *g_firstTypeInfo = ourHead;                       // undo their link

        // Field-by-field, never memcmp: `_pad04` is written by neither side and
        // comparing it would be the Frustum-padding false alarm from WAVE1.md.
        const bool equal =
            scratch.typeId           == self->typeId &&
            scratch.name             == self->name &&
            scratch.parentName       == self->parentName &&
            scratch.fileExtension    == self->fileExtension &&
            scratch.factoryFn        == self->factoryFn &&
            scratch.inPlaceFactoryFn == self->inPlaceFactoryFn &&
            scratch.next             == prevHead &&
            self->next               == prevHead;

        char a[64], b[64];
        std::snprintf(a, sizeof(a), "parent=%s", self->parentName ? self->parentName : "(null)");
        std::snprintf(b, sizeof(b), "parent=%s", scratch.parentName ? scratch.parentName : "(null)");
        RtDiffReport(&rt_diff_site__, equal, a, b);
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

    // ---- Survey (lane TYPE, 2026-09-08): BANISHED_RT_TYPE_SURVEY=1 --------
    //
    // A READ-ONLY pre-pass over the tree pass 1 just built. It resolves every
    // parentName exactly as pass 2 will and reports the ones that miss --
    // ALL of them, which is the one thing the faithful fault cannot tell you,
    // because the fault stops at the first.
    //
    // It writes nothing and is skipped entirely unless the env var is set, so
    // pass 2 below is bit-identical with the var unset. Its EFFECTIVENESS
    // control is the `withParent` count: a survey reporting 0 missing out of 0
    // parents resolved has proved nothing, and says so on its own line.
    if (const char* sv = std::getenv("BANISHED_RT_TYPE_SURVEY")) {
      if (sv[0] && sv[0] != '0') {
        int chain = 0, withParent = 0, missing = 0;
        for (TypeNode* n2 = LeftMost(self->root); n2; n2 = Successor(n2)) {
            ++chain;
            BaseTypeInfo* const bi = n2->value.info;
            if (!bi->parentName) continue;
            ++withParent;
            const uint32_t pid = TypeNameCRC(bi->parentName);
            TypeNode* q = self->root;
            while (q) {
                if (pid < q->key)      q = q->left;
                else if (q->key < pid) q = q->right;
                else                   break;
            }
            if (!q) {
                ++missing;
                printf("[RtTypeSurvey] MISSING PARENT: type \"%s\" "
                            "(id 0x%08X) names parent \"%s\" (id 0x%08X)\n",
                            bi->name ? bi->name : "(null)",
                            n2->value.typeId, bi->parentName, pid);
            }
        }
        printf("[RtTypeSurvey build LANETYPE-A2] %d nodes in tree, %d with a parentName, "
                    "%d parentName(s) UNRESOLVED%s\n",
                    chain, withParent, missing,
                    withParent == 0 ? "  (NO PARENTS RESOLVED -- this survey "
                                      "proved nothing)" : "");
        std::fflush(stdout);
      }
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

// ============================================================================
// The probe: BANISHED_RT_FACTORY_STARTUP_PROBE=1
// ============================================================================
//
// WHY A PROBE AND NOT RT_DIFF. The live call runs ONCE, on the engine's own
// factory, and it mutates. "Call ours, call theirs, compare" would insert every
// type twice into the one tree — the failure
// docs/runtime_port/WAVE9.md rows 27-28 correctly identify for
// ExternalFactory::FindOrLoad and ::Clean.
//
// WHY A PROBE IS NEVERTHELESS POSSIBLE HERE, and this is the whole argument:
// Startup's writes are of exactly two kinds.
//
//   (a) `info->typeId = CRC(info->name)`, into the SHARED BaseTypeInfo chain.
//       This is a PURE FUNCTION of a field neither run modifies, so it is
//       IDEMPOTENT — running Startup three times writes the same 32 bits three
//       times. It is not state that has to be snapshot and restored.
//   (b) Everything else — the tree, the nodes, the children arrays — is
//       reachable only through `this`.
//
// So a Factory the probe OWNS is a complete, independent subject: theirs into
// factory A, ours into factory B, compare A with B. That is
// reference_synthetic_subject_beats_a_fake.md, and it needs no fake at all —
// a zeroed 0x10 bytes is a valid empty Factory, which is what the ctor makes.
//
// WHAT IT COMPARES, AND WHY BY MAPPING. `value.parent` and every `children[i]`
// point INTO their own tree, so a byte compare would fail on exactly the
// fields that represent the work. The comparison is therefore the same shape
// as lane C2's mint shadow: A's pointer must denote the same TYPE as B's, and
// `value.info` — a pointer into the one shared chain — is that identity.
//
// COST. Both trees are LEAKED: ~N nodes x 0x50 bytes plus the children arrays,
// on the order of 60 KB for the ~600 types this engine registers. Freeing them
// would mean writing a post-order teardown of a tree this lane did not build,
// on the run where the port is least trusted. Opt-in, once per process.
//
// NOT RUN. Lane OBJ2 does not build or execute. This probe has never executed.
// ============================================================================

struct ProbeStats {
    int nodes = 0;            // node pairs compared
    int withParent = 0;       // ...whose info->parentName was non-null  <-- the
                              //    EFFECTIVENESS control for pass 2's main arm
    int bogusParent = 0;      // ...that resolved to the (TypeRecord*)8 sentinel
    int withChildren = 0;     // ...that ended up with >= 1 child
    int childEntries = 0;     // child pointers compared
    int grownArrays = 0;      // capacity != count, i.e. the *3 rule fired
                              //    <-- EFFECTIVENESS control for the growth arm
    int mismatches = 0;
};

// Returns the mismatch count. `verbose` false is the negative control's mode:
// count, print nothing.
int CompareTrees(FactorySet* theirs, FactorySet* ours, ProbeStats* st, bool verbose) {
    TypeNode* a = LeftMost(theirs->root);
    TypeNode* b = LeftMost(ours->root);

    while (a && b) {
        bool bad = false;
        const char* why = "";

        if (a->key != b->key)                       { bad = true; why = "key"; }
        else if (a->value.typeId != b->value.typeId){ bad = true; why = "typeId"; }
        else if (a->value.info != b->value.info)    { bad = true; why = "info"; }
        else if (a->value.childCount != b->value.childCount)
                                                    { bad = true; why = "childCount"; }
        else if (a->value.childCapacity != b->value.childCapacity)
                                                    { bad = true; why = "childCapacity"; }

        // Parent, by mapping. Three cases have to agree: both null, both the
        // (TypeRecord*)8 not-found sentinel, or both denoting the same type.
        if (!bad) {
            const TypeRecord* pa = a->value.parent;
            const TypeRecord* pb = b->value.parent;
            const bool sa = (reinterpret_cast<uintptr_t>(pa) == 8);
            const bool sb = (reinterpret_cast<uintptr_t>(pb) == 8);
            if (sa || sb) {
                if (sa != sb) { bad = true; why = "parent-sentinel"; }
                else          { ++st->bogusParent; }
            } else if ((pa == nullptr) != (pb == nullptr)) {
                bad = true; why = "parent-null";
            } else if (pa && pa->info != pb->info) {
                bad = true; why = "parent-mapping";
            }
            if (pa) ++st->withParent;
        }

        if (!bad && a->value.childCount > 0) {
            ++st->withChildren;
            if (a->value.childCapacity != a->value.childCount) ++st->grownArrays;
            for (int i = 0; i < a->value.childCount; ++i) {
                ++st->childEntries;
                const TypeRecord* ca = a->value.children[i];
                const TypeRecord* cb = b->value.children[i];
                if (!ca || !cb || ca->info != cb->info) {
                    bad = true; why = "child-mapping"; break;
                }
            }
        }

        if (bad) {
            ++st->mismatches;
            if (verbose && st->mismatches <= 20) {
                const char* nm = (a->value.info && a->value.info->name)
                                     ? a->value.info->name : "(unnamed)";
                printf("[FactoryStartupProbe] MISMATCH %-24s field=%s "
                            "theirs.key=0x%08X ours.key=0x%08X\n",
                            nm, why, a->key, b->key);
            }
        }

        ++st->nodes;
        a = Successor(a);
        b = Successor(b);
    }

    if (a || b) {
        ++st->mismatches;
        if (verbose)
            printf("[FactoryStartupProbe] MISMATCH node counts differ "
                        "(theirs %s, ours %s past the shorter walk)\n",
                        a ? "longer" : "equal", b ? "longer" : "equal");
    }
    return st->mismatches;
}

bool g_probeArmed = false;
bool g_probeRan   = false;

// ---------------------------------------------------------------------------
// Why `nodes != chain` is an ANSWER, not an alarm
// ---------------------------------------------------------------------------
// The probe used to print "pass 1 did not insert what it walked" whenever the
// tree held fewer nodes than the BaseTypeInfo chain. Wave 9 lane SYS2 reported
// the resulting `chain=416 nodes=415` as an unowned divergence, and lane PKG2
// independently confirmed it reproduces on a control exe. It is neither a
// divergence nor a defect in pass 1.
//
// Pass 1 has NO skip: every chain entry gets a `setInsert` call, and the key IS
// `TypeNameCRC(info->name)`. A Set holds one node per KEY, so 416 inserts
// landing 415 nodes means two entries share a key -- and retail's Startup runs
// the identical loop into the identical Set, so retail's tree is 415 too. The
// old message accused our port of losing an insert it never lost.
//
// What the collision DOES mean is worth naming rather than shrugging at: in a
// CRC-keyed type registry, one of the two colliding types is unreachable by id.
// So instead of an alarm, print WHICH types collide and let the reader judge.
// Two distinct names sharing a CRC is a hash collision; the same name twice is
// a type registered twice, which is a different (and more interesting) fact.
void ReportKeyCollisions(int chain, int nodes) {
    printf("[FactoryStartupProbe] chain=%d nodes=%d — the Set holds one "
                "node per KEY, so %d entr%s share a key with an earlier one. "
                "Pass 1 inserted all %d; retail's identical loop into its "
                "identical Set produces the same %d. Naming the collisions:\n",
                chain, nodes, chain - nodes, (chain - nodes) == 1 ? "y" : "ies",
                chain, nodes);

    int reported = 0;
    for (BaseTypeInfo* a = *g_firstTypeInfo; a && reported < 20; a = a->next) {
        for (BaseTypeInfo* b = *g_firstTypeInfo; b && b != a; b = b->next) {
            if (TypeNameCRC(a->name) != TypeNameCRC(b->name)) continue;
            const char* an = a->name ? a->name : "(unnamed)";
            const char* bn = b->name ? b->name : "(unnamed)";
            const bool sameName = a->name && b->name && strcmp(an, bn) == 0;
            printf("[FactoryStartupProbe]   key 0x%08X  %-28s  %-28s  %s\n",
                        TypeNameCRC(a->name), bn, an,
                        sameName ? "SAME NAME — this type is registered TWICE"
                                 : "distinct names — a CRC COLLISION, and one "
                                   "of the two is unreachable by type id");
            ++reported;
            break;                      // one report per later duplicate
        }
    }
    if (reported == 0)
        printf("[FactoryStartupProbe]   none found — so the shortfall is "
                    "NOT a duplicate key, and pass 1 really did lose an insert. "
                    "That IS a defect; do not dismiss it.\n");
}

// The kill switch, BANISHED_RT_FACTORY_STARTUP=0, set once at registration.
//
// It is spelled as a `!g_portOn` fallback INSIDE the body rather than as an
// `if (switch) RegisterRuntimeExport(...)` around the registration, because
// runtime_census.py refuses to count a gated registration — "A gated
// registration is not a port. Arm it, measure it, then move it." — and it is
// right to: an `if (flag)` registration is how the census once counted 6 dead
// Object entries as ported (reference_census_counted_gated_and_missed_dx11.md).
// The registration below is therefore UNCONDITIONAL and the census sees it.
// `g_portOn` has internal linkage; the identically-named flag in
// runtime/src/io/serializer.cpp is a different object, and the name is shared
// deliberately because tools/port_bypass_audit.py classifies this exact
// spelling as OK-PORT-OFF.
bool g_portOn = false;

void FactoryStartupProbe() {
    g_probeRan = true;

    // Chain length first — it is the denominator every other number below is
    // read against, and if it is 0 nothing that follows means anything.
    int chain = 0;
    for (BaseTypeInfo* i = *g_firstTypeInfo; i && chain < 100000; i = i->next) ++chain;

    typedef bool (*StartupFn)(void*);
    StartupFn orig = RtOriginal<StartupFn>("?Startup@Factory@Object@@QEAA_NXZ");
    if (!orig) {
        printf("[FactoryStartupProbe] NO ORACLE — the shipped DLL is not "
                    "loaded (this is expected at cutover). chain=%d\n", chain);
        return;
    }

    FactorySet theirs = { nullptr, 0, 0 };
    FactorySet ours   = { nullptr, 0, 0 };

    orig(&theirs);              // writes info->typeId (idempotent) + its own tree
    FactoryStartupRun(&ours);   // writes the same typeIds + our tree

    ProbeStats st;
    const int mism = CompareTrees(&theirs, &ours, &st, /*verbose=*/true);

    printf("[FactoryStartupProbe] chain=%d nodes=%d theirs.count=%u "
                "ours.count=%u withParent=%d bogusParent=%d withChildren=%d "
                "childEntries=%d grownArrays=%d mismatches=%d\n",
                chain, st.nodes, theirs.count, ours.count, st.withParent,
                st.bogusParent, st.withChildren, st.childEntries,
                st.grownArrays, mism);

    // ---- EFFECTIVENESS CONTROL --------------------------------------------
    // reference_probe_needs_an_effectiveness_control.md: 64 green comparisons
    // where zero did any work read exactly like a pass. Say so out loud when
    // an arm never ran, rather than letting `mismatches=0` speak for it.
    if (st.nodes == 0)
        printf("[FactoryStartupProbe] VACUOUS: nodes=0 — pass 1 inserted "
                    "nothing, so every comparison below is empty\n");
    else if (st.nodes != chain)
        ReportKeyCollisions(chain, st.nodes);
    if (st.withParent == 0)
        printf("[FactoryStartupProbe] VACUOUS: pass 2's parent arm never "
                    "ran — every comparison below it is untested\n");
    if (st.grownArrays == 0)
        printf("[FactoryStartupProbe] WEAK: no children array ever grew "
                    "past its first element, so the capacity*3 rule is "
                    "UNTESTED by this run\n");

    // ---- NEGATIVE CONTROLS -------------------------------------------------
    // A comparator that cannot fail is not evidence. Perturb `ours` — which is
    // discarded either way — in the two arms that matter and require a hit.
    // reference_recheck_must_match_its_own_proof.md: these must exercise the
    // SAME CompareTrees the verdict above came from, not a re-derived one.
    if (mism == 0) {
        for (TypeNode* n = LeftMost(ours.root); n; n = Successor(n)) {
            if (n->value.childCount <= 0) continue;

            const int32_t savedCap = n->value.childCapacity;
            n->value.childCapacity = savedCap + 1;
            ProbeStats nc1;
            const int hit1 = CompareTrees(&theirs, &ours, &nc1, /*verbose=*/false);
            n->value.childCapacity = savedCap;

            TypeRecord* const savedChild = n->value.children[0];
            n->value.children[0] = &n->value;      // point it at the wrong type
            ProbeStats nc2;
            const int hit2 = CompareTrees(&theirs, &ours, &nc2, /*verbose=*/false);
            n->value.children[0] = savedChild;

            printf("[FactoryStartupProbe] negative controls: "
                        "capacity-perturbation detected=%s, "
                        "child-perturbation detected=%s%s\n",
                        hit1 > 0 ? "YES" : "NO", hit2 > 0 ? "YES" : "NO",
                        (hit1 > 0 && hit2 > 0)
                            ? "" : "  <-- THE COMPARATOR IS BROKEN, IGNORE THE "
                                   "mismatches=0 ABOVE");
            break;
        }
    }

    printf("[FactoryStartupProbe] VERDICT: %s\n",
                mism == 0 ? "0 mismatches" : "DIVERGED — see the rows above");
}

} // namespace

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
    RT_DIFF_BEGIN("Object::Factory::Startup [probe, not diffable]");
    ResolveGlobals();

    if (!g_portOn) {
        // NEVER call this unchecked — serializer.cpp's rule. `!g_portOn` means
        // the port was switched off, and the commonest future reason for that
        // is the DLL not being there, in which case RT_ORIGINAL_ONCE is null
        // and calling it is the crash the switch was meant to avoid. With no
        // shipped body, ours is the only implementation there is.
        using Orig = bool (*)(void*);
        Orig orig = RT_ORIGINAL_ONCE(Orig, "?Startup@Factory@Object@@QEAA_NXZ");
        if (orig) return orig(factory);
    }

    // Opt-in, once, and BEFORE the real run so a divergence is reported even
    // if the real run is what faults.
    if (g_probeArmed && !g_probeRan) FactoryStartupProbe();

    FactoryStartupRun(static_cast<FactorySet*>(factory));

    if (rt_diff_on__) {
        int chain = 0, mismatched = 0;
        for (BaseTypeInfo* i = *g_firstTypeInfo; i && chain < 100000; i = i->next) {
            ++chain;
            if (i->typeId != TypeNameCRC(i->name)) ++mismatched;
        }
        int nodes = 0;
        for (TypeNode* n = LeftMost(static_cast<FactorySet*>(factory)->root);
             n && nodes < 100000; n = Successor(n)) ++nodes;

        // *** THE `theirs=` SIDE HERE IS NOT A MEASUREMENT OF RETAIL. ***
        //
        // This site runs no DLL body -- it is a self-check on OUR result, and
        // the second string is a CONSTRUCTED EXPECTATION (`chain, chain`). It
        // was previously formatted exactly like the measured side, so the
        // report read
        //
        //     ours=chain=416 nodes=415   theirs=chain=416 nodes=416
        //
        // which any reader would take as "retail's tree has 416 nodes and ours
        // has 415" -- a port defect. It says nothing of the kind: the 416 is
        // this line's own assumption that nodes==chain, restated. Wave 9 lane
        // UIC read it the natural way and reported a false divergence onward to
        // another lane, which is a fair reading of a badly labelled instrument,
        // not a mistake on its part.
        //
        // reference_recheck_must_match_its_own_proof.md is exactly this shape: a
        // check whose "expected" side is true by construction. The fix is to
        // stop dressing an assertion up as a comparison. The genuine question --
        // whether the shortfall is a lost insert or two entries sharing a key --
        // is answered empirically by ReportKeyCollisions() in the probe above,
        // which names the colliding types instead of asserting anything.
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "chain=%d nodes=%d badCrc=%d", chain, nodes, mismatched);
        std::snprintf(b, sizeof(b),
                      "EXPECTED (no DLL run): nodes==chain==%d, badCrc==0", chain);
        RtDiffReport(&rt_diff_site__, chain > 0 && nodes == chain && mismatched == 0, a, b);
    }

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
    // RtOriginal lookups). tools/runtime_census.py matches
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
    // ITS OWN KILL SWITCH, and it is DEFAULT-ON: BANISHED_RT_FACTORY_STARTUP=0
    // sets `g_portOn` false and the body forwards to the shipped function.
    // Registration is UNCONDITIONAL so the census counts it — see the comment
    // on g_portOn. It gets a switch of its own rather than sharing
    // BANISHED_RT_TYPESYS because it is step 6 of Engine::Startup's queue: if
    // it is wrong nothing boots, and the bisection wants one variable.
    RegisterRuntimeExport("?Startup@Factory@Object@@QEAA_NXZ",
                          (void*)&RtObject_Factory_Startup); ++n;
    RegisterRuntimeAddr(0x1800c0630, (void*)&RtObject_Factory_Startup);
    RtObject::g_portOn = RtWaveEnabled("BANISHED_RT_FACTORY_STARTUP");
    if (!RtObject::g_portOn)
        printf("[RtPort] BANISHED_RT_FACTORY_STARTUP=0 -- Object::Factory::Startup "
               "forwards to the DLL\n");
    // The probe is OPT-IN, so it is an explicit "set and not 0", not
    // RtWaveEnabled (which means "on unless switched off").
    {
        const char* probe = std::getenv("BANISHED_RT_FACTORY_STARTUP_PROBE");
        RtObject::g_probeArmed = RtObject::g_portOn && probe && std::strcmp(probe, "0") != 0;
    }

    RegisterRuntimeAddr(0x1800491c0, (void*)&RtObject_Base_IsA);
    RegisterRuntimeAddr(0x1800c0390, (void*)&RtObject_Base_IsDerivedFrom);
    RegisterRuntimeAddr(0x1800c0580, (void*)&RtObject_Factory_GetTypeIdFromName);
    RegisterRuntimeAddr(0x1800c0ae0, (void*)&RtObject_Factory_CreateObject);
    RegisterRuntimeAddr(0x1800c0c40, (void*)&RtObject_Type_Serialize);
    RegisterRuntimeAddr(0x180006320, (void*)&RtObject_BaseTypeInfo_Ctor);

    // Ordinals: engine/funcs/engine_funcs.cpp:4552 falls back to
    // ResolveOrdinal(2991) for GetTypeIdFromName and :4566 to
    // ResolveOrdinal(3089) for IsA when the name lookup fails. Both names DO
    // resolve, so these are belt-and-braces — but an unregistered ordinal
    // fallback is exactly how a port goes silently inert.
    RegisterRuntimeOrdinal(3089, (void*)&RtObject_Base_IsA);
    RegisterRuntimeOrdinal(2991, (void*)&RtObject_Factory_GetTypeIdFromName);

    return n;
}
