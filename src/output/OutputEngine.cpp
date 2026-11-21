#include "output/OutputEngine.h"

#include "output/DeckLinkOutput.h"
#include "output/DisplayOutput.h"
#include "output/NdiOutput.h"
#include "show/ShowController.h"

OutputEngine::OutputEngine(ShowController *show, QObject *parent)
    : QObject(parent), m_show(show)
{
    m_clock.start();
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &OutputEngine::tick);
    connect(show, &ShowController::stateChanged, this, &OutputEngine::onStateChanged);
    m_timer.start(4);
}

OutputEngine::~OutputEngine() = default;

void OutputEngine::configure(const QList<ScreenConfig> &screens)
{
    m_screens.clear();   // tears down windows / NDI senders / DeckLink ports
    const qint64 now = m_clock.elapsed();
    for (const ScreenConfig &cfg : screens) {
        auto s = std::make_unique<Screen>(&m_pool);
        s->renderer.setConfig(cfg);
        s->renderer.setState(m_show->state(), now);
        if (m_enabled) buildSinks(*s);
        m_screens.push_back(std::move(s));
    }
}

void OutputEngine::setOutputsEnabled(bool enabled)
{
    if (enabled == m_enabled) return;
    m_enabled = enabled;
    for (auto &s : m_screens) {
        s->sinks.clear();
        if (m_enabled) buildSinks(*s);
    }
}

void OutputEngine::buildSinks(Screen &s)
{
    const ScreenConfig &cfg = s.renderer.config();
    for (const OutputTarget &t : cfg.targets) {
        if (!t.enabled) continue;
        switch (t.kind) {
        case OutputTarget::Kind::Display:
            s.sinks.emplace_back(new DisplayOutput(t.display, cfg.name));
            break;
        case OutputTarget::Kind::Ndi:
            s.sinks.emplace_back(new NdiOutput(t.ndiName, cfg.fps, cfg.look.transparent));
            break;
        case OutputTarget::Kind::DeckLink:
            s.sinks.emplace_back(createDeckLinkOutput(t.deckLinkDevice, cfg.resolution, cfg.fps, t.deckLinkKey));
            break;
        }
    }
}

QStringList OutputEngine::status(int i) const
{
    QStringList out;
    for (const auto &sink : m_screens[i]->sinks) out << sink->status();
    return out;
}

void OutputEngine::onStateChanged()
{
    const qint64 now = m_clock.elapsed();
    for (auto &s : m_screens) s->renderer.setState(m_show->state(), now);
}

void OutputEngine::tick()
{
    const double now = m_clock.nsecsElapsed() / 1e6;
    for (size_t i = 0; i < m_screens.size(); ++i) {
        Screen &s = *m_screens[i];
        if (now < s.nextDueMs) continue;
        const double interval = 1000.0 / qMax(1.0, s.renderer.config().fps);
        // Stay on the frame grid; if we fell far behind, resync instead of bursting.
        s.nextDueMs = (now - s.nextDueMs > interval * 2) ? now + interval : s.nextDueMs + interval;

        s.last = s.renderer.render(qint64(now));
        for (auto &sink : s.sinks) sink->pushFrame(s.last);
        emit frameRendered(int(i));
    }
}
