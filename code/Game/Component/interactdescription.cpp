// code/Game/Component/interactdescription.cpp
// Clean reimplementation of InteractDescription (entity sub-description).
//
// InteractDescription (0x58 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140163930.  GetVersion = 1.
//
// Field layout (from Factory InteractDescription_sub_3710 + Serialize
// FUN_140123500 + dtor FUN_140035280):
//   +0x20: void*  _descRef        ExternalPtr (Description)   (default 0)
//   +0x28: { void* data; int count; int cap }   _useNodes   (0x18-stride, default empty)
//   +0x38: { void* data; int count; int cap }   _buildNodes (0x18-stride, default empty)
//   +0x48: { void* data; int count; int cap }   _createNodes(0x18-stride, default empty)
//
// The three node arrays are built in eh_vector_constructor_iterator (3 elements,
// stride 0x10) at +0x28 in the factory, and torn down in the dtor.
//
// Serialize (FUN_140123500):
//   STREAM-ACCURATE part: exactly ONE field is read from the stream —
//     ExternalPtr serialize (+0x20)  FUN_14000ab20 (identical layout to Serialize2).
//   POST-LOAD build (load only, computed — NOT streamed, so it never desyncs the
//   stream cursor): walks the resolved external's child array (data @ +0x30,
//   count @ +0x38, stride 0x18). For each child whose name String == "use" /
//   "build" / "create", builds a 0x18-byte node {offset.xyz (child +0x8/+0xc/+0x10),
//   dir = Vector3::FromYawPitch(yaw=child+0x14, pitch=0)} and appends to the
//   corresponding node array (+0x28 / +0x38 / +0x48).
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

namespace {
constexpr uintptr_t kVtable  = 0x140163930;
constexpr int       kVersion = 1;

// Append one 0x18-byte node {6 dwords from src} to array header {ptr,count,cap}.
// Mirrors FUN_140035360 for the incremental (count+1) growth case.
void AppendNode(void* header, const uint32_t src[6]) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    int count = *(int32_t*)(h + 8);
    int cap   = *(int32_t*)(h + 0xc);
    void* oldBuf = *(void**)h;
    int newCount = count + 1;
    if (cap < newCount) {
        int newCap = (count != 0) ? newCount * 3 : newCount;  // matches exe growth
        *(int32_t*)(h + 0xc) = newCap;
        void* nb = (fn_MemAlloc && g_pMemAllocSingleton)
                   ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 0x18, 4)
                   : nullptr;
        if (oldBuf && nb && count > 0) {
            memcpy(nb, oldBuf, (size_t)count * 0x18);
        }
        *(void**)h = nb;
        if (oldBuf && oldBuf != nb && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
    }
    *(int32_t*)(h + 8) = newCount;
    uint8_t* buf = *(uint8_t**)h;
    if (buf) memcpy(buf + (size_t)count * 0x18, src, 0x18);
}
}

static void* InteractDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;   // _descRef
    // three array headers at +0x28/+0x38/+0x48 (each 0x10 bytes, zeroed)
    memset((void*)(s + 0x28), 0, 0x30);
    return mem;
}

// Serialize (vtable +0x28, FUN_140123500)
void InteractDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // ---- stream-accurate: single ExternalPtr (+0x20) ----
    fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);

    // ---- post-load build (load only; reads from the resolved external) ----
    void* serObj = *(void**)(serializer + 8);
    bool loading = serObj && (*(int*)((uintptr_t)serObj + 8) == 0);
    void* ext = *(void**)(s + 0x20);
    if (loading && ext && fn_String_Equals && fn_Math_Vector3_FromYawPitch) {
        uint8_t* children = *(uint8_t**)((uintptr_t)ext + 0x30);
        int childCount = *(int32_t*)((uintptr_t)ext + 0x38);
        for (int i = 0; children && i < childCount; ++i) {
            uint8_t* child = children + (size_t)i * 0x18;  // {String@0, f@8, f@0xc, f@0x10, yaw@0x14}
            float yaw   = *(float*)(child + 0x14);
            float pitch = 0.0f;
            // node = {offset.xyz, dir.xyz}
            uint32_t node[6];
            node[0] = *(uint32_t*)(child + 0x08);
            node[1] = *(uint32_t*)(child + 0x0c);
            node[2] = *(uint32_t*)(child + 0x10);
            float dir[3] = {0, 0, 0};
            fn_Math_Vector3_FromYawPitch(dir, &yaw, &pitch);
            node[3] = *(uint32_t*)&dir[0];
            node[4] = *(uint32_t*)&dir[1];
            node[5] = *(uint32_t*)&dir[2];

            if (fn_String_Equals(child, "use")) {
                AppendNode((void*)(s + 0x28), node);
            } else if (fn_String_Equals(child, "build")) {
                AppendNode((void*)(s + 0x38), node);
            } else if (fn_String_Equals(child, "create")) {
                AppendNode((void*)(s + 0x48), node);
            }
        }
    }

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, FUN_140035280) — frees the three node-array buffers.
void* InteractDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_MemFreeStatic) {
        for (int off = 0x28; off <= 0x48; off += 0x10) {
            fn_MemFreeStatic(*(void**)(s + off));
            *(uint64_t*)(s + off) = 0;
        }
    }
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t InteractDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("InteractDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&InteractDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),               // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&InteractDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),               // +0x18 (=1)
    reinterpret_cast<void*>(&Vtbl_Return0),                  // +0x20 Compile
    reinterpret_cast<void*>(&InteractDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                  // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                  // +0x38 GetDesc
    reinterpret_cast<void*>(&Vtbl_ReturnNull),               // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),               // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),               // +0x50 guard
};

extern "C" void RegisterInteractDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* InteractDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    if (!p) return nullptr;
    return InteractDescription_Ctor_Impl(p);
}
extern "C" void* InteractDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return InteractDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(InteractDescription, "Description",
              &InteractDescription_Factory, &InteractDescription_CreateInPlace);
#endif
