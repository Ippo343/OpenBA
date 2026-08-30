// standalone/reg_namedb.cpp
// NameDatabase::GenerateName — the .ndb name generator's retail entry point.
//
// addr          symbol                  one-liner
// -----------   ---------------------   ----------------------------------------
// 0x1400a4b30   NameDB_GenerateName     retry-until-accepted 4-gram name generate
//
// 0x1400a4c60   NameDB_PickAndGenerate  gender -> .ndb picker, tail-calls the above
//
// Body: code/Game/Game/citizen_names.cpp (which also holds the port of the
// inner Markov walk 0x1400a4880 — that one stays unregistered because nothing
// resolves it by address).
//
// 0x1400a4c60 USED to be unregistered on the same "nothing resolves it" grounds.
// That premise went stale when TradeComponent::Setup (tradecomponent.cpp:295)
// added exactly that resolve: with the pointer null it left its String scratch
// zeroed and wrote an EMPTY name into every merchant slot, so every trader
// rendered blank in the trade UI.
//
// Without this, fn_NameDB_GenerateName (engine_funcs.cpp, ResolveAddr
// 0x1400a4b30) stays NULL and both town-name buttons are dead: NewGameDialog's
// "new name" and MapParamsDialog's in-game rename.
#include "resolve.h"

extern "C" void* NameDB_GenerateName(void* retStr, void* db, void* rng,
                                     int minLen, int maxLen);  // 0x1400a4b30
extern "C" void* NameDB_PickAndGenerate(void* retStr, void* rng, char female);  // 0x1400a4c60

void RegisterNameDb() {
    RegisterExeAddr(0x1400a4b30, reinterpret_cast<uintptr_t>(&NameDB_GenerateName));
    RegisterExeAddr(0x1400a4c60, reinterpret_cast<uintptr_t>(&NameDB_PickAndGenerate));
}
