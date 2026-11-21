// Headless Bible tests: Zefania loading and text cleanup, reference parsing
// (English + Afrikaans), slide packing, catalog path parsing and the panel.
// No network access. Run with QT_QPA_PLATFORM=offscreen.

#include "bible/Bible.h"
#include "bible/BibleLibrary.h"
#include "bible/BiblePanel.h"
#include "bible/BookNames.h"
#include "bible/Reference.h"
#include "bible/Scripture.h"
#include "bible/ZefaniaCatalog.h"
#include "core/RichText.h"

#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>
#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++failures;                                                                \
        }                                                                              \
    } while (0)
#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        const auto va = (a);                                                           \
        const auto vb = (b);                                                           \
        if (!(va == vb)) {                                                             \
            std::fprintf(stderr, "FAIL %s:%d: %s == %s\n  got:      %s\n  expected: %s\n", \
                         __FILE__, __LINE__, #a, #b, qPrintable(QVariant(va).toString()), \
                         qPrintable(QVariant(vb).toString()));                         \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

static const char *kLongVerse =
    "And on the seventh day God ended his work which he had made; and he rested on the seventh day "
    "from all his work which he had made, and the morning and the evening were the seventh day, and "
    "it was very good, and the heavens and the earth were finished, and all the host of them, every "
    "living creature that moveth, and every herb bearing seed which is upon the face of all the earth, "
    "and every tree in the which is the fruit of a tree yielding seed.";

static const QByteArray kEnglishXml = QByteArray(R"(<?xml version="1.0" encoding="utf-8"?>
<XMLBIBLE xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" biblename="Test Version">
  <INFORMATION>
    <title>Test Version Title</title>
    <language>ENG</language>
    <identifier>TST</identifier>
    <rights>Public domain</rights>
  </INFORMATION>
  <BIBLEBOOK bnumber="1" bname="Genesis" bsname="Gen">
    <CHAPTER cnumber="1">
      <CAPTION>The Creation</CAPTION>
      <VERS vnumber="1"> In the <gr str="7225">beginning </gr><gr str="430">God </gr>created the heaven and the earth. </VERS>
      <VERS vnumber="2">And the earth was without form.</VERS>
      <VERS vnumber="3">And God said, Let there be light.</VERS>
    </CHAPTER>
    <CHAPTER cnumber="2">
      <VERS vnumber="1">Thus the heavens and the earth were finished.</VERS>
      <VERS vnumber="2">)") + kLongVerse + R"(</VERS>
      <VERS vnumber="3">And God blessed the seventh day.</VERS>
    </CHAPTER>
  </BIBLEBOOK>
  <BIBLEBOOK bnumber="43">
    <CHAPTER cnumber="3">
      <VERS vnumber="16">For <gr str="2316">God</gr> so <STYLE css="color:#808080">loved</STYLE> the world,<NOTE type="x-studynote">Greek: kosmos, see <i>note</i></NOTE> that he gave<BR/>his only begotten <sup>Son</sup> , that whosoever believeth in him should not perish, but have everlasting life.</VERS>
      <VERS vnumber="17">For God sent not his Son into the world to condemn the world; but that the world through him might be saved.</VERS>
      <VERS vnumber="18">He that believeth on him is not condemned.</VERS>
    </CHAPTER>
  </BIBLEBOOK>
  <BIBLEBOOK bnumber="89" bname="Psalms" bsname="Ps">
    <CHAPTER cnumber="23">
      <VERS vnumber="1">The LORD is my shepherd; I shall not want.</VERS>
      <VERS vnumber="2">He maketh me to lie down in green pastures.</VERS>
    </CHAPTER>
  </BIBLEBOOK>
  <BIBLEBOOK bnumber="65" bname="Jude" bsname="Jude">
    <CHAPTER cnumber="1">
      <VERS vnumber="3">Beloved, when I gave all diligence.</VERS>
    </CHAPTER>
  </BIBLEBOOK>
</XMLBIBLE>
)";

static const char *kAfrikaansXml = R"(<?xml version="1.0" encoding="utf-8"?>
<XMLBIBLE biblename="1933/1953 Afrikaans Bybel">
  <INFORMATION><title>1933_1953 Afrikaans Bybel</title><language>AFR</language><identifier>AFR3353</identifier></INFORMATION>
  <BIBLEBOOK bname="John" bnumber="43" bsname="John">
    <CHAPTER cnumber="3">
      <VERS vnumber="16">Want so lief het God die wêreld gehad, dat Hy sy eniggebore Seun gegee het.</VERS>
    </CHAPTER>
  </BIBLEBOOK>
</XMLBIBLE>
)";

