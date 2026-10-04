// code/Game/Action/action_pf_pools.cpp
// Pathfinder/Action node-pool grow + dynamic-array resize family.
//
// Faithful 1:1 ports of the A*/pathfinder node-pool free-list grow helpers and the
// associated dynamic-array (vector) resize/copy/free helpers.
// None of this is on the autostart path, so a *wrong* port is undetectable
// latent corruption — every line below is transcribed literally from the Ghidra
// decompile (cross-checked against the disassembly for the intricate index math). Do
// NOT "improve" or simplify.
//
// B.4 ACT-a: the bodies read their records by name (code/Game/Action/PathSearch.h).
// Allocation: the original passes (size, align) to the static
// System::MemoryAllocator::MemAlloc and frees through MemoryAllocator::MemFree; so do we.
//
// The three pool allocators (0x14001a9b0, 0x14001d730, 0x14001df40) carve nodes out of
// a raw chunk: the first qword of a chunk links the chunk list, the nodes follow, and
// a free node's first qword is the free-list link (PathPoolLink). Those casts are the
// pool's own raw memory becoming a node (each carries an elem_stride marker).
//
// Pf_VectorResize60_fa80 (0x14001fa80) calls TradeWork_impl_sub_f5e0 (0x14001f5e0, element
// copy-ctor) and TradeWork_impl_sub_f250_sub_fb80 (0x14001fb80, 0x60-stride element free),
// now Reimpl_Struct60B_CopyCtor / Reimpl_Struct60B_DtorRange (Game), called by name.
#include "Action/exe_bodies.h"
#include "Action/PathSearch.h"

#include "Game/exe_bodies.h"          // Reimpl_Struct60B_CopyCtor, Reimpl_Struct60B_DtorRange
#include "Game/WorldGeometryElements.h"   // GeometryBatch, GeometryBatchVertex (the 0x60 / 0x14 elements)
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include <cstdio>    // [POOLDIAG] diagnostic reporting
#include <cstdlib>

namespace {
// The chunk size every pool grow allocates: growCount * stride + 0xf (the chunk link
// and the rounding), rounded down to 8 with retail's signed-division idiom.
static inline uint64_t PoolChunkBytes(const PathNodePool* pool) {
    int bytes = pool->stride * pool->growCount + 0xf;
    return static_cast<uint64_t>(static_cast<int>((bytes + (bytes >> 0x1f & 7U)) & static_cast<int>(0xfffffff8)));
}
} // namespace

// ============================================================
// 0x14001a9b0 — Map's cell queue: take a node (growing the pool first).
// ============================================================
// If the free list is empty, double growCount, allocate a chunk of growCount nodes
// (stride 0x18 when unset), link it onto the chunk list and push each new node onto
// the free list. Then pop one node, zero next/prev, write the cell (one DWORD), return
// it. Null if the pop yielded null.
template <>
MapCellQueue::Node* MapCellQueue::NewNode(const PathCellIndex* value) {
    PathNodePool* p = &pool;
    if (p->freeList == nullptr) {
        if (p->stride == 0) {
            p->stride = 0x18;
        }
        p->growCount = p->growCount * 2;
        PathPoolLink* chunk = static_cast<PathPoolLink*>(System::MemoryAllocator::MemAlloc(PoolChunkBytes(p), 8));
        int iVar4 = 0;
        chunk->next = p->chunks;
        int iVar2 = p->growCount;
        p->chunks = chunk;
        p->total = p->total + iVar2;
        p->inUse = p->inUse + iVar2;
        PathPoolLink* link = chunk + 1;
        if (0 < iVar2) {
            do {
                p->inUse = p->inUse + -1;
                p->available = p->available + 1;
                link->next = p->freeList;
                p->freeList = link;
                iVar4 = iVar4 + 1;
                link = reinterpret_cast<PathPoolLink*>(reinterpret_cast<uint8_t*>(link) + p->stride);  // CAST-OK: elem_stride
            } while (iVar4 < p->growCount);
        }
    }
    PathPoolLink* link = p->freeList;
    p->inUse = p->inUse + 1;
    p->available = p->available + -1;
    p->freeList = link->next;
    if (link != nullptr) {
        Node* node = reinterpret_cast<Node*>(link);  // CAST-OK: elem_stride
        int32_t cell = value->cell;
        node->next = nullptr;
        node->prev = nullptr;
        node->value.cell = cell;
        return node;
    }
    return nullptr;
}

