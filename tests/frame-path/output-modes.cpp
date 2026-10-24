// Exercise the real output menu and widget with a known, two-eye lossless source.
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QSettings>
#include "gui.hpp"
#include "commandinterpreter.hpp"
#include "tools.hpp"
#include "log.hpp"

int main(int argc, char** argv)
{
    if (argc != 4) return 2; // clip, output directory, mode number
    const int mode = QByteArray(argv[3]).toInt();
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStereo(mode == 2);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    initializeOpenGLType(format);
    SetLogLevel(Log_Level_Debug);
    Playlist playlist;
    CommandInterpreter commands;
    playlist.append(PlaylistEntry(QUrl::fromLocalFile(argv[1]), Input_Left_Right, Surround_Off));
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice());
    bino.startPlaylistMode();
    Gui gui(Output_Left, 90.0f, false);
    Widget* widget = gui.findChild<Widget*>();
    if (!widget) qFatal("missing real GUI video widget");
    gui.show();
    QTimer::singleShot(1300, &app, [&] {
        bino.pause();
        gui.updateActions();
        const auto actions = gui.findChildren<QAction*>();
        QJsonArray labels;
        int outputs = 0;
        for (QAction* action : actions) {
            if (action->isCheckable() && action->text().startsWith("Output ")) {
                outputs++;
                labels.append(action->text());
#if !BINO_STEREO_DESKTOP
                if (action->data().toInt() == mode) {
                    if (!action->isEnabled()) qFatal("output action disabled");
                    action->trigger();
                }
#endif
            }
        }
#if BINO_STEREO_DESKTOP
        if (outputs != 0) qFatal("edition exposes output menu actions");
#else
        if (outputs != 30 || int(widget->outputMode()) != mode)
            qFatal("output menu incomplete or action did not select mode: %d", outputs);
        const QString name = outputModeToString(static_cast<OutputMode>(mode));
        bool ok = false;
        if (int(outputModeFromString(name, &ok)) != mode || !ok)
            qFatal("output mode string round trip failed");
        const QString documentedName = QString(name).replace("fullcolor", "full-color").replace("halfcolor", "half-color");
        if (int(outputModeFromString(documentedName, &ok)) != mode || !ok)
            qFatal("documented command-line output name not accepted");
        if (QSettings().value("OutputMode").toString() != name)
            qFatal("menu action did not save output mode");
        gui.setOutputMode(Output_Left);
        if (widget->outputMode() != Output_Left) qFatal("control interface ignores output mode");
        gui.setOutputMode(static_cast<OutputMode>(mode));
#endif
        const QPoint origin = widget->mapToGlobal(QPoint());
        QJsonObject info{{"mode", mode}, {"outputActions", labels}, {"x", origin.x()},
            {"y", origin.y()}, {"width", widget->width()}, {"height", widget->height()},
            {"screenHeight", widget->screen()->geometry().height()}, {"stereo", widget->isOpenGLStereo()}};
        QFile file(QString::fromLocal8Bit(argv[2]) + "/mode.json");
        if (!file.open(QIODevice::WriteOnly)) qFatal("cannot save mode information");
        file.write(QJsonDocument(info).toJson());
    });
    QTimer::singleShot(1900, &app, [&] {
        const QString dir = QString::fromLocal8Bit(argv[2]);
        if (!widget->grabFramebuffer(QOpenGLWidget::LeftBuffer).save(dir + "/first.png"))
            qFatal("cannot save first view");
        if (!widget->grabFramebuffer(mode == 2 ? QOpenGLWidget::RightBuffer : QOpenGLWidget::LeftBuffer).save(dir + "/second.png"))
            qFatal("cannot save second view");
        app.quit();
    });
    return app.exec();
}
