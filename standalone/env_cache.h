// Process-wide getenv cache — force-included into every TU (Makefile SA_CXXFLAGS -include).
//
// Why: the port has ~370 getenv sites, many of them diagnostic `if (std::getenv("X"))`
// checks inside per-tick code that nobody cached. The CRT getenv is a linear scan of the
// environment block with a strcmp per entry; the sampling profiler put getenv+strcmp at
// 20% of the CitizenList phase and ~1% of the EntityList phase at 800 citizens x10.
// Rather than hunt every site, `getenv`/`std::getenv` resolve to a hash-cached wrapper.
// The environment only changes through our own two _putenv_s sites, which are wrapped
// as well and invalidate the cache.
//
// Semantics: identical pointers to what the CRT returned (the CRT's own env block), so a
// cached value stays valid exactly as long as the CRT's would.
#pragma once
#include <cstdlib>
#include <stdlib.h>

extern "C" char* BanishedGetenvCached(const char* name);
extern "C" int   BanishedPutenvS(const char* name, const char* value);

inline char* banished_getenv(const char* name) { return ::BanishedGetenvCached(name); }
namespace std { inline char* banished_getenv(const char* name) { return ::BanishedGetenvCached(name); } }
#define getenv banished_getenv
#define _putenv_s(n, v) ::BanishedPutenvS((n), (v))
