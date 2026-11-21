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

// Positions the reader inside the root element: <XMLBIBLE biblename> (Zefania)
// or <bible translation status link> (Beblia).
static bool openRoot(QXmlStreamReader &xml, BibleInfo *info, QString *error)
{
    if (!xml.readNextStartElement() || !(is(xml.name(), u"XMLBIBLE") || is(xml.name(), u"bible"))) {
        if (error)
            *error = xml.hasError() ? xml.errorString()
                                    : tr("Not a Zefania or Beblia XML Bible (no <XMLBIBLE> or <bible> element).");
        return false;
    }
    const QXmlStreamAttributes a = xml.attributes();
    info->beblia = is(xml.name(), u"bible");
    if (info->beblia) {
        info->name = a.value(QLatin1String("translation")).toString().simplified();
        info->rights = a.value(QLatin1String("status")).toString().simplified();
        info->link = a.value(QLatin1String("link")).toString().trimmed();
    } else {
        // biblename keeps characters the title sometimes mangles ("1933/1953" vs "1933_1953").
        info->name = a.value(QLatin1String("biblename")).toString().simplified();
    }
    return true;
}

bool Bible::bebliaName(const QString &fileName, QString *language, QString *version)
{
    QString base = QFileInfo(fileName).fileName();
    static const QRegularExpression suffix(QStringLiteral("(Bible)?\\.xml$"), QRegularExpression::CaseInsensitiveOption);
    base.remove(suffix);
    static const QRegularExpression head(QStringLiteral("^([A-Z][a-z]+(?:[A-Z][a-z]+)?)(.*)$"));
    const auto m = head.match(base);
    if (!m.hasMatch()) return false;
    QString lang = m.captured(1), ver = m.captured(2);
    // "NorthernSotho" is one language; "AmharicDawro..." is Amharic + a version.
    static const QRegularExpression two(QStringLiteral("^([A-Z][a-z]+)([A-Z][a-z]+)$"));
    static const QStringList joined = {"Sotho", "Ndebele", "Creole", "Malay", "Arabic", "Quechua",
                                       "Nahuatl", "Zapotec", "Mixtec", "Otomi"};
    const auto t = two.match(lang);
    if (t.hasMatch() && !joined.contains(t.captured(2))) {
        lang = t.captured(1);
        ver = t.captured(2) + ver;
    }
    static const QRegularExpression camel(QStringLiteral("([a-z])([A-Z])"));
    static const QRegularExpression camelVer(QStringLiteral("([a-z0-9])([A-Z])"));
    static const QRegularExpression letterDigit(QStringLiteral("([A-Za-z])(\\d)"));
    if (language) *language = lang.replace(camel, QStringLiteral("\\1 \\2"));
    if (version)
        *version = ver.replace(camelVer, QStringLiteral("\\1 \\2")).replace(letterDigit, QStringLiteral("\\1 \\2")).simplified();
    return true;
}

// Beblia has no <language>: take it from a "Afrikaans1983Bible.xml" style name.
static void languageFromFileName(BibleInfo *info, const QString &path)
{
    QString lang;
    if (info->beblia && info->language.isEmpty() && Bible::bebliaName(path, &lang)) info->language = lang;
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
    while (!info->beblia && xml.readNextStartElement()) {
        if (is(xml.name(), u"INFORMATION")) readInformation(xml, info);
        else break;  // first BIBLEBOOK: header is done
    }
    if (info->name.isEmpty()) info->name = QFileInfo(path).completeBaseName();
    languageFromFileName(info, path);
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
    if (ok) languageFromFileName(&m_info, path);
    return ok;
}

bool Bible::load(QIODevice *device, QString *error)
{
    *this = Bible();
    QXmlStreamReader xml(device);
    if (!openRoot(xml, &m_info, error)) return false;

    // Element/attribute names for the two layouts.
    const bool beblia = m_info.beblia;
    const QStringView bookTag = beblia ? u"book" : u"BIBLEBOOK";
    const QLatin1String bookNumber(beblia ? "number" : "bnumber");
    const QStringView chapterTag = beblia ? u"chapter" : u"CHAPTER";
    const QLatin1String chapterNumber(beblia ? "number" : "cnumber");
    const QStringView verseTag = beblia ? u"verse" : u"VERS";
    const QLatin1String verseNumber(beblia ? "number" : "vnumber");

    auto readBook = [&] {
        const QXmlStreamAttributes a = xml.attributes();
        const QString bname = a.value(QLatin1String("bname")).toString().simplified();
        const QString bsname = a.value(QLatin1String("bsname")).toString().simplified();
        int number = leadingInt(a.value(bookNumber));
        if (number <= 0) number = BookNames::match(bname);
        if (number <= 0) {
            xml.skipCurrentElement();
            return;
        }
        Book &book = m_books[number];
        book.info.number = number;
        if (book.info.fileName.isEmpty()) book.info.fileName = bname;
        if (book.info.shortName.isEmpty()) book.info.shortName = bsname;

        while (xml.readNextStartElement()) {
            if (!is(xml.name(), chapterTag)) {
                xml.skipCurrentElement();
                continue;
            }
            const int ch = qMax(1, leadingInt(xml.attributes().value(chapterNumber)));
            QList<Verse> &verses = book.chapters[ch];
            while (xml.readNextStartElement()) {
                if (!is(xml.name(), verseTag)) {
                    xml.skipCurrentElement();  // CAPTION, PROLOG, REMARK, ...
                    continue;
                }
                Verse v;
                v.book = number;
                v.chapter = ch;
                v.verse = leadingInt(xml.attributes().value(verseNumber));
                v.text = readVerseText(xml);
                if (v.verse > 0 && !v.text.isEmpty()) verses.append(v);
            }
        }
    };

    while (xml.readNextStartElement()) {
        if (is(xml.name(), u"INFORMATION")) {
            readInformation(xml, &m_info);
        } else if (beblia && is(xml.name(), u"testament")) {
            while (xml.readNextStartElement()) {
                if (is(xml.name(), bookTag)) readBook();
                else xml.skipCurrentElement();
            }
        } else if (is(xml.name(), bookTag)) {
            readBook();
        } else {
            xml.skipCurrentElement();
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

QString Bible::bebliaAbbreviation(const QString &language, const QString &version)
{
    const QString lang = BookNames::stripDiacritics(language).remove(' ').left(3).toUpper();
    const QStringList words = version.split(' ', Qt::SkipEmptyParts);
    if (words.isEmpty()) return lang;
    static const QRegularExpression year(QStringLiteral("^\\d{4}$"));
    if (words.size() == 1 && year.match(words[0]).hasMatch()) return lang + words[0].right(2);  // "AFR83"
    QString v = words.last().toUpper();
    if (v == QLatin1String("KJ")) v = QStringLiteral("KJV");
    return v.left(8);
}

QString Bible::abbreviationOf(const BibleInfo &info)
{
    if (info.beblia && info.identifier.isEmpty()) {
        // translation="Afrikaans 1983" / "English KJV": first word is the language.
        const int sp = info.name.indexOf(' ');
        return sp > 0 ? bebliaAbbreviation(info.name.left(sp), info.name.mid(sp + 1))
                      : bebliaAbbreviation(info.name, QString());
    }
    return abbreviationFor(info.identifier, info.name);
}

QString Bible::abbreviation() const
{
    return abbreviationOf(m_info);
}

QList<BookInfo> Bible::books() const
{
    QList<BookInfo> list;
    list.reserve(m_books.size());
    for (const Book &b : m_books) {
        list.append(b.info);
        list.last().name = bookName(b.info.number);  // the language may be set after loading
    }
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
