#pragma once

#include "bible/Bible.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <memory>

struct BibleEntry {
    QString id;            // file stem under the bibles folder
    QString name;          // "King James Version"
    QString language;      // display name, "English"
    QString languageCode;  // "ENG"
    QString abbreviation;  // "KJV", shown on slides
    QString path;
    QString source;        // catalog path it was downloaded from, if any
};

// Installed translations: Zefania XML files in AppDataLocation/bibles stored
// as <id>.xml, with an index.json for names that the files get wrong.
class BibleLibrary : public QObject {
    Q_OBJECT
public:
    explicit BibleLibrary(QObject *parent = nullptr);   // default location
    explicit BibleLibrary(const QString &root, QObject *parent = nullptr);

    QString root() const { return m_root; }
    void refresh();                         // rescan the folder

    QList<BibleEntry> installed() const { return m_entries; }  // sorted by language, name
    bool contains(const QString &id) const;
    BibleEntry entry(const QString &id) const;

    // Validates by parsing, then copies the file in. Non-empty fields of
    // `hint` (id, name, language, abbreviation, source) override what the file
    // says. Returns the new id, or empty with `error` set.
    QString importFile(const QString &path, QString *error = nullptr, const BibleEntry &hint = BibleEntry());
    bool remove(const QString &id);
    void setAbbreviation(const QString &id, const QString &abbreviation);

    // Parsed Bible, cached (the three most recently used stay in memory).
    std::shared_ptr<const Bible> load(const QString &id, QString *error = nullptr);

    static QString makeId(const QString &text);

signals:
    void biblesChanged();

private:
    void saveIndex();
    BibleEntry entryFor(const QString &id, const BibleInfo &info, const QString &path) const;
    void cache(const QString &id, const std::shared_ptr<const Bible> &bible);

    QString m_root;
    QList<BibleEntry> m_entries;
    QJsonObject m_index;
    QList<QPair<QString, std::shared_ptr<const Bible>>> m_cache;  // most recent first
};
