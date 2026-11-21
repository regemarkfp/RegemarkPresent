#pragma once

#include "render/ScreenRenderer.h"

#include <QObject>
#include <QTimer>

// The single source of truth for what is live. UI actions and keyboard
// shortcuts go through here; OutputEngine renders whatever it publishes.
class ShowController : public QObject {
    Q_OBJECT
public:
    explicit ShowController(QObject *parent = nullptr);

    void take(const Presentation &p, int index);
    void next();
    void previous();

    void clearText();
    void clearBackground();
    void clearAll();

    // Media bin "send live": replaces the background until a slide with its
    // own background is taken or the background is cleared.
    void setLiveBackground(const Background &bg);

    // Re-read the live presentation after it was edited.
    void presentationUpdated(const Presentation &p);

    const LiveState &state() const { return m_state; }
    QString livePresentationId() const { return m_hasPres ? m_pres.id : QString(); }
    int liveIndex() const { return m_index; }

signals:
    void stateChanged();
    // next() was asked for past the last slide of a non-looping item.
    void reachedEnd();

private:
    void publish();

    Presentation m_pres;
    bool m_hasPres = false;
    int m_index = -1;
    bool m_textVisible = true;
    bool m_backgroundVisible = true;
    bool m_hasOverride = false;
    Background m_override;
    LiveState m_state;
    QTimer m_advance;                // announcement loops / timed slides
};
