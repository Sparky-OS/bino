// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QSurfaceFormat>
#include <QWindow>
#include "tools.hpp"
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include "bino.hpp"
#include "gui.hpp"
#include "playlist.hpp"
#include "commandinterpreter.hpp"

class Observer : public QObject {
public:
    QString directory;
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
                || event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            QFile log(directory + "/events.jsonl"); log.open(QIODevice::WriteOnly | QIODevice::Append);
            log.write(QJsonDocument(QJsonObject{{"object",object->metaObject()->className()},
                {"type",int(event->type())},{"x",mouse->position().x()},{"y",mouse->position().y()}}).toJson(QJsonDocument::Compact)+'\n');
        }
        return false;
    }
};
int main(int argc, char **argv)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStereo(false);
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    initializeOpenGLType(format);
    if (argc != 3) return 2;
    QCoreApplication::setOrganizationName("Bino");
    QCoreApplication::setApplicationName("Bino-fullscreen-test");
    Playlist playlist; CommandInterpreter commands;
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice()); bino.startPlaylistMode();
    Gui gui(qEnvironmentVariableIsSet("FULLSCREEN_FALLBACK") ? Output_Left : Output_OpenGL_Stereo,90.0f,false);
    auto *widget=gui.findChild<Widget*>(); if (!widget) return 3;
    Observer observer; observer.directory=argv[2]; app.installEventFilter(&observer);
    gui.show();
    bino.mediaChanged(PlaylistEntry(QUrl::fromLocalFile(argv[1]),Input_Unknown,Surround_Off));
    QTimer timer; timer.setInterval(30);
    QString previous;
    QObject::connect(&timer,&QTimer::timeout,[&] {
        QFile request(QString(argv[2])+"/request");
        if (!request.open(QIODevice::ReadOnly)) return;
        QString name=QString::fromUtf8(request.readAll()).trimmed();
        if (name==previous || name.isEmpty()) return;
        previous=name;
        if (name=="quit") { app.quit(); return; }
        const QRect r(widget->mapToGlobal(QPoint()),widget->size());
        const auto margins=gui.windowHandle()->frameMargins();
        QJsonObject state{{"fullscreen",gui.isFullScreen()},{"maximized",gui.isMaximized()},
            {"state",int(gui.windowState())},{"widget",QJsonArray{r.x(),r.y(),r.width(),r.height()}},
            {"margins",QJsonArray{margins.left(),margins.top(),margins.right(),margins.bottom()}},
            {"playing",bino.playing()},{"native",widget->usesNativeVideo()}};
        QFile result(QString(argv[2])+"/"+name+".json");result.open(QIODevice::WriteOnly);
        result.write(QJsonDocument(state).toJson());
        gui.grab().save(QString(argv[2])+"/"+name+"-ui.png");
    });
    timer.start();
    QTimer::singleShot(90000,&app,&QApplication::quit);
    return app.exec();
}
