#include "ui/Widgets.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>

// ---- ColorButton ----------------------------------------------------------

ColorButton::ColorButton(QWidget *parent, bool alpha) : QToolButton(parent), m_alpha(alpha)
{
    setMinimumWidth(48);
    connect(this, &QToolButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_color, this, tr("Choose colour"),
                                                m_alpha ? QColorDialog::ShowAlphaChannel : QColorDialog::ColorDialogOptions());
        if (c.isValid()) {
            setColor(c);
            emit colorChanged(c);
        }
    });
    setColor(Qt::white);
}

void ColorButton::setColor(const QColor &c)
{
    m_color = c;
    QPixmap pm(32, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    for (int x = 0; x < 32; x += 8)
        for (int y = 0; y < 16; y += 8)
            p.fillRect(x, y, 8, 8, ((x + y) / 8) % 2 ? Qt::lightGray : Qt::white);
    p.fillRect(pm.rect(), c);
    p.setPen(Qt::gray);
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    setIcon(pm);
    setIconSize(pm.size());
}

// ---- StyleEditor ----------------------------------------------------------

StyleEditor::StyleEditor(QWidget *parent) : QWidget(parent)
{
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    m_font = new QFontComboBox;
    m_size = new QDoubleSpinBox;
    m_size->setRange(8, 400);
    m_size->setSuffix(" px");
    m_color = new ColorButton(this, true);
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(m_size);
    fontRow->addWidget(m_color);
    form->addRow(tr("Font"), m_font);
    form->addRow(tr("Size / colour"), fontRow);

    m_bold = new QCheckBox(tr("Bold"));
    m_italic = new QCheckBox(tr("Italic"));
    m_upper = new QCheckBox(tr("ALL CAPS"));
    auto *flags = new QHBoxLayout;
    flags->addWidget(m_bold);
    flags->addWidget(m_italic);
    flags->addWidget(m_upper);
    form->addRow(QString(), flags);

    m_hAlign = new QComboBox;
    m_hAlign->addItems({tr("Left"), tr("Centre"), tr("Right")});
    m_vAlign = new QComboBox;
    m_vAlign->addItems({tr("Top"), tr("Middle"), tr("Bottom")});
    auto *align = new QHBoxLayout;
    align->addWidget(m_hAlign);
    align->addWidget(m_vAlign);
    form->addRow(tr("Align"), align);

    m_lineSpacing = new QDoubleSpinBox;
    m_lineSpacing->setRange(0.5, 3.0);
    m_lineSpacing->setSingleStep(0.05);
    m_autoFit = new QCheckBox(tr("Shrink to fit"));
    auto *spacing = new QHBoxLayout;
    spacing->addWidget(m_lineSpacing);
    spacing->addWidget(m_autoFit);
    form->addRow(tr("Line spacing"), spacing);

    m_outline = new QDoubleSpinBox;
    m_outline->setRange(0, 30);
    m_outline->setSuffix(" px");
    m_outlineColor = new ColorButton(this, true);
    auto *outline = new QHBoxLayout;
    outline->addWidget(m_outline);
    outline->addWidget(m_outlineColor);
    form->addRow(tr("Outline"), outline);

    m_shadow = new QCheckBox(tr("Drop shadow"));
    m_shadowColor = new ColorButton(this, true);
    auto *shadow = new QHBoxLayout;
    shadow->addWidget(m_shadow);
    shadow->addWidget(m_shadowColor);
    form->addRow(QString(), shadow);

    auto emitChanged = [this] { if (!m_loading) emit changed(); };
    connect(m_font, &QFontComboBox::currentFontChanged, this, emitChanged);
    for (QDoubleSpinBox *s : {m_size, m_lineSpacing, m_outline})
        connect(s, &QDoubleSpinBox::valueChanged, this, emitChanged);
    for (QCheckBox *c : {m_bold, m_italic, m_upper, m_shadow, m_autoFit})
        connect(c, &QCheckBox::toggled, this, emitChanged);
    for (QComboBox *c : {m_hAlign, m_vAlign})
        connect(c, &QComboBox::currentIndexChanged, this, emitChanged);
    for (ColorButton *c : {m_color, m_outlineColor, m_shadowColor})
        connect(c, &ColorButton::colorChanged, this, emitChanged);
}

void StyleEditor::setStyle(const TextStyle &s)
{
    m_loading = true;
    m_font->setCurrentFont(QFont(s.fontFamily));
    m_size->setValue(s.fontSize);
    m_color->setColor(s.color);
    m_bold->setChecked(s.bold);
    m_italic->setChecked(s.italic);
    m_upper->setChecked(s.uppercase);
    m_hAlign->setCurrentIndex(s.hAlign & Qt::AlignLeft ? 0 : s.hAlign & Qt::AlignRight ? 2 : 1);
    m_vAlign->setCurrentIndex(s.vAlign & Qt::AlignTop ? 0 : s.vAlign & Qt::AlignBottom ? 2 : 1);
    m_lineSpacing->setValue(s.lineSpacing);
    m_autoFit->setChecked(s.autoFit);
    m_outline->setValue(s.outlineWidth);
    m_outlineColor->setColor(s.outlineColor);
    m_shadow->setChecked(s.shadow);
    m_shadowColor->setColor(s.shadowColor);
    m_loading = false;
}

TextStyle StyleEditor::style() const
{
    TextStyle s;
    s.fontFamily = m_font->currentFont().family();
    s.fontSize = m_size->value();
    s.color = m_color->color();
    s.bold = m_bold->isChecked();
    s.italic = m_italic->isChecked();
    s.uppercase = m_upper->isChecked();
    static const Qt::Alignment h[] = {Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight};
    static const Qt::Alignment v[] = {Qt::AlignTop, Qt::AlignVCenter, Qt::AlignBottom};
    s.hAlign = h[m_hAlign->currentIndex()];
    s.vAlign = v[m_vAlign->currentIndex()];
    s.lineSpacing = m_lineSpacing->value();
    s.autoFit = m_autoFit->isChecked();
    s.outlineWidth = m_outline->value();
    s.outlineColor = m_outlineColor->color();
    s.shadow = m_shadow->isChecked();
    s.shadowColor = m_shadowColor->color();
    return s;
}

// ---- RectEditor -----------------------------------------------------------

RectEditor::RectEditor(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    auto make = [&](const QString &prefix) {
        auto *s = new QDoubleSpinBox;
        s->setRange(-50, 150);
        s->setDecimals(1);
        s->setPrefix(prefix);
        s->setSuffix("%");
        row->addWidget(s);
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { if (!m_loading) emit changed(); });
        return s;
    };
    m_x = make("X ");
    m_y = make("Y ");
    m_w = make("W ");
    m_h = make("H ");
}

void RectEditor::setRect(const QRectF &r)
{
    m_loading = true;
    m_x->setValue(r.x() * 100);
    m_y->setValue(r.y() * 100);
    m_w->setValue(r.width() * 100);
    m_h->setValue(r.height() * 100);
    m_loading = false;
}

QRectF RectEditor::rect() const
{
    return QRectF(m_x->value() / 100, m_y->value() / 100, m_w->value() / 100, m_h->value() / 100);
}
