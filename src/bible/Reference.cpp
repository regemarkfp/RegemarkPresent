#include "bible/Reference.h"

#include "bible/Bible.h"
#include "bible/BookNames.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

static QString tr(const char *s)
{
    return QCoreApplication::translate("Reference", s);
}

// Anything bigger is a typo, not a reference (Psalm 119 has 176 verses).
static constexpr int kMaxNumber = 200;

bool VerseRange::contains(int ch, int v) const
{
    if (ch < chapter || ch > chapterEnd) return false;
    if (verseStart > 0 && ch == chapter && v < verseStart) return false;
    if (verseEnd > 0 && ch == chapterEnd && v > verseEnd) return false;
    return true;
}

bool Reference::contains(int ch, int v) const
{
    for (const VerseRange &r : ranges)
        if (r.contains(ch, v)) return true;
    return false;
}

static QList<QPair<int, QString>> bibleNames(const Bible *bible)
{
    QList<QPair<int, QString>> names;
    if (!bible) return names;
    for (const BookInfo &b : bible->books()) {
        if (!b.fileName.isEmpty()) names.append({b.number, b.fileName});
        if (!b.shortName.isEmpty()) names.append({b.number, b.shortName});
    }
    return names;
}

Reference Reference::parse(const QString &text, const Bible *bible, QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error) *error = msg;
        return Reference();
    };
    if (error) error->clear();

    QString s = text.simplified();
    for (QChar &c : s)
        if (c == QChar(0x2013) || c == QChar(0x2014) || c == QChar(0x2010) || c == QChar(0x2212)) c = '-';
    if (s.isEmpty()) return fail(tr("Type a reference, e.g. John 3:16"));

    // Book (optionally led by "1", "1." or "1 "), then the numbers.
    static const QRegularExpression split(QStringLiteral("^((?:[1-3]\\s*\\.?\\s*)?[^\\d]*?)\\s*(\\d[^\\p{L}]*)?$"));
    const auto m = split.match(s);
    if (!m.hasMatch()) return fail(tr("Can't read “%1” as a reference.").arg(text.trimmed()));
    const QString bookPart = m.captured(1).trimmed();
    if (!bookPart.contains(QRegularExpression(QStringLiteral("\\p{L}"))))
        return fail(tr("Start with a book name, e.g. John 3:16"));

    Reference ref;
    QList<int> candidates;
    ref.book = BookNames::match(bookPart, bibleNames(bible), &candidates);
    if (ref.book <= 0) {
        if (candidates.isEmpty()) return fail(tr("Unknown book “%1”.").arg(bookPart));
        QStringList names;
        for (int b : candidates.mid(0, 5))
            names << (bible ? bible->bookName(b) : BookNames::english(b));
        if (candidates.size() > 5) names << QStringLiteral("…");
        return fail(tr("“%1” could be %2 — type a few more letters.").arg(bookPart, names.join(", ")));
    }

    QString nums = m.captured(2).trimmed();
    static const QRegularExpression spacedPair(QStringLiteral("^(\\d+)\\s+(\\d+)"));
    if (!nums.contains(':') && !nums.contains('.')) nums.replace(spacedPair, QStringLiteral("\\1:\\2"));  // "John 3 16"
    nums.remove(QRegularExpression(QStringLiteral("\\s+")));
    while (nums.endsWith('.') || nums.endsWith(',') || nums.endsWith(';')) nums.chop(1);

    if (nums.isEmpty()) {  // just a book: its first chapter
        ref.ranges.append({1, 0, 1, 0});
        return ref;
    }

    const bool singleChapter = BookNames::isSingleChapter(ref.book)
                               && (!bible || bible->chapterCount(ref.book) <= 1);
    static const QRegularExpression segRe(QStringLiteral("^(\\d+)(?:[:.](\\d+))?(?:-(\\d+)(?:[:.](\\d+))?)?$"));
    bool verseMode = false;
    int current = 0;
    for (const QString &seg : nums.split(QRegularExpression(QStringLiteral("[,;]")))) {
        const auto sm = segRe.match(seg);
        if (!sm.hasMatch()) return fail(tr("Can't read “%1” in the reference.").arg(seg));
        const int a = sm.captured(1).toInt();
        const bool hasB = sm.capturedLength(2) > 0, hasC = sm.capturedLength(3) > 0, hasD = sm.capturedLength(4) > 0;
        const int b = sm.captured(2).toInt(), c = sm.captured(3).toInt(), d = sm.captured(4).toInt();

        VerseRange r;
        if (hasB) {                       // 3:16, 3:16-18, 1:1-2:3
            r.chapter = a;
            r.verseStart = b;
            r.chapterEnd = hasD ? c : a;
            r.verseEnd = hasD ? d : hasC ? c : b;
            verseMode = true;
        } else if (verseMode || singleChapter) {  // ", 31" after 8:28; "Jude 3"
            if (!verseMode) current = 1;
            verseMode = true;
            r.chapter = current;
            r.verseStart = a;
            r.chapterEnd = hasD ? c : current;
            r.verseEnd = hasD ? d : hasC ? c : a;
        } else {                          // whole chapters: 23, 23-24, 1-2:3
            r.chapter = a;
            r.chapterEnd = hasC ? c : a;
            if (hasD) {
                r.verseStart = 1;
                r.verseEnd = d;
            }
        }
        const bool sane = r.chapter >= 1 && r.chapterEnd >= r.chapter && r.chapterEnd <= kMaxNumber
                          && r.verseEnd <= kMaxNumber
                          && (r.verseStart == 0
                              || (r.verseStart >= 1 && r.verseEnd >= 1
                                  && (r.chapterEnd > r.chapter || r.verseEnd >= r.verseStart)));
        if (!sane) return fail(tr("“%1” isn't a valid chapter/verse range.").arg(seg));
        current = r.chapterEnd;
        ref.ranges.append(r);
    }
    return ref;
}

