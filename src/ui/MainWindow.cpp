#include "ui/MainWindow.h"

#include "import/DeckImporter.h"
#include "import/SongImporter.h"
#include "output/OutputEngine.h"
#include "render/TextRenderer.h"
#include "show/ShowController.h"
#include "show/ShowTimer.h"
#include "bible/BibleLibrary.h"
#include "bible/BiblePanel.h"
#include "ui/ArrangementDialog.h"
#include "ui/MediaBin.h"
#include "ui/ScreenSetupDialog.h"
#include "ui/SlideEditor.h"
#include "ui/SongSelectBrowser.h"
#include "ui/TimerPanel.h"
#include "ui/Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

static const QSize kThumb(240, 135);
static const QSize kPreview(272, 153);
static const QStringList kSongExt = {"usr", "txt", "cho", "chordpro", "chopro", "pro"};
static const QStringList kDeckExt = {"pdf", "pptx", "ppt", "odp", "key"};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Regemark Present %1").arg(RP_VERSION));
    setAcceptDrops(true);
    resize(1600, 950);

    m_settings = AppSettings::load();
    QString root = qEnvironmentVariable("RP_LIBRARY_DIR");
    if (root.isEmpty()) root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Regemark Present";
    m_library = new Library(root, this);
    m_show = new ShowController(this);
    m_engine = new OutputEngine(m_show, this);

    buildUi();
    buildActions();

    connect(m_library, &Library::presentationsChanged, this, &MainWindow::refreshLibrary);
    connect(m_library, &Library::playlistsChanged, this, &MainWindow::refreshPlaylists);
    connect(m_show, &ShowController::stateChanged, this, &MainWindow::updateGridIcons);
    // Automatic advances (timed slides, finished videos) that run past the end
    // of an item carry on through the Pre-Service loop.
    connect(m_show, &ShowController::reachedEnd, this, [this] {
        const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
        if (p && m_playlistRow >= 0 && p->segmentAt(m_playlistRow) == "pre") advanceRundown();
    });
    connect(m_engine, &OutputEngine::liveVideoFinished, this, [this] { m_show->next(); });
    connect(m_media, &MediaBin::foldersChanged, this, [this](const QStringList &f) {
        m_settings.mediaFolders = f;
        m_settings.save();
    });
    connect(m_media, &MediaBin::sendLive, this, [this](const QString &path) {
        m_show->setLiveBackground(Background::media(path));
    });
    connect(m_media, &MediaBin::applyToSlides, this, &MainWindow::applyMediaToSlides);
    connect(m_media, &MediaBin::applyToPresentation, this, &MainWindow::applyMediaToPresentation);
    connect(m_media, &MediaBin::createCue, this, &MainWindow::createMediaCue);
    connect(m_media, &MediaBin::posterReady, this, [this] {
        if (m_hasCurrent) refreshGrid();
    });

    m_library->load();
    Presentation scriptureTemplate;
    applyDefaultLook(scriptureTemplate);
    m_biblePanel->setTemplate(scriptureTemplate);
    m_media->setFolders(m_settings.mediaFolders);
    m_engine->configure(m_settings.screens);
    buildPreviews();

    auto *previewTimer = new QTimer(this);
    connect(previewTimer, &QTimer::timeout, this, &MainWindow::refreshPreviews);
    previewTimer->start(100);
}

MainWindow::~MainWindow() = default;

// ---- UI construction ------------------------------------------------------

static QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setProperty("role", "section");
    return l;
}

static QWidget *panel(QBoxLayout *layout)
{
    auto *w = new QWidget;
    w->setLayout(layout);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    return w;
}

