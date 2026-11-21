#include "bible/ZefaniaCatalog.h"

#include "bible/BibleLibrary.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

const char *const ZefaniaCatalog::kRepo = "kohelet-net-admin/zefania-xml-bibles";
const char *const ZefaniaCatalog::kBranch = "master";

static constexpr qint64 kCacheMaxAgeSecs = 7 * 24 * 3600;

static QByteArray userAgent()
{
    return QByteArray("RegemarkPresent/" RP_VERSION);
}

// ---- CatalogEntry ----------------------------------------------------------

QUrl CatalogEntry::downloadUrl() const
{
    const QByteArray encoded = QUrl::toPercentEncoding(path, "/");
    return QUrl::fromEncoded("https://raw.githubusercontent.com/" + QByteArray(ZefaniaCatalog::kRepo) + '/'
                             + ZefaniaCatalog::kBranch + '/' + encoded);
}

QString CatalogEntry::installId() const
{
    // No date: a newer upload of the same module replaces the old install.
    return BibleLibrary::makeId(languageCode + '-' + module + '-' + name);
}

// ---- parsing ---------------------------------------------------------------

QString ZefaniaCatalog::languageName(const QString &code)
{
    static const QHash<QString, QString> names = {
        {"ENG", "English"}, {"AFR", "Afrikaans"}, {"GER", "German"}, {"DEU", "German"},
        {"DEUTSCH", "German"}, {"DUT", "Dutch"}, {"NLD", "Dutch"}, {"NL", "Dutch"},
        {"FRE", "French"}, {"FRA", "French"}, {"SPA", "Spanish"}, {"POR", "Portuguese"},
        {"ZUL", "Zulu"}, {"XHO", "Xhosa"}, {"SOT", "Sesotho"}, {"TSN", "Setswana"},
        {"NSO", "Sepedi"}, {"SSW", "Swati"}, {"TSO", "Xitsonga"}, {"VEN", "Venda"},
        {"NBL", "Ndebele"}, {"SWA", "Swahili"}, {"GRC", "Greek (Ancient)"}, {"GRE", "Greek"},
        {"ELL", "Greek"}, {"HEB", "Hebrew"}, {"LAT", "Latin"}, {"ITA", "Italian"},
        {"RUS", "Russian"}, {"UKR", "Ukrainian"}, {"CHI", "Chinese"}, {"ZHO", "Chinese"},
        {"ARA", "Arabic"}, {"BUL", "Bulgarian"}, {"CEB", "Cebuano"}, {"GOT", "Gothic"},
        {"HUN", "Hungarian"}, {"NDS", "Low German"}, {"NOR", "Norwegian"}, {"RUM", "Romanian"},
        {"RON", "Romanian"}, {"SWE", "Swedish"}, {"VIE", "Vietnamese"}, {"ALB", "Albanian"},
        {"BAQ", "Basque"}, {"CHA", "Chamorro"}, {"CHU", "Church Slavonic"}, {"CKB", "Kurdish (Sorani)"},
        {"COP", "Coptic"}, {"CZE", "Czech"}, {"CES", "Czech"}, {"DAN", "Danish"},
        {"ESP", "Esperanto"}, {"EPO", "Esperanto"}, {"FIN", "Finnish"}, {"GAE", "Gaelic"},
        {"GLA", "Gaelic"}, {"GLV", "Manx"}, {"HAT", "Haitian Creole"}, {"HIN", "Hindi"},
        {"KAB", "Kabyle"}, {"KOR", "Korean"}, {"LAV", "Latvian"}, {"MAO", "Maori"},
        {"MRI", "Maori"}, {"POL", "Polish"}, {"PON", "Pohnpeian"}, {"SCR", "Croatian"},
        {"HRV", "Croatian"}, {"SHU", "Chadian Arabic"}, {"SYR", "Syriac"}, {"TGL", "Tagalog"},
        {"THA", "Thai"}, {"AZB", "Azerbaijani (South)"}, {"TUR", "Turkish"}, {"JPN", "Japanese"},
        {"IND", "Indonesian"}, {"EST", "Estonian"}, {"LIT", "Lithuanian"}, {"SLO", "Slovak"},
        {"SLK", "Slovak"}, {"SLV", "Slovenian"}, {"SRP", "Serbian"}, {"WEL", "Welsh"},
        {"CYM", "Welsh"}, {"ICE", "Icelandic"}, {"ISL", "Icelandic"}, {"IRI", "Irish"},
        {"GLE", "Irish"}, {"AMH", "Amharic"}, {"UND", "Other"},
    };
    const QString c = code.trimmed().toUpper();
    return names.value(c, c);
}

