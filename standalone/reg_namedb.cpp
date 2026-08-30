// standalone/reg_namedb.cpp
// NameDatabase::GenerateName — the .ndb name generator's retail entry point.
//
// addr          symbol                  one-liner
// -----------   ---------------------   ----------------------------------------
// 0x1400a4b30   NameDB_GenerateName     retry-until-accepted 4-gram name generate
//
// Body: code/Game/Game/citizen_names.cpp (which also holds the ports of the
// inner Markov walk 0x1400a4880 and the gender picker 0x1400a4c60 — neither is
// registered because nothing resolves them by address).
//
// Without this, fn_NameDB_GenerateName (engine_funcs.cpp, ResolveAddr
// 0x1400a4b30) stays NULL and both town-name buttons are dead: NewGameDialog's
// "new name" and MapParamsDialog's in-game rename.
#include "resolve.h"

extern "C" void* NameDB_GenerateName(void* retStr, void* db, void* rng,
                                     int minLen, int maxLen);  // 0x1400a4b30

void RegisterNameDb() {
    RegisterExeAddr(0x1400a4b30, reinterpret_cast<uintptr_t>(&NameDB_GenerateName));
}
