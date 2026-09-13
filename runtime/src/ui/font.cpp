// ============================================================================
// runtime/src/ui/font.cpp — UI::Font, the whole class plus its text metrics
// ============================================================================
//
// ??_7Font@UI@@6B@ (retail 0x180155b50), NINE slots — one fewer than the
// ElementDescription family, because UI::Font is a direct Object::External
// subclass and has no `CreateInstance`:
//
//   0  deleting dtor      0x180032130  NOT HERE — runtime/src/object/
//                                      deleting_dtor*.cpp owns that table. Its
//                                      array arm encodes ~Font 0x1801175a0 and
//                                      sizeof 0x90 (`LEA R9,[0x1801175a0]` and
//                                      `MOV EDX,0x90` at 0x180032148/0x18003214f).
//   1  TypeName           0x1800319c0  here
//   2  Type               0x1800319a0  here
//   3  GetFileVersion     0x1801176b0  here  (returns 8)
//   4  Compile            0x1801176c0  here  (returns 1 — NOT the base's
//                                      External::Compile; this class overrides
//                                      it with its own two-instruction body)
//   5  Serialize          0x1801176d0  here
//   6  Reconstruct        0x180031c40  here
//   7  Destruct           0x180031d80  here
//   8  Construct          0x180031ce0  here
//
// plus, off the vtable:
//   ??1Font@UI@@UEAA@XZ                          0x1801175a0  here
//   ?ComputeStringLayout@Font@UI@@                0x180116fd0  here
//   ?ComputeLineLayout@Font@UI@@                  0x180116c70  here
//   ?ComputeCharacterPosition@Font@UI@@           0x180116ee0  here
//
// NOT HERE, and deliberately: `?CreateNewObject@Font@UI@@` (0x180031ab0) and
// `?CreateInPlaceObject@Font@UI@@` (0x180031ba0) are ALREADY ported and
// registered, table-driven, by runtime/src/ui/factory_ui1.cpp — the `"Font"`
// row of factory_ui1_tables.inc, whose `kFont` store table carries the same 21
// constructor stores `Construct` transcribes below. `??3Font@UI@@SAXPEAX@Z`
// (0x180031a50), the class operator delete, is ported in
// runtime/src/ui/ui_operator_delete.cpp.
//
// Layout: runtime/types/ui/Font.h (sizeof 0x90).
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED AND THERE IS NO OUT-OF-LINE ??0Font@UI@@.
// ----------------------------------------------------------------------------
// `Construct` (0x180031ce0), `Reconstruct` (0x180031c40) and both already-
// ported factory steps each open-code the same 21 stores; the DLL exports no
// `??0Font@UI@@`. `InlineCtorStores` below is that sequence transcribed from
// the disassembly at 0x180031cf6..0x180031d67, exactly as
// runtime/src/ui/characterset.cpp's sibling does for its own class. Two details
// worth keeping visible:
//   * `+0x00` is written THREE TIMES (Object::Base's vtable, then
//     Object::External's, then Font's). Those are the two inlined base
//     constructors followed by this class's own stamp; all three stores run.
//   * `+0x58` is a DWORD store and `+0x5c` is never written at all
//     (`MOV dword ptr [RCX+0x58],EDX` at 0x180031d46 is followed by a QWORD
//     store to +0x60). So a fresh Font's `_spaceWidth` is whatever the
//     allocator handed back, and Serialize only ever writes it on the
//     reader-version-0 path. That is retail's behaviour, not an omission here.
//
// ----------------------------------------------------------------------------
// NO HIDDEN RETURN SLOT ANYWHERE IN THIS FILE. Checked against every body:
//   * `?Compile@...UEAA?AW4CompileResult@External@Object@@...` LOOKS like the
//     `?A` by-value-return shape that costs a hidden RDX pointer, and it is
//     NOT: `W4` is an enum, MSVC returns it in EAX, and the whole body is
//     `MOV EAX,0x1 ; RET` (0x1801176c0) with RDX never touched. The Filename&
//     stays in RDX.
//   * `ComputeStringLayout`'s `AEAH`/`AEAH`/`PEAULayout` are by-REFERENCE out
//     parameters on the stack (read at [RSP+0xe0], [RSP+0xe8], [RSP+0xf0]),
//     and the function returns `X` (void). No slot.
//   * `ComputeLineLayout` and `ComputeCharacterPosition` both return `H` (int)
//     in EAX.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/Font.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtFont()     { return RT_DATA_SLOT(void*, "??_7Font@UI@@6B@"); }

const uint8_t* FontTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@Font@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// IO::Stream / IO::Serializer, the two-field pair every Serialize in this
// family builds on its stack. Same shape as characterset.cpp's and
// animatedimagedescription.cpp's.
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

// ---- the dependencies these bodies call, all by address ---------------------
//
// ResolveAddr is registry-first, so each of these reaches our own port where
// one exists. Ported today: 0x1800c9930 (System::String::Serialize),
// 0x180004590 / 0x180014520 (the two ExternalPtr<>::Serialize doors),
// 0x1800c9fc0 (IO::TextCodec::DecodeOne), 0x18002fea0 (Array<int32>::SetSize),
// 0x1800c7ad0 (MemoryAllocator::MemFree), 0x18001c750 (Array<T>::~Array),
// 0x1800bc950 (Object::External::~External). NOT ported yet, and listed in
// this file's report: 0x180033c50, 0x180076e90, 0x1800774d0, 0x180077330,
// 0x180076d10, 0x180076a40.

using StringSerializeFn = void (*)(void* str, Serializer* ser);
void StringSerialize(void* str, Serializer* ser) {
    RtAddr<StringSerializeFn>(0x1800c9930ull)(str, ser);
}

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z — 0x1800c7ad0. Retail inlines the
// EnterCriticalSection(alloc+0x640) / Free / LeaveCriticalSection triple at
// each site; the exported function IS that triple with an extra `if (p)` in
// front of it, so calling it from inside retail's own `if (p)` is instruction
// for instruction the same work. runtime/src/collection/array.cpp:171 records
// the same equivalence for the same reason.
using MemFreeFn = void (*)(void* p);
void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

