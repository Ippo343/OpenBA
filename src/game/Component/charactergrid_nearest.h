#pragma once
// ============================================================================
// src/game/Component/charactergrid_nearest.h -- BESTTARGET (perf, byte-identical)
//
// The walk of CharacterGrid::QueryNearestCharacter 0x140092260 (retail body:
// CharacterGrid::QueryNearestCharacterRetail, action_pf_roots_misc.cpp), with the same
// result, the same visit order and the same float expressions, at a lower cost:
//
//  * No CharacterGridWalker. Retail's walker::Init 0x140018dd0 zeroes all 256 nodes
//    (6 KB of stores on a stack local) before pushing the root; every node the query ever
//    reads was written by a push first, so those stores are dead. Here a small stack of
//    nodes is used and only pushes write it.
//  * Children are tested when they are made, not when they are popped. Retail's
//    Subdivide 0x140018880 pushes all four children (cell 4i+1..4i+4) and the query pops
//    and drops the ones whose subtree is empty (cellOccupancy <= 0) or whose loose box
//    misses the query box. The drop has no side effect and nothing the test reads changes
//    during a query (the query writes only *best and locals), so testing at push time
//    keeps exactly the nodes retail processes, in the same LIFO order (4, 3, 2, 1 among
//    the survivors).
//  * The candidates of a cell are gathered (slot array only) in retail's order, a chunk
//    at a time, and their CharacterComponent / ModelComponent lines are prefetched before
//    the chunk is scored in that same order. The scoring is retail's: skip *exclude,
//    the optional exact-type filter, dsq = Vector3::Dot(diff, diff), keep it on a strict
//    `dsq < best` (the first of equal distances wins).
//
// The policy P supplies the character type and three reads:
//   P::Char                                  the character type
//   static Char* Get(const Slot&)            the slot's character
//   static const Math::Vector3& Pos(Char*)   the character's model translation
//   static bool Keep(Char*, int typeId)      the type filter (typeId != 0 only)
//   static void Prefetch(Char*)              a hint (no load that can fault)
//   static void PrefetchPos(Char*)           may load Char's model pointer (typeId == 0,
//                                            non-excluded candidates only: the reads retail
//                                            itself makes)
// Slot is any record with int32 `prev` (CharacterGridSlot; the unit test's own record).
//
// Returns false (and writes nothing) only when `levels` is deeper than the node stack can
// hold; the caller then runs the retail body. The game grid has levels == 8 (22 nodes at
// most on the stack).
// ============================================================================
#include <cstdint>
#include "math/Math.h"   // Math::Vector3 / Scalar / Dot: the retail dsq expression

