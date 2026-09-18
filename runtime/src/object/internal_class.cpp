// ============================================================================
// runtime/src/object/internal_class.cpp — ENDGAME B.3.3 step 5
// ============================================================================
//
// `Object::Internal` and `Object::InternalFactory` as real classes, under the
// retail names. The measured vtable, the layouts and the naming evidence are in
// runtime/types/object/Internal.h; the design rules are
// docs/runtime_port/endgame_b/B33_RULES.md §1, §5, §6, §7 and §9 step 5.
//
// WHY THIS FILE IS SEPARATE FROM internal.cpp
// -------------------------------------------
// The same split runtime/src/object/type_reflection.cpp has against
// type_system.cpp: internal.cpp owns the REGISTRY — the `RtObject_*` ABI shims,
// the mangled-name doors, the attendance counters and the vtable-pin screen.
// None of that can link into `make unit` (tests/unit/rt_port_stub.cpp stubs
// `ResolveExport`/`ResolveAddr` but not `RtResolveCached`). The CLASSES have to
// be unit-testable — B33_RULES §9 step 5 asks for the slot order, the
// `Type()`/`TypeName()` chain, `Destroy(p)` and the factory table logic — so the
// bodies live here behind one `-DBANISHED_UNIT_TEST` seam and the doors stay
// there.
//
// WHAT DID **NOT** MOVE, AND WHY
// ------------------------------
//   * `??0InternalFactory@Object@@QEAA@XZ` (0x1800c0030) is registered from
//     runtime/src/engine/singleton_ctors.cpp and `??1InternalFactory@Object@@`
//     (0x18002f860) from runtime/src/object/factory_dtors.cpp. Neither file is
//     in this step's set, so the class declares neither: a second spelling of a
//     body that is already registered at those two addresses is exactly the
//     drift B31_RULES' two-step shim pattern exists to avoid.
//   * `?Clone@Internal@Object@@QEAAPEAV12@XZ` (0x1800bfb10) is UNPORTED —
//     nothing in the tree registers it by name or by address. It is a
//     serialize-round-trip deep copy (a 0x1000-byte MemoryStream pair, the
//     Factory descent, `IsDerivedFrom`, then slot 3 both ways), and porting it
//     needs `IO::MemoryStream`, which is B.3 step 4. It is named here so the
//     next reader does not re-derive that it is missing, and it is NOT declared
//     on the class, because a declared method with no body is a link error
//     waiting for its first caller.
//   * Every `Serialize`/persist body still goes through `RtObject::SerializerLite`
//     and the **retail** ReaderWriter/Encoding vtable byte offsets. Those DIFFER
//     in the profile build (Internal.h:209-218). Nothing here is cross-checked
//     against the profile disassembly and nothing is modernised into
//     `IO::Serializer` — that is B.3 step 4.
//
// No C++ exceptions anywhere in this file (PLAN.md §4.3 risk 5).
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdio>
#include <cstdlib>

#include "../../types/object/Internal.h"

#if !defined(BANISHED_UNIT_TEST)
#include "../../funcs/rt_port.h"
#endif

