#include "ui/ScreenSetupDialog.h"

#include "output/DeckLinkOutput.h"
#include "output/DisplayOutput.h"
#include "output/NdiOutput.h"
#include "ui/Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static const QList<QPair<QString, double>> kRates = {
    {"23.976", 23.976}, {"24", 24}, {"25", 25}, {"29.97", 29.97}, {"30", 30},
    {"50", 50}, {"59.94", 59.94}, {"60", 60}};

ScreenSetupDialog::ScreenSetupDialog(const QList<ScreenConfig> &screens, QWidget *parent)
    : QDialog(parent), m_screens(screens)
{
    setWindowTitle(tr("Screen setup"));
    resize(1000, 760);
    auto *root = new QVBoxLayout(this);
    auto *body = new QHBoxLayout;
    root->addLayout(body, 1);

    // Screen list
    auto *left = new QVBoxLayout;
    m_list = new QListWidget;
    m_list->setMaximumWidth(240);
    left->addWidget(m_list, 1);
    auto *btns = new QHBoxLayout;
    auto *add = new QToolButton;
    add->setText(tr("+ Add"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto *addMenu = new QMenu(add);
    for (ScreenRole r : {ScreenRole::Audience, ScreenRole::Confidence, ScreenRole::Stream})
        addMenu->addAction(ScreenConfig::roleName(r), this, [this, r] {
            store();
            m_screens.append(ScreenConfig::defaults(r));
            m_current = int(m_screens.size()) - 1;
            refreshList();
            load();
        });
    add->setMenu(addMenu);
    auto *remove = new QToolButton;
    remove->setText(tr("Remove"));
    btns->addWidget(add);
    btns->addWidget(remove);
    auto *identify = new QToolButton;
    identify->setText(tr("Identify displays"));
    identify->setToolTip(tr("Show a big number on every connected monitor for a few seconds"));
    connect(identify, &QToolButton::clicked, this, &ScreenSetupDialog::identifyDisplays);
    btns->addStretch();
    left->addLayout(btns);
    left->addWidget(identify);
    body->addLayout(left);

    // Editor
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *panel = new QWidget;
    scroll->setWidget(panel);
    body->addWidget(scroll, 1);
    auto *pl = new QVBoxLayout(panel);
    auto *intro = new QLabel(tr("Each screen is a picture Regemark Present draws. Send it to a projector or TV, "
                                "to NDI on the network, to a DeckLink SDI card, or to several at once."));
    intro->setWordWrap(true);
    intro->setProperty("role", "hint");
    pl->addWidget(intro);

    auto *general = new QGroupBox(tr("Screen"));
    auto *gf = new QFormLayout(general);
    m_name = new QLineEdit;
    gf->addRow(tr("Name"), m_name);
    m_role = new QComboBox;
    for (ScreenRole r : {ScreenRole::Audience, ScreenRole::Confidence, ScreenRole::Stream})
        m_role->addItem(ScreenConfig::roleName(r));
    gf->addRow(tr("Type"), m_role);
    m_roleHint = new QLabel;
    m_roleHint->setWordWrap(true);
    m_roleHint->setProperty("role", "hint");
    gf->addRow(QString(), m_roleHint);
    m_width = new QSpinBox;
    m_width->setRange(320, 7680);
    m_height = new QSpinBox;
    m_height->setRange(240, 4320);
    auto *preset = new QComboBox;
    preset->addItems({tr("Preset…"), "1920×1080", "1280×720", "3840×2160", "1024×768", "1080×1920 (portrait)"});
    auto *res = new QHBoxLayout;
    res->addWidget(m_width);
    res->addWidget(new QLabel("×"));
    res->addWidget(m_height);
    res->addWidget(preset);
    gf->addRow(tr("Resolution"), res);
    m_fps = new QComboBox;
    for (const auto &r : kRates) m_fps->addItem(r.first, r.second);
    gf->addRow(tr("Frame rate"), m_fps);
    pl->addWidget(general);

    m_advanced = new QToolButton;
    m_advanced->setText(tr("Advanced: what this screen shows ▸"));
    m_advanced->setCheckable(true);
    m_advanced->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto *look = new QGroupBox(tr("What this screen shows"));
    auto *lf = new QFormLayout(look);
    m_showBg = new QCheckBox(tr("Backgrounds"));
    m_showText = new QCheckBox(tr("Slide text"));
    m_transparent = new QCheckBox(tr("Transparent (alpha for NDI / key for DeckLink)"));
    auto *showRow = new QHBoxLayout;
    showRow->addWidget(m_showBg);
    showRow->addWidget(m_showText);
    lf->addRow(showRow);
    lf->addRow(m_transparent);
    m_overrideRect = new QCheckBox(tr("Move the main text box on this screen"));
    m_rect = new RectEditor;
    lf->addRow(m_overrideRect);
    lf->addRow(tr("Position"), m_rect);
    m_useBar = new QCheckBox(tr("Band behind text"));
    m_bar = new ColorButton(this, true);
    auto *barRow = new QHBoxLayout;
    barRow->addWidget(m_useBar);
    barRow->addWidget(m_bar);
    barRow->addStretch();
    lf->addRow(barRow);
    m_overrideStyle = new QCheckBox(tr("Use its own text style on this screen"));
    m_style = new StyleEditor;
    lf->addRow(m_overrideStyle);
    lf->addRow(m_style);

    m_confBox = new QGroupBox(tr("Confidence layout"));
    auto *cf = new QHBoxLayout(m_confBox);
    m_showNext = new QCheckBox(tr("Next slide"));
    m_showClock = new QCheckBox(tr("Clock"));
    m_showNotes = new QCheckBox(tr("Notes"));
    m_showLabel = new QCheckBox(tr("Group label"));
    for (QCheckBox *c : {m_showNext, m_showClock, m_showNotes, m_showLabel}) cf->addWidget(c);

    auto *outs = new QGroupBox(tr("Send this screen to"));
    auto *of = new QFormLayout(outs);
    m_dispOn = new QCheckBox(tr("Projector / TV / monitor"));
    m_display = new QComboBox;
    const auto allScreens = QGuiApplication::screens();
    for (int i = 0; i < allScreens.size(); ++i)
        m_display->addItem(QStringLiteral("%1: %2 — %3×%4").arg(i + 1).arg(allScreens[i]->name())
                               .arg(allScreens[i]->size().width()).arg(allScreens[i]->size().height()),
                           allScreens[i]->name());
    of->addRow(m_dispOn, m_display);

    m_ndiOn = new QCheckBox(tr("NDI (network)"));
    m_ndiName = new QLineEdit;
    of->addRow(m_ndiOn, m_ndiName);
    QString ndiError;
    if (!NdiOutput::runtimeAvailable(&ndiError)) {
        auto *warn = new QLabel(ndiError);
        warn->setStyleSheet("color: #d08000");
        of->addRow(QString(), warn);
    }

    m_dlOn = new QCheckBox(tr("DeckLink SDI"));
    m_deckLink = new QComboBox;
    QString dlError;
    const QStringList devices = deckLinkDevices(&dlError);
    for (int i = 0; i < devices.size(); ++i) m_deckLink->addItem(QStringLiteral("%1: %2").arg(i + 1).arg(devices[i]));
    if (devices.isEmpty())
        for (int i = 0; i < 4; ++i) m_deckLink->addItem(tr("Device %1").arg(i + 1));
    m_dlKey = new QCheckBox(tr("Fill + key"));
    m_dlKey->setToolTip(tr("Output alpha as a key signal on the card's second connector"));
    auto *dlRow = new QHBoxLayout;
    dlRow->addWidget(m_deckLink, 1);
    dlRow->addWidget(m_dlKey);
    of->addRow(m_dlOn, dlRow);
    if (!dlError.isEmpty()) {
        auto *warn = new QLabel(dlError);
        warn->setStyleSheet("color: #d08000");
        of->addRow(QString(), warn);
    }
    pl->addWidget(outs);
    pl->addWidget(m_advanced, 0, Qt::AlignLeft);
    pl->addWidget(look);
    pl->addWidget(m_confBox);
    pl->addStretch();
    look->setVisible(false);
    connect(m_advanced, &QToolButton::toggled, this, [this, look](bool on) {
        look->setVisible(on);
        m_confBox->setVisible(on && m_role->currentIndex() == int(ScreenRole::Confidence));
        m_advanced->setText(on ? tr("Advanced: what this screen shows ▾") : tr("Advanced: what this screen shows ▸"));
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { store(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading) return;
        store();
        m_current = row;
        load();
    });
    connect(remove, &QToolButton::clicked, this, [this] {
        if (m_current < 0) return;
        m_screens.removeAt(m_current);
        m_current = qMin(m_current, int(m_screens.size()) - 1);
        refreshList();
        load();
    });
    connect(preset, &QComboBox::activated, this, [this, preset](int i) {
        static const QSize sizes[] = {{}, {1920, 1080}, {1280, 720}, {3840, 2160}, {1024, 768}, {1080, 1920}};
        if (i > 0) {
            m_width->setValue(sizes[i].width());
            m_height->setValue(sizes[i].height());
        }
        preset->setCurrentIndex(0);
    });
    connect(m_role, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_confBox->setVisible(m_advanced->isChecked() && i == int(ScreenRole::Confidence));
        m_roleHint->setText(roleDescription(ScreenRole(i)));
    });
    connect(m_overrideRect, &QCheckBox::toggled, m_rect, &QWidget::setEnabled);
    connect(m_overrideStyle, &QCheckBox::toggled, m_style, &QWidget::setEnabled);
    connect(m_useBar, &QCheckBox::toggled, m_bar, &QWidget::setEnabled);

    m_current = m_screens.isEmpty() ? -1 : 0;
    refreshList();
    load();
}

QString ScreenSetupDialog::roleDescription(ScreenRole r)
{
    switch (r) {
    case ScreenRole::Audience:
        return tr("What the congregation sees: lyrics, scripture and backgrounds.");
    case ScreenRole::Confidence:
        return tr("For the stage: current and next slide, clock and notes. Usually a TV facing the band or pastor.");
    case ScreenRole::Stream:
        return tr("A transparent lower third for the live stream or recording, keyed over the camera in OBS, vMix or an ATEM.");
    }
    return {};
}

QString ScreenSetupDialog::itemText(const ScreenConfig &s)
{
    QStringList to;
    for (const OutputTarget &t : s.targets) {
        if (!t.enabled) continue;
        switch (t.kind) {
        case OutputTarget::Kind::Display: to << tr("Display %1").arg(t.display); break;
        case OutputTarget::Kind::Ndi: to << tr("NDI"); break;
        case OutputTarget::Kind::DeckLink: to << tr("SDI"); break;
        }
    }
    return QStringLiteral("%1\n→ %2").arg(s.name, to.isEmpty() ? tr("not sent anywhere yet") : to.join(" · "));
}

void ScreenSetupDialog::identifyDisplays()
{
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        auto *w = new QLabel;
        w->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setAlignment(Qt::AlignCenter);
        w->setStyleSheet(QStringLiteral("background: #a259ff; color: white; border-radius: 24px;"));
        w->setText(QStringLiteral("<div style='font-size:120px; font-weight:800'>%1</div>"
                                  "<div style='font-size:26px'>%2 · %3×%4</div>")
                       .arg(i + 1).arg(screens[i]->name().toHtmlEscaped())
                       .arg(screens[i]->size().width()).arg(screens[i]->size().height()));
        const QRect g = screens[i]->geometry();
        w->setGeometry(QRect(0, 0, 520, 320).translated(g.center() - QPoint(260, 160)));
        w->show();
        QTimer::singleShot(3500, w, &QWidget::close);
    }
}

void ScreenSetupDialog::refreshList()
{
    m_loading = true;
    m_list->clear();
    for (const ScreenConfig &s : std::as_const(m_screens))
        m_list->addItem(itemText(s));
    m_list->setCurrentRow(m_current);
    m_loading = false;
}

OutputTarget &ScreenSetupDialog::target(OutputTarget::Kind kind)
{
    ScreenConfig &s = m_screens[m_current];
    for (OutputTarget &t : s.targets)
        if (t.kind == kind) return t;
    OutputTarget t;
    t.kind = kind;
    s.targets.append(t);
    return s.targets.last();
}

void ScreenSetupDialog::load()
{
    const bool valid = m_current >= 0 && m_current < m_screens.size();
    for (QWidget *w : findChildren<QGroupBox *>()) w->setEnabled(valid);
    if (!valid) return;
    m_loading = true;
    const ScreenConfig &s = m_screens[m_current];
    m_name->setText(s.name);
    m_role->setCurrentIndex(int(s.role));
    m_confBox->setVisible(m_advanced->isChecked() && s.role == ScreenRole::Confidence);
    m_roleHint->setText(roleDescription(s.role));
    m_width->setValue(s.resolution.width());
    m_height->setValue(s.resolution.height());
    int fpsIndex = 4;
    for (int i = 0; i < kRates.size(); ++i)
        if (qAbs(kRates[i].second - s.fps) < 0.01) fpsIndex = i;
    m_fps->setCurrentIndex(fpsIndex);

    const ScreenLook &l = s.look;
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

    const OutputTarget &d = target(OutputTarget::Kind::Display);
    m_dispOn->setChecked(d.enabled);
    const int di = m_display->findData(d.display);
    m_display->setCurrentIndex(di >= 0 ? di : qMin(1, m_display->count() - 1));
    const OutputTarget &n = target(OutputTarget::Kind::Ndi);
    m_ndiOn->setChecked(n.enabled);
    m_ndiName->setText(n.ndiName.isEmpty() ? QStringLiteral("Regemark Present %1").arg(s.name) : n.ndiName);
    const OutputTarget &k = target(OutputTarget::Kind::DeckLink);
    m_dlOn->setChecked(k.enabled);
    m_deckLink->setCurrentIndex(qBound(0, k.deckLinkDevice, m_deckLink->count() - 1));
    m_dlKey->setChecked(k.deckLinkKey);
    m_loading = false;
}

void ScreenSetupDialog::store()
{
    if (m_loading || m_current < 0 || m_current >= m_screens.size()) return;
    ScreenConfig &s = m_screens[m_current];
    s.name = m_name->text().trimmed();
    s.role = ScreenRole(m_role->currentIndex());
    s.resolution = QSize(m_width->value(), m_height->value());
    s.fps = m_fps->currentData().toDouble();

    ScreenLook &l = s.look;
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

    OutputTarget &d = target(OutputTarget::Kind::Display);
    d.enabled = m_dispOn->isChecked();
    d.display = m_display->currentData().toString();
    OutputTarget &n = target(OutputTarget::Kind::Ndi);
    n.enabled = m_ndiOn->isChecked();
    n.ndiName = m_ndiName->text().trimmed();
    OutputTarget &k = target(OutputTarget::Kind::DeckLink);
    k.enabled = m_dlOn->isChecked();
    k.deckLinkDevice = m_deckLink->currentIndex();
    k.deckLinkKey = m_dlKey->isChecked();

    if (QListWidgetItem *item = m_list->item(m_current))
        item->setText(itemText(s));
}
