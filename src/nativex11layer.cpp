/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nativex11layer.hpp"
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPointer>
#include <QWidget>
#include <QWindow>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <stereo-declare.h>
#include <algorithm>
#include <vector>

namespace {
Display* xDisplay()
{
    if (QGuiApplication::platformName() != "xcb")
        return nullptr;
    auto* app = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return app ? app->display() : nullptr;
}
void listChild(Display* display, Window parent, Window child, bool add)
{
    const Atom atom = XInternAtom(display, "_KDE_NET_WM_STEREO_CONTENT_CHILDREN", False);
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char* data = nullptr;
    std::vector<unsigned long> children;
    if (XGetWindowProperty(display, parent, atom, 0, 1024, False, XA_WINDOW,
                &type, &format, &count, &remaining, &data) == Success
            && type == XA_WINDOW && format == 32 && !remaining) {
        auto* values = reinterpret_cast<unsigned long*>(data);
        for (unsigned long i = 0; i < count; i++)
            if (values[i] != child)
                children.push_back(values[i]);
    }
    XFree(data);
    if (add)
        children.push_back(child);
    XChangeProperty(display, parent, atom, XA_WINDOW, 32, PropModeReplace,
            reinterpret_cast<const unsigned char*>(children.data()), children.size());
}
}

