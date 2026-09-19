// code/Game/Component/citizencomponent_serialize.cpp
// Slot-3 (+0x18) stream Serialize for CitizenComponent.
//
//   CitizenComponent::Serialize   retail 0x140114160   object size 0x290
//
// Save/load wave (2026-08-27). Byte-faithful port of the 306-instruction retail
// body. Field list (Serializer version 3):
//   +0x28  System::String            name
//   +0x38  int                       (unnamed)
//   +0x3c..0x50  6 floats            (hunger/thirst/etc. accumulators)
//   +0x54  uint                      (flag bits)
//   +0x58  Time                      work timer
//   +0x60  Time                      (unnamed timer)
//   +0x68  ExternalPtr<Disease?>     helper_0790, validated 0x1401c5948
//   +0x70  Time
//   +0x78  Time
//   +0x80  float
//   +0x88  uint                      (flag bits)
//   +0x90  Time
//   +0x98/+0xa0  ExternalPtr<ComponentDescription>[count]  action-stack-like array,
//                load-side resize via FUN_140001bb0 (ported below as
//                ResizeCitizenExternalArray), elements via the ALREADY-PORTED
//                Reimpl_ExternalPtrArray_Serialize (serialize_externals_array.cpp;
//                same exe address FUN_1400049c0, validated ComponentDescription).
//   +0x1f0 ExternalPtr<Profession?>  helper_140018020 (called twice; same body,
//                validated TypeInfo_Profession 0x1401c5a98)
//   +0xac  int
//   +0x1d8 int
//   +0xa8  int                       (version > 0)
//   +0x1e4 int
//   +0x1e8 Time
//   +0x1e0 int                       (version > 1)
//   +0xb0  MeshInstance              via the Encoding-object virtual call retail
//                emits inline; equivalent to the already-resolved
//                Graphics::MeshInstance::Serialize dispatch slot 0x140152990
//                (see modelcomponent.cpp) called as (self+0xb0, ser).
//   +0x30  int
//   +0x238 IBP<ComponentEntity>      validated 0x1401c5d70
//   +0x240 int
//   +0x250/+600(0x258)/+0x260/+0x268/+0x270  IBP<CitizenComponent>  each validated
//                0x1401c7158 (CitizenComponent's own type id — GRADUATED default-on,
//                see project_citid_graduated memory)
//   +0x248 IBP<ComponentEntity>      validated 0x1401c5d70
//   +0x1f8 ExternalPtr<Profession?>  helper_140018020 again
//   +0x208 IBP                      validated 0x1401c3f40 (same raw cell used for
//                +0x228/+0x230 below — Ghidra emits the identical dword for all
//                three sites; resolved once, used generically per the house
//                pattern rather than guessing a C++ name for it)
//   +0x210 int
//   +0x218 Time                     (version > 2)
//   +0x220 bool
//   +0x228 IBP                      validated 0x1401c3f40
//   +0x230 IBP                      validated 0x1401c3f40
//   +0x278 IBP<CharacterComponent>  validated 0x1401c7078
//   +0x280 IBP<StatusIconComponent> validated 0x1401c7e08
//   +0x288 ExternalPtr<CitizenDescription> helper_0990, validated 0x1401c7190
//   +0x34  int
//
// Sub-callees:
//   * Component_SerializeReimpl        — already ported (component_factory_base.cpp)
//   * ExternalPtrSerializeGeneric       — already ported (Game/serialize_externals.cpp),
//                                          reused for helper_0790/helper_0990/helper_140018020
//                                          (all three are the SAME ExternalPtr<T>::Serialize
//                                          template body, differing only by validation type)
//   * Reimpl_ExternalPtrArray_Serialize — already ported (serialize_externals_array.cpp),
//                                          exact match for FUN_1400049c0
//   * FUN_140001bb0 (array-header resize) — ported below (ResizeCitizenExternalArray);
//                                          not previously registered anywhere in the repo.
//   * Graphics::MeshInstance::Serialize — dispatch slot 0x140152990, already resolved
//                                          in modelcomponent.cpp; resolved again locally
//                                          here (same house pattern as elsewhere).
// All scalar/Time/Bool/IBP primitives come from the shared engine_funcs.h globals
// already initialized at startup.

#include "resolve.h"
#include "engine_funcs.h"
#include "Game/serialize_externals.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

