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

#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include <new>

static constexpr uintptr_t kVtableAddr = 0x14015eea0;
static constexpr int       kSize       = 0x60;

// Array resize helpers (exe addresses; we register our reimpls for these).
static constexpr uintptr_t kResizeArr1 = 0x14001c7c0;  // stride 0xc
static constexpr uintptr_t kResizeArr2 = 0x14001c890;  // stride 0x2
static constexpr uintptr_t kResizeArr3 = 0x14000a590;  // stride 0x1 (byte)
static constexpr uintptr_t kResizeArr4 = 0x1400021d0;  // stride 0x4

typedef void  (*Resize_t)(void* arrayHeader, int newCount);

static Resize_t         s_resize1    = nullptr;
static Resize_t         s_resize2    = nullptr;
static Resize_t         s_resize3    = nullptr;
static Resize_t         s_resize4    = nullptr;
static bool             s_resolved   = false;

static void NameDatabase_ResolveFuncs() {
    if (s_resolved) return;
    s_resolved = true;

    s_resize1 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr1));
    s_resize2 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr2));
    s_resize3 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr3));
    s_resize4 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr4));
}

// ============================================================
// NameDatabase::Init — base ctor + vtable + zero the four arrays
// ============================================================
// Mirrors MenuScene_Setup_sub_c660 (0x1400a...c660).
static void NameDatabase_Init(void* mem) {
    if (!mem) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    if (fn_External_External) fn_External_External(mem);     // Object::External base
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    // Four {data,count,cap} array headers at +0x20..+0x5F → all zero.
    for (int off = 0x20; off < 0x60; off += 8)
        *(uint64_t*)(s + off) = 0;
}

// ============================================================
// NameDatabase::Serialize (vtable +0x28) — faithful port of 0x1400a45e0
// ============================================================
extern "C" void NameDatabase_Serialize(void* self, void* stream) {
    if (!self) return;
    NameDatabase_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Loading when the serializer's stream mode field (stream+8) == 0.
    auto isLoading = [&]() -> bool {
        void* s0 = *(void**)ser;
        return s0 && static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    };

    // --- array1: stride 0xc (3 fields/elem) ---
    int c1 = *(int*)(s + 0x28);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c1, nullptr);   // B.3.4 bridge
    if (isLoading() && s_resize1) s_resize1(s + 0x20, c1);
    if (c1 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x20);   // element0
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
    int c2 = *(int*)(s + 0x38);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c2, nullptr);   // B.3.4 bridge
    if (isLoading() && s_resize2) s_resize2(s + 0x30, c2);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c2, reinterpret_cast<unsigned short*>(*(void**)(s + 0x30)), nullptr);   // B.3.4 bridge

    // --- array3: stride 0x1 (byte buffer) — bulk ---
    int c3 = *(int*)(s + 0x48);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c3, nullptr);   // B.3.4 bridge
    if (isLoading() && s_resize3) s_resize3(s + 0x40, c3);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c3, reinterpret_cast<unsigned char*>(*(void**)(s + 0x40)), nullptr);   // B.3.4 bridge

    // --- array4: stride 0x4 (int) — bulk ---
    int c4 = *(int*)(s + 0x58);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c4, nullptr);   // B.3.4 bridge
    if (isLoading() && s_resize4) s_resize4(s + 0x50, c4);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(c4, reinterpret_cast<int*>(*(void**)(s + 0x50)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// NameDatabase::Dtor (vtable +0x00) — free arrays then base dtor
// ============================================================
// Mirrors 0x14001c790 -> FUN_1400a4790.
extern "C" void* NameDatabase_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    // ENDGAME B.3.2 step 2: four trivial-element `Collection::Array::~Array`s,
    // back to front (+0x50 int32, +0x40 byte, +0x30, +0x20). Each was
    // MemFree(data); data = 0; one qword over size+capacity = 0 —
    // `rtcoll::ArrayDtorImpl` exactly, with no element step. The int32 and
    // byte arrays are trait-derived rows; the two at +0x20/+0x30 are not pinned
    // to an element type here, so the header alias is used for all four.
    for (int off = 0x50; off >= 0x20; off -= 0x10)
        reinterpret_cast<rtcoll::ArrayHdr*>(s + off)->~Array();   // B.3.2 bridge
    if (fn_External_Dtor) fn_External_Dtor(self);            // base dtor
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t NameDatabase_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("NameDatabase");
    return cached;
}
static int   NDB_Return0x401(void* /*self*/) { return 0x401; }  // GetVersion
static int   NDB_Return1(void* /*self*/)     { return 1; }      // vtable +0x20
static int   NDB_Return0(void* /*self*/)     { return 0; }
static void* NDB_ReturnNull(void*, ...)      { return nullptr; }
extern "C" void* NameDatabase_CreateInPlace(void* mem);

// NameDatabase::Cleanup (vtable +0x30, 0x14001c730): dtor(self,0) then re-init.
static void* NameDatabase_Cleanup(void* self) {
    if (!self) return self;
    NameDatabase_Dtor(self, 0);
    NameDatabase_Init(self);
    return self;
}

// vtable @ 0x14015eea0 — 9 slots (+0x00..+0x40); string data follows in .rdata.
static void* g_NDB_Vtable[9] = {
    reinterpret_cast<void*>(&NameDatabase_Dtor),        // +0x00 dtor
    reinterpret_cast<void*>(&NDB_ReturnNull),           // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&NameDatabase_GetTypeId),   // +0x10 GetTypeId
    reinterpret_cast<void*>(&NDB_Return0x401),          // +0x18 GetVersion=0x401
    reinterpret_cast<void*>(&NDB_Return1),              // +0x20 (returns 1)
    reinterpret_cast<void*>(&NameDatabase_Serialize),   // +0x28 Serialize
    reinterpret_cast<void*>(&NameDatabase_Cleanup),     // +0x30 Cleanup
    reinterpret_cast<void*>(&NDB_Return0),              // +0x38 GetDesc forwarder
    reinterpret_cast<void*>(&NameDatabase_CreateInPlace), // +0x40 CreateInPlace
};

extern "C" void RegisterNameDatabaseVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_NDB_Vtable));
}

extern "C" void* NameDatabase_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, kSize, 8);
    if (!p) return nullptr;
    NameDatabase_Init(p);
    return p;
}
extern "C" void* NameDatabase_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    NameDatabase_Init(mem);
    return mem;
}

REGISTER_TYPE_FULL(NameDatabase, "Object::External", "ndb",
                   &NameDatabase_Factory, &NameDatabase_CreateInPlace);
