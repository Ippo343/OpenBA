// code/Game/Game/musiclist.cpp
// Clean reimplementation of the MusicList resource type.
//
// MusicList (0x30 bytes) — the music-track playlist resource (Game\MusicTracks.rsc).
// Extends Object::External (0x20 base). Vtable @ 0x14015ee40.
// Registered (NullFactory stub) at exe 0x14014cc00 (MenuScene_StaticInit).
//
// It is loaded by External::Load when the in-game resource preload
// (UpdateLoop::LoadResources) reads Game\MusicTracks.rsc out of the resource
// tree. In standalone it had only a NullFactory stub, so ExternalFactory::
// FindOrLoad got NULL back and crashed in IsDerivedFrom(NULL) (Runtime+0xC039A).
// This file provides the real factory + faithful Serialize, mirroring the
// Toolbar / Scenario / NameDatabase factory-cascade pattern.
//
// Layout (from factory sub_c260, dtor 0x14001c360, Serialize 0x1400a3e90):
//   +0x00 vtable (0x14015ee40)
//   +0x08..+0x1F  Object::External base
//   +0x20 entries.data   (void*, dynamic array of 0x18-byte track entries)
//   +0x28 entries.count  (int)
// (The array header is the standard {data,count,cap} shape; the resize helper
//  at exe 0x14001c3c0 manages it. cap lives at +0x2c.)
//
// Each track entry (stride 0x18):
//   +0x00 ExternalPtr (8 bytes, ref to the track's audio resource)
//   +0x08 float   +0x0c float   +0x10 int   +0x14 int
//
// Serialize (vtable +0x28, exe 0x1400a3e90): serialize count int; if loading
// (serializer version word == 0) resize the array; then per element serialize
// float@+0x08, float@+0x0c, int@+0x10, int@+0x14, ExternalPtr@+0x00 (THIS ORDER —
// serialize order is load-bearing). The +0x00 field is serialized by exe
// FUN_1400019a0 = the templated Object::ExternalPtr::Serialize (NOT String::Serialize)
// — see the per-element note below. GetFileVersion (vtable +0x18, exe 0x1400a3f80)
// = 1 (verified: `mov eax,1; ret`).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

static constexpr uintptr_t kVtableAddr = 0x14015ee40;
static constexpr int       kSize       = 0x30;
static constexpr size_t    kEntryStride = 0x18;

// Exe-internal array resize helper for the 0x18-byte track entries.
//   DLL mode: reuse the real exe function (it default-inits new slots correctly).
//   Standalone: exe-internal addr is not registered → ResolveAddr returns 0, so we
//   fall back to a faithful local resize with the same default-init.
static constexpr uintptr_t kResizeEntries = 0x14001c3c0;

