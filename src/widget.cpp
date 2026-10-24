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

#include <QGuiApplication>
#include <QMessageBox>
#include <QQuaternion>
#include <QtMath>

#include "widget.hpp"
#include "playlist.hpp"
#include "tools.hpp"
#include "log.hpp"
#include "frametrace.hpp"
#include "gputrace.hpp"

/* These might not be defined on Mac OS. Use crude replacements. */
#ifndef GL_MAX_FRAMEBUFFER_WIDTH
# define GL_MAX_FRAMEBUFFER_WIDTH GL_MAX_TEXTURE_SIZE
#endif
#ifndef GL_MAX_FRAMEBUFFER_HEIGHT
# define GL_MAX_FRAMEBUFFER_HEIGHT GL_MAX_TEXTURE_SIZE
#endif


static const QSize SizeBase(16, 9);

Widget::Widget(OutputMode outputMode, float surroundVerticalFOV, QWidget* parent) :
    QOpenGLWidget(parent),
    _sizeHint(0.5f * SizeBase),
    _lastFrameRelWidth(1.0f), _lastFrameRelHeight(1.0f),
    _outputMode(outputMode),
    _inOverlayUIEvent(false),
    _inSurroundMovement(false),
    _surroundHorizontalAngleBase(0.0f),
    _surroundVerticalAngleBase(0.0f),
    _surroundHorizontalAngleCurrent(0.0f),
    _surroundVerticalAngleCurrent(0.0f)
{
    if (outputMode == Output_OpenGL_Stereo && NativeVideoLayer::supported()) {
        _nativeLayer = std::make_unique<NativeVideoLayer>(this);
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setStereo(false);
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setAlphaBufferSize(8);
        setFormat(format);
        setProperty("binoNativeVideo", true);
        NativeVideoLayer::prepareWindow(window());
    }
    setSurroundVerticalFieldOfView(surroundVerticalFOV);
    _surroundVerticalFOVDefault = _surroundVerticalFOV; // to make sure clamping was applied
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    setMouseTracking(true);
    setMinimumSize(8, 8);
    QSize screenSize = QGuiApplication::primaryScreen()->availableSize();
    QSize maxSize = 0.75f * screenSize;
    _sizeHint = SizeBase.scaled(maxSize, Qt::KeepAspectRatio);
    connect(Bino::instance(), &Bino::newVideoFrame, [=]() { update(); });
    connect(Bino::instance(), &Bino::toggleFullscreen, [=]() { emit toggleFullscreen(); });
    connect(Playlist::instance(), SIGNAL(mediaChanged(PlaylistEntry)), this, SLOT(mediaChanged(PlaylistEntry)));
    _updateTimer.setSingleShot(true);
    _updateTimer.setTimerType(Qt::CoarseTimer);
    connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(update()));
    connect(this, &QOpenGLWidget::frameSwapped, this, []() {
        FrameTrace::event("swap", FrameTrace::timestamp(), 0, FrameTrace::pts);
    });
    setFocus();
}

Widget::~Widget()
{
    _nativeLayer.reset();
}

bool Widget::isOpenGLStereo() const
{
    return bool(_nativeLayer) || (context() ? context()->format().stereo() : false);
}

OutputMode Widget::outputMode() const
{
    return _outputMode;
}

void Widget::setSurroundVerticalFieldOfView(float vfov)
{
    _surroundVerticalFOV = qBound(5.0f, vfov, 115.0f);
}

void Widget::resetSurroundView()
{
    _surroundVerticalFOV = _surroundVerticalFOVDefault;
    _surroundHorizontalAngleBase = 0.0f;
    _surroundVerticalAngleBase = 0.0f;
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}

QSize Widget::sizeHint() const
{
    return _sizeHint;
}

