#pragma once

#include "core/Types.h"

// Brings PDF and PowerPoint files in as slide decks. Each page becomes a
// full-screen image slide, which is how sermon decks usually arrive and
// keeps their exact look. Pages are rendered once into the library's
// Media/Imported folder.
//   PDF:        Qt PDF (when built with it)
//   PowerPoint: Microsoft PowerPoint (Windows) exports slides directly;
//               otherwise LibreOffice converts to PDF first.
namespace DeckImporter {

bool pdfSupported();
QStringList fileFilters();

// Render pages/slides to PNGs in `outDir`; returns the files in order.
QStringList renderPdf(const QString &pdf, const QString &outDir, QString *error);
QStringList renderPowerPoint(const QString &pptx, const QString &outDir, QString *error);

// Everything in one go: render `path` and build a deck presentation.
bool importFile(const QString &path, const QString &mediaRoot, Presentation *out, QString *error);

}
