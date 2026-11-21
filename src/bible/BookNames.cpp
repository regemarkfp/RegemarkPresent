#include "bible/BookNames.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace BookNames {

static const char *const kEnglish[] = {
    "Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy", "Joshua", "Judges", "Ruth",
    "1 Samuel", "2 Samuel", "1 Kings", "2 Kings", "1 Chronicles", "2 Chronicles", "Ezra",
    "Nehemiah", "Esther", "Job", "Psalms", "Proverbs", "Ecclesiastes", "Song of Solomon",
    "Isaiah", "Jeremiah", "Lamentations", "Ezekiel", "Daniel", "Hosea", "Joel", "Amos",
    "Obadiah", "Jonah", "Micah", "Nahum", "Habakkuk", "Zephaniah", "Haggai", "Zechariah",
    "Malachi", "Matthew", "Mark", "Luke", "John", "Acts", "Romans", "1 Corinthians",
    "2 Corinthians", "Galatians", "Ephesians", "Philippians", "Colossians", "1 Thessalonians",
    "2 Thessalonians", "1 Timothy", "2 Timothy", "Titus", "Philemon", "Hebrews", "James",
    "1 Peter", "2 Peter", "1 John", "2 John", "3 John", "Jude", "Revelation",
    // Zefania apocrypha numbering
    "Judith", "Wisdom", "Tobit", "Sirach", "Baruch", "1 Maccabees", "2 Maccabees",
    "Additions to Daniel", "Additions to Esther", "Prayer of Manasseh",
};

static const char *const kAfrikaans[] = {
    "Genesis", "Eksodus", "Levitikus", "Numeri", "Deuteronomium", "Josua", "Rigters", "Rut",
    "1 Samuel", "2 Samuel", "1 Konings", "2 Konings", "1 Kronieke", "2 Kronieke", "Esra",
    "Nehemia", "Ester", "Job", "Psalms", "Spreuke", "Prediker", "Hooglied", "Jesaja",
    "Jeremia", "Klaagliedere", "Esegiël", "Daniël", "Hosea", "Joël", "Amos", "Obadja", "Jona",
    "Miga", "Nahum", "Habakuk", "Sefanja", "Haggai", "Sagaria", "Maleagi", "Matteus", "Markus",
    "Lukas", "Johannes", "Handelinge", "Romeine", "1 Korintiërs", "2 Korintiërs", "Galasiërs",
    "Efesiërs", "Filippense", "Kolossense", "1 Tessalonisense", "2 Tessalonisense",
    "1 Timoteus", "2 Timoteus", "Titus", "Filemon", "Hebreërs", "Jakobus", "1 Petrus",
    "2 Petrus", "1 Johannes", "2 Johannes", "3 Johannes", "Judas", "Openbaring",
};

// Abbreviations (already in key() form). Conventional picks settle the
// prefix clashes: "phil"/"fil" Philippians, "jud" Jude, "jon" Jonah.
static const char *const kAliases[] = {
    /* 1 */ "gen gn ge",
    /* 2 */ "exod exo ex eks",
    /* 3 */ "lev lv le",
    /* 4 */ "num nm nu",
    /* 5 */ "deut deu dt",
    /* 6 */ "josh jos js",
    /* 7 */ "judg jdg jg ri rig",
    /* 8 */ "rut rt",
    /* 9 */ "1sam 1sa 1sm",
    /* 10 */ "2sam 2sa 2sm",
    /* 11 */ "1kgs 1ki 1kg 1kon",
    /* 12 */ "2kgs 2ki 2kg 2kon",
    /* 13 */ "1chr 1ch 1chron 1kron 1kr",
    /* 14 */ "2chr 2ch 2chron 2kron 2kr",
    /* 15 */ "ezr esr",
    /* 16 */ "neh ne",
    /* 17 */ "esth est es",
    /* 18 */ "jb",
    /* 19 */ "ps psa pss psalm psalmen",
    /* 20 */ "prov pro prv pr spr",
    /* 21 */ "eccl ecc ec qoh pred",
    /* 22 */ "song sng sos songofsongs canticles cant hoogl",
    /* 23 */ "isa is jes",
    /* 24 */ "jer je jr",
    /* 25 */ "lam la klaagl",
    /* 26 */ "ezek ezk eze esg eseg",
    /* 27 */ "dan da dn",
    /* 28 */ "hos ho",
    /* 29 */ "jl jol",
    /* 30 */ "am amo",
    /* 31 */ "obad ob oba",
    /* 32 */ "jon jnh",
    /* 33 */ "mic mi mig",
    /* 34 */ "nah na nam",
    /* 35 */ "hab",
    /* 36 */ "zeph zep zp sef",
    /* 37 */ "hag hg",
    /* 38 */ "zech zec zc sag",
    /* 39 */ "mal ml",
    /* 40 */ "matt mat mt",
    /* 41 */ "mrk mk mr",
    /* 42 */ "luk lk lu",
    /* 43 */ "jhn jn joh",
    /* 44 */ "act ac hand",
    /* 45 */ "rom ro rm",
    /* 46 */ "1cor 1co 1kor",
    /* 47 */ "2cor 2co 2kor",
    /* 48 */ "gal ga",
    /* 49 */ "eph ep ef efe",
    /* 50 */ "phil php pp fil flp",
    /* 51 */ "col kol",
    /* 52 */ "1thess 1th 1thes 1tes 1tess",
    /* 53 */ "2thess 2th 2thes 2tes 2tess",
    /* 54 */ "1tim 1ti 1tm",
    /* 55 */ "2tim 2ti 2tm",
    /* 56 */ "tit tt",
    /* 57 */ "phlm phm filem flm",
    /* 58 */ "heb he hebr",
    /* 59 */ "jas jm jam jak",
    /* 60 */ "1pet 1pe 1pt",
    /* 61 */ "2pet 2pe 2pt",
    /* 62 */ "1jn 1jhn 1joh 1jo",
    /* 63 */ "2jn 2jhn 2joh 2jo",
    /* 64 */ "3jn 3jhn 3joh 3jo",
    /* 65 */ "jud jd",
    /* 66 */ "rev re rv op openb apoc revelations",
};