// Title-case the all-caps names in the file names, keeping acronyms:
// "1933_1953 AFRIKAANS BYBEL" -> "1933/1953 Afrikaans Bybel", "ISV NT" stays.
QString ZefaniaCatalog::prettyName(const QString &raw)
{
    QString s = raw;
    s.remove(QRegularExpression(QStringLiteral("^DESCRIPTION_[A-Z]{2}="), QRegularExpression::CaseInsensitiveOption));
    s.replace(QRegularExpression(QStringLiteral("(\\d{4})_(\\d{4})")), QStringLiteral("\\1/\\2"));
    s.replace(QRegularExpression(QStringLiteral("(\\d)_(\\d)")), QStringLiteral("\\1.\\2"));
    s.replace('_', ' ');
    s = s.simplified();
    if (s != s.toUpper()) return s;  // already mixed case

    static const QSet<QString> words = {
        "THE", "OF", "AND", "NEW", "OLD", "IN", "WITH", "FOR", "FROM", "TO", "A", "AN", "HOLY",
        "BOOK", "WORD", "LIFE", "GOOD", "NEWS", "OUR", "LORD", "GOD", "DE", "LA", "LE", "DU", "DES",
        "DER", "DIE", "DAS", "DEN", "EN", "ET", "VON", "UND", "ZU", "IM", "EL", "LOS", "LAS", "DEL",
        "Y", "E", "DI", "IL", "ANG", "SA", "NA", "NG", "AT", "VAN", "HET", "NOVA", "SANT", "SACRA",
    };
    static const QSet<QString> small = {"of", "and", "the", "in", "with", "for", "from", "to", "a", "an",
                                        "de", "la", "le", "du", "des", "der", "die", "das", "den", "en",
                                        "et", "von", "und", "zu", "im", "el", "los", "las", "del", "y",
                                        "e", "di", "il", "sa", "ng", "van", "het"};
    static const QRegularExpression core(QStringLiteral("[^\\p{L}\\d+]"));
    QStringList out;
    const QStringList tokens = s.split(' ');
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &t = tokens[i];
        const QString letters = QString(t).remove(core);
        const bool acronym = letters.contains(QRegularExpression(QStringLiteral("[\\d+]")))
                             || (letters.size() <= 3 && !words.contains(letters))
                             || (letters.size() == 4 && !words.contains(letters)
                                 && !letters.contains(QRegularExpression(QStringLiteral("[AEIOUY]"))));
        if (acronym) {
            out << t;
            continue;
        }
        QString w = t.toLower();
        if (i > 0 && small.contains(letters.toLower())) {
            out << w;
            continue;
        }
        // Capitalise each part of "PERRET-GENTIL" and the start of "(BUGNA".
        bool start = true;
        for (QChar &c : w) {
            if (c.isLetter()) {
                if (start) c = c.toUpper();
                start = false;
            } else if (c == '-' || c == '(' || c == '/') {
                start = true;
            }
        }
        out << w;
    }
    return out.join(' ');
}

static QString normaliseDate(const QString &d)
{
    static const QRegularExpression dmy(QStringLiteral("^(\\d{2})\\.(\\d{2})\\.(\\d{4})$"));
    const auto m = dmy.match(d);
    if (m.hasMatch()) return QStringLiteral("%1-%2-%3").arg(m.captured(3), m.captured(2), m.captured(1));
    return d;
}

