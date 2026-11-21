// Headless self-test: song importers, model round-trip, and frame rendering
// for each screen role. Writes PNGs to the directory given as argv[1] so the
// renders can be eyeballed. Run with QT_QPA_PLATFORM=offscreen.

#include "core/RichText.h"
#include "core/ScreenConfig.h"
#include "import/SongImporter.h"
#include "render/MediaPool.h"
#include "render/ScreenRenderer.h"

#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLinearGradient>
#include <QPainter>
#include <QTextDocument>
#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

static const char *kSongSelectTxt = R"(Amazing Grace (My Chains Are Gone)

Verse 1
Amazing grace how sweet the sound
That saved a wretch like me
I once was lost but now am found
Was blind but now I see

Verse 2
'Twas grace that taught my heart to fear
And grace my fears relieved

Chorus
My chains are gone
I've been set free
My God my Savior has ransomed me

CCLI Song # 4768151
Chris Tomlin | John Newton | Louie Giglio
© 2006 sixsteps Music
For use solely with the SongSelect® Terms of Use. All rights reserved. www.ccli.com
CCLI License # 123456
)";

static const char *kUsr = "[File]\r\nType=SongSelect Import File\r\nVersion=3.0\r\n[S A4768151]\r\n"
                          "Title=Amazing Grace (My Chains Are Gone)\r\nAuthor=Chris Tomlin | John Newton\r\n"
                          "Copyright=2006 sixsteps Music\r\nFields=Verse 1/tChorus\r\n"
                          "Words=Amazing grace how sweet the sound/nThat saved a wretch like me/tMy chains are gone/nI've been set free\r\n";

static const char *kChordPro = R"({title: Test Song}
{artist: Someone}
{ccli: 999}
{start_of_verse: Verse 1}
[G]Line one [C]here
Line [D]two here
{end_of_verse}
{soc}
Chorus line one
Chorus line two
Chorus line three
{eoc}
)";

static void testImporters()
{
    const auto txt = SongImporter::parseSongSelectText(kSongSelectTxt);
    CHECK(txt.title == "Amazing Grace (My Chains Are Gone)");
    CHECK(txt.ccliNumber == "4768151");
    CHECK(txt.author == "Chris Tomlin, John Newton, Louie Giglio");
    CHECK(txt.copyright.startsWith(QChar(0x00A9)));
    CHECK(txt.sections.size() == 3);
    CHECK(txt.sections.value(2).label == "Chorus");
    CHECK(txt.sections.value(0).lines.size() == 4);

    const auto usr = SongImporter::parseUsr(kUsr);
    CHECK(usr.ccliNumber == "4768151");
    CHECK(usr.sections.size() == 2);
    CHECK(usr.sections.value(1).lines.value(1) == "I've been set free");

    const auto cho = SongImporter::parseChordPro(kChordPro);
    CHECK(cho.title == "Test Song");
    CHECK(cho.ccliNumber == "999");
    CHECK(cho.sections.size() == 2);
    CHECK(cho.sections.value(0).lines.value(0) == "Line one here");
    CHECK(cho.sections.value(1).label == "Chorus");

    const Presentation p = SongImporter::toPresentation(txt, 2, Presentation());
    // Verse 1: 4 lines -> 2 slides, Verse 2: 2 -> 1, Chorus: 3 -> 2
    CHECK(p.slides.size() == 5);
    CHECK(p.slides.value(4).label == "Chorus");
}

static void testModel()
{
    Presentation p = SongImporter::toPresentation(SongImporter::parseSongSelectText(kSongSelectTxt), 2, Presentation());
    p.defaultBackground = Background::solid(Qt::darkBlue);
    p.slides[2].background = Background::media("/x/loop.mp4");
    CHECK(p.effectiveBackground(0).type == Background::Type::Color);
    CHECK(p.effectiveBackground(3).path == "/x/loop.mp4");   // carries forward
    CHECK(p.slides[2].background.type == Background::Type::Video);

    const Presentation back = Presentation::fromJson(p.toJson());
    CHECK(QJsonDocument(back.toJson()).toJson() == QJsonDocument(p.toJson()).toJson());

    const ScreenConfig s = ScreenConfig::defaults(ScreenRole::Stream);
    const ScreenConfig s2 = ScreenConfig::fromJson(s.toJson());
    CHECK(s2.look.transparent && s2.look.overrideRect && s2.role == ScreenRole::Stream);

    // Rich text: emphasis survives a round trip through a document.
    TextStyle base;
    // Built explicitly: `QJsonArray{QJsonArray{...}}` copies on some
    // compilers and nests on others.
    QJsonArray paragraph;
    paragraph.append(QJsonObject{{"t", "Hello "}});
    paragraph.append(QJsonObject{{"t", "world"}, {"c", "#ffffd700"}, {"i", true}});
    QJsonArray runs;
    runs.append(paragraph);
    QTextDocument doc;
    RichText::toDocument(&doc, QString(), runs, base, 0.5);
    const QJsonArray again = RichText::fromDocument(&doc, base, 0.5);
    CHECK(RichText::plainText(again) == "Hello world");
    CHECK(again[0].toArray().size() == 2);
    CHECK(again[0].toArray()[1].toObject()["i"].toBool());
}

