/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTimer>
#include "widget.hpp"

// Observe real paints without requesting any; actions use the public player API.
class RepaintProbe
{
    Widget& widget;
    QString directory;
    QFile log;
    QElapsedTimer clock;
    bool armed = false;
    QString phase = "playing-hidden";
    int serial = 0;
    QImage video;
    int views = 0;
    QSize original;
    void write(QJsonObject object)
    {
        object["ms"] = double(clock.elapsed());
        object["phase"] = phase;
        log.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
        log.flush();
    }
    void at(int delay, const QString& name, std::function<void()> action = {})
    {
        QTimer::singleShot(delay, Qt::PreciseTimer, &widget, [this, name, action]() {
            phase = name;
            write({{"event", "phase"}});
            if (action) action();
        });
    }
    void start()
    {
        original = widget.size();
        Bino::instance()->pause();
        phase = "settle";
        write({{"event", "phase"}});
        at(500, "idle");
        at(1500, "show", [] { Bino::instance()->overlayUIPointerMove({0.5, 0.2}, true); });
        at(1900, "lock", [] { Bino::instance()->overlayUIPointerPress({0.5, 0.2}, true); });
        at(2100, "animate", [] { Bino::instance()->play(); });
        at(2800, "pause", [] { Bino::instance()->pause(); });
        at(3200, "mono", [] { Bino::instance()->setInputMode(Input_Mono); });
        at(3600, "stereo", [] { Bino::instance()->setInputMode(Input_Left_Right); });
        at(4000, "resize", [this] { widget.resize(original.width() * 3 / 4, original.height() * 3 / 4); });
        at(4400, "restore", [this] { widget.resize(original); });
        at(4800, "locked-expired");
        at(5000, "release", [] { Bino::instance()->overlayUIPointerRelease({0.5, 0.2}); });
        at(5400, "hidden");
        at(5500, "show-again", [] { Bino::instance()->overlayUIPointerMove({0.1, 0.2}, true); });
        at(8800, "expired");
        at(9000, "idle-again");
        at(10000, "hide", [this] { widget.hide(); });
        at(10200, "expose", [this] { widget.show(); });
        at(11000, "end", [] { QCoreApplication::quit(); });
    }
public:
    RepaintProbe(Widget& widget, const QString& directory) : widget(widget), directory(directory),
        log(directory + "/repaint.jsonl")
    {
        if (!log.open(QIODevice::WriteOnly)) qFatal("cannot open repaint trace");
        clock.start();
    }
    void native(const QImage& image, int count) { video = image; views = count; }
    void paint(const QImage& target, qint64 pts, int buffer)
    {
        if (pts < 0) return;
        const QString stem = QString("repaint-%1-%2").arg(serial++, 5, 10, QChar('0')).arg(buffer);
        if (!target.save(directory + "/" + stem + "-target.png")) qFatal("cannot save repaint target");
        if (widget.usesNativeVideo() && !video.save(directory + "/" + stem + "-video.png")) qFatal("cannot save repaint video");
        write({{"event", "paint"}, {"stem", stem}, {"pts", double(pts)}, {"buffer", buffer},
            {"native", widget.usesNativeVideo()}, {"views", views}, {"stereo", widget.isOpenGLStereo()},
            {"width", widget.width()}, {"height", widget.height()}, {"playing", Bino::instance()->playing()}});
        if (!armed && pts >= 1500000) {
            armed = true;
            QTimer::singleShot(0, &widget, [this] { start(); });
        }
    }
};
