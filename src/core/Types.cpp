#include "core/Types.h"

#include <QFileInfo>
#include <QUuid>

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString colorToString(const QColor &c)
{
    return c.isValid() ? c.name(QColor::HexArgb) : QString();
}

QColor colorFromString(const QString &s, const QColor &fallback)
{
    QColor c(s);
    return c.isValid() ? c : fallback;
}

static QString alignToString(Qt::Alignment a)
{
    if (a & Qt::AlignLeft) return "left";
    if (a & Qt::AlignRight) return "right";
    if (a & Qt::AlignTop) return "top";
    if (a & Qt::AlignBottom) return "bottom";
    return "center";
}

static Qt::Alignment alignFromString(const QString &s, bool horizontal)
{
    if (s == "left") return Qt::AlignLeft;
    if (s == "right") return Qt::AlignRight;
    if (s == "top") return Qt::AlignTop;
    if (s == "bottom") return Qt::AlignBottom;
    return horizontal ? Qt::AlignHCenter : Qt::AlignVCenter;
}

// ---- Background -----------------------------------------------------------

bool Background::operator==(const Background &o) const
{
    if (type != o.type) return false;
    switch (type) {
    case Type::None: return true;
    case Type::Color: return color == o.color;
    default: return path == o.path && fit == o.fit && loop == o.loop;
    }
}

QString Background::describe() const
{
    switch (type) {
    case Type::None: return QObject::tr("(inherit)");
    case Type::Color: return color.name();
    default: return QFileInfo(path).fileName();
    }
}

QJsonObject Background::toJson() const
{
    static const char *types[] = {"none", "color", "image", "video"};
    QJsonObject o{{"type", types[int(type)]}};
    if (type == Type::Color) o["color"] = colorToString(color);
    if (type == Type::Image || type == Type::Video) {
        o["path"] = path;
        o["fit"] = int(fit);
        o["loop"] = loop;
    }
    return o;
}

Background Background::fromJson(const QJsonObject &o)
{
    Background b;
    const QString t = o["type"].toString();
    if (t == "color") b.type = Type::Color;
    else if (t == "image") b.type = Type::Image;
    else if (t == "video") b.type = Type::Video;
    b.color = colorFromString(o["color"].toString(), Qt::black);
    b.path = o["path"].toString();
    b.fit = FitMode(o["fit"].toInt(0));
    b.loop = o["loop"].toBool(true);
    return b;
}

Background Background::solid(const QColor &c)
{
    Background b;
    b.type = Type::Color;
    b.color = c;
    return b;
}

Background Background::media(const QString &path)
{
    static const QStringList videoExt = {"mp4", "mov", "m4v", "mkv", "avi", "webm", "wmv", "mpg", "mpeg"};
    Background b;
    b.path = path;
    b.type = videoExt.contains(QFileInfo(path).suffix().toLower()) ? Type::Video : Type::Image;
    return b;
}

// ---- Transition -----------------------------------------------------------

QStringList Transition::names()
{
    return {QObject::tr("Cut"), QObject::tr("Fade"), QObject::tr("Slide left"),
            QObject::tr("Slide up"), QObject::tr("Zoom")};
}

QJsonObject Transition::toJson() const
{
    return {{"type", int(type)}, {"duration", durationMs}};
}

Transition Transition::fromJson(const QJsonObject &o)
{
    Transition t;
    t.type = TransitionType(qBound(0, o["type"].toInt(1), 4));
    t.durationMs = o["duration"].toInt(400);
    return t;
}

// ---- TextStyle ------------------------------------------------------------

QJsonObject TextStyle::toJson() const
{
    return {
        {"font", fontFamily}, {"size", fontSize}, {"color", colorToString(color)},
        {"bold", bold}, {"italic", italic}, {"uppercase", uppercase},
        {"hAlign", alignToString(hAlign)}, {"vAlign", alignToString(vAlign)},
        {"lineSpacing", lineSpacing},
        {"outlineWidth", outlineWidth}, {"outlineColor", colorToString(outlineColor)},
        {"shadow", shadow}, {"shadowColor", colorToString(shadowColor)},
        {"shadowBlur", shadowBlur}, {"shadowX", shadowOffset.x()}, {"shadowY", shadowOffset.y()},
        {"autoFit", autoFit},
    };
}

TextStyle TextStyle::fromJson(const QJsonObject &o)
{
    TextStyle s;
    s.fontFamily = o["font"].toString(s.fontFamily);
    s.fontSize = o["size"].toDouble(s.fontSize);
    s.color = colorFromString(o["color"].toString(), s.color);
    s.bold = o["bold"].toBool(s.bold);
    s.italic = o["italic"].toBool(s.italic);
    s.uppercase = o["uppercase"].toBool(s.uppercase);
    s.hAlign = alignFromString(o["hAlign"].toString(), true);
    s.vAlign = alignFromString(o["vAlign"].toString(), false);
    s.lineSpacing = o["lineSpacing"].toDouble(s.lineSpacing);
    s.outlineWidth = o["outlineWidth"].toDouble(s.outlineWidth);
    s.outlineColor = colorFromString(o["outlineColor"].toString(), s.outlineColor);
    s.shadow = o["shadow"].toBool(s.shadow);
    s.shadowColor = colorFromString(o["shadowColor"].toString(), s.shadowColor);
    s.shadowBlur = o["shadowBlur"].toDouble(s.shadowBlur);
    s.shadowOffset = QPointF(o["shadowX"].toDouble(3), o["shadowY"].toDouble(3));
    s.autoFit = o["autoFit"].toBool(s.autoFit);
    return s;
}

