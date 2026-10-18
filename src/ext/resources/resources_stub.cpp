// ============================================================================
// src/ext/resources/resources_stub.cpp -- DELETE AT MERGE (block RSCSPRITE)
// ============================================================================
// A minimal stand-in for block RSCTEXT's resources.cpp (the provider table,
// Provide(), SetPath(), Publish()) so the RSCSPRITE branch links and runs on
// its own. It has NO engine seam: nothing calls Provide() here, so the
// providers are registered but never asked. When resources.cpp merges, this
// file duplicates every symbol in it: delete this file, nothing else.
// ============================================================================

#include "ext/resources/Resources.h"

#include <cstdio>
#include <cstring>

#include "object/External.h"
#include "system/Filename.h"

namespace Ext {
namespace Resources {

namespace {

struct Entry {
    const char* prefix;
    ProviderFn  fn;
    void*       user;
};
Entry g_table[8];
int   g_count = 0;

}  // namespace

bool Register(const char* prefix, ProviderFn fn, void* user) {
    if (g_count >= 8 || prefix == nullptr || fn == nullptr) return false;
    g_table[g_count++] = Entry{ prefix, fn, user };
    std::fprintf(stderr, "[ExtRes] (stub) provider registered for %s\n", prefix);
    return true;
}

Object::External* Provide(const char* path) {
    if (path == nullptr) return nullptr;
    for (int i = 0; i < g_count; ++i) {
        const Entry& e = g_table[i];
        if (std::strncmp(path, e.prefix, std::strlen(e.prefix)) != 0) continue;
        if (Object::External* obj = e.fn(path, e.user)) return obj;
    }
    return nullptr;
}

void SetPath(Object::External* obj, const char* path) {
    if (obj == nullptr || path == nullptr) return;
    obj->_path = System::Filename(path);
}

Object::External* Publish(Object::External* obj) {
    if (obj == nullptr) return nullptr;
    if (Object::ExternalFactory* f = Object::ExternalFactory::_singleton) f->Add(obj);
    return obj;
}

}  // namespace Resources
}  // namespace Ext