extern void Component_SerializeReimpl(void* self, void* stream);
extern "C" void Reimpl_ExternalPtrArray_Serialize(void* serializer, int count,
                                                  long long arrayBase, void* deadInit);

namespace {

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

// FUN_140001bb0 — resize an 8-byte-element array given a {ptr@0, count@8(int),
// capacity@0xc(int)} header, preserving existing elements and zero-filling new
// ones. Byte-faithful port (retail grows by 3x on a +1 append, matching the
// std::vector-ish growth policy used throughout this codebase's other resize
// helpers). Not previously ported/registered anywhere else in the repo.
void ResizeCitizenExternalArray(int64_t* header, int newCount) {
    int oldCount = static_cast<int>(header[1]);
    if (newCount == oldCount) return;

    void* oldPtr = reinterpret_cast<void*>(header[0]);
    *reinterpret_cast<int32_t*>(header + 1) = newCount;

    int32_t* capPtr = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(header) + 0xc);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            *capPtr = newCount * 3;
        void* newPtr = fn_MemAllocStatic
            ? fn_MemAllocStatic(static_cast<uint64_t>(*capPtr) * 8, 8)
            : nullptr;
        header[0] = reinterpret_cast<int64_t>(newPtr);
    }

    int startIdx = (oldPtr == reinterpret_cast<void*>(header[0])) ? oldCount : 0;
    void** newArr = reinterpret_cast<void**>(header[0]);
    void** oldArr = reinterpret_cast<void**>(oldPtr);
    for (int i = startIdx; i < newCount; ++i) {
        if (!newArr) break;
        if (i < oldCount) {
            newArr[i] = oldArr ? oldArr[i] : nullptr;
        } else {
            newArr[i] = nullptr;
        }
    }

    if (oldPtr != reinterpret_cast<void*>(header[0]) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldPtr);
}

typedef void (*fn_MeshInstance_Serialize_t)(void* meshInstance, void* serializer);
fn_MeshInstance_Serialize_t s_fn_MeshInstance_Serialize = nullptr;
bool s_resolved = false;

void Resolve() {
    if (s_resolved) return;
    void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152990));
    if (slot && *slot)
        s_fn_MeshInstance_Serialize = reinterpret_cast<fn_MeshInstance_Serialize_t>(*slot);
    s_resolved = true;
}

inline bool Loading(void* stream) {
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(stream) + 8) == 0;
}

void SerializeValidatedIBP(void* ibp, void* ser, uintptr_t addr) {
    const uint32_t* tid = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, addr));
    if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(ibp)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    void* obj = *reinterpret_cast<void**>(ibp);
    const bool checked = (obj && fn_IsDerivedFrom && tid && *tid);
    const bool derived = checked ? fn_IsDerivedFrom(obj, *tid) : true;
    if (checked && !derived)
        *reinterpret_cast<uint64_t*>(ibp) = 0;

}

} // namespace