// ============================================================
// Local resize fallback (standalone) — mirrors exe FUN_14001c3c0
// ============================================================
// Standard {data,count,cap} header at arrayHeader (= &self[+0x20]). Grows the
// 0x18-byte entry array and default-inits NEW elements to:
//   String@+0x00 = null, float@+0x08 = 0, float@+0x0c = 1.0f, int@+0x14 = 11.
// (int@+0x10 is left zero — the exe inits the {+0x08=0,+0x0c=1.0f,+0x14=11}
//  triple plus the null String; +0x10 falls out as zero from the alloc clear.)
static void MusicList_ResizeEntries_Fallback(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    if (newCount < 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[MusicList::Resize] negative newCount=%d header=%p — stream "
                   "misaligned upstream; leaving array unchanged\n",
                   newCount, arrayHeader);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return;

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeader);
    void**   dataSlot  = reinterpret_cast<void**>(hdr + 0x00);
    int32_t* countSlot = reinterpret_cast<int32_t*>(hdr + 0x08);
    int32_t* capSlot   = reinterpret_cast<int32_t*>(hdr + 0x0C);

    int oldCount = *countSlot;
    if (newCount == oldCount) return;

    void* oldData = *dataSlot;
    *countSlot = newCount;

    if (*capSlot < newCount) {
        *capSlot = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capSlot = newCount * 3;
        }
        size_t bytes = static_cast<size_t>(*capSlot) * kEntryStride;
        *dataSlot = fn_MemAlloc(g_pMemAllocSingleton, bytes, 8);
    }

    int copyStart = (oldData == *dataSlot) ? oldCount : 0;
    auto* dst = reinterpret_cast<uint8_t*>(*dataSlot);

    for (int slot = copyStart; slot < newCount; ++slot) {
        uint8_t* elem = dst + static_cast<size_t>(slot) * kEntryStride;
        if (!elem) continue;
        if (slot < oldCount) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldData)
                               + static_cast<size_t>(slot) * kEntryStride;
            for (size_t i = 0; i < kEntryStride; ++i) elem[i] = src[i];
        } else {
            // Default-init a fresh entry (matches exe FUN_14001c3c0).
            for (size_t i = 0; i < kEntryStride; ++i) elem[i] = 0;
            *reinterpret_cast<void**>(elem + 0x00)  = nullptr;   // String
            *reinterpret_cast<float*>(elem + 0x08)  = 0.0f;
            *reinterpret_cast<float*>(elem + 0x0c)  = 1.0f;
            *reinterpret_cast<int32_t*>(elem + 0x14) = 11;
        }
    }

    if (oldData != *dataSlot && oldData != nullptr && fn_MemFreeStatic) {
        fn_MemFreeStatic(oldData);
    }
}

typedef void (*Resize_t)(void* arrayHeader, int newCount);
static Resize_t s_resize = nullptr;
static bool     s_resolved = false;

static void MusicList_ResolveFuncs() {
    if (s_resolved) return;
    s_resolved = true;
    // DLL mode: real exe helper resolves; standalone: addr unregistered → 0,
    // and we use the local fallback below.
    s_resize = reinterpret_cast<Resize_t>(ResolveAddr(Module::Exe, kResizeEntries));
    if (!s_resize) s_resize = &MusicList_ResizeEntries_Fallback;
}

// ============================================================
// MusicList::Init — base ctor + vtable + zero the entry array
// ============================================================
// Mirrors factory sub_c260: External::External(self); vtable; +0x20=0; +0x28=0.
static void MusicList_Init(void* mem) {
    if (!mem) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    if (fn_External_External) fn_External_External(mem);     // Object::External base
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    *(uint64_t*)(s + 0x20) = 0;   // entries.data (+0x20) + count (+0x28 low) cleared
    *(uint64_t*)(s + 0x28) = 0;   // entries.count (+0x28) + cap (+0x2c)
}

// ============================================================
// MusicList::Serialize (vtable +0x28) — faithful port of 0x1400a3e90
// ============================================================
extern "C" void MusicList_Serialize(void* self, void* stream) {
    if (!self) return;
    MusicList_ResolveFuncs();
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    int count = *(int*)(s + 0x28);
    g_fn_Serialize_Int(ser, &count, nullptr);

    // Loading when the serializer's stream-mode field == 0. The original reads
    // *(int*)(local_20[0] + 8), i.e. DEREFERENCE the serializer's first qword
    // (the stream/state ptr) then +8 — NOT serializer+8. (Canonical idiom, see
    // namedatabase.cpp.) Reading ser+8 directly mis-evaluates the load flag, so
    // the resize is skipped, entries.data stays null, and the per-element read
    // below faults (dst = elem+0x08 = 0x8 when elem == null).
    void* serState = *(void**)ser;
    if (serState && *(int*)((uint8_t*)serState + 8) == 0 && s_resize) {
        s_resize(s + 0x20, count);   // &entries header
    }

    // Per-element read. Guard on a non-null buffer: if the array wasn't
    // allocated (e.g. count==0, or a resize that couldn't allocate), don't
    // deref null. With the load flag fixed this holds whenever count>0.
    uint8_t* elem = *(uint8_t**)(s + 0x20);   // entries.data
    if (count > 0 && elem) {
        for (int i = 0; i < count; ++i) {
            // ORDER IS LOAD-BEARING: float@+0x08, float@+0x0c, int@+0x10,
            // int@+0x14, then String@+0x00.
            if (fn_Serializer_Serialize_Float)
                fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(elem + 0x08), nullptr);
            if (fn_Serializer_Serialize_Float)
                fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(elem + 0x0c), nullptr);
            g_fn_Serialize_Int(ser, elem + 0x10, nullptr);
            g_fn_Serialize_Int(ser, elem + 0x14, nullptr);
            // +0x00 is an Object::External REFERENCE (the track's audio resource),
            // NOT a String. The original element serializer (exe FUN_1400019a0) is
            // the templated ExternalPtr::Serialize (encoding/ref-mode aware: reads a
            // uint refId in reference mode, else a bool + path String) — NOT
            // System::String::Serialize. fn_ExternalPtr_Serialize2 (0x14000bf40) is
            // the byte-identical instantiation (same one UIDescription's element
            // ptrs use). Reading +0x00 as a bare String consumed the wrong byte
            // count and drifted the whole element loop past EOF.
            if (fn_ExternalPtr_Serialize2)
                fn_ExternalPtr_Serialize2(elem + 0x00, ser);
            elem += kEntryStride;
        }
    }

    g_fn_Serializer_Dtor(ser);
}

