// code/Game/Dialog/tradeui_buyentries.cpp
// Wave A, last leaf — 0x140059670 fn_Building_GetBuyEntries.
//
// Fills `out` (an 8-byte-stride dynamic array) with every entity the docked merchant
// will deal in. It feeds the buy table directly, so a wrong port produces a
// POPULATED-but-wrong panel — not an empty one and not a crash. That is why the
// control flow below was taken from the DISASSEMBLY rather than the decompile.
//
// ── The `joined_r0x000140059767` back-edge is NOT an infinite loop ────────────
// Ghidra renders the ExternalFactory walk as a `joined_` label with a bare
// `if (x == 0) return;` at the top and a `goto` at the bottom, which reads like a
// spin. The disassembly at 0x140059740 shows what it really is:
//
//   140059740: MOV RAX, [0x1401529d8]      ; the ExternalFactory singleton slot
//   140059747: MOV EDX, [0x1401c7318]      ; typeId = ComponentDescription
//   14005974d: XOR R8D, R8D                ; cursor = null  -> "give me the first"
//   140059750: MOV R12, [RAX]              ; factory = *singleton
//   14005975b: CALL [0x1401514d0]          ; FindNext(factory, typeId, cursor)
//   140059761: MOV RBP, RAX
//   140059764: TEST RAX, RAX
//   140059767: JZ  <exit>                  ; <== the "joined_" target
//
// It is the standard cursor iteration: seed with null, advance with the previous
// entity, stop on null. The second `CALL [0x1401514d0]` at 0x1400598f4 is the
// advance.
//
// ⚠ AND `0x1401514d0` IS NOT `fn_Sprite_IsValid`. Ghidra labels both call sites as
// that and types the result as `bool`, which is what makes the loop look degenerate.
// It is `ExternalFactory::FindNext(self, typeId, prev) -> void*` — already in the
// tree as `fn_ExternalFactory_FindNext`, with the exact RCX/EDX/R8 signature the
// disassembly shows. The dependency list this leaf was handed off with named
// `fn_Sprite_IsValid`; that came from the bad label and no sprite is involved.
//
// ── The merchant record (M = *(tradeDesc+0x48) + tradeState[+0x2c] * 0x48) ────
//   M + 0x10  uint32  resource-category mask for the raw-material match
//   M + 0x20  void**  the merchant's own goods list      | count at M + 0x28
//   M + 0x30  int32*  the merchant's select-type id list | count at M + 0x38
// The +0x20/+0x28 and +0x30/+0x38 pairs are independently corroborated by the
// existing dump in standalone/trade_probe.cpp, which prints them as "goods" and
// "ints" respectively.
//
// ── Three ways an entity gets into the buy list ──────────────────────────────
//   1. It is in the merchant's own goods list          (unconditional)
//   2. Its RawMaterialDescription flags at +0x5c hit the M+0x10 mask
//   3. Its SelectDescription id at +0x40 equals one of the M+0x30 entries
// Rules 2 and 3 are evaluated for EVERY ComponentDescription in the factory, and an
// entity can be appended more than once — rule 3 appends once per matching entry in
// the id list. That duplication is retail behaviour, not a bug in the transcription.
//
// ── Dead scans, preserved as a comment rather than as code ───────────────────
// Retail scans each entity's components for ValueDescription at three separate
// points and then throws the result away — nothing reads it, and the appended value
// is always the entity itself. Those scans call only the vtable type-id getter and
// IsDerivedFrom, both pure queries with no side effects, so omitting them is not
// observable. They are the residue of an inlined "find" whose consumer was optimised
// out. Noted here so the next reader does not go looking for the missing use.
#include "resolve.h"
#include "engine_funcs.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>

namespace {

// The 8-byte-stride dynamic array header the out-param uses:
//   +0x00 void** data   +0x08 int32 count   +0x0c int32 capacity
struct PtrArray {
    void**  data;
    int32_t count;
    int32_t cap;
};

// Append one pointer. This is FUN_14000ee40 specialised to newCount = count + 1,
// which is how retail inlines it at both append sites. Identical growth rule to
// AddPurchaseOrder in tradeui_leaves.cpp: capacity = newCount, then newCount * 3
// when the array was non-empty; MemAlloc(cap * 8, 8) via the STATIC two-arg form.
//
// B.3.2 NOT CONVERTED to Collection::Array<T>::SetSize, same reason as
// AddPurchaseOrder's append in tradeui_leaves.cpp: the `if (!grown)` arm below
// restores count and cap and returns, and SetSize has no failure path — it
// commits the new size before allocating, exactly as retail does. That guard is
// standalone-only (retail never checks) and dropping it to fit the template
// would trade a bail-out for a wild write.
void AppendPtr(PtrArray* a, void* value) {
    if (!a) return;
    const int32_t oldCount = a->count;
    const int32_t oldCap   = a->cap;
    const int32_t newCount = oldCount + 1;
    void** oldData = a->data;
    a->count = newCount;
    if (a->cap < newCount) {
        a->cap = newCount;
        if (oldCount != 0) a->cap = newCount * 3;
        void** grown = fn_MemAllocStatic
            ? static_cast<void**>(fn_MemAllocStatic(
                  static_cast<uint64_t>(a->cap) * 8, 8))
            : nullptr;
        if (!grown) {                        // standalone-only; retail never checks
            a->count = oldCount;
            a->cap   = oldCap;
            return;
        }
        a->data = grown;
    }
    for (int32_t i = (oldData == a->data) ? oldCount : 0; i < newCount; ++i) {
        a->data[i] = (i < oldCount) ? oldData[i] : value;
    }
    if (oldData != a->data && fn_MemFreeStatic) fn_MemFreeStatic(oldData);
}

// Find the first component of `typeId` on an entity, confirming with IsDerivedFrom.
// Entity layout: component pointer array at +0x28, count at +0x30; type id from
// vtable slot +0x10. Retail stops at the FIRST id match and yields null if the
// confirmation fails, rather than continuing the scan.
void* FindComp(const void* entity, uint32_t typeId) {
    if (!entity) return nullptr;
    const uint8_t* e = reinterpret_cast<const uint8_t*>(entity);
    const int32_t count = *reinterpret_cast<const int32_t*>(e + 0x30);
    void* const* arr = *reinterpret_cast<void* const* const*>(e + 0x28);
    if (!arr || count <= 0) return nullptr;
    for (int32_t i = 0; i < count; ++i) {
        void* comp = arr[i];
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        GetTypeId_t getId = reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10));
        if (!getId || getId(comp) != typeId) continue;
        if (fn_IsDerivedFrom && !fn_IsDerivedFrom(comp, typeId)) return nullptr;
        return comp;
    }
    return nullptr;
}

} // namespace

