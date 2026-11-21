#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace Theme {

static QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

void apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, panel);
    p.setColor(QPalette::ToolTipBase, raised);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, textDim);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, raised);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, accentHover);
    p.setColor(QPalette::Mid, border);
    p.setColor(QPalette::Dark, base);
    for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        p.setColor(QPalette::Disabled, role, QColor(0x5c, 0x5c, 0x66));
    app.setPalette(p);

    QFont f = app.font();
    f.setPointSizeF(qMax(9.5, f.pointSizeF()));
    app.setFont(f);

    QString qss = R"(
        QMainWindow, QDialog { background: @window; }
        QToolTip { background: @raised; color: @text; border: 1px solid @border; padding: 4px; }

        QToolBar { background: @panel; border: none; border-bottom: 1px solid @border; padding: 4px 6px; spacing: 4px; }
        QToolBar QToolButton { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 5px 10px; color: @text; }
        QToolBar QToolButton:hover { background: @raised; border-color: @border; }
        QToolBar QToolButton:checked { background: @accentDeep; border-color: @accent; }
        QToolBar::separator { background: @border; width: 1px; margin: 4px 6px; }

        QMenuBar { background: @panel; color: @text; }
        QMenuBar::item:selected, QMenu::item:selected { background: @accent; color: white; }
        QMenu { background: @raised; border: 1px solid @border; padding: 4px; }
        QMenu::item { padding: 5px 20px; border-radius: 4px; }

        QPushButton { background: @raised; border: 1px solid @border; border-radius: 6px; padding: 6px 12px; color: @text; }
        QPushButton:hover { border-color: @accent; }
        QPushButton:pressed { background: @accentDeep; }
        QPushButton:checked { background: @accentDeep; border-color: @accent; }
        QPushButton:disabled { color: #5c5c66; border-color: #2c2c32; }
        QPushButton[accent="true"] { background: @accent; border-color: @accent; color: white; font-weight: 600; }
        QPushButton[accent="true"]:hover { background: @accentHover; }
        QPushButton[danger="true"] { background: #3a1d22; border-color: @danger; color: #ffd9dd; font-weight: 600; }
        QPushButton[danger="true"]:hover { background: @danger; color: white; }

        QToolButton { background: @raised; border: 1px solid @border; border-radius: 6px; padding: 4px 8px; color: @text; }
        QToolButton:hover { border-color: @accent; }

        QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: @base; border: 1px solid @border; border-radius: 6px; padding: 4px 8px; color: @text;
            selection-background-color: @accent; }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: @accent; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView { background: @raised; border: 1px solid @border; selection-background-color: @accent; }

        QListView, QTreeView, QTableView { background: @base; border: 1px solid @border; border-radius: 8px; outline: none; }
        QListView::item { border-radius: 6px; padding: 4px; }
        QListView::item:hover { background: @raised; }
        QListView::item:selected { background: @accentDeep; color: white; }
        QHeaderView::section { background: @panel; color: @textDim; border: none; border-bottom: 1px solid @border; padding: 5px; }

        QTabWidget::pane { border: none; }
        QTabBar::tab { background: transparent; color: @textDim; padding: 7px 14px; border: none; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: @text; border-bottom: 2px solid @accent; }
        QTabBar::tab:hover { color: @text; }

        QGroupBox { border: 1px solid @border; border-radius: 8px; margin-top: 14px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @textDim; }

        QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid @border; background: @base; }
        QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
        QScrollBar::handle { background: @border; border-radius: 4px; min-height: 30px; min-width: 30px; }
        QScrollBar::handle:hover { background: @accent; }
        QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; height: 0; width: 0; }

        QSplitter::handle { background: @window; }
        QSplitter::handle:hover { background: @accentDeep; }
        QProgressBar { background: @base; border: 1px solid @border; border-radius: 6px; text-align: center; }
        QProgressBar::chunk { background: @accent; border-radius: 5px; }
        QStatusBar { background: @panel; color: @textDim; border-top: 1px solid @border; }

        QLabel[role="heading"] { color: @text; font-weight: 700; font-size: 11pt; }
        QLabel[role="section"] { color: @textDim; font-weight: 700; font-size: 8.5pt; letter-spacing: 1px; }
        QLabel[role="hint"] { color: @textDim; }
        QFrame[card="true"] { background: @panel; border: 1px solid @border; border-radius: 10px; }
        QFrame[card="true"][selected="true"] { border: 2px solid @accent; }
    )";
    const QList<QPair<QString, QColor>> vars = {
        {"@accentHover", accentHover}, {"@accentDeep", accentDeep}, {"@accent", accent},
        {"@window", window}, {"@panel", panel}, {"@base", base}, {"@raised", raised},
        {"@border", border}, {"@textDim", textDim}, {"@text", text}, {"@danger", danger}};
    for (const auto &v : vars) qss.replace(v.first, hex(v.second));
    app.setStyleSheet(qss);
}

}
