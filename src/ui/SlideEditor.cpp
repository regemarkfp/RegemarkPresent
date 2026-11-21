#include "ui/SlideEditor.h"

#include "core/RichText.h"
#include "render/TextRenderer.h"
#include "ui/MediaBin.h"
#include "ui/Widgets.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

// The rich-text editor shows text at this fraction of its 1080p pixel size.
static constexpr double kEditScale = 0.4;
static const QSize kPreview(640, 360);

SlideEditor::SlideEditor(const Presentation &p, int startIndex, MediaBin *media, QWidget *parent)
    : QDialog(parent), m_pres(p), m_media(media)
{
    setWindowTitle(tr("Edit — %1").arg(p.title));
    resize(1400, 860);
    if (m_pres.slides.isEmpty()) m_pres.slides.append(m_pres.makeSlide(tr("Verse 1"), QString()));
    m_slide = qBound(0, startIndex, int(m_pres.slides.size()) - 1);
    buildUi();
    refreshSlideList();
    loadSlide();
}

Slide *SlideEditor::slide() { return &m_pres.slides[m_slide]; }

TextBox *SlideEditor::box()
{
    Slide *s = slide();
    return m_box >= 0 && m_box < s->boxes.size() ? &s->boxes[m_box] : nullptr;
}

void SlideEditor::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // Song metadata
    auto *meta = new QHBoxLayout;
    m_title = new QLineEdit(m_pres.title);
    m_author = new QLineEdit(m_pres.author);
    m_copyright = new QLineEdit(m_pres.copyright);
    m_ccli = new QLineEdit(m_pres.ccliNumber);
    m_ccli->setMaximumWidth(110);
    meta->addWidget(new QLabel(tr("Title")));
    meta->addWidget(m_title, 2);
    meta->addWidget(new QLabel(tr("Author")));
    meta->addWidget(m_author, 2);
    meta->addWidget(new QLabel(tr("©")));
    meta->addWidget(m_copyright, 2);
    meta->addWidget(new QLabel(tr("CCLI #")));
    meta->addWidget(m_ccli);
    root->addLayout(meta);

    auto *split = new QSplitter;
    root->addWidget(split, 1);

    // ---- Slide list ------------------------------------------------------
    auto *left = new QWidget;
    auto *leftL = new QVBoxLayout(left);
    leftL->setContentsMargins(0, 0, 0, 0);
    m_slides = new QListWidget;
    leftL->addWidget(m_slides, 1);
    auto *slideBtns = new QHBoxLayout;
    auto button = [&](const QString &text, const QString &tip) {
        auto *b = new QToolButton;
        b->setText(text);
        b->setToolTip(tip);
        slideBtns->addWidget(b);
        return b;
    };
    auto *addSlide = button("+", tr("Add slide after this one"));
    auto *dupSlide = button("⧉", tr("Duplicate slide"));
    auto *delSlide = button("−", tr("Delete slide"));
    auto *upSlide = button("↑", tr("Move up"));
    auto *downSlide = button("↓", tr("Move down"));
    slideBtns->addStretch();
    leftL->addLayout(slideBtns);
    split->addWidget(left);

    // ---- Preview + rich text ---------------------------------------------
    auto *center = new QWidget;
    auto *centerL = new QVBoxLayout(center);
    centerL->setContentsMargins(0, 0, 0, 0);
    m_preview = new QLabel;
    m_preview->setFixedSize(kPreview);
    m_preview->setAlignment(Qt::AlignCenter);
    centerL->addWidget(m_preview, 0, Qt::AlignHCenter);

    auto *fmt = new QHBoxLayout;
    auto fmtButton = [&](const QString &text, const QString &tip) {
        auto *b = new QToolButton;
        b->setText(text);
        b->setToolTip(tip);
        fmt->addWidget(b);
        return b;
    };
    auto *bold = fmtButton("B", tr("Bold selection"));
    QFont bf = bold->font();
    bf.setBold(true);
    bold->setFont(bf);
    auto *italic = fmtButton("I", tr("Italic selection"));
    QFont itf = italic->font();
    itf.setItalic(true);
    italic->setFont(itf);
    auto *underline = fmtButton("U", tr("Underline selection"));
    auto *color = fmtButton(tr("Colour"), tr("Colour selection"));
    fmt->addWidget(new QLabel(tr("Size")));
    m_runSize = new QSpinBox;
    m_runSize->setRange(8, 400);
    m_runSize->setSuffix(" px");
    m_runSize->setToolTip(tr("Font size for the selection (pixels at 1080p)"));
    fmt->addWidget(m_runSize);
    auto *applySize = fmtButton(tr("Apply"), tr("Apply size to selection"));
    auto *plain = fmtButton(tr("Clear formatting"), tr("Remove per-word formatting from the selection"));
    fmt->addStretch();
    centerL->addLayout(fmt);

    m_text = new QTextEdit;
    m_text->setAcceptRichText(false);
    m_text->setStyleSheet("QTextEdit { background: #1e1e1e; }");
    centerL->addWidget(m_text, 1);
    split->addWidget(center);

    // ---- Property tabs ---------------------------------------------------
    auto *tabs = new QTabWidget;
    split->addWidget(tabs);
    split->setSizes({240, 680, 420});

    // Slide tab
    auto *slideTab = new QWidget;
    auto *sf = new QFormLayout(slideTab);
    m_label = new QLineEdit;
    m_labelColor = new ColorButton;
    auto *labelRow = new QHBoxLayout;
    labelRow->addWidget(m_label);
    labelRow->addWidget(m_labelColor);
    sf->addRow(tr("Group label"), labelRow);

    auto *bgBox = new QGroupBox(tr("Background"));
    auto *bgf = new QFormLayout(bgBox);
    m_bgMode = new QComboBox;
    m_bgMode->addItems({tr("Carry over from previous slide"), tr("Solid colour"), tr("Image / video")});
    bgf->addRow(m_bgMode);
    m_bgColor = new ColorButton;
    bgf->addRow(tr("Colour"), m_bgColor);
    m_bgPath = new QLabel;
    m_bgPath->setWordWrap(true);
    bgf->addRow(tr("Media"), m_bgPath);
    auto *bgBtns = new QHBoxLayout;
    auto *bgFile = new QPushButton(tr("Choose file…"));
    auto *bgBin = new QPushButton(tr("Use media bin selection"));
    bgBtns->addWidget(bgFile);
    bgBtns->addWidget(bgBin);
    bgf->addRow(bgBtns);
    m_bgFit = new QComboBox;
    m_bgFit->addItems({tr("Fill (crop)"), tr("Fit (letterbox)"), tr("Stretch")});
    bgf->addRow(tr("Scaling"), m_bgFit);
    m_bgAudio = new QCheckBox(tr("Play the video's sound (plays once)"));
    bgf->addRow(m_bgAudio);
    sf->addRow(bgBox);

    auto *trBox = new QGroupBox(tr("Transition into this slide"));
    auto *trf = new QFormLayout(trBox);
    m_ownTransition = new QCheckBox(tr("Override the presentation default"));
    trf->addRow(m_ownTransition);
    m_transition = new QComboBox;
    m_transition->addItems(Transition::names());
    m_transitionMs = new QSpinBox;
    m_transitionMs->setRange(0, 5000);
    m_transitionMs->setSingleStep(50);
    m_transitionMs->setSuffix(" ms");
    auto *trRow = new QHBoxLayout;
    trRow->addWidget(m_transition);
    trRow->addWidget(m_transitionMs);
    trf->addRow(trRow);
    sf->addRow(trBox);

    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(tr("Shown on confidence screens"));
    m_notes->setMaximumHeight(90);
    sf->addRow(tr("Notes"), m_notes);
    m_advance = new QSpinBox;
    m_advance->setRange(0, 3600);
    m_advance->setSuffix(tr(" s"));
    m_advance->setSpecialValueText(tr("Off"));
    m_advance->setToolTip(tr("Go to the next slide automatically after this many seconds"));
    sf->addRow(tr("Advance after"), m_advance);
    tabs->addTab(slideTab, tr("Slide"));

    // Text box tab
    auto *boxTab = new QWidget;
    auto *bxf = new QFormLayout(boxTab);
    m_boxPicker = new QComboBox;
    auto *addBox = new QToolButton;
    addBox->setText("+");
    addBox->setToolTip(tr("Add a text box to this slide"));
    auto *delBox = new QToolButton;
    delBox->setText("−");
    delBox->setToolTip(tr("Remove this text box"));
    auto *boxRow = new QHBoxLayout;
    boxRow->addWidget(m_boxPicker, 1);
    boxRow->addWidget(addBox);
    boxRow->addWidget(delBox);
    bxf->addRow(tr("Text box"), boxRow);
    m_boxMain = new QCheckBox(tr("Main lyric box (screen overrides apply)"));
    bxf->addRow(m_boxMain);
    m_rect = new RectEditor;
    bxf->addRow(tr("Position"), m_rect);
    m_style = new StyleEditor;
    bxf->addRow(m_style);
    auto *buildBox = new QGroupBox(tr("Build animation"));
    auto *buf = new QHBoxLayout(buildBox);
    m_build = new QComboBox;
    m_build->addItems({tr("None"), tr("Fade in"), tr("Rise in"), tr("Zoom in"), tr("Wipe")});
    m_buildDelay = new QSpinBox;
    m_buildDelay->setRange(0, 10000);
    m_buildDelay->setSingleStep(100);
    m_buildDelay->setPrefix(tr("after "));
    m_buildDelay->setSuffix(" ms");
    m_buildMs = new QSpinBox;
    m_buildMs->setRange(0, 5000);
    m_buildMs->setSingleStep(50);
    m_buildMs->setPrefix(tr("over "));
    m_buildMs->setSuffix(" ms");
    buf->addWidget(m_build);
    buf->addWidget(m_buildDelay);
    buf->addWidget(m_buildMs);
    bxf->addRow(buildBox);
    tabs->addTab(boxTab, tr("Text box"));

    // Presentation tab
    auto *presTab = new QWidget;
    auto *pf = new QFormLayout(presTab);
    m_defTransition = new QComboBox;
    m_defTransition->addItems(Transition::names());
    m_defTransition->setCurrentIndex(int(m_pres.defaultTransition.type));
    m_defTransitionMs = new QSpinBox;
    m_defTransitionMs->setRange(0, 5000);
    m_defTransitionMs->setSingleStep(50);
    m_defTransitionMs->setSuffix(" ms");
    m_defTransitionMs->setValue(m_pres.defaultTransition.durationMs);
    auto *dtRow = new QHBoxLayout;
    dtRow->addWidget(m_defTransition);
    dtRow->addWidget(m_defTransitionMs);
    pf->addRow(tr("Default transition"), dtRow);
    m_defBg = new QLabel(m_pres.defaultBackground.describe());
    auto *defBgFile = new QPushButton(tr("Choose file…"));
    auto *defBgBin = new QPushButton(tr("Media bin selection"));
    auto *defBgClear = new QPushButton(tr("None"));
    auto *dbRow = new QHBoxLayout;
    dbRow->addWidget(m_defBg, 1);
    dbRow->addWidget(defBgFile);
    dbRow->addWidget(defBgBin);
    dbRow->addWidget(defBgClear);
    pf->addRow(tr("Default background"), dbRow);
    m_loop = new QCheckBox(tr("Loop: after the last slide, start again (announcements)"));
    m_loop->setChecked(m_pres.loop);
    pf->addRow(m_loop);
    connect(m_loop, &QCheckBox::toggled, this, [this](bool on) { m_pres.loop = on; });
    auto *applyStyle = new QPushButton(tr("Apply this box's style to every slide"));
    applyStyle->setToolTip(tr("Copies the current main box style and position to all slides and makes it the default for new slides"));
    pf->addRow(applyStyle);
    tabs->addTab(presTab, tr("Presentation"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        m_pres.title = m_title->text().trimmed();
        m_pres.author = m_author->text().trimmed();
        m_pres.copyright = m_copyright->text().trimmed();
        m_pres.ccliNumber = m_ccli->text().trimmed();
        if (m_pres.title.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Please give the presentation a title."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ---- Wiring ------------------------------------------------------------
    connect(m_slides, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading || row < 0) return;
        m_slide = row;
        m_box = 0;
        loadSlide();
    });
    connect(addSlide, &QToolButton::clicked, this, [this] {
        Slide s = m_pres.makeSlide(slide()->label, QString());
        s.boxes.first().style = box() ? box()->style : m_pres.defaultStyle;
        if (box()) s.boxes.first().rect = box()->rect;
        m_pres.slides.insert(++m_slide, s);
        refreshSlideList();
        loadSlide();
        m_text->setFocus();
    });
    connect(dupSlide, &QToolButton::clicked, this, [this] {
        Slide s = *slide();
        s.id = newId();
        for (TextBox &b : s.boxes) b.id = newId();
        m_pres.slides.insert(++m_slide, s);
        refreshSlideList();
        loadSlide();
    });
    connect(delSlide, &QToolButton::clicked, this, [this] {
        if (m_pres.slides.size() <= 1) return;
        m_pres.slides.removeAt(m_slide);
        m_slide = qMin(m_slide, int(m_pres.slides.size()) - 1);
        refreshSlideList();
        loadSlide();
    });
    connect(upSlide, &QToolButton::clicked, this, [this] {
        if (m_slide <= 0) return;
        m_pres.slides.swapItemsAt(m_slide, m_slide - 1);
        --m_slide;
        refreshSlideList();
    });
    connect(downSlide, &QToolButton::clicked, this, [this] {
        if (m_slide + 1 >= m_pres.slides.size()) return;
        m_pres.slides.swapItemsAt(m_slide, m_slide + 1);
        ++m_slide;
        refreshSlideList();
    });

    connect(m_text, &QTextEdit::textChanged, this, &SlideEditor::storeText);
    auto mergeFormat = [this](const QTextCharFormat &f) {
        QTextCursor c = m_text->textCursor();
        if (!c.hasSelection()) c.select(QTextCursor::WordUnderCursor);
        c.mergeCharFormat(f);
        m_text->mergeCurrentCharFormat(f);
    };
    connect(bold, &QToolButton::clicked, this, [=] {
        QTextCharFormat f;
        f.setFontWeight(m_text->currentCharFormat().fontWeight() >= QFont::DemiBold ? QFont::Normal : QFont::Bold);
        mergeFormat(f);
    });
    connect(italic, &QToolButton::clicked, this, [=] {
        QTextCharFormat f;
        f.setFontItalic(!m_text->currentCharFormat().fontItalic());
        mergeFormat(f);
    });
    connect(underline, &QToolButton::clicked, this, [=] {
        QTextCharFormat f;
        f.setFontUnderline(!m_text->currentCharFormat().fontUnderline());
        mergeFormat(f);
    });
    connect(color, &QToolButton::clicked, this, [=] {
        const QColor c = QColorDialog::getColor(m_text->currentCharFormat().foreground().color(), this);
        if (!c.isValid()) return;
        QTextCharFormat f;
        f.setForeground(c);
        mergeFormat(f);
    });
    connect(applySize, &QToolButton::clicked, this, [=] {
        QTextCharFormat f;
        f.setProperty(QTextFormat::FontPixelSize, qMax(1, qRound(m_runSize->value() * kEditScale)));
        mergeFormat(f);
    });
    connect(plain, &QToolButton::clicked, this, [this] {
        TextBox *b = box();
        if (!b) return;
        // Re-render plain text with the base style, dropping run overrides.
        b->runs = QJsonArray();
        b->text = m_text->toPlainText();
        loadText();
        updatePreview();
    });

    // Slide properties
    connect(m_label, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_labelColor->setColor(Slide::colorForLabel(t));
        storeSlide();
    });
    connect(m_labelColor, &ColorButton::colorChanged, this, &SlideEditor::storeSlide);
    connect(m_notes, &QPlainTextEdit::textChanged, this, &SlideEditor::storeSlide);
    connect(m_ownTransition, &QCheckBox::toggled, this, &SlideEditor::storeSlide);
    connect(m_transition, &QComboBox::currentIndexChanged, this, &SlideEditor::storeSlide);
    connect(m_transitionMs, &QSpinBox::valueChanged, this, &SlideEditor::storeSlide);
    connect(m_bgMode, &QComboBox::currentIndexChanged, this, [this](int mode) {
        if (m_loading) return;
        Background &bg = slide()->background;
        if (mode == 0) bg = Background();
        else if (mode == 1) bg = Background::solid(m_bgColor->color());
        else if (!bg.path.isEmpty()) bg = Background::media(bg.path);
        else { chooseBackground(false); return; }
        loadSlide();
    });
    connect(m_bgColor, &ColorButton::colorChanged, this, [this](const QColor &c) {
        slide()->background = Background::solid(c);
        loadSlide();
    });
    connect(m_bgAudio, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading) return;
        slide()->background.audio = on;
        slide()->background.loop = !on;
    });
    connect(m_advance, &QSpinBox::valueChanged, this, [this](int v) {
        if (!m_loading) slide()->advanceMs = v * 1000;
    });
    connect(m_bgFit, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading) return;
        slide()->background.fit = FitMode(i);
        updatePreview();
    });
    connect(bgFile, &QPushButton::clicked, this, [this] { chooseBackground(false); });
    connect(bgBin, &QPushButton::clicked, this, [this] {
        if (m_media->selectedPath().isEmpty()) return;
        slide()->background = Background::media(m_media->selectedPath());
        loadSlide();
    });

    // Box properties
    connect(m_boxPicker, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading || i < 0) return;
        m_box = i;
        loadBox();
    });
    connect(addBox, &QToolButton::clicked, this, [this] {
        TextBox b;
        b.id = newId();
        b.text = tr("Text");
        b.style = m_pres.defaultStyle;
        b.style.fontSize = 48;
        b.rect = QRectF(0.1, 0.05, 0.8, 0.15);
        b.isMain = false;
        slide()->boxes.append(b);
        m_box = int(slide()->boxes.size()) - 1;
        loadSlide();
    });
    connect(delBox, &QToolButton::clicked, this, [this] {
        if (slide()->boxes.size() <= 1) return;
        slide()->boxes.removeAt(m_box);
        m_box = 0;
        loadSlide();
    });
    connect(m_boxMain, &QCheckBox::toggled, this, &SlideEditor::storeBox);
    connect(m_rect, &RectEditor::changed, this, &SlideEditor::storeBox);
    connect(m_style, &StyleEditor::changed, this, [this] {
        storeBox();
        loadText();   // runs are relative to the base style; re-render in the editor
    });
    connect(m_build, &QComboBox::currentIndexChanged, this, &SlideEditor::storeBox);
    connect(m_buildDelay, &QSpinBox::valueChanged, this, &SlideEditor::storeBox);
    connect(m_buildMs, &QSpinBox::valueChanged, this, &SlideEditor::storeBox);

    // Presentation properties
    auto storeDefTransition = [this] {
        m_pres.defaultTransition.type = TransitionType(m_defTransition->currentIndex());
        m_pres.defaultTransition.durationMs = m_defTransitionMs->value();
    };
    connect(m_defTransition, &QComboBox::currentIndexChanged, this, storeDefTransition);
    connect(m_defTransitionMs, &QSpinBox::valueChanged, this, storeDefTransition);
    connect(defBgFile, &QPushButton::clicked, this, [this] { chooseBackground(true); });
    connect(defBgBin, &QPushButton::clicked, this, [this] {
        if (m_media->selectedPath().isEmpty()) return;
        m_pres.defaultBackground = Background::media(m_media->selectedPath());
        m_defBg->setText(m_pres.defaultBackground.describe());
        updatePreview();
    });
    connect(defBgClear, &QPushButton::clicked, this, [this] {
        m_pres.defaultBackground = Background();
        m_defBg->setText(m_pres.defaultBackground.describe());
        updatePreview();
    });
    connect(applyStyle, &QPushButton::clicked, this, [this] {
        const TextBox *b = box();
        if (!b) return;
        m_pres.defaultStyle = b->style;
        for (Slide &s : m_pres.slides)
            for (TextBox &other : s.boxes)
                if (other.isMain) {
                    other.style = b->style;
                    other.rect = b->rect;
                }
        updatePreview();
    });
}

