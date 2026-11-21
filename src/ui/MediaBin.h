#pragma once

#include <QHash>
#include <QImage>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QListWidget;
class QListWidgetItem;
class QMediaPlayer;
class QVideoSink;

// Thumbnail browser over the configured media folders (motion loops and
// stills, e.g. a Sunday Screens download folder).
class MediaBin : public QWidget {
    Q_OBJECT
public:
    explicit MediaBin(QWidget *parent = nullptr);

    void setFolders(const QStringList &folders);
    QStringList folders() const { return m_folders; }
    QString selectedPath() const;
    QStringList selectedPaths() const;

    // Cached still for any media file (first frame for videos); null until ready.
    QImage poster(const QString &path);

signals:
    void foldersChanged(const QStringList &folders);
    void sendLive(const QString &path);
    void applyToSlides(const QString &path);
    void applyToPresentation(const QString &path);
    void createCue(const QStringList &paths);
    void posterReady(const QString &path);

private:
    void rescan();
    void queueVideoThumb(const QString &path);
    void nextVideoThumb();
    void setPoster(const QString &path, const QImage &img);
    QString cachePath(const QString &path) const;

    QStringList m_folders;
    QComboBox *m_folderBox;
    QListWidget *m_list;
    QHash<QString, QImage> m_posters;
    QStringList m_videoQueue;
    QMediaPlayer *m_grabber = nullptr;
    QVideoSink *m_grabSink = nullptr;
    QString m_grabbing;
};