// IO::TextCodec::DecodeOne, 0x1800c9fc0. Every caller in this file passes
// format 2 (UTF-8) in BOTH the ECX and the arg-5 position, exactly as retail
// does (`MOV ECX,0x2` + `MOV dword ptr [RSP+0x20],0x2`).
using DecodeOneFn = int32_t (*)(int32_t fmtEcx, const void* src, const void* end,
                                int32_t* cp, int32_t fmtArg5);
int32_t DecodeUtf8(const void* src, const void* end, int32_t* cp) {
    return RtAddr<DecodeOneFn>(0x1800c9fc0ull)(2, src, end, cp, 2);
}

// The two literals Serialize measures when it meets a file written before
// reader version 1, and the one ComputeLineLayout elides with. Retail points
// at its own .rdata — 0x180159138 is "..." (three bytes, and the loop bound is
// literally 3) and 0x1801567e0 is " " — and both byte strings were read out of
// the shipped image to get them right.
//
// THEY ARE OUR OWN STORAGE HERE, NOT THOSE ADDRESSES. A raw 0x180159138 would
// be a Ghidra address, not a runtime one: the DLL is relocated by ASLR today
// and is not mapped at all after cutover. What the bodies consume is the
// CONTENT, and the content is three dots and a space.
const char kEllipsisText[] = "...";
const char kSpaceText[]    = " ";

// The 21 constructor stores, from 0x180031cf6..0x180031d67. `Reconstruct`
// carries a byte-identical copy at 0x180031c5e..0x180031ccf.
void InlineCtorStores(uint8_t* p) {
    *reinterpret_cast<void**>(p)          = VtBase();      // 180031cfd
    *reinterpret_cast<void**>(p)          = VtExternal();  // 180031d07
    *reinterpret_cast<void**>(p + 0x08)   = nullptr;       // 180031d15
    *reinterpret_cast<void**>(p + 0x10)   = nullptr;       // 180031d18
    *reinterpret_cast<void**>(p + 0x18)   = nullptr;       // 180031d1c
    *reinterpret_cast<void**>(p)          = VtFont();      // 180031d27
    *reinterpret_cast<void**>(p + 0x20)   = nullptr;       // 180031d2a
    *reinterpret_cast<void**>(p + 0x28)   = nullptr;       // 180031d2e
    *reinterpret_cast<void**>(p + 0x30)   = nullptr;       // 180031d32
    *reinterpret_cast<void**>(p + 0x38)   = nullptr;       // 180031d36  _size+_capacity
    *reinterpret_cast<void**>(p + 0x40)   = nullptr;       // 180031d3a
    *reinterpret_cast<void**>(p + 0x48)   = nullptr;       // 180031d3e  _size+_capacity
    *reinterpret_cast<void**>(p + 0x50)   = nullptr;       // 180031d42  +0x50 and +0x54
    *reinterpret_cast<int32_t*>(p + 0x58) = 0;             // 180031d46  DWORD; +0x5c is NOT written
    *reinterpret_cast<void**>(p + 0x60)   = nullptr;       // 180031d49
    *reinterpret_cast<void**>(p + 0x68)   = nullptr;       // 180031d4d
    *reinterpret_cast<void**>(p + 0x70)   = nullptr;       // 180031d51  +0x70 and +0x74
    *reinterpret_cast<void**>(p + 0x78)   = nullptr;       // 180031d55
    *reinterpret_cast<void**>(p + 0x80)   = nullptr;       // 180031d59
    *reinterpret_cast<void**>(p + 0x88)   = nullptr;       // 180031d60
}

// The System::String release the destructor performs four times over, from
// 0x1801175c5..0x1801175f8 (the +0x88 copy; the other three are identical).
// `data[-4]` is the refcount and `data-8` is the allocation base — see
// PLAN.md §4.3 risk 2, which records that `str-4`/`str-8` is String's header
// and NOT the allocator's.
//
// TWO ASYMMETRIES ARE RETAIL'S, NOT TYPOS. The refcount-hits-zero arm frees
// and leaves the field pointing 8 bytes BELOW the freed block; the
// still-shared arm nulls the field instead. Reproduced as written.
void ReleaseStringField(uint8_t* p, size_t off) {
    uint8_t** slot = reinterpret_cast<uint8_t**>(p + off);
    uint8_t*  data = *slot;
    if (data == nullptr) return;                                     // 1801175ce

    int32_t* refs = reinterpret_cast<int32_t*>(data - 4);
    --*refs;                                                         // 1801175d3
    if (*refs == 0) {                                                // 1801175d6
        *slot -= 8;                                                  // 1801175db
        MemFree(*slot);                                              // 1801175ea
    } else {
        *slot = nullptr;                                             // 1801175f1
    }
}

// ---- the reader/writer's scalar and array doors ----------------------------
using ScalarFn = void (*)(void* rw, void* value, const char* name);
using ArrayFn  = void (*)(void* rw, int32_t n, void* data, const char* name);
using VersionFn = int32_t (*)(void* rw);
using RwDtorFn  = void (*)(void* rw, int flag);

void ScalarInt(void* rw, void* value) {
    reinterpret_cast<ScalarFn>(VTableOf(rw)[rt::io::rw_vtbl::kScalarInt / 8])(rw, value, nullptr);
}

