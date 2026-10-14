// ============================================================================
// src/engine/object/expand_memo.cpp -- the ExpandRelativePath memo
//                                       (block SAVELOAD, NOT RETAIL, exact)
// ============================================================================
// The proof is in expand_memo.h. Plain std containers on the CRT heap: the
// memo never hands out or holds a game-allocator String.
// ============================================================================

#include "object/expand_memo.h"

#include <cstring>
#include <unordered_map>

namespace RtObject {

namespace {

// More distinct requests than this (a whole game asks for ~20k) only means
// something is generating names; drop everything rather than grow.
constexpr size_t kMaxEntries = 1u << 16;

struct Memo {
    // The context the entries were made under.
    const void* fs = nullptr;
    bool        hasDataDir = false;
    std::string dataDir;
    uint32_t    generation = 0;
    bool        valid = false;   // false until the first context is recorded

    std::unordered_map<std::string, std::string> entries;
    std::string key;             // reused scratch: no allocation per lookup
};

// Never destroyed: a loader thread still inside FindOrLoad at process exit must
// not meet a destructed map.
Memo& State() {
    static Memo* const m = new Memo;
    return *m;
}

bool SameContext(const Memo& m, const void* fs, const char* dataDir, uint32_t generation) {
    if (!m.valid || m.fs != fs || m.generation != generation)
        return false;
    if ((dataDir != nullptr) != m.hasDataDir)
        return false;
    return dataDir == nullptr || std::strcmp(dataDir, m.dataDir.c_str()) == 0;
}

// Make the stored context this one, dropping every entry when it changes.
void UseContext(Memo& m, const void* fs, const char* dataDir, uint32_t generation) {
    if (SameContext(m, fs, dataDir, generation))
        return;
    m.entries.clear();
    m.fs = fs;
    m.hasDataDir = (dataDir != nullptr);
    m.dataDir.assign(dataDir != nullptr ? dataDir : "");
    m.generation = generation;
    m.valid = true;
}

void BuildKey(Memo& m, const char* path, int len, bool appendIdentifier) {
    m.key.assign(path, static_cast<size_t>(len));
    m.key.push_back(appendIdentifier ? '\1' : '\0');
}

}  // namespace

const std::string* ExpandMemoFind(const char* path, int len, bool appendIdentifier,
                                  const void* fs, const char* dataDir, uint32_t generation) {
    Memo& m = State();
    UseContext(m, fs, dataDir, generation);
    if (m.entries.empty())
        return nullptr;
    BuildKey(m, path, len, appendIdentifier);
    auto it = m.entries.find(m.key);
    return it == m.entries.end() ? nullptr : &it->second;
}

void ExpandMemoStore(const char* path, int len, bool appendIdentifier,
                     const void* fs, const char* dataDir, uint32_t generation,
                     const char* result, int resultLen) {
    Memo& m = State();
    UseContext(m, fs, dataDir, generation);
    if (m.entries.size() >= kMaxEntries)
        m.entries.clear();
    BuildKey(m, path, len, appendIdentifier);
    m.entries[m.key].assign(result, static_cast<size_t>(resultLen));
}

size_t ExpandMemoSize() {
    return State().entries.size();
}

void ExpandMemoClear() {
    Memo& m = State();
    m.entries.clear();
    m.valid = false;
}

}  // namespace RtObject
