#pragma once

#include <QDialog>

class BibleLibrary;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class BibleCatalog;

// Browse and install translations from the online Beblia and Zefania XML catalogs.
class BibleDownloadDialog : public QDialog {
    Q_OBJECT
public:
    explicit BibleDownloadDialog(BibleLibrary *library, QWidget *parent = nullptr);

signals:
    void bibleInstalled(const QString &id);

private:
    void populateLanguages();
    void refilter();
    void updateButtons();
    void startDownload();
    void setStatus(const QString &text, bool error = false);
    int currentEntry() const;   // index into catalog entries, -1 if none

    BibleLibrary *m_library;
    BibleCatalog *m_catalog;
    QLineEdit *m_search;
    QComboBox *m_language;
    QTableWidget *m_table;
    QPushButton *m_download;
    QPushButton *m_cancel;
    QPushButton *m_refresh;
    QProgressBar *m_progress;
    QLabel *m_status;
    QCheckBox *m_confirm;
    QString m_confirmFor;   // path the confirmation applies to
};
