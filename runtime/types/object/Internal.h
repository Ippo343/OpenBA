#pragma once
// ============================================================================
// runtime/types/object/Internal.h — Object::Internal / InternalBasePtr /
//                                   InternalFactory  (Wave 3, lane B)
// ============================================================================
//
// The engine-wide handle table. EVERY Object::Internal-derived object — every
// UI::Element, every DialogInstance, every AchievementInstance, every
// GoalInstance — enters this table in its constructor and leaves it in its
// destructor, and `InternalPtr<T>` (the 103 template instantiations in the
// export table) is a bare pointer plus this table's index/generation pair,
// used ONLY by serialization.
//
// EVIDENCE. Every offset below was read out of a decompiled Runtime-x64.dll
// body and the body is named next to it. Where the retail build inlines a
// template, the PROFILE build (Runtime-x64-profile.dll, asserts on) is quoted
// as the independent witness — it calls the same helpers out of line and its
// assert strings name the fields.
//
// ORIGINAL SOURCE FILES (profile-build strings, docs/runtime_port/
// original_source_tree.txt): there is NO `Object\Internal.cpp`. The three
// classes live in
//     Object\InternalFactory.cpp
//     C:\Projects\Buildy\code\Runtime\Object/InternalFactory.h   (assert at :39)
//     c:\projects\buildy\code\runtime\Object/InternalPtr.h
// so `runtime/src/object/internal.cpp` is our name, not theirs. Recorded here
// because the file-name mirror in runtime/src/object/README.md is otherwise a
// lie for this lane.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
#include "../system/Value.h"          // ::Random (0x9c4 bytes, index + mt[624])