static const char *kNoNamesXml = R"(<?xml version="1.0"?>
<XMLBIBLE><BIBLEBOOK bnumber="43"><CHAPTER cnumber="1"><VERS vnumber="1">Im Anfang war das Wort.</VERS></CHAPTER></BIBLEBOOK>
<BIBLEBOOK bnumber="19" bname="Psalmen"><CHAPTER cnumber="1"><VERS vnumber="1">Wohl dem.</VERS></CHAPTER></BIBLEBOOK></XMLBIBLE>
)";

static QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &data)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(data);
    return path;
}

static void testLoader(const QTemporaryDir &dir)
{
    Bible b;
    QString error;
    CHECK(b.load(writeFile(dir, "en.xml", kEnglishXml), &error));
    CHECK(error.isEmpty());
    CHECK_EQ(b.name(), QString("Test Version"));
    CHECK_EQ(b.language(), QString("ENG"));
    CHECK_EQ(b.abbreviation(), QString("TST"));
    CHECK_EQ(b.rights(), QString("Public domain"));
    CHECK_EQ(int(b.books().size()), 4);
    CHECK_EQ(b.bookName(43), QString("John"));           // no bname in the file
    CHECK(b.hasBook(19) && !b.hasBook(89));               // Psalms numbered 89 moved home
    CHECK_EQ(b.chapterCount(1), 2);
    CHECK_EQ(b.verseCount(1, 1), 3);
    CHECK_EQ(b.totalVerses(), 12);

    CHECK_EQ(b.verse(1, 1, 1).text, QString("In the beginning God created the heaven and the earth."));
    CHECK_EQ(b.verse(43, 3, 16).text,
             QString("For God so loved the world, that he gave his only begotten Son, that whosoever "
                     "believeth in him should not perish, but have everlasting life."));
    CHECK(!b.verse(1, 1, 1).text.contains("Creation"));   // caption is not verse text

    const QList<Verse> r = b.range(Reference::parse("Gen 1:2-2:1"));
    CHECK_EQ(int(r.size()), 3);
    CHECK(r.size() == 3 && r[0].verse == 2 && r[2].chapter == 2 && r[2].verse == 1);
    CHECK_EQ(int(b.range(Reference::parse("John 3:16, 18")).size()), 2);
    CHECK_EQ(int(b.range(Reference::parse("Ps 23")).size()), 2);
    CHECK_EQ(int(b.range(Reference::parse("Jude 3")).size()), 1);
    CHECK(b.range(Reference::parse("Rev 1")).isEmpty());

    BibleInfo info;
    CHECK(Bible::readInfo(dir.filePath("en.xml"), &info));
    CHECK_EQ(info.identifier, QString("TST"));

    Bible afr;
    CHECK(afr.load(writeFile(dir, "afr.xml", kAfrikaansXml)));
    CHECK_EQ(afr.bookName(43), QString("Johannes"));      // file says "John"; language is AFR
    CHECK_EQ(afr.abbreviation(), QString("AFR53"));
    CHECK_EQ(Reference::parse("Joh 3:16", &afr).format(&afr), QString::fromUtf8("Johannes 3:16"));
    CHECK(afr.verse(43, 3, 16).text.contains(QString::fromUtf8("wêreld")));

    Bible plain;
    CHECK(plain.load(writeFile(dir, "plain.xml", kNoNamesXml)));
    CHECK_EQ(plain.bookName(43), QString("John"));
    CHECK_EQ(plain.bookName(19), QString("Psalmen"));
    CHECK_EQ(Reference::parse("Psalmen 1:1", &plain).book, 19);  // the file's own name

    Bible bad;
    CHECK(!bad.load(writeFile(dir, "bad.xml", "<html><body>nope</body></html>"), &error));
    CHECK(!error.isEmpty());
    CHECK(!bad.load(writeFile(dir, "broken.xml", "<XMLBIBLE><BIBLEBOOK bnumber=\"1\"><CHAPTER"), &error));
}

