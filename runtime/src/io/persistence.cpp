// ============================================================================
// runtime/src/io/persistence.cpp — Wave 4 lane C: IO::Persistence
// ============================================================================
// Ports the 5 of 7 `IO::Persistence` exports our tree calls that can be
// ported safely this wave: the two `Get` overloads, the two `Set` overloads,
// and `GetSavePath`. `ReadSettings`/`WriteSettings` are NOT ported — see
// docs/runtime_port/WAVE4_LANE_C.md for why (registry mutation with deletion
// logic, no safe oracle, and getting it wrong corrupts the player's real
// Windows registry). Both stay on the DLL exactly as before.
//
// All five retail bodies were decompiled at their real addresses (verified
// non-phantom against the shipped DLL's export table, see the wave doc) and
// are transcribed line-for-line below, including one documented deviation
// (see RetailValueIsTrue). PLAN §6 step 8: this file corrects nothing outside
// itself — see the wave doc's "comments to correct" section for two stale
// comments in engine_funcs.h this lane found but does not own.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../system/string.h"   // RtSystem::String, RtString_ReleaseInline (Wave 2, lane String)

using RtSystem::String;
using RtSystem::StringRefCount;
using RtIo::Persistence;
using RtIo::SettingsMapNode;

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// -----------------------------------------------------------------------------
// Same reasoning rt_port.h itself gives for declaring RegisterRuntimeExport /
// the raw export door locally: resolve.h drags in <windows.h>, and PLAN.md §4.1
// requires runtime/src/ to stay platform-neutral until a file is explicitly a
// `_win` twin. This declaration must stay byte-identical to resolve.h's; the
// two are checked against each other by the linker.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---- Map_Find / Map_Insert -------------------------------------------------
// See runtime/types/io/Persistence.h for why these are called by address
// rather than reimplemented: they are Collection::Set internals shared across
// the whole DLL, not something this lane owns.
using MapFindFn   = void* (*)(void* self, void* key);
using MapInsertFn = void* (*)(void* map, void* outNode, void* key, void* value);

void* CallMapFind(void* self, void* key) {
    static MapFindFn fn = reinterpret_cast<MapFindFn>(
        ResolveAddr(Module::Runtime, 0x18002ace0));   // Map_Find, retail
    return fn(self, key);
}
void* CallMapInsert(void* map, void** outNode, void* key, void* value) {
    static MapInsertFn fn = reinterpret_cast<MapInsertFn>(
        ResolveAddr(Module::Runtime, 0x18002a090));   // Map_Insert, retail
    return fn(map, outNode, key, value);
}

// ---- System::String::Format(retval, fmt, ...) ------------------------------
// ?Format@String@System@@SA?AV12@PEBDZZ  retail 0x1800c9730. Real varargs, so
// (like engine_funcs.cpp's own fn_String_Format) it is resolved as a plain
// typedef rather than through the raw DLL door's non-variadic Fn constraint.
// Resolved via ResolveAddr, not ResolveExport: Format is already ported by
// Wave 2 (lane String, runtime/src/system/string.cpp) and registering THAT
// port would mean this file's normal (non-diff) path silently depends on
// another lane's port being enabled — harmless in practice since the two are
// meant to agree, but this lane's job is to be correct standing alone against
// the DLL, so it always asks the DLL for its own dependency.
using FormatFn = void* (*)(void* retval, const char* fmt, ...);
FormatFn StringFormat() {
    static FormatFn fn = reinterpret_cast<FormatFn>(ResolveAddr(Module::Runtime, 0x1800c9730));
    return fn;
}

// Retail's key encoding: bare concatenation, no separator. "Game" + "StatusIconEnable"
// -> "GameStatusIconEnable", exactly the live HKCU value name this project's
// own memory note (project_debugmenu_combos_and_statusicon_setting.md) found
// on disk. Confirmed at all four Get/Set retail sites (0x1800b81e0 + 0x22,
// 0x1800b8290 + 0x22, 0x1800b80c0 + 0x30, 0x1800b8380 + 0x38): every one calls
// `String::Format((String*)&local, "%s%s", section, key)`.
String MakeSettingsKey(const char* section, const char* key) {
    String out{};
    StringFormat()(&out, "%s%s", section, key);
    return out;
}

