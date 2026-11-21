#include "ui/Theme.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>
#include <QToolButton>

namespace Theme {

static QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

// Parallelogram: top edge shifted right by `s`, bottom edge shifted left.
static QPainterPath slanted(const QRectF &r, double s)
{
    QPainterPath p;
    p.moveTo(r.left() + s, r.top());
    p.lineTo(r.right(), r.top());
    p.lineTo(r.right() - s, r.bottom());
    p.lineTo(r.left(), r.bottom());
    p.closeSubpath();
    return p;
}

static bool flag(const QWidget *w, const char *name)
{
    return w && w->property(name).toBool();
}

// Qt stylesheets can't clip to a parallelogram, so the slanted controls are
// drawn by this style; the stylesheet below only covers the flat parts.
class SlantStyle : public QProxyStyle {
public:
    SlantStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    void polish(QWidget *w) override
    {
        QProxyStyle::polish(w);
        if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QTabBar *>(w) || qobject_cast<QComboBox *>(w)
            || qobject_cast<QAbstractSpinBox *>(w))
            w->setAttribute(Qt::WA_Hover);
        if (flag(w, "accent") || flag(w, "danger")) {
            QPalette p = w->palette();
            p.setColor(QPalette::ButtonText, flag(w, "accent") ? onAccent : text);
            w->setPalette(p);
            QFont f = w->font();
            f.setBold(true);
            w->setFont(f);
        }
    }

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p, const QWidget *w) const override
    {
        switch (pe) {
        case PE_PanelButtonCommand:
        case PE_PanelButtonBevel:
        case PE_PanelButtonTool: {
            const bool down = opt->state & (State_Sunken | State_On);
            const bool over = opt->state & State_MouseOver;
            const bool enabled = opt->state & State_Enabled;
            QColor fill = raised;
            if (flag(w, "accent")) fill = over ? accentHover : accent;
            else if (flag(w, "danger")) fill = over ? danger : QColor(0x3d, 0x14, 0x18);
            else if (down) fill = accentDeep;
            else if (over) fill = hover;
            if (!enabled) fill = QColor(0x1e, 0x1e, 0x1e);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(fill);
            p->drawPath(slanted(QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5), kButtonSlant));
            p->restore();
            return;
        }
        case PE_FrameButtonTool:
        case PE_FrameDefaultButton:
        case PE_FrameFocusRect:
        case PE_FrameLineEdit:
            return;   // focus shows as an accent underline instead
        case PE_PanelLineEdit:
            // A line edit inside a combo/spin box draws nothing; its parent does.
            if (w && (qobject_cast<QComboBox *>(w->parentWidget()) || qobject_cast<QAbstractSpinBox *>(w->parentWidget())))
                return;
            drawInput(p, opt->rect, opt->state);
            return;
        default:
            QProxyStyle::drawPrimitive(pe, opt, p, w);
        }
    }

    void drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p, const QWidget *w) const override
    {
        if (ce == CE_TabBarTabShape) {
            const bool selected = opt->state & State_Selected;
            const bool over = opt->state & State_MouseOver;
            if (!selected && !over) return;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            const QRectF r = QRectF(opt->rect).adjusted(2, 3, -2, -1);
            p->setBrush(selected ? raised : panel);
            p->drawPath(slanted(r, kTabSlant));
            if (selected) {
                p->setBrush(accent);
                p->drawRect(QRectF(r.left() + 1, r.bottom() - 2, r.width() - kTabSlant - 1, 2));
            }
            p->restore();
            return;
        }
        if (ce == CE_TabBarTabLabel) {
            if (auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
                QStyleOptionTab t = *tab;
                const bool selected = opt->state & State_Selected;
                t.palette.setColor(QPalette::WindowText, selected || (opt->state & State_MouseOver) ? text : textDim);
                QProxyStyle::drawControl(ce, &t, p, w);
                return;
            }
        }
        if ((ce == CE_PushButtonLabel || ce == CE_ToolButtonLabel) && (flag(w, "accent") || flag(w, "danger"))) {
            // Dark text on accent fills, as on the site (set here: the
            // stylesheet layer resets widget palettes).
            if (ce == CE_PushButtonLabel) {
                if (auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
                    QStyleOptionButton o = *b;
                    o.palette.setColor(QPalette::ButtonText, flag(w, "accent") ? onAccent : text);
                    QProxyStyle::drawControl(ce, &o, p, w);
                    return;
                }
            } else if (auto *t = qstyleoption_cast<const QStyleOptionToolButton *>(opt)) {
                QStyleOptionToolButton o = *t;
                o.palette.setColor(QPalette::ButtonText, flag(w, "accent") ? onAccent : text);
                QProxyStyle::drawControl(ce, &o, p, w);
                return;
            }
        }
        if (ce == CE_ShapedFrame && flag(w, "card")) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(flag(w, "selected") ? QPen(accent, 2) : Qt::NoPen);
            p->setBrush(panel);
            p->drawPath(slanted(QRectF(opt->rect).adjusted(1, 1, -1, -1), kCardSlant));
            p->restore();
            return;
        }
        QProxyStyle::drawControl(ce, opt, p, w);
    }

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const override
    {
        if (cc == CC_ComboBox) {
            auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(opt);
            drawInput(p, opt->rect, opt->state);
            const QRect arrow = subControlRect(cc, opt, SC_ComboBoxArrow, w);
            drawChevron(p, arrow, true, (opt->state & State_MouseOver) ? text : textDim);
            Q_UNUSED(cb);
            return;
        }
        if (cc == CC_SpinBox) {
            drawInput(p, opt->rect, opt->state);
            const QColor c = (opt->state & State_MouseOver) ? text : textDim;
            drawChevron(p, subControlRect(cc, opt, SC_SpinBoxUp, w), false, c);
            drawChevron(p, subControlRect(cc, opt, SC_SpinBoxDown, w), true, c);
            return;
        }
        QProxyStyle::drawComplexControl(cc, opt, p, w);
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *w) const override
    {
        QRect r = QProxyStyle::subControlRect(cc, opt, sc, w);
        // Keep text clear of the slanted left edge.
        if ((cc == CC_ComboBox && sc == SC_ComboBoxEditField) || (cc == CC_SpinBox && sc == SC_SpinBoxEditField))
            r.adjust(int(kInputSlant) + 2, 0, 0, 0);
        if (cc == CC_ComboBox && sc == SC_ComboBoxArrow) r.adjust(-int(kInputSlant), 0, -int(kInputSlant), 0);
        return r;
    }

    QRect subElementRect(SubElement se, const QStyleOption *opt, const QWidget *w) const override
    {
        QRect r = QProxyStyle::subElementRect(se, opt, w);
        if (se == SE_LineEditContents) r.adjust(int(kInputSlant) + 2, 0, -int(kInputSlant) - 2, 0);
        return r;
    }

    QSize sizeFromContents(ContentsType ct, const QStyleOption *opt, const QSize &size, const QWidget *w) const override
    {
        QSize s = QProxyStyle::sizeFromContents(ct, opt, size, w);
        switch (ct) {
        case CT_PushButton:
            return QSize(s.width() + int(kButtonSlant * 1.2) + 6, qMax(s.height() + 4, 32));
        case CT_ToolButton:
            return QSize(s.width() + int(kButtonSlant) + 6, qMax(s.height() + 4, 28));
        case CT_LineEdit:
        case CT_ComboBox:
        case CT_SpinBox:
            return QSize(s.width() + int(kInputSlant) * 2, qMax(s.height() + 2, 30));
        case CT_TabBarTab:
            return QSize(s.width() + int(kTabSlant) + 8, s.height() + 6);
        default:
            return s;
        }
    }

    int styleHint(StyleHint hint, const QStyleOption *opt, const QWidget *w, QStyleHintReturn *ret) const override
    {
        if (hint == SH_DialogButtonBox_ButtonsHaveIcons) return 0;
        return QProxyStyle::styleHint(hint, opt, w, ret);
    }

