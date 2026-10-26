// SPDX-License-Identifier: GPL-3.0-or-later
// Installed Qt sink, independently decoded eye planes, and source/seek/track life cycle.
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QFile>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <cmath>

int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    if(argc!=3) return 2;
    const QString root=argv[1]; const bool native=QByteArray(argv[2])=="native";
    QByteArray reference[2];
    for(int eye=0;eye<2;eye++) { QFile f(root+QString("/view%1.yuv").arg(eye));if(!f.open(QIODevice::ReadOnly)) return 2; reference[eye]=f.readAll();if(reference[eye].size()!=96*115200) return 2; }
    QMediaPlayer player;QVideoSink sink;sink.setPreserveNativeSize(native);player.setVideoSink(&sink);
    int stage=0,count=0,checks=0; qint64 last=-1;bool changing=false;
    auto fail=[&](const char *why){std::fprintf(stderr,"FAIL stage=%d %s\n",stage,why);app.exit(1);};
    auto source=[&](const QString &path){changing=true;QTimer::singleShot(0,&app,[&,path]{player.stop();player.setSource(QUrl::fromLocalFile(path));last=-1;count=0;changing=false;player.play();});};
    QObject::connect(&sink,&QVideoSink::videoFrameChanged,&app,[&](const QVideoFrame &input){
        if(!input.isValid()||changing)return;
        const auto fmt=input.surfaceFormat();
        if(stage<=1||stage==5){
            if(input.size()!=QSize(640,240)||fmt.pixelAspectRatio()!=QSize(1,1)||fmt.stereoMode()!=QVideoFrameFormat::StereoMode::SideBySide
               ||!fmt.stereoFlags().testFlag(QVideoFrameFormat::StereoFlag::Multiview)){fail("native pair identity/raster/aspect");return;}
            QVideoFrame frame=input;
            if(fmt.pixelFormat()!=QVideoFrameFormat::Format_YUV420P||!frame.map(QVideoFrame::ReadOnly)){fail("reference plane mapping");return;}
            const int n=int(std::llround(input.startTime()*24.0/1000000));
            if(n<0||n>=96){fail("source timestamp range");return;}
            int offset=n*115200;
            for(int plane=0;plane<3;plane++){
                const int w=plane?160:320,h=plane?120:240;
                for(int y=0;y<h;y++)for(int eye=0;eye<2;eye++){
                    if(std::memcmp(frame.bits(plane)+y*frame.bytesPerLine(plane)+eye*w,reference[eye].constData()+offset+y*w,w)){
                        fail("per-eye source sample mismatch");return;
                    }
                    checks+=w;
                }
                offset+=w*h;
            }
            frame.unmap();
            if(last>=0&&input.startTime()<last){fail("stale/reordered pair after reset");return;}
            last=input.startTime();
        } else {
            const QSize expected=stage==2 ? QSize(native?160:320,90) : stage==3 ? QSize(80,40) : QSize(96,64);
            if(input.size()!=expected||fmt.stereoMode()!=QVideoFrameFormat::StereoMode::None
               ||fmt.stereoFlags().testFlag(QVideoFrameFormat::StereoFlag::Multiview)){fail("mono/track transition leaked stereo state");return;}
        }
        if(++count<6)return;
        count=0;
        if(stage==0){stage=1;changing=true;QTimer::singleShot(0,&app,[&]{player.setPosition(0);last=-1;changing=false;});}
        else if(stage==1){stage=2;source(root+"/mono.mkv");}
        else if(stage==2){stage=3;source(root+"/two-tracks.mkv");}
        else if(stage==3){stage=4;changing=true;QTimer::singleShot(0,&app,[&]{player.setActiveVideoTrack(1);last=-1;changing=false;});}
        else if(stage==4){stage=5;source(root+"/native.mp4");}
        // Stage 5 runs to EOF, proving no stray half or duplicate after draining.
    });
    QObject::connect(&player,&QMediaPlayer::mediaStatusChanged,&app,[&](QMediaPlayer::MediaStatus status){
        if(status==QMediaPlayer::EndOfMedia&&stage==5){std::printf("PASS native=%d samples=%d seek/source/track/cancel/end, mono and default consumer controls\n",native,checks);app.exit(0);}
    });
    QObject::connect(&player,&QMediaPlayer::errorOccurred,&app,[&]{fail("media error");});
    QTimer::singleShot(30000,&app,[&]{fail("timeout");});
    source(root+"/native.mp4");return app.exec();
}
