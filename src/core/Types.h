#pragma once

#include <QColor>
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

struct TextBox {
    QString id;
    QRectF rect = QRectF(0.05, 0.1, 0.9, 0.8);
    QString text;                    // plain text, always kept in sync
    QJsonArray runs;                 // rich text, see RichText.h (may be empty)
    TextStyle style;
    BuildAnimation build = BuildAnimation::None;
    int buildDelayMs = 0;
    int buildDurationMs = 500;
    bool isMain = true;              // main lyric box; screen overrides apply to it

    QJsonObject toJson() const;
    static TextBox fromJson(const QJsonObject &o);
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

    QString mainText() const;
    QJsonObject toJson() const;
    static Slide fromJson(const QJsonObject &o);
    static QColor colorForLabel(const QString &label);
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
