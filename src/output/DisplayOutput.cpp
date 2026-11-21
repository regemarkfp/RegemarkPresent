#include "output/DisplayOutput.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

DisplayOutput::DisplayOutput(const QString &screenName, const QString &title)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setWindowTitle(title);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setCursor(Qt::BlankCursor);

    QScreen *target = nullptr;
    for (QScreen *s : QGuiApplication::screens())
        if (s->name() == screenName) target = s;

    if (!target) {
        m_status = tr("Display \"%1\" not connected").arg(screenName);
        return;
    }
    setGeometry(target->geometry());
    show();
    windowHandle()->setScreen(target);
    setGeometry(target->geometry());
    showFullScreen();
    m_status = tr("Display %1 (%2x%3)").arg(screenName).arg(target->size().width()).arg(target->size().height());
}

void DisplayOutput::pushFrame(const QImage &frame)
{
    if (!isVisible()) return;
    m_frame = frame;
    update();
}

void DisplayOutput::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_frame.isNull()) return;
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QSizeF s = m_frame.size();
    s.scale(size(), Qt::KeepAspectRatio);
    p.drawImage(QRectF((width() - s.width()) / 2, (height() - s.height()) / 2, s.width(), s.height()), m_frame);
}

QStringList DisplayOutput::availableDisplays()
{
    QStringList names;
    for (QScreen *s : QGuiApplication::screens()) names << s->name();
    return names;
}