// The per-codepoint advance walk, shared by both of Serialize's measuring
// loops (0x180117a20..0x180117a93 and 0x180117ab0..0x180117b22 — the two are
// instruction for instruction the same, over different literals and bounds).
// It is written once here because retail's two copies are one inlined helper.
int32_t MeasureLiteral(const uint8_t* self, const char* text, int32_t bytes) {
    int32_t total = 0;   // R15D / EDI
    int32_t kern  = 0;   // R13D / R15D
    int32_t idx   = 0;   // EBX

    do {
        int32_t cp = 0;
        idx += DecodeUtf8(text + idx, text + bytes, &cp);            // 180117a3e

        if (cp >= 0 && cp < *reinterpret_cast<const int32_t*>(self + 0x38)) {
            const int32_t glyph =
                reinterpret_cast<const int32_t*>(
                    *reinterpret_cast<void* const*>(self + 0x30))[cp];
            if (glyph != 0xffff && glyph != -1 && glyph >= 0) {      // 180117a5c..a6f
                const uint8_t* sheet = *reinterpret_cast<const uint8_t* const*>(self + 0x60);
                if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                    const uint8_t* e = *reinterpret_cast<const uint8_t* const*>(self + 0x40) +
                                       static_cast<size_t>(glyph) * 0x10;
                    total += *reinterpret_cast<const int32_t*>(e + 8) +
                             *reinterpret_cast<const int32_t*>(e) + kern;   // 180117a81..a89
                    kern = *reinterpret_cast<const int32_t*>(e + 4);        // 180117a8c
                }
            }
        }
    } while (idx < bytes);                                           // 180117a90

    return total + kern;                                             // 180117a95
}

}  // namespace

// ============================================================================
// The four one-liners
// ============================================================================
// 0x1800319a0: `MOV EAX,[0x1801fab68] ; RET` — the first dword of the class's
// own BaseTypeInfo node. 0x1801fab68 IS `?_typeInfo@Font@UI@@0VBaseTypeInfo@
// Object@@A`, so this reads it through the name rather than the address.
extern "C" uint32_t RtUi_Font_Type(const void*) {
    const uint8_t* ti = FontTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// 0x1800319c0: `MOV RAX,[0x1801fab70] ; RET` — the same node at +8.
extern "C" const char* RtUi_Font_TypeName(const void*) {
    const uint8_t* ti = FontTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1801176b0: `MOV EAX,0x8 ; RET`. Eight — a plain integer, unlike the
// *Description family's packed 0x104 / 0x204 / 0x10204.
extern "C" int32_t RtUi_Font_GetFileVersion(const void*) { return 8; }
// 0x1801176c0: `MOV EAX,0x1 ; RET`. The Filename& in RDX is never read. See
// the banner: W4CompileResult comes back in EAX, there is no hidden slot.
extern "C" uint32_t RtUi_Font_Compile(void*, const void*) { return 1; }

// ============================================================================
// Construct / Reconstruct / Destruct
// ============================================================================
// 0x180031ce0: `TEST RCX,RCX ; JZ ret0`, then the inlined constructor, then
// `MOV RAX,RCX`. The null arm returns 0 (`XOR EDX,EDX ; MOV EAX,EDX`).
extern "C" void* RtUi_Font_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                            // 180031cf1
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                                    // 180031d67
}
// 0x180031c40: slot0(this, 0) — the deleting destructor with the free bit
// CLEAR, i.e. destroy in place — and then the same inlined constructor. NOT a
// tail jump: there is no out-of-line ctor to jump to.
extern "C" void* RtUi_Font_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                             // 180031c57
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                                    // 180031ccf
}
// 0x180031d80: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_Font_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1Font@UI@@UEAA@XZ   0x1801175a0
// ============================================================================
// Stamp, then tear down in REVERSE declaration order except that the three
// trailing strings go first:
//     [this] = ??_7Font@UI@@6B@                          ; 1801175c2
//     release String +0x88, +0x80, +0x78                 ; 1801175c5..18011764c
//     Array<FontGlyph>::~Array(this + 0x40)   0x180033c50 ; 180117654
//     Array<int32>::~Array   (this + 0x30)    0x18001c750 ; 18011765e
//     release String +0x20                                ; 180117664
//     JMP ~External                           0x1800bc950 ; 180117697
// The two array destructors are DIFFERENT functions and the difference is not
// cosmetic: 0x18001c750 is the plain `MemFree(_data); _data = 0; _size = 0`
// our tree already ports, while 0x180033c50 is the same body compiled for the
// 16-byte element. Both leave `_capacity` alone.
extern "C" void RtUi_Font_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtFont();                         // 1801175c2

    ReleaseStringField(p, 0x88);                                     // 1801175c5
    ReleaseStringField(p, 0x80);                                     // 1801175f8
    ReleaseStringField(p, 0x78);                                     // 18011762a

    using ArrayDtorFn = void (*)(void*);
    RtAddr<ArrayDtorFn>(0x180033c50ull)(p + 0x40);                   // 180117654
    RtAddr<ArrayDtorFn>(0x18001c750ull)(p + 0x30);                   // 18011765e

    ReleaseStringField(p, 0x20);                                     // 180117664

    using BaseDtorFn = void (*)(void*);
    RtAddr<BaseDtorFn>(0x1800bc950ull)(p);                           // 180117697 JMP
}

