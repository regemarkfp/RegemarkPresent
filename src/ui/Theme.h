#pragma once

#include <QColor>

class QApplication;

// One place for the app's look, modelled on regemarkfp.com: near-black
// surfaces, slanted (parallelogram) buttons, inputs, tabs and cards, bold
// accent headings. The accent is the purple from the logo.
namespace Theme {

inline const QColor accent{0xa2, 0x59, 0xff};       // #a259ff
inline const QColor accentHover{0xb8, 0x80, 0xff};
inline const QColor accentDeep{0x5b, 0x2a, 0xa8};
inline const QColor onAccent{0x0b, 0x0b, 0x0b};     // text on accent fills, as on the site
inline const QColor window{0x0f, 0x0f, 0x0f};       // --bg
inline const QColor panel{0x1b, 0x1b, 0x1b};        // --panel
inline const QColor raised{0x27, 0x27, 0x27};       // --panel2
inline const QColor hover{0x3a, 0x3a, 0x3a};
inline const QColor base{0x15, 0x15, 0x15};
inline const QColor border{0x30, 0x30, 0x30};       // --line
inline const QColor text{0xf1, 0xf1, 0xf1};         // --text
inline const QColor textDim{0xaa, 0xaa, 0xaa};      // --dim
inline const QColor danger{0xff, 0x3b, 0x3b};       // --live

// Slant of the parallelograms, in pixels (the site's --slant family).
constexpr double kButtonSlant = 8;
constexpr double kInputSlant = 6;
constexpr double kTabSlant = 7;
constexpr double kCardSlant = 12;

void apply(QApplication &app);

}
