// ============================================================================
// runtime/src/ui/font.cpp — UI::Font, the whole class plus its text metrics
// ============================================================================
//
// ??_7Font@UI@@6B@ (retail 0x180155b50), NINE slots — one fewer than the
// ElementDescription family, because UI::Font is a direct Object::External
// subclass and has no `CreateInstance`:
//
//   0  deleting dtor      0x180032130  Font.h (the class's own, B.3.9). Its
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
// and the factory pair `?CreateNewObject@Font@UI@@` (0x180031ab0) /
// `?CreateInPlaceObject@Font@UI@@` (0x180031ba0) — ENDGAME B.3.9 block UI-D
// (B39_RULES D2): the class's own statics, replacing the FACTUI1 row.
// `??3Font@UI@@SAXPEAX@Z` (0x180031a50), the class operator delete, is ported
// in runtime/src/ui/ui_operator_delete.cpp.
//
// Layout: runtime/types/ui/Font.h (sizeof 0x90).
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED AND THERE IS NO OUT-OF-LINE ??0Font@UI@@.
// ----------------------------------------------------------------------------
// `Construct` (0x180031ce0), `Reconstruct` (0x180031c40) and both already-
// ported factory steps each open-code the same 21 stores; the DLL exports no
// `??0Font@UI@@`. ENDGAME B.3.9 block UI-D: that sequence (0x180031cf6..
// 0x180031d67) is the class's REAL constructor, in Font.h, and every live Font
// wears the COMPILER's table. Two details worth keeping visible:
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
#include "../../types/system/String.h"
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

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
// ENDGAME B.3.6 block D: every body below is a UI::Font member; the doors
// that keep the registered names/addresses follow the members and delegate
// with a QUALIFIED call (a live Font wears the MINTED table).
namespace UI {

// 0x1800319c0: `MOV RAX,[0x1801fab70] ; RET` — the same node at +8.
// 0x1801176b0: `MOV EAX,0x8 ; RET`. Eight — a plain integer, unlike the
// *Description family's packed 0x104 / 0x204 / 0x10204.
int32_t Font::GetFileVersion() const { return 8; }
// 0x1801176c0: `MOV EAX,0x1 ; RET`. The Filename& in RDX is never read. See
// the banner: W4CompileResult comes back in EAX, there is no hidden slot.
Object::External::CompileResult Font::Compile(const System::Filename&) { return 1; }

// ============================================================================
// Construct / Reconstruct / Destruct
// ============================================================================
// 0x180031ce0: `TEST RCX,RCX ; JZ ret0`, then the inlined constructor, then
// `MOV RAX,RCX`. (The null test sat in the retired door; a virtual call always
// has a `this`.)
Object::External* Font::Construct() {
    return ::new (this) Font();                                      // 180031cf6..180031d67
}
// 0x180031c40: slot0(this, 0) — the deleting destructor with the free bit
// CLEAR, i.e. destroy in place, VIRTUALLY — and then the same inlined
// constructor. NOT a tail jump: there is no out-of-line ctor to jump to.
Object::External* Font::Reconstruct() {
    DeletingDtor(0);                                                 // 180031c57
    return ::new (this) Font();                                      // 180031c5e..180031ccf
}
// 0x180031d80: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void Font::Destruct() { DeletingDtor(0); }

// ?CreateNewObject@Font@UI@@SAPEAVBase@Object@@XZ  0x180031ab0
// The FACTUI1 row this replaces: the locked MemAlloc of 0x90 bytes, align 8;
// retail's null branch; then the inlined constructor.
Object::Base* Font::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Font), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) Font();
}
// ?CreateInPlaceObject@Font@UI@@SAPEAVBase@Object@@PEAX@Z  0x180031ba0
Object::Base* Font::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) Font();
}

