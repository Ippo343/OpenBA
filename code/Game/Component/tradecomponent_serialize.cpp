// code/Game/Component/tradecomponent_serialize.cpp
// TradeComponent::Serialize (vtable slot [3], +0x18, retail 0x14005bd50).
//
// Ghidra names this function "Shutdown" (same off-by-one mislabel family as
// DiseaseComponent's slot-3/4/6/7 — see that file's header). tradecomponent.cpp
// already flagged this exact address as "delegated to original exe (too
// complex for safe reimpl)"; this file is that port, done in full.
//
// Field table (self is the 0xa8-byte TradeComponent body):
//   +0x50/+0x58  ComponentRef table, stride 0x10, count @+0x58
//                elem+0x00 ComponentRef (ExternalPtr<ComponentDescription>,
//                          fn_ExternalPtr_BuildSerialize / retail 0x140002340)
//                elem+0x08 int   elem+0x0c int
//                resize (load only) via fn_SummaryArray_Init (retail 0x14000f090,
//                already registered elsewhere as StorageComponent_DynArrayResize —
//                same generic 0x10-stride array-resize body, reused as-is).
//   +0x2c        int — selected-merchant index (-1 = none; see the ctor note
//                in tradecomponent.cpp).
//   +0x60        AVL tree {root@+0x00, count@+0x08} keyed by ComponentRef,
//                value int. See the TradeBst_* block below.
//   +0x30/+0x38  trade-name array, stride 0x20, count @+0x38 (the SAME array
//                TradeComponent_ResizeNames in tradecomponent.cpp already
//                grows/shrinks — reused here, not re-implemented).
//                elem+0x00 String (g_fn_String_Serialize, retail 0x140151238)
//                elem+0x08 int
//                elem+0x10 ptr-array {ptr,count:i32,cap:i32} of
//                          ExternalPtr<ComponentDescription>; count is
//                          elem+0x18, resized (load only) via
//                          Collection::Array<Ptr8>::SetSize
//                          (retail 0x140001bb0), contents via
//                          fn_ExternalRefArray_Serialize (retail 0x1400049c0,
//                          already ported as Reimpl_ExternalPtrArray_Serialize
//                          in serialize_externals_array.cpp).
//   +0x40/+0x48  item table, stride 0xc, count @+0x48 (TradeComponent_ResizeItems,
//                reused from tradecomponent.cpp).
//                elem+0x00 int resourceId — an ENUM, serialized through the
//                          serializer's own "encoding" object (ser+8's vtable
//                          +0x98), the same idiom already ported repeatedly
//                          elsewhere (see storagedescription.cpp's SerializeEnum).
//                elem+0x08 int   elem+0x04 int   (retail's own field order —
//                          +0x08 before +0x04, kept faithful)
//   +0x28        int (plain)
//   +0x78        Time
//   +0x71        bool
//   +0x80        InternalBasePtr, validated against ComponentEntity
//                (g_TypeId_ComponentEntity) and nulled on type mismatch —
//                same SerializeValidatedIBP idiom as animalwave_serialize.cpp.
//   +0x70        bool
//
// Retail also constructs+destructs a throwaway version-0 Serializer right at
// entry (0x14005bd7d/0x14005bd87) before building the real one used for
// everything else. Verified from the disassembly to have no effect on the
// stream (ctor/dtor with nothing serialized in between) — omitted here.
//
// ⚠ NOT WIRED into the vtable by this file, matching animalwave_serialize.cpp's
// deliberate choice: standalone has no save/load driver yet, and slot-3
// wiring is a repo-wide pass (this session has dedicated wire-ser-* agents
// for exactly that). The retail address is registered below so the wiring
// pass can find it the same way it finds every other slot-3 body.
//
// ---------------------------------------------------------------------------
// +0x60 AVL tree — node layout (0x30 bytes), verified byte-exact from the
// disassembly of 0x14000ebe0 (this field's insert/save body), 0x14000e360
// (AVL retrace), and 0x14000e580/0x14000e450 (the two single rotations):
//   +0x00 key (ComponentRef, 8B)   +0x08 value (int, low 32 bits only)
//   +0x10 parent   +0x18 left   +0x20 right
//   +0x28 balance (int16, right-height - left-height)   +0x2a height (int16)
// This is a SEPARATE compiler instantiation of the same AVL-tree template the
// project already ported once for MarketWork (marketwork_bst_a9f0.cpp) — same
// shape, different field offsets, so it needed its own port rather than reuse.
//
// The node's value scratch is never zero-initialized by retail before the
// int-serialize call that fills only its low 32 bits, so retail's node ends
// up with garbage high 32 bits that nothing ever reads back. We zero-init
// instead of replicating that specific piece of undefined behaviour.

