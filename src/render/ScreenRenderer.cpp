#include "render/ScreenRenderer.h"

#include "render/MediaPool.h"
#include "render/TextRenderer.h"
#include "show/ShowTimer.h"

#include <QEasingCurve>
#include <QJsonDocument>
#include <QPainter>
#include <QTime>

static double progress(qint64 elapsed, int duration)
{
    if (duration <= 0) return 1.0;
    return qBound(0.0, double(elapsed) / duration, 1.0);
}

static QByteArray textSignature(const LiveState &s)
{
    if (!s.hasSlide || !s.textVisible) return {};
    QJsonArray boxes;
    for (const TextBox &b : s.slide.boxes) boxes.append(b.toJson());
    return s.slide.id.toUtf8() + QJsonDocument(boxes).toJson(QJsonDocument::Compact);
}

void ScreenRenderer::setConfig(const ScreenConfig &cfg)
{
    m_cfg = cfg;
    m_prevText = QImage();
    rebuildLayers();
}

void ScreenRenderer::setState(const LiveState &state, qint64 nowMs)
{
    const bool textChanged = textSignature(state) != textSignature(m_state);

    if (textChanged && m_cfg.role != ScreenRole::Confidence) {
        // Freeze what is on screen now so it can transition out.
        m_prevText = QImage(m_cfg.resolution, QImage::Format_ARGB32_Premultiplied);
        m_prevText.fill(Qt::transparent);
        QPainter p(&m_prevText);
        drawLayers(p, 0, false);
    }

    m_state = state;
    if (m_cfg.role == ScreenRole::Confidence) {
        rebuildLayers();
    } else if (textChanged) {
        rebuildLayers();
        m_textStart = nowMs;
        m_transition = state.transition;
    }

    const Background target = state.backgroundVisible ? state.background : Background();
    if (target != m_bg) {
        m_prevBg = m_bg;
        m_bg = target;
        m_bgStart = nowMs;
        m_bgDuration = state.transition.type == TransitionType::Cut ? 0 : state.transition.durationMs;
    }
}

void ScreenRenderer::rebuildLayers()
{
    m_layers.clear();
    m_confCurrent = m_confNext = QImage();
    m_hasTokens = false;
    for (const TextBox &b : m_state.slide.boxes)
        if (ShowTimer::hasTokens(b.text)) m_hasTokens = true;
    const QSize res = m_cfg.resolution;
    const double scale = m_cfg.scale();

    if (m_cfg.role == ScreenRole::Confidence) {
        TextStyle cur;
        cur.fontSize = 96;
        cur.shadow = false;
        cur.autoFit = true;
        TextStyle next = cur;
        next.fontSize = 64;
        next.bold = false;
        next.color = QColor("#ffd84a");
        auto allText = [](const Slide &s) {
            QStringList t;
            for (const TextBox &b : s.boxes)
                if (!b.text.trimmed().isEmpty()) t << b.text;
            return t.join('\n');
        };
        if (m_state.hasSlide) {
            TextBox b;
            b.text = allText(m_state.slide);
            m_confCurrent = TextRenderer::renderBox(b, cur, QSize(res.width() * 0.94, res.height() * 0.52), scale);
        }
        if (m_state.hasNext && m_cfg.look.showNext) {
            TextBox b;
            b.text = allText(m_state.next);
            m_confNext = TextRenderer::renderBox(b, next, QSize(res.width() * 0.94, res.height() * 0.22), scale);
        }
        return;
    }

    if (!m_state.hasSlide || !m_state.textVisible || !m_cfg.look.showText) return;

    for (const TextBox &box : m_state.slide.boxes) {
        const QRectF r = TextRenderer::effectiveRect(box, &m_cfg.look);
        BoxLayer layer;
        layer.rect = QRect(qRound(r.x() * res.width()), qRound(r.y() * res.height()),
                           qRound(r.width() * res.width()), qRound(r.height() * res.height()));
        if (layer.rect.isEmpty() || (box.isText() && box.text.trimmed().isEmpty())) continue;

        layer.image = QImage(layer.rect.size(), QImage::Format_ARGB32_Premultiplied);
        layer.image.fill(box.isMain && box.isText() ? m_cfg.look.barColor : QColor(Qt::transparent));
        QPainter p(&layer.image);
        p.drawImage(0, 0, TextRenderer::renderBox(box, TextRenderer::effectiveStyle(box, &m_cfg.look),
                                                  layer.rect.size(), scale));
        layer.build = box.build;
        layer.delayMs = box.buildDelayMs;
        layer.durationMs = box.buildDurationMs;
        m_layers.append(layer);
    }
}

