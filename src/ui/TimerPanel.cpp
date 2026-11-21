#include "ui/TimerPanel.h"

#include "show/ShowTimer.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

TimerPanel::TimerPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    m_mode = new QComboBox;
    m_mode->addItems({tr("Count down"), tr("Until")});
    m_time = new QTimeEdit;
    m_display = new QLabel(QStringLiteral("–:––"));
    QFont f = m_display->font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    m_display->setFont(f);
    m_display->setMinimumWidth(80);
    m_display->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_mode);
    row->addWidget(m_time, 1);
    row->addWidget(m_display);
    layout->addLayout(row);

    auto *buttons = new QHBoxLayout;
    m_start = new QPushButton(tr("Start"));
    m_start->setProperty("accent", true);
    m_pause = new QPushButton(tr("Pause"));
    auto *reset = new QPushButton(tr("Reset"));
    auto *slide = new QPushButton(tr("Countdown slide"));
    slide->setToolTip(tr("Show \"Service starts in\" with the timer on the audience screen"));
    for (QPushButton *b : {m_start, m_pause, reset, slide}) buttons->addWidget(b);
    layout->addLayout(buttons);

    auto setMode = [this](int mode) {
        if (mode == 0) {
            m_time->setDisplayFormat("mm:ss");
            m_time->setTime(QTime(0, 5, 0));
        } else {
            m_time->setDisplayFormat("HH:mm");
            // Next quarter hour is a sensible guess for a service start.
            const QTime now = QTime::currentTime();
            m_time->setTime(QTime(now.hour(), (now.minute() / 15) * 15).addSecs(15 * 60));
        }
    };
    connect(m_mode, &QComboBox::currentIndexChanged, this, setMode);
    setMode(0);

    ShowTimer *timer = ShowTimer::instance();
    connect(m_start, &QPushButton::clicked, this, [this, timer] {
        const QTime t = m_time->time();
        if (m_mode->currentIndex() == 0) timer->countdown((t.minute() * 60 + t.second()) * 1000LL);
        else timer->countdownTo(t);
    });
    connect(m_pause, &QPushButton::clicked, this, [timer] {
        if (timer->isRunning()) timer->pause();
        else timer->resume();
    });
    connect(reset, &QPushButton::clicked, timer, &ShowTimer::reset);
    connect(slide, &QPushButton::clicked, this, &TimerPanel::countdownSlideRequested);
    connect(timer, &ShowTimer::changed, this, &TimerPanel::refresh);

    auto *tick = new QTimer(this);
    connect(tick, &QTimer::timeout, this, &TimerPanel::refresh);
    tick->start(250);
    refresh();
}

void TimerPanel::refresh()
{
    const ShowTimer *timer = ShowTimer::instance();
    m_display->setText(timer->isSet() ? timer->text() : QStringLiteral("–:––"));
    const bool lastMinute = timer->isSet() && timer->remainingMs() <= 60000;
    m_display->setStyleSheet(QStringLiteral("color: %1").arg(lastMinute ? Theme::danger.name() : Theme::text.name()));
    m_pause->setEnabled(timer->isSet());
    m_pause->setText(timer->isSet() && !timer->isRunning() ? tr("Resume") : tr("Pause"));
}
