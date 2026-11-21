#include "bible/Bible.h"

#include "bible/BookNames.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>

static bool is(QStringView name, QStringView want)
{
    return name.compare(want, Qt::CaseInsensitive) == 0;
}

static QString tr(const char *s)
{
    return QCoreApplication::translate("Bible", s);
}

// Leading integer of an attribute; "16-17" (merged verses) -> 16.
static int leadingInt(QStringView s)
{
    int n = 0, i = 0;
    while (i < s.size() && s[i].isSpace()) ++i;
    for (; i < s.size() && s[i].isDigit(); ++i) n = n * 10 + s[i].digitValue();
    return n;
}

// Visible text of a VERS element: inline markup (<gr>, <STYLE>, <sup>, <DIV>)
// keeps its text, footnotes and cross references are dropped.
static QString readVerseText(QXmlStreamReader &xml)
{
    QString out;
    int depth = 1;
    while (depth > 0 && !xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::Characters:
            out += xml.text();
            break;
        case QXmlStreamReader::StartElement: {
            const QStringView n = xml.name();
            if (is(n, u"NOTE") || is(n, u"XREF") || is(n, u"MEDIA") || is(n, u"REMARK")) {
                xml.skipCurrentElement();
                out += ' ';
            } else {
                if (is(n, u"BR")) out += ' ';
                ++depth;
            }
            break;
        }
        case QXmlStreamReader::EndElement:
            --depth;
            break;
        default:
            break;
        }
    }
    out = out.simplified();
    // Several files have "word ," left over from tag boundaries.
    if (out.contains(QLatin1String(" ,")) || out.contains(QLatin1String(" ."))) {
        static const QRegularExpression spaceBefore(QStringLiteral(" +([,.])(?=\\s|$)"));
        out.replace(spaceBefore, QStringLiteral("\\1"));
    }
    return out;
}

static void readInformation(QXmlStreamReader &xml, BibleInfo *info)
{
    while (xml.readNextStartElement()) {
        const QStringView n = xml.name();
        auto text = [&] { return xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified(); };
        if (is(n, u"title")) { const QString t = text(); if (info->name.isEmpty()) info->name = t; }
        else if (is(n, u"language")) info->language = text();
        else if (is(n, u"identifier")) info->identifier = text();
        else if (is(n, u"rights")) info->rights = text();
        else if (is(n, u"description")) info->description = text();
        else xml.skipCurrentElement();
    }
}

// Positions the reader inside <XMLBIBLE> and reads its name attribute.
static bool openRoot(QXmlStreamReader &xml, BibleInfo *info, QString *error)
{
    if (!xml.readNextStartElement() || !is(xml.name(), u"XMLBIBLE")) {
        if (error)
            *error = xml.hasError() ? xml.errorString() : tr("Not a Zefania XML Bible (no <XMLBIBLE> element).");
        return false;
    }
    // biblename keeps characters the title sometimes mangles ("1933/1953" vs "1933_1953").
    info->name = xml.attributes().value(QLatin1String("biblename")).toString().simplified();
    return true;
}

bool Bible::readInfo(const QString &path, BibleInfo *info, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    QXmlStreamReader xml(&f);
    *info = BibleInfo();
    if (!openRoot(xml, info, error)) return false;
    while (xml.readNextStartElement()) {
        if (is(xml.name(), u"INFORMATION")) readInformation(xml, info);
        else break;  // first BIBLEBOOK: header is done
    }
    if (info->name.isEmpty()) info->name = QFileInfo(path).completeBaseName();
    return true;
}

bool Bible::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const bool ok = load(&f, error);
    if (ok && m_info.name.isEmpty()) m_info.name = QFileInfo(path).completeBaseName();
    return ok;
}

