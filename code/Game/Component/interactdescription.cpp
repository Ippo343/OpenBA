// code/Game/Component/interactdescription.cpp
// Clean reimplementation of InteractDescription (entity sub-description).
//
// InteractDescription (0x58 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140163930.  GetVersion = 1.
//
// Field layout (from Factory InteractDescription_sub_3710 + Serialize
// FUN_140123500 + dtor FUN_140035280):
//   +0x20: void*  _descRef        ExternalPtr<Pathfinding::PointList> (default 0) —
//          the exe's serializer 0x14000ab20 type-checks it against
//          ?_typeInfo@PointList@Pathfinding@@ (import 0x140151448), B37_RULES 6.2
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
//   stream cursor): walks the resolved PointList's `_points` array
//   (Pathfinding::PointList::Point, stride 0x18). For each child whose name String == "use" /
//   "build" / "create", builds a 0x18-byte node {offset.xyz (child +0x8/+0xc/+0x10),
//   dir = Vector3::FromYawPitch(yaw=child+0x14, pitch=0)} and appends to the
//   corresponding node array (+0x28 / +0x38 / +0x48).
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (InteractDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163930) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/InteractDescription.h"
#include "Component/InteractComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "math/Math.h"
#include "io/Stream.h"
#include "pathfinding/PointList.h"
#include <cstring>
#include <initializer_list>
#include <cstdint>
#include <new>

namespace {
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
}  // namespace

// ctor (inlined into create 0x140035200 / createInPlace 0x140035240):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
InteractDescription::InteractDescription()
    : Description(),
      _descRef(nullptr) {
    _useNodes = _buildNodes = _createNodes = NodeArray{nullptr, 0, 0};
}

// dtor (slot 0 0x140035280 minus the stamps): frees the three node arrays (FUN_140035280).
// The compiler then chains ~Description -> ~External.
InteractDescription::~InteractDescription() {
    if (fn_MemFreeStatic) {
        for (NodeArray* a : {&_useNodes, &_buildNodes, &_createNodes}) {
            fn_MemFreeStatic(a->data);
            a->data = nullptr;
        }
    }
}

// [5] 0x140123500: stream order as retail.
bool InteractDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_Serialize2) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    // ---- stream-accurate: single ExternalPtr (+0x20) ----
    fn_ExternalPtr_Serialize2(&_descRef, &ser);
    
    // ---- post-load build (load only; reads from the resolved external) ----
    // Direction flag: retail tests *(int*)(stream + 8) == 0 (FUN_140123500 reads
    // param_2+8 DIRECTLY). The old read `*(serializer+8)` grabbed the serializer's
    // second qword instead of the stream's direction field -> `loading` was always
    // false -> the use/build/create node lists never built -> ChildSum(interact)==0
    // -> d5d0 found no access point -> the eat/food search rejected every barn
    // (Economy Brick M-D root cause #3).
    bool loading = (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
    void* ext = _descRef;
    if (loading && ext) {
        // Pathfinding::PointList::_points (+0x30; Points() is LEA RAX,[RCX+0x30]).
        const Pathfinding::PointList* pointList = reinterpret_cast<const Pathfinding::PointList*>(ext);   // B.3.7 bridge
        uint8_t* children = reinterpret_cast<uint8_t*>(pointList->_points._data);
        int childCount = pointList->_points._size;
        for (int i = 0; children && i < childCount; ++i) {
            uint8_t* child = children + (size_t)i * 0x18;  // {String@0, f@8, f@0xc, f@0x10, yaw@0x14}
            float yaw   = *(float*)(child + 0x14);
            float pitch = 0.0f;
            // node = {offset.xyz, dir.xyz}
            uint32_t node[6];
            node[0] = *(uint32_t*)(child + 0x08);
            node[1] = *(uint32_t*)(child + 0x0c);
            node[2] = *(uint32_t*)(child + 0x10);
            Math::Vector3 dir = Math::Vector3::FromYawPitch(yaw, pitch);
            node[3] = *(uint32_t*)&dir.x;
            node[4] = *(uint32_t*)&dir.y;
            node[5] = *(uint32_t*)&dir.z;
    
            if (*reinterpret_cast<System::String*>(child) == "use") {  // B.3.1 bridge
                AppendNode(&_useNodes, node);
            } else if (*reinterpret_cast<System::String*>(child) == "build") {  // B.3.1 bridge
                AppendNode(&_buildNodes, node);
            } else if (*reinterpret_cast<System::String*>(child) == "create") {  // B.3.1 bridge
                AppendNode(&_createNodes, node);
            }
        }
    }
    return true;
}

// [6] 0x140035250: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* InteractDescription::Reconstruct() {
    this->~InteractDescription();
    return new (static_cast<void*>(this)) InteractDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void InteractDescription::Destruct() { this->~InteractDescription(); }

// [8] 0x140035270: InteractComponent's type id through the hidden return slot.
Object::Type* InteractDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = InteractComponent::TypeId();
    return ret;
}

// [9] 0x140035240: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* InteractDescription::Construct() {
    return new (static_cast<void*>(this)) InteractDescription();
}

// create 0x140035200: MemAlloc(0x58, 8), then the (inlined) ctor.
void* InteractDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(InteractDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) InteractDescription());
}
// createInPlace 0x140035240.
void* InteractDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) InteractDescription());
}

DEFINE_EXE_TYPE(InteractDescription, "Description")
