/*
 * Banished reimplementation, third_party/wmadec: stands in for FFmpeg's
 * libavutil/thread.h. The vendored files use exactly one thing from it: AVOnce /
 * ff_thread_once, guarding the lazily built sine-window and FFT twiddle tables.
 * FFmpeg's no-threads fallback (a plain flag) is NOT safe here: the game decodes a
 * SoundBuffer on the resource loader thread while the music Stream worker decodes
 * its next chunk, so both can reach the same table init. This shim is a real
 * one-time init: INIT_ONCE on Windows, pthread_once elsewhere (wmadec_shim.c).
 * Not an FFmpeg file; see EXTRACT.md.
 */
#ifndef AVUTIL_THREAD_H
#define AVUTIL_THREAD_H

#ifdef _WIN32
typedef void *AVOnce;            /* storage of a Win32 INIT_ONCE (one pointer) */
#define AV_ONCE_INIT 0
#else
#include <pthread.h>
typedef pthread_once_t AVOnce;
#define AV_ONCE_INIT PTHREAD_ONCE_INIT
#endif

int ff_thread_once(AVOnce *control, void (*routine)(void));

#endif /* AVUTIL_THREAD_H */
