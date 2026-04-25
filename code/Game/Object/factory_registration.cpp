#include "factory_registration.h"
#include "engine_funcs.h"
#include <cstdio>
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
    std::printf("[FactoryReg] Registered %d types into Runtime _firstTypeInfo\n", n);
    return n;
}

}  // namespace Object

// ============================================================
// Self-test registration — proves the framework works end-to-end.
// Parent must be either nullptr or the name of another registered type;
// Factory::Startup's second loop crashes on an unresolvable parent string.
// ============================================================
#ifdef STANDALONE_EXE
namespace {
    void* NullFactory(void) { return nullptr; }
    void* NullInPlace(void* /*mem*/) { return nullptr; }
}
REGISTER_TYPE(StandaloneFactoryTest, /*parent=*/nullptr,
              &NullFactory, &NullInPlace);

// Stage 7a: incremental registration of exe-side types as we discover them
// crashing in External::Load. Each type uses NullFactory — External::Load
// gracefully bails when the factory returns NULL (param_2==NULL check).
// Parent MUST be either nullptr or a registered type — Factory::Startup's
// second loop crashes on unresolvable parents.
//
// StatGoal: parent="Achievement::Goal" → colon-stripped to "Goal" (Runtime type).
//   Discovered via crash diagnostic 2026-04-25: missed typeId 0x2294CCAE.
REGISTER_TYPE(StatGoal, "Goal", &NullFactory, &NullInPlace);
#endif