namespace RtObject {

// ============================================================
// Object::Internal — 16 bytes.
//
//   +0x00  void*    vptr           (Object::Base's only member)
//   +0x08  int32    _factoryIndex  own slot in InternalFactory::_objects
//   +0x0c  uint32   _generation    InternalFactory::_random.Rand() at birth
//
// Pinned by ??0Internal@Object@@QEAA@XZ (retail 0x1800bfd10, profile
// 0x18008b370): after the two vtable stores it writes `-1` at +0x08 and `0` at
// +0x0c, then overwrites +0x08 with the factory's element count and +0x0c with
// a Rand() draw. ??1Internal (retail 0x1800bfdb0) reads +0x08 as the swap-
// remove index and zeroes +0x0c.
//
// There is no field past +0x0c: the profile ctor is literally
// `Base::Base(this); vptr = &Internal::_vftable_; _factoryIndex = -1;
//  _generation = 0; AddObject(_singleton, this);` and Base contributes only the
// vptr.
//
// VTABLE — 4 slots, and it is EXPORTED (??_7Internal@Object@@6B@,
// 0x180154720), so a port stamps the DLL's own vtable by name and never owns
// one of its own. Read out of .rdata at that address:
//     [0] 0x180006440  scalar deleting destructor  (flag 1 => operator delete)
//     [1] 0x1800064c0  TypeName()  -> const char*
//     [2] 0x1800064d0  Type()      -> unsigned int   (the type id)
//     [3] 0x1800bfda0  Persist(IO::Stream&)          (EMPTY in both builds)
// Object::Base's vtable (??_7Base@Object@@6B@, 0x180154788) has the first
// three. Slot indices are confirmed by two independent consumers:
// InternalBasePtr::Serialize calls `vtbl[0x10]` = slot 2 for the type id, and
// InternalFactory::PersistObject calls `vtbl[0x18]` = slot 3 to persist a
// freshly created object.
// ============================================================
struct Internal {
    void*    vptr;            // +0x00
    int32_t  factoryIndex;    // +0x08
    uint32_t generation;      // +0x0c
};
static_assert(offsetof(Internal, factoryIndex) == 0x08, "Internal::_factoryIndex at +0x08");
static_assert(offsetof(Internal, generation)   == 0x0c, "Internal::_generation at +0x0c");
static_assert(sizeof(Internal) == 0x10, "Object::Internal is 16 bytes");
// ENDGAME B.3.3 step 5. This POD is now an ALIAS for the real class
// `Object::Internal` below, and it must STAY TRIVIALLY DESTRUCTIBLE: the ported
// bodies in runtime/src/object/ hold `RtObject::Internal` by pointer and the
// registered `RtObject_Internal_Dtor` shim is called BY HAND out of a vtable
// slot. Give the POD a destructor and the compiler runs a second one —
// B31_RULES' `RtSystem::String` double-free lesson, verbatim. The assertion
// itself is below, next to <type_traits>.

// ============================================================
// Object::InternalBasePtr — 8 bytes, one raw pointer. NOT refcounted, NOT
// weak, NOT validated.
//
// Pinned by all four of its trivial members, each a single instruction:
//   ??0InternalBasePtr@Object@@QEAA@XZ            0x1800062c0  `*this = 0`
//   ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z 0x1800062a0 `*this = arg`
//   ?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z  0x1800062b0  `*this = *arg`
//   ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ    0x1800c0150  reads only +0x00
//
// *** CheckPtr AND AssignId ARE BARE `RET`s IN BOTH BUILDS. ***
// ?CheckPtr@InternalBasePtr@Object@@IEBAXXZ (retail 0x1800c0290, profile
// 0x18008c5d0) and ?AssignId@InternalBasePtr@Object@@IEAAXXZ (retail
// 0x1800c0280, profile 0x18008c5e0) contain nothing but `ret` — in the PROFILE
// build too, which is the build with System::Debug::Assert calls compiled in
// and 5,182 exports. So this is NOT "a debug-build leftover that got optimised
// out of retail" (the claim in engine/funcs/engine_funcs.h:1983): the bodies
// were compiled out at the SOURCE level in every shipped configuration, and no
// build of this engine has ever validated an InternalBasePtr.
//
// The consequence, stated plainly because a lot of our code is written as if
// the opposite were true: a stale InternalBasePtr NEVER self-invalidates, in
// retail or here. The handle table exists for SERIALIZATION — Serialize writes
// {index, generation, typeId} and InternalFactory::PersistObject resolves them
// back — not for weak-reference safety. Retail clears references explicitly at
// removal time and relies on that.
// ============================================================
struct InternalBasePtr {
    Internal* ptr;            // +0x00
};
static_assert(sizeof(InternalBasePtr) == 0x08, "Object::InternalBasePtr is 8 bytes");

// ============================================================
// Object::InternalFactory — 0x9e4 (2532) bytes.
//
//   +0x00  Collection::Array<Internal*> _objects      {data, size, capacity}
//   +0x10  Collection::Array<int32_t>   _persistIds   {data, size, capacity}
//   +0x20  System::Random               _random       (0x9c4 bytes)
//
// _objects is pinned by ??0InternalFactory (0x1800c0030, zeroes +0x00/+0x08),
// AddObject (0x1800c00c0, `SetSize(_objects, _size + 1, &obj)`), RemoveObject
// (0x1800c0110, `table[i] = table[last]` off +0x00 with the count at +0x08),
// and the exe-side audit that has been walking it for months
// (code/Game/Object/internal_factory_audit.h).
//
// _persistIds is pinned by ClearPersistState (0x1800bfe30 — retail inlines
// `Free(_data); _data = 0; *(u64*)(this+0x18) = 0`, and the PROFILE build,
// 0x18008c0b0, spells it out as `MemFree(*(void**)(this+0x10)); *(u64*)(this+
// 0x10) = 0; *(u64*)(this+0x18) = 0`) and by ResetPersistState (0x1800bfe40),
// which resizes it to the object count and fills it with -1 through a 4-byte
// stride — element size 4. The profile build's assert inside that fill loop is
// "index >= 0 && index < _size" from Collection/Array.h, bounding on the dword
// at +0x18, which is therefore the SIZE field of the array at +0x10.
//
// _random at +0x20 is pinned twice: the ctor seeds 624 words starting at +0x24
// with the standard MT init_genrand recurrence (so index is the dword at
// +0x20 and mt[] starts at +0x24, exactly ::Random's layout), and both the
// ctor and AddObject call `System::Random::Rand((Random*)(this + 0x20))`.
//
// SIZE is pinned exactly by the copy constructor
// ??0InternalFactory@Object@@QEAA@AEBV01@@Z (0x18002f890): after copy-assigning
// the two arrays it copies the tail from +0x20 with a 16-byte-aligned unrolled
// loop (19 x 0x80 bytes, then 4 x 0x10, then a trailing dword) whose scalar
// fallback is `memcpy(dst + 0x20, src + 0x20, 0x9c4)`. So the last BYTE the
// class occupies is 0x20 + 0x9c4 - 1 = 0x9e3, and because the type contains a
// pointer its alignment is 8, giving sizeof == 0x9e8 with 4 bytes of trailing
// pad. Nothing reads that pad.
//
// The singleton is `?_singleton@InternalFactory@Object@@0PEAV12@EA`, an
// exported DATA symbol at 0x1801fc6d8 holding an InternalFactory*. The factory
// is NOT embedded in Engine the way the allocator is: Engine::GetInternalFactory
// (0x18000c910) returns `*(InternalFactory**)(engine + 0x6a8)` — a pointer
// member, not an embedded object.
// ============================================================
struct InternalFactory {
    Collection::Array<Internal*> objects;      // +0x00
    Collection::Array<int32_t>   persistIds;   // +0x10
    ::Random                     random;       // +0x20
};
static_assert(offsetof(InternalFactory, objects)    == 0x00, "_objects at +0x00");
static_assert(offsetof(InternalFactory, persistIds) == 0x10, "_persistIds at +0x10");
static_assert(offsetof(InternalFactory, random)     == 0x20, "_random at +0x20");
static_assert(sizeof(InternalFactory) == 0x9e8,
              "Object::InternalFactory: 0x9e4 bytes of fields, padded to 0x9e8 by alignof 8");

// ============================================================
// IO::Serializer, the 16 bytes of it that ResetPersistState needs.
//
// Reproduced here rather than taken from a Wave 4 header, which does not exist
// yet. Pinned by ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z (0x1800b9960) and by
// runtime/src/io/serializer.cpp's forwarder table:
//   +0x00  IO::Stream*    the stream (what PersistObject is handed)
//   +0x08  IO::Encoding*  a BinaryReader on load / BinaryWriter on save; every
//                         Serialize<T> forwarder is `jmp [encoding->vtbl+N]`
// The `int version` argument of the constructor is IGNORED by the body.
//
// The Stream fields the inlined constructor reads:
//   stream + 0x08  int   mode: 0 == reading (create a decoder), else writing
//   stream + 0x10  Encoding factory object; its vtable slot 3 (+0x18) makes the
//                  reader and slot 4 (+0x20) makes the writer
// The encoding is destroyed through its own vtable slot 0 with flag 1.
//
// NOT PINNED: anything else in Stream or Encoding. Wave 4 owns those; this
// header claims only the four offsets above, each of which is a literal in the
// disassembly of 0x1800b9960 and 0x1800bfe40.
// ============================================================
// NAME CORRECTED 2026-09-07 (Wave 9). +0x08 is the READER/WRITER, not the
// Encoding -- Wave 4 established that (`Serializer+0x08 is a ReaderWriter*`,
// one of the three recorded facts it disproved), and the slot offsets this
// file goes on to use (0x78 bool / 0x98 uint / 0xa0 int) are `rw_vtbl`'s, not
// the Encoding's. The Encoding is reached from the STREAM, at Stream+0x10.
//
// The port was always correct -- the offsets are right and it never touched a
// real Encoding -- so this is a name that was right about the MECHANISM and
// wrong about the OWNER, which PLAN.md §6 step 8 names as its own error class
// and which cost this project months elsewhere (the `str-4`/`str-8` header).
// The old name is kept in the comment so a reader who meets it elsewhere
// recognises it.
struct SerializerLite {
    void* stream;         // +0x00  IO::Stream*
    void* readerWriter;   // +0x08  IO::ReaderWriter*  (was named `encoding`)
};
static_assert(sizeof(SerializerLite) == 0x10, "IO::Serializer is 16 bytes");

// Encoding vtable byte offsets used by InternalBasePtr::Serialize, taken from
// the RETAIL disassembly (0x1800c0180) and cross-checked against the forwarder
// table in runtime/src/io/serializer.cpp.
//
// *** These differ between builds. *** The PROFILE build's Serialize
// (0x18008c460) uses +0x90 / +0xb0 / +0xb8 for the same three calls, because
// its Encoding vtable carries extra virtuals. A port MUST use the retail
// numbers below; copying them out of a profile decompile writes the right
// values through the wrong slots.
enum : int {
    kEncSerializeBool = 0x78,   // Serialize(bool&,  const char*)
    kEncSerializeUint = 0x98,   // Serialize(uint&,  const char*)
    kEncSerializeInt  = 0xa0,   // Serialize(int&,   const char*)
};

// Object::Internal vtable slot byte offsets (see the vtable dump above).
enum : int {
    kInternalVtblScalarDtor = 0x00,
    kInternalVtblTypeName   = 0x08,
    kInternalVtblType       = 0x10,
    kInternalVtblPersist    = 0x18,
};

}  // namespace RtObject

