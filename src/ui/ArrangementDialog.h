#pragma once

#include "core/Types.h"

#include <QDialog>

class QLineEdit;
class QListWidget;

// Build a song order from its groups: click groups to append them, drag to
// reorder, Delete to remove. "V1 C V2 C B C C" without duplicating slides.
class ArrangementDialog : public QDialog {
    Q_OBJECT
public:
    ArrangementDialog(const Presentation &p, const Arrangement &start, QWidget *parent = nullptr);
    Arrangement arrangement() const;

private:
    void addGroup(const QString &label);

    QLineEdit *m_name;
    QListWidget *m_order;
    QHash<QString, QColor> m_colors;
};
