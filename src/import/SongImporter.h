#pragma once

#include "core/Types.h"

// Imports songs downloaded from CCLI SongSelect (and similar sources):
//   .usr        SongSelect "USR" import file
//   .txt        SongSelect lyrics text download
//   .cho/.chordpro/.chopro/.pro  ChordPro (chords are stripped)
namespace SongImporter {

struct Section {
    QString label;
    QStringList lines;
};

struct Song {
    QString title, author, copyright, ccliNumber;
    QList<Section> sections;
};

bool parseFile(const QString &path, Song *song, QString *error);
Song parseUsr(const QString &content);
Song parseSongSelectText(const QString &content);
Song parseChordPro(const QString &content);

// Build a presentation, splitting each section into slides of `linesPerSlide`.
Presentation toPresentation(const Song &song, int linesPerSlide, const Presentation &templ);

QStringList fileFilters();

}