// ============================================================================
// ENDGAME B.3.1 step 1 -- the REAL classes, under retail's names.
// ============================================================================
//
// `RtObject::InternalBasePtr` above stays declared (B31_RULES step 1: every
// existing vocabulary keeps working; the POD goes when its last user does --
// today runtime/src/achievement/goal.cpp:411 and the C shims below).
//
// TRIVIALLY COPYABLE AND TRIVIALLY DESTRUCTIBLE, ON PURPOSE AND BY ASSERTION.
// `Copy` (0x1800062b0) is `mov rax,[rdx]; mov [rcx],rax` -- a bare 8-byte copy,
// after which the two pointers are ALIASES. There is no refcount anywhere in
// this family: `InternalFactory::_objects` plus an explicit `new`/`delete` owns
// every `Internal`, and the {index, generation, typeId} triple exists for SAVE
// FILES, not for weak-reference safety (SMARTPTR_INVENTORY.md sections 1, 5.2).
// The `Collection::Array`/`LinkedList` bulk paths move these eight bytes with a
// plain qword copy, so a user-provided copy ctor or dtor here would be silently
// bypassed (section 5.4).
//
// *** CheckPtr AND AssignId ARE BARE `RET`s IN EVERY BUILD, PROFILE INCLUDED. ***
// They are transcribed below as empty bodies and MUST STAY EMPTY: retail never
// validates a stale handle, it relies on explicit clearing at removal time, and
// adding validation on deref would diverge from every save file and every
// caller (memory reference_checkptr_is_a_ret_in_every_build; hazard 5.1).
//
// RETAIL ADDRESSES (retail / profile):
//   ??0InternalBasePtr@Object@@QEAA@XZ                 0x1800062c0 / 0x180023900
//   ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z  0x1800062a0 / 0x1800238e0
//   ?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z       0x1800062b0 / 0x1800257d0
//   ?CheckPtr@InternalBasePtr@Object@@IEBAXXZ          0x1800c0290 / 0x18008c5d0
//   ?AssignId@InternalBasePtr@Object@@IEAAXXZ          0x1800c0280 / 0x18008c5e0
//   ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ         0x1800c0150 / 0x18008c5a0
//   ?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z
//                                                      0x1800c0180 / 0x18008c460
//   InternalPtr<T> array Serialize (ONE 180-byte body, two instantiations):
//     0x180009a20 Achievement::AchievementInstance
//     0x180009fa0 Achievement::GoalInstance
//   Re-verified 2026-09-13, function-size-bounded, with
//   tools/extptr_clone_check.py's diff: identical=55 position-only=2 REAL=1,
//   the one REAL instruction being the `_typeInfo` load. ONE BODY.
//
// There is no `Clone` for this family either -- copying IS `Copy`.
// ============================================================================

