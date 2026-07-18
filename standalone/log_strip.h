// Compiled-out debug logging for release builds.
//
// Auto-included via -include in the Makefile when BANISHED_STRIP_LOGS is set
// (release DLL + standalone exe targets). When the flag is on, every printf
// call site in the project becomes ((void)0) at preprocess time, so format
// strings are never parsed and no I/O happens — zero runtime cost.
//
// Debug builds leave printf alone, so existing logs to patch_debug.log /
// standalone_debug.log keep working during development.
//
// Notes:
//  - Only `printf` is stripped. fprintf(stderr, ...) (used for FATAL errors)
//    and the dedicated CrashLogf path stay intact, so crash diagnostics still
//    work in release builds.
//  - The preprocessor can't replace qualified names like `std::printf`, so
//    all call sites must use unqualified `printf` (cstdio's `using std::printf`
//    makes this a no-op semantic change).
#ifndef BANISHED_LOG_STRIP_H
#define BANISHED_LOG_STRIP_H

#ifdef BANISHED_STRIP_LOGS
// Pull in the real declaration first so any subsequent <cstdio> include is a
// guarded no-op and the macro override below isn't seen by the CRT header.
#include <cstdio>
#undef printf
// Drop both the call and its arguments at preprocess time. Variables that
// existed only to be printed will surface as -Wunused-variable in release;
// fix individually if they show up as noise.
#define printf(...) ((void)0)
#endif

#endif