// ============================================================================
// ?Serialize@Font@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801176d0
// ============================================================================
// The reader/writer is built with THIRD ARGUMENT 2, not 0 (`LEA R8D,[RDI+0x2]`
// at 0x180117708, RDI being the zero register here). Every other Serialize in
// this family passes 0; this one does not, and the 2 is transcribed as-is.
//
// The field order below is the file format and is NOT the declaration order:
// +0x20, +0x50, +0x80, +0x70, +0x74, +0x88, +0x78, +0x28, +0x60, +0x68, then
// the two tables, then +0x54, then the pair +0x58/+0x5c.
//
// TWO VERSION GATES, both asking the READER/WRITER for its version
// (vt[0x08]), not this class's GetFileVersion:
//   * < 2  — the codepoint table was written as UNSIGNED SHORTS. Read them
//            into a scratch Array<uint16>, then widen each into the int32
//            table. The scratch buffer is freed by hand.
//   * >= 2 — the table is int32 and goes through vt[0x48] (kArrayInt) directly.
//   * < 1  — +0x58 and +0x5c were not in the file at all, so they are
//            RECOMPUTED by measuring "..." and " " with the font's own glyph
//            metrics. That is what gives ComputeLineLayout its ellipsis
//            reserve for an old file.
extern "C" bool RtUi_Font_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                            // [RBP-0x28], [RBP-0x20]

    void*  factory = stream->encodingFactory;                        // 180117704
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? rt::io::encoding_vtbl::kCreateReader
                                          : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int two);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 2);
    ser.readerWriter = rw;                                           // 180117720

    StringSerialize(p + 0x20, &ser);                                 // 18011772c
    ScalarInt(rw, p + 0x50);                                         // 18011773f
    StringSerialize(p + 0x80, &ser);                                 // 180117750
    ScalarInt(rw, p + 0x70);                                         // 180117763
    ScalarInt(rw, p + 0x74);                                         // 180117777
    StringSerialize(p + 0x88, &ser);                                 // 180117788
    StringSerialize(p + 0x78, &ser);                                 // 180117795

    using PairFn = void (*)(void* field, Serializer* ser);
    RtAddr<PairFn>(0x180076e90ull)(p + 0x28, &ser);                  // 1801177a2
    RtAddr<PairFn>(0x180004590ull)(p + 0x60, &ser);                  // 1801177af  ExternalPtr<SpriteSheet>
    RtAddr<PairFn>(0x180014520ull)(p + 0x68, &ser);                  // 1801177bc  ExternalPtr<MaterialInstance>

    void** rvt = VTableOf(rw);
    const int32_t version =
        reinterpret_cast<VersionFn>(rvt[rt::io::rw_vtbl::kGetVersion / 8])(rw);   // 1801177c8

    if (version < 2) {                                               // 1801177ce
        // ---- legacy uint16 codepoint table -----------------------
        struct { uint16_t* _data; int32_t _size; int32_t _capacity; } tmp =
            { nullptr, 0, 0 };                                       // [RBP-0x18]
        int32_t n = 0;                                               // [RBP+0x30]

        ScalarInt(rw, &n);                                           // 1801177f4

        if (ser.stream->mode == 0) {                                 // 1801177fe (re-read)
            using U16SetSizeFn = void (*)(void* array, int32_t n);
            RtAddr<U16SetSizeFn>(0x1800774d0ull)(&tmp, n);           // 18011780b
        }

        reinterpret_cast<ArrayFn>(rvt[rt::io::rw_vtbl::kArrayUShort / 8])(
            rw, n, tmp._data, nullptr);                              // 180117827

        using I32SetSizeFn = void (*)(void* array, int32_t n);
        RtAddr<I32SetSizeFn>(0x18002fea0ull)(p + 0x30, tmp._size);   // 180117833

        for (int32_t i = 0; i < tmp._size; ++i) {                    // 180117850
            // `MOV RAX,qword ptr [R14]` sits INSIDE the loop: the destination
            // pointer is re-read every iteration, not hoisted.
            int32_t* dst = *reinterpret_cast<int32_t**>(p + 0x30);
            dst[i] = tmp._data[i];
        }

        if (tmp._data != nullptr) MemFree(tmp._data);                // 180117867
        tmp._data = nullptr;                                         // 1801178a1
        tmp._size = 0;
        tmp._capacity = 0;                                           // 1801178a5, one qword
    } else {
        // ---- int32 codepoint table -------------------------------
        int32_t n = *reinterpret_cast<int32_t*>(p + 0x38);           // 1801178af
        ScalarInt(rw, &n);                                           // 1801178c4

        if (ser.stream->mode == 0) {                                 // 1801178ce
            using I32SetSizeFn = void (*)(void* array, int32_t n);
            RtAddr<I32SetSizeFn>(0x18002fea0ull)(p + 0x30, n);       // 1801178da
        }

        reinterpret_cast<ArrayFn>(rvt[rt::io::rw_vtbl::kArrayInt / 8])(
            rw, n, *reinterpret_cast<void**>(p + 0x30), nullptr);    // 1801178ef
    }

    // ---- the glyph table: four separate int32 scalars per row ----
    // NOT one kArrayInt call over 4*n ints — retail emits four kScalarInt
    // calls per glyph, which is what a NAMED text encoding needs even though
    // every name here is null.
    int32_t glyphs = *reinterpret_cast<int32_t*>(p + 0x48);          // 1801178f2
    ScalarInt(rw, &glyphs);                                          // 180117906

    if (ser.stream->mode == 0) {                                     // 180117910
        using GlyphSetSizeFn = void (*)(void* array, int32_t n);
        RtAddr<GlyphSetSizeFn>(0x180077330ull)(p + 0x40, glyphs);    // 18011791d
    }

    uint8_t* row = *reinterpret_cast<uint8_t**>(p + 0x40);           // 180117922
    if (glyphs > 0) {
        row += 8;                                                    // 18011792d
        for (int32_t i = 0; i < glyphs; ++i) {                       // 180117940
            ScalarInt(rw, row - 8);
            ScalarInt(rw, row - 4);
            ScalarInt(rw, row);
            ScalarInt(rw, row + 4);
            row += 0x10;                                             // 18011798f
        }
    }

    ScalarInt(rw, p + 0x54);                                         // 1801179a6

    const int32_t version2 =
        reinterpret_cast<VersionFn>(rvt[rt::io::rw_vtbl::kGetVersion / 8])(rw);   // 1801179b3

    if (version2 >= 1) {                                             // 1801179b6
        ScalarInt(rw, p + 0x58);                                     // 1801179c9
        ScalarInt(rw, p + 0x5c);                                     // 1801179dd
    } else if (ser.stream->mode == 0) {                              // 1801179ec
        // The encoding factory's slot 0 is a bool: only when it says yes are
        // the two widths recomputed. (`MOV RCX,[RCX+0x10] ; MOV RAX,[RCX] ;
        // CALL [RAX] ; TEST AL,AL`.)
        void* f = ser.stream->encodingFactory;                       // 1801179f6
        using FactoryFlagFn = bool (*)(void*);
        if (reinterpret_cast<FactoryFlagFn>(VTableOf(f)[0])(f)) {    // 1801179fd
            *reinterpret_cast<int32_t*>(p + 0x58) =
                MeasureLiteral(p, kEllipsisText, 3);                 // 180117a99
            *reinterpret_cast<int32_t*>(p + 0x5c) =
                MeasureLiteral(p, kSpaceText, 1);                    // 180117b28
        }
    }

    if (rw != nullptr) {                                             // 180117b32
        reinterpret_cast<RwDtorFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;                                                     // 180117b3e MOV AL,1
}

