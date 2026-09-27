#pragma once
// ============================================================================
// standalone/rt_mint_redirect.h — the storage REDIRECT table (B39_RULES D7)
// ============================================================================
//
// A minted storage row (`kMintStorage`, standalone/rt_data_mint_tables.inc)
// normally lives in the mint's own zeroed block. A namespace block that has
// given the row a C++ home (a class static, a file static) calls
//
//     RtMintRedirectStorage("<mangled name>" | "0x<VA>", &X::_s);
//
// from its `*PortInit()`. From then on the mint uses THAT cell for the row on
// every path: the name door, the address door, the manifest publish, the
// storage pass and the constructed-statics pass all go through
// `MintStorageCell()` (rt_data_mint.cpp), which consults this table first.
//
// THE SEEDING RULE (what a redirected cell holds, and who wrote it)
//
// A minted cell receives, exactly once: zero (MintStorage's memset), then the
// row's `kInit` constant if the generator carries one (EXTRA_INIT). A
// redirected cell receives the same bytes exactly once, but not from the mint:
//
//   1. ZERO comes from the language. The cell is an object of static storage
//      duration; C++ zero-initialises it before any code runs. The mint NEVER
//      memsets a redirected cell -- that would erase what CRT-init or the
//      owner's port-init wrote into it (the `_firstTypeInfo` chain is built by
//      DEFINE_TYPE constructors at CRT init, long before the mint).
//   2. The `kInit` SEED, when the row has one, is applied HERE, at the
//      redirect call, and only here: if the cell's bytes are all zero it is
//      seeded; if they already equal the seed (the class static's own
//      initialiser reproduces it) nothing is written; anything else aborts --
//      somebody wrote the cell before redirecting it, and "which value wins"
//      would be an accident of order. Everything the owner writes AFTER the
//      redirect call is its own and is never overwritten by the mint.
//   3. Rows the mint CONSTRUCTS (`_loader`, `Particle::_rng`) are refused:
//      the mint would construct into the class static a second time (D7: those
//      two convert in one commit with their constructor, never by redirect).
//
// Other refusals (each aborts at boot with a message naming the symbol): a key
// that is not a minted storage row, a null cell, the same row redirected to
// two different cells, a redirect made after RtDataMintInit() ran, a full
// table. A named row is redirected by its MANGLED NAME; a row the mint carries
// by address only (symbol == nullptr) by its "0x<VA>" spelling, matching the
// retirement fragments' key format.
//
// This header is the table's pure logic (no registry, no mint tables) so that
// tests/unit/mint_redirect_test.cpp can drive it without the game.
// ============================================================================

#include <cstdlib>
#include <cstring>

namespace RtMintRedirect {

// What the table needs to know about one storage row.
struct RowView {
    const char*          symbol;    // nullptr for a by-address row
    unsigned long long   dllVa;
    unsigned int         size;
    const unsigned char* init;      // the kInit seed, or nullptr
    unsigned int         initLen;
    bool                 constructed; // the mint constructs it (_loader, _rng)
};

enum Result {
    kOk = 0,
    kUnknownRow,      // key names no storage row
    kNullCell,
    kConstructed,     // a row the mint constructs
    kConflict,        // row already redirected to a different cell
    kSeedMismatch,    // cell neither zero nor equal to the row's seed
    kTableFull,
    kTooLate,         // RtDataMintInit() already ran
};

inline const char* ResultText(Result r) {
    switch (r) {
    case kOk:           return "ok";
    case kUnknownRow:   return "is not a minted storage row (misspelt, or the row was retired)";
    case kNullCell:     return "was given a null cell";
    case kConstructed:  return "is a row the mint CONSTRUCTS (_loader / _rng convert with their ctor, never by redirect -- D7)";
    case kConflict:     return "is already redirected to a different cell";
    case kSeedMismatch: return "carries a kInit seed and the cell already holds other non-zero bytes (written before the redirect)";
    case kTableFull:    return "does not fit: the redirect table is full";
    case kTooLate:      return "was redirected after RtDataMintInit() ran (call it from a *PortInit)";
    }
    return "?";
}

// Parses "0x<hex>" (case-insensitive digits). False for anything else.
inline bool ParseVaKey(const char* key, unsigned long long* va) {
    if (!key || key[0] != '0' || (key[1] != 'x' && key[1] != 'X') || !key[2]) return false;
    unsigned long long v = 0;
    for (const char* p = key + 2; *p; ++p) {
        char c = *p;
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *va = v;
    return true;
}

// The row a key names: a named row by its exact mangled name, a by-address row
// (symbol == nullptr) by "0x<VA>". A named row is NOT found by its VA -- its
// name is the literal the retirement refusal scans for. -1 when none.
inline int FindRow(const char* key, const RowView* rows, int n) {
    if (!key) return -1;
    unsigned long long va = 0;
    bool isVa = ParseVaKey(key, &va);
    for (int i = 0; i < n; ++i) {
        if (rows[i].symbol) {
            if (strcmp(rows[i].symbol, key) == 0) return i;
        } else if (isVa && rows[i].dllVa == va) {
            return i;
        }
    }
    return -1;
}

template <int Capacity>
class Table {
public:
    // Record `cell` as row `key`'s storage and apply the seeding rule above.
    Result Add(const char* key, void* cell, const RowView* rows, int n, bool minted) {
        if (minted) return kTooLate;
        int row = FindRow(key, rows, n);
        if (row < 0) return kUnknownRow;
        if (!cell) return kNullCell;
        const RowView& r = rows[row];
        if (r.constructed) return kConstructed;
        for (int i = 0; i < count_; ++i)
            if (entries_[i].row == row)
                return entries_[i].cell == cell ? kOk : kConflict;   // idempotent
        if (count_ >= Capacity) return kTableFull;
        if (r.init && r.initLen && r.initLen <= r.size) {
            unsigned char* b = (unsigned char*)cell;
            bool zero = true;
            for (unsigned k = 0; k < r.size; ++k) if (b[k]) { zero = false; break; }
            if (zero) {
                memcpy(b, r.init, r.initLen);
                ++seeded_;
            } else if (memcmp(b, r.init, r.initLen) != 0) {
                return kSeedMismatch;
            } else {
                ++seeded_;   // the class's own initialiser reproduced the seed
            }
        }
        entries_[count_].row  = row;
        entries_[count_].cell = cell;
        ++count_;
        return kOk;
    }

    // The redirected cell for row index `row`, or nullptr.
    void* CellFor(int row) const {
        for (int i = 0; i < count_; ++i)
            if (entries_[i].row == row) return entries_[i].cell;
        return nullptr;
    }

    int Count()  const { return count_; }
    int Seeded() const { return seeded_; }   // rows whose kInit seed the cell holds

private:
    struct Entry { int row; void* cell; };
    Entry entries_[Capacity] = {};
    int   count_  = 0;
    int   seeded_ = 0;
};

} // namespace RtMintRedirect
