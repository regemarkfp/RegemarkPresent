#include "bible/Scripture.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

namespace Scripture {

static QStringList greedy(const QStringList &words, int limit)
{
    QStringList pieces;
    QString cur;
    for (const QString &w : words) {
        if (!cur.isEmpty() && cur.size() + 1 + w.size() > limit) {
            pieces << cur;
            cur.clear();
        }
        cur += cur.isEmpty() ? w : ' ' + w;
    }
    if (!cur.isEmpty()) pieces << cur;
    return pieces;
}

QStringList splitText(const QString &text, int maxChars)
{
    const QString t = text.simplified();
    if (maxChars <= 0 || t.size() <= maxChars) return {t};
    const QStringList words = t.split(' ', Qt::SkipEmptyParts);
    const int parts = int(std::ceil(double(t.size()) / maxChars));
    // Smallest even-ish limit that still needs only `parts` slides, so a
    // 230-char verse becomes 115+115 rather than 220+10.
    for (int limit = int(std::ceil(double(t.size()) / parts)); limit <= maxChars; limit += 4) {
        const QStringList p = greedy(words, limit);
        if (p.size() <= parts) return p;
    }
    return greedy(words, maxChars);
}

TextStyle verseStyle(const TextStyle &base)
{
    TextStyle s = base;
    s.hAlign = Qt::AlignLeft;
    s.fontSize = 64;
    s.uppercase = false;
    return s;
}

static TextStyle referenceStyle(const TextStyle &verse)
{
    TextStyle s = verse;
    s.fontSize = 40;
    s.bold = false;
    s.italic = false;
    s.hAlign = Qt::AlignRight;
    s.vAlign = Qt::AlignVCenter;
    s.outlineWidth = 0;
    s.shadowBlur = 4;
    s.shadowOffset = QPointF(2, 2);
    s.shadowColor = QColor(0, 0, 0, 150);
    return s;
}

namespace {
struct Segment {
    Verse verse;
    QString text;
    bool number = false;
};
}

Presentation toPresentation(const QList<Verse> &verses, const Bible &bible,
                            const Options &opt, const Presentation &templ)
{
    Presentation p = templ;
    p.id = newId();
    p.slides.clear();
    p.author.clear();
    p.ccliNumber.clear();
    p.copyright = bible.rights();
    p.category = QStringLiteral("Scripture");
    p.defaultStyle = verseStyle(templ.defaultStyle);
    const QString abbr = opt.translation.isEmpty() ? bible.abbreviation() : opt.translation;
    const QString whole = Reference::fromVerses(verses, &bible).format(&bible);
    p.title = abbr.isEmpty() ? whole : QStringLiteral("%1 (%2)").arg(whole, abbr);

    // Group verse pieces into slides.
    QList<QList<Segment>> slides;
    QList<Segment> cur;
    int curLen = 0;
    auto flush = [&] {
        if (!cur.isEmpty()) slides.append(cur);
        cur.clear();
        curLen = 0;
    };
    for (const Verse &v : verses) {
        const QStringList pieces = splitText(v.text, opt.maxChars);
        const int numLen = opt.verseNumbers ? QString::number(v.verse).size() + 1 : 0;
        const int len = numLen + pieces.first().size();
        const bool fits = opt.combineVerses && pieces.size() == 1
                          && (opt.maxChars <= 0 || curLen + 1 + len <= opt.maxChars);
        if (!fits) flush();
        for (int i = 0; i < pieces.size(); ++i) {
            cur.append({v, pieces[i], opt.verseNumbers && i == 0});
            curLen += (curLen ? 1 : 0) + (i == 0 ? len : int(pieces[i].size()));
            if (pieces.size() > 1 || !opt.combineVerses) flush();
        }
    }
    flush();

    const TextStyle refStyle = referenceStyle(p.defaultStyle);
    const double numberSize = std::round(p.defaultStyle.fontSize * 0.6);
    for (const QList<Segment> &segs : slides) {
        QList<Verse> onSlide;
        QJsonArray runs;
        QString plain;
        for (int i = 0; i < segs.size(); ++i) {
            const Segment &s = segs[i];
            onSlide.append(s.verse);
            if (s.number) {
                const QString n = QString::number(s.verse.verse) + ' ';
                runs.append(QJsonObject{{"t", n}, {"z", numberSize}});
                plain += n;
            }
            const QString t = s.text + (i + 1 < segs.size() ? QStringLiteral(" ") : QString());
            runs.append(QJsonObject{{"t", t}});
            plain += t;
        }
        const QString label = Reference::fromVerses(onSlide, &bible).format(&bible);

        Slide slide;
        slide.id = newId();
        slide.label = label;
        slide.labelColor = kLabelColor;

        TextBox main;
        main.id = newId();
        main.rect = opt.referenceBox ? QRectF(0.06, 0.07, 0.88, 0.76) : QRectF(0.06, 0.08, 0.88, 0.84);
        main.text = plain;
        // One paragraph of runs. Not `QJsonArray{runs}`: MSVC treats that
        // as a copy and GCC as nesting.
        QJsonArray paragraphs;
        paragraphs.append(runs);
        main.runs = paragraphs;
        main.style = p.defaultStyle;
        slide.boxes.append(main);

        if (opt.referenceBox) {
            TextBox ref;
            ref.id = newId();
            ref.rect = QRectF(0.05, 0.86, 0.9, 0.1);
            ref.text = abbr.isEmpty() ? label : label + QStringLiteral(" · ") + abbr;
            ref.style = refStyle;
            ref.isMain = false;
            slide.boxes.append(ref);
        }
        p.slides.append(slide);
    }
    return p;
}

}
