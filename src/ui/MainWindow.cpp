#include "ui/MainWindow.h"

#include "import/SongImporter.h"
#include "output/OutputEngine.h"
#include "render/TextRenderer.h"
#include "show/ShowController.h"
#include "ui/MediaBin.h"
#include "ui/ScreenSetupDialog.h"
#include "ui/SlideEditor.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

static const QSize kThumb(240, 135);
static const QSize kPreview(288, 162);
static const QStringList kSongExt = {"usr", "txt", "cho", "chordpro", "chopro", "pro"};

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
    connect(m_media, &MediaBin::foldersChanged, this, [this](const QStringList &f) {
        m_settings.mediaFolders = f;
        m_settings.save();
    });
    connect(m_media, &MediaBin::sendLive, this, [this](const QString &path) {
        m_show->setLiveBackground(Background::media(path));
    });
    connect(m_media, &MediaBin::applyToSlides, this, &MainWindow::applyMediaToSlides);
    connect(m_media, &MediaBin::applyToPresentation, this, &MainWindow::applyMediaToPresentation);
    connect(m_media, &MediaBin::posterReady, this, [this] {
        if (m_hasCurrent) refreshGrid();
    });

    m_library->load();
    m_media->setFolders(m_settings.mediaFolders);
    m_engine->configure(m_settings.screens);
    buildPreviews();

    auto *previewTimer = new QTimer(this);
    connect(previewTimer, &QTimer::timeout, this, &MainWindow::refreshPreviews);
    previewTimer->start(100);
}

MainWindow::~MainWindow() = default;

// ---- UI construction ------------------------------------------------------

