#include "factory_registration.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdlib>
#include "Component/component_factory_base.h"
#include <cstring>

namespace Object {

// Linked list of pending registrations. CRT-init populates this; InvokeAll
// drains it. Zero-initialized as a static, so order of static ctors across
// TUs doesn't matter — each ctor head-inserts.
static FactoryRegistration* g_pending = nullptr;
static bool g_invoked = false;

FactoryRegistration::FactoryRegistration(const char* name, const char* parent,
                                         const char* abstract,
                                         FactoryFn factory, InPlaceCreateFn inPlace)
    : storage{}, typeName(name), parentName(parent), abstractName(abstract),
      factoryFn(factory), inPlaceCreateFn(inPlace),
      next(g_pending) {
    g_pending = this;
}

int InvokeAllRegistrations() {
    if (g_invoked) {
        return 0;
    }
    g_invoked = true;

    if (!fn_BaseTypeInfo_Ctor) {
        std::fprintf(stderr,
            "[FactoryReg] FATAL: fn_BaseTypeInfo_Ctor not resolved — call EngineFuncsInit() first.\n");
        return 0;
    }

    int n = 0;
    for (FactoryRegistration* r = g_pending; r != nullptr; r = r->next) {
        std::memset(&r->storage, 0, sizeof(r->storage));
        fn_BaseTypeInfo_Ctor(&r->storage,
                             r->typeName,
                             r->parentName,
                             r->abstractName,
                             reinterpret_cast<void*>(r->factoryFn),
                             reinterpret_cast<void*>(r->inPlaceCreateFn));
        n++;
    }
    printf("[FactoryReg] Registered %d types into Runtime _firstTypeInfo\n", n);
    return n;
}

uint32_t GetTypeIdByName(const char* name) {
    if (!name) return 0;
    for (FactoryRegistration* r = g_pending; r != nullptr; r = r->next) {
        if (r->typeName && std::strcmp(r->typeName, name) == 0) {
            return *reinterpret_cast<uint32_t*>(&r->storage);
        }
    }
    return 0;
}


// BANISHED_DUMP_SLOT3=1 — see header. This wave alone found the base-only
// fallback shipping silently on Storage, Residence, the whole animal family and
// 16 audited classes; a text audit keeps missing satellite-file serializers, so
// this reports the OUTCOME (what the vtable actually holds at runtime).
void DumpSlot3Inventory() {
    const char* e = getenv("BANISHED_DUMP_SLOT3");
    if (!e || !*e || *e == '0') return;
    void* baseSlot = ::ComponentFactory::BaseSerializeSlot();
    void* stubSlot = ::ComponentFactory::InertStubSlot();
    int total = 0, baseOnly = 0, typed = 0;
    std::fprintf(stderr, "[Slot3Dump] BEGIN (base-only probe=%p)\n", baseSlot);
    for (FactoryRegistration* r = g_pending; r != nullptr; r = r->next) {
        if (!r->typeName || !r->factoryFn) continue;
        size_t len = std::strlen(r->typeName);
        if (len < 9 || std::strcmp(r->typeName + len - 9, "Component") != 0) continue;
        void* obj = r->factoryFn();
        if (!obj) {
            std::fprintf(stderr, "[Slot3Dump] %-32s FACTORY RETURNED NULL\n", r->typeName);
            continue;
        }
        void** vt = *reinterpret_cast<void***>(obj);
        void* s3 = vt ? vt[3] : nullptr;
        ++total;
        if (s3 == baseSlot || s3 == stubSlot || !s3) {
            ++baseOnly;
            std::fprintf(stderr, "[Slot3Dump] %-32s slot3=%p BASE-ONLY (state lost on save/load)\n",
                         r->typeName, s3);
        } else {
            ++typed;
        }
    }
    std::fprintf(stderr, "[Slot3Dump] END: %d Component classes, %d typed serializers, %d BASE-ONLY\n",
                 total, typed, baseOnly);
    std::fflush(stderr);
}

const char* GetTypeNameById(uint32_t typeId) {
    if (typeId == 0 || typeId == 0xFFFFFFFFu) return nullptr;
    for (FactoryRegistration* r = g_pending; r != nullptr; r = r->next) {
        if (*reinterpret_cast<uint32_t*>(&r->storage) == typeId) return r->typeName;
    }
    return nullptr;
}

// The BaseTypeInfo POINTER for a registered type — the same thing each class's
// GetTypeInfo virtual (vtable slot 1) returns in retail, where it reads a per-class
// .rdata global sitting 8 bytes after the type-id global (e.g. FishingComponent's id at
// 0x1401c7660, its type-info at 0x1401c7668). Our BaseTypeInfoStorage IS that object, so
// handing back its address is the faithful answer.
void* GetTypeInfoByName(const char* name) {
    if (!name) return nullptr;
    for (FactoryRegistration* r = g_pending; r != nullptr; r = r->next) {
        if (r->typeName && std::strcmp(r->typeName, name) == 0) {
            return &r->storage;
        }
    }
    return nullptr;
}

// Tiny per-name cache. Names are static string literals from the call sites,
// so we compare pointers (after a strcmp on first miss). 16 entries is
// plenty for the External-serialize helper set (Mesh, Particle, MaterialInst,
// Weather, ...) and avoids any heap allocation.
namespace {
struct TypeIdCacheEntry {
    const char* name;
    uint32_t    typeId;
};
constexpr int kTypeIdCacheSize = 16;
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

