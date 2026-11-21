#include "ui/LooksDialog.h"

#include "core/Library.h"
#include "render/MediaPool.h"
#include "ui/MediaBin.h"
#include "ui/ScreenLookEditor.h"
#include "ui/Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static const QSize kPreview(384, 216);

LooksDialog::LooksDialog(Library *library, const QList<ScreenConfig> &screens, const LiveState &sample,
                         MediaBin *media, QWidget *parent)
    : QDialog(parent), m_library(library), m_screens(screens), m_sample(sample), m_media(media),
      m_pool(new MediaPool(this))
{
    setWindowTitle(tr("Looks"));
    resize(1100, 880);
    auto *root = new QHBoxLayout(this);

    // ---- Look list --------------------------------------------------------
    auto *left = new QVBoxLayout;
    auto *intro = new QLabel(tr("A look sets what every screen shows while it's live. Switch looks from the "
                                "toolbar during the service."));
    intro->setWordWrap(true);
    intro->setProperty("role", "hint");
    intro->setMaximumWidth(240);
    left->addWidget(intro);
    m_list = new QListWidget;
    m_list->setMaximumWidth(240);
    left->addWidget(m_list, 1);
    auto *buttons = new QGridLayout;
    auto *add = new QPushButton(tr("New"));
    add->setProperty("accent", true);
    auto *dup = new QPushButton(tr("Duplicate"));
    auto *rename = new QPushButton(tr("Rename"));
    auto *del = new QPushButton(tr("Delete"));
    buttons->addWidget(add, 0, 0);
    buttons->addWidget(dup, 0, 1);
    buttons->addWidget(rename, 1, 0);
    buttons->addWidget(del, 1, 1);
    left->addLayout(buttons);
    root->addLayout(left);

    // ---- Tabs: one per screen, plus Slides ------------------------------------
    auto *right = new QVBoxLayout;
    m_tabs = new QTabWidget;
    right->addWidget(m_tabs, 1);
    for (const ScreenConfig &cfg : std::as_const(m_screens)) {
        auto *page = new QWidget;
        auto *pl = new QVBoxLayout(page);
        ScreenTab tab;
        // Preview on top, settings below, so the settings get the full width.
        auto *top = new QHBoxLayout;
        auto *topText = new QVBoxLayout;
        tab.customise = new QCheckBox(tr("Customise “%1” in this look").arg(cfg.name));
        auto *hint = new QLabel(tr("Off: this screen keeps its own setup from Screens… while the look is live."));
        hint->setWordWrap(true);
        hint->setProperty("role", "hint");
        topText->addWidget(tab.customise);
        topText->addWidget(hint);
        topText->addStretch();
        top->addLayout(topText, 1);
        tab.preview = new QLabel;
        tab.preview->setFixedSize(kPreview);
        top->addWidget(tab.preview);
        pl->addLayout(top);
        tab.editor = new ScreenLookEditor;
        auto *scroll = new QScrollArea;
        scroll->setWidget(tab.editor);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        pl->addWidget(scroll, 1);
        m_tabs->addTab(page, cfg.name);
        m_screenTabs.append(tab);

        const int i = int(m_screenTabs.size()) - 1;
        connect(tab.customise, &QCheckBox::toggled, this, [this, i](bool on) {
            m_screenTabs[i].editor->setEnabled(on);
            if (m_loading) return;
            Look *l = current();
            if (!l) return;
            // Start from the screen's own setup, the natural thing to tweak.
            if (on) l->screens.insert(m_screens[i].id, m_screens[i].look.toJson());
            else l->screens.remove(m_screens[i].id);
            m_loading = true;
            m_screenTabs[i].editor->setLook(m_screens[i].lookIn(l), m_screens[i].role);
            m_loading = false;
            storeLook();
        });
        connect(tab.editor, &ScreenLookEditor::changed, this, [this, i] {
            if (m_loading) return;
            if (Look *l = current()) {
                l->screens.insert(m_screens[i].id, m_screenTabs[i].editor->look().toJson());
                storeLook();
            }
        });
    }

    auto *slides = new QWidget;
    auto *sf = new QFormLayout(slides);
    auto *slideHint = new QLabel(tr("Used when you apply this look to songs (right-click songs → Apply look) or "
                                    "make it the default for new songs (Looks menu)."));
    slideHint->setWordWrap(true);
    slideHint->setProperty("role", "hint");
    sf->addRow(slideHint);
    m_rect = new RectEditor;
    sf->addRow(tr("Text position"), m_rect);
    m_style = new StyleEditor;
    sf->addRow(m_style);
    m_bgLabel = new QLabel;
    auto *bgFile = new QPushButton(tr("Choose file…"));
    auto *bgBin = new QPushButton(tr("Media bin selection"));
    auto *bgNone = new QPushButton(tr("None"));
    auto *bgRow = new QHBoxLayout;
    bgRow->addWidget(m_bgLabel, 1);
    bgRow->addWidget(bgFile);
    bgRow->addWidget(bgBin);
    bgRow->addWidget(bgNone);
    sf->addRow(tr("Background"), bgRow);
    m_transition = new QComboBox;
    m_transition->addItems(Transition::names());
    m_transitionMs = new QSpinBox;
    m_transitionMs->setRange(0, 5000);
    m_transitionMs->setSingleStep(50);
    m_transitionMs->setSuffix(" ms");
    auto *trRow = new QHBoxLayout;
    trRow->addWidget(m_transition);
    trRow->addWidget(m_transitionMs);
    sf->addRow(tr("Transition"), trRow);
    m_tabs->addTab(slides, tr("Slides"));

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    right->addWidget(close);
    root->addLayout(right, 1);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);

    // ---- Wiring -------------------------------------------------------------
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        m_index = row;
        loadLook();
    });
    connect(add, &QPushButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("New look"), tr("Name:"), QLineEdit::Normal, tr("Sunday"));
        if (name.trimmed().isEmpty()) return;
        Look l;
        l.id = newId();
        l.name = name.trimmed();
        m_library->saveLook(l);
        refreshList(l.id);
    });
    connect(dup, &QPushButton::clicked, this, [this] {
        const Look *src = current();
        if (!src) return;
        Look l = *src;
        l.id = newId();
        l.name = tr("%1 copy").arg(src->name);
        m_library->saveLook(l);
        refreshList(l.id);
    });
    connect(rename, &QPushButton::clicked, this, [this] {
        Look *l = current();
        if (!l) return;
        const QString name = QInputDialog::getText(this, tr("Rename look"), tr("Name:"), QLineEdit::Normal, l->name);
        if (name.trimmed().isEmpty()) return;
        l->name = name.trimmed();
        storeLook();
        refreshList(l->id);
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const Look *l = current();
        if (!l || QMessageBox::question(this, tr("Delete look"), tr("Delete “%1”?").arg(l->name)) != QMessageBox::Yes)
            return;
        const QString id = l->id;
        m_library->removeLook(id);
        emit lookEdited(id);
        refreshList(QString());
    });

    auto storeSlides = [this] {
        if (m_loading) return;
        Look *l = current();
        if (!l) return;
        l->style = m_style->style();
        l->rect = m_rect->rect();
        l->transition.type = TransitionType(m_transition->currentIndex());
        l->transition.durationMs = m_transitionMs->value();
        storeLook();
    };
    connect(m_style, &StyleEditor::changed, this, storeSlides);
    connect(m_rect, &RectEditor::changed, this, storeSlides);
    connect(m_transition, &QComboBox::currentIndexChanged, this, storeSlides);
    connect(m_transitionMs, &QSpinBox::valueChanged, this, storeSlides);
    auto setBackground = [this](const Background &bg) {
        if (Look *l = current()) {
            l->background = bg;
            m_bgLabel->setText(bg.isSet() ? bg.describe() : tr("(keep each song's own)"));
            storeLook();
        }
    };
    connect(bgFile, &QPushButton::clicked, this, [this, setBackground] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Background"), QString(),
            tr("Media (*.jpg *.jpeg *.png *.bmp *.webp *.mp4 *.mov *.m4v *.mkv *.avi *.webm *.wmv)"));
        if (!path.isEmpty()) setBackground(Background::media(path));
    });
    connect(bgBin, &QPushButton::clicked, this, [this, setBackground] {
        if (!m_media->selectedPath().isEmpty()) setBackground(Background::media(m_media->selectedPath()));
    });
    connect(bgNone, &QPushButton::clicked, this, [setBackground] { setBackground(Background()); });

    // Video backgrounds need a moment to decode their first frame.
    QTimer::singleShot(700, this, &LooksDialog::updatePreviews);
    refreshList(QString());
}

