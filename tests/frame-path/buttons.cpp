#include <QApplication>
#include <QDir>
#include <QIcon>
#include "bino.hpp"
#include "playlist.hpp"
#include "commandinterpreter.hpp"
#include "overlay-ui.hpp"
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    QIcon::setThemeSearchPaths({qEnvironmentVariable("BINO_TEST_ICONS", "/work/test-icons")});
    QIcon::setThemeName("breeze-dark");
    if (qEnvironmentVariableIsSet("BINO_TEST_ICONS") && QIcon::fromTheme("media-playback-start").isNull())
        return 4;
    Playlist playlist;
    CommandInterpreter commands;
    Bino bino(Bino::ScreenGeometry, Screen(), false);
    bino.initializeOutput(QAudioDevice());
    bino.startPlaylistMode();
    playlist.append(PlaylistEntry(QUrl("file:///unused-first")));
    playlist.append(PlaylistEntry(QUrl("file:///unused-second")));
    QDir().mkpath(argv[1]);
    for (int stereo = 0; stereo < 2; ++stereo) {
        for (int hover = -1; hover < 9; ++hover) {
            OverlayUI overlay;
            overlay.updateParameters(false, stereo, 5000, 20000, true, false,
                    hover < 0 ? QPointF(-1,-1) : QPointF((40.0 + hover * 80) / 720, 320.0 / 400), false);
            overlay.redraw(720, 400);
            if (!overlay.image().save(QString("%1/%2-%3.png").arg(argv[1]).arg(stereo).arg(hover)))
                return 3;
        }
    }
}
