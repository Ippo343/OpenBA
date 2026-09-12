// ============================================================================
// runtime/src/object/external_list.cpp — VTABLE BATCH 9
//   Object::ExternalList, the whole class (8 of 9 vtable slots; slot 4,
//   External::Compile, is already ported and shared with every ElementDescription
//   subclass).
// ============================================================================
//
// Layout: runtime/types/object/External.h already pins ExternalList (Wave 3
// lane C) as `External _base` at +0x00 plus `External** _resources` at +0x20,
// `int32_t _count` at +0x28 — the plain Array<External*> shape, sizeof 0x30
// (the deleting destructor's own `MOV EDX,0x30`).
//
// ??_7ExternalList@Object@@6B@ (retail 0x180154800), 9 slots:
//   0  deleting dtor     0x180007800  deleting_dtor.cpp row (sizeof 0x30)
//   1  TypeName          0x180007370  here
//   2  Type              0x180007350  here
//   3  GetFileVersion    0x1800bfa10  here  (returns 3)
//   4  External::Compile 0x1800066e0  already ported
//   5  Serialize         0x1800bfa20  here
//   6  Reconstruct       0x180007570  here
//   7  Destruct          0x180007640  here
//   8  Construct         0x1800075e0  here
// plus ??1ExternalList@Object@@UEAA@XZ (0x1800076c0), the deleting-dtor row's
// dtorVa.
//
// EVERY NON-SLOT HELPER THIS CLASS'S BODY REACHES IS ALREADY PORTED:
//   Array<ExternalPtr<External>>::~Array   0x180009460  (runtime/src/collection/array.cpp, J1_ENTRY)
//   Array<External*>::SetSize              0x18002f070  (runtime/src/collection/array.cpp, J3_ENTRY2)
//   Object::External::~External            0x1800bc950  (runtime/src/object/external.cpp)
//   Object::ExternalPtrArraySerialize_External  0x18002f170  (runtime/src/object/external_ptr_serialize.cpp)
// so this file is composition only, no new bodies for any of the four.
//
// ---------------------------------------------------------------------------
// ??1ExternalList@Object@@UEAA@XZ   0x1800076c0
// ---------------------------------------------------------------------------
//   FUN_180009460(this + 0x20);        // Array<ExternalPtr<External>>::~Array
//   Object::External::~External(this); // tail call in retail; ordinary call here
//
// ---------------------------------------------------------------------------
// ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z   0x1800bfa20
// ---------------------------------------------------------------------------
// Same Serializer-pair shape every ported ElementDescription's Serialize uses
// (runtime/src/ui/groupdescription.cpp documents the pair and the retail
// encoding-vtable offsets at length; not re-derived here):
//
//   pair = { stream, nullptr }
//   rw   = stream->mode == 0 ? factory->vt[0x18](factory,&pair,0)
//                            : factory->vt[0x20](factory,&pair,0)
//   n    = this->_count
//   rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) SetSize(&_resources, n)     0x18002f070
//   ExternalPtrArraySerialize_External(&pair, n, _resources) 0x18002f170
//   if (rw) rw->vt[0](rw, 1)
//   return true
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/object/External.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtObject {

namespace {

void* VtBase()         { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()     { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtExternalList() { return RT_DATA_SLOT(void*, "??_7ExternalList@Object@@6B@"); }

const uint8_t* ExternalListTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// Same Serializer-pair shape as runtime/src/ui/groupdescription.cpp; retail
// vtable offsets into Encoding/ReaderWriter, RETAIL not profile
// (reference_encoding_vtable_offsets_differ_between_builds.md).
struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;             // +0x08  0 == reading
    int32_t _pad0c;
    void*   encodingFactory;  // +0x10
};
struct Serializer {
    Stream* stream;   // +0x00
    void*   readerWriter;  // +0x08
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

constexpr int kEncCreateReader = 0x18;   // CALL [RAX+0x18]
constexpr int kEncCreateWriter = 0x20;   // CALL [RAX+0x20]
constexpr int kRwScalarInt     = 0xa0;   // CALL [RAX+0xa0]
constexpr int kRwDeletingDtor  = 0x00;   // CALL [RAX]

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

// The ExternalPtrArraySerialize_External thunk (runtime/src/object/
// external_ptr_serialize.cpp), reached by address since it is an unexported
// internal.
using ArraySerFn = void (*)(Serializer* ser, int count, External** elems);

}  // namespace

// ---- ?TypeName@ExternalList@Object@@UEBAPEBDXZ   0x180007370 --------------
extern "C" const char* RtObject_ExternalList_TypeName(const void*) {
    const char* v = nullptr;
    __builtin_memcpy(&v, ExternalListTypeInfo() + 8, sizeof v);
    return v;
}
// ---- ?Type@ExternalList@Object@@UEBAIXZ          0x180007350 --------------
extern "C" uint32_t RtObject_ExternalList_Type(const void*) {
    uint32_t v = 0;
    __builtin_memcpy(&v, ExternalListTypeInfo(), sizeof v);
    return v;
}
// ---- ?GetFileVersion@ExternalList@Object@@UEBAHXZ 0x1800bfa10 -------------
// `MOV EAX,3 ; RET` -- a literal, not ElementDescription's.
extern "C" int32_t RtObject_ExternalList_GetFileVersion(const void*) { return 3; }

// ---- ??1ExternalList@Object@@UEAA@XZ   0x1800076c0 -------------------------
extern "C" void RtObject_ExternalList_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    using ArrayDtorFn = void (*)(void*);
    RtAddr<ArrayDtorFn>(0x180009460ull)(p + 0x20);            // Array<ExternalPtr<External>>::~Array
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);  // 0x1800bc950
}

// ---- ?Construct@ExternalList@Object@@UEAAPEAVExternal@2@XZ   0x1800075e0 --
extern "C" void* RtObject_ExternalList_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);
    *v = VtBase();
    *v = VtExternal();
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *v = VtExternalList();
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;   // _resources
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;   // _count + _pad2c
    return selfv;
}

