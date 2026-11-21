#include "ui/ArrangementDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

ArrangementDialog::ArrangementDialog(const Presentation &p, const Arrangement &start, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Arrangement — %1").arg(p.title));
    resize(640, 480);
    for (const Slide &s : p.slides)
        if (!m_colors.contains(s.label))
            m_colors.insert(s.label, s.labelColor.isValid() ? s.labelColor : Slide::colorForLabel(s.label));

    auto *layout = new QVBoxLayout(this);
    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(tr("Name")));
    m_name = new QLineEdit(start.name.isEmpty() ? tr("Sunday") : start.name);
    nameRow->addWidget(m_name, 1);
    layout->addLayout(nameRow);

    auto *hint = new QLabel(tr("Click a group to add it to the end. Drag to reorder; select and press Delete to remove."));
    hint->setProperty("role", "hint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *body = new QHBoxLayout;
    auto *groups = new QGridLayout;
    int i = 0;
    for (const QString &g : p.groups()) {
        auto *b = new QPushButton(g);
        const QColor c = m_colors.value(g);
        b->setStyleSheet(QStringLiteral("QPushButton { background: %1; border-color: %1; color: white; font-weight: 600; }"
                                        "QPushButton:hover { border-color: white; }").arg(c.name()));
        connect(b, &QPushButton::clicked, this, [this, g] { addGroup(g); });
        groups->addWidget(b, i / 2, i % 2);
        ++i;
    }
    groups->setRowStretch(i / 2 + 1, 1);
    body->addLayout(groups, 1);

    m_order = new QListWidget;
    m_order->setDragDropMode(QAbstractItemView::InternalMove);
    m_order->setSelectionMode(QAbstractItemView::ExtendedSelection);
    body->addWidget(m_order, 1);
    layout->addLayout(body, 1);

    auto *row = new QHBoxLayout;
    auto *asWritten = new QPushButton(tr("Start from slides as written"));
    auto *clear = new QPushButton(tr("Clear"));
    row->addWidget(asWritten);
    row->addWidget(clear);
    row->addStretch();
    layout->addLayout(row);
    connect(asWritten, &QPushButton::clicked, this, [this, p] {
        m_order->clear();
        QString last;
        for (const Slide &s : p.slides)
            if (s.label != last) addGroup(last = s.label);
    });
    connect(clear, &QPushButton::clicked, m_order, &QListWidget::clear);
    auto *del = new QShortcut(QKeySequence::Delete, m_order);
    connect(del, &QShortcut::activated, this, [this] { qDeleteAll(m_order->selectedItems()); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (const QString &g : start.groups) addGroup(g);
}

void ArrangementDialog::addGroup(const QString &label)
{
    auto *item = new QListWidgetItem(label);
    QPixmap swatch(10, 22);
    swatch.fill(m_colors.value(label, Slide::colorForLabel(label)));
    item->setIcon(QIcon(swatch));
    m_order->addItem(item);
    m_order->scrollToBottom();
}

Arrangement ArrangementDialog::arrangement() const
{
    Arrangement a;
    a.name = m_name->text().trimmed().isEmpty() ? tr("Arrangement") : m_name->text().trimmed();
    for (int i = 0; i < m_order->count(); ++i) a.groups << m_order->item(i)->text();
    return a;
}
