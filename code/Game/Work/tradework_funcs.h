#pragma once
// TradeWork — ENDGAME B.3.8: the ctor, the dtor and the slot bodies are members of the
// class (Work/TradeWork.h). What is left: the file's helper resolver (tradecomponent.cpp
// and tradecomponent_states.cpp call it by name) and the slot-4 door
// standalone/reg_work_tail_trade.cpp registers at 0x1400f5d40.

#include "Work/TradeWork.h"
#include <cstdint>

int TradeWork_Slot4_Impl(TradeWork* self, void* citizenIBP, uint64_t param3, uint64_t param4);

bool TradeWork_ResolveFuncs();
