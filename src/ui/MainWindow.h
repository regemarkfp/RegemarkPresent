#pragma once

#include "core/AppSettings.h"
#include "core/Library.h"
#include "render/ScreenRenderer.h"

#include <QMainWindow>

class BibleLibrary;
class BiblePanel;
class MediaBin;
class OutputEngine;
class QComboBox;
class QDialog;
class QPushButton;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QStackedWidget;
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void buildActions();
    void buildPreviews();

    void refreshLibrary();
    void refreshPlaylists();
    void refreshPlaylistItems();
    void showPresentation(const QString &id, int playlistRow = -1);
    void refreshGrid();
    void sizeGridHeaders();
    QList<QImage> slideThumbs(const Presentation &view);
    int viewSlideForRow(int row) const;
    int focusGridRow(int row);
    void updateGridIcons();
    void refreshPreviews();

    void takeSlide(int index);
    void goNext();
    void goPrevious();

    QString newPresentation();
    QString newFromLyrics();
    void importSongs(const QStringList &files = {});
    void importFiles(const QStringList &files, bool quiet);
    QStringList importDecks(const QStringList &files);
    void openSongSelect();
    void ensureRundown();
    void ensureSongSelect();
    void editPresentation(int slideIndex = 0);
    void deletePresentation();
    void saveCurrent();
    void applyMediaToSlides(const QString &path);
    void applyMediaToPresentation(const QString &path);
    void openScreenSetup();
    void addToCurrentPlaylist(const QString &presentationId);
    void createMediaCue(const QStringList &paths);
    void showCountdownSlide();
    QString countdownPresentationId();
    void advanceRundown();
    int rundownInsertRow() const;
    QIcon rundownThumb(const Presentation &p);
    QMenu *addItemMenu();
    QStringList pickFromLibrary(bool songs);
    void addServiceSection(const QString &name);
    void removeRundownRow();
    QString newDeckPresentation();
    void showUnsaved(const Presentation &p);
    void refreshLookBox();
    void applyActiveLook();
    void openLooks();
    LiveState sampleState() const;
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

    QTabWidget *m_leftTabs;          // inside the Library window
    QDialog *m_libraryWindow;
    QDialog *m_bibleWindow;
    QDialog *m_mediaWindow;
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
    // One per grid row: a slide of some item, or (slide < 0) an item's title strip.
    struct GridEntry {
        QString presentationId;
        int playlistRow = -1;
        int slide = -1;
        QString header;
        Slide slideData;
    };
    QList<GridEntry> m_gridEntries;
    QList<QImage> m_thumbs;
    QHash<QString, QList<QImage>> m_slideThumbCache;
    bool m_wholeRundown = false;
    int m_liveRundownRow = -1;
    Presentation m_current;         // as stored
    Presentation m_view;            // in playing order (active arrangement)
    QList<int> m_viewMap;           // view slide -> m_current slide
    QComboBox *m_arrangementBox;
    int m_pendingRundownRow = -1;
    QComboBox *m_lookBox = nullptr;
    QHash<QString, QIcon> m_rundownThumbs;   // row to select after the rundown reloads
    bool m_hasCurrent = false;
    int m_playlistRow = -1;

    QWidget *m_previewHost;
    QList<QLabel *> m_previews;
    QList<QLabel *> m_previewStatus;
    MediaBin *m_media;
    SongSelectBrowser *m_songSelect = nullptr;
    QStackedWidget *m_centerStack;
    QWidget *m_songSelectPage = nullptr;
    QLabel *m_songSelectAdded = nullptr;
    QPushButton *m_songSelectAdd = nullptr;
    bool m_addSongSelectToRundown = false;
};
