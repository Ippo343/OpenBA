// ============================================================================
// runtime/src/video/video_interfaces_win.cpp — Wave 9 lane EVM
//   The plugin-loading half of the EnumerateVideoModes chain.
// ============================================================================
//
//   | Export                                                     | Retail      | Ord  |
//   |------------------------------------------------------------|-------------|------|
//   | ?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z | 0x1800f7b20 | 2542 |
//   | ?InitializeInterfaces@Device@Video@@QEAAXXZ                 | 0x1800f79f0 | 3073 |
//
// Both names were read out of the shipped export directory with pefile BEFORE
// a line of body was written, and both were grepped (name AND address) across
// code/, engine/, runtime/, standalone/ and dx11/ first — every hit was a
// comment, so neither was already ported under another name.
//
// WHY THESE TWO, AND WHY IN THIS LANE
// -----------------------------------
// `?EnumerateVideoModes@Device@Video@@QEAA_NXZ` (attempt 22's wall) opens with
// `CALL 0x1800f79f0` — its FIRST instruction after the prologue IS
// `InitializeInterfaces`. And `InitializeInterfaces`'s only non-KERNEL32 callee
// besides the String primitives is `GetDLLForInterface`. The three are one
// chain; porting the wall without them leaves two nulls one call deep.
//
// A SEPARATE _win.cpp because these are the only bodies in the chain that need
// <windows.h> (SetErrorMode / LoadLibraryW / GetProcAddress / FreeLibrary),
// and PLAN §4.1 decision 1 keeps the rest of runtime/src platform-neutral.
// All four are reached DIRECTLY by name against the exe's own KERNEL32 import,
// never through Runtime-x64.dll's IAT, which does not exist at cutover.
// The slots retail used were mapped with pefile rather than guessed:
//   0x18014b1b0 SetErrorMode   0x18014b1b8 LoadLibraryW
//   0x18014b1c0 GetProcAddress 0x18014b118 FreeLibrary
//
// THE POINT OF THE WHOLE THING: THE DX11 SLOT
// -------------------------------------------
// `standalone/video_interface.cpp` has been asking for this port in writing
// since Wave 9 lane ENGIO:
//
//     "`FillDx11Slot` is reachable ONLY from `WrapInitializeInterfaces`
//      (grepped -- there is no other caller), so with the detour gone NOBODY
//      FILLS THE DX11 INTERFACE SLOT AT ALL unless the ported
//      `Video::Device::InitializeInterfaces` calls `VideoInterfaceFillDx11Slot`
//      itself."
//
// This body calls it, first thing, which is exactly what the detour's wrapper
// does today (`FillDx11Slot(device); orig(device);`). That is the whole reason
// the pre-fill survives cutover, where there is no 0x1800f79f0 to detour.
//
// The detour stays installed and becomes redundant rather than wrong: it
// resolves its target RAW, so it still patches the DLL's copy, which nothing on
// our side reaches once these registrations are live.
// ============================================================================

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/widestring.h"
#include "video_interface.h"   // -Istandalone; this header is windows.h-free

using RtSystem::String;
using RtSystem::WideString;
using RtVideo::Device;
using RtVideo::InterfaceSlot;

