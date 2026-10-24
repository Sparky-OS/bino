// Exercise the real player, sink and widget without GUI controls over the pixels.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenuBar>
#include "overlay-subtitle.hpp"
#include "overlay-ui.hpp"
#include "overlay-audio.hpp"
#include "widget.hpp"
#include "commandinterpreter.hpp"
#include "frametrace.hpp"
#include "tools.hpp"
#include "log.hpp"
#include "native-target.hpp"

class Probe : public Widget
{
public:
    QString directory;
    QSet<QString> captured;
    bool held = false;
    int stage = 0;
    Probe(bool stereo, const QString& dir, QWidget* parent = nullptr) :
        Widget(stereo ? Output_OpenGL_Stereo : Output_Left, 90.0f, parent), directory(dir) {}

protected:
    void nativeFrameRendered(const QSize&, int views) override
    {
        const auto pts = FrameTrace::pts;
        if ((pts < 500000 || pts > 1500000) && !held)
            return;
        if (qEnvironmentVariableIsSet("PIXEL_HOLD") && !held && pts >= 1500000) {
            held = true;
            QTimer::singleShot(0, this, [this, pts] {
                Bino::instance()->pause();
                if (qEnvironmentVariableIsSet("PIXEL_UI")) {
                    Bino::instance()->overlayUIPointerMove(QPointF(0.5, 0.2), false);
                    Bino::instance()->overlayUIPointerPress(QPointF(0.5, 0.2), true);
                }
                QFile ready(directory + "/held");
                if (!ready.open(QIODevice::WriteOnly))
                    qFatal("cannot save held timestamp");
                ready.write(QByteArray::number(pts));
            });
        }
        GLint framebuffer = -1;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
        FrameTrace::event("native_read_framebuffer", FrameTrace::timestamp(), 0, pts, framebuffer);
        if (framebuffer != 0)
            qFatal("native EGL readback bound framebuffer %d instead of the window buffer", framebuffer);
        GLint red = 0, alpha = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha);
        FrameTrace::event("video_precision", FrameTrace::timestamp(), 0, pts, red, alpha);
        const QSize target = nativeTargetSize();
        const int w = target.width(), h = target.height();
        QImage packed(w, h, QImage::Format_RGBA8888);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, packed.bits());
        packed = packed.flipped(Qt::Vertical);
        FrameTrace::event("target_geometry", FrameTrace::timestamp(), 0, pts, w / views, h);
        FrameTrace::event("packed_geometry", FrameTrace::timestamp(), 0, pts, w, h);
        if (held) {
            QFile ready(directory + "/held");
            if (!ready.open(QIODevice::WriteOnly)) qFatal("cannot save held timestamp");
            ready.write(QByteArray::number(pts));
            packed.save(directory + "/held-buffer.png");
            if (qEnvironmentVariableIsSet("PIXEL_LIFECYCLE")) {
                packed.save(directory + QString("/stage%1.png").arg(stage));
                QFile info(directory + QString("/stage%1.json").arg(stage));
                if (!info.open(QIODevice::WriteOnly)) qFatal("cannot save lifecycle stage");
                info.write(QJsonDocument(QJsonObject{{"pts", double(pts)}, {"views", views},
                    {"width", w}, {"height", h}, {"windowWidth", width()}, {"windowHeight", height()}}).toJson());
            }
        }
        if (pts < 500000 || pts > 1500000)
            return;
        for (int eye = 0; eye < views; eye++) {
            const QString key = QString::number(pts) + "-" + QString::number(eye);
            if (!captured.contains(key)) {
                if (!packed.copy(eye * w / views, 0, w / views, h).save(directory + "/" + key + ".png"))
                    qFatal("cannot save native video buffer");
                captured.insert(key);
            }
        }
    }
    void paintGL() override
    {
        Widget::paintGL();
        if (usesNativeVideo()) {
            if (held) {
                QImage overlay(width(), height(), QImage::Format_RGBA8888);
                glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
                glReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, overlay.bits());
                overlay.flipped(Qt::Vertical).save(directory + "/overlay.png");
                GLint red = 0, alpha = 0;
                glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
                glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha);
                FrameTrace::event("overlay_precision", FrameTrace::timestamp(), 0, FrameTrace::pts, red, alpha);
                QFile geometry(directory + "/placement.json");
                if (!geometry.open(QIODevice::WriteOnly)) qFatal("cannot save placement");
                const QPoint position = mapToGlobal(QPoint());
                geometry.write(QJsonDocument(QJsonObject{{"x", position.x()}, {"y", position.y()},
                    {"width", width()}, {"height", height()}}).toJson());
                if (qEnvironmentVariableIsSet("PIXEL_UI")) {
                    QByteArray state;
                    QDataStream writer(&state, QIODevice::WriteOnly);
                    Bino::instance()->serializeDynamicData(writer);
                    QDataStream reader(state);
                    bool noFrame, swap, shown;
                    reader >> noFrame;
                    if (!noFrame) {
                        VideoFrame frame;
                        reader >> frame;
                    }
                    OverlayAudio audio;
                    OverlayUI reference;
                    reader >> swap >> audio >> reference >> shown;
                    if (reader.status() != QDataStream::Ok) qFatal("invalid UI reference state");
                    if (shown) {
                        reference.redraw(width(), height());
                        reference.image().save(directory + "/ui-reference.png");
                    }
                }
                if (qEnvironmentVariableIsSet("PIXEL_SUBTITLE")) {
                    OverlaySubtitle reference;
                    reference.updateParameters("Native size: Aa Wm 0123456789");
                    reference.redraw(width(), height());
                    reference.image().save(directory + "/subtitle-reference.png");
                }
            }
            return;
        }
        const auto pts = FrameTrace::pts;
        if (pts < 500000 || pts > 1500000)
            return;
        const QString key = QString::number(pts) + "-" + QString::number(currentTargetBuffer());
        if (captured.contains(key))
            return;
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(currentTargetBuffer()));
        GLint type = 0, object = 0, bufferWidth = 0, bufferHeight = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
        if (type == GL_TEXTURE) {
            QOpenGLFunctions_3_3_Core functions;
            functions.initializeOpenGLFunctions();
            GLint previous;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
            glBindTexture(GL_TEXTURE_2D, object);
            functions.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &bufferWidth);
            functions.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &bufferHeight);
            glBindTexture(GL_TEXTURE_2D, previous);
        } else if (type == GL_RENDERBUFFER) {
            GLint previous;
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
            glBindRenderbuffer(GL_RENDERBUFFER, object);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &bufferWidth);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &bufferHeight);
            glBindRenderbuffer(GL_RENDERBUFFER, previous);
        } else {
            qFatal("unexpected target attachment");
        }
        FrameTrace::event("target_geometry", FrameTrace::timestamp(), 0, pts, bufferWidth, bufferHeight);
        QImage pixels(bufferWidth, bufferHeight, QImage::Format_RGBA8888);
        glReadPixels(0, 0, bufferWidth, bufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.bits());
        if (!pixels.flipped(Qt::Vertical).save(directory + "/" + key + ".png"))
            qFatal("cannot save target buffer");
        captured.insert(key);
    }
};