// ============================================================================
// ?ComputeLineLayout@Font@UI@@QEBAHPEBDHHHHHPEAULayout@12@@Z   0x180116c70
// ============================================================================
//   int ComputeLineLayout(const char* text, int count, int startX, int baseY,
//                         int maxWidth, int line, Layout* layout) const
//
// The parameter names come from the call sites, which pin every one of them:
// ComputeStringLayout passes its accumulated y in the `baseY` slot and its
// running line counter in `line`, and passes -1 in `maxWidth` on every wrapped
// segment (0x180117164, 0x18011720a, 0x180117360, 0x180117448) so the ellipsis
// test cannot fire; the single-line path passes ComputeStringLayout's own
// fourth parameter there instead (0x180117060).
//
// Returns the ADVANCE of the run, `penX + kern - startX`, which is why both
// exits recompute it from the saved copy of startX rather than from EBX (EBX
// is reused as the glyph-row pointer inside the loop, 0x180116d4f).
//
// THE ELLIPSIS ARM RECURSES INTO THIS SAME FUNCTION over the literal "...",
// and DISCARDS the recursive return value (0x180116e2f) — the value it hands
// back is recomputed from the pen at 0x180116e34. Not an oversight to
// "simplify": the recursive call is there for its SIDE EFFECT on `layout`.
extern "C" int32_t RtUi_Font_ComputeLineLayout(const void* selfv, const char* text,
                                               int32_t count, int32_t startX,
                                               int32_t baseY, int32_t maxWidth,
                                               int32_t line, void* layoutv) {
    if (count == 0) return 0;                                        // 180116c8f

    const uint8_t* self   = reinterpret_cast<const uint8_t*>(selfv);
    uint8_t*       layout = reinterpret_cast<uint8_t*>(layoutv);

    int32_t       penX       = startX;   // EDI
    int32_t       kern       = 0;        // ESI
    const int32_t startSaved = startX;   // [RSP+0x44]
    int32_t       byteIdx    = 0;        // R8D

    if (count > 0) {                                                 // 180116cb8
        const char* const end = text + count;                        // [RSP+0x50]
        for (;;) {
            int32_t cp = 0;
            byteIdx += DecodeUtf8(text + byteIdx, end, &cp);         // 180116ceb

            if (cp >= 0 && cp < *reinterpret_cast<const int32_t*>(self + 0x38)) {
                const int32_t glyph =
                    reinterpret_cast<const int32_t*>(
                        *reinterpret_cast<void* const*>(self + 0x30))[cp];
                if (glyph != 0xffff && glyph != -1 && glyph >= 0) {  // 180116d1e..d35
                    const uint8_t* sheet =
                        *reinterpret_cast<const uint8_t* const*>(self + 0x60);
                    if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                        const uint8_t* sprites =
                            *reinterpret_cast<const uint8_t* const*>(sheet + 0x20);
                        const uint8_t* e =
                            *reinterpret_cast<const uint8_t* const*>(self + 0x40) +
                            static_cast<size_t>(glyph) * 0x10;       // 180116d52
                        const int32_t spriteTop = *reinterpret_cast<const int32_t*>(
                            sprites + static_cast<size_t>(glyph) * 0x38 + 0x20);

                        if (maxWidth > 0) {                          // 180116d6c
                            const int32_t reach =
                                *reinterpret_cast<const int32_t*>(self + 0x58) -
                                startSaved + *reinterpret_cast<const int32_t*>(e) +
                                kern + penX;                         // 180116d6e..d79
                            if (reach >= maxWidth) {                 // 180116d7d
                                // 180116df0 — elide: lay out "..." from here.
                                RtUi_Font_ComputeLineLayout(self, kEllipsisText, 3,
                                                            kern + penX, baseY, -1,
                                                            line, layout);
                                return *reinterpret_cast<const int32_t*>(self + 0x58) -
                                       startSaved + kern + penX;     // 180116e34
                            }
                        }

                        penX += *reinterpret_cast<const int32_t*>(e) + kern;  // 180116d8b

                        if (layout != nullptr) {                     // 180116d8d
                            int32_t entry[4];                        // [RSP+0x58]
                            entry[0] = cp;                           // 180116d99
                            entry[1] = penX;                         // 180116db1
                            entry[2] = baseY -
                                       *reinterpret_cast<const int32_t*>(e + 0xc) -
                                       spriteTop;                    // 180116da0..daf
                            entry[3] = line;                         // 180116dc0

                            using AppendFn = void (*)(void* array, int32_t n, void* value);
                            RtAddr<AppendFn>(0x180076d10ull)(
                                layout + 0x08,
                                *reinterpret_cast<int32_t*>(layout + 0x10) + 1,
                                entry);                              // 180116dc4
                        }

                        penX += *reinterpret_cast<const int32_t*>(e + 8);  // 180116dce
                        kern  = *reinterpret_cast<const int32_t*>(e + 4);  // 180116dd1
                    }
                }
            }

            if (byteIdx >= count) break;                             // 180116dd4
        }
    }

    return penX + (kern - startSaved);                               // 180116e45
}

