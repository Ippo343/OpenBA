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
// `?_firstTypeInfo@`), the factory roster and the minted-node predicates are
// deleted with them (`git show 851adb15:standalone/rt_data_mint.cpp` holds the
// last version). What is left is STORAGE: exported `.data` statics with no C++
// home yet, owned here as zeroed bytes and registered through both doors.
//
// RESIDUE DISCIPLINE: nothing in this file may call ResolveExport (it records
// into the residue census); the mint only registers.
// ============================================================================

#include "resolve.h"
#include "rt_data_mint.h"
#include "rt_mint_redirect.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// The shape tools/rt_mint_gen.py emits.
// ============================================================
struct RtMintStorage {
    const char*        symbol;   // nullptr for a row carried by address only
    unsigned long long dllVa;
    unsigned int       size;
    void*              storage;
    const char*        note;
    // A KNOWN CORRECT INITIAL VALUE, for a row whose retail writer stores a
    // compile-time constant (EXTRA_INIT in tools/rt_mint_gen.py).
    const unsigned char* init;
    unsigned             initLen;
};

#include "rt_data_mint_tables.inc"

static const int kStorageCount = (int)(sizeof(kMintStorage) / sizeof(kMintStorage[0]));

// ============================================================
// B.3.9 (D7) -- the storage REDIRECT. Contract and seeding rule:
// rt_mint_redirect.h. Every storage path below asks MintStorageCell(i), never
// `kMintStorage[i].storage` directly.
// ============================================================
static bool g_minted = false;
static RtMintRedirect::Table<64> g_redirects;

// The cell the row lives in: the owner's redirected static, else the mint's.
static void* MintStorageCell(int i) {
    void* c = g_redirects.CellFor(i);
    return c ? c : kMintStorage[i].storage;
}

void RtMintRedirectStorage(const char* symbol, void* cell) {
    RtMintRedirect::RowView rows[kStorageCount];
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        rows[i] = { s.symbol, s.dllVa, s.size, s.init, s.initLen, false };
    }
    RtMintRedirect::Result r = g_redirects.Add(symbol, cell, rows, kStorageCount, g_minted);
    if (r == RtMintRedirect::kOk) return;
    fprintf(stderr, "[RtMint] FATAL: RtMintRedirectStorage(\"%s\", %p) refused: the row %s.\n",
            symbol ? symbol : "(null)", cell, RtMintRedirect::ResultText(r));
    fflush(stderr);
    abort();
}

// ============================================================
// The mint
// ============================================================

// Every row is BSS in the DLL (the harvest read b'' from all of them), so
// "minting" is owning zeroed bytes and letting the existing writer fill them.
// kStorageBytes is a static array and therefore already zero; this loop says
// so, and seeds a row that has a known constant initial value. A REDIRECTED
// row is not memset or seeded here: its cell is a C++ static the language
// zeroed and RtMintRedirectStorage() seeded, once (rt_mint_redirect.h).
static void MintStorage() {
    int seeded = g_redirects.Seeded();
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        void* cell = MintStorageCell(i);
        if (cell != s.storage) continue;
        memset(cell, 0, s.size);
        if (s.init && s.initLen && s.initLen <= s.size) {
            memcpy(cell, s.init, s.initLen);
            ++seeded;
        }
    }
    if (seeded)
        printf("[RtMint] seeded %d storage row(s) with a known constant value\n", seeded);
}

// BOTH DOORS, always: a caller may ask for exported DATA by NAME
// (ResolveExport / RT_DATA_SLOT) or by ADDRESS (ResolveAddr(Module::Runtime,
// 0x1801fc718)), and a row registered under one door only is a NULL at the
// other. The address door is also the only door of a row with no exported name.
static void RegisterMintedData() {
    int names = 0, addrs = 0;
    for (int i = 0; i < kStorageCount; ++i) {
        const RtMintStorage& s = kMintStorage[i];
        void* cell = MintStorageCell(i);
        if (s.symbol) { RegisterRuntimeExport(s.symbol, cell); ++names; }
        RegisterRuntimeAddr((uintptr_t)s.dllVa, cell); ++addrs;
    }
    printf("[RtMint] registered %d by name, %d by address\n", names, addrs);
}

void RtDataMintInit() {
    if (g_minted) return;
    g_minted = true;

    MintStorage();
    RegisterMintedData();

    printf("[RtMint] minted %d storage symbols (%zu bytes); no vtable and no "
           "BaseTypeInfo is minted (ENDGAME B.3.9)\n",
           kStorageCount, sizeof(kStorageBytes));
}