void SlideEditor::refreshSlideList()
{
    m_loading = true;
    m_slides->clear();
    for (const Slide &s : std::as_const(m_pres.slides)) {
        QPixmap swatch(12, 28);
        swatch.fill(s.labelColor.isValid() ? s.labelColor : Slide::colorForLabel(s.label));
        const QString first = s.mainText().section('\n', 0, 0);
        m_slides->addItem(new QListWidgetItem(QIcon(swatch), QStringLiteral("%1\n%2").arg(s.label, first)));
    }
    m_slides->setCurrentRow(m_slide);
    m_loading = false;
}

void SlideEditor::loadSlide()
{
    m_loading = true;
    const Slide &s = *slide();
    m_label->setText(s.label);
    m_labelColor->setColor(s.labelColor.isValid() ? s.labelColor : Slide::colorForLabel(s.label));
    m_notes->setPlainText(s.notes);
    m_ownTransition->setChecked(s.hasTransition);
    const Transition t = s.hasTransition ? s.transition : m_pres.defaultTransition;
    m_transition->setCurrentIndex(int(t.type));
    m_transitionMs->setValue(t.durationMs);
    m_transition->setEnabled(s.hasTransition);
    m_transitionMs->setEnabled(s.hasTransition);

    const Background &bg = s.background;
    m_bgMode->setCurrentIndex(bg.type == Background::Type::None ? 0 : bg.type == Background::Type::Color ? 1 : 2);
    m_bgColor->setColor(bg.color);
    if (bg.isSet())
        m_bgPath->setText(bg.type == Background::Type::Color ? QString() : QFileInfo(bg.path).fileName());
    else
        m_bgPath->setText(tr("Inherited: %1").arg(m_pres.effectiveBackground(m_slide - 1).describe()));
    m_bgFit->setCurrentIndex(int(bg.fit));
    m_bgAudio->setChecked(bg.audio);
    m_bgAudio->setEnabled(bg.type == Background::Type::Video);
    m_advance->setValue(s.advanceMs / 1000);

    m_boxPicker->clear();
    for (int i = 0; i < s.boxes.size(); ++i)
        m_boxPicker->addItem(s.boxes[i].isMain ? tr("Box %1 (main)").arg(i + 1) : tr("Box %1").arg(i + 1));
    m_box = qBound(0, m_box, int(s.boxes.size()) - 1);
    m_boxPicker->setCurrentIndex(m_box);
    m_loading = false;

    loadBox();
    if (!m_slides->currentItem() || m_slides->currentRow() != m_slide) refreshSlideList();
}

