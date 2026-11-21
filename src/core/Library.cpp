#include "core/Library.h"

#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

static QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

static bool writeJson(const QString &path, const QJsonObject &o)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

QJsonObject Playlist::toJson() const
{
    QJsonArray a;
    for (const PlaylistItem &i : items)
        a.append(i.presentationId.isEmpty() ? QJsonObject{{"header", i.header}}
                                            : QJsonObject{{"presentation", i.presentationId}});
    return {{"format", "churchpresenter.playlist"}, {"id", id}, {"name", name}, {"items", a}};
}

Playlist Playlist::fromJson(const QJsonObject &o)
{
    Playlist p;
    p.id = o["id"].toString(newId());
    p.name = o["name"].toString();
    for (const QJsonValue &v : o["items"].toArray()) {
        const QJsonObject i = v.toObject();
        p.items.append({i["presentation"].toString(), i["header"].toString()});
    }
    return p;
}

Library::Library(const QString &root, QObject *parent)
    : QObject(parent), m_root(root)
{
    QDir().mkpath(m_root + "/Presentations");
    QDir().mkpath(m_root + "/Playlists");
    QDir().mkpath(m_root + "/Media");
}

QString Library::presPath(const QString &id) const { return m_root + "/Presentations/" + id + ".cpres"; }
QString Library::playlistPath(const QString &id) const { return m_root + "/Playlists/" + id + ".cplist"; }

void Library::load()
{
    m_presentations.clear();
    const QDir pd(m_root + "/Presentations");
    for (const QString &f : pd.entryList({"*.cpres"}, QDir::Files)) {
        Presentation p = Presentation::fromJson(readJson(pd.filePath(f)));
        if (!p.id.isEmpty()) m_presentations.insert(p.id, p);
    }
    m_playlists.clear();
    const QDir ld(m_root + "/Playlists");
    for (const QString &f : ld.entryList({"*.cplist"}, QDir::Files))
        m_playlists.append(Playlist::fromJson(readJson(ld.filePath(f))));
    std::sort(m_playlists.begin(), m_playlists.end(),
              [](const Playlist &a, const Playlist &b) { return a.name < b.name; });
    emit presentationsChanged();
    emit playlistsChanged();
}

QList<Presentation> Library::presentations() const
{
    QList<Presentation> list = m_presentations.values();
    std::sort(list.begin(), list.end(), [](const Presentation &a, const Presentation &b) {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    return list;
}

const Presentation *Library::find(const QString &id) const
{
    auto it = m_presentations.constFind(id);
    return it == m_presentations.constEnd() ? nullptr : &it.value();
}

void Library::save(const Presentation &p)
{
    const bool isNew = !m_presentations.contains(p.id);
    m_presentations.insert(p.id, p);
    writeJson(presPath(p.id), p.toJson());
    if (isNew) emit presentationsChanged();
    emit presentationChanged(p.id);
}

void Library::remove(const QString &id)
{
    m_presentations.remove(id);
    QFile::remove(presPath(id));
    emit presentationsChanged();
}

const Playlist *Library::playlist(const QString &id) const
{
    for (const Playlist &p : m_playlists)
        if (p.id == id) return &p;
    return nullptr;
}

void Library::savePlaylist(const Playlist &p)
{
    auto it = std::find_if(m_playlists.begin(), m_playlists.end(), [&](const Playlist &x) { return x.id == p.id; });
    if (it == m_playlists.end()) m_playlists.append(p);
    else *it = p;
    writeJson(playlistPath(p.id), p.toJson());
    emit playlistsChanged();
}

void Library::removePlaylist(const QString &id)
{
    m_playlists.erase(std::remove_if(m_playlists.begin(), m_playlists.end(),
                                     [&](const Playlist &x) { return x.id == id; }),
                      m_playlists.end());
    QFile::remove(playlistPath(id));
    emit playlistsChanged();
}
