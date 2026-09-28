// ============================================================================
// runtime/src/object/external_class.cpp — ENDGAME B.3.3 step 5 (EXTERNAL)
// ============================================================================
//
// `Object::External`, `Object::CompiledExternal` and the registry-free half of
// `Object::ExternalList`, as real C++ classes deriving from the real
// `Object::Base`. The layouts, the measured vtable slot lists and the naming
// evidence are in runtime/types/object/External.h; the design rules are
// docs/runtime_port/endgame_b/B33_RULES.md §1.4, §6, §7 and B31_RULES' two-step
// shim pattern.
//
// WHY THIS FILE IS SEPARATE FROM external.cpp, AND FROM ITS OWN SIBLING
// ---------------------------------------------------------------------
// Exactly the split runtime/src/object/type_reflection.cpp made against
// type_system.cpp at step 3, for the same reason: the thirteen existing
// `external*.cpp` files own the REGISTRY — the `RtObject_*` ABI shims, the
// mangled-name and retail-address doors, `RT_EXPORT_ONCE`, `RT_DATA_SLOT`,
// `RtResolveCached` — and none of that links into `make unit`. The CLASSES have
// to be unit-testable (B33_RULES §9 step 5 asks for the slot order, the
// Type()/TypeName() chain, `Destroy(p)` and a tree find at two depths), so the
// bodies that need nothing but memory live here and the doors stay where they
// are.
//
// `external_class_doors.cpp` is the other half: every member whose body is a
// door into an already-ported C function. It is NOT linked into `make unit`.
//
// NOTHING HERE IS REGISTERED. No `RegisterRuntimeExport`, no
// `RegisterRuntimeAddr`, no `RT_DELDTOR` row added or retired. Every existing
// entry point keeps working and keeps its address; the minted
// `??_7External@Object@@6B@` table stays (ExternalList's was retired by
// ENDGAME B.3.9 OBJ), because it is what ~80 unconverted files stamp into live objects and
// because the `RT_DELDTOR(0x180006640, …)` row is what FILLS slot 0 of the
// minted table (B33_RULES §6.1). A converted object and a minted-vtable object
// are slot-for-slot interchangeable — tests/unit/object_vtable_abi_test.cpp
// proves it in both directions — which is what lets the two coexist.
//
// NOTHING IN PRODUCTION CONSTRUCTS ONE OF THESE YET. Every live `External` in
// the process is still built by `RtObject_External_Ctor` over the POD and wears
// the minted table. These classes become live as step 6's caller batches move
// over; until then their value is that the ABI claim is now compiler-checked
// and unit-tested rather than asserted in a comment.
// ============================================================================

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../types/object/External.h"
#include "../system/string.h"       // RtSystem::StringRefCount / RtMemFree
#include "../system/pod_bridge.h"   // the ONE transitional POD<->class bridge

#if !defined(BANISHED_UNIT_TEST)
#include "../../funcs/rt_port.h"
#endif