void MainWindow::buildUi()
{
    auto *split = new QSplitter;
    split->setHandleWidth(4);
    setCentralWidget(split);

    // ---- Left: library tabs (songs, later Bible) + playlist --------------
    auto *left = new QSplitter(Qt::Vertical);
    left->setHandleWidth(4);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search title, words, author or CCLI #"));
    m_search->setClearButtonEnabled(true);

    // Songs and slide decks live in separate tabs but behave the same way.
    auto makeList = [this] {
        auto *list = new QListWidget;
        list->setDragEnabled(true);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list, &QListWidget::customContextMenuRequested, this, [this, list](const QPoint &pos) {
            if (!list->itemAt(pos)) return;
            QMenu menu;
            QMenu *looks = menu.addMenu(tr("Apply look"));
            for (const Look &l : m_library->looks())
                looks->addAction(l.name, this, [this, l] { applyLookToSelection(l); });
            if (m_library->looks().isEmpty()) looks->addAction(tr("(save one from the editor's Presentation tab)"))->setEnabled(false);
            menu.addAction(tr("Add to playlist"), this, [this, list] {
                for (QListWidgetItem *it : list->selectedItems()) addToCurrentPlaylist(it->data(Qt::UserRole).toString());
            });
            menu.addSeparator();
            menu.addAction(tr("Delete"), this, [this] { deletePresentation(); });
            menu.exec(list->viewport()->mapToGlobal(pos));
        });
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            showPresentation(item->data(Qt::UserRole).toString());
        });
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            addToCurrentPlaylist(item->data(Qt::UserRole).toString());
        });
        return list;
    };
    m_libList = makeList();
    m_deckList = makeList();

    auto *decksL = new QVBoxLayout;
    decksL->addWidget(m_deckList, 1);
    auto *newDeck = new QPushButton(tr("+ New presentation"));
    newDeck->setToolTip(tr("A slide deck: sermon points, announcements, photos and videos"));
    connect(newDeck, &QPushButton::clicked, this, [this] { newDeckPresentation(); });
    decksL->addWidget(newDeck);
    auto *decksPanel = panel(decksL);
    decksL->setContentsMargins(0, 8, 0, 0);
    auto *songsL = new QVBoxLayout;
    songsL->addWidget(m_libList, 1);
    auto *songsPanel = panel(songsL);
    songsL->setContentsMargins(0, 8, 0, 0);

    m_leftTabs = new QTabWidget;
    m_leftTabs->addTab(songsPanel, tr("Songs"));
    m_leftTabs->addTab(decksPanel, tr("Presentations"));
    m_bibles = new BibleLibrary(this);
    m_biblePanel = new BiblePanel(m_bibles);
    m_leftTabs->addTab(m_biblePanel, tr("Bible"));
    connect(m_biblePanel, &BiblePanel::presentationReady, this, [this](const Presentation &p) {
        showUnsaved(p);
    });
    connect(m_biblePanel, &BiblePanel::addToPlaylist, this, [this](const Presentation &p) {
        Presentation saved = p;
        saved.id = newId();
        m_library->save(saved);
        if (!currentPlaylist()) {
            QMessageBox::information(this, tr("Playlist"), tr("Saved to Presentations. Create a playlist to add it to one."));
            return;
        }
        addToCurrentPlaylist(saved.id);
        statusBar()->showMessage(tr("Added %1 to the playlist").arg(saved.title), 5000);
    });
    // The search box doesn't apply to the Bible tab, which has its own reference field.
    // SongSelect tab: the site itself fills the centre; this side explains.
    auto *ssL = new QVBoxLayout;
    auto *ssInfo = new QLabel(tr("<b>SongSelect</b><br>Search and download in the main area with your church's "
                                 "CCLI account. Each download goes straight into Songs."));
    ssInfo->setWordWrap(true);
    ssInfo->setProperty("role", "hint");
    ssL->addWidget(ssInfo);
    m_songSelectLog = new QListWidget;
    m_songSelectLog->setToolTip(tr("Songs imported from SongSelect this session. Click one to open it."));
    ssL->addWidget(sectionLabel(tr("Imported")));
    ssL->addWidget(m_songSelectLog, 1);
    auto *ssBack = new QPushButton(tr("Back to my songs"));
    ssL->addWidget(ssBack);
    auto *ssPanel = panel(ssL);
    ssL->setContentsMargins(0, 8, 0, 0);
    m_leftTabs->addTab(ssPanel, tr("SongSelect"));
    connect(ssBack, &QPushButton::clicked, this, [this] { m_leftTabs->setCurrentIndex(0); });
    connect(m_songSelectLog, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        m_leftTabs->setCurrentIndex(0);
        showPresentation(item->data(Qt::UserRole).toString());
    });

    connect(m_leftTabs, &QTabWidget::currentChanged, this, [this](int i) {
        m_search->setVisible(i < 2);
        if (i == 3) {
            ensureSongSelect();
            m_centerStack->setCurrentWidget(m_songSelect);
        } else {
            m_centerStack->setCurrentIndex(0);
        }
    });
    auto *libL = new QVBoxLayout;
    libL->addWidget(m_search);
    libL->addWidget(m_leftTabs, 1);
    left->addWidget(panel(libL));

    auto *plL = new QVBoxLayout;
    auto *plTop = new QHBoxLayout;
    plTop->addWidget(sectionLabel(tr("Rundown")));
    plTop->addStretch();
    auto *plNew = new QToolButton;
    plNew->setText(tr("New"));
    plNew->setToolTip(tr("Start a new service rundown"));
    auto *plDel = new QToolButton;
    plDel->setText(tr("Delete"));
    plTop->addWidget(plNew);
    plTop->addWidget(plDel);
    plL->addLayout(plTop);
    m_playlistBox = new QComboBox;
    plL->addWidget(m_playlistBox);
    m_playlist = new QListWidget;
    m_playlist->setObjectName("playlist");
    m_playlist->setDragDropMode(QAbstractItemView::InternalMove);
    m_playlist->setContextMenuPolicy(Qt::CustomContextMenu);
    plL->addWidget(m_playlist, 1);
    auto *plBtns = new QHBoxLayout;
    auto *plAdd = new QPushButton(tr("+ Add item"));
    plAdd->setProperty("accent", true);
    plAdd->setToolTip(tr("Add a song, presentation, media, scripture or countdown to the selected segment"));
    auto *plRemove = new QPushButton(tr("Remove"));
    plBtns->addWidget(plAdd, 2);
    plBtns->addWidget(plRemove, 1);
    plL->addLayout(plBtns);
    left->addWidget(panel(plL));
    left->setSizes({520, 420});
    split->addWidget(left);

    // ---- Centre: slide grid -------------------------------------------------
    auto *cL = new QVBoxLayout;
    auto *head = new QHBoxLayout;
    m_presTitle = new QLabel(tr("Pick a song from the library or playlist"));
    m_presTitle->setProperty("role", "heading");
    QFont tf = m_presTitle->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.35);
    tf.setBold(true);
    m_presTitle->setFont(tf);
    head->addWidget(m_presTitle, 1);
    m_arrangementBox = new QComboBox;
    m_arrangementBox->setToolTip(tr("Song order: play the groups in a different order without duplicating slides"));
    m_arrangementBox->setMinimumWidth(200);
    m_arrangementBox->setEnabled(false);
    head->addWidget(m_arrangementBox);
    auto *arrangeBtn = new QToolButton;
    arrangeBtn->setText(tr("Edit order"));
    head->addWidget(arrangeBtn);
    connect(arrangeBtn, &QToolButton::clicked, this, [this] { editArrangement(m_current.activeArrangement); });
    connect(m_arrangementBox, &QComboBox::activated, this, [this](int i) {
        if (!m_hasCurrent) return;
        if (i == m_arrangementBox->count() - 1) {
            editArrangement(-1);
            return;
        }
        m_current.activeArrangement = i - 1;
        saveCurrent();
    });
    auto *editBtn = new QPushButton(tr("Edit"));
    editBtn->setToolTip(tr("Open the slide editor (Ctrl+E)"));
    head->addWidget(editBtn);
    cL->addLayout(head);
    m_grid = new QListWidget;
    m_grid->setObjectName("slideGrid");
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(kThumb + QSize(0, 24));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setSpacing(8);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setContextMenuPolicy(Qt::ActionsContextMenu);
    cL->addWidget(m_grid, 1);
    // The centre shows the slide grid, or SongSelect while its tab is open.
    m_centerStack = new QStackedWidget;
    m_centerStack->addWidget(panel(cL));
    split->addWidget(m_centerStack);
    connect(editBtn, &QPushButton::clicked, this, [this] { editPresentation(qMax(0, m_grid->currentRow())); });

    // ---- Right: live outputs + clear buttons over the media bin -------------
    auto *outL = new QVBoxLayout;
    outL->addWidget(sectionLabel(tr("Live outputs")));
    m_previewHost = new QWidget;
    auto *previewScroll = new QScrollArea;
    previewScroll->setWidget(m_previewHost);
    previewScroll->setWidgetResizable(true);
    previewScroll->setFrameShape(QFrame::NoFrame);
    previewScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outL->addWidget(previewScroll, 1);
    auto *clearRow = new QHBoxLayout;
    auto *clearAll = new QPushButton(tr("Clear all\nF1"));
    auto *clearBg = new QPushButton(tr("Background\nF2"));
    auto *clearText = new QPushButton(tr("Text\nF3"));
    for (QPushButton *b : {clearAll, clearBg, clearText}) {
        b->setMinimumHeight(46);
        clearRow->addWidget(b);
    }
    clearAll->setProperty("danger", true);
    clearBg->setToolTip(tr("Clear the background only"));
    clearText->setToolTip(tr("Clear the slide text only"));
    outL->addLayout(clearRow);
    outL->addSpacing(4);
    outL->addWidget(sectionLabel(tr("Timer")));
    auto *timerPanel = new TimerPanel;
    outL->addWidget(timerPanel);
    connect(timerPanel, &TimerPanel::countdownSlideRequested, this, &MainWindow::showCountdownSlide);

    auto *mediaL = new QVBoxLayout;
    auto *mediaHead = new QHBoxLayout;
    mediaHead->addWidget(sectionLabel(tr("Media")));
    auto *mediaHint = new QLabel(tr("double-click to send live"));
    mediaHint->setProperty("role", "hint");
    mediaHead->addWidget(mediaHint);
    mediaHead->addStretch();
    mediaL->addLayout(mediaHead);
    m_media = new MediaBin;
    mediaL->addWidget(m_media, 1);

    auto *right = new QSplitter(Qt::Vertical);
    right->setHandleWidth(4);
    right->addWidget(panel(outL));
    right->addWidget(panel(mediaL));
    right->setSizes({620, 380});
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    split->setSizes({320, 900, 380});

    // Wiring
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshLibrary);
    connect(m_playlist, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty()) showPresentation(id, m_playlist->row(item));
    });
    connect(m_playlistBox, &QComboBox::currentIndexChanged, this, &MainWindow::refreshPlaylistItems);
    connect(plNew, &QToolButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("New rundown"), tr("Name:"), QLineEdit::Normal,
                                                   QLocale().toString(QDate::currentDate(), "dddd d MMMM"));
        if (name.isEmpty()) return;
        const Playlist p = Playlist::create(name);
        m_library->savePlaylist(p);
        m_playlistBox->setCurrentIndex(m_playlistBox->findData(p.id));
    });
    connect(plDel, &QToolButton::clicked, this, [this] {
        Playlist *p = currentPlaylist();
        if (p && QMessageBox::question(this, tr("Delete rundown"), tr("Delete \"%1\"?").arg(p->name)) == QMessageBox::Yes)
            m_library->removePlaylist(p->id);
    });
    connect(plAdd, &QPushButton::clicked, this, [this, plAdd] {
        if (!currentPlaylist()) {
            const Playlist p = Playlist::create(QLocale().toString(QDate::currentDate(), "dddd d MMMM"));
            m_library->savePlaylist(p);
            m_playlistBox->setCurrentIndex(m_playlistBox->findData(p.id));
        }
        addItemMenu()->exec(plAdd->mapToGlobal(QPoint(0, plAdd->height())));
    });
    connect(plRemove, &QPushButton::clicked, this, &MainWindow::removeRundownRow);
    connect(new QShortcut(QKeySequence::Delete, m_playlist, nullptr, nullptr, Qt::WidgetShortcut),
            &QShortcut::activated, this, &MainWindow::removeRundownRow);
    connect(m_playlist, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *it = m_playlist->itemAt(pos);
        QMenu menu;
        menu.addMenu(addItemMenu())->setText(tr("Add here"));
        if (it && it->data(Qt::UserRole + 2).toString() == "pre") {
            menu.addAction(tr("Pre-Service: change slides every… (now %1 s)").arg(currentPlaylist()->preServiceAdvanceSec),
                           this, [this] {
                Playlist *p = currentPlaylist();
                bool ok = false;
                const int s = QInputDialog::getInt(this, tr("Pre-Service loop"),
                                                   tr("Seconds per slide (videos play to the end):"),
                                                   p->preServiceAdvanceSec, 2, 600, 1, &ok);
                if (!ok) return;
                p->preServiceAdvanceSec = s;
                m_library->savePlaylist(*p);
            });
        }
        if (it && it->data(Qt::UserRole + 2).toString().isEmpty())
            menu.addAction(tr("Remove"), this, &MainWindow::removeRundownRow);
        menu.exec(m_playlist->viewport()->mapToGlobal(pos));
    });
    connect(m_playlist->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        Playlist *p = currentPlaylist();
        if (!p) return;
        p->items.clear();
        for (int i = 0; i < m_playlist->count(); ++i) {
            QListWidgetItem *it = m_playlist->item(i);
            p->items.append({it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString(),
                             it->data(Qt::UserRole + 2).toString()});
        }
        p->normalize();   // an item dropped above Pre-Service still lands in a segment
        m_library->savePlaylist(*p);
    });

    connect(m_grid, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) return;
        takeSlide(m_grid->row(item));
    });
    auto *editSlide = new QAction(tr("Edit slide…"), m_grid);
    connect(editSlide, &QAction::triggered, this, [this] { editPresentation(qMax(0, m_grid->currentRow())); });
    auto *clearSlideBg = new QAction(tr("Remove background from selected slides"), m_grid);
    connect(clearSlideBg, &QAction::triggered, this, [this] {
        if (!m_hasCurrent) return;
        for (QListWidgetItem *it : m_grid->selectedItems())
            m_current.slides[m_viewMap.value(m_grid->row(it))].background = Background();
        saveCurrent();
    });
    m_grid->addActions({editSlide, clearSlideBg});

    connect(clearAll, &QPushButton::clicked, m_show, &ShowController::clearAll);
    connect(clearBg, &QPushButton::clicked, m_show, &ShowController::clearBackground);
    connect(clearText, &QPushButton::clicked, m_show, &ShowController::clearText);
}

