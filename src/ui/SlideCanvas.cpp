#include "ui/SlideCanvas.h"

#include "ui/Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

static constexpr double kSnap = 0.012;      // snap distance, fraction of the frame
static constexpr double kMinSize = 0.03;
static constexpr double kHandle = 9;        // handle size in widget pixels

SlideCanvas::SlideCanvas(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SlideCanvas::setSlide(const QImage &rendered, const QList<QRectF> &boxes, int selected)
{
    m_image = rendered;
    if (m_drag == None) m_boxes = boxes;   // don't fight the user's drag with stale geometry
    m_selected = selected;
    update();
}

QRectF SlideCanvas::frame() const
{
    QSizeF s(16, 9);
    s.scale(QSizeF(size()) - QSizeF(12, 12), Qt::KeepAspectRatio);
    return QRectF((width() - s.width()) / 2, (height() - s.height()) / 2, s.width(), s.height());
}

QRectF SlideCanvas::toWidget(const QRectF &n) const
{
    const QRectF f = frame();
    return QRectF(f.x() + n.x() * f.width(), f.y() + n.y() * f.height(), n.width() * f.width(), n.height() * f.height());
}

QPointF SlideCanvas::toNorm(const QPointF &p) const
{
    const QRectF f = frame();
    return QPointF((p.x() - f.x()) / f.width(), (p.y() - f.y()) / f.height());
}

static QList<QPointF> handlePoints(const QRectF &r)
{
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(), QPointF(r.right(), r.center().y()),
            r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
}

SlideCanvas::Handle SlideCanvas::handleAt(const QPointF &p) const
{
    if (m_selected < 0 || m_selected >= m_boxes.size()) return None;
    const QRectF r = toWidget(m_boxes[m_selected]);
    const QList<QPointF> pts = handlePoints(r);
    for (int i = 0; i < pts.size(); ++i)
        if (QRectF(pts[i] - QPointF(kHandle, kHandle), QSizeF(kHandle * 2, kHandle * 2)).contains(p))
            return Handle(TL + i);
    return r.contains(p) ? Move : None;
}

QRectF SlideCanvas::snapped(const QRectF &in, Handle h)
{
    m_guidesX.clear();
    m_guidesY.clear();
    QList<double> xs = {0.0, 0.05, 0.5, 0.95, 1.0}, ys = {0.0, 0.05, 0.5, 0.95, 1.0};
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (i == m_selected) continue;
        const QRectF &o = m_boxes[i];
        xs << o.left() << o.center().x() << o.right();
        ys << o.top() << o.center().y() << o.bottom();
    }
    auto nearest = [](double v, const QList<double> &lines, double *line) {
        double best = kSnap;
        bool found = false;
        for (double l : lines)
            if (qAbs(v - l) < best) { best = qAbs(v - l); *line = l; found = true; }
        return found;
    };

    QRectF r = in;
    double line;
    if (h == Move) {
        // Snap whichever of left / centre / right is closest to a guide.
        for (double edge : {r.left(), r.center().x(), r.right()})
            if (nearest(edge, xs, &line)) { r.translate(line - edge, 0); m_guidesX << line; break; }
        for (double edge : {r.top(), r.center().y(), r.bottom()})
            if (nearest(edge, ys, &line)) { r.translate(0, line - edge); m_guidesY << line; break; }
        return r;
    }
    if ((h == TL || h == L || h == BL) && nearest(r.left(), xs, &line)) { r.setLeft(line); m_guidesX << line; }
    if ((h == TR || h == R || h == BR) && nearest(r.right(), xs, &line)) { r.setRight(line); m_guidesX << line; }
    if ((h == TL || h == T || h == TR) && nearest(r.top(), ys, &line)) { r.setTop(line); m_guidesY << line; }
    if ((h == BL || h == B || h == BR) && nearest(r.bottom(), ys, &line)) { r.setBottom(line); m_guidesY << line; }
    return r;
}

void SlideCanvas::mousePressEvent(QMouseEvent *e)
{
    const QPointF p = e->position();
    Handle h = handleAt(p);
    if (h == None) {
        int hit = -1;
        for (int i = int(m_boxes.size()) - 1; i >= 0 && hit < 0; --i)
            if (toWidget(m_boxes[i]).contains(p)) hit = i;
        if (hit != m_selected) {
            m_selected = hit;
            emit boxSelected(hit);
        }
        h = hit >= 0 ? Move : None;
    }
    m_drag = h;
    m_pressNorm = toNorm(p);
    if (m_selected >= 0) m_startRect = m_boxes[m_selected];
    update();
}

