#pragma once
// TradeWork — ENDGAME B.3.8: the ctor, the dtor and the slot bodies are members of the
// class (Work/TradeWork.h). What is left: the file's helper resolver (tradecomponent.cpp
// and tradecomponent_states.cpp call it by name). (The slot-4 door at 0x1400f5d40
// went with its reader-less row, B.3.8 CB3.)

#include "Work/TradeWork.h"
#include <cstdint>

bool TradeWork_ResolveFuncs();
