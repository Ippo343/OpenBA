// ============================================================================
// runtime/src/engine/updateloop_loadresources.cpp — Wave 23 lane ENGINE
// ?LoadResources@UpdateLoop@Engine@@QEAAXXZ   retail 0x1800dd7d0, ord 3210
// ============================================================================
//
// WHY THIS BODY IS WORTH MORE THAN ONE CENSUS ROW. Wave 22 lane ERRCFG found
// that this function plus one eight-byte accessor are the COMPLETE set of
// still-unported functions that READ the exported static
// `?_singleton@Configuration@System@@0PEAV12@EA` (0x1801fc6f8). Lane ENGINE
// RE-DERIVED that claim rather than inheriting it — all 8 xrefs walked, READ
// split from WRITE at the instruction, plus a raw-file scan of the DLL for the
// RVA and the VA to close the "computed address / second alias" gap ERRCFG had
// left open. The claim holds exactly as written. Evidence:
// docs/runtime_port/WAVE23_LANE_ENGINE.md §2.
//
// The reader set was:
//     0x1800abc8b  FileSystem::Startup            READ   ported + registered
//     0x1800abd2e  FileSystem::SetupDirectories   READ   ported + registered
//     0x1800ddbe0  UpdateLoop::LoadResources      READ   <- this file
//     0x18000da70  Configuration::Get             READ   8 bytes, uncalled
//     0x1800c51e0  Configuration::Configuration   WRITE  ported
//     0x1800db954  Engine::Startup                WRITE  ported
//
// So this file removes one of the two remaining readers, and it does it
// THROUGH THE PORT-AWARE DATA DOOR (`RT_DATA_SLOT`) — which is the whole point.
// A port that read the raw DLL address would leave the row exactly as blocked
// as before while looking like progress.
//
// ---------------------------------------------------------------------------
// WHAT THIS DOES **NOT** DISCHARGE — say it before someone quotes the above
// ---------------------------------------------------------------------------
// Promotion of that DATA row is ROUTE-SCOPED and this body only moves the
// CUTOVER half of it:
//   * AT CUTOVER there is no DLL code, so once `Configuration::Get` is also
//     ported the reader set is entirely ours and the row becomes promotable.
//   * WITH THE DLL PRESENT the DLL's own `Configuration::Get` at 0x18000da70
//     still reads 0x1801fc6f8 through a RIP-relative operand baked into .text,
//     and any in-DLL rel32 caller of it reaches that, not us. Porting our body
//     cannot intercept an in-DLL call. So promoting the row today still
//     strands those readers.
// Lane ENGINE therefore hands lane ALLOC / the System namespace owner a
// CONDITIONAL, not a green light. See the lane doc §2.3.
//
// ---------------------------------------------------------------------------
// THE ARGUMENT GHIDRA DROPS, AND WHY IT MATTERS HERE
// ---------------------------------------------------------------------------
// Ghidra's C prints the command-line lookup as
//     GetCommandLineString(Configuration::_singleton, &ret);
// TWO arguments. The disassembly at 0x1800ddbd8 is
//     LEA R8,[RBP+0x48]     ; <- the KEY string, dropped by the decompiler
//     LEA RDX,[RBP+0x60]    ; <- the hidden return slot
//     MOV RCX,[0x1801fc6f8]
//     CALL 0x1800c5300
// THREE. A typedef that omits either the return slot or the key is a wild
// write that still returns a plausible answer. Both are spelled out below.
//
// Kill switch: RT_ENGINELOADRES=0.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/engine/UpdateLoop.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"
#include "../system/widestring.h"

// Declared here the way runtime/src/io/filesystem_setupdirs_win.cpp declares
// it: `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) is variadic, so it
// is not in string.h's fixed-signature list.
namespace RtSystem {
String* RtSystem_String_Format(String* ret, const char* fmt, ...);   // 0x1800c9730
}

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::WideString;
using RtSystem::StringLength;
using RtSystem::StringRefCount;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Set;
using RtSystem::RtSystem_WideString_CtorFromString;
using RtSystem::RtSystem_String_Format;

// The platform seam — runtime/src/engine/engine_platform_win.cpp keeps
// <windows.h> out of this file (PLAN.md §4.1 decision 1). Retail's call is
// `GetFileAttributesW(w.data) != INVALID_FILE_ATTRIBUTES` at 0x1800dd9c4.
bool RtEnginePlatformWidePathExists(const wchar_t* path);

