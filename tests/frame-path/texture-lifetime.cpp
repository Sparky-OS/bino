/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QtMultimedia/private/qvideoframe_p.h>
#include <rhi/qrhi.h>
#include "videotextureimporter.hpp"

#define CHECK(condition) do { if (!(condition)) qFatal("line %d: %s", __LINE__, #condition); } while (false)
struct Counts { int buffers = 0; int textures = 0; int maps = 0; int initialized = 0; };
class Textures : public QVideoFrameTextures
{
    Counts& counts;
    std::unique_ptr<QRhiTexture> plane;
public:
    Textures(Counts& counts, QRhi& rhi, bool wrongFormat) : counts(counts),
        plane(rhi.newTexture(wrongFormat ? QRhiTexture::RG8 : QRhiTexture::RGBA8, {8, 4}))
    {
        CHECK(plane->create());
        ++counts.textures;
    }
    ~Textures() override { --counts.textures; }
    QRhiTexture* texture(uint index) const override { return index ? nullptr : plane.get(); }
};
class Buffer : public QHwVideoBuffer
{
    Counts& counts;
    bool available;
    bool wrongFormat;
    QRhi* initialized = nullptr;
public:
    Buffer(Counts& counts, bool available, bool wrongFormat) : QHwVideoBuffer(QVideoFrame::NoHandle),
        counts(counts), available(available), wrongFormat(wrongFormat) { ++counts.buffers; }
    ~Buffer() override { --counts.buffers; }
    MapData map(QVideoFrame::MapMode) override { ++counts.maps; return {}; }
    void initTextureConverter(QRhi& rhi) override
    {
        ++counts.initialized;
        initialized = &rhi;
        m_type = available ? QVideoFrame::RhiTextureHandle : QVideoFrame::NoHandle;
    }
    QVideoFrameTexturesUPtr mapTextures(QRhi& rhi, QVideoFrameTexturesUPtr&) override
    {
        CHECK(initialized == &rhi);
        return std::make_unique<Textures>(counts, rhi, wrongFormat);
    }
};
static QVideoFrame frame(Counts& counts, bool available = true, bool wrongFormat = false)
{
    return QVideoFramePrivate::createFrame(std::make_unique<Buffer>(counts, available, wrongFormat),
            {{8, 4}, QVideoFrameFormat::Format_RGBA8888});
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    CHECK(surface.isValid());
    Counts counts;
    VideoTextureImporter importer;
    VideoTextureImporter::Planes planes;
    CHECK(!importer.tryImport(frame(counts), &planes)); // No current context.
    CHECK(counts.buffers == 0 && counts.maps == 0);
    for (int cycle = 0; cycle < 3; cycle++) {
        auto context = std::make_unique<QOpenGLContext>();
        context->setFormat(format);
        CHECK(context->create());
        CHECK(context->makeCurrent(&surface));
        auto* f = context->extraFunctions();
        CHECK(!importer.tryImport(frame(counts, false), &planes));
        CHECK(!importer.tryImport(frame(counts, true, true), &planes));
        CHECK(counts.buffers == 0 && counts.textures == 0 && counts.maps == 0);
        for (int n = 0; n < 16; n++) {
            CHECK(importer.tryImport(frame(counts), &planes));
            CHECK(QOpenGLContext::currentContext() == context.get());
            CHECK(context->surface() == &surface);
            CHECK(counts.buffers > 0 && counts.textures > 0);
            CHECK(planes.count == 1 && planes.format == 1 && planes.channelOrder == 0);
            CHECK(f->glIsTexture(planes.textures[0]) && f->glIsSampler(planes.samplers[0]));
            GLuint fbo;
            f->glGenFramebuffers(1, &fbo);
            f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planes.textures[0], 0);
            CHECK(f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            f->glClearColor(1, 0.5, 0, 1);
            f->glClear(GL_COLOR_BUFFER_BIT);
            unsigned char pixel[4] = {};
            f->glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            CHECK(pixel[0] == 255 && pixel[1] >= 127 && pixel[1] <= 128 && pixel[2] == 0 && pixel[3] == 255);
            f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
            f->glDeleteFramebuffers(1, &fbo);
            importer.submitted();
            CHECK(counts.buffers <= 4 && counts.textures <= 4);
            CHECK(context->surface() == &surface);
        }
        if (cycle == 0) {
            importer.release();
            CHECK(QOpenGLContext::currentContext() == context.get());
            CHECK(context->surface() == &surface);
        } else {
            context.reset(); // aboutToBeDestroyed must drain leases before QRhi is lost.
        }
        CHECK(counts.buffers == 0 && counts.textures == 0 && counts.maps == 0);
    }
    CHECK(counts.initialized == 54);
    qInfo("PASS: live texture handles, retained source frames, bounded leases, rejected format, no CPU map, context teardown/recreation");
}
