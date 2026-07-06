// code/Game/Game/citizen_names.cpp
//
// Faithful port of the citizen name generator that the retail deferred citizen
// init (FUN_140117080) runs to fill CitizenComponent+0x28 (the name String):
//
//   FUN_1400a4c60(retbuf, rng, femaleFlag)   -- pick Male.ndb / Female.ndb, load it
//     -> FUN_1400a4b30(retbuf, db, rng, 3, 10)  -- retry generate until len >= 3,
//                                                   rejecting names in the db reject list
//        -> FUN_1400a4880(db, rng, 3, 10, buf)   -- 4-gram Markov walk over the .ndb
//
// The whole retail init is guarded off in standalone (it dereferences housing /
// animation sub-objects that are NULL), so founding colonists never get a name and
// the CitizenUI dialog's textName is skipped (CitUI_OkString rejects the garbage
// String at +0x28). This reproduces just the name-generation slice and constructs a
// real System::String directly at the citizen's +0x28, so the dialog shows a name.
//
// The .ndb name database is an Object::External. Field layout (from FUN_1400a4880 /
// FUN_1400a4b30 disassembly):
//   +0x20  record table base (12 bytes per record)
//   +0x28  record count (int)
//   +0x30  start-index array base (uint16 per entry -> index into record table)
//   +0x38  start count (uint)
//   +0x40  reject-list string pool base (C-strings)
//   +0x50  reject-list offset array (int per entry -> offset into +0x40 pool)
//   +0x58  reject-list count (int)
// Record (12 bytes):
//   [0..3]  4-char context key (compared big-endian; the sorted key of the table)
//   [4]     word-boundary flag (nonzero => this transition can end the name)
//   [8..11] bitmask of possible next chars (bits 0..25 = 'a'..'z', bit 26 = end)

#include "engine_funcs.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// System::Random::Rand(int min, int max)  (Runtime 0x1800c7d70) returns
//   min + Rand() % ((max - min) + 1)   -- inclusive [min, max].
// Built from the (self, N) overload we already resolve (fn_Random_Rand = Rand()%N).
static inline int RandRange(void* rng, int lo, int hi) {
    if (hi < lo) return lo;
    return lo + (int)fn_Random_Rand(rng, (uint32_t)(hi - lo + 1));
}

// Faithful port of FUN_1400a4880: one 4-gram Markov walk. Writes a C-string into
// `out` (caller supplies a >=0x100 byte buffer, memset to 0). No allocation.
static void MarkovGenerate(uintptr_t db, void* rng, int minLen, int maxLen, char* out) {
    uintptr_t table    = *(uintptr_t*)(db + 0x20);
    int       recCount = *(int*)(db + 0x28);
    uintptr_t startArr = *(uintptr_t*)(db + 0x30);
    uint32_t  startCnt = *(uint32_t*)(db + 0x38);
    if (!table || recCount <= 0 || !startArr || startCnt == 0) { out[0] = 0; return; }

    // --- Seed: pick a random starting 4-gram and copy it into out[0..3] ---
    uint32_t s = fn_Random_Rand(rng, startCnt);
    uint16_t startIdx = *(uint16_t*)(startArr + (uintptr_t)(int)s * 2);
    if ((int)startIdx >= recCount) { out[0] = 0; return; }
    uint32_t seedKey = *(uint32_t*)(table + (uintptr_t)startIdx * 0xc);
    *(uint32_t*)out = seedKey;
    out[0] = (char)(out[0] - 0x20);   // uppercase the first letter ('a'-0x20 = 'A')

    // Context window = the 4 seed bytes, in memory (little-endian) order.
    unsigned char c0 = (unsigned char)(seedKey);
    unsigned char c1 = (unsigned char)(seedKey >> 8);
    unsigned char c2 = (unsigned char)(seedKey >> 16);
    unsigned char cCur = (unsigned char)(seedKey >> 24);

    int  targetLen  = RandRange(rng, minLen, maxLen - 4);
    long loopBound  = (long)maxLen * 2 - 3;
    if (loopBound <= 0) return;

    long charCount   = 0;   // chars appended past the seed
    int  outIdx      = 0;
    int  lastBoundary = -1;

    do {
        // Context key packed big-endian (the table's sort order).
        uint32_t ctxKey = ((uint32_t)c0 << 24) | ((uint32_t)c1 << 16)
                        | ((uint32_t)c2 << 8) | (uint32_t)cCur;

        // Binary search the record table for the record whose key == ctxKey.
        int lo = 0, hi = recCount, foundIdx = -1;
        for (;;) {
            int mid = (hi + lo) >> 1;
            if (mid < 0 || mid >= recCount) return;   // context not found -> stop
            const unsigned char* rp = (const unsigned char*)(table + (uintptr_t)mid * 0xc);
            uint32_t rkey = ((uint32_t)rp[0] << 24) | ((uint32_t)rp[1] << 16)
                          | ((uint32_t)rp[2] << 8) | (uint32_t)rp[3];
            if (rkey == ctxKey) { foundIdx = mid; break; }
            if (rkey < ctxKey) lo = mid + 1; else hi = mid - 1;
            if (hi < lo) return;                       // not found -> stop
        }

        uintptr_t rec = table + (uintptr_t)foundIdx * 0xc;
        uint32_t recKey  = *(uint32_t*)rec;
        uint32_t mask    = *(uint32_t*)(rec + 8);
        // Slide the context window: drop the oldest byte (recKey byte0), keep 1..3.
        c0 = (unsigned char)(recKey >> 8);
        c1 = (unsigned char)(recKey >> 16);
        c2 = (unsigned char)(recKey >> 24);
        if (mask == 0) return;                         // no continuations
        unsigned char flag = (unsigned char)*(uint32_t*)(rec + 4);
        if (maxLen <= charCount && flag != 0) return;  // too long, at a word boundary
        if (flag != 0) lastBoundary = outIdx + 4;

        // Pick a uniformly random set bit among the 27 possible next-chars. The
        // original counts bits in three interleaved groups; the sum is the popcount
        // of the low 27 bits, so a single popcount + one Rand call is faithful.
        int total = __builtin_popcount(mask & 0x07FFFFFFu);
        if (total <= 0) return;
        uint32_t pick = fn_Random_Rand(rng, (uint32_t)total);
        int chosenBit = 0;
        uint32_t seen = 0;
        for (int bit = 0; bit < 0x1b; ++bit) {
            if ((mask >> bit) & 1u) {
                chosenBit = bit;
                if (seen == pick) break;
                ++seen;
            }
        }

        // Stop if we've reached the target length and 'end' is allowed, or the
        // chosen transition IS the end marker (bit 0x1a).
        if (((targetLen <= charCount) && ((mask >> 0x1a) & 1u)) || chosenBit == 0x1a) {
            out[outIdx + 4] = 0;
            return;
        }

        char nc = (char)(chosenBit + 'a');
        out[charCount + 4] = nc;
        cCur = (unsigned char)nc;                      // new current char feeds next context
        ++charCount;
        ++outIdx;
    } while (charCount < loopBound);

    if (lastBoundary > 0) out[lastBoundary] = 0;       // truncate at last word boundary
}

