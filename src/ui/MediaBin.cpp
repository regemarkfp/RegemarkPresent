#include "ui/MediaBin.h"

#include "core/Types.h"

#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

static const QSize kThumb(160, 90);
static const QStringList kMediaGlobs = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp", "*.gif",
                                        "*.mp4", "*.mov", "*.m4v", "*.mkv", "*.avi", "*.webm", "*.wmv"};

static QIcon thumbIcon(const QImage &img, bool video)
{
    QPixmap pm(kThumb);
    pm.fill(QColor(30, 30, 30));
    QPainter p(&pm);
    if (!img.isNull()) {
        const QImage s = img.scaled(kThumb, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawImage((kThumb.width() - s.width()) / 2, (kThumb.height() - s.height()) / 2, s);
    }
    if (video) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(0, 0, 0, 160));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRect(4, kThumb.height() - 22, 26, 18), 4, 4);
        p.setBrush(Qt::white);
        const QPointF tri[3] = {{12, double(kThumb.height() - 19)}, {12, double(kThumb.height() - 7)}, {23, double(kThumb.height() - 13)}};
        p.drawPolygon(tri, 3);
    }
    return QIcon(pm);
}

MediaBin::MediaBin(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *top = new QHBoxLayout;
    m_folderBox = new QComboBox;
    auto *add = new QToolButton;
    add->setText("+");
    add->setToolTip(tr("Add a media folder (e.g. your Sunday Screens downloads)"));
    auto *remove = new QToolButton;
    remove->setText("−");
    remove->setToolTip(tr("Remove this folder from the media bin"));
    top->addWidget(m_folderBox, 1);
    top->addWidget(add);
    top->addWidget(remove);
    layout->addLayout(top);

    m_list = new QListWidget;
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(kThumb);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(4);
    m_list->setWordWrap(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout;
    auto *live = new QPushButton(tr("Live"));
    live->setToolTip(tr("Replace the live background now (double-click does the same)"));
    auto *toSlides = new QPushButton(tr("→ Slides"));
    toSlides->setToolTip(tr("Set as the background of the selected slides; it carries forward until the next slide with its own background"));
    auto *toPres = new QPushButton(tr("→ Song"));
    toPres->setToolTip(tr("Set as the default background of the open presentation"));
    buttons->addWidget(live);
    buttons->addWidget(toSlides);
    buttons->addWidget(toPres);
    auto *cue = new QPushButton(tr("New cue"));
    cue->setToolTip(tr("Make a playlist item from the selected media: video with sound, or an announcement loop"));
    buttons->addWidget(cue);
    connect(cue, &QPushButton::clicked, this, [this] {
        const QStringList paths = selectedPaths();
        if (!paths.isEmpty()) emit createCue(paths);
    });
    layout->addLayout(buttons);

    auto withSelection = [this](void (MediaBin::*sig)(const QString &)) {
        return [this, sig] {
            const QString p = selectedPath();
            if (!p.isEmpty()) emit (this->*sig)(p);
        };
    };
    connect(live, &QPushButton::clicked, this, withSelection(&MediaBin::sendLive));
    connect(toSlides, &QPushButton::clicked, this, withSelection(&MediaBin::applyToSlides));
    connect(toPres, &QPushButton::clicked, this, withSelection(&MediaBin::applyToPresentation));
    connect(m_list, &QListWidget::itemDoubleClicked, this, withSelection(&MediaBin::sendLive));
    connect(m_list, &QListWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        if (!m_list->itemAt(pos)) return;
        QMenu menu;
        menu.addAction(tr("Send live"), this, withSelection(&MediaBin::sendLive));
        menu.addAction(tr("Set as background of selected slides"), this, withSelection(&MediaBin::applyToSlides));
        menu.addAction(tr("Set as song background"), this, withSelection(&MediaBin::applyToPresentation));
        menu.addSeparator();
        menu.addAction(tr("New cue / announcement loop from selection…"), this, [this] { emit createCue(selectedPaths()); });
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });

    connect(m_folderBox, &QComboBox::currentIndexChanged, this, &MediaBin::rescan);
    connect(add, &QToolButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Add media folder"));
        if (dir.isEmpty() || m_folders.contains(dir)) return;
        m_folders << dir;
        setFolders(m_folders);
        m_folderBox->setCurrentIndex(m_folderBox->count() - 1);
        emit foldersChanged(m_folders);
    });
    connect(remove, &QToolButton::clicked, this, [this] {
        const int i = m_folderBox->currentIndex() - 1;
        if (i < 0) return;
        m_folders.removeAt(i);
        setFolders(m_folders);
        emit foldersChanged(m_folders);
    });

    m_grabber = new QMediaPlayer(this);
    m_grabSink = new QVideoSink(this);
    m_grabber->setVideoOutput(m_grabSink);
    connect(m_grabSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &f) {
        if (m_grabbing.isEmpty() || !f.isValid()) return;
        const QImage img = f.toImage().scaled(kThumb * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QString path = m_grabbing;
        m_grabbing.clear();
        m_grabber->stop();
        setPoster(path, img);
        img.save(cachePath(path), "JPG", 85);
        QTimer::singleShot(0, this, &MediaBin::nextVideoThumb);
    });
    connect(m_grabber, &QMediaPlayer::errorOccurred, this, [this] {
        m_grabbing.clear();
        QTimer::singleShot(0, this, &MediaBin::nextVideoThumb);
    });
}