void MainWindow::buildActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QToolBar *tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto *brand = new QLabel;
    brand->setText(QStringLiteral("<img src=':/icon.png' width='22' height='22' style='vertical-align:middle'>"
                                  "&nbsp;&nbsp;<b>Regemark</b> Present&nbsp;&nbsp;&nbsp;"));
    tb->addWidget(brand);
    tb->addSeparator();

    auto add = [&](QMenu *menu, const QString &text, auto slot, const QKeySequence &key = {}, bool toolbar = true) {
        QAction *a = menu->addAction(text, this, slot);
        if (!key.isEmpty()) a->setShortcut(key);
        if (toolbar) tb->addAction(a);
        return a;
    };
    QAction *ss = add(file, tr("SongSelect"), [this] { openSongSelect(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    ss->setToolTip(tr("Find songs on CCLI SongSelect; downloads go straight into your library"));
    add(file, tr("New song…"), [this] { newPresentation(); }, QKeySequence::New);
    add(file, tr("New presentation…"), [this] { newDeckPresentation(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    add(file, tr("Paste lyrics…"), [this] { newFromLyrics(); });
    add(file, tr("Import files…"), [this] { importSongs(); }, QKeySequence(Qt::CTRL | Qt::Key_I));
    file->addSeparator();
    add(file, tr("Edit song…"), [this] { editPresentation(qMax(0, m_grid->currentRow())); }, QKeySequence(Qt::CTRL | Qt::Key_E), false);
    add(file, tr("Delete song"), [this] { deletePresentation(); }, {}, false);
    file->addSeparator();
    add(file, tr("Open library folder"), [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_library->root()));
    }, {}, false);
    add(file, tr("Quit"), [this] { close(); }, QKeySequence::Quit, false);

    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    QMenu *screens = menuBar()->addMenu(tr("&Screens"));
    add(screens, tr("Screens…"), [this] { openScreenSetup(); });
    QAction *outputs = add(screens, tr("● Outputs live"), [this](bool on) { m_engine->setOutputsEnabled(on); },
                           QKeySequence(Qt::CTRL | Qt::Key_O));
    outputs->setCheckable(true);
    outputs->setChecked(true);
    outputs->setToolTip(tr("Turn all physical outputs (monitors, NDI, DeckLink) on or off (Ctrl+O)"));
    connect(outputs, &QAction::toggled, this, [outputs](bool on) {
        outputs->setText(on ? tr("● Outputs live") : tr("○ Outputs off"));
    });

    QMenu *looksMenu = menuBar()->addMenu(tr("&Looks"));
    connect(looksMenu, &QMenu::aboutToShow, this, [this, looksMenu] {
        looksMenu->clear();
        QMenu *def = looksMenu->addMenu(tr("Default look for new songs"));
        auto *group = new QActionGroup(def);
        auto addChoice = [&](const QString &name, const QString &id) {
            QAction *a = def->addAction(name, this, [this, id] {
                m_settings.defaultLookId = id;
                m_settings.save();
            });
            a->setCheckable(true);
            a->setChecked(m_settings.defaultLookId == id);
            group->addAction(a);
        };
        addChoice(tr("None (built-in)"), QString());
        for (const Look &l : m_library->looks()) addChoice(l.name, l.id);
        QMenu *del = looksMenu->addMenu(tr("Delete look"));
        for (const Look &l : m_library->looks())
            del->addAction(l.name, this, [this, l] { m_library->removeLook(l.id); });
        del->setEnabled(!m_library->looks().isEmpty());
        looksMenu->addSeparator();
        looksMenu->addAction(tr("Tip: save a look from the slide editor's Presentation tab; "
                                "right-click songs in the library to apply one."))->setEnabled(false);
    });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Keyboard shortcuts"), this, [this] {
        QMessageBox::information(this, tr("Keyboard shortcuts"),
            tr("Next slide: Space, →, ↓, Page Down\n"
               "Previous slide: ←, ↑, Page Up\n"
               "Clear all: F1\nClear background: F2\nClear text: F3\n"
               "Edit song: Ctrl+E\nImport: Ctrl+I\nOutputs on/off: Ctrl+O\n\n"
               "Tip: drag SongSelect downloads (.usr / .txt / ChordPro) onto the window to import them."));
    });
    help->addAction(tr("About"), this, [this] {
        QMessageBox::about(this, tr("About Regemark Present"),
            tr("Regemark Present %1\n\nNDI® is a registered trademark of Vizrt NDI AB.\n"
               "DeckLink is a trademark of Blackmagic Design Pty. Ltd.\n"
               "SongSelect and CCLI are trademarks of Christian Copyright Licensing International.").arg(RP_VERSION));
    });

    auto shortcut = [this](const QKeySequence &k, auto fn) {
        auto *s = new QShortcut(k, this);
        connect(s, &QShortcut::activated, this, fn);
    };
    for (int key : {Qt::Key_Space, Qt::Key_Right, Qt::Key_Down, Qt::Key_PageDown})
        shortcut(QKeySequence(key), [this] { goNext(); });
    for (int key : {Qt::Key_Left, Qt::Key_Up, Qt::Key_PageUp})
        shortcut(QKeySequence(key), [this] { goPrevious(); });
    shortcut(QKeySequence(Qt::Key_F1), [this] { m_show->clearAll(); });
    shortcut(QKeySequence(Qt::Key_F2), [this] { m_show->clearBackground(); });
    shortcut(QKeySequence(Qt::Key_F3), [this] { m_show->clearText(); });
}

void MainWindow::buildPreviews()
{
    delete m_previewHost->layout();
    qDeleteAll(m_previewHost->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
    m_previews.clear();
    m_previewStatus.clear();

    auto *list = new QVBoxLayout(m_previewHost);
    list->setContentsMargins(0, 0, 0, 0);
    list->setSpacing(8);
    for (int i = 0; i < m_engine->screenCount(); ++i) {
        const ScreenConfig &cfg = m_engine->screenConfig(i);
        auto *card = new QFrame;
        card->setProperty("card", true);
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(4);
        QString detail = QStringLiteral("%1×%2 · %3p").arg(cfg.resolution.width()).arg(cfg.resolution.height()).arg(cfg.fps);
        if (cfg.name != ScreenConfig::roleName(cfg.role)) detail.prepend(ScreenConfig::roleName(cfg.role) + " · ");
        auto *title = new QLabel(QStringLiteral("<b>%1</b>&nbsp;&nbsp;<span style='color:%2'>%3</span>")
                                     .arg(cfg.name.toHtmlEscaped(), Theme::textDim.name(), detail));
        auto *img = new QLabel;
        img->setFixedSize(kPreview);
        img->setStyleSheet(QStringLiteral("border-radius: 4px; background: black;"));
        auto *status = new QLabel;
        status->setProperty("role", "hint");
        status->setWordWrap(true);
        QFont sf = status->font();
        sf.setPointSizeF(sf.pointSizeF() * 0.85);
        status->setFont(sf);
        cl->addWidget(title);
        cl->addWidget(img, 0, Qt::AlignHCenter);
        cl->addWidget(status);
        list->addWidget(card);
        m_previews << img;
        m_previewStatus << status;
    }
}

// ---- Library / playlist ---------------------------------------------------

QListWidget *MainWindow::activeLibraryList() const
{
    return m_leftTabs->currentIndex() == 1 ? m_deckList : m_libList;
}

static bool isSong(const Presentation &p)
{
    return p.category == QLatin1String("Song");
}

void MainWindow::refreshLibrary()
{
    const QString q = m_search->text().trimmed();
    for (QListWidget *list : {m_libList, m_deckList}) {
        const QString current = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString();
        list->clear();
        for (const Presentation &p : m_library->presentations()) {
            if (isSong(p) != (list == m_libList)) continue;
            if (!q.isEmpty()) {
                bool match = p.title.contains(q, Qt::CaseInsensitive) || p.ccliNumber == q
                             || p.author.contains(q, Qt::CaseInsensitive);
                for (int i = 0; !match && i < p.slides.size(); ++i)
                    match = p.slides[i].mainText().contains(q, Qt::CaseInsensitive);
                if (!match) continue;
            }
            QString title = p.title;
            if (list == m_deckList && p.category != QLatin1String("Presentation"))
                title += QStringLiteral("  ·  %1").arg(p.category == QLatin1String("Media") ? tr("media") : p.category.toLower());
            auto *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, p.id);
            item->setToolTip(QStringList{p.author, p.copyright, p.ccliNumber.isEmpty() ? QString() : "CCLI #" + p.ccliNumber}
                                 .filter(QRegularExpression(".")).join('\n'));
            list->addItem(item);
            if (p.id == current) list->setCurrentItem(item);
        }
    }
    refreshPlaylistItems();   // titles may have changed
}

void MainWindow::showUnsaved(const Presentation &p)
{
    // Scripture is shown straight from the Bible tab; it only reaches the
    // library if the operator edits it or adds it to a playlist.
    m_current = p;
    m_hasCurrent = true;
    m_playlistRow = -1;
    m_presTitle->setText(p.title + QStringLiteral("  <span style='color:gray; font-weight:normal'>%1</span>").arg(tr("not saved")));
    refreshArrangements();
    refreshGrid();
}

void MainWindow::newDeckPresentation()
{
    const QString title = QInputDialog::getText(this, tr("New presentation"), tr("Title:"), QLineEdit::Normal,
                                                tr("Sermon"));
    if (title.trimmed().isEmpty()) return;
    Presentation p;
    p.id = newId();
    p.title = title.trimmed();
    p.category = QStringLiteral("Presentation");
    p.defaultTransition.type = TransitionType::Fade;
    p.defaultBackground = Background::solid(QColor(18, 18, 24));
    applyDefaultLook(p);
    // A title slide to start from: big heading plus a smaller line.
    Slide s = p.makeSlide(tr("Slide 1"), p.title);
    s.labelColor = Theme::accent;
    TextBox &heading = s.boxes.first();
    heading.rect = QRectF(0.08, 0.30, 0.84, 0.25);
    heading.style.fontSize = 110;
    heading.style.shadow = false;
    TextBox sub;
    sub.id = newId();
    sub.text = QLocale().toString(QDate::currentDate(), QLocale::LongFormat);
    sub.style = heading.style;
    sub.style.fontSize = 48;
    sub.style.bold = false;
    sub.style.color = QColor(200, 200, 210);
    sub.rect = QRectF(0.08, 0.56, 0.84, 0.10);
    sub.isMain = false;
    s.boxes.append(sub);
    p.slides.append(s);
    m_library->save(p);
    m_leftTabs->setCurrentIndex(1);
    showPresentation(p.id);
    editPresentation(0);
}

void MainWindow::refreshPlaylists()
{
    const QString current = m_playlistBox->currentData().toString();
    {
        const QSignalBlocker block(m_playlistBox);
        m_playlistBox->clear();
        for (const Playlist &p : m_library->playlists()) m_playlistBox->addItem(p.name, p.id);
        const int i = m_playlistBox->findData(current);
        m_playlistBox->setCurrentIndex(i >= 0 ? i : 0);
    }
    refreshPlaylistItems();
}

Playlist *MainWindow::currentPlaylist()
{
    const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
    if (!p) return nullptr;
    m_playlistCache = *p;
    return &m_playlistCache;
}

static QString itemGlyph(const Presentation *p)
{
    if (!p) return QStringLiteral("?");
    if (p->category == QLatin1String("Song")) return QStringLiteral("♪");
    if (p->category == QLatin1String("Media")) return QStringLiteral("▶");
    if (p->category == QLatin1String("Scripture")) return QStringLiteral("✝");
    if (p->category == QLatin1String("Countdown")) return QStringLiteral("⏱");
    return QStringLiteral("▤");
}

void MainWindow::refreshPlaylistItems()
{
    const int row = m_pendingRundownRow >= 0 ? m_pendingRundownRow : m_playlist->currentRow();
    m_pendingRundownRow = -1;
    m_playlist->clear();
    const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
    if (!p) return;
    for (const PlaylistItem &i : p->items) {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, i.presentationId);
        item->setData(Qt::UserRole + 1, i.header);
        item->setData(Qt::UserRole + 2, i.segment);
        QFont f = item->font();
        if (i.isSegment()) {
            // Fixed segments: bold bands that can't be dragged or removed.
            QString text = i.header.toUpper();
            if (i.segment == "pre") text += tr("   ⟳ loops");
            item->setText(text);
            item->setForeground(Theme::accentHover);
            item->setBackground(Theme::raised);
            f.setBold(true);
            item->setFlags((item->flags() | Qt::ItemIsDropEnabled) & ~Qt::ItemIsDragEnabled);
        } else if (i.isSection()) {
            item->setText(QStringLiteral("   ▸ ") + i.header);
            item->setForeground(Theme::text);
            f.setBold(true);
        } else {
            const Presentation *pres = m_library->find(i.presentationId);
            item->setText(QStringLiteral("      %1  %2").arg(itemGlyph(pres), pres ? pres->title : tr("(missing item)")));
        }
        item->setFont(f);
        m_playlist->addItem(item);
    }
    m_playlist->setCurrentRow(qMin(row, m_playlist->count() - 1));
}