bool ZefaniaCatalog::parsePath(const QString &path, CatalogEntry *entry)
{
    if (!path.startsWith(QLatin1String("Bibles/")) || !path.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive))
        return false;
    const QStringList parts = path.split('/');
    if (parts.size() < 3) return false;
    const QString dirCode = parts[1].toUpper();
    const QString stem = parts.last().chopped(4);

    CatalogEntry e;
    e.path = path;
    // SF_<date>_<LANG>_<MODULE>_(<NAME>); the module may contain '_' and the
    // name nested parentheses.
    static const QRegularExpression re(QStringLiteral("^SF_([^_]+)_([A-Za-z]{2,8})_(.+?)_\\((.*)\\)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(stem);
    if (m.hasMatch()) {
        e.date = normaliseDate(m.captured(1));
        const QString code = m.captured(2).toUpper();
        // The folder is authoritative unless the file names a different 3-letter code (AZB under TUR).
        e.languageCode = code.size() == 3 ? code : dirCode;
        e.module = m.captured(3);
        e.name = prettyName(m.captured(4));
    } else {
        // Odd ones like "sf_czech_bkr.xml": use the folder names.
        e.languageCode = dirCode;
        e.module = QString(stem).remove(QRegularExpression(QStringLiteral("^sf_"), QRegularExpression::CaseInsensitiveOption)).toUpper();
        e.name = parts.size() > 3 ? parts[parts.size() - 2] : e.module;
    }
    if (e.name.isEmpty()) e.name = parts.size() > 3 ? parts[parts.size() - 2] : e.module;
    e.language = languageName(e.languageCode);
    if (entry) *entry = e;
    return true;
}

// ---- catalog ---------------------------------------------------------------

ZefaniaCatalog::ZefaniaCatalog(BibleLibrary *library, QObject *parent)
    : QObject(parent), m_library(library), m_nam(new QNetworkAccessManager(this))
{
}

ZefaniaCatalog::~ZefaniaCatalog()
{
    // Don't let aborts call back into a half-destroyed object.
    if (m_treeReply) {
        m_treeReply->disconnect(this);
        m_treeReply->abort();
    }
    if (m_download) {
        m_download->disconnect(this);
        m_download->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
    }
}

QString ZefaniaCatalog::cachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/zefania-catalog.json";
}

bool ZefaniaCatalog::isInstalled(const CatalogEntry &e) const
{
    return m_library && m_library->contains(e.installId());
}

void ZefaniaCatalog::setItems(const QJsonArray &items)
{
    // Keep only the newest upload of each module.
    QHash<QString, int> latest;
    m_entries.clear();
    for (const QJsonValue &v : items) {
        const QJsonObject o = v.toObject();
        CatalogEntry e;
        if (!parsePath(o["path"].toString(), &e)) continue;
        e.size = qint64(o["size"].toDouble());
        const QString key = e.installId();
        const auto it = latest.constFind(key);
        if (it == latest.cend()) {
            latest.insert(key, int(m_entries.size()));
            m_entries.append(e);
        } else if (e.date > m_entries[*it].date) {
            m_entries[*it] = e;
        }
    }
    auto rank = [](const CatalogEntry &e) { return e.languageCode == "ENG" ? 0 : e.languageCode == "AFR" ? 1 : 2; };
    std::sort(m_entries.begin(), m_entries.end(), [&](const CatalogEntry &a, const CatalogEntry &b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        if (a.language != b.language) return a.language.localeAwareCompare(b.language) < 0;
        return a.name.localeAwareCompare(b.name) < 0;
    });
}

bool ZefaniaCatalog::loadCache(bool requireFresh)
{
    QFile f(cachePath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QDateTime fetched = QDateTime::fromString(o["fetched"].toString(), Qt::ISODate);
    if (!fetched.isValid() || o["repo"].toString() != QLatin1String(kRepo)) return false;
    if (requireFresh && fetched.secsTo(QDateTime::currentDateTimeUtc()) > kCacheMaxAgeSecs) return false;
    setItems(o["items"].toArray());
    m_fetchedAt = fetched;
    return !m_entries.isEmpty();
}

void ZefaniaCatalog::saveCache(const QJsonArray &items)
{
    QDir().mkpath(QFileInfo(cachePath()).absolutePath());
    QSaveFile f(cachePath());
    if (!f.open(QIODevice::WriteOnly)) return;
    const QJsonObject o{{"repo", kRepo}, {"fetched", m_fetchedAt.toString(Qt::ISODate)}, {"items", items}};
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    f.commit();
}

void ZefaniaCatalog::refresh(bool force)
{
    if (m_treeReply) return;
    if (!force && loadCache(true)) {
        emit catalogReady();
        return;
    }
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/%1/git/trees/%2?recursive=1")
                                 .arg(QLatin1String(kRepo), QLatin1String(kBranch))));
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(30000);
    m_treeReply = m_nam->get(req);
    connect(m_treeReply, &QNetworkReply::finished, this, &ZefaniaCatalog::onTreeFinished);
}

