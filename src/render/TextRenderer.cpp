#include "render/TextRenderer.h"

#include "core/RichText.h"
#include "show/ShowTimer.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>

namespace TextRenderer {

static QImage blurred(const QImage &src, double radius)
{
    // Cheap, good-looking soft shadow: shrink, then smooth-scale back up.
    const int k = qMax(1, qRound(radius / 2));
    if (k <= 1) return src;
    const QImage small = src.scaled(qMax(1, src.width() / k), qMax(1, src.height() / k),
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return small.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

static TextBox expandTokens(const TextBox &box)
{
    TextBox b = box;
    b.text = ShowTimer::expand(box.text);
    QJsonArray paragraphs;
    for (const QJsonValue &p : box.runs) {
        QJsonArray runs;
        for (const QJsonValue &r : p.toArray()) {
            QJsonObject o = r.toObject();
            o["t"] = ShowTimer::expand(o["t"].toString());
            runs.append(o);
        }
        paragraphs.append(runs);
    }
    b.runs = paragraphs;
    return b;
}

static QImage renderText(const TextBox &source, const TextStyle &style, QSize size, double scale);

static QImage cachedImage(const QString &path)
{
    // Slide decks reuse the same photos and logos; decode each once.
    static QHash<QString, QImage> cache;
    auto it = cache.constFind(path);
    if (it != cache.constEnd()) return it.value();
    QImage img(path);
    if (img.width() > 3840 || img.height() > 3840)
        img = img.scaled(3840, 3840, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (cache.size() > 64) cache.clear();
    cache.insert(path, img);
    return img;
}

static QImage renderGraphic(const TextBox &box, QSize size, double scale)
{
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    p.setOpacity(box.opacity);
    if (box.kind == TextBox::Kind::Shape) {
        p.setPen(Qt::NoPen);
        p.setBrush(box.fill);
        const double r = qMin(box.cornerRadius * scale, qMin(size.width(), size.height()) / 2.0);
        p.drawRoundedRect(QRectF(QPointF(0, 0), QSizeF(size)), r, r);
    } else {
        Background as;
        as.type = Background::Type::Image;
        as.fit = box.imageFit;
        const double r = box.cornerRadius * scale;
        if (r > 0) {
            QPainterPath clip;
            clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(size)), r, r);
            p.setClipPath(clip);
        }
        drawBackground(p, out.rect(), as, cachedImage(box.imagePath));
    }
    return out;
}

static QImage withOpacity(const QImage &img, double opacity)
{
    if (opacity >= 1.0) return img;
    QImage out(img.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setOpacity(opacity);
    p.drawImage(0, 0, img);
    return out;
}

QImage renderBox(const TextBox &source, const TextStyle &style, QSize size, double scale)
{
    if (!source.isText()) return renderGraphic(source, size, scale);
    return withOpacity(renderText(source, style, size, scale), source.opacity);
}

static QImage renderText(const TextBox &source, const TextStyle &style, QSize size, double scale)
{
    const TextBox box = ShowTimer::hasTokens(source.text) ? expandTokens(source) : source;
    QImage text(size, QImage::Format_ARGB32_Premultiplied);
    text.fill(Qt::transparent);
    if (size.isEmpty() || box.text.trimmed().isEmpty()) return text;

    QTextDocument doc;
    RichText::toDocument(&doc, box.text, box.runs, style, scale);
    doc.setTextWidth(size.width());

    double fit = 1.0;
    if (style.autoFit) {
        for (int i = 0; i < 5; ++i) {
            const double h = doc.size().height() * fit;
            const double w = doc.idealWidth() * fit;
            if (h <= size.height() && w <= size.width() + 1) break;
            fit = qMax(0.15, fit * qMin(size.height() / h, size.width() / qMax(1.0, w)) * 0.98);
            doc.setTextWidth(size.width() / fit);
        }
    }

    const double docH = doc.size().height() * fit;
    double y = 0;
    if (style.vAlign & Qt::AlignVCenter) y = (size.height() - docH) / 2;
    else if (style.vAlign & Qt::AlignBottom) y = size.height() - docH;

    {
        QPainter p(&text);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        p.translate(0, y);
        p.scale(fit, fit);
        doc.drawContents(&p);
    }

    if (!style.shadow) return text;

    QImage shadow = text;
    {
        QPainter sp(&shadow);
        sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        sp.fillRect(shadow.rect(), style.shadowColor);
    }
    shadow = blurred(shadow, style.shadowBlur * scale);

    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawImage(style.shadowOffset * scale, shadow);
    p.drawImage(0, 0, text);
    return out;
}

QRectF effectiveRect(const TextBox &box, const ScreenLook *look)
{
    return look && look->overrideRect && box.isMain ? look->rect : box.rect;
}

const TextStyle &effectiveStyle(const TextBox &box, const ScreenLook *look)
{
    return look && look->overrideStyle && box.isMain ? look->style : box.style;
}

void drawBackground(QPainter &p, const QRect &target, const Background &bg, const QImage &frame)
{
    if (bg.type == Background::Type::Color) {
        p.fillRect(target, bg.color);
        return;
    }
    if (frame.isNull()) return;

    QSizeF s = frame.size();
    switch (bg.fit) {
    case FitMode::Fill: s.scale(target.size(), Qt::KeepAspectRatioByExpanding); break;
    case FitMode::Fit: s.scale(target.size(), Qt::KeepAspectRatio); break;
    case FitMode::Stretch: s = target.size(); break;
    }
    const QRectF dest(target.x() + (target.width() - s.width()) / 2,
                      target.y() + (target.height() - s.height()) / 2, s.width(), s.height());
    p.save();
    p.setClipRect(target);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(dest, frame);
    p.restore();
}

QImage renderSlide(const Slide &slide, const Background &bg, QSize size,
                   const ScreenLook *look, const QImage &poster)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(look && look->transparent ? QColor(40, 40, 40) : Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    if (!look || look->showBackground) {
        if (!poster.isNull()) {
            drawBackground(p, img.rect(), bg, poster);
        } else if (bg.type == Background::Type::Image) {
            drawBackground(p, img.rect(), bg, QImage(bg.path));
        } else if (bg.type == Background::Type::Video) {
            {
                p.fillRect(img.rect(), QColor(25, 30, 45));
                p.setPen(QColor(120, 130, 160));
                p.drawText(img.rect().adjusted(4, 4, -4, -4), Qt::AlignRight | Qt::AlignBottom, QStringLiteral("▶"));
            }
        } else {
            drawBackground(p, img.rect(), bg, QImage());
        }
    }

    if (look && !look->showText) return img;
    const double scale = size.height() / 1080.0;
    for (const TextBox &box : slide.boxes) {
        const QRectF r = effectiveRect(box, look);
        const QRect px(qRound(r.x() * size.width()), qRound(r.y() * size.height()),
                       qRound(r.width() * size.width()), qRound(r.height() * size.height()));
        if (look && box.isMain && box.isText() && look->barColor.alpha() > 0 && !box.text.trimmed().isEmpty())
            p.fillRect(px, look->barColor);
        p.drawImage(px.topLeft(), renderBox(box, effectiveStyle(box, look), px.size(), scale));
    }
    return img;
}

}
