/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <QRect>
#include <memory>
class QWidget;
class NativeX11Layer
{
public:
    explicit NativeX11Layer(QWidget* area);
    ~NativeX11Layer();
    static bool supported();
    bool begin(const QSize& eyeSize, int views, const QRect& destination);
    bool failed() const;
    void end();
    void hide();
private:
    struct Private;
    std::unique_ptr<Private> d;
};