struct NativeX11Layer::Private {
    QPointer<QWidget> area;
    QPointer<QOpenGLContext> qtContext;
    Display* display = nullptr;
    Window parent = 0, child = 0;
    Colormap colormap = 0;
    GLXWindow window = 0;
    GLXContext videoContext = nullptr, savedContext = nullptr;
    GLXDrawable savedDraw = 0, savedRead = 0;
    QSize pixels;
    QRect destination;
    int views = 0;
    GLuint vao = 0, buffers[2] = {};
    bool current = false;
    bool failed = false;
    void reset()
    {
        if (videoContext) {
            GLXContext oldContext = glXGetCurrentContext();
            GLXDrawable oldDraw = glXGetCurrentDrawable(), oldRead = glXGetCurrentReadDrawable();
            if (qtContext && window && glXMakeContextCurrent(display, window, window, videoContext)) {
                auto* f = qtContext->extraFunctions();
                f->glDeleteVertexArrays(1, &vao);
                f->glDeleteBuffers(2, buffers);
            }
            glXMakeContextCurrent(display, oldDraw, oldRead, oldContext);
        }
        if (current)
            glXMakeContextCurrent(display, savedDraw, savedRead, savedContext);
        if (videoContext) glXDestroyContext(display, videoContext);
        if (window) glXDestroyWindow(display, window);
        if (child) {
            listChild(display, parent, child, false);
            XDestroyWindow(display, child);
        }
        if (colormap) XFreeColormap(display, colormap);
        if (display) XFlush(display);
        parent = child = colormap = window = 0;
        videoContext = nullptr;
        vao = 0;
        current = false;
        views = 0;
        pixels = {};
        destination = {};
    }
    ~Private() { reset(); }
};
NativeX11Layer::NativeX11Layer(QWidget* area) : d(std::make_unique<Private>()) { d->area = area; }
NativeX11Layer::~NativeX11Layer() = default;
bool NativeX11Layer::supported()
{
    Display* display = xDisplay();
    return display && stereo_supported_x11(display) >= 8;
}
bool NativeX11Layer::failed() const { return d->failed; }
void NativeX11Layer::hide() { d->reset(); }
bool NativeX11Layer::begin(const QSize& eyeSize, int views, const QRect& destination)
{
    if (!d->area || !d->area->isVisible() || eyeSize.isEmpty() || destination.isEmpty())
        return false;
    Display* display = xDisplay();
    if (!display || !glXGetCurrentContext()) {
        d->failed = true;
        return false;
    }
    const Window parent = d->area->window()->winId();
    if (parent != d->parent)
        d->reset();
    d->display = display;
    d->qtContext = QOpenGLContext::currentContext();
    d->savedContext = glXGetCurrentContext();
    d->savedDraw = glXGetCurrentDrawable();
    d->savedRead = glXGetCurrentReadDrawable();
    const QSize pixels(eyeSize.width() * views, eyeSize.height());
    if (!d->child) {
        int count = 0;
        int attributes[] = { GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DOUBLEBUFFER, True, GLX_STEREO, False,
            GLX_RED_SIZE, 10, GLX_GREEN_SIZE, 10, GLX_BLUE_SIZE, 10, GLX_ALPHA_SIZE, 0, None };
        GLXFBConfig* configs = glXChooseFBConfig(display, DefaultScreen(display), attributes, &count);
        if (!configs || !count) {
            XFree(configs);
            attributes[11] = attributes[13] = attributes[15] = 8;
            configs = glXChooseFBConfig(display, DefaultScreen(display), attributes, &count);
        }
        if (!configs || !count) {
            d->failed = true;
            XFree(configs);
            return false;
        }
        const GLXFBConfig config = configs[0];
        XFree(configs);
        XVisualInfo* visual = glXGetVisualFromFBConfig(display, config);
        if (!visual) {
            d->failed = true;
            return false;
        }
        d->parent = parent;
        d->colormap = XCreateColormap(display, parent, visual->visual, AllocNone);
        XSetWindowAttributes wa = {};
        wa.colormap = d->colormap;
        wa.border_pixel = 0;
        d->child = XCreateWindow(display, parent, 0, 0, pixels.width(), pixels.height(), 0,
                visual->depth, InputOutput, visual->visual, CWColormap | CWBorderPixel, &wa);
        XFree(visual);
        XShapeCombineRectangles(display, d->child, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
        d->window = glXCreateWindow(display, config, d->child, nullptr);
        auto createContext = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(glXGetProcAddressARB(
                    reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
        const int contextAttributes[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, None };
        if (createContext)
            d->videoContext = createContext(display, config, d->savedContext, True, contextAttributes);
        if (!d->window || !d->videoContext) {
            d->failed = true;
            d->reset();
            return false;
        }
        listChild(display, parent, d->child, true);
        XMapWindow(display, d->child);
    }
    if (pixels != d->pixels) {
        XResizeWindow(display, d->child, pixels.width(), pixels.height());
        d->pixels = pixels;
    }
    if (views != d->views) {
        if (stereo_declare_x11(display, d->child, views == 2 ? STEREO_SBS_FULL : STEREO_NONE,
                    STEREO_CLASS_VIDEO, STEREO_VIDEO_CURRENT) < 0)
            return false;
        d->views = views;
        d->destination = {};
    }
    const QRect logical = destination.translated(d->area->mapTo(d->area->window(), QPoint()));
    const qreal scale = d->area->devicePixelRatioF();
    const QRect placed(qRound(logical.x() * scale), qRound(logical.y() * scale),
            qRound(logical.width() * scale), qRound(logical.height() * scale));
    if (placed != d->destination) {
        const stereo_x11_child_place place = { placed.x(), placed.y(), unsigned(placed.width()), unsigned(placed.height()), 1 };
        if (stereo_declare_x11_child_place(display, d->child, &place) < 0)
            return false;
        d->destination = placed;
    }
    auto* f = d->qtContext->extraFunctions();
    GLsync fence = f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    f->glFlush();
    if (!glXMakeContextCurrent(display, d->window, d->window, d->videoContext)) {
        d->failed = true;
        f->glDeleteSync(fence);
        return false;
    }
    d->current = true;
    auto bindFramebuffer = reinterpret_cast<void (*)(GLenum, GLuint)>(glXGetProcAddressARB(
                reinterpret_cast<const GLubyte*>("glBindFramebuffer")));
    bindFramebuffer(GL_FRAMEBUFFER, 0);
    f->glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
    f->glDeleteSync(fence);
    if (!d->vao) {
        const GLfloat vertices[] = { -1,1,0,1, 1,1,1,1, 1,-1,1,0, -1,-1,0,0 };
        const GLushort indices[] = { 0,3,1, 1,3,2 };
        f->glGenVertexArrays(1, &d->vao);
        f->glBindVertexArray(d->vao);
        f->glGenBuffers(2, d->buffers);
        f->glBindBuffer(GL_ARRAY_BUFFER, d->buffers[0]);
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<void*>(2 * sizeof(GLfloat)));
        f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->buffers[1]);
        f->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }
    f->glBindVertexArray(d->vao);
    return true;
}
void NativeX11Layer::end()
{
    if (!d->current)
        return;
    glXSwapBuffers(d->display, d->window);
    glXMakeContextCurrent(d->display, d->savedDraw, d->savedRead, d->savedContext);
    d->current = false;
}
