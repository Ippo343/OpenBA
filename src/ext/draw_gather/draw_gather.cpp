// ============================================================================
// src/ext/draw_gather/draw_gather.cpp -- ext.draw_gather (class A): the bodies
// ============================================================================
// NOT RETAIL. draw_gather.h and gather_side.h have the contract;
// docs/extensions/handins/DRAWGATHER.md the design, the writers and the shadow.
//
// Two replaced bodies, reached through the null-by-default statics on
// Graphics::Scene (src/engine/graphics/scene_ext.h):
//
//   Clean        retail's Scene::Clean (scene.cpp, 0x1801023f0) line for line --
//                the same geometry cleans, the same GridReindex calls, the same
//                re-read of the node count -- plus, per live node, the side
//                record's upkeep (full refresh when Clean reported a change or
//                the record is not this geometry's; the two hints otherwise) and
//                a prefetch of the geometry kCleanAhead nodes on.
//   Gather       retail's AddCulledDrawItems (cull.cpp, 0x1801017a0): the same
//                quadtree walk (the CullStack below is a copy of cull.cpp's,
//                record for record), the same frusta, the same per-cell masks;
//                per object the test reads the side record first (NodeVisible),
//                then the same virtual AddDrawItems over the visible list in
//                its order, with a prefetch kDispatchAhead geometries on.
//
// The presentation is identical by construction (same list, same order, same
// virtual call per geometry, so the same draw items, sort keys, transforms and
// bone sets); the COMPPROF=4 shadows `DrawGather` (the list against retail's
// reads) and `DrawGatherSide` (each record used against the live box) check it.
// Nothing here is read by the sim or the pick scene (Picking::Scene is a
// different class), nothing is serialized.
// ============================================================================
#include "ext/draw_gather/draw_gather.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "rt_libm.h"
#include "math/Math.h"
#include "volume/Volume.h"
#include "volume/frustum_cull.h"   // CULLROUTE: the prepared tables
#include "graphics/Camera.h"
#include "graphics/Geometry.h"
#include "graphics/Scene.h"
#include "graphics/scene_ext.h"
#include "graphics/scene_grid.h"   // rtgfxgrid::GridReindex, Clean's re-index
#include "ext/registry.h"
#include "stress_harness.h"        // g_verifyOn / StressHarness_Verify (COMPPROF=4 shadows)