namespace {

typedef const char* (*GetResourceFilenameFn)(void*);
typedef const void* (*FindOrLoadFn)(void* factory, const Filename* name);
typedef bool        (*IsDerivedFromFn)(const void* base, uint32_t typeId);
typedef Filename*   (*GetPathFilenameFn)(const Filename* self, Filename* ret, bool);
typedef Filename*   (*GetCompiledPathFn)(Filename* ret, const Filename* src);
typedef bool        (*ExistsInPackfileFn)(const void* fs, const Filename* name);
typedef String*     (*GetCommandLineStringFn)(const void*, String*, const String*);
typedef void        (*AdjustDelimitersFn)(Filename*);

// ---- the exported DATA this body reads, all through the port-aware door ----
void* ExternalFactorySingleton() {
    void** s = RT_DATA_SLOT(void**, "?_singleton@ExternalFactory@Object@@0PEAV12@EA");
    return s ? *s : nullptr;
}
uint8_t* ModificationSingleton() {
    uint8_t** s = RT_DATA_SLOT(uint8_t**, "?_singleton@Modification@IO@@2PEAV12@EA");
    return s ? *s : nullptr;
}
uint8_t* FileSystemSingleton() {
    uint8_t** s = RT_DATA_SLOT(uint8_t**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return s ? *s : nullptr;
}
// THE ROW THIS FILE IS ABOUT. Through RT_DATA_SLOT on purpose.
void* ConfigurationSingleton() {
    void** s = RT_DATA_SLOT(void**, "?_singleton@Configuration@System@@0PEAV12@EA");
    return s ? *s : nullptr;
}
// `MOV EDX,dword ptr [0x1801f9be0]` — BaseTypeInfo's FIRST dword is the typeId.
// Retail reads +0x00 and nothing else, which is the whole content of
// reference_basetypeinfo_name_is_content_only.md.
uint32_t ExternalListTypeId() {
    uint32_t* s = RT_DATA_SLOT(uint32_t*, "?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A");
    return s ? *s : 0u;
}

void AdjustDelimiters(Filename* f) {
    RT_EXPORT_ONCE(AdjustDelimitersFn,
        "?AdjustDelimiters@Filename@System@@AEAAXXZ")(f);
}

// The idiom retail inlines at every one of these sites: take a second owning
// reference to another String's buffer. Written out rather than delegated
// because that is what the five occurrences in this function do
// (0x1800dd815, 0x1800dd95a, 0x1800dda58, 0x1800ddc4a and the loop copy).
void AddRefInto(String* dst, const String* src) {
    dst->data = nullptr;
    if (src->data) {
        StringRefCount(src->data) += 1;
        dst->data = src->data;
    }
}

// 0x1800dd978-0x1800dd9fa. "Is this compiled resource reachable at all?" —
// packfile first, loose file second, and the two FileSystem bytes at +0x18 and
// +0x19 gate them independently.
bool CompiledResourceExists(const Filename* compiled) {
    uint8_t* fs = FileSystemSingleton();
    if (fs == nullptr) return false;

    if (fs[0x18] != 0) {
        if (RT_EXPORT_ONCE(ExistsInPackfileFn,
                "?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z")(
                    fs, compiled))
            return true;
    }
    if (fs[0x19] == 0) return false;

    WideString w{nullptr};
    RtSystem_WideString_CtorFromString(&w, reinterpret_cast<const String*>(compiled));
    const bool exists = RtEnginePlatformWidePathExists(w.data);
    RtString_ReleaseInline(reinterpret_cast<String*>(&w));
    return exists;
}

// 0x1800dd850 / 0x1800ddc6f / 0x1800dda9a. Retail calls IsDerivedFrom and
// THROWS THE ANSWER AWAY at two of the three sites — the inlined
// `External::Cast<ExternalList>` whose result nothing reads. Transcribed as
// written: it is a virtual-free call today, but a later build could make it
// observable, and "simplifying" it away is exactly the class of edit that
// turns a faithful port into a different function.
const void* FindAndCheck(const Filename* name, bool keep) {
    const void* ext = RT_EXPORT_ONCE(FindOrLoadFn,
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z")(
            ExternalFactorySingleton(), name);
    if (ext == nullptr) return nullptr;
    const bool derived = RT_EXPORT_ONCE(IsDerivedFromFn,
        "?IsDerivedFrom@Base@Object@@QEBA_NI@Z")(ext, ExternalListTypeId());
    if (!keep) return nullptr;
    return derived ? ext : nullptr;
}

unsigned g_calls     = 0;
unsigned g_ownList   = 0;   // the loop's own Resources.rsc was found
unsigned g_modsSeen  = 0;   // enabled mods walked
unsigned g_modsFound = 0;   // ... of which had a compiled Resources.rsc
unsigned g_refUsed   = 0;   // the `-ref` command-line override fired

}  // namespace

// ============================================================================
// ?LoadResources@UpdateLoop@Engine@@QEAAXXZ   retail 0x1800dd7d0
// ============================================================================
extern "C" void RtEngine_UpdateLoop_LoadResources(RtEngine::UpdateLoop* self) {
    ++g_calls;

    // ---- 0x1800dd7ef: the loop's OWN resource list ------------------------
    // vtable slot 10, GetResourceFilename. The base returns null, which is why
    // standalone/gameupdateloop_stubs.cpp's default null makes this block a
    // no-op rather than a crash.
    void** vt = *reinterpret_cast<void***>(self);
    const char* root = reinterpret_cast<GetResourceFilenameFn>(vt[10])(self);
    if (root != nullptr) {
        String spec{nullptr};
        RtSystem_String_Format(&spec, "%sResources.rsc", root);

        String name{nullptr};
        AddRefInto(&name, &spec);
        AdjustDelimiters(reinterpret_cast<Filename*>(&name));

        const void* list = FindAndCheck(reinterpret_cast<const Filename*>(&name), true);
        self->_resourceList = const_cast<void*>(list);
        if (list) ++g_ownList;

        RtString_ReleaseInline(&name);
        RtString_ReleaseInline(&spec);
    }

    // ---- 0x1800dd8c8: every ENABLED modification --------------------------
    // Retail reads Modification::_singleton here, NOT `Engine::_modification`,
    // and derefs it unguarded. The null test below is ours ONLY for the case
    // the mint leaves the slot empty; with the DLL present it is never taken.
    if (uint8_t* mods = ModificationSingleton()) {
        const int32_t count = *reinterpret_cast<int32_t*>(mods + 0x10);
        uint8_t* base = *reinterpret_cast<uint8_t**>(mods + 8);
        for (int32_t i = 0; i < count; ++i) {
            uint8_t* mod = base + static_cast<size_t>(i) * 0x80;
            if (mod[0x50] == 0) continue;         // Mod::enabled
            ++g_modsSeen;

            // Filename::GetPathFilename(&mod->filename, &ret, /*keepSlash*/true)
            String dir{nullptr};
            {
                Filename ret{nullptr};
                RT_EXPORT_ONCE(GetPathFilenameFn,
                    "?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z")(
                        reinterpret_cast<const Filename*>(mod + 0x58), &ret, true);
                AddRefInto(&dir, reinterpret_cast<String*>(&ret));
                RtString_ReleaseInline(reinterpret_cast<String*>(&ret));
            }

            String spec{nullptr};
            RtSystem_String_Format(&spec, "%sResources.rsc:resource", dir.data);

            bool exists;
            {
                String probe{nullptr};
                AddRefInto(&probe, &spec);
                AdjustDelimiters(reinterpret_cast<Filename*>(&probe));

                Filename compiled{nullptr};
                RT_EXPORT_ONCE(GetCompiledPathFn,
                    "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z")(
                        &compiled, reinterpret_cast<const Filename*>(&probe));

                exists = CompiledResourceExists(&compiled);

                RtString_ReleaseInline(reinterpret_cast<String*>(&compiled));
                RtString_ReleaseInline(&probe);
            }

            if (exists) {
                ++g_modsFound;
                String name{nullptr};
                AddRefInto(&name, &spec);
                AdjustDelimiters(reinterpret_cast<Filename*>(&name));
                // Result DISCARDED by retail — see FindAndCheck's comment.
                FindAndCheck(reinterpret_cast<const Filename*>(&name), false);
                RtString_ReleaseInline(&name);
            }

            RtString_ReleaseInline(&spec);
            RtString_ReleaseInline(&dir);
        }
    }

    // ---- 0x1800ddb16: the `-ref <path>` command-line override --------------
    // THE READ OF ?_singleton@Configuration@System@@ THIS WHOLE FILE IS ABOUT.
    String key{nullptr};
    RtSystem_String_Set(&key, "ref");

    String value{nullptr};
    RT_EXPORT_ONCE(GetCommandLineStringFn,
        "?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z")(
            ConfigurationSingleton(), &value, &key);
    RtString_ReleaseInline(&key);

    // 0x1800ddc44: `CMP dword ptr [RAX-0x8],0` — the LENGTH word, not the
    // pointer. An empty-but-allocated String must not load anything.
    if (value.data != nullptr && StringLength(value.data) != 0) {
        ++g_refUsed;
        String name{nullptr};
        AddRefInto(&name, &value);
        AdjustDelimiters(reinterpret_cast<Filename*>(&name));
        FindAndCheck(reinterpret_cast<const Filename*>(&name), false);
        RtString_ReleaseInline(&name);
    }
    RtString_ReleaseInline(&value);
}

extern "C" void RtEngineLoadResourcesSummary() {
    printf("[RtPort]   Engine::UpdateLoop::LoadResources: calls=%u ownList=%u "
           "modsEnabled=%u modsWithResources=%u refOverride=%u\n",
           g_calls, g_ownList, g_modsSeen, g_modsFound, g_refUsed);
}

// ============================================================================
// Registration — name AND address, one literal per line for the census.
// ============================================================================

int RtEngineLoadResourcesRegister() {
    RegisterRuntimeExport("?LoadResources@UpdateLoop@Engine@@QEAAXXZ",
                          (void*)&RtEngine_UpdateLoop_LoadResources);
    RegisterRuntimeAddr(0x1800dd7d0, (void*)&RtEngine_UpdateLoop_LoadResources);

    return 1;
}