void ScreenRenderer::drawLayers(QPainter &p, qint64 elapsed, bool animate) const
{
    const double scale = m_cfg.scale();
    for (const BoxLayer &l : m_layers) {
        if (!animate || l.build == BuildAnimation::None) {
            p.drawImage(l.rect.topLeft(), l.image);
            continue;
        }
        const double b = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            progress(elapsed - l.delayMs, l.durationMs));
        if (b <= 0) continue;

        p.save();
        const double baseOpacity = p.opacity();
        switch (l.build) {
        case BuildAnimation::Fade:
            p.setOpacity(baseOpacity * b);
            p.drawImage(l.rect.topLeft(), l.image);
            break;
        case BuildAnimation::Rise:
            p.setOpacity(baseOpacity * b);
            p.drawImage(l.rect.topLeft() + QPointF(0, (1 - b) * 60 * scale), l.image);
            break;
        case BuildAnimation::Zoom: {
            p.setOpacity(baseOpacity * b);
            const QPointF c = QRectF(l.rect).center();
            p.translate(c);
            p.scale(0.6 + 0.4 * b, 0.6 + 0.4 * b);
            p.translate(-c);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.drawImage(l.rect.topLeft(), l.image);
            break;
        }
        case BuildAnimation::Wipe:
            p.setClipRect(QRect(l.rect.topLeft(), QSize(qRound(l.rect.width() * b), l.rect.height())));
            p.drawImage(l.rect.topLeft(), l.image);
            break;
        case BuildAnimation::None:
            break;
        }
        p.restore();
    }
}

QImage ScreenRenderer::render(qint64 nowMs)
{
    if (m_hasTokens || m_cfg.role == ScreenRole::Confidence) {
        const qint64 second = QDateTime::currentSecsSinceEpoch();
        if (second != m_tokenSecond) {
            m_tokenSecond = second;
            if (m_hasTokens) rebuildLayers();
        }
    }

    QImage frame(m_cfg.resolution, QImage::Format_ARGB32_Premultiplied);
    frame.fill(m_cfg.look.transparent ? QColor(Qt::transparent) : QColor(Qt::black));
    QPainter p(&frame);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Background layer with crossfade.
    if (m_cfg.look.showBackground) {
        const double t = progress(nowMs - m_bgStart, m_bgDuration);
        if (t < 1.0 && m_prevBg.isSet())
            TextRenderer::drawBackground(p, frame.rect(), m_prevBg, m_pool->frame(m_prevBg));
        else
            m_prevBg = Background();
        if (m_bg.isSet()) {
            p.setOpacity(t);
            TextRenderer::drawBackground(p, frame.rect(), m_bg, m_pool->frame(m_bg));
            p.setOpacity(1.0);
        }
    }

    if (m_cfg.role == ScreenRole::Confidence) {
        drawConfidence(p);
        return frame;
    }

    // Text layer: previous slide transitions out, current one transitions in
    // and then runs its per-box builds.
    const qint64 elapsed = nowMs - m_textStart;
    const int duration = m_transition.type == TransitionType::Cut ? 0 : m_transition.durationMs;
    const double raw = progress(elapsed, duration);
    const double t = QEasingCurve(QEasingCurve::InOutCubic).valueForProgress(raw);
    const int w = frame.width(), h = frame.height();

    if (raw < 1.0 && !m_prevText.isNull()) {
        p.save();
        switch (m_transition.type) {
        case TransitionType::Fade: p.setOpacity(1 - t); break;
        case TransitionType::SlideLeft: p.translate(-t * w, 0); break;
        case TransitionType::SlideUp: p.translate(0, -t * h); break;
        case TransitionType::Zoom:
            p.setOpacity(1 - t);
            p.translate(w / 2.0, h / 2.0);
            p.scale(1 + 0.15 * t, 1 + 0.15 * t);
            p.translate(-w / 2.0, -h / 2.0);
            break;
        case TransitionType::Cut: break;
        }
        p.drawImage(0, 0, m_prevText);
        p.restore();
    } else {
        m_prevText = QImage();
    }

    p.save();
    switch (m_transition.type) {
    case TransitionType::Fade: p.setOpacity(t); break;
    case TransitionType::SlideLeft: p.translate((1 - t) * w, 0); break;
    case TransitionType::SlideUp: p.translate(0, (1 - t) * h); break;
    case TransitionType::Zoom:
        p.setOpacity(t);
        p.translate(w / 2.0, h / 2.0);
        p.scale(0.85 + 0.15 * t, 0.85 + 0.15 * t);
        p.translate(-w / 2.0, -h / 2.0);
        break;
    case TransitionType::Cut: break;
    }
    drawLayers(p, elapsed, true);
    p.restore();
    return frame;
}

