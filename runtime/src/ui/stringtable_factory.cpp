// ============================================================================
// runtime/src/ui/stringtable_factory.cpp — UI::StringTable, the class surface
//
//   ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ      0x180002280 ord 2116
//   ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z
//                                                              0x180002330 ord 1979
//   ??0StringTable@UI@@QEAA@XZ (the class's real ctor, StringTable.h) 0x18011d890
//   ??1StringTable@UI@@UEAA@XZ (the class's real dtor, below)         0x18011d8f0
//   and the nine slots of ??_7StringTable@UI@@6B@ (StringTable.h's banner).
// ============================================================================
//
// ENDGAME B.3.9 block UI-D (B39_RULES D1/D2): UI::StringTable is CONCRETE and
// every live StringTable wears the COMPILER's table. What this file used to be
// — five passes of three waves (26 FACTORY, 27 FRONTIER, 28 MISC) building a
// door for every slot of the minted `??_7StringTable@UI@@6B@` so a cutover boot
// would not trap on it — is retired with that row: the `??0` door and its
// `ConstructBody`, the eight slot doors, slot 0's by-address body
// `RtUiStringTable_ScalarDelete` (0x180002740) and the `??1` door
// (stringtable_lifecycle.cpp). Their attendance counters go with them. The
// factory pair is the class's own statics, registered by retail name and VA.
//
// THE BODIES, READ OFF THE DISASSEMBLY (kept from the Wave 26 transcription):
//
// 0x180002280 ?CreateNewObject@StringTable@UI@@:
//     MOV RBX,[0x1801fc718] ; ADD RBX,0x640     ; allocator singleton + lock
//     MOV RCX,RBX ; CALL [0x18014b390]          ; lock acquire
//     MOV EDX,0x30 ; LEA R8D,[RDX-0x28]         ; size 0x30, align 8
//     MOV RCX,[0x1801fc718] ; CALL 0x1800c6e60  ; ?Allocate@
//     MOV RDI,RAX ; MOV RCX,RBX ; CALL [0x18014b388]   ; lock release
//     TEST RDI,RDI ; JZ -> return 0
//     <the ctor at 0x18011d890, INLINED verbatim>
//     MOV RAX,RDI
//
// That lock / Allocate / unlock triple is instruction-for-instruction the same
// prologue `?CreateNewObject@Dialog@UI@@` (0x180038530) carries, which
// runtime/src/ui/dialog_factory.cpp already transcribes as a call to the
// out-of-line locked wrapper `?MemAlloc@System@@YAPEAX_KH@Z` (0x1800c7a50).
// The two prologues were compared side by side rather than assumed to match
// from resemblance (reference_a_written_recipe_can_be_wrong.md): the ONLY
// differing bytes are the size immediate (0x30 vs 0x88) and the LEA
// displacement that computes align 8 from it (-0x28 vs -0x80). The `LEA
// R8D,[RDX-0x28]` is a code-size trick for `MOV R8D,8`; it is written below as
// the literal 8, because that is the value, and the derivation would imply a
// relationship between size and alignment that does not exist.
//
// 0x180002330 ?CreateInPlaceObject@StringTable@UI@@:
//     TEST RCX,RCX ; JZ -> return 0      <- RETAIL'S null check, not a guard
//     <the same inlined ctor>
//     MOV RAX,RCX
//
// 0x18011d890 ??0StringTable@UI@@: the out-of-line ctor. Its eight stores are
// byte-for-byte the sequence both factories inline; the three copies were
// diffed against each other instruction by instruction, and they agree.
//
// 0x18011d890 ??0StringTable@UI@@: the out-of-line ctor. Its eight stores are
// byte-for-byte the sequence both factories inline; the three copies were
// diffed against each other instruction by instruction, and they agree. The
// three vptr stores are the compiler's now (StringTable.h).
//
// ---------------------------------------------------------------------------
// SLOT 5 - `?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z`, 0x18011d940
// ---------------------------------------------------------------------------
// The SAME `IO::Serializer` 16-byte facade `Object::External::Load` documents at
// length (runtime/src/object/external_load.cpp) - a {Stream*, impl} pair on the
// stack, handed to the Encoding's factory with only the stream filled in. That
// file's account was used to SHAPE this one and every offset below was re-read
// from 0x18011d940's own listing:
//
//     ser.stream = stream                       18011d957  [RSP+0x28]
//     ser.impl   = 0                            18011d95c  [RSP+0x30]
//     enc  = stream->encoding                   18011d965  [RDX+0x10]
//     slot = (stream->direction == 0) ? +0x18 : +0x20      18011d96f/97a/97f
//     ser.impl = enc->vtbl[slot](enc, &ser, 0)
//     count = self->size                        18011d987  [RBX+0x28]
//     ser.impl->vtbl[0xa0](impl, &count, 0)     18011d99e  the uint32 overload
//     if (stream->direction == 0)               18011d9a9
//         Array<TextEntry>::SetSize(&self->data, count)    18011d9b7 -> 0x180078c90
//     e = self->data                            18011d9bc
//     for (i = 0; i < count; ++i, e += 0x10) {
//         String::Serialize(e,       &ser)      18011d9d8 -> 0x1800c9930
//         String::Serialize(e + 0x8, &ser)      18011d9e6
//     }
//     if (ser.impl) ser.impl->vtbl[0](ser.impl, 1)         18011da06
//     return true                               18011da08  MOV AL,0x1
//
// TWO THINGS THAT ARE EASY TO GET WRONG AND WERE READ, NOT ASSUMED:
//   * the loop bound is the LOCAL `count` at [RSP+0x50] (`MOV EAX,[RSP+0x50]`
//     at 18011d9c0), NOT `self->size` re-read. On the read path SetSize has
//     just made them equal; on the WRITE path nothing changes either. It is
//     transcribed as the local because that is what retail reads.
//   * `e` is latched BEFORE the loop (`MOV RBX,[RBX+0x20]` at 18011d9bc), so a
//     reallocation inside the loop would not be seen. Retail's own choice.
//
// RETURN IS UNCONDITIONALLY TRUE. There is no failure path: the only branches
// are the direction test and the count loop. Not simplified - there is nothing
// to simplify; recorded because "a Serialize returning bool" invites a guess.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/ui/StringTable.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_StringPair10

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

