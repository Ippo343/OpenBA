// code/Game/Object/factory_registration.cpp — see factory_registration.h.
//
// ENDGAME B.3.9 RF (B39_RULES.md D6): the FactoryRegistration staging list
// (`g_pending`), InvokeAllRegistrations() and the fn_BaseTypeInfo_Ctor door are
// gone. The exe records are real BaseTypeInfo class statics built at CRT init
// (Object/exe_type.h); the helpers below answer through Object::Factory.
#include "factory_registration.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C

namespace {

// ---------------------------------------------------------------------------
// The exe-record section. Every DEFINE_EXE_TYPE expansion leaves one
// constant-initialised `BaseTypeInfo*` (its `_exeTypeInfoEntry`) in the input
// section `.data$exetype_b`. The PE linker merges `.data$*` input sections
// into `.data` SORTED BY NAME (`*(SORT(.data$*))`, the default MinGW script),
// so the two markers below bracket every entry: `.data$exetype_a` sorts before
// and `.data$exetype_c` after. The entries are data with an address constant,
// not code: they exist whether or not a dynamic initialiser ran, which is what
// makes them an INDEPENDENT count of the expansions for VerifyExeTypeInfos().
// Alignment padding between input sections, if any, is zero bytes; a null
// entry is skipped.
// ---------------------------------------------------------------------------
__attribute__((used, section(".data$exetype_a")))
::Object::BaseTypeInfo* s_exeTypeInfoBegin = nullptr;
__attribute__((used, section(".data$exetype_c")))
::Object::BaseTypeInfo* s_exeTypeInfoEnd = nullptr;

template <class F>
void WalkExeTypeInfoSection(F&& f) {
    ::Object::BaseTypeInfo** p = &s_exeTypeInfoBegin;
    ::Object::BaseTypeInfo** e = &s_exeTypeInfoEnd;
    // The LINKER relates the two markers, not the language: hide their
    // provenance so the compiler cannot reason about two distinct objects.
    __asm__ volatile("" : "+r"(p), "+r"(e));
    for (++p; p < e; ++p)
        if (*p) f(**p);
}

[[noreturn]] void ExeTypeFatal(const char* what, const ::Object::BaseTypeInfo* ti) {
    std::fprintf(stderr, "[FactoryReg] FATAL: %s%s%s\n", what,
                 ti ? " -- record " : "", ti ? (ti->name ? ti->name : "(unconstructed)") : "");
    std::fflush(stderr);
    std::abort();
}

}  // namespace

