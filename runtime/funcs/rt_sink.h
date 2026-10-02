#pragma once
// ============================================================================
// runtime/funcs/rt_sink.h — the write sink ported Runtime bodies are written
// against.
// ============================================================================
//
// A ported body does not store directly; it routes every write through an
// `RtSink`:
//
//     void UpdateContainers(TabElement* self, RtSink& out) {
//         ...
//         out.U32(&child->flags, newFlags, "child.flags");
//     }
//
// and the caller runs it with
//
//     RtSink live;
//     UpdateContainers(self, live);
//
// The sink performs the store. `label` is carried for readability at the call
// site and is otherwise unused.
//
// This shape is a leftover of the predictive shadow oracle that verified these
// bodies against Runtime-x64.dll while the DLL was still loaded. The DLL is
// gone, the oracle with it, and the sink is now a thin pass-through. It stays
// only because the bodies are written against it; ENDGAME phase B rewrites them
// to plain stores and this header goes away with the last one.
//
// Deliberately NOT virtual: these calls sit in ported Runtime bodies that run
// per frame per element, and every one of them must stay a plain store.
// ============================================================================

#include <cstdint>
#include <cstddef>

class RtSink {
public:
    RtSink() {}

    // ---- Stores ------------------------------------------------------------
    // B.4 (R-CORE): each store takes the field it writes, typed; the store is
    // the plain assignment it always was.
    void U8 (uint8_t*  addr, uint8_t  v, const char* label) { (void)label; *addr = v; }
    void U16(uint16_t* addr, uint16_t v, const char* label) { (void)label; *addr = v; }
    void U32(uint32_t* addr, uint32_t v, const char* label) { (void)label; *addr = v; }
    void U64(uint64_t* addr, uint64_t v, const char* label) { (void)label; *addr = v; }
    template <class T>
    void Ptr(T** addr, T* v, const char* label) { (void)label; *addr = v; }

    // ---- Read-back ---------------------------------------------------------
    //
    // A body that stores to X and later re-reads X sees its own store. These
    // exist so such a re-read is spelled out at the call site; they are plain
    // loads.
    uint8_t  ReadU8 (const uint8_t*  addr) const { return *addr; }
    uint16_t ReadU16(const uint16_t* addr) const { return *addr; }
    uint32_t ReadU32(const uint32_t* addr) const { return *addr; }
    uint64_t ReadU64(const uint64_t* addr) const { return *addr; }
    template <class T>
    T*       ReadPtr(T* const* addr) const { return *addr; }

    // ---- Indirect call -----------------------------------------------------
    //
    // Returns true, meaning "make the call"; the body makes it:
    //
    //     if (out.Call("Element::SetBounds", slot, a, b, c, d))
    //         slot(child, a, b, c, d);
    bool Call(const char* label, const void* target,
              int64_t a0 = 0, int64_t a1 = 0, int64_t a2 = 0, int64_t a3 = 0) {
        (void)label; (void)target; (void)a0; (void)a1; (void)a2; (void)a3;
        return true;
    }
};