int MainWindow::rundownInsertRow() const
{
    // Into the selected segment/section: after a selected item, or at the end
    // of the selected header's block. Nothing selected: end of Service.
    const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
    if (!p) return -1;
    const int row = m_playlist->currentRow();
    if (row < 0 || row >= p->items.size()) return p->segmentEnd("service");
    if (p->items[row].isItem()) return row + 1;
    int r = row + 1;
    while (r < p->items.size() && p->items[r].isItem()) ++r;
    return r;
}

QMenu *MainWindow::addItemMenu()
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(tr("♪  Song…"), this, [this] {
        for (const QString &id : pickFromLibrary(true)) addToCurrentPlaylist(id);
    });
    menu->addAction(tr("▤  Presentation…"), this, [this] {
        for (const QString &id : pickFromLibrary(false)) addToCurrentPlaylist(id);
    });
    menu->addAction(tr("▶  Media (videos, photos)…"), this, [this] {
        QStringList paths = m_media->selectedPaths();
        if (paths.isEmpty())
            paths = QFileDialog::getOpenFileNames(this, tr("Add media"), QString(),
                tr("Photos and videos (*.jpg *.jpeg *.png *.webp *.bmp *.mp4 *.mov *.m4v *.mkv *.avi *.webm *.wmv)"));
        createMediaCue(paths);
    });
    menu->addAction(tr("✝  Scripture…"), this, [this] {
        const QString ref = QInputDialog::getText(this, tr("Scripture"), tr("Reference (e.g. John 3:16-18, Johannes 3:16):"));
        if (ref.trimmed().isEmpty()) return;
        if (!m_biblePanel->showReference(ref)) {
            m_leftTabs->setCurrentIndex(2);   // the Bible tab shows why (no Bible, bad reference...)
            return;
        }
        Presentation p = m_biblePanel->currentPresentation();
        p.id = newId();
        m_library->save(p);
        addToCurrentPlaylist(p.id);
    });
    menu->addAction(tr("⏱  Countdown"), this, [this] { addToCurrentPlaylist(countdownPresentationId()); });
    menu->addSeparator();
    QMenu *section = menu->addMenu(tr("Service section"));
    for (const QString &name : {tr("Welcoming"), tr("Worship"), tr("Announcements"), tr("Offering"), tr("Sermon"),
                                tr("Baptism"), tr("Communion"), tr("Response"), tr("Prayer")})
        section->addAction(name, this, [this, name] { addServiceSection(name); });
    section->addSeparator();
    section->addAction(tr("Other…"), this, [this] {
        const QString name = QInputDialog::getText(this, tr("Service section"), tr("Name:"));
        if (!name.trimmed().isEmpty()) addServiceSection(name.trimmed());
    });
    return menu;
}