extern "C" void CitizenComponent_Serialize(void* self, void* stream) {
    if (!self || !stream) return;
    Resolve();
    Component_SerializeReimpl(self, stream);

    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge

    reinterpret_cast<System::String*>(At(self, 0x28))->Serialize(ser);   // B.3.1 bridge
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x38)), nullptr);   // B.3.4 bridge
    if (fn_Serializer_Serialize_Float) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x3c)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x40)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x44)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x48)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x4c)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x50)), nullptr);   // B.3.4 bridge
    }
    if (fn_Serializer_Serialize_Uint)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(At(self, 0x54)), nullptr);   // B.3.4 bridge
    if (g_fn_Time_Serialize) {
        g_fn_Time_Serialize(At(self, 0x58), ser);
        g_fn_Time_Serialize(At(self, 0x60), ser);
    }

    {
        const uint32_t* tid = reinterpret_cast<const uint32_t*>(
            ResolveAddr(Module::Exe, 0x1401c5948));
        ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x68)), ser,
                                    tid, "CitizenDisease");
    }

    if (g_fn_Time_Serialize) {
        g_fn_Time_Serialize(At(self, 0x70), ser);
        g_fn_Time_Serialize(At(self, 0x78), ser);
    }
    if (fn_Serializer_Serialize_Float)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x80)), nullptr);   // B.3.4 bridge
    if (fn_Serializer_Serialize_Uint)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(At(self, 0x88)), nullptr);   // B.3.4 bridge
    if (g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0x90), ser);

    int arrCount = *reinterpret_cast<int*>(At(self, 0xa0));
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(arrCount, nullptr);   // B.3.4 bridge
    if (Loading(stream))
        ResizeCitizenExternalArray(reinterpret_cast<int64_t*>(At(self, 0x98)), arrCount);
    Reimpl_ExternalPtrArray_Serialize(ser, arrCount,
        *reinterpret_cast<int64_t*>(At(self, 0x98)), nullptr);

    {
        const uint32_t* tidProf = reinterpret_cast<const uint32_t*>(
            ResolveAddr(Module::Exe, 0x1401c5a98));
        ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x1f0)), ser,
                                    tidProf, "Profession");
    }

    if (g_fn_Serialize_Int) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0xac)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x1d8)), nullptr);   // B.3.4 bridge
    }
    if (g_fn_Serializer_GetVersion) {
        int v = reinterpret_cast<IO::Serializer*>(ser)->GetVersion();   // B.3.4 bridge
        if (v > 0 && g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0xa8)), nullptr);   // B.3.4 bridge
    }
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x1e4)), nullptr);   // B.3.4 bridge
    if (g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0x1e8), ser);
    if (g_fn_Serializer_GetVersion) {
        int v = reinterpret_cast<IO::Serializer*>(ser)->GetVersion();   // B.3.4 bridge
        if (v > 1 && g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x1e0)), nullptr);   // B.3.4 bridge
    }

    // +0xb0 is a PLAIN UINT, not a MeshInstance (save/load wave 2026-08-28).
    // Retail's inlined `CALL [encodingVtbl+0x98]` here is Serialize(uint&) —
    // the identical slot the +0x54/+0x88 uints above go through; the real
    // MeshInstance sub-object starts at +0xB8 and this stream never touches
    // it. The previous MeshInstance-dispatch call fed a raw uint's address as
    // a MeshInstance "this" and corrupted the round-trip from +0xb0 onward
    // (wrong citizen clothing after load was the visible symptom).
    if (fn_Serializer_Serialize_Uint)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(At(self, 0xb0)), nullptr);   // B.3.4 bridge

    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x30)), nullptr);   // B.3.4 bridge

    SerializeValidatedIBP(At(self, 0x238), ser, 0x1401c5d70);
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x240)), nullptr);   // B.3.4 bridge
    SerializeValidatedIBP(At(self, 0x250), ser, 0x1401c7158);
    SerializeValidatedIBP(At(self, 0x258), ser, 0x1401c7158);
    SerializeValidatedIBP(At(self, 0x260), ser, 0x1401c7158);
    SerializeValidatedIBP(At(self, 0x268), ser, 0x1401c7158);
    SerializeValidatedIBP(At(self, 0x270), ser, 0x1401c7158);
    SerializeValidatedIBP(At(self, 0x248), ser, 0x1401c5d70);

    {
        const uint32_t* tidProf = reinterpret_cast<const uint32_t*>(
            ResolveAddr(Module::Exe, 0x1401c5a98));
        ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x1f8)), ser,
                                    tidProf, "Profession");
    }

    SerializeValidatedIBP(At(self, 0x208), ser, 0x1401c3f40);
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x210)), nullptr);   // B.3.4 bridge
    if (g_fn_Serializer_GetVersion) {
        int v = reinterpret_cast<IO::Serializer*>(ser)->GetVersion();   // B.3.4 bridge
        if (v > 2 && g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0x218), ser);
    }
    if (g_fn_Serialize_Bool) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(self, 0x220)), nullptr);   // B.3.4 bridge
    SerializeValidatedIBP(At(self, 0x228), ser, 0x1401c3f40);
    SerializeValidatedIBP(At(self, 0x230), ser, 0x1401c3f40);
    SerializeValidatedIBP(At(self, 0x278), ser, 0x1401c7078);
    SerializeValidatedIBP(At(self, 0x280), ser, 0x1401c7e08);

    {
        const uint32_t* tidDesc = reinterpret_cast<const uint32_t*>(
            ResolveAddr(Module::Exe, 0x1401c7190));
        ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x288)), ser,
                                    tidDesc, "CitizenDescription");
    }

    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x34)), nullptr);   // B.3.4 bridge


    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
