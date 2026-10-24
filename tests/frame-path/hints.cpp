#include <QApplication>
#include "metadata.hpp"
#include "videosink.hpp"
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    int failed = 0;
    auto check = [&](bool good, const char *name) {
        qInfo("%s %s", good ? "PASS" : "FAIL", name); failed += !good;
    };
    struct Case { const char *name; InputMode input; bool declared; };
    const Case cases[] = {
        {"unmarked-SBS", Input_Left_Right_Half, false},
        {"unmarked-FSBS", Input_Left_Right, false},
        {"unmarked-HSBS", Input_Left_Right_Half, false},
        {"unmarked-TAB", Input_Top_Bottom_Half, false},
        {"unmarked-FTAB", Input_Top_Bottom, false},
        {"unmarked-lrq", Input_Left_Right_Half, false},
        {"unmarked-SBS-SAR", Input_Left_Right, false},
        {"declared-TAB", Input_Left_Right, true},
        {"declared-HSBS", Input_Bottom_Top, true},
        {"declared-mono-SBS", Input_Mono, true},
        {"plain", Input_Unknown, false}
    };
    for (const auto &c : cases) {
        MetaData data; QString error;
        const auto url = QUrl::fromLocalFile(QString(argv[1]) + '/' + c.name + ".mkv");
        if (!data.detectCached(url, &error)) { qWarning("%s", qPrintable(error)); return 2; }
        check(data.inputModes.value(0) == c.input && data.declaredInputs.value(0) == c.declared, c.name);
        VideoFrame primary, secondary; bool fresh = false;
        VideoSink sink(&primary, &secondary, &fresh);
        sink.newPlaylistEntry(PlaylistEntry(url, Input_Mono, Surround_Off), data);
        check(sink.inputMode == (c.declared ? c.input : Input_Mono), "correction wins only over guesses");
    }
    MetaData surround;
    surround.detectCached(QUrl::fromLocalFile(QString(argv[1]) + "/plain-360.mkv"));
    check(surround.surroundModes.value(0) == Surround_360, "surround filename hint retained");
    VideoFrame frame;
    QVideoFrameFormat wide(QSize(640,180), QVideoFrameFormat::Format_RGBA8888);
    frame.update(Input_Unknown, Surround_Off, QVideoFrame(wide), true, true);
    check(frame.inputMode == Input_Left_Right, "unmarked wide frame shape guesses SBS");
    frame.update(Input_Mono, Surround_Off, QVideoFrame(wide), true, true);
    check(frame.inputMode == Input_Mono, "explicit mono defeats wide frame shape");
    QVideoFrameFormat plain(QSize(320,180), QVideoFrameFormat::Format_RGBA8888);
    frame.update(Input_Unknown, Surround_Off, QVideoFrame(plain), true, true);
    check(frame.inputMode == Input_Mono, "plain frame stays mono");
    return failed ? 1 : 0;
}
