/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <QRect>
#include <QObject>
#include <memory>

class QWidget;

// A video buffer's stored pixels and its place in the window are independent.
class NativeVideoLayer : public QObject
{
public:
    explicit NativeVideoLayer(QWidget* area);
    ~NativeVideoLayer();
    static bool supported();
    static void prepareWindow(QWidget* window);
    bool begin(const QSize& eyeSize, int views, const QRect& destination);
    bool failed() const;
    void end();
    void hide();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
