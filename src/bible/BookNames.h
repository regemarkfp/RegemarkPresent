#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Standard book tables, numbered like Zefania (1 Genesis .. 66 Revelation,
// 67+ apocrypha). Used when a file lacks bname, to show names in the
// congregation's language, and to match typed references.
namespace BookNames {

QString english(int book);       // empty for unknown numbers
QString afrikaans(int book);     // empty outside 1..66
QString standard(int book, const QString &languageCode);  // falls back to English, then "Book N"

// Book number for a typed name ("1 Cor", "Psalm", "Hebreërs"), 0 if unknown.
// `extra` adds names to try exactly (a loaded Bible's own bname/bsname);
// `candidates` receives the books an ambiguous prefix could mean.
int match(const QString &name, const QList<QPair<int, QString>> &extra = {},
          QList<int> *candidates = nullptr);

// Lowercase, strip diacritics/spaces/punctuation and turn a leading roman or
// ordinal prefix into a digit: "I Korintiërs" -> "1korintiers".
QString key(const QString &name);
QString stripDiacritics(const QString &s);

bool isSingleChapter(int book);  // Obadiah, Philemon, 2/3 John, Jude
int canonicalCount();            // 66

// "ENG"/"AFR"/... from whatever a file puts in <language> ("en", "English", "AFR").
QString canonicalLanguage(const QString &language);

// Display names for a book completer, English and Afrikaans.
QStringList allNames();

}
