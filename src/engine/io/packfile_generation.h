#pragma once
// ============================================================================
// src/engine/io/packfile_generation.h -- the package index's change counter
//                                         (block SAVELOAD, NOT RETAIL)
// ============================================================================
//
// `IO::FileSystem::_packfileIndex` (the merged directory of every mounted .pkg)
// changes in exactly three places: `ReadPackfile`'s insert
// (filesystem_packages.cpp), `ClosePackages` (filesystem_packages.cpp) and
// `~FileSystem` (filesystem_createfile.cpp). Each bumps this counter after it
// has changed the tree. It is read by the ExpandRelativePath memo
// (src/engine/object/expand_memo.h), which drops every entry when the counter
// moved, so a memoised "found in a package" answer never outlives the package
// directory that gave it. Nothing else reads it; it changes no retail state.
// ============================================================================

#include <atomic>
#include <cstdint>

namespace IO {

inline std::atomic<uint32_t> g_packfileIndexGeneration{0};

inline void BumpPackfileIndexGeneration() {
    g_packfileIndexGeneration.fetch_add(1, std::memory_order_relaxed);
}

inline uint32_t PackfileIndexGeneration() {
    return g_packfileIndexGeneration.load(std::memory_order_relaxed);
}

}  // namespace IO
