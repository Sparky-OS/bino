// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <QVariant>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

// The sink alone, before Bino can map, import, unpack or draw the frame.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 7) return 2;
    const bool native = QByteArray(argv[6]) == "native";
    const QSize expectedSize(atoi(argv[2]), atoi(argv[3]));
    const QSize expectedSar(atoi(argv[4]), atoi(argv[5]));
    QMediaPlayer player;
    QVideoSink sink;
    // The same binary exercises the old library, where the property is absent.
    sink.setProperty("preserveNativeSize", native);
    player.setVideoSink(&sink);
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame &frame) {
        if (!frame.isValid()) return;
        const auto format = frame.surfaceFormat();
        const QSize size = frame.size(), sar = format.pixelAspectRatio();
        const bool good = size == expectedSize && sar == expectedSar;
        const auto image = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        for (int y = 0; y < image.height(); ++y)
            hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constScanLine(y)), image.width() * 4));
        QJsonObject result{{"width", size.width()}, {"height", size.height()},
            {"sar_num", sar.width()}, {"sar_den", sar.height()},
            {"pixel_sha256", QString::fromLatin1(hash.result().toHex())}, {"native_requested", native}, {"stereo_mode", int(format.stereoMode())}, {"pass", good}};
        puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        app.exit(good ? 0 : 1);
    });
    QTimer::singleShot(15000, &app, [&] { app.exit(3); });
    player.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    player.play();
    return app.exec();
}
