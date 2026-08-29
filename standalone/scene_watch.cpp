// standalone/scene_watch.cpp — see scene_watch.h for what this measures and why.
//
// Scene layout used here (Runtime Graphics::Scene, read-only):
//   +0x10  Node*  nodes        (0x18 bytes per node)
//   +0x18  int    count
//   node+0x00  Geometry*  obj
//   node+0x10  int        slot state; < 0 means the slot is free
// Confirmed against Graphics::Scene::Clean 0x1801023f0 and AddObject 0x1801026a0.

#include "scene_watch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace {

bool Enabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_SCENE_WATCH");
        on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return on == 1;
}

struct Note {
    void*       geometry;
    const char* who;
    long        frame;
};

std::unordered_map<uint32_t, Note>& Notes() {
    static std::unordered_map<uint32_t, Note> m;
    return m;
}

// A real vtable lives in a mapped image (our exe's .rdata or a DLL's). A vtable
// pointer in MEM_PRIVATE memory means the object was freed and the block reused.
bool VtableLooksLikeCode(void* vt) {
    static std::unordered_set<uintptr_t> good, bad;
    const uintptr_t v = reinterpret_cast<uintptr_t>(vt);
    if (v < 0x10000 || (v & 7) != 0) return false;
    if (good.count(v)) return true;
    if (bad.count(v))  return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(vt, &mbi, sizeof(mbi)) != sizeof(mbi)) { bad.insert(v); return false; }
    const bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE;
    (ok ? good : bad).insert(v);
    return ok;
}

bool Readable(void* p) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    return a >= 0x10000 && a < 0x00007FFFFFFFFFFFULL && (a & 7) == 0;
}

} // namespace

void SceneWatch_Note(uint32_t sceneId, void* geometry, const char* who) {
    if (!Enabled()) return;
    Notes()[sceneId] = Note{geometry, who, 0};
}

void SceneWatch_Check(void* scene) {
    if (!Enabled() || !scene) return;

    static long   s_frame  = 0;
    static int    s_report = 0;
    static bool   s_seen   = false;
    ++s_frame;

    const uintptr_t s = reinterpret_cast<uintptr_t>(scene);
    uint8_t* nodes = *reinterpret_cast<uint8_t**>(s + 0x10);
    const int count = *reinterpret_cast<int*>(s + 0x18);
    if (!nodes || count <= 0 || count > (1 << 22)) return;

    int bad = 0, firstBad = -1;
    void* firstObj = nullptr;
    void* firstVt  = nullptr;

    for (int i = 0; i < count; ++i) {
        uint8_t* node = nodes + static_cast<size_t>(i) * 0x18;
        if (*reinterpret_cast<int*>(node + 0x10) < 0) continue;   // free slot
        void* obj = *reinterpret_cast<void**>(node);
        if (!Readable(obj)) {
            ++bad;
            if (firstBad < 0) { firstBad = i; firstObj = obj; firstVt = nullptr; }
            continue;
        }
        void* vt = *reinterpret_cast<void**>(obj);
        if (VtableLooksLikeCode(vt)) continue;
        ++bad;
        if (firstBad < 0) { firstBad = i; firstObj = obj; firstVt = vt; }
    }

    if (bad == 0) return;
    if (s_report >= 12) return;
    ++s_report;

    const bool firstEver = !s_seen;
    s_seen = true;

    std::fprintf(stderr,
        "[SceneWatch]%s frame=%ld nodes=%d BAD=%d firstBad=index %d obj=%p vtable=%p\n",
        firstEver ? " FIRST" : "", s_frame, count, bad, firstBad, firstObj, firstVt);

    auto it = Notes().find(static_cast<uint32_t>(firstBad));
    if (it != Notes().end())
        std::fprintf(stderr, "[SceneWatch]   id %d was last registered by %s "
                             "(geometry=%p)%s\n",
                     firstBad, it->second.who, it->second.geometry,
                     it->second.geometry == firstObj ? "  <== SAME OBJECT" : "  (different object — id was reused)");
    else
        std::fprintf(stderr, "[SceneWatch]   id %d has no record from our AddObject "
                             "sites — the engine or an unported path registered it\n",
                     firstBad);

    // The freed block often still carries recognisable remnants (the old vtable in
    // an adjacent field, the owner IBP, our factory flag word).
    if (Readable(firstObj)) {
        std::fprintf(stderr, "[SceneWatch]   obj dump:");
        for (int q = 0; q < 8; ++q)
            std::fprintf(stderr, " %016llx",
                         (unsigned long long)*reinterpret_cast<uint64_t*>(
                             reinterpret_cast<uintptr_t>(firstObj) + q * 8));
        std::fprintf(stderr, "\n");
        // Geometry is embedded at owner+0x28 on every drawable component in this
        // port, so dump the presumed owner header too (vtable, flags, owner IBP).
        uintptr_t owner = reinterpret_cast<uintptr_t>(firstObj) - 0x28;
        if (Readable(reinterpret_cast<void*>(owner))) {
            std::fprintf(stderr, "[SceneWatch]   owner(obj-0x28) dump:");
            for (int q = 0; q < 6; ++q)
                std::fprintf(stderr, " %016llx",
                             (unsigned long long)*reinterpret_cast<uint64_t*>(owner + q * 8));
            std::fprintf(stderr, "\n");
        }
    }
    std::fflush(stderr);
}