namespace Object {

// ============================================================================
// The seam
// ============================================================================
//
// Retail's `Collection::Array` growth inside this class goes through the
// MemoryAllocator STATICS, not through the container's own allocator policy:
// every inlined copy is `EnterCS(_singleton+0x640); Allocate(...); LeaveCS`,
// which is precisely `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
// (0x1800c7a50) with the align argument threaded through. This step does not
// move that door — it only gives the bodies a home on the class.
//
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) is the same body as
// the class-scoped `operator delete` at 0x180006a10 that `Internal`'s slot 0
// tail-calls (proved for all 88 such deletes in
// runtime/src/object/class_operator_delete.cpp's banner), so slot 0's free arm
// goes through this one door rather than pinning an 89th copy.
namespace detail {

#if defined(BANISHED_UNIT_TEST)

// The host has no MemoryAllocator singleton and no registry. `align` is ignored
// because malloc already returns storage aligned for any scalar.
void* InternalMemAlloc(uint64_t bytes, int /*align*/) { return std::malloc((size_t)bytes); }
void  InternalMemFree(void* p)                        { std::free(p); }

// `?Rand@Random@System@@QEAAIXZ` (0x1800c7f90) is ported in
// runtime/src/system/value_types.cpp, which stands on the registry and is not
// linked into `make unit`. A test that wants to observe the generation draw
// installs this hook for its duration and clears it again — the same shape
// tests/unit/rt_port_stub.cpp's `g_unitResolveAddr` has, and the same shape
// tests/unit/array_test.cpp already drives. With no hook installed the draw is
// 0, which is what `_generation` is initialised to anyway.
uint32_t (*g_unitInternalRandHook)(::Random*) = nullptr;

uint32_t InternalRand(::Random* random) {
    return g_unitInternalRandHook ? g_unitInternalRandHook(random) : 0u;
}

#else

typedef void*    (*InternalMemAllocFn)(uint64_t, int);
typedef void     (*InternalMemFreeFn)(void*);
typedef uint32_t (*InternalRandFn)(void*);

void* InternalMemAlloc(uint64_t bytes, int align) {
    return RT_EXPORT_ONCE(InternalMemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(bytes, align);
}
void InternalMemFree(void* p) {
    RT_EXPORT_ONCE(InternalMemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}
// Wave 2 ported this and proved it bit-exact over 16,035,064 calls.
uint32_t InternalRand(::Random* random) {
    return RT_EXPORT_ONCE(InternalRandFn, "?Rand@Random@System@@QEAAIXZ")(random);
}

#endif

// ============================================================================
// Collection::Array<T> bodies, as retail instantiates them
// ============================================================================
// Moved here VERBATIM from runtime/src/object/internal.cpp (Wave 3, lane B);
// the only edit is the element type's spelling, `RtObject::Internal*` ->
// `Object::Internal*`, which is the same eight bytes.
//
// WAVE2's warning is live: node/element offsets are a function of sizeof(T), so
// MSVC emits a SEPARATE body per instantiation and one ported body cannot serve
// two element widths. Retail has four distinct ones in this class alone and all
// four are transcribed separately below with their own addresses. They are NOT
// re-expressed as `Collection::Array<T>::SetSize` (B32_RULES' "the template
// WRAPS the ported core" cuts the other way here): the container template binds
// to `Collection::DefaultAllocator`, and retail's inlined growth here binds to
// the MemoryAllocator statics above. Changing which heap these buffers come
// from is not this step's business, and the resave gate would see it.
//
// The `if (slot != nullptr)` tests are RETAIL's, not ours — the template's
// per-element address check surviving into codegen. They can never fire for a
// non-null buffer. Kept because this family is byte-faithful first.

// FUN_18002ffa0 — Array<Internal*>::SetSize(int newSize, const Internal*& fill)
void ArrayPtrSetSizeFill(Collection::Array<Internal*>* a, int newSize, Internal* const* fill) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            InternalMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        Internal** slot = a->_data + i;
        if (slot != nullptr)
            *slot = (i < oldSize) ? old[i] : *fill;
    }
    if (old != a->_data && old != nullptr)
        InternalMemFree(old);
}

// FUN_18002fca0 — Array<Internal*>::SetSize(int newSize), no fill value.
// Only the surviving prefix is copied; a grown tail is left UNINITIALISED,
// which is why retail only ever calls this one to SHRINK.
void ArrayPtrSetSize(Collection::Array<Internal*>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            InternalMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        InternalMemFree(old);
}

// FUN_18002fd90 — Array<Internal*>::Reserve(int n). Grows CAPACITY only: it sets
// _size to n while it reallocates and restores the old _size on the way out, and
// it returns immediately unless n is strictly greater than _size.
void ArrayPtrReserve(Collection::Array<Internal*>* a, int n) {
    const int oldSize = a->_size;
    if (oldSize >= n)
        return;
    Internal** old = a->_data;
    a->_size = n;
    if (a->_capacity < n) {
        a->_capacity = n;
        if (n == oldSize + 1 && oldSize != 0)
            a->_capacity = n * 3;
        a->_data = static_cast<Internal**>(
            InternalMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    if (old != a->_data) {
        for (int i = 0; i < oldSize; ++i) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
        if (old != nullptr)
            InternalMemFree(old);
    }
    a->_size = oldSize;
}

// FUN_18002fea0 — Array<int32_t>::SetSize(int newSize). Element width 4,
// alignment 4 (Collection.h: the alignment argument is alignof(T), not
// sizeof(T)); otherwise identical to ArrayPtrSetSize.
void ArrayIntSetSize(Collection::Array<int32_t>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    int32_t* old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<int32_t*>(
            InternalMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 2), 4));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            int32_t* slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        InternalMemFree(old);
}

}  // namespace detail