// ============================================================
// MusicList::Dtor (vtable +0x00, exe 0x14001c360) — free array, base dtor
// ============================================================
extern "C" void* MusicList_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    if (fn_MemFreeStatic) fn_MemFreeStatic(*(void**)(s + 0x20));   // free entries.data
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);                  // base dtor
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// MusicList::Cleanup (vtable +0x30, exe 0x14001c310) — dtor(0) then re-init
// ============================================================
extern "C" void* MusicList_Cleanup(void* self) {
    if (!self) return self;
    // Original calls the vtable[0] dtor with flags=0, then re-inits in place.
    MusicList_Dtor(self, 0);
    MusicList_Init(self);
    return self;
}

// ============================================================
// MusicList::CreateInPlace (vtable +0x40, exe 0x14001c2c0) — in-place init
// ============================================================
extern "C" void* MusicList_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    MusicList_Init(mem);
    return mem;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

// Slot2 (vtable +0x10): GetTypeId — original returns TypeInfo_MusicList; standalone
// reads it from REGISTER_TYPE storage.
static uint32_t MusicList_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MusicList");
    return cached;
}

// Slot3 (vtable +0x18): GetFileVersion — original (0x1400a3f80) is `mov eax,1; ret`.
static int MusicList_GetFileVersion(void* /*self*/) { return 1; }

// 9-slot vtable matching exe 0x14015ee40 (Object::External base layout).
// Base/Runtime thunk slots (+0x08, +0x20, +0x38) stay null — same as Toolbar.
static void* g_MusicList_Vtable[9] = {
    reinterpret_cast<void*>(&MusicList_Dtor),           // +0x00 Dtor
    nullptr,                                            // +0x08 (base thunk 0x14001c250)
    reinterpret_cast<void*>(&MusicList_GetTypeId),      // +0x10 GetTypeId
    reinterpret_cast<void*>(&MusicList_GetFileVersion), // +0x18 GetFileVersion = 1
    nullptr,                                            // +0x20 (base thunk 0x14003a912)
    reinterpret_cast<void*>(&MusicList_Serialize),      // +0x28 Serialize
    reinterpret_cast<void*>(&MusicList_Cleanup),        // +0x30 Cleanup
    nullptr,                                            // +0x38 (base 0x140001640)
    reinterpret_cast<void*>(&MusicList_CreateInPlace),  // +0x40 CreateInPlace (init)
};

extern "C" void RegisterMusicListVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_MusicList_Vtable));
}

extern "C" void* MusicList_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, kSize, 8);  // MusicList size = 0x30
    if (!p) return nullptr;
    MusicList_Init(p);
    return p;
}

// Parent "Object::External" matches the original MusicList registration.
REGISTER_TYPE(MusicList, "Object::External",
              &MusicList_Factory, &MusicList_CreateInPlace);

