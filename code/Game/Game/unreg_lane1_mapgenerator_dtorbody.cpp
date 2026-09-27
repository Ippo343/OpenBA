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

#include "Game/MapGenerator.h"

// FUN_14001aea0 — MapGenerator::~MapGenerator (body only, no deallocation).
// ENDGAME B.3.8 LOOP: the body is ~MapGenerator (mapgenerator.cpp) now; this
// registered door delegates. The slot-0 body with flags 0 is exactly the dtor
// body plus External::~External and no free — what this address ran.
extern "C" void MapGenerator_DtorBody_1aea0(void* selfV) {
    if (!selfV) return;
    static_cast<MapGenerator*>(selfV)->DeletingDtor(0);
}
