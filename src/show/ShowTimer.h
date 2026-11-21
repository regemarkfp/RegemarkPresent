#pragma once

#include <QDateTime>
#include <QObject>

// The service countdown. One per app: shown on confidence screens and
// substituted into slide text wherever it says {timer}.
class ShowTimer : public QObject {
    Q_OBJECT
public:
    static ShowTimer *instance();

    void countdown(qint64 ms);              // e.g. 5 minutes from now
    void countdownTo(const QTime &time);    // e.g. until 10:00 (tomorrow if already past)
    void pause();
    void resume();
    void reset();

    bool isSet() const { return m_set; }
    bool isRunning() const { return m_set && !m_paused; }
    qint64 remainingMs() const;
    QString text() const;                   // "4:59", "1:02:03", or "0:00"

    // Replace {timer}, {clock} and {date} in slide text.
    static QString expand(const QString &text);
    static bool hasTokens(const QString &text);

signals:
    void changed();

private:
    ShowTimer() = default;

    bool m_set = false;
    bool m_paused = false;
    QDateTime m_target;
    qint64 m_pausedRemaining = 0;
};
