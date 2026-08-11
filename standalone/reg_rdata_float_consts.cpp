// standalone/reg_rdata_float_consts.cpp
// .rdata float constants that code/ resolves by address but nothing registered.
//
// WHY THIS FILE EXISTS
// ResolveAddr(Module::Exe, <.rdata addr>) returns 0 unless the address is registered,
// and a null constant is SILENT. It cost this wave the entire crop-field harvest:
// 0x140172144 (0.99f, the crop maturity threshold) was unregistered, so state 10's
// per-frame returned on its constants guard -- before its own trace could print -- and
// BOTH harvest arms were dead. The symptom read as behavioural ("conditions never met")
// rather than structural, which is why it survived so long.
//
// A sweep of every 0x14017xxxx address that code/ passes to ResolveAddr found 25 more
// with no registration anywhere in standalone/. Two consumption patterns, dangerous in
// different ways:
//   - hard guard    `if (!p) return;`   -> feature inert, usually silent
//   - soft fallback `p ? *p : 0.0f`     -> WRONG VALUE, no error, no trace at all
// The second is worse. 0x1401722a8 is read exactly that way in the plant/hunt action
// chain (WorkAct_ShowToolCtor3's PerformWork heading): unregistered, it silently
// substitutes 0.0f for the real -1000.0f.
//
// VALUES ARE READ FROM THE BINARY, not inferred -- each entry is the literal 4-byte
// pattern at that address in Application-x64.exe, dumped via Ghidra and stored as its
// bit pattern so the float is exact. Cross-check: 0x1401722a8 decodes to -1000.0f,
// matching a comment in work_act_hunt.cpp written independently from disassembly.

#include "resolve.h"
#include <cstdint>

namespace {

struct RdataFloat { uint64_t addr; uint32_t bits; };

const RdataFloat g_consts[] = {
    { 0x1401720B0ULL, 0x3991A2B4U },  // 0.000277777785 1/3600 (one second of a game year)
    { 0x1401720B8ULL, 0x3A83126FU },  // 0.00100000005 0.001
    { 0x1401720BCULL, 0x3B800000U },  // 0.00390625   1/256
    { 0x1401720CCULL, 0x3C8EFA35U },  // 0.0174532924 pi/180, degrees -> radians
    { 0x1401720E4ULL, 0x3DCCCCCDU },  // 0.100000001  0.1
    { 0x14017210CULL, 0x3EA8F5C3U },  // 0.330000013  0.33
    { 0x140172120ULL, 0x3F2B851EU },  // 0.669999957  0.67
    { 0x14017213CULL, 0x3F7851ECU },  // 0.970000029  0.97
    { 0x140172148ULL, 0x3F7FBE77U },  // 0.999000013  0.999
    { 0x140172188ULL, 0x40600000U },  // 3.5          3.5
    { 0x14017219CULL, 0x40C00000U },  // 6            6
    { 0x1401721DCULL, 0x42200000U },  // 40           40
    { 0x1401721E4ULL, 0x42400000U },  // 48           48
    { 0x140172208ULL, 0x42C80000U },  // 100          100
    { 0x140172214ULL, 0x42FF0000U },  // 127.5        127.5
    { 0x140172228ULL, 0x43480000U },  // 200          200
    { 0x140172230ULL, 0x43800000U },  // 256          256
    { 0x140172238ULL, 0x43B40000U },  // 360          360 (full turn, degrees)
    { 0x140172240ULL, 0x44000000U },  // 512          512
    { 0x140172254ULL, 0x44E10000U },  // 1800         1800 (half a game year)
    { 0x140172264ULL, 0x46800000U },  // 16384        16384
    { 0x14017226CULL, 0xB8D1B717U },  // -9.99999975e-05 -1e-4
    { 0x1401722A0ULL, 0xC3340000U },  // -180         -180
    { 0x1401722A8ULL, 0xC47A0000U },  // -1000        -1000 (PerformWork "no heading" sentinel)
    { 0x1401722C0ULL, 0x80000000U },  // -0           -0.0 (sign-flip pattern)
};

constexpr int kCount = (int)(sizeof(g_consts) / sizeof(g_consts[0]));

// Storage the registered addresses point at. Held as uint32_t bit patterns and handed
// out by address, so a consumer casting to const float* reads exactly the exe's bytes.
uint32_t g_storage[kCount];

}  // namespace

void RegisterRdataFloatConsts() {
    for (int i = 0; i < kCount; ++i) {
        g_storage[i] = g_consts[i].bits;
        RegisterExeAddr((uintptr_t)g_consts[i].addr, (uintptr_t)&g_storage[i]);
    }
}
