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
#include "Component/UIComponent.h"
#include "Component/coll_bridge.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>

// ============================================================
// UIComponent::Persist (vtable slot [3], 0x14012dd00)
// ============================================================
// B.3.8 CB1b: the body moved here from the free UIComponent_Serialize.
void UIComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)

    int count = *reinterpret_cast<int*>(&uiData1);   // +0x30 count
    ser.Serialize(count, nullptr);
    // B.3.2 bridge: FUN_14000a590 is Collection::Array<uint8_t>::SetSize(n)
    // True when the serializer is READING (retail: *(int*)(stream + 8) == 0).
    if (static_cast<int>(ser.GetDirection()) == 0)   // IO::Serializer::GetDirection, retail 0x180006210
        GameColl::AsArray<GameColl::U8>(&uiData0).SetSize(count);

    uint8_t* ptr = static_cast<uint8_t*>(uiData0);
    ser.Serialize(count, ptr, nullptr);
}

// 0x14012dd00 door: registered by address (standalone/reg_ambientenv_ui_serialize.cpp).
extern "C" void UIComponent_Serialize(void* self, void* stream) {
    if (!self) return;
    reinterpret_cast<UIComponent*>(self)->UIComponent::Persist(stream);   // B.3.8 bridge
}
