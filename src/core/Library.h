#pragma once

#include "core/Types.h"

#include <QHash>
#include <QObject>

// One row of a rundown: a fixed segment, a section inside Service, or an
// item (song, presentation, media cue, scripture...).
struct PlaylistItem {
    QString presentationId;  // set for items
    QString header;          // display name for segments / sections
    QString segment;         // fixed segment key ("pre", "intro", ...); empty otherwise

    bool isItem() const { return !presentationId.isEmpty(); }
    bool isSegment() const { return !segment.isEmpty(); }
    bool isSection() const { return !isItem() && !isSegment(); }

    static PlaylistItem item(const QString &id) { return {id, QString(), QString()}; }
    static PlaylistItem section(const QString &name) { return {QString(), name, QString()}; }
};

// A service rundown. It always has the five fixed segments, in order:
// Pre-Service (loops), Intro, Service (optionally split into sections such
// as Worship or Sermon), Outro and Post-Service.
struct Playlist {
    QString id;
    QString name;
    QList<PlaylistItem> items;
    int preServiceAdvanceSec = 8;   // Pre-Service slides advance on their own

    static QStringList segmentKeys();                 // pre, intro, service, outro, post
    static QString segmentName(const QString &key);

    // Put the fixed segments in place and order, keeping everything else
    // under the segment it belongs to (loose items go to Service).
    void normalize();
    QString segmentAt(int row) const;                 // which fixed segment a row is in
    int segmentRow(const QString &key) const;
    int segmentEnd(const QString &key) const;         // row after the segment's last row

    QJsonObject toJson() const;
    static Playlist fromJson(const QJsonObject &o);
    static Playlist create(const QString &name);
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