    // 2. Try Runtime DLL Factory (covers self-registered Runtime types).
    uint32_t typeId = 0;
    if (fn_Factory_GetTypeIdFromName && g_pFactorySingleton
            && *g_pFactorySingleton) {
        // Triple deref: dispatch slot -> pointer -> Factory*. See the same
        // pattern in inputbindings.cpp / engine_funcs.h FindDialogByName_Impl.
        void* factoryObj = *(void**)*g_pFactorySingleton;
        if (factoryObj) {
            typeId = fn_Factory_GetTypeIdFromName(
                factoryObj, simpleName, /*includeBase=*/true);
        }
    }

    // 3. Fallback to exe-side REGISTER_TYPE list.
    if (typeId == 0) {
        typeId = GetTypeIdByName(simpleName);
    }

    // 4. Cache the result (even 0, to avoid re-querying every call) at the
    // first empty slot. If the cache is full, we just pay the lookup cost.
    for (int i = 0; i < kTypeIdCacheSize; ++i) {
        if (s_typeIdCache[i].name == nullptr) {
            s_typeIdCache[i].name   = simpleName;
            s_typeIdCache[i].typeId = typeId;
            break;
        }
    }

    if (typeId != 0) {
        printf("[FactoryReg] ResolveTypeIdByNameLazy(\"%s\") = 0x%08x\n",
                    simpleName, typeId);
    }
    return typeId;
}

bool IsDerivedFromOrTrust(void* obj, uint32_t typeId) {
    (void)typeId;
    return obj != nullptr;
}

bool IsTypeDerivedFromByName(uint32_t typeId, const char* ancestorName) {
    if (typeId == 0 || typeId == 0xFFFFFFFF || !ancestorName || !*ancestorName)
        return false;
    // Find the registration owning this typeId.
    const FactoryRegistration* r = nullptr;
    for (const FactoryRegistration* it = g_pending; it != nullptr; it = it->next) {
        if (*reinterpret_cast<const uint32_t*>(&it->storage) == typeId) {
            r = it;
            break;
        }
    }
    // Walk the parent-name chain (bounded — the deepest real chain is ~6).
    for (int depth = 0; r != nullptr && depth < 16; ++depth) {
        if (r->typeName && std::strcmp(r->typeName, ancestorName) == 0)
            return true;
        const char* parent = r->parentName;
        if (!parent || !*parent) return false;
        const FactoryRegistration* next = nullptr;
        for (const FactoryRegistration* it = g_pending; it != nullptr; it = it->next) {
            if (it->typeName && std::strcmp(it->typeName, parent) == 0) {
                next = it;
                break;
            }
        }
        // Parent names may refer to engine-side classes that are not in the
        // exe registry (e.g. "Object::Internal") — chain ends there.
        if (next == nullptr) return std::strcmp(parent, ancestorName) == 0;
        r = next;
    }
    return false;
}

}  // namespace Object

// ============================================================
// Self-test registration — proves the framework works end-to-end.
// Parent must be either nullptr or the name of another registered type;
// Factory::Startup's second loop crashes on an unresolvable parent string.
//
// All 310 exe-side types are registered in factory_registrations_bulk.cpp
// (auto-generated by tools/gen_factory_registrations.py from
// standalone/factory_registrations.csv).
// ============================================================
namespace {
    void* SelfTest_Factory(void) { return nullptr; }
    void* SelfTest_InPlace(void* /*mem*/) { return nullptr; }
}
REGISTER_TYPE(StandaloneFactoryTest, /*parent=*/nullptr,
              &SelfTest_Factory, &SelfTest_InPlace);