void MediaBin::setFolders(const QStringList &folders)
{
    m_folders = folders;
    const QSignalBlocker block(m_folderBox);
    const int prev = m_folderBox->currentIndex();
    m_folderBox->clear();
    m_folderBox->addItem(tr("All media folders"));
    for (const QString &f : m_folders) m_folderBox->addItem(QFileInfo(f).fileName(), f);
    m_folderBox->setCurrentIndex(qBound(0, prev, m_folderBox->count() - 1));
    rescan();
}

QString MediaBin::selectedPath() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QStringList MediaBin::selectedPaths() const
{
    QStringList paths;
    for (int i = 0; i < m_list->count(); ++i)   // keep bin order, not click order
        if (m_list->item(i)->isSelected()) paths << m_list->item(i)->data(Qt::UserRole).toString();
    return paths;
}

QString MediaBin::cachePath(const QString &path) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbs";
    QDir().mkpath(dir);
    const QFileInfo fi(path);
    const QByteArray key = (path + QString::number(fi.lastModified().toMSecsSinceEpoch())).toUtf8();
    return dir + "/" + QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex() + ".jpg";
}

void MediaBin::rescan()
{
    m_list->clear();
    m_videoQueue.clear();
    const QString only = m_folderBox->currentData().toString();
    const QStringList roots = only.isEmpty() ? m_folders : QStringList{only};
    for (const QString &root : roots) {
        QDirIterator it(root, kMediaGlobs, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const bool video = Background::media(path).type == Background::Type::Video;
            auto *item = new QListWidgetItem(QFileInfo(path).completeBaseName());
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            item->setIcon(thumbIcon(poster(path), video));
            item->setSizeHint(kThumb + QSize(12, 36));
            m_list->addItem(item);
        }
    }
}

QImage MediaBin::poster(const QString &path)
{
    auto it = m_posters.constFind(path);
    if (it != m_posters.constEnd()) return it.value();

    const QString cached = cachePath(path);
    if (QFile::exists(cached)) {
        const QImage img(cached);
        m_posters.insert(path, img);
        return img;
    }
    if (Background::media(path).type == Background::Type::Video) {
        queueVideoThumb(path);
        return {};
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    if (full.isValid()) reader.setScaledSize(full.scaled(kThumb * 2, Qt::KeepAspectRatioByExpanding));
    const QImage img = reader.read();
    if (!img.isNull()) img.save(cached, "JPG", 85);
    m_posters.insert(path, img);
    return img;
}

void MediaBin::queueVideoThumb(const QString &path)
{
    if (m_videoQueue.contains(path) || m_grabbing == path) return;
    m_videoQueue << path;
    if (m_grabbing.isEmpty()) QTimer::singleShot(0, this, &MediaBin::nextVideoThumb);
}

void MediaBin::nextVideoThumb()
{
    if (!m_grabbing.isEmpty() || m_videoQueue.isEmpty()) return;
    m_grabbing = m_videoQueue.takeFirst();
    const QString path = m_grabbing;
    m_grabber->setSource(QUrl::fromLocalFile(path));
    m_grabber->play();
    // Give up on files the decoder chokes on.
    QTimer::singleShot(6000, this, [this, path] {
        if (m_grabbing != path) return;
        m_grabbing.clear();
        m_grabber->stop();
        m_posters.insert(path, QImage());
        nextVideoThumb();
    });
}

void MediaBin::setPoster(const QString &path, const QImage &img)
{
    m_posters.insert(path, img);
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (item->data(Qt::UserRole).toString() == path) item->setIcon(thumbIcon(img, true));
    }
    emit posterReady(path);
}
