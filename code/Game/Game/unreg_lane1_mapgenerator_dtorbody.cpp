// code/Game/Game/unreg_lane1_mapgenerator_dtorbody.cpp
//
// Unported exe .text wave — LANE 1. Faithful port of FUN_14001aea0,
// MapGenerator::~MapGenerator (the destructor BODY; the scalar-deleting dtor
// 0x14001ae70 is the only caller and is already ported as MapGenerator_Dtor in
// code/Game/Game/mapgenerator.cpp).
//
// WHY THIS WAS A REAL GAP
// -----------------------
// mapgenerator.cpp:75 calls the body through a null-guarded pointer:
//     if (s_fn_DtorBody) s_fn_DtorBody(self);
// 0x14001aea0 was registered nowhere, so ResolveAddr handed back 0 and the
// guard SKIPPED the whole body. That is a null-guard-and-SKIP, so registering
// restores retail exactly. Two consequences of the skip:
//   1. Every MapGenerator teardown leaked three Strings, seven dynamic arrays
//      and the +0x60 buffer.
//   2. Worse than a leak: MapGenerator_Cleanup (0x14001ae30, mapgenerator.cpp:88)
//      is "dtor(self, 0) then MapGenerator_Init(self)" — a reset-in-place. With
//      the body skipped, Init overwrites live String/array headers that were
//      never released, so a Cleanup'd MapGenerator both leaks AND loses the only
//      pointers to the old buffers.
//
// CLOSURE — every callee this body needs is already available:
//   0x140152af0  ??1String@System@@QEAA@XZ            (address_registry.cpp:1279)
//   0x140152b68  ?MemFree@MemoryAllocator@System@@SA…  (address_registry.cpp:1334)
//   0x140151ea0  ??1External@Object@@UEAA@XZ           (address_registry.cpp:1589)
//   0x14001b030  Game_DestroyStringArray18             (reg_game_leaf.cpp:38)
//   0x14001b0a0  Game_DestroyStringArray30             (reg_game_leaf.cpp:39)
// so this registration does not create a half-null closure.
//
// Retail body (0x14001aea0, verified against the disassembly's CALL list —
// three String dtors, 5x b030, 1x b0a0, 1x b030, MemFree, three String dtors,
// then a tail JMP to External::~External):
//
//   ~String(self+0x158); ~String(self+0x138); ~String(self+0x118);
//   b030(self+0xd0); b030(self+0xc0); b030(self+0xb0); b030(self+0xa0);
//   b030(self+0x90); b0a0(self+0x80); b030(self+0x70);
//   MemFree(*(void**)(self+0x60)); *(u64*)(self+0x60)=0; *(u64*)(self+0x68)=0;
//   ~String(self+0x48); ~String(self+0x30); ~String(self+0x28);
//   External::~External(self);
//
// Field order is load-bearing (it is reverse construction order), so it is kept
// literal rather than folded into a loop.

#include "resolve.h"
#include <cstdint>

namespace {

typedef void (*fn_StringDtor_t)(void* str);
typedef void (*fn_MemFreeStatic_t)(void* p);
typedef void (*fn_ExternalDtor_t)(void* self);
typedef void (*fn_ArrayDtor_t)(void* arrayHeader);

fn_StringDtor_t    s_strDtor      = nullptr;   // slot 0x140152af0
fn_MemFreeStatic_t s_memFree      = nullptr;   // slot 0x140152b68
fn_ExternalDtor_t  s_externalDtor = nullptr;   // slot 0x140151ea0
fn_ArrayDtor_t     s_arrayDtor18  = nullptr;   // exe 0x14001b030
fn_ArrayDtor_t     s_arrayDtor30  = nullptr;   // exe 0x14001b0a0
bool s_resolved = false;

// Read a Runtime function pointer out of an exe .rdata dispatch slot.
void* Slot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}

void EnsureFns() {
    if (s_resolved) return;
    s_resolved = true;
    s_strDtor      = reinterpret_cast<fn_StringDtor_t>(Slot(0x140152af0));
    s_memFree      = reinterpret_cast<fn_MemFreeStatic_t>(Slot(0x140152b68));
    s_externalDtor = reinterpret_cast<fn_ExternalDtor_t>(Slot(0x140151ea0));
    s_arrayDtor18  = reinterpret_cast<fn_ArrayDtor_t>(
        ResolveAddr(Module::Exe, 0x14001b030));
    s_arrayDtor30  = reinterpret_cast<fn_ArrayDtor_t>(
        ResolveAddr(Module::Exe, 0x14001b0a0));
}

inline void Str(uint8_t* p) { if (s_strDtor) s_strDtor(p); }
inline void Arr18(uint8_t* p) { if (s_arrayDtor18) s_arrayDtor18(p); }

}  // namespace

// FUN_14001aea0 — MapGenerator::~MapGenerator (body only, no deallocation).
extern "C" void MapGenerator_DtorBody_1aea0(void* selfV) {
    if (!selfV) return;
    EnsureFns();
    uint8_t* s = reinterpret_cast<uint8_t*>(selfV);

    // Three trailing description-file references.
    Str(s + 0x158);
    Str(s + 0x138);
    Str(s + 0x118);

    // Seven resource/biome arrays, high offset first.
    Arr18(s + 0xd0);
    Arr18(s + 0xc0);
    Arr18(s + 0xb0);
    Arr18(s + 0xa0);
    Arr18(s + 0x90);
    if (s_arrayDtor30) s_arrayDtor30(s + 0x80);   // 0x30-byte elements
    Arr18(s + 0x70);

    // The plain bool/int buffer at +0x60 (header is {data, count} — retail zeroes
    // both qwords, so +0x68 goes with it).
    void* buf = *reinterpret_cast<void**>(s + 0x60);
    if (buf && s_memFree) s_memFree(buf);
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x68) = 0;

    // templateRef / _field30 / _field28.
    Str(s + 0x48);
    Str(s + 0x30);
    Str(s + 0x28);

    // Object::External base (retail tail-JMPs here).
    if (s_externalDtor) s_externalDtor(s);
}
