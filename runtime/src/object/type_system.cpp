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
#include <new>          // placement new, for the BaseTypeInfo ctor shim

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

Factory**      g_factorySingleton = nullptr;// ?_singleton@Factory@Object@@0PEAV12@EA           0x1801fc6e8

// `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` (0x1801fc6e0) is NO LONGER
// RESOLVED (ENDGAME B.3.3 step 3). It is `Object::BaseTypeInfo::_firstTypeInfo`,
// a plain static in runtime/src/object/type_reflection.cpp, because the
// constructor that writes it runs at CRT-init where the registry does not exist
// yet. standalone/rt_data_mint.cpp routes the minted row's two doors at that
// address to the same cell, so every by-name and by-address consumer — the
// probes in standalone/main.cpp, `[GameMain] Post-Startup _firstTypeInfo`,
// `cutover_manifest` — still reads exactly one head pointer, ours.
void ResolveGlobals() {
    if (!g_factorySingleton) {
        g_factorySingleton = reinterpret_cast<Factory**>(
            ResolveExport("?_singleton@Factory@Object@@0PEAV12@EA"));
    }
}

// ---- CRC-32, the type-id hash -------------------------------------------
//
// MOVED (B.3.3 step 3) to `Object::TypeNameCRC` in
// runtime/src/object/type_reflection.cpp, together with the crcTable evidence,
// so the class, this file's ABI shims and standalone/rt_data_mint.cpp all call
// ONE body. The name is kept here because six sites below read better with it.
inline uint32_t TypeNameCRC(const char* name) { return ::Object::TypeNameCRC(name); }

// The live factory, viewed as the real class. The two describe the same 0x10
// bytes (`Object::Factory` asserts `sizeof == 0x10` and `offsetof(_types) == 0`
// against `??0Factory@Object@@`'s two stores), so this is the B.3.3 bridge from
// a caller that still hands us a `void*`/`RtObject::Factory*`.
inline ::Object::Factory* AsFactory(const void* factory) {
    return reinterpret_cast<::Object::Factory*>(const_cast<void*>(factory));
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

    // ORDER MATTERS AND IS PRESERVED: retail loads `_singleton` into RDI BEFORE
    // making the virtual GetTypeId call (0x1800c039a-0x1800c03a6).
    const Factory* factory = *g_factorySingleton;
    const uint32_t ownId   = GetTypeIdVirtual(self);

    // The walk itself is `Object::Factory::IsDerivedFrom` — including retail's
    // unguarded `ADD RAX,0x8` on a null find, which is why a missing
    // registration still faults here rather than returning false.
    return AsFactory(factory)->IsDerivedFrom(ownId, typeId);
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
    return AsFactory(factory)->GetTypeIdFromName(name, existingOnly);
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
    // Faithful: retail's `MOV RCX,[RAX+0x10]` at 0x1800c0b03 runs on a null
    // RAX and faults. No guard — `Object::Factory::CreateObject` keeps it.
    return reinterpret_cast<void*>(AsFactory(factory)->CreateObject(typeId));
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
// B.3.3 step 3: THE BODY IS NOW `Object::BaseTypeInfo`'s constructor
// (runtime/src/object/type_reflection.cpp), including the backward scan, which
// is retail's own `?StripNamespace@BaseTypeInfo@Object@@QEAAXXZ`. This is the
// ABI shim the registry and code/Game/Object/factory_registration.cpp still call
// by name and by address: it placement-constructs the real class into storage
// the CALLER owns, exactly as retail's ctor does.
//
// `?_firstTypeInfo@` is no longer resolved — see ResolveGlobals' banner. The
// consequence that matters at cutover: this shim no longer needs the registry
// at all, so it works from a static initialiser.
BaseTypeInfo* RtObject_BaseTypeInfo_Ctor(BaseTypeInfo* self,
                                         const char* name,
                                         const char* parentName,
                                         const char* fileExtension,
                                         void* factoryFn,
                                         void* inPlaceFactoryFn) {
    using Real = ::Object::BaseTypeInfo;
    static_assert(sizeof(Real) == sizeof(BaseTypeInfo),
                  "the shim writes the caller's 0x38 bytes in place");

    new (static_cast<void*>(self)) Real(
        name, parentName, fileExtension,
        reinterpret_cast<Real::FactoryFn>(factoryFn),
        reinterpret_cast<Real::InPlaceFn>(inPlaceFactoryFn));

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

// ---- WHAT USED TO LIVE HERE ------------------------------------------------
//
// The allocator boundary, the `FactorySet` probe-only view struct, `LeftMost`,
// `Successor` and `FactoryStartupRun` — ~225 lines of hand-walked AVL and an
// inlined `Array<TypeRecord*>::SetSize` — are now `Object::Factory::Startup`
// and the `Collection::Set` it stands on (ENDGAME B.3.3 step 3,
// runtime/src/object/type_reflection.cpp). Every measurement they carried moved
// with them; the two that a reader of THIS file still needs are repeated in
// runtime/types/object/TypeInfo.h next to the class:
//
//   * `Object::Factory` is 0x10 bytes, not 8. `??0Factory@Object@@QEAA@XZ`
//     (0x1800c09d0) writes `[RCX]=0` and a DWORD `[RCX+8]=0`, and the insert
//     core INCs that dword at 0x1800303ac / 0x18003043c — it is the Set's
//     element count. `RtObject::Factory`'s `{ TypeNode* root; }` above stays a
//     correct VIEW that must never be used as STORAGE.
//   * `TypeNode` is 0x50 bytes and its +0x48 is TWO 16-BIT FIELDS, not one
//     int32 (0x180030540 `CMP word [RDX+0x48],-1` and
//     `MOVZX ...,word [RAX+0x4a]`; 0x1800087c0 zeroes both in one dword store,
//     which is why an int32 view looked right until something rebalanced).
//     `Collection::Set<TypeEntry,…>::Node` spells them as `int16_t balance` and
//     `int16_t height` and the header asserts both offsets.
//
// The by-address call to the Set insert at 0x180030140 is also gone: the
// template reaches the SAME ported core (`AvlOps<Node,0x30>::Retrace` ->
// `PortedRetrace48`). `rtavl::TypeSetInsert` keeps its registration for any
// caller that still resolves that address.
// ----------------------------------------------------------------------------

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
    // `factory` is the singleton `??0Factory@Object@@` built
    // (runtime/src/engine/singleton_ctors.cpp:288) — 0x10 bytes, zeroed. The
    // real class is laid over it; the walk itself is Object::Factory::Startup.
    return AsFactory(factory)->Startup();
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

    // (Ordinals 3089 for IsA and 2991 for GetTypeIdFromName used to be
    // registered here as well. ENDGAME A.3 deleted the ordinal door; both
    // functions are registered by mangled name above, which is the only door
    // left.)

    // BATCH 5 (2026-09-12): the private CRC helper. Both doors — the name is a
    // real export (ordinal 2989) despite the `AEBA` private mangling.
    RegisterRuntimeExport("?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z",
                          (void*)&RtObject_Factory_GetTypeFromNameCRC); ++n;
    RegisterRuntimeAddr(0x1800c0930, (void*)&RtObject_Factory_GetTypeFromNameCRC);

    return n;
}