// ============================================================================
// Object::Internal — the reflection block
// ============================================================================
//
// Retail's own macro expansion, legible in the export table as one contiguous
// per-class block:
//
//   ?_TypeName@Internal@Object@@CAPEBDXZ          0x18002fc60  LEA RAX,[0x180155b10]  -> "Internal"
//   ?_ParentTypeName@Internal@Object@@CAPEBDXZ    0x18002fc70  LEA RAX,[0x180155ad0]  -> "Base"
//   ?Type@Internal@Object@@UEBAIXZ                0x1800064d0  MOV EAX,[0x1801f9c18]  (slot 2)
//   ?TypeId@Internal@Object@@SAIXZ                0x18002fc50  MOV EAX,[0x1801f9c18]
//   ?TypeName@Internal@Object@@UEBAPEBDXZ         0x1800064c0  MOV RAX,[0x1801f9c20]  (slot 1)
//   ?CreateNewObject@Internal@Object@@SAPEAVBase@2@XZ      0x18002fc80  XOR EAX,EAX; RET
//   ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z 0x1800070c0 XOR EAX,EAX; RET
//   ?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A         0x1801f9c18
//
// 0x1801f9c18 is `_typeInfo` + 0x00 (typeId) and 0x1801f9c20 is `_typeInfo` +
// 0x08 (name) — which is what identifies those two two-instruction bodies as the
// macro's, and is why `DECLARE_TYPE` reproduces them over the class's own
// record rather than over a pair of hand-named globals.
//
// *** BOTH FACTORY FUNCTIONS RETURN NULL IN RETAIL. *** `Object::Internal` is
// never created through `Factory::CreateObject`; only its subclasses are. The
// two `XOR EAX,EAX` bodies are transcribed as `return nullptr`, NOT as an
// abort: `InternalFactory::PersistObject`'s read path calls `factoryFn` and then
// dereferences the result unguarded, so a save naming "Internal" itself faults
// exactly where retail faults.
//
// A SECOND RECORD FOR THIS TYPE NOW EXISTS, AND IT IS HARMLESS — say it out loud
// rather than let the next reader find it. `standalone/rt_data_mint_tables.inc`
// mints `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A` at 0x1801f9c18
// (kTypeInfoStorage[52]) and `standalone/rt_data_mint.cpp`'s `MintStorage()`
// SPLICES the minted nodes in FRONT of whatever CRT-init left in
// `BaseTypeInfo::_firstTypeInfo` — it was written that way, by name, so that a
// `DEFINE_TYPE` static could exist one day (B33_RULES §9.3). So:
//   * the minted record is walked FIRST by `Factory::Startup`, is the one that
//     wins the AVL insert, and stays the one behind the `_typeInfo@Internal`
//     name door that internalfactory_persist.cpp reads;
//   * the record below is also walked, also gets `typeId = CRC("Internal")` in
//     pass 1, and its `Insert` finds the existing key and returns that node — so
//     it adds NO node, NO child link and NO second type id;
//   * `Internal::Type()`/`TypeId()` therefore answer the same value as the
//     minted `?Type@Internal@Object@@UEBAIXZ` does, 0xFFFFFFFF before Startup
//     and CRC("Internal") after it.
// The one difference is the two factory pointers: the minted row carries
// `RtMintTiTrapStub_52_*`, this one carries the real `return nullptr` bodies,
// which is what retail has. Nothing reads them for this type.
DEFINE_TYPE(Object::Internal, "Base", nullptr)