// Retail's Get(bool&) (0x1800b8290) is an inlined strcmp against the literal
// "true": walk both strings until a mismatch or a NUL, then diff the bytes at
// the stopping position. That is exactly `strcmp(value, "true") == 0`.
//
// ONE DEVIATION, documented rather than silently "fixed": if the stored
// value's String has `data == nullptr` (only possible if something OTHER
// than Set(bool) — which always Formats a real, if zero-length, buffer —
// stored a null-data String as a bool setting's value), retail computes
// index -1 into the null pointer and reads `pcVar1[-1]`, an out-of-bounds
// read certain to fault. CLAUDE.md's "don't fix a faithful crash" rule is
// about retail RELYING on a value always being present; this is the reverse
// — an out-of-bounds read retail does not rely on, reachable by no known
// write path. Reproducing the exact byte read here would be equally
// undefined behaviour in our own binary, bought for a state nothing can
// currently create. Treated as false instead of replicated.
bool RetailValueIsTrue(const char* value) {
    if (value == nullptr) return false;
    return std::strcmp(value, "true") == 0;
}

}  // namespace

// ============================================================================
// bool IO::Persistence::Get(const char* section, const char* key, int& out)
// retail 0x1800b81e0 — full RT_DIFF oracle (read-only: Map_Find + a transient
// String::Format/MemFree pair, no persistent mutation).
// ============================================================================
bool RtIo_Persistence_GetInt(void* self, const char* section, const char* key, int* out) {

    String k = MakeSettingsKey(section, key);
    void* node = CallMapFind(self, &k);
    bool ours = (node != nullptr);
    int oursVal = *out;   // retail only ever writes *param_3 when found; preserve the caller's value otherwise
    if (ours) {
        auto* n = static_cast<SettingsMapNode*>(node);
        // FUN_18007f034 in retail is a bare tail-jump to CRT strtol(s, 0, 10)
        // — no need to call the DLL for it, this is not engine-specific.
        oursVal = static_cast<int>(std::strtol(n->value.data ? n->value.data : "", nullptr, 10));
    }
    RtSystem::RtString_ReleaseInline(&k);


    if (ours) *out = oursVal;
    return ours;
}

// ============================================================================
// bool IO::Persistence::Get(const char* section, const char* key, bool& out)
// retail 0x1800b8290 — full RT_DIFF oracle, same shape as Get(int&).
// ============================================================================
bool RtIo_Persistence_GetBool(void* self, const char* section, const char* key, bool* out) {

    String k = MakeSettingsKey(section, key);
    void* node = CallMapFind(self, &k);
    bool ours = (node != nullptr);
    bool oursVal = *out;
    if (ours) {
        auto* n = static_cast<SettingsMapNode*>(node);
        oursVal = RetailValueIsTrue(n->value.data);
    }
    RtSystem::RtString_ReleaseInline(&k);


    if (ours) *out = oursVal;
    return ours;
}

// ============================================================================
// void IO::Persistence::Set(const char* section, const char* key, int value)
// retail 0x1800b80c0 — MUTATES the shared live map, so "call ours, call
// theirs, compare" would insert twice (WARNING 3 in the brief). Instead: run
// ours for real, then verify with the DLL's OWN already-oracle-proven
// Get(int&) that the value it now sees round-trips. the diff oracle: EQUAL is
// our own verdict, computed from a real (read-only) call into the DLL.
// ============================================================================
void RtIo_Persistence_SetInt(void* self, const char* section, const char* key, int value) {

    String valStr{};
    StringFormat()(&valStr, "%i", value);
    String keyStr = MakeSettingsKey(section, key);

    void* outNode = nullptr;
    CallMapInsert(self, &outNode, &keyStr, &valStr);
    auto* node = static_cast<SettingsMapNode*>(outNode);

    // Retail's asymmetric release-then-adopt, transcribed exactly
    // (0x1800b80c0 + 0x50..0x90): Map_Insert only installs the value on a
    // BRAND NEW node (its own internal refcounting already left node->value
    // == valStr in that case, so this branch is skipped); on an EXISTING
    // node Map_Insert leaves the old value untouched and Persistence::Set
    // itself must swap it.
    if (node->value.data != valStr.data) {
        RtSystem::RtString_ReleaseInline(&node->value);
        node->value.data = valStr.data;
        if (valStr.data) ++StringRefCount(valStr.data);
    }
    RtSystem::RtString_ReleaseInline(&keyStr);
    RtSystem::RtString_ReleaseInline(&valStr);

}