void ZefaniaCatalog::onTreeFinished()
{
    QNetworkReply *reply = m_treeReply;
    m_treeReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QString message;
    if (reply->error() == QNetworkReply::NoError && status == 200) {
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray items;
        for (const QJsonValue &v : o["tree"].toArray()) {
            const QJsonObject t = v.toObject();
            if (t["type"].toString() == QLatin1String("blob") && parsePath(t["path"].toString(), nullptr))
                items.append(QJsonObject{{"path", t["path"]}, {"size", t["size"]}});
        }
        if (!items.isEmpty()) {
            m_fetchedAt = QDateTime::currentDateTimeUtc();
            saveCache(items);
            setItems(items);
            emit catalogReady();
            return;
        }
        message = tr("The online list was empty or unreadable.");
    } else if ((status == 403 || status == 429) && reply->rawHeader("x-ratelimit-remaining") == "0") {
        const QDateTime reset = QDateTime::fromSecsSinceEpoch(reply->rawHeader("x-ratelimit-reset").toLongLong());
        message = tr("GitHub's hourly limit for anonymous requests was reached; try again after %1.")
                      .arg(reset.toLocalTime().toString(QStringLiteral("HH:mm")));
    } else {
        message = reply->errorString();
    }

    if (loadCache(false)) {
        emit notice(tr("Couldn't refresh the online list (%1). Showing the list from %2.")
                        .arg(message, m_fetchedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd"))));
        emit catalogReady();
    } else {
        emit catalogError(tr("Couldn't load the online Bible list: %1").arg(message));
    }
}

bool ZefaniaCatalog::download(const CatalogEntry &e)
{
    if (m_download) return false;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/downloads";
    QDir().mkpath(dir);
    m_downloadFile = new QFile(dir + '/' + e.installId() + ".xml.part", this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = m_downloadFile->errorString();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        emit downloadFailed(tr("Couldn't write the download: %1").arg(err));
        return false;
    }
    m_downloadEntry = e;
    QNetworkRequest req(e.downloadUrl());
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setTransferTimeout(60000);
    m_download = m_nam->get(req);
    connect(m_download, &QNetworkReply::readyRead, this, [this] {
        if (m_download && m_downloadFile) m_downloadFile->write(m_download->readAll());
    });
    connect(m_download, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total > 0 ? total : m_downloadEntry.size);
    });
    connect(m_download, &QNetworkReply::finished, this, &ZefaniaCatalog::onDownloadFinished);
    return true;
}

void ZefaniaCatalog::cancelDownload()
{
    if (m_download) m_download->abort();  // finishes with OperationCanceledError
}

void ZefaniaCatalog::onDownloadFinished()
{
    QNetworkReply *reply = m_download;
    m_download = nullptr;
    if (!reply || !m_downloadFile) return;
    reply->deleteLater();
    m_downloadFile->write(reply->readAll());
    m_downloadFile->close();
    const QString path = m_downloadFile->fileName();
    delete m_downloadFile;
    m_downloadFile = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status != 200) {
        QFile::remove(path);
        emit downloadFailed(reply->error() == QNetworkReply::OperationCanceledError
                                ? tr("Download cancelled.")
                                : tr("Download failed: %1").arg(reply->errorString()));
        return;
    }

    BibleEntry hint;
    hint.id = m_downloadEntry.installId();
    hint.name = m_downloadEntry.name;
    hint.languageCode = m_downloadEntry.languageCode;
    hint.language = m_downloadEntry.language;
    hint.abbreviation = Bible::abbreviationFor(m_downloadEntry.module, m_downloadEntry.name);
    hint.source = m_downloadEntry.path;
    QString error;
    const QString id = m_library ? m_library->importFile(path, &error, hint) : QString();
    QFile::remove(path);
    if (id.isEmpty()) emit downloadFailed(error.isEmpty() ? tr("Couldn't install the Bible.") : error);
    else emit downloadFinished(id);
}
