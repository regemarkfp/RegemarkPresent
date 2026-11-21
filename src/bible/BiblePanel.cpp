#include "bible/BiblePanel.h"

#include "bible/BibleDownloadDialog.h"
#include "bible/BibleLibrary.h"
#include "bible/Scripture.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringListModel>
#include <QToolButton>
#include <QVBoxLayout>

BiblePanel::BiblePanel(BibleLibrary *library, QWidget *parent)
    : QWidget(parent), m_library(library)
{
    QSettings settings;

    m_translation = new QComboBox;
    m_translation->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_translation->setMinimumContentsLength(12);
    m_translation->setToolTip(tr("Translation"));
    m_getBibles = new QPushButton(tr("Get Bibles…"));
    m_getBibles->setToolTip(tr("Download translations from the online Zefania XML library"));
    m_import = new QPushButton(tr("Import XML…"));
    m_import->setToolTip(tr("Install a Zefania XML Bible file from disk"));
    auto *top = new QHBoxLayout;
    top->addWidget(m_translation, 1);
    top->addWidget(m_getBibles);
    top->addWidget(m_import);

    m_reference = new QLineEdit;
    m_reference->setPlaceholderText(tr("Reference, e.g. John 3:16-18 or Ps 23"));
    m_reference->setClearButtonEnabled(true);
    m_bookModel = new QStringListModel(this);
    m_completer = new QCompleter(m_bookModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchStartsWith);
    m_reference->setCompleter(m_completer);
    auto *goButton = new QToolButton;
    goButton->setText(tr("Go"));
    auto *refRow = new QHBoxLayout;
    refRow->addWidget(m_reference, 1);
    refRow->addWidget(goButton);

    m_book = new QComboBox;
    m_book->setToolTip(tr("Book"));
    m_book->setMaxVisibleItems(20);
    m_chapter = new QSpinBox;
    m_chapter->setPrefix(tr("Ch "));
    m_chapter->setRange(1, 1);
    m_chapter->setToolTip(tr("Chapter"));
    auto *pickRow = new QHBoxLayout;
    pickRow->addWidget(m_book, 1);
    pickRow->addWidget(m_chapter);

    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setWordWrap(true);
    m_list->setAlternatingRowColors(true);
    m_list->setUniformItemSizes(false);
    m_list->setToolTip(tr("Select verses to show only those; none selected shows them all"));

    m_combine = new QCheckBox(tr("Combine verses"));
    m_combine->setToolTip(tr("Put several short verses on one slide, up to the character limit"));
    m_combine->setChecked(settings.value("bible/combineVerses", false).toBool());
    m_maxChars = new QSpinBox;
    m_maxChars->setRange(60, 1000);
    m_maxChars->setSingleStep(20);
    m_maxChars->setSuffix(tr(" chars"));
    m_maxChars->setToolTip(tr("Most characters per slide; longer verses are split across slides"));
    m_maxChars->setValue(settings.value("bible/maxChars", 220).toInt());
    auto *optRow = new QHBoxLayout;
    optRow->addWidget(m_combine);
    optRow->addStretch(1);
    optRow->addWidget(new QLabel(tr("Max per slide")));
    optRow->addWidget(m_maxChars);

    m_show = new QPushButton(tr("Show"));
    m_show->setToolTip(tr("Open the passage for sending live"));
    m_add = new QPushButton(tr("Add to playlist"));
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_show, 1);
    btnRow->addWidget(m_add, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(top);
    layout->addLayout(refRow);
    layout->addLayout(pickRow);
    layout->addWidget(m_message);
    layout->addWidget(m_list, 1);
    layout->addLayout(optRow);
    layout->addLayout(btnRow);

    connect(m_translation, &QComboBox::currentIndexChanged, this, &BiblePanel::onTranslationChanged);
    connect(m_getBibles, &QPushButton::clicked, this, &BiblePanel::getBibles);
    connect(m_import, &QPushButton::clicked, this, &BiblePanel::importXml);
    connect(m_reference, &QLineEdit::returnPressed, this, &BiblePanel::go);
    connect(goButton, &QToolButton::clicked, this, &BiblePanel::go);
    connect(m_book, &QComboBox::currentIndexChanged, this, [this] {
        // A new book starts at chapter 1 (the spinbox change shows it).
        const int count = m_bible ? m_bible->chapterCount(m_book->currentData().toInt()) : 1;
        const QSignalBlocker block(m_chapter);
        m_chapter->setRange(1, qMax(1, count));
        m_chapter->setValue(1);
        onBookOrChapterPicked();
    });
    connect(m_chapter, &QSpinBox::valueChanged, this, &BiblePanel::onBookOrChapterPicked);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &BiblePanel::updateButtons);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] {
        if (!chosenVerses().isEmpty()) emit presentationReady(currentPresentation());
    });
    connect(m_show, &QPushButton::clicked, this, [this] {
        if (!chosenVerses().isEmpty()) emit presentationReady(currentPresentation());
    });
    connect(m_add, &QPushButton::clicked, this, [this] {
        if (!chosenVerses().isEmpty()) emit addToPlaylist(currentPresentation());
    });
    connect(m_combine, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue("bible/combineVerses", on); });
    connect(m_maxChars, &QSpinBox::valueChanged, this, [](int v) { QSettings().setValue("bible/maxChars", v); });
    connect(m_library, &BibleLibrary::biblesChanged, this, &BiblePanel::refreshTranslations);

    refreshTranslations();
}

