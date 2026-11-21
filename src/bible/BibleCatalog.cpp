#include "bible/BibleCatalog.h"

#include "bible/Bible.h"
#include "bible/BibleLibrary.h"
#include "bible/BookNames.h"

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

const char *const BibleCatalog::kBebliaRepo = "Beblia/Holy-Bible-XML-Format";
const char *const BibleCatalog::kZefaniaRepo = "kohelet-net-admin/zefania-xml-bibles";
const char *const BibleCatalog::kBranch = "master";

static constexpr qint64 kCacheMaxAgeSecs = 7 * 24 * 3600;

static QByteArray userAgent()
{
    return QByteArray("RegemarkPresent/" RP_VERSION);
}

// ---- CatalogEntry ----------------------------------------------------------

QUrl CatalogEntry::downloadUrl() const
{
    const QByteArray repo(source == Source::Beblia ? BibleCatalog::kBebliaRepo : BibleCatalog::kZefaniaRepo);
    return QUrl::fromEncoded("https://raw.githubusercontent.com/" + repo + '/' + BibleCatalog::kBranch + '/'
                             + QUrl::toPercentEncoding(path, "/"));
}

QString CatalogEntry::installId() const
{
    if (source == Source::Beblia) {
        static const QRegularExpression suffix(QStringLiteral("(Bible)?\\.xml$"), QRegularExpression::CaseInsensitiveOption);
        return BibleLibrary::makeId(QStringLiteral("beblia-") + QString(path).remove(suffix));
    }
    // No date: a newer upload of the same module replaces the old install.
    return BibleLibrary::makeId(languageCode + '-' + module + '-' + name);
}

QString CatalogEntry::abbreviation() const
{
    return source == Source::Beblia ? Bible::bebliaAbbreviation(language, module)
                                    : Bible::abbreviationFor(module, name);
}

// ---- parsing ---------------------------------------------------------------

static const QHash<QString, QString> &languageTable()
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
    return names;
}

QString BibleCatalog::languageName(const QString &code, const QString &fallback)
{
    const auto it = languageTable().constFind(code.trimmed().toUpper());
    if (it != languageTable().cend()) return it.value();
    return fallback.isEmpty() ? code.trimmed() : fallback;
}

QString BibleCatalog::languageCode(const QString &language)
{
    const QString c = BookNames::canonicalLanguage(language);
    if (languageTable().contains(c)) return c;
    // Reverse lookup by name; several codes share a name, prefer the ones Zefania uses.
    static const QHash<QString, QString> byName = [] {
        static const QSet<QString> preferred = {"GER", "DUT", "FRE", "GRE", "CHI", "CZE", "RUM", "ESP",
                                                "GAE", "MAO", "SCR", "SLO", "WEL", "ICE", "IRI"};
        QHash<QString, QString> h;
        for (auto it = languageTable().cbegin(); it != languageTable().cend(); ++it) {
            const QString key = it.value().toLower();
            const auto cur = h.constFind(key);
            if (cur == h.cend() || (preferred.contains(it.key()) && !preferred.contains(cur.value()))
                || (preferred.contains(it.key()) == preferred.contains(cur.value()) && it.key() < cur.value()))
                h.insert(key, it.key());
        }
        return h;
    }();
    return byName.value(language.trimmed().toLower(), language.trimmed());
}

bool BibleCatalog::looksPublicDomain(const QString &language, const QString &version)
{
    Q_UNUSED(language);
    // Only the obvious ones; everything else needs the user's confirmation.
    static const QSet<QString> free = {
        "KJ", "KJV", "AKJV", "KJV1611", "ASV", "WEB", "WEBBE", "WEBME", "YLT", "DARBY", "WBS", "WEBSTER",
        "RWEBSTER", "BBE", "DRA", "DOUAY", "GENEVA", "TYNDALE", "WYCLIFFE", "ROTHERHAM", "ROTH", "WEYMOUTH",
        "WMTH", "NOYES", "LEESER", "JPS", "CPDV", "SEGOND", "LSG", "STATENVERTALING", "VULGATE", "CLVUL",
    };
    if (version.contains(QLatin1String("public domain"), Qt::CaseInsensitive)) return true;
    static const QRegularExpression split(QStringLiteral("[^A-Za-z0-9]+"));
    for (const QString &w : version.split(split, Qt::SkipEmptyParts)) {
        const QString u = w.toUpper();
        if (free.contains(u)) return true;
        // A translation dated before 1930 is out of copyright almost everywhere.
        if (u.size() == 4 && u[0].isDigit() && u.toInt() >= 1380 && u.toInt() < 1930) return true;
    }
    return false;
}