#include "Component/tradecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// TradeComponent's own array-resize helpers (tradecomponent.cpp, file-scope,
// external linkage — not declared in tradecomponent_funcs.h).
void TradeComponent_ResizeNames(int64_t* field, int count);
void TradeComponent_ResizeItems(int64_t* field, int count);

namespace {

inline uint8_t* At(void* self, ptrdiff_t off) { return reinterpret_cast<uint8_t*>(self) + off; }

// True when the serializer is READING — same double-dereference pattern used
// by every other slot-3 body in this project (see animalwave_serialize.cpp).
inline bool Loading(void* ser) {
    return *reinterpret_cast<int32_t*>(*reinterpret_cast<uintptr_t*>(ser) + 8) == 0;
}

// The "enum" dispatch: the serializer's field+8 holds a live encoding object
// whose vtable slot +0x98 handles compact int/enum fields. Identical idiom to
// storagedescription.cpp's SerializeEnum, ported from the same retail shape.
inline void SerializeEnumField(void* ser, void* field) {
    void* serObj = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ser) + 8);
    if (!serObj) return;
    typedef void (*fn_t)(void* self, void* field, const char* name);
    void* vt = *reinterpret_cast<void**>(serObj);
    fn_t fn = *reinterpret_cast<fn_t*>(reinterpret_cast<uint8_t*>(vt) + 0x98);
    if (fn) fn(serObj, field, nullptr);
}

// ============================================================================
// +0x60 field — AVL tree of ComponentRef -> int
// ============================================================================
inline int16_t TradeBst_ChildHeightPlus1(uint8_t* child) {
    return child ? static_cast<int16_t>(*reinterpret_cast<int16_t*>(child + 0x2a) + 1) : 0;
}
inline void TradeBst_Recompute(uint8_t* n) {
    uint8_t* left  = *reinterpret_cast<uint8_t**>(n + 0x18);
    uint8_t* right = *reinterpret_cast<uint8_t**>(n + 0x20);
    int16_t lh = TradeBst_ChildHeightPlus1(left);
    int16_t rh = TradeBst_ChildHeightPlus1(right);
    *reinterpret_cast<int16_t*>(n + 0x28) = static_cast<int16_t>(rh - lh);  // balance
    *reinterpret_cast<int16_t*>(n + 0x2a) = (rh > lh) ? rh : lh;            // height
}