void Widget::initializeGL()
{
#ifndef Q_OS_WASM
    bool contextIsOk = (context()->isValid() && context()->format().majorVersion() >= 3);
    if (!contextIsOk) {
        LOG_FATAL("%s", qPrintable(tr("Insufficient OpenGL capabilities.")));
        QMessageBox::critical(this, tr("Error"), tr("Insufficient OpenGL capabilities."));
        std::exit(1);
    }
#endif
    if (outputMode() == Output_OpenGL_Stereo && !isOpenGLStereo())
        _outputMode = Output_Left;

    bool haveAnisotropicFiltering = checkTextureAnisotropicFilterAvailability();
    initializeOpenGLFunctions();
    bool isCoreProfile = (QOpenGLContext::currentContext()->format().profile() == QSurfaceFormat::CoreProfile);

    QString variantString = (OpenGLType == OpenGL_Type_WebGL ? "WebGL" : OpenGLType == OpenGL_Type_OpenGLES ? "ES" : "Desktop");
    if (OpenGLType == OpenGL_Type_Desktop)
        variantString += isCoreProfile ? " core profile" : " compatibility profile";
    GLint maxTexSize, maxFBWidth, maxFBHeight;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &maxFBWidth);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &maxFBHeight);
    LOG_INFO("OpenGL Variant:      %s", qPrintable(variantString));
    LOG_INFO("OpenGL Version:      %s", getOpenGLString(this, GL_VERSION));
    LOG_INFO("OpenGL GLSL Version: %s", getOpenGLString(this, GL_SHADING_LANGUAGE_VERSION));
    LOG_INFO("OpenGL Vendor:       %s", getOpenGLString(this, GL_VENDOR));
    LOG_INFO("OpenGL Renderer:     %s", getOpenGLString(this, GL_RENDERER));
    LOG_INFO("OpenGL AnisoTexFilt: %s", haveAnisotropicFiltering ? "yes" : "no");
    LOG_INFO("OpenGL Max Tex Size: %d", maxTexSize);
    LOG_INFO("OpenGL Max FB Size:  %dx%d", maxFBWidth, maxFBHeight);
    LOG_INFO("OpenGL Stereo:       %s", isOpenGLStereo() ? "yes" : "no");

    // View textures
    glGenTextures(2, _viewTex);
    CHECK_GL();
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, _viewTex[i]);
        unsigned char nullBytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        switch (OpenGLType) {
        case OpenGL_Type_WebGL:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullBytes);
            break;
        case OpenGL_Type_OpenGLES:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullBytes);
            break;
        case OpenGL_Type_Desktop:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, nullBytes);
            break;
        }
        CHECK_GL();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        if (haveAnisotropicFiltering)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
        CHECK_GL();
        _viewTexWidth[i] = 1;
        _viewTexHeight[i] = 1;
    }

    // Quad geometry
    const float quadPositions[] = {
        -1.0f, +1.0f, 0.0f,
        +1.0f, +1.0f, 0.0f,
        +1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };
    const float quadTexCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    static const unsigned short quadIndices[] = {
        0, 3, 1, 1, 3, 2
    };
    glGenVertexArrays(1, &_quadVao);
    glBindVertexArray(_quadVao);
    GLuint quadPositionBuf;
    glGenBuffers(1, &quadPositionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, quadPositionBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadPositions), quadPositions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint quadTexCoordBuf;
    glGenBuffers(1, &quadTexCoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, quadTexCoordBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadTexCoords), quadTexCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint quadIndexBuf;
    glGenBuffers(1, &quadIndexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
    CHECK_GL();

    // Initialize Bino
    Bino::instance()->initProcess();
}

void Widget::rebuildDisplayPrgIfNecessary()
{
    if (_displayPrg.isLinked())
        return;
    QString vertexShaderSource = readFile(":src/shader-display.vert.glsl");
    QString fragmentShaderSource = readFile(":src/shader-display.frag.glsl");
    if (OpenGLType != OpenGL_Type_Desktop) {
        vertexShaderSource.prepend("#version 300 es\n");
        fragmentShaderSource.prepend("#version 300 es\n"
                "precision mediump float;\n");
    } else {
        vertexShaderSource.prepend("#version 330\n");
        fragmentShaderSource.prepend("#version 330\n");
    }
    _displayPrg.removeAllShaders();
    _displayPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    _displayPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    _displayPrg.link();
}

