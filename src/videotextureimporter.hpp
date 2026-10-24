/* SPDX-License-Identifier: GPL-3.0-or-later */
// Qt Multimedia texture import for the edition's OpenGL renderer.
#pragma once

#include <QObject>
#include <QVideoFrame>
#include <memory>

class VideoTextureImporter : public QObject
{
    struct Private;
    std::unique_ptr<Private> d;
public:
    struct Planes {
        unsigned int textures[3] = {};
        unsigned int samplers[3] = {};
        int count = 0;
        int format = 0; // shader-color.frag.glsl
        int channelOrder = 0; // Qt's rgba, argb and abgr texture shaders
    };
    VideoTextureImporter();
    ~VideoTextureImporter() override;
    bool tryImport(const QVideoFrame& frame, Planes* planes);
    void submitted();
    void release();
};