// Faithful port of FUN_1400a4b30: retry the generator up to 50 times until it
// yields a name of at least `minLen`, rejecting any candidate that appears in the
// db's reject list (a sorted C-string table at +0x40 / +0x50 / +0x58). Writes the
// accepted C-string into `out` (>=0x100 bytes).
static void GenerateName(uintptr_t db, void* rng, int minLen, int maxLen, char* out) {
    uintptr_t pool  = *(uintptr_t*)(db + 0x40);
    uintptr_t offs  = *(uintptr_t*)(db + 0x50);
    int       rejN  = *(int*)(db + 0x58);

    for (int attempt = 0; attempt < 0x32; ++attempt) {
        std::memset(out, 0, 0x100);
        MarkovGenerate(db, rng, minLen, maxLen, out);
        int len = (int)std::strlen(out);
        if (len < minLen) continue;                    // too short -> regenerate

        // Reject if the generated name is present in the reject list. A missing name
        // (search exhausts) is accepted. Empty list -> accept immediately.
        bool rejected = false;
        if (pool && offs && rejN > 0) {
            int lo = 0, hi = rejN;
            while (true) {
                int mid = (lo + hi) >> 1;
                if (mid < 0 || mid >= rejN) break;     // not found -> accept
                const char* cand = (const char*)(pool + (uintptr_t)*(int*)(offs + (uintptr_t)mid * 4));
                int cmp = std::strcmp(out, cand);
                if (cmp == 0) { rejected = true; break; }
                if (cmp < 1) hi = mid - 1; else lo = mid + 1;
                if (hi < lo) break;                    // not found -> accept
            }
        }
        if (!rejected) return;                         // accepted
    }
    // 50 failed attempts: keep whatever the last generate produced (retail leaves
    // the buffer as-is and constructs from it).
}

// ---- .ndb load + gender selection (FUN_1400a4c60) ---------------------------
// Loaded name databases are cached (one Male, one Female) so we don't FindOrLoad
// per colonist. FindOrLoad by exact filename is authoritative, so we skip the
// IsDerivedFrom type check (its type-id .rdata is NULL in standalone anyway).
static uintptr_t LoadNameDb(bool female) {
    static uintptr_t s_male = 0, s_female = 0;
    static bool s_maleTried = false, s_femaleTried = false;
    uintptr_t* slot   = female ? &s_female : &s_male;
    bool*      tried  = female ? &s_femaleTried : &s_maleTried;
    if (*tried) return *slot;
    *tried = true;

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_Filename_CtorCStr || !fn_ExternalFactory_FindOrLoad || !fn_Filename_Dtor)
        return 0;

    uint8_t fname[8] = {0};
    fn_Filename_CtorCStr(fname, female ? "Game/Names/Female.ndb" : "Game/Names/Male.ndb");
    void* obj = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fname);
    fn_Filename_Dtor(fname);
    *slot = (uintptr_t)obj;
    return *slot;
}

// Public entry: generate a citizen name for `female` into `out` (>=0x100 bytes,
// caller owns). Returns true if a name of length >= minLen was produced. Faithful
// minLen=3 / maxLen=10 (retail FUN_1400a4c60 passes 3, 10).
extern "C" bool GenerateCitizenName(void* rng, bool female, char* out, int cap) {
    if (!out || cap < 0x100 || !rng || !fn_Random_Rand) return false;
    uintptr_t db = LoadNameDb(female);
    if (!db) { out[0] = 0; return false; }
    GenerateName(db, rng, 3, 10, out);
    return std::strlen(out) >= 3;
}
