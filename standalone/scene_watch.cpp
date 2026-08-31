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
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "resolve.h"
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

// ---------------------------------------------------------------------------
// BANISHED_LOD_CENSUS — see scene_watch.h for the mechanism this measures.
// ---------------------------------------------------------------------------

namespace {

bool ForceLod0() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_FORCE_LOD0");
        on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return on == 1;
}

bool LodEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_LOD_CENSUS");
        on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    // BANISHED_FORCE_LOD0 needs the same per-frame walk to do its clamping, so it
    // implies the census; the numbers it prints are wanted for that A/B anyway.
    return on == 1 || ForceLod0();
}

// Only Graphics::MeshInstance runs the distance pick. Identify one by its
// AddDrawItems slot (vtable index 3, +0x18) rather than by the class vtable, so
// the test still holds for any subclass that inherits the retail implementation.
bool IsMeshInstance(void* obj) {
    static uintptr_t s_addDrawItems = 0;
    if (s_addDrawItems == 0) s_addDrawItems = ResolveAddr(Module::Runtime, 0x180100f20);
    if (!s_addDrawItems) return false;
    void** vt = *reinterpret_cast<void***>(obj);
    if (!VtableLooksLikeCode(vt)) return false;
    return reinterpret_cast<uintptr_t>(vt[3]) == s_addDrawItems;
}

struct MeshInfo {
    int   levels;
    int   instances;
    float dist[8];
    int   tris[8];    // level+0xa0: TriangleList count. ZERO means the level draws NOTHING.
    void* vb[8];      // level+0x20: vertex buffer object
    void* ib[8];      // level+0x40: index buffer object
    bool  dumped;
};

} // namespace