static void checkRef(const char *text, int book, std::initializer_list<VerseRange> ranges, int line)
{
    QString error;
    const Reference r = Reference::parse(QString::fromUtf8(text), nullptr, &error);
    const QList<VerseRange> want(ranges);
    if (r.book != book || r.ranges != want) {
        QStringList got;
        for (const VerseRange &v : r.ranges)
            got << QStringLiteral("{%1,%2,%3,%4}").arg(v.chapter).arg(v.verseStart).arg(v.chapterEnd).arg(v.verseEnd);
        std::fprintf(stderr, "FAIL %s:%d: parse(\"%s\") -> book %d %s (%s)\n", __FILE__, line, text, r.book,
                     qPrintable(got.join(' ')), qPrintable(error));
        ++failures;
    }
}
#define REF(text, book, ...) checkRef(text, book, __VA_ARGS__, __LINE__)

static void checkBad(const char *text, int line)
{
    QString error;
    const Reference r = Reference::parse(QString::fromUtf8(text), nullptr, &error);
    if (r.isValid() || error.isEmpty()) {
        std::fprintf(stderr, "FAIL %s:%d: parse(\"%s\") should fail\n", __FILE__, line, text);
        ++failures;
    }
}
#define BAD(text) checkBad(text, __LINE__)

static void testReferences()
{
    // English
    REF("John 3:16", 43, {{3, 16, 3, 16}});
    REF("Jn 3:16-18", 43, {{3, 16, 3, 18}});
    REF("jhn 3.16", 43, {{3, 16, 3, 16}});
    REF("John 3 16", 43, {{3, 16, 3, 16}});
    REF("1 Cor 13", 46, {{13, 0, 13, 0}});
    REF("1Cor 13:4-7", 46, {{13, 4, 13, 7}});
    REF("I Corinthians 13:4", 46, {{13, 4, 13, 4}});
    REF("2 Tim 3:16", 55, {{3, 16, 3, 16}});
    REF("III John 4", 64, {{1, 4, 1, 4}});
    REF("Ps 23", 19, {{23, 0, 23, 0}});
    REF("Ps. 23-24", 19, {{23, 0, 24, 0}});
    REF("Psalm 119:105", 19, {{119, 105, 119, 105}});
    REF("Gen 1:1-2:3", 1, {{1, 1, 2, 3}});
    REF("Gen 1:1–2:3", 1, {{1, 1, 2, 3}});   // en dash
    REF("Rom 8:28, 31", 45, {{8, 28, 8, 28}, {8, 31, 8, 31}});
    REF("Rom 8:28-30; 9:1", 45, {{8, 28, 8, 30}, {9, 1, 9, 1}});
    REF("Phil 4:13", 50, {{4, 13, 4, 13}});
    REF("Phm 4", 57, {{1, 4, 1, 4}});
    REF("Jud 5", 65, {{1, 5, 1, 5}});
    REF("Judg 6", 7, {{6, 0, 6, 0}});
    REF("Song of Solomon 2:4", 22, {{2, 4, 2, 4}});
    REF("Mt 5:3-12", 40, {{5, 3, 5, 12}});
    REF("Rev 22:21", 66, {{22, 21, 22, 21}});
    REF("Revelation 21", 66, {{21, 0, 21, 0}});
    REF("Isa 40:31", 23, {{40, 31, 40, 31}});
    REF("John", 43, {{1, 0, 1, 0}});
    REF("  jOHN   3 : 16 ", 43, {{3, 16, 3, 16}});
    REF("Ecc 3:1", 21, {{3, 1, 3, 1}});
    REF("Lam 3:22-23", 25, {{3, 22, 3, 23}});
    // Afrikaans
    REF("Johannes 3:16", 43, {{3, 16, 3, 16}});
    REF("Joh. 3:16", 43, {{3, 16, 3, 16}});
    REF("1 Johannes 4:8", 62, {{4, 8, 4, 8}});
    REF("Esegiël 37", 26, {{37, 0, 37, 0}});
    REF("Esegiel 37:1", 26, {{37, 1, 37, 1}});
    REF("Hebreërs 11:1", 58, {{11, 1, 11, 1}});
    REF("Hebreers 11", 58, {{11, 0, 11, 0}});
    REF("Openbaring 21:4", 66, {{21, 4, 21, 4}});
    REF("Spreuke 3:5-6", 20, {{3, 5, 3, 6}});
    REF("Hand 2:38", 44, {{2, 38, 2, 38}});
    REF("Handelinge 2", 44, {{2, 0, 2, 0}});
    REF("Matteus 5", 40, {{5, 0, 5, 0}});
    REF("Klaagliedere 3:22", 25, {{3, 22, 3, 22}});
    REF("2 Kon 2", 12, {{2, 0, 2, 0}});
    REF("1 Kronieke 1", 13, {{1, 0, 1, 0}});
    REF("Fil 4:13", 50, {{4, 13, 4, 13}});
    REF("Filemon 4", 57, {{1, 4, 1, 4}});
    REF("Jak 1:5", 59, {{1, 5, 1, 5}});
    REF("Sag 9:9", 38, {{9, 9, 9, 9}});
    REF("Rigters 6", 7, {{6, 0, 6, 0}});
    REF("Prediker 3:1", 21, {{3, 1, 3, 1}});
    REF("Hooglied 2", 22, {{2, 0, 2, 0}});
    REF("Daniël 3", 27, {{3, 0, 3, 0}});
    REF("Joël 2:28", 29, {{2, 28, 2, 28}});
    REF("Romeine 8:28", 45, {{8, 28, 8, 28}});
    REF("2 Korintiërs 5:17", 47, {{5, 17, 5, 17}});
    REF("Galasiërs 5:22", 48, {{5, 22, 5, 22}});
    REF("1 Tessalonisense 5:16-18", 52, {{5, 16, 5, 18}});
    REF("Judas 24", 65, {{1, 24, 1, 24}});
    REF("Psalms 23", 19, {{23, 0, 23, 0}});
    REF("Eksodus 20", 2, {{20, 0, 20, 0}});
    REF("Maleagi 3:10", 39, {{3, 10, 3, 10}});
    // Unique prefixes
    REF("Deuter 6:4", 5, {{6, 4, 6, 4}});
    REF("Phili 4", 50, {{4, 0, 4, 0}});
    REF("Philem 4", 57, {{1, 4, 1, 4}});

    // Garbage
    BAD("");
    BAD("hello");
    BAD("3:16");
    BAD("Xyz 3:16");
    BAD("Jo 3:16");          // Joshua, Job, Joel, John, Jonah...
    BAD("John 0:1");
    BAD("John 3:18-16");
    BAD("John 3:16abc");
    BAD("John 3:16-");
    BAD("John 999");
    BAD("John 3::16");
    BAD("John 3:16,,17");

    QString error;
    Reference::parse("Jo 3:16", nullptr, &error);
    CHECK(error.contains("John") && error.contains("Job"));

    // Formatting
    CHECK_EQ(Reference::parse("Jn 3:16-18").format(), QString::fromUtf8("John 3:16–18"));
    CHECK_EQ(Reference::parse("Rom 8:28, 31; 9:1").format(), QString::fromUtf8("Romans 8:28, 31; 9:1"));
    CHECK_EQ(Reference::parse("Gen 1:1-2:3").format(), QString::fromUtf8("Genesis 1:1–2:3"));
    CHECK_EQ(Reference::parse("Ps 23").format(), QString("Psalms 23"));
    CHECK_EQ(Reference::parse("Ps 23-24").format(), QString::fromUtf8("Psalms 23–24"));
    CHECK_EQ(Reference::parse("1cor 13:4").format(), QString("1 Corinthians 13:4"));

    QList<Verse> vs;
    for (int v : {16, 17, 18, 20}) vs.append({43, 3, v, QString()});
    CHECK_EQ(Reference::fromVerses(vs).format(), QString::fromUtf8("John 3:16–18, 20"));

    const QStringList c = Reference::bookCompletions("1 c");
    CHECK(c.contains("1 Corinthians") && c.contains("1 Chronicles") && !c.contains("John"));
    CHECK(Reference::bookCompletions("Ese").contains(QString::fromUtf8("Esegiël")));
    CHECK(Reference::bookCompletions().size() > 100);
    CHECK_EQ(BookNames::key(QString::fromUtf8("I Korintiërs")), QString("1korintiers"));
}