void MainWindow::buildUi()
{
    auto *split = new QSplitter;
    setCentralWidget(split);

    // Left: library + playlist
    auto *left = new QSplitter(Qt::Vertical);
    auto *libPanel = new QWidget;
    auto *libL = new QVBoxLayout(libPanel);
    libL->setContentsMargins(4, 4, 4, 4);
    auto *libTitle = new QLabel(tr("<b>Library</b>"));
    libL->addWidget(libTitle);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search songs (title, lyrics, CCLI #)…"));
    m_search->setClearButtonEnabled(true);
    libL->addWidget(m_search);
    m_libList = new QListWidget;
    m_libList->setDragEnabled(true);
    libL->addWidget(m_libList, 1);
    left->addWidget(libPanel);

    auto *plPanel = new QWidget;
    auto *plL = new QVBoxLayout(plPanel);
    plL->setContentsMargins(4, 4, 4, 4);
    auto *plTop = new QHBoxLayout;
    plTop->addWidget(new QLabel(tr("<b>Playlist</b>")));
    m_playlistBox = new QComboBox;
    plTop->addWidget(m_playlistBox, 1);
    auto *plNew = new QToolButton;
    plNew->setText(tr("New"));
    auto *plDel = new QToolButton;
    plDel->setText(tr("Delete"));
    plTop->addWidget(plNew);
    plTop->addWidget(plDel);
    plL->addLayout(plTop);
    m_playlist = new QListWidget;
    m_playlist->setObjectName("playlist");
    m_playlist->setDragDropMode(QAbstractItemView::InternalMove);
    plL->addWidget(m_playlist, 1);
    auto *plBtns = new QHBoxLayout;
    auto *plAdd = new QPushButton(tr("Add song"));
    plAdd->setToolTip(tr("Add the selected library song (or double-click it)"));
    auto *plHeader = new QPushButton(tr("Header"));
    auto *plRemove = new QPushButton(tr("Remove"));
    plBtns->addWidget(plAdd);
    plBtns->addWidget(plHeader);
    plBtns->addWidget(plRemove);
    plL->addLayout(plBtns);
    left->addWidget(plPanel);
    split->addWidget(left);

    // Centre: slide grid
    auto *center = new QWidget;
    auto *cL = new QVBoxLayout(center);
    cL->setContentsMargins(4, 4, 4, 4);
    m_presTitle = new QLabel(tr("Select a song from the library or playlist"));
    QFont tf = m_presTitle->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.3);
    tf.setBold(true);
    m_presTitle->setFont(tf);
    cL->addWidget(m_presTitle);
    m_grid = new QListWidget;
    m_grid->setObjectName("slideGrid");
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(kThumb + QSize(0, 24));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setSpacing(6);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_grid->setStyleSheet("QListWidget { background: #151515; }");
    cL->addWidget(m_grid, 1);
    split->addWidget(center);

    // Right: live previews, clear buttons, media bin
    auto *right = new QWidget;
    auto *rL = new QVBoxLayout(right);
    rL->setContentsMargins(4, 4, 4, 4);
    rL->addWidget(new QLabel(tr("<b>Live outputs</b>")));
    m_previewHost = new QWidget;
    rL->addWidget(m_previewHost);
    auto *clearRow = new QHBoxLayout;
    auto *clearAll = new QPushButton(tr("Clear all\nF1"));
    auto *clearBg = new QPushButton(tr("Clear background\nF2"));
    auto *clearText = new QPushButton(tr("Clear text\nF3"));
    for (QPushButton *b : {clearAll, clearBg, clearText}) {
        b->setMinimumHeight(48);
        clearRow->addWidget(b);
    }
    clearAll->setStyleSheet("QPushButton { background: #7a1f1f; color: white; }");
    rL->addLayout(clearRow);
    rL->addWidget(new QLabel(tr("<b>Media</b> — double-click to send live")));
    m_media = new MediaBin;
    rL->addWidget(m_media, 1);
    split->addWidget(right);
    split->setSizes({300, 900, 400});

    // Wiring
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshLibrary);
    connect(m_libList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        showPresentation(item->data(Qt::UserRole).toString());
    });
    connect(m_libList, &QListWidget::itemDoubleClicked, this, [plAdd] { plAdd->click(); });
    connect(m_playlist, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty()) showPresentation(id, m_playlist->row(item));
    });
    connect(m_playlistBox, &QComboBox::currentIndexChanged, this, &MainWindow::refreshPlaylistItems);
    connect(plNew, &QToolButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("New playlist"), tr("Name:"), QLineEdit::Normal,
                                                   QLocale().toString(QDate::currentDate(), "d MMMM yyyy"));
        if (name.isEmpty()) return;
        Playlist p;
        p.id = newId();
        p.name = name;
        m_library->savePlaylist(p);
        m_playlistBox->setCurrentIndex(m_playlistBox->findData(p.id));
    });
    connect(plDel, &QToolButton::clicked, this, [this] {
        Playlist *p = currentPlaylist();
        if (p && QMessageBox::question(this, tr("Delete playlist"), tr("Delete \"%1\"?").arg(p->name)) == QMessageBox::Yes)
            m_library->removePlaylist(p->id);
    });
    connect(plAdd, &QPushButton::clicked, this, [this] {
        Playlist *p = currentPlaylist();
        QListWidgetItem *sel = m_libList->currentItem();
        if (!p) {
            QMessageBox::information(this, tr("Playlist"), tr("Create a playlist first."));
            return;
        }
        if (!sel) return;
        p->items.append({sel->data(Qt::UserRole).toString(), QString()});
        m_library->savePlaylist(*p);
    });
    connect(plHeader, &QPushButton::clicked, this, [this] {
        Playlist *p = currentPlaylist();
        if (!p) return;
        const QString h = QInputDialog::getText(this, tr("Header"), tr("Section header (e.g. Worship, Sermon):"));
        if (h.isEmpty()) return;
        p->items.append({QString(), h});
        m_library->savePlaylist(*p);
    });
    connect(plRemove, &QPushButton::clicked, this, [this] {
        Playlist *p = currentPlaylist();
        const int row = m_playlist->currentRow();
        if (!p || row < 0 || row >= p->items.size()) return;
        p->items.removeAt(row);
        m_library->savePlaylist(*p);
    });
    connect(m_playlist->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        Playlist *p = currentPlaylist();
        if (!p) return;
        p->items.clear();
        for (int i = 0; i < m_playlist->count(); ++i) {
            QListWidgetItem *it = m_playlist->item(i);
            p->items.append({it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString()});
        }
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
        for (QListWidgetItem *it : m_grid->selectedItems()) m_current.slides[m_grid->row(it)].background = Background();
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

    auto add = [&](QMenu *menu, const QString &text, auto slot, const QKeySequence &key = {}, bool toolbar = true) {
        QAction *a = menu->addAction(text, this, slot);
        if (!key.isEmpty()) a->setShortcut(key);
        if (toolbar) tb->addAction(a);
        return a;
    };
    add(file, tr("New song…"), [this] { newPresentation(); }, QKeySequence::New);
    add(file, tr("New from pasted lyrics…"), [this] { newFromLyrics(); });
    add(file, tr("Import SongSelect files…"), [this] { importSongs(); }, QKeySequence(Qt::CTRL | Qt::Key_I));
    file->addSeparator();
    tb->addSeparator();
    add(file, tr("Edit song…"), [this] { editPresentation(qMax(0, m_grid->currentRow())); }, QKeySequence(Qt::CTRL | Qt::Key_E));
    add(file, tr("Delete song"), [this] { deletePresentation(); }, {}, false);
    file->addSeparator();
    add(file, tr("Open library folder"), [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_library->root()));
    }, {}, false);
    add(file, tr("Quit"), [this] { close(); }, QKeySequence::Quit, false);

    QMenu *screens = menuBar()->addMenu(tr("&Screens"));
    tb->addSeparator();
    add(screens, tr("Screen setup…"), [this] { openScreenSetup(); });
    QAction *outputs = add(screens, tr("Outputs on"), [this](bool on) { m_engine->setOutputsEnabled(on); },
                           QKeySequence(Qt::CTRL | Qt::Key_O));
    outputs->setCheckable(true);
    outputs->setChecked(true);
    outputs->setToolTip(tr("Turn all physical outputs (monitors, NDI, DeckLink) on or off"));

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

    auto *grid = new QGridLayout(m_previewHost);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < m_engine->screenCount(); ++i) {
        const ScreenConfig &cfg = m_engine->screenConfig(i);
        auto *title = new QLabel(QStringLiteral("<b>%1</b> <span style='color:gray'>%2×%3 @ %4</span>")
                                     .arg(cfg.name.toHtmlEscaped()).arg(cfg.resolution.width())
                                     .arg(cfg.resolution.height()).arg(cfg.fps));
        auto *img = new QLabel;
        img->setFixedSize(kPreview);
        img->setStyleSheet("background: black; border: 1px solid #444;");
        auto *status = new QLabel;
        status->setStyleSheet("color: gray; font-size: 10px;");
        status->setWordWrap(true);
        const int row = (i / 1) * 3;
        grid->addWidget(title, row, 0);
        grid->addWidget(img, row + 1, 0);
        grid->addWidget(status, row + 2, 0);
        m_previews << img;
        m_previewStatus << status;
    }
}

