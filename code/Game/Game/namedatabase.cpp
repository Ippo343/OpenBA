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
#include <cstdint>
#include <cstdio>

static constexpr uintptr_t kVtableAddr = 0x14015eea0;
static constexpr int       kSize       = 0x60;

// Array resize helpers (exe addresses; we register our reimpls for these).
static constexpr uintptr_t kResizeArr1 = 0x14001c7c0;  // stride 0xc
static constexpr uintptr_t kResizeArr2 = 0x14001c890;  // stride 0x2
static constexpr uintptr_t kResizeArr3 = 0x14000a590;  // stride 0x1 (byte)
static constexpr uintptr_t kResizeArr4 = 0x1400021d0;  // stride 0x4

// IO::Serializer dispatch slots used by NameDatabase::Serialize.
typedef void  (*Ser_Ctor_t)(void* self, void* stream, int version);
typedef void  (*Ser_Dtor_t)(void* self);
typedef void  (*Ser_Int_t)(void* ser, void* field, const char* name);     // Serialize(int&)
typedef void  (*Ser_Byte_t)(void* ser, void* field, const char* name);    // Serialize(uchar&)
typedef void  (*Ser_UInt_t)(void* ser, void* field, const char* name);    // Serialize(uint&)
typedef void  (*Ser_Bytes_t)(void* ser, int count, void* data, const char* name);   // SerializeBytes
typedef void  (*Ser_BulkUShort_t)(void* ser, int count, void* data, const char* name);
typedef void  (*Ser_BulkByte_t)(void* ser, int count, void* data, const char* name);
typedef void  (*Ser_BulkInt_t)(void* ser, int count, void* data, const char* name);
typedef void  (*Resize_t)(void* arrayHeader, int newCount);

static Ser_Ctor_t       s_ctor       = nullptr;
static Ser_Dtor_t       s_dtor       = nullptr;
static Ser_Int_t        s_int        = nullptr;
static Ser_Byte_t       s_byte       = nullptr;
static Ser_UInt_t       s_uint       = nullptr;
static Ser_Bytes_t      s_bytes      = nullptr;
static Ser_BulkUShort_t s_bulkUShort = nullptr;
static Ser_BulkByte_t   s_bulkByte   = nullptr;
static Ser_BulkInt_t    s_bulkInt    = nullptr;
static Resize_t         s_resize1    = nullptr;
static Resize_t         s_resize2    = nullptr;
static Resize_t         s_resize3    = nullptr;
static Resize_t         s_resize4    = nullptr;
static bool             s_resolved   = false;

template <typename T>
static T ResolveSlot(uintptr_t slot) {
    uintptr_t iat = ResolveAddr(Module::Exe, slot);
    return iat ? *reinterpret_cast<T*>(iat) : nullptr;
}

static void NameDatabase_ResolveFuncs() {
    if (s_resolved) return;
    s_resolved = true;

    s_ctor       = ResolveSlot<Ser_Ctor_t>(0x140152ad8);
    s_dtor       = ResolveSlot<Ser_Dtor_t>(0x140152ad0);
    s_int        = ResolveSlot<Ser_Int_t>(0x140152ac8);
    s_byte       = ResolveSlot<Ser_Byte_t>(0x140152418);
    s_uint       = ResolveSlot<Ser_UInt_t>(0x140152a50);
    s_bytes      = ResolveSlot<Ser_Bytes_t>(0x140152478);
    s_bulkUShort = ResolveSlot<Ser_BulkUShort_t>(0x140152488);
    s_bulkByte   = ResolveSlot<Ser_BulkByte_t>(0x140152490);
    s_bulkInt    = ResolveSlot<Ser_BulkInt_t>(0x140151278);

    s_resize1 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr1));
    s_resize2 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr2));
    s_resize3 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr3));
    s_resize4 = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeArr4));

    // Standalone fallbacks via Runtime exports for the serialize overloads that
    // have no engine_funcs global (the IAT dispatch above already covers both
    // modes once the slots are registered, but resolve defensively by name too).
    if (!s_ctor)  s_ctor  = reinterpret_cast<Ser_Ctor_t>((uintptr_t)g_fn_Serializer_Ctor);
    if (!s_dtor)  s_dtor  = reinterpret_cast<Ser_Dtor_t>((uintptr_t)g_fn_Serializer_Dtor);
    if (!s_int)   s_int   = reinterpret_cast<Ser_Int_t>((uintptr_t)g_fn_Serialize_Int);
    if (!s_byte)  s_byte  = reinterpret_cast<Ser_Byte_t>(ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z"));
    if (!s_uint)  s_uint  = reinterpret_cast<Ser_UInt_t>(ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z"));
    if (!s_bytes) s_bytes = reinterpret_cast<Ser_Bytes_t>(ResolveExport("?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z"));
    if (!s_bulkUShort) s_bulkUShort = reinterpret_cast<Ser_BulkUShort_t>(ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAGPEBD@Z"));
    if (!s_bulkByte)   s_bulkByte   = reinterpret_cast<Ser_BulkByte_t>(ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z"));
    if (!s_bulkInt)    s_bulkInt    = reinterpret_cast<Ser_BulkInt_t>(ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAHPEBD@Z"));
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
    if (!s_ctor || !s_dtor || !s_int) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);

    uint8_t ser[24];
    s_ctor(ser, stream, 0);

    // Loading when the serializer's stream mode field (stream+8) == 0.
    auto isLoading = [&]() -> bool {
        void* s0 = *(void**)ser;
        return s0 && *(int*)((uint8_t*)s0 + 8) == 0;
    };

    // --- array1: stride 0xc (3 fields/elem) ---
    int c1 = *(int*)(s + 0x28);
    s_int(ser, &c1, nullptr);
    if (isLoading() && s_resize1) s_resize1(s + 0x20, c1);
    if (c1 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x20);   // element0
        for (int i = 0; i < c1; ++i) {
            if (s_bytes) s_bytes(ser, 4, p + 0x0, nullptr);  // field0: 4 raw bytes
            if (s_byte)  s_byte(ser, p + 0x4, nullptr);      // field1: uchar
            if (s_uint)  s_uint(ser, p + 0x8, nullptr);      // field2: uint
            p += 0xc;
        }
    }

    // --- array2: stride 0x2 (uint16) — bulk ---
    int c2 = *(int*)(s + 0x38);
    s_int(ser, &c2, nullptr);
    if (isLoading() && s_resize2) s_resize2(s + 0x30, c2);
    if (s_bulkUShort) s_bulkUShort(ser, c2, *(void**)(s + 0x30), nullptr);

    // --- array3: stride 0x1 (byte buffer) — bulk ---
    int c3 = *(int*)(s + 0x48);
    s_int(ser, &c3, nullptr);
    if (isLoading() && s_resize3) s_resize3(s + 0x40, c3);
    if (s_bulkByte) s_bulkByte(ser, c3, *(void**)(s + 0x40), nullptr);

    // --- array4: stride 0x4 (int) — bulk ---
    int c4 = *(int*)(s + 0x58);
    s_int(ser, &c4, nullptr);
    if (isLoading() && s_resize4) s_resize4(s + 0x50, c4);
    if (s_bulkInt) s_bulkInt(ser, c4, *(void**)(s + 0x50), nullptr);

    s_dtor(ser);
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