// ============================================================================
// ?ComputeCharacterPosition@Font@UI@@QEBAHAEBVString@System@@H@Z  0x180116ee0
// ============================================================================
//   int ComputeCharacterPosition(const String& s, int byteCount) const
//
// The pen x after `byteCount` BYTES of `s` — the caret position for an edit
// field. Identical accumulation to ComputeLineLayout's, minus the layout
// emission, the ellipsis and the start offset.
//
// The String's data pointer is read ONCE, before the loop (`MOV R15,[RDX]` at
// 0x180116eea), unlike ComputeStringLayout which re-reads it every iteration.
extern "C" int32_t RtUi_Font_ComputeCharacterPosition(const void* selfv,
                                                      const void* strRefv,
                                                      int32_t count) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);
    const char* const data = *reinterpret_cast<const char* const*>(strRefv);  // 180116eea

    if (count == 0) return 0;                                        // 180116ef8

    int32_t total   = 0;   // EDI
    int32_t kern    = 0;   // R14D
    int32_t byteIdx = 0;   // ESI

    if (count > 0) {                                                 // 180116f18
        const char* const end = data + count;                        // R12
        do {
            int32_t cp = 0;
            byteIdx += DecodeUtf8(data + byteIdx, end, &cp);         // 180116f4b

            if (cp >= 0 && cp < *reinterpret_cast<const int32_t*>(self + 0x38)) {
                const int32_t glyph =
                    reinterpret_cast<const int32_t*>(
                        *reinterpret_cast<void* const*>(self + 0x30))[cp];
                if (glyph != 0xffff && glyph != -1 && glyph >= 0) {  // 180116f6b..f79
                    const uint8_t* sheet =
                        *reinterpret_cast<const uint8_t* const*>(self + 0x60);
                    if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                        const uint8_t* e =
                            *reinterpret_cast<const uint8_t* const*>(self + 0x40) +
                            static_cast<size_t>(glyph) * 0x10;       // 180116f87
                        total += *reinterpret_cast<const int32_t*>(e + 8) +
                                 *reinterpret_cast<const int32_t*>(e) + kern;
                        kern = *reinterpret_cast<const int32_t*>(e + 4);
                    }
                }
            }
        } while (byteIdx < count);                                   // 180116f9d
    }

    return kern + total;                                             // 180116fab
}

