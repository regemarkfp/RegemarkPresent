#pragma once

#include "core/ScreenConfig.h"

#include <QImage>

class QPainter;

namespace TextRenderer {

// Lay out and draw a text box (wrapping, auto-fit, outline, soft shadow) into
// a transparent image of `size`. `scale` is output height / 1080.
QImage renderBox(const TextBox &box, const TextStyle &style, QSize size, double scale);

// The box geometry and style a given screen look resolves to.
QRectF effectiveRect(const TextBox &box, const ScreenLook *look);
const TextStyle &effectiveStyle(const TextBox &box, const ScreenLook *look);

// Draw a background (colour or image/video frame) into `target`.
void drawBackground(QPainter &p, const QRect &target, const Background &bg, const QImage &frame);

// Quick still render of a slide for thumbnails and editor previews. `poster`
// is a pre-scaled still of the background media; without one, images are
// loaded from disk and videos show a placeholder.
QImage renderSlide(const Slide &slide, const Background &bg, QSize size,
                   const ScreenLook *look = nullptr, const QImage &poster = QImage());

}