// 0x18002fc80 / 0x1800070c0 — `XOR EAX,EAX; RET`, both of them.
Base* Internal::CreateNewObject()              { return nullptr; }
Base* Internal::CreateInPlaceObject(void*)     { return nullptr; }

// ============================================================================
// Object::Internal — lifetime
// ============================================================================

// ??0Internal@Object@@QEAA@XZ   retail 0x1800bfd10 / profile 0x18008b370
//
// The two vptr stores retail makes at 0x1800bfd2b and 0x1800bfd35 are the
// constructor chain (Base's, then Internal's own) and GCC emits both for us.
// They are NOT written by hand: a hand-written store would name the MINTED
// table address, and an object built by this constructor carries the COMPILER's.
// tests/unit/object_vtable_abi_test.cpp is why that is safe — the two tables are
// slot-for-slot interchangeable in both directions.
Internal::Internal() {
    _factoryIndex = -1;   // 0x1800bfd3f — dead: AddObject overwrites it
    _generation   = 0;    // 0x1800bfd46 — dead likewise. Reproduced anyway.
    InternalFactory::Get()->AddObject(this);
}

// ??0Internal@Object@@QEAA@AEBV01@@Z   retail 0x1800064e0
// It copies the two dwords and NOTHING ELSE — no AddObject. See the header.
Internal::Internal(const Internal& other)
    : Base(),
      _factoryIndex(other._factoryIndex),
      _generation(other._generation) {}

// ??4Internal@Object@@QEAAAEAV01@AEBV01@@Z   retail 0x180006430
Internal& Internal::operator=(const Internal& other) {
    _factoryIndex = other._factoryIndex;   // 0x180006430
    _generation   = other._generation;     // 0x180006436
    return *this;                          // MOV RAX,RCX
}

// ??1Internal@Object@@UEAA@XZ   retail 0x1800bfdb0 / profile 0x18008b400
//
// The `vptr = &Internal::_vftable_` at 0x1800bfdc6 and the
// `vptr = &Base::_vftable_` at 0x1800bfe13 are the destructor chain, emitted by
// the compiler. The only body is the two lines below.
Internal::~Internal() {
    _generation = 0;                                 // 0x1800bfdd0
    InternalFactory::Get()->RemoveObject(this);      // 0x1800bfdd7..0x1800bfe0d
}

// ---- vtable slot 0 — 0x180006440 -------------------------------------------
//
// MSVC's VECTOR deleting destructor. Two arms, transcribed:
//
//   TEST DL,0x2 -> the ARRAY arm (0x180006454): reads the element count at
//                  `this - 8`, calls the EH vector-destructor helper
//                  (0x18007ec98) with elem size 0x10 and ~Internal
//                  (0x1800bfdb0), optionally frees `this - 8`, returns
//                  `this - 8`. `Object::Destroy` never takes it (flags 1) and
//                  nothing in retail `new[]`s an Internal, so it is transcribed
//                  as the faithful UNREACHABLE it is rather than reimplemented
//                  against an array cookie our allocator does not write. Taking
//                  it is a bug upstream and it faults at `this - 8` exactly as
//                  retail would.
//   else        -> 0x180006491: CALL ~Internal, then `TEST SIL,0x1` /
//                  CALL 0x180006a10 (the free), then RAX = this.
Base* Internal::DeletingDtor(unsigned flags) {
    if (flags & 2) {
        // The array arm. Faithful, and faithfully unreachable: the cookie read
        // faults loudly instead of silently destroying one element.
        const uint64_t count = *reinterpret_cast<const uint64_t*>(
            reinterpret_cast<const char*>(this) - 8);
        char* const base = reinterpret_cast<char*>(this) - 8;
        for (uint64_t i = count; i-- > 0;)
            reinterpret_cast<Internal*>(reinterpret_cast<char*>(this) + i * 0x10)->~Internal();
        if (flags & 1)
            detail::InternalMemFree(base);
        return reinterpret_cast<Base*>(base);
    }
    this->~Internal();                          // 0x180006491
    if (flags & 1)
        detail::InternalMemFree(this);          // 0x18000649f -> 0x180006a10
    return this;                                // 0x1800064a4
}