QString BiblePanel::currentBibleId() const
{
    return m_translation->currentData().toString();
}

void BiblePanel::refreshTranslations()
{
    const QString current = currentBibleId().isEmpty() ? QSettings().value("bible/translation").toString()
                                                       : currentBibleId();
    {
        const QSignalBlocker block(m_translation);
        m_translation->clear();
        for (const BibleEntry &e : m_library->installed()) {
            const QString text = e.abbreviation.isEmpty() || e.abbreviation == e.name
                                     ? e.name : QStringLiteral("%1 — %2").arg(e.abbreviation, e.name);
            m_translation->addItem(text, e.id);
            m_translation->setItemData(m_translation->count() - 1, e.language, Qt::ToolTipRole);
        }
        const int i = m_translation->findData(current);
        m_translation->setCurrentIndex(i >= 0 ? i : 0);
    }
    m_translation->setEnabled(m_translation->count() > 0);
    // Reload only if the selection actually moved (or the Bible was removed).
    if (currentBibleId() != current || !m_bible || !m_library->contains(current)) onTranslationChanged();
}

void BiblePanel::selectTranslation(const QString &id)
{
    const int i = m_translation->findData(id);
    if (i >= 0) m_translation->setCurrentIndex(i);
}

void BiblePanel::onTranslationChanged()
{
    const QString id = currentBibleId();
    m_bible.reset();
    if (id.isEmpty()) {
        m_bookModel->setStringList(Reference::bookCompletions());
        const QSignalBlocker block(m_book);
        m_book->clear();
        m_list->clear();
        m_verses.clear();
        setMessage(tr("No Bibles installed — click Get Bibles…"), true);
        updateButtons();
        return;
    }
    QSettings().setValue("bible/translation", id);

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_bible = m_library->load(id, &error);
    QApplication::restoreOverrideCursor();
    if (!m_bible) {
        setMessage(tr("Couldn't open this translation: %1").arg(error), true);
        updateButtons();
        return;
    }

    m_bookModel->setStringList(Reference::bookCompletions(QString(), m_bible.get()));
    {
        const QSignalBlocker block(m_book);
        m_book->clear();
        for (const BookInfo &b : m_bible->books()) m_book->addItem(b.name, b.number);
    }
    // Keep showing the same passage in the new translation.
    if (m_ref.isValid()) display(m_ref);
    else if (!m_reference->text().trimmed().isEmpty()) go();
    else display(Reference::parse(QStringLiteral("John 3"), m_bible.get()));
}

void BiblePanel::onBookOrChapterPicked()
{
    if (!m_bible || m_book->currentIndex() < 0) return;
    Reference ref;
    ref.book = m_book->currentData().toInt();
    ref.ranges.append({m_chapter->value(), 0, m_chapter->value(), 0});
    display(ref);
}