bool Bible::load(QIODevice *device, QString *error)
{
    *this = Bible();
    QXmlStreamReader xml(device);
    if (!openRoot(xml, &m_info, error)) return false;

    while (xml.readNextStartElement()) {
        if (is(xml.name(), u"INFORMATION")) {
            readInformation(xml, &m_info);
            continue;
        }
        if (!is(xml.name(), u"BIBLEBOOK")) {
            xml.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes a = xml.attributes();
        const QString bname = a.value(QLatin1String("bname")).toString().simplified();
        const QString bsname = a.value(QLatin1String("bsname")).toString().simplified();
        int number = leadingInt(a.value(QLatin1String("bnumber")));
        if (number <= 0) number = BookNames::match(bname);
        if (number <= 0) {
            xml.skipCurrentElement();
            continue;
        }
        Book &book = m_books[number];
        book.info.number = number;
        if (book.info.fileName.isEmpty()) book.info.fileName = bname;
        if (book.info.shortName.isEmpty()) book.info.shortName = bsname;

        while (xml.readNextStartElement()) {
            if (!is(xml.name(), u"CHAPTER")) {
                xml.skipCurrentElement();
                continue;
            }
            const int ch = leadingInt(xml.attributes().value(QLatin1String("cnumber")));
            QList<Verse> &verses = book.chapters[qMax(1, ch)];
            while (xml.readNextStartElement()) {
                if (!is(xml.name(), u"VERS")) {
                    xml.skipCurrentElement();  // CAPTION, PROLOG, REMARK, ...
                    continue;
                }
                Verse v;
                v.book = number;
                v.chapter = qMax(1, ch);
                v.verse = leadingInt(xml.attributes().value(QLatin1String("vnumber")));
                v.text = readVerseText(xml);
                if (v.verse > 0 && !v.text.isEmpty()) verses.append(v);
            }
        }
    }
    if (xml.hasError()) {
        if (error) *error = tr("XML error at line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        *this = Bible();
        return false;
    }

    // Some files number a canonical book out of range (the 1933/1953 Afrikaans
    // file has Psalms as 89); move it home when its name says where it belongs.
    for (const int n : m_books.keys()) {
        if (n >= 1 && n <= BookNames::canonicalCount()) continue;
        const int home = BookNames::match(m_books[n].info.fileName);
        if (home >= 1 && home <= BookNames::canonicalCount() && !m_books.contains(home)) {
            Book b = m_books.take(n);
            b.info.number = home;
            for (auto &ch : b.chapters)
                for (Verse &v : ch) v.book = home;
            m_books.insert(home, b);
        }
    }

    for (auto it = m_books.begin(); it != m_books.end();) {
        // Drop empty chapters/books so chapterCount() and pickers stay honest.
        for (auto c = it->chapters.begin(); c != it->chapters.end();)
            c = c->isEmpty() ? it->chapters.erase(c) : std::next(c);
        if (it->chapters.isEmpty()) {
            it = m_books.erase(it);
            continue;
        }
        it->info.chapters = it->chapters.lastKey();
        for (const auto &c : it->chapters) m_verseTotal += c.size();
        ++it;
    }
    for (auto it = m_books.begin(); it != m_books.end(); ++it) it->info.name = bookName(it.key());

    if (m_books.isEmpty()) {
        if (error) *error = tr("The file contains no Bible books.");
        return false;
    }
    return true;
}

QString Bible::abbreviationFor(const QString &identifier, const QString &name)
{
    QString id = identifier.trimmed();
    if (id.startsWith(QLatin1String("BIBLE_"), Qt::CaseInsensitive)) id = id.mid(6);
    if (id.compare(QLatin1String("AFR3353"), Qt::CaseInsensitive) == 0) return QStringLiteral("AFR53");
    if (!id.isEmpty() && id.size() <= 10 && !id.contains(' ')) return id.toUpper();
    // Initials of the name: "King James Version" -> "KJV".
    QString abbr;
    for (const QString &w : name.split(QRegularExpression(QStringLiteral("[\\s_/-]+")), Qt::SkipEmptyParts)) {
        if (w[0].isDigit()) abbr += w.right(2);
        else if (w[0].isLetter()) abbr += w[0].toUpper();
    }
    return abbr.left(8);
}

QString Bible::abbreviation() const
{
    return abbreviationFor(m_info.identifier, m_info.name);
}

QList<BookInfo> Bible::books() const
{
    QList<BookInfo> list;
    list.reserve(m_books.size());
    for (const Book &b : m_books) list.append(b.info);
    return list;
}

QString Bible::bookName(int book) const
{
    const QString lang = BookNames::canonicalLanguage(m_info.language);
    if (lang == "ENG" || lang == "AFR") {
        const QString n = lang == "AFR" ? BookNames::afrikaans(book) : BookNames::english(book);
        if (!n.isEmpty()) return n;
    }
    const auto it = m_books.constFind(book);
    if (it != m_books.cend() && !it->info.fileName.isEmpty()) return it->info.fileName;
    return BookNames::standard(book, QStringLiteral("ENG"));
}

int Bible::chapterCount(int book) const
{
    const auto it = m_books.constFind(book);
    return it == m_books.cend() ? 0 : it->info.chapters;
}

int Bible::verseCount(int book, int chapter) const
{
    const QList<Verse> v = this->chapter(book, chapter);
    return v.isEmpty() ? 0 : v.last().verse;
}

QList<Verse> Bible::chapter(int book, int chapter) const
{
    const auto it = m_books.constFind(book);
    return it == m_books.cend() ? QList<Verse>() : it->chapters.value(chapter);
}

Verse Bible::verse(int book, int chapter, int verse) const
{
    for (const Verse &v : this->chapter(book, chapter))
        if (v.verse == verse) return v;
    return {};
}

QList<Verse> Bible::range(const Reference &ref) const
{
    QList<Verse> out;
    const auto it = m_books.constFind(ref.book);
    if (it == m_books.cend()) return out;
    for (const VerseRange &r : ref.ranges) {
        for (auto c = it->chapters.lowerBound(r.chapter); c != it->chapters.cend() && c.key() <= r.chapterEnd; ++c)
            for (const Verse &v : c.value())
                if (r.contains(v.chapter, v.verse)) out.append(v);
    }
    return out;
}
