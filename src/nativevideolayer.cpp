/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nativevideolayer.hpp"
#include "nativex11layer.hpp"

#include <QGuiApplication>
#include <QPaintEvent>
#include <QPlatformSurfaceEvent>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPainter>
#include <QPointer>
#include <QWidget>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>
#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <cstring>
#include <stereo-declare.h>
#include "wayland-viewporter-client-protocol.h"

namespace {
struct Connection {
    wl_display* display = nullptr;
    wl_event_queue* queue = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    wl_subcompositor* subcompositor = nullptr;
    wp_viewporter* viewporter = nullptr;
    bool initialized = false;

    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        auto* c = static_cast<Connection*>(data);
        if (!strcmp(interface, "wl_compositor"))
            c->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, qMin(version, 4u)));
        else if (!strcmp(interface, "wl_subcompositor"))
            c->subcompositor = static_cast<wl_subcompositor*>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
        else if (!strcmp(interface, "wp_viewporter"))
            c->viewporter = static_cast<wp_viewporter*>(wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
    }
    static void removed(void*, wl_registry*, uint32_t) {}
    Connection()
    {
        if (!QGuiApplication::platformName().startsWith("wayland"))
            return;
        auto* native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        display = native ? native->display() : nullptr;
        if (!display)
            return;
        queue = wl_display_create_queue(display);
        auto* wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), queue);
        registry = wl_display_get_registry(wrapper);
        wl_proxy_wrapper_destroy(wrapper);
        static const wl_registry_listener listener = { global, removed };
        wl_registry_add_listener(registry, &listener, this);
        wl_display_roundtrip_queue(display, queue);
        if (compositor && subcompositor && viewporter) {
            initialized = stereo_declare_wayland_init(display, queue) == 0;
            wl_display_roundtrip_queue(display, queue);
        }
    }
    ~Connection()
    {
        if (initialized) stereo_declare_wayland_finish();
        if (viewporter) wp_viewporter_destroy(viewporter);
        if (subcompositor) wl_subcompositor_destroy(subcompositor);
        if (compositor) wl_compositor_destroy(compositor);
        if (registry) wl_registry_destroy(registry);
        if (queue) wl_event_queue_destroy(queue);
    }
    bool supported() const { return initialized && stereo_supported_wayland(); }
};
std::shared_ptr<Connection> connection()
{
    static std::weak_ptr<Connection> shared;
    auto c = shared.lock();
    if (!c) {
        c = std::make_shared<Connection>();
        shared = c;
    }
    return c;
}

class Background : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() != QEvent::Paint)
            return false;
        auto* window = static_cast<QWidget*>(object);
        QRegion region = static_cast<QPaintEvent*>(event)->region();
        for (auto* area : window->findChildren<QWidget*>()) {
            if (area->property("binoNativeVideo").toBool() && area->isVisible())
                region -= QRect(area->mapTo(window, QPoint()), area->size());
        }
        if (!window->property("binoNativeVideo").toBool()) {
            QPainter painter(window);
            painter.setClipRegion(region);
            painter.fillRect(window->rect(), window->palette().brush(window->backgroundRole()));
        }
        return false;
    }
};
}

struct NativeVideoLayer::Private {
    QPointer<QWidget> area;
    std::unique_ptr<NativeX11Layer> x11;
    std::shared_ptr<Connection> connection;
    QPointer<QWindow> parent;
    wl_surface* parentSurface = nullptr;
    wl_surface* surface = nullptr;
    wl_subsurface* subsurface = nullptr;
    wp_viewport* viewport = nullptr;
    wl_egl_window* window = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLSurface savedDraw = EGL_NO_SURFACE;
    EGLSurface savedRead = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLContext videoContext = EGL_NO_CONTEXT;
    QPointer<QOpenGLContext> qtContext;
    GLuint vao = 0;
    GLuint buffers[2] = {};
    QSize pixels;
    QRect destination;
    int views = 0;
    bool current = false;
    bool failed = false;

