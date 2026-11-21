#pragma once

#include "core/Types.h"

#include <QDialog>

class ColorButton;
class Library;
class MediaBin;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QTextEdit;
class RectEditor;
class SlideCanvas;
class StyleEditor;

// Edits a whole presentation: slide list, rich text per box, box layout and
// style, per-slide background, transition, build animations and notes.
class SlideEditor : public QDialog {
    Q_OBJECT
public:
    SlideEditor(const Presentation &p, int startIndex, MediaBin *media, Library *library, QWidget *parent = nullptr);
    Presentation presentation() const { return m_pres; }

private:
    Slide *slide();
    TextBox *box();
    void buildUi();
    void refreshSlideList();
    void refreshSlideIcon(int index);
    void addMediaSlides(const QStringList &paths);
    void loadSlide();
    void loadBox();
    void loadText();
    void updatePreview();
    void storeText();
    void storeSlide();
    void storeBox();
    void chooseBackground(bool presentationDefault);

    Presentation m_pres;
    int m_slide = 0;
    int m_box = 0;
    bool m_loading = false;
    MediaBin *m_media;
    Library *m_library;

    QLineEdit *m_title, *m_author, *m_copyright, *m_ccli;
    QListWidget *m_slides;
    SlideCanvas *m_canvas;
    QTextEdit *m_text;
    QSpinBox *m_runSize;

    QLineEdit *m_label;
    ColorButton *m_labelColor;
    QPlainTextEdit *m_notes;
    QCheckBox *m_ownTransition;
    QComboBox *m_transition;
    QSpinBox *m_transitionMs;
    QComboBox *m_bgMode;
    ColorButton *m_bgColor;
    QLabel *m_bgPath;
    QComboBox *m_bgFit;
    QCheckBox *m_bgAudio;
    QSpinBox *m_advance;
    QCheckBox *m_loop;

    QComboBox *m_boxPicker;
    QGroupBox *m_imageGroup, *m_shapeGroup;
    QLabel *m_imagePath;
    QComboBox *m_imageFit;
    ColorButton *m_fill;
    QSpinBox *m_radius, *m_opacity;
    QCheckBox *m_boxMain;
    RectEditor *m_rect;
    StyleEditor *m_style;
    QComboBox *m_build;
    QSpinBox *m_buildDelay, *m_buildMs;

    QComboBox *m_defTransition;
    QSpinBox *m_defTransitionMs;
    QLabel *m_defBg;
};
