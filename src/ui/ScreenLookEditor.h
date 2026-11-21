#pragma once

#include "core/ScreenConfig.h"

#include <QWidget>

class ColorButton;
class QCheckBox;
class QGroupBox;
class RectEditor;
class StyleEditor;

// Edits what one screen shows (ScreenLook): backgrounds/text on or off,
// transparency, its own text position and style, a band behind the text,
// and the confidence layout.
class ScreenLookEditor : public QWidget {
    Q_OBJECT
public:
    explicit ScreenLookEditor(QWidget *parent = nullptr);

    void setLook(const ScreenLook &look, ScreenRole role);
    ScreenLook look() const;

signals:
    void changed();

private:
    QCheckBox *m_showBg, *m_showText, *m_transparent;
    QCheckBox *m_overrideRect, *m_overrideStyle, *m_useBar;
    RectEditor *m_rect;
    StyleEditor *m_style;
    ColorButton *m_bar;
    QGroupBox *m_confBox;
    QCheckBox *m_showNext, *m_showClock, *m_showNotes, *m_showLabel;
    bool m_loading = false;
};
