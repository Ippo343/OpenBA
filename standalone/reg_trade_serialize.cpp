// TradeComponent — slot-3 stream Serialize registration (save/load parity).
//
// Registers the retail address of TradeComponent::Serialize
// (code/Game/Component/tradecomponent_serialize.cpp) so
// ResolveAddr(Module::Exe, ...) finds it. Same rationale as
// reg_animal_serialize.cpp: nothing reachable today (standalone has no
// save/load driver yet), and this is deliberately NOT wired into
// TradeComponent's live vtable here — slot-3 wiring is a repo-wide pass
// once a save/load driver exists (see animalwave_serialize.cpp's header for
// the full reasoning: wiring some components' slot 3 while others keep the
// inert no-op would desync every byte after the first missing component on
// load).
//
// Hook: address_registry.cpp calls RegisterTradeSerialize().
#include "resolve.h"
#include <cstdint>

class TradeComponent;
extern "C" void TradeComponent_Serialize(TradeComponent* self, void* stream);

void RegisterTradeSerialize() {
    RegisterExeAddr(0x14005bd50, reinterpret_cast<uintptr_t>(&TradeComponent_Serialize));
}