LooksDialog::~LooksDialog() = default;

void LooksDialog::select(const QString &lookId)
{
    refreshList(lookId);
}

Look *LooksDialog::current()
{
    return m_index >= 0 && m_index < m_looks.size() ? &m_looks[m_index] : nullptr;
}

void LooksDialog::refreshList(const QString &selectId)
{
    m_looks = m_library->looks();
    const QSignalBlocker block(m_list);
    m_list->clear();
    int select = m_looks.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_looks.size(); ++i) {
        m_list->addItem(m_looks[i].name);
        if (m_looks[i].id == selectId) select = i;
    }
    m_list->setCurrentRow(select);
    m_index = select;
    loadLook();
}

void LooksDialog::loadLook()
{
    const Look *l = current();
    m_tabs->setEnabled(l);
    if (!l) {
        updatePreviews();
        return;
    }
    m_loading = true;
    for (int i = 0; i < m_screenTabs.size(); ++i) {
        const bool custom = l->screens.contains(m_screens[i].id);
        m_screenTabs[i].customise->setChecked(custom);
        m_screenTabs[i].editor->setLook(m_screens[i].lookIn(l), m_screens[i].role);
        m_screenTabs[i].editor->setEnabled(custom);
    }
    m_style->setStyle(l->style);
    m_rect->setRect(l->rect);
    m_bgLabel->setText(l->background.isSet() ? l->background.describe() : tr("(keep each song's own)"));
    m_transition->setCurrentIndex(int(l->transition.type));
    m_transitionMs->setValue(l->transition.durationMs);
    m_loading = false;
    updatePreviews();
}

