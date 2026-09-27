// code/Game/Component/componentdescription.cpp
// Foundation C keystone — ComponentDescription (the entity-template container).
//
// ComponentDescription (0x38 bytes) extends Object::External. It is the top-level
// type of every entity Template (Template\Citizen.rsc etc.). Layout:
//   +0x00 vtable (0x140162a00)
//   +0x08 Object::External base
//   +0x20 int   category (default 6)
//   +0x28 void* subDescArray  — array of sub-description External* (stride 8)
//   +0x30 int   subDescCount
//   +0x34 int   subDescCap
//
// Loading a template: Factory creates a ComponentDescription, Serialize reads the
// category + sub-description count, resizes the array, then loads each sub-
// description as its own External resource via ExternalFactory::FindOrLoad
// (FUN_140032090). Each sub-description (CitizenDescription, ModelDescription, …)
// has its own factory/Serialize — see the other Foundation C files.
//
// Reimplemented for standalone (the original 0x14014ea90 / 0x140118af0 / 0x140032090
// aren't mapped). Mirrors mapgenerator.cpp (deserialize) + statgoal.cpp (scaffolding)
// + serialize_struct60b_serialize.cpp (the proven External-array FindOrLoad body).

#include "Component/ComponentDescription.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <new>


// ---- internal: serializer "stream error" word (serializer->stream + 8 == 0 ⇒ ok)
static inline int SerStreamErr(void* serializer) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
}

// ============================================================
// Sub-description array deserialize (FUN_140032090)
// ============================================================
// Per element: load the referenced sub-description External via FindOrLoad
// (reference-mode or path-string mode, plus the encoding==3 export walk). This is
// the proven body from serialize_struct60b_serialize.cpp, minus type validation
// (sub-descriptions self-identify their type in the package; we trust it).
// Exported so other description types with External-ref arrays (AnimalDescription,
// etc.) reuse this exact proven loop.
// B6-5: descriptions load on the loader thread; the one-shot log flag is an atomic
// at namespace scope (was a function-local plain bool).
static std::atomic<bool> s_warnedUnwired{false};

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase) {
    if (!serializer || count <= 0 || !arrayBase) return;
    if (!::Object::ExternalFactory::_singleton) {
        if (!s_warnedUnwired.exchange(true)) { fprintf(stderr, "[ComponentDescription] sub-array primitives unwired — skip %d\n", count); fflush(stderr); }
        return;
    }
    void* factory = ::Object::ExternalFactory::_singleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge

    void* encoding = const_cast<IO::Encoding*>(&reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (!encoding) return;
    using EncFn = int (*)(void*);
    EncFn getState = reinterpret_cast<EncFn>(
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(encoding)) + 0x10));
    int encState = getState ? getState(encoding) : 0;

    void** slots = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        void** slot = &slots[i];

        if (encState == 3) {                       // path-export walk (read-only)
            if (!*slot) continue;
            System::Filename pathBuf =
                reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
            (void)efactory->FindOrLoad(pathBuf);
            continue;
        }

        System::Filename fnameBuf;
        bool refMode = efactory->IsReferenceModeEnabled();

        if (refMode) {
            uint32_t refId = 0xFFFFFFFFu;
            if (*slot) refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*slot));  // B.3.3 bridge
            bool needsPath = false;
            if (!*slot || SerStreamErr(serializer) != 0 || refId != 0xFFFFFFFFu) {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
                if (refId != 0xFFFFFFFFu) {
                    if (SerStreamErr(serializer) == 0) {
                        System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                        fnameBuf = tmpFn;
                    }
                    needsPath = true;
                }
            } else {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
            }
            if (needsPath && SerStreamErr(serializer) == 0) {
                *slot = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnameBuf)));
            }
        } else {
            bool hasRef = (*slot != nullptr);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(hasRef, nullptr);   // B.3.4 bridge
            if (hasRef) {
                if (*slot) {
                    fnameBuf = reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
                }
                fnameBuf.AsString().Serialize(serializer);
                if (SerStreamErr(serializer) == 0) {
                    *slot = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnameBuf)));
                }
            }
        }
    }
}


// ============================================================
// ComponentDescription — ENDGAME B.3.8 K1: a REAL class (ComponentDescription.h)
// with a compiler-generated 9-slot vtable. The hand-built 11-slot table (slot 8
// null, slot 9 past retail's end holding createInPlace) and its
// RegisterExeAddr(0x140162a00) row are gone: nothing in the tree reads that
// address back (B38_RULES D5).
// ============================================================
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"

// ctor (inlined into create 0x140031f30 / createInPlace 0x140031f90):
// `??0External@Object@@`, the stamp (the compiler's), then the defaults.
ComponentDescription::ComponentDescription()
    : Object::External(),
      _category(6),                 // default category
      _subDescs(nullptr),
      _subDescCount(0),
      _subDescCap(0) {}

// dtor (slot 0 0x140032030 minus the stamps): free the sub-description pointer
// array (the sub-descriptions themselves are owned by the ExternalFactory cache,
// not freed here). The compiler then runs ~External.
ComponentDescription::~ComponentDescription() {
    if (_subDescs && fn_MemFreeStatic) fn_MemFreeStatic(_subDescs);
    _subDescs = nullptr;
    _subDescCount = 0;
}

// [0] 0x140032030: ~ComponentDescription -> ~External, MemFree on &1.
Object::Base* ComponentDescription::DeletingDtor(unsigned flags) {
    this->~ComponentDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140118af0.
bool ComponentDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)

    // category enum (+0x20) through the UINT door. `serBuf + 8` was the
    // ReaderWriter, not an Encoding, and vtable +0x98 is rw_vtbl::kScalarUInt.
    ser.Serialize(*reinterpret_cast<unsigned int*>(&_category), nullptr);   // 0x98 = uint door

    // sub-description count (+0x30) + fresh array resize (+0x28, stride 8)
    ser.Serialize(_subDescCount, nullptr);
    int count = _subDescCount;
    if (SerStreamErr(&ser) == 0 && count > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 8, 8);
        _subDescs = static_cast<void**>(buf);
        _subDescCap = count;
        if (buf) for (int i = 0; i < count; ++i) static_cast<void**>(buf)[i] = nullptr;
    }

    // load each sub-description (FUN_140032090)
    Desc_LoadExternalArray(&ser, count, _subDescs);
    return true;
}

// [6] 0x140031fe0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* ComponentDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) ComponentDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void ComponentDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140031f90: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* ComponentDescription::Construct() {
    return new (static_cast<void*>(this)) ComponentDescription();
}

// create 0x140031f30: MemAlloc(0x38, 8), then the (inlined) ctor.
void* ComponentDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ComponentDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ComponentDescription());
}
// createInPlace 0x140031f90.
void* ComponentDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ComponentDescription());
}

DEFINE_EXE_VTABLE(ComponentDescription, "_ZTV20ComponentDescription")
DEFINE_EXE_TYPE(ComponentDescription, "Object::External")