namespace CharGridNearest {

struct Node {
    float cx, cy, cz;   // walker node center[0..2]
    float ex, ey, ez;   // walker node extent[0..2]
    int32_t cell;
    int32_t depth;
};

// Node stack capacity. A pop pushes at most four, so the stack holds at most
// 1 + 3 * (levels - 1) nodes.
constexpr int kStack = 96;
constexpr int kMaxLevels = (kStack - 1) / 3;   // 31
constexpr int kChunk = 16;

// 0x140092260's node box against the query box, every float operation as retail writes
// it: ext = (2.0 * e0, 2.0 * e1, g_fUVScale * e2); min = padLo + (center - ext),
// max = padHi + (ext + center), padLo = (0, 0, -1), padHi = (0, 0, 1) (walker::Init's
// lo / hi); then Volume::Box::Intersect (InlBoxIsect).
inline bool NodeHits(const float* q, const Node& n) {
    const float f2 = 2.0f;
    const float fUV = 1.0f;
    const float extx = f2 * n.ex;
    const float exty = f2 * n.ey;
    const float extz = fUV * n.ez;
    const float minx = 0.0f + (n.cx - extx);
    const float miny = 0.0f + (n.cy - exty);
    const float minz = -1.0f + (n.cz - extz);
    const float maxx = 0.0f + (extx + n.cx);
    const float maxy = 0.0f + (exty + n.cy);
    const float maxz = 1.0f + (extz + n.cz);
    return q[0] <= maxx && minx <= q[3] && q[1] <= maxy && miny <= q[4] && q[2] <= maxz && minz <= q[5];
}

// Subdivide 0x140018880's child k (1..4) of `p`, its float operations in its order:
// h = e * 0.5, n = h * -1; child 1 = (cx + nx, cy + ny, cz + ez), 2 = (hx + cx, ny + cy,
// ez + cz), 3 = (nx + cx, hy + cy, ez + cz), 4 = (hx * 1 + cx, hy * 1 + cy, ez * 1 + cz);
// extent (hx, hy, ez), cell 4i + k, depth + 1.
inline void MakeChildren(const Node& p, Node* out) {
    const float hy = p.ey * 0.5f;
    const float hx = p.ex * 0.5f;
    const float ny = hy * -1.0f;
    const float nx = hx * -1.0f;
    const float one = 1.0f;
    const int d = p.depth + 1;
    out[0] = Node{p.cx + nx, p.cy + ny, p.cz + p.ez, hx, hy, p.ez, p.cell * 4 + 1, d};
    out[1] = Node{hx + p.cx, ny + p.cy, p.ez + p.cz, hx, hy, p.ez, p.cell * 4 + 2, d};
    out[2] = Node{nx + p.cx, hy + p.cy, p.ez + p.cz, hx, hy, p.ez, p.cell * 4 + 3, d};
    out[3] = Node{hx * one + p.cx, hy * one + p.cy, p.ez * one + p.cz, hx, hy, p.ez, p.cell * 4 + 4, d};
}

template <class P, class Slot>
bool Query(typename P::Char** best, const float* point, float radius, int typeId,
           typename P::Char* const* exclude, const Slot* slots, const int32_t* cellHead,
           const int32_t* cellOccupancy, float worldSize, int levels) {
    using Char = typename P::Char;
    if (levels > kMaxLevels) return false;

    *best = nullptr;
    float fr2 = radius * radius;
    // the query box: min = point - radius, max = radius + point (retail's operand order)
    float q[6];
    q[0] = point[0] - radius; q[1] = point[1] - radius; q[2] = point[2] - radius;
    q[3] = radius + point[0]; q[4] = radius + point[1]; q[5] = radius + point[2];

    Node stack[kStack];
    int top = -1;
    {
        // walker::Init's root: center splat(0), extent (worldSize * 0.5, same, 0), cell 0
        const float half = worldSize * 0.5f;
        const Node root{0.0f, 0.0f, 0.0f, half, half, 0.0f, 0, 0};
        if (0 < cellOccupancy[0] && NodeHits(q, root)) stack[++top] = root;
    }

    while (top >= 0) {
        const Node n = stack[top--];

        const int head = cellHead[n.cell];
        if (-1 < head) {
            int ent = head;
            do {
                Char* buf[kChunk];
                int cnt = 0;
                do {
                    buf[cnt++] = P::Get(slots[ent]);
                    ent = slots[ent].prev;
                } while (ent != head && cnt < kChunk);
                for (int k = 0; k < cnt; ++k) P::Prefetch(buf[k]);
                if (typeId == 0) {
                    for (int k = 0; k < cnt; ++k)
                        if (buf[k] != *exclude) P::PrefetchPos(buf[k]);
                }
                for (int k = 0; k < cnt; ++k) {
                    Char* c = buf[k];
                    if (c == *exclude) continue;
                    if (typeId != 0 && !P::Keep(c, typeId)) continue;
                    const Math::Vector3& pos = P::Pos(c);
                    Math::Vector3 diff(pos.x - point[0], pos.y - point[1], pos.z - point[2]);
                    const float dsq = diff.Dot(diff).v;
                    if (dsq < fr2) {
                        *best = c;
                        fr2 = dsq;
                    }
                }
            } while (ent != head);
        }

        if (n.depth + 1 < levels) {
            Node kids[4];
            MakeChildren(n, kids);
            for (int k = 0; k < 4; ++k) {
                if (0 < cellOccupancy[kids[k].cell] && NodeHits(q, kids[k])) stack[++top] = kids[k];
            }
        }
    }
    return true;
}

}  // namespace CharGridNearest
