// Exercise the real player, sink and widget without GUI controls over the pixels.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSet>
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
        QImage pixels(width(), height(), QImage::Format_RGBA8888);
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(currentTargetBuffer()));
        glReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels.bits());
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
