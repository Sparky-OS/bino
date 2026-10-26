/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025, 2026
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

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QTimer>

#include "modes.hpp"
#include "bino.hpp"
#if BINO_STEREO_DESKTOP
#include "nativevideolayer.hpp"
#endif


class Widget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
Q_OBJECT

private:
#if BINO_STEREO_DESKTOP
    std::unique_ptr<NativeVideoLayer> _nativeLayer;
#else
    int _displayPrgOutputMode;
#endif
    QTimer _updateTimer;
    QSize _sizeHint;
    int _width, _height;
    float _lastFrameRelWidth, _lastFrameRelHeight;

    OutputMode _outputMode;
#if !BINO_STEREO_DESKTOP
    int _alternatingLastView; // last view displayed in Mode_Alternating (0 or 1)
#endif

    bool _inOverlayUIEvent;
    float _surroundVerticalFOVDefault;
    float _surroundVerticalFOV;
    bool _inSurroundMovement;
    QPointF _surroundMovementStart;
    float _surroundHorizontalAngleBase;
    float _surroundVerticalAngleBase;
    float _surroundHorizontalAngleCurrent;
    float _surroundVerticalAngleCurrent;
    QMatrix4x4 _surroundProjectionMatrix;

    unsigned int _viewTex[2];
    int _viewTexWidth[2], _viewTexHeight[2];
    unsigned int _quadVao;
    QOpenGLShaderProgram _displayPrg;

#if BINO_STEREO_DESKTOP
    void rebuildDisplayPrgIfNecessary();
#else
    void rebuildDisplayPrgIfNecessary(OutputMode outputMode);
#endif

    void scheduleOverlayUpdate();
    QPointF toView(const QPointF& pos) const;

protected:
    virtual void nativeFrameRendered(const QSize&, int) {}

public:
    ~Widget() override;
#if BINO_STEREO_DESKTOP
    bool usesNativeVideo() const { return bool(_nativeLayer); }
#else
    bool usesNativeVideo() const { return false; }
    void setOutputMode(OutputMode mode);
#endif

    Widget(OutputMode outputMode, float surroundVerticalFOV, QWidget* parent = nullptr);

    bool isOpenGLStereo() const;
    OutputMode outputMode() const;
    void setSurroundVerticalFieldOfView(float vfov);
    void resetSurroundView();

    virtual QSize sizeHint() const override;
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;

    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void mousePressEvent(QMouseEvent* e) override;
    virtual void mouseReleaseEvent(QMouseEvent* e) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* e) override;
    virtual void mouseMoveEvent(QMouseEvent* e) override;
    virtual void wheelEvent(QWheelEvent* e) override;

public slots:
    void mediaChanged(PlaylistEntry entry);

signals:
    void toggleFullscreen();
};