// ============================================================
// 0x14001d730 — the open set: take a node (growing the pool first).
// ============================================================
// Same grow as 0x14001a9b0 with stride 0x30. The popped node gets the key's cell and
// seq, the parent, zeroed children and balance/height, then the key's fcost.
PathOpenNode* PathOpenSet::NewNode(const PathOpenKey* key, PathOpenNode* parent) {
    PathNodePool* p = &pool;
    int iVar4 = 0;
    if (p->freeList == nullptr) {
        if (p->stride == 0) {
            p->stride = 0x30;
        }
        p->growCount = p->growCount * 2;
        PathPoolLink* chunk = static_cast<PathPoolLink*>(System::MemoryAllocator::MemAlloc(PoolChunkBytes(p), 8));
        chunk->next = p->chunks;
        int iVar2 = p->growCount;
        p->chunks = chunk;
        p->total = p->total + iVar2;
        p->inUse = p->inUse + iVar2;
        PathPoolLink* link = chunk + 1;
        if (0 < iVar2) {
            do {
                p->inUse = p->inUse + -1;
                p->available = p->available + 1;
                link->next = p->freeList;
                p->freeList = link;
                iVar4 = iVar4 + 1;
                link = reinterpret_cast<PathPoolLink*>(reinterpret_cast<uint8_t*>(link) + p->stride);  // CAST-OK: elem_stride
            } while (iVar4 < p->growCount);
        }
    }
    PathPoolLink* link = p->freeList;
    p->inUse = p->inUse + 1;
    p->available = p->available + -1;
    p->freeList = link->next;
    if (link == nullptr) {
        return nullptr;
    }
    PathOpenNode* node = reinterpret_cast<PathOpenNode*>(link);  // CAST-OK: elem_stride
    node->cell = key->cell;
    node->seq = key->seq;
    int32_t fcost = key->fcost;
    node->parent = parent;
    node->left = nullptr;
    node->right = nullptr;
    node->balance = 0;                     // +0x28 / +0x2a: one dword store (H7: two stores)
    node->height = 0;
    node->fcost = fcost;
    return node;
}

// ============================================================
// 0x14001df40 — the region graph's (x, y) queue: take a node (growing the pool first).
// ============================================================
// Identical grow + pop to 0x14001a9b0; the popped node gets the whole (x, y) qword.
template <>
RegionCellQueue::Node* RegionCellQueue::NewNode(const PathCellXY* value) {
    PathNodePool* p = &pool;
    if (p->freeList == nullptr) {
        if (p->stride == 0) {
            p->stride = 0x18;
        }
        p->growCount = p->growCount * 2;
        PathPoolLink* chunk = static_cast<PathPoolLink*>(System::MemoryAllocator::MemAlloc(PoolChunkBytes(p), 8));
        int iVar4 = 0;
        chunk->next = p->chunks;
        int iVar2 = p->growCount;
        p->chunks = chunk;
        p->total = p->total + iVar2;
        p->inUse = p->inUse + iVar2;
        PathPoolLink* link = chunk + 1;
        if (0 < iVar2) {
            do {
                p->inUse = p->inUse + -1;
                p->available = p->available + 1;
                link->next = p->freeList;
                p->freeList = link;
                iVar4 = iVar4 + 1;
                link = reinterpret_cast<PathPoolLink*>(reinterpret_cast<uint8_t*>(link) + p->stride);  // CAST-OK: elem_stride
            } while (iVar4 < p->growCount);
        }
    }
    PathPoolLink* link = p->freeList;
    p->inUse = p->inUse + 1;
    p->available = p->available + -1;
    p->freeList = link->next;
    if (link != nullptr) {
        Node* node = reinterpret_cast<Node*>(link);  // CAST-OK: elem_stride
        PathCellXY xy = *value;
        node->next = nullptr;
        node->prev = nullptr;
        node->value = xy;
        return node;
    }
    return nullptr;
}

