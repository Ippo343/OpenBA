/*
 * third_party/wmadec — the FFmpeg runtime pieces the vendored decoder links
 * against, replaced by the smallest bodies that keep its behaviour:
 *
 *   av_log / av_vlog      silent (the decoder's messages are diagnostics; every
 *                         error also comes back as a negative return code)
 *   av_malloc & family    aligned heap allocation (FFmpeg's ALIGN of 64), same
 *                         zeroing / overflow-check contracts as libavutil/mem.c
 *   av_get_cpu_flags      0: only the portable C transform codelets exist here
 *   ff_thread_once        a real one-time init (see shim/libavutil/thread.h)
 *   avpriv_request_sample silent, like av_log
 *   av_gcd, av_realloc_f, av_fast_realloc
 *                         copied from FFmpeg n8.1.3 libavutil/mathematics.c and
 *                         libavutil/mem.c (bodies unchanged except the
 *                         max_alloc_size atomic, a constant here)
 *   ff_tx_codelet_list_{double,int32}_c
 *                         empty: tx.c's codelet table names all three sample
 *                         types; the WMA decoder only builds float MDCTs, so
 *                         tx_double.c / tx_int32.c are not vendored
 *
 * Banished reimplementation glue, not an FFmpeg file; see EXTRACT.md.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#else
#include <pthread.h>
#endif

#include "libavutil/common.h"
#include "libavutil/cpu.h"
#include "libavutil/internal.h"
#include "libavutil/log.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/thread.h"
#include "libavutil/tx_priv.h"

#define SHIM_ALIGN 64
#define SHIM_MAX_ALLOC ((size_t)INT_MAX)   /* libavutil/mem.c's max_alloc_size default */

void av_log(void *avcl, int level, const char *fmt, ...)
{
    (void)avcl; (void)level; (void)fmt;
}

void av_vlog(void *avcl, int level, const char *fmt, va_list vl)
{
    (void)avcl; (void)level; (void)fmt; (void)vl;
}

void *av_malloc(size_t size)
{
    void *ptr;
    if (size > SHIM_MAX_ALLOC)
        return NULL;
    if (!size)
        size = 1;                       /* mem.c: av_malloc(0) returns a valid pointer */
#ifdef _WIN32
    ptr = _aligned_malloc(size, SHIM_ALIGN);
#else
    if (posix_memalign(&ptr, SHIM_ALIGN, size))
        ptr = NULL;
#endif
    return ptr;
}

void *av_mallocz(size_t size)
{
    void *ptr = av_malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void *av_malloc_array(size_t nmemb, size_t size)
{
    if (size && nmemb > SHIM_MAX_ALLOC / size)
        return NULL;
    return av_malloc(nmemb * size);
}

void *av_calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > SHIM_MAX_ALLOC / size)
        return NULL;
    return av_mallocz(nmemb * size);
}

void *av_realloc(void *ptr, size_t size)
{
    if (size > SHIM_MAX_ALLOC)
        return NULL;
#ifdef _WIN32
    return _aligned_realloc(ptr, size + !size, SHIM_ALIGN);
#else
    return realloc(ptr, size + !size);  /* what libavutil/mem.c does too */
#endif
}

void *av_realloc_array(void *ptr, size_t nmemb, size_t size)
{
    if (size && nmemb > SHIM_MAX_ALLOC / size)
        return NULL;
    return av_realloc(ptr, nmemb * size);
}

void av_free(void *ptr)
{
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void av_freep(void *arg)
{
    void *val;
    memcpy(&val, arg, sizeof(val));
    memset(arg, 0, sizeof(val));
    av_free(val);
}

void *av_realloc_f(void *ptr, size_t nelem, size_t elsize)
{
    size_t size;
    void *r;

    if (elsize && nelem > SIZE_MAX / elsize) {   /* mem.c's size_mult overflow test */
        av_free(ptr);
        return NULL;
    }
    size = nelem * elsize;
    r = av_realloc(ptr, size);
    if (!r)
        av_free(ptr);
    return r;
}

void *av_fast_realloc(void *ptr, unsigned int *size, size_t min_size)
{
    size_t max_size;

    if (min_size <= *size)
        return ptr;

    max_size = SHIM_MAX_ALLOC;
    /* *size is an unsigned, so the real maximum is <= UINT_MAX. */
    max_size = FFMIN(max_size, UINT_MAX);

    if (min_size > max_size) {
        *size = 0;
        return NULL;
    }

    min_size = FFMIN(max_size, FFMAX(min_size + min_size / 16 + 32, min_size));

    ptr = av_realloc(ptr, min_size);
    /* we could set this to the unmodified min_size but this is safer
     * if the user lost the ptr and uses NULL now
     */
    if (!ptr)
        min_size = 0;

    *size = min_size;

    return ptr;
}

int64_t av_gcd(int64_t a, int64_t b) {
    int za, zb, k;
    int64_t u, v;
    if (a == 0)
        return b;
    if (b == 0)
        return a;
    za = ff_ctzll(a);
    zb = ff_ctzll(b);
    k  = FFMIN(za, zb);
    u = llabs(a >> za);
    v = llabs(b >> zb);
    while (u != v) {
        if (u > v)
            FFSWAP(int64_t, v, u);
        v -= u;
        v >>= ff_ctzll(v);
    }
    return (uint64_t)u << k;
}

void avpriv_request_sample(void *avc, const char *msg, ...)
{
    (void)avc; (void)msg;
}

const FFTXCodelet * const ff_tx_codelet_list_double_c[] = { NULL };
const FFTXCodelet * const ff_tx_codelet_list_int32_c[]  = { NULL };

int av_get_cpu_flags(void)
{
    return 0;
}

#ifdef _WIN32
static BOOL CALLBACK shim_once_trampoline(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once; (void)ctx;
    void (*routine)(void);
    memcpy(&routine, &param, sizeof(routine));
    routine();
    return TRUE;
}

int ff_thread_once(AVOnce *control, void (*routine)(void))
{
    PVOID param;
    _Static_assert(sizeof(AVOnce) == sizeof(INIT_ONCE), "AVOnce holds an INIT_ONCE");
    memcpy(&param, &routine, sizeof(param));
    return InitOnceExecuteOnce((PINIT_ONCE)control, shim_once_trampoline, param, NULL) ? 0 : -1;
}
#else
int ff_thread_once(AVOnce *control, void (*routine)(void))
{
    return pthread_once(control, routine);
}
#endif
