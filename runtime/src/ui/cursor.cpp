// ============================================================================
// runtime/src/ui/cursor.cpp — UI::Cursor, the whole class
// ============================================================================
//
// ??_7Cursor@UI@@6B@ (retail 0x180156570), nine slots:
//   0  deleting dtor      0x18003b3a0  deleting_dtor.cpp row (~Class 0x1800cd1c0,
//                                      sizeof 0x30 — both read off its own bytes)
//   1  TypeName           0x18003af80  here
//   2  Type               0x18003af60  here
//   3  GetFileVersion     0x1800cd1b0  here  (returns 1)
//   4  External::Compile  0x1800066e0  already ported (the base's slot)
//   5  Serialize          0x1800cd110  here
//   6  Reconstruct        0x18003b180  here
//   7  Destruct           0x18003b240  here
//   8  Construct          0x18003b1e0  here
// plus ??1Cursor@UI@@UEAA@XZ 0x1800cd1c0, here.
//
// LAYOUT, sizeof 0x30 (`MOV EDX,0x30` in the deleting destructor's array arm at
// 0x18003b3d2, and the `Allocate(0x30, 8)` in the already-ported factory row):
//     +0x00  vtable        Object::Base -> Object::External -> UI::Cursor
//     +0x08 .. +0x1f       Object::External's three words
//     +0x20  int32         serialized, NOT initialised by the constructor
//     +0x24  int32         serialized, NOT initialised by the constructor
//     +0x28  ExternalPtr<UI::SpriteSheet>   (serialized by 0x180004590)
//
// TWO FIELDS THE CONSTRUCTOR DOES NOT WRITE. +0x20 and +0x24 are read and
// written by Serialize, and the constructor (0x18003b1f6..0x18003b22a, seven
// stores, the last of them `[RCX+0x28] = 0`) never touches them: a freshly
// constructed Cursor carries whatever the allocator left there until something
// deserialises into it. That is retail's behaviour, not a transcription slip —
// the stores were counted against the already-ported `kCursor` table in
// runtime/src/ui/factory_ui1_tables.inc, which has the same seven and no more.
//
// ~Cursor DOES NOT RELEASE +0x28. Its whole body is a vtable restamp and a tail
// jump to ??1External@Object@@ (0x1800cd1d2..0x1800cd1e0, four instructions);
// the ExternalPtr at +0x28 is left alone. Compare ~CharacterSet next door,
// which does release its one member. Faithful, and stated here because "the
// destructor must release the owning pointer" is exactly the assumption that
// would otherwise get a helpful line added.
//
// THE CONSTRUCTOR IS NOT IN THIS FILE, for the same reason as in
// runtime/src/ui/characterset.cpp: there is no out-of-line `??0Cursor@UI@@` at
// all — it is inlined into Construct and Reconstruct, and the same stores are
// already ported table-driven by runtime/src/ui/factory_ui1.cpp.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/Cursor.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtCursor()   { return RT_DATA_SLOT(void*, "??_7Cursor@UI@@6B@"); }

const uint8_t* CursorTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Cursor@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

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

// The seven stores retail inlines into Construct (0x18003b1f6..0x18003b22a)
// and, identically, into Reconstruct (0x18003b19e..0x18003b1d2).
void InlineCtorStores(uint8_t* p) {
    *reinterpret_cast<void**>(p)        = VtBase();       // 18003b1f6
    *reinterpret_cast<void**>(p)        = VtExternal();   // 18003b200
    *reinterpret_cast<void**>(p + 0x08) = nullptr;        // 18003b215
    *reinterpret_cast<void**>(p + 0x10) = nullptr;        // 18003b218
    *reinterpret_cast<void**>(p + 0x18) = nullptr;        // 18003b21c
    *reinterpret_cast<void**>(p)        = VtCursor();     // 18003b220
    *reinterpret_cast<void**>(p + 0x28) = nullptr;        // 18003b22a
}

}  // namespace