static void testScripture(const QTemporaryDir &dir)
{
    Bible b;
    CHECK(b.load(dir.filePath("en.xml")));
    const QList<Verse> john = b.range(Reference::parse("John 3:16-18"));
    CHECK_EQ(int(john.size()), 3);

    Presentation templ;
    templ.defaultBackground = Background::solid(Qt::darkBlue);
    Scripture::Options opt;
    const Presentation p = Scripture::toPresentation(john, b, opt, templ);
    CHECK_EQ(int(p.slides.size()), 3);
    CHECK_EQ(p.category, QString("Scripture"));
    CHECK_EQ(p.title, QString::fromUtf8("John 3:16–18 (TST)"));
    CHECK(p.defaultBackground == templ.defaultBackground);
    CHECK(p.defaultStyle.hAlign == Qt::AlignLeft);
    CHECK_EQ(p.defaultStyle.fontSize, 64.0);
    if (p.slides.size() == 3) {
        const Slide &s = p.slides[0];
        CHECK_EQ(s.label, QString("John 3:16"));
        CHECK_EQ(p.slides[2].label, QString("John 3:18"));
        CHECK(s.labelColor == Scripture::kLabelColor);
        CHECK_EQ(int(s.boxes.size()), 2);
        if (s.boxes.size() == 2) {
            const TextBox &main = s.boxes[0], &ref = s.boxes[1];
            CHECK(main.isMain && !ref.isMain);
            CHECK(main.text.startsWith("16 For God so loved"));
            CHECK_EQ(RichText::plainText(main.runs), main.text);
            const QJsonObject num = main.runs[0].toArray()[0].toObject();
            CHECK_EQ(num["t"].toString(), QString("16 "));
            CHECK(num["z"].toDouble() < main.style.fontSize);
            CHECK_EQ(ref.text, QString::fromUtf8("John 3:16 · TST"));
            CHECK(qAbs(ref.rect.y() - 0.86) < 0.01 && ref.style.fontSize == 40);
            CHECK(main.rect.bottom() <= ref.rect.top());
        }
    }

    // Combined: all three fit in a big budget.
    opt.combineVerses = true;
    opt.maxChars = 1000;
    opt.translation = "KJV";
    const Presentation combined = Scripture::toPresentation(john, b, opt);
    CHECK_EQ(int(combined.slides.size()), 1);
    if (!combined.slides.isEmpty()) {
        CHECK_EQ(combined.slides[0].label, QString::fromUtf8("John 3:16–18"));
        CHECK(combined.slides[0].boxes[0].text.contains(" 17 For God sent"));
        CHECK_EQ(combined.slides[0].boxes[1].text, QString::fromUtf8("John 3:16–18 · KJV"));
    }
    // Combined with a small budget: 16 alone (it's long), 17+18 together.
    opt.maxChars = 200;
    const Presentation packed = Scripture::toPresentation(john, b, opt);
    CHECK_EQ(int(packed.slides.size()), 2);
    if (packed.slides.size() == 2) CHECK_EQ(packed.slides[1].label, QString::fromUtf8("John 3:17–18"));

    // A long verse splits at word boundaries, numbered only on its first part.
    opt = Scripture::Options();
    const Presentation split = Scripture::toPresentation(b.range(Reference::parse("Gen 2:2")), b, opt);
    CHECK_EQ(int(split.slides.size()), 3);
    QStringList parts;
    for (const Slide &s : split.slides) {
        CHECK_EQ(s.label, QString("Genesis 2:2"));
        CHECK(s.boxes[0].text.size() <= opt.maxChars + 2);
        parts << s.boxes[0].text;
    }
    if (split.slides.size() == 3) {
        CHECK(parts[0].startsWith("2 And on the seventh"));
        CHECK(!parts[1].startsWith("2 "));
        CHECK_EQ(parts.join(' ').mid(2), QString::fromLatin1(kLongVerse));
    }
    // Balanced split: 230 chars -> two halves, not 220 + 10.
    const QString text230 = QString("word ").repeated(46).trimmed();
    const QStringList halves = Scripture::splitText(text230, 220);
    CHECK_EQ(int(halves.size()), 2);
    if (halves.size() == 2) CHECK(qAbs(halves[0].size() - halves[1].size()) < 12);
    CHECK_EQ(int(Scripture::splitText("short", 220).size()), 1);
    CHECK_EQ(int(Scripture::splitText(text230, 0).size()), 1);
}

