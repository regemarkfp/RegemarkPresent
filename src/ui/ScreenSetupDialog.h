#pragma once

#include "core/ScreenConfig.h"

#include <QDialog>

class ColorButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QToolButton;
class QListWidget;
class QSpinBox;
class RectEditor;
class StyleEditor;

// Configure logical screens (Audience / Confidence / Stream), what each one
// shows, and which physical outputs (monitor, NDI, DeckLink) it feeds.
class ScreenSetupDialog : public QDialog {
    Q_OBJECT
public:
    ScreenSetupDialog(const QList<ScreenConfig> &screens, QWidget *parent = nullptr);
    QList<ScreenConfig> screens() const { return m_screens; }

private:
    void refreshList();
    void load();
    void store();
    OutputTarget &target(OutputTarget::Kind kind);
    void identifyDisplays();
    static QString roleDescription(ScreenRole r);
    static QString itemText(const ScreenConfig &s);

    QList<ScreenConfig> m_screens;
    int m_current = -1;
    bool m_loading = false;

    QListWidget *m_list;
    QLineEdit *m_name;
    QComboBox *m_role;
    QSpinBox *m_width, *m_height;
    QComboBox *m_fps;
    QLabel *m_roleHint;
    QToolButton *m_advanced;

    QCheckBox *m_showBg, *m_showText, *m_transparent;
    QCheckBox *m_overrideRect, *m_overrideStyle, *m_useBar;
    RectEditor *m_rect;
    StyleEditor *m_style;
    ColorButton *m_bar;
    QGroupBox *m_confBox;
    QCheckBox *m_showNext, *m_showClock, *m_showNotes, *m_showLabel;

    QCheckBox *m_dispOn, *m_ndiOn, *m_dlOn, *m_dlKey;
    QComboBox *m_display, *m_deckLink;
    QLineEdit *m_ndiName;
};
