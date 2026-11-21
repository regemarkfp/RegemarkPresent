#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

class BibleLibrary;
class QFile;
class QNetworkAccessManager;

struct CatalogEntry {
    enum class Source { Beblia, Zefania };
    Source source = Source::Beblia;
    QString path;          // repo path: "Afrikaans1983Bible.xml" or "Bibles/ENG/King James/KJV+/SF_..._(KJV+).xml"
    QString languageCode;  // "ENG", "AFR"; the language name when there is no known code
    QString language;      // "English"
    QString module;        // version/module code: "1983", "NLV", "KJV"
    QString name;          // "Afrikaans 1983", "King James Version"
    QString date;          // "2009-01-20" (Zefania only, may be empty)
    qint64 size = 0;       // bytes, from the git tree
    bool publicDomain = false;  // obviously free to use (KJV, ASV, WEB, pre-1930 ...)

    QString sourceName() const { return source == Source::Beblia ? QStringLiteral("Beblia") : QStringLiteral("Zefania"); }
    QUrl downloadUrl() const;
    QString installId() const;   // id used in BibleLibrary, stable per translation
    QString abbreviation() const;
};

// The online list of installable Bibles, merged from two GitHub repos:
//   Beblia/Holy-Bible-XML-Format (primary, ~1000 translations at the repo root)
//   kohelet-net-admin/zefania-xml-bibles (Zefania XML, under Bibles/)
// Each comes from one git-trees API call. Unauthenticated clients get 60
// calls/hour, so each list is cached for a week and a stale cache is used when
// offline or rate limited.
class BibleCatalog : public QObject {
    Q_OBJECT
public:
    explicit BibleCatalog(BibleLibrary *library, QObject *parent = nullptr);
    ~BibleCatalog() override;

    void refresh(bool force = false);
    bool isRefreshing() const { return m_pending > 0; }
    QList<CatalogEntry> entries() const { return m_entries; }  // English, Afrikaans first
    bool isInstalled(const CatalogEntry &e) const;

    // One download at a time; finishes by installing into the library.
    bool download(const CatalogEntry &e);
    void cancelDownload();
    bool isDownloading() const { return m_download != nullptr; }
    QString downloadingPath() const { return m_downloadEntry.path; }

    // Repo path -> entry; false for files that aren't Bibles.
    static bool parseBebliaPath(const QString &path, CatalogEntry *entry);
    static bool parseZefaniaPath(const QString &path, CatalogEntry *entry);
    static QString languageName(const QString &code, const QString &fallback = QString());
    static QString languageCode(const QString &language);   // "Afrikaans"/"af"/"AFR" -> "AFR"
    static QString prettyName(const QString &raw);
    static bool looksPublicDomain(const QString &language, const QString &version);

    static const char *const kBebliaRepo;   // "Beblia/Holy-Bible-XML-Format"
    static const char *const kZefaniaRepo;  // "kohelet-net-admin/zefania-xml-bibles"
    static const char *const kBranch;       // "master" (both)

signals:
    void catalogReady();
    void catalogError(const QString &message);
    void notice(const QString &message);   // e.g. "offline, showing cached list"
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &bibleId);
    void downloadFailed(const QString &message);

private:
    struct SourceState {
        CatalogEntry::Source source;
        QString repo;
        QString cacheFile;
        QPointer<QNetworkReply> reply;
        QJsonArray items;          // [{path, size}] from the tree
        QDateTime fetchedAt;
    };
    void fetch(SourceState &s);
    void onTreeFinished(SourceState &s);
    bool loadCache(SourceState &s, bool requireFresh);
    void saveCache(const SourceState &s);
    void rebuild();
    void finishRefresh();
    void onDownloadFinished();

    BibleLibrary *m_library;
    QNetworkAccessManager *m_nam;
    QList<SourceState *> m_sources;
    int m_pending = 0;
    QStringList m_problems;
    QPointer<QNetworkReply> m_download;
    QFile *m_downloadFile = nullptr;
    CatalogEntry m_downloadEntry;
    QList<CatalogEntry> m_entries;
};