// ---- Library / playlist ---------------------------------------------------

void MainWindow::refreshLibrary()
{
    const QString q = m_search->text().trimmed();
    const QString current = m_libList->currentItem() ? m_libList->currentItem()->data(Qt::UserRole).toString() : QString();
    m_libList->clear();
    for (const Presentation &p : m_library->presentations()) {
        if (!q.isEmpty()) {
            bool match = p.title.contains(q, Qt::CaseInsensitive) || p.ccliNumber == q
                         || p.author.contains(q, Qt::CaseInsensitive);
            for (int i = 0; !match && i < p.slides.size(); ++i)
                match = p.slides[i].mainText().contains(q, Qt::CaseInsensitive);
            if (!match) continue;
        }
        auto *item = new QListWidgetItem(p.title);
        item->setData(Qt::UserRole, p.id);
        item->setToolTip(QStringList{p.author, p.copyright, p.ccliNumber.isEmpty() ? QString() : "CCLI #" + p.ccliNumber}
                             .filter(QRegularExpression(".")).join('\n'));
        m_libList->addItem(item);
        if (p.id == current) m_libList->setCurrentItem(item);
    }
    refreshPlaylistItems();   // titles may have changed
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

void MainWindow::refreshPlaylistItems()
{
    const int row = m_playlist->currentRow();
    m_playlist->clear();
    const Playlist *p = m_library->playlist(m_playlistBox->currentData().toString());
    if (!p) return;
    for (const PlaylistItem &i : p->items) {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, i.presentationId);
        item->setData(Qt::UserRole + 1, i.header);
        if (i.presentationId.isEmpty()) {
            item->setText(i.header.toUpper());
            item->setForeground(QColor("#d0a040"));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        } else {
            const Presentation *pres = m_library->find(i.presentationId);
            item->setText(pres ? "   " + pres->title : tr("   (missing song)"));
        }
        m_playlist->addItem(item);
    }
    m_playlist->setCurrentRow(qMin(row, m_playlist->count() - 1));
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
    refreshGrid();
}

