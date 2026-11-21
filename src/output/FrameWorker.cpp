#include "output/FrameWorker.h"

FrameWorker::~FrameWorker()
{
    // Subclasses must call stopWorker() in their own destructor so close()
    // still dispatches to them; this is a safety net.
    stopWorker();
}

void FrameWorker::stopWorker()
{
    {
        QMutexLocker lock(&m_mutex);
        m_stop = true;
        m_cond.wakeAll();
    }
    wait();
}

void FrameWorker::pushFrame(const QImage &frame)
{
    QMutexLocker lock(&m_mutex);
    m_pending = frame;
    m_cond.wakeOne();
}

QString FrameWorker::status() const
{
    QMutexLocker lock(&m_mutex);
    return m_status;
}

void FrameWorker::setStatus(const QString &s)
{
    QMutexLocker lock(&m_mutex);
    m_status = s;
}

void FrameWorker::run()
{
    if (!open()) return;
    for (;;) {
        QImage frame;
        {
            QMutexLocker lock(&m_mutex);
            while (!m_stop && m_pending.isNull()) m_cond.wait(&m_mutex);
            if (m_stop) break;
            frame = m_pending;
            m_pending = QImage();
        }
        sendFrame(frame);
    }
    close();
}
