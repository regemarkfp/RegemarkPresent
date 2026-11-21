#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTimeEdit;

// Compact countdown controls for the live column: "for 5:00" or "until 10:00".
class TimerPanel : public QWidget {
    Q_OBJECT
public:
    explicit TimerPanel(QWidget *parent = nullptr);

signals:
    // Put a "Service starts in {timer}" slide live.
    void countdownSlideRequested();

private:
    void refresh();

    QComboBox *m_mode;
    QTimeEdit *m_time;
    QPushButton *m_start, *m_pause;
    QLabel *m_display;
};