private:
    // Inputs: a slanted panel with a 2px underline, accent when focused.
    static void drawInput(QPainter *p, const QRect &rect, State state)
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        const QRectF r = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
        const QPainterPath shape = slanted(r, kInputSlant);
        p->setBrush((state & State_Enabled) ? panel : QColor(0x16, 0x16, 0x16));
        p->drawPath(shape);
        p->setClipPath(shape);
        const bool focus = state & State_HasFocus;
        p->setBrush(focus ? accent : ((state & State_MouseOver) ? QColor(0x4a, 0x4a, 0x4a) : border));
        p->drawRect(QRectF(r.left(), r.bottom() - 2, r.width(), 2));
        p->restore();
    }

    static void drawChevron(QPainter *p, const QRect &r, bool down, const QColor &c)
    {
        if (r.isEmpty()) return;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF m = QRectF(r).center();
        const double s = qMin(r.width(), r.height()) * 0.22;
        if (down) {
            const QPointF pts[3] = {{m.x() - s, m.y() - s / 2}, {m.x(), m.y() + s / 2}, {m.x() + s, m.y() - s / 2}};
            p->drawPolyline(pts, 3);
        } else {
            const QPointF pts[3] = {{m.x() - s, m.y() + s / 2}, {m.x(), m.y() - s / 2}, {m.x() + s, m.y() + s / 2}};
            p->drawPolyline(pts, 3);
        }
        p->restore();
    }
};