void Widget::paintGL()
{
    FrameTrace::Scope trace("paint", FrameTrace::pts, currentTargetBuffer(), isOpenGLStereo());
    // Support for HighDPI output
    int width = _width * devicePixelRatioF();
    int height = _height * devicePixelRatioF();

    // Find out about the views we have
    int viewCount, viewWidth, viewHeight;
    float frameDisplayAspectRatio;
    bool surround;
    Bino::instance()->updateMainProcess();
    Bino::instance()->preRenderProcess(width, height, &viewCount, &viewWidth, &viewHeight, &frameDisplayAspectRatio, &surround, bool(_nativeLayer));

    FrameTrace::event("geometry", FrameTrace::timestamp(), 0, FrameTrace::pts, viewWidth, viewHeight);

    const bool frameIsStereo = viewCount == 2;
    const int selectedView = frameIsStereo && _outputMode == Output_OpenGL_Stereo
            && currentTargetBuffer() == QOpenGLWidget::RightBuffer ? 1 : 0;

    // A native layer presents a packed pair in one callback. A toolkit stereo target
    // still needs only its current eye, as verified by the retained target regression.
    for (int v = 0; v < viewCount; v++) {
        if (!_nativeLayer && v != selectedView)
            continue;
        // prepare view texture
        glBindTexture(GL_TEXTURE_2D, _viewTex[v]);
        if (_viewTexWidth[v] != viewWidth || _viewTexHeight[v] != viewHeight) {
            switch (OpenGLType) {
            case OpenGL_Type_WebGL:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewWidth, viewHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                break;
            case OpenGL_Type_OpenGLES:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, viewWidth, viewHeight, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
                break;
            case OpenGL_Type_Desktop:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, viewWidth, viewHeight, 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
                break;
            }
            _viewTexWidth[v] = viewWidth;
            _viewTexHeight[v] = viewHeight;
        }
        // render view into view texture
        LOG_FIREHOSE("%s: getting view %d", Q_FUNC_INFO, v);
        QMatrix4x4 projectionMatrix;
        QMatrix4x4 orientationMatrix;
        QMatrix4x4 viewMatrix;
        if (Bino::instance()->assumeSurroundMode() != Surround_Off) {
            float verticalFieldOfView = qDegreesToRadians(_surroundVerticalFOV);
            float aspectRatio = 2.0f; // always 2:1 for surround video!
            float top = qTan(verticalFieldOfView * 0.5f);
            float bottom = -top;
            float right = top * aspectRatio;
            float left = -right;
            projectionMatrix.frustum(left, right, bottom, top, 1.0f, 100.0f);
            QQuaternion orientation = QQuaternion::fromEulerAngles(
                    (_surroundVerticalAngleBase + _surroundVerticalAngleCurrent),
                    (_surroundHorizontalAngleBase + _surroundHorizontalAngleCurrent), 0.0f);
            orientationMatrix.rotate(orientation.inverted());
            // Remember projection matrix for overlay UI in surround mode
            _surroundProjectionMatrix = projectionMatrix;
        }
        Bino::instance()->render(
                QVector3D(), QVector3D(), QVector3D(), QVector3D(), QVector3D(), QVector3D(),
                projectionMatrix, orientationMatrix, viewMatrix, v, viewWidth, viewHeight, _viewTex[v]);
        // generate mipmaps for the view texture
        glBindTexture(GL_TEXTURE_2D, _viewTex[v]);
        {
            FrameTrace::Scope mipTrace("view_mipmap", FrameTrace::pts, v);
            FrameTrace::GpuScope gpuTrace("gpu_view_mipmap", FrameTrace::pts, v);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    if (_nativeLayer) {
        const QSize eyeSize(viewWidth, viewHeight);
        double placedWidth = this->width();
        double placedHeight = placedWidth / frameDisplayAspectRatio;
        if (placedHeight > this->height()) {
            placedHeight = this->height();
            placedWidth = placedHeight * frameDisplayAspectRatio;
        }
        const QSize placedSize(qRound(placedWidth), qRound(placedHeight));
        const QRect placed(QPoint((this->width() - placedSize.width()) / 2,
                (this->height() - placedSize.height()) / 2), placedSize);
        _lastFrameRelWidth = placed.width() / float(this->width());
        _lastFrameRelHeight = placed.height() / float(this->height());
        rebuildDisplayPrgIfNecessary();
        if (_nativeLayer->begin(eyeSize, viewCount, placed)) {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glUseProgram(_displayPrg.programId());
            _displayPrg.setUniformValue("view0", 0);
            _displayPrg.setUniformValue("view1", 1);
            _displayPrg.setUniformValue("relativeWidth", 1.0f);
            _displayPrg.setUniformValue("relativeHeight", 1.0f);
            _displayPrg.setUniformValue("nativeSamples", true);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _viewTex[0]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _viewTex[1]);
            for (int v = 0; v < viewCount; v++) {
                glViewport(v * viewWidth, 0, viewWidth, viewHeight);
                _displayPrg.setUniformValue("nativeOffsetX", v * viewWidth);
                _displayPrg.setUniformValue("outputModeLeftRightView", v);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            }
            nativeFrameRendered(eyeSize, viewCount);
            _nativeLayer->end();
        } else if (_nativeLayer->failed()) {
            qWarning("Native video surface unavailable; using the ordinary Qt target");
            _nativeLayer.reset();
            setProperty("binoNativeVideo", false);
            _outputMode = context()->format().stereo() ? Output_OpenGL_Stereo : Output_Left;
            paintGL();
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        QPainter painter(this);
        painter.fillRect(QRect(0, 0, this->width(), placed.top()), Qt::black);
        painter.fillRect(QRect(0, placed.bottom() + 1, this->width(), this->height() - placed.bottom() - 1), Qt::black);
        painter.fillRect(QRect(0, placed.top(), placed.left(), placed.height()), Qt::black);
        painter.fillRect(QRect(placed.right() + 1, placed.top(), this->width() - placed.right() - 1, placed.height()), Qt::black);
        painter.translate(placed.topLeft());
        // Paint at the display raster, independently of the video's stored samples.
        painter.scale(1.0 / devicePixelRatioF(), 1.0 / devicePixelRatioF());
        Bino::instance()->paintOverlays(painter, placed.size() * devicePixelRatioF());
        painter.end();
        _updateTimer.start(40);
        return;
    }

    // Put the views on screen in the current mode
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    float relWidth = 1.0f;
    float relHeight = 1.0f;
    float screenAspectRatio = width / float(height);
    if (screenAspectRatio < frameDisplayAspectRatio)
        relHeight = screenAspectRatio / frameDisplayAspectRatio;
    else
        relWidth = frameDisplayAspectRatio / screenAspectRatio;
    _lastFrameRelWidth = relWidth;
    _lastFrameRelHeight = relHeight;
    rebuildDisplayPrgIfNecessary();
    glUseProgram(_displayPrg.programId());
    _displayPrg.setUniformValue("nativeOffsetX", 0);
    _displayPrg.setUniformValue("view0", 0);
    _displayPrg.setUniformValue("view1", 1);
    _displayPrg.setUniformValue("relativeWidth", relWidth);
    _displayPrg.setUniformValue("relativeHeight", relHeight);
    _displayPrg.setUniformValue("nativeSamples", width == viewWidth && height == viewHeight
            && relWidth == 1.0f && relHeight == 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _viewTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _viewTex[1]);
    glBindVertexArray(_quadVao);
    _displayPrg.setUniformValue("outputModeLeftRightView", selectedView);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(currentTargetBuffer()));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

    // Redraw at least 25 times per second, for overlay UI updates
    _updateTimer.start(40);
}

void Widget::resizeGL(int w, int h)
{
    _width = w;
    _height = h;
}

void Widget::keyPressEvent(QKeyEvent* e)
{
    Bino::instance()->keyPressEvent(e);
}

static QPointF toPixelCoord(const QMatrix4x4 P, const QVector3D& v, int w, int h)
{
    QVector4D clipSpace = P.map(QVector4D(v, 1.0f));
    QVector2D ndc(clipSpace.x() / clipSpace.w(), clipSpace.y() / clipSpace.w());
    return QPointF((ndc.x() + 1.0f) * 0.5f * w, (ndc.y() + 1.0f) * 0.5f * h);
}

QPointF Widget::toView(const QPointF& pos) const
{
    float tx = (pos.x() / _width  - 0.5f * (1.0f - _lastFrameRelWidth )) / _lastFrameRelWidth ;
    float ty = (pos.y() / _height - 0.5f * (1.0f - _lastFrameRelHeight)) / _lastFrameRelHeight;
    if (Bino::instance()->assumeSurroundMode() != Surround_Off) {
        QPointF tl = toPixelCoord(_surroundProjectionMatrix, Bino::surroundCubeScale * QVector3D(+1.0f, +1.0f, +1.0f), _width, _height);
        QPointF br = toPixelCoord(_surroundProjectionMatrix, Bino::surroundCubeScale * QVector3D(-1.0f, -1.0f, +1.0f), _width, _height);
        QRectF cubeSide(tl, br);
        tx = (tx * _width  - cubeSide.left()) / cubeSide.width();
        ty = (ty * _height - cubeSide.top()) / cubeSide.height();
    }
    return QPointF(tx, ty);
}

void Widget::mousePressEvent(QMouseEvent* e)
{
    if (Bino::instance()->overlayUIPointerPress(toView(e->position()))) {
        _inOverlayUIEvent = true;
    } else {
        _inSurroundMovement = true;
        _surroundMovementStart = e->position();
        _surroundHorizontalAngleCurrent = 0.0f;
        _surroundVerticalAngleCurrent = 0.0f;
    }
}

void Widget::mouseReleaseEvent(QMouseEvent* e)
{
    if (_inOverlayUIEvent) {
        Bino::instance()->overlayUIPointerRelease(toView(e->position()));
        _inOverlayUIEvent = false;
    } else {
        _inSurroundMovement = false;
        _surroundHorizontalAngleBase += _surroundHorizontalAngleCurrent;
        _surroundVerticalAngleBase += _surroundVerticalAngleCurrent;
        _surroundHorizontalAngleCurrent = 0.0f;
        _surroundVerticalAngleCurrent = 0.0f;
    }
}

void Widget::mouseMoveEvent(QMouseEvent* e)
{
    // Support for HighDPI output
    int width = _width * devicePixelRatioF();
    int height = _height * devicePixelRatioF();

    if (_inSurroundMovement) {
        // position delta
        QPointF posDelta = e->position() - _surroundMovementStart;
        // horizontal angle delta
        float dx = posDelta.x();
        float xf = dx / width; // in [-1,+1]
        _surroundHorizontalAngleCurrent = xf * 180.0f;
        // vertical angle
        float dy = posDelta.y();
        float yf = dy / height; // in [-1,+1]
        _surroundVerticalAngleCurrent = yf * 90.0f;
        update();
    } else {
        Bino::instance()->overlayUIPointerMove(toView(e->position()));
        update();
    }
}

void Widget::wheelEvent(QWheelEvent* e)
{
    setSurroundVerticalFieldOfView(_surroundVerticalFOV - e->angleDelta().y() / 120.0f);
    update();
}

void Widget::mediaChanged(PlaylistEntry)
{
    _inSurroundMovement = false;
    _surroundHorizontalAngleBase = 0.0f;
    _surroundVerticalAngleBase = 0.0f;
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}
