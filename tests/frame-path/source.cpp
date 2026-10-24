#include <QApplication>
#include <QFile>
#include <QCryptographicHash>
#include <QSettings>
#include "bino.hpp"
#include "playlist.hpp"
#include "commandinterpreter.hpp"
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Bino");
    QCoreApplication::setApplicationName("Bino-source-test");
    if (argc != 3) return 2;
    const QString root = argv[1];
    const bool write = QByteArray(argv[2]) == "write";
    Playlist playlist;
    CommandInterpreter commands;
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice());
    bino.startPlaylistMode();
    int failed = 0;
    auto check = [&](bool good, const char *name) {
        qInfo("%s %s", good ? "PASS" : "FAIL", name); failed += !good;
    };
    auto open = [&](const char *name, InputMode mode = Input_Unknown) {
        bino.mediaChanged(PlaylistEntry(QUrl::fromLocalFile(root + '/' + name), mode, Surround_Off));
        bino.pause();
    };
    open("case-10.mkv", Input_Mono);
    check(bino.inputMode() == Input_Left_Right, "declared packing wins over explicit input override");
    bino.setInputMode(Input_Top_Bottom);
    check(bino.inputMode() == Input_Left_Right, "declared packing cannot be changed through the input action");
    if (write) bino.setSwapEyes(true);
    else check(bino.swapEyes(), "marked file swap survives process restart");
    open("case-11.mkv");
    check(!bino.swapEyes(), "swap does not leak to another file");
    if (write) bino.setInputMode(Input_Top_Bottom);
    else check(bino.inputMode() == Input_Top_Bottom, "unmarked file correction survives process restart");
    open("case-01.mkv");
    check(bino.inputMode() == Input_Right_Left, "inverted StereoMode is right-first input");
    open("case-10.mkv");
    check(bino.swapEyes(), "returning to the marked file restores its swap");
    return failed ? 1 : 0;
}
