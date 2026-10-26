// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include "videoframe.hpp"
#include "modes.hpp"
#include <cstdio>
int main(int argc, char **argv)
{
    QGuiApplication app(argc,argv);
    int failures=0;
    for (int layout : {-1,3,255}) {
        VideoFrame frame;
        QVideoFrameFormat fmt(QSize(640,180),QVideoFrameFormat::Format_RGBA8888);
        frame.update(inputModeFromPacking(layout,0),Surround_Off,QVideoFrame(fmt),true,true);
        const bool good=frame.inputMode==Input_Mono;
        std::printf("%s declared layout %d cannot infer a pair from wide shape (mode=%d)\n",good ? "PASS":"FAIL",layout,int(frame.inputMode));
        failures+=!good;
    }
    VideoFrame ordinary;
    QVideoFrameFormat fmt(QSize(640,180),QVideoFrameFormat::Format_RGBA8888);
    ordinary.update(Input_Unknown,Surround_Off,QVideoFrame(fmt),true,true);
    const bool retained=ordinary.inputMode==Input_Left_Right;
    std::printf("%s unmarked wide-shape inference retained\n",retained ? "PASS":"FAIL");
    return failures || !retained;
}