void SceneWatch_LodCensus(void* scene, const void* camera, int variantIdx) {
    if (!LodEnabled() || !scene || !camera) return;

    static long s_frame = 0;
    ++s_frame;

    const uintptr_t s = reinterpret_cast<uintptr_t>(scene);
    uint8_t* nodes = *reinterpret_cast<uint8_t**>(s + 0x10);
    const int count = *reinterpret_cast<int*>(s + 0x18);
    if (!nodes || count <= 0 || count > (1 << 22)) return;

    const uintptr_t c = reinterpret_cast<uintptr_t>(camera);
    const float camX = *reinterpret_cast<const float*>(c + 0x88);
    const float camY = *reinterpret_cast<const float*>(c + 0x8c);
    const float camZ = *reinterpret_cast<const float*>(c + 0x90);

    static std::unordered_map<void*, int>      s_lod;    // geometry -> last chosen level
    static std::unordered_map<void*, MeshInfo> s_meshes; // GraphicsMesh -> its table

    int instances = 0, multiLevel = 0, flips = 0, fellBack = 0, prevBoxEmpty = 0;
    int variantHist[9] = {0};
    int variantMiss = 0, variantNotReady = 0;
    int lodHist[8] = {0};
    void* flipExample = nullptr;
    int   flipFrom = 0, flipTo = 0;
    float flipDist = 0.0f;

    for (auto& kv : s_meshes) kv.second.instances = 0;

    for (int i = 0; i < count; ++i) {
        uint8_t* node = nodes + static_cast<size_t>(i) * 0x18;
        if (*reinterpret_cast<int*>(node + 0x10) < 0) continue;   // free slot
        void* obj = *reinterpret_cast<void**>(node);
        if (!Readable(obj) || !IsMeshInstance(obj)) continue;
        const uintptr_t g = reinterpret_cast<uintptr_t>(obj);

        void* mesh = *reinterpret_cast<void**>(g + 0xc0);
        if (!Readable(mesh)) continue;
        ++instances;

        const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
        // BANISHED_FORCE_LOD0=1 — pin every mesh to its highest detail level.
        // Graphics::MeshInstance::AddDrawItems only runs the distance pick when
        // GraphicsMesh+0x58 (the level count) exceeds 1, so clamping it to 1 makes
        // level 0 draw at every distance. The levels array itself is untouched.
        // This is the A/B for "is the detail swap what I am seeing pop?": with it on,
        // the swap cannot happen, so anything still popping is NOT the LOD.
        if (ForceLod0() && *reinterpret_cast<int*>(m + 0x58) > 1)
            *reinterpret_cast<int*>(m + 0x58) = 1;
        const int n = *reinterpret_cast<int*>(m + 0x58);
        uint8_t* levels = *reinterpret_cast<uint8_t**>(m + 0x50);

        MeshInfo& info = s_meshes[mesh];
        if (info.levels == 0) {
            info.levels = n;
            info.dumped = false;
            for (int k = 0; k < 8; ++k) {
                info.dist[k] = 0.0f; info.tris[k] = -1; info.vb[k] = nullptr; info.ib[k] = nullptr;
                if (k >= n || !Readable(levels)) continue;
                uint8_t* lv = levels + static_cast<size_t>(k) * 0xb0;
                info.dist[k] = *reinterpret_cast<float*>(lv + 0x10);
                // Graphics::MeshInstance::AddDrawItems emits one draw item per entry of
                // level+0xa0 — a level whose count is zero emits NOTHING, so the model just
                // stops being rendered past that level's distance. On screen that is exactly
                // "the model popped out", and it is invisible to every cull, transform and
                // membership measurement, all of which stay perfectly healthy.
                info.tris[k] = *reinterpret_cast<int*>(lv + 0xa0);
                info.vb[k]   = *reinterpret_cast<void**>(lv + 0x20);
                info.ib[k]   = *reinterpret_cast<void**>(lv + 0x40);
            }
        }
        ++info.instances;

        // Graphics::Renderer::AddDrawItem (Runtime 0x1801057b0) silently DROPS an item
        // whose material has no shader variant at the group's variant index:
        //     idx = groupState+0x1b8   (BeginDrawGroup's param_4)
        //     mat = matInstance+0x28;  requires 0 <= idx < *(int*)(mat+0x30)
        //     and the variant's +0x20/+0x28/+0x30 programs must all be non-null
        //     with a positive size at +0x48.
        // For the main scene pass that index is renderer+0x3c, which THIS PORT writes
        // every frame from VideoConfig+0x30 (the shadow-quality setting). A setting
        // past a material's variant count drops every draw item using that material,
        // with no error anywhere - the model is simply absent.
        {
            void** matInsts = *reinterpret_cast<void***>(g + 0xc8);
            if (Readable(matInsts)) {
                void* mi = matInsts[0];
                if (Readable(mi)) {
                    void* mat = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mi) + 0x28);
                    if (Readable(mat)) {
                        const uintptr_t mt = reinterpret_cast<uintptr_t>(mat);
                        const int variants = *reinterpret_cast<int*>(mt + 0x30);
                        if (variants >= 0 && variants < 8) ++variantHist[variants];
                        else                               ++variantHist[8];
                        if (variantIdx < 0 || variantIdx >= variants) {
                            ++variantMiss;
                        } else {
                            void** arr = *reinterpret_cast<void***>(mt + 0x28);
                            void*  v   = Readable(arr) ? arr[variantIdx] : nullptr;
                            if (!Readable(v)) {
                                ++variantNotReady;
                            } else {
                                const uintptr_t vv = reinterpret_cast<uintptr_t>(v);
                                void* p0 = *reinterpret_cast<void**>(vv + 0x20);
                                void* p1 = *reinterpret_cast<void**>(vv + 0x28);
                                void* p2 = *reinterpret_cast<void**>(vv + 0x30);
                                const bool ok =
                                    p0 && Readable(p1) && Readable(p2) &&
                                    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(p1) + 0x48) > 0 &&
                                    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(p2) + 0x48) > 0;
                                if (!ok) ++variantNotReady;
                            }
                        }
                    }
                }
            }
        }

        if (n <= 1 || !Readable(levels)) continue;
        ++multiLevel;

        // Retail union: current world box geo+0x4c..+0x60 with the previous one
        // geo+0x64..+0x78.
        const float* b = reinterpret_cast<const float*>(g + 0x4c);   // 12 floats
        const float minx = b[0] < b[6]  ? b[0] : b[6];
        const float miny = b[1] < b[7]  ? b[1] : b[7];
        const float minz = b[2] < b[8]  ? b[2] : b[8];
        const float maxx = b[3] > b[9]  ? b[3] : b[9];
        const float maxy = b[4] > b[10] ? b[4] : b[10];
        const float maxz = b[5] > b[11] ? b[5] : b[11];
        // The ctor seeds the previous box with the empty sentinel (+/-1e8), so a
        // geometry whose Clean never rolled it still unions to the current box.
        // Count them anyway: it tells us whether the roll is happening at all.
        if (b[6] > b[9]) ++prevBoxEmpty;

        const float dx = camX - (minx + maxx) * 0.5f;
        const float dy = camY - (miny + maxy) * 0.5f;
        const float dz = camZ - (minz + maxz) * 0.5f;
        const float d2 = dx * dx + dy * dy + dz * dz;

        int lod = 0;
        bool matched = false;
        for (int k = 0; k < n; ++k) {
            const float d = *reinterpret_cast<float*>(levels + static_cast<size_t>(k) * 0xb0 + 0x10);
            if (d2 < d * d) { lod = k; matched = true; break; }
        }
        if (!matched) ++fellBack;   // beyond every level: retail draws level 0
        if (lod >= 0 && lod < 8) ++lodHist[lod];

        auto it = s_lod.find(obj);
        if (it == s_lod.end()) {
            s_lod.emplace(obj, lod);
        } else if (it->second != lod) {
            ++flips;
            if (!flipExample) {
                flipExample = obj; flipFrom = it->second; flipTo = lod;
                flipDist = d2 > 0.0f ? std::sqrt(d2) : 0.0f;
            }
            it->second = lod;
        }
    }

    // One-time dump of every distinct mesh's distance table. A table that is not
    // strictly increasing, or that holds a zero/negative/absurd distance, sends the
    // pick to the wrong level at a distance that the camera crosses constantly.
    if (s_frame == 120) {
        for (auto& kv : s_meshes) {
            MeshInfo& info = kv.second;
            if (info.dumped) continue;
            info.dumped = true;
            char buf[512]; int off = 0;
            for (int k = 0; k < info.levels && k < 8; ++k)
                off += std::snprintf(buf + off, sizeof(buf) - off,
                                     "%sL%d{d=%.1f tris=%d vb=%p ib=%p}", k ? " " : "", k,
                                     info.dist[k], info.tris[k], info.vb[k], info.ib[k]);
            if (info.levels == 0) std::snprintf(buf, sizeof(buf), "-");
            bool sane = info.levels >= 1 && info.levels <= 8;
            for (int k = 0; sane && k < info.levels && k < 8; ++k) {
                if (!(info.dist[k] > 0.0f) || info.dist[k] > 1.0e6f) sane = false;
                if (k && info.dist[k] <= info.dist[k - 1])           sane = false;
            }
            bool empty = false;
            for (int k = 0; k < info.levels && k < 8; ++k)
                if (info.tris[k] <= 0 || !info.vb[k] || !info.ib[k]) empty = true;
            std::fprintf(stderr, "[LodCensus] mesh=%p levels=%d instances=%d %s%s%s\n",
                         kv.first, info.levels, info.instances, buf,
                         sane  ? "" : "   <== TABLE NOT MONOTONIC/POSITIVE",
                         empty ? "   <== A LEVEL DRAWS NOTHING" : "");
        }
        std::fflush(stderr);
    }

    static int s_flipReports = 0;
    if (flips > 0 && s_flipReports < 30) {
        ++s_flipReports;
        std::fprintf(stderr, "[LodCensus] frame=%ld FLIPS=%d e.g. geo=%p %d -> %d at dist=%.1f\n",
                     s_frame, flips, flipExample, flipFrom, flipTo, flipDist);
        std::fflush(stderr);
    }
    if ((s_frame % 60) == 0) {
        std::fprintf(stderr,
            "[LodCensus] frame=%ld meshInstances=%d multiLevel=%d meshes=%zu flips=%d "
            "fellBackToL0=%d prevBoxEmpty=%d | lod0=%d lod1=%d lod2=%d lod3=%d "
            "| variantIdx=%d MISS=%d notReady=%d variants[1..4]=%d/%d/%d/%d "
            "cam=(%.1f,%.1f,%.1f)\n",
            s_frame, instances, multiLevel, s_meshes.size(), flips, fellBack, prevBoxEmpty,
            lodHist[0], lodHist[1], lodHist[2], lodHist[3],
            variantIdx, variantMiss, variantNotReady,
            variantHist[1], variantHist[2], variantHist[3], variantHist[4],
            camX, camY, camZ);
        std::fflush(stderr);
    }
}