#include <new>
#include <type_traits>

namespace Object {

// Defined in runtime/src/object/internal.cpp, next to the ReaderWriter vtable
// slots and the `PersistObject` name door it needs -- `IO::Serializer`,
// `Object::Base` and `InternalFactory` are all still unconverted
// (SMARTPTR_INVENTORY.md section 4), so the body keeps reaching them exactly as
// the C body does.
void InternalBasePtrSerializeBody(RtObject::Internal** self,
                                  RtObject::SerializerLite& ser);
// Object::Base::IsDerivedFrom's ancestor walk, reached by name from
// runtime/src/object/type_system.cpp -- the typed half of InternalPtr<T>.
bool InternalPtrIsDerivedFrom(const void* obj, uint32_t typeId);

class InternalBasePtr {
public:
    // 0x1800062c0 -- one instruction, `mov qword ptr [rcx], 0`.
    InternalBasePtr() : _ptr(nullptr) {}
    // 0x1800062a0 -- one instruction, `mov qword ptr [rcx], rdx`.
    InternalBasePtr(RtObject::Internal* p) : _ptr(p) {}

    // Trivial by construction and by assertion; see the banner.
    InternalBasePtr(const InternalBasePtr&) = default;
    InternalBasePtr& operator=(const InternalBasePtr&) = default;
    ~InternalBasePtr() = default;