Reference Reference::fromVerses(const QList<Verse> &verses, const Bible *bible)
{
    Reference ref;
    if (verses.isEmpty()) return ref;
    ref.book = verses.first().book;
    for (const Verse &v : verses) {
        if (v.book != ref.book) continue;
        if (!ref.ranges.isEmpty()) {
            VerseRange &last = ref.ranges.last();
            if (v.chapter == last.chapterEnd && v.verse == last.verseEnd) continue;  // split verse
            const bool next = v.chapter == last.chapterEnd && v.verse == last.verseEnd + 1;
            // Across a chapter break only when the previous chapter really ended.
            const bool nextChapter = bible && v.chapter == last.chapterEnd + 1 && v.verse == 1
                                     && last.verseEnd == bible->verseCount(ref.book, last.chapterEnd);
            if (next || nextChapter) {
                last.chapterEnd = v.chapter;
                last.verseEnd = v.verse;
                continue;
            }
        }
        ref.ranges.append({v.chapter, v.verse, v.chapter, v.verse});
    }
    return ref;
}

QString Reference::format(const Bible *bible) const
{
    if (!isValid()) return QString();
    const QString name = bible ? bible->bookName(book) : BookNames::english(book);
    const QChar dash(0x2013);
    QString out;
    int prevChapter = -1;
    for (const VerseRange &r : ranges) {
        QString s;
        if (r.verseStart == 0) {
            s = QString::number(r.chapter);
            if (r.chapterEnd > r.chapter) s += dash + QString::number(r.chapterEnd);
            if (r.verseEnd > 0) s += ':' + QString::number(r.verseEnd);
            if (!out.isEmpty()) out += QLatin1String("; ");
            prevChapter = -1;
        } else {
            const bool sameChapter = r.chapter == prevChapter;
            s = sameChapter ? QString::number(r.verseStart)
                            : QStringLiteral("%1:%2").arg(r.chapter).arg(r.verseStart);
            if (r.chapterEnd != r.chapter)
                s += dash + QStringLiteral("%1:%2").arg(r.chapterEnd).arg(r.verseEnd);
            else if (r.verseEnd != r.verseStart)
                s += dash + QString::number(r.verseEnd);
            if (!out.isEmpty()) out += sameChapter ? QLatin1String(", ") : QLatin1String("; ");
            prevChapter = r.chapterEnd;
        }
        out += s;
    }
    return name + ' ' + out;
}

QStringList Reference::bookCompletions(const QString &prefix, const Bible *bible)
{
    QStringList names;
    if (bible)
        for (const BookInfo &b : bible->books()) names << b.name;
    names += BookNames::allNames();
    const QString k = BookNames::key(prefix);
    QStringList out;
    QSet<QString> seen;
    for (const QString &n : names) {
        if (seen.contains(n) || !BookNames::key(n).startsWith(k)) continue;
        seen.insert(n);
        out << n;
    }
    return out;
}