// ============================================================================
// ??1Font@UI@@UEAA@XZ   0x1801175a0 — retail's body, moved here from the
// retired `RtUi_Font_Dtor` door (ENDGAME B.3.9 UI-D). Retail's sequence:
//     release String +0x88, +0x80, +0x78                 ; 1801175c5..18011764c
//     Array<FontGlyph>::~Array(this + 0x40)   0x180033c50 ; 180117654
//     Array<int32>::~Array   (this + 0x30)    0x18001c750 ; 18011765e
//     release String +0x20                                ; 180117664
//     JMP ~External                           0x1800bc950 ; 180117697
// Here the two ~Array are the MEMBERS' own destructors (Collection::Array:
// free `_data`, `_capacity` left alone — the same body both retail copies
// compile to), which the compiler runs AFTER this body, then ~External. So the
// one visible difference is ORDER: the +0x20 release now precedes the two
// array frees instead of following them. The four releases and two frees are
// of independent blocks; nothing reads between them.
Font::~Font() {
    System::StringPrivateAccess::Release(*reinterpret_cast<System::String*>(&_string88));   // 1801175c5
    System::StringPrivateAccess::Release(*reinterpret_cast<System::String*>(&_string80));   // 1801175f8
    System::StringPrivateAccess::Release(*reinterpret_cast<System::String*>(&_string78));   // 18011762a
    System::StringPrivateAccess::Release(*reinterpret_cast<System::String*>(&_string20));   // 180117664
}

}  // namespace UI

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
namespace UI {

// The body reads its own fields at the file format's offsets (+0x20, +0x50,
// +0x80 ...) through `p`, in FORMAT order, which is not declaration order;
// every offset is asserted against a named member in Font.h.
bool Font::Serialize(void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(this);
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

}  // namespace UI

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
namespace UI {

int32_t Font::ComputeLineLayout(const char* text, int32_t count, int32_t startX,
                                int32_t baseY, int32_t maxWidth, int32_t line,
                                Layout* layoutArg) const {
    if (count == 0) return 0;                                        // 180116c8f

    // The Layout is written through its raw offsets (+0x08 entries, +0x10 its
    // size), as retail's append does; RtUI::FontLayout asserts both.
    uint8_t*       layout = reinterpret_cast<uint8_t*>(layoutArg);

    int32_t       penX       = startX;   // EDI
    int32_t       kern       = 0;        // ESI
    const int32_t startSaved = startX;   // [RSP+0x44]
    int32_t       byteIdx    = 0;        // R8D

    if (count > 0) {                                                 // 180116cb8
        const char* const end = text + count;                        // [RSP+0x50]
        for (;;) {
            int32_t cp = 0;
            byteIdx += DecodeUtf8(text + byteIdx, end, &cp);         // 180116ceb

            if (cp >= 0 && cp < _charToGlyph._size) {
                const int32_t glyph =
                    reinterpret_cast<const int32_t*>(
                        _charToGlyph._data)[cp];
                if (glyph != 0xffff && glyph != -1 && glyph >= 0) {  // 180116d1e..d35
                    const uint8_t* sheet =
                        reinterpret_cast<const uint8_t*>(_spriteSheet.Get());
                    if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                        const uint8_t* sprites =
                            *reinterpret_cast<const uint8_t* const*>(sheet + 0x20);
                        const uint8_t* e =
                            reinterpret_cast<const uint8_t*>(_glyphs._data) +
                            static_cast<size_t>(glyph) * 0x10;       // 180116d52
                        const int32_t spriteTop = *reinterpret_cast<const int32_t*>(
                            sprites + static_cast<size_t>(glyph) * 0x38 + 0x20);

                        if (maxWidth > 0) {                          // 180116d6c
                            const int32_t reach =
                                _ellipsisWidth -
                                startSaved + *reinterpret_cast<const int32_t*>(e) +
                                kern + penX;                         // 180116d6e..d79
                            if (reach >= maxWidth) {                 // 180116d7d
                                // 180116df0 — elide: lay out "..." from here.
                                ComputeLineLayout(kEllipsisText, 3,
                                                            kern + penX, baseY, -1,
                                                            line, reinterpret_cast<Layout*>(layout));
                                return _ellipsisWidth -
                                       startSaved + kern + penX;     // 180116e34
                            }
                        }

                        penX += *reinterpret_cast<const int32_t*>(e) + kern;  // 180116d8b

                        if (layout != nullptr) {                     // 180116d8d
                            int32_t entry[4];                        // [RSP+0x58]
                            // GLYPH index, not the codepoint: EDX still holds
                            // the MOVSXD from the codepoint table (0x180116d1a)
                            // and nothing rewrites it before this store. This
                            // is what SpriteContext::AddFont draws by, and the
                            // sheet has only as many sprites as there are
                            // glyphs (storing `cp` here drew "n" for "New").
                            entry[0] = glyph;                        // 180116d99
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

}  // namespace UI

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
namespace UI {

int32_t Font::ComputeCharacterPosition(const System::String& text, int32_t count) const {
    const char* const data = text._data;                              // 180116eea

    if (count == 0) return 0;                                        // 180116ef8

    int32_t total   = 0;   // EDI
    int32_t kern    = 0;   // R14D
    int32_t byteIdx = 0;   // ESI

    if (count > 0) {                                                 // 180116f18
        const char* const end = data + count;                        // R12
        do {
            int32_t cp = 0;
            byteIdx += DecodeUtf8(data + byteIdx, end, &cp);         // 180116f4b

            if (cp >= 0 && cp < _charToGlyph._size) {
                const int32_t glyph =
                    reinterpret_cast<const int32_t*>(
                        _charToGlyph._data)[cp];
                if (glyph != 0xffff && glyph != -1 && glyph >= 0) {  // 180116f6b..f79
                    const uint8_t* sheet =
                        reinterpret_cast<const uint8_t*>(_spriteSheet.Get());
                    if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                        const uint8_t* e =
                            reinterpret_cast<const uint8_t*>(_glyphs._data) +
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

}  // namespace UI

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
namespace UI {

void Font::ComputeStringLayout(const System::String& text, bool multiline,
                               int32_t wrapWidth, int32_t maxWidth, Justify justify,
                               int32_t& outWidthRef, int32_t& outHeightRef,
                               Layout* layoutArg) const {
    // Retail keeps the String's data pointer's HOME and re-reads it every
    // iteration (0x1801170f0); `strRef` is that home. The two out-references
    // and the Layout are written through pointers exactly as retail does.
    const char* const* strRef = &text._data;
    int32_t* outWidth  = &outWidthRef;
    int32_t* outHeight = &outHeightRef;
    uint8_t* layout = reinterpret_cast<uint8_t*>(layoutArg);

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
        *outWidth = ComputeLineLayout(data, len, /*startX*/ 0,
                                                /*baseY*/ 0, maxWidth,
                                                /*line*/ 0, reinterpret_cast<Layout*>(layout));  // 18011706e
        *outHeight = _lineHeight;  // 180117076
        return;
    }

    // ---- multiline ------------------------------------------------
    const char* data0 = *strRef;                                     // 18011708c
    const int32_t len =
        (data0 != nullptr) ? *reinterpret_cast<const int32_t*>(data0 - 8) : 0;
    const int32_t lineHeight = _lineHeight;

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
            const int32_t w = ComputeLineLayout(*strRef + lineStart, byteIdx - lineStart, tabX,
                y, -1, lineIndex, reinterpret_cast<Layout*>(layout));                           // 180117171
            if (w > *outWidth) *outWidth = w;                        // 18011717d
            appendLineWidth(w);                                      // 18011719b

            y += lineHeight;                                         // 1801171a0
            pendingKern = 0;                                         // 1801171ac
            curX        = 0;                                         // 1801171b6
            lineStart   = byteIdx + 1;                               // 1801171bd
            tabX        = 0;                                         // 1801171c5
            ++lineIndex;                                             // 1801171c7
        } else if (cp == 0x09) {                                     // 1801171d3  '\t'
            const int32_t w = ComputeLineLayout(*strRef + lineStart, byteIdx - lineStart, tabX,
                y, -1, lineIndex, reinterpret_cast<Layout*>(layout));                           // 180117217
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
        if (cp < _charToGlyph._size) {   // 1801172d1
            const int32_t glyph =
                reinterpret_cast<const int32_t*>(
                    _charToGlyph._data)[cp];
            if (glyph != 0xffff && glyph != -1 && glyph >= 0) {      // 1801172e3..fa
                const uint8_t* sheet =
                    reinterpret_cast<const uint8_t*>(_spriteSheet.Get());
                if (glyph < *reinterpret_cast<const int32_t*>(sheet + 0x28)) {
                    const uint8_t* e =
                        reinterpret_cast<const uint8_t*>(_glyphs._data) +
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
                            w = ComputeLineLayout(*strRef + lineStart, lastSpace - lineStart,
                                tabX, y, -1, lineIndex, reinterpret_cast<Layout*>(layout));     // 180117381
                            byteIdx   = lastSpace + 1;               // 180117386
                            lineStart = byteIdx;                     // 18011738b
                        } else {
                            w = ComputeLineLayout(*strRef + lineStart, byteIdx - lineStart - 1,
                                tabX, y, -1, lineIndex, reinterpret_cast<Layout*>(layout));     // 18011739d
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
        const int32_t w = ComputeLineLayout(*strRef + lineStart, len - lineStart, /*startX*/ 0,
            y, -1, lineIndex, reinterpret_cast<Layout*>(layout));                               // 180117455
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

}  // namespace UI

// ============================================================================
// THE DOORS. ENDGAME B.3.9 UI-D: the eight slot doors and the ??1 door are
// gone -- their only reader was the minted `??_7Font@UI@@6B@` (and, for ??1,
// its RT_DELDTOR row). The three metric exports are NOT slots and keep their
// doors (reached by name/address); each delegates with a QUALIFIED
// non-virtual call. None of the three is a retslot (the banner).
// ============================================================================
extern "C" int32_t RtUi_Font_ComputeLineLayout(const void* selfv, const char* text,
                                               int32_t count, int32_t startX,
                                               int32_t baseY, int32_t maxWidth,
                                               int32_t line, void* layoutv) {
    return reinterpret_cast<const UI::Font*>(selfv)                                  // B.3.6 bridge
        ->UI::Font::ComputeLineLayout(text, count, startX, baseY, maxWidth, line,
                                      static_cast<UI::Font::Layout*>(layoutv));
}
extern "C" int32_t RtUi_Font_ComputeCharacterPosition(const void* selfv,
                                                      const void* strRefv,
                                                      int32_t count) {
    return reinterpret_cast<const UI::Font*>(selfv)                                  // B.3.6 bridge
        ->UI::Font::ComputeCharacterPosition(*static_cast<const System::String*>(strRefv), count);
}
extern "C" void RtUi_Font_ComputeStringLayout(const void* selfv, const void* strRefv,
                                              bool multiline, int32_t wrapWidth,
                                              int32_t maxWidth, int32_t justify,
                                              int32_t* outWidth, int32_t* outHeight,
                                              void* layoutv) {
    reinterpret_cast<const UI::Font*>(selfv)                                         // B.3.6 bridge
        ->UI::Font::ComputeStringLayout(*static_cast<const System::String*>(strRefv),
                                        multiline, wrapWidth, maxWidth, justify,
                                        *outWidth, *outHeight,
                                        static_cast<UI::Font::Layout*>(layoutv));
}

int RtUiFontRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@Font@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::Font::CreateNewObject);                 ++n;
    RegisterRuntimeAddr(0x180031ab0ull, (void*)&UI::Font::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@Font@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::Font::CreateInPlaceObject);             ++n;
    RegisterRuntimeAddr(0x180031ba0ull, (void*)&UI::Font::CreateInPlaceObject);

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

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::Font's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@Font@UI@@ (0x1801fab68): name "Font", parent
// "Object::External" (retail ?_ParentTypeName@Font@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::Font, "Object::External", nullptr)
