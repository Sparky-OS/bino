// Exercise the real player, sink and widget without GUI controls over the pixels.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>
#include <QTimer>
#include "widget.hpp"
#include "commandinterpreter.hpp"
#include "frametrace.hpp"
#include "tools.hpp"
#include "log.hpp"

class Probe : public Widget
{
public:
    QString directory;
    QSet<QString> captured;
    Probe(bool stereo, const QString& dir) :
        Widget(stereo ? Output_OpenGL_Stereo : Output_Left, 90.0f), directory(dir) {}

protected:
    void paintGL() override
    {
        Widget::paintGL();
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
        if (!pixels.mirrored().save(directory + "/" + key + ".png"))
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
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStereo(stereo);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    SetLogLevel(Log_Level_Debug);
    initializeOpenGLType(format);
    Playlist playlist;
    CommandInterpreter commands;
    playlist.append(PlaylistEntry(QUrl::fromLocalFile(argv[1]),
                QString::fromLocal8Bit(argv[3]) == "mono" ? Input_Mono : Input_Left_Right,
                Surround_Off));
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice());
    bino.startPlaylistMode();
    Probe widget(stereo, argv[2]);
    const int width = qEnvironmentVariableIntValue("PIXEL_WIDTH");
    const int height = qEnvironmentVariableIntValue("PIXEL_HEIGHT");
    widget.resize(width > 0 ? width : 320, height > 0 ? height : 180);
    widget.show();
    QTimer::singleShot(3500, &app, [&]() {
        if (widget.isOpenGLStereo() != stereo || widget.captured.size() < (stereo ? 30 : 15)) {
            qWarning("unexpected stereo capability %d or capture count %lld",
                    widget.isOpenGLStereo(), static_cast<long long>(widget.captured.size()));
            app.exit(3);
        } else {
            app.quit();
        }
    });
    return app.exec();
}
