// Optional diagnostics for frame delivery and rendering.
#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace FrameTrace {
inline long long timestamp()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline FILE* stream()
{
    static FILE* file = []() -> FILE* {
        const char* path = std::getenv("BINO_FRAME_TRACE");
        if (!path || !*path)
            return nullptr;
        FILE* f = std::fopen(path, "w");
        if (f) {
            std::setvbuf(f, nullptr, _IOLBF, 0);
            std::fputs("event,start_ns,duration_ns,pts_us,arg1,arg2\n", f);
        }
        return f;
    }();
    return file;
}

inline long long pts = -1;

inline void event(const char* name, long long start, long long duration,
        long long framePts, int arg1 = 0, int arg2 = 0)
{
    if (FILE* f = stream())
        std::fprintf(f, "%s,%lld,%lld,%lld,%d,%d\n",
                name, start, duration, framePts, arg1, arg2);
}

class Scope {
    const char* _name;
    long long _start;
    long long _pts;
    int _arg1, _arg2;
public:
    Scope(const char* name, long long framePts, int arg1 = 0, int arg2 = 0) :
        _name(name), _start(stream() ? timestamp() : 0),
        _pts(framePts), _arg1(arg1), _arg2(arg2) {}
    ~Scope()
    {
        if (_start)
            event(_name, _start, timestamp() - _start, _pts, _arg1, _arg2);
    }
};
}
