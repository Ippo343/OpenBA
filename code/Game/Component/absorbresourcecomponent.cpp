// code/Game/Component/absorbresourcecomponent.cpp
// Clean reimplementation of AbsorbResourceComponent lifecycle methods.
//
// AbsorbResourceComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Only one non-trivial vtable override:
//   Shutdown (0x140105c00) -- Component_Serialize + two empty Serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/absorbresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(this, stream, version) -- dispatch slot at 0x140152ad8
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this) -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// AbsorbResourceComponent::Shutdown (vtable slot [3], 0x140105c00)
// ============================================================
// Pattern: Component_Serialize + two empty Serializer ctor/dtor pairs.
// Same pattern as MarketLocationComponent and StorageLocationComponent.
void AbsorbResourceComponent_Shutdown(AbsorbResourceComponent* self, void* stream) {
    // Slot-3 serializers are dispatched straight off the vtable by the entity
    // stream, so nothing else ever reaches this file. Resolving here is what
    // keeps the pointers below non-null -- when they were null the whole body
    // read ZERO bytes and silently desynced every object after it.
    AbsorbResourceComponent_InitFuncs();
    // Call base Component_Serialize
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        ::new (static_cast<void*>(serializerBuf1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializerBuf1)->~Serializer();                                       // B.3.4 bridge
    }

    // Second empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();                                       // B.3.4 bridge
    }
}

// ============================================================
// Reimpl of FUN_1401189b0 (Component_Serialize — the shared Component base
//   serialize used by every Component subclass)
// ============================================================
// Persist the Object base, then serialize the owner InternalBasePtr (+0x10) with a
// ComponentEntity type-check, a uint capability word (+0x20), and the external
// description reference (+0x18) via Component_helper_1d10.
// Dispatch slots (exe .rdata): Persist 0x1401515e0, Serializer ctor 0x140152ad8,
// IBP::Serialize 0x140152aa0, IsDerivedFrom 0x140152a98, Serialize-uint 0x140152a50,
// Serializer dtor 0x140152ad0. Direct: helper_1d10 0x140031d10.
typedef void (*fn_CS_Persist_t)(void* self, void* stream);
typedef void (*fn_CS_Ctor_t)(void* ser, void* stream, int ver);
typedef void (*fn_CS_IBPSer_t)(void* ibp, void* ser);
typedef bool (*fn_CS_IsDerived_t)(void* obj, uint32_t tid);
typedef void (*fn_CS_SerUint_t)(void* ser, uint32_t* field, const char* name);
typedef void (*fn_CS_Helper1d10_t)(void* extRef, void* ser);
typedef void (*fn_CS_Dtor_t)(void* ser);
static fn_CS_Persist_t    s_cs_Persist = nullptr;
static fn_CS_Ctor_t      s_cs_Ctor = nullptr;
static fn_CS_IBPSer_t    s_cs_IBPSer = nullptr;
static fn_CS_IsDerived_t  s_cs_IsDerived = nullptr;
static fn_CS_SerUint_t   s_cs_SerUint = nullptr;
static fn_CS_Helper1d10_t s_cs_Helper1d10 = nullptr;
static fn_CS_Dtor_t      s_cs_Dtor = nullptr;
static uint32_t*         s_cs_pTId_CompEntity = nullptr;  // DWORD_1401c5d70
static bool s_csResolved = false;
static void EnsureComponentSerializeHelpers() {
    if (s_csResolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401515e0);
    if (slot) s_cs_Persist = *reinterpret_cast<fn_CS_Persist_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_cs_Ctor = *reinterpret_cast<fn_CS_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa0);
    if (slot) s_cs_IBPSer = *reinterpret_cast<fn_CS_IBPSer_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_cs_IsDerived = *reinterpret_cast<fn_CS_IsDerived_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a50);
    if (slot) s_cs_SerUint = *reinterpret_cast<fn_CS_SerUint_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_cs_Dtor = *reinterpret_cast<fn_CS_Dtor_t*>(slot);
    s_cs_Helper1d10 = reinterpret_cast<fn_CS_Helper1d10_t>(ResolveAddr(Module::Exe, 0x140031d10));
    s_cs_pTId_CompEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));
    s_csResolved = true;
}

// ============================================================
// Reimpl of Component_helper_1d10 (0x140031d10) — Component reg wave B.
// This is the canonical Object::ExternalPtr<T>::Serialize body specialised for the
// Description-family External validated against DAT_1401c72e0 (a Description type id,
// xref'd from Description_Register). It is byte-identical to the other ExternalPtr<T>
// serialize bodies, so we delegate to the shared ExternalPtrSerializeGeneric rather
// than re-port the read/write/FindOrLoad/IsDerivedFrom logic. Closure-safe: only the
// engine serialize/factory externals. The typeId is resolved at call time; a null
// typeId makes the generic body load without validation (cascade-safe).
// ============================================================
void Component_Helper1d10_Reimpl(void* extRefField, void* serializer) {
    static const uint32_t* s_typeId = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_typeId = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c72e0));
        s_resolved = true;
    }
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(extRefField), serializer,
                                s_typeId, "Description");
}

void Component_SerializeReimpl(void* selfP, void* stream) {
    EnsureComponentSerializeHelpers();
    uintptr_t self = (uintptr_t)selfP;

    if (s_cs_Persist) s_cs_Persist(selfP, stream);

    alignas(8) uint8_t ser[24];
    if (s_cs_Ctor) ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    if (s_cs_IBPSer) s_cs_IBPSer((void*)(self + 0x10), ser);
    void* owner = *(void**)(self + 0x10);
    if (owner && s_cs_IsDerived && s_cs_pTId_CompEntity &&
        !s_cs_IsDerived(owner, *s_cs_pTId_CompEntity)) {
        *(uint64_t*)(self + 0x10) = 0;
    }

    if (s_cs_SerUint) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(self + 0x20), nullptr);   // B.3.4 bridge
    if (s_cs_Helper1d10) s_cs_Helper1d10((void*)(self + 0x18), ser);

    if (s_cs_Dtor) reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AbsorbResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;

#ifdef _DEBUG
    printf("[AbsorbResourceComponent] Funcs resolved: Serialize=%p Ctor=%p Dtor=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid AbsorbResourceComponent
// (size 0x48, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from AbsorbResourceComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Slot 3 must be wired: retail 0x140105c00 is Component::Serialize plus TWO
// empty Serializer scopes, and an empty scope still occupies 16 bytes in the
// save encoding — the base-only fill read 32 bytes too few and desynced the
// stream for every object after it.
COMPONENT_INERT_FACTORY_SER(AbsorbResourceComponent, 0x48, 0x80,
                            AbsorbResourceComponent_Shutdown)
REGISTER_TYPE(AbsorbResourceComponent, "ControllerComponent",
              &AbsorbResourceComponent_Factory, &AbsorbResourceComponent_CreateInPlace);