// ############################################################################
// UI::Cursor's member bodies (ENDGAME B.3.6 block D) — after the anonymous
// namespace, inside `namespace UI` (B34_BRIEFS A.2).
// ############################################################################
namespace UI {

// [2] 0x18003af60: `MOV EAX,[0x1801f9e10] ; RET`.
uint32_t Cursor::Type() const {
    const uint8_t* ti = CursorTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// [1] 0x18003af80: `MOV RAX,[0x1801f9e18] ; RET` — the same node at +8.
const char* Cursor::TypeName() const {
    const uint8_t* ti = CursorTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// [3] 0x1800cd1b0: `MOV EAX,0x1 ; RET`.
int32_t Cursor::GetFileVersion() const { return 1; }

// [8] 0x18003b1e0 — retail's null test (18003b1f1 TEST/JZ) is in the DOOR.
Object::External* Cursor::Construct() {
    InlineCtorStores(reinterpret_cast<uint8_t*>(this));
    return this;                                          // 18003b22e
}
// [6] 0x18003b180: slot0(this, 0), then the same inlined constructor, open-coded.
Object::External* Cursor::Reconstruct() {
    CallSlot0(this, 0);                                   // 18003b197
    InlineCtorStores(reinterpret_cast<uint8_t*>(this));
    return this;                                          // 18003b1d6
}
// [7] 0x18003b240: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void Cursor::Destruct() { CallSlot0(this, 0); }

// ============================================================================
// [5] ?Serialize@Cursor@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cd110
// ============================================================================
//   rw = encoding factory, read (+0x18) or write (+0x20) side by stream->mode
//   ExternalPtr<UI::SpriteSheet>::Serialize(&this->_spriteSheet, &ser)  0x180004590
//   rw->vt[0xa0](rw, &this->_hotSpotX, 0)
//   rw->vt[0xa0](rw, &this->_hotSpotY, 0)
//   if (rw) rw->vt[0](rw, 1)
//   return true
//
// THE SHEET COMES FIRST and the two ints after it — the reverse of the order
// the fields sit in. Sequence, not layout, is what a stream records, and this
// is the one place the difference is visible.
bool Cursor::Serialize(void* streamv) {
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                 // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;             // 1800cd130
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;  // 1800cd13a CMP / JNZ
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                // 1800cd14d

    using PtrSerFn = void (*)(void* extPtr, Serializer* ser);
    RtAddr<PtrSerFn>(0x180004590ull)(&_spriteSheet, &ser);  // 1800cd15b

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_hotSpotX, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_hotSpotY, nullptr);

    if (rw != nullptr) {                                  // 1800cd190
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;                                          // 1800cd19f MOV AL,1
}

// ~Cursor: NON-VIRTUAL and EMPTY; the registered dtor door does retail's work.
Cursor::~Cursor() {}

}  // namespace UI

// ============================================================================
// THE DOORS — qualified delegation (a live Cursor wears the MINTED table).
// ============================================================================
extern "C" uint32_t RtUi_Cursor_Type(const void* selfv) {
    return reinterpret_cast<const UI::Cursor*>(selfv)->UI::Cursor::Type();          // B.3.6 bridge
}
extern "C" const char* RtUi_Cursor_TypeName(const void* selfv) {
    return reinterpret_cast<const UI::Cursor*>(selfv)->UI::Cursor::TypeName();      // B.3.6 bridge
}
extern "C" int32_t RtUi_Cursor_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const UI::Cursor*>(selfv)->UI::Cursor::GetFileVersion(); // B.3.6 bridge
}
extern "C" void* RtUi_Cursor_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                 // 18003b1f1 TEST/JZ
    return reinterpret_cast<UI::Cursor*>(selfv)->UI::Cursor::Construct();           // B.3.6 bridge
}
extern "C" void* RtUi_Cursor_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::Cursor*>(selfv)->UI::Cursor::Reconstruct();         // B.3.6 bridge
}
extern "C" void RtUi_Cursor_Destruct(void* selfv) {
    reinterpret_cast<UI::Cursor*>(selfv)->UI::Cursor::Destruct();                   // B.3.6 bridge
}
extern "C" bool RtUi_Cursor_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::Cursor*>(selfv)->UI::Cursor::Serialize(streamv);    // B.3.6 bridge
}

// ============================================================================
// ??1Cursor@UI@@UEAA@XZ   0x1800cd1c0 — restamp, then tail-jump the base. The
// dtor DOOR keeps its body; the member destructor is empty (Cursor.h).
// ============================================================================
extern "C" void RtUi_Cursor_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    *reinterpret_cast<void**>(p) = VtCursor();            // 1800cd1d9

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cd1e0 JMP
}
int RtUiCursorRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@Cursor@UI@@UEBAIXZ", (void*)&RtUi_Cursor_Type);   ++n;
    RegisterRuntimeAddr(0x18003af60ull, (void*)&RtUi_Cursor_Type);

    RegisterRuntimeExport("?TypeName@Cursor@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_Cursor_TypeName);                          ++n;
    RegisterRuntimeAddr(0x18003af80ull, (void*)&RtUi_Cursor_TypeName);

    RegisterRuntimeExport("?GetFileVersion@Cursor@UI@@UEBAHXZ",
                          (void*)&RtUi_Cursor_GetFileVersion);                     ++n;
    RegisterRuntimeAddr(0x1800cd1b0ull, (void*)&RtUi_Cursor_GetFileVersion);

    RegisterRuntimeExport("?Construct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_Cursor_Construct);                          ++n;
    RegisterRuntimeAddr(0x18003b1e0ull, (void*)&RtUi_Cursor_Construct);

    RegisterRuntimeExport("?Reconstruct@Cursor@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_Cursor_Reconstruct);                        ++n;
    RegisterRuntimeAddr(0x18003b180ull, (void*)&RtUi_Cursor_Reconstruct);

    RegisterRuntimeExport("?Destruct@Cursor@UI@@UEAAXXZ",
                          (void*)&RtUi_Cursor_Destruct);                           ++n;
    RegisterRuntimeAddr(0x18003b240ull, (void*)&RtUi_Cursor_Destruct);

    RegisterRuntimeExport("??1Cursor@UI@@UEAA@XZ", (void*)&RtUi_Cursor_Dtor);      ++n;
    RegisterRuntimeAddr(0x1800cd1c0ull, (void*)&RtUi_Cursor_Dtor);

    RegisterRuntimeExport("?Serialize@Cursor@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_Cursor_Serialize);                          ++n;
    RegisterRuntimeAddr(0x1800cd110ull, (void*)&RtUi_Cursor_Serialize);

    return n;
}
