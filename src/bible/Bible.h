#pragma once

#include "bible/Reference.h"

#include <QList>
#include <QMap>
#include <QString>

class QIODevice;

struct Verse {
    int book = 0;
    int chapter = 0;
    int verse = 0;
    QString text;
};

struct BookInfo {
    int number = 0;
    QString name;        // display name (see Bible::bookName)
    QString fileName;    // bname as written in the file (may be empty)
    QString shortName;   // bsname as written in the file (may be empty)
    int chapters = 0;
};

// Header fields readable without parsing the whole file.
struct BibleInfo {
    QString name;
    QString language;    // as written in the file, e.g. "ENG"
    QString identifier;
    QString rights;      // Zefania <rights>, Beblia status ("Public Domain", "© ...")
    QString description;
    QString link;        // Beblia link attribute
    bool beblia = false; // <bible><testament><book number> layout rather than Zefania
};

// One translation loaded into memory from a Zefania XML or Beblia XML file
// (a full Bible is ~31k verses, a few MB of text).
class Bible {
public:
    bool load(const QString &path, QString *error = nullptr);
    bool load(QIODevice *device, QString *error = nullptr);
    // Reads only the root element and <INFORMATION>, stopping at the first book.
    static bool readInfo(const QString &path, BibleInfo *info, QString *error = nullptr);

    // Beblia files carry no language; it is in the file name:
    // "Afrikaans1983Bible.xml" -> ("Afrikaans", "1983"), "NorthernSothoBible.xml"
    // -> ("Northern Sotho", ""), "AmharicDawroDFBEBible.xml" -> ("Amharic", "Dawro DFBE").
    static bool bebliaName(const QString &fileName, QString *language, QString *version = nullptr);

    // For files whose language comes from outside (catalog, library index).
    void setLanguage(const QString &language) { m_info.language = language; }

    bool isEmpty() const { return m_books.isEmpty(); }
    QString name() const { return m_info.name; }
    QString language() const { return m_info.language; }
    QString identifier() const { return m_info.identifier; }
    QString rights() const { return m_info.rights; }
    const BibleInfo &info() const { return m_info; }
    // Short tag for slides ("KJV", "AFR83"); from the identifier or Beblia translation name.
    QString abbreviation() const;
    static QString abbreviationFor(const QString &identifier, const QString &name);
    static QString abbreviationOf(const BibleInfo &info);
    // "Afrikaans","1983" -> "AFR83"; "English","KJ" -> "KJV"; "Afrikaans","NLV" -> "NLV".
    static QString bebliaAbbreviation(const QString &language, const QString &version);

    QList<BookInfo> books() const;
    bool hasBook(int book) const { return m_books.contains(book); }
    // English/Afrikaans files get the standard names in that language (many
    // files carry German or English bnames regardless); others use bname.
    QString bookName(int book) const;
    int chapterCount(int book) const;          // highest chapter number present
    int verseCount(int book, int chapter) const;
    int totalVerses() const { return m_verseTotal; }

    QList<Verse> chapter(int book, int chapter) const;
    QList<Verse> range(const Reference &ref) const;
    Verse verse(int book, int chapter, int verse) const;

private:
    struct Book {
        BookInfo info;
        QMap<int, QList<Verse>> chapters;
    };
    BibleInfo m_info;
    QMap<int, Book> m_books;
    int m_verseTotal = 0;
};