// ============================================================================
// ?ComputeStringLayout@Font@UI@@   0x180116fd0
// ============================================================================
//   void ComputeStringLayout(const String& s, bool multiline, int wrapWidth,
//                            int maxWidth, Justify justify,
//                            int& outWidth, int& outHeight,
//                            Layout* layout) const
//
// THE MANGLED NAME'S `3` IS `AEAH`, NOT A THIRD `H`. MSVC's argument
// back-reference table records every encoding longer than one character, and
// `_N` (bool) counts: the table here is 0 = `AEBVString@System@@`, 1 = `_N`,
// 2 = `W4Justify@12@`, 3 = `AEAH`. Reading `3` as anything else would shift
// every later parameter. Confirmed against the bytes: [RSP+0xe0] and
// [RSP+0xe8] are both DEREFERENCED as `int*` (0x180117034, 0x180117037), so
// they are two int references, and the Layout* is the ninth slot at [RSP+0xf0].
//
// NOT MULTILINE is one straight call into ComputeLineLayout with maxWidth in
// the ellipsis slot, and it RETURNS EARLY (0x18011708b) — so it never reaches
// the justify pass and never appends a line width. Only the multiline path
// fills `Layout::_lineWidths`, which is exactly why the justify pass is safe
// to index by `_line`.
//
// THREE STACK SLOTS ARE REUSED AS SCRATCH after their parameter is consumed —
// [RSP+0xc0] (the bool's home) holds the current line's start byte AND, for
// one instruction each time, the width just measured; [RSP+0xe0] and
// [RSP+0xf0] (the two out-parameter homes) hold the pen x and the carried
// kern once R15/RDI have the real pointers. They are separate locals here.
extern "C" void RtUi_Font_ComputeStringLayout(const void* selfv, const void* strRefv,
                                              bool multiline, int32_t wrapWidth,
                                              int32_t maxWidth, int32_t justify,
                                              int32_t* outWidth, int32_t* outHeight,
                                              void* layoutv) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);
    const char* const* strRef = reinterpret_cast<const char* const*>(strRefv);
    uint8_t* layout = reinterpret_cast<uint8_t*>(layoutv);

    if (layout != nullptr) {                                         // 180117003
        using I32SetSizeFn = void (*)(void* array, int32_t n);
        RtAddr<I32SetSizeFn>(0x18002fea0ull)(layout + 0x18, 0);      // 18011700b
        using ClearFn = void (*)(void* array);
        RtAddr<ClearFn>(0x180076a40ull)(layout + 0x08);              // 180117014
        *reinterpret_cast<void**>(layout) = nullptr;                 // 180117021, one qword
    }

    *outWidth  = 0;                                                  // 180117034
    *outHeight = 0;                                                  // 180117037

    if (!multiline) {                                                // 18011703d
        const char* data = *strRef;                                  // 18011703f
        const int32_t len =
            (data != nullptr) ? *reinterpret_cast<const int32_t*>(data - 8) : 0;
        *outWidth = RtUi_Font_ComputeLineLayout(self, data, len, /*startX*/ 0,
                                                /*baseY*/ 0, maxWidth,
                                                /*line*/ 0, layout);  // 18011706e
        *outHeight = *reinterpret_cast<const int32_t*>(self + 0x50);  // 180117076
        return;
    }

    // ---- multiline ------------------------------------------------
    const char* data0 = *strRef;                                     // 18011708c
    const int32_t len =
        (data0 != nullptr) ? *reinterpret_cast<const int32_t*>(data0 - 8) : 0;
    const int32_t lineHeight = *reinterpret_cast<const int32_t*>(self + 0x50);

    int32_t pendingKern = 0;    // [RSP+0xf0]
    int32_t lineStart   = 0;    // [RSP+0xc0]
    int32_t lastSpace   = -1;   // [RSP+0x40]
    int32_t curX        = 0;    // [RSP+0xe0]
    int32_t tabX        = 0;    // EBP
    int32_t y           = 0;    // R14D
    int32_t lineIndex   = 0;    // R13D
    int32_t byteIdx     = 0;    // ESI

    using AppendIntFn = void (*)(void* array, int32_t n, void* value);
    auto appendLineWidth = [&](int32_t w) {
        if (layout == nullptr) return;                               // 180117188
        int32_t v = w;
        RtAddr<AppendIntFn>(0x180023830ull)(
            layout + 0x18, *reinterpret_cast<int32_t*>(layout + 0x20) + 1, &v);
    };

    while (byteIdx < len) {                                          // 1801170e1 / 18011740d
        const char* data = *strRef;                                  // 1801170f0, RE-READ
        int32_t cp = 0;
        const int32_t used = DecodeUtf8(data + byteIdx, data + len, &cp);  // 18011710f

        if (cp < 0) {                                                // 180117120
            byteIdx += used;                                         // 180117122
            continue;
        }

        if (cp == 0x0a) {                                            // 18011712d  '\n'
            const int32_t w = RtUi_Font_ComputeLineLayout(
                self, *strRef + lineStart, byteIdx - lineStart, tabX,
                y, -1, lineIndex, layout);                           // 180117171
            if (w > *outWidth) *outWidth = w;                        // 18011717d
            appendLineWidth(w);                                      // 18011719b

            y += lineHeight;                                         // 1801171a0
            pendingKern = 0;                                         // 1801171ac
            curX        = 0;                                         // 1801171b6
            lineStart   = byteIdx + 1;                               // 1801171bd
            tabX        = 0;                                         // 1801171c5
            ++lineIndex;                                             // 1801171c7
        } else if (cp == 0x09) {                                     // 1801171d3  '\t'
            const int32_t w = RtUi_Font_ComputeLineLayout(
                self, *strRef + lineStart, byteIdx - lineStart, tabX,
                y, -1, lineIndex, layout);                           // 180117217
            if (w > *outWidth) *outWidth = w;                        // 180117223
            appendLineWidth(w);                                      // 180117241

            // The next 32-pixel tab stop. Retail spells the round-down with
            // `CDQ ; AND EDX,0x1f ; LEA EBP,[RDX+RAX] ; AND EBP,~0x1f`, which
            // is round-TOWARD-ZERO for a negative total, not floor.
            const int32_t raw = curX + 0x20 + pendingKern;           // 180117246..18011725f
            lineStart   = byteIdx + 1;                               // 180117257
            pendingKern = 0;                                         // 180117266
            const int32_t bias = (raw < 0) ? 0x1f : 0;               // 18011726d CDQ/AND
            tabX = (bias + raw) & ~0x1f;                             // 180117271

            if (tabX > wrapWidth) {                                  // 180117277
                y += lineHeight;                                     // 180117280
                curX = 0;                                            // 180117288
                ++lineIndex;                                         // 18011728f
                tabX = 0;                                            // 180117292
            } else {
                curX = tabX;                                         // 180117299
            }
        } else {
            if (cp == 0x20) lastSpace = byteIdx;                     // 1801172c2 CMOVZ
        }

        // ---- 1801172c9: the glyph accumulate, reached by ALL THREE arms.
        // A newline or a tab runs it too; they simply have no glyph, so the
        // codepoint table lookup normally falls through. Faithful.
        if (cp < *reinterpret_cast<const int32_t*>(self + 0x38)) {   // 1801172d1
            const int32_t glyph =
                reinterpret_cast<const int32_t*>(
                    *reinterpret_cast<void* const*>(self + 0x30))[cp];
            if (glyph != 0xffff && glyph != -1 && glyph >= 0) {      // 1801172e3..fa
                const uint8_t* sheet =
                    *reinterpret_cast<const uint8_t* const*>(self + 0x60);
                if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                    const uint8_t* e =
                        *reinterpret_cast<const uint8_t* const*>(self + 0x40) +
                        static_cast<size_t>(glyph) * 0x10;           // 180117310
                    const int32_t advance =
                        *reinterpret_cast<const int32_t*>(e + 8) +
                        *reinterpret_cast<const int32_t*>(e) + pendingKern;
                    const int32_t nextKern = *reinterpret_cast<const int32_t*>(e + 4);

                    pendingKern = nextKern;                          // 180117323
                    curX       += advance;                           // 18011732a

                    if (curX + nextKern >= wrapWidth) {              // 180117339
                        // ---- wrap 180117346 ----------------------
                        int32_t w;
                        if (lastSpace >= 0) {                        // 180117379
                            w = RtUi_Font_ComputeLineLayout(
                                self, *strRef + lineStart, lastSpace - lineStart,
                                tabX, y, -1, lineIndex, layout);     // 180117381
                            byteIdx   = lastSpace + 1;               // 180117386
                            lineStart = byteIdx;                     // 18011738b
                        } else {
                            w = RtUi_Font_ComputeLineLayout(
                                self, *strRef + lineStart, byteIdx - lineStart - 1,
                                tabX, y, -1, lineIndex, layout);     // 18011739d
                            lineStart = byteIdx - 1;                 // 1801173a8
                            byteIdx   = byteIdx - 2;                 // 1801173a5
                        }

                        if (w > *outWidth) *outWidth = w;            // 1801173b6
                        appendLineWidth(w);                          // 1801173d4

                        y += lineHeight;                             // 1801173d9
                        lastSpace   = -1;                            // 1801173de
                        pendingKern = 0;                             // 1801173e6
                        ++lineIndex;                                 // 1801173ed
                        curX        = 0;                             // 1801173f0
                        tabX        = 0;                             // 1801173f7
                    }
                }
            }
        }

        byteIdx += used;                                             // 1801173f9
    }

    // ---- the trailing partial line, 180117425 ---------------------
    if (len - lineStart > 0) {                                       // 18011742d
        const int32_t w = RtUi_Font_ComputeLineLayout(
            self, *strRef + lineStart, len - lineStart, /*startX*/ 0,
            y, -1, lineIndex, layout);                               // 180117455
        if (w > *outWidth) *outWidth = w;                            // 180117461
        appendLineWidth(w);                                          // 18011747f
        y += lineHeight;                                             // 180117484
    }

    *outHeight = y;                                                  // 180117496

    if (layout == nullptr) return;                                   // 18011749c

    // ---- justify, 18011749e --------------------------------------
    // The loop bound is RE-READ every iteration (`CMP EBX,[RDI+0x10]`), and
    // `*outWidth` is the widest line, so `d` is this line's slack.
    for (int32_t i = 0; i < *reinterpret_cast<int32_t*>(layout + 0x10); ++i) {
        uint8_t* e = *reinterpret_cast<uint8_t**>(layout + 0x08) +
                     static_cast<size_t>(i) * 0x10;                  // 1801174b0
        const int32_t* widths = *reinterpret_cast<int32_t**>(layout + 0x18);
        const int32_t d = *outWidth - widths[*reinterpret_cast<int32_t*>(e + 0xc)];

        if (justify == 1) {                                          // 1801174c5
            *reinterpret_cast<int32_t*>(e + 4) += d / 2;             // 1801174cb, trunc
        } else if (justify == 2) {                                   // 1801174dc
            *reinterpret_cast<int32_t*>(e + 4) += d;                 // 1801174de
        }
    }

    *reinterpret_cast<int32_t*>(layout + 0x00) = *outWidth;          // 1801174f0
    *reinterpret_cast<int32_t*>(layout + 0x04) = *outHeight;         // 1801174f5
}