extern "C" void TradeUI_GetBuyEntries_59670(void* tradeState, void* outArray) {
    PtrArray* out = static_cast<PtrArray*>(outArray);
    if (!out) return;

    // Retail opens with FUN_140001bb0(out, 0) — the stride-8 resize-to-zero. Its only
    // observable effect is count = 0; the buffer and capacity are retained.
    out->count = 0;
    if (!tradeState) return;

    const uint8_t* ts = reinterpret_cast<const uint8_t*>(tradeState);
    void* tradeDesc = *reinterpret_cast<void* const*>(ts + 0x18);
    if (tradeDesc && g_TypeInfo_TradeDescription && fn_IsDerivedFrom &&
        !fn_IsDerivedFrom(tradeDesc, *g_TypeInfo_TradeDescription)) {
        tradeDesc = nullptr;
    }
    if (!tradeDesc) return;                  // retail would deref null

    const int32_t merchantIdx = *reinterpret_cast<const int32_t*>(ts + 0x2c);
    const uintptr_t merchants = *reinterpret_cast<const uintptr_t*>(
        reinterpret_cast<uint8_t*>(tradeDesc) + 0x48);
    if (!merchants || merchantIdx < 0) return;
    const uint8_t* M = reinterpret_cast<const uint8_t*>(
        merchants + static_cast<uintptr_t>(merchantIdx) * 0x48);

    // ── 1. Everything in the merchant's own goods list ────────────────────────
    void* const* goods    = *reinterpret_cast<void* const* const*>(M + 0x20);
    const int32_t goodsN  = *reinterpret_cast<const int32_t*>(M + 0x28);
    if (goods) {
        for (int32_t i = 0; i < goodsN; ++i) AppendPtr(out, goods[i]);
    }

    // ── 2 & 3. Walk every ComponentDescription the ExternalFactory knows ──────
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    if (!fn_ExternalFactory_FindNext || !g_TypeId_ComponentDescription_FC) return;
    void* factory = *g_pExternalFactorySingleton;
    const uint32_t tidComponentDesc = *g_TypeId_ComponentDescription_FC;

    const uint32_t rawMatMask = *reinterpret_cast<const uint32_t*>(M + 0x10);
    const int32_t* selectIds  = *reinterpret_cast<const int32_t* const*>(M + 0x30);
    const int32_t  selectN    = *reinterpret_cast<const int32_t*>(M + 0x38);

    const ::Object::ExternalFactory* ebuyFactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    void* cur = const_cast<::Object::External*>(
        ebuyFactory->FindNext(tidComponentDesc, nullptr));              // B.3.3 bridge
    while (cur) {
        void* desc = cur;
        if (fn_IsDerivedFrom && !fn_IsDerivedFrom(cur, tidComponentDesc)) desc = nullptr;

        if (desc) {
            // 2. Raw-material category match against the merchant's M+0x10 mask.
            void* rawMat = g_TypeId_RawMaterialDescription
                ? FindComp(desc, *g_TypeId_RawMaterialDescription) : nullptr;
            if (rawMat) {
                const uint32_t flags = *reinterpret_cast<const uint32_t*>(
                    reinterpret_cast<uint8_t*>(rawMat) + 0x5c);
                if ((rawMatMask & flags) != 0) AppendPtr(out, desc);
            }
        }

        // Retail fetches the NEXT entity here, before the select-id pass, and only
        // uses it at the bottom of the loop. Order preserved: FindNext is an engine
        // call and moving it could change what the iterator observes if the pass
        // below ever mutates the factory.
        void* next = const_cast<::Object::External*>(
            ebuyFactory->FindNext(tidComponentDesc,
                                  reinterpret_cast<const ::Object::External*>(cur)));  // B.3.3 bridge

        if (desc && selectIds) {
            // 3. Select-type id match. Note the append is INSIDE the id loop, so an
            // entity whose id appears more than once in the merchant's list is added
            // more than once — retail behaviour, deliberately not de-duplicated.
            void* sel = g_TypeId_SelectDescription
                ? FindComp(desc, *g_TypeId_SelectDescription) : nullptr;
            if (sel) {
                const int32_t selId = *reinterpret_cast<const int32_t*>(
                    reinterpret_cast<uint8_t*>(sel) + 0x40);
                for (int32_t k = 0; k < selectN; ++k) {
                    if (selId == selectIds[k]) AppendPtr(out, desc);
                }
            }
        }

        cur = next;
    }
}
