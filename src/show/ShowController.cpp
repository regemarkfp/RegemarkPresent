#include "show/ShowController.h"

ShowController::ShowController(QObject *parent) : QObject(parent)
{
    m_advance.setSingleShot(true);
    connect(&m_advance, &QTimer::timeout, this, &ShowController::next);
}

void ShowController::take(const Presentation &p, int index)
{
    if (index < 0 || index >= p.slides.size()) return;
    m_pres = p;
    m_hasPres = true;
    m_index = index;
    m_textVisible = true;
    m_backgroundVisible = true;
    if (p.slides[index].background.isSet()) m_hasOverride = false;
    publish();
}

void ShowController::next()
{
    if (!m_hasPres) return;
    if (m_index + 1 < m_pres.slides.size()) take(m_pres, m_index + 1);
    else if (m_pres.loop && !m_pres.slides.isEmpty()) take(m_pres, 0);
}

void ShowController::previous()
{
    if (m_hasPres && m_index > 0) take(m_pres, m_index - 1);
}

void ShowController::clearText()
{
    m_textVisible = false;
    publish();
}

void ShowController::clearBackground()
{
    m_backgroundVisible = false;
    m_hasOverride = false;
    publish();
}

void ShowController::clearAll()
{
    m_advance.stop();
    m_hasPres = false;
    m_index = -1;
    m_hasOverride = false;
    m_backgroundVisible = false;
    publish();
}

void ShowController::setLiveBackground(const Background &bg)
{
    m_override = bg;
    m_hasOverride = true;
    m_backgroundVisible = true;
    publish();
}

void ShowController::presentationUpdated(const Presentation &p)
{
    if (!m_hasPres || p.id != m_pres.id) return;
    m_pres = p;
    m_index = qMin(m_index, int(p.slides.size()) - 1);
    if (m_index < 0) m_hasPres = false;
    publish();
}

void ShowController::publish()
{
    LiveState s;
    s.textVisible = m_textVisible;
    s.backgroundVisible = m_backgroundVisible;
    if (m_hasPres) {
        s.hasSlide = true;
        s.slide = m_pres.slides[m_index];
        s.title = m_pres.title;
        s.index = m_index;
        s.count = m_pres.slides.size();
        s.hasNext = m_index + 1 < m_pres.slides.size();
        if (s.hasNext) s.next = m_pres.slides[m_index + 1];
        s.background = m_pres.effectiveBackground(m_index);
        s.transition = m_pres.effectiveTransition(m_index);
    }
    if (m_hasOverride) s.background = m_override;
    m_state = s;

    m_advance.stop();
    if (m_hasPres && m_textVisible && m_pres.slides[m_index].advanceMs > 0)
        m_advance.start(m_pres.slides[m_index].advanceMs);
    emit stateChanged();
}
