#pragma once

#include <QColor>

class QApplication;

// One place for the app's look. The accent is the same purple as the logo.
namespace Theme {

inline const QColor accent{0xa2, 0x59, 0xff};       // #a259ff
inline const QColor accentHover{0xb8, 0x80, 0xff};
inline const QColor accentDeep{0x5b, 0x2a, 0xa8};
inline const QColor window{0x1b, 0x1b, 0x1e};
inline const QColor panel{0x23, 0x23, 0x27};
inline const QColor base{0x16, 0x16, 0x19};
inline const QColor raised{0x2d, 0x2d, 0x33};
inline const QColor border{0x38, 0x38, 0x40};
inline const QColor text{0xe8, 0xe8, 0xee};
inline const QColor textDim{0x8c, 0x8c, 0x99};
inline const QColor danger{0xd9, 0x3f, 0x4c};

void apply(QApplication &app);

}
