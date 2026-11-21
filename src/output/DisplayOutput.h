#pragma once

#include "output/OutputSink.h"

#include <QWidget>

// Borderless full-screen window on a chosen monitor / projector.
class DisplayOutput : public QWidget, public OutputSink {
    Q_OBJECT
public:
    DisplayOutput(const QString &screenName, const QString &title);

    void pushFrame(const QImage &frame) override;
    QString status() const override { return m_status; }

    static QStringList availableDisplays();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage m_frame;
    QString m_status;
};
