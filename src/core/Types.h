#pragma once

#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>

// Core document model. All geometry is normalised (0..1) against the output
// frame and all font sizes are pixels at 1080 lines, so a slide renders the
// same on a 720p confidence monitor and a 4K LED wall.

enum class FitMode { Fill, Fit, Stretch };

struct Background {
    enum class Type { None, Color, Image, Video };
    Type type = Type::None;
    QColor color = Qt::black;
    QString path;
    FitMode fit = FitMode::Fill;
    bool loop = true;
    bool audio = false;              // play the video's sound (media cues)

    bool isSet() const { return type != Type::None; }
    bool operator==(const Background &o) const;
    bool operator!=(const Background &o) const { return !(*this == o); }
    QString describe() const;
    QJsonObject toJson() const;
    static Background fromJson(const QJsonObject &o);
    static Background solid(const QColor &c);
    static Background media(const QString &path);
};

enum class TransitionType { Cut, Fade, SlideLeft, SlideUp, Zoom };

struct Transition {
    TransitionType type = TransitionType::Fade;
    int durationMs = 400;

    QJsonObject toJson() const;
    static Transition fromJson(const QJsonObject &o);
    static QStringList names();
};

enum class BuildAnimation { None, Fade, Rise, Zoom, Wipe };

struct TextStyle {
    QString fontFamily = QStringLiteral("Arial");
    double fontSize = 80;            // px at 1080 lines
    QColor color = Qt::white;
    bool bold = true;
    bool italic = false;
    bool uppercase = false;
    Qt::Alignment hAlign = Qt::AlignHCenter;
    Qt::Alignment vAlign = Qt::AlignVCenter;
    double lineSpacing = 1.1;
    double outlineWidth = 0;         // px at 1080 lines
    QColor outlineColor = Qt::black;
    bool shadow = true;
    QColor shadowColor = QColor(0, 0, 0, 200);
    double shadowBlur = 8;
    QPointF shadowOffset = QPointF(3, 3);
    bool autoFit = true;

    QJsonObject toJson() const;
    static TextStyle fromJson(const QJsonObject &o);
};

// A slide element. Mostly text, but slide decks also place images (logos,
// photos) and shapes (colour panels, bands, cards).
struct TextBox {
    enum class Kind { Text, Image, Shape };
    Kind kind = Kind::Text;
    QString id;
    QRectF rect = QRectF(0.05, 0.1, 0.9, 0.8);
    QString text;                    // plain text, always kept in sync
    QJsonArray runs;                 // rich text, see RichText.h (may be empty)
    TextStyle style;
    BuildAnimation build = BuildAnimation::None;
    int buildDelayMs = 0;
    int buildDurationMs = 500;
    bool isMain = true;              // main lyric box; screen overrides apply to it

    QString imagePath;               // Kind::Image
    FitMode imageFit = FitMode::Fit;
    QColor fill = QColor(162, 89, 255);   // Kind::Shape
    double cornerRadius = 0;         // px at 1080 lines
    double opacity = 1.0;

    bool isText() const { return kind == Kind::Text; }
    QJsonObject toJson() const;
    static TextBox fromJson(const QJsonObject &o);
    static TextBox image(const QString &path);
    static TextBox shape(const QColor &fill);
};

struct Slide {
    QString id;
    QString label;                   // "Verse 1", "Chorus", ...
    QColor labelColor;
    QList<TextBox> boxes;
    Background background;           // unset = carry over from previous slide
    bool hasTransition = false;      // false = use presentation default
    Transition transition;
    QString notes;                   // shown on confidence screens
    int advanceMs = 0;               // > 0: go to the next slide automatically

    QString mainText() const;
    QJsonObject toJson() const;
    static Slide fromJson(const QJsonObject &o);
    static QColor colorForLabel(const QString &label);
};

// A named order of a song's groups, e.g. V1 C V2 C B C C.
struct Arrangement {
    QString name;
    QStringList groups;              // slide labels, may repeat
};

// A named look, like ProPresenter's: what every screen shows while it is
// active (switched live from the toolbar), plus a slide style (text,
// position, background, transition) that can be applied to songs.
struct Look {
    QString id;
    QString name;
    TextStyle style;
    QRectF rect = QRectF(0.05, 0.1, 0.9, 0.8);
    Background background;
    Transition transition;
    // Per-screen settings while this look is live, keyed by screen id
    // (ScreenLook JSON, see ScreenConfig.h). Screens not listed keep their own.
    QHash<QString, QJsonObject> screens;

    QJsonObject toJson() const;
    static Look fromJson(const QJsonObject &o);
};

struct Presentation {
    QString id;
    QString title;
    QString author;
    QString copyright;
    QString ccliNumber;
    QString category = QStringLiteral("Song");
    QList<Slide> slides;
    Background defaultBackground;
    Transition defaultTransition;
    TextStyle defaultStyle;
    bool loop = false;               // after the last slide, go back to the first
    QList<Arrangement> arrangements;
    int activeArrangement = -1;      // -1 = slides as written

    // Group labels in first-appearance order ("Verse 1", "Chorus", ...).
    QStringList groups() const;
    // Slide indices in playing order for the active arrangement.
    QList<int> playOrder() const;
    // A copy with slides laid out in playing order (what the operator runs).
    Presentation arranged(QList<int> *sourceIndex = nullptr) const;
    void applyLook(const Look &look);

    bool isMediaCue() const { return category == QLatin1String("Media"); }

    // ProPresenter-style background builds: a slide shows its own background,
    // or the nearest one set on an earlier slide, or the presentation default.
    Background effectiveBackground(int index) const;
    Transition effectiveTransition(int index) const;
    Slide makeSlide(const QString &label, const QString &text) const;

    QJsonObject toJson() const;
    static Presentation fromJson(const QJsonObject &o);
};

QString newId();
QString colorToString(const QColor &c);
QColor colorFromString(const QString &s, const QColor &fallback = QColor());
