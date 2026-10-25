// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

// The sink alone, before Bino can map, import, unpack or draw the frame.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 6) return 2;
    const QSize expectedSize(atoi(argv[2]), atoi(argv[3]));
    const QSize expectedSar(atoi(argv[4]), atoi(argv[5]));
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame &frame) {
        if (!frame.isValid()) return;
        const auto format = frame.surfaceFormat();
        const QSize size = frame.size(), sar = format.pixelAspectRatio();
        const bool good = size == expectedSize && sar == expectedSar;
        QJsonObject result{{"width", size.width()}, {"height", size.height()},
            {"sar_num", sar.width()}, {"sar_den", sar.height()},
            {"stereo_mode", int(format.stereoMode())}, {"pass", good}};
        puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        app.exit(good ? 0 : 1);
    });
    QTimer::singleShot(15000, &app, [&] { app.exit(3); });
    player.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    player.play();
    return app.exec();
}
