#pragma once

#include "core/ScreenConfig.h"

#include <QImage>

class MediaPool;

// What is live right now, independent of any particular screen.
struct LiveState {
    bool hasSlide = false;
    Slide slide;
    bool hasNext = false;
    Slide next;
    QString title;
    int index = -1;
    int count = 0;

    Background background;
    Transition transition;
    bool textVisible = true;
    bool backgroundVisible = true;
};

// Turns the live state into frames for one screen: applies the screen's look,
// runs slide transitions, per-box build animations and background crossfades.
class ScreenRenderer {
public:
    explicit ScreenRenderer(MediaPool *pool) : m_pool(pool) {}

    void setConfig(const ScreenConfig &cfg);
    const ScreenConfig &config() const { return m_cfg; }

    void setState(const LiveState &state, qint64 nowMs);
    QImage render(qint64 nowMs);

private:
    struct BoxLayer {
        QImage image;
        QRect rect;
        BuildAnimation build = BuildAnimation::None;
        int delayMs = 0;
        int durationMs = 0;
    };

    void rebuildLayers();
    void drawLayers(QPainter &p, qint64 elapsed, bool animate) const;
    void drawConfidence(QPainter &p);

    MediaPool *m_pool;
    ScreenConfig m_cfg;
    LiveState m_state;

    QList<BoxLayer> m_layers;
    QImage m_prevText;
    qint64 m_textStart = 0;
    Transition m_transition;

    Background m_bg, m_prevBg;
    qint64 m_bgStart = 0;
    int m_bgDuration = 0;

    QImage m_confCurrent, m_confNext;
    bool m_hasTokens = false;        // some text uses {timer}/{clock}: refresh every second
    qint64 m_tokenSecond = -1;
};
