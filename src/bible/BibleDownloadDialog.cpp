#include "bible/BibleDownloadDialog.h"

#include "bible/BibleLibrary.h"
#include "bible/BookNames.h"
#include "bible/BibleCatalog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

static const char *const kFeatured = "*featured";  // English + Afrikaans
static const char *const kAll = "*all";

enum Column { ColName, ColLanguage, ColSource, ColSize, ColStatus, ColCount };

BibleDownloadDialog::BibleDownloadDialog(BibleLibrary *library, QWidget *parent)
    : QDialog(parent), m_library(library), m_catalog(new BibleCatalog(library, this))
{
    setWindowTitle(tr("Get Bibles"));
    resize(820, 560);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search translations…"));
    m_search->setClearButtonEnabled(true);
    m_language = new QComboBox;
    m_language->setMinimumContentsLength(18);
    m_refresh = new QPushButton(tr("Refresh list"));
    m_refresh->setToolTip(tr("The list is cached for a week; refresh to fetch it again now."));

    auto *filters = new QHBoxLayout;
    filters->addWidget(m_search, 1);
    filters->addWidget(m_language);
    filters->addWidget(m_refresh);

    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels({tr("Translation"), tr("Language"), tr("Source"), tr("Size"), tr("Status")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    for (int c = ColLanguage; c < ColCount; ++c)
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(true);
    m_progress->hide();
    m_download = new QPushButton(tr("Download"));
    m_download->setDefault(true);
    m_cancel = new QPushButton(tr("Cancel download"));
    m_cancel->hide();

    // Most modern translations are copyrighted: anything not obviously public
    // domain needs the user to confirm their church may use it.
    m_confirm = new QCheckBox(tr("Our church may use this translation (it is public domain, or we have "
                                 "permission or a licence)"));
    m_confirm->hide();
    connect(m_confirm, &QCheckBox::toggled, this, &BibleDownloadDialog::updateButtons);

    auto *actions = new QHBoxLayout;
    actions->addWidget(m_status, 1);
    actions->addWidget(m_progress, 1);
    actions->addWidget(m_cancel);
    actions->addWidget(m_download);

    auto *note = new QLabel(tr("Translations come from the <a href=\"https://github.com/%1\">Beblia</a> and "
                               "<a href=\"https://github.com/%2\">Zefania XML</a> projects. Many modern "
                               "translations are copyrighted: you are responsible for having the right to "
                               "display a translation in your services.")
                                .arg(QLatin1String(BibleCatalog::kBebliaRepo), QLatin1String(BibleCatalog::kZefaniaRepo)));
    note->setWordWrap(true);
    note->setOpenExternalLinks(true);
    note->setForegroundRole(QPalette::PlaceholderText);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_confirm);
    layout->addLayout(actions);
    layout->addWidget(note);
    layout->addWidget(close);

    connect(m_search, &QLineEdit::textChanged, this, &BibleDownloadDialog::refilter);
    connect(m_language, &QComboBox::currentIndexChanged, this, &BibleDownloadDialog::refilter);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &BibleDownloadDialog::updateButtons);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &BibleDownloadDialog::startDownload);
    connect(m_download, &QPushButton::clicked, this, &BibleDownloadDialog::startDownload);
    connect(m_cancel, &QPushButton::clicked, m_catalog, &BibleCatalog::cancelDownload);
    connect(m_refresh, &QPushButton::clicked, this, [this] {
        setStatus(tr("Fetching the list…"));
        m_refresh->setEnabled(false);
        m_catalog->refresh(true);
    });

    connect(m_catalog, &BibleCatalog::catalogReady, this, [this] {
        m_refresh->setEnabled(true);
        setStatus(tr("%n translation(s) available.", nullptr, int(m_catalog->entries().size())));
        populateLanguages();
        refilter();
    });
    connect(m_catalog, &BibleCatalog::notice, this, [this](const QString &msg) { setStatus(msg); });
    connect(m_catalog, &BibleCatalog::catalogError, this, [this](const QString &msg) {
        m_refresh->setEnabled(true);
        setStatus(msg, true);
    });
    connect(m_catalog, &BibleCatalog::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_progress->setRange(0, total > 0 ? 1000 : 0);
        if (total > 0) m_progress->setValue(int(received * 1000 / total));
        m_progress->setFormat(QLocale().formattedDataSize(received));
    });
    auto finish = [this] {
        m_progress->hide();
        m_cancel->hide();
        m_status->show();
        refilter();
    };
    connect(m_catalog, &BibleCatalog::downloadFinished, this, [this, finish](const QString &id) {
        finish();
        setStatus(tr("Installed %1.").arg(m_library->entry(id).name));
        emit bibleInstalled(id);
    });
    connect(m_catalog, &BibleCatalog::downloadFailed, this, [this, finish](const QString &msg) {
        finish();
        setStatus(msg, true);
    });

    setStatus(tr("Fetching the list…"));
    updateButtons();
    QTimer::singleShot(0, m_catalog, [this] { m_catalog->refresh(); });
}

