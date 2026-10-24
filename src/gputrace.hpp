// Asynchronous timestamp queries; diagnostic runs only.
#pragma once
#include <QOpenGLContext>
#include <vector>
#include "frametrace.hpp"

namespace FrameTrace {
class GpuScope {
    struct Entry {
        GLuint ids[2];
        const char* name;
        long long start, pts;
        int arg1, arg2;
    };
    using Gen = void (*)(GLsizei, GLuint*);
    using Counter = void (*)(GLuint, GLenum);
    using Get = void (*)(GLuint, GLenum, GLint*);
    using Get64 = void (*)(GLuint, GLenum, GLuint64*);
    using Delete = void (*)(GLsizei, const GLuint*);
    inline static std::vector<Entry> pending;
    inline static Gen gen = nullptr;
    inline static Counter counter = nullptr;
    inline static Get get = nullptr;
    inline static Get64 get64 = nullptr;
    inline static Delete remove = nullptr;
    Entry _entry{};
    bool _active = false;
public:
    GpuScope(const char* name, long long pts, int arg1 = 0, int arg2 = 0)
    {
        static const bool enabled = std::getenv("BINO_GPU_TRACE") != nullptr;
        if (!enabled || !stream())
            return;
        auto* context = QOpenGLContext::currentContext();
        if (!context || context->isOpenGLES())
            return;
        if (!gen) {
            gen = reinterpret_cast<Gen>(context->getProcAddress("glGenQueries"));
            counter = reinterpret_cast<Counter>(context->getProcAddress("glQueryCounter"));
            get = reinterpret_cast<Get>(context->getProcAddress("glGetQueryObjectiv"));
            get64 = reinterpret_cast<Get64>(context->getProcAddress("glGetQueryObjectui64v"));
            remove = reinterpret_cast<Delete>(context->getProcAddress("glDeleteQueries"));
        }
        if (!gen || !counter || !get || !get64 || !remove)
            return;
        while (!pending.empty()) {
            const Entry& p = pending.front();
            GLint available = 0;
            get(p.ids[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available)
                break;
            GLuint64 begin = 0, end = 0;
            get64(p.ids[0], GL_QUERY_RESULT, &begin);
            get64(p.ids[1], GL_QUERY_RESULT, &end);
            event(p.name, p.start, end - begin, p.pts, p.arg1, p.arg2);
            remove(2, p.ids);
            pending.erase(pending.begin());
        }
        if (pending.size() > 4096)
            return;
        _entry = {{0, 0}, name, timestamp(), pts, arg1, arg2};
        gen(2, _entry.ids);
        counter(_entry.ids[0], GL_TIMESTAMP);
        _active = true;
    }
    ~GpuScope()
    {
        if (_active) {
            counter(_entry.ids[1], GL_TIMESTAMP);
            pending.push_back(_entry);
        }
    }
};
}
