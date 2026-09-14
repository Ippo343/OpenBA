// code/Game/Component/uicomponent_serialize.cpp
// Slot-3 (+0x18) stream Serialize body for UIComponent
// (retail 0x14012dd00 — Ghidra names it UIComponent_Shutdown; per disassembly
// it first CALLs Component_Serialize, confirming this is really Serialize).
//
// Retail body (verified by decompile + disassembly of 0x14012dd00):
//   Component_Serialize(self, stream)
//   Serializer ser(stream, /*version*/0)
//   count = *(int*)(self + 0x30)
//   Serialize(ser, &count, null)                    // int, r/w in place
//   if (loading) FUN_14000a590(self + 0x28, count)   // resize byte array
//   Serialize(ser, count, *(uint8_t**)(self+0x28), null)   // raw byte array
//   ~Serializer
//
// FUN_14000a590 (0x14000a590) is a growable-array resize helper operating on
// the triple {ptr(8)@+0x00, count(4)@+0x08, cap(4)@+0x0c} with 1-byte
// elements (MemAlloc(newCap, /*align*/1)) — i.e. self+0x28 is a genuine
// growable uint8_t array whose header spans self+0x28..0x37 (ptr@0x28,
// count@0x30, cap@0x34). This fills every byte of UIComponent that the ctor
// (engine/types/UIComponent.h) leaves zeroed — uiData0/uiData1 are that
// header, not two independent scratch pointers — so this class is NOT empty:
// it round-trips a real per-instance byte buffer through save/load.
//
// The byte-array Serialize overload is a distinct dispatch slot from the
// scalar int/bool/float Serialize used elsewhere (0x140152400, called as
// Serialize(ser, count, ptr, name) — RCX=ser, EDX=count, R8=ptr, R9=name).
//
// ⚠ NOT WIRED as an opt-in feature: slot 3 is wired UNCONDITIONALLY (see
// UIComponent_FinishInertVtableSer in uicomponent.cpp) — serialize round-trip
// is not behind any BANISHED_LIVE_* gate. Standalone has no save/load driver
// yet (GameUpdateLoop::SaveRestore is a no-op stub), so this is unreachable
// today; it brings the class to parity with the rest of the save/load wave.
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdint>

namespace {

typedef void (*fn_ComponentSerialize_t)(void* self, void* stream);
typedef void (*fn_SerCtor_t)(void* ser, void* stream, int version);
typedef void (*fn_SerDtor_t)(void* ser);
typedef void (*fn_SerInt_t)(void* ser, void* field, const char* name);
typedef void (*fn_SerByteArray_t)(void* ser, int count, void* data, const char* name);

fn_ComponentSerialize_t s_Base     = nullptr;  // 0x1401189b0
fn_SerCtor_t            s_Ctor     = nullptr;  // slot 0x140152ad8
fn_SerDtor_t            s_Dtor     = nullptr;  // slot 0x140152ad0
fn_SerInt_t             s_Int      = nullptr;  // slot 0x140152ac8
fn_SerByteArray_t       s_ByteArr  = nullptr;  // slot 0x140152400

bool s_resolved = false;

// Resolved unconditionally at the top of every call — this can be the very
// first call into the class on load-from-save, so lazy/cached resolution
// elsewhere is not safe (see animalwave_serialize.cpp's header note).
void Resolve() {
    if (s_resolved) return;
    s_Base = reinterpret_cast<fn_ComponentSerialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_Ctor = *reinterpret_cast<fn_SerCtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_Dtor = *reinterpret_cast<fn_SerDtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_Int = *reinterpret_cast<fn_SerInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152400);
    if (slot) s_ByteArr = *reinterpret_cast<fn_SerByteArray_t*>(slot);

    s_resolved = true;
}

// True when the serializer is READING (retail: *(int*)(stream + 8) == 0,
// reached through the serializer's first field).
inline bool Loading(void* ser) {
    return *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(ser) + 8) == 0;
}

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

}  // namespace

// ============================================================
// UIComponent::Serialize (vtable slot [3], 0x14012dd00)
// ============================================================
extern "C" void UIComponent_Serialize(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor || !s_Int) return;
    if (s_Base) s_Base(self, stream);

    uint8_t ser[24];
    s_Ctor(ser, stream, 0);

    int count = *reinterpret_cast<int*>(At(self, 0x30));
    s_Int(ser, &count, nullptr);
    // B.3.2 bridge: FUN_14000a590 is Collection::Array<uint8_t>::SetSize(n)
    if (Loading(ser)) GameColl::AsArray<GameColl::U8>(At(self, 0x28)).SetSize(count);

    if (s_ByteArr) {
        uint8_t* ptr = *reinterpret_cast<uint8_t**>(At(self, 0x28));
        s_ByteArr(ser, count, ptr, nullptr);
    }

    s_Dtor(ser);
}