namespace {

// The three plugin base names, read out of the shipped .rdata rather than
// retyped: 0x1801588c0 / 0x1801588d0 / 0x1801588e0, then the two suffixes at
// 0x1801588e8 and 0x1801588f0. So slot 1 is "VideoDX11-x64.dll".
const char kNameDX9[]  = "VideoDX9";
const char kNameDX11[] = "VideoDX11";
const char kNameGL[]   = "VideoGL";
const char kSuffixArch[] = "-x64";
const char kSuffixExt[]  = ".dll";

// The two GetProcAddress names, 0x1801588f8 and 0x180158910.
const char kProcCreateDeviceData[] = "CreateDeviceData";
const char kProcGetDeviceName[]    = "GetDeviceName";

constexpr int kSlotCount  = 3;
constexpr uintptr_t kSlot0 = 0xb0;

inline InterfaceSlot* SlotAt(Device* self, int i) {
    return reinterpret_cast<InterfaceSlot*>(
        reinterpret_cast<uint8_t*>(self) + kSlot0 + static_cast<uintptr_t>(i) * 0x18);
}

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// Retail's inlined `String::operator=(const String&)` at 0x1800f7b7a (and the
// two identical copies at 0x1800f7bc9 / 0x1800f7c16). Written out rather than
// folded into RtString_ReleaseInline + RtString_AdoptInline because the
// SELF-ASSIGNMENT GUARD comes FIRST and neither helper has it: with `old ==
// new` retail touches nothing at all, where release-then-adopt would decrement
// a refcount it is about to re-increment. Equivalent for a live string,
// different for one whose refcount is 1.
void StringAssign(String* dst, char* src) {
    char* const old = dst->data;
    if (old == src) return;                       // 0x1800f7b81 CMP / JZ
    if (old != nullptr) {                         // 0x1800f7b86 TEST / JZ
        if (--RtSystem::StringRefCount(old) != 0) {
            dst->data = nullptr;                  // 0x1800f7ba1 sharing branch
        } else {
            // 0x1800f7b93: `ADD qword [RDI],-8` then free THROUGH the slot.
            dst->data = old - 8;
            RtSystem::RtMemFree(dst->data);
        }
    }
    dst->data = src;                              // 0x1800f7ba4
    if (src != nullptr) ++RtSystem::StringRefCount(src);   // 0x1800f7bac
}

}  // namespace

// ============================================================================
// ?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z   0x1800f7b20
//                                                              ord 2542
// ============================================================================
//   String GetDLLForInterface(int index) const;
//
// RETURNS A `System::String` BY VALUE, so the Microsoft x64 ABI gives it a
// HIDDEN RETURN SLOT in RDX and pushes `index` out to R8. Getting that wrong is
// a wild write that still returns a plausible answer
// (reference_hidden_retslot_wild_write.md), so it is spelled out here:
//
//     RCX = this   (SPILLED AT ENTRY AND NEVER READ AGAIN — see below)
//     RDX = String* ret        (the hidden slot)
//     R8D = int index
//
// **`this` IS NEVER READ.** `MOV qword ptr [RSP+0x10],RDX` at 0x1800f7b20
// spills the RET SLOT, not RCX; RCX is dead from the first instruction. The
// function is a pure function of `index`, which is what makes it the one body
// in this lane with a perfect oracle. The parameter is kept for ABI fidelity
// and named so.
//
// THE DEFAULT CASE IS NOT AN ERROR PATH. `TEST R8D,R8D / DEC / DEC / JNZ` at
// 0x1800f7b4e..0x1800f7b5f sends any index outside 0..2 straight to the two
// appends at 0x1800f7c6c, SKIPPING both the assignment and the temporary's
// release. So `GetDLLForInterface(7)` returns **"-x64.dll"** — a name, not an
// empty string and not a failure. Reproduced exactly; the probe below tests it.
// ============================================================================
extern "C" String* RtVideo_Device_GetDLLForInterface(const Device* /*this, never read*/,
                                                     String* ret, int32_t index) {
    RT_DIFF_BEGIN("Video::Device::GetDLLForInterface");

    ret->data = nullptr;                          // 0x1800f7b44

    String tmp{nullptr};                          // the [RBP+0x38] local
    const char* base = nullptr;
    if (index == 0)      base = kNameDX9;         // 0x1800f7c01
    else if (index == 1) base = kNameDX11;        // 0x1800f7bb4
    else if (index == 2) base = kNameGL;          // 0x1800f7b65

    if (base != nullptr) {
        RtSystem::RtSystem_String_Set(&tmp, base);   // 0x1800c95d0
        StringAssign(ret, tmp.data);                 // the inlined operator=
        RtSystem::RtString_ReleaseInline(&tmp);      // 0x1800f7c4c
    }

    // 0x1800f7c6c and 0x1800f7ca2 — two `operator+=(const char*)` calls whose
    // RETURNED TEMPORARIES ARE DISCARDED. Retail materialises each into the
    // same [RBP+0x38] slot and releases it immediately; that is what the two
    // scratch Strings below are, and dropping them would leak a reference.
    {
        String discarded{nullptr};
        RtSystem::RtSystem_String_AppendAssignCStr(ret, &discarded, kSuffixArch);
        RtSystem::RtString_ReleaseInline(&discarded);
    }
    {
        String discarded{nullptr};
        RtSystem::RtSystem_String_AppendAssignCStr(ret, &discarded, kSuffixExt);
        RtSystem::RtString_ReleaseInline(&discarded);
    }

    if (rt_diff_on__) {
        // A REAL ORACLE, and the only one in this lane that needs no scaffolding:
        // the body is a pure function of `index` with no device, no COM and no
        // machine state. Run the shipped body into a scratch String and compare
        // the TEXT.
        using Fn = String* (*)(const Device*, String*, int32_t);
        Fn theirsFn = RtOriginal<Fn>(
            "?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z");
        if (theirsFn) {
            String theirs{nullptr};
            theirsFn(nullptr, &theirs, index);   // `this` is never read; null proves it
            const char* a = ret->data    ? ret->data    : "(null)";
            const char* b = theirs.data  ? theirs.data  : "(null)";
            const bool equal = std::strcmp(a, b) == 0;
            RT_DIFF_SHADOW(equal, a, b);
            // Effectiveness: an index that produced only the suffixes exercised
            // neither the switch nor the assignment. `worked` counts the calls
            // that actually named a plugin.
            RT_DIFF_WORK(base != nullptr);
            RtSystem::RtString_ReleaseInline(&theirs);
        } else {
            RT_DIFF_SHADOW(false, "ours ran", "shipped body did not resolve");
            RT_DIFF_WORK(true);
        }
    }
    return ret;                                   // 0x1800f7cd8 MOV RAX,RDI
}