void SlideEditor::loadBox()
{
    const TextBox *b = box();
    m_loading = true;
    if (b) {
        m_boxMain->setChecked(b->isMain);
        m_rect->setRect(b->rect);
        m_style->setStyle(b->style);
        m_build->setCurrentIndex(int(b->build));
        m_buildDelay->setValue(b->buildDelayMs);
        m_buildMs->setValue(b->buildDurationMs);
        m_runSize->setValue(qRound(b->style.fontSize));
    }
    m_loading = false;
    loadText();
    updatePreview();
}

void SlideEditor::loadText()
{
    const TextBox *b = box();
    m_loading = true;
    if (b) {
        RichText::toDocument(m_text->document(), b->text, b->runs, b->style, kEditScale);
        // The editor background is dark; make sure dark text is still visible.
        if (b->style.color.lightness() < 60) m_text->setStyleSheet("QTextEdit { background: #c8c8c8; }");
        else m_text->setStyleSheet("QTextEdit { background: #1e1e1e; }");
    } else {
        m_text->clear();
    }
    m_loading = false;
}

void SlideEditor::storeText()
{
    if (m_loading) return;
    TextBox *b = box();
    if (!b) return;
    b->runs = RichText::fromDocument(m_text->document(), b->style, kEditScale);
    b->text = RichText::plainText(b->runs);
    if (QListWidgetItem *item = m_slides->item(m_slide))
        item->setText(QStringLiteral("%1\n%2").arg(slide()->label, slide()->mainText().section('\n', 0, 0)));
    updatePreview();
}

