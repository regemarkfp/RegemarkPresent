#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>

class BibleLibrary;
class QFile;
class QNetworkAccessManager;

struct CatalogEntry {
    QString path;          // repo path, "Bibles/ENG/King James/KJV+/SF_2009-01-20_ENG_KJV_(KJV+).xml"
    QString languageCode;  // "ENG"
    QString language;      // "English"
    QString module;        // "KJV"
    QString name;          // "KJV+", "King James Version"
    QString date;          // "2009-01-20" (may be empty)
    qint64 size = 0;       // bytes, from the git tree

    QUrl downloadUrl() const;
    QString installId() const;   // id used in BibleLibrary, stable per file
};

// Online list of Zefania XML Bibles from the kohelet-net-admin/zefania-xml-bibles
// GitHub repo. The whole tree comes from one API call; unauthenticated
// clients get 60 calls/hour, so the result is cached for a week and the stale
// cache is used when offline or rate limited.
class ZefaniaCatalog : public QObject {
    Q_OBJECT
public:
    explicit ZefaniaCatalog(BibleLibrary *library, QObject *parent = nullptr);
    ~ZefaniaCatalog() override;

    void refresh(bool force = false);
    bool isRefreshing() const { return m_treeReply != nullptr; }
    QList<CatalogEntry> entries() const { return m_entries; }
    QDateTime fetchedAt() const { return m_fetchedAt; }
    bool isInstalled(const CatalogEntry &e) const;

    // One download at a time; finishes by installing into the library.
    bool download(const CatalogEntry &e);
    void cancelDownload();
    bool isDownloading() const { return m_download != nullptr; }
    QString downloadingPath() const { return m_downloadEntry.path; }

    // Repo path -> entry; false for non-Bible files (dictionaries, CSVs, schema).
    static bool parsePath(const QString &path, CatalogEntry *entry);
    static QString languageName(const QString &code);
    static QString prettyName(const QString &raw);

    static const char *const kRepo;     // "kohelet-net-admin/zefania-xml-bibles"
    static const char *const kBranch;   // "master"

signals:
    void catalogReady();
    void catalogError(const QString &message);
    void notice(const QString &message);   // e.g. "offline, showing cached list"
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &bibleId);
    void downloadFailed(const QString &message);

private:
    void onTreeFinished();
    void onDownloadFinished();
    bool loadCache(bool requireFresh);
    void saveCache(const QJsonArray &items);
    void setItems(const QJsonArray &items);
    QString cachePath() const;

    BibleLibrary *m_library;
    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_treeReply;
    QPointer<QNetworkReply> m_download;
    QFile *m_downloadFile = nullptr;
    CatalogEntry m_downloadEntry;
    QList<CatalogEntry> m_entries;
    QDateTime m_fetchedAt;
};