constexpr size_t kTiTypeIdOffset = 0x00;
constexpr size_t kTiNameOffset   = 0x08;
static_assert(kTiTypeIdOffset == 0, "BaseTypeInfo::typeId is at +0x00");
static_assert(kTiNameOffset == 8,   "BaseTypeInfo::name is at +0x08");

using SerFactoryFn = void* (*)(void* encoding, void* ser, int flag);
using StrSerFn     = void  (*)(void* str, void* ser);
using VoidPtrFn    = void  (*)(void*);

struct SerializerPair {
    void* stream;   // +0x00
    void* impl;     // +0x08
};
static_assert(sizeof(SerializerPair) == 0x10, "the pair retail keeps on the stack");

void* VSlotOf(const void* obj, size_t byteOffset) {
    void** vtbl = *reinterpret_cast<void** const*>(obj);
    return vtbl[byteOffset / sizeof(void*)];
}

}  // namespace

namespace UI {

// ?GetFileVersion@StringTable@UI@@UEBAHXZ 0x18011d930: `MOV EAX,1`.
int32_t StringTable::GetFileVersion() const { return 1; }

// [8] 0x180002400: the inlined constructor, then `MOV RAX,RCX` (the null test
// sat in the retired door; a virtual call always has a `this`).
Object::External* StringTable::Construct() {
    return ::new (this) StringTable();                       // 180002452 MOV RAX,RCX
}
// [7] 0x180002460: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void StringTable::Destruct() { DeletingDtor(0); }
// [6] 0x180002390: slot0(this, 0) — VIRTUALLY — then the inlined constructor.
Object::External* StringTable::Reconstruct() {
    DeletingDtor(0);                                         // 1800023a7 CALL [RAX], EDX=0
    return ::new (this) StringTable();                       // 1800023ea MOV RAX,RBX
}

// ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ  0x180002280
// The locked MemAlloc of 0x30 bytes, align 8; retail's null branch
// (0x1800022da); then the inlined constructor.
Object::Base* StringTable::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StringTable), 8);
    if (p == nullptr) return nullptr;                        // 1800022da / 18000231d
    return ::new (p) StringTable();                          // 180002318 MOV RAX,RDI
}
// ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z  0x180002330
// THE NULL CHECK IS RETAIL'S (0x180002341 `TEST RCX,RCX / JZ`).
Object::Base* StringTable::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;                    // 180002341 / 180002387
    return ::new (place) StringTable();                      // 180002382 MOV RAX,RCX
}

