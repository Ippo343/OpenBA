#pragma once
// ============================================================================
// runtime/src/collection/array_destroy_internal.h — Wave 9 lane D2
// ============================================================================
//
// It exists so the probe drives the SAME bodies and the SAME DestroySpec rows
// the registered entry points drive. A probe that re-derived either would be
// testing a copy of the port rather than the port — the same reason lane Q's
// array_internal.h exists, and the reason both files are one header thick.
//
// THREE files include it, not the two this comment used to claim
// (WAVE 9 LANE SAVE2, corrected in place per PLAN §6 step 8):
// array_destroy.cpp, array_destroy_probe.cpp, and set_clear.cpp — the last for
// `ReleaseStringsCore` alone, because retail's `~Pair<String,String>` at
// 0x18000fa90 IS Body B with one element and the offsets {0x08, 0x00}. That is
// the reuse this header was written to make possible, so the third includer is
// the rule working, not an exception to it.
//
// The commentary — which address each row was read out of, why Body B's two
// arms leave the slot holding different things, and why 0x18001c8c0 is Body A
// and not a CRT dependency — is the banner of array_destroy.cpp.
// ============================================================================

#include <cstdint>

namespace rtcoll {

// `offs` is in RETAIL'S OWN ORDER, which is not always ascending. Three of the
// rows walk their offsets downward and the order reaches the allocator even
// though no comparator on the element buffer can see it (the slots do not
// alias). Reproduced, not sorted.
struct DestroySpec {
    const char* name;
    uint32_t    stride;
    int         nOffs;
    uint32_t    offs[3];
};

// BODY A — free the inner Array<T>'s buffer at element+off and clear its
// 16-byte header. The clear is unconditional, including when the buffer was
// already null.
void FreeInnerArraysCore(uint8_t* base, int count, const DestroySpec* s);

// BODY B — release one System::String reference per (element, offset).
// *** The two arms leave the slot holding DIFFERENT THINGS: NULL when the
// string survives, `str - 8` when it does not. *** That asymmetry is the only
// part of either body a comparator can see; see array_destroy.cpp.
void ReleaseStringsCore(uint8_t* base, int count, const DestroySpec* s);

// Collection::Array<T*>::operator=, retail 0x180003970. Exposed because the
// probe replays it (unlike the destructors, it has a comparable post-state).
void* RtColl_ArrayAssign_180003970(void* self, const void* src);

// ---- BODY A rows ----
extern const DestroySpec kD_InnerArr_110;    // 0x18001ce90
extern const DestroySpec kD_InnerArr_18;     // 0x1800419b0
extern const DestroySpec kD_InnerArr_40;     // 0x180052110
extern const DestroySpec kD_InnerArr_48;     // 0x180074100
extern const DestroySpec kD_InnerArr3_48;    // 0x18005bce0
extern const DestroySpec kD_InnerArr3_1f0;   // 0x18001c8c0's inlined eh_vector loop

// ---- BODY B rows ----
extern const DestroySpec kD_Str_08;          // 0x180011010
extern const DestroySpec kD_Str_10;          // 0x180005930
extern const DestroySpec kD_Str_48;          // 0x180014a40
extern const DestroySpec kD_Str_18;          // 0x180034440
extern const DestroySpec kD_Str2_38;         // 0x1800059e0

}  // namespace rtcoll

// Runs the synthetic-subject replay probe. Armed by RtCollectionDestroyPortInit
// and RUN on the first call into any registered entry point — never at init,
// where the DLL's MemoryAllocator is constructed but not started (lane Q's
// array_internal.h "Probe scheduling" note applies unchanged).
void RtCollectionDestroyProbeRun();
// True when BANISHED_RT_ARRAY_DESTROY_PROBE selects a mode. Read once, at init.
bool RtCollectionDestroyProbeArmed();
