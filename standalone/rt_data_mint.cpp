// ============================================================================
// standalone/rt_data_mint.cpp — the minted exported DATA (storage rows only)
// ============================================================================
//
// See rt_data_mint.h for what "minting" means. The data is generated into
// rt_data_mint_tables.inc by tools/rt_mint_gen.py.
//
// ENDGAME B.3.9 (the end of B.3) retired every minted vtable and every minted
// Object::BaseTypeInfo: each Runtime class is a real C++ class with its
// compiler vtable and its DEFINE_TYPE record. The vtable minting (per-slot
// traps filled from the port registry, the COMPLETE count), the BaseTypeInfo
// minting (factory traps, the parents-first chain splice into
// `?_firstTypeInfo@`), the factory roster and the minted-node predicates went
// with them (commit 851adb15 has the last full version), and the D7 storage
// REDIRECT (a class static standing in for a minted cell until its last
// by-name / by-address reader was converted) went with its last call. What is
// left is STORAGE: exported `.data` statics with no C++ home yet, owned here as
// zeroed bytes and registered through both doors.
//
// RESIDUE DISCIPLINE: nothing in this file may call ResolveExport (it records
// into the residue census); the mint only registers.
// ============================================================================

#include "resolve.h"
#include "rt_data_mint.h"

#include <cstdio>

// ============================================================
// The shape tools/rt_mint_gen.py emits.
// ============================================================
struct RtMintStorage {
    const char*        symbol;   // nullptr for a row carried by address only
    unsigned long long dllVa;
    unsigned int       size;
    void*              storage;
    const char*        note;
};

#include "rt_data_mint_tables.inc"

static const int kStorageCount = (int)(sizeof(kMintStorage) / sizeof(kMintStorage[0]));

// Every row is BSS in the DLL (the harvest read b'' from all of them), so
// "minting" is owning zeroed bytes -- kStorageBytes is a static array and
// therefore already zero -- and letting the existing writer fill them.
//
// BOTH DOORS, always: a caller may ask for exported DATA by NAME
// (ResolveExport / RT_DATA_SLOT) or by ADDRESS (ResolveAddr(Module::Runtime,
// 0x1801fc718)), and a row registered under one door only is a NULL at the
// other. The address door is also the only door of a row with no exported name.
static bool g_minted = false;

void RtDataMintInit() {
    if (g_minted) return;
    g_minted = true;

    int names = 0, addrs = 0;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        if (s.symbol) { RegisterRuntimeExport(s.symbol, s.storage); ++names; }
        RegisterRuntimeAddr((uintptr_t)s.dllVa, s.storage); ++addrs;
    }
    printf("[RtMint] registered %d by name, %d by address\n", names, addrs);
    printf("[RtMint] minted %d storage symbols (%zu bytes); no vtable and no "
           "BaseTypeInfo is minted (ENDGAME B.3.9)\n",
           kStorageCount, sizeof(kStorageBytes));
}
