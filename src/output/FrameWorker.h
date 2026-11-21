#pragma once

#include "output/OutputSink.h"

#include <QMutex>
#include <QThread>
#include <QWaitCondition>

// Base for sinks that talk to hardware or the network. Frames are handed to a
// dedicated thread through a one-slot mailbox: if the device falls behind,
// stale frames are dropped instead of building latency.
class FrameWorker : public QThread, public OutputSink {
public:
    FrameWorker() = default;
    ~FrameWorker() override;

    void pushFrame(const QImage &frame) override;
    QString status() const override;

protected:
    // Called on the worker thread.
    virtual bool open() = 0;
    virtual void sendFrame(const QImage &frame) = 0;
    virtual void close() = 0;

    void setStatus(const QString &s);
    void stopWorker();

private:
    void run() override;

    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QImage m_pending;
    bool m_stop = false;
    QString m_status;
};
