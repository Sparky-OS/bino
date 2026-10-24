#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct AVFrame;
static long long now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

int av_hwframe_transfer_data(struct AVFrame *dst, const struct AVFrame *src, int flags)
{
    typedef int (*transfer_fn)(struct AVFrame *, const struct AVFrame *, int);
    transfer_fn real = (transfer_fn)dlsym(RTLD_NEXT, "av_hwframe_transfer_data");
    if (!real) {
        // Qt loads its FFmpeg plugin locally, outside RTLD_NEXT's lookup scope.
        void *avutil = dlopen("libavutil.so.61", RTLD_LAZY | RTLD_NOLOAD);
        if (avutil) {
            real = (transfer_fn)dlsym(avutil, "av_hwframe_transfer_data");
            dlclose(avutil);
        }
    }
    if (!real) {
        fprintf(stderr, "transfer-trace: cannot resolve FFmpeg 9 av_hwframe_transfer_data\n");
        abort();
    }
    long long start = now();
    int result = real(dst, src, flags);
    long long elapsed = now() - start;
    const char *path = getenv("BINO_TRANSFER_TRACE");
    if (path) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "%lld,%lld,%d\n", start, elapsed, result);
            fclose(f);
        }
    }
    return result;
}
