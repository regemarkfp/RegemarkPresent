#pragma once

#include <QSize>
#include <QStringList>

class OutputSink;

// Blackmagic DeckLink SDI/HDMI playout. Built only when DECKLINK_SDK_DIR is
// set; otherwise DeckLinkStub.cpp provides these and reports it as missing.
QStringList deckLinkDevices(QString *error = nullptr);

// `key` enables the card's external keyer: fill on one connector, key (alpha)
// on the other, which is what a vision mixer wants for a stream overlay.
OutputSink *createDeckLinkOutput(int device, QSize resolution, double fps, bool key);