// 0x14000e580 — rotate RIGHT at `node` (node's LEFT child becomes the subtree root).
uint8_t* TradeBst_RotateRight(void* treeHdr, uint8_t* node) {
    uint8_t** rootSlot = reinterpret_cast<uint8_t**>(treeHdr);
    uint8_t* pivot = *reinterpret_cast<uint8_t**>(node + 0x18);
    if (node == *rootSlot) *rootSlot = pivot;
    if (uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x10)) {
        if (*reinterpret_cast<uint8_t**>(parent + 0x18) == node)
            *reinterpret_cast<uint8_t**>(parent + 0x18) = pivot;
        else
            *reinterpret_cast<uint8_t**>(parent + 0x20) = pivot;
    }
    *reinterpret_cast<uint8_t**>(pivot + 0x10) = *reinterpret_cast<uint8_t**>(node + 0x10);
    uint8_t* pivotRight = *reinterpret_cast<uint8_t**>(pivot + 0x20);
    *reinterpret_cast<uint8_t**>(node + 0x18) = pivotRight;
    if (pivotRight) *reinterpret_cast<uint8_t**>(pivotRight + 0x10) = node;
    *reinterpret_cast<uint8_t**>(pivot + 0x20) = node;
    *reinterpret_cast<uint8_t**>(node + 0x10) = pivot;

    if (uint8_t* pl = *reinterpret_cast<uint8_t**>(pivot + 0x18)) TradeBst_Recompute(pl);
    if (uint8_t* pr = *reinterpret_cast<uint8_t**>(pivot + 0x20)) TradeBst_Recompute(pr);
    TradeBst_Recompute(pivot);
    return pivot;
}

// 0x14000e450 — rotate LEFT at `node` (node's RIGHT child becomes the subtree root).
uint8_t* TradeBst_RotateLeft(void* treeHdr, uint8_t* node) {
    uint8_t** rootSlot = reinterpret_cast<uint8_t**>(treeHdr);
    uint8_t* pivot = *reinterpret_cast<uint8_t**>(node + 0x20);
    if (node == *rootSlot) *rootSlot = pivot;
    if (uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x10)) {
        if (*reinterpret_cast<uint8_t**>(parent + 0x18) == node)
            *reinterpret_cast<uint8_t**>(parent + 0x18) = pivot;
        else
            *reinterpret_cast<uint8_t**>(parent + 0x20) = pivot;
    }
    *reinterpret_cast<uint8_t**>(pivot + 0x10) = *reinterpret_cast<uint8_t**>(node + 0x10);
    uint8_t* pivotLeft = *reinterpret_cast<uint8_t**>(pivot + 0x18);
    *reinterpret_cast<uint8_t**>(node + 0x20) = pivotLeft;
    if (pivotLeft) *reinterpret_cast<uint8_t**>(pivotLeft + 0x10) = node;
    *reinterpret_cast<uint8_t**>(pivot + 0x18) = node;
    *reinterpret_cast<uint8_t**>(node + 0x10) = pivot;

    if (uint8_t* pl = *reinterpret_cast<uint8_t**>(pivot + 0x18)) TradeBst_Recompute(pl);
    if (uint8_t* pr = *reinterpret_cast<uint8_t**>(pivot + 0x20)) TradeBst_Recompute(pr);
    TradeBst_Recompute(pivot);
    return pivot;
}

// 0x14000e360 — AVL retrace/rebalance, walking from `startNode` up to the root.
void TradeBst_Retrace(void* treeHdr, uint8_t* startNode) {
    uint8_t* node = startNode;
    while (node) {
        uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x10);
        TradeBst_Recompute(node);

        int16_t bal = *reinterpret_cast<int16_t*>(node + 0x28);
        if (bal > 1) {
            uint8_t* right = *reinterpret_cast<uint8_t**>(node + 0x20);
            if (right) {
                int16_t rbal = *reinterpret_cast<int16_t*>(right + 0x28);
                if (rbal < 0) TradeBst_RotateRight(treeHdr, right);
                TradeBst_RotateLeft(treeHdr, node);
            }
        }
        bal = *reinterpret_cast<int16_t*>(node + 0x28);
        if (bal < -1) {
            uint8_t* left = *reinterpret_cast<uint8_t**>(node + 0x18);
            if (left) {
                int16_t lbal = *reinterpret_cast<int16_t*>(left + 0x28);
                if (lbal > 0) TradeBst_RotateLeft(treeHdr, left);
                TradeBst_RotateRight(treeHdr, node);
            }
        }
        node = parent;
    }
}