void MainWindow::refreshGrid()
{
    const int row = m_grid->currentRow();
    m_grid->clear();
    m_thumbs.clear();
    if (!m_hasCurrent) return;
    for (int i = 0; i < m_current.slides.size(); ++i) {
        const Background bg = m_current.effectiveBackground(i);
        const QImage poster = bg.type == Background::Type::Image || bg.type == Background::Type::Video
                                  ? m_media->poster(bg.path) : QImage();
        m_thumbs << TextRenderer::renderSlide(m_current.slides[i], bg, kThumb, nullptr, poster);
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
        const Slide &s = m_current.slides[i];
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
            p.setPen(QPen(QColor(255, 140, 0), 6));
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
        m_previewStatus[i]->setText(st.isEmpty() ? tr("Preview only — no outputs enabled") : st.join(" · "));
    }
}

// ---- Show control ---------------------------------------------------------

void MainWindow::takeSlide(int index)
{
    if (!m_hasCurrent) return;
    m_show->take(m_current, index);
}

void MainWindow::goNext()
{
    const bool liveIsShown = m_hasCurrent && m_show->livePresentationId() == m_current.id;
    if (!liveIsShown) {
        // Nothing of this song is live yet: start it.
        if (m_hasCurrent && !m_current.slides.isEmpty()) takeSlide(0);
        return;
    }
    if (m_show->liveIndex() + 1 < m_current.slides.size()) {
        m_show->next();
        return;
    }
    // End of song: continue with the next song in the playlist.
    if (m_playlistRow < 0) return;
    for (int r = m_playlistRow + 1; r < m_playlist->count(); ++r) {
        const QString id = m_playlist->item(r)->data(Qt::UserRole).toString();
        if (id.isEmpty()) continue;
        m_playlist->setCurrentRow(r);
        showPresentation(id, r);
        if (!m_current.slides.isEmpty()) takeSlide(0);
        return;
    }
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
    m_show->presentationUpdated(m_current);
    refreshGrid();
}

void MainWindow::newPresentation()
{
    const QString title = QInputDialog::getText(this, tr("New song"), tr("Title:"));
    if (title.trimmed().isEmpty()) return;
    Presentation p;
    p.id = newId();
    p.title = title.trimmed();
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
    const Presentation p = SongImporter::toPresentation(song, m_settings.linesPerSlide, templ);
    m_library->save(p);
    showPresentation(p.id);
}

void MainWindow::importSongs(const QStringList &given)
{
    QStringList files = given;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(this, tr("Import songs from SongSelect"), m_settings.lastImportDir,
                                              SongImporter::fileFilters().join(";;"));
        if (files.isEmpty()) return;
    }
    bool ok = false;
    const int lines = QInputDialog::getInt(this, tr("Import songs"),
                                           tr("Lines per slide (0 = whole section on one slide):"),
                                           m_settings.linesPerSlide, 0, 12, 1, &ok);
    if (!ok) return;
    m_settings.linesPerSlide = lines;
    m_settings.lastImportDir = QFileInfo(files.first()).absolutePath();
    m_settings.save();

    QStringList errors;
    QString lastId;
    Presentation templ;
    for (const QString &f : files) {
        SongImporter::Song song;
        QString err;
        if (!SongImporter::parseFile(f, &song, &err)) {
            errors << err;
            continue;
        }
        const Presentation p = SongImporter::toPresentation(song, lines, templ);
        m_library->save(p);
        lastId = p.id;
    }
    if (!lastId.isEmpty()) showPresentation(lastId);
    statusBar()->showMessage(tr("Imported %1 of %2 songs").arg(files.size() - errors.size()).arg(files.size()), 5000);
    if (!errors.isEmpty()) QMessageBox::warning(this, tr("Import"), errors.join('\n'));
}

void MainWindow::editPresentation(int slideIndex)
{
    if (!m_hasCurrent) return;
    SlideEditor editor(m_current, slideIndex, m_media, this);
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
    for (QListWidgetItem *it : selected) m_current.slides[m_grid->row(it)].background = Background::media(path);
    saveCurrent();
}

void MainWindow::applyMediaToPresentation(const QString &path)
{
    if (!m_hasCurrent) return;
    m_current.defaultBackground = Background::media(path);
    saveCurrent();
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
        if (kSongExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower())) {
            e->acceptProposedAction();
            return;
        }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList files;
    for (const QUrl &u : e->mimeData()->urls())
        if (kSongExt.contains(QFileInfo(u.toLocalFile()).suffix().toLower())) files << u.toLocalFile();
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
