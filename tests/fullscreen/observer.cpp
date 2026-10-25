// SPDX-License-Identifier: GPL-3.0-or-later
// Observe the installed executable without replacing its input or window handlers.
#include <QApplication>
#include <QWidget>
#include <QAction>
#include <QIcon>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QTimer>
#include <QWindow>

namespace {
class Observer : public QObject {
    QString directory = qEnvironmentVariable("CASE_DIR");
    QString previous;
public:
    using QObject::QObject;
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
                || event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse=static_cast<QMouseEvent*>(event);
            QFile log(directory+"/events.jsonl");
            if (log.open(QIODevice::WriteOnly|QIODevice::Append))
                log.write(QJsonDocument(QJsonObject{{"object",object->metaObject()->className()},
                    {"type",int(event->type())},{"x",mouse->position().x()},{"y",mouse->position().y()}}).toJson(QJsonDocument::Compact)+'\n');
        }
        return false;
    }
    void snapshot() {
        QFile request(directory+"/request");
        if (!request.open(QIODevice::ReadOnly)) return;
        const QString name=QString::fromUtf8(request.readAll()).trimmed();
        if (name==previous || name.isEmpty()) return;
        if (name=="quit") { qApp->quit(); return; }
        QWidget *gui=nullptr,*widget=nullptr;
        for (auto *window : QApplication::topLevelWidgets())
            if (QByteArray(window->metaObject()->className())=="Gui") gui=window;
        if (!gui) return;
        for (auto *child : gui->findChildren<QWidget*>())
            if (QByteArray(child->metaObject()->className())=="Widget") widget=child;
        if (!widget || !gui->windowHandle()) return;
        bool playing=false;
        for (auto *action : gui->findChildren<QAction*>())
            if (action->shortcuts().contains(QKeySequence(Qt::Key_Space)))
                playing=action->isEnabled() && !action->isChecked();
        const QRect r(widget->mapToGlobal(QPoint()),widget->size());
        const auto m=gui->windowHandle()->frameMargins();
        QJsonObject state{{"fullscreen",gui->isFullScreen()},{"maximized",gui->isMaximized()},
            {"state",int(gui->windowState())},{"widget",QJsonArray{r.x(),r.y(),r.width(),r.height()}},
            {"margins",QJsonArray{m.left(),m.top(),m.right(),m.bottom()}},
            {"playing",playing},{"native",widget->property("binoNativeVideo").toBool()}};
        QFile result(directory+"/"+name+".json");
        if (!result.open(QIODevice::WriteOnly)) qFatal("cannot write observer state");
        result.write(QJsonDocument(state).toJson());previous=name;
    }
};
void initialize() {
    if (qEnvironmentVariableIsEmpty("CASE_DIR")) return;
    QTimer::singleShot(0,[] {
        if (qEnvironmentVariableIsSet("BINO_TEST_ICONS")) {
            QIcon::setThemeSearchPaths({qEnvironmentVariable("BINO_TEST_ICONS")});
            QIcon::setThemeName("breeze-dark");
            if (QIcon::fromTheme("media-playback-start").isNull())
                qFatal("missing test icon theme");
        }
        auto *observer=new Observer(qApp);
        qApp->installEventFilter(observer);
        auto *timer=new QTimer(observer);
        QObject::connect(timer,&QTimer::timeout,observer,[observer]{ observer->snapshot(); });
        timer->start(30);
    });
}
__attribute__((constructor)) void registerObserver() { qAddPreRoutine(initialize); }
}