    void reset()
    {
        if (x11)
            x11->hide();
        if (videoContext != EGL_NO_CONTEXT && eglSurface != EGL_NO_SURFACE) {
            EGLDisplay oldDisplay = eglGetCurrentDisplay();
            EGLContext oldContext = eglGetCurrentContext();
            EGLSurface oldDraw = eglGetCurrentSurface(EGL_DRAW);
            EGLSurface oldRead = eglGetCurrentSurface(EGL_READ);
            if (qtContext && eglMakeCurrent(display, eglSurface, eglSurface, videoContext)) {
                auto* f = qtContext->extraFunctions();
                f->glDeleteVertexArrays(1, &vao);
                f->glDeleteBuffers(2, buffers);
            }
            eglMakeCurrent(oldDisplay == EGL_NO_DISPLAY ? display : oldDisplay, oldDraw, oldRead, oldContext);
        }
        if (current)
            eglMakeCurrent(display, savedDraw, savedRead, context);
        if (videoContext != EGL_NO_CONTEXT)
            eglDestroyContext(display, videoContext);
        videoContext = EGL_NO_CONTEXT;
        vao = 0;
        if (eglSurface != EGL_NO_SURFACE)
            eglDestroySurface(display, eglSurface);
        if (window) wl_egl_window_destroy(window);
        if (surface) stereo_remove_wayland(surface);
        if (viewport) wp_viewport_destroy(viewport);
        if (subsurface) wl_subsurface_destroy(subsurface);
        if (surface) wl_surface_destroy(surface);
        parent = nullptr;
        parentSurface = nullptr;
        surface = nullptr;
        subsurface = nullptr;
        viewport = nullptr;
        window = nullptr;
        eglSurface = EGL_NO_SURFACE;
        current = false;
        views = 0;
        pixels = {};
        destination = {};
    }
    ~Private() { reset(); }
};

