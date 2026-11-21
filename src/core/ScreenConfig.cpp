#include "core/ScreenConfig.h"

QString OutputTarget::describe() const
{
    switch (kind) {
    case Kind::Display: return QObject::tr("Display %1").arg(display);
    case Kind::Ndi: return QObject::tr("NDI \"%1\"").arg(ndiName);
    case Kind::DeckLink: return QObject::tr("DeckLink #%1%2").arg(deckLinkDevice + 1).arg(deckLinkKey ? " (key)" : "");
    }
    return {};
}

QJsonObject OutputTarget::toJson() const
{
    static const char *kinds[] = {"display", "ndi", "decklink"};
    return {{"kind", kinds[int(kind)]}, {"enabled", enabled}, {"display", display},
            {"ndiName", ndiName}, {"deckLinkDevice", deckLinkDevice}, {"deckLinkKey", deckLinkKey}};
}

OutputTarget OutputTarget::fromJson(const QJsonObject &o)
{
    OutputTarget t;
    const QString k = o["kind"].toString();
    t.kind = k == "ndi" ? Kind::Ndi : k == "decklink" ? Kind::DeckLink : Kind::Display;
    t.enabled = o["enabled"].toBool();
    t.display = o["display"].toString();
    t.ndiName = o["ndiName"].toString();
    t.deckLinkDevice = o["deckLinkDevice"].toInt();
    t.deckLinkKey = o["deckLinkKey"].toBool();
    return t;
}

QJsonObject ScreenLook::toJson() const
{
    return {
        {"showBackground", showBackground}, {"showText", showText}, {"transparent", transparent},
        {"overrideStyle", overrideStyle}, {"style", style.toJson()},
        {"overrideRect", overrideRect},
        {"rect", QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()}},
        {"barColor", colorToString(barColor)},
        {"showNext", showNext}, {"showClock", showClock}, {"showNotes", showNotes}, {"showLabel", showLabel},
    };
}

ScreenLook ScreenLook::fromJson(const QJsonObject &o)
{
    ScreenLook l;
    l.showBackground = o["showBackground"].toBool(true);
    l.showText = o["showText"].toBool(true);
    l.transparent = o["transparent"].toBool(false);
    l.overrideStyle = o["overrideStyle"].toBool(false);
    l.style = TextStyle::fromJson(o["style"].toObject());
    l.overrideRect = o["overrideRect"].toBool(false);
    const QJsonArray r = o["rect"].toArray();
    if (r.size() == 4) l.rect = QRectF(r[0].toDouble(), r[1].toDouble(), r[2].toDouble(), r[3].toDouble());
    l.barColor = colorFromString(o["barColor"].toString(), QColor(0, 0, 0, 0));
    l.showNext = o["showNext"].toBool(true);
    l.showClock = o["showClock"].toBool(true);
    l.showNotes = o["showNotes"].toBool(true);
    l.showLabel = o["showLabel"].toBool(true);
    return l;
}

ScreenLook ScreenConfig::lookIn(const Look *active) const
{
    if (active && active->screens.contains(id)) return ScreenLook::fromJson(active->screens.value(id));
    return look;
}

ScreenConfig ScreenConfig::withLook(const Look *active) const
{
    ScreenConfig c = *this;
    c.look = lookIn(active);
    return c;
}

QString ScreenConfig::roleName(ScreenRole r)
{
    switch (r) {
    case ScreenRole::Audience: return QObject::tr("Audience");
    case ScreenRole::Confidence: return QObject::tr("Confidence");
    case ScreenRole::Stream: return QObject::tr("Stream overlay");
    }
    return {};
}

QJsonObject ScreenConfig::toJson() const
{
    static const char *roles[] = {"audience", "confidence", "stream"};
    QJsonArray t;
    for (const OutputTarget &target : targets) t.append(target.toJson());
    return {{"id", id}, {"name", name}, {"role", roles[int(role)]},
            {"width", resolution.width()}, {"height", resolution.height()}, {"fps", fps},
            {"look", look.toJson()}, {"targets", t}};
}

ScreenConfig ScreenConfig::fromJson(const QJsonObject &o)
{
    ScreenConfig s;
    s.id = o["id"].toString(newId());
    s.name = o["name"].toString();
    const QString r = o["role"].toString();
    s.role = r == "confidence" ? ScreenRole::Confidence : r == "stream" ? ScreenRole::Stream : ScreenRole::Audience;
    s.resolution = QSize(o["width"].toInt(1920), o["height"].toInt(1080));
    s.fps = o["fps"].toDouble(30);
    s.look = ScreenLook::fromJson(o["look"].toObject());
    for (const QJsonValue &v : o["targets"].toArray()) s.targets.append(OutputTarget::fromJson(v.toObject()));
    return s;
}

ScreenConfig ScreenConfig::defaults(ScreenRole role)
{
    ScreenConfig s;
    s.id = newId();
    s.role = role;
    s.name = roleName(role);

    OutputTarget display;
    display.kind = OutputTarget::Kind::Display;
    OutputTarget ndi;
    ndi.kind = OutputTarget::Kind::Ndi;
    ndi.ndiName = QStringLiteral("Regemark Present %1").arg(s.name);
    OutputTarget dl;
    dl.kind = OutputTarget::Kind::DeckLink;
    s.targets = {display, ndi, dl};

    switch (role) {
    case ScreenRole::Audience:
        break;
    case ScreenRole::Confidence:
        s.look.showBackground = false;
        break;
    case ScreenRole::Stream:
        s.look.showBackground = false;
        s.look.transparent = true;
        s.look.overrideRect = true;
        s.look.rect = QRectF(0.08, 0.76, 0.84, 0.18);
        s.look.overrideStyle = true;
        s.look.style.fontSize = 52;
        s.look.style.shadow = false;
        s.look.style.lineSpacing = 1.0;
        s.look.barColor = QColor(0, 0, 0, 170);
        s.targets[2].deckLinkKey = true;
        break;
    }
    return s;
}