void SlideCanvas::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF p = e->position();
    if (m_drag == None || m_selected < 0) {
        static const Qt::CursorShape cursors[] = {Qt::SizeAllCursor, Qt::SizeFDiagCursor, Qt::SizeVerCursor,
                                                  Qt::SizeBDiagCursor, Qt::SizeHorCursor, Qt::SizeFDiagCursor,
                                                  Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor};
        const Handle h = handleAt(p);
        setCursor(h == None ? Qt::ArrowCursor : cursors[h]);
        return;
    }
    const QPointF d = toNorm(p) - m_pressNorm;
    QRectF r = m_startRect;
    switch (m_drag) {
    case Move: r.translate(d); break;
    case TL: r.setTopLeft(r.topLeft() + d); break;
    case T: r.setTop(r.top() + d.y()); break;
    case TR: r.setTopRight(r.topRight() + d); break;
    case R: r.setRight(r.right() + d.x()); break;
    case BR: r.setBottomRight(r.bottomRight() + d); break;
    case B: r.setBottom(r.bottom() + d.y()); break;
    case BL: r.setBottomLeft(r.bottomLeft() + d); break;
    case L: r.setLeft(r.left() + d.x()); break;
    case None: break;
    }
    if (!(e->modifiers() & Qt::AltModifier)) r = snapped(r, m_drag);   // Alt: free placement
    r = r.normalized();
    if (r.width() < kMinSize) r.setWidth(kMinSize);
    if (r.height() < kMinSize) r.setHeight(kMinSize);
    m_boxes[m_selected] = r;
    emit boxMoved(m_selected, r);
    update();
}

void SlideCanvas::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = None;
    m_guidesX.clear();
    m_guidesY.clear();
    update();
}

void SlideCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    for (int i = int(m_boxes.size()) - 1; i >= 0; --i)
        if (toWidget(m_boxes[i]).contains(e->position())) {
            emit boxEditRequested(i);
            return;
        }
}

void SlideCanvas::keyPressEvent(QKeyEvent *e)
{
    if (m_selected < 0) return QWidget::keyPressEvent(e);
    const double step = (e->modifiers() & Qt::ShiftModifier) ? 0.02 : 0.005;
    QPointF d;
    switch (e->key()) {
    case Qt::Key_Left: d = {-step, 0}; break;
    case Qt::Key_Right: d = {step, 0}; break;
    case Qt::Key_Up: d = {0, -step}; break;
    case Qt::Key_Down: d = {0, step}; break;
    default: return QWidget::keyPressEvent(e);
    }
    m_boxes[m_selected].translate(d);
    emit boxMoved(m_selected, m_boxes[m_selected]);
    update();
}

void SlideCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF f = frame();
    p.fillRect(f, Qt::black);
    if (!m_image.isNull()) p.drawImage(f, m_image);

    // Title-safe area, faintly.
    p.setPen(QPen(QColor(255, 255, 255, 40), 1, Qt::DotLine));
    p.drawRect(toWidget(QRectF(0.05, 0.05, 0.9, 0.9)));

    for (int i = 0; i < m_boxes.size(); ++i) {
        if (i == m_selected) continue;
        p.setPen(QPen(QColor(255, 255, 255, 110), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(toWidget(m_boxes[i]));
    }

    p.setPen(QPen(Theme::accent, 1));
    for (double x : m_guidesX) p.drawLine(QPointF(f.x() + x * f.width(), f.top()), QPointF(f.x() + x * f.width(), f.bottom()));
    for (double y : m_guidesY) p.drawLine(QPointF(f.left(), f.y() + y * f.height()), QPointF(f.right(), f.y() + y * f.height()));

    if (m_selected >= 0 && m_selected < m_boxes.size()) {
        const QRectF r = toWidget(m_boxes[m_selected]);
        p.setPen(QPen(Theme::accent, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        p.setBrush(Qt::white);
        p.setPen(QPen(Theme::accent, 1.5));
        for (const QPointF &pt : handlePoints(r))
            p.drawRect(QRectF(pt - QPointF(4.5, 4.5), QSizeF(9, 9)));
    }
}