namespace DrawGather {

namespace {

using Graphics::Geometry;
using Graphics::Renderer;
using Graphics::RendererDrawGroup;
using Graphics::Scene;
using Graphics::SceneNode;

static_assert(sizeof(::Box) == sizeof(float) * 6, "a side record's box is a ::Box's bits");

// Prefetch distances (nodes / geometries ahead). Exact: a prefetch changes no value.
constexpr int32_t kCleanAhead    = 8;
constexpr int32_t kDispatchAhead = 4;

// ---------------------------------------------------------------------------
// The state. Main thread only (Clean and the gather run there), except the add
// log, which locks itself (scene_ext.h).
// ---------------------------------------------------------------------------
Graphics::SceneAddLog g_addLog;      // static storage: never freed, so a racing AddObject
                                     // on the loader thread never sees a dangling pointer
SceneSides            g_sides;
Counters              g_counters;
bool                  g_installed = false;

// Every id AddObject logged: that node's record is not to be trusted. An
// overflow forgets every record of every scene.
void DrainLog() {
    bool overflow = false;
    g_addLog.Drain(
        [](const Graphics::SceneAddLog::Entry& e) {
            ++g_counters.logged;
            if (SideArray* s = g_sides.Find(e.scene)) s->Invalidate(static_cast<int32_t>(e.id));
        },
        [&overflow](bool o) { overflow = o; });
    if (overflow) {
        g_sides.ForgetAll();
        ++g_counters.rebuilds;
    }
}

void StoreFull(SideArray& side, int32_t id, const Geometry* geo) {
    float box[6];
    std::memcpy(box, &geo->_worldBounds, sizeof(box));
    side.Store(id, geo, box, geo->_materialTypeFlags, geo->_detailId);
    ++g_counters.refreshes;
}

// ===========================================================================
// Clean -- retail's Scene::Clean (scene.cpp) plus the side upkeep.
// ===========================================================================
void ExtClean(Scene& scene) {
    using List = Collection::LinkedList<Geometry*>;

    DrainLog();
    SideArray& side = g_sides.Acquire(&scene);
    ++g_counters.cleans;

    int32_t count = scene._grid._nodes.GetSize();
    int32_t i = 0;
    if (count > 0) {
        while (i < count && scene._grid._nodes[i]._cell < 0) ++i;
    }
    if (i < count) {
        do {
            // The node array is dense; the Geometry it names is the miss.
            if (i + kCleanAhead < count) {
                const Geometry* ahead = scene._grid._nodes[i + kCleanAhead]._object;
                if (ahead != nullptr) {
                    __builtin_prefetch(ahead);                  // vptr, _detailId, _dirtyFlags
                    __builtin_prefetch(&ahead->_worldBounds);   // a box refresh, and the next line
                }
            }

            Geometry* geo = scene._grid._nodes[i]._object;
            bool changed = false;
            if (geo->_dirtyFlags != 0 && geo->Clean()) {        // VIRTUAL, slot 8 (retail's test)
                rtgfxgrid::GridReindex(&scene, i);              // = RtSceneGridReindex
                changed = true;
            }
            // Side upkeep. A box recompute always reports a change, so an unchanged
            // node's box is the one the record holds when the record is valid.
            if (UpkeepAfterClean(side, i, geo, changed,
                                 [geo](float* b) { std::memcpy(b, &geo->_worldBounds, sizeof(float) * 6); },
                                 geo->_materialTypeFlags, geo->_detailId))
                ++g_counters.refreshes;

            count = scene._grid._nodes.GetSize();
            ++i;
            if (count <= i) break;
            while (i < count && scene._grid._nodes[i]._cell < 0) ++i;
        } while (i < count);
    }

    for (List::Iterator it = scene._globals.GetFirst(); it != scene._globals.GetEnd(); ++it) {
        Geometry* geo = *it;
        if (geo->_dirtyFlags != 0)
            geo->Clean();                                       // VIRTUAL, slot 8
    }
}

// ===========================================================================
// The walk -- cull.cpp's CullStack and helpers, copied record for record (the
// faithful file keeps its own; FRUSTUMCULL is changing that body and a shared
// header is the follow-up once both are merged, DRAWGATHER.md §8).
// ===========================================================================
constexpr float kHalf         =  0.5f;   // 0x180161694
constexpr float kTwo          =  2.0f;   // 0x1801616d8
constexpr float kMaxZBias     =  8.0f;   // 0x1801616ec
constexpr float kShadowExtend = 11.0f;   // 0x1801616f4
constexpr float kMinusOne     = -1.0f;   // 0x18016175c
constexpr float kMinZBias     = -3.0f;   // 0x18016176c

struct CullRecord {
    float    x, y, z;
    float    halfX, halfY, halfZ;
    int32_t  node;
    int32_t  depth;
    uint32_t mask;          // shadowMask<<16 | cullMask
};

struct CullStack {
    int32_t      top;
    float        minBias[3];
    float        maxBias[3];
    CullRecord   rec[256];
    const Scene* scene;
};

void CullStackInit(CullStack* self, const Scene* scene, float minZBias, float maxZBias) {
    self->top        = -1;
    self->minBias[0] = 0.0f; self->minBias[1] = 0.0f; self->minBias[2] = minZBias;
    self->maxBias[0] = 0.0f; self->maxBias[1] = 0.0f; self->maxBias[2] = maxZBias;
    self->scene = scene;
    const float h = scene->_grid._cellSize * kHalf;
    self->top += 1;
    CullRecord& r = self->rec[self->top];
    r.x = 0.0f; r.y = 0.0f; r.z = 0.0f;
    r.halfX = h; r.halfY = h; r.halfZ = 0.0f;
    r.node  = 0;
    r.depth = 0;
    r.mask  = 0x003f003fu;
}

void CullStackPushChildren(CullStack* self, uint32_t mask) {
    const int32_t t = self->top;
    self->top = t - 1;
    const int32_t depth = self->rec[t].depth + 1;
    if (depth >= self->scene->_grid._levelCount) return;

    const float   x    = self->rec[t].x;
    const float   y    = self->rec[t].y;
    const float   z    = self->rec[t].z;
    const int32_t node = self->rec[t].node;
    float         hx   = self->rec[t].halfX;
    float         hy   = self->rec[t].halfY;
    const float   hz   = self->rec[t].halfZ;

    self->top = t;
    hx = hx * kHalf;
    hy = hy * kHalf;
    const float nhx = hx * kMinusOne;
    const float nhy = hy * kMinusOne;

    struct { float dx, dy; int32_t n; } kChild[4] = {
        { nhx, nhy, 1 }, { hx, nhy, 2 }, { nhx, hy, 3 }, { hx, hy, 4 },
    };
    for (int i = 0; i < 4; ++i) {
        CullRecord& c = self->rec[self->top];
        c.x = x + kChild[i].dx;
        c.y = y + kChild[i].dy;
        c.z = z + hz;
        c.halfX = hx;
        c.halfY = hy;
        c.halfZ = hz;
        c.node  = node * 4 + kChild[i].n;
        c.depth = depth;
        c.mask  = mask;
        if (i != 3) self->top += 1;
    }
}

// cull.cpp's two node boxes, with their two DIFFERENT max-X / max-Y groupings.
Box NodeBoxCull(const CullStack* st, const CullRecord& r) {
    Box b;
    b.min.x = st->minBias[0] + (r.x - r.halfX * kTwo);
    b.min.y = st->minBias[1] + (r.y - r.halfY * kTwo);
    b.min.z = st->minBias[2] + (r.z - r.halfZ);
    b.max.x = r.halfX * kTwo + r.x + st->maxBias[0];
    b.max.y = st->maxBias[1] + r.y + r.halfY * kTwo;
    b.max.z = st->maxBias[2] + r.z + r.halfZ;
    return b;
}
Box NodeBoxShadow(const CullStack* st, const CullRecord& r) {
    Box b = NodeBoxCull(st, r);
    b.max.x = r.x + r.halfX * kTwo + st->maxBias[0];
    b.max.y = r.y + r.halfY * kTwo + st->maxBias[1];
    return b;
}

// The shadow-caster box of an object: cull.cpp's arithmetic, term for term.
Box ShadowCasterBox(const Box& wb, float extX, float extY, float extZ) {
    const float cz = (wb.min.z + wb.max.z) * kHalf;
    const float cy = (wb.min.y + wb.max.y) * kHalf;
    const float cx = (wb.min.x + wb.max.x) * kHalf;
    const float hz = (wb.max.z - wb.min.z) * kHalf;
    const float hy = (wb.max.y - wb.min.y) * kHalf;
    const float hx = (wb.max.x - wb.min.x) * kHalf;
    const float rad = RtLibm::Sqrt((hy * hy + hx * hx) + hz * hz);

    const float lz = cz - rad;
    const float ly = cy - rad;
    const float lx = cx - rad;

    Box b;
    b.max.z = (wb.max.z <= lz) ? lz : wb.max.z;
    b.max.y = (wb.max.y <= ly) ? ly : wb.max.y;
    b.max.x = (wb.max.x <= lx) ? lx : wb.max.x;
    b.min.z = (lz <= wb.min.z) ? lz : wb.min.z;
    b.min.y = (ly <= wb.min.y) ? ly : wb.min.y;
    b.min.x = (lx <= wb.min.x) ? lx : wb.min.x;

    const float ez = rad + cz + extZ * kShadowExtend;
    const float ey = rad + cy + extY * kShadowExtend;
    const float ex = extX * kShadowExtend + cx + rad;

    if (b.max.z <= ez) b.max.z = ez;
    if (b.max.y <= ey) b.max.y = ey;
    if (b.max.x <= ex) b.max.x = ex;
    if (ez <= b.min.z) b.min.z = ez;
    if (ey <= b.min.y) b.min.y = ey;
    if (ex <= b.min.x) b.min.x = ex;
    return b;
}

// What one walk needs about its frame.
struct Frame {
    ::Frustum cull;
    ::Frustum shadow;
    // CULLROUTE (volume/frustum_cull.h): the two frusta's prepared tables, built
    // in ExtCulledGather after the two MulTransform3 that fill `cull` / `shadow`
    // (their only writers; the walk reads the Frame through `const Frame&`).
    FrustumCullPlanes cullPlanes;
    FrustumCullPlanes shadowPlanes;
    float     extX, extY, extZ;
    uint32_t  passMask;
    int32_t   rendererDetail;
    bool      notShadow;
    bool      cullVerify;          // g_frustumCullVerify, read once per gather (COMPPROF=4)
    mutable uint64_t boxes;        // box tests this gather (the stress report's per-site count)
};

// The one frustum test (retail 0x1800f8e80), over a prepared table: the same
// answer as RtVolume_Frustum_Contains on the frustum the table came from.
inline unsigned int Contains(const Frame& f, const FrustumCullPlanes* p, const Box* b, unsigned int mask) {
    ++f.boxes;
    return FrustumCull_Test(p, b, mask, f.cullVerify);
}

// Does the object pass, given its box? Retail's box half of the per-object test.
bool BoxPasses(const Frame& f, const Box& wb, uint32_t cullMask, uint32_t shadowMask) {
    if (cullMask != 0) {
        Box b = wb;
        if (Contains(f, &f.cullPlanes, &b, cullMask) == 0x40) return false;
    }
    if (f.notShadow) return true;
    const Box b = ShadowCasterBox(wb, f.extX, f.extY, f.extZ);
    return shadowMask == 0 || Contains(f, &f.shadowPlanes, &b, shadowMask) != 0x40;
}

// The quadtree walk, retail's, with the per-object test as a policy:
// `test(cur, geo, cullMask, shadowMask)` -> push or not.
template <class Test>
void Walk(const Scene* self, const Frame& f, std::vector<Geometry*>& out, Test& test) {
    uint32_t shadowMask = 0x3f;

    CullStack st;
    CullStackInit(&st, self, kMinZBias, kMaxZBias);

    while (st.top >= 0) {
        const int32_t t    = st.top;
        const int32_t cell = st.rec[t].node;

        if (self->_grid._cellCounts._data[cell] < 1) {
            st.top -= 1;
            continue;
        }

        const uint32_t packed = st.rec[t].mask;
        uint32_t cullMask = packed & 0xffffu;
        if (cullMask != 0) {
            const Box b = NodeBoxCull(&st, st.rec[t]);
            cullMask = Contains(f, &f.cullPlanes, &b, cullMask);
            if (cullMask == 0x40) {
                st.top -= 1;
                continue;
            }
        }
        if (!f.notShadow) {
            shadowMask = packed >> 16;
            if (shadowMask != 0) {
                const Box b = NodeBoxShadow(&st, st.rec[t]);
                shadowMask = Contains(f, &f.shadowPlanes, &b, shadowMask);
            }
        }

        const int32_t head = self->_grid._cellHeads._data[cell];
        if (head >= 0) {
            int32_t cur = head;
            do {
                const SceneNode& node = self->_grid._nodes._data[cur];
                Geometry* const  geo  = node._object;
                if (test(cur, geo, cullMask, shadowMask)) out.push_back(geo);
                cur = node._next;
            } while (cur != head);
        }

        CullStackPushChildren(&st, (shadowMask << 16) | cullMask);
    }
}

// Retail's per-object test, reading the Geometry in retail's order (the shadow's reference).
struct RetailTest {
    const Frame& f;
    bool operator()(int32_t, const Geometry* geo, uint32_t cullMask, uint32_t shadowMask) const {
        if ((geo->_materialTypeFlags & f.passMask) == 0 || geo->_detailId > f.rendererDetail)
            return false;
        return BoxPasses(f, geo->_worldBounds, cullMask, shadowMask);
    }
};

// The extension's: the side record first.
struct SideTest {
    const Frame& f;
    SideArray&   side;
    bool         verify;
    bool operator()(int32_t cur, const Geometry* geo, uint32_t cullMask, uint32_t shadowMask) {
        ++g_counters.nodesTested;
        bool touched = false;
        if (!side.Valid(cur, geo)) {
            StoreFull(side, cur, geo);
            touched = true;
        } else if (verify) {
            // COMPPROF=4: a valid record must hold the live box, bit for bit.
            const bool same = std::memcmp(side.At(cur).box, &geo->_worldBounds, sizeof(float) * 6) == 0;
            StressHarness_Verify("DrawGatherSide", same, same ? 0 : cur);
        }
        const SideRecord& r = side.At(cur);
        Box wb;
        std::memcpy(&wb, r.box, sizeof(wb));
        const bool pass = NodeVisible(
            HintPasses(r, f.passMask, f.rendererDetail),
            [&] {
                touched = true;
                return (geo->_materialTypeFlags & f.passMask) != 0 &&
                       geo->_detailId <= f.rendererDetail;
            },
            [&] { return BoxPasses(f, wb, cullMask, shadowMask); });
        if (touched) ++g_counters.geoTouched;
        return pass;
    }
};

// The visible lists, one per nesting depth: a dispatch can re-enter the gather
// (MaterialInstance::RenderTextures -> a generated image's own scene draw).
std::vector<std::vector<Geometry*>> g_lists;
int32_t                             g_depth = 0;

// ===========================================================================
// Gather -- the replaced Scene::AddCulledDrawItems.
// ===========================================================================
void ExtCulledGather(const Scene& scene, Renderer& rendererRef) {
    const Scene* self     = &scene;
    Renderer*    renderer = &rendererRef;
    ++g_counters.gathers;

    DrainLog();
    SideArray& side = g_sides.Acquire(self);

    const RendererDrawGroup* g = &renderer->_drawGroups[renderer->_currentDrawGroup];

    Frame f;
    f.passMask       = g->_materialTypeFlags;
    f.rendererDetail = renderer->_detailId;
    RtVolume_Frustum_MulTransform3(&g->_cullCamera._frustum, &f.cull, &g->_cullCamera._cameraToWorld);
    f.extX = f.cull.planes[0].x;
    f.extY = f.cull.planes[0].y;
    f.extZ = f.cull.planes[0].z;
    RtVolume_Frustum_MulTransform3(&g->_camera._frustum, &f.shadow, &g->_camera._cameraToWorld);
    f.notShadow = (g->_shader != 1);
    FrustumCull_Prepare(&f.cull, &f.cullPlanes);      // CULLROUTE: after both frusta's last writer
    FrustumCull_Prepare(&f.shadow, &f.shadowPlanes);
    f.cullVerify = g_frustumCullVerify != 0;
    f.boxes = 0;

    const int32_t depth = g_depth++;
    if (static_cast<int32_t>(g_lists.size()) <= depth) g_lists.resize(static_cast<size_t>(depth) + 1);
    std::vector<Geometry*>& list = g_lists[static_cast<size_t>(depth)];
    list.clear();

    const bool verify = g_verifyOn != 0;
    SideTest test{ f, side, verify };
    Walk(self, f, list, test);
    g_counters.visible += list.size();

    if (verify) {
        // COMPPROF=4: retail's reads, the same walk, into a scratch list.
        std::vector<Geometry*> ref;
        ref.reserve(list.size());
        RetailTest rt{ f };
        Walk(self, f, ref, rt);
        size_t firstDiff = 0;
        while (firstDiff < ref.size() && firstDiff < list.size() && ref[firstDiff] == list[firstDiff])
            ++firstDiff;
        const bool same = ref.size() == list.size() && firstDiff == ref.size();
        StressHarness_Verify("DrawGather", same,
                             same ? 0 : (static_cast<long long>(ref.size()) << 32) |
                                            static_cast<long long>(firstDiff));
    }
    FrustumCull_CountSite(kFrustumCullSiteExtDrawGather, f.boxes);   // CULLROUTE: once per gather

    // The dispatch: retail's virtual slot 3 over the list, in order.
    const int32_t n = static_cast<int32_t>(list.size());
    for (int32_t i = 0; i < n; ++i) {
        if (i + kDispatchAhead < n) {
            const Geometry* ahead = g_lists[static_cast<size_t>(depth)][static_cast<size_t>(i + kDispatchAhead)];
            __builtin_prefetch(ahead);                         // vptr, _detailId
            __builtin_prefetch(&ahead->_worldBounds);          // the LOD box
            __builtin_prefetch(&ahead->_opacity);              // opacity pair, transform
            __builtin_prefetch(&ahead->_transform._previous);
            __builtin_prefetch(ahead + 1);                     // a MeshInstance's mesh / materials / animation
        }
        // `list` may move if a nested gather grows g_lists: index it afresh.
        Geometry* geo = g_lists[static_cast<size_t>(depth)][static_cast<size_t>(i)];
        geo->AddDrawItems(*self, *renderer);                   // VIRTUAL, slot 3
    }
    --g_depth;
}

}  // namespace

void Sync() {
    const bool want = Ext::Ext().draw_gather;
    if (want == g_installed) return;
    if (want) {
        DrainLog();                                // stale ids from an earlier spell
        g_sides.ForgetAll();                       // every scene starts with no valid record
        ++g_counters.rebuilds;
        Scene::_extAddLog       = &g_addLog;
        Scene::_extClean        = &ExtClean;
        Scene::_extCulledGather = &ExtCulledGather;
    } else {
        Scene::_extCulledGather = nullptr;
        Scene::_extClean        = nullptr;
        Scene::_extAddLog       = nullptr;
        g_sides.ForgetAll();
        std::vector<std::vector<Geometry*>>().swap(g_lists);
    }
    g_installed = want;
}

bool Installed() { return g_installed; }

Counters TakeWindow() {
    const Counters c = g_counters;
    g_counters = Counters{};
    return c;
}

}  // namespace DrawGather