// ============================================================================
// ?InitializeInterfaces@Device@Video@@QEAAXXZ                   0x1800f79f0
//                                                               ord 3073
// ============================================================================
//   for (int i = 0; i < 3; ++i) {
//       InterfaceSlot* s = this + 0xb0 + i*0x18;
//       if (s->module != nullptr) continue;                  // 0x1800f7a14
//       SetErrorMode(SEM_FAILCRITICALERRORS);                // 0x1800f7a23
//       String  name = GetDLLForInterface(i);                // 0x1800f7a34
//       WideString wide(name);                               // 0x1800f7a44
//       s->module = LoadLibraryW(wide.data);                 // 0x1800f7a4d
//       <release wide>
//       if (s->module) {
//           s->createDeviceData = GetProcAddress(s->module, "CreateDeviceData");
//           s->getDeviceName    = GetProcAddress(s->module, "GetDeviceName");
//           if (!s->createDeviceData || !s->getDeviceName) {
//               FreeLibrary(s->module);
//               s->module = s->getDeviceName = s->createDeviceData = nullptr;
//           }
//       }
//       <release name>
//   }
//
// FOUR THINGS THAT ARE RETAIL'S AND LOOK LIKE MISTAKES
// ----------------------------------------------------
//  1. **`SetErrorMode` is called INSIDE the loop and never restored.** Retail
//     sets SEM_FAILCRITICALERRORS (1) before each LoadLibraryW and leaves it
//     set for the life of the process — it does not save the previous mode and
//     does not put it back. That suppresses the OS "cannot find DLL" box for a
//     missing VideoGL-x64.dll. Transcribed, including the leak of the setting.
//  2. **The two GetProcAddress results are stored in the OPPOSITE order to the
//     field order**: `CreateDeviceData` into +0x10 first (0x1800f7a98), then
//     `GetDeviceName` into +0x08 (0x1800f7aac). Irrelevant to behaviour,
//     recorded because the struct in Device.h lists them the other way and the
//     next reader will otherwise think one of the two is wrong.
//  3. **The failure test reads the STORED field, not the register**:
//     `CMP qword [RBX+0x10],0` then `TEST RAX,RAX` where RAX still holds the
//     second call's result. Same answer, transcribed as written.
//  4. **A slot with a module but a null proc is fully reset**, including the
//     module word, after a FreeLibrary. So a half-usable plugin leaves no
//     trace, which is what lets the pre-fill's sentinel module be safe: a
//     sentinel that failed lookup would be FreeLibrary'd, which is why
//     video_interface.cpp takes a real extra reference on our own module
//     rather than fabricating a handle.
//
// ORACLE: none, and not for want of trying. The body's whole effect is
// LoadLibrary/GetProcAddress on the live process — a replay would load the same
// DLLs a second time, and the slot values it produced could not be compared to
// ours anyway because ours are deliberately DIFFERENT for slot 1 (our own
// functions, by design). What IS asserted is the postcondition that actually
// matters and that a bad port would break, checked against the pre-fill's own
// detector.
// ============================================================================
extern "C" void RtVideo_Device_InitializeInterfaces(Device* self) {
    RT_DIFF_BEGIN("Video::Device::InitializeInterfaces [no oracle: LoadLibrary on the live process]");

    // *** THE LINE THE TREE HAS BEEN ASKING FOR. ***
    // standalone/video_interface.cpp:158 and its DETOUR FAILED message both
    // name this call as the thing that must exist once this function is ported.
    // It fills slot 1 with our own CreateDeviceData/GetDeviceName so the loop
    // below SKIPS it — byte-for-byte what WrapInitializeInterfaces does today,
    // minus the detour that cannot exist at cutover.
    VideoInterfaceFillDx11Slot(self);

    int skipped = 0, loaded = 0, rejected = 0;

    for (int i = 0; i < kSlotCount; ++i) {            // 0x1800f7afd / 0x1800f7b03
        InterfaceSlot* const s = SlotAt(self, i);
        if (s->module != nullptr) {                   // 0x1800f7a14
            ++skipped;
            continue;
        }

        SetErrorMode(SEM_FAILCRITICALERRORS);         // 0x1800f7a23, ECX = 1

        String name{nullptr};
        RtVideo_Device_GetDLLForInterface(self, &name, i);   // 0x1800f7a34

        WideString wide{nullptr};
        RtSystem::RtSystem_WideString_CtorFromString(&wide, &name);  // 0x1800f7a44

        s->module = LoadLibraryW(wide.data);          // 0x1800f7a4d, RCX = *wide
        RtSystem::RtSystem_WideString_Dtor(&wide);    // 0x1800f7a56..0x1800f7a83

        if (s->module != nullptr) {                   // 0x1800f7a86
            s->createDeviceData = reinterpret_cast<void*>(GetProcAddress(
                static_cast<HMODULE>(s->module), kProcCreateDeviceData));   // 0x1800f7a92
            void* gotName = reinterpret_cast<void*>(GetProcAddress(
                static_cast<HMODULE>(s->module), kProcGetDeviceName));      // 0x1800f7aa6
            s->getDeviceName = gotName;
            if (s->createDeviceData == nullptr || gotName == nullptr) {     // 0x1800f7ab0
                FreeLibrary(static_cast<HMODULE>(s->module));               // 0x1800f7abf
                s->module           = nullptr;
                s->getDeviceName    = nullptr;
                s->createDeviceData = nullptr;
                ++rejected;
            } else {
                ++loaded;
            }
        }

        RtSystem::RtString_ReleaseInline(&name);      // 0x1800f7ad0..0x1800f7afd
    }

    if (rt_diff_on__) {
        // The postcondition a bad port would break, and the ONE thing this body
        // exists to guarantee: slot 1 carries OUR two functions and a non-null
        // module, so nothing will LoadLibrary VideoDX11-x64.dll.
        const InterfaceSlot* dx11 = SlotAt(self, 1);
        const bool dx11Ok = dx11->module != nullptr &&
                            dx11->createDeviceData != nullptr &&
                            dx11->getDeviceName != nullptr &&
                            VideoInterfacePrefilled();
        char a[160];
        std::snprintf(a, sizeof(a),
                      "skipped=%d loaded=%d rejected=%d dx11Prefilled=%d",
                      skipped, loaded, rejected, dx11Ok ? 1 : 0);
        RT_DIFF_SHADOW(dx11Ok, a,
                       "no oracle: a replay would LoadLibrary the same DLLs again, "
                       "and slot 1 is OURS by design so it could not match");
        // `skipped >= 1` is the effectiveness control: it is exactly the
        // assertion that the pre-fill happened BEFORE the loop looked at slot 1.
        // A run reporting skipped=0 means the fill did not take and the loop
        // went and loaded the real VideoDX11-x64.dll.
        RT_DIFF_WORK(skipped >= 1);
        std::printf("[EVM] InitializeInterfaces %s\n", a);
    }
}

