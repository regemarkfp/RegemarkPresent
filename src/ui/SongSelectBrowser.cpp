#include "ui/SongSelectBrowser.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef RP_HAVE_WEBENGINE
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>
#include <QWebEngineView>
#endif

static const QUrl kHome(QStringLiteral("https://songselect.ccli.com/"));
static const QStringList kSongExt = {"usr", "txt", "cho", "chordpro", "chopro", "pro"};

bool SongSelectBrowser::embedded()
{
#ifdef RP_HAVE_WEBENGINE
    return true;
#else
    return false;
#endif
}

SongSelectBrowser::SongSelectBrowser(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    m_status = new QLabel;
    m_status->setProperty("role", "hint");
    m_status->setWordWrap(true);

#ifdef RP_HAVE_WEBENGINE
    // A named profile keeps cookies on disk, so the operator signs in once.
    auto *profile = new QWebEngineProfile(QStringLiteral("songselect"), this);
    profile->setPersistentStoragePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/web/songselect");
    auto *view = new QWebEngineView(this);
    view->setPage(new QWebEnginePage(profile, view));

    auto *bar = new QHBoxLayout;
    auto button = [&](const QString &text, const QString &tip) {
        auto *b = new QToolButton;
        b->setText(text);
        b->setToolTip(tip);
        bar->addWidget(b);
        return b;
    };
    connect(button("←", tr("Back")), &QToolButton::clicked, view, &QWebEngineView::back);
    connect(button("→", tr("Forward")), &QToolButton::clicked, view, &QWebEngineView::forward);
    connect(button("⟳", tr("Reload")), &QToolButton::clicked, view, &QWebEngineView::reload);
    connect(button(tr("Home"), tr("SongSelect home")), &QToolButton::clicked, view, [view] { view->load(kHome); });
    bar->addWidget(m_status, 1);
    layout->addLayout(bar);
    layout->addWidget(view, 1);
    notify(tr("Sign in with your church's CCLI account, find a song and click Download (lyrics, ChordPro or USR). "
              "It goes straight into your library."));

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/regemark-songselect";
    QDir().mkpath(dir);
    connect(profile, &QWebEngineProfile::downloadRequested, this, [this, dir](QWebEngineDownloadRequest *d) {
        const QString suffix = QFileInfo(d->downloadFileName()).suffix().toLower();
        if (!kSongExt.contains(suffix)) {
            notify(tr("Skipped \"%1\": only lyrics, ChordPro and USR downloads can be imported.").arg(d->downloadFileName()));
            d->cancel();
            return;
        }
        d->setDownloadDirectory(dir);
        connect(d, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, d] {
            if (d->state() != QWebEngineDownloadRequest::DownloadCompleted) return;
            const QString path = QDir(d->downloadDirectory()).filePath(d->downloadFileName());
            notify(tr("Imported \"%1\" into your Songs.").arg(QFileInfo(path).completeBaseName()));
            emit songFileReady(path);
        });
        d->accept();
    });
    view->load(kHome);
#else
    auto *title = new QLabel(tr("<b>Get songs from SongSelect</b>"));
    layout->addWidget(title);
    auto *how = new QLabel(tr("SongSelect opens in your web browser. Sign in, find a song and click Download "
                              "(lyrics, ChordPro or USR). While this tab is open, new song files in your "
                              "Downloads folder are imported automatically."));
    how->setWordWrap(true);
    layout->addWidget(how);
    auto *open = new QPushButton(tr("Open SongSelect"));
    open->setProperty("accent", true);
    connect(open, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(kHome); });
    layout->addWidget(open, 0, Qt::AlignLeft);
    layout->addWidget(m_status);
    layout->addStretch();

    m_since = QDateTime::currentDateTime().addSecs(-2);
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_watcher = new QFileSystemWatcher({downloads}, this);
    // Browsers write a temp file and rename it, so give them a moment.
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        QTimer::singleShot(700, this, &SongSelectBrowser::scanDownloads);
    });
    notify(tr("Watching %1").arg(QDir::toNativeSeparators(downloads)));
#endif
}

void SongSelectBrowser::notify(const QString &text)
{
    m_status->setText(text);
}

void SongSelectBrowser::scanDownloads()
{
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files, QDir::Time)) {
        if (fi.lastModified() < m_since) break;   // sorted newest first
        if (!kSongExt.contains(fi.suffix().toLower()) || m_seen.contains(fi.absoluteFilePath())) continue;
        m_seen.insert(fi.absoluteFilePath());
        notify(tr("Imported \"%1\"").arg(fi.completeBaseName()));
        emit songFileReady(fi.absoluteFilePath());
    }
}