// ---- vtable slot 3 — 0x1800bfda0, a bare RET in retail AND in the profile
// build. It exists so a subclass has something to override. MUST STAY EMPTY.
void Internal::Persist(void* /*stream*/) {
}

// ============================================================================
// Object::InternalFactory
// ============================================================================

// ?Get@InternalFactory@Object@@SAPEAV12@XZ   retail 0x18002f850 —
// `MOV RAX,[0x1801fc6d8]; RET`.
//
// The cell is `?_singleton@InternalFactory@Object@@0PEAV12@EA`, reached through
// the port door for the same reason `Object::Factory::Get()` is (B33_RULES
// §9.3): it is only ever read at RUNTIME, never from a static initialiser, so
// the door answers it and no second cell is created. `reg_mint_promoted.cpp`
// records that row as UNPROMOTABLE while the DLL is present and REQUIRED at
// cutover; nothing here changes that.
#if defined(BANISHED_UNIT_TEST)
namespace detail {
// A unit test builds its own factory and installs it for the duration of a
// case. Production takes the door below; this pointer does not exist there.
InternalFactory* g_unitInternalFactory = nullptr;
}  // namespace detail
InternalFactory* InternalFactory::Get() { return detail::g_unitInternalFactory; }
#else
InternalFactory* InternalFactory::Get() {
    InternalFactory** slot = RT_DATA_SLOT(InternalFactory**,
        "?_singleton@InternalFactory@Object@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}
#endif

// ?Startup@InternalFactory@Object@@QEAA_NXZ   retail 0x1800c0020 — `MOV AL,1; RET`.
bool InternalFactory::Startup() { return true; }

// ?Shutdown@InternalFactory@Object@@QEAAXXZ   retail 0x1800c0010 — bare `RET`.
// EMPTY IN RETAIL. The handle table is not drained at shutdown; every object
// removes itself in its own destructor. Do not "complete" this.
void InternalFactory::Shutdown() {}

// ?AddObject@InternalFactory@Object@@QEAAXPEAVInternal@2@@Z   retail 0x1800c00c0
//
//   0x1800c00cf  EAX  = _objects._size
//   0x1800c00d9  obj->_factoryIndex = EAX
//   0x1800c00df  CALL 0x1800c7f90  Random::Rand(&_random)     (RCX = this + 0x20)
//   0x1800c00e4  obj->_generation = EAX
//   0x1800c00f4  CALL 0x18002ffa0  _objects.SetSize(_size + 1, &obj)
//
// The fill argument is the ADDRESS OF THE SPILLED `obj` ARGUMENT (LEA R8,[RSP+0x38]),
// i.e. a `const Internal*&`, which is why the local below exists.
void InternalFactory::AddObject(Internal* obj) {
    obj->_factoryIndex = _objects._size;
    obj->_generation   = detail::InternalRand(&_random);
    Internal* elem = obj;
    detail::ArrayPtrSetSizeFill(&_objects, _objects._size + 1, &elem);
}

// ?RemoveObject@InternalFactory@Object@@QEAAXPEAVInternal@2@@Z  retail 0x1800c0110
//
// The swap-remove reference_internalfactory_swapremove_wild_write.md is about.
// THERE IS NO VALIDATION OF `idx` — not here and not in the profile build. If
// `idx` is stale the second store lands wherever the allocator has since put
// that block. That is retail's behaviour and this port keeps it; the fix for the
// family is upstream (unregister before freeing), never a guard here.
void InternalFactory::RemoveObject(Internal* obj) {
    const int last = _objects._size - 1;      // 0x1800c011b  DEC R10D
    const int idx  = obj->_factoryIndex;      // 0x1800c0114  MOVSXD RDX,[RDX+8]
    if (last != idx) {
        _objects._data[idx] = _objects._data[last];
        _objects._data[idx]->_factoryIndex = idx;
    }
    detail::ArrayPtrSetSize(&_objects, last); // 0x1800c0149  JMP 0x18002fca0
}

// ?ClearPersistState@InternalFactory@Object@@QEAAXXZ
//                                      retail 0x1800bfe30 / profile 0x18008c0b0
// Frees the persist-id array and zeroes all three of its fields. Retail inlines
// the free (FUN_18001c750); the profile build calls MemFree — and it calls it
// UNCONDITIONALLY, where retail's inline null-checks first. Same behaviour:
// MemoryAllocator::Free opens with `test rdx,rdx; jz`.
//
// It touches `_persistIds` and NEVER `_objects`, which is why it is not part of
// the handle-table hazard family: `_persistIds` has exactly two writers in the
// whole engine, this function and `ResetPersistState`.
void InternalFactory::ClearPersistState() {
    if (_persistIds._data != nullptr)
        detail::InternalMemFree(_persistIds._data);
    _persistIds._data     = nullptr;
    _persistIds._size     = 0;
    _persistIds._capacity = 0;
}

#if !defined(BANISHED_UNIT_TEST)

// ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
//                                      retail 0x1800bfe40 / profile 0x18008bfc0
//
//     Serializer ser(stream, 0);              // inlined; see below
//     int count = _objects._size;
//     ser.readerWriter->vtbl[0xa0](rw, &count, 0);   // WRITE on save, READ on load
//     _persistIds.SetSize(count);
//     _objects.Reserve(count);
//     for (i = 0; i < _persistIds._size; ++i) _persistIds[i] = -1;
//     ~Serializer(ser);                       // rw->vtbl[0](rw, 1)
//
// ARITY, closed from the bytes at 0x1800bfe68-0x1800bfe84 (Wave 9 lane Y):
//     MOV RCX,[RDX+0x10]   ; arg1 = stream->encoding (the factory)
//     XOR R8D,R8D          ; arg3 = 0                <-- SET BEFORE the test
//     CMP [RDX+0x8],EBX    ; direction == 0 ?
//     LEA RDX,[RSP+0x28]   ; arg2 = &serializer
//     CALL [RAX+0x18] / CALL [RAX+0x20]
// THREE arguments, identical on both branches, third a literal 0 — the
// serializer VERSION, which this call site does not use. Wave 4's IO::Serializer
// port pinned the same arity from the out-of-line 0x1800b9960, where
// BinaryWriter STORES that int and writes it to the stream.
//
// *** THE READERWRITER VTABLE OFFSETS ARE BUILD-SPECIFIC. *** 0xa0 below is the
// RETAIL number (Internal.h's kEncSerializeInt). The PROFILE build uses 0xb8 for
// the same call. Copying the profile number writes the right value through the
// wrong slot and desyncs every save.
void InternalFactory::ResetPersistState(void* stream) {
    RtObject::SerializerLite ser;
    ser.stream       = stream;
    ser.readerWriter = nullptr;

    char*  s       = static_cast<char*>(stream);
    void*  factory = *reinterpret_cast<void**>(s + 0x10);
    void** fvtbl   = *reinterpret_cast<void***>(factory);
    const int mode = *reinterpret_cast<int*>(s + 0x08);
    const int kSerializerVersion = 0;   // retail's XOR R8D,R8D at 0x1800bfe6c
    ser.readerWriter = reinterpret_cast<void* (*)(void*, void*, int)>(
        fvtbl[(mode == 0 ? 0x18 : 0x20) / 8])(factory, &ser, kSerializerVersion);

    void*  enc     = ser.readerWriter;
    void** encVtbl = *reinterpret_cast<void***>(enc);

    int count = _objects._size;
    reinterpret_cast<void (*)(void*, int*, const char*)>(
        encVtbl[RtObject::kEncSerializeInt / 8])(enc, &count, nullptr);

    detail::ArrayIntSetSize(&_persistIds, count);
    detail::ArrayPtrReserve(&_objects, count);

    // The bound is re-read from _persistIds._size on every iteration, as retail
    // does; it cannot change, but this family is byte-faithful first.
    for (int i = 0; i < _persistIds._size; ++i)
        _persistIds._data[i] = -1;

    if (enc != nullptr)
        reinterpret_cast<void (*)(void*, int)>(encVtbl[0])(enc, 1);
}

// ============================================================================
// ?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z
//                                                          retail 0x1800bff10
// ============================================================================
// Moved here UNCHANGED from runtime/src/object/internalfactory_persist.cpp,
// whose banner carries the full read-out of both arms and is still the place to
// read it. The three things that must survive any future tidy-up are restated
// because each one is load-bearing:
//
//   * THE DESCENT'S MISS ARM PRODUCES A NULL NODE AND RETAIL DEREFERENCES IT.
//     `MOV [RSI+0xc],R15D` at address 0xc is how a save file naming an
//     unregistered type announces itself. No guard is added (CLAUDE.md).
//   * `_persistIds._data` is RE-LOADED after the factory call (0x1800bffb4 /
//     0x1800bffbc) because the factory can have grown the table, and
//     `_objects._data` is RE-LOADED after the Serialize (0x1800bffea) because
//     Serialize can move the buffer. Caching either at the top returns a pointer
//     into a freed block.
//   * `_persistIds[i]` is a SENTINEL on the way in and an INDEX on the way out,
//     and the two arms store DIFFERENT things into it — the write path stores
//     `index` itself, the read path stores the NEW object's `_factoryIndex`.
//     That asymmetry is the function.
namespace {

// `?_singleton@Factory@Object@@0PEAV12@EA` — note the `0PEAV12@EA` spelling; the
// obvious `1PEAV12@A` is a phantom that resolves NULL silently
// (reference_resolveexport_phantom_names.md).
void* g_factorySingletonCache = nullptr;                 // Runtime 0x1801fc6e8

RtObject::Factory** FactorySingletonSlot() {
    return reinterpret_cast<RtObject::Factory**>(RtResolveCached(
        "?_singleton@Factory@Object@@0PEAV12@EA", &g_factorySingletonCache));
}

// `Object::Internal`'s own BaseTypeInfo node, read through the NAME door so a
// promotion of the minted row is followed. Field +0x00 is the type id.
//
// NOT `Internal::TypeId()`. The two agree after `Factory::Startup` (both are
// CRC("Internal")) and both are 0xFFFFFFFF before it, but this is a LOAD path
// and the door is what five in-DLL readers and reg_mint_promoted.cpp's row set
// are keyed on. Swapping it for the class's own record would be a change to what
// a load reads with no measurement behind it.
const uint8_t* InternalTypeInfoNode() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A");
}

// `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` (0x1800c0390), ported and registered
// by runtime/src/object/type_system.cpp. Reached through the registry so this
// call lands on OUR body with the DLL present or absent.
bool PersistIsDerivedFrom(const void* self, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    static void* cache = nullptr;
    Fn fn = reinterpret_cast<Fn>(
        RtResolveCached("?IsDerivedFrom@Base@Object@@QEBA_NI@Z", &cache));
    return fn(self, typeId);
}

// IO::Stream + 0x08 — `0 == reading`, non-zero == writing. The only field of
// Stream this body touches.
inline int32_t StreamMode(const void* stream) {
    return *reinterpret_cast<const int32_t*>(
        reinterpret_cast<const uint8_t*>(stream) + 0x08);
}

// vtable slot 3 (+0x18) — `Object::Internal::Persist(IO::Stream&)`. Spelled
// through the raw table rather than as `obj->Persist(stream)` because the object
// may be carrying the MINTED `??_7Internal@Object@@6B@`, whose slot 3 is the
// registered `?Persist@Internal@Object@@` shim. The two are slot-compatible
// (tests/unit/object_vtable_abi_test.cpp) and this is the instruction retail
// emits either way.
inline void SerializeVirtual(void* self, void* stream) {
    void* const* vt = *reinterpret_cast<void* const* const*>(self);
    reinterpret_cast<void (*)(void*, void*)>(vt[3])(self, stream);
}

}  // namespace

Internal* InternalFactory::PersistObject(void* stream, uint32_t generation,
                                         int index, uint32_t typeId) {
    int32_t* const persistIds = _persistIds._data;             // 0x1800bff27
    const int64_t i = index;                                   // MOVSXD RBX,R9D

    int32_t slot = index;

    if (StreamMode(stream) != 0) {
        // ---- WRITE, 0x1800bffd4 --------------------------------------------
        if (persistIds[i] < 0) {
            persistIds[i] = static_cast<int32_t>(i);
            Internal* obj = _objects._data[i];
            SerializeVirtual(obj, stream);
        }
        // `slot` stays `index`: EBX is never reassigned on this arm.
    } else {
        // ---- READ, 0x1800bff3d ---------------------------------------------
        slot = persistIds[i];
        if (slot < 0) {
            // ---- the type-tree descent, 0x1800bff5f ------------------------
            RtObject::Factory** const pSingleton = FactorySingletonSlot();
            RtObject::TypeNode* node  = pSingleton ? (*pSingleton)->root : nullptr;
            RtObject::TypeNode* found = nullptr;
            while (node != nullptr) {
                if (typeId < node->key)  { node = node->left;  continue; }
                if (node->key >= typeId) { found = node; break; }   // equal
                node = node->right;
            }
            // A MISS LEAVES `found` NULL AND RETAIL DEREFERENCES IT.
            RtObject::BaseTypeInfo* const info = found->value.info;   // node + 0x10
            // Zero-argument: retail leaves `info` in RCX at the call site but
            // the factory is a static that reads nothing.
            void* const created =
                reinterpret_cast<void* (*)()>(info->factoryFn)();     // CALL [RCX+0x20]

            const uint32_t internalTypeId =
                *reinterpret_cast<const uint32_t*>(InternalTypeInfoNode());
            Internal* obj = PersistIsDerivedFrom(created, internalTypeId)
                                ? static_cast<Internal*>(created)
                                : nullptr;                            // CMOVNZ RSI,RBX

            // UNGUARDED on purpose. Retail reads `_factoryIndex` (+0x08) BEFORE
            // it writes `_generation` (+0x0c); two distinct fields, so the order
            // is unobservable, and it is kept because an unobservable order that
            // is transcribed costs nothing while one that is tidied is a
            // difference nobody can find.
            const int32_t newIndex = obj->_factoryIndex;              // 0x1800bffad
            obj->_generation       = generation;                      // 0x1800bffb0
            // `_persistIds` is RE-LOADED here (0x1800bffb4 / 0x1800bffbc).
            _persistIds._data[i] = newIndex;
            slot = _persistIds._data[i];

            SerializeVirtual(obj, stream);                            // 0x1800bffca
        }
    }

    // 0x1800bffea — `_objects._data` is re-read AFTER the Serialize.
    return _objects._data[static_cast<int64_t>(slot)];
}

#endif  // !BANISHED_UNIT_TEST

}  // namespace Object