// ============================================================================
// RtLaneEvmGetDllProbe — GetDLLForInterface over its whole input domain
// ============================================================================
// The live call site only ever passes 0 and 2 (slot 1 is pre-filled and
// `continue`s before reaching it), so the diff site above can never exercise
// index 1 or the default case. This probe closes both gaps against the shipped
// body, deterministically and with no device.
//
// EFFECTIVENESS CONTROL: `cases` and, separately, `named` — the number of
// inputs that produced a plugin name rather than the bare suffixes. A run with
// named < 3 has not covered the switch.
//
// NEGATIVE CONTROL: BANISHED_EVM_NEGCTL=1 appends the WRONG architecture suffix
// ("-x86"), which must make every one of the six cases diverge. It perturbs the
// produced STRING — the thing compared — not the return pointer, which is the
// same for both sides by construction.
// ============================================================================
extern "C" void RtLaneEvmGetDllProbe() {
    if (!RtDiffEnabled()) return;
    static bool done = false;
    if (done) return;
    done = true;

    using Fn = String* (*)(const Device*, String*, int32_t);
    Fn theirsFn = RtOriginal<Fn>(
        "?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z");
    if (!theirsFn) {
        std::printf("[EvmProbe] GetDLLForInterface SKIPPED: shipped body did not resolve\n");
        return;
    }
    const bool neg = EnvOn("BANISHED_EVM_NEGCTL");

    static const int32_t kCases[] = { 0, 1, 2, 3, -1, 0x7fffffff };
    int cases = 0, differing = 0, named = 0;
    char detail[320];
    detail[0] = 0;

    for (int32_t idx : kCases) {
        String ours{nullptr};
        String theirs{nullptr};
        RtVideo_Device_GetDLLForInterface(nullptr, &ours, idx);
        if (neg) {
            // Perturb the RESULT, which is what the comparison reads.
            String discarded{nullptr};
            RtSystem::RtSystem_String_AppendAssignCStr(&ours, &discarded, "-x86");
            RtSystem::RtString_ReleaseInline(&discarded);
        }
        theirsFn(nullptr, &theirs, idx);

        const char* a = ours.data   ? ours.data   : "(null)";
        const char* b = theirs.data ? theirs.data : "(null)";
        ++cases;
        if (idx >= 0 && idx <= 2) ++named;
        if (std::strcmp(a, b) != 0) {
            ++differing;
            char one[96];
            std::snprintf(one, sizeof(one), "idx=%d ours='%s' theirs='%s'; ", idx, a, b);
            std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
        }
        RtSystem::RtString_ReleaseInline(&ours);
        RtSystem::RtString_ReleaseInline(&theirs);
    }
    std::printf("[EvmProbe] GetDLLForInterface%s cases=%d named=%d differing=%d %s\n",
                neg ? " NEGATIVE-CONTROL" : "", cases, named, differing, detail);
}

// ============================================================================
// Registration
// ============================================================================
int RtVideoInterfacesRegister() {
    int n = 0;
    RegisterRuntimeExport("?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z",  // 2542
                          (void*)&RtVideo_Device_GetDLLForInterface); ++n;
    RegisterRuntimeAddr(0x1800f7b20, (void*)&RtVideo_Device_GetDLLForInterface);

    RegisterRuntimeExport("?InitializeInterfaces@Device@Video@@QEAAXXZ",                  // 3073
                          (void*)&RtVideo_Device_InitializeInterfaces); ++n;
    RegisterRuntimeAddr(0x1800f79f0, (void*)&RtVideo_Device_InitializeInterfaces);

    std::printf("[LaneEVM] rev=EVM-r1 interfaces registered=%d\n", n);
    return n;
}
