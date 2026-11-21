#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class Bible;
struct Verse;

// One contiguous span. verseStart 0 = whole chapter(s); verseEnd 0 = to the
// end of chapterEnd. "Gen 1:1-2:3" is {1, 1, 2, 3}, "Ps 23" is {23, 0, 23, 0}.
struct VerseRange {
    int chapter = 0;
    int verseStart = 0;
    int chapterEnd = 0;
    int verseEnd = 0;

    bool operator==(const VerseRange &o) const
    {
        return chapter == o.chapter && verseStart == o.verseStart
               && chapterEnd == o.chapterEnd && verseEnd == o.verseEnd;
    }
    bool contains(int chapter, int verse) const;
};

// A parsed passage reference. "Rom 8:28, 31" keeps both spans in `ranges`;
// the accessors describe the first span for the common single-span case.
struct Reference {
    int book = 0;
    QList<VerseRange> ranges;

    bool isValid() const { return book > 0 && !ranges.isEmpty(); }
    int chapter() const { return ranges.value(0).chapter; }
    int verseStart() const { return ranges.value(0).verseStart; }
    int chapterEnd() const { return ranges.value(0).chapterEnd; }
    int verseEnd() const { return ranges.value(0).verseEnd; }
    bool contains(int chapter, int verse) const;

    // Accepts "John 3:16", "Jn 3:16-18", "jhn 3.16", "1Cor 13:4-7", "I Corinthians 13",
    // "Gen 1:1-2:3", "Rom 8:28, 31; 9:1", "Johannes 3:16", "Jude 3" (single-chapter
    // books take a bare number as the verse). `bible` adds its own book names.
    static Reference parse(const QString &text, const Bible *bible = nullptr, QString *error = nullptr);

    // Collapse a verse list (one book) into ranges, e.g. 16,17,18,20 -> 16-18, 20.
    // With `bible`, a run continuing into the next chapter stays one range.
    static Reference fromVerses(const QList<Verse> &verses, const Bible *bible = nullptr);

    // "John 3:16–18", "Romans 8:28, 31; 9:1", "Psalms 23", using the Bible's book name.
    QString format(const Bible *bible = nullptr) const;

    // Book names (the Bible's, then English and Afrikaans) starting with
    // `prefix`, diacritic-insensitive; an empty prefix lists them all.
    static QStringList bookCompletions(const QString &prefix = QString(), const Bible *bible = nullptr);
};