bool BibleCatalog::parseBebliaPath(const QString &path, CatalogEntry *entry)
{
    // Every Bible sits at the repo root as "<Language><Version>Bible.xml".
    if (path.contains('/') || !path.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive)) return false;
    QString language, version;
    if (!Bible::bebliaName(path, &language, &version)) return false;
    CatalogEntry e;
    e.source = CatalogEntry::Source::Beblia;
    e.path = path;
    e.language = language;
    e.languageCode = languageCode(language);
    e.module = version;
    e.name = version.isEmpty() ? language : language + ' ' + version;
    e.publicDomain = looksPublicDomain(language, version);
    if (entry) *entry = e;
    return true;
}

// Title-case the all-caps names in the file names, keeping acronyms:
// "1933_1953 AFRIKAANS BYBEL" -> "1933/1953 Afrikaans Bybel", "ISV NT" stays.
QString BibleCatalog::prettyName(const QString &raw)
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

bool BibleCatalog::parseZefaniaPath(const QString &path, CatalogEntry *entry)
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
    e.source = CatalogEntry::Source::Zefania;
    e.language = languageName(e.languageCode);
    e.publicDomain = looksPublicDomain(e.language, e.module + ' ' + e.name);
    if (entry) *entry = e;
    return true;
}

// ---- catalog ---------------------------------------------------------------

BibleCatalog::BibleCatalog(BibleLibrary *library, QObject *parent)
    : QObject(parent), m_library(library), m_nam(new QNetworkAccessManager(this))
{
    // Zefania keeps its old cache file name so existing caches stay valid.
    m_sources.append(new SourceState{CatalogEntry::Source::Beblia, QLatin1String(kBebliaRepo),
                                     QStringLiteral("beblia-catalog.json"), {}, {}, {}});
    m_sources.append(new SourceState{CatalogEntry::Source::Zefania, QLatin1String(kZefaniaRepo),
                                     QStringLiteral("zefania-catalog.json"), {}, {}, {}});
}

BibleCatalog::~BibleCatalog()
{
    // Don't let aborts call back into a half-destroyed object.
    for (SourceState *s : m_sources)
        if (s->reply) {
            s->reply->disconnect(this);
            s->reply->abort();
        }
    if (m_download) {
        m_download->disconnect(this);
        m_download->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
    }
    qDeleteAll(m_sources);
}

static QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

bool BibleCatalog::isInstalled(const CatalogEntry &e) const
{
    return m_library && m_library->contains(e.installId());
}

void BibleCatalog::rebuild()
{
    m_entries.clear();
    QHash<QString, int> latest;  // Zefania: keep only the newest upload of each module
    for (const SourceState *s : m_sources) {
        for (const QJsonValue &v : s->items) {
            const QJsonObject o = v.toObject();
            CatalogEntry e;
            const bool ok = s->source == CatalogEntry::Source::Beblia ? parseBebliaPath(o["path"].toString(), &e)
                                                                       : parseZefaniaPath(o["path"].toString(), &e);
            if (!ok) continue;
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
    }
    auto rank = [](const CatalogEntry &e) { return e.languageCode == "ENG" ? 0 : e.languageCode == "AFR" ? 1 : 2; };
    std::sort(m_entries.begin(), m_entries.end(), [&](const CatalogEntry &a, const CatalogEntry &b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        if (a.language != b.language) return a.language.localeAwareCompare(b.language) < 0;
        if (a.source != b.source) return a.source == CatalogEntry::Source::Beblia;
        return a.name.localeAwareCompare(b.name) < 0;
    });
}

bool BibleCatalog::loadCache(SourceState &s, bool requireFresh)
{
    QFile f(cacheDir() + '/' + s.cacheFile);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QDateTime fetched = QDateTime::fromString(o["fetched"].toString(), Qt::ISODate);
    if (!fetched.isValid() || o["repo"].toString() != s.repo) return false;
    if (requireFresh && fetched.secsTo(QDateTime::currentDateTimeUtc()) > kCacheMaxAgeSecs) return false;
    const QJsonArray items = o["items"].toArray();
    if (items.isEmpty()) return false;
    s.items = items;
    s.fetchedAt = fetched;
    return true;
}

void BibleCatalog::saveCache(const SourceState &s)
{
    QDir().mkpath(cacheDir());
    QSaveFile f(cacheDir() + '/' + s.cacheFile);
    if (!f.open(QIODevice::WriteOnly)) return;
    const QJsonObject o{{"repo", s.repo}, {"fetched", s.fetchedAt.toString(Qt::ISODate)}, {"items", s.items}};
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    f.commit();
}

void BibleCatalog::refresh(bool force)
{
    if (m_pending > 0) return;
    m_problems.clear();
    for (SourceState *s : m_sources)
        if (force || !loadCache(*s, true)) fetch(*s);
    if (m_pending == 0) finishRefresh();
}

void BibleCatalog::fetch(SourceState &s)
{
    // Beblia keeps every file at the root, so no recursion (and a small reply).
    const bool recursive = s.source == CatalogEntry::Source::Zefania;
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/%1/git/trees/%2%3")
                                 .arg(s.repo, QLatin1String(kBranch),
                                      recursive ? QStringLiteral("?recursive=1") : QString())));
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(30000);
    s.reply = m_nam->get(req);
    ++m_pending;
    SourceState *sp = &s;
    connect(s.reply, &QNetworkReply::finished, this, [this, sp] { onTreeFinished(*sp); });
}