void SlideEditor::storeSlide()
{
    if (m_loading) return;
    Slide *s = slide();
    s->label = m_label->text();
    s->labelColor = m_labelColor->color();
    s->notes = m_notes->toPlainText();
    s->hasTransition = m_ownTransition->isChecked();
    s->transition.type = TransitionType(m_transition->currentIndex());
    s->transition.durationMs = m_transitionMs->value();
    m_transition->setEnabled(s->hasTransition);
    m_transitionMs->setEnabled(s->hasTransition);
    if (QListWidgetItem *item = m_slides->item(m_slide)) {
        QPixmap swatch(12, 28);
        swatch.fill(s->labelColor);
        item->setIcon(QIcon(swatch));
        item->setText(QStringLiteral("%1\n%2").arg(s->label, s->mainText().section('\n', 0, 0)));
    }
    updatePreview();
}

void SlideEditor::storeBox()
{
    if (m_loading) return;
    TextBox *b = box();
    if (!b) return;
    b->isMain = m_boxMain->isChecked();
    b->rect = m_rect->rect();
    b->style = m_style->style();
    b->build = BuildAnimation(m_build->currentIndex());
    b->buildDelayMs = m_buildDelay->value();
    b->buildDurationMs = m_buildMs->value();
    updatePreview();
}