// ============================================================
// 0x14001d400 — give an open-set subtree back to the pool.
// ============================================================
// Post-order: the left child's subtree, the right child's, then this node (children
// freed before the node).
void PathOpenSet::FreeSubtree(PathOpenNode* node) {
    if (node != nullptr) {
        FreeSubtree(node->left);
        FreeSubtree(node->right);
        pool.Push(node);
    }
}

// ============================================================
// 0x14001d830 — resize the search-node array (element 0x18), copy + fill.
// ============================================================
// Grow (3x on a +1 growth from non-empty) via MemAlloc(cap*0x18, 8), copy existing
// elements, and give new slots the empty node {openNode 0, parent -1, 0, 0, 0}.
// MemFree the old buffer. The new-slot init writes parent as a dword, then one qword
// over openStamp / closedStamp and a dword gCost — two named stores for the qword (H7).
void SearchNodesResize(RawArray<PathSearchNode>* nodes, int newSize) {
    int iVar1 = nodes->size;
    if (newSize != iVar1) {
        PathSearchNode* old = nodes->data;
        nodes->size = newSize;
        if (nodes->capacity < newSize) {
            nodes->capacity = newSize;
            if ((newSize == iVar1 + 1) && (iVar1 != 0)) {
                nodes->capacity = newSize * 3;
            }
            nodes->data = static_cast<PathSearchNode*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(nodes->capacity) * 0x18, 8));
        }
        int iVar3 = 0;
        if (old == nodes->data) {
            iVar3 = iVar1;
        }
        int64_t lVar6 = iVar3;
        if (lVar6 < newSize) {
            do {
                PathSearchNode* dst = &nodes->data[lVar6];
                if (lVar6 < iVar1) {
                    if (dst != nullptr) {
                        *dst = old[lVar6];
                    }
                } else {
                    if (dst != nullptr) {
                        dst->openNode = nullptr;
                        dst->parent = -1;
                        dst->openStamp = 0;
                        dst->closedStamp = 0;
                        dst->gCost = 0;
                    }
                }
                lVar6 = lVar6 + 1;
            } while (lVar6 < newSize);
        }
        if (old != nodes->data) {
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// 0x14001db40 — free the path arrays of `count` result slots.
// ============================================================
// Each slot owns its path buffer: MemFree it, then zero data and size/capacity.
// Retail's RCX (a count-shaped value the caller leaves there) is never read.
void PathResultsFree(PathResult* first, int count) {
    if (0 < count) {
        int64_t lVar2 = count;
        PathResult* r = first;
        do {
            System::MemoryAllocator::MemFree(r->path._data);
            r->path._data = nullptr;
            r->path._size = 0;             // +0x10 / +0x14: one qword store (H7: two stores)
            r->path._capacity = 0;
            r = r + 1;
            lVar2 = lVar2 + -1;
        } while (lVar2 != 0);
    }
}

// ============================================================
// 0x14001e490 — free the two int arrays of `count` region nodes.
// ============================================================
// borderCells (+0x20) first, then neighbours (+0x10); each: MemFree, zero data, one
// qword over size/capacity. Retail's RCX is never read.
void RegionNodesFree(RegionNode* first, int count) {
    if (0 < count) {
        int64_t lVar2 = count;
        RegionNode* n = first;
        do {
            System::MemoryAllocator::MemFree(n->borderCells._data);
            n->borderCells._data = nullptr;
            n->borderCells._size = 0;      // one qword store (H7: two stores)
            n->borderCells._capacity = 0;
            System::MemoryAllocator::MemFree(n->neighbours._data);
            n->neighbours._data = nullptr;
            n->neighbours._size = 0;       // one qword store (H7: two stores)
            n->neighbours._capacity = 0;
            n = n + 1;
            lVar2 = lVar2 + -1;
        } while (lVar2 != 0);
    }
}

// ============================================================
// FUN_14001ee40 — resize a GeometryBatch's vertex array (element 0x14), copy only.
// ============================================================
// Resize to newCount: grow (3x on +1 from non-empty) via MemAlloc(cap*0x14, 2), copy
// the existing elements from the old buffer; new slots are NOT written (the copy is
// guarded by lVar5 < iVar1 and there is no else branch). MemFree old.
// The element is GeometryBatchVertex (Game/WorldGeometryElements.h): retail's callers
// are the vertex-array serializer 0x14001f9c0, the decal / road builders 0x1400ace40,
// 0x1400b0d00 and 0x1400af370; the copy is retail's qword, qword, dword
// (0x14001eec4..0x14001eed9).
void Pf_VectorResize14_ee40(int64_t* param_1, int param_2) {
    RawArray<GeometryBatchVertex>* arr = reinterpret_cast<RawArray<GeometryBatchVertex>*>(param_1);   // LEFT: SIG Pf_VectorResize14_ee40
    int iVar1 = arr->size;
    if (param_2 != iVar1) {
        GeometryBatchVertex* old = arr->data;
        arr->size = param_2;
        if (arr->capacity < param_2) {
            arr->capacity = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                arr->capacity = param_2 * 3;
            }
            arr->data = static_cast<GeometryBatchVertex*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(arr->capacity) * 0x14, 2));
        }
        int iVar3 = 0;
        if (old == arr->data) {
            iVar3 = iVar1;
        }
        int64_t lVar5 = iVar3;
        if (iVar3 < param_2) {
            do {
                if ((lVar5 < iVar1)) {
                    GeometryBatchVertex* dst = &arr->data[lVar5];
                    if (dst != nullptr) {
                        *dst = old[lVar5];
                    }
                }
                lVar5 = lVar5 + 1;
            } while (lVar5 < param_2);
        }
        if (old != arr->data) {
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// FUN_14001f8d0 — resize a GeometryBatch's vertex array (element 0x14), fill from *param_3.
// ============================================================
// Variant of ee40 that fills new slots (lVar7>=iVar1) from the template vertex *param_3.
// Copy existing elements from the old buffer; for slots at or past the old count, copy
// from the template. MemFree old. The loop counts (param_2-iVar4) iterations and walks
// the dst and the old-buffer cursor in lockstep. Callers: the decal tessellator
// 0x1400ace40 (twelve sites) and the road mesh rebuild 0x1400af370.
void Pf_VectorResize14Fill_f8d0(int64_t* param_1, int param_2, uint64_t* param_3) {
    RawArray<GeometryBatchVertex>* arr = reinterpret_cast<RawArray<GeometryBatchVertex>*>(param_1);   // LEFT: SIG Pf_VectorResize14Fill_f8d0
    const GeometryBatchVertex* fill = reinterpret_cast<const GeometryBatchVertex*>(param_3);          // LEFT: SIG Pf_VectorResize14Fill_f8d0
    int iVar1 = arr->size;
    if (param_2 != iVar1) {
        GeometryBatchVertex* old = arr->data;
        arr->size = param_2;
        if (arr->capacity < param_2) {
            arr->capacity = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                arr->capacity = param_2 * 3;
            }
            arr->data = static_cast<GeometryBatchVertex*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(arr->capacity) * 0x14, 2));
        }
        int iVar4 = 0;
        if (old == arr->data) {
            iVar4 = iVar1;
        }
        int64_t lVar7 = iVar4;
        if (iVar4 < param_2) {
            uint64_t uVar10 = static_cast<uint64_t>(static_cast<uint32_t>(param_2 - iVar4));
            const GeometryBatchVertex* puVar9 = old + lVar7;
            do {
                GeometryBatchVertex* dst = &arr->data[lVar7];
                if (dst != nullptr) {
                    const GeometryBatchVertex* src = fill;
                    if (lVar7 < iVar1) {
                        src = puVar9;
                    }
                    *dst = *src;
                }
                puVar9 = puVar9 + 1;
                lVar7 = lVar7 + 1;
                uVar10 = uVar10 - 1;
            } while (uVar10 != 0);
        }
        if (old != arr->data) {
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// 0x14001d940 — resize the result-slot array (element 0x18, owns its path array).
// ============================================================
// Grow (3x on +1 from non-empty) via MemAlloc(cap*0x18, 8). Each slot is copied from the
// old buffer (or the fill for appended slots): ready (a byte), token, then the path
// array zeroed and deep-copied (0x14000f9e0). On an in-place shrink the tail slots' paths
// are freed (0x14001db40); on a realloc the whole old buffer's paths are freed (inline
// loop) and the old buffer is MemFreed.
void PathResultsResize(RawArray<PathResult>* results, int newSize, const PathResult* fill) {
    int iVar1 = results->size;
    int64_t lVar8 = iVar1;
    if (newSize != iVar1) {
        PathResult* old = results->data;
        results->size = newSize;
        if (results->capacity < newSize) {
            results->capacity = newSize;
            if ((newSize == iVar1 + 1) && (iVar1 != 0)) {
                results->capacity = newSize * 3;
            }
            results->data = static_cast<PathResult*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(results->capacity) * 0x18, 8));
        }
        int iVar4 = 0;
        if (old == results->data) {
            iVar4 = iVar1;
        }
        int64_t lVar12 = iVar4;
        if (iVar4 < newSize) {
            const PathResult* src9 = old + lVar12;
            uint64_t uVar10 = static_cast<uint64_t>(static_cast<uint32_t>(newSize - iVar4));
            do {
                PathResult* dst = &results->data[lVar12];
                if (dst != nullptr) {
                    const PathResult* src = fill;
                    if (lVar12 < lVar8) {
                        src = src9;
                    }
                    dst->ready = src->ready;
                    dst->token = src->token;
                    dst->path._data = nullptr;
                    dst->path._size = 0;   // +0x10 / +0x14: one qword store (H7: two stores)
                    dst->path._capacity = 0;
                    IntArrayAssign(&dst->path, &src->path);
                }
                src9 = src9 + 1;
                lVar12 = lVar12 + 1;
                uVar10 = uVar10 - 1;
            } while (uVar10 != 0);
        }
        if (old == results->data) {
            if (newSize < iVar1) {
                PathResultsFree(old + newSize, iVar1 - newSize);   // 0x14001db40
            }
        } else {
            if (0 < lVar8) {
                PathResult* r = old;
                do {
                    System::MemoryAllocator::MemFree(r->path._data);
                    r->path._data = nullptr;
                    r->path._size = 0;     // one qword store (H7: two stores)
                    r->path._capacity = 0;
                    r = r + 1;
                    lVar8 = lVar8 + -1;
                } while (lVar8 != 0);
            }
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// FUN_14001fa80 — resize a GeometryBatch array (element 0x60) with element copy-ctor + dtor.
// ============================================================
// The element is GeometryBatch (Game/WorldGeometryElements.h): its copy-ctor 0x14001f5e0
// and its range dtor 0x14001fb80 are called here. Callers: the decal slot manager
// 0x1400b0d80 (WorldGeometry +0x1F0) and the road mesh rebuild 0x1400af370 (+0x200).
// Resize: grow (3x on +1 from non-empty) via MemAlloc(cap*0x60, 8). For each slot,
// copy-construct from the old buffer (or the template *param_3 for appended slots). On
// shrink-in-place, destruct the tail; on realloc, destruct the whole old buffer then
// MemFree it.
void Pf_VectorResize60_fa80(int64_t* param_1, int param_2, uint64_t* param_3) {
    RawArray<GeometryBatch>* arr = reinterpret_cast<RawArray<GeometryBatch>*>(param_1);   // LEFT: SIG Pf_VectorResize60_fa80
    const GeometryBatch* fill = reinterpret_cast<const GeometryBatch*>(param_3);          // LEFT: SIG Pf_VectorResize60_fa80
    int iVar1 = arr->size;
    if (param_2 != iVar1) {
        GeometryBatch* old = arr->data;
        arr->size = param_2;
        if (arr->capacity < param_2) {
            arr->capacity = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                arr->capacity = param_2 * 3;
            }
            arr->data = static_cast<GeometryBatch*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(arr->capacity) * 0x60, 8));
        }
        int iVar6 = 0;
        if (old == arr->data) {
            iVar6 = iVar1;
        }
        if (iVar6 < param_2) {
            do {
                GeometryBatch* dst = &arr->data[iVar6];
                if (dst != nullptr) {
                    const GeometryBatch* src = old + iVar6;
                    if (iVar1 <= iVar6) {
                        src = fill;
                    }
                    Reimpl_Struct60B_CopyCtor(dst, src);   // 0x14001f5e0
                }
                iVar6 = iVar6 + 1;
            } while (iVar6 < param_2);
        }
        if (old == arr->data) {
            if (param_2 < iVar1) {
                Reimpl_Struct60B_DtorRange(nullptr, old + param_2, iVar1 - param_2);   // 0x14001fb80
            }
        } else {
            Reimpl_Struct60B_DtorRange(nullptr, old, iVar1);   // 0x14001fb80
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// 0x14001e210 — resize the region-node array (element 0x30, owns two int arrays).
// ============================================================
// Grow (3x on +1 from non-empty) via MemAlloc(cap*0x30, 8). Each node gets x, y, block
// from the old buffer (or the fill), then both arrays zeroed and deep-copied
// (0x14000f9e0). On an in-place shrink the tail's arrays are freed (0x14001e490); on a
// realloc the whole old buffer's (0x14001e490), then the buffer is MemFreed.
void RegionNodesResize(RawArray<RegionNode>* regions, int newSize, const RegionNode* fill) {
    int uVar1 = regions->size;
    if (newSize != uVar1) {
        RegionNode* old = regions->data;
        regions->size = newSize;
        if (regions->capacity < newSize) {
            regions->capacity = newSize;
            if ((newSize == uVar1 + 1) && (uVar1 != 0)) {
                regions->capacity = newSize * 3;
            }
            regions->data = static_cast<RegionNode*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(regions->capacity) * 0x30, 8));
        }
        int iVar4 = 0;
        if (old == regions->data) {
            iVar4 = uVar1;
        }
        int64_t lVar8 = iVar4;
        if (iVar4 < newSize) {
            const RegionNode* src9 = old + lVar8;
            uint32_t uVar5 = static_cast<uint32_t>(newSize - iVar4);
            do {
                RegionNode* dst = &regions->data[lVar8];
                if (dst != nullptr) {
                    const RegionNode* src = fill;
                    if (lVar8 < uVar1) {
                        src = src9;
                    }
                    dst->x = src->x;
                    dst->y = src->y;
                    dst->block = src->block;
                    dst->neighbours._data = nullptr;
                    dst->neighbours._size = 0;       // one qword store (H7: two stores)
                    dst->neighbours._capacity = 0;
                    IntArrayAssign(&dst->neighbours, &src->neighbours);
                    dst->borderCells._data = nullptr;
                    dst->borderCells._size = 0;      // one qword store (H7: two stores)
                    dst->borderCells._capacity = 0;
                    IntArrayAssign(&dst->borderCells, &src->borderCells);
                }
                src9 = src9 + 1;
                lVar8 = lVar8 + 1;
                uVar5 = uVar5 - 1;
            } while (uVar5 != 0);
        }
        if (old == regions->data) {
            if (newSize < uVar1) {
                RegionNodesFree(old + newSize, uVar1 - newSize);   // 0x14001e490
            }
        } else {
            RegionNodesFree(old, uVar1);   // 0x14001e490
            System::MemoryAllocator::MemFree(old);
        }
    }
}