void BibleCatalog::onTreeFinished(SourceState &s)
{
    QNetworkReply *reply = s.reply;
    s.reply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString label = s.source == CatalogEntry::Source::Beblia ? QStringLiteral("Beblia") : QStringLiteral("Zefania");

    QString message;
    if (reply->error() == QNetworkReply::NoError && status == 200) {
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray items;
        for (const QJsonValue &v : o["tree"].toArray()) {
            const QJsonObject t = v.toObject();
            const QString path = t["path"].toString();
            if (t["type"].toString() != QLatin1String("blob")) continue;
            const bool ok = s.source == CatalogEntry::Source::Beblia ? parseBebliaPath(path, nullptr)
                                                                     : parseZefaniaPath(path, nullptr);
            if (ok) items.append(QJsonObject{{"path", path}, {"size", t["size"]}});
        }
        if (!items.isEmpty()) {
            s.items = items;
            s.fetchedAt = QDateTime::currentDateTimeUtc();
            saveCache(s);
        } else {
            message = tr("the list was empty or unreadable");
        }
    } else if ((status == 403 || status == 429) && reply->rawHeader("x-ratelimit-remaining") == "0") {
        const QDateTime reset = QDateTime::fromSecsSinceEpoch(reply->rawHeader("x-ratelimit-reset").toLongLong());
        message = tr("GitHub's hourly limit for anonymous requests was reached; try again after %1")
                      .arg(reset.toLocalTime().toString(QStringLiteral("HH:mm")));
    } else {
        message = reply->errorString();
    }

    if (!message.isEmpty()) {
        if (loadCache(s, false))
            m_problems << tr("%1: %2 (showing the list from %3)")
                              .arg(label, message, s.fetchedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd")));
        else
            m_problems << tr("%1: %2").arg(label, message);
    }
    if (--m_pending == 0) finishRefresh();
}

void BibleCatalog::finishRefresh()
{
    rebuild();
    if (m_entries.isEmpty()) {
        emit catalogError(tr("Couldn't load the online Bible list. %1").arg(m_problems.join(QStringLiteral("; "))));
        return;
    }
    if (!m_problems.isEmpty())
        emit notice(tr("Couldn't refresh part of the online list. %1").arg(m_problems.join(QStringLiteral("; "))));
    emit catalogReady();
}

bool BibleCatalog::download(const CatalogEntry &e)
{
    if (m_download) return false;
    const QString dir = cacheDir() + "/downloads";
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
    connect(m_download, &QNetworkReply::finished, this, &BibleCatalog::onDownloadFinished);
    return true;
}

void BibleCatalog::cancelDownload()
{
    if (m_download) m_download->abort();  // finishes with OperationCanceledError
}

void BibleCatalog::onDownloadFinished()
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

    const CatalogEntry &e = m_downloadEntry;
    BibleEntry hint;
    hint.id = e.installId();
    // Beblia files name themselves well ("Afrikaans 1983"); Zefania file names are better than their titles.
    if (e.source == CatalogEntry::Source::Zefania) hint.name = e.name;
    hint.languageCode = e.languageCode;
    hint.language = e.language;
    hint.abbreviation = e.abbreviation();
    hint.source = e.sourceName() + QStringLiteral(": ") + e.path;
    QString error;
    const QString id = m_library ? m_library->importFile(path, &error, hint) : QString();
    QFile::remove(path);
    if (id.isEmpty()) emit downloadFailed(error.isEmpty() ? tr("Couldn't install the Bible.") : error);
    else emit downloadFinished(id);
}
