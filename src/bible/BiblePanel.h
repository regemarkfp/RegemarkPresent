#pragma once

#include "bible/Bible.h"
#include "core/Types.h"

#include <QWidget>
#include <memory>

class BibleLibrary;
class QCheckBox;
class QComboBox;
class QCompleter;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStringListModel;

// Scripture lookup: pick a translation, type a reference (or pick book and
// chapter), preview the verses and send them out as a presentation.
class BiblePanel : public QWidget {
    Q_OBJECT
public:
    explicit BiblePanel(BibleLibrary *library, QWidget *parent = nullptr);

    // Backgrounds, transition and base text style for generated presentations.
    void setTemplate(const Presentation &templ) { m_template = templ; }

    QString currentBibleId() const;
    std::shared_ptr<const Bible> currentBible() const { return m_bible; }
    // Parse and preview a reference; false (with the error shown) if it fails.
    bool showReference(const QString &text);
    // The selected verses, or every previewed verse when none are selected.
    Presentation currentPresentation() const;

signals:
    void presentationReady(const Presentation &presentation);   // "Show": send live
    void addToPlaylist(const Presentation &presentation);

private:
    void refreshTranslations();
    void selectTranslation(const QString &id);
    void onTranslationChanged();
    void onBookOrChapterPicked();
    bool go();
    bool display(const Reference &ref);
    void setMessage(const QString &text, bool error = false);
    void updateButtons();
    void importXml();
    void getBibles();
    QList<Verse> chosenVerses() const;

    BibleLibrary *m_library;
    std::shared_ptr<const Bible> m_bible;
    Presentation m_template;
    Reference m_ref;
    QList<Verse> m_verses;

    QComboBox *m_translation;
    QPushButton *m_getBibles;
    QPushButton *m_import;
    QLineEdit *m_reference;
    QCompleter *m_completer;
    QStringListModel *m_bookModel;
    QComboBox *m_book;
    QSpinBox *m_chapter;
    QLabel *m_message;
    QListWidget *m_list;
    QCheckBox *m_combine;
    QSpinBox *m_maxChars;
    QPushButton *m_show;
    QPushButton *m_add;
};