bool StringTable::Serialize(void* streamV) {
    uint8_t*       stream = static_cast<uint8_t*>(streamV);

    SerializerPair ser;
    ser.stream = stream;                                     // 18011d957
    ser.impl   = nullptr;                                    // 18011d95c

    void*    enc       = *reinterpret_cast<void**>(stream + 0x10);   // 18011d965
    const int32_t dir  = *reinterpret_cast<const int32_t*>(stream + 0x8);
    const size_t  slot = (dir == 0) ? 0x18 : 0x20;                   // 18011d96f
    ser.impl = reinterpret_cast<SerFactoryFn>(VSlotOf(enc, slot))(enc, &ser, 0);

    int32_t count = _size;                                   // 18011d987
    // +0xa0 is SerializeInt (the old typedef here was named for +0x98).
    reinterpret_cast<IO::ReaderWriter*>(ser.impl)->SerializeInt(     // B.3.x bridge
        count, nullptr);                                             // 18011d99e

    // The direction is RE-READ from the stream here, not cached from above:
    // 18011d9a4 reloads [RSP+0x28] and 18011d9a9 tests [RAX+0x8] again.
    if (*reinterpret_cast<const int32_t*>(stream + 0x8) == 0) {      // 18011d9a9
        // ENDGAME B.3.2 step 2 — `_data`/`_size`/`_capacity` are the
        // {data,size,capacity} triple `Collection::Array<TextEntry>` would use,
        // contiguous and asserted at StringTable.h's offsets, and
        // `kSpec_StringPair10` is NON-TRIVIAL, so the call bridges through it.
        rtcoll::AsArray<&rtcoll::kSpec_StringPair10>(&_data)
            ->SetSize(count);                                 // 18011d9b7, retail 0x180078c90
    }

    uint8_t* e = reinterpret_cast<uint8_t*>(_data);          // 18011d9bc
    StrSerFn strSer = reinterpret_cast<StrSerFn>(
        ResolveAddr(Module::Runtime, 0x1800c9930));          // String::Serialize
    for (int32_t i = 0; i < count; ++i, e += 0x10) {         // bound is the LOCAL
        strSer(e,       &ser);                               // 18011d9d8
        strSer(e + 0x8, &ser);                               // 18011d9e6
    }

    if (ser.impl != nullptr) {                               // 18011d9f9
        reinterpret_cast<IO::ReaderWriter*>(ser.impl)->DeletingDtor(1);  // 18011da06  B.3.x bridge
    }
    return true;                                             // 18011da08
}

// ~StringTable: NON-VIRTUAL and EMPTY; `RtUiStringTable_Dtor`
// (stringtable_lifecycle.cpp) is the registered door that does retail's work.
// ??1StringTable@UI@@UEAA@XZ   0x18011d8f0 — retail's body, moved here from
// the retired `RtUiStringTable_Dtor` door (stringtable_lifecycle.cpp):
//     18011d906  re-stamp OUR vptr                 (the compiler's now)
//     18011d910  ADD RCX,0x20 ; CALL 0x180004a90   ~Array<TextEntry>
//     18011d91a  JMP 0x1800bc950                   ~External (the compiler's)
// 0x180004a90 releases both refcounted strings of every entry, frees the block
// and zeroes the header; it is ported (array_destroy.cpp J2, spec kD_Str2_10)
// and reached registry-first. `_data/_size/_capacity` are raw words (the
// NON-TRIVIAL kSpec_StringPair10 row), so it stays a by-address call rather
// than a member ~Array.
StringTable::~StringTable() {
    reinterpret_cast<VoidPtrFn>(ResolveAddr(Module::Runtime, 0x180004a90))(&_data);  // 18011d914
}

}  // namespace UI

// ============================================================================
// Registration — called from RtUiPortInit(), before RtDataMintInit(). The
// factory pair (B39_RULES D2): the class's OWN statics, by name and address,
// so the minted `?_typeInfo@StringTable@UI@@` record (MintTypeInfos resolves
// its factory pointers by name) builds a real StringTable.
// ============================================================================
int RtUiStringTableFactoryRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::StringTable::CreateInPlaceObject);
    RegisterRuntimeAddr(0x180002330, (void*)&UI::StringTable::CreateInPlaceObject);  ++n;

    RegisterRuntimeExport("?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::StringTable::CreateNewObject);
    RegisterRuntimeAddr(0x180002280, (void*)&UI::StringTable::CreateNewObject);      ++n;
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::StringTable's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@StringTable@UI@@ (0x1801fae08): name "StringTable", parent
// "Object::External" (retail ?_ParentTypeName@StringTable@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::StringTable, "Object::External", nullptr)