QStringList MainWindow::pickFromLibrary(bool songs)
{
    QDialog dlg(this);
    dlg.setWindowTitle(songs ? tr("Add songs") : tr("Add presentations"));
    dlg.resize(460, 560);
    auto *layout = new QVBoxLayout(&dlg);
    auto *search = new QLineEdit;
    search->setPlaceholderText(songs ? tr("Search title, words, author or CCLI #") : tr("Search presentations"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    auto *list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(list, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Add"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    auto fill = [this, list, songs](const QString &q) {
        list->clear();
        for (const Presentation &p : m_library->presentations()) {
            if ((p.category == QLatin1String("Song")) != songs) continue;
            bool match = q.isEmpty() || p.title.contains(q, Qt::CaseInsensitive) || p.ccliNumber == q
                         || p.author.contains(q, Qt::CaseInsensitive);
            for (int i = 0; !match && i < p.slides.size(); ++i)
                match = p.slides[i].mainText().contains(q, Qt::CaseInsensitive);
            if (!match) continue;
            auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(itemGlyph(&p), p.title));
            item->setData(Qt::UserRole, p.id);
            list->addItem(item);
        }
        if (list->count()) list->setCurrentRow(0);
    };
    connect(search, &QLineEdit::textChanged, &dlg, fill);
    fill(QString());
    search->setFocus();
    if (dlg.exec() != QDialog::Accepted) return {};
    QStringList ids;
    for (int i = 0; i < list->count(); ++i)   // keep list order
        if (list->item(i)->isSelected()) ids << list->item(i)->data(Qt::UserRole).toString();
    return ids;
}

void MainWindow::addServiceSection(const QString &name)
{
    Playlist *p = currentPlaylist();
    if (!p) return;
    // Sections only live in Service: inside it at the chosen spot, else at its end.
    int at = rundownInsertRow();
    if (p->segmentAt(qMax(0, at - 1)) != "service") at = p->segmentEnd("service");
    p->items.insert(at, PlaylistItem::section(name));
    m_pendingRundownRow = at;
    m_library->savePlaylist(*p);
}

void MainWindow::removeRundownRow()
{
    Playlist *p = currentPlaylist();
    const int row = m_playlist->currentRow();
    if (!p || row < 0 || row >= p->items.size() || p->items[row].isSegment()) return;
    p->items.removeAt(row);
    m_pendingRundownRow = row - 1;
    m_library->savePlaylist(*p);
}

// ---- Slide grid -----------------------------------------------------------

void MainWindow::showPresentation(const QString &id, int playlistRow)
{
    const Presentation *p = m_library->find(id);
    if (!p) return;
    m_current = *p;
    m_hasCurrent = true;
    m_playlistRow = playlistRow;
    QString title = m_current.title;
    if (!m_current.author.isEmpty()) title += QStringLiteral("  <span style='color:gray; font-weight:normal'>%1</span>").arg(m_current.author.toHtmlEscaped());
    m_presTitle->setText(title);
    refreshArrangements();
    refreshGrid();
}

void MainWindow::refreshArrangements()
{
    const QSignalBlocker block(m_arrangementBox);
    m_arrangementBox->clear();
    m_arrangementBox->addItem(tr("As written"));
    for (const Arrangement &a : std::as_const(m_current.arrangements))
        m_arrangementBox->addItem(QStringLiteral("%1 — %2").arg(a.name, a.groups.join(", ")));
    m_arrangementBox->addItem(tr("New arrangement…"));
    m_arrangementBox->setCurrentIndex(m_current.activeArrangement + 1);
    m_arrangementBox->setEnabled(m_hasCurrent);
}

void MainWindow::editArrangement(int index)
{
    if (!m_hasCurrent) return;
    const Arrangement start = index >= 0 && index < m_current.arrangements.size() ? m_current.arrangements[index]
                                                                                  : Arrangement();
    ArrangementDialog dlg(m_current, start, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (index >= 0 && index < m_current.arrangements.size()) {
            m_current.arrangements[index] = dlg.arrangement();
        } else {
            m_current.arrangements.append(dlg.arrangement());
            index = int(m_current.arrangements.size()) - 1;
        }
        m_current.activeArrangement = index;
        saveCurrent();
    }
    refreshArrangements();
}

void MainWindow::applyLookToSelection(const Look &look)
{
    const auto items = activeLibraryList()->selectedItems();
    for (QListWidgetItem *item : items) {
        const Presentation *found = m_library->find(item->data(Qt::UserRole).toString());
        if (!found) continue;
        Presentation p = *found;
        p.applyLook(look);
        m_library->save(p);
        if (m_hasCurrent && p.id == m_current.id) {
            m_current = p;
            refreshGrid();
            m_show->presentationUpdated(m_view);
        }
    }
    statusBar()->showMessage(tr("Applied \"%1\" to %n song(s)", nullptr, int(items.size())).arg(look.name), 5000);
}

void MainWindow::applyDefaultLook(Presentation &p) const
{
    if (const Look *l = m_library->look(m_settings.defaultLookId)) p.applyLook(*l);
}

void MainWindow::refreshGrid()
{
    const int row = m_grid->currentRow();
    m_grid->clear();
    m_thumbs.clear();
    if (!m_hasCurrent) return;
    m_view = m_current.arranged(&m_viewMap);
    // In Pre-Service everything keeps moving: slides without their own timing
    // advance on the rundown's interval (play-once videos run to their end).
    if (const Playlist *pl = m_library->playlist(m_playlistBox->currentData().toString());
        pl && m_playlistRow >= 0 && pl->segmentAt(m_playlistRow) == "pre") {
        for (int i = 0; i < m_view.slides.size(); ++i) {
            Slide &s = m_view.slides[i];
            const Background bg = m_view.effectiveBackground(i);
            const bool playOnceVideo = bg.type == Background::Type::Video && !bg.loop;
            if (s.advanceMs == 0 && !playOnceVideo) s.advanceMs = pl->preServiceAdvanceSec * 1000;
        }
    }
    for (int i = 0; i < m_view.slides.size(); ++i) {
        const Background bg = m_view.effectiveBackground(i);
        const QImage poster = bg.type == Background::Type::Image || bg.type == Background::Type::Video
                                  ? m_media->poster(bg.path) : QImage();
        m_thumbs << TextRenderer::renderSlide(m_view.slides[i], bg, kThumb, nullptr, poster);
        auto *item = new QListWidgetItem;
        item->setSizeHint(kThumb + QSize(8, 32));
        m_grid->addItem(item);
    }
    m_grid->setCurrentRow(qMin(row, m_grid->count() - 1));
    updateGridIcons();
}

void MainWindow::updateGridIcons()
{
    if (!m_hasCurrent) return;
    const bool isLive = m_show->livePresentationId() == m_current.id;
    for (int i = 0; i < m_thumbs.size() && i < m_grid->count(); ++i) {
        const Slide &s = m_view.slides[i];
        QPixmap pm(kThumb + QSize(0, 24));
        pm.fill(QColor(30, 30, 30));
        QPainter p(&pm);
        p.drawImage(0, 0, m_thumbs[i]);
        const QRect bar(0, kThumb.height(), kThumb.width(), 24);
        p.fillRect(bar, s.labelColor.isValid() ? s.labelColor : Slide::colorForLabel(s.label));
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        p.setFont(f);
        p.drawText(bar.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("%1  %2").arg(i + 1).arg(s.label));
        if (s.background.isSet()) {
            p.drawText(bar.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("◼ BG"));
        }
        if (isLive && i == m_show->liveIndex()) {
            p.setPen(QPen(Theme::accent, 6));
            p.drawRect(pm.rect().adjusted(3, 3, -3, -3));
        }
        m_grid->item(i)->setIcon(QIcon(pm));
    }
    if (isLive && m_show->liveIndex() >= 0) m_grid->scrollToItem(m_grid->item(m_show->liveIndex()));
}

void MainWindow::refreshPreviews()
{
    for (int i = 0; i < m_previews.size() && i < m_engine->screenCount(); ++i) {
        const QImage frame = m_engine->lastFrame(i);
        QPixmap pm(kPreview);
        QPainter p(&pm);
        if (m_engine->screenConfig(i).look.transparent) {
            for (int x = 0; x < kPreview.width(); x += 12)
                for (int y = 0; y < kPreview.height(); y += 12)
                    p.fillRect(x, y, 12, 12, ((x + y) / 12) % 2 ? QColor(60, 60, 60) : QColor(90, 90, 90));
        } else {
            p.fillRect(pm.rect(), Qt::black);
        }
        if (!frame.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            QSizeF s = frame.size();
            s.scale(kPreview, Qt::KeepAspectRatio);
            p.drawImage(QRectF((kPreview.width() - s.width()) / 2, (kPreview.height() - s.height()) / 2, s.width(), s.height()), frame);
        }
        p.end();
        m_previews[i]->setPixmap(pm);
        const QStringList st = m_engine->status(i);
        m_previewStatus[i]->setText(st.isEmpty() ? tr("Preview only. Choose where this screen goes in Screens…") : st.join(" · "));
    }
}

// ---- Show control ---------------------------------------------------------

void MainWindow::takeSlide(int index)
{
    if (!m_hasCurrent) return;
    m_show->take(m_view, index);
}

void MainWindow::goNext()
{
    const bool liveIsShown = m_hasCurrent && m_show->livePresentationId() == m_current.id;
    if (!liveIsShown) {
        // Nothing of this song is live yet: start it.
        if (m_hasCurrent && !m_view.slides.isEmpty()) takeSlide(0);
        return;
    }
    if (m_show->liveIndex() + 1 < m_view.slides.size() || m_current.loop) {
        m_show->next();
        return;
    }
    advanceRundown();
}

void MainWindow::advanceRundown()
{
    // End of an item: continue with the next one in the rundown. Pre-Service
    // loops: after its last item it starts again at its first.
    const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
    if (m_playlistRow < 0 || !p) return;
    const bool pre = p->segmentAt(m_playlistRow) == "pre";
    const int end = pre ? p->segmentEnd("pre") : int(p->items.size());
    auto takeRow = [&](int r) {
        m_playlist->setCurrentRow(r);
        showPresentation(p->items[r].presentationId, r);
        if (!m_view.slides.isEmpty()) takeSlide(0);
    };
    for (int r = m_playlistRow + 1; r < end; ++r)
        if (p->items[r].isItem()) return takeRow(r);
    if (pre)
        for (int r = p->segmentRow("pre") + 1; r < end; ++r)
            if (p->items[r].isItem()) return takeRow(r);
}

void MainWindow::goPrevious()
{
    if (m_hasCurrent && m_show->livePresentationId() == m_current.id) m_show->previous();
}

// ---- Editing --------------------------------------------------------------

void MainWindow::saveCurrent()
{
    if (!m_hasCurrent) return;
    m_library->save(m_current);
    refreshGrid();
    m_show->presentationUpdated(m_view);
}

void MainWindow::newPresentation()
{
    const QString title = QInputDialog::getText(this, tr("New song"), tr("Title:"));
    if (title.trimmed().isEmpty()) return;
    Presentation p;
    p.id = newId();
    p.title = title.trimmed();
    applyDefaultLook(p);
    p.slides.append(p.makeSlide(tr("Verse 1"), QString()));
    m_library->save(p);
    showPresentation(p.id);
    editPresentation(0);
}

void MainWindow::newFromLyrics()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("New song from lyrics"));
    dlg.resize(560, 640);
    auto *l = new QVBoxLayout(&dlg);
    l->addWidget(new QLabel(tr("Paste lyrics. First line is the title; blank lines separate sections; "
                               "lines like \"Verse 1\" or \"Chorus\" become group labels. "
                               "SongSelect's copied lyrics work as-is.")));
    auto *edit = new QPlainTextEdit;
    l->addWidget(edit, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    l->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const SongImporter::Song song = SongImporter::parseSongSelectText(edit->toPlainText());
    if (song.sections.isEmpty()) return;
    Presentation templ;
    applyDefaultLook(templ);
    const Presentation p = SongImporter::toPresentation(song, m_settings.linesPerSlide, templ);
    m_library->save(p);
    showPresentation(p.id);
}

