#pragma once

#include "core/ScreenConfig.h"
#include "render/ScreenRenderer.h"

#include <QDialog>

class Library;
class MediaBin;
class MediaPool;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;
class QTabWidget;
class RectEditor;
class ScreenLookEditor;
class StyleEditor;

// Create and edit looks. Each look has a tab per screen (customise it or
// leave the screen's own setup) with a true-to-output preview, and a Slides
// tab for the text style the look applies to songs.
class LooksDialog : public QDialog {
    Q_OBJECT
public:
    LooksDialog(Library *library, const QList<ScreenConfig> &screens, const LiveState &sample,
                MediaBin *media, QWidget *parent = nullptr);
    ~LooksDialog() override;

    void select(const QString &lookId);

signals:
    // A look was edited and saved: the main window re-applies it if it's live.
    void lookEdited(const QString &lookId);

private:
    struct ScreenTab {
        QCheckBox *customise;
        ScreenLookEditor *editor;
        QLabel *preview;
    };

    void refreshList(const QString &selectId);
    void loadLook();
    void storeLook();
    void updatePreviews();
    Look *current();

    Library *m_library;
    QList<ScreenConfig> m_screens;
    LiveState m_sample;
    MediaBin *m_media;
    MediaPool *m_pool;

    QList<Look> m_looks;
    int m_index = -1;
    bool m_loading = false;

    QListWidget *m_list;
    QTabWidget *m_tabs;
    QList<ScreenTab> m_screenTabs;
    StyleEditor *m_style;
    RectEditor *m_rect;
    QLabel *m_bgLabel;
    QComboBox *m_transition;
    QSpinBox *m_transitionMs;
};
