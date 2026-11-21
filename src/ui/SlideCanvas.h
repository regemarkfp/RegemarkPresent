#pragma once

#include "core/Types.h"

#include <QImage>
#include <QWidget>

// Interactive slide preview for the editor: click a text box to select it,
// drag to move, drag the handles to resize. Snaps to the frame's centre
// lines, edges and safe margins, and shows guides while snapping.
class SlideCanvas : public QWidget {
    Q_OBJECT
public:
    explicit SlideCanvas(QWidget *parent = nullptr);

    // The rendered slide (background + text) and the boxes to interact with.
    void setSlide(const QImage &rendered, const QList<QRectF> &boxes, int selected);

    // Where box `i` is drawn, and how many widget pixels a 1080p pixel is:
    // the in-place text editor sits exactly over the box at this scale.
    QRectF boxRectInWidget(int i) const { return i >= 0 && i < m_boxes.size() ? toWidget(m_boxes[i]) : QRectF(); }
    double displayScale() const { return frame().height() / 1080.0; }

    QSize sizeHint() const override { return QSize(640, 360); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

signals:
    void boxSelected(int index);
    void boxMoved(int index, const QRectF &rect);   // normalised 0..1
    void boxEditRequested(int index);               // double-click / Enter: edit its text
    void pressed();                                 // any click on the canvas itself
    void layoutChanged();                           // resized: overlays must follow

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    enum Handle { None = -1, Move = 0, TL, T, TR, R, BR, B, BL, L };

    QRectF frame() const;                                  // where the slide is drawn
    QRectF toWidget(const QRectF &n) const;
    QPointF toNorm(const QPointF &p) const;
    Handle handleAt(const QPointF &p) const;
    QRectF snapped(const QRectF &r, Handle h);

    QImage m_image;
    QList<QRectF> m_boxes;
    int m_selected = -1;

    Handle m_drag = None;
    QPointF m_pressNorm;
    QRectF m_startRect;
    QList<double> m_guidesX, m_guidesY;   // normalised, shown while dragging
};