void MainWindow::importSongs(const QStringList &given)
{
    QStringList files = given;
    if (files.isEmpty()) {
        QStringList filters = {tr("Songs and presentations (*.usr *.txt *.cho *.chordpro *.chopro *.pro *.pdf *.pptx *.ppt *.odp *.key)")};
        filters += SongImporter::fileFilters();
        filters += DeckImporter::fileFilters();
        files = QFileDialog::getOpenFileNames(this, tr("Import songs or presentations"), m_settings.lastImportDir,
                                              filters.join(";;"));
        if (files.isEmpty()) return;
    }
    // PDFs and PowerPoints become slide decks; everything else is a song.
    QStringList decks;
    for (const QString &f : files)
        if (kDeckExt.contains(QFileInfo(f).suffix().toLower())) decks << f;
    for (const QString &f : decks) files.removeAll(f);
    if (!decks.isEmpty()) importDecks(decks);
    if (files.isEmpty()) return;
    bool ok = false;
    const int lines = QInputDialog::getInt(this, tr("Import songs"),
                                           tr("Lines per slide (0 = whole section on one slide):"),
                                           m_settings.linesPerSlide, 0, 12, 1, &ok);
    if (!ok) return;
    m_settings.linesPerSlide = lines;
    m_settings.lastImportDir = QFileInfo(files.first()).absolutePath();
    m_settings.save();
    importFiles(files, false);
}

