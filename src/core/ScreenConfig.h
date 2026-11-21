#pragma once

#include "core/Types.h"

#include <QSize>

// A "screen" is a logical output (Main audience, Confidence, Stream overlay).
// It has its own resolution and look, and can be sent to any number of
// physical targets at once: a monitor/projector, an NDI source, a DeckLink port.

enum class ScreenRole { Audience, Confidence, Stream };

struct OutputTarget {
    enum class Kind { Display, Ndi, DeckLink };
    Kind kind = Kind::Display;
    bool enabled = false;
    QString display;          // QScreen::name()
    QString ndiName;          // NDI source name
    int deckLinkDevice = 0;   // device index
    bool deckLinkKey = false; // use internal keyer (fill + key) for overlays

    QString describe() const;
    QJsonObject toJson() const;
    static OutputTarget fromJson(const QJsonObject &o);
};

struct ScreenLook {
    bool showBackground = true;
    bool showText = true;
    bool transparent = false;         // render alpha (stream overlay / keying)

    bool overrideStyle = false;       // replace the slide's text style on this screen
    TextStyle style;
    bool overrideRect = false;        // move the main text box on this screen
    QRectF rect = QRectF(0.05, 0.1, 0.9, 0.8);
    QColor barColor = QColor(0, 0, 0, 0); // band behind the main text (lower thirds)

    // Confidence / stage display
    bool showNext = true;
    bool showClock = true;
    bool showNotes = true;
    bool showLabel = true;

    QJsonObject toJson() const;
    static ScreenLook fromJson(const QJsonObject &o);
};

struct ScreenConfig {
    QString id;
    QString name;
    ScreenRole role = ScreenRole::Audience;
    QSize resolution = QSize(1920, 1080);
    double fps = 30;
    ScreenLook look;
    QList<OutputTarget> targets;

    double scale() const { return resolution.height() / 1080.0; }
    // This screen's look while `active` is live (its own when the look
    // doesn't customise this screen, or no look is active).
    ScreenLook lookIn(const Look *active) const;
    ScreenConfig withLook(const Look *active) const;
    QJsonObject toJson() const;
    static ScreenConfig fromJson(const QJsonObject &o);
    static ScreenConfig defaults(ScreenRole role);
    static QString roleName(ScreenRole r);
};
