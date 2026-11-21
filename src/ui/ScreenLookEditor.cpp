#include "ui/ScreenLookEditor.h"

#include "ui/Widgets.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

ScreenLookEditor::ScreenLookEditor(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *show = new QGroupBox(tr("Show"));
    auto *sf = new QVBoxLayout(show);
    m_showBg = new QCheckBox(tr("Backgrounds"));
    m_showText = new QCheckBox(tr("Slide text"));
    m_transparent = new QCheckBox(tr("Transparent (alpha for NDI / key for DeckLink)"));
    auto *row = new QHBoxLayout;
    row->addWidget(m_showBg);
    row->addWidget(m_showText);
    row->addStretch();
    sf->addLayout(row);
    sf->addWidget(m_transparent);
    layout->addWidget(show);

    auto *text = new QGroupBox(tr("Text on this screen"));
    auto *tf = new QFormLayout(text);
    m_overrideRect = new QCheckBox(tr("Its own position"));
    m_rect = new RectEditor;
    tf->addRow(m_overrideRect, m_rect);
    m_useBar = new QCheckBox(tr("Band behind text"));
    m_bar = new ColorButton(this, true);
    tf->addRow(m_useBar, m_bar);
    m_overrideStyle = new QCheckBox(tr("Its own text style"));
    tf->addRow(m_overrideStyle);
    m_style = new StyleEditor;
    tf->addRow(m_style);
    layout->addWidget(text);

    m_confBox = new QGroupBox(tr("Confidence layout"));
    auto *cf = new QHBoxLayout(m_confBox);
    m_showNext = new QCheckBox(tr("Next slide"));
    m_showClock = new QCheckBox(tr("Clock / timer"));
    m_showNotes = new QCheckBox(tr("Notes"));
    m_showLabel = new QCheckBox(tr("Group label"));
    for (QCheckBox *c : {m_showNext, m_showClock, m_showNotes, m_showLabel}) cf->addWidget(c);
    layout->addWidget(m_confBox);
    layout->addStretch();

    auto emitChanged = [this] {
        m_rect->setEnabled(m_overrideRect->isChecked());
        m_style->setEnabled(m_overrideStyle->isChecked());
        m_bar->setEnabled(m_useBar->isChecked());
        if (!m_loading) emit changed();
    };
    for (QCheckBox *c : {m_showBg, m_showText, m_transparent, m_overrideRect, m_overrideStyle, m_useBar,
                         m_showNext, m_showClock, m_showNotes, m_showLabel})
        connect(c, &QCheckBox::toggled, this, emitChanged);
    connect(m_rect, &RectEditor::changed, this, emitChanged);
    connect(m_style, &StyleEditor::changed, this, emitChanged);
    connect(m_bar, &ColorButton::colorChanged, this, emitChanged);
}

void ScreenLookEditor::setLook(const ScreenLook &l, ScreenRole role)
{
    m_loading = true;
    m_showBg->setChecked(l.showBackground);
    m_showText->setChecked(l.showText);
    m_transparent->setChecked(l.transparent);
    m_overrideRect->setChecked(l.overrideRect);
    m_rect->setRect(l.rect);
    m_rect->setEnabled(l.overrideRect);
    m_overrideStyle->setChecked(l.overrideStyle);
    m_style->setStyle(l.style);
    m_style->setEnabled(l.overrideStyle);
    m_useBar->setChecked(l.barColor.alpha() > 0);
    m_bar->setColor(l.barColor.alpha() > 0 ? l.barColor : QColor(0, 0, 0, 170));
    m_bar->setEnabled(l.barColor.alpha() > 0);
    m_showNext->setChecked(l.showNext);
    m_showClock->setChecked(l.showClock);
    m_showNotes->setChecked(l.showNotes);
    m_showLabel->setChecked(l.showLabel);
    m_confBox->setVisible(role == ScreenRole::Confidence);
    m_loading = false;
}

ScreenLook ScreenLookEditor::look() const
{
    ScreenLook l;
    l.showBackground = m_showBg->isChecked();
    l.showText = m_showText->isChecked();
    l.transparent = m_transparent->isChecked();
    l.overrideRect = m_overrideRect->isChecked();
    l.rect = m_rect->rect();
    l.overrideStyle = m_overrideStyle->isChecked();
    l.style = m_style->style();
    l.barColor = m_useBar->isChecked() ? m_bar->color() : QColor(0, 0, 0, 0);
    l.showNext = m_showNext->isChecked();
    l.showClock = m_showClock->isChecked();
    l.showNotes = m_showNotes->isChecked();
    l.showLabel = m_showLabel->isChecked();
    return l;
}