static constexpr int kCanonical = 66;
static constexpr int kEnglishCount = int(sizeof(kEnglish) / sizeof(kEnglish[0]));

QString english(int book)
{
    return book >= 1 && book <= kEnglishCount ? QString::fromLatin1(kEnglish[book - 1]) : QString();
}

QString afrikaans(int book)
{
    return book >= 1 && book <= kCanonical ? QString::fromUtf8(kAfrikaans[book - 1]) : QString();
}

QString standard(int book, const QString &languageCode)
{
    QString n = canonicalLanguage(languageCode) == "AFR" ? afrikaans(book) : QString();
    if (n.isEmpty()) n = english(book);
    return n.isEmpty() ? QStringLiteral("Book %1").arg(book) : n;
}

int canonicalCount() { return kCanonical; }

bool isSingleChapter(int book)
{
    return book == 31 || book == 57 || book == 63 || book == 64 || book == 65;
}

QString stripDiacritics(const QString &s)
{
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d)
        if (c.category() != QChar::Mark_NonSpacing) out += c;
    return out;
}

QString key(const QString &name)
{
    QString s = stripDiacritics(name).toLower().trimmed();
    static const QRegularExpression ordinal(
        QStringLiteral("^(iii|ii|i|1st|2nd|3rd|first|second|third)[\\s.]+(?=[a-z])"));
    const auto m = ordinal.match(s);
    if (m.hasMatch()) {
        const QString o = m.captured(1);
        const char *digit = (o == "i" || o == "1st" || o == "first") ? "1"
                           : (o == "ii" || o == "2nd" || o == "second") ? "2" : "3";
        s = QString::fromLatin1(digit) + s.mid(m.capturedLength());
    }
    QString out;
    out.reserve(s.size());
    for (const QChar c : s)
        if (c.isLetterOrNumber()) out += c;
    return out;
}

QString canonicalLanguage(const QString &language)
{
    const QString l = stripDiacritics(language).trimmed().toLower();
    if (l == "en" || l == "eng" || l.startsWith("english")) return QStringLiteral("ENG");
    if (l == "af" || l == "afr" || l.startsWith("afrikaans")) return QStringLiteral("AFR");
    if (l == "de" || l == "ger" || l == "deu" || l == "german" || l == "deutsch") return QStringLiteral("GER");
    if (l == "nl" || l == "dut" || l == "nld" || l == "dutch") return QStringLiteral("DUT");
    return l.toUpper();
}

struct Table {
    QHash<QString, int> exact;
    QList<QPair<QString, int>> all;  // for prefix matching
};

static const Table &table()
{
    static const Table t = [] {
        Table t;
        auto add = [&t](const QString &k, int book) {
            if (k.isEmpty() || t.exact.contains(k)) return;
            t.exact.insert(k, book);
            t.all.append({k, book});
        };
        for (int b = 1; b <= kCanonical; ++b) {
            add(key(english(b)), b);
            add(key(afrikaans(b)), b);
            for (const QString &a : QString::fromLatin1(kAliases[b - 1]).split(' ', Qt::SkipEmptyParts))
                add(a, b);
        }
        for (int b = kCanonical + 1; b <= kEnglishCount; ++b) add(key(english(b)), b);
        return t;
    }();
    return t;
}

int match(const QString &name, const QList<QPair<int, QString>> &extra, QList<int> *candidates)
{
    const QString k = key(name);
    if (candidates) candidates->clear();
    if (k.isEmpty()) return 0;
    const Table &t = table();
    if (const auto it = t.exact.constFind(k); it != t.exact.cend()) return it.value();
    for (const auto &e : extra)
        if (key(e.second) == k) return e.first;

    // Unique-prefix match over every known spelling.
    QList<int> books;
    auto consider = [&](const QString &candidate, int book) {
        if (candidate.startsWith(k) && !books.contains(book)) books.append(book);
    };
    for (const auto &e : t.all) consider(e.first, e.second);
    for (const auto &e : extra) consider(key(e.second), e.first);
    if (books.size() == 1) return books.first();
    if (candidates) {
        std::sort(books.begin(), books.end());
        *candidates = books;
    }
    return 0;
}

QStringList allNames()
{
    QStringList names;
    QSet<QString> seen;
    for (int b = 1; b <= kCanonical; ++b)
        for (const QString &n : {english(b), afrikaans(b)})
            if (!seen.contains(n)) {
                seen.insert(n);
                names << n;
            }
    return names;
}

}