NativeVideoLayer::NativeVideoLayer(QWidget* area) : QObject(area), d(std::make_unique<Private>())
{
    d->area = area;
    area->installEventFilter(this);
    if (QGuiApplication::platformName() == "xcb")
        d->x11 = std::make_unique<NativeX11Layer>(area);
    else
        d->connection = connection();
}
NativeVideoLayer::~NativeVideoLayer() = default;
bool NativeVideoLayer::supported()
{
    return QGuiApplication::platformName() == "xcb" ? NativeX11Layer::supported() : connection()->supported();
}
void NativeVideoLayer::prepareWindow(QWidget* window)
{
    if (window->property("binoNativePrepared").toBool())
        return;
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setProperty("binoNativePrepared", true);
    window->installEventFilter(new Background(window));
}
bool NativeVideoLayer::failed() const { return d->x11 ? d->x11->failed() : d->failed; }
void NativeVideoLayer::hide() { d->reset(); }
bool NativeVideoLayer::eventFilter(QObject*, QEvent* event)
{
    if (event->type() == QEvent::Hide
            || (event->type() == QEvent::PlatformSurface
                && static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed))
        hide();
    return false;
}
bool NativeVideoLayer::begin(const QSize& eyeSize, int views, const QRect& destination)
{
    if (d->x11) {
        if (d->area && d->area->window()->windowHandle())
            d->area->window()->windowHandle()->installEventFilter(this);
        return d->x11->begin(eyeSize, views, destination);
    }
    if (!d->area || !d->area->isVisible() || eyeSize.isEmpty() || destination.isEmpty()
            || !d->connection->supported())
        return false;
    QWindow* parent = d->area->window()->windowHandle();
    auto* native = parent ? parent->nativeInterface<QNativeInterface::Private::QWaylandWindow>() : nullptr;
    if (!native || !native->surface())
        return false;
    if (parent != d->parent || native->surface() != d->parentSurface)
        d->reset();
    d->qtContext = QOpenGLContext::currentContext();
    d->display = eglGetCurrentDisplay();
    d->context = eglGetCurrentContext();
    d->savedDraw = eglGetCurrentSurface(EGL_DRAW);
    d->savedRead = eglGetCurrentSurface(EGL_READ);
    if (d->display == EGL_NO_DISPLAY || d->context == EGL_NO_CONTEXT) {
        d->failed = true;
        qWarning("Native video has no current EGL context");
        return false;
    }
    const QSize pixels(eyeSize.width() * views, eyeSize.height());
    if (!d->surface) {
        auto& c = *d->connection;
        d->parent = parent;
        parent->installEventFilter(this);
        d->parentSurface = native->surface();
        d->surface = wl_compositor_create_surface(c.compositor);
        d->subsurface = wl_subcompositor_get_subsurface(c.subcompositor, d->surface, d->parentSurface);
        wl_subsurface_place_below(d->subsurface, d->parentSurface);
        wl_subsurface_set_desync(d->subsurface);
        d->viewport = wp_viewporter_get_viewport(c.viewporter, d->surface);
        wl_region* empty = wl_compositor_create_region(c.compositor);
        wl_surface_set_input_region(d->surface, empty);
        wl_region_destroy(empty);
        d->window = wl_egl_window_create(d->surface, pixels.width(), pixels.height());
        EGLint count = 0;
        EGLConfig config = nullptr;
        const bool es = QOpenGLContext::currentContext()->isOpenGLES();
        EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, es ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT,
            EGL_RED_SIZE, 10, EGL_GREEN_SIZE, 10, EGL_BLUE_SIZE, 10, EGL_ALPHA_SIZE, 0, EGL_NONE };
        eglChooseConfig(d->display, attributes, &config, 1, &count);
        if (!count) {
            // Match the best window precision the driver exposes, as Qt's chooser does.
            attributes[5] = attributes[7] = attributes[9] = 8;
            eglChooseConfig(d->display, attributes, &config, 1, &count);
        }
        if (!count) {
            d->failed = true;
            qWarning("No native video EGL configuration");
            d->reset();
            return false;
        }
        const EGLint desktopContext[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
        const EGLint esContext[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        d->videoContext = eglCreateContext(d->display, config, d->context, es ? esContext : desktopContext);
        if (d->videoContext != EGL_NO_CONTEXT && d->window)
            d->eglSurface = eglCreateWindowSurface(d->display, config,
                    reinterpret_cast<EGLNativeWindowType>(d->window), nullptr);
        if (d->eglSurface == EGL_NO_SURFACE) {
            d->failed = true;
            qWarning("Native video EGL surface creation failed: %#x", eglGetError());
            d->reset();
            return false;
        }
    }
    if (pixels != d->pixels) {
        wl_egl_window_resize(d->window, pixels.width(), pixels.height(), 0, 0);
        d->pixels = pixels;
    }
    if (views != d->views) {
        if (views == 2) {
            if (stereo_declare_wayland(d->surface, STEREO_SBS_FULL, STEREO_CLASS_VIDEO, STEREO_VIDEO_CURRENT) < 0)
                return false;
        } else {
            stereo_remove_wayland(d->surface);
        }
        d->views = views;
    }
    const QRect placed = destination.translated(d->area->mapTo(d->area->window(), QPoint()))
            .translated(parent->frameMargins().left(), parent->frameMargins().top());
    if (placed != d->destination) {
        wl_subsurface_set_position(d->subsurface, placed.x(), placed.y());
        wp_viewport_set_destination(d->viewport, placed.width(), placed.height());
        d->destination = placed;
        parent->requestUpdate();
    }
    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    GLsync fence = f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    f->glFlush();
    if (!eglMakeCurrent(d->display, d->eglSurface, d->eglSurface, d->videoContext)) {
        d->failed = true;
        qWarning("Native video EGL context activation failed: %#x", eglGetError());
        f->glDeleteSync(fence);
        return false;
    }
    d->current = true;
    // QOpenGLFunctions redirects framebuffer zero to the widget's FBO.
    // This surface owns a real window buffer, outside Qt's framebuffer redirection.
    auto bindFramebuffer = reinterpret_cast<void (*)(GLenum, GLuint)>(eglGetProcAddress("glBindFramebuffer"));
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
    eglSwapInterval(d->display, 0);
    return true;
}
void NativeVideoLayer::end()
{
    if (d->x11) {
        d->x11->end();
        return;
    }
    if (!d->current)
        return;
    eglSwapBuffers(d->display, d->eglSurface);
    eglMakeCurrent(d->display, d->savedDraw, d->savedRead, d->context);
    d->current = false;
    wl_display_dispatch_queue_pending(d->connection->display, d->connection->queue);
    wl_display_flush(d->connection->display);
}