static QString makeBackground(const QString &dir)
{
    QImage img(1920, 1080, QImage::Format_RGB32);
    QPainter p(&img);
    QLinearGradient g(0, 0, 1920, 1080);
    g.setColorAt(0, QColor("#1b2a4a"));
    g.setColorAt(1, QColor("#7a3b6e"));
    p.fillRect(img.rect(), g);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 30));
    for (int i = 0; i < 12; ++i) p.drawEllipse(QPoint(150 * i + 60, 300 + (i % 3) * 200), 120, 120);
    const QString path = dir + "/bg.png";
    img.save(path);
    return path;
}

static void testRender(const QString &outDir)
{
    MediaPool pool;
    Presentation p = SongImporter::toPresentation(SongImporter::parseSongSelectText(kSongSelectTxt), 2, Presentation());
    p.defaultBackground = Background::media(makeBackground(outDir));
    p.slides[0].notes = "Band: soft intro, build on line 2";
    p.slides[0].boxes[0].build = BuildAnimation::Rise;
    p.slides[1].boxes[0].runs = QJsonArray{
        QJsonArray{QJsonObject{{"t", "I once was "}}, QJsonObject{{"t", "lost"}, {"c", "#ffffc040"}}, QJsonObject{{"t", " but now am found"}}},
        QJsonArray{QJsonObject{{"t", "Was blind but now I see"}, {"i", true}}}};
    p.slides[1].boxes[0].text = RichText::plainText(p.slides[1].boxes[0].runs);
    p.slides[4].background = Background::solid(QColor("#402010"));

    auto stateFor = [&](int i) {
        LiveState s;
        s.hasSlide = true;
        s.slide = p.slides[i];
        s.hasNext = i + 1 < p.slides.size();
        if (s.hasNext) s.next = p.slides[i + 1];
        s.title = p.title;
        s.index = i;
        s.count = p.slides.size();
        s.background = p.effectiveBackground(i);
        s.transition = p.effectiveTransition(i);
        return s;
    };

    for (ScreenRole role : {ScreenRole::Audience, ScreenRole::Confidence, ScreenRole::Stream}) {
        ScreenRenderer r(&pool);
        ScreenConfig cfg = ScreenConfig::defaults(role);
        r.setConfig(cfg);
        const QString name = QStringList{"audience", "confidence", "stream"}[int(role)];

        r.setState(stateFor(0), 0);
        const QImage mid = r.render(250);    // mid build-in
        const QImage a = r.render(2000);
        r.setState(stateFor(1), 2000);
        const QImage fade = r.render(2200);  // mid crossfade
        const QImage b = r.render(4000);
        r.setState(stateFor(4), 4000);
        const QImage c = r.render(6000);

        CHECK(a.size() == cfg.resolution);
        CHECK(a != b);
        if (role == ScreenRole::Stream) {
            CHECK(qAlpha(a.pixel(10, 10)) == 0);                 // transparent overlay
            CHECK(qAlpha(a.pixel(960, int(1080 * 0.85))) > 0);   // lower-third band
        } else {
            CHECK(qAlpha(a.pixel(10, 10)) == 255);
        }
        mid.save(outDir + "/" + name + "_0_build.png");
        a.save(outDir + "/" + name + "_1.png");
        fade.save(outDir + "/" + name + "_2_fade.png");
        b.save(outDir + "/" + name + "_3.png");
        c.save(outDir + "/" + name + "_4.png");
    }
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::temp().filePath("cp-selftest");
    QDir().mkpath(out);
    testImporters();
    testModel();
    testRender(out);
    std::printf("%s (%d failures), renders in %s\n", failures ? "FAILED" : "OK", failures, qPrintable(out));
    return failures ? 1 : 0;
}