static void testCatalogPaths()
{
    CatalogEntry e;
    CHECK(ZefaniaCatalog::parsePath("Bibles/ENG/King James/KJV+/SF_2009-01-20_ENG_KJV_(KJV+).xml", &e));
    CHECK_EQ(e.languageCode, QString("ENG"));
    CHECK_EQ(e.language, QString("English"));
    CHECK_EQ(e.module, QString("KJV"));
    CHECK_EQ(e.name, QString("KJV+"));
    CHECK_EQ(e.date, QString("2009-01-20"));
    CHECK_EQ(e.downloadUrl().toString(QUrl::FullyEncoded),
             QString("https://raw.githubusercontent.com/kohelet-net-admin/zefania-xml-bibles/master/"
                     "Bibles/ENG/King%20James/KJV%2B/SF_2009-01-20_ENG_KJV_%28KJV%2B%29.xml"));

    CHECK(ZefaniaCatalog::parsePath(
        "Bibles/AFR/1933/1953 Afrikaans Bybel/SF_2009-01-20_AFR_AFR3353_(1933_1953 AFRIKAANS BYBEL).xml", &e));
    CHECK_EQ(e.languageCode, QString("AFR"));
    CHECK_EQ(e.language, QString("Afrikaans"));
    CHECK_EQ(e.module, QString("AFR3353"));
    CHECK_EQ(e.name, QString("1933/1953 Afrikaans Bybel"));
    CHECK_EQ(e.installId(), QString("afr-afr3353-1933-1953-afrikaans-bybel"));

    CHECK(ZefaniaCatalog::parsePath("Bibles/ENG/World English Bible/SF_2009-01-20_ENG_WEB_(WORLD ENGLISH BIBLE).xml", &e));
    CHECK_EQ(e.name, QString("World English Bible"));
    CHECK(ZefaniaCatalog::parsePath("Bibles/ENG/x/SF_2009-01-20_ENG_ISV_(ISV NT).xml", &e));
    CHECK_EQ(e.name, QString("ISV NT"));
    CHECK(ZefaniaCatalog::parsePath(
        "Bibles/GER/Lutherbibel/Luther 1545 (Letzte Hand)/SF_2012-08-14_DEUTSCH_LUT_1545_LH_(LUTHER 1545 (LETZTE HAND)).xml", &e));
    CHECK_EQ(e.languageCode, QString("GER"));
    CHECK_EQ(e.module, QString("LUT_1545_LH"));
    CHECK_EQ(e.name, QString("Luther 1545 (Letzte Hand)"));
    CHECK(ZefaniaCatalog::parsePath(
        "Bibles/TUR/Southern Azeri Translation/SF_2012-01-07_AZB_AZERI_(DESCRIPTION_EN=SOUTHERN AZERI TRANSLATION).xml", &e));
    CHECK_EQ(e.languageCode, QString("AZB"));
    CHECK_EQ(e.name, QString("Southern Azeri Translation"));
    CHECK(ZefaniaCatalog::parsePath(
        "Bibles/ENG/Galatians/SF_25.04.2004_ENG_MARCGAL_(THE EPISTLE TO THE GALATIANS(DETERING)).xml", &e));
    CHECK_EQ(e.date, QString("2004-04-25"));
    CHECK(ZefaniaCatalog::parsePath("Bibles/NL/Dutch Statenvertaling/SF_2009-01-20_NL_DUTV_(DUTCH STATENVERTALING).xml", &e));
    CHECK_EQ(e.language, QString("Dutch"));
    CHECK(ZefaniaCatalog::parsePath("Bibles/CZE/Czech BKR/Czech BKR/sf_czech_bkr.xml", &e));
    CHECK_EQ(e.languageCode, QString("CZE"));
    CHECK_EQ(e.name, QString("Czech BKR"));
    CHECK(ZefaniaCatalog::parsePath(
        "Bibles/CEB/Cebuano/SF_2009-01-20_CEB_CEB-B_(CEBUANO ANG BIBLIA (BUGNA VERSION)).xml", &e));
    CHECK_EQ(e.name, QString("Cebuano Ang Biblia (Bugna Version)"));

    CHECK(!ZefaniaCatalog::parsePath("Dictionaries/GER/x/SF_2006-01-24_GER_RIEG_(X).xml", &e));
    CHECK(!ZefaniaCatalog::parsePath("Bibles/index.csv", &e));
    CHECK(!ZefaniaCatalog::parsePath("README.md", &e));
    CHECK_EQ(ZefaniaCatalog::languageName("xho"), QString("Xhosa"));
    CHECK_EQ(ZefaniaCatalog::languageName("QQQ"), QString("QQQ"));
}