// ---- TextBox --------------------------------------------------------------

QJsonObject TextBox::toJson() const
{
    QJsonObject o{
        {"id", id},
        {"rect", QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()}},
        {"text", text},
        {"style", style.toJson()},
        {"build", int(build)},
        {"buildDelay", buildDelayMs},
        {"buildDuration", buildDurationMs},
        {"main", isMain},
    };
    if (!runs.isEmpty()) o["runs"] = runs;
    return o;
}

TextBox TextBox::fromJson(const QJsonObject &o)
{
    TextBox b;
    b.id = o["id"].toString(newId());
    const QJsonArray r = o["rect"].toArray();
    if (r.size() == 4)
        b.rect = QRectF(r[0].toDouble(), r[1].toDouble(), r[2].toDouble(), r[3].toDouble());
    b.text = o["text"].toString();
    b.runs = o["runs"].toArray();
    b.style = TextStyle::fromJson(o["style"].toObject());
    b.build = BuildAnimation(qBound(0, o["build"].toInt(0), 4));
    b.buildDelayMs = o["buildDelay"].toInt(0);
    b.buildDurationMs = o["buildDuration"].toInt(500);
    b.isMain = o["main"].toBool(true);
    return b;
}

// ---- Slide ----------------------------------------------------------------

QString Slide::mainText() const
{
    for (const TextBox &b : boxes)
        if (b.isMain) return b.text;
    return boxes.isEmpty() ? QString() : boxes.first().text;
}

QColor Slide::colorForLabel(const QString &label)
{
    const QString l = label.toLower();
    if (l.startsWith("verse")) return QColor("#2f6fdd");
    if (l.startsWith("pre")) return QColor("#d98a1c");
    if (l.startsWith("chorus") || l.startsWith("refrain")) return QColor("#d23c3c");
    if (l.startsWith("bridge")) return QColor("#8a3cd2");
    if (l.startsWith("tag") || l.startsWith("vamp")) return QColor("#1c9e8a");
    if (l.startsWith("intro") || l.startsWith("outro") || l.startsWith("ending")) return QColor("#666666");
    return QColor("#3a3a3a");
}

QJsonObject Slide::toJson() const
{
    QJsonArray b;
    for (const TextBox &box : boxes) b.append(box.toJson());
    QJsonObject o{
        {"id", id}, {"label", label}, {"labelColor", colorToString(labelColor)},
        {"boxes", b}, {"background", background.toJson()}, {"notes", notes},
    };
    if (hasTransition) o["transition"] = transition.toJson();
    return o;
}

Slide Slide::fromJson(const QJsonObject &o)
{
    Slide s;
    s.id = o["id"].toString(newId());
    s.label = o["label"].toString();
    s.labelColor = colorFromString(o["labelColor"].toString(), colorForLabel(s.label));
    for (const QJsonValue &v : o["boxes"].toArray()) s.boxes.append(TextBox::fromJson(v.toObject()));
    s.background = Background::fromJson(o["background"].toObject());
    s.hasTransition = o.contains("transition");
    if (s.hasTransition) s.transition = Transition::fromJson(o["transition"].toObject());
    s.notes = o["notes"].toString();
    return s;
}

// ---- Presentation ---------------------------------------------------------

Background Presentation::effectiveBackground(int index) const
{
    for (int i = qMin(index, int(slides.size()) - 1); i >= 0; --i)
        if (slides[i].background.isSet()) return slides[i].background;
    return defaultBackground;
}

Transition Presentation::effectiveTransition(int index) const
{
    if (index >= 0 && index < slides.size() && slides[index].hasTransition)
        return slides[index].transition;
    return defaultTransition;
}

Slide Presentation::makeSlide(const QString &label, const QString &text) const
{
    Slide s;
    s.id = newId();
    s.label = label;
    s.labelColor = Slide::colorForLabel(label);
    TextBox box;
    box.id = newId();
    box.text = text;
    box.style = defaultStyle;
    s.boxes.append(box);
    return s;
}

QJsonObject Presentation::toJson() const
{
    QJsonArray s;
    for (const Slide &slide : slides) s.append(slide.toJson());
    return {
        {"format", "churchpresenter.presentation"}, {"version", 1},
        {"id", id}, {"title", title}, {"author", author}, {"copyright", copyright},
        {"ccli", ccliNumber}, {"category", category}, {"slides", s},
        {"defaultBackground", defaultBackground.toJson()},
        {"defaultTransition", defaultTransition.toJson()},
        {"defaultStyle", defaultStyle.toJson()},
    };
}

Presentation Presentation::fromJson(const QJsonObject &o)
{
    Presentation p;
    p.id = o["id"].toString(newId());
    p.title = o["title"].toString();
    p.author = o["author"].toString();
    p.copyright = o["copyright"].toString();
    p.ccliNumber = o["ccli"].toString();
    p.category = o["category"].toString("Song");
    for (const QJsonValue &v : o["slides"].toArray()) p.slides.append(Slide::fromJson(v.toObject()));
    p.defaultBackground = Background::fromJson(o["defaultBackground"].toObject());
    p.defaultTransition = Transition::fromJson(o["defaultTransition"].toObject());
    p.defaultStyle = TextStyle::fromJson(o["defaultStyle"].toObject());
    return p;
}
