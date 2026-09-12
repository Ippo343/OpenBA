// ============================================================================
// runtime/src/ui/characterset.cpp — UI::CharacterSet, the whole class
// ============================================================================
//
// ??_7CharacterSet@UI@@6B@ (retail 0x180156220), nine slots:
//   0  deleting dtor      0x1800378a0  deleting_dtor.cpp row (~Class 0x1800cb840,
//                                      sizeof 0x28 — both read off its own bytes)
//   1  TypeName           0x180037450  here
//   2  Type               0x180037430  here
//   3  GetFileVersion     0x1800cb830  here  (returns 1)
//   4  External::Compile  0x1800066e0  already ported (the base's slot)
//   5  Serialize          0x1800cb7b0  here
//   6  Reconstruct        0x180037650  here
//   7  Destruct           0x180037710  here
//   8  Construct          0x1800376b0  here
// plus ??1CharacterSet@UI@@UEAA@XZ 0x1800cb840, here.
//
// LAYOUT, sizeof 0x28 (`MOV EDX,0x28` in the deleting destructor's array arm at
// 0x1800378d2, and the `Allocate(0x28, 8)` in the already-ported factory row):
//     +0x00  vtable        Object::Base -> Object::External -> UI::CharacterSet
//     +0x08 .. +0x1f       Object::External's three words
//     +0x20  System::String   the one member: the character-set string
// The field is NOT named beyond that here. Serialize proves it is one
// System::String and the destructor proves the class OWNS it; neither says what
// the characters mean, and one class is not enough to name a field.
//
// THE CONSTRUCTOR IS NOT IN THIS FILE, and deliberately. It is INLINED into
// both `Construct` (0x1800376b0) and `Reconstruct` (0x180037650) — there is no
// out-of-line `??0CharacterSet@UI@@` at all — and the same seven stores are
// ALREADY PORTED AND REGISTERED, table-driven, by runtime/src/ui/factory_ui1.cpp
// (the `kCharacterSet` store table in factory_ui1_tables.inc, transcribed from
// `CreateInPlaceObject`'s copy at 0x1800375a3..0x1800375d0). The two bodies
// below transcribe the stores again because retail transcribed them again:
// they are straight-line code in these functions, not a call, and a call here
// would be a departure, not a simplification.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()        { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()    { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtCharacterSet() { return RT_DATA_SLOT(void*, "??_7CharacterSet@UI@@6B@"); }

const uint8_t* CharacterSetTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@CharacterSet@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// IO::Stream / IO::Serializer, the two-field pair every Serialize in this
// family builds on its stack. Same shape as animatedimagedescription.cpp's.
struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// The seven stores retail inlines into Construct and Reconstruct, byte for
// byte (0x1800376c6..0x1800376fa and 0x18003766e..0x1800376a2 — the two copies
// are identical). Three consecutive writes to +0x00 are the inlined
// Object::Base and Object::External constructors followed by this class's own
// vtable stamp; they are reproduced as three stores because that is what runs.
void InlineCtorStores(uint8_t* p) {
    *reinterpret_cast<void**>(p)        = VtBase();          // 1800376c6
    *reinterpret_cast<void**>(p)        = VtExternal();      // 1800376d0
    *reinterpret_cast<void**>(p + 0x08) = nullptr;           // 1800376e5
    *reinterpret_cast<void**>(p + 0x10) = nullptr;           // 1800376e8
    *reinterpret_cast<void**>(p + 0x18) = nullptr;           // 1800376ec
    *reinterpret_cast<void**>(p)        = VtCharacterSet();  // 1800376f0
    *reinterpret_cast<void**>(p + 0x20) = nullptr;           // 1800376fa
}

}  // namespace