// ============================================================================
// void IO::Persistence::Set(const char* section, const char* key, bool value)
// retail 0x1800b8380 — same shape as Set(int), value formatted as "true"/"false".
// ============================================================================
void RtIo_Persistence_SetBool(void* self, const char* section, const char* key, bool value) {

    String valStr{};
    StringFormat()(&valStr, "%s", value ? "true" : "false");
    String keyStr = MakeSettingsKey(section, key);

    void* outNode = nullptr;
    CallMapInsert(self, &outNode, &keyStr, &valStr);
    auto* node = static_cast<SettingsMapNode*>(outNode);

    if (node->value.data != valStr.data) {
        RtSystem::RtString_ReleaseInline(&node->value);
        node->value.data = valStr.data;
        if (valStr.data) ++StringRefCount(valStr.data);
    }
    RtSystem::RtString_ReleaseInline(&keyStr);
    RtSystem::RtString_ReleaseInline(&valStr);

}

// ============================================================================
// System::String IO::Persistence::GetSavePath() const
// retail 0x180007ad0 — full RT_DIFF oracle. Pure read (a ref-counted copy of
// a field) with no mutation, so both sides can run for real; only the
// oracle's own temporary copy needs releasing afterward.
// ============================================================================
void* RtIo_Persistence_GetSavePath(void* self, void* retval) {

    auto* p = static_cast<Persistence*>(self);
    auto* ret = static_cast<String*>(retval);
    ret->data = nullptr;
    if (p->savePath.data) {
        ret->data = p->savePath.data;
        ++StringRefCount(ret->data);
    }


    return retval;
}

// ============================================================================
// Registration. ReadSettings/WriteSettings are DELIBERATELY absent — see
// docs/runtime_port/WAVE4_LANE_C.md.
// ============================================================================
int RtIoPersistenceRegister() {
    int n = 0;

    RegisterRuntimeExport("?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z",
                          (void*)&RtIo_Persistence_GetInt); ++n;
    RegisterRuntimeExport("?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z",
                          (void*)&RtIo_Persistence_GetBool); ++n;
    RegisterRuntimeExport("?Set@Persistence@IO@@QEAAXPEBD0H@Z",
                          (void*)&RtIo_Persistence_SetInt); ++n;
    RegisterRuntimeExport("?Set@Persistence@IO@@QEAAXPEBD0_N@Z",
                          (void*)&RtIo_Persistence_SetBool); ++n;
    RegisterRuntimeExport("?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ",
                          (void*)&RtIo_Persistence_GetSavePath); ++n;

    // Address fallbacks too (WAVE3_LANE_A.md §6's lesson: engine_funcs.cpp
    // falls back to ResolveAddr(Module::Runtime, ...) on a failed name
    // resolve for Set(bool)/Set(int)/GetSavePath already — registering only
    // the names would leave that fallback path silently on the DLL).
    RegisterRuntimeAddr(0x1800b81e0, (void*)&RtIo_Persistence_GetInt);
    RegisterRuntimeAddr(0x1800b8290, (void*)&RtIo_Persistence_GetBool);
    RegisterRuntimeAddr(0x1800b80c0, (void*)&RtIo_Persistence_SetInt);
    RegisterRuntimeAddr(0x1800b8380, (void*)&RtIo_Persistence_SetBool);
    RegisterRuntimeAddr(0x180007ad0, (void*)&RtIo_Persistence_GetSavePath);

    // CORRECTED 2026-09-08 (Wave 9 lanes BOOT2 and SAVE2). This line used to
    // read "(ReadSettings/WriteSettings held back -- see WAVE4_LANE_C.md)".
    // BOTH are ported now, in persistence_registry_win.cpp, and neither was
    // ever counted by this `n` -- so the message named a holdback that had
    // stopped being one AND attributed it to the wrong denominator.
    printf("[RtPort] IO::Persistence: %d/7 exports registered here; "
           "ReadSettings + WriteSettings + Registry::Registry are registered by "
           "persistence_registry_win.cpp\n", n);
    return n;
}
