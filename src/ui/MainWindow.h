#pragma once

#include "core/AppSettings.h"
#include "core/Library.h"

#include <QMainWindow>

class BibleLibrary;
class BiblePanel;
class MediaBin;
class OutputEngine;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTabWidget;
class ShowController;
class SongSelectBrowser;

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
    void importFiles(const QStringList &files, bool quiet);
    void openSongSelect();
    void editPresentation(int slideIndex = 0);
    void deletePresentation();
    void saveCurrent();
    void applyMediaToSlides(const QString &path);
    void applyMediaToPresentation(const QString &path);
    void openScreenSetup();
    void addToCurrentPlaylist(const QString &presentationId);
    void createMediaCue(const QStringList &paths);
    void showCountdownSlide();
    void newDeckPresentation();
    void showUnsaved(const Presentation &p);
    QListWidget *activeLibraryList() const;
    void refreshArrangements();
    void editArrangement(int index);
    void applyLookToSelection(const Look &look);
    void applyDefaultLook(Presentation &p) const;

    Playlist *currentPlaylist();

    AppSettings m_settings;
    Library *m_library;
    ShowController *m_show;
    OutputEngine *m_engine;

    QTabWidget *m_leftTabs;
    QLineEdit *m_search;
    QListWidget *m_libList;          // songs
    QListWidget *m_deckList;         // presentations, media cues, countdown
    BibleLibrary *m_bibles;
    BiblePanel *m_biblePanel;
    QComboBox *m_playlistBox;
    QListWidget *m_playlist;
    Playlist m_playlistCache;

    QLabel *m_presTitle;
    QListWidget *m_grid;
    QList<QImage> m_thumbs;
    Presentation m_current;         // as stored
    Presentation m_view;            // in playing order (active arrangement)
    QList<int> m_viewMap;           // view slide -> m_current slide
    QComboBox *m_arrangementBox;
    bool m_hasCurrent = false;
    int m_playlistRow = -1;

    QWidget *m_previewHost;
    QList<QLabel *> m_previews;
    QList<QLabel *> m_previewStatus;
    MediaBin *m_media;
    SongSelectBrowser *m_songSelect = nullptr;
};
