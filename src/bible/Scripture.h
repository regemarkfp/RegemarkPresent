#pragma once

#include "bible/Bible.h"
#include "core/Types.h"

// Turns a passage into a presentation: verse text in the main box, the
// reference and translation in a small second box at the bottom.
namespace Scripture {

struct Options {
    int maxChars = 220;        // per slide; longer verses split at word boundaries (0 = never)
    bool combineVerses = false; // pack consecutive verses up to maxChars instead of one per slide
    bool verseNumbers = true;  // lead each verse with a smaller "16 "
    bool referenceBox = true;  // "John 3:16 · KJV" along the bottom
    QString translation;       // tag for the reference box; empty = bible.abbreviation()
};

inline const QColor kLabelColor{0x14, 0xa0, 0xa8};  // teal, distinct from song section colours

// `templ` supplies backgrounds/transition/base style (e.g. the app's default
// presentation); the verse style is derived from its defaultStyle.
Presentation toPresentation(const QList<Verse> &verses, const Bible &bible,
                            const Options &options = Options(), const Presentation &templ = Presentation());

// Base style used for verse text: templ style, left-aligned, 64px.
TextStyle verseStyle(const TextStyle &base);

// Split text into pieces of at most ~maxChars at word boundaries, balanced so
// the last piece isn't a stub.
QStringList splitText(const QString &text, int maxChars);

}
