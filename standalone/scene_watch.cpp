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

// ---------------------------------------------------------------------------
// SceneWatch_Census — see scene_watch.h for what this measures and why.
// ---------------------------------------------------------------------------
namespace {

bool CensusEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_SCENE_CENSUS");
        on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return on == 1;
}

struct GeoPos { float x, y, z; int cell; long frame; };

// Distance a walker can cover in one frame is small; anything past this is a
// teleport, which is what "the model popped somewhere else" actually is.
constexpr float kJumpDist = 2.0f;

} // namespace

void SceneWatch_Census(void* scene) {
    if (!CensusEnabled() || !scene) return;

    static long s_frame = 0;
    ++s_frame;

    const uintptr_t s = reinterpret_cast<uintptr_t>(scene);
    uint8_t* nodes = *reinterpret_cast<uint8_t**>(s + 0x10);
    const int count = *reinterpret_cast<int*>(s + 0x18);
    if (!nodes || count <= 0 || count > (1 << 22)) return;

    static std::unordered_map<void*, GeoPos> s_last;

    int live = 0, moved = 0, jumped = 0, added = 0, staleprev = 0;
    int degenerate = 0, zerobox = 0, posOutside = 0, noRefresh = 0;
    int dirty = 0, layer0 = 0, rebucketed = 0;
    long long detailSum = 0;
    void* jumpExample = nullptr;
    float jx = 0, jy = 0, jz = 0, px = 0, py = 0, pz = 0;
    void* staleExample = nullptr;
    float sx = 0, sy = 0, sz = 0;
    float sbx0 = 0, sby0 = 0, sbz0 = 0, sbx1 = 0, sby1 = 0, sbz1 = 0;

    for (int i = 0; i < count; ++i) {
        uint8_t* node = nodes + static_cast<size_t>(i) * 0x18;
        if (*reinterpret_cast<int*>(node + 0x10) < 0) continue;   // free slot
        void* obj = *reinterpret_cast<void**>(node);
        if (!Readable(obj)) continue;
        const uintptr_t g = reinterpret_cast<uintptr_t>(obj);
        const int cell = *reinterpret_cast<int*>(node + 0x10);
        ++live;

        // Graphics::Scene::Clean (0x1801023f0) is the per-frame refresh: for every live
        // node whose DIRTY word at geo+0x2c is non-zero it dispatches the geometry's
        // vtable slot 8 (+0x40, "recompute my bounds; tell me if they moved") and, when
        // that returns true, re-buckets the node through FUN_18005ee30. node+0x10 IS
        // that bucket — a Morton cell index — so counting how often it changes measures
        // whether the refresh actually runs. A world where things move but nothing is
        // ever re-bucketed culls them against the cell they were FIRST inserted into,
        // which on screen is a model that pops in and out as the view changes.
        if (*reinterpret_cast<int32_t*>(g + 0x2c) != 0) ++dirty;
        if (*reinterpret_cast<uint32_t*>(g + 0x30) == 0) ++layer0;

        const float x = *reinterpret_cast<float*>(g + 0x94);
        const float y = *reinterpret_cast<float*>(g + 0x98);
        const float z = *reinterpret_cast<float*>(g + 0x9c);
        // Previous transform the renderer interpolates FROM (+0xB0..+0xB8).
        const float ox = *reinterpret_cast<float*>(g + 0xb0);
        const float oy = *reinterpret_cast<float*>(g + 0xb4);
        const float oz = *reinterpret_cast<float*>(g + 0xb8);
        const float pdx = x - ox, pdy = y - oy, pdz = z - oz;
        if (pdx * pdx + pdy * pdy + pdz * pdz > kJumpDist * kJumpDist) ++staleprev;

        // --- CULL INPUTS ------------------------------------------------------
        // Graphics::Scene::AddCulledDrawItems (Runtime 0x1801017a0) decides whether a
        // node is drawn from exactly three things on the Geometry:
        //   +0x30 layer mask   & the render pass mask
        //   +0x28 detail level <= renderer+0x48
        //   +0x4c..+0x60 world AABB (min xyz / max xyz) vs Volume::Frustum::Contains
        // A geometry whose AABB no longer contains its own origin is carrying a STALE
        // box: the frustum test then answers about where the model USED to be, so it
        // vanishes and reappears as the camera moves. That is the shape of the "models
        // keep popping" report, and it is invisible to the membership and position
        // counts above because both of those stay perfectly stable.
        //
        // +0x20 is the counter Graphics::Geometry::SetTransform (0x1800155a0) tests
        // before dispatching vtable slot 4 — the box refresh. Zero there means the box
        // is never recomputed when the transform changes, which is one way to get a
        // stale box, so count it separately from the symptom.
        const float bx0 = *reinterpret_cast<float*>(g + 0x4c);
        const float by0 = *reinterpret_cast<float*>(g + 0x50);
        const float bz0 = *reinterpret_cast<float*>(g + 0x54);
        const float bx1 = *reinterpret_cast<float*>(g + 0x58);
        const float by1 = *reinterpret_cast<float*>(g + 0x5c);
        const float bz1 = *reinterpret_cast<float*>(g + 0x60);
        if (bx1 < bx0 || by1 < by0 || bz1 < bz0) {
            ++degenerate;
        } else if (bx0 == 0.0f && by0 == 0.0f && bz0 == 0.0f &&
                   bx1 == 0.0f && by1 == 0.0f && bz1 == 0.0f) {
            ++zerobox;
        } else {
            // Slack, for meshes whose origin sits on an edge of their own bounds.
            const float k = 0.5f;
            if (x < bx0 - k || x > bx1 + k || y < by0 - k || y > by1 + k ||
                z < bz0 - k || z > bz1 + k) {
                ++posOutside;
                if (!staleExample) {
                    staleExample = obj;
                    sx = x; sy = y; sz = z;
                    sbx0 = bx0; sby0 = by0; sbz0 = bz0;
                    sbx1 = bx1; sby1 = by1; sbz1 = bz1;
                }
            }
        }
        detailSum += *reinterpret_cast<int32_t*>(g + 0x28);
        if (*reinterpret_cast<int32_t*>(g + 0x20) == 0) ++noRefresh;

        auto it = s_last.find(obj);
        if (it == s_last.end()) {
            ++added;
            s_last.emplace(obj, GeoPos{x, y, z, cell, s_frame});
            continue;
        }
        GeoPos& p = it->second;
        if (p.cell != cell) { ++rebucketed; p.cell = cell; }
        if (p.x != x || p.y != y || p.z != z) {
            ++moved;
            const float dx = x - p.x, dy = y - p.y, dz = z - p.z;
            if (dx * dx + dy * dy + dz * dz > kJumpDist * kJumpDist) {
                ++jumped;
                if (!jumpExample) {
                    jumpExample = obj;
                    jx = x; jy = y; jz = z; px = p.x; py = p.y; pz = p.z;
                }
            }
            p.x = x; p.y = y; p.z = z;
        }
        p.frame = s_frame;
    }

    // Anything we tracked that no longer appears in a live node has LEFT the scene.
    int removed = 0;
    for (auto it = s_last.begin(); it != s_last.end(); ) {
        if (it->second.frame != s_frame) { it = s_last.erase(it); ++removed; }
        else ++it;
    }

    // Report every second, and immediately on any teleport — a jump is the whole
    // point of the probe and rate-limiting it away would hide the event.
    static int s_jumpReports = 0;
    const bool due = (s_frame % 60) == 0;
    if (jumped > 0 && s_jumpReports < 40) {
        ++s_jumpReports;
        std::fprintf(stderr,
            "[SceneCensus] frame=%ld JUMPED=%d e.g. geo=%p (%.2f,%.2f,%.2f) -> "
            "(%.2f,%.2f,%.2f)\n",
            s_frame, jumped, jumpExample, px, py, pz, jx, jy, jz);
        std::fflush(stderr);
    }
    if (due) {
        std::fprintf(stderr,
            "[SceneCensus] frame=%ld nodes=%d live=%d added=%d removed=%d moved=%d "
            "jumped=%d staleprev=%d | bbox degenerate=%d zero=%d posOutside=%d "
            "noRefresh(+0x20==0)=%d meanDetail=%.2f | dirty=%d layer0=%d rebucketed=%d\n",
            s_frame, count, live, added, removed, moved, jumped, staleprev,
            degenerate, zerobox, posOutside, noRefresh,
            live ? (double)detailSum / (double)live : 0.0,
            dirty, layer0, rebucketed);
        if (staleExample)
            std::fprintf(stderr,
                "[SceneCensus]   stale-box e.g. geo=%p pos=(%.2f,%.2f,%.2f) "
                "box=(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f)\n",
                staleExample, sx, sy, sz, sbx0, sby0, sbz0, sbx1, sby1, sbz1);
        std::fflush(stderr);
    }
}