// ============================================================================
// Registration. Both doors for every row: the mangled name (which is what the
// minted BaseTypeInfo and the minted vtable ask for first) and the address
// (which is what a ported caller's `ResolveAddr` literal asks for).
// Unconditional — no kill switch, no env gate (DIRECTION.md §2).
//
// The deleting destructor 0x180032130 is deliberately absent: that table is
// owned by runtime/src/object/deleting_dtor*.cpp.
// ============================================================================
int RtUiFontRegister() {
    int n = 0;

    RegisterRuntimeExport("?TypeName@Font@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_Font_TypeName);                        ++n;
    RegisterRuntimeAddr(0x1800319c0ull, (void*)&RtUi_Font_TypeName);

    RegisterRuntimeExport("?Type@Font@UI@@UEBAIXZ",
                          (void*)&RtUi_Font_Type);                            ++n;
    RegisterRuntimeAddr(0x1800319a0ull, (void*)&RtUi_Font_Type);

    RegisterRuntimeExport("?GetFileVersion@Font@UI@@UEBAHXZ",
                          (void*)&RtUi_Font_GetFileVersion);                  ++n;
    RegisterRuntimeAddr(0x1801176b0ull, (void*)&RtUi_Font_GetFileVersion);

    RegisterRuntimeExport(
        "?Compile@Font@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
        (void*)&RtUi_Font_Compile);                                           ++n;
    RegisterRuntimeAddr(0x1801176c0ull, (void*)&RtUi_Font_Compile);

    RegisterRuntimeExport("?Serialize@Font@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_Font_Serialize);                       ++n;
    RegisterRuntimeAddr(0x1801176d0ull, (void*)&RtUi_Font_Serialize);

    RegisterRuntimeExport("?Reconstruct@Font@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_Font_Reconstruct);                     ++n;
    RegisterRuntimeAddr(0x180031c40ull, (void*)&RtUi_Font_Reconstruct);

    RegisterRuntimeExport("?Destruct@Font@UI@@UEAAXXZ",
                          (void*)&RtUi_Font_Destruct);                        ++n;
    RegisterRuntimeAddr(0x180031d80ull, (void*)&RtUi_Font_Destruct);

    RegisterRuntimeExport("?Construct@Font@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_Font_Construct);                       ++n;
    RegisterRuntimeAddr(0x180031ce0ull, (void*)&RtUi_Font_Construct);

    RegisterRuntimeExport("??1Font@UI@@UEAA@XZ",
                          (void*)&RtUi_Font_Dtor);                            ++n;
    RegisterRuntimeAddr(0x1801175a0ull, (void*)&RtUi_Font_Dtor);

    RegisterRuntimeExport(
        "?ComputeStringLayout@Font@UI@@QEBAXAEBVString@System@@_NHHW4Justify@12@AEAH3PEAULayout@12@@Z",
        (void*)&RtUi_Font_ComputeStringLayout);                               ++n;
    RegisterRuntimeAddr(0x180116fd0ull, (void*)&RtUi_Font_ComputeStringLayout);

    RegisterRuntimeExport("?ComputeLineLayout@Font@UI@@QEBAHPEBDHHHHHPEAULayout@12@@Z",
                          (void*)&RtUi_Font_ComputeLineLayout);               ++n;
    RegisterRuntimeAddr(0x180116c70ull, (void*)&RtUi_Font_ComputeLineLayout);

    RegisterRuntimeExport("?ComputeCharacterPosition@Font@UI@@QEBAHAEBVString@System@@H@Z",
                          (void*)&RtUi_Font_ComputeCharacterPosition);        ++n;
    RegisterRuntimeAddr(0x180116ee0ull, (void*)&RtUi_Font_ComputeCharacterPosition);

    return n;
}
