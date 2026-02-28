#pragma once
// System::String — refcounted copy-on-write string (8 bytes).
// Runtime-x64.dll. Used pervasively throughout the engine.
//
// The String object is just a pointer to char data. Metadata lives
// at negative offsets from the data pointer:
//
//   Memory layout of non-empty string:
//     [int32_t length][int32_t refcount][char data...]['\0']
//      ^               ^                ^
//      m_data - 8      m_data - 4       m_data
//
//   Empty string: m_data = NULL (length 0, no allocation)
//
// Reference counting:
//   - Copy ctor / operator=: share data, increment refcount
//   - Destructor / Release: decrement refcount, free if reaches 0
//   - Set(): always releases old, allocates new (no in-place mutation)
//   - NOT thread-safe (no atomics on refcount)
//
// System::Filename uses the same layout (8-byte pointer, same header).

#include <cstdint>

struct SystemString {
    char* m_data;  // +0x00: pointer to char data (NULL = empty)
};
static_assert(sizeof(SystemString) == 8, "SystemString must be 8 bytes");

// --- Inline helpers for reading string metadata ---

inline int StringGetLength(const SystemString* s) {
    if (!s->m_data) return 0;
    return *reinterpret_cast<int32_t*>(s->m_data - 8);
}

inline int StringGetRefCount(const SystemString* s) {
    if (!s->m_data) return 0;
    return *reinterpret_cast<int32_t*>(s->m_data - 4);
}

inline const char* StringGetData(const SystemString* s) {
    return s->m_data;
}

// --- Lifecycle wrappers (call through resolved function pointers) ---
// Include engine_funcs.h before using these.

#include "engine_funcs.h"

inline void StringInit(SystemString* s) {
    fn_String_Ctor_Default(s);
}

inline void StringInitCStr(SystemString* s, const char* src) {
    fn_String_Ctor_CStr(s, src);
}

inline void StringInitCopy(SystemString* dst, const SystemString* src) {
    fn_String_Ctor_Copy(dst, src);
}

inline void StringSet(SystemString* s, const char* src) {
    fn_String_Set(s, src);
}

inline void StringDestroy(SystemString* s) {
    fn_String_Dtor(s);
}
