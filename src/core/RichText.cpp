#include "core/RichText.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace RichText {

static QTextCharFormat baseCharFormat(const TextStyle &base, double scale)
{
    QTextCharFormat f;
    QFont font(base.fontFamily);
    font.setPixelSize(qMax(1, qRound(base.fontSize * scale)));
    font.setBold(base.bold);
    font.setItalic(base.italic);
    if (base.uppercase) font.setCapitalization(QFont::AllUppercase);
    f.setFont(font);
    f.setForeground(base.color);
    if (base.outlineWidth > 0) {
        QPen pen(base.outlineColor, base.outlineWidth * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        f.setTextOutline(pen);
    }
    return f;
}

static QTextBlockFormat baseBlockFormat(const TextStyle &base)
{
    QTextBlockFormat b;
    b.setAlignment(base.hAlign);
    b.setLineHeight(base.lineSpacing * 100, QTextBlockFormat::ProportionalHeight);
    return b;
}

void toDocument(QTextDocument *doc, const QString &plain, const QJsonArray &runs,
                const TextStyle &base, double scale)
{
    doc->clear();
    doc->setDocumentMargin(0);
    const QTextCharFormat cf = baseCharFormat(base, scale);
    const QTextBlockFormat bf = baseBlockFormat(base);
    doc->setDefaultFont(cf.font());

    QTextCursor cur(doc);
    cur.setBlockFormat(bf);
    cur.setBlockCharFormat(cf);

    if (runs.isEmpty()) {
        const QStringList lines = plain.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            if (i) cur.insertBlock(bf, cf);
            cur.insertText(lines[i], cf);
        }
        return;
    }

    for (int p = 0; p < runs.size(); ++p) {
        if (p) cur.insertBlock(bf, cf);
        for (const QJsonValue &rv : runs[p].toArray()) {
            const QJsonObject r = rv.toObject();
            QTextCharFormat f = cf;
            if (r.contains("b")) f.setFontWeight(r["b"].toBool() ? QFont::Bold : QFont::Normal);
            if (r.contains("i")) f.setFontItalic(r["i"].toBool());
            if (r.contains("u")) f.setFontUnderline(r["u"].toBool());
            if (r.contains("s")) f.setFontStrikeOut(r["s"].toBool());
            if (r.contains("c")) f.setForeground(colorFromString(r["c"].toString(), base.color));
            if (r.contains("f")) f.setFontFamilies({r["f"].toString()});
            if (r.contains("z")) {
                QFont fnt = f.font();
                fnt.setPixelSize(qMax(1, qRound(r["z"].toDouble() * scale)));
                f.setFont(fnt, QTextCharFormat::FontPropertiesSpecifiedOnly);
            }
            cur.insertText(r["t"].toString(), f);
        }
    }
}

QJsonArray fromDocument(const QTextDocument *doc, const TextStyle &base, double scale)
{
    QJsonArray paragraphs;
    const int basePx = qRound(base.fontSize * scale);
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QJsonArray runs;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat f = frag.charFormat();
            const QFont fnt = f.font();
            QJsonObject r{{"t", frag.text()}};
            if ((fnt.weight() >= QFont::DemiBold) != base.bold) r["b"] = fnt.weight() >= QFont::DemiBold;
            if (fnt.italic() != base.italic) r["i"] = fnt.italic();
            if (fnt.underline()) r["u"] = true;
            if (fnt.strikeOut()) r["s"] = true;
            const QColor c = f.foreground().color();
            if (f.foreground().style() != Qt::NoBrush && c != base.color) r["c"] = colorToString(c);
            if (!fnt.family().isEmpty() && fnt.family() != base.fontFamily) r["f"] = fnt.family();
            if (fnt.pixelSize() > 0 && qAbs(fnt.pixelSize() - basePx) > 1)
                r["z"] = fnt.pixelSize() / scale;
            runs.append(r);
        }
        paragraphs.append(runs);
    }
    return paragraphs;
}

QString plainText(const QJsonArray &runs)
{
    QStringList lines;
    for (const QJsonValue &p : runs) {
        QString line;
        for (const QJsonValue &r : p.toArray()) line += r.toObject()["t"].toString();
        lines << line;
    }
    return lines.join('\n');
}

}