void SlideEditor::chooseBackground(bool presentationDefault)
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose background"), QString(),
        tr("Media (*.jpg *.jpeg *.png *.bmp *.webp *.mp4 *.mov *.m4v *.mkv *.avi *.webm *.wmv)"));
    if (path.isEmpty()) {
        loadSlide();
        return;
    }
    if (presentationDefault) {
        m_pres.defaultBackground = Background::media(path);
        m_defBg->setText(m_pres.defaultBackground.describe());
        updatePreview();
    } else {
        slide()->background = Background::media(path);
        loadSlide();
    }
}

void SlideEditor::updatePreview()
{
    const Background bg = m_pres.effectiveBackground(m_slide);
    const QImage poster = bg.type == Background::Type::Image || bg.type == Background::Type::Video
                              ? m_media->poster(bg.path) : QImage();
    QImage img = TextRenderer::renderSlide(*slide(), bg, kPreview, nullptr, poster);
    // Outline the box being edited.
    if (const TextBox *b = box()) {
        QPainter p(&img);
        p.setPen(QPen(QColor(255, 170, 0), 1, Qt::DashLine));
        p.drawRect(QRectF(b->rect.x() * kPreview.width(), b->rect.y() * kPreview.height(),
                          b->rect.width() * kPreview.width(), b->rect.height() * kPreview.height()));
    }
    m_preview->setPixmap(QPixmap::fromImage(img));
}
