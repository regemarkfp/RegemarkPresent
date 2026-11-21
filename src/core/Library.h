#pragma once

#include "core/Types.h"

#include <QHash>
#include <QObject>

struct PlaylistItem {
    QString presentationId;  // empty = section header
    QString header;
};

struct Playlist {
    QString id;
    QString name;
    QList<PlaylistItem> items;

    QJsonObject toJson() const;
    static Playlist fromJson(const QJsonObject &o);
};

// File-backed store: one JSON file per presentation / playlist under
// Documents/Regemark Present so libraries are easy to back up or sync.
class Library : public QObject {
    Q_OBJECT
public:
    explicit Library(const QString &root, QObject *parent = nullptr);

    void load();
    QString root() const { return m_root; }

    QList<Presentation> presentations() const;
    const Presentation *find(const QString &id) const;
    void save(const Presentation &p);
    void remove(const QString &id);

    QList<Playlist> playlists() const { return m_playlists; }
    const Playlist *playlist(const QString &id) const;
    void savePlaylist(const Playlist &p);
    void removePlaylist(const QString &id);

    QList<Look> looks() const { return m_looks; }
    const Look *look(const QString &id) const;
    void saveLook(const Look &l);
    void removeLook(const QString &id);

signals:
    void presentationsChanged();
    void presentationChanged(const QString &id);
    void playlistsChanged();
    void looksChanged();

private:
    QString presPath(const QString &id) const;
    QString playlistPath(const QString &id) const;

    QString m_root;
    QHash<QString, Presentation> m_presentations;
    QList<Playlist> m_playlists;
    QList<Look> m_looks;
};