// 0x180037430: `MOV EAX,[0x1801f9cf8] ; RET` — the first dword of the class's
// own BaseTypeInfo node.
extern "C" uint32_t RtUi_CharacterSet_Type(const void*) {
    const uint8_t* ti = CharacterSetTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// 0x180037450: `MOV RAX,[0x1801f9d00] ; RET` — the same node at +8.
extern "C" const char* RtUi_CharacterSet_TypeName(const void*) {
    const uint8_t* ti = CharacterSetTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800cb830: `MOV EAX,0x1 ; RET`. Version 1 — the plainest in the family;
// the *Description classes run 0x104..0x10204.
extern "C" int32_t RtUi_CharacterSet_GetFileVersion(const void*) { return 1; }

// 0x1800376b0
extern "C" void* RtUi_CharacterSet_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                    // 1800376c1 TEST/JZ
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                            // 1800376fe MOV RAX,RCX
}
// 0x180037650: slot0(this, 0), then the same inlined constructor. NOT a tail
// jump — the stores are open-coded here too.
extern "C" void* RtUi_CharacterSet_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                     // 180037667
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                            // 1800376a6
}
// 0x180037710: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_CharacterSet_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1CharacterSet@UI@@UEAA@XZ   0x1800cb840
// ============================================================================
// Restamp, release the String at +0x20, tail-jump ??1External@Object@@
// (0x1800bc950 — `JMP`, not `CALL`, at 0x1800cb892).
//
// The String release is System::String's own refcount protocol, read at
// 0x1800cb860..0x1800cb88a and identical to ~GroupDescription's: the count
// lives at `s[-4]` and the allocation starts at `s[-8]`, and on the
// still-referenced path retail NULLS THE FIELD rather than leaving it —
// so the object never sees a pointer whose count it no longer holds.
extern "C" void RtUi_CharacterSet_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtCharacterSet();         // 1800cb85d

    char*& s = *reinterpret_cast<char**>(p + 0x20);
    if (s != nullptr) {                                      // 1800cb864
        --*reinterpret_cast<int32_t*>(s - 4);                // 1800cb869
        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {       // 1800cb86c
            s = s - 8;                                       // 1800cb872  the FIELD moves
            using MemFreeFn = void (*)(void*);
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(s);
        } else {
            s = nullptr;                                     // 1800cb882
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cb892 JMP
}

// ============================================================================
// ?Serialize@CharacterSet@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cb7b0
// ============================================================================
// The shortest Serialize in the UI family: make the reader/writer from the
// stream's encoding factory, hand the one String to System::String::Serialize
// (0x1800c9930), delete the reader/writer, return true.
//
// The read/write side is chosen by `stream->mode == 0` exactly as everywhere
// else in this family: factory vtable +0x18 for the reader, +0x20 for the
// writer (0x1800cb7e5 / 0x1800cb7ea).
extern "C" bool RtUi_CharacterSet_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                    // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;                // 1800cb7d0
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;     // 1800cb7da CMP / JNZ
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                   // 1800cb7ed

    using StrSerFn = void (*)(void* str, Serializer* ser);
    RtAddr<StrSerFn>(0x1800c9930ull)(p + 0x20, &ser);        // 1800cb7fb

    if (rw != nullptr) {                                     // 1800cb806
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(VTableOf(rw)[0])(rw, 1);  // 1800cb813
    }
    return true;                                             // 1800cb815 MOV AL,1
}

int RtUiCharacterSetRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@CharacterSet@UI@@UEBAIXZ",
                          (void*)&RtUi_CharacterSet_Type);                  ++n;
    RegisterRuntimeAddr(0x180037430ull, (void*)&RtUi_CharacterSet_Type);

    RegisterRuntimeExport("?TypeName@CharacterSet@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_CharacterSet_TypeName);              ++n;
    RegisterRuntimeAddr(0x180037450ull, (void*)&RtUi_CharacterSet_TypeName);

    RegisterRuntimeExport("?GetFileVersion@CharacterSet@UI@@UEBAHXZ",
                          (void*)&RtUi_CharacterSet_GetFileVersion);        ++n;
    RegisterRuntimeAddr(0x1800cb830ull, (void*)&RtUi_CharacterSet_GetFileVersion);

    RegisterRuntimeExport("?Construct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CharacterSet_Construct);             ++n;
    RegisterRuntimeAddr(0x1800376b0ull, (void*)&RtUi_CharacterSet_Construct);

    RegisterRuntimeExport("?Reconstruct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CharacterSet_Reconstruct);           ++n;
    RegisterRuntimeAddr(0x180037650ull, (void*)&RtUi_CharacterSet_Reconstruct);

    RegisterRuntimeExport("?Destruct@CharacterSet@UI@@UEAAXXZ",
                          (void*)&RtUi_CharacterSet_Destruct);              ++n;
    RegisterRuntimeAddr(0x180037710ull, (void*)&RtUi_CharacterSet_Destruct);

    RegisterRuntimeExport("??1CharacterSet@UI@@UEAA@XZ",
                          (void*)&RtUi_CharacterSet_Dtor);                  ++n;
    RegisterRuntimeAddr(0x1800cb840ull, (void*)&RtUi_CharacterSet_Dtor);

    RegisterRuntimeExport("?Serialize@CharacterSet@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_CharacterSet_Serialize);             ++n;
    RegisterRuntimeAddr(0x1800cb7b0ull, (void*)&RtUi_CharacterSet_Serialize);

    return n;
}
