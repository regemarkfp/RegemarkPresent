#include "core/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

QString AppSettings::path()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/settings.json";
}

AppSettings AppSettings::load()
{
    AppSettings s;
    QFile f(path());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &v : o["screens"].toArray()) s.screens.append(ScreenConfig::fromJson(v.toObject()));
        for (const QJsonValue &v : o["mediaFolders"].toArray()) s.mediaFolders.append(v.toString());
        s.linesPerSlide = o["linesPerSlide"].toInt(2);
        s.lastImportDir = o["lastImportDir"].toString();
        s.defaultLookId = o["defaultLookId"].toString();
        s.activeLookId = o["activeLookId"].toString();
        s.wholeRundown = o["wholeRundown"].toBool(false);
    }
    if (s.screens.isEmpty()) {
        s.screens = {ScreenConfig::defaults(ScreenRole::Audience),
                     ScreenConfig::defaults(ScreenRole::Confidence),
                     ScreenConfig::defaults(ScreenRole::Stream)};
        // Save straight away: looks refer to screens by id, so the ids must
        // stay the same from one launch to the next.
        s.save();
    }
    return s;
}

void AppSettings::save() const
{
    QJsonArray sc;
    for (const ScreenConfig &c : screens) sc.append(c.toJson());
    const QJsonObject o{{"screens", sc}, {"mediaFolders", QJsonArray::fromStringList(mediaFolders)},
                        {"linesPerSlide", linesPerSlide}, {"lastImportDir", lastImportDir},
                        {"defaultLookId", defaultLookId}, {"activeLookId", activeLookId},
                        {"wholeRundown", wholeRundown}};
    QSaveFile f(path());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        f.commit();
    }
}
