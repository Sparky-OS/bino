#include <QGuiApplication>
#include <QMediaPlayer>
#include <QTimer>
#include <QVideoSink>
#include <QUrl>
#include "videoframe.hpp"
#include "frametrace.hpp"

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 3)
        return 2;
    const bool map = QString::fromLocal8Bit(argv[2]) == "map";
    QVideoSink sink;
    QMediaPlayer player;
    VideoFrame mapped;
    int frames = 0;
    player.setVideoSink(&sink);
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame& frame) {
        if (!frame.isValid())
            return;
        FrameTrace::Scope trace("sink", frame.startTime(), frame.handleType(), frame.pixelFormat());
        if (map)
            mapped.update(Input_Left_Right_Half, Surround_Off, frame, frames == 0);
        ++frames;
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &app,
            [&](QMediaPlayer::Error, const QString& message) {
        qCritical("%s", qPrintable(message));
        app.exit(3);
    });
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &app,
            [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            app.exit(frames ? 0 : 4);
    });
    QTimer::singleShot(30000, &app, [&]() { app.exit(5); });
    player.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    player.play();
    int result = app.exec();
    qInfo("frames=%d map=%d result=%d", frames, map, result);
    return result;
}
