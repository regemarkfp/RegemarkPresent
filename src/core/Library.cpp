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

QStringList Playlist::segmentKeys()
{
    return {"pre", "intro", "service", "outro", "post"};
}

QString Playlist::segmentName(const QString &key)
{
    if (key == "pre") return QObject::tr("Pre-Service");
    if (key == "intro") return QObject::tr("Intro");
    if (key == "service") return QObject::tr("Service");
    if (key == "outro") return QObject::tr("Outro");
    if (key == "post") return QObject::tr("Post-Service");
    return key;
}

void Playlist::normalize()
{
    QHash<QString, QList<PlaylistItem>> bySegment;
    QString current = "service";   // rows before any segment (old playlists) belong to Service
    for (const PlaylistItem &i : std::as_const(items)) {
        if (i.isSegment() && segmentKeys().contains(i.segment)) current = i.segment;
        else bySegment[current].append(i);
    }
    items.clear();
    for (const QString &key : segmentKeys()) {
        items.append({QString(), segmentName(key), key});
        for (const PlaylistItem &i : bySegment.value(key)) {
            // Sections only make sense inside Service; elsewhere drop the divider.
            if (i.isSection() && key != "service") continue;
            items.append(i);
        }
    }
}

QString Playlist::segmentAt(int row) const
{
    for (int r = qMin(row, int(items.size()) - 1); r >= 0; --r)
        if (items[r].isSegment()) return items[r].segment;
    return "service";
}

int Playlist::segmentRow(const QString &key) const
{
    for (int r = 0; r < items.size(); ++r)
        if (items[r].segment == key) return r;
    return -1;
}

int Playlist::segmentEnd(const QString &key) const
{
    int r = segmentRow(key);
    if (r < 0) return int(items.size());
    for (++r; r < items.size() && !items[r].isSegment(); ++r) {}
    return r;
}

Playlist Playlist::create(const QString &name)
{
    Playlist p;
    p.id = newId();
    p.name = name;
    p.normalize();
    return p;
}

QJsonObject Playlist::toJson() const
{
    QJsonArray a;
    for (const PlaylistItem &i : items) {
        if (i.isItem()) a.append(QJsonObject{{"presentation", i.presentationId}});
        else if (i.isSegment()) a.append(QJsonObject{{"segment", i.segment}});
        else a.append(QJsonObject{{"header", i.header}});
    }
    return {{"format", "regemarkpresent.playlist"}, {"id", id}, {"name", name}, {"items", a},
            {"preServiceAdvance", preServiceAdvanceSec}};
}

Playlist Playlist::fromJson(const QJsonObject &o)
{
    Playlist p;
    p.id = o["id"].toString(newId());
    p.name = o["name"].toString();
    p.preServiceAdvanceSec = o["preServiceAdvance"].toInt(8);
    for (const QJsonValue &v : o["items"].toArray()) {
        const QJsonObject i = v.toObject();
        const QString seg = i["segment"].toString();
        p.items.append({i["presentation"].toString(), seg.isEmpty() ? i["header"].toString() : segmentName(seg), seg});
    }
    p.normalize();
    return p;
}

Library::Library(const QString &root, QObject *parent)
    : QObject(parent), m_root(root)
{
    QDir().mkpath(m_root + "/Presentations");
    QDir().mkpath(m_root + "/Playlists");
    QDir().mkpath(m_root + "/Media");
    QDir().mkpath(m_root + "/Looks");
}

QString Library::presPath(const QString &id) const { return m_root + "/Presentations/" + id + ".rpres"; }
QString Library::playlistPath(const QString &id) const { return m_root + "/Playlists/" + id + ".rplist"; }

void Library::load()
{
    m_presentations.clear();
    const QDir pd(m_root + "/Presentations");
    for (const QString &f : pd.entryList({"*.rpres"}, QDir::Files)) {
        Presentation p = Presentation::fromJson(readJson(pd.filePath(f)));
        if (!p.id.isEmpty()) m_presentations.insert(p.id, p);
    }
    m_playlists.clear();
    const QDir ld(m_root + "/Playlists");
    for (const QString &f : ld.entryList({"*.rplist"}, QDir::Files))
        m_playlists.append(Playlist::fromJson(readJson(ld.filePath(f))));
    std::sort(m_playlists.begin(), m_playlists.end(),
              [](const Playlist &a, const Playlist &b) { return a.name < b.name; });
    m_looks.clear();
    const QDir lk(m_root + "/Looks");
    for (const QString &f : lk.entryList({"*.rplook"}, QDir::Files))
        m_looks.append(Look::fromJson(readJson(lk.filePath(f))));
    std::sort(m_looks.begin(), m_looks.end(), [](const Look &a, const Look &b) { return a.name < b.name; });
    emit presentationsChanged();
    emit playlistsChanged();
    emit looksChanged();
}

const Look *Library::look(const QString &id) const
{
    for (const Look &l : m_looks)
        if (l.id == id) return &l;
    return nullptr;
}

void Library::saveLook(const Look &l)
{
    auto it = std::find_if(m_looks.begin(), m_looks.end(), [&](const Look &x) { return x.id == l.id; });
    if (it == m_looks.end()) m_looks.append(l);
    else *it = l;
    writeJson(m_root + "/Looks/" + l.id + ".rplook", l.toJson());
    emit looksChanged();
}

void Library::removeLook(const QString &id)
{
    m_looks.erase(std::remove_if(m_looks.begin(), m_looks.end(), [&](const Look &x) { return x.id == id; }),
                  m_looks.end());
    QFile::remove(m_root + "/Looks/" + id + ".rplook");
    emit looksChanged();
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