void BibleDownloadDialog::setStatus(const QString &text, bool error)
{
    m_status->setText(text);
    // Theme-neutral red that reads on both the dark and light palettes.
    m_status->setStyleSheet(error ? QStringLiteral("color: #e5534b;") : QString());
}

void BibleDownloadDialog::populateLanguages()
{
    const QString previous = m_language->currentData().toString();
    QMap<QString, int> counts;  // language name -> count
    for (const CatalogEntry &e : m_catalog->entries()) ++counts[e.language];

    QSignalBlocker block(m_language);
    m_language->clear();
    m_language->addItem(tr("English + Afrikaans"), QString::fromLatin1(kFeatured));
    m_language->addItem(tr("All languages (%1)").arg(m_catalog->entries().size()), QString::fromLatin1(kAll));
    m_language->insertSeparator(m_language->count());
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        m_language->addItem(QStringLiteral("%1 (%2)").arg(it.key()).arg(it.value()), it.key());
    const int i = m_language->findData(previous);
    m_language->setCurrentIndex(i >= 0 ? i : 0);
}

void BibleDownloadDialog::refilter()
{
    const QString lang = m_language->currentData().toString();
    const QString needle = BookNames::stripDiacritics(m_search->text().trimmed()).toLower();
    const QString selectedPath = currentEntry() >= 0 ? m_catalog->entries()[currentEntry()].path : QString();
    const QList<CatalogEntry> entries = m_catalog->entries();

    m_table->setRowCount(0);
    for (int i = 0; i < entries.size(); ++i) {
        const CatalogEntry &e = entries[i];
        if (lang == QLatin1String(kFeatured)) {
            // Searching widens the default view to every language.
            if (needle.isEmpty() && e.languageCode != "ENG" && e.languageCode != "AFR") continue;
        } else if (lang != QLatin1String(kAll) && !lang.isEmpty() && e.language != lang) {
            continue;
        }
        if (!needle.isEmpty()) {
            const QString hay = BookNames::stripDiacritics(e.name + ' ' + e.module + ' ' + e.language + ' '
                                                           + e.path + ' ' + e.sourceName()).toLower();
            if (!hay.contains(needle)) continue;
        }
        const bool installed = m_catalog->isInstalled(e);
        const bool busy = m_catalog->isDownloading() && m_catalog->downloadingPath() == e.path;
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *name = new QTableWidgetItem(e.name);
        name->setData(Qt::UserRole, i);
        name->setToolTip(e.date.isEmpty() ? e.path : QStringLiteral("%1\n%2").arg(e.path, e.date));
        m_table->setItem(row, ColName, name);
        m_table->setItem(row, ColLanguage, new QTableWidgetItem(e.language));
        m_table->setItem(row, ColSource, new QTableWidgetItem(e.sourceName()));
        m_table->setItem(row, ColSize, new QTableWidgetItem(e.size > 0 ? QLocale().formattedDataSize(e.size) : QString()));
        m_table->setItem(row, ColStatus, new QTableWidgetItem(busy ? tr("Downloading…")
                                                              : installed ? tr("Installed")
                                                              : e.publicDomain ? tr("Public domain") : QString()));
        if (e.path == selectedPath) m_table->selectRow(row);
    }
    updateButtons();
}

int BibleDownloadDialog::currentEntry() const
{
    const QList<QTableWidgetItem *> sel = m_table->selectedItems();
    for (QTableWidgetItem *it : sel)
        if (it->column() == ColName) return it->data(Qt::UserRole).toInt();
    return -1;
}

void BibleDownloadDialog::updateButtons()
{
    const int i = currentEntry();
    const bool busy = m_catalog->isDownloading();
    const CatalogEntry e = i >= 0 ? m_catalog->entries()[i] : CatalogEntry();
    if (e.path != m_confirmFor) {  // the confirmation is per translation
        m_confirmFor = e.path;
        const QSignalBlocker block(m_confirm);
        m_confirm->setChecked(false);
    }
    const bool needsConfirm = i >= 0 && !e.publicDomain;
    m_confirm->setVisible(needsConfirm);
    m_download->setEnabled(i >= 0 && !busy && (!needsConfirm || m_confirm->isChecked()));
    m_download->setText(i >= 0 && m_catalog->isInstalled(m_catalog->entries()[i]) ? tr("Reinstall") : tr("Download"));
}

void BibleDownloadDialog::startDownload()
{
    const int i = currentEntry();
    if (i < 0 || m_catalog->isDownloading()) return;
    const CatalogEntry e = m_catalog->entries()[i];
    if (!e.publicDomain && !m_confirm->isChecked()) {
        setStatus(tr("Tick the box to confirm your church may use %1.").arg(e.name), true);
        return;
    }
    if (!m_catalog->download(e)) return;
    m_status->hide();
    m_progress->setRange(0, 0);
    m_progress->setFormat(QString());
    m_progress->show();
    m_cancel->show();
    refilter();
}
