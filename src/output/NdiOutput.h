#pragma once

#include "output/FrameWorker.h"

// Publishes a screen as an NDI source. The NDI runtime is loaded at run time
// (it is installed separately from ndi.video), so the app still starts on
// machines without it and simply reports NDI as unavailable.
class NdiOutput : public FrameWorker {
public:
    NdiOutput(const QString &sourceName, double fps, bool alpha);
    ~NdiOutput() override;

    static bool runtimeAvailable(QString *error = nullptr);

protected:
    bool open() override;
    void sendFrame(const QImage &frame) override;
    void close() override;

private:
    QString m_name;
    int m_rateN, m_rateD;
    bool m_alpha;
    void *m_sender = nullptr;
};