namespace Object {

void ForEachExeTypeInfo(ExeTypeInfoVisitFn fn, void* ctx) {
    if (!fn) return;
    WalkExeTypeInfoSection([&](const BaseTypeInfo& ti) { fn(ti, ctx); });
}

int VerifyExeTypeInfos() {
    // 1. Every collected record is in the chain: its CRT-init ctor ran.
    int entries = 0;
    WalkExeTypeInfoSection([&](const BaseTypeInfo& ti) {
        ++entries;
        bool linked = false;
        for (const BaseTypeInfo* c = BaseTypeInfo::_firstTypeInfo; c; c = c->next)
            if (c == &ti) { linked = true; break; }
        if (!linked)
            ExeTypeFatal("an exe type record is not in _firstTypeInfo (its static "
                         "initialiser did not run)", &ti);
        // 2. Factory::Startup walked it: the id is the CRC of the name.
        if (!ti.name || ti.typeId != TypeNameCRC(ti.name))
            ExeTypeFatal("an exe type record does not carry ~CRC32(name) after "
                         "Factory::Startup", &ti);
    });
    if (entries == 0)
        ExeTypeFatal("no exe type record in the .data$exetype_b section (the "
                     "linker did not bracket it between the markers)", nullptr);

    // 3. No two records of the whole chain share an id (B39_RULES D6: Startup's
    //    Insert keeps the first of two equal keys without a word).
    for (const BaseTypeInfo* a = BaseTypeInfo::_firstTypeInfo; a; a = a->next)
        for (const BaseTypeInfo* b = a->next; b; b = b->next)
            if (a->typeId == b->typeId)
                ExeTypeFatal("two records in _firstTypeInfo share a type id", b);

    printf("[FactoryReg] Registered %d exe type records in _firstTypeInfo\n", entries);
    return entries;
}

// Abstract base types (DEFINE_EXE_ABSTRACT_TYPE). The exe registers these with
// a _purecall stub (0x140003730) -- they are never instantiated, they only
// exist as parents in the type hierarchy. Moved here from the deleted
// factory_registrations_bulk.cpp (ENDGAME B.3.9 RF); same text, same stream.
void* AbstractTypeFactory(const char* typeName) {
    std::fprintf(stderr,
        "[AbstractFactory] Unexpected instantiate of abstract type: %s\n", typeName);
    std::fflush(stderr);
    return nullptr;
}

// Before Engine::Startup has built the Factory there is no id to answer: 0, the
// value the old exe list answered before its records existed. This is the
// helper's contract for an early caller, not a guard around a faulting read.
uint32_t GetTypeIdByName(const char* name) {
    if (!name) return 0;
    const Factory* factory = Factory::Get();
    if (!factory) return 0;
    return factory->GetTypeIdFromName(name, /*existingOnly=*/true);
}

// GetTypeName faults on a miss in retail; this diagnostic helper asks
// IsTypeValid first and answers nullptr instead.
const char* GetTypeNameById(uint32_t typeId) {
    if (typeId == 0 || typeId == 0xFFFFFFFFu) return nullptr;
    const Factory* factory = Factory::Get();
    if (!factory || !factory->IsTypeValid(typeId)) return nullptr;
    return factory->GetTypeName(typeId);
}

// Per-name cache. Names are static string literals from the call sites, so we
// compare pointers (with a strcmp fallback for safety), and the scan stops at
// the first empty slot — so it costs one compare per DISTINCT name resolved so
// far, not kTypeIdCacheSize.
//
// This was 16 entries, sized when only the External-serialize helper set (Mesh,
// Particle, MaterialInst, Weather, ...) used it. The call-site set has since
// grown well past that — a single tutorial run resolves 48+ distinct names —
// and a FULL cache is silently catastrophic: every call for a name that did not
// make the first 16 misses, re-queries the Runtime Factory by name, and then
// prints an unbuffered line. That was 21,901 of the 24,719 lines in a one-minute
// standalone_debug.log (88%), and it is what actually held the tutorial at
// ~1 fps — the -O0 codegen was blamed for it. Sized with headroom; still a
// plain array, still no heap allocation.
namespace {
struct TypeIdCacheEntry {
    const char* name;
    uint32_t    typeId;
};
constexpr int kTypeIdCacheSize = 256;
static TypeIdCacheEntry s_typeIdCache[kTypeIdCacheSize] = {};
}  // namespace

uint32_t ResolveTypeIdByNameLazy(const char* simpleName) {
    if (!simpleName || !*simpleName) return 0;

    // 1. Cache lookup (pointer-equal first, strcmp fallback for safety).
    for (int i = 0; i < kTypeIdCacheSize; ++i) {
        const char* n = s_typeIdCache[i].name;
        if (!n) break;
        if (n == simpleName || std::strcmp(n, simpleName) == 0) {
            return s_typeIdCache[i].typeId;
        }
    }

    // 2. The live Factory tree: it holds every record of `_firstTypeInfo`, the
    //    exe's and the Runtime's. (The old third step, a fallback to the exe
    //    staging list, went with that list: after Factory::Startup the tree is
    //    a superset of it.)
    const uint32_t typeId = GetTypeIdByName(simpleName);

    // 3. Cache the result (even 0, to avoid re-querying every call) at the
    // first empty slot. If the cache is full, we just pay the lookup cost.
    bool cached = false;
    for (int i = 0; i < kTypeIdCacheSize; ++i) {
        if (s_typeIdCache[i].name == nullptr) {
            s_typeIdCache[i].name   = simpleName;
            s_typeIdCache[i].typeId = typeId;
            cached = true;
            break;
        }
    }

    // Print only on a genuine FIRST resolve. Reaching here at all means the cache
    // missed, so an uncacheable name (cache full) would otherwise print on every
    // single call from per-tick code — which is exactly the flood this trace
    // caused before the cache was resized. Say so once instead.
    if (cached) {
        if (typeId != 0) {
            printf("[FactoryReg] ResolveTypeIdByNameLazy(\"%s\") = 0x%08x\n",
                        simpleName, typeId);
        }
    } else {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            printf("[FactoryReg] WARNING: type-id cache FULL at %d entries — every "
                   "further miss re-queries the Runtime Factory by name on EVERY "
                   "call. Raise kTypeIdCacheSize.\n", kTypeIdCacheSize);
        }
    }
    return typeId;
}

bool IsDerivedFromOrTrust(void* obj, uint32_t typeId) {
    (void)typeId;
    return obj != nullptr;
}

bool IsTypeDerivedFrom(uint32_t typeId, uint32_t ancestorId) {
    if (typeId == 0 || typeId == 0xFFFFFFFF || ancestorId == 0 || ancestorId == 0xFFFFFFFF)
        return false;
    const Factory* factory = Factory::Get();
    if (!factory || !factory->IsTypeValid(typeId) || !factory->IsTypeValid(ancestorId))
        return false;
    return factory->IsDerivedFrom(typeId, ancestorId);
}

}  // namespace Object
