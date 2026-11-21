#include "show/ShowTimer.h"

#include <QLocale>

ShowTimer *ShowTimer::instance()
{
    static ShowTimer timer;
    return &timer;
}

void ShowTimer::countdown(qint64 ms)
{
    m_set = true;
    m_paused = false;
    m_target = QDateTime::currentDateTime().addMSecs(ms);
    emit changed();
}

void ShowTimer::countdownTo(const QTime &time)
{
    QDateTime target(QDate::currentDate(), time);
    if (target < QDateTime::currentDateTime()) target = target.addDays(1);
    m_set = true;
    m_paused = false;
    m_target = target;
    emit changed();
}

void ShowTimer::pause()
{
    if (!isRunning()) return;
    m_pausedRemaining = remainingMs();
    m_paused = true;
    emit changed();
}

void ShowTimer::resume()
{
    if (!m_set || !m_paused) return;
    countdown(m_pausedRemaining);
}

void ShowTimer::reset()
{
    m_set = false;
    m_paused = false;
    emit changed();
}

qint64 ShowTimer::remainingMs() const
{
    if (!m_set) return 0;
    if (m_paused) return m_pausedRemaining;
    return qMax<qint64>(0, QDateTime::currentDateTime().msecsTo(m_target));
}

QString ShowTimer::text() const
{
    // Round up so the display reads 5:00 for the whole first second.
    const qint64 total = (remainingMs() + 999) / 1000;
    const qint64 h = total / 3600, m = (total / 60) % 60, s = total % 60;
    if (h > 0) return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

bool ShowTimer::hasTokens(const QString &text)
{
    return text.contains(QLatin1String("{timer}")) || text.contains(QLatin1String("{clock}"))
           || text.contains(QLatin1String("{date}"));
}

QString ShowTimer::expand(const QString &text)
{
    if (!hasTokens(text)) return text;
    QString out = text;
    out.replace(QLatin1String("{timer}"), instance()->text());
    out.replace(QLatin1String("{clock}"), QTime::currentTime().toString("HH:mm"));
    out.replace(QLatin1String("{date}"), QLocale().toString(QDate::currentDate(), QLocale::LongFormat));
    return out;
}
