#pragma once

#include "core/Types.h"

class QTextDocument;

// Rich text is stored as paragraphs of runs, and each run only records how it
// differs from the box's base TextStyle:
//   [[{"t":"Amazing ","b":true}, {"t":"grace","c":"#ffffd700"}], [{"t":"..."}]]
// Keys: t text, b bold, i italic, u underline, s strike, c colour, f font, z size(px@1080).
// Keeping runs relative to the base means a screen can swap the base style
// (e.g. a lower third on the stream) and still keep the operator's emphasis.
namespace RichText {

// Fill `doc` with the box content rendered at `scale` (output height / 1080).
void toDocument(QTextDocument *doc, const QString &plain, const QJsonArray &runs,
                const TextStyle &base, double scale);

// Read an edited document back into runs, relative to `base` at `scale`.
QJsonArray fromDocument(const QTextDocument *doc, const TextStyle &base, double scale);

QString plainText(const QJsonArray &runs);

}