// ---- ?Reconstruct@ExternalList@Object@@UEAAPEAVExternal@2@XZ   0x180007570
// slot0(this, 0) -- the deleting destructor with flag 0 (destroy without
// freeing) -- then the ctor body again, with NO null test (retail derefs
// `this` unconditionally to reach the vtable, so a null here faults in retail
// too).
extern "C" void* RtObject_ExternalList_Reconstruct(void* selfv) {
    using Slot0Fn = void* (*)(void* self, unsigned flag);
    reinterpret_cast<Slot0Fn>(VTableOf(selfv)[0])(selfv, 0);
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);
    *v = VtBase();
    *v = VtExternal();
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *v = VtExternalList();
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    return selfv;
}

// ---- ?Destruct@ExternalList@Object@@UEAAXXZ   0x180007640 -----------------
// `slot0(this, 0)` -- retail's tail jump through vtable slot 0 with flag 0.
extern "C" void RtObject_ExternalList_Destruct(void* selfv) {
    using Slot0Fn = void* (*)(void* self, unsigned flag);
    reinterpret_cast<Slot0Fn>(VTableOf(selfv)[0])(selfv, 0);
}

// ---- ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z   0x1800bfa20
extern "C" bool RtObject_ExternalList_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };

    void* factory = stream->encodingFactory;
    void** fvt    = VTableOf(factory);
    const int which = (stream->mode == 0) ? kEncCreateReader : kEncCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;

    int32_t n = *reinterpret_cast<int32_t*>(p + 0x28);   // _count
    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    reinterpret_cast<ScalarFn>(VTableOf(rw)[kRwScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x18002f070ull)(p + 0x20, n);
    }

    RtAddr<ArraySerFn>(0x18002f170ull)(&ser, n, *reinterpret_cast<External***>(p + 0x20));

    if (rw != nullptr) {
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(VTableOf(rw)[kRwDeletingDtor / 8])(rw, 1);
    }
    return true;
}

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
int RtObjectExternalListRegister() {
    using namespace RtObject;
    int n = 0;

    RegisterRuntimeExport("?TypeName@ExternalList@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_ExternalList_TypeName); ++n;
    RegisterRuntimeAddr(0x180007370ull, (void*)&RtObject_ExternalList_TypeName);

    RegisterRuntimeExport("?Type@ExternalList@Object@@UEBAIXZ",
                          (void*)&RtObject_ExternalList_Type); ++n;
    RegisterRuntimeAddr(0x180007350ull, (void*)&RtObject_ExternalList_Type);

    RegisterRuntimeExport("?GetFileVersion@ExternalList@Object@@UEBAHXZ",
                          (void*)&RtObject_ExternalList_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x1800bfa10ull, (void*)&RtObject_ExternalList_GetFileVersion);

    RegisterRuntimeExport("?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtObject_ExternalList_Serialize); ++n;
    RegisterRuntimeAddr(0x1800bfa20ull, (void*)&RtObject_ExternalList_Serialize);

    RegisterRuntimeExport("?Reconstruct@ExternalList@Object@@UEAAPEAVExternal@2@XZ",
                          (void*)&RtObject_ExternalList_Reconstruct); ++n;
    RegisterRuntimeAddr(0x180007570ull, (void*)&RtObject_ExternalList_Reconstruct);

    RegisterRuntimeExport("?Destruct@ExternalList@Object@@UEAAXXZ",
                          (void*)&RtObject_ExternalList_Destruct); ++n;
    RegisterRuntimeAddr(0x180007640ull, (void*)&RtObject_ExternalList_Destruct);

    RegisterRuntimeExport("?Construct@ExternalList@Object@@UEAAPEAVExternal@2@XZ",
                          (void*)&RtObject_ExternalList_Construct); ++n;
    RegisterRuntimeAddr(0x1800075e0ull, (void*)&RtObject_ExternalList_Construct);

    RegisterRuntimeExport("??1ExternalList@Object@@UEAA@XZ",
                          (void*)&RtObject_ExternalList_Dtor); ++n;
    RegisterRuntimeAddr(0x1800076c0ull, (void*)&RtObject_ExternalList_Dtor);

    return n;
}