bool BiblePanel::showReference(const QString &text)
{
    m_reference->setText(text);
    return go();
}

bool BiblePanel::go()
{
    if (!m_bible) {
        setMessage(m_library->installed().isEmpty() ? tr("No Bibles installed — click Get Bibles…")
                                                    : tr("Choose a translation first."), true);
        return false;
    }
    QString error;
    const Reference ref = Reference::parse(m_reference->text(), m_bible.get(), &error);
    if (!ref.isValid()) {
        setMessage(error, true);
        return false;
    }
    return display(ref);
}

bool BiblePanel::display(const Reference &ref)
{
    if (!m_bible || !ref.isValid()) return false;
    const QList<Verse> verses = m_bible->range(ref);
    if (verses.isEmpty()) {
        // Don't leave another translation's verses behind the error.
        m_list->clear();
        m_verses.clear();
        updateButtons();
        setMessage(m_bible->hasBook(ref.book)
                       ? tr("%1 isn't in this translation.").arg(ref.format(m_bible.get()))
                       : tr("%1 isn't in this translation.").arg(m_bible->bookName(ref.book)),
                   true);
        return false;
    }
    m_ref = ref;
    m_verses = verses;
    m_reference->setText(ref.format(m_bible.get()));

    {
        const QSignalBlocker b1(m_book), b2(m_chapter);
        m_book->setCurrentIndex(m_book->findData(ref.book));
        m_chapter->setRange(1, qMax(1, m_bible->chapterCount(ref.book)));
        m_chapter->setValue(ref.chapter());
    }

    m_list->clear();
    const bool multiChapter = verses.first().chapter != verses.last().chapter;
    for (int i = 0; i < verses.size(); ++i) {
        const Verse &v = verses[i];
        const QString num = multiChapter ? QStringLiteral("%1:%2").arg(v.chapter).arg(v.verse) : QString::number(v.verse);
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(num, v.text), m_list);
        item->setData(Qt::UserRole, i);
    }
    m_list->scrollToTop();
    setMessage(QString());
    updateButtons();
    return true;
}

void BiblePanel::setMessage(const QString &text, bool error)
{
    m_message->setText(text);
    m_message->setVisible(!text.isEmpty());
    // Theme-neutral red that reads on both the dark and light palettes.
    m_message->setStyleSheet(error ? QStringLiteral("color: #e5534b;") : QString());
}

QList<Verse> BiblePanel::chosenVerses() const
{
    QList<Verse> out;
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->isSelected()) out.append(m_verses.value(m_list->item(i)->data(Qt::UserRole).toInt()));
    return out.isEmpty() ? m_verses : out;
}

Presentation BiblePanel::currentPresentation() const
{
    const QList<Verse> verses = chosenVerses();
    if (!m_bible || verses.isEmpty()) return Presentation();
    Scripture::Options opt;
    opt.maxChars = m_maxChars->value();
    opt.combineVerses = m_combine->isChecked();
    opt.translation = m_library->entry(currentBibleId()).abbreviation;
    return Scripture::toPresentation(verses, *m_bible, opt, m_template);
}

void BiblePanel::updateButtons()
{
    const bool any = m_bible && !m_verses.isEmpty();
    m_show->setEnabled(any);
    m_add->setEnabled(any);
    const int selected = int(m_list->selectedItems().size());
    m_show->setText(selected > 0 ? tr("Show %n verse(s)", nullptr, selected) : tr("Show"));
}

void BiblePanel::importXml()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Zefania XML Bible"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("Zefania XML Bible (*.xml);;All files (*)"));
    if (path.isEmpty()) return;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QString id = m_library->importFile(path, &error);
    QApplication::restoreOverrideCursor();
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Import Bible"), error);
        return;
    }
    selectTranslation(id);
}

void BiblePanel::getBibles()
{
    BibleDownloadDialog dlg(m_library, this);
    QString installed;
    connect(&dlg, &BibleDownloadDialog::bibleInstalled, this, [&installed](const QString &id) { installed = id; });
    dlg.exec();
    // First Bible, or the user had none selected: switch to what they just got.
    if (!installed.isEmpty() && (m_translation->count() == 1 || !m_bible)) selectTranslation(installed);
}