void SceneWatch_DrawItemCensus(void* renderer) {
    if (!LodEnabled() || !renderer) return;

    static long s_frame = 0;
    ++s_frame;

    const uintptr_t r = reinterpret_cast<uintptr_t>(renderer);
    uint8_t* groups = *reinterpret_cast<uint8_t**>(r + 0x10);
    const int used = *reinterpret_cast<int*>(r + 0x38);
    static int s_why = 0;
    if (!groups || used <= 0 || used > 64) {
        if (s_why < 3) {
            ++s_why;
            std::fprintf(stderr, "[DrawCensus] frame=%ld NO GROUPS: renderer=%p groups=%p "
                                 "used(+0x38)=%d cap(+0x18)=%d scenes(+0x8)=%d\n",
                         s_frame, renderer, groups, used,
                         *reinterpret_cast<int*>(r + 0x18), *reinterpret_cast<int*>(r + 8));
            std::fflush(stderr);
        }
        return;
    }

    static int s_groupDump = 0;
    if (s_frame == 120 || s_frame == 600) ++s_groupDump;
    int total = 0, mainGroup = 0;
    for (int g = 0; g < used; ++g) {
        uint8_t* st = groups + static_cast<size_t>(g) * 0x1f0;
        int sum = 0;
        for (int k = 0; k < 3; ++k)
            sum += *reinterpret_cast<int*>(st + 0x1c0 + static_cast<size_t>(k) * 0x10 + 8);
        total += sum;
        if (g == 0) mainGroup = sum;
        // groupState+0x1b8 is BeginDrawGroup's param_4 — the shader-VARIANT index that
        // Graphics::Renderer::AddDrawItem indexes each material's variant array with;
        // +0x1bc is the layer mask. Read them rather than infer them: Renderer::Draw
        // feeds BeginDrawGroup the scene entry's +0xec (AddScene's param_6), NOT
        // renderer+0x3c, so the obvious guess is wrong.
        if (s_groupDump < 2) {
            std::fprintf(stderr, "[DrawCensus] group %d/%d variantIdx(+0x1b8)=%d mask(+0x1bc)=0x%x "
                                 "items=%d\n",
                         g, used, *reinterpret_cast<int*>(st + 0x1b8),
                         *reinterpret_cast<unsigned*>(st + 0x1bc), sum);
        }
    }

    // A smoothly moving camera should change this count smoothly. A step change of
    // hundreds between consecutive frames IS the pop, and locating it in the group
    // breakdown says whether the shadow passes or the main pass dropped the objects.
    static int s_prev = -1;
    static int s_stepReports = 0;
    if (s_prev >= 0) {
        const int step = total > s_prev ? total - s_prev : s_prev - total;
        if (step > 100 && s_stepReports < 30) {
            ++s_stepReports;
            std::fprintf(stderr, "[DrawCensus] frame=%ld STEP %d -> %d (%+d) groups=%d\n",
                         s_frame, s_prev, total, total - s_prev, used);
            std::fflush(stderr);
        }
    }
    s_prev = total;
    if (s_groupDump && (s_frame == 120 || s_frame == 600)) std::fflush(stderr);

    if ((s_frame % 60) == 0) {
        std::fprintf(stderr, "[DrawCensus] frame=%ld drawItems=%d group0=%d groups=%d\n",
                     s_frame, total, mainGroup, used);
        std::fflush(stderr);
    }
}
