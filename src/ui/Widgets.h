#pragma once

#include "core/Types.h"

#include <QToolButton>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;

class ColorButton : public QToolButton {
    Q_OBJECT
public:
    explicit ColorButton(QWidget *parent = nullptr, bool alpha = false);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);

signals:
    void colorChanged(const QColor &c);

private:
    QColor m_color;
    bool m_alpha;
};

// Edits a TextStyle (base font, colour, alignment, outline, shadow...).
class StyleEditor : public QWidget {
    Q_OBJECT
public:
    explicit StyleEditor(QWidget *parent = nullptr);
    void setStyle(const TextStyle &s);
    TextStyle style() const;

signals:
    void changed();

private:
    QFontComboBox *m_font;
    QDoubleSpinBox *m_size;
    ColorButton *m_color;
    QCheckBox *m_bold, *m_italic, *m_upper, *m_shadow, *m_autoFit;
    QComboBox *m_hAlign, *m_vAlign;
    QDoubleSpinBox *m_lineSpacing, *m_outline;
    ColorButton *m_outlineColor, *m_shadowColor;
    bool m_loading = false;
};

// Edits a normalised rectangle as percentages of the frame.
class RectEditor : public QWidget {
    Q_OBJECT
public:
    explicit RectEditor(QWidget *parent = nullptr);
    void setRect(const QRectF &r);
    QRectF rect() const;

signals:
    void changed();

private:
    QDoubleSpinBox *m_x, *m_y, *m_w, *m_h;
    bool m_loading = false;
};