    RtObject::Internal* Get() const        { return _ptr; }
    RtObject::Internal* operator->() const { return _ptr; }
    RtObject::Internal& operator*()  const { return *_ptr; }
    explicit operator bool() const { return _ptr != nullptr; }
    bool operator==(const InternalBasePtr& o) const { return _ptr == o._ptr; }
    bool operator!=(const InternalBasePtr& o) const { return _ptr != o._ptr; }

    // ?DeletePtr@... 0x1800c0150 --
    //     p = _ptr; if (p) { p->vtbl[0](p, 1); _ptr = 0; }
    // Slot 0 with flag 1 is MSVC's scalar deleting destructor: it runs
    // ~Internal (and therefore InternalFactory's swap-remove) and frees the
    // block. The raw-vtable call is the one the C body already had; it becomes
    // a real virtual when Object::Internal is a class (ENDGAME B.3 step 3).
    void DeletePtr() {
        RtObject::Internal* p = _ptr;
        if (p != nullptr) {
            void** vtbl = *reinterpret_cast<void***>(p);
            reinterpret_cast<void (*)(RtObject::Internal*, int)>(vtbl[0])(p, 1);
            _ptr = nullptr;
        }
    }

    // ?Serialize@... 0x1800c0180. Writes/reads {present, index, generation,
    // type} through the ReaderWriter vtable and resolves the triple back
    // through InternalFactory::PersistObject on load.
    void Serialize(RtObject::SerializerLite& ser) {
        InternalBasePtrSerializeBody(&_ptr, ser);
    }

    // ---- the transitional entry points the C shims call --------------------
    // `Copy`, `CheckPtr` and `AssignId` are PROTECTED in retail (the `IEAA` /
    // `IEBA` manglings). The registered C shims are free functions and cannot
    // reach them, so each gets a one-line static forwarder that disappears with
    // the shim in step 2. Nothing else may call these.
    static void Shim_Copy(InternalBasePtr* self, const InternalBasePtr* other) {
        self->Copy(*other);
    }
    static void Shim_CheckPtr(const InternalBasePtr* self) { self->CheckPtr(); }
    static void Shim_AssignId(InternalBasePtr* self)       { self->AssignId(); }

protected:
    // ?Copy@... 0x1800062b0 -- `mov rax,[rdx]; mov [rcx],rax`. NOT refcounted.
    void Copy(const InternalBasePtr& other) { _ptr = other._ptr; }

