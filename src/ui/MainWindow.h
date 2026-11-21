#pragma once

#include "core/AppSettings.h"
#include "core/Library.h"

#include <QMainWindow>

class MediaBin;
class OutputEngine;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class ShowController;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    void buildUi();
    void buildActions();
    void buildPreviews();

    void refreshLibrary();
    void refreshPlaylists();
    void refreshPlaylistItems();
    void showPresentation(const QString &id, int playlistRow = -1);
    void refreshGrid();
    void updateGridIcons();
    void refreshPreviews();

    void takeSlide(int index);
    void goNext();
    void goPrevious();

    void newPresentation();
    void newFromLyrics();
    void importSongs(const QStringList &files = {});
    void editPresentation(int slideIndex = 0);
    void deletePresentation();
    void saveCurrent();
    void applyMediaToSlides(const QString &path);
    void applyMediaToPresentation(const QString &path);
    void openScreenSetup();

    Playlist *currentPlaylist();

    AppSettings m_settings;
    Library *m_library;
    ShowController *m_show;
    OutputEngine *m_engine;

    QLineEdit *m_search;
    QListWidget *m_libList;
    QComboBox *m_playlistBox;
    QListWidget *m_playlist;
    Playlist m_playlistCache;

    QLabel *m_presTitle;
    QListWidget *m_grid;
    QList<QImage> m_thumbs;
    Presentation m_current;
    bool m_hasCurrent = false;
    int m_playlistRow = -1;

    QWidget *m_previewHost;
    QList<QLabel *> m_previews;
    QList<QLabel *> m_previewStatus;
    MediaBin *m_media;
};