void MainWindow::importDecks(const QStringList &files)
{
    QStringList errors;
    QString lastId;
    for (const QString &f : files) {
        statusBar()->showMessage(tr("Importing %1…").arg(QFileInfo(f).fileName()));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();
        Presentation p;
        QString err;
        const bool ok = DeckImporter::importFile(f, m_library->root() + "/Media", &p, &err);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            errors << QStringLiteral("%1: %2").arg(QFileInfo(f).fileName(), err);
            continue;
        }
        m_library->save(p);
        lastId = p.id;
    }
    if (!lastId.isEmpty()) {
        m_leftTabs->setCurrentIndex(1);
        showPresentation(lastId);
        statusBar()->showMessage(tr("Imported %1 of %2 presentations").arg(files.size() - errors.size()).arg(files.size()), 6000);
    } else {
        statusBar()->clearMessage();
    }
    if (!errors.isEmpty()) QMessageBox::warning(this, tr("Import"), errors.join('\n'));
}

void MainWindow::importFiles(const QStringList &files, bool quiet)
{
    QStringList errors;
    QString lastId;
    int added = 0;
    Presentation templ;
    applyDefaultLook(templ);
    for (const QString &f : files) {
        SongImporter::Song song;
        QString err;
        if (!SongImporter::parseFile(f, &song, &err)) {
            errors << err;
            continue;
        }
        // Downloading the same SongSelect song twice shouldn't duplicate it.
        if (!song.ccliNumber.isEmpty()) {
            QString existing;
            for (const Presentation &p : m_library->presentations())
                if (p.ccliNumber == song.ccliNumber) existing = p.id;
            if (!existing.isEmpty()) {
                lastId = existing;
                continue;
            }
        }
        const Presentation p = SongImporter::toPresentation(song, m_settings.linesPerSlide, templ);
        m_library->save(p);
        lastId = p.id;
        ++added;
    }
    if (!lastId.isEmpty()) showPresentation(lastId);
    if (added == 0 && !lastId.isEmpty())
        statusBar()->showMessage(tr("Already in your library: %1").arg(m_current.title), 6000);
    else
        statusBar()->showMessage(tr("Imported %1 of %2 songs").arg(added).arg(files.size()), 6000);
    if (!errors.isEmpty() && !quiet) QMessageBox::warning(this, tr("Import"), errors.join('\n'));
}

