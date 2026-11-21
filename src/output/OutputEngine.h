#pragma once

#include "core/ScreenConfig.h"
#include "output/OutputSink.h"
#include "render/MediaPool.h"
#include "render/ScreenRenderer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

class ShowController;

// Drives every configured screen: renders it at its own frame rate from the
// live state and fans each frame out to that screen's sinks.
class OutputEngine : public QObject {
    Q_OBJECT
public:
    OutputEngine(ShowController *show, QObject *parent = nullptr);
    ~OutputEngine() override;

    void configure(const QList<ScreenConfig> &screens);
    void setOutputsEnabled(bool enabled);
    // Switch what screens show (a look change) without touching their outputs.
    void setLooks(const QList<ScreenLook> &looks);
    bool outputsEnabled() const { return m_enabled; }

    int screenCount() const { return int(m_screens.size()); }
    const ScreenConfig &screenConfig(int i) const { return m_screens[i]->renderer.config(); }
    QImage lastFrame(int i) const { return m_screens[i]->last; }
    QStringList status(int i) const;

    MediaPool *mediaPool() { return &m_pool; }

signals:
    void frameRendered(int screen);
    void liveVideoFinished();   // the live background was a play-once video and it ended

private:
    struct Screen {
        explicit Screen(MediaPool *pool) : renderer(pool) {}
        ScreenRenderer renderer;
        std::vector<std::unique_ptr<OutputSink>> sinks;
        double nextDueMs = 0;
        QImage last;
    };

    void tick();
    void onStateChanged();
    void buildSinks(Screen &s);

    ShowController *m_show;
    MediaPool m_pool;
    std::vector<std::unique_ptr<Screen>> m_screens;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_enabled = true;
    Background m_lastBackground;
};
