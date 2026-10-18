// src/diag/err_line.h -- one write per diagnostic line on the unbuffered stderr.
//
// HITCH_POSTDRAIN (docs/extensions/handins/HITCH_POSTDRAIN.md). main.cpp opens
// standalone_error.log with setvbuf(stderr, NULL, _IONBF, 0) so every line survives an
// abnormal exit. MinGW's fprintf (__mingw_vfprintf -> __pformat_putc) writes an
// UNBUFFERED stream one character at a time: one WriteFile per character, ~3 us each,
// ~0.3 ms for a 100-character line. A diagnostic that prints a few dozen lines in one
// tick is a frame hitch in every flavour (only printf is stripped, src/app/log_strip.h).
//
// ErrLine formats into a stack buffer and hands the whole line to stderr in one fwrite,
// i.e. one WriteFile: the same bytes, the same unbuffered guarantee (a line is written
// before the call returns), ~100x cheaper. A line longer than the buffer falls back to
// vfprintf. Header-only so game code needs no new link dependency.
#pragma once
#include <cstdarg>
#include <cstdio>

__attribute__((format(gnu_printf, 1, 2)))
inline void ErrLine(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n < (int)sizeof(buf)) {
        std::fwrite(buf, 1, (size_t)n, stderr);
        return;
    }
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}