void apply(QApplication &app)
{
    app.setStyle(new SlantStyle);

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, panel);
    p.setColor(QPalette::ToolTipBase, raised);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x77, 0x77, 0x77));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, raised);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, onAccent);
    p.setColor(QPalette::Link, accentHover);
    p.setColor(QPalette::Mid, border);
    p.setColor(QPalette::Dark, base);
    for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        p.setColor(QPalette::Disabled, role, QColor(0x5a, 0x5a, 0x5a));
    app.setPalette(p);

    // The site's typeface: Segoe UI on Windows, the system UI font elsewhere.
    QFont f = app.font();
#ifdef Q_OS_WIN
    f.setFamily("Segoe UI");
#endif
    f.setPointSizeF(qMax(9.5, f.pointSizeF()));
    app.setFont(f);

    // Flat parts only: buttons, inputs, tabs and cards are drawn by SlantStyle.
    QString qss = R"(
        QMainWindow, QDialog { background: @window; }
        QToolTip { background: @raised; color: @text; border: 1px solid @border; padding: 5px 8px; }

        QToolBar { background: @window; border: none; border-bottom: 1px solid @border; padding: 6px 8px; spacing: 6px; }
        QToolBar::separator { background: @border; width: 1px; margin: 6px 8px; }

        QMenuBar { background: @window; color: @textDim; }
        QMenuBar::item { padding: 4px 10px; }
        QMenuBar::item:selected { background: @raised; color: @text; }
        QMenu { background: @panel; border: 1px solid @border; padding: 5px; }
        QMenu::item { padding: 6px 22px; }
        QMenu::item:selected { background: @accent; color: @onAccent; }
        QMenu::separator { height: 1px; background: @border; margin: 5px 8px; }

        QPlainTextEdit, QTextEdit { background: @panel; border: none; border-bottom: 2px solid @border; border-radius: 2px;
            padding: 4px 6px; color: @text; selection-background-color: @accent; selection-color: @onAccent; }
        QPlainTextEdit:focus, QTextEdit:focus { border-bottom-color: @accent; }
        QComboBox QAbstractItemView { background: @panel; border: 1px solid @border; selection-background-color: @accent;
            selection-color: @onAccent; outline: none; }

        QListView, QTreeView, QTableView { background: @base; border: 1px solid @border; border-radius: 2px; outline: none; }
        QListView::item { border-radius: 2px; padding: 4px; }
        QListView::item:hover { background: @raised; }
        QListView::item:selected { background: @accentDeep; color: white; }
        QHeaderView::section { background: @window; color: @textDim; border: none; border-bottom: 1px solid @border;
            padding: 6px; font-weight: 600; }

        QTabWidget::pane { border: none; }
        QTabBar { qproperty-drawBase: 0; }

        QGroupBox { border: 1px solid @border; border-radius: 2px; margin-top: 16px; padding-top: 10px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: @accent; }

        QCheckBox { spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; border-radius: 2px; border: 1px solid #4a4a4a; background: @panel; }
        QCheckBox::indicator:hover { border-color: @accent; }
        QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
        QScrollBar::handle { background: #3a3a3a; border-radius: 2px; min-height: 30px; min-width: 30px; }
        QScrollBar::handle:hover { background: @accent; }
        QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; height: 0; width: 0; }

        QSplitter::handle { background: @window; }
        QSplitter::handle:hover { background: @accentDeep; }
        QProgressBar { background: @panel; border: none; border-radius: 2px; text-align: center; color: @text; }
        QProgressBar::chunk { background: @accent; border-radius: 2px; }
        QStatusBar { background: @window; color: @textDim; border-top: 1px solid @border; }

        QLabel[role="heading"] { color: @text; font-weight: 700; font-size: 12pt; }
        QLabel[role="section"] { color: @accent; font-weight: 700; font-size: 10.5pt; }
        QLabel[role="hint"] { color: @textDim; }
    )";
    const QList<QPair<QString, QColor>> vars = {
        {"@accentHover", accentHover}, {"@accentDeep", accentDeep}, {"@accent", accent}, {"@onAccent", onAccent},
        {"@window", window}, {"@panel", panel}, {"@base", base}, {"@raised", raised},
        {"@border", border}, {"@textDim", textDim}, {"@text", text}, {"@danger", danger}};
    for (const auto &v : vars) qss.replace(v.first, hex(v.second));
    app.setStyleSheet(qss);
}

}