void* TradeBst_NewNode(uint64_t key, int64_t value, void* parent) {
    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
        ? fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8) : nullptr;
    if (!mem) return nullptr;
    uint8_t* n = reinterpret_cast<uint8_t*>(mem);
    *reinterpret_cast<uint64_t*>(n + 0x00) = key;
    *reinterpret_cast<int64_t*>(n + 0x08)  = value;
    *reinterpret_cast<void**>(n + 0x10)    = parent;
    *reinterpret_cast<void**>(n + 0x18)    = nullptr;
    *reinterpret_cast<void**>(n + 0x20)    = nullptr;
    *reinterpret_cast<int32_t*>(n + 0x28)  = 0;
    return mem;
}

// 0x14000ebe0 — the whole +0x60 field body: count-prefixed, then either an
// ordered insert-if-absent loop (loading) or an in-order walk of the existing
// tree (saving, no mutation).
void TradeComp_BstSerialize(void* treeHdr, void* ser) {
    int32_t count = *reinterpret_cast<int32_t*>(At(treeHdr, 0x08));
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
    if (!g_fn_Serialize_Int) return;

    if (Loading(ser)) {
        for (int32_t i = 0; i < count; ++i) {
            uint64_t key = 0;
            if (fn_ExternalPtr_BuildSerialize) fn_ExternalPtr_BuildSerialize(&key, ser);
            int64_t value = 0;
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(&value), nullptr);   // B.3.4 bridge

            uint8_t* root = *reinterpret_cast<uint8_t**>(treeHdr);
            if (!root) {
                void* node = TradeBst_NewNode(key, value, nullptr);
                *reinterpret_cast<void**>(treeHdr) = node;
                *reinterpret_cast<int32_t*>(At(treeHdr, 8)) += 1;
                continue;
            }

            uint8_t* cur = root;
            for (;;) {
                uint64_t curKey = *reinterpret_cast<uint64_t*>(cur);
                if (key < curKey) {
                    uint8_t* left = *reinterpret_cast<uint8_t**>(cur + 0x18);
                    if (!left) {
                        void* node = TradeBst_NewNode(key, value, cur);
                        *reinterpret_cast<void**>(cur + 0x18) = node;
                        TradeBst_Retrace(treeHdr, cur);
                        *reinterpret_cast<int32_t*>(At(treeHdr, 8)) += 1;
                        break;
                    }
                    cur = left;
                    continue;
                }
                if (curKey >= key) break;   // duplicate key: retail skips, no update
                uint8_t* right = *reinterpret_cast<uint8_t**>(cur + 0x20);
                if (!right) {
                    void* node = TradeBst_NewNode(key, value, cur);
                    *reinterpret_cast<void**>(cur + 0x20) = node;
                    TradeBst_Retrace(treeHdr, cur);
                    *reinterpret_cast<int32_t*>(At(treeHdr, 8)) += 1;
                    break;
                }
                cur = right;
            }
        }
        return;
    }

    // SAVE: in-order walk, no mutation.
    uint8_t* n = *reinterpret_cast<uint8_t**>(treeHdr);
    if (!n) return;
    while (uint8_t* l = *reinterpret_cast<uint8_t**>(n + 0x18)) n = l;
    while (n) {
        if (fn_ExternalPtr_BuildSerialize) fn_ExternalPtr_BuildSerialize(n, ser);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(n + 0x08), nullptr);   // B.3.4 bridge

        uint8_t* r = *reinterpret_cast<uint8_t**>(n + 0x20);
        if (r) {
            n = r;
            while (uint8_t* l = *reinterpret_cast<uint8_t**>(n + 0x18)) n = l;
        } else {
            uint8_t* p = *reinterpret_cast<uint8_t**>(n + 0x10);
            while (p && *reinterpret_cast<uint8_t**>(p + 0x18) != n) {
                n = p;
                p = *reinterpret_cast<uint8_t**>(p + 0x10);
            }
            n = p;
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// TradeStock_SeedForTest — TEST FIXTURE, not retail. See standalone/trade_buy_harness.cpp.
// ---------------------------------------------------------------------------
// The trade panel builds its SELL rows by walking this same +0x60 AVL tree and
// skipping any node whose value is 0, so a post whose tree is empty shows an empty
// sell table however much the town is holding -- which is exactly what an unattended
// run produces, because filling the tree is the player's job (they tick resources for
// trade and haulers deliver). With no rows there is nothing to sell, units-traded
// stays 0, and six achievements are unreachable.
//
// This inserts-or-updates one {resource, quantity} pair using the file's own AVL
// helpers, i.e. the same insert retail's load leg performs -- with one deliberate
// difference: retail's loader SKIPS a duplicate key (it is reading a stream it trusts
// to be unique), whereas a fixture called every visit has to be able to top a resource
// back up, so an existing key has its value overwritten.
extern "C" void TradeStock_SeedForTest(void* tradeComp, void* resourceEntity, int qty) {
    if (!tradeComp || !resourceEntity) return;
    void* treeHdr = At(tradeComp, 0x60);
    const uint64_t key = reinterpret_cast<uint64_t>(resourceEntity);

    uint8_t* root = *reinterpret_cast<uint8_t**>(treeHdr);
    if (!root) {
        void* node = TradeBst_NewNode(key, qty, nullptr);
        if (!node) return;
        *reinterpret_cast<void**>(treeHdr) = node;
        *reinterpret_cast<int32_t*>(At(treeHdr, 8)) += 1;
        return;
    }

    uint8_t* cur = root;
    for (;;) {
        const uint64_t curKey = *reinterpret_cast<uint64_t*>(cur);
        if (key == curKey) {
            *reinterpret_cast<int64_t*>(cur + 0x08) = qty;   // top up, see note above
            return;
        }
        uint8_t** childSlot = (key < curKey)
            ? reinterpret_cast<uint8_t**>(cur + 0x18)
            : reinterpret_cast<uint8_t**>(cur + 0x20);
        if (!*childSlot) {
            void* node = TradeBst_NewNode(key, qty, cur);
            if (!node) return;
            *childSlot = reinterpret_cast<uint8_t*>(node);
            TradeBst_Retrace(treeHdr, cur);
            *reinterpret_cast<int32_t*>(At(treeHdr, 8)) += 1;
            return;
        }
        cur = *childSlot;
    }
}


// ============================================================
// TradeComponent::Serialize (vtable slot [3], 0x14005bd50)
// ============================================================
extern "C" void TradeComponent_Serialize(TradeComponent* self, void* stream) {
    if (!self || !stream) return;
    if (fn_ComponentBase_Serialize) fn_ComponentBase_Serialize(self, stream);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int) return;

    // Retail 0x14005bd50 opens TWO Serializer scopes: an EMPTY one that is
    // constructed and immediately destroyed (the controller-section stub), and
    // only then the scope that carries the body.
    //
    //     Component_Serialize(param_1, param_2);
    //     IO::Serializer::Serializer(local_30, param_2, 0);   <- this one
    //     IO::Serializer::~Serializer(local_30);
    //     IO::Serializer::Serializer(&local_40, param_2, 0);  <- the body
    //
    // An empty scope is NOT free: it is 16 real bytes in the save encoding. This
    // port opened only the second, so it read 16 bytes too few at the very start
    // of the body -- every field below then came from the wrong offset, refCount
    // and nameCount decoded as poison, and TradeComponent_ResizeNames walked a
    // garbage array. Same trap as the two empty scopes in AbsorbResourceComponent
    // and HealerComponent.
    alignas(8) uint8_t serStub[24] = {0};
    ::new (static_cast<void*>(serStub)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serStub)->~Serializer();                                       // B.3.4 bridge

    alignas(8) uint8_t ser[24] = {0};
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);        // B.3.4 bridge

    // +0x50/+0x58: ComponentRef table, stride 0x10 {ComponentRef, int, int}.
    int32_t refCount = *reinterpret_cast<int32_t*>(At(self, 0x58));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(refCount, nullptr);   // B.3.4 bridge
    if (Loading(ser) && fn_SummaryArray_Init)
        fn_SummaryArray_Init(At(self, 0x50), refCount);
    {
        uint8_t* elem = *reinterpret_cast<uint8_t**>(At(self, 0x50));
        for (int32_t i = 0; i < refCount && elem; ++i, elem += 0x10) {
            if (fn_ExternalPtr_BuildSerialize) fn_ExternalPtr_BuildSerialize(elem + 0x0, ser);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(elem + 0x8), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(elem + 0xc), nullptr);   // B.3.4 bridge
        }
    }

    // +0x2c: selected-merchant index.
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(At(self, 0x2c)), nullptr);   // B.3.4 bridge

    // +0x60: AVL tree, ComponentRef -> int.
    TradeComp_BstSerialize(At(self, 0x60), ser);

    // +0x30/+0x38: trade-name array, stride 0x20.
    int32_t nameCount = *reinterpret_cast<int32_t*>(At(self, 0x38));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(nameCount, nullptr);   // B.3.4 bridge
    if (Loading(ser))
        TradeComponent_ResizeNames(reinterpret_cast<int64_t*>(At(self, 0x30)), nameCount);
    {
        uint8_t* elem = *reinterpret_cast<uint8_t**>(At(self, 0x30));
        for (int32_t i = 0; i < nameCount && elem; ++i, elem += 0x20) {
            reinterpret_cast<System::String*>(elem + 0x0)->Serialize(ser);   // B.3.1 bridge
            int32_t innerCount = *reinterpret_cast<int32_t*>(elem + 0x18);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(innerCount, nullptr);   // B.3.4 bridge
            // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8,
            // new tail zeroed. Element is the 8-byte ExternalPtr<ComponentDescription>.
            if (Loading(ser))
                GameColl::ArrayAt<GameColl::Ptr8>(elem, 0x10).SetSize(innerCount);
            if (fn_ExternalRefArray_Serialize)
                fn_ExternalRefArray_Serialize(ser, innerCount,
                    *reinterpret_cast<void**>(elem + 0x10), nullptr);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(elem + 0x8), nullptr);   // B.3.4 bridge
        }
    }

    // +0x40/+0x48: item table, stride 0xc {resourceId(enum), index, spare}.
    int32_t itemCount = *reinterpret_cast<int32_t*>(At(self, 0x48));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(itemCount, nullptr);   // B.3.4 bridge
    if (Loading(ser))
        TradeComponent_ResizeItems(reinterpret_cast<int64_t*>(At(self, 0x40)), itemCount);
    {
        uint8_t* elem = *reinterpret_cast<uint8_t**>(At(self, 0x40));
        for (int32_t i = 0; i < itemCount && elem; ++i, elem += 0xc) {
            SerializeEnumField(ser, elem + 0x0);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(elem + 0x8), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(elem + 0x4), nullptr);   // B.3.4 bridge
        }
    }

    // +0x28: plain int.
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int32_t*>(At(self, 0x28)), nullptr);   // B.3.4 bridge

    // +0x78: Time.
    if (g_fn_Time_Serialize) g_fn_Time_Serialize(At(self, 0x78), ser);

    // +0x71: bool.
    if (g_fn_Serialize_Bool) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(self, 0x71)), nullptr);   // B.3.4 bridge

    // +0x80: InternalBasePtr, validated against ComponentEntity.
    if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(At(self, 0x80))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    {
        void* obj = *reinterpret_cast<void**>(At(self, 0x80));
        if (obj && fn_IsDerivedFrom && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity &&
            !fn_IsDerivedFrom(obj, *g_TypeId_ComponentEntity))
            *reinterpret_cast<uint64_t*>(At(self, 0x80)) = 0;
    }

    // +0x70: bool.
    if (g_fn_Serialize_Bool) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(self, 0x70)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