int main(int argc, char** argv)
{
    if (argc != 5)
        return 2; // clip, capture directory, mono|stereo, stereo|fallback
    const bool stereo = QString::fromLocal8Bit(argv[4]) == "stereo";
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStereo(false);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if (stereo && NativeVideoLayer::supported()) {
        format.setStereo(false);
        format.setAlphaBufferSize(8);
    } else {
        format.setStereo(stereo);
    }
    QSurfaceFormat::setDefaultFormat(format);
    SetLogLevel(Log_Level_Debug);
    initializeOpenGLType(format);
    Playlist playlist;
    CommandInterpreter commands;
    const QString mode = qEnvironmentVariable("PIXEL_MODE",
        QString::fromLocal8Bit(argv[3]) == "mono" ? "mono" : "left-right");
    bool valid = false;
    const InputMode input = inputModeFromString(mode, &valid);
    if (!valid) qFatal("invalid fixture input mode");
    PlaylistEntry entry(QUrl::fromLocalFile(argv[1]), input, Surround_Off);
    if (qEnvironmentVariableIsSet("PIXEL_SUBTITLE"))
        entry.subtitleTrack = 0;
    playlist.append(entry);
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice());
    bino.startPlaylistMode();
    QMainWindow frame;
    const bool wrapped = qEnvironmentVariableIsSet("PIXEL_WRAPPED");
    Probe widget(stereo, argv[2], wrapped ? &frame : nullptr);
    if (wrapped) {
        frame.setCentralWidget(&widget);
        frame.menuBar()->addMenu("Ordinary controls")->addAction("Both eyes identical");
        frame.setWindowTitle("Native video, ordinary controls");
    }
    const int width = qEnvironmentVariableIntValue("PIXEL_WIDTH");
    const int height = qEnvironmentVariableIntValue("PIXEL_HEIGHT");
    widget.resize(width > 0 ? width : 320, height > 0 ? height : 180);
    if (wrapped) {
        frame.resize(widget.width(), widget.height() + frame.menuBar()->sizeHint().height());
        frame.show();
    } else {
        widget.show();
    }
    if (qEnvironmentVariableIsSet("PIXEL_LIFECYCLE")) {
        QTimer::singleShot(2500, &app, [&]() { widget.stage = 1; widget.resize(480, 270); });
        QTimer::singleShot(3200, &app, [&]() { widget.hide(); });
        QTimer::singleShot(3500, &app, [&]() { widget.stage = 2; widget.show(); });
        QTimer::singleShot(4500, &app, [&]() { widget.stage = 3; bino.setInputMode(Input_Mono); });
        QTimer::singleShot(5500, &app, [&]() { widget.stage = 4; bino.setInputMode(Input_Left_Right); });
    }
    const bool warmup = qEnvironmentVariableIsSet("PIXEL_WARMUP");
    if (warmup) {
        for (int delay : {4000, 8000, 12000})
            QTimer::singleShot(delay, &app, [&]() { bino.setPosition(0.0f); bino.play(); });
    }
    QTimer::singleShot(qEnvironmentVariableIsSet("PIXEL_HOLD") ? 12000 : warmup ? 16000 : 3500, &app, [&]() {
        if (widget.isOpenGLStereo() != stereo || widget.captured.size() < (stereo && QString::fromLocal8Bit(argv[3]) != "mono" ? 30 : 15)) {
            qWarning("unexpected stereo capability %d or capture count %lld",
                    widget.isOpenGLStereo(), static_cast<long long>(widget.captured.size()));
            app.exit(3);
        } else {
            app.quit();
        }
    });
    return app.exec();
}