static void testLibraryAndPanel(const QTemporaryDir &dir)
{
    const QString root = dir.filePath("library");
    {
        BibleLibrary lib(root);
        CHECK(lib.installed().isEmpty());
        int changes = 0;
        QObject::connect(&lib, &BibleLibrary::biblesChanged, [&] { ++changes; });
        QString error;
        CHECK(lib.importFile(dir.filePath("bad.xml"), &error).isEmpty());
        CHECK(!error.isEmpty());
        BibleEntry hint;
        hint.name = "Afrikaans 1953";
        const QString afr = lib.importFile(dir.filePath("afr.xml"), &error, hint);
        const QString en = lib.importFile(dir.filePath("en.xml"), &error);
        CHECK(!afr.isEmpty() && !en.isEmpty() && afr != en);
        CHECK(changes >= 2);
        CHECK_EQ(int(lib.installed().size()), 2);
        CHECK_EQ(lib.installed().value(0).languageCode, QString("ENG"));   // English sorts first
        CHECK_EQ(lib.entry(afr).name, QString("Afrikaans 1953"));
        CHECK_EQ(lib.entry(afr).language, QString("Afrikaans"));
        CHECK_EQ(lib.entry(en).abbreviation, QString("TST"));
        const auto b1 = lib.load(en);
        CHECK(b1 && b1->hasBook(43));
        CHECK(lib.load(en) == b1);  // cached
    }
    BibleLibrary lib(root);  // fresh instance reads the index back
    CHECK_EQ(int(lib.installed().size()), 2);
    QString afrId;
    for (const BibleEntry &e : lib.installed())
        if (e.languageCode == "AFR") afrId = e.id;
    CHECK_EQ(lib.entry(afrId).name, QString("Afrikaans 1953"));

    BiblePanel panel(&lib);
    int shown = 0;
    Presentation last;
    QObject::connect(&panel, &BiblePanel::presentationReady, [&](const Presentation &p) {
        ++shown;
        last = p;
    });
    CHECK(panel.currentBible() != nullptr);
    CHECK(panel.showReference("Jn 3:16-17"));
    CHECK_EQ(int(panel.currentPresentation().slides.size()), panel.currentBible()->language() == "AFR" ? 1 : 2);
    CHECK(!panel.showReference("Nonsense 1:1"));

    CHECK(lib.remove(afrId));
    CHECK_EQ(int(lib.installed().size()), 1);
    CHECK(panel.currentBible() != nullptr);   // panel follows the library
    CHECK(lib.remove(lib.installed().value(0).id));
    CHECK(panel.currentBible() == nullptr);
    CHECK(panel.currentPresentation().slides.isEmpty());
    CHECK_EQ(shown, 0);
}

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);  // QSettings/AppData go to a scratch location
    QApplication app(argc, argv);
    QApplication::setOrganizationName("RegemarkTest");
    QApplication::setApplicationName("bibletest");
    QTemporaryDir dir;
    if (!dir.isValid()) return 2;
    testLoader(dir);
    testReferences();
    testScripture(dir);
    testCatalogPaths();
    testLibraryAndPanel(dir);
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
