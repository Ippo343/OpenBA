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

#include "../../types/object/TypeInfo.h"
#include "../../funcs/rt_port.h"

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
            ResolveExportRaw("?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA"));
    }
    if (!g_factorySingleton) {
        g_factorySingleton = reinterpret_cast<Factory**>(
            ResolveExportRaw("?_singleton@Factory@Object@@0PEAV12@EA"));
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