void MainWindow::openSongSelect()
{
    m_leftTabs->setCurrentIndex(3);
}

void MainWindow::ensureSongSelect()
{
    // Created on first use: the embedded browser is heavy and needs the network.
    if (m_songSelect) return;
    m_songSelect = new SongSelectBrowser;
    m_centerStack->addWidget(m_songSelect);
    connect(m_songSelect, &SongSelectBrowser::songFileReady, this, [this](const QString &path) {
        importFiles({path}, true);
        if (!m_hasCurrent) return;
        // Newest first, without repeats, so the operator can jump to what they just got.
        for (int i = 0; i < m_songSelectLog->count(); ++i)
            if (m_songSelectLog->item(i)->data(Qt::UserRole).toString() == m_current.id) delete m_songSelectLog->takeItem(i);
        auto *item = new QListWidgetItem(m_current.title);
        item->setData(Qt::UserRole, m_current.id);
        m_songSelectLog->insertItem(0, item);
    });
}

void MainWindow::editPresentation(int slideIndex)
{
    if (!m_hasCurrent) return;
    // The editor works on the slides as written; map the grid row back.
    SlideEditor editor(m_current, m_viewMap.value(slideIndex, slideIndex), m_media, m_library, this);
    if (editor.exec() != QDialog::Accepted) return;
    m_current = editor.presentation();
    m_presTitle->setText(m_current.title);
    saveCurrent();
}

void MainWindow::deletePresentation()
{
    if (!m_hasCurrent) return;
    if (QMessageBox::question(this, tr("Delete song"),
                              tr("Delete \"%1\" from the library? This removes it from playlists too.").arg(m_current.title))
        != QMessageBox::Yes)
        return;
    for (Playlist p : m_library->playlists()) {
        const auto before = p.items.size();
        p.items.erase(std::remove_if(p.items.begin(), p.items.end(),
                                     [&](const PlaylistItem &i) { return i.presentationId == m_current.id; }),
                      p.items.end());
        if (p.items.size() != before) m_library->savePlaylist(p);
    }
    m_library->remove(m_current.id);
    m_hasCurrent = false;
    m_grid->clear();
    m_presTitle->clear();
}

void MainWindow::applyMediaToSlides(const QString &path)
{
    if (!m_hasCurrent) return;
    const auto selected = m_grid->selectedItems();
    if (selected.isEmpty()) {
        statusBar()->showMessage(tr("Select one or more slides first"), 4000);
        return;
    }
    for (QListWidgetItem *it : selected)
        m_current.slides[m_viewMap.value(m_grid->row(it))].background = Background::media(path);
    saveCurrent();
}

void MainWindow::applyMediaToPresentation(const QString &path)
{
    if (!m_hasCurrent) return;
    m_current.defaultBackground = Background::media(path);
    saveCurrent();
}

void MainWindow::addToCurrentPlaylist(const QString &presentationId)
{
    Playlist *p = currentPlaylist();
    if (!p || presentationId.isEmpty()) return;
    const int at = qBound(0, rundownInsertRow(), int(p->items.size()));
    p->items.insert(at, PlaylistItem::item(presentationId));
    m_pendingRundownRow = at;   // keep adding after what was just added
    m_library->savePlaylist(*p);
}

void MainWindow::createMediaCue(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    bool anyVideo = false;
    for (const QString &path : paths)
        anyVideo |= Background::media(path).type == Background::Type::Video;
    const bool several = paths.size() > 1;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("New media cue"));
    auto *form = new QFormLayout(&dlg);
    auto *name = new QLineEdit(several ? tr("Announcements") : QFileInfo(paths.first()).completeBaseName());
    form->addRow(tr("Name"), name);
    auto *sound = new QCheckBox(tr("Play video sound"));
    sound->setChecked(anyVideo);
    sound->setEnabled(anyVideo);
    form->addRow(sound);
    auto *advance = new QCheckBox(tr("Go to the next item automatically"));
    advance->setChecked(several);
    auto *seconds = new QSpinBox;
    seconds->setRange(1, 600);
    seconds->setValue(8);
    seconds->setSuffix(tr(" s"));
    auto *advRow = new QHBoxLayout;
    advRow->addWidget(advance);
    advRow->addWidget(seconds);
    form->addRow(advRow);
    auto *loop = new QCheckBox(tr("Loop (start again after the last item)"));
    loop->setChecked(several);
    form->addRow(loop);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    Presentation p;
    p.id = newId();
    p.title = name->text().trimmed().isEmpty() ? tr("Media") : name->text().trimmed();
    p.category = QStringLiteral("Media");
    p.loop = loop->isChecked();
    p.defaultTransition.type = TransitionType::Fade;
    p.defaultTransition.durationMs = 600;
    for (const QString &path : paths) {
        Slide s;
        s.id = newId();
        s.label = QFileInfo(path).completeBaseName();
        s.labelColor = QColor("#2a9d8f");
        s.background = Background::media(path);
        s.background.audio = sound->isChecked() && s.background.type == Background::Type::Video;
        s.background.loop = !s.background.audio;   // a video with sound plays once
        s.background.fit = FitMode::Fit;
        if (advance->isChecked()) s.advanceMs = seconds->value() * 1000;
        p.slides.append(s);
    }
    m_library->save(p);
    addToCurrentPlaylist(p.id);
    showPresentation(p.id);
    statusBar()->showMessage(tr("Created media cue \"%1\"").arg(p.title), 5000);
}

QString MainWindow::countdownPresentationId()
{
    QString id;
    for (const Presentation &p : m_library->presentations())
        if (p.category == QLatin1String("Countdown")) id = p.id;
    if (id.isEmpty()) {
        Presentation p;
        p.id = newId();
        p.title = tr("Countdown");
        p.category = QStringLiteral("Countdown");
        Slide s = p.makeSlide(tr("Countdown"), tr("Service starts in") + QStringLiteral("\n{timer}"));
        s.labelColor = Theme::accent;
        p.slides.append(s);
        m_library->save(p);
        id = p.id;
    }
    return id;
}

void MainWindow::showCountdownSlide()
{
    const QString id = countdownPresentationId();
    if (!ShowTimer::instance()->isSet()) {
        statusBar()->showMessage(tr("Start the timer first; the slide shows its countdown."), 6000);
    }
    showPresentation(id);
    takeSlide(0);
}

void MainWindow::openScreenSetup()
{
    ScreenSetupDialog dlg(m_settings.screens, this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_settings.screens = dlg.screens();
    m_settings.save();
    m_engine->configure(m_settings.screens);
    buildPreviews();
}

// ---- Window events --------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    for (const QUrl &u : e->mimeData()->urls())
        if (kSongExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower())
            || kDeckExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower())) {
            e->acceptProposedAction();
            return;
        }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList files;
    for (const QUrl &u : e->mimeData()->urls())
        if (kSongExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower())
            || kDeckExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower()))
            files << u.toLocalFile();
    if (!files.isEmpty()) importSongs(files);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_show->state().hasSlide
        && QMessageBox::question(this, tr("Quit"), tr("Something is live on the outputs. Quit anyway?")) != QMessageBox::Yes) {
        e->ignore();
        return;
    }
    m_engine->setOutputsEnabled(false);
    e->accept();
    qApp->quit();
}