void ScreenRenderer::drawConfidence(QPainter &p)
{
    const QSize res = m_cfg.resolution;
    const double s = m_cfg.scale();
    const ScreenLook &look = m_cfg.look;
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Header: slide label + presentation, clock on the right.
    const int headerH = qRound(res.height() * 0.08);
    QFont f("Arial");
    f.setPixelSize(qRound(44 * s));
    f.setBold(true);
    p.setFont(f);
    int x = qRound(res.width() * 0.03);
    if (look.showLabel && m_state.hasSlide && !m_state.slide.label.isEmpty()) {
        const QString label = m_state.slide.label.toUpper();
        const int lw = p.fontMetrics().horizontalAdvance(label) + qRound(30 * s);
        const QRect lr(x, qRound(12 * s), lw, headerH - qRound(18 * s));
        p.fillRect(lr, m_state.slide.labelColor.isValid() ? m_state.slide.labelColor : QColor("#3a3a3a"));
        p.setPen(Qt::white);
        p.drawText(lr, Qt::AlignCenter, label);
        x += lw + qRound(24 * s);
    }
    p.setPen(QColor(180, 180, 180));
    f.setBold(false);
    p.setFont(f);
    QString title = m_state.title;
    if (m_state.count > 0) title += QStringLiteral("  %1/%2").arg(m_state.index + 1).arg(m_state.count);
    p.drawText(QRect(x, 0, res.width() / 2, headerH), Qt::AlignVCenter | Qt::AlignLeft, title);

    if (look.showClock) {
        f.setPixelSize(qRound(56 * s));
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        QString clock = QTime::currentTime().toString("HH:mm:ss");
        p.drawText(QRect(0, 0, res.width() - qRound(res.width() * 0.03), headerH),
                   Qt::AlignVCenter | Qt::AlignRight, clock);
        // The countdown sits left of the clock, red in the last minute.
        const ShowTimer *timer = ShowTimer::instance();
        if (timer->isSet()) {
            const int clockW = p.fontMetrics().horizontalAdvance(clock) + qRound(40 * s);
            p.setPen(timer->remainingMs() <= 60000 ? QColor("#ff5c6c") : QColor("#c9a2ff"));
            p.drawText(QRect(0, 0, res.width() - qRound(res.width() * 0.03) - clockW, headerH),
                       Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("⏱ ") + timer->text());
        }
    }
    p.fillRect(QRect(0, headerH, res.width(), qMax(1, qRound(2 * s))), QColor(80, 80, 80));

    const int marginX = qRound(res.width() * 0.03);
    if (!m_confCurrent.isNull()) p.drawImage(marginX, qRound(res.height() * 0.1), m_confCurrent);
    if (!m_state.textVisible) {
        f.setPixelSize(qRound(30 * s));
        p.setFont(f);
        p.setPen(QColor("#ff6060"));
        p.drawText(QRect(marginX, headerH, res.width(), qRound(40 * s)), Qt::AlignLeft | Qt::AlignVCenter,
                   QObject::tr("TEXT CLEARED ON AUDIENCE"));
    }

    const int nextTop = qRound(res.height() * 0.64);
    if (look.showNext && m_state.hasNext) {
        p.fillRect(QRect(0, nextTop, res.width(), qMax(1, qRound(2 * s))), QColor(80, 80, 80));
        f.setPixelSize(qRound(30 * s));
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor("#ffd84a"));
        p.drawText(QRect(marginX, nextTop + qRound(6 * s), res.width(), qRound(40 * s)), Qt::AlignLeft | Qt::AlignVCenter,
                   QObject::tr("NEXT: %1").arg(m_state.next.label.toUpper()));
        if (!m_confNext.isNull()) p.drawImage(marginX, nextTop + qRound(48 * s), m_confNext);
    }

    if (look.showNotes && m_state.hasSlide && !m_state.slide.notes.isEmpty()) {
        const QRect nr(0, qRound(res.height() * 0.92), res.width(), res.height() - qRound(res.height() * 0.92));
        p.fillRect(nr, QColor(20, 60, 90));
        f.setPixelSize(qRound(36 * s));
        f.setBold(false);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(nr.adjusted(marginX, 0, -marginX, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                   m_state.slide.notes);
    }
}
