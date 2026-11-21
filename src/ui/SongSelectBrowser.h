#pragma once

#include <QDateTime>
#include <QDialog>
#include <QSet>

class QFileSystemWatcher;
class QLabel;

// SongSelect has no public API, so we let the operator use the real site
// with their own CCLI subscription and import whatever they download.
// With Qt WebEngine the site opens inside the app (persistent sign-in) and
// downloads are captured directly; without it we open the system browser
// and watch the Downloads folder while this window is open.
class SongSelectBrowser : public QDialog {
    Q_OBJECT
public:
    explicit SongSelectBrowser(QWidget *parent = nullptr);

    static bool embedded();

signals:
    void songFileReady(const QString &path);

private:
    void notify(const QString &text);
    void scanDownloads();

    QLabel *m_status = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QDateTime m_since;
    QSet<QString> m_seen;
};
