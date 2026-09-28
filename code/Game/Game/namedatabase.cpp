// code/Game/Game/namedatabase.cpp
// Clean reimplementation of the NameDatabase resource type.
//
// NameDatabase (0x60 bytes) — the ".ndb" name-generation resource
// (Game/Names/Cities.ndb, Female.ndb, etc). Extends Object::External (0x20 base).
// Registered by MenuScene_Setup (exe 0x14014cc40) with factory
// MenuScene_Setup_sub_c660 / cleanup MenuScene_Setup_sub_c6d0; vtable @ 0x14015eea0.
//
// It is loaded as part of the menu/scene resource closure during
// DialogLoaderLoopT::Load. In standalone it had only a NullFactory stub, so
// ExternalFactory::FindOrLoad got NULL back and crashed in IsDerivedFrom(NULL)
// (Runtime+0xC039A). This file provides the real factory + faithful Serialize.
//
// Layout (from factory 0xc660, dtor 0x1400a4790, Serialize 0x1400a45e0):
//   +0x00 vtable (0x14015eea0)
//   +0x08..+0x1F  Object::External base
//   +0x20 array1.data   (stride 0xc = 3 ints/elem)   +0x28 count  +0x2c cap
//   +0x30 array2.data   (stride 0x2 = uint16)        +0x38 count  +0x3c cap
//   +0x40 array3.data   (stride 0x1 = byte buffer)   +0x48 count  +0x4c cap
//   +0x50 array4.data   (stride 0x4 = int)           +0x58 count  +0x5c cap
//
// Serialize (vtable +0x28, exe 0x1400a45e0) — per array: serialize the count int;
// if loading (serializer stream mode == 0) resize the array; then serialize the
// elements. array1 is a per-element loop (SerializeBytes(4)+Serialize(byte)+
// Serialize(uint)); arrays 2/3/4 are bulk serializes. GetVersion (vtable +0x18,
// exe 0x1400a4780) = 0x401 (verified: `mov eax,0x401; ret`).
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/NameDatabase.h, field
// layout and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_NDB_Vtable`, its stubs, the lazily-resolved resize
// pointers (a non-atomic first-call cache, B6-5) and the C entry points.

#include "Game/NameDatabase.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstdio>
#include <new>

// Array resize helpers — the bodies registered at the four exe addresses
// (address_registry.cpp), called directly. The old lazily-resolved pointers
// (a non-atomic first-call cache, B6-5) resolved to exactly these.
extern "C" void Reimpl_Array_Resize_Stride0C(void* arrayHeader, int newCount);  // 0x14001c7c0
extern "C" void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);  // 0x14001c890
extern "C" void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount);      // 0x14000a590
extern "C" void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);  // 0x1400021d0

// ============================================================
// NameDatabase::NameDatabase — factory sub_c660 body
// ============================================================
// External() and the 0x14015eea0 stamp are the compiler's; the four
// {data,count,cap} array headers at +0x20..+0x5F are zeroed.
NameDatabase::NameDatabase() {
    _array1 = nullptr; _array1Count = 0; _array1Capacity = 0;
    _array2 = nullptr; _array2Count = 0; _array2Capacity = 0;
    _array3 = nullptr; _array3Count = 0; _array3Capacity = 0;
    _array4 = nullptr; _array4Count = 0; _array4Capacity = 0;
}

// ============================================================
// NameDatabase::Serialize (vtable +0x28) — faithful port of 0x1400a45e0
// ============================================================
bool NameDatabase::Serialize(void* stream) {
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Loading when the serializer's stream mode field (stream+8) == 0.
    auto isLoading = [&]() -> bool {
        void* s0 = *(void**)ser;
        return s0 && static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    };

    // --- array1: stride 0xc (3 fields/elem) ---
    int c1 = _array1Count;
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c1, nullptr);   // B.3.4 bridge
    if (isLoading()) Reimpl_Array_Resize_Stride0C(&_array1, c1);
    if (c1 > 0) {
        uint8_t* p = reinterpret_cast<uint8_t*>(_array1);   // element0
        for (int i = 0; i < c1; ++i) {
            // field0: 4 raw bytes
            reinterpret_cast<IO::Serializer*>(ser)->SerializeBytes(4, p + 0x0, nullptr);   // B.3.4 bridge
            // field1: uchar
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned char*>(p + 0x4), nullptr);   // B.3.4 bridge
            // field2: uint
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(p + 0x8), nullptr);   // B.3.4 bridge
            p += 0xc;
        }
    }

    // --- array2: stride 0x2 (uint16) — bulk ---
    int c2 = _array2Count;
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c2, nullptr);   // B.3.4 bridge
    if (isLoading()) Reimpl_Array_Resize_Stride02(&_array2, c2);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c2, reinterpret_cast<unsigned short*>(_array2), nullptr);   // B.3.4 bridge

    // --- array3: stride 0x1 (byte buffer) — bulk ---
    int c3 = _array3Count;
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c3, nullptr);   // B.3.4 bridge
    if (isLoading()) Reimpl_Array_Resize_Byte(&_array3, c3);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c3, reinterpret_cast<unsigned char*>(_array3), nullptr);   // B.3.4 bridge

    // --- array4: stride 0x4 (int) — bulk ---
    int c4 = _array4Count;
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c4, nullptr);   // B.3.4 bridge
    if (isLoading()) Reimpl_Array_Resize_Stride04(&_array4, c4);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c4, reinterpret_cast<int*>(_array4), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// NameDatabase::~NameDatabase / DeletingDtor (vtable +0x00)
// ============================================================
// Mirrors 0x14001c790 -> FUN_1400a4790.
// ENDGAME B.3.2 step 2: four trivial-element `Collection::Array::~Array`s,
// back to front (+0x50 int32, +0x40 byte, +0x30, +0x20). Each was
// MemFree(data); data = 0; one qword over size+capacity = 0 —
// `rtcoll::ArrayDtorImpl` exactly, with no element step. The header alias is
// used for all four. External::~External is the compiler's chain.
NameDatabase::~NameDatabase() {
    reinterpret_cast<rtcoll::ArrayHdr*>(&_array4)->~Array();   // B.3.2 bridge
    reinterpret_cast<rtcoll::ArrayHdr*>(&_array3)->~Array();
    reinterpret_cast<rtcoll::ArrayHdr*>(&_array2)->~Array();
    reinterpret_cast<rtcoll::ArrayHdr*>(&_array1)->~Array();
}

// NameDatabase::Reconstruct (vtable +0x30, 0x14001c730): dtor(self,0) then re-init.
Object::External* NameDatabase::Reconstruct() {
    this->~NameDatabase();
    return ::new (static_cast<void*>(this)) NameDatabase();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void NameDatabase::Destruct() { this->~NameDatabase(); }

// [8] 0x14001c6d0 — placement-new of the class on `this`.
Object::External* NameDatabase::Construct() {
    return ::new (static_cast<void*>(this)) NameDatabase();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* NameDatabase::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NameDatabase), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) NameDatabase());
}
void* NameDatabase::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) NameDatabase());
}


DEFINE_EXE_TYPE_EXT(NameDatabase, "Object::External", "ndb")