// ============================================================
// 0x14000f9e0 — Collection::Array<int>'s deep assign (dst <- src), MemAlloc.
// ============================================================
// Frees dst's old buffer, resets ptr/count/cap to 0, sets count=src.count, and (if
// count>0) cap=count + allocates count*4 bytes (align 4), then copies each element from
// src. No-op if dst==src. Returns dst.
Collection::Array<int32_t>* IntArrayAssign(Collection::Array<int32_t>* dst,
                                           const Collection::Array<int32_t>* src) {
    if (dst != src) {
        System::MemoryAllocator::MemFree(dst->_data);
        dst->_size = 0;                    // one qword store over size / capacity (H7)
        dst->_capacity = 0;
        dst->_data = nullptr;
        int iVar1 = src->_size;
        dst->_size = iVar1;
        if (0 < iVar1) {
            dst->_capacity = iVar1;
            dst->_data = static_cast<int32_t*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(iVar1) << 2, 4));
        }
        int64_t i = 0;
        if (0 < dst->_size) {
            int32_t uVar4;
            do {
                int32_t* slot = &dst->_data[i];
                if (slot != nullptr) {
                    *slot = src->_data[i];
                }
                uVar4 = static_cast<int32_t>(i) + 1;
                i = i + 1;
            } while (uVar4 < dst->_size);
        }
        return dst;
    }
    return dst;
}