namespace Object {

// ============================================================================
// The reflection-record seam
// ============================================================================
//
// External.h's closing banner is the full argument for why these three are
// DOORS and not `DECLARE_TYPE`/`DEFINE_TYPE`: the records are minted, not built
// by our code, `standalone/cutover_manifest.inc` carries one of them as
// `CUTOVER_REQUIRED`, ~20 live sites already read them by name, and a
// `DEFINE_TYPE` would head-insert a SECOND record for the same name into
// `BaseTypeInfo::_firstTypeInfo`. Retiring the mint rows in favour of the macro
// means editing standalone/rt_data_mint.cpp and re-running
// tools/rt_mint_gen.py, both outside this step's file set.
//
// So `Type()` / `TypeName()` / `TypeId()` below answer out of the ONE existing
// record, through the SAME named door runtime/src/object/external_list.cpp:75
// and runtime/src/engine/mainloop_video.cpp:343 already use, and they are
// byte-for-byte retail's `MOV EAX,dword[0x1801f9b38]` /
// `MOV RAX,qword[0x1801f9b40]` (the second address is the first plus 8, i.e.
// `BaseTypeInfo::name`, which is exactly what the record's +0x08 is).
// ============================================================================

#if defined(BANISHED_UNIT_TEST)
// The unit build has no port registry and no mint. The test supplies the three
// records; they start null and the test sets them before it calls anything that
// reads one. This is a TEST seam, not a guard on a faithful path — the real
// build's door below is unguarded, as retail's rip-relative load is.
const BaseTypeInfo* g_unitExternalTypeInfo         = nullptr;
const BaseTypeInfo* g_unitExternalListTypeInfo     = nullptr;
const BaseTypeInfo* g_unitCompiledExternalTypeInfo = nullptr;

const BaseTypeInfo* ExternalTypeInfoRecord()         { return g_unitExternalTypeInfo; }
const BaseTypeInfo* ExternalListTypeInfoRecord()     { return g_unitExternalListTypeInfo; }
const BaseTypeInfo* CompiledExternalTypeInfoRecord() { return g_unitCompiledExternalTypeInfo; }
#else
// ############################################################################
// ENDGAME B.3.9 OBJ — THE RUNTIME `TypeId()` ACCESSOR PATTERN (B39_RULES D6
// step 1), the one every Object class with a MINTED record uses until the W3
// fold gives it a `DEFINE_TYPE`:
//
//   * one NAMESPACE-SCOPE `std::atomic<const BaseTypeInfo*>` per record — no
//     function-local static, no `RT_DATA_SLOT` lambda cache (B9-10 / B6-5);
//   * filled by the FIRST reader through `ResolveExport(<the record's exact
//     mangled name>)`, which never caches a miss: a null answer is returned
//     and NOT stored, so a read before the mint registers the record (none
//     exists — B39_RULES D6 "Timing") is not frozen. It cannot be filled at
//     port init: the mint, which registers the record, runs after every
//     *PortInit. The store is idempotent (every thread resolves the same
//     pointer), so the loader thread and the main thread may race it freely;
//   * read as the real `BaseTypeInfo` (`->typeId`, `->name`), never by
//     memcpy off raw bytes;
//   * `X::TypeId()` returns `->typeId`; slot 2 `Type()` returns `TypeId()`;
//     slot 1 `TypeName()` returns `->name` (content-only, never compared by
//     pointer). At the W3 fold the accessor body becomes `_typeInfo` and the
//     slot disappears with the minted row; no caller changes.
// ############################################################################
namespace {

std::atomic<const BaseTypeInfo*> g_externalTypeInfo{nullptr};          // 0x1801f9b38
std::atomic<const BaseTypeInfo*> g_externalListTypeInfo{nullptr};      // 0x1801f9be0
std::atomic<const BaseTypeInfo*> g_compiledExternalTypeInfo{nullptr};  // 0x1801f9b00

const BaseTypeInfo* MintedRecord(std::atomic<const BaseTypeInfo*>& slot,
                                 const char* mangled) {
    const BaseTypeInfo* ti = slot.load(std::memory_order_acquire);
    if (ti == nullptr) {
        ti = static_cast<const BaseTypeInfo*>(ResolveExport(mangled));
        if (ti != nullptr) slot.store(ti, std::memory_order_release);
    }
    return ti;
}

}  // namespace

const BaseTypeInfo* ExternalTypeInfoRecord() {
    return MintedRecord(g_externalTypeInfo,
                        "?_typeInfo@External@Object@@0VBaseTypeInfo@2@A");
}
const BaseTypeInfo* ExternalListTypeInfoRecord() {
    return MintedRecord(g_externalListTypeInfo,
                        "?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A");
}
const BaseTypeInfo* CompiledExternalTypeInfoRecord() {
    return MintedRecord(g_compiledExternalTypeInfo,
                        "?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A");
}
#endif

// ============================================================================
// Object::External
// ============================================================================

// ??0External@Object@@QEAA@XZ   retail 0x1800bc9c0
//
// Retail stamps `??_7Base@Object@@6B@` and then immediately overwrites it with
// `??_7External@Object@@6B@`. That IS the inlined `Object::Base` constructor
// followed by External's own stamp, and a real constructor chain emits both
// stores for us — which is why they are not transcribed. The object then
// carries GCC's table rather than the minted one; the two are slot-compatible
// (see the file banner).
//
// The four zero stores retail makes at +0x08, +0x10, +0x18 are the member
// initialisers below: `_path.data` and the Array's three fields.
External::External() {
    _path.data = nullptr;
    // `Collection::Array`'s own constructor is the three zero stores retail
    // makes here; it has already run as a member initialiser. Restating them
    // would be a second write of the same zeros.
}

// ??1External@Object@@UEAA@XZ   retail 0x1800bc950
//
// Retail's order, preserved exactly:
//   1. stamp External's vtable  — a real destructor chain emits this on entry
//      to ~External, so a virtual called during teardown dispatches to
//      External's override and not the subclass's. Not hand-written, because a
//      hand-written store would name the MINTED table and a converted object
//      carries the compiler's.
//   2. CALL 0x180006b80 -> 0x180006ac0: release each element's leading String,
//      then free the buffer and zero the header. THE FIRST HALF IS THE LOOP
//      BELOW; the second half is the compiler-emitted `~_refs`, which for a
//      trivially-destructible element is exactly `ArrayDtorImpl` (free, then
//      zero `_data`/`_size`/`_capacity`) and nothing else. Each half runs once.
//      See External.h's member banner.
//   3. release `_path` — retail's inlined release IS `~Filename` (0x180006980).
//      `_path` keeps the POD type so it is trivially destructible, so it is
//      released BY HAND here, through the one `Sys(...)` bridge, exactly as
//      runtime/src/object/external.cpp's `RtObject_External_Dtor` does. Giving
//      the member the class type instead would make the compiler release it a
//      second time.
//   4. stamp Base's vtable — `~Base()` running after this body emits it.
//
// The element release is retail's String idiom verbatim, including the
// `e->owned = p - 8` store on the free branch, which is the release asymmetry
// runtime/types/system/String.h documents and which callers depend on.
External::~External() {
    RtObject::ExternalArrayElement* e = _refs._data;
    for (int i = 0; i < _refs._size; ++i, ++e) {
        char* p = e->owned;
        if (p != nullptr) {
            if (--RtSystem::StringRefCount(p) == 0) {
                e->owned = p - 8;
                if (e->owned != nullptr)
                    RtSystem::RtMemFree(e->owned);
            } else {
                e->owned = nullptr;
            }
        }
    }
    RtSystem::Sys(_path).~Filename();
}

// slot 0 — the `??_G` vector deleting destructor at 0x180006640, whose
// `MOV EDX,0x20` is the element size and whose `LEA R9,[0x1800bc950]` is
// ~External. `flags & 1` frees the storage; `flags & 2` is the array form,
// which reads the count from `this - 8` — a header only `operator new[]`
// writes, so taking that arm on an object that was not array-allocated faults
// at `this - 8` exactly as retail would. Deletion is `Object::Destroy(p)`,
// never `delete p` (~External is non-virtual).
Base* External::DeletingDtor(unsigned flags) {
    this->~External();
    if (flags & 1) TypeTreeFree(this);
    return this;
}

// slot 1 — ?TypeName@External@Object@@UEBAPEBDXZ  0x1800066c0
//           `MOV RAX,qword ptr [0x1801f9b40]; RET`, and 0x1801f9b40 is
//           `?_typeInfo@External@Object@@` + 0x08, i.e. `BaseTypeInfo::name`.
//           CONTENT-only: never compared by pointer (B33_RULES §2.4).
const char* External::TypeName() const { return ExternalTypeInfoRecord()->name; }

// slot 2 — ?Type@External@Object@@UEBAIXZ  0x1800066d0
//           `MOV EAX,dword ptr [0x1801f9b38]; RET` — the record's +0x00, which
//           is 0xFFFFFFFF until `Factory::Startup` (B33_RULES §2.5; NOT
//           self-initialising, the pre-Startup ordering traps depend on it).
uint32_t External::Type() const { return ExternalTypeInfoRecord()->typeId; }

// The static companion retail's reflection block emits as
// `?TypeId@X@Ns@@SAIXZ`. Same record, same sentinel rule.
uint32_t External::TypeId() { return ExternalTypeInfoRecord()->typeId; }

// slot 4 — ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
//          retail 0x1800066e0, whose whole body is `MOV EAX,0x1; RET`.
//
// External's virtual DEFAULT: the base class cannot compile anything, so it
// answers CompileResult(1) and every resource type that can compile overrides
// the slot. `path` is genuinely dead — RDX is never read in the two
// instructions — and is kept because the slot's signature demands it.
//
// (This slot is what B33_RULES §1.4 calls "name UNKNOWN, keep it as Slot4()".
// It has a retail name; External.h's slot banner carries the evidence.)
External::CompileResult External::Compile(const System::Filename& /*path*/) {
    return 1u;
}

// ?GetPath@External@Object@@QEBA?AVFilename@System@@XZ   retail 0x180006250
//
// One line in retail: the Filename copy constructor from `this->_path`, which
// SHARES the buffer and bumps its refcount — it does not copy the characters.
// The hidden return slot the C entry point carries is the ABI's, not the
// language's: a real member returning by value has it emitted for it.
System::Filename External::GetPath() const {
    return System::Filename(RtSystem::Sys(_path));
}

// ============================================================================
// Object::CompiledExternal
// ============================================================================

// ?TypeId@CompiledExternal@Object@@SAIXZ  0x18000b6a0 — `MOV EAX,[0x1801f9b00]`.
uint32_t CompiledExternal::TypeId() { return CompiledExternalTypeInfoRecord()->typeId; }

// slot 1 — 0x18000b6b0 `MOV RAX,[0x1801f9b08]` (the record's name, content-only)
// slot 2 — 0x18000b690 `MOV EAX,[0x1801f9b00]` (the record's type id)
const char* CompiledExternal::TypeName() const { return CompiledExternalTypeInfoRecord()->name; }
uint32_t    CompiledExternal::Type() const     { return TypeId(); }

// ??1CompiledExternal@Object@@UEAA@XZ   retail 0x1800bb980
//
// Seven instructions: re-stamp `??_7CompiledExternal@Object@@6B@`
// (0x180154a30), then TAIL-JMP ~External. Both are the destructor chain's, so
// the body is empty — and the re-stamp is NOT redundant tidying being dropped:
// GCC emits the same store on entry to ~CompiledExternal, naming the compiler's
// table instead of the minted one, which is the whole point of the class.
CompiledExternal::~CompiledExternal() {}

// ============================================================================
// Object::ExternalList's registry-free members ARE NOT HERE, and that is a LINK
// decision, not an oversight. GCC emits a class's vtable in the translation
// unit that defines its KEY FUNCTION (the first non-inline virtual in
// declaration order), and that vtable references EVERY slot. ExternalList's
// slots 5-8 (`Serialize`, `Reconstruct`, `Destruct`, `Construct`) are doors
// into the registry, so a vtable emitted in THIS file would drag the registry
// into `make unit` through the back door and undo the split this file exists
// for. All of ExternalList therefore lives in external_class_doors.cpp, whose
// vtable emission is harmless because that file is not a unit-test input.
//
// `Object::External` itself has no such problem: its four unimplemented slots
// are PURE in retail too (`_purecall` at 0x18007ec28), so its vtable references
// `__cxa_pure_virtual` and nothing else.
// ============================================================================

}  // namespace Object