void LooksDialog::storeLook()
{
    const Look *l = current();
    if (!l) return;
    m_library->saveLook(*l);
    emit lookEdited(l->id);
    updatePreviews();
}

void LooksDialog::updatePreviews()
{
    const Look *l = current();
    for (int i = 0; i < m_screenTabs.size(); ++i) {
        // The real screen renderer, so the preview matches the output exactly.
        ScreenRenderer renderer(m_pool);
        renderer.setConfig(m_screens[i].withLook(l));
        renderer.setState(m_sample, 0);
        const QImage frame = renderer.render(100000);   // long after any transition

        QPixmap pm(kPreview);
        QPainter p(&pm);
        if (m_screens[i].withLook(l).look.transparent) {
            for (int x = 0; x < kPreview.width(); x += 14)
                for (int y = 0; y < kPreview.height(); y += 14)
                    p.fillRect(x, y, 14, 14, ((x + y) / 14) % 2 ? QColor(60, 60, 60) : QColor(90, 90, 90));
        } else {
            p.fillRect(pm.rect(), Qt::black);
        }
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QSizeF s = frame.size();
        s.scale(kPreview, Qt::KeepAspectRatio);
        p.drawImage(QRectF((kPreview.width() - s.width()) / 2, (kPreview.height() - s.height()) / 2, s.width(), s.height()), frame);
        p.end();
        m_screenTabs[i].preview->setPixmap(pm);
    }
}
