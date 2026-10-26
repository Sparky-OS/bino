/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025
 * Martin Lambers <marlam@marlam.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QUrl>

#include "videosink.hpp"
#include "log.hpp"
#include "frametrace.hpp"


VideoSink::VideoSink(VideoFrame* frame, VideoFrame* extFrame, bool* frameIsNew) :
    frameCounter(0),
    frame(frame),
    extFrame(extFrame),
    frameIsNew(frameIsNew),
    needExtFrame(false),
    inputMode(Input_Unknown),
    surroundMode(Surround_Unknown),
    lastFrameWasValid(false)
{
#if BINO_STEREO_DESKTOP
    // Unmarked input can acquire a packing from a hint or a per-file correction.
    // Keep its samples until that decision is made, without changing its marks.
    setPreserveNativeSize(true);
#endif
    connect(this, SIGNAL(videoFrameChanged(const QVideoFrame&)), this, SLOT(processNewFrame(const QVideoFrame&)));
}

// called whenever new media is played:
void VideoSink::newPlaylistEntry(const PlaylistEntry& entry, const MetaData& metaData)
{
    frameCounter = 0;
    needExtFrame = false;
    pendingFrame = QVideoFrame();
    lastFrameWasValid = false;

    int vt = entry.videoTrack;
    if (vt < 0)
        vt = 0;

    inputDeclared = false;
#if BINO_STEREO_DESKTOP
    inputDeclared = vt < metaData.declaredInputs.size() && metaData.declaredInputs[vt];
#endif
    inputMode = inputDeclared ? metaData.inputModes.value(vt, Input_Unknown) : entry.inputMode;
    if (inputMode == Input_Unknown) {
        if (vt < metaData.inputModes.size())
            inputMode = metaData.inputModes[vt];
    }
    LOG_DEBUG("input mode for %s: %s", qPrintable(entry.url.toString()), inputModeToString(inputMode));
    surroundMode = entry.surroundMode;
    if (surroundMode == Surround_Unknown) {
        if (vt < metaData.surroundModes.size())
            surroundMode = metaData.surroundModes[vt];
    }
    LOG_DEBUG("surround mode for %s: %s", qPrintable(entry.url.toString()), surroundModeToString(surroundMode));
}

void VideoSink::processNewFrame(const QVideoFrame& frame)
{
    FrameTrace::pts = frame.startTime();
    FrameTrace::Scope trace("sink", frame.startTime(), frame.isValid(), *frameIsNew);
    if (!frame.isValid() && lastFrameWasValid) {
        // keep showing the last frame of the current media; ignore that QtMultiMedia
        // with the FFmpeg backend gives us an invalid frame as soon as the media stops
        // (it does not do this with the GStreamer backend)
        LOG_DEBUG("video sink gets invalid frame and ignores it since last frame of current media was valid");
        return;
    }
    if (frame.isValid()) {
        LOG_FIREHOSE("video sink gets a valid frame");
        lastFrameWasValid = true;
    }

#if BINO_STEREO_DESKTOP
    if (frame.isValid()) {
        const auto format = frame.surfaceFormat();
        using Stereo = QVideoFrameFormat::StereoMode;
        int layout = 0;
        int anaglyph = 0;
        switch (format.stereoMode()) {
        case Stereo::SideBySide: layout = 1; break;
        case Stereo::TopBottom: layout = 2; break;
        case Stereo::RowInterleaved: layout = 4; break;
        case Stereo::ColumnInterleaved: layout = 5; break;
        case Stereo::Checkerboard: layout = 6; break;
        case Stereo::SideBySideQuincunx: layout = 7; break;
        case Stereo::FrameSequence: layout = 8; break;
        case Stereo::AnaglyphCyanRed: anaglyph = 1; break;
        case Stereo::AnaglyphGreenMagenta: anaglyph = 2; break;
        default: break;
        }
        if (layout || anaglyph) {
            const bool inverted = format.stereoFlags().testFlag(QVideoFrameFormat::StereoFlag::RightEyeFirst);
            const InputMode declared = inputModeFromPacking(layout, inverted, anaglyph);
            if (declared != inputMode)
                needExtFrame = false;
            inputMode = declared;
            inputDeclared = true;
        }
    }
#endif

#if BINO_STEREO_DESKTOP
    if (inputMode == Input_Alternating_LR || inputMode == Input_Alternating_RL) {
        if (!needExtFrame) {
            pendingFrame = frame;
            needExtFrame = true;
            ++frameCounter;
            return;
        }
        // Publish both temporal views together, including on unrelated UI paints.
        this->frame->update(inputMode, surroundMode, pendingFrame, frameCounter == 1, true);
        this->extFrame->update(inputMode, surroundMode, frame, frameCounter == 1, true);
        pendingFrame = QVideoFrame();
        needExtFrame = false;
        *frameIsNew = true;
        ++frameCounter;
        emit newVideoFrame();
        return;
    }
    pendingFrame = QVideoFrame();
#endif

    bool updateExtFrame;
    if (inputMode == Input_Alternating_LR || inputMode == Input_Alternating_RL) {
        if (needExtFrame) {
            LOG_FIREHOSE("video sink updates extended frame for alternating mode");
            updateExtFrame = true;
            needExtFrame = false;
        } else {
            LOG_FIREHOSE("video sink updates standard frame for alternating mode");
            updateExtFrame = false;
            needExtFrame = true;
        }
    } else {
        LOG_FIREHOSE("video sink updates standard frame for non-alternating mode");
        updateExtFrame = false;
        needExtFrame = false;
    }
    if (updateExtFrame) {
        this->extFrame->update(inputMode, surroundMode, frame, frameCounter == 0, BINO_STEREO_DESKTOP);
    } else {
        this->frame->update(inputMode, surroundMode, frame, frameCounter == 0, BINO_STEREO_DESKTOP);
        this->extFrame->invalidate();
    }
    if (!needExtFrame) {
        LOG_FIREHOSE("video sink signals that new frame is complete");
        *frameIsNew = true;
        emit newVideoFrame();
    }
    frameCounter++;
}
