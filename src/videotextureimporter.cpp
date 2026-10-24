/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "videotextureimporter.hpp"
#include "frametrace.hpp"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPointer>
#include <QtMultimedia/private/qvideoframe_p.h>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <deque>

struct VideoTextureImporter::Private
{
    struct Frame {
        QVideoFrame source;
        QVideoFrameTexturesUPtr textures;
        GLsync fence = nullptr;
    };
    QPointer<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QRhi> rhi;
    QMetaObject::Connection destruction;
    GLuint samplers[2] = {};
    Frame current;
    std::deque<Frame> pending;

    void collect(bool all)
    {
        if (!context || !rhi)
            return;
        auto* f = context->extraFunctions();
        if (all && !pending.empty())
            f->glFinish(); // Context teardown only; steady-state release polls fences.
        while (!pending.empty()) {
            const GLenum result = f->glClientWaitSync(pending.front().fence, 0, 0);
            if (!all && result == GL_WAIT_FAILED) {
                qWarning("Video import fence wait failed; completing outstanding reads");
                f->glFinish();
                all = true;
            }
            if (!all && result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
                break;
            f->glDeleteSync(pending.front().fence);
            pending.pop_front();
        }
    }
};

// Qt's importer and texture destructors can make its fallback surface current.
class RestoreSurface
{
    QPointer<QOpenGLContext> context = QOpenGLContext::currentContext();
    QSurface* surface = context ? context->surface() : nullptr;
    QPointer<QObject> surfaceObject = dynamic_cast<QObject*>(surface);
public:
    ~RestoreSurface()
    {
        if (context && surfaceObject)
            context->makeCurrent(surface);
    }
};

VideoTextureImporter::VideoTextureImporter() : d(new Private) {}
VideoTextureImporter::~VideoTextureImporter() { release(); }

void VideoTextureImporter::release()
{
    RestoreSurface restore;
    QObject::disconnect(d->destruction);
    if (d->context && d->surface)
        d->context->makeCurrent(d->surface.get());
    d->collect(true);
    d->current = {};
    d->pending.clear();
    if (d->context)
        d->context->extraFunctions()->glDeleteSamplers(2, d->samplers);
    d->samplers[0] = d->samplers[1] = 0;
    d->rhi.reset();
    d->surface.reset();
    d->context = nullptr;
}

bool VideoTextureImporter::tryImport(const QVideoFrame& frame, Planes* planes)
{
    RestoreSurface restore;
    auto* context = QOpenGLContext::currentContext();
    auto* buffer = QVideoFramePrivate::hwBuffer(frame);
    if (!context || !buffer)
        return false;
    if (d->context && d->context != context)
        release();
    if (!d->rhi) {
        d->context = context;
        d->surface.reset(QRhiGles2InitParams::newFallbackSurface(context->format()));
        if (!d->surface || !d->surface->isValid())
            return false;
        QRhiGles2InitParams params;
        params.format = context->format();
        params.fallbackSurface = d->surface.get();
        QRhiGles2NativeHandles native;
        native.context = context;
        d->rhi.reset(QRhi::create(QRhi::OpenGLES2, &params, {}, &native));
        if (!d->rhi)
            return false;
        auto* f = context->extraFunctions();
        f->glGenSamplers(2, d->samplers);
        for (int p = 0; p < 2; p++) {
            f->glSamplerParameteri(d->samplers[p], GL_TEXTURE_MIN_FILTER, p ? GL_LINEAR : GL_NEAREST);
            f->glSamplerParameteri(d->samplers[p], GL_TEXTURE_MAG_FILTER, p ? GL_LINEAR : GL_NEAREST);
            f->glSamplerParameteri(d->samplers[p], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            f->glSamplerParameteri(d->samplers[p], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        d->destruction = connect(context, &QOpenGLContext::aboutToBeDestroyed,
                this, &VideoTextureImporter::release, Qt::DirectConnection);
    }
    d->collect(false);
    buffer->initTextureConverter(*d->rhi);
    if (buffer->handleType() != QVideoFrame::RhiTextureHandle)
        return false;

    Planes result;
    int chromaWidth = 1, chromaHeight = 1;
    switch (frame.pixelFormat()) {
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_P010:
    case QVideoFrameFormat::Format_P016:
        result.format = 4; result.count = 2; chromaWidth = chromaHeight = 2;
        break;
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_YV12:
        result.format = frame.pixelFormat() == QVideoFrameFormat::Format_YV12 ? 3 : 2;
        result.count = 3; chromaWidth = chromaHeight = 2;
        break;
    case QVideoFrameFormat::Format_YUV422P:
        result.format = 2; result.count = 3; chromaWidth = 2;
        break;
    case QVideoFrameFormat::Format_Y8:
    case QVideoFrameFormat::Format_Y16:
        result.format = 5; result.count = 1;
        break;
    case QVideoFrameFormat::Format_ARGB8888:
    case QVideoFrameFormat::Format_ARGB8888_Premultiplied:
    case QVideoFrameFormat::Format_XRGB8888:
        result.channelOrder = 1;
        result.format = 1; result.count = 1;
        break;
    case QVideoFrameFormat::Format_ABGR8888:
    case QVideoFrameFormat::Format_XBGR8888:
        result.channelOrder = 2;
        result.format = 1; result.count = 1;
        break;
    case QVideoFrameFormat::Format_BGRA8888:
    case QVideoFrameFormat::Format_BGRA8888_Premultiplied:
    case QVideoFrameFormat::Format_BGRX8888:
    case QVideoFrameFormat::Format_RGBA8888:
    case QVideoFrameFormat::Format_RGBX8888:
        result.format = 1; result.count = 1;
        break;
    default:
        return false;
    }
    QVideoFrameTexturesUPtr unused;
    auto textures = buffer->mapTextures(*d->rhi, unused);
    if (!textures)
        return false;
    for (int p = 0; p < result.count; p++) {
        auto* texture = textures->texture(p);
        const QSize size(p ? (frame.width() + chromaWidth - 1) / chromaWidth : frame.width(),
                p ? (frame.height() + chromaHeight - 1) / chromaHeight : frame.height());
        if (!texture || texture->pixelSize() != size || (texture->flags() & (QRhiTexture::CubeMap | QRhiTexture::ExternalOES
                    | QRhiTexture::ThreeDimensional | QRhiTexture::TextureRectangleGL
                    | QRhiTexture::TextureArray | QRhiTexture::OneDimensional | QRhiTexture::sRGB)))
            return false;
        // Qt may emulate unsupported normalized 16-bit formats with byte textures.
        // Those require different shaders; leave them to the CPU fallback.
        const bool wide = frame.pixelFormat() == QVideoFrameFormat::Format_P010
                || frame.pixelFormat() == QVideoFrameFormat::Format_P016
                || frame.pixelFormat() == QVideoFrameFormat::Format_Y16;
        const auto expected = result.format == 1 ? QRhiTexture::RGBA8
                : result.format == 4 && p ? (wide ? QRhiTexture::RG16 : QRhiTexture::RG8)
                : wide ? QRhiTexture::R16 : QRhiTexture::R8;
        if (texture->format() != expected
                && !(result.format == 1 && texture->format() == QRhiTexture::BGRA8))
            return false;
        result.textures[p] = static_cast<unsigned int>(texture->nativeTexture().object);
        result.samplers[p] = d->samplers[p ? 1 : 0];
        if (!result.textures[p])
            return false;
    }
    d->current = {frame, std::move(textures), nullptr};
    *planes = result;
    FrameTrace::event("texture_import", FrameTrace::timestamp(), 0, frame.startTime(),
            frame.pixelFormat(), result.count);
    return true;
}

void VideoTextureImporter::submitted()
{
    if (!d->current.textures)
        return;
    RestoreSurface restore;
    auto* f = d->context->extraFunctions();
    d->current.fence = f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!d->current.fence) {
        qWarning("Cannot fence imported video frame; completing its reads");
        f->glFinish();
        d->current = {};
        return;
    }
    d->pending.push_back(std::move(d->current));
    d->current = {};
    // Bound the leases on decoder surfaces if the GPU falls behind the producer.
    if (d->pending.size() > 4) {
        while (f->glClientWaitSync(d->pending.front().fence, GL_SYNC_FLUSH_COMMANDS_BIT,
                    1000000) == GL_TIMEOUT_EXPIRED) {}
        d->collect(false);
    }
}
