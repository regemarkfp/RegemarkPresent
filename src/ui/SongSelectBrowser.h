#pragma once

#include <QDateTime>
#include <QWidget>
#include <QSet>

class QFileSystemWatcher;
class QLabel;

// SongSelect has no public API for new apps (CCLI retired its partner
// program), so the operator uses the real site with their own CCLI
// subscription, docked in the main window, and we import what they download.
// With Qt WebEngine the site is embedded (persistent sign-in) and downloads
// are captured directly; without it we open the system browser and watch
// the Downloads folder while the panel is in use.
class SongSelectBrowser : public QWidget {
    Q_OBJECT
public:
    explicit SongSelectBrowser(QWidget *parent = nullptr);

    static bool embedded();

    void notify(const QString &text);

signals:
    void songFileReady(const QString &path);

private:
    void scanDownloads();

    QLabel *m_status = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QDateTime m_since;
    QSet<QString> m_seen;
};
