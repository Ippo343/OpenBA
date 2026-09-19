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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {

typedef void (*fn_ComponentSerialize_t)(void* self, void* stream);

fn_ComponentSerialize_t s_Base     = nullptr;  // 0x1401189b0

bool s_resolved = false;

// Resolved unconditionally at the top of every call — this can be the very
// first call into the class on load-from-save, so lazy/cached resolution
// elsewhere is not safe (see animalwave_serialize.cpp's header note).
void Resolve() {
    if (s_resolved) return;
    s_Base = reinterpret_cast<fn_ComponentSerialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_resolved = true;
}

// True when the serializer is READING (retail: *(int*)(stream + 8) == 0,
// reached through the serializer's first field).
inline bool Loading(void* ser) {
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
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
    if (!self || !stream) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int count = *reinterpret_cast<int*>(At(self, 0x30));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
    // B.3.2 bridge: FUN_14000a590 is Collection::Array<uint8_t>::SetSize(n)
    if (Loading(ser)) GameColl::AsArray<GameColl::U8>(At(self, 0x28)).SetSize(count);

    uint8_t* ptr = *reinterpret_cast<uint8_t**>(At(self, 0x28));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, ptr, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