    // ?CheckPtr@... 0x1800c0290 -- a bare `ret` in retail AND in the profile
    // build. DO NOT ADD VALIDATION HERE. See the banner.
    void CheckPtr() const {}

    // ?AssignId@... 0x1800c0280 -- the same three bytes. Likewise empty.
    void AssignId() {}

    RtObject::Internal* _ptr;   // +0x00
};

static_assert(sizeof(InternalBasePtr) == 0x08, "Object::InternalBasePtr is 8 bytes");
static_assert(std::is_trivially_copyable<InternalBasePtr>::value,
              "Copy (0x1800062b0) is a bare qword move and the array/list bulk "
              "paths memcpy these; a real copy ctor would be silently skipped");
static_assert(std::is_trivially_destructible<InternalBasePtr>::value,
              "no destructor exists in retail; DeletePtr is explicit and manual");

// ============================================================
// Object::InternalPtr<T> -- retail's TYPED wrapper over the same eight bytes.
//
// Single inheritance, no virtuals, so the layout is unchanged (asserted). The
// only typed operation retail emits is the array Serialize's per-element type
// check, which is `InternalBasePtr::Serialize` plus one `IsDerivedFrom` against
// `T::_typeInfo`'s first dword -- ONE body, both instantiations
// (SMARTPTR_INVENTORY.md section 3). Until `T` is a real C++ class with a
// `_typeInfo` static (ENDGAME B.3 step 3, decision 4) the type id is supplied
// by the caller, exactly as the C thunks read it through the name door.
// ============================================================
template <class T>
class InternalPtr : public InternalBasePtr {
public:
    InternalPtr() : InternalBasePtr() {}
    InternalPtr(T* p) : InternalBasePtr(reinterpret_cast<RtObject::Internal*>(p)) {}

    InternalPtr(const InternalPtr&) = default;
    InternalPtr& operator=(const InternalPtr&) = default;
    ~InternalPtr() = default;

    // TRANSITIONAL CAST, same reason as ExternalPtr<T>::Serialize: `T` is not a
    // class deriving from `Object::Internal` in this tree yet, so the downcast
    // cannot be spelled `static_cast` here. It becomes one in B.3 step 3.
    T* Get() const        { return reinterpret_cast<T*>(InternalBasePtr::Get()); }
    T* operator->() const { return Get(); }
    T& operator*()  const { return *Get(); }

    // 0x180009a20 / 0x180009fa0 -- the one array body, per element.
    void Serialize(RtObject::SerializerLite& ser, uint32_t typeId) {
        InternalBasePtr::Serialize(ser);
        void* obj = InternalBasePtr::Get();
        if (obj != nullptr && !InternalPtrIsDerivedFrom(obj, typeId))
            _ptr = nullptr;   // retail nulls the element on total mismatch
    }

    static void SerializeArray(RtObject::SerializerLite& ser, int count,
                               InternalPtr* elems, uint32_t typeId) {
        for (int i = 0; i < count; ++i)
            elems[i].Serialize(ser, typeId);
    }
};

static_assert(sizeof(InternalPtr<RtObject::Internal>) == 0x08,
              "Object::InternalPtr<T> adds nothing to InternalBasePtr");
static_assert(std::is_trivially_copyable<InternalPtr<RtObject::Internal> >::value,
              "the typed wrapper must stay as trivial as its base");
static_assert(std::is_trivially_destructible<InternalPtr<RtObject::Internal> >::value,
              "the typed wrapper must stay as trivial as its base");

// The POD alias's trivial destructibility, asserted where <type_traits> is in
// scope. See the note beside `RtObject::Internal` itself.
static_assert(std::is_trivially_destructible<RtObject::Internal>::value,
              "RtObject::Internal is an ALIAS POD for Object::Internal and is "
              "destroyed by hand through a vtable slot; a destructor here is a "
              "double free (B31_RULES, the RtSystem::String lesson)");

}  // namespace Object
