#include "bible/BibleLibrary.h"

#include "bible/BookNames.h"
#include "bible/BibleCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

static constexpr int kCacheSize = 3;

BibleLibrary::BibleLibrary(QObject *parent)
    : BibleLibrary(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bibles", parent)
{
}

BibleLibrary::BibleLibrary(const QString &root, QObject *parent)
    : QObject(parent), m_root(root)
{
    QDir().mkpath(m_root);
    refresh();
}

QString BibleLibrary::makeId(const QString &text)
{
    QString id = BookNames::stripDiacritics(text).toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id = id.left(60);
    while (id.startsWith('-')) id.remove(0, 1);
    while (id.endsWith('-')) id.chop(1);
    return id.isEmpty() ? QStringLiteral("bible") : id;
}

BibleEntry BibleLibrary::entryFor(const QString &id, const BibleInfo &info, const QString &path) const
{
    BibleEntry e;
    e.id = id;
    e.path = path;
    e.name = QString(info.name).replace('_', ' ').simplified();
    e.languageCode = BibleCatalog::languageCode(info.language);
    e.language = BibleCatalog::languageName(e.languageCode, info.language);
    e.abbreviation = Bible::abbreviationOf(info);
    return e;
}

static QJsonObject toJson(const BibleEntry &e)
{
    return {{"name", e.name}, {"language", e.language}, {"languageCode", e.languageCode},
            {"abbreviation", e.abbreviation}, {"source", e.source}};
}

void BibleLibrary::refresh()
{
    {
        QFile f(m_root + "/index.json");
        m_index = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object()["bibles"].toObject()
                                              : QJsonObject();
    }
    bool dirty = false;
    m_entries.clear();
    const QDir dir(m_root);
    QStringList seen;
    for (const QFileInfo &fi : dir.entryInfoList({"*.xml"}, QDir::Files)) {
        const QString id = fi.completeBaseName();
        seen << id;
        const QJsonObject meta = m_index[id].toObject();
        BibleEntry e;
        if (!meta["name"].toString().isEmpty()) {
            e.id = id;
            e.path = fi.absoluteFilePath();
            e.name = meta["name"].toString();
            e.language = meta["language"].toString();
            e.languageCode = meta["languageCode"].toString();
            e.abbreviation = meta["abbreviation"].toString();
            e.source = meta["source"].toString();
        } else {
            // Dropped in by hand: read just the header.
            BibleInfo info;
            if (!Bible::readInfo(fi.absoluteFilePath(), &info)) continue;
            e = entryFor(id, info, fi.absoluteFilePath());
            m_index[id] = toJson(e);
            dirty = true;
        }
        m_entries.append(e);
    }
    for (const QString &id : m_index.keys())
        if (!seen.contains(id)) {
            m_index.remove(id);
            dirty = true;
        }
    if (dirty) saveIndex();

    // Group by language with English and Afrikaans first, then by name.
    auto rank = [](const BibleEntry &e) { return e.languageCode == "ENG" ? 0 : e.languageCode == "AFR" ? 1 : 2; };
    std::sort(m_entries.begin(), m_entries.end(), [&](const BibleEntry &a, const BibleEntry &b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        if (a.language != b.language) return a.language.localeAwareCompare(b.language) < 0;
        return a.name.localeAwareCompare(b.name) < 0;
    });
    emit biblesChanged();
}

void BibleLibrary::saveIndex()
{
    QSaveFile f(m_root + "/index.json");
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{{"format", "regemarkpresent.bibles"}, {"bibles", m_index}})
                .toJson(QJsonDocument::Indented));
    f.commit();
}

bool BibleLibrary::contains(const QString &id) const
{
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [&](const BibleEntry &e) { return e.id == id; });
}

BibleEntry BibleLibrary::entry(const QString &id) const
{
    for (const BibleEntry &e : m_entries)
        if (e.id == id) return e;
    return {};
}

QString BibleLibrary::importFile(const QString &path, QString *error, const BibleEntry &hint)
{
    auto bible = std::make_shared<Bible>();
    QString err;
    if (!bible->load(path, &err)) {
        if (error) *error = tr("“%1” isn't a usable Zefania or Beblia XML Bible: %2").arg(QFileInfo(path).fileName(), err);
        return {};
    }
    if (!hint.languageCode.isEmpty()) bible->setLanguage(hint.languageCode);  // Beblia files have none
    QString id = makeId(hint.id);
    if (hint.id.isEmpty()) {
        const QString lang = BookNames::canonicalLanguage(bible->language());
        id = !bible->identifier().isEmpty() ? makeId(lang + '-' + bible->identifier() + '-' + bible->name())
                                            : makeId(QFileInfo(path).completeBaseName());
    }

    const QString dest = m_root + '/' + id + ".xml";
    if (QFileInfo(path).canonicalFilePath() != QFileInfo(dest).canonicalFilePath()) {
        QFile::remove(dest);
        if (!QFile::copy(path, dest)) {
            if (error) *error = tr("Couldn't copy the Bible into %1.").arg(m_root);
            return {};
        }
        QFile::setPermissions(dest, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }

    BibleEntry e = entryFor(id, bible->info(), dest);
    if (!hint.name.isEmpty()) e.name = hint.name;
    if (!hint.languageCode.isEmpty()) e.languageCode = hint.languageCode;
    if (!hint.language.isEmpty()) e.language = hint.language;
    else if (!hint.languageCode.isEmpty()) e.language = BibleCatalog::languageName(hint.languageCode);
    if (!hint.abbreviation.isEmpty()) e.abbreviation = hint.abbreviation;
    e.source = hint.source;
    m_index[id] = toJson(e);
    saveIndex();
    cache(id, bible);
    refresh();
    return id;
}

bool BibleLibrary::remove(const QString &id)
{
    const BibleEntry e = entry(id);
    if (e.id.isEmpty()) return false;
    m_cache.erase(std::remove_if(m_cache.begin(), m_cache.end(), [&](const auto &c) { return c.first == id; }),
                  m_cache.end());
    const bool ok = QFile::remove(e.path);
    m_index.remove(id);
    saveIndex();
    refresh();
    return ok;
}

void BibleLibrary::setAbbreviation(const QString &id, const QString &abbreviation)
{
    QJsonObject meta = m_index[id].toObject();
    if (meta.isEmpty()) return;
    meta["abbreviation"] = abbreviation.trimmed();
    m_index[id] = meta;
    saveIndex();
    refresh();
}

void BibleLibrary::cache(const QString &id, const std::shared_ptr<const Bible> &bible)
{
    m_cache.erase(std::remove_if(m_cache.begin(), m_cache.end(), [&](const auto &c) { return c.first == id; }),
                  m_cache.end());
    m_cache.prepend({id, bible});
    while (m_cache.size() > kCacheSize) m_cache.removeLast();
}

std::shared_ptr<const Bible> BibleLibrary::load(const QString &id, QString *error)
{
    for (const auto &c : m_cache)
        if (c.first == id) {
            const auto bible = c.second;
            cache(id, bible);
            return bible;
        }
    const BibleEntry e = entry(id);
    if (e.id.isEmpty()) {
        if (error) *error = tr("Bible “%1” isn't installed.").arg(id);
        return nullptr;
    }
    auto bible = std::make_shared<Bible>();
    if (!bible->load(e.path, error)) return nullptr;
    // The index knows the language of files that don't say (Beblia).
    if (!e.languageCode.isEmpty()) bible->setLanguage(e.languageCode);
    cache(id, bible);
    return bible;
}
